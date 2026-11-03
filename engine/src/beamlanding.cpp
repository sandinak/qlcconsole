/*
  Q Light Controller Plus
  beamlanding.cpp

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QCoreApplication>
#include <QStringList>
#include <QVector2D>
#include <QMap>
#include <cmath>

#include "beamlanding.h"
#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "stagetarget.h"
#include "aimsolver.h"
#include "linecurve.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

namespace
{
QString tr(const char *text)
{
    return QCoreApplication::translate("BeamLanding", text);
}

const QLCChannel *axisChannel(const Fixture *fx, QLCChannel::Group group, quint32 &msb, quint32 &lsb)
{
    msb = fx->channelNumber(group, QLCChannel::MSB);
    lsb = fx->channelNumber(group, QLCChannel::LSB);
    const QLCFixtureMode *mode = fx->fixtureMode();
    return (mode != nullptr && msb != QLCChannel::invalid()) ? mode->channel(msb) : nullptr;
}

/** Distance in the floor plane from @p p to a polyline, and how far along it
 *  (0..1, by length) the nearest point is. */
double distanceToLine(const QVector<QVector3D> &pts, const QVector3D &p, double &fraction)
{
    fraction = 0.0;
    if (pts.isEmpty())
        return 1e9;
    double total = 0.0;
    for (int i = 1; i < pts.size(); ++i)
        total += double(QVector3D(pts.at(i) - pts.at(i - 1)).toVector2D().length());
    double best = 1e9, run = 0.0;
    if (pts.size() == 1)
        return double((pts.first() - p).toVector2D().length());
    for (int i = 1; i < pts.size(); ++i)
    {
        const QVector2D a = pts.at(i - 1).toVector2D(), b = pts.at(i).toVector2D(), q = p.toVector2D();
        const QVector2D ab = b - a;
        const double len2 = double(QVector2D::dotProduct(ab, ab));
        double t = len2 > 1e-12 ? double(QVector2D::dotProduct(q - a, ab)) / len2 : 0.0;
        t = qBound(0.0, t, 1.0);
        const QVector2D at = a + ab * float(t);
        const double d = double((q - at).length());
        const double seg = std::sqrt(len2);
        if (d < best)
        {
            best = d;
            fraction = total > 1e-9 ? (run + t * seg) / total : 0.0;
        }
        run += seg;
    }
    return best;
}
}

bool BeamLanding::direction(Doc *doc, quint32 fixtureId, double panDmx, double tiltDmx, QVector3D &dir)
{
    Fixture *fx = doc != nullptr ? doc->fixture(fixtureId) : nullptr;
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (fx == nullptr || mp == nullptr || fx->fixtureMode() == nullptr)
        return false;
    quint32 msb, lsb;
    const QLCChannel *panCh = axisChannel(fx, QLCChannel::Pan, msb, lsb);
    const QLCChannel *tiltCh = axisChannel(fx, QLCChannel::Tilt, msb, lsb);
    const QLCPhysical &phy = fx->fixtureMode()->physical();
    const double panMax = phy.focusPanMax() != 0 ? phy.focusPanMax() : 360.0;
    const double tiltMax = phy.focusTiltMax() != 0 ? phy.focusTiltMax() : 270.0;
    /* As the rig view: across the pointing band (Fixture::positionBand()),
       centred on the range. The band is read at 16 bits, as
       positionToValues() writes into it -- MSB lo .. hi plus a whole fine
       byte at the top. Fixture::positionFraction() stops the band at MSB hi,
       which reads an indexed head (0-127 of 255) about 0.4% of its range
       wide: a degree on a 540 degree pan, ten centimetres at a six-metre
       throw. Without a band the two agree. */
    auto centred = [](const QLCChannel *ch, double dmx, double max) {
        if (ch == nullptr)
            return 0.0;
        uchar lo = 0, hi = 255;
        double f;
        if (Fixture::positionBand(ch, lo, hi))
        {
            const double lo16 = double(lo) * 256.0, hi16 = double(hi) * 256.0 + 255.0;
            f = qBound(0.0, (dmx * 65535.0 / 255.0 - lo16) / (hi16 - lo16), 1.0);
        }
        else
            f = Fixture::positionFraction(ch, dmx);
        return f * max - max / 2.0;
    };
    return AimSolver::beamDirection(mp->fixtureRigProps(fixtureId), centred(panCh, panDmx, panMax),
                                    centred(tiltCh, tiltDmx, tiltMax), dir);
}

void BeamLanding::dmxOf(Doc *doc, quint32 fixtureId, const QList<SceneValue> &values,
                        double &panDmx, double &tiltDmx, bool &hasPan, bool &hasTilt)
{
    hasPan = hasTilt = false;
    panDmx = tiltDmx = 127.5;
    Fixture *fx = doc != nullptr ? doc->fixture(fixtureId) : nullptr;
    if (fx == nullptr)
        return;
    quint32 panMsb, panLsb, tiltMsb, tiltLsb;
    axisChannel(fx, QLCChannel::Pan, panMsb, panLsb);
    axisChannel(fx, QLCChannel::Tilt, tiltMsb, tiltLsb);
    double panFine = 0.0, tiltFine = 0.0;
    for (const SceneValue &sv : values)
    {
        if (sv.fxi != fixtureId)
            continue;
        if (panMsb != QLCChannel::invalid() && sv.channel == panMsb)
        {
            panDmx = sv.value;
            hasPan = true;
        }
        else if (tiltMsb != QLCChannel::invalid() && sv.channel == tiltMsb)
        {
            tiltDmx = sv.value;
            hasTilt = true;
        }
        else if (panLsb != QLCChannel::invalid() && sv.channel == panLsb)
            panFine = sv.value / 256.0;
        else if (tiltLsb != QLCChannel::invalid() && sv.channel == tiltLsb)
            tiltFine = sv.value / 256.0;
    }
    /* 16-bit as the writer meant it: (MSB * 256 + LSB) of 65535, on the
       MSB's 0..255 scale -- not MSB + LSB / 256, which reads a centred head
       a tenth of a degree per step further out. */
    if (hasPan && panFine > 0.0)
        panDmx = (panDmx * 256.0 + panFine * 256.0) * 255.0 / 65535.0;
    if (hasTilt && tiltFine > 0.0)
        tiltDmx = (tiltDmx * 256.0 + tiltFine * 256.0) * 255.0 / 65535.0;
}

QString BeamLanding::areaAt(Doc *doc, const QVector3D &point)
{
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (mp == nullptr)
        return QString();
    const double width = double(mp->gridSize().x() * mp->gridUnitMetres());
    const double front = double(mp->downstageEdgeY());
    const double x = double(point.x()), y = double(point.y());
    if (width <= 0.0 || front <= 0.0 || x < 0.0 || x > width || y < 0.0 || y > front)
        return QString();
    const int row = qBound(0, int(y / front * 3.0), 2);      // 0 upstage .. 2 downstage
    const int col = qBound(0, int(x / width * 3.0), 2);      // 0 stage right .. 2 stage left
    if (row == 1 && col == 1)
        return tr("centre stage");
    static const char *rows[] = { "US", "CS", "DS" };
    const QString side = col == 0 ? tr("right") : col == 1 ? tr("centre") : tr("left");
    return QStringLiteral("%1 %2").arg(QLatin1String(rows[row]), side);
}

BeamLanding::Result BeamLanding::land(Doc *doc, quint32 fixtureId, const QList<SceneValue> &values)
{
    Result r;
    r.fixture = fixtureId;
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    AimSolver::Frame frame;
    if (mp == nullptr || !AimSolver::frameFor(doc, fixtureId, frame))
        return r;
    double pan, tilt;
    bool hasPan, hasTilt;
    dmxOf(doc, fixtureId, values, pan, tilt, hasPan, hasTilt);
    if (!hasPan && !hasTilt)
        return r;
    QVector3D dir;
    if (!direction(doc, fixtureId, pan, tilt, dir))
        return r;
    r.origin = frame.position;
    r.direction = dir;
    if (dir.z() > -1e-3f)
    {
        r.kind = IntoAir;
        r.point = r.origin + dir * 10.0f;
        return r;
    }
    // Down to the floor, then once more to the deck height found there.
    auto at = [&](float z) { return r.origin + dir * ((z - r.origin.z()) / dir.z()); };
    QVector3D p = at(0.0f);
    const float deck = mp->platformHeightAt(p.x(), p.y());
    if (deck > 0.0f && deck < r.origin.z())
        p = at(deck);
    r.point = p;

    // A target it lands at: the nearest within reach.
    double best = nearTarget();
    for (const StageTarget *t : mp->stageTargets())
    {
        if (t == nullptr)
            continue;
        double fraction = -1.0, d;
        if (t->isLine())
            d = distanceToLine(mp->targetLinePoints(t), p, fraction);
        else
            d = double((mp->targetAimPoint(t).toVector2D() - p.toVector2D()).length());
        if (d < best)
        {
            best = d;
            r.target = t->id();
            r.lineFraction = t->isLine() ? fraction : -1.0;
        }
    }
    r.area = areaAt(doc, p);
    if (!r.area.isEmpty() || r.target != UINT_MAX)
        r.kind = OnStage;
    else
        r.kind = p.y() > mp->downstageEdgeY() ? IntoAudience : OffStage;
    return r;
}

QString BeamLanding::describe(Doc *doc, const Result &r)
{
    switch (r.kind)
    {
        case NoGeometry: return QString();
        case IntoAir: return tr("into the air");
        case IntoAudience: return tr("into the audience");
        case OffStage: return tr("off stage");
        case OnStage: break;
    }
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    const StageTarget *t = (mp != nullptr && r.target != UINT_MAX) ? mp->stageTarget(r.target) : nullptr;
    if (t != nullptr)
        return QStringLiteral("≈ %1").arg(t->name());
    return r.area;
}

QString BeamLanding::summary(Doc *doc, const QList<Result> &results)
{
    // Where most land (first seen wins a tie), then each way of missing.
    QStringList order;
    QMap<QString, int> count;
    int air = 0, audience = 0, off = 0;
    for (const Result &r : results)
    {
        switch (r.kind)
        {
            case IntoAir: ++air; break;
            case IntoAudience: ++audience; break;
            case OffStage: ++off; break;
            case OnStage:
            {
                const QString d = describe(doc, r);
                if (!count.contains(d))
                    order << d;
                ++count[d];
                break;
            }
            case NoGeometry: break;
        }
    }
    QStringList parts;
    QString most;
    for (const QString &d : order)
        if (most.isEmpty() || count.value(d) > count.value(most))
            most = d;
    if (!most.isEmpty())
    {
        const int others = order.count() - 1;
        parts << (others > 0 ? tr("lands %1 (+%2 more places)").arg(most).arg(others)
                             : tr("lands %1").arg(most));
    }
    auto heads = [](int n) {
        return n == 1 ? tr("1 head") : tr("%1 heads").arg(n);
    };
    if (off > 0)
        parts << tr("%1 off stage").arg(heads(off));
    if (audience > 0)
        parts << tr("%1 into the audience").arg(heads(audience));
    if (air > 0)
        parts << tr("%1 into the air").arg(heads(air));
    return parts.join(QStringLiteral(", "));
}

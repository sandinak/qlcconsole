/*
  Q Light Controller Plus
  pixelpath.cpp

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
#include <QtMath>

#include "monitorproperties.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "pixelpath.h"
#include "fountain.h"
#include "scenery.h"
#include "truss.h"
#include "doc.h"

QString PixelPath::name(int shape)
{
    switch (shape)
    {
        case InnerWallTop: return QCoreApplication::translate("PixelPath", "Inner wall top ring");
        case OuterWallTop: return QCoreApplication::translate("PixelPath", "Outer wall top ring");
        case BaseRing:     return QCoreApplication::translate("PixelPath", "Base ring");
        case FlameSpiral:  return QCoreApplication::translate("PixelPath", "Wrapped up the flame");
        case Wand:         return QCoreApplication::translate("PixelPath", "Wand (wound round a staff)");
        default:           return QCoreApplication::translate("PixelPath", "As its heads (none)");
    }
}

QVector<QVector3D> PixelPath::along(const QVector<QVector3D> &line, int n, bool closed)
{
    QVector<QVector3D> out;
    if (n <= 0 || line.isEmpty())
        return out;
    QVector<QVector3D> pts = line;
    if (closed && pts.size() > 1)
        pts << pts.first();
    QVector<double> cum(pts.size(), 0.0);
    for (int i = 1; i < pts.size(); ++i)
        cum[i] = cum[i - 1] + double((pts.at(i) - pts.at(i - 1)).length());
    const double total = cum.last();
    out.reserve(n);
    int seg = 1;
    for (int k = 0; k < n; ++k)
    {
        const double s = closed ? total * k / n : total * (k + 0.5) / n;
        while (seg < pts.size() - 1 && cum.at(seg) < s)
            ++seg;
        if (pts.size() == 1 || total <= 0.0)
        {
            out << pts.first();
            continue;
        }
        const double l = cum.at(seg) - cum.at(seg - 1);
        const double t = l > 0.0 ? (s - cum.at(seg - 1)) / l : 0.0;
        out << pts.at(seg - 1) + (pts.at(seg) - pts.at(seg - 1)) * float(t);
    }
    return out;
}

QVector<QVector3D> PixelPath::helix(const QVector3D &from, const QVector3D &to, double radius,
                                    double turns, int n)
{
    QVector<QVector3D> out;
    if (n <= 0)
        return out;
    QVector3D axis = to - from;
    if (axis.length() < 1e-6f)
        axis = QVector3D(0, 0, 1);
    const QVector3D a = axis.normalized();
    // Two directions across the axis: the first as near level as it can be.
    QVector3D u = QVector3D::crossProduct(a, QVector3D(0, 0, 1));
    if (u.length() < 1e-4f)
        u = QVector3D(1, 0, 0);
    u.normalize();
    const QVector3D v = QVector3D::crossProduct(a, u).normalized();
    out.reserve(n);
    for (int k = 0; k < n; ++k)
    {
        const double t = n > 1 ? double(k) / (n - 1) : 0.5;
        const double th = 2.0 * M_PI * turns * t;
        out << from + (to - from) * float(t)
                    + (u * float(qCos(th)) + v * float(qSin(th))) * float(radius);
    }
    return out;
}

double PixelPath::autoWandTurns(int n, double length, double strip, double diameter)
{
    if (strip <= 0.0)
        strip = n * kLedSpacing;
    const double round = M_PI * (diameter > 0.0 ? diameter : 2.0 * kWandRadius);
    if (strip <= length)
        return 0.5;
    return qMax(0.5, qSqrt(strip * strip - length * length) / round);
}

double PixelPath::wandTurns(const FixtureRigProps &rp, int n)
{
    if (rp.pixelTurns > 0.0f)
        return double(rp.pixelTurns);
    return autoWandTurns(n, double(qMax(0.1f, rp.wandLength) * (1.0f - kWandGrip)),
                         double(rp.wandStripLength), double(rp.wandDiameter));
}

double PixelPath::autoSpiralTurns(int n, double height)
{
    Q_UNUSED(n)
    // About a turn every 30 cm, at least two.
    return qMax(2.0, height / 0.3);
}

QVector3D PixelPath::heldCentre(const MonitorProperties *props, const StageTarget *person,
                                const FixtureRigProps &rp)
{
    if (person == nullptr || props == nullptr)
        return QVector3D();
    // Where the person stands in the blocking read (BLOCKING.md).
    const QVector3D at = props->targetPosition(person);
    // Facing the audience (+Y downstage); the hand a forearm in front.
    return QVector3D(at.x(), at.y() + 0.25f, at.z() + rp.holdHeight);
}

bool PixelPath::wandEnds(MonitorProperties *props, quint32 fid, QVector3D &bottom, QVector3D &top)
{
    if (props == nullptr)
        return false;
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const float len = qMax(0.1f, rp.wandLength);
    const StageTarget *t = rp.isHeld() ? props->stageTarget(rp.heldBy) : nullptr;
    if (t != nullptr)
    {
        const QVector3D c = heldCentre(props, t, rp);
        const double tilt = qDegreesToRadians(double(rp.holdTilt));
        const QVector3D axis(0.0f, float(qSin(tilt)), float(qCos(tilt)));
        bottom = c - axis * (len / 2.0f);
        top = c + axis * (len / 2.0f);
        return true;
    }
    bottom = props->fixtureRigPosition(fid);
    top = bottom + QVector3D(0.0f, 0.0f, len);
    return true;
}

static QVector<QVector3D> wallTop(const Scenery *s, float z)
{
    QVector<QVector3D> line;
    if (s == nullptr)
        return line;
    foreach (const QPointF &p, s->baseLine(48))
        line << QVector3D(float(p.x()), float(p.y()), z);
    return line;
}

QVector<QVector3D> PixelPath::positions(const Doc *doc, MonitorProperties *props, quint32 fid, int n)
{
    QVector<QVector3D> out;
    if (props == nullptr || n <= 0)
        return out;
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.pixelPath == Wand)
    {
        QVector3D b, t;
        if (!wandEnds(props, fid, b, t))
            return out;
        // The strip from a hand's width above the butt to the tip.
        const QVector3D grip = b + (t - b) * kWandGrip;
        const double radius = rp.wandDiameter > 0.0f ? double(rp.wandDiameter) / 2.0 : kWandRadius;
        const double turns = wandTurns(rp, n);
        // LED 1 where the data comes in: the tip, or the grip.
        return rp.wandFromTop ? helix(t, grip, radius + 0.002, turns, n)
                              : helix(grip, t, radius + 0.002, turns, n);
    }
    if (!needsFountain(rp.pixelPath))
        return out;
    Fountain::Parts f = Fountain::ofFixture(props, fid);
    if (!f.isValid())
        f = Fountain::ofBase(props, rp.deckPlatformId);
    if (!f.isValid())
        return out;
    switch (rp.pixelPath)
    {
        case InnerWallTop:
            return along(wallTop(f.inner, Fountain::innerTop(f) + 0.005f), n);
        case OuterWallTop:
            return along(wallTop(f.outer, f.outer->bottom() + f.outer->height() + 0.005f), n);
        case BaseRing:
        {
            // Round the base's edge, half way up its face, just proud of it.
            QVector<QVector3D> ring;
            const float z = props->platformBaseZ(f.base->id()) + f.base->height() * 0.5f;
            foreach (const QPointF &p, f.base->footprint(48))
                ring << QVector3D(float(p.x()), float(p.y()), z);
            return along(ring, n, true);
        }
        case FlameSpiral:
        {
            // Up the flame of this fountain (its middle when it has none).
            QVector3D at(0, 0, 0);
            float bottom = Fountain::innerTop(f), height = Fountain::kDefaultFireHeight;
            double radius = 0.12;
            bool found = false;
            foreach (quint32 other, props->fixtureItemsID())
                if (props->fixtureGroup(other) == f.groupId && doc != nullptr
                    && Fountain::isFlame(doc->fixture(other)))
                {
                    at = props->fixtureRigPosition(other);
                    bottom = Fountain::flameBottom(doc, props, other);
                    height = Fountain::flameHeight(doc, props, other);
                    radius = qMax(0.05, Fountain::flameUnitSize(doc->fixture(other)).x() / 2.0);
                    found = true;
                    break;
                }
            if (!found)
            {
                const QPointF c = f.base->centre();
                at = QVector3D(float(c.x()), float(c.y()), 0.0f);
            }
            const double turns = rp.pixelTurns > 0.0f ? double(rp.pixelTurns)
                                                      : autoSpiralTurns(n, double(height));
            return helix(QVector3D(at.x(), at.y(), bottom), QVector3D(at.x(), at.y(), bottom + height),
                         radius, turns, n);
        }
        default:
            break;
    }
    return out;
}

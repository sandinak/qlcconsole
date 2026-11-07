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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QtMath>
#include <algorithm>
#include <utility>

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
        case SprinkleLoops: return QCoreApplication::translate("PixelPath", "Sprinkle loops (over the gap)");
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

/*********************************************************************
 * Sprinkle loops
 *********************************************************************/

QVector<SprinkleSpan> PixelPath::sprinkleLayout(const FixtureRigProps &rp, int n)
{
    QVector<SprinkleSpan> out;
    if (n <= 0)
        return out;
    if (!rp.sprinkleSpans.isEmpty())
    {
        QVector<SprinkleSpan> t = rp.sprinkleSpans;
        std::stable_sort(t.begin(), t.end(), [](const SprinkleSpan &a, const SprinkleSpan &b) {
            return a.firstLed < b.firstLed;
        });
        int last = 0;
        for (SprinkleSpan sp : std::as_const(t))
        {
            sp.firstLed = qMax(sp.firstLed, last + 1);
            sp.lastLed = qMin(sp.lastLed, n);
            sp.point = qBound(0.0f, sp.point, 1.0f);
            if (sp.lastLed < sp.firstLed)
                continue;
            out << sp;
            last = sp.lastLed;
        }
        return out;
    }
    const int points = qMax(1, rp.sprinklePoints), per = qMax(1, rp.sprinkleSpansPerPoint);
    const int m = qMin(n, points * per);
    for (int j = 0; j < m; ++j)
    {
        SprinkleSpan sp;
        sp.firstLed = int(qint64(j) * n / m) + 1;
        sp.lastLed = int(qint64(j + 1) * n / m);
        sp.point = float((j / per + 0.5) / points);
        out << sp;
    }
    return out;
}

double PixelPath::sprinkleSag(double chord, int leds, double meanLeds)
{
    const double share = meanLeds > 0.0 ? double(leds) / meanLeds : 1.0;
    return chord * qBound(0.1, 0.3 * share, 1.0);
}

QString PixelPath::checkSprinkleSpans(const QVector<SprinkleSpan> &spans, int leds)
{
    QVector<SprinkleSpan> t = spans;
    for (int i = 0; i < t.size(); ++i)
    {
        const SprinkleSpan &sp = t.at(i);
        if (sp.firstLed < 1)
            return QCoreApplication::translate("PixelPath", "Span %1 starts before LED 1.").arg(i + 1);
        if (sp.lastLed < sp.firstLed)
            return QCoreApplication::translate("PixelPath", "Span %1 ends before it starts.").arg(i + 1);
        if (leds > 0 && sp.lastLed > leds)
            return QCoreApplication::translate("PixelPath", "Span %1 runs past the last LED (%2).")
                .arg(i + 1).arg(leds);
        if (sp.point < 0.0f || sp.point > 1.0f)
            return QCoreApplication::translate("PixelPath", "Span %1 is not on the wall (0 to 100%).").arg(i + 1);
    }
    std::stable_sort(t.begin(), t.end(), [](const SprinkleSpan &a, const SprinkleSpan &b) {
        return a.firstLed < b.firstLed;
    });
    for (int i = 1; i < t.size(); ++i)
        if (t.at(i).firstLed <= t.at(i - 1).lastLed)
            return QCoreApplication::translate("PixelPath", "LEDs %1 to %2 and %3 to %4 overlap.")
                .arg(t.at(i - 1).firstLed).arg(t.at(i - 1).lastLed)
                .arg(t.at(i).firstLed).arg(t.at(i).lastLed);
    return QString();
}

QString PixelPath::sprinkleSpansToString(const QVector<SprinkleSpan> &spans)
{
    QStringList parts;
    foreach (const SprinkleSpan &sp, spans)
        parts << QStringLiteral("%1-%2@%3").arg(sp.firstLed).arg(sp.lastLed)
                     .arg(double(sp.point), 0, 'f', 4);
    return parts.join(QLatin1Char(';'));
}

QVector<SprinkleSpan> PixelPath::sprinkleSpansFromString(const QString &text)
{
    QVector<SprinkleSpan> out;
    foreach (const QString &part, text.split(QLatin1Char(';'), Qt::SkipEmptyParts))
    {
        const int dash = part.indexOf(QLatin1Char('-')), at = part.indexOf(QLatin1Char('@'));
        if (dash <= 0 || at <= dash)
            continue;
        bool ok1 = false, ok2 = false, ok3 = false;
        SprinkleSpan sp;
        sp.firstLed = part.left(dash).trimmed().toInt(&ok1);
        sp.lastLed = part.mid(dash + 1, at - dash - 1).trimmed().toInt(&ok2);
        sp.point = part.mid(at + 1).trimmed().toFloat(&ok3);
        if (ok1 && ok2 && ok3)
            out << sp;
    }
    return out;
}

bool PixelPath::sprinkleSpansFromWled(const QByteArray &json, int spansPerPoint,
                                      QVector<SprinkleSpan> &spans, int &points,
                                      QString *error, QList<int> *notReversed)
{
    spans.clear();
    points = 0;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    QJsonObject root = doc.object();
    // /json/state is the state itself; /json has it under "state".
    if (!root.contains(QStringLiteral("seg")) && root.value(QStringLiteral("state")).isObject())
        root = root.value(QStringLiteral("state")).toObject();
    if (pe.error != QJsonParseError::NoError || !root.value(QStringLiteral("seg")).isArray())
    {
        if (error != nullptr)
            *error = QCoreApplication::translate("PixelPath", "The reply is not a WLED state with segments.");
        return false;
    }
    struct Seg { int start; int stop; bool rev; };
    QVector<Seg> segs;
    const QJsonArray segArray = root.value(QStringLiteral("seg")).toArray();
    for (const QJsonValue &v : segArray)
    {
        const QJsonObject o = v.toObject();
        const int start = o.value(QStringLiteral("start")).toInt(-1);
        const int stop = o.value(QStringLiteral("stop")).toInt(-1);
        if (start < 0 || stop <= start)
            continue;               // an unused slot
        segs << Seg{ start, stop, o.value(QStringLiteral("rev")).toBool(false) };
    }
    if (segs.isEmpty())
    {
        if (error != nullptr)
            *error = QCoreApplication::translate("PixelPath", "The WLED has no segments set up.");
        return false;
    }
    std::stable_sort(segs.begin(), segs.end(), [](const Seg &a, const Seg &b) { return a.start < b.start; });
    const int per = qMax(1, spansPerPoint);
    points = (segs.size() + per - 1) / per;
    for (int g = 0; g < segs.size(); ++g)
    {
        SprinkleSpan sp;
        sp.firstLed = segs.at(g).start + 1;
        sp.lastLed = segs.at(g).stop;
        sp.point = float((g / per + 0.5) / points);
        spans << sp;
        // Every second span runs outer to inner on the string: reversed.
        if (notReversed != nullptr && segs.at(g).rev != (g % 2 == 1))
            *notReversed << g + 1;
    }
    return true;
}

namespace
{
/** A wall's face along its top: @p side = +1 its outside, -1 its inside. */
QVector<QVector3D> faceTop(const Scenery *s, float z, int side, int samples = 96)
{
    QVector<QVector3D> line;
    const double off = side * double(s->thickness()) / 2.0;
    for (int i = 0; i <= samples; ++i)
        line << s->pointAt(-1.0 + 2.0 * i / samples, double(z), off);
    return line;
}

/** The point of @p line nearest @p p, seen from above. */
QVector3D nearestOn(const QVector<QVector3D> &line, const QVector3D &p)
{
    QVector3D best = line.isEmpty() ? p : line.first();
    double bestD = 1e18;
    const QPointF q(double(p.x()), double(p.y()));
    for (int i = 0; i + 1 < line.size(); ++i)
    {
        const QPointF a(double(line.at(i).x()), double(line.at(i).y()));
        const QPointF b(double(line.at(i + 1).x()), double(line.at(i + 1).y()));
        const QPointF ab = b - a;
        const double len2 = QPointF::dotProduct(ab, ab);
        const double t = len2 > 0.0 ? qBound(0.0, QPointF::dotProduct(q - a, ab) / len2, 1.0) : 0.0;
        const QPointF h = a + ab * t;
        const QPointF d = q - h;
        const double dd = QPointF::dotProduct(d, d);
        if (dd < bestD)
        {
            bestD = dd;
            best = line.at(i) + (line.at(i + 1) - line.at(i)) * float(t);
        }
    }
    return best;
}

double lineLength(const QVector<QVector3D> &line)
{
    double len = 0.0;
    for (int i = 1; i < line.size(); ++i)
        len += double((line.at(i) - line.at(i - 1)).length());
    return len;
}
}

static QVector<QVector3D> sprinkles(MonitorProperties *props, const Fountain::Parts &f,
                                    const FixtureRigProps &rp, int n)
{
    QVector<QVector3D> out;
    const float zi = Fountain::innerTop(f) + 0.005f;
    if (f.outer == nullptr || f.outer == f.inner)
        return PixelPath::along(wallTop(f.inner, zi), n);   // no gap to cross
    const float zo = f.outer->bottom() + f.outer->height() + 0.005f;
    const float floorZ = Fountain::baseTop(props, f) + 0.03f;
    const QVector<QVector3D> innerFace = faceTop(f.inner, zi, +1);
    const QVector<QVector3D> outerFace = faceTop(f.outer, zo, -1);
    const double innerLen = qMax(0.1, lineLength(innerFace));
    auto innerAt = [&](double u) {
        return f.inner->pointAt(qBound(-1.0, u, 1.0), double(zi), double(f.inner->thickness()) / 2.0);
    };
    // Along a wall's top between two places (u), on the inner or outer wall.
    auto run = [&](bool onInner, double u0, double u1) {
        QVector<QVector3D> line;
        const int steps = 16;
        for (int i = 0; i <= steps; ++i)
        {
            const QVector3D p = innerAt(u0 + (u1 - u0) * i / steps);
            line << (onInner ? p : nearestOn(outerFace, p));
        }
        return line;
    };

    const QVector<SprinkleSpan> spans = PixelPath::sprinkleLayout(rp, n);
    if (spans.isEmpty())
        return PixelPath::along(innerFace, n);

    // Where each span crosses, round the U: the ones at one place side by side.
    QVector<double> us(spans.size());
    double meanLeds = 0.0;
    for (int g = 0; g < spans.size(); ++g)
    {
        int first = g, count = 0;
        while (first > 0 && qFuzzyCompare(spans.at(first - 1).point + 1.0f, spans.at(g).point + 1.0f))
            --first;
        while (first + count < spans.size()
               && qFuzzyCompare(spans.at(first + count).point + 1.0f, spans.at(g).point + 1.0f))
            ++count;
        const double pitchU = 2.0 * PixelPath::kSprinklePitch / innerLen;
        us[g] = -1.0 + 2.0 * double(spans.at(g).point) + (g - first - (count - 1) / 2.0) * pitchU;
        meanLeds += spans.at(g).lastLed - spans.at(g).firstLed + 1;
    }
    meanLeds /= spans.size();

    out.fill(innerAt(us.first()), n);
    auto place = [&out, n](int firstLed, const QVector<QVector3D> &pts) {
        for (int i = 0; i < pts.size(); ++i)
            if (firstLed - 1 + i >= 0 && firstLed - 1 + i < n)
                out[firstLed - 1 + i] = pts.at(i);
    };
    int next = 1;               // the first LED not laid yet
    bool atInner = true;        // where the string is: the first span starts on the inner wall
    double uAt = -1.0;          // and how far round
    for (int g = 0; g < spans.size(); ++g)
    {
        const SprinkleSpan &sp = spans.at(g);
        // The run to it, along the top of the wall the string is on.
        if (sp.firstLed > next)
            place(next, PixelPath::along(run(atInner, g == 0 ? -1.0 : uAt, us.at(g)), sp.firstLed - next));
        // The span: a droop from one wall's top to the other's.
        const QVector3D a = innerAt(us.at(g));
        const QVector3D b = nearestOn(outerFace, a);
        const QVector3D from = atInner ? a : b, to = atInner ? b : a;
        const int leds = sp.lastLed - sp.firstLed + 1;
        const double chord = double((to - from).length());
        const double sag = qMin(PixelPath::sprinkleSag(chord, leds, meanLeds),
                                qMax(0.0, double(qMin(from.z(), to.z()) - floorZ)));
        QVector<QVector3D> arc;
        const int steps = 24;
        for (int i = 0; i <= steps; ++i)
        {
            const double t = double(i) / steps;
            arc << from + (to - from) * float(t) - QVector3D(0.0f, 0.0f, float(4.0 * sag * t * (1.0 - t)));
        }
        place(sp.firstLed, PixelPath::along(arc, leds));
        atInner = !atInner;
        uAt = us.at(g);
        next = sp.lastLed + 1;
    }
    // Whatever is left runs on along the wall to its end.
    if (next <= n)
        place(next, PixelPath::along(run(atInner, uAt, 1.0), n - next + 1));
    return out;
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
        case SprinkleLoops:
            return sprinkles(props, f, rp, n);
        default:
            break;
    }
    return out;
}

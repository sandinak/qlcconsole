/*
  Q Light Controller Plus
  performerfill.cpp

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
#include <QRandomGenerator>
#include <QSet>
#include <QtMath>

#include "performerfill.h"
#include "monitorproperties.h"
#include "qlcpalette.h"
#include "doc.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "linecurve.h"

namespace
{
    QString trp(const char *s)
    {
        return QCoreApplication::translate("PerformerFill", s);
    }

    /** The topmost deck under (x, y), counting what it stands on. */
    quint32 topmostPlatformAt(const MonitorProperties *props, float x, float y)
    {
        quint32 best = StagePlatform::invalidId();
        float bestH = -1.0f;
        foreach (const StagePlatform *p, props->platforms())
        {
            if (!p->containsPoint(x, y))
                continue;
            const float h = props->platformBaseZ(p->id()) + p->topAt(x, y);
            if (h > bestH)
            {
                bestH = h;
                best = p->id();
            }
        }
        return best;
    }

    /** 0..1 from a seed and an index: the same pair, the same number
     *  (splitmix64). */
    double unitHash(quint32 seed, int index)
    {
        quint64 z = (quint64(seed) << 32) ^ quint64(quint32(index)) ^ 0x9E3779B97F4A7C15ULL;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        z = z ^ (z >> 31);
        return double(z >> 11) / double(1ULL << 53);
    }

    /** One end of a row to the other, along it from its start: the first
     *  and last person and how many, for a row @p len long. */
    bool rowPeople(double len, const PerformerFill::Options &o, bool staggered,
                   double *first, double *last, int *count)
    {
        if (len <= 0.0)
            return false;
        if (o.perRow > 0)
        {
            const double gap = len / o.perRow;
            *first = gap * 0.5;
            *last = len - gap * 0.5;
            *count = o.perRow;
            return true;
        }
        const double s = qMax(0.2, double(o.spacing));
        int n = qMax(1, int(len / s + 1e-4));
        double start = (len - (n - 1) * s) / 2.0;
        if (staggered && o.stagger && n > 1)
        {
            // Between the places of the row in front: a face in every window.
            start += s * 0.5;
            --n;
        }
        *first = start;
        *last = start + (n - 1) * s;
        *count = n;
        return true;
    }
}

float PerformerFill::presetHeight(HeightPreset p)
{
    switch (p)
    {
    case Children: return 1.30f;
    case Students: return 1.60f;
    case Adults:
    default:       return StageTarget::defaultPersonHeight();
    }
}

float PerformerFill::personHeight(const StageTarget *t, int index)
{
    if (t == nullptr)
        return 0.0f;
    const double u = unitHash(t->seed(), index);
    return t->effectivePersonHeight() + float((2.0 * u - 1.0) * double(t->heightSpread()));
}

QVector2D PerformerFill::facingAt(const MonitorProperties *props, float x, float y)
{
    if (props == nullptr)
        return QVector2D(0.0f, 1.0f);
    const StagePlatform *top = nullptr;
    float topZ = 0.0f;
    foreach (const StagePlatform *p, props->platforms())
    {
        if (p == nullptr || !p->containsPoint(x, y))
            continue;
        const float z = props->platformBaseZ(p->id()) + p->topAt(x, y);
        if (top == nullptr || z > topZ)
        {
            top = p;
            topZ = z;
        }
    }
    // The highest deck decides; an unturned one (or none) faces the house.
    if (top == nullptr || !top->isRotated())
        return QVector2D(0.0f, 1.0f);
    const QPointF c = top->centre();
    const QPointF v = top->toWorld(c + QPointF(0.0, 1.0)) - top->toWorld(c);
    return QVector2D(float(v.x()), float(v.y())).normalized();
}

QList<PerformerFill::Performer> PerformerFill::people(const MonitorProperties *props, const StageTarget *t)
{
    QList<Performer> out;
    if (props == nullptr || t == nullptr || !t->isPerformerLine())
        return out;
    const LineCurve curve(props->targetLinePoints(t));
    if (!curve.isValid())
        return out;
    const int n = t->performerCount();
    for (int i = 0; i < n; ++i)
    {
        // The first on one end, the last on the other; one alone in the middle.
        const double f = (n == 1) ? 0.5 : double(i) / double(n - 1);
        const QVector3D at = curve.pointAt(f);
        Performer p;
        p.feet = QVector3D(at.x(), at.y(), props->platformHeightAt(at.x(), at.y()));
        p.height = personHeight(t, i);
        p.facing = facingAt(props, at.x(), at.y());
        out << p;
    }
    return out;
}

QList<PerformerFill::Row> PerformerFill::rows(const MonitorProperties *props, const StagePlatform *p,
                                              const Options &o)
{
    QList<Row> out;
    if (props == nullptr || p == nullptr)
        return out;

    /* Rows in local plan coordinates: a line across the platform, from
       (ax, ay) along (dx, dy) for len metres. Front row first. */
    struct Line { double ax, ay, dx, dy, len; };
    QList<Line> lines;
    const double x0 = p->originX(), y0 = p->originY();
    const double x1 = x0 + p->width(), y1 = y0 + p->depth();
    if (p->isSteps())
    {
        // One row along the middle of each tread, bottom step first.
        foreach (const StagePlatform::Block &b, p->blocks())
        {
            if (p->stepsClimb() == StagePlatform::ClimbStageLeft
                || p->stepsClimb() == StagePlatform::ClimbStageRight)
                lines << Line{ (b.x0 + b.x1) / 2.0, double(b.y0), 0.0, 1.0, double(b.y1 - b.y0) };
            else
                lines << Line{ double(b.x0), (b.y0 + b.y1) / 2.0, 1.0, 0.0, double(b.x1 - b.x0) };
        }
    }
    else
    {
        /* A deck: a row to every 0.9 m of depth, across the stage, the front
           (downstage, +Y) one first. */
        const double rowDepth = 0.9;
        const int n = qMax(1, int(double(p->depth()) / rowDepth + 1e-4));
        for (int r = 0; r < n; ++r)
            lines << Line{ x0, y1 - (r + 0.5) * (y1 - y0) / n, 1.0, 0.0, x1 - x0 };
    }

    for (int r = 0; r < lines.size(); ++r)
    {
        const Line &l = lines.at(r);
        double first = 0.0, last = 0.0;
        int count = 0;
        if (!rowPeople(l.len, o, (r % 2) == 1, &first, &last, &count))
            continue;
        /* A rounded front cuts the ends off the row: pull them in a place
           at a time until both stand on it. */
        const double step = count > 1 ? (last - first) / (count - 1) : 0.0;
        auto world = [&](double along) {
            return p->toWorld(QPointF(l.ax + l.dx * along, l.ay + l.dy * along));
        };
        while (count > 0 && !p->containsPoint(float(world(first).x()), float(world(first).y())))
        {
            first += step;
            --count;
        }
        while (count > 0 && !p->containsPoint(float(world(last).x()), float(world(last).y())))
        {
            last -= step;
            --count;
        }
        if (count <= 0)
            continue;
        const QPointF wa = world(first), wb = world(last);
        Row row;
        row.a = QVector3D(float(wa.x()), float(wa.y()), props->platformHeightAt(float(wa.x()), float(wa.y())));
        row.b = QVector3D(float(wb.x()), float(wb.y()), props->platformHeightAt(float(wb.x()), float(wb.y())));
        row.count = count;
        out << row;
    }
    return out;
}

StageTarget *PerformerFill::addLine(MonitorProperties *props, const QVector3D &a, const QVector3D &b,
                                    int count, const Options &o, quint32 layerId)
{
    if (props == nullptr || count <= 0)
        return nullptr;
    QVector3D ea(a.x(), a.y(), 0.0f), eb(b.x(), b.y(), 0.0f);
    /* One alone, or two ends on one spot: a short line across the stage,
       the person in its middle -- a line needs two ends to drag. */
    if ((eb - ea).length() < 0.05f)
    {
        const QVector3D mid = (ea + eb) * 0.5f;
        ea = mid - QVector3D(0.3f, 0.0f, 0.0f);
        eb = mid + QVector3D(0.3f, 0.0f, 0.0f);
    }
    const QVector3D centre = (ea + eb) * 0.5f;

    StageTarget *t = props->addStageTarget();
    t->makeLine();
    /* The line runs at their chests, so light aimed along it lands on
       them; the people themselves stand on whatever is under them. */
    const float chest = o.height * StageTarget::chestShare();
    t->setPosition(QVector3D(centre.x(), centre.y(), props->platformHeightAt(centre.x(), centre.y())));
    t->setLinePoints({ ea - centre + QVector3D(0.0f, 0.0f, chest),
                       eb - centre + QVector3D(0.0f, 0.0f, chest) });
    t->setPerformerCount(count);
    t->setPersonHeight(o.height);
    t->setHeightSpread(o.spread);
    t->setSeed(QRandomGenerator::global()->generate());
    t->setCostumeColor(o.costume);
    t->setLayerId(layerId);
    return t;
}

quint32 PerformerFill::ensureLayer(MonitorProperties *props)
{
    const QString name = trp("Performers");
    foreach (const MonitorProperties::MonitorLayer &l, props->layers())
        if (l.id != MonitorProperties::defaultLayerId && l.name == name)
            return l.id;
    return props->addLayer(name);
}

bool PerformerFill::isPerformer(const MonitorProperties *props, const StageTarget *t)
{
    if (props == nullptr || t == nullptr)
        return false;
    if (t->isPerformerLine())
        return true;
    if (t->kind() != StageTarget::Kind::Person
        || t->layerId() == MonitorProperties::defaultLayerId || !props->hasLayer(t->layerId()))
        return false;
    return props->layer(t->layerId()).name == trp("Performers");
}

QList<StageTarget *> PerformerFill::setMates(const MonitorProperties *props, const StageTarget *t)
{
    QList<StageTarget *> out;
    if (props == nullptr || t == nullptr || !t->isPerformerLine() || t->groupId() == 0)
        return out;
    foreach (StageTarget *o, props->stageTargets())
        if (o != nullptr && o != t && o->isPerformerLine() && o->groupId() == t->groupId())
            out << o;
    return out;
}

int PerformerFill::copyLookToSet(const MonitorProperties *props, const StageTarget *from)
{
    const QList<StageTarget *> mates = setMates(props, from);
    foreach (StageTarget *o, mates)
    {
        o->setCostumeColor(from->costumeColor());
        o->setPersonHeight(from->effectivePersonHeight());
        o->setHeightSpread(from->heightSpread());
    }
    return mates.size();
}

QList<StageTarget *> PerformerFill::pickable(Doc *doc, quint32 keepId)
{
    QList<StageTarget *> out;
    const MonitorProperties *props = doc == nullptr ? nullptr : doc->monitorProperties();
    if (props == nullptr)
        return out;
    QSet<quint32> used;
    foreach (const QLCPalette *p, doc->palettes())
        used.insert(p->stageTargetId());
    foreach (StageTarget *t, props->stageTargets())
    {
        if (t == nullptr)
            continue;
        if (t->isPerformerLine() && t->id() != keepId && !used.contains(t->id())
            && !setMates(props, t).isEmpty())
            continue;
        out << t;
    }
    return out;
}

QList<StageTarget *> PerformerFill::standingOn(const MonitorProperties *props, quint32 platformId)
{
    QList<StageTarget *> out;
    if (props == nullptr)
        return out;
    foreach (StageTarget *t, props->stageTargets())
    {
        if (t == nullptr || t->isBound()
            || (t->kind() != StageTarget::Kind::Person && !t->isPerformerLine()))
            continue;
        if (topmostPlatformAt(props, t->x(), t->y()) == platformId)
            out << t;
    }
    return out;
}

PerformerFill::Result PerformerFill::fill(MonitorProperties *props, const QList<quint32> &platformIds,
                                          const Options &o, const std::function<bool(quint32)> &inUse)
{
    Result res;
    if (props == nullptr)
        return res;
    res.layerId = ensureLayer(props);

    /* Fill again: what already stands on these risers in the Performers
       layer makes way, and a set left with nothing in it goes too. */
    if (o.replace)
    {
        QSet<quint32> emptied;
        foreach (quint32 pid, platformIds)
            foreach (StageTarget *t, standingOn(props, pid))
            {
                if (t->layerId() != res.layerId || (inUse && inUse(t->id())))
                    continue;
                if (t->groupId() != 0)
                    emptied.insert(t->groupId());
                props->removeStageTarget(t->id());
                ++res.replaced;
            }
        foreach (StageTarget *t, props->stageTargets())
            emptied.remove(t->groupId());
        foreach (const MonitorProperties::MonitorGroup &g, props->groups())
            emptied.remove(g.parentGroupId);
        foreach (quint32 fid, props->fixtureItemsID())
            emptied.remove(props->fixtureGroup(fid));
        foreach (quint32 gid, emptied)
            if (props->group(gid).layerId == res.layerId)
                props->removeGroup(gid);
    }

    QList<Row> all;
    foreach (quint32 pid, platformIds)
        all << rows(props, props->platform(pid), o);
    if (all.isEmpty())
        return res;

    const QString setName = o.setName.trimmed().isEmpty() ? trp("Performers") : o.setName.trimmed();
    res.groupId = props->nextGroupId();
    props->createGroup(res.groupId, setName, res.layerId, 0);

    int n = 0;
    foreach (const Row &row, all)
    {
        StageTarget *t = addLine(props, row.a, row.b, row.count, o, res.layerId);
        if (t == nullptr)
            continue;
        t->setName(trp("%1 row %2").arg(setName).arg(++n));
        t->setGroupId(res.groupId);
        res.targets << t->id();
        res.people += row.count;
    }
    props->notifyStageTargetsChanged();
    return res;
}

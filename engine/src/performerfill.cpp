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
#include <QSet>
#include <QtMath>

#include "performerfill.h"
#include "monitorproperties.h"
#include "stageplatform.h"
#include "stagetarget.h"

namespace
{
    QString trp(const char *s)
    {
        return QCoreApplication::translate("PerformerFill", s);
    }

    /** Places along a row @p len long, from its start. */
    QList<double> rowPlaces(double len, const PerformerFill::Options &o, bool staggered)
    {
        QList<double> out;
        if (len <= 0.0)
            return out;
        if (o.perRow > 0)
        {
            for (int i = 0; i < o.perRow; ++i)
                out << (i + 0.5) * len / o.perRow;
            return out;
        }
        const double s = qMax(0.2, double(o.spacing));
        const int n = qMax(1, int(len / s + 1e-4));
        const double start = (len - (n - 1) * s) / 2.0;
        if (staggered && o.stagger && n > 1)
        {
            // Between the places of the row in front: a face in every window.
            for (int i = 0; i + 1 < n; ++i)
                out << start + s * 0.5 + i * s;
            return out;
        }
        for (int i = 0; i < n; ++i)
            out << start + i * s;
        return out;
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

QList<QVector3D> PerformerFill::spots(const MonitorProperties *props, const StagePlatform *p,
                                      const Options &o)
{
    QList<QVector3D> out;
    if (props == nullptr || p == nullptr)
        return out;

    /* Rows in local plan coordinates: a line across the platform, from
       (ax, ay) along (dx, dy) for len metres. Front row first. */
    struct Row { double ax, ay, dx, dy, len; };
    QList<Row> rows;
    const double x0 = p->originX(), y0 = p->originY();
    const double x1 = x0 + p->width(), y1 = y0 + p->depth();
    if (p->isSteps())
    {
        // One row along the middle of each tread, bottom step first.
        foreach (const StagePlatform::Block &b, p->blocks())
        {
            if (p->stepsClimb() == StagePlatform::ClimbStageLeft
                || p->stepsClimb() == StagePlatform::ClimbStageRight)
                rows << Row{ (b.x0 + b.x1) / 2.0, double(b.y0), 0.0, 1.0, double(b.y1 - b.y0) };
            else
                rows << Row{ double(b.x0), (b.y0 + b.y1) / 2.0, 1.0, 0.0, double(b.x1 - b.x0) };
        }
    }
    else
    {
        /* A deck: a row to every 0.9 m of depth, across the stage, the front
           (downstage, +Y) one first. */
        const double rowDepth = 0.9;
        const int n = qMax(1, int(double(p->depth()) / rowDepth + 1e-4));
        for (int r = 0; r < n; ++r)
            rows << Row{ x0, y1 - (r + 0.5) * (y1 - y0) / n, 1.0, 0.0, x1 - x0 };
    }

    for (int r = 0; r < rows.size(); ++r)
    {
        const Row &row = rows.at(r);
        foreach (double along, rowPlaces(row.len, o, (r % 2) == 1))
        {
            const QPointF w = p->toWorld(QPointF(row.ax + row.dx * along, row.ay + row.dy * along));
            if (!p->containsPoint(float(w.x()), float(w.y())))
                continue;                       // cut off by a rounded front
            out << QVector3D(float(w.x()), float(w.y()),
                             props->platformHeightAt(float(w.x()), float(w.y())));
        }
    }
    return out;
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
    if (props == nullptr || t == nullptr || t->kind() != StageTarget::Kind::Person
        || t->layerId() == MonitorProperties::defaultLayerId || !props->hasLayer(t->layerId()))
        return false;
    return props->layer(t->layerId()).name == trp("Performers");
}

QList<StageTarget *> PerformerFill::standingOn(const MonitorProperties *props, quint32 platformId)
{
    QList<StageTarget *> out;
    if (props == nullptr)
        return out;
    foreach (StageTarget *t, props->stageTargets())
    {
        if (t == nullptr || t->kind() != StageTarget::Kind::Person || t->isBound())
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

    /* Fill again: the people already on these risers in the Performers
       layer make way, and a set left with nobody in it goes too. */
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

    QList<QVector3D> feet;
    foreach (quint32 pid, platformIds)
        feet << spots(props, props->platform(pid), o);
    if (feet.isEmpty())
        return res;

    const QString setName = o.setName.trimmed().isEmpty() ? trp("Performers") : o.setName.trimmed();
    res.groupId = props->nextGroupId();
    props->createGroup(res.groupId, setName, res.layerId, 0);

    int n = 0;
    foreach (const QVector3D &at, feet)
    {
        StageTarget *t = props->addStageTarget();
        t->setKind(StageTarget::Kind::Person);
        t->setName(QStringLiteral("%1 %2").arg(setName).arg(++n));
        t->setPosition(at);
        t->setPersonHeight(o.height);
        t->setCostumeColor(o.costume);
        t->setLayerId(res.layerId);
        t->setGroupId(res.groupId);
        res.targets << t->id();
    }
    props->notifyStageTargetsChanged();
    return res;
}

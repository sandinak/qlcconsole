/*
  Q Light Controller Plus
  lineformation.cpp

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
#include <QHash>
#include <QVector3D>

#include "lineformation.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "aimsolver.h"
#include "qlcpalette.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

namespace
{
/* From above: x across (stage left is +X), y up / downstage (+Y is
   downstage, drawn towards the bottom, the audience's side). It was (x, z) --
   across and HEIGHT, a front elevation upside down -- so a hung truss drew
   below a line on the deck and every plan was a side-on picture. */
QPointF plan(const QVector3D &v)
{
    return QPointF(v.x(), v.y());
}

/* The dataviz reference categorical hues, dark steps, yellow left out (the
   line itself is amber): validated for this order on the plan surface
   #1e2026, adjacent pairs and the last with the first -- worst CVD delta E
   8.6 (red/green, protan), normal-vision 19.7, all >= 3:1 contrast. */
const QRgb kGroupColours[] = { 0x3987e5, 0xd95926, 0x199e70, 0x9085e9, 0xe66767, 0x008300, 0xd55181 };
const int kGroupColourCount = int(sizeof(kGroupColours) / sizeof(kGroupColours[0]));
}

QColor LineFormation::groupColour(int group)
{
    const int i = ((group % kGroupColourCount) + kGroupColourCount) % kGroupColourCount;
    return QColor(kGroupColours[i]);
}

LineFormation LineFormation::build(Doc *doc, const Scene *scene, const QLCPalette *look)
{
    LineFormation out;
    if (look == nullptr)
        return out;
    out.m_settings = LineSpread::fromParams(look->effectParamValues());

    MonitorProperties *props = doc != nullptr ? doc->monitorProperties() : nullptr;
    const StageTarget *target = props != nullptr ? props->stageTarget(look->stageTargetId()) : nullptr;
    QList<quint32> heads;
    if (scene != nullptr && target != nullptr && target->isLine())
        heads = AimSolver::orderedLineHeads(doc, target, AimSolver::aimHeads(doc, scene, look->id()),
                                            out.m_settings.order);

    // The engine's rule, over the same ordered list AimSolver::lineFractions uses.
    const int n = heads.isEmpty() ? 6 : heads.size();
    const QVector<double> fr = LineSpread::fractions(n, out.m_settings);
    const QVector<int> groups = LineSpread::groupOf(n, out.m_settings);
    QHash<quint32, QVector3D> points;
    if (!heads.isEmpty())
        points = AimSolver::linePoints(doc, look, target, heads);

    for (int i = 0; i < n; ++i)
    {
        Light l;
        l.number = i + 1;
        l.group = groups.value(i);
        l.slot = n > 1 ? double(i) / (n - 1) : 0.5;
        l.fraction = fr.at(i);
        out.m_groupCount = qMax(out.m_groupCount, l.group + 1);
        if (!heads.isEmpty())
        {
            l.fixture = heads.at(i);
            if (const Fixture *fx = doc->fixture(l.fixture))
                l.name = fx->name();
            if (points.contains(l.fixture))
            {
                l.placed = true;
                l.to = plan(points.value(l.fixture));
                AimSolver::Frame frame;
                l.from = AimSolver::frameFor(doc, l.fixture, frame) ? plan(frame.position) : l.to;
            }
        }
        out.m_lights << l;
    }
    return out;
}

LineFormation LineFormation::positions(Doc *doc, const QList<QList<quint32>> &groups,
                                       const QList<const QLCPalette *> &palettes)
{
    LineFormation out;
    out.m_raw = true;
    if (doc == nullptr)
        return out;
    QList<BeamLanding::Result> results;
    QList<quint32> seen;
    int number = 0;
    for (int gi = 0; gi < groups.count(); ++gi)
    {
        for (quint32 fid : groups.at(gi))
        {
            if (seen.contains(fid))
                continue;
            seen << fid;
            QList<SceneValue> values;
            for (const QLCPalette *p : palettes)
                if (p != nullptr)
                    values << const_cast<QLCPalette *>(p)->valuesFromFixtures(doc, { fid });
            // Later palettes win: keep the last value per channel.
            QList<SceneValue> last;
            for (int i = values.count() - 1; i >= 0; --i)
            {
                bool dup = false;
                for (const SceneValue &v : last)
                    if (v.fxi == values.at(i).fxi && v.channel == values.at(i).channel)
                        dup = true;
                if (!dup)
                    last << values.at(i);
            }
            const BeamLanding::Result r = BeamLanding::land(doc, fid, last);
            if (!r.valid())
                continue;
            results << r;
            Light l;
            l.fixture = fid;
            l.number = ++number;
            l.group = gi;
            l.placed = true;
            l.from = QPointF(r.origin.x(), r.origin.y());
            l.to = QPointF(r.point.x(), r.point.y());
            l.landing = r.kind;
            l.hits = BeamLanding::describe(doc, r);
            if (const Fixture *fx = doc->fixture(fid))
                l.name = fx->name();
            out.m_groupCount = qMax(out.m_groupCount, gi + 1);
            out.m_lights << l;
        }
    }
    out.m_landingSummary = BeamLanding::summary(doc, results);
    return out;
}

QList<QList<quint32>> LineFormation::targetHeads(Doc *doc, ShowTreeController *st, const ShowTree::Target &target)
{
    QList<QList<quint32>> out;
    if (doc == nullptr)
        return out;
    QList<quint32> groups = target.groups;
    if (st != nullptr)
        for (quint32 sid : target.sets)
            for (const ShowTree::GroupSet &gs : st->groupSets())
                if (gs.id == sid)
                    for (quint32 g : gs.groups)
                        if (!groups.contains(g))
                            groups << g;
    QList<quint32> seen;
    for (quint32 gid : groups)
    {
        const FixtureGroup *g = doc->fixtureGroup(gid);
        if (g == nullptr)
            continue;
        QList<quint32> heads;
        for (quint32 fid : AimSolver::aimable(doc, g->fixtureList()))
            if (!seen.contains(fid))
            {
                seen << fid;
                heads << fid;
            }
        if (!heads.isEmpty())
            out << heads;
    }
    QList<quint32> own;
    for (quint32 fid : AimSolver::aimable(doc, target.fixtures))
        if (!seen.contains(fid))
        {
            seen << fid;
            own << fid;
        }
    if (!own.isEmpty())
        out << own;
    return out;
}

bool LineFormation::grouped() const
{
    if (m_raw)
        return m_groupCount > 1;
    return m_groupCount > 1 && m_groupCount < m_lights.count();
}

int LineFormation::crossings() const
{
    // Lights in the order they hang; a pair that lands the other way round crosses.
    int count = 0;
    for (int i = 0; i < m_lights.count(); ++i)
        for (int j = i + 1; j < m_lights.count(); ++j)
            if (m_lights.at(i).fraction > m_lights.at(j).fraction + 1e-9)
                ++count;
    return count;
}

bool LineFormation::crossIdle() const
{
    return !m_raw && m_settings.spread >= 0.0 && m_lights.count() > 1;
}

QColor LineFormation::colourFor(const Light &light) const
{
    return grouped() ? groupColour(light.group) : pointColour();
}

QString LineFormation::caption() const
{
    if (m_lights.isEmpty())
        return QString();
    if (m_raw)
        return grouped() ? QCoreApplication::translate("LineFormation", "Stored position: %1. A colour per group.")
                                   .arg(m_landingSummary)
                         : QCoreApplication::translate("LineFormation", "Stored position: %1.").arg(m_landingSummary);
    QString what;
    if (grouped())
    {
        QString grouping;
        if (m_settings.group == LineSpread::Halves)
            grouping = QCoreApplication::translate("LineFormation", "Halves");
        else if (m_settings.group == 2)
            grouping = QCoreApplication::translate("LineFormation", "Pairs");
        else if (m_settings.group == 3)
            grouping = QCoreApplication::translate("LineFormation", "Threes");
        else if (m_settings.group == 4)
            grouping = QCoreApplication::translate("LineFormation", "Fours");
        else
            grouping = QCoreApplication::translate("LineFormation", "Groups of %1").arg(m_settings.group);
        what = QCoreApplication::translate("LineFormation", "%1: %2 groups, a colour each.")
                .arg(grouping).arg(m_groupCount);
    }
    QString cross;
    const int c = crossings();
    if (crossIdle())
        cross = QCoreApplication::translate("LineFormation", "Nothing crosses at Spread %1 (Cross acts below 0).")
                .arg(m_settings.spread, 0, 'f', 1);
    else if (c == 0)
        cross = QCoreApplication::translate("LineFormation", "No beams cross.");
    else
    {
        QString how;
        if (m_settings.share || m_groupCount == m_lights.count())
            how = QCoreApplication::translate("LineFormation", "Crossed");
        else if (m_groupCount == 1 || m_settings.cross == LineSpread::Mirror)
            how = QCoreApplication::translate("LineFormation", "Each group flips");
        else
            how = QCoreApplication::translate("LineFormation", "Groups swap sides");
        cross = c == 1 ? QCoreApplication::translate("LineFormation", "%1: 1 beam crossing.").arg(how)
                       : QCoreApplication::translate("LineFormation", "%1: %2 beam crossings.").arg(how).arg(c);
    }
    return what.isEmpty() ? cross : what + QLatin1Char(' ') + cross;
}

QPolygonF LineFormation::arrowhead(const QPointF &tail, const QPointF &tip, double size)
{
    const QLineF l(tail, tip);
    if (l.length() < size)
        return QPolygonF();
    const QPointF d = (tip - tail) / l.length();
    const QPointF base = tip - d * size;
    const QPointF n(-d.y(), d.x());
    return QPolygonF({ tip, base + n * (size * 0.4), base - n * (size * 0.4) });
}

QPointF LineFormation::arrowBase(const QPointF &tail, const QPointF &tip, double size)
{
    const QLineF l(tail, tip);
    if (l.length() < size)
        return tip;
    return tip - (tip - tail) / l.length() * (size * 0.8);
}

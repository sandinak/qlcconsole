/*
  Q Light Controller Plus
  selectionshape.cpp

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

#include <QStringList>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>

#include "selectionshape.h"
#include "showtreecontroller.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

namespace
{

/** A group that counts: two or more fixtures, all in the show. */
struct Candidate
{
    quint32 id = 0;
    QString name;
    QList<quint32> members;     //!< its order, deduplicated
    QSet<quint32> set;
    int usage = 0;
};

QList<Candidate> candidates(const Doc *doc)
{
    QList<Candidate> out;
    for (FixtureGroup *g : doc->fixtureGroups())
    {
        if (g == nullptr)
            continue;
        Candidate c;
        c.id = g->id();
        c.name = g->name();
        for (quint32 fid : g->fixtureList())
            if (doc->fixture(fid) != nullptr && !c.set.contains(fid))
            {
                c.members << fid;
                c.set.insert(fid);
            }
        if (c.members.size() < 2)
            continue;
        c.usage = SelectionShape::groupUsage(doc, c.id);
        out << c;
    }
    return out;
}

/** The tie-break: used most, then name, then id. */
bool before(const Candidate &a, const Candidate &b)
{
    if (a.usage != b.usage)
        return a.usage > b.usage;
    const int byName = QString::localeAwareCompare(a.name, b.name);
    if (byName != 0)
        return byName < 0;
    return a.id < b.id;
}

double distanceToRect(const QPointF &p, const QRectF &r)
{
    const double dx = std::max({ r.left() - p.x(), 0.0, p.x() - r.right() });
    const double dy = std::max({ r.top() - p.y(), 0.0, p.y() - r.bottom() });
    return std::sqrt(dx * dx + dy * dy);
}

/** The group's fixture spacing: the mean distance to the nearest member
 *  (0: fewer than two placed). */
double spacingOf(const Candidate &c, const QHash<quint32, QPointF> &positions)
{
    QList<QPointF> pts;
    for (quint32 fid : c.members)
        if (positions.contains(fid))
            pts << positions.value(fid);
    if (pts.size() < 2)
        return 0.0;
    double sum = 0.0;
    for (int i = 0; i < pts.size(); ++i)
    {
        double best = std::numeric_limits<double>::max();
        for (int j = 0; j < pts.size(); ++j)
            if (i != j)
            {
                const QPointF d = pts.at(i) - pts.at(j);
                best = std::min(best, std::sqrt(d.x() * d.x() + d.y() * d.y()));
            }
        sum += best;
    }
    return sum / pts.size();
}

}

int SelectionShape::groupUsage(const Doc *doc, quint32 groupId)
{
    int n = 0;
    for (Function *f : doc->functions())
        if (Scene *s = qobject_cast<Scene *>(f))
            if (s->fixtureGroups().contains(groupId))
                ++n;
    if (const ShowTreeController *st = doc->showTree())
        for (const ShowTree::Production &p : st->productions())
            for (const ShowTree::CueList &l : p.lists)
                for (const ShowTree::Cue &c : l.cues)
                    for (const ShowTree::Layer &layer : c.layers)
                        if (layer.target.groups.contains(groupId))
                            ++n;
    return n;
}

SelectionShape SelectionShape::resolve(const Doc *doc, const QList<quint32> &fixtureIds)
{
    SelectionShape shape;
    QList<quint32> sel;
    for (quint32 fid : fixtureIds)
        if (!sel.contains(fid))
            sel << fid;
    if (doc == nullptr || sel.size() < 2)
    {
        shape.fixtures = sel;
        return shape;
    }
    const QSet<quint32> s(sel.begin(), sel.end());
    QList<Candidate> inside;
    for (const Candidate &c : candidates(doc))
        if (c.set.size() <= s.size() && s.contains(c.set))
            inside << c;
    // Largest first (a parent equal to the union beats its parts), then
    // the tie-break.
    std::sort(inside.begin(), inside.end(), [](const Candidate &a, const Candidate &b) {
        if (a.members.size() != b.members.size())
            return a.members.size() > b.members.size();
        return before(a, b);
    });
    QSet<quint32> covered;
    for (const Candidate &c : inside)
    {
        if (c.set.intersects(covered))
            continue;
        shape.groups << c.id;
        covered.unite(c.set);
    }
    for (quint32 fid : sel)
        if (!covered.contains(fid))
            shape.fixtures << fid;
    return shape;
}

QString SelectionShape::label(const Doc *doc, const QList<quint32> &fixtureIds)
{
    if (fixtureIds.isEmpty())
        return QString();
    const SelectionShape shape = resolve(doc, fixtureIds);
    auto fixtures = [](int n) {
        return n == 1 ? tr("1 fixture") : tr("%1 fixtures").arg(n);
    };
    if (shape.groups.isEmpty())
        return fixtures(int(shape.fixtures.size()));
    QStringList names;
    int count = 0;
    for (quint32 gid : shape.groups)
        if (FixtureGroup *g = doc->fixtureGroup(gid))
        {
            names << g->name();
            QSet<quint32> m;
            for (quint32 fid : g->fixtureList())
                if (doc->fixture(fid) != nullptr)
                    m.insert(fid);
            count += m.size();
        }
    const QString groups = names.join(QStringLiteral(" + "));
    if (shape.fixtures.isEmpty())
        return tr("%1 (%2)").arg(groups).arg(count);
    return tr("%1 + %2").arg(groups, fixtures(int(shape.fixtures.size())));
}

SelectionShape::Hint SelectionShape::hint(const Doc *doc, const QList<quint32> &fixtureIds,
                                          const QHash<quint32, QPointF> &positions, const QRectF &band)
{
    Hint best;
    if (doc == nullptr || fixtureIds.size() < 2)
        return best;
    const QSet<quint32> s(fixtureIds.begin(), fixtureIds.end());
    // Whole groups already: nothing is missed.
    const SelectionShape shape = resolve(doc, fixtureIds);
    if (!shape.groups.isEmpty() && shape.fixtures.isEmpty())
        return best;

    Candidate bestGroup;
    for (const Candidate &c : candidates(doc))
    {
        Hint h;
        QList<quint32> missing;
        for (quint32 fid : c.members)
            if (!s.contains(fid))
                missing << fid;
        QList<quint32> stray;
        for (quint32 fid : fixtureIds)
            if (!c.set.contains(fid) && !stray.contains(fid))
                stray << fid;
        const int in = c.members.size() - missing.size();

        if (stray.isEmpty() && in >= 2 && !missing.isEmpty())
        {
            if (missing.size() <= 2 && in > missing.size())
            {
                h.kind = Hint::AddMissing;
                h.fixtures = missing;
            }
            else if (band.isValid() && !positions.isEmpty())
            {
                // A rubber band that just missed: members within a third of
                // the group's spacing of its edge.
                const double reach = spacingOf(c, positions) / 3.0;
                QList<quint32> closeBy;
                for (quint32 fid : missing)
                    if (reach > 0.0 && positions.contains(fid)
                        && distanceToRect(positions.value(fid), band) <= reach)
                        closeBy << fid;
                if (!closeBy.isEmpty() && closeBy.size() <= 2)
                {
                    h.kind = Hint::AddMissing;
                    h.fixtures = closeBy;
                    h.nearBand = true;
                }
            }
        }
        else if (missing.isEmpty() && stray.size() == 1)
        {
            h.kind = Hint::DropStray;
            h.fixtures = stray;
        }
        if (h.isNull())
            continue;

        // Fewest changes, adding before dropping, then the tie-break.
        bool better = best.isNull();
        if (!better)
        {
            if (h.fixtures.size() != best.fixtures.size())
                better = h.fixtures.size() < best.fixtures.size();
            else if (h.kind != best.kind)
                better = h.kind == Hint::AddMissing;
            else
                better = before(c, bestGroup);
        }
        if (!better)
            continue;
        h.group = c.id;
        if (h.kind == Hint::AddMissing)
        {
            h.result = fixtureIds;
            for (quint32 fid : h.fixtures)
                h.result << fid;
            if (h.nearBand)
                h.text = (h.fixtures.size() == 1 ? tr("%1: one more just outside the box — add it?").arg(c.name)
                                                 : tr("%1: two more just outside the box — add them?").arg(c.name));
            else if (h.result.size() == c.members.size())
                h.text = tr("%1 is %2 — select all %2?").arg(c.name).arg(c.members.size());
            else
                h.text = tr("%1: add the %2 missing?").arg(c.name).arg(h.fixtures.size());
        }
        else
        {
            for (quint32 fid : fixtureIds)
                if (!h.fixtures.contains(fid))
                    h.result << fid;
            Fixture *f = doc->fixture(h.fixtures.first());
            h.text = tr("%1 + %2 — drop the stray?").arg(c.name, f != nullptr ? f->name() : QString());
        }
        best = h;
        bestGroup = c;
    }
    return best;
}

/*
  Q Light Controller Plus
  showtreeconvert.cpp

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

#include <algorithm>

#include "showtreecontroller.h"
#include "doc.h"

using namespace ShowTree;

/* Convert to tracking… (SHOW_TREE_DESIGN.md §7, P4). An imported list is
   Non-tracking: every cue Block, carrying its looks forward by repeating
   them or by including the cue before. Converting makes the list Tracking
   one change at a time, each kept only if every cue still plays exactly
   what it played: the same stage (ShowTree::stageOf) and the same generated
   scenes -- target, palettes, what each leaves out for another layer and
   what it lays out over -- and opaque functions. So the DMX output is
   unchanged cue by cue, by construction, and a change that would alter it
   is simply not made (the cue stays Block, the value stays). */

namespace
{
/** What a cue plays, as the bake would make it: its scenes' signatures and
    its opaque functions, order-free (in a cue that doesn't mix, each
    fixture's attribute has one writer, so the order doesn't matter). */
typedef QMap<quint32, QStringList> Plays;
}

ShowTreeController::ConvertPlan ShowTreeController::planConvertToTracking(quint32 listId) const
{
    ConvertPlan plan;
    const CueList *src = cueList(listId);
    if (src == nullptr)
    {
        plan.why = tr("No such cue list.");
        return plan;
    }
    if (src->looks)
    {
        plan.why = tr("Looks never track: each plays only what it has.");
        return plan;
    }
    const LineAimTest aimTest = lineAimTest();
    quint32 next = m_nextId;

    auto plays = [&](const CueList &list) {
        Plays out;
        const QList<ResolvedCue> r = resolve(list, aimTest);
        for (int i = 0; i < r.count(); ++i)
        {
            QStringList sigs;
            for (const ScenePlan &p : planCue(list, r, i))
                sigs << p.signature();
            for (const ResolvedLayer &rl : r.at(i).layers)
                if (rl.target.isOpaque())
                    sigs << rl.target.signature();
            // A cue that mixes: order kept (LTP between its scenes).
            if (!list.cues.at(i).mixOverlap)
                sigs.sort();
            out.insert(r.at(i).cueId, sigs);
        }
        return out;
    };
    auto stages = [&](const CueList &list) {
        QMap<quint32, Stage> out;
        for (const ResolvedCue &rc : resolve(list, aimTest))
            out.insert(rc.cueId, stageOf(rc));
        return out;
    };
    const Plays playsBefore = plays(*src);
    const QMap<quint32, Stage> stageBefore = stages(*src);
    auto same = [&](const CueList &list) {
        return stages(list) == stageBefore && plays(list) == playsBefore;
    };
    auto label = [&](const CueList &list, quint32 cueId) {
        return cueLabel(list, list.indexOf(cueId));
    };
    auto layerName = [this](const Layer &l) {
        return l.name.isEmpty() ? targetLabel(l.target) : l.name;
    };

    CueList work = *src;
    // Each cue's mode as it plays now, made explicit; then the list tracks.
    for (Cue &c : work.cues)
    {
        if (c.mode != CueMode::ListDefault)
            continue;
        if (c.reusable != invalidId())
        {
            plan.why = tr("Cue %1 is a use of the reusable cue %2 and plays the list's mode: give it a "
                          "mode of its own first (or Unlink it here).")
                       .arg(label(work, c.id), reusableName(c.reusable));
            return plan;
        }
        c.mode = effectiveMode(*src, c);
    }
    work.mode = ListMode::Tracking;
    if (!same(work))
    {
        plan.why = tr("The list can't be made tracking without changing what it plays.");
        return plan;
    }

    // 1. Each Block cue tracks from the one before, where it then plays the
    //    same: an include of the cue before goes (tracking brings it), and a
    //    layer the cue before passes on but this cue doesn't light is
    //    released there (Release goes to the base cue, else dark).
    for (int i = 1; i < work.cues.count(); ++i)
    {
        const Cue &c = work.cues.at(i);
        if (c.mode != CueMode::Block || c.reusable != invalidId())
            continue;
        const quint32 prevId = work.cues.at(i - 1).id;
        const QList<ResolvedCue> now = resolve(work, aimTest);
        QSet<QString> lit;
        for (const ResolvedLayer &rl : now.at(i).layers)
            lit.insert(rl.target.signature());

        auto attempt = [&](bool dropInclude, QStringList *said, int *releases, bool *dropped) -> bool {
            CueList cand = work;
            Cue &cc = cand.cues[i];
            Cue &prev = cand.cues[i - 1];
            cc.mode = CueMode::ListDefault;
            *dropped = dropInclude && cc.includes.removeAll(prevId) > 0;
            *releases = 0;
            for (const ResolvedLayer &pl : now.at(i - 1).passOn)
            {
                if (lit.contains(pl.target.signature()))
                    continue;
                if (prev.reusable != invalidId())
                    return false;               // its layers are a reusable's
                Layer *own = nullptr;
                for (Layer &l : prev.layers)
                    if (l.target == pl.target && l.parent == invalidId())
                        own = &l;
                if (own == nullptr)
                {
                    Layer fresh;
                    fresh.id = next++;
                    fresh.target = pl.target;
                    prev.layers << fresh;
                    own = &prev.layers.last();
                }
                if (!own->release)
                {
                    own->release = true;
                    ++*releases;
                    *said << tr("%1: %2 released (%3 doesn't light it)")
                             .arg(label(cand, prevId), layerName(*own), label(cand, cc.id));
                }
            }
            if (!same(cand))
                return false;
            work = cand;
            return true;
        };

        QStringList said;
        int releases = 0;
        bool dropped = false;
        const bool includesPrev = c.includes.contains(prevId);
        bool tracks = attempt(true, &said, &releases, &dropped);
        if (!tracks && includesPrev)
        {
            said.clear();
            tracks = attempt(false, &said, &releases, &dropped);
        }
        if (tracks)
        {
            ++plan.tracking;
            plan.releases += releases;
            QString line = tr("%1: tracks from %2").arg(label(work, work.cues.at(i).id), label(work, prevId));
            if (dropped)
            {
                ++plan.includesRemoved;
                line += tr(" (its include of %1 taken out)").arg(label(work, prevId));
            }
            plan.changes << line << said;
        }
        else
        {
            ++plan.blocks;
            plan.changes << tr("%1: stays Block (tracking would change what it plays)")
                            .arg(label(work, work.cues.at(i).id));
        }
    }

    // 2. In each cue that starts from the one before: a value its own layer
    //    sets to what tracks in anyway is taken out; a layer left with
    //    nothing of its own (and no Release, no name of its own to keep) is
    //    taken out too -- each only where every cue still plays the same.
    for (int i = 1; i < work.cues.count(); ++i)
    {
        if (work.cues.at(i).reusable != invalidId())
            continue;
        const CueMode mode = effectiveMode(work, work.cues.at(i));
        if (mode != CueMode::Track && mode != CueMode::CueOnly)
            continue;
        const quint32 cueId = work.cues.at(i).id;
        for (int k = 0; k < work.cues.at(i).layers.count(); ++k)
        {
            const Layer l = work.cues.at(i).layers.at(k);
            if (l.reusable != invalidId() || l.target.isOpaque() || l.parent != invalidId()
                || !l.continues.isEmpty() || !l.continuesAlso.isEmpty())
                continue;
            bool hasChild = false;
            for (const Layer &o : work.cues.at(i).layers)
                hasChild |= o.parent == l.id;
            if (hasChild)
                continue;
            QStringList gone;
            for (const Look &look : l.looks)
            {
                CueList cand = work;
                Layer *cl = cand.cues[i].layer(l.id);
                Look empty;
                empty.key = look.key;
                cl->setLook(empty);
                if (!same(cand))
                    continue;
                work = cand;
                ++plan.looksRemoved;
                gone << keyLabel(look.key);
            }
            const Layer *after = work.cues.at(i).layer(l.id);
            bool removed = false;
            if (after != nullptr && after->looks.isEmpty() && !after->release)
            {
                CueList cand = work;
                for (int m = 0; m < cand.cues.at(i).layers.count(); ++m)
                    if (cand.cues.at(i).layers.at(m).id == l.id)
                    {
                        cand.cues[i].layers.removeAt(m);
                        break;
                    }
                if (same(cand))
                {
                    work = cand;
                    removed = true;
                    ++plan.layersRemoved;
                }
            }
            if (removed)
                plan.changes << tr("%1: %2 — all of it tracks in, layer taken out").arg(label(work, cueId), layerName(l));
            else if (!gone.isEmpty())
                plan.changes << tr("%1: %2 %3 — tracks in, taken out")
                                .arg(label(work, cueId), layerName(l), gone.join(QStringLiteral(", ")));
            if (removed)
                --k;
        }
    }

    // The base of it all: nothing changed on stage, checked once more as a
    // whole.
    plan.ok = same(work);
    if (!plan.ok)
    {
        plan.why = tr("The converted list would play differently.");
        return plan;
    }
    // In list order, each cue's lines together (a release is said with
    // the cue it is set in).
    {
        QList<QPair<int, QString>> keyed;
        for (int k = 0; k < plan.changes.count(); ++k)
        {
            int at = int(work.cues.count());
            for (int i = 0; i < work.cues.count(); ++i)
                if (plan.changes.at(k).startsWith(cueLabel(work, i) + QLatin1Char(':')))
                    at = i;
            keyed << qMakePair(at, plan.changes.at(k));
        }
        std::stable_sort(keyed.begin(), keyed.end(), [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
            return a.first < b.first;
        });
        plan.changes.clear();
        for (const auto &k : keyed)
            plan.changes << k.second;
    }
    plan.list = work;
    if (plan.tracking == 0 && plan.looksRemoved == 0 && plan.layersRemoved == 0 && src->mode == ListMode::Tracking)
    {
        plan.ok = false;
        plan.why = tr("Nothing to convert: %1 tracks already.").arg(src->name);
    }
    return plan;
}

bool ShowTreeController::convertToTracking(quint32 listId, ConvertPlan *planOut)
{
    const ConvertPlan plan = planConvertToTracking(listId);
    if (planOut != nullptr)
        *planOut = plan;
    if (!plan.ok)
        return false;
    // Ids the plan gave new layers (a release on a layer only carried in).
    for (const Cue &c : plan.list.cues)
        for (const Layer &l : c.layers)
            m_nextId = qMax(m_nextId, l.id + 1);
    return edit(tr("Convert %1 to tracking").arg(plan.list.name), [&](QList<Production> &ps) {
        for (Production &p : ps)
            for (CueList &l : p.lists)
                if (l.id == listId)
                {
                    const quint32 chaser = l.chaser;
                    QHash<quint32, quint32> colls;
                    for (const Cue &c : l.cues)
                        colls.insert(c.id, c.collection);
                    l = plan.list;
                    l.chaser = chaser;
                    for (Cue &c : l.cues)
                        c.collection = colls.value(c.id, c.collection);
                    return true;
                }
        return false;
    });
}

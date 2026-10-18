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
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "cueoutput.h"
#include "fixture.h"
#include "scene.h"
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

QHash<quint64, uchar> ShowTreeController::staticCueOutput(const CueList &list, const QList<ResolvedCue> &resolved,
                                                          int index, QStringList *effects) const
{
    QHash<quint64, uchar> out;
    if (index < 0 || index >= resolved.count())
        return out;
    const ResolvedCue &rc = resolved.at(index);
    QHash<int, ScenePlan> byLayer;
    for (const ScenePlan &p : planCue(list, resolved, index))
        byLayer.insert(p.layer, p);

    auto isHtp = [this](quint32 fid, quint32 ch) {
        Fixture *fx = m_doc->fixture(fid);
        if (fx == nullptr)
            return false;
        if (fx->forcedHTPChannels().contains(int(ch)))
            return true;
        if (fx->forcedLTPChannels().contains(int(ch)))
            return false;
        const QLCChannel *c = fx->channel(ch);
        return c != nullptr && c->group() == QLCChannel::Intensity;
    };
    auto merge = [&](const QHash<quint64, uchar> &one) {
        for (auto it = one.constBegin(); it != one.constEnd(); ++it)
        {
            const quint32 fid = quint32(it.key() >> 32), ch = quint32(it.key() & 0xFFFFFFFF);
            if (out.contains(it.key()) && isHtp(fid, ch))
                out[it.key()] = qMax(out.value(it.key()), it.value());
            else
                out[it.key()] = it.value();             // LTP: the later started
        }
    };

    // In layer order, as the cue's Collection runs them.
    for (int k = 0; k < rc.layers.count(); ++k)
    {
        const ResolvedLayer &rl = rc.layers.at(k);
        if (rl.target.isOpaque())
        {
            const QHash<quint32, QHash<quint32, uchar>> o = CueOutput::compute(m_doc, rl.target.function);
            QHash<quint64, uchar> one;
            for (auto f = o.constBegin(); f != o.constEnd(); ++f)
                for (auto c = f.value().constBegin(); c != f.value().constEnd(); ++c)
                    one.insert((quint64(f.key()) << 32) | c.key(), c.value());
            merge(one);
            continue;
        }
        if (!byLayer.contains(k))
            continue;
        const ScenePlan &plan = byLayer.value(k);
        // A scene as the bake would make it, never added to the show.
        Scene scene(m_doc);
        for (quint32 g : plan.target.groups)
            scene.addFixtureGroup(g);
        for (quint32 f : plan.target.fixtures)
            scene.addFixture(f);
        for (quint32 p : plan.palettes)
            scene.addPalette(p);
        for (auto it = plan.excluded.constBegin(); it != plan.excluded.constEnd(); ++it)
            scene.setExcludedFixtures(it.key(), it.value());
        for (quint32 p : plan.palettes)
            if (plan.spanGroups.contains(p) || plan.spanFixtures.contains(p))
            {
                Scene::LayoutSpan span;
                span.groups = plan.spanGroups.value(p);
                span.fixtures = plan.spanFixtures.value(p);
                scene.setLayoutSpan(p, span);
            }
        const QList<quint32> pals = scene.palettes();
        QHash<quint64, uchar> one;
        for (quint32 pid : pals)
        {
            QLCPalette *palette = m_doc->palette(pid);
            if (palette == nullptr)
                continue;
            const QList<quint32> left = plan.excluded.value(pid);
            if (palette->type() == QLCPalette::Effect)
            {
                if (effects != nullptr)
                {
                    QList<quint32> lit = targetFixtures(plan.target);
                    QStringList on;
                    for (quint32 f : lit)
                        if (!left.contains(f))
                            on << QString::number(f);
                    *effects << QStringLiteral("%1:%2:%3|%4").arg(pid).arg(on.join(QLatin1Char(',')),
                                                                     idsToString(plan.spanGroups.value(pid)),
                                                                     idsToString(plan.spanFixtures.value(pid)));
                }
                continue;
            }
            if (QLCPalette::isEffectScoped(m_doc, pals, pid))
                continue;
            const int cOff = QLCPalette::colorSetOffset(m_doc, pals, pid);
            const QList<SceneValue> vals = palette->valuesFromFixtureGroups(m_doc, scene.fixtureGroups(), &scene, cOff)
                                           + palette->valuesFromFixtures(m_doc, scene.fixtures(), &scene, cOff);
            for (const SceneValue &sv : vals)
                if (!left.contains(sv.fxi))
                    one.insert((quint64(sv.fxi) << 32) | sv.channel, sv.value);
        }
        const int lookDim = QLCPalette::lookDimmerValue(m_doc, pals);
        for (auto it = one.begin(); it != one.end(); ++it)
            it.value() = QLCPalette::dimScaleWithoutDimmer(m_doc->fixture(quint32(it.key() >> 32)),
                                                           quint32(it.key() & 0xFFFFFFFF), it.value(), lookDim);
        merge(one);
    }
    if (effects != nullptr)
        effects->sort();
    return out;
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
    if (src->isSubCues() || subCueListsOf(listId) > 0)
    {
        plan.why = src->isSubCues() ? tr("Sub-cues always track from the one before.")
                                    : tr("It has sub-cues: Convert reads imported lists, which have none.");
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
    Plays playsBefore = plays(*src);
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

    // 0. One rule for overlaps (open 13): a cue that mixes (an import keeps
    //    the chaser's highest-wins) is put on the tree's rule -- the most
    //    specific source wins, then the deeper layer, then the lower -- where
    //    its output is the same channel for channel (static values and
    //    where each effect plays). Else it stays Mix, flagged, and said.
    for (int i = 0; i < work.cues.count(); ++i)
    {
        const Cue c = work.cues.at(i);
        if (!c.mixOverlap || c.reusable != invalidId())
            continue;
        CueList cand = work;
        cand.cues[i].mixOverlap = false;
        QStringList e0, e1;
        const QHash<quint64, uchar> o0 = staticCueOutput(work, resolve(work, aimTest), i, &e0);
        const QHash<quint64, uchar> o1 = staticCueOutput(cand, resolve(cand, aimTest), i, &e1);
        if (o0 == o1 && e0 == e1 && stages(cand) == stageBefore)
        {
            work = cand;
            playsBefore.insert(c.id, plays(cand).value(c.id));
            ++plan.mixCleared;
            plan.changes << tr("%1: overlapping layers follow the one rule now (it plays the same)")
                            .arg(label(work, c.id));
            continue;
        }
        int n = 0;
        QSet<quint64> keys;
        for (auto it = o0.constBegin(); it != o0.constEnd(); ++it)
            keys.insert(it.key());
        for (auto it = o1.constBegin(); it != o1.constEnd(); ++it)
            keys.insert(it.key());
        QSet<quint32> fixtures;
        for (quint64 k : keys)
            if (o0.value(k) != o1.value(k))
            {
                ++n;
                fixtures.insert(quint32(k >> 32));
            }
        QString line = n > 0
                ? tr("%1: stays Mix — the one rule would change %n channel(s)", "", n).arg(label(work, c.id))
                  + tr(" on %n fixture(s)", "", int(fixtures.count()))
                : tr("%1: stays Mix — the one rule would change where an effect plays").arg(label(work, c.id));
        plan.mixKept << line;
        plan.changes << line;
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
    struct Stripped { QStringList lines; int looks = 0; int layers = 0; };
    auto strip = [&](CueList &w, int i) {
        Stripped st;
        if (w.cues.at(i).reusable != invalidId())
            return st;
        const CueMode mode = effectiveMode(w, w.cues.at(i));
        if (mode != CueMode::Track && mode != CueMode::CueOnly)
            return st;
        const quint32 cueId = w.cues.at(i).id;
        for (int k = 0; k < w.cues.at(i).layers.count(); ++k)
        {
            const Layer l = w.cues.at(i).layers.at(k);
            if (l.reusable != invalidId() || l.target.isOpaque() || l.parent != invalidId()
                || !l.continues.isEmpty() || !l.continuesAlso.isEmpty())
                continue;
            bool hasChild = false;
            for (const Layer &o : w.cues.at(i).layers)
                hasChild |= o.parent == l.id;
            if (hasChild)
                continue;
            QStringList gone;
            for (const Look &look : l.looks)
            {
                CueList cand = w;
                Layer *cl = cand.cues[i].layer(l.id);
                Look empty;
                empty.key = look.key;
                cl->setLook(empty);
                if (!same(cand))
                    continue;
                w = cand;
                ++st.looks;
                gone << keyLabel(look.key);
            }
            const Layer *after = w.cues.at(i).layer(l.id);
            bool removed = false;
            if (after != nullptr && after->looks.isEmpty() && !after->release)
            {
                CueList cand = w;
                for (int m = 0; m < cand.cues.at(i).layers.count(); ++m)
                    if (cand.cues.at(i).layers.at(m).id == l.id)
                    {
                        cand.cues[i].layers.removeAt(m);
                        break;
                    }
                if (same(cand))
                {
                    w = cand;
                    removed = true;
                    ++st.layers;
                }
            }
            if (removed)
                st.lines << tr("%1: %2 — all of it tracks in, layer taken out").arg(label(w, cueId), layerName(l));
            else if (!gone.isEmpty())
                st.lines << tr("%1: %2 %3 — tracks in, taken out")
                            .arg(label(w, cueId), layerName(l), gone.join(QStringLiteral(", ")));
            if (removed)
                --k;
        }
        return st;
    };
    for (int i = 1; i < work.cues.count(); ++i)
    {
        const Stripped st = strip(work, i);
        plan.looksRemoved += st.looks;
        plan.layersRemoved += st.layers;
        plan.changes << st.lines;
    }

    // 3. Cue-only (open 18): a cue whose changes the next cue undoes --
    //    0.0.6 adds FlashWashes, 0.0.7 goes back to 0.0.5 -- is made
    //    Cue-only where that takes repetition out: the next cue then starts
    //    from what this one started from (a Block one may track), releases
    //    made for it go, and what then repeats is taken out. Kept only when
    //    every cue plays the same and the list holds less.
    auto weight = [](const CueList &l) {
        int n = 0;
        for (const Cue &c : l.cues)
        {
            n += int(c.includes.count()) + int(c.looks.count());
            for (const Layer &y : c.layers)
                n += 1 + int(y.looks.count()) + (y.release ? 1 : 0);
        }
        return n;
    };
    for (int i = 1; i + 1 < work.cues.count(); ++i)
    {
        const Cue &c = work.cues.at(i);
        if (c.reusable != invalidId() || work.cues.at(i + 1).reusable != invalidId()
            || effectiveMode(work, c) != CueMode::Track)
            continue;
        const bool nextBlock = effectiveMode(work, work.cues.at(i + 1)) == CueMode::Block;
        CueList best;
        Stripped bestSt;
        int bestReleases = 0;
        QStringList bestNames;
        bool found = false;
        for (int variant = 0; variant < (nextBlock ? 2 : 1); ++variant)
        {
            CueList cand = work;
            cand.cues[i].mode = CueMode::CueOnly;
            if (variant == 1)
            {
                cand.cues[i + 1].mode = CueMode::ListDefault;
                cand.cues[i + 1].includes.removeAll(cand.cues.at(i - 1).id);
            }
            if (!same(cand))
                continue;
            // Releases in it: the next cue no longer starts from it.
            int dropped = 0;
            QStringList names;
            for (int k = 0; k < cand.cues.at(i).layers.count(); ++k)
            {
                if (!cand.cues.at(i).layers.at(k).release)
                    continue;
                CueList r = cand;
                r.cues[i].layers[k].release = false;
                if (same(r))
                {
                    cand = r;
                    ++dropped;
                    names << tr("%1: %2 released").arg(label(cand, cand.cues.at(i).id),
                                                        layerName(cand.cues.at(i).layers.at(k)));
                }
            }
            const Stripped st = strip(cand, i + 1);
            if (weight(cand) < weight(found ? best : work))
            {
                best = cand;
                bestSt = st;
                bestReleases = dropped;
                bestNames = names;
                found = true;
            }
        }
        if (!found)
            continue;
        // What step 1 said about what is now undone.
        for (int k = int(plan.changes.count()) - 1; k >= 0; --k)
        {
            const QString &ln = plan.changes.at(k);
            bool gone = false;
            for (const QString &n : bestNames)
                gone |= ln.startsWith(n + QLatin1Char(' '));
            if (nextBlock && ln.startsWith(label(work, work.cues.at(i + 1).id) + tr(": stays Block")))
                gone = gone || effectiveMode(best, best.cues.at(i + 1)) != CueMode::Block;
            if (gone)
                plan.changes.removeAt(k);
        }
        const bool nowTracks = nextBlock && effectiveMode(best, best.cues.at(i + 1)) != CueMode::Block;
        work = best;
        ++plan.cueOnly;
        plan.looksRemoved += bestSt.looks;
        plan.layersRemoved += bestSt.layers;
        plan.releases -= qMin(plan.releases, bestReleases);
        QString line = tr("%1: Cue-only (%2 starts from what %1 started from)")
                       .arg(label(work, work.cues.at(i).id), label(work, work.cues.at(i + 1).id));
        if (bestReleases > 0)
            line += tr(", %n release(s) no longer needed", "", bestReleases);
        plan.changes << line;
        if (nowTracks)
        {
            ++plan.tracking;
            --plan.blocks;
            plan.changes << tr("%1: tracks from %2").arg(label(work, work.cues.at(i + 1).id),
                                                         label(work, work.cues.at(i - 1).id));
        }
        plan.changes << bestSt.lines;
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
    if (plan.tracking == 0 && plan.looksRemoved == 0 && plan.layersRemoved == 0 && plan.mixCleared == 0
        && plan.cueOnly == 0 && src->mode == ListMode::Tracking)
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

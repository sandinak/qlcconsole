/*
  Q Light Controller Plus
  showtreetrackops.cpp

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

/* Two explicit actions on a layer carried into a cue (the grey row):

   Bring Into This Cue -- the cue holds the layer with exactly the values it
   carried in (cue looks and inherited values as the grey row shows them),
   so every cue plays as before and the cues that tracked it now track from
   here.

   Start Here Instead -- the layer's start moves from the cue that sets it to
   this cue: that cue and the ones between it and this one no longer have the
   layer, this cue sets it with the carried values, the later cues track from
   here.

   Both are checked, not trusted: the edited list is resolved again and
   every cue that should play as before must (stageOf). Whatever can't be
   kept the same is refused with the reason. */

#include <QSet>

#include "showtreecontroller.h"

using namespace ShowTree;

namespace
{

typedef ShowTreeController::TrackOp TrackOp;

TrackOp refused(const QString &why)
{
    TrackOp op;
    op.why = why;
    return op;
}

struct Ctx
{
    const CueList *list = nullptr;
    int at = -1;
    const Cue *cue = nullptr;
    QList<ResolvedCue> res;
    const ResolvedLayer *rl = nullptr;
};

/** Take the layer on @a target out of cues @a range and put @a op.layer into
 *  cue @a at of @a list. */
void applyOp(CueList &list, int at, const TrackOp &op, const Target &target)
{
    for (int r : op.range)
    {
        QList<Layer> &ls = list.cues[r].layers;
        for (int i = ls.count() - 1; i >= 0; --i)
            if (ls.at(i).target == target)
                ls.removeAt(i);
    }
    QList<Layer> &ls = list.cues[at].layers;
    if (op.front)
        ls.prepend(op.layer);
    else
        ls.append(op.layer);
}

/** Whether every cue not in @a except plays the same in both. */
bool samePlay(const QList<ResolvedCue> &a, const QList<ResolvedCue> &b, const QSet<quint32> &except)
{
    if (a.count() != b.count())
        return false;
    for (int i = 0; i < a.count(); ++i)
    {
        if (except.contains(a.at(i).cueId))
            continue;
        if (stageOf(a.at(i)) != stageOf(b.at(i)))
            return false;
    }
    return true;
}

} // namespace

/* What both actions need: the cue plays the layer carried in, not its own,
   and nothing that can't be copied faithfully into a layer. */
static bool commonChecks(const ShowTreeController &st, quint32 cueId, const Target &t, Ctx *c, QString *why)
{
    c->list = st.listOfCue(cueId);
    if (c->list == nullptr || t.isEmpty())
    {
        *why = ShowTreeController::tr("No such layer.");
        return false;
    }
    if (c->list->looks)
    {
        *why = ShowTreeController::tr("A look plays only what it has: nothing is carried in.");
        return false;
    }
    if (c->list->isSubCues())
    {
        *why = ShowTreeController::tr("A sub-cue starts from its cue: change the layer in the cue.");
        return false;
    }
    c->at = c->list->indexOf(cueId);
    c->cue = c->list->cue(cueId);
    c->res = st.resolved(*c->list);
    if (c->at < 0 || c->at >= c->res.count())
    {
        *why = ShowTreeController::tr("No such cue.");
        return false;
    }
    c->rl = c->res.at(c->at).layer(t);
    const QString label = st.targetLabel(t);
    if (c->rl == nullptr)
    {
        *why = ShowTreeController::tr("%1 isn't on stage in this cue.").arg(label);
        return false;
    }
    if (c->rl->own)
    {
        *why = ShowTreeController::tr("%1 is this cue's own already.").arg(label);
        return false;
    }
    // One effect or aim across several layers (set at cue level) would split
    // into one each.
    const ResolvedCue &rc = c->res.at(c->at);
    for (auto it = c->rl->values.constBegin(); it != c->rl->values.constEnd(); ++it)
    {
        if (it.value().look.none || it.value().span == invalidId())
            continue;
        for (const ResolvedLayer &other : rc.layers)
        {
            if (other.target == t)
                continue;
            const auto ov = other.values.constFind(it.key());
            if (ov != other.values.constEnd() && !ov.value().look.none && ov.value().span == it.value().span
                && ov.value().look == it.value().look)
            {
                *why = ShowTreeController::tr("Its %1 is one with %2's, set for the cue as a whole: on its own "
                                              "it would play as two.")
                           .arg(keyLabel(it.key()), st.targetLabel(other.target));
                return false;
            }
        }
    }
    // A child layer stays under its parent.
    if (c->rl->depth > 0 && !c->rl->parent.isEmpty())
    {
        bool found = false;
        for (const Layer &l : c->cue->layers)
            found |= l.target == c->rl->parent;
        if (!found)
        {
            *why = ShowTreeController::tr("It is a child layer of %1, which is carried in too: bring that in first.")
                       .arg(st.targetLabel(c->rl->parent));
            return false;
        }
    }
    return true;
}

/* The layer the cue will hold: the carried values, one look each. @a nones:
   also "off here" for what the cue's own looks would otherwise start to set
   on it. */
static Layer carriedLayer(const Ctx &c, bool nones)
{
    Layer l;
    l.name = c.rl->name;
    l.target = c.rl->target;
    for (auto it = c.rl->values.constBegin(); it != c.rl->values.constEnd(); ++it)
        l.setLook(it.value().look);
    if (c.rl->depth > 0)
        for (const Layer &o : c.cue->layers)
            if (o.target == c.rl->parent)
            {
                l.parent = o.id;
                break;
            }
    if (nones && !l.target.isOpaque())
    {
        auto off = [&](const QString &key) {
            if (c.rl->values.contains(key) || l.look(key) != nullptr)
                return;
            Look none;
            none.key = key;
            none.none = true;
            l.setLook(none);
        };
        bool formation = false;
        for (const Look &cl : c.cue->looks)
        {
            if (cl.key == KeyFormation)
                formation = true;
            off(cl.key);
        }
        if (formation)
            for (const QString &k : { KeySpread, KeyGrouping, KeyCross })
                off(k);
    }
    return l;
}

/* Try the ways the carried layer can sit among the cue's own until a cue
   that should play as before doesn't change; false: none holds. */
static bool settle(const ShowTreeController &st, const Ctx &c, const Target &t, const QSet<quint32> &except,
                   TrackOp *op)
{
    for (bool nones : { false, true })
        for (bool front : { true, false })
        {
            op->layer = carriedLayer(c, nones);
            op->front = front;
            CueList trial = *c.list;
            applyOp(trial, c.at, *op, t);
            const QList<ResolvedCue> after = resolve(trial, st.lineAimTest(), nullptr, st.formationRows());
            if (samePlay(c.res, after, except))
                return true;
        }
    return false;
}

TrackOp ShowTreeController::bringIntoPlan(quint32 cueId, const Target &target) const
{
    Ctx c;
    QString why;
    if (!commonChecks(*this, cueId, target, &c, &why))
        return refused(why);
    // A cue that mixes plays an included layer beside its own.
    if (c.cue->mixOverlap && c.rl->origin == Origin::Included)
        return refused(tr("This cue mixes what it includes: %1 would play twice. Turn mixing off first.")
                           .arg(targetLabel(target)));
    TrackOp op;
    if (!settle(*this, c, target, QSet<quint32>(), &op))
        return refused(tr("%1 can't be brought in without changing what a cue plays (its layer order or the "
                          "cue's own looks).").arg(targetLabel(target)));
    op.ok = true;
    return op;
}

TrackOp ShowTreeController::startHerePlan(quint32 cueId, const Target &target) const
{
    Ctx c;
    QString why;
    if (!commonChecks(*this, cueId, target, &c, &why))
        return refused(why);
    const CueList &list = *c.list;
    const QString label = targetLabel(target);
    const CueMode mode = effectiveMode(list, *c.cue);
    if (mode == CueMode::CueOnly)
        return refused(tr("This cue is Cue-only: what it sets doesn't pass on, so the cues after it would lose "
                          "%1.").arg(label));
    if (mode == CueMode::Block)
        return refused(tr("%1 isn't tracked into this cue: it comes from an include.").arg(label));
    for (auto it = c.rl->values.constBegin(); it != c.rl->values.constEnd(); ++it)
        if (it.value().origin != Origin::Tracked)
            return refused(tr("Part of %1 comes from an include or the base cue, not from an earlier cue.")
                               .arg(label));
    if (c.rl->origin != Origin::Tracked)
        return refused(tr("%1 is here from an include or the base cue, not tracked.").arg(label));

    // Walk back to the cue that sets it: through the cues it tracks through.
    QList<int> range;
    int from = -1;
    int k = c.at;
    for (;;)
    {
        const int p = playedBefore(list, k);
        if (p < 0)
            return refused(tr("No cue before this one sets %1.").arg(label));
        for (int s = p + 1; s < k; ++s)         // skipped cues between play it too
            range << s;
        const Cue &pc = list.cues.at(p);
        const ResolvedLayer *rp = c.res.at(p).layer(target);
        if (rp == nullptr)
            return refused(tr("%1 isn't on stage in %2.").arg(label, cueLabel(list, p)));
        for (quint32 inc : pc.includes)
        {
            const int j = list.indexOf(inc);
            if (j >= 0 && c.res.at(j).layer(target) != nullptr)
                return refused(tr("%1 includes %2, which plays %3 too: it feeds an include.")
                                   .arg(cueLabel(list, p), cueLabel(list, j), label));
        }
        for (const Layer &l : pc.layers)
        {
            if (l.continues == target || l.continuesAlso.contains(target))
                return refused(tr("A layer in %1 carries on from %2.").arg(cueLabel(list, p), label));
        }
        range << p;
        const CueMode pm = effectiveMode(list, pc);
        if (pm == CueMode::CueOnly)
        {
            // Its changes don't pass on: what reaches this cue comes from before it.
            k = p;
            continue;
        }
        if (!rp->own)
        {
            if (pm == CueMode::Block)
                return refused(tr("%1 isn't tracked into %2.").arg(label, cueLabel(list, p)));
            if (rp->origin != Origin::Tracked)
                return refused(tr("%1 is here from an include or the base cue in %2.")
                                   .arg(label, cueLabel(list, p)));
            k = p;
            continue;
        }
        if (!rp->tookOver.isEmpty())
        {
            if (!(rp->tookOver == target))
                return refused(tr("In %1 the layer was %2.").arg(cueLabel(list, p), targetLabel(rp->tookOver)));
            if (pm == CueMode::Block)
                return refused(tr("%1 comes into %2 from an include.").arg(label, cueLabel(list, p)));
            k = p;
            continue;
        }
        from = p;
        break;
    }
    std::sort(range.begin(), range.end());
    range.erase(std::unique(range.begin(), range.end()), range.end());

    QSet<quint32> ids;
    for (int r : range)
        ids.insert(list.cues.at(r).id);
    for (const Cue &m : list.cues)
        for (quint32 inc : m.includes)
            if (ids.contains(inc))
                return refused(tr("%1 includes %2, which would lose %3.")
                                   .arg(cueLabel(list, list.indexOf(m.id)), cueLabel(list, list.indexOf(inc)), label));
    for (int r : range)
        for (const Layer &l : list.cues.at(r).layers)
        {
            if (!(l.target == target))
                continue;
            for (const Layer &child : list.cues.at(r).layers)
                if (child.parent == l.id)
                    return refused(tr("%1 has a child layer (%2) in %3: move that first.")
                                       .arg(label, targetLabel(child.target), cueLabel(list, r)));
        }
    if (ids.contains(list.baseCue))
        for (const Cue &m : list.cues)
            for (const Layer &l : m.layers)
                if (l.release && l.target == target)
                    return refused(tr("%1 is the base cue, and %2 releases %3 back to it.")
                                       .arg(cueLabel(list, list.indexOf(list.baseCue)),
                                            cueLabel(list, list.indexOf(m.id)), label));

    TrackOp op;
    op.range = range;
    if (!settle(*this, c, target, ids, &op))
        return refused(tr("%1 can't start here without changing what another cue plays (its layer order or the "
                          "cue's own looks).").arg(label));
    op.from = list.cues.at(from).id;
    for (int r : range)
        if (c.res.at(r).layer(target) != nullptr)
            op.lose << list.cues.at(r).id;
    op.ok = true;
    return op;
}

bool ShowTreeController::bringIntoCue(quint32 cueId, const Target &target, QString *why)
{
    TrackOp op = bringIntoPlan(cueId, target);
    if (!op.ok)
    {
        if (why != nullptr)
            *why = op.why;
        return false;
    }
    op.layer.id = newId();
    return edit(tr("Bring %1 into this cue").arg(targetLabel(target)), [&](QList<Production> &ps) {
        for (Production &p : ps)
            for (CueList &l : p.lists)
            {
                const int at = l.indexOf(cueId);
                if (at < 0)
                    continue;
                applyOp(l, at, op, target);
                return true;
            }
        return false;
    });
}

bool ShowTreeController::startLayerHere(quint32 cueId, const Target &target, QString *why, QList<quint32> *lost)
{
    TrackOp op = startHerePlan(cueId, target);
    if (!op.ok)
    {
        if (why != nullptr)
            *why = op.why;
        return false;
    }
    op.layer.id = newId();
    const bool ok = edit(tr("Start %1 here").arg(targetLabel(target)), [&](QList<Production> &ps) {
        for (Production &p : ps)
            for (CueList &l : p.lists)
            {
                const int at = l.indexOf(cueId);
                if (at < 0)
                    continue;
                applyOp(l, at, op, target);
                return true;
            }
        return false;
    });
    if (ok && lost != nullptr)
        *lost = op.lose;
    return ok;
}

/****************************************************************************
 * Release / Untrack (Q222)
 ****************************************************************************/

/* Release ends a layer after the cue it is set on; Untrack ends a layer
   carried into a cue before it: the Release goes on the cue it is carried
   from, so that cue still plays it and this one starts without it. */

ShowTreeController::UntrackPlan ShowTreeController::untrackPlan(quint32 cueId, const Target &target) const
{
    UntrackPlan plan;
    const CueList *list = listOfCue(cueId);
    const QString label = targetLabel(target);
    auto no = [&plan](const QString &why) {
        plan.why = why;
        return plan;
    };
    if (list == nullptr || target.isEmpty())
        return no(tr("No such layer."));
    if (list->looks)
        return no(tr("A hit plays only what it has: nothing tracks into it."));
    const int at = list->indexOf(cueId);
    const QList<ResolvedCue> res = resolved(*list);
    const ResolvedLayer *rl = at >= 0 && at < res.count() ? res.at(at).layer(target) : nullptr;
    if (rl == nullptr)
        return no(tr("%1 isn't on stage in this cue.").arg(label));
    if (rl->own)
        return no(tr("This cue sets %1: Release After This Cue ends it after this cue.").arg(label));
    if (rl->origin == Origin::Included)
        return no(tr("%1 comes from an include, not tracked: take the include out instead.").arg(label));
    if (rl->origin == Origin::Base)
        return no(tr("%1 is released already: here it is back at the base cue's value.").arg(label));
    const int before = playedBefore(*list, at);
    if (before < 0)
    {
        if (list->isSubCues())
        {
            const CueList *up = listOfCue(list->subOf);
            const QString cue = up == nullptr ? QString() : cueLabel(*up, up->indexOf(list->subOf));
            return no(tr("%1 comes in with %2 (a sub-cue starts from its cue's look): release it in %2 or "
                         "before.").arg(label, cue));
        }
        return no(tr("No cue before this one carries %1.").arg(label));
    }
    const Cue &pc = list->cues.at(before);
    const QString carrier = cueLabel(*list, before);
    if (res.at(before).layer(target) == nullptr)
        return no(tr("%1 isn't on stage in %2.").arg(label, carrier));
    if (effectiveMode(*list, pc) == CueMode::CueOnly)
        return no(tr("%1 is Cue-only: its changes don't pass on, so a Release there can't end %2. Release it "
                     "in the cue before %1, or make %1 track.").arg(carrier, label));
    if (pc.reusable != invalidId())
        return no(tr("%1 is a use of a reusable cue: a Release there would change every use. Unlink it "
                     "first.").arg(carrier));
    plan.ok = true;
    plan.carrier = pc.id;
    return plan;
}

bool ShowTreeController::untrackLayers(const QList<QPair<quint32, Target>> &layers, QString *why)
{
    // Planned against the tree as it is; a layer untracked from a cue that
    // another of them already untracks it from is left out (one Release).
    struct Step
    {
        quint32 cue;
        Target target;
        Layer layer;            // a fresh layer when the cue only carries it in
        bool fresh = false;
        bool front = false;
    };
    QList<Step> steps;
    QString first;
    for (const QPair<quint32, Target> &ref : layers)
    {
        const UntrackPlan plan = untrackPlan(ref.first, ref.second);
        if (!plan.ok)
        {
            if (first.isEmpty())
                first = plan.why;
            continue;
        }
        bool covered = false;
        for (const QPair<quint32, Target> &other : layers)
            covered |= other.first == plan.carrier && other.second == ref.second;
        for (const Step &s : steps)
            covered |= s.cue == plan.carrier && s.target == ref.second;
        if (covered)
            continue;
        Step s;
        s.cue = plan.carrier;
        s.target = ref.second;
        const CueList *list = listOfCue(plan.carrier);
        const int at = list->indexOf(plan.carrier);
        const QList<ResolvedCue> res = resolved(*list);
        const ResolvedLayer *rl = res.at(at).layer(ref.second);
        if (!rl->own)
        {
            // The carrier only carries it in: a layer of its own there, with
            // Release, that plays as the carried one did -- empty (it takes
            // the carried values), at the end or the front, else holding the
            // carried values as looks; the first that leaves the cue as it was.
            s.fresh = true;
            const Stage was = stageOf(res.at(at));
            QList<QPair<Layer, bool>> tries;
            Layer empty;
            empty.target = ref.second;
            empty.name = rl->name;
            empty.release = true;
            tries << qMakePair(empty, false) << qMakePair(empty, true);
            Layer held = empty;
            for (auto it = rl->values.constBegin(); it != rl->values.constEnd(); ++it)
                held.setLook(it.value().look);
            tries << qMakePair(held, false) << qMakePair(held, true);
            s.layer = empty;
            for (const QPair<Layer, bool> &t : tries)
            {
                CueList trial = *list;
                if (t.second)
                    trial.cues[at].layers.prepend(t.first);
                else
                    trial.cues[at].layers.append(t.first);
                const QList<ResolvedCue> after = resolved(trial);
                if (at < after.count() && stageOf(after.at(at)) == was)
                {
                    s.layer = t.first;
                    s.front = t.second;
                    break;
                }
            }
        }
        steps << s;
    }
    if (steps.isEmpty())
    {
        if (why != nullptr)
            *why = first.isEmpty() ? tr("Nothing to untrack.") : first;
        return false;
    }
    for (Step &s : steps)
        if (s.fresh)
            s.layer.id = newId();
    const QString label = steps.count() == 1 ? tr("Untrack %1").arg(targetLabel(steps.first().target))
                                             : tr("Untrack %1 layers").arg(steps.count());
    return edit(label, [&](QList<Production> &ps) {
        bool any = false;
        for (const Step &s : steps)
            for (Production &p : ps)
                for (CueList &l : p.lists)
                {
                    Cue *c = l.cue(s.cue);
                    if (c == nullptr)
                        continue;
                    if (s.fresh)
                    {
                        if (s.front)
                            c->layers.prepend(s.layer);
                        else
                            c->layers.append(s.layer);
                        any = true;
                        continue;
                    }
                    for (Layer &have : c->layers)
                        if (have.target == s.target && !have.release)
                        {
                            have.release = true;
                            any = true;
                        }
                }
        return any;
    });
}

ShowTreeController::TrackSpan ShowTreeController::trackSpan(quint32 cueId, const Target &target,
                                                            const QList<ResolvedCue> &given) const
{
    TrackSpan span;
    const CueList *list = listOfCue(cueId);
    if (list == nullptr || list->looks)
        return span;
    const QList<ResolvedCue> res = given.isEmpty() ? resolved(*list) : given;
    const int at = list->indexOf(cueId);
    if (at < 0 || at >= res.count())
        return span;
    const ResolvedLayer *rl = res.at(at).layer(target);
    if (rl == nullptr || rl->own || rl->origin != Origin::Tracked)
        return span;
    span.setIn = rl->fromCue;

    // Cues since the one that set it: this one, and the played cues back
    // to it; in a sub-cue, back through its cue to the main cue that set it.
    auto countBack = [](const CueList &l, int from, int to) {
        // played cues in (from, to], @a to counted even when skipped
        int n = 1;
        for (int k = playedBefore(l, to); k > from; k = playedBefore(l, k))
            ++n;
        return n;
    };
    const int from = list->indexOf(rl->fromCue);
    if (from >= 0 && from < at)
        span.cues = countBack(*list, from, at);
    else if (list->isSubCues())
    {
        span.cues = countBack(*list, -1, at);
        if (rl->fromCue != list->subOf)
            if (const CueList *up = listOfCue(list->subOf))
            {
                const int upFrom = up->indexOf(rl->fromCue), upAt = up->indexOf(list->subOf);
                if (upFrom >= 0 && upFrom < upAt)
                    span.cues += countBack(*up, upFrom, upAt);
            }
    }
    if (span.cues == 0)
    {
        span.setIn = invalidId();
        return span;
    }

    // Where a Release set ahead ends it.
    for (int k = at + 1; k < res.count(); ++k)
    {
        const ResolvedLayer *next = res.at(k).layer(target);
        if (next == nullptr)
            break;
        if (!next->own)
            continue;
        if (next->release)
            span.endsAfter = list->cues.at(k).id;
        break;
    }
    return span;
}

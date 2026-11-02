/*
  Q Light Controller Plus
  showtreesubcueconvert.cpp

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

/* Sub-cue <-> inline cue (0.4): Make Inline Cue turns sub-cues 4.2, 4.3 into
   cues of the list after cue 4, and Make Sub-cue turns cue 4.5 into the last
   sub-cue of cue 4. What is on stage at every step stays the same:

   - the edit is made on a copy of the tree, every cue of the production is
     resolved before and after, and each cue (by id) must put the same
     values on the same units, as the same cue-wide effects (Stage plus
     spans). Where the two models differ -- a sub-cue starts from its cue's
     whole look and passes nothing on, an inline cue starts from what the
     cue before passes on, a Block cue from nothing -- the cue that would
     play differently gets what it started from carried in as looks of its
     own (carryBase()), and the result is checked again. When it still
     differs, nothing changes and the message says where;
   - ids are kept, so calling-script anchors, hits under the cue, show
     control references and skip flags stay with it; the number too when it
     is free in the new place.

   It edits the model only: the runtime plays what the bake makes of it. */

#include <algorithm>
#include <QHash>
#include <QSet>

#include "showtreecontroller.h"
#include "fixturegroup.h"
#include "function.h"
#include "fixture.h"
#include "doc.h"

using namespace ShowTree;

namespace
{

/** What a cue puts on stage, strictly: its Stage (unit, attribute, value)
    and which values are ONE cue-wide effect or formation (their span). */
struct Print
{
    Stage stage;
    QStringList spans;
    bool operator==(const Print &o) const { return stage == o.stage && spans == o.spans; }
    bool operator!=(const Print &o) const { return !(*this == o); }
};

Print printOf(const ResolvedCue &rc)
{
    Print p;
    p.stage = stageOf(rc);
    for (const ResolvedLayer &l : rc.layers)
        for (auto it = l.values.constBegin(); it != l.values.constEnd(); ++it)
            if (!it.value().look.none && it.value().span != invalidId())
                p.spans << QStringLiteral("%1|%2|%3").arg(l.target.signature(), it.key()).arg(it.value().span);
    p.spans.sort();
    p.spans.removeDuplicates();
    return p;
}

/** Every cue of a production resolved (sub-cues from their cue's look), and
    the order they are checked in: a cue, then its sub-cues. */
struct Resolution
{
    QHash<quint32, ResolvedCue> cues;
    QList<quint32> order;
};

Resolution resolveProduction(const Production &p, const LineAimTest &aim, const FormationRows &forms)
{
    Resolution r;
    for (const CueList &l : p.lists)
    {
        if (l.looks || l.isSubCues())
            continue;
        const QList<ResolvedCue> out = resolve(l, aim, nullptr, forms);
        for (int i = 0; i < l.cues.count(); ++i)
        {
            const quint32 id = l.cues.at(i).id;
            r.cues.insert(id, out.at(i));
            r.order << id;
            for (const CueList &s : p.lists)
            {
                if (s.subOf != id)
                    continue;
                const QList<ResolvedLayer> start = out.at(i).layers;
                const QList<ResolvedCue> sout = resolve(s, aim, &start, forms);
                for (int k = 0; k < s.cues.count(); ++k)
                {
                    r.cues.insert(s.cues.at(k).id, sout.at(k));
                    r.order << s.cues.at(k).id;
                }
            }
        }
    }
    return r;
}

Cue *cueOf(Production &p, quint32 cueId, CueList **listOut = nullptr)
{
    for (CueList &l : p.lists)
        if (Cue *c = l.cue(cueId))
        {
            if (listOut != nullptr)
                *listOut = &l;
            return c;
        }
    return nullptr;
}

CueList *subsOf(Production &p, quint32 cueId)
{
    for (CueList &l : p.lists)
        if (l.subOf == cueId && cueId != invalidId())
            return &l;
    return nullptr;
}

/** What cue @a cueId of @a p has before its own layers and cue looks: what
    it starts from and what its includes bring. */
QList<ResolvedLayer> baseOf(const Production &p, quint32 cueId, const LineAimTest &aim, const FormationRows &forms)
{
    Production q = p;
    Cue *c = cueOf(q, cueId);
    if (c == nullptr)
        return QList<ResolvedLayer>();
    c->layers.clear();
    c->looks.clear();
    return resolveProduction(q, aim, forms).cues.value(cueId).layers;
}

const ResolvedLayer *layerIn(const QList<ResolvedLayer> &layers, const QString &sig)
{
    for (const ResolvedLayer &l : layers)
        if (l.target.signature() == sig)
            return &l;
    return nullptr;
}

/** A value as the stage sees it: none for "Off here" or nothing. */
const ResolvedValue *valueIn(const ResolvedLayer *l, const QString &key)
{
    if (l == nullptr)
        return nullptr;
    const auto it = l->values.constFind(key);
    return it == l->values.constEnd() || it.value().look.none ? nullptr : &it.value();
}

Look noneLook(const QString &key)
{
    Look l;
    l.key = key;
    l.none = true;
    return l;
}

/** What carryBase() added, so the tidy-up after can try taking it out. */
struct Carried
{
    QList<QPair<quint32, quint32>> layers;              // (cue, layer) made
    QList<QPair<quint32, QPair<quint32, QString>>> looks; // (cue, (layer, key)) set
};

/** Make @a cue play from @a now as it played from @a was (its bases,
    baseOf()): where they differ on a target and attribute that the cue
    doesn't set itself, the value it had goes on the cue's layer on that
    target (one made if needed, keeping what the cue's own looks would
    otherwise change on it). False when it can't: a cue-wide value (one
    effect across layers can't be one layer's), a layer that carries on
    from another, or a reusable's content. */
bool carryBase(Cue &cue, const QList<ResolvedLayer> &was, const QList<ResolvedLayer> &now,
               const std::function<quint32()> &newId, Carried *carried)
{
    QStringList sigs;
    for (const QList<ResolvedLayer> *side : { &was, &now })
        for (const ResolvedLayer &l : *side)
            if (!sigs.contains(l.target.signature()))
                sigs << l.target.signature();

    bool changed = false;
    for (const QString &sig : sigs)
    {
        const ResolvedLayer *o = layerIn(was, sig);
        const ResolvedLayer *n = layerIn(now, sig);
        QStringList keys;
        for (const ResolvedLayer *l : { o, n })
            if (l != nullptr)
                for (auto it = l->values.constBegin(); it != l->values.constEnd(); ++it)
                    if (!keys.contains(it.key()))
                        keys << it.key();
        QList<Look> need;
        for (const QString &key : keys)
        {
            const ResolvedValue *ov = valueIn(o, key), *nv = valueIn(n, key);
            if (ov == nullptr && nv == nullptr)
                continue;
            if (ov != nullptr && nv != nullptr && ov->look == nv->look && ov->span == nv->span
                && ov->formation == nv->formation)
                continue;
            if (ov != nullptr && ov->span != invalidId())
                return false;
            need << (ov != nullptr ? ov->look : noneLook(key));
        }
        if (need.isEmpty())
            continue;

        Layer *own = nullptr;
        for (Layer &ly : cue.layers)
        {
            if (ly.continues.signature() == sig && !ly.continues.isEmpty())
                return false;
            for (const Target &also : ly.continuesAlso)
                if (also.signature() == sig)
                    return false;
            if (own == nullptr && ly.target.signature() == sig)
                own = &ly;
        }
        if (cue.reusable != invalidId())
            return false;
        if (own != nullptr)
        {
            if (own->reusable != invalidId() || own->target.isOpaque())
                return false;
            const bool top = own->parent == invalidId();
            for (const Look &l : need)
            {
                if (own->look(l.key) != nullptr)
                    continue;               // the cue sets it: what came in never shows
                if (top && cue.look(l.key) != nullptr && !isLineParamKey(l.key) && l.key != KeyFormation)
                    continue;               // ... nor where the cue's own look reaches it
                own->setLook(l);
                carried->looks << qMakePair(cue.id, qMakePair(own->id, l.key));
                changed = true;
            }
            continue;
        }

        const ResolvedLayer *from = o != nullptr ? o : n;
        if (from->target.isOpaque())
            return false;
        Layer ly;
        ly.id = newId();
        ly.target = from->target;
        ly.name = from->name;
        for (const Look &l : need)
            ly.setLook(l);
        // Its own now, so the cue's looks would reach it: it keeps what it had.
        for (const Look &cl : cue.looks)
        {
            if (ly.look(cl.key) != nullptr)
                continue;
            const ResolvedValue *ov = valueIn(o, cl.key);
            if (ov != nullptr && ov->span != invalidId())
                return false;
            ly.setLook(ov != nullptr ? ov->look : noneLook(cl.key));
        }
        cue.layers << ly;
        carried->layers << qMakePair(cue.id, ly.id);
        changed = true;
    }
    return changed;
}

/** @a want for a cue placed after @a afterIndex of @a list when it is free
    there and in number order; else the number + Cue would give. */
QString placeNumber(const CueList &list, int afterIndex, const QString &want, Numbering numbering,
                    const QStringList &taken, bool isSubList, const QString &parentNumber)
{
    auto shown = [&](int i) { return cueNumberAt(list, i, parentNumber); };
    const QString full = isSubList ? parentNumber + QLatin1Char('.') + want : want;
    const QString prev = afterIndex >= 0 ? shown(afterIndex) : QString();
    const QString next = afterIndex + 1 < list.cues.count() ? shown(afterIndex + 1) : QString();
    bool used = taken.contains(full);
    for (int i = 0; i < list.cues.count() && !used; ++i)
        used = compareCueNumbers(shown(i), full) == 0;
    if (isCueNumber(want) && !used && (prev.isEmpty() || compareCueNumbers(prev, full) < 0)
        && (next.isEmpty() || compareCueNumbers(full, next) < 0))
        return want;
    return isSubList ? numberForInsert(list, afterIndex) : numberForInsertIn(list, numbering, afterIndex, taken);
}

bool infiniteHold(uint hold)
{
    return int(hold) < 0;
}

/** A sub-cue that only stands for its cue's own look (Make Sub-cue puts one
    first when the cue had none): nothing of its own, its cue's fades. */
bool isStandIn(const Cue &c, const Cue &parent, const Production &p)
{
    if (!c.layers.isEmpty() || !c.looks.isEmpty() || !c.name.isEmpty() || !c.note.isEmpty() || c.skip
        || !c.colour.isEmpty() || c.needsWork || c.noMoveInBlack || c.reusable != invalidId()
        || c.fadeIn != parent.fadeIn || c.fadeOut != parent.fadeOut || p.script.anchor(c.id) != nullptr)
        return false;
    for (const CueList &l : p.lists)
        if (l.looks)
            for (const Cue &h : l.cues)
                if (h.scope == HitScope::Cue && h.scopeOf == c.id)
                    return false;
    return true;
}

QString listJoin(const QStringList &items)
{
    if (items.count() < 2)
        return items.join(QString());
    return QStringList(items.mid(0, items.count() - 1)).join(QStringLiteral(", "))
           + QObject::tr(" and ") + items.last();
}

/** Take out of cues @a ids each layer that only turns things off (every look
    "Off here", no child under it) while @a same says the show still plays
    as before: what a carry once added and nothing needs now. */
void tidyOffLayers(QList<Production> &work, int pi, const QList<quint32> &ids, const std::function<bool()> &same)
{
    for (quint32 id : ids)
    {
        Cue *c = cueOf(work[pi], id);
        if (c == nullptr || c->reusable != invalidId())
            continue;
        for (int k = int(c->layers.count()) - 1; k >= 0; --k)
        {
            c = cueOf(work[pi], id);
            const Layer ly = c->layers.at(k);
            bool off = !ly.looks.isEmpty() && ly.reusable == invalidId();
            for (const Look &l : ly.looks)
                off = off && l.none;
            for (const Layer &other : c->layers)
                off = off && other.parent != ly.id;
            if (!off)
                continue;
            c->layers.removeAt(k);
            if (!same())
                cueOf(work[pi], id)->layers.insert(k, ly);
        }
    }
}

} // namespace

/****************************************************************************
 * Checking an edit: every cue plays as it did
 ****************************************************************************/

QString ShowTreeController::unitText(const QString &unit) const
{
    const QString kind = unit.section(QLatin1Char(':'), 0, 0);
    const quint32 id = unit.section(QLatin1Char(':'), 1).toUInt();
    if (kind == QLatin1String("g"))
        if (const FixtureGroup *g = m_doc->fixtureGroup(id))
            return g->name();
    if (kind == QLatin1String("f"))
        if (const Fixture *f = m_doc->fixture(id))
            return f->name();
    return unit;
}

/* The tree as @a work would be, carried until every cue of production @a pi
   plays as in m_productions (@a pair: a new cue -> the old one whose step it
   plays). @a carriedInto gets the cues that got looks carried in. False
   (and @a why) when it can't be made to. */
bool ShowTreeController::carryUntilSame(QList<Production> &work, int pi, const QHash<quint32, quint32> &pair,
                                        bool carry, QList<quint32> *carriedInto, QString *why)
{
    const LineAimTest aim = lineAimTest();
    const FormationRows forms = formationRows();
    const Production &old = m_productions.at(pi);
    const Resolution was = resolveProduction(old, aim, forms);
    QHash<quint32, Print> wasPrint;
    for (auto it = was.cues.constBegin(); it != was.cues.constEnd(); ++it)
        wasPrint.insert(it.key(), printOf(it.value()));

    // Each pass: the first cue (in play order) that differs.
    auto firstDiff = [&](quint32 *bad, QString *unit, QString *key) {
        const Resolution now = resolveProduction(work[pi], aim, forms);
        for (quint32 id : now.order)
        {
            const quint32 oid = pair.value(id, id);
            if (!wasPrint.contains(oid))
                continue;
            const Print np = printOf(now.cues.value(id));
            const Print &op = wasPrint[oid];
            if (np == op)
                continue;
            *bad = id;
            // Where: the first unit and attribute (else a cue-wide effect).
            QStringList units = op.stage.keys();
            for (const QString &u : np.stage.keys())
                if (!units.contains(u))
                    units << u;
            for (const QString &u : units)
            {
                if (op.stage.value(u) == np.stage.value(u))
                    continue;
                *unit = u;
                QStringList keys = op.stage.value(u).keys();
                keys << np.stage.value(u).keys();
                for (const QString &k : keys)
                    if (op.stage.value(u).value(k) != np.stage.value(u).value(k))
                    {
                        *key = k;
                        break;
                    }
                break;
            }
            return true;
        }
        return false;
    };

    Carried carried;
    QSet<quint32> fixed;
    for (int pass = 0; pass < 64; ++pass)
    {
        quint32 bad = invalidId();
        QString unit, key;
        if (!firstDiff(&bad, &unit, &key))
            return true;
        CueList *badList = nullptr;
        const Cue *badCue = cueOf(work[pi], bad, &badList);
        const QString number = badCue == nullptr ? QString() : cueNumber(*badList, badList->indexOf(bad));
        auto fail = [&]() {
            if (why == nullptr)
                return false;
            if (!unit.isEmpty() && !key.isEmpty())
                *why = tr("Not changed: %1 would play differently (%2 on %3).").arg(number, keyLabel(key),
                                                                                     unitText(unit));
            else
                *why = tr("Not changed: %1 would play differently (an effect for the whole cue would split "
                          "into one per layer).").arg(number);
            return false;
        };
        if (!carry || fixed.contains(bad) || !was.cues.contains(pair.value(bad, bad)))
            return fail();
        fixed.insert(bad);
        const QList<ResolvedLayer> wasBase = baseOf(old, pair.value(bad, bad), aim, forms);
        const QList<ResolvedLayer> nowBase = baseOf(work[pi], bad, aim, forms);
        Cue *c = cueOf(work[pi], bad);
        if (!carryBase(*c, wasBase, nowBase, [this]() { return newId(); }, &carried))
            return fail();
        if (carriedInto != nullptr && !carriedInto->contains(bad))
            *carriedInto << bad;
    }
    return false;
}

/****************************************************************************
 * Make Inline Cue
 ****************************************************************************/

bool ShowTreeController::makeInlineCues(const QList<quint32> &subCueIds, QString *message)
{
    auto no = [message](const QString &text) {
        if (message != nullptr)
            *message = text;
        return false;
    };
    if (m_doc->mode() != Doc::Design)
        return no(tr("Design mode only: switch to Design to make sub-cues inline cues."));
    if (subCueIds.isEmpty())
        return no(tr("Select a sub-cue first."));
    quint32 parent = invalidId();
    for (quint32 id : subCueIds)
    {
        quint32 up = invalidId();
        if (!isSubCue(id, &up))
            return no(tr("Only a sub-cue can become an inline cue: select sub-cues (4.1, 4.2…)."));
        if (parent != invalidId() && up != parent)
            return no(tr("Select sub-cues of one cue."));
        parent = up;
    }

    int pi = -1;
    for (int i = 0; i < m_productions.count() && pi < 0; ++i)
        for (const CueList &l : m_productions.at(i).lists)
            if (!l.isSubCues() && !l.looks && l.indexOf(parent) >= 0)
                pi = i;
    if (pi < 0)
        return no(tr("No such cue."));
    const Production &prod = m_productions.at(pi);
    const CueList *mainList = nullptr;
    for (const CueList &l : prod.lists)
        if (!l.isSubCues() && l.indexOf(parent) >= 0)
            mainList = &l;
    const CueList *subs = cueList(subCueList(parent));
    if (mainList == nullptr || subs == nullptr)
        return no(tr("No such cue."));
    const int p = mainList->indexOf(parent);
    const QString pNumber = cueNumber(*mainList, p);
    if (mainList->cues.at(p).skip)
        return no(tr("Cue %1 is skipped, so its sub-cues don't play from GO: put it back first.").arg(pNumber));

    int first = int(subs->cues.count());
    for (quint32 id : subCueIds)
        first = qMin(first, subs->indexOf(id));
    const quint32 mainId = mainList->id, subsId = subs->id;
    QList<quint32> moved;
    QStringList numbers, along;
    for (int j = first; j < subs->cues.count(); ++j)
    {
        moved << subs->cues.at(j).id;
        numbers << cueNumber(*subs, j);
        if (!subCueIds.contains(subs->cues.at(j).id))
            along << numbers.last();
    }

    // 0: as the list's cues play (List default), 1: tracking from the cue
    // before, 2: Cue-only, each including the one before -- a sub-cue's
    // rule exactly: from what its cue and the sub-cues before it put on
    // stage, nothing passed on. The first that plays every cue as before.
    QStringList renumbered;
    bool standInGone = false;
    auto build = [&](int variant) {
        renumbered.clear();
        QList<Production> ps = m_productions;
        Production &pr = ps[pi];
        CueList *list = pr.list(mainId);
        CueList *sl = pr.list(subsId);
        const uint parentHold = list->cues.at(p).hold;
        const QList<Cue> taken = sl->cues.mid(first);
        sl->cues = sl->cues.mid(0, first);
        // The stand-in Make Sub-cue put first for the cue's own look goes
        // when it is all that is left: the cue's step is its own again.
        standInGone = sl->cues.count() == 1 && isStandIn(sl->cues.first(), list->cues.at(p), pr);
        if (standInGone)
            sl->cues.clear();
        // Into the first: it played with GO (the cue goes on to it at once),
        // or Sub GO / Auto-follow, which is now the cue's own hold.
        list->cues[p].hold = first == 0 ? 0u
                             : taken.first().follow ? taken.first().wait : Function::infiniteSpeed();
        int at = p + 1;
        quint32 before = parent;
        for (int k = 0; k < taken.count(); ++k)
        {
            Cue c = taken.at(k);
            c.hold = k + 1 < taken.count()
                     ? (taken.at(k + 1).follow ? taken.at(k + 1).wait : Function::infiniteSpeed())
                     : parentHold;
            c.follow = false;
            c.wait = 0;
            c.includes.clear();
            c.mode = variant == 0 ? CueMode::ListDefault : variant == 1 ? CueMode::Track : CueMode::CueOnly;
            if (variant == 2)
                c.includes << before;
            if (!c.skip)
                before = c.id;
            const QString want = numbers.at(k);
            c.number = placeNumber(*list, at - 1, want, pr.numbering, takenNumbers(ps, *list), false, QString());
            if (c.number != want)
                renumbered << tr("%1 is numbered %2 here, as + Cue would (%1 is taken or out of order)").arg(want, c.number);
            list->cues.insert(at++, c);
        }
        return ps;
    };

    QList<Production> work;
    QList<quint32> carriedInto;
    QString why;
    int used = -1;
    for (int variant = 0; variant < 3 && used < 0; ++variant)
    {
        if (variant == 1 && mainList->mode == ListMode::Tracking)
            continue;                   // the same as List default
        work = build(variant);
        carriedInto.clear();
        if (carryUntilSame(work, pi, QHash<quint32, quint32>(), variant == 2, &carriedInto, &why))
            used = variant;
    }
    if (used < 0)
        return no(why);

    // Tidy: an include the check doesn't need goes.
    if (used == 2)
        for (quint32 id : moved)
        {
            Cue *c = cueOf(work[pi], id);
            if (c == nullptr || c->includes.isEmpty())
                continue;
            const QList<quint32> had = c->includes;
            c->includes.clear();
            QList<quint32> unused;
            if (!carryUntilSame(work, pi, QHash<quint32, quint32>(), false, &unused, nullptr))
                cueOf(work[pi], id)->includes = had;
        }
    tidyOffLayers(work, pi, moved, [&]() {
        return carryUntilSame(work, pi, QHash<quint32, quint32>(), false, nullptr, nullptr);
    });
    if (!reusablesKept(work, pi, moved + carriedInto, &why))
        return no(why);

    const QString label = moved.count() == 1 ? tr("Make inline cue") : tr("Make inline cues");
    if (!edit(label, [&work](QList<Production> &ps) {
            ps = work;
            return true;
        }))
        return no(tr("Not changed."));

    if (message != nullptr)
    {
        QStringList now;
        const CueList *list = cueList(mainId);
        for (quint32 id : moved)
            if (list != nullptr && list->indexOf(id) >= 0)
                now << cueNumber(*list, list->indexOf(id));
        QString text = moved.count() == 1 ? tr("%1 is a cue now, after %2: GO plays it.").arg(listJoin(now), pNumber)
                                          : tr("%1 are cues now, after %2: GO steps through them.")
                                                .arg(listJoin(now), pNumber);
        if (!along.isEmpty())
            text += QLatin1Char(' ') + tr("%1 came along: a sub-cue plays only after the ones before it.")
                                           .arg(listJoin(along));
        if (first == 0)
            text += QLatin1Char(' ') + tr("%1 goes on to %2 at once (it played with GO).").arg(pNumber, now.first());
        if (standInGone)
            text += QLatin1Char(' ') + tr("The sub-cue that stood for %1's own look went with them.").arg(pNumber);
        if (!renumbered.isEmpty())
            text += QLatin1Char(' ') + renumbered.join(QStringLiteral("; ")) + QLatin1Char('.');
        if (used == 2)
            text += QLatin1Char(' ') + tr("Cue-only, as sub-cues were: they pass nothing on.");
        QStringList into;
        for (quint32 id : carriedInto)
            if (const CueList *l = listOfCue(id))
                into << cueNumber(*l, l->indexOf(id));
        if (!into.isEmpty())
            text += QLatin1Char(' ') + tr("Looks carried into %1, so it plays as before.").arg(listJoin(into));
        *message = text;
    }
    return true;
}

/****************************************************************************
 * Make Sub-cue
 ****************************************************************************/

bool ShowTreeController::makeSubCues(const QList<quint32> &cueIds, quint32 parentId, QString *message)
{
    auto no = [message](const QString &text) {
        if (message != nullptr)
            *message = text;
        return false;
    };
    if (m_doc->mode() != Doc::Design)
        return no(tr("Design mode only: switch to Design to make cues sub-cues."));
    if (cueIds.isEmpty())
        return no(tr("Select a cue first."));
    const CueList *list = listOfCue(cueIds.first());
    if (list == nullptr)
        return no(tr("No such cue."));
    if (list->looks)
        return no(tr("A hit has no sub-cues: it plays what it has, from a VC button."));
    if (list->isSubCues())
        return no(tr("%1 is a sub-cue already.").arg(cueNumber(*list, list->indexOf(cueIds.first()))));
    QList<int> at;
    for (quint32 id : cueIds)
    {
        if (list->indexOf(id) < 0)
            return no(tr("Select cues of one cue list."));
        if (!at.contains(list->indexOf(id)))
            at << list->indexOf(id);
    }
    std::sort(at.begin(), at.end());
    if (parentId == invalidId())
    {
        if (at.first() == 0)
            return no(tr("%1 is the first cue: there is no cue before it to go under.").arg(cueNumber(*list, 0)));
        parentId = list->cues.at(at.first() - 1).id;
    }
    const int pIdx = list->indexOf(parentId);
    if (pIdx < 0)
    {
        const CueList *pl = listOfCue(parentId);
        if (pl != nullptr && pl->isSubCues())
            return no(tr("A sub-cue has no sub-cues of its own (one level): choose a cue."));
        return no(tr("Choose a cue of the same list to go under."));
    }
    if (pIdx >= at.first())
        return no(tr("A sub-cue plays inside its cue, so it goes under a cue before it."));
    const Cue &P = list->cues.at(pIdx);
    const QString pNumber = cueNumber(*list, pIdx);

    // What can't be a sub-cue, said before anything changes.
    int pi = -1;
    for (int i = 0; i < m_productions.count() && pi < 0; ++i)
        if (m_productions.at(i).list(list->id) != nullptr)
            pi = i;
    const Production &prod = m_productions.at(pi);
    for (int i : at)
    {
        const Cue &c = list->cues.at(i);
        const QString n = cueNumber(*list, i);
        if (c.id == list->baseCue)
            return no(tr("%1 is the list's base cue: released layers go back to it. Choose another base first.").arg(n));
        if (c.timecode >= 0 || c.tcAnchor)
            return no(tr("%1 goes on timecode, and a sub-cue can't: clear its time first.").arg(n));
        if (c.focusCall)
            return no(tr("%1 is a focus-call cue: it stays a cue of the list.").arg(n));
        for (const SectionHead &h : list->sections)
            if (h.cue == c.id && prod.numbering == Numbering::Manual)
                return no(tr("%1 begins a song section: it stays a cue of the list.").arg(n));
        for (int k = 0; k < list->cues.count(); ++k)
            if (!cueIds.contains(list->cues.at(k).id) && list->cues.at(k).includes.contains(c.id))
                return no(tr("%1 includes %2, and a sub-cue can't be included: take the include out first.")
                              .arg(cueNumber(*list, k), n));
        if (P.skip && !c.skip)
            return no(tr("Cue %1 is skipped, so its sub-cues wouldn't play from GO: put it back first.").arg(pNumber));
    }

    const quint32 listId = list->id;
    const quint32 newSubs = subCueList(parentId) == invalidId() ? newId() : invalidId();
    const quint32 standIn = newId();
    QStringList numbersBefore, renumbered;
    for (int i : at)
        numbersBefore << cueNumber(*list, i);
    QList<quint32> moved;
    bool madeStandIn = false;
    QStringList cameAlong;

    QList<Production> work = m_productions;
    {
        Production &pr = work[pi];
        QList<quint32> order;
        for (int i : at)
            order << list->cues.at(i).id;
        for (quint32 cid : order)
        {
            CueList *L = pr.list(listId);
            const int c = L->indexOf(cid);
            const int pc = L->indexOf(parentId);
            const QString cNumber = cueNumberAt(*L, c);
            const Cue cue = L->cues.at(c);
            CueList *S = subsOf(pr, parentId);
            if (S == nullptr)
            {
                CueList l;
                l.id = newSubs;
                l.subOf = parentId;
                l.mode = ListMode::Tracking;
                l.name = tr("Sub-cues");
                pr.lists << l;
                S = &pr.lists.last();
                L = pr.list(listId);
            }
            const bool fresh = S->cues.isEmpty();
            Cue sub = cue;
            sub.mode = CueMode::ListDefault;
            sub.includes.clear();
            sub.hold = Function::infiniteSpeed();
            sub.follow = false;
            sub.wait = 0;
            // How it was reached: right after its new cue, that cue's hold
            // (Auto-follow) or GO (Sub GO); its own hold is now the cue's.
            Cue &parentCue = L->cues[pc];
            bool first = false;
            if (c == pc + 1)
            {
                if (fresh && parentCue.hold == 0)
                    first = true;                   // the cue went on to it at once: it plays with GO
                else if (!infiniteHold(parentCue.hold))
                {
                    sub.follow = true;
                    sub.wait = parentCue.hold;
                }
                parentCue.hold = cue.hold;
            }
            const QString pn = cueNumberAt(*L, pc);
            QString suffix = cNumber.startsWith(pn + QLatin1Char('.')) ? cNumber.mid(pn.length() + 1) : QString();
            if (fresh && !first)
            {
                // The cue's own look was a step of its own: a sub-cue
                // stands for it, first, with the cue's fades.
                Cue s;
                s.id = standIn;
                s.fadeIn = parentCue.fadeIn;
                s.fadeOut = parentCue.fadeOut;
                s.number = !suffix.isEmpty() && isCueNumber(suffix) && compareCueNumbers(QStringLiteral("1"), suffix) < 0
                           ? QStringLiteral("1") : numberBetween(QString(), suffix.isEmpty() ? QStringLiteral("2") : suffix);
                if (s.number.isEmpty())
                    s.number = QStringLiteral("1");
                S->cues << s;
                madeStandIn = true;
            }
            const QString n = placeNumber(*S, int(S->cues.count()) - 1, suffix.isEmpty() ? QStringLiteral("x") : suffix,
                                          pr.numbering, QStringList(), true, pn);
            sub.number = n;
            if (suffix.isEmpty() || n != suffix)
                renumbered << tr("%1 is %2.%3").arg(cNumber, pn, n);
            S->cues << sub;
            moved << cid;
            // Its own sub-cues come along, after it: the first went on with
            // GO into it, so now it follows at once.
            if (CueList *T = subsOf(pr, cid))
            {
                const QList<Cue> ts = T->cues;
                for (int k = 0; k < ts.count(); ++k)
                {
                    Cue t = ts.at(k);
                    const QString tn = cueNumberAt(*T, k, cNumber);
                    if (k == 0)
                    {
                        t.follow = true;
                        t.wait = 0;
                    }
                    const QString ts2 = tn.startsWith(pn + QLatin1Char('.')) ? tn.mid(pn.length() + 1) : QString();
                    t.number = placeNumber(*S, int(S->cues.count()) - 1, ts2.isEmpty() ? QStringLiteral("x") : ts2,
                                           pr.numbering, QStringList(), true, pn);
                    if (ts2.isEmpty() || t.number != ts2)
                        renumbered << tr("%1 is %2.%3").arg(tn, pn, t.number);
                    S->cues << t;
                    moved << t.id;
                    cameAlong << tn;
                }
                for (int i = 0; i < pr.lists.count(); ++i)
                    if (pr.lists.at(i).subOf == cid)
                    {
                        pr.lists.removeAt(i);
                        break;
                    }
                L = pr.list(listId);
            }
            L->cues.removeAt(L->indexOf(cid));
        }
    }

    QHash<quint32, quint32> pair;
    if (madeStandIn)
        pair.insert(standIn, parentId);
    QList<quint32> carriedInto;
    QString why;
    if (!carryUntilSame(work, pi, pair, true, &carriedInto, &why))
        return no(why);
    tidyOffLayers(work, pi, moved, [&]() { return carryUntilSame(work, pi, pair, false, nullptr, nullptr); });
    if (!reusablesKept(work, pi, moved + carriedInto, &why))
        return no(why);

    const QString label = at.count() == 1 ? tr("Make sub-cue") : tr("Make sub-cues");
    if (!edit(label, [&work](QList<Production> &ps) {
            ps = work;
            return true;
        }))
        return no(tr("Not changed."));

    if (message != nullptr)
    {
        const CueList *sl = cueList(subCueList(parentId));
        QString text = at.count() == 1
                       ? tr("%1 is a sub-cue of %2 now: Sub GO plays it.").arg(numbersBefore.first(), pNumber)
                       : tr("%1 are sub-cues of %2 now: Sub GO steps through them.").arg(listJoin(numbersBefore), pNumber);
        if (madeStandIn && sl != nullptr && sl->indexOf(standIn) >= 0)
            text += QLatin1Char(' ') + tr("%1 is %2's own look, first (GO plays it).")
                                           .arg(cueNumber(*sl, sl->indexOf(standIn)), pNumber);
        if (!cameAlong.isEmpty())
            text += QLatin1Char(' ') + tr("Its sub-cues came along after it.");
        if (!renumbered.isEmpty())
            text += QLatin1Char(' ') + tr("Numbered as + Sub-cue does: %1.").arg(renumbered.join(QStringLiteral("; ")));
        QStringList into;
        for (quint32 id : carriedInto)
            if (const CueList *l = listOfCue(id))
                into << cueNumber(*l, l->indexOf(id));
        if (!into.isEmpty())
            text += QLatin1Char(' ') + tr("Looks carried into %1, so it plays as before.").arg(listJoin(into));
        *message = text;
    }
    return true;
}

/* A reusable cue's use (P4) can't change content here: an edit to one is an
   edit to all. */
bool ShowTreeController::reusablesKept(const QList<Production> &work, int pi, const QList<quint32> &cues,
                                       QString *why) const
{
    for (quint32 id : cues)
    {
        Production a = m_productions.at(pi), b = work.at(pi);
        CueList *la = nullptr, *lb = nullptr;
        const Cue *ca = cueOf(a, id, &la);
        const Cue *cb = cueOf(b, id, &lb);
        if (ca == nullptr || cb == nullptr || ca->reusable == invalidId())
            continue;
        if (cueContent(*ca) != cueContent(*cb))
        {
            if (why != nullptr)
                *why = tr("Not changed: %1 is a use of a reusable cue, and it would have to change (an edit to "
                          "one is an edit to all). Unlink it first.").arg(cueNumber(*la, la->indexOf(id)));
            return false;
        }
    }
    return true;
}

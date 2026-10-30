/*
  Q Light Controller Plus
  showtreehits.cpp

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

#include "showtreecontroller.h"
#include "showcontrol.h"
#include "chaser.h"
#include "doc.h"

using namespace ShowTree;

/* Scoped hits (Q154, Q157; 0.3.1). A hit's place is where it lives in the
   tree -- under a cue, a cue list, its production (the Hits section) or at
   the top (global) -- and it is armed while that place is live. The place
   is only Cue::scope / scopeOf: the hit stays a look of its production's
   Hits section, so its layers, bake, Collection, fades, folders and VC
   buttons are exactly a production hit's. normalizeLooks() moves a hit
   whose cue or list is gone up to its production.

   Arming is read when asked (a VC button asks on hitArmingChanged()):
   whether a list's chaser runs and which cue it is on. Nothing here runs
   per frame. */

ShowTree::HitScope ShowTreeController::hitScope(quint32 hitId, quint32 *scopeOf) const
{
    const CueList *l = listOfCue(hitId);
    const Cue *c = l != nullptr && l->looks ? l->cue(hitId) : nullptr;
    if (scopeOf != nullptr)
        *scopeOf = c == nullptr ? invalidId() : c->scopeOf;
    return c == nullptr ? HitScope::Production : c->scope;
}

/** The production a place belongs to (Global: @a placeId if a production,
    else the current one, else the first); invalid when there is no such
    place. */
static quint32 placeProduction(const ShowTreeController *st, HitScope scope, quint32 placeId)
{
    quint32 prod = invalidId();
    switch (scope)
    {
    case HitScope::Cue:
    {
        const CueList *l = st->listOfCue(placeId);
        if (l == nullptr || l->looks)
            return invalidId();
        st->cueList(l->id, &prod);
        return prod;
    }
    case HitScope::List:
    {
        const CueList *l = st->cueList(placeId, &prod);
        return l == nullptr || l->looks || l->isSubCues() ? invalidId() : prod;
    }
    case HitScope::Production:
        return st->production(placeId) != nullptr ? placeId : invalidId();
    case HitScope::Global:
        if (st->production(placeId) != nullptr)
            return placeId;
        if (st->production(st->currentProduction()) != nullptr)
            return st->currentProduction();
        return st->productions().isEmpty() ? invalidId() : st->productions().first().id;
    }
    return invalidId();
}

quint32 ShowTreeController::addHit(HitScope scope, quint32 placeId, const QString &name)
{
    const quint32 prodId = placeProduction(this, scope, placeId);
    if (prodId == invalidId())
        return invalidId();
    const quint32 sectionId = looksSection(prodId) == invalidId() ? newId() : invalidId();
    const quint32 id = newId();
    const bool ok = edit(tr("New hit"), [&](QList<Production> &ps) {
        for (Production &p : ps)
        {
            if (p.id != prodId)
                continue;
            CueList *l = p.looks();
            if (l == nullptr)
            {
                CueList fresh;
                fresh.id = sectionId;
                fresh.name = tr("Hits");
                fresh.looks = true;
                p.lists << fresh;
                l = &p.lists.last();
            }
            Cue c;
            c.id = id;
            c.name = name;
            c.scope = scope;
            c.scopeOf = scope == HitScope::Cue || scope == HitScope::List ? placeId : invalidId();
            c.number = numberForInsert(*l, int(l->cues.count()) - 1);
            l->cues << c;
            return true;
        }
        return false;
    });
    return ok ? id : invalidId();
}

bool ShowTreeController::setHitScope(const QList<quint32> &hitIds, HitScope scope, quint32 placeId, QString *why)
{
    const quint32 prodId = scope == HitScope::Global ? invalidId() : placeProduction(this, scope, placeId);
    if (scope != HitScope::Global && prodId == invalidId())
    {
        if (why != nullptr)
            *why = tr("A hit can live under a cue, a cue list or a production, or at the top (everywhere).");
        return false;
    }
    QList<quint32> hits;
    for (quint32 h : hitIds)
        if (isLook(h) && !hits.contains(h))
            hits << h;
    if (hits.isEmpty())
        return false;
    const quint32 sectionId = prodId != invalidId() && looksSection(prodId) == invalidId() ? newId() : invalidId();
    const quint32 scopeOf = scope == HitScope::Cue || scope == HitScope::List ? placeId : invalidId();
    QString label = hits.count() == 1 ? tr("Move hit") : tr("Move %1 hits").arg(hits.count());
    const bool ok = edit(label, [&](QList<Production> &ps) {
        bool any = false;
        Production *dest = nullptr;
        for (Production &p : ps)
            if (p.id == prodId)
                dest = &p;
        auto move = [&](quint32 h) {
            for (Production &p : ps)
                for (CueList &l : p.lists)
                {
                    const int i = l.looks ? l.indexOf(h) : -1;
                    if (i < 0)
                        continue;
                    if (dest != nullptr && dest != &p)
                    {
                        // Into another production: its Hits section.
                        Cue moved = l.cues.takeAt(i);
                        moved.scope = scope;
                        moved.scopeOf = scopeOf;
                        moved.folder.clear();
                        CueList *to = dest->looks();
                        if (to == nullptr)
                        {
                            CueList fresh;
                            fresh.id = sectionId;
                            fresh.name = tr("Hits");
                            fresh.looks = true;
                            dest->lists << fresh;
                            to = &dest->lists.last();
                        }
                        moved.number = numberForInsert(*to, int(to->cues.count()) - 1);
                        to->cues << moved;
                        return true;
                    }
                    Cue &c = l.cues[i];
                    if (c.scope == scope && c.scopeOf == scopeOf)
                        return false;
                    c.scope = scope;
                    c.scopeOf = scopeOf;
                    return true;
                }
            return false;
        };
        for (quint32 h : hits)
            any = move(h) || any;
        return any;
    });
    if (!ok && why != nullptr && why->isEmpty())
        *why = tr("Already there");
    return ok;
}

QList<quint32> ShowTreeController::copyHitsTo(const QList<quint32> &hitIds, HitScope scope, quint32 placeId)
{
    const quint32 prodId = placeProduction(this, scope, placeId);
    if (prodId == invalidId())
        return {};
    // Global: kept in the copied hit's own production.
    QList<Cue> copies;
    QList<quint32> prods;
    for (quint32 h : hitIds)
    {
        const CueList *from = listOfCue(h);
        const Cue *src = from == nullptr || !from->looks ? nullptr : from->cue(h);
        if (src == nullptr)
            continue;
        quint32 fromProd = invalidId();
        cueList(from->id, &fromProd);
        Cue copy = *src;
        copy.id = newId();
        copy.collection = invalidId();
        copy.scope = scope;
        copy.scopeOf = scope == HitScope::Cue || scope == HitScope::List ? placeId : invalidId();
        QHash<quint32, quint32> ids;
        for (Layer &l : copy.layers)
        {
            const quint32 nid = newId();
            ids.insert(l.id, nid);
            l.id = nid;
        }
        for (Layer &l : copy.layers)
            if (l.parent != invalidId())
                l.parent = ids.value(l.parent, invalidId());
        copies << copy;
        prods << (scope == HitScope::Global ? fromProd : prodId);
    }
    if (copies.isEmpty())
        return {};
    QHash<quint32, quint32> sections;
    for (quint32 p : prods)
        if (looksSection(p) == invalidId() && !sections.contains(p))
            sections.insert(p, newId());
    QList<quint32> made;
    const bool ok = edit(copies.count() == 1 ? tr("Copy hit") : tr("Copy %1 hits").arg(copies.count()),
                         [&](QList<Production> &ps) {
        for (int k = 0; k < copies.count(); ++k)
            for (Production &p : ps)
            {
                if (p.id != prods.at(k))
                    continue;
                CueList *l = p.looks();
                if (l == nullptr)
                {
                    CueList fresh;
                    fresh.id = sections.value(p.id);
                    fresh.name = tr("Hits");
                    fresh.looks = true;
                    p.lists << fresh;
                    l = &p.lists.last();
                }
                Cue c = copies.at(k);
                c.number = numberForInsert(*l, int(l->cues.count()) - 1);
                l->cues << c;
                made << c.id;
            }
        return !made.isEmpty();
    });
    return ok ? made : QList<quint32>();
}

QList<quint32> ShowTreeController::hitsAt(HitScope scope, quint32 placeId) const
{
    QList<quint32> out;
    for (const Production &p : m_productions)
    {
        if (scope == HitScope::Production && p.id != placeId)
            continue;
        if (const CueList *l = p.looks())
            for (const Cue &c : l->cues)
                if (c.scope == scope && (scope == HitScope::Global || scope == HitScope::Production
                                         || c.scopeOf == placeId))
                    out << c.id;
    }
    return out;
}

QString ShowTreeController::hitPlace(quint32 hitId) const
{
    quint32 scopeOf = invalidId();
    const HitScope scope = hitScope(hitId, &scopeOf);
    switch (scope)
    {
    case HitScope::Global:
        return tr("everywhere");
    case HitScope::Cue:
        if (const CueList *l = listOfCue(scopeOf))
            return tr("in cue %1").arg(cueLabel(*l, l->indexOf(scopeOf)));
        break;
    case HitScope::List:
        if (const CueList *l = cueList(scopeOf))
            return tr("in cue list %1").arg(l->name);
        break;
    case HitScope::Production:
        break;
    }
    const CueList *l = listOfCue(hitId);
    quint32 prod = invalidId();
    if (l != nullptr)
        cueList(l->id, &prod);
    const Production *p = production(prod);
    return p == nullptr ? QString() : tr("in %1").arg(p->name);
}

bool ShowTreeController::isHitPlaceLive(HitScope scope, quint32 placeId) const
{
    auto productionLive = [this](quint32 prod) {
        return m_current == invalidId() || production(m_current) == nullptr || m_current == prod;
    };
    switch (scope)
    {
    case HitScope::Global:
        return true;
    case HitScope::Production:
        return productionLive(placeId);
    case HitScope::Cue:
    {
        const CueList *l = listOfCue(placeId);
        return l != nullptr && runningCue(l->id) == placeId;
    }
    case HitScope::List:
    {
        quint32 prod = invalidId();
        const CueList *l = cueList(placeId, &prod);
        if (l == nullptr || l->looks)
            return false;
        auto runs = [this](const CueList &list) {
            const Function *f = list.chaser == invalidId() ? nullptr : m_doc->function(list.chaser);
            return f != nullptr && f->isRunning();
        };
        if (runs(*l))
            return true;
        // Nothing of its production running: its main list, while the
        // production is current, is "the list we're in".
        const Production *p = production(prod);
        for (const CueList &other : p->lists)
            if (!other.looks && !other.isSubCues() && runs(other))
                return false;
        return productionLive(prod) && mainList(prod) == placeId;
    }
    }
    return false;
}

bool ShowTreeController::isHitArmed(quint32 hitId) const
{
    const CueList *l = listOfCue(hitId);
    const Cue *c = l != nullptr && l->looks ? l->cue(hitId) : nullptr;
    if (c == nullptr)
        return false;
    if (c->scope == HitScope::Production)
    {
        quint32 prod = invalidId();
        cueList(l->id, &prod);
        return isHitPlaceLive(HitScope::Production, prod);
    }
    return isHitPlaceLive(c->scope, c->scopeOf);
}

quint32 ShowTreeController::armedHit(const QList<quint32> &hitIds) const
{
    quint32 best = invalidId();
    int bestRank = -1;
    for (quint32 h : hitIds)
    {
        if (!isHitArmed(h))
            continue;
        const int rank = int(hitScope(h));
        if (rank > bestRank)
        {
            best = h;
            bestRank = rank;
        }
    }
    return best;
}

QList<quint32> ShowTreeController::armedTier(const QList<quint32> &hitIds) const
{
    QList<quint32> out;
    int bestRank = -1;
    for (quint32 h : hitIds)
    {
        if (!isHitArmed(h))
            continue;
        const int rank = int(hitScope(h));
        if (rank > bestRank)
        {
            out.clear();
            bestRank = rank;
        }
        if (rank == bestRank && !out.contains(h))
            out << h;
    }
    return out;
}

/** A hit's place as (scope, id): the production for a production hit. */
static QPair<int, quint32> placeKey(const ShowTreeController *st, quint32 hitId)
{
    quint32 of = invalidId();
    const HitScope scope = st->hitScope(hitId, &of);
    if (scope == HitScope::Global)
        return qMakePair(int(scope), invalidId());
    if (scope == HitScope::Production)
    {
        const CueList *l = st->listOfCue(hitId);
        if (l != nullptr)
            st->cueList(l->id, &of);
    }
    return qMakePair(int(scope), of);
}

bool ShowTreeController::sameHitPlace(quint32 a, quint32 b) const
{
    return isLook(a) && isLook(b) && placeKey(this, a) == placeKey(this, b);
}

bool ShowTreeController::hitOverrides(quint32 narrow, quint32 wide) const
{
    if (!isLook(narrow) || !isLook(wide))
        return false;
    const QPair<int, quint32> n = placeKey(this, narrow);
    const QPair<int, quint32> w = placeKey(this, wide);
    if (n.first <= w.first)
        return false;
    if (HitScope(w.first) == HitScope::Global)
        return true;
    // The production, list and cue list narrow's place is in.
    quint32 prod = invalidId(), list = invalidId();
    if (HitScope(n.first) == HitScope::Cue)
    {
        const CueList *l = listOfCue(n.second);
        if (l != nullptr && l->isSubCues())
            l = listOfCue(l->subOf);            // a sub-cue: its cue's list
        if (l != nullptr)
        {
            list = l->id;
            cueList(l->id, &prod);
        }
    }
    else if (HitScope(n.first) == HitScope::List)
    {
        list = n.second;
        cueList(list, &prod);
    }
    if (HitScope(w.first) == HitScope::Production)
        return prod == w.second;
    return HitScope(w.first) == HitScope::List && list == w.second;
}

void ShowTreeController::noteListRuntime(quint32 chaserId)
{
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (l.chaser == chaserId)
            {
                if (Chaser *ch = qobject_cast<Chaser *>(m_doc->function(chaserId)))
                    connect(ch, &Chaser::currentStepChanged, this, &ShowTreeController::slotListStepChanged,
                            Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
                emit hitArmingChanged();
                return;
            }
}

void ShowTreeController::slotListStepChanged()
{
    emit hitArmingChanged();
}

/*
  Q Light Controller Plus
  showtreesubcues.cpp

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

/* Sub-cues in the tree (docs/design/SHOW_TREE_DESIGN.md §3C, D9, D25; P6). A cue's
   sub-cues 4.1, 4.2... are a list of their own (CueList::subOf), as the
   Looks section is a flagged list (R3): so layers, looks, the cascade,
   copy/paste, rename, delete, undo and the Design preview work on a sub-cue
   as they do on any cue. What makes them sub-cues:

   - the resolver starts the first from what its cue puts on stage, each next
     from the one before (always tracking), and nothing passes on to the cue
     after the parent -- sub-cue changes are cue-only (D9);
   - the bake makes the list a hidden Chaser of the sub-cues' Collections and
     puts it as the parent's step, SubCues="SubGo" (docs/design/SUBCUES_DESIGN.md), or
     "SubGoFollow" when a sub-cue auto-follows (its wait is the hold of the
     sub-cue before it). */

#include "showtreecontroller.h"
#include "doc.h"

using namespace ShowTree;

static const CueList *parentListOf(const QList<Production> &ps, quint32 cueId)
{
    for (const Production &p : ps)
        for (const CueList &l : p.lists)
            if (!l.isSubCues() && l.indexOf(cueId) >= 0)
                return &l;
    return nullptr;
}

quint32 ShowTreeController::subCueList(quint32 cueId) const
{
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (l.subOf == cueId && cueId != invalidId())
                return l.id;
    return invalidId();
}

bool ShowTreeController::isSubCue(quint32 cueId, quint32 *parentCue) const
{
    const CueList *l = listOfCue(cueId);
    if (l == nullptr || !l->isSubCues())
        return false;
    if (parentCue != nullptr)
        *parentCue = l->subOf;
    return true;
}

QString ShowTreeController::cueNumber(const CueList &list, int index) const
{
    if (!list.isSubCues())
        return cueNumberAt(list, index);
    const CueList *parent = parentListOf(m_productions, list.subOf);
    const QString up = parent == nullptr ? QString() : cueNumberAt(*parent, parent->indexOf(list.subOf));
    return cueNumberAt(list, index, up);
}

QStringList ShowTreeController::takenNumbers(const QList<Production> &ps, const CueList &list)
{
    QStringList out;
    if (list.isSubCues() || list.looks)
        return out;
    for (const Production &p : ps)
        for (const CueList &s : p.lists)
        {
            const int at = s.isSubCues() ? list.indexOf(s.subOf) : -1;
            if (at < 0)
                continue;
            const QString up = cueNumberAt(list, at);
            for (int k = 0; k < s.cues.count(); ++k)
                out << cueNumberAt(s, k, up);
        }
    return out;
}

quint32 ShowTreeController::addSubCue(quint32 cueId, const QString &name, quint32 afterId, QString *why)
{
    auto no = [why](const QString &text) {
        if (why != nullptr)
            *why = text;
        return invalidId();
    };
    const CueList *list = listOfCue(cueId);
    if (list == nullptr)
        return no(tr("No such cue."));
    if (list->looks)
        return no(tr("A look has no sub-cues: it plays what it has, from a VC button."));
    if (list->isSubCues())
        return no(tr("A sub-cue has no sub-cues of its own (one level): add another sub-cue to its cue."));

    const quint32 id = newId();
    const quint32 newList = subCueList(cueId) == invalidId() ? newId() : invalidId();
    const bool ok = edit(tr("New sub-cue"), [&](QList<Production> &ps) {
        CueList *subs = nullptr;
        Production *home = nullptr;
        for (Production &p : ps)
            for (CueList &l : p.lists)
            {
                if (l.indexOf(cueId) >= 0 && !l.isSubCues())
                    home = &p;
                if (l.subOf == cueId)
                    subs = &l;
            }
        if (home == nullptr)
            return false;
        if (subs == nullptr)
        {
            CueList l;
            l.id = newList;
            l.subOf = cueId;
            l.mode = ListMode::Tracking;
            l.name = tr("Sub-cues");
            home->lists << l;
            subs = &home->lists.last();
        }
        const int after = afterId == invalidId() ? int(subs->cues.count()) - 1 : subs->indexOf(afterId);
        Cue c;
        c.id = id;
        c.name = name;
        c.number = numberForInsert(*subs, after);
        subs->cues.insert(qBound(0, after + 1, int(subs->cues.count())), c);
        return true;
    });
    return ok ? id : invalidId();
}

bool ShowTreeController::setSubCueFollow(quint32 subCueId, bool follow, uint wait)
{
    if (!isSubCue(subCueId))
        return false;
    const Cue *now = cue(subCueId);
    if (now->follow == follow && (!follow || now->wait == wait))
        return false;
    return edit(follow ? tr("Auto-follow") : tr("Sub GO"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            for (CueList &l : p.lists)
                if (Cue *c = l.isSubCues() ? l.cue(subCueId) : nullptr)
                {
                    c->follow = follow;
                    c->wait = follow ? wait : 0;
                    return true;
                }
        return false;
    });
}

int ShowTreeController::subCueListsOf(quint32 listId) const
{
    const CueList *list = cueList(listId);
    int n = 0;
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (list != nullptr && l.isSubCues() && list->indexOf(l.subOf) >= 0)
                ++n;
    return n;
}

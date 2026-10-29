/*
  Q Light Controller Plus
  showtreeproductions.cpp

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

/* Multiple productions (docs/design/SHOW_TREE_DESIGN.md §8 P5): which production is
   current -- what GO, the footer, VC and show control play -- its main list
   and its timecode source. The current production is the show's main cue
   list seen from the tree: making a production current makes its main
   list's chaser the main cue list, and a main cue list chosen elsewhere (a
   chaser editor) makes its production current. One source of truth for what
   a GO with no list plays. */

#include "showtreecontroller.h"
#include "timecodesource.h"
#include "showcontrol.h"
#include "chaser.h"
#include "doc.h"

using namespace ShowTree;

QList<quint32> ShowTreeController::playableLists(quint32 productionId) const
{
    QList<quint32> out;
    const Production *p = production(productionId);
    if (p == nullptr)
        return out;
    for (const CueList &l : p->lists)
        if (!l.looks && !l.isSubCues())
            out << l.id;
    return out;
}

quint32 ShowTreeController::mainList(quint32 productionId) const
{
    const Production *p = production(productionId);
    if (p == nullptr)
        return invalidId();
    const QList<quint32> lists = playableLists(productionId);
    if (lists.contains(p->mainList))
        return p->mainList;
    return lists.isEmpty() ? invalidId() : lists.first();
}

quint32 ShowTreeController::productionOfChaser(quint32 chaserId, quint32 *listId) const
{
    if (chaserId == invalidId())
        return invalidId();
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (!l.looks && !l.isSubCues() && l.chaser == chaserId)
            {
                if (listId != nullptr)
                    *listId = l.id;
                return p.id;
            }
    return invalidId();
}

bool ShowTreeController::setCurrentProduction(quint32 productionId)
{
    if (productionId != invalidId() && production(productionId) == nullptr)
        return false;
    const bool moved = productionId != m_current;
    m_current = productionId;
    applyProductionTimecode();
    syncCurrentProduction();
    if (moved)
    {
        m_doc->setModified();
        emit currentProductionChanged();
    }
    return true;
}

bool ShowTreeController::setMainList(quint32 listId)
{
    quint32 pid = invalidId();
    const CueList *list = cueList(listId, &pid);
    if (list == nullptr || list->looks || list->isSubCues())
        return false;
    if (production(pid)->mainList == listId)
        return true;
    const bool ok = edit(tr("Main list"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == pid)
            {
                p.mainList = listId;
                return true;
            }
        return false;
    });
    if (ok && pid == m_current)
        emit currentProductionChanged();
    return ok;
}

bool ShowTreeController::setProductionTimecode(quint32 productionId, qint32 universe)
{
    if (universe < -2)
        universe = -2;
    const Production *prod = production(productionId);
    if (prod == nullptr)
        return false;
    if (prod->timecode == universe)
        return true;
    const bool ok = edit(tr("Timecode source"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == productionId)
            {
                p.timecode = universe;
                return true;
            }
        return false;
    });
    applyProductionTimecode();
    return ok;
}

void ShowTreeController::syncCurrentProduction()
{
    if (m_current == invalidId())
        return;
    if (production(m_current) == nullptr)
    {
        // Deleted (or undone away): no current production; the main cue
        // list is whatever it was left as.
        m_current = invalidId();
        applyProductionTimecode();
        emit currentProductionChanged();
        return;
    }
    applyProductionTimecode();
    ShowControl *sc = m_doc->showControl();
    const CueList *list = cueList(mainList(m_current));
    if (sc == nullptr || list == nullptr || m_doc->function(list->chaser) == nullptr)
        return;
    if (sc->mainCueList() == list->chaser)
        return;
    m_settingMain = true;
    sc->setMainCueList(list->chaser);
    m_settingMain = false;
}

void ShowTreeController::applyProductionTimecode()
{
    TimecodeSource *tc = m_doc->timecodeSource();
    if (tc == nullptr)
        return;
    const Production *p = production(m_current);
    tc->setProductionUniverse(p != nullptr ? p->timecode : -2);
}

void ShowTreeController::slotMainCueListChanged(quint32 chaserId)
{
    // Our own setting, or a chaser deleted by a bake (the next sync points
    // the main cue list at the production's list again).
    if (m_settingMain || m_baking)
        return;
    quint32 listId = invalidId();
    const quint32 pid = productionOfChaser(chaserId, &listId);
    if (pid == invalidId())
    {
        // A plain chaser (or none) is the main cue list now: GO plays it,
        // not a production.
        if (m_current != invalidId())
        {
            m_current = invalidId();
            applyProductionTimecode();
            emit currentProductionChanged();
        }
        return;
    }
    // A list of a production: it is current, with that list as its main --
    // the operator's choice, as the main cue list itself is, not an undo step.
    for (Production &p : m_productions)
        if (p.id == pid)
            p.mainList = listId;
    m_current = pid;
    applyProductionTimecode();
    emit currentProductionChanged();
}

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

#include <QCoreApplication>
#include <QHash>

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

/****************************************************************************
 * Duplicate and paste: a cue list, a production, a reusable cue (Q215)
 ****************************************************************************/

namespace {

/** The ids a copy of some lists / cues takes, old -> new. */
struct CopyMap
{
    QHash<quint32, quint32> cues;
    QHash<quint32, quint32> lists;
};

quint32 mapped(const QHash<quint32, quint32> &m, quint32 id)
{
    return m.value(id, id);
}

/** New ids for the layers of @a layers, their parents following. A layer
 *  that uses a reusable keeps using it (the reusable is shared). */
void freshLayers(QList<Layer> &layers, const std::function<quint32()> &newId)
{
    QHash<quint32, quint32> ids;
    for (Layer &l : layers)
    {
        const quint32 nid = newId();
        ids.insert(l.id, nid);
        l.id = nid;
    }
    for (Layer &l : layers)
        if (l.parent != invalidId())
            l.parent = ids.value(l.parent, invalidId());
}

/** Ids for every cue of @a lists (and the lists), before any is copied, so
 *  includes, sub-cue lists, hit scopes and sections can follow. */
void planCopy(const QList<CueList> &lists, const std::function<quint32()> &newId, CopyMap *map)
{
    for (const CueList &l : lists)
    {
        map->lists.insert(l.id, newId());
        for (const Cue &c : l.cues)
            map->cues.insert(c.id, newId());
    }
}

/** A deep copy of @a src with the ids of @a map. What it plays is generated
 *  again (no chaser, no collections), and it is not an import. */
CueList copyOfList(const CueList &src, const CopyMap &map, const std::function<quint32()> &newId)
{
    CueList l = src;
    l.id = mapped(map.lists, src.id);
    l.chaser = invalidId();
    l.source = invalidId();
    l.inPlace = false;
    l.originals.clear();
    if (l.baseCue != invalidId())
        l.baseCue = mapped(map.cues, l.baseCue);
    if (l.subOf != invalidId())
        l.subOf = mapped(map.cues, l.subOf);
    for (SectionHead &h : l.sections)
        if (h.cue != invalidId())
            h.cue = mapped(map.cues, h.cue);
    for (Cue &c : l.cues)
    {
        c.id = mapped(map.cues, c.id);
        c.collection = invalidId();
        for (quint32 &inc : c.includes)
            inc = mapped(map.cues, inc);
        if (c.scopeOf != invalidId())
            c.scopeOf = mapped(map.cues, mapped(map.lists, c.scopeOf));
        freshLayers(c.layers, newId);
    }
    return l;
}

QString copyName(const QString &name, const QStringList &taken)
{
    QString n = QCoreApplication::translate("ShowTreeController", "%1 (copy)").arg(name);
    for (int k = 2; taken.contains(n); ++k)
        n = QCoreApplication::translate("ShowTreeController", "%1 (copy %2)").arg(name).arg(k);
    return n;
}

} // namespace

quint32 ShowTreeController::pasteCueList(quint32 listId, quint32 toProductionId, bool nameAsIs)
{
    quint32 fromProduction = invalidId();
    const CueList *src = cueList(listId, &fromProduction);
    if (src == nullptr || src->looks || src->isSubCues() || production(toProductionId) == nullptr)
        return invalidId();
    // The list and the sub-cue lists of its cues, ids made up front.
    QList<CueList> parts { *src };
    const Production *from = production(fromProduction);
    for (const CueList &sub : from->lists)
        if (sub.isSubCues() && src->indexOf(sub.subOf) >= 0)
            parts << sub;
    CopyMap map;
    planCopy(parts, [this] { return newId(); }, &map);
    QList<CueList> copies;
    for (const CueList &part : parts)
        copies << copyOfList(part, map, [this] { return newId(); });
    const quint32 newListId = copies.first().id;
    const bool ok = edit(nameAsIs ? tr("Paste cue list") : tr("Duplicate cue list"), [&](QList<Production> &ps) {
        for (Production &p : ps)
        {
            if (p.id != toProductionId)
                continue;
            QStringList taken;
            for (const CueList &l : p.lists)
                taken << l.name;
            // Pasted into another production: the name it had, when free.
            if (!nameAsIs || taken.contains(copies.first().name))
                copies.first().name = copyName(src->name, taken);
            for (const CueList &l : copies)
                p.lists << l;
            return true;
        }
        return false;
    });
    return ok ? newListId : invalidId();
}

quint32 ShowTreeController::duplicateCueList(quint32 listId)
{
    quint32 pid = invalidId();
    if (cueList(listId, &pid) == nullptr)
        return invalidId();
    return pasteCueList(listId, pid, false);
}

quint32 ShowTreeController::pasteProduction(quint32 productionId)
{
    const Production *src = production(productionId);
    if (src == nullptr)
        return invalidId();
    CopyMap map;
    planCopy(src->lists, [this] { return newId(); }, &map);
    Production p = *src;
    p.id = newId();
    p.lists.clear();
    for (const CueList &l : src->lists)
        p.lists << copyOfList(l, map, [this] { return newId(); });
    if (p.mainList != invalidId())
        p.mainList = mapped(map.lists, p.mainList);
    for (RunningSong &r : p.runningOrder)
        if (r.id.startsWith(QLatin1Char('#')))
        {
            bool ok = false;
            const quint32 cue = r.id.mid(1).toUInt(&ok);
            if (ok)
                r.id = QLatin1Char('#') + QString::number(mapped(map.cues, cue));
        }
    for (ScriptAnchor &a : p.script.anchors)
        a.cue = mapped(map.cues, a.cue);
    const quint32 id = p.id;
    const bool ok = edit(tr("Duplicate production"), [&](QList<Production> &ps) {
        QStringList taken;
        for (const Production &have : ps)
            taken << have.name;
        p.name = copyName(src->name, taken);
        ps << p;
        return true;
    });
    return ok ? id : invalidId();
}

quint32 ShowTreeController::duplicateReusableCue(quint32 reusableId)
{
    const ReusableCue *src = reusableCue(reusableId);
    if (src == nullptr)
        return invalidId();
    ReusableCue copy = *src;
    copy.id = newId();
    freshLayers(copy.layers, [this] { return newId(); });
    QStringList taken;
    for (const ReusableCue &r : m_reusableCues)
        taken << r.name;
    copy.name = copyName(src->name, taken);
    const quint32 id = copy.id;
    const bool ok = edit(tr("Duplicate reusable cue"), [&](QList<Production> &) {
        m_reusableCues << copy;
        return true;
    });
    return ok ? id : invalidId();
}

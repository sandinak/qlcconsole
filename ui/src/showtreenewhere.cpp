/*
  Q Light Controller Plus
  showtreenewhere.cpp

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

#include <QApplication>
#include <QTreeWidget>
#include <QLineEdit>
#include <QLabel>

#include "showtreeview.h"
#include "showtreecontroller.h"
#include "treecuestack.h"
#include "workbench.h"

using namespace ShowTree;

/* ⇧⌘N, New (here): the next thing the selection calls for -- a production,
   a cue list, a cue after the selected one, a layer in the cue, a look. The
   plan is worked out once, for the Edit menu's text and for the doing. */

struct ShowTreeView::NewHerePlan
{
    enum What { Nothing, Production, CueList, Cue, Layer, PickLayer, Look, Bench };
    What what = Nothing;
    quint32 production = ShowTree::invalidId();
    quint32 list = ShowTree::invalidId();
    quint32 cue = ShowTree::invalidId();
    int after = -1;                     //!< Cue: after this index of the list
    QList<quint32> groups, fixtures, sets;
    QString where;                      //!< PickLayer: the cue, as the tree labels it
    QString label;
};

/* A name not yet taken among @a taken: @a base, else "@a base 2", "3"... */
static QString freeName(const QString &base, const QStringList &taken)
{
    if (!taken.contains(base))
        return base;
    for (int n = 2; ; ++n)
    {
        const QString name = QStringLiteral("%1 %2").arg(base).arg(n);
        if (!taken.contains(name))
            return name;
    }
}

ShowTreeView::NewHerePlan ShowTreeView::planNewHere() const
{
    NewHerePlan plan;
    if (m_st.isNull())
        return plan;
    QWidget *focus = QApplication::focusWidget();

    // Something other than a cue opened in the docked Workbench (▣ Open on
    // a collection layer): its own + Scene / + Step.
    if (Workbench *bench = dockedWorkbench())
        if (focus != nullptr && bench->isAncestorOf(focus)
            && qobject_cast<TreeCueStack *>(bench->stack()) == nullptr)
        {
            plan.label = bench->newHereLabel();
            if (!plan.label.isEmpty())
                plan.what = NewHerePlan::Bench;
            return plan;
        }

    // What the Library has selected that can be a layer's target.
    for (QTreeWidgetItem *item : m_library->selectedItems())
    {
        const int kind = item->data(0, KindRole).toInt();
        const quint32 id = item->data(0, IdRole).toUInt();
        if (kind == LibGroup && !plan.groups.contains(id))
            plan.groups << id;
        else if (kind == LibFixture && !plan.fixtures.contains(id))
            plan.fixtures << id;
        else if (kind == LibSet && !plan.sets.contains(id))
            plan.sets << id;
    }
    const bool targets = !plan.groups.isEmpty() || !plan.fixtures.isEmpty() || !plan.sets.isEmpty();
    const bool inLibrary = focus != nullptr && (focus == m_library || m_library->isAncestorOf(focus));
    const NodeKind kind = currentKind();
    const quint32 cueId = currentCueId();

    auto layerOn = [&](quint32 cue) {
        const CueList *l = m_st->listOfCue(cue);
        plan.cue = cue;
        Target t;
        t.groups = plan.groups;
        t.fixtures = plan.fixtures;
        const QString where = l != nullptr ? m_st->cueLabel(*l, l->indexOf(cue)) : QString();
        QString on = m_st->targetLabel(t);
        if (!plan.sets.isEmpty())
        {
            QStringList names;
            if (!on.isEmpty())
                names << on;
            for (quint32 s : plan.sets)
                if (const GroupSet *gs = m_st->groupSet(s))
                    names << gs->name;
            on = names.join(QStringLiteral(", "));
        }
        plan.what = NewHerePlan::Layer;
        plan.label = tr("New Layer on %1 in %2").arg(on, where);
    };

    // Groups picked in the Library: a layer on them in the selected cue.
    if (inLibrary && targets && cueId != invalidId())
    {
        layerOn(cueId);
        return plan;
    }

    if (selectionInLooks())
    {
        if (currentProductionId() != invalidId())
        {
            plan.what = NewHerePlan::Look;
            plan.label = tr("New Hit");
        }
        return plan;
    }

    switch (kind)
    {
    case LayerNode:
    case TrackedLayerNode:
        if (targets)
            layerOn(cueId);
        else if (const CueList *l = m_st->listOfCue(cueId))
        {
            plan.what = NewHerePlan::PickLayer;
            plan.cue = cueId;
            plan.where = m_st->cueLabel(*l, l->indexOf(cueId));
            plan.label = tr("New Layer in %1…").arg(plan.where);
        }
        return plan;
    case CueNode:
    {
        // A sub-cue: the next main cue after its cue, as + Cue makes it.
        quint32 cue = cueId;
        m_st->isSubCue(cue, &cue);
        const CueList *l = m_st->listOfCue(cue);
        if (l == nullptr)
            return plan;
        plan.what = NewHerePlan::Cue;
        plan.list = l->id;
        plan.after = l->indexOf(cue);
        plan.label = tr("New Cue After %1").arg(m_st->cueNumber(*l, plan.after));
        return plan;
    }
    case SectionNode:
    {
        // A song section (Q190): a cue at its end, numbered in it.
        Section section;
        const CueList *l = m_st->cueList(currentId());
        if (l == nullptr || !currentSection(&section))
            return plan;
        plan.what = NewHerePlan::Cue;
        plan.list = l->id;
        plan.after = section.last;
        plan.label = tr("New Cue After %1").arg(m_st->cueNumber(*l, plan.after));
        return plan;
    }
    case ListNode:
        if (const CueList *l = m_st->cueList(currentId()))
        {
            plan.what = NewHerePlan::Cue;
            plan.list = l->id;
            plan.after = int(l->cues.count()) - 1;
            plan.label = plan.after < 0 ? tr("New Cue in %1").arg(l->name)
                                        : tr("New Cue After %1").arg(m_st->cueNumber(*l, plan.after));
        }
        return plan;
    default:
        break;
    }

    // Nothing selected, or a production: a cue list in it -- the first
    // production of the show, made when there is none.
    quint32 prod = kind == ProductionNode ? currentId() : invalidId();
    if (prod == invalidId())
        prod = m_st->currentProduction();
    if (m_st->production(prod) == nullptr)
        prod = m_st->productions().isEmpty() ? invalidId() : m_st->productions().first().id;
    if (prod == invalidId())
    {
        plan.what = NewHerePlan::Production;
        plan.label = tr("New Production");
        return plan;
    }
    plan.what = NewHerePlan::CueList;
    plan.production = prod;
    plan.label = tr("New Cue List in %1").arg(m_st->production(prod)->name);
    return plan;
}

QString ShowTreeView::newHereLabel() const
{
    return planNewHere().label;
}

quint32 ShowTreeView::newHere()
{
    if (m_renaming)
        return invalidId();
    const NewHerePlan plan = planNewHere();
    quint32 made = invalidId();
    quint32 madeCue = invalidId();      // a layer's cue
    switch (plan.what)
    {
    case NewHerePlan::Nothing:
        return invalidId();
    case NewHerePlan::Bench:
        return dockedWorkbench()->newHere();
    case NewHerePlan::Look:
        // In its name field, as + Look leaves it; where it lives (Q154).
        return newHit();
    case NewHerePlan::Production:
    {
        QStringList taken;
        for (const Production &p : m_st->productions())
            taken << p.name;
        made = m_st->addProduction(freeName(tr("Show"), taken));
        break;
    }
    case NewHerePlan::CueList:
    {
        QStringList taken;
        for (const CueList &l : m_st->production(plan.production)->lists)
            taken << l.name;
        // Tracking, as + Cue list makes one.
        made = m_st->addCueList(plan.production, freeName(tr("Cue list"), taken), ListMode::Tracking);
        break;
    }
    case NewHerePlan::Cue:
        made = m_st->addCue(plan.list, plan.after, QString());
        break;
    case NewHerePlan::PickLayer:
        // A layer needs its fixtures: the cue selected (Return in the
        // Library makes a layer in it, not adds to the layer) and the
        // Library's filter to find them.
        selectNode(plan.cue);
        focusLibraryFilter();
        m_hint->setText(tr("New layer in %1: pick its groups in the Library, then Return").arg(plan.where));
        return invalidId();
    case NewHerePlan::Layer:
    {
        const ShowTree::Cue *before = m_st->cue(plan.cue);
        QList<quint32> had;
        if (before != nullptr)
            for (const ShowTree::Layer &l : before->layers)
                had << l.id;
        if (!dropTargets(plan.cue, plan.groups, plan.fixtures, false, plan.sets))
            return invalidId();
        if (const ShowTree::Cue *after = m_st->cue(plan.cue))
            for (const ShowTree::Layer &l : after->layers)
                if (!had.contains(l.id))
                    made = l.id;
        madeCue = plan.cue;
        break;
    }
    }
    if (made == invalidId())
        return invalidId();
    // Selected, its name in place to type (F2's editor).
    selectNode(made, madeCue);
    if (QTreeWidgetItem *item = m_tree->currentItem())
        m_tree->scrollToItem(item);
    m_tree->setFocus(Qt::ShortcutFocusReason);
    startRename();
    return made;
}

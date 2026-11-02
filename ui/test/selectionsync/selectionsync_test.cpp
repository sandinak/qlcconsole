/*
  Q Light Controller Plus - Test Unit
  selectionsync_test.cpp

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

#include <QTreeWidgetItemIterator>
#include <QApplication>
#include <QTreeWidget>
#include <QKeyEvent>
#include <QAction>
#include <QMenu>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "structurestudioview.h"
#include "fixturemanager.h"
#include "showtreeview.h"
#include "treecuestack.h"
#include "workbench.h"
#include "workbenchstack.h"
#undef protected
#undef private

#include "selectionhintchip.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "fixturetreewidget.h"
#include "selectionshape.h"
#include "selectionmenu.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "fixturegroup.h"
#include "paletteops.h"
#include "collection.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

#include "selectionsync_test.h"

using namespace ShowTree;

#define KColumnName 0

namespace
{

FixtureGroup *addGroup(Doc *doc, const QString &name, const QList<quint32> &fids)
{
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName(name);
    g->setSize(QSize(qMax(1, int(fids.size())), 1));
    doc->addFixtureGroup(g);
    for (quint32 fid : fids)
        g->assignFixture(fid);
    return g;
}

/* The texts of a menu, submenus as "Sub ▸ Item". */
QStringList menuTree(QMenu *m, const QString &prefix = QString())
{
    QStringList out;
    for (QAction *a : m->actions())
    {
        if (a->isSeparator())
            continue;
        out << prefix + a->text();
        if (a->menu() != nullptr)
            out << menuTree(a->menu(), prefix + a->text() + QStringLiteral(" ▸ "));
    }
    return out;
}

QAction *actionNamed(QMenu *m, const QString &text)
{
    for (QAction *a : m->actions())
    {
        if (a->text() == text)
            return a;
        if (a->menu() != nullptr)
            if (QAction *in = actionNamed(a->menu(), text))
                return in;
    }
    return nullptr;
}

QTreeWidgetItem *fixtureRow(FixtureManager &mgr, quint32 fid)
{
    for (QTreeWidgetItemIterator it(mgr.m_fixtures_tree); *it != nullptr; ++it)
        if ((*it)->data(KColumnName, PROP_ID).isValid() && !(*it)->data(KColumnName, PROP_HEAD).isValid()
            && !(*it)->data(KColumnName, PROP_GROUP).isValid()
            && (*it)->data(KColumnName, PROP_ID).toString().toUInt() == fid)
            return *it;
    return nullptr;
}

QTreeWidgetItem *groupRow(FixtureManager &mgr, quint32 gid)
{
    for (QTreeWidgetItemIterator it(mgr.m_fixtures_tree); *it != nullptr; ++it)
        if ((*it)->data(KColumnName, PROP_GROUP).isValid()
            && (*it)->data(KColumnName, PROP_GROUP).toUInt() == gid)
            return *it;
    return nullptr;
}

QList<quint32> sorted(QList<quint32> l)
{
    std::sort(l.begin(), l.end());
    return l;
}

}

void SelectionSync_Test::init()
{
    m_doc = new Doc(this);
    m_doc->inputOutputMap()->addUniverse();
    m_fixtures.clear();
    for (int i = 0; i < 8; ++i)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("Dim %1").arg(i + 1));
        f->setAddress(quint32(i));
        f->setUniverse(0);
        f->setChannels(1);
        QVERIFY(m_doc->addFixture(f));
        m_fixtures << f->id();
        // On the plot, one metre apart in a row.
        m_doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(1000.0f * (i + 1), 2000.0f, 0.0f));
    }
    m_doc->setMode(Doc::Design);
}

void SelectionSync_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    delete m_doc;
    m_doc = nullptr;
}

/****************************************************************************
 * One selection
 ****************************************************************************/

void SelectionSync_Test::studioPickWorkbenchFollowsHidden()
{
    Scene *s = new Scene(m_doc);
    s->setName("Look");
    QVERIFY(m_doc->addFunction(s));
    s->addFixture(m_fixtures[0]);

    // The Build tab not in front: its Workbench, never shown.
    Workbench wb(m_doc);
    wb.load(s->id());
    QVERIFY(!wb.isVisible());

    StructureStudioView studio(m_doc, StructureStudioView::StageKind, 0);
    const QList<quint32> pick{ m_fixtures[1], m_fixtures[2] };
    emit studio.fixturesSelected(pick);       // what a click / cmd-click emits

    QCOMPARE(m_doc->programmerSelection(), pick);
    QCOMPARE(wb.selectedFixtures(), pick);
    QCOMPARE(int(wb.currentPage()), int(Workbench::TargetPage));

    // Cleared anywhere: the Workbench lets go too.
    m_doc->clearProgrammerSelection();
    QVERIFY(wb.selectedFixtures().isEmpty());
}

void SelectionSync_Test::plotAndStudioSync()
{
    QWidget parent;
    Monitor mon(&parent, m_doc);
    MonitorGraphicsView *plot = mon.m_graphicsView;
    QVERIFY(plot != nullptr);
    QVERIFY(plot->m_fixtures.contains(m_fixtures[0]));
    StructureStudioView studio(m_doc, StructureStudioView::StageKind, 0);

    // A pick on the plot: the programmer's, ringed in the Studio.
    plot->m_scene->clearSelection();
    plot->m_fixtures.value(m_fixtures[0])->setSelected(true);
    mon.slotViewClicked();
    QCOMPARE(m_doc->programmerSelection(), QList<quint32>{ m_fixtures[0] });
    QCOMPARE(studio.highlighted(), QList<quint32>{ m_fixtures[0] });
    QVERIFY(plot->m_fixtures.value(m_fixtures[0])->isHighlighted());

    // A pick in the Studio: ringed on the plot, the old one let go.
    emit studio.fixturesSelected({ m_fixtures[3], m_fixtures[4] });
    QCOMPARE(m_doc->programmerSelection(), QList<quint32>({ m_fixtures[3], m_fixtures[4] }));
    QVERIFY(plot->m_fixtures.value(m_fixtures[3])->isHighlighted());
    QVERIFY(plot->m_fixtures.value(m_fixtures[4])->isHighlighted());
    QVERIFY(!plot->m_fixtures.value(m_fixtures[0])->isHighlighted());
    QCOMPARE(sorted(studio.highlighted()), sorted({ m_fixtures[3], m_fixtures[4] }));

    // Another Studio window opens on it as well.
    StructureStudioView overview(m_doc, StructureStudioView::StageKind, 0);
    QCOMPARE(sorted(overview.highlighted()), sorted({ m_fixtures[3], m_fixtures[4] }));
}

void SelectionSync_Test::structureEditorRingsItsOwn()
{
    // No structure 99: nothing is mounted on it, so nothing rings there.
    StructureStudioView editor(m_doc, StructureStudioView::TrussKind, 99);
    m_doc->setProgrammerSelection({ m_fixtures[0], m_fixtures[1] });
    QVERIFY(editor.highlighted().isEmpty());
}

void SelectionSync_Test::fixtureManagerPicks()
{
    FixtureGroup *g = addGroup(m_doc, "Front Wash", { m_fixtures[0], m_fixtures[1], m_fixtures[2] });
    FixtureManager mgr(nullptr, m_doc);
    mgr.resize(900, 600);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));

    QTreeWidgetItem *row = fixtureRow(mgr, m_fixtures[5]);
    QVERIFY(row != nullptr);
    mgr.m_fixtures_tree->clearSelection();
    row->setSelected(true);
    QCOMPARE(m_doc->programmerSelection(), QList<quint32>{ m_fixtures[5] });

    // A group row: its fixtures (decided, can change).
    QTreeWidgetItem *grow = groupRow(mgr, g->id());
    QVERIFY(grow != nullptr);
    mgr.m_fixtures_tree->clearSelection();
    QCOMPARE(m_doc->programmerSelection(), QList<quint32>{ m_fixtures[5] });   // a deselect: no pick
    grow->setSelected(true);
    QCOMPARE(sorted(m_doc->programmerSelection()), sorted(g->fixtureList()));

    // Hidden: the tree's own business (a rebuild there is no pick).
    mgr.hide();
    mgr.m_fixtures_tree->clearSelection();
    fixtureRow(mgr, m_fixtures[6])->setSelected(true);
    QCOMPARE(sorted(m_doc->programmerSelection()), sorted(g->fixtureList()));
}

void SelectionSync_Test::noSignalLoop()
{
    Scene *s = new Scene(m_doc);
    QVERIFY(m_doc->addFunction(s));
    s->addFixture(m_fixtures[0]);
    QWidget parent;
    Monitor mon(&parent, m_doc);
    Workbench wb(m_doc);
    wb.load(s->id());
    StructureStudioView studio(m_doc, StructureStudioView::StageKind, 0);

    QSignalSpy spy(m_doc, &Doc::programmerSelectionChanged);
    emit studio.fixturesSelected({ m_fixtures[1] });
    QCOMPARE(spy.count(), 1);
    wb.selectFixtures({ m_fixtures[2] });
    QCOMPARE(spy.count(), 2);
    mon.pickFixtures({ m_fixtures[3] });
    QCOMPARE(spy.count(), 3);
    m_doc->setProgrammerSelection({ m_fixtures[3] });        // the same: nothing
    QCOMPARE(spy.count(), 3);
    QCOMPARE(wb.selectedFixtures(), QList<quint32>{ m_fixtures[3] });
}

/****************************************************************************
 * Selection ▸ New Layer from Selection / Add to Current Layer
 ****************************************************************************/

void SelectionSync_Test::newLayerFunctionStack()
{
    Scene *base = new Scene(m_doc);
    base->setName("Base");
    QVERIFY(m_doc->addFunction(base));
    Collection *c = new Collection(m_doc);
    c->setName("Look");
    QVERIFY(m_doc->addFunction(c));
    QVERIFY(c->addFunction(base->id()));
    const quint32 cid = c->id();
    const quint32 baseId = base->id();

    Workbench wb(m_doc);
    wb.load(cid);
    const QList<quint32> pick{ m_fixtures[4], m_fixtures[5] };
    QVERIFY2(wb.newLayerBlocker(pick).isEmpty(), qPrintable(wb.newLayerBlocker(pick)));
    QVERIFY(wb.newLayerFromFixtures(pick));

    Collection *col = qobject_cast<Collection *>(m_doc->function(cid));
    QCOMPARE(col->functions().size(), 2);
    const quint32 made = col->functions().last();
    Scene *n = qobject_cast<Scene *>(m_doc->function(made));
    QVERIFY(n != nullptr);
    QCOMPARE(n->fixtures(), pick);              // not one group: discrete
    QVERIFY(n->fixtureGroups().isEmpty());
    QCOMPARE(wb.currentSceneId(), made);        // selected, ready to edit
    QCOMPARE(m_doc->programmerSelection(), pick);

    // One undo takes it all back.
    QVERIFY(m_doc->paletteOps()->undo());
    col = qobject_cast<Collection *>(m_doc->function(cid));
    QCOMPARE(col->functions().size(), 1);
    QVERIFY(m_doc->function(made) == nullptr);

    // Whole groups: a layer each, on the group (it tracks it). One undo.
    FixtureGroup *sr = addGroup(m_doc, "SR Tower", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *sl = addGroup(m_doc, "SL Tower", { m_fixtures[2], m_fixtures[3] });
    QVERIFY(wb.newLayerFromFixtures({ m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[3], m_fixtures[7] }));
    col = qobject_cast<Collection *>(m_doc->function(cid));
    QCOMPARE(col->functions().size(), 4);
    Scene *first = qobject_cast<Scene *>(m_doc->function(col->functions().at(1)));
    Scene *second = qobject_cast<Scene *>(m_doc->function(col->functions().at(2)));
    Scene *rest = qobject_cast<Scene *>(m_doc->function(col->functions().at(3)));
    QVERIFY(first && second && rest);
    QCOMPARE(first->fixtureGroups(), QList<quint32>{ sl->id() });      // name order on a tie
    QVERIFY(first->fixtures().isEmpty());
    QCOMPARE(second->fixtureGroups(), QList<quint32>{ sr->id() });
    QCOMPARE(rest->fixtures(), QList<quint32>{ m_fixtures[7] });
    QVERIFY(m_doc->paletteOps()->undo());
    QCOMPARE(qobject_cast<Collection *>(m_doc->function(cid))->functions().size(), 1);

    // A scene alone is one layer: no new layers there.
    Workbench alone(m_doc);
    alone.load(baseId);
    QVERIFY(!alone.newLayerBlocker(pick).isEmpty());
    QVERIFY(!alone.newLayerFromFixtures(pick));
}

void SelectionSync_Test::addToLayerFunctionStack()
{
    Scene *s = new Scene(m_doc);
    s->setName("Look");
    QVERIFY(m_doc->addFunction(s));
    s->addFixture(m_fixtures[0]);
    const quint32 sid = s->id();
    FixtureGroup *g = addGroup(m_doc, "Front Wash", { m_fixtures[4], m_fixtures[5] });

    Workbench wb(m_doc);
    wb.load(sid);
    QCOMPARE(wb.currentLayer(), 0);
    QVERIFY2(wb.addToLayerBlocker({ m_fixtures[2] }).isEmpty(), qPrintable(wb.addToLayerBlocker({ m_fixtures[2] })));
    QVERIFY(wb.addFixturesToCurrentLayer({ m_fixtures[2], m_fixtures[4], m_fixtures[5] }));
    Scene *after = qobject_cast<Scene *>(m_doc->function(sid));
    QCOMPARE(after->fixtures(), QList<quint32>({ m_fixtures[0], m_fixtures[2] }));
    QCOMPARE(after->fixtureGroups(), QList<quint32>{ g->id() });

    QVERIFY(m_doc->paletteOps()->undo());
    after = qobject_cast<Scene *>(m_doc->function(sid));
    QCOMPARE(after->fixtures(), QList<quint32>{ m_fixtures[0] });
    QVERIFY(after->fixtureGroups().isEmpty());

    // Nothing loaded: says why.
    Workbench empty(m_doc);
    QVERIFY(!empty.addToLayerBlocker({ m_fixtures[2] }).isEmpty());
}

void SelectionSync_Test::newLayerTreeCueStack()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 cue = st->addCue(list, -1, "One");
    st->clearUndo();
    FixtureGroup *sr = addGroup(m_doc, "SR Tower", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *sl = addGroup(m_doc, "SL Tower", { m_fixtures[2], m_fixtures[3] });

    ShowTreeView view(nullptr, m_doc);
    view.resize(1400, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.openInWorkbench(cue));
    Workbench *bench = view.dockedWorkbench();
    QVERIFY(bench != nullptr && bench->isVisible());
    QVERIFY(qobject_cast<TreeCueStack *>(bench->stack()) != nullptr);
    QCOMPARE(SelectionMenu::currentWorkbench(), bench);

    // Discrete fixtures: one layer of the cue's own on them.
    QVERIFY(bench->newLayerFromFixtures({ m_fixtures[5], m_fixtures[6] }));
    QCOMPARE(st->cue(cue)->layers.size(), 1);
    QCOMPARE(sorted(st->cue(cue)->layers.first().target.fixtures), sorted({ m_fixtures[5], m_fixtures[6] }));
    QVERIFY(st->undo());
    QCOMPARE(st->cue(cue)->layers.size(), 0);

    // Two whole groups and a stray: a layer each, one undo step.
    QVERIFY(bench->newLayerFromFixtures({ m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[3], m_fixtures[7] }));
    QCOMPARE(st->cue(cue)->layers.size(), 3);
    QList<quint32> groups;
    for (const Layer &l : st->cue(cue)->layers)
        groups << l.target.groups;
    QCOMPARE(sorted(groups), sorted({ sr->id(), sl->id() }));
    QVERIFY(st->undo());
    QCOMPARE(st->cue(cue)->layers.size(), 0);

    // Add to Current Layer: into the layer selected.
    QVERIFY(bench->newLayerFromFixtures({ m_fixtures[5] }));
    QCOMPARE(st->cue(cue)->layers.size(), 1);
    QVERIFY2(bench->addToLayerBlocker({ m_fixtures[6] }).isEmpty(),
             qPrintable(bench->addToLayerBlocker({ m_fixtures[6] })));
    QVERIFY(bench->addFixturesToCurrentLayer({ m_fixtures[6] }));
    QCOMPARE(sorted(st->cue(cue)->layers.first().target.fixtures), sorted({ m_fixtures[5], m_fixtures[6] }));
    QVERIFY(st->undo());
    QCOMPARE(st->cue(cue)->layers.first().target.fixtures, QList<quint32>{ m_fixtures[5] });
}

/****************************************************************************
 * The menu
 ****************************************************************************/

void SelectionSync_Test::selectionMenu()
{
    Collection *c = new Collection(m_doc);
    QVERIFY(m_doc->addFunction(c));
    const quint32 cid = c->id();
    addGroup(m_doc, "Front Wash", { m_fixtures[0], m_fixtures[1] });
    Workbench wb(m_doc);
    wb.load(cid);

    QMenu menu;
    QAction *extra = new QAction("Create Group from Selection…", &menu);
    QMenu *sub = SelectionMenu::addTo(&menu, m_doc, { m_fixtures[0], m_fixtures[1] }, { extra }, &wb);
    QVERIFY(sub != nullptr);
    QCOMPARE(sub->title(), QString("Selection"));
    const QStringList texts = menuTree(&menu);
    QVERIFY2(texts.contains("Selection ▸ Front Wash (2)"), qPrintable(texts.join(" | ")));
    QVERIFY2(texts.contains("Selection ▸ New Layer from Selection"), qPrintable(texts.join(" | ")));
    QVERIFY2(texts.contains("Selection ▸ Add to Current Layer"), qPrintable(texts.join(" | ")));
    QVERIFY2(texts.contains("Selection ▸ Create Group from Selection…"), qPrintable(texts.join(" | ")));
    QCOMPARE(menu.actions().size(), 1);         // one entry on the menu itself

    QAction *add = actionNamed(sub, SelectionMenu::addToLayerText());
    QVERIFY(!add->isEnabled());                 // an empty collection: no layer
    QVERIFY(!add->toolTip().isEmpty());
    QAction *newLayer = actionNamed(sub, SelectionMenu::newLayerText());
    QVERIFY(newLayer->isEnabled());
    newLayer->trigger();
    Collection *col = qobject_cast<Collection *>(m_doc->function(cid));
    QCOMPARE(col->functions().size(), 1);
    Scene *made = qobject_cast<Scene *>(m_doc->function(col->functions().first()));
    QCOMPARE(made->fixtureGroups().size(), 1);  // the group, not its fixtures

    // No Workbench anywhere: disabled, saying why.
    QMenu none;
    QMenu *nsub = SelectionMenu::addTo(&none, m_doc, { m_fixtures[3] });
    QAction *nl = actionNamed(nsub, SelectionMenu::newLayerText());
    QVERIFY(!nl->isEnabled());
    QVERIFY(!nl->toolTip().isEmpty());
}

void SelectionSync_Test::plotMenu()
{
    QWidget parent;
    Monitor mon(&parent, m_doc);
    MonitorGraphicsView *plot = mon.m_graphicsView;
    MonitorGraphicsView::MenuPlace place;
    place.selection << qMakePair(QStringLiteral("fixture"), m_fixtures[0])
                    << qMakePair(QStringLiteral("fixture"), m_fixtures[1]);
    QMenu menu;
    plot->buildItemMenu(&menu, QStringLiteral("fixture"), m_fixtures[0], place, QPointF());
    const QStringList texts = menuTree(&menu);
    QVERIFY2(texts.contains("Selection ▸ New Layer from Selection"), qPrintable(texts.join(" | ")));
    QVERIFY2(texts.contains("Selection ▸ Create Fixture Group from Selection…"), qPrintable(texts.join(" | ")));
    QVERIFY2(!texts.contains("Create Fixture Group from Selection…"), qPrintable(texts.join(" | ")));
    QVERIFY2(texts.contains("New Map Layer from Selection…"), qPrintable(texts.join(" | ")));
    QVERIFY2(!texts.contains("New Layer from Selection…"), qPrintable(texts.join(" | ")));
}

/****************************************************************************
 * Groups recognised
 ****************************************************************************/

void SelectionSync_Test::exactGroup()
{
    FixtureGroup *fw = addGroup(m_doc, "Front Wash", { m_fixtures[0], m_fixtures[1], m_fixtures[2] });
    const SelectionShape shape = SelectionShape::resolve(m_doc, { m_fixtures[2], m_fixtures[0], m_fixtures[1] });
    QCOMPARE(shape.groups, QList<quint32>{ fw->id() });
    QVERIFY(shape.fixtures.isEmpty());
    QCOMPARE(SelectionShape::label(m_doc, { m_fixtures[0], m_fixtures[1], m_fixtures[2] }),
             QString("Front Wash (3)"));

    // A partial match stays discrete.
    const SelectionShape part = SelectionShape::resolve(m_doc, { m_fixtures[0], m_fixtures[1] });
    QVERIFY(part.groups.isEmpty());
    QCOMPARE(SelectionShape::label(m_doc, { m_fixtures[0], m_fixtures[1] }), QString("2 fixtures"));
}

void SelectionSync_Test::twoGroupsAndStray()
{
    FixtureGroup *sr = addGroup(m_doc, "SR Tower", { m_fixtures[0], m_fixtures[1], m_fixtures[2] });
    FixtureGroup *sl = addGroup(m_doc, "SL Tower", { m_fixtures[3], m_fixtures[4], m_fixtures[5] });
    const QList<quint32> both{ m_fixtures[0], m_fixtures[1], m_fixtures[2],
                               m_fixtures[3], m_fixtures[4], m_fixtures[5] };
    SelectionShape shape = SelectionShape::resolve(m_doc, both);
    QCOMPARE(shape.groups, QList<quint32>({ sl->id(), sr->id() }));   // a tie: name order
    QVERIFY(shape.fixtures.isEmpty());
    QCOMPARE(SelectionShape::label(m_doc, both), QString("SL Tower + SR Tower (6)"));

    // A group and a stray: the stray stays a fixture beside it.
    const QList<quint32> withStray{ m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[7] };
    shape = SelectionShape::resolve(m_doc, withStray);
    QCOMPARE(shape.groups, QList<quint32>{ sr->id() });
    QCOMPARE(shape.fixtures, QList<quint32>{ m_fixtures[7] });
    QCOMPARE(SelectionShape::label(m_doc, withStray), QString("SR Tower + 1 fixture"));

    // A parent equal to both: the parent.
    FixtureGroup *towers = addGroup(m_doc, "Towers", both);
    shape = SelectionShape::resolve(m_doc, both);
    QCOMPARE(shape.groups, QList<quint32>{ towers->id() });
    QList<quint32> plusStray = both;
    plusStray << m_fixtures[7];
    shape = SelectionShape::resolve(m_doc, plusStray);
    QCOMPARE(shape.groups, QList<quint32>{ towers->id() });
    QCOMPARE(shape.fixtures, QList<quint32>{ m_fixtures[7] });
}

void SelectionSync_Test::mostUsedWins()
{
    FixtureGroup *a = addGroup(m_doc, "A Wash", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *b = addGroup(m_doc, "B Wash", { m_fixtures[0], m_fixtures[1] });
    QCOMPARE(SelectionShape::resolve(m_doc, { m_fixtures[0], m_fixtures[1] }).groups, QList<quint32>{ a->id() });
    // B used in a scene: B.
    Scene *s = new Scene(m_doc);
    QVERIFY(m_doc->addFunction(s));
    s->addFixtureGroup(b->id());
    QCOMPARE(SelectionShape::groupUsage(m_doc, b->id()), 1);
    QCOMPARE(SelectionShape::resolve(m_doc, { m_fixtures[0], m_fixtures[1] }).groups, QList<quint32>{ b->id() });
}

/****************************************************************************
 * Near misses
 ****************************************************************************/

void SelectionSync_Test::nearMissHints()
{
    addGroup(m_doc, "Front Wash", { m_fixtures[0], m_fixtures[1], m_fixtures[2],
                                    m_fixtures[3], m_fixtures[4], m_fixtures[5] });
    const QList<quint32> five{ m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[3], m_fixtures[4] };

    // One missing: select all 6?
    SelectionShape::Hint h = SelectionShape::hint(m_doc, five);
    QCOMPARE(int(h.kind), int(SelectionShape::Hint::AddMissing));
    QCOMPARE(h.fixtures, QList<quint32>{ m_fixtures[5] });
    QCOMPARE(h.text, QString("Front Wash is 6 — select all 6?"));
    QCOMPARE(h.result.size(), 6);

    // Three of six: no offer from a click...
    const QList<quint32> three{ m_fixtures[0], m_fixtures[1], m_fixtures[2] };
    QVERIFY(SelectionShape::hint(m_doc, three).isNull());
    // ...but a band that just missed the fourth (within a third of the spacing).
    QHash<quint32, QPointF> pos;
    for (int i = 0; i < 6; ++i)
        pos.insert(m_fixtures[i], QPointF(i * 1.0, 0.0));
    h = SelectionShape::hint(m_doc, three, pos, QRectF(QPointF(-0.5, -0.5), QPointF(2.75, 0.5)));
    QCOMPARE(int(h.kind), int(SelectionShape::Hint::AddMissing));
    QVERIFY(h.nearBand);
    QCOMPARE(h.fixtures, QList<quint32>{ m_fixtures[3] });
    // A band well short of it: nothing.
    QVERIFY(SelectionShape::hint(m_doc, three, pos, QRectF(QPointF(-0.5, -0.5), QPointF(2.4, 0.5))).isNull());

    // The whole group and one stray: drop it?
    QList<quint32> stray{ m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[3], m_fixtures[4],
                          m_fixtures[5], m_fixtures[7] };
    h = SelectionShape::hint(m_doc, stray);
    QCOMPARE(int(h.kind), int(SelectionShape::Hint::DropStray));
    QCOMPARE(h.fixtures, QList<quint32>{ m_fixtures[7] });
    QCOMPARE(h.result.size(), 6);

    // Exactly the group, or one fixture: nothing.
    stray.removeLast();
    QVERIFY(SelectionShape::hint(m_doc, stray).isNull());
    QVERIFY(SelectionShape::hint(m_doc, { m_fixtures[0] }).isNull());
}

void SelectionSync_Test::hintChip()
{
    addGroup(m_doc, "Front Wash", { m_fixtures[0], m_fixtures[1], m_fixtures[2] });
    QWidget host;
    host.resize(400, 300);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    SelectionHintChip chip(m_doc, &host);

    const QList<quint32> two{ m_fixtures[0], m_fixtures[1] };
    m_doc->setProgrammerSelection(two);
    chip.offer(SelectionShape::hint(m_doc, two), QPoint(20, 20));
    QVERIFY(chip.isOffering());
    QCOMPARE(chip.text(), QString("Front Wash is 3 — select all 3?"));
    QCOMPARE(m_doc->programmerSelection(), two);            // offered, not applied

    // Esc in the view: gone, the selection as it was.
    QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&host, &esc);
    QVERIFY(!chip.isOffering());
    QCOMPARE(m_doc->programmerSelection(), two);

    // Tab takes it.
    chip.offer(SelectionShape::hint(m_doc, two), QPoint(20, 20));
    QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
    QApplication::sendEvent(&host, &tab);
    QVERIFY(!chip.isOffering());
    QCOMPARE(sorted(m_doc->programmerSelection()), sorted({ m_fixtures[0], m_fixtures[1], m_fixtures[2] }));

    // Another pick meanwhile: the offer goes, nothing changes.
    m_doc->setProgrammerSelection(two);
    chip.offer(SelectionShape::hint(m_doc, two));
    m_doc->setProgrammerSelection({ m_fixtures[6] });
    QVERIFY(!chip.isOffering());
    QCOMPARE(m_doc->programmerSelection(), QList<quint32>{ m_fixtures[6] });

    // A click on it takes it.
    m_doc->setProgrammerSelection(two);
    chip.offer(SelectionShape::hint(m_doc, two));
    QTest::mouseClick(&chip, Qt::LeftButton, Qt::NoModifier, QPoint(4, 4));
    QVERIFY(!chip.isOffering());
    QCOMPARE(m_doc->programmerSelection().size(), 3);
}

void SelectionSync_Test::plotBandOutlinesMiss()
{
    addGroup(m_doc, "Front Wash", { m_fixtures[0], m_fixtures[1], m_fixtures[2],
                                    m_fixtures[3], m_fixtures[4], m_fixtures[5] });
    QWidget parent;
    Monitor mon(&parent, m_doc);
    MonitorGraphicsView *plot = mon.m_graphicsView;
    const QHash<quint32, QPointF> centres = plot->fixtureCentres();
    QVERIFY(centres.contains(m_fixtures[3]));
    const double spacing = centres.value(m_fixtures[1]).x() - centres.value(m_fixtures[0]).x();
    QVERIFY(spacing > 0.0);

    // A band round the first three, its edge just short of the fourth.
    const QPointF a = centres.value(m_fixtures[0]);
    const QPointF d = centres.value(m_fixtures[3]);
    plot->m_lastBand = QRectF(QPointF(a.x() - spacing / 2, a.y() - spacing / 2),
                              QPointF(d.x() - spacing * 0.25, a.y() + spacing / 2));
    plot->m_scene->clearSelection();
    for (int i = 0; i < 3; ++i)
        plot->m_fixtures.value(m_fixtures[i])->setSelected(true);
    mon.slotViewClicked();
    QCOMPARE(m_doc->programmerSelection().size(), 3);           // never changed by itself
    QVERIFY(mon.selectionHint() != nullptr && mon.selectionHint()->isOffering());
    QVERIFY(mon.selectionHint()->hint().nearBand);
    QVERIFY(plot->m_fixtures.value(m_fixtures[3])->isHinted());   // the miss, outlined
    QVERIFY(!plot->m_fixtures.value(m_fixtures[4])->isHinted());

    mon.selectionHint()->dismiss();
    QVERIFY(!plot->m_fixtures.value(m_fixtures[3])->isHinted());
    QCOMPARE(m_doc->programmerSelection().size(), 3);
}

QTEST_MAIN(SelectionSync_Test)

/*
  Q Light Controller Plus - Test Unit
  showtreeworkbench_test.cpp

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
#include <QStackedWidget>
#include <QDataStream>
#include <QTabWidget>
#include <QToolButton>
#include <QPushButton>
#include <QTreeWidget>
#include <QMimeData>
#include <QSplitter>
#include <QLineEdit>
#include <QLabel>
#include <QDir>
#include <QtTest>

#define private public
#define protected public
#include "app.h"
#include "programmingmanager.h"
#include "programmingoutline.h"
#include "showtreeview.h"
#include "treecuestack.h"
#include "workbench.h"
#include "workbenchstack.h"
#undef protected
#undef private

#include "functionstreewidget.h"
#include "showtreecontroller.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "fixturegroup.h"
#include "mastertimer.h"
#include "lookeditor.h"
#include <QColorDialog>
#include <QApplication>
#include "qlcpalette.h"
#include "universe.h"
#include "fixture.h"
#include "chaserstep.h"
#include "chaser.h"
#include "collection.h"
#include "scene.h"
#include "efx.h"
#include "doc.h"

#include "showtreeworkbench_test.h"

class ImportSheet;
using namespace ShowTree;

namespace
{

/* The DMX the universe would send, by stepping the timer by hand (no output
   thread, nothing goes out). */
QList<int> settle(Doc *doc, int ticks = 12)
{
    for (int i = 0; i < ticks; ++i)
    {
        doc->masterTimer()->timerTick();
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        doc->inputOutputMap()->releaseUniverses(false);
    }
    QList<int> out;
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    for (int i = 0; i < 6; ++i)
        out << int(uchar(ua.first()->preGMValues().at(i)));
    doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

QMimeData *paletteMime(quint32 pid)
{
    QMimeData *mime = new QMimeData;
    QByteArray data;
    QDataStream s(&data, QIODevice::WriteOnly);
    s << pid;
    mime->setData(FunctionsTreeWidget::paletteDragMimeType(), data);
    return mime;
}

int layerNamed(WorkbenchStack *stack, const QString &name)
{
    const QList<WorkbenchStack::Layer> ls = stack->layers();
    for (int i = 0; i < ls.size(); ++i)
        if (ls.at(i).label.trimmed() == name)
            return i;
    return -1;
}

int attributeKeyed(WorkbenchStack *stack, int layer, const QString &key)
{
    const QList<WorkbenchStack::Layer> ls = stack->layers();
    if (layer < 0 || layer >= ls.size())
        return -1;
    for (int i = 0; i < ls.at(layer).attributes.size(); ++i)
        if (ls.at(layer).attributes.at(i).key == key)
            return i;
    return -1;
}

ProgrammingOutline::Row row(int kind, quint32 scene, quint32 id)
{
    ProgrammingOutline::Row r;
    r.kind = kind;
    r.scene = scene;
    r.id = id;
    return r;
}

/* The show every slot starts from: Towers (fixtures 1-2), USTs (3-4);
   list "L", Tracking; cue 1 One: Towers Full, USTs Half; cue 2 Two:
   Towers Low -- USTs tracks in from One. */
struct Show
{
    quint32 list, c1, c2, towersLayer1, ustsLayer1, towersLayer2;
    quint32 full, half, low;
    FixtureGroup *towers, *usts;
};

Show makeShow(Doc *doc, const QList<quint32> &fixtures,
              const std::function<quint32(int, const QString &)> &dimmer)
{
    Show s;
    s.towers = new FixtureGroup(doc);
    s.towers->setName("Towers");
    doc->addFixtureGroup(s.towers);
    s.towers->assignFixture(fixtures[0]);
    s.towers->assignFixture(fixtures[1]);
    s.usts = new FixtureGroup(doc);
    s.usts->setName("USTs");
    doc->addFixtureGroup(s.usts);
    s.usts->assignFixture(fixtures[2]);
    s.usts->assignFixture(fixtures[3]);
    s.full = dimmer(255, "Full");
    s.half = dimmer(128, "Half");
    s.low = dimmer(51, "Low");

    ShowTreeController *st = doc->showTree();
    s.list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    s.c1 = st->addCue(s.list, -1, "One");
    s.c2 = st->addCue(s.list, 0, "Two");
    Target tt, ut;
    tt.groups << s.towers->id();
    ut.groups << s.usts->id();
    s.towersLayer1 = st->addLayer(s.c1, tt);
    s.ustsLayer1 = st->addLayer(s.c1, ut);
    st->setLayerLook(s.c1, s.towersLayer1, st->lookForPalette(s.full));
    st->setLayerLook(s.c1, s.ustsLayer1, st->lookForPalette(s.half));
    s.towersLayer2 = st->addLayer(s.c2, tt);
    st->setLayerLook(s.c2, s.towersLayer2, st->lookForPalette(s.low));
    st->clearUndo();
    return s;
}

QSplitter *pageSplitter(ShowTreeView *view)
{
    return view->findChild<QSplitter *>("stVSplitter");
}

}

void ShowTreeWorkbench_Test::init()
{
    m_doc = new Doc(this);
    m_fixtures.clear();
    for (int i = 0; i < 6; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dim %1").arg(i + 1));
        fxi->setAddress(quint32(i));
        fxi->setUniverse(0);
        fxi->setChannels(1);
        m_doc->addFixture(fxi);
        m_fixtures << fxi->id();
    }
    m_doc->setMode(Doc::Design);
}

void ShowTreeWorkbench_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    delete m_doc;
    m_doc = nullptr;
}

quint32 ShowTreeWorkbench_Test::dimmer(int level, const QString &name)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
    p->setName(name);
    p->setValue(level);
    m_doc->addPalette(p);
    return p->id();
}

#define SHOW() makeShow(m_doc, m_fixtures, [this](int l, const QString &n) { return dimmer(l, n); })

void ShowTreeWorkbench_Test::openShowsTheCue()
{
    const Show s = SHOW();
    ShowTreeView view(nullptr, m_doc);
    view.resize(1400, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(!view.isWorkbenchDocked());

    QVERIFY(view.openInWorkbench(s.c2));
    QVERIFY(view.isWorkbenchDocked());
    Workbench *bench = view.dockedWorkbench();
    QVERIFY(bench != nullptr && bench->isVisible());
    QVERIFY(!pageSplitter(&view)->isVisible());          // in place of the page
    TreeCueStack *stack = qobject_cast<TreeCueStack *>(bench->stack());
    QVERIFY(stack != nullptr);
    QCOMPARE(stack->cueId(), s.c2);
    QCOMPARE(stack->title(), QString("2 Two"));
    QCOMPARE(stack->kindText(), QString("Cue"));
    QCOMPARE(bench->m_title->text(), QString("2 Two"));
    QVERIFY2(bench->m_crumb->text().contains("P › L"), qPrintable(bench->m_crumb->text()));
    QVERIFY2(bench->m_info->text().contains("1 own layer(s) · 1 carried in"), qPrintable(bench->m_info->text()));
    QVERIFY(bench->m_backBtn->isVisible());
    QCOMPARE(bench->m_backBtn->text(), QString("↩ Show Tree"));

    // Its own layer, then -- grey -- the one tracking in, each with its
    // value and where that comes from.
    const int towers = layerNamed(stack, "Towers");
    const int usts = layerNamed(stack, "USTs");
    QVERIFY(towers >= 0 && usts >= 0);
    const QList<WorkbenchStack::Layer> ls = stack->layers();
    QCOMPARE(ls.at(towers).kind, WorkbenchStack::Layer::CueLayer);
    QVERIFY(!ls.at(towers).tracked);
    QCOMPARE(ls.at(towers).layerId, s.towersLayer2);
    QCOMPARE(ls.at(towers).attributes.size(), 1);
    QCOMPARE(ls.at(towers).attributes.first().key, KeyDimmer);
    QCOMPARE(ls.at(towers).attributes.first().value, QString("Low"));
    QVERIFY(!ls.at(towers).attributes.first().inherited);
    QVERIFY(ls.at(usts).tracked);
    QVERIFY2(ls.at(usts).badge.contains("tracked from 1 One"), qPrintable(ls.at(usts).badge));
    QCOMPARE(ls.at(usts).attributes.first().value, QString("Half"));
    QVERIFY(ls.at(usts).attributes.first().inherited);
    QVERIFY2(ls.at(usts).attributes.first().origin.contains("1 One"),
             qPrintable(ls.at(usts).attributes.first().origin));

    // The rows: a layer row each, its attributes under it; carried in: grey.
    ProgrammingOutline *rows = bench->rows();
    QTreeWidgetItem *towersRow = rows->layerItem(towers);
    QTreeWidgetItem *ustsRow = rows->layerItem(usts);
    QVERIFY(towersRow != nullptr && ustsRow != nullptr);
    QCOMPARE(towersRow->data(0, ProgrammingOutline::KindRole).toInt(), int(ProgrammingOutline::CueLayerRow));
    QCOMPARE(towersRow->childCount(), 1);
    QCOMPARE(towersRow->child(0)->text(0), QString("Dimmer  Low"));
    QVERIFY(towersRow->isExpanded());
    QVERIFY(!towersRow->font(0).italic());
    QVERIFY(ustsRow->font(0).italic());
    QVERIFY(ustsRow->child(0)->font(0).italic());
    QVERIFY(ustsRow->child(0)->text(1).contains("1 One"));

    // In Design the cue plays in context: what tracks in too.
    ShowTreeController *st = m_doc->showTree();
    QCOMPARE(st->previewedCue(), s.c2);
    QVERIFY(bench->m_ownPreview);
    QVERIFY2(bench->m_onRig->text().startsWith("▶ On the rig: 2 Two"), qPrintable(bench->m_onRig->text()));
    QCOMPARE(settle(m_doc).mid(0, 4), QList<int>({ 51, 51, 128, 128 }));

    // The 2D view's highlight and the programmer's selection: the layer's.
    QVERIFY(bench->selectRow(row(ProgrammingOutline::CueLayerRow, 0xFFFFFFFF, quint32(towers))));
    QCOMPARE(bench->selectedFixtures(), QList<quint32>({ m_fixtures[0], m_fixtures[1] }));
    QCOMPARE(stack->layerOfFixture(m_fixtures[2]), usts);
    QCOMPARE(stack->layerOfFixture(m_fixtures[5]), -1);
}

void ShowTreeWorkbench_Test::editUndoRedo()
{
    const Show s = SHOW();
    ShowTreeView view(nullptr, m_doc);
    view.resize(1400, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.openInWorkbench(s.c2, s.towersLayer2));
    Workbench *bench = view.dockedWorkbench();
    WorkbenchStack *stack = bench->stack();
    const int towers = layerNamed(stack, "Towers");
    QCOMPARE(settle(m_doc).at(0), 51);

    // Half dropped on Towers (the drop the tree takes): the output at once.
    QScopedPointer<QMimeData> half(paletteMime(s.half));
    QVERIFY2(stack->dropPlanOn(towers, half.data(), false).contains("Half"),
             qPrintable(stack->dropPlanOn(towers, half.data(), false)));
    QVERIFY(stack->dropOn(towers, half.data(), false));
    ShowTreeController *st = m_doc->showTree();
    QCOMPARE(st->cue(s.c2)->layer(s.towersLayer2)->look(KeyDimmer)->palettes, QList<quint32>({ s.half }));
    QCOMPARE(settle(m_doc).mid(0, 4), QList<int>({ 128, 128, 128, 128 }));
    QTRY_COMPARE(bench->rows()->layerItem(towers)->child(0)->text(0), QString("Dimmer  Half"));

    // One tree step: undone, Low again; redone, Half.
    QVERIFY(st->canUndo());
    QVERIFY(st->undo());
    QCOMPARE(settle(m_doc).at(0), 51);
    QTRY_COMPARE(bench->rows()->layerItem(towers)->child(0)->text(0), QString("Dimmer  Low"));
    QVERIFY(st->redo());
    QCOMPARE(settle(m_doc).at(0), 128);
    QTRY_COMPARE(bench->rows()->layerItem(towers)->child(0)->text(0), QString("Dimmer  Half"));

    // ⌫ on the value: cleared, it tracks in from One again (Full).
    const int dim = attributeKeyed(stack, towers, KeyDimmer);
    QVERIFY(bench->selectRow(row(ProgrammingOutline::AttributeRow, quint32(towers), quint32(dim))));
    QTest::keyClick(bench->rows(), Qt::Key_Backspace);
    QVERIFY(st->cue(s.c2)->layer(s.towersLayer2)->look(KeyDimmer) == nullptr);
    QCOMPARE(settle(m_doc).at(0), 255);
    QTRY_VERIFY(bench->rows()->layerItem(towers) != nullptr
                && bench->rows()->layerItem(towers)->childCount() == 1
                && bench->rows()->layerItem(towers)->child(0)->text(0) == QString("Dimmer  Full")
                && bench->rows()->layerItem(towers)->child(0)->font(0).italic());
    QVERIFY(st->undo());
    QCOMPARE(settle(m_doc).at(0), 128);
}

void ShowTreeWorkbench_Test::trackedRowsReadOnly()
{
    const Show s = SHOW();
    ShowTreeView view(nullptr, m_doc);
    view.resize(1400, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.openInWorkbench(s.c2));
    Workbench *bench = view.dockedWorkbench();
    WorkbenchStack *stack = bench->stack();
    const int towers = layerNamed(stack, "Towers");
    const int usts = layerNamed(stack, "USTs");
    ShowTreeController *st = m_doc->showTree();
    const QString before = QString::fromUtf8(st->snapshotXml());

    // Not this cue's to take out, clear or move.
    QVERIFY(!stack->removeLayers({ usts }));
    QVERIFY(!stack->clearAttribute(usts, 0));
    QVERIFY(!stack->moveLayer(usts, towers));
    QVERIFY(bench->selectRow(row(ProgrammingOutline::CueLayerRow, 0xFFFFFFFF, quint32(usts))));
    QVERIFY(!bench->m_removeBtn->isEnabled());
    QTest::keyClick(bench->rows(), Qt::Key_Backspace);
    QVERIFY(bench->selectRow(row(ProgrammingOutline::AttributeRow, quint32(usts), 0)));
    QTest::keyClick(bench->rows(), Qt::Key_Backspace);
    QCOMPARE(QString::fromUtf8(st->snapshotXml()), before);
    QVERIFY(!st->canUndo());
    QVERIFY(bench->rows()->layerItem(usts)->flags() & Qt::ItemIsDropEnabled);
    QVERIFY(!(bench->rows()->layerItem(usts)->flags() & Qt::ItemIsDragEnabled));
    QVERIFY(!(bench->rows()->layerItem(usts)->flags() & Qt::ItemIsEditable));

    // A value set on it makes it the cue's own, as in the tree.
    QScopedPointer<QMimeData> low(paletteMime(s.low));
    QVERIFY(stack->dropOn(usts, low.data(), false));
    QCOMPARE(st->cue(s.c2)->layers.size(), 2);
    const int nowUsts = layerNamed(stack, "USTs");
    QVERIFY(!stack->layers().at(nowUsts).tracked);
    QCOMPARE(settle(m_doc).mid(2, 2), QList<int>({ 51, 51 }));
    // ...and now it is: removed in one step.
    QVERIFY(stack->removeLayers({ nowUsts }));
    QCOMPARE(st->cue(s.c2)->layers.size(), 1);
    QVERIFY(st->undo());
    QCOMPARE(st->cue(s.c2)->layers.size(), 2);
}

void ShowTreeWorkbench_Test::editorPane()
{
    const Show s = SHOW();
    ShowTreeView view(nullptr, m_doc);
    view.resize(1400, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.openInWorkbench(s.c2));
    Workbench *bench = view.dockedWorkbench();
    WorkbenchStack *stack = bench->stack();
    QStackedWidget *inspector = view.findChild<QStackedWidget *>("stInspector");
    QVERIFY(inspector != nullptr);

    // Nothing selected: the cue's own page (number, mode, fades...).
    QCOMPARE(bench->lentPane(), static_cast<QWidget *>(inspector));
    QVERIFY(bench->isAncestorOf(inspector));
    QCOMPARE(inspector->currentWidget(), view.m_cuePage);
    QVERIFY(inspector->isVisible());

    // A layer: the tree's layer page for it, every attribute with its ⋯.
    const int towers = layerNamed(stack, "Towers");
    QVERIFY(bench->selectRow(row(ProgrammingOutline::CueLayerRow, 0xFFFFFFFF, quint32(towers))));
    QCOMPARE(bench->currentPage(), Workbench::FunctionPage);
    QCOMPARE(inspector->currentWidget(), view.m_layerPage);
    QCOMPARE(view.currentKind(), ShowTreeView::LayerNode);         // the tree follows
    QCOMPARE(view.currentId(), s.towersLayer2);
    QVERIFY2(view.attributeRows().join("\n").contains("Dimmer | Low"), qPrintable(view.attributeRows().join("\n")));

    // A carried-in layer: its page too, from where it tracks.
    const int usts = layerNamed(stack, "USTs");
    QVERIFY(bench->selectRow(row(ProgrammingOutline::CueLayerRow, 0xFFFFFFFF, quint32(usts))));
    QCOMPARE(view.currentKind(), ShowTreeView::TrackedLayerNode);
    QVERIFY2(view.m_layerTitle->text().contains("tracked from 1 One"), qPrintable(view.m_layerTitle->text()));

    // A value: its palette in the Look Editor at full height, on the layer.
    QVERIFY(bench->selectRow(row(ProgrammingOutline::AttributeRow, quint32(towers), 0)));
    QCOMPARE(bench->currentPage(), Workbench::LookPage);
    QCOMPARE(bench->lookEditor()->paletteId(), s.low);
    QVERIFY2(bench->lookEditor()->trialNote().contains("shared palette"), qPrintable(bench->lookEditor()->trialNote()));
    QCOMPARE(view.currentKind(), ShowTreeView::LayerNode);         // the tree shows its layer
    QCOMPARE(view.currentId(), s.towersLayer2);
    QVERIFY(bench->lentPane() == nullptr);
    QCOMPARE(inspector->parentWidget(), static_cast<QWidget *>(pageSplitter(&view)));   // given back
    // ✎ on the page while docked: the Workbench's Look Editor.
    view.editPalette(s.full);
    QCOMPARE(bench->lookEditor()->paletteId(), s.full);

    // W4's plug: an Effect row asks the factory (none here: nothing).
    QVERIFY(!bench->designEffect());
    int asked = 0;
    Workbench::setEffectPaneFactory([&asked](Workbench *, const Workbench::EffectRequest &req, QWidget *parent) {
        ++asked;
        QLabel *l = new QLabel(QString("designer %1/%2").arg(req.cueId).arg(req.layerId), parent);
        l->setObjectName("fakeDesigner");
        return l;
    });
    QVERIFY(bench->selectRow(row(ProgrammingOutline::CueLayerRow, 0xFFFFFFFF, quint32(towers))));
    QVERIFY(bench->designEffect());
    QCOMPARE(asked, 1);
    QLabel *fake = bench->findChild<QLabel *>("fakeDesigner");
    QVERIFY(fake != nullptr);
    QCOMPARE(fake->text(), QString("designer %1/%2").arg(s.c2).arg(s.towersLayer2));
    Workbench::setEffectPaneFactory(nullptr);
}

void ShowTreeWorkbench_Test::backKeepsSelection()
{
    const Show s = SHOW();
    ShowTreeView view(nullptr, m_doc);
    view.resize(1400, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.selectNode(s.c2));
    QVERIFY(view.openSelectionInWorkbench());
    Workbench *bench = view.dockedWorkbench();
    const int towers = layerNamed(bench->stack(), "Towers");
    QVERIFY(bench->selectRow(row(ProgrammingOutline::CueLayerRow, 0xFFFFFFFF, quint32(towers))));

    QVERIFY(bench->canGoBack());
    QVERIFY(bench->goBack());
    QTRY_VERIFY(!view.isWorkbenchDocked());
    QVERIFY(!bench->isVisible());
    QVERIFY(pageSplitter(&view)->isVisible());
    QStackedWidget *inspector = view.findChild<QStackedWidget *>("stInspector");
    QCOMPARE(inspector->parentWidget(), static_cast<QWidget *>(pageSplitter(&view)));
    QVERIFY(inspector->isVisible());
    // The layer the Workbench had selected, selected in the tree, its page.
    QCOMPARE(view.currentKind(), ShowTreeView::LayerNode);
    QCOMPARE(view.currentId(), s.towersLayer2);
    QCOMPARE(view.currentCueId(), s.c2);
    QCOMPARE(inspector->currentWidget(), view.m_layerPage);
    // The tree previews again.
    QCOMPARE(m_doc->showTree()->previewedCue(), s.c2);
    QCOMPARE(settle(m_doc).mid(0, 4), QList<int>({ 51, 51, 128, 128 }));

    // Back from the cue itself: the cue selected.
    QVERIFY(view.openInWorkbench(s.c1));
    QVERIFY(view.closeWorkbench());
    QCOMPARE(view.currentKind(), ShowTreeView::CueNode);
    QCOMPARE(view.currentId(), s.c1);
    QVERIFY(!view.closeWorkbench());

    // The cue deleted while it is open: the page comes back.
    QVERIFY(view.openInWorkbench(s.c1));
    QVERIFY(m_doc->showTree()->removeCue(s.c1));
    QTRY_VERIFY(!view.isWorkbenchDocked());
    QVERIFY(pageSplitter(&view)->isVisible());
}

void ShowTreeWorkbench_Test::buildAfterHandBack()
{
    const Show s = SHOW();
    Scene *mine = new Scene(m_doc);
    mine->setName("Mine");
    m_doc->addFunction(mine);
    mine->addFixtureGroup(s.towers->id());
    mine->addPalette(s.half);
    Scene *other = new Scene(m_doc);
    other->setName("Other");
    m_doc->addFunction(other);

    QWidget window;
    window.resize(1500, 800);
    QTabWidget *tabs = new QTabWidget(&window);
    tabs->resize(1500, 800);
    ProgrammingManager *pm = new ProgrammingManager(tabs, m_doc);
    ShowTreeView *view = new ShowTreeView(tabs, m_doc);
    tabs->addTab(pm, "Build");
    tabs->addTab(view, "Show Tree");
    QPointer<ProgrammingManager> buildTab(pm);
    view->setWorkbenchLender(
        [buildTab]() -> Workbench * { return buildTab.isNull() ? nullptr : buildTab->lendWorkbench(); },
        [buildTab](Workbench *wb) { if (!buildTab.isNull()) buildTab->returnWorkbench(wb); });
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    pm->showFunction(mine->id());
    Workbench *bench = pm->workbench();
    QCOMPARE(bench->loadedFunction(), mine->id());

    // To the Show Tree: the cue opens in Build's very Workbench.
    tabs->setCurrentWidget(view);
    QVERIFY(view->openInWorkbench(s.c2, s.towersLayer2));
    QCOMPARE(view->dockedWorkbench(), bench);
    QVERIFY(pm->isWorkbenchLent());
    QVERIFY(view->isAncestorOf(bench));
    QVERIFY(pm->m_mainSplit->indexOf(bench) < 0);
    QVERIFY(pm->findChild<QLabel *>("workbenchLentNote") != nullptr);
    QVERIFY(qobject_cast<TreeCueStack *>(bench->stack()) != nullptr);
    QVERIFY(bench->m_stackWidget == nullptr);       // W2's walker: chasers only

    // Leaving the tab hands it back; coming back docks it again, the same
    // cue and layer.
    tabs->setCurrentWidget(pm);
    QTRY_VERIFY(!pm->isWorkbenchLent());
    QVERIFY(!view->isWorkbenchDocked());
    QCOMPARE(bench->loadedFunction(), mine->id());
    QVERIFY(pm->isAncestorOf(bench));
    QVERIFY(bench->isVisible());
    tabs->setCurrentWidget(view);
    QTRY_VERIFY(view->isWorkbenchDocked());
    QVERIFY(pm->isWorkbenchLent());
    TreeCueStack *cs = qobject_cast<TreeCueStack *>(bench->stack());
    QVERIFY(cs != nullptr && cs->cueId() == s.c2);
    QCOMPARE(bench->rows()->layerOf(bench->rows()->currentItem()), layerNamed(cs, "Towers"));

    // ↩: given back to Build, loaded as it was.
    QVERIFY(bench->goBack());
    QTRY_VERIFY(!pm->isWorkbenchLent());
    QVERIFY(pm->m_mainSplit->indexOf(bench) >= 0);
    QVERIFY(pm->findChild<QLabel *>("workbenchLentNote") == nullptr
            || pm->findChild<QLabel *>("workbenchLentNote")->parent() == nullptr);
    QCOMPARE(bench->loadedFunction(), mine->id());
    QVERIFY(!bench->canGoBack());       // no ↩ Show Tree left behind

    // Build works after: load another, edit, undo.
    tabs->setCurrentWidget(pm);
    pm->showFunction(other->id());
    QCOMPARE(bench->loadedFunction(), other->id());
    pm->showFunction(mine->id());
    QCOMPARE(bench->loadedFunction(), mine->id());
    QCOMPARE(bench->currentSceneId(), mine->id());
    QVERIFY(bench->isVisible());
    // W2's chaser walker, after the hand-back.
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Walk");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(mine->id()));
    chaser->addStep(ChaserStep(other->id()));
    pm->showFunction(chaser->id());
    QCOMPARE(bench->loadedFunction(), chaser->id());
    QVERIFY(bench->m_stackWidget != nullptr);
    QCOMPARE(QString(bench->m_stackWidget->metaObject()->className()), QString("ChaserWalker"));

    // The tree going first (the app closing) gives it back too.
    tabs->setCurrentWidget(view);
    QVERIFY(view->openInWorkbench(s.c1));
    QVERIFY(pm->isWorkbenchLent());
    delete view;
    QVERIFY(!pm->isWorkbenchLent());
    QVERIFY(pm->m_mainSplit->indexOf(bench) >= 0);
}

void ShowTreeWorkbench_Test::waysIn()
{
    const Show s = SHOW();
    ShowTreeView view(nullptr, m_doc);
    view.resize(1400, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // `wb` with a number: that cue of the list selected.
    QVERIFY(view.selectNode(s.list));
    QVERIFY(!view.runCommand("wb"));                 // no cue selected
    QVERIFY(!view.isWorkbenchDocked());
    QVERIFY(view.runCommand("wb 1"));
    QVERIFY(view.isWorkbenchDocked());
    QCOMPARE(qobject_cast<TreeCueStack *>(view.dockedWorkbench()->stack())->cueId(), s.c1);
    QVERIFY(!view.runCommand("wb 9"));
    QVERIFY(view.commandMessage().contains("No cue 9"));

    // The tree beside it: another cue, and the Workbench follows.
    QVERIFY(view.selectNode(s.c2));
    QCOMPARE(qobject_cast<TreeCueStack *>(view.dockedWorkbench()->stack())->cueId(), s.c2);
    // ...a layer: that row.
    QVERIFY(view.selectNode(s.towersLayer2, s.c2));
    Workbench *bench = view.dockedWorkbench();
    QCOMPARE(bench->rows()->layerOf(bench->rows()->currentItem()), layerNamed(bench->stack(), "Towers"));
    QVERIFY(view.closeWorkbench());

    // `wb` on a selected layer, double-click, ✎ and the menu.
    QVERIFY(view.selectNode(s.towersLayer2, s.c2));
    QVERIFY(view.runCommand("wb"));
    QCOMPARE(view.dockedWorkbench()->rows()->layerOf(view.dockedWorkbench()->rows()->currentItem()),
             layerNamed(view.dockedWorkbench()->stack(), "Towers"));
    QVERIFY(view.closeWorkbench());
    QTreeWidgetItem *cueItem = view.findItem(s.c1, invalidId());
    QVERIFY(cueItem != nullptr);
    emit view.tree()->itemDoubleClicked(cueItem, 0);
    QVERIFY(view.isWorkbenchDocked());
    QCOMPARE(qobject_cast<TreeCueStack *>(view.dockedWorkbench()->stack())->cueId(), s.c1);
    QVERIFY(view.closeWorkbench());
    QVERIFY(view.selectNode(s.c2));
    QToolButton *pencil = view.findChild<QToolButton *>("stOpenWorkbench");
    QVERIFY(pencil != nullptr);
    pencil->click();
    QVERIFY(view.isWorkbenchDocked());
    QVERIFY(view.closeWorkbench());
    QVERIFY(view.selectNode(s.list));
    pencil->click();
    QVERIFY(!view.isWorkbenchDocked());               // a list isn't opened
    // `help wb` explains it.
    QVERIFY(view.runCommand("help wb"));
    QVERIFY(view.helpOutput().contains("Workbench"));
}

void ShowTreeWorkbench_Test::solo()
{
    const Show s = SHOW();
    ShowTreeView view(nullptr, m_doc);
    view.resize(1400, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.openInWorkbench(s.c2, s.towersLayer2));
    Workbench *bench = view.dockedWorkbench();
    TreeCueStack *stack = qobject_cast<TreeCueStack *>(bench->stack());
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(bench->m_soloBtn->isVisible());
    QCOMPARE(settle(m_doc).mid(0, 4), QList<int>({ 51, 51, 128, 128 }));

    // Solo: only Towers plays (USTs, carried in, goes dark).
    bench->m_soloBtn->click();
    QVERIFY(bench->isSolo());
    QCOMPARE(st->previewedCue(), invalidId());
    QVERIFY(stack->soloScene() != Function::invalidId());
    QCOMPARE(stack->soloScene(), st->layerScene(s.c2, stack->targetAt(layerNamed(stack, "Towers"))));
    QCOMPARE(settle(m_doc, 20).mid(0, 4), QList<int>({ 51, 51, 0, 0 }));
    QVERIFY2(bench->m_onRig->text().contains("Solo: Towers"), qPrintable(bench->m_onRig->text()));

    // Another layer selected: it plays alone instead.
    QVERIFY(bench->selectRow(row(ProgrammingOutline::CueLayerRow, 0xFFFFFFFF,
                                 quint32(layerNamed(stack, "USTs")))));
    QCOMPARE(settle(m_doc, 20).mid(0, 4), QList<int>({ 0, 0, 128, 128 }));
    // An edit while Solo: the layer's new scene plays.
    QScopedPointer<QMimeData> full(paletteMime(s.full));
    QVERIFY(stack->dropOn(layerNamed(stack, "USTs"), full.data(), false));
    QTRY_COMPARE(settle(m_doc, 20).mid(0, 4), QList<int>({ 0, 0, 255, 255 }));

    // Off: in context again.
    bench->m_soloBtn->click();
    QVERIFY(!bench->isSolo());
    QCOMPARE(st->previewedCue(), s.c2);
    QCOMPARE(stack->soloScene(), Function::invalidId());
    QCOMPARE(settle(m_doc, 20).mid(0, 4), QList<int>({ 51, 51, 255, 255 }));

    // Stop / Play; Operate: nothing plays from here (the tree's rule).
    bench->m_playBtn->click();
    QCOMPARE(st->previewedCue(), invalidId());
    bench->m_playBtn->click();
    QCOMPARE(st->previewedCue(), s.c2);
    m_doc->setMode(Doc::Operate);
    QVERIFY(!bench->m_ownPreview);
    QCOMPARE(st->previewedCue(), invalidId());
    m_doc->setMode(Doc::Design);
    QCOMPARE(st->previewedCue(), s.c2);
}

void ShowTreeWorkbench_Test::newHereInTheDockedWorkbench()
{
    const Show s = SHOW();
    Scene *member = new Scene(m_doc);
    member->setName("Haze");
    m_doc->addFunction(member);
    Collection *coll = new Collection(m_doc);
    coll->setName("Effects");
    m_doc->addFunction(coll);
    coll->addFunction(member->id());
    ShowTreeController *st = m_doc->showTree();
    Target ft;
    ft.function = coll->id();
    st->addLayer(s.c2, ft);
    st->clearUndo();

    ShowTreeView view(nullptr, m_doc);
    view.resize(1400, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QVERIFY(view.openInWorkbench(s.c1));
    Workbench *bench = view.dockedWorkbench();
    bench->rows()->setFocus();
    QTRY_VERIFY(bench->rows()->hasFocus());

    // A cue in the Workbench: the tree's next cue after it.
    QCOMPARE(view.newHereLabel(), QString("New Cue After 1"));
    const quint32 made = view.newHere();
    QVERIFY(st->cue(made) != nullptr);
    QCOMPARE(st->cueList(s.list)->indexOf(made), 1);
    QCOMPARE(view.currentId(), made);
    TreeCueStack *cs = qobject_cast<TreeCueStack *>(bench->stack());
    QVERIFY(cs != nullptr);
    QCOMPARE(cs->cueId(), made);                // the Workbench followed
    QVERIFY(view.isRenaming());
    QTreeWidgetItem *editing = view.tree()->currentItem();
    view.tree()->closePersistentEditor(editing, 0);
    editing->setText(0, "New");
    QCoreApplication::processEvents();          // the rename lands after the editor
    QCOMPARE(st->cue(made)->name, QString("New"));
    view.slotUndo();
    view.slotUndo();                            // the cue, one step
    QVERIFY(st->cue(made) == nullptr);
    QCOMPARE(st->cueList(s.list)->cues.count(), 2);

    // A collection opened in the docked Workbench: its + Scene.
    QVERIFY(view.openInWorkbench(s.c2));
    bench = view.dockedWorkbench();
    const int effects = layerNamed(bench->stack(), "Effects");
    QVERIFY(effects >= 0);
    emit bench->rows()->itemDoubleClicked(bench->rows()->layerItem(effects), 0);
    QCOMPARE(bench->loadedFunction(), coll->id());
    bench->rows()->setFocus();
    QTRY_VERIFY(bench->rows()->hasFocus());
    QCOMPARE(view.newHereLabel(), QString("New Scene in Effects"));
    const quint32 scene = view.newHere();
    QCOMPARE(coll->functions(), QList<quint32>({ member->id(), scene }));
    QVERIFY(st->undoShared());
    QCOMPARE(coll->functions(), QList<quint32>({ member->id() }));
}

void ShowTreeWorkbench_Test::opaqueLayersAndAddNew()
{
    const Show s = SHOW();
    Scene *raw = new Scene(m_doc);
    raw->setName("Haze");
    m_doc->addFunction(raw);
    raw->setValue(m_fixtures[5], 0, 200);
    ShowTreeController *st = m_doc->showTree();
    Target ft;
    ft.function = raw->id();
    st->addLayer(s.c2, ft);
    st->clearUndo();

    ShowTreeView view(nullptr, m_doc);
    view.resize(1400, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.openInWorkbench(s.c2));
    Workbench *bench = view.dockedWorkbench();
    WorkbenchStack *stack = bench->stack();
    const int haze = layerNamed(stack, "Haze");
    QVERIFY(haze >= 0);
    QCOMPARE(stack->layers().at(haze).kind, WorkbenchStack::Layer::FunctionLayer);
    QCOMPARE(stack->layers().at(haze).functionId, raw->id());
    QCOMPARE(settle(m_doc).at(5), 200);

    // ▣ Open (double-click): it loads in the Workbench; ↩ back to the cue.
    emit bench->rows()->itemDoubleClicked(bench->rows()->layerItem(haze), 0);
    QCOMPARE(bench->loadedFunction(), raw->id());
    QVERIFY(view.isWorkbenchDocked());
    QCOMPARE(bench->m_backBtn->text(), QString("↩ 2 Two"));
    QVERIFY(bench->goBack());
    QVERIFY(qobject_cast<TreeCueStack *>(bench->stack()) != nullptr);
    QVERIFY(view.isWorkbenchDocked());
    QCOMPARE(bench->m_backBtn->text(), QString("↩ Show Tree"));
    // The tree's layer page Open, docked: in the Workbench too.
    view.m_functionOpener = [](quint32) { return false; };
    QVERIFY(view.selectNode(st->cue(s.c2)->layers.last().id, s.c2));
    QPushButton *open = view.findChild<QPushButton *>("stLayerOpen");
    QVERIFY(open != nullptr);
    open->click();
    QCOMPARE(bench->loadedFunction(), raw->id());
    QVERIFY(bench->goBack());

    // + EFX on the selected layer's fixtures: one tree step, with the EFX.
    const int towers = layerNamed(stack = bench->stack(), "Towers");
    QVERIFY(bench->selectRow(row(ProgrammingOutline::CueLayerRow, 0xFFFFFFFF, quint32(towers))));
    QVERIFY(bench->m_addEfxBtn->isEnabled());
    const int before = m_doc->functions().size();
    bench->m_addEfxBtn->trigger();
    QCOMPARE(st->cue(s.c2)->layers.size(), 3);
    const quint32 efxId = st->cue(s.c2)->layers.last().target.function;
    EFX *efx = qobject_cast<EFX *>(m_doc->function(efxId));
    QVERIFY(efx != nullptr);
    QCOMPARE(efx->fixtures().size(), 2);
    QVERIFY(st->undo());
    QCOMPARE(st->cue(s.c2)->layers.size(), 2);
    QVERIFY(m_doc->function(efxId) == nullptr);
    QVERIFY(st->redo());
    QCOMPARE(st->cue(s.c2)->layers.size(), 3);
    QVERIFY(qobject_cast<EFX *>(m_doc->function(efxId)) != nullptr);
    QCOMPARE(qobject_cast<EFX *>(m_doc->function(efxId))->fixtures().size(), 2);
    QVERIFY(m_doc->functions().size() >= before);
    efx = nullptr;          // undone and redone: another object, the same id
    QTRY_VERIFY(layerNamed(bench->stack(), m_doc->function(efxId)->name()) >= 0);

    // Removing it from the cue: one step; the EFX stays in the show.
    const int efxRow = layerNamed(bench->stack(), m_doc->function(efxId)->name());
    QVERIFY(bench->stack()->removeLayers({ efxRow }));
    QCOMPARE(st->cue(s.c2)->layers.size(), 2);
    QVERIFY(m_doc->function(efxId) != nullptr);
}

/****************************************************************************
 * Pictures on real cues (not part of the run unless asked for)
 ****************************************************************************/

namespace
{
/* Esc as the user presses it: to whatever has the focus (the window's
   own focus widget when that's what it is). */
void pressEsc(QWidget *where)
{
    QWidget *w = where;
    if (w->focusProxy() != nullptr)
        w = w->focusProxy();
    QTest::keyClick(w, Qt::Key_Escape);
}

QString currentKey(QTreeWidget *rows)
{
    QTreeWidgetItem *it = rows->currentItem();
    return it == nullptr ? QString() : it->data(0, ProgrammingOutline::KeyRole).toString();
}
}

void ShowTreeWorkbench_Test::escBacksOutOfAPaletteInBuild()
{
    const Show s = SHOW();
    Scene *mine = new Scene(m_doc);
    mine->setName("Mine");
    m_doc->addFunction(mine);
    mine->addFixtureGroup(s.towers->id());
    mine->addPalette(s.half);
    QLCPalette *red = new QLCPalette(QLCPalette::Color, m_doc);
    red->setName("Red");
    red->setValue(QColor(Qt::red).name());
    m_doc->addPalette(red);

    ProgrammingManager pm(nullptr, m_doc);
    pm.resize(1500, 900);
    pm.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pm));
    pm.showFunction(mine->id());
    Workbench *bench = pm.workbench();
    QCOMPARE(bench->currentPage(), Workbench::ScenePage);
    // A target row selected: the page that shows, and the row, come back.
    QVERIFY(bench->selectRow(row(ProgrammingOutline::GroupRow, mine->id(), s.towers->id())));
    QCOMPARE(bench->currentPage(), Workbench::TargetPage);
    const QString selected = currentKey(bench->rows());
    QVERIFY(!selected.isEmpty());

    // Build ▸ Palettes ▸ Color: double-click Red -- the Look Editor over
    // the page, ↩ Back on it.
    pm.setSection(ProgrammingManager::PalettesSection);
    QTreeWidgetItem *redItem = pm.m_paletteTree->paletteItem(red);
    QVERIFY(redItem != nullptr);
    pm.revealPalette(red->id());
    pm.m_paletteTree->setFocus(Qt::OtherFocusReason);
    QCoreApplication::processEvents();
    emit pm.m_paletteTree->itemDoubleClicked(pm.m_paletteTree->paletteItem(red), 0);
    QCOMPARE(bench->currentPage(), Workbench::LookPage);
    QCOMPARE(bench->currentPaletteId(), red->id());
    LookEditor *look = bench->lookEditor();
    QToolButton *back = look->findChild<QToolButton *>("lookBack");
    QVERIFY(back != nullptr && back->isVisible());
    QCOMPARE(look->backText(), QString("↩ Back"));

    // An edit, made live.
    QColorDialog *picker = look->findChild<QColorDialog *>("lookColor");
    QVERIFY(picker != nullptr && picker->isVisible());
    const QString before = red->value().toString();
    picker->setCurrentColor(QColor(Qt::green));
    const QString edited = red->value().toString();
    QVERIFY(edited != before);
    QColor rgb, wauv;
    QVERIFY(QLCPalette::stringToColor(edited, rgb, wauv));
    QCOMPARE(rgb, QColor(Qt::green));
    const QString editedName = red->name();

    // Esc from the list (where the focus stays after the double-click):
    // the page that was showing, the row still selected, the edit kept.
    pm.m_paletteTree->setFocus(Qt::OtherFocusReason);
    pressEsc(pm.m_paletteTree);
    QCOMPARE(bench->currentPage(), Workbench::TargetPage);
    QCOMPARE(currentKey(bench->rows()), selected);
    QVERIFY(bench->rows()->currentItem()->isSelected());
    QCOMPARE(red->value().toString(), edited);
    QVERIFY(!back->isVisible());
    // Nothing more to back out of in Build: Esc goes on, nothing changes.
    pressEsc(bench->rows());
    QCOMPARE(bench->currentPage(), Workbench::TargetPage);
    QCOMPARE(bench->loadedFunction(), mine->id());

    // Esc inside the colour picker: the picker (a QColorDialog) used to
    // take it and hide itself -- as any embedded QColorDialog does:
    {
        QWidget host;
        QColorDialog *bare = new QColorDialog(&host);
        bare->setOptions(QColorDialog::NoButtons | QColorDialog::DontUseNativeDialog);
        bare->setWindowFlags(Qt::Widget);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QTest::keyClick(bare, Qt::Key_Escape);
        QVERIFY(bare->isHidden());
    }
    // Now it backs out, the picker still there.
    QCoreApplication::processEvents();
    emit pm.m_paletteTree->itemDoubleClicked(pm.m_paletteTree->paletteItem(red), 0);
    QCOMPARE(bench->currentPage(), Workbench::LookPage);
    QLineEdit *hex = nullptr;
    for (QLineEdit *e : picker->findChildren<QLineEdit *>())
        if (e->isVisible())
            hex = e;
    QVERIFY(hex != nullptr);
    hex->setFocus(Qt::OtherFocusReason);
    QTest::keyClick(hex, Qt::Key_Escape);
    QCOMPARE(bench->currentPage(), Workbench::TargetPage);
    QVERIFY(!picker->isHidden());
    QCOMPARE(currentKey(bench->rows()), selected);

    // ↩ Back, clicked, does the same.
    QCoreApplication::processEvents();
    emit pm.m_paletteTree->itemDoubleClicked(pm.m_paletteTree->paletteItem(red), 0);
    QVERIFY(back->isVisible());
    back->click();
    QCOMPARE(bench->currentPage(), Workbench::TargetPage);

    // A look row of the scene's own is the rows' page: no ↩ Back, and Esc
    // there goes nowhere.
    QVERIFY(bench->selectRow(row(ProgrammingOutline::PaletteRow, mine->id(), s.half)));
    QCOMPARE(bench->currentPage(), Workbench::LookPage);
    QVERIFY(!back->isVisible());
    pressEsc(bench->rows());
    QCOMPARE(bench->currentPage(), Workbench::LookPage);

    // A rename in the palette list: Esc cancels only the rename.
    QCoreApplication::processEvents();
    emit pm.m_paletteTree->itemDoubleClicked(pm.m_paletteTree->paletteItem(red), 0);
    QCOMPARE(bench->currentPage(), Workbench::LookPage);
    pm.revealPalette(red->id());
    QCOMPARE(pm.m_paletteTree->currentItem(), pm.m_paletteTree->paletteItem(red));
    pm.m_paletteTree->setFocus(Qt::OtherFocusReason);
    QTest::keyClick(pm.m_paletteTree, Qt::Key_F2);       // rename in place
    auto renameEditor = [&pm]() -> QLineEdit * {
        for (QLineEdit *e : pm.m_paletteTree->viewport()->findChildren<QLineEdit *>())
            if (e->isVisible())
                return e;
        return nullptr;
    };
    QLineEdit *rename = renameEditor();
    QVERIFY(rename != nullptr);
    rename->setText("Not Red");
    QTest::keyClick(rename, Qt::Key_Escape);
    QTRY_VERIFY(renameEditor() == nullptr);
    QCOMPARE(red->name(), editedName);
    QCOMPARE(bench->currentPage(), Workbench::LookPage);
    QVERIFY(back->isVisible());
    pressEsc(pm.m_paletteTree);
    QCOMPARE(bench->currentPage(), Workbench::LookPage);     // back to the look row's own
    QCOMPARE(bench->currentPaletteId(), s.half);
}

void ShowTreeWorkbench_Test::escBacksOutOfALayoutAndAnOpenedLayer()
{
    const Show s = SHOW();
    Scene *mine = new Scene(m_doc);
    mine->setName("Mine");
    m_doc->addFunction(mine);
    mine->addFixtureGroup(s.towers->id());
    mine->addPalette(s.half);
    Scene *other = new Scene(m_doc);
    other->setName("Other");
    m_doc->addFunction(other);
    Collection *coll = new Collection(m_doc);
    coll->setName("Both");
    m_doc->addFunction(coll);
    coll->addFunction(mine->id());
    coll->addFunction(other->id());

    Workbench bench(m_doc);
    bench.resize(1200, 800);
    bench.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bench));

    // A group's layout from a scene: ← Back to it; Esc too.
    bench.load(mine->id());
    QCOMPARE(bench.currentPage(), Workbench::ScenePage);
    bench.showGroupLayout(s.towers->id());
    QCOMPARE(bench.currentPage(), Workbench::FunctionPage);
    QPushButton *groupBack = bench.findChild<QPushButton *>("workbenchGroupBack");
    QVERIFY(groupBack != nullptr && groupBack->isVisible());
    QCOMPARE(groupBack->text(), QString("← Back to Mine"));
    pressEsc(bench.m_funcEditor);
    QCOMPARE(bench.currentPage(), Workbench::ScenePage);
    QCOMPARE(bench.currentSceneId(), mine->id());

    // With no scene open before: ← Back, to the empty page.
    {
        Workbench fresh(m_doc);
        fresh.resize(1200, 800);
        fresh.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fresh));
        fresh.showGroupLayout(s.towers->id());
        QPushButton *freshBack = fresh.findChild<QPushButton *>("workbenchGroupBack");
        QVERIFY(freshBack != nullptr && freshBack->isVisible());
        QCOMPARE(freshBack->text(), QString("← Back"));
        pressEsc(fresh.m_funcEditor);
        QCOMPARE(fresh.currentPage(), Workbench::EmptyPage);
        QVERIFY(freshBack->isHidden());
    }

    // ▣ Open a layer of the collection, Esc: ↩ to the collection, the row
    // selected again.
    bench.load(coll->id());
    const int otherLayer = layerNamed(bench.stack(), "Other");
    QVERIFY(otherLayer >= 0);
    QVERIFY(bench.selectRow(row(ProgrammingOutline::SceneRow, other->id(), other->id())));
    const QString selected = currentKey(bench.rows());
    bench.openSelected();
    QCOMPARE(bench.loadedFunction(), other->id());
    QVERIFY(bench.canGoBack());
    bench.rows()->setFocus(Qt::OtherFocusReason);
    pressEsc(bench.rows());
    QCOMPARE(bench.loadedFunction(), coll->id());
    QCOMPARE(currentKey(bench.rows()), selected);
    QVERIFY(!bench.canGoBack());
    // Nowhere left: Esc goes on, nothing changes.
    pressEsc(bench.rows());
    QCOMPARE(bench.loadedFunction(), coll->id());
}

void ShowTreeWorkbench_Test::escBacksOutInTheShowTree()
{
    const Show s = SHOW();
    {
        // Undocked: the Look Editor under the page; ↩ Back and Esc close it.
        ShowTreeView view(nullptr, m_doc);
        view.resize(1400, 800);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.selectNode(s.towersLayer2, s.c2);
        view.editPalette(s.low);
        LookEditor *look = view.m_lookEditor;
        QVERIFY(look->isVisible());
        QCOMPARE(look->backText(), QString("↩ Back"));
        view.m_tree->setFocus(Qt::OtherFocusReason);
        pressEsc(view.m_tree);
        QVERIFY(!look->isVisible());
        QCOMPARE(view.currentId(), s.towersLayer2);
        QCOMPARE(view.currentCueId(), s.c2);
        view.editPalette(s.low);
        QVERIFY(look->isVisible());
        look->findChild<QToolButton *>("lookBack")->click();
        QVERIFY(!look->isVisible());
        // Esc with nothing open: nothing happens (no Look Editor, no undock).
        pressEsc(view.m_tree);
        QCOMPARE(view.currentId(), s.towersLayer2);
    }

    // Docked: a palette over the layer's page, then ↩ Show Tree.
    ShowTreeView view(nullptr, m_doc);
    view.resize(1400, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.openInWorkbench(s.c2, s.towersLayer2));
    Workbench *bench = view.dockedWorkbench();
    QVERIFY(bench != nullptr);
    const Workbench::Page page = bench->currentPage();
    const QString selected = currentKey(bench->rows());
    view.editPalette(s.half);
    QCOMPARE(bench->currentPage(), Workbench::LookPage);
    QCOMPARE(bench->currentPaletteId(), s.half);
    view.m_tree->setFocus(Qt::OtherFocusReason);
    pressEsc(view.m_tree);
    QCOMPARE(bench->currentPage(), page);
    QCOMPARE(currentKey(bench->rows()), selected);
    QVERIFY(view.isWorkbenchDocked());
    // Esc again, from the Workbench's rows: the page back, the layer selected.
    bench->rows()->setFocus(Qt::OtherFocusReason);
    pressEsc(bench->rows());
    QTRY_VERIFY(!view.isWorkbenchDocked());
    QVERIFY(pageSplitter(&view)->isVisible());
    QCOMPARE(view.currentId(), s.towersLayer2);
    QCOMPARE(view.currentCueId(), s.c2);
}

void ShowTreeWorkbench_Test::shots()
{
    const QString out = qEnvironmentVariable("W3_SHOTS");
    const QString show06 = qEnvironmentVariable("W3_SHOW_06");
    const QString show01 = qEnvironmentVariable("W3_SHOW_01");
    if (out.isEmpty() || show06.isEmpty())
        QSKIP("set W3_SHOTS, W3_SHOW_06 (and W3_SHOW_01) to take the pictures");
    QDir().mkpath(out);

    App *app = new App();
    app->init();
    app->resize(1680, 1000);
    app->showNormal();
    QVERIFY(QTest::qWaitForWindowExposed(app));
    auto settleApp = [](int ms = 300) { QTest::qWait(ms); };
    auto toTree = [app, &settleApp]() {
        ShowTreeView *v = ShowTreeView::instance();
        QWidget *w = v;
        while (w != nullptr && app->m_tab->indexOf(w) < 0)
            w = w->parentWidget();
        if (w != nullptr)
            app->m_tab->setCurrentWidget(w);
        settleApp();
        return v;
    };
    auto shoot = [app, &out, &settleApp](const QString &name) {
        settleApp(500);
        QVERIFY(app->grab().save(QDir(out).filePath(name)));
        qInfo().noquote() << "[W3SHOT]" << QDir(out).filePath(name);
    };

    // DanceHysteria-0.6: its Show Tree list, a cue with layers tracking in.
    app->clearDocument();
    QCOMPARE(app->loadXML(show06), QFile::NoError);
    app->doc()->setMode(Doc::Design);
    settleApp(800);
    ShowTreeView *view = toTree();
    ShowTreeController *st = app->doc()->showTree();
    quint32 pick = invalidId(), layer = invalidId();
    int bestCarried = -1;
    for (const Production &p : st->productions())
        for (const CueList &l : p.lists)
        {
            if (l.looks || l.isSubCues())
                continue;
            const QList<ResolvedCue> rcs = st->resolved(l);
            for (int i = 0; i < l.cues.size() && i < rcs.size(); ++i)
            {
                int carried = 0;
                for (const ResolvedLayer &rl : rcs.at(i).layers)
                    carried += rl.own ? 0 : 1;
                if (!l.cues.at(i).layers.isEmpty() && carried > bestCarried)
                {
                    bestCarried = carried;
                    pick = l.cues.at(i).id;
                    layer = l.cues.at(i).layers.first().id;
                    for (const Layer &own : l.cues.at(i).layers)
                        if (!own.target.isOpaque() && !own.looks.isEmpty())
                        {
                            layer = own.id;
                            break;
                        }
                }
            }
        }
    QVERIFY2(pick != invalidId(), "no cue with layers in the show");
    QVERIFY(view->openInWorkbench(pick, layer));
    Workbench *bench = view->dockedWorkbench();
    bench->rows()->expandAll();
    shoot("w3-cue-in-workbench.png");
    // A value: its palette in the Look Editor at full height.
    {
        TreeCueStack *cs = qobject_cast<TreeCueStack *>(bench->stack());
        const int li = cs->indexOfLayer(layer);
        if (li >= 0 && !cs->layers().at(li).attributes.isEmpty())
        {
            int ai = 0;
            for (int i = 0; i < cs->layers().at(li).attributes.size(); ++i)
                if (cs->layers().at(li).attributes.at(i).key == KeyColour)
                    ai = i;
            bench->selectRow(row(ProgrammingOutline::AttributeRow, quint32(li), quint32(ai)));
            shoot("w3-value-in-look-editor.png");
        }
    }
    QVERIFY(view->closeWorkbench());
    shoot("w3-back-in-tree.png");

    // DanceHysteria-0.1, "Full Show" imported: a real imported cue.
    if (!show01.isEmpty())
    {
        app->clearDocument();
        QCOMPARE(app->loadXML(show01), QFile::NoError);
        app->doc()->setMode(Doc::Design);
        settleApp(800);
        view = toTree();
        quint32 chaser = invalidId();
        for (Function *f : app->doc()->functions())
            if (f->type() == Function::ChaserType && f->name() == QLatin1String("Full Show"))
                chaser = f->id();
        QVERIFY2(chaser != invalidId(), "no Full Show chaser");
        view->setSheetRunner([](ImportSheet *) {});
        view->importChaser(chaser, false);
        st = app->doc()->showTree();
        quint32 cue = invalidId();
        for (const Production &p : st->productions())
            for (const CueList &l : p.lists)
                for (const Cue &c : l.cues)
                    if (cue == invalidId() && c.name.contains("Announcer", Qt::CaseInsensitive))
                        cue = c.id;
        if (cue == invalidId())
            for (const Production &p : st->productions())
                for (const CueList &l : p.lists)
                    if (l.cues.size() > 3 && cue == invalidId())
                        cue = l.cues.at(3).id;
        QVERIFY(cue != invalidId());
        QVERIFY(view->openInWorkbench(cue));
        view->dockedWorkbench()->rows()->expandAll();
        shoot("w3-imported-cue.png");
        // Solo on its first own layer.
        TreeCueStack *cs = qobject_cast<TreeCueStack *>(view->dockedWorkbench()->stack());
        for (int i = 0; i < cs->layers().size(); ++i)
            if (!cs->layers().at(i).tracked && cs->layers().at(i).kind == WorkbenchStack::Layer::CueLayer)
            {
                view->dockedWorkbench()->selectRow(row(ProgrammingOutline::CueLayerRow, 0xFFFFFFFF, quint32(i)));
                break;
            }
        view->dockedWorkbench()->setSolo(true);
        shoot("w3-imported-cue-solo.png");
        view->dockedWorkbench()->setSolo(false);
        view->closeWorkbench();
    }

    app->doc()->resetModified();
    delete app;
}

QTEST_MAIN(ShowTreeWorkbench_Test)

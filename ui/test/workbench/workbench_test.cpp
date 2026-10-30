/*
  Q Light Controller Plus - Test Unit
  workbench_test.cpp

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

#include <QtTest>
#include <QScopeGuard>
#include <QTreeWidgetItemIterator>
#include <QXmlStreamReader>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QButtonGroup>
#include <QToolButton>
#include <QMenu>
#include <QPushButton>
#include <QDropEvent>
#include <QMimeData>
#include <QSplitter>
#include <QLabel>
#include <QFile>

#define protected public
#define private public
#include "programmingmanager.h"
#include "programmingoutline.h"
#include "workbench.h"
#include "workbenchstack.h"
#include "mastertimer.h"            // timerTick(): the engine ticked by hand
#undef private
#undef protected

#include "showtreecontroller.h"
#include "qlcfixturedefcache.h"
#include "rgbmatrixeditor.h"
#include "showembedding.h"
#include "fixturegroup.h"
#include "chaserstep.h"
#include "efxeditor.h"
#include "paletteops.h"
#include "collection.h"
#include "qlcpalette.h"
#include "rgbmatrix.h"
#include "fixture.h"
#include "chaser.h"
#include "script.h"
#include "scene.h"
#include "efx.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcchannel.h"
#include "universe.h"
#include "doc.h"

#include "workbench_test.h"
#include "showcontrol.h"

using Row = ProgrammingOutline::Row;

static Row row(int kind, quint32 scene, quint32 id)
{
    Row r;
    r.kind = kind;
    r.scene = scene;
    r.id = id;
    return r;
}

static quint32 s_address = 0;

void Workbench_Test::init()
{
    m_doc = new Doc(this);
    m_doc->inputOutputMap()->addUniverse();
    m_doc->inputOutputMap()->addUniverse();
    s_address = 0;
}

void Workbench_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

Scene *Workbench_Test::scene(const QString &name, const QString &path)
{
    Scene *s = new Scene(m_doc);
    s->setName(name);
    m_doc->addFunction(s);
    s->setPath(path);
    return s;
}

QLCPalette *Workbench_Test::palette(int type, const QString &name)
{
    QLCPalette *p = new QLCPalette(QLCPalette::PaletteType(type), m_doc);
    p->setName(name);
    if (type == QLCPalette::Color)
        p->setValue(QColor(Qt::blue).name());
    else if (type == QLCPalette::Dimmer)
        p->setValue(128);
    m_doc->addPalette(p);
    return p;
}

FixtureGroup *Workbench_Test::group(const QString &name, int count)
{
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName(name);
    g->setSize(QSize(count, 1));
    m_doc->addFixtureGroup(g);
    for (int i = 0; i < count; ++i)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("%1 %2").arg(name).arg(i + 1));
        f->setChannels(3);
        f->setAddress(s_address);
        s_address += 3;
        if (!m_doc->addFixture(f))
            qFatal("fixture %s not added", qPrintable(f->name()));
        g->assignFixture(f->id(), QLCPoint(i, 0));
    }
    return g;
}

Collection *Workbench_Test::collection(const QString &name, const QList<quint32> &members)
{
    Collection *c = new Collection(m_doc);
    c->setName(name);
    m_doc->addFunction(c);
    for (quint32 m : members)
        c->addFunction(m);
    return c;
}

/* A drop as the view delivers it, on @a onto's row (null: empty space). */
static bool dropOn(ProgrammingOutline *o, QMimeData *mime, QTreeWidgetItem *onto, QString *plan = nullptr)
{
    const QPoint pos = onto != nullptr ? o->visualItemRect(onto).center()
                                       : QPoint(10, o->viewport()->height() - 4);
    if (plan != nullptr)
        *plan = o->dropPlan(mime, pos, false);
    QDropEvent ev(QPointF(pos), Qt::CopyAction | Qt::MoveAction, mime, Qt::LeftButton, Qt::NoModifier);
    o->dropEvent(&ev);
    const bool ok = ev.isAccepted();
    delete mime;
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    return ok;
}

static void selectItem(ProgrammingOutline *o, QTreeWidgetItem *it)
{
    o->clearSelection();
    o->setCurrentItem(it);
    it->setSelected(true);
}

static int kindOf(QTreeWidgetItem *it)
{
    return it != nullptr ? it->data(0, ProgrammingOutline::KindRole).toInt() : 0;
}

void Workbench_Test::aSceneIsOneLayer()
{
    FixtureGroup *steps = group("Steps", 2);
    QLCPalette *blue = palette(QLCPalette::Color, "Blue");
    Scene *s = scene("Base", "Show/Base");
    s->addFixtureGroup(steps->id());
    s->addPalette(blue->id());

    Workbench wb(m_doc);
    QCOMPARE(wb.loadedFunction(), Function::invalidId());
    QCOMPARE(wb.currentPage(), Workbench::EmptyPage);
    QCOMPARE(wb.rows()->mode(), ProgrammingOutline::Stack);
    QCOMPARE(kindOf(wb.rows()->topLevelItem(0)), int(ProgrammingOutline::HintRow));

    wb.load(s->id());
    QCOMPARE(wb.loadedFunction(), s->id());
    ProgrammingOutline *rows = wb.rows();
    QCOMPARE(rows->topLevelItemCount(), 1);
    QTreeWidgetItem *layer = rows->topLevelItem(0);
    QCOMPARE(kindOf(layer), int(ProgrammingOutline::SceneRow));
    QVERIFY(layer->isExpanded());                  // a layer is open: it's what you loaded
    QCOMPARE(layer->childCount(), 2);              // Steps, Blue
    QCOMPARE(rows->layerOf(layer->child(1)), 0);

    // The header: what, where, the kind.
    QCOMPARE(wb.m_title->text(), QString("Base"));
    QCOMPARE(wb.m_kind->text(), QString("SCENE"));
    QCOMPARE(wb.m_crumb->text(), QString("Show › Base"));
    QVERIFY(wb.m_info->text().contains("1 look"));
    QVERIFY(wb.m_info->text().contains("Fired by"));
    QVERIFY(wb.m_backBtn->isHidden());

    // Its own layer is selected: the scene page, the scene being edited.
    QCOMPARE(wb.currentPage(), Workbench::ScenePage);
    QCOMPARE(wb.currentSceneId(), s->id());
    QVERIFY(wb.m_canvas != nullptr);

    // A scene is one layer: nothing is added to it here.
    QVERIFY(!wb.m_addSceneBtn->isEnabled());
    QVERIFY(!wb.m_addEfxBtn->isEnabled());
    QVERIFY(!wb.m_addEffectBtn->isEnabled());
    QVERIFY(wb.m_addEfxBtn->toolTip().contains("load a collection"));
    QVERIFY(!wb.m_removeBtn->isEnabled());
    QCOMPARE(wb.stack()->addNew(Function::EFXType, 0, {}, FixtureGroup::invalidId()), Function::invalidId());

    // Unloaded: nothing, and the header says how to load something.
    wb.clear();
    QCOMPARE(wb.loadedFunction(), Function::invalidId());
    QCOMPARE(wb.currentSceneId(), Function::invalidId());
    QCOMPARE(wb.m_title->text(), QString("Nothing loaded"));
}

void Workbench_Test::aCollectionsMembersAreLayers()
{
    FixtureGroup *g = group("Washes", 3);
    QLCPalette *blue = palette(QLCPalette::Color, "Blue");
    Scene *a = scene("Main Drape", "Show");
    a->addFixtureGroup(g->id());
    a->addPalette(blue->id());
    Scene *raw = scene("Wash-Red", "Washes");
    raw->setValue(g->fixtureList().first(), 0, 255);
    EFX *efx = new EFX(m_doc);
    efx->setName("Sweep");
    m_doc->addFunction(efx);
    Collection *base = collection("0.0.0-base", { raw->id() });
    Collection *c = collection("0.0.4-SHOUT", { a->id(), raw->id(), efx->id(), base->id() });
    c->setPath("Show");
    const int functions = m_doc->functions().size();

    Workbench wb(m_doc);
    wb.resize(1200, 700);
    wb.load(c->id());
    ProgrammingOutline *rows = wb.rows();
    QCOMPARE(rows->topLevelItemCount(), 4);
    QCOMPARE(kindOf(rows->layerItem(0)), int(ProgrammingOutline::SceneRow));
    QCOMPARE(kindOf(rows->layerItem(1)), int(ProgrammingOutline::SceneRow));
    QCOMPARE(kindOf(rows->layerItem(2)), int(ProgrammingOutline::FunctionRow));
    QCOMPARE(kindOf(rows->layerItem(3)), int(ProgrammingOutline::FunctionRow));
    // A group scene opens up into its targets and looks; a raw one says so.
    QCOMPARE(rows->layerItem(0)->childCount(), 2);
    QVERIFY(rows->layerItem(1)->text(1).contains("▣"));
    // Anything else plays as it is: a ▣ row, named by its kind.
    QVERIFY(rows->layerItem(2)->text(1).contains("EFX ▣"));
    QVERIFY(rows->layerItem(3)->text(1).contains("Collection ▣"));

    // The whole: its header and overview.
    QCOMPARE(wb.m_kind->text(), QString("COLLECTION"));
    QCOMPARE(wb.currentPage(), Workbench::OverviewPage);
    QVERIFY(wb.m_overviewText->text().contains("4 layer"));
    QVERIFY(wb.m_addSceneBtn->isEnabled() && wb.m_addEfxBtn->isEnabled() && wb.m_addMatrixBtn->isEnabled());
    // One + Effect ▾ holds EFX and RGB Matrix (Q94): no button each.
    {
        QToolButton *effect = wb.findChild<QToolButton *>("workbenchAddEffect");
        QVERIFY(effect != nullptr && effect->isEnabled());
        QCOMPARE(effect->text(), QString("+ Effect"));
        QVERIFY(effect->menu() != nullptr);
        QCOMPARE(effect->menu()->actions().mid(0, 2), QList<QAction *>({ wb.m_addEfxBtn, wb.m_addMatrixBtn }));
        QCOMPARE(effect->menu()->actions().last()->objectName(), QString("workbenchDesignEffect"));
        QCOMPARE(wb.m_addEfxBtn->text(), QString("EFX"));
        QCOMPARE(wb.m_addMatrixBtn->text(), QString("RGB Matrix"));
        for (QAbstractButton *b : wb.findChildren<QAbstractButton *>())
            QVERIFY2(!b->text().startsWith("+ EFX") && !b->text().startsWith("+ RGB"), qPrintable(b->text()));
    }
    QCOMPARE(wb.currentSceneId(), Function::invalidId());

    // A ▣ EFX row: its stock editor in the editor pane.
    selectItem(rows, rows->layerItem(2));
    QCOMPARE(wb.currentPage(), Workbench::FunctionPage);
    QVERIFY(wb.m_functionPage->findChild<EFXEditor *>() != nullptr);
    QVERIFY(wb.m_openBtn->isEnabled());
    // A nested collection: a way to open it.
    selectItem(rows, rows->layerItem(3));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(wb.currentPage(), Workbench::FunctionPage);
    QVERIFY(wb.m_functionPage->findChild<QPushButton *>("workbenchOpenHere") != nullptr);
    QVERIFY(wb.m_functionPage->findChild<EFXEditor *>() == nullptr);
    // A look in a layer: the Look Editor; the layer's scene is the one edited.
    QTreeWidgetItem *look = rows->itemFor(row(ProgrammingOutline::PaletteRow, a->id(), blue->id()));
    QVERIFY(look != nullptr);
    selectItem(rows, look);
    QCOMPARE(wb.currentPage(), Workbench::LookPage);
    QCOMPARE(wb.currentPaletteId(), blue->id());
    QCOMPARE(wb.currentSceneId(), a->id());
    // The layer's own row: its timing.
    selectItem(rows, rows->layerItem(0));
    QCOMPARE(wb.currentPage(), Workbench::ScenePage);
    QCOMPARE(wb.currentSceneId(), a->id());

    // Nothing converted by looking.
    QCOMPARE(m_doc->functions().size(), functions);
    QCOMPARE(c->functions(), QList<quint32>({ a->id(), raw->id(), efx->id(), base->id() }));
}

void Workbench_Test::openAndBackKeepTheSelection()
{
    Scene *a = scene("A", QString());
    Scene *r = scene("R", QString());
    Collection *inner = collection("Inner", { r->id() });
    Collection *c = collection("Outer", { a->id(), inner->id() });

    Workbench wb(m_doc);
    wb.load(c->id());
    ProgrammingOutline *rows = wb.rows();
    selectItem(rows, rows->layerItem(1));
    QVERIFY(wb.m_openBtn->isEnabled());
    wb.openSelected();
    QCOMPARE(wb.loadedFunction(), inner->id());
    QCOMPARE(rows->topLevelItemCount(), 1);
    QVERIFY(!wb.m_backBtn->isHidden());
    QCOMPARE(wb.m_backBtn->text(), QString("↩ Outer"));

    // ↩: what was loaded, the row you opened selected again.
    QVERIFY(wb.goBack());
    QCOMPARE(wb.loadedFunction(), c->id());
    QVERIFY(rows->currentItem() != nullptr);
    QCOMPARE(rows->layerOf(rows->currentItem()), 1);
    QVERIFY(rows->currentItem()->isSelected());
    QVERIFY(wb.m_backBtn->isHidden());
    QVERIFY(!wb.goBack());

    // A double-click opens it too; a scene opens on its own.
    emit rows->itemDoubleClicked(rows->layerItem(1), 0);
    QCOMPARE(wb.loadedFunction(), inner->id());
    wb.open(r->id());
    QCOMPARE(wb.loadedFunction(), r->id());
    QCOMPARE(wb.m_backBtn->text(), QString("↩ Inner"));
    QVERIFY(wb.goBack() && wb.goBack());
    QCOMPARE(wb.loadedFunction(), c->id());

    // Loaded afresh (the navigator): ↩ starts over.
    wb.open(inner->id());
    wb.load(a->id());
    QVERIFY(wb.m_backBtn->isHidden());

    // Where the Workbench was opened from (W3: the Show Tree): ↩ goes there.
    bool returned = false;
    wb.setReturnTo(QStringLiteral("Show Tree"), [&returned]() { returned = true; });
    QCOMPARE(wb.m_backBtn->text(), QString("↩ Show Tree"));
    QVERIFY(wb.goBack());
    QVERIFY(returned);

    // An opened function deleted meanwhile: ↩ skips it.
    wb.setReturnTo(QString(), nullptr);
    wb.load(c->id());
    wb.open(inner->id());
    wb.open(r->id());
    m_doc->deleteFunction(inner->id());
    QVERIFY(wb.goBack());
    QCOMPARE(wb.loadedFunction(), c->id());
}

void Workbench_Test::layersAddRemoveReorderOnTheSharedHistory()
{
    Scene *a = scene("A", "Show");
    Scene *b = scene("B", "Show");
    Scene *d = scene("D", "Show");
    Scene *e = scene("E", "Show");
    Collection *c = collection("0.0.1-Startup", { a->id(), b->id() });
    c->setPath("Show");
    ShowTreeController *st = m_doc->showTree();

    ProgrammingManager mgr(nullptr, m_doc);
    mgr.resize(1400, 900);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    Workbench *wb = mgr.m_workbench;
    mgr.showFunction(c->id());
    QCOMPARE(wb->loadedFunction(), c->id());
    ProgrammingOutline *rows = wb->rows();
    using L = QList<quint32>;

    // + Scene (picked): after the selected layer, one step.
    selectItem(rows, rows->layerItem(0));
    QVERIFY(wb->stack()->addLayers({ d->id() }, 1));
    QCOMPARE(c->functions(), L({ a->id(), d->id(), b->id() }));
    QVERIFY(mgr.undoText().contains("add D"));
    QVERIFY(st->undoShared());
    QCOMPARE(c->functions(), L({ a->id(), b->id() }));
    QVERIFY(st->redoShared());
    QCOMPARE(c->functions(), L({ a->id(), d->id(), b->id() }));
    QTRY_COMPARE(rows->topLevelItemCount(), 3);

    // ⌫ on a layer: out of the collection; the scene stays in the show.
    QApplication::processEvents();
    selectItem(rows, rows->layerItem(1));
    rows->setFocus();
    QTest::keyClick(rows, Qt::Key_Backspace);
    QCOMPARE(c->functions(), L({ a->id(), b->id() }));
    QVERIFY(m_doc->function(d->id()) != nullptr);
    QVERIFY(mgr.undoText().contains("remove D"));
    mgr.slotUndo();
    QCOMPARE(c->functions(), L({ a->id(), d->id(), b->id() }));

    // ▲ ▼: one step each.
    QTRY_COMPARE(rows->topLevelItemCount(), 3);
    selectItem(rows, rows->layerItem(2));
    QVERIFY(wb->m_upBtn->isEnabled() && !wb->m_downBtn->isEnabled());
    wb->m_upBtn->click();
    QCOMPARE(c->functions(), L({ a->id(), b->id(), d->id() }));
    QCOMPARE(rows->layerOf(rows->currentItem()), 1);        // it stays selected
    QVERIFY(mgr.undoText().contains("reorder layers"));
    mgr.slotUndo();
    QCOMPARE(c->functions(), L({ a->id(), d->id(), b->id() }));

    // A scene dragged from Scenes onto a layer: a layer above it.
    QApplication::processEvents();
    ProgrammingOutline *nav = mgr.m_outline;
    nav->rebuild();
    QString plan;
    QVERIFY(dropOn(rows, nav->mimeData({ nav->sceneItem(e->id()) }), rows->layerItem(0), &plan));
    QVERIFY2(plan.contains("Add “E”") && plan.contains("above “A”"), qPrintable(plan));
    QCOMPARE(c->functions(), L({ e->id(), a->id(), d->id(), b->id() }));
    QVERIFY(st->undoShared());
    QCOMPARE(c->functions(), L({ a->id(), d->id(), b->id() }));

    // A layer dragged within: a reorder (to the bottom on empty space).
    QTRY_COMPARE(rows->topLevelItemCount(), 3);
    QVERIFY(dropOn(rows, rows->mimeData({ rows->layerItem(0) }), nullptr, &plan));
    QVERIFY2(plan.contains("bottom"), qPrintable(plan));
    QCOMPARE(c->functions(), L({ d->id(), b->id(), a->id() }));
    QTRY_COMPARE(rows->topLevelItemCount(), 3);
    QVERIFY(dropOn(rows, rows->mimeData({ rows->layerItem(2) }), rows->layerItem(0), &plan));
    QCOMPARE(c->functions(), L({ a->id(), d->id(), b->id() }));

    // The collection into itself: refused, and says why.
    QTreeWidgetItem *self = nav->itemFor(row(ProgrammingOutline::FunctionRow, c->id(), c->id()));
    QVERIFY(self != nullptr);
    QTRY_COMPARE(rows->topLevelItemCount(), 3);
    QVERIFY(!dropOn(rows, nav->mimeData({ self }), rows->layerItem(0), &plan));
    QVERIFY2(plan.startsWith("!") && plan.contains("can't go in"), qPrintable(plan));
}

void Workbench_Test::newEffectsAreMadeOnTheSelectedLayer()
{
    FixtureGroup *g = group("Spots", 3);
    Scene *a = scene("Announcer", "Show/0-Preshow");
    a->addFixtureGroup(g->id());
    Collection *c = collection("0.0.3-Announcer", { a->id() });
    c->setPath("Show/0-Preshow");
    ShowTreeController *st = m_doc->showTree();

    Workbench wb(m_doc);
    wb.load(c->id());
    ProgrammingOutline *rows = wb.rows();
    selectItem(rows, rows->layerItem(0));
    QVERIFY(wb.m_addEfxBtn->toolTip().contains("selected layer"));
    // + Effect ▾ offers an effect look too, beside the effects there.
    QAction *design = wb.findChild<QAction *>("workbenchDesignEffect");
    QVERIFY(design != nullptr && design->isEnabled());
    QVERIFY(design->toolTip().contains("beside"));

    // + EFX: on the layer's fixtures, after it, selected, its editor open.
    wb.addNew(Function::EFXType);
    QCOMPARE(c->functions().size(), 2);
    const quint32 eid = c->functions().at(1);
    EFX *efx = qobject_cast<EFX *>(m_doc->function(eid));
    QVERIFY(efx != nullptr);
    QCOMPARE(efx->fixtures().size(), g->headList().size());      // the group's heads, as laid out
    QVERIFY(!efx->fixtures().isEmpty());
    QCOMPARE(efx->path(true), QString("Show/0-Preshow"));     // filed with the collection
    QCOMPARE(rows->layerOf(rows->currentItem()), 1);
    QCOMPARE(wb.currentPage(), Workbench::FunctionPage);
    QVERIFY(wb.m_functionPage->findChild<EFXEditor *>() != nullptr);

    // Undo: the layer AND the function go; Redo: back, with its id.
    QVERIFY(m_doc->paletteOps()->undoText().contains("new EFX"));
    QVERIFY(st->undoShared());
    QCOMPARE(c->functions(), QList<quint32>({ a->id() }));
    QVERIFY(m_doc->function(eid) == nullptr);
    QVERIFY(st->redoShared());
    QCOMPARE(c->functions(), QList<quint32>({ a->id(), eid }));
    efx = qobject_cast<EFX *>(m_doc->function(eid));
    QVERIFY(efx != nullptr);
    QCOMPARE(efx->fixtures().size(), g->headList().size());

    // + RGB Matrix on the first layer: on its group, right after it.
    QTRY_COMPARE(rows->topLevelItemCount(), 2);
    selectItem(rows, rows->layerItem(0));
    wb.addNew(Function::RGBMatrixType);
    QCOMPARE(c->functions().size(), 3);
    RGBMatrix *mtx = qobject_cast<RGBMatrix *>(m_doc->function(c->functions().at(1)));
    QVERIFY(mtx != nullptr);
    QCOMPARE(mtx->fixtureGroup(), g->id());
    QVERIFY(wb.m_functionPage->findChild<RGBMatrixEditor *>() != nullptr);

    // Nothing selected: at the end, on nothing.
    rows->clearSelection();
    wb.addNew(Function::EFXType);
    QCOMPARE(c->functions().size(), 4);
    EFX *loose = qobject_cast<EFX *>(m_doc->function(c->functions().last()));
    QVERIFY(loose != nullptr && loose->fixtures().isEmpty());

    // + Scene ▸ New empty scene: a scene layer, filed with the collection.
    wb.addNew(Function::SceneType);
    Scene *fresh = qobject_cast<Scene *>(m_doc->function(c->functions().last()));
    QVERIFY(fresh != nullptr);
    QCOMPARE(fresh->path(true), QString("Show/0-Preshow"));
}

void Workbench_Test::newHereMakesAStepOrALayer()
{
    Scene *a = scene("Announcer", "Show/0-Preshow");
    Scene *b = scene("Walk-in", "Show/0-Preshow");
    Collection *c = collection("0.0.3-Announcer", { a->id() });
    c->setPath("Show/0-Preshow");
    ShowTreeController *st = m_doc->showTree();

    // A lone scene: nothing to add to.
    Workbench wb(m_doc);
    wb.load(a->id());
    QCOMPARE(wb.newHereLabel(), QString());
    QCOMPARE(wb.newHere(), Function::invalidId());

    // A collection: a new empty scene, as a layer at the end, filed with it.
    wb.load(c->id());
    QCOMPARE(wb.newHereLabel(), QString("New Scene in 0.0.3-Announcer"));
    const quint32 made = wb.newHere();
    QVERIFY(made != Function::invalidId());
    QCOMPARE(c->functions(), QList<quint32>({ a->id(), made }));
    QCOMPARE(m_doc->function(made)->path(true), QString("Show/0-Preshow"));
    QCOMPARE(wb.rows()->layerOf(wb.rows()->currentItem()), 1);
    QVERIFY(st->undoShared());                  // one step: layer and scene go
    QCOMPARE(c->functions(), QList<quint32>({ a->id() }));
    QVERIFY(m_doc->function(made) == nullptr);

    // A chaser: a new scene as the step after the one walked to.
    Chaser *ch = new Chaser(m_doc);
    ch->setName("Walk");
    m_doc->addFunction(ch);
    ch->setPath("Show/0-Preshow");
    ch->addStep(ChaserStep(a->id()));
    ch->addStep(ChaserStep(b->id()));
    wb.load(ch->id());
    FunctionStack *fs = qobject_cast<FunctionStack *>(wb.stack());
    QVERIFY(fs != nullptr);
    QCOMPARE(fs->step(), 0);
    QCOMPARE(wb.newHereLabel(), QString("New Step After 1 in Walk"));
    const quint32 stepScene = wb.newHere();
    QVERIFY(qobject_cast<Scene *>(m_doc->function(stepScene)) != nullptr);
    QCOMPARE(ch->stepsCount(), 3);
    QCOMPARE(ch->steps().at(1).fid, stepScene);
    QCOMPARE(fs->step(), 1);                    // walked to
    QCOMPARE(m_doc->function(stepScene)->path(true), QString("Show/0-Preshow"));
    QVERIFY(st->undoShared());
    QCOMPARE(ch->stepsCount(), 2);
    QCOMPARE(ch->steps().at(1).fid, b->id());
    QVERIFY(m_doc->function(stepScene) == nullptr);
}

void Workbench_Test::sharedMembersSayWhereElse()
{
    Scene *back = scene("BackStage", "Show");
    Scene *other = scene("Other", "Show");
    Collection *c3 = collection("0.0.3-Announcer", { back->id(), other->id() });
    collection("0.0.4-SHOUT", { back->id() });
    Chaser *ch = new Chaser(m_doc);
    ch->setName("Full Show");
    m_doc->addFunction(ch);
    ch->addStep(ChaserStep(back->id()));

    Workbench wb(m_doc);
    wb.load(c3->id());
    ProgrammingOutline *rows = wb.rows();
    QTreeWidgetItem *shared = rows->layerItem(0);
    QVERIFY2(shared->text(1).contains("also in 2"), qPrintable(shared->text(1)));
    QVERIFY(shared->toolTip(1).contains("0.0.4-SHOUT"));
    QVERIFY(shared->toolTip(1).contains("Full Show (step 1)"));
    QVERIFY(shared->toolTip(1).contains("changes it there too"));
    QVERIFY(!rows->layerItem(1)->text(1).contains("also in"));

    // On its own page: everywhere it is used.
    selectItem(rows, shared);
    QVERIFY(wb.m_sceneSummary->text().contains("Used in: 0.0.3-Announcer"));

    // A chaser is walked a step at a time (W2): step 1's scene, opened up.
    wb.load(ch->id());
    QCOMPARE(rows->topLevelItemCount(), 1);
    QCOMPARE(kindOf(rows->layerItem(0)), int(ProgrammingOutline::SceneRow));
    QVERIFY(rows->layerItem(0)->text(0).startsWith("BackStage"));
    QVERIFY(!wb.m_addSceneBtn->isEnabled());
    QCOMPARE(wb.currentPage(), Workbench::OverviewPage);
    QVERIFY(wb.m_overviewEditor != nullptr);        // its step table
}

void Workbench_Test::whatIsLoadedPlays()
{
    FixtureGroup *g = group("Washes", 2);
    QLCPalette *half = palette(QLCPalette::Dimmer, "Half");
    Scene *a = scene("A", QString());
    a->addFixtureGroup(g->id());
    a->addPalette(half->id());
    Scene *b = scene("B", QString());
    b->setValue(g->fixtureList().first(), 1, 200);
    Collection *c = collection("Look", { a->id() });
    QVERIFY(m_doc->mode() == Doc::Design);

    Workbench wb(m_doc);
    wb.resize(1000, 700);
    wb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wb));
    m_doc->masterTimer()->start();
    wb.load(c->id());
    QTRY_VERIFY(c->isRunning());
    QTRY_VERIFY(a->isRunning());
    QVERIFY(wb.m_onRig->text().contains("On the rig: Look"));

    // A layer added: the collection plays it too.
    QVERIFY(wb.stack()->addLayers({ b->id() }, -1));
    QTRY_VERIFY(b->isRunning());
    QTRY_VERIFY(c->isRunning());

    // Stop / Play.
    QCOMPARE(wb.m_playBtn->text(), QString("Stop"));
    wb.m_playBtn->click();
    QTRY_VERIFY(!c->isRunning());
    QVERIFY(wb.m_onRig->text().contains("Not playing"));
    wb.m_playBtn->click();
    QTRY_VERIFY(c->isRunning());

    // Another load: the last one stops, this one plays.
    wb.load(a->id());
    QTRY_VERIFY(!c->isRunning());
    QTRY_VERIFY(a->isRunning());

    // Hidden (the tab left): nothing plays from here.
    wb.hide();
    QTRY_VERIFY(!a->isRunning());
    m_doc->masterTimer()->stop();
}

void Workbench_Test::goHoldsTheBuildPreview()
{
    FixtureGroup *g = group("Washes", 2);
    QLCPalette *half = palette(QLCPalette::Dimmer, "Half");
    Scene *a = scene("A", QString());
    a->addFixtureGroup(g->id());
    a->addPalette(half->id());
    Scene *b = scene("B", QString());
    b->setValue(g->fixtureList().first(), 1, 200);
    Collection *c = collection("Look", { a->id() });
    // The show: a Show Tree list, one empty cue.
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "Full Show", ShowTree::ListMode::NonTracking);
    st->addCue(listId, -1, "One");
    Chaser *list = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QVERIFY(list != nullptr);
    QVERIFY(m_doc->mode() == Doc::Design);

    Workbench wb(m_doc);
    wb.resize(1000, 700);
    wb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wb));
    m_doc->masterTimer()->start();
    auto stopTimer = qScopeGuard([this]() { m_doc->masterTimer()->stop(); });
    wb.load(c->id());
    QTRY_VERIFY(c->isRunning());

    // GO (OSC, MSC, a key: all through show control) takes the stage.
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = list->id();
    QVERIFY(m_doc->showControl()->execute(go));
    QTRY_VERIFY(!c->isRunning());
    QTRY_VERIFY(list->isRunning());
    QVERIFY2(wb.m_onRig->text().startsWith("■ Held"), qPrintable(wb.m_onRig->text()));
    QCOMPARE(wb.m_playBtn->text(), QString("Play"));

    // Another load while the show runs: still held.
    wb.load(b->id());
    QTest::qWait(100);
    QVERIFY(!b->isRunning());
    QVERIFY(wb.m_onRig->text().startsWith("■ Held"));

    // Play: it previews over the show.
    wb.m_playBtn->click();
    QTRY_VERIFY(b->isRunning());
    QVERIFY(list->isRunning());
    QCOMPARE(wb.m_playBtn->text(), QString("Stop"));
    wb.hide();
    list->stop(FunctionParent::master());
    QTRY_VERIFY(!list->isRunning() && !b->isRunning());
}

/* A pick in the docked Workbench's Show Tree (Workbench::playPicked) takes
   the stage back from a GO's hold: the latest action wins. With nothing
   held, it changes nothing. */
void Workbench_Test::aPickTakesTheStageBackFromAGo()
{
    Scene *b = scene("B", QString());
    FixtureGroup *g = group("Washes", 2);
    b->setValue(g->fixtureList().first(), 1, 200);
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "Full Show", ShowTree::ListMode::NonTracking);
    st->addCue(listId, -1, "One");
    Chaser *list = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QVERIFY(list != nullptr);

    Workbench wb(m_doc);
    wb.resize(1000, 700);
    wb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wb));
    m_doc->masterTimer()->start();
    auto stopTimer = qScopeGuard([this]() { m_doc->masterTimer()->stop(); });
    wb.load(b->id());
    QTRY_VERIFY(b->isRunning());

    // Nothing held: a pick leaves the preview as it is.
    wb.playPicked();
    QVERIFY(b->isRunning());

    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = list->id();
    QVERIFY(m_doc->showControl()->execute(go));
    QTRY_VERIFY(!b->isRunning());
    QTRY_VERIFY(list->isRunning());
    QVERIFY(wb.m_onRig->text().startsWith("■ Held"));

    // The pick: the bench plays again, over the show.
    wb.playPicked();
    QTRY_VERIFY(b->isRunning());
    QVERIFY(list->isRunning());
    QVERIFY(!wb.m_onRig->text().startsWith("■ Held"));
    QCOMPARE(wb.m_playBtn->text(), QString("Stop"));
    wb.hide();
    list->stop(FunctionParent::master());
    QTRY_VERIFY(!list->isRunning() && !b->isRunning());
}

void Workbench_Test::theNavigatorDrivesTheWorkbench()
{
    FixtureGroup *g = group("Steps", 2);
    QLCPalette *blue = palette(QLCPalette::Color, "Blue");
    Scene *s = scene("Steps-Blue", "Show");
    s->addFixtureGroup(g->id());
    s->addPalette(blue->id());
    Collection *c = collection("0.0.2-Preshow", { s->id() });
    c->setPath("Show");

    ProgrammingManager mgr(nullptr, m_doc);
    Workbench *wb = mgr.m_workbench;
    ProgrammingOutline *nav = mgr.m_outline;
    nav->rebuild();

    // A look in a scene opened up in the navigator: the scene loads, the
    // look selected there.
    QVERIFY(nav->selectRow(row(ProgrammingOutline::PaletteRow, s->id(), blue->id())));
    QCOMPARE(wb->loadedFunction(), s->id());
    QCOMPARE(wb->currentPage(), Workbench::LookPage);
    QCOMPARE(mgr.currentPaletteId(), blue->id());
    QCOMPARE(mgr.currentSceneId(), s->id());

    // The collection: loaded; the same look, in its layer, stays in it.
    mgr.showFunction(c->id());
    QCOMPARE(wb->loadedFunction(), c->id());
    QCOMPARE(Row(ProgrammingOutline::rowOf(nav->currentItem())).id, c->id());
    QCOMPARE(mgr.currentSceneId(), Function::invalidId());
    QVERIFY(nav->selectRow(row(ProgrammingOutline::PaletteRow, s->id(), blue->id())));
    QCOMPARE(wb->loadedFunction(), c->id());
    QCOMPARE(mgr.currentPaletteId(), blue->id());
    QCOMPARE(mgr.currentSceneId(), s->id());

    // Opened from the Workbench: the navigator follows, without loading again.
    wb->open(s->id());
    QCOMPARE(Row(ProgrammingOutline::rowOf(nav->currentItem())).id, s->id());
    QVERIFY(!wb->m_backBtn->isHidden());

    // The sections: only the navigator changes.
    mgr.setSection(ProgrammingManager::PalettesSection);
    QCOMPARE(mgr.m_sectionStack->currentIndex(), 0);
    QCOMPARE(wb->loadedFunction(), s->id());
    mgr.setSection(ProgrammingManager::EffectsSection);
    QCOMPARE(mgr.section(), ProgrammingManager::EffectsSection);
    mgr.m_sections->button(ProgrammingManager::ScenesSection)->click();
    QCOMPARE(mgr.section(), ProgrammingManager::ScenesSection);
    QCOMPARE(QSettings().value("programming/buildSection").toInt(), int(ProgrammingManager::ScenesSection));

    // The Effects section lists the classic effects; a click loads one.
    EFX *efx = new EFX(m_doc);
    efx->setName("Ballyhoo");
    m_doc->addFunction(efx);
    mgr.rebuildEffectsNow();      // rebuildEffects() is queued
    QTreeWidgetItem *fx = nullptr;
    for (QTreeWidgetItemIterator it(mgr.m_effectsList); *it; ++it)
        if ((*it)->text(0) == "Ballyhoo")
            fx = *it;
    QVERIFY(fx != nullptr);
    emit mgr.m_effectsList->itemClicked(fx, 0);
    QCOMPARE(wb->loadedFunction(), efx->id());
    QCOMPARE(wb->currentPage(), Workbench::FunctionPage);
}

void Workbench_Test::aRealShowsCollectionsLoadAsLayers()
{
    const QString path = qEnvironmentVariable("QLC_WORKBENCH_SHOW");
    if (path.isEmpty())
        QSKIP("set QLC_WORKBENCH_SHOW (a .qlcc or .qxw, read from a copy) to load a real show's collections");

    // Read a copy: the show itself is never touched.
    QTemporaryDir dir;
    const QString copy = dir.filePath(QStringLiteral("show.") + QFileInfo(path).suffix());
    QVERIFY(QFile::copy(path, copy));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures")));
    if (m_doc->showEmbedding()->readWorkspace(copy))
        m_doc->showEmbedding()->apply(dir.path());
    QFile file(copy);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader r(&file);
    bool loaded = false;
    while (!r.atEnd() && !loaded)
        if (r.readNextStartElement() && r.name() == QLatin1String("Engine"))
            loaded = m_doc->loadXML(r, false);
    QVERIFY(loaded);

    ProgrammingManager mgr(nullptr, m_doc);
    mgr.resize(1700, 980);
    mgr.setSection(ProgrammingManager::ScenesSection);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    Workbench *wb = mgr.m_workbench;
    ShowTreeController *st = m_doc->showTree();

    // Every collection, as QLC+ made it: its members are its layers, in order.
    const int functions = m_doc->functions().size();
    int collections = 0, layers = 0, opened = 0;
    Collection *pick = nullptr;
    for (Function *f : m_doc->functions())
    {
        Collection *c = qobject_cast<Collection *>(f);
        if (c == nullptr || !c->isVisible() || (st != nullptr && st->isGenerated(c->id())))
            continue;
        const QList<quint32> members = c->functions();
        mgr.showFunction(c->id());
        QCOMPARE(wb->loadedFunction(), c->id());
        ProgrammingOutline *rows = wb->rows();
        if (members.isEmpty())
            QCOMPARE(kindOf(rows->topLevelItem(0)), int(ProgrammingOutline::HintRow));
        else
            QCOMPARE(rows->topLevelItemCount(), members.size());
        for (int i = 0; i < members.size(); ++i)
        {
            QTreeWidgetItem *it = rows->layerItem(i);
            QVERIFY(it != nullptr);
            QCOMPARE(it->data(0, ProgrammingOutline::IdRole).toUInt(), members.at(i));
            const bool isScene = qobject_cast<Scene *>(m_doc->function(members.at(i))) != nullptr;
            QCOMPARE(kindOf(it), int(isScene ? ProgrammingOutline::SceneRow : ProgrammingOutline::FunctionRow));
            if (isScene)
                ++opened;
        }
        QCOMPARE(c->functions(), members);                   // nothing converted
        ++collections;
        layers += members.size();
        if (pick == nullptr || members.size() > pick->functions().size())
            pick = c;
    }
    QCOMPARE(m_doc->functions().size(), functions);
    qInfo() << "[WORKBENCH]" << collections << "collections," << layers << "layers," << opened << "scene layers";

    const QString shot = qEnvironmentVariable("QLC_WORKBENCH_SHOT");
    if (shot.isEmpty())
        return;
    QSplitter *split = mgr.findChild<QSplitter *>("programmingSplitter");
    if (split != nullptr)
        split->setSizes({ 380, 1320 });
    if (QSplitter *bench = mgr.findChild<QSplitter *>("workbenchSplitter"))
        bench->setSizes({ 640, 680 });

    // The scene with the most looks, its first look on the editor pane.
    Scene *best = nullptr;
    for (Function *f : m_doc->functions())
        if (Scene *s = qobject_cast<Scene *>(f))
            if (s->isVisible() && (st == nullptr || !st->isGenerated(s->id()))
                && (best == nullptr || s->palettes().size() > best->palettes().size()))
                best = s;
    if (best != nullptr)
    {
        mgr.showFunction(best->id());
        if (!best->palettes().isEmpty())
            wb->selectRow(row(ProgrammingOutline::PaletteRow, best->id(), best->palettes().first()));
        QTest::qWait(80);
        QVERIFY(mgr.grab().save(QString(shot).replace(".png", "-scene.png")));
    }
    if (pick != nullptr)
    {
        mgr.showFunction(pick->id());
        QTest::qWait(80);
        QVERIFY(mgr.grab().save(QString(shot).replace(".png", "-collection.png")));
        // Its first scene layer selected: the scene's page beside the layers.
        for (int i = 0; i < pick->functions().size(); ++i)
            if (qobject_cast<Scene *>(m_doc->function(pick->functions().at(i))) != nullptr)
            {
                selectItem(wb->rows(), wb->rows()->layerItem(i));
                break;
            }
        QTest::qWait(80);
        QVERIFY(mgr.grab().save(QString(shot).replace(".png", "-collection-layer.png")));
    }
}

QTEST_MAIN(Workbench_Test)

/* A pan-only head at @a address: pan is LTP, so which layer wins shows. */
static Fixture *panHead(Doc *doc, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Pan Head");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("1ch");
    QLCChannel *pan = new QLCChannel();
    pan->setName("Pan");
    pan->setGroup(QLCChannel::Pan);
    pan->setControlByte(QLCChannel::MSB);
    def->addChannel(pan);
    mode->insertChannel(pan, 0);
    def->addMode(mode);
    if (!doc->fixtureDefCache()->addFixtureDef(def))
        return nullptr;
    Fixture *fxi = new Fixture(doc);
    fxi->setName("Pan Head");
    fxi->setFixtureDefinition(def, mode);
    fxi->setAddress(address);
    return doc->addFixture(fxi) ? fxi : nullptr;
}

/* One engine tick (what the master timer's thread does), then universe 1's
   value at @a address. */
static int tickAndRead(Doc *doc, int address)
{
    doc->masterTimer()->timerTick();
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    ua.first()->processFaders();
    const int v = uchar(ua.first()->preGMValues().at(address));
    doc->inputOutputMap()->releaseUniverses(false);
    return v;
}

void Workbench_Test::operateRunsWhatIsLoadedAndStopsOnlyItsOwn()
{
    FixtureGroup *g = group("Washes", 2);
    const quint32 f1 = g->fixtureList().at(0), f2 = g->fixtureList().at(1);
    Scene *a = scene("A", QString());
    a->setValue(f1, 0, 180);
    Scene *elsewhere = scene("Elsewhere", QString());
    elsewhere->setValue(f2, 0, 90);
    const FunctionParent vcButton(FunctionParent::ManualVCWidget, 7);

    m_doc->setMode(Doc::Operate);
    Workbench wb(m_doc);
    wb.resize(1000, 700);
    wb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wb));

    // Operate: what is loaded runs, as the show's output.
    wb.load(a->id());
    QTRY_COMPARE(tickAndRead(m_doc, 0), 180);
    QVERIFY(a->isRunning());
    QCOMPARE(wb.m_operateFunction, a->id());
    QVERIFY(wb.m_onRig->text().contains("Live: A"));

    // Something started elsewhere (a VC button) plays alongside.
    elsewhere->start(m_doc->masterTimer(), vcButton);
    QTRY_COMPARE(tickAndRead(m_doc, 3), 90);

    // To Design: the Workbench leaves what it didn't start running; what
    // is loaded carries on as its preview.
    m_doc->setMode(Doc::Design);
    for (int i = 0; i < 5; ++i)
        tickAndRead(m_doc, 0);
    QVERIFY(elsewhere->isRunning());
    QCOMPARE(tickAndRead(m_doc, 3), 90);
    QVERIFY(a->isRunning());
    QCOMPARE(wb.m_previewFunction, a->id());

    // Back to Operate: the preview is the live run; hidden, only that stops.
    m_doc->setMode(Doc::Operate);
    QCOMPARE(wb.m_operateFunction, a->id());
    wb.hide();
    QTRY_VERIFY(tickAndRead(m_doc, 0) == 0 && !a->isRunning());
    QVERIFY(elsewhere->isRunning());
    QCOMPARE(tickAndRead(m_doc, 3), 90);

    // Loaded while already running from elsewhere: hiding the Workbench
    // must leave it to the VC button that started it.
    wb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wb));
    wb.load(elsewhere->id());
    QCOMPARE(wb.m_operateFunction, elsewhere->id());
    wb.hide();
    for (int i = 0; i < 5; ++i)
        tickAndRead(m_doc, 3);
    QVERIFY(elsewhere->isRunning());
    elsewhere->stop(vcButton);
    tickAndRead(m_doc, 0);
}

void Workbench_Test::aPreviewNeverRunsAScript()
{
    FixtureGroup *g = group("Washes", 2);
    const quint32 f1 = g->fixtureList().at(0), f2 = g->fixtureList().at(1);
    Scene *a = scene("A", QString());
    a->setValue(f1, 0, 150);
    // What the Script would do: wash 2 (address 3) to 200, held (a Script
    // that ends lets its channels go).
    Script *scr = new Script(m_doc);
    scr->setName("Act");
    QVERIFY(scr->setData(QString("setfixture:%1 ch:0 val:200\nwait:60s").arg(f2)));
    m_doc->addFunction(scr);
    Collection *c = new Collection(m_doc);
    c->setName("Look with a script");
    m_doc->addFunction(c);
    c->addFunction(a->id());
    c->addFunction(scr->id());
    QVERIFY(m_doc->mode() == Doc::Design);

    Workbench wb(m_doc);
    wb.resize(1000, 700);
    wb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wb));

    // A collection previewed: its scene plays, its Script does nothing.
    wb.load(c->id());
    QCOMPARE(wb.m_previewFunction, c->id());
    QTRY_COMPARE(tickAndRead(m_doc, 0), 150);
    for (int i = 0; i < 10; ++i)
        QCOMPARE(tickAndRead(m_doc, 3), 0);
    QTRY_VERIFY(scr->isRunning() == false);

    // A chaser walked to a Script step: the step is previewed on its own.
    Chaser *ch = new Chaser(m_doc);
    ch->setName("Walk");
    m_doc->addFunction(ch);
    ch->addStep(ChaserStep(scr->id()));
    ch->addStep(ChaserStep(a->id()));
    wb.load(ch->id());
    QCOMPARE(wb.m_previewFunction, scr->id());
    for (int i = 0; i < 10; ++i)
        QCOMPARE(tickAndRead(m_doc, 3), 0);

    // ...and a chaser whose step is the collection, walked to it.
    Chaser *ch2 = new Chaser(m_doc);
    ch2->setName("Walk 2");
    m_doc->addFunction(ch2);
    ch2->addStep(ChaserStep(c->id()));
    wb.load(ch2->id());
    QCOMPARE(wb.m_previewFunction, c->id());
    QTRY_COMPARE(tickAndRead(m_doc, 0), 150);
    for (int i = 0; i < 10; ++i)
        QCOMPARE(tickAndRead(m_doc, 3), 0);
    wb.hide();
    for (int i = 0; i < 5; ++i)
        tickAndRead(m_doc, 0);

    // The control: the same collection from a VC button runs its Script.
    const FunctionParent vcButton(FunctionParent::ManualVCWidget, 9);
    c->start(m_doc->masterTimer(), vcButton);
    QTRY_COMPARE(tickAndRead(m_doc, 3), 200);
    c->stop(vcButton);
    tickAndRead(m_doc, 0);
}

void Workbench_Test::designPreviewLeavesAnotherRunAlone()
{
    FixtureGroup *g = group("Washes", 1);
    const quint32 f1 = g->fixtureList().at(0);
    Scene *a = scene("A", QString());
    a->setValue(f1, 0, 150);
    const FunctionParent runTab(FunctionParent::AutoVCWidget, 42);
    QVERIFY(m_doc->mode() == Doc::Design);

    // Already playing (the Run tab) when the Workbench loads it to preview.
    a->start(m_doc->masterTimer(), runTab);
    QTRY_COMPARE(tickAndRead(m_doc, 0), 150);

    Workbench wb(m_doc);
    wb.resize(1000, 700);
    wb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wb));
    wb.load(a->id());
    QCOMPARE(wb.m_previewFunction, a->id());

    // Hidden: the preview lets go; the Run tab's run carries on.
    wb.hide();
    QCOMPARE(wb.m_previewFunction, Function::invalidId());
    for (int i = 0; i < 5; ++i)
        tickAndRead(m_doc, 0);
    QVERIFY(a->isRunning());
    QCOMPARE(tickAndRead(m_doc, 0), 150);

    // ...and stopping it there still stops it.
    a->stop(runTab);
    QTRY_VERIFY(tickAndRead(m_doc, 0) == 0 && !a->isRunning());
}

void Workbench_Test::theLowerLayerWinsAndFollowsAReorder()
{
    Fixture *head = panHead(m_doc, 100);
    QVERIFY(head != nullptr);
    Scene *upper = scene("Upper", QString());
    upper->setValue(head->id(), 0, 40);
    Scene *lower = scene("Lower", QString());
    lower->setValue(head->id(), 0, 220);
    Collection *c = collection("Stack", { upper->id(), lower->id() });
    QVERIFY(m_doc->mode() == Doc::Design);

    Workbench wb(m_doc);
    wb.resize(1000, 700);
    wb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wb));
    wb.load(c->id());
    ProgrammingOutline *rows = wb.rows();
    QCOMPARE(rows->topLevelItemCount(), 2);

    // Both set the pan: the lower layer (added last) wins.
    QTRY_COMPARE(tickAndRead(m_doc, 100), 220);
    QVERIFY(upper->isRunning() && lower->isRunning());

    // ▲ on the lower layer: Upper is now the lower one, and the DMX follows.
    selectItem(rows, rows->layerItem(1));
    QVERIFY(wb.m_upBtn->isEnabled());
    wb.m_upBtn->click();
    QCOMPARE(c->functions(), QList<quint32>({ lower->id(), upper->id() }));
    QTRY_COMPARE(tickAndRead(m_doc, 100), 40);

    // ⌘Z: the order back, and its output.
    QVERIFY(m_doc->showTree()->undoShared());
    QCOMPARE(c->functions(), QList<quint32>({ upper->id(), lower->id() }));
    QTRY_COMPARE(tickAndRead(m_doc, 100), 220);

    // Hidden: it stops (pan is LTP: the head holds where it was).
    wb.hide();
    auto stopped = [&]() { tickAndRead(m_doc, 100); return !c->isRunning() && !lower->isRunning(); };
    QTRY_VERIFY(stopped());
}

/*
  Q Light Controller Plus - Test Unit
  programmingoutline_test.cpp

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
#include <QTreeWidgetItemIterator>
#include <QXmlStreamReader>
#include <QTemporaryDir>
#include <QMimeData>
#include <QDropEvent>
#include <QDataStream>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QFile>
#include <QSettings>

#define protected public
#define private public
#include "programmingmanager.h"
#include "programmingoutline.h"
#include "workbench.h"
#include "workbenchstack.h"
#include "scenegrouplooks.h"
#include "fixtureconsole.h"
#include "lookeditor.h"
#include "fadetimespin.h"
#undef private
#undef protected

#include "functionstreewidget.h"
#include "fixturegroupsource.h"
#include "showtreecontroller.h"
#include "qlcfixturedefcache.h"
#include "scenetransition.h"
#include "showembedding.h"
#include "speedmasters.h"
#include "fixturegroup.h"
#include "paletteops.h"
#include "collection.h"
#include "qlcpalette.h"
#include "scenevalue.h"
#include "fixture.h"
#include "scene.h"
#include "inputoutputmap.h"
#include "doc.h"

#include "programmingoutline_test.h"

using Row = ProgrammingOutline::Row;

static Row row(int kind, quint32 scene, quint32 id)
{
    Row r;
    r.kind = kind;
    r.scene = scene;
    r.id = id;
    return r;
}

static QMimeData *idsMime(const char *type, const QList<quint32> &ids)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    for (quint32 id : ids)
        out << id;
    QMimeData *mime = new QMimeData;
    mime->setData(QString::fromLatin1(type), data);
    return mime;
}

/* A drop as the view delivers it (viewport coordinates), on @a onto's row. */
static bool drop(ProgrammingOutline *o, QMimeData *mime, QTreeWidgetItem *onto,
                 Qt::KeyboardModifiers mods = Qt::NoModifier, QString *plan = nullptr)
{
    const QPoint pos = o->visualItemRect(onto).center();
    if (plan != nullptr)
        *plan = o->dropPlan(mime, pos, mods & Qt::AltModifier);
    QDropEvent ev(QPointF(pos), Qt::CopyAction | Qt::MoveAction, mime, Qt::LeftButton, mods);
    o->dropEvent(&ev);
    const bool ok = ev.isAccepted();
    delete mime;
    QCoreApplication::processEvents();      // the rebuild after an edit
    return ok;
}

static quint32 s_address = 0;

void ProgrammingOutline_Test::init()
{
    m_doc = new Doc(this);
    m_doc->inputOutputMap()->addUniverse();
    m_doc->inputOutputMap()->addUniverse();
    s_address = 0;
}

void ProgrammingOutline_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

Scene *ProgrammingOutline_Test::scene(const QString &name, const QString &path)
{
    Scene *s = new Scene(m_doc);
    s->setName(name);
    m_doc->addFunction(s);
    s->setPath(path);
    return s;
}

QLCPalette *ProgrammingOutline_Test::palette(int type, const QString &name)
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

FixtureGroup *ProgrammingOutline_Test::group(const QString &name, int count)
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

void ProgrammingOutline_Test::everySceneAndWhatIsInItIsARow()
{
    // Nothing yet: one line that says so.
    {
        ProgrammingOutline empty(m_doc);
        empty.rebuild();
        QCOMPARE(empty.topLevelItemCount(), 1);
        QCOMPARE(empty.topLevelItem(0)->data(0, ProgrammingOutline::KindRole).toInt(),
                 int(ProgrammingOutline::HintRow));
        QVERIFY(empty.topLevelItem(0)->text(0).startsWith("No scenes yet"));
    }

    FixtureGroup *steps = group("Steps", 2);
    Fixture *solo = new Fixture(m_doc);
    solo->setName("Solo");
    solo->setChannels(3);
    solo->setAddress(400);
    QVERIFY(m_doc->addFixture(solo));
    QLCPalette *blue = palette(QLCPalette::Color, "Blue");
    QLCPalette *half = palette(QLCPalette::Dimmer, "Half");
    QLCPalette *wave = palette(QLCPalette::Effect, "Wave");
    QLCPalette *red = palette(QLCPalette::Color, "Red");

    Scene *base = scene("Base -Washes", "Show/Base");
    base->addFixtureGroup(steps->id());
    base->addFixture(solo->id());
    base->addPalette(blue->id());
    base->addPalette(half->id());
    base->addPalette(wave->id());
    base->addPalette(red->id());        // under the effect: its own
    Scene *pre = scene("Preshow", "Show/0-Preshow");
    Scene *top = scene("Loose", QString());
    Collection *coll = new Collection(m_doc);
    coll->setName("0.0.1-Startup");
    m_doc->addFunction(coll);
    coll->setPath("Show/Base");

    ProgrammingManager mgr(nullptr, m_doc);
    ProgrammingOutline *o = mgr.m_outline;
    o->rebuild();

    // The line over the outline says what it is.
    QVERIFY(mgr.m_outlineHint->text().contains("Your scene folders"));
    QVERIFY(mgr.m_outlineHint->text().contains("drag rows between scenes"));

    // Show -> Base, 0-Preshow; the loose scene at the top.
    QTreeWidgetItem *show = nullptr;
    for (int i = 0; i < o->topLevelItemCount(); ++i)
        if (o->topLevelItem(i)->text(0) == "Show")
            show = o->topLevelItem(i);
    QVERIFY(show != nullptr);
    QCOMPARE(show->childCount(), 2);
    QCOMPARE(show->child(0)->text(0), QString("0-Preshow"));     // numbers as numbers, A-Z
    QCOMPARE(show->child(1)->text(0), QString("Base"));
    QVERIFY(o->sceneItem(top->id()) != nullptr && o->sceneItem(top->id())->parent() == nullptr);
    QVERIFY(o->sceneItem(pre->id())->parent() == show->child(0));

    // Base -Washes: targets, then looks in order, Red under Wave.
    QTreeWidgetItem *b = o->sceneItem(base->id());
    QVERIFY(b != nullptr && b->parent() == show->child(1));
    QStringList kids;
    for (int i = 0; i < b->childCount(); ++i)
        kids << b->child(i)->text(0);
    QCOMPARE(kids, QStringList({ "Steps", "Solo", "Blue", "Half", "Wave" }));
    QTreeWidgetItem *waveRow = o->itemFor(row(ProgrammingOutline::PaletteRow, base->id(), wave->id()));
    QVERIFY(waveRow != nullptr && waveRow->childCount() == 1);
    QCOMPARE(waveRow->child(0)->text(0), QString("Red"));
    QVERIFY(waveRow->child(0)->toolTip(0).contains("effect above"));
    QCOMPARE(ProgrammingOutline::rowOf(waveRow->child(0)).scene, base->id());

    // The collection filed beside it: a full row (W1), dragged onto the
    // Workbench as a layer, loaded there on a click.
    QTreeWidgetItem *other = o->itemFor(row(ProgrammingOutline::FunctionRow, coll->id(), coll->id()));
    QVERIFY(other != nullptr && other->parent() == show->child(1));
    QVERIFY(other->flags() & Qt::ItemIsDragEnabled);
    QVERIFY(other->text(1).contains("collection"));
    QVERIFY(other->toolTip(0).contains("Workbench"));

    // Folders open, scenes closed, by default.
    QVERIFY(show->isExpanded());
    QVERIFY(!b->isExpanded());
}

void ProgrammingOutline_Test::badgesSayWhatAndHoverSaysWhy()
{
    FixtureGroup *steps = group("Steps", 2);
    m_doc->fixture(steps->fixtureList().first())->setPatched(false);
    QLCPalette *blue = palette(QLCPalette::Color, "Blue");
    Scene *s = scene("Base", "Show");
    s->addFixtureGroup(steps->id());
    s->addPalette(blue->id());
    s->setPaletteFade(blue->id(), 1500, -1);
    SceneTransition t;
    t.shape = SceneTransition::Shape(1);
    t.time = 2000;
    s->setTransitionIn(t);
    s->setSpeedMaster(SpeedMasters::Master1);
    s->setValue(steps->fixtureList().last(), 0, 200);

    ProgrammingOutline o(m_doc);
    o.rebuild();
    QTreeWidgetItem *it = o.sceneItem(s->id());
    const QString badges = it->text(ProgrammingOutline::BadgeCol);
    const QString tip = it->toolTip(ProgrammingOutline::BadgeCol);
    QVERIFY2(badges.contains("⚠"), qPrintable(badges));
    QVERIFY(tip.contains("not patched"));
    QVERIFY(badges.contains("▣") && tip.contains("raw channel value"));
    QVERIFY(badges.contains("⇄") && tip.contains("transition"));
    QVERIFY(badges.contains("⏱") && tip.contains("speed master"));
    QVERIFY(!badges.contains("VC"));
    // The name's tooltip says what the row is, then the same details.
    QVERIFY(it->toolTip(0).contains("Scene “Base”"));
    QVERIFY(it->toolTip(0).contains("not patched"));

    QTreeWidgetItem *g = o.itemFor(row(ProgrammingOutline::GroupRow, s->id(), steps->id()));
    QVERIFY(g->text(1).startsWith("⚠"));
    QVERIFY(g->toolTip(1).contains("not patched"));
    QTreeWidgetItem *look = o.itemFor(row(ProgrammingOutline::PaletteRow, s->id(), blue->id()));
    QVERIFY(look->text(1).contains("◷"));
    QVERIFY(look->toolTip(1).contains("1.5"));
}

void ProgrammingOutline_Test::aClickedRowFillsThePage()
{
    FixtureGroup *steps = group("Steps", 2);
    Fixture *solo = new Fixture(m_doc);
    solo->setName("Solo");
    solo->setChannels(3);
    solo->setAddress(400);
    QVERIFY(m_doc->addFixture(solo));
    QLCPalette *blue = palette(QLCPalette::Color, "Blue");
    Scene *s = scene("Base", "Show");
    s->addFixtureGroup(steps->id());
    s->addFixture(solo->id());
    s->addPalette(blue->id());
    Scene *other = scene("Other", "Show");

    ProgrammingManager mgr(nullptr, m_doc);
    ProgrammingOutline *o = mgr.m_outline;
    o->rebuild();
    QCOMPARE(mgr.m_workbench->currentPage(), Workbench::EmptyPage);
    QCOMPARE(mgr.currentSceneId(), Function::invalidId());

    // A scene: its page, with its timing (the canvas's row, lists hidden).
    QVERIFY(o->selectRow(row(ProgrammingOutline::SceneRow, s->id(), s->id())));
    QCOMPARE(mgr.m_workbench->currentPage(), Workbench::ScenePage);
    QCOMPARE(mgr.currentSceneId(), s->id());
    QCOMPARE(mgr.currentPaletteId(), QLCPalette::invalidId());
    QCOMPARE(mgr.m_workbench->m_sceneHeading->text(), QString("Base"));
    QVERIFY(mgr.m_workbench->m_sceneSummary->text().contains("In Show"));
    QVERIFY(mgr.m_workbench->m_sceneSummary->text().contains("Fired by"));
    QVERIFY(mgr.m_workbench->m_canvas != nullptr && mgr.m_workbench->m_scenePage->isAncestorOf(mgr.m_workbench->m_canvas));
    QVERIFY(mgr.m_workbench->m_canvas->findChild<FadeTimeSpin *>("sceneFadeIn") != nullptr);
    QVERIFY(mgr.m_workbench->m_canvas->m_lookList->isHidden() && mgr.m_workbench->m_canvas->m_targetList->isHidden());

    // The Workbench has it: its one layer, opened up.
    QCOMPARE(mgr.m_workbench->loadedFunction(), s->id());
    QVERIFY(mgr.m_workbench->rows()->sceneItem(s->id()) != nullptr);

    // A look: the Look Editor fills the page; the scene is the context.
    QVERIFY(o->selectRow(row(ProgrammingOutline::PaletteRow, s->id(), blue->id())));
    QCOMPARE(mgr.m_workbench->currentPage(), Workbench::LookPage);
    QCOMPARE(mgr.currentPaletteId(), blue->id());
    QCOMPARE(mgr.currentSceneId(), s->id());

    // A group: its members' channels.
    QVERIFY(o->selectRow(row(ProgrammingOutline::GroupRow, s->id(), steps->id())));
    QCOMPARE(mgr.m_workbench->currentPage(), Workbench::TargetPage);
    QCOMPARE(mgr.m_workbench->m_selectedFixtures, steps->fixtureList());
    QVERIFY(mgr.m_workbench->m_targetHeading->text().contains("Steps"));
    QVERIFY(!mgr.m_workbench->m_targetLayoutBtn->isHidden());
    // A fixed fixture: no layout button.
    QVERIFY(o->selectRow(row(ProgrammingOutline::FixtureRow, s->id(), solo->id())));
    QCOMPARE(mgr.m_workbench->m_selectedFixtures, QList<quint32>({ solo->id() }));
    QVERIFY(mgr.m_workbench->m_targetLayoutBtn->isHidden());

    // Another scene's row: that scene.
    QVERIFY(o->selectRow(row(ProgrammingOutline::SceneRow, other->id(), other->id())));
    QCOMPARE(mgr.currentSceneId(), other->id());

    // A folder: the Workbench keeps what it has.
    for (QTreeWidgetItemIterator it(o); *it; ++it)
        if ((*it)->text(0) == "Show")
        {
            o->setCurrentItem(*it);
            break;
        }
    QCOMPARE(mgr.currentSceneId(), other->id());

    // A scene removed while loaded: the Workbench lets go of it.
    QVERIFY(o->selectRow(row(ProgrammingOutline::SceneRow, other->id(), other->id())));
    m_doc->deleteFunction(other->id());
    QCOMPARE(mgr.currentSceneId(), Function::invalidId());
    QCOMPARE(mgr.m_workbench->loadedFunction(), Function::invalidId());
    QCOMPARE(mgr.m_workbench->currentPage(), Workbench::EmptyPage);
}

void ProgrammingOutline_Test::rowsMoveBetweenScenesAndOptionCopies()
{
    FixtureGroup *steps = group("Steps", 2);
    Fixture *solo = new Fixture(m_doc);
    solo->setName("Solo");
    solo->setChannels(3);
    solo->setAddress(400);
    QVERIFY(m_doc->addFixture(solo));
    QLCPalette *blue = palette(QLCPalette::Color, "Blue");
    QLCPalette *half = palette(QLCPalette::Dimmer, "Half");
    QLCPalette *wave = palette(QLCPalette::Effect, "Wave");
    QLCPalette *red = palette(QLCPalette::Color, "Red");
    QLCPalette *amber = palette(QLCPalette::Color, "Amber");
    Scene *a = scene("A", "Show");
    a->addFixtureGroup(steps->id());
    a->addFixture(solo->id());
    a->setValue(solo->id(), 1, 77);
    a->addPalette(blue->id());
    a->setPaletteFade(blue->id(), 1200, 3400);
    a->addPalette(half->id());
    a->addPalette(wave->id());
    a->addPalette(red->id());
    Scene *b = scene("B", "Show");
    b->addPalette(amber->id());

    ProgrammingManager mgr(nullptr, m_doc);
    mgr.resize(1300, 900);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    ProgrammingOutline *o = mgr.m_outline;
    o->rebuild();
    o->expandEverything();
    auto item = [&](int kind, Scene *s, quint32 id) { return o->itemFor(row(kind, s->id(), id)); };

    // Blue to B: moved, the hint said so first, its fade with it.
    QString plan;
    QVERIFY(drop(o, o->mimeData({ item(ProgrammingOutline::PaletteRow, a, blue->id()) }),
                 o->sceneItem(b->id()), Qt::NoModifier, &plan));
    QCOMPARE(plan, QString("Move Blue to “B” (hold ⌥ to copy)"));
    QVERIFY(!a->palettes().contains(blue->id()));
    QCOMPARE(b->palettes(), QList<quint32>({ amber->id(), blue->id() }));
    QCOMPARE(b->paletteFadeIn(blue->id()), 1200);
    QCOMPARE(b->paletteFadeOut(blue->id()), 3400);
    // Selected where it went.
    QTRY_VERIFY(o->currentItem() != nullptr
                && ProgrammingOutline::rowOf(o->currentItem()) == row(ProgrammingOutline::PaletteRow, b->id(), blue->id()));

    // ⌥: Half copied, still in A.
    o->expandEverything();
    QVERIFY(drop(o, o->mimeData({ item(ProgrammingOutline::PaletteRow, a, half->id()) }),
                 o->sceneItem(b->id()), Qt::AltModifier, &plan));
    QVERIFY(plan.startsWith("Copy Half"));
    QVERIFY(a->palettes().contains(half->id()));
    QVERIFY(b->palettes().contains(half->id()));

    // The effect brings its own colour, and they stay nested.
    o->expandEverything();
    QVERIFY(drop(o, o->mimeData({ item(ProgrammingOutline::PaletteRow, a, wave->id()) }),
                 o->sceneItem(b->id())));
    QVERIFY(!a->palettes().contains(wave->id()) && !a->palettes().contains(red->id()));
    QCOMPARE(b->palettes().mid(b->palettes().indexOf(wave->id())), QList<quint32>({ wave->id(), red->id() }));
    QVERIFY(QLCPalette::isEffectScoped(m_doc, b->palettes(), red->id()));

    // A group target moves; a fixed one with its raw values.
    o->expandEverything();
    QVERIFY(drop(o, o->mimeData({ item(ProgrammingOutline::GroupRow, a, steps->id()),
                                  item(ProgrammingOutline::FixtureRow, a, solo->id()) }),
                 o->sceneItem(b->id())));
    QVERIFY(a->fixtureGroups().isEmpty() && a->fixtures().isEmpty() && a->values().isEmpty());
    QCOMPARE(b->fixtureGroups(), QList<quint32>({ steps->id() }));
    QCOMPARE(b->fixtures(), QList<quint32>({ solo->id() }));
    QCOMPARE(b->value(solo->id(), 1), uchar(77));

    // Within B: Blue dropped on Amber goes above it (lower rows win).
    o->expandEverything();
    QVERIFY(drop(o, o->mimeData({ item(ProgrammingOutline::PaletteRow, b, blue->id()) }),
                 item(ProgrammingOutline::PaletteRow, b, amber->id()), Qt::NoModifier, &plan));
    QVERIFY(plan.startsWith("Reorder"));
    QVERIFY(b->palettes().indexOf(blue->id()) < b->palettes().indexOf(amber->id()));

    // A copy into its own scene, a target into its own scene: refused.
    o->expandEverything();
    QVERIFY(!drop(o, o->mimeData({ item(ProgrammingOutline::PaletteRow, b, blue->id()) }),
                  o->sceneItem(b->id()), Qt::AltModifier));
    QVERIFY(!drop(o, o->mimeData({ item(ProgrammingOutline::GroupRow, b, steps->id()) }),
                  o->sceneItem(b->id())));
    // Onto a folder: not a scene.
    QTreeWidgetItem *folder = o->sceneItem(b->id())->parent();
    QVERIFY(o->dropPlan(o->mimeData({ item(ProgrammingOutline::PaletteRow, b, blue->id()) }),
                        o->visualItemRect(folder).center(), false).startsWith("!"));
}

void ProgrammingOutline_Test::sourcesDropOntoAScene()
{
    FixtureGroup *steps = group("Steps", 2);
    QLCPalette *blue = palette(QLCPalette::Color, "Blue");
    QLCPalette *red = palette(QLCPalette::Color, "Red");
    QLCPalette *wave = palette(QLCPalette::Effect, "Wave");
    Scene *s = scene("Base", "Show");
    s->addPalette(wave->id());

    ProgrammingManager mgr(nullptr, m_doc);
    mgr.resize(1300, 900);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    ProgrammingOutline *o = mgr.m_outline;
    o->rebuild();
    o->expandEverything();

    // A palette on the scene: a base look, before the effect.
    QString plan;
    QVERIFY(drop(o, idsMime(FunctionsTreeWidget::paletteDragMimeType(), { blue->id() }),
                 o->sceneItem(s->id()), Qt::NoModifier, &plan));
    QCOMPARE(plan, QString("Add to “Base”"));
    QCOMPARE(s->palettes(), QList<quint32>({ blue->id(), wave->id() }));
    // On the effect: its own.
    o->expandEverything();
    QVERIFY(drop(o, idsMime(FunctionsTreeWidget::paletteDragMimeType(), { red->id() }),
                 o->itemFor(row(ProgrammingOutline::PaletteRow, s->id(), wave->id())), Qt::NoModifier, &plan));
    QVERIFY(plan.contains("effect “Wave”'s own"));
    QCOMPARE(s->palettes(), QList<quint32>({ blue->id(), wave->id(), red->id() }));
    // A group: a dynamic target.
    QVERIFY(drop(o, idsMime(FixtureGroupSource::fixtureGroupMimeType(), { steps->id() }),
                 o->sceneItem(s->id())));
    QCOMPARE(s->fixtureGroups(), QList<quint32>({ steps->id() }));
    // On a folder, or nowhere: nothing.
    QTreeWidgetItem *folder = o->sceneItem(s->id())->parent();
    QVERIFY(!drop(o, idsMime(FunctionsTreeWidget::paletteDragMimeType(), { blue->id() }), folder));
    // One step each on the history.
    QVERIFY(m_doc->showTree()->undoShared());
    QVERIFY(s->fixtureGroups().isEmpty());
}

void ProgrammingOutline_Test::editsAreStepsOnTheSharedHistory()
{
    FixtureGroup *steps = group("Steps", 2);
    Fixture *solo = new Fixture(m_doc);
    solo->setName("Solo");
    solo->setChannels(3);
    solo->setAddress(400);
    QVERIFY(m_doc->addFixture(solo));
    QLCPalette *blue = palette(QLCPalette::Color, "Blue");
    QLCPalette *amber = palette(QLCPalette::Color, "Amber");
    Scene *a = scene("A", "Show");
    a->addFixtureGroup(steps->id());
    a->addFixture(solo->id());
    a->addPalette(blue->id());
    Scene *b = scene("B", "Show");
    b->addPalette(amber->id());
    ShowTreeController *st = m_doc->showTree();
    PaletteOps *ops = m_doc->paletteOps();

    ProgrammingManager mgr(nullptr, m_doc);
    ProgrammingOutline *o = mgr.m_outline;
    o->rebuild();

    // A move: one step, both scenes back on undo, again on redo.
    QVERIFY(o->moveRows({ row(ProgrammingOutline::PaletteRow, a->id(), blue->id()) }, b->id(),
                        QLCPalette::invalidId(), false));
    QCOMPARE(mgr.undoText(), QString("move Blue to B"));
    QVERIFY(!a->palettes().contains(blue->id()));
    mgr.slotUndo();
    QCOMPARE(a->palettes(), QList<quint32>({ blue->id() }));
    QCOMPARE(b->palettes(), QList<quint32>({ amber->id() }));
    QCOMPARE(mgr.redoText(), QString("move Blue to B"));
    mgr.slotRedo();
    QVERIFY(!a->palettes().contains(blue->id()));
    QVERIFY(b->palettes().contains(blue->id()));
    QVERIFY(st->undoShared());              // the Show Tree's ⌘Z reaches it too
    QVERIFY(a->palettes().contains(blue->id()));

    // A rename (F2 commits it as an item edit): undone, the name comes back.
    QTreeWidgetItem *aRow = o->sceneItem(a->id());
    aRow->setText(0, "01-01.00-Alpha");
    QCOMPARE(a->name(), QString("01-01.00-Alpha"));
    QVERIFY(st->undoShared());
    QCOMPARE(a->name(), QString("A"));

    // Removing rows: one step for the lot.
    o->rebuild();
    QVERIFY(o->removeRows({ row(ProgrammingOutline::GroupRow, a->id(), steps->id()),
                            row(ProgrammingOutline::PaletteRow, b->id(), amber->id()) }));
    QVERIFY(a->fixtureGroups().isEmpty() && b->palettes().isEmpty());
    QVERIFY(st->undoShared());
    QCOMPARE(a->fixtureGroups(), QList<quint32>({ steps->id() }));
    QCOMPARE(b->palettes(), QList<quint32>({ amber->id() }));

    // The page's timing: one step (a rename made in Functions meanwhile stays).
    QVERIFY(o->selectRow(row(ProgrammingOutline::SceneRow, a->id(), a->id())));
    FadeTimeSpin *fadeIn = mgr.m_workbench->m_canvas->findChild<FadeTimeSpin *>("sceneFadeIn");
    QVERIFY(fadeIn != nullptr);
    fadeIn->setMs(2500);
    emit fadeIn->msChanged(2500);
    QCOMPARE(a->fadeInSpeed(), uint(2500));
    QVERIFY(mgr.undoText().startsWith("edit A"));
    a->setName("Renamed elsewhere");
    mgr.slotUndo();
    QCOMPARE(a->fadeInSpeed(), uint(0));
    QCOMPARE(a->name(), QString("Renamed elsewhere"));
    mgr.slotRedo();
    QCOMPARE(a->fadeInSpeed(), uint(2500));

    // A fader drag on a fixed target: many ticks, one step.
    QVERIFY(o->selectRow(row(ProgrammingOutline::FixtureRow, a->id(), solo->id())));
    QCOMPARE(mgr.m_workbench->currentPage(), Workbench::TargetPage);
    const quint64 before = ops->undoStamp();
    for (int v = 10; v <= 100; v += 10)
        mgr.m_workbench->slotFixtureValueChanged(solo->id(), 0, uchar(v));
    QCOMPARE(a->value(solo->id(), 0), uchar(100));
    QVERIFY(ops->undoStamp() > before);
    mgr.slotUndo();
    QCOMPARE(a->value(solo->id(), 0), uchar(0));
    QVERIFY(mgr.undoText().startsWith("edit A"));   // the fade, before the drag
}

void ProgrammingOutline_Test::expandCollapseAndOptionClick()
{
    QLCPalette *wave = palette(QLCPalette::Effect, "Wave");
    QLCPalette *red = palette(QLCPalette::Color, "Red");
    Scene *a = scene("A", "Show/Act 1");
    a->addPalette(wave->id());
    a->addPalette(red->id());
    Scene *b = scene("B", "Show/Act 2");
    b->addPalette(red->id());
    // What earlier tests left open is theirs.
    QSettings().remove(QStringLiteral("programming/outline/expanded"));

    {
        ProgrammingManager mgr(nullptr, m_doc);
        mgr.resize(1300, 900);
        mgr.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mgr));
        ProgrammingOutline *o = mgr.m_outline;
        o->rebuild();
        auto allOpen = [o]() {
            for (QTreeWidgetItemIterator it(o); *it; ++it)
                if ((*it)->childCount() > 0 && !(*it)->isExpanded())
                    return false;
            return true;
        };
        auto allClosed = [o]() {
            for (QTreeWidgetItemIterator it(o); *it; ++it)
                if ((*it)->childCount() > 0 && (*it)->isExpanded())
                    return false;
            return true;
        };
        QVERIFY(!allOpen());

        // The buttons.
        mgr.findChild<QToolButton *>("outlineExpandAll")->click();
        QVERIFY(allOpen());
        mgr.findChild<QToolButton *>("outlineCollapseAll")->click();
        QVERIFY(allClosed());

        // ⌘→ / ⌘← (Ctrl in Qt's terms).
        o->setFocus();
        o->setCurrentItem(o->topLevelItem(0));
        QTest::keyClick(o, Qt::Key_Right, Qt::ControlModifier);
        QVERIFY(allOpen());
        QTest::keyClick(o, Qt::Key_Left, Qt::ControlModifier);
        QVERIFY(allClosed());

        // ⌥-click Show's triangle: Show and everything under it.
        QTreeWidgetItem *show = o->topLevelItem(0);
        const QRect r = o->visualItemRect(show);
        const QPoint triangle(r.x() - o->indentation() / 2, r.center().y());
        QTest::mouseClick(o->viewport(), Qt::LeftButton, Qt::AltModifier, triangle);
        QVERIFY(allOpen());
        QTest::mouseClick(o->viewport(), Qt::LeftButton, Qt::AltModifier, triangle);
        QVERIFY(allClosed());

        // Leave A's scene open and B closed, for the next time.
        o->expandEverything();
        o->sceneItem(b->id())->setExpanded(false);
    }

    // Next time (a new tab on the same show): as it was left.
    ProgrammingOutline o2(m_doc);
    o2.rebuild();
    QVERIFY(o2.sceneItem(a->id())->isExpanded());
    QVERIFY(!o2.sceneItem(b->id())->isExpanded());
}

void ProgrammingOutline_Test::filterAndRemoveSeveral()
{
    QLCPalette *blue = palette(QLCPalette::Color, "Blue");
    QLCPalette *half = palette(QLCPalette::Dimmer, "Half");
    Scene *a = scene("Washes", "Show");
    a->addPalette(blue->id());
    a->addPalette(half->id());
    Scene *b = scene("Spots", "Show");
    b->addPalette(blue->id());

    ProgrammingManager mgr(nullptr, m_doc);
    mgr.resize(1300, 900);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    ProgrammingOutline *o = mgr.m_outline;
    o->rebuild();

    // "half": its scene shows (opened to it), the other scene goes.
    o->setFilterText("half");
    QTreeWidgetItem *halfRow = o->itemFor(row(ProgrammingOutline::PaletteRow, a->id(), half->id()));
    QVERIFY(!halfRow->isHidden() && o->sceneItem(a->id())->isExpanded());
    QVERIFY(o->sceneItem(b->id())->isHidden());
    o->setFilterText(QString());
    QVERIFY(!o->sceneItem(b->id())->isHidden());

    // Blue in both scenes, selected together, ⌫: out of both, one step.
    o->expandEverything();
    o->clearSelection();
    o->itemFor(row(ProgrammingOutline::PaletteRow, a->id(), blue->id()))->setSelected(true);
    o->itemFor(row(ProgrammingOutline::PaletteRow, b->id(), blue->id()))->setSelected(true);
    o->setFocus();
    QTest::keyClick(o, Qt::Key_Backspace);
    QCOMPARE(a->palettes(), QList<quint32>({ half->id() }));
    QVERIFY(b->palettes().isEmpty());
    QVERIFY(m_doc->palette(blue->id()) != nullptr);     // the palette stays
    mgr.slotUndo();
    QVERIFY(a->palettes().contains(blue->id()) && b->palettes().contains(blue->id()));
}

void ProgrammingOutline_Test::aRealShow()
{
    const QString path = qEnvironmentVariable("QLC_OUTLINE_SHOW");
    if (path.isEmpty())
        QSKIP("set QLC_OUTLINE_SHOW (a .qlcc, read only) to check the outline against a real show");

    // Read a copy: the show itself is never touched.
    QTemporaryDir dir;
    const QString copy = dir.filePath("show.qlcc");
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
    mgr.resize(1600, 950);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    ProgrammingOutline *o = mgr.m_outline;
    o->rebuild();

    // Every scene once (the Show Tree's own hidden), each with every look.
    int scenes = 0;
    for (Function *f : m_doc->functions())
    {
        Scene *s = qobject_cast<Scene *>(f);
        if (s == nullptr || !s->isVisible() || m_doc->showTree()->isGenerated(s->id()))
            continue;
        ++scenes;
        QTreeWidgetItem *it = o->sceneItem(s->id());
        QVERIFY2(it != nullptr, qPrintable(s->name()));
        QCOMPARE(it->text(0), s->name());
        int looks = 0;
        for (QTreeWidgetItemIterator i(it); *i != nullptr; ++i)
        {
            QTreeWidgetItem *up = *i;
            while (up != nullptr && up != it)
                up = up->parent();
            if (up == nullptr)
                break;
            if ((*i)->data(0, ProgrammingOutline::KindRole).toInt() == ProgrammingOutline::PaletteRow)
                ++looks;
        }
        QCOMPARE(looks, s->palettes().size());
    }
    int rows = 0;
    for (QTreeWidgetItemIterator i(o); *i; ++i)
        if ((*i)->data(0, ProgrammingOutline::KindRole).toInt() == ProgrammingOutline::SceneRow)
            ++rows;
    QCOMPARE(rows, scenes);
    qInfo() << "[OUTLINE]" << scenes << "scenes," << o->topLevelItemCount() << "top-level rows";

    const QString shot = qEnvironmentVariable("QLC_OUTLINE_SHOT");
    if (shot.isEmpty())
        return;
    // The scene with the most looks, opened, its first look on the page.
    Scene *pick = nullptr;
    for (Function *f : m_doc->functions())
        if (Scene *s = qobject_cast<Scene *>(f))
            if (o->sceneItem(s->id()) != nullptr && (pick == nullptr || s->palettes().size() > pick->palettes().size()))
                pick = s;
    QVERIFY(pick != nullptr);
    if (QSplitter *split = mgr.findChild<QSplitter *>("programmingSplitter"))
        split->setSizes({ 420, 820, 360 });
    o->setSubtreeExpanded(o->sceneItem(pick->id()), true);
    QVERIFY(o->selectRow(row(ProgrammingOutline::SceneRow, pick->id(), pick->id())));
    QTest::qWait(50);
    QVERIFY(mgr.grab().save(shot));
    if (!pick->palettes().isEmpty())
    {
        QVERIFY(o->selectRow(row(ProgrammingOutline::PaletteRow, pick->id(), pick->palettes().first())));
        QTest::qWait(50);
        QVERIFY(mgr.grab().save(QString(shot).replace(".png", "-look.png")));
    }
    o->expandEverything();
    QTest::qWait(50);
    QVERIFY(mgr.grab().save(QString(shot).replace(".png", "-expanded.png")));
}

QTEST_MAIN(ProgrammingOutline_Test)

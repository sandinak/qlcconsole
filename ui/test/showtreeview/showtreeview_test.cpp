/*
  Q Light Controller Plus
  showtreeview_test.cpp

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
#include <QDoubleSpinBox>
#include <QMenu>
#include <QTimer>
#include <QDialog>
#include <QSettings>
#include <QApplication>
#include <QRadioButton>
#include <QDataStream>
#include <QTreeWidget>
#include <QDropEvent>
#include <QSignalSpy>
#include <QComboBox>
#include <QLineEdit>
#include <QShortcut>
#include <QLabel>
#include <QCheckBox>
#include <QToolButton>
#include <QToolBar>
#include <QSplitter>
#include <QHeaderView>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTabWidget>
#include <QListWidget>
#include <QToolButton>
#include <QMimeData>
#include <QXmlStreamReader>
#include <QtTest>

#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#include "programmingmanager.h"
#include "workbench.h"
#include "treecuestack.h"
#undef protected
#undef private
#include "functionliveeditdialog.h"
#include "fadetimespin.h"
#include "effectrunsmenu.h"
#include "effectscriptrunner.h"
#include "functionmanager.h"
#include "chasereditor.h"
#include "noteedit.h"
#include <QInputDialog>
#include <QWidgetAction>
#include <QPointer>
#include <QSpinBox>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QBuffer>
#include <QStackedWidget>
#include <QRegularExpression>

#include <QTemporaryDir>
#include <memory>
#include "showembedding.h"
#include "fixturemanager.h"
#include "showtreecontroller.h"
#include "showcontrol.h"
#include "blindstyle.h"
#include "timecodesource.h"
#include "paletteops.h"
#include "virtualconsole.h"
#include "vcbutton.h"
#include "vccuelist.h"
#include "vcspeeddial.h"
#include "vcspeeddialfunction.h"
#include "vcclock.h"
#include "showconverter.h"
#include "importsheet.h"
#include "programmingoutline.h"
#include "sceneeditor.h"
#include "efxeditor.h"
#include "efx.h"
#include "vcframe.h"
#include "inputoutputmap.h"
#include "chaseraction.h"
#include "functionstreewidget.h"
#include "fixturegroupsource.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "qlcfixturehead.h"
#include "qlcchannel.h"
#include "qlcfixturemode.h"
#include "showtreeview.h"
#include "showcontrollistener.h"
#include "moveinblackbutton.h"
#include "programmercontroller.h"
#include "linespreadpreview.h"
#include "lineplanview.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "qlcphysical.h"
#include "lookeditor.h"
#include "effectbrowser.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "collection.h"
#include "chaserstep.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "showtreeview_test.h"
#include "../common/dragpos.h"

using namespace ShowTree;

/* The Show Tree tab (docs/design/SHOW_TREE_DESIGN.md §6): the tree, the layer rows with
   inherited values greyed and their source, drops and their hints, one-off
   values in Recent, undo, the Library filter, import. Offscreen. */

void ShowTreeView_Test::init()
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
}

void ShowTreeView_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    delete m_doc;
    m_doc = nullptr;
}

quint32 ShowTreeView_Test::dimmer(int level, const QString &name)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
    p->setName(name);
    p->setValue(level);
    m_doc->addPalette(p);
    return p->id();
}

quint32 ShowTreeView_Test::colour(const QColor &c, const QString &name)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Color, m_doc);
    p->setName(name);
    p->setValue(QLCPalette::colorToString(c, QColor(Qt::black)));
    m_doc->addPalette(p);
    return p->id();
}

FixtureGroup *ShowTreeView_Test::group(const QString &name, int from, int count)
{
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName(name);
    m_doc->addFixtureGroup(g);
    for (int i = from; i < from + count; ++i)
        g->assignFixture(m_fixtures[i]);
    return g;
}

static QStringList treeText(QTreeWidget *tree)
{
    QStringList out;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
    {
        int depth = 0;
        for (QTreeWidgetItem *p = (*it)->parent(); p != nullptr; p = p->parent())
            ++depth;
        QString line = QString(depth * 2, QLatin1Char(' ')) + (*it)->text(0);
        // The info column as it reads, each flag column by its header (Q104).
        QStringList cols;
        for (int c = 1; c < tree->columnCount(); ++c)
            if (!(*it)->text(c).isEmpty())
                cols << (c == ShowTreeView::InfoCol ? (*it)->text(c)
                                                    : tree->headerItem()->text(c) + " " + (*it)->text(c));
        if (!cols.isEmpty())
            line += QStringLiteral(" [%1]").arg(cols.join(", "));
        out << line;
    }
    return out;
}

static QString rowFor(const QStringList &rows, const QString &label)
{
    for (const QString &r : rows)
        if (r.startsWith(label + " |"))
            return r;
    return QString();
}

void ShowTreeView_Test::theTreeShowsProductionsListsCuesAndLayers()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("DanceHysteria");
    const quint32 list = st->addCueList(prod, "01-Everybody Scream", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "PreCurtain");
    Target t;
    t.groups << towers->id();
    st->addLayer(c1, t);

    ShowTreeView view(nullptr, m_doc);
    const QStringList text = treeText(view.tree());
    // The only production (Q75): current as it was made, GO plays its one
    // list, and neither "current" nor "main" is shown -- nothing to choose.
    QCOMPARE(st->currentProduction(), prod);
    QCOMPARE(text, QStringList({ QString::fromUtf8("DanceHysteria"),
                                 QString::fromUtf8("  01-Everybody Scream [M B]"),
                                 "    1.0.0 PreCurtain", "      Towers" }));
    auto switcherShown = [&view]() {
        QComboBox *combo = view.findChild<QComboBox *>("stProduction");
        for (QWidgetAction *a : view.findChildren<QWidgetAction *>())
            if (a->defaultWidget() == combo)
                return a->isVisible();
        return false;
    };
    QVERIFY(!switcherShown());
    // A second production brings current, main and the switcher back.
    const quint32 prod2 = st->addProduction("Spring");
    st->addCueList(prod2, "Opener", ListMode::Tracking);
    QVERIFY(treeText(view.tree()).contains(QString::fromUtf8("DanceHysteria [\u25B6 current]")));
    QVERIFY(treeText(view.tree()).contains(QString::fromUtf8("  01-Everybody Scream [main, M B]")));
    QVERIFY(switcherShown());
    QVERIFY(st->removeProduction(prod2));
    QVERIFY(!switcherShown());

    // An edit elsewhere shows at once.
    st->addCue(list, 0, "Verse");
    QVERIFY(treeText(view.tree()).contains("    1.0.1 Verse"));

    QVERIFY(view.selectNode(c1));
    QCOMPARE(view.currentKind(), ShowTreeView::CueNode);
    QCOMPARE(view.currentCueId(), c1);
}

void ShowTreeView_Test::newHereMakesTheNextThing()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    ShowTreeController *st = m_doc->showTree();
    ShowTreeView view(nullptr, m_doc);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    // The shortcut is the view's own.
    QShortcut *sc = view.findChild<QShortcut *>("stNewHere");
    QVERIFY(sc != nullptr);
    QCOMPARE(sc->key(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    // The new row is renaming in place: type a name and commit it.
    auto name = [&](const QString &text) {
        QVERIFY(view.isRenaming());
        QTreeWidgetItem *editing = view.tree()->currentItem();
        view.tree()->closePersistentEditor(editing, 0);
        editing->setText(0, text);
        QVERIFY(!view.isRenaming());
        QCoreApplication::processEvents();      // the rename lands after the editor
    };

    // Nothing yet: a production. One step (the name is another).
    view.tree()->setFocus();
    QCOMPARE(view.newHereLabel(), QString("New Production"));
    quint32 prod = view.newHere();
    QVERIFY(st->production(prod) != nullptr);
    QCOMPARE(view.currentKind(), ShowTreeView::ProductionNode);
    name("Gig");
    QCOMPARE(st->production(prod)->name, QString("Gig"));
    view.slotUndo();
    QCOMPARE(st->production(prod)->name, QString("Show"));
    view.slotUndo();
    QVERIFY(st->productions().isEmpty());
    prod = view.newHere();
    name("Gig");

    // A production: a cue list in it (also with nothing selected).
    QCOMPARE(view.newHereLabel(), QString("New Cue List in Gig"));
    view.tree()->setCurrentItem(nullptr);
    QCOMPARE(view.newHereLabel(), QString("New Cue List in Gig"));
    QVERIFY(view.selectNode(prod));
    const quint32 list = view.newHere();
    QVERIFY(st->cueList(list) != nullptr);
    QCOMPARE(st->cueList(list)->mode, ListMode::Tracking);
    QCOMPARE(view.currentKind(), ShowTreeView::ListNode);
    name("01-Opener");

    // A cue list: a cue at its end.
    QCOMPARE(view.newHereLabel(), QString("New Cue in 01-Opener"));
    const quint32 c1 = view.newHere();
    QCOMPARE(view.currentKind(), ShowTreeView::CueNode);
    QCOMPARE(view.currentId(), c1);
    name("Preset");
    QCOMPARE(st->cue(c1)->name, QString("Preset"));
    const CueList *l = st->cueList(list);
    QCOMPARE(st->cueNumber(*l, 0), QString("1.0.0"));

    // A cue: the next cue after it, numbered as + Cue numbers it.
    QCOMPARE(view.newHereLabel(), QString("New Cue After 1.0.0"));
    const quint32 c2 = view.newHere();
    l = st->cueList(list);
    QCOMPARE(l->indexOf(c2), 1);
    QCOMPARE(st->cueNumber(*l, 1), QString("1.0.1"));
    name("Walk-in");
    // Back on the first: between them, as + Cue makes one there.
    QVERIFY(view.selectNode(c1));
    const quint32 between = view.newHere();
    l = st->cueList(list);
    QCOMPARE(l->indexOf(between), 1);
    QCOMPARE(l->indexOf(c2), 2);
    const QString betweenNumber = st->cueNumber(*l, 1);
    QVERIFY(betweenNumber != "1.0.0" && betweenNumber != "1.0.1");
    name("Point");
    view.slotUndo();
    view.slotUndo();                            // the cue, one step
    QVERIFY(st->cue(between) == nullptr);
    QCOMPARE(st->cueList(list)->cues.count(), 2);

    // Groups picked in the Library, the focus there: a layer on them in the
    // selected cue.
    QVERIFY(view.selectNode(c1));
    // (The Library is rebuilt after an edit: pick again each time.)
    auto pickTowers = [&]() {
        QTreeWidgetItem *towersRow = nullptr;
        for (QTreeWidgetItemIterator it(view.library()); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LibGroup
                && (*it)->data(0, ShowTreeView::IdRole).toUInt() == towers->id())
                towersRow = *it;
        QVERIFY(towersRow != nullptr);
        view.library()->clearSelection();
        towersRow->setSelected(true);
    };
    QTest::qWait(10);
    pickTowers();
    view.library()->setFocus();
    QTRY_VERIFY(view.library()->hasFocus());
    QCOMPARE(view.newHereLabel(), QString("New Layer on Towers in 1.0.0 Preset"));
    const quint32 layer = view.newHere();
    QVERIFY(st->cue(c1)->layer(layer) != nullptr);
    QCOMPARE(st->cue(c1)->layer(layer)->target.groups, QList<quint32>({ towers->id() }));
    QCOMPARE(view.currentKind(), ShowTreeView::LayerNode);
    QCOMPARE(view.currentId(), layer);
    name("Sides");
    QCOMPARE(st->cue(c1)->layer(layer)->name, QString("Sides"));
    view.slotUndo();
    view.slotUndo();                            // the layer, one step
    QVERIFY(st->cue(c1)->layers.isEmpty());

    // The focus back in the tree, on the cue: the Library doesn't count --
    // the next cue.
    QVERIFY(view.selectNode(c1));
    view.tree()->setFocus();
    QTRY_VERIFY(view.tree()->hasFocus());
    QCOMPARE(view.newHereLabel(), QString("New Cue After 1.0.0"));

    // A layer selected, groups picked in the Library: a new layer on them.
    Target t;
    t.groups << towers->id();
    const quint32 own = st->addLayer(c1, t);
    QTest::qWait(10);
    pickTowers();
    QVERIFY(view.selectNode(own, c1));
    QCOMPARE(view.newHereLabel(), QString("New Layer on Towers in 1.0.0 Preset"));
    const quint32 second = view.newHere();
    QCOMPARE(st->cue(c1)->layers.count(), 2);
    QCOMPARE(view.currentId(), second);
    name("Again");
    view.slotUndo();
    view.slotUndo();
    QCOMPARE(st->cue(c1)->layers.count(), 1);

    // ... nothing picked: the cue selected and the Library's filter, so
    // Return makes the layer there. Nothing made yet.
    view.library()->clearSelection();
    QVERIFY(view.selectNode(own, c1));
    QCOMPARE(view.newHereLabel(), QString::fromUtf8("New Layer in 1.0.0 Preset\u2026"));
    QCOMPARE(view.newHere(), invalidId());
    QCOMPARE(view.currentKind(), ShowTreeView::CueNode);
    QCOMPARE(view.currentId(), c1);
    QLineEdit *filter = view.findChild<QLineEdit *>("stLibraryFilter");
    QVERIFY(filter != nullptr);
    QTRY_VERIFY(filter->hasFocus());
    QCOMPARE(st->cue(c1)->layers.count(), 1);

    // A sub-cue: the next main cue after its cue, as + Cue makes it.
    const quint32 sub = st->addSubCue(c1, "Flash");
    QVERIFY(sub != invalidId());
    QVERIFY(view.selectNode(sub));
    view.tree()->setFocus();
    QCOMPARE(view.newHereLabel(), QString("New Cue After 1.0.0"));
    const quint32 afterSub = view.newHere();
    QCOMPARE(st->cueList(list)->indexOf(afterSub), 1);
    name("After");

    // A look: the next look.
    const quint32 look = st->addLook(prod, "Hit");
    QVERIFY(view.selectNode(look));
    QCOMPARE(view.newHereLabel(), QString("New Hit"));
    const quint32 look2 = view.newHere();
    QVERIFY(st->isLook(look2));
}

void ShowTreeView_Test::trackedLayersShowGreyWithWhereTheyCameFrom()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    const quint32 blue = colour(Qt::blue, "Blue");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    Target t1, t2;
    t1.groups << towers->id();
    t2.groups << usts->id();
    st->setLayerLook(c1, st->addLayer(c1, t1), st->lookForPalette(blue));
    st->addLayer(c2, t2);

    ShowTreeView view(nullptr, m_doc);
    const QStringList text = treeText(view.tree());
    // The row says where by the cue's number; the tooltip says the rest.
    QVERIFY2(text.contains("      ⤳ Towers [1, T ●]"), qPrintable(text.join("\n")));
    QVERIFY2(text.contains("    2 Two [S 1, T 1]"), qPrintable(text.join("\n")));

    // Selecting the tracked row shows it, read from where it was set.
    QTreeWidgetItem *tracked = nullptr;
    for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::TrackedLayerNode)
            tracked = *it;
    QVERIFY(tracked != nullptr);
    view.tree()->setCurrentItem(tracked);
    QCOMPARE(rowFor(view.attributeRows(), "Colour"), QString("Colour | Blue | tracked from 1 One"));

    // A palette set on it there makes it cue 2's own layer -- one undo step.
    const quint32 red = colour(Qt::red, "Red");
    QVERIFY(view.dropPalettes(c2, invalidId(), { red }, t1));
    QCOMPARE(st->cue(c2)->layers.count(), 2);
    QCOMPARE(st->cue(c2)->layers.last().target, t1);
    QCOMPARE(st->cue(c2)->layers.last().looks.first().palettes, QList<quint32>({ red }));
    QVERIFY(st->cue(c2)->looks.isEmpty());          // not a cue look
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c2)->layers.count(), 1);
    QVERIFY(view.selectNode(c2));
    QCOMPARE(view.currentKind(), ShowTreeView::CueNode);
}

void ShowTreeView_Test::groupsDroppedOnACueBecomeLayers()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");

    ShowTreeView view(nullptr, m_doc);
    // Several groups dropped together: one layer on all of them.
    QVERIFY(view.dropTargets(c1, { towers->id(), usts->id() }, {}));
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    QCOMPARE(st->cue(c1)->layers.first().target.groups, QList<quint32>({ towers->id(), usts->id() }));
    QCOMPARE(view.currentKind(), ShowTreeView::LayerNode);      // the new layer is selected
    QCOMPARE(view.tree()->currentItem()->text(0), QString("Towers + USTs"));
    // ⌥: one layer per group, as it was.
    QVERIFY(view.dropTargets(c1, { towers->id(), usts->id() }, {}, true));
    QCOMPARE(st->cue(c1)->layers.count(), 3);
    QVERIFY(view.dropTargets(c1, {}, { m_fixtures[4], m_fixtures[5] }));
    QCOMPARE(st->cue(c1)->layers.count(), 4);
    QCOMPARE(st->cue(c1)->layers.last().target.fixtures.count(), 2);
    QVERIFY(view.recentItems().first().startsWith("USTs"));
}

void ShowTreeView_Test::groupsDroppedOnALayerJoinIt()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    FixtureGroup *spots = group("Spots", 4, 2);
    const quint32 d50 = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, t);
    st->setLayerLook(c1, l1, st->lookForPalette(d50));

    ShowTreeView view(nullptr, m_doc);
    auto groupMime = [](QList<quint32> ids) {
        QMimeData *mime = new QMimeData();
        QByteArray data;
        QDataStream s(&data, QIODevice::WriteOnly);
        for (quint32 id : ids)
            s << id;
        mime->setData(FixtureGroupSource::fixtureGroupMimeType(), data);
        return mime;
    };
    QScopedPointer<QMimeData> one(groupMime({ usts->id() }));
    QScopedPointer<QMimeData> two(groupMime({ usts->id(), spots->id() }));

    // The hint says which will happen before the drop.
    QCOMPARE(view.dropHint(c1, invalidId(), one.data()), QString("a new layer on USTs in 1 One"));
    QCOMPARE(view.dropHint(c1, invalidId(), two.data()),
             QString("one layer on USTs + Spots in 1 One (⌥: a layer for each group)"));
    QCOMPARE(view.dropHint(c1, invalidId(), two.data(), true),
             QString("2 new layers in 1 One, one for each group"));
    QVERIFY(view.acceptsDrop(c1, l1, one.data()));
    QCOMPARE(view.dropHint(c1, l1, two.data()), QString("add USTs + Spots to layer Towers in 1 One"));

    // Dropped on the layer: it lights all three, one layer, one scene.
    QVERIFY(view.dropTargetsOnLayer(c1, l1, { usts->id(), spots->id() }, {}));
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    QCOMPARE(st->cue(c1)->layers.first().target.groups.count(), 3);
    const Collection *coll = qobject_cast<const Collection *>(m_doc->function(st->cue(c1)->collection));
    QCOMPARE(coll->functions().count(), 1);
    const Scene *scene = qobject_cast<const Scene *>(m_doc->function(coll->functions().first()));
    QCOMPARE(scene->fixtureGroups().count(), 3);
    QCOMPARE(view.currentId(), l1);
    QVERIFY(st->undoText().startsWith("Add USTs, Spots to layer"));
    // Undo deletes that scene (nothing plays it then): keep its id only.
    const quint32 sceneId = scene->id();

    // Undo, and it is Towers alone again.
    view.slotUndo();
    QCOMPARE(st->cue(c1)->layers.first().target.groups, QList<quint32>({ towers->id() }));

    // An opaque layer takes no groups.
    Target fn;
    fn.function = sceneId;
    const quint32 opaque = st->addLayer(c1, fn);
    QVERIFY(!view.acceptsDrop(c1, opaque, one.data()));
}

void ShowTreeView_Test::aPaletteOnTheCueIsACueLookAndTheHintSaysWhatItReaches()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    const quint32 blue = colour(Qt::blue, "Blue");
    const quint32 red = colour(Qt::red, "Red");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t1, t2;
    t1.groups << towers->id();
    t2.groups << usts->id();
    st->addLayer(c1, t1);
    const quint32 l2 = st->addLayer(c1, t2);
    st->setLayerLook(c1, l2, st->lookForPalette(red));

    ShowTreeView view(nullptr, m_doc);
    QMimeData mime;
    QByteArray data;
    QDataStream s(&data, QIODevice::WriteOnly);
    s << blue;
    mime.setData(FunctionsTreeWidget::paletteDragMimeType(), data);
    QVERIFY(view.acceptsDrop(c1, invalidId(), &mime));
    QCOMPARE(view.dropHint(c1, invalidId(), &mime), QString("Blue → cue look on 1 One: reaches 1 of 2 layers"));
    QVERIFY(view.dropHint(c1, l2, &mime).startsWith("Blue → layer USTs"));

    QVERIFY(view.dropPalettes(c1, invalidId(), { blue }));
    QCOMPARE(st->cue(c1)->looks.count(), 1);
    QCOMPARE(st->cue(c1)->looks.first().palettes, QList<quint32>({ blue }));

    // Groups go on a cue (a new layer) and on a layer (they join it).
    QMimeData groups;
    QByteArray g;
    QDataStream gs(&g, QIODevice::WriteOnly);
    gs << towers->id();
    groups.setData(FixtureGroupSource::fixtureGroupMimeType(), g);
    QVERIFY(view.acceptsDrop(c1, invalidId(), &groups));
    QVERIFY(view.acceptsDrop(c1, l2, &groups));
}

void ShowTreeView_Test::layerRowsShowOwnAndInheritedValues()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 blue = colour(Qt::blue, "Blue");
    const quint32 full = dimmer(255, "Full");
    const quint32 d45 = dimmer(115, "45%");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    Target t;
    t.groups << towers->id();
    st->setCueLook(c1, st->lookForPalette(blue));
    const quint32 l1 = st->addLayer(c1, t);
    st->setLayerLook(c1, l1, st->lookForPalette(full));
    const quint32 l2 = st->addLayer(c2, t);
    st->setLayerLook(c2, l2, st->lookForPalette(d45));

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.selectNode(l1, c1));
    QStringList rows = view.attributeRows();
    QCOMPARE(rowFor(rows, "Colour"), QString("Colour | Blue | from cue 1 One"));
    QCOMPARE(rowFor(rows, "Dimmer"), QString("Dimmer | Full | "));
    QCOMPARE(rowFor(rows, "Position"), QString("Position | — | "));

    QVERIFY(view.selectNode(l2, c2));
    rows = view.attributeRows();
    QCOMPARE(rowFor(rows, "Colour"), QString("Colour | Blue | tracked from 1 One"));
    QCOMPARE(rowFor(rows, "Dimmer"), QString("Dimmer | 45% | "));
    QVERIFY(rowFor(rows, "Gobo").startsWith("Gobo |"));
    QVERIFY(rowFor(rows, "Focus / Frost / Iris").startsWith("Focus / Frost / Iris |"));
}

void ShowTreeView_Test::beamRowsSayWhatTheyApplyTo()
{
    QVERIFY(m_doc->fixtureDefCache()->load(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/Clay_Paky")));
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef("Clay Paky", "Sharpy");
    QVERIFY(def != nullptr);
    Fixture *spot = new Fixture(m_doc);
    spot->setName("Sharpy");
    spot->setFixtureDefinition(def, def->modes().first());
    spot->setAddress(100);
    m_doc->addFixture(spot);
    FixtureGroup *mixed = group("AT-All", 0, 2);
    mixed->assignFixture(spot->id());

    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << mixed->id();
    const quint32 l1 = st->addLayer(c1, t);

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.selectNode(l1, c1));
    QLabel *gobo = view.findChild<QLabel *>("stRow:" + KeyGobo);
    QVERIFY(gobo != nullptr);
    bool found = false;
    for (QLabel *l : view.findChildren<QLabel *>())
        if (l->text() == "applies to 1 of 3")
            found = true;
    QVERIFY(found);
}

void ShowTreeView_Test::goboLooksMatchByNamePerType()
{
    for (const char *dir : { "Chauvet", "Contest", "Clay_Paky" })
        QVERIFY(m_doc->fixtureDefCache()->load(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/" + dir)));
    auto spot = [this](const QString &make, const QString &model, quint32 address) {
        QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef(make, model);
        Fixture *f = new Fixture(m_doc);
        f->setName(model);
        f->setFixtureDefinition(def, def->modes().first());
        f->setAddress(address);
        m_doc->addFixture(f);
        return f;
    };
    Fixture *chauvet = spot("Chauvet", "MiN Spot RGBW", 100);
    Fixture *contest = spot("Contest", "PSC-930", 200);
    Fixture *sharpy = spot("Clay Paky", "Sharpy", 300);
    FixtureGroup *mixed = group("AT-All", 0, 1);
    for (Fixture *f : { chauvet, contest, sharpy })
        mixed->assignFixture(f->id());

    // A gobo look made the old way: one DMX value (Chauvet's Dots).
    QLCPalette *gobo = new QLCPalette(QLCPalette::Gobo, m_doc);
    gobo->setName("Dots");
    gobo->setValue(10);
    m_doc->addPalette(gobo);

    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << mixed->id();
    const quint32 l1 = st->addLayer(c1, t);
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(gobo->id())));

    ShowTreeView view(nullptr, m_doc);
    auto reachText = [&view]() {
        // The rows of an earlier page go later: let them go first.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QLabel *l = view.findChild<QLabel *>("stReach:" + KeyGobo);
        return l == nullptr ? QString() : l->text();
    };
    QVERIFY(view.selectNode(l1, c1));
    QCOMPARE(reachText(), QString("applies to 3 of 4"));      // every gobo channel

    // Pick Dots on the Chauvet's wheel in the Look Editor: the look is
    // named for it, and the Contest finds its own Dots by that name.
    view.editPalette(gobo->id());
    LookEditor *ed = view.lookEditor();
    QComboBox *source = ed->findChild<QComboBox *>("singleSourceCombo");
    QComboBox *caps = ed->findChild<QComboBox *>("singleCapabilityCombo");
    QVERIFY(source != nullptr && caps != nullptr);
    int ci = -1;
    for (int i = 0; i < source->count(); ++i)
        if (source->itemText(i).contains("MiN Spot"))
            ci = i;
    QVERIFY(ci >= 0);
    source->setCurrentIndex(ci);
    QMetaObject::invokeMethod(source, "activated", Q_ARG(int, ci));
    const int dots = caps->findText("Dots");
    QVERIFY(dots >= 0);
    caps->setCurrentIndex(dots);
    QMetaObject::invokeMethod(caps, "activated", Q_ARG(int, dots));
    QCOMPARE(gobo->goboName(), QString("Dots"));
    QCOMPARE(gobo->goboValues().value("Chauvet/MiN Spot RGBW"), 71);
    QCOMPARE(gobo->goboValueFor(contest), 24);
    QCOMPARE(gobo->goboValueFor(sharpy), -1);
    QLineEdit *name = ed->findChild<QLineEdit *>("goboNameEdit");
    QCOMPARE(name->text(), QString("Dots"));
    const QString matches = ed->findChild<QLabel *>("goboMatches")->text();
    QVERIFY2(matches.contains("PSC-930: Dots (by name)"), qPrintable(matches));
    QVERIFY2(matches.contains("Sharpy: no gobo of that name"), qPrintable(matches));

    // The row counts it now: the Sharpy (and the dimmer) ignore it.
    QVERIFY(view.selectNode(c1));
    QVERIFY(view.selectNode(l1, c1));
    QCOMPARE(reachText(), QString("applies to 2 of 4"));

    // Typing a name the Sharpy has: it plays it, the others keep their pick
    // or find it too.
    name->setText("Gobo 3");
    QMetaObject::invokeMethod(name, "editingFinished");
    QCOMPARE(gobo->goboName(), QString("Gobo 3"));
    QCOMPARE(gobo->goboValueFor(sharpy), (12 + 15) / 2);
    QCOMPARE(gobo->goboValueFor(chauvet), 71);                 // picked for its type

    // Looser (Branson 2026-10-17): a plural finds the singular...
    name->setText("Turbines");
    QMetaObject::invokeMethod(name, "editingFinished");
    QCOMPARE(int(gobo->resolveGobo(contest).how), int(QLCPalette::GoboSingular));
    QVERIFY(gobo->goboValueFor(contest) >= 0);
    QVERIFY2(ed->findChild<QLabel *>("goboMatches")->text().contains("PSC-930: Turbine (by name, singular/plural)"),
             qPrintable(ed->findChild<QLabel *>("goboMatches")->text()));

    // ...but a name that fits several slots on a type isn't guessed: that
    // type ignores it, the row says it's ambiguous and offers the pick.
    name->setText("Shake");
    QMetaObject::invokeMethod(name, "editingFinished");
    QCOMPARE(int(gobo->resolveGobo(contest).how), int(QLCPalette::GoboAmbiguous));
    QCOMPARE(gobo->goboValueFor(contest), -1);
    QCOMPARE(gobo->goboValueFor(sharpy), -1);
    QCOMPARE(gobo->goboValueFor(chauvet), 71);
    QVERIFY2(ed->findChild<QLabel *>("goboMatches")->text().contains("PSC-930: ambiguous (Dot Shake / Dots Shake"),
             qPrintable(ed->findChild<QLabel *>("goboMatches")->text()));
    QVERIFY(view.selectNode(c1));
    QVERIFY(view.selectNode(l1, c1));
    QVERIFY2(reachText().startsWith("applies to 1 of 4 — ambiguous"), qPrintable(reachText()));
    QLabel *reach = view.findChild<QLabel *>("stReach:" + KeyGobo);
    QVERIFY(reach->text().contains("<a href=\"pick\">"));
    QVERIFY2(reach->toolTip().contains("PSC-930: Dot Shake / Dots Shake"), qPrintable(reach->toolTip()));
    QVERIFY(reach->toolTip().contains("Sharpy: "));
    const QStringList amb = st->lookAmbiguities(t, *st->cue(c1)->layer(l1)->look(KeyGobo));
    QCOMPARE(amb.count(), 2);                                   // one line a type
    emit reach->linkActivated("pick");                          // the per-type pick
    QCOMPARE(view.lookEditor()->paletteId(), gobo->id());
}

void ShowTreeView_Test::oneOffValuesGoToRecentNotAll()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, t);

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.setOneOffDimmer(c1, l1, 30));
    const Look *look = st->cue(c1)->layer(l1)->look(KeyDimmer);
    QVERIFY(look != nullptr);
    const quint32 pid = look->palettes.first();
    QVERIFY(st->isInlinePalette(pid));
    QCOMPARE(m_doc->palette(pid)->value().toInt(), 77);
    QCOMPARE(m_doc->palette(pid)->name(), QString("30% · 1 One Towers"));
    QVERIFY(view.recentItems().first().startsWith("30% · 1 One Towers"));

    // Not in All.
    for (QTreeWidgetItemIterator it(view.library()); *it; ++it)
    {
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() != ShowTreeView::LibPalette)
            continue;
        if ((*it)->parent() != nullptr && (*it)->parent()->parent() == nullptr)
            continue;   // directly under a section: Recent or Pinned
        QVERIFY((*it)->data(0, ShowTreeView::IdRole).toUInt() != pid);
    }

    QVERIFY(view.setOneOffColour(c1, invalidId(), QColor(0x33, 0xaa, 0x55)));
    const Look *cueColour = st->cue(c1)->look(KeyColour);
    QVERIFY(cueColour != nullptr);
    QCOMPARE(m_doc->palette(cueColour->palettes.first())->rgbValue(), QColor(0x33, 0xaa, 0x55));
}

void ShowTreeView_Test::noneAndClear()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 blue = colour(Qt::blue, "Blue");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    st->setCueLook(c1, st->lookForPalette(blue));
    const quint32 l1 = st->addLayer(c1, t);

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.setAttributeNone(c1, l1, KeyColour));
    QVERIFY(view.selectNode(l1, c1));
    QCOMPARE(rowFor(view.attributeRows(), "Colour"), QString("Colour | Colour none | "));
    QVERIFY(view.clearAttribute(c1, l1, KeyColour));
    QVERIFY(view.selectNode(l1, c1));
    QCOMPARE(rowFor(view.attributeRows(), "Colour"), QString("Colour | Blue | from cue 1 One"));
}

void ShowTreeView_Test::cueFieldsEditTheCue()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.selectNode(c1));
    QLineEdit *name = view.findChild<QLineEdit *>("stCueName");
    QVERIFY(name != nullptr);
    QCOMPARE(name->text(), QString("One"));
    name->setText("Opener");
    emit name->editingFinished();
    QCOMPARE(st->cue(c1)->name, QString("Opener"));
    QComboBox *mode = view.findChild<QComboBox *>("stCueMode");
    mode->setCurrentIndex(mode->findData(int(CueMode::CueOnly)));
    emit mode->activated(mode->currentIndex());
    QCOMPARE(st->cue(c1)->mode, CueMode::CueOnly);

    // The collection it plays is renamed with it.
    const Collection *coll = qobject_cast<Collection *>(m_doc->function(st->cue(c1)->collection));
    QCOMPARE(coll->name(), QString("1-Opener"));

    // The note opens up while typed in: Shift+Return a new line, Return
    // commits (one undo step), Esc puts it back.
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    NoteEdit *note = view.findChild<NoteEdit *>("stCueNote");
    QVERIFY(note != nullptr);
    const int closed = note->height();
    note->setFocus(Qt::MouseFocusReason);
    QVERIFY(note->isExpanded());
    QTest::keyClicks(note, "House to half");
    QTest::keyClick(note, Qt::Key_Return, Qt::ShiftModifier);
    QTest::keyClicks(note, "then out");
    QVERIFY(note->height() > closed);
    QCOMPARE(st->cue(c1)->note, QString());
    QTest::keyClick(note, Qt::Key_Return);
    QCOMPARE(st->cue(c1)->note, QString("House to half\nthen out"));
    QCOMPARE(note->height(), closed);
    note->setFocus(Qt::MouseFocusReason);
    QTest::keyClicks(note, " nope");
    QTest::keyClick(note, Qt::Key_Escape);
    QCOMPARE(note->text(), QString("House to half\nthen out"));
    QCOMPARE(st->cue(c1)->note, QString("House to half\nthen out"));
    view.slotUndo();
    QCOMPARE(st->cue(c1)->note, QString());
}

void ShowTreeView_Test::undoAndRedoFromTheView()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    ShowTreeView view(nullptr, m_doc);
    st->addCue(list, -1, "One");
    QCOMPARE(st->cueList(list)->cues.count(), 1);
    view.slotUndo();
    QCOMPARE(st->cueList(list)->cues.count(), 0);
    QVERIFY(!treeText(view.tree()).contains("    1 One"));
    view.slotRedo();
    QCOMPARE(st->cueList(list)->cues.count(), 1);
    QVERIFY(treeText(view.tree()).contains("    1 One"));
}

void ShowTreeView_Test::theLibraryFilters()
{
    group("Towers", 0, 2);
    group("USTs", 2, 2);
    colour(Qt::blue, "Blue");
    ShowTreeView view(nullptr, m_doc);
    QLineEdit *filter = view.findChild<QLineEdit *>("stLibraryFilter");
    QVERIFY(filter != nullptr);
    filter->setText("tow");
    QStringList visible;
    for (QTreeWidgetItemIterator it(view.library()); *it; ++it)
    {
        const int kind = (*it)->data(0, ShowTreeView::KindRole).toInt();
        if (!(*it)->isHidden() && kind != ShowTreeView::LibSection && kind != ShowTreeView::LibFolder)
            visible << (*it)->text(0);
    }
    QCOMPARE(visible, QStringList({ "Towers (2)" }));
    filter->clear();
    int groups = 0;
    for (QTreeWidgetItemIterator it(view.library()); *it; ++it)
        if (!(*it)->isHidden() && (*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LibGroup)
            ++groups;
    QCOMPARE(groups, 2);
}

static QTreeWidgetItem *libraryItem(QTreeWidget *lib, const QString &text)
{
    for (QTreeWidgetItemIterator it(lib); *it; ++it)
        if ((*it)->text(0) == text)
            return *it;
    return nullptr;
}

void ShowTreeView_Test::theLibraryShowsGroupsInTheirFolders()
{
    group("Towers", 0, 1);
    group("AT-Spots", 1, 1)->setPath("Movers/Front");
    group("AT-Wash", 2, 1)->setPath("Movers/Back");
    group("Truss", 3, 1)->setPath("Movers");
    group("Floor", 4, 1)->setPath("Practicals");
    ShowTreeView view(nullptr, m_doc);
    QTreeWidget *lib = view.library();

    // Nested as in the Fixture Manager: folders first, then groups.
    QTreeWidgetItem *groups = libraryItem(lib, "Groups");
    QVERIFY(groups != nullptr);
    QStringList top;
    for (int i = 0; i < groups->childCount(); ++i)
        top << groups->child(i)->text(0);
    QCOMPARE(top, QStringList({ "Movers", "Practicals", "Towers (1)" }));
    QTreeWidgetItem *movers = groups->child(0);
    QStringList inMovers;
    for (int i = 0; i < movers->childCount(); ++i)
        inMovers << movers->child(i)->text(0);
    QCOMPARE(inMovers, QStringList({ "Back", "Front", "Truss (1)" }));
    QTreeWidgetItem *front = movers->child(1);
    QCOMPARE(front->child(0)->text(0), QString("AT-Spots (1)"));
    QCOMPARE(front->data(0, ShowTreeView::PathRole).toString(), QString("Groups/Movers/Front"));

    // An open folder stays open when the Library rebuilds (a group added).
    groups->setExpanded(true);
    movers->setExpanded(true);
    front->setExpanded(true);
    group("Spare", 5, 1);
    QCoreApplication::processEvents();
    QVERIFY(libraryItem(lib, "Spare (1)") != nullptr);
    QVERIFY(libraryItem(lib, "Front")->isExpanded());
    QVERIFY(!libraryItem(lib, "Back")->isExpanded());

    // The filter finds a group at any depth, and opens the way to it...
    QLineEdit *filter = view.findChild<QLineEdit *>("stLibraryFilter");
    filter->setText("wash");
    QStringList visible;
    for (QTreeWidgetItemIterator it(lib); *it; ++it)
        if (!(*it)->isHidden() && (*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LibGroup)
            visible << (*it)->text(0);
    QCOMPARE(visible, QStringList({ "AT-Wash (1)" }));
    QVERIFY(libraryItem(lib, "Back")->isExpanded());
    // ...a folder's name shows what is in it...
    filter->setText("movers");
    visible.clear();
    for (QTreeWidgetItemIterator it(lib); *it; ++it)
        if (!(*it)->isHidden() && (*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LibGroup)
            visible << (*it)->text(0);
    QCOMPARE(visible, QStringList({ "AT-Wash (1)", "AT-Spots (1)", "Truss (1)" }));
    // ...and clearing it puts the folders back as they were.
    filter->clear();
    QVERIFY(libraryItem(lib, "Front")->isExpanded());
    QVERIFY(!libraryItem(lib, "Back")->isExpanded());
    QVERIFY(!libraryItem(lib, "Practicals")->isExpanded());
}

void ShowTreeView_Test::importingAChaserSelectsTheNewList()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    Scene *s = new Scene(m_doc);
    s->setName("Towers-Full");
    s->addFixtureGroup(towers->id());
    s->addPalette(full);
    m_doc->addFunction(s);
    Collection *c = new Collection(m_doc);
    c->setName("0.0.1-Open");
    m_doc->addFunction(c);
    c->addFunction(s->id());
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(c->id()));

    ShowTreeView view(nullptr, m_doc);
    view.importChaser(chaser->id(), false);
    QCOMPARE(view.currentKind(), ShowTreeView::ListNode);
    QVERIFY(treeText(view.tree()).contains("    0.0.1 Open"));
    QVERIFY(treeText(view.tree()).contains("      Towers"));
}

void ShowTreeView_Test::inlinePalettesStayOutOfThePaletteTree()
{
    colour(Qt::blue, "Blue");
    const quint32 pid = m_doc->showTree()->createInlinePalette(QLCPalette::Dimmer, { 10 }, "4% · somewhere");
    FunctionsTreeWidget tree(m_doc);
    tree.updateTree();
    bool blue = false;
    for (QTreeWidgetItemIterator it(&tree); *it; ++it)
    {
        QVERIFY((*it)->text(0) != "4% · somewhere");
        if ((*it)->text(0) == "Blue")
            blue = true;
    }
    QVERIFY(blue);
    QVERIFY(tree.addPalette(pid) == nullptr);
}

void ShowTreeView_Test::dropsOnTheTreeWidget()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");

    ShowTreeView view(nullptr, m_doc);
    view.resize(1000, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    QTreeWidgetItem *cueItem = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode)
            cueItem = *it;
    QVERIFY(cueItem != nullptr);
    tree->scrollToItem(cueItem);
    const QPoint at = tree->visualItemRect(cueItem).center();

    QMimeData *mime = new QMimeData();
    QByteArray g;
    QDataStream gs(&g, QIODevice::WriteOnly);
    gs << towers->id();
    mime->setData(FixtureGroupSource::fixtureGroupMimeType(), g);
    QDragEnterEvent enter(dragPos(at), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &enter);
    QDragMoveEvent move(dragPos(at), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &move);
    QVERIFY(move.isAccepted());
    QLabel *hint = view.findChild<QLabel *>("stDropHint");
    QCOMPARE(hint->text(), QString("a new layer on Towers in 1 One"));
    QDropEvent drop(at, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &drop);
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    QCOMPARE(hint->text(), QString());
    delete mime;

    // Two groups with ⌥ held: the hint says a layer each, and so it is.
    FixtureGroup *usts = group("USTs", 2, 2);
    FixtureGroup *spots = group("Spots", 4, 2);
    QMimeData *two = new QMimeData();
    QByteArray g2;
    QDataStream gs2(&g2, QIODevice::WriteOnly);
    gs2 << usts->id() << spots->id();
    two->setData(FixtureGroupSource::fixtureGroupMimeType(), g2);
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode)
            cueItem = *it;
    tree->doItemsLayout();
    tree->scrollToItem(cueItem);
    const QPoint at2 = tree->visualItemRect(cueItem).center();
    QDragEnterEvent enter2(dragPos(at2), Qt::CopyAction, two, Qt::LeftButton, Qt::AltModifier);
    QCoreApplication::sendEvent(tree->viewport(), &enter2);
    QDragMoveEvent altMove(dragPos(at2), Qt::CopyAction, two, Qt::LeftButton, Qt::AltModifier);
    QCoreApplication::sendEvent(tree->viewport(), &altMove);
    QCOMPARE(hint->text(), QString("2 new layers in 1 One, one for each group"));
    QDropEvent altDrop(at2, Qt::CopyAction, two, Qt::LeftButton, Qt::AltModifier);
    QCoreApplication::sendEvent(tree->viewport(), &altDrop);
    QCOMPARE(st->cue(c1)->layers.count(), 3);

    // ...and onto a layer row: it joins that layer.
    QTreeWidgetItem *layerItem = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LayerNode
            && (*it)->text(0) == "Towers")
            layerItem = *it;
    QVERIFY(layerItem != nullptr);
    tree->doItemsLayout();
    tree->scrollToItem(layerItem);
    const QPoint at3 = tree->visualItemRect(layerItem).center();
    QDragEnterEvent enter3(dragPos(at3), Qt::CopyAction, two, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &enter3);
    QDragMoveEvent onLayer(dragPos(at3), Qt::CopyAction, two, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &onLayer);
    QVERIFY(onLayer.isAccepted());
    QCOMPARE(hint->text(), QString("add USTs + Spots to layer Towers in 1 One"));
    QDropEvent layerDrop(at3, Qt::CopyAction, two, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &layerDrop);
    QCOMPARE(st->cue(c1)->layers.count(), 3);
    QCOMPARE(st->cue(c1)->layers.first().target.groups.count(), 3);
    delete two;
}

void ShowTreeView_Test::anEffectGetsLooksOfItsOwn()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 red = colour(Qt::red, "Red");
    const quint32 blue = colour(Qt::blue, "Blue");
    QLCPalette *fx = new QLCPalette(QLCPalette::Effect, m_doc);
    fx->setName("Confetti");
    m_doc->addPalette(fx);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, t);

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.dropPalettes(c1, l1, { blue, fx->id() }));
    const QString key = st->keyForPalette(fx->id());
    QVERIFY(isEffectKey(key));
    QVERIFY(view.setEffectOwn(c1, l1, key, { red }));

    // The scene plays the base, then the effect with its own Red.
    const Collection *coll = qobject_cast<Collection *>(m_doc->function(st->cue(c1)->collection));
    const Scene *scene = qobject_cast<Scene *>(m_doc->function(coll->functions().first()));
    QCOMPARE(scene->palettes(), QList<quint32>({ blue, fx->id(), red }));
    QVERIFY(QLCPalette::isEffectScoped(m_doc, scene->palettes(), red));
    QVERIFY(view.selectNode(l1, c1));
    QVERIFY(rowFor(view.attributeRows(), keyLabel(key)).contains("Confetti in Red"));

    // Move a cue (dragged onto another): the list's order and its chaser follow.
    const quint32 c2 = st->addCue(list, 0, "Two");
    QCOMPARE(view.cueDropHint(c2, c1, false), QString("Move 2 Two before 1 One, as 0.5"));
    QVERIFY(view.dropCue(c2, c1, false));
    QCOMPARE(st->cueList(list)->cues.first().id, c2);
    const Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QCOMPARE(chaser->steps().first().fid, st->cue(c2)->collection);
    // With Alt: an include.
    QCOMPARE(view.cueDropHint(c2, c1, true), QString("1 One includes 0.5 Two"));
    QVERIFY(view.dropCue(c2, c1, true));
    QCOMPARE(st->cue(c1)->includes, QList<quint32>({ c2 }));
    QVERIFY(!view.dropCue(c2, c2, false));
}

void ShowTreeView_Test::anEffectRowSaysOneOffAndHowItRuns()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, t);
    const quint32 pid = st->createInlinePalette(QLCPalette::Effect, QVariantList(), "Confetti · One Towers");
    QVERIFY(pid != QLCPalette::invalidId());

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.dropPalettes(c1, l1, { pid }));
    const QString key = st->keyForPalette(pid);
    QVERIFY(isEffectKey(key));
    QVERIFY(view.selectNode(l1, c1));
    QLabel *row = view.findChild<QLabel *>(QStringLiteral("stRow:%1").arg(key));
    QVERIFY(row != nullptr);
    QVERIFY2(row->text().contains("<i>one-off</i>"), qPrintable(row->text()));
    QVERIFY(view.findChild<QLabel *>(QStringLiteral("stReuse:%1").arg(key)) != nullptr);

    // Its ⋯: Runs ▸ Once, then At the end ▸ Release.
    auto choose = [&](const char *action) {
        QTimer driver;
        driver.setInterval(20);
        bool done = false;
        connect(&driver, &QTimer::timeout, this, [&]() {
            QMenu *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (menu == nullptr)
                return;
            driver.stop();
            QMenu *runs = menu->findChild<QMenu *>("effectRunsMenu");
            QAction *a = runs == nullptr ? nullptr : runs->findChild<QAction *>(action);
            menu->close();
            if (a != nullptr && a->isEnabled())
                a->trigger();
            done = true;
        });
        driver.start();
        QVERIFY(view.selectNode(l1, c1));
        QToolButton *more = view.findChild<QToolButton *>(QStringLiteral("stMore:%1").arg(key));
        QVERIFY(more != nullptr);
        more->click();
        QTRY_VERIFY(done);
    };
    choose("effectRunsOnce");
    QCOMPARE(m_doc->palette(pid)->effectParamValues().value("__runs"), 1.0);
    choose("effectAtEndRelease");
    QCOMPARE(m_doc->palette(pid)->effectParamValues().value("__atEnd"), 1.0);
    choose("effectRunsLoop");
    QCOMPARE(m_doc->palette(pid)->effectParamValues().value("__runs"), 0.0);
}

/* Reordering and numbering (docs/features/show-tree.md): a list's order is
   its numbers' order. */
void ShowTreeView_Test::cuesReorderAndRenumber()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 c3 = st->addCue(list, 1, "Three");
    const quint32 c4 = st->addCue(list, 2, "Four");
    auto numbers = [st, list]() {
        QStringList out;
        const CueList *l = st->cueList(list);
        for (int i = 0; i < l->cues.count(); ++i)
            out << st->cueNumber(*l, i);
        return out;
    };
    ShowTreeView view(nullptr, m_doc);

    // Typed in the cue's Number: it moves to its place, still selected.
    QVERIFY(view.selectNode(c1));
    QLineEdit *number = view.findChild<QLineEdit *>("stCueNumber");
    number->setText("3.5");
    emit number->editingFinished();
    QCOMPARE(numbers(), QStringList({ "2", "3", "3.5", "4" }));
    QCOMPARE(view.currentCueId(), c1);
    QCOMPARE(number->text(), QString("3.5"));
    QVERIFY(st->undo());
    QCOMPARE(numbers(), QStringList({ "1", "2", "3", "4" }));

    // Inline, as the row reads: "2.5 Fog" -- number and name, one step.
    QVERIFY(view.renameCue(c4, "2.5", "Fog"));
    QCOMPARE(st->cueList(list)->cues.at(2).id, c4);
    QCOMPARE(st->cue(c4)->name, QString("Fog"));
    QCOMPARE(view.currentCueId(), c4);
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c4)->name, QString("Four"));
    QCOMPARE(st->cue(c4)->number, QString("4"));

    // Dragged: before or after a row, numbered for its new place; the
    // hint says the number first.
    bool refused = true;
    QCOMPARE(view.cuesDropHint({ c4 }, c2, true, &refused), QString("Move 4 Four after 2 Two, as 2.5"));
    QVERIFY(!refused);
    QVERIFY(view.dropCues({ c4 }, c2, true));
    QCOMPARE(numbers(), QStringList({ "1", "2", "2.5", "3" }));
    QCOMPARE(view.currentCueId(), c4);
    // A block keeps its order and stays selected.
    QCOMPARE(view.cuesDropHint({ c1, c2 }, c3, true), QString("Move 2 cues after 3 Three, as 4, 5"));
    QVERIFY(view.dropCues({ c2, c1 }, c3, true));
    QCOMPARE(numbers(), QStringList({ "2.5", "3", "4", "5" }));
    QCOMPARE(st->cueList(list)->cues.at(2).id, c1);
    QCOMPARE(view.selectedCues().count(), 2);
    QVERIFY(st->undo());
    QCOMPARE(numbers(), QStringList({ "1", "2", "2.5", "3" }));

    // No room: refused, and the hint says so.
    Cue three = *st->cue(c3);
    three.number = "2.5.0.0.1";
    QVERIFY(st->setCue(three));
    QString why;
    const QString hint = view.cuesDropHint({ c1 }, c4, true, &refused);
    QVERIFY(refused);
    QVERIFY(hint.contains("Renumber this level"));
    QVERIFY(!view.dropCues({ c1 }, c4, true, &why));
    QCOMPARE(why, hint);

    // Renumber this level (no dialog here), from 1 in twos: 1, 3; the point
    // cues under 2 follow it (2.5 -> 3.5).
    QVERIFY(!view.renumberLevel(c2, 1, 1, false));          // so already
    QVERIFY(view.renumberLevel(c2, 1, 2, false));
    QCOMPARE(numbers(), QStringList({ "1", "3", "3.5", "3.5.0.0.1" }));
    QCOMPARE(st->undoText(), QString("Renumber this level"));
    QVERIFY(st->undo());
    QCOMPARE(numbers(), QStringList({ "1", "2", "2.5", "2.5.0.0.1" }));
}

void ShowTreeView_Test::theLookEditorEditsARowsPalette()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, t);
    st->setLayerLook(c1, l1, st->lookForPalette(full));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QCOMPARE(view.editedPalette(), QLCPalette::invalidId());   // hidden until asked
    QVERIFY(view.selectNode(l1, c1));
    view.editPalette(full);
    QCOMPARE(view.editedPalette(), full);

    // An edit there tells every scene playing that palette; nothing re-bakes.
    const quint32 scene = qobject_cast<Collection *>(m_doc->function(st->cue(c1)->collection))->functions().first();
    QSignalSpy changed(m_doc, SIGNAL(functionChanged(quint32)));
    const int functions = m_doc->functions().count();
    m_doc->palette(full)->setValue(128);
    emit view.lookEditor()->paletteChanged(full);
    bool told = false;
    for (const QList<QVariant> &args : changed)
        if (args.first().toUInt() == scene)
            told = true;
    QVERIFY(told);
    QCOMPARE(m_doc->functions().count(), functions);
    QVERIFY(!st->canRedo());
}

/* QLC_SHOWTREE_SHOW=<a COPY of a show> QLC_SHOWTREE_SHOT=<png>: load it,
   import its first chaser and picture the tab -- a look at the real thing,
   offscreen. Skipped without the variables. */
void ShowTreeView_Test::selectingACuePreviewsItOnTheRig()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    const quint32 d50 = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    Target t1, t2;
    t1.groups << towers->id();
    t2.groups << usts->id();
    st->setLayerLook(c1, st->addLayer(c1, t1), st->lookForPalette(d50));
    const quint32 l2 = st->addLayer(c2, t2);
    st->setLayerLook(c2, l2, st->lookForPalette(d50));

    ShowTreeView view(nullptr, m_doc);
    // Off screen (another tab): selecting plays nothing.
    QVERIFY(view.selectNode(c1));
    QCOMPARE(st->previewedCue(), invalidId());

    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QCOMPARE(st->previewedCue(), c1);           // shown: the selected cue plays
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
    QCOMPARE(view.previewText(), QString("▶ On the rig: 1 One"));
    QCOMPARE(view.findChild<QLabel *>("stPreview")->text(), view.previewText());
    QVERIFY(!st->previewFunctions().isEmpty());

    // A layer in another cue plays that cue.
    QVERIFY(view.selectNode(l2, c2));
    QCOMPARE(st->previewedCue(), c2);
    // The list itself, or nothing: no preview.
    QVERIFY(view.selectNode(list));
    QCOMPARE(st->previewedCue(), invalidId());
    QVERIFY(view.previewText().isEmpty());

    // Leaving the tab stops it; coming back brings it back.
    QVERIFY(view.selectNode(c1));
    QCOMPARE(st->previewedCue(), c1);
    view.hide();
    QCOMPARE(st->previewedCue(), invalidId());
    QVERIFY(st->previewFunctions().isEmpty());
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QCOMPARE(st->previewedCue(), c1);

    // Live: blind. Selecting doesn't touch the stage, and the header says so.
    m_doc->setMode(Doc::Operate);
    QVERIFY(st->previewFunctions().isEmpty());
    QVERIFY(view.selectNode(c2));
    QVERIFY(st->previewFunctions().isEmpty());
    QVERIFY(view.previewText().startsWith("Live: editing is blind"));
    m_doc->setMode(Doc::Design);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
    view.hide();
}

/* The DMX the universe would send, by stepping the timer by hand (no output
   thread, nothing goes out). */
static QHash<quint32, uchar> settleDmx(Doc *doc, int ticks = 10)
{
    for (int i = 0; i < ticks; ++i)
    {
        doc->masterTimer()->timerTick();
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        doc->inputOutputMap()->releaseUniverses(false);
    }
    QHash<quint32, uchar> out;
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    for (int i = 0; i < 6; ++i)
        out.insert(quint32(i), uchar(ua.first()->preGMValues().at(i)));
    doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

static void goList(Doc *doc, Chaser *c)
{
    ChaserAction action;
    action.m_action = ChaserNextStep;
    action.m_stepIndex = -1;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    c->setAction(action);
    if (!c->isRunning())
        c->start(doc->masterTimer(), FunctionParent::master());
}

/* Branson 2026-10-17: in Design whatever is in edit is visible, also while
   its own list runs for real; in Perform the running list owns the stage. */
void ShowTreeView_Test::inDesignTheEditShowsOverItsRunningList()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 d50 = dimmer(127, "Half");
    const quint32 d100 = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "Full Show", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, t);
    st->setLayerLook(c1, l1, st->lookForPalette(d100));
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QVERIFY(chaser != nullptr);

    ShowTreeView view(nullptr, m_doc);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // The cue picked plays (LIVE_BLIND.md: the latest action wins).
    QVERIFY(view.selectNode(l1, c1));
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));

    // The list runs cue One at full, for real.
    goList(m_doc, chaser);
    QCOMPARE(int(settleDmx(m_doc)[0]), 255);
    QCoreApplication::processEvents();     // its start reaches the Show Tree

    // RC8: the list started, so the preview is off stage -- Held, with
    // Play beside the line; the same selection refreshed keeps it so.
    st->previewCue(c1);                 // what the view does on a refresh
    QCOMPARE(st->previewedCue(), c1);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Held));
    QCOMPARE(view.previewText(), QString("■ Held — Full Show running. Play to preview 1 One over it."));
    QToolButton *play = view.findChild<QToolButton *>("stPreviewPlay");
    QVERIFY(play != nullptr);
    // On the bar (its action shown; a narrow window may put it in the
    // bar's overflow).
    auto playShown = [play]() {
        QToolBar *bar = qobject_cast<QToolBar *>(play->parentWidget());
        if (bar == nullptr)
            return false;
        for (QAction *a : bar->actions())
            if (bar->widgetForAction(a) == play)
                return a->isVisible();
        return false;
    };
    QVERIFY(playShown());
    QCOMPARE(int(settleDmx(m_doc)[0]), 255);

    // Design, Play: the cue previews over the run, and an edit DOWN shows
    // (HTP stacking alone would keep the running 255).
    play->click();
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
    QVERIFY(!playShown());
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(d50)));
    QCoreApplication::processEvents();
    QCOMPARE(view.previewText(), QString("▶ On the rig: 1 One, over Full Show running"));
    QHash<quint32, uchar> v = settleDmx(m_doc);
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(int(v[1]), 127);
    QVERIFY(chaser->isRunning());

    // Stopping the preview (select the list): the running cue is back.
    QVERIFY(view.selectNode(list));
    QCOMPARE(st->previewedCue(), invalidId());
    v = settleDmx(m_doc);
    QCOMPARE(int(v[0]), 255);
    QVERIFY(chaser->isRunning());
    // ...and so it is on leaving the tab.
    QVERIFY(view.selectNode(c1));
    QCOMPARE(int(settleDmx(m_doc)[0]), 127);
    view.hide();
    QCOMPARE(int(settleDmx(m_doc)[0]), 255);
    QVERIFY(chaser->isRunning());
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // Perform (Live): the running list owns the stage; no preview, and an
    // edit lands on GO.
    chaser->stop(FunctionParent::master());
    settleDmx(m_doc);
    m_doc->setMode(Doc::Operate);
    goList(m_doc, chaser);
    QCOMPARE(int(settleDmx(m_doc)[0]), 127);     // cue One, as edited
    QVERIFY(view.selectNode(l1, c1));
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::NotDesign));
    QVERIFY(st->previewFunctions().isEmpty());
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(d100)));
    QCOMPARE(int(settleDmx(m_doc)[0]), 127);     // blind until GO
    QVERIFY(view.previewText().startsWith("Live: editing is blind"));
    m_doc->setMode(Doc::Design);
    view.hide();
}

/****************************************************************************
 * P2: tracking in the tab
 ****************************************************************************/

namespace
{
Target groupTarget(quint32 g)
{
    Target t;
    t.groups << g;
    return t;
}

QTreeWidgetItem *trackedRow(QTreeWidget *tree, quint32 cueId, const QString &text)
{
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::TrackedLayerNode
            && (*it)->data(0, ShowTreeView::CueRole).toUInt() == cueId && (*it)->text(0).contains(text))
            return *it;
    return nullptr;
}
}


/* LIVE_BLIND.md, Design: "if I select something in the tree editor it
   should be that look .. if I hit go .. it should be that look ..
   consistently" -- the latest action wins, a click on the cue already
   selected included. Operate: only the run is live; a tree edit is blind
   (and its line blue). */
void ShowTreeView_Test::theLatestActionWinsAndOperateIsBlind()
{
    FixtureGroup *a = group("A", 0, 1);
    FixtureGroup *b = group("B", 1, 1);
    FixtureGroup *c = group("C", 2, 1);
    const quint32 d100 = dimmer(255, "Full");
    const quint32 d50 = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "Run", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 c3 = st->addCue(list, 1, "Three");
    QList<quint32> layers;
    for (const QPair<quint32, FixtureGroup *> &cg : QList<QPair<quint32, FixtureGroup *> >
             { { c1, a }, { c2, b }, { c3, c } })
    {
        Target t;
        t.groups << cg.second->id();
        const quint32 l = st->addLayer(cg.first, t);
        layers << l;
        st->setLayerLook(cg.first, l, st->lookForPalette(d100));
    }
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QVERIFY(chaser != nullptr);

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    auto lit = [this]() {
        const QHash<quint32, uchar> v = settleDmx(m_doc);
        return QList<int>{ int(v[0]), int(v[1]), int(v[2]) };
    };
    auto goCue = [&](int index) {
        ShowControl::Request r;
        r.command = ShowControl::Go;
        r.chaser = chaser->id();
        r.cue = st->cueNumber(*st->cueList(list), index);
        QVERIFY(m_doc->showControl()->execute(r));
        settleDmx(m_doc, 2);                 // the run starts on a tick...
        QCoreApplication::processEvents();   // ...and the Show Tree hears of it
    };
    auto clickCue = [&](quint32 cue) {
        QTreeWidgetItem *it = nullptr;
        for (QTreeWidgetItemIterator i(tree); *i && it == nullptr; ++i)
            if ((*i)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
                && (*i)->data(0, ShowTreeView::IdRole).toUInt() == cue)
                it = *i;
        QVERIFY(it != nullptr);
        tree->scrollToItem(it);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(it).center());
        QCoreApplication::processEvents();
    };

    // Select: that look.
    QVERIFY(view.selectNode(c1));
    QCOMPARE(lit(), (QList<int>{ 255, 0, 0 }));
    // GO (cue Two): that look -- the run's, the selection held.
    goCue(1);
    QCOMPARE(lit(), (QList<int>{ 0, 255, 0 }));
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Held));
    // A click on the cue still selected: that look again, over the run.
    clickCue(c1);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
    QCOMPARE(lit(), (QList<int>{ 255, 255, 0 }));
    // GO again: the run's only.
    goCue(1);
    QCOMPARE(lit(), (QList<int>{ 0, 255, 0 }));
    // Another cue selected: that one.
    QVERIFY(view.selectNode(c3));
    QCOMPARE(lit(), (QList<int>{ 0, 255, 255 }));
    // GO: the run's.
    goCue(0);
    QCOMPARE(lit(), (QList<int>{ 255, 0, 0 }));
    QLabel *line = view.findChild<QLabel *>("stPreview");
    QVERIFY(line != nullptr);
    QVERIFY(!line->property("blind").toBool());

    // Operate: only the run is live. Selecting and editing a cue in the
    // tree changes nothing on stage, and the line says so in blue.
    m_doc->setMode(Doc::Operate);
    goCue(1);
    QCOMPARE(lit(), (QList<int>{ 0, 255, 0 }));
    QVERIFY(view.selectNode(c3));
    QVERIFY(st->setLayerLook(c3, layers.at(2), st->lookForPalette(d50)));
    clickCue(c3);
    QCOMPARE(lit(), (QList<int>{ 0, 255, 0 }));
    QVERIFY(line->property("blind").toBool());
    QVERIFY(line->styleSheet().contains(BlindStyle::colour()));
    QCOMPARE(st->editedCue(), c3);
    // ...until GO plays it, as edited.
    goCue(2);
    QCOMPARE(lit(), (QList<int>{ 0, 0, 127 }));

    chaser->stop(FunctionParent::master());
    m_doc->setMode(Doc::Design);
    view.hide();
}

void ShowTreeView_Test::trackingControlsOnTheListAndCuePages()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(towers->id())), st->lookForPalette(full));

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.selectNode(list));
    QComboBox *mode = view.findChild<QComboBox *>("stListMode");
    QComboBox *base = view.findChild<QComboBox *>("stListBase");
    QVERIFY(mode != nullptr && base != nullptr);
    QCOMPARE(base->count(), 3);                     // none + two cues
    QCOMPARE(base->currentIndex(), 0);

    // Tracking: cue 2 now carries Towers on, greyed in the tree.
    mode->setCurrentIndex(1);
    emit mode->activated(1);
    QCOMPARE(st->cueList(list)->mode, ListMode::Tracking);
    QVERIFY(trackedRow(view.tree(), c2, "Towers") != nullptr);
    QVERIFY(treeText(view.tree()).contains(QString::fromUtf8("  L [M \u21e2]")));   // one production (Q75)

    // The base from the list page...
    base->setCurrentIndex(1);
    emit base->activated(1);
    QCOMPARE(st->cueList(list)->baseCue, c1);
    QVERIFY(treeText(view.tree()).contains("    1 One [S 1, F base]"));
    // ...and from the cue page's check box.
    QVERIFY(view.selectNode(c2));
    QCheckBox *isBase = view.findChild<QCheckBox *>("stCueBase");
    QVERIFY(!isBase->isChecked());
    isBase->setChecked(true);
    QCOMPARE(st->cueList(list)->baseCue, c2);
    QVERIFY(view.selectNode(c2));
    QVERIFY(view.findChild<QCheckBox *>("stCueBase")->isChecked());
    view.findChild<QCheckBox *>("stCueBase")->setChecked(false);
    QCOMPARE(st->cueList(list)->baseCue, invalidId());

    // Cue mode: Block -- nothing tracked into it; the tree says so.
    QVERIFY(view.selectNode(c2));
    QComboBox *cueMode = view.findChild<QComboBox *>("stCueMode");
    cueMode->setCurrentIndex(cueMode->findData(int(CueMode::Block)));
    emit cueMode->activated(cueMode->currentIndex());
    QCOMPARE(st->cue(c2)->mode, CueMode::Block);
    QVERIFY(trackedRow(view.tree(), c2, "Towers") == nullptr);
    QVERIFY(treeText(view.tree()).contains("    2 Two [M B]"));
}

void ShowTreeView_Test::includesAddMoveRemoveAndNoLoops()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 c3 = st->addCue(list, 1, "Three");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(towers->id())), st->lookForPalette(full));
    st->setLayerLook(c2, st->addLayer(c2, groupTarget(usts->id())), st->lookForPalette(full));
    QVERIFY(st->setCueMode(c3, CueMode::Block));

    ShowTreeView view(nullptr, m_doc);
    // ⌥-drag of a cue onto another includes it -- not when it would loop.
    QCOMPARE(view.cueDropHint(c1, c3, true), QString("3 Three includes 1 One"));
    QVERIFY(view.dropCue(c1, c3, true));
    QVERIFY(view.cueDropHint(c1, c3, true).isEmpty());      // already
    QVERIFY(view.cueDropHint(c3, c1, true).isEmpty());      // 3 plays 1: a loop
    QVERIFY(!view.dropCue(c3, c1, true));
    QVERIFY(view.cueDropHint(c2, c1, true).isEmpty());      // 2 tracks from 1
    QVERIFY(st->addInclude(c3, c2));

    QVERIFY(view.selectNode(c3));
    QCOMPARE(view.includeItems(), QStringList({ "1 One", "2 Two" }));
    QListWidget *incs = view.findChild<QListWidget *>("stCueIncludes");
    QToolButton *up = view.findChild<QToolButton *>("stIncludeUp");
    QToolButton *down = view.findChild<QToolButton *>("stIncludeDown");
    QToolButton *remove = view.findChild<QToolButton *>("stIncludeRemove");
    QVERIFY(!remove->isEnabled());
    incs->setCurrentRow(1);
    QVERIFY(up->isEnabled());
    QVERIFY(!down->isEnabled());
    up->click();
    QCOMPARE(st->cue(c3)->includes, QList<quint32>({ c2, c1 }));
    QCOMPARE(view.includeItems(), QStringList({ "2 Two", "1 One" }));
    QCOMPARE(incs->currentRow(), 0);                        // the moved one stays selected
    remove->click();
    QCOMPARE(st->cue(c3)->includes, QList<quint32>({ c1 }));
    QCOMPARE(view.includeItems(), QStringList({ "1 One" }));
    // The included cue's layer shows in it, greyed with where it came from.
    QVERIFY(trackedRow(view.tree(), c3, "Towers") != nullptr);
    QCOMPARE(trackedRow(view.tree(), c3, "Towers")->text(1), QString("⊕ 1"));
    QVERIFY(trackedRow(view.tree(), c3, "Towers")->toolTip(1).contains("included from 1 One"));
}

void ShowTreeView_Test::nextCueReceivesSaysWhatPassesOn()
{
    FixtureGroup *at = group("AT-Spots", 0, 1);
    FixtureGroup *bs = group("BackStage", 1, 2);
    FixtureGroup *fx = group("Wash", 3, 1);
    const quint32 base = dimmer(60, "Base");
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c0 = st->addCue(list, -1, "base");
    const quint32 c1 = st->addCue(list, 0, "One");
    const quint32 c2 = st->addCue(list, 1, "Two");
    const quint32 c3 = st->addCue(list, 2, "Three");
    st->setLayerLook(c0, st->addLayer(c0, groupTarget(at->id())), st->lookForPalette(base));
    const quint32 al = st->addLayer(c1, groupTarget(at->id()));
    st->setLayerLook(c1, al, st->lookForPalette(full));
    st->setLayerRelease(c1, al, true);
    const quint32 bl = st->addLayer(c1, groupTarget(bs->id()));
    st->setLayerLook(c1, bl, st->lookForPalette(full));
    const quint32 wl = st->addLayer(c1, groupTarget(fx->id()));
    st->setLayerLook(c1, wl, st->lookForPalette(full));
    st->setLayerRelease(c1, wl, true);
    QVERIFY(st->setBaseCue(list, c0));

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.selectNode(c1));
    const QString next = view.nextCueText();
    QVERIFY2(next.contains("AT-Spots (Base) — back to the base"), qPrintable(next));
    QVERIFY2(next.contains("BackStage (Full)"), qPrintable(next));
    QVERIFY2(next.contains("Wash — released, dark"), qPrintable(next));

    QVERIFY(st->setCueMode(c2, CueMode::CueOnly));
    QVERIFY(view.selectNode(c2));
    QVERIFY(view.nextCueText().startsWith("what this cue started from (it is cue-only)"));
    QVERIFY(st->setCueMode(c3, CueMode::Block));
    QVERIFY(view.selectNode(c2));
    QCOMPARE(view.nextCueText(), QString("nothing: 4 Three is a Block cue, it starts from nothing"));
    QVERIFY(view.selectNode(c3));
    QCOMPARE(view.nextCueText(), QString("— (the last cue)"));
}

void ShowTreeView_Test::aCarriedInLayerCanBeReleased()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 c3 = st->addCue(list, 1, "Three");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(towers->id())), st->lookForPalette(full));

    ShowTreeView view(nullptr, m_doc);
    view.tree()->setCurrentItem(trackedRow(view.tree(), c2, "Towers"));
    QCheckBox *release = view.findChild<QCheckBox *>("stLayerRelease");
    QVERIFY(!release->isHidden());
    release->setChecked(true);
    // Cue 2's own now, released; the selection moved onto it; cue 3 dark.
    QCOMPARE(st->cue(c2)->layers.count(), 1);
    QVERIFY(st->cue(c2)->layers.first().release);
    QCOMPARE(view.currentKind(), ShowTreeView::LayerNode);
    QCOMPARE(view.currentId(), st->cue(c2)->layers.first().id);
    QCOMPARE(rowFor(view.attributeRows(), "Dimmer"), QString("Dimmer | Full | tracked from 1 One"));
    QVERIFY(trackedRow(view.tree(), c3, "Towers") == nullptr);
    QCOMPARE(st->undoText(), QString("Release layer"));
}

void ShowTreeView_Test::updateCueWhereATrackedValueWasSet()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(towers->id())), st->lookForPalette(full));

    ShowTreeView view(nullptr, m_doc);
    // The picker of a tracked value names where it was set (D22).
    QCOMPARE(view.updateSourceLabel(c2, groupTarget(towers->id()), KeyDimmer), QString("1 One"));
    QVERIFY(view.updateSourceLabel(c1, groupTarget(towers->id()), KeyDimmer).isEmpty());
    QVERIFY(view.updateSourceLabel(c2, groupTarget(towers->id()), KeyColour).isEmpty());

    // Updating there: cue 2 still tracks it, with the new value.
    view.tree()->setCurrentItem(trackedRow(view.tree(), c2, "Towers"));
    QVERIFY(st->updateSource(c2, groupTarget(towers->id()), st->lookForPalette(half)));
    QCOMPARE(rowFor(view.attributeRows(), "Dimmer"), QString("Dimmer | Half | tracked from 1 One"));
    QVERIFY(st->cue(c2)->layers.isEmpty());
}

void ShowTreeView_Test::recordAsNewCueFromTheHeader()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(towers->id())), st->lookForPalette(full));
    st->setLayerLook(c2, st->addLayer(c2, groupTarget(usts->id())), st->lookForPalette(full));

    ShowTreeView view(nullptr, m_doc);
    QToolButton *record = view.findChild<QToolButton *>("stRecord");
    QVERIFY(view.selectNode(list));
    QVERIFY(!record->isEnabled());
    QVERIFY(view.selectNode(c2));
    QVERIFY(record->isEnabled());
    record->click();
    const CueList *l = st->cueList(list);
    QCOMPARE(l->cues.count(), 3);
    const Cue &rec = l->cues.at(2);
    QCOMPARE(view.currentId(), rec.id);
    QCOMPARE(rec.number, QString("3"));
    QCOMPARE(rec.mode, CueMode::Block);
    QCOMPARE(rec.layers.count(), 2);                 // Towers (tracked) and USTs, now its own
    QVERIFY(treeText(view.tree()).contains("    3 Two (recorded) [S 2, M B]"));
    // What changed on stage: only the new cue.
    QCOMPARE(view.findChild<QLabel *>("stDropHint")->text(), QString("Changed: 3 Two (recorded)"));
}

void ShowTreeView_Test::theLibraryShowsWhatTheListUses()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 a = st->addCueList(prod, "A", ListMode::NonTracking);
    const quint32 b = st->addCueList(prod, "B", ListMode::NonTracking);
    const quint32 a1 = st->addCue(a, -1, "One");
    const quint32 a2 = st->addCue(a, 0, "Two");
    const quint32 b1 = st->addCue(b, -1, "Other");
    st->setLayerLook(a1, st->addLayer(a1, groupTarget(towers->id())), st->lookForPalette(full));
    st->setLayerLook(a2, st->addLayer(a2, groupTarget(towers->id())), st->lookForPalette(half));
    st->setLayerLook(a2, st->addLayer(a2, groupTarget(usts->id())), st->lookForPalette(full));
    st->setLayerLook(b1, st->addLayer(b1, groupTarget(usts->id())), st->lookForPalette(half));

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.inListItems().isEmpty());          // no list selected: no section
    QVERIFY(view.selectNode(a2));
    QTest::qWait(0);
    QCoreApplication::processEvents();
    QCOMPARE(view.inListItems(), QStringList({ "Towers (2) ×2", "Full ×2", "USTs (2) ×1", "Half ×1" }));
    QVERIFY(view.selectNode(b1));
    QCoreApplication::processEvents();
    QCOMPARE(view.inListItems(), QStringList({ "USTs (2) ×1", "Half ×1" }));
    // An edit counts at once.
    st->setLayerLook(b1, st->addLayer(b1, groupTarget(towers->id())), st->lookForPalette(half));
    QCoreApplication::processEvents();
    QCOMPARE(view.inListItems(), QStringList({ "Half ×2", "Towers (2) ×1", "USTs (2) ×1" }));
    // The filter narrows it too.
    view.findChild<QLineEdit *>("stLibraryFilter")->setText("Tow");
    bool shown = false;
    for (QTreeWidgetItemIterator it(view.library()); *it; ++it)
        if ((*it)->text(0) == "Towers (2) ×1" && !(*it)->isHidden() && (*it)->parent()->data(0, ShowTreeView::IdRole).toUInt() == 3)
            shown = true;
    QVERIFY(shown);
}

void ShowTreeView_Test::theCommandLine()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    colour(Qt::blue, "Blue");
    dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");

    ShowTreeView view(nullptr, m_doc);
    // Always there, at the bottom; no toggle. It never takes the keyboard on
    // its own: : in the tree does, Esc goes back.
    QVERIFY(view.isCommandLineVisible());
    QVERIFY(view.findChild<QToolButton *>("stCommandToggle") == nullptr);
    QCOMPARE(view.commandLine()->focusPolicy(), Qt::ClickFocus);
    QVERIFY(view.commandLine()->placeholderText().contains(": from the tree"));
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    // Below the tree and the pages.
    QVERIFY(view.commandLine()->mapTo(&view, QPoint(0, 0)).y()
            >= view.tree()->mapTo(&view, QPoint(0, 0)).y() + view.tree()->height());
    view.activateWindow();
    view.tree()->setFocus();
    QVERIFY(!view.commandLine()->hasFocus());
    QTest::keyClick(view.tree(), Qt::Key_Colon, Qt::ShiftModifier);
    QVERIFY(view.commandLine()->hasFocus() || !view.isActiveWindow());
    QTest::keyClick(view.commandLine(), Qt::Key_Escape);
    QVERIFY(view.tree()->hasFocus() || !view.isActiveWindow());

    QVERIFY(view.selectNode(prod));
    QVERIFY(view.runCommand("list \"01-Opener\""));
    QCOMPARE(view.currentKind(), ShowTreeView::ListNode);
    QVERIFY(view.runCommand("cue + \"base\" block"));
    QCOMPARE(view.currentKind(), ShowTreeView::CueNode);
    const quint32 c1 = view.currentId();
    QCOMPARE(st->cue(c1)->number, QString("1.0.0"));
    QCOMPARE(st->cue(c1)->mode, CueMode::Block);

    // Groups and looks: the layer it set is selected.
    QVERIFY(view.runCommand("Towers colour Blue dimmer Full"));
    QCOMPARE(view.currentKind(), ShowTreeView::LayerNode);
    QCOMPARE(rowFor(view.attributeRows(), "Colour"), QString("Colour | Blue | "));
    QVERIFY(view.commandMessage().startsWith("✓"));

    // A mistake says so and changes nothing.
    QVERIFY(!view.runCommand("Towers colour Mauve"));
    QVERIFY(view.commandMessage().contains("Mauve"));

    // Typed, with Tab and Enter, in the line itself.
    QLineEdit *line = view.commandLine();
    view.focusCommandLine();                    // as a click (or :) does
    line->setText("cue + \"Verse\"");
    QTest::keyClick(line, Qt::Key_Return);
    QVERIFY(line->text().isEmpty());
    const quint32 c2 = view.currentCueId();
    QVERIFY(c2 != c1);
    line->setText("Tow");
    QTest::keyClick(line, Qt::Key_Tab);
    QCOMPARE(line->text(), QString("Towers "));
    QVERIFY(line->hasFocus() || !view.isActiveWindow());       // Tab didn't move on
    QTest::keyClicks(line, "dimmer 40");
    QTest::keyClick(line, Qt::Key_Return);
    QCOMPARE(st->cue(c2)->layers.count(), 1);
    // A level typed: a named palette (none played 40 %), not a one-off.
    const quint32 d40 = st->cue(c2)->layers.first().look(KeyDimmer)->palettes.first();
    QVERIFY(!st->isInlinePalette(d40));
    QCOMPARE(m_doc->palette(d40)->name(), QString("Dimmer 40%"));
    // ↑ recalls.
    QTest::keyClick(line, Qt::Key_Up);
    QCOMPARE(line->text(), QString("Towers dimmer 40"));
    QTest::keyClick(line, Qt::Key_Up);
    QCOMPARE(line->text(), QString("cue + \"Verse\""));
    QTest::keyClick(line, Qt::Key_Down);
    QTest::keyClick(line, Qt::Key_Down);
    QVERIFY(line->text().isEmpty());

    // Tab again: the next match, the one shown in [ ].
    line->setText("record ");
    QTest::keyClick(line, Qt::Key_Tab);
    QCOMPARE(view.commandMessage(), QString("lit · running · selected"));
    QTest::keyClick(line, Qt::Key_Tab);
    QCOMPARE(line->text(), QString("record lit"));
    QCOMPARE(view.commandMessage(), QString("[lit] · running · selected"));
    QTest::keyClick(line, Qt::Key_Tab);
    QCOMPARE(line->text(), QString("record running"));
    line->clear();

    // undo: the whole last command.
    QVERIFY(view.runCommand("undo"));
    QVERIFY(st->cue(c2)->layers.isEmpty());
    Q_UNUSED(towers)
    view.hide();
}

/* Branson, 2026-10-17: a cue with one layer on two RGB pixel-bar groups,
   Colour Blue and Dimmer 19%, selected in Design -- "the truss bars aren't
   blue on the Rig Overview". The engine side, end to end: the preview runs
   the cue's scene and every head of both groups goes out blue at 19% (the
   Dimmer look scales the colour of a fixture with no dimmer channel); the
   Rig Overview's half is in monitor_test (it counted that 19% twice). In
   Operate nothing reaches the stage and the header doesn't claim it does. */
void ShowTreeView_Test::aDimmedColourOnPixelBarsReachesTheUniverse()
{
    // An RGB pixel bar with no dimmer channel: 4 heads, R G B each.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("RGB Bar 4");
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("12ch");
    for (int h = 0; h < 4; ++h)
    {
        const QLCChannel::PrimaryColour cols[3] = { QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue };
        QLCFixtureHead head;
        for (int k = 0; k < 3; ++k)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("%1-%2").arg(h + 1).arg(k));
            ch->setGroup(QLCChannel::Intensity);
            ch->setColour(cols[k]);
            def->addChannel(ch);
            mode->insertChannel(ch, quint32(h * 3 + k));
            head.addChannel(quint32(h * 3 + k));
        }
        mode->insertHead(-1, head);
    }
    def->addMode(mode);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));
    QList<Fixture *> bars;
    for (int i = 0; i < 3; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Bar %1").arg(i + 1));
        fxi->setFixtureDefinition(def, mode);
        fxi->setUniverse(0);
        fxi->setAddress(quint32(100 + i * 12));
        QVERIFY(m_doc->addFixture(fxi));
        bars << fxi;
    }
    FixtureGroup *trussC = new FixtureGroup(m_doc);
    trussC->setName("TrussC-RGB");
    m_doc->addFixtureGroup(trussC);
    trussC->assignFixture(bars[0]->id());
    FixtureGroup *trussW = new FixtureGroup(m_doc);
    trussW->setName("T TrussW-RGB");
    m_doc->addFixtureGroup(trussW);
    trussW->assignFixture(bars[1]->id());
    const quint32 blue = colour(Qt::blue, "Blue");
    const quint32 base = dimmer(48, "Base");

    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "00-preshow", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "Setup");
    ShowTreeView view(nullptr, m_doc);
    view.resize(1000, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.selectNode(c1));
    // Both groups dropped together: one layer, then its looks.
    QVERIFY(view.dropTargets(c1, { trussC->id(), trussW->id() }, {}));
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    const quint32 layer = st->cue(c1)->layers.first().id;
    QVERIFY(view.dropPalettes(c1, layer, { blue }));
    QVERIFY(view.dropPalettes(c1, layer, { base }));
    QVERIFY(view.selectNode(c1));
    QVERIFY2(view.previewText().startsWith("▶ On the rig: ") && view.previewText().endsWith("Setup"),
             qPrintable(view.previewText()));

    auto rgb = [this](Fixture *f, int head) {
        for (int i = 0; i < 10; ++i)
        {
            m_doc->masterTimer()->timerTick();
            QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
            ua.first()->processFaders();
            m_doc->inputOutputMap()->releaseUniverses(false);
        }
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        const QByteArray v = *ua.first()->postGMValues();
        m_doc->inputOutputMap()->releaseUniverses(false);
        const int at = int(f->address()) + head * 3;
        return QColor(uchar(v.at(at)), uchar(v.at(at + 1)), uchar(v.at(at + 2)));
    };
    // Blue at 19% (48/255) on every head of both groups; the third bar dark.
    for (int b = 0; b < 2; ++b)
        for (int h = 0; h < 4; ++h)
        {
            const QColor c = rgb(bars[b], h);
            QVERIFY2(c.red() == 0 && c.green() <= 1 && qAbs(c.blue() - 48) <= 1,
                     qPrintable(QString("bar %1 head %2: %3").arg(b + 1).arg(h).arg(c.name())));
        }
    QCOMPARE(rgb(bars[2], 0), QColor(0, 0, 0));

    // An edit shows at once: Dimmer cleared, full blue.
    QVERIFY(view.clearAttribute(c1, layer, KeyDimmer));
    QCOMPARE(rgb(bars[1], 3).blue(), 255);

    // Operate: blind. The header says so, and nothing new reaches the stage.
    m_doc->setMode(Doc::Operate);
    QVERIFY(view.previewText().startsWith("Live: editing is blind"));
    QVERIFY(!view.previewText().contains("On the rig"));
    QCOMPARE(rgb(bars[0], 0), QColor(0, 0, 0));
    m_doc->setMode(Doc::Design);
    QCOMPARE(rgb(bars[0], 0).blue(), 255);
    view.hide();
}

static QMimeData *idsMime(const char *type, const QList<quint32> &ids)
{
    QMimeData *mime = new QMimeData();
    QByteArray data;
    QDataStream ds(&data, QIODevice::WriteOnly);
    for (quint32 id : ids)
        ds << id;
    mime->setData(QLatin1String(type), data);
    return mime;
}

/* Drag over @a w at @a at, then let go: the hint while over it, and whether
   the move was taken. */
static QString dragAndDrop(QWidget *w, const QPoint &at, QMimeData *mime, QLabel *hint,
                           Qt::KeyboardModifiers mods = Qt::NoModifier, bool *accepted = nullptr)
{
    QDragEnterEvent enter(dragPos(at), Qt::CopyAction, mime, Qt::LeftButton, mods);
    QCoreApplication::sendEvent(w, &enter);
    QDragMoveEvent move(dragPos(at), Qt::CopyAction, mime, Qt::LeftButton, mods);
    QCoreApplication::sendEvent(w, &move);
    if (accepted != nullptr)
        *accepted = move.isAccepted();
    const QString said = hint->text();
    QDropEvent drop(at, Qt::CopyAction, mime, Qt::LeftButton, mods);
    QCoreApplication::sendEvent(w, &drop);
    delete mime;
    return said;
}

/* Branson, 2026-10-17: "When I have a cue loaded I should be able to just
   drag to the main window and it adds layers -- and onto a layer to add to
   it." The cue page takes every drop the tree does: groups on the page are
   one new layer (⌥: one each), on a layer's row join it (a carried-in
   layer is brought into the cue), palettes are cue looks or the row's
   layer's, functions play as they are. The cue stays selected. */
void ShowTreeView_Test::dropsOnTheCuePage()
{
    FixtureGroup *towers = group("Towers", 0, 1);
    FixtureGroup *usts = group("USTs", 1, 1);
    FixtureGroup *spots = group("Spots", 2, 1);
    FixtureGroup *floor = group("Floor", 3, 1);
    FixtureGroup *side = group("Side", 4, 1);
    const quint32 blue = colour(Qt::blue, "Blue");
    const quint32 red = colour(Qt::red, "Red");
    Scene *fn = new Scene(m_doc);
    fn->setName("Haze look");
    m_doc->addFunction(fn);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 c3 = st->addCue(list, 1, "Three");
    Target tt;
    tt.groups << towers->id();
    st->setLayerLook(c1, st->addLayer(c1, tt), st->lookForPalette(blue));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QLabel *hint = view.findChild<QLabel *>("stDropHint");
    QWidget *page = view.findChild<QWidget *>("stCuePage");
    QLabel *zone = view.findChild<QLabel *>("stCueDropZone");
    QVERIFY(page != nullptr && zone != nullptr);

    // An empty cue: the drop area says what to do.
    QVERIFY(view.selectNode(c3));
    QVERIFY(zone->isVisible());
    QCOMPARE(zone->text(), QString("Drag groups or fixtures here to add a layer — onto a layer to add to it"));
    QVERIFY(zone->styleSheet().contains("dashed"));

    // Cue Two: Towers carried in, a grey row of its own.
    QVERIFY(view.selectNode(c2));
    QCOMPARE(view.cueLayerItems(), QStringList({ "⤳ Towers   Blue · tracked from 1 One" }));

    // Two groups on the page: ONE layer, the hint says so, the cue stays.
    const QPoint onZone = zone->mapTo(page, zone->rect().center());
    bool taken = false;
    QCOMPARE(dragAndDrop(page, onZone, idsMime(FixtureGroupSource::fixtureGroupMimeType(),
                                               { usts->id(), spots->id() }), hint, Qt::NoModifier, &taken),
             QString("one layer on USTs + Spots in 2 Two (⌥: a layer for each group)"));
    QVERIFY(taken);
    QCOMPARE(st->cue(c2)->layers.count(), 1);
    QCOMPARE(st->cue(c2)->layers.first().target.groups.count(), 2);
    QCOMPARE(view.currentKind(), ShowTreeView::CueNode);
    QCOMPARE(view.currentId(), c2);
    QCOMPARE(st->previewedCue(), c2);           // Design: the rig shows it
    QVERIFY(hint->text().startsWith("Changed:") || hint->text().isEmpty());
    QVERIFY(zone->styleSheet().contains("dashed"));     // lit only while over it

    // ⌥: one layer each.
    QCOMPARE(dragAndDrop(page, onZone, idsMime(FixtureGroupSource::fixtureGroupMimeType(),
                                               { floor->id(), side->id() }), hint, Qt::AltModifier),
             QString("2 new layers in 2 Two, one for each group"));
    QCOMPARE(st->cue(c2)->layers.count(), 3);
    st->undo();
    QCOMPARE(st->cue(c2)->layers.count(), 1);

    // Onto the USTs + Spots row: added to that layer.
    QListWidget *rows = view.cueLayerList();
    QCOMPARE(rows->count(), 2);
    auto rowAt = [rows](const QString &start) {
        for (int i = 0; i < rows->count(); ++i)
            if (rows->item(i)->text().startsWith(start))
                return rows->visualItemRect(rows->item(i)).center();
        return QPoint(-1, -1);
    };
    const quint32 both = st->cue(c2)->layers.first().id;
    QCOMPARE(dragAndDrop(rows->viewport(), rowAt("USTs + Spots"),
                         idsMime(FixtureGroupSource::fixtureGroupMimeType(), { floor->id() }), hint),
             QString("add Floor to layer USTs + Spots in 2 Two"));
    QCOMPARE(st->cue(c2)->layers.count(), 1);
    QCOMPARE(st->cue(c2)->layer(both)->target.groups.count(), 3);
    QCOMPARE(view.currentId(), c2);

    // A palette on the row: that layer's look.
    QVERIFY(dragAndDrop(rows->viewport(), rowAt("USTs + Spots + Floor"),
                        idsMime(FunctionsTreeWidget::paletteDragMimeType(), { red }), hint)
            .startsWith("Red → layer USTs + Spots + Floor in 2 Two"));
    QVERIFY(st->cue(c2)->layer(both)->look(KeyColour) != nullptr);

    // Onto the carried-in Towers row: brought into the cue with Side added,
    // its Blue kept -- one undo step.
    QCOMPARE(dragAndDrop(rows->viewport(), rowAt("⤳ Towers"),
                         idsMime(FixtureGroupSource::fixtureGroupMimeType(), { side->id() }), hint),
             QString("add Side to layer Towers in 2 Two (brings it into this cue)"));
    QCOMPARE(st->cue(c2)->layers.count(), 2);
    Target ts;
    ts.groups << towers->id() << side->id();
    {
        const CueList *l = st->cueList(list);
        const QList<ResolvedCue> rc = resolve(*l);
        const ResolvedLayer *rl = rc.at(1).layer(ts);
        QVERIFY(rl != nullptr && rl->own);
        QVERIFY(rl->values.contains(KeyColour));
        QCOMPARE(rl->values.value(KeyColour).look.palettes, QList<quint32>({ blue }));
    }
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c2)->layers.count(), 1);
    QVERIFY(view.cueLayerItems().contains("⤳ Towers   Blue · tracked from 1 One"));

    // A palette on the page: a cue look.
    QVERIFY(dragAndDrop(page, onZone, idsMime(FunctionsTreeWidget::paletteDragMimeType(), { red }), hint)
            .startsWith("Red → cue look on 2 Two"));
    QVERIFY(st->cue(c2)->look(KeyColour) != nullptr);

    // A function on the page: played as it is.
    QCOMPARE(dragAndDrop(page, onZone, idsMime(FunctionsTreeWidget::functionDragMimeType(), { fn->id() }), hint),
             QString("played as it is in 2 Two"));
    QCOMPARE(st->cue(c2)->layers.count(), 2);
    QVERIFY(st->cue(c2)->layers.last().target.isOpaque());

    // A click on a row opens that layer.
    QTest::mouseClick(rows->viewport(), Qt::LeftButton, Qt::NoModifier, rowAt("USTs + Spots"));
    QCOMPARE(view.currentKind(), ShowTreeView::LayerNode);
    QCOMPARE(view.currentId(), both);
    view.hide();
}

/* The layer page: several groups dropped on it all join the layer; on a
   carried-in layer's page they bring it into the cue. The tree takes the
   same on a carried-in row. */
void ShowTreeView_Test::dropsOnTheLayerPage()
{
    FixtureGroup *towers = group("Towers", 0, 1);
    FixtureGroup *usts = group("USTs", 1, 1);
    FixtureGroup *spots = group("Spots", 2, 1);
    FixtureGroup *floor = group("Floor", 3, 1);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 c3 = st->addCue(list, 1, "Three");
    Target tt;
    tt.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, tt);

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QLabel *hint = view.findChild<QLabel *>("stDropHint");
    QWidget *page = view.findChild<QWidget *>("stLayerPage");

    // Own layer: both groups join it.
    QVERIFY(view.selectNode(l1, c1));
    QCOMPARE(dragAndDrop(page, QPoint(20, 20), idsMime(FixtureGroupSource::fixtureGroupMimeType(),
                                                        { usts->id(), spots->id() }), hint),
             QString("add USTs + Spots to layer Towers in 1 One"));
    QCOMPARE(st->cue(c1)->layer(l1)->target.groups.count(), 3);

    // Carried into Two: the drop brings it in, with Floor.
    Target all3;
    all3.groups << towers->id() << usts->id() << spots->id();
    QVERIFY(view.selectTracked(c2, all3.signature()));
    QCOMPARE(view.currentKind(), ShowTreeView::TrackedLayerNode);
    QCOMPARE(dragAndDrop(page, QPoint(20, 20), idsMime(FixtureGroupSource::fixtureGroupMimeType(),
                                                        { floor->id() }), hint),
             QString("add Floor to layer Towers + USTs + Spots in 2 Two (brings it into this cue)"));
    QCOMPARE(st->cue(c2)->layers.count(), 1);
    QCOMPARE(st->cue(c2)->layers.first().target.groups.count(), 4);
    QCOMPARE(view.currentKind(), ShowTreeView::LayerNode);

    // The tree: a carried-in row in Three takes groups too.
    QTreeWidget *tree = view.tree();
    QTreeWidgetItem *carried = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::TrackedLayerNode
            && (*it)->data(0, ShowTreeView::CueRole).toUInt() == c3)
            carried = *it;
    QVERIFY(carried != nullptr);
    tree->scrollToItem(carried);
    const QPoint at = tree->visualItemRect(carried).center();
    const Target was = view.carriedTarget(c3, carried->data(0, ShowTreeView::TargetRole).toString());
    QVERIFY(!was.isEmpty());
    FixtureGroup *side = group("Side", 4, 1);
    bool taken = false;
    QVERIFY(dragAndDrop(tree->viewport(), at, idsMime(FixtureGroupSource::fixtureGroupMimeType(), { side->id() }),
                        hint, Qt::NoModifier, &taken).endsWith("(brings it into this cue)"));
    QVERIFY(taken);
    QCOMPARE(st->cue(c3)->layers.count(), 1);
    QCOMPARE(st->cue(c3)->layers.first().target.groups.count(), 5);
    view.hide();
}

void ShowTreeView_Test::pictureOfARealShow()
{
    const QString path = qEnvironmentVariable("QLC_SHOWTREE_SHOW");
    const QString shot = qEnvironmentVariable("QLC_SHOWTREE_SHOT");
    if (path.isEmpty() || shot.isEmpty())
        QSKIP("set QLC_SHOWTREE_SHOW and QLC_SHOWTREE_SHOT to picture a real show");

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader r(&file);
    bool loaded = false;
    while (!r.atEnd() && !loaded)
        if (r.readNextStartElement() && r.name() == QLatin1String("Engine"))
            loaded = m_doc->loadXML(r, false);
    QVERIFY(loaded);

    ShowTreeView view(nullptr, m_doc);
    view.resize(1500, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    quint32 chaser = Function::invalidId();
    for (Function *f : m_doc->functions())
        if (f->type() == Function::ChaserType && f->name() == qEnvironmentVariable("QLC_SHOWTREE_CHASER", "Full Show"))
            chaser = f->id();
    QVERIFY(chaser != Function::invalidId());
    view.importChaser(chaser, false);
    const CueList *list = nullptr;
    for (const Production &p : m_doc->showTree()->productions())
        for (const CueList &l : p.lists)
            list = &l;
    QVERIFY(list != nullptr);
    // QLC_SHOWTREE_TRACKING=1: the list made Tracking (on the copy in
    // memory), the command line shown.
    const bool tracking = !qEnvironmentVariableIsEmpty("QLC_SHOWTREE_TRACKING");
    if (tracking)
    {
        m_doc->showTree()->setListMode(list->id, ListMode::Tracking);
        // An edit replaces the tree: read the list again.
        for (const Production &p : m_doc->showTree()->productions())
            for (const CueList &l : p.lists)
                list = &l;
    }
    const int index = qMin(2, int(list->cues.count()) - 1);
    const Cue &cue = list->cues.at(index);
    if (!cue.layers.isEmpty())
    {
        view.selectNode(cue.layers.first().id, cue.id);
        if (!cue.layers.first().looks.isEmpty())
            view.editPalette(cue.layers.first().looks.first().palettes.value(0));
    }
    else
        view.selectNode(cue.id);
    QTest::qWait(50);
    QVERIFY(view.grab().save(shot));
    view.selectNode(cue.id);
    QTest::qWait(50);
    QVERIFY(view.grab().save(QString(shot).replace(".png", "-cue.png")));
}

/* A picture of a real show's tree with its tracking expanded:
   QLC_TRACKTREE_SHOW (a .qlcc, read only) and QLC_TRACKTREE_SHOT (a .png).
   The tree pane is kept at its default share of a 1280-wide tab, every cue
   open, the second cue selected -- what tracks into it is what shows. */
void ShowTreeView_Test::pictureOfTrackingInTheTree()
{
    const QString path = qEnvironmentVariable("QLC_TRACKTREE_SHOW");
    const QString shot = qEnvironmentVariable("QLC_TRACKTREE_SHOT");
    if (path.isEmpty() || shot.isEmpty())
        QSKIP("set QLC_TRACKTREE_SHOW and QLC_TRACKTREE_SHOT to picture a show's tracking");

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader r(&file);
    bool loaded = false;
    while (!r.atEnd() && !loaded)
        if (r.readNextStartElement() && r.name() == QLatin1String("Engine"))
            loaded = m_doc->loadXML(r, false);
    QVERIFY(loaded);

    // QLC_TRACKTREE_LONG=1: (in memory) nothing released, every cue's
    // layers carried on -- a long list of what tracks in.
    if (!qEnvironmentVariableIsEmpty("QLC_TRACKTREE_LONG"))
    {
        ShowTreeController *st = m_doc->showTree();
        QList<QPair<quint32, quint32>> released;
        for (const Production &p : st->productions())
            for (const CueList &l : p.lists)
                for (const Cue &c : l.cues)
                    for (const Layer &y : c.layers)
                        if (y.release)
                            released << qMakePair(c.id, y.id);
        for (const auto &r : released)
            QVERIFY(st->setLayerRelease(r.first, r.second, false));
    }
    QSettings().remove(QStringLiteral("showtree/splitter2"));
    QSettings().remove(QStringLiteral("showtree/badgeColumn2"));
    ShowTreeView view(nullptr, m_doc);
    view.resize(1280, 760);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    const CueList *list = nullptr;
    for (const Production &p : m_doc->showTree()->productions())
        for (const CueList &l : p.lists)
            if (list == nullptr && l.cues.count() > 1)
                list = &l;
    QVERIFY(list != nullptr);
    view.selectNode(list->cues.at(1).id);
    view.tree()->expandAll();
    QTest::qWait(50);
    QVERIFY(view.grab().save(shot));
    QVERIFY(view.tree()->grab().save(QString(shot).replace(".png", "-tree.png")));
    // The cue row's tooltip, as text beside the picture.
    QFile tip(QString(shot).replace(".png", "-tooltip.txt"));
    QVERIFY(tip.open(QIODevice::WriteOnly));
    tip.write(view.cueTrackingTip(list->cues.at(1).id).toUtf8());
}

/****************************************************************************
 * P2 answers (2026-10-17)
 ****************************************************************************/

/* Record asks which stage when the choices differ; "Don't ask again" (or
   Settings → Show Tree) sets the default per mode, which the command line
   uses too; a word on the command line overrides it. */
void ShowTreeView_Test::recordAsksWhenTheChoicesDiffer()
{
    QSettings settings;
    const QVariant wasDesign = settings.value(ShowTreeView::recordSettingKey(false));
    const QVariant wasPerform = settings.value(ShowTreeView::recordSettingKey(true));
    settings.remove(ShowTreeView::recordSettingKey(false));
    settings.remove(ShowTreeView::recordSettingKey(true));
    QCOMPARE(ShowTreeView::recordSetting(false), QString("ask"));

    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(towers->id())), st->lookForPalette(full));
    st->setLayerLook(c2, st->addLayer(c2, groupTarget(usts->id())), st->lookForPalette(full));

    ShowTreeView view(nullptr, m_doc);
    QList<QList<ShowTreeView::RecordSource>> asked;
    ShowTreeView::RecordSource answer = ShowTreeView::RecordSource::Running;
    bool remember = false;
    view.setRecordAsker([&](const QList<ShowTreeView::RecordSource> &choices, ShowTreeView::RecordSource *chosen,
                            bool *dontAsk) {
        asked << choices;
        *chosen = answer;
        *dontAsk = remember;
        return true;
    });
    QToolButton *record = view.findChild<QToolButton *>("stRecord");

    // Nothing running: one choice, no question.
    QVERIFY(view.selectNode(c2));
    record->click();
    QVERIFY(asked.isEmpty());
    QCOMPARE(st->cueList(list)->cues.count(), 3);
    QCOMPARE(st->cueList(list)->cues.at(2).name, QString("Two (recorded)"));
    QVERIFY(st->undo());

    // The list runs cue One: asked, the selected and the running cue.
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    goList(m_doc, chaser);
    settleDmx(m_doc, 3);
    QVERIFY(view.selectNode(c2));
    record->click();
    QCOMPARE(asked.count(), 1);
    QCOMPARE(asked.first(), QList<ShowTreeView::RecordSource>({ ShowTreeView::RecordSource::Selected,
                                                                ShowTreeView::RecordSource::Running }));
    QCOMPARE(st->cueList(list)->cues.at(2).name, QString("One (recorded)"));
    QCOMPARE(view.currentId(), st->cueList(list)->cues.at(2).id);
    QVERIFY(st->undo());

    // "Don't ask again": the Design default is written, and used.
    remember = true;
    QVERIFY(view.selectNode(c2));
    record->click();
    QCOMPARE(asked.count(), 2);
    QCOMPARE(ShowTreeView::recordSetting(false), QString("running"));
    QCOMPARE(ShowTreeView::recordSetting(true), QString("ask"));        // Perform's is its own
    QVERIFY(st->undo());
    QVERIFY(view.selectNode(c2));
    record->click();
    QCOMPARE(asked.count(), 2);                                         // not asked
    QCOMPARE(st->cueList(list)->cues.at(2).name, QString("One (recorded)"));
    QVERIFY(st->undo());

    // The command line uses the same setting; a word overrides it.
    QVERIFY(view.selectNode(c2));
    QVERIFY(view.runCommand("record"));
    QCOMPARE(asked.count(), 2);
    QCOMPARE(st->cue(view.currentCueId())->name, QString("One (recorded)"));
    QVERIFY(view.runCommand("undo"));
    QVERIFY(view.selectNode(c2));
    QVERIFY(view.runCommand("record selected"));
    QCOMPARE(st->cue(view.currentCueId())->name, QString("Two (recorded)"));
    QVERIFY(view.runCommand("undo"));

    // Operate: its own setting -- still Ask.
    chaser->stop(FunctionParent::master());
    settleDmx(m_doc, 3);
    m_doc->setMode(Doc::Operate);
    goList(m_doc, chaser);
    settleDmx(m_doc, 3);
    QVERIFY(view.selectNode(c2));
    record->click();
    QCOMPARE(asked.count(), 3);
    QCOMPARE(ShowTreeView::recordSetting(true), QString("running"));
    QVERIFY(st->undo());

    // The dialog itself: the choices as radio buttons, Perform's preselected
    // the running cue; Cancel records nothing.
    settings.remove(ShowTreeView::recordSettingKey(true));
    view.setRecordAsker(ShowTreeView::RecordAsker());
    QStringList seen;
    bool preRunning = false;
    QTimer::singleShot(0, [&]() {
        QWidget *dlg = QApplication::activeModalWidget();
        if (dlg == nullptr || dlg->objectName() != "stRecordDialog")
            return;
        for (QRadioButton *rb : dlg->findChildren<QRadioButton *>())
            seen << rb->objectName();
        if (QRadioButton *rb = dlg->findChild<QRadioButton *>("stRecord:running"))
            preRunning = rb->isChecked();
        QVERIFY(dlg->findChild<QCheckBox *>("stRecordDontAsk") != nullptr);
        static_cast<QDialog *>(dlg)->reject();
    });
    QVERIFY(view.selectNode(c2));
    record->click();
    QCOMPARE(seen, QStringList({ "stRecord:selected", "stRecord:running" }));
    QVERIFY(preRunning);
    QCOMPARE(st->cueList(list)->cues.count(), 2);

    chaser->stop(FunctionParent::master());
    m_doc->setMode(Doc::Design);
    for (bool perform : { false, true })
    {
        const QVariant was = perform ? wasPerform : wasDesign;
        if (was.isValid())
            settings.setValue(ShowTreeView::recordSettingKey(perform), was);
        else
            settings.remove(ShowTreeView::recordSettingKey(perform));
    }
}

/* Several groups: ONE layer by default, a layer each as the explicit
   alternative -- dropped, from + Layer, or typed. */
void ShowTreeView_Test::severalGroupsOneLayerOrEach()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");

    ShowTreeView view(nullptr, m_doc);
    // Both groups selected in the Library (a Library rebuild clears it).
    auto selectGroups = [&view]() {
        view.library()->clearSelection();
        for (QTreeWidgetItemIterator it(view.library()); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LibGroup)
                (*it)->setSelected(true);
    };
    QVERIFY(view.selectNode(c1));
    selectGroups();
    QToolButton *add = view.findChild<QToolButton *>("stAddLayer");
    QVERIFY(add->menu() != nullptr);
    QCOMPARE(add->menu()->actions().count(), 2);
    QVERIFY(add->toolTip().contains("ONE layer"));

    QVERIFY(view.addLayersFromLibrary(false));
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    QCOMPARE(st->cue(c1)->layers.first().target.groups.count(), 2);
    QVERIFY(view.selectNode(c1));
    selectGroups();
    QVERIFY(view.addLayersFromLibrary(true));
    QCOMPARE(st->cue(c1)->layers.count(), 3);

    // The drop hint says which.
    QMimeData mime;
    QByteArray data;
    {
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds << towers->id() << usts->id();
    }
    mime.setData(FixtureGroupSource::fixtureGroupMimeType(), data);
    QVERIFY(view.dropHint(c1, invalidId(), &mime).contains("⌥: a layer for each group"));
    QVERIFY(view.dropHint(c1, invalidId(), &mime, true).contains("one for each group"));

    // Split from the tree: one per group; then merge the two selected.
    const quint32 both = st->cue(c1)->layers.first().id;
    QString why;
    QVERIFY2(view.splitLayer(c1, both, &why), qPrintable(why));
    QCOMPARE(st->cue(c1)->layers.count(), 4);
    view.tree()->clearSelection();
    QList<quint32> want;
    for (const Layer &l : st->cue(c1)->layers)
        if (l.target.groups == QList<quint32>({ towers->id() }))
        {
            want << l.id;
            break;
        }
    for (const Layer &l : st->cue(c1)->layers)
        if (l.target.groups == QList<quint32>({ usts->id() }))
        {
            want << l.id;
            break;
        }
    for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LayerNode
            && want.contains((*it)->data(0, ShowTreeView::IdRole).toUInt()))
            (*it)->setSelected(true);
    quint32 cue = invalidId();
    const QList<quint32> sel = view.selectedLayers(&cue);
    QCOMPARE(cue, c1);
    QCOMPARE(QSet<quint32>(sel.begin(), sel.end()), QSet<quint32>(want.begin(), want.end()));
    const quint32 merged = view.mergeLayers(cue, sel, &why);
    QVERIFY2(merged != invalidId(), qPrintable(why));
    QCOMPARE(st->cue(c1)->layers.count(), 3);
    QCOMPARE(view.currentId(), merged);
}

/* A value picked reuses the palette playing exactly it; else a one-off with
   "Make reusable" right on its row -- one click names and files it. */
void ShowTreeView_Test::aPickedValueReusesAPalette()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 i30 = dimmer(77, "Intensity 30%");
    m_doc->palette(i30)->setPath("Palettes/Dimmer");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 l1 = st->addLayer(c1, groupTarget(towers->id()));

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.setOneOffDimmer(c1, l1, 30));
    QCOMPARE(st->cue(c1)->layer(l1)->look(KeyDimmer)->palettes, QList<quint32>({ i30 }));
    QCOMPARE(view.recentPalettes().first(), i30);

    // 31 %: none plays it -- a one-off, with Make reusable by it.
    QVERIFY(view.setOneOffDimmer(c1, l1, 31));
    const quint32 pid = st->cue(c1)->layer(l1)->look(KeyDimmer)->palettes.first();
    QVERIFY(st->isInlinePalette(pid));
    QVERIFY(view.selectNode(l1, c1));
    QLabel *reuse = view.findChild<QLabel *>("stReuse:Dimmer");
    QVERIFY(reuse != nullptr);
    QVERIFY(reuse->toolTip().contains("Intensity 31%"));
    emit reuse->linkActivated("reuse");
    QVERIFY(!st->isInlinePalette(pid));
    QCOMPARE(m_doc->palette(pid)->name(), QString("Intensity 31%"));
    QCOMPARE(m_doc->palette(pid)->path(), QString("Palettes/Dimmer"));
    QVERIFY(view.selectNode(l1, c1));
    QVERIFY(view.findChild<QLabel *>("stReuse:Dimmer") == nullptr
            || !view.findChild<QLabel *>("stReuse:Dimmer")->isVisibleTo(&view));
}

/* Closes every modal box that opens while it lives, noting its object name
   and text: the tests never wait on a dialog. @a answer may fill one in and
   accept it instead (return true). */
class ModalCloser final : public QObject
{
public:
    explicit ModalCloser(std::function<bool(QWidget *)> answer = nullptr)
        : m_answer(answer)
    {
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            QWidget *w = QApplication::activeModalWidget();
            if (w == nullptr)
                w = QApplication::activePopupWidget();
            if (w == nullptr)
                return;
            if (QMessageBox *mb = qobject_cast<QMessageBox *>(w))
                seen << QStringLiteral("%1|%2|%3").arg(mb->objectName(), mb->text(), mb->informativeText());
            else
                seen << w->objectName();
            if (m_answer && m_answer(w))
                return;
            if (QDialog *d = qobject_cast<QDialog *>(w))
                d->reject();
            else
                w->close();
        });
        m_timer.start(20);
    }
    QStringList seen;

private:
    QTimer m_timer;
    std::function<bool(QWidget *)> m_answer;
};

static QTreeWidgetItem *rowOf(QTreeWidget *tree, int kind, quint32 id, quint32 cueId = invalidId())
{
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == kind
            && (*it)->data(0, ShowTreeView::IdRole).toUInt() == id
            && (cueId == invalidId() || (*it)->data(0, ShowTreeView::CueRole).toUInt() == cueId))
            return *it;
    return nullptr;
}

/* A click on a tree row as the mouse does it, with @a mods (⌘ is Control
   to Qt). */
static void clickRow(QTreeWidget *tree, QTreeWidgetItem *item, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QVERIFY(item != nullptr);
    for (QTreeWidgetItem *p = item->parent(); p != nullptr; p = p->parent())
        p->setExpanded(true);
    tree->scrollToItem(item);
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, mods, tree->visualItemRect(item).center());
}

/* Merge layers by mouse: ⌘-click and ⇧-click select several layers of one
   cue; the header's Merge and the context menu's take them. The page, the
   preview and the rest still follow the row clicked last. */
void ShowTreeView_Test::mergeLayersBySelectingThem()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    const quint32 blue = colour(Qt::blue, "Blue");
    const quint32 red = colour(Qt::red, "Red");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    Target both;
    both.groups = { towers->id(), usts->id() };
    const quint32 l1 = st->addLayer(c1, both);
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(blue)));
    QList<quint32> made;
    QVERIFY(st->splitLayer(c1, l1, &made));
    QCOMPARE(made.count(), 2);
    const quint32 c2Layer = st->addLayer(c2, groupTarget(towers->id()));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    QToolButton *merge = view.findChild<QToolButton *>("stMerge");
    QVERIFY(merge != nullptr);
    QVERIFY(tree->selectionMode() == QAbstractItemView::ExtendedSelection);

    // One click: one row; Merge has nothing to take.
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, made[0], c1));
    QCOMPARE(tree->selectedItems().count(), 1);
    QCOMPARE(view.currentKind(), ShowTreeView::LayerNode);
    QVERIFY(!merge->isEnabled());

    // ⌘-click the other: both selected, the page shows the one clicked.
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, made[1], c1), Qt::ControlModifier);
    QCOMPARE(tree->selectedItems().count(), 2);
    QCOMPARE(view.currentId(), made[1]);
    QCOMPARE(view.currentCueId(), c1);
    QCOMPARE(view.findChild<QStackedWidget *>("stInspector")->currentWidget()->objectName(),
             QString("stLayerPage"));
    QVERIFY(merge->isEnabled());
    QCOMPARE(st->previewedCue(), c1);

    // The context menu offers it too, enabled.
    {
        bool sawMerge = false, enabled = false;
        ModalCloser closer([&](QWidget *w) {
            if (QMenu *m = qobject_cast<QMenu *>(w))
                for (QAction *a : m->actions())
                    if (a->objectName() == QLatin1String("stMergeLayers"))
                    {
                        sawMerge = true;
                        enabled = a->isEnabled();
                    }
            return false;
        });
        QTreeWidgetItem *row = rowOf(tree, ShowTreeView::LayerNode, made[1], c1);
        QMetaObject::invokeMethod(&view, "slotTreeContextMenu",
                                  Q_ARG(QPoint, tree->visualItemRect(row).center()));
        QVERIFY(sawMerge);
        QVERIFY(enabled);
        QCOMPARE(tree->selectedItems().count(), 2);   // the right-click kept the selection
    }

    // A layer of another cue in it: not mergeable.
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, c2Layer, c2), Qt::ControlModifier);
    QCOMPARE(tree->selectedItems().count(), 3);
    QVERIFY(!merge->isEnabled());
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, c2Layer, c2), Qt::ControlModifier);
    QVERIFY(merge->isEnabled());

    // Merge: one layer again, selected; one undo step.
    const QString undoBefore = st->undoText();
    merge->click();
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    QCOMPARE(st->cue(c1)->layers.first().target.groups.count(), 2);
    QCOMPARE(view.currentId(), st->cue(c1)->layers.first().id);
    QCOMPARE(tree->selectedItems().count(), 1);
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c1)->layers.count(), 2);
    QCOMPARE(st->undoText(), undoBefore);

    // ⇧-click selects the range; a merge that would change a look is
    // refused with the reason.
    QVERIFY(st->setLayerLook(c1, made[1], st->lookForPalette(red)));
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, made[0], c1));
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, made[1], c1), Qt::ShiftModifier);
    QCOMPARE(tree->selectedItems().count(), 2);
    QVERIFY(merge->isEnabled());
    {
        ModalCloser closer;
        merge->click();
        QCOMPARE(closer.seen.count(), 1);
        QVERIFY2(closer.seen.first().contains("Colour"), qPrintable(closer.seen.first()));
    }
    QCOMPARE(st->cue(c1)->layers.count(), 2);
}

/* Several rows selected: Delete takes the cues and layers in one undo step
   (a list or production only on its own); several cues dragged together
   are refused, said in the hint. */
void ShowTreeView_Test::severalRowsSelectedDeleteAndDrag()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 c3 = st->addCue(list, 1, "Three");
    const quint32 lt = st->addLayer(c1, groupTarget(towers->id()));
    const quint32 lu = st->addLayer(c1, groupTarget(usts->id()));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();

    // Two layers and a cue: one step.
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lt, c1));
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lu, c1), Qt::ControlModifier);
    clickRow(tree, rowOf(tree, ShowTreeView::CueNode, c3), Qt::ControlModifier);
    QCOMPARE(tree->selectedItems().count(), 3);
    view.findChild<QToolButton *>("stDelete")->click();
    QVERIFY(st->cue(c1)->layers.isEmpty());
    QVERIFY(st->cue(c3) == nullptr);
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c1)->layers.count(), 2);
    QVERIFY(st->cue(c3) != nullptr);

    // A list among them: refused, said, nothing gone.
    clickRow(tree, rowOf(tree, ShowTreeView::CueNode, c2));
    clickRow(tree, rowOf(tree, ShowTreeView::ListNode, list), Qt::ControlModifier);
    {
        ModalCloser closer;
        view.findChild<QToolButton *>("stDelete")->click();
        QCOMPARE(closer.seen.count(), 1);
        QVERIFY(closer.seen.first().contains("on its own"));
    }
    QVERIFY(st->cueList(list) != nullptr && st->cue(c2) != nullptr);

    // Two cues dragged with Alt (include) onto a third: refused, and the
    // hint says why.
    QMimeData *mime = new QMimeData();
    QByteArray data;
    {
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds << c1 << c2;
    }
    mime->setData(ShowTreeView::cueMimeType(), data);
    QTreeWidgetItem *onto = rowOf(tree, ShowTreeView::CueNode, c3);
    tree->scrollToItem(onto);
    const QRect rect = tree->visualItemRect(onto);
    const QPoint at(rect.center().x(), rect.top() + rect.height() * 3 / 4);     // its lower half: after it
    {
        QDragEnterEvent enter(dragPos(at), Qt::MoveAction, mime, Qt::LeftButton, Qt::AltModifier);
        QCoreApplication::sendEvent(tree->viewport(), &enter);
        QDragMoveEvent move(dragPos(at), Qt::MoveAction, mime, Qt::LeftButton, Qt::AltModifier);
        QCoreApplication::sendEvent(tree->viewport(), &move);
        QVERIFY(!move.isAccepted());
        QVERIFY(view.findChild<QLabel *>("stDropHint")->text().contains("one cue at a time"));
        QDropEvent drop(at, Qt::MoveAction, mime, Qt::LeftButton, Qt::AltModifier);
        QCoreApplication::sendEvent(tree->viewport(), &drop);
        QCOMPARE(st->cueList(list)->indexOf(c1), 0);
        QCOMPARE(st->cueList(list)->indexOf(c2), 1);
        QCOMPARE(st->cueList(list)->indexOf(c3), 2);
        QVERIFY(st->cue(c3)->includes.isEmpty());
    }

    // Without Alt they move as a block after it, renumbered; the hint says
    // where and as what first.
    const QString c3Number = st->cue(c3)->number;
    QDragEnterEvent enter(dragPos(at), Qt::MoveAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &enter);
    QDragMoveEvent move(dragPos(at), Qt::MoveAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &move);
    QVERIFY(move.isAccepted());
    QVERIFY(view.findChild<QLabel *>("stDropHint")->text().startsWith("Move 2 cues after"));
    QDropEvent drop(at, Qt::MoveAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &drop);
    QCOMPARE(st->cueList(list)->indexOf(c3), 0);
    QCOMPARE(st->cueList(list)->indexOf(c1), 1);
    QCOMPARE(st->cueList(list)->indexOf(c2), 2);
    QCOMPARE(st->cue(c3)->number, c3Number);
    QVERIFY(compareCueNumbers(st->cue(c1)->number, c3Number) > 0);
    QVERIFY(compareCueNumbers(st->cue(c2)->number, st->cue(c1)->number) > 0);
    QVERIFY(st->undo());
    // Onto a cue (dropCue): before it moving up, as the drag's upper half.
    QCOMPARE(view.cueDropHint(c3, c1, false), view.cuesDropHint({ c3 }, c1, false));
    delete mime;
}

/* One rule: a new list is Tracking, by mouse (+ Cue list) and typed; an
   imported one Non-tracking (D12). */
void ShowTreeView_Test::aNewCueListIsTracking()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.selectNode(prod));
    {
        ModalCloser closer([](QWidget *w) {
            QInputDialog *d = qobject_cast<QInputDialog *>(w);
            if (d == nullptr)
                return false;
            d->setTextValue("01-Opener");
            d->accept();
            return true;
        });
        view.findChild<QToolButton *>("stNewList")->click();
    }
    QCOMPARE(view.currentKind(), ShowTreeView::ListNode);
    const CueList *mouse = st->cueList(view.currentId());
    QVERIFY(mouse != nullptr);
    QCOMPARE(mouse->name, QString("01-Opener"));
    QVERIFY(mouse->mode == ListMode::Tracking);
    QCOMPARE(view.findChild<QComboBox *>("stListMode")->currentIndex(), 1);

    QVERIFY(view.runCommand("list \"02-Typed\""));
    QCOMPARE(view.currentKind(), ShowTreeView::ListNode);
    QVERIFY(st->cueList(view.currentId())->mode == ListMode::Tracking);

    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Old");
    m_doc->addFunction(chaser);
    view.importChaser(chaser->id(), false);
    QVERIFY(st->cueList(view.currentId()) != nullptr);
    QVERIFY(st->cueList(view.currentId())->mode == ListMode::NonTracking);
}

/* D13: a generated function is edited in the Show Tree. Its chaser opens
   locked (steps and name; its playback settings and testing stay), with
   Show in Show Tree; deleting, renaming or adding to it elsewhere is
   refused with the same words; a layer's scene can be detached. */
void ShowTreeView_Test::generatedFunctionsAreLockedElsewhere()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 l1 = st->addLayer(c1, groupTarget(towers->id()));
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(full)));
    const quint32 chaserId = st->cueList(list)->chaser;
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(chaserId));
    QVERIFY(chaser != nullptr && chaser->isVisible());
    const quint32 collId = st->cue(c1)->collection;
    const Collection *coll = qobject_cast<const Collection *>(m_doc->function(collId));
    QVERIFY(coll != nullptr && coll->functions().count() == 1);
    const quint32 sceneId = coll->functions().first();
    Scene *plain = new Scene(m_doc);
    plain->setName("Plain");
    m_doc->addFunction(plain);
    const QString locked = ShowTreeController::lockedText();

    ShowTreeView view(nullptr, m_doc);
    QCOMPARE(ShowTreeView::instance(), &view);

    // Function Manager: the chaser's row isn't renamed in place; its editor
    // opens locked; Delete is refused.
    {
        FunctionManager fm(nullptr, m_doc);
        FunctionsTreeWidget *ftree = fm.findChild<FunctionsTreeWidget *>();
        QVERIFY(ftree != nullptr);
        QTreeWidgetItem *row = ftree->functionItem(chaser);
        QVERIFY(row != nullptr);
        QVERIFY(!(row->flags() & Qt::ItemIsEditable));
        QVERIFY(row->toolTip(0).contains(locked));
        QVERIFY(QMetaObject::invokeMethod(&fm, "slotTreeItemDoubleClicked", Q_ARG(QTreeWidgetItem *, row)));
        ChaserEditor *ed = fm.findChild<ChaserEditor *>();
        QVERIFY(ed != nullptr);
        QVERIFY(ed->isLocked());
        QVERIFY(!ed->m_add->isEnabled());
        QVERIFY(!ed->m_remove->isEnabled());
        QVERIFY(!ed->m_raise->isEnabled());
        QVERIFY(ed->m_nameEdit->isReadOnly());
        QVERIFY(ed->m_tree->editTriggers() == QAbstractItemView::NoEditTriggers);
        QVERIFY(ed->m_singleShot->isEnabled() && ed->m_loop->isEnabled());   // run order: the user's
        QVERIFY(ed->m_testPlayButton->isEnabled());                           // it still plays
        QLabel *text = ed->findChild<QLabel *>("stLockedText");
        QVERIFY(text != nullptr && text->text().contains(locked.toHtmlEscaped()));
        QPushButton *reveal = ed->findChild<QPushButton *>("stLockedReveal");
        QVERIFY(reveal != nullptr && reveal->isEnabled());
        reveal->click();
        QCOMPARE(view.currentKind(), ShowTreeView::ListNode);
        QCOMPARE(view.currentId(), list);

        ftree->clearSelection();
        row = ftree->functionItem(chaser);
        row->setSelected(true);
        ModalCloser closer;
        QVERIFY(QMetaObject::invokeMethod(&fm, "slotDelete"));
        QCOMPARE(closer.seen.count(), 1);
        QVERIFY(closer.seen.first().startsWith("stLockedMessage|" + locked));
        QVERIFY(m_doc->function(chaserId) != nullptr);
    }

    // The Programming tab: the same editor, locked; delete and add-to
    // refused.
    {
        ProgrammingManager pm(nullptr, m_doc);
        pm.showFunction(chaserId);
        ChaserEditor *ed = pm.findChild<ChaserEditor *>();
        QVERIFY(ed != nullptr && ed->isLocked());
        ModalCloser closer;
        pm.deleteFunctions({ chaserId });
        QVERIFY(m_doc->function(chaserId) != nullptr);
        // (Adding to a collection was the old function tree's; the outline
        // holds scenes only, docs/design/PROGRAMMING_REDESIGN.md P1.)
        QCOMPARE(closer.seen.count(), 1);
        // A plain function is deleted as ever.
        pm.deleteFunctions({ plain->id() });
    }

    // A VC widget's live edit: locked too.
    {
        FunctionLiveEditDialog dlg(m_doc, chaserId, nullptr);
        ChaserEditor *ed = dlg.findChild<ChaserEditor *>();
        QVERIFY(ed != nullptr && ed->isLocked());
    }

    // A layer's scene: its bar offers Detach, which makes a plain scene the
    // layer then plays as it is -- one undo step.
    quint32 detached = Function::invalidId();
    QWidget *bar = ShowTreeView::lockedBanner(nullptr, m_doc, sceneId, [&](quint32 sid) { detached = sid; });
    QPushButton *detach = bar->findChild<QPushButton *>("stLockedDetach");
    QVERIFY(detach != nullptr);
    detach->click();
    QVERIFY(detached != Function::invalidId());
    QVERIFY(!st->isGenerated(detached));
    QCOMPARE(st->cue(c1)->layer(l1)->target.function, detached);
    delete bar;
    QVERIFY(st->undo());
    QVERIFY(m_doc->function(detached) == nullptr);
    QCOMPARE(st->cue(c1)->layer(l1)->target.groups, QList<quint32>({ towers->id() }));
    // A chaser has no Detach.
    bar = ShowTreeView::lockedBanner(nullptr, m_doc, chaserId, [](quint32) {});
    QVERIFY(bar->findChild<QPushButton *>("stLockedDetach") == nullptr);
    delete bar;
}

/* The tree's state as an undo must give it back: the productions (a
   remade cue's Collection id aside), every palette and whether it is a
   one-off, and every function the tree didn't make. */
static QString treeState(Doc *doc)
{
    ShowTreeController *st = doc->showTree();
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.writeStartDocument();
    w.writeStartElement("S");
    for (const GroupSet &gs : st->groupSets())
        ShowTree::saveXML(&w, gs);
    for (const ReusableLayer &r : st->reusableLayers())
        ShowTree::saveXML(&w, r);
    for (const ReusableCue &r : st->reusableCues())
        ShowTree::saveXML(&w, r);
    for (const Production &p : st->productions())
        ShowTree::saveXML(&w, p);
    w.writeEndElement();
    w.writeEndDocument();
    QString out = QString::fromUtf8(xml);
    out.remove(QRegularExpression(QStringLiteral(" Collection=\"\\d+\"")));
    QList<quint32> pids;
    for (const QLCPalette *p : doc->palettes())
        pids << p->id();
    std::sort(pids.begin(), pids.end());
    for (quint32 id : pids)
    {
        const QLCPalette *p = doc->palette(id);
        out += QStringLiteral("\nP %1 %2 %3 %4 %5").arg(id).arg(p->name(), p->path())
               .arg(st->isInlinePalette(id)).arg(p->values().count() > 0 ? p->values().first().toString() : QString());
    }
    QList<quint32> fids;
    for (const Function *f : doc->functions())
        if (!st->isGenerated(f->id()))
            fids << f->id();
    std::sort(fids.begin(), fids.end());
    for (quint32 id : fids)
        out += QStringLiteral("\nF %1 %2 %3").arg(id).arg(doc->function(id)->name(), doc->function(id)->path());
    return out;
}

/* D22: every action in the tab is one undo step -- one undo gives back the
   state before it exactly, and redo the state after. Walked by the mouse
   (drops, pickers, clicks) and by typed commands. */
void ShowTreeView_Test::everyActionIsOneUndoStep()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    FixtureGroup *pars = group("Pars", 4, 2);
    const quint32 blue = colour(Qt::blue, "Blue");
    const quint32 i45 = dimmer(115, "Intensity 45%");
    m_doc->palette(i45)->setPath("Palettes/Dimmer");
    Scene *haze = new Scene(m_doc);
    haze->setName("Haze");
    haze->setValue(m_fixtures[5], 0, 100);
    m_doc->addFunction(haze);
    Scene *smoke = new Scene(m_doc);
    smoke->setName("Smoke");
    smoke->setValue(m_fixtures[4], 0, 50);
    m_doc->addFunction(smoke);
    // A chaser of a scene of groups to import.
    Scene *look = new Scene(m_doc);
    look->setName("Look");
    look->addFixtureGroup(pars->id());
    look->addPalette(i45);
    m_doc->addFunction(look);
    Collection *step = new Collection(m_doc);
    step->setName("0.0.1-Step");
    step->addFunction(look->id());
    m_doc->addFunction(step);
    Chaser *old = new Chaser(m_doc);
    old->setName("Old");
    m_doc->addFunction(old);
    old->addStep(ChaserStep(step->id()));

    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "L");     // Tracking
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.setRecordAsker([](const QList<ShowTreeController::RecordSource> &, ShowTreeController::RecordSource *chosen,
                           bool *) {
        *chosen = ShowTreeController::RecordSource::Selected;
        return true;
    });

    // Nothing here asks; were it to, the test says so rather than waits.
    ModalCloser closer;
    int walked = 0;
    auto oneStep = [&](const char *what, const std::function<bool()> &action) -> bool {
        const QString before = treeState(m_doc);
        if (!action())
        {
            qWarning() << what << "did nothing";
            return false;
        }
        const QString after = treeState(m_doc);
        if (after == before)
        {
            qWarning() << what << "changed nothing";
            return false;
        }
        if (!st->undo() || treeState(m_doc) != before)
        {
            qWarning() << what << ": one undo didn't give back the state before it";
            return false;
        }
        if (!st->redo() || treeState(m_doc) != after)
        {
            qWarning() << what << ": redo didn't give back the state after it";
            return false;
        }
        ++walked;
        return true;
    };
    auto layerOn = [&](quint32 cue, quint32 g) -> quint32 {
        for (const Layer &l : st->cue(cue)->layers)
            if (l.target.groups.contains(g))
                return l.id;
        return invalidId();
    };

    // Drops
    QVERIFY(oneStep("a drop of two groups", [&] { return view.dropTargets(c1, { towers->id(), usts->id() }, {}); }));
    const quint32 both = layerOn(c1, towers->id());
    QVERIFY(oneStep("a colour and a dimmer dropped together", [&] {
        return view.dropPalettes(c1, both, { blue, i45 });
    }));
    QVERIFY(oneStep("Split", [&] { return view.splitLayer(c1, both); }));
    QVERIFY(oneStep("Merge, by selecting", [&] {
        view.tree()->clearSelection();
        clickRow(view.tree(), rowOf(view.tree(), ShowTreeView::LayerNode, layerOn(c1, towers->id()), c1));
        clickRow(view.tree(), rowOf(view.tree(), ShowTreeView::LayerNode, layerOn(c1, usts->id()), c1),
                 Qt::ControlModifier);
        view.findChild<QToolButton *>("stMerge")->click();
        return st->cue(c1)->layers.count() == 1;
    }));
    const quint32 merged = layerOn(c1, towers->id());
    QVERIFY(oneStep("an ⌥-drop: a layer each", [&] {
        return view.dropTargets(c2, { pars->id() }, { m_fixtures[0] }, true);
    }));
    QVERIFY(oneStep("two functions dropped together", [&] {
        return view.dropFunctions(c1, { haze->id(), smoke->id() });
    }));
    QVERIFY(oneStep("groups dropped on a layer", [&] {
        return view.dropTargetsOnLayer(c1, merged, { pars->id() }, {});
    }));
    QVERIFY(oneStep("a fixture dropped on a carried-in layer", [&] {
        const Target carried = st->cue(c1)->layer(merged)->target;
        return view.dropTargetsOnTracked(c2, carried, {}, { m_fixtures[5] });
    }));
    QVERIFY(oneStep("a palette dropped on a cue (a cue look)", [&] {
        return view.dropPalettes(c2, invalidId(), { blue });
    }));
    // The ⋯ pickers
    QVERIFY(oneStep("Level… to a one-off", [&] { return view.setOneOffDimmer(c1, merged, 31); }));
    QVERIFY(oneStep("Make reusable, the row's link", [&] {
        view.selectNode(merged, c1);
        QLabel *reuse = view.findChild<QLabel *>("stReuse:Dimmer");
        if (reuse == nullptr)
            return false;
        emit reuse->linkActivated("reuse");
        return true;
    }));
    QVERIFY(oneStep("Colour… to a one-off", [&] { return view.setOneOffColour(c1, merged, QColor(1, 2, 3)); }));
    QVERIFY(oneStep("Make reusable…, named by hand", [&] {
        const quint32 pid = st->cue(c1)->layer(merged)->look(KeyColour)->palettes.first();
        return st->makeReusable(pid, "Mine", "Palettes/Mine");
    }));
    QVERIFY(oneStep("Level… reusing a palette", [&] { return view.setOneOffDimmer(c1, merged, 45); }));
    QVERIFY(oneStep("None (off here)", [&] { return view.setAttributeNone(c1, merged, KeyColour); }));
    QVERIFY(oneStep("Clear (inherit)", [&] { return view.clearAttribute(c1, merged, KeyColour); }));
    // The pages and the context menu
    QVERIFY(oneStep("the cue's name", [&] {
        view.selectNode(c2);
        QLineEdit *name = view.findChild<QLineEdit *>("stCueName");
        name->setText("Second");
        emit name->editingFinished();
        return st->cue(c2)->name == QLatin1String("Second");
    }));
    QVERIFY(oneStep("the cue's mode", [&] { return st->setCueMode(c2, CueMode::Block); }));
    QVERIFY(oneStep("the list's mode", [&] {
        view.selectNode(list);
        QComboBox *mode = view.findChild<QComboBox *>("stListMode");
        mode->setCurrentIndex(0);
        emit mode->activated(0);    // as a pick from the list does
        return st->cueList(list)->mode == ListMode::NonTracking;
    }));
    QVERIFY(oneStep("the base cue", [&] { return st->setBaseCue(list, c1); }));
    QVERIFY(oneStep("Release", [&] {
        view.selectNode(merged, c1);
        view.findChild<QCheckBox *>("stLayerRelease")->click();
        return st->cue(c1)->layer(merged)->release;
    }));
    QVERIFY(oneStep("an include", [&] { return st->addInclude(c2, c1); }));
    QVERIFY(oneStep("a cue dropped on another (moved)", [&] { return view.dropCue(c2, c1, false); }));
    QVERIFY(oneStep("+ Cue", [&] {
        view.selectNode(c1);
        view.findChild<QToolButton *>("stNewCue")->click();
        return st->cueList(list)->cues.count() == 3;
    }));
    QVERIFY(oneStep("Record", [&] { return view.recordFrom(c1) != invalidId(); }));
    QVERIFY(oneStep("Duplicate cue", [&] { return st->duplicateCue(c1) != invalidId(); }));
    QVERIFY(oneStep("Rename a layer", [&] { return st->renameNode(merged, "Front"); }));
    QVERIFY(oneStep("Detach to plain scene", [&] {
        return st->detachLayer(c2, layerOn(c2, pars->id())) != Function::invalidId();
    }));
    QVERIFY(oneStep("Delete a layer", [&] {
        if (!view.selectNode(merged, c1))
            return false;
        view.findChild<QToolButton *>("stDelete")->click();
        return st->cue(c1)->layer(merged) == nullptr;
    }));
    QVERIFY(oneStep("Delete several rows", [&] {
        view.tree()->clearSelection();
        clickRow(view.tree(), rowOf(view.tree(), ShowTreeView::CueNode, c1));
        clickRow(view.tree(), rowOf(view.tree(), ShowTreeView::CueNode, c2), Qt::ControlModifier);
        view.findChild<QToolButton *>("stDelete")->click();
        return st->cue(c1) == nullptr && st->cue(c2) == nullptr;
    }));
    QVERIFY(oneStep("+ Production", [&] { return st->addProduction("Q") != invalidId(); }));
    QVERIFY(oneStep("Import chaser", [&] {
        view.importChaser(old->id(), false);
        return st->productions().count() == 3;
    }));
    // Typed: each command one step.
    QVERIFY(view.selectNode(list));
    for (const char *line : { "cue + \"Typed\"", "Towers colour Blue dimmer 45", "Towers USTs dimmer 45",
                              "Pars, USTs colour Blue", "cue look colour Blue", "block", "base",
                              "release Towers", "function Haze, Smoke", "Towers dimmer none",
                              "Towers dimmer clear", "dup", "record selected", "cue + \"Again\"" })
        QVERIFY2(oneStep(line, [&] {
                     // Undo and redo leave the selection where they find it:
                     // back on the list's last cue, as a user would be.
                     view.selectNode(st->cueList(list)->cues.last().id);
                     return view.runCommand(QString::fromUtf8(line));
                 }),
                 qPrintable(QStringLiteral("%1: %2").arg(QString::fromUtf8(line), view.commandMessage())));
    const QString include = QStringLiteral("include %1").arg(st->cueList(list)->cues.first().number);
    QVERIFY2(oneStep("include", [&] {
                 view.selectNode(st->cueList(list)->cues.last().id);
                 return view.runCommand(include);
             }), qPrintable(include + ": " + view.commandMessage()));
    // P3: group sets, child layers, rename in place, overlap.
    quint32 wings = invalidId();
    QVERIFY(oneStep("Make set", [&] {
        wings = st->addGroupSet("Wings", { towers->id(), usts->id() });
        return wings != invalidId();
    }));
    QVERIFY(oneStep("a set's members", [&] { return st->setGroupSetMembers(wings, { towers->id() }); }));
    QVERIFY(oneStep("rename a set", [&] { return st->renameGroupSet(wings, "Wing"); }));
    const quint32 last = st->cueList(list)->cues.last().id;
    QVERIFY(oneStep("a set dropped on a cue", [&] { return view.dropTargets(last, {}, {}, false, { wings }); }));
    QVERIFY(oneStep("a set ⌥-dropped: a layer per member", [&] {
        return view.dropTargets(last, {}, {}, true, { wings });
    }));
    quint32 big = invalidId();
    QVERIFY(oneStep("a layer for children", [&] {
        big = st->addLayer(last, Target { { pars->id() }, {}, {}, 0xFFFFFFFF });
        return big != invalidId();
    }));
    QVERIFY(oneStep("a child layer by ⇧-drop", [&] {
        return view.dropChildLayers(last, big, {}, { m_fixtures[4] });
    }));
    QVERIFY(oneStep("overlap: mix", [&] { return st->setCueMixOverlap(last, true); }));
    QVERIFY(oneStep("rename in place (Return)", [&] {
        view.selectNode(big, last);
        view.tree()->setFocus();
        QTest::keyClick(view.tree(), Qt::Key_Return);
        QLineEdit *editor = qobject_cast<QLineEdit *>(QApplication::focusWidget());
        if (editor == nullptr)
            return false;
        editor->clear();
        QTest::keyClicks(editor, "Floor wash");
        QTest::keyClick(editor, Qt::Key_Return);
        // The rename lands once the editor has let go of the row.
        return QTest::qWaitFor([&] { return st->cue(last)->layer(big)->name == QLatin1String("Floor wash"); }, 3000);
    }));
    for (const char *line : { "set \"Sides\" Towers Pars", "Sides dimmer 45", "Dim 2 in Towers dimmer 45",
                              "name \"Typed\"" })
        QVERIFY2(oneStep(line, [&] {
                     view.selectNode(st->cueList(list)->cues.last().id);
                     return view.runCommand(QString::fromUtf8(line));
                 }),
                 qPrintable(QStringLiteral("%1: %2").arg(QString::fromUtf8(line), view.commandMessage())));
    // P4: reusables, Detach a list, Convert to tracking.
    view.setConfirm([](const QString &, const QString &, const QString &) { return true; });
    const quint32 lastCue = st->cueList(list)->cues.last().id;
    quint32 reusable = invalidId();
    QVERIFY(oneStep("Make a layer reusable", [&] {
        reusable = st->makeLayerReusable(lastCue, big, "Floor");
        return reusable != invalidId();
    }));
    QVERIFY(oneStep("a reusable dropped on a cue (a use)", [&] {
        return view.dropReusables(st->cueList(list)->cues.first().id, { reusable });
    }));
    QVERIFY(oneStep("an edit to one use (all of them)", [&] {
        return view.dropPalettes(lastCue, big, { blue });
    }));
    QVERIFY(oneStep("rename a reusable", [&] { return st->renameReusable(reusable, "Floor 2"); }));
    QVERIFY(oneStep("Unlink here", [&] { return st->unlinkLayer(lastCue, big); }));
    QVERIFY(oneStep("delete a reusable in use (asks)", [&] { return view.deleteReusable(reusable); }));
    quint32 rcue = invalidId();
    QVERIFY(oneStep("Make a cue reusable", [&] {
        rcue = st->makeCueReusable(lastCue, "State");
        return rcue != invalidId();
    }));
    QVERIFY(oneStep("a reusable cue dropped on a cue (a cue after it)", [&] {
        return view.dropReusables(st->cueList(list)->cues.first().id, { rcue });
    }));
    // Typed, on the list's first cue (a plain one) and the use after it.
    for (const char *line : { "reusable \"Typed state\"", "use State", "unlink" })
        QVERIFY2(oneStep(line, [&] {
                     const bool second = QByteArray(line) == "unlink";
                     view.selectNode(st->cueList(list)->cues.at(second ? 1 : 0).id);
                     return view.runCommand(QString::fromUtf8(line));
                 }),
                 qPrintable(QStringLiteral("%1: %2").arg(QString::fromUtf8(line), view.commandMessage())));
    const quint32 imported = st->productions().at(2).lists.first().id;
    QVERIFY(oneStep("Convert to tracking", [&] { return view.convertListToTracking(imported); }));
    QVERIFY(oneStep("Detach to plain chaser", [&] { return view.detachListToChaser(imported); }));
    QVERIFY(walked >= 70);
}

/****************************************************************************
 * P3 (2026-10-17): group sets, child layers, renaming in place
 ****************************************************************************/

/* §1 "Make set": groups selected in the Library made into a named set,
   shown under All → Group sets, dragged like a group (one layer on the
   set; ⌥ a layer per member), and usable by name on the command line. */
void ShowTreeView_Test::groupSetsInTheLibrary()
{
    FixtureGroup *sl = group("SL", 0, 2);
    FixtureGroup *sr = group("SR", 2, 2);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *lib = view.library();
    QTreeWidgetItem *a = libraryItem(lib, "SL (2)");
    QTreeWidgetItem *b = libraryItem(lib, "SR (2)");
    QVERIFY(a != nullptr && b != nullptr);
    QCOMPARE(view.makeSetFromLibrary("Towers"), invalidId());      // nothing selected
    a->setSelected(true);
    b->setSelected(true);
    const quint32 towers = view.makeSetFromLibrary("Towers");
    QVERIFY(towers != invalidId());
    QCOMPARE(st->groupSet(towers)->groups, QList<quint32>({ sl->id(), sr->id() }));
    QTRY_VERIFY(libraryItem(lib, "Towers (SL + SR)") != nullptr);
    bool inAll = false;
    for (QTreeWidgetItemIterator it(lib); *it; ++it)
        if ((*it)->text(0) == "Towers (SL + SR)" && (*it)->parent()->text(0) == "Group sets")
            inAll = true;
    QVERIFY(inAll);
    QVERIFY(st->undo());
    QVERIFY(st->groupSets().isEmpty());
    QVERIFY(st->redo());

    // Dropped on the cue page: one layer on the set.
    QVERIFY(view.selectNode(c1));
    QLabel *hint = view.findChild<QLabel *>("stDropHint");
    QWidget *page = view.findChild<QWidget *>("stCuePage");
    QLabel *zone = view.findChild<QLabel *>("stCueDropZone");
    const QPoint onZone = zone->mapTo(page, zone->rect().center());
    QCOMPARE(dragAndDrop(page, onZone, idsMime(ShowTreeView::setMimeType(), { towers }), hint),
             QString("one layer on Towers in 1 One (⌥: a layer for each group)"));
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    QCOMPARE(st->cue(c1)->layers.first().target.sets, QList<quint32>({ towers }));
    QVERIFY(treeText(view.tree()).join("\n").contains("Towers"));
    // ⌥: a layer per member, one undo step.
    QVERIFY(view.selectNode(c2));
    QCOMPARE(dragAndDrop(page, onZone, idsMime(ShowTreeView::setMimeType(), { towers }), hint, Qt::AltModifier),
             QString("2 new layers in 2 Two, one for each group"));
    QCOMPARE(st->cue(c2)->layers.count(), 2);
    QCOMPARE(st->cue(c2)->layers.at(1).target.groups, QList<quint32>({ sr->id() }));
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c2)->layers.count(), 0);

    // The command line: a set by name, and made there.
    QVERIFY(view.selectNode(c2));
    QVERIFY2(view.runCommand("set \"Wings\" SL SR"), qPrintable(view.commandMessage()));
    QCOMPARE(st->groupSets().count(), 2);
    QVERIFY2(view.runCommand("Wings dimmer 50"), qPrintable(view.commandMessage()));
    QCOMPARE(st->cue(c2)->layers.first().target.sets.count(), 1);
    QVERIFY(st->undo());
    QVERIFY(st->undo());
    QCOMPARE(st->groupSets().count(), 1);
}

/* §3.5 by mouse: ⇧-drop a group that is part of a layer onto it -- a child
   layer, nested under it in the tree; its page shows what the parent
   lights grey ("from Steps (parent)"); a group that isn't part of it is
   refused, and the hint says why. */
void ShowTreeView_Test::childLayersByMouse()
{
    FixtureGroup *steps = group("Steps", 0, 3);
    FixtureGroup *us = group("US", 1, 1);
    FixtureGroup *floor = group("Floor", 4, 1);
    const quint32 full = dimmer(255, "Full");
    const quint32 red = colour(Qt::red, "Red");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << steps->id();
    const quint32 parent = st->addLayer(c1, t);
    st->setLayerLook(c1, parent, st->lookForPalette(full));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QLabel *hint = view.findChild<QLabel *>("stDropHint");
    QTreeWidget *tree = view.tree();
    QVERIFY(view.selectNode(parent, c1));
    QTreeWidgetItem *row = rowOf(tree, ShowTreeView::LayerNode, parent, c1);
    tree->scrollToItem(row);
    const QPoint at = tree->visualItemRect(row).center();

    // Not part of Steps: refused, the hint says so.
    bool taken = true;
    QCOMPARE(dragAndDrop(tree->viewport(), at, idsMime(FixtureGroupSource::fixtureGroupMimeType(), { floor->id() }),
                         hint, Qt::ShiftModifier, &taken),
             QString("Floor isn't part of Steps: a child layer is on part of its parent"));
    QCOMPARE(st->cue(c1)->layers.count(), 1);

    // US is: a child layer, under Steps's row.
    row = rowOf(tree, ShowTreeView::LayerNode, parent, c1);
    QCOMPARE(dragAndDrop(tree->viewport(), tree->visualItemRect(row).center(),
                         idsMime(FixtureGroupSource::fixtureGroupMimeType(), { us->id() }), hint, Qt::ShiftModifier),
             QString("a child layer on US under Steps in 1 One: it sets only what it sets"));
    QCOMPARE(st->cue(c1)->layers.count(), 2);
    const quint32 child = st->cue(c1)->layers.at(1).id;
    QCOMPARE(st->cue(c1)->layers.at(1).parent, parent);
    QTreeWidgetItem *childRow = rowOf(tree, ShowTreeView::LayerNode, child, c1);
    QVERIFY(childRow != nullptr);
    QCOMPARE(childRow->parent(), rowOf(tree, ShowTreeView::LayerNode, parent, c1));
    // Without ⇧ the same group joins the layer, as before.
    QCOMPARE(view.dropHint(c1, parent, idsMime(FixtureGroupSource::fixtureGroupMimeType(), { us->id() })),
             QString("add US to layer Steps in 1 One (⇧: a child layer on it)"));

    // Its page: what it sets, and what its parent lights for it, grey.
    st->setLayerLook(c1, child, st->lookForPalette(red));
    QVERIFY(view.selectNode(child, c1));
    QCOMPARE(rowFor(view.attributeRows(), "Colour"), QString("Colour | Red | "));
    QCOMPARE(rowFor(view.attributeRows(), "Dimmer"), QString("Dimmer | Full | from Steps (parent)"));
    // The cue page lists it under its parent.
    QVERIFY(view.selectNode(c1));
    QVERIFY2(view.cueLayerItems().join("|").contains(QString::fromUtf8("   ↳ US")),
             qPrintable(view.cueLayerItems().join("|")));
    // The command line: US in Steps.
    QVERIFY2(view.runCommand("US in Steps dimmer 50"), qPrintable(view.commandMessage()));
    QCOMPARE(st->cue(c1)->layers.count(), 2);
    QVERIFY(st->cue(c1)->layers.at(1).look(KeyDimmer) != nullptr);

    // Overlap: the cue page's check box.
    QVERIFY(view.selectNode(c1));
    QCheckBox *mix = view.findChild<QCheckBox *>("stCueMix");
    QVERIFY(mix != nullptr && !mix->isChecked());
    mix->click();
    QVERIFY(st->cue(c1)->mixOverlap);
    QVERIFY(st->undo());
    QVERIFY(!st->cue(c1)->mixOverlap);
}

/* Branson (2026-10-17): a layer's name is its own, renamed as in the
   Programming tab -- Return or F2 on the row, Return commits, Esc cancels,
   empty goes back to its groups' name; one undo step; it stays the same
   layer (tracking unchanged). A cue's name, not its number; a list's. */
void ShowTreeView_Test::renameInPlace()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    Target t;
    t.groups << towers->id();
    const quint32 layer = st->addLayer(c1, t);
    st->setLayerLook(c1, layer, st->lookForPalette(full));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    auto rename = [&](int key, const QString &text, int finish) {
        tree->setFocus();
        QTest::keyClick(tree, Qt::Key(key));
        QTRY_VERIFY(view.isRenaming());
        QLineEdit *editor = qobject_cast<QLineEdit *>(QApplication::focusWidget());
        QVERIFY(editor != nullptr);
        editor->clear();
        QTest::keyClicks(editor, text);
        QTest::keyClick(editor, Qt::Key(finish));
        QTRY_VERIFY(!view.isRenaming());
        QTest::qWait(100);
    };

    QVERIFY(view.selectNode(layer, c1));
    const int steps = st->canUndo() ? 1 : 0;
    Q_UNUSED(steps)
    const QString undoBefore = st->undoText();
    rename(Qt::Key_Return, "Hero", Qt::Key_Return);
    QTRY_COMPARE(st->cue(c1)->layer(layer)->name, QString("Hero"));
    QCOMPARE(st->undoText(), QString("Rename"));
    QVERIFY(st->cue(c1)->layer(layer)->target == t);
    // Still the same layer: tracked into Two under its new name.
    const QList<ResolvedCue> r = st->resolved(*st->cueList(list));
    QCOMPARE(r.at(1).layers.first().name, QString("Hero"));
    QCOMPARE(int(r.at(1).layers.first().origin), int(Origin::Tracked));
    QVERIFY(treeText(tree).join("\n").contains("Hero"));
    QVERIFY(st->undo());
    QVERIFY(st->cue(c1)->layer(layer)->name.isEmpty());
    QCOMPARE(st->undoText(), undoBefore);
    QVERIFY(st->redo());

    // Esc: nothing changes.
    QVERIFY(view.selectNode(layer, c1));
    rename(Qt::Key_F2, "Nope", Qt::Key_Escape);
    QCOMPARE(st->cue(c1)->layer(layer)->name, QString("Hero"));
    QTRY_VERIFY(treeText(tree).join("\n").contains("Hero"));

    // Empty: back to its groups' name.
    QVERIFY(view.selectNode(layer, c1));
    rename(Qt::Key_Return, "", Qt::Key_Return);
    QTRY_VERIFY(st->cue(c1)->layer(layer)->name.isEmpty());

    // A cue: its name, the number stays.
    QVERIFY(view.selectNode(c2));
    rename(Qt::Key_Return, "Verse", Qt::Key_Return);
    QTRY_COMPARE(st->cue(c2)->name, QString("Verse"));
    QCOMPARE(st->cue(c2)->number, QString("2"));
    // A cue list.
    QVERIFY(view.selectNode(list));
    rename(Qt::Key_F2, "Opener", Qt::Key_Return);
    QTRY_COMPARE(st->cueList(list)->name, QString("Opener"));

    // The command line names the selected layer.
    QVERIFY(view.selectNode(layer, c1));
    QVERIFY2(view.runCommand("name \"Lead\""), qPrintable(view.commandMessage()));
    QCOMPARE(st->cue(c1)->layer(layer)->name, QString("Lead"));
    QVERIFY2(view.runCommand("name \"\""), qPrintable(view.commandMessage()));
    QVERIFY(st->cue(c1)->layer(layer)->name.isEmpty());
}

namespace
{
Fixture *placedSpot(Doc *doc, const QString &name, double x)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt };
    for (int k = 0; k < 2; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(k == 0 ? "Pan" : "Tilt");
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    head.addChannel(0);
    head.addChannel(1);
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(quint32(100 + doc->fixtures().size() * 2));
    if (!doc->addFixture(fxi))
        return nullptr;
    doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0, QVector3D(float(x * 1000.0), 0, 6));
    return fxi;
}
}

/* D19 / open 14: the plan view under a line aim's order strip -- the line
   from above, each light's beam to its point, and the previous cue's
   points as rings; it follows Spread at once. Rendered offscreen. */
void ShowTreeView_Test::planViewOfALineAim()
{
    QList<Fixture *> spots { placedSpot(m_doc, "L1", -4), placedSpot(m_doc, "L2", -1),
                             placedSpot(m_doc, "R1", 2), placedSpot(m_doc, "R2", 4) };
    FixtureGroup *all = new FixtureGroup(m_doc);
    all->setName("All");
    m_doc->addFixtureGroup(all);
    for (Fixture *f : spots)
        all->assignFixture(f->id());
    StageTarget *edge = m_doc->monitorProperties()->addStageTarget();
    edge->setName("DS-Edge");
    edge->setPosition(QVector3D(0, 3, 0));
    edge->setLinePoints({ QVector3D(-5, 0, 0), QVector3D(5, 0, 0) });
    edge->makeLine();
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(list, -1, "Wide");
    const quint32 c2 = st->addCue(list, 0, "Tight");
    Target t;
    t.groups << all->id();
    const quint32 l1 = st->addLayer(c1, t);
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(st->aimPaletteFor(edge->id()))));
    QVERIFY(st->setLayerLook(c1, l1, ShowTreeController::lineParamLook(KeySpread, "5")));
    // Cue 2: the same lights, Spread 0 (all to the centre) -- tracked aim.
    const quint32 l2 = st->addLayer(c2, t);
    QVERIFY(st->setLayerLook(c2, l2, ShowTreeController::lineParamLook(KeySpread, "0")));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 900);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // Cue 1: four beams, no cue before it, no rings.
    QVERIFY(view.selectNode(l1, c1));
    LinePlanView *planView = qobject_cast<LinePlanView *>(view.linePlan());
    QVERIFY(planView != nullptr);
    QCOMPARE(planView->beams().count(), 4);
    QVERIFY(!planView->line().isEmpty());
    QStringList order;
    for (const LinePlanView::Beam &b : planView->beams())
    {
        order << b.name;
        QVERIFY(!b.hasPrevious);
    }
    QCOMPARE(order, QStringList({ "L1", "L2", "R1", "R2" }));
    const double wide = planView->beams().last().to.x() - planView->beams().first().to.x();
    QVERIFY2(wide > 5.0, qPrintable(QString::number(wide)));

    // Cue 2: Spread 0 puts every light on one point; the rings are cue 1's.
    QVERIFY(view.selectNode(l2, c2));
    planView = qobject_cast<LinePlanView *>(view.linePlan());
    QVERIFY(planView != nullptr);
    QCOMPARE(planView->beams().count(), 4);
    for (const LinePlanView::Beam &b : planView->beams())
    {
        QVERIFY(b.hasPrevious);
        QVERIFY(qAbs(b.to.x() - planView->beams().first().to.x()) < 0.01);
    }
    QVERIFY(planView->beams().last().previous.x() - planView->beams().first().previous.x() > 5.0);

    // Live: Spread changed in cue 2, the plan follows.
    QVERIFY(view.setLineParam(c2, l2, KeySpread, "3"));
    QVERIFY(view.selectNode(l2, c2));
    planView = qobject_cast<LinePlanView *>(view.linePlan());
    QVERIFY(planView->beams().last().to.x() - planView->beams().first().to.x() > 0.5);

    // Rendered offscreen: the line (amber), the points, the rings drawn.
    planView->resize(320, 200);
    const QImage img = planView->grab().toImage();
    int amber = 0, ring = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
        {
            const QColor px = img.pixelColor(x, y);
            if (px.red() > 200 && px.green() > 140 && px.blue() < 140)
                ++amber;
            if (px.blue() > 200 && px.red() < 180 && px.green() > 140)
                ++ring;
        }
    QVERIFY2(amber > 100, qPrintable(QString::number(amber)));
    QVERIFY2(ring > 10, qPrintable(QString::number(ring)));
    if (qEnvironmentVariableIsSet("QLC_SHOWTREE_SHOT"))
        img.save(qEnvironmentVariable("QLC_SHOWTREE_SHOT") + "-plan.png");
}

/* §3B in the tab: Library → Targets; a line dropped on the cue is one
   formation over its layers (the rows say so, the order strip numbers the
   lights across both by where they hang); Spread / Grouping / Cross rows
   under Position, set from ⋯, the cue's Line ⋯ and the command line. */
void ShowTreeView_Test::lineAimsInTheTab()
{
    Fixture *l1 = placedSpot(m_doc, "L1", -4), *l2 = placedSpot(m_doc, "L2", -1);
    Fixture *r1 = placedSpot(m_doc, "R1", 2), *r2 = placedSpot(m_doc, "R2", 4);
    FixtureGroup *left = new FixtureGroup(m_doc);
    left->setName("Left");
    m_doc->addFixtureGroup(left);
    left->assignFixture(l1->id());
    left->assignFixture(l2->id());
    FixtureGroup *right = new FixtureGroup(m_doc);
    right->setName("Right");
    m_doc->addFixtureGroup(right);
    right->assignFixture(r1->id());
    right->assignFixture(r2->id());
    StageTarget *edge = m_doc->monitorProperties()->addStageTarget();
    edge->setName("DS-Edge");
    edge->setPosition(QVector3D(0, 3, 0));
    edge->setLinePoints({ QVector3D(-5, 0, 0), QVector3D(5, 0, 0) });
    edge->makeLine();
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(list, -1, "One");
    Target tl, tr;
    tl.groups << left->id();
    tr.groups << right->id();
    const quint32 ll = st->addLayer(c1, tl);
    st->addLayer(c1, tr);

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTRY_VERIFY(libraryItem(view.library(), "DS-Edge (line)") != nullptr);
    QVERIFY(view.selectNode(c1));
    QLabel *hint = view.findChild<QLabel *>("stDropHint");
    QWidget *page = view.findChild<QWidget *>("stCuePage");
    QLabel *zone = view.findChild<QLabel *>("stCueDropZone");
    const QPoint onZone = zone->mapTo(page, zone->rect().center());
    QCOMPARE(dragAndDrop(page, onZone, idsMime(ShowTreeView::targetMimeType(), { edge->id() }), hint),
             QString("Aim at DS-Edge → cue look on 1 One: one formation over its layers"));
    QVERIFY(st->cue(c1)->look(KeyPosition) != nullptr);

    QVERIFY(view.selectNode(ll, c1));
    QCOMPARE(rowFor(view.attributeRows(), "Position"),
             QString("Position | Aim DS-Edge | from cue 1 One · one formation with Right"));
    QCOMPARE(rowFor(view.attributeRows(), "Spread"), QString("Spread | +5 | the Aim look's"));
    QCOMPARE(rowFor(view.attributeRows(), "Grouping"), QString("Grouping | Each light | the Aim look's"));
    LineSpreadPreview *strip = qobject_cast<LineSpreadPreview *>(view.lineStrip());
    QVERIFY(strip != nullptr);
    QStringList names;
    for (const LineSpreadPreview::Dot &d : strip->dots())
        names << d.name;
    QCOMPARE(names, QStringList({ "L1", "L2", "R1", "R2" }));

    // The cue's Spread (Line ⋯): reaches both, as one.
    QVERIFY(view.setLineParam(c1, invalidId(), KeySpread, "0"));
    QVERIFY(view.selectNode(ll, c1));
    QCOMPARE(rowFor(view.attributeRows(), "Spread"), QString("Spread | 0 | from cue 1 One"));
    // Its own Grouping: it leaves the formation.
    QVERIFY(view.setLineParam(c1, ll, KeyGrouping, "pairs"));
    QCOMPARE(rowFor(view.attributeRows(), "Grouping"), QString("Grouping | Pairs | "));
    QVERIFY(!rowFor(view.attributeRows(), "Position").contains("one formation"));
    QVERIFY(st->undo());
    // Typed.
    QVERIFY2(view.runCommand("Right spread -5 group pairs cross mirror"), qPrintable(view.commandMessage()));
    const Layer *rl = nullptr;
    for (const Layer &l : st->cue(c1)->layers)
        if (l.target == tr)
            rl = &l;
    QVERIFY(rl != nullptr);
    QCOMPARE(rl->look(KeySpread)->value, QString("-5"));
    QCOMPARE(rl->look(KeyGrouping)->value, QString("pairs"));
    QCOMPARE(rl->look(KeyCross)->value, QString("mirror"));
    QVERIFY2(view.runCommand("Left aim DS-Edge"), qPrintable(view.commandMessage()));
    QVERIFY(!view.runCommand("Left spread 9"));
    QVERIFY(view.commandMessage().contains("spread -5"));
}

/* ⌘C / ⌘V / ⌘D in the tree: cues (as a run, after the selected cue) and
   layers (into the selected cue), one undo step each; the command line's
   own ⌘C / ⌘V are left alone. `help` shows over the command line, Esc
   closes it, a click on an example puts it in the line. */
void ShowTreeView_Test::copyPasteAndHelp()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCueNumbered(list, "1", "One");
    const quint32 c2 = st->addCueNumbered(list, "2", "Two");
    Target t;
    t.groups << towers->id();
    st->setLayerLook(c1, st->addLayer(c1, t), st->lookForPalette(full));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    QVERIFY(view.selectNode(c1));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Copy);
    QVERIFY(view.selectNode(c2));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Paste);
    QCOMPARE(st->cueList(list)->cues.count(), 3);
    QCOMPARE(st->cueList(list)->cues.last().number, QString("3"));
    QCOMPARE(st->cueList(list)->cues.last().name, QString("One"));
    QCOMPARE(view.currentId(), st->cueList(list)->cues.last().id);
    QVERIFY(st->undo());
    QCOMPARE(st->cueList(list)->cues.count(), 2);
    // ⌘D: right after, as a point cue between 1 and 2.
    QVERIFY(view.selectNode(c1));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence(Qt::CTRL | Qt::Key_D));
    QCOMPARE(st->cueList(list)->cues.at(1).number, QString("1.5"));
    // A layer copied, pasted into another cue.
    const quint32 layer = st->cue(c1)->layers.first().id;
    QVERIFY(view.selectNode(layer, c1));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Copy);
    QVERIFY(view.selectNode(c2));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Paste);
    QCOMPARE(st->cue(c2)->layers.count(), 1);
    QVERIFY(st->cue(c2)->layers.first().target == t);

    // The command line keeps its own copy and paste.
    QLineEdit *line = view.commandLine();
    line->setFocus();
    line->setText("Towers dimmer");
    line->selectAll();
    const int cues = int(st->cueList(list)->cues.count());
    QTest::keySequence(line, QKeySequence::Copy);
    QTest::keySequence(line, QKeySequence::Paste);
    QCOMPARE(int(st->cueList(list)->cues.count()), cues);
    QCOMPARE(st->cue(c2)->layers.count(), 1);

    // help: shown, Esc closes it; an example clicked goes in the line.
    QVERIFY(!view.isHelpShown());
    QVERIFY(view.runCommand("help"));
    QVERIFY(view.isHelpShown());
    QVERIFY(view.helpOutput().contains("Record and copy"));
    QVERIFY(view.runCommand("help paste"));
    QVERIFY2(view.helpOutput().contains("after the selected cue"), qPrintable(view.helpOutput()));
    QTextBrowser *out = view.findChild<QTextBrowser *>("stCommandHelp");
    emit out->anchorClicked(QUrl("ex:" + QString::fromUtf8(QUrl::toPercentEncoding("Towers dimmer 45"))));
    QCOMPARE(line->text(), QString("Towers dimmer 45"));
    line->setFocus();
    QTest::keyClick(line, Qt::Key_Escape);
    QVERIFY(!view.isHelpShown());
    // A misspelling says what was meant.
    QVERIFY(!view.runCommand("recrod"));
    QVERIFY2(view.commandMessage().contains("record"), qPrintable(view.commandMessage()));
}

/* Branson (2026-10-17): Library | Tree | Editor, the Look Editor under the
   editor; a drop below the tree's last row is a new layer in the selected
   cue; and adding without dragging -- Return / double-click in the Library
   acts on the tree's selection, ⌘F or / goes to the filter, Return there
   adds the first match. One undo step each; the hint says what Return does. */
/* A split saved while the Workbench was docked in the Show Tree (four panes,
   the hidden page's at 0) brought the page back at no width on the next
   start. Such a state is dropped; and a split moved while docked is not
   saved at all -- closing the Workbench puts the split back as it was. */
void ShowTreeView_Test::aSplitSavedWhileDockedDoesNotHideThePage()
{
    // The user's own showtree/splitter2: 127 | 171 | 0 | 1162.
    const QByteArray users = QByteArray::fromBase64("AAAA/wAAAAEAAAAEAAAAfwAAAKsAAAAAAAAEigH/////AQAAAAEA");
    QSettings().setValue("showtree/splitter2", users);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCueNumbered(list, "1", "One");
    {
        ShowTreeView view(nullptr, m_doc);
        view.resize(1400, 800);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QSplitter *split = view.findChild<QSplitter *>("stSplitter");
        QSplitter *page = view.findChild<QSplitter *>("stVSplitter");
        QVERIFY(split != nullptr && page != nullptr);
        QCOMPARE(split->count(), 3);
        QVERIFY2(page->width() > 200, qPrintable(QString("the page came back %1 px wide").arg(page->width())));
        QVERIFY2(!QSettings().contains("showtree/splitter2"), "the four-pane state was kept");

        // Docked: moving the split saves nothing.
        QVERIFY(view.openInWorkbench(c1));
        QVERIFY(view.isWorkbenchDocked());
        QCOMPARE(split->count(), 4);
        emit split->splitterMoved(300, 1);
        QVERIFY2(!QSettings().contains("showtree/splitter2"), "a split was saved with the Workbench docked");
        QVERIFY(view.closeWorkbench());
        QVERIFY(page->isVisible() && page->width() > 200);
        // Back to the page: a move is saved, three panes, the page showing.
        emit split->splitterMoved(300, 1);
        const QByteArray saved = QSettings().value("showtree/splitter2").toByteArray();
        QVERIFY(!saved.isEmpty());
        QDataStream in(saved);
        in.setVersion(QDataStream::Qt_5_0);
        qint32 marker = 0, version = 0;
        QList<int> sizes;
        in >> marker >> version >> sizes;
        QCOMPARE(sizes.size(), 3);
        QVERIFY(sizes.at(2) > 0);
    }
    // A good three-pane state is still restored.
    {
        ShowTreeView view(nullptr, m_doc);
        view.resize(1400, 800);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY(QSettings().contains("showtree/splitter2"));
        QVERIFY(view.findChild<QSplitter *>("stVSplitter")->width() > 200);
    }
    QSettings().remove("showtree/splitter2");
}

void ShowTreeView_Test::libraryTreeEditorAndAddingWithoutDragging()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    group("Floor", 4, 1);
    const quint32 blue = colour(Qt::blue, "Blue");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCueNumbered(list, "1", "One");

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QSplitter *split = view.findChild<QSplitter *>("stSplitter");
    QVERIFY(split != nullptr && split->count() == 3);
    QVERIFY(split->widget(0)->isAncestorOf(view.library()));
    QCOMPARE(split->widget(1), static_cast<QWidget *>(view.tree()));
    QSplitter *editorCol = qobject_cast<QSplitter *>(split->widget(2));
    QVERIFY(editorCol != nullptr);
    QVERIFY(editorCol->isAncestorOf(view.lookEditor()));
    QVERIFY(editorCol->isAncestorOf(view.findChild<QWidget *>("stInspector")));

    // Below the last row: a new layer in the selected cue.
    QTreeWidget *tree = view.tree();
    QLabel *hint = view.findChild<QLabel *>("stDropHint");
    QVERIFY(view.selectNode(c1));
    const QPoint below(20, tree->viewport()->height() - 5);
    QVERIFY(tree->itemAt(below) == nullptr);
    QCOMPARE(dragAndDrop(tree->viewport(), below, idsMime(FixtureGroupSource::fixtureGroupMimeType(), { towers->id() }),
                         hint), QString("New layer in 1 One: Towers"));
    QCOMPARE(st->cue(c1)->layers.count(), 1);

    // Return in the Library, a cue selected: a new layer, the cue stays.
    QVERIFY(view.selectNode(c1));
    QTreeWidget *lib = view.library();
    lib->clearSelection();
    QTreeWidgetItem *u = nullptr;
    for (QTreeWidgetItemIterator it(lib); *it; ++it)
        if ((*it)->text(0) == "USTs (2)" && (*it)->parent()->text(0) == "Groups")
            u = *it;
    QVERIFY(u != nullptr);
    u->setSelected(true);
    lib->setCurrentItem(u);
    QCOMPARE(view.libraryReturnHint(), QString("Return: a new layer on USTs in 1 One"));
    lib->setFocus();
    QTest::keyClick(lib, Qt::Key_Return);
    QCOMPARE(st->cue(c1)->layers.count(), 2);
    QCOMPARE(view.currentKind(), ShowTreeView::CueNode);
    QCOMPARE(st->undoText(), QString("Add layer"));
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c1)->layers.count(), 1);

    // A layer selected: double-click a palette -> that layer's look; a
    // group -> added to it.
    const quint32 layer = st->cue(c1)->layers.first().id;
    QVERIFY(view.selectNode(layer, c1));
    lib->clearSelection();
    QTreeWidgetItem *b = libraryItem(lib, "Blue");
    QVERIFY(b != nullptr);
    b->setSelected(true);
    emit lib->itemDoubleClicked(b, 0);
    QVERIFY(st->cue(c1)->layer(layer)->look(KeyColour) != nullptr);
    QVERIFY(view.selectNode(layer, c1));
    lib->clearSelection();
    u = nullptr;                                // the Library was rebuilt
    for (QTreeWidgetItemIterator it(lib); *it; ++it)
        if ((*it)->text(0) == "USTs (2)" && (*it)->parent()->text(0) == "Groups")
            u = *it;
    QVERIFY(u != nullptr);
    u->setSelected(true);
    QVERIFY(view.applyLibrarySelection());
    QCOMPARE(st->cue(c1)->layer(layer)->target.groups, QList<quint32>({ towers->id(), usts->id() }));
    Q_UNUSED(blue)

    // Keyboard only: ⌘F, type, Return -- the first match, as a new layer.
    QVERIFY(view.selectNode(c1));
    tree->setFocus();
    QTest::keyClick(tree, Qt::Key_Slash);
    QLineEdit *filter = view.findChild<QLineEdit *>("stLibraryFilter");
    QTRY_VERIFY(filter->hasFocus());
    QTest::keyClicks(filter, "Floo");
    lib->clearSelection();
    QTest::keyClick(filter, Qt::Key_Return);
    QCOMPARE(st->cue(c1)->layers.count(), 2);
    QVERIFY(st->cue(c1)->layers.last().target.groups.count() == 1);
    tree->setFocus();
    QTest::keySequence(&view, QKeySequence::Find);
    QTRY_VERIFY(filter->hasFocus());
}

/* Branson (2026-10-17): a named layer still shows what is in it -- in the
   tree after its name (whole in the tooltip), on the cue page, and on its
   page as "Contains:" with each group a link (to the Library) and a ✕ to
   take it out. Its name works on the command line and completes. */
void ShowTreeView_Test::namedLayersShowWhatTheyHold()
{
    FixtureGroup *a = group("TrussC", 0, 2);
    FixtureGroup *b = group("TrussW", 2, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCueNumbered(list, "1", "One");
    Target t;
    t.groups << a->id() << b->id();
    const quint32 layer = st->addLayer(c1, t);

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidgetItem *row = rowOf(view.tree(), ShowTreeView::LayerNode, layer, c1);
    QCOMPARE(row->text(0), QString("TrussC + TrussW"));       // unnamed: its groups
    QVERIFY(st->renameNode(layer, "Truss wash"));
    row = rowOf(view.tree(), ShowTreeView::LayerNode, layer, c1);
    QCOMPARE(row->text(0), QString("Truss wash"));           // the short name only
    QVERIFY2(row->toolTip(0).startsWith("TrussC (2) · TrussW (2) — 4 fixture(s)"), qPrintable(row->toolTip(0)));
    QVERIFY(view.selectNode(c1));
    QCOMPARE(view.cueLayerItems().first(),
             QString("Truss wash\n    TrussC (2) · TrussW (2) — 4 fixture(s)"));

    QVERIFY(view.selectNode(layer, c1));
    QCOMPARE(view.layerContentsText(), QString("Contains: TrussC (2) ✕ · TrussW (2) ✕ — 4 fixture(s)"));
    QLabel *contents = view.findChild<QLabel *>("stLayerContents");
    emit contents->linkActivated(QString("show:g:%1").arg(b->id()));
    QVERIFY(view.library()->currentItem() != nullptr);
    QCOMPARE(view.library()->currentItem()->data(0, ShowTreeView::IdRole).toUInt(), b->id());
    emit contents->linkActivated(QString("rm:g:%1").arg(a->id()));
    QCOMPARE(st->cue(c1)->layer(layer)->target.groups, QList<quint32>({ b->id() }));
    QCOMPARE(st->cue(c1)->layer(layer)->name, QString("Truss wash"));
    QVERIFY(view.selectNode(layer, c1));
    QCOMPARE(view.layerContentsText(), QString("Contains: TrussW (2) — 2 fixture(s)"));   // the last: no ✕
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c1)->layer(layer)->target.groups.count(), 2);

    // Its name on the command line, and Tab.
    QVERIFY(view.selectNode(c1));
    QVERIFY2(view.runCommand("Truss wash dimmer Full"), qPrintable(view.commandMessage()));
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    QCOMPARE(st->cue(c1)->layer(layer)->look(KeyDimmer)->palettes, QList<quint32>({ full }));
    QLineEdit *line = view.commandLine();
    line->setFocus();
    line->setText("Truss w");
    QTest::keyClick(line, Qt::Key_Tab);
    QCOMPARE(line->text(), QString("Truss wash "));
    // The command line says what a selected layer holds.
    QVERIFY(view.selectNode(layer, c1));
    QVERIFY2(line->placeholderText().contains("TrussC (2) · TrussW (2)"), qPrintable(line->placeholderText()));
}

/* Branson (2026-10-17): several layers selected are edited together -- the
   page says how many, Release is ticked / unticked / part-ticked and a click
   sets it on all, a value set (dropped, picked) goes on all in one undo
   step, a row shows the value they share or "mixed"; the command line acts
   on them; across cues too. */
void ShowTreeView_Test::editSeveralLayersAtOnce()
{
    FixtureGroup *a = group("A", 0, 1);
    FixtureGroup *b = group("B", 1, 1);
    FixtureGroup *c = group("C", 2, 1);
    FixtureGroup *d = group("D", 3, 1);
    const quint32 blue = colour(Qt::blue, "Blue");
    const quint32 red = colour(Qt::red, "Red");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCueNumbered(list, "1", "One");
    const quint32 c2 = st->addCueNumbered(list, "2", "Two");
    auto layerOn = [&](quint32 cue, FixtureGroup *g) {
        Target t;
        t.groups << g->id();
        return st->addLayer(cue, t);
    };
    const quint32 la = layerOn(c1, a), lb = layerOn(c1, b), lc = layerOn(c1, c), ld = layerOn(c2, d);
    st->setLayerRelease(c1, la, true);

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    view.tree()->clearSelection();
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, la, c1));
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lb, c1), Qt::ControlModifier);
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lc, c1), Qt::ControlModifier);
    QCOMPARE(view.multiLayers().count(), 3);
    QLabel *title = view.findChild<QWidget *>("stLayerPage")->findChildren<QLabel *>().first();
    QCOMPARE(title->text(), QString("3 layers in cue 1 One — edited together"));
    QCOMPARE(view.layerContentsText(), QString("A · B · C — 3 fixture(s)"));

    // Release: part-ticked; a click releases all; again, carries all.
    QCheckBox *release = view.findChild<QCheckBox *>("stLayerRelease");
    QCOMPARE(release->checkState(), Qt::PartiallyChecked);
    release->click();
    QVERIFY(st->cue(c1)->layer(lb)->release && st->cue(c1)->layer(lc)->release);
    QCOMPARE(st->undoText(), QString("Release 3 layers"));
    QCOMPARE(view.multiLayers().count(), 3);            // still edited together
    QCOMPARE(release->checkState(), Qt::Checked);
    release->click();
    QVERIFY(!st->cue(c1)->layer(la)->release && !st->cue(c1)->layer(lc)->release);

    // A colour on all three, one undo step; the row shows it.
    QVERIFY(view.dropPalettes(c1, view.currentId(), { blue }));
    for (quint32 l : { la, lb, lc })
        QCOMPARE(st->cue(c1)->layer(l)->look(KeyColour)->palettes, QList<quint32>({ blue }));
    QCOMPARE(st->undoText(), QString("Set Colour on 3 layers"));
    QCOMPARE(rowFor(view.attributeRows(), "Colour"), QString("Colour | Blue | "));
    QVERIFY(st->undo());
    QVERIFY(st->cue(c1)->layer(lb)->look(KeyColour) == nullptr);
    QVERIFY(st->redo());
    // One different: "mixed".
    QVERIFY(st->setLayerLook(c1, lb, st->lookForPalette(red)));
    QCOMPARE(rowFor(view.attributeRows(), "Colour"), QString("Colour | mixed | "));
    // None on all from the row.
    QVERIFY(view.setAttributeNone(c1, view.currentId(), KeyColour));
    QVERIFY(st->cue(c1)->layer(la)->look(KeyColour)->none && st->cue(c1)->layer(lb)->look(KeyColour)->none);

    // The command line: an attribute first acts on the selection.
    QVERIFY2(view.runCommand("dimmer 45"), qPrintable(view.commandMessage()));
    for (quint32 l : { la, lb, lc })
        QVERIFY(st->cue(c1)->layer(l)->look(KeyDimmer) != nullptr);
    QVERIFY(st->undo());
    QVERIFY(st->cue(c1)->layer(la)->look(KeyDimmer) == nullptr);
    QVERIFY(st->redo());
    QVERIFY2(view.runCommand("release on"), qPrintable(view.commandMessage()));
    QVERIFY(st->cue(c1)->layer(lc)->release);

    // Across cues.
    view.tree()->clearSelection();
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, la, c1));
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, ld, c2), Qt::ControlModifier);
    QCOMPARE(view.multiLayers().count(), 2);
    QCOMPARE(view.findChild<QWidget *>("stLayerPage")->findChildren<QLabel *>().first()->text(),
             QString("2 layers in 2 cues — edited together"));
    QVERIFY(view.dropPalettes(c2, ld, { red }));
    QCOMPARE(st->cue(c1)->layer(la)->look(KeyColour)->palettes, QList<quint32>({ red }));
    QCOMPARE(st->cue(c2)->layer(ld)->look(KeyColour)->palettes, QList<quint32>({ red }));
    QVERIFY(view.setReleaseOnSelected(true));
    QVERIFY(st->cue(c2)->layer(ld)->release);
}

/****************************************************************************
 * P4 (2026-10-17): reusable layers and cues, Detach a list, Convert
 ****************************************************************************/

/* Make reusable: the tree marks a use ⟲ (and how many), the page says so
   with Used by and Unlink here, the Library lists it under All → Reusable,
   and a drop of it on a cue is a use there -- an edit to one is an edit to
   all. Delete asks while it is used. */
void ShowTreeView_Test::reusablesInTheTreeAndTheLibrary()
{
    FixtureGroup *bs = group("BackStage", 0, 2);
    FixtureGroup *steps = group("Steps", 2, 1);
    const quint32 blue = colour(Qt::blue, "Blue");
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "L");
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 l1 = st->addLayer(c1, Target { { bs->id() }, {}, {}, 0xFFFFFFFF });
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(blue)));
    const quint32 other = st->addCueList(prod, "Other");
    const quint32 o1 = st->addCue(other, -1, "Walk-in");
    QVERIFY(st->setLayerLook(o1, st->addLayer(o1, Target { { steps->id() }, {}, {}, 0xFFFFFFFF }),
                             st->lookForPalette(full)));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QStringList asked;
    view.setConfirm([&asked](const QString &title, const QString &text, const QString &) {
        asked << title + ": " + text;
        return true;
    });

    // Before: the page offers it.
    QVERIFY(view.selectNode(l1, c1));
    QVERIFY(view.reuseLineText().contains("Make reusable"));
    QVERIFY(libraryItem(view.library(), "(none yet: right-click a layer or a cue → Make reusable…)") != nullptr);
    const quint32 rid = st->makeLayerReusable(c1, l1, "Back Wash");
    QVERIFY(rid != invalidId());
    QTRY_VERIFY(treeText(view.tree()).join("\n").contains("⟲ Back Wash"));
    QTRY_VERIFY(libraryItem(view.library(), "⟲ Back Wash — layer, used in 1") != nullptr);
    QVERIFY(view.selectNode(l1, c1));
    QVERIFY2(view.reuseLineText().contains("Reusable layer Back Wash — used in 1"), qPrintable(view.reuseLineText()));

    // Dragged onto the other list's cue page: a use there, said before.
    QVERIFY(view.selectNode(o1));
    QLabel *hint = view.findChild<QLabel *>("stDropHint");
    QWidget *page = view.findChild<QWidget *>("stCuePage");
    QLabel *zone = view.findChild<QLabel *>("stCueDropZone");
    QCOMPARE(dragAndDrop(page, zone->mapTo(page, zone->rect().center()),
                         idsMime(ShowTreeView::reusableMimeType(), { rid }), hint),
             QString("⟲ Back Wash → a layer in 1 Walk-in, linked: an edit to it is an edit to every use"));
    QCOMPARE(st->reusableUses(rid).count(), 2);
    QTRY_VERIFY(libraryItem(view.library(), "⟲ Back Wash — layer, used in 2") != nullptr);
    const quint32 use = st->cue(o1)->layers.last().id;
    // An edit there (a palette dropped on its row) reaches list L's.
    QVERIFY(view.dropPalettes(o1, use, { full }));
    QCOMPARE(st->cue(c1)->layer(l1)->look(KeyDimmer)->palettes, QList<quint32>({ full }));
    // Used by: both cues.
    const QStringList usedBy = view.reusableUsedBy(rid);
    QCOMPARE(usedBy, QStringList({ "L 1 One", "Other 1 Walk-in" }));

    // A cue made reusable and used in L after cue 2, from the Library (Return).
    const quint32 rc = st->makeCueReusable(o1);
    QVERIFY(rc != invalidId());
    QVERIFY(view.selectNode(c2));
    QTRY_VERIFY(libraryItem(view.library(), "⟲ Walk-in — cue, used in 1") != nullptr);
    view.library()->clearSelection();
    libraryItem(view.library(), "⟲ Walk-in — cue, used in 1")->setSelected(true);
    QVERIFY(view.applyLibrarySelection());
    QCOMPARE(st->cueList(list)->cues.count(), 3);
    const quint32 c3 = st->cueList(list)->cues.last().id;
    QCOMPARE(st->cue(c3)->reusable, rc);
    QCOMPARE(view.currentId(), c3);
    QVERIFY(treeText(view.tree()).join("\n").contains("⟲ 3 Walk-in"));
    QVERIFY(view.reuseLineText().contains("Reusable cue Walk-in — used in 2"));

    // Unlink here, from the page's link: L's use stays as it is, unlinked.
    QLabel *line = view.findChild<QLabel *>("stCueReuse");
    emit line->linkActivated("unlink");
    QCOMPARE(st->cue(c3)->reusable, invalidId());
    QCOMPARE(st->reusableUses(rc).count(), 1);
    // No badge; the row's tooltip says where it came from (open 20).
    QTRY_VERIFY(rowOf(view.tree(), ShowTreeView::CueNode, c3) != nullptr);
    QVERIFY(!rowOf(view.tree(), ShowTreeView::CueNode, c3)->text(0).startsWith("⟲"));
    QVERIFY2(rowOf(view.tree(), ShowTreeView::CueNode, c3)->toolTip(0).startsWith("Unlinked from Walk-in"),
             qPrintable(rowOf(view.tree(), ShowTreeView::CueNode, c3)->toolTip(0)));
    QCOMPARE(st->cue(c3)->unlinkedFrom, QString("Walk-in"));

    // Delete while used: asked, then the uses are plain copies.
    QVERIFY(view.deleteReusable(rid));
    QCOMPARE(asked.count(), 1);
    // L 1, Other 1, L 3 (a copy of Walk-in, its layer still a use) and the reusable cue Walk-in
    QVERIFY2(asked.first().contains("used in 4 place"), qPrintable(asked.first()));
    QVERIFY(st->reusableLayer(rid) == nullptr);
    QCOMPARE(st->cue(c1)->layer(l1)->reusable, invalidId());
    QVERIFY(rowOf(view.tree(), ShowTreeView::LayerNode, l1, c1)->toolTip(0).startsWith("Unlinked from "));
    view.setConfirm([](const QString &, const QString &, const QString &) { return false; });
    QVERIFY(!view.deleteReusable(rc));               // cancelled
    QVERIFY(st->reusableCue(rc) != nullptr);
}

/* The list page's Convert to tracking… shows the plan and converts in one
   step; Detach to plain chaser… asks and hands the chaser over. */
void ShowTreeView_Test::convertAndDetachFromTheListPage()
{
    FixtureGroup *a = group("A", 0, 1);
    FixtureGroup *b = group("B", 1, 1);
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    Scene *sa = new Scene(m_doc);
    sa->setName("A-Full");
    sa->addFixtureGroup(a->id());
    sa->addPalette(full);
    m_doc->addFunction(sa);
    Scene *sb = new Scene(m_doc);
    sb->setName("B-Half");
    sb->addFixtureGroup(b->id());
    sb->addPalette(half);
    m_doc->addFunction(sb);
    Collection *one = new Collection(m_doc);
    one->setName("0.0.1-One");
    m_doc->addFunction(one);
    one->addFunction(sa->id());
    Collection *two = new Collection(m_doc);
    two->setName("0.0.2-Two");
    m_doc->addFunction(two);
    // A copy of A-Full, not A-Full itself: the import tracks only what the
    // steps share (the same function); Convert reads the values.
    Scene *sa2 = new Scene(m_doc);
    sa2->setName("A-Full 2");
    sa2->addFixtureGroup(a->id());
    sa2->addPalette(full);
    m_doc->addFunction(sa2);
    two->addFunction(sa2->id());
    two->addFunction(sb->id());
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Song");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(one->id()));
    chaser->addStep(ChaserStep(two->id()));

    ShowTreeController *st = m_doc->showTree();
    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.importChaser(chaser->id(), false);
    const quint32 listId = st->productions().last().lists.first().id;
    QVERIFY(view.selectNode(listId));
    QPushButton *convert = view.findChild<QPushButton *>("stListConvert");
    QPushButton *detach = view.findChild<QPushButton *>("stListDetach");
    QVERIFY(convert != nullptr && detach != nullptr);
    QVERIFY(convert->isEnabled());

    QString details;
    view.setConfirm([&details](const QString &, const QString &, const QString &d) {
        details = d;
        return true;
    });
    convert->click();
    QVERIFY2(details.contains("0.0.2 Two: tracks from 0.0.1 One"), qPrintable(details));
    QVERIFY2(details.contains("all of it tracks in, layer taken out"), qPrintable(details));
    QCOMPARE(st->cueList(listId)->mode, ListMode::Tracking);
    QCOMPARE(st->cueList(listId)->cues.at(1).layers.count(), 1);
    QVERIFY(view.findChild<QLabel *>("stDropHint")->text().contains("tracks now"));
    // Again: nothing more to convert, said.
    convert->click();
    QVERIFY(view.findChild<QLabel *>("stDropHint")->text().contains("Nothing to convert"));
    QCOMPARE(st->cueList(listId)->mode, ListMode::Tracking);

    const quint32 chaserId = st->cueList(listId)->chaser;
    detach->click();
    QVERIFY(st->cueList(listId) == nullptr);
    QVERIFY(!st->isGenerated(chaserId));
    QVERIFY(m_doc->function(chaserId) != nullptr);
    QVERIFY(st->undo());
    QVERIFY(st->cueList(listId) != nullptr);
    QVERIFY(st->isGenerated(chaserId));
}

/****************************************************************************
 * R2: palettes in the Library (docs/design/PROGRAMMING_ROLE_DESIGN.md §4.2, P-D8)
 ****************************************************************************/

namespace
{
/* A Virtual Console for one test, gone however the test ends. */
struct ScopedVc
{
    explicit ScopedVc(Doc *doc) { new VirtualConsole(nullptr, doc); }
    ~ScopedVc() { delete VirtualConsole::instance(); }
};

/* The tree's state and the palette folders (an empty folder is PaletteOps'
   alone). */
QString paletteState(Doc *doc)
{
    return treeState(doc) + QStringLiteral("\nfolders ") + doc->paletteOps()->folders().join(QLatin1Char('|'));
}
}

/* "creating an effect in Build doesn't update the list in the Show Tree":
   an EFX or RGB Matrix made, renamed or deleted elsewhere is in the
   Library at once; so is a named effect palette. */
void ShowTreeView_Test::anEffectMadeInBuildIsInTheLibraryAtOnce()
{
    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(libraryItem(view.library(), "Sweep") == nullptr);

    EFX *efx = new EFX(m_doc);
    QVERIFY(m_doc->addFunction(efx));
    efx->setName("Sweep");                  // as Build's + New ▾ does: added, then named
    QTRY_VERIFY(libraryItem(view.library(), "Sweep") != nullptr);
    efx->setName("Big Sweep");
    QTRY_VERIFY(libraryItem(view.library(), "Big Sweep") != nullptr);
    QVERIFY(m_doc->deleteFunction(efx->id()));
    QTRY_VERIFY(libraryItem(view.library(), "Big Sweep") == nullptr);

    QLCPalette *ripple = new QLCPalette(QLCPalette::Effect, m_doc);
    ripple->setName("Ripple");
    m_doc->addPalette(ripple);
    QTRY_VERIFY(libraryItem(view.library(), "Ripple") != nullptr);
}

/* "when editing the name in the Show Tree with Enter, it should
   default-select the WORDS, not the CUE# and WORDS". */
void ShowTreeView_Test::renameSelectsTheWords()
{
    QCOMPARE(ShowTreeView::wordsStart("1.2.0 Chorus"), 6);
    QCOMPARE(ShowTreeView::wordsStart("01-02.03-Chorus"), 9);
    QCOMPARE(ShowTreeView::wordsStart("Chorus"), 0);
    QCOMPARE(ShowTreeView::wordsStart("2.5"), 0);

    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "Songs");
    const quint32 c1 = st->addCueNumbered(list, "1.2.0", "Chorus");
    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QVERIFY(view.selectNode(c1));
    view.tree()->setFocus();
    QTest::keyClick(view.tree(), Qt::Key_Return);
    QLineEdit *ed = nullptr;
    QTRY_VERIFY((ed = view.tree()->viewport()->findChild<QLineEdit *>()) != nullptr);
    QCOMPARE(ed->text(), QString("1.2.0 Chorus"));
    QTRY_COMPARE(ed->selectedText(), QString("Chorus"));
    QTest::keyClick(ed, Qt::Key_Escape);

    // No number: all of it (a list).
    QVERIFY(view.selectNode(list));
    view.tree()->setFocus();
    QTest::keyClick(view.tree(), Qt::Key_Return);
    auto songsEditor = [&view]() -> QLineEdit * {
        for (QLineEdit *e : view.tree()->viewport()->findChildren<QLineEdit *>())
            if (e->text() == QString("Songs"))
                return e;
        return nullptr;
    };
    QTRY_VERIFY((ed = songsEditor()) != nullptr);
    QTRY_COMPARE(ed->selectedText(), QString("Songs"));
    QTest::keyClick(ed, Qt::Key_Escape);
}

void ShowTreeView_Test::palettesInTheLibrary()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 blue = colour(Qt::blue, "Blue");
    const quint32 full = dimmer(255, "Full");
    // An older show's layout (the type the first folder) as loading reads it.
    m_doc->palette(blue)->setPath(QLCPalette::migrateLegacyPath("Palettes/Color/01-Song/", QLCPalette::Color));
    m_doc->palette(full)->setPath("Palettes/01-Song/");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "L");
    const quint32 c1 = st->addCue(list, -1, "One");
    QVERIFY(st->setLayerLook(c1, st->addLayer(c1, Target { { towers->id() }, {}, {}, 0xFFFFFFFF }),
                             st->lookForPalette(blue)));

    QSettings().remove("showtree/libraryPalettesByType");
    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.paletteGroupByType());
    // By type, the folder under each type -- read the same both ways.
    QCOMPARE(view.libraryPaletteRows(), QStringList({ "Colour", "  01-Song/", "    Blue", "Dimmer", "  01-Song/",
                                                      "    Full" }));
    // Folders first: one folder, any type in it.
    view.setPaletteGroupByType(false);
    QCOMPARE(view.libraryPaletteRows(), QStringList({ "01-Song/", "  Blue", "  Full" }));

    // + Palette, in the folder selected: made, selected, in the Look Editor,
    // Used by: nothing yet.
    QTreeWidgetItem *song = libraryItem(view.library(), "01-Song");
    QVERIFY(song != nullptr);
    view.library()->setCurrentItem(song, 0, QItemSelectionModel::ClearAndSelect);
    QCOMPARE(view.libraryPaletteFolder(), QString("01-Song"));
    QToolButton *plus = view.findChild<QToolButton *>("stNewPalette");
    QVERIFY(plus != nullptr && plus->menu() != nullptr);
    const quint32 amber = view.newPaletteFromLibrary(QLCPalette::Color);
    QVERIFY(amber != QLCPalette::invalidId());
    QCOMPARE(m_doc->palette(amber)->name(), QString("New Color"));
    QCOMPARE(m_doc->palette(amber)->path(), QString("Palettes/01-Song/"));
    QCOMPARE(view.editedPalette(), amber);
    QCOMPARE(view.library()->currentItem()->data(0, ShowTreeView::IdRole).toUInt(), amber);
    QCOMPARE(view.usedByText(), QString("Used by: nothing yet"));

    // F2: rename in place.
    QVERIFY(view.startLibraryRename());
    QVERIFY(view.isLibraryRenaming());
    QTreeWidgetItem *editing = view.library()->currentItem();
    view.library()->closePersistentEditor(editing, 0);
    editing->setText(0, "Amber");
    QTRY_COMPARE(m_doc->palette(amber)->name(), QString("Amber"));
    QTRY_VERIFY(view.libraryPaletteRows().contains("  Amber"));

    // New folder; drag (drop) Blue and Amber into it; rename it; delete it.
    view.library()->setCurrentItem(libraryItem(view.library(), "Palettes"), 0, QItemSelectionModel::ClearAndSelect);
    QVERIFY(view.newPaletteFolder("Hits"));
    QVERIFY(m_doc->paletteOps()->folders().contains("Hits"));
    QVERIFY(view.libraryPaletteRows().contains("Hits/"));
    QVERIFY(view.dropPalettesOnFolder({ blue, amber }, "Hits"));
    QCOMPARE(PaletteOps::folderOf(m_doc->palette(blue)), QString("Hits"));
    QCOMPARE(view.libraryPaletteRows(), QStringList({ "01-Song/", "  Full", "Hits/", "  Amber", "  Blue" }));
    view.library()->setCurrentItem(libraryItem(view.library(), "Hits"), 0, QItemSelectionModel::ClearAndSelect);
    QVERIFY(view.startLibraryRename());
    editing = view.library()->currentItem();
    view.library()->closePersistentEditor(editing, 0);
    editing->setText(0, "Stabs");
    QTRY_COMPARE(PaletteOps::folderOf(m_doc->palette(blue)), QString("Stabs"));
    QStringList asked;
    view.setConfirm([&asked](const QString &title, const QString &text, const QString &) {
        asked << title + ": " + text;
        return true;
    });
    QTRY_VERIFY(libraryItem(view.library(), "Stabs") != nullptr);
    view.library()->clearSelection();
    view.library()->setCurrentItem(libraryItem(view.library(), "Stabs"), 0, QItemSelectionModel::ClearAndSelect);
    QVERIFY(view.deleteLibrarySelection());
    QCOMPARE(asked.count(), 1);
    QVERIFY2(asked.first().contains("move up"), qPrintable(asked.first()));
    QCOMPARE(PaletteOps::folderOf(m_doc->palette(blue)), QString());

    // Used by on the Look Editor line: the cue, by number and layer.
    view.editPalette(blue);
    QCOMPARE(view.usedByText(), QString("Used by: 1 cue(s) — which"));
    // Undo (the shared history) puts the folder back.
    view.findChild<QToolButton *>("stUndo")->click();
    QCOMPARE(PaletteOps::folderOf(m_doc->palette(blue)), QString("Stabs"));
    // Back to by type (remembered).
    view.setPaletteGroupByType(true);
    QVERIFY(QSettings().value("showtree/libraryPalettesByType").toBool());
    QVERIFY(view.libraryPaletteRows().contains("Colour"));
    // One-off values never show in All (D21) and aren't managed here.
    const quint32 inl = st->createInlinePalette(QLCPalette::Dimmer, { 10 }, "4% · 1 Towers");
    view.reload();
    for (const QString &row : view.libraryPaletteRows())
        QVERIFY(!row.contains("4% · 1 Towers"));
    QVERIFY(!m_doc->paletteOps()->isManaged(inl));
}

void ShowTreeView_Test::aPaletteDeletedFromTheLibraryComesBackWithUndo()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 blue = colour(Qt::blue, "Blue");
    Scene *old = new Scene(m_doc);
    old->setName("Old Busk");
    old->addFixtureGroup(towers->id());
    old->addPalette(blue);
    m_doc->addFunction(old);
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "L");
    const quint32 c1 = st->addCueNumbered(list, "1.0.0", "One");
    const quint32 l1 = st->addLayer(c1, Target { { towers->id() }, {}, {}, 0xFFFFFFFF });
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(blue)));
    ScopedVc vc(m_doc);
    VCButton *btn = new VCButton(VirtualConsole::instance()->contents(), m_doc);
    VirtualConsole::instance()->placeWidget(btn, VirtualConsole::instance()->contents());
    btn->setCaption("Busk");
    btn->setFunction(old->id());

    {
        ShowTreeView view(nullptr, m_doc);
        view.resize(1200, 800);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QStringList asked;
        bool answer = false;
        view.setConfirm([&](const QString &title, const QString &text, const QString &) {
            asked << title + ": " + text;
            return answer;
        });
        view.library()->clearSelection();
        QTreeWidgetItem *row = nullptr;
        for (QTreeWidgetItemIterator it(view.library()); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LibPalette
                && (*it)->data(0, ShowTreeView::IdRole).toUInt() == blue)
                row = *it;
        QVERIFY(row != nullptr);
        view.library()->setCurrentItem(row);
        row->setSelected(true);
        // Asked, naming the cue (not its generated scene), the scene and the
        // VC button firing that scene. Cancelled: nothing goes.
        QVERIFY(!view.deleteLibrarySelection());
        QCOMPARE(asked.count(), 1);
        QVERIFY2(asked.first().contains("Cue 1.0.0 One in L — Towers"), qPrintable(asked.first()));
        QVERIFY2(asked.first().contains("Scene “Old Busk” — look 1"), qPrintable(asked.first()));
        QVERIFY2(asked.first().contains("VC button “Busk”"), qPrintable(asked.first()));
        QVERIFY(!asked.first().contains(" · Towers"));                 // no generated scene name
        QVERIFY(m_doc->palette(blue) != nullptr);
        // It says the uses go with it, and Undo brings them back (open 26).
        QVERIFY2(asked.first().contains("lose it"), qPrintable(asked.first()));
        QVERIFY2(asked.first().contains("Undo brings it back, and every use with it"), qPrintable(asked.first()));
        QVERIFY(!asked.first().contains("(missing)"));
        // ⌫ in the Library, answered yes: gone, and so is the cue's Colour
        // -- unset, not (missing), not None.
        answer = true;
        QTest::keyClick(view.library(), Qt::Key_Backspace);
        QVERIFY(m_doc->palette(blue) == nullptr);
        QVERIFY(old->palettes().isEmpty());
        QVERIFY(st->cue(c1)->layer(l1)->looks.isEmpty());
        QVERIFY(view.selectNode(l1, c1));
        QVERIFY(!rowFor(view.attributeRows(), "Colour").contains("(missing)"));
        QVERIFY(!rowFor(view.attributeRows(), "Colour").contains("Blue"));
        QCOMPARE(st->sharedUndoText(), QString("Delete Blue"));      // one step
        // Undo: back, with its id, in the scene, and the cue plays it.
        view.findChild<QToolButton *>("stUndo")->click();
        QVERIFY(m_doc->palette(blue) != nullptr);
        QCOMPARE(old->palettes(), QList<quint32>({ blue }));
        QVERIFY(view.selectNode(l1, c1));
        QVERIFY(rowFor(view.attributeRows(), "Colour").contains("Blue"));
        QCOMPARE(st->cue(c1)->layer(l1)->looks.first().palettes, QList<quint32>({ blue }));
        // Redo deletes it again, and its use.
        view.findChild<QToolButton *>("stRedo")->click();
        QVERIFY(m_doc->palette(blue) == nullptr);
        QVERIFY(st->cue(c1)->layer(l1)->looks.isEmpty());
    }
}

/* One action, one undo step (D22) for the Library's palette operations,
   on the history the tab shares with the tree -- and with the Programming
   tab's palette operations (P-D14). */
void ShowTreeView_Test::paletteStepsAreOneUndoStepAndSharedWithProgramming()
{
    const quint32 blue = colour(Qt::blue, "Blue");
    ShowTreeController *st = m_doc->showTree();
    st->addProduction("P");
    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.setConfirm([](const QString &, const QString &, const QString &) { return true; });
    int walked = 0;
    auto oneStep = [&](const char *what, const std::function<bool()> &action) -> bool {
        const QString before = paletteState(m_doc);
        if (!action())
        {
            qWarning() << what << "did nothing";
            return false;
        }
        const QString after = paletteState(m_doc);
        if (after == before)
        {
            qWarning() << what << "changed nothing";
            return false;
        }
        if (!st->undoShared() || paletteState(m_doc) != before)
        {
            qWarning() << what << ": one undo didn't give back the state before it";
            return false;
        }
        if (!st->redoShared() || paletteState(m_doc) != after)
        {
            qWarning() << what << ": redo didn't give back the state after it";
            return false;
        }
        ++walked;
        return true;
    };
    quint32 made = QLCPalette::invalidId();
    QVERIFY(oneStep("+ Palette", [&] {
        made = view.newPaletteFromLibrary(QLCPalette::Dimmer);
        return made != QLCPalette::invalidId();
    }));
    QVERIFY(oneStep("New folder", [&] {
        view.library()->setCurrentItem(libraryItem(view.library(), "Palettes"));
        return view.newPaletteFolder("Levels");
    }));
    QVERIFY(oneStep("a drop on a folder", [&] { return view.dropPalettesOnFolder({ blue }, "Levels"); }));
    QVERIFY(oneStep("Duplicate", [&] {
        return m_doc->paletteOps()->duplicate(blue) != QLCPalette::invalidId();
    }));
    QVERIFY(oneStep("Delete", [&] {
        view.reload();
        view.library()->clearSelection();
        for (QTreeWidgetItemIterator it(view.library()); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LibPalette
                && (*it)->data(0, ShowTreeView::IdRole).toUInt() == blue)
            {
                view.library()->setCurrentItem(*it);
                (*it)->setSelected(true);
            }
        return view.deleteLibrarySelection();
    }));
    QVERIFY(oneStep("palette typed", [&] { return view.runCommand("palette colour \"Amber\" #ffb000"); }));
    QVERIFY(oneStep("palette folder typed", [&] { return view.runCommand("palette folder \"Spare\""); }));
    QCOMPARE(walked, 7);

    // The Programming tab's New palette is a step of the same history: the
    // Show Tree's Undo takes it back.
    {
        ProgrammingManager mgr(nullptr, m_doc);
        const int before = m_doc->palettes().count();
        mgr.createPalette(QLCPalette::Gobo);
        QCOMPARE(m_doc->palettes().count(), before + 1);
        view.findChild<QToolButton *>("stUndo")->click();
        QCOMPARE(m_doc->palettes().count(), before);
        QVERIFY(view.findChild<QToolButton *>("stRedo")->toolTip().startsWith("Redo New Gobo palette"));
    }
}

/* Open 22: Make VC page again updates the frame it made -- a button for
   a new look, the button of a look deleted taken away, a button the user
   moved or resized and a widget of their own left as they are; the frame
   deleted on the console, a new one. */
void ShowTreeView_Test::makeVcPageUpdatesItsFrame()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 a = st->addLook(prod, "Hit A", "Hits");
    const quint32 b = st->addLook(prod, "Hit B", "Hits");
    const quint32 section = st->looksSection(prod);
    ScopedVc vc(m_doc);
    VirtualConsole *console = VirtualConsole::instance();
    auto framesCalled = [console](const QString &caption) {
        QList<VCFrame *> out;
        for (VCFrame *f : console->contents()->findChildren<VCFrame *>())
            if (f->caption() == caption)
                out << f;
        return out;
    };
    auto buttonFor = [](VCFrame *f, quint32 fid) -> VCButton * {
        for (VCButton *btn : f->findChildren<VCButton *>())
            if (btn->function() == fid && !btn->property("stGoing").toBool())
                return btn;
        return nullptr;
    };

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QString msg;
    QVERIFY(view.makeVcPage(section, "Hits", &msg));
    QCOMPARE(framesCalled("Hits — Hits").count(), 1);
    VCFrame *frame = framesCalled("Hits — Hits").first();
    QCOMPARE(st->lookVcFrame(section, "Hits"), frame->id());

    // The user moves and resizes A's button and adds a widget of their own.
    VCButton *ba = buttonFor(frame, st->lookFunction(a));
    QVERIFY(ba != nullptr);
    ba->move(QPoint(100, 45));
    ba->resize(QSize(120, 50));
    const QRect userPlace = ba->geometry();
    VCButton *own = new VCButton(frame, m_doc);
    console->placeWidget(own, frame);
    own->setCaption("My Strobe");
    own->move(QPoint(5, 36));                   // where a new look's button would go first
    own->resize(QSize(80, 80));

    // A look added, one deleted; Make VC page again.
    const quint32 c = st->addLook(prod, "Hit C", "Hits");
    const quint32 bFid = st->lookFunction(b);
    QVERIFY(buttonFor(frame, bFid) != nullptr);
    QVERIFY(st->removeCue(b));
    QVERIFY(view.makeVcPage(section, "Hits", &msg));
    QVERIFY2(msg.contains("1 button(s) added, 1 taken away"), qPrintable(msg));
    QCOMPARE(framesCalled("Hits — Hits").count(), 1);       // the same frame
    QCOMPARE(framesCalled("Hits — Hits").first(), frame);
    QCOMPARE(ba->geometry(), userPlace);                     // left as the user put it
    QVERIFY(buttonFor(frame, st->lookFunction(c)) != nullptr);
    QVERIFY(!buttonFor(frame, st->lookFunction(c))->geometry().intersects(own->geometry()));
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    int ownLeft = 0, gone = 0;
    for (VCButton *btn : frame->findChildren<VCButton *>())
    {
        ownLeft += btn->caption() == "My Strobe";
        gone += btn->caption() == "Hit B";
    }
    QCOMPARE(ownLeft, 1);
    QCOMPARE(gone, 0);
    // Nothing new: up to date.
    QVERIFY(view.makeVcPage(section, "Hits", &msg));
    QVERIFY2(msg.contains("up to date"), qPrintable(msg));

    // The frame deleted on the console: a new one.
    console->removeWidget(frame);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(view.makeVcPage(section, "Hits", &msg));
    QCOMPARE(framesCalled("Hits — Hits").count(), 1);
    QVERIFY(st->lookVcFrame(section, "Hits") != invalidId());
    QCOMPARE(framesCalled("Hits — Hits").first()->findChildren<VCButton *>().count(), 2);
}

/****************************************************************************
 * R3: the Looks section (docs/design/PROGRAMMING_ROLE_DESIGN.md §4.1, P-D5..P-D7)
 ****************************************************************************/

/* Scoped hits (Q154): + Hit makes a hit where it is to fire -- for the
   selected cue, list or production, or (on empty space) for everywhere;
   each place lists its hits under a "Hits" row; a hit dragged to another
   place lives there (⌥ copies); its page says when it fires. */
static QTreeWidgetItem *rowOf(QTreeWidget *tree, quint32 id)
{
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::IdRole).toUInt() == id
            && (*it)->data(0, ShowTreeView::KindRole).toInt() != ShowTreeView::HitsNode
            && (*it)->data(0, ShowTreeView::KindRole).toInt() != ShowTreeView::LayerNode
            && (*it)->data(0, ShowTreeView::KindRole).toInt() != ShowTreeView::TrackedLayerNode)
            return *it;
    return nullptr;
}

void ShowTreeView_Test::hitsLiveUnderTheirPlace()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("DanceHysteria");
    const quint32 list = st->addCueList(prod, "Full Show");
    const quint32 one = st->addCue(list, -1, "One");
    const quint32 two = st->addCue(list, 0, "Two");

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // + Hit for this cue: under the cue, in a Hits row.
    QVERIFY(view.selectNode(one));
    const quint32 strobe = view.newHit();
    QVERIFY(strobe != invalidId());
    quint32 at = invalidId();
    QCOMPARE(st->hitScope(strobe, &at), HitScope::Cue);
    QCOMPARE(at, one);
    QTreeWidgetItem *row = rowOf(view.tree(), strobe);
    QVERIFY(row != nullptr);
    QCOMPARE(row->parent()->data(0, ShowTreeView::KindRole).toInt(), int(ShowTreeView::HitsNode));
    QCOMPARE(row->parent()->text(0), QString("Hits"));
    QCOMPARE(row->parent()->parent(), rowOf(view.tree(), one));
    // Its page says when.
    QVERIFY(view.selectNode(strobe));
    QLabel *when = view.findChild<QLabel *>("stHitWhen");
    QVERIFY(when->isVisibleTo(&view));
    QVERIFY2(when->text().startsWith("While it is live"), qPrintable(when->text()));
    QVERIFY(when->text().contains("not armed now"));
    // + Hit on the Hits row: another for the same cue.
    view.tree()->setCurrentItem(rowOf(view.tree(), strobe)->parent());
    QCOMPARE(view.currentKind(), ShowTreeView::HitsNode);
    QCOMPARE(view.findChild<QToolButton *>("stNewCue")->text(), QString("+ Hit"));
    const quint32 second = view.newHit();
    QCOMPARE(st->hitScope(second, &at), HitScope::Cue);
    QCOMPARE(at, one);

    // For the list, for the production, for everywhere.
    QVERIFY(view.selectNode(list));
    const quint32 red = view.newHit();
    QCOMPARE(st->hitScope(red, &at), HitScope::List);
    QCOMPARE(rowOf(view.tree(), red)->parent()->parent(), rowOf(view.tree(), list));
    QVERIFY(view.selectNode(prod));
    const quint32 wash = view.newHit();
    QCOMPARE(st->hitScope(wash), HitScope::Production);
    QCOMPARE(rowOf(view.tree(), wash)->parent(), rowOf(view.tree(), st->looksSection(prod)));
    view.tree()->setCurrentItem(nullptr);
    const quint32 black = view.newHit(true);
    QCOMPARE(st->hitScope(black), HitScope::Global);
    QTreeWidgetItem *top = view.tree()->topLevelItem(view.tree()->topLevelItemCount() - 1);
    QCOMPARE(top->data(0, ShowTreeView::KindRole).toInt(), int(ShowTreeView::HitsNode));
    QCOMPARE(rowOf(view.tree(), black)->parent(), top);

    // Dragged onto cue Two: it lives there now; one undo step.
    QString hint;
    QVERIFY(view.dropHitsAt({ wash }, rowOf(view.tree(), two), false, false, &hint));
    QVERIFY2(hint.contains("while cue"), qPrintable(hint));
    QVERIFY(view.dropHitsAt({ wash }, rowOf(view.tree(), two), false, true, &hint));
    QCOMPARE(st->hitScope(wash, &at), HitScope::Cue);
    QCOMPARE(at, two);
    QCOMPARE(rowOf(view.tree(), wash)->parent()->parent(), rowOf(view.tree(), two));
    QVERIFY(st->undo());
    QCOMPARE(st->hitScope(wash), HitScope::Production);
    // ⌥: a copy for the list; the hit stays.
    QVERIFY(view.dropHitsAt({ wash }, rowOf(view.tree(), list), true, true, &hint));
    QCOMPARE(st->hitScope(wash), HitScope::Production);
    QCOMPARE(st->hitsAt(HitScope::List, list).count(), 2);
    // Onto a hit of another place: that place. Onto its own: a reorder.
    QVERIFY(view.dropHitsAt({ black }, rowOf(view.tree(), strobe), false, true, &hint));
    QCOMPARE(st->hitScope(black, &at), HitScope::Cue);
    QCOMPARE(at, one);
    QVERIFY(!view.dropHitsAt({ second }, rowOf(view.tree(), strobe), false, false, &hint));
    // Cues are not hits: not this drop.
    QVERIFY(!view.dropHitsAt({ two }, rowOf(view.tree(), list), false, false, &hint));
    // A hit row drags as a hit, its layer row as a layer (lyr): each keeps
    // its own drop.
    {
        const quint32 hl = st->addLayer(strobe, groupTarget(group("Towers", 0, 2)->id()));
        Q_UNUSED(hl);
        QTreeWidgetItem *hitRow = rowOf(view.tree(), strobe);
        QTreeWidgetItem *layerRow = nullptr;
        for (int i = 0; i < hitRow->childCount(); ++i)
            if (hitRow->child(i)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LayerNode)
                layerRow = hitRow->child(i);
        QVERIFY(layerRow != nullptr);
        // The tree model asks the tree's mimeData() for the rows dragged.
        auto mimeOf = [&](QTreeWidgetItem *row) {
            view.tree()->setCurrentItem(row, 0, QItemSelectionModel::ClearAndSelect);
            QModelIndexList at;
            for (const QModelIndex &i : view.tree()->selectionModel()->selectedIndexes())
                if (i.column() == 0)
                    at << i;
            return view.tree()->model()->mimeData(at);
        };
        QScopedPointer<QMimeData> hm(mimeOf(hitRow));
        QVERIFY(hm->hasFormat(ShowTreeView::cueMimeType()));
        layerRow = nullptr;
        for (int i = 0; i < rowOf(view.tree(), strobe)->childCount(); ++i)
            if (rowOf(view.tree(), strobe)->child(i)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LayerNode)
                layerRow = rowOf(view.tree(), strobe)->child(i);
        QScopedPointer<QMimeData> lm(mimeOf(layerRow));
        QVERIFY(lm->hasFormat(ShowTreeView::layerMimeType()));
        QVERIFY(!lm->hasFormat(ShowTreeView::cueMimeType()));
    }

    // The cue deleted: its hits move up to the production's Hits.
    QVERIFY(st->removeCue(one));
    QCOMPARE(st->hitScope(strobe), HitScope::Production);
    QCOMPARE(rowOf(view.tree(), strobe)->parent(), rowOf(view.tree(), st->looksSection(prod)));
}

void ShowTreeView_Test::looksSectionInTheTree()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("DanceHysteria");
    const quint32 list = st->addCueList(prod, "Full Show");
    st->addCue(list, -1, "One");

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // + Look on the production: the section and the look, one step.
    QVERIFY(view.selectNode(prod));
    const quint32 hit = view.newLook();
    QVERIFY(hit != invalidId());
    QVERIFY(st->isLook(hit));
    QCOMPARE(view.currentId(), hit);
    QCOMPARE(view.findChild<QToolButton *>("stNewCue")->text(), QString("+ Hit"));
    // Named on its page; groups dropped on it make a layer, as on a cue.
    QLineEdit *name = view.findChild<QLineEdit *>("stCueName");
    name->setText("Hit 1");
    emit name->editingFinished();
    QCOMPARE(st->cue(hit)->name, QString("Hit 1"));
    QVERIFY(view.dropTargets(hit, { towers->id() }, {}));
    const quint32 layer = st->cue(hit)->layers.first().id;
    QVERIFY(view.dropPalettes(hit, layer, { full }));
    QVERIFY(treeText(view.tree()).contains("  Hits [busking]"));
    QVERIFY(treeText(view.tree()).contains("    Hit 1"));
    // A look's page: no number, mode, includes or timing; who fires it.
    QVERIFY(view.selectNode(hit));
    QVERIFY(!view.findChild<QLineEdit *>("stCueNumber")->isVisibleTo(&view));
    QVERIFY(!view.findChild<QComboBox *>("stCueMode")->isVisibleTo(&view));
    QLabel *fired = view.findChild<QLabel *>("stLookFiredBy");
    QVERIFY(fired->isVisibleTo(&view));
    QVERIFY(fired->text().contains("no VC button yet"));
    // Its fade in / out (Q180), not a hold: typed, then the page moves on.
    QDoubleSpinBox *hitIn = view.findChild<QDoubleSpinBox *>("stCueFadeIn");
    QDoubleSpinBox *hitOut = view.findChild<QDoubleSpinBox *>("stCueFadeOut");
    QVERIFY(hitIn->isVisibleTo(&view));
    QVERIFY(hitOut->isVisibleTo(&view));
    QVERIFY(!view.findChild<QDoubleSpinBox *>("stCueHold")->isVisibleTo(&view));
    hitIn->setValue(1.5);
    hitOut->setValue(3);
    QVERIFY(view.selectNode(st->cueList(list)->cues.first().id));
    QCOMPARE(st->cue(hit)->fadeIn, 1500u);
    QCOMPARE(st->cue(hit)->fadeOut, 3000u);
    QCOMPARE(qobject_cast<Collection *>(m_doc->function(st->cue(hit)->collection))->hitFadeIn(), 1500u);
    QVERIFY(view.selectNode(hit));
    QCOMPARE(hitIn->value(), 1.5);
    QVERIFY(fired->text().contains("Make VC page"));
    // Its layer page has no Release (a look never passes anything on).
    QVERIFY(view.selectNode(layer, hit));
    QVERIFY(!view.findChild<QCheckBox *>("stLayerRelease")->isVisibleTo(&view));
    // Selected in Design, it plays on the rig.
    QCOMPARE(st->previewedCue(), hit);
    QCOMPARE(int(settleDmx(m_doc)[0]), 255);
    // A cue's page still has them all.
    QVERIFY(view.selectNode(st->cueList(list)->cues.first().id));
    QVERIFY(view.findChild<QLineEdit *>("stCueNumber")->isVisibleTo(&view));
    QVERIFY(!fired->isVisibleTo(&view));

    // Folders: new one on the section, a look dragged (dropped) onto it,
    // renamed in place, deleted (its hits move up).
    const quint32 section = st->looksSection(prod);
    QVERIFY(view.selectNode(section));
    QVERIFY(st->addLookFolder(section, "Hits"));
    QTreeWidgetItem *folderRow = nullptr;
    for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LookFolderNode)
            folderRow = *it;
    QVERIFY(folderRow != nullptr);
    QCOMPARE(view.lookFolderDropHint({ hit }, section, "Hits"), QString("Hit 1 → Hits"));
    QCOMPARE(view.lookFolderDropHint({ st->cueList(list)->cues.first().id }, section, "Hits"), QString());
    QVERIFY(view.dropLooksOnFolder({ hit }, section, "Hits"));
    QVERIFY(treeText(view.tree()).contains("      Hit 1"));            // under Looks › Hits
    view.tree()->setCurrentItem(nullptr);
    for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LookFolderNode)
            view.tree()->setCurrentItem(*it);
    QCOMPARE(view.currentLookFolder(), QString("Hits"));
    QVERIFY(view.findChild<QPushButton *>("stMakeVcPage")->isVisibleTo(&view));
    QVERIFY(!view.findChild<QComboBox *>("stListMode")->isVisibleTo(&view));
    // + Look in the folder.
    const quint32 hit2 = view.newLook();
    QCOMPARE(st->cue(hit2)->folder, QString("Hits"));
    for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LookFolderNode)
            view.tree()->setCurrentItem(*it);
    QVERIFY(view.startRename());
    QTreeWidgetItem *editing = view.tree()->currentItem();
    view.tree()->closePersistentEditor(editing, 0);
    editing->setText(0, "Stabs");
    QTRY_COMPARE(st->cue(hit)->folder, QString("Stabs"));
    for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LookFolderNode)
            view.tree()->setCurrentItem(*it);
    view.slotDelete();
    QCOMPARE(st->cue(hit)->folder, QString());
    QCOMPARE(st->cue(hit2)->folder, QString());
    QVERIFY(st->lookFolders(section).isEmpty());
    // The section can't be renamed; its looks can.
    QVERIFY(view.selectNode(section));
    QVERIFY(!view.startRename());
    // Copy to look: a cue's stage frozen as a look.
    const quint32 one = st->cueList(list)->cues.first().id;
    QVERIFY(view.dropTargets(one, { towers->id() }, {}));
    QVERIFY(view.dropPalettes(one, st->cue(one)->layers.first().id, { full }));
    const quint32 copied = st->copyCueToLook(st->cueList(list)->cues.first().id);
    QVERIFY(st->isLook(copied));
    QCOMPARE(st->cue(copied)->name, QString("One"));
    QCOMPARE(st->cue(copied)->layers.count(), 1);
}

void ShowTreeView_Test::makeVcPageAndAdoptAsLook()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 red = colour(Qt::red, "Red");
    const quint32 full = dimmer(255, "Full");
    Scene *old = new Scene(m_doc);
    old->setName("Old Busk");
    old->addFixtureGroup(towers->id());
    old->addPalette(full);
    m_doc->addFunction(old);
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 a = st->addLook(prod, "Red Hit", "Hits");
    QVERIFY(st->setLayerLook(a, st->addLayer(a, Target { { towers->id() }, {}, {}, 0xFFFFFFFF }),
                             st->lookForPalette(red)));
    const quint32 b = st->addLook(prod, "Blackout", "Hits");
    const quint32 c = st->addLook(prod, "Wash");
    const quint32 section = st->looksSection(prod);
    ScopedVc vc(m_doc);
    VCButton *busk = new VCButton(VirtualConsole::instance()->contents(), m_doc);
    VirtualConsole::instance()->placeWidget(busk, VirtualConsole::instance()->contents());
    busk->setCaption("Busk");
    busk->setFunction(old->id());

    {
        ShowTreeView view(nullptr, m_doc);
        view.resize(1200, 800);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        bool repoint = true;
        view.setConfirm([&](const QString &, const QString &, const QString &) { return repoint; });

        // Make VC page for the Hits folder: a frame, a Toggle button each.
        QString msg;
        QVERIFY(view.makeVcPage(section, "Hits", &msg));
        QVERIFY2(msg.contains("2 button"), qPrintable(msg));
        VCFrame *frame = nullptr;
        for (VCFrame *f : VirtualConsole::instance()->contents()->findChildren<VCFrame *>())
            if (f->caption() == "Hits — Hits")
                frame = f;
        QVERIFY(frame != nullptr);
        const QList<VCButton *> buttons = frame->findChildren<VCButton *>();
        QCOMPARE(buttons.count(), 2);
        QMap<QString, VCButton *> byCaption;
        for (VCButton *btn : buttons)
            byCaption.insert(btn->caption(), btn);
        QCOMPARE(byCaption.keys(), QStringList({ "Blackout", "Red Hit" }));
        QCOMPARE(byCaption.value("Red Hit")->function(), st->lookFunction(a));
        QCOMPARE(byCaption.value("Blackout")->function(), st->lookFunction(b));
        QCOMPARE(byCaption.value("Red Hit")->action(), VCButton::Toggle);
        QCOMPARE(byCaption.value("Red Hit")->backgroundColor(), QColor(Qt::red));
        // The look says who fires it; the tree shows it.
        QCOMPARE(view.lookFiredBy(a), QStringList({ "VC button “Red Hit”" }));
        QVERIFY(treeText(view.tree()).join("\n").contains("Red Hit [F VC ×1]"));
        // Pressed: it plays (red on the RGB-less dimmers is nothing, so a
        // look playing Full shows it).
        QVERIFY(st->setLayerLook(a, st->cue(a)->layers.first().id, st->lookForPalette(full)));
        QCOMPARE(byCaption.value("Red Hit")->function(), st->lookFunction(a));   // the same Collection
        // All looks: the section.
        QVERIFY(view.makeVcPage(section, QString(), &msg));
        QVERIFY2(msg.contains("3 button"), qPrintable(msg));
        Q_UNUSED(c)

        // Adopt as look: the scene's groups and palettes a layer; the VC
        // button firing it re-pointed (asked).
        QVERIFY(view.selectNode(section));
        const quint32 adopted = view.adoptAsLook(old->id());
        QVERIFY(adopted != invalidId());
        QCOMPARE(st->cue(adopted)->name, QString("Old Busk"));
        QCOMPARE(st->cue(adopted)->layers.first().looks.first().palettes, QList<quint32>({ full }));
        QCOMPARE(busk->function(), st->lookFunction(adopted));
        QVERIFY(m_doc->function(old->id()) != nullptr);
        // Not re-pointed when declined.
        busk->setFunction(old->id());
        repoint = false;
        const quint32 again = view.adoptAsLook(old->id());
        QVERIFY(again != invalidId());
        QCOMPARE(busk->function(), old->id());
        // The function picker sees the look by its name (visible, in its folder).
        Function *coll = m_doc->function(st->lookFunction(a));
        QVERIFY(coll->isVisible());
        QCOMPARE(coll->name(), QString("Red Hit"));
    }
}

void ShowTreeView_Test::looksAreOneUndoStepAndTyped()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    dimmer(255, "Full");
    Scene *old = new Scene(m_doc);
    old->setName("Old Busk");
    old->addFixtureGroup(towers->id());
    m_doc->addFunction(old);
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.setConfirm([](const QString &, const QString &, const QString &) { return true; });
    int walked = 0;
    auto oneStep = [&](const char *what, const std::function<bool()> &action) -> bool {
        const QString before = paletteState(m_doc);
        if (!action())
        {
            qWarning() << what << "did nothing";
            return false;
        }
        const QString after = paletteState(m_doc);
        if (after == before)
        {
            qWarning() << what << "changed nothing";
            return false;
        }
        if (!st->undoShared() || paletteState(m_doc) != before)
        {
            qWarning() << what << ": one undo didn't give back the state before it";
            return false;
        }
        if (!st->redoShared() || paletteState(m_doc) != after)
        {
            qWarning() << what << ": redo didn't give back the state after it";
            return false;
        }
        ++walked;
        return true;
    };
    QVERIFY(view.selectNode(prod));
    QVERIFY(oneStep("+ Look (and the section)", [&] { return view.newLook() != invalidId(); }));
    const quint32 section = st->looksSection(prod);
    const quint32 first = st->cueList(section)->cues.first().id;
    QVERIFY(oneStep("a group dropped on a look", [&] { return view.dropTargets(first, { towers->id() }, {}); }));
    QVERIFY(oneStep("New folder", [&] { return st->addLookFolder(section, "Hits"); }));
    QVERIFY(oneStep("a look onto a folder", [&] { return view.dropLooksOnFolder({ first }, section, "Hits"); }));
    QVERIFY(oneStep("Adopt as look", [&] { return view.adoptAsLook(old->id()) != invalidId(); }));
    QVERIFY(view.selectNode(prod));
    QVERIFY(oneStep("look typed", [&] { return view.runCommand("look \"Hit 2\" in \"Hits\""); }));
    // (undo + redo of the step before left the selection elsewhere)
    for (const Cue &c : st->cueList(section)->cues)
        if (c.name == "Hit 2")
            QVERIFY(view.selectNode(c.id));
    QVERIFY(oneStep("its layer typed", [&] { return view.runCommand("Towers dimmer Full"); }));
    QVERIFY(oneStep("adopt typed", [&] { return view.runCommand("adopt \"Old Busk\""); }));
    QCOMPARE(walked, 8);
    // vcpage with no Virtual Console: said, nothing made.
    QVERIFY(!view.runCommand("vcpage"));
    QVERIFY(view.commandMessage().contains("No Virtual Console"));
}

/* P5: the header's switcher sets the current production (what GO plays),
   the tree marks it and its main list, the production page sets its main
   list and timecode source (each one undo step), the context menu offers
   Make current / Make main list. */
void ShowTreeView_Test::theProductionSwitcherAndPage()
{
    FixtureGroup *a = group("A", 0, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 pa = st->addProduction("Dance");
    const quint32 la = st->addCueList(pa, "Full Show");
    const quint32 ca = st->addCue(la, -1, "One");
    Target t;
    t.groups << a->id();
    st->setLayerLook(ca, st->addLayer(ca, t), st->lookForPalette(full));
    const quint32 pb = st->addProduction("Corporate");
    const quint32 lb = st->addCueList(pb, "Keynote");
    st->addCue(lb, -1, "Walk-in");
    const quint32 lb2 = st->addCueList(pb, "Awards");
    st->addCue(lb2, -1, "Fanfare");
    // The first was made current as it was made: start from none.
    QCOMPARE(st->currentProduction(), pa);
    QVERIFY(st->setCurrentProduction(invalidId()));

    ShowTreeView view(nullptr, m_doc);
    QComboBox *sw = view.findChild<QComboBox *>("stProduction");
    QVERIFY(sw != nullptr);
    QCOMPARE(sw, view.productionSwitcher());
    QCOMPARE(sw->count(), 3);                       // none + two
    QCOMPARE(sw->currentIndex(), 0);

    // Choose Corporate: current, GO plays its first list.
    const int at = sw->findData(pb);
    sw->setCurrentIndex(at);
    emit sw->activated(at);
    QCOMPARE(st->currentProduction(), pb);
    QCOMPARE(m_doc->showControl()->mainCueList(), st->cueList(lb)->chaser);
    QTreeWidget *tree = view.tree();
    QTreeWidgetItem *prow = nullptr, *mainRow = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
    {
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::ProductionNode
            && (*it)->data(0, ShowTreeView::IdRole).toUInt() == pb)
            prow = *it;
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::ListNode
            && (*it)->data(0, ShowTreeView::IdRole).toUInt() == lb)
            mainRow = *it;
    }
    QVERIFY(prow != nullptr && mainRow != nullptr);
    QVERIFY(prow->text(1).contains("current"));
    QVERIFY(mainRow->text(1).contains("main"));

    // The production page.
    QVERIFY(view.selectNode(pb));
    QCOMPARE(view.findChild<QStackedWidget *>("stInspector")->currentWidget(),
             view.findChild<QPushButton *>("stProdCurrent")->parentWidget());
    QVERIFY(view.findChild<QPushButton *>("stProdCurrent")->text().contains("Current"));
    QComboBox *mainList = view.findChild<QComboBox *>("stProdMainList");
    QCOMPARE(mainList->count(), 2);
    const int awards = mainList->findData(lb2);
    mainList->setCurrentIndex(awards);
    emit mainList->activated(awards);
    QCOMPARE(st->mainList(pb), lb2);
    QCOMPARE(m_doc->showControl()->mainCueList(), st->cueList(lb2)->chaser);
    QCOMPARE(st->undoText(), QString("Main list"));

    QComboBox *tc = view.findChild<QComboBox *>("stProdTimecode");
    QCOMPARE(tc->itemData(0).toInt(), -2);          // the show's setting first
    const int any = tc->findData(-1);
    tc->setCurrentIndex(any);
    emit tc->activated(any);
    QCOMPARE(st->production(pb)->timecode, -1);
    QCOMPARE(m_doc->timecodeSource()->effectiveUniverse(), -1);
    QVERIFY(st->undo());
    QCOMPARE(st->production(pb)->timecode, -2);

    // Make current from the page; the switcher follows.
    QVERIFY(view.selectNode(pa));
    view.findChild<QPushButton *>("stProdCurrent")->click();
    QCOMPARE(st->currentProduction(), pa);
    QCOMPARE(sw->currentData().toUInt(), pa);

    // The context menu of a list of the other production offers Play on GO.
    {
        QStringList names;
        ModalCloser closer([&](QWidget *w) {
            if (QMenu *m = qobject_cast<QMenu *>(w))
                for (QAction *act : m->actions())
                    if (act->objectName() == QLatin1String("stPlayOnGo"))
                    {
                        names << act->objectName();
                        m->close();
                        act->trigger();
                        return true;
                    }
            return false;
        });
        QVERIFY(view.selectNode(lb));
        QTreeWidgetItem *row = tree->currentItem();
        QMetaObject::invokeMethod(&view, "slotTreeContextMenu",
                                  Q_ARG(QPoint, tree->visualItemRect(row).center()));
        QCOMPARE(names, QStringList({ "stPlayOnGo" }));
    }
    QCOMPARE(st->currentProduction(), pb);
    QCOMPARE(st->mainList(pb), lb);
    QCOMPARE(sw->currentData().toUInt(), pb);

    // Typed.
    QVERIFY(view.runCommand("production none"));
    QCOMPARE(st->currentProduction(), invalidId());
    QCOMPARE(sw->currentIndex(), 0);
}

/* P6 (§3C): a cue's sub-cues sit under it in the tree (↳ 2.1 · with GO,
   Sub GO, auto 0.5s), each a cue page of its own with Advance instead of a
   mode; + Sub-cue (header, context menu, the cue page's link) is one undo
   step each; what tracks from a sub-cue says "from sub-cue 2.1"; Design
   previews the selected sub-cue's full state. */
void ShowTreeView_Test::subCuesInTheTree()
{
    FixtureGroup *a = group("A", 0, 1);
    FixtureGroup *b = group("B", 1, 1);
    const quint32 half = dimmer(127, "Half");
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance");
    const quint32 list = st->addCueList(prod, "Song");
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    Target ta, tb;
    ta.groups << a->id();
    tb.groups << b->id();
    st->setTargetLook(c1, ta, st->lookForPalette(half));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    ModalCloser closer;

    // + Sub-cue with cue 1 selected: 1.1, one undo step.
    QVERIFY(view.selectNode(c1));
    QToolButton *plus = view.findChild<QToolButton *>("stNewSubCue");
    QVERIFY(plus != nullptr && plus->isEnabled());
    const QString before = treeState(m_doc);
    plus->click();
    const quint32 s1 = view.currentId();
    QVERIFY(st->isSubCue(s1));
    QCOMPARE(st->undoText(), QString("New sub-cue"));
    QVERIFY(st->undo());
    QCOMPARE(treeState(m_doc), before);
    QVERIFY(st->redo());
    QVERIFY(view.selectNode(s1));
    QCOMPARE(view.currentKind(), ShowTreeView::CueNode);
    // Its row: under cue 1, "↳ 1.1", with GO.
    QTreeWidgetItem *row = view.tree()->currentItem();
    QCOMPARE(row->parent()->data(0, ShowTreeView::IdRole).toUInt(), c1);
    QVERIFY(row->text(0).startsWith(QString::fromUtf8("↳ 1.1")));
    QVERIFY(row->text(1).contains("with GO"));

    // Another from the selected sub-cue: after it.
    plus->click();
    const quint32 s2 = view.currentId();
    QVERIFY(s2 != s1 && st->isSubCue(s2));
    const CueList *subs = st->cueList(st->subCueList(c1));
    QCOMPARE(subs->cues.count(), 2);
    QCOMPARE(subs->cues.at(1).id, s2);
    QVERIFY(st->setTargetLook(s1, tb, st->lookForPalette(full)));

    // The page: Advance (not mode / base / hold); Auto-follow is one step.
    QVERIFY(view.selectNode(s2));
    QComboBox *adv = view.findChild<QComboBox *>("stSubCueAdvance");
    QVERIFY(adv->isVisibleTo(&view));
    QVERIFY(!view.findChild<QComboBox *>("stCueMode")->isVisibleTo(&view));
    QDoubleSpinBox *wait = view.findChild<QDoubleSpinBox *>("stSubCueWait");
    wait->setValue(0.5);
    adv->setCurrentIndex(1);
    emit adv->activated(1);
    QVERIFY(st->cue(s2)->follow);
    QCOMPARE(st->cue(s2)->wait, 500u);
    QCOMPARE(st->undoText(), QString("Auto-follow"));
    QTreeWidgetItem *r2 = view.tree()->currentItem();
    QVERIFY(r2->text(1).contains("auto 0.5s"));
    // What 1.2 carries from 1.1: "from sub-cue 1.1"; from cue 1: tracked.
    bool fromSub = false, fromCue = false;
    for (int i = 0; i < r2->childCount(); ++i)
    {
        fromSub = fromSub || (r2->child(i)->text(1) == QString("↳ 1.1")
                              && r2->child(i)->toolTip(1).contains("from sub-cue 1.1"));
        fromCue = fromCue || (r2->child(i)->text(1) == QString("1")
                              && r2->child(i)->toolTip(1).contains("tracked from 1 One"));
    }
    QVERIFY(fromSub);
    QVERIFY(fromCue);

    // A main cue's page lists its sub-cues; + Cue from a sub-cue makes a
    // main cue after its cue.
    QVERIFY(view.selectNode(c1));
    QLabel *line = view.findChild<QLabel *>("stCueSubCues");
    QVERIFY(line->isVisibleTo(&view));
    QVERIFY(line->text().contains("1.1"));
    QVERIFY(view.selectNode(s2));
    view.findChild<QToolButton *>("stNewCue")->click();
    const quint32 made = view.currentId();
    QVERIFY(!st->isSubCue(made));
    QCOMPARE(st->listOfCue(made)->id, list);
    QCOMPARE(st->cueList(list)->indexOf(made), 1);
    QCOMPARE(st->cueList(list)->indexOf(c2), 2);

    // Design: selecting a sub-cue previews its full state (A from cue 1, B).
    m_doc->setMode(Doc::Design);
    QVERIFY(view.selectNode(s1));
    QCOMPARE(st->previewedCue(), s1);
    const QHash<quint32, uchar> dmx = settleDmx(m_doc);
    QCOMPARE(int(dmx.value(0)), 127);
    QCOMPARE(int(dmx.value(1)), 255);
    m_doc->setMode(Doc::Operate);

    // Typed, and help.
    QVERIFY(view.runCommand("sub \"Strobe\" subgo"));
    QCOMPARE(st->cueList(st->subCueList(c1))->cues.count(), 3);
    QVERIFY(view.runCommand("help sub"));
    view.hide();
}

/****************************************************************************
 * Tracking in the tree: a badge, details on hover (2026-10-21)
 ****************************************************************************/

namespace
{
QTreeWidgetItem *cueRow(QTreeWidget *tree, quint32 cueId)
{
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
            && (*it)->data(0, ShowTreeView::IdRole).toUInt() == cueId)
            return *it;
    return nullptr;
}
}

/* Branson: "when the tree starts printing details of what's being tracked
   it overflows over the tree so you can't read everything about the
   layers". A cue row counts what it sets (●), releases (○) and what
   tracks in (⤳); a carried-in row says only its cue's number; the rest is
   in the tooltip and on the cue page. However long the names, the layer
   names keep their column. */
void ShowTreeView_Test::trackingIsABadgeWithDetailsOnHover()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    FixtureGroup *spots = group("Spots", 4, 2);
    const quint32 blue = colour(Qt::blue, "Blue");
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const QString longName = QString("Opening with every wash, spot and UST in the rig set by hand for the long "
                                     "walk-in before the house opens");
    const quint32 c1 = st->addCue(list, -1, longName);
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 c3 = st->addCue(list, 1, "Three");
    const quint32 t1 = st->addLayer(c1, groupTarget(towers->id()));
    st->setLayerLook(c1, t1, st->lookForPalette(blue));
    st->setLayerLook(c1, t1, st->lookForPalette(half));
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(spots->id())), st->lookForPalette(half));
    st->setLayerLook(c2, st->addLayer(c2, groupTarget(usts->id())), st->lookForPalette(half));
    const quint32 rel = st->addLayer(c2, groupTarget(spots->id()));
    QVERIFY(st->setLayerRelease(c2, rel, true));
    QVERIFY(st->setCueMode(c3, CueMode::Block));

    QSettings().remove("showtree/splitter2");
    QSettings().remove("showtree/badgeColumn2");
    ShowTreeView view(nullptr, m_doc);
    view.resize(1280, 500);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.tree()->expandAll();
    QTest::qWait(20);

    // The kinds: set here (S), released (R), tracked in (T), a Block cue (M: B).
    // Q104: a column each, headed by what it is.
    QCOMPARE(cueRow(view.tree(), c1)->text(ShowTreeView::SetsCol), QString("2"));
    QCOMPARE(cueRow(view.tree(), c2)->text(ShowTreeView::SetsCol), QString("1"));
    QCOMPARE(cueRow(view.tree(), c2)->text(ShowTreeView::ReleasesCol), QString("1"));
    QCOMPARE(cueRow(view.tree(), c2)->text(ShowTreeView::TracksCol), QString("1"));
    QCOMPARE(cueRow(view.tree(), c3)->text(ShowTreeView::ModeCol), QString("B"));
    QVERIFY(cueRow(view.tree(), c3)->text(ShowTreeView::TracksCol).isEmpty());
    QCOMPARE(view.tree()->headerItem()->text(ShowTreeView::TracksCol), QString("T"));
    QVERIFY(cueRow(view.tree(), c3)->toolTip(1).contains("Mode B (Block)"));
    QTreeWidgetItem *carried = trackedRow(view.tree(), c2, "Towers");
    QVERIFY(carried != nullptr);
    QCOMPARE(carried->text(0), QString("⤳ Towers"));
    QCOMPARE(carried->text(1), QString("1"));
    bool released = false;
    for (int i = 0; i < cueRow(view.tree(), c2)->childCount(); ++i)
        released = released || cueRow(view.tree(), c2)->child(i)->text(ShowTreeView::ReleasesCol) == QString("\u25cf");
    QVERIFY(released);

    // On hover: the group, each attribute's value, and the cue it came from.
    const QString rowTip = carried->toolTip(1);
    QVERIFY2(rowTip.startsWith(QString("⤳ Towers — tracked from 1 %1").arg(longName)), qPrintable(rowTip));
    QVERIFY2(rowTip.contains("    Colour: Blue"), qPrintable(rowTip));
    QVERIFY2(rowTip.contains("    Dimmer: Half"), qPrintable(rowTip));
    const QString cueTip = cueRow(view.tree(), c2)->toolTip(1);
    QVERIFY2(cueTip.contains("Tracked in (1):\n⤳ Towers — tracked from 1 "), qPrintable(cueTip));
    QVERIFY2(cueTip.contains("Sets (1): USTs\nReleases (1): Spots"), qPrintable(cueTip));
    for (const QString &old : { QString("●"), QString("○"), QString("⊘") })
        QVERIFY2(!cueTip.contains(old), qPrintable(cueTip));
    QVERIFY2(cueTip.contains("Next cue receives:\n    nothing: 3 Three is a Block cue"), qPrintable(cueTip));
    QCOMPARE(cueRow(view.tree(), c2)->toolTip(0), cueTip);      // the row's name too
    QCOMPARE(view.cueTrackingTip(c2), cueTip);

    // The cue page lists it in full.
    QVERIFY(view.selectNode(c2));
    QVERIFY(view.cueTrackedText().contains("⤳ Towers — tracked from 1 "));
    QVERIFY(view.cueTrackedText().contains("    Dimmer: Half"));
    QLabel *page = view.findChild<QLabel *>("stCueTracked");
    QVERIFY(page != nullptr && page->isVisibleTo(&view));
    QVERIFY(page->text().contains("    Colour: Blue"));

    // However long the text, the names keep their column: the badge
    // column stays narrow, nothing runs past the pane, and every layer's
    // name fits beside it (a long cue name ends in "…" within its own).
    QTreeWidget *tree = view.tree();
    const int room = tree->viewport()->width();
    int flagCols = 0;
    for (int c = ShowTreeView::SetsCol; c < ShowTreeView::TreeColumnCount; ++c)
        if (!tree->header()->isSectionHidden(c))
            flagCols += tree->header()->sectionSize(c);
    QVERIFY(tree->header()->sectionSize(0) >= (room - flagCols) * 3 / 5 - 1);
    QVERIFY(tree->header()->sectionSize(0) + tree->header()->sectionSize(1) + flagCols <= room);
    QVERIFY(!tree->horizontalScrollBar()->isVisible());
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
    {
        const QRect name = tree->visualItemRect(*it).intersected(
            QRect(tree->header()->sectionViewportPosition(0), 0, tree->header()->sectionSize(0), 1 << 20));
        const QRect badge = tree->visualItemRect(*it).intersected(
            QRect(tree->header()->sectionViewportPosition(1), 0, tree->header()->sectionSize(1), 1 << 20));
        QVERIFY(!name.intersects(badge));
        const int kind = (*it)->data(0, ShowTreeView::KindRole).toInt();
        if (kind == ShowTreeView::LayerNode || kind == ShowTreeView::TrackedLayerNode)
            QVERIFY2(tree->fontMetrics().horizontalAdvance((*it)->text(0)) < name.width() - tree->indentation(),
                     qPrintable((*it)->text(0)));
        // The badge text fits its column, or ends in "…" there: never wider.
        QVERIFY(tree->header()->sectionSize(1) < room / 2);
    }
    QVERIFY(tree->fontMetrics().horizontalAdvance(cueRow(tree, c1)->text(0)) > tree->header()->sectionSize(0));
    QCOMPARE(tree->textElideMode(), Qt::ElideRight);
    view.hide();
}

/* The Library | Tree | Editor splitter is remembered; so is the tree's
   badge column, set by dragging the line between its columns, and kept as
   the pane is resized (the names take the change). */
void ShowTreeView_Test::theTreeColumnsResizeAndAreRemembered()
{
    QSettings settings;
    settings.remove("showtree/badgeColumn2");
    ShowTreeController *st = m_doc->showTree();
    st->addCue(st->addCueList(st->addProduction("P"), "L", ListMode::Tracking), -1, "One");
    {
        ShowTreeView view(nullptr, m_doc);
        view.resize(1600, 500);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QSplitter *split = view.findChild<QSplitter *>("stSplitter");
        QVERIFY(split != nullptr && split->widget(1) == view.tree());
        QVERIFY(!split->widget(1)->isHidden());
        QHeaderView *header = view.tree()->header();
        QCOMPARE(header->sectionResizeMode(0), QHeaderView::Interactive);
        QCOMPARE(header->sectionResizeMode(1), QHeaderView::Interactive);

        // Dragging the line between the first two columns (a user resize);
        // the flag columns (Q104) keep their width.
        int flags = 0;
        for (int c = ShowTreeView::SetsCol; c < ShowTreeView::TreeColumnCount; ++c)
            flags += header->sectionSize(c);
        const int room = view.tree()->viewport()->width();
        header->resizeSection(0, room - flags - 70);
        QCOMPARE(view.badgeColumnWidth(), 70);
        QCOMPARE(settings.value("showtree/badgeColumn2").toInt(), 70);
        QCOMPARE(header->sectionSize(1), 70);

        // A wider pane: the names take it, the badges keep their width.
        QList<int> sizes = split->sizes();
        sizes[1] += 120;
        sizes[2] -= 120;
        split->setSizes(sizes);
        QTest::qWait(20);
        QCOMPARE(header->sectionSize(1), 70);
        int flagsNow = 0;           // a wider pane can give the flag columns their full width
        for (int c = ShowTreeView::SetsCol; c < ShowTreeView::TreeColumnCount; ++c)
            flagsNow += header->sectionSize(c);
        QCOMPARE(header->sectionSize(0), view.tree()->viewport()->width() - flagsNow - 70);
        view.hide();
    }
    // A new view (the next start): as it was left.
    ShowTreeView again(nullptr, m_doc);
    again.resize(1600, 500);
    again.show();
    QVERIFY(QTest::qWaitForWindowExposed(&again));
    QCOMPARE(again.badgeColumnWidth(), 70);
    QCOMPARE(again.tree()->header()->sectionSize(1), 70);
    again.hide();
    settings.remove("showtree/badgeColumn2");
}

/****************************************************************************
 * Skipped cues (2026-10-21)
 ****************************************************************************/

/* Right-click @a row (selected as a click would leave it) and trigger the
   action named @a name, in a submenu too. Its text then ("" not offered);
   @a enabled / @a checked as it was offered. */
static QString contextAction(ShowTreeView &view, QTreeWidgetItem *row, const QString &name,
                             bool *checked = nullptr, bool *enabled = nullptr)
{
    QString seen;
    ModalCloser closer([&](QWidget *w) {
        QMenu *m = qobject_cast<QMenu *>(w);
        if (m == nullptr)
            return false;
        for (QAction *act : m->findChildren<QAction *>())
            if (act->objectName() == name)
            {
                seen = act->text();
                if (checked != nullptr)
                    *checked = act->isChecked();
                if (enabled != nullptr)
                    *enabled = act->isEnabled();
                m->close();
                if (act->isEnabled())
                    act->trigger();
                return true;
            }
        m->close();
        return true;
    });
    QTreeWidget *tree = view.tree();
    QMetaObject::invokeMethod(&view, "slotTreeContextMenu", Q_ARG(QPoint, tree->visualItemRect(row).center()));
    return seen;
}

/* Branson: "in the show-tree we need a way to disable cues inline and skip
   them" -- then (2026-10-29) "skipping cues should be a right click menu off
   of a selected cue or set of cues ... the checkbox is overmuch". The menu's
   Skip cue / Unskip cue, ⌘/, the cue page's Skip and the command line --
   each one undo step, on every selected cue; a skipped cue greyed, struck
   through and marked, its layers greyed, and what the next cue receives
   said past it. No tick on the rows. */
void ShowTreeView_Test::skipCuesInTheTree()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    const quint32 blue = colour(Qt::blue, "Blue");
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    QList<quint32> c;
    for (int i = 0; i < 4; ++i)
        c << st->addCue(list, i - 1, QString("C%1").arg(i + 1));
    st->setLayerLook(c[0], st->addLayer(c[0], groupTarget(towers->id())), st->lookForPalette(blue));
    st->setLayerLook(c[1], st->addLayer(c[1], groupTarget(usts->id())), st->lookForPalette(half));
    st->setLayerLook(c[2], st->addLayer(c[2], groupTarget(towers->id())), st->lookForPalette(half));
    st->clearUndo();

    ShowTreeView view(nullptr, m_doc);
    view.resize(1280, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    tree->expandAll();
    // No cue row has a tick, and none is marked.
    for (quint32 id : c)
    {
        QTreeWidgetItem *row = cueRow(tree, id);
        QVERIFY(row != nullptr);
        QVERIFY(!row->data(0, Qt::CheckStateRole).isValid());
        QVERIFY(row->icon(0).isNull());
    }

    // Right-click ▸ Skip cue: skipped, one undo step.
    QVERIFY(view.selectNode(c[1]));
    QCOMPARE(contextAction(view, cueRow(tree, c[1]), "stSkipCue"), QString("Skip cue"));
    QVERIFY(st->isCueSkipped(c[1]));
    QCOMPARE(st->undoText(), QString("Skip cue"));
    tree->expandAll();
    QTreeWidgetItem *row = cueRow(tree, c[1]);
    QVERIFY(!row->data(0, Qt::CheckStateRole).isValid());
    QVERIFY(!row->icon(0).isNull());                    // the skip mark
    QVERIFY(cueRow(tree, c[0])->icon(0).isNull());
    QVERIFY(row->text(ShowTreeView::FlagsCol).startsWith("skip"));
    QVERIFY(row->font(0).strikeOut());
    QVERIFY(row->toolTip(0).contains("Right-click"));
    const QColor skipGrey = row->foreground(0).color();
    QVERIFY(skipGrey != tree->palette().color(QPalette::Text));
    QVERIFY(row->toolTip(0).startsWith("Skipped: GO passes over it"));
    QVERIFY(row->toolTip(1).startsWith("Skipped: GO passes over it"));
    QVERIFY(row->childCount() > 0);
    for (int k = 0; k < row->childCount(); ++k)
        QCOMPARE(row->child(k)->foreground(0).color(), skipGrey);
    QVERIFY(!cueRow(tree, c[0])->font(0).strikeOut());
    // What the next cue receives -- and the cue before, past it.
    QVERIFY2(view.nextCueText(c[0]).startsWith("(3 C3, past the skipped) "), qPrintable(view.nextCueText(c[0])));
    QVERIFY2(view.nextCueText(c[1]).contains("nothing of this cue (skipped): what 1 C1 passes on"),
             qPrintable(view.nextCueText(c[1])));
    QVERIFY(view.cueTrackingTip(c[1]).contains("Skipped: below is what a jump to it plays"));
    // 3 tracks from 1, not from 2: the USTs never come in.
    QVERIFY(trackedRow(tree, c[2], "USTs") == nullptr);
    QVERIFY(st->undo());
    QVERIFY(!st->isCueSkipped(c[1]));
    tree->expandAll();
    QVERIFY(trackedRow(tree, c[2], "USTs") != nullptr);

    // Several selected, the menu on one of them: all of them, one step.
    tree->clearSelection();
    cueRow(tree, c[1])->setSelected(true);
    cueRow(tree, c[2])->setSelected(true);
    QCOMPARE(contextAction(view, cueRow(tree, c[2]), "stSkipCue"), QString("Skip cue (2 cues)"));
    QVERIFY(st->isCueSkipped(c[1]) && st->isCueSkipped(c[2]));
    QCOMPARE(st->undoText(), QString("Skip 2 cues"));
    // Every one skipped: the menu puts them back, one step.
    tree->clearSelection();
    cueRow(tree, c[1])->setSelected(true);
    cueRow(tree, c[2])->setSelected(true);
    QCOMPARE(contextAction(view, cueRow(tree, c[1]), "stSkipCue"), QString("Unskip cue (2 cues)"));
    QVERIFY(!st->isCueSkipped(c[1]) && !st->isCueSkipped(c[2]));
    QCOMPARE(st->undoText(), QString("Unskip 2 cues"));
    // One of them skipped: Skip (as ⌘/ does).
    QVERIFY(st->setCuesSkipped({ c[1] }, true));
    tree->clearSelection();
    cueRow(tree, c[1])->setSelected(true);
    cueRow(tree, c[2])->setSelected(true);
    QCOMPARE(contextAction(view, cueRow(tree, c[1]), "stSkipCue"), QString("Skip cue (2 cues)"));
    QVERIFY(st->isCueSkipped(c[1]) && st->isCueSkipped(c[2]));

    // ⌘/ on the selection: every one skipped already -- put back.
    tree->setFocus();
    tree->clearSelection();
    cueRow(tree, c[1])->setSelected(true);
    cueRow(tree, c[2])->setSelected(true);
    QTest::keyClick(tree, Qt::Key_Slash, Qt::ControlModifier);
    QVERIFY(!st->isCueSkipped(c[1]) && !st->isCueSkipped(c[2]));
    QCOMPARE(st->undoText(), QString("Unskip 2 cues"));
    // ... one of them not: all skipped.
    tree->clearSelection();
    cueRow(tree, c[0])->setSelected(true);
    cueRow(tree, c[3])->setSelected(true);
    QVERIFY(st->setCuesSkipped({ c[3] }, true));
    tree->clearSelection();
    cueRow(tree, c[0])->setSelected(true);
    cueRow(tree, c[3])->setSelected(true);
    QTest::keyClick(tree, Qt::Key_Slash, Qt::ControlModifier);
    QVERIFY(st->isCueSkipped(c[0]) && st->isCueSkipped(c[3]));
    QVERIFY(view.setCuesSkipped({ c[0], c[3] }, false));
    // A plain / still goes to the Library's filter.
    QVERIFY(view.selectNode(c[0]));
    QTest::keyClick(tree, Qt::Key_Slash);
    QVERIFY(!st->isCueSkipped(c[0]));

    // Right-click: Skip cue, then Unskip cue.
    for (const bool skipped : { false, true })
    {
        QVERIFY(view.selectNode(c[2]));
        QCOMPARE(contextAction(view, cueRow(tree, c[2]), "stSkipCue"),
                 skipped ? QString("Unskip cue") : QString("Skip cue"));
        QCOMPARE(st->isCueSkipped(c[2]), !skipped);
    }

    // The cue page's Skip.
    QVERIFY(view.selectNode(c[3]));
    QCheckBox *skipBox = view.findChild<QCheckBox *>("stCueSkip");
    QVERIFY(skipBox != nullptr && skipBox->isVisibleTo(&view));
    QVERIFY(!skipBox->isChecked());
    skipBox->setChecked(true);
    QVERIFY(st->isCueSkipped(c[3]));
    QVERIFY(view.selectNode(c[3]));
    QVERIFY(view.findChild<QCheckBox *>("stCueSkip")->isChecked());
    QVERIFY(st->undo());

    // The command line.
    QVERIFY(view.runCommand("skip 2 thru 3"));
    QVERIFY(st->isCueSkipped(c[1]) && st->isCueSkipped(c[2]));
    QCOMPARE(st->undoText(), QString("skip 2 thru 3"));
    QVERIFY(view.commandMessage().contains("skipped 2 cues"));
    QVERIFY(view.runCommand("unskip 2, 3"));
    QVERIFY(!st->isCueSkipped(c[1]) && !st->isCueSkipped(c[2]));

    // Picked, a skipped cue still plays (the preview says so).
    QVERIFY(st->setCuesSkipped({ c[1] }, true));
    QVERIFY(view.selectNode(c[1]));
    QTest::qWait(20);
    QVERIFY2(view.previewText().contains("skipped"), qPrintable(view.previewText()));

    // A busking look has no tick: it is fired, not GOne through.
    const quint32 look = st->addLook(st->productions().first().id, "Hit");
    QVERIFY(look != invalidId());
    QTreeWidgetItem *lookRow = cueRow(tree, look);
    QVERIFY(lookRow != nullptr);
    QVERIFY(!lookRow->data(0, Qt::CheckStateRole).isValid());
    view.hide();
}

/* Branson, 2026-10-29: "the second column in show tree says Tracking; it
   could be a flagged column to save space". A list's mode is a flag in M
   (⇢ tracking, B non-tracking) with its words on hover; the info column
   keeps only what it has to say (main, MIB). */
void ShowTreeView_Test::listModeIsAFlag()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 tracking = st->addCueList(prod, "T", ListMode::Tracking);
    const quint32 plain = st->addCueList(prod, "N", ListMode::NonTracking);
    st->addCue(tracking, -1, "One");
    st->addCue(plain, -1, "One");

    ShowTreeView view(nullptr, m_doc);
    view.resize(1000, 500);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    QTreeWidgetItem *t = rowOf(tree, ShowTreeView::ListNode, tracking);
    QTreeWidgetItem *n = rowOf(tree, ShowTreeView::ListNode, plain);
    QVERIFY(t != nullptr && n != nullptr);
    QCOMPARE(t->text(ShowTreeView::ModeCol), QString::fromUtf8("⇢"));
    QCOMPARE(n->text(ShowTreeView::ModeCol), QString("B"));
    for (QTreeWidgetItem *row : { t, n })
        QVERIFY2(!row->text(ShowTreeView::InfoCol).contains("racking"), qPrintable(row->text(ShowTreeView::InfoCol)));
    QVERIFY(t->toolTip(ShowTreeView::ModeCol).startsWith("Tracking list"));
    QVERIFY(n->toolTip(ShowTreeView::ModeCol).startsWith("Non-tracking list"));
    // Said on the name too: the M column can be hidden.
    QVERIFY(t->toolTip(0).startsWith("Tracking list"));
    QVERIFY(tree->headerItem()->toolTip(ShowTreeView::ModeCol).contains("A list:"));
    // Two lists: "main" says which one GO plays, short.
    QCOMPARE(t->text(ShowTreeView::InfoCol), QString("main"));
    QVERIFY(n->text(ShowTreeView::InfoCol).isEmpty());
    // Its mode changed: the flag with it.
    QVERIFY(st->setListMode(plain, ListMode::Tracking));
    QCOMPARE(rowOf(tree, ShowTreeView::ListNode, plain)->text(ShowTreeView::ModeCol), QString::fromUtf8("⇢"));
    // The info column starts narrower than "Tracking · main" was.
    QVERIFY(tree->columnWidth(ShowTreeView::InfoCol)
            < tree->fontMetrics().horizontalAdvance(QString::fromUtf8("Tracking · main  ▸ MIB")));
    view.hide();
}

/* Branson, 2026-10-29: "how do I release a scene on a cue, I don't seem to
   be able to make it non tracking". A layer that plays a scene as it is (▣,
   + Scene) releases as any other: the page's Release, the tree's and the
   Workbench's layer menus, on its own layer or the one carried in -- and
   the cue after plays without it. */
void ShowTreeView_Test::releaseAFunctionLayer()
{
    Scene *fn = new Scene(m_doc);
    fn->setName("Haze look");
    m_doc->addFunction(fn);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 c3 = st->addCue(list, 1, "Three");
    Target ft;
    ft.function = fn->id();
    const quint32 l1 = st->addLayer(c1, ft);
    QVERIFY(l1 != invalidId());
    st->clearUndo();
    auto plays = [&](quint32 cue) {
        const Collection *coll = qobject_cast<const Collection *>(m_doc->function(st->cue(cue)->collection));
        return coll != nullptr && coll->functions().contains(fn->id());
    };
    auto onStage = [&](int at) {
        return resolve(*st->cueList(list)).at(at).layer(ft) != nullptr;
    };
    QVERIFY(onStage(0) && onStage(1) && onStage(2));
    QVERIFY(plays(c1) && plays(c2) && plays(c3));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1280, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    tree->expandAll();

    // Its own layer: the page's Release is there, and works.
    QVERIFY(view.selectNode(l1, c1));
    QCheckBox *release = view.findChild<QCheckBox *>("stLayerRelease");
    QVERIFY(release != nullptr);
    QVERIFY(release->isVisibleTo(&view));
    release->setChecked(true);
    QVERIFY(st->cue(c1)->layer(l1)->release);
    QCOMPARE(st->undoText(), QString("Release layer"));
    QVERIFY(onStage(0) && !onStage(1) && !onStage(2));
    QVERIFY(plays(c1) && !plays(c2) && !plays(c3));
    QVERIFY(st->undo());
    QVERIFY(onStage(2));

    // The tree's menu on the carried-in row in 2: 2 is the last cue it plays.
    tree->expandAll();
    QTreeWidgetItem *carried = rowOf(tree, ShowTreeView::TrackedLayerNode, l1, c2);
    QVERIFY(carried != nullptr);
    bool checked = true;
    QCOMPARE(contextAction(view, carried, "stLayerReleaseMenu", &checked), QString("Release (leaving this cue)"));
    QVERIFY(!checked);
    QCOMPARE(st->cue(c2)->layers.count(), 1);
    QVERIFY(st->cue(c2)->layers.first().release);
    QVERIFY(st->cue(c2)->layers.first().target.isOpaque());
    QVERIFY(onStage(1) && !onStage(2));
    QVERIFY(plays(c2) && !plays(c3));
    QCOMPARE(view.currentKind(), ShowTreeView::LayerNode);   // the new own layer, selected
    // ... and off again, from its own row's menu.
    tree->expandAll();
    const quint32 l2 = st->cue(c2)->layers.first().id;
    QCOMPARE(contextAction(view, rowOf(tree, ShowTreeView::LayerNode, l2, c2), "stLayerReleaseMenu", &checked),
             QString("Release (leaving this cue)"));
    QVERIFY(checked);
    QVERIFY(!st->cue(c2)->layer(l2)->release);
    QVERIFY(onStage(2));
    // Both undone: 2 holds nothing of its own again.
    QVERIFY(st->undo());
    QVERIFY(st->undo());
    QVERIFY(st->cue(c2)->layers.isEmpty());

    // The Workbench's layer menu (the cue's stack): Release on the ▣ row.
    {
        TreeCueStack stack(m_doc, &view, c1);
        int at = -1;
        const QList<WorkbenchStack::Layer> ls = stack.layers();
        for (int i = 0; i < ls.size(); ++i)
            if (ls.at(i).layerId == l1)
                at = i;
        QVERIFY(at >= 0);
        QString seen;
        ModalCloser closer([&](QWidget *w) {
            QMenu *m = qobject_cast<QMenu *>(w);
            if (m == nullptr)
                return false;
            QAction *a = m->findChild<QAction *>("cueStackRelease");
            seen = a != nullptr ? a->text() : QString("(none)");
            m->close();
            if (a != nullptr)
                a->trigger();
            return true;
        });
        QVERIFY(stack.showMenu(at, -1, QPoint(10, 10)));
        QCOMPARE(seen, QString("Release (leaving this cue)"));
        QVERIFY(st->cue(c1)->layer(l1)->release);
        QVERIFY(!onStage(1));
    }
    view.hide();
}

/* The same ask: a cue's Mode (Track / Block / Cue-only) from the tree's cue
   menu, on the selected cues in one step, and from the Workbench's layer
   menu -- on a cue whose layer plays a scene as it is. */
void ShowTreeView_Test::cueModeFromTheMenus()
{
    Scene *fn = new Scene(m_doc);
    fn->setName("Haze look");
    m_doc->addFunction(fn);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 c3 = st->addCue(list, 1, "Three");
    Target ft;
    ft.function = fn->id();
    const quint32 l1 = st->addLayer(c1, ft);
    st->clearUndo();
    auto onStage = [&](int at) {
        return resolve(*st->cueList(list)).at(at).layer(ft) != nullptr;
    };
    QVERIFY(onStage(1) && onStage(2));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1280, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    tree->expandAll();

    // Cue-only on 1: the scene plays in 1 and goes with it.
    QVERIFY(view.selectNode(c1));
    QCOMPARE(contextAction(view, rowOf(tree, ShowTreeView::CueNode, c1), "stCueMode3"), QString("Cue-only"));
    QCOMPARE(st->cue(c1)->mode, CueMode::CueOnly);
    QVERIFY(onStage(0) && !onStage(1) && !onStage(2));
    QCOMPARE(rowOf(tree, ShowTreeView::CueNode, c1)->text(ShowTreeView::ModeCol), QString("C"));
    bool checked = false;
    QCOMPARE(contextAction(view, rowOf(tree, ShowTreeView::CueNode, c1), "stCueMode3", &checked), QString("Cue-only"));
    QVERIFY(checked);
    QVERIFY(st->undo());
    QVERIFY(onStage(2));

    // Block on 2 and 3 together: one step; neither starts from 1.
    tree->clearSelection();
    rowOf(tree, ShowTreeView::CueNode, c2)->setSelected(true);
    rowOf(tree, ShowTreeView::CueNode, c3)->setSelected(true);
    QCOMPARE(contextAction(view, rowOf(tree, ShowTreeView::CueNode, c3), "stCueMode2"), QString("Block"));
    QCOMPARE(st->cue(c2)->mode, CueMode::Block);
    QCOMPARE(st->cue(c3)->mode, CueMode::Block);
    QCOMPARE(st->undoText(), QString("Cue mode (2 cues)"));
    QVERIFY(onStage(0) && !onStage(1) && !onStage(2));
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c2)->mode, CueMode::ListDefault);
    QCOMPARE(st->cue(c3)->mode, CueMode::ListDefault);

    // The Workbench's layer menu: Cue mode ▸ Cue-only, for its cue.
    {
        TreeCueStack stack(m_doc, &view, c1);
        int at = -1;
        const QList<WorkbenchStack::Layer> ls = stack.layers();
        for (int i = 0; i < ls.size(); ++i)
            if (ls.at(i).layerId == l1)
                at = i;
        QVERIFY(at >= 0);
        bool offered = false;
        ModalCloser closer([&](QWidget *w) {
            QMenu *m = qobject_cast<QMenu *>(w);
            if (m == nullptr)
                return false;
            offered = m->findChild<QMenu *>("cueStackMode") != nullptr;
            QAction *a = m->findChild<QAction *>("stCueMode3");
            m->close();
            if (a != nullptr)
                a->trigger();
            return true;
        });
        QVERIFY(stack.showMenu(at, -1, QPoint(10, 10)));
        QVERIFY(offered);
        QCOMPARE(st->cue(c1)->mode, CueMode::CueOnly);
        QVERIFY(!onStage(1));
    }
    view.hide();
}

/* A real show (read only) with cues skipped: the tree, and the skipped
   cue's row tooltip as text beside it. */
void ShowTreeView_Test::pictureOfSkippedCues()
{
    const QString path = qEnvironmentVariable("QLC_SKIPCUE_SHOW");
    const QString shot = qEnvironmentVariable("QLC_SKIPCUE_SHOT");
    if (path.isEmpty() || shot.isEmpty())
        QSKIP("set QLC_SKIPCUE_SHOW and QLC_SKIPCUE_SHOT to picture a show with cues skipped");

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader r(&file);
    bool loaded = false;
    while (!r.atEnd() && !loaded)
        if (r.readNextStartElement() && r.name() == QLatin1String("Engine"))
            loaded = m_doc->loadXML(r, false);
    QVERIFY(loaded);
    ShowTreeController *st = m_doc->showTree();
    auto longest = [st]() {
        const CueList *list = nullptr;
        for (const Production &p : st->productions())
            for (const CueList &l : p.lists)
                if (!l.looks && !l.isSubCues() && (list == nullptr || l.cues.count() > list->cues.count()))
                    list = &l;
        return list;
    };
    const CueList *list = longest();
    if (list == nullptr || list->cues.count() < 6)
    {
        // Too short a tree: the show's chaser (QLC_SKIPCUE_CHASER, else
        // "Full Show") imported and made Tracking, as Convert would.
        for (Function *f : m_doc->functions())
            if (f->type() == Function::ChaserType && !st->isGenerated(f->id())
                && f->name() == qEnvironmentVariable("QLC_SKIPCUE_CHASER", "Full Show"))
            {
                const ShowTreeController::ImportReport rep = st->importChaser(f->id());
                ShowTreeController::ConvertPlan plan = st->planConvertToTracking(rep.list);
                if (plan.ok)
                    st->convertToTracking(rep.list, &plan);
                else
                    st->setListMode(rep.list, ListMode::Tracking);
                break;
            }
        list = longest();
    }
    QVERIFY(list != nullptr && list->cues.count() >= 6);
    const quint32 listId = list->id;
    const quint32 one = list->cues.at(1).id, two = list->cues.at(2).id;
    const quint32 three = list->cues.at(3).id, four = list->cues.at(4).id;
    QVERIFY(st->setCuesSkipped({ two }, true));
    QVERIFY(st->setCuesSkipped({ four }, true));
    list = st->cueList(listId);

    QSettings().remove(QStringLiteral("showtree/splitter2"));
    QSettings().remove(QStringLiteral("showtree/badgeColumn2"));
    ShowTreeView view(nullptr, m_doc);
    view.resize(1280, 760);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.tree()->collapseAll();
    for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() != ShowTreeView::CueNode)
            (*it)->setExpanded(true);
    cueRow(view.tree(), two)->setExpanded(true);
    view.selectNode(one);
    QTest::qWait(50);
    QVERIFY(view.grab().save(shot));
    QVERIFY(view.tree()->grab().save(QString(shot).replace(".png", "-tree.png")));
    view.selectNode(two);
    QTest::qWait(50);
    QVERIFY(view.grab().save(QString(shot).replace(".png", "-cue.png")));
    QFile tip(QString(shot).replace(".png", "-tooltip.txt"));
    QVERIFY(tip.open(QIODevice::WriteOnly));
    tip.write(cueRow(view.tree(), two)->toolTip(0).toUtf8());
    tip.write("\n\n---- next cue receives, from the cue before ----\n");
    tip.write(view.nextCueText(one).toUtf8());
    tip.write("\n");
    tip.write(view.nextCueText(three).toUtf8());
    view.hide();
}

/****************************************************************************
 * W0: seam fixes
 ****************************************************************************/

namespace
{
/* An old show's shape: a raw scene in two steps, an EFX beside it in one,
   a scene of groups and palettes in the other. */
Chaser *oldShow(Doc *doc, quint32 towers, quint32 full, Scene **hazeOut, EFX **efxOut)
{
    Scene *haze = new Scene(doc);
    haze->setName("Haze");
    haze->setValue(0, 0, 90);
    doc->addFunction(haze);
    EFX *efx = new EFX(doc);
    efx->setName("SC Circle 1");
    doc->addFunction(efx);
    Scene *tw = new Scene(doc);
    tw->setName("Towers-Full");
    tw->addFixtureGroup(towers);
    tw->addPalette(full);
    doc->addFunction(tw);
    Collection *one = new Collection(doc);
    one->setName("0.0.1-Open");
    doc->addFunction(one);
    one->addFunction(haze->id());
    one->addFunction(efx->id());
    Collection *two = new Collection(doc);
    two->setName("0.0.2-Towers");
    doc->addFunction(two);
    two->addFunction(haze->id());
    two->addFunction(tw->id());
    Chaser *chaser = new Chaser(doc);
    chaser->setName("Full Show");
    chaser->setDurationMode(Chaser::PerStep);
    doc->addFunction(chaser);
    chaser->addStep(ChaserStep(one->id(), 0, Function::infiniteSpeed(), 0));
    chaser->addStep(ChaserStep(two->id(), 0, Function::infiniteSpeed(), 0));
    *hazeOut = haze;
    *efxOut = efx;
    return chaser;
}

QTreeWidgetItem *layerRow(QTreeWidget *tree, const QString &text)
{
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LayerNode
            && (*it)->text(0).contains(text))
            return *it;
    return nullptr;
}

QString w0Shot(const QString &name)
{
    const QString dir = qEnvironmentVariable("QLC_W0_SHOTS");
    return dir.isEmpty() ? QString() : dir + QLatin1Char('/') + name;
}
}

/* An opaque layer (a raw scene, an EFX, ...) opens its function in its own
   editor: from the row's menu, a double-click, and its page. A layer of
   groups and palettes doesn't (it is edited on the page). */
void ShowTreeView_Test::openAnOpaqueLayer()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    Scene *haze = nullptr;
    EFX *efx = nullptr;
    Chaser *chaser = oldShow(m_doc, towers->id(), full, &haze, &efx);

    ShowTreeView view(nullptr, m_doc);
    QList<quint32> opened;
    view.setFunctionOpener([&opened](quint32 fid) {
        opened << fid;
        return true;
    });
    view.resize(1100, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.importChaser(chaser->id(), false);
    view.tree()->expandAll();

    QTreeWidgetItem *hazeRow = layerRow(view.tree(), "Haze");
    QTreeWidgetItem *efxRow = layerRow(view.tree(), "SC Circle 1");
    QTreeWidgetItem *towersRow = layerRow(view.tree(), "Towers");
    QVERIFY(hazeRow != nullptr && efxRow != nullptr && towersRow != nullptr);
    QCOMPARE(view.openableFunction(hazeRow), haze->id());
    QCOMPARE(view.openableFunction(efxRow), efx->id());
    QCOMPARE(view.openableFunction(towersRow), Function::invalidId());
    QCOMPARE(view.openableFunction(nullptr), Function::invalidId());

    // Double-click (W3): the layer in the Workbench, docked here -- the
    // EFX's own editor in its pane, ▣ Open loads it there.
    emit view.tree()->itemDoubleClicked(efxRow, 0);
    QVERIFY(view.isWorkbenchDocked());
    QCOMPARE(view.dockedWorkbench()->currentPage(), Workbench::FunctionPage);
    QVERIFY(opened.isEmpty());
    QVERIFY(view.closeWorkbench());
    efxRow = layerRow(view.tree(), "SC Circle 1");
    towersRow = layerRow(view.tree(), "Towers");
    emit view.tree()->itemDoubleClicked(towersRow, 0);
    QVERIFY(view.isWorkbenchDocked());
    QVERIFY(opened.isEmpty());
    QVERIFY(view.closeWorkbench());
    view.tree()->expandAll();
    hazeRow = layerRow(view.tree(), "Haze");
    efxRow = layerRow(view.tree(), "SC Circle 1");
    towersRow = layerRow(view.tree(), "Towers");

    // The row's menu: Open first.
    {
        QStringList actions;
        ModalCloser closer([&actions](QWidget *w) {
            QMenu *menu = qobject_cast<QMenu *>(w);
            if (menu == nullptr)
                return false;
            for (QAction *a : menu->actions())
                actions << a->objectName() + QLatin1Char('|') + a->text();
            for (QAction *a : menu->actions())
                if (a->objectName() == "stOpenFunction")
                {
                    menu->close();
                    a->trigger();
                    return true;
                }
            return false;
        });
        view.tree()->scrollToItem(hazeRow);
        const QPoint pos = view.tree()->visualItemRect(hazeRow).center();
        QVERIFY(QMetaObject::invokeMethod(&view, "slotTreeContextMenu", Q_ARG(QPoint, pos)));
        QVERIFY2(!actions.isEmpty() && actions.first() == QString("stOpenFunction|Open “Haze”…"),
                 qPrintable(actions.join(", ")));
        QCOMPARE(opened, QList<quint32>({ haze->id() }));
    }

    // The layer's page: Open, and where else it plays.
    const CueList *list = nullptr;
    for (const Production &p : m_doc->showTree()->productions())
        for (const CueList &l : p.lists)
            list = &l;
    QVERIFY(list != nullptr);
    const Cue &cue = list->cues.first();
    quint32 hazeLayer = invalidId();
    for (const Layer &l : cue.layers)
        if (l.target.function == haze->id())
            hazeLayer = l.id;
    QVERIFY(view.selectNode(hazeLayer, cue.id));
    QPushButton *open = view.findChild<QPushButton *>("stLayerOpen");
    QVERIFY(open != nullptr && open->isVisible());
    QCOMPARE(open->text(), QString("Open “Haze”"));
    QLabel *note = view.findChild<QLabel *>("stLayerOpenNote");
    QVERIFY(note != nullptr);
    // Set in 0.0.1 only: 0.0.2 tracks it in (the import's tracking reading).
    QVERIFY2(!note->text().contains("cue 0.0.2 Towers"), qPrintable(note->text()));
    QVERIFY2(note->text().contains("collection “0.0.1-Open”"), qPrintable(note->text()));
    QVERIFY(view.alsoIn(efx->id(), cue.id).contains("collection “0.0.1-Open”"));
    if (!w0Shot("layer-open.png").isEmpty())
        QVERIFY(view.grab().save(w0Shot("layer-open.png")));
    open->click();
    QCOMPARE(opened.last(), haze->id());

    // A layer of groups and palettes: no Open on its page.
    const Cue &two = list->cues.at(1);
    for (const Layer &l : two.layers)
        if (!l.target.isOpaque())
            QVERIFY(view.selectNode(l.id, two.id));
    QTest::qWait(10);
    QPushButton *gone = view.findChild<QPushButton *>("stLayerOpen");
    QVERIFY(gone == nullptr || !gone->isVisible());
    view.hide();
}

/* Open with no stand-in (board Q32, the Functions window retired): the
   Workbench -- the one docked in the tree when it is (↩ back to the cue),
   Build's otherwise; opening one from elsewhere brings the docked one home. */
void ShowTreeView_Test::openLandsInTheWorkbench()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    Scene *haze = nullptr;
    EFX *efx = nullptr;
    Chaser *chaser = oldShow(m_doc, towers->id(), full, &haze, &efx);
    ShowTreeView view(nullptr, m_doc);
    view.resize(1100, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.importChaser(chaser->id(), false);

    QVERIFY(!view.openFunction(haze->id()));        // no Build tab
    {
        ProgrammingManager build(nullptr, m_doc);
        QPointer<ProgrammingManager> guard(&build);
        view.setWorkbenchLender(
            [guard]() -> Workbench * { return guard.isNull() ? nullptr : guard->lendWorkbench(); },
            [guard](Workbench *wb) { if (!guard.isNull()) guard->returnWorkbench(wb); });

        // Nothing docked: Build's Workbench, with the function's own editor.
        QVERIFY(view.openFunction(efx->id()));
        QCOMPARE(build.workbench()->loadedFunction(), efx->id());
        QVERIFY(build.workbench()->findChild<EFXEditor *>() != nullptr);
        QVERIFY(!view.openFunction(Function::invalidId()));

        // Docked on a cue: it loads there, ↩ back to the cue.
        const CueList *list = nullptr;
        for (const Production &p : m_doc->showTree()->productions())
            for (const CueList &l : p.lists)
                list = &l;
        QVERIFY(list != nullptr);
        const quint32 cueId = list->cues.first().id;
        QVERIFY(view.openInWorkbench(cueId));
        QVERIFY(build.isWorkbenchLent());
        Workbench *bench = view.dockedWorkbench();
        QCOMPARE(bench, build.workbench());
        QVERIFY(view.openFunction(haze->id()));
        QCOMPARE(bench->loadedFunction(), haze->id());
        QVERIFY(bench->canGoBack());
        if (!w0Shot("open-in-docked-workbench.png").isEmpty())
            QVERIFY(view.grab().save(w0Shot("open-in-docked-workbench.png")));
        QVERIFY(bench->goBack());
        QVERIFY(qobject_cast<TreeCueStack *>(bench->stack()) != nullptr);

        // Opened from outside the tree: the Workbench goes home to Build.
        QVERIFY(ProgrammingManager::openFunction(efx->id()));
        QVERIFY(!view.isWorkbenchDocked());
        QVERIFY(!build.isWorkbenchLent());
        QCOMPARE(build.workbench()->loadedFunction(), efx->id());
        view.setWorkbenchLender(nullptr, nullptr);
    }
    view.hide();
}

/* Import chaser… shows a sheet: plays the same?, what each step became,
   what plays as it is and why -- and offers (ticked) to fire the list from
   what fires the chaser now: VC buttons, a VC Cue List, the main cue list
   and its cue-list number. One undo step with the import. */
void ShowTreeView_Test::theImportSheetReadsAndRepoints()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    Scene *haze = nullptr;
    EFX *efx = nullptr;
    Chaser *chaser = oldShow(m_doc, towers->id(), full, &haze, &efx);
    chaser->setCueListNumber("3");
    m_doc->showControl()->setMainCueList(chaser->id());
    ScopedVc vc(m_doc);
    VCFrame *contents = VirtualConsole::instance()->contents();
    VCButton *go = new VCButton(contents, m_doc);
    VirtualConsole::instance()->placeWidget(go, contents);
    go->setCaption("Show");
    go->setFunction(chaser->id());
    VCButton *other = new VCButton(contents, m_doc);
    VirtualConsole::instance()->placeWidget(other, contents);
    other->setCaption("Haze");
    other->setFunction(haze->id());
    VCCueList *cues = new VCCueList(contents, m_doc);
    VirtualConsole::instance()->placeWidget(cues, contents);
    cues->setCaption("Cues");
    cues->setChaser(chaser->id());

    ShowTreeView view(nullptr, m_doc);
    QString text;
    bool ticked = false;
    view.setSheetRunner([&](ImportSheet *sheet) {
        text = sheet->text();
        ticked = sheet->repointBox()->isChecked() && !sheet->repointBox()->isHidden();
        sheet->resize(680, 640);
        sheet->show();
        QVERIFY(QTest::qWaitForWindowExposed(sheet));
        if (!w0Shot("import-sheet.png").isEmpty())
            QVERIFY(sheet->grab().save(w0Shot("import-sheet.png")));
        sheet->findChild<QPushButton *>("stImportDone")->click();
    });
    view.importChaser(chaser->id());
    QVERIFY(ticked);
    for (const QString &want : { QString("Imported “Full Show”"), QString("Plays the same"),
                                 QString("0 channels differ"), QString("What each step became"),
                                 QString("Played as they are, and why"), QString("Haze: has channel values of its own (×2)"),
                                 QString("SC Circle 1"), QString("4 place(s)") })
        QVERIFY2(text.contains(want), qPrintable(want + " not in:\n" + text));

    ShowTreeController *st = m_doc->showTree();
    const CueList *list = nullptr;
    for (const Production &p : st->productions())
        for (const CueList &l : p.lists)
            list = &l;
    QVERIFY(list != nullptr);
    const quint32 listChaser = list->chaser;
    Chaser *generated = qobject_cast<Chaser *>(m_doc->function(listChaser));
    QVERIFY(generated != nullptr);
    QCOMPARE(go->function(), listChaser);
    QCOMPARE(other->function(), haze->id());          // not the chaser's: left alone
    QCOMPARE(cues->chaserID(), listChaser);
    QCOMPARE(m_doc->showControl()->mainCueList(), listChaser);
    QCOMPARE(generated->cueListNumber(), QString("3"));
    QCOMPARE(chaser->cueListNumber(), QString());

    // One undo step: all of it back on the chaser...
    QVERIFY(st->undo());
    QVERIFY(st->productions().isEmpty());
    QCOMPARE(go->function(), chaser->id());
    QCOMPARE(cues->chaserID(), chaser->id());
    QCOMPARE(m_doc->showControl()->mainCueList(), chaser->id());
    QCOMPARE(chaser->cueListNumber(), QString("3"));
    // ...and redone, on the list again.
    QVERIFY(st->redo());
    QCOMPARE(go->function(), listChaser);
    QCOMPARE(cues->chaserID(), listChaser);
    QCOMPARE(m_doc->showControl()->mainCueList(), listChaser);
    QCOMPARE(qobject_cast<Chaser *>(m_doc->function(listChaser))->cueListNumber(), QString("3"));
    QCOMPARE(chaser->cueListNumber(), QString());
}

/* Unticked, the VC stays on the chaser; Undo import takes the import back;
   nothing firing the chaser: no offer. */
void ShowTreeView_Test::theImportSheetCanUndoOrLeaveVcAlone()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    Scene *haze = nullptr;
    EFX *efx = nullptr;
    Chaser *chaser = oldShow(m_doc, towers->id(), full, &haze, &efx);
    ShowTreeController *st = m_doc->showTree();

    // Nothing fires it: no offer.
    {
        ShowTreeView view(nullptr, m_doc);
        bool offered = true;
        view.setSheetRunner([&offered](ImportSheet *sheet) {
            offered = !sheet->repointBox()->isHidden();
            QVERIFY(sheet->findChild<QLabel *>("stImportRepointNone") != nullptr);
            QVERIFY(!sheet->repoint());
            sheet->accept();
        });
        view.importChaser(chaser->id());
        QVERIFY(!offered);
        QCOMPARE(st->productions().count(), 1);
        QVERIFY(st->undo());
    }

    ScopedVc vc(m_doc);
    VCButton *go = new VCButton(VirtualConsole::instance()->contents(), m_doc);
    VirtualConsole::instance()->placeWidget(go, VirtualConsole::instance()->contents());
    go->setFunction(chaser->id());

    // Unticked.
    {
        ShowTreeView view(nullptr, m_doc);
        view.setSheetRunner([](ImportSheet *sheet) {
            sheet->repointBox()->setChecked(false);
            sheet->accept();
        });
        view.importChaser(chaser->id());
        QCOMPARE(st->productions().count(), 1);
        QCOMPARE(go->function(), chaser->id());
        QVERIFY(st->undo());
    }
    // Undo import.
    {
        ShowTreeView view(nullptr, m_doc);
        view.setSheetRunner([](ImportSheet *sheet) {
            sheet->findChild<QPushButton *>("stImportUndo")->click();
            QVERIFY(sheet->undoAsked());
            QVERIFY(!sheet->repoint());
        });
        view.importChaser(chaser->id());
        QVERIFY(st->productions().isEmpty());
        QCOMPARE(go->function(), chaser->id());
    }
}

/* Build's Send to Show Tree… on a chaser: the chaser row's right-click
   (Undo import here) and the Workbench's header button, which runs the
   Import in place through the sheet; "Done, show it in the Show Tree"
   goes to the new list's tab. What repeats from step to step (Haze) is
   set once and tracks (the sheet says so). */
void ShowTreeView_Test::sendToShowTreeFromBuild()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    Scene *haze = nullptr;
    EFX *efx = nullptr;
    Chaser *chaser = oldShow(m_doc, towers->id(), full, &haze, &efx);
    ShowTreeController *st = m_doc->showTree();

    QTabWidget tabs;
    ProgrammingManager *mgr = new ProgrammingManager(&tabs, m_doc);
    ShowTreeView *view = new ShowTreeView(&tabs, m_doc);
    tabs.addTab(mgr, "Build");
    tabs.addTab(view, "Show Tree");
    tabs.resize(1400, 900);
    tabs.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tabs));
    QCOMPARE(tabs.currentWidget(), mgr);
    mgr->setSection(ProgrammingManager::ScenesSection);
    mgr->showFunction(chaser->id());

    // The chaser row's right-click: Send to Show Tree…, then Undo import.
    ProgrammingOutline *nav = nullptr;
    for (ProgrammingOutline *o : mgr->findChildren<ProgrammingOutline *>())
        if (o->mode() == ProgrammingOutline::Navigator)
            nav = o;
    QVERIFY(nav != nullptr);
    ProgrammingOutline::Row row;
    row.kind = ProgrammingOutline::FunctionRow;
    row.id = row.scene = chaser->id();
    QTreeWidgetItem *item = nav->itemFor(row);
    QVERIFY(item != nullptr);
    nav->scrollToItem(item);
    QStringList actions;
    bool closed = false;
    // The one door (Q76): Upgrade This Show… with just this chaser ticked
    // and nothing captured. Closed without upgrading, nothing changes.
    view->setConverterRunner([&](ShowConverter *dialog) {
        QCOMPARE(dialog->mode(), ShowConverter::Upgrade);
        QCOMPARE(dialog->chosen(), QList<quint32>({ chaser->id() }));
        QVERIFY(dialog->chosenScenes().isEmpty());
        closed = true;
    });
    QTimer driver;
    driver.setInterval(20);
    connect(&driver, &QTimer::timeout, this, [&]() {
        QMenu *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (menu == nullptr)
            return;
        driver.stop();
        for (QAction *a : menu->actions())
            actions << a->objectName();
        QAction *send = menu->findChild<QAction *>("outlineSendToTree");
        menu->close();
        if (send != nullptr && send->isEnabled())
            send->trigger();
    });
    driver.start();
    emit nav->customContextMenuRequested(nav->visualItemRect(item).center());
    QTRY_VERIFY(closed);
    QVERIFY2(actions.contains("outlineSendToTree"), qPrintable(actions.join(", ")));
    QVERIFY(st->productions().isEmpty());
    QCOMPARE(tabs.currentWidget(), mgr);

    // The Workbench's header button: upgraded, the Show Tree comes forward.
    mgr->showFunction(chaser->id());
    QToolButton *send = mgr->findChild<QToolButton *>("workbenchSendToTree");
    QVERIFY(send != nullptr);
    QVERIFY(send->isVisible());
    QString text;
    view->setConverterRunner([&](ShowConverter *dialog) {
        QCOMPARE(dialog->chosen(), QList<quint32>({ chaser->id() }));
        QVERIFY(dialog->inPlace());
        text = dialog->convert().join('\n');
    });
    send->click();
    for (const QString &want : { QString("0 channels differ") })
        QVERIFY2(text.contains(want), qPrintable(want + " not in:\n" + text));
    QCOMPARE(st->productions().count(), 1);
    const CueList *list = &st->productions().first().lists.first();
    QVERIFY(list->inPlace);
    QCOMPARE(list->mode, ListMode::Tracking);
    QVERIFY(chaser->path(true).startsWith("Imported/"));
    QCOMPARE(tabs.currentWidget(), view);
    // Sent: not offered again.
    QVERIFY(!st->whyNotSend(chaser->id()).isEmpty());
    QVERIFY(send->isHidden());
}

/****************************************************************************
 * W5: the bridge
 ****************************************************************************/

namespace
{
/** A looping strobe of raw scenes: an effect chaser. */
Chaser *strobe(Doc *doc)
{
    Scene *on = new Scene(doc);
    on->setName("Strobe on");
    on->setValue(3, 0, 255);
    doc->addFunction(on);
    Scene *off = new Scene(doc);
    off->setName("Strobe off");
    off->setValue(3, 0, 0);
    doc->addFunction(off);
    Chaser *c = new Chaser(doc);
    c->setName("Strobe");
    c->setRunOrder(Function::Loop);
    c->setDurationMode(Chaser::Common);
    c->setDuration(100);
    doc->addFunction(c);
    c->addStep(ChaserStep(on->id()));
    c->addStep(ChaserStep(off->id()));
    return c;
}
}

/* An old show (chasers, no tree) says so once, at the top of the tab; it
   never converts on its own, ✕ puts it away, and a tree hides it. */
void ShowTreeView_Test::anOldShowOpensWithABanner()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    Scene *haze = nullptr;
    EFX *efx = nullptr;
    Chaser *chaser = oldShow(m_doc, towers->id(), dimmer(255, "Full"), &haze, &efx);
    strobe(m_doc);
    ShowTreeView view(nullptr, m_doc);
    view.updateBridgeBanner();
    QVERIFY(!view.bridgeBanner()->isHidden());
    const QString text = view.bridgeBanner()->findChild<QLabel *>("stBridgeText")->text();
    QVERIFY2(text.contains("1 chaser(s) that work like cue lists (Full Show)"), qPrintable(text));
    QVERIFY(!text.contains("Strobe"));
    QVERIFY(m_doc->showTree()->productions().isEmpty());     // nothing converted

    view.bridgeBanner()->findChild<QToolButton *>("stBridgeDismiss")->click();
    QVERIFY(view.bridgeBanner()->isHidden());
    emit m_doc->loaded();                                     // another show: asks again
    QVERIFY(!view.bridgeBanner()->isHidden());
    m_doc->showTree()->importChaser(chaser->id());            // a tree: never
    QVERIFY(view.bridgeBanner()->isHidden());
}

/* The converter: the cue list ticked with its plan and check, the effect
   chaser not offered; Convert imports in place -- originals filed, VC
   (buttons, Cue Lists, speed dials, clocks) and show control on the list
   -- one undo step a chaser. */
void ShowTreeView_Test::theConverterImportsCueListsInPlace()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    Scene *haze = nullptr;
    EFX *efx = nullptr;
    Chaser *chaser = oldShow(m_doc, towers->id(), dimmer(255, "Full"), &haze, &efx);
    Chaser *fx = strobe(m_doc);
    m_doc->showControl()->setMainCueList(chaser->id());
    ScopedVc vc(m_doc);
    VCFrame *contents = VirtualConsole::instance()->contents();
    VCButton *go = new VCButton(contents, m_doc);
    VirtualConsole::instance()->placeWidget(go, contents);
    go->setFunction(chaser->id());
    VCButton *flash = new VCButton(contents, m_doc);
    VirtualConsole::instance()->placeWidget(flash, contents);
    flash->setFunction(fx->id());
    VCSpeedDial *dial = new VCSpeedDial(contents, m_doc);
    VirtualConsole::instance()->placeWidget(dial, contents);
    dial->setCaption("Tempo");
    dial->setFunctions({ VCSpeedDialFunction(chaser->id()), VCSpeedDialFunction(fx->id()) });
    VCClock *clock = new VCClock(contents, m_doc);
    VirtualConsole::instance()->placeWidget(clock, contents);
    clock->setCaption("Doors");
    VCClockSchedule sch;
    sch.setFunction(chaser->id());
    sch.setTime(QDateTime(QDate(2026, 1, 1), QTime(19, 30)));
    clock->addSchedule(sch);

    ShowTreeView view(nullptr, m_doc);
    QString text;
    QStringList results;
    view.setConverterRunner([&](ShowConverter *c) {
        text = c->text();
        QCOMPARE(c->chosen(), QList<quint32>({ chaser->id() }));
        QVERIFY(c->inPlace());
        c->resize(900, 700);
        c->show();
        QVERIFY(QTest::qWaitForWindowExposed(c));
        if (!w0Shot("show-converter.png").isEmpty())
            QVERIFY(c->grab().save(w0Shot("show-converter.png")));
        results = c->convert();
        QVERIFY(c->chosen().isEmpty());          // Full Show is converted: no longer offered
    });
    view.openConverter();
    for (const QString &want : { QString("[x] Full Show | Cue list — waits for GO on 2 of 2"),
                                 QString("✓ 0 channels differ"), QString("Strobe | Effect — loops"),
                                 QString("stays a function"), QString("What each step became") })
        QVERIFY2(text.contains(want), qPrintable(want + " not in:\n" + text));
    QVERIFY2(results.join("\n").contains("original(s) filed under Imported/Full Show (originals)"),
             qPrintable(results.join("\n")));

    ShowTreeController *st = m_doc->showTree();
    QCOMPARE(st->productions().count(), 1);
    const CueList *list = &st->productions().first().lists.first();
    const quint32 listChaser = list->chaser;
    QVERIFY(list->inPlace);
    QCOMPARE(list->source, chaser->id());
    QCOMPARE(chaser->path(true), QString("Imported/Full Show (originals)"));
    QCOMPARE(go->function(), listChaser);
    QCOMPARE(flash->function(), fx->id());
    QCOMPARE(dial->functions().at(0).functionId, listChaser);
    QCOMPARE(dial->functions().at(1).functionId, fx->id());
    QCOMPARE(clock->schedules().first().function(), listChaser);
    QCOMPARE(m_doc->showControl()->mainCueList(), listChaser);

    // One undo step: all of it back.
    QVERIFY(st->undo());
    QVERIFY(st->productions().isEmpty());
    QCOMPARE(chaser->path(true), QString());
    QCOMPARE(go->function(), chaser->id());
    QCOMPARE(dial->functions().at(0).functionId, chaser->id());
    QCOMPARE(clock->schedules().first().function(), chaser->id());
    QCOMPARE(m_doc->showControl()->mainCueList(), chaser->id());
    QVERIFY(st->redo());
    QCOMPARE(go->function(), listChaser);
    QCOMPARE(dial->functions().at(0).functionId, listChaser);
}

/* Q141: an original Import in place keeps for Restore original, which the
   list doesn't play, opens view only under its own banner -- not "Made by
   the Show Tree" -- naming the cue to edit, with Show in Show Tree going
   there; a scene the list plays as it is opens to edit as ever; what the
   tree made keeps its words. The Functions tab, its tooltips, Build. */
void ShowTreeView_Test::originalsOpenViewOnlyElsewhere()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    Scene *haze = nullptr;
    EFX *efx = nullptr;
    Chaser *chaser = oldShow(m_doc, towers->id(), dimmer(255, "Full"), &haze, &efx);
    Scene *tw = nullptr;
    for (Function *f : m_doc->functions())
        if (f->name() == "Towers-Full")
            tw = qobject_cast<Scene *>(f);
    QVERIFY(tw != nullptr);
    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importInPlace(chaser->id());
    const CueList *list = st->cueList(rep.list);
    QVERIFY(list != nullptr && list->cues.count() == 2);
    quint32 cueId = invalidId();
    QCOMPARE(st->originalFor(tw->id(), &cueId), rep.list);
    QCOMPARE(cueId, list->cues[1].id);
    QCOMPARE(st->originalFor(haze->id()), invalidId());      // played as it is
    quint32 made = invalidId();
    const Collection *cue1 = qobject_cast<const Collection *>(m_doc->function(list->cues[1].collection));
    QVERIFY(cue1 != nullptr);
    for (quint32 fid : cue1->functions())
        if (st->isGenerated(fid) && qobject_cast<Scene *>(m_doc->function(fid)) != nullptr)
            made = fid;
    QVERIFY(made != invalidId());
    m_doc->function(made)->setVisible(true);                  // listed, as an Eos import's are
    const QString original = ShowTreeController::originalText();
    const QString locked = ShowTreeController::lockedText();
    const QString edit = QString("edit cue %1 there").arg(st->cueLabel(*list, 1));

    ShowTreeView view(nullptr, m_doc);
    {
        FunctionManager fm(nullptr, m_doc);
        FunctionsTreeWidget *ftree = fm.findChild<FunctionsTreeWidget *>();
        QVERIFY(ftree != nullptr);
        auto open = [&](Function *f) {
            QTreeWidgetItem *row = ftree->functionItem(f);
            if (row == nullptr)
                return static_cast<SceneEditor *>(nullptr);
            QMetaObject::invokeMethod(&fm, "slotTreeItemDoubleClicked", Q_ARG(QTreeWidgetItem *, row));
            // The editor before goes later: gone now.
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            return fm.findChild<SceneEditor *>();
        };

        // The original: greyed under its banner, which names the cue.
        QTreeWidgetItem *row = ftree->functionItem(tw);
        QVERIFY(row != nullptr);
        QVERIFY(row->toolTip(0).startsWith(original));
        QVERIFY(row->toolTip(0).contains(edit));
        QVERIFY(!row->toolTip(0).contains(locked));
        SceneEditor *ed = open(tw);
        QVERIFY(ed != nullptr);
        QVERIFY(!ed->isEnabled());
        QList<QLabel *> texts = fm.findChildren<QLabel *>("stLockedText");
        QCOMPARE(texts.count(), 1);
        QVERIFY(texts.first()->text().contains(original.toHtmlEscaped()));
        QVERIFY(texts.first()->text().contains(edit));
        QVERIFY(!texts.first()->text().contains(locked.toHtmlEscaped()));
        QVERIFY(fm.findChild<QPushButton *>("stLockedDetach") == nullptr);
        QPushButton *reveal = fm.findChild<QPushButton *>("stLockedReveal");
        QVERIFY(reveal != nullptr && reveal->isEnabled());
        reveal->click();
        QCOMPARE(view.currentKind(), ShowTreeView::CueNode);
        QCOMPARE(view.currentId(), list->cues[1].id);

        // Its chaser too, naming the list.
        QMetaObject::invokeMethod(&fm, "slotTreeItemDoubleClicked",
                                  Q_ARG(QTreeWidgetItem *, ftree->functionItem(chaser)));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        ChaserEditor *ced = fm.findChild<ChaserEditor *>();
        QVERIFY(ced != nullptr && !ced->isEnabled());
        texts = fm.findChildren<QLabel *>("stLockedText");
        QCOMPARE(texts.count(), 1);
        QVERIFY(texts.first()->text().contains(original.toHtmlEscaped()));
        QVERIFY(texts.first()->text().contains("edit it there"));

        // Played as it is: the user's scene, to edit.
        row = ftree->functionItem(haze);
        QVERIFY(row != nullptr && !row->toolTip(0).contains(original) && !row->toolTip(0).contains(locked));
        ed = open(haze);
        QVERIFY(ed != nullptr && ed->isEnabled());

        // What the tree made: its own words.
        row = ftree->functionItem(m_doc->function(made));
        QVERIFY(row != nullptr && row->toolTip(0).startsWith(locked));
        ed = open(m_doc->function(made));
        QVERIFY(ed != nullptr && !ed->isEnabled());
        texts = fm.findChildren<QLabel *>("stLockedText");
        QCOMPARE(texts.count(), 1);
        QVERIFY(texts.first()->text().contains(locked.toHtmlEscaped()));
        QVERIFY(!texts.first()->text().contains(original.toHtmlEscaped()));
    }

    // Build: the Workbench says the same, greyed; Send to Show Tree too.
    {
        FunctionStack fs(m_doc, tw->id());
        QVERIFY(fs.readOnlyReason().startsWith(original));
        FunctionStack plain(m_doc, haze->id());
        QVERIFY(plain.readOnlyReason().isEmpty());
        FunctionStack gen(m_doc, made);
        QVERIFY(gen.readOnlyReason().startsWith("Made by the Show Tree"));

        ProgrammingManager pm(nullptr, m_doc);
        Workbench *wb = pm.workbench();
        wb->load(tw->id());
        QVERIFY(wb->m_lockedBanner != nullptr);
        QLabel *text = wb->m_lockedBanner->findChild<QLabel *>("stLockedText");
        QVERIFY(text != nullptr && text->text().contains(original.toHtmlEscaped()) && text->text().contains(edit));
        QVERIFY(!wb->m_page->isEnabled());
        // Refine edits what is loaded: not offered on an original.
        QVERIFY(wb->m_refineBtn->isHidden());
        QVERIFY(!wb->openRefine());
        wb->load(haze->id());
        QVERIFY(wb->m_lockedBanner == nullptr);
        QVERIFY(wb->m_page->isEnabled());
        QVERIFY(!wb->m_refineBtn->isHidden());
    }
    QVERIFY(st->whyNotSend(chaser->id()).startsWith("Upgraded already"));
}

/* Restore original: asks, then the list goes, the originals come back and
   what fired the list fires the chaser -- one undo step, both ways. */
void ShowTreeView_Test::restoreOriginalPointsVcBack()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    Scene *haze = nullptr;
    EFX *efx = nullptr;
    Chaser *chaser = oldShow(m_doc, towers->id(), dimmer(255, "Full"), &haze, &efx);
    ScopedVc vc(m_doc);
    VCFrame *contents = VirtualConsole::instance()->contents();
    VCButton *go = new VCButton(contents, m_doc);
    VirtualConsole::instance()->placeWidget(go, contents);
    go->setFunction(chaser->id());
    VCCueList *cues = new VCCueList(contents, m_doc);
    VirtualConsole::instance()->placeWidget(cues, contents);
    cues->setChaser(chaser->id());

    ShowTreeView view(nullptr, m_doc);
    QStringList asked;
    view.setConfirm([&asked](const QString &title, const QString &text, const QString &) {
        asked << title + ": " + text;
        return true;
    });
    view.importInPlace(chaser->id());
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->productions().first().lists.first().id;
    const quint32 listChaser = st->cueList(listId)->chaser;
    QCOMPARE(go->function(), listChaser);
    QCOMPARE(cues->chaserID(), listChaser);

    QVERIFY(view.restoreOriginal(listId));
    QVERIFY2(asked.join("\n").contains("Restore original: Restore “Full Show”?"), qPrintable(asked.join("\n")));
    QVERIFY(st->productions().isEmpty());
    QVERIFY(m_doc->function(listChaser) == nullptr);
    QCOMPARE(go->function(), chaser->id());
    QCOMPARE(cues->chaserID(), chaser->id());
    QCOMPARE(chaser->path(true), QString());

    QVERIFY(st->undo());                    // back in place
    QVERIFY(st->cueList(listId) != nullptr);
    QCOMPARE(go->function(), listChaser);
    QCOMPARE(cues->chaserID(), listChaser);
    QCOMPARE(chaser->path(true), QString("Imported/Full Show (originals)"));
    QVERIFY(st->redo());                    // restored again
    QCOMPARE(go->function(), chaser->id());
    QCOMPARE(cues->chaserID(), chaser->id());
}

/* A ▣ raw scene's row: Capture into palettes… on its page and its menu;
   the plan asked, then the layers in every cue that plays it. */
void ShowTreeView_Test::captureFromTheOpaqueRow()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    Scene *haze = nullptr;
    EFX *efx = nullptr;
    Chaser *chaser = oldShow(m_doc, towers->id(), dimmer(255, "Full"), &haze, &efx);
    ShowTreeView view(nullptr, m_doc);
    view.resize(1100, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QString details;
    view.setConfirm([&details](const QString &, const QString &, const QString &d) {
        details = d;
        return true;
    });
    view.importChaser(chaser->id(), false);
    view.tree()->expandAll();
    QTreeWidgetItem *hazeRow = layerRow(view.tree(), "Haze");
    QVERIFY(hazeRow != nullptr);
    view.tree()->setCurrentItem(hazeRow);
    QPushButton *cap = view.findChild<QPushButton *>("stLayerCapture");
    QVERIFY(cap != nullptr);
    QVERIFY(cap->isEnabled());
    cap->click();
    QVERIFY2(details.contains("Dimmer"), qPrintable(details));
    QVERIFY2(details.contains("Where: Full Show 0.0.1 Open"), qPrintable(details));
    QVERIFY(!details.contains("0.0.2 Towers"));     // it tracks in there
    const CueList &list = m_doc->showTree()->productions().first().lists.first();
    for (const Cue &c : list.cues)
        for (const Layer &l : c.layers)
            QVERIFY(l.target.function != haze->id());
    QVERIFY(m_doc->function(haze->id()) != nullptr);          // kept
    QVERIFY(m_doc->showTree()->undo());
    view.tree()->expandAll();
    QVERIFY(layerRow(view.tree(), "Haze") != nullptr);
}

/* Upgrade this show (§3.7, the File menu and the banner): every chaser and
   collection with its plan, raw scenes offered for Capture into palettes
   once their list is ticked; Upgrade is one undo step; Restore original
   from the same dialog. */
void ShowTreeView_Test::upgradeThisShow()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    Scene *haze = nullptr;
    EFX *efx = nullptr;
    Chaser *chaser = oldShow(m_doc, towers->id(), dimmer(255, "Full"), &haze, &efx);
    Chaser *fx = strobe(m_doc);
    Collection *loose = new Collection(m_doc);
    loose->setName("Busk Haze");
    m_doc->addFunction(loose);
    loose->addFunction(haze->id());
    ScopedVc vc(m_doc);
    VCFrame *contents = VirtualConsole::instance()->contents();
    VCButton *go = new VCButton(contents, m_doc);
    VirtualConsole::instance()->placeWidget(go, contents);
    go->setFunction(chaser->id());
    ShowTreeController *st = m_doc->showTree();

    ShowTreeView view(nullptr, m_doc);
    view.setConfirm([](const QString &, const QString &, const QString &) { return true; });
    view.updateBridgeBanner();
    QPushButton *bannerButton = view.bridgeBanner()->findChild<QPushButton *>("stBridgeUpgrade");
    QVERIFY(bannerButton != nullptr);
    QCOMPARE(bannerButton->text(), QString("Upgrade this show…"));

    QString planText, untickedText, afterText;
    QStringList results;
    quint32 listId = ShowTree::invalidId();
    int opened = 0;
    view.setConverterRunner([&](ShowConverter *c) {
        ++opened;
        QCOMPARE(c->mode(), ShowConverter::Upgrade);
        QCOMPARE(c->windowTitle(), QString("Upgrade this show"));
        QCOMPARE(c->chosen(), QList<quint32>({ chaser->id() }));
        QVERIFY(c->chosenScenes().isEmpty());        // capture is never ticked for you
        QVERIFY(st->productions().isEmpty());        // nothing changed by opening it
        // Haze is offered: Full Show's list would play it raw.
        bool hazeOffered = false;
        for (const ShowConverter::CaptureRow &r : c->captureRows())
            if (r.scene == haze->id())
                hazeOffered = !r.plan.layers.isEmpty() && r.chasers.contains(chaser->id()) && !r.inTree;
        QVERIFY(hazeOffered);
        c->setChosen(chaser->id(), false);
        untickedText = c->text();
        c->setChosen(chaser->id(), true);
        c->setCaptureChosen(haze->id(), true);
        QCOMPARE(c->chosenScenes(), QList<quint32>({ haze->id() }));
        planText = c->text();
        c->resize(1000, 720);
        c->show();
        QVERIFY(QTest::qWaitForWindowExposed(c));
        if (!w0Shot("upgrade-show.png").isEmpty())
        {
            QVERIFY(c->grab().save(w0Shot("upgrade-show.png")));
            c->pages()->setCurrentIndex(1);
            QVERIFY(c->grab().save(w0Shot("upgrade-capture.png")));
            c->pages()->setCurrentIndex(0);
        }
        results = c->convert();
        afterText = c->text();
        QVERIFY(c->chosen().isEmpty());
        QCOMPARE(c->convertedLists().count(), 1);
        listId = c->convertedLists().first();
        if (!w0Shot("upgrade-done.png").isEmpty())
            QVERIFY(c->grab().save(w0Shot("upgrade-done.png")));
    });
    bannerButton->click();
    QCOMPARE(opened, 1);

    for (const QString &want : { QString("[x] Full Show | Cue list — waits for GO on 2 of 2"),
                                 QString("cue list: 2 cue(s)"), QString("✓ 0 channels differ"),
                                 QString("Strobe | Effect — loops"), QString("kept as a function"),
                                 QString("0.0.1-Open | Collection — 2 function(s) | read into the cues of Full Show"),
                                 QString("Busk Haze | Collection — 1 function(s) | leave alone: plays as it is"),
                                 QString("[x] Haze | Full Show (once converted)") })
        QVERIFY2(planText.contains(want), qPrintable(want + " not in:\n" + planText));
    for (const QString &want : { QString("[ ] Full Show"), QString("leave alone: plays as it is"),
                                 QString("( ) Haze") })
        QVERIFY2(untickedText.contains(want), qPrintable(want + " not in:\n" + untickedText));
    const QString all = results.join("\n");
    QVERIFY2(all.contains("Full Show: a cue list of 2 cue(s); plays the same (0 channels differ)."), qPrintable(all));
    // Haze is set in 0.0.1 and tracks into 0.0.2 (the import's tracking
    // reading): captured where it is set, the capture tracks in with it.
    QVERIFY2(all.contains("Haze: captured into palettes in 1 cue(s), 0 channels differ."), qPrintable(all));
    QVERIFY2(afterText.contains("Full Show | Upgraded already: cue list Full Show (Restore original puts the "
                                "chaser back). | [Restore original…]"), qPrintable(afterText));
    QVERIFY2(afterText.contains("takes the whole upgrade back as one step"), qPrintable(afterText));

    const CueList *list = st->cueList(listId);
    QVERIFY(list != nullptr && list->inPlace);
    const quint32 listChaser = list->chaser;
    QCOMPARE(go->function(), listChaser);
    for (const Cue &c : list->cues)
        for (const Layer &l : c.layers)
            QVERIFY(l.target.function != haze->id());          // captured
    QCOMPARE(fx->path(true), QString());                       // the effect left as it is
    QCOMPARE(loose->path(true), QString());

    // One undo step for the lot, both ways.
    QCOMPARE(st->undoText(), QString("Upgrade this show"));
    QVERIFY(st->undo());
    QVERIFY(st->productions().isEmpty());
    QVERIFY(!st->canUndo());
    QCOMPARE(go->function(), chaser->id());
    QCOMPARE(chaser->path(true), QString());
    QVERIFY(st->redo());
    QCOMPARE(go->function(), listChaser);
    QCOMPARE(chaser->path(true), QString("Imported/Full Show (originals)"));

    // Restore original from the same dialog (File ▸ Upgrade this show…).
    bool restored = false;
    view.setConverterRunner([&](ShowConverter *c) {
        QCOMPARE(c->convertedLists(), QList<quint32>({ listId }));
        restored = c->restore(listId);
        QVERIFY(c->convertedLists().isEmpty());
        QCOMPARE(c->chosen(), QList<quint32>({ chaser->id() }));   // offered again
    });
    view.openUpgrade();
    QVERIFY(restored);
    QVERIFY(st->productions().isEmpty());
    QCOMPARE(go->function(), chaser->id());
    QCOMPARE(chaser->path(true), QString());
    QVERIFY(st->undo());                                       // the restore
    QCOMPARE(go->function(), listChaser);
    QVERIFY(st->undo());                                       // the upgrade
    QVERIFY(st->productions().isEmpty());
    QCOMPARE(go->function(), chaser->id());
}

/* Upgrade This Show… on a show upgraded already (PMS Stage, 2026-10-24):
   File ▸ Upgrade This Show… still opens, the upgraded chaser greyed with
   why and Restore original… on its row, the Show Tree's own chaser counted,
   not listed, and "nothing left to upgrade" said. The header's button stays
   hidden (Q90). */
void ShowTreeView_Test::upgradeListsWhatIsUpgradedAlready()
{
    ShowTreeController *st = m_doc->showTree();
    ShowTreeView view(nullptr, m_doc);
    int asked = 0;
    view.setConfirm([&asked](const QString &, const QString &, const QString &) {
        ++asked;
        return true;
    });

    // A show with no chaser at all: it opens and says so.
    QString emptyNote;
    int opened = 0;
    view.setConverterRunner([&](ShowConverter *c) {
        ++opened;
        emptyNote = c->note();
        QVERIFY(!c->convertButton()->isEnabled());
    });
    view.openUpgrade();
    QCOMPARE(opened, 1);
    QCOMPARE(emptyNote, QString("Nothing to upgrade: no chaser of this show works as a cue list."));

    FixtureGroup *towers = group("Towers", 0, 2);
    Scene *haze = nullptr;
    EFX *efx = nullptr;
    Chaser *chaser = oldShow(m_doc, towers->id(), dimmer(255, "Full"), &haze, &efx);
    view.setConverterRunner([&](ShowConverter *c) {
        QCOMPARE(c->chosen(), QList<quint32>({ chaser->id() }));
        QVERIFY(c->note().isEmpty());              // something to upgrade: nothing to say
        c->convert();
    });
    view.openUpgrade();
    const quint32 listId = st->importedInto(chaser->id());
    QVERIFY(listId != ShowTree::invalidId());
    const quint32 listChaser = st->cueList(listId)->chaser;
    QVERIFY(st->isGenerated(listChaser));
    QVERIFY(st->convertibleChasers().isEmpty());
    QCOMPARE(st->whyNotSend(listChaser),
             QString("Made by the Show Tree: it plays the cue list %1.").arg(chaser->name()));

    // The header's button: hidden, nothing to convert (Q90).
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.updateBridgeBanner();
    QToolButton *header = view.findChild<QToolButton *>("stUpgradeShow");
    QVERIFY(header != nullptr);
    QTRY_VERIFY(!header->isVisible());

    // The dialog (as File ▸ Upgrade This Show… opens it): greyed, why, the
    // row's Restore original…
    QString text;
    bool restoredInDialog = false;
    view.setConverterRunner([&](ShowConverter *c) {
        ++opened;
        text = c->text();
        QVERIFY(c->chosen().isEmpty());
        QVERIFY(!c->convertButton()->isEnabled());
        QCOMPARE(c->convertedLists(), QList<quint32>({ listId }));
        QPushButton *restore = c->restoreButton(listId);
        QVERIFY(restore != nullptr);
        QVERIFY(restore->isEnabled());
        QCOMPARE(restore->text(), QString("Restore original…"));
        c->resize(900, 600);
        c->show();
        QVERIFY(QTest::qWaitForWindowExposed(c));
        if (!w0Shot("upgrade-nothing-left.png").isEmpty())
            QVERIFY(c->grab().save(w0Shot("upgrade-nothing-left.png")));
        restore->click();                           // queued: the table is rebuilt
        QTRY_VERIFY(c->convertedLists().isEmpty());
        restoredInDialog = true;
        QCOMPARE(c->chosen(), QList<quint32>({ chaser->id() }));   // offered again
        QVERIFY(c->note().isEmpty());
        QVERIFY(c->convertButton()->isEnabled());
        c->hide();
    });
    view.openUpgrade();
    QCOMPARE(opened, 2);
    const QString name = chaser->name();
    for (const QString &want : { QString("    %1 | Upgraded already: cue list %1 (Restore original puts the chaser "
                                         "back). | [Restore original…]").arg(name),
                                 QString("Nothing left to upgrade: every cue-list chaser of this show is a Show "
                                         "Tree cue list already. Restore original… on its row puts one back."),
                                 QString("1 chaser(s) belong to the Show Tree's own cue lists, and aren't listed.") })
        QVERIFY2(text.contains(want), qPrintable(want + " not in:\n" + text));
    // The Show Tree's own chaser (the same name) isn't a row.
    QVERIFY2(text.count("    " + name + " | ") == 1 && !text.contains("] " + name + " | "), qPrintable(text));

    // Restore original, as it always does: asked, done, one undo step.
    QVERIFY(restoredInDialog);
    QCOMPARE(asked, 1);
    QVERIFY(st->productions().isEmpty());
    QCOMPARE(chaser->path(true), QString());
    QVERIFY(st->undo());
    QCOMPARE(st->importedInto(chaser->id()), listId);
}

/* Library group rows, right-click (2026-10-22): Select all similar -- the
   groups like these selected, ready for Make set (offered right after) --
   and Move to folder ▸ / New Folder…, the Fixture Manager's own rules. */
void ShowTreeView_Test::libraryGroupToolsSelectSimilarAndMove()
{
    FixtureGroup *sl = group("SL", 0, 2);
    FixtureGroup *sr = group("SR", 2, 2);
    FixtureGroup *mid = group("Mid", 4, 1);
    sl->setPath("Towers");
    sr->setPath("Towers");
    mid->setPath("Towers");
    FixtureGroup *other = group("Other", 4, 2);   // alike, but not in Towers
    ShowTreeController *st = m_doc->showTree();

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *lib = view.library();
    auto selectedGroups = [lib]() {
        QSet<quint32> out;
        for (QTreeWidgetItem *it : lib->selectedItems())
            if (it->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LibGroup)
                out.insert(it->data(0, ShowTreeView::IdRole).toUInt());
        return out;
    };
    QTreeWidgetItem *a = rowOf(lib, ShowTreeView::LibGroup, sl->id());
    QVERIFY(a != nullptr);
    QCOMPARE(view.selectSimilarLibraryGroups(), 0);   // nothing selected
    lib->clearSelection();
    for (QTreeWidgetItem *p = a->parent(); p != nullptr; p = p->parent())
        p->setExpanded(true);
    lib->scrollToItem(a);
    a->setSelected(true);

    // Through the menu: Select all similar, Make set right after it, then
    // Move to folder.
    QStringList names;
    QTimer driver;
    driver.setInterval(20);
    connect(&driver, &QTimer::timeout, this, [&]() {
        QMenu *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (menu == nullptr)
            return;
        driver.stop();
        for (QAction *act : menu->actions())
            if (!act->isSeparator())
                names << (act->menu() != nullptr ? act->menu()->objectName() : act->objectName());
        QAction *similar = menu->findChild<QAction *>("stSelectSimilarGroups");
        menu->close();
        if (similar != nullptr)
            similar->trigger();
    });
    driver.start();
    emit lib->customContextMenuRequested(lib->visualItemRect(a).center());
    QTRY_VERIFY(!driver.isActive());
    const int similarAt = names.indexOf("stSelectSimilarGroups");
    QVERIFY2(similarAt >= 0, qPrintable(names.join(", ")));
    QCOMPARE(names.value(similarAt + 1), QString("stMakeSet"));
    QCOMPARE(names.value(similarAt + 2), QString("stMoveGroupsToFolder"));
    // Output Rules… for the groups' members (relays, house lights).
    QVERIFY2(names.contains("stOutputRules"), qPrintable(names.join(", ")));
    QTRY_COMPARE(selectedGroups(), QSet<quint32>({ sl->id(), sr->id() }));
    QVERIFY(view.findChild<QLabel *>("stDropHint")->text().contains("Make set"));
    QVERIFY(!selectedGroups().contains(mid->id()) && !selectedGroups().contains(other->id()));

    // ...and the set from them.
    const quint32 towers = view.makeSetFromLibrary("Towers");
    QVERIFY(towers != invalidId());
    QCOMPARE(QSet<quint32>(st->groupSet(towers)->groups.begin(), st->groupSet(towers)->groups.end()),
             QSet<quint32>({ sl->id(), sr->id() }));

    // Move to folder ▸ New Folder…: inside the one they are in, still selected.
    QTest::qWait(50);   // the Library reloads with the new set
    lib->clearSelection();
    rowOf(lib, ShowTreeView::LibGroup, sl->id())->setSelected(true);
    rowOf(lib, ShowTreeView::LibGroup, sr->id())->setSelected(true);
    QTreeWidgetItem *b = rowOf(lib, ShowTreeView::LibGroup, sr->id());
    for (QTreeWidgetItem *p = b->parent(); p != nullptr; p = p->parent())
        p->setExpanded(true);
    lib->scrollToItem(b);
    QStringList folders;
    std::unique_ptr<ModalCloser> closer;   // the New Folder… name, once the menu is gone
    QTimer driver2;
    driver2.setInterval(20);
    connect(&driver2, &QTimer::timeout, this, [&]() {
        QMenu *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (menu == nullptr)
            return;
        driver2.stop();
        QMenu *move = menu->findChild<QMenu *>("stMoveGroupsToFolder");
        QAction *nf = move == nullptr ? nullptr : move->findChild<QAction *>("newGroupFolder");
        if (move != nullptr)
            for (QAction *act : move->actions())
                folders << act->text().trimmed();
        menu->close();
        closer.reset(new ModalCloser([](QWidget *w) {
            QInputDialog *d = qobject_cast<QInputDialog *>(w);
            if (d == nullptr)
                return false;
            d->setTextValue("Pair");
            d->accept();
            return true;
        }));
        if (nf != nullptr)
            nf->trigger();
    });
    driver2.start();
    emit lib->customContextMenuRequested(lib->visualItemRect(b).center());
    QTRY_VERIFY2(sl->path() == QString("Towers/Pair"), qPrintable(sl->path() + " / " + folders.join(", ")));
    QCOMPARE(sr->path(), QString("Towers/Pair"));
    QCOMPARE(mid->path(), QString("Towers"));
    QVERIFY2(folders.contains("Towers") && folders.contains("Fixture Groups (no folder)"),
             qPrintable(folders.join(", ")));
    QCOMPARE(selectedGroups(), QSet<quint32>({ sl->id(), sr->id() }));
    QTreeWidgetItem *moved = rowOf(lib, ShowTreeView::LibGroup, sl->id());
    QCOMPARE(moved->parent()->text(0), QString("Pair"));
    QCOMPARE(moved->parent()->parent()->text(0), QString("Towers"));

    // The API, to no folder: they go to the Groups heading.
    QCOMPARE(view.moveLibraryGroupsToFolder(QString()), 2);
    QCOMPARE(sl->path(), QString());
    QCOMPARE(selectedGroups(), QSet<quint32>({ sl->id(), sr->id() }));
    QCOMPARE(view.moveLibraryGroupsToFolder(QString()), 0);
}

/* The same on DanceHysteria-0.6 (a copy): DS1t's like, all the truss
   tops, made a set and filed together. Skipped when the show is not here. */
void ShowTreeView_Test::libraryGroupToolsOnARealShow()
{
    QString show = qEnvironmentVariable("QLC_SHOWTREE_DH_SHOW");
    if (show.isEmpty())
        show = QString::fromUtf8(QLC_SHOWTREE_DH_SHOW);
    if (!QFileInfo::exists(show))
        QSKIP("DanceHysteria-0.6 is not here (set QLC_SHOWTREE_DH_SHOW)");
    QTemporaryDir dir;
    const QString copy = dir.filePath(QStringLiteral("show.qlcc"));
    QVERIFY(QFile::copy(show, copy));
    delete m_doc;
    m_doc = new Doc(this);
    m_doc->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures"));
    m_doc->setWorkspacePath(dir.path());
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
    QVERIFY2(m_doc->fixtureGroups().size() >= 40, "not the show with the groups");
    FixtureGroup *ds1t = nullptr;
    for (FixtureGroup *g : m_doc->fixtureGroups())
        if (g->name() == "DS1t")
            ds1t = g;
    QVERIFY(ds1t != nullptr);
    const QList<quint32> tops = FixtureManager::similarGroups(m_doc, { ds1t->id() });
    QVERIFY(tops.size() >= 4);

    ShowTreeView view(nullptr, m_doc);
    QTreeWidget *lib = view.library();
    QTreeWidgetItem *row = rowOf(lib, ShowTreeView::LibGroup, ds1t->id());
    QVERIFY(row != nullptr);
    lib->clearSelection();
    row->setSelected(true);
    QCOMPARE(view.selectSimilarLibraryGroups(), int(tops.size()));
    const quint32 set = view.makeSetFromLibrary("Truss tops");
    QVERIFY(set != invalidId());
    QCOMPARE(QSet<quint32>(m_doc->showTree()->groupSet(set)->groups.begin(),
                           m_doc->showTree()->groupSet(set)->groups.end()),
             QSet<quint32>(tops.begin(), tops.end()));
    // Made, the Library reloads: select them again to file them.
    QTest::qWait(50);
    row = rowOf(lib, ShowTreeView::LibGroup, ds1t->id());
    lib->clearSelection();
    row->setSelected(true);
    QCOMPARE(view.selectSimilarLibraryGroups(), int(tops.size()));
    QCOMPARE(view.moveLibraryGroupsToFolder("Truss tops"), int(tops.size()));
    for (quint32 gid : tops)
        QCOMPARE(m_doc->fixtureGroup(gid)->path(), QString("Truss tops"));
    QCOMPARE(view.selectSimilarLibraryGroups(), int(tops.size()));   // alike still
}

void ShowTreeView_Test::moveInBlackPerListAndCueInTheTree()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    ProgrammerController *pc = m_doc->programmer();
    QVERIFY(pc != nullptr);
    pc->setAutoMoveInBlack(true);
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    QList<quint32> c;
    for (int i = 0; i < 3; ++i)
        c << st->addCue(list, i - 1, QString("C%1").arg(i + 1));
    st->setLayerLook(c[0], st->addLayer(c[0], groupTarget(towers->id())), st->lookForPalette(half));
    st->clearUndo();

    ShowTreeView view(nullptr, m_doc);
    view.resize(1280, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    tree->expandAll();
    auto listRow = [tree, list]() -> QTreeWidgetItem * {
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::ListNode
                && (*it)->data(0, ShowTreeView::IdRole).toUInt() == list)
                return *it;
        return nullptr;
    };
    auto trigger = [](QMenu *menu, const QString &name) {
        for (QAction *a : menu->actions())
            if (a->objectName() == name)
            {
                a->trigger();
                return true;
            }
        return false;
    };
    QVERIFY(!listRow()->text(1).contains("MIB"));
    QVERIFY(!cueRow(tree, c[1])->text(ShowTreeView::FlagsCol).contains("MIB"));

    // The list's page: Move in black, "Use show's (On)".
    tree->setCurrentItem(listRow());
    QComboBox *listMib = view.findChild<QComboBox *>("stListMib");
    QVERIFY(listMib != nullptr && listMib->isVisibleTo(&view));
    QCOMPARE(listMib->currentText(), QString("Use show's (On)"));
    listMib->setCurrentIndex(2);
    QMetaObject::invokeMethod(listMib, "activated", Q_ARG(int, 2));
    QCOMPARE(st->listMoveInBlack(list), MoveInBlack::Off);
    QCOMPARE(st->undoText(), QString("Move in black: Off"));
    tree->expandAll();
    QVERIFY2(listRow()->text(1).contains("⊘ MIB"), qPrintable(listRow()->text(1)));
    QVERIFY(listRow()->toolTip(1).contains("Move in black Off for this list"));
    QVERIFY(st->undo());
    QCOMPARE(st->listMoveInBlack(list), MoveInBlack::Show);

    // Right-click ▸ Move in black ▸ On, with the show's off: badged.
    pc->setAutoMoveInBlack(false);
    {
        QMenu menu;
        view.listMibMenu(&menu, list);
        QCOMPARE(menu.actions().first()->text(), QString("Use show's (Off)"));
        QVERIFY(menu.actions().first()->isChecked());
        QVERIFY(trigger(&menu, "stListMibOn"));
    }
    QCOMPARE(st->listMoveInBlack(list), MoveInBlack::On);
    tree->expandAll();
    QVERIFY2(listRow()->text(1).contains("▸ MIB"), qPrintable(listRow()->text(1)));
    // The show's turned on: the list says what the show says -- no badge.
    pc->setAutoMoveInBlack(true);
    tree->expandAll();
    QVERIFY(!listRow()->text(1).contains("MIB"));

    // The cue page's "Don't move in black into this cue": badged on the row.
    QVERIFY(view.selectNode(c[1]));
    QCheckBox *noMib = view.findChild<QCheckBox *>("stCueNoMib");
    QVERIFY(noMib != nullptr && noMib->isVisibleTo(&view));
    QVERIFY(!noMib->isChecked());
    noMib->setChecked(true);
    QVERIFY(st->isCueNoMoveInBlack(c[1]));
    QCOMPARE(st->undoText(), QString("Don't move in black into cue"));
    tree->expandAll();
    QVERIFY(cueRow(tree, c[1])->text(ShowTreeView::FlagsCol).contains("no MIB"));
    QVERIFY(cueRow(tree, c[1])->toolTip(1).contains("Don't move in black into this cue"));
    QVERIFY(!cueRow(tree, c[0])->text(ShowTreeView::FlagsCol).contains("MIB"));
    QVERIFY(view.selectNode(c[1]));
    QVERIFY(view.findChild<QCheckBox *>("stCueNoMib")->isChecked());

    // The MIB button: still the show's, its tooltip names the overrides.
    MoveInBlackButton *mib = view.moveInBlackButton();
    QVERIFY(mib != nullptr);
    QVERIFY(mib->isChecked());
    QVERIFY2(mib->toolTip().contains("overridden by 1 cue list set On or Off and 1 cue marked"),
             qPrintable(mib->toolTip()));
    QVERIFY(st->undo());
    QVERIFY(!st->isCueNoMoveInBlack(c[1]));

    // Right-click a cue: the checkbox action, both ways.
    for (const bool was : { false, true })
    {
        bool seen = false, checked = false;
        ModalCloser closer([&](QWidget *w) {
            if (QMenu *m = qobject_cast<QMenu *>(w))
                for (QAction *act : m->actions())
                    if (act->objectName() == QLatin1String("stCueNoMib"))
                    {
                        seen = true;
                        checked = act->isChecked();
                        m->close();
                        act->trigger();
                        return true;
                    }
            return false;
        });
        QVERIFY(view.selectNode(c[2]));
        QTreeWidgetItem *r = cueRow(tree, c[2]);
        QMetaObject::invokeMethod(&view, "slotTreeContextMenu", Q_ARG(QPoint, tree->visualItemRect(r).center()));
        QVERIFY(seen);
        QCOMPARE(checked, was);
        QCOMPARE(st->isCueNoMoveInBlack(c[2]), !was);
    }

    // The command line: mib off list, mib show list, nomib / allowmib.
    QVERIFY(view.selectNode(c[0]));
    QVERIFY(view.runCommand("mib off list"));
    QCOMPARE(st->listMoveInBlack(list), MoveInBlack::Off);
    QVERIFY2(view.commandMessage().contains("move in black Off"), qPrintable(view.commandMessage()));
    QVERIFY(view.runCommand("mib show list"));
    QCOMPARE(st->listMoveInBlack(list), MoveInBlack::Show);
    QVERIFY(view.runCommand("nomib 1 thru 2"));
    QVERIFY(st->isCueNoMoveInBlack(c[0]) && st->isCueNoMoveInBlack(c[1]));
    QCOMPARE(st->undoText(), QString("nomib 1 thru 2"));
    QVERIFY(view.runCommand("allowmib 1, 2"));
    QVERIFY(!st->isCueNoMoveInBlack(c[0]) && !st->isCueNoMoveInBlack(c[1]));
    QVERIFY(!view.runCommand("mib sideways"));

    pc->setAutoMoveInBlack(false);
}

/* Branson: "for cuelist, and cues can we save a color for the text in the
   list as an option per cuelist or cue?" Colour ▸ on a list or cues (one
   undo step), the pages' Text colour buttons, the rows in their colour (a
   cue's own over its list's, a sub-cue its cue's; skipped stays grey), and
   the docked Workbench's header. */
void ShowTreeView_Test::textColoursInTheTreePagesAndWorkbench()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    QList<quint32> c;
    for (int i = 0; i < 4; ++i)
        c << st->addCue(list, i - 1, QString("C%1").arg(i + 1));
    st->setLayerLook(c[0], st->addLayer(c[0], groupTarget(towers->id())), st->lookForPalette(half));
    const quint32 sub = st->addSubCue(c[1], "Hit");
    st->clearUndo();

    ShowTreeView view(nullptr, m_doc);
    view.resize(1280, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    auto listRow = [tree, list]() -> QTreeWidgetItem * {
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::ListNode
                && (*it)->data(0, ShowTreeView::IdRole).toUInt() == list)
                return *it;
        return nullptr;
    };
    auto trigger = [](QMenu *menu, const QString &name) {
        for (QAction *a : menu->actions())
            if (a->objectName() == name)
            {
                a->trigger();
                return true;
            }
        return false;
    };
    const QColor themeText = cueRow(tree, c[0])->foreground(0).color();

    // The list: Colour ▸ Blue. Its row and every cue row without one.
    {
        QMenu menu;
        view.colourMenu(&menu, {}, list);
        QVERIFY(trigger(&menu, "stColourBlue"));
    }
    QCOMPARE(st->listColour(list), QString("#1e88e5"));
    QCOMPARE(st->undoText(), QString("Cue list colour"));
    tree->expandAll();
    const QColor blue = view.treeTextColour("#1e88e5");
    QVERIFY(blue.isValid() && blue != themeText);
    QCOMPARE(listRow()->foreground(0).color(), blue);
    for (quint32 id : c)
        QCOMPARE(cueRow(tree, id)->foreground(0).color(), blue);
    QCOMPARE(cueRow(tree, sub)->foreground(0).color(), blue);

    // The list's page: its Text colour button says Blue.
    tree->setCurrentItem(listRow());
    QToolButton *listButton = view.findChild<QToolButton *>("stListColour");
    QVERIFY(listButton != nullptr && listButton->isVisibleTo(&view));
    QCOMPARE(listButton->text(), QString("Blue"));

    // A cue's page: None (the list's: Blue); from its button's menu, Red.
    QVERIFY(view.selectNode(c[1]));
    QToolButton *cueButton = view.findChild<QToolButton *>("stCueColour");
    QVERIFY(cueButton != nullptr && cueButton->isVisibleTo(&view));
    QCOMPARE(cueButton->text(), QString("None (the list's: Blue)"));
    QMenu *menu = cueButton->menu();
    QVERIFY(menu != nullptr);
    emit menu->aboutToShow();
    QAction *none = menu->findChild<QAction *>("stColourNone");
    QVERIFY(none != nullptr && none->isChecked());
    QCOMPARE(none->text(), QString("None (the list's: Blue)"));
    QVERIFY(trigger(menu, "stColourRed"));
    QCOMPARE(st->cueColour(c[1]), QString("#e53935"));
    QCOMPARE(st->undoText(), QString("Cue colour"));
    QTRY_COMPARE(cueButton->text(), QString("Red"));
    tree->expandAll();
    const QColor red = view.treeTextColour("#e53935");
    QCOMPARE(cueRow(tree, c[1])->foreground(0).color(), red);
    QCOMPARE(cueRow(tree, sub)->foreground(0).color(), red);   // a sub-cue: its cue's
    QCOMPARE(cueRow(tree, c[2])->foreground(0).color(), blue);
    emit menu->aboutToShow();
    QVERIFY(menu->findChild<QAction *>("stColourRed")->isChecked());

    // Several selected, from the row's Colour ▸: one step for them all.
    tree->clearSelection();
    cueRow(tree, c[2])->setSelected(true);
    cueRow(tree, c[3])->setSelected(true);
    {
        QMenu several;
        view.colourMenu(&several, view.selectedCues());
        QVERIFY(trigger(&several, "stColourGreen"));
    }
    QCOMPARE(st->undoText(), QString("Colour of 2 cues"));
    QCOMPARE(st->cueColour(c[2]), QString("#43a047"));
    QCOMPARE(st->cueColour(c[3]), QString("#43a047"));
    QVERIFY(st->undo());
    QCOMPARE(st->cueColour(c[2]), QString("#1e88e5"));
    QCOMPARE(st->cueColour(c[3]), QString("#1e88e5"));
    tree->expandAll();
    QCOMPARE(cueRow(tree, c[3])->foreground(0).color(), blue);

    // Skipped: its grey wins over its colour.
    QVERIFY(view.setCuesSkipped({ c[1] }, true));
    tree->expandAll();
    QVERIFY(cueRow(tree, c[1])->foreground(0).color() != red);
    QVERIFY(view.setCuesSkipped({ c[1] }, false));

    // The docked Workbench: its header in the cue's colour.
    QVERIFY(view.selectNode(c[1]));
    QVERIFY(view.openSelectionInWorkbench());
    QLabel *title = view.dockedWorkbench()->findChild<QLabel *>("workbenchTitle");
    QVERIFY(title != nullptr);
    const QColor onHeader = readableOn(QColor("#e53935"), title->palette().color(QPalette::Window));
    QVERIFY2(title->styleSheet().contains(onHeader.name()), qPrintable(title->styleSheet()));
    QVERIFY(st->setCuesColour({ c[1] }, QString()));
    QTRY_VERIFY2(title->styleSheet().contains(QColor(readableOn(QColor("#1e88e5"),
                                                                 title->palette().color(QPalette::Window))).name()),
                 qPrintable(title->styleSheet()));

    const QString dir = qEnvironmentVariable("QLC_CUECOLOUR_SHOTS");
    if (!dir.isEmpty())
    {
        QVERIFY(st->setCuesColour({ c[0] }, "Orange"));
        QVERIFY(st->setCuesColour({ c[2] }, "Purple"));
        QVERIFY(st->setCuesColour({ c[1] }, "Red"));
        view.dockedWorkbench()->goBack();
        QTest::qWait(50);
        QVERIFY(view.selectNode(c[1]));
        tree->expandAll();
        QTest::qWait(50);
        QVERIFY(view.grab().save(dir + "/showtree.png"));
    }
}

/* Q59 (UX review problem 3): the command line stays, its three lines of
   syntax don't -- not even on an empty show. ? typed first in the line, the
   ? button beside it, or F1 in the line shows them; the choice is kept. */
void ShowTreeView_Test::commandSyntaxFoldsAwayAndIsRemembered()
{
    QSettings().remove(QStringLiteral("showtree/commandSyntax"));
    {
        ShowTreeView view(nullptr, m_doc);       // an empty show
        view.resize(1200, 800);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&view));
        QLineEdit *line = view.commandLine();
        QLabel *syntax = view.findChild<QLabel *>("stCommandHint");
        QToolButton *button = view.findChild<QToolButton *>("stCommandSyntaxButton");
        QVERIFY(line != nullptr && syntax != nullptr && button != nullptr);
        QVERIFY(line->isVisible());
        QVERIFY(button->isVisible());
        QVERIFY(!view.isCommandSyntaxShown());
        QVERIFY(!syntax->isVisible());
        QVERIFY(!button->isChecked());
        // The empty show's page: the way in, not "select".
        QVERIFY(view.findChild<QStackedWidget *>("stInspector") != nullptr);
        QLabel *empty = qobject_cast<QLabel *>(view.findChild<QStackedWidget *>("stInspector")->currentWidget());
        QVERIFY(empty != nullptr);
        QVERIFY2(empty->text().startsWith("Start with + Production"), qPrintable(empty->text()));
        // A picture of the empty show: QLC_COMMANDLINE_SHOT (a .png).
        if (qEnvironmentVariableIsSet("QLC_COMMANDLINE_SHOT"))
            view.grab().save(qEnvironmentVariable("QLC_COMMANDLINE_SHOT"));
        // The button: shown, and remembered.
        QTest::mouseClick(button, Qt::LeftButton);
        QVERIFY(view.isCommandSyntaxShown());
        QVERIFY(syntax->isVisible());
        QVERIFY(syntax->text().contains("cueonly"));
        QVERIFY(QSettings().value("showtree/commandSyntax").toBool());
        QTest::mouseClick(button, Qt::LeftButton);
        QVERIFY(!view.isCommandSyntaxShown());
        QVERIFY(!QSettings().value("showtree/commandSyntax").toBool());
        // F1 in the line: shown, again: hidden. The line keeps its text.
        line->setFocus();
        QTRY_VERIFY(line->hasFocus());
        line->setText("Towers");
        QTest::keyClick(line, Qt::Key_F1);
        QVERIFY(view.isCommandSyntaxShown());
        QVERIFY(button->isChecked());
        QCOMPARE(line->text(), QString("Towers"));
        QTest::keyClick(line, Qt::Key_F1);
        QVERIFY(!view.isCommandSyntaxShown());
        // ? typed later in a command: just a character.
        QTest::keyClicks(line, "?");
        QVERIFY(!view.isCommandSyntaxShown());
        // ? typed first: the syntax; Return still runs help.
        line->clear();
        QTest::keyClicks(line, "?");
        QVERIFY(view.isCommandSyntaxShown());
        QCOMPARE(line->text(), QString("?"));
        QTest::keyClick(line, Qt::Key_Return);
        QVERIFY(view.isHelpShown());
    }
    // Remembered for the next Show Tree (the next run).
    {
        ShowTreeView view(nullptr, m_doc);
        QVERIFY(view.isCommandSyntaxShown());
        view.setCommandSyntaxShown(false);
    }
    {
        ShowTreeView view(nullptr, m_doc);
        QVERIFY(!view.isCommandSyntaxShown());
    }
    QSettings().remove(QStringLiteral("showtree/commandSyntax"));
}

/* Q60 (UX review problem 8): a time typed on the cue page is kept when the
   focus leaves the field -- not only on Return. Clicking another cue in the
   tree, Tab, or the page moving on applies it to the cue it was typed for;
   Esc in the field puts it back. */
void ShowTreeView_Test::typedTimesAreKeptWhenFocusLeaves()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCueNumbered(list, "1", "One");
    const quint32 c2 = st->addCueNumbered(list, "2", "Two");
    const quint32 c3 = st->addCueNumbered(list, "3", "Three");

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QTreeWidget *tree = view.tree();
    QDoubleSpinBox *fadeIn = view.findChild<QDoubleSpinBox *>("stCueFadeIn");
    QDoubleSpinBox *fadeOut = view.findChild<QDoubleSpinBox *>("stCueFadeOut");
    QDoubleSpinBox *hold = view.findChild<QDoubleSpinBox *>("stCueHold");
    QVERIFY(fadeIn != nullptr && fadeOut != nullptr && hold != nullptr);
    auto clickCue = [&](quint32 cue) {
        QTreeWidgetItem *it = nullptr;
        for (QTreeWidgetItemIterator i(tree); *i && it == nullptr; ++i)
            if ((*i)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
                && (*i)->data(0, ShowTreeView::IdRole).toUInt() == cue)
                it = *i;
        QVERIFY(it != nullptr);
        const QRect r = tree->visualItemRect(it);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
    };
    auto typeIn = [&](QDoubleSpinBox *box, const QString &text) {
        box->setFocus();
        QTRY_VERIFY(box->hasFocus());
        box->selectAll();
        QTest::keyClicks(box, text);
    };

    // Fade in typed, no Return, then a click on another cue: kept, on the
    // cue it was typed for; the page shows the new cue with its own.
    QVERIFY(view.selectNode(c1));
    QCOMPARE(view.currentCueId(), c1);
    typeIn(fadeIn, "3");
    clickCue(c2);
    QCOMPARE(st->cue(c1)->fadeIn, 3000u);
    QCOMPARE(st->cue(c2)->fadeIn, 0u);
    QCOMPARE(view.currentCueId(), c2);
    QCOMPARE(fadeIn->value(), 0.0);
    // Hold then Fade out, Tab between: both kept.
    typeIn(hold, "2");
    QTest::keyClick(hold, Qt::Key_Tab);
    QCOMPARE(st->cue(c2)->hold, 2000u);
    typeIn(fadeOut, "4");
    clickCue(c3);
    QCOMPARE(st->cue(c2)->fadeOut, 4000u);
    QCOMPARE(st->cue(c3)->fadeOut, 0u);
    // The page moving on with no click (the keyboard, a command, the
    // Workbench): kept too.
    typeIn(fadeIn, "1.5");
    QVERIFY(view.selectNode(c1));
    QCOMPARE(st->cue(c3)->fadeIn, 1500u);
    QCOMPARE(st->cue(c1)->fadeIn, 3000u);
    // Stepped with the arrow keys, then a click away: kept.
    fadeOut->setFocus();
    QTRY_VERIFY(fadeOut->hasFocus());
    QTest::keyClick(fadeOut, Qt::Key_Up);
    clickCue(c2);
    QCOMPARE(st->cue(c1)->fadeOut, 1000u);
    // Each one undo step, as on Return.
    QVERIFY(st->canUndo());
    // Return still applies at once.
    typeIn(fadeIn, "5");
    QTest::keyClick(fadeIn, Qt::Key_Return);
    QCOMPARE(st->cue(c2)->fadeIn, 5000u);
    // Esc puts it back: nothing applied, not even when the focus goes.
    typeIn(fadeIn, "9");
    QTest::keyClick(fadeIn, Qt::Key_Escape);
    QCOMPARE(fadeIn->value(), 5.0);
    QCOMPARE(st->cue(c2)->fadeIn, 5000u);
    clickCue(c3);
    QCOMPARE(st->cue(c2)->fadeIn, 5000u);
    QCOMPARE(st->cue(c3)->fadeIn, 1500u);
    // Esc with nothing typed: the tab's own Esc, as before (no revert).
    QCOMPARE(view.currentCueId(), c3);
    hold->setFocus();
    QTest::keyClick(hold, Qt::Key_Escape);
    QCOMPARE(st->cue(c3)->hold, Function::infiniteSpeed());
}

/* "when testing a cue timing it doesn't always apply until you move away
   and come back": a time typed or stepped, with the focus still in the
   field, is what GO plays -- committed before the run takes the stage. */
void ShowTreeView_Test::aTimeNotYetLeftIsWhatGoPlays()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCueNumbered(list, "1", "One");
    st->addCueNumbered(list, "2", "Two");

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QDoubleSpinBox *fadeIn = view.findChild<QDoubleSpinBox *>("stCueFadeIn");
    QDoubleSpinBox *hold = view.findChild<QDoubleSpinBox *>("stCueHold");
    QVERIFY(fadeIn != nullptr && hold != nullptr);
    QVERIFY(view.selectNode(c1));

    // Typed, no Return, the focus still there: GO.
    fadeIn->setFocus();
    QTRY_VERIFY(fadeIn->hasFocus());
    fadeIn->selectAll();
    QTest::keyClicks(fadeIn, "2.5");
    QVERIFY(fadeIn->hasFocus());
    QCOMPARE(st->cue(c1)->fadeIn, 0u);
    const quint32 chaserId = st->cueList(list)->chaser;
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = chaserId;
    go.cue = QStringLiteral("1");
    QVERIFY(m_doc->showControl()->execute(go));
    QCOMPARE(st->cue(c1)->fadeIn, 2500u);
    const Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(chaserId));
    QVERIFY(chaser != nullptr);
    QCOMPARE(chaser->steps().first().fadeIn, 2500u);

    // Stepped with the arrow keys, then Play: applied too.
    m_doc->function(chaserId)->stop(FunctionParent::master());
    hold->setFocus();
    QTRY_VERIFY(hold->hasFocus());
    hold->selectAll();
    QTest::keyClicks(hold, "3");
    QToolButton *play = view.findChild<QToolButton *>("stPreviewPlay");
    QVERIFY(play != nullptr);
    play->click();
    QCOMPARE(st->cue(c1)->hold, 3000u);
}

void ShowTreeView_Test::aTimeTakesEitherDecimalMark()
{
    const QLocale german(QLocale::German, QLocale::Germany);
    QCOMPARE(german.decimalPoint(), QString(","));
    QCOMPARE(german.groupSeparator(), QString("."));

    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCueNumbered(list, "1", "One");

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QVERIFY(view.selectNode(c1));
    QDoubleSpinBox *fadeIn = view.findChild<QDoubleSpinBox *>("stCueFadeIn");
    QDoubleSpinBox *fadeOut = view.findChild<QDoubleSpinBox *>("stCueFadeOut");
    QVERIFY(fadeIn != nullptr && fadeOut != nullptr);
    auto typeIn = [&](QDoubleSpinBox *box, const QString &text) {
        box->setFocus();
        QTRY_VERIFY(box->hasFocus());
        box->selectAll();
        QTest::keyClicks(box, text);
        QTest::keyClick(box, Qt::Key_Return);
    };

    for (const QLocale &loc : { german, QLocale(QLocale::English, QLocale::UnitedStates), QLocale::c() })
    {
        fadeIn->setLocale(loc);
        fadeOut->setLocale(loc);
        typeIn(fadeIn, "1.5");
        QCOMPARE(st->cue(c1)->fadeIn, 1500u);
        typeIn(fadeOut, "2,25");
        QCOMPARE(st->cue(c1)->fadeOut, 2250u);
        // Shown with the mark this language writes.
        QVERIFY2(fadeIn->text().startsWith(QStringLiteral("1") + loc.decimalPoint() + QStringLiteral("5")),
                 qPrintable(fadeIn->text()));
        typeIn(fadeIn, "3");
        QCOMPARE(st->cue(c1)->fadeIn, 3000u);
    }

    // The fade fields everywhere else (a scene's, a look's) the same.
    FadeTimeSpin spin(nullptr, QStringLiteral("scene"));
    spin.setLocale(german);
    spin.setMs(0);
    for (const QString &typed : { QStringLiteral("1.5"), QStringLiteral("1,5") })
    {
        spin.setMs(0);
        spin.selectAll();
        QTest::keyClicks(&spin, typed);
        spin.interpretText();
        QCOMPARE(spin.ms(), 1500);
        QCOMPARE(spin.QAbstractSpinBox::text(), QStringLiteral("1,50 s"));
    }
    // Its word for the default is left alone.
    spin.setMs(-1);
    QCOMPARE(spin.QAbstractSpinBox::text(), QStringLiteral("scene"));
    QCOMPARE(spin.ms(), -1);
    QCOMPARE(SecondsSpinBox::withDecimalPoint(QStringLiteral("1,5"), QLocale::c()), QStringLiteral("1.5"));
    QCOMPARE(SecondsSpinBox::withDecimalPoint(QStringLiteral("1.5"), german), QStringLiteral("1,5"));
}

void ShowTreeView_Test::theHeaderFoldsUnderMore()
{
    ShowTreeView view(nullptr, m_doc);
    view.resize(1600, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QToolButton *more = view.findChild<QToolButton *>("stMore");
    QVERIFY(more != nullptr && more->menu() != nullptr);
    QStringList names;
    for (QAction *a : more->menu()->actions())
        names << a->text();
    QCOMPARE(names, QStringList({ "+ Production", "+ Sub-cue", "Merge", "Cue Sheet…" }));
    QVERIFY(!view.findChild<QToolButton *>("stNewSubCue")->isVisible());
    QVERIFY(!view.findChild<QToolButton *>("stMerge")->isVisible());

    // An empty show: + Production on the bar as well.
    QToolButton *prod = view.findChild<QToolButton *>("stNewProduction");
    QVERIFY(m_doc->showTree()->productions().isEmpty());
    QVERIFY(prod->isVisible());
    m_doc->showTree()->addProduction("Spring Show");
    QTRY_VERIFY(!prod->isVisible());     // the tree follows the controller
    // The menu still offers it.
    QVERIFY(more->menu()->actions().first()->isEnabled());

    // Q90: a show made here has nothing to upgrade, its lists' own chasers
    // included.
    QToolButton *upgrade = view.findChild<QToolButton *>("stUpgradeShow");
    QVERIFY(upgrade != nullptr);
    m_doc->showTree()->addCueList(m_doc->showTree()->productions().first().id, "01-Everybody Scream");
    QTest::qWait(50);
    QTRY_VERIFY(!upgrade->isVisible());
    // ... and a chaser of the show's own brings it back while it's there.
    Chaser *old = new Chaser(m_doc);
    old->setName("Old cues");
    QVERIFY(m_doc->addFunction(old));
    QTRY_VERIFY(upgrade->isVisible());
    QVERIFY(m_doc->deleteFunction(old->id()));
    QTRY_VERIFY(!upgrade->isVisible());
}

/* Q93: a cue's right-click keeps what acts on a selection. Duplicate (⌘D)
   is there once, not also "Duplicate cue"; the base cue and Include cue
   are on the cue's page; Mode is one submenu (Branson, 2026-10-29: "I
   don't seem to be able to make it non tracking"); + Production only on
   a production or empty space. */
void ShowTreeView_Test::aCuesRightClickIsShort()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "L");
    const quint32 c1 = st->addCue(list, -1, "One");
    st->addCue(list, 0, "Two");

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();

    auto menuOn = [&](const QPoint &pos) {
        QStringList texts;
        ModalCloser closer([&](QWidget *w) {
            if (QMenu *m = qobject_cast<QMenu *>(w))
            {
                for (QAction *a : m->actions())
                    if (!a->isSeparator())
                        texts << a->text();
                m->close();
                return true;
            }
            return false;
        });
        QMetaObject::invokeMethod(&view, "slotTreeContextMenu", Q_ARG(QPoint, pos));
        return texts;
    };

    QVERIFY(view.selectNode(c1));
    const QStringList cue = menuOn(tree->visualItemRect(cueRow(tree, c1)).center());
    QVERIFY2(!cue.isEmpty(), "no menu on a cue");
    QVERIFY2(cue.contains("Duplicate"), qPrintable(cue.join(" | ")));
    QVERIFY2(cue.contains("Mode"), qPrintable(cue.join(" | ")));
    for (const char *gone : { "Duplicate cue", "Make the base cue", "Not the base cue",
                              "Include cue", "Track", "Block", "Cue-only", "+ Production…" })
        QVERIFY2(!cue.contains(QString::fromUtf8(gone)), gone);

    // Empty space: + Production.
    const QStringList empty = menuOn(QPoint(10, tree->viewport()->height() - 5));
    QVERIFY2(empty.contains(QString::fromUtf8("+ Production…")), qPrintable(empty.join(" | ")));
}

/* CHANGELOG 0.2.2 (Q75), "One production, nothing to choose": no Current,
   switcher or "· main" with a single production, and GO plays its list. */
void ShowTreeView_Test::oneProductionHasNothingToChooseAndGoPlaysIt()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "Main", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    st->setLayerLook(c1, st->addLayer(c1, t), st->lookForPalette(full));
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QVERIFY(chaser != nullptr);

    ShowTreeView view(nullptr, m_doc);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QPushButton *current = view.findChild<QPushButton *>("stProdCurrent");
    QVERIFY(current != nullptr);
    QVERIFY(view.selectNode(prod));
    QVERIFY(!current->isVisible());
    for (const QString &row : treeText(view.tree()))
        QVERIFY2(!row.contains("current") && !row.contains(QString::fromUtf8("· main")), qPrintable(row));

    // GO (show control's, as ⌘↩ and the Run tab send it): its list plays.
    view.selectNode(list);                                  // no preview of the cue
    QVERIFY(m_doc->showControl()->execute({ ShowControl::Go, QString(), QString() }));
    const QHash<quint32, uchar> v = settleDmx(m_doc);         // the timer, stepped by hand
    QVERIFY(chaser->isRunning());
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[1]), 255);
    chaser->stop(FunctionParent::master());
    settleDmx(m_doc);

    // A second production: Current is back on the page.
    const quint32 prod2 = st->addProduction("Spring");
    st->addCueList(prod2, "Opener", ListMode::Tracking);
    QVERIFY(view.selectNode(prod));
    QVERIFY(current->isVisible());
    view.hide();
}

/* CHANGELOG 0.2.2: "Renumber this level… numbers a level ... and asks first
   while OSC/MSC is listening". */
void ShowTreeView_Test::renumberThisLevelAsksFirstWhileShowControlListens()
{
    const bool oscWas = ShowControlListener::oscEnabled(), mscWas = ShowControlListener::mscEnabled();
    const int portWas = ShowControlListener::oscPort(), deviceWas = ShowControlListener::mscDeviceId();
    auto restore = qScopeGuard([=]() { ShowControlListener::setSettings(oscWas, portWas, mscWas, deviceWas); });
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    st->addCue(list, 0, "Two");
    st->addCue(list, 1, "Three");
    auto numbers = [st, list]() {
        QStringList out;
        const CueList *l = st->cueList(list);
        for (int i = 0; i < l->cues.count(); ++i)
            out << st->cueNumber(*l, i);
        return out;
    };
    const QStringList before = numbers();
    QCOMPARE(before, QStringList({ "1", "2", "3" }));
    ShowTreeView view(nullptr, m_doc);

    // The dialog: from 10 in tens, OK; the question after it: @a yes.
    auto answer = [](bool yes) {
        return [yes](QWidget *w) {
            if (QMessageBox *mb = qobject_cast<QMessageBox *>(w))
            {
                mb->button(yes ? QMessageBox::Yes : QMessageBox::No)->click();
                return true;
            }
            QSpinBox *from = w->findChild<QSpinBox *>("stRenumberStart");
            QSpinBox *by = w->findChild<QSpinBox *>("stRenumberStep");
            QDialog *dlg = qobject_cast<QDialog *>(w);
            if (from == nullptr || by == nullptr || dlg == nullptr)
                return false;
            from->setValue(10);
            by->setValue(10);
            dlg->accept();
            return true;
        };
    };

    // OSC on: asked, and No changes nothing (not even an undo step).
    ShowControlListener::setSettings(true, portWas, false, deviceWas);
    const QString undoBefore = st->undoText();
    {
        ModalCloser no(answer(false));
        QVERIFY(!view.renumberLevel(c1));
        QCOMPARE(no.seen.count(), 2);
        QVERIFY2(no.seen.last().contains("Show control (OSC / MSC) is listening"), qPrintable(no.seen.last()));
        QVERIFY2(no.seen.last().contains(QString::fromUtf8("1 → 10")), qPrintable(no.seen.last()));
    }
    QCOMPARE(numbers(), before);
    QCOMPARE(st->undoText(), undoBefore);

    // MSC on: asked too; Yes renumbers.
    ShowControlListener::setSettings(false, portWas, true, deviceWas);
    {
        ModalCloser yes(answer(true));
        QVERIFY(view.renumberLevel(c1));
        QCOMPARE(yes.seen.count(), 2);
    }
    QCOMPARE(numbers(), QStringList({ "10", "20", "30" }));
    QVERIFY(st->undo());

    // Nothing listening: the dialog only.
    ShowControlListener::setSettings(false, portWas, false, deviceWas);
    {
        ModalCloser quiet(answer(true));
        QVERIFY(view.renumberLevel(c1));
        QCOMPARE(quiet.seen.count(), 1);
    }
    QCOMPARE(numbers(), QStringList({ "10", "20", "30" }));
}

/* CHANGELOG [0.2.2] "One word per idea, part 2": the busking Looks are now
   Hits -- "+ Hit, the Hits section, Adopt as a hit, Copy to a hit". A cue's
   right-click says Copy to a hit, a Library scene's says Adopt as a hit,
   neither says "look", and each one makes a hit. */
void ShowTreeView_Test::theHitMenusSayHit()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    Scene *old = new Scene(m_doc);
    old->setName("Old Busk");
    old->addFixtureGroup(towers->id());
    old->addPalette(full);
    m_doc->addFunction(old);
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "L", ListMode::Tracking);
    const quint32 one = st->addCue(list, -1, "One");
    QVERIFY(st->setLayerLook(one, st->addLayer(one, groupTarget(towers->id())), st->lookForPalette(full)));
    auto hits = [st, prod]() {
        QList<quint32> out;
        if (const CueList *l = st->cueList(st->looksSection(prod)))
            for (const Cue &c : l->cues)
                out << c.id;
        return out;
    };

    ShowTreeView view(nullptr, m_doc);
    view.resize(1400, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.setConfirm([](const QString &, const QString &, const QString &) { return true; });

    // Right-click: the action with @p name, its text read and triggered.
    auto viaMenu = [](const char *slot, QObject *target, const QPoint &pos, const QString &name) {
        QString text;
        QStringList all;
        ModalCloser closer([&](QWidget *w) {
            QMenu *menu = qobject_cast<QMenu *>(w);
            if (menu == nullptr)
                return false;
            for (QAction *a : menu->actions())
                all << a->text();
            for (QAction *a : menu->actions())
                if (a->objectName() == name)
                {
                    text = a->text();
                    menu->close();
                    a->trigger();
                    return true;
                }
            menu->close();
            return true;
        });
        QMetaObject::invokeMethod(target, slot, Q_ARG(QPoint, pos));
        return text.isEmpty() ? QStringLiteral("(none of: %1)").arg(all.join(" | ")) : text;
    };

    // A cue: Copy to a hit.
    view.tree()->expandAll();
    QTreeWidgetItem *row = cueRow(view.tree(), one);
    QVERIFY(row != nullptr);
    view.tree()->scrollToItem(row);
    QVERIFY(view.selectNode(one));
    const QList<quint32> before = hits();
    const QString copy = viaMenu("slotTreeContextMenu", &view,
                                 view.tree()->visualItemRect(row).center(), "stCopyToLook");
    QCOMPARE(copy, QString("Copy to a hit"));
    QVERIFY(!copy.contains("look", Qt::CaseInsensitive));
    const QList<quint32> afterCopy = hits();
    QCOMPARE(afterCopy.count(), before.count() + 1);
    QCOMPARE(st->cue(afterCopy.last())->name, QString("One"));

    // A scene in the Library: Adopt as a hit.
    QTreeWidgetItem *sceneRow = nullptr;
    for (QTreeWidgetItemIterator it(view.library()); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LibFunction
            && (*it)->data(0, ShowTreeView::IdRole).toUInt() == old->id())
            sceneRow = *it;
    QVERIFY(sceneRow != nullptr);
    for (QTreeWidgetItem *p = sceneRow->parent(); p != nullptr; p = p->parent())
        p->setExpanded(true);
    view.library()->scrollToItem(sceneRow);
    const QString adopt = viaMenu("slotLibraryContextMenu", &view,
                                  view.library()->visualItemRect(sceneRow).center(), "stAdoptAsLook");
    QCOMPARE(adopt, QString("Adopt as a hit"));
    QVERIFY(!adopt.contains("look", Qt::CaseInsensitive));
    const QList<quint32> afterAdopt = hits();
    QCOMPARE(afterAdopt.count(), afterCopy.count() + 1);
    QCOMPARE(st->cue(afterAdopt.last())->name, QString("Old Busk"));
}

/* CHANGELOG [0.2.2] Move in black: the show's switch is the Show Tree's MIB
   button, "MIB · n while pre-positioning". A running chaser whose next step
   lights a dark mover, the show's MIB on: the button reads "MIB · 1"; off,
   nothing pre-positioned, plain "MIB". */
void ShowTreeView_Test::theMibButtonCountsWhatItPrePositions()
{
    // A mover: Pan and a master Dimmer.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("MIB Mover");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    for (const auto &c : { qMakePair(QString("Pan"), QLCChannel::Pan),
                           qMakePair(QString("Dimmer"), QLCChannel::Intensity) })
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(c.first);
        ch->setGroup(c.second);
        def->addChannel(ch);
        mode->insertChannel(ch, def->channels().size() - 1);
    }
    mode->cacheHeads();
    def->addMode(mode);
    Fixture *mover = new Fixture(m_doc);
    mover->setFixtureDefinition(def, mode);
    mover->setUniverse(0);
    mover->setAddress(100);
    m_doc->addFixture(mover);

    // A chaser on its first (empty) step; the next lights the mover.
    Scene *current = new Scene(m_doc);
    current->setName("Current");
    m_doc->addFunction(current);
    Scene *next = new Scene(m_doc);
    next->setName("Next");
    next->setValue(mover->id(), 1, 200);
    next->setValue(mover->id(), 0, 128);
    m_doc->addFunction(next);
    Chaser *chaser = new Chaser(m_doc);
    chaser->setDuration(MasterTimer::tick() * 10);
    chaser->addStep(current->id());
    chaser->addStep(next->id());
    m_doc->addFunction(chaser);
    MasterTimer timer(m_doc);
    chaser->start(&timer, FunctionParent::master());
    timer.timerTick();
    QCOMPARE(chaser->currentStepIndex(), 0);

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    MoveInBlackButton *mib = view.moveInBlackButton();
    QVERIFY(mib != nullptr);
    ProgrammerController *pc = m_doc->programmer();
    pc->setMoveInBlackDarkGapMs(0);         // any lead time will do
    QCOMPARE(mib->text(), QString("MIB"));

    mib->click();                           // the show's MIB on
    QVERIFY(pc->isAutoMoveInBlack());
    QCOMPARE(mib->text(), QString::fromUtf8("MIB · 1"));
    QVERIFY2(mib->toolTip().contains("Pre-positioning 1 head"), qPrintable(mib->toolTip()));

    mib->click();                           // off: let go
    QVERIFY(!pc->isAutoMoveInBlack());
    QCOMPARE(mib->text(), QString("MIB"));

    chaser->stop(FunctionParent::master());
    timer.timerTick();
}

/* CHANGELOG [0.2.2] "The Show Tree's flags in columns" (Q104): S, T, R, M
   and F, "each header naming itself on hover ... Right-click the header to
   hide any; in a narrow pane they fold away before the names do." */
void ShowTreeView_Test::theFlagColumnsNameThemselvesHideAndFold()
{
    QSettings().remove("showtree/hiddenColumns");
    auto settingsBack = qScopeGuard([] { QSettings().remove("showtree/hiddenColumns"); });
    ShowTreeController *st = m_doc->showTree();
    st->addCue(st->addCueList(st->addProduction("P"), "L", ListMode::Tracking), -1, "A cue with a long enough name");
    ShowTreeView view(nullptr, m_doc);
    view.resize(1600, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    QHeaderView *header = tree->header();
    QSplitter *split = view.findChild<QSplitter *>("stSplitter");
    QVERIFY(split != nullptr && split->widget(1) == tree);
    split->setSizes({ 150, 1000, 450 });
    QTRY_VERIFY(tree->viewport()->width() > 900);

    // On hover: what the header view shows is the model's tooltip.
    const struct { int col; const char *starts; } names[] = {
        { ShowTreeView::SetsCol, "S — Sets." }, { ShowTreeView::TracksCol, "T — Tracks in." },
        { ShowTreeView::ReleasesCol, "R — Releases." }, { ShowTreeView::ModeCol, "M — Mode." },
        { ShowTreeView::FlagsCol, "F — Flags." } };
    for (const auto &n : names)
    {
        const QString tip = tree->model()->headerData(n.col, Qt::Horizontal, Qt::ToolTipRole).toString();
        QVERIFY2(tip.startsWith(QString::fromUtf8(n.starts)), qPrintable(tip));
        QVERIFY2(!header->isSectionHidden(n.col),       // a wide pane: all of them
                 qPrintable(QString("column %1 hidden at %2 px").arg(n.col).arg(tree->viewport()->width())));
    }

    // Right-click the header, untick "S — Sets": that column goes, and
    // stays gone in the next view.
    auto headerMenu = [&](const QString &entry, QStringList *seen) {
        ModalCloser closer([&](QWidget *w) {
            QMenu *menu = qobject_cast<QMenu *>(w);
            if (menu == nullptr)
                return false;
            for (QAction *a : menu->actions())
                *seen << a->text();
            for (QAction *a : menu->actions())
                if (a->text() == entry)
                {
                    menu->close();
                    a->trigger();
                    return true;
                }
            menu->close();
            return true;
        });
        emit header->customContextMenuRequested(QPoint(header->sectionViewportPosition(ShowTreeView::SetsCol) + 2,
                                                       header->height() / 2));
    };
    QStringList seen;
    headerMenu(QString::fromUtf8("S — Sets"), &seen);
    QCOMPARE(seen.count(), ShowTreeView::TreeColumnCount - ShowTreeView::SetsCol);
    QVERIFY2(seen.contains(QString::fromUtf8("F — Flags")), qPrintable(seen.join(" | ")));
    QVERIFY(header->isSectionHidden(ShowTreeView::SetsCol));
    QVERIFY(!header->isSectionHidden(ShowTreeView::TracksCol));
    {
        ShowTreeView again(nullptr, m_doc);
        again.resize(1600, 600);
        again.show();
        QVERIFY(QTest::qWaitForWindowExposed(&again));
        QVERIFY(again.tree()->header()->isSectionHidden(ShowTreeView::SetsCol));
    }
    seen.clear();
    headerMenu(QString::fromUtf8("S — Sets"), &seen);        // ticked again: back
    QVERIFY(!header->isSectionHidden(ShowTreeView::SetsCol));

    // A narrow tree pane: flag columns fold away, the names keep the room.
    QList<int> sizes = split->sizes();
    const int all = sizes.value(0) + sizes.value(1) + sizes.value(2);
    sizes[1] = 300;
    sizes[0] = (all - 300) / 2;
    sizes[2] = all - 300 - sizes[0];
    split->setSizes(sizes);
    QTRY_VERIFY(tree->viewport()->width() <= 320);
    int hidden = 0;
    for (int c = ShowTreeView::SetsCol; c < ShowTreeView::TreeColumnCount; ++c)
        if (header->isSectionHidden(c))
            ++hidden;
    QVERIFY2(hidden > 0, "every flag column still shown in a 300 px pane");
    const int room = tree->viewport()->width();
    QVERIFY2(header->sectionSize(ShowTreeView::NameCol) >= room / 2,
             qPrintable(QString("names %1 of %2").arg(header->sectionSize(ShowTreeView::NameCol)).arg(room)));
    // Wide again: they are all back.
    split->setSizes({ 150, 1000, 450 });
    QTRY_VERIFY(tree->viewport()->width() > 900);
    for (int c = ShowTreeView::SetsCol; c < ShowTreeView::TreeColumnCount; ++c)
        QVERIFY2(!header->isSectionHidden(c), qPrintable(QString::number(c)));
}

QTEST_MAIN(ShowTreeView_Test)

void ShowTreeView_Test::pickersDockInTheWindow()
{
    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QWidget *strip = view.findChild<QWidget *>("stDockedPicker");
    QVERIFY(strip != nullptr);
    QVERIFY(!view.dockedPickerOpen());

    // Return applies; the editor lives inside the view, not a window.
    int applied = 0;
    QSpinBox *spin = new QSpinBox;
    view.openDockedPicker(QStringLiteral("Dimmer level"), spin, spin, [&applied]() { ++applied; });
    QVERIFY(view.dockedPickerOpen());
    QVERIFY(strip->isVisible());
    QVERIFY(!spin->isWindow());
    QCOMPARE(spin->window(), static_cast<QWidget *>(&view));
    QCOMPARE(view.findChild<QLabel *>("stPickerTitle")->text(), QString("Dimmer level"));
    QTRY_VERIFY(spin->hasFocus());
    QTest::keyClick(spin, Qt::Key_Return);
    QCOMPARE(applied, 1);
    QVERIFY(!view.dockedPickerOpen());

    // Esc just closes.
    QSpinBox *spin2 = new QSpinBox;
    view.openDockedPicker(QStringLiteral("Dimmer level"), spin2, spin2, [&applied]() { ++applied; });
    QTRY_VERIFY(spin2->hasFocus());
    QTest::keyClick(spin2, Qt::Key_Escape);
    QCOMPARE(applied, 1);
    QVERIFY(!view.dockedPickerOpen());

    // One at a time: a second replaces the first; Cancel applies nothing.
    QSpinBox *first = new QSpinBox;
    view.openDockedPicker(QStringLiteral("One"), first, first, [&applied]() { applied += 10; });
    QPointer<QSpinBox> gone(first);
    QSpinBox *second = new QSpinBox;
    view.openDockedPicker(QStringLiteral("Two"), second, second, [&applied]() { applied += 100; });
    QTRY_VERIFY(gone.isNull());
    view.findChild<QPushButton *>("stPickerCancel")->click();
    QCOMPARE(applied, 1);
    QVERIFY(!view.dockedPickerOpen());

    // Apply runs the newest only.
    QSpinBox *third = new QSpinBox;
    view.openDockedPicker(QStringLiteral("Three"), third, third, [&applied]() { applied += 1000; });
    view.findChild<QPushButton *>("stPickerApply")->click();
    QCOMPARE(applied, 1001);
}

void ShowTreeView_Test::aSecondNewEffectPaletteStartsAfresh()
{
    m_doc->effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    LookEditor *ed = view.lookEditor();
    auto browser = [&view]() -> EffectBrowser * {
        for (EffectBrowser *b : view.findChildren<EffectBrowser *>())
            if (b->isVisible())
                return b;
        return nullptr;
    };
    // As a person does: type the name, click the row, Done.
    auto pick = [&](const QString &name) {
        EffectBrowser *b = browser();
        if (b == nullptr)
            return false;
        b->search()->setFocus();
        b->search()->selectAll();
        QTest::keyClicks(b->search(), name);
        QApplication::processEvents();
        for (int i = 0; i < b->list()->count(); ++i)
            if (b->list()->item(i)->text() == name)
            {
                const QRect r = b->list()->visualItemRect(b->list()->item(i));
                QTest::mouseClick(b->list()->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
                QApplication::processEvents();
                view.findChild<QPushButton *>("effectPickerDone")->click();
                return true;
            }
        return false;
    };

    const quint32 first = view.newPaletteFromLibrary(QLCPalette::Effect);
    QTRY_VERIFY(ed->effectPickerOpen());
    QVERIFY(pick("Color Chase"));
    QCOMPARE(m_doc->palette(first)->name(), QString("Color Chase"));

    // The second, made while the editor shows the first.
    const quint32 second = view.newPaletteFromLibrary(QLCPalette::Effect);
    QLCPalette *p = m_doc->palette(second);
    QTRY_VERIFY(ed->effectPickerOpen());
    QCOMPARE(p->name(), QString("New Effect"));
    QVERIFY(p->scriptPath().isEmpty());
    QVERIFY(pick("Sparkle"));
    QCOMPARE(p->name(), QString("Sparkle"));
    QCOMPARE(m_doc->palette(first)->name(), QString("Color Chase"));

    // A third with the editor closed first (↩ Back, Esc): the picker opens
    // by itself all the same.
    QTest::keyClick(&view, Qt::Key_Escape);
    QTRY_VERIFY(!ed->isVisible());
    const quint32 third = view.newPaletteFromLibrary(QLCPalette::Effect);
    QTRY_VERIFY(ed->effectPickerOpen());
    QCOMPARE(m_doc->palette(third)->name(), QString("New Effect"));
    QVERIFY(pick("Rainbow"));
    QCOMPARE(m_doc->palette(third)->name(), QString("Rainbow"));
}

/* ------------------------------------------------------------------ */
/* 0.3.1 (lyr): moving layers                                          */
/* ------------------------------------------------------------------ */

static QStringList layerTargets(const ShowTreeController *st, quint32 cue, ShowTreeView_Test *)
{
    QStringList out;
    for (const Layer &l : st->cue(cue)->layers)
        out << st->targetLabel(l.target);
    return out;
}

/* Branson (build 2593): "can't cut/paste a layer from places". ⌘X marks the
   layers, ⌘V on a cue moves them there -- one undo step for the pair; several
   selected layers go together; a reusable layer stays the same reusable. */
void ShowTreeView_Test::cutAndPasteMovesLayersInOneUndoStep()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    FixtureGroup *floor = group("Floor", 3, 1);
    const quint32 blue = colour(Qt::blue, "Blue");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    Target tt, tu, tf;
    tt.groups << towers->id();
    tu.groups << usts->id();
    tf.groups << floor->id();
    const quint32 lt = st->addLayer(c1, tt);
    const quint32 lu = st->addLayer(c1, tu);
    st->addLayer(c2, tf);
    st->setLayerLook(c1, lt, st->lookForPalette(blue));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();

    // Cut one layer of One, paste on cue Two: it is Two's now (looks and all).
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lt, c1));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Cut);
    QCOMPARE(st->cue(c1)->layers.count(), 2);               // cut alone takes nothing away
    clickRow(tree, rowOf(tree, ShowTreeView::CueNode, c2));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Paste);
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    QCOMPARE(layerTargets(st, c2, this), QStringList({ "Floor", "Towers" }));
    QVERIFY(st->cue(c2)->layer(lt) != nullptr);             // the same layer: id kept
    QVERIFY(st->cue(c2)->layer(lt)->look(KeyColour) != nullptr);
    // The clipboard is spent: a second paste does nothing.
    QTest::keySequence(tree, QKeySequence::Paste);
    // One undo puts it all back; redo moves it again.
    QVERIFY(st->undo());
    QCOMPARE(layerTargets(st, c1, this), QStringList({ "Towers", "USTs" }));
    QCOMPARE(layerTargets(st, c2, this), QStringList({ "Floor" }));
    QVERIFY(st->redo());
    QCOMPARE(layerTargets(st, c1, this), QStringList({ "USTs" }));
    QCOMPARE(layerTargets(st, c2, this), QStringList({ "Floor", "Towers" }));
    QVERIFY(st->undo());

    // Several selected layers move together, in their order.
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lt, c1));
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lu, c1), Qt::ControlModifier);
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Cut);
    clickRow(tree, rowOf(tree, ShowTreeView::CueNode, c2));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Paste);
    QVERIFY(st->cue(c1)->layers.isEmpty());
    QCOMPARE(layerTargets(st, c2, this), QStringList({ "Floor", "Towers", "USTs" }));
    QVERIFY(st->undo());
    QCOMPARE(int(st->cue(c1)->layers.count()), 2);

    // Copy and paste still copy: the original stays.
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lu, c1));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Copy);
    clickRow(tree, rowOf(tree, ShowTreeView::CueNode, c2));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Paste);
    QCOMPARE(int(st->cue(c1)->layers.count()), 2);
    QCOMPARE(layerTargets(st, c2, this), QStringList({ "Floor", "USTs" }));
    QVERIFY(st->undo());

    // A reusable layer pastes as the same reusable.
    const quint32 reusable = st->makeLayerReusable(c1, lu, "Back wash");
    QVERIFY(reusable != invalidId());
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lu, c1));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Cut);
    clickRow(tree, rowOf(tree, ShowTreeView::CueNode, c2));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Paste);
    QVERIFY(st->cue(c2)->layer(lu) != nullptr);
    QCOMPARE(st->cue(c2)->layer(lu)->reusable, reusable);
    QCOMPARE(st->reusableUses(reusable).count(), 1);

    // Cut takes layers only: cues are dragged, or copied.
    clickRow(tree, rowOf(tree, ShowTreeView::CueNode, c1));
    tree->setFocus();
    QVERIFY(!view.cutSelection());
}

/* Paste on a layer's row puts the layers after it; within a cue it is a
   reorder (Cut + Paste) -- and onto its own children's parent it stays whole. */
void ShowTreeView_Test::pasteOnALayerGoesAfterIt()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    FixtureGroup *floor = group("Floor", 3, 1);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    Target tt, tu, tf;
    tt.groups << towers->id();
    tu.groups << usts->id();
    tf.groups << floor->id();
    const quint32 lt = st->addLayer(c1, tt);
    const quint32 lu = st->addLayer(c1, tu);
    const quint32 lf = st->addLayer(c1, tf);
    const quint32 other = st->addLayer(c2, tt);

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();

    // Reorder: cut Floor, paste on Towers' row: Towers, Floor, USTs.
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lf, c1));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Cut);
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lt, c1));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Paste);
    QCOMPARE(layerTargets(st, c1, this), QStringList({ "Towers", "Floor", "USTs" }));
    QCOMPARE(st->cue(c1)->layers.at(1).id, lf);             // the same layer, not a copy
    QVERIFY(st->undo());
    QCOMPARE(layerTargets(st, c1, this), QStringList({ "Towers", "USTs", "Floor" }));
    QVERIFY(st->redo());
    QVERIFY(st->undo());

    // Into another cue, after a layer of it: copy pastes after it too.
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lu, c1));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Copy);
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, other, c2));
    tree->setFocus();
    QTest::keySequence(tree, QKeySequence::Paste);
    QCOMPARE(layerTargets(st, c2, this), QStringList({ "Towers", "USTs" }));
    QCOMPARE(int(st->cue(c1)->layers.count()), 3);
    QVERIFY(st->undo());

    // The engine: first place, and a child stays under its parent.
    const QList<QPair<quint32, quint32>> one { qMakePair(c1, lf) };
    QVERIFY(!st->moveLayers(one, c1, ShowTreeController::layerFirst).isEmpty());
    QCOMPARE(layerTargets(st, c1, this), QStringList({ "Floor", "Towers", "USTs" }));
    QVERIFY(st->undo());
    QString why;
    const quint32 child = st->addLayer(c1, tu);             // same target as USTs: sets there
    Q_UNUSED(child);
    // A layer cannot be moved after itself.
    QVERIFY(st->moveLayers(one, c1, lf, &why).isEmpty());
    QVERIFY(!why.isEmpty());
}

/* Dragging a layer row moves it (⌥ copies), onto a cue, a layer row (its
   upper half: before it), another cue's layer; a line shows the place. */
void ShowTreeView_Test::aDragMovesLayersAndAltCopiesThem()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    FixtureGroup *floor = group("Floor", 3, 1);
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 hit = st->addLook(prod, "Stab", "Hits");
    Target tt, tu, tf;
    tt.groups << towers->id();
    tu.groups << usts->id();
    tf.groups << floor->id();
    const quint32 lt = st->addLayer(c1, tt);
    const quint32 lu = st->addLayer(c1, tu);
    const quint32 lf = st->addLayer(c2, tf);
    st->setLayerRelease(c1, lu, true);

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    QLabel *hint = view.findChild<QLabel *>("stDropHint");
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        (*it)->setExpanded(true);

    auto mimeOf = [](const QList<QPair<quint32, quint32>> &refs) {
        QMimeData *mime = new QMimeData();
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        for (const auto &r : refs)
            ds << r.first << r.second;
        mime->setData(ShowTreeView::layerMimeType(), data);
        return mime;
    };
    auto dragOnto = [&](QMimeData *mime, QTreeWidgetItem *row, double frac, Qt::KeyboardModifiers mods,
                        bool drop, bool *accepted = nullptr) {
        tree->scrollToItem(row);
        const QRect r = tree->visualItemRect(row);
        const QPoint at(r.center().x(), r.top() + int(r.height() * frac));
        QDragEnterEvent enter(dragPos(at), Qt::MoveAction, mime, Qt::LeftButton, mods);
        QCoreApplication::sendEvent(tree->viewport(), &enter);
        QDragMoveEvent move(dragPos(at), Qt::MoveAction, mime, Qt::LeftButton, mods);
        QCoreApplication::sendEvent(tree->viewport(), &move);
        if (accepted != nullptr)
            *accepted = move.isAccepted();
        if (drop)
        {
            QDropEvent d(dragPos(at), Qt::MoveAction, mime, Qt::LeftButton, mods);
            QCoreApplication::sendEvent(tree->viewport(), &d);
        }
    };

    // A drag over a layer row's lower half: accepted, says what, and a line.
    QMimeData *m1 = mimeOf({ qMakePair(c1, lt) });
    bool ok = false;
    dragOnto(m1, rowOf(tree, ShowTreeView::LayerNode, lf, c2), 0.8, Qt::NoModifier, false, &ok);
    QVERIFY(ok);
    QVERIFY2(hint->text().startsWith("Move “Towers” into"), qPrintable(hint->text()));
    QVERIFY(!view.dropIndicator().isNull());
    QVERIFY(view.dropIndicator().height() <= 2);            // a line between rows
    QCOMPARE(view.dropIndicator().top(), tree->visualItemRect(rowOf(tree, ShowTreeView::LayerNode, lf, c2)).bottom() + 1);
    // Alt: copy.
    dragOnto(m1, rowOf(tree, ShowTreeView::LayerNode, lf, c2), 0.8, Qt::AltModifier, false, &ok);
    QVERIFY(ok);
    QVERIFY2(hint->text().startsWith("Copy “Towers” into"), qPrintable(hint->text()));
    // Over the cue row: a frame round it.
    dragOnto(m1, rowOf(tree, ShowTreeView::CueNode, c2), 0.5, Qt::NoModifier, false, &ok);
    QVERIFY(ok);
    QVERIFY(view.dropIndicator().height() > 4);
    // Over a list row: nowhere.
    QTreeWidgetItem *listRow = rowOf(tree, ShowTreeView::ListNode, list);
    QVERIFY(listRow != nullptr);
    dragOnto(m1, listRow, 0.5, Qt::NoModifier, false, &ok);
    QVERIFY(!ok);
    QVERIFY(view.dropIndicator().isNull());

    // Drop: moved after Floor in Two. One undo step.
    dragOnto(m1, rowOf(tree, ShowTreeView::LayerNode, lf, c2), 0.8, Qt::NoModifier, true);
    QCOMPARE(layerTargets(st, c1, this), QStringList({ "USTs" }));
    QCOMPARE(layerTargets(st, c2, this), QStringList({ "Floor", "Towers" }));
    QVERIFY(view.dropIndicator().isNull());
    QVERIFY(st->undo());
    QCOMPARE(layerTargets(st, c1, this), QStringList({ "Towers", "USTs" }));
    QCOMPARE(layerTargets(st, c2, this), QStringList({ "Floor" }));
    delete m1;

    // Upper half of the first layer: first place; reorders within the cue.
    QMimeData *m2 = mimeOf({ qMakePair(c1, lu) });
    dragOnto(m2, rowOf(tree, ShowTreeView::LayerNode, lt, c1), 0.2, Qt::NoModifier, true);
    QCOMPARE(layerTargets(st, c1, this), QStringList({ "USTs", "Towers" }));
    QVERIFY(st->undo());
    // Onto itself: nothing to do, nothing taken.
    dragOnto(m2, rowOf(tree, ShowTreeView::LayerNode, lu, c1), 0.8, Qt::NoModifier, true);
    QCOMPARE(layerTargets(st, c1, this), QStringList({ "Towers", "USTs" }));
    delete m2;

    // ⌥-drag copies: the original stays, an undo step.
    QMimeData *m3 = mimeOf({ qMakePair(c1, lt) });
    dragOnto(m3, rowOf(tree, ShowTreeView::CueNode, c2), 0.5, Qt::AltModifier, true);
    QCOMPARE(layerTargets(st, c1, this), QStringList({ "Towers", "USTs" }));
    QCOMPARE(layerTargets(st, c2, this), QStringList({ "Floor", "Towers" }));
    QVERIFY(st->undo());
    delete m3;

    // Two layers together, onto a hit: both move; the hit has no Release.
    QMimeData *m4 = mimeOf({ qMakePair(c1, lt), qMakePair(c1, lu) });
    dragOnto(m4, rowOf(tree, ShowTreeView::CueNode, hit), 0.5, Qt::NoModifier, false, &ok);
    QVERIFY(ok);
    QVERIFY2(hint->text().contains("2 layers into"), qPrintable(hint->text()));
    QVERIFY2(hint->text().contains("no Release"), qPrintable(hint->text()));
    dragOnto(m4, rowOf(tree, ShowTreeView::CueNode, hit), 0.5, Qt::NoModifier, true);
    QVERIFY(st->cue(c1)->layers.isEmpty());
    QCOMPARE(layerTargets(st, hit, this), QStringList({ "Towers", "USTs" }));
    for (const Layer &l : st->cue(hit)->layers)
        QVERIFY(!l.release);
    QVERIFY(st->undo());
    QVERIFY(st->cue(c1)->layer(lu)->release);               // undo brings Release back
    delete m4;
}

/* Why a drag only selected: a layer row carried no data, so Qt never began
   the drag. It carries (cue, layer) now -- the selected ones, together --
   and a press and release in place still only selects. */
void ShowTreeView_Test::aLayerRowIsDraggableAndAClickOnlySelects()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target tt, tu;
    tt.groups << towers->id();
    tu.groups << usts->id();
    const quint32 lt = st->addLayer(c1, tt);
    const quint32 lu = st->addLayer(c1, tu);

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    QTreeWidgetItem *row = rowOf(tree, ShowTreeView::LayerNode, lt, c1);
    QVERIFY(row != nullptr);
    QVERIFY(row->flags() & Qt::ItemIsDragEnabled);

    // A press and release without moving: selected, nothing moved.
    clickRow(tree, row);
    QCOMPARE(view.currentId(), lt);
    QCOMPARE(layerTargets(st, c1, this), QStringList({ "Towers", "USTs" }));
    // A small wobble (inside the platform's start distance) is a click too.
    QTreeWidgetItem *row2 = rowOf(tree, ShowTreeView::LayerNode, lu, c1);
    const QPoint p = tree->visualItemRect(row2).center();
    QTest::mousePress(tree->viewport(), Qt::LeftButton, Qt::NoModifier, p);
    QTest::mouseMove(tree->viewport(), p + QPoint(1, 1));
    QTest::mouseRelease(tree->viewport(), Qt::LeftButton, Qt::NoModifier, p + QPoint(1, 1));
    QCOMPARE(view.currentId(), lu);
    QCOMPARE(layerTargets(st, c1, this), QStringList({ "Towers", "USTs" }));

    // What a real drag carries: the selected layers.
    clickRow(tree, row);
    clickRow(tree, row2, Qt::ControlModifier);
    QMimeData *mime = view.selectionDragMime();
    QVERIFY(mime != nullptr);
    QVERIFY(mime->hasFormat(ShowTreeView::layerMimeType()));
    const QList<ShowTreeView::LayerRef> refs = ShowTreeView::layersFromMime(mime);
    QCOMPARE(refs.count(), 2);
    QCOMPARE(refs.first(), qMakePair(c1, lt));
    delete mime;
}

/* A hit never tracks: its layers have no Release. */
void ShowTreeView_Test::layersIntoAHitLoseTheirRelease()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 hit = st->addLook(prod, "Stab", "Hits");
    Target tt;
    tt.groups << towers->id();
    const quint32 lt = st->addLayer(c1, tt);
    QVERIFY(st->setLayerRelease(c1, lt, true));

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *tree = view.tree();
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, lt, c1));
    QVERIFY(view.copySelection());
    clickRow(tree, rowOf(tree, ShowTreeView::CueNode, hit));
    QVERIFY(view.pasteClipboard());
    QCOMPARE(st->cue(hit)->layers.count(), 1);
    QVERIFY(!st->cue(hit)->layers.first().release);
    QVERIFY(st->cue(c1)->layer(lt)->release);               // the original keeps its own
    // And back into a cue (cut): Release stays off, as it was in the hit.
    clickRow(tree, rowOf(tree, ShowTreeView::LayerNode, st->cue(hit)->layers.first().id, hit));
    QVERIFY(view.cutSelection());
    clickRow(tree, rowOf(tree, ShowTreeView::CueNode, c1));
    QVERIFY(view.pasteClipboard());
    QVERIFY(st->cue(hit)->layers.isEmpty());
}

/* efn: Runs on a Show Tree effect row is an undo step, shared palette or the
   tree's own one-off. */
void ShowTreeView_Test::effectRunsIsAnUndoStep()
{
    QLCPalette *fx = new QLCPalette(QLCPalette::Effect, m_doc);
    fx->setName("Confetti");
    m_doc->addPalette(fx);
    ShowTreeController *st = m_doc->showTree();
    const quint32 own = st->createInlinePalette(QLCPalette::Effect, QVariantList(), "Confetti · One Towers");
    QVERIFY(own != QLCPalette::invalidId());
    ShowTreeView view(nullptr, m_doc);

    for (quint32 pid : { fx->id(), own })
    {
        QLCPalette *p = m_doc->palette(pid);
        QVERIFY(p != nullptr);
        QVERIFY(!p->effectParamValues().contains(EffectRuns::runsKey()));
        EffectRunsMenu::apply(m_doc, { pid }, 3);
        QCOMPARE(p->effectParamValues().value(EffectRuns::runsKey(), -1), 3.0);
        QVERIFY(m_doc->paletteOps()->canUndo());
        view.slotUndo();
        QVERIFY2(!p->effectParamValues().contains(EffectRuns::runsKey()), "undo takes Runs back");
        view.slotRedo();
        QCOMPARE(p->effectParamValues().value(EffectRuns::runsKey(), -1), 3.0);
        view.slotUndo();
        // At the end and Length are steps too.
        EffectRunsMenu::apply(m_doc, { pid }, 1, 1);
        QCOMPARE(p->effectParamValues().value(EffectRuns::atEndKey(), -1), 1.0);
        view.slotUndo();
        QVERIFY(!p->effectParamValues().contains(EffectRuns::atEndKey()));
    }
    // Both at once (a layer's palettes): one step.
    EffectRunsMenu::apply(m_doc, { fx->id(), own }, 0);
    view.slotUndo();
    QVERIFY(!m_doc->palette(fx->id())->effectParamValues().contains(EffectRuns::runsKey()));
    QVERIFY(!m_doc->palette(own)->effectParamValues().contains(EffectRuns::runsKey()));
}

/* The command line's `release` takes a ▣ function layer, by its function's
   name, as it does a group. */
void ShowTreeView_Test::releaseTakesAFunctionLayer()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    Scene *haze = new Scene(m_doc);
    haze->setName("Haze look");
    m_doc->addFunction(haze);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target fn, tt;
    fn.function = haze->id();
    tt.groups << towers->id();
    const quint32 lf = st->addLayer(c1, fn);
    const quint32 lt = st->addLayer(c1, tt);

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.selectNode(c1));
    QVERIFY2(view.runCommand("release Haze look"), qPrintable(view.commandMessage()));
    QVERIFY(st->cue(c1)->layer(lf)->release);
    QVERIFY(!st->cue(c1)->layer(lt)->release);
    QVERIFY(view.runCommand("release Haze look"));            // toggles, as for a group
    QVERIFY(!st->cue(c1)->layer(lf)->release);
    QVERIFY(view.runCommand("release Towers"));
    QVERIFY(st->cue(c1)->layer(lt)->release);
    QVERIFY(!st->cue(c1)->layer(lf)->release);
    QVERIFY(!view.runCommand("release Nothing here"));
    QCOMPARE(int(st->cue(c1)->layers.count()), 2);
}

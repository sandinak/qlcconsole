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
#include <QDataStream>
#include <QTreeWidget>
#include <QDropEvent>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QMimeData>
#include <QXmlStreamReader>
#include <QtTest>

#include "showtreecontroller.h"
#include "functionstreewidget.h"
#include "fixturegroupsource.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "showtreeview.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "collection.h"
#include "chaserstep.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "showtreeview_test.h"

using namespace ShowTree;

/* The Show Tree tab (SHOW_TREE_DESIGN.md §6): the tree, the layer rows with
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
        if (!(*it)->text(1).isEmpty())
            line += QStringLiteral(" [%1]").arg((*it)->text(1));
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
    const quint32 list = st->addCueList(prod, "01-Everybody Scream");
    const quint32 c1 = st->addCue(list, -1, "PreCurtain");
    Target t;
    t.groups << towers->id();
    st->addLayer(c1, t);

    ShowTreeView view(nullptr, m_doc);
    const QStringList text = treeText(view.tree());
    QCOMPARE(text, QStringList({ "DanceHysteria", "  01-Everybody Scream [Non-tracking]",
                                 "    1.0.0 PreCurtain", "      Towers" }));

    // An edit elsewhere shows at once.
    st->addCue(list, 0, "Verse");
    QVERIFY(treeText(view.tree()).contains("    1.0.1 Verse"));

    QVERIFY(view.selectNode(c1));
    QCOMPARE(view.currentKind(), ShowTreeView::CueNode);
    QCOMPARE(view.currentCueId(), c1);
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
    QVERIFY2(text.contains("      ⤳ Towers [tracked from 1 One]"), qPrintable(text.join("\n")));

    // Selecting the tracked row shows it, read from where it was set.
    QTreeWidgetItem *tracked = nullptr;
    for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::TrackedLayerNode)
            tracked = *it;
    QVERIFY(tracked != nullptr);
    view.tree()->setCurrentItem(tracked);
    QCOMPARE(rowFor(view.attributeRows(), "Colour"), QString("Colour | Blue | tracked from 1 One"));
    QVERIFY(view.selectNode(c2));
    QCOMPARE(view.currentKind(), ShowTreeView::CueNode);
}

void ShowTreeView_Test::groupsDroppedOnACueBecomeLayers()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(list, -1, "One");

    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.dropTargets(c1, { towers->id(), usts->id() }, {}));
    QCOMPARE(st->cue(c1)->layers.count(), 2);
    QCOMPARE(view.currentKind(), ShowTreeView::LayerNode);      // the new layer is selected
    QVERIFY(view.dropTargets(c1, {}, { m_fixtures[4], m_fixtures[5] }));
    QCOMPARE(st->cue(c1)->layers.count(), 3);
    QCOMPARE(st->cue(c1)->layers.last().target.fixtures.count(), 2);
    QVERIFY(view.recentItems().first().startsWith("USTs"));
}

void ShowTreeView_Test::aPaletteOnTheCueIsACueLookAndTheHintSaysWhatItReaches()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    FixtureGroup *usts = group("USTs", 2, 2);
    const quint32 blue = colour(Qt::blue, "Blue");
    const quint32 red = colour(Qt::red, "Red");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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

    // Groups don't go on a layer.
    QMimeData groups;
    QByteArray g;
    QDataStream gs(&g, QIODevice::WriteOnly);
    gs << towers->id();
    groups.setData(FixtureGroupSource::fixtureGroupMimeType(), g);
    QVERIFY(view.acceptsDrop(c1, invalidId(), &groups));
    QVERIFY(!view.acceptsDrop(c1, l2, &groups));
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
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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

void ShowTreeView_Test::oneOffValuesGoToRecentNotAll()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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
}

void ShowTreeView_Test::undoAndRedoFromTheView()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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
    QDragEnterEvent enter(at, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &enter);
    QDragMoveEvent move(at, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &move);
    QVERIFY(move.isAccepted());
    QLabel *hint = view.findChild<QLabel *>("stDropHint");
    QCOMPARE(hint->text(), QString("1 new layer(s) in 1 One"));
    QDropEvent drop(at, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &drop);
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    QCOMPARE(hint->text(), QString());
    delete mime;
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
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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

    // Move a cue: the list's order and its chaser follow.
    const quint32 c2 = st->addCue(list, 0, "Two");
    QVERIFY(st->moveCue(c2, 0));
    QCOMPARE(st->cueList(list)->cues.first().id, c2);
    const Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QCOMPARE(chaser->steps().first().fid, st->cue(c2)->collection);
}

/* QLC_SHOWTREE_SHOW=<a COPY of a show> QLC_SHOWTREE_SHOT=<png>: load it,
   import its first chaser and picture the tab -- a look at the real thing,
   offscreen. Skipped without the variables. */
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
    const int index = qMin(2, int(list->cues.count()) - 1);
    const Cue &cue = list->cues.at(index);
    if (!cue.layers.isEmpty())
        view.selectNode(cue.layers.first().id, cue.id);
    else
        view.selectNode(cue.id);
    QTest::qWait(50);
    QVERIFY(view.grab().save(shot));
    view.selectNode(cue.id);
    QTest::qWait(50);
    QVERIFY(view.grab().save(QString(shot).replace(".png", "-cue.png")));
}

QTEST_MAIN(ShowTreeView_Test)

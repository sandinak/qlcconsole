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
#include <QSignalSpy>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QListWidget>
#include <QToolButton>
#include <QMimeData>
#include <QXmlStreamReader>
#include <QtTest>

#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private

#include "showtreecontroller.h"
#include "inputoutputmap.h"
#include "chaseraction.h"
#include "functionstreewidget.h"
#include "fixturegroupsource.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "showtreeview.h"
#include "lookeditor.h"
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
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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

    // Undo, and it is Towers alone again.
    view.slotUndo();
    QCOMPARE(st->cue(c1)->layers.first().target.groups, QList<quint32>({ towers->id() }));

    // An opaque layer takes no groups.
    Target fn;
    fn.function = scene->id();
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
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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
    QDragEnterEvent enter2(at2, Qt::CopyAction, two, Qt::LeftButton, Qt::AltModifier);
    QCoreApplication::sendEvent(tree->viewport(), &enter2);
    QDragMoveEvent altMove(at2, Qt::CopyAction, two, Qt::LeftButton, Qt::AltModifier);
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
    QDragEnterEvent enter3(at3, Qt::CopyAction, two, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &enter3);
    QDragMoveEvent onLayer(at3, Qt::CopyAction, two, Qt::LeftButton, Qt::NoModifier);
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

    // Move a cue (dragged onto another): the list's order and its chaser follow.
    const quint32 c2 = st->addCue(list, 0, "Two");
    QCOMPARE(view.cueDropHint(c2, c1, false), QString("Move 2 Two to 1 One's place"));
    QVERIFY(view.dropCue(c2, c1, false));
    QCOMPARE(st->cueList(list)->cues.first().id, c2);
    const Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QCOMPARE(chaser->steps().first().fid, st->cue(c2)->collection);
    // With Alt: an include.
    QCOMPARE(view.cueDropHint(c2, c1, true), QString("1 One includes 2 Two"));
    QVERIFY(view.dropCue(c2, c1, true));
    QCOMPARE(st->cue(c1)->includes, QList<quint32>({ c2 }));
    QVERIFY(!view.dropCue(c2, c2, false));
}

void ShowTreeView_Test::theLookEditorEditsARowsPalette()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 list = st->addCueList(st->addProduction("P"), "Full Show");
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

    // The list runs cue One at full, for real.
    goList(m_doc, chaser);
    QCOMPARE(int(settleDmx(m_doc)[0]), 255);

    // Design: select the running cue and edit its dimmer DOWN -- the edit
    // is what shows (HTP stacking alone would keep the running 255).
    QVERIFY(view.selectNode(l1, c1));
    QCOMPARE(st->previewedCue(), c1);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
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

void ShowTreeView_Test::trackingControlsOnTheListAndCuePages()
{
    FixtureGroup *towers = group("Towers", 0, 2);
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
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
    QVERIFY(treeText(view.tree()).contains("  L [Tracking]"));

    // The base from the list page...
    base->setCurrentIndex(1);
    emit base->activated(1);
    QCOMPARE(st->cueList(list)->baseCue, c1);
    QVERIFY(treeText(view.tree()).contains("    1 One [■ base]"));
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
    QVERIFY(treeText(view.tree()).contains("    2 Two [⊘ block]"));
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
    QCOMPARE(trackedRow(view.tree(), c3, "Towers")->text(1), QString("included from 1 One"));
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
    QVERIFY(treeText(view.tree()).contains("    3 Two (recorded) [⊘ block]"));
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
    const quint32 a = st->addCueList(prod, "A");
    const quint32 b = st->addCueList(prod, "B");
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
    view.setCommandLineVisible(false);
    QVERIFY(!view.isCommandLineVisible());
    QVERIFY(view.commandLine()->isHidden() || !view.commandLine()->isVisibleTo(&view));
    view.findChild<QToolButton *>("stCommandToggle")->click();
    QVERIFY(view.isCommandLineVisible());

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
    QVERIFY(st->isInlinePalette(st->cue(c2)->layers.first().look(KeyDimmer)->palettes.first()));
    // ↑ recalls.
    QTest::keyClick(line, Qt::Key_Up);
    QCOMPARE(line->text(), QString("Towers dimmer 40"));
    QTest::keyClick(line, Qt::Key_Up);
    QCOMPARE(line->text(), QString("cue + \"Verse\""));
    QTest::keyClick(line, Qt::Key_Down);
    QTest::keyClick(line, Qt::Key_Down);
    QVERIFY(line->text().isEmpty());

    // undo: the whole last command.
    QVERIFY(view.runCommand("undo"));
    QVERIFY(st->cue(c2)->layers.isEmpty());
    Q_UNUSED(towers)
    view.setCommandLineVisible(false);
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
        view.setCommandLineVisible(true);
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
    if (tracking)
        view.setCommandLineVisible(false);
}

QTEST_MAIN(ShowTreeView_Test)

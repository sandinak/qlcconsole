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
#include <QLabel>
#include <QCheckBox>
#include <QSplitter>
#include <QTextBrowser>
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
#undef protected
#undef private
#include "functionliveeditdialog.h"
#include "functionmanager.h"
#include "chasereditor.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QBuffer>
#include <QStackedWidget>
#include <QRegularExpression>

#include "showtreecontroller.h"
#include "paletteops.h"
#include "virtualconsole.h"
#include "vcbutton.h"
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
#include "linespreadpreview.h"
#include "lineplanview.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "qlcphysical.h"
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
    const quint32 list = st->addCueList(prod, "01-Everybody Scream", ListMode::NonTracking);
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
    QDragEnterEvent enter(at, Qt::CopyAction, mime, Qt::LeftButton, mods);
    QCoreApplication::sendEvent(w, &enter);
    QDragMoveEvent move(at, Qt::CopyAction, mime, Qt::LeftButton, mods);
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

    // Perform (Operate): its own setting -- still Ask.
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

    // Two cues dragged onto a third: refused, and the hint says why.
    QMimeData *mime = new QMimeData();
    QByteArray data;
    {
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds << c1 << c2;
    }
    mime->setData(ShowTreeView::cueMimeType(), data);
    QTreeWidgetItem *onto = rowOf(tree, ShowTreeView::CueNode, c3);
    tree->scrollToItem(onto);
    const QPoint at = tree->visualItemRect(onto).center();
    QDragEnterEvent enter(at, Qt::MoveAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &enter);
    QDragMoveEvent move(at, Qt::MoveAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &move);
    QVERIFY(!move.isAccepted());
    QVERIFY(view.findChild<QLabel *>("stDropHint")->text().contains("one cue at a time"));
    QDropEvent drop(at, Qt::MoveAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tree->viewport(), &drop);
    QCOMPARE(st->cueList(list)->indexOf(c1), 0);
    QCOMPARE(st->cueList(list)->indexOf(c2), 1);
    QCOMPARE(st->cueList(list)->indexOf(c3), 2);
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
        pm.addFunctionsToCollection(collId, { plain->id() });
        QVERIFY(!qobject_cast<const Collection *>(m_doc->function(collId))->functions().contains(plain->id()));
        QCOMPARE(closer.seen.count(), 2);
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
    two->addFunction(one->id());
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
    QVERIFY2(details.contains("0.0.2 Two: tracks from 0.0.1 One (its include of 0.0.1 One taken out)"),
             qPrintable(details));
    QCOMPARE(st->cueList(listId)->mode, ListMode::Tracking);
    QVERIFY(st->cueList(listId)->cues.at(1).includes.isEmpty());
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
 * R2: palettes in the Library (PROGRAMMING_ROLE_DESIGN.md §4.2, P-D8)
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
        // ⌫ in the Library, answered yes: gone; the cue says (missing).
        answer = true;
        QTest::keyClick(view.library(), Qt::Key_Backspace);
        QVERIFY(m_doc->palette(blue) == nullptr);
        QVERIFY(old->palettes().isEmpty());
        QVERIFY(view.selectNode(l1, c1));
        QVERIFY(rowFor(view.attributeRows(), "Colour").contains("(missing)"));
        // Undo: back, with its id, in the scene, and the cue plays it.
        view.findChild<QToolButton *>("stUndo")->click();
        QVERIFY(m_doc->palette(blue) != nullptr);
        QCOMPARE(old->palettes(), QList<quint32>({ blue }));
        QVERIFY(view.selectNode(l1, c1));
        QVERIFY(rowFor(view.attributeRows(), "Colour").contains("Blue"));
        // Redo deletes it again.
        view.findChild<QToolButton *>("stRedo")->click();
        QVERIFY(m_doc->palette(blue) == nullptr);
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

/****************************************************************************
 * R3: the Looks section (PROGRAMMING_ROLE_DESIGN.md §4.1, P-D5..P-D7)
 ****************************************************************************/

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
    QCOMPARE(view.findChild<QToolButton *>("stNewCue")->text(), QString("+ Look"));
    // Named on its page; groups dropped on it make a layer, as on a cue.
    QLineEdit *name = view.findChild<QLineEdit *>("stCueName");
    name->setText("Hit 1");
    emit name->editingFinished();
    QCOMPARE(st->cue(hit)->name, QString("Hit 1"));
    QVERIFY(view.dropTargets(hit, { towers->id() }, {}));
    const quint32 layer = st->cue(hit)->layers.first().id;
    QVERIFY(view.dropPalettes(hit, layer, { full }));
    QVERIFY(treeText(view.tree()).contains("  Looks [busking]"));
    QVERIFY(treeText(view.tree()).contains("    Hit 1"));
    // A look's page: no number, mode, includes or timing; who fires it.
    QVERIFY(view.selectNode(hit));
    QVERIFY(!view.findChild<QLineEdit *>("stCueNumber")->isVisibleTo(&view));
    QVERIFY(!view.findChild<QComboBox *>("stCueMode")->isVisibleTo(&view));
    QLabel *fired = view.findChild<QLabel *>("stLookFiredBy");
    QVERIFY(fired->isVisibleTo(&view));
    QVERIFY(fired->text().contains("no VC button yet"));
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
    // renamed in place, deleted (its looks move up).
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
            if (f->caption() == "Looks — Hits")
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
        QVERIFY(treeText(view.tree()).join("\n").contains("Red Hit [VC ×1]"));
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

QTEST_MAIN(ShowTreeView_Test)

/*
  Q Light Controller Plus - Test Unit
  programmingmanager_test.cpp

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
#include <QXmlStreamReader>
#include "qlcfile.h"
#include "scenetransition.h"
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSignalSpy>
#include <QMimeData>
#include <QDropEvent>
#include <QDataStream>
#include <QTreeWidgetItemIterator>
#include <QSplitter>
#include <QLayout>
#include <QScrollArea>
#include <QLabel>

#define protected public
#define private public
#include "fadetimespin.h"
#include "programmingmanager.h"
#include "scenegrouplooks.h"
#include "functionstreewidget.h"
#include "fixturegroupsource.h"
#include "lookeditor.h"
#include "scene.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "apputil.h"
#include "showstatus.h"
#include "qlcfixturedefcache.h"
#include <QRegularExpression>
#include <QDateTime>
#include <QTemporaryDir>
#include "showstatusdialog.h"
#include <QPushButton>
#include <QTreeWidget>
#include "doc.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "monitor/structurestudioview.h"
#undef private
#undef protected

#include "programmingmanager_test.h"

/* Real QMimeData for a palette, built the same way FunctionsTreeWidget's own
 * external drag would (m_paletteTree->setExternalDragMode(true) is set
 * unconditionally at construction -- see programmingmanager.cpp -- so its
 * mimeData() always returns the external payload, never QTreeWidget's
 * default). mimeData() is protected; reachable via the
 * "#define protected public" above the includes. */
static QMimeData *paletteMimeData(FunctionsTreeWidget *tree, QLCPalette *pal)
{
    QTreeWidgetItem *item = tree->paletteItem(pal);
    if (item == nullptr)
        return nullptr;
    return tree->buildMimeData(QList<QTreeWidgetItem *>() << item);
}

/* Real QMimeData for a fixture group or an individual fixture, built the
 * same way FixtureGroupSource's own drag would. Finds the source row by
 * walking the tree for the matching IdRole/KindRole pair -- there's no
 * public itemById() for a plain fixture leaf the way groupItemById() exists
 * for a group. */
static QTreeWidgetItem *fixtureGroupSourceItem(FixtureGroupSource *tree, quint32 id,
                                               FixtureGroupSource::NodeKind kind)
{
    QTreeWidgetItemIterator it(tree);
    for (; *it != nullptr; ++it)
    {
        if ((*it)->data(0, FixtureGroupSource::KindRole).toInt() == int(kind)
            && (*it)->data(0, FixtureGroupSource::IdRole).toUInt() == id)
            return *it;
    }
    return nullptr;
}

static QMimeData *fixtureGroupMimeData(FixtureGroupSource *tree, quint32 groupId)
{
    QTreeWidgetItem *item = fixtureGroupSourceItem(tree, groupId, FixtureGroupSource::GroupNode);
    if (item == nullptr)
        return nullptr;
    return tree->buildMimeData(QList<QTreeWidgetItem *>() << item);
}

static QMimeData *fixtureMimeData(FixtureGroupSource *tree, quint32 fixtureId)
{
    QTreeWidgetItem *item = fixtureGroupSourceItem(tree, fixtureId, FixtureGroupSource::FixtureNode);
    if (item == nullptr)
        return nullptr;
    return tree->buildMimeData(QList<QTreeWidgetItem *>() << item);
}

/* Open the Programming tab's canvas for @p sceneId with REAL, non-degenerate
 * geometry for m_targetList/m_lookList -- without this, mgr's top-level
 * QSplitter (a local variable in ProgrammingManager's constructor, not
 * exposed as a member the way FixtureManager::m_splitter is) never gets
 * explicit sizes, and every child widget stays stuck at Qt's default
 * unconfigured QRect(0,0 100x30) -- an unshown top-level widget's layout is
 * never activated by resize() alone the way FixtureGroupEditor's table was
 * in the Fixture Manager tests, because THAT test could reach the actual
 * splitter member and call setSizes() on it directly. Found via
 * findChild<QSplitter*>() instead, since ProgrammingManager has exactly one. */
static SceneGroupLooks *openCanvas(ProgrammingManager &mgr, quint32 sceneId)
{
    mgr.resize(1400, 900);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // resize() alone leaves every descendant's geometry at Qt's unconfigured
    // default (100x30): an unshown top-level widget's layout is never
    // activated by the normal resize-event cascade the way a visible
    // window's is. Force it explicitly, or the splitter (and everything
    // inside it) never gets real, non-degenerate geometry at all.
    mgr.layout()->activate();
    QSplitter *splitter = mgr.findChild<QSplitter *>();
    if (splitter != nullptr)
        splitter->setSizes(QList<int>() << 300 << 700 << 300);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    mgr.loadCanvas(sceneId);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // m_canvas is created fresh INSIDE loadCanvas(), after the splitter
    // above already settled -- it's a brand new child of an already-laid-out
    // ancestor, and the same activation gap applies to it individually: the
    // LayoutRequest its insertion posts never gets delivered for a hidden
    // widget under offscreen QTest. Force it directly on the canvas itself.
    if (mgr.m_canvas != nullptr)
    {
        mgr.m_canvas->resize(900, 700);
        mgr.m_canvas->layout()->activate();
        QCoreApplication::processEvents();
    }
    return mgr.m_canvas;
}

/* Deliver a synthetic drop straight to SceneGroupLooks::dropEvent() -- the
 * real internal Qt::QDrag session this simulates can't run under offscreen
 * QTest (no window manager, no real drop target to negotiate with), the
 * same reasoning as the Fixture Manager sub-group drag test earlier this
 * session. @p posInCanvas MUST be in the canvas ("this") widget's own local
 * coordinates, not m_targetList's/m_lookList's viewport coordinates --
 * dropEvent()'s own column-precision guard reads qlcEventPos(event)
 * directly against m_targetList->geometry()/m_lookList->geometry(), which
 * are themselves canvas-local (see scenegrouplooks.cpp's dropEvent()). */
static void deliverDrop(SceneGroupLooks *canvas, QMimeData *mime, const QPoint &posInCanvas)
{
    QVERIFY(mime != nullptr);
    QDropEvent drop(QPointF(posInCanvas), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    canvas->dropEvent(&drop);
    delete mime;
}

void ProgrammingManager_Test::init()
{
    m_doc = new Doc(this);
    /* Not this machine's cloud storage: a user folder it has not pinned
       leaves files online-only, which the libraries skip and the footer
       warns of -- an entry no test here expects. */
    QLCFile::forgetSkippedOnlineOnly();
    ShowStatus::instance()->clearStatus("storage.onlineonly");
}

void ProgrammingManager_Test::cleanup()
{
    delete m_doc;
}

void ProgrammingManager_Test::fixturesAndGroupsNarrowToASearch()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    int address = 0;
    auto group = [this, &address](const QString &name, const QStringList &members) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        g->setSize(QSize(members.size(), 1));
        m_doc->addFixtureGroup(g);
        foreach (const QString &m, members)
        {
            Fixture *f = new Fixture(m_doc);
            f->setName(m);
            f->setChannels(1);
            f->setAddress(quint32(address++));
            QVERIFY(m_doc->addFixture(f));
            g->assignFixture(f->id());
        }
    };
    group("CS1f", { "Step L1", "Step L2" });
    group("DS1f", { "Step R1", "Step R2" });
    group("AT-W-Top", { "Lantern SL", "Lantern SR" });

    ProgrammingManager mgr(nullptr, m_doc);
    FixtureGroupSource *tree = mgr.findChild<FixtureGroupSource *>();
    QLineEdit *search = mgr.findChild<QLineEdit *>(QStringLiteral("fixGroupFilter"));
    QVERIFY(tree != nullptr && search != nullptr);
    tree->reload();
    auto row = [tree](const QString &startsWith) -> QTreeWidgetItem * {
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->text(0).startsWith(startsWith))
                return *it;
        return nullptr;
    };
    auto shown = [&](const QString &startsWith) {
        QTreeWidgetItem *it = row(startsWith);
        for (QTreeWidgetItem *p = it; p != nullptr; p = p->parent())
            if (p->isHidden())
                return false;
        return it != nullptr;
    };
    QVERIFY(shown("CS1f") && shown("DS1f") && shown("AT-W-Top"));

    // A group by name: it, and all it holds.
    search->setText("cs1");
    QVERIFY(shown("CS1f"));
    QVERIFY(!shown("DS1f"));
    QVERIFY(!shown("AT-W-Top"));
    QVERIFY(shown("Step L1") && shown("Step L2"));

    // A fixture by name: its group opened to show it, its neighbours gone.
    search->setText("lantern sl");
    QVERIFY(shown("AT-W-Top"));
    QVERIFY(row("AT-W-Top")->isExpanded());
    QVERIFY(shown("Lantern SL"));
    QVERIFY(!shown("Lantern SR"));
    QVERIFY(!shown("CS1f"));

    // Kept when the tree is rebuilt.
    tree->reload();
    QVERIFY(shown("Lantern SL") && !shown("CS1f"));

    // Cleared: everything back, closed.
    search->clear();
    QVERIFY(shown("CS1f") && shown("DS1f") && shown("AT-W-Top"));
    QVERIFY(!row("AT-W-Top")->isExpanded());
}

void ProgrammingManager_Test::fromAGroupsViewTheSceneAndItsEffectComeBack()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    Fixture *fx = new Fixture(m_doc);
    fx->setName("Bar");
    fx->setChannels(3);
    QVERIFY(m_doc->addFixture(fx));
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Towers");
    grp->setSize(QSize(1, 1));
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->assignFixture(fx->id());
    QLCPalette *chase = new QLCPalette(QLCPalette::Effect, m_doc);
    chase->setName("Chase");
    QVERIFY(m_doc->addPalette(chase));
    Scene *scene = new Scene(m_doc);
    scene->setName("01-01.01-Opening");
    QVERIFY(m_doc->addFunction(scene));
    scene->addFixtureGroup(grp->id());
    scene->addPalette(chase->id());

    ProgrammingManager mgr(nullptr, m_doc);
    mgr.loadCanvas(scene->id());
    QVERIFY(mgr.m_canvas != nullptr);

    // The group's layout view: and a way back.
    mgr.loadGroupEditor(grp->id());
    QVERIFY(mgr.m_canvas == nullptr);
    QVERIFY(!mgr.m_groupBackButton->isHidden());
    QVERIFY(mgr.m_groupBackButton->text().contains("01-01.01-Opening"));

    // An effect opened from there: back on its scene, the effect showing.
    mgr.m_paletteTree->updateTree();
    QTreeWidgetItem *row = nullptr;
    for (QTreeWidgetItemIterator it(mgr.m_paletteTree); *it; ++it)
        if (mgr.m_paletteTree->itemPaletteId(*it) == chase->id())
            row = *it;
    QVERIFY(row != nullptr);
    mgr.slotPaletteDoubleClicked(row);
    QCOMPARE(mgr.m_currentScene, scene->id());
    QVERIFY(mgr.m_canvas != nullptr);
    QVERIFY(mgr.m_groupBackButton->isHidden());
    QVERIFY(!mgr.m_lookEditor->isHidden());
    QCOMPARE(mgr.m_lookEditor->paletteId(), chase->id());

    // And the button itself.
    mgr.loadGroupEditor(grp->id());
    mgr.m_groupBackButton->click();
    QCOMPARE(mgr.m_currentScene, scene->id());
    QVERIFY(mgr.m_canvas != nullptr);
}

void ProgrammingManager_Test::aScenesTransitionsAreSetOnItsCanvas()
{
    Scene *scene = new Scene(m_doc);
    scene->setName("01-01.01-Opening");
    QVERIFY(m_doc->addFunction(scene));
    ProgrammingManager mgr(nullptr, m_doc);
    mgr.loadCanvas(scene->id());
    QVERIFY(mgr.m_canvas != nullptr);
    QComboBox *in = mgr.m_canvas->findChild<QComboBox *>("transitionInShape");
    QComboBox *out = mgr.m_canvas->findChild<QComboBox *>("transitionOutShape");
    QDoubleSpinBox *inTime = mgr.m_canvas->findChild<QDoubleSpinBox *>("transitionInTime");
    QSpinBox *edge = mgr.m_canvas->findChild<QSpinBox *>("transitionEdge");
    QVERIFY(in && out && inTime && edge);
    QCOMPARE(in->currentIndex(), int(SceneTransition::None));     // the scene's fade, as always
    QVERIFY(!inTime->isEnabled());

    in->setCurrentIndex(int(SceneTransition::WipeToStageLeft));
    inTime->setValue(3.5);
    edge->setValue(20);
    out->setCurrentIndex(int(SceneTransition::Dissolve));
    QCOMPARE(scene->transitionIn().shape, SceneTransition::WipeToStageLeft);
    QCOMPARE(scene->transitionIn().time, 3500u);
    QCOMPARE(scene->transitionIn().softness, 0.2);
    QCOMPARE(scene->transitionOut().shape, SceneTransition::Dissolve);
    QVERIFY(inTime->isEnabled());

    // Shown again when the scene is opened again.
    mgr.loadCanvas(Function::invalidId());
    mgr.loadCanvas(scene->id());
    in = mgr.m_canvas->findChild<QComboBox *>("transitionInShape");
    QCOMPARE(in->currentIndex(), int(SceneTransition::WipeToStageLeft));
}

void ProgrammingManager_Test::filesInCloudStorageNotHereAreAWarning()
{
    auto entry = [](const QString &key) {
        foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
            if (e.key == key)
                return e;
        return ShowStatus::Entry();
    };
    QLCFile::forgetSkippedOnlineOnly();
    AppUtil::updateFixtureReadiness(m_doc);
    QVERIFY(entry("storage.onlineonly").key.isEmpty());

    QLCFile::noteSkippedOnlineOnly("/Users/x/Library/CloudStorage/SynologyDrive-Home/QLC+/Fixtures/Betopper-LM70.qxf");
    QLCFile::noteSkippedOnlineOnly("/Users/x/Library/CloudStorage/SynologyDrive-Home/QLC+/RGBScripts/fire.js");
    AppUtil::updateFixtureReadiness(m_doc);
    const ShowStatus::Entry e = entry("storage.onlineonly");
    QCOMPARE(e.severity, ShowStatus::Warning);
    QVERIFY2(e.summary.contains("2 file"), qPrintable(e.summary));
    QCOMPARE(e.items, QStringList() << "Betopper-LM70.qxf" << "fire.js");
    QCOMPARE(e.itemGroups, QStringList() << "Fixtures" << "RGBScripts");
    QVERIFY2(e.detail.contains("SynologyDrive"), qPrintable(e.detail));   // whose cloud it waits on

    QLCFile::forgetSkippedOnlineOnly();
    AppUtil::updateFixtureReadiness(m_doc);
    QVERIFY(entry("storage.onlineonly").key.isEmpty());
}

void ProgrammingManager_Test::fixturesWithoutTheirDefinitionsAreAnError()
{
    auto entry = [](const QString &key) {
        foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
            if (e.key == key)
                return e;
        return ShowStatus::Entry();
    };
    QXmlStreamReader in(QStringLiteral(
        "<Fixture><Manufacturer>Warmdance</Manufacturer><Model>XL-450</Model>"
        "<Mode>16 Channel</Mode><ID>7</ID><Name>Mover SL</Name><Universe>0</Universe>"
        "<Address>100</Address><Channels>16</Channels></Fixture>"));
    in.readNextStartElement();
    QVERIFY(Fixture::loader(in, m_doc));
    AppUtil::updateFixtureReadiness(m_doc);
    const ShowStatus::Entry e = entry("fixtures.nodefinition");
    QCOMPARE(e.severity, ShowStatus::Error);
    QCOMPARE(e.items, QStringList() << "Mover SL");
    QCOMPARE(e.itemGroups, QStringList() << "Warmdance XL-450");
    m_doc->deleteFixture(7);
    AppUtil::updateFixtureReadiness(m_doc);
    QVERIFY(entry("fixtures.nodefinition").key.isEmpty());
}

void ProgrammingManager_Test::effectLooksThatCannotRunAreInTheFooter()
{
    auto entry = [](const QString &key) {
        foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
            if (e.key == key)
                return e;
        return ShowStatus::Entry();
    };
    QLCPalette *none = new QLCPalette(QLCPalette::Effect, m_doc);
    none->setName("Nothing chosen");
    QVERIFY(m_doc->addPalette(none));
    QLCPalette *gone = new QLCPalette(QLCPalette::Effect, m_doc);
    gone->setName("Lost script");
    gone->setScriptPath("/nowhere/at/all/sparkle.js");
    QVERIFY(m_doc->addPalette(gone));
    QLCPalette *unused = new QLCPalette(QLCPalette::Effect, m_doc);
    unused->setName("In no scene");
    QVERIFY(m_doc->addPalette(unused));

    // Only effect looks a scene uses count.
    AppUtil::updateFixtureReadiness(m_doc);
    QVERIFY(entry("effects.failed").key.isEmpty());

    Scene *scene = new Scene(m_doc);
    QVERIFY(m_doc->addFunction(scene));
    scene->addPalette(none->id());
    scene->addPalette(gone->id());
    AppUtil::updateFixtureReadiness(m_doc);
    ShowStatus::Entry e = entry("effects.failed");
    QCOMPARE(e.severity, ShowStatus::Warning);
    QVERIFY2(e.summary.contains("2 effect look"), qPrintable(e.summary));
    QStringList items = e.items;
    items.sort();
    QCOMPARE(items.count(), 2);
    QCOMPARE(items.at(0), QString("Lost script — its script is not on this computer (sparkle.js)"));
    QCOMPARE(items.at(1), QString("Nothing chosen — no effect is chosen"));

    scene->removePalette(none->id());
    scene->removePalette(gone->id());
    AppUtil::updateFixtureReadiness(m_doc);
    QVERIFY(entry("effects.failed").key.isEmpty());
}

void ProgrammingManager_Test::loadProblemsAreInTheFooter()
{
    auto entry = [](const QString &key) {
        foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
            if (e.key == key)
                return e;
        return ShowStatus::Entry();
    };
    AppUtil::updateLoadProblems(QString(
        "Unable to add fixture <b>Wash 3</b>: address out of range<br>"
        "No fixture definition found for <b>Warmdance XL-450</b><br>"
        "Video file <i>intro.mp4</i> not found<br>"));
    const ShowStatus::Entry e = entry("show.load");
    QCOMPARE(e.severity, ShowStatus::Warning);
    QCOMPARE(e.items, QStringList() << "Unable to add fixture Wash 3: address out of range"
                                    << "Video file intro.mp4 not found");
    QVERIFY2(e.summary.contains("2 problem"), qPrintable(e.summary));

    // The next load starts clean.
    AppUtil::updateLoadProblems(QString("No fixture definition found for <b>X</b><br>"));
    QVERIFY(entry("show.load").key.isEmpty());
}

void ProgrammingManager_Test::aScenesTimingIsAllOnItsCanvas()
{
    Scene *scene = new Scene(m_doc);
    scene->setName("01-01.02-Verse");
    QVERIFY(m_doc->addFunction(scene));
    ProgrammingManager mgr(nullptr, m_doc);
    mgr.loadCanvas(scene->id());
    FadeTimeSpin *fadeIn = mgr.m_canvas->findChild<FadeTimeSpin *>("sceneFadeIn");
    FadeTimeSpin *fadeOut = mgr.m_canvas->findChild<FadeTimeSpin *>("sceneFadeOut");
    QComboBox *transIn = mgr.m_canvas->findChild<QComboBox *>("transitionInShape");
    QVERIFY(fadeIn && fadeOut && transIn);

    // The scene's own fade, set here.
    fadeIn->setValue(2.5);
    fadeOut->setValue(1.25);
    QCOMPARE(scene->fadeInSpeed(), 2500u);
    QCOMPARE(scene->fadeOutSpeed(), 1250u);

    // A transition in stands in for the fade in.
    QVERIFY(fadeIn->isEnabled());
    transIn->setCurrentIndex(int(SceneTransition::WipeUp));
    QVERIFY(!fadeIn->isEnabled());
    QVERIFY(fadeOut->isEnabled());
    transIn->setCurrentIndex(int(SceneTransition::None));
    QVERIFY(fadeIn->isEnabled());

    // Shown as it is when the scene opens again.
    mgr.loadCanvas(Function::invalidId());
    mgr.loadCanvas(scene->id());
    QCOMPARE(mgr.m_canvas->findChild<FadeTimeSpin *>("sceneFadeIn")->ms(), 2500);
}

void ProgrammingManager_Test::theCanvasAndTheLookEditorAgreeOnFades()
{
    QLCPalette *effect = new QLCPalette(QLCPalette::Effect, m_doc);
    effect->setName("Chase");
    QVERIFY(m_doc->addPalette(effect));
    QLCPalette *feeds = new QLCPalette(QLCPalette::Color, m_doc);
    feeds->setName("Blue");
    feeds->setValue(QColor(0, 0, 255));
    QVERIFY(m_doc->addPalette(feeds));
    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, m_doc);
    dim->setName("Half");
    dim->setValue(128);
    QVERIFY(m_doc->addPalette(dim));
    Scene *scene = new Scene(m_doc);
    QVERIFY(m_doc->addFunction(scene));
    scene->addPalette(dim->id());           // a base look
    scene->addPalette(effect->id());
    scene->addPalette(feeds->id());          // after the effect: its colour

    // The rule.
    QVERIFY(LookFades::hasFadeIn(m_doc, scene, dim->id()) && LookFades::hasFadeOut(m_doc, scene, dim->id()));
    QVERIFY(!LookFades::hasFadeIn(m_doc, scene, effect->id()) && LookFades::hasFadeOut(m_doc, scene, effect->id()));
    QVERIFY(!LookFades::hasFadeIn(m_doc, scene, feeds->id()) && !LookFades::hasFadeOut(m_doc, scene, feeds->id()));

    ProgrammingManager mgr(nullptr, m_doc);
    mgr.loadCanvas(scene->id());
    LookEditor *ed = mgr.m_lookEditor;

    // A base look: both fades, defaulting to the scene's.
    ed->setPalette(dim->id());
    QVERIFY(!ed->m_fadeRow->isHidden());
    QVERIFY(!ed->m_fadeInSpin->isHidden() && !ed->m_fadeOutSpin->isHidden());
    QCOMPARE(ed->m_fadeInSpin->specialValueText(), QString("scene"));

    // An effect: only its release, and left alone it snaps -- as on the canvas,
    // where the effect's Fade Out cell is the one that edits.
    ed->setPalette(effect->id());
    QVERIFY(!ed->m_fadeRow->isHidden());
    QVERIFY(ed->m_fadeInSpin->isHidden());
    QVERIFY(!ed->m_fadeOutSpin->isHidden());
    QCOMPARE(ed->m_fadeOutLabel->text(), QString("Release"));
    QCOMPARE(ed->m_fadeOutSpin->specialValueText(), QString("snap"));
    ed->m_fadeOutSpin->setValue(1.5);
    QCOMPARE(scene->paletteFadeOut(effect->id()), 1500);

    // A colour feeding the effect: no fades at all.
    ed->setPalette(feeds->id());
    QVERIFY(ed->m_fadeRow->isHidden());
}

void ProgrammingManager_Test::editingALookColourNotifiesItsScene()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Probe");
    fxi->setChannels(4);
    QVERIFY(m_doc->addFixture(fxi));

    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, m_doc);
    dim->setName("Full");
    dim->setValue(255);
    QVERIFY(m_doc->addPalette(dim));

    QLCPalette *col = new QLCPalette(QLCPalette::Color, m_doc);
    col->setName("White");
    col->setValue(QColor(255, 255, 255));
    QVERIFY(m_doc->addPalette(col));

    Scene *scene = new Scene(m_doc);
    scene->setName("Edited Look");
    scene->addFixture(fxi->id());
    scene->addPalette(dim->id());
    scene->addPalette(col->id());
    QVERIFY(m_doc->addFunction(scene));

    ProgrammingManager mgr(nullptr, m_doc);
    QVERIFY(mgr.m_lookEditor != nullptr);
    mgr.m_lookEditor->setPalette(col->id());
    QCOMPARE(mgr.m_lookEditor->paletteId(), col->id());

    QSignalSpy spy(m_doc, &Doc::functionChanged);

    // What the real colour picker in the look editor does when dragged --
    // not a manually-injected Doc::functionChanged like a shallower test
    // would use, which proves nothing about whether the real UI action
    // actually reaches the scene.
    mgr.m_lookEditor->slotColorChanged(QColor(0, 0, 255));

    QVERIFY2(spy.count() > 0,
             "Doc::functionChanged never fired -- editing the look's colour "
             "through the real editor left the scene looking unchanged to "
             "anything listening for it (e.g. the rig view)");
    bool sawScene = false;
    for (const QList<QVariant> &call : spy)
        if (call.at(0).toUInt() == scene->id())
            sawScene = true;
    QVERIFY2(sawScene, "functionChanged fired, but not for the scene that "
                        "actually uses the edited palette");
}

/* The Programming tab's own construction gestures -- CLAUDE.md: "Drag
 * palettes→looks, groups→dynamic targets, fixtures→fixed targets" -- had no
 * test anywhere driving the real drop (WORKFLOW-SUBTLETIES.md #6, accepted
 * as a real but speculative gap; this closes it now that it's the thing
 * being worked on). Each step below builds real QMimeData the same way the
 * real source trees (FunctionsTreeWidget, FixtureGroupSource) build it and
 * delivers it via a synthetic QDropEvent straight to
 * SceneGroupLooks::dropEvent() -- the real internal Qt::QDrag session this
 * simulates can't run under offscreen QTest, the same reasoning as the
 * Fixture Manager sub-group drag test earlier this session.
 */
void ProgrammingManager_Test::buildingALookThroughRealProgrammingTabDragAndDropStaysConsistent()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());

    Fixture *fixedFx = new Fixture(m_doc);
    fixedFx->setName("Fixed Target");
    fixedFx->setChannels(4);
    QVERIFY(m_doc->addFixture(fixedFx));

    Fixture *groupFx = new Fixture(m_doc);
    groupFx->setName("Group Member");
    groupFx->setChannels(4);
    groupFx->setAddress(4);
    QVERIFY(m_doc->addFixture(groupFx));

    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Wash Group");
    grp->setSize(QSize(1, 1));
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->assignFixture(groupFx->id());

    QLCPalette *col = new QLCPalette(QLCPalette::Color, m_doc);
    col->setName("Blue");
    col->setValue(QColor(0, 0, 255));
    QVERIFY(m_doc->addPalette(col));

    QLCPalette *fx = new QLCPalette(QLCPalette::Effect, m_doc);
    fx->setName("Followspot");
    fx->setScriptPath("Palettes/Effect/Position/Followspot/");
    QVERIFY(m_doc->addPalette(fx));

    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, m_doc);
    dim->setName("Half");
    dim->setValue(128);
    QVERIFY(m_doc->addPalette(dim));

    Scene *scene = new Scene(m_doc);
    scene->setName("Drag Built Look");
    QVERIFY(m_doc->addFunction(scene));

    // 0. Open the tab and load the (empty) scene into the real canvas --
    //    ProgrammingManager::loadCanvas(), the exact function selecting a
    //    scene in the nav tree calls.
    ProgrammingManager mgr(nullptr, m_doc);
    SceneGroupLooks *canvas = openCanvas(mgr, scene->id());
    QVERIFY2(canvas != nullptr, "loadCanvas() did not build a SceneGroupLooks canvas");

    // 1. Drag the Colour palette from the real palette tree onto empty
    //    space in the Looks column -- the base-look gesture.
    QMimeData *colMime = paletteMimeData(mgr.m_paletteTree, col);
    QVERIFY2(colMime != nullptr, "the colour palette has no row in the real palette tree");
    deliverDrop(canvas, colMime, canvas->m_lookList->geometry().center());
    QCoreApplication::processEvents();   // reconcileAfterPaletteApply()'s deferred timer
    QVERIFY2(scene->palettes().contains(col->id()),
             "dropping the colour palette onto the Looks list did not add it to the scene");

    // 2. Drag the fixture group from the real fixtures & groups tree onto
    //    the Targets list -- a dynamic target.
    QMimeData *grpMime = fixtureGroupMimeData(mgr.m_fixGroupSource, grp->id());
    QVERIFY2(grpMime != nullptr, "the group has no row in the real fixtures & groups tree");
    deliverDrop(canvas, grpMime, canvas->m_targetList->geometry().center());
    QVERIFY2(scene->fixtureGroups().contains(grp->id()),
             "dropping the group onto the Targets list did not add it as a dynamic target");

    // 3. Drag an individual fixture onto the Targets list -- a fixed
    //    target (only accepted because loadCanvas() built this canvas with
    //    includeFixtureTargets=true, unlike the classic scene editor).
    QMimeData *fxTargetMime = fixtureMimeData(mgr.m_fixGroupSource, fixedFx->id());
    QVERIFY2(fxTargetMime != nullptr, "the fixture has no row in the real fixtures & groups tree");
    deliverDrop(canvas, fxTargetMime, canvas->m_targetList->geometry().center());
    QVERIFY2(scene->fixtures().contains(fixedFx->id()),
             "dropping the fixture onto the Targets list did not add it as a fixed target");

    // 4. Drag the Effect palette onto empty Looks space -- with no other
    //    effect yet, it becomes a base look too, same as the colour.
    QMimeData *fxPalMime = paletteMimeData(mgr.m_paletteTree, fx);
    QVERIFY2(fxPalMime != nullptr, "the effect palette has no row in the real palette tree");
    deliverDrop(canvas, fxPalMime, canvas->m_lookList->geometry().center());
    QCoreApplication::processEvents();
    QVERIFY2(scene->palettes().contains(fx->id()),
             "dropping the effect palette onto the Looks list did not add it to the scene");

    // 5. Drag the Dimmer palette directly onto the effect's OWN row -- must
    //    nest UNDER it (feed it) rather than land as another base look.
    auto findLookRow = [&](quint32 pid) -> QTreeWidgetItem * {
        QTreeWidgetItemIterator it(canvas->m_lookList);
        for (; *it != nullptr; ++it)
            if ((*it)->data(0, Qt::UserRole).toUInt() == pid)
                return *it;
        return nullptr;
    };
    QTreeWidgetItem *effectItem = findLookRow(fx->id());
    QVERIFY2(effectItem != nullptr, "the effect palette never got a row in the Looks list");
    const QPoint effectRowInCanvas = canvas->m_lookList->viewport()->mapTo(
        canvas, canvas->m_lookList->visualItemRect(effectItem).center());

    QMimeData *dimMime = paletteMimeData(mgr.m_paletteTree, dim);
    QVERIFY2(dimMime != nullptr, "the dimmer palette has no row in the real palette tree");
    deliverDrop(canvas, dimMime, effectRowInCanvas);
    QCoreApplication::processEvents();
    QVERIFY2(scene->palettes().contains(dim->id()),
             "dropping the dimmer onto the effect row did not add it to the scene");

    // moveLooksToTarget() rebuilt the tree (see its own "Rebuilds the tree"
    // doc comment) -- effectItem above is dead the moment that ran. Re-find
    // both rows fresh rather than dereference a stale pointer.
    QTreeWidgetItem *effectItemNow = findLookRow(fx->id());
    QTreeWidgetItem *dimItemNow = findLookRow(dim->id());
    QVERIFY2(effectItemNow != nullptr, "the effect row is gone after the nested drop");
    QVERIFY2(dimItemNow != nullptr, "the dimmer never got a row in the Looks list");
    QVERIFY2(dimItemNow->parent() == effectItemNow,
             qPrintable(QString("dropping the dimmer on the effect row did not nest it -- "
                                "landed as a %1 instead of a child of the effect")
                        .arg(dimItemNow->parent() == nullptr ? "top-level base look"
                                                              : "child of something else")));
}

/* SceneGroupLooks::dropEvent()'s column-precision guard: the two-column
 * layout visually promises "drag here means this," so a palette (-> Looks)
 * dropped squarely on the Targets column, or a fixture group (-> Targets)
 * dropped squarely on the Looks column, is rejected -- not silently
 * accepted anywhere the way a naive "just add whatever it is" drop handler
 * would. The guard reads qlcEventPos(event) against
 * m_targetList->geometry()/m_lookList->geometry(), both canvas-local -- get
 * that coordinate space wrong (viewport-local instead) and a test could
 * "pass" for the wrong reason, seeing nothing happen because the drop
 * landed off both widgets entirely, not because the guard caught it. This
 * test rules that out explicitly: the SAME mime, dropped in the SAME
 * column, immediately afterward, must succeed.
 */
void ProgrammingManager_Test::droppingOnTheWrongColumnIsRejected()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Member");
    fxi->setChannels(4);
    QVERIFY(m_doc->addFixture(fxi));

    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Some Group");
    grp->setSize(QSize(1, 1));
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->assignFixture(fxi->id());

    QLCPalette *col = new QLCPalette(QLCPalette::Color, m_doc);
    col->setName("Green");
    col->setValue(QColor(0, 255, 0));
    QVERIFY(m_doc->addPalette(col));

    Scene *scene = new Scene(m_doc);
    scene->setName("Column Guard");
    QVERIFY(m_doc->addFunction(scene));

    ProgrammingManager mgr(nullptr, m_doc);
    SceneGroupLooks *canvas = openCanvas(mgr, scene->id());
    QVERIFY(canvas != nullptr);

    // A palette dropped on the TARGETS column: rejected, nothing changes.
    QMimeData *colMimeWrong = paletteMimeData(mgr.m_paletteTree, col);
    QVERIFY(colMimeWrong != nullptr);
    deliverDrop(canvas, colMimeWrong, canvas->m_targetList->geometry().center());
    QCoreApplication::processEvents();
    QVERIFY2(scene->palettes().isEmpty(),
             "a palette dropped on the Targets column was accepted -- the "
             "column-precision guard let it through");

    // The SAME palette, dropped on the LOOKS column right after: succeeds.
    // Rules out "the coordinates just miss both widgets" as the reason the
    // drop above did nothing.
    QMimeData *colMimeRight = paletteMimeData(mgr.m_paletteTree, col);
    QVERIFY(colMimeRight != nullptr);
    deliverDrop(canvas, colMimeRight, canvas->m_lookList->geometry().center());
    QCoreApplication::processEvents();
    QVERIFY2(scene->palettes().contains(col->id()),
             "the same palette, dropped on the correct column right after, still "
             "did not land -- the earlier rejection was not the guard working, "
             "the drop coordinates just never reach either widget");

    // A fixture group dropped on the LOOKS column: rejected, nothing changes.
    QMimeData *grpMimeWrong = fixtureGroupMimeData(mgr.m_fixGroupSource, grp->id());
    QVERIFY(grpMimeWrong != nullptr);
    deliverDrop(canvas, grpMimeWrong, canvas->m_lookList->geometry().center());
    QVERIFY2(scene->fixtureGroups().isEmpty(),
             "a fixture group dropped on the Looks column was accepted -- the "
             "column-precision guard let it through");

    // The SAME group, dropped on the TARGETS column right after: succeeds.
    QMimeData *grpMimeRight = fixtureGroupMimeData(mgr.m_fixGroupSource, grp->id());
    QVERIFY(grpMimeRight != nullptr);
    deliverDrop(canvas, grpMimeRight, canvas->m_targetList->geometry().center());
    QVERIFY2(scene->fixtureGroups().contains(grp->id()),
             "the same group, dropped on the correct column right after, still "
             "did not land");
}

/* scenegrouplooks.cpp's group-drop branch strips a fixed target's baked
 * values that the group's own looks now cover, and drops the fixed target
 * entirely once nothing uncovered is left on it
 * (WORKFLOW-SUBTLETIES.md-style finding from surveying this code: the check
 * is `keep.isEmpty()`, built ONLY from the fixture's existing baked
 * SceneValues -- a freshly-added fixed target with zero baked values has
 * nothing to iterate, so `keep` never gets anything added to it and stays
 * empty regardless of whether the group's looks cover anything at all. A
 * fixture added as an individual target and never given a single value
 * disappears from Targets the instant its group is dropped in, even with no
 * palette applied to either).
 */
void ProgrammingManager_Test::droppingAGroupFoldsAwayAnAlreadyCoveredFixedTarget()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Shared Member");
    fxi->setChannels(4);
    QVERIFY(m_doc->addFixture(fxi));

    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Covering Group");
    grp->setSize(QSize(1, 1));
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->assignFixture(fxi->id());

    Scene *scene = new Scene(m_doc);
    scene->setName("Overlap");
    QVERIFY(m_doc->addFunction(scene));

    ProgrammingManager mgr(nullptr, m_doc);
    SceneGroupLooks *canvas = openCanvas(mgr, scene->id());
    QVERIFY(canvas != nullptr);

    // 1. Drop the fixture individually first -- a fixed target with no
    //    baked values at all (nothing was ever set on it).
    QMimeData *fxMime = fixtureMimeData(mgr.m_fixGroupSource, fxi->id());
    QVERIFY(fxMime != nullptr);
    deliverDrop(canvas, fxMime, canvas->m_targetList->geometry().center());
    QVERIFY2(scene->fixtures().contains(fxi->id()),
             "the individual fixture drop did not add it as a fixed target");
    bool hasBakedValue = false;
    foreach (const SceneValue &scv, scene->values())
        if (scv.fxi == fxi->id())
            hasBakedValue = true;
    QVERIFY2(hasBakedValue == false,
             "test setup assumption broken: the fixed target already has a "
             "baked value, which is supposed to be the point of this scenario");

    // 2. Drop the group that shares this fixture as a member. The fixed
    //    target must fold away -- the group now covers the same fixture,
    //    and it had nothing uncovered to keep it around for.
    QMimeData *grpMime = fixtureGroupMimeData(mgr.m_fixGroupSource, grp->id());
    QVERIFY(grpMime != nullptr);
    deliverDrop(canvas, grpMime, canvas->m_targetList->geometry().center());
    QVERIFY2(scene->fixtureGroups().contains(grp->id()),
             "dropping the group did not add it as a dynamic target");
    QVERIFY2(scene->fixtures().contains(fxi->id()) == false,
             "the individual fixed target survived the group drop -- it should "
             "have folded away (group now covers the same fixture with nothing "
             "of its own left uncovered)");
}

/* A 540/270 moving head with pan and tilt only. */
static Fixture *makeMover(Doc *doc, QLCFixtureDef *def)
{
    def->setManufacturer("Test");
    def->setModel("Mover");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    for (int k = 0; k < 2; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(k == 0 ? "Pan" : "Tilt");
        ch->setGroup(k == 0 ? QLCChannel::Pan : QLCChannel::Tilt);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    def->addMode(mode);
    Fixture *fxi = new Fixture(doc);
    fxi->setName("Mover");
    fxi->setFixtureDefinition(def, mode);
    return doc->addFixture(fxi) ? fxi : nullptr;
}

void ProgrammingManager_Test::aNewPanTiltIsTriedOnTheLookSoTheRigFollows()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    QLCFixtureDef *def = new QLCFixtureDef();
    Fixture *mover = makeMover(m_doc, def);
    QVERIFY(mover != nullptr);

    QLCPalette *base = new QLCPalette(QLCPalette::PanTilt, m_doc);
    base->setName("Base");
    base->setValue(90, 45);
    QVERIFY(m_doc->addPalette(base));
    Scene *look = new Scene(m_doc);
    look->setName("Spots");
    look->addFixture(mover->id());
    look->addPalette(base->id());
    QVERIFY(m_doc->addFunction(look));

    StructureStudioView rig(m_doc, StructureStudioView::StageKind, 0);
    rig.setActiveScene(look->id());
    rig.rebuildAimTargets();
    auto rigPan = [&]() {
        const QByteArray v = rig.m_sceneValues.value(mover->id());
        return v.isEmpty() ? -1 : int(uchar(v.at(0)));
    };
    const int basePan = rigPan();

    {
        ProgrammingManager mgr(nullptr, m_doc);
        QVERIFY(openCanvas(mgr, look->id()) != nullptr);
        QVERIFY(mgr.m_lookEditor->trialNote().isEmpty());

        // "New Pan/Tilt" from the palette list: it opens in the editor,
        // and the look on the canvas tries it on.
        const int before = m_doc->palettes().size();
        mgr.createPalette(QLCPalette::PanTilt);
        QCOMPARE(m_doc->palettes().size(), before + 1);
        const quint32 pid = mgr.m_lookEditor->paletteId();
        QLCPalette *fresh = m_doc->palette(pid);
        QVERIFY(fresh != nullptr && fresh->type() == QLCPalette::PanTilt);
        QCOMPARE(look->trialPalette(), pid);
        QVERIFY(!look->palettes().contains(pid));
        // Its new "Palettes/PanTilt" folder didn't rename the look (id 0).
        QCOMPARE(look->name(), QString("Spots"));
        QVERIFY2(mgr.m_lookEditor->trialNote().contains("Spots"),
                 qPrintable(mgr.m_lookEditor->trialNote()));

        // Moving the pad moves the rig: the look is re-resolved with the trial.
        QSignalSpy changed(m_doc, &Doc::functionChanged);
        mgr.m_lookEditor->slotPanTiltChanged(QPointF(200.0, 100.0));
        QVERIFY(changed.count() > 0);
        QCOMPARE(changed.last().at(0).toUInt(), look->id());
        rig.rebuildAimTargets();
        const QList<SceneValue> want = fresh->valuesFromFixtures(m_doc, QList<quint32>() << mover->id());
        QVERIFY(!want.isEmpty());
        QCOMPARE(rigPan(), int(want.first().value));
        QVERIFY2(rigPan() != basePan, "the rig still shows the look's own position");

        // Dropped INTO the look for real: no longer a trial, no note.
        look->addPalette(pid);
        mgr.slotCanvasModified();
        QCOMPARE(look->trialPalette(), QLCPalette::invalidId());
        QVERIFY(mgr.m_lookEditor->trialNote().isEmpty());
        look->removePalette(pid);

        // Editing one of the look's own palettes tries nothing on.
        mgr.m_lookEditor->setPalette(base->id());
        QCOMPARE(look->trialPalette(), QLCPalette::invalidId());
        QVERIFY(mgr.m_lookEditor->trialNote().isEmpty());

        // Back to the loose one, then away from the look: the trial ends.
        mgr.m_lookEditor->setPalette(pid);
        QCOMPARE(look->trialPalette(), pid);
        mgr.loadCanvas(Function::invalidId());
        QCOMPARE(look->trialPalette(), QLCPalette::invalidId());

        // No look on the canvas: nothing to try it on, and the editor says so.
        mgr.m_lookEditor->setPalette(pid);
        QVERIFY2(mgr.m_lookEditor->trialNote().contains("Not in a look"),
                 qPrintable(mgr.m_lookEditor->trialNote()));
    }
    rig.rebuildAimTargets();
    QCOMPARE(rigPan(), basePan);                    // nothing was saved into it
    QCOMPARE(look->palettes(), QList<quint32>() << base->id());
    m_doc->deleteFixture(mover->id());
    delete def;
}

void ProgrammingManager_Test::aRenamedFixedTargetShowsItsNewName()
{
    Fixture *spot = new Fixture(m_doc);
    spot->setName("Spot A");
    spot->setChannels(4);
    QVERIFY(m_doc->addFixture(spot));
    Fixture *other = new Fixture(m_doc);
    other->setName("Not In It");
    other->setChannels(4);
    other->setAddress(10);
    QVERIFY(m_doc->addFixture(other));
    Scene *look = new Scene(m_doc);
    look->addFixture(spot->id());
    QVERIFY(m_doc->addFunction(look));

    SceneGroupLooks canvas(look, m_doc, nullptr, true);
    auto listed = [&canvas](const QString &name) {
        for (QTreeWidgetItemIterator it(canvas.m_targetList); *it != nullptr; ++it)
            if ((*it)->text(0).contains(name))
                return true;
        return false;
    };
    QVERIFY(listed("Spot A"));
    spot->setName("Downstage Spot");
    QCoreApplication::processEvents();
    QVERIFY(listed("Downstage Spot"));
    QVERIFY(!listed("Spot A"));
    // A fixture this look doesn't use changes nothing here.
    other->setName("Still Not In It");
    QCoreApplication::processEvents();
    QVERIFY(!listed("Still Not In It"));
}

/* "everytime I click a fader in the color picker it drops out of the color
   picker": a look edit reloads the canvas, which re-announced the selected
   group, which swapped the look editor for the fixtures' faders. */
void ProgrammingManager_Test::theColourPickerStaysOpenWhileItsLookIsEdited()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    QList<quint32> fids;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("USt %1").arg(i + 1));
        f->setChannels(3);
        f->setAddress(quint32(i) * 3);
        QVERIFY(m_doc->addFixture(f));
        fids << f->id();
    }
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("USt");
    grp->setSize(QSize(2, 1));
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->assignFixture(fids[0], QLCPoint(0, 0));
    grp->assignFixture(fids[1], QLCPoint(1, 0));

    QLCPalette *col = new QLCPalette(QLCPalette::Color, m_doc);
    col->setName("Blue");
    col->setValue(QColor(0, 0, 200));
    QVERIFY(m_doc->addPalette(col));

    Fixture *solo = new Fixture(m_doc);       // a fixed target of its own
    solo->setName("Solo");
    solo->setChannels(3);
    solo->setAddress(20);
    QVERIFY(m_doc->addFixture(solo));

    Scene *scene = new Scene(m_doc);
    scene->setName("USt test");
    scene->addFixtureGroup(grp->id());
    scene->addFixture(solo->id());
    scene->addPalette(col->id());
    QVERIFY(m_doc->addFunction(scene));

    ProgrammingManager mgr(nullptr, m_doc);
    mgr.loadCanvas(scene->id());
    SceneGroupLooks *canvas = mgr.m_canvas;
    QVERIFY(canvas != nullptr);

    // Select the group target: the fixtures' faders.
    QTreeWidgetItem *groupRow = nullptr;
    for (QTreeWidgetItemIterator it(canvas->m_targetList); *it; ++it)
        if ((*it)->data(0, Qt::UserRole + 1).toInt() == 0 /* TargetGroup */
            && (*it)->data(0, Qt::UserRole).toUInt() == grp->id())
            groupRow = *it;
    QVERIFY(groupRow != nullptr);
    groupRow->setSelected(true);
    QCOMPARE(mgr.m_selectedFixtures, fids);
    QVERIFY(mgr.m_lookEditor->isHidden());

    // Then the look: its editor, the group still selected behind it.
    QTreeWidgetItem *lookRow = nullptr;
    for (int i = 0; i < canvas->m_lookList->topLevelItemCount(); ++i)
        if (canvas->m_lookList->topLevelItem(i)->data(0, Qt::UserRole).toUInt() == col->id())
            lookRow = canvas->m_lookList->topLevelItem(i);
    QVERIFY(lookRow != nullptr);
    canvas->m_lookList->setCurrentItem(lookRow);
    lookRow->setSelected(true);
    QVERIFY(!mgr.m_lookEditor->isHidden());
    QCOMPARE(mgr.m_lookEditor->paletteId(), col->id());

    // Move the blue: every tick reloads the canvas. The picker stays.
    for (int b = 150; b <= 250; b += 50)
    {
        mgr.m_lookEditor->slotColorChanged(QColor(0, 0, b));
        QVERIFY2(!mgr.m_lookEditor->isHidden(), qPrintable(QString("dropped out at blue %1").arg(b)));
        QVERIFY(mgr.m_fixtureScroll->isHidden());
        QCOMPARE(mgr.m_lookEditor->paletteId(), col->id());
    }
    QCOMPARE(col->rgbValue().blue(), 250);

    // A real change of selection still swaps to the faders.
    groupRow = nullptr;
    for (QTreeWidgetItemIterator it(canvas->m_targetList); *it; ++it)
        if ((*it)->data(0, Qt::UserRole + 1).toInt() == 1 /* TargetFixture */
            && (*it)->data(0, Qt::UserRole).toUInt() == solo->id())
            groupRow = *it;
    QVERIFY(groupRow != nullptr);
    canvas->m_targetList->clearSelection();
    groupRow->setSelected(true);
    QCOMPARE(mgr.m_selectedFixtures, QList<quint32>({ solo->id() }));
    QVERIFY(mgr.m_lookEditor->isHidden());
}

/* "push blue up from all the USt bars .. I don't see anything on the rig
   view": a look built on the scene's own faders, no palette, drew nothing
   without live DMX. */
void ProgrammingManager_Test::theRigViewDrawsWhatTheScenesFadersSet()
{
    Fixture *f = new Fixture(m_doc);
    f->setName("USt 1");
    f->setChannels(3);
    QVERIFY(m_doc->addFixture(f));
    Scene *scene = new Scene(m_doc);
    scene->setName("USt test");
    scene->setValue(f->id(), 2, 200);           // blue up, on the fader
    QVERIFY(m_doc->addFunction(scene));

    StructureStudioView view(m_doc, StructureStudioView::StageKind, 0);
    view.setLiveValues(false);
    view.setActiveScene(scene->id());
    const QByteArray v = view.valuesForDrawing(f);
    QVERIFY(v.size() >= 3);
    QCOMPARE(uchar(v.at(2)), uchar(200));

    // A fader moved later shows too.
    scene->setValue(f->id(), 2, 90);
    auto blue = [&view, f]() {
        const QByteArray b = view.valuesForDrawing(f);
        return b.size() > 2 ? uchar(b.at(2)) : uchar(0);
    };
    QTRY_COMPARE(blue(), uchar(90));
}

/* "changed to red.. no red on the person": a look SWAPPED on a scene (Blue
   out, Red in) changed what it paints but told nobody, so the rig view went
   on drawing blue. Adding or removing a look or a target now notifies. */
void ProgrammingManager_Test::theRigViewFollowsALookSwappedOnTheScene()
{
    // An RGB fixture: a colour look paints its red, green and blue.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("RGB Washer");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("RGB");
    const QLCChannel::Preset presets[3] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                            QLCChannel::IntensityBlue };
    for (int c = 0; c < 3; ++c)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("C%1").arg(c));
        ch->setPreset(presets[c]);
        def->addChannel(ch);
        mode->insertChannel(ch, c);
    }
    def->addMode(mode);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));
    Fixture *f = new Fixture(m_doc);
    f->setName("US1t1");
    f->setFixtureDefinition(def, mode);
    QVERIFY(m_doc->addFixture(f));
    QLCPalette *blue = new QLCPalette(QLCPalette::Color, m_doc);
    blue->setName("Blue");
    blue->setValue(QColor(0, 0, 255));
    QVERIFY(m_doc->addPalette(blue));
    QLCPalette *red = new QLCPalette(QLCPalette::Color, m_doc);
    red->setName("Red");
    red->setValue(QColor(230, 0, 2));
    QVERIFY(m_doc->addPalette(red));
    Scene *scene = new Scene(m_doc);
    scene->setName("USt-Blue");
    scene->addFixture(f->id());
    scene->addPalette(blue->id());
    QVERIFY(m_doc->addFunction(scene));

    QSignalSpy spy(m_doc, &Doc::functionChanged);
    StructureStudioView view(m_doc, StructureStudioView::StageKind, 0);
    view.setLiveValues(false);
    view.setActiveScene(scene->id());
    auto rgb = [&view, f]() {
        const QByteArray b = view.valuesForDrawing(f);
        return b.size() > 2 ? QColor(uchar(b.at(0)), uchar(b.at(1)), uchar(b.at(2))) : QColor();
    };
    QTRY_VERIFY(rgb().isValid() && rgb().blue() > 200 && rgb().red() < 20);

    // Swap the look, as dropping Red and removing Blue in the Looks list does.
    spy.clear();
    scene->removePalette(blue->id());
    scene->addPalette(red->id());
    QVERIFY2(spy.count() >= 2, "swapping a look on the scene told nobody");
    QTRY_VERIFY2(rgb().red() > 200 && rgb().blue() < 20,
                 qPrintable("the rig view still draws " + rgb().name()));

    // No change, no notice.
    spy.clear();
    scene->addPalette(red->id());
    scene->removePalette(blue->id());
    QCOMPARE(spy.count(), 0);
}

/* "not seeing output on farts .. we need a warning in programming view for
   unpatched fixtures": a group whose fixtures are in the inventory looked
   like any other target. */
void ProgrammingManager_Test::unpatchedTargetsAreFlaggedInTheProgrammingView()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    Fixture *patched = new Fixture(m_doc);
    patched->setName("FART-SR-1");
    patched->setChannels(3);
    QVERIFY(m_doc->addFixture(patched));
    Fixture *staged = new Fixture(m_doc);
    staged->setName("FART-SR-2");
    staged->setChannels(3);
    staged->setPatched(false);
    QVERIFY(m_doc->addFixture(staged));
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("FARTS");
    grp->setSize(QSize(2, 1));
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->assignFixture(patched->id(), QLCPoint(0, 0));
    grp->assignFixture(staged->id(), QLCPoint(1, 0));
    Scene *scene = new Scene(m_doc);
    scene->setName("Gradient");
    scene->addFixtureGroup(grp->id());
    QVERIFY(m_doc->addFunction(scene));

    ProgrammingManager mgr(nullptr, m_doc);
    mgr.loadCanvas(scene->id());
    SceneGroupLooks *canvas = mgr.m_canvas;
    QVERIFY(canvas != nullptr);
    QVERIFY(!canvas->m_unpatchedNote->isHidden());
    QVERIFY2(canvas->m_unpatchedNote->text().contains("1 of these 2"), qPrintable(canvas->m_unpatchedNote->text()));
    QVERIFY(canvas->m_unpatchedNote->text().contains("send no DMX"));

    // Patched: the warning goes.
    QVERIFY(m_doc->patchFixtures({ { staged->id(), 0, 10 } }));
    canvas->reload();
    QVERIFY(canvas->m_unpatchedNote->isHidden());
}

/* A colour effect is a look for colour, a dimmer effect for the dimmer:
   the Targets list must not say "no look for" what the effect drives. */
void ProgrammingManager_Test::anEffectIsALookForWhatItDrives()
{
    Fixture *fx = new Fixture(m_doc);
    fx->setName("Par");
    fx->setChannels(1);                          // a dimmer
    QVERIFY(m_doc->addFixture(fx));
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Pars");
    grp->setSize(QSize(1, 1));
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->assignFixture(fx->id(), QLCPoint(0, 0));
    Scene *scene = new Scene(m_doc);
    scene->setName("Flicker");
    scene->addFixtureGroup(grp->id());
    QVERIFY(m_doc->addFunction(scene));

    ProgrammingManager mgr(nullptr, m_doc);
    mgr.loadCanvas(scene->id());
    SceneGroupLooks *canvas = mgr.m_canvas;
    QVERIFY(canvas != nullptr);
    auto groupRow = [canvas]() {
        return canvas->m_targetList->topLevelItemCount() > 0
               ? canvas->m_targetList->topLevelItem(0)->text(0) : QString();
    };
    QVERIFY2(groupRow().contains("no look for: dimmer"), qPrintable(groupRow()));

    QLCPalette *effect = new QLCPalette(QLCPalette::Effect, m_doc);
    effect->setName("fire");
    effect->setPath("Palettes/Effect/Dimmer/Fire/");
    QVERIFY(m_doc->addPalette(effect));
    scene->addPalette(effect->id());
    canvas->reload();
    QVERIFY2(!groupRow().contains("no look for"), qPrintable(groupRow()));
}

/* The footer's "Not ready" list carries fixtures that send no DMX, and
   fixture types whose definition declares no light output; each clears when
   it is fixed. */
void ProgrammingManager_Test::unpatchedAndSilentFixturesAreInTheFooter()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    Fixture *patched = new Fixture(m_doc);
    patched->setName("Par 1");
    patched->setChannels(1);
    QVERIFY(m_doc->addFixture(patched));
    Fixture *staged = new Fixture(m_doc);
    staged->setName("FART-SL-9");
    staged->setChannels(1);
    staged->setPatched(false);
    QVERIFY(m_doc->addFixture(staged));

    auto entry = [](const QString &key) {
        foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
            if (e.key == key)
                return e;
        return ShowStatus::Entry();
    };
    AppUtil::updateFixtureReadiness(m_doc);
    const ShowStatus::Entry unpatched = entry("fixtures.unpatched");
    QVERIFY2(unpatched.summary.contains("1 fixture"), qPrintable(unpatched.summary));
    QCOMPARE(unpatched.items, QStringList() << "FART-SL-9");
    const ShowStatus::Entry silent = entry("fixtures.output");
    QVERIFY2(silent.summary.contains("1 fixture type"), qPrintable(silent.summary));
    QCOMPARE(silent.items.size(), 1);
    QVERIFY2(silent.items.first().contains("2 fixture"), qPrintable(silent.items.first()));

    // Patched: that one goes.
    QVERIFY(m_doc->patchFixtures({ { staged->id(), 0, 10 } }));
    AppUtil::updateFixtureReadiness(m_doc);
    QVERIFY(entry("fixtures.unpatched").key.isEmpty());

    // The definition says what it puts out: so does that one.
    for (Fixture *fx : { patched, staged })        // a generic fixture has a definition of its own
    {
        QLCFixtureMode *mode = const_cast<QLCFixtureMode *>(fx->fixtureMode());
        QLCPhysical ph = mode->physical();
        ph.setBulbLumens(300);
        mode->setPhysical(ph);
    }
    AppUtil::updateFixtureReadiness(m_doc);
    QVERIFY(entry("fixtures.output").key.isEmpty());
}

/* The footer's detail dialog fixes problems in place: select rows -- a
   shift-click range, or an entry's own row for all of it -- and its action
   runs on exactly those; a problem nothing can fix is not selectable, a
   selection across two problems offers no fix, and the list follows as
   things are fixed. */
void ProgrammingManager_Test::theReadinessDialogFixesWhatIsSelected()
{
    ShowStatus *status = ShowStatus::instance();
    QVariantList ran;
    status->setAction("test.fixable", "Patch…", [&ran](QWidget *, const QVariantList &items) {
        ran = items;
    });
    status->setStatus("test.fixable", ShowStatus::Warning, "4 things", QString(),
                      { "A", "B", "C", "D" }, { 1, 2, 3, 4 });
    status->setStatus("test.unfixable", ShowStatus::Warning, "1 other thing", QString(),
                      { "E" }, { 5 });

    ShowStatusDialog dlg;
    QTreeWidget *tree = dlg.tree();
    auto row = [tree](const QString &text) -> QTreeWidgetItem * {
        const QList<QTreeWidgetItem *> found = tree->findItems(text, Qt::MatchExactly | Qt::MatchRecursive);
        return found.isEmpty() ? nullptr : found.first();
    };
    QVERIFY(row("B") && row("D") && row("E"));

    // A shift-click range: B..D.
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                      tree->visualItemRect(row("B")).center());
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::ShiftModifier,
                      tree->visualItemRect(row("D")).center());
    QVariantList items;
    QCOMPARE(dlg.selectionKey(&items), QString("test.fixable"));
    QCOMPARE(items, (QVariantList{ 2, 3, 4 }));
    QVERIFY(dlg.actionButton()->isEnabled());
    QCOMPARE(dlg.actionButton()->text(), QString("Patch… (3)"));
    dlg.actionButton()->click();
    QCOMPARE(ran, (QVariantList{ 2, 3, 4 }));

    // The entry's own row: all of it.
    tree->clearSelection();
    row("4 things")->setSelected(true);
    QVERIFY(dlg.selectionKey(&items) == "test.fixable");
    QCOMPARE(items, (QVariantList{ 1, 2, 3, 4 }));

    // Nothing fixes the other one: it cannot be picked, so a selection
    // reaching for it is still only the fixable rows.
    QVERIFY(!(row("E")->flags() & Qt::ItemIsSelectable));

    // Double-click: that one.
    ran.clear();
    emit tree->itemDoubleClicked(row("A"), 0);
    QCOMPARE(ran, (QVariantList{ 1 }));

    // Fixed elsewhere: the list follows.
    status->setStatus("test.fixable", ShowStatus::Warning, "1 thing", QString(), { "D" }, { 4 });
    QVERIFY(row("A") == nullptr);
    QVERIFY(row("D") != nullptr);

    status->clearStatus("test.fixable");
    status->clearStatus("test.unfixable");
    status->setAction("test.fixable", QString(), ShowStatus::Action());
}

/* Address clashes, names used twice, and universes with fixtures but no
   output are in the footer too, each item carrying what its fix needs. */
void ProgrammingManager_Test::otherRigProblemsAreInTheFooter()
{
    auto entry = [](const QString &key) {
        foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
            if (e.key == key)
                return e;
        return ShowStatus::Entry();
    };
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    auto make = [this](const QString &name, quint32 uni, quint32 addr, bool patched = true) {
        Fixture *fx = new Fixture(m_doc);
        fx->setName(name);
        fx->setChannels(1);
        fx->setUniverse(uni);
        fx->setAddress(addr);
        fx->setPatched(patched);
        m_doc->addFixture(fx);
        return fx;
    };
    Fixture *a = make("Dup", 0, 0);
    Fixture *b = make("Dup", 0, 1);
    Fixture *other = make("Twin", 0, 2);
    Fixture *twin = make("Twin", 0, 3);
    // Patched on universe 2, which has no output.
    make("Lonely", 1, 0);
    // Loaded unpatched because its address was taken.
    // (as a load leaves it: unpatched, flagged)
    Fixture *clash = make("Clash", 0, 0, false);
    clash->setAddressClash(true);
    QCOMPARE(a->name(), QString("Dup"));
    // addFixture() may number a taken name: put the duplicates back.
    b->setName("Dup");
    twin->setName("Twin");
    other->setName("Twin");

    AppUtil::updateFixtureReadiness(m_doc);

    const ShowStatus::Entry dups = entry("fixtures.duplicates");
    QCOMPARE(dups.itemData, (QVariantList{ "Dup", "Twin" }));
    const ShowStatus::Entry clashes = entry("fixtures.clash");
    QCOMPARE(clashes.itemData, (QVariantList{ clash->id() }));
    // ...and only there, not also as merely unpatched.
    QVERIFY(!entry("fixtures.unpatched").itemData.contains(clash->id()));
    const ShowStatus::Entry silent = entry("universes.nooutput");
    QVERIFY2(silent.itemData.contains(1), qPrintable(silent.items.join("; ")));

    // Numbering only the names picked.
    QCOMPARE(m_doc->makeFixtureNamesUnique(QStringList() << "Dup"), 1);
    AppUtil::updateFixtureReadiness(m_doc);
    QCOMPARE(entry("fixtures.duplicates").itemData, (QVariantList{ "Twin" }));
    QCOMPARE(m_doc->makeFixtureNamesUnique(), 1);
    AppUtil::updateFixtureReadiness(m_doc);
    QVERIFY(entry("fixtures.duplicates").key.isEmpty());

    ShowStatus::instance()->clearStatus("universes.nooutput");
    ShowStatus::instance()->clearStatus("fixtures.clash");
}

/* Broken definitions under "Local" and "Included" headings; a heading
   picks everything under it; and an entry can offer more than one fix
   (repair it, or move it to the Trash) -- one button each. */
void ProgrammingManager_Test::readinessItemsSitUnderHeadingsWithMoreThanOneFix()
{
    ShowStatus *status = ShowStatus::instance();
    QString ran;
    QVariantList on;
    status->setAction("test.files", "Repair…", [&](QWidget *, const QVariantList &items) { ran = "repair"; on = items; });
    status->addAction("test.files", "Move to Trash…", [&](QWidget *, const QVariantList &items) { ran = "trash"; on = items; });
    status->setStatus("test.files", ShowStatus::Warning, "3 files", QString(),
                      { "a.qxf — read error", "b.qxf — read error", "c.qxf — read error" },
                      { "/a", "/b", "/c" }, { "Local", "Local", "Included" });

    ShowStatusDialog dlg;
    QTreeWidget *tree = dlg.tree();
    const QList<QTreeWidgetItem *> local = tree->findItems("Local", Qt::MatchExactly | Qt::MatchRecursive);
    QCOMPARE(local.size(), 1);
    QCOMPARE(local.first()->childCount(), 2);
    QCOMPARE(local.first()->child(0)->text(0), QString("a.qxf — read error"));

    local.first()->setSelected(true);
    QVariantList items;
    QCOMPARE(dlg.selectionKey(&items), QString("test.files"));
    QCOMPARE(items, (QVariantList{ "/a", "/b" }));
    QCOMPARE(dlg.actionButtons().size(), 2);
    QCOMPARE(dlg.actionButtons().at(0)->text(), QString("Repair… (2)"));
    QCOMPARE(dlg.actionButtons().at(1)->text(), QString("Move to Trash… (2)"));
    dlg.actionButtons().at(1)->click();
    QCOMPARE(ran, QString("trash"));
    QCOMPARE(on, (QVariantList{ "/a", "/b" }));

    status->clearStatus("test.files");
    status->setAction("test.files", QString(), ShowStatus::Action());
}

/* Two local files for one fixture: the one not used is listed under the
   fixture's name ("older than ..., not used"), to be trashed. A broken file
   for a fixture another file does provide says which is used instead. */
void ProgrammingManager_Test::duplicateDefinitionsAreListedWithWhatIsUsed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QByteArray xml(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE FixtureDefinition>\n"
        "<FixtureDefinition xmlns=\"http://www.qlcplus.org/FixtureDefinition\">\n"
        " <Manufacturer>Zz Dup</Manufacturer>\n"
        " <Model>Par</Model>\n"
        " <Type>Color Changer</Type>\n"
        " <Channel Name=\"Dimmer\" Preset=\"IntensityDimmer\"/>\n"
        " <Mode Name=\"1ch\">\n"
        "  <Channel Number=\"0\">Dimmer</Channel>\n"
        " </Mode>\n"
        "</FixtureDefinition>\n");
    auto write = [&](const QString &name, QByteArray data, int age) {
        QFile f(dir.filePath(name));
        f.open(QIODevice::WriteOnly);
        f.write(data);
        f.close();
        f.open(QIODevice::ReadWrite);
        f.setFileTime(QDateTime::currentDateTime().addSecs(-age), QFileDevice::FileModificationTime);
    };
    QByteArray newer = xml;
    newer.replace("<Type>", "<!-- edited -->\n <Type>");
    write("Zz-Dup-Par.qxf", xml, 3600);
    write("Zz-Dup-Par 2.qxf", newer, 10);
    QByteArray broken = xml;
    broken.replace("IntensityDimmer\"/>", "IntensityDimmer\"\\>");
    write("Zz-Dup-Par 3.qxf", broken, 7200);
    QVERIFY(m_doc->fixtureDefCache()->load(QDir(dir.path(), "*.qxf")));

    AppUtil::updateFixtureReadiness(m_doc);
    ShowStatus::Entry dups, failed;
    foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
    {
        if (e.key == "definitions.duplicates") dups = e;
        if (e.key == "definitions.failed") failed = e;
    }
    const int i = dups.items.indexOf(QRegularExpression("^Zz-Dup-Par\\.qxf.*"));
    QVERIFY2(i >= 0, qPrintable(dups.items.join("; ")));
    QVERIFY(dups.items.at(i).contains("older than Zz-Dup-Par 2.qxf, not used"));
    QCOMPARE(dups.itemGroups.at(i), QString("Zz Dup Par"));
    QCOMPARE(dups.itemData.at(i).toString(), dir.filePath("Zz-Dup-Par.qxf"));

    const int j = failed.items.indexOf(QRegularExpression("^Zz-Dup-Par 3\\.qxf.*"));
    QVERIFY2(j >= 0, qPrintable(failed.items.join("; ")));
    QVERIFY2(failed.items.at(j).contains("Zz-Dup-Par 2.qxf is used"), qPrintable(failed.items.at(j)));

    m_doc->fixtureDefCache()->forgetFailedDefinition(dir.filePath("Zz-Dup-Par 3.qxf"));
    ShowStatus::instance()->clearStatus("definitions.duplicates");
    ShowStatus::instance()->clearStatus("definitions.failed");
}

QTEST_MAIN(ProgrammingManager_Test)

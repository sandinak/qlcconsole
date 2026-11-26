/*
  Q Light Controller Plus
  curtaindraw_test.cpp

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
#include <QElapsedTimer>
#include <QVector3D>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QDir>
#include <QFile>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "sceneryitem.h"
#include "structurestudioview.h"
#include "fixturemanager.h"
#include "fixtureremap.h"
#undef protected
#undef private
#include <QComboBox>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QTreeWidget>
#include "../common/fixtureeditor.h"
#include "monitorfixtureitem.h"
#include "fixturetreewidget.h"
#include "fixturefields.h"
#include "contextmenus.h"
#include "cuerunner.h"
#include "qlcpalette.h"
#include "fixturegroup.h"
#include "fixturerest.h"
#include "programmercontroller.h"
#include "doc.h"
#include "chaser.h"
#include "chaseraction.h"
#include "scene.h"
#include "scenery.h"
#include "fixture.h"
#include "universe.h"
#include "mastertimer.h"
#include "inputoutputmap.h"
#include "monitorproperties.h"
#include "functionparent.h"
#include "qlcfixturedefcache.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "showcontrol.h"
#include "outputrulekeeper.h"
#include <QLabel>
#include "curtaindraw_test.h"

using namespace ShowTree;

namespace
{

void settle(int ms)
{
    QElapsedTimer t;
    t.start();
    do
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    while (t.elapsed() < ms);
}

}

void CurtainDraw_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    m_app->showNormal();
    m_app->resize(1280, 800);
    QVERIFY(m_app->doc()->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures")));
    QVERIFY(m_app->doc()->fixtureDefCache()->fixtureDef("Generic", "Stage Curtain") != nullptr);
    connect(m_app->doc()->inputOutputMap(), &InputOutputMap::universeWritten, this,
            [this](quint32 universe, const QByteArray &frame) { m_frames[universe] = frame; });
}

void CurtainDraw_Test::cleanupTestCase()
{
    stopAll();
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

double CurtainDraw_Test::drawnOpen(quint32 sceneryId, quint32 fixtureId, const char *what)
{
    Monitor *mon = Monitor::instance();
    MonitorGraphicsView *gv = mon != nullptr ? mon->m_graphicsView : nullptr;
    SceneryItem *item = gv != nullptr ? gv->m_sceneryItems.value(sceneryId, nullptr) : nullptr;
    Doc *doc = m_app->doc();
    Fixture *fx = doc->fixture(fixtureId);
    const int out = onWire(fixtureId, 0);
    const double drawn = (item != nullptr && item->hasPose()) ? item->pose().open : -1.0;
    qInfo().noquote() << what << "on the wire" << out
                      << "fixture" << (fx != nullptr ? fx->channelValues().toHex() : QByteArray())
                      << "plot draws open" << drawn;
    return drawn;
}

void CurtainDraw_Test::loadAndGo(Chaser *c, const QString &number)
{
    ShowControl::Request load;
    load.command = ShowControl::Load;
    load.chaser = c->id();
    load.cue = number;
    m_app->doc()->showControl()->run(load, ShowControl::RouteRunTab);
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = c->id();
    m_app->doc()->showControl()->run(go, ShowControl::RouteRunTab);
}

void CurtainDraw_Test::jump(Chaser *c, int step)
{
    ChaserAction action;
    action.m_action = ChaserSetStepIndex;
    action.m_stepIndex = step;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    c->setAction(action);
    if (!c->isRunning())
        c->start(m_app->doc()->masterTimer(), FunctionParent::master());
}

void CurtainDraw_Test::stopAll()
{
    if (m_app == nullptr)
        return;
    m_app->doc()->showTree()->stopPreview();
    m_app->doc()->masterTimer()->stopAllFunctions();
    settle(200);
}

/* What the wire has for fixture @p fx's channel @p ch now (-1: none). */
int CurtainDraw_Test::onWire(quint32 fixtureId, quint32 ch)
{
    /* Whole frames, as each universe hands them on (universeWritten: what
       goes out, and what the rig views get) -- never a read of the live
       array mid-frame, which can catch a lower fader's value before the
       layer over it has written. */
    Fixture *fx = m_app->doc()->fixture(fixtureId);
    if (fx == nullptr)
        return -1;
    const QByteArray frame = m_frames.value(fx->universe());
    const int at = int(fx->address() + ch);
    return at < frame.size() ? int(uchar(frame.at(at))) : 0;
}

bool CurtainDraw_Test::markShown(quint32 sceneryId)
{
    Monitor *mon = Monitor::instance();
    SceneryItem *item = mon != nullptr && mon->m_graphicsView != nullptr
                      ? mon->m_graphicsView->m_sceneryItems.value(sceneryId, nullptr) : nullptr;
    return item != nullptr && item->heldMark();
}

/* Board Q350, his report ("clicking between cues in the Show Tree, in
   Design: the curtain stays closed on screen; clicked again while it opens,
   it stops"): in Design a cue picked while editing holds the curtain where
   it physically is -- nothing on the wire moves it, the preview's own
   scene neither -- while the plot (and the Rig Overview) draw it where the
   picked cue's tracked state puts it, marked "held while editing". A GO
   moves it for real; Operate is the run's. His structure: the curtain
   fixture on a universe with no output, driving the Main drape, its scene
   in every cue but the first. */
void CurtainDraw_Test::aPickedCueDrawsTheCurtainAndHoldsTheStage()
{
    Doc *doc = m_app->doc();
    m_app->clearDocument();
    doc->setMode(Doc::Design);
    while (doc->inputOutputMap()->universesCount() < 4)
        QVERIFY(doc->inputOutputMap()->addUniverse());
    QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef("Generic", "Stage Curtain");
    QVERIFY(def != nullptr && def->mode("Open + Fly") != nullptr);
    Fixture *curtain = new Fixture(doc);
    curtain->setName("Main drape curtain");
    curtain->setFixtureDefinition(def, def->mode("Open + Fly"));
    curtain->setUniverse(3);                  // "U04-Control": no output patched
    curtain->setAddress(0);
    QVERIFY(doc->addFixture(curtain));
    Fixture *dimmer = new Fixture(doc);
    dimmer->setName("Dimmer");
    dimmer->setChannels(1);
    dimmer->setUniverse(0);
    dimmer->setAddress(0);
    QVERIFY(doc->addFixture(dimmer));

    // A 12 x 10 m stage, the drape across it (in the shots).
    doc->monitorProperties()->setGridUnits(MonitorProperties::Meters);
    doc->monitorProperties()->setGridSize(QVector3D(12.0f, 6.0f, 10.0f));
    Scenery *drape = doc->monitorProperties()->addScenery();
    drape->setKind(Scenery::MainDrape);
    drape->setOriginX(6.0f); drape->setOriginY(6.0f);
    drape->setWidth(10.0f);
    drape->setState(Scenery::Open);
    drape->setDriverFixture(curtain->id());

    Scene *open = new Scene(doc);
    open->setName("Curtain Open");
    open->setValue(curtain->id(), 0, 255);
    open->setValue(curtain->id(), 1, 0);
    open->setFadeInSpeed(2000);
    QVERIFY(doc->addFunction(open));
    Scene *half = new Scene(doc);
    half->setName("Half");
    half->setValue(dimmer->id(), 0, 128);
    QVERIFY(doc->addFunction(half));
    Scene *full = new Scene(doc);
    full->setName("Full");
    full->setValue(dimmer->id(), 0, 255);
    QVERIFY(doc->addFunction(full));

    ShowTreeController *st = doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("Show"), "Show list");
    auto layer = [&](quint32 cue, quint32 function) {
        Target t;
        t.function = function;
        return st->addLayer(cue, t) != ShowTree::invalidId();
    };
    const quint32 preset = st->addCue(listId, -1, "Preset");
    QVERIFY(layer(preset, half->id()));
    const quint32 c1 = st->addCue(listId, 0, "Curtain");
    QVERIFY(layer(c1, open->id()) && layer(c1, half->id()));
    const quint32 c2 = st->addCue(listId, 1, "Song");
    QVERIFY(layer(c2, open->id()) && layer(c2, full->id()));
    {
        // His curtain cue: a Mix, fading in over 2 s.
        Cue timed = *st->cue(c1);
        timed.fadeIn = 2000;
        QVERIFY(st->setCue(timed));
    }
    const CueList *list = st->cueList(listId);
    QVERIFY(list != nullptr);
    Chaser *c = qobject_cast<Chaser *>(doc->function(list->chaser));
    QVERIFY(c != nullptr);
    doc->showControl()->setMainCueList(c->id());
    const QString n1 = st->cueNumber(*list, 1);

    // The Lighting Studio's plot, as he has it up.
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    m_app->m_tab->setCurrentIndex(m_app->m_tab->indexOf(mon));
    mon->updateView();
    settle(300);
    QVERIFY(mon->m_graphicsView != nullptr);
    const quint32 sid = drape->id(), fid = curtain->id();
    QVERIFY(mon->m_graphicsView->m_sceneryItems.contains(sid));
    OutputRuleKeeper *keeper = doc->outputRuleKeeper();
    QVERIFY(keeper != nullptr);
    QCOMPARE(drawnOpen(sid, fid, "loaded:"), 0.0);    // nothing plays on it: closed
    QVERIFY(!markShown(sid));

    // The cue that opens it picked: drawn open, marked; the stage stays shut.
    st->pickCue(c1);
    QTRY_COMPARE_WITH_TIMEOUT(drawnOpen(sid, fid, "1 picked:"), 1.0, 2000);
    QVERIFY(markShown(sid));
    settle(2500);                              // longer than its fade
    QCOMPARE(onWire(fid, 0), 0);
    QCOMPARE(drawnOpen(sid, fid, "1 picked, its fade over:"), 1.0);
    // The next one (the curtain tracks on): the same.
    st->pickCue(c2);
    settle(400);
    QCOMPARE(drawnOpen(sid, fid, "2 picked:"), 1.0);
    QVERIFY(markShown(sid));
    QCOMPARE(onWire(fid, 0), 0);
    // The preset (nothing sends it): drawn where it is, still held.
    st->pickCue(preset);
    settle(300);
    QCOMPARE(drawnOpen(sid, fid, "preset picked:"), 0.0);
    QVERIFY(markShown(sid));

    // A GO from the Run moves it for real -- the cue's 2 s fade from where
    // it was held, though its scene is at full under the preview: a motor
    // is never told to jump.
    st->pickCue(c1);
    settle(300);
    QCOMPARE(onWire(fid, 0), 0);
    loadAndGo(c, n1);
    {
        const QString why = travels(fid, 0, 255, 1200, 4000);
        QVERIFY2(why.isEmpty(), qPrintable("GO after a pick: " + why));
    }
    QTRY_VERIFY_WITH_TIMEOUT(!markShown(sid), 1000);
    QCOMPARE(drawnOpen(sid, fid, "GO, opened:"), 1.0);

    /* The same from a start that is not the Run's (a VC cue list, OSC): the
       list is already playing the curtain's scene, at full under the hold,
       when the preview lets go -- still the cue's fade from where it was. */
    auto closeIt = [&]() {
        stopAll();
        jump(c, 0);
        settle(300);
        stopAll();
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        ua.at(3)->reset(0, 2);
        doc->inputOutputMap()->releaseUniverses(true);
    };
    closeIt();
    QTRY_COMPARE_WITH_TIMEOUT(onWire(fid, 0), 0, 1000);
    st->pickCue(c1);
    settle(2500);                              // the scene at full, held
    QCOMPARE(onWire(fid, 0), 0);
    jump(c, 1);
    {
        const QString why = travels(fid, 0, 255, 1200, 4000);
        QVERIFY2(why.isEmpty(), qPrintable("a list started after a pick: " + why));
    }

    // Picked while it opens: held where it is, drawn where the cue puts it.
    closeIt();
    QTRY_COMPARE_WITH_TIMEOUT(onWire(fid, 0), 0, 1000);
    loadAndGo(c, n1);
    QTRY_VERIFY_WITH_TIMEOUT(onWire(fid, 0) > 20, 1500);
    QVERIFY2(onWire(fid, 0) < 255, "still opening");
    st->pickCue(c2);
    settle(200);
    const int stopped = onWire(fid, 0);
    QVERIFY2(stopped > 0 && stopped < 255, qPrintable(QString::number(stopped)));
    settle(800);
    QCOMPARE(onWire(fid, 0), stopped);
    QCOMPARE(drawnOpen(sid, fid, "2 picked mid-move:"), 1.0);
    QVERIFY(markShown(sid));
    // For the UX review: the plot with the drape marked.
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shots.isEmpty())
    {
        QDir().mkpath(shots);
        for (const QSize &size : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
        {
            m_app->resize(size);
            settle(300);
            mon->m_graphicsView->centerOn(mon->m_graphicsView->m_sceneryItems.value(sid));
            settle(100);
            mon->m_graphicsView->grab().save(QDir(shots).filePath(
                QStringLiteral("curtain-held-while-editing-%1x%2.png").arg(size.width()).arg(size.height())));
        }
        m_app->resize(1280, 800);
    }

    // The Rig Overview draws it the same.
    {
        StructureStudioView view(doc, StructureStudioView::StageKind, 0);
        view.setLiveValues(true);
        QCOMPARE(view.computeDrapePose(drape).open, 1.0);
    }

    // Into Operate: let go to the run, which plays the cue that opens it --
    // over that cue's fade from where it was held, never a jump.
    doc->setMode(Doc::Operate);
    {
        const QString why = travels(fid, 0, 255, 1200, 4000);
        QVERIFY2(why.isEmpty(), qPrintable("into Operate: " + why));
    }
    QTRY_VERIFY_WITH_TIMEOUT(!markShown(sid), 1000);
    QVERIFY(!keeper->machineryHeldForEdit());

    // Operate: the run's -- a jump moves it, nothing holds it.
    stopAll();
    jump(c, 2);
    QTRY_COMPARE_WITH_TIMEOUT(onWire(fid, 0), 255, 4000);
    QTRY_COMPARE_WITH_TIMEOUT(drawnOpen(sid, fid, "Operate, jumped:"), 1.0, 1000);
    QVERIFY(!markShown(sid));
    stopAll();
    doc->setMode(Doc::Design);
}

void CurtainDraw_Test::behavesAsAndParkOnScreen()
{
    Doc *doc = m_app->doc();
    stopAll();
    m_app->clearDocument();
    doc->setMode(Doc::Design);
    auto dimmer = [&](const QString &name, quint32 address, float x) {
        Fixture *f = new Fixture(doc);
        f->setName(name);
        f->setChannels(1);
        f->setUniverse(0);
        f->setAddress(address);
        doc->addFixture(f);
        doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(x, 3000.0f, 0));
        FixtureGroup *g = new FixtureGroup(doc);
        g->setName(name);
        doc->addFixtureGroup(g);
        g->assignFixture(f->id());
        return f;
    };
    Fixture *wash = dimmer("Stage wash", 0, 2000.0f);
    Fixture *relay = dimmer("House relay", 1, 4000.0f);
    relay->setOutputRules(Fixture::Switch);
    Fixture *drop = dimmer("Kabuki drop", 2, 6000.0f);
    Fixture *jet = dimmer("CO2 jet", 3, 8000.0f);
    Fixture *laser = dimmer("Sky laser", 4, 10000.0f);
    {
        // Its label says Class 3B (Fixture Properties' Laser class).
        FixtureRigProps rp = doc->monitorProperties()->fixtureRigProps(laser->id());
        rp.laserClass = 4;
        doc->monitorProperties()->setFixtureRigProps(laser->id(), rp);
    }
    auto group = [&](const Fixture *f) {
        for (FixtureGroup *g : doc->fixtureGroups())
            if (g->fixtureList() == QList<quint32>() << f->id())
                return g->id();
        return FixtureGroup::invalidId();
    };
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, doc);
    full->setName("Full");
    full->setValue(255);
    doc->addPalette(full);
    ShowTreeController *st = doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("Show"), "Show list");
    const quint32 c1 = st->addCue(listId, -1, "Preset");
    const quint32 c2 = st->addCue(listId, 0, "The drop");
    for (const Fixture *f : { wash, relay, drop, jet })
    {
        Target t = Target();
        t.groups << group(f);
        QVERIFY(st->setTargetLook(c2, t, st->lookForPalette(full->id())));
    }
    Q_UNUSED(c1);
    doc->outputRuleKeeper()->rebuild();
    QCOMPARE(FixtureRest::behaviour(relay), Fixture::SwitchBehaviour);
    QCOMPARE(FixtureRest::behaviour(drop), Fixture::OneShotBehaviour);
    QCOMPARE(FixtureRest::behaviour(jet), Fixture::HazardBurstBehaviour);
    QCOMPARE(FixtureRest::behaviour(laser), Fixture::HazardRunningBehaviour);

    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    m_app->m_tab->setCurrentIndex(m_app->m_tab->indexOf(mon));
    mon->updateView();
    settle(300);
    MonitorGraphicsView *gv = mon->m_graphicsView;
    QVERIFY(gv != nullptr);
    MonitorFixtureItem *relayItem = gv->fixtureItemForId(relay->id());
    MonitorFixtureItem *dropItem = gv->fixtureItemForId(drop->id());
    MonitorFixtureItem *washItem = gv->fixtureItemForId(wash->id());
    QVERIFY(relayItem != nullptr && dropItem != nullptr && washItem != nullptr);

    // A cue picked while editing: the relay and the drop held, marked held,
    // drawn where the cue puts them; the wash previews; nothing fires the jet.
    st->pickCue(c2);
    QTRY_VERIFY_WITH_TIMEOUT(relayItem->isHeldForEdit(), 2000);
    QVERIFY(dropItem->isHeldForEdit());
    QVERIFY(!washItem->isHeldForEdit());
    settle(400);
    QCOMPARE(onWire(relay->id(), 0), 0);
    QCOMPARE(onWire(drop->id(), 0), 0);
    QCOMPARE(onWire(jet->id(), 0), 0);
    QCOMPARE(onWire(wash->id(), 0), 255);
    {
        QByteArray drawn;
        QVERIFY(doc->outputRuleKeeper()->heldForEdit(relay->id(), &drawn));
        QCOMPARE(drawn, QByteArray(1, char(255)));
    }
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
    auto shoot = [&](QWidget *w, const QString &name, bool sized) {
        if (shots.isEmpty())
            return;
        QDir().mkpath(shots);
        if (!sized)
        {
            w->grab().save(QDir(shots).filePath(name + QStringLiteral(".png")));
            return;
        }
        for (const QSize &size : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
        {
            m_app->resize(size);
            settle(300);
            if (w == gv)
                gv->centerOn(relayItem);
            settle(100);
            m_app->grab().save(QDir(shots).filePath(
                QStringLiteral("%1-%2x%3.png").arg(name).arg(size.width()).arg(size.height())));
        }
        m_app->resize(1280, 800);
    };

    // Park Here, from the fixture's own menu (the plot's, the Fixture
    // Manager's, Build's: one menu): a P on the plot.
    {
        QMenu menu;
        ContextMenu cm(&menu);
        ContextMenus::RigPlace place;
        ContextMenus::fixture(cm, doc, { relay->id() }, place);
        QAction *park = menu.findChild<QAction *>(QStringLiteral("ctxParkHere"));
        QVERIFY(park != nullptr);
        QVERIFY(menu.findChild<QAction *>(QStringLiteral("ctxUnpark")) == nullptr);
        park->trigger();
    }
    QVERIFY(doc->programmer()->isFixtureParked(relay->id()));
    QTRY_VERIFY_WITH_TIMEOUT(relayItem->isParked(), 1000);
    QVERIFY(!dropItem->isParked());
    // Park wins: no longer held for the edit, it stays where it is.
    QVERIFY(!doc->outputRuleKeeper()->heldForEdit(relay->id()));
    shoot(gv, QStringLiteral("behaves-plot-held-and-parked"), true);

    // The Fixture Manager: Behaves as, guessed in italics, Parked said.
    FixtureManager *fm = FixtureManager::instance();
    QVERIFY(fm != nullptr);
    m_app->m_tab->setCurrentIndex(m_app->m_tab->indexOf(fm));
    fm->updateView();
    settle(200);
    int col = -1;
    QTreeWidgetItem *head = fm->m_fixtures_tree->headerItem();
    for (int i = 0; i < head->columnCount(); ++i)
        if (head->text(i) == QStringLiteral("Behaves as"))
            col = i;
    QVERIFY2(col > 0, "a Behaves as column");
    QHash<quint32, QString> says;
    QHash<quint32, bool> italic;
    for (QTreeWidgetItemIterator it(fm->m_fixtures_tree); *it; ++it)
    {
        const QVariant v = (*it)->data(0, PROP_ID);
        if (v.isValid() && !(*it)->data(0, PROP_HEAD).isValid())
        {
            says.insert(v.toUInt(), (*it)->text(col));
            italic.insert(v.toUInt(), (*it)->font(col).italic());
        }
    }
    QCOMPARE(says.value(wash->id()), QString());          // a light says nothing (UX-23)
    QVERIFY(!fm->m_fixtures_tree->isColumnHidden(col));
    QCOMPARE(says.value(relay->id()), QStringLiteral("Switch · Parked"));
    QCOMPARE(says.value(drop->id()), QStringLiteral("One-shot"));
    QCOMPARE(says.value(jet->id()), QStringLiteral("Hazard — Burst"));
    QVERIFY(italic.value(drop->id()));
    shoot(fm, QStringLiteral("behaves-fixture-manager"), true);

    // The Run tab: a Parked list, no count; Unpark lets it go.
    CueRunner *run = m_app->findChild<CueRunner *>(QStringLiteral("runTab"));
    QVERIFY(run != nullptr);
    m_app->m_tab->setCurrentIndex(m_app->m_tab->indexOf(run));
    settle(200);
    QWidget *box = run->findChild<QWidget *>(QStringLiteral("crParkedBox"));
    QListWidget *parked = run->findChild<QListWidget *>(QStringLiteral("crParked"));
    QPushButton *unpark = run->findChild<QPushButton *>(QStringLiteral("crUnpark"));
    QVERIFY(box != nullptr && parked != nullptr && unpark != nullptr);
    QVERIFY(box->isVisible());
    QCOMPARE(parked->count(), 1);
    QCOMPARE(parked->item(0)->text(), relay->name());
    shoot(run, QStringLiteral("behaves-run-parked"), true);

    /* Hazard arming (Q352/Q353): the Run tab lists each hazard, its kind
       and state; the footer's chip says DISARMED / ARMED / HELD and what a
       cue skipped. */
    {
        QWidget *hbox = run->findChild<QWidget *>(QStringLiteral("crHazardBox"));
        QLabel *hlist = run->findChild<QLabel *>(QStringLiteral("crHazards"));
        QVERIFY(hbox != nullptr && hlist != nullptr);
        QVERIFY(hbox->isVisible());
        QVERIFY2(hlist->text().contains(QStringLiteral("CO2 jet · Burst · DISARMED")), qPrintable(hlist->text()));
        QVERIFY2(hlist->text().contains(QStringLiteral("Sky laser · Running · DISARMED")), qPrintable(hlist->text()));
        QLabel *chip = m_app->findChild<QLabel *>(QStringLiteral("footerHazard"));
        QVERIFY(chip != nullptr);
        QVERIFY(chip->isVisibleTo(m_app));
        QCOMPARE(chip->text(), QStringLiteral("Hazards: DISARMED"));
        OutputRuleKeeper *k = doc->outputRuleKeeper();
        k->armHazards();
        settle(50);
        QCOMPARE(chip->text(), QStringLiteral("Hazards: ARMED"));
        QVERIFY(hlist->text().contains(QStringLiteral("Sky laser · Running · ARMED")));
        k->setHazardHold(true);
        settle(50);
        QCOMPARE(chip->text(), QStringLiteral("Hazards: ARMED · HELD"));
        QVERIFY(hlist->text().contains(QStringLiteral("CO2 jet · Burst · HELD")));
        shoot(run, QStringLiteral("hazards-run-armed-held"), true);
        k->disarmHazards(QStringLiteral("the test"));
        k->setHazardHold(false);
        settle(50);
        QCOMPARE(chip->text(), QStringLiteral("Hazards: DISARMED"));
        QVERIFY(chip->toolTip().contains(QStringLiteral("Disarmed: the test")));
    }
    unpark->click();
    settle(100);
    QVERIFY(!doc->programmer()->isFixtureParked(relay->id()));
    QVERIFY(!box->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(!relayItem->isParked(), 1000);
    st->stopPreview();
    settle(200);

    // Fixture Properties: Behaves as; a 3B laser as a Light needs "3B" typed.
    m_app->m_tab->setCurrentIndex(m_app->m_tab->indexOf(mon));
    settle(200);
    QStringList asked;
    QString answer;
    FixtureFields::setLaserConfirm([&](const QString &cls) { asked << cls; return answer; });
    QString autoText;
    bool shotDialog = false;
    int step = 0;
    QTimer watch;
    watch.setInterval(20);
    QObject::connect(&watch, &QTimer::timeout, [&]() {
        QDialog *d = qobject_cast<QDialog *>(modalOrFixtureEditor());
        if (d == nullptr)
            return;
        QComboBox *cb = d->findChild<QComboBox *>(QStringLiteral("fixtureBehavesAs"));
        if (cb == nullptr)
        {
            d->reject();
            return;
        }
        if (step == 0)
        {
            autoText = cb->itemText(0);
            if (!shots.isEmpty() && !shotDialog)
            {
                QDir().mkpath(shots);
                if (!fixtureRowShown(d, cb))
                    qWarning() << "Behaves as not on the tab shown";
                d->grab().save(QDir(shots).filePath(QStringLiteral("behaves-fixture-properties.png")));
                shotDialog = true;
            }
            answer = QStringLiteral("3");               // the wrong class: refused
            cb->setCurrentIndex(cb->findData(int(Fixture::LightBehaviour)));
            step = 1;
            return;
        }
        if (step == 1)
        {
            answer = QStringLiteral("3b");              // its class: taken
            cb->setCurrentIndex(cb->findData(int(Fixture::LightBehaviour)));
            step = 2;
            return;
        }
        d->accept();
        watch.stop();
    });
    watch.start();
    mon->editFixtureProperties(laser->id());
    QTRY_VERIFY_WITH_TIMEOUT(!watch.isActive(), 10000);
    settle(200);
    FixtureFields::setLaserConfirm(nullptr);
    QCOMPARE(autoText, QStringLiteral("Auto (Hazard — Running)"));
    QCOMPARE(asked, QStringList({ QStringLiteral("3B"), QStringLiteral("3B") }));
    QCOMPARE(laser->behaviourSetting(), int(Fixture::LightBehaviour));
    QVERIFY(FixtureRest::isHazardousLaserAsLight(laser));
    laser->setBehaviourSetting(-1);
    doc->resetModified();
}

void CurtainDraw_Test::remapKeepsBehavesAs()
{
    Doc *doc = m_app->doc();
    Fixture *f = new Fixture(doc);
    f->setName(QStringLiteral("Glitter cannon prop"));
    f->setChannels(1);
    f->setUniverse(3);
    f->setAddress(300);
    QVERIFY(doc->addFixture(f));
    f->setBehaviourSetting(Fixture::HazardBurstBehaviour);
    {
        FixtureRemap remap(doc);
        QTreeWidgetItem *item = nullptr;
        for (QTreeWidgetItemIterator it(remap.m_sourceTree); *it && item == nullptr; ++it)
            if ((*it)->text(0) == f->name())
                item = *it;
        QVERIFY(item != nullptr);
        remap.m_sourceTree->clearSelection();
        item->setSelected(true);
        remap.slotCloneSourceFixture();
        Fixture *clone = nullptr;
        for (Fixture *t : remap.m_targetDoc->fixtures())
            if (t->name() == f->name())
                clone = t;
        QVERIFY(clone != nullptr);
        QCOMPARE(clone->behaviourSetting(), int(Fixture::HazardBurstBehaviour));
    }
    doc->deleteFixture(f->id());
    doc->resetModified();
}

QString CurtainDraw_Test::travels(quint32 fixtureId, quint32 ch, int to, int notBefore, int within)
{
    /* Watched every 10 ms: no step bigger than a 2 s fade's 40 ms worth
       (the wire is sampled, a frame can be missed), at @p to no sooner than
       @p notBefore ms, and there within @p within. Empty: it did. */
    QElapsedTimer t;
    t.start();
    int last = onWire(fixtureId, ch);
    int biggest = 0;
    qint64 reached = -1;
    QStringList trace;
    trace << QStringLiteral("0:%1").arg(last);
    while (t.elapsed() < within)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QTest::qSleep(10);
        const int now = onWire(fixtureId, ch);
        if (now != last)
            trace << QStringLiteral("%1:%2").arg(t.elapsed()).arg(now);
        biggest = qMax(biggest, qAbs(now - last));
        last = now;
        if (now == to && reached < 0)
            reached = t.elapsed();
        if (reached >= 0 && t.elapsed() > reached + 300)
            break;
    }
    if (biggest <= 40 && reached >= notBefore && last == to)
        return QString();
    return QStringLiteral("to %1: biggest step %2, reached at %3 ms, ends %4; %5")
        .arg(to).arg(biggest).arg(reached).arg(last).arg(trace.mid(0, 60).join(QLatin1Char(' ')));
}

/* His show (a copy, never in the gate): QLC_CURTAIN_SHOW=<the .qlcc>, or
   QLC_CURTAIN_SHOW_QZ=<base64 of its qCompress()> to send it to a pool
   host. Scenery 1 "Main drape", driven by fixture 182 on U04-Control: in
   Design, 1.0.1 then 1.1.1 clicked in the Show Tree. */
void CurtainDraw_Test::hisCurtain()
{
    QTemporaryDir dir;
    QString path = qEnvironmentVariable("QLC_CURTAIN_SHOW");
    const QByteArray qz = qgetenv("QLC_CURTAIN_SHOW_QZ");
    if (path.isEmpty() && !qz.isEmpty())
    {
        path = dir.filePath("show.qlcc");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(qUncompress(QByteArray::fromBase64(qz)));
    }
    if (path.isEmpty())
        QSKIP("set QLC_CURTAIN_SHOW");
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Design);
    m_app->clearDocument();
    QVERIFY(m_app->loadXML(path) == QFile::NoError);
    doc->resetModified();
    Chaser *c = nullptr;
    for (Function *f : doc->functions())
        if (f->name() == "Production Show" && f->type() == Function::ChaserType)
            c = qobject_cast<Chaser *>(f);
    QVERIFY(c != nullptr);
    doc->showControl()->setMainCueList(c->id());
    Fixture *cur = nullptr;
    for (Fixture *f : doc->fixtures())
        if (f->name() == "Main drape 2 curtain")
            cur = f;
    QVERIFY(cur != nullptr);
    ShowTreeController *st = doc->showTree();
    quint32 listId = ShowTree::invalidId();
    st->productionOfChaser(c->id(), &listId);
    const CueList *list = st->cueList(listId);
    QVERIFY(list != nullptr);
    auto cueId = [&](const char *n) {
        for (int i = 0; i < list->cues.count(); ++i)
            if (st->cueNumber(*list, i) == QLatin1String(n))
                return list->cues.at(i).id;
        return ShowTree::invalidId();
    };
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    m_app->m_tab->setCurrentIndex(m_app->m_tab->indexOf(mon));
    mon->updateView();
    settle(500);
    const quint32 sid = 1, fid = cur->id();
    QVERIFY(mon->m_graphicsView->m_sceneryItems.contains(sid));
    const int was = onWire(fid, 0);
    drawnOpen(sid, fid, "loaded:");
    for (const char *n : { "1.0.1", "1.1.1", "1.0.2.1", "1.1.2" })
    {
        st->pickCue(cueId(n));
        settle(3500);
        QVERIFY2(drawnOpen(sid, fid, n) == 1.0, n);
        QVERIFY2(markShown(sid), n);
        QCOMPARE(onWire(fid, 0), was);
    }
    st->stopPreview();
    stopAll();
    doc->resetModified();
}

QTEST_MAIN(CurtainDraw_Test)

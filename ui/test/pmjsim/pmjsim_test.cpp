/*
  Q Light Controller Plus
  pmjsim_test.cpp

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
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QDialog>
#include <QMessageBox>
#include <QTimer>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "programmingmanager.h"
#undef protected
#undef private
#include "simmididevice.h"
#include "controlsurfaceengine.h"
#include "pmjoverlay.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "ioplugincache.h"
#include "showcontrol.h"
#include "controlmap.h"
#include "deskrecord.h"
#include "outputrulekeeper.h"
#include "surfacescenarios.h"
#include "mastertimer.h"
#include "cuerunner.h"
#include "chaser.h"
#include "qlcfile.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "function.h"
#include "surfacepages.h"
#include "fixturegroup.h"
#include "workbench.h"
#include "scene.h"
#include "doc.h"

#include "pmjsim_test.h"

namespace
{
const QString kPmj = QStringLiteral("PMJ Black 1");

/* The PMJ's keys the overlay gives no job, on purpose: free for a show to
   learn (a GO, a hit, the stop -- docs/features/control-surfaces.md, "Check
   it on the real device" 6-7). A key here that gets a role, or a key with
   no role that is not here, fails: say which in this table. */
const QStringList kFreeKeys = { "Favorites", "1-Down", "2-Down", "3-Down", "4-Down", "5-Down",
                                "6-Down", "7-Down", "8-Down", "9-Down", "10-Down" };

/* OpenDeck's steady LED levels (the low bits of the velocity blink). */
const QList<int> kSteady = { 0, 15, 31, 47, 63, 79, 95, 111, 127 };
/* The one blink the app sends on purpose: 500 ms, near full (board Q327). */
const int kBlink = 119;
const QStringList kPages = { "Effects", "Groups", "Looks", "Macros", "Fix Cont" };

void settle(int ms = 0)
{
    for (int i = 0; i < 3; ++i)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
    if (ms > 0)
        QTest::qWait(ms);
}

/* A window a press opened that waits for an answer would hang the test:
   each is named in the log and answered No / Cancel. */
QStringList s_dialogs;
void closeDialogs()
{
    QWidget *w = QApplication::activeModalWidget();
    if (w == nullptr)
        return;
    QString text = w->windowTitle();
    if (QMessageBox *mb = qobject_cast<QMessageBox *>(w))
        text += QStringLiteral(": ") + mb->text();
    s_dialogs << text;
    qWarning().noquote() << "[PmjSim] a press opened" << w->metaObject()->className() << text;
    if (QDialog *d = qobject_cast<QDialog *>(w))
        d->reject();
    else
        w->close();
}

bool hasLamp(const QLCInputChannel *ich)
{
    return ich->type() == QLCInputChannel::Button && (ich->lowerValue() != 0 || ich->upperValue() != UCHAR_MAX);
}
}

void PmjSim_Test::initTestCase()
{
    // No real plugin (and so no real board of this machine): the stub only.
    m_pluginDir = new QTemporaryDir();
    QVERIFY(m_pluginDir->isValid());
    qputenv("QLC_IOPLUGIN_DIR", m_pluginDir->path().toLocal8Bit());
    QTimer *closer = new QTimer(this);
    connect(closer, &QTimer::timeout, this, &closeDialogs);
    closer->start(50);
    m_app = new App();
    m_app->init();
    m_app->doc()->showControl()->setGoGuardMs(0);
    m_app->showNormal();
    m_app->resize(1280, 800);
    QDir stubDir(QStringLiteral("../../../engine/test/iopluginstub"));
    stubDir.setFilter(QDir::Files);
    stubDir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    IOPluginCache *cache = m_app->doc()->ioPluginCache();
    if (cache->plugin(QStringLiteral("I/O Plugin Stub")) == nullptr)
        cache->load(stubDir);
    m_stub = static_cast<IOPluginStub *>(cache->plugin(QStringLiteral("I/O Plugin Stub")));
    QVERIFY2(m_stub != nullptr, "the I/O plugin stub did not load");
    m_stub->m_feedback.clear();
    m_stub->m_feedbackLines.clear();
    m_stub->m_feedbackParams.clear();

    // The board: its real profile, patched in and out as a person patches it.
    m_pmj = new SimMidiDevice(m_app->doc(), m_stub, kPmj, 0, kPmj,
                              QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/PMJ-Black-1.qxi"));
    QVERIFY2(m_pmj->profile() != nullptr, "the PMJ Black 1 profile did not load");
    /* His patch (2026-11-25): the feedback line on the MIDI plugin's own
       default for an output, channel 1 (dink's saved setting), and a DMX
       output on the board's universe beside it. */
    m_pmj->setFeedbackLineChannel(0);
    /* The DMX line first: a replug (theReplugDumpGoesNothingOnThePmj) puts
       the board's line back last, where it was. */
    m_stub->m_lineNames.removeAll(QStringLiteral("(no device)"));
    m_stub->m_lineNames.prepend(QStringLiteral("ArtNet 172.18.2.17"));
    QVERIFY2(m_pmj->plug(), qPrintable(m_pmj->lastError()));
    QVERIFY(m_app->doc()->inputOutputMap()->setOutputPatch(0, m_stub->name(), QStringLiteral("ArtNet 172.18.2.17"), 0));
    makeShow();
    CueRunner *runTab = m_app->findChild<CueRunner *>(QStringLiteral("runTab"));
    QVERIFY(runTab != nullptr);
    m_app->m_tab->setCurrentWidget(runTab);
    settle(50);
}

void PmjSim_Test::cleanupTestCase()
{
    settle();
    if (m_app != nullptr)
    {
        m_app->doc()->masterTimer()->stopAllFunctions();
        m_app->doc()->setMode(Doc::Design);
        settle(50);
        m_app->doc()->resetModified();
    }
    delete m_pmj;
    m_pmj = nullptr;
    if (m_stub != nullptr)
    {
        m_stub->m_lineNames.clear();
        m_stub->m_linesAreHardware = false;
    }
    delete m_app;
    m_app = nullptr;
    delete m_pluginDir;
    m_pluginDir = nullptr;
}

void PmjSim_Test::makeShow()
{
    Doc *doc = m_app->doc();
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("PMJ Dimmer");
    def->setType(QLCFixtureDef::Dimmer);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("1ch");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setGroup(QLCChannel::Intensity);
    ch->setControlByte(QLCChannel::MSB);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    QLCFixtureHead head;
    head.addChannel(0);
    mode->insertHead(-1, head);
    def->addMode(mode);
    QList<quint32> dimmers;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *fxi = new Fixture(doc);
        fxi->setName(QString("Dimmer %1").arg(i + 1));
        fxi->setFixtureDefinition(def, mode);
        fxi->setUniverse(0);
        fxi->setAddress(quint32(i));
        doc->addFixture(fxi);
        dimmers << fxi->id();
    }
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer);
    full->setName("Full");
    full->setValue(255);
    doc->addPalette(full);
    ShowTreeController *st = doc->showTree();
    const quint32 prod = st->addProduction("Macbeth");
    m_list = st->addCueList(prod, "Act One", ShowTree::ListMode::NonTracking);
    QList<quint32> cues;
    for (const char *name : { "Opening", "Ballad", "Finale" })
        cues << st->addCue(m_list, int(cues.size()) - 1, QString::fromUtf8(name));
    ShowTree::Target t;
    t.fixtures = { dimmers.first() };
    for (quint32 cue : std::as_const(cues))
        QVERIFY(st->setLayerLook(cue, st->addLayer(cue, t, "Front"), st->lookForPalette(full->id())));
    ShowTree::Target h;
    h.fixtures = { dimmers.last() };
    m_breathe = st->addHit(ShowTree::HitScope::Production, prod, QStringLiteral("Breathe"));
    QVERIFY(m_breathe != ShowTree::invalidId());
    QVERIFY(st->setLayerLook(m_breathe, st->addLayer(m_breathe, h, "Breathe"), st->lookForPalette(full->id())));
    QVERIFY(st->setCurrentProduction(prod) || st->currentProduction() == prod);
    doc->showControl()->setMainCueList(st->cueList(m_list)->chaser);
    settle(50);
}

void PmjSim_Test::theOverlayIsTheProfile()
{
    QStringList problems;
    const QMap<quint32, QLCInputChannel *> chans = m_pmj->profile()->channels();
    for (auto it = chans.constBegin(); it != chans.constEnd(); ++it)
    {
        const QString overlay = PMJOverlay::controlName(it.key());
        if (overlay.isEmpty())
            problems << QStringLiteral("%1 '%2': in the profile, not the overlay's").arg(it.key()).arg(it.value()->name());
        else if (overlay != it.value()->name())
            problems << QStringLiteral("%1: the profile's '%2' is the overlay's '%3'").arg(it.key()).arg(it.value()->name(), overlay);
    }
    for (quint32 ch : PMJOverlay::controlChannels())
        if (!chans.contains(ch))
            problems << QStringLiteral("%1 '%2': the overlay acts on it, the profile has no such control")
                            .arg(ch).arg(PMJOverlay::controlName(ch));
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QStringLiteral("\n"))));
}

void PmjSim_Test::everyControlReachesItsRole()
{
    Doc *doc = m_app->doc();
    InputOutputMap *iom = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    settle();
    QSignalSpy roles(m_app->m_controlSurfaceEngine, &ControlSurfaceEngine::roleActivated);
    QStringList problems, notWired;
    const QList<quint32> acted = PMJOverlay::controlChannels();
    const QMap<quint32, QLCInputChannel *> chans = m_pmj->profile()->channels();
    for (auto it = chans.constBegin(); it != chans.constEnd(); ++it)
    {
        const QString name = it.value()->name();
        roles.clear();
        qInfo().noquote() << "PMJ control" << it.key() << name;
        // A press and its release, as a person makes them (a fader: up and back).
        QVERIFY2(m_pmj->tap(name, 5), qPrintable(m_pmj->lastError()));
        bool reached = false;
        for (const QList<QVariant> &a : std::as_const(roles))
            if (a.at(0).toString() == PMJOverlay::deviceId)
                reached = true;
        const bool free = kFreeKeys.contains(name);
        if (reached && free)
            problems << name + QStringLiteral(": documented free, but it has a role now (update kFreeKeys)");
        else if (!reached && !free)
            problems << name + QStringLiteral(": reaches no role and is not documented free");
        if (reached && !acted.contains(it.key()))
            notWired << name;
    }
    // What the presses toggled, back as it was. Go GOes the Run tab's list
    // now (in Design too, as the Run tab's GO): stopped again.
    doc->masterTimer()->stopAllFunctions();
    doc->masterTimer()->timerTick();
    iom->setBlackout(false);
    iom->setOutputInhibited(false);
    iom->setGrandMasterValue(255);
    settle();
    /* A role the overlay registers but does nothing with (yet): received,
       so not a problem here; the scenario tests below say what each should
       do. Named so the log shows the gap. */
    qInfo().noquote() << "PMJ controls with a role and no action yet:" << notWired.join(QStringLiteral(", "));
    if (!s_dialogs.isEmpty())
        qInfo().noquote() << "PMJ presses opened:" << s_dialogs.join(QStringLiteral("; "));
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QStringLiteral("\n"))));
}

void PmjSim_Test::everyLampIsSentAndNoneStray()
{
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Design);
    settle();
    m_pmj->forgetLamps();
    // What the board shows after a replug: every lamp repainted.
    m_app->m_controlSurfaceEngine->refreshDevice(PMJOverlay::deviceId);
    settle();
    const QMap<quint32, int> lamps = m_pmj->lamps();
    QStringList problems, blinking;
    const QMap<quint32, QLCInputChannel *> chans = m_pmj->profile()->channels();
    int withLamp = 0;
    for (auto it = chans.constBegin(); it != chans.constEnd(); ++it)
    {
        const QString name = it.value()->name();
        if (!hasLamp(it.value()))
        {
            if (lamps.contains(it.key()))
                problems << name + QStringLiteral(": has no lamp, was sent one (%1)").arg(lamps.value(it.key()));
            continue;
        }
        withLamp++;
        if (!lamps.contains(it.key()))
        {
            problems << name + QStringLiteral(": its lamp was never sent");
            continue;
        }
        const int level = lamps.value(it.key());
        if (level == kBlink)
            blinking << name;
        else if (!kSteady.contains(level))
            problems << name + QStringLiteral(": lamp %1 is not a steady level (it blinks)").arg(level);
        // A mode button is a page to switch to (lit) or the page in use (blinking).
        if (kPages.contains(name) && level != 127 && level != kBlink)
            problems << name + QStringLiteral(": a page lamp at %1, neither lit nor blinking").arg(level);
        // A key with no job in a show with none learned: dark, never a stray light.
        if (kFreeKeys.contains(name) && level != 0)
            problems << name + QStringLiteral(": a free key lit (%1)").arg(level);
    }
    for (auto it = lamps.constBegin(); it != lamps.constEnd(); ++it)
        if (!chans.contains(it.key()))
            problems << QStringLiteral("%1: a lamp sent to a control the PMJ does not have").arg(it.key());
    QCOMPARE(withLamp, 54);     // the profile's <Feedback> controls
    // Only what is in use blinks: here, the one page the strips are on.
    QCOMPARE(blinking.count(), 1);
    QVERIFY2(kPages.contains(blinking.first()), qPrintable(blinking.join(QStringLiteral(", "))));
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QStringLiteral("\n"))));
    // Another page pressed: it blinks, and the one before is lit again.
    const QString other = blinking.first() == QLatin1String("Groups") ? QStringLiteral("Looks") : QStringLiteral("Groups");
    QVERIFY(m_pmj->tap(other, 20));
    settle(20);
    QCOMPARE(m_pmj->lamp(other), kBlink);
    QCOMPARE(m_pmj->lamp(blinking.first()), 127);
}

void PmjSim_Test::theMasterFaderMovesTheGrandMaster()
{
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    iom->setGrandMasterValue(255);
    settle();
    // Down all the way, as a hand pulls it.
    QVERIFY(m_pmj->ramp(QStringLiteral("Master"), 127, 0, 60));
    QCOMPARE(int(iom->grandMasterValue()), 0);
    QVERIFY(m_pmj->set(QStringLiteral("Master"), 64));
    QCOMPARE(int(iom->grandMasterValue()), 128);
    QVERIFY(m_pmj->ramp(QStringLiteral("Master"), 64, 127, 60));
    QCOMPARE(int(iom->grandMasterValue()), 255);
    // In Operate too: the Grand Master is not an edit.
    m_app->doc()->setMode(Doc::Operate);
    settle();
    QVERIFY(m_pmj->set(QStringLiteral("Master"), 100));
    QCOMPARE(int(iom->grandMasterValue()), 200);
    QVERIFY(m_pmj->set(QStringLiteral("Master"), 127));
    QCOMPARE(int(iom->grandMasterValue()), 255);
    m_app->doc()->setMode(Doc::Design);
    settle();
}

void PmjSim_Test::goInTheRunTabGoesTheList()
{
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Operate);
    settle(20);
    Function *list = doc->function(doc->showTree()->cueList(m_list)->chaser);
    QVERIFY(list != nullptr);
    // From a stopped list, whatever an earlier press played.
    doc->masterTimer()->stopAllFunctions();
    doc->masterTimer()->timerTick();
    settle(20);
    QVERIFY(!list->isRunning());
    QSignalSpy handled(doc->showControl(), &ShowControl::commandHandled);

    QVERIFY(m_pmj->tap(QStringLiteral("Go"), 60));
    doc->masterTimer()->timerTick();
    settle(30);
    QVERIFY2(list->isRunning(), "Go on the PMJ did not GO the Run tab's list");
    QVERIFY2(handled.count() >= 1, "no command reached ShowControl");

    doc->masterTimer()->stopAllFunctions();
    doc->setMode(Doc::Design);
    settle(20);
}

void PmjSim_Test::aLearnedHitLightsItsLamp()
{
    Doc *doc = m_app->doc();
    ControlMap *cm = doc->controlMap();
    const QString key = QStringLiteral("1-Down");
    bool ok = false;
    const quint32 ch = m_pmj->channelOf(key, &ok);
    QVERIFY(ok);
    cm->setDimWhenReady(true);
    cm->setBinding({ 0, ch, ControlMap::hit(m_breathe), ControlMap::Toggle });
    doc->setMode(Doc::Operate);
    settle(50);
    /* On the PMJ a key a press would fire now is lit (127), and stays lit
       while what it fired runs: the board idles dim (15), so "ready" at
       that level could not be told from a key that does nothing. The
       light is what the board was last sent (never forgotten here: a lamp
       that does not change is not sent again). */
    QTRY_COMPARE(m_pmj->lamp(ch), 127);

    QVERIFY(m_pmj->tap(key, 60));
    for (int i = 0; i < 3; ++i)
    {
        doc->masterTimer()->timerTick();
        settle(20);
    }
    Function *f = doc->function(doc->showTree()->lookFunction(m_breathe));
    QVERIFY(f != nullptr);
    QTRY_VERIFY2(f->isRunning(), "the learned key did not fire the hit");
    settle(50);
    /* The bench saw the hit fire with its lamp dark. Through the board's
       own path it lights here (2026-11-25), and stays lit through what
       repaints the board -- a Master move, a page key, a full refresh --
       so the bench case is not this one: see SURFACE_SIMULATOR.md. */
    // Running: it blinks (board Q327), the board's own blink.
    QTRY_COMPARE(m_pmj->lamp(ch), kBlink);
    QVERIFY(m_pmj->ramp(QStringLiteral("Master"), 127, 90, 30));
    QVERIFY(m_pmj->set(QStringLiteral("Master"), 127));
    QVERIFY(m_pmj->tap(QStringLiteral("Looks"), 20));
    m_app->m_controlSurfaceEngine->refreshDevice(PMJOverlay::deviceId);
    settle(30);
    QVERIFY2(m_pmj->lamp(ch) == kBlink,
             qPrintable(QStringLiteral("the hit plays, its key's lamp went to %1").arg(m_pmj->lamp(ch))));
    // Let go: still lit, a press would fire it again (its place is live).
    QVERIFY(m_pmj->tap(key, 60));
    for (int i = 0; i < 3; ++i)
    {
        doc->masterTimer()->timerTick();
        settle(20);
    }
    QTRY_VERIFY(!f->isRunning());
    QTRY_COMPARE(m_pmj->lamp(ch), 127);     // armed again: lit

    // Unlearned: a free key again, dark.
    cm->removeBinding(ControlMap::hit(m_breathe));
    settle(20);
    QTRY_COMPARE(m_pmj->lamp(ch), 0);
    doc->masterTimer()->stopAllFunctions();
    doc->setMode(Doc::Design);
    settle(20);
}

/* Phase R on the PMJ's own Go: after a replug the board dumps its keys,
   in either shape ({127}, and OpenDeck's at rest / lamp echo / at rest),
   and the overlay GOes nothing (it asks the Control Map's one rule); a
   release and a press 300 ms on is one GO, and a held key resending is
   still the one press. */
void PmjSim_Test::theReplugDumpGoesNothingOnThePmj()
{
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Operate);
    ControlMap *cm = doc->controlMap();
    cm->setDumpWindowMs(5000);          // never over while the test runs
    Function *list = doc->function(doc->showTree()->cueList(m_list)->chaser);
    QVERIFY(list != nullptr);
    QSignalSpy handled(doc->showControl(), &ShowControl::commandHandled);
    auto stopped = [&]() {
        doc->masterTimer()->stopAllFunctions();
        doc->masterTimer()->timerTick();
        settle(20);
        return !list->isRunning();
    };
    const QString go = QStringLiteral("Go");
    const QList<QList<QPair<QString, int>>> shapes = { { { go, 127 } }, { { go, 0 }, { go, 127 }, { go, 0 } } };
    for (const auto &dump : shapes)
    {
        QVERIFY(stopped());
        handled.clear();
        QVERIFY2(SurfaceScenarios::unplugReplug(m_pmj, dump), qPrintable(m_pmj->lastError()));
        doc->masterTimer()->timerTick();
        settle(20);
        QVERIFY2(!list->isRunning(), qPrintable(QStringLiteral("the PMJ's dump of %1 values GOed the list")
                                                    .arg(dump.count())));
        QCOMPARE(handled.count(), 0);

        // 300 ms on, a hand: a release and a press, one GO.
        QTest::qWait(300);
        QVERIFY(m_pmj->set(go, 0));
        QVERIFY(m_pmj->tap(go, 60));
        doc->masterTimer()->timerTick();
        settle(30);
        QVERIFY2(list->isRunning(), "a press after the burst did not GO");
        QCOMPARE(handled.count(), 1);
    }
    // Held, the key resending: one press.
    QVERIFY(stopped());
    handled.clear();
    QVERIFY(m_pmj->set(go, 127));
    QVERIFY(m_pmj->set(go, 127));
    QVERIFY(m_pmj->set(go, 127));
    QVERIFY(m_pmj->set(go, 0));
    doc->masterTimer()->timerTick();
    settle(30);
    QCOMPARE(handled.count(), 1);

    cm->setDumpWindowMs(500);
    QVERIFY(stopped());
    doc->setMode(Doc::Design);
    settle(20);
}

/* This computer's STOP on a PMJ key: the Control Map lights it, with the
   show's stop's rule on a board that idles dim -- lit at every moment,
   blinking lit and dim after an ALL OFF -- and the overlay's own repaint
   (a full refresh, a Master move) never writes it. Its GO on the Go key
   likewise. */
void PmjSim_Test::theDesksStopIsLitOnThePmj()
{
    Doc *doc = m_app->doc();
    ControlMap *cm = doc->controlMap();
    DeskRecord *desk = doc->deskRecord();
    QVERIFY(desk != nullptr);
    bool ok = false;
    const quint32 fav = m_pmj->channelOf(QStringLiteral("Favorites"), &ok);
    QVERIFY(ok);
    const quint32 goKey = m_pmj->channelOf(QStringLiteral("Go"), &ok);
    QVERIFY(ok);
    auto bind = [&](DeskRecord::Role role, quint32 channel) {
        DeskRecord::Binding b;
        b.identity = desk->identityOfUniverse(0);
        b.device = kPmj;
        b.plugin = QStringLiteral("MIDI");
        b.channel = channel;
        b.control = QStringLiteral("#%1").arg(channel);
        return desk->setBinding(role, b) && desk->isLive(role);
    };
    auto unbind = qScopeGuard([desk]() {
        desk->clear(DeskRecord::Stop);
        desk->clear(DeskRecord::Go);
    });
    doc->masterTimer()->stopAllFunctions();
    doc->masterTimer()->timerTick();
    settle(20);
    QVERIFY(bind(DeskRecord::Stop, fav));
    QVERIFY(bind(DeskRecord::Go, goKey));
    settle(30);
    QCOMPARE(m_pmj->lamp(fav), 127);
    QCOMPARE(m_pmj->lamp(goKey), 127);      // a GO would play the list

    // The overlay repaints the board: those two keys are not its to write.
    const int favSent = m_pmj->lampMessages(fav);
    m_app->m_controlSurfaceEngine->refreshDevice(PMJOverlay::deviceId);
    QVERIFY(m_pmj->ramp(QStringLiteral("Master"), 127, 100, 30));
    QVERIFY(m_pmj->set(QStringLiteral("Master"), 127));
    settle(30);
    QCOMPARE(m_pmj->lampMessages(fav), favSent);
    QCOMPARE(m_pmj->lamp(fav), 127);
    QCOMPARE(m_pmj->lamp(goKey), 127);

    // The desk's STOP pressed: ALL OFF, and its key blinks lit and dim.
    QVERIFY(m_pmj->tap(QStringLiteral("Favorites"), 30));
    settle(30);
    QTRY_VERIFY(doc->outputRuleKeeper()->emergencyStopped());
    QTRY_COMPARE(m_pmj->lamp(fav), 15);
    QTRY_COMPARE(m_pmj->lamp(fav), 127);
    QTRY_COMPARE(m_pmj->lamp(goKey), 15);   // nothing is ready after ALL OFF
    doc->outputRuleKeeper()->noteGo();
    settle(30);
    QTRY_COMPARE(m_pmj->lamp(fav), 127);
    QTRY_COMPARE(m_pmj->lamp(goKey), 127);

    // Let go of: the overlay's own lamps again (Favorites is a free key: dark).
    unbind.dismiss();
    QVERIFY(desk->clear(DeskRecord::Stop));
    QVERIFY(desk->clear(DeskRecord::Go));
    settle(30);
    QTRY_COMPARE(m_pmj->lamp(fav), 0);
    QCOMPARE(cm->deskLamps().count(), 0);
}

namespace
{
QString hex(const QByteArray &b)
{
    return QString::fromLatin1(b.toHex(' '));
}
QString ledFile(const char *name)
{
    return QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("../engine/test/surfacesim/captures/") + QLatin1String(name);
}
}

void PmjSim_Test::hisRigLightsGoAndBackOnChannelNine()
{
    Doc *doc = m_app->doc();
    QString err;
    const QList<SimMidiDevice::Led> leds = SimMidiDevice::loadLeds(ledFile("pmj-leds-aligned-20261125.json"), &err);
    QVERIFY2(!leds.isEmpty(), qPrintable(err));
    m_pmj->setLeds(leds);
    auto restore = qScopeGuard([this]() { m_pmj->setLeds({}); });
    QVERIFY(doc->inputOutputMap()->universe(0)->outputPatchesCount() >= 1);     // DMX beside it, as his
    Function *list = doc->function(doc->showTree()->cueList(m_list)->chaser);
    QVERIFY(list != nullptr);
    doc->masterTimer()->stopAllFunctions();
    doc->masterTimer()->timerTick();
    doc->setMode(Doc::Operate);
    settle(50);
    QVERIFY(!list->isRunning());
    bool ok = false;
    const quint32 go = m_pmj->channelOf(QStringLiteral("Go"), &ok);
    QVERIFY(ok);
    const quint32 back = m_pmj->channelOf(QStringLiteral("Back"), &ok);
    QVERIFY(ok);
    auto lastBytes = [&](quint32 ch) { return m_pmj->lastMidi(ch); };
    m_pmj->forgetLamps();
    m_app->m_controlSurfaceEngine->refreshDevice(PMJOverlay::deviceId);
    settle(30);
    // A list that can GO: Go lit, note 0 on MIDI channel 9 (0x98), velocity 127.
    QCOMPARE(hex(lastBytes(go)), QStringLiteral("98 00 7f"));
    QCOMPARE(m_pmj->lampUnder(QStringLiteral("Go")), 127);

    // GO from the board, as he does: the list runs, Back can go back.
    QVERIFY(m_pmj->tap(QStringLiteral("Go"), 60));
    doc->masterTimer()->timerTick();
    settle(50);
    QVERIFY2(list->isRunning(), "Go on the PMJ did not GO the Run tab's list");
    QTRY_COMPARE(m_pmj->lampUnder(QStringLiteral("Back")), 127);
    QCOMPARE(hex(lastBytes(back)), QStringLiteral("98 0b 7f"));
    QCOMPARE(m_pmj->lampUnder(QStringLiteral("Go")), 127);     // the next cue can GO

    // To the last cue: nothing left to GO, Go dim; Back still lit.
    const Chaser *chaser = qobject_cast<const Chaser *>(list);
    for (int i = 0; i < 4 && doc->showControl()->standbyStep(chaser) >= 0; ++i)
    {
        QVERIFY(m_pmj->tap(QStringLiteral("Go"), 60));
        doc->masterTimer()->timerTick();
        settle(50);
    }
    QCOMPARE(doc->showControl()->standbyStep(chaser), -1);
    QTRY_COMPARE(m_pmj->lampUnder(QStringLiteral("Go")), 15);
    QCOMPARE(hex(lastBytes(go)), QStringLiteral("98 00 0f"));
    QCOMPARE(m_pmj->lampUnder(QStringLiteral("Back")), 127);

    doc->masterTimer()->stopAllFunctions();
    doc->masterTimer()->timerTick();
    doc->setMode(Doc::Design);
    settle(30);
}

void PmjSim_Test::anUnalignedBoardLightsNoButtonOfItsOwn()
{
    Doc *doc = m_app->doc();
    QString err;
    const QList<SimMidiDevice::Led> leds = SimMidiDevice::loadLeds(ledFile("pmj-leds-20261125.json"), &err);
    QVERIFY2(!leds.isEmpty(), qPrintable(err));
    m_pmj->setLeds(leds);
    auto restore = qScopeGuard([this]() { m_pmj->setLeds({}); });
    doc->setMode(Doc::Operate);
    settle(30);
    m_pmj->forgetLamps();
    m_app->m_controlSurfaceEngine->refreshDevice(PMJOverlay::deviceId);
    settle(30);
    // Every lamp goes out on its button's own address (channel 9): none of
    // this board's LEDs listens there, so no button lights.
    QStringList lit;
    for (const SimMidiDevice::Led &l : leds)
        if (!l.under.isEmpty() && m_pmj->lampUnder(l.under) > 0)
            lit << l.under;
    QVERIFY2(lit.isEmpty(), qPrintable(QStringLiteral("lit on an unaligned board: ") + lit.join(QStringLiteral(", "))));
    QVERIFY(m_pmj->lamp(QStringLiteral("Go")) > 0);       // sent all the same
    // His 4667 symptoms, as the board takes channel-1 lamps: the Effects
    // page lamp (note 50) lights the LED under 1-Load, Set's (49) the one
    // under Effects, O's (22) the one under 10-Load; Go's (0) none.
    m_pmj->boardReceives(QByteArray::fromHex("90327f"));
    m_pmj->boardReceives(QByteArray::fromHex("90317f"));
    m_pmj->boardReceives(QByteArray::fromHex("90167f"));
    m_pmj->boardReceives(QByteArray::fromHex("90007f"));
    QCOMPARE(m_pmj->lampUnder(QStringLiteral("1-Load")), 127);
    QCOMPARE(m_pmj->lampUnder(QStringLiteral("Effects")), 127);
    QCOMPARE(m_pmj->lampUnder(QStringLiteral("10-Load")), 127);
    QCOMPARE(m_pmj->lampUnder(QStringLiteral("Go")), 0);
    doc->setMode(Doc::Design);
    settle(20);
}

int main(int argc, char **argv)
{
    // A home of its own: nothing touches yours (as padhit).
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    // A hang says so in a minute, not five.
    if (qEnvironmentVariableIsEmpty("QTEST_FUNCTION_TIMEOUT"))
        qputenv("QTEST_FUNCTION_TIMEOUT", "60000");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    PmjSim_Test test;
    return QTest::qExec(&test, argc, argv);
}void PmjSim_Test::theGroupsFaderPicksUpAndAnEmptyPageIsDim()
{
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Design);
    // A scene of one group (both dimmers), open in Build.
    QList<quint32> dimmers;
    for (Fixture *f : doc->fixtures())
        dimmers << f->id();
    QVERIFY(dimmers.size() >= 2);
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName(QStringLiteral("Front"));
    g->setSize(QSize(2, 1));
    doc->addFixtureGroup(g);
    g->assignFixture(dimmers.at(0), QLCPoint(0, 0));
    g->assignFixture(dimmers.at(1), QLCPoint(1, 0));
    Scene *s = new Scene(doc);
    s->setName(QStringLiteral("Groups look"));
    doc->addFunction(s);
    s->addFixtureGroup(g->id());
    ProgrammingManager *pm = m_app->findChild<ProgrammingManager *>();
    QVERIFY(pm != nullptr && pm->m_workbench != nullptr);
    pm->m_workbench->load(s->id());
    settle();
    QCOMPARE(pm->currentSceneId(), s->id());

    SurfacePages *pages = doc->controlMap()->surfacePages();
    QVERIFY(m_pmj->tap(QStringLiteral("Groups")));
    settle();
    QCOMPARE(pages->pageName(m_pmj->universe()), QStringLiteral("Groups"));
    auto level = [doc, &dimmers]() { return int(doc->programmerValues().value(dimmers.at(0)).value(0, 0)); };
    const int before = level();

    // Ch 1 far from the group's level: it waits.
    QVERIFY(m_pmj->set(QStringLiteral("Ch 1"), 100));
    settle();
    QCOMPARE(level(), before);
    // Down through it: picked up, then it follows.
    QVERIFY(m_pmj->set(QStringLiteral("Ch 1"), 0));
    QVERIFY(m_pmj->set(QStringLiteral("Ch 1"), 50));
    settle();
    QCOMPARE(level(), 100);

    // Effects has no jobs yet: its key dim, not dark, not lit; Groups lit.
    QVERIFY(m_pmj->tap(QStringLiteral("Groups")));
    settle();
    const int effects = m_pmj->lamp(QStringLiteral("Effects"));
    QVERIFY2(effects > 0 && effects < 127, qPrintable(QString::number(effects)));
    QCOMPARE(m_pmj->lamp(QStringLiteral("Groups")), 127);
    QCOMPARE(m_pmj->lamp(QStringLiteral("Fix Cont")), 0);
    QVERIFY(m_pmj->tap(QStringLiteral("Looks")));
    settle();
    pm->m_workbench->load(Function::invalidId());
    doc->deleteFunction(s->id());
    doc->deleteFixtureGroup(g->id());
    settle();
}



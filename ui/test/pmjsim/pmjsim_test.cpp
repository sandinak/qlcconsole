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
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
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
#include "mastertimer.h"
#include "cuerunner.h"
#include "qlcfile.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "function.h"
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
    QVERIFY2(m_pmj->plug(), qPrintable(m_pmj->lastError()));
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
    // What the presses toggled, back as it was.
    iom->setBlackout(false);
    iom->setOutputInhibited(false);
    iom->setGrandMasterValue(255);
    settle();
    /* A role the overlay registers but does nothing with (yet): received,
       so not a problem here; the scenario tests below say what each should
       do. Named so the log shows the gap. */
    qInfo().noquote() << "PMJ controls with a role and no action yet:" << notWired.join(QStringLiteral(", "));
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
    QStringList problems;
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
        if (!kSteady.contains(level))
            problems << name + QStringLiteral(": lamp %1 is not a steady level (it blinks)").arg(level);
        // A key with no job in a show with none learned: dark, never a stray light.
        if (kFreeKeys.contains(name) && level != 0)
            problems << name + QStringLiteral(": a free key lit (%1)").arg(level);
    }
    for (auto it = lamps.constBegin(); it != lamps.constEnd(); ++it)
        if (!chans.contains(it.key()))
            problems << QStringLiteral("%1: a lamp sent to a control the PMJ does not have").arg(it.key());
    QCOMPARE(withLamp, 54);     // the profile's <Feedback> controls
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QStringLiteral("\n"))));
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
    QVERIFY(!list->isRunning());
    QSignalSpy handled(doc->showControl(), &ShowControl::commandHandled);

    QVERIFY(m_pmj->tap(QStringLiteral("Go"), 60));
    doc->masterTimer()->timerTick();
    settle(30);
    QEXPECT_FAIL("", "fix/053-pmj-run-buttons wires the PMJ's Go and Back: PMJOverlay registers Transport "
                     "but its slotRoleActivated() does nothing with it", Continue);
    QVERIFY2(list->isRunning(), "Go on the PMJ did not GO the Run tab's list");
    QEXPECT_FAIL("", "fix/053-pmj-run-buttons (as above)", Continue);
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
    m_pmj->forgetLamps();

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
    const int lit = m_pmj->lamp(ch);
    QEXPECT_FAIL("", "fix/053-pmj-run-buttons: a hit learned on a PMJ key fires, its lamp is not lit", Continue);
    QVERIFY2(lit == 127, qPrintable(QStringLiteral("the hit plays, its key's lamp is %1").arg(lit)));

    cm->removeBinding(ControlMap::hit(m_breathe));
    doc->masterTimer()->stopAllFunctions();
    doc->setMode(Doc::Design);
    settle(20);
}

int main(int argc, char **argv)
{
    // A home of its own: nothing touches yours (as padhit).
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    PmjSim_Test test;
    return QTest::qExec(&test, argc, argv);
}

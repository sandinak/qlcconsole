/*
  Q Light Controller Plus
  padrunrulesui_test.cpp

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
#include <QTemporaryDir>
#include <QToolButton>
#include <QLabel>
#include <QtTest>
#include <QScopeGuard>
#include <QRegularExpression>
#include <QDir>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "programmercontroller.h"
#undef protected
#undef private
#include "showtreecontroller.h"
#include "refinesession.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "ioplugincache.h"
#include "iopluginstub.h"
#include "qlcioplugin.h"
#include "showcontrol.h"
#include "mastertimer.h"
#include "padrun.h"
#include "controllerjobs.h"
#include "qlcfile.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "doc.h"
#include "focuscallcue.h"
#include "focuscalls.h"
#include "focussession.h"
#include "fixturegroup.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "qlcpoint.h"
#include "cuerunner.h"
#include "controlmap.h"
#include "scene.h"

#include "padrunrulesui_test.h"
#include "../common/padframe.h"

/* The frame as drawn: the armed/stopped/focus colour on its edges, or none of them. */
static bool frameIs(QWidget *tab, const QColor &want)
{
    const QColor got = padFrameDrawn(tab);
    if (want.isValid())
        return got == want && padFrameShown(tab);
    return got != PadRun::armedColour() && got != PadRun::stoppedColour()
        && got != PadRun::focusColour() && !padFrameShown(tab);
}

namespace
{
const quint32 kLT = 4, kRT = 5, kA = 10, kLB = 14, kRB = 15, kView = 16, kMenu = 17;

QMap<quint32, QString> xboxRoles()
{
    return { { 0, "lx" }, { 1, "ly" }, { 2, "rx" }, { 3, "ry" }, { kLT, "lt" }, { kRT, "rt" },
             { kA, "a" }, { 11, "b" }, { 12, "x" }, { 13, "y" }, { kLB, "lb" }, { kRB, "rb" },
             { kView, "back" }, { kMenu, "start" } };
}

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

QString lastLight(IOPluginStub *stub)
{
    for (int i = stub->m_feedbackParams.size() - 1; i >= 0; --i)
    {
        const QVariantMap p = stub->m_feedbackParams.at(i).toMap();
        if (p.value("gamepad").toString() == QLatin1String("light"))
            return p.value("rgb").toString();
    }
    return QStringLiteral("none");
}
}

void PadRunRulesUi_Test::initTestCase()
{
    // No real plugin (and so no real controller of this machine): the stub only.
    m_pluginDir = new QTemporaryDir();
    QVERIFY(m_pluginDir->isValid());
    qputenv("QLC_IOPLUGIN_DIR", m_pluginDir->path().toLocal8Bit());
    m_app = new App();
    m_app->init();
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
    m_stub->m_channelRoles = xboxRoles();
}

void PadRunRulesUi_Test::cleanupTestCase()
{
    settle();
    if (m_app != nullptr)
    {
        m_app->doc()->programmer()->padRun()->setEnabled(false, false);
        m_app->doc()->masterTimer()->stopAllFunctions();
        m_app->doc()->setMode(Doc::Design);
        settle(50);
        m_app->doc()->resetModified();
    }
    if (m_stub != nullptr)
        m_stub->m_feedbackParams.clear();
    delete m_app;
    m_app = nullptr;
    delete m_pluginDir;
    m_pluginDir = nullptr;
}

void PadRunRulesUi_Test::everySurfaceAgreesAtEveryStep()
{
    Doc *doc = m_app->doc();
    InputOutputMap *iom = doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    ProgrammerController *pc = doc->programmer();
    PadRun *run = pc->padRun();
    run->setArmChordMs(15);

    // A show to play and walk: two spots, one list, two cues with a fade.
    QList<quint32> spots;
    for (int i = 0; i < 2; ++i)
    {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test");
        def->setModel(QString("Surface Spot %1").arg(i));
        def->setType(QLCFixtureDef::MovingHead);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("3ch");
        QLCFixtureHead head;
        const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
        for (int k = 0; k < 3; ++k)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("Ch %1").arg(k));
            ch->setGroup(groups[k]);
            ch->setControlByte(QLCChannel::MSB);
            def->addChannel(ch);
            mode->insertChannel(ch, k);
            head.addChannel(k);
        }
        QLCPhysical ph;
        ph.setFocusPanMax(540);
        ph.setFocusTiltMax(270);
        mode->setPhysical(ph);
        mode->insertHead(-1, head);
        def->addMode(mode);
        Fixture *fxi = new Fixture(doc);
        fxi->setName(QString("Spot %1").arg(i + 1));
        fxi->setFixtureDefinition(def, mode);
        fxi->setUniverse(0);
        fxi->setAddress(quint32(i * 3));
        doc->addFixture(fxi);
        spots << fxi->id();
    }
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer);
    full->setName("Full");
    full->setValue(255);
    doc->addPalette(full);
    ShowTreeController *st = doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Spring"), "Main", ShowTree::ListMode::NonTracking);
    const quint32 cue1 = st->addCue(list, -1, "Opening");
    const quint32 cue2 = st->addCue(list, 0, "Ballad");
    ShowTree::Target t;
    t.fixtures = spots;
    for (quint32 cue : { cue1, cue2 })
    {
        const quint32 layer = st->addLayer(cue, t, "Spots");
        ShowTree::Look l;
        l.key = ShowTree::KeyDimmer;
        l.palettes = { full->id() };
        QVERIFY(st->setLayerLook(cue, layer, l));
        ShowTree::Cue c = *st->cue(cue);
        c.fadeIn = 3000;
        QVERIFY(st->setCue(c));
    }
    pc->refineSession()->setFlashEnabled(false);
    st->pickCue(cue1);
    settle(50);

    QToolButton *btn = m_app->findChild<QToolButton *>(QStringLiteral("crPadRun"));
    QVERIFY(btn != nullptr);
    QWidget *runTab = btn;
    while (runTab != nullptr && QLatin1String(runTab->metaObject()->className()) != QLatin1String("CueRunner"))
        runTab = runTab->parentWidget();
    QVERIFY(runTab != nullptr);
    QLabel *chip = m_app->padRoleChip();
    QVERIFY(chip != nullptr);

    auto value = [&](quint32 ch, uchar v) { emit iom->inputValueChanged(0, ch, v, QString()); };
    auto tap = [&](quint32 ch) { value(ch, 255); value(ch, 0); };
    auto chord = [&](bool on) {
        value(kLB, 255);
        value(kRB, 255);
        QTRY_COMPARE(run->isArmed(), on);
        value(kRB, 0);
        value(kLB, 0);
    };

    /* Every surface, read back as drawn: words, colour, frame, light. */
    auto surfaces = [&](const char *when) {
        settle();
        const QString words = run->roleText();
        const QColor colour = run->stateColour();
        const QString name = colour.isValid() ? colour.name() : QString();
        QVERIFY2(btn->text() == words, qPrintable(QString("%1: button '%2', state '%3'").arg(when, btn->text(), words)));
        if (run->state() == PadRun::Tools)
            QVERIFY2(chip->isHidden(), when);
        else
            QVERIFY2(chip->isVisible() && chip->text().startsWith(words),
                     qPrintable(QString("%1: chip '%2', state '%3'").arg(when, chip->text(), words)));
        QVERIFY2(colour.isValid() ? btn->styleSheet().contains(name) : btn->styleSheet().isEmpty(),
                 qPrintable(QString("%1: button style '%2'").arg(when, btn->styleSheet())));
        QVERIFY2(frameIs(runTab, colour), qPrintable(QString("%1: frame, drawn %2, state %3")
                 .arg(when, padFrameDrawn(runTab).name(), name)));
        if (run->state() != PadRun::Tools)
            QVERIFY2(colour.isValid() ? chip->styleSheet().contains(name)
                                      : !chip->styleSheet().contains(QStringLiteral("background")),
                     qPrintable(QString("%1: chip style '%2'").arg(when, chip->styleSheet())));
        // The pad's light: the state's colour, where the state has the light.
        const PadRun::State s = run->state();
        if (!run->isPlaying() && (s == PadRun::Run || s == PadRun::Armed || s == PadRun::Stopped
                                  || s == PadRun::Tools))
        {
            QVERIFY2(pc->padLight() == colour, qPrintable(QString("%1: light %2, state colour %3")
                     .arg(when, pc->padLight().name(), name)));
            const QString sent = lastLight(m_stub);
            // (A pad that is not there is sent nothing: only one that is.)
            const InputPatch *ip = iom->inputPatch(0);
            if (ip != nullptr && ip->plugin() != nullptr)
                QVERIFY2(sent == QLatin1String("none") || sent == name,
                     qPrintable(QString("%1: the pad was last sent '%2', the state is '%3'").arg(when, sent, name)));
        }
    };

    doc->setMode(Doc::Design);       // Focus this cue from the laptop: Design
    settle(50);
    surfaces("Tools, at the start");
    btn->click();
    QVERIFY(run->isEnabled());
    surfaces("the Run role on");
    chord(true);
    surfaces("armed");

    // A tool started on the laptop while armed, then ended.
    QVERIFY(pc->refineSession()->start(cue1, 0xFFFFFFFF, nullptr));
    surfaces("a tool took the pad while armed");
    QVERIFY(!run->isArmed());
    pc->refineSession()->stop();
    surfaces("the tool ended");

    // GO (Operate): the cue fades 3 s; the role switched off from the screen mid-fade.
    doc->setMode(Doc::Operate);
    settle(50);
    chord(true);
    value(kLT, 255);
    tap(kA);
    value(kLT, 0);
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(list) != 0xFFFFFFFF, 3000);
    surfaces("armed, a cue fading");
    QTest::qWait(600);
    btn->click();
    QVERIFY(!run->isEnabled());
    surfaces("the role off mid-fade");
    QTest::qWait(300);
    surfaces("the role off, the fade going on");
    btn->click();
    surfaces("the role on again");
    doc->masterTimer()->stopAllFunctions();
    doc->setMode(Doc::Design);
    settle(50);

    // The Controller Test opened while armed, closed with both triggers held.
    chord(true);
    value(kLT, 255);
    value(kRT, 255);
    run->setTesting(true);
    surfaces("the Controller Test while armed");
    run->setTesting(false);
    surfaces("the test closed, triggers held");
    value(kRT, 0);
    value(kLT, 0);

    // The emergency stop; a tool there and back; a walk there and back.
    value(kLB, 255);
    value(kRB, 255);
    value(kLT, 255);
    value(kRT, 255);
    surfaces("the emergency stop");
    value(kRT, 0);
    value(kLT, 0);
    QTest::qWait(60);
    surfaces("the stop's triggers let go, bumpers held");
    value(kRB, 0);
    value(kLB, 0);
    QVERIFY(pc->refineSession()->start(cue1, 0xFFFFFFFF, nullptr));
    pc->refineSession()->stop();
    surfaces("stopped, after a tool");
    doc->setMode(Doc::Design);
    settle(50);
    tap(kMenu);
    QVERIFY(run->isWalking());
    surfaces("walking from stopped");
    tap(kMenu);
    surfaces("stopped, after a walk");

    // The stop while walking.
    chord(true);
    chord(false);
    surfaces("disarmed");
    tap(kMenu);
    QVERIFY(run->isWalking());
    value(kLB, 255);
    value(kRB, 255);
    value(kLT, 255);
    value(kRT, 255);
    surfaces("the emergency stop while walking");
    value(kRT, 0);
    value(kLT, 0);
    value(kRB, 0);
    value(kLB, 0);
    if (run->isWalking())
        pc->refineSession()->stop();
    surfaces("the walk ended after the stop");

    /* View + Menu held, armed (Q251): the role stays, the arm stays, and
       the chip says where Run / Tools is switched. */
    chord(true);
    run->setRoleChordMs(40);
    value(kView, 255);
    value(kMenu, 255);
    QTRY_COMPARE(run->notice(), PadRun::roleSwitchText());
    value(kMenu, 0);
    value(kView, 0);
    QVERIFY(run->isEnabled());
    QVERIFY(run->isArmed());
    surfaces("View + Menu held: the role stays");
    QVERIFY2(m_app->padRoleChip()->text().contains(PadRun::roleSwitchText()),
             qPrintable(m_app->padRoleChip()->text()));
    // Switched off on the computer, armed: Tools on every surface.
    btn->click();
    QVERIFY(!run->isEnabled());
    surfaces("the role off from the Run tab");
    // In Tools the hold is said on the chip too, and switches nothing.
    value(kView, 255);
    value(kMenu, 255);
    QTRY_COMPARE(run->notice(), PadRun::roleSwitchText());
    value(kMenu, 0);
    value(kView, 0);
    settle();
    QVERIFY(!run->isEnabled());
    QVERIFY2(m_app->padRoleChip()->isVisible()
                 && m_app->padRoleChip()->text() == QString("Pad: Tools \u00B7 ") + PadRun::roleSwitchText(),
             qPrintable(m_app->padRoleChip()->text()));
    btn->click();
    QVERIFY(run->isEnabled());

    // The pad lost while armed, and back (it comes back in its own colour).
    chord(true);
    iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
    emit iom->inputDeviceDisconnected(0, m_stub->name(), QStringLiteral("Stub pad"));
    m_stub->m_feedbackParams.clear();
    surfaces("the pad lost while armed");
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    emit iom->inputDeviceConnected(0, m_stub->name(), QStringLiteral("Stub pad"));
    settle(100);
    surfaces("the pad back");
    // And it arms again with the chord.
    value(kLB, 255);
    value(kRB, 255);
    QTest::qWait(300);
    QTRY_VERIFY_WITH_TIMEOUT(!run->isArmChordPending(), 5000);   // and its timer fired (slow CI)
    const QString diag = QString("after the pad came back: armed %1, state %2, LB role %3, holder '%4', testing %5")
                             .arg(run->isArmed()).arg(int(run->state())).arg(pc->controllerRole(0, kLB))
                             .arg(run->holder()).arg(run->isTesting());
    qInfo().noquote() << diag;
    value(kRB, 0);
    value(kLB, 0);
    QVERIFY2(run->isArmed(), qPrintable(diag));
    surfaces("armed again after the pad came back");

    /* Once more in the order an earlier run of this test failed: the stop
       while walking, the walk ended, armed, the pad lost and back -- with
       no role switch in between -- then the chord, with what it saw. */
    chord(false);
    tap(kMenu);
    QVERIFY(run->isWalking());
    value(kLB, 255);
    value(kRB, 255);
    value(kLT, 255);
    value(kRT, 255);
    value(kRT, 0);
    value(kLT, 0);
    value(kRB, 0);
    value(kLB, 0);
    if (run->isWalking())
        pc->refineSession()->stop();
    chord(true);
    iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
    emit iom->inputDeviceDisconnected(0, m_stub->name(), QStringLiteral("Stub pad"));
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    emit iom->inputDeviceConnected(0, m_stub->name(), QStringLiteral("Stub pad"));
    settle(100);
    value(kLB, 255);
    value(kRB, 255);
    QTest::qWait(300);
    QTRY_VERIFY_WITH_TIMEOUT(!run->isArmChordPending(), 5000);   // and its timer fired (slow CI)
    const QString diag2 = QString("again after the pad came back: armed %1, state %2, LB role %3, holder '%4', "
                                  "testing %5, walking %6, chord pending %7, roleText %8")
                              .arg(run->isArmed()).arg(int(run->state())).arg(pc->controllerRole(0, kLB))
                              .arg(run->holder()).arg(run->isTesting()).arg(run->isWalking())
                              .arg(run->isArmChordPending()).arg(run->roleText());
    qInfo().noquote() << diag2;
    value(kRB, 0);
    value(kLB, 0);
    QVERIFY2(run->isArmed(), qPrintable(diag2));
    doc->masterTimer()->stopAllFunctions();
}

void PadRunRulesUi_Test::focusOnEverySurface()
{
    Doc *doc = m_app->doc();
    InputOutputMap *iom = doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    ProgrammerController *pc = doc->programmer();
    PadRun *run = pc->padRun();
    run->setArmChordMs(15);
    run->setEnabled(true, false);
    quint32 focus = 0, house = 0;
    const quint32 list = makeFocusShow(&focus, &house);
    FocusCallCue *fcc = pc->focusCallCue();
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QList<ShowControl::Request> runs;
    ShowControl *sc = doc->showControl();
    sc->setRunHandler([&runs](const ShowControl::Request &r, ShowControl::Route) {
        runs << r;
        return true;
    });
    auto unhook = qScopeGuard([sc]() { sc->setRunHandler(ShowControl::RunHandler()); });

    QToolButton *btn = m_app->findChild<QToolButton *>(QStringLiteral("crPadRun"));
    QVERIFY(btn != nullptr);
    QWidget *w = btn;
    while (w != nullptr && qobject_cast<CueRunner *>(w) == nullptr)
        w = w->parentWidget();
    CueRunner *runTab = qobject_cast<CueRunner *>(w);
    QVERIFY(runTab != nullptr);
    QLabel *chip = m_app->padRoleChip();
    QVERIFY(chip != nullptr);
    auto value = [&](quint32 ch, uchar v) { emit iom->inputValueChanged(0, ch, v, QString()); };
    auto ltMenu = [&]() {
        value(kLT, 255);
        value(kMenu, 255);
        value(kMenu, 0);
        value(kLT, 0);
    };
    // One state on every surface: words, colour, frame, chip, light.
    auto surfaces = [&](PadRun::State want, const char *when) {
        settle();
        QVERIFY2(run->state() == want, qPrintable(QString("%1: %2").arg(when, run->roleText())));
        const QString words = run->roleText();
        const QColor colour = run->stateColour();
        QVERIFY2(colour.isValid(), when);
        const QString name = colour.name();
        QVERIFY2(btn->text() == words, qPrintable(QString("%1: button '%2'").arg(when, btn->text())));
        QVERIFY2(chip->isVisible() && chip->text().startsWith(words),
                 qPrintable(QString("%1: chip '%2'").arg(when, chip->text())));
        QVERIFY2(btn->styleSheet().contains(name), qPrintable(QString("%1: button '%2'").arg(when, btn->styleSheet())));
        QVERIFY2(frameIs(runTab, colour), qPrintable(QString("%1: frame, drawn %2, state %3")
                 .arg(when, padFrameDrawn(runTab).name(), name)));
        QVERIFY2(chip->styleSheet().contains(name), qPrintable(QString("%1: chip '%2'").arg(when, chip->styleSheet())));
        QVERIFY2(pc->padLight() == colour, qPrintable(QString("%1: light %2, state %3")
                 .arg(when, pc->padLight().name(), name)));
        const QString sent = lastLight(m_stub);
        QVERIFY2(sent == name, qPrintable(QString("%1: the pad was last sent '%2', the state is '%3'").arg(when, sent, name)));
    };

    value(kLB, 255);
    value(kRB, 255);
    QTRY_VERIFY(run->isArmed());
    value(kRB, 0);
    value(kLB, 0);
    surfaces(PadRun::Armed, "armed, the focus cue waiting");
    QVERIFY2(runTab->focusArmedText().contains("armed, hold LT and press Menu"), qPrintable(runTab->focusArmedText()));
    ltMenu();
    QTRY_COMPARE(fcc->state(), FocusCallCue::Running);
    surfaces(PadRun::Focus, "FOCUS");
    QCOMPARE(btn->text(), QString("Pad: FOCUS"));
    QCOMPARE(run->stateColour(), PadRun::focusColour());
    QVERIFY(runTab->focusArmedText().contains("LT + Menu ends it"));
    // The focus's own work changes nothing shown (a save colour on the lamp is not the state).
    value(kRT, 255);
    value(kRT, 0);
    surfaces(PadRun::Focus, "FOCUS, a call stepped");
    // LT + Menu: ARMED, the list waits on the cue.
    ltMenu();
    QTRY_VERIFY(!pc->focusSession()->isActive());
    surfaces(PadRun::Armed, "armed after the focus");
    QVERIFY(runs.isEmpty());
    QCOMPARE(runTab->focusArmedText(), QString("Venue focus done: LT + A for the next cue"));
    // Again, and the emergency stop in FOCUS: red everywhere.
    m_running = house;
    fcc->cueCurrent(list, house);
    m_running = focus;
    fcc->cueCurrent(list, focus);
    ltMenu();
    QTRY_COMPARE(fcc->state(), FocusCallCue::Running);
    surfaces(PadRun::Focus, "FOCUS again");
    value(kLB, 255);
    value(kRB, 255);
    value(kLT, 255);
    value(kRT, 255);
    surfaces(PadRun::Stopped, "the emergency stop in FOCUS");
    QCOMPARE(runs.size(), 1);
    QCOMPARE(runs.at(0).command, ShowControl::AllOff);
    value(kRT, 0);
    value(kLT, 0);
    value(kRB, 0);
    value(kLB, 0);
    ltMenu();
    QTRY_VERIFY(!pc->focusSession()->isActive());
    QVERIFY(!run->isArmed());
    fcc->setRunningCueProbe({});
    run->setEnabled(false, false);
    doc->masterTimer()->stopAllFunctions();
    doc->setMode(Doc::Design);
    settle(50);
}

void PadRunRulesUi_Test::twoPadsOnEverySurface()
{
    Doc *doc = m_app->doc();
    InputOutputMap *iom = doc->inputOutputMap();
    ProgrammerController *pc = doc->programmer();
    PadRun *run = pc->padRun();
    ControllerJobs *jobs = pc->controllerJobs();
    m_stub->m_lineNames = QStringList() << "Pad A (AA:BB:CC:00:00:01)" << "Pad B (AA:BB:CC:00:00:02)";
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 1));
    auto restore = qScopeGuard([&]() {
        run->setEnabled(false, false);
        jobs->resetAll();
        iom->setInputPatch(1, QString(), QString(), QLCIOPlugin::invalidLine());
        m_stub->m_lineNames.clear();
        settle(20);
    });
    const QString padB = jobs->identityOfUniverse(1);
    jobs->setJobs(padB, ControllerJobs::FollowSpot | ControllerJobs::Focus);
    run->setEnabled(true, false);
    run->setArmChordMs(15);
    QToolButton *btn = m_app->findChild<QToolButton *>(QStringLiteral("crPadRun"));
    QLabel *chip = m_app->padRoleChip();
    QVERIFY(btn != nullptr && chip != nullptr);
    auto value = [&](quint32 u, quint32 ch, uchar v) { emit iom->inputValueChanged(u, ch, v, QString()); };
    auto light = [&](quint32 line) {
        const int offset = m_stub->m_feedbackLines.size() - m_stub->m_feedbackParams.size();
        for (int i = m_stub->m_feedbackParams.size() - 1; i >= 0; --i)
        {
            const QVariantMap p = m_stub->m_feedbackParams.at(i).toMap();
            if (p.value("gamepad").toString() == QLatin1String("light")
                && m_stub->m_feedbackLines.value(offset + i) == line)
                return p.value("rgb").toString();
        }
        return QStringLiteral("none");
    };

    // A arms: the one state names it on the button and the chip.
    value(0, kLB, 255);
    value(0, kRB, 255);
    QTRY_VERIFY(run->isArmed());
    value(0, kRB, 0);
    value(0, kLB, 0);
    settle();
    const QString armed = QString::fromUtf8("Pad: ARMED \u00B7 Pad A (\u202600:01)");
    QCOMPARE(run->roleText(), armed);
    QCOMPARE(btn->text(), armed);
    QVERIFY2(chip->isVisible() && chip->text().startsWith(armed), qPrintable(chip->text()));
    QCOMPARE(light(0), PadRun::armedColour().name());
    QCOMPARE(light(1), QString());

    // B, no Run cues: its chord says so on the chip; A as it was.
    value(1, kLB, 255);
    value(1, kRB, 255);
    QTest::qWait(60);
    QTRY_VERIFY_WITH_TIMEOUT(!run->isArmChordPending(), 5000);   // and its timer fired (slow CI)
    value(1, kRB, 0);
    value(1, kLB, 0);
    settle();
    QVERIFY2(chip->text().contains("This controller doesn't run cues."), qPrintable(chip->text()));
    QCOMPARE(run->armedUniverse(), quint32(0));
    // B given Run cues: told who runs them.
    jobs->setJob(padB, ControllerJobs::RunCues, true);
    value(1, kLB, 255);
    value(1, kRB, 255);
    QTest::qWait(60);
    QTRY_VERIFY_WITH_TIMEOUT(!run->isArmChordPending(), 5000);   // and its timer fired (slow CI)
    value(1, kRB, 0);
    value(1, kLB, 0);
    settle();
    // The chip names the runner once; the full words are the notice (and said).
    QVERIFY2(chip->text().startsWith(armed + QString::fromUtf8(" \u00B7 another controller is running cues")),
             qPrintable(chip->text()));
    QCOMPARE(chip->text().count(QString::fromUtf8("Pad A (\u202600:01)")), 1);
    QCOMPARE(run->notice(), QString::fromUtf8("Pad A (\u202600:01) is running cues."));
    QCOMPARE(run->armedUniverse(), quint32(0));
    /* QLC_SHOT_DIR: the footer as it is now (A runs cues, B told so) at
       three sizes, for the UX review; and once the notice is gone. */
    const QString shotDir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shotDir.isEmpty())
    {
        QDir().mkpath(shotDir);
        const QSize was = m_app->size();
        for (const QString &when : { QStringLiteral("refused"), QStringLiteral("armed") })
        {
            if (when == QLatin1String("armed"))
                QTRY_VERIFY_WITH_TIMEOUT(run->notice().isEmpty(), PadRun::noticeMs() + 2000);
            for (const QSize &z : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
            {
                m_app->resize(z);
                settle(150);
                const QPixmap full = m_app->grab();
                const int h = 44 * int(full.devicePixelRatio());
                full.copy(0, full.height() - h, full.width(), h)
                    .save(QDir(shotDir).filePath(QStringLiteral("after-footer-two-pads-%1-%2x%3.png")
                                                 .arg(when).arg(z.width()).arg(z.height())));
            }
        }
        m_app->resize(was);
        settle(50);
    }

    // The stop from B: red everywhere, on both pads.
    for (quint32 ch : { kLB, kRB, kLT, kRT })
        value(1, ch, 255);
    settle();
    QCOMPARE(run->state(), PadRun::Stopped);
    QVERIFY(chip->text().startsWith(run->roleText()));
    QCOMPARE(light(0), PadRun::stoppedColour().name());
    QCOMPARE(light(1), PadRun::stoppedColour().name());
    for (quint32 ch : { kRT, kLT, kRB, kLB })
        value(1, ch, 0);
}

void PadRunRulesUi_Test::focusBesideTheRunnerOnEverySurface()
{
    Doc *doc = m_app->doc();
    InputOutputMap *iom = doc->inputOutputMap();
    ProgrammerController *pc = doc->programmer();
    PadRun *run = pc->padRun();
    ControllerJobs *jobs = pc->controllerJobs();
    FocusCallCue *fcc = pc->focusCallCue();
    m_stub->m_lineNames = QStringList() << "Pad A (AA:BB:CC:00:00:01)" << "Pad B (AA:BB:CC:00:00:02)";
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 1));
    QList<ShowControl::Request> runs;
    ShowControl *sc = doc->showControl();
    sc->setRunHandler([&runs](const ShowControl::Request &r, ShowControl::Route) {
        runs << r;
        return true;
    });
    auto restore = qScopeGuard([&]() {
        sc->setRunHandler(ShowControl::RunHandler());
        if (pc->focusSession()->isActive())
            pc->focusSession()->finish();
        fcc->setRunningCueProbe({});
        run->setEnabled(false, false);
        jobs->resetAll();
        iom->setInputPatch(1, QString(), QString(), QLCIOPlugin::invalidLine());
        m_stub->m_lineNames.clear();
        doc->masterTimer()->stopAllFunctions();
        doc->setMode(Doc::Design);
        settle(50);
    });
    jobs->setJobs(jobs->identityOfUniverse(0), ControllerJobs::RunCues);
    jobs->setJobs(jobs->identityOfUniverse(1), ControllerJobs::Focus);
    run->setArmChordMs(15);
    run->setEnabled(true, false);
    quint32 focus = 0, house = 0;
    const quint32 list = makeFocusShow(&focus, &house);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    // B ends the focus with the panel's Leave, as the laptop does.
    QMetaObject::Connection leave = connect(pc->focusSession(), &FocusSession::exitRequested,
                                            pc->focusSession(), &FocusSession::finish);
    auto unleave = qScopeGuard([leave]() { QObject::disconnect(leave); });

    QToolButton *btn = m_app->findChild<QToolButton *>(QStringLiteral("crPadRun"));
    QVERIFY(btn != nullptr);
    QWidget *w = btn;
    while (w != nullptr && qobject_cast<CueRunner *>(w) == nullptr)
        w = w->parentWidget();
    CueRunner *runTab = qobject_cast<CueRunner *>(w);
    QVERIFY(runTab != nullptr);
    QLabel *chip = m_app->padRoleChip();
    QVERIFY(chip != nullptr);
    auto value = [&](quint32 u, quint32 ch, uchar v) { emit iom->inputValueChanged(u, ch, v, QString()); };
    auto with = [&](quint32 u, quint32 ch) {
        value(u, kLT, 255);
        value(u, ch, 255);
        value(u, ch, 0);
        value(u, kLT, 0);
    };
    auto light = [&](quint32 line) {
        const int offset = m_stub->m_feedbackLines.size() - m_stub->m_feedbackParams.size();
        for (int i = m_stub->m_feedbackParams.size() - 1; i >= 0; --i)
        {
            const QVariantMap p = m_stub->m_feedbackParams.at(i).toMap();
            if (p.value("gamepad").toString() == QLatin1String("light")
                && m_stub->m_feedbackLines.value(offset + i) == line)
                return p.value("rgb").toString();
        }
        return QStringLiteral("none");
    };

    // A arms.
    value(0, kLB, 255);
    value(0, kRB, 255);
    QTRY_VERIFY(run->isArmed());
    value(0, kRB, 0);
    value(0, kLB, 0);
    settle();
    // The Run tab says how B starts it: LT + Menu, no arm.
    QVERIFY2(runTab->focusArmedText().contains("hold LT and press Menu")
                 && !runTab->focusArmedText().contains("armed"),
             qPrintable(runTab->focusArmedText()));
    // A job changed: the Run tab follows at once, as the footer does (review 7).
    jobs->setJob(jobs->identityOfUniverse(1), ControllerJobs::Focus, false);
    settle();
    QVERIFY2(runTab->focusArmedText().contains("no controller does focus"), qPrintable(runTab->focusArmedText()));
    jobs->setJob(jobs->identityOfUniverse(1), ControllerJobs::Focus, true);
    settle();
    QVERIFY2(runTab->focusArmedText().contains("hold LT and press Menu"), qPrintable(runTab->focusArmedText()));

    // B: LT + Menu. FOCUS names B on every surface; A stays armed.
    with(1, kMenu);
    QTRY_COMPARE(fcc->state(), FocusCallCue::Running);
    settle();
    const QString focusText = QString::fromUtf8("Pad: FOCUS · Pad B (…00:02)");
    QCOMPARE(run->roleText(), focusText);
    QCOMPARE(btn->text(), focusText);
    QVERIFY2(chip->isVisible() && chip->text().startsWith(focusText), qPrintable(chip->text()));
    QVERIFY2(chip->styleSheet().contains(PadRun::focusColour().name()), qPrintable(chip->styleSheet()));
    QCOMPARE(light(1), PadRun::focusColour().name());
    QCOMPARE(light(0), QString());
    QVERIFY(run->isArmed());
    QCOMPARE(run->armedUniverse(), quint32(0));

    // A's GO: nothing plays, and the chip says why (the footer names B once).
    with(0, kA);
    settle();
    QVERIFY(runs.isEmpty());
    QCOMPARE(run->notice(), QString::fromUtf8("Venue focus is running on Pad B (…00:02): nothing plays until it ends."));
    QVERIFY2(chip->text().startsWith(focusText + QString::fromUtf8(" · nothing plays until the venue focus ends")),
             qPrintable(chip->text()));
    QCOMPARE(chip->text().count(QString::fromUtf8("Pad B (…00:02)")), 1);

    const QString shotDir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shotDir.isEmpty())
    {
        QDir().mkpath(shotDir);
        const QSize was = m_app->size();
        m_app->showContext(QStringLiteral("Run"));
        for (const QSize &z : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
        {
            m_app->resize(z);
            with(0, kA);        // the refusal, fresh on the chip for the shot
            settle(150);
            const QString wh = QString("%1x%2").arg(z.width()).arg(z.height());
            const QPixmap full = m_app->grab();
            full.save(QDir(shotDir).filePath(QStringLiteral("after-focus-beside-runner-%1.png").arg(wh)));
            const int h = 44 * int(full.devicePixelRatio());
            full.copy(0, full.height() - h, full.width(), h)
                .save(QDir(shotDir).filePath(QStringLiteral("after-footer-focus-beside-runner-%1.png").arg(wh)));
        }
        m_app->resize(was);
        settle(50);
    }

    // B ends it: ARMED names A, A's light shows it, A's GO plays.
    with(1, kMenu);
    QTRY_VERIFY(!pc->focusSession()->isActive());
    settle();
    const QString armedText = QString::fromUtf8("Pad: ARMED · Pad A (…00:01)");
    QCOMPARE(run->roleText(), armedText);
    QCOMPARE(btn->text(), armedText);
    QVERIFY2(chip->text().startsWith(armedText), qPrintable(chip->text()));
    QCOMPARE(light(0), PadRun::armedColour().name());
    with(0, kA);
    QCOMPARE(runs.size(), 1);
    QCOMPARE(runs.at(0).command, ShowControl::Go);
    Q_UNUSED(list);
}

quint32 PadRunRulesUi_Test::makeFocusShow(quint32 *focus, quint32 *house)
{
    Doc *doc = m_app->doc();
    ProgrammerController *pc = doc->programmer();
    // Made once (the App and its show live across the test functions).
    if (m_focusList != 0)
    {
        *focus = m_focusCue;
        *house = m_houseCue;
        m_running = *focus;
        pc->focusCallCue()->setRunningCueProbe([this](quint32) { return m_running; });
        doc->setMode(Doc::Operate);
        settle(50);
        pc->focusCallCue()->cueCurrent(m_focusList, *house);
        pc->focusCallCue()->cueCurrent(m_focusList, *focus);
        return m_focusList;
    }
    MonitorProperties *props = doc->monitorProperties();
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName(QStringLiteral("Focus Spots"));
    g->setSize(QSize(2, 1));
    for (int i = 0; i < 2; ++i)
    {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test");
        def->setModel(QString("Focus Spot %1").arg(i));
        def->setType(QLCFixtureDef::MovingHead);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("3ch");
        QLCFixtureHead head;
        const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
        for (int k = 0; k < 3; ++k)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("Ch %1").arg(k));
            ch->setGroup(groups[k]);
            ch->setControlByte(QLCChannel::MSB);
            def->addChannel(ch);
            mode->insertChannel(ch, k);
            head.addChannel(k);
        }
        QLCPhysical ph;
        ph.setFocusPanMax(540);
        ph.setFocusTiltMax(270);
        mode->setPhysical(ph);
        mode->insertHead(-1, head);
        def->addMode(mode);
        Fixture *fxi = new Fixture(doc);
        fxi->setName(QString("Focus Spot %1").arg(i + 1));
        fxi->setFixtureDefinition(def, mode);
        fxi->setUniverse(0);
        fxi->setAddress(quint32(100 + i * 3));
        doc->addFixture(fxi);
        props->setFixturePosition(fxi->id(), 0, 0, QVector3D(2000.0f * (i + 1), 2000.0f, 5.0f));
        g->assignFixture(fxi->id(), QLCPoint(i, 0));
    }
    doc->addFixtureGroup(g);
    StageTarget *dsc = props->addStageTarget();
    dsc->setName(QStringLiteral("DSC"));
    dsc->setPosition(QVector3D(5.0f, 6.0f, 0.0f));
    FocusCall c;
    c.groups << g->id();
    c.target.kind = FocusSession::Reference::Target;
    c.target.id = dsc->id();
    pc->focusCalls()->setCalls({ c });
    ShowTreeController *st = doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Pre-show"), "Pre-show");
    *focus = st->addCue(list, -1, "Focus");
    *house = st->addCue(list, 0, "House");
    st->setCuesFocusCall({ *focus }, true);
    m_running = *focus;
    pc->focusCallCue()->setRunningCueProbe([this](quint32) { return m_running; });
    doc->setMode(Doc::Operate);
    settle(50);
    pc->focusCallCue()->cueCurrent(list, *focus);
    m_focusList = list;
    m_focusCue = *focus;
    m_houseCue = *house;
    return list;
}

void PadRunRulesUi_Test::venueFocusShots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    // "after" on this branch; the base's shots for the review are "before".
    const QString phase = QStringLiteral("after");
    Doc *doc = m_app->doc();
    InputOutputMap *iom = doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    ProgrammerController *pc = doc->programmer();
    PadRun *run = pc->padRun();
    run->setArmChordMs(15);
    run->setEnabled(true, false);
    quint32 focus = 0, house = 0;
    const quint32 list = makeFocusShow(&focus, &house);
    FocusCallCue *fcc = pc->focusCallCue();
    // The Run tab plays the focus cue's list, as on a pre-show night.
    doc->controlMap()->setRunTabList(list);
    // What an earlier test's pad left on the chip (a notice) has gone.
    QTest::qWait(PadRun::noticeMs() + 200);
    auto value = [&](quint32 ch, uchar v) { emit iom->inputValueChanged(0, ch, v, QString()); };
    auto ltMenu = [&]() {
        value(kLT, 255);
        value(kMenu, 255);
        value(kMenu, 0);
        value(kLT, 0);
    };
    for (const QSize &size : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
    {
        const QString wh = QString("%1x%2").arg(size.width()).arg(size.height());
        m_app->resize(size);
        m_app->showContext(QStringLiteral("Run"));
        // Armed, the focus cue waiting.
        if (!run->isArmed())
        {
            value(kLB, 255);
            value(kRB, 255);
            QTRY_VERIFY(run->isArmed());
            value(kRB, 0);
            value(kLB, 0);
        }
        m_running = house;
        fcc->cueCurrent(list, house);
        m_running = focus;
        fcc->cueCurrent(list, focus);
        QCOMPARE(fcc->state(), FocusCallCue::Armed);
        settle(100);
        m_app->grab().save(QString("%1/%2-runtab-focuswaiting-%3.png").arg(dir, phase, wh));
        // LT + Menu: the calls.
        ltMenu();
        QTRY_COMPARE(fcc->state(), FocusCallCue::Running);
        m_app->showContext(QStringLiteral("Run"));
        m_app->raise();
        m_app->activateWindow();
        settle(200);
        m_app->grab().save(QString("%1/%2-runtab-focus-%3.png").arg(dir, phase, wh));
        // LT + Menu again: finished.
        ltMenu();
        QTRY_VERIFY(!pc->focusSession()->isActive());
        settle(200);
        m_app->grab().save(QString("%1/%2-runtab-focusdone-%3.png").arg(dir, phase, wh));
        doc->masterTimer()->stopAllFunctions();
        settle(50);
    }
    fcc->setRunningCueProbe({});
    doc->controlMap()->setRunTabList(0xFFFFFFFF);
    run->setEnabled(false, false);
    doc->setMode(Doc::Design);
    settle(50);
}

void PadRunRulesUi_Test::aChangedDeviceDisarmsOnTheChip()
{
    /* Another device behind the armed pad's patch (re-enumerated where it
       was, another pad shifted onto its line): the pad lost -- the footer's
       chip, as drawn, and the Run tab's button say it is not armed and why
       (board Q253's fourth review). */
    Doc *doc = m_app->doc();
    InputOutputMap *iom = doc->inputOutputMap();
    m_stub->m_deviceIds = { { 0, 1 } };
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    PadRun *run = doc->programmer()->padRun();
    auto restore = qScopeGuard([&]() {
        m_stub->m_deviceIds.clear();
        run->setEnabled(false, false);
        iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
        settle(50);
    });
    run->setEnabled(true, false);
    run->setArmChordMs(15);
    auto value = [&](quint32 ch, uchar v) { emit iom->inputValueChanged(0, ch, v, QString()); };
    value(kLB, 255);
    value(kRB, 255);
    QTRY_VERIFY(run->isArmed());
    value(kRB, 0);
    value(kLB, 0);
    value(kLT, 255);
    QLabel *chip = m_app->padRoleChip();
    QToolButton *btn = m_app->findChild<QToolButton *>(QStringLiteral("crPadRun"));
    QVERIFY(btn != nullptr);
    QTRY_VERIFY2_WITH_TIMEOUT(chip->isVisible() && chip->text().startsWith(QStringLiteral("Pad: ARMED")),
                              qPrintable(chip->text()), 2000);
    m_stub->m_deviceIds[0] = 2;
    emit m_stub->configurationChanged();
    settle(50);
    QTRY_VERIFY2_WITH_TIMEOUT(chip->text() == QStringLiteral("Pad: Run · Disarmed: the controller went away."),
                              qPrintable(chip->text()), 2000);
    QCOMPARE(btn->text(), QString("Pad: Run"));
    QVERIFY2(!chip->styleSheet().contains(PadRun::armedColour().name()), qPrintable(chip->styleSheet()));
    value(kLT, 0);
}

void PadRunRulesUi_Test::q253TheFollowChipSaysWhyItHolds()
{
    Doc *doc = m_app->doc();
    ProgrammerController *pc = doc->programmer();
    // Three lights: two the follow spot plays, one it does not.
    QList<quint32> fids;
    for (int i = 0; i < 3; ++i)
    {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test");
        def->setModel(QString("Q253 Spot %1").arg(i));
        def->setType(QLCFixtureDef::MovingHead);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("3ch");
        QLCFixtureHead head;
        const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
        for (int k = 0; k < 3; ++k)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("Ch %1").arg(k));
            ch->setGroup(groups[k]);
            ch->setControlByte(QLCChannel::MSB);
            def->addChannel(ch);
            mode->insertChannel(ch, k);
            head.addChannel(k);
        }
        mode->insertHead(-1, head);
        def->addMode(mode);
        Fixture *fxi = new Fixture(doc);
        fxi->setName(QString("Q253 Spot %1").arg(i + 1));
        fxi->setFixtureDefinition(def, mode);
        fxi->setUniverse(0);
        fxi->setAddress(quint32(200 + i * 3));
        doc->addFixture(fxi);
        fids << fxi->id();
    }
    QLCPalette *follow = new QLCPalette(QLCPalette::Effect, doc);
    follow->setName("Followspot");
    follow->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/followspot.js");
    doc->addPalette(follow);
    Scene *spot = new Scene(doc);
    spot->setName("Follow");
    doc->addFunction(spot);
    spot->addFixture(fids.at(0));
    spot->addFixture(fids.at(1));
    spot->addPalette(follow->id());
    // It follows a subject; another light aimed at that subject moves with it.
    StageTarget *subject = doc->monitorProperties()->addStageTarget();
    subject->setName("Soloist");
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("On Soloist");
    aim->setStageTargetId(subject->id());
    doc->addPalette(aim);
    spot->addPalette(aim->id());
    Scene *aimed = new Scene(doc);
    aimed->setName("Another light on the subject");
    doc->addFunction(aimed);
    aimed->addFixture(fids.at(2));
    aimed->addPalette(aim->id());
    Scene *mine = new Scene(doc);
    mine->setName("On the spot's lights");
    doc->addFunction(mine);
    mine->addFixture(fids.at(1));
    Scene *others = new Scene(doc);
    others->setName("On another light");
    doc->addFunction(others);
    others->addFixture(fids.at(2));
    const quint32 wasFocused = pc->m_focusedSceneId;
    pc->m_focusedSceneId = spot->id();      // the Workbench's follow spot
    RefineSession *r = pc->refineSession();
    r->setFlashEnabled(false);
    auto restore = qScopeGuard([&]() {
        r->stop();
        pc->m_focusedSceneId = wasFocused;
        doc->deleteFunction(spot->id());
        doc->deleteFunction(mine->id());
        doc->deleteFunction(others->id());
        doc->deleteFunction(aimed->id());
        doc->deletePalette(follow->id());
        doc->deletePalette(aim->id());
        doc->monitorProperties()->removeStageTarget(subject->id());
        for (quint32 fid : fids)
            doc->deleteFixture(fid);
        settle(50);
    });
    QLabel *chip = m_app->followChip();
    const QString held("Follow spot holds: Focus This Look is working on its lights");
    QTRY_VERIFY_WITH_TIMEOUT(chip->isVisible(), 2000);
    QVERIFY2(!chip->property("fullText").toString().contains(held), qPrintable(chip->property("fullText").toString()));

    // Focus This Look on one of its lights: it holds, and the chip says why.
    QString why;
    QVERIFY2(r->startScene({ mine->id() }, 0xFFFFFFFF, &why), qPrintable(why));
    QTRY_VERIFY2_WITH_TIMEOUT(chip->property("fullText").toString().contains(held),
                              qPrintable(chip->property("fullText").toString()), 2000);
    QVERIFY2(chip->toolTip().contains(held), qPrintable(chip->toolTip()));
    QCOMPARE(pc->followSpotStatus().heldBy, QString("Focus This Look"));
    /* What the footer draws, at three sizes: the chip's own text (the
       variant the footer fitted, "holds: Focus This Look" in its short
       form too), on screen inside the window, and drawn (its pixels).
       QLC_SHOT_DIR: saved for the UX review. */
    auto shown = [&]() {
        QString t = chip->text();
        t.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
        return t.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    };
    const QString shotDir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shotDir.isEmpty())
        QDir().mkpath(shotDir);
    const QSize was = m_app->size();
    for (const QSize &z : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
    {
        m_app->resize(z);
        settle(200);
        const QString where = QString("%1x%2: '%3'").arg(z.width()).arg(z.height()).arg(shown());
        QTRY_VERIFY2_WITH_TIMEOUT(chip->isVisible() && shown().contains(QStringLiteral("holds: Focus This Look")),
                                  qPrintable(where), 2000);
        const QPoint right = chip->mapTo(m_app, QPoint(chip->width(), chip->height()));
        QVERIFY2(chip->width() > 20 && right.x() <= m_app->width() && right.y() <= m_app->height(),
                 qPrintable(QString("%1: the chip is squeezed or off the window").arg(where)));
        const QImage drawn = chip->grab().toImage();
        QSet<QRgb> colours;
        for (int y = 0; y < drawn.height() && colours.size() < 3; ++y)
            for (int x = 0; x < drawn.width() && colours.size() < 3; ++x)
                colours.insert(drawn.pixel(x, y));
        QVERIFY2(colours.size() >= 2, qPrintable(QString("%1: the chip draws nothing").arg(where)));
        if (!shotDir.isEmpty())
        {
            const QPixmap full = m_app->grab();
            const int h = 44 * int(full.devicePixelRatio());
            full.copy(0, full.height() - h, full.width(), h)
                .save(QDir(shotDir).filePath(QStringLiteral("after-footer-follow-spot-holds-%1x%2.png")
                                             .arg(z.width()).arg(z.height())));
            m_app->grab().save(QDir(shotDir).filePath(QStringLiteral("after-window-follow-spot-holds-%1x%2.png")
                                                      .arg(z.width()).arg(z.height())));
        }
    }
    m_app->resize(was);
    settle(50);

    // On another light: no hold, nothing said of one.
    r->stop();
    QVERIFY2(r->startScene({ others->id() }, 0xFFFFFFFF, &why), qPrintable(why));
    QTRY_VERIFY2_WITH_TIMEOUT(!chip->property("fullText").toString().contains("holds")
                                  && !shown().contains(QStringLiteral("holds")),
                              qPrintable(shown()), 2000);
    QCOMPARE(pc->followSpotStatus().heldBy, QString());
    r->stop();

    // A light aimed at the follow spot's subject (not one of its own): it holds too.
    QVERIFY2(r->startScene({ aimed->id() }, 0xFFFFFFFF, &why), qPrintable(why));
    QTRY_VERIFY2_WITH_TIMEOUT(chip->isVisible() && shown().contains(QStringLiteral("holds: Focus This Look")),
                              qPrintable(shown()), 2000);
    QCOMPARE(pc->followSpotStatus().heldBy, QString("Focus This Look"));
    r->stop();
    QTRY_VERIFY2_WITH_TIMEOUT(!shown().contains(QStringLiteral("holds")), qPrintable(shown()), 2000);
}

int main(int argc, char **argv)
{
    // A home of its own: nothing touches yours (as padbenchshow).
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    PadRunRulesUi_Test test;
    return QTest::qExec(&test, argc, argv);
}

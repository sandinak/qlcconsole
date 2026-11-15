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

#include "padrunrulesui_test.h"

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
        QVERIFY2(colour.isValid() ? runTab->styleSheet().contains(name) : runTab->styleSheet().isEmpty(),
                 qPrintable(QString("%1: frame '%2'").arg(when, runTab->styleSheet())));
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

    // Switched off from the pad (View + Menu held), armed.
    chord(true);
    run->setRoleChordMs(40);
    value(kView, 255);
    value(kMenu, 255);
    QTRY_VERIFY(!run->isEnabled());
    value(kMenu, 0);
    value(kView, 0);
    surfaces("the role off from the pad");
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

quint32 PadRunRulesUi_Test::makeFocusShow(quint32 *focus, quint32 *house)
{
    Doc *doc = m_app->doc();
    ProgrammerController *pc = doc->programmer();
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
    run->setEnabled(false, false);
    doc->setMode(Doc::Design);
    settle(50);
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

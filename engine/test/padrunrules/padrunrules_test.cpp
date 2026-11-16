/*
  Q Light Controller Plus
  padrunrules_test.cpp

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

#include <QtTest>
#include <QSignalSpy>
#include <QSettings>

// MasterTimer::timerTick() is private: driven by hand here, as markplanner_test does.
#include "../common/stdprelude.h"
#define private public
#include "mastertimer.h"
#undef private

#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QTemporaryDir>

#include <algorithm>
#include <climits>

#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "controllerscheme.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "iopluginstub.h"
#include "qlcioplugin.h"
#include "showcontrol.h"
#include "padrunrules_test.h"
#include "refinesession.h"
#include "controlmap.h"
#include "qlcfile.h"
#include "inputpatch.h"
#include "gamepadrole.h"
#include "padrun.h"
#include "controllerjobs.h"
#include "chaser.h"
#include "doc.h"
#include "showtreemodel.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "venuecorrections.h"
#include "padtriggers.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "focuscallcue.h"
#include "focuscalls.h"
#include "focussession.h"
#include "fixturegroup.h"
#include "qlcpoint.h"
#include "function.h"
#include "functionparent.h"

namespace
{
/* An Xbox pad as its profile describes it: axes, then buttons. Channel 50
   is a button with no gamepad role (a DualSense's mute, an Edge paddle). */
const quint32 kLT = 4, kRT = 5, kA = 10, kB = 11, kX = 12, kY = 13, kLB = 14, kRB = 15,
              kView = 16, kMenu = 17, kLS = 18, kRS = 19, kUp = 20, kUnroled = 50;

QMap<quint32, QString> xboxRoles()
{
    return { { 0, "lx" }, { 1, "ly" }, { 2, "rx" }, { 3, "ry" }, { kLT, "lt" }, { kRT, "rt" },
             { kA, "a" }, { kB, "b" }, { kX, "x" }, { kY, "y" }, { kLB, "lb" }, { kRB, "rb" },
             { kView, "back" }, { kMenu, "start" }, { kLS, "ls" }, { kRS, "rs" }, { kUp, "dpad_up" } };
}

Fixture *makeSpot(Doc *doc, const QString &name)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Rules Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
    const char *names[] = { "Pan", "Tilt", "Dimmer" };
    QLCFixtureHead head;
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
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
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(quint32(doc->fixtures().size() * 3));
    doc->addFixture(fxi);
    return fxi;
}

/* The last "light" the pad was sent ("": its own colour; "none": never). */
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

bool isGoFamily(ShowControl::Command c)
{
    return c == ShowControl::Go || c == ShowControl::Back || c == ShowControl::SubGo
           || c == ShowControl::SubBack;
}
}

/*****************************************************************************
 * The bench
 *****************************************************************************/

void PadRunRules_Test::init()
{
    QSettings().remove(ControllerJobs::settingsGroup());    // every pad every job
    m_doc = new Doc(this);
    m_runs.clear();
    m_said.clear();
    m_doc->showControl()->setRunHandler([this](const ShowControl::Request &r, ShowControl::Route route) {
        m_runs << qMakePair(r, route);
        return true;
    });
    connect(m_doc->programmer(), &ProgrammerController::operatorAnnounce, this,
            [this](const QString &text) { m_said << text; });
    m_stub = loadStub();
    QVERIFY2(m_stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, m_stub->name(), QString(), 0));
}

void PadRunRules_Test::cleanup()
{
    m_doc->showControl()->setRunHandler(ShowControl::RunHandler());
    if (m_stub != nullptr)
    {
        m_stub->m_linesAreHardware = false;
        m_stub->m_lineNames.clear();
        m_stub->m_lineChannelRoles.clear();
        m_stub->m_lineDeviceKind.clear();
        m_stub->m_buttonActions.clear();
        m_stub->m_feedbackParams.clear();
        m_stub->m_feedbackLines.clear();
        m_stub->m_lineLocations.clear();
    }
    QSettings().remove(ControllerJobs::settingsGroup());
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
}

IOPluginStub *PadRunRules_Test::loadStub()
{
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    if (m_doc->ioPluginCache()->plugins().isEmpty())
        return nullptr;
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = xboxRoles();
    stub->m_linesAreHardware = false;
    stub->m_lineNames.clear();
    stub->m_lineChannelRoles.clear();
    stub->m_lineDeviceKind.clear();
    stub->m_buttonActions.clear();
    stub->m_feedbackParams.clear();
    return stub;
}

void PadRunRules_Test::makeShow()
{
    QList<quint32> spots;
    for (int i = 0; i < 2; ++i)
        spots << makeSpot(m_doc, QString("Spot %1").arg(i + 1))->id();
    m_spots = spots;
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer);
    full->setName("Full");
    full->setValue(255);
    m_doc->addPalette(full);
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Spring");
    m_list = st->addCueList(prod, "Main", ShowTree::ListMode::NonTracking);
    m_cue1 = st->addCue(m_list, -1, "Opening");
    m_cue2 = st->addCue(m_list, 0, "Ballad");
    ShowTree::Target t;
    t.fixtures = spots;
    for (quint32 cue : { m_cue1, m_cue2 })
    {
        const quint32 layer = st->addLayer(cue, t, "Spots");
        ShowTree::Look l;
        l.key = ShowTree::KeyDimmer;
        l.palettes = { full->id() };
        QVERIFY(st->setLayerLook(cue, layer, l));
    }
    m_doc->programmer()->refineSession()->setFlashEnabled(false);
    st->pickCue(m_cue1);
}

void PadRunRules_Test::makeFocusCue()
{
    ProgrammerController *pc = m_doc->programmer();
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Spots");
    g->setSize(QSize(int(m_spots.size()), 1));
    for (int i = 0; i < m_spots.size(); ++i)
    {
        props->setFixturePosition(m_spots.at(i), 0, 0, QVector3D(2000.0f * (i + 1), 2000.0f, 5.0f));
        g->assignFixture(m_spots.at(i), QLCPoint(i, 0));
    }
    m_doc->addFixtureGroup(g);
    StageTarget *dsc = props->addStageTarget();
    dsc->setName("DSC");
    dsc->setPosition(QVector3D(5.0f, 6.0f, 0.0f));
    FocusCall c;
    c.groups << g->id();
    c.target.kind = FocusSession::Reference::Target;
    c.target.id = dsc->id();
    pc->focusCalls()->setCalls({ c });
    QVERIFY(m_doc->showTree()->setCuesFocusCall({ m_cue1 }, true));
    m_running = m_cue1;
    pc->focusCallCue()->setRunningCueProbe([this](quint32) { return m_running; });
    // The panel's part: Leave asks about unsaved work, then finishes.
    connect(pc->focusSession(), &FocusSession::exitRequested, pc->focusSession(), &FocusSession::finish);
    m_doc->setMode(Doc::Operate);
    pc->focusCallCue()->cueCurrent(m_list, m_cue1);
    QCOMPARE(pc->focusCallCue()->state(), FocusCallCue::Armed);
}

void PadRunRules_Test::focusCueAgain()
{
    FocusCallCue *fcc = m_doc->programmer()->focusCallCue();
    m_running = m_cue2;
    fcc->cueCurrent(m_list, m_cue2);
    m_running = m_cue1;
    fcc->cueCurrent(m_list, m_cue1);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
}

void PadRunRules_Test::ltMenu(quint32 universe)
{
    withLt(kMenu, universe);
}

void PadRunRules_Test::withLt(quint32 channel, quint32 universe)
{
    axis(kLT, 255, universe);
    press(channel, universe);
    axis(kLT, 0, universe);
}

void PadRunRules_Test::press(quint32 channel, quint32 universe)
{
    emit m_doc->inputOutputMap()->inputValueChanged(universe, channel, 255, QString());
    emit m_doc->inputOutputMap()->inputValueChanged(universe, channel, 0, QString());
}

void PadRunRules_Test::axis(quint32 channel, uchar value, quint32 universe)
{
    emit m_doc->inputOutputMap()->inputValueChanged(universe, channel, value, QString());
}

void PadRunRules_Test::hold(quint32 channel, bool down, quint32 universe)
{
    axis(channel, down ? 255 : 0, universe);
}

void PadRunRules_Test::arm(bool on, quint32 universe)
{
    PadRun *run = m_doc->programmer()->padRun();
    if (run->isArmed() == on)
        return;
    run->setArmChordMs(15);
    hold(kLB, true, universe);
    hold(kRB, true, universe);
    QTRY_COMPARE(run->isArmed(), on);
    hold(kRB, false, universe);
    hold(kLB, false, universe);
    QCOMPARE(run->isArmed(), on);
}

void PadRunRules_Test::drop(quint32 universe)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    iom->setInputPatch(universe, QString(), QString(), QLCIOPlugin::invalidLine());
    emit iom->inputDeviceDisconnected(universe, m_stub->name(), QString("Stub pad"));
}

void PadRunRules_Test::back(quint32 universe)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    iom->setInputPatch(universe, m_stub->name(), QString(), universe);
    emit iom->inputDeviceConnected(universe, m_stub->name(), QString("Stub pad"));
}

int PadRunRules_Test::goes() const
{
    int n = 0;
    for (const auto &r : m_runs)
        n += isGoFamily(r.first.command) ? 1 : 0;
    return n;
}

int PadRunRules_Test::stops() const
{
    int n = 0;
    for (const auto &r : m_runs)
        n += r.first.command == ShowControl::AllOff ? 1 : 0;
    return n;
}

int PadRunRules_Test::saidCount(const QString &part) const
{
    int n = 0;
    for (const QString &s : m_said)
        n += s.contains(part) ? 1 : 0;
    return n;
}

void PadRunRules_Test::twoPads(int jobsA, int jobsB)
{
    m_stub->m_lineNames = QStringList() << "Pad A (AA:BB:CC:00:00:01)" << "Pad B (AA:BB:CC:00:00:02)";
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 1));
    ControllerJobs *jobs = m_doc->programmer()->controllerJobs();
    jobs->setJobs(jobs->identityOfUniverse(0), ControllerJobs::Jobs(jobsA));
    jobs->setJobs(jobs->identityOfUniverse(1), ControllerJobs::Jobs(jobsB));
    QCOMPARE(int(jobs->jobsOf(jobs->identityOfUniverse(0))), jobsA);
    QCOMPARE(int(jobs->jobsOf(jobs->identityOfUniverse(1))), jobsB);
}

QString PadRunRules_Test::lightOn(quint32 line) const
{
    // Other tests clear the params alone: the lines are aligned from the end.
    const int offset = m_stub->m_feedbackLines.size() - m_stub->m_feedbackParams.size();
    for (int i = m_stub->m_feedbackParams.size() - 1; i >= 0; --i)
    {
        const QVariantMap p = m_stub->m_feedbackParams.at(i).toMap();
        if (p.value("gamepad").toString() == QLatin1String("light")
            && m_stub->m_feedbackLines.value(offset + i) == line)
            return p.value("rgb").toString();
    }
    return QStringLiteral("none");
}

QList<QVariantMap> PadRunRules_Test::feedbackOn(quint32 line, const QString &kind) const
{
    QList<QVariantMap> out;
    const int offset = m_stub->m_feedbackLines.size() - m_stub->m_feedbackParams.size();
    for (int i = 0; i < m_stub->m_feedbackParams.size(); ++i)
    {
        const QVariantMap p = m_stub->m_feedbackParams.at(i).toMap();
        if (p.value("gamepad").toString() == kind && m_stub->m_feedbackLines.value(offset + i) == line)
            out << p;
    }
    return out;
}

/*****************************************************************************
 * Rule 3 (and 4): the pad going away
 *****************************************************************************/

void PadRunRules_Test::aLostPadDisarmsOnTheRealPath()
{
    /* The help: "The arm drops ... the controller disconnects". Not a
       signal emitted by hand: the patch going offline as InputOutputMap
       does it when the device leaves, in the Run role with no tool. */
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    arm(true);
    QCOMPARE(pc->padLight(), PadRun::armedColour());
    drop();
    QVERIFY2(!run->isArmed(), "the pad went away and the Run tab still says ARMED");
    QCOMPARE(run->state(), PadRun::Run);
    QCOMPARE(run->roleText(), QString("Pad: Run"));
    QCOMPARE(run->notice(), QString("Disarmed: the controller went away."));
    // Back: not armed until the chord.
    back();
    QVERIFY(!run->isArmed());
    axis(kLT, 255);
    press(kA);
    axis(kLT, 0);
    QCOMPARE(goes(), 0);
}

void PadRunRules_Test::aSafetyHeldAsThePadDropsIsNoSafetyAfter()
{
    /* LT held as the pad's battery dies: its release never comes. Back
       again, A alone must not be a GO (rule 4: no cue on one control). */
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    arm(true);
    axis(kLT, 255);
    drop();
    back();
    press(kA);
    QVERIFY2(goes() == 0, "A alone was a GO: the dropped pad's safety was still held");
    // The ordinary way still works once armed again.
    arm(true);
    axis(kLT, 255);
    press(kA);
    axis(kLT, 0);
    QCOMPARE(goes(), 1);
}

void PadRunRules_Test::aSwappedPadForgetsWhatTheOldOneHeld()
{
    /* One controller slot, two pads: the armed Xbox pad dies with LT
       held and the DualSense takes the slot (the help: "a controller slot
       that finds a DualSense in place of the Xbox pad keeps it"). Its
       Cross alone must not GO, and the arm was the Xbox pad's. */
    const QString xbox = QStringLiteral("Xbox Wireless Controller");
    const QString dualSense = QStringLiteral("DualSense Wireless Controller");
    m_stub->m_linesAreHardware = true;
    m_stub->m_lineNames = QStringList() << xbox << dualSense;
    const QMap<quint32, QString> xroles = { { 6, "a" }, { 7, "b" }, { 8, "lb" }, { 9, "x" },
                                            { 10, "rb" }, { 11, "lt" } };
    const QMap<quint32, QString> droles = { { 6, "x" }, { 7, "a" }, { 8, "b" }, { 9, "lb" },
                                            { 10, "lt" }, { 11, "rb" } };
    m_stub->m_lineChannelRoles[0] = xroles;
    m_stub->m_lineChannelRoles[1] = droles;
    m_stub->m_lineDeviceKind[0] = "xbox";
    m_stub->m_lineDeviceKind[1] = "dualsense";
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    QVERIFY(iom->addInputSlotDevice(0, m_stub->name(), dualSense));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    run->setArmChordMs(15);
    InputPatch *ip = iom->inputPatch(0);
    hold(ip->showChannel(8), true);
    hold(ip->showChannel(10), true);
    QTRY_VERIFY(run->isArmed());
    hold(ip->showChannel(10), false);
    hold(ip->showChannel(8), false);
    hold(ip->showChannel(11), true);           // the Xbox pad's LT, held as it dies

    m_stub->m_lineNames = QStringList() << dualSense;
    m_stub->m_lineChannelRoles.clear();
    m_stub->m_lineChannelRoles[0] = droles;
    m_stub->m_lineDeviceKind.clear();
    m_stub->m_lineDeviceKind[0] = "dualsense";
    m_stub->configure();
    QCOMPARE(iom->inputSlotLive(0), 1);
    ip = iom->inputPatch(0);
    press(ip->showChannel(7));                 // the DualSense's Cross, no L2
    QVERIFY2(goes() == 0, "Cross alone on the new pad was a GO (the old pad's LT still held)");
    QVERIFY2(!run->isArmed(), "the armed pad went away; the new one is armed without its chord");
}

void PadRunRules_Test::aPadBackShowsTheStateAgain()
{
    /* Rule 2/6: the emergency stop is red on the screen and on the pad. A
       pad that reconnects starts in its own colour: it must be told red
       again, or the screen says STOPPED and the pad says nothing. */
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    hold(kLB, true);
    hold(kRB, true);
    axis(kLT, 255);
    axis(kRT, 255);
    QCOMPARE(stops(), 1);
    QCOMPARE(lastLight(m_stub), PadRun::stoppedColour().name());
    drop();
    m_stub->m_feedbackParams.clear();
    back();
    QCOMPARE(run->state(), PadRun::Stopped);
    QCOMPARE(lastLight(m_stub), PadRun::stoppedColour().name());
}

/*****************************************************************************
 * Rules 3 and 5: the chords
 *****************************************************************************/

void PadRunRules_Test::theStopsOwnBumpersDoNotArm()
{
    /* The stop, then the triggers let go first and the bumpers still
       squeezed for a beat (hands clenched): that is no arm chord. */
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    for (bool armedFirst : { false, true })
    {
        if (armedFirst)
            arm(true);
        run->setArmChordMs(40);
        const int before = stops();
        hold(kLB, true);
        hold(kRB, true);
        axis(kLT, 255);
        axis(kRT, 255);
        QCOMPARE(stops(), before + 1);
        axis(kRT, 0);
        axis(kLT, 0);
        QTest::qWait(200);
        QVERIFY2(!run->isArmed(), armedFirst ? "re-armed by the stop's own bumpers (armed first)"
                                             : "armed by the stop's own bumpers");
        QCOMPARE(run->state(), PadRun::Stopped);
        QCOMPARE(pc->padLight(), PadRun::stoppedColour());
        hold(kRB, false);
        hold(kLB, false);
        QVERIFY(!run->isArmed());
    }
    // A fresh chord still arms.
    arm(true);
    QCOMPARE(run->state(), PadRun::Armed);
}

void PadRunRules_Test::aTriggerInTheArmChordSpendsIt()
{
    /* LB + RB, a trigger touched on the way to the stop and let go: the
       stop was meant, not an arm. Nothing arms until both bumpers are let
       go and squeezed again. */
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    run->setArmChordMs(40);
    hold(kLB, true);
    hold(kRB, true);
    axis(kLT, 255);
    axis(kLT, 0);
    QTest::qWait(200);
    QVERIFY2(!run->isArmed(), "armed after a trigger was pulled inside the chord");
    // One bumper let go and pressed again: still spent.
    hold(kRB, false);
    hold(kRB, true);
    QTest::qWait(200);
    QVERIFY(!run->isArmed());
    hold(kRB, false);
    hold(kLB, false);
    arm(true);
    QVERIFY(run->isArmed());
}

void PadRunRules_Test::everyOrderOfTheFour()
{
    /* Rule 5 in all 24 orders, armed or not: one stop, no GO, no arm flip
       on the way in or out. Rule 4 / R5: on the way to the stop, RT is a
       hit only if it came before the bumpers and the safety (alone). */
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    m_doc->setMode(Doc::Operate);       // a hit pull says "No hit armed."
    const int chordMs = 12;
    for (bool armedFirst : { false, true })
    {
        quint32 order[4] = { kLB, kLT, kRB, kRT };
        std::sort(order, order + 4);
        do
        {
            const QString what = QString("%1 order %2 %3 %4 %5").arg(armedFirst ? "armed" : "not armed")
                                     .arg(order[0]).arg(order[1]).arg(order[2]).arg(order[3]);
            if (armedFirst)
                arm(true);
            run->setArmChordMs(chordMs);
            const int s0 = stops(), g0 = goes(), h0 = saidCount("No hit armed");
            QSignalSpy armed(run, &PadRun::armedChanged);
            for (quint32 ch : order)
            {
                hold(ch, true);
                QTest::qWait(2);
            }
            QTest::qWait(30);
            QVERIFY2(stops() == s0 + 1, qPrintable(what));
            QVERIFY2(goes() == g0, qPrintable(what));
            QVERIFY2(!run->isArmed(), qPrintable(what));
            QVERIFY2(armed.count() == (armedFirst ? 1 : 0), qPrintable(what));
            const bool rtAlone = order[0] == kRT;
            QVERIFY2(saidCount("No hit armed") - h0 == (armedFirst && rtAlone ? 1 : 0),
                     qPrintable(what + ": a hit on the way to the stop"));
            // Let go as pressed: the first two, then the last two, a beat each.
            hold(order[0], false);
            hold(order[1], false);
            QTest::qWait(30);
            QVERIFY2(!run->isArmed(), qPrintable(what + ", two let go"));
            hold(order[2], false);
            QTest::qWait(30);
            hold(order[3], false);
            QVERIFY2(!run->isArmed(), qPrintable(what + ", all let go"));
            QVERIFY2(run->state() == PadRun::Stopped, qPrintable(what));
            QVERIFY2(stops() == s0 + 1, qPrintable(what));
        } while (std::next_permutation(order, order + 4));
    }
    m_doc->setMode(Doc::Design);
}

void PadRunRules_Test::holdingTheFourIsOneStop()
{
    /* Held a long while (the trigger pressure wobbling), one stop. */
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    run->setArmChordMs(15);
    hold(kLB, true);
    hold(kRB, true);
    axis(kLT, 255);
    axis(kRT, 255);
    QElapsedTimer t;
    t.start();
    int i = 0;
    while (t.elapsed() < 1000)
    {
        axis(kLT, uchar(240 + (i % 16)));
        axis(kRT, uchar(255 - (i % 16)));
        ++i;
        QTest::qWait(10);
    }
    QCOMPARE(stops(), 1);
    QVERIFY(!run->isArmed());
    axis(kRT, 0);
    axis(kLT, 0);
    hold(kRB, false);
    hold(kLB, false);
    QCOMPARE(stops(), 1);
    QVERIFY(!run->isArmed());
}

void PadRunRules_Test::theStopWorksWhileWalking()
{
    /* Rule 5: "armed or not" -- and walking (Focus this cue from the pad,
       which can run in Operate). The four are the stop in the Run role
       whatever the pad is doing. */
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    press(kMenu);
    QVERIFY(run->isWalking());
    hold(kLB, true);
    hold(kRB, true);
    axis(kLT, 255);
    axis(kRT, 255);
    QVERIFY2(stops() == 1, "the four while walking were no emergency stop");
    QCOMPARE(run->state(), PadRun::Stopped);
    QCOMPARE(run->roleText(), QString("Pad: STOPPED"));
    QCOMPARE(pc->padLight(), PadRun::stoppedColour());
    axis(kRT, 0);
    axis(kLT, 0);
    hold(kRB, false);
    hold(kLB, false);
    QCOMPARE(stops(), 1);
    if (run->isWalking())
        pc->refineSession()->stop();
    QCOMPARE(run->state(), PadRun::Stopped);
    QCOMPARE(pc->padLight(), PadRun::stoppedColour());
}

void PadRunRules_Test::theStopWorksWhileAToolHasThePad()
{
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    RefineSession *r = pc->refineSession();

    // The Tools role: the four are Focus this cue's, never a stop.
    QVERIFY(r->start(m_cue1, 0xFFFFFFFF, nullptr));
    hold(kLB, true);
    hold(kRB, true);
    axis(kLT, 255);
    axis(kRT, 255);
    axis(kRT, 0);
    axis(kLT, 0);
    hold(kRB, false);
    hold(kLB, false);
    QCOMPARE(stops(), 0);
    r->stop();

    // The Run role, a tool started on the laptop: the stop.
    run->setEnabled(true);
    QVERIFY(r->start(m_cue1, 0xFFFFFFFF, nullptr));
    QCOMPARE(run->state(), PadRun::ToolHasPad);
    hold(kLB, true);
    hold(kRB, true);
    axis(kLT, 255);
    axis(kRT, 255);
    QVERIFY2(stops() == 1, "the four while a tool had the pad were no emergency stop");
    axis(kRT, 0);
    axis(kLT, 0);
    hold(kRB, false);
    hold(kLB, false);
    QCOMPARE(stops(), 1);
    QCOMPARE(run->state(), PadRun::Stopped);
    r->stop();
    QCOMPARE(run->state(), PadRun::Stopped);

    /* The Controller Test open: it runs nothing else, but the four are
       still the stop (review 0.4.10, M2: nothing swallows it). */
    QVERIFY(r->start(m_cue1, 0xFFFFFFFF, nullptr));
    run->setTesting(true);
    hold(kLB, true);
    hold(kRB, true);
    axis(kLT, 255);
    axis(kRT, 255);
    axis(kRT, 0);
    axis(kLT, 0);
    hold(kRB, false);
    hold(kLB, false);
    QCOMPARE(stops(), 2);
    run->setTesting(false);
    r->stop();
}

/*****************************************************************************
 * Rule 4: real pads' shapes
 *****************************************************************************/

void PadRunRules_Test::triggersThatJitterAreOnePull()
{
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    m_doc->setMode(Doc::Operate);
    arm(true);

    // RT bouncing round the press point: one pull, one hit.
    for (int v : { 150, 155, 149, 160, 152, 158, 151, 157, 154, 159 })
        axis(kRT, uchar(v));
    axis(kRT, 0);
    QCOMPARE(saidCount("No hit armed"), 1);
    // Pulled, then sagging round the release point and back: still one.
    axis(kRT, 255);
    for (int v : { 95, 92, 99, 91, 97, 120, 140 })
        axis(kRT, uchar(v));
    axis(kRT, 0);
    QCOMPARE(saidCount("No hit armed"), 2);
    // Sent as a button (0 / 255): one pull each.
    press(kRT);
    press(kRT);
    QCOMPARE(saidCount("No hit armed"), 4);

    // LT held but wobbling near its release: still the safety, one GO per A.
    axis(kLT, 255);
    for (int v : { 100, 92, 96, 91 })
        axis(kLT, uchar(v));
    press(kA);
    QCOMPARE(goes(), 1);
    // Down to the release point and only half back: no safety.
    axis(kLT, 85);
    axis(kLT, 120);
    press(kA);
    QCOMPARE(goes(), 1);
    axis(kLT, 0);

    // A worn LT that rests at 20-40: it neither blocks the arm nor is the safety.
    arm(false);
    axis(kLT, 35);
    arm(true);
    QVERIFY(run->isArmed());
    for (int v : { 20, 40, 28 })
    {
        axis(kLT, uchar(v));
        press(kA);
    }
    QCOMPARE(goes(), 1);
    // And disarms with it resting there too.
    arm(false);
    QVERIFY(!run->isArmed());
    m_doc->setMode(Doc::Design);
}

void PadRunRules_Test::theSamePressTwiceIsOneGo()
{
    /* Through the patch, as a pad's plugin delivers it: the same value
       sent twice (a repeated HID report) is one press. */
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    run->setArmChordMs(15);
    InputOutputMap *iom = m_doc->inputOutputMap();
    auto send = [&](quint32 ch, uchar v) { m_stub->emitValueChanged(UINT_MAX, 0, ch, v); };
    send(kLB, 255);
    send(kRB, 255);
    iom->flushInputs();
    QTRY_VERIFY(run->isArmed());
    send(kRB, 0);
    send(kLB, 0);
    iom->flushInputs();
    send(kLT, 255);
    iom->flushInputs();
    send(kA, 255);
    send(kA, 255);
    iom->flushInputs();
    send(kA, 0);
    iom->flushInputs();
    QCOMPARE(goes(), 1);
}

void PadRunRules_Test::aBouncingButton()
{
    /* A worn button chattering: down-up-down inside one frame (< 5 ms).
       Each down is a GO today: there is no debounce. Kept as a record of
       what a chattering A does; only a real worn pad says whether its
       firmware lets it through. */
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    run->setArmChordMs(15);
    InputOutputMap *iom = m_doc->inputOutputMap();
    auto send = [&](quint32 ch, uchar v) { m_stub->emitValueChanged(UINT_MAX, 0, ch, v); };
    arm(true);
    send(kLT, 255);
    iom->flushInputs();
    send(kA, 255);
    send(kA, 0);
    send(kA, 255);
    iom->flushInputs();
    send(kA, 0);
    iom->flushInputs();
    qInfo() << "a chattering A (down-up-down in one frame) made" << goes() << "GO(s)";
    QEXPECT_FAIL("", "no debounce: a chattering button is two GOs (reported, not fixed)", Continue);
    QCOMPARE(goes(), 1);
}

void PadRunRules_Test::theDuplicateDevice()
{
    /* The same pad seen twice (macOS can list one controller as two HID
       devices) and patched to two universes: every press arrives on both. */
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 1));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    run->setArmChordMs(15);
    auto both = [&](quint32 ch, uchar v) {
        axis(ch, v, 0);
        axis(ch, v, 1);
    };
    both(kLB, 255);
    both(kRB, 255);
    QTRY_VERIFY(run->isArmed());
    both(kRB, 0);
    both(kLB, 0);
    both(kLT, 255);
    both(kA, 255);
    both(kA, 0);
    both(kLT, 0);
    QCOMPARE(goes(), 1);                     // one GO, not two
    QEXPECT_FAIL("", "the twin's press is refused aloud on every GO (reported, not fixed)", Continue);
    QCOMPARE(saidCount("is running cues"), 0);
    // The stop on both: how many ALL OFFs.
    both(kLB, 255);
    both(kRB, 255);
    both(kLT, 255);
    both(kRT, 255);
    qInfo() << "the duplicated pad's emergency stop sent" << stops() << "ALL OFF(s)";
    QVERIFY(stops() >= 1);
    QVERIFY(!run->isArmed());
    both(kRT, 0);
    both(kLT, 0);
    both(kRB, 0);
    both(kLB, 0);
}

/*****************************************************************************
 * Rules 2 and 6: one state, the light
 *****************************************************************************/

void PadRunRules_Test::theLightAfterAToolOrWalkEndsStopped()
{
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    run->emergencyStop();
    QCOMPARE(pc->padLight(), PadRun::stoppedColour());

    // A tool started and ended on the laptop: still STOPPED, so still red.
    QVERIFY(pc->refineSession()->start(m_cue1, 0xFFFFFFFF, nullptr));
    pc->refineSession()->stop();
    QCOMPARE(run->state(), PadRun::Stopped);
    QVERIFY2(pc->padLight() == PadRun::stoppedColour(), "the screen says STOPPED, the pad's light does not");
    QCOMPARE(lastLight(m_stub), PadRun::stoppedColour().name());

    // A walk there and back from the pad: the same.
    press(kMenu);
    QVERIFY(run->isWalking());
    press(kMenu);
    QVERIFY(!run->isWalking());
    QCOMPARE(run->state(), PadRun::Stopped);
    QVERIFY2(pc->padLight() == PadRun::stoppedColour(), "after a walk: STOPPED on the screen, not on the pad");
    QCOMPARE(lastLight(m_stub), PadRun::stoppedColour().name());
}

void PadRunRules_Test::aFadingCueShowsPlayingForItsFade()
{
    /* Rule 6: a cue with a 3 s fade shows playing (the blink) for its fade
       and stops after. The real GO path and the real master timer. */
    makeShow();
    ShowTreeController *st = m_doc->showTree();
    ShowTree::Cue c = *st->cue(m_cue1);
    c.fadeIn = 3000;
    QVERIFY(st->setCue(c));
    ShowControl *sc = m_doc->showControl();
    sc->setRunHandler([this, sc](const ShowControl::Request &r, ShowControl::Route route) {
        m_runs << qMakePair(r, route);
        return sc->execute(r);
    });
    m_doc->setMode(Doc::Operate);
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    arm(true);
    m_doc->masterTimer()->start();
    auto stopTimer = qScopeGuard([this]() { m_doc->masterTimer()->stop(); });
    axis(kLT, 255);
    QElapsedTimer t;
    t.start();
    press(kA);
    axis(kLT, 0);
    QCOMPARE(goes(), 1);
    QVERIFY(run->isPlaying());

    // Sampled through the fade (after the 0.4 s flash) and well after it.
    QList<QPair<qint64, bool>> samples;
    while (t.elapsed() < 5000)
    {
        QTest::qWait(20);
        samples << qMakePair(t.elapsed(), run->isPlaying());
    }
    qint64 lastPlaying = -1;
    for (const auto &smp : samples)
        if (smp.second)
            lastPlaying = smp.first;
    qInfo() << "a 3 s fade showed playing until" << lastPlaying << "ms";
    for (const auto &smp : samples)
    {
        if (smp.first >= 700 && smp.first <= 2700)
            QVERIFY2(smp.second, qPrintable(QString("not playing %1 ms into a 3 s fade").arg(smp.first)));
        if (smp.first >= 4300)
            QVERIFY2(!smp.second, qPrintable(QString("still playing %1 ms into a 3 s fade").arg(smp.first)));
    }
    QCOMPARE(m_doc->programmer()->padLight(), PadRun::armedColour());
    m_doc->masterTimer()->stopAllFunctions();
    m_doc->setMode(Doc::Design);
}

/*****************************************************************************
 * Rule 7, and tools taking the pad mid-chord
 *****************************************************************************/

void PadRunRules_Test::aChordHeldAsAToolStartsIsForgotten()
{
    /* LB + RB squeezed, and inside the chord's time a tool starts on the
       laptop; the bumpers are let go while it has the pad (the tool hears
       that, the Run map does not). After the tool, a trigger pull and
       release must not arm, and LT + RT must not read as the four. */
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    run->setArmChordMs(150);
    hold(kLB, true);
    hold(kRB, true);
    QVERIFY(pc->refineSession()->start(m_cue1, 0xFFFFFFFF, nullptr));
    hold(kRB, false);
    hold(kLB, false);
    QTest::qWait(200);
    pc->refineSession()->stop();
    QCOMPARE(run->state(), PadRun::Run);
    axis(kLT, 255);
    axis(kLT, 0);
    QTest::qWait(300);
    QVERIFY2(!run->isArmed(), "armed with nothing held (the bumpers' release was the tool's)");
    axis(kLT, 255);
    axis(kRT, 255);
    QVERIFY2(stops() == 0, "LT + RT read as all four: an emergency stop");
    axis(kRT, 0);
    axis(kLT, 0);
}

void PadRunRules_Test::theControllerTestMidChord()
{
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    m_doc->setMode(Doc::Operate);

    // The chord begun, the test opened inside its time: no arm, then or after.
    run->setArmChordMs(60);
    hold(kLB, true);
    hold(kRB, true);
    run->setTesting(true);
    QTest::qWait(150);
    QVERIFY(!run->isArmed());
    hold(kRB, false);
    hold(kLB, false);
    run->setTesting(false);
    QTest::qWait(150);
    QVERIFY(!run->isArmed());

    // Armed, both triggers squeezed; the test opens (the arm drops) and
    // closes with them still squeezed: nothing fires, on the wobble either.
    arm(true);
    axis(kLT, 255);
    axis(kRT, 255);
    run->setTesting(true);
    QVERIFY(!run->isArmed());
    QCOMPARE(run->roleText(), QString("Pad: Controller Test"));
    QVERIFY(!pc->padLight().isValid());
    press(kA);
    run->setTesting(false);
    QCOMPARE(run->state(), PadRun::Run);
    axis(kRT, 250);
    axis(kLT, 250);
    press(kA);
    axis(kRT, 0);
    axis(kLT, 0);
    QCOMPARE(goes(), 0);
    QCOMPARE(stops(), 0);
    QCOMPARE(saidCount("No hit armed"), 0);
    m_doc->setMode(Doc::Design);
}

void PadRunRules_Test::theControllerTestSwallowsUnroledButtons()
{
    /* A button the profile gives an action but no gamepad role (a
       DualSense's mute, an Edge paddle, a Deck grip): while the Controller
       Test is open it is being tried, so it must not fire its action. */
    m_stub->m_buttonActions = { { kUnroled, "chaser_step_forward" } };
    ProgrammerController *pc = m_doc->programmer();
    pc->autoBindFromProfile();
    PadRun *run = pc->padRun();
    QSignalSpy actions(pc, &ProgrammerController::buttonActionTriggered);
    press(kUnroled);
    QCOMPARE(actions.count(), 1);           // what it does with the test closed
    run->setTesting(true);
    press(kUnroled);
    QVERIFY2(actions.count() == 1, "a button tried in the Controller Test fired its action");
    run->setTesting(false);
    run->setEnabled(true);
    run->setTesting(true);
    press(kUnroled);
    QCOMPARE(actions.count(), 1);
    run->setTesting(false);
}

void PadRunRules_Test::aButtonLetGoDuringTheTestIsLetGo()
{
    /* View held as the test opens, let go while it is open: after it,
       Menu alone is Menu, not the View + Menu role chord. */
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    run->setRoleChordMs(30);
    hold(kView, true);
    run->setTesting(true);
    hold(kView, false);
    run->setTesting(false);
    hold(kMenu, true);
    QTest::qWait(90);
    hold(kMenu, false);
    QVERIFY2(run->isEnabled(), "Menu alone switched the role: View was still 'held'");
}

/*****************************************************************************
 * Rules 1, 3, 4 and 5 over random presses
 *****************************************************************************/

void PadRunRules_Test::theRulesHoldOverRandomPresses_data()
{
    QTest::addColumn<quint32>("seed");
    QTest::addColumn<int>("pads");
    QTest::newRow("one pad, seed 11") << quint32(11) << 1;
    QTest::newRow("one pad, seed 2026") << quint32(2026) << 1;
    QTest::newRow("two pads, seed 7") << quint32(7) << 2;
}

void PadRunRules_Test::theRulesHoldOverRandomPresses()
{
    QFETCH(quint32, seed);
    QFETCH(int, pads);
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (pads > 1)
        QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 1));

    // Two lists; the Run tab plays the second (rule 1: the pad never changes it).
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Show");
    const quint32 act1 = st->addCueList(prod, "Act 1");
    const quint32 act2 = st->addCueList(prod, "Act 2");
    st->addCue(act1, -1, "Open");
    st->addCue(act2, -1, "Second");
    m_doc->controlMap()->setRunTabList(act2);
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    const quint32 list = run->listChaser();
    QCOMPARE(list, st->cueList(act2)->chaser);
    QSignalSpy listMoved(m_doc->controlMap(), &ControlMap::runTabListChanged);

    m_doc->setMode(Doc::Operate);           // a hit pull is said ("No hit armed.")
    run->setEnabled(true);
    const int chordMs = 15;
    run->setArmChordMs(chordMs);
    run->setRoleChordMs(600000);           // View + Menu never completes here

    /* The operator's model of each pad: what is held, and whether LB + RB
       are an arm chord (both down from a fresh second-bumper press, no
       trigger since, and since when). */
    struct Pad
    {
        bool lb = false, rb = false;
        ControllerScheme::TriggerButton lt, rt;
        bool spent = false;
        bool chord = false;
        QElapsedTimer since;
        QSet<quint32> down;
    };
    QHash<quint32, Pad> model;
    QString failure;
    bool inEvent = false;
    QString eventText;
    QList<QString> log;

    auto chordNow = [&](Pad &p) {
        const bool any = p.lt.isDown() || p.rt.isDown();
        if (p.lb && p.rb && any)
            p.spent = true;
        if (!p.lb && !p.rb)
            p.spent = false;
        const bool c = p.lb && p.rb && !any && !p.spent;
        if (c && !p.chord)
            p.since.start();
        p.chord = c;
    };
    const QMetaObject::Connection watch = connect(run, &PadRun::armedChanged, this, [&](bool on) {
        if (!failure.isEmpty())
            return;
        if (inEvent)
        {
            // Inside an event only the stop drops the arm.
            bool four = false;
            for (const Pad &p : std::as_const(model))
                four = four || (p.lb && p.rb && p.lt.isDown() && p.rt.isDown());
            if (on || !four)
                failure = QString("the arm went %1 on %2").arg(on ? "on" : "off", eventText);
            return;
        }
        // The chord's own time: some pad held a fresh LB + RB long enough.
        bool ok = false;
        for (const Pad &p : std::as_const(model))
            ok = ok || (p.chord && p.since.elapsed() >= chordMs * 95 / 100 - 2);
        if (!ok)
            failure = QString("the arm went %1 with no fresh LB + RB held %2 ms (after %3)")
                          .arg(on ? "on" : "off").arg(chordMs).arg(eventText);
    });

    auto unwatch = qScopeGuard([&watch]() { QObject::disconnect(watch); });

    const quint32 buttons[] = { kLB, kRB, kA, kB, kX, kY, kRS, kMenu, kView, kLS, kUp };
    const uchar triggerValues[] = { 0, 0, 20, 40, 90, 100, 150, 160, 200, 255 };
    const int waits[] = { 0, 0, 1, 2, 5, 10, 20, 30 };
    QRandomGenerator rng(seed);
    for (int e = 0; e < 200 && failure.isEmpty(); ++e)
    {
        const quint32 u = pads > 1 ? quint32(rng.bounded(pads)) : 0;
        Pad &p = model[u];
        const bool trigger = rng.bounded(3) == 0;
        quint32 ch;
        uchar value;
        if (trigger)
        {
            ch = rng.bounded(2) == 0 ? kLT : kRT;
            value = triggerValues[rng.bounded(int(sizeof(triggerValues)))];
        }
        else
        {
            ch = buttons[rng.bounded(int(sizeof(buttons) / sizeof(buttons[0])))];
            value = p.down.contains(ch) ? 0 : 255;
        }
        const bool armedBefore = run->isArmed();
        const quint32 armedOn = run->armedUniverse();
        const bool ltBefore = p.lt.isDown();
        const bool wasFour = p.lb && p.rb && p.lt.isDown() && p.rt.isDown();
        bool rtPull = false;
        if (ch == kLT)
            p.lt.update(value / 255.0f);
        else if (ch == kRT)
            rtPull = p.rt.update(value / 255.0f);
        else
        {
            if (value > 0)
                p.down.insert(ch);
            else
                p.down.remove(ch);
            if (ch == kLB)
                p.lb = value > 0;
            if (ch == kRB)
                p.rb = value > 0;
        }
        chordNow(p);
        const bool isFour = p.lb && p.rb && p.lt.isDown() && p.rt.isDown();
        eventText = QString("event %1: pad %2 ch %3 = %4").arg(e).arg(u).arg(ch).arg(value);
        log << eventText;
        const int runs0 = m_runs.size();
        const int hits0 = saidCount("No hit armed");
        inEvent = true;
        axis(ch, value, u);
        inEvent = false;
        if (!failure.isEmpty())
            break;

        // Rules 4 and 1: what this event sent.
        for (int i = runs0; i < m_runs.size(); ++i)
        {
            const ShowControl::Request &r = m_runs.at(i).first;
            if (r.command == ShowControl::AllOff)
            {
                if (!(isFour && !wasFour))
                    failure = "an ALL OFF without the four: " + eventText;
                continue;
            }
            const quint32 want = r.command == ShowControl::Go ? kA : r.command == ShowControl::Back ? kB
                               : r.command == ShowControl::SubGo ? kX : kY;
            if (!isGoFamily(r.command) || !armedBefore || armedOn != u || ch != want || value == 0
                || !ltBefore)
                failure = QString("%1 without the arm, the safety and its button: %2")
                              .arg(ShowControl::commandName(r.command), eventText);
            else if (r.chaser != list)
                failure = "a GO to another list than the Run tab's: " + eventText;
        }
        // Rule 4 (and R5): a hit is RT pulled alone, armed, on the armed pad.
        if (saidCount("No hit armed") != hits0
            && !(rtPull && armedBefore && armedOn == u && !p.lb && !p.rb && !ltBefore))
            failure = "a hit not on RT alone, armed: " + eventText;
        if (run->isWalking())
            failure = "a walk began in Operate with nothing running: " + eventText;
        // Rule 2: the state's words and colour, and the pad's light.
        if (failure.isEmpty())
        {
            const PadRun::State s = run->state();
            if (!run->isPlaying() && (s == PadRun::Run || s == PadRun::Armed || s == PadRun::Stopped)
                && pc->padLight() != run->stateColour())
                failure = "the pad's light is not the state's colour after " + eventText;
            /* Armed: those words; with two pads, and the pad that runs cues
               (controller jobs: "Pad: ARMED · 2: Stub 2"). */
            const QString armedWords = run->padCount() > 1
                ? QStringLiteral("Pad: ARMED \u00B7 ") + run->runnerName() : QStringLiteral("Pad: ARMED");
            if (s == PadRun::Armed && run->roleText() != armedWords)
                failure = "armed, but the words are " + run->roleText();
        }
        QTest::qWait(waits[rng.bounded(int(sizeof(waits) / sizeof(waits[0])))]);
    }
    if (!failure.isEmpty())
        QFAIL(qPrintable(failure + "\nlast events:\n  " + log.mid(qMax(0, log.size() - 12)).join("\n  ")));
    QVERIFY(run->isEnabled());
    // Rule 1: the list is as the screen left it.
    QCOMPARE(m_doc->controlMap()->runTabList(), act2);
    QCOMPARE(run->listChaser(), list);
    QCOMPARE(listMoved.count(), 0);
    m_doc->setMode(Doc::Design);
}

/*****************************************************************************
 * Controller jobs (docs/design/CONTROLLER_JOBS.md), swept
 *****************************************************************************/

namespace
{
using CJ = ControllerJobs;
/* One gesture on a pad: (channel, value) in order, all let go at the end;
   chord: the arm chord's hold is waited out in the middle. */
struct Gesture
{
    QString name;
    QList<QPair<quint32, uchar>> steps;
    bool chord = false;
};

QList<Gesture> everyGesture()
{
    QList<Gesture> g;
    for (quint32 ch : { kA, kB, kX, kY, kLB, kRB, kView, kMenu, kLS, kRS, kUp, kUnroled })
        g << Gesture { QString("press %1").arg(ch), { { ch, 255 }, { ch, 0 } } };
    g << Gesture { "LT", { { kLT, 255 }, { kLT, 0 } } };
    g << Gesture { "RT (the hit)", { { kRT, 255 }, { kRT, 0 } } };
    for (quint32 ch : { kA, kB, kX, kY, kMenu })
        g << Gesture { QString("LT + %1").arg(ch), { { kLT, 255 }, { ch, 255 }, { ch, 0 }, { kLT, 0 } } };
    g << Gesture { "View + Menu", { { kView, 255 }, { kMenu, 255 }, { kMenu, 0 }, { kView, 0 } } };
    Gesture arm { "LB + RB held (the arm)", { { kLB, 255 }, { kRB, 255 }, { kRB, 0 }, { kLB, 0 } } };
    arm.chord = true;
    g << arm;
    g << Gesture { "the four (the stop)", { { kLB, 255 }, { kRB, 255 }, { kLT, 255 }, { kRT, 255 },
                                            { kRT, 0 }, { kLT, 0 }, { kRB, 0 }, { kLB, 0 } } };
    return g;
}
}

void PadRunRules_Test::jobsEveryControlOnASecondPad_data()
{
    QTest::addColumn<int>("jobsB");
    QTest::addColumn<bool>("aArmed");
    // Every mix of pad B's jobs (eight), pad A armed and not.
    for (int j = 0; j <= int(CJ::AllJobs); ++j)
        for (bool armed : { false, true })
        {
            QStringList names;
            for (CJ::Job job : CJ::jobs())
                if (j & job)
                    names << CJ::jobKey(job);
            QTest::newRow(qPrintable(QString("B[%1] A %2").arg(names.isEmpty() ? "none" : names.join('+'),
                                                                armed ? "armed" : "not armed")))
                << j << armed;
        }
}

void PadRunRules_Test::jobsEveryControlOnASecondPad()
{
    /* Pad A has every job; pad B the row's. Every control and chord on B,
       with A armed and not: what it does is exactly its jobs' (invariants
       4-6, 8). A GO never comes from B (it never holds Run here), the arm
       only moves to B on its chord with Run cues and no runner, the stop
       always stops; a refusal says why. */
    QFETCH(int, jobsB);
    QFETCH(bool, aArmed);
    makeShow();
    twoPads(int(CJ::AllJobs), jobsB);
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    run->setArmChordMs(15);
    const bool runs = jobsB & CJ::RunCues, focus = jobsB & CJ::Focus;

    for (const Gesture &g : everyGesture())
    {
        if (aArmed)
            arm(true, 0);
        else
            arm(false, run->armedUniverse() == UINT_MAX ? 0 : run->armedUniverse());
        QCOMPARE(run->isArmed(), aArmed);
        if (run->isWalking())
            run->stopWalk();
        const int goesBefore = goes(), stopsBefore = stops();
        const int noRun = saidCount("doesn't run cues"), noFocus = saidCount("doesn't do focus");
        const int runningB = saidCount("is running cues");
        for (int i = 0; i < g.steps.size(); ++i)
        {
            axis(g.steps.at(i).first, g.steps.at(i).second, 1);
            /* The arm's hold, waited out: until its timer has fired, not
               a fixed time (CI's slow macOS runner fired a 15 ms timer
               after 60 ms). A chord that is never timed is not pending. */
            if (g.chord && i == 1)
                QTRY_VERIFY_WITH_TIMEOUT(!run->isArmChordPending(), 5000);
        }
        const QString why = QString("%1 on B").arg(g.name);
        // A GO never comes from a pad that does not hold Run.
        QVERIFY2(goes() == goesBefore, qPrintable(why + ": a GO"));
        if (g.name.startsWith("the four"))
        {
            // The stop is every pad's, whatever its jobs (invariant 4).
            QVERIFY2(stops() == stopsBefore + 1, qPrintable(why));
            QVERIFY(!run->isArmed());
            QCOMPARE(run->state(), PadRun::Stopped);
            continue;
        }
        QVERIFY2(stops() == stopsBefore, qPrintable(why + ": a stop"));
        if (g.chord)
        {
            if (!runs)
            {
                QVERIFY2(saidCount("doesn't run cues") > noRun, qPrintable(why));
                QCOMPARE(run->isArmed(), aArmed);
            }
            else if (aArmed)
            {
                // One runner: refused, named, and A as it was (invariant 6).
                QVERIFY2(saidCount("Pad A (\u202600:01) is running cues") > runningB, qPrintable(why));
                QVERIFY(run->isArmed());
                QCOMPARE(run->armedUniverse(), quint32(0));
            }
            else
            {
                QVERIFY2(run->isArmed() && run->armedUniverse() == 1, qPrintable(why + ": B did not arm"));
                arm(false, 1);
            }
            continue;
        }
        // Nothing else moves the arm.
        QVERIFY2(run->isArmed() == aArmed, qPrintable(why + ": the arm moved"));
        if (aArmed)
            QCOMPARE(run->armedUniverse(), quint32(0));
        const bool cueCommand = g.name == "press 10" || g.name == "press 11" || g.name == "press 12"
                                || g.name == "press 13"
                                || (g.name.startsWith("LT + ") && !g.name.endsWith(QString::number(kMenu)));
        const bool menu = g.name.endsWith(QString::number(kMenu)) && !g.name.startsWith("View");
        if (cueCommand || g.name == QString("press %1").arg(kRS) || g.name.startsWith("RT"))
        {
            if (!runs)
                QVERIFY2(saidCount("doesn't run cues") > noRun, qPrintable(why + ": not told"));
            else if (aArmed && cueCommand)
                QVERIFY2(saidCount("is running cues") > runningB, qPrintable(why + ": not told who runs"));
        }
        if (menu)
            QVERIFY2((saidCount("doesn't do focus") > noFocus) == !focus, qPrintable(why + ": focus"));
        // A runner is never told the job words of B's.
        if (runs && !menu)
            QVERIFY2(saidCount("doesn't run cues") == noRun, qPrintable(why + ": refused with Run cues"));
    }
    // And pad A still runs the show.
    arm(true, 0);
    const int before = goes();
    axis(kLT, 255, 0);
    press(kA, 0);
    axis(kLT, 0, 0);
    QCOMPARE(goes(), before + 1);
}

void PadRunRules_Test::jobsOneRunnerAtATime()
{
    makeShow();
    twoPads(int(CJ::AllJobs), int(CJ::AllJobs));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    arm(true, 0);
    QCOMPARE(run->runnerName(), QString::fromUtf8("Pad A (\u202600:01)"));
    // Two pads: the chip names the one that runs cues (invariant 9).
    QCOMPARE(run->roleText(), QString::fromUtf8("Pad: ARMED \u00B7 Pad A (\u202600:01)"));
    // B's chord, held long and let go: refused, A as it was, its safety kept.
    axis(kLT, 255, 0);
    hold(kLB, true, 1);
    hold(kRB, true, 1);
    QTest::qWait(80);
    QTRY_VERIFY_WITH_TIMEOUT(!run->isArmChordPending(), 5000);   // and its timer fired (slow CI)
    hold(kRB, false, 1);
    hold(kLB, false, 1);
    QCOMPARE(run->notice(), QString::fromUtf8("Pad A (\u202600:01) is running cues."));
    // The chip already names A: its refusal does not again.
    QCOMPARE(run->chipNotice(), QString("another controller is running cues"));
    QVERIFY(run->isArmed());
    QCOMPARE(run->armedUniverse(), quint32(0));
    press(kA, 0);                       // A's LT still held: its GO goes
    axis(kLT, 0, 0);
    QCOMPARE(goes(), 1);
    // B's LT + A: refused, named.
    axis(kLT, 255, 1);
    press(kA, 1);
    axis(kLT, 0, 1);
    QCOMPARE(goes(), 1);
    QCOMPARE(run->notice(), QString::fromUtf8("Pad A (\u202600:01) is running cues."));
    // A lets go: the next to arm holds it.
    arm(false, 0);
    arm(true, 1);
    QCOMPARE(run->armedUniverse(), quint32(1));
    QCOMPARE(run->roleText(), QString::fromUtf8("Pad: ARMED \u00B7 Pad B (\u202600:02)"));
    axis(kLT, 255, 1);
    press(kA, 1);
    axis(kLT, 0, 1);
    QCOMPARE(goes(), 2);
    hold(kLB, true, 0);
    hold(kRB, true, 0);
    QTest::qWait(80);
    QTRY_VERIFY_WITH_TIMEOUT(!run->isArmChordPending(), 5000);   // and its timer fired (slow CI)
    hold(kRB, false, 0);
    hold(kLB, false, 0);
    QCOMPARE(run->armedUniverse(), quint32(1));
    QCOMPARE(run->notice(), QString::fromUtf8("Pad B (\u202600:02) is running cues."));
}

void PadRunRules_Test::jobsTheStopFromAPadWithNoJobs()
{
    makeShow();
    twoPads(int(CJ::AllJobs), int(CJ::NoJobs));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    // A armed: B's four stop everything.
    arm(true, 0);
    for (quint32 ch : { kLB, kRB, kLT, kRT })
        axis(ch, 255, 1);
    QCOMPARE(stops(), 1);
    QCOMPARE(run->state(), PadRun::Stopped);
    QVERIFY(!run->isArmed());
    for (quint32 ch : { kRT, kLT, kRB, kLB })
        axis(ch, 0, 1);
    // Nobody armed: still the stop.
    arm(true, 0);
    arm(false, 0);
    for (quint32 ch : { kRT, kLB, kLT, kRB })
        axis(ch, 255, 1);
    QCOMPARE(stops(), 2);
    for (quint32 ch : { kRT, kLB, kLT, kRB })
        axis(ch, 0, 1);
    // While a tool has the pad: the stop too, from the pad with no jobs.
    RefineSession *r = m_doc->programmer()->refineSession();
    run->setEnabled(false);
    run->setEnabled(true);
    QVERIFY(r->start(m_cue1, 0xFFFFFFFF, nullptr));
    for (quint32 ch : { kLB, kRB, kLT, kRT })
        axis(ch, 255, 1);
    QCOMPARE(stops(), 3);
    for (quint32 ch : { kRT, kLT, kRB, kLB })
        axis(ch, 0, 1);
    r->stop();
    // And nothing else it does is a command.
    QCOMPARE(goes(), 0);
}

void PadRunRules_Test::jobsOnePadNothingSavedIsAsBefore()
{
    /* One pad, nothing saved (invariant 1): every job, the chip unnamed,
       the arm, GO and the stop as they were, nothing written. */
    makeShow();
    ControllerJobs *jobs = m_doc->programmer()->controllerJobs();
    QVERIFY(jobs->saved().isEmpty());
    for (CJ::Job job : CJ::jobs())
        QVERIFY(jobs->has(0, job));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    QCOMPARE(run->padCount(), 1);
    press(kA);
    QCOMPARE(run->notice(), QString("Not armed: hold LB and RB."));
    arm(true);
    QCOMPARE(run->roleText(), QString("Pad: ARMED"));
    QCOMPARE(lightOn(0), PadRun::armedColour().name());
    axis(kLT, 255);
    press(kA);
    axis(kLT, 0);
    QCOMPARE(goes(), 1);
    for (quint32 ch : { kLB, kRB, kLT, kRT })
        axis(ch, 255);
    QCOMPARE(stops(), 1);
    for (quint32 ch : { kRT, kLT, kRB, kLB })
        axis(ch, 0);
    QCOMPARE(saidCount("doesn't"), 0);
    QCOMPARE(saidCount("is running cues"), 0);
    QVERIFY(jobs->saved().isEmpty());
}

void PadRunRules_Test::jobsEachPadsLight()
{
    makeShow();
    twoPads(int(CJ::AllJobs), int(CJ::AllJobs));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    arm(true, 1);
    // The runner shows the arm; the other its own colour (invariant 9).
    QCOMPARE(lightOn(1), PadRun::armedColour().name());
    QCOMPARE(lightOn(0), QString());
    arm(false, 1);
    arm(true, 0);
    QCOMPARE(lightOn(0), PadRun::armedColour().name());
    QCOMPARE(lightOn(1), QString());
    // The stop: red on every pad.
    for (quint32 ch : { kLB, kRB, kLT, kRT })
        axis(ch, 255, 1);
    QCOMPARE(lightOn(0), PadRun::stoppedColour().name());
    QCOMPARE(lightOn(1), PadRun::stoppedColour().name());
    for (quint32 ch : { kRT, kLT, kRB, kLB })
        axis(ch, 0, 1);
}

void PadRunRules_Test::jobsTheFollowSpotSkipsAPad()
{
    twoPads(int(CJ::RunCues | CJ::Focus), int(CJ::AllJobs));
    ProgrammerController *pc = m_doc->programmer();
    pc->refreshStickBinding();
    // A has no Follow spot: the binding is B's (invariant 7).
    QCOMPARE(pc->followSpotStickName(), QString("Pad B"));
    QVERIFY(pc->followSpotJobNote().isEmpty());
    // B's job taken away: no pad steers it, and that is said.
    ControllerJobs *jobs = pc->controllerJobs();
    jobs->setJob(jobs->identityOfUniverse(1), CJ::FollowSpot, false);
    QCOMPARE(pc->followSpotStickName(), QString());
    QCOMPARE(pc->followSpotJobNote(), QString("No controller has the Follow spot job."));
    // A given it: A's.
    jobs->setJob(jobs->identityOfUniverse(0), CJ::FollowSpot, true);
    QCOMPARE(pc->followSpotStickName(), QString("Pad A"));
}

void PadRunRules_Test::jobsFocusOnlyFromFocusPads()
{
    makeShow();
    twoPads(int(CJ::AllJobs), int(CJ::RunCues | CJ::FollowSpot));
    ProgrammerController *pc = m_doc->programmer();
    // Tools: View + Menu on B is no Focus mode; on A it is.
    QSignalSpy focusMode(pc, &ProgrammerController::focusModeRequested);
    for (quint32 u : { quint32(1), quint32(0) })
    {
        hold(kView, true, u);
        hold(kMenu, true, u);
        hold(kMenu, false, u);
        hold(kView, false, u);
    }
    QCOMPARE(focusMode.count(), 1);
    QCOMPARE(saidCount("doesn't do focus"), 1);
    // A tool started on the laptop: B's presses take nothing, A's do (Q135).
    RefineSession *r = pc->refineSession();
    QVERIFY(r->start(m_cue1, 0xFFFFFFFF, nullptr));
    press(kY, 1);
    QCOMPARE(pc->controllerOwner(), quint32(0xFFFFFFFF));
    QCOMPARE(saidCount("doesn't do focus"), 2);
    press(kY, 0);
    QCOMPARE(pc->controllerOwner(), quint32(0));
    // Two pads with Focus: the first press still owns it.
    ControllerJobs *jobs = pc->controllerJobs();
    jobs->setJob(jobs->identityOfUniverse(1), CJ::Focus, true);
    press(kY, 1);
    QCOMPARE(pc->controllerOwner(), quint32(0));
    QCOMPARE(saidCount("Another controller is in use"), 1);
    r->stop();
    // The Run role: Menu (the walk) on a pad without Focus is refused.
    jobs->setJob(jobs->identityOfUniverse(1), CJ::Focus, false);
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    press(kMenu, 1);
    QVERIFY(!run->isWalking());
    QCOMPARE(run->notice(), QString("This controller doesn't do focus."));
}

namespace
{
QString jobNames(int jobs)
{
    QStringList names;
    for (ControllerJobs::Job job : ControllerJobs::jobs())
        if (jobs & job)
            names << ControllerJobs::jobKey(job);
    return names.isEmpty() ? QStringLiteral("none") : names.join('+');
}
}

void PadRunRules_Test::jobsVenueFocusFromAFocusPad_data()
{
    QTest::addColumn<int>("jobs");
    QTest::addColumn<int>("runnerLine");
    // The other pad: each of the eight job mixes; the runner on either line.
    for (int line : { 0, 1 })
        for (int j = 0; j <= int(CJ::AllJobs); ++j)
            QTest::newRow(qPrintable(QString("other[%1] runner on line %2").arg(jobNames(j)).arg(line)))
                << j << line;
}

void PadRunRules_Test::jobsVenueFocusFromAFocusPad()
{
    QFETCH(int, jobs);
    QFETCH(int, runnerLine);
    const quint32 r = quint32(runnerLine), o = 1 - r;
    makeShow();
    if (r == 0)
        twoPads(int(CJ::RunCues), jobs);
    else
        twoPads(jobs, int(CJ::RunCues));
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    run->setEnabled(true);
    arm(true, r);
    QCOMPARE(run->armedUniverse(), r);
    const QString oName = pc->controllerJobs()->nameOfUniverse(o);
    const bool focus = jobs & CJ::Focus;
    // How it starts, said everywhere from one source: no arm on a Focus pad.
    QCOMPARE(fcc->readyPhrase(), focus ? QString("Venue focus ready. Hold {LT} and press {Menu} to begin.")
                                       : QString("Venue focus ready. No controller does focus: start it on the laptop."));

    // Menu alone on the other pad: never a start on one button.
    press(kMenu, o);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCOMPARE(run->notice(), focus ? QString("To start the venue focus, hold LT and press Menu.")
                                  : QString("This controller doesn't do focus."));
    QVERIFY(run->isArmed());

    // LT + Menu on it.
    ltMenu(o);
    QVERIFY2(run->isArmed() && run->armedUniverse() == r, "the runner's arm moved");
    if (!focus)
    {
        QCOMPARE(fcc->state(), FocusCallCue::Armed);
        QCOMPARE(run->state(), PadRun::Armed);
        QCOMPARE(run->notice(), QString("This controller doesn't do focus."));
        withLt(kA, r);
        QCOMPARE(goes(), 1);
        return;
    }
    QVERIFY2(fcc->state() == FocusCallCue::Running, qPrintable(run->notice()));
    QVERIFY(pc->focusSession()->isActive());
    QCOMPARE(pc->controllerOwner(), o);
    // FOCUS names the focus's pad; only its light is blue (one source).
    QCOMPARE(run->state(), PadRun::Focus);
    QCOMPARE(run->focusOwnerUniverse(), o);
    QCOMPARE(run->roleText(), QString::fromUtf8("Pad: FOCUS · %1").arg(oName));
    QCOMPARE(run->lightUniverse(), o);
    QCOMPARE(lightOn(o), PadRun::focusColour().name());
    QCOMPARE(lightOn(r), QString());

    // The runner: armed all through; its GO family refused, saying why.
    const QString why = QString("Venue focus is running on %1: nothing plays until it ends.").arg(oName);
    for (quint32 ch : { kA, kB, kX, kY })
    {
        run->showNotice(QString());
        withLt(ch, r);
        QVERIFY2(run->notice() == why, qPrintable(QString("LT + %1: '%2'").arg(ch).arg(run->notice())));
        QVERIFY(run->isArmed());
        QCOMPARE(run->armedUniverse(), r);
    }
    // RT (no hit on): no new hit.
    run->showNotice(QString());
    press(kRT, r);
    QCOMPARE(run->notice(), why);
    // Its arm chord: no change in FOCUS, and said.
    run->showNotice(QString());
    hold(kLB, true, r);
    hold(kRB, true, r);
    QTRY_VERIFY_WITH_TIMEOUT(!run->isArmChordPending(), 5000);
    hold(kRB, false, r);
    hold(kLB, false, r);
    QVERIFY(run->isArmed());
    QCOMPARE(run->notice(), why);
    // Its LT + Menu ends nothing (it has no Focus).
    ltMenu(r);
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QCOMPARE(run->notice(), QString("This controller doesn't do focus."));
    QCOMPARE(goes(), 0);
    QCOMPARE(stops(), 0);

    // The focus's pad ends it: the runner is ARMED, its GO works, no re-arm.
    ltMenu(o);
    QVERIFY(!pc->focusSession()->isActive());
    QCOMPARE(run->state(), PadRun::Armed);
    QCOMPARE(run->armedUniverse(), r);
    QCOMPARE(lightOn(r), PadRun::armedColour().name());
    withLt(kA, r);
    QCOMPARE(goes(), 1);
}

void PadRunRules_Test::jobsTheStopDuringAnotherPadsFocus_data()
{
    QTest::addColumn<int>("stopper");
    QTest::newRow("the runner (Run cues only)") << 0;
    QTest::newRow("the focus pad (Focus only)") << 1;
}

void PadRunRules_Test::jobsTheStopDuringAnotherPadsFocus()
{
    QFETCH(int, stopper);
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    arm(true, 0);
    ltMenu(1);
    QCOMPARE(run->state(), PadRun::Focus);
    QVERIFY(run->isArmed());
    for (quint32 ch : { kLB, kRB, kLT, kRT })
        axis(ch, 255, quint32(stopper));
    // First, from either pad, whatever its jobs: everything stops, red on both.
    QCOMPARE(stops(), 1);
    QCOMPARE(run->state(), PadRun::Stopped);
    QVERIFY(!run->isArmed());
    QCOMPARE(lightOn(0), PadRun::stoppedColour().name());
    QCOMPARE(lightOn(1), PadRun::stoppedColour().name());
    for (quint32 ch : { kRT, kLT, kRB, kLB })
        axis(ch, 0, quint32(stopper));
    // The focus ended after the stop: the runner is not armed again.
    ltMenu(1);
    QVERIFY(!pc->focusSession()->isActive());
    QVERIFY(!run->isArmed());
    withLt(kA, 0);
    QCOMPARE(goes(), 0);
}

void PadRunRules_Test::jobsRunAndFocusOnOnePadAsBefore()
{
    makeShow();
    twoPads(int(CJ::AllJobs), int(CJ::FollowSpot));
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    run->setEnabled(true);
    // The pad that runs cues starts from its arm: not armed, refused.
    QCOMPARE(fcc->readyPhrase(), QString("Venue focus ready. Armed, hold {LT} and press {Menu} to begin."));
    ltMenu(0);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCOMPARE(run->notice(), QString("Not armed. To start the venue focus, hold LB and RB to arm, then LT + Menu."));
    arm(true, 0);
    press(kMenu, 0);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QVERIFY(run->notice().contains("LT + Menu starts the venue focus"));
    // Armed, LT + Menu: FOCUS, the arm put by.
    ltMenu(0);
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QCOMPARE(run->state(), PadRun::Focus);
    QVERIFY(!run->isArmed());
    QCOMPARE(run->m_focusArm, quint32(0));
    QCOMPARE(run->roleText(), QString::fromUtf8("Pad: FOCUS · Pad A (…00:01)"));
    // The end: ARMED again on it.
    ltMenu(0);
    QVERIFY(!pc->focusSession()->isActive());
    QCOMPARE(run->state(), PadRun::Armed);
    QCOMPARE(run->armedUniverse(), quint32(0));
    withLt(kA, 0);
    QCOMPARE(goes(), 1);
}

void PadRunRules_Test::jobsAHitPlayingAcrossAnotherPadsFocus()
{
    /* "A cue that fires while a hit is firing lets the hit complete": the
       focus start is no exception. The runner's toggle hit stays on across
       it until the runner's own pull lets it go; a new pull waits. */
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    makeFocusCue();
    ShowTreeController *st = m_doc->showTree();
    const quint32 hit = st->addHit(ShowTree::HitScope::Production, st->productions().first().id, "Strobe");
    QVERIFY(hit != ShowTree::invalidId());
    ShowTree::Target t;
    t.fixtures = m_spots;
    ShowTree::Look l;
    l.key = ShowTree::KeyDimmer;
    l.palettes = { m_doc->palettes().first()->id() };
    QVERIFY(st->setLayerLook(hit, st->addLayer(hit, t, "Spots"), l));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    arm(true, 0);
    QCOMPARE(run->armedHits(), QList<quint32>({ hit }));
    QSignalSpy fired(run, &PadRun::hitFired);
    press(kRT, 0);
    QCOMPARE(fired.count(), 1);
    Function *f = m_doc->function(st->lookFunction(hit));
    QVERIFY(f != nullptr);
    m_doc->masterTimer()->timerTick();
    QVERIFY(f->isRunning());

    // B starts the venue focus: the hit plays on.
    ltMenu(1);
    QCOMPARE(run->state(), PadRun::Focus);
    m_doc->masterTimer()->timerTick();
    QVERIFY2(f->isRunning(), "the focus start cut the hit");
    QVERIFY(run->isArmed());
    // A's pull lets it go: that is no new hit.
    press(kRT, 0);
    QCOMPARE(fired.count(), 2);
    QVERIFY(!fired.last().at(1).toBool());
    m_doc->masterTimer()->timerTick();
    QVERIFY(!f->isRunning());
    // A new pull: refused while the focus runs, and why.
    run->showNotice(QString());
    press(kRT, 0);
    QCOMPARE(fired.count(), 2);
    QCOMPARE(run->notice(), QString::fromUtf8("Venue focus is running on Pad B (…00:02): nothing plays until it ends."));
    m_doc->masterTimer()->timerTick();
    QVERIFY(!f->isRunning());
    // The end: the words go with it; A's pull fires it again, no re-arm.
    ltMenu(1);
    QVERIFY(!pc->focusSession()->isActive());
    QCOMPARE(run->notice(), QString());
    press(kRT, 0);
    QCOMPARE(fired.count(), 3);
    QVERIFY(fired.last().at(1).toBool());
    QCOMPARE(goes(), 0);
    m_doc->masterTimer()->stopAllFunctions();
}

void PadRunRules_Test::jobsTheWalkBesideTheRunner()
{
    // Operate (Q252): the walk waits for the runner, and says who.
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    m_doc->setMode(Doc::Operate);
    arm(true, 0);
    press(kMenu, 1);
    QVERIFY(!run->isWalking());
    QVERIFY(run->isArmed());
    QCOMPARE(run->notice(), QString::fromUtf8("Pad A (\u202600:01) is running cues: in Operate the walk waits "
                                              "until it disarms."));
    // A disarms: B walks with its Focus job alone (Design, the cue in hand).
    arm(false, 0);
    m_doc->setMode(Doc::Design);
    press(kMenu, 1);
    QVERIFY(run->isWalking());
    run->stopWalk();
}

void PadRunRules_Test::jobsASwipeIsTheFocusJobs()
{
    const quint32 kTouchX = 30, kTouchY = 31, kTouchDown = 32;
    m_stub->m_channelRoles.insert(kTouchX, "touch_x");
    m_stub->m_channelRoles.insert(kTouchY, "touch_y");
    m_stub->m_channelRoles.insert(kTouchDown, "touch_down");
    makeShow();
    twoPads(int(CJ::RunCues | CJ::FollowSpot), int(CJ::Focus));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    auto swipe = [&](quint32 u) {
        auto at = [&](float x) {
            axis(kTouchX, uchar(x * 255.0f), u);
            axis(kTouchY, uchar(0.5f * 255.0f), u);
        };
        at(0.2f);
        axis(kTouchDown, 255, u);
        run->swipeTick(0.02);
        for (int i = 1; i <= 10; ++i)
        {
            at(0.2f + 0.6f * i / 10.0f);
            run->swipeTick(0.02);
        }
        axis(kTouchDown, 0, u);
        run->swipeTick(0.02);
        run->swipeTick(1.0);
    };
    // A, no Focus: its swipe walks nothing.
    swipe(0);
    QVERIFY2(!run->isWalking(), "a pad without the Focus job started a walk");
    // B, Focus: it walks.
    swipe(1);
    QVERIFY(run->isWalking());
    run->stopWalk();
}

void PadRunRules_Test::jobsFocusModeInRunIsSaidOnScreen()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    QSignalSpy focusMode(pc, &ProgrammerController::focusModeRequested);
    hold(kView, true, 1);
    hold(kMenu, true, 1);
    hold(kMenu, false, 1);
    hold(kView, false, 1);
    QCOMPARE(focusMode.count(), 0);
    QVERIFY(run->isEnabled());
    QCOMPARE(run->notice(), QString("Focus mode is in the Tools role. Run / Tools is switched on the computer: "
                                    "the Run tab's Pad: button."));
}

void PadRunRules_Test::jobsTheFocusControllerHasTheFocusJob()
{
    twoPads(int(CJ::RunCues | CJ::FollowSpot), int(CJ::Focus));
    ProgrammerController *pc = m_doc->programmer();
    pc->refreshStickBinding();
    QCOMPARE(pc->focusController().universe, quint32(1));
    // With no pad with the job: the first, as before.
    ControllerJobs *jobs = pc->controllerJobs();
    jobs->setJob(jobs->identityOfUniverse(1), CJ::Focus, false);
    QVERIFY(pc->focusController().present);
}

/*****************************************************************************
 * Review of a430351ef (Q250, each job on its own)
 *****************************************************************************/

void PadRunRules_Test::reviewFeedbackGoesToThePadWhoseActionItIs()
{
    /* A runs cues (a DualSense), B does focus (an Xbox pad). The arm's
       firm triggers (Q199), the GO's buzz and the button names in A's
       refusals are A's -- not the pad the focus tools read. */
    m_stub->m_lineDeviceKind.insert(0, "dualsense");
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    m_stub->m_feedbackParams.clear();
    m_stub->m_feedbackLines.clear();
    arm(true, 0);
    const PadTriggers::State armed = PadTriggers::forState(true, true, false);
    const QList<QVariantMap> onA = feedbackOn(0, "triggers");
    QVERIFY2(!onA.isEmpty(), "the runner was never told its triggers");
    QCOMPARE(onA.last().value("right").toString(), QString::fromLatin1(PadTriggers::name(armed.right)));
    QCOMPARE(onA.last().value("left").toString(), QString::fromLatin1(PadTriggers::name(armed.left)));
    const QList<QVariantMap> onB = feedbackOn(1, "triggers");
    QVERIFY2(onB.isEmpty() || onB.last().value("right").toString() != QString::fromLatin1(PadTriggers::name(armed.right))
                 || armed.right == PadTriggers::State().right,
             "the focus pad got the runner's armed triggers");
    // A's GO: the buzz on A, none on B.
    const int rumblesA = int(feedbackOn(0, "rumble").size()), rumblesB = int(feedbackOn(1, "rumble").size());
    withLt(kA, 0);
    QCOMPARE(goes(), 1);
    QVERIFY2(feedbackOn(0, "rumble").size() > rumblesA, "the GO's buzz did not go to the runner");
    QCOMPARE(int(feedbackOn(1, "rumble").size()), rumblesB);
    // A's refusal in A's own names.
    press(kA, 0);
    QCOMPARE(run->notice(), QString("GO is L2 held and Cross."));
    // The stop: every pad buzzes.
    const int stopA = int(feedbackOn(0, "rumble").size()), stopB = int(feedbackOn(1, "rumble").size());
    for (quint32 ch : { kLB, kRB, kLT, kRT })
        axis(ch, 255, 1);
    QVERIFY(feedbackOn(0, "rumble").size() > stopA);
    QVERIFY(feedbackOn(1, "rumble").size() > stopB);
    for (quint32 ch : { kRT, kLT, kRB, kLB })
        axis(ch, 0, 1);
}

void PadRunRules_Test::reviewALaptopToolDisarmsABesideRunner()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    arm(true, 0);
    ltMenu(1);
    QCOMPARE(run->state(), PadRun::Focus);
    QVERIFY(run->isArmed());
    const int rumblesA = int(feedbackOn(0, "rumble").size());
    // The laptop's Focus this cue (it ends the venue focus): A disarmed.
    RefineSession *r = pc->refineSession();
    r->setRunningCueProbe([this](quint32) { return m_running; });
    QString why;
    QVERIFY2(r->start(m_cue1, 0xFFFFFFFF, &why), qPrintable(why));
    QVERIFY(!pc->focusSession()->isActive());
    QVERIFY2(!run->isArmed(), "a tool took the pad and the runner is still armed");
    QCOMPARE(run->notice(), QString("Disarmed: Focus This Cue has the controller."));
    for (const QVariantMap &m : feedbackOn(0, "rumble").mid(rumblesA))
        QVERIFY2(m.value("pulses").toInt() != 2, "told GO works again as a tool took the pad");
    r->stop();
    QVERIFY(!run->isArmed());
    withLt(kA, 0);
    QCOMPARE(goes(), 0);
}

void PadRunRules_Test::reviewTheControllerTestDisarmsABesideRunner()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    arm(true, 0);
    ltMenu(1);
    QCOMPARE(run->state(), PadRun::Focus);
    run->setTesting(true);
    QVERIFY2(!run->isArmed(), "the Controller Test opened and the runner is still armed");
    QCOMPARE(run->notice(), QString("Disarmed: Controller Test has the controller."));
    run->setTesting(false);
    ltMenu(1);
    QVERIFY(!pc->focusSession()->isActive());
    QVERIFY(!run->isArmed());
    withLt(kA, 0);
    QCOMPARE(goes(), 0);
}

void PadRunRules_Test::reviewTheFourSurviveAToolTakingThePad()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    // A holds three of the four; B's venue focus takes the pad meanwhile.
    for (quint32 ch : { kLB, kRB, kLT })
        axis(ch, 255, 0);
    ltMenu(1);
    QCOMPARE(run->state(), PadRun::Focus);
    axis(kRT, 255, 0);
    QCOMPARE(stops(), 1);
    QCOMPARE(run->state(), PadRun::Stopped);
    for (quint32 ch : { kRT, kLT, kRB, kLB })
        axis(ch, 0, 0);
    // And the Controller Test opening mid-hold.
    for (quint32 ch : { kLB, kRB, kLT })
        axis(ch, 255, 0);
    run->setTesting(true);
    axis(kRT, 255, 0);
    QCOMPARE(stops(), 2);
    for (quint32 ch : { kRT, kLT, kRB, kLB })
        axis(ch, 0, 0);
    run->setTesting(false);
}

void PadRunRules_Test::reviewALaptopStartBesideARunnerWithoutFocus()
{
    QTemporaryDir dir;
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    pc->venueCorrections()->setShowFile(dir.filePath("Spring.qlcc"), false);
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    run->setEnabled(true);
    arm(true, 0);
    // The laptop starts it: A can't work a focus, so its arm stays.
    QVERIFY(fcc->start());
    QCOMPARE(fcc->state(), FocusCallCue::NamingVenue);
    QCOMPARE(run->state(), PadRun::Focus);
    QVERIFY(run->isArmed());
    QCOMPARE(run->armedUniverse(), quint32(0));
    QCOMPARE(run->roleText(), QString("Pad: FOCUS"));
    QVERIFY(run->lightUniverse() != 0);
    withLt(kA, 0);
    QCOMPARE(goes(), 0);
    QCOMPARE(run->notice(), QString("Venue focus is running: nothing plays until it ends."));
    // The calls run; B's first press takes it: FOCUS names B, B's light.
    QVERIFY(fcc->useVenue(QString("Hall")));
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    press(kY, 1);
    QCOMPARE(pc->controllerOwner(), quint32(1));
    QCOMPARE(run->roleText(), QString::fromUtf8("Pad: FOCUS · Pad B (…00:02)"));
    QCOMPARE(lightOn(1), PadRun::focusColour().name());
    QCOMPARE(lightOn(0), QString());
    ltMenu(1);
    QVERIFY(!pc->focusSession()->isActive());
    QVERIFY(run->isArmed());
    withLt(kA, 0);
    QCOMPARE(goes(), 1);
}

void PadRunRules_Test::reviewNoVenueFocusFromAStoppedShow()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    run->setEnabled(true);
    arm(true, 0);
    for (quint32 ch : { kLB, kRB, kLT, kRT })
        axis(ch, 255, 0);
    for (quint32 ch : { kRT, kLT, kRB, kLB })
        axis(ch, 0, 0);
    QCOMPARE(run->state(), PadRun::Stopped);
    ltMenu(1);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCOMPARE(run->state(), PadRun::Stopped);
    QCOMPARE(run->notice(), QString("Emergency stop: arm a pad (or GO on the computer) before the venue focus."));
    // Armed again, the stop is over: B starts it.
    arm(true, 0);
    ltMenu(1);
    QCOMPARE(fcc->state(), FocusCallCue::Running);
}

void PadRunRules_Test::reviewOneBuzzAtTheFocusEnd()
{
    QTemporaryDir dir;
    makeShow();
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    arm(true, 0);
    ltMenu(0);
    QCOMPARE(run->state(), PadRun::Focus);
    QSignalSpy rumbled(pc, &ProgrammerController::padRumbled);
    ltMenu(0);
    QCOMPARE(run->state(), PadRun::Armed);
    int twos = 0;
    for (const QList<QVariant> &r : rumbled)
        twos += r.at(0).toInt() == 2 ? 1 : 0;
    QCOMPARE(twos, 1);
    // A start given up (the venue asked): one, too.
    pc->venueCorrections()->setShowFile(dir.filePath("Spring.qlcc"), false);
    focusCueAgain();
    ltMenu(0);
    QCOMPARE(pc->focusCallCue()->state(), FocusCallCue::NamingVenue);
    rumbled.clear();
    ltMenu(0);
    QCOMPARE(pc->focusCallCue()->state(), FocusCallCue::Armed);
    QVERIFY(run->isArmed());
    twos = 0;
    for (const QList<QVariant> &r : rumbled)
        twos += r.at(0).toInt() == 2 ? 1 : 0;
    QCOMPARE(twos, 1);
}

void PadRunRules_Test::reviewAViewLetGoInFocusIsLetGo()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    arm(true, 0);
    hold(kView, true, 0);
    ltMenu(1);
    QCOMPARE(run->state(), PadRun::Focus);
    hold(kView, false, 0);
    ltMenu(1);
    QVERIFY(!pc->focusSession()->isActive());
    // Menu alone on A is Menu (A has no Focus: said), not View + Menu.
    run->showNotice(QString());
    press(kMenu, 0);
    QVERIFY(!run->isRoleChordPending());
    QCOMPARE(run->notice(), QString("This controller doesn't do focus."));
}

void PadRunRules_Test::reviewTheFocusJobTakenMidFocus_data()
{
    QTest::addColumn<bool>("runnerHasFocus");
    QTest::newRow("the runner without Focus") << false;
    QTest::newRow("the runner with Focus") << true;
}

void PadRunRules_Test::reviewTheFocusJobTakenMidFocus()
{
    /* B's Focus job taken while its venue focus runs: the focus runs on
       with no pad, said; only a pad with Focus takes it, and the runner's
       arm is put by only when the runner can work it. */
    QFETCH(bool, runnerHasFocus);
    makeShow();
    twoPads(int(runnerHasFocus ? CJ::RunCues | CJ::Focus : CJ::RunCues), int(CJ::Focus));
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    ControllerJobs *jobs = pc->controllerJobs();
    run->setEnabled(true);
    arm(true, 0);
    ltMenu(1);
    QCOMPARE(pc->controllerOwner(), quint32(1));
    QVERIFY(run->isArmed());
    jobs->setJob(jobs->identityOfUniverse(1), CJ::Focus, false);
    QVERIFY(pc->focusSession()->isActive());
    QCOMPARE(pc->controllerOwner(), quint32(0xFFFFFFFF));
    QCOMPARE(run->state(), PadRun::Focus);
    QCOMPARE(run->notice(), QString("Venue focus has no controller now: a controller with the Focus job takes it "
                                    "with a press, or finish it on the laptop."));
    // B can't take it back.
    press(kY, 1);
    QCOMPARE(pc->controllerOwner(), quint32(0xFFFFFFFF));
    if (runnerHasFocus)
    {
        // A can work it: its arm is put by, its press takes the focus.
        QVERIFY(!run->isArmed());
        press(kY, 0);
        QCOMPARE(pc->controllerOwner(), quint32(0));
        QCOMPARE(run->roleText(), QString::fromUtf8("Pad: FOCUS · Pad A (…00:01)"));
        ltMenu(0);
    }
    else
    {
        // A can't: armed beside it, nothing plays, the laptop ends it.
        QVERIFY(run->isArmed());
        withLt(kA, 0);
        QCOMPARE(goes(), 0);
        QCOMPARE(run->notice(), QString("Venue focus is running: nothing plays until it ends."));
        pc->focusSession()->finish();
    }
    QVERIFY(!pc->focusSession()->isActive());
    QVERIFY(run->isArmed());
    QCOMPARE(run->armedUniverse(), quint32(0));
    withLt(kA, 0);
    QCOMPARE(goes(), 1);
}

void PadRunRules_Test::reviewTheFocusPadUnpluggedMidFocus_data()
{
    QTest::addColumn<bool>("runnerHasFocus");
    QTest::newRow("the runner without Focus") << false;
    QTest::newRow("the runner with Focus") << true;
}

void PadRunRules_Test::reviewTheFocusPadUnpluggedMidFocus()
{
    QFETCH(bool, runnerHasFocus);
    makeShow();
    twoPads(int(runnerHasFocus ? CJ::RunCues | CJ::Focus : CJ::RunCues), int(CJ::Focus));
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    arm(true, 0);
    ltMenu(1);
    QCOMPARE(pc->controllerOwner(), quint32(1));
    drop(1);
    QCOMPARE(pc->controllerOwner(), quint32(0xFFFFFFFF));
    // A runner that can work it has its arm put by; one that can't keeps it.
    QCOMPARE(run->isArmed(), !runnerHasFocus);
    QVERIFY(run->notice().startsWith("Venue focus has no controller now"));
    // FOCUS names no pad while none has it (the notice says so too).
    QCOMPARE(run->roleText(), QString("Pad: FOCUS"));
    // Back: B, with Focus, takes it again with a press, and ends it.
    back(1);
    press(kY, 1);
    QCOMPARE(pc->controllerOwner(), quint32(1));
    QCOMPARE(run->roleText(), QString::fromUtf8("Pad: FOCUS · Pad B (…00:02)"));
    ltMenu(1);
    QVERIFY(!pc->focusSession()->isActive());
    QVERIFY(run->isArmed());
    QCOMPARE(run->armedUniverse(), quint32(0));
    withLt(kA, 0);
    QCOMPARE(goes(), 1);
}

/*****************************************************************************
 * Board Q252: a walk beside the runner
 *****************************************************************************/

void PadRunRules_Test::q252AWalkBesideTheRunnerInDesign()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    arm(true, 0);
    // B's Menu in Design: B walks; A stays armed.
    press(kMenu, 1);
    QVERIFY2(run->isWalking(), qPrintable(run->notice()));
    QVERIFY(run->isArmed());
    QCOMPARE(run->armedUniverse(), quint32(0));
    QCOMPARE(pc->controllerOwner(), quint32(1));
    QCOMPARE(run->roleText(), QString::fromUtf8("Pad: Walk · Pad B (…00:02)"));
    // A: nothing plays while the walk runs, on the screen.
    const QString why = QString::fromUtf8("Pad B (…00:02) is walking the show: nothing plays until the walk ends.");
    for (quint32 ch : { kA, kB, kX, kY })
    {
        run->showNotice(QString());
        withLt(ch, 0);
        QVERIFY2(run->notice() == why, qPrintable(QString("LT + %1: '%2'").arg(ch).arg(run->notice())));
    }
    run->showNotice(QString());
    press(kRT, 0);
    QCOMPARE(run->notice(), why);
    hold(kLB, true, 0);
    hold(kRB, true, 0);
    QTRY_VERIFY_WITH_TIMEOUT(!run->isArmChordPending(), 5000);
    hold(kRB, false, 0);
    hold(kLB, false, 0);
    QVERIFY(run->isArmed());
    QCOMPARE(goes(), 0);
    QVERIFY(run->isWalking());
    // The walk's feedback (its triggers) is B's.
    QCOMPARE(pc->statePad(), quint32(1));
    // The walk ends (B's Menu): A's GO works, no re-arm, one double buzz on A.
    const int rumblesA = int(feedbackOn(0, "rumble").size());
    press(kMenu, 1);
    QTRY_VERIFY(!run->isWalking());
    QCOMPARE(run->state(), PadRun::Armed);
    QCOMPARE(run->notice(), QString());
    QTRY_VERIFY(feedbackOn(0, "rumble").size() > rumblesA);
    int twos = 0;
    for (const QVariantMap &m : feedbackOn(0, "rumble").mid(rumblesA))
        twos += m.value("pulses").toInt() == 2 ? 1 : 0;
    QCOMPARE(twos, 1);
    withLt(kA, 0);
    QCOMPARE(goes(), 1);
}

void PadRunRules_Test::q252NoWalkBesideTheRunnerInOperate()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    m_doc->setMode(Doc::Operate);
    arm(true, 0);
    press(kMenu, 1);
    QVERIFY(!run->isWalking());
    QVERIFY(run->isArmed());
    QVERIFY2(run->notice().contains("in Operate the walk waits until it disarms"), qPrintable(run->notice()));
    withLt(kA, 0);
    QCOMPARE(goes(), 1);
    m_doc->setMode(Doc::Design);
}

void PadRunRules_Test::q252TheStopDuringABesideWalk_data()
{
    QTest::addColumn<int>("stopper");
    QTest::newRow("the runner") << 0;
    QTest::newRow("the walker") << 1;
}

void PadRunRules_Test::q252TheStopDuringABesideWalk()
{
    QFETCH(int, stopper);
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    arm(true, 0);
    press(kMenu, 1);
    QVERIFY2(run->isWalking(), qPrintable(run->notice()));
    for (quint32 ch : { kLB, kRB, kLT, kRT })
        axis(ch, 255, quint32(stopper));
    QCOMPARE(stops(), 1);
    QCOMPARE(run->state(), PadRun::Stopped);
    QVERIFY(!run->isArmed());
    for (quint32 ch : { kRT, kLT, kRB, kLB })
        axis(ch, 0, quint32(stopper));
    if (run->isWalking())
        run->stopWalk();
    QVERIFY(!run->isArmed());
}

void PadRunRules_Test::q252ALaptopToolMidWalkDisarmsTheRunner()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    // The Controller Test opened mid-walk.
    arm(true, 0);
    press(kMenu, 1);
    QVERIFY2(run->isWalking(), qPrintable(run->notice()));
    run->setTesting(true);
    QVERIFY2(!run->isArmed(), "the Controller Test opened and the runner is still armed");
    QCOMPARE(run->notice(), QString("Disarmed: Controller Test has the controller."));
    run->setTesting(false);
    if (run->isWalking())
        run->stopWalk();
    QTRY_VERIFY(!run->isWalking());
    // The laptop's Focus this cue on another cue mid-walk.
    arm(true, 0);
    press(kMenu, 1);
    QVERIFY2(run->isWalking(), qPrintable(run->notice()));
    const int rumblesA = int(feedbackOn(0, "rumble").size());
    QString why;
    QVERIFY2(pc->refineSession()->start(m_cue2, 0xFFFFFFFF, &why), qPrintable(why));
    QCoreApplication::processEvents();
    QVERIFY2(!run->isArmed(), "the laptop's Focus this cue took the pad and the runner is still armed");
    for (const QVariantMap &m : feedbackOn(0, "rumble").mid(rumblesA))
        QVERIFY2(m.value("pulses").toInt() != 2, "told GO works again as a tool took the pad");
    pc->refineSession()->stop();
    withLt(kA, 0);
    QCOMPARE(goes(), 0);
}

/*****************************************************************************
 * Second review of 46cfdd708
 *****************************************************************************/

void PadRunRules_Test::review2OperateMidWalkEndsABesideWalk()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    arm(true, 0);
    press(kMenu, 1);
    QVERIFY2(run->isWalking(), qPrintable(run->notice()));
    // The laptop switches to Operate: the walk beside the runner ends.
    m_doc->setMode(Doc::Operate);
    QVERIFY2(!run->isWalking(), "a walk beside the runner went on into Operate");
    QVERIFY(run->isArmed());
    QCOMPARE(run->armedUniverse(), quint32(0));
    QCOMPARE(run->notice(), QString::fromUtf8("Walk ended: the show is in Operate and Pad A (…00:01) runs cues."));
    withLt(kA, 0);
    QCOMPARE(goes(), 1);
    m_doc->setMode(Doc::Design);
}

void PadRunRules_Test::review2TheWalkerLosesFocusMidWalk()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    ControllerJobs *jobs = pc->controllerJobs();
    run->setEnabled(true);
    arm(true, 0);
    press(kMenu, 1);
    QVERIFY2(run->isWalking(), qPrintable(run->notice()));
    jobs->setJob(jobs->identityOfUniverse(1), CJ::Focus, false);
    QVERIFY2(!run->isWalking(), "the walker lost Focus and its walk went on");
    QVERIFY(run->isArmed());
    QCOMPARE(run->notice(), QString::fromUtf8("Walk ended: Pad B (…00:02) no longer does focus."));
    withLt(kA, 0);
    QCOMPARE(goes(), 1);
}

void PadRunRules_Test::review2TheWalkerUnpluggedMidWalk()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    arm(true, 0);
    press(kMenu, 1);
    QVERIFY2(run->isWalking(), qPrintable(run->notice()));
    drop(1);
    QVERIFY2(!run->isWalking(), "the walker went away and its walk went on");
    QVERIFY(run->isArmed());
    QCOMPARE(run->notice(), QString("Walk ended: the controller walking it went away."));
    withLt(kA, 0);
    QCOMPARE(goes(), 1);
}

void PadRunRules_Test::review2ARunnersSwipeBesideAWalk()
{
    const quint32 kTouchX = 30, kTouchY = 31, kTouchDown = 32;
    m_stub->m_channelRoles.insert(kTouchX, "touch_x");
    m_stub->m_channelRoles.insert(kTouchY, "touch_y");
    m_stub->m_channelRoles.insert(kTouchDown, "touch_down");
    makeShow();
    twoPads(int(CJ::RunCues | CJ::Focus), int(CJ::Focus));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    arm(true, 0);
    press(kMenu, 1);
    QVERIFY2(run->isWalking(), qPrintable(run->notice()));
    const quint32 cue = pc->refineSession()->cueId();
    run->showNotice(QString());
    // A swipes across its touchpad: no step, said as its other presses are.
    for (int i = 0; i <= 10; ++i)
    {
        axis(kTouchX, uchar((0.2f + 0.06f * i) * 255.0f), 0);
        axis(kTouchY, uchar(0.5f * 255.0f), 0);
        if (i == 0)
            axis(kTouchDown, 255, 0);
        run->swipeTick(0.02);
    }
    axis(kTouchDown, 0, 0);
    run->swipeTick(0.02);
    run->swipeTick(1.0);
    QCOMPARE(pc->refineSession()->cueId(), cue);
    QCOMPARE(run->notice(), QString::fromUtf8("Pad B (…00:02) is walking the show: nothing plays until the walk ends."));
    run->stopWalk();
}

void PadRunRules_Test::review2ARoleSwitchEndsABesideWalk()
{
    makeShow();
    twoPads(int(CJ::RunCues), int(CJ::Focus));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    arm(true, 0);
    press(kMenu, 1);
    QVERIFY2(run->isWalking(), qPrintable(run->notice()));
    run->setEnabled(false);
    QVERIFY2(!run->isWalking(), "a walk beside the runner went on into the Tools role");
    QVERIFY(!m_doc->programmer()->refineSession()->isActive());
}

void PadRunRules_Test::jobsTakenAwayWhileInUse()
{
    makeShow();
    twoPads(int(CJ::AllJobs), int(CJ::AllJobs));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    ControllerJobs *jobs = pc->controllerJobs();
    run->setEnabled(true);
    arm(true, 0);
    // The runner's Run cues taken away: disarmed, with no button, said.
    jobs->setJob(jobs->identityOfUniverse(0), CJ::RunCues, false);
    QVERIFY(!run->isArmed());
    QCOMPARE(run->notice(), QString("Disarmed: this controller no longer runs cues."));
    QCOMPARE(goes(), 0);
    // Now B may arm; A may not.
    arm(true, 1);
    QCOMPARE(run->armedUniverse(), quint32(1));
    arm(false, 1);
    // The focus's pad loses Focus: the tool is free for another (Q135).
    run->setEnabled(false);
    QVERIFY(pc->refineSession()->start(m_cue1, 0xFFFFFFFF, nullptr));
    press(kY, 0);
    QCOMPARE(pc->controllerOwner(), quint32(0));
    jobs->setJob(jobs->identityOfUniverse(0), CJ::Focus, false);
    QCOMPARE(pc->controllerOwner(), quint32(0xFFFFFFFF));
    press(kY, 1);
    QCOMPARE(pc->controllerOwner(), quint32(1));
    pc->refineSession()->stop();
}

void PadRunRules_Test::reviewTheSamePadTwiceIsToldOneLight()
{
    /* macOS lists one DualSense as two HID devices, both patched (as
       theDuplicateDevice): one address, so one controller by its identity. */
    makeShow();
    m_stub->m_lineNames = QStringList() << "Pad (AA:BB:CC:00:00:01)" << "Pad (AA:BB:CC:00:00:01)";
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 1));
    ControllerJobs *jobs = m_doc->programmer()->controllerJobs();
    QCOMPARE(jobs->identityOfUniverse(0), jobs->identityOfUniverse(1));
    PadRun *run = m_doc->programmer()->padRun();
    QVERIFY(run->sameController(0, 1));
    run->setEnabled(true);
    run->setArmChordMs(15);
    auto both = [&](quint32 ch, uchar v) {
        axis(ch, v, 0);
        axis(ch, v, 1);
    };
    both(kLB, 255);
    both(kRB, 255);
    QTRY_VERIFY(run->isArmed());
    both(kRB, 0);
    both(kLB, 0);
    // One physical pad: its two lines must not be told two different lights.
    QVERIFY2(lightOn(0) == lightOn(1),
             qPrintable(QString("line 0 told \"%1\", line 1 told \"%2\" (the armed colour is %3); the pad "
                                "shows whichever came last").arg(lightOn(0), lightOn(1),
                                                                 PadRun::armedColour().name())));
    // And the chip treats it as one pad (one pad: no runner's name).
    QCOMPARE(run->roleText(), QString("Pad: ARMED"));
}

QTEST_MAIN(PadRunRules_Test)

/*
  Q Light Controller Plus
  padrun_test.cpp

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

#include "mastertimer.h"

#include <QtTest>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QSemaphore>
#include <QThread>

#include <climits>
#include <cstdio>
#include <cstdlib>

#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "iopluginstub.h"
#include "qlcioplugin.h"
#include "showcontrol.h"
#include "padrun_test.h"
#include "controlmap.h"
#include "inputpatch.h"
#include "padnames.h"
#include "padrun.h"
#include "controllerjobs.h"
#include "padtriggers.h"
#include "touchswipe.h"
#include "chaser.h"
#include "function.h"
#include "functionparent.h"
#include "qlcfile.h"
#include "doc.h"
#include "refinesession.h"
#include "showtreemodel.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "monitorproperties.h"
#include "venuecorrections.h"
#include "stagetarget.h"
#include "focuscallcue.h"
#include "focuscalls.h"
#include "focussession.h"
#include "fixturegroup.h"
#include "qlcpoint.h"

namespace
{
/* An Xbox pad as its profile describes it: axes, then buttons. */
const quint32 kLX = 0, kLT = 4, kRT = 5, kA = 10, kB = 11, kX = 12, kY = 13, kLB = 14, kRB = 15,
              kView = 16, kMenu = 17, kRS = 19, kUp = 20,
              kTouchX = 30, kTouchY = 31, kTouchDown = 32, kTouch2Down = 33, kTouchClick = 34;

QMap<quint32, QString> xboxRoles()
{
    return { { 0, "lx" }, { 1, "ly" }, { 2, "rx" }, { 3, "ry" }, { kLT, "lt" }, { kRT, "rt" },
             { kA, "a" }, { kB, "b" }, { kX, "x" }, { kY, "y" }, { kLB, "lb" }, { kRB, "rb" },
             { kView, "back" }, { kMenu, "start" }, { kRS, "rs" }, { kUp, "dpad_up" },
             { kTouchX, "touch_x" }, { kTouchY, "touch_y" }, { kTouchDown, "touch_down" },
             { kTouch2Down, "touch2_down" }, { kTouchClick, "touchpad" } };
}

/* A spot: pan, pan fine, tilt, tilt fine, dimmer, zoom (10-40 degrees). */
Fixture *makeSpot(Doc *doc, const QString &name)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Walk Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("6ch");
    struct Ch { const char *name; QLCChannel::Group group; QLCChannel::ControlByte byte; QLCChannel::Preset preset; };
    const Ch chans[6] = {
        { "Pan", QLCChannel::Pan, QLCChannel::MSB, QLCChannel::Custom },
        { "Pan fine", QLCChannel::Pan, QLCChannel::LSB, QLCChannel::Custom },
        { "Tilt", QLCChannel::Tilt, QLCChannel::MSB, QLCChannel::Custom },
        { "Tilt fine", QLCChannel::Tilt, QLCChannel::LSB, QLCChannel::Custom },
        { "Dimmer", QLCChannel::Intensity, QLCChannel::MSB, QLCChannel::Custom },
        { "Zoom", QLCChannel::Beam, QLCChannel::MSB, QLCChannel::BeamZoomSmallBig },
    };
    QLCFixtureHead head;
    for (int k = 0; k < 6; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(chans[k].name);
        ch->setGroup(chans[k].group);
        ch->setControlByte(chans[k].byte);
        if (chans[k].preset != QLCChannel::Custom)
            ch->setPreset(chans[k].preset);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
        head.addChannel(k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    ph.setLensDegreesMin(10.0);
    ph.setLensDegreesMax(40.0);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(quint32(doc->fixtures().size() * 6));
    doc->addFixture(fxi);
    return fxi;
}

ShowTree::Look lookOf(const QString &key, quint32 palette)
{
    ShowTree::Look l;
    l.key = key;
    l.palettes = { palette };
    return l;
}

QByteArray showXml(Doc *doc)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.writeStartDocument();
    doc->saveXML(&w);
    w.writeEndDocument();
    return xml;
}
}

void PadRun_Test::makeShow()
{
    m_fronts.clear();
    m_backs.clear();
    for (int i = 0; i < 2; ++i)
        m_fronts << makeSpot(m_doc, QString("Front %1").arg(i + 1))->id();
    for (int i = 0; i < 2; ++i)
        m_backs << makeSpot(m_doc, QString("Back %1").arg(i + 1))->id();
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer);
    full->setName("Full");
    full->setValue(255);
    m_doc->addPalette(full);

    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Spring");
    m_list = st->addCueList(prod, "Main", ShowTree::ListMode::NonTracking);
    m_cue1 = st->addCue(m_list, -1, "Opening");
    m_empty = st->addCue(m_list, 0, "Blackout");      // nothing to walk
    m_cue2 = st->addCue(m_list, 1, "Ballad");
    m_frontTarget = ShowTree::Target();
    m_frontTarget.fixtures = m_fronts;
    m_backTarget = ShowTree::Target();
    m_backTarget.fixtures = m_backs;
    for (quint32 cue : { m_cue1, m_cue2 })
    {
        const quint32 front = st->addLayer(cue, m_frontTarget, "Fronts");
        const quint32 back = st->addLayer(cue, m_backTarget, "Backs");
        QVERIFY(st->setLayerLook(cue, front, lookOf(ShowTree::KeyDimmer, full->id())));
        QVERIFY(st->setLayerLook(cue, back, lookOf(ShowTree::KeyDimmer, full->id())));
        if (cue == m_cue1)
        {
            m_layer1 = front;
            m_layer2 = back;
        }
    }
    m_doc->programmer()->refineSession()->setFlashEnabled(false);
    st->pickCue(m_cue1);
}

void PadRun_Test::walkMapByRole()
{
    QCOMPARE(PadRun::walkActionFor(GamepadRole::RightTrigger), PadRun::NextCue);
    QCOMPARE(PadRun::walkActionFor(GamepadRole::LeftTrigger), PadRun::PreviousCue);
    QCOMPARE(PadRun::walkActionFor(GamepadRole::RB), PadRun::NextLayer);
    QCOMPARE(PadRun::walkActionFor(GamepadRole::LB), PadRun::PreviousLayer);
    QCOMPARE(PadRun::walkActionFor(GamepadRole::Back), PadRun::Solo);
    QCOMPARE(PadRun::walkActionFor(GamepadRole::Y), PadRun::TagCue);
    QCOMPARE(PadRun::walkActionFor(GamepadRole::Start), PadRun::LeaveWalk);
    // The adjusting controls stay Focus this cue's.
    QCOMPARE(PadRun::walkActionFor(GamepadRole::A), PadRun::WalkNoAction);
    QCOMPARE(PadRun::walkActionFor(GamepadRole::DpadUp), PadRun::WalkNoAction);
    for (PadRun::WalkAction a : PadRun::walkActions())
    {
        QCOMPARE(PadRun::walkActionFor(PadRun::roleFor(a)), a);
        QVERIFY(!PadRun::label(a).isEmpty());
    }
    // One map for both pads, in each pad's words.
    const QStringList xbox = PadRun::walkMapLines(PadNames::Xbox);
    const QStringList ps = PadRun::walkMapLines(PadNames::DualSense);
    QCOMPARE(xbox.size(), PadRun::walkActions().size() + 3);
    QCOMPARE(xbox.at(0).left(3), QString("RT:"));
    QCOMPARE(ps.at(0).left(3), QString("R2:"));
    // The modifier, in each pad's words.
    QVERIFY(xbox.join('\n').contains("LB held + RT / LT: level"));
    QVERIFY(ps.join('\n').contains("L1 held + R2 / L2: level"));
    QVERIFY(ps.join('\n').contains("the light itself on its target"));
}

void PadRun_Test::init()
{
    m_doc = new Doc(this);
    m_runs.clear();
    m_answer = true;
    m_doc->showControl()->setRunHandler([this](const ShowControl::Request &r, ShowControl::Route route) {
        m_runs << qMakePair(r, route);
        return m_answer;
    });
}

void PadRun_Test::cleanup()
{
    m_doc->showControl()->setRunHandler(ShowControl::RunHandler());
    delete m_doc;
    m_doc = nullptr;
}

IOPluginStub *PadRun_Test::loadStub()
{
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    if (m_doc->ioPluginCache()->plugins().isEmpty())
        return nullptr;
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = xboxRoles();
    return stub;
}

void PadRun_Test::press(quint32 channel, quint32 universe)
{
    emit m_doc->inputOutputMap()->inputValueChanged(universe, channel, 255, QString());
    emit m_doc->inputOutputMap()->inputValueChanged(universe, channel, 0, QString());
}

void PadRun_Test::axis(quint32 channel, uchar value, quint32 universe)
{
    emit m_doc->inputOutputMap()->inputValueChanged(universe, channel, value, QString());
}

void PadRun_Test::hold(quint32 channel, bool down, quint32 universe)
{
    emit m_doc->inputOutputMap()->inputValueChanged(universe, channel, down ? 255 : 0, QString());
}

void PadRun_Test::arm(bool on, quint32 universe)
{
    PadRun *run = m_doc->programmer()->padRun();
    if (run->isArmed() == on)
        return;
    run->setArmChordMs(15);
    // The chord: both bumpers held, then let go -- the arm stays as it fell.
    hold(kLB, true, universe);
    hold(kRB, true, universe);
    QTRY_COMPARE(run->isArmed(), on);
    hold(kRB, false, universe);
    hold(kLB, false, universe);
    QCOMPARE(run->isArmed(), on);
}

void PadRun_Test::with(quint32 trigger, quint32 button, quint32 universe)
{
    axis(trigger, 255, universe);
    press(button, universe);
    axis(trigger, 0, universe);
}

void PadRun_Test::theMapByRole()
{
    // The bumpers are the mode only: together, the arm.
    QCOMPARE(PadRun::actionFor(GamepadRole::LB), PadRun::Arm);
    QCOMPARE(PadRun::actionFor(GamepadRole::RB), PadRun::Arm);
    QCOMPARE(PadRun::roleFor(PadRun::Arm), GamepadRole::LB);
    // LT is the safety; the face buttons are the cue commands; RT is the hit.
    QCOMPARE(PadRun::actionFor(GamepadRole::LeftTrigger), PadRun::Safety);
    QCOMPARE(PadRun::safetyRole(), GamepadRole::LeftTrigger);
    QCOMPARE(PadRun::actionFor(GamepadRole::A), PadRun::Go);
    QCOMPARE(PadRun::actionFor(GamepadRole::B), PadRun::Back);
    QCOMPARE(PadRun::actionFor(GamepadRole::X), PadRun::SubGo);
    QCOMPARE(PadRun::actionFor(GamepadRole::Y), PadRun::SubBack);
    QCOMPARE(PadRun::actionFor(GamepadRole::RightTrigger), PadRun::FireHit);
    QCOMPARE(PadRun::actionFor(GamepadRole::RS), PadRun::WhereAmI);
    QCOMPARE(PadRun::actionFor(GamepadRole::LeftX), PadRun::NoAction);
    QCOMPARE(PadRun::actionFor(GamepadRole::Start), PadRun::Walk);
    for (PadRun::Action a : PadRun::actions())
    {
        QCOMPARE(PadRun::actionFor(PadRun::roleFor(a)), a);
        QVERIFY(!PadRun::label(a).isEmpty());
    }
    // The emergency stop is four controls: no one role's.
    QVERIFY(!PadRun::actions().contains(PadRun::EmergencyStop));
    QCOMPARE(PadRun::roleFor(PadRun::EmergencyStop), GamepadRole::None);

    // Every cue command needs the arm and the safety; the hit, the arm.
    for (PadRun::Action a : { PadRun::Go, PadRun::Back, PadRun::SubGo, PadRun::SubBack })
    {
        QVERIFY(PadRun::needsArm(a));
        QVERIFY(PadRun::needsSafety(a));
    }
    QVERIFY(PadRun::needsArm(PadRun::FireHit));
    QVERIFY(!PadRun::needsSafety(PadRun::FireHit));
    for (PadRun::Action a : { PadRun::Arm, PadRun::Safety, PadRun::WhereAmI, PadRun::Walk })
        QVERIFY(!PadRun::needsArm(a) && !PadRun::needsSafety(a));
    QCOMPARE(PadRun::commandFor(PadRun::Back), ShowControl::Back);
    QCOMPARE(PadRun::commandFor(PadRun::SubGo), ShowControl::SubGo);
    QCOMPARE(PadRun::commandFor(PadRun::SubBack), ShowControl::SubBack);
    QCOMPARE(PadRun::commandFor(PadRun::EmergencyStop), ShowControl::AllOff);

    // The map in the pad's own words, the emergency stop last.
    const QStringList xbox = PadRun::mapLines(PadNames::Xbox);
    const QStringList ps = PadRun::mapLines(PadNames::DualSense);
    QCOMPARE(xbox.size(), PadRun::actions().size() + 1);
    QCOMPARE(xbox.at(0), QString("LB: LB + RB held: arm, and the same to disarm"));
    QCOMPARE(ps.at(0), QString("L1: L1 + R1 held: arm, and the same to disarm"));
    QVERIFY(xbox.contains(QString("LT: The safety: hold for GO, Back, Sub GO and Sub Back")));
    QVERIFY(xbox.contains(QString("A: GO (armed, LT held)")));
    QVERIFY(xbox.contains(QString("B: Back (armed, LT held)")));
    QVERIFY(xbox.contains(QString("X: Sub GO (armed, LT held)")));
    QVERIFY(xbox.contains(QString("Y: Sub Back (armed, LT held)")));
    QVERIFY(xbox.contains(QString("RT: Fire the armed hit (armed)")));
    QVERIFY(ps.contains(QString("Cross: GO (armed, L2 held)")));
    QVERIFY(ps.contains(QString("Square: Sub GO (armed, L2 held)")));
    QCOMPARE(xbox.last(), QString("Emergency stop, everything off: LB + RB + LT + RT"));
    QCOMPARE(ps.last(), QString("Emergency stop, everything off: L1 + R1 + L2 + R2"));
    // No line picks a list, and none stops one: the screen does both.
    for (const QString &l : xbox + ps)
        QVERIFY2(!l.contains("cue list", Qt::CaseInsensitive) && !l.contains("stop the list"), qPrintable(l));
}

void PadRun_Test::theArmIsATwoButtonLatch()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    // Every button and trigger the Xbox profile has, by its role.
    QMap<quint32, QString> roles = xboxRoles();
    const QMap<quint32, QString> more = {
        { 40, "ls" }, { 41, "guide" }, { 42, "dpad_down" }, { 43, "dpad_left" }, { 44, "dpad_right" } };
    for (auto it = more.begin(); it != more.end(); ++it)
        roles.insert(it.key(), it.value());
    stub->m_channelRoles = roles;
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    run->setArmChordMs(40);
    QSignalSpy armed(run, &PadRun::armedChanged);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QCOMPARE(run->state(), PadRun::Run);

    // One bumper, held as long as you like: nothing.
    for (quint32 one : { kLB, kRB })
    {
        hold(one, true);
        QVERIFY(!run->isArmChordPending());
        QTest::qWait(90);
        hold(one, false);
        QVERIFY(!run->isArmed());
    }
    // Both, brushed: nothing.
    hold(kLB, true);
    hold(kRB, true);
    QVERIFY(run->isArmChordPending());
    hold(kRB, false);
    hold(kLB, false);
    QVERIFY(!run->isArmChordPending());
    QTest::qWait(90);
    QVERIFY(!run->isArmed());
    // Both with a trigger pulled: nothing.
    axis(kLT, 255);
    hold(kLB, true);
    hold(kRB, true);
    QVERIFY(!run->isArmChordPending());
    QTest::qWait(90);
    hold(kRB, false);
    hold(kLB, false);
    axis(kLT, 0);
    QVERIFY(!run->isArmed());
    QCOMPARE(armed.count(), 0);

    // Both held: armed, said, and it stays armed with everything let go.
    hold(kLB, true);
    hold(kRB, true);
    QTRY_VERIFY(run->isArmed());
    QCOMPARE(armed.count(), 1);
    QCOMPARE(said.last().at(0).toString(), QString("Armed. LT and A is GO."));
    // Still held: it does not flip back.
    QTest::qWait(120);
    QVERIFY(run->isArmed());
    hold(kRB, false);
    hold(kLB, false);
    QVERIFY(run->isArmed());
    QCOMPARE(run->state(), PadRun::Armed);
    QCOMPARE(run->armedUniverse(), quint32(0));

    /* Rule 3: no single control disarms. Every button and trigger, pressed,
       held past the chord's time and let go: still armed. */
    for (auto it = roles.begin(); it != roles.end(); ++it)
    {
        if (it.key() <= 3)
            continue;       // the sticks
        press(it.key());
        QVERIFY2(run->isArmed(), qPrintable(it.value() + " pressed"));
        hold(it.key(), true);
        QTest::qWait(60);
        QVERIFY2(run->isArmed(), qPrintable(it.value() + " held"));
        hold(it.key(), false);
        QVERIFY2(run->isArmed(), qPrintable(it.value() + " let go"));
        QVERIFY2(!run->isWalking(), qPrintable(it.value()));
        QCOMPARE(run->roleText(), QString("Pad: ARMED"));
    }
    QCOMPARE(armed.count(), 1);
    // Nor a trigger with a bumper, nor the two triggers.
    axis(kRT, 255);
    hold(kLB, true);
    QTest::qWait(60);
    hold(kLB, false);
    axis(kLT, 255);
    QTest::qWait(60);
    axis(kLT, 0);
    axis(kRT, 0);
    QVERIFY(run->isArmed());

    // The chord again: disarmed, said.
    hold(kRB, true);
    hold(kLB, true);
    QTRY_VERIFY(!run->isArmed());
    QCOMPARE(armed.count(), 2);
    QCOMPARE(said.last().at(0).toString(), QString("Disarmed."));
    hold(kLB, false);
    hold(kRB, false);
    QVERIFY(!run->isArmed());
    QCOMPARE(run->state(), PadRun::Run);
    QCOMPARE(run->armedUniverse(), quint32(UINT_MAX));

    // Not in the Tools role: the chord arms nothing.
    run->setEnabled(false);
    hold(kLB, true);
    hold(kRB, true);
    QTest::qWait(90);
    hold(kRB, false);
    hold(kLB, false);
    QVERIFY(!run->isArmed());
}

void PadRun_Test::goNeedsTheArmAndItsTrigger()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    QSignalSpy refused(run, &PadRun::refused);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    // Not armed: nothing goes, with the safety or without, and the pad says how.
    press(kA);
    with(kLT, kA);
    QVERIFY(m_runs.isEmpty());
    QCOMPARE(refused.count(), 2);
    QCOMPARE(refused.at(0).at(0).value<PadRun::Action>(), PadRun::Go);
    QCOMPARE(said.last().at(0).toString(), QString("Not armed: hold LB and RB."));
    QCOMPARE(run->notice(), QString("Not armed: hold LB and RB."));

    // Armed, the button alone: nothing, and it says what each is.
    arm(true);
    refused.clear();
    press(kA);
    QVERIFY(m_runs.isEmpty());
    QCOMPARE(refused.count(), 1);
    QCOMPARE(run->notice(), QString("GO is LT held and A."));
    press(kB);
    QCOMPARE(run->notice(), QString("Back is LT held and B."));
    press(kX);
    QCOMPARE(run->notice(), QString("Sub GO is LT held and X."));
    press(kY);
    QCOMPARE(run->notice(), QString("Sub Back is LT held and Y."));
    // The other trigger is no safety, nor is LT only brushed.
    for (quint32 b : { kA, kB, kX, kY })
        with(kRT, b);
    axis(kLT, 100);
    press(kA);
    axis(kLT, 0);
    QVERIFY(m_runs.isEmpty());
    // The safety alone, and the bumpers one at a time with it: nothing.
    axis(kLT, 255);
    press(kLB);
    press(kRB);
    axis(kLT, 0);
    QVERIFY(m_runs.isEmpty());
    QVERIFY(run->isArmed());

    // Armed + LT held + A: one GO, through run() with the pad's route.
    with(kLT, kA);
    QCOMPARE(m_runs.size(), 1);
    QCOMPARE(m_runs.at(0).first.command, ShowControl::Go);
    QCOMPARE(m_runs.at(0).second, ShowControl::RoutePad);
    QCOMPARE(m_runs.at(0).first.chaser, quint32(UINT_MAX));        // the main list
    // The one safety for all four: LT + B Back, + X Sub GO, + Y Sub Back.
    with(kLT, kB);
    with(kLT, kX);
    with(kLT, kY);
    QCOMPARE(m_runs.size(), 4);
    QCOMPARE(m_runs.at(1).first.command, ShowControl::Back);
    QCOMPARE(m_runs.at(2).first.command, ShowControl::SubGo);
    QCOMPARE(m_runs.at(3).first.command, ShowControl::SubBack);
    for (const auto &r : m_runs)
        QCOMPARE(r.second, ShowControl::RoutePad);
    // Held, A twice is two GOs; a release sends nothing.
    axis(kLT, 255);
    press(kA);
    press(kA);
    hold(kA, false);
    axis(kLT, 0);
    QCOMPARE(m_runs.size(), 6);
    // Still armed after all of it.
    QVERIFY(run->isArmed());

    // The arm is one pad's: another's LT + A is refused, and said.
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(1, stub->name(), QString(), 1));
    with(kLT, kA, 1);
    QCOMPARE(m_runs.size(), 6);
    // One runner (controller jobs): the refusal names the pad that runs cues.
    QCOMPARE(run->notice(), QString("1: Stub 1 is running cues."));
    // Nor may it drop the first pad's arm.
    hold(kLB, true, 1);
    hold(kRB, true, 1);
    QTest::qWait(60);
    hold(kRB, false, 1);
    hold(kLB, false, 1);
    QVERIFY(run->isArmed());
    QCOMPARE(run->armedUniverse(), quint32(0));

    // Disarmed: nothing again.
    arm(false);
    with(kLT, kA);
    QCOMPARE(m_runs.size(), 6);
}

void PadRun_Test::theEmergencyStop()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    run->setArmChordMs(200);
    QSignalSpy rumbles(pc, &ProgrammerController::padRumbled);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QSignalSpy states(run, &PadRun::stateChanged);

    // Three of the four: nothing.
    hold(kLB, true);
    axis(kLT, 255);
    axis(kRT, 255);
    QVERIFY(m_runs.isEmpty());
    // The fourth: ALL OFF through run(), not armed and all the same.
    hold(kRB, true);
    QCOMPARE(m_runs.size(), 1);
    QCOMPARE(m_runs.at(0).first.command, ShowControl::AllOff);
    QCOMPARE(m_runs.at(0).second, ShowControl::RoutePad);
    QCOMPARE(run->state(), PadRun::Stopped);
    QCOMPARE(run->roleText(), QString("Pad: STOPPED"));
    QCOMPARE(run->stateColour(), PadRun::stoppedColour());
    QCOMPARE(pc->padLight(), PadRun::stoppedColour());
    QCOMPARE(rumbles.last().at(0).toInt(), 3);
    QCOMPARE(said.last().at(0).toString(), QString("Emergency stop. Everything is off."));
    QCOMPARE(run->notice(), QString("Emergency stop. Everything is off."));
    QCOMPARE(states.count(), 1);
    // Held on: once only, and the bumpers in it arm nothing.
    QTest::qWait(260);
    QCOMPARE(m_runs.size(), 1);
    QVERIFY(!run->isArmed());
    // One let go and pulled again: a new chord, a new stop.
    axis(kRT, 0);
    axis(kRT, 255);
    QCOMPARE(m_runs.size(), 2);
    hold(kRB, false);
    hold(kLB, false);
    axis(kLT, 0);
    axis(kRT, 0);
    QCOMPARE(run->state(), PadRun::Stopped);

    // Stopped is not armed: RT + A goes nowhere.
    with(kLT, kA);
    QCOMPARE(m_runs.size(), 2);

    // Armed again: Stopped is over.
    arm(true);
    QCOMPARE(run->state(), PadRun::Armed);
    QCOMPARE(pc->padLight(), PadRun::armedColour());

    // From armed, the bumpers first (inside the chord's time): the stop,
    // the arm dropped by it and not flipped by the chord.
    run->setArmChordMs(200);
    QSignalSpy armed(run, &PadRun::armedChanged);
    hold(kLB, true);
    hold(kRB, true);
    axis(kLT, 255);
    axis(kRT, 255);
    QCOMPARE(m_runs.size(), 3);
    QCOMPARE(m_runs.at(2).first.command, ShowControl::AllOff);
    QVERIFY(!run->isArmed());
    QCOMPARE(armed.count(), 1);
    QTest::qWait(260);
    QVERIFY(!run->isArmed());
    QCOMPARE(run->state(), PadRun::Stopped);
    axis(kRT, 0);
    axis(kLT, 0);
    hold(kRB, false);
    hold(kLB, false);

    // The show run again from anywhere (the keys, the Run tab): no longer Stopped.
    emit m_doc->showControl()->commandHandled(QStringLiteral("GO"), true);
    QCOMPARE(run->state(), PadRun::Run);
    QVERIFY(!pc->padLight().isValid());

    // In the Tools role the four are not a stop.
    run->setEnabled(false);
    hold(kLB, true);
    hold(kRB, true);
    axis(kLT, 255);
    axis(kRT, 255);
    QCOMPARE(m_runs.size(), 3);
    axis(kRT, 0);
    axis(kLT, 0);
    hold(kRB, false);
    hold(kLB, false);
}

void PadRun_Test::theStateHasOneSource()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    QSignalSpy states(run, &PadRun::stateChanged);
    /* What every surface reads, checked as one: the state, its words, its
       colour and the pad's light. */
    auto is = [&](PadRun::State state, const QString &text, const QColor &colour, const char *when) {
        QVERIFY2(run->state() == state, when);
        QVERIFY2(run->roleText() == text, qPrintable(QString("%1: %2").arg(when, run->roleText())));
        QVERIFY2(run->stateColour() == colour, when);
        QVERIFY2(run->stateInk().isValid() == colour.isValid(), when);
        QVERIFY2(run->isArmed() == (state == PadRun::Armed), when);
    };
    is(PadRun::Tools, "Pad: Tools", QColor(), "at the start");

    run->setEnabled(true);
    is(PadRun::Run, "Pad: Run", QColor(), "the role on");
    QCOMPARE(states.count(), 1);
    QVERIFY(!pc->padLight().isValid());

    arm(true);
    is(PadRun::Armed, "Pad: ARMED", PadRun::armedColour(), "armed");
    QCOMPARE(states.count(), 2);
    QCOMPARE(pc->padLight(), PadRun::armedColour());

    // Armed, Menu walks nothing: the state does not move, and it says why.
    press(kMenu);
    is(PadRun::Armed, "Pad: ARMED", PadRun::armedColour(), "Menu while armed");
    QVERIFY(run->notice().contains("disarm first"));
    QCOMPARE(states.count(), 2);

    // A tool started on the laptop takes the pad: the arm drops with it.
    QVERIFY(pc->refineSession()->start(m_cue1, 0xFFFFFFFF, nullptr));
    is(PadRun::ToolHasPad, "Pad: Focus This Cue", QColor(), "a tool has the pad");
    QCOMPARE(run->notice(), QString("Disarmed: Focus This Cue has the controller."));
    pc->refineSession()->stop();
    is(PadRun::Run, "Pad: Run", QColor(), "the tool ended");
    QVERIFY(!pc->padLight().isValid());

    // The walk.
    press(kMenu);
    is(PadRun::Walking, "Pad: Walk", QColor(), "walking");
    // The chord arms nothing while walking.
    hold(kLB, true);
    hold(kRB, true);
    QTest::qWait(60);
    hold(kRB, false);
    hold(kLB, false);
    is(PadRun::Walking, "Pad: Walk", QColor(), "the chord while walking");
    run->stopWalk();
    is(PadRun::Run, "Pad: Run", QColor(), "the walk ended");

    // The emergency stop, then the role off.
    states.clear();
    run->emergencyStop();
    is(PadRun::Stopped, "Pad: STOPPED", PadRun::stoppedColour(), "stopped");
    QCOMPARE(states.count(), 1);
    QCOMPARE(pc->padLight(), PadRun::stoppedColour());
    run->setEnabled(false);
    is(PadRun::Tools, "Pad: Tools", QColor(), "the role off");
    QCOMPARE(states.count(), 2);
    QVERIFY(!pc->padLight().isValid());
    // And on again: not still Stopped.
    run->setEnabled(true);
    is(PadRun::Run, "Pad: Run", QColor(), "the role on again");
}

void PadRun_Test::nothingFiresInTheWrongRole()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    stub->m_buttonActions = { { kA, "chaser_step_forward" } };
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    pc->autoBindFromProfile();              // reads the controller, as on load
    QVERIFY(!pc->padRun()->isEnabled());    // decided: Tools by default
    QSignalSpy actions(pc, &ProgrammerController::buttonActionTriggered);
    QSignalSpy focusAsked(pc, &ProgrammerController::focusModeRequested);

    // Tools: the chord arms nothing, and RT + A is the profile's, never a GO.
    pc->padRun()->setArmChordMs(15);
    hold(kLB, true);
    hold(kRB, true);
    QTest::qWait(60);
    hold(kRB, false);
    hold(kLB, false);
    QVERIFY(!pc->padRun()->isArmed());
    with(kLT, kA);
    QVERIFY(m_runs.isEmpty());
    QCOMPARE(actions.count(), 1);
    QCOMPARE(actions.at(0).at(0).toString(), QString("chaser_step_forward"));

    // Run: the GO, and not the profile's action beside it.
    pc->padRun()->setEnabled(true);
    arm(true);
    with(kLT, kA);
    arm(false);
    QCOMPARE(m_runs.size(), 1);
    QCOMPARE(actions.count(), 1);

    // View + Menu asks for Focus mode only in Tools.
    hold(kView, true);
    press(kMenu);
    hold(kView, false);
    QCOMPARE(focusAsked.count(), 0);
    pc->padRun()->setEnabled(false);
    hold(kView, true);
    press(kMenu);
    hold(kView, false);
    QCOMPARE(focusAsked.count(), 1);
    QCOMPARE(m_runs.size(), 1);

    stub->m_buttonActions.clear();
}

void PadRun_Test::xboxAndDualSenseSendTheSameCommands()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    const QString xbox = QStringLiteral("Xbox Wireless Controller");
    const QString dualSense = QStringLiteral("DualSense Wireless Controller");
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << xbox << dualSense;
    // The DualSense lists its face buttons and bumpers in another order.
    QMap<quint32, QString> xroles = { { 6, "a" }, { 7, "b" }, { 8, "lb" }, { 9, "x" },
                                      { 10, "rb" }, { 11, "lt" } };
    QMap<quint32, QString> droles = { { 6, "x" }, { 7, "a" }, { 8, "b" }, { 9, "lb" },
                                      { 10, "lt" }, { 11, "rb" } };
    stub->m_lineChannelRoles[0] = xroles;
    stub->m_lineChannelRoles[1] = droles;
    stub->m_lineDeviceKind[0] = "xbox";
    stub->m_lineDeviceKind[1] = "dualsense";

    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    QVERIFY(iom->addInputSlotDevice(0, stub->name(), dualSense));
    ProgrammerController *pc = m_doc->programmer();
    pc->padRun()->setEnabled(true);
    QCOMPARE(pc->padFamily(), PadNames::Xbox);

    /* By the pad's own channels: the chord (both bumpers) arms or disarms,
       then the trigger held with a button. */
    PadRun *run = pc->padRun();
    run->setArmChordMs(15);
    auto chord = [&](quint32 lb, quint32 rb, bool on) {
        hold(lb, true);
        hold(rb, true);
        QTRY_COMPARE(run->isArmed(), on);
        hold(rb, false);
        hold(lb, false);
    };
    // The Xbox pad: armed, LT + A, then LT + B.
    InputPatch *ip = iom->inputPatch(0);
    chord(ip->showChannel(8), ip->showChannel(10), true);
    hold(ip->showChannel(11), true);
    press(ip->showChannel(6));
    press(ip->showChannel(7));
    hold(ip->showChannel(11), false);
    chord(ip->showChannel(8), ip->showChannel(10), false);
    QCOMPARE(m_runs.size(), 2);

    // The Xbox pad off: the DualSense in its place, its own channels.
    stub->m_lineNames = QStringList() << dualSense;
    stub->m_lineChannelRoles.clear();
    stub->m_lineChannelRoles[0] = droles;
    stub->m_lineDeviceKind.clear();
    stub->m_lineDeviceKind[0] = "dualsense";
    stub->configure();
    QCOMPARE(iom->inputSlotLive(0), 1);
    QCOMPARE(pc->padFamily(), PadNames::DualSense);

    // Armed (L1 + R1), L2 + Cross, then L2 + Circle, by the DualSense's own
    // channels: the same two commands.
    ip = iom->inputPatch(0);
    chord(ip->showChannel(9), ip->showChannel(11), true);
    hold(ip->showChannel(10), true);
    press(ip->showChannel(7));
    press(ip->showChannel(8));
    hold(ip->showChannel(10), false);
    chord(ip->showChannel(9), ip->showChannel(11), false);
    QCOMPARE(m_runs.size(), 4);
    QCOMPARE(m_runs.at(0).first.command, ShowControl::Go);
    QCOMPARE(m_runs.at(1).first.command, ShowControl::Back);
    QCOMPARE(m_runs.at(2).first.command, ShowControl::Go);
    QCOMPARE(m_runs.at(3).first.command, ShowControl::Back);
    for (const auto &r : m_runs)
        QCOMPARE(r.second, ShowControl::RoutePad);

    stub->m_linesAreHardware = false;
    stub->m_lineNames.clear();
    stub->m_lineChannelRoles.clear();
    stub->m_lineDeviceKind.clear();
}

void PadRun_Test::theListIsTheRunTabs()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Show");
    const quint32 act1 = st->addCueList(prod, "Act 1");
    const quint32 act2 = st->addCueList(prod, "Act 2");
    st->addCue(act1, -1, "Open");
    st->addCue(act2, -1, "Second");
    const quint32 c1 = st->cueList(act1)->chaser;
    const quint32 c2 = st->cueList(act2)->chaser;
    QVERIFY(qobject_cast<Chaser *>(m_doc->function(c1)) != nullptr);
    QVERIFY(qobject_cast<Chaser *>(m_doc->function(c2)) != nullptr);

    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    QCOMPARE(run->listChaser(), quint32(UINT_MAX));        // the main list: Act 1
    QCOMPARE(m_doc->showControl()->mainCueList(), c1);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    // RT / LT, pulled and let go, armed or not: no list picked, nothing sent or said.
    for (bool armed : { false, true })
    {
        arm(armed);
        said.clear();
        for (quint32 t : { kRT, kLT })
        {
            axis(t, 255);
            axis(t, 0);
        }
        // The safety says nothing; RT is the hit's (none here); neither names a list.
        for (const QList<QVariant> &a : said)
            QVERIFY2(!a.first().toString().contains("Act"), qPrintable(a.first().toString()));
    }
    arm(false);
    QCOMPARE(run->listChaser(), quint32(UINT_MAX));
    QVERIFY(m_runs.isEmpty());

    // The Run tab picks the list: the pad's GO plays it, RS names it.
    m_doc->resetModified();
    m_doc->controlMap()->setRunTabList(act2);
    QVERIFY(m_doc->isModified());                          // the show's choice
    QCOMPARE(run->listChaser(), c2);
    arm(true);
    with(kLT, kA);
    arm(false);
    QCOMPARE(m_runs.size(), 1);
    QCOMPARE(m_runs.at(0).first.chaser, c2);
    press(kRS);
    QCOMPARE(said.last().at(0).toString(), QString("Act 2. Next: cue 1."));
    QVERIFY(run->wherePhrase().startsWith("Act 2"));

    // Saved with the show and back on load.
    const QByteArray xml = showXml(m_doc);
    QVERIFY(xml.contains(QString("RunList=\"%1\"").arg(act2).toUtf8()));
    Doc *other = new Doc(this);
    QXmlStreamReader sr(xml);
    sr.readNextStartElement();
    QVERIFY(other->loadXML(sr, false));
    QCOMPARE(other->controlMap()->runTabList(), act2);
    QCOMPARE(other->programmer()->padRun()->listChaser(), c2);
    delete other;

    // Back to the main list: nothing written.
    m_doc->controlMap()->setRunTabList(0xFFFFFFFF);
    QCOMPARE(run->listChaser(), quint32(UINT_MAX));
    QVERIFY(!showXml(m_doc).contains("RunList"));
}

void PadRun_Test::nothingOnThePadLeavesTheRunRole()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    // Every button and trigger the Xbox profile has, by its role.
    QMap<quint32, QString> roles = xboxRoles();
    const QMap<quint32, QString> more = {
        { 40, "ls" }, { 41, "guide" }, { 42, "dpad_down" }, { 43, "dpad_left" }, { 44, "dpad_right" } };
    for (auto it = more.begin(); it != more.end(); ++it)
        roles.insert(it.key(), it.value());
    stub->m_channelRoles = roles;
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Show");
    const quint32 list = st->addCueList(prod, "Main");
    st->addCue(list, -1, "Open");
    m_doc->setMode(Doc::Operate);
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    QSignalSpy off(run, &PadRun::enabledChanged);

    auto check = [&](const QString &what) {
        QVERIFY2(run->isEnabled(), qPrintable(what));
        const QString text = run->roleText();
        QVERIFY2(text == "Pad: Run" || text == "Pad: ARMED" || text == "Pad: Walk",
                 qPrintable(what + ": " + text));
    };
    for (bool armed : { false, true })
    {
        arm(armed);
        for (auto it = roles.begin(); it != roles.end(); ++it)
        {
            const quint32 ch = it.key();
            const QString what = QString("%1%2").arg(armed ? "armed, " : "", it.value());
            // A press, then a hold past every hold time, then the release.
            press(ch);
            check(what + " pressed");
            hold(ch, true);
            QTest::qWait(60);
            check(what + " held");
            hold(ch, false);
            check(what + " let go");
            // No one control arms or disarms either.
            QVERIFY2(run->isArmed() == armed, qPrintable(what));
            // A walk (Menu) is still the Run role; Menu again leaves it.
            if (run->isWalking())
                run->stopWalk();
            QVERIFY(!run->isWalking());
        }
    }
    // The sticks and the triggers, pushed all the way and back.
    for (quint32 a : { kLX, quint32(1), quint32(2), quint32(3), kLT, kRT })
    {
        axis(a, 255);
        axis(a, 0);
        axis(a, 128);
        check(QString("axis %1").arg(a));
    }
    QCOMPARE(off.count(), 0);
    // Only the role chord leaves: View + Menu held.
    run->setRoleChordMs(30);
    hold(kView, true);
    hold(kMenu, true);
    QTRY_VERIFY(!run->isEnabled());
    hold(kMenu, false);
    hold(kView, false);
    QCOMPARE(run->roleText(), QString("Pad: Tools"));
}

void PadRun_Test::theControllerTestTakesThePad()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    stub->m_buttonActions = { { kA, "chaser_step_forward" } };
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    pc->autoBindFromProfile();
    PadRun *run = pc->padRun();
    QSignalSpy actions(pc, &ProgrammerController::buttonActionTriggered);

    // Tools role: a button tried in the test is not the profile's action.
    run->setTesting(true);
    QVERIFY(run->isTesting());
    press(kA);
    QCOMPARE(actions.count(), 0);
    run->setTesting(false);
    press(kA);
    QCOMPARE(actions.count(), 1);

    // Run role, armed: the test takes the pad and the arm drops, on every surface.
    run->setEnabled(true);
    run->setRoleChordMs(30);
    arm(true);
    run->setTesting(true);
    QVERIFY(!run->isArmed());
    QCOMPARE(run->state(), PadRun::ToolHasPad);
    QCOMPARE(run->roleText(), QString("Pad: Controller Test"));
    QVERIFY(!run->stateColour().isValid());
    QCOMPARE(run->notice(), QString("Disarmed: Controller Test has the controller."));
    QVERIFY(!pc->padLight().isValid());
    // Everything a tester presses: no arm, no GO, no hit, no role change.
    hold(kLB, true);
    hold(kRB, true);
    QTest::qWait(60);
    QVERIFY(!run->isArmed());
    axis(kLT, 255);
    press(kA);
    axis(kLT, 0);
    hold(kRB, false);
    hold(kLB, false);
    axis(kRT, 255);                 // RT alone: no hit
    axis(kRT, 0);
    hold(kView, true);
    hold(kMenu, true);
    QTest::qWait(90);
    hold(kMenu, false);
    hold(kView, false);
    press(kMenu);
    QVERIFY(m_runs.isEmpty());
    QVERIFY(run->isEnabled());
    QVERIFY(!run->isWalking());
    QCOMPARE(actions.count(), 1);
    QCOMPARE(run->state(), PadRun::ToolHasPad);

    // The test closed: the Run role again, not armed until the chord.
    run->setTesting(false);
    QCOMPARE(run->state(), PadRun::Run);
    QCOMPARE(run->roleText(), QString("Pad: Run"));
    with(kLT, kA);
    QVERIFY(m_runs.isEmpty());
    arm(true);
    with(kLT, kA);
    QCOMPARE(m_runs.size(), 1);
    arm(false);

    /* The emergency stop, though (review 0.4.10, M2): the four together
       stop the show with the test open too -- the test shows the presses,
       and the stop acts. */
    run->setTesting(true);
    hold(kLB, true);
    hold(kRB, true);
    axis(kLT, 255);
    axis(kRT, 255);
    QCOMPARE(m_runs.size(), 2);
    QCOMPARE(m_runs.last().first.command, ShowControl::AllOff);
    QCOMPARE(run->state(), PadRun::Stopped);
    axis(kRT, 0);
    axis(kLT, 0);
    hold(kRB, false);
    hold(kLB, false);
    QCOMPARE(m_runs.size(), 2);     // once a chord
    run->setTesting(false);
    QCOMPARE(run->state(), PadRun::Stopped);
    stub->m_buttonActions.clear();
}

void PadRun_Test::theLightBarAndRumble()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    QSignalSpy rumbles(pc, &ProgrammerController::padRumbled);
    QVERIFY(!pc->padLight().isValid());        // not armed: the pad's own colour

    // Armed: the arm's colour, as the screen's, and two short buzzes.
    arm(true);
    QCOMPARE(pc->padLight(), PadRun::armedColour());
    QCOMPARE(pc->padLight(), run->stateColour());
    QVERIFY(!stub->m_feedbackParams.isEmpty());
    QCOMPARE(rumbles.count(), 1);
    QCOMPARE(rumbles.last().at(0).toInt(), 2);
    QCOMPARE(rumbles.last().at(1).toInt(), ControllerScheme::rumbleShortMs());

    // A GO: green at once, one short buzz, then it blinks with the arm's colour...
    with(kLT, kA);
    QVERIFY(run->isPlaying());
    QCOMPARE(pc->padLight(), PadRun::playingColour());
    QCOMPARE(rumbles.count(), 2);
    QCOMPARE(rumbles.last().at(0).toInt(), 1);
    QCOMPARE(rumbles.last().at(1).toInt(), ControllerScheme::rumbleShortMs());
    QTRY_COMPARE_WITH_TIMEOUT(pc->padLight(), PadRun::armedColour(), PadRun::blinkMs() * 4);
    // ...and steady in it once nothing fades any more.
    QTRY_VERIFY_WITH_TIMEOUT(!run->isPlaying(), PadRun::wentFlashMs() * 5);
    QTest::qWait(PadRun::blinkMs() * 2 + 50);
    QCOMPARE(pc->padLight(), PadRun::armedColour());

    // Nothing to play: the long buzz, no green.
    m_answer = false;
    with(kLT, kA);
    QCOMPARE(rumbles.count(), 3);
    QCOMPARE(rumbles.last().at(1).toInt(), ControllerScheme::rumbleLongMs());
    QCOMPARE(pc->padLight(), PadRun::armedColour());
    QVERIFY(!run->isPlaying());

    // Disarmed: the pad's own colour, one long buzz.
    arm(false);
    QVERIFY(!pc->padLight().isValid());
    QCOMPARE(rumbles.last().at(0).toInt(), 1);
    QCOMPARE(rumbles.last().at(1).toInt(), ControllerScheme::rumbleLongMs());

    // The emergency stop: red, three long.
    run->emergencyStop();
    QCOMPARE(pc->padLight(), PadRun::stoppedColour());
    QCOMPARE(rumbles.last().at(0).toInt(), 3);
    // The three colours are three: none passes for another.
    QVERIFY(PadRun::armedColour() != PadRun::playingColour());
    QVERIFY(PadRun::armedColour() != PadRun::stoppedColour());
    QVERIFY(PadRun::playingColour() != PadRun::stoppedColour());
}

void PadRun_Test::aLostPadDisarms()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    arm(true);
    emit pc->controllerLost(true);
    QVERIFY(!run->isArmed());
    QCOMPARE(run->state(), PadRun::Run);
    // No button did it, so the screen says what did.
    QCOMPARE(run->notice(), QString("Disarmed: the controller went away."));
    QVERIFY(!pc->padLight().isValid());
    // Back again it is not armed: RT + A goes nowhere until the chord.
    emit pc->controllerLost(false);
    with(kLT, kA);
    QVERIFY(m_runs.isEmpty());
    // Turning the role off disarms too, and its buttons are free again.
    arm(true);
    run->setEnabled(false);
    QVERIFY(!run->isArmed());
    with(kLT, kA);
    QVERIFY(m_runs.isEmpty());
}

/*****************************************************************************
 * Venue focus from the pad (rule 6)
 *****************************************************************************/

void PadRun_Test::makeFocusCue()
{
    ProgrammerController *pc = m_doc->programmer();
    MonitorProperties *props = m_doc->monitorProperties();
    QList<quint32> all = m_fronts + m_backs;
    for (int i = 0; i < all.size(); ++i)
        props->setFixturePosition(all.at(i), 0, 0, QVector3D(2000.0f * (i + 1), 2000.0f, 5.0f));
    auto group = [this](const QString &name, const QList<quint32> &ids) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        g->setSize(QSize(int(ids.size()), 1));
        for (int i = 0; i < ids.size(); ++i)
            g->assignFixture(ids.at(i), QLCPoint(i, 0));
        m_doc->addFixtureGroup(g);
        return g->id();
    };
    StageTarget *dsc = props->addStageTarget();
    dsc->setName("DSC");
    dsc->setPosition(QVector3D(5.0f, 6.0f, 0.0f));
    FocusCall c1;
    c1.groups << group("Fronts", m_fronts);
    c1.target.kind = FocusSession::Reference::Target;
    c1.target.id = dsc->id();
    FocusCall c2 = c1;
    c2.groups = { group("Backs", m_backs) };
    pc->focusCalls()->setCalls({ c1, c2 });
    QVERIFY(m_doc->showTree()->setCuesFocusCall({ m_cue1 }, true));
    m_running = m_cue1;
    pc->focusCallCue()->setRunningCueProbe([this](quint32) { return m_running; });
    // The panel's part: Leave asks about unsaved work, then finishes.
    connect(pc->focusSession(), &FocusSession::exitRequested, pc->focusSession(), &FocusSession::finish);
    m_doc->setMode(Doc::Operate);
    pc->focusCallCue()->cueCurrent(m_list, m_cue1);
    QCOMPARE(pc->focusCallCue()->state(), FocusCallCue::Armed);
}

void PadRun_Test::theVenueFocusSweep()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    // Every button and trigger the Xbox profile has, by its role.
    QMap<quint32, QString> roles = xboxRoles();
    const QMap<quint32, QString> more = {
        { 40, "ls" }, { 41, "guide" }, { 42, "dpad_down" }, { 43, "dpad_left" }, { 44, "dpad_right" } };
    for (auto it = more.begin(); it != more.end(); ++it)
        roles.insert(it.key(), it.value());
    stub->m_channelRoles = roles;
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    FocusSession *s = pc->focusSession();
    run->setEnabled(true);
    run->setRoleChordMs(30);
    QSignalSpy exits(s, &FocusSession::exitRequested);

    const char *names[] = { "not armed", "ARMED", "FOCUS" };
    for (int mode = 0; mode < 3; ++mode)
    {
        if (mode == 1)
            arm(true);
        if (mode == 2)
        {
            with(kLT, kMenu);
            QCOMPARE(run->state(), PadRun::Focus);
        }
        const PadRun::State state = run->state();
        const FocusCallCue::State cue = fcc->state();
        const bool armed = run->isArmed();
        auto check = [&](const QString &what) {
            QVERIFY2(run->state() == state, qPrintable(what + ": " + run->roleText()));
            QVERIFY2(fcc->state() == cue, qPrintable(what + ": the focus started or ended"));
            QVERIFY2(run->isArmed() == armed, qPrintable(what + ": the arm changed"));
            QVERIFY2(m_runs.isEmpty(), qPrintable(what + ": a command went to run()"));
            QVERIFY2(exits.isEmpty(), qPrintable(what + ": the focus was asked to end"));
            QVERIFY2(s->isActive() == (mode == 2), qPrintable(what));
            QVERIFY2(run->isEnabled(), qPrintable(what));
        };
        for (auto it = roles.begin(); it != roles.end(); ++it)
        {
            const quint32 ch = it.key();
            const QString what = QString("%1, %2").arg(names[mode], it.value());
            // A press, then a hold past every hold time, then the release.
            press(ch);
            check(what + " pressed");
            hold(ch, true);
            QTest::qWait(60);
            check(what + " held");
            hold(ch, false);
            check(what + " let go");
            if (ch == kMenu)
            {
                // Menu alone says what starts or ends it.
                const QString n = run->notice();
                if (mode == 0)
                    QVERIFY2(n.contains("Not armed. To start the venue focus, hold LB and RB to arm, then LT + Menu."),
                             qPrintable(n));
                else if (mode == 1)
                    QVERIFY2(n.contains("disarm first") && n.contains("LT + Menu starts the venue focus"),
                             qPrintable(n));
                else
                    QVERIFY2(n.contains("hold LT and press Menu to finish it"), qPrintable(n));
            }
        }
        // View + Menu held past the role chord's time: in FOCUS, nothing.
        if (mode == 2)
        {
            hold(kView, true);
            hold(kMenu, true);
            QTest::qWait(80);
            hold(kMenu, false);
            hold(kView, false);
            check("FOCUS, View + Menu held");

            /* The exit chord never moves the focus: LT + Menu, held and let
               go in either order, leaves the call where it was -- here with
               the laptop's prompt answered Cancel, so the focus goes on.
               A fresh focus first: the sweep left the target page on with
               the target moved, which holds the calls (by design). */
            s->stop();
            QCOMPARE(run->state(), PadRun::Armed);
            m_running = m_empty;
            fcc->cueCurrent(m_list, m_empty);
            m_running = m_cue1;
            fcc->cueCurrent(m_list, m_cue1);
            with(kLT, kMenu);
            QCOMPARE(run->state(), PadRun::Focus);
            exits.clear();
            QVERIFY(disconnect(s, &FocusSession::exitRequested, s, &FocusSession::finish));
            auto diag = [&]() {
                return QString("call %1, pc holding %2, session holding %3, target page %4, target moved %5, "
                               "state %6, cue %7")
                    .arg(s->callIndex()).arg(pc->isHolding()).arg(s->isHolding()).arg(s->isTargetPage())
                    .arg(s->targetMoved()).arg(run->roleText()).arg(int(fcc->state()));
            };
            // First an LT tap on its own, before any chord: back one call, on the release.
            s->setCallIndex(1);
            axis(kLT, 255);
            QVERIFY2(s->callIndex() == 1, qPrintable("LT down: " + diag()));
            axis(kLT, 0);
            QVERIFY2(s->callIndex() == 0, qPrintable("LT tap, before the chords: " + diag()));
            s->setCallIndex(1);
            const QList<QList<QPair<quint32, bool>>> orders = {
                { { kLT, true }, { kMenu, true }, { kMenu, false }, { kLT, false } },
                { { kLT, true }, { kMenu, true }, { kLT, false }, { kMenu, false } },
                { { kMenu, true }, { kLT, true }, { kLT, false }, { kMenu, false } },
                { { kMenu, true }, { kLT, true }, { kMenu, false }, { kLT, false } } };
            int asked = 0;
            for (int o = 0; o < orders.size(); ++o)
            {
                for (const QPair<quint32, bool> &step : orders.at(o))
                    step.first == kLT ? axis(kLT, step.second ? 255 : 0) : hold(kMenu, step.second);
                const QString what = QString("FOCUS, LT + Menu, order %1").arg(o + 1);
                QVERIFY2(s->callIndex() == 1, qPrintable(what + QString(": call %1").arg(s->callIndex())));
                QVERIFY2(run->state() == PadRun::Focus, qPrintable(what));
                QVERIFY2(m_runs.isEmpty(), qPrintable(what));
                // LT held as Menu went down: the end was asked (Cancel here).
                if (o < 2)
                    ++asked;
                QVERIFY2(exits.count() == asked, qPrintable(what + QString(": asked %1").arg(exits.count())));
            }
            // An LT tap alone still steps back one call (on its release).
            axis(kLT, 255);
            QVERIFY2(s->callIndex() == 1, qPrintable("LT down: " + diag()));
            axis(kLT, 0);
            QVERIFY2(s->callIndex() == 0, qPrintable("LT tap, after the chords: " + diag()));
            // RT is unchanged: the next call on the press.
            axis(kRT, 255);
            QCOMPARE(s->callIndex(), 1);
            axis(kRT, 0);
            QCOMPARE(run->state(), PadRun::Focus);
            QVERIFY(m_runs.isEmpty());
        }
    }
}

void PadRun_Test::ltMenuGoesIntoAndOutOfFocus()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    FocusSession *s = pc->focusSession();
    run->setEnabled(true);
    QCOMPARE(fcc->readyPhrase(), QString("Venue focus ready. Armed, hold {LT} and press {Menu} to begin."));

    // Not armed: refused, saying to arm first.
    with(kLT, kMenu);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCOMPARE(run->state(), PadRun::Run);
    QCOMPARE(run->notice(), QString("Not armed. To start the venue focus, hold LB and RB to arm, then LT + Menu."));

    // ARMED, LT + Menu: FOCUS, from the one source.
    arm(true);
    QSignalSpy states(run, &PadRun::stateChanged);
    with(kLT, kMenu);
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QVERIFY(s->isActive());
    QCOMPARE(run->state(), PadRun::Focus);
    QCOMPARE(run->roleText(), QString("Pad: FOCUS"));
    QCOMPARE(run->stateColour(), PadRun::focusColour());
    QCOMPARE(run->lightColour(), PadRun::focusColour());
    QCOMPARE(pc->padLight(), PadRun::focusColour());
    QCOMPARE(run->stateInk(), QColor(Qt::black));
    QVERIFY(!run->isArmed());
    QCOMPARE(states.count(), 1);
    QVERIFY(m_runs.isEmpty());
    // The colour is its own: not the arm's, the GO's, the stop's.
    for (const QColor &c : { PadRun::armedColour(), PadRun::playingColour(), PadRun::stoppedColour() })
        QVERIFY(c != PadRun::focusColour());
    QVERIFY(run->helpHtml().contains("Venue focus has the controller"));

    // Menu alone: nothing ends.
    press(kMenu);
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QCOMPARE(run->state(), PadRun::Focus);
    QCOMPARE(run->notice(), QString("Venue focus: hold LT and press Menu to finish it."));

    // LT + Menu again: the focus ends (as the panel's Leave), ARMED again.
    QSignalSpy armedSpy(run, &PadRun::armedChanged);
    with(kLT, kMenu);
    QVERIFY(!s->isActive());
    QCOMPARE(fcc->state(), FocusCallCue::Idle);
    QCOMPARE(run->state(), PadRun::Armed);
    QVERIFY(run->isArmed());
    QCOMPARE(run->armedUniverse(), quint32(0));
    QCOMPARE(armedSpy.count(), 1);
    QCOMPARE(pc->padLight(), PadRun::armedColour());
    // The list did not move: no command at all, still on the focus cue, done.
    QVERIFY(m_runs.isEmpty());
    QVERIFY(fcc->isDone());
    fcc->cueCurrent(m_list, m_cue1);            // the list still there: not armed again
    QCOMPARE(fcc->state(), FocusCallCue::Idle);
    QVERIFY(fcc->isDone());

    // LT + A: the GO that moves it on.
    with(kLT, kA);
    QCOMPARE(m_runs.size(), 1);
    QCOMPARE(m_runs.at(0).first.command, ShowControl::Go);
    QCOMPARE(m_runs.at(0).second, ShowControl::RoutePad);
    m_running = m_empty;
    fcc->cueCurrent(m_list, m_empty);
    QVERIFY(!fcc->isDone());
}

void PadRun_Test::rtPastTheLastCallAsks()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    FocusSession *s = pc->focusSession();
    run->setEnabled(true);
    arm(true);
    with(kLT, kMenu);
    QCOMPARE(run->state(), PadRun::Focus);
    QCOMPARE(s->callIndex(), 0);
    QSignalSpy exits(s, &FocusSession::exitRequested);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    // RT: the second (last) call; RT again: it says how to finish, and stays.
    axis(kRT, 255);
    axis(kRT, 0);
    QCOMPARE(s->callIndex(), 1);
    axis(kRT, 255);
    axis(kRT, 0);
    QCOMPARE(s->callIndex(), 1);
    QCOMPARE(exits.count(), 0);
    QVERIFY(s->isActive());
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QCOMPARE(run->state(), PadRun::Focus);
    QCOMPARE(run->notice(), QString("Last call. Hold LT and press Menu to finish."));
    QCOMPARE(said.last().at(0).toString(), QString("Last call. Hold LT and press Menu to finish."));
    // Held on RT: still nothing ends.
    axis(kRT, 255);
    QTest::qWait(60);
    axis(kRT, 0);
    QCOMPARE(exits.count(), 0);
    // Then LT + Menu finishes: ARMED, nothing played.
    with(kLT, kMenu);
    QCOMPARE(exits.count(), 1);
    QCOMPARE(run->state(), PadRun::Armed);
    QVERIFY(m_runs.isEmpty());

    // In the Tools role, RT past the last call ends the calls, as it did.
    run->setEnabled(false);
    m_running = m_empty;
    fcc->cueCurrent(m_list, m_empty);
    m_running = m_cue1;
    fcc->cueCurrent(m_list, m_cue1);
    QVERIFY(fcc->start());
    s->setCallIndex(1);
    s->setTrigger(1, 1.0f);
    s->setTrigger(1, 0.0f);
    QCOMPARE(exits.count(), 2);
    QVERIFY(!s->isActive());
    QVERIFY(m_runs.isEmpty());
}

void PadRun_Test::focusWhileTheVenueIsNamed()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    QTemporaryDir dir;
    makeShow();
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    pc->venueCorrections()->setShowFile(dir.filePath("Spring.qlcc"), false);
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    FocusSession *s = pc->focusSession();
    run->setEnabled(true);
    arm(true);
    with(kLT, kMenu);
    QCOMPARE(fcc->state(), FocusCallCue::NamingVenue);
    QVERIFY(!s->isActive());
    QCOMPARE(run->state(), PadRun::Focus);
    QVERIFY(!run->isArmed());
    // No cue moves meanwhile, whatever is pressed; the bumpers arm nothing.
    with(kLT, kA);
    with(kLT, kB);
    with(kLT, kX);
    with(kLT, kY);
    press(kA);
    axis(kRT, 255);
    axis(kRT, 0);
    run->setArmChordMs(15);
    hold(kLB, true);
    hold(kRB, true);
    QTest::qWait(60);
    hold(kRB, false);
    hold(kLB, false);
    QVERIFY(m_runs.isEmpty());
    QVERIFY(!run->isArmed());
    QCOMPARE(run->state(), PadRun::Focus);
    QCOMPARE(fcc->state(), FocusCallCue::NamingVenue);
    // LT + Menu gives the start up: the cue waits again, ARMED.
    with(kLT, kMenu);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCOMPARE(run->state(), PadRun::Armed);
    QVERIFY(run->isArmed());
    // Again, and named on the laptop: the calls, never ARMED on the way.
    with(kLT, kMenu);
    QCOMPARE(fcc->state(), FocusCallCue::NamingVenue);
    QSignalSpy armedSpy(run, &PadRun::armedChanged);
    QVERIFY(fcc->useVenue("Kennedy"));
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QCOMPARE(run->state(), PadRun::Focus);
    QCOMPARE(armedSpy.count(), 0);
    with(kLT, kMenu);
    QCOMPARE(run->state(), PadRun::Armed);
    QVERIFY(m_runs.isEmpty());
}

void PadRun_Test::theStopInFocus()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    run->setEnabled(true);
    arm(true);
    with(kLT, kMenu);
    QCOMPARE(run->state(), PadRun::Focus);

    // The focus's own presses and chords: no stop.
    const QList<QList<quint32>> notStops = {
        { kLB, kRB }, { kLT }, { kRT }, { kLT, kRT }, { kLB, kRB, kRT }, { kLB, kRB, kLT },
        { kLB, kLT, kRT }, { kRB, kLT, kRT } };
    for (const QList<quint32> &chord : notStops)
    {
        for (quint32 ch : chord)
            (ch == kLT || ch == kRT) ? axis(ch, 255) : hold(ch, true);
        QTest::qWait(20);
        for (quint32 ch : chord)
            (ch == kLT || ch == kRT) ? axis(ch, 0) : hold(ch, false);
        QVERIFY2(m_runs.isEmpty(), qPrintable(QString("%1 controls made a stop").arg(chord.size())));
        QCOMPARE(run->state(), PadRun::Focus);
        QCOMPARE(fcc->state(), FocusCallCue::Running);
    }
    // All four: the stop.
    hold(kLB, true);
    hold(kRB, true);
    axis(kLT, 255);
    axis(kRT, 255);
    QCOMPARE(m_runs.size(), 1);
    QCOMPARE(m_runs.at(0).first.command, ShowControl::AllOff);
    QCOMPARE(m_runs.at(0).second, ShowControl::RoutePad);
    QCOMPARE(run->state(), PadRun::Stopped);
    QCOMPARE(run->roleText(), QString("Pad: STOPPED"));
    QCOMPARE(pc->padLight(), PadRun::stoppedColour());
    axis(kRT, 0);
    axis(kLT, 0);
    hold(kRB, false);
    hold(kLB, false);
    // The focus ended after the stop: not armed again.
    with(kLT, kMenu);
    QCOMPARE(fcc->state(), FocusCallCue::Idle);
    QVERIFY(!run->isArmed());
    QCOMPARE(run->state(), PadRun::Stopped);
    QCOMPARE(m_runs.size(), 1);
}

void PadRun_Test::theLaptopsFocusMovesNoList()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    FocusSession *s = pc->focusSession();
    auto again = [&]() {
        m_running = m_empty;
        fcc->cueCurrent(m_list, m_empty);
        m_running = m_cue1;
        fcc->cueCurrent(m_list, m_cue1);
        QCOMPARE(fcc->state(), FocusCallCue::Armed);
    };
    run->setEnabled(true);

    // Armed, the laptop's Start (the Run tab's button, Cmd+Return): FOCUS.
    arm(true);
    QVERIFY(fcc->start());
    QCOMPARE(run->state(), PadRun::Focus);
    QVERIFY(!run->isArmed());
    // The laptop's end (the panel's Finish): no command, ARMED, done.
    s->finish();
    QVERIFY(m_runs.isEmpty());
    QVERIFY(fcc->isDone());
    QCOMPARE(run->state(), PadRun::Armed);

    // Not armed: FOCUS, then back to Run, not armed.
    arm(false);
    again();
    QVERIFY(fcc->start());
    QCOMPARE(run->state(), PadRun::Focus);
    s->finish();
    QCOMPARE(run->state(), PadRun::Run);
    QVERIFY(m_runs.isEmpty());
    QVERIFY(fcc->isDone());

    // The Tools role: Menu still starts and ends it, and the list stays too.
    run->setEnabled(false);
    again();
    QCOMPARE(fcc->readyPhrase(), FocusCallCue::armedPhrase());
    press(kMenu);
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QCOMPARE(run->state(), PadRun::Tools);
    press(kMenu);
    QVERIFY(!s->isActive());
    QVERIFY(m_runs.isEmpty());
    QVERIFY(fcc->isDone());
}

void PadRun_Test::theVenueFocusNeedsTheFocusJob()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    ControllerJobs *jobs = pc->controllerJobs();
    auto restore = qScopeGuard([jobs]() { jobs->resetAll(); });
    const QString padA = jobs->identityOfUniverse(0);
    jobs->setJobs(padA, ControllerJobs::RunCues | ControllerJobs::FollowSpot);
    run->setEnabled(true);

    // ARMED, no Focus job: LT + Menu starts nothing, and says why.
    arm(true);
    with(kLT, kMenu);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCOMPARE(run->state(), PadRun::Armed);
    QCOMPARE(run->notice(), QString("This controller doesn't do focus."));
    QVERIFY(m_runs.isEmpty());

    // Given Focus: FOCUS.
    jobs->setJob(padA, ControllerJobs::Focus, true);
    with(kLT, kMenu);
    QCOMPARE(run->state(), PadRun::Focus);

    // A second pad, no Focus: its LT + Menu ends nothing.
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(1, stub->name(), QString(), 1));
    const QString padB = jobs->identityOfUniverse(1);
    QVERIFY(padB != padA);
    jobs->setJobs(padB, ControllerJobs::RunCues);
    with(kLT, kMenu, 1);
    QCOMPARE(run->state(), PadRun::Focus);
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    // With Focus, it is not the focus's pad: refused (Q135).
    jobs->setJob(padB, ControllerJobs::Focus, true);
    with(kLT, kMenu, 1);
    QCOMPARE(run->state(), PadRun::Focus);
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    // The focus's own pad ends it: ARMED, on it.
    with(kLT, kMenu);
    QCOMPARE(run->state(), PadRun::Armed);
    QCOMPARE(run->armedUniverse(), quint32(0));
    QVERIFY(m_runs.isEmpty());
}

void PadRun_Test::reviewTheStopFromAnotherPadAfterAFocusGivenUp()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    QVERIFY(iom->setInputPatch(1, stub->name(), QString(), 1));
    QTemporaryDir dir;
    makeShow();
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    pc->venueCorrections()->setShowFile(dir.filePath("Spring.qlcc"), false);
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    run->setEnabled(true);
    // A arms and starts the venue focus; the laptop asks for the venue.
    arm(true, 0);
    with(kLT, kMenu, 0);
    QCOMPARE(fcc->state(), FocusCallCue::NamingVenue);
    // A gives the start up: ARMED again, the focus cue waiting again.
    with(kLT, kMenu, 0);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCOMPARE(run->state(), PadRun::Armed);
    QCOMPARE(run->armedUniverse(), quint32(0));
    // (A is still the owner the start claimed.)
    QCOMPARE(pc->controllerOwner(), quint32(0));
    // B, every job (nothing saved): its four together.
    for (quint32 ch : { kLB, kRB, kLT, kRT })
        axis(ch, 255, 1);
    int stops = 0;
    for (const auto &r : m_runs)
        stops += r.first.command == ShowControl::AllOff ? 1 : 0;
    QVERIFY2(stops == 1, "pad B's emergency stop was swallowed (\"Another controller is in use.\")");
    QCOMPARE(run->state(), PadRun::Stopped);
    QVERIFY(!run->isArmed());
    for (quint32 ch : { kRT, kLT, kRB, kLB })
        axis(ch, 0, 1);
}

void PadRun_Test::aFocusStartGivenUpLetsTheControllerGo()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    QVERIFY(iom->setInputPatch(1, stub->name(), QString(), 1));
    QTemporaryDir dir;
    makeShow();
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    pc->venueCorrections()->setShowFile(dir.filePath("Spring.qlcc"), false);
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    run->setEnabled(true);

    // Given up from the pad: no owner once the press is done.
    arm(true, 0);
    with(kLT, kMenu, 0);
    QCOMPARE(fcc->state(), FocusCallCue::NamingVenue);
    QCOMPARE(pc->controllerOwner(), quint32(0));
    with(kLT, kMenu, 0);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCoreApplication::processEvents();
    QCOMPARE(pc->controllerOwner(), quint32(0xFFFFFFFF));
    QCOMPARE(run->state(), PadRun::Armed);

    // Started again: claimed again. Given up on the laptop: let go again.
    with(kLT, kMenu, 0);
    QCOMPARE(fcc->state(), FocusCallCue::NamingVenue);
    QCOMPARE(pc->controllerOwner(), quint32(0));
    QVERIFY(fcc->cancelStart());
    QCoreApplication::processEvents();
    QCOMPARE(pc->controllerOwner(), quint32(0xFFFFFFFF));

    // Refused (the calls have no lights): the cue waits, owned by none.
    FocusCall empty;
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Nobody");
    m_doc->addFixtureGroup(g);
    empty.groups << g->id();
    pc->focusCalls()->setCalls({ empty });
    QCOMPARE(run->state(), PadRun::Armed);
    with(kLT, kMenu, 0);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCOMPARE(run->state(), PadRun::Armed);
    QCoreApplication::processEvents();
    QCOMPARE(pc->controllerOwner(), quint32(0xFFFFFFFF));
    arm(false, 0);
}

void PadRun_Test::theStopFromAnyPadInEveryFocusState()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    QVERIFY(iom->setInputPatch(1, stub->name(), QString(), 1));
    QTemporaryDir dir;
    makeShow();
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    pc->venueCorrections()->setShowFile(dir.filePath("Spring.qlcc"), false);
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    run->setEnabled(true);
    auto stops = [this]() {
        int n = 0;
        for (const auto &r : m_runs)
            n += r.first.command == ShowControl::AllOff ? 1 : 0;
        return n;
    };
    auto fourOnB = [this]() {
        for (quint32 ch : { kLB, kRB, kLT, kRT })
            axis(ch, 255, 1);
        for (quint32 ch : { kRT, kLT, kRB, kLB })
            axis(ch, 0, 1);
    };

    // The focus cue waiting, A armed: B's four.
    arm(true, 0);
    fourOnB();
    QCOMPARE(stops(), 1);
    QCOMPARE(run->state(), PadRun::Stopped);

    // Naming the venue, A the owner: B's four.
    arm(true, 0);
    with(kLT, kMenu, 0);
    QCOMPARE(fcc->state(), FocusCallCue::NamingVenue);
    QCOMPARE(pc->controllerOwner(), quint32(0));
    fourOnB();
    QCOMPARE(stops(), 2);
    QCOMPARE(run->state(), PadRun::Stopped);

    // The calls running, A the owner: B's four.
    if (fcc->state() == FocusCallCue::Armed)
    {
        arm(true, 0);
        with(kLT, kMenu, 0);
    }
    if (fcc->state() == FocusCallCue::NamingVenue)
        QVERIFY(fcc->useVenue(QString("Hall")));
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QCOMPARE(pc->controllerOwner(), quint32(0));
    fourOnB();
    QCOMPARE(stops(), 3);
    QCOMPARE(run->state(), PadRun::Stopped);
    pc->focusSession()->finish();
}

void PadRun_Test::aLostPadInFocus()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    makeFocusCue();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    FocusCallCue *fcc = pc->focusCallCue();
    FocusSession *s = pc->focusSession();
    run->setEnabled(true);
    arm(true);
    with(kLT, kMenu);
    QCOMPARE(run->state(), PadRun::Focus);
    // Gone: unpatched (its releases never come) and said gone.
    QVERIFY(iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine()));
    emit iom->inputDeviceDisconnected(0, stub->name(), QStringLiteral("Stub pad"));
    QVERIFY2(run->state() != PadRun::Focus, qPrintable(QString("present %1").arg(pc->focusController().present)));
    QCOMPARE(run->roleText(), QString("Pad: Venue Focus"));
    QVERIFY(!run->isArmed());
    QCOMPARE(run->notice(), QString("Disarmed: the controller went away."));
    // Back mid-focus: FOCUS again, and its end leaves the pad not armed.
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    emit iom->inputDeviceConnected(0, stub->name(), QStringLiteral("Stub pad"));
    QCOMPARE(run->state(), PadRun::Focus);
    QVERIFY(fcc->state() == FocusCallCue::Running);
    s->finish();
    QCOMPARE(run->state(), PadRun::Run);
    QVERIFY(!run->isArmed());
    QVERIFY(m_runs.isEmpty());
}

void PadRun_Test::menuStartsAndStopsTheWalk()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    RefineSession *r = pc->refineSession();
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QSignalSpy walking(run, &PadRun::walkingChanged);

    // Menu does nothing in the Tools role.
    press(kMenu);
    QVERIFY(!run->isWalking());
    run->setEnabled(true);
    // In Run: the cue in hand, the one picked in the tree.
    QCOMPARE(run->cueInHand(), m_cue1);
    press(kMenu);
    QVERIFY(run->isWalking());
    QVERIFY(r->isActive());
    QVERIFY(r->isWalking());
    QCOMPARE(run->walkCue(), m_cue1);
    QCOMPARE(walking.count(), 1);
    QCOMPARE(r->currentLayer(), m_layer1);
    // Said with its place among the layers.
    QVERIFY(r->wherePhrase().startsWith("Layer 1 of 2: Fronts"));
    // Walking has no GO: LB + A keeps, it sends nothing to run().
    hold(kLB, true);
    press(kA);
    hold(kLB, false);
    QVERIFY(m_runs.isEmpty());
    QVERIFY(!run->isArmed());

    // Menu again: the walk ends, the Run map is back.
    said.clear();
    press(kMenu);
    QVERIFY(!run->isWalking());
    QVERIFY(!r->isActive());
    QVERIFY(!r->isWalking());
    QCOMPARE(walking.count(), 2);
    QVERIFY(said.count() > 0);
    QVERIFY(said.last().at(0).toString().contains("Walk off"));
    arm(true);
    with(kLT, kA);
    QCOMPARE(m_runs.size(), 1);
    // Armed, Menu walks nothing: the pad is the cue map until disarmed.
    press(kMenu);
    QVERIFY(!run->isWalking());
    QVERIFY(run->isArmed());
    QVERIFY(run->notice().contains("disarm first"));
    QString armedWhy;
    QVERIFY(!run->startWalk(0xFFFFFFFF, &armedWhy));
    QVERIFY(armedWhy.contains("disarm first"));
    arm(false);

    // Ended from the laptop (Focus this cue closed): the walk ends too.
    QVERIFY(run->startWalk());
    r->stop();
    QVERIFY(!run->isWalking());

    // Operate: the cue its list runs; with none running, refused and said.
    m_doc->setMode(Doc::Operate);
    QString why;
    QVERIFY(!run->startWalk(0xFFFFFFFF, &why));
    QVERIFY(why.contains("GO first"));
    m_doc->setMode(Doc::Design);
}

void PadRun_Test::theTriggersWalkTheCuesWithoutGo()
{
    IOPluginStub *stub = loadStub();
    QVERIFY(stub != nullptr);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    RefineSession *r = pc->refineSession();
    ShowTreeController *st = m_doc->showTree();
    run->setEnabled(true);
    press(kMenu);
    QCOMPARE(run->walkCue(), m_cue1);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    // RT: the next cue it can walk (Blackout has no layer: passed over),
    // previewed in Design -- never a GO.
    press(kRT);
    QVERIFY(run->isWalking());
    QCOMPARE(run->walkCue(), m_cue2);
    QCOMPARE(st->previewedCue(), m_cue2);
    QVERIFY(m_runs.isEmpty());
    // The last cue: it stays, and says so.
    said.clear();
    press(kRT);
    QCOMPARE(run->walkCue(), m_cue2);
    QVERIFY(said.count() > 0 && said.last().at(0).toString().contains("Last cue"));
    // LT: back.
    press(kLT);
    QCOMPARE(run->walkCue(), m_cue1);
    QVERIFY(m_runs.isEmpty());

    // A change not kept: the step is refused, and the change stays.
    QVERIFY(r->nudgeZoom(-5.0f));
    press(kRT);
    QCOMPARE(run->walkCue(), m_cue1);
    QVERIFY(r->hasPending());
    // ... and Menu does not throw it away either.
    press(kMenu);
    QVERIFY(run->isWalking());
    r->revert();

    // Operate: the cue moves with GO only (quiet Refine).
    m_doc->setMode(Doc::Operate);
    QVERIFY(!run->stepCue(1));
    m_doc->setMode(Doc::Design);
    run->stopWalk();
}

void PadRun_Test::theBumpersWalkTheLayers()
{
    IOPluginStub *stub = loadStub();
    QVERIFY(stub != nullptr);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    RefineSession *r = pc->refineSession();
    run->setEnabled(true);
    press(kMenu);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    press(kRB);
    QCOMPARE(r->currentLayer(), m_layer2);
    QVERIFY(said.count() > 0);
    QCOMPARE(said.last().at(0).toString(), QString("Layer 2 of 2: Backs."));
    press(kRB);
    QCOMPARE(r->currentLayer(), m_layer1);      // wraps
    press(kLB);
    QCOMPARE(r->currentLayer(), m_layer2);

    // A tap of Y: the scope (Fixture: one group only); the bumpers then
    // step the fixtures, as in Focus this cue.
    press(kY);
    QCOMPARE(r->scope(), RefineSession::FixtureScope);
    QCOMPARE(r->currentFixture(), m_backs.at(0));
    press(kRB);
    QCOMPARE(r->currentFixture(), m_backs.at(1));
    QCOMPARE(r->currentLayer(), m_layer2);
    QVERIFY(!m_doc->showTree()->isCueNeedsWork(m_cue1));   // a tap is not the tag
    press(kY);
    QCOMPARE(r->scope(), RefineSession::LayerScope);
    run->stopWalk();
}

void PadRun_Test::soloAndMixedOutput()
{
    IOPluginStub *stub = loadStub();
    QVERIFY(stub != nullptr);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    ShowTreeController *st = m_doc->showTree();
    run->setEnabled(true);
    press(kMenu);

    const quint32 frontScene = st->layerScene(m_cue1, m_frontTarget);
    const quint32 backScene = st->layerScene(m_cue1, m_backTarget);
    QVERIFY(frontScene != Function::invalidId() && backScene != Function::invalidId());
    QVERIFY(frontScene != backScene);
    // Mixed: the whole cue plays.
    QVERIFY(st->previewFunctions().contains(frontScene));
    QVERIFY(st->previewFunctions().contains(backScene));

    // View: solo -- only the layer walked.
    press(kView);
    QVERIFY(run->isSolo());
    QCOMPARE(st->previewSoloLayer(), m_layer1);
    QCOMPARE(st->previewFunctions(), QList<quint32>({ frontScene }));
    // The solo follows the layer.
    press(kRB);
    QCOMPARE(st->previewFunctions(), QList<quint32>({ backScene }));
    // ... and the cue.
    press(kRT);
    QCOMPARE(run->walkCue(), m_cue2);
    QCOMPARE(st->previewFunctions(), QList<quint32>({ st->layerScene(m_cue2, m_frontTarget) }));
    press(kLT);

    // View again: the whole cue.
    press(kView);
    QVERIFY(!run->isSolo());
    QCOMPARE(st->previewSoloLayer(), ShowTree::invalidId());
    QVERIFY(st->previewFunctions().contains(frontScene));
    QVERIFY(st->previewFunctions().contains(backScene));

    // Leaving the walk soloed puts the whole cue back.
    press(kView);
    QVERIFY(run->isSolo());
    press(kMenu);
    QVERIFY(!run->isSolo());
    QCOMPARE(st->previewSoloLayer(), ShowTree::invalidId());

    // Operate: nothing goes dark under a running show.
    QVERIFY(run->startWalk(m_cue1));
    m_doc->setMode(Doc::Operate);
    if (run->isWalking())
        QVERIFY(!run->setSolo(true));
    m_doc->setMode(Doc::Design);
}

void PadRun_Test::anAdjustUndoes()
{
    IOPluginStub *stub = loadStub();
    QVERIFY(stub != nullptr);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    RefineSession *r = pc->refineSession();
    ShowTreeController *st = m_doc->showTree();
    run->setEnabled(true);
    press(kMenu);
    const QList<ShowTree::Look> before = st->cue(m_cue1)->layer(m_layer1)->looks;

    // D-pad up: zoom in, pending on the rig; A keeps it in the cue.
    press(kUp);
    QVERIFY(r->hasPending());
    press(kA);
    QVERIFY(!r->hasPending());
    QVERIFY(st->cue(m_cue1)->layer(m_layer1)->looks != before);

    // View + B: one undo step takes it back (View alone would solo).
    hold(kView, true);
    press(kB);
    hold(kView, false);
    QCOMPARE(st->cue(m_cue1)->layer(m_layer1)->looks, before);
    QVERIFY(!run->isSolo());
    QVERIFY(run->isWalking());
    run->stopWalk();
}

void PadRun_Test::holdYTagsTheCue()
{
    IOPluginStub *stub = loadStub();
    QVERIFY(stub != nullptr);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    RefineSession *r = pc->refineSession();
    ShowTreeController *st = m_doc->showTree();
    run->setEnabled(true);
    press(kMenu);
    QSignalSpy rumble(pc, &ProgrammerController::padRumbled);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    // Held: the tag, and no scope change on the release.
    hold(kY, true);
    run->walkTick(PadRun::tagHoldSeconds() / 2);
    QVERIFY(!st->isCueNeedsWork(m_cue1));
    run->walkTick(PadRun::tagHoldSeconds());
    QVERIFY(st->isCueNeedsWork(m_cue1));
    hold(kY, false);
    QCOMPARE(r->scope(), RefineSession::LayerScope);
    QVERIFY(rumble.count() > 0);
    QVERIFY(said.last().at(0).toString().contains("needs work"));
    QCOMPARE(st->needsWorkCues(), QList<quint32>({ m_cue1 }));

    // A tagged cue says so when the walk comes to it.
    press(kRT);
    press(kLT);
    QVERIFY(said.last().at(0).toString().contains("Needs work"));

    // The tag is a tree step: View + B takes it back.
    hold(kView, true);
    press(kB);
    hold(kView, false);
    QVERIFY(!st->isCueNeedsWork(m_cue1));

    // Held again, twice: on, then off.
    QVERIFY(run->toggleNeedsWork());
    QVERIFY(st->isCueNeedsWork(m_cue1));
    QVERIFY(run->toggleNeedsWork());
    QVERIFY(!st->isCueNeedsWork(m_cue1));
    run->stopWalk();
}

void PadRun_Test::theTagSavesAndLoads()
{
    makeShow();
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setCuesNeedsWork({ m_cue2 }, true));
    QVERIFY(!st->setCuesNeedsWork({ m_cue2 }, true));      // nothing changed
    QCOMPARE(st->needsWorkCues(), QList<quint32>({ m_cue2 }));
    const QByteArray xml = showXml(m_doc);
    QVERIFY(xml.contains("NeedsWork=\"1\""));

    Doc *other = new Doc(this);
    QXmlStreamReader sr(xml);
    sr.readNextStartElement();
    QVERIFY(other->loadXML(sr, false));
    QVERIFY(other->showTree()->isCueNeedsWork(m_cue2));
    QVERIFY(!other->showTree()->isCueNeedsWork(m_cue1));
    QCOMPARE(other->showTree()->needsWorkCues(), QList<quint32>({ m_cue2 }));
    delete other;

    // Cleared, it is not written at all (an older build sees nothing new).
    QVERIFY(st->setCuesNeedsWork({ m_cue2 }, false));
    QVERIFY(!showXml(m_doc).contains("NeedsWork"));
}

/*****************************************************************************
 * The walk's leftovers: level and the light itself
 *****************************************************************************/

namespace
{
/* The pan a palette sends @p fid, 16 bit, 0 when none. */
int panOf(Doc *doc, QLCPalette *p, const QList<quint32> &lit, quint32 fid)
{
    int v = 0;
    for (const SceneValue &sv : p->valuesFromFixtures(doc, lit))
        if (sv.fxi == fid && sv.channel == 0)
            v |= sv.value << 8;
        else if (sv.fxi == fid && sv.channel == 1)
            v |= sv.value;
    return v;
}
}

quint32 PadRun_Test::aimFronts()
{
    MonitorProperties *props = m_doc->monitorProperties();
    int x = 0;
    for (quint32 fid : m_fronts + m_backs)
        props->setFixturePosition(fid, 0, 0, QVector3D(float(x++) * 1000.0f, 2000.0f, 5.0f));
    StageTarget *dsc = props->addStageTarget();
    dsc->setName("DSC");
    dsc->setPosition(QVector3D(5.0f, 6.0f, 0.0f));
    ShowTreeController *st = m_doc->showTree();
    for (quint32 cue : { m_cue1, m_cue2 })
    {
        QLCPalette *aim = new QLCPalette(QLCPalette::Aim);
        aim->setName(cue == m_cue1 ? "Aim DSC" : "Aim DSC again");
        aim->setStageTargetId(dsc->id());
        m_doc->addPalette(aim);
        quint32 front = ShowTree::invalidId();
        for (const ShowTree::Layer &l : st->cue(cue)->layers)
            if (l.target == m_frontTarget)
                front = l.id;
        st->setLayerLook(cue, front, lookOf(ShowTree::KeyPosition, aim->id()));
    }
    st->clearUndo();
    return dsc->id();
}

void PadRun_Test::levelFromTheTriggers_data()
{
    QTest::addColumn<bool>("dualSense");
    QTest::newRow("xbox") << false;
    QTest::newRow("dualsense") << true;
}

void PadRun_Test::levelFromTheTriggers()
{
    QFETCH(bool, dualSense);
    IOPluginStub *stub = loadStub();
    QVERIFY(stub != nullptr);
    // A DualSense lists its controls in another order: the same roles.
    quint32 lt = kLT, lb = kLB, a = kA, view = kView, b = kB, menu = kMenu, rb = kRB;
    if (dualSense)
    {
        lt = 30; lb = 31; a = 32; view = 33; b = 34; menu = 35; rb = 36;
        stub->m_channelRoles = { { lt, "lt" }, { 31, "lb" }, { 32, "a" }, { 33, "back" },
                                 { 34, "b" }, { 35, "start" }, { 36, "rb" }, { kRT, "rt" } };
    }
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    RefineSession *r = pc->refineSession();
    ShowTreeController *st = m_doc->showTree();
    run->setEnabled(true);
    press(menu);
    QVERIFY(run->isWalking());
    QCOMPARE(r->currentLayer(), m_layer1);
    const QList<ShowTree::Look> before = st->cue(m_cue1)->layer(m_layer1)->looks;
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    // LB held + LT pulled all the way for a second: half the range down,
    // from Full -- and no cue step.
    hold(lb, true);
    QVERIFY(run->isModifierHeld());
    axis(lt, 255);
    run->walkTick(1.0);
    QCOMPARE(run->walkCue(), m_cue1);
    int level = -1;
    QVERIFY(r->pendingLevel(m_fronts.at(0), level));
    QCOMPARE(level, 128);
    QVERIFY(!r->pendingLevel(m_backs.at(0), level));
    // On the rig: the Fronts' dimmer (channel 4).
    const QHash<QPair<int, int>, uchar> w = r->pendingWrites();
    const Fixture *f0 = m_doc->fixture(m_fronts.at(0));
    QCOMPARE(int(w.value(qMakePair(0, int(f0->address()) + 4))), 128);
    // Let go of the trigger: the level is said.
    axis(lt, 0);
    run->walkTick(0.02);
    QVERIFY(said.count() > 0);
    QCOMPARE(said.last().at(0).toString(), QString("Level 50%."));
    hold(lb, false);
    // LB was the modifier, not a tap: the layer stays.
    QCOMPARE(r->currentLayer(), m_layer1);
    QVERIFY(!run->isModifierHeld());

    // A keeps it in the cue: the layer's own Dimmer look, one tree step.
    press(a);
    QVERIFY(!r->hasPending());
    const ShowTree::Look *dim = st->cue(m_cue1)->layer(m_layer1)->look(ShowTree::KeyDimmer);
    QVERIFY(dim != nullptr);
    QCOMPARE(m_doc->palette(dim->palettes.first())->value().toInt(), 128);
    QCOMPARE(st->undoText(), QString("Focus this cue"));

    // View + B: undone.
    hold(view, true);
    press(b);
    hold(view, false);
    QCOMPARE(st->cue(m_cue1)->layer(m_layer1)->looks, before);

    // A tap of LB on its own still steps back a layer (on its release).
    press(rb);
    QCOMPARE(r->currentLayer(), m_layer2);
    press(lb);
    QCOMPARE(r->currentLayer(), m_layer1);
    run->stopWalk();
    QVERIFY(!run->isWalking());
}

void PadRun_Test::theLightItselfOnItsTarget()
{
    IOPluginStub *stub = loadStub();
    QVERIFY(stub != nullptr);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    aimFronts();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    RefineSession *r = pc->refineSession();
    ShowTreeController *st = m_doc->showTree();
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 f0 = m_fronts.at(0), f1 = m_fronts.at(1);
    QLCPalette *aim2 = m_doc->palette(st->cue(m_cue2)->layers.first().look(ShowTree::KeyPosition)->palettes.first());
    QVERIFY(aim2 != nullptr && aim2->type() == QLCPalette::Aim);
    const int cue2Pan0 = panOf(m_doc, aim2, m_fronts, f0);
    const int cue2Pan1 = panOf(m_doc, aim2, m_fronts, f1);
    QVERIFY(cue2Pan0 > 0);
    const QList<ShowTree::Look> cue1Looks = st->cue(m_cue1)->layer(m_layer1)->looks;

    run->setEnabled(true);
    press(kMenu);
    press(kY);                                  // Fixture scope: Front 1
    QCOMPARE(r->scope(), RefineSession::FixtureScope);
    QCOMPARE(r->currentFixture(), f0);

    // Hold LB, stick right for half a second: the light itself turns.
    hold(kLB, true);
    QVERIFY(r->isLightMode());
    axis(kLX, 255);
    r->tick(0.5);
    axis(kLX, 128);
    QVERIFY(r->hasLightPending());
    QCOMPARE(r->lightPendingFixture(), f0);
    const float offset = props->fixtureRigProps(f0).panOffsetDeg;
    QVERIFY2(offset > 1.0f, qPrintable(QString::number(offset)));
    // The cue itself is not changed; the rig shows where it lands now.
    QCOMPARE(st->cue(m_cue1)->layer(m_layer1)->looks, cue1Looks);
    QVERIFY(r->pendingWrites().contains(qMakePair(0, int(m_doc->fixture(f0)->address()))));
    QVERIFY(!st->canUndo());
    // A change not kept: no cue step, no leaving.
    press(kRT);                                 // LB held: a level, not a cue
    QCOMPARE(run->walkCue(), m_cue1);

    // LB + A keeps it: one Show Tree step.
    press(kA);
    hold(kLB, false);
    QCOMPARE(r->currentFixture(), f0);          // LB was no tap
    QVERIFY(!r->hasLightPending());
    QVERIFY(!r->hasPending());
    QCOMPARE(props->fixtureRigProps(f0).panOffsetDeg, offset);
    QVERIFY(st->canUndo());
    QCOMPARE(st->undoText(), QString("Zero Front 1 on its target"));
    QCOMPARE(st->cue(m_cue1)->layer(m_layer1)->looks, cue1Looks);

    // Every cue aimed with it uses it: cue 2's own Aim palette moved Front 1
    // (by the offset) and left Front 2 alone.
    const int cue2Now0 = panOf(m_doc, aim2, m_fronts, f0);
    QVERIFY2(cue2Now0 > cue2Pan0, qPrintable(QString("%1 %2").arg(cue2Now0).arg(cue2Pan0)));
    QCOMPARE(panOf(m_doc, aim2, m_fronts, f1), cue2Pan1);

    // View + B: one undo puts the calibration back; redo moves it again.
    hold(kView, true);
    press(kB);
    hold(kView, false);
    QCOMPARE(props->fixtureRigProps(f0).panOffsetDeg, 0.0f);
    QCOMPARE(panOf(m_doc, aim2, m_fronts, f0), cue2Pan0);
    QVERIFY(st->redo());
    QCOMPARE(props->fixtureRigProps(f0).panOffsetDeg, offset);
    run->stopWalk();

    // No venue applied: it is the show's own calibration, saved and loaded.
    const QByteArray xml = showXml(m_doc);
    Doc *other = new Doc(this);
    QXmlStreamReader sr(xml);
    sr.readNextStartElement();
    QVERIFY(other->loadXML(sr, false));
    QCOMPARE(other->monitorProperties()->fixtureRigProps(f0).panOffsetDeg, offset);
    QCOMPARE(other->monitorProperties()->fixtureRigProps(f1).panOffsetDeg, 0.0f);
    delete other;
}

void PadRun_Test::theLightItselfIsRefused()
{
    IOPluginStub *stub = loadStub();
    QVERIFY(stub != nullptr);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    makeShow();
    aimFronts();
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    RefineSession *r = pc->refineSession();
    MonitorProperties *props = m_doc->monitorProperties();
    QSignalSpy said(r, &RefineSession::announce);
    run->setEnabled(true);
    press(kMenu);

    // Layer scope: pick the light first.
    r->setLightMode(true);
    QVERIFY(!r->nudgeLight(1.0f, 0.0f));
    QVERIFY(said.last().at(0).toString().contains("Pick the light first"));
    // Said once while the stick is held, not every tick.
    const int n = said.count();
    QVERIFY(!r->nudgeLight(1.0f, 0.0f));
    QCOMPARE(said.count(), n);
    // Without the modifier an aimed layer still asks for a fixture -- and
    // says how to zero a light while walking.
    r->setLightMode(false);
    QVERIFY(!r->nudgePosition(1.0f, 0.0f));
    QVERIFY(said.last().at(0).toString().contains("hold LB"));

    // A light the cue does not aim (the Backs: no position).
    press(kRB);
    press(kY);
    QCOMPARE(r->currentFixture(), m_backs.at(0));
    QVERIFY(!r->nudgeLight(1.0f, 0.0f));
    QVERIFY(said.last().at(0).toString().contains("not aimed at a target"));
    press(kY);                                  // back to Layer scope
    press(kRB);                                 // the Fronts
    press(kY);
    QCOMPARE(r->currentFixture(), m_fronts.at(0));

    // B puts a light not kept back.
    QVERIFY(r->nudgeLight(2.0f, 0.0f));
    QCOMPARE(props->fixtureRigProps(m_fronts.at(0)).panOffsetDeg, 2.0f);
    press(kB);
    QVERIFY(!r->hasLightPending());
    QCOMPARE(props->fixtureRigProps(m_fronts.at(0)).panOffsetDeg, 0.0f);
    QVERIFY(!m_doc->showTree()->canUndo());
    // Stopping puts it back too.
    QVERIFY(r->nudgeLight(2.0f, 0.0f));
    r->revert();
    run->stopWalk();
    QCOMPARE(props->fixtureRigProps(m_fronts.at(0)).panOffsetDeg, 0.0f);

    // Operate (quiet Refine): only while its cue runs.
    quint32 running = m_cue1;
    r->setRunningCueProbe([&running](quint32) { return running; });
    m_doc->setMode(Doc::Operate);
    QVERIFY(r->start(m_cue1));
    r->setScope(RefineSession::FixtureScope);
    QVERIFY(r->nudgeLight(1.0f, 0.0f));
    QVERIFY(r->keepLight());
    QCOMPARE(props->fixtureRigProps(m_fronts.at(0)).panOffsetDeg, 1.0f);
    running = m_cue2;                           // GO moved on: blind
    QVERIFY(!r->nudgeLight(1.0f, 0.0f));
    QVERIFY(said.last().at(0).toString().contains("not running"));
    QCOMPARE(props->fixtureRigProps(m_fronts.at(0)).panOffsetDeg, 1.0f);
    r->stop();
    m_doc->setMode(Doc::Design);
    r->setRunningCueProbe(std::function<quint32(quint32)>());
}

void PadRun_Test::theLightItselfGoesIntoTheVenue()
{
    makeShow();
    aimFronts();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ProgrammerController *pc = m_doc->programmer();
    VenueCorrections *v = pc->venueCorrections();
    v->setShowFile(dir.filePath("Tour.qlcc"), false);
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    RefineSession *r = pc->refineSession();
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 f0 = m_fronts.at(0);

    QVERIFY(r->start(m_cue1));
    r->setScope(RefineSession::FixtureScope);
    QVERIFY(r->nudgeLight(-1.5f, 0.5f));
    // Live, not a correction yet.
    QVERIFY(!v->hasCorrection(f0));
    QVERIFY(r->keepLight());
    QVERIFY(v->hasCorrection(f0));
    QCOMPARE(v->correction(f0).panOffsetDeg, -1.5f);
    QCOMPARE(v->correction(f0).tiltOffsetDeg, 0.5f);
    QVERIFY(v->isChecked(f0));
    // The show's own calibration is untouched: the base.
    QCOMPARE(props->calibrationBase(f0).panOffsetDeg, 0.0f);
    QVERIFY(!showXml(m_doc).contains("PanOfs"));
    // The sidecar has it, and a reader of it gets it back.
    QVERIFY(QFile::exists(v->filePath()));
    QFile side(v->filePath());
    QVERIFY(side.open(QIODevice::ReadOnly));
    VenueCorrections reread(m_doc);
    QVERIFY(reread.fromXml(side.readAll()));
    QCOMPARE(reread.venue("Lincoln HS").fixtures.value(f0).panOffsetDeg, -1.5f);

    // Undo takes the correction out of the venue again.
    QVERIFY(m_doc->showTree()->undo());
    QVERIFY(!v->hasCorrection(f0));
    QCOMPARE(props->fixtureRigProps(f0).panOffsetDeg, 0.0f);
    r->stop();
}


quint32 PadRun_Test::hitWithLayer(ShowTree::HitScope scope, quint32 placeId, const QString &name)
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 hit = st->addHit(scope, placeId, name);
    const quint32 layer = st->addLayer(hit, m_frontTarget, "Fronts");
    if (!st->setLayerLook(hit, layer, lookOf(ShowTree::KeyDimmer, m_doc->palettes().first()->id())))
        return ShowTree::invalidId();
    return hit;
}

PadRun *PadRun_Test::runPad(bool armed)
{
    IOPluginStub *stub = loadStub();
    if (stub == nullptr || !m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0))
        return nullptr;
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    m_doc->setMode(Doc::Operate);
    if (armed)
        arm(true);
    return run;
}

void PadRun_Test::rtFiresTheArmedHit()
{
    makeShow();
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->productions().first().id;
    const quint32 breathe = hitWithLayer(ShowTree::HitScope::Production, prod, "Breathe");
    PadRun *run = runPad(true);
    QVERIFY2(run != nullptr, "no I/O plugin stub in ../iopluginstub");
    ProgrammerController *pc = m_doc->programmer();
    QSignalSpy fired(run, &PadRun::hitFired);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    QCOMPARE(run->armedHits(), QList<quint32>({ breathe }));
    press(kRT);
    QCOMPARE(fired.count(), 1);
    QCOMPARE(fired.at(0).at(0).toUInt(), breathe);
    QVERIFY(fired.at(0).at(1).toBool());
    QVERIFY(said.last().at(0).toString().contains("Breathe"));
    // Not a run command: the GO path saw nothing.
    QVERIFY(m_runs.isEmpty());
    Function *f = m_doc->function(st->lookFunction(breathe));
    QVERIFY(f != nullptr);
    m_doc->masterTimer()->timerTick();
    QVERIFY(f->isRunning());

    // LT held with it: no hit -- LT + RT is the emergency stop in the making.
    with(kLT, kRT);
    QCOMPARE(fired.count(), 1);
    // Again on its own: it is let go (and said).
    press(kRT);
    QCOMPARE(fired.count(), 2);
    QVERIFY(!fired.at(1).at(1).toBool());
    QVERIFY(said.last().at(0).toString().contains("Breathe off"));
    m_doc->masterTimer()->timerTick();
    QVERIFY(!f->isRunning());
    // Still armed.
    QVERIFY(run->isArmed());
    arm(false);
    m_doc->setMode(Doc::Design);
}

void PadRun_Test::noHitWithoutArmOrAnArmedHit()
{
    makeShow();
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->productions().first().id;
    // A hit on a cue that is not running is not armed.
    const quint32 onCue = st->addHit(ShowTree::HitScope::Cue, m_cue1, "Strobe");
    PadRun *run = runPad(false);
    QVERIFY2(run != nullptr, "no I/O plugin stub in ../iopluginstub");
    ProgrammerController *pc = m_doc->programmer();
    QSignalSpy fired(run, &PadRun::hitFired);
    QSignalSpy refused(run, &PadRun::refused);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QVERIFY(run->armedHits().isEmpty());

    // Not armed: refused, said, nothing starts.
    press(kRT);
    QCOMPARE(refused.count(), 1);
    QCOMPARE(refused.at(0).at(0).value<PadRun::Action>(), PadRun::FireHit);
    QVERIFY(said.last().at(0).toString().contains("LB"));
    QCOMPARE(fired.count(), 0);

    // Armed, no hit armed: said. A bumper is the mode's only: nothing, unsaid.
    arm(true);
    press(kRT);
    QCOMPARE(fired.count(), 0);
    QVERIFY(said.last().at(0).toString().contains("No hit armed"));
    said.clear();
    press(kRB);
    press(kLB);
    QCOMPARE(said.count(), 0);

    // An armed hit in Design: not fired either (as a VC button).
    const quint32 wash = hitWithLayer(ShowTree::HitScope::Production, prod, "Wash");
    m_doc->setMode(Doc::Design);
    press(kRT);
    QCOMPARE(fired.count(), 0);
    QVERIFY(said.last().at(0).toString().contains("Operate"));
    QVERIFY(!m_doc->function(st->lookFunction(wash))->isRunning());
    QVERIFY(!m_doc->function(st->lookFunction(onCue))->isRunning());

    // Disarmed in Operate: nothing again.
    m_doc->setMode(Doc::Operate);
    arm(false);
    press(kRT);
    QCOMPARE(fired.count(), 0);
    arm(true);
    // RT only brushed, or pulled with a bumper down (a chord in the making): nothing.
    axis(kRT, 100);
    axis(kRT, 0);
    hold(kRB, true);
    press(kRT);
    hold(kRB, false);
    QCOMPARE(fired.count(), 0);
    // Pulled: the hit, once a pull however long it is held.
    axis(kRT, 255);
    axis(kRT, 250);
    axis(kRT, 255);
    QCOMPARE(fired.count(), 1);
    QCOMPARE(fired.at(0).at(0).toUInt(), wash);
    axis(kRT, 0);
    QCOMPARE(fired.count(), 1);
    arm(false);
    m_doc->setMode(Doc::Design);
}

void PadRun_Test::theNarrowestArmedHitWins()
{
    makeShow();
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->productions().first().id;
    const quint32 everywhere = hitWithLayer(ShowTree::HitScope::Global, ShowTree::invalidId(), "Blackout");
    const quint32 onProd = hitWithLayer(ShowTree::HitScope::Production, prod, "Wash");
    const quint32 onList = hitWithLayer(ShowTree::HitScope::List, m_list, "Red");
    const quint32 onList2 = hitWithLayer(ShowTree::HitScope::List, m_list, "Blue");
    const quint32 onCue = st->addHit(ShowTree::HitScope::Cue, m_cue1, "Strobe");
    PadRun *run = runPad(true);
    QVERIFY2(run != nullptr, "no I/O plugin stub in ../iopluginstub");
    QSignalSpy fired(run, &PadRun::hitFired);

    // The cue is not running: the list's two are the narrowest armed.
    QCOMPARE(run->armedHits(), QList<quint32>({ onList, onList2 }));
    // Two at one place take turns: Red, then (Red let go first) Blue.
    press(kRT);
    QCOMPARE(fired.last().at(0).toUInt(), onList);
    m_doc->masterTimer()->timerTick();
    press(kRT);                                  // lets Red go
    QVERIFY(!fired.last().at(1).toBool());
    m_doc->masterTimer()->timerTick();
    press(kRT);
    QCOMPARE(fired.last().at(0).toUInt(), onList2);
    m_doc->masterTimer()->timerTick();
    press(kRT);
    QVERIFY(!fired.last().at(1).toBool());
    QVERIFY(onCue != 0 && everywhere != 0 && onProd != 0);

    // Only wider ones: the production's beats everywhere.
    QVERIFY(st->setHitScope({ onList, onList2 }, ShowTree::HitScope::Production, prod));
    QCOMPARE(run->armedHits().size(), 3);
    QVERIFY(!run->armedHits().contains(everywhere));
    QVERIFY(st->setHitScope({ onList, onList2, onProd }, ShowTree::HitScope::Global, ShowTree::invalidId()));
    QCOMPARE(run->armedHits().size(), 4);
    arm(false);
    m_doc->setMode(Doc::Design);
}

void PadRun_Test::theNextHitIsNamedBeforeItFires()
{
    makeShow();
    ShowTreeController *st = m_doc->showTree();
    PadRun *run = runPad(true);
    QVERIFY2(run != nullptr, "no I/O plugin stub in ../iopluginstub");
    MasterTimer *timer = m_doc->masterTimer();
    QSignalSpy changed(run, &PadRun::hitChanged);
    QSignalSpy fired(run, &PadRun::hitFired);
    const QString arrow = QString::fromUtf8("Hit \u25b8 ");

    // No hits at all, armed in Operate: nothing to say.
    QVERIFY(run->hitText().isEmpty());
    QVERIFY(run->armedHitText().isEmpty());
    QCOMPARE(run->nextHit(), ShowTree::invalidId());

    // One hit: its name, no "1 of 1".
    const quint32 breathe = hitWithLayer(ShowTree::HitScope::Production, st->productions().first().id, "Breathe");
    QTRY_VERIFY(changed.count() > 0);
    QCOMPARE(run->hitText(), arrow + "Breathe");
    QCOMPARE(run->armedHitText(), arrow + "Breathe");
    QCOMPARE(run->nextHit(), breathe);

    // Two at a narrower place take turns: the one the next pull fires, and which.
    const quint32 red = hitWithLayer(ShowTree::HitScope::List, m_list, "Red");
    const quint32 blue = hitWithLayer(ShowTree::HitScope::List, m_list, "Blue");
    QTRY_COMPARE(run->hitText(), arrow + "Red (1 of 2)");
    struct Turn { quint32 hit; bool on; const char *text; };
    const Turn turns[] = {
        { red, true, "Red off (1 of 2)" },      // Red on: the next pull lets it go
        { red, false, "Blue (2 of 2)" },        // let go: Blue's turn
        { blue, true, "Blue off (2 of 2)" },
        { blue, false, "Red (1 of 2)" },        // round again
    };
    for (const Turn &t : turns)
    {
        // What was named is what the pull does.
        bool letsGo = false;
        QCOMPARE(run->nextHit(&letsGo), t.hit);
        QCOMPARE(letsGo, !t.on);
        changed.clear();
        press(kRT);
        QCOMPARE(fired.last().at(0).toUInt(), t.hit);
        QCOMPARE(fired.last().at(1).toBool(), t.on);
        timer->timerTick();
        QTRY_COMPARE(run->hitText(), arrow + QString::fromUtf8(t.text));
        QCOMPARE(run->armedHitText(), run->hitText());
        // The surfaces are told (MasterTimer's start / stop, queued here).
        QTRY_VERIFY(changed.count() > 0);
    }

    // Disarmed: the console's surfaces say nothing; what a pull would fire stays known.
    changed.clear();
    arm(false);
    QVERIFY(run->armedHitText().isEmpty());
    QCOMPARE(run->hitText(), arrow + "Red (1 of 2)");
    QVERIFY(changed.count() > 0);
    arm(true);
    QCOMPARE(run->armedHitText(), arrow + "Red (1 of 2)");

    // Design: no hit fires there, so none is named.
    changed.clear();
    m_doc->setMode(Doc::Design);
    QVERIFY(run->hitText().isEmpty());
    QVERIFY(run->armedHitText().isEmpty());
    QVERIFY(changed.count() > 0);
    m_doc->setMode(Doc::Operate);
    QCOMPARE(run->hitText(), arrow + "Red (1 of 2)");

    // A hit renamed: the new name, at once.
    QVERIFY(st->renameNode(red, "Crimson"));
    QCOMPARE(run->hitText(), arrow + "Crimson (1 of 2)");

    // The emergency stop: the arm drops and the hit goes from the console's surfaces.
    hold(kLB, true);
    hold(kRB, true);
    axis(kLT, 255);
    axis(kRT, 255);
    QCOMPARE(run->state(), PadRun::Stopped);
    QVERIFY(run->armedHitText().isEmpty());
    axis(kRT, 0);
    axis(kLT, 0);
    hold(kRB, false);
    hold(kLB, false);
    // (The stop's ALL OFF went to the test's run handler: let the hits go here.)
    for (quint32 h : { breathe, red, blue })
        if (Function *f = m_doc->function(st->lookFunction(h)))
            if (f->isRunning())
                f->stop(FunctionParent::master());
    timer->timerTick();
    m_doc->setMode(Doc::Design);
}

void PadRun_Test::theRoleChordSwitches()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setRoleChordMs(60);
    pc->autoBindFromProfile();
    QSignalSpy changed(run, &PadRun::enabledChanged);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QSignalSpy focusAsked(pc, &ProgrammerController::focusModeRequested);
    QVERIFY(!run->isEnabled());

    // Tools to Run: held, the role switches and says so; nothing else fires.
    hold(kView, true);
    hold(kMenu, true);
    QVERIFY(run->isRoleChordPending());
    QVERIFY(!run->isEnabled());
    QTRY_VERIFY(run->isEnabled());
    QCOMPARE(changed.count(), 1);
    QVERIFY(said.last().at(0).toString().contains("Run role"));
    hold(kMenu, false);
    hold(kView, false);
    QCOMPARE(focusAsked.count(), 0);
    QVERIFY(!run->isWalking());
    QVERIFY(m_runs.isEmpty());

    // Run to Tools, the other order.
    hold(kMenu, true);
    hold(kView, true);
    QTRY_VERIFY(!run->isEnabled());
    QVERIFY(said.last().at(0).toString().contains("Tools"));
    hold(kView, false);
    hold(kMenu, false);
    QCOMPARE(changed.count(), 2);
    QCOMPARE(focusAsked.count(), 0);

    // The Run tab's button follows enabledChanged (the same signal).
    run->setEnabled(false);
}

void PadRun_Test::theRoleChordResistsAccidents()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setRoleChordMs(80);
    pc->autoBindFromProfile();
    QSignalSpy changed(run, &PadRun::enabledChanged);
    QSignalSpy focusAsked(pc, &ProgrammerController::focusModeRequested);

    // A brush: both down, up before the time. No switch; in Tools the
    // short press is Focus mode, on its release, once.
    hold(kView, true);
    hold(kMenu, true);
    hold(kMenu, false);
    hold(kView, false);
    QCOMPARE(focusAsked.count(), 1);
    QTest::qWait(200);
    QVERIFY(!run->isEnabled());
    QCOMPARE(changed.count(), 0);

    // A third button in the hold cancels it.
    hold(kView, true);
    hold(kMenu, true);
    hold(kA, true);
    QVERIFY(!run->isRoleChordPending());
    QTest::qWait(200);
    QVERIFY(!run->isEnabled());
    hold(kA, false);
    hold(kMenu, false);
    hold(kView, false);
    QCOMPARE(focusAsked.count(), 1);        // a cancelled chord is not Focus mode

    // One alone, held long: nothing.
    hold(kView, true);
    QTest::qWait(200);
    hold(kView, false);
    hold(kMenu, true);
    QTest::qWait(200);
    hold(kMenu, false);
    QVERIFY(!run->isEnabled());
    QCOMPARE(changed.count(), 0);

    // In the Run role a brush does nothing at all: no walk, no Focus mode.
    run->setEnabled(true);
    m_doc->setMode(Doc::Design);
    hold(kView, true);
    hold(kMenu, true);
    hold(kMenu, false);
    hold(kView, false);
    QTest::qWait(150);
    QVERIFY(run->isEnabled());
    QVERIFY(!run->isWalking());
    QCOMPARE(focusAsked.count(), 1);
    run->setEnabled(false);
}

namespace
{
/* Fails the run, fast, when the GUI thread is stuck: a deadlock in the input
   path would otherwise hang the test (and the gate) until it is killed. */
class InputWatchdog
{
public:
    InputWatchdog(int ms, const char *what)
    {
        m_thread = QThread::create([this, ms, what]() {
            if (m_done.tryAcquire(1, ms))
                return;
            std::fprintf(stderr, "FAIL!  : %s: the GUI thread is still blocked after %d ms "
                                 "(a deadlock in the input path)\n", what, ms);
            std::fflush(stderr);
            std::_Exit(3);
        });
        m_thread->start();
    }
    ~InputWatchdog()
    {
        m_done.release();
        m_thread->wait();
        delete m_thread;
    }

private:
    QSemaphore m_done;
    QThread *m_thread = nullptr;
};
}

void PadRun_Test::padPress(IOPluginStub *stub, quint32 line, quint32 channel)
{
    stub->emitValueChanged(UINT_MAX, line, channel, 255);
    stub->emitValueChanged(UINT_MAX, line, channel, 0);
    m_doc->inputOutputMap()->flushInputs();
}

void PadRun_Test::padHold(IOPluginStub *stub, quint32 line, quint32 channel, bool down)
{
    stub->emitValueChanged(UINT_MAX, line, channel, down ? 255 : 0);
    m_doc->inputOutputMap()->flushInputs();
}

void PadRun_Test::inputThroughThePatchDoesNotDeadlock()
{
    InputWatchdog watchdog(20000, "PadRun_Test::inputThroughThePatchDoesNotDeadlock");

    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    makeShow();
    ShowTreeController *st = m_doc->showTree();
    const quint32 breathe = hitWithLayer(ShowTree::HitScope::Production,
                                         st->productions().first().id, "Breathe");

    /* Two pads in one controller slot, the DualSense's controls on other
       channels: the role lookup goes through the patch's channel map. */
    const QString xbox = QStringLiteral("Xbox Wireless Controller");
    const QString dualSense = QStringLiteral("DualSense Wireless Controller");
    const quint32 xA = 6, xLB = 8, xRB = 10, xMenu = 11, xLT = 13, xRT = 14;
    const quint32 dA = 7, dLB = 9, dRB = 11, dMenu = 10;
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << xbox << dualSense;
    const QMap<quint32, QString> xroles = { { 6, "a" }, { 7, "b" }, { 8, "lb" }, { 9, "x" },
                                            { 10, "rb" }, { 11, "start" }, { 12, "back" }, { 13, "lt" },
                                            { 14, "rt" } };
    const QMap<quint32, QString> droles = { { 6, "x" }, { 7, "a" }, { 8, "b" }, { 9, "lb" },
                                            { 10, "start" }, { 11, "rb" }, { 12, "back" }, { 13, "lt" },
                                            { 14, "rt" } };
    stub->m_lineChannelRoles[0] = xroles;
    stub->m_lineChannelRoles[1] = droles;
    stub->m_lineDeviceKind[0] = "xbox";
    stub->m_lineDeviceKind[1] = "dualsense";
    stub->m_buttonActions = { { xA, "chaser_step_forward" } };

    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    QVERIFY(iom->addInputSlotDevice(0, stub->name(), dualSense));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    // The controller is read (in the app the auto-bind at start sees to it).
    run->setEnabled(true);
    run->setEnabled(false);

    // The Tools role: A is the profile's button action, as it always was.
    QSignalSpy actions(pc, &ProgrammerController::buttonActionTriggered);
    padPress(stub, 0, xA);
    QCOMPARE(actions.count(), 1);
    QCOMPARE(actions.at(0).at(0).toString(), QString("chaser_step_forward"));
    QVERIFY(m_runs.isEmpty());

    /* The Run role: the chord arms (and disarms), then LT held + A is one
       GO through the pad's route. */
    run->setEnabled(true);
    run->setArmChordMs(15);
    auto chord = [&](bool on) {
        padHold(stub, 0, xLB, true);
        padHold(stub, 0, xRB, true);
        QTRY_COMPARE(run->isArmed(), on);
        padHold(stub, 0, xRB, false);
        padHold(stub, 0, xLB, false);
    };
    chord(true);
    QVERIFY(run->isArmed());
    padHold(stub, 0, xLT, true);
    padPress(stub, 0, xA);
    padHold(stub, 0, xLT, false);
    chord(false);
    QCOMPARE(m_runs.size(), 1);
    QCOMPARE(m_runs.at(0).first.command, ShowControl::Go);
    QCOMPARE(m_runs.at(0).second, ShowControl::RoutePad);
    QCOMPARE(actions.count(), 1);          // no profile action beside it

    // A walk: Menu starts Focus this cue on the cue in hand, Menu ends it.
    padPress(stub, 0, xMenu);
    QVERIFY(run->isWalking());
    padPress(stub, 0, xMenu);
    QVERIFY(!run->isWalking());

    // A hit: armed, RT fires the armed one, in Operate.
    m_doc->setMode(Doc::Operate);
    QSignalSpy fired(run, &PadRun::hitFired);
    chord(true);
    padPress(stub, 0, xRT);
    QCOMPARE(fired.count(), 1);
    QCOMPARE(fired.at(0).at(0).toUInt(), breathe);
    padPress(stub, 0, xRT);                 // and lets it go
    QCOMPARE(fired.count(), 2);
    chord(false);
    m_doc->setMode(Doc::Design);

    // The Xbox pad gone: the DualSense on its line, its own channels mapped.
    stub->m_lineNames = QStringList() << dualSense;
    stub->m_lineChannelRoles.clear();
    stub->m_lineChannelRoles[0] = droles;
    stub->m_lineDeviceKind.clear();
    stub->m_lineDeviceKind[0] = "dualsense";
    stub->configure();
    QCOMPARE(iom->inputSlotLive(0), 1);
    QVERIFY(!iom->inputPatch(0)->channelMap().isEmpty());

    // The DualSense's own chord and trigger: the same GO.
    padHold(stub, 0, dLB, true);
    padHold(stub, 0, dRB, true);
    QTRY_VERIFY(run->isArmed());
    padHold(stub, 0, dRB, false);
    padHold(stub, 0, dLB, false);
    padHold(stub, 0, xLT, true);            // L2: the same channel on both here
    padPress(stub, 0, dA);
    padHold(stub, 0, xLT, false);
    padHold(stub, 0, dLB, true);
    padHold(stub, 0, dRB, true);
    QTRY_VERIFY(!run->isArmed());
    padHold(stub, 0, dRB, false);
    padHold(stub, 0, dLB, false);
    QCOMPARE(m_runs.size(), 2);
    QCOMPARE(m_runs.at(1).first.command, ShowControl::Go);
    padPress(stub, 0, dMenu);
    QVERIFY(run->isWalking());
    padPress(stub, 0, dMenu);
    QVERIFY(!run->isWalking());
    // The hit on the DualSense: armed (its own bumpers), R2.
    m_doc->setMode(Doc::Operate);
    padHold(stub, 0, dLB, true);
    padHold(stub, 0, dRB, true);
    QTRY_VERIFY(run->isArmed());
    padHold(stub, 0, dRB, false);
    padHold(stub, 0, dLB, false);
    padPress(stub, 0, xRT);
    QCOMPARE(fired.count(), 3);
    padPress(stub, 0, xRT);
    QCOMPARE(fired.count(), 4);
    padHold(stub, 0, dLB, true);
    padHold(stub, 0, dRB, true);
    QTRY_VERIFY(!run->isArmed());
    padHold(stub, 0, dRB, false);
    padHold(stub, 0, dLB, false);
    m_doc->setMode(Doc::Design);

    // Back to the Tools role: nothing of the Run map fires, nothing locks.
    run->setEnabled(false);
    padPress(stub, 0, dA);
    QCOMPARE(m_runs.size(), 2);

    run->setEnabled(false);
    stub->m_linesAreHardware = false;
    stub->m_lineNames.clear();
    stub->m_lineChannelRoles.clear();
    stub->m_lineDeviceKind.clear();
    stub->m_buttonActions.clear();
}

/*****************************************************************************
 * Adaptive triggers and swipes (Q199)
 *****************************************************************************/

void PadRun_Test::theTriggerFeelByState()
{
    using T = PadTriggers;
    // The role off: always off.
    QCOMPARE(T::forState(false, true, true), T::State());
    QCOMPARE(T::forState(true, false, false), T::State());
    // Armed: both firm (each is held for a GO or a Sub GO).
    QCOMPARE(T::forState(true, true, false).right, T::Firm);
    QCOMPARE(T::forState(true, true, false).left, T::Firm);
    // Walking: both click, the arm or not.
    QCOMPARE(T::forState(true, false, true).right, T::Click);
    QCOMPARE(T::forState(true, true, true).left, T::Click);
    for (T::Feel f : { T::Off, T::Firm, T::Click })
        QCOMPARE(T::fromName(T::name(f)), f);
    QCOMPARE(T::fromName("nonsense"), T::Off);
    QCOMPARE(T::fromName(nullptr), T::Off);
}

void PadRun_Test::theTriggersFollowTheArm()
{
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    IOPluginStub *stub = loadStub();
    QVERIFY(stub != nullptr);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    PadRun *run = pc->padRun();
    QSignalSpy told(pc, &ProgrammerController::padTriggersChanged);

    // Tools role: nothing is sent for an arm that is not one.
    run->setArmChordMs(15);
    hold(kLB, true);
    hold(kRB, true);
    QTest::qWait(60);
    hold(kRB, false);
    hold(kLB, false);
    QCOMPARE(pc->padTriggers(), PadTriggers::State());

    run->setEnabled(true);
    told.clear();
    // Armed: both firm, for as long as it is armed; disarmed: off.
    arm(true);
    QCOMPARE(pc->padTriggers().right, PadTriggers::Firm);
    QCOMPARE(pc->padTriggers().left, PadTriggers::Firm);
    arm(false);
    QCOMPARE(pc->padTriggers(), PadTriggers::State());

    // Walking: both click; leaving the walk: off.
    press(kMenu);
    QVERIFY(run->isWalking());
    QCOMPARE(pc->padTriggers().right, PadTriggers::Click);
    QCOMPARE(pc->padTriggers().left, PadTriggers::Click);
    press(kMenu);
    QVERIFY(!run->isWalking());
    QCOMPARE(pc->padTriggers(), PadTriggers::State());
    // The walk ended from the laptop.
    QVERIFY(run->startWalk());
    QCOMPARE(pc->padTriggers().right, PadTriggers::Click);
    pc->refineSession()->stop();
    QCOMPARE(pc->padTriggers(), PadTriggers::State());

    // Walking, then the role leaves: off.
    QVERIFY(run->startWalk());
    QCOMPARE(pc->padTriggers().left, PadTriggers::Click);
    run->setEnabled(false);
    QCOMPARE(pc->padTriggers(), PadTriggers::State());
}

void PadRun_Test::swipeAcross(float x0, float y0, float x1, float y1)
{
    // As a pad sends them: down at (x0, y0), moving in steps, up.
    auto at = [this](float x, float y) {
        axis(kTouchX, uchar(x * 255.0f));
        axis(kTouchY, uchar(y * 255.0f));
    };
    at(x0, y0);
    axis(kTouchDown, 255);
    PadRun *run = m_doc->programmer()->padRun();
    run->swipeTick(0.02);
    for (int i = 1; i <= 10; ++i)
    {
        at(x0 + (x1 - x0) * i / 10.0f, y0 + (y1 - y0) * i / 10.0f);
        run->swipeTick(0.02);
    }
    axis(kTouchDown, 0);
    run->swipeTick(0.02);
}

void PadRun_Test::oneSwipeIsOneStep()
{
    TouchSwipe s;
    using R = GamepadRole::Role;
    auto finger = [&s](float x, float y, bool down) {
        s.setValue(R::TouchX, x);
        s.setValue(R::TouchY, y);
        s.setValue(R::TouchDown, down ? 1.0f : 0.0f);
    };
    // A right swipe: one, once, however far on; none until the lift.
    finger(0.2f, 0.5f, true);
    QCOMPARE(s.take(0.02), TouchSwipe::None);
    finger(0.35f, 0.5f, true);
    QCOMPARE(s.take(0.02), TouchSwipe::None);       // not far enough
    finger(0.5f, 0.5f, true);
    QCOMPARE(s.take(0.02), TouchSwipe::Right);
    finger(0.9f, 0.5f, true);
    QCOMPARE(s.take(0.02), TouchSwipe::None);
    finger(0.9f, 0.5f, false);
    QCOMPARE(s.take(0.02), TouchSwipe::None);

    // The next one waits out the cooldown, then is swallowed (not queued).
    finger(0.8f, 0.5f, true);
    s.take(0.02);
    finger(0.4f, 0.5f, true);
    QCOMPARE(s.take(0.02), TouchSwipe::None);
    finger(0.4f, 0.5f, false);
    s.take(TouchSwipe::cooldown());
    // After it: left, up, down.
    finger(0.8f, 0.5f, true);
    s.take(0.02);
    finger(0.4f, 0.5f, true);
    QCOMPARE(s.take(0.02), TouchSwipe::Left);
    finger(0.4f, 0.5f, false);
    s.take(TouchSwipe::cooldown());
    finger(0.5f, 0.8f, true);
    s.take(0.02);
    finger(0.5f, 0.4f, true);
    QCOMPARE(s.take(0.02), TouchSwipe::Up);
    finger(0.5f, 0.4f, false);
    s.take(TouchSwipe::cooldown());
    finger(0.5f, 0.2f, true);
    s.take(0.02);
    finger(0.5f, 0.6f, true);
    QCOMPARE(s.take(0.02), TouchSwipe::Down);
    finger(0.5f, 0.6f, false);
    s.take(TouchSwipe::cooldown());

    // A slanted drag is none; a second finger or a click spoils the touch.
    finger(0.2f, 0.2f, true);
    s.take(0.02);
    finger(0.6f, 0.6f, true);
    QCOMPARE(s.take(0.02), TouchSwipe::None);
    finger(0.6f, 0.6f, false);
    s.take(0.02);
    finger(0.2f, 0.5f, true);
    s.take(0.02);
    s.setValue(R::Touch2Down, 1.0f);
    finger(0.7f, 0.5f, true);
    QCOMPARE(s.take(0.02), TouchSwipe::None);
    s.setValue(R::Touch2Down, 0.0f);
    finger(0.7f, 0.5f, false);
    s.take(0.02);
    finger(0.2f, 0.5f, true);
    s.take(0.02);
    s.setClick(true);
    finger(0.7f, 0.5f, true);
    QCOMPARE(s.take(0.02), TouchSwipe::None);
    s.setClick(false);
    // Even after a long wait the spoilt touch gives nothing more.
    QCOMPARE(s.take(1.0), TouchSwipe::None);
}

void PadRun_Test::swipesStepAndNeverGo()
{
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    IOPluginStub *stub = loadStub();
    QVERIFY(stub != nullptr);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    PadRun *run = pc->padRun();
    RefineSession *r = pc->refineSession();

    // Not in the Run role: a swipe does nothing.
    swipeAcross(0.2f, 0.5f, 0.8f, 0.5f);
    QVERIFY(!run->isWalking());
    run->setEnabled(true);

    // Run, not walking, Operate: left / right refused; up / down nothing.
    m_doc->setMode(Doc::Operate);
    swipeAcross(0.2f, 0.5f, 0.8f, 0.5f);
    QVERIFY(!run->isWalking());
    QVERIFY(m_runs.isEmpty());
    run->swipeTick(1.0);
    m_doc->setMode(Doc::Design);

    // Design: a swipe right walks from the next cue (Blackout has no layer:
    // passed over to Ballad), never a GO.
    QCOMPARE(run->cueInHand(), m_cue1);
    swipeAcross(0.2f, 0.5f, 0.8f, 0.5f);
    QVERIFY(run->isWalking());
    QCOMPARE(run->walkCue(), m_cue2);
    QVERIFY(m_runs.isEmpty());
    run->swipeTick(PadRun::tagHoldSeconds());     // the cooldown
    // Left: back to the first cue.
    swipeAcross(0.8f, 0.5f, 0.2f, 0.5f);
    QCOMPARE(run->walkCue(), m_cue1);
    run->swipeTick(1.0);

    // Down / up: the next / previous layer (wrapping).
    QCOMPARE(r->currentLayer(), m_layer1);
    swipeAcross(0.5f, 0.2f, 0.5f, 0.8f);
    QCOMPARE(r->currentLayer(), m_layer2);
    run->swipeTick(1.0);
    swipeAcross(0.5f, 0.8f, 0.5f, 0.2f);
    QCOMPARE(r->currentLayer(), m_layer1);
    run->swipeTick(1.0);
    QVERIFY(m_runs.isEmpty());
    QVERIFY(!run->isArmed());

    // A click while the finger is down is no swipe (a firm press).
    hold(kTouchClick, true);
    swipeAcross(0.2f, 0.5f, 0.8f, 0.5f);
    hold(kTouchClick, false);
    QCOMPARE(run->walkCue(), m_cue1);
    run->swipeTick(1.0);

    // In Operate the cue moves with GO: a left / right swipe is refused,
    // the layers still step.
    m_doc->setMode(Doc::Operate);
    swipeAcross(0.2f, 0.5f, 0.8f, 0.5f);
    QCOMPARE(run->walkCue(), m_cue1);
    QVERIFY(m_runs.isEmpty());
    m_doc->setMode(Doc::Design);
}

// 0.4.0 "DualSense extras for the Run role": while walking, the touchpad aim nudge
// needs LB held (a one-finger drag alone is a swipe, never an aim).
void PadRun_Test::whileWalkingTheTouchpadAimsOnlyWithLBHeld()
{
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    IOPluginStub *stub = loadStub();
    QVERIFY(stub != nullptr);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    PadRun *run = pc->padRun();
    RefineSession *r = pc->refineSession();
    run->setEnabled(true);
    press(kMenu);
    QVERIFY(run->isWalking());
    QVERIFY(!r->hasPending());

    // A slow drag along the pad (below swipe distance per frame), the joystick tick after each.
    auto drag = [&](float x0, float y0, float x1, float y1) {
        axis(kTouchX, uchar(x0 * 255.0f));
        axis(kTouchY, uchar(y0 * 255.0f));
        axis(kTouchDown, 255);
        pc->applyDesignJoystick();
        for (int i = 1; i <= 10; ++i)
        {
            axis(kTouchX, uchar((x0 + (x1 - x0) * i / 10.0f) * 255.0f));
            axis(kTouchY, uchar((y0 + (y1 - y0) * i / 10.0f) * 255.0f));
            pc->applyDesignJoystick();
        }
        axis(kTouchDown, 0);
        pc->applyDesignJoystick();
    };

    // LB not held: nothing is aimed.
    drag(0.3f, 0.5f, 0.45f, 0.5f);
    QVERIFY(!r->hasPending());
    run->swipeTick(1.0);

    // LB held: the same drag nudges the aim.
    hold(kLB, true);
    QVERIFY(run->isModifierHeld());
    drag(0.3f, 0.5f, 0.45f, 0.5f);
    QVERIFY(r->hasPending());
    hold(kLB, false);
    run->swipeTick(1.0);
    run->stopWalk();
}

// "Other pads: nothing changes": a pad whose profile has no touchpad sends no touch
// channels, so whatever arrives on those numbers steps nothing.
void PadRun_Test::aPadWithoutATouchpadGetsNoSwipeSteps()
{
    makeShow();
    ProgrammerController *pc = m_doc->programmer();
    IOPluginStub *stub = loadStub();
    QVERIFY(stub != nullptr);
    QMap<quint32, QString> roles = xboxRoles();
    for (quint32 ch : { kTouchX, kTouchY, kTouchDown, kTouch2Down, kTouchClick })
        roles.remove(ch);
    stub->m_channelRoles = roles;
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    PadRun *run = pc->padRun();
    run->setEnabled(true);

    swipeAcross(0.2f, 0.5f, 0.8f, 0.5f);
    QVERIFY(!run->isWalking());
    press(kMenu);
    QVERIFY(run->isWalking());
    const quint32 cue = run->walkCue();
    swipeAcross(0.2f, 0.5f, 0.8f, 0.5f);
    QCOMPARE(run->walkCue(), cue);
    swipeAcross(0.5f, 0.2f, 0.5f, 0.8f);
    QCOMPARE(pc->refineSession()->currentLayer(), m_layer1);
    QVERIFY(m_runs.isEmpty());
    run->stopWalk();
}

QTEST_MAIN(PadRun_Test)

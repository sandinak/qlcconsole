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

#include <QtTest>
#include <QSignalSpy>

#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "iopluginstub.h"
#include "qlcioplugin.h"
#include "showcontrol.h"
#include "padrun_test.h"
#include "inputpatch.h"
#include "padnames.h"
#include "padrun.h"
#include "chaser.h"
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

namespace
{
/* An Xbox pad as its profile describes it: axes, then buttons. */
const quint32 kLT = 4, kRT = 5, kA = 10, kB = 11, kX = 12, kY = 13, kLB = 14, kRB = 15,
              kView = 16, kMenu = 17, kRS = 19, kUp = 20;

QMap<quint32, QString> xboxRoles()
{
    return { { 0, "lx" }, { 1, "ly" }, { 2, "rx" }, { 3, "ry" }, { kLT, "lt" }, { kRT, "rt" },
             { kA, "a" }, { kB, "b" }, { kX, "x" }, { kY, "y" }, { kLB, "lb" }, { kRB, "rb" },
             { kView, "back" }, { kMenu, "start" }, { kRS, "rs" }, { kUp, "dpad_up" } };
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
    QCOMPARE(xbox.size(), PadRun::walkActions().size() + 1);
    QCOMPARE(xbox.at(0).left(3), QString("RT:"));
    QCOMPARE(ps.at(0).left(3), QString("R2:"));
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

void PadRun_Test::hold(quint32 channel, bool down, quint32 universe)
{
    emit m_doc->inputOutputMap()->inputValueChanged(universe, channel, down ? 255 : 0, QString());
}

void PadRun_Test::theMapByRole()
{
    QCOMPARE(PadRun::actionFor(GamepadRole::LB), PadRun::Arm);
    QCOMPARE(PadRun::actionFor(GamepadRole::A), PadRun::Go);
    QCOMPARE(PadRun::actionFor(GamepadRole::B), PadRun::Back);
    QCOMPARE(PadRun::actionFor(GamepadRole::Y), PadRun::SubGo);
    QCOMPARE(PadRun::actionFor(GamepadRole::X), PadRun::Stop);
    QCOMPARE(PadRun::actionFor(GamepadRole::RightTrigger), PadRun::NextList);
    QCOMPARE(PadRun::actionFor(GamepadRole::LeftTrigger), PadRun::PreviousList);
    QCOMPARE(PadRun::actionFor(GamepadRole::RS), PadRun::WhereAmI);
    QCOMPARE(PadRun::actionFor(GamepadRole::LeftX), PadRun::NoAction);
    QCOMPARE(PadRun::actionFor(GamepadRole::Start), PadRun::Walk);
    QVERIFY(!PadRun::needsArm(PadRun::Walk));
    for (PadRun::Action a : PadRun::actions())
    {
        QCOMPARE(PadRun::actionFor(PadRun::roleFor(a)), a);
        QVERIFY(!PadRun::label(a).isEmpty());
    }
    QVERIFY(PadRun::needsArm(PadRun::Go) && PadRun::needsArm(PadRun::Back)
            && PadRun::needsArm(PadRun::SubGo) && PadRun::needsArm(PadRun::Stop));
    QVERIFY(!PadRun::needsArm(PadRun::NextList) && !PadRun::needsArm(PadRun::WhereAmI));
    QCOMPARE(PadRun::commandFor(PadRun::Back), ShowControl::Back);
    QCOMPARE(PadRun::commandFor(PadRun::Stop), ShowControl::Stop);

    // The map in the pad's own words.
    const QStringList xbox = PadRun::mapLines(PadNames::Xbox);
    const QStringList ps = PadRun::mapLines(PadNames::DualSense);
    QCOMPARE(xbox.size(), PadRun::actions().size());
    QCOMPARE(xbox.at(1), QString("A: GO (LB held)"));
    QCOMPARE(ps.at(0).left(3), QString("L1:"));
    QCOMPARE(ps.at(1), QString("Cross: GO (L1 held)"));
}

void PadRun_Test::goNeedsTheArmHeld()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    PadRun *run = pc->padRun();
    run->setEnabled(true);
    QSignalSpy refused(run, &PadRun::refused);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    // Not armed: nothing goes, and the pad says how.
    press(kA);
    QVERIFY(m_runs.isEmpty());
    QCOMPARE(refused.count(), 1);
    QCOMPARE(refused.at(0).at(0).value<PadRun::Action>(), PadRun::Go);
    QVERIFY(said.last().at(0).toString().contains("LB"));

    // Armed: one GO, through run() with the pad's route.
    QSignalSpy armed(run, &PadRun::armedChanged);
    hold(kLB, true);
    QVERIFY(run->isArmed());
    QCOMPARE(armed.count(), 1);
    press(kA);
    QCOMPARE(m_runs.size(), 1);
    QCOMPARE(m_runs.at(0).first.command, ShowControl::Go);
    QCOMPARE(m_runs.at(0).second, ShowControl::RoutePad);
    QCOMPARE(m_runs.at(0).first.chaser, quint32(UINT_MAX));        // the main list

    press(kB);
    press(kY);
    press(kX);
    QCOMPARE(m_runs.size(), 4);
    QCOMPARE(m_runs.at(1).first.command, ShowControl::Back);
    QCOMPARE(m_runs.at(2).first.command, ShowControl::SubGo);
    QCOMPARE(m_runs.at(3).first.command, ShowControl::Stop);
    for (const auto &r : m_runs)
        QCOMPARE(r.second, ShowControl::RoutePad);

    // A press's release sends nothing; RB is not in the map.
    hold(kA, false);
    press(kRB);
    QCOMPARE(m_runs.size(), 4);

    // Let go of the arm: disarmed.
    hold(kLB, false);
    QVERIFY(!run->isArmed());
    QCOMPARE(armed.count(), 2);
    press(kA);
    QCOMPARE(m_runs.size(), 4);
    QCOMPARE(refused.count(), 2);

    // The arm is per pad: LB held on one pad does not arm another's A.
    hold(kLB, true, 0);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(1, stub->name(), QString(), 1));
    press(kA, 1);
    QCOMPARE(m_runs.size(), 4);
    hold(kLB, false, 0);
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

    // Tools: LB + A is the profile's, never a GO.
    hold(kLB, true);
    press(kA);
    hold(kLB, false);
    press(kRT);
    QVERIFY(m_runs.isEmpty());
    QCOMPARE(actions.count(), 1);
    QCOMPARE(actions.at(0).at(0).toString(), QString("chaser_step_forward"));

    // Run: the GO, and not the profile's action beside it.
    pc->padRun()->setEnabled(true);
    hold(kLB, true);
    press(kA);
    hold(kLB, false);
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
    QMap<quint32, QString> xroles = { { 6, "a" }, { 7, "b" }, { 8, "lb" }, { 9, "x" } };
    QMap<quint32, QString> droles = { { 6, "x" }, { 7, "a" }, { 8, "b" }, { 9, "lb" } };
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

    // The Xbox pad: LB + A, then LB + B.
    InputPatch *ip = iom->inputPatch(0);
    hold(ip->showChannel(8), true);
    press(ip->showChannel(6));
    press(ip->showChannel(7));
    hold(ip->showChannel(8), false);
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

    // L1 + Cross, then L1 + Circle, by the DualSense's own channels: the
    // same two commands.
    ip = iom->inputPatch(0);
    hold(ip->showChannel(9), true);
    press(ip->showChannel(7));
    press(ip->showChannel(8));
    hold(ip->showChannel(9), false);
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

void PadRun_Test::theTriggersPickTheList()
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
    QCOMPARE(run->lists(), QList<quint32>({ c1, c2 }));
    QCOMPARE(run->listChaser(), quint32(UINT_MAX));
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    // RT: the list after the main one (Act 1 plays first).
    QCOMPARE(m_doc->showControl()->mainCueList(), c1);
    press(kRT);
    QCOMPARE(run->listChaser(), c2);
    QCOMPARE(said.last().at(0).toString(), QString("Act 2."));
    // A trigger resting a little in does not step again.
    hold(kRT, true);
    emit m_doc->inputOutputMap()->inputValueChanged(0, kRT, 120, QString());
    emit m_doc->inputOutputMap()->inputValueChanged(0, kRT, 255, QString());
    QCOMPARE(run->listChaser(), c1);                       // one step, wrapped
    hold(kRT, false);

    // LT back, wrapping the other way.
    press(kLT);
    QCOMPARE(run->listChaser(), c2);

    // The commands go to the list picked, by its chaser.
    hold(kLB, true);
    press(kA);
    hold(kLB, false);
    QCOMPARE(m_runs.size(), 1);
    QCOMPARE(m_runs.at(0).first.chaser, c2);

    // RS: the list and what is on standby.
    press(kRS);
    QCOMPARE(said.last().at(0).toString(), QString("Act 2. Next: cue 1."));
    QVERIFY(run->wherePhrase().startsWith("Act 2"));
}

void PadRun_Test::theLightBarAndRumble()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    pc->padRun()->setEnabled(true);
    QSignalSpy rumbles(pc, &ProgrammerController::padRumbled);

    hold(kLB, true);
    QCOMPARE(pc->padLight(), PadRun::armedColour());
    QVERIFY(!stub->m_feedbackParams.isEmpty());
    QCOMPARE(stub->m_feedbackParams.last().toMap().value("gamepad").toString(), QString("light"));

    press(kA);
    QCOMPARE(pc->padLight(), PadRun::wentColour());
    QCOMPARE(rumbles.count(), 1);
    QCOMPARE(rumbles.last().at(1).toInt(), ControllerScheme::rumbleShortMs());
    // Back to the armed colour after the flash.
    QTRY_COMPARE_WITH_TIMEOUT(pc->padLight(), PadRun::armedColour(), PadRun::wentFlashMs() * 5);

    // Nothing to play: the long buzz, no flash.
    m_answer = false;
    press(kA);
    QCOMPARE(rumbles.count(), 2);
    QCOMPARE(rumbles.last().at(1).toInt(), ControllerScheme::rumbleLongMs());
    QCOMPARE(pc->padLight(), PadRun::armedColour());

    hold(kLB, false);
    QVERIFY(!pc->padLight().isValid());
}

void PadRun_Test::aLostPadDisarms()
{
    IOPluginStub *stub = loadStub();
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    pc->padRun()->setEnabled(true);
    hold(kLB, true);
    QVERIFY(pc->padRun()->isArmed());
    emit pc->controllerLost(true);
    QVERIFY(!pc->padRun()->isArmed());
    // Turning the role off disarms too, and its buttons are free again.
    hold(kLB, true);
    QVERIFY(pc->padRun()->isArmed());
    pc->padRun()->setEnabled(false);
    QVERIFY(!pc->padRun()->isArmed());
    press(kA);
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
    hold(kLB, true);
    press(kA);
    hold(kLB, false);
    QCOMPARE(m_runs.size(), 1);

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

QTEST_MAIN(PadRun_Test)

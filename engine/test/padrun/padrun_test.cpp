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
#include "padtriggers.h"
#include "touchswipe.h"
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
#include "monitorproperties.h"
#include "venuecorrections.h"
#include "stagetarget.h"

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

void PadRun_Test::theMapByRole()
{
    QCOMPARE(PadRun::actionFor(GamepadRole::LB), PadRun::Arm);
    QCOMPARE(PadRun::actionFor(GamepadRole::A), PadRun::Go);
    QCOMPARE(PadRun::actionFor(GamepadRole::B), PadRun::Back);
    QCOMPARE(PadRun::actionFor(GamepadRole::Y), PadRun::SubGo);
    QCOMPARE(PadRun::actionFor(GamepadRole::X), PadRun::Stop);
    // The triggers pick no list (0.4.5): nothing in Run.
    QCOMPARE(PadRun::actionFor(GamepadRole::RightTrigger), PadRun::NoAction);
    QCOMPARE(PadRun::actionFor(GamepadRole::LeftTrigger), PadRun::NoAction);
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
    QVERIFY(!PadRun::needsArm(PadRun::WhereAmI));
    QCOMPARE(PadRun::commandFor(PadRun::Back), ShowControl::Back);
    QCOMPARE(PadRun::commandFor(PadRun::Stop), ShowControl::Stop);

    // The map in the pad's own words.
    const QStringList xbox = PadRun::mapLines(PadNames::Xbox);
    const QStringList ps = PadRun::mapLines(PadNames::DualSense);
    QCOMPARE(xbox.size(), PadRun::actions().size());
    QCOMPARE(xbox.at(1), QString("A: GO (LB held)"));
    QCOMPARE(ps.at(0).left(3), QString("L1:"));
    QCOMPARE(ps.at(1), QString("Cross: GO (L1 held)"));
    QVERIFY(xbox.contains(QString("X: Hold 1 s: stop the list (LB held)")));
    QVERIFY(ps.contains(QString("Square: Hold 1 s: stop the list (L1 held)")));
    // No line names a trigger: the pad picks no list.
    for (const QString &l : xbox + ps)
        QVERIFY2(!l.startsWith("RT") && !l.startsWith("LT") && !l.startsWith("R2") && !l.startsWith("L2")
                 && !l.contains("cue list", Qt::CaseInsensitive), qPrintable(l));
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
    QCOMPARE(m_runs.size(), 3);
    QCOMPARE(m_runs.at(1).first.command, ShowControl::Back);
    QCOMPARE(m_runs.at(2).first.command, ShowControl::SubGo);

    /* Stop sits beside GO (0.4.5): a tap of X, armed, stops nothing and
       says to hold it -- on the screen too. */
    run->setStopHoldMs(80);
    press(kX);
    QCOMPARE(m_runs.size(), 3);
    QVERIFY(!run->isStopPending());
    QCOMPARE(run->notice(), QString("Hold X to stop the list."));
    QCOMPARE(said.last().at(0).toString(), QString("Hold X to stop the list."));
    // Held: the Stop goes once the hold is up, and once only.
    hold(kX, true);
    QVERIFY(run->isStopPending());
    QTRY_COMPARE(m_runs.size(), 4);
    QCOMPARE(m_runs.at(3).first.command, ShowControl::Stop);
    QTest::qWait(120);
    hold(kX, false);
    QCOMPARE(m_runs.size(), 4);
    // LB let go during the hold: no Stop.
    hold(kX, true);
    hold(kLB, false);
    QVERIFY(!run->isStopPending());
    QTest::qWait(120);
    hold(kX, false);
    QCOMPARE(m_runs.size(), 4);
    hold(kLB, true);
    for (const auto &r : m_runs)
        QCOMPARE(r.second, ShowControl::RoutePad);

    // A press's release sends nothing; RB is not in the map.
    hold(kA, false);
    press(kRB);
    QCOMPARE(m_runs.size(), 4);

    // Let go of the arm: disarmed.
    hold(kLB, false);
    QVERIFY(!run->isArmed());
    QCOMPARE(armed.count(), 4);      // on, off and on again in the Stop hold, off
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
        hold(kLB, armed);
        for (quint32 t : { kRT, kLT })
        {
            axis(t, 255);
            axis(t, 0);
        }
        hold(kLB, false);
    }
    QCOMPARE(run->listChaser(), quint32(UINT_MAX));
    QVERIFY(m_runs.isEmpty());
    QCOMPARE(said.count(), 0);

    // The Run tab picks the list: the pad's GO plays it, RS names it.
    m_doc->resetModified();
    m_doc->controlMap()->setRunTabList(act2);
    QVERIFY(m_doc->isModified());                          // the show's choice
    QCOMPARE(run->listChaser(), c2);
    hold(kLB, true);
    press(kA);
    hold(kLB, false);
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
    run->setStopHoldMs(30);
    QSignalSpy off(run, &PadRun::enabledChanged);

    auto check = [&](const QString &what) {
        QVERIFY2(run->isEnabled(), qPrintable(what));
        const QString text = run->roleText();
        QVERIFY2(text == "Pad: Run" || text == "Pad: ARMED" || text == "Pad: Walk",
                 qPrintable(what + ": " + text));
    };
    for (bool armed : { false, true })
    {
        for (auto it = roles.begin(); it != roles.end(); ++it)
        {
            const quint32 ch = it.key();
            if (ch == kLB)
                continue;
            const QString what = QString("%1%2").arg(armed ? "LB + " : "", it.value());
            hold(kLB, armed);
            // A press, then a hold past every hold time, then the release.
            press(ch);
            check(what + " pressed");
            hold(ch, true);
            QTest::qWait(60);
            check(what + " held");
            hold(ch, false);
            check(what + " let go");
            hold(kLB, false);
            check(what + " LB let go");
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
        hold(kLB, true);
    return run;
}

void PadRun_Test::rbFiresTheArmedHit()
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
    press(kRB);
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

    // Again: it is let go (and said).
    press(kRB);
    QCOMPARE(fired.count(), 2);
    QVERIFY(!fired.at(1).at(1).toBool());
    QVERIFY(said.last().at(0).toString().contains("Breathe off"));
    m_doc->masterTimer()->timerTick();
    QVERIFY(!f->isRunning());
    hold(kLB, false);
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

    // No LB: refused, said, nothing starts.
    press(kRB);
    QCOMPARE(refused.count(), 1);
    QCOMPARE(refused.at(0).at(0).value<PadRun::Action>(), PadRun::FireHit);
    QVERIFY(said.last().at(0).toString().contains("LB"));
    QCOMPARE(fired.count(), 0);

    // LB held, no hit armed: said.
    hold(kLB, true);
    press(kRB);
    QCOMPARE(fired.count(), 0);
    QVERIFY(said.last().at(0).toString().contains("No hit armed"));

    // An armed hit in Design: not fired either (as a VC button).
    const quint32 wash = hitWithLayer(ShowTree::HitScope::Production, prod, "Wash");
    m_doc->setMode(Doc::Design);
    press(kRB);
    QCOMPARE(fired.count(), 0);
    QVERIFY(said.last().at(0).toString().contains("Operate"));
    QVERIFY(!m_doc->function(st->lookFunction(wash))->isRunning());
    QVERIFY(!m_doc->function(st->lookFunction(onCue))->isRunning());

    // Let go of the arm in Operate: nothing again.
    m_doc->setMode(Doc::Operate);
    hold(kLB, false);
    press(kRB);
    QCOMPARE(fired.count(), 0);
    hold(kLB, true);
    press(kRB);
    QCOMPARE(fired.count(), 1);
    QCOMPARE(fired.at(0).at(0).toUInt(), wash);
    hold(kLB, false);
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
    press(kRB);
    QCOMPARE(fired.last().at(0).toUInt(), onList);
    m_doc->masterTimer()->timerTick();
    press(kRB);                                  // lets Red go
    QVERIFY(!fired.last().at(1).toBool());
    m_doc->masterTimer()->timerTick();
    press(kRB);
    QCOMPARE(fired.last().at(0).toUInt(), onList2);
    m_doc->masterTimer()->timerTick();
    press(kRB);
    QVERIFY(!fired.last().at(1).toBool());
    QVERIFY(onCue != 0 && everywhere != 0 && onProd != 0);

    // Only wider ones: the production's beats everywhere.
    QVERIFY(st->setHitScope({ onList, onList2 }, ShowTree::HitScope::Production, prod));
    QCOMPARE(run->armedHits().size(), 3);
    QVERIFY(!run->armedHits().contains(everywhere));
    QVERIFY(st->setHitScope({ onList, onList2, onProd }, ShowTree::HitScope::Global, ShowTree::invalidId()));
    QCOMPARE(run->armedHits().size(), 4);
    hold(kLB, false);
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
    const quint32 xA = 6, xLB = 8, xRB = 10, xMenu = 11;
    const quint32 dA = 7, dLB = 9, dRB = 11, dMenu = 10;
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << xbox << dualSense;
    const QMap<quint32, QString> xroles = { { 6, "a" }, { 7, "b" }, { 8, "lb" }, { 9, "x" },
                                            { 10, "rb" }, { 11, "start" }, { 12, "back" } };
    const QMap<quint32, QString> droles = { { 6, "x" }, { 7, "a" }, { 8, "b" }, { 9, "lb" },
                                            { 10, "start" }, { 11, "rb" }, { 12, "back" } };
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

    // The Run role: LB + A is one GO through the pad's route.
    run->setEnabled(true);
    padHold(stub, 0, xLB, true);
    QVERIFY(run->isArmed());
    padPress(stub, 0, xA);
    padHold(stub, 0, xLB, false);
    QCOMPARE(m_runs.size(), 1);
    QCOMPARE(m_runs.at(0).first.command, ShowControl::Go);
    QCOMPARE(m_runs.at(0).second, ShowControl::RoutePad);
    QCOMPARE(actions.count(), 1);          // no profile action beside it

    // A walk: Menu starts Focus this cue on the cue in hand, Menu ends it.
    padPress(stub, 0, xMenu);
    QVERIFY(run->isWalking());
    padPress(stub, 0, xMenu);
    QVERIFY(!run->isWalking());

    // A hit: LB + RB fires the armed one, in Operate.
    m_doc->setMode(Doc::Operate);
    QSignalSpy fired(run, &PadRun::hitFired);
    padHold(stub, 0, xLB, true);
    padPress(stub, 0, xRB);
    QCOMPARE(fired.count(), 1);
    QCOMPARE(fired.at(0).at(0).toUInt(), breathe);
    padPress(stub, 0, xRB);                 // and lets it go
    QCOMPARE(fired.count(), 2);
    padHold(stub, 0, xLB, false);
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

    padHold(stub, 0, dLB, true);
    padPress(stub, 0, dA);
    padHold(stub, 0, dLB, false);
    QCOMPARE(m_runs.size(), 2);
    QCOMPARE(m_runs.at(1).first.command, ShowControl::Go);
    padPress(stub, 0, dMenu);
    QVERIFY(run->isWalking());
    padPress(stub, 0, dMenu);
    QVERIFY(!run->isWalking());
    m_doc->setMode(Doc::Operate);
    padHold(stub, 0, dLB, true);
    padPress(stub, 0, dRB);
    QCOMPARE(fired.count(), 3);
    padPress(stub, 0, dRB);
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
    // Armed: R2 firm, L2 free.
    QCOMPARE(T::forState(true, true, false).right, T::Firm);
    QCOMPARE(T::forState(true, true, false).left, T::Off);
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
    hold(kLB, true);
    hold(kLB, false);
    QCOMPARE(pc->padTriggers(), PadTriggers::State());

    run->setEnabled(true);
    told.clear();
    hold(kLB, true);
    QVERIFY(run->isArmed());
    QCOMPARE(pc->padTriggers().right, PadTriggers::Firm);
    QCOMPARE(pc->padTriggers().left, PadTriggers::Off);
    QCOMPARE(told.count(), 1);
    // Held, nothing is said again.
    hold(kLB, true);
    QCOMPARE(told.count(), 1);
    // Disarm: off.
    hold(kLB, false);
    QCOMPARE(pc->padTriggers(), PadTriggers::State());
    QCOMPARE(told.count(), 2);

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

    // Armed, then the role leaves: off.
    hold(kLB, true);
    QCOMPARE(pc->padTriggers().right, PadTriggers::Firm);
    run->setEnabled(false);
    QCOMPARE(pc->padTriggers(), PadTriggers::State());
    hold(kLB, false);

    // Armed, the controller is lost: reset puts them off.
    run->setEnabled(true);
    hold(kLB, true);
    QCOMPARE(pc->padTriggers().right, PadTriggers::Firm);
    run->reset();
    QCOMPARE(pc->padTriggers(), PadTriggers::State());

    // Quit while armed: off before the controller goes.
    hold(kLB, true);
    QCOMPARE(pc->padTriggers().right, PadTriggers::Firm);
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

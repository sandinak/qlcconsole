/*
  Q Light Controller Plus
  hazardarming_test.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QSignalSpy>
#include <QSettings>
#include <QBuffer>
#include <QtTest>

#include "../common/stdprelude.h"
#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#include "outputrulekeeper.h"
#include "chaser.h"
#undef protected
#undef private

#include "hazardarming_test.h"
#include "simmididevice.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "outputrulekeeper.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "ioplugincache.h"
#include "fixturegroup.h"
#include "fixturerest.h"
#include "mastertimer.h"
#include "showcontrol.h"
#include "showreview.h"
#include "deskrecord.h"
#include "controlmap.h"
#include "qlcpalette.h"
#include "universe.h"
#include "qlcfile.h"
#include "fixture.h"
#include "chaser.h"
#include "chaseraction.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "scene.h"
#include "monitorproperties.h"
#include "controllerkeepalive.h"
#include "chaserstep.h"
#include "collection.h"
#include "efx.h"
#include "efxfixture.h"
#include "functionparent.h"
#include "doc.h"
#include <thread>

using D = DeskRecord;
using K = OutputRuleKeeper;

namespace
{
const QString kPmj = QStringLiteral("PMJ Black 1");
// The spotter's board is on universe 4; the rig on universe 1.
const quint32 kBoardUniverse = 3;

QLCInputProfile *mapProfile(Doc *doc)
{
    const QString file = QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/PMJ-Black-1.qxi");
    QLCInputProfile *p = QLCInputProfile::loader(file);
    if (p == nullptr)
        return nullptr;
    InputOutputMap *iom = doc->inputOutputMap();
    if (QLCInputProfile *have = iom->profile(p->name()))
    {
        delete p;
        return have;
    }
    if (!iom->addProfile(p))
    {
        delete p;
        return nullptr;
    }
    return p;
}
}

void HazardArming_Test::initTestCase()
{
    // This computer's desk is kept in the settings: the test's own.
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
    // The Hold's limit (15 s in a show) short enough to test (third review N3).
    qputenv("QLC_TEST_HAZARD_HOLD_MS", "2500");
}

void HazardArming_Test::init()
{
    QSettings().remove(DeskRecord::settingsGroup());
    m_doc = new Doc(this);
    QDir dir(QStringLiteral("../iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_feedback.clear();
    m_stub->m_lineNames.clear();
    QVERIFY(mapProfile(m_doc) != nullptr);
    m_pmj = new SimMidiDevice(m_doc, m_stub, kPmj, kBoardUniverse, kPmj);
    QVERIFY2(m_pmj->plug(), qPrintable(m_pmj->lastError()));
    m_doc->controlMap()->setDumpWindowMs(0);

    // The rig: a wash, a CO2 jet (Burst), a hazer (Running).
    quint32 address = 0;
    auto dimmer = [&](const QString &name) {
        Fixture *f = new Fixture(m_doc);
        f->setName(name);
        f->setChannels(1);
        f->setUniverse(0);
        f->setAddress(address++);
        if (!m_doc->addFixture(f))
            qFatal("could not add %s", qPrintable(name));
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        m_doc->addFixtureGroup(g);
        g->assignFixture(f->id());
        return g->id();
    };
    auto level = [&](const QString &name, int v) {
        QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
        p->setName(name);
        p->setValue(v);
        m_doc->addPalette(p);
        return p->id();
    };
    const quint32 wash = dimmer(QStringLiteral("Stage wash"));
    const quint32 jet = dimmer(QStringLiteral("CO2 jet"));
    const quint32 hazer = dimmer(QStringLiteral("Hazer"));
    const quint32 full = level(QStringLiteral("Full"), 255), half = level(QStringLiteral("Half"), 128),
                  out = level(QStringLiteral("Out"), 0), low = level(QStringLiteral("Low"), 50);
    ShowTreeController *st = m_doc->showTree();
    auto set = [st](quint32 cue, quint32 group, quint32 palette) {
        ShowTree::Target t;
        t.groups << group;
        return st->setTargetLook(cue, t, st->lookForPalette(palette));
    };
    m_list = st->addCueList(st->addProduction(QStringLiteral("Show")), QStringLiteral("Main"));
    m_c1 = st->addCue(m_list, -1, QStringLiteral("Preset"));
    QVERIFY(set(m_c1, wash, full));
    QVERIFY(set(m_c1, jet, out));
    QVERIFY(set(m_c1, hazer, out));
    m_c2 = st->addCue(m_list, 0, QStringLiteral("The blast"));
    QVERIFY(set(m_c2, wash, half));
    QVERIFY(set(m_c2, jet, full));
    QVERIFY(set(m_c2, hazer, full));
    m_c3 = st->addCue(m_list, 1, QStringLiteral("After"));
    QVERIFY(set(m_c3, wash, full));      // the jet and the hazer track on
    m_c4 = st->addCue(m_list, 2, QStringLiteral("Low haze"));
    QVERIFY(set(m_c4, hazer, low));
    QVERIFY(set(m_c4, jet, out));
    m_c5 = st->addCue(m_list, 3, QStringLiteral("Haze out"));
    QVERIFY(set(m_c5, hazer, out));
    m_c6 = st->addCue(m_list, 4, QStringLiteral("Slow haze"));
    QVERIFY(set(m_c6, hazer, full));
    {
        // Over two seconds: a Running hazard follows its cue's fade (N7).
        QList<ShowTree::Cue> cues = st->cueList(m_list)->cues;
        for (ShowTree::Cue &c : cues)
            if (c.id == m_c6)
            {
                c.fadeIn = 2000;
                st->setCue(c);
            }
    }
    const ShowTree::CueList *list = st->cueList(m_list);
    QVERIFY(list != nullptr);
    m_chaser = qobject_cast<Chaser *>(m_doc->function(list->chaser));
    QVERIFY(m_chaser != nullptr);
    m_doc->controlMap()->setRunTabList(m_list);
    m_doc->outputRuleKeeper()->rebuild();
    QCOMPARE(FixtureRest::behaviour(named("CO2 jet")), Fixture::HazardBurstBehaviour);
    QCOMPARE(FixtureRest::behaviour(named("Hazer")), Fixture::HazardRunningBehaviour);
}

void HazardArming_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(3);
    delete m_pmj;
    m_pmj = nullptr;
    m_stub->m_lineNames.clear();
    m_stub->m_linesAreHardware = false;
    m_stub->m_networkDmx = true;
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
    QSettings().remove(DeskRecord::settingsGroup());
}

/****************************************************************************
 * Helpers
 ****************************************************************************/

void HazardArming_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        for (Universe *u : ua)
            u->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
}

int HazardArming_Test::at(const char *fixture)
{
    const Fixture *f = named(fixture);
    if (f == nullptr)
        return -1;
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    const QByteArray out = *ua.at(int(f->universe()))->postGMValues();
    m_doc->inputOutputMap()->releaseUniverses(false);
    return int(uchar(out.at(int(f->address()))));
}

void HazardArming_Test::go()
{
    ShowControl::Request r;
    r.command = ShowControl::Go;
    r.chaser = m_chaser->id();
    m_doc->showControl()->setMainCueList(m_chaser->id());
    m_doc->showControl()->execute(r);
    tick(2);
}

void HazardArming_Test::waitForCheck()
{
    QTest::qWait(int(4 * MasterTimer::tick()) + 120);
    tick(10);
}

Fixture *HazardArming_Test::named(const char *name) const
{
    for (Fixture *f : m_doc->fixtures())
        if (f->name() == QLatin1String(name))
            return f;
    return nullptr;
}

/* This computer's @p role on the PMJ's @p control, as Learn… leaves it. */
bool HazardArming_Test::bindRole(int role, const QString &control)
{
    D *desk = m_doc->deskRecord();
    D::Binding b;
    b.identity = desk->identityOfUniverse(kBoardUniverse);
    b.device = kPmj;
    b.plugin = m_stub->name();
    b.channel = m_pmj->channelOf(control);
    b.control = control;
    return desk->setBinding(D::Role(role), b) && desk->isLive(D::Role(role));
}

/****************************************************************************
 * Tests
 ****************************************************************************/

void HazardArming_Test::twoKindsGuessedSetAndLoaded()
{
    QVERIFY(Fixture::behaviours().contains(Fixture::HazardBurstBehaviour));
    QVERIFY(Fixture::behaviours().contains(Fixture::HazardRunningBehaviour));
    QCOMPARE(Fixture::behaviourName(Fixture::HazardBurstBehaviour), QStringLiteral("Hazard — Burst"));
    QCOMPARE(Fixture::behaviourName(Fixture::HazardRunningBehaviour), QStringLiteral("Hazard — Running"));
    // The guesses: a burst of something, or something that runs.
    auto guess = [this](const QString &name) {
        Fixture *f = new Fixture(m_doc);
        f->setName(name);
        f->setChannels(1);
        f->setUniverse(1);
        f->setAddress(100);
        m_doc->addFixture(f);
        const Fixture::Behaviour b = FixtureRest::behaviour(f);
        m_doc->deleteFixture(f->id());
        return b;
    };
    for (const char *burst : { "Flame bar 1", "CO2 cannon", "Confetti blaster", "Sparkular left", "Pyro cue 3" })
        QCOMPARE(guess(QLatin1String(burst)), Fixture::HazardBurstBehaviour);
    for (const char *running : { "Fogger DSL", "Hazer USR", "Smoke machine" })
        QCOMPARE(guess(QLatin1String(running)), Fixture::HazardRunningBehaviour);

    // Saved only when set by hand; the old one-kind "Hazard" loads as the
    // guessed kind (and stays a hazard).
    auto load = [this](const QString &name, const QString &behavesAs) {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        Fixture f(m_doc);
        f.setID(9000);
        f.setName(name);
        f.setChannels(1);
        f.setUniverse(2);
        f.setAddress(10);
        f.saveXML(&w);
        buf.close();
        QByteArray xml = buf.data();
        if (!behavesAs.isEmpty())
            xml.replace("</Fixture>", "<BehavesAs>" + behavesAs.toUtf8() + "</BehavesAs></Fixture>");
        QXmlStreamReader r(xml);
        r.readNextStartElement();
        Fixture *back = new Fixture(m_doc);
        back->loadXML(r, m_doc, m_doc->fixtureDefCache());
        const int set = back->behaviourSetting();
        delete back;
        return set;
    };
    QCOMPARE(load(QStringLiteral("Hazer 2"), QStringLiteral("Hazard")), int(Fixture::HazardRunningBehaviour));
    QCOMPARE(load(QStringLiteral("Glitter gun"), QStringLiteral("Hazard")), int(Fixture::HazardBurstBehaviour));
    QCOMPARE(load(QStringLiteral("Stage wash 2"), QStringLiteral("HazardRunning")), int(Fixture::HazardRunningBehaviour));
    QCOMPARE(load(QStringLiteral("CO2 2"), QString()), -1);
    Fixture *jet = named("CO2 jet");
    jet->setBehaviourSetting(Fixture::HazardRunningBehaviour);
    {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        jet->saveXML(&w);
        QVERIFY(buf.data().contains("<BehavesAs>HazardRunning</BehavesAs>"));
    }
    jet->setBehaviourSetting(-1);
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    jet->saveXML(&w);
    QVERIFY(!buf.data().contains("BehavesAs"));
}

void HazardArming_Test::aBurstFiresOnlyWhileHeld()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    QCOMPARE(k->hazardChipText(), QStringLiteral("Hazards: DISARMED"));
    go();                                   // the preset
    waitForCheck();
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    QVERIFY(k->hazardHeld());
    QCOMPARE(k->hazardChipText(), QStringLiteral("Hazards: HELD"));
    go();                                   // the blast, held
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 255);
    QCOMPARE(at("Stage wash"), 128);
    QVERIFY(k->hazardLive(named("CO2 jet")->id()));
    // Let go: safe the very next frame.
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    tick(1);
    QCOMPARE(at("CO2 jet"), 0);
    // DISARMED, and the hazer the cue reached unarmed said skipped.
    QCOMPARE(k->hazardChipText(), QStringLiteral("Hazards: DISARMED · Hazer skipped — not armed"));
    // The deadman: held again within its cue, it is out again.
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    tick(1);
    QCOMPARE(at("CO2 jet"), 255);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    tick(1);
    QCOMPARE(at("CO2 jet"), 0);
    // The hazer (Running) never fired: nothing armed it.
    QCOMPARE(at("Hazer"), 0);
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::aCueReachedUnheldSkipsTheBurst()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    go();
    waitForCheck();
    go();                                   // the blast, NOT held
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 0);
    QVERIFY(k->hazardsSkipped().contains(named("CO2 jet")->id()));
    QCOMPARE(k->hazardSkippedText(), QStringLiteral("CO2 jet skipped — not held · Hazer skipped — not armed"));
    QVERIFY(k->hazardChipText().contains(QStringLiteral("CO2 jet skipped — not held")));
    // The hold comes late: the jet never fires late.
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    tick(5);
    QTest::qWait(150);
    tick(5);
    QCOMPARE(at("CO2 jet"), 0);
    QVERIFY(!k->hazardLive(named("CO2 jet")->id()));
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::armedRunningStaysOnUntilKill()
{
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    QVERIFY(bindRole(D::HazardKill, QStringLiteral("3-Down")));
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));          // armed
    QVERIFY(k->hazardsArmed());
    QCOMPARE(k->hazardChipText(), QStringLiteral("Hazards: ARMED"));
    go();
    waitForCheck();
    go();                                   // the hazer to full
    waitForCheck();
    QCOMPARE(at("Hazer"), 255);
    QCOMPARE(at("CO2 jet"), 0);            // a burst needs the Hold
    go();                                   // the next cue: it stays on
    waitForCheck();
    QCOMPARE(m_chaser->currentStepIndex(), 2);
    QCOMPARE(at("Hazer"), 255);
    // The Kill: off the same frame, and disarmed.
    QVERIFY(m_pmj->press(QStringLiteral("3-Down")));
    tick(1);
    QCOMPARE(at("Hazer"), 0);
    QVERIFY(!k->hazardsArmed());
    QVERIFY(k->hazardChipText().startsWith(QStringLiteral("Hazards: DISARMED")));
    QVERIFY(m_pmj->release(QStringLiteral("3-Down")));
    // Armed again: nothing until a cue sets it.
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    tick(5);
    QCOMPARE(at("Hazer"), 0);
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::unarmedRunningWaitsForTheNextCue()
{
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    go();
    waitForCheck();
    go();                                   // the hazer's cue, unarmed
    waitForCheck();
    QCOMPARE(at("Hazer"), 0);
    QVERIFY(k->hazardSkippedText().contains(QStringLiteral("Hazer skipped — not armed")));
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));          // armed after
    tick(5);
    QTest::qWait(150);
    tick(5);
    QCOMPARE(at("Hazer"), 0);               // not fired late
    go();                                   // the next cue sets it (it tracks)
    waitForCheck();
    QCOMPARE(at("Hazer"), 255);
    QVERIFY(k->hazardSkippedText().isEmpty() || !k->hazardSkippedText().contains(QStringLiteral("Hazer")));
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::stopsAndOpeningDisarm()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    go();
    waitForCheck();
    go();
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 255);
    QCOMPARE(at("Hazer"), 255);
    // The emergency stop: off and disarmed; the Hold still down counts
    // only once let go of.
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    m_doc->showControl()->execute(off);
    tick(2);
    QCOMPARE(at("CO2 jet"), 0);
    QCOMPARE(at("Hazer"), 0);
    QVERIFY(!k->hazardsArmed());
    QVERIFY(!k->hazardHeld());
    QVERIFY(k->hazardChipText().startsWith(QStringLiteral("Hazards: DISARMED")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));        // still down: not a hold
    QVERIFY(!k->hazardHeld());
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));        // a fresh press is
    QVERIFY(k->hazardHeld());
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));

    // Stop ALL disarms too.
    QTest::qWait(100);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    QVERIFY(k->hazardsArmed());
    k->holdStageMachinery();        // what Stop ALL and Fade and stop call first
    QVERIFY(!k->hazardsArmed());

    // Another show opened: disarmed.
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    QVERIFY(k->hazardsArmed());
    m_doc->clearContents();
    QVERIFY(!k->hazardsArmed());
    QVERIFY(!k->hazardHeld());
}

void HazardArming_Test::theSpottersDeviceLostDisarms()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    go();
    waitForCheck();
    go();
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 255);
    QCOMPARE(at("Hazer"), 255);
    // The cable out, the Hold still held in the spotter's hand.
    QVERIFY2(m_pmj->unplug(), qPrintable(m_pmj->lastError()));
    QCoreApplication::processEvents();
    tick(1);
    QCOMPARE(at("CO2 jet"), 0);
    QCOMPARE(at("Hazer"), 0);
    QVERIFY(!k->hazardsArmed());
    QVERIFY(!k->hazardHeld());
    QVERIFY(k->hazardLog().last().contains(QStringLiteral("Disarmed")));
    // Back in: nothing fires until armed again and a cue sets it.
    QVERIFY(m_pmj->replug({ { QStringLiteral("1-Down"), 127 } }));
    QCoreApplication::processEvents();
    tick(5);
    QVERIFY(!k->hazardHeld());
    QCOMPARE(at("CO2 jet"), 0);
    QCOMPARE(at("Hazer"), 0);
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::theControlsAreRefusedOnGoAndTheKeyboard()
{
    D *desk = m_doc->deskRecord();
    QVERIFY(bindRole(D::Go, QStringLiteral("Go")));
    const quint32 goCh = m_pmj->channelOf(QStringLiteral("Go"));
    // On GO's control: refused, named.
    QVERIFY(!desk->learnRefusal(D::HazardArm, kBoardUniverse, goCh).isEmpty());
    QVERIFY(!desk->learnRefusal(D::HazardHold, kBoardUniverse, goCh).isEmpty());
    QVERIFY(desk->learnRefusal(D::HazardArm, kBoardUniverse, m_pmj->channelOf(QStringLiteral("2-Down"))).isEmpty());
    // On a computer key: refused.
    QVERIFY(desk->learnRefusal(D::HazardArm, ControlMap::KeyboardUniverse, Qt::Key_A)
                .contains(QStringLiteral("keyboard")));
    // Learned by pressing: GO's control is refused, a free button taken.
    QSignalSpy refused(desk, &D::learnRefused);
    QVERIFY(desk->learn(D::HazardArm));
    desk->handleInput(ControlMap::KeyboardUniverse, Qt::Key_A, 255);
    QCOMPARE(refused.count(), 1);
    QVERIFY(m_pmj->tap(QStringLiteral("Go")));
    QVERIFY(refused.count() >= 2);
    QVERIFY(!desk->isSet(D::HazardArm));
    QVERIFY(desk->isSet(D::Go));
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    QVERIFY(desk->isSet(D::HazardArm));
    QCOMPARE(desk->binding(D::HazardArm).channel, m_pmj->channelOf(QStringLiteral("2-Down")));
    // The show's own emergency stop on a control: refused there too.
    const quint32 down4 = m_pmj->channelOf(QStringLiteral("4-Down"));
    m_doc->controlMap()->setBinding({ kBoardUniverse, down4, ControlMap::emergencyStop(), ControlMap::Toggle });
    QVERIFY(!desk->learnRefusal(D::HazardKill, kBoardUniverse, down4).isEmpty());
    // On GO's device: allowed, and the setup says a device of its own is better.
    QVERIFY(desk->hazardDeviceNote().contains(kPmj));
}

void HazardArming_Test::parkAndEditsNeverFire()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    K *k = m_doc->outputRuleKeeper();
    // Design: armed and held, a pick of the blast fires nothing.
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    m_doc->showTree()->pickCue(m_c2);
    tick(20);
    QCOMPARE(at("Stage wash"), 128);
    QCOMPARE(at("CO2 jet"), 0);
    QCOMPARE(at("Hazer"), 0);
    m_doc->showTree()->stopPreview();
    tick(5);
    // A run fires it; a pick after takes that away at once.
    go();
    waitForCheck();
    go();
    waitForCheck();
    QCOMPARE(at("Hazer"), 255);
    m_doc->showTree()->pickCue(m_c1);
    tick(1);
    QCOMPARE(at("Hazer"), 0);
    QCOMPARE(at("CO2 jet"), 0);
    m_doc->showTree()->stopPreview();
    // Parked: off, armed or not, a GO or not.
    m_doc->setMode(Doc::Operate);
    m_doc->programmer()->parkFixtures({ named("Hazer")->id() });
    tick(2);
    QVERIFY(k->isParkedHazard(named("Hazer")->id()));
    go();
    waitForCheck();
    QCOMPARE(at("Hazer"), 0);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->programmer()->unparkAllFixtures();
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::showReviewSaysWhatIsMissing()
{
    auto findings = [this]() {
        ShowReview::Options o;
        o.checks = QStringList() << QStringLiteral("FIX-22") << QStringLiteral("FIX-23");
        o.record = false;
        QStringList got;
        for (const ReviewFinding &f : m_doc->showReview()->run(o).findings)
            got << f.check + QLatin1Char(':') + QString::number(int(f.severity)) + QLatin1Char(':') + f.message;
        return got;
    };
    QStringList got = findings();
    // Hazard cues, no Hold or Arm: an error for each kind.
    QCOMPARE(got.filter(QStringLiteral("FIX-22:2:")).size(), 2);
    QVERIFY2(got.filter(QStringLiteral("CO2 jet (a Burst hazard)")).size() == 1, qPrintable(got.join('\n')));
    // Every hazard, with its kind.
    QVERIFY(got.filter(QStringLiteral("FIX-23:0:CO2 jet behaves as Hazard — Burst (guessed)")).size() == 1);
    QVERIFY(got.filter(QStringLiteral("FIX-23:0:Hazer behaves as Hazard — Running (guessed)")).size() == 1);
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    got = findings();
    // Still no Kill: a warning says so.
    QCOMPARE(got.filter(QStringLiteral("FIX-22:1:")).size(), 1);
    QVERIFY(bindRole(D::HazardKill, QStringLiteral("3-Down")));
    got = findings();
    QCOMPARE(got.filter(QStringLiteral("FIX-22")).size(), 0);
    QCOMPARE(got.filter(QStringLiteral("FIX-23")).size(), 2);
    // This computer's lines name the spotter's controls.
    QVERIFY(m_doc->deskRecord()->reviewLines().filter(QStringLiteral("Hazard hold: PMJ Black 1")).size() == 1);
}

void HazardArming_Test::everyEventIsLogged()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    QVERIFY(bindRole(D::HazardKill, QStringLiteral("3-Down")));
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    go();
    waitForCheck();
    go();                                   // both skipped
    waitForCheck();
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    go();                                   // the next cue: both out
    waitForCheck();
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    QVERIFY(m_pmj->tap(QStringLiteral("3-Down")));
    const QString log = k->hazardLog().join(QLatin1Char('\n'));
    for (const char *line : { "CO2 jet skipped: not held", "Hazer skipped: not armed", "Armed:",
                              "CO2 jet fired at 255 (cue reached)", "Hazer on at 255 (cue reached)",
                              "CO2 jet back to safe (Hold let go)", "Hazer back to safe",
                              "Kill: every hazard off, disarmed" })
        QVERIFY2(log.contains(QLatin1String(line)), qPrintable(QStringLiteral("%1 not in:\n%2").arg(line, log)));
    m_doc->setMode(Doc::Design);
}

/****************************************************************************
 * The second review
 ****************************************************************************/

void HazardArming_Test::deskPush(const char *fixture, uchar value)
{
    const Fixture *fx = named(fixture);
    Universe *u = m_doc->inputOutputMap()->universe(fx->universe());
    QSharedPointer<GenericFader> f = u->requestFader(Universe::SimpleDesk);
    FadeChannel fc(m_doc, fx->id(), 0);
    fc.setStart(value);
    fc.setCurrent(value);
    fc.setTarget(value);
    fc.setReady(true);
    f->replaceAll({ fc });
}

Scene *HazardArming_Test::strayScene(int value)
{
    Scene *sc = new Scene(m_doc);
    sc->setName(QStringLiteral("Stray"));
    sc->setValue(named("CO2 jet")->id(), 0, uchar(value));
    sc->setValue(named("Hazer")->id(), 0, uchar(value));
    m_doc->addFunction(sc);
    sc->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    return sc;
}

void HazardArming_Test::aCueThatSetsItOutLeavesItOut()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));          // armed
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));        // held
    // The Simple Desk pushes both up; the GO's cue sets them out.
    deskPush("CO2 jet", 255);
    deskPush("Hazer", 255);
    go();                                                   // the preset: jet, hazer OUT
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 0);
    QCOMPARE(at("Hazer"), 0);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::onlyTheCuesOwnValueGoesOut()
{
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    for (int i = 0; i < 4; ++i)
    {
        go();
        waitForCheck();
    }
    QCOMPARE(m_chaser->currentStepIndex(), 3);             // "Low haze": the hazer at 50
    QCOMPARE(at("Hazer"), 50);
    // The Simple Desk and a scene push it up: still the cue's 50.
    deskPush("Hazer", 255);
    strayScene(255);
    tick(5);
    QCOMPARE(at("Hazer"), 50);
    QCOMPARE(at("CO2 jet"), 0);
    // A cue that sets it to 0: 0, armed or not.
    go();
    waitForCheck();
    QCOMPARE(m_chaser->currentStepIndex(), 4);
    QCOMPARE(at("Hazer"), 0);
    QVERIFY(k->hazardsArmed());
    // Every change of what went out is in the log.
    const QString log = k->hazardLog().join(QLatin1Char('\n'));
    QVERIFY2(log.contains(QStringLiteral("Hazer 255 → 50")), qPrintable(log));
    QVERIFY2(log.contains(QStringLiteral("Hazer back to safe")), qPrintable(log));
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::aLaserClassSetLaterIsHeldAtOnce()
{
    K *k = m_doc->outputRuleKeeper();
    Fixture *beam = new Fixture(m_doc);
    beam->setName(QStringLiteral("Sky beam"));
    beam->setChannels(1);
    beam->setUniverse(0);
    beam->setAddress(20);
    QVERIFY(m_doc->addFixture(beam));
    QCOMPARE(FixtureRest::behaviour(beam), Fixture::LightBehaviour);
    Scene *sc = new Scene(m_doc);
    sc->setValue(beam->id(), 0, 255);
    m_doc->addFunction(sc);
    sc->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    QCOMPARE(at("Sky beam"), 255);
    // Fixture Properties: Laser class 3B. No event loop between.
    FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(beam->id());
    rp.laserClass = 4;
    m_doc->monitorProperties()->setFixtureRigProps(beam->id(), rp);
    tick(1);
    QCOMPARE(at("Sky beam"), 0);
    QVERIFY(k->hazards().contains(beam->id()));
}

void HazardArming_Test::everyStepTakesTheBurstBack()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Operate);
    go();
    waitForCheck();
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    go();                                                   // the blast, held: out
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 255);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    tick(1);
    QCOMPARE(at("CO2 jet"), 0);
    // The chaser steps on by itself (no GO: an auto-follow, the editor's
    // Test, a timecode), to cue 3 that tracks the jet on, hold up.
    ChaserAction next;
    next.m_action = ChaserNextStep;
    next.m_stepIndex = 0;
    next.m_masterIntensity = 1.0;
    next.m_stepIntensity = 1.0;
    next.m_fadeMode = Chaser::FromFunction;
    m_chaser->setAction(next);
    tick(3);
    waitForCheck();
    QCOMPARE(m_chaser->currentStepIndex(), 2);
    // A Hold pressed now never fires it: its step was reached unheld.
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    tick(3);
    QTest::qWait(150);
    tick(3);
    QCOMPARE(at("CO2 jet"), 0);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::holdAndArmAreRefusedOnTheNetwork()
{
    D *desk = m_doc->deskRecord();
    const quint32 ch = m_pmj->channelOf(QStringLiteral("2-Down"));
    m_stub->m_linesAreHardware = false;     // the stub as Art-Net, sACN, OSC
    const QString hold = desk->learnRefusal(D::HazardHold, kBoardUniverse, ch);
    const QString arm = desk->learnRefusal(D::HazardArm, kBoardUniverse, ch);
    const QString kill = desk->learnRefusal(D::HazardKill, kBoardUniverse, ch);
    m_stub->m_linesAreHardware = true;
    QVERIFY2(hold.contains(QStringLiteral("network")), qPrintable(hold));
    QVERIFY2(arm.contains(QStringLiteral("network")), qPrintable(arm));
    QVERIFY2(kill.isEmpty(), qPrintable(kill));
}

void HazardArming_Test::sleepAndWakeDisarm()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    K *k = m_doc->outputRuleKeeper();
    ControllerKeepAlive *ka = m_doc->programmer()->controllerKeepAlive();
    QVERIFY(ka != nullptr);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    QVERIFY(k->hazardsArmed());
    QVERIFY(k->hazardHeld());
    emit ka->systemStateChanged(ControllerKeepAlive::Asleep);
    QCoreApplication::processEvents();
    QVERIFY(!k->hazardsArmed());
    QVERIFY(!k->hazardHeld());
    emit ka->systemStateChanged(ControllerKeepAlive::Waking);
    QCoreApplication::processEvents();
    // Woken with the Hold still down: not a hold until let go and pressed.
    k->setHazardHold(true);
    QVERIFY(!k->hazardHeld());
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    QVERIFY(k->hazardHeld());
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
}

void HazardArming_Test::releaseAndKillNeedNoMainThread()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    QVERIFY(bindRole(D::HazardKill, QStringLiteral("3-Down")));
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    go();
    waitForCheck();
    go();
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 255);
    QCOMPARE(at("Hazer"), 255);
    Universe *u = m_doc->inputOutputMap()->universe(0);
    auto frame = [this, u]() {
        // A frame, nothing else: no input flush, no event loop.
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        u->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    };
    const quint32 line = quint32(m_pmj->line());
    // The Hold let go, from the plugin's own thread; the main thread
    // never turns (no processEvents) before the next frame.
    std::thread([this, line]() {
        m_stub->emitValueChanged(UINT_MAX, line, m_pmj->channelOf(QStringLiteral("1-Down")), 0);
    }).join();
    frame();
    QCOMPARE(at("CO2 jet"), 0);
    QCOMPARE(at("Hazer"), 255);
    // The Kill, the same way: the hazer off.
    std::thread([this, line]() {
        m_stub->emitValueChanged(UINT_MAX, line, m_pmj->channelOf(QStringLiteral("3-Down")), 127);
    }).join();
    frame();
    QCOMPARE(at("Hazer"), 0);
    QCoreApplication::processEvents();
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::aRenamedHazardStaysAHazard()
{
    K *k = m_doc->outputRuleKeeper();
    Fixture *jet = named("CO2 jet");
    QVERIFY(!jet->behaviourOverridden());
    jet->setName(QStringLiteral("DS Jet"));
    QCoreApplication::processEvents();
    QVERIFY(FixtureRest::isHazard(jet));
    QCOMPARE(FixtureRest::behaviour(jet), Fixture::HazardBurstBehaviour);
    QVERIFY(k->hazards().contains(jet->id()));
    QVERIFY(k->hazardLog().join(QLatin1Char('\n')).contains(QStringLiteral("DS Jet stays a hazard")));
    // Set to a Light by hand: then it is one, and said.
    jet->setBehaviourSetting(Fixture::LightBehaviour);
    QCoreApplication::processEvents();
    QVERIFY(!k->hazards().contains(jet->id()));
    QVERIFY(k->hazardLog().join(QLatin1Char('\n')).contains(QStringLiteral("DS Jet set to Light by hand")));
}

void HazardArming_Test::theHoldIsLearnedFromAPressHeld()
{
    D *desk = m_doc->deskRecord();
    QSignalSpy refused(desk, &D::learnRefused);
    QVERIFY(desk->learn(D::HazardHold));
    // A tap: refused, said.
    QVERIFY(m_pmj->tap(QStringLiteral("1-Down"), 40));
    QVERIFY(!desk->isSet(D::HazardHold));
    QVERIFY(refused.count() >= 1);
    // A press never let go (a latching button): refused when no release comes.
    QVERIFY(m_pmj->press(QStringLiteral("4-Down")));
    QTRY_VERIFY_WITH_TIMEOUT(refused.count() >= 2, 8000);
    QVERIFY(!desk->isSet(D::HazardHold));
    QVERIFY(m_pmj->release(QStringLiteral("4-Down")));
    // Held about a second, then let go: learned.
    QVERIFY(m_pmj->tap(QStringLiteral("1-Down"), 1100));
    QVERIFY(desk->isSet(D::HazardHold));
    QCOMPARE(desk->binding(D::HazardHold).channel, m_pmj->channelOf(QStringLiteral("1-Down")));
}

void HazardArming_Test::aGoInBlindGrantsNothing()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    go();
    waitForCheck();
    m_doc->inputOutputMap()->setOutputInhibited(true);     // Blind
    go();                                                   // the blast, in Blind
    waitForCheck();
    m_doc->inputOutputMap()->setOutputInhibited(false);
    tick(3);
    QCOMPARE(at("CO2 jet"), 0);
    QCOMPARE(at("Hazer"), 0);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::theHoldIsRefusedOnTheKill()
{
    D *desk = m_doc->deskRecord();
    QVERIFY(bindRole(D::HazardKill, QStringLiteral("3-Down")));
    const QString why = desk->learnRefusal(D::HazardHold, kBoardUniverse, m_pmj->channelOf(QStringLiteral("3-Down")));
    QVERIFY2(why.contains(QStringLiteral("Hazard kill")), qPrintable(why));
    // Show Review: hazard cues with no Kill are flagged.
    QVERIFY(desk->clear(D::HazardKill));
    ShowReview::Options o;
    o.checks = QStringList() << QStringLiteral("FIX-22");
    o.record = false;
    bool noKill = false;
    for (const ReviewFinding &f : m_doc->showReview()->run(o).findings)
        noKill = noKill || f.message.contains(QStringLiteral("no Hazard kill"));
    QVERIFY(noKill);
}

void HazardArming_Test::aProgrammerEditNeverDrivesAHazard()
{
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    // Design: a run to the low haze (50).
    for (int i = 0; i < 4; ++i)
    {
        go();
        waitForCheck();
    }
    QCOMPARE(at("Hazer"), 50);
    m_doc->programmer()->setProgrammerValue(named("Hazer")->id(), 0, 200);
    tick(5);
    QCoreApplication::processEvents();
    tick(5);
    QVERIFY2(at("Hazer") != 200, "the programmer drove a hazard");
    QVERIFY(at("Hazer") <= 50);
}

/****************************************************************************
 * The third review
 ****************************************************************************/

void HazardArming_Test::aVcOrTestStepNeverFires()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    K *k = m_doc->outputRuleKeeper();
    // A plain chaser (a VC cue list's, a button's): its step sets the jet.
    Scene *sc = new Scene(m_doc);
    sc->setName(QStringLiteral("Jet up"));
    sc->setValue(named("CO2 jet")->id(), 0, 255);
    m_doc->addFunction(sc);
    Chaser *ch = new Chaser(m_doc);
    ch->setName(QStringLiteral("VC chase"));
    m_doc->addFunction(ch);
    ch->addStep(ChaserStep(sc->id()));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));        // held
    ch->start(m_doc->masterTimer(), FunctionParent(FunctionParent::ManualVCWidget, 7));
    tick(5);
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 0);
    QVERIFY2(k->hazardSkippedText().contains(QStringLiteral("CO2 jet held — fire it from Run")),
             qPrintable(k->hazardSkippedText()));
    ch->stop(FunctionParent(FunctionParent::ManualVCWidget, 7));
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
}

void HazardArming_Test::aSecondDocNeverKillsTheFastPath()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    go();
    waitForCheck();
    go();
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 255);
    // Another Doc comes and goes (an import's scratch, Fixture Remap).
    {
        Doc *other = new Doc(this);
        delete other;
    }
    Universe *u = m_doc->inputOutputMap()->universe(0);
    const quint32 line = quint32(m_pmj->line());
    std::thread([this, line]() {
        m_stub->emitValueChanged(UINT_MAX, line, m_pmj->channelOf(QStringLiteral("1-Down")), 0);
    }).join();
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    u->processFaders();
    m_doc->inputOutputMap()->releaseUniverses(false);
    QCOMPARE(at("CO2 jet"), 0);
    QCoreApplication::processEvents();
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::holdIsRefusedOverTheAir()
{
    D *desk = m_doc->deskRecord();
    SimMidiDevice session(m_doc, m_stub, QStringLiteral("Network Session 1"), 2);
    QVERIFY2(session.plug(), qPrintable(session.lastError()));
    const QString hold = desk->learnRefusal(D::HazardHold, 2, 60);
    const QString kill = desk->learnRefusal(D::HazardKill, 2, 60);
    QVERIFY2(hold.contains(QStringLiteral("network")), qPrintable(hold));
    QVERIFY2(kill.isEmpty(), qPrintable(kill));
    QVERIFY(session.unplug());
}

void HazardArming_Test::aHoldHeldTooLongTimesOut()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    go();
    waitForCheck();
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    go();
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 255);
    // Held past the limit (15 s in a show; 2.5 s here): as let go.
    QTest::qWait(2800);
    tick(1);
    QCOMPARE(at("CO2 jet"), 0);
    QVERIFY2(k->hazardChipText().contains(QStringLiteral("Hold timed out — release and press again")),
             qPrintable(k->hazardChipText()));
    // Still down: no fire. Let go and pressed again: the same cue fires.
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    tick(1);
    QCOMPARE(at("CO2 jet"), 0);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    tick(1);
    QCOMPARE(at("CO2 jet"), 255);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::teardownUnderLoad()
{
    // A second show with a live hazard and its MasterTimer running,
    // deleted mid-frame: nothing runs on what the keeper owned.
    Doc *d = new Doc(this);
    Fixture *fx = new Fixture(d);
    fx->setName(QStringLiteral("Fogger"));
    fx->setChannels(1);
    fx->setAddress(0);
    QVERIFY(d->addFixture(fx));
    Scene *sc = new Scene(d);
    sc->setValue(fx->id(), 0, 255);
    d->addFunction(sc);
    d->outputRuleKeeper()->rebuild();
    d->outputRuleKeeper()->armHazards();
    d->masterTimer()->start();
    sc->start(d->masterTimer(), FunctionParent::master());
    QTest::qWait(100);
    QElapsedTimer t;
    t.start();
    delete d;
    QVERIFY(t.elapsed() < 5000);
}

void HazardArming_Test::aCrossfadeEndIsNoNewStep()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Operate);
    go();
    waitForCheck();
    go();                                   // the blast, unheld: skipped
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 0);
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    // A VC cue list's crossfade ending says the same step again.
    emit m_chaser->currentStepChanged(m_chaser->currentStepIndex());
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 0);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::everyStepOfAReachedListClosesTheGrants()
{
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    go();                                   // the preset: grants nothing
    waitForCheck();
    const quint32 before = k->m_hazardStep.load();
    go();                                   // a step of a list with a reach
    const quint32 after = k->m_hazardStep.load();
    QVERIFY2(after > before, qPrintable(QStringLiteral("%1 -> %2").arg(before).arg(after)));
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::aRunningHazardFollowsItsCuesFade()
{
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    for (int i = 0; i < 5; ++i)
    {
        go();
        waitForCheck();
    }
    QCOMPARE(at("Hazer"), 0);               // "Haze out"
    go();                                   // "Slow haze": 0 -> full over 2 s
    tick(10);
    const int mid = at("Hazer");
    QVERIFY2(mid > 0 && mid < 255, qPrintable(QString::number(mid)));
    tick(110);
    QCOMPARE(at("Hazer"), 255);
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::anEffectOnAHazardIsNoted()
{
    ShowTreeController *st = m_doc->showTree();
    const ShowTree::Cue *cue = nullptr;
    for (const ShowTree::Cue &c : st->cueList(m_list)->cues)
        if (c.id == m_c3)
            cue = &c;
    QVERIFY(cue != nullptr);
    Collection *coll = qobject_cast<Collection *>(m_doc->function(cue->collection));
    QVERIFY(coll != nullptr);
    EFX *efx = new EFX(m_doc);
    efx->setName(QStringLiteral("Jet sweep"));
    m_doc->addFunction(efx);
    EFXFixture *ef = new EFXFixture(efx);
    ef->setHead(GroupHead(named("CO2 jet")->id(), 0));
    efx->addFixture(ef);
    QVERIFY(coll->addFunction(efx->id()));
    ShowReview::Options o;
    o.checks = QStringList() << QStringLiteral("FIX-24");
    o.record = false;
    QStringList got;
    for (const ReviewFinding &f : m_doc->showReview()->run(o).findings)
        got << f.message;
    QVERIFY2(got.filter(QStringLiteral("Jet sweep does not drive CO2 jet")).size() == 1, qPrintable(got.join('\n')));
}

void HazardArming_Test::aLiveHazardIsLaidInPlace()
{
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    go();
    waitForCheck();
    go();
    waitForCheck();
    QCOMPARE(at("Hazer"), 255);
    Universe *u = m_doc->inputOutputMap()->universe(0);
    // Each fader by its token (unique per fader asked for: an address can
    // be reused by a new one).
    auto fireFaders = [u]() {
        QList<quint32> out;
        for (const QSharedPointer<GenericFader> &f : u->faders())
            if (!f.isNull() && !f->deleteRequested() && f->name().startsWith(QStringLiteral("Hazard fired")))
                out << f->token();
        return out;
    };
    const QList<quint32> before = fireFaders();
    QVERIFY(!before.isEmpty());
    k->rebuild();                           // the rig read again: nothing changed
    tick(1);
    QCOMPARE(fireFaders(), before);
    QCOMPARE(at("Hazer"), 255);
    m_doc->setMode(Doc::Design);
}

/****************************************************************************
 * The fourth review
 ****************************************************************************/

void HazardArming_Test::setHold(quint32 cue, uint ms)
{
    ShowTreeController *st = m_doc->showTree();
    QList<ShowTree::Cue> cues = st->cueList(m_list)->cues;
    for (ShowTree::Cue &c : cues)
        if (c.id == cue)
        {
            c.hold = ms;
            QVERIFY(st->setCue(c));
        }
    m_chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(m_list)->chaser));
    QVERIFY(m_chaser != nullptr);
}

void HazardArming_Test::aStepFromAnotherRouteNeverFires()
{
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    go();                                   // the preset, from the Run
    waitForCheck();
    // Design: the list plays on (its sources still the master's).
    m_doc->setMode(Doc::Design);
    QVERIFY(m_chaser->isRunning());
    QVERIFY(k->m_hazardArmed.load());
    // A step as the programmer's step button (or any VC widget) takes it.
    ChaserAction next;
    next.m_action = ChaserNextStep;
    next.m_stepIndex = 0;
    next.m_masterIntensity = 1.0;
    next.m_stepIntensity = 1.0;
    next.m_fadeMode = Chaser::FromFunction;
    m_chaser->setAction(next);
    tick(3);
    waitForCheck();
    QCOMPARE(m_chaser->currentStepIndex(), 1);
    QCOMPARE(at("Hazer"), 0);
    QVERIFY2(k->hazardSkippedText().contains(QStringLiteral("Hazer held — fire it from Run")),
             qPrintable(k->hazardSkippedText()));
}

void HazardArming_Test::aWebStartNeverFires()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    setHold(m_c1, 100);                     // the preset goes on by itself
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    // As the web's setFunctionStatus starts it.
    m_chaser->start(m_doc->masterTimer(), FunctionParent::master());
    for (int i = 0; i < 20 && m_chaser->currentStepIndex() != 1; ++i)
        tick(1);
    waitForCheck();
    QCOMPARE(m_chaser->currentStepIndex(), 1);
    QCOMPARE(at("CO2 jet"), 0);
    QCOMPARE(at("Hazer"), 0);
    QCOMPARE(at("Stage wash"), 128);        // the cue itself plays
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::aStartupFunctionNeverFires()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    QVERIFY(bindRole(D::HazardArm, QStringLiteral("2-Down")));
    setHold(m_c1, 100);
    m_doc->setStartupFunction(m_chaser->id());
    m_doc->setMode(Doc::Operate);           // starts it
    QVERIFY(m_pmj->tap(QStringLiteral("2-Down")));
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    for (int i = 0; i < 20 && m_chaser->currentStepIndex() != 1; ++i)
        tick(1);
    waitForCheck();
    QVERIFY(m_chaser->isRunning());
    QCOMPARE(m_chaser->currentStepIndex(), 1);
    QCOMPARE(at("CO2 jet"), 0);
    QCOMPARE(at("Hazer"), 0);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->setStartupFunction(Function::invalidId());
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::aGoStillFiresInDesignAndOperate()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    for (Doc::Mode mode : { Doc::Design, Doc::Operate })
    {
        m_doc->setMode(mode);
        go();                               // the preset
        waitForCheck();
        QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
        go();                               // the blast, held
        waitForCheck();
        QVERIFY2(at("CO2 jet") == 255, mode == Doc::Design ? "Design" : "Operate");
        QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
        tick(1);
        QCOMPARE(at("CO2 jet"), 0);
        m_chaser->stop(FunctionParent::master());
        tick(3);
    }
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::anAutoFollowAfterAHeldGoStillFires()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    setHold(m_c2, 100);                     // the blast goes on to "After"
    m_doc->setMode(Doc::Operate);
    go();
    waitForCheck();
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    go();                                   // the blast, held
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 255);
    for (int i = 0; i < 20 && m_chaser->currentStepIndex() != 2; ++i)
        tick(1);
    waitForCheck();
    QCOMPARE(m_chaser->currentStepIndex(), 2);
    // "After" tracks the jet on, reached held: still out.
    QCOMPARE(at("CO2 jet"), 255);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    tick(1);
    QCOMPARE(at("CO2 jet"), 0);
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::anEffectChaseNeverClosesTheFireLayers()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    K *k = m_doc->outputRuleKeeper();
    m_doc->setMode(Doc::Operate);
    go();
    waitForCheck();
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    go();                                   // the blast, held
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 255);
    // A wash chase stepping every 60 ms (an effect's, a VC button's).
    Scene *up = new Scene(m_doc), *down = new Scene(m_doc);
    up->setValue(named("Stage wash")->id(), 0, 255);
    down->setValue(named("Stage wash")->id(), 0, 20);
    m_doc->addFunction(up);
    m_doc->addFunction(down);
    Chaser *chase = new Chaser(m_doc);
    chase->setName(QStringLiteral("Wash chase"));
    m_doc->addFunction(chase);
    chase->addStep(ChaserStep(up->id()));
    chase->addStep(ChaserStep(down->id()));
    chase->setDuration(60);
    chase->start(m_doc->masterTimer(), FunctionParent(FunctionParent::ManualVCWidget, 9));
    const quint32 before = k->m_hazardStep.load();
    // The frames on their own thread, the main thread busy (no event loop).
    int lowest = 255, steps = 0, last = -1;
    std::thread([&]() {
        for (int i = 0; i < 40; ++i)
        {
            tick(1);
            lowest = qMin(lowest, at("CO2 jet"));
            if (chase->currentStepIndex() != last)
            {
                last = chase->currentStepIndex();
                ++steps;
            }
        }
    }).join();
    QVERIFY2(steps >= 5, qPrintable(QString::number(steps)));
    QCOMPARE(lowest, 255);
    // The main thread free again: the chase's steps (queued) close nothing.
    QCoreApplication::processEvents();
    QCOMPARE(k->m_hazardStep.load(), before);
    tick(3);
    QCOMPARE(at("CO2 jet"), 255);
    chase->stop(FunctionParent(FunctionParent::ManualVCWidget, 9));
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Design);
}

/****************************************************************************
 * The fifth review
 ****************************************************************************/

quint32 HazardArming_Test::jetSubCues()
{
    // Cue 4 ("Low haze": the jet out) with a sub-cue that sends the jet.
    ShowTreeController *st = m_doc->showTree();
    const quint32 s1 = st->addSubCue(m_c4, QStringLiteral("Jet hit"));
    if (s1 == ShowTree::invalidId())
        return 0;
    ShowTree::Target t;
    for (FixtureGroup *g : m_doc->fixtureGroups())
        if (g->name() == QLatin1String("CO2 jet"))
            t.groups << g->id();
    quint32 full = 0;
    for (QLCPalette *p : m_doc->palettes())
        if (p->name() == QLatin1String("Full"))
            full = p->id();
    if (!st->setTargetLook(s1, t, st->lookForPalette(full)))
        return 0;
    m_doc->outputRuleKeeper()->rebuild();
    m_chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(m_list)->chaser));
    const ShowTree::CueList *subs = st->cueList(st->subCueList(m_c4));
    return subs != nullptr ? subs->chaser : 0;
}

void HazardArming_Test::aSubCueStartedAloneNeverFires()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    const quint32 subId = jetSubCues();
    Chaser *sub = qobject_cast<Chaser *>(m_doc->function(subId));
    QVERIFY(sub != nullptr);
    m_doc->setMode(Doc::Operate);
    go();                                   // the preset: the list is the Run's
    waitForCheck();
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    // Cue 4's sub-cues started alone (the web's setFunctionStatus, a VC
    // button, the Function Manager).
    sub->start(m_doc->masterTimer(), FunctionParent::master());
    tick(5);
    waitForCheck();
    QVERIFY(sub->isRunning());
    QCOMPARE(at("CO2 jet"), 0);
    sub->stop(FunctionParent::master());
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::aSubCueTheRunBringsStillFires()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    QVERIFY(jetSubCues() != 0);
    m_doc->setMode(Doc::Operate);
    for (int i = 0; i < 3; ++i)             // to "After", unheld
    {
        go();
        waitForCheck();
    }
    QCOMPARE(at("CO2 jet"), 0);
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    go();                                   // "Low haze": its first sub-cue sends the jet
    tick(3);
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 255);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    tick(1);
    QCOMPARE(at("CO2 jet"), 0);
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::aStepInFlightKeepsItsOwnStamp()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Operate);
    go();                                   // the preset
    waitForCheck();
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    // A programmer/VC step, taken by the runner...
    ChaserAction next;
    next.m_action = ChaserNextStep;
    next.m_stepIndex = 0;
    next.m_masterIntensity = 1.0;
    next.m_stepIntensity = 1.0;
    next.m_fadeMode = Chaser::FromFunction;
    m_chaser->setAction(next);
    // ...while a GO's stamp lands before that step is started (the GO's own
    // action not yet taken): the stamp alone, as the race leaves it.
    m_chaser->m_runSerial.store(m_chaser->m_routeSerial.load());
    tick(3);
    waitForCheck();
    QCOMPARE(m_chaser->currentStepIndex(), 1);
    QCOMPARE(at("CO2 jet"), 0);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Design);
}

void HazardArming_Test::aGoDuringFadeAndStopStillFires()
{
    QVERIFY(bindRole(D::HazardHold, QStringLiteral("1-Down")));
    // The preset sends the jet here: a GO that restarts the list plays it.
    {
        ShowTreeController *st = m_doc->showTree();
        ShowTree::Target t;
        for (FixtureGroup *g : m_doc->fixtureGroups())
            if (g->name() == QLatin1String("CO2 jet"))
                t.groups << g->id();
        quint32 full = 0;
        for (QLCPalette *p : m_doc->palettes())
            if (p->name() == QLatin1String("Full"))
                full = p->id();
        QVERIFY(st->setTargetLook(m_c1, t, st->lookForPalette(full)));
        m_chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(m_list)->chaser));
    }
    m_doc->setMode(Doc::Operate);
    go();                                   // the preset, unheld: skipped
    waitForCheck();
    QCOMPARE(at("CO2 jet"), 0);
    // Fade and stop, as MasterTimer::fadeAndStopAll() starts it: the faders
    // fade out, the list asked to stop...
    {
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        for (Universe *u : ua)
            u->setFaderFadeOut(1000);
        m_doc->inputOutputMap()->releaseUniverses();
    }
    m_chaser->stop(FunctionParent::master());
    // ...and a GO from the Run before it has, the Hold held.
    QVERIFY(m_pmj->press(QStringLiteral("1-Down")));
    ShowControl::Request r;
    r.command = ShowControl::Go;
    r.chaser = m_chaser->id();
    m_doc->showControl()->execute(r);
    tick(5);
    waitForCheck();
    // The GO started the list again (its first cue, as a GO after a stop
    // does): that GO's, so held, the jet is out.
    QVERIFY(m_chaser->isRunning());
    QCOMPARE(m_chaser->currentStepIndex(), 0);
    QCOMPARE(at("CO2 jet"), 255);
    QVERIFY(m_pmj->release(QStringLiteral("1-Down")));
    m_doc->setMode(Doc::Design);
}

QTEST_GUILESS_MAIN(HazardArming_Test)

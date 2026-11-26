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
#include "doc.h"

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
                  out = level(QStringLiteral("Out"), 0);
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
    QCOMPARE(k->hazardChipText(), QStringLiteral("Hazards: DISARMED"));
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
    QCOMPARE(k->hazardsSkipped(), QList<quint32>() << named("CO2 jet")->id());
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
    QCOMPARE(k->hazardChipText(), QStringLiteral("Hazards: DISARMED"));
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
    QCOMPARE(k->hazardChipText(), QStringLiteral("Hazards: DISARMED"));
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
                              "CO2 jet fired (Hold held)", "Hazer on (armed)", "back to safe",
                              "Kill: every hazard off, disarmed" })
        QVERIFY2(log.contains(QLatin1String(line)), qPrintable(QStringLiteral("%1 not in:\n%2").arg(line, log)));
    m_doc->setMode(Doc::Design);
}

QTEST_GUILESS_MAIN(HazardArming_Test)

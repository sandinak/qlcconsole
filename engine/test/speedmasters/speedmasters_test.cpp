/*
  Q Light Controller Plus
  speedmasters_test.cpp

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
#include <QtTest>

#include <cmath>

#define private public                  // MasterTimer::timerTick()
#include "mastertimer.h"
#undef private
#include "speedmasters.h"
#include "chaserstep.h"
#include "collection.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "speedmasters_test.h"

void SpeedMasters_Test::init()
{
    m_doc = new Doc(this);
    m_doc->masterTimer()->setBpm(120);
}

void SpeedMasters_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void SpeedMasters_Test::freshMastersRunOnTheirOwnAt120()
{
    SpeedMasters *sm = m_doc->speedMasters();
    for (int i = 0; i < SpeedMasters::Count; ++i)
    {
        QCOMPARE(sm->mode(i), SpeedMasters::Own);
        QCOMPARE(sm->bpm(i), 120.0);
        QCOMPARE(sm->name(i), QString("Speed %1").arg(i + 1));
    }
    QVERIFY(sm->isDefault());
    QCOMPARE(sm->assignmentName(SpeedMasters::Master1 + 2), QString("Speed 3"));
    QCOMPARE(sm->assignmentName(SpeedMasters::MainClock), QString("Main clock"));
}

void SpeedMasters_Test::anOwnMasterRunsAtItsTempoAndRate()
{
    SpeedMasters *sm = m_doc->speedMasters();
    sm->setBpm(0, 600);                         // ten beats a second
    const double from = sm->beatPosition(0);
    QTest::qWait(300);
    const double beats = sm->beatPosition(0) - from;
    QVERIFY2(beats > 2.5 && beats < 4.5, qPrintable(QString::number(beats)));

    sm->setRate(0, 0.5);
    QCOMPARE(sm->bpm(0), 300.0);
    QCOMPARE(sm->setBpm(0), 600.0);
    // A tempo or rate change carries on from where it is.
    const double before = sm->beatPosition(0);
    sm->setBpm(0, 60);
    QVERIFY(qAbs(sm->beatPosition(0) - before) < 0.05);
}

void SpeedMasters_Test::aFollowerIsLockedToWhatItFollows()
{
    SpeedMasters *sm = m_doc->speedMasters();
    MasterTimer *mt = m_doc->masterTimer();
    QVERIFY(sm->setFollow(1, SpeedMasters::MainClock, 2.0));
    QCOMPARE(sm->bpm(1), 240.0);
    QVERIFY(qAbs(sm->beatPosition(1) - 2.0 * mt->beatPosition()) < 0.02);

    // Half time off master 2: on every other one of its beats.
    QVERIFY(sm->setFollow(2, 1, 0.5));
    QCOMPARE(sm->bpm(2), 120.0);
    QVERIFY(qAbs(sm->beatPosition(2) - mt->beatPosition()) < 0.02);

    // A tap on the main clock moves every follower's beat with it.
    mt->tap();
    const double f = sm->beatPosition(1) - std::floor(sm->beatPosition(1));
    QVERIFY2(f < 0.05 || f > 0.95, qPrintable(QString::number(f)));
}

void SpeedMasters_Test::followLoopsAreRefused()
{
    SpeedMasters *sm = m_doc->speedMasters();
    QVERIFY(sm->setFollow(1, 0, 1.0));
    QVERIFY(sm->setFollow(2, 1, 1.0));
    QVERIFY(!sm->setFollow(0, 2, 1.0));          // 0 → 2 → 1 → 0
    QVERIFY(!sm->setFollow(3, 3, 1.0));          // itself
    QVERIFY(!sm->setFollow(3, 0, 0.0));          // no ratio
    QVERIFY(!sm->setFollow(3, 7, 1.0));          // no such master
    QCOMPARE(sm->mode(0), SpeedMasters::Own);
}

void SpeedMasters_Test::pauseFreezesAndResumes()
{
    SpeedMasters *sm = m_doc->speedMasters();
    sm->setBpm(0, 600);
    sm->setPaused(0, true);
    const double frozen = sm->beatPosition(0);
    QCOMPARE(sm->bpm(0), 0.0);
    QTest::qWait(100);
    QCOMPARE(sm->beatPosition(0), frozen);
    sm->setPaused(0, false);                     // carries on from there
    QVERIFY(sm->beatPosition(0) - frozen < 0.2);

    // A follower drops back onto what it follows.
    QVERIFY(sm->setFollow(1, 0, 1.0));
    sm->setPaused(1, true);
    QTest::qWait(200);
    QVERIFY(sm->beatPosition(0) - sm->beatPosition(1) > 1.0);
    sm->setPaused(1, false);
    QVERIFY(qAbs(sm->beatPosition(0) - sm->beatPosition(1)) < 0.05);
}

void SpeedMasters_Test::tapSetsAnOwnMasterNotAFollower()
{
    SpeedMasters *sm = m_doc->speedMasters();
    sm->setRate(0, 0.5);
    for (int i = 0; i < 5; ++i)
    {
        if (i)
            QTest::qWait(250);                   // 240 BPM heard
        QVERIFY(sm->tap(0));
    }
    QVERIFY2(sm->bpm(0) > 200 && sm->bpm(0) < 260, qPrintable(QString::number(sm->bpm(0))));
    QCOMPARE(sm->rate(0), 0.5);                  // the fader stays put
    const double f = sm->beatPosition(0) - std::floor(sm->beatPosition(0));
    QVERIFY2(f < 0.05 || f > 0.95, qPrintable(QString::number(f)));

    QVERIFY(sm->setFollow(1, SpeedMasters::MainClock));
    QVERIFY(!sm->tap(1));
}

void SpeedMasters_Test::goingOwnCarriesOnFromTheFollow()
{
    SpeedMasters *sm = m_doc->speedMasters();
    QVERIFY(sm->setFollow(0, SpeedMasters::MainClock, 2.0));
    const double before = sm->beatPosition(0);
    sm->setOwn(0);
    QCOMPARE(sm->mode(0), SpeedMasters::Own);
    QCOMPARE(sm->bpm(0), 240.0);
    QVERIFY(qAbs(sm->beatPosition(0) - before) < 0.05);
}

void SpeedMasters_Test::mastersAreSavedWithTheShow()
{
    SpeedMasters *sm = m_doc->speedMasters();
    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        QVERIFY(sm->saveXML(&w));
    }
    QVERIFY(xml.isEmpty());                      // nothing set: nothing written

    sm->setName(0, "Strobes");
    sm->setBpm(0, 128.5);
    sm->setRate(0, 0.75);
    QVERIFY(sm->setFollow(1, 0, 0.5));
    QVERIFY(sm->setFollow(2, SpeedMasters::MainClock, 4));
    sm->setPaused(3, true);
    {
        QXmlStreamWriter w(&xml);
        QVERIFY(sm->saveXML(&w));
    }

    Doc other(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other.speedMasters()->loadXML(r));
    SpeedMasters *o = other.speedMasters();
    QCOMPARE(o->name(0), QString("Strobes"));
    QCOMPARE(o->setBpm(0), 128.5);
    QCOMPARE(o->rate(0), 0.75);
    QCOMPARE(o->mode(1), SpeedMasters::Follow);
    QCOMPARE(o->follows(1), 0);
    QCOMPARE(o->ratio(1), 0.5);
    QCOMPARE(o->follows(2), int(SpeedMasters::MainClock));
    QCOMPARE(o->ratio(2), 4.0);
    QVERIFY(o->isPaused(3));
    QCOMPARE(o->name(3), QString("Speed 4"));
}

void SpeedMasters_Test::assignmentsAreSavedOnFunctionsAndSteps()
{
    Scene *scene = new Scene(m_doc);
    scene->setName("Look");
    scene->setSpeedMaster(SpeedMasters::Master1 + 1);
    QVERIFY(m_doc->addFunction(scene));

    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        QVERIFY(scene->saveXML(&w));
    }
    QVERIFY(xml.contains("SpeedMaster=\"2\""));
    Doc other(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(Function::loader(r, &other));
    QCOMPARE(other.functions().first()->speedMaster(), int(SpeedMasters::Master1 + 1));

    ChaserStep step(scene->id());
    step.speedMaster = SpeedMasters::MainClock;
    QCOMPARE(ChaserStep::fromVariant(step.toVariant()).speedMaster, int(SpeedMasters::MainClock));
    ChaserStep copy(step);
    QCOMPARE(copy.speedMaster, int(SpeedMasters::MainClock));
    QByteArray sx;
    {
        QXmlStreamWriter w(&sx);
        QVERIFY(step.saveXML(&w, 0, false));
    }
    QVERIFY2(sx.contains("SpeedMaster=\"Main\""), sx.constData());
    QXmlStreamReader sr(sx);
    sr.readNextStartElement();
    ChaserStep back;
    int number = 0;
    QVERIFY(back.loadXML(sr, number, m_doc));
    QCOMPARE(back.speedMaster, int(SpeedMasters::MainClock));

    // A plain step says nothing about it.
    ChaserStep plain(scene->id());
    QByteArray px;
    {
        QXmlStreamWriter w(&px);
        plain.saveXML(&w, 0, false);
    }
    QVERIFY(!px.contains("SpeedMaster"));
}

void SpeedMasters_Test::theInnermostChoiceWins()
{
    Scene scene(m_doc);
    QCOMPARE(scene.effectiveSpeedMaster(), int(SpeedMasters::Inherit));
    scene.setInheritedSpeedMaster(SpeedMasters::Master1 + 2);
    QCOMPARE(scene.effectiveSpeedMaster(), int(SpeedMasters::Master1 + 2));
    scene.setSpeedMaster(SpeedMasters::Master1);
    QCOMPARE(scene.effectiveSpeedMaster(), int(SpeedMasters::Master1));
    scene.setSpeedMaster(SpeedMasters::Free);    // "not on a master", explicitly
    QCOMPARE(scene.effectiveSpeedMaster(), int(SpeedMasters::Free));
}

void SpeedMasters_Test::aChaseHandsItsMasterToItsSteps()
{
    MasterTimer *timer = m_doc->masterTimer();
    Scene *a = new Scene(m_doc);
    Scene *b = new Scene(m_doc);
    m_doc->addFunction(a);
    m_doc->addFunction(b);
    Chaser *chase = new Chaser(m_doc);
    chase->setSpeedMaster(SpeedMasters::Master1 + 1);
    ChaserStep sa(a->id(), 0, 1000, 0);
    ChaserStep sb(b->id(), 0, 1000, 0);
    sb.speedMaster = SpeedMasters::Master1 + 3;  // this step says otherwise
    chase->addStep(sa);
    chase->addStep(sb);
    m_doc->addFunction(chase);

    // What each step's look runs on, seen as it starts (an empty look stops
    // again at once, and stopping drops what it borrowed).
    int seenA = -100, seenB = -100;
    connect(a, &Function::running, this, [&]() { seenA = a->effectiveSpeedMaster(); });
    connect(b, &Function::running, this, [&]() { seenB = b->effectiveSpeedMaster(); });

    chase->start(timer, FunctionParent::master());
    for (int i = 0; i < 10 && seenA == -100; ++i)
        timer->timerTick();
    QCOMPARE(seenA, int(SpeedMasters::Master1 + 1));

    ChaserAction next;
    next.m_action = ChaserNextStep;
    next.m_masterIntensity = 1.0;
    next.m_stepIntensity = 1.0;
    next.m_fadeMode = Chaser::FromFunction;
    next.m_stepIndex = -1;
    chase->setAction(next);
    for (int i = 0; i < 10 && seenB == -100; ++i)
        timer->timerTick();
    QCOMPARE(seenB, int(SpeedMasters::Master1 + 3));

    chase->stop(FunctionParent::master());
    for (int i = 0; i < 10; ++i)
        timer->timerTick();
    // Stopped, the borrowed choice is dropped.
    QCOMPARE(a->effectiveSpeedMaster(), int(SpeedMasters::Inherit));
}

void SpeedMasters_Test::aCollectionHandsItsMasterToItsMembers()
{
    MasterTimer *timer = m_doc->masterTimer();
    Scene *a = new Scene(m_doc);
    Scene *own = new Scene(m_doc);
    own->setSpeedMaster(SpeedMasters::Free);
    m_doc->addFunction(a);
    m_doc->addFunction(own);
    Collection *col = new Collection(m_doc);
    col->addFunction(a->id());
    col->addFunction(own->id());
    col->setSpeedMaster(SpeedMasters::MainClock);
    m_doc->addFunction(col);

    int seenA = -100, seenOwn = -100;
    connect(a, &Function::running, this, [&]() { seenA = a->effectiveSpeedMaster(); });
    connect(own, &Function::running, this, [&]() { seenOwn = own->effectiveSpeedMaster(); });
    col->start(timer, FunctionParent::master());
    for (int i = 0; i < 10 && (seenA == -100 || seenOwn == -100); ++i)
        timer->timerTick();
    QCOMPARE(seenA, int(SpeedMasters::MainClock));
    QCOMPARE(seenOwn, int(SpeedMasters::Free));      // its own choice wins
    col->stop(FunctionParent::master());
    for (int i = 0; i < 10; ++i)
        timer->timerTick();
}

/* A chase that counts in beats, on a master, steps on that master's beats --
   its tap and rate move it, and it holds while the master is paused. */
void SpeedMasters_Test::aBeatChaseStepsOnItsMastersBeats()
{
    MasterTimer *timer = m_doc->masterTimer();
    QList<Scene *> looks;
    Chaser *chase = new Chaser(m_doc);
    for (int i = 0; i < 4; ++i)
    {
        Scene *s = new Scene(m_doc);
        m_doc->addFunction(s);
        looks << s;
        chase->addStep(ChaserStep(s->id(), 0, 1000, 0));    // one beat each
    }
    chase->setTempoType(Function::Beats);
    chase->setDurationMode(Chaser::PerStep);
    chase->setSpeedMaster(SpeedMasters::Master1);
    m_doc->addFunction(chase);
    // The main clock is off: only the master can step it.
    timer->setBeatSourceType(MasterTimer::None);
    SpeedMasters *sm = m_doc->speedMasters();
    sm->setBpm(0, 600);                                     // ten beats a second

    int steps = 0;
    connect(chase, &Chaser::currentStepChanged, this, [&steps](int) { steps++; });
    chase->start(timer, FunctionParent::master());
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 600)
    {
        timer->timerTick();
        QTest::qWait(10);
    }
    QVERIFY2(steps >= 3, qPrintable(QString("%1 steps in 0.6 s at 600 BPM").arg(steps)));

    // Paused: it holds.
    sm->setPaused(0, true);
    for (int i = 0; i < 5; ++i)
        timer->timerTick();
    const int held = steps;
    t.restart();
    while (t.elapsed() < 400)
    {
        timer->timerTick();
        QTest::qWait(10);
    }
    QCOMPARE(steps, held);

    chase->stop(FunctionParent::master());
    for (int i = 0; i < 5; ++i)
        timer->timerTick();
}

/* PLAYBACK_TIMING_DESIGN.md, P1. A chase of four manual steps (held until a
   GO), started; GOs asked for between beats. */
static Chaser *manualChase(Doc *doc, QList<Scene *> *looks = nullptr)
{
    Chaser *chase = new Chaser(doc);
    for (int i = 0; i < 4; ++i)
    {
        Scene *s = new Scene(doc);
        doc->addFunction(s);
        if (looks)
            *looks << s;
        chase->addStep(ChaserStep(s->id(), 0, Function::infiniteSpeed(), 0));
    }
    chase->setDurationMode(Chaser::PerStep);
    doc->addFunction(chase);
    return chase;
}

static void go(Chaser *chase)
{
    ChaserAction next;
    next.m_action = ChaserNextStep;
    next.m_masterIntensity = 1.0;
    next.m_stepIntensity = 1.0;
    next.m_fadeMode = Chaser::FromFunction;
    next.m_stepIndex = -1;
    chase->setAction(next);
}

void SpeedMasters_Test::aGoLandsOnTheNextBeat()
{
    MasterTimer *timer = m_doc->masterTimer();
    timer->setBpm(60);                               // a beat a second
    Chaser *chase = manualChase(m_doc);
    chase->setGoQuantize(Chaser::GoOnBeat);
    chase->start(timer, FunctionParent::master());
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QCOMPARE(chase->currentStepIndex(), 0);

    // Just after a beat: the GO waits about a second.
    timer->alignBeat(1.0);
    QTest::qWait(150);
    go(chase);
    go(chase);                                       // twice in one beat: one step
    QElapsedTimer t;
    t.start();
    while (chase->currentStepIndex() == 0 && t.elapsed() < 2000)
    {
        timer->timerTick();
        QTest::qWait(10);
    }
    QCOMPARE(chase->currentStepIndex(), 1);
    QVERIFY2(t.elapsed() > 650 && t.elapsed() < 1100,
             qPrintable(QString("landed %1 ms after the GO, the beat was ~850 away").arg(t.elapsed())));
    // ...and on the beat.
    const double pos = timer->beatPosition();
    QVERIFY2(pos - std::floor(pos) < 0.1, qPrintable(QString::number(pos)));
    for (int i = 0; i < 20; ++i)
        timer->timerTick();
    QCOMPARE(chase->currentStepIndex(), 1);          // not two steps

    // At once: as before.
    chase->setGoQuantize(Chaser::GoNow);
    go(chase);
    timer->timerTick();
    QCOMPARE(chase->currentStepIndex(), 2);

    chase->stop(FunctionParent::master());
    for (int i = 0; i < 5; ++i)
        timer->timerTick();
}

void SpeedMasters_Test::aGoOnTheBarWaitsForTheBar()
{
    MasterTimer *timer = m_doc->masterTimer();
    timer->setBpm(240);                              // a bar a second
    Chaser *chase = manualChase(m_doc);
    chase->setGoQuantize(Chaser::GoOnBar);
    chase->start(timer, FunctionParent::master());
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QTest::qWait(30);
    go(chase);
    QElapsedTimer t;
    t.start();
    bool sawBeatsPass = false;
    const double from = timer->beatPosition();
    while (chase->currentStepIndex() == 0 && t.elapsed() < 2500)
    {
        timer->timerTick();
        if (timer->beatPosition() - from > 1.0)
            sawBeatsPass = true;
        QTest::qWait(10);
    }
    QCOMPARE(chase->currentStepIndex(), 1);
    // It landed on a bar line: a whole number of bars (4 beats).
    const double bars = timer->beatPosition() / 4.0;
    QVERIFY2(bars - std::floor(bars) < 0.1, qPrintable(QString::number(bars)));
    Q_UNUSED(sawBeatsPass);
    chase->stop(FunctionParent::master());
    for (int i = 0; i < 5; ++i)
        timer->timerTick();
}

void SpeedMasters_Test::goQuantizeIsSavedWithTheChase()
{
    Chaser *chase = manualChase(m_doc);
    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        QVERIFY(chase->saveXML(&w));
    }
    QVERIFY(!xml.contains("GoQuantize"));            // at once: not written
    chase->setGoQuantize(Chaser::GoOnBar);
    xml.clear();
    {
        QXmlStreamWriter w(&xml);
        QVERIFY(chase->saveXML(&w));
    }
    QVERIFY(xml.contains("GoQuantize=\"Bar\""));
    Chaser back(m_doc);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(back.loadXML(r));
    QCOMPARE(back.goQuantize(), Chaser::GoOnBar);
}

QTEST_MAIN(SpeedMasters_Test)

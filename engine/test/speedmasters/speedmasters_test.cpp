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
#include "qlcpalette.h"
#include "chaserstep.h"
#include "collection.h"
#include "show.h"
#include "track.h"
#include "showfunction.h"
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
    for (int i = 0; i < SpeedMasters::MaxCount; ++i)
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
    double pos = 0;
    qint64 landed = 0;
    while (t.elapsed() < 2000)
    {
        timer->timerTick();
        if (chase->currentStepIndex() != 0)
        {
            pos = timer->beatPosition();             // the moment it moved
            landed = t.elapsed();
            break;
        }
        QTest::qWait(10);
    }
    QCOMPARE(chase->currentStepIndex(), 1);
    QVERIFY2(landed > 650 && landed < 1100,
             qPrintable(QString("landed %1 ms after the GO, the beat was ~850 away").arg(landed)));
    // ...and on the beat.
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
    double bars = 0;
    while (t.elapsed() < 2500)
    {
        timer->timerTick();
        if (chase->currentStepIndex() != 0)
        {
            bars = timer->beatPosition() / 4.0;      // the moment it moved
            break;
        }
        QTest::qWait(10);
    }
    QCOMPARE(chase->currentStepIndex(), 1);
    // It landed on a bar line: a whole number of bars (4 beats).
    QVERIFY2(bars - std::floor(bars) < 0.1, qPrintable(QString::number(bars)));
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

/* "yes .. 4-16 seems right" */
void SpeedMasters_Test::aShowUsesFourToSixteen()
{
    SpeedMasters *sm = m_doc->speedMasters();
    QCOMPARE(sm->count(), 4);
    sm->setCount(8);
    QCOMPARE(sm->count(), 8);
    sm->setCount(2);
    QCOMPARE(sm->count(), 4);                        // no fewer than four
    sm->setCount(40);
    QCOMPARE(sm->count(), 16);                       // no more than sixteen
    QVERIFY(!sm->isDefault());
    // Master 12 is a master like any other.
    QVERIFY(SpeedMasters::isMaster(SpeedMasters::Master1 + 11));
    sm->setBpm(11, 90);
    QCOMPARE(sm->bpm(11), 90.0);

    sm->setCount(6);
    sm->setName(5, "Six");
    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        QVERIFY(sm->saveXML(&w));
    }
    QVERIFY(xml.contains("Count=\"6\""));
    QVERIFY(!xml.contains("Index=\"6\""));           // only the ones in use
    Doc other(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other.speedMasters()->loadXML(r));
    QCOMPARE(other.speedMasters()->count(), 6);
    QCOMPARE(other.speedMasters()->name(5), QString("Six"));
}

/* "I don't see timecode in there": a master can follow the show timeline --
   its BPM grid at its playhead -- so whatever drives the show (timecode)
   drives the master. */
void SpeedMasters_Test::aMasterFollowsTheShowTimeline()
{
    SpeedMasters *sm = m_doc->speedMasters();
    MasterTimer *timer = m_doc->masterTimer();
    QVERIFY(sm->setFollow(0, SpeedMasters::ShowTimeline, 1.0));
    // No show running: the main clock.
    QVERIFY(qAbs(sm->beatPosition(0) - timer->beatPosition()) < 0.02);
    QCOMPARE(sm->bpm(0), timer->bpm());

    // A show with something on it (an empty one stops at once).
    Scene *look = new Scene(m_doc);
    m_doc->addFunction(look);
    Show *show = new Show(m_doc);
    show->setTimeDivision(Show::BPM_4_4, 150);       // 400 ms a beat
    QVERIFY(m_doc->addFunction(show));
    Track *track = new Track(Function::invalidId(), show);
    ShowFunction *item = new ShowFunction(show->getLatestShowFunctionId());
    item->setFunctionID(look->id());
    item->setStartTime(0);
    item->setDuration(60000);
    track->addShowFunction(item);
    show->addTrack(track);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 60; ++i)                     // 1.2 s of ticks
        timer->timerTick();
    QVERIFY(show->isRunning());
    const double expect = double(show->runningTime()) / 400.0;
    QVERIFY2(expect > 1.0, qPrintable(QString::number(show->runningTime())));
    QVERIFY2(qAbs(sm->beatPosition(0) - expect) < 0.01,
             qPrintable(QString("%1 vs %2").arg(sm->beatPosition(0)).arg(expect)));
    QCOMPARE(sm->bpm(0), 150.0);
    // Saved as following the timeline.
    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        sm->saveXML(&w);
    }
    QVERIFY(xml.contains("Follows=\"Timeline\""));
    show->stop(FunctionParent::master());
    for (int i = 0; i < 5; ++i)
        timer->timerTick();
}

/* "in the window have mapping to midi targets": Tap, Rate and Pause, each
   from an input channel, saved with the show. */
void SpeedMasters_Test::aControlSurfaceDrivesAMaster()
{
    SpeedMasters *sm = m_doc->speedMasters();
    sm->setInput(1, SpeedMasters::TapControl, 0, 10);
    sm->setInput(1, SpeedMasters::RateControl, 0, 11);
    sm->setInput(1, SpeedMasters::PauseControl, 0, 12);
    quint32 u = 0, c = 0;
    QVERIFY(sm->input(1, SpeedMasters::RateControl, &u, &c));
    QCOMPARE(u, quint32(0));
    QCOMPARE(c, quint32(11));
    QVERIFY(!sm->input(0, SpeedMasters::RateControl, nullptr, nullptr));

    // The fader: top is 200 %, the middle 100 %, the bottom stopped.
    emit m_doc->inputOutputMap()->inputValueChanged(0, 11, 255);
    QCOMPARE(sm->rate(1), 2.0);
    emit m_doc->inputOutputMap()->inputValueChanged(0, 11, 128);
    QCOMPARE(sm->rate(1), 1.0);
    emit m_doc->inputOutputMap()->inputValueChanged(0, 11, 0);
    QCOMPARE(sm->rate(1), 0.0);
    sm->setRate(1, 1.0);

    // Pause: a press toggles it, the release does nothing.
    emit m_doc->inputOutputMap()->inputValueChanged(0, 12, 127);
    QVERIFY(sm->isPaused(1));
    emit m_doc->inputOutputMap()->inputValueChanged(0, 12, 0);
    QVERIFY(sm->isPaused(1));
    emit m_doc->inputOutputMap()->inputValueChanged(0, 12, 127);
    emit m_doc->inputOutputMap()->inputValueChanged(0, 12, 0);
    QVERIFY(!sm->isPaused(1));

    // Tap: presses half a second apart are 120 BPM; releases aren't taps.
    sm->setBpm(1, 60);
    for (int i = 0; i < 3; ++i)
    {
        if (i > 0)
            QTest::qWait(500);
        emit m_doc->inputOutputMap()->inputValueChanged(0, 10, 127);
        emit m_doc->inputOutputMap()->inputValueChanged(0, 10, 0);
    }
    QVERIFY2(qAbs(sm->setBpm(1) - 120.0) < 8.0, qPrintable(QString::number(sm->setBpm(1))));
    QCOMPARE(sm->isPaused(1), false);                // the tap channel only taps
    QCOMPARE(sm->rate(0), 1.0);                      // and only master 2 moved

    // Saved with the show.
    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        QVERIFY(sm->saveXML(&w));
    }
    Doc other(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other.speedMasters()->loadXML(r));
    QVERIFY(other.speedMasters()->input(1, SpeedMasters::PauseControl, &u, &c));
    QCOMPARE(c, quint32(12));
    QVERIFY(other.speedMasters()->input(1, SpeedMasters::TapControl, &u, &c));
    QCOMPARE(c, quint32(10));

    sm->clearInput(1, SpeedMasters::TapControl);
    QVERIFY(!sm->input(1, SpeedMasters::TapControl, nullptr, nullptr));
}

/* "If the masters are not used .. they should not fire or be visible" */
void SpeedMasters_Test::onlyTheMastersSomethingIsOnAreUsed()
{
    SpeedMasters *sm = m_doc->speedMasters();
    QCOMPARE(sm->usedMask(), quint32(0));            // a fresh show: none

    Scene *look = new Scene(m_doc);
    m_doc->addFunction(look);
    look->setSpeedMaster(SpeedMasters::Master1);     // a look on Speed 1
    QCOMPARE(sm->usedMask(), quint32(0x1));
    look->setSpeedMaster(SpeedMasters::MainClock);   // the main clock isn't a master
    QCOMPARE(sm->usedMask(), quint32(0));

    Chaser *chase = new Chaser(m_doc);
    ChaserStep step(look->id());
    step.speedMaster = SpeedMasters::Master1 + 2;    // a chase step on Speed 3
    chase->addStep(step);
    m_doc->addFunction(chase);
    QCOMPARE(sm->usedMask(), quint32(0x4));

    QLCPalette *fx = new QLCPalette(QLCPalette::Effect, m_doc);
    fx->setEffectParamValue(QStringLiteral("__speedMaster"), SpeedMasters::Master1 + 1);
    QVERIFY(m_doc->addPalette(fx));                  // an effect on Speed 2
    QCOMPARE(sm->usedMask(), quint32(0x6));
    fx->setEffectParamValue(QStringLiteral("__speedMaster"), SpeedMasters::Inherit);
    QCOMPARE(sm->usedMask(), quint32(0x6));          // cached: no edit was signalled...
    m_doc->setModified();                            // ...as the look editor does
    QCOMPARE(sm->usedMask(), quint32(0x4));

    QVERIFY(sm->setFollow(2, 3, 2.0));               // Speed 3 follows Speed 4: both
    QCOMPARE(sm->usedMask(), quint32(0xC));
    QVERIFY(sm->setFollow(1, 0, 1.0));               // a follower nothing uses: neither
    QCOMPARE(sm->usedMask(), quint32(0xC));

    sm->setInput(1, SpeedMasters::TapControl, 0, 3); // a surface drives Speed 2
    QCOMPARE(sm->usedMask(), quint32(0xF));          // and so Speed 1, which it follows
    QVERIFY(sm->isUsed(0));

    look->setSpeedMaster(SpeedMasters::Master1 + 7); // Speed 8, with 4 in use: not counted
    QCOMPARE(sm->usedMask() & 0x80u, quint32(0));
    sm->setCount(8);
    QCOMPARE(sm->usedMask() & 0x80u, quint32(0x80));
}

QTEST_MAIN(SpeedMasters_Test)

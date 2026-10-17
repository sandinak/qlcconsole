/*
  Q Light Controller Plus
  cuehandover_test.cpp

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

#include <QSignalSpy>
#include <QtTest>

#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private
#include "inputoutputmap.h"
#include "functionparent.h"
#include "collection.h"
#include "chaserstep.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "cuehandover_test.h"

/* The Show Tree's premise (SHOW_TREE_DESIGN.md §4): a cue is a Collection
   holding its complete output, and a layer that is the same in two cues is the
   SAME Scene in both Collections. On GO from one cue to the next that Scene
   must keep running -- no stop/start, no re-fade from zero, its elapsed time
   (and with it an effect's phase) carrying on -- while what only the old cue
   held still goes. */

void CueHandover_Test::init()
{
    m_doc = new Doc(this);
    m_fixture = new Fixture(m_doc);
    m_fixture->setAddress(0);
    m_fixture->setUniverse(0);
    m_fixture->setChannels(3);
    m_doc->addFixture(m_fixture);
}

void CueHandover_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(5);
    delete m_doc;
    m_doc = nullptr;
    m_fixture = nullptr;
}

void CueHandover_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
    {
        m_doc->masterTimer()->timerTick();
        // One universe frame per tick, as the universe thread does.
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
}

int CueHandover_Test::level(int channel)
{
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    const uchar v = uchar(ua.first()->preGMValues().at(channel));
    m_doc->inputOutputMap()->releaseUniverses(false);
    return v;
}

static ChaserAction stepAction(ChaserActionType type, int index = -1)
{
    ChaserAction action;
    action.m_action = type;
    action.m_stepIndex = index;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    return action;
}

void CueHandover_Test::go(Chaser *c)
{
    ChaserAction action = stepAction(ChaserNextStep);
    c->setAction(action);
    if (!c->isRunning())
        c->start(m_doc->masterTimer(), FunctionParent::master());
}

void CueHandover_Test::back(Chaser *c)
{
    ChaserAction action = stepAction(ChaserPreviousStep);
    c->setAction(action);
}

void CueHandover_Test::jump(Chaser *c, int index)
{
    ChaserAction action = stepAction(ChaserSetStepIndex, index);
    c->setAction(action);
}

Scene *CueHandover_Test::scene(const QString &name, int channel, int value, uint fadeIn)
{
    Scene *s = new Scene(m_doc);
    s->setName(name);
    s->setValue(m_fixture->id(), quint32(channel), uchar(value));
    s->setFadeInSpeed(fadeIn);
    m_doc->addFunction(s);
    return s;
}

Collection *CueHandover_Test::cue(const QString &name, const QList<Scene *> &members)
{
    Collection *c = new Collection(m_doc);
    c->setName(name);
    m_doc->addFunction(c);
    for (Scene *s : members)
        c->addFunction(s->id());
    return c;
}

Chaser *CueHandover_Test::list(const QList<Collection *> &cues, uint fadeIn)
{
    Chaser *c = new Chaser(m_doc);
    c->setName("Cue list");
    c->setFadeInMode(Chaser::Common);
    c->setFadeInSpeed(fadeIn);
    c->setFadeOutMode(Chaser::Common);
    c->setFadeOutSpeed(0);
    c->setDurationMode(Chaser::Common);
    c->setDuration(Function::infiniteSpeed());     // each cue holds until GO
    m_doc->addFunction(c);
    for (Collection *cue : cues)
        c->addStep(ChaserStep(cue->id()));
    return c;
}

/* Cue 1 = {Shared, A}, cue 2 = {Shared, B}. The cue fade (400 ms) makes a
   restart visible: a re-started Shared would fade up from zero again. */
void CueHandover_Test::aSceneSharedByTwoCuesKeepsRunningOnGo()
{
    Scene *shared = scene("Shared", 0, 255);
    Scene *a = scene("A", 1, 100);
    Scene *b = scene("B", 1, 200);
    Chaser *c = list({ cue("1", { shared, a }), cue("2", { shared, b }) }, 400);

    go(c);
    tick(40);                                   // well past the fade
    QVERIFY(shared->isRunning());
    QCOMPARE(level(0), 255);
    QCOMPARE(level(1), 100);

    QSignalSpy stopped(shared, SIGNAL(stopped(quint32)));
    QSignalSpy started(shared, SIGNAL(running(quint32)));
    // What the effect runner creates and tears down an effect's instance on:
    // neither for Shared means its effects run on, phase and all.
    QSignalSpy timerStopped(m_doc->masterTimer(), SIGNAL(functionStopped(quint32)));
    QSignalSpy timerStarted(m_doc->masterTimer(), SIGNAL(functionStarted(quint32)));
    quint32 lastElapsed = shared->elapsed();

    go(c);
    for (int i = 0; i < 40; ++i)
    {
        tick();
        QVERIFY2(shared->isRunning(), qPrintable(QString("tick %1").arg(i)));
        QVERIFY2(level(0) == 255, qPrintable(QString("tick %1: %2").arg(i).arg(level(0))));
        // Its clock runs on: an effect on it keeps its phase.
        QVERIFY2(shared->elapsed() > lastElapsed, qPrintable(QString("tick %1").arg(i)));
        lastElapsed = shared->elapsed();
    }
    QCOMPARE(c->currentStepIndex(), 1);
    QCOMPARE(stopped.count(), 0);
    QCOMPARE(started.count(), 0);
    for (const QList<QVariant> &args : timerStopped)
        QVERIFY(args.first().toUInt() != shared->id());
    for (const QList<QVariant> &args : timerStarted)
        QVERIFY(args.first().toUInt() != shared->id());
    QVERIFY(!a->isRunning());
    QVERIFY(b->isRunning());
    QCOMPARE(level(1), 200);
}

void CueHandover_Test::aSceneSharedByTwoCuesKeepsRunningOnBack()
{
    Scene *shared = scene("Shared", 0, 255);
    Chaser *c = list({ cue("1", { shared, scene("A", 1, 100) }),
                       cue("2", { shared, scene("B", 1, 200) }) }, 400);
    go(c);
    tick(40);
    go(c);
    tick(40);
    QCOMPARE(c->currentStepIndex(), 1);

    QSignalSpy stopped(shared, SIGNAL(stopped(quint32)));
    back(c);
    for (int i = 0; i < 40; ++i)
    {
        tick();
        QVERIFY(shared->isRunning());
        QCOMPARE(level(0), 255);
    }
    QCOMPARE(c->currentStepIndex(), 0);
    QCOMPARE(stopped.count(), 0);
    QCOMPARE(level(1), 100);
}

void CueHandover_Test::aSceneSharedByTwoCuesKeepsRunningOnAJump()
{
    // A jump (show control, a click in the cue list) from cue 1 to cue 3.
    Scene *shared = scene("Shared", 0, 255);
    Chaser *c = list({ cue("1", { shared, scene("A", 1, 100) }),
                       cue("2", { scene("B", 1, 200) }),
                       cue("3", { shared, scene("C", 2, 50) }) }, 400);
    go(c);
    tick(40);

    QSignalSpy stopped(shared, SIGNAL(stopped(quint32)));
    jump(c, 2);
    for (int i = 0; i < 40; ++i)
    {
        tick();
        QVERIFY(shared->isRunning());
        QCOMPARE(level(0), 255);
    }
    QCOMPARE(c->currentStepIndex(), 2);
    QCOMPARE(stopped.count(), 0);
    QCOMPARE(level(1), 0);
    QCOMPARE(level(2), 50);
}

void CueHandover_Test::autoFollowHandsOverToo()
{
    Scene *shared = scene("Shared", 0, 255);
    Chaser *c = list({ cue("1", { shared, scene("A", 1, 100) }),
                       cue("2", { shared, scene("B", 1, 200) }) }, 400);
    c->setRunOrder(Function::SingleShot);
    c->setDuration(MasterTimer::tick() * 40);   // each cue follows on

    go(c);
    tick(30);
    QCOMPARE(c->currentStepIndex(), 0);
    QSignalSpy stopped(shared, SIGNAL(stopped(quint32)));
    for (int i = 0; i < 20; ++i)
    {
        tick();
        QVERIFY(shared->isRunning());
        QCOMPARE(level(0), 255);
    }
    QCOMPARE(c->currentStepIndex(), 1);
    QCOMPARE(stopped.count(), 0);
    tick(25);                                   // B's fade done
    QCOMPARE(level(1), 200);
}

void CueHandover_Test::theLeavingCuesIntensityIsGivenBack()
{
    // Cue 1 played at half (a cue list's fader), cue 2 at full: Shared is
    // cue 2's alone now, at cue 2's level -- not half of it.
    Scene *shared = scene("Shared", 0, 255);
    Chaser *c = list({ cue("1", { shared }), cue("2", { shared, scene("B", 1, 200) }) }, 0);

    ChaserAction half = stepAction(ChaserNextStep);
    half.m_masterIntensity = 0.5;
    c->setAction(half);
    c->start(m_doc->masterTimer(), FunctionParent::master());
    tick(5);
    QCOMPARE(shared->getAttributeValue(Function::Intensity), 0.5);

    go(c);
    tick(5);
    QVERIFY(shared->isRunning());
    QCOMPARE(c->currentStepIndex(), 1);
    QCOMPARE(shared->getAttributeValue(Function::Intensity), 1.0);
    QCOMPARE(level(0), 255);
}

void CueHandover_Test::aCueThatIncludesTheCueBeforeKeepsItRunning()
{
    // DanceHysteria today: 0.0.5 is a Collection of the 0.0.4 Collection
    // plus StepTops-Bright. GO from 0.0.4 to 0.0.5 must not restart 0.0.4.
    Scene *backStage = scene("BackStage", 0, 255);
    Collection *shout = cue("0.0.4", { backStage, scene("StepTops-Dim", 1, 60) });
    Collection *who = new Collection(m_doc);
    who->setName("0.0.5");
    m_doc->addFunction(who);
    who->addFunction(shout->id());
    Scene *bright = scene("StepTops-Bright", 1, 255);
    who->addFunction(bright->id());
    Chaser *c = list({ shout, who }, 400);

    go(c);
    tick(40);
    QSignalSpy stopped(backStage, SIGNAL(stopped(quint32)));
    QSignalSpy shoutStopped(shout, SIGNAL(stopped(quint32)));
    go(c);
    for (int i = 0; i < 40; ++i)
    {
        tick();
        QVERIFY(backStage->isRunning());
        QCOMPARE(level(0), 255);
    }
    QCOMPARE(stopped.count(), 0);
    QCOMPARE(shoutStopped.count(), 0);
    QCOMPARE(level(1), 255);                    // HTP: the brighter wins

    // And Back to 0.0.4: it was running all along inside 0.0.5.
    back(c);
    for (int i = 0; i < 40; ++i)
    {
        tick();
        QCOMPARE(level(0), 255);
    }
    QCOMPARE(stopped.count(), 0);
    QVERIFY(!bright->isRunning());
}

void CueHandover_Test::theOldCuesOwnScenesStillStop()
{
    Scene *shared = scene("Shared", 0, 255);
    Scene *a = scene("A", 1, 100);
    Scene *b = scene("B", 2, 200);
    Collection *one = cue("1", { shared, a });
    Collection *two = cue("2", { shared, b });
    Chaser *c = list({ one, two }, 0);

    go(c);
    tick(5);
    go(c);
    tick(5);
    QVERIFY(!one->isRunning());
    QVERIFY(two->isRunning());
    QVERIFY(!a->isRunning());
    QCOMPARE(level(1), 0);
    QCOMPARE(level(2), 200);

    // Shared now belongs to cue 2 alone: GO on into a cue without it stops it.
    Scene *d = scene("D", 1, 10);
    Collection *three = cue("3", { d });
    c->addStep(ChaserStep(three->id()));
    go(c);
    tick(5);
    QVERIFY(!shared->isRunning());
    QCOMPARE(level(0), 0);
    QCOMPARE(level(1), 10);
}

void CueHandover_Test::stoppingTheListStopsTheSharedScene()
{
    Scene *shared = scene("Shared", 0, 255);
    Chaser *c = list({ cue("1", { shared, scene("A", 1, 100) }),
                       cue("2", { shared, scene("B", 1, 200) }) }, 0);
    go(c);
    tick(5);
    go(c);
    tick(5);
    QVERIFY(shared->isRunning());

    c->stop(FunctionParent::master());
    tick(5);
    QVERIFY(!c->isRunning());
    QVERIFY(!shared->isRunning());
    QCOMPARE(level(0), 0);
    QCOMPARE(level(1), 0);
}

void CueHandover_Test::cuesWithNothingSharedChangeAsBefore()
{
    Scene *a = scene("A", 0, 255);
    Scene *b = scene("B", 1, 200);
    Collection *one = cue("1", { a });
    Collection *two = cue("2", { b });
    Chaser *c = list({ one, two }, 0);

    go(c);
    tick(5);
    QSignalSpy aStopped(a, SIGNAL(stopped(quint32)));
    go(c);
    tick(5);
    QCOMPARE(aStopped.count(), 1);
    QVERIFY(!one->isRunning());
    QCOMPARE(level(0), 0);
    QCOMPARE(level(1), 200);
}

QTEST_MAIN(CueHandover_Test)

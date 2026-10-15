/*
  Q Light Controller Plus
  subcues_test.cpp

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
#include <QElapsedTimer>
#include <QtTest>
#include <cmath>

#define private public
#include "mastertimer.h"
#undef private
#include "functionparent.h"
#include "chaserstep.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "subcues_test.h"

/* Sub-cues (SUBCUES_DESIGN.md, P1): a chase step's own steps as 4.1, 4.2,
   stepped through on a Sub GO while GO goes on to cue 5. */

void SubCues_Test::init()
{
    m_doc = new Doc(this);
}

void SubCues_Test::cleanup()
{
    // Stop everything before the Doc goes, as a show closing does.
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(5);
    delete m_doc;
    m_doc = nullptr;
}

Chaser *SubCues_Test::makeInner(int steps)
{
    Chaser *c = new Chaser(m_doc);
    c->setName("Inner");
    c->setDurationMode(Chaser::Common);
    c->setDuration(MasterTimer::tick() * 3);     // runs on, three ticks a step
    m_doc->addFunction(c);
    for (int i = 0; i < steps; ++i)
    {
        Scene *s = new Scene(m_doc);
        s->setName(QString("Inner %1").arg(i + 1));
        m_doc->addFunction(s);
        c->addStep(ChaserStep(s->id()));
    }
    return c;
}

Chaser *SubCues_Test::makeList(int steps, int chaseAt, Chaser *inner, int mode)
{
    Chaser *c = new Chaser(m_doc);
    c->setName("Cue list");
    c->setDurationMode(Chaser::Common);
    c->setDuration(Function::infiniteSpeed());   // each cue holds until GO
    m_doc->addFunction(c);
    for (int i = 0; i < steps; ++i)
    {
        if (i == chaseAt && inner != nullptr)
        {
            ChaserStep step(inner->id());
            step.subCues = ChaserStep::SubCueMode(mode);
            c->addStep(step);
            continue;
        }
        Scene *s = new Scene(m_doc);
        s->setName(QString("Cue %1").arg(i + 1));
        m_doc->addFunction(s);
        c->addStep(ChaserStep(s->id()));
    }
    return c;
}

void SubCues_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
        m_doc->masterTimer()->timerTick();
}

void SubCues_Test::go(Chaser *c)
{
    // As the VC cue list and show control GO: the action, then start it.
    ChaserAction action;
    action.m_action = ChaserNextStep;
    action.m_stepIndex = -1;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    c->setAction(action);
    if (!c->isRunning())
        c->start(m_doc->masterTimer(), FunctionParent::master());
}

void SubCues_Test::back(Chaser *c)
{
    ChaserAction action;
    action.m_action = ChaserPreviousStep;
    action.m_stepIndex = -1;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    c->setAction(action);
}

void SubCues_Test::goIntoASubGoStepHoldsItsFirstSubCue()
{
    Chaser *inner = makeInner(3);
    Chaser *list = makeList(3, 1, inner, ChaserStep::SubGo);
    QVERIFY(list->isSubGoStep(1));
    QVERIFY(!list->isSubGoStep(0));
    QVERIFY(list->hasSubGoSteps());

    go(list);
    tick();
    QCOMPARE(list->currentStepIndex(), 0);
    QCOMPARE(list->currentSubCueIndex(), -1);   // a scene: no sub-cues
    QCOMPARE(list->nextSubCueIndex(), -1);

    go(list);
    tick();
    QCOMPARE(list->currentStepIndex(), 1);
    QVERIFY(inner->isRunning());
    QVERIFY(inner->isSubCueHeld());
    QCOMPARE(list->currentSubCueIndex(), 0);
    QCOMPARE(list->nextSubCueIndex(), 1);
    QCOMPARE(list->previousSubCueIndex(), -1);

    // Its holds (three ticks a step) aren't counted: it stays.
    tick(30);
    QCOMPARE(list->currentSubCueIndex(), 0);
    QCOMPARE(list->currentStepIndex(), 1);
}

void SubCues_Test::subGoAndSubBackStopAtTheEnds()
{
    Chaser *inner = makeInner(3);
    Chaser *list = makeList(3, 1, inner, ChaserStep::SubGo);
    go(list);
    tick();
    go(list);
    tick();
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(list->currentSubCueIndex(), 0);

    list->subGo();
    tick();
    QCOMPARE(list->currentSubCueIndex(), 1);
    list->subGo();
    tick();
    QCOMPARE(list->currentSubCueIndex(), 2);
    QCOMPARE(list->nextSubCueIndex(), -1);       // the last: "end of 2"
    list->subGo();
    tick(10);
    QCOMPARE(list->currentSubCueIndex(), 2);     // nothing
    QCOMPARE(list->currentStepIndex(), 1);       // and the cue stays

    list->subBack();
    tick();
    QCOMPARE(list->currentSubCueIndex(), 1);
    list->subBack();
    tick();
    QCOMPARE(list->currentSubCueIndex(), 0);
    list->subBack();
    tick(10);
    QCOMPARE(list->currentSubCueIndex(), 0);
    QCOMPARE(list->currentStepIndex(), 1);
}

void SubCues_Test::skippedSubCuesArePassedOver()
{
    Chaser *inner = makeInner(4);
    inner->setStepSkipped(0, true);
    inner->setStepSkipped(2, true);
    Chaser *list = makeList(2, 1, inner, ChaserStep::SubGo);
    go(list);
    tick();
    go(list);
    tick();
    QCOMPARE(list->currentSubCueIndex(), 1);     // the first playable
    list->subGo();
    tick();
    QCOMPARE(list->currentSubCueIndex(), 3);     // past 2
    list->subBack();
    tick();
    QCOMPARE(list->currentSubCueIndex(), 1);
    QCOMPARE(list->previousSubCueIndex(), -1);   // 0 is skipped

    // A jump straight to a skipped one plays it.
    list->jumpToSubCue(1, 2);
    tick();
    QCOMPARE(list->currentSubCueIndex(), 2);
}

void SubCues_Test::goAndBackStopTheInnerChase()
{
    Chaser *inner = makeInner(3);
    Chaser *list = makeList(3, 1, inner, ChaserStep::SubGo);
    go(list);
    tick();
    go(list);
    tick();
    list->subGo();
    tick();
    QCOMPARE(list->currentSubCueIndex(), 1);

    go(list);                                   // sub-cues left: dropped
    tick();
    QCOMPARE(list->currentStepIndex(), 2);
    QVERIFY(!inner->isRunning());
    QVERIFY(!inner->isSubCueHeld());
    QCOMPARE(list->currentSubCueIndex(), -1);

    back(list);                                 // back into it: from its first
    tick();
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(list->currentSubCueIndex(), 0);
    back(list);
    tick();
    QCOMPARE(list->currentStepIndex(), 0);
    QVERIFY(!inner->isRunning());

    // A Sub GO with a GO in the same tick: the GO's cue doesn't get it.
    go(list);
    tick();
    QCOMPARE(list->currentStepIndex(), 1);
    list->subGo();
    go(list);
    tick();
    QCOMPARE(list->currentStepIndex(), 2);
    back(list);
    tick();
    QCOMPARE(list->currentSubCueIndex(), 0);
}

void SubCues_Test::autoFollowLeavesWithSubCuesLeft()
{
    Chaser *inner = makeInner(3);
    Chaser *list = makeList(3, 1, inner, ChaserStep::SubGo);
    list->setDuration(MasterTimer::tick() * 8); // each cue follows on
    list->setRunOrder(Function::SingleShot);

    QList<int> played;
    bool heldAtOne = false;
    list->start(m_doc->masterTimer(), FunctionParent::master());
    for (int i = 0; i < 60; ++i)
    {
        tick(1);
        const int now = list->currentStepIndex();
        if (list->isRunning() && now >= 0 && (played.isEmpty() || played.last() != now))
            played << now;
        if (now == 1 && list->currentSubCueIndex() == 0)
            heldAtOne = true;
        if (now == 1)
            QVERIFY(list->currentSubCueIndex() <= 0);   // held, never ran on
    }
    QCOMPARE(played, QList<int>({ 0, 1, 2 }));
    QVERIFY(heldAtOne);
    QVERIFY(!inner->isRunning());
}

/* Regression: an Auto step (every step in an existing show) is as ever --
   its chase runs on by itself, and Sub GO leaves it alone. */
void SubCues_Test::anAutoStepRunsOnItsOwnTiming()
{
    Chaser *inner = makeInner(3);
    inner->setRunOrder(Function::Loop);
    Chaser *list = makeList(3, 1, inner, ChaserStep::Auto);
    QVERIFY(!list->isSubGoStep(1));
    QVERIFY(!list->hasSubGoSteps());
    QCOMPARE(list->subCueCount(1), 3);          // still listed under it

    go(list);
    tick();
    go(list);
    tick();
    QVERIFY(inner->isRunning());
    QVERIFY(!inner->isSubCueHeld());
    QCOMPARE(list->nextSubCueIndex(), -1);      // no Sub GO for it

    QList<int> seen;
    for (int i = 0; i < 30; ++i)
    {
        tick(1);
        const int now = inner->currentStepIndex();
        if (seen.isEmpty() || seen.last() != now)
            seen << now;
    }
    QVERIFY2(seen.size() >= 4, qPrintable(QString("inner steps seen: %1").arg(seen.size())));
    for (int i = 1; i < seen.size(); ++i)          // 0, 1, 2, 0, 1... in turn
        QCOMPARE(seen.at(i), (seen.at(i - 1) + 1) % 3);
    QCOMPARE(list->currentStepIndex(), 1);

    go(list);
    tick();
    QVERIFY(!inner->isRunning());
}

void SubCues_Test::subGoWaitsForTheBeat()
{
    MasterTimer *timer = m_doc->masterTimer();
    timer->setBpm(60);                          // a beat a second
    Chaser *inner = makeInner(3);
    Chaser *list = makeList(2, 1, inner, ChaserStep::SubGo);
    go(list);
    tick();
    go(list);
    tick();
    QCOMPARE(list->currentSubCueIndex(), 0);
    list->setGoQuantize(Chaser::GoOnBeat);

    // Just after a beat: the Sub GO waits about a second.
    timer->alignBeat(1.0);
    QTest::qWait(150);
    // When the next beat is, as asked (a busy machine can sleep long).
    const double pos = timer->beatPosition();
    const qint64 toBeat = qint64((std::floor(pos) + 1.0 - pos) * 1000.0);
    QElapsedTimer t;
    t.start();
    list->subGo();
    tick(1);
    QCOMPARE(list->currentSubCueIndex(), 0);
    QVERIFY(toBeat < 100 || list->goHeld());
    qint64 landed = 0;
    while (t.elapsed() < 2000)
    {
        tick(1);
        if (list->currentSubCueIndex() != 0)
        {
            landed = t.elapsed();
            break;
        }
        QTest::qWait(10);
    }
    QCOMPARE(list->currentSubCueIndex(), 1);
    QVERIFY2(landed > toBeat - 100 && landed < toBeat + 300,
             qPrintable(QString("landed %1 ms after the Sub GO, the beat was %2 away")
                        .arg(landed).arg(toBeat)));
    QVERIFY(!list->goHeld());

    // A GO asked while a Sub GO waits waits for the beat too (and takes
    // the Sub GO's place: it was for the cue being left).
    timer->alignBeat(1.0);
    QTest::qWait(150);
    list->subGo();
    tick(1);
    go(list);
    tick(1);
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(list->currentSubCueIndex(), 1);
    t.restart();
    while (t.elapsed() < 2000 && list->currentStepIndex() == 1)
    {
        tick(1);
        QTest::qWait(10);
    }
    QVERIFY2(t.elapsed() > 600, qPrintable(QString::number(t.elapsed())));
    QCOMPARE(list->currentStepIndex(), 0);           // two steps: loops round to 0
    QCOMPARE(list->currentSubCueIndex(), -1);
    back(list);
    list->setGoQuantize(Chaser::GoNow);
    tick(1);
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(list->currentSubCueIndex(), 0);
    list->subGo();
    tick(1);
    QCOMPARE(list->currentSubCueIndex(), 1);

    // At once: at once.
    list->setGoQuantize(Chaser::GoNow);
    list->subGo();
    tick(1);
    QCOMPARE(list->currentSubCueIndex(), 2);
}

void SubCues_Test::subCuesAreNumberedUnderTheirParent()
{
    Chaser *inner = makeInner(3);
    Chaser *list = makeList(3, 1, inner, ChaserStep::SubGo);
    QCOMPARE(list->subCueCount(0), 0);
    QCOMPARE(list->subCue(0, 0), QString());
    QCOMPARE(list->subCue(1, 0), QString("2.1"));
    QCOMPARE(list->subCue(1, 2), QString("2.3"));
    QCOMPARE(list->subCue(1, 3), QString());

    ChaserStep parent = list->steps().at(1);
    parent.cueNumber = "4";
    list->replaceStep(parent, 1);
    QCOMPARE(list->subCue(1, 0), QString("4.1"));

    // An inner step's own number wins.
    ChaserStep own = inner->steps().at(2);
    own.cueNumber = "4.25";
    inner->replaceStep(own, 2);
    QCOMPARE(list->subCue(1, 2), QString("4.25"));
    QCOMPARE(list->subCue(1, 1), QString("4.2"));

    int index = -1, sub = -1;
    QVERIFY(list->findSubCue("4.2", &index, &sub));
    QCOMPARE(index, 1);
    QCOMPARE(sub, 1);
    QVERIFY(list->findSubCue("4.250", &index, &sub));
    QCOMPARE(sub, 2);
    QVERIFY(!list->findSubCue("4.3", &index, &sub));   // it has its own
    QVERIFY(!list->findSubCue("4", &index, &sub));     // a main cue, not a sub-cue

    QVERIFY(list->findCueOrSubCue("4.1", &index, &sub));
    QCOMPARE(index, 1);
    QCOMPARE(sub, 0);
    QVERIFY(list->findCueOrSubCue("4", &index, &sub));
    QCOMPARE(index, 1);
    QCOMPARE(sub, -1);

    // A main cue numbered 4.1 wins over sub-cue 1 of cue 4.
    ChaserStep main = list->steps().at(2);
    main.cueNumber = "4.1";
    list->replaceStep(main, 2);
    QVERIFY(list->findCueOrSubCue("4.1", &index, &sub));
    QCOMPARE(index, 2);
    QCOMPARE(sub, -1);
    QVERIFY(!list->findCueOrSubCue("9", &index, &sub));
}

void SubCues_Test::aJumpPlaysTheParentFirst()
{
    Chaser *inner = makeInner(3);
    Chaser *list = makeList(3, 1, inner, ChaserStep::SubGo);
    go(list);
    tick();
    QCOMPARE(list->currentStepIndex(), 0);

    int index = -1, sub = -1;
    QVERIFY(list->findCueOrSubCue("2.3", &index, &sub));
    list->jumpToSubCue(index, sub);
    tick();
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(list->currentSubCueIndex(), 2);
    QVERIFY(inner->isSubCueHeld());

    // Its parent playing: just the sub-cue.
    list->jumpToSubCue(1, 0);
    tick();
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(list->currentSubCueIndex(), 0);

    // A jump into an Auto step's chase: it carries on from there.
    ChaserStep step = list->steps().at(1);
    step.subCues = ChaserStep::Auto;
    list->replaceStep(step, 1);
    go(list);
    tick();
    list->jumpToSubCue(1, 2);
    tick(1);                                    // (three ticks a step)
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(list->currentSubCueIndex(), 2);
    QVERIFY(!inner->isSubCueHeld());
}

void SubCues_Test::aJumpFromStoppedStartsThere()
{
    Chaser *inner = makeInner(3);
    Chaser *list = makeList(3, 1, inner, ChaserStep::SubGo);

    list->subGo();                              // stopped: nothing to step
    list->jumpToSubCue(1, 1);
    list->start(m_doc->masterTimer(), FunctionParent::master());
    tick();
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(list->currentSubCueIndex(), 1);
    tick(20);
    QCOMPARE(list->currentSubCueIndex(), 1);
}

void SubCues_Test::subCuesAreSavedOnlyWhenSubGo()
{
    Chaser *inner = makeInner(2);
    Chaser *list = makeList(2, 1, inner, ChaserStep::SubGo);

    auto save = [](Chaser *c) {
        QByteArray xml;
        QXmlStreamWriter w(&xml);
        c->saveXML(&w);
        return xml;
    };

    QByteArray xml = save(list);
    QCOMPARE(xml.count("SubCues="), 1);
    QVERIFY(xml.contains("SubCues=\"SubGo\""));

    Chaser loaded(m_doc);
    {
        QXmlStreamReader r(xml);
        r.readNextStartElement();
        QVERIFY(loaded.loadXML(r));
    }
    QCOMPARE(loaded.stepSubCues(1), ChaserStep::SubGo);
    QCOMPARE(loaded.stepSubCues(0), ChaserStep::Auto);   // absent: Auto

    // Auto: not written, so older shows save as they were.
    list->setStepSubCues(1, ChaserStep::Auto);
    QVERIFY(!save(list).contains("SubCues"));

    // Copied with the step, and through a variant (the editor's clipboard).
    ChaserStep step(inner->id());
    step.subCues = ChaserStep::SubGo;
    QCOMPARE(ChaserStep(step).subCues, ChaserStep::SubGo);
    QCOMPARE(ChaserStep::fromVariant(step.toVariant()).subCues, ChaserStep::SubGo);
    QVariantList older = step.toVariant().toList().mid(0, 9);   // before sub-cues
    QCOMPARE(ChaserStep::fromVariant(older).subCues, ChaserStep::Auto);
    QCOMPARE(ChaserStep::fromVariant(older).fid, inner->id());

    QCOMPARE(ChaserStep::stringToSubCueMode("SubGo"), ChaserStep::SubGo);
    QCOMPARE(ChaserStep::stringToSubCueMode("anything"), ChaserStep::Auto);
}

/* Two cues in a row whose steps are the same chase: GO from one to the
   next stops and restarts that chase in one tick. The start on the first
   sub-cue (and a jump's sub-cue) must reach the runner that plays, not
   the one being torn down. Random run order: its natural first step is
   anywhere, so only the hand-over lands on the first sub-cue. */
void SubCues_Test::consecutiveCuesOnOneChaseStartWhereAsked()
{
    Chaser *inner = makeInner(10);
    inner->setRunOrder(Function::Random);
    Chaser *list = new Chaser(m_doc);
    list->setDurationMode(Chaser::Common);
    list->setDuration(Function::infiniteSpeed());
    m_doc->addFunction(list);
    for (int i = 0; i < 6; ++i)
    {
        ChaserStep step(inner->id());
        step.subCues = ChaserStep::SubGo;
        list->addStep(step);
    }

    go(list);
    tick();
    QCOMPARE(list->currentStepIndex(), 0);
    QCOMPARE(list->currentSubCueIndex(), 0);
    for (int cue = 1; cue < 6; ++cue)
    {
        list->subGo();
        tick(1);
        list->subGo();
        tick(1);
        QCOMPARE(list->currentSubCueIndex(), 2);

        // GO: the next cue, the same chase, from its first sub-cue.
        go(list);
        tick(1);
        QCOMPARE(list->currentStepIndex(), cue);
        QVERIFY(inner->isSubCueHeld());
        QCOMPARE(list->currentSubCueIndex(), 0);
        tick(10);
        QCOMPARE(list->currentSubCueIndex(), 0);         // and held there
    }

    // A jump from one cue's sub-cue into the next cue's 2nd sub-cue.
    list->jumpToSubCue(0, 3);
    tick(1);
    QCOMPARE(list->currentStepIndex(), 0);
    QCOMPARE(list->currentSubCueIndex(), 3);
    for (int cue = 1; cue < 6; ++cue)
    {
        list->jumpToSubCue(cue, 1);
        tick(1);
        QCOMPARE(list->currentStepIndex(), cue);
        QCOMPARE(list->currentSubCueIndex(), 1);
        tick(10);
        QCOMPARE(list->currentSubCueIndex(), 1);
    }
}

QTEST_MAIN(SubCues_Test)

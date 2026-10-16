/*
  Q Light Controller Plus
  subcues_test.h

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

#ifndef SUBCUES_TEST_H
#define SUBCUES_TEST_H

#include <QObject>

class Chaser;
class Doc;

class SubCues_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void goIntoASubGoStepHoldsItsFirstSubCue();
    void subGoAndSubBackStopAtTheEnds();
    void skippedSubCuesArePassedOver();
    void goAndBackStopTheInnerChase();
    void autoFollowLeavesWithSubCuesLeft();
    void anAutoStepRunsOnItsOwnTiming();
    void subGoWaitsForTheBeat();
    void subCuesAreNumberedUnderTheirParent();
    void aJumpPlaysTheParentFirst();
    void aJumpFromStoppedStartsThere();
    void subCuesAreSavedOnlyWhenSubGo();
    void consecutiveCuesOnOneChaseStartWhereAsked();

private:
    /** A cue list: steps that hold until GO, each a scene, but step
     *  @a chaseAt is @a inner, its sub-cues set to @a mode. */
    Chaser *makeList(int steps, int chaseAt, Chaser *inner, int mode);
    /** A chase of @a steps scenes that runs on by itself, a few ticks
     *  a step. */
    Chaser *makeInner(int steps);
    void tick(int n = 3);
    void go(Chaser *c);
    void back(Chaser *c);

    Doc *m_doc = nullptr;
};

#endif

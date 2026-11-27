/*
  Q Light Controller Plus
  showscale_test.h

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

#ifndef SHOWSCALE_TEST_H
#define SHOWSCALE_TEST_H

#include <QObject>

#include "scalerig.h"

class App;
class CountingOutput;

/** The gate's size check (board Q360, docs/project/STRATEGY.md guardrail 7):
 *  the whole app on a show the size and shape of the ones we run --
 *  engine/test/common/scalerig.h: 260 fixtures, 26,598 channels over 80
 *  universes, 50 of them holding one or two devices -- with a Show Tree
 *  cue list fading, an RGB matrix over every pixel, an EFX on every mover
 *  and three chasers all running, in Operate.
 *
 *  Budgets, each asserted and measured on the pool (the numbers and hosts
 *  are next to the asserts): the MasterTimer's frame at the configured rate,
 *  every universe sent every frame (a counting output, no network), the
 *  Lighting Studio's plot and the Rig Overview drawing at Operate's cap
 *  without holding the UI thread, memory flat over a minute of the show,
 *  and saving and loading the show. Serial (unittest-serial.txt): it
 *  measures wall-clock time. */
class ShowScale_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void theRigHasTheShowsShape();
    void theShowSavesAndLoadsInTime();
    void theEngineKeepsItsFrame();
    void theStudioAndOverviewKeepUp();
    void memoryStaysFlatOverAMinute();

private:
    /** Everything running, in Operate: the cue list on its first cue */
    void startShow();
    void stopShow();
    /** GO on the cue list: the next cue fades in */
    void go();

private:
    App *m_app = nullptr;
    CountingOutput *m_output = nullptr;
    ScaleRig::Built m_rig;
    double m_loadedMB = 0;      ///< resident memory with the show loaded, nothing run
};

#endif

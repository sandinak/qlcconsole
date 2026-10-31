/*
  Q Light Controller Plus
  hotpath_test.h

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

#ifndef HOTPATH_TEST_H
#define HOTPATH_TEST_H

#include <QObject>
#include <QList>

class Doc;
class Scene;
class Function;

/** The engine's hot paths, guarded two ways (docs/performance.md,
 *  "Keeping it fast").
 *
 *  Work counts, asserted: how many frames a universe sends, faders it holds,
 *  maps a matrix computes. They are deterministic, so they are ordinary gate
 *  tests: a change that makes the engine do more work per frame fails here
 *  whatever the machine.
 *
 *  CPU time per operation, reported only: this thread's CPU time (not wall
 *  clock) for a universe frame, a fader write, a scene tick, a script map
 *  and an input flush. Nothing is asserted on them, so they can't flake the
 *  gate; tools/perf/guard.py (make perf) runs this program from a Release
 *  build with QLC_PERF_REPORT=<file> and compares the numbers with the
 *  host's baseline. */
class HotPath_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    /* Work counts */
    void aStaticLookSendsOneFrameThenNone();
    void aRunningSceneHoldsOneFaderOfItsChannels();
    void startStopCyclesLeaveNoFadersBehind();
    void overrideHitCyclesLeaveNoFadersBehind();
    void aChaserKeepsItsFadersBounded();
    void aMatrixComputesAMapPerStepNotPerTick();

    /* The mixed rig (stresstest/engine/showbuilder.cpp, ShowSpec::MixedRig):
       movers, LED pars, pixel bars and strips, dimmers on 32 universes */
    void aPanTiltEfxWritesBothBytesOfEveryMover();
    void thePowerEstimateOfABigRigAddsUp();
    void theCapHoldsEveryParOfABigRig();
    void aGoInABigShowTreeMakesAndDeletesNothing();

    /* CPU time per operation (report only) */
    void cpuPerOperation();
    void cpuPerRigOperation();

private:
    Scene *makeLook(int universe, int fixtures, uchar level);
    void frame(int frames = 1);
    static void frame(Doc *doc, int frames = 1);
    /** The mixed rig, built once: 32 universes, nothing running */
    Doc *rig();
    void stopRig();
    /** A Show Tree of @a cues tracking cues over the rig's groups; its
     *  list's chaser id */
    quint32 bigShowTree(int cues);

private:
    Doc *m_doc = nullptr;
    /** Generic RGB fixtures, 170 per universe on universes 0 and 1 */
    QList<quint32> m_fixtures[2];
    Doc *m_rig = nullptr;
    QList<Function *> m_rigFunctions;
    quint32 m_rigTree = 0;      ///< bigShowTree()'s chaser, once made
    quint32 m_rigTreeList = 0;  ///< ...and its cue list
};

#endif

/*
  Q Light Controller Plus
  renderperf_test.h

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

#ifndef RENDERPERF_TEST_H
#define RENDERPERF_TEST_H

#include <QObject>
#include <QList>

class Doc;

/** What drawing the rig costs (docs/performance.md, "Keeping it fast"),
 *  offscreen, on the mixed rig (stresstest/engine/showbuilder.cpp,
 *  ShowSpec::MixedRig) on 8 universes: 240 fixtures across a stage.
 *
 *  Paint counts, asserted (the gate): a rig view left open on an idle show
 *  draws nothing -- the 3D/rig view (StructureStudioView) and the Lighting
 *  Studio's 2D plot, with the live timer really running. An idle show that
 *  redraws is how 3.8 cores went for 19 hours (the (idle) fix); an engine
 *  benchmark never sees it.
 *
 *  CPU per frame, reported only (QLC_PERF_REPORT=<file>, as
 *  engine/test/hotpath): the process's CPU time (the 3D view renders on a
 *  thread pool) for a frame with every fixture's values changed, in the
 *  angled view with beams, in plan, and in the 2D plot, and for an idle
 *  live tick. tools/perf/guard.py runs it as the point "gui". */
class RenderPerf_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void anIdleRigViewDrawsNothing();
    void anIdlePlotDrawsNothing();

    void cpuPerFrame();

private:
    /** Every fixture a step further on: new values to draw */
    void changeValues(int step);

private:
    Doc *m_doc = nullptr;
};

#endif

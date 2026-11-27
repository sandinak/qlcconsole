/*
  Q Light Controller Plus
  universesched_test.h

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

#ifndef UNIVERSESCHED_TEST_H
#define UNIVERSESCHED_TEST_H

#include <QObject>

class Doc;
class QLCIOPlugin;

/** How the universe threads are scheduled (Universe::setThreadPriority()):
 *  the setting and its override, a running thread taking a change, and --
 *  only with QLC_SCHED_AB=1 -- the A/B that chose the default, on a rig the
 *  shape of the shows we run (docs/performance.md#universe-threads). */
class UniverseSched_Test final : public QObject
{
    Q_OBJECT

public:
    /** @a plugin into @a doc's plugin cache, which then owns it */
    static void adoptOutput(Doc *doc, QLCIOPlugin *plugin);

private slots:
    void cleanup();

    void theSettingIsUsedUnlessTheEnvironmentSaysOtherwise();
    void aRunningUniverseTakesAChangeAtItsNextFrame();
    void theRigIsTheShowsSize();
    void abOnTheShowsRig_data();
    void abOnTheShowsRig();
    void matrixShareOfTheTick();
};

#endif

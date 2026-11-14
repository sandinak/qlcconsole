/*
  Q Light Controller Plus
  padrunrules_test.h

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

#ifndef PADRUNRULES_TEST_H
#define PADRUNRULES_TEST_H

#include <QObject>
#include <QList>
#include <QPair>

#include "showcontrol.h"

class IOPluginStub;
class PadRun;
class Doc;

/* The operator's seven Pad Run rules (padrun.h), tried the way a pad on
   stage breaks them rather than the way they were built: pads that drop
   mid-hold and come back, triggers that jitter or never rest at zero,
   presses that bounce or arrive twice, the four of the stop in every
   order, two pads and the same pad twice, tools and the Controller Test
   starting mid-chord, and a seeded random sweep of the whole map. Pad
   input goes in as a plugin's does (the stub's roles, the patch). */
class PadRunRules_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /* Rule 3 (and 4): the pad going away */
    void aLostPadDisarmsOnTheRealPath();
    void aSafetyHeldAsThePadDropsIsNoSafetyAfter();
    void aSwappedPadForgetsWhatTheOldOneHeld();
    void aPadBackShowsTheStateAgain();

    /* Rules 3 and 5: the chords */
    void theStopsOwnBumpersDoNotArm();
    void aTriggerInTheArmChordSpendsIt();
    void everyOrderOfTheFour();
    void holdingTheFourIsOneStop();
    void theStopWorksWhileWalking();
    void theStopWorksWhileAToolHasThePad();

    /* Rule 4: real pads' shapes */
    void triggersThatJitterAreOnePull();
    void theSamePressTwiceIsOneGo();
    void aBouncingButton();
    void theDuplicateDevice();

    /* Rules 2 and 6: one state, the light */
    void theLightAfterAToolOrWalkEndsStopped();
    void aFadingCueShowsPlayingForItsFade();

    /* Rule 7, and tools taking the pad mid-chord */
    void aChordHeldAsAToolStartsIsForgotten();
    void theControllerTestMidChord();
    void theControllerTestSwallowsUnroledButtons();
    void aButtonLetGoDuringTheTestIsLetGo();

    /* Rules 1, 3, 4 and 5 over random presses */
    void theRulesHoldOverRandomPresses_data();
    void theRulesHoldOverRandomPresses();

private:
    IOPluginStub *loadStub();
    /** Four spots, a list: Opening, Blackout (no layer), Ballad. */
    void makeShow();
    void press(quint32 channel, quint32 universe = 0);
    void axis(quint32 channel, uchar value, quint32 universe = 0);
    void hold(quint32 channel, bool down, quint32 universe = 0);
    void arm(bool on, quint32 universe = 0);
    /** The pad unplugged / plugged back, as InputOutputMap does it. */
    void drop(quint32 universe = 0);
    void back(quint32 universe = 0);
    int goes() const;
    int stops() const;
    /** Announcements said since @p from that contain @p part. */
    int saidCount(const QString &part) const;

private:
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    QList<QPair<ShowControl::Request, ShowControl::Route>> m_runs;
    QStringList m_said;
    quint32 m_list = 0, m_cue1 = 0, m_cue2 = 0;
};

#endif

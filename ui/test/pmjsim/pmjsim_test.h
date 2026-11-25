/*
  Q Light Controller Plus
  pmjsim_test.h

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

#ifndef PMJSIM_TEST_H
#define PMJSIM_TEST_H

#include <QObject>

class QTemporaryDir;
class IOPluginStub;
class SimMidiDevice;
class App;

/* The PMJ Black 1 as the MIDI it sends, in the whole app
   (docs/design/SURFACE_SIMULATOR.md): the board's every control against
   the overlay's table and the profile, every lamp it is sent, and the
   bench's three findings -- Go in the Run tab, a learned hit's lamp, the
   Grand Master fader. */
class PmjSim_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** The overlay's controls are the profile's, by channel and name. */
    void theOverlayIsTheProfile();
    /** Every control reaches the surface engine as its role, or is a key
     *  documented free (for a show to learn). */
    void everyControlReachesItsRole();
    /** Every control with a lamp is sent one, at a steady level; none is
     *  sent to a control without one; a key with no job is dark. */
    void everyLampIsSentAndNoneStray();
    /** The Master fader is the Grand Master, all the way. */
    void theMasterFaderMovesTheGrandMaster();
    /** Go on the board in the Run tab GOes the list (bench: did nothing). */
    void goInTheRunTabGoesTheList();
    /** A hit learned on a free key fires and lights its lamp, which stays
     *  lit through what repaints the board (bench: it stayed dark). */
    void aLearnedHitLightsItsLamp();
    /** Phase R: the board's dump after a replug (both shapes) GOes
     *  nothing; a release and press after the burst is one GO. */
    void theReplugDumpGoesNothingOnThePmj();
    /** This computer's STOP and GO on PMJ keys are lit by the Control Map
     *  (STOP lit always, blinking after ALL OFF); the overlay leaves them. */
    void theDesksStopIsLitOnThePmj();
    /** His rig (build 4667, 2026-11-25): the PMJ's input on any MIDI
     *  channel, its feedback line on the MIDI plugin's default (channel 1)
     *  and a DMX output on the same universe. Go and Back light on the
     *  board's channel 9, byte for byte, under Go and Back on his aligned
     *  board's LED table. */
    void hisRigLightsGoAndBackOnChannelNine();
    /** His board before the align (LEDs on channel 1 in factory order):
     *  no lamp the app sends lights its own button, and the channel-1
     *  lamps build 4667 sent lit the wrong ones (Effects under 1-Load). */
    void anUnalignedBoardLightsNoButtonOfItsOwn();

private:
    void makeShow();

    App *m_app = nullptr;
    QTemporaryDir *m_pluginDir = nullptr;
    IOPluginStub *m_stub = nullptr;
    SimMidiDevice *m_pmj = nullptr;
    quint32 m_list = 0;
    quint32 m_breathe = 0;
};

#endif

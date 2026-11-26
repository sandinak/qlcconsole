/*
  Q Light Controller Plus
  hazardarming_test.h

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

#ifndef HAZARDARMING_TEST_H
#define HAZARDARMING_TEST_H

#include <QObject>
#include <QTemporaryDir>

class Doc;
class Chaser;
class Fixture;
class IOPluginStub;
class SimMidiDevice;

/* Hazard arming (board Q352 A, Q353 A): a Burst fires only under the
   spotter's Hold, a Running one once armed until their Kill; every press
   comes from a simulated PMJ Black 1, as the board sends it. */
class HazardArming_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /** Two kinds in the one dropdown; guessed; saved only when set by
     *  hand; 0.5.3's one-kind "Hazard" loads as the guessed kind. */
    void twoKindsGuessedSetAndLoaded();
    /** A Burst fires its cue only while the Hold is held; let go, it is
     *  safe the very next frame. The chip says HELD, then DISARMED. */
    void aBurstFiresOnlyWhileHeld();
    /** A cue reached while not held skips the burst: the footer says
     *  "CO2 jet skipped — not held", and a hold after fires nothing. */
    void aCueReachedUnheldSkipsTheBurst();
    /** Armed, a Running hazard is on from its cue and stays on across
     *  cues; the Kill turns it off the same frame and disarms. */
    void armedRunningStaysOnUntilKill();
    /** Reached unarmed: skipped; arming then fires nothing until the next
     *  GO whose cue sets it. */
    void unarmedRunningWaitsForTheNextCue();
    /** The emergency stop, Stop ALL and another show opened disarm (a
     *  Hold still down counts only once let go of and pressed again). */
    void stopsAndOpeningDisarm();
    /** The spotter's board unplugged: every hazard safe and disarmed. */
    void theSpottersDeviceLostDisarms();
    /** Never on GO's, Back's or a STOP's control, the show's GO or stop,
     *  nor a computer key; learned on a button of its own. */
    void theControlsAreRefusedOnGoAndTheKeyboard();
    /** Park still wins; an edit pick in Design never fires, armed or not. */
    void parkAndEditsNeverFire();
    /** Show Review: an error when hazard cues have no Hold / Arm control;
     *  every hazard listed with its kind. */
    void showReviewSaysWhatIsMissing();
    /** Every fire, skip, arm, kill and disarm is in the hazard log. */
    void everyEventIsLogged();

private:
    void tick(int n = 1);
    int at(const char *fixture);
    void go();
    /** The GO's check, a few frames on */
    void waitForCheck();
    Fixture *named(const char *name) const;
    bool bindRole(int role, const QString &control);

    QTemporaryDir m_settings;
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    SimMidiDevice *m_pmj = nullptr;
    Chaser *m_chaser = nullptr;
    quint32 m_c1 = 0, m_c2 = 0, m_c3 = 0, m_list = 0;
};

#endif

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

    /* The second review (each fails on 4975d59a5). */
    /** P1: a GO to a cue that sets the jet and the hazer out leaves them
     *  out, whatever the Simple Desk or a stray scene sends. */
    void aCueThatSetsItOutLeavesItOut();
    /** P2: armed at the cue's 50, nothing raises it (the Simple Desk, a
     *  scene); a later cue that sets it to 0 puts it at 0; every output
     *  change is logged. */
    void onlyTheCuesOwnValueGoesOut();
    /** P3: a laser given class 3B in Fixture Properties is held at once
     *  and listed. */
    void aLaserClassSetLaterIsHeldAtOnce();
    /** P4: a step no GO made (the chaser's own step: auto-follow, the
     *  editor's Test, a timecode) takes the burst back; a Hold pressed
     *  after never fires it. */
    void everyStepTakesTheBurstBack();
    /** 5: Hold and Arm are refused on a network input; Kill is not. */
    void holdAndArmAreRefusedOnTheNetwork();
    /** 6: sleep and wake disarm; after it the Hold needs a fresh press. */
    void sleepAndWakeDisarm();
    /** 7: the Hold let go and the Kill reach the output with the main
     *  thread blocked (sent from the plugin's own thread). */
    void releaseAndKillNeedNoMainThread();
    /** 8: a guessed hazard renamed stays a hazard until set by hand. */
    void aRenamedHazardStaysAHazard();
    /** 9: the Hold is learned from a press held about a second; a quick
     *  tap and a press never let go are refused. */
    void theHoldIsLearnedFromAPressHeld();
    /** 10: a GO in Blind grants nothing. */
    void aGoInBlindGrantsNothing();
    /** 12: the Hold is refused on Kill's control; hazard cues with no Kill
     *  are flagged in Show Review. */
    void theHoldIsRefusedOnTheKill();
    /** A programmer edit never drives a hazard, after a Design run too. */
    void aProgrammerEditNeverDrivesAHazard();

    /* The third review (each fails on bce438577 unless said). */
    /** N1: a plain chaser stepped by a VC button or a Test never fires a
     *  hazard; the footer says to fire it from Run. */
    void aVcOrTestStepNeverFires();
    /** N2: another Doc made and deleted never kills this show's fast path. */
    void aSecondDocNeverKillsTheFastPath();
    /** N3: the Hold refused on a network MIDI session; held past its limit
     *  it counts as let go and the footer says so. */
    void holdIsRefusedOverTheAir();
    void aHoldHeldTooLongTimesOut();
    /** N4: a show with a live hazard deleted with its timer running. */
    void teardownUnderLoad();
    /** N5: the same step said again (a crossfade ending) is no new step. */
    void aCrossfadeEndIsNoNewStep();
    /** N6: every step of a list that has a reach closes the grants. */
    void everyStepOfAReachedListClosesTheGrants();
    /** N7: a Running hazard follows its cue's fade; an effect on a hazard
     *  is noted in Show Review. */
    void aRunningHazardFollowsItsCuesFade();
    void anEffectOnAHazardIsNoted();
    /** N8: a live hazard's fire layer laid in place, never anew. */
    void aLiveHazardIsLaidInPlace();

    /* The fourth review */
    /** R1: a step from another route, on a list a GO started, fires nothing. */
    void aStepFromAnotherRouteNeverFires();
    /** R2: a start alone (the web's, the startup function's) never fires. */
    void aWebStartNeverFires();
    void aStartupFunctionNeverFires();
    /** The Run's own GO still fires, in Design and in Operate; and the
     *  list going on by itself after a held GO. */
    void aGoStillFiresInDesignAndOperate();
    void anAutoFollowAfterAHeldGoStillFires();
    /** R4: an effect chase never closes the fire layers. */
    void anEffectChaseNeverClosesTheFireLayers();

private:
    void setHold(quint32 cue, uint ms);
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
    quint32 m_c1 = 0, m_c2 = 0, m_c3 = 0, m_c4 = 0, m_c5 = 0, m_c6 = 0, m_list = 0;
    /** The Simple Desk's own fader on @p fixture's channel 0 at @p value */
    void deskPush(const char *fixture, uchar value);
    class Scene *strayScene(int value);
};

#endif

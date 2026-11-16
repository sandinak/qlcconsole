/*
  Q Light Controller Plus
  padrun_test.h

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

#ifndef PADRUN_TEST_H
#define PADRUN_TEST_H

#include <QObject>
#include <QList>
#include <QPair>

#include "showcontrol.h"
#include "showtreemodel.h"

class IOPluginStub;
class PadRun;
class Doc;

/* The pad's Run role (0.4): a game controller runs the show through the one
   GO path (ShowControl::run, RoutePad), with the arm held. Synthetic pad
   input through the stub plugin, as a real pad's arrives. */
class PadRun_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** The table: roles to actions and back, which need the arm, and the
     *  map in each pad's own words. */
    void theMapByRole();
    /** The arm is a latch on LB + RB held: one bumper, a brush of both, or
     *  both with a trigger arm nothing; held, it arms and stays armed with
     *  everything let go; no single control, however held, disarms; the
     *  chord again does. */
    void theArmIsATwoButtonLatch();
    /** Armed, a button alone sends nothing and says what it needs; with
     *  the other trigger, or the safety only brushed, nothing; LT held + A
     *  is one GO through run() with RoutePad, LT + B Back, LT + X Sub GO,
     *  LT + Y Sub Back. Another pad's presses are refused. */
    void goNeedsTheArmAndItsTrigger();
    /** LB + RB + LT + RT: ALL OFF through run(), armed or not, once a
     *  chord; the arm drops, the state is Stopped (red) until armed again
     *  or the show is run from elsewhere. */
    void theEmergencyStop();
    /** State, words, colour and the pad's light agree through every
     *  change -- role, arm, a tool taking the pad, the walk, the stop --
     *  and stateChanged() goes once for each. */
    void theStateHasOneSource();
    /** In the Tools role the Run map is nothing: no run() call, and the
     *  profile's button action fires as before; in the Run role the
     *  profile's action does not fire beside it, nor View + Menu ask for
     *  Focus mode. */
    void nothingFiresInTheWrongRole();
    /** An Xbox pad and a DualSense in one controller slot, their buttons on
     *  other channels: the same presses send the same commands. */
    void xboxAndDualSenseSendTheSameCommands();
    /** The pad picks no list (0.4.5): RT / LT do nothing, armed or not;
     *  the commands go to the Run tab's list (ControlMap::runTabList(),
     *  saved with the show), else the main one; RS names it. */
    void theListIsTheRunTabs();
    /** Every button, trigger and stick, armed and not, pressed, held and
     *  let go: the role stays Run (Pad: Run / ARMED / Walk); only View +
     *  Menu held leaves it. */
    void nothingOnThePadLeavesTheRunRole();
    /** The Controller Test open: nothing pressed on the pad runs the show
     *  or a profile's action -- no arm, GO, hit, emergency stop, walk or
     *  role change -- the arm drops with a notice, and the state reads
     *  Pad: Controller Test; closed, the pad is the Run role's again. */
    void theControllerTestTakesThePad();
    /** The pad is told: the light the arm's colour while armed, green then
     *  blinking on a GO, steady again when nothing fades, its own colour
     *  disarmed, red after the emergency stop; two short buzzes armed, one
     *  long disarmed, a short one on a GO, a long one for nothing to play. */
    void theLightBarAndRumble();
    /** The controller lost while armed: disarmed, and the screen says why. */
    void aLostPadDisarms();

    /* Venue focus from the pad (rule 6, 2026-11-15). */
    /** A venue focus cue waiting, the pad not armed, ARMED, and in FOCUS:
     *  every control alone, pressed and held past every hold, starts and
     *  ends no focus, plays nothing and leaves the state as it was. In
     *  FOCUS, LT + Menu in any order leaves the call where it was, an LT
     *  tap steps back one call on its release, RT steps on as before. */
    void theVenueFocusSweep();
    /** Not armed, LT + Menu is refused, saying to arm first; ARMED, LT +
     *  Menu is FOCUS (its words, colour and light, one stateChanged()); Menu
     *  alone does nothing; LT + Menu again ends it (no command) and the
     *  pad is ARMED, the list still on the focus cue, done; LT + A is the
     *  GO that moves it on. */
    void ltMenuGoesIntoAndOutOfFocus();
    /** RT past the last call says "LT + Menu to finish" and ends nothing. */
    void rtPastTheLastCallAsks();
    /** The venue named on the laptop first: FOCUS meanwhile (LT + A plays
     *  nothing); LT + Menu gives the start up, ARMED, the cue waiting again;
     *  named, the calls run without passing through ARMED. */
    void focusWhileTheVenueIsNamed();
    /** In FOCUS the focus's own chords (LB + RB, LT, RT, LT + RT, the
     *  bumpers with one trigger) are no stop; all four are: STOPPED, and
     *  the focus ending after it does not re-arm. */
    void theStopInFocus();
    /** The laptop's Start and the laptop's end of focus (armed, not armed,
     *  and in the Tools role): no command, the list stays; armed before, the
     *  pad is ARMED after. */
    void theLaptopsFocusMovesNoList();
    /** The pad unpatched and gone in FOCUS: not FOCUS, not armed, said;
     *  back mid-focus it is FOCUS; the end leaves it not armed. */
    void aLostPadInFocus();
    /** Controller jobs: a pad without the Focus job, ARMED, LT + Menu
     *  starts no venue focus (said why); with it, FOCUS. In FOCUS another
     *  pad's LT + Menu ends nothing: without Focus it is not heard, with
     *  Focus it is not the focus's pad (Q135). */
    void theVenueFocusNeedsTheFocusJob();

    /* Walking the show (Q192). */
    /** The walking map by role, the same for both pads. */
    void walkMapByRole();
    /** Menu in the Run role starts Focus this cue on the cue in hand, with
     *  no GO on the pad; Menu again (or Focus this cue ending) stops it. */
    void menuStartsAndStopsTheWalk();
    /** RT / LT step the cues (previewed, no run() call), passing over one
     *  with nothing to walk; refused with a change not kept, and in Operate. */
    void theTriggersWalkTheCuesWithoutGo();
    /** RB / LB step the layers, said with their place; a tap of Y changes
     *  the scope, and the bumpers then step fixtures. */
    void theBumpersWalkTheLayers();
    /** View solos the walked layer (only its scene previews), following
     *  the layer and the cue; again: the whole cue. */
    void soloAndMixedOutput();
    /** A D-pad adjust kept with A is undone by View + B. */
    void anAdjustUndoes();
    /** Y held tags the cue Needs work (one tree step, undone by View + B). */
    void holdYTagsTheCue();
    /** Needs work round-trips through the show file. */
    void theTagSavesAndLoads();

    /* The walk's leftovers: level, and the light itself (Q192). */
    /** Hold LB + LT / RT moves the walked layer's level (no cue step), a
     *  tap of LB still steps back a layer; A keeps it as one tree step,
     *  undone by View + B. The same by role on an Xbox pad and a
     *  DualSense whose channels come in another order. */
    void levelFromTheTriggers_data();
    void levelFromTheTriggers();
    /** Hold LB + stick on a fixture the cue aims at a target moves that
     *  light's calibration (no cue changes); LB + A keeps it as one tree
     *  step; a second cue aimed with it plays it; View + B undoes, redo
     *  puts it back; it saves and loads with the show (no venue). */
    void theLightItselfOnItsTarget();
    /** Refused, and said: at Layer scope, on a light the cue does not aim,
     *  and in Operate while the cue is not running (quiet Refine); allowed
     *  in Operate while it runs. B puts a light not kept back. */
    void theLightItselfIsRefused();
    /** With a venue applied the light's correction goes into the venue
     *  (its sidecar), not the show's calibration, and loads from it. */
    void theLightItselfGoesIntoTheVenue();

    /* The Hits page (0.4 item 2 leftovers). */
    /** Armed, RT fires the armed hit and says its name; again lets it go. */
    void rtFiresTheArmedHit();
    /** Not armed, with no hit armed, in Design, RT only brushed or pulled
     *  with a bumper down: nothing starts; one pull is one hit. */
    void noHitWithoutArmOrAnArmedHit();
    /** Hits at several places: the narrowest armed place wins; several at
     *  one place take turns. */
    void theNarrowestArmedHitWins();
    /** The hit is named before it fires (0.4.6): hitText() says what the
     *  next pull does -- "Hit ▸ Red (1 of 2)", "Hit ▸ Red off (1 of 2)" --
     *  and is what the pull then fires, at every turn; armedHitText() only
     *  while armed; nothing in Design, with no hit armed, or with no hits;
     *  hitChanged() goes with each change. */
    void theNextHitIsNamedBeforeItFires();
    /** Hold View + Menu 1 s: Tools to Run and back, said, the Run tab's
     *  button follows. */
    void theRoleChordSwitches();
    /** A brush of View + Menu, a third button, one of the two alone: no
     *  switch; in Tools a short View + Menu is still Focus mode. */
    void theRoleChordResistsAccidents();

    /** A pad's input as a real one arrives (the plugin's valueChanged
     *  through the InputPatch, a controller slot moving its channels): the
     *  Tools role, the Run role's GO, a walk and a hit all answer, and
     *  nothing locks up. Main db08acd9a froze here: InputPatch emitted the
     *  value with its lock held, and the role lookup took it again. A
     *  watchdog fails the run instead of hanging the gate. */
    void inputThroughThePatchDoesNotDeadlock();

    /* Adaptive triggers and swipes (Q199). */
    /** The feel by state: off outside the Run role, both firm while armed,
     *  both clicking while walking. */
    void theTriggerFeelByState();
    /** Arm, disarm, walk, leave the walk, leave the role, lose the pad,
     *  quit: what the pad is told each time, and nothing twice. */
    void theTriggersFollowTheArm();
    /** One swipe is one step; a pause, a second finger and a click are no
     *  swipe; a slanted drag is none. */
    void oneSwipeIsOneStep();
    /** Walking: left / right the cues, up / down the layers, never a GO;
     *  one finger aims only with LB held. In Run: up / down pick the list,
     *  left / right walk from the next cue (Design), refused in Operate. */
    void swipesStepAndNeverGo();
    void whileWalkingTheTouchpadAimsOnlyWithLBHeld();
    void aPadWithoutATouchpadGetsNoSwipeSteps();

private:
    IOPluginStub *loadStub();
    void swipeAcross(float x0, float y0, float x1, float y1);
    /** Four spots, a list: Opening (Fronts, Backs), Blackout (no layer),
     *  Ballad (Fronts, Backs); Opening picked in the tree. */
    void makeShow();
    void press(quint32 channel, quint32 universe = 0);
    void axis(quint32 channel, uchar value, quint32 universe = 0);
    /** Aim the Fronts at a stage target in both cues (each its own Aim
     *  palette); the target's id. */
    quint32 aimFronts();
    void hold(quint32 channel, bool down, quint32 universe = 0);
    /** Arm or disarm as the operator does: LB + RB held, then let go. */
    void arm(bool on, quint32 universe = 0);
    /** @p trigger pulled, @p button pressed, the trigger let go. */
    void with(quint32 trigger, quint32 button, quint32 universe = 0);
    /** As a pad's plugin sends them, on device line @p line: the press and
     *  release (the release passes the press straight through the patch),
     *  then the frame's flush; a hold, then the flush. */
    void padPress(IOPluginStub *stub, quint32 line, quint32 channel);
    void padHold(IOPluginStub *stub, quint32 line, quint32 channel, bool down);
    /** A hit with one layer (so its look has something to play). */
    quint32 hitWithLayer(ShowTree::HitScope scope, quint32 placeId, const QString &name);
    /** A pad in the Run role, armed or not as asked; Operate. */
    PadRun *runPad(bool armed);
    /** After makeShow(): two focus calls (the fronts, the backs, to a
     *  target), Opening a venue focus cue the list is on (a probe stands
     *  in for it playing), armed, Operate; the panel's part done by
     *  finishing on the session's exitRequested(). */
    void makeFocusCue();

private:
    Doc *m_doc = nullptr;
    QList<QPair<ShowControl::Request, ShowControl::Route>> m_runs;
    bool m_answer = true;          // what the run handler says it did
    QList<quint32> m_fronts, m_backs;
    ShowTree::Target m_frontTarget, m_backTarget;
    quint32 m_list = 0, m_cue1 = 0, m_cue2 = 0, m_empty = 0, m_layer1 = 0, m_layer2 = 0;
    quint32 m_running = 0;         // the cue the probe says the list is on
};

#endif

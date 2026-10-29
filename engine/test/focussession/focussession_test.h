/*
  Q Light Controller Plus - Unit test
  focussession_test.h

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

#ifndef FOCUSSESSION_TEST_H
#define FOCUSSESSION_TEST_H

#include <QObject>

class Doc;

class FocusSession_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** Front to back, then house left to right. */
    void stepsThroughTheStageInOrder();
    /** Bumpers wrap round; as-given order is kept. */
    void nextAndPreviousWrap();
    /** Perform mode, no reference, nothing aimable: refused, and why. */
    void refusesWhatItCannotFocus();
    /** Q102: a fixed light is focused by where it lands at head height;
     *  that sets its Pan and Tilt, and revert puts it back as it was. */
    void aFixedLightIsFocusedByWhereItLands();
    /** Q103: a lens zoomed by hand is set within its range and saved; a
     *  zoom channel is driven while focusing and not saved. */
    void theBeamIsSetWithinTheLensRange();
    /** A nudge changes the live offset, the rig's calibration and the DMX,
     *  and does not mark the show modified. */
    void aNudgeMovesTheLightNotTheShow();
    /** Save, revert, zero, undo -- and the show is modified by a save. */
    void saveRevertZeroUndo();
    /** Exit with unsaved heads: saveAll keeps them, stop discards them. */
    void exitKeepsOrDiscards();
    /** unsavedChanged() fires when the set of unsaved heads changes, on a
     *  nudge, save, revert, zero, undo, saveAll and a discard -- not on a
     *  second nudge of a head already unsaved. */
    void theUnsavedHeadsAreTold();
    /** A save blinks the real light twice (dark, lit, dark, lit), stepped
     *  by advanceBlink(); a revert does not; setRigFeedback(false) stops
     *  it. */
    void aSaveBlinksTheLightTwice();
    /** The focused head is aimed (the solver's DMX) and lit; the others are
     *  dark, and writeDMX puts it in the universes. */
    void theFocusedHeadIsAimedAndLit();
    /** Y swaps to the second reference, per head. */
    /** Its light survives the frame and beats a brighter running cue (B1) */
    void itsLightHoldsOverARunningCue();
    void secondReferenceToggles();
    /** A line target gives each head its own point. */
    void aLineTargetSpreadsTheHeads();
    /** The follow spot's stick response; ticking integrates it; the
     *  programmer's joystick tick drives the session while it is on. */
    void theStickFeelsLikeTheFollowSpot();
    /** Buttons by role do what the design says; Exit only asks. */
    void buttonsByRole();
    /** Roles from a profile key or, without one, from the usual names. */
    void gamepadRolesParse();
    /** The shipped Xbox profile names a role for every control Focus mode
     *  uses, and they parse to the right controls. */
    void theXboxProfileMapsFocusMode();
    /** Through the programmer, as a controller's events arrive: roles from
     *  the device profile, View+Menu asks for Focus mode, and while it is on
     *  the buttons and sticks are its own (no chaser step, no follow spot). */
    void theControllerReachesItThroughTheProgrammer();
    /** The session keeps the app out of App Nap while it runs, and says
     *  when its controller is paused -- with the plugin's reason. */
    void theSessionKeepsTheControllerAwake();
    /** Going into Perform ends Focus mode and drops unsaved nudges. */
    void performEndsIt();
    /** What the views draw for the focused head is where it is MEANT to
     *  point -- at its reference -- through a nudge, Save, Revert, Zero, Undo
     *  and a discard; nothing for any other head, or once it stops. */
    void theViewsAreToldTheIntentNotTheNudge();

    /** Q140: the focus look takes gobo, prism and frost out, on the
     *  universe, over a cue that has them in; the cue's back after. */
    void theFocusLookTakesTheBeamOut();
    /** A fixed light's save is undone: the aim before it, then as it was. */
    void aFixedLightsSaveIsUndone();
    /** An LED head with no plain dimmer is lit white (RGB full, CMY out);
     *  a prism rotation known by its name is left alone. */
    void aColourMixingHeadIsLitWhite();
    /** Sticks left pushed when the keyboard takes over don't drift the
     *  head when the controller comes back. */
    void aStaleStickDoesNotDriftAfterTheKeyboard();
    /** The breath and flash timers have an interval: none spins the GUI
     *  thread. */
    void itsTimersNeverSpin();
    /** A layer held by one of the Doc's children lets go after ~Doc has
     *  run: it forgets the Doc rather than call it (TEST_MATRIX L7). */
    void aLayerOutlivingItsDocNeverCallsIt();

private:
    Doc *m_doc = nullptr;
};

#endif

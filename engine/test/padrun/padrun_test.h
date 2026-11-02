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
    /** A press of A without LB held sends nothing and says why; with LB
     *  held it is one GO through run() with RoutePad. B, Y and X likewise
     *  (Back, Sub GO, Stop); letting go of LB disarms. */
    void goNeedsTheArmHeld();
    /** In the Tools role the Run map is nothing: no run() call, and the
     *  profile's button action fires as before; in the Run role the
     *  profile's action does not fire beside it, nor View + Menu ask for
     *  Focus mode. */
    void nothingFiresInTheWrongRole();
    /** An Xbox pad and a DualSense in one controller slot, their buttons on
     *  other channels: the same presses send the same commands. */
    void xboxAndDualSenseSendTheSameCommands();
    /** RT / LT pick the next / previous list (wrapping), and the commands
     *  then go to its chaser; RS says the list and what is on standby. */
    void theTriggersPickTheList();
    /** The pad is told: the light bar orange while armed, blue on a GO,
     *  a short buzz on a GO and a long one when there was nothing to play. */
    void theLightBarAndRumble();
    /** The controller lost while armed: disarmed. */
    void aLostPadDisarms();

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
    /** LB + RB fires the armed hit and says its name; again lets it go. */
    void rbFiresTheArmedHit();
    /** Without LB, with no hit armed, or in Design: nothing starts. */
    void noHitWithoutArmOrAnArmedHit();
    /** Hits at several places: the narrowest armed place wins; several at
     *  one place take turns. */
    void theNarrowestArmedHitWins();
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

private:
    IOPluginStub *loadStub();
    /** Four spots, a list: Opening (Fronts, Backs), Blackout (no layer),
     *  Ballad (Fronts, Backs); Opening picked in the tree. */
    void makeShow();
    void press(quint32 channel, quint32 universe = 0);
    void axis(quint32 channel, uchar value, quint32 universe = 0);
    /** Aim the Fronts at a stage target in both cues (each its own Aim
     *  palette); the target's id. */
    quint32 aimFronts();
    void hold(quint32 channel, bool down, quint32 universe = 0);
    /** As a pad's plugin sends them, on device line @p line: the press and
     *  release (the release passes the press straight through the patch),
     *  then the frame's flush; a hold, then the flush. */
    void padPress(IOPluginStub *stub, quint32 line, quint32 channel);
    void padHold(IOPluginStub *stub, quint32 line, quint32 channel, bool down);
    /** A hit with one layer (so its look has something to play). */
    quint32 hitWithLayer(ShowTree::HitScope scope, quint32 placeId, const QString &name);
    /** A pad in the Run role, armed or not as asked; Operate. */
    PadRun *runPad(bool armed);

private:
    Doc *m_doc = nullptr;
    QList<QPair<ShowControl::Request, ShowControl::Route>> m_runs;
    bool m_answer = true;          // what the run handler says it did
    QList<quint32> m_fronts, m_backs;
    ShowTree::Target m_frontTarget, m_backTarget;
    quint32 m_list = 0, m_cue1 = 0, m_cue2 = 0, m_empty = 0, m_layer1 = 0, m_layer2 = 0;
};

#endif

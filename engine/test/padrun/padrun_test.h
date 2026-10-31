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

private:
    IOPluginStub *loadStub();
    /** Four spots, a list: Opening (Fronts, Backs), Blackout (no layer),
     *  Ballad (Fronts, Backs); Opening picked in the tree. */
    void makeShow();
    void press(quint32 channel, quint32 universe = 0);
    void hold(quint32 channel, bool down, quint32 universe = 0);

private:
    Doc *m_doc = nullptr;
    QList<QPair<ShowControl::Request, ShowControl::Route>> m_runs;
    bool m_answer = true;          // what the run handler says it did
    QList<quint32> m_fronts, m_backs;
    ShowTree::Target m_frontTarget, m_backTarget;
    quint32 m_list = 0, m_cue1 = 0, m_cue2 = 0, m_empty = 0, m_layer1 = 0, m_layer2 = 0;
};

#endif

/*
  Q Light Controller Plus
  rigroomlight_test.h

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

#ifndef RIGROOMLIGHT_TEST_H
#define RIGROOMLIGHT_TEST_H

#include <QObject>

class App;

/* The Rig Overview's 45° view in the room's light: an object's edges follow
 * the room as its faces do, so a blackout leaves dim silhouettes, not a line
 * drawing in plot or warning colours; and only fixtures really in the same
 * space are outlined in the overlap warning's red. */
class RigRoomLight_Test : public QObject
{
    Q_OBJECT

private slots:
    void cleanupTestCase();

    /** A deck's and a strip's edges in a blackout: faint, neutral, still
     *  above the canvas; brighter in House light. */
    void blackoutEdgesAreFaintAndNeutral();
    /** Strips stacked up a step's face, each touching the next, are not
     *  "in the same space"; two strips rigged in one place are. */
    void stackedStripsAreNotAnOverlap();
    /** Bars stood on end side by side along a truss are not an overlap. */
    void uprightBarsSideBySideAreNotAnOverlap();
    /** A selected fixture's edge is the selection's, whatever the room. */
    void aSelectedFixtureKeepsItsEdge();

    /** The invariant, on the pixels: a school stage rig (stage-show.qlcc,
     *  a real show stripped to its rig) in every view, Room = Blackout,
     *  nothing playing, with nothing selected and with the whole rig selected
     *  -- no pixel is a saturated colour (HSV saturation > 0.35 at value
     *  > 0.25), apart from a real clash's red and a status mark's amber
     *  (an unsaved focus change, a Show Review warning). Lists where any is. */
    void blackoutDrawsNoColour_data();
    void blackoutDrawsNoColour();
    /** And the other side of it: one fixture lit is in colour where it is. */
    void aLitFixtureIsInColourInABlackout();
    /** The whole rig selected: no name is written over another (as painted,
     *  checked against every name the view wanted to write). */
    void selectedNamesNeverOverlap();
    /** Work light, House and Dim as before: a clashing mover's edges and
     *  aim stub in the warning red at every level, a selected strip in the
     *  mint at Dim and above (not in a Blackout). */
    void aboveDimTheClashAndTheSelectionAreAsBefore();
    /** An unsaved focus change's badge keeps its amber in a Blackout, and is
     *  allowed by the invariant as a status mark. */
    void statusMarksKeepTheirColourInABlackout();

    /** Review shots of a real show (repro/show.qlcc beside the test, never
     *  committed) in Blackout and House, with a per-object list of the
     *  red-dominant colours each draws. Skipped when there is no show or no
     *  QLC_SHOT_DIR. */
    void userShowShots();

private:
    /** stage-show.qlcc loaded into m_app (once). */
    void loadStageShow();

    App *m_app = nullptr;
    bool m_stageShowLoaded = false;
};

#endif

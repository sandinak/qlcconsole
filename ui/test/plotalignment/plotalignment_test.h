/*
  Q Light Controller Plus - Test Unit
  plotalignment_test.h

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

#ifndef PLOTALIGNMENT_TEST_H
#define PLOTALIGNMENT_TEST_H

#include <QObject>

/* Does every overlay of the Lighting Studio's 2D plot (rulers, their
 * selection band and cursor mark, the centre line, the target readout)
 * land where the thing it describes is drawn? Each check maps the item's
 * own geometry through the view and compares, in viewport pixels, at
 * several zooms, quarter turns and both units. */
class PlotAlignment_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    /** The ruler strips start exactly where the viewport does. */
    void rulersRegisterWithTheViewport();
    /** Every tick sits on its grid line, labelled with the distance the
     *  readout would give there, at every zoom and turn. */
    void rulerTicksSitOnTheirGridLines_data();
    void rulerTicksSitOnTheirGridLines();
    /** The selection band spans the item's own geometry, not its paint
     *  extent (labels, halos, padding), at every zoom/turn/unit. */
    void theBandSpansTheItem_data();
    void theBandSpansTheItem();
    /** The band follows a zoom, a scroll, a turn and a selection made
     *  without the mouse. */
    void theBandFollowsTheViewWithoutTheMouse();
    /** The grid's centre line is the stage centre the readout counts from,
     *  in both units. */
    void theCentreLineIsTheReadoutsCentre_data();
    void theCentreLineIsTheReadoutsCentre();
    /** A target's readout reads the same distance the rulers do. */
    void theReadoutAgreesWithTheRuler();
    /** A quarter turn re-places the fixtures with everything else. */
    void aQuarterTurnRePlacesFixtures();
    /** Rendered, the band's columns on the ruler are the ring's columns on
     *  the canvas (pixel probe of the grabbed widget: font-free, so the
     *  same on every host). */
    void theRenderedBandCoversTheRenderedItem_data();
    void theRenderedBandCoversTheRenderedItem();

    /** Grid lines end at the grid's edge at every quarter turn, and the
     *  grid is centred on the canvas. */
    void gridLinesEndWhereTheGridDoes();
    /** The Snap setting is the step a move snaps to. */
    void snapStepIsTheSnapSetting();
    /** A dropped free fixture snaps its centre to the grid. */
    void aDroppedFixtureSnapsItsCentre();
    /** A free fixture's icon is centred on its light, in every view, and a
     *  drop in place does not move it. */
    void aFreeFixtureIsDrawnWhereItsLightIs();
    /** The name label's hit and repaint areas turn with the view. */
    void theLabelHitAreaTurnsWithIt();
    /** The selection halo stays inside the repaint area when zoomed out. */
    void theHaloStaysInsideTheRepaintArea();

    /** Structure editor: a frame member's bar ends follow its drawn body. */
    void studioBarEndsFollowTheBody();
    /** Structure editor: a fixture is picked where it is drawn, and only
     *  there, for every shape in every plane. */
    void studioHitAreaIsWhatIsDrawn_data();
    void studioHitAreaIsWhatIsDrawn();
    /** Structure editor: at 45 degrees what is in front is picked. */
    void studioAngledPickHonoursWhatIsInFront();
    /** Structure editor: the rulers paint over the rig. */
    void studioRulersPaintOverTheRig();
    /** Rig Overview: the ruler counts from the plot's zero. */
    void studioRulerCountsFromThePlotsZero();
    /** Studio group editor: a drag keeps the grab offset. */
    void studioDragKeepsTheGrabOffset();
    /** Studio group editor: grid steps are round in the show's units. */
    void studioPlaneGridStepsAreRound_data();
    void studioPlaneGridStepsAreRound();

    /** Not a gate test: ALIGN_SHOWS=a.qxw;b.qlcc ALIGN_OUT=dir measures
     *  real shows and writes a CSV and the worst cases as PNGs. */
    void measureShows();
};

#endif

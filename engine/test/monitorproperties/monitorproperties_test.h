/*
  Q Light Controller Plus - Test Unit
  monitorproperties_test.h

  Copyright (c) Massimo Callegari

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

#ifndef MONITORPROPERTIES_TEST_H
#define MONITORPROPERTIES_TEST_H

#include <QObject>

class MonitorProperties_Test final : public QObject
{
    Q_OBJECT

private slots:
    void defaults();
    void fixtureItems();
    void genericItems();
    void layers();
    void layersXmlRoundTrip();
    void groupRegistry();
    void groupsXmlRoundTrip();
    void riserMount();
    /** Mounting a fixture must give it a plot item there and then -- without
     *  one it is invisible in the rig view AND the Layers tree. */
    void mountingAFixtureGivesItAPlotItem();
    /** A riser fixture's face and the plane it is drawn on are one fact stored
     *  twice; a file carrying them in disagreement is reconciled on load. */
    void riserFaceAndDrawingPlaneCannotDisagree();
    /** ...and setting the plane carries the face with it, so the repair above
     *  never has a legitimate edit to undo. */
    void settingAFaceTakesTheRiserFaceWithIt();
    /** A deck mount has exactly one surface, the platform's top -- unlike a
     *  riser it has no second face to legitimately disagree with, so a stray
     *  default (studioMount left at 1, "front face") is always wrong and is
     *  always corrected to 0 on load. */
    void deckMountAlwaysDrawsOnTop();
    /** A truss/pipe mount has the same stray Front/face default a deck mount
     *  does, but which plane is actually correct depends on which way the
     *  thing runs -- a fixture on a HORIZONTAL truss/bar gets the same Top
     *  correction a deck fixture does; one on a VERTICAL truss/tower is
     *  already right at the default and must be left alone. Also covers the
     *  live, no-reload path: a dialog committing a fresh mount must apply
     *  the same correction immediately, not only on the next file load. */
    void trussMountPlaneMatchesWhichWayItRuns();
    void stageOrigin();
    void imagesXmlRoundTrip();
    void childBarFollowsParent();
    /** A tower's offset IS the height: on top of a 3 m tower is 3 m, whatever
     *  side the fixture was attached with. The half-width side nudge belongs
     *  to horizontal runs, where the width is vertical. */
    void aTowerOffsetIsTheHeight();
    void aLanternHangsOnItsHanger();
    void aTrussSockIsSavedWithItsColourAndStretch();
    void aTowersFacesAreSavedWithTheirSidesAndPrint();
    void studioFrameDerivation();
    /** A target kind changes where light is aimed -- a drum kit is not a
     *  vocalist, and one global subject height made them the same. */
    void targetKindsAndAimPoint();
    /** A bound target follows its structure rather than remembering where it
     *  used to be. */
    void targetFollowsWhatItIsBoundTo();
    /** Kinds persist by name, so an unknown one degrades instead of becoming
     *  whatever now sits at that index. */
    void targetXmlRoundTrip();
    void studioFrameXmlRoundTrip();
    void studioFrameSlavedToPlatform();
    void reset();

    // Lighting Studio add/remove gate coverage (2026-08-15): Truss add/remove
    // and Platform add/remove/round-trip already existed above; Pipe/Stand/
    // Tower/StageTarget had none at all.
    void stageStructureAddRemove();
    void stageStructuresXmlRoundTrip();

    /** A fixture that has only ever been placed on the plot saves nothing it
     *  was never given: no fixed zoom, and not hidden. */
    void aNewlyPlacedFixtureSavesNoJunk();

    /** Locks are per LAYER: an object locked on its own (older workspaces)
     *  moves onto a locked "Locked" layer on load, and every object's own
     *  flag then mirrors its layer's. */
    void itemLocksFoldIntoALockedLayer();
    void itemLockFlagsMirrorTheirLayer();

    /** A gobo slot of one unit fitted with other glass: kept per fixture,
     *  saved with the workspace, dropped when the fixture is deleted. */
    void goboSwapsAreKeptPerFixtureAndSaved();
    /** The operator's gobo folder: import copies (never overwriting a
     *  different picture), resolve finds a name there or in the library. */
    void theGoboLibraryImportsAndResolves();

    /** "Steps as a stage feature .. count, rise, run and width". */
    void stepsAreAFlightOfTreads();
    void stepsClimbEveryWay();
    void stepsRoundTripInXml();
    void aFixtureOnStepsStandsOnItsTread();

    /** "No way to put this fixture on the back? Also when I say side ..
     *  which side?" -- six faces, each a direction. */
    void aFixtureFacesEachOfSixWays();
    void aFaceOnATrussIsThatSideOfIt();
    void aChosenFaceSurvivesSaveAndRepair();

    /** The XL-450's LED map: heads numbered by kind, built RGB/W/RGB/W/RGB. */
    void headKindsAreReadFromTheChannels();
    void aRowPatternMapsEachKindToItsRows();
    void anLedMapLivesInTheLibraryAndTheShow();

    /** Undo and Cancel put the whole rig back from a snapshot; what they
     *  replace is retired, not freed, while the DMX thread may hold it. */
    void aSnapshotPutsTheRigBack();

    /** "that target height is 5'4 .. there should be 2'6" of truss above the
     *  person" -- a person is as tall as they are, aimed at their chest. */
    void aPersonIsAsTallAsTheirHeight();
};

#endif

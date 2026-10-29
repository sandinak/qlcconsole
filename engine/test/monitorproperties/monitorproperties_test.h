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
    /** Settings -> Units: the stage keeps its size in a new unit, and is
     *  saved in it with whole numbers older readers understand */
    void gridUnitsConvertExactly();
    void gridSizeSavedWholeAndExact();
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
    void aFixtureOnAHangerHangsStraight();
    void aTrussSpansTwoStands();
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

    /** Locks are per object AND per layer: an object's own lock is its own,
     *  saved as such, and survives its layer being locked and unlocked. */
    void anObjectsLockIsItsOwn();
    /** A show from when an object's lock only mirrored its layer: the
     *  mirrored locks are cleared and the "Locked" layer the old load made
     *  is taken apart -- its objects stay locked, go back to the layer their
     *  folder or mount is on, and the layer goes when it is empty. */
    void legacyLockedLayerIsTakenApart();

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
    void aRoundedFrontCutsTheCorners();
    void aPlatformTurnsAboutItsMiddle();
    void whatIsOnAPlatformOrTowerTurnsWithIt();
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
    /** A matrix effect reads the show's LED maps on the DMX thread while the
     *  editor sets them and a new show clears them: every read is a whole
     *  map (one of those set, or the head order), never a torn one. */
    void anLedMapReadWhileItIsSetIsWhole();

    /** Undo and Cancel put the whole rig back from a snapshot; what they
     *  replace is retired, not freed, while the DMX thread may hold it. */
    void aSnapshotPutsTheRigBack();

    /** "that target height is 5'4 .. there should be 2'6" of truss above the
     *  person" -- a person is as tall as they are, aimed at their chest. */
    void aPersonIsAsTallAsTheirHeight();

    /** "Draw Auto fixtures as their closest stage look": per show, off by
     *  default and then not written; on survives save, load and a snapshot,
     *  and says when it changes. */
    void autoStageLooksIsSavedWithTheShow();

    /** Board Q85: a sock or tower-face picture from outside the show's
     *  folder (~/Downloads) is copied into its images/ folder on save and
     *  saved relative; loading resolves it against the show's folder. */
    void patternPicturesAreKeptWithTheShow();
    /** ...the same through the show's real save (Doc::saveXML), while an
     *  undo snapshot of the rig copies nothing and keeps the original path. */
    void savingTheShowKeepsItsPatternPictures();

    /** "The beam in Focus mode": a lens set by hand is saved with the
     *  light's rig props and read back; a light never set writes no Lens. */
    void aLensSetByHandIsSavedWithTheLight();

    /** Q100: a fixed light aimed by pan and tilt points there, and keeps it
     *  through save and load; one never aimed is left as it was. */
    void aFixedLightIsAimedByPanAndTilt();

    /** Stage reference marks: off and automatic by default (and in an older
     *  show), saved with the show only when used, and back after a load. */
    void stageMarksAreSavedWithTheShow();
    /** ...and where they go: the front of the deck (and of an apron added to
     *  it), the main drape or proscenium for the curtain line, the grid's
     *  width for the number line. */
    void stageMarksFollowTheStage();
    /** Where a target is, in crew terms: SL/SR/CL from centre, US/DS from the
     *  number line (or the curtain line), rounded to what is written, in the
     *  show's unit; and the same spoken in whole units. */
    void aPositionIsWrittenAgainstTheStageMarks();
    /** Q123: the curtain line is entered as a distance upstage of the front
     *  of the stage, the number line as a distance downstage of the curtain
     *  line; both kept and saved as the plan's Y, so an older show reads as
     *  it did and a set line stays put when the deck or the other line
     *  moves. */
    void stageLinesAreSetFromTheFrontAndTheCurtainLine();
    /** Q147: the apron is its own piece: from the curtain line, as deep as
     *  set (or to the deck's front on Auto), its front curve at its front,
     *  its own deck on the Stage layer, saved with it; and what hangs
     *  downstage of the curtain line written in crew terms. */
    void theApronIsMeasuredFromTheCurtainLine();
    /** Q147 regression: a riser butted against the deck's front is never
     *  moved or resized by the apron (it was taken for the apron piece). */
    void theApronNeverMovesARiserAtTheFront();
    void hidingTheApronMovesNothingOnIt();
    /** Q147: a show saved with Show ▸ Stage marks (StageMarks, CurtainLineY,
     *  NumberLineY, TargetPositions) loads as the same stage pieces, at the
     *  same places, and saves them the new way; an older show has none. */
    void stageMarksMigrateToStagePieces();
    /** Q147: each piece is shown and locked on its own and with the Stage
     *  layer, which is made when one is shown and can't be removed. */
    void stagePiecesAreShownAndLockedOnTheirOwnAndByLayer();
    /** The number line's numbering: CL at centre, a stride that leaves
     *  room, long ticks only on numbered places, mirrored about centre. */
    void theNumberLineReadsFromCentre();
    /** Q125: target positions are shown unless turned off, saved only when
     *  off, and back after a load. */
    void targetPositionsAreSavedOnlyWhenOff();
    /** A QLC+ 4 show's <Background>, relative to the show, becomes a
     *  floor picture saved relative to it again, so the plot keeps its
     *  picture when the show's folder moves (TEST_MATRIX C4). */
    void anOldBackgroundStaysRelativeToTheShow();
};

#endif

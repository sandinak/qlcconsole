/*
  Q Light Controller Plus - Test Unit
  monitor_test.h

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

#ifndef MONITOR_TEST_H
#define MONITOR_TEST_H

#include <QObject>

class Doc;

class Monitor_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** A mover's colour ring (Branson's LED Movinghead+Circle): found in
     *  the two-head definition and the old one-head one, not on a mover
     *  without; its live segments off / solid / chasing; drawn round the
     *  head on the 2D plot and in the Rig Overview. */
    void aColourRingIsFoundAndDrawn();
    void aShowsRigViewDrawTime();
    void theRigOverviewShortcutTogglesIt();
    void layersShowAndHideFromTheRigOverviewInAnyMode();
    void theRulerMarksTheMiddleOfADraggedDrape();
    void theRigViewHidesWhatTheStudioHides();
    void aLightAtTheFootOfATowerIsNotHungOnItsTopBar();
    void theAttachBorderShowsOnlyWithShift();
    void aFixtureUnderABarIsDrawnBeneathIt();
    void aBarThatPansCanBeSetUprightAndTestsAsSuch();
    void fixturesAreStoodOnTheDeckUnderThem();
    void theNetworkIsUndoneWithTheRig();
    void aModeChangedByThePatchIsUndone();
    void hangersAreOnThePlot();
    void aPatchCanBeUndone();
    void fixturesSitAlongAHorizontalPipe();
    void theTrussEditorSpansTwoStands();
    /** The stage marks: ticked in Show > Stage marks, drawn on the plot's
     *  Top view with their labels, moved from the same menu, and on the Rig
     *  Overview's floor. */
    void stageMarksAreDrawnOnThePlotAndTheOverview();
    /** Apron depth is one rig undo step a change: two changes, two undos,
     *  each back to the depth before it; Cancel leaves none. */
    void apronDepthIsOneUndoStepAChange();
    /** Q147: the stage pieces are objects on the plot: selected by a click
     *  (never a rubber band), dragged (the lines, the apron's front), one
     *  undo step a drag, held by their lock or their layer's, hidden with
     *  their layer, with a menu and properties, and in Plot layers with
     *  their own eye and lock. */
    void stagePiecesAreObjectsOnThePlot();
    /** Q147: their positions are entered and read in the show's unit. */
    void stagePiecePositionsFollowTheShowsUnit();
    /** Q147 regression: a riser at the deck's front is never moved by the
     *  apron. */
    void theApronLeavesARiserAtTheFrontAlone();
    /** Q146: the apron is drawn as deck in the Rig Overview whatever marks
     *  are on. */
    void theApronIsDeckInTheRigOverview();
    /** Q145: the Rig Overview's controls are in a footer bar like the plot's. */
    void theRigOverviewHasAFooterBar();
    void initTestCase();
    void init();
    void cleanupTestCase();

    void addTrussAccepted();
    void addTrussCancelled();
    void addTargetAccepted();
    void addTargetEditCancelled();
    void addPlatformEditCancelled();
    void removeSelectedTruss();
    void removeSelectedCancelled();

    /* Drop round-trips: a released drag must (a) not jump at the moment of
       the drop and (b) still be where it was dropped after the item is
       re-placed from the model. Failing either is what the operator reports
       as "snaps back" / "jumps somewhere else". One test per view, plus the
       attach/detach transitions, all through the same slotFixtureMoved() the
       real mouse release invokes. */
    void dropStaysPutTopView();
    void dropStaysPutFrontView();
    void dropStaysPutSideView();
    /** "in Front and Side every free fixture of the selection keeps its
     *  drop instead of snapping back": two dragged together, both stay. */
    void twoDroppedTogetherStayPutFrontView();
    void twoDroppedTogetherStayPutSideView();
    void dropOnTrussAttachesAndStays();
    /** Several fixtures dropped together on a truss all hang on it, each
     *  where it landed along it, not only the one over it. */
    void aGroupDroppedOnATrussAllHang();
    void aDoubleClickEditsTheWholeSelection();
    /** M414: the same for two fixtures hung on a truss -- the editor says
     *  2 fixtures and its OK changes both */
    void aDoubleClickOnTwoHungOnATrussEditsBoth();
    /** ...and the same in Plot layers: a double-click on one of two selected
     *  fixture rows opens Fixture Properties for both, and OK sets both. */
    void aLayersDoubleClickEditsTheWholeSelection();
    /** The truss editor's fixture list: a double-click on one of several
     *  selected rows edits them all. */
    void aTrussEditorsDoubleClickEditsItsSelectedRows();
    /** Q100: a fixed light's editor aims it by Pan and Tilt. */
    void aFixedLightsEditorAimsItByPanAndTilt();
    /** Q103: a light whose lens is zoomed by hand (a lens range, no zoom
     *  channel) has "Beam (lens set to)" in Fixture Properties; setting it
     *  sets the light's lens, which is what its beam is drawn at. */
    void aLensSetInFixturePropertiesIsTheLights();
    void theStageStaysCentredWithFixturesOffIt();
    /** "can't c/p stage features": scenery, towers, booms and stands copy
     *  and paste, one undo step. */
    void stageFeaturesCopyAndPaste();
    void dropOffTrussDetachesAndStays();
    /** Bound, dragged a little off the bar (inside the two-widths zone):
     *  stays bound, keeps the sideways offset, and stays where dropped. */
    void nudgeOffTrussKeepsBindingAndOffset();
    /** The hysteresis band and the locked-truss rule: near-but-free stays
     *  free, and a locked truss never acquires a dropped fixture. */
    void nearTrussStaysFreeAndLockedTrussRefuses();

    /* Lighting Studio Editor (StructureStudioView) — a different widget from
       the plot's MonitorGraphicsView, with its own drag path. The operator
       report is "cannot move this fixture on this truss even though it's
       bound to the truss", so these drive the real press/move/release on a
       VERTICAL truss (the T-2 case) in every plane. */
    void studioTrussDragMovesFixture();
    /** "when selecting a feature on the bar.truss editor .. should show the
     *  fixture width on the horizontal .. and the fixture height on the
     *  vertical like it does on the studio editor" -- drawDimensions() was
     *  written for the structure only and never fed a selected fixture at
     *  all (nor even called from paintEvent()). */
    void selectingAFixtureShowsItsOwnDimensionCallouts();
    void studioTrussDragBlockedWhenLocked();
    void studioTrussDragBlockedWhenTrussLocked();
    void studioTrussDragInEveryPlane();
    /** Every plane moves a truss-mounted fixture in SOME direction -- including
     *  the top view of a vertical run, where there is no axis to slide along
     *  and only the across-the-truss freedom exists. */
    void studioTrussDragAcrossTheRun();
    /** The same drag on a HORIZONTAL truss, and with a fixture whose id is not
     *  0 -- so a pass here isn't an artefact of the id-0 rig. */
    void studioHorizontalTrussDragMovesFixture();
    /** "handles to allow for rotation via mouse movement": the selected
     *  fixture's knob turns its run (Angle) facing its mount face... */
    void aFixtureOnATrussTurnsByItsAngleHandle();
    /** ...and rolls an LED bar (Tilt) looking down its length. */
    void anLedBarOnATrussRollsByItsTiltHandle();
    /** "where do I grab that to rotate it .. I see no handles": a bar laid
     *  on its Top face, seen from the Side, still turns. */
    void aBarTurnsInAViewOfAnotherFace();
    /** "clean ways to assign fixtures into universes": the structure
     *  editor's inspector moves the selected fixture's universe/address. */
    void theInspectorMovesAFixturesDmxAddress();
    /** "if I select all the fixtures in the left hand .. apply tilt/angle
     *  changes to all of them at same time .. auto distribute and
     *  multi-select to move them around". */
    void theInspectorEditsEveryFixtureSelected();
    void cmdClickBuildsASelectionThatDragsTogether();
    void distributingAFewSpacesThemBetweenTheOutermost();
    /** "I can't right click on a FART to move to a new layer .. it unselects
     *  after I select it": a right-press keeps the selection it lands in. */
    void aRightClickKeepsTheSelectionItLandsIn();
    /** "I still can't select things in the side layers menu .. they select
     *  and deselect immediately .. T1-W": a row whose plot item cannot be
     *  selected (stacked out of sight in a tower) stays selected. */
    void aLayersRowStaysSelectedWhenThePlotCannotFollow();
    /** "remove from truss" for a fixture group listed under one it is not on. */
    void takingFixturesOutOfAStructure();
    /** "create fixture group from those groups that uses the relative layout
     *  sizing from the studio". */
    void aGroupOfGroupsIsLaidOutAsOnStage();
    void aGroupOfGroupsIsLaidOutFromTheChosenView();
    void aGroupIsLaidOutAgainFromTheStage();
    void enterRenamesAnyLayersRowFromItsBareName();
    void stepFrontsAndTopsAreLaidOutAsOnStage();
    void aClearTopGlowsWithTheLightInsideIt();
    void twoBarsOfOneKindEachSplitIntoAGroupPerKind();
    void editFixtureFromLayersEditsThatFixture();
    void aBarLitInSectionsLightsOnlyAboveThem();
    void aFixturesLightFollowsWhatItsDefinitionPutsOut();
    void stepBarsTippedUpstageWashTheTopOfTheCyc();
    void aPixelBarsLightComesFromItsLitPixels();
    void aBarHungOnATrussShinesOutOfItsFace();
    void aLanternHangsOffATowerAndGlowsAllRound();
    void aWledLanternShowsItsEffectLedByLed();
    void aSockedTrussGlowsWithTheBarsInsideIt();
    void aTowerWearsItsFacesPlainOrPrinted();
    void ledsGlowInTheHaze();
    void hazeGlowStaysBehindAClosedDrape();
    void aBigPixelRigPaintsInGoodTime();
    void aWashStopsOnARiserSeenFromTheSide();
    void aWashStopsOnARiserSeenFromTheFront();
    void aBeamClearOfTheRisersIsLeftAlone();
    /** Consolidation 3: double-click a fixture = Fixture Properties; click
     *  again picks one out of a group. */
    void doubleClickingAFixtureOpensItWhereverItIs();
    void clickingAGroupMemberAgainPicksItOut();
    /** "Undo: as long as it works across most things so that users can
     *  reverse mistakes" -- a rig dialog previews live, Cancel puts
     *  everything back, OK is one step on the plot's undo. */
    void aRigDialogPreviewsCancelsAndUndoes();
    void fixturePropertiesCancelAndUndoToo();
    /** One fixture property model: the inspector and Fixture Properties
     *  share their rows, show "mixed" where the selection differs and set
     *  only what is set; the inspector's edits are Ctrl+Z steps. */
    void theInspectorShowsMixedAndSetsOnlyWhatIsSet();
    void anInspectorEditIsOneUndoStep();
    void fixturePropertiesForSeveralIsMixedAndLive();
    void aMaxDrawIsSetOnEverySelectedFixture();
    void measuringPowerDrivesEachStateAndKeepsTheReadings();
    void powerReadingsAreCopiedToEveryFixtureLikeIt();
    void changingAFixtureKeepsWhatIsSelected();
    void aPersonsHeightIsSetInTheTargetDialog();
    /** "can we have the crosshairs hover give H, V, and Z" -- the point under
     *  the pointer, exact in every view, from the depth buffer. */
    void theCrosshairsSayWhereThePointerIs();
    /** "for followspot .. iris/aperture/zoom .. if we have a wider field the
     *  center should move down so that it covers more of the target w/o too
     *  much top overwash". */
    void aWiderFollowSpotAimsLowerOnThePerson();
    /** "moving head washes .. 36x18w .. I need to be able to set that":
     *  LEDs on the lens face, drawn there in the beam's colour. */
    void aWashShowsItsLedsOnItsFace();
    /** "how are we handling laser projectors .. one that just does color
     *  stars": a star field of points, a laser's pencil beams -- not a wash. */
    void lasersThrowStarsAndPencilBeams();
    void hangersAreRiggingCopiedToEveryTower();
    void theSplitWizardOffersAViewAndKeepsAnExistingGroup();
    /** "I can't click on any side arrow to open any of the groups": an
     *  opened fixture group stays open through a rebuild of the list; and a
     *  duplicate group ("UST" beside "USTs") can be deleted from its row. */
    void aFixtureGroupRowStaysOpenAndCanBeDeleted();
    /** "No way to put this fixture on the back? .. which side?": the
     *  inspector's Face names six directions, and Back faces upstage. */
    void theInspectorPutsFixturesOnTheBack();
    /** "the bars on the farts render funny in the studio view but look right
     *  in the rig view": the plan view draws a bar by the way it runs. */
    void aBarHangingStraightDownIsADotInPlan();
    /** "fixtures like XL-450 that have two distinct types of heads .. auto-
     *  generate fixture groups based on the type". */
    void headKindGroupsFollowTheLedMapAndTheRig();
    void closedUpBarsKeepTheirOrderAcrossDistantTrusses();
    void theStageGroupDialogNamesViewsAndSpotsDuplicates();
    void selectAllSimilarFindsLikeGroupsAndFixtures();
    void theSplitWizardMapsRowsAndMakesAGroupPerKind();
    void theRigViewDrawsHeadsWhereTheLedMapSays();
    /** "it puts the fixture in that group but not the other groups" / where
     *  does a group across several trusses go: Layers lists a fixture under
     *  every group it is in, and gives a spanning group a home. */
    void layersListsEveryGroupAFixtureIsIn();
    /** "no way to remove fixture from tower": the editor's list menu does it. */
    void fixturesAreRemovedFromATowerFromItsList();
    /** "can we patch more than one fixture .. selected 4 and got 1": from the
     *  Layers list, Patch takes the LIST's selection. */
    void patchingFromLayersTakesEveryFixtureSelectedThere();
    /** A wireless transmitter's receivers appear on the plot by themselves,
     *  and are dragged to where the box is rigged. */
    void wirelessReceiversArePlacedAndDragged();
    /** "I can't move USTs fixture group to a different layer .. drag/drop
     *  or right click move to layer": the group's row moves its fixtures. */
    void aFixtureGroupRowMovesToAnotherLayer();
    /** Vertical drag in an elevation on a horizontal run = the drop length
     *  (mountZOffset), so a bar-hung fixture can be raised/lowered there. */
    void studioTrussDragSetsDropInElevation();
    /** Scroll-wheel and native trackpad pinch zoom share one anchor-at-cursor
     *  math (zoomAt()) -- both directions, and the clamp at each end. */
    void studioZoomAtKeepsCursorPointFixed();
    /** A fixture that is NOT on the 2D plot must not be able to poison
     *  MonitorGraphicsView::m_fixtures with a null entry -- QHash::operator[]
     *  INSERTS on a miss, and updateFixture()'s contains() guard then waves the
     *  null straight through to item->setSize(). Observed as a SIGSEGV on
     *  releasing a studio-editor drag. */
    void updateFixtureSurvivesUnplacedFixture();
    /** End to end through the REAL "Lighting Studio Editor" dialog: open it on
     *  a truss, unlock, drag a mounted fixture, release. This is the exact
     *  chain that crashed -- mouseReleaseEvent -> fixtureMoved ->
     *  Monitor::updateFixture -> MonitorFixtureItem::setSize. */
    void studioEditorDialogDragDoesNotCrash();
    /** stage-structures-demo.qxw's fixture 7 (XL-450): rigged on truss 2 with
     *  NO FxItem. It drew at the world origin instead of on its truss, was
     *  absent from the Layers tree, and dragging it changed the rig props while
     *  the reported position stayed (0,0,0) -- so it never moved. */
    void mountedFixtureWithoutPlotItemIsPositionedAndMovable();
    /** A definition that DECLARES its pixel layout (the XL-450 says 15 x 5)
     *  must be drawn in that arrangement, not in one re-derived from the head
     *  count and the aspect ratio. */
    void declaredHeadLayoutIsUsed();
    /** A VERTICAL run's axis is Z, so both horizontals are free around it. The
     *  side view's horizontal screen axis is Y, which had no field to move --
     *  lateral drags there did nothing. */
    void verticalTrussMovesLaterallyInSideView();
    /** Every view must box a fixture by the two of its W/H/D that the view
     *  actually sees: Top = W x D, Front = W x H, Side = D x H. The side view
     *  of a front-facing panel used to collapse to a dot because both ends of
     *  its long axis landed on the same pixel. */
    void fixtureIsBoxedByItsRealDimensionsInEveryView();
    /** The structural mounts are mutually exclusive. Attaching to a truss must
     *  clear a previous deck/riser/pipe/tower mount, and load must collapse any
     *  file that already carries two -- otherwise the position resolver and the
     *  plot's double-click each pick a different structure. */
    void structuralMountsAreExclusive();
    /** The "On Rig" toggle hides only the fixtures mounted on a structure, so
     *  the structure under them can be clicked; free-standing ones stay. */
    void mountedFixturesToggleHidesOnlyMountedOnes();
    /** The view rotation is a VIEW transform: a fixture's stored position must
     *  not move, the drag must still land where the cursor is, and the edge
     *  labels must turn with the view. */
    void viewRotationIsScreenOnlyAndDragStillWorks();
    /** The PLOT's rotation: a view transform only. The grid must still fit (the
     *  turn swaps which viewport extent the grid is laid out along -- the same
     *  fit code behind the "opens zoomed" regression), the zoom must survive it,
     *  and stored positions must not move. */
    void plotViewRotationFitsGridAndKeepsPositions();
    /** The angled view: its projection must agree with the flat views at the
     *  angles where they coincide, and it must REFUSE drags (there is no honest
     *  inverse for an axonometric). */
    void angledViewProjectsAndRefusesEdits();
    /** StageKind draws the WHOLE rig: every structure and every placed
     *  fixture, framed by the fit, and read-only. */
    void stageOverviewDrawsEveryStructure();
    /** In the angled view a fixture must keep its own orientation as the
     *  camera orbits, not swing round to face it. */
    void angledFixturesAreNotBillboards();
    /** Painter's algorithm across the whole rig: an object nearer the eye must
     *  cover one behind it, whatever order they sit in the workspace. */
    void angledOverviewDrawsNearThingsInFront();
    /** A mover set upright on a tower shelf must not be drawn hanging in the
     *  rig view -- the elevation and the 3D view have to agree about which way
     *  up a fixture is. */
    void aMoverStandingOnAShelfIsNotDrawnHanging();
    /** ...and it stands ON the shelf rather than sunk halfway through it. */
    void aMoverOnAShelfSitsOnItRatherThanInIt();
    /** ...and one standing on a DECK is not buried in it by a mountingType
     *  that defaults to "hung" when nothing set it. */
    void aMoverOnADeckIsNotBuriedInIt();
    /** "the truss editor shows the fixtures just under the cross member .. but
     *  the rig view shows them down a bit": a mover hung on a truss is drawn
     *  at the same height in the elevation and in the 3D rig view. */
    void aMoverHungOnATrussIsAtTheSameHeightInEveryView();
    /** "fixture set as underhung .. visually upright": a mover mounted on a
     *  horizontal TRUSS/bar (not a tower shelf) must also flip in its own
     *  Front/Side elevation -- the generic (non-tower) mover branch used to
     *  hardcode "not hung" regardless of mountingType. */
    void aMoverOnATrussElevationIsNotDrawnUpright();
    /** "when selecing the bar .. should just show the bar on the rulers":
     *  a parent truss's own editor's rulers always measured the PARENT,
     *  even with a bar selected on the canvas -- switch to the selected
     *  bar's own extent, back to the parent's when nothing is selected. */
    void rulersFollowWhicheverOfTrussOrBarIsSelected();
    /** The plan and the rig must agree about WHERE a fixture is. */
    void planAndRigAgreeOnAFixturesPosition();
    /** A fixture placed outside the grid must stay reachable in the plan --
     *  otherwise it cannot be selected or dragged back without editing XML. */
    void theStudioPlanCanReachAFixturePlacedOffTheGrid();
    /** ...and the plan says so, and can select them for you. */
    void theFooterCountsAndSelectsOffStageFixtures();
    /** A Layers row that reads "on truss" opens the TRUSS -- including when
     *  its auto-group carries a studio frame, which is the case that broke. */
    void aTrussRowInLayersOpensTheTrussNotItsGroup();
    void theLayersListOffersEverythingThePlotDoes();
    /** "if I add a bar to a truss .. that becomes it's own container for
     *  lights like the truss does": a bar gets its own row with its own
     *  fixtures nested under it, and the truss's group stays anchored (not
     *  a bare "frame" folder) even though the bar shares its group. */
    void aBarBecomesItsOwnContainerInTheLayersList();
    /** Integration test for the whole build-out flow: add a fixture, add a
     *  truss (the real "Add Truss" dialog), add a bar to it (the real
     *  context-menu action's slot), drop the fixture onto the bar, drag the
     *  bar on its own editor, then drag the fixture back onto the truss --
     *  each step through the same production code path a user's mouse would
     *  hit, checking the model and the Layers list agree after every one. */
    void buildingARigStepByStepThroughRealUiStaysConsistent();

    /** The Studio's Inventory panel lists staged fixtures that are not on the plot yet. */
    void inventoryPanelListsOnlyStagedFixturesNotOnThePlot();
    /** Dragging a staged fixture onto the plot places it where it was dropped. */
    void droppingAnInventoryFixtureOnThePlotPlacesIt();
    /** ...and dropping it onto a truss mounts it there, as a dragged fixture would. */
    void droppingAnInventoryFixtureOnATrussMountsIt();

    /** A staged fixture on the plot says "unpatched" in the DMX and network
     *  overlays and never contributes a universe or an address clash. */
    void aStagedFixtureOnThePlotNeverClaimsAUniverse();
    /** Locate and reset drive real DMX, so they must refuse a staged fixture. */
    void locateAndResetLeaveAStagedFixtureAlone();

    /** A wireless port's receiver is drawn where the receiver is, with a dashed
     *  link back to its transmitter that follows the transmitter when dragged. */
    void wirelessReceiversAreDrawnWithALinkToTheirTransmitter();
    /** A wired node has no receivers to draw. */
    void aWiredNodeDrawsNoReceiverMarkers();

    /** The Network overlay draws each port's cable run through its fixtures in
     *  address order, with the run's stats on hover, and only in that overlay. */
    void theNetworkOverlayDrawsCableRunsInAddressOrder();
    /** A run follows a fixture that moves. */
    void aCableRunFollowsAMovedFixture();
    void aWirelessRunStartsAtItsReceiverAndCountsWhatIsOffThePlot();
    void networkNodesJoinLayersGroupsLocksAndRenames();
    void networkNodesMoveTogetherAndSnap();
    void doubleClickingANetworkNodeOpensIt();
    void onlyInventoryDragsAreTakenByThePlot();

    /** In the Network overlay a DMX fixture on a universe no port carries is flagged. */
    void theNetworkOverlayFlagsFixturesNoPortServes();

    /** The panel files staged fixtures manufacturer -> model -> fixture, and
     *  dragging a folder carries every fixture beneath it. */
    void inventoryPanelIsOrganisedByManufacturerAndModel();
    void inventoryPanelKeepsFoldersCollapsedAcrossAReload();
    void inventoryPanelCountsNetworkFixtures();
    void aDragFromTheInventoryCarriesEverySelectedFixture();
    void theInventoryButtonShowsAndHidesThePanel();
    void addToInventoryFromTheStudioShowsTheNewFixture();
    void aDropOnALockedPlotIsRefused();
    void addNetworkNodePlacesItWhereTheMenuWasOpened();
    void addNetworkNodeCancelledAddsNothing();
    /** "network gear": a node placed from a catalog model arrives with its
     *  transport and ports; a hand-built one can be saved as a model. */
    void aNetworkNodeCanBePlacedFromAGearModel();
    void deletingANetworkNodeAsksFirst();
    void editingANetworkNodeRebuildsThePlot();
    void theNetworkReportOpensFromTheStudio();
    void aFixtureOnATowerIsListedUnderThatTower();
    /** "pipes/booms still list their fixtures loose in Layers": a fixture on
     *  a pipe or boom is listed under it, as on a tower. */
    void aFixtureOnAPipeOrBoomIsListedUnderIt();
    /** "when I edit a fixture from studio I should be able to change its
     *  name" / "when I rename it should update visually everywhere". */
    void aFixtureIsRenamedFromItsEditDialogAndEverywhere();
    /** "when I select a fixture in the main studio view .. should also select
     *  it in the side layers". */
    void selectingOnThePlotSelectsInLayers();
    /** "in truss view we have text stomping on each other .. shelf info on
     *  left and light info on right": a tower's shelf labels sit left of it,
     *  and the height dimension moves out past them. */
    void towerShelfLabelsAreOnTheLeft();
    /** "i can't add cross bar/truss on T2 .. that option is missing": the
     *  truss editor's right-click menu adds a bar where you clicked. */
    void aBarIsAddedWhereTheTrussEditorWasRightClicked();
    /** ...from the right-click menu itself, over a fixture as well as on
     *  empty canvas. */
    void theTrussEditorsMenusOfferAddBar();
    void draggingATargetAnnouncesTheMove();
    void aMoverOnATopFaceStandsUpOrHangsDown();
    void aFixtureCanBeRaisedOntoATowersTopCap();
    void fixturesAndBarsAreRenamedInTheStructureEditorsList();
    void addCycloramaSpansTheBackOfTheStage();
    void theCycloramaEditorPreviewsAndCancelReverts();
    void aCycloramaOnThePlotMovesLocksHidesAndDeletes();
    void everySceneryKindIsAddedWhereItUsuallyGoes();
    void aDrapeOverTheStepsIsWhatADoubleClickOpens();
    void anOpenMainDrapeShowsTheCycBehindIt();
    void aUWallIsDrawnRoundItsFrontWithItsTop();
    void aRoundedStageFrontIsDrawnAndLitRoundItsCurve();
    void aBeamPastTheEndOfTheCycStillWashesIt();
    void rotatingASelectionTurnsItAboutItsMiddle();
    void aTurnedDeckIsDrawnAndLitWhereItIs();
    void aSilkFlameRisesOnlyWhenItsRelayIsOn();
    void aDrapeDrivenByACurtainFixtureOpensAsItsChannelMoves();
    void cancellingTheDrapeDialogTakesItsNewCurtainAway();
    /** Q51: a dimmer marked Curtain (Plot Symbol ▸ Scenery ▸ Curtain) is
     *  hidden on the plot (its drape stands for it) and out of the
     *  inventory, listed as a drape's driver, opened by its intensity;
     *  one ⌘Z puts it back */
    void aFixtureMarkedCurtainIsDrawnAsItsDrape();
    void addFountainBuildsItAsOneGroup();
    void beamsLandOnTheCycloramaInTheRigView();
    /** "set a gobo and projected on the cyc .. and it's not visible" /
     *  "handle prisms and rotation". What the gobo wheels, their rotation and
     *  the prism make of a beam (BeamOptics), from the channel values. */
    void beamOpticsReadGobosRotationAndPrism();
    void prismAndRotationNamesAreRead();
    /** The pool is drawn through the mask: none of it where the gobo is
     *  metal, all of it where it is clear, and the haze dimmed to match. */
    void aPoolsEdgeFollowsTheFixturesOptics();
    void aWashPoolFadesPastItsBeamAngle();
    void keptLandingsFollowTheDecks();
    void aPoolOnADeckIsDrawn();
    void theGpuDrawsWhatTheCpuDraws();
    /** Coloured light on a grey surface reads as its colour ("blue on the
     *  cyc looks pale"): white light is plain Screen, a saturated surface
     *  keeps its own colour. */
    void colouredLightTakesOverAGreySurface();
    /** A fixture standing in for a missing definition: one small body,
     *  no overlap warning (not a 19 m wall of heads in red). */
    void aStandInForAMissingDefinitionIsOneSmallBody();
    void aGoboPoolIsDrawnThroughItsMask();
    /** "specialized gobos": a slot of ONE unit fitted with the operator's
     *  own picture (Gobo Wheels…), saved per fixture, drawn on the rig. */
    void goboWheelsFitsYourOwnGlassInOneUnit();
    void selectAllTakesEverythingSelectable();
    void aDecksCountBubbleMovesWithIt();
    /** "when I zoom in, the blue circles can stay the same size so what's
     *  under them is more easily clickable." Same screen size at any zoom,
     *  still centred on its structure. */
    void aCountBubbleKeepsItsScreenSizeWhenZoomed();
    void oneUndoPutsBackAWholeMixedDrag();
    /** Two locks: an object's own (held, still selectable) and its layer's
     *  (everything on it, unselectable); the menu offers both, a power
     *  source's toggle is its own, and every locked thing -- fixtures on a
     *  locked layer included -- draws red and won't drag. */
    void anObjectAndItsLayerEachHaveALock();
    void aStagedFixturesTestDialogOffersNoDmxActions();
    void aStagedFixtureOnThePlotCannotBeTestedOrLocated();
    void theDmxViewHasNoTileForAStagedFixture();
    /** Integration test for the target/aim workflow: add a fixture, add a
     *  target (the real "Add Target" -> "Edit Target" dialog chain), assign
     *  the fixture to aim at it, verify the resolved beam actually arrives
     *  at the target (not just that a reference was stored), move the
     *  target through its real edit dialog and confirm the beam follows,
     *  then move it out of reach and confirm the "cannot reach" warning
     *  appears -- and clears once the fixture is re-rigged within reach. */
    void aimingAFixtureAtATargetFollowsItThroughARealSession();
    /** Integration test for the platform/riser workflow: add a platform (the
     *  real "Lighting Studio Editor -- Riser" dialog), deck-mount one
     *  fixture (the plot's real auto-deck drop) and riser-mount another
     *  (the real Structure Studio drop path), drag the riser fixture on its
     *  own editor, then move the platform and confirm BOTH fixtures land
     *  exactly where the geometry says they should -- via two genuinely
     *  different mechanisms (deck is carried explicitly, riser is derived
     *  live), which is itself one of the subtleties this work surfaced. */
    void buildingAPlatformStepByStepThroughRealUiStaysConsistent();
    /** Integration test for the stand/boom/crossbar workflow: add a stand
     *  (its real edit dialog), add a boom then a crossbar via the Stand
     *  editor's own "Add Bar" action, mount a fixture on the boom (the real
     *  context-menu action's target function), drag it on the Stand's own
     *  editor, adjust the crossbar through ITS real edit dialog (a
     *  crossbar cannot be dragged at all -- see the subtleties list), then
     *  relocate the fixture with a second real drag. */
    void buildingAStandStepByStepThroughRealUiStaysConsistent();
    void doubleClickingAnywhereOnATowerTopOpensIt();
    void doubleClickingABubbleShowsWhatItHides();
    /** Stacked fixtures get one named tag each, and a tag picks its own. */
    void aRevealedFixtureCanBePickedOnItsOwn();
    /** A footprint names the head that casts it, and can be asked for from
     *  the menu without selecting anything. */
    void aLightFootprintSaysWhoseItIs();
    /** "Add Person here" stands a Person target at the click. */
    void addingAPersonStandsOneWhereYouClicked();
    /** Aim a head at a mark, feed it its own DMX, and the beam it draws
     *  arrives at that mark -- hung or standing, inverted or not. */
    void aHeadAimedAtAMarkDrawsItsBeamThere();
    /** The plot's pool from live DMX of a head with calibration offsets
     *  lands on the mark it is aimed at, not the offset beside it. */
    void aCalibratedHeadsPoolLandsOnItsMark();
    /** Focus mode's setup offers the selected scene's aim first, and the
     *  selected heads. */
    void focusModeSetupOffersTheSceneAimFirst();
    /** Focus mode end to end from the keyboard: nudge, save, next, revert,
     *  step size, zero, Esc asks about unsaved, the plot ring, one undo. */
    void focusModeRunsFromTheKeyboard();
    /** Focus mode: through a big unsaved nudge, Save, Revert and a discard,
     *  the focused head's beam, footprint and Studio aim stay on its
     *  reference -- what it is meant to do, not its nudged, rounded DMX. */
    void focusDrawsTheIntentNotTheNudge();
    /** Focus mode: the head being focused is lit and ringed in the Rig
     *  Overview, opened before or during the session, on the CPU and GPU. */
    void focusLightsAndRingsTheHeadInTheRigOverview();
    /** A head standing on a truss top cannot point at its own feet. */
    void anUprightHeadCannotPointBesideItself();
    /** A par or bar throws light as well -- beams were movers-only. */
    void aWashOnATrussThrowsLightToo();
    /** A riser's menu offers to put a fixture, a target or a person on it. */
    void aPlatformOffersToPutSomethingOnIt();
    /** Unticking "Light footprint" takes every footprint away. */
    void theFootprintToggleTurnsThemOff();
    /** A footprint is drawn where the SCENE aims the head, not where its
     *  resting DMX happens to point. */
    void aFootprintFollowsTheSceneAim();
    /** Selecting a head shows every part of the deck it can put light on --
     *  including the hole under a head standing on a truss top. */
    void aSelectedHeadShowsEverywhereItCanHit();
    /** The same head hung covers the deck beneath it. */
    void aHungHeadCoversWhatIsUnderIt();
    /** A head that can genuinely light nothing says why, with numbers. */
    void aHeadThatCanLightNothingSaysWhy();
    /** Lay a step out once, save it as a component, stamp it onto the next
     *  step's fixtures: the arrangement must arrive intact. */
    void aSavedComponentStampsItsLayoutOntoOtherFixtures();
    /** "Fixtures on the rig" unchecked must STAY unchecked across a refresh. */
    void hidingMountedFixturesSurvivesARefresh();
    /** Fixtures that bury their structure in THIS view are replaced by a count;
     *  in a view where they do not bury it, they stay. */
    void fixturesThatBuryTheirStructureBecomeACount();
    void aFixtureOverATowerBecomesItsCount();
    void endOnTrussIsNotBuriedByItsOwnFixtures();
    void anEditedStructureRecountsItsFixtures();
    void aFixtureMountedFromAnEditorLandsOnThePlot();
    /** Fixtures stacked on one spot but covering little must STAY visible --
     *  a step's face tape is the case that must not be hidden. */
    void stackedFixturesOnAStepBecomeACount();
    /** studioTilt rolls a fixture about its own length, so a bar in a step can
     *  fire up toward the cast instead of straight up. */
    void studioTiltRollsAFixtureAboutItsLength();
    /** ...and the flat views DRAW it rolled, rather than as an upright box. */
    void aTiltedBarIsDrawnTiltedInTheSideView();
    /** A bar hung on a tower must actually be drawn in the rig view. */
    void aBarOnATowerIsDrawnInTheRigView();
    /** ...and specifically in ITS OWN PARENT's per-object editor too -- a
     *  separate code path from the whole-rig StageKind view above, which
     *  proves nothing about this one. */
    void aBarShowsInItsParentTrusssOwnEditor();
    /** A bar added to a HORIZONTAL truss through the real "Add Bar" action
     *  (Monitor::createBarOnTruss()) must default somewhere a click can
     *  actually tell apart from the truss itself -- its old FaceBottom
     *  default offset the bar along Z only, invisible in Top view, so the
     *  bar's drawn line landed exactly on the truss's own line. */
    void aBarOnAHorizontalTrussDefaultsClearOfItInTopView();
    /** Visible is not the same as clickable -- hit-test and drag it too. */
    void hitTestBarFindsANearbyBar();
    void clickingABarSelectsAndHighlightsIt();
    void doubleClickingABarActivatesIt();
    void draggingABarChangesItsPositionAlongTheParent();
    /** A bar rigged INSIDE a tower is still drawn in the rig view. */
    void aFixtureInsideATrussIsStillDrawn();
    /** A clear-topped step must let what is inside it show through, and a
     *  fixture placed Inside must not be seated out onto the surface. */
    void clearTopAndInsidePlacement();
    /** A fixture with no structural mount and no frame group -- an LED bar
     *  simply laid on a step -- must still be draggable in the editor that
     *  lists it. */
    void freePlacedFixtureIsDraggable();
    /** Dragging the background pans the view, and a later resize must not
     *  silently recentre what was moved by hand. */
    void draggingBackgroundPansAndSurvivesResize();
    /** Placement=Inside must put a platform-mounted fixture IN the box, not on
     *  top of it -- the flag has to move the fixture, not just how it is drawn. */
    void insidePlacementPutsFixtureInThePlatform();
    /** ...and its height within the step must be settable by dragging in an
     *  elevation, or Inside just pins it to the floor. */
    void insideFixtureHeightIsDraggableInElevation();
    /** A DECK-mounted fixture had no drag branch at all: the horizontal wrote
     *  the stored position while its Z stayed derived, so it could never be
     *  moved vertically inside (or above) the step. */
    void deckMountedFixtureMovesVerticallyToo();
    /** A studio-frame-group fixture set to Inside must be positionable within
     *  the volume: the face PIN is what welded it to the surface. */
    void insideFrameGroupFixtureIsNotPinnedToTheFace();
    /** EVERY mount kind dragFixtureTo() dispatches on must actually move a
     *  fixture. Five separate "I can't move this" reports were each a branch
     *  that did not exist or silently declined; this walks the whole chain so a
     *  sixth fails here instead of arriving as a bug report. */
    void everyMountKindCanBeDragged();
    /** Sub-pixel LED grids are culled, so a wide overview draws far fewer
     *  primitives than a zoomed-in one. This is what lets the live repaint keep
     *  up: unculled, a 96-strip rig was 7800 primitives and 59 ms a frame. */
    void subPixelLedGridsAreCulledWhenZoomedOut();
    /** Deleting a fixture group must leave its fixtures in the workspace,
     *  patched exactly as they were. */
    void deletingAGroupKeepsItsFixtures();
    /** A mover's aim vector, from the pan/tilt it is being driven with. The
     *  conventions are easy to get backwards and invisible until someone looks
     *  at a rig, so pin each one. */
    void moverAimFollowsPanAndTilt();
    void aRunningLookIsDrawnWhereItsHeadsAreSent();
    void theAngledViewStandsOnCentreStage();
    /** Room light applies whether or not live output is shown, and takes the
     *  whole picture down toward a blackout. */
    void ambientLevelDimsTheWholeView();
    /** Room light is for the room. A lamp that is lit reads at its own
     *  brightness even in a blackout -- that is the whole point of a blackout
     *  shot. Movers once went black here because the ambient multiplier was
     *  applied to emitters too. */
    void litFixturesKeepTheirBrightnessInABlackout();
    /** A pixel bar shows its actual pattern, not one averaged colour: a step
     *  front running red-and-blue drew as a pale wash because every head was
     *  reduced to the fixture-wide per-primary maximum. */
    /** A fixture at zero is an object in the room, not a black hole: the
     *  room lights its body even while its lamp is off. */
    void unlitFixturesAreStillObjectsInTheRoom();
    /** The housing of a pixel bar is a box, not a lamp: a strip with most of
     *  its pixels dark must not draw a bright line end to end. */
    /** A pixel and the housing it is painted on are one surface: off a
     *  square-on view the far half of a strip must not vanish behind its own
     *  body. */
    /** Everything rigged inside a clear-topped step stays visible from any
     *  angle -- seeing it is the whole reason for putting it there. */
    /** One paint shows one instant: the frame draws from a snapshot taken up
     *  front, not from values the engine keeps moving underneath it. */
    /** A head with a fixed cone dims when it is not pointed at you -- and a
     *  fixture whose definition declares no lens is never dimmed at all. */
    /** A beam ends on the floor, or on the step it is thrown over -- and a
     *  step only catches beams that actually cross its footprint. */
    /** The rig view rules its floor in the studio's grid, not a fraction of
     *  whatever the rig happens to measure -- and uniformly in depth. */
    void theRigGridMatchesTheStudioGrid();
    /** A beam is as bright as it is driven, all the way down to nothing --
     *  no constant floor holding it half-lit at 3%. */
    /** An undriven head points where the scene's Aim palette says -- the same
     *  association the studio draws as dashed lines. */
    /** A zoom head's cone follows its zoom channel across the declared
     *  min..max, in whichever direction the definition says. */
    void aZoomHeadsConeFollowsItsZoomChannel();
    /** A look selected in Design lights the rig -- dimmer, colour and beam --
     *  without waiting for a desk to output it. */
    /** A follow-spot aims at a person: the target's XY, at subject height above
     *  whatever they stand on -- and it follows the target as it moves. */
    /** Editing a look updates the rig view: a cache of something the user is
     *  actively editing goes stale the moment they edit it. */
    void editingALookUpdatesTheRigView();
    /** Two fixtures rigged into the same physical space get flagged; two
     *  well apart don't. */
    void overlappingFixturesAreFlagged();
    void aFollowSpotAimsAtTheSubjectNotTheFloor();
    /** A beam aimed at something ends there instead of sailing past it. */
    void aBeamCarriesOnPastWhatItIsAimedAt();
    void aBeamLandsAsAnOvalAtAnAngle();
    void aBeamFadesAlongItsLengthWhicheverWayItPoints();
    void aPersonTargetIsDrawnAsAPerson();
    void anAreaTargetShowsItsSize();
    /** A line target in the studio: drag an end, Alt-drag along it, double-
     *  click the curve to add a point and the point to take it out, drag the
     *  body to move it all; each one undoes. */
    void aLineTargetIsEditedInTheStudio();
    /** Each head aimed at a line traces to its own point, in the studio and
     *  in the rig view. */
    void headsAimedAtALineGoToTheirOwnPoints();
    void aPersonTakesTheColourOfTheLightOnIt();
    void manyWhiteBeamsDoNotBlowOut();
    void aFrameFilledAcrossCoresIsTheSameFrame();
    void aFaceFixtureDrawsOverWhatIsInside();
    void aTargetSaysWhichHeadsCannotReachIt();
    void aBlockedBeamIsTracedToWhatBlocksIt();
    void theLineOfSightMenuSetsWhatBlocks();
    void theReachOverlayShadesWhereASpotCannotAim();
    void aSelectedLookLightsTheRigInDesign();
    void anUndrivenHeadPointsAtTheScenesTarget();
    void aBeamFadesAllTheWayOutWithTheDimmer();
    void aBeamStopsAtWhatItLandsOn();
    /** A lit head puts light between itself and what it lands on; a doused one
     *  does not; and the toggle turns them off. */
    void aLitMoverThrowsAVisibleBeam();
    void aTwoArmHeadDrawsEachArmOnItsOwn();
    void aFixedConeHeadDimsWhenAimedAway();
    void aFrameIsDrawnFromOneInstant();
    /** House left and house right are the same room: a lit strip reads the
     *  same from either side, and does not go dark at whole-rig zoom. */
    void aStripStaysLitFromEitherSideOfTheHouse();
    /** Lifting in-step fixtures clear of the step must not lift them past the
     *  tape on the outside of that same step. */
    void aStepsOwnTapeSitsInFrontOfWhatIsInsideIt();
    void fixturesInsideAStepAreAllVisible();
    void pixelsSurviveOffAxisOnTheirOwnHousing();
    void aMostlyDarkPixelBarDrawsNoBrightOutline();
    /** All twelve of QLCChannel::PrimaryColour, not the five we started with:
     *  subtractive CMY especially, which showed no live colour at all. */
    /** Depth decided per pixel, so a depth-ramped polygon can be partly in
     *  front of and partly behind another -- what one depth per primitive can
     *  never express. */
    /** On a scene where one depth per primitive is enough, the depth buffer
     *  and the painter it replaces must agree -- which is what keeps the
     *  fallback path honest rather than dead. */
    /** A solid box does not show its own far edges -- which the structure
     *  editors did, because only the whole-rig view was depth-buffered. */
    /** A truss chord spans the rig: it must be able to be in front of a deck
     *  at one end and behind it at the other. */
    void aLongTrussSortsAlongItsLength();
    void aSolidBoxHidesItsOwnFarEdges();
    void bothRenderersAgreeOnPlainOcclusion();
    void zRasterResolvesDepthPerPixel();
    /** Translucent surfaces are depth-tested but do not write depth. */
    void zRasterBlendsWithoutOccluding();
    /** There is no such thing as black light: every emitter at zero is not
     *  emitting, however far up the dimmer is. */
    /** A closed shutter emits nothing; a strobing one is still emitting. */
    void aClosedShutterEmitsNothingAStrobeStillDoes();
    void aLampWithNoColourEmitsNothingNotBlack();
    void everyColourModelTheEngineDefinesIsRead();
    void pixelBarDrawsEachPixelInItsOwnColour();
    /** A new or opened show deletes the stage targets while the Doc is still
     *  emitting; the view's target items go first, so nothing reads a freed
     *  target (it hung courseshow_test in Release). */
    void clearingTheShowDropsTargetItemsFirst();

    /* Stage looks (Fixture::StageLook): the LD's word for what it looks like. */
    /** A look decides the silhouette; Auto keeps the definition's. */
    void aStageLookDecidesTheSilhouette();
    /** Every look has a body and a lens inside its rect, in its proportions. */
    void everyStageLookHasItsShape();
    /** The plot redraws a fixture whose look changed: shape, square or round
     *  heads in its lens, the lens hint. */
    void thePlotRedrawsAFixtureWhenItsLookChanges();
    /** Fixture Properties' Plot symbol row sets it on every fixture shown,
     *  reads "mixed" where they differ, and is one undo with the rig. */
    void fixturePropertiesSetsTheStageLook();
    /** The Fixture Manager's Plot Symbol menu: Auto named for its type, every
     *  look, the shared one checked, a choice set on all of them. */
    void theStageLookMenuSetsEveryFixtureSelected();
    /** The studio's flat views draw a look in its proportions. */
    void theStudioDrawsAStageLook();
    /** A lantern is a practical (hurricane / porch lantern): hook, cap,
     *  glass and base, glowing all round. */
    void aLanternIsAPracticalThatGlowsAllRound();
    /** A staff is a pole wound with LED strip: a helix from the side, a
     *  dot from above. */
    void aStaffIsAPoleWoundWithLedStrip();
    /** The show's "draw Auto fixtures as their closest stage look": off
     *  draws exactly as before; on draws each Auto fixture as if its
     *  closest look were set; on the plot, in the studio, and one undo. */
    void autoFixturesCanBeDrawnAsTheirClosestLook();

    /** Output Rules… on two fixtures (one of them mixed) is one rig undo:
     *  ⌘Z puts every fixture's rules and the universe's rule bits back */
    void outputRulesAreOneRigUndoStep();
    /** ⌘C / ⌘V on the plot: a truss comes back as a copy, offset, with the
     *  fixtures it carries; ⌘Z takes the copy away */
    void copyAndPasteDuplicatesATrussWithItsFixtures();
    /** Q55: a pasted truss's fixtures -- Patch at next free addresses
     *  (after the originals; spilling on when the universe is full), Leave
     *  unpatched, or Cancel; one ⌘Z each, the status line says which */
    void pastingATrussAsksWhatItsFixturesGet();
    /** Fixture Properties → Test orientation → Aim at target sends the pan
     *  and tilt AimSolver gives a hung mover for that target */
    void testOrientationAimsWhereTheSolverSays();
    /** Edit Plot's lock refuses a drag of a placed fixture or truss; off,
     *  the same drag moves them */
    void aLockedPlotRefusesADrag();
    /** A folder's position lock refuses moving a member, and a folder
     *  inside it is held by it too */
    void aLockedFolderHoldsItsMembersAndItsSubFolders();

    /** M417: Mount on ▸ Boom / Tower acts on the whole selection, says how
     *  many, each keeping its place up the boom */
    void mountOnABoomOrATowerTakesTheWholeSelection();
    /** M432: Duplicate selected on a truss brings its fixtures, as ⌘C/⌘V */
    void duplicateSelectedBringsATrussesFixtures();
    /** M022: Fixture Properties' Owner combo moves a fixture into the
     *  applied venue (House) and back (Tour); "From <group>: <owner>" */
    void theOwnerInFixturePropertiesMovesItIntoTheVenue();
    /** M076: in a practice the plot draws a scrambled head's pool where the
     *  light really lands */
    void aPracticeDrawsTheScrambledHeadsPoolWhereItLands();
    /** M163: an aimed fixed light's pool (plot) and beam (rig view) follow
     *  its aim, not straight down */
    void aFixedLightAimedByPanAndTiltThrowsItsPoolThere();

    /** Picked on the plot, picked in Plot layers: one row per object (where
     *  it is, not each fixture group listing it besides), only the branches
     *  holding them opened, the first scrolled into view */
    void plotSelectionOpensOnlyItsBranchesInLayers();
    /** Plot layers' "Selected only" (off by default): the selection and
     *  what holds it, following the plot */
    void selectedOnlyShowsTheSelectionAndWhatHoldsIt();
    /** A target is shown, hidden and locked by its layer like everything
     *  else -- in an elevation too -- and has a lock of its own */
    void targetsFollowTheirLayersEyeAndLock();
    /** A line target draws in its own colour, locked or not (a red ring
     *  says locked); a new one never lands on a locked layer; its end
     *  handle drags that end on the grid, one undo step; its body moves */
    void aLineTargetKeepsItsColourLocksAndSnaps();
    /** A truss moved to a layer takes its folder, bars, hangers and
     *  fixtures with it; an old show's "Locked" layer is taken apart on
     *  load and a fixture can be dropped back onto its truss */
    void aStructureMovedToALayerTakesItsMountsWithIt();
    /** With overlay scroll bars, the list stops short of the bar: it never
     *  lies over a layer's lock button */
    void theScrollBarLeavesTheLockButtonsClear();
    /** Right-click a layer: New Folder (named in place), and New Folder
     *  Inside on a folder; New Layer still there */
    void newFolderFromALayersMenu();
    /** What hangs on what is a tree in Plot layers: Tower > Hanger >
     *  Lantern, Truss > Bar > Spot, Stand > Truss, Boom > Crossbar; a
     *  fixture dropped on a hanger or a bar's row is hung there */
    void mountsNestInPlotLayers();

private:
    Doc* m_doc;
};

#endif

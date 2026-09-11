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
    void initTestCase();
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
    void dropOnTrussAttachesAndStays();
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
    void studioTrussDragBlockedWhenLocked();
    void studioTrussDragInEveryPlane();
    /** Every plane moves a truss-mounted fixture in SOME direction -- including
     *  the top view of a vertical run, where there is no axis to slide along
     *  and only the across-the-truss freedom exists. */
    void studioTrussDragAcrossTheRun();
    /** The same drag on a HORIZONTAL truss, and with a fixture whose id is not
     *  0 -- so a pass here isn't an artefact of the id-0 rig. */
    void studioHorizontalTrussDragMovesFixture();
    /** Vertical drag in an elevation on a horizontal run = the drop length
     *  (mountZOffset), so a bar-hung fixture can be raised/lowered there. */
    void studioTrussDragSetsDropInElevation();
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
    void aFollowSpotAimsAtTheSubjectNotTheFloor();
    /** A beam aimed at something ends there instead of sailing past it. */
    void aBeamStopsAtWhatItIsAimedAt();
    void aSelectedLookLightsTheRigInDesign();
    void anUndrivenHeadPointsAtTheScenesTarget();
    void aBeamFadesAllTheWayOutWithTheDimmer();
    void aBeamStopsAtWhatItLandsOn();
    /** A lit head puts light between itself and what it lands on; a doused one
     *  does not; and the toggle turns them off. */
    void aLitMoverThrowsAVisibleBeam();
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

private:
    Doc* m_doc;
};

#endif

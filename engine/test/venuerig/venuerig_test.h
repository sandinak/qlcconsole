/*
  Q Light Controller Plus
  venuerig_test.h

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

#ifndef VENUERIG_TEST_H
#define VENUERIG_TEST_H

#include <QTemporaryDir>
#include <QObject>

class Doc;

class VenueRig_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void ownersRoundTrip();
    void groupOwnerIsInheritedAndOverridden();
    void slotOwnerRoundTrip();
    void sidecarRoundTrip();
    void oldShowsAndVenuesAreUnchanged();
    void houseFixturesMoveIntoTheVenue();
    /** Mutation gap 1 (RC6): with the venue off, or the show replaced, no
     *  id is still the venue's: a show fixture holding a former house id
     *  (or set to House with no venue applied) is in the show file. */
    void aVenueOffLeavesNoVenueFixtures();
    void switchingVenuesSwapsTheHouseRig();
    void aGroupCuePlaysOnEachVenuesHouseRig();
    void aTakenIdIsRenumbered();
    void newShowFixturesSkipHouseIds();
    void theClashCheck();
    void notOursIsNeverSent();
    void theVenuePatchIsAppliedAndRestored();
    void moveTheShowsHouseFixturesIn();
    void aPracticeKeepsTheHouseRig();
    /** Not ours stays zero through Blind holding the Blackout-exempt
     *  channels, and through an output-patch blackout */
    void notOursStaysZeroThroughBlindAndBlackout();
    /** On the real tick (the timer's thread, the universes' threads): a
     *  range is zero on the wire, a whole universe sends nothing */
    void notOursOnTheRealTick();
    /** "Patching there refused": every way a fixture gets an address */
    void patchingOntoNotOursIsRefusedEveryWay();
    /** The clash check on open: overlap, not ours, slot -- the note, and
     *  an overlapping house fixture in unpatched */
    void theClashCheckRunsOnOpen();
    /** A sidecar written by a newer build (version 3, unknown elements and
     *  attributes) still loads, everything known intact, and saves back */
    void aSidecarFromANewerBuildStillLoads();
    /** A practice scramble never reaches the show file, nor the real
     *  venue's section of the sidecar */
    void aPracticeNeverReachesTheShowFile();

    /* Mutation gaps in venuerig.cpp (the RC6 survivors) */
    /** The venue's patch on U2: kept, put back on, taken off */
    void aVenuePatchOnASecondUniverse();
    /** The show replaced: no held patch base, nothing not ours left */
    void resetClearsTheUniverses();
    /** capture() with nothing changed says so */
    void captureSeesNoChange();
    /** capture() sees each kind of house-rig change */
    void captureSeesEachChange();
    /** Owners followed on fixture and group add and remove */
    void ownersFollowAddsAndRemoves();
    /** samePatch() compares every field; nothing edited, nothing kept */
    void samePatchComparesEveryField();
    /** A house fixture and a patch past the show's universes */
    void aVenueBeyondTheShowsUniverses();
    /** A house fixture just beside not ours, or unpatched on it */
    void houseFixturesBesideNotOurs();
    /** A fill head's fixture head index and cell, through every path */
    void fillHeadGeometryRoundTrips();
    /** A fill found again by its group's name; kept for a deleted group */
    void aFillFindsItsGroupByName();
    /** The sidecar patch keeps its uid, network and parameters */
    void sidecarPatchKeepsUidAndNetwork();
    /** A first output added; a universe added after apply */
    void patchEditsOnNewOutputsAndUniverses();
    /** Fill heads outside a shrunk group grow it again */
    void fillHeadsOutsideTheGridGrowIt();
    /** The dry run reads the house fixtures' universe, size, patched */
    void theDryRunReadsTheHouseFixtures();
    /** The clash report's universe, address, channels and text */
    void theClashReportFieldsAndText();
    /** A house fixture's calibration edit is recorded when the timer fires */
    void aHouseCalibrationEditIsRecordedOnTheTimer();
    /** A fixture set to House drops its show calibration base */
    void aFixtureMovedInLosesItsCalibrationBase();
    /** A renumbered house fixture skips every venue's house ids */
    void aRenumberedIdSkipsOtherVenuesIds();

    /* New Venue from a Show (docs/design/VENUE_FROM_SHOW.md) */
    /** Another show's fixtures come in as House, on the venue's universe at
     *  their own addresses, at their plot places; the show is unchanged */
    void aVenueFromAnotherShow();
    /** Its groups are suggested by name; only confirmed matches fill, the
     *  rest wait in the venue and can be matched later */
    void aVenueFromAnotherShowFillsOnlyConfirmedGroups();
    /** The venue's own universe: no output, the imported Art-Net left over,
     *  or the show's own output are clashes; a chosen one is not */
    void theRoomOutputAndItsClashes();
    /** An Art-Net room output with no route yet is kept waiting and said
     *  so; it goes out once the network is there; the venue keeps only its
     *  own universe's patch through any patch signal */
    void anArtNetRoomOutputWaitsForItsNetwork();
    /** Removing the venue leaves the show and its fixtures as they were */
    void undoingAVenueFromAnotherShow();
    /** A file that isn't a show, or has no fixtures, is refused with a reason */
    void aVenueFromAFileThatIsNotAShow();
    /* Lining up the room (docs/design/VENUE_ROLES_DATUM.md, phase 1) */
    /** A room file with a cyc into a show on a smaller grid: a house light
     *  lands at the show's centre line and upstage line plus its distances
     *  from the room's; the tour and the show file unchanged */
    void importedRoomLinesUpOnTheDrape();
    /** Line Up the Room on a venue made before: every house fixture moves
     *  by the offset, once; the tour, its trusses and targets do not; the
     *  drape measured downstage of the cyc on site; a missing file said */
    void lineUpMovesHouseNotTour();
    /** A house light moved with the venue applied is kept less the offset;
     *  applied again, it lands where it was dropped */
    void houseDragSavesRoomCoordinates();
    /** Moving the show's upstage line moves the house rig, not the tour;
     *  an undo puts the plot back without moving it twice */
    void upstageLineFollowsMovedShowLine();
    /** A venue with no <Datum> applies as before; a newer build's datum is
     *  kept whole */
    void oldVenueAppliesWithoutOffset();
    /* House roles (docs/design/VENUE_ROLES_DATUM.md, phase 2) */
    /** A role is a tag on a group: saved only when set, read back, kept
     *  through a rename; a role group refuses an owner; a copy has none */
    void roleTagRoundTrip();
    /** The first Fill Roles makes the eight groups, the palettes, six
     *  standard cues and their override Global hits; never again; his own
     *  group of a role's name is kept, or used when he says */
    void rolesMadeOnFirstUseOnly();
    /** A standard cue he edited is not overwritten; a hit he deleted is
     *  made again */
    void standardCuesNotOverwritten();
    /** Use My Groups on a House group: it stays House, its light the
     *  venue's, and the house cue plays it (review finding 1) */
    void adoptedHouseGroupStaysHouse();
    /** A group of a role's name holding a tour light is not used; a tour
     *  light is refused by a role group; a house hit never drives it */
    void tourLightsNeverInARole();
    /** Fill Roles on the PMS-like room: one rig pass, one write (finding 2) */
    void fillRolesIsOnePass();
    /** A venue switched in Operate re-plans nothing until Design (finding 3) */
    void roleBakeWaitsForDesign();
    /** The suggestions for a PMS-like room: words, the mover, the CYC
     *  model, the place over the stage (section 4's table, trimmed) */
    void suggestPmsLike();
    /** Not ours never suggested; a mover with no word left for him */
    void notOursAndMoversNeverSuggested();
    /** Stage up on a 3-channel RGB light reads 255/255/255, House to half
     *  128/128/128 (a Dimmer look alone writes nothing there) */
    void stageUpLightsRgbOnlyHead();
    /** House to half on a relay (the Switch rule) is on; on a dimmer, half */
    void houseToHalfOnRelayIsOn();
    /** A role the room lacks sends nothing and says "none in <venue>" (the
     *  house desk when the room keeps channels); Show Review VEN-11 */
    void emptyRoleLayerSendsNothingAndSaysSo();
    /** One standard cue, two venues: each room's lights */
    void roleCueFollowsVenueSwitch();
    /** A real touring show and a real room's show (QLC_VENUE_REAL_SHOW,
     *  QLC_VENUE_REAL_ROOM; skipped without them), copied into a temp dir:
     *  the import's counts into $QLC_SHOT_DIR/venue-import-report.txt */
    void aRealShowIntoAVenue();

private:
    QString showFile() const;
    quint32 dimmers(int count, quint32 universe, quint32 address, const QString &name);
    void tick(int n = 4);
    uchar dmx(quint32 universe, quint32 address);

    Doc *m_doc = nullptr;
    QTemporaryDir *m_dir = nullptr;
};

#endif

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

private:
    QString showFile() const;
    quint32 dimmers(int count, quint32 universe, quint32 address, const QString &name);
    void tick(int n = 4);
    uchar dmx(quint32 universe, quint32 address);

    Doc *m_doc = nullptr;
    QTemporaryDir *m_dir = nullptr;
};

#endif

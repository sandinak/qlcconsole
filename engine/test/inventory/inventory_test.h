/*
  Q Light Controller Plus - Test Unit
  inventory_test.h

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

#ifndef INVENTORY_TEST_H
#define INVENTORY_TEST_H

#include <QObject>

class Doc;

/** Inventory (unpatched) fixtures: they exist in the show but hold no address,
 *  and must never reach DMX. See docs/proposals/inventory-first-workflow.md. */
class Inventory_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void defaultsAreAPatchedDmxFixture();
    void manyUnpatchedFixturesCanCoexistAtTheDefaultAddress();
    void unpatchedFixtureClaimsNoAddress();
    void unpatchedFixtureNeverReachesTheUniverse();
    void patchFixtureAssignsAndRegisters();
    void patchFixtureRefusesAnOverlap();
    void patchFixtureRefusesToRunPastTheUniverse();
    void patchedFixtureWritesAfterPatching();
    void groupingNamesFileFixturesByManufacturerAndModel();
    void unpatchReturnsAFixtureToTheInventory();
    void unpatchRefusesInOperateModeAndForStagedFixtures();
    void xmlRoundTripKeepsInventoryFields();
    void ordinaryFixtureXmlIsUnchanged();

    void plannerPacksDmxFirstFitAroundPatchedFixtures();
    void plannerSpillsIntoANewUniverseWhenFull();
    void plannerGivesEachIpItsOwnUniverse();
    void plannerKeepsDmxOutOfNetworkUniverses();
    void plannerIsPureAndApplyCommitsIt();
    void plannerSkipsPatchedAndUnknown();
    void plannerKeepsIdenticalFixturesTogether();
    void plannerNumbersCompareAsNumbers();

    void modelNamesKeepTheirOwnDigits();

    void modeCanChangeWhenNothingReferencesItsChannels();
    void aSceneThatSetsChannelsDirectlyLocksTheMode();
    void aModeWithADifferentHeadCountIsRefused();
    void patchTimeModeChangeSetsTheFootprintBeforePatching();
    void patchTimeDimmerChannelsCanChange();
    void aRefusedModeChangeLeavesTheFixtureUnpatched();

    void aFixtureTooBigForAUniverseIsNotedNotPlaced();
    void applyReportsAnAddressTakenSincePlanning();
    void applyRefusesADimmerResizeAScenePins();
    void applySkipsAFixtureDeletedSincePlanning();
    void applyPointsAnArtNetUniverseAtTheFixtureIp();
    void aWledFixtureGetsTheWledOutputAimedAtIt();
    void theTransportRoundTrips();
    void applyBorrowsTheOutputOfAnotherNetworkUniverse();
    void applyAsksForAnOutputWhenNoNetworkUniverseExists();

    void repatchingMovesTheFixturesClaim();
    void anUnknownFixtureCannotChangeMode();
    void aModelNameThatNeverFreesUpIsReturnedAsIs();
    void groupingNamesForGenericAndRgbPanels();

    void eachNodeTakesTheFixturesNearIt();
    void equallyNearPortsFillLeastLoadedFirst();
    void aFullPortOverflowsToAFreePortThatApplyClaims();
    void aWirelessPortIsNearWhereItsReceiverIs();
    void placedFixturesArePlannedBeforeTheRest();
    void unplacedNetworkAndOptedOutFixturesKeepTheOldRules();
    void nothingNearWithRoomFallsBackToFirstFit();

    // Re-addressing patched fixtures ("clean ways to assign fixtures into universes")
    void aBlockCanShiftOntoItsOwnChannels();
    void patchFixturesIsAllOrNothing();
    void movingAPatchedFixtureIsDesignModeOnly();
    void findFreeAddressSeesThroughTheFixturesMoving();
    void aClashingFixtureLoadsUnpatchedAndSavesUnchanged();
    void patchingAClashClearsIt();
    void placeKeepsSpacingAsOneBlock();
    void placePacksAroundOthersAndSpills();
    void applyMovesPatchedFixturesTogether();

private:
};

#endif

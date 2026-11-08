/*
  Q Light Controller Plus - Test Unit
  fixturecreator_test.h

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

#ifndef FIXTURECREATOR_TEST_H
#define FIXTURECREATOR_TEST_H

#include <QObject>

class Doc;

/** Drives the real "Add to inventory" dialog end to end and checks what it
 *  creates and where that shows up. */
class FixtureCreator_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void addToInventoryCreatesAnUnpatchedFixture();
    void aStagedFixtureAppearsInNoUniverseAnywhere();
    void aStagedFixtureNeverOccupiesAnAddress();
    void ordinaryAddStillPatches();
    void addPastTheUniverseLimitSaysSo();
    void networkFixtureKeepsItsIp();
    void aBatchOfNetworkFixturesGetsOneIpEach();
    void aMultiAddStagesEveryFixtureUnpatched();

    /** A run of a model whose name ends in digits numbers with a counter and
     *  never invents models (LM70, LM71, LM72...). */
    void aMultiAddOfANumberedModelAppendsACounter();
    /** An explicit "#N" counter in a typed name still continues (US #4, #5, #6). */
    void anExplicitCounterStillContinues();
    /** A taken default name gets a counter appended. */
    void aTakenDefaultNameGetsACounterNotBumpedDigits();

    void aPreselectedUniverseIsWhereTheFixtureLands();
    void anInvalidAddressIsRefusedWithAMessage();
    void aCancelledDialogCreatesNothing();
    void anEmptyNameFallsBackToTheModelOrGenericDimmer();
    void newFixturesJoinTheGroupTheyWereAddedFrom();
    void aFixtureTheDocRefusesIsNotLeaked();
    void aSingleAddWithATakenNameIsNumbered();

    /* WLED by its own API: one shot from Add Fixture (docs/features/fixture-manager.md). */
    /** It gets a universe of its own sent to its IP -- never the Art-Net
     *  universe the dialog was opened in -- and the dialog says so. */
    void aWledGetsAUniverseOfItsOwn();
    /** Four at once, counting-up IPs: a universe each. */
    void aBatchOfWledsGetsAUniverseEach();
    /** The universe already sent to its IP (its fixture deleted) is reused. */
    void aWledReusesTheUniverseSentToItsIp();
    /** Another fixture at its IP on the Art-Net universe does not draw it there. */
    void aWledNeverFollowsItsIpOntoArtNet();
    /** One Cmd/Ctrl+Z takes the add back: the fixture and its universe. */
    void addingAWledIsOneUndo();
    /** A show like the one that found it: the WLED on the Art-Net universe
     *  is a footer finding, and its fix moves it (one undo). */
    void aWledOnAnArtNetUniverseIsAFindingWithAFix();
    /** Fixture Properties turning a fixture into a WLED by its API moves it
     *  the same way. */
    void propertiesMakingItAWledMovesIt();
    /** QLC_SHOT_DIR set: the dialog with WLED picked and the finding, before
     *  (main's widgets) and after, at three sizes, for review. */
    void wledShotsForReview();

private:
    /** The WLED output and a stub named ArtNet (it sends nowhere), loaded
     *  into m_doc; universe 1 "U01-Office" goes out as that Art-Net with a
     *  60-channel bar on it, as in the show that found the bug. */
    void officeShow();
    Doc *m_doc;
};

#endif

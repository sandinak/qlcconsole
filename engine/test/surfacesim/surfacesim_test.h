/*
  Q Light Controller Plus
  surfacesim_test.h

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

#ifndef SURFACESIM_TEST_H
#define SURFACESIM_TEST_H

#include <QObject>
#include <QTemporaryDir>

class Doc;
class IOPluginStub;

/* The surface simulator's own checks, and every input profile driven
   through it (docs/design/SURFACE_SIMULATOR.md): each profile's every
   control sent as the device sends it, through the plugin's decoder, the
   input patch and its flush, arriving where the app listens; each control
   with a lamp lit back as MIDI that addresses it. One row per profile
   file, so a new profile is tested by being added. */
class SurfaceSim_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /** The simulated board's bytes are the plugin decoder's (note on,
     *  note off, CC, the PMJ's MIDI channel 9) and what goes back out is
     *  MidiPlugin's MIDI. */
    void theBoardSpeaksMidi();
    /** A script (and a capture) plays on the board as the person did it. */
    void aScriptPlaysAsThePersonDidIt();

    /** Every control of every profile reaches the app; every lamp lights. */
    void everyProfileControlReachesTheApp_data();
    void everyProfileControlReachesTheApp();

    /** Phase R: a board's state dump after a replug fires no ALL OFF. */
    void aStateDumpAfterAReplugFiresNoAllOff();
    /** Phase R: the same dump fires no desk STOP or GO (both dump shapes). */
    void aStateDumpFiresNoDeskRole();
    /** Phase R: the PMJ's Go held across a replug, resending, is no GO
     *  until a release then a press (desk GO and a show trigger). */
    void aKeyHeldAcrossAReplugGoesOnlyAfterARelease();
    /** The desk's STOP and GO lamps are the Control Map's (PMJ, APC). */
    void deskLampsAreTheControlMaps_data();
    void deskLampsAreTheControlMaps();
    /** Q314: the show's GO's lamp follows the desk's GO going live or off. */
    void theShowsGoLampFollowsTheDesk();
    /** Two identical boards are two universes: a key learned on the
     *  second is not the first's. */
    void twoIdenticalBoardsAreTwoControllers();

    /** Every built-in surface layout holds to its profile: each control
     *  placed exactly once, nothing overlapping, no channel the profile
     *  lacks, its roles on a control of the right kind. */
    void everyLayoutHoldsToItsProfile();
    /** The self-test catches a broken layout (a channel missing, one
     *  twice, an overlap, one the profile lacks, a role on nothing). */
    void theLayoutSelfTestCatchesABrokenOne();
    /** A fresh computer and an APC40 mk2: its default roles are this
     *  computer's (PLAY GOes the Run tab's list; RECORD, STOP ALL CLIPS
     *  and MASTER are Back, STOP and the Grand Master). */
    void aNewBoardSeedsItsDefaultRoles();
    /** A GO learned on another board wins: the default seeds the rest. */
    void aLearnedRoleWinsOverTheDefault();
    /** A default cleared stays cleared: a board is seeded once. */
    void aClearedDefaultStaysCleared();
    /** A default role is not seeded onto a control the open show already
     *  uses (the desk would block the show's trigger). */
    void aDefaultLeavesTheShowsTriggerAlone();
    /** The MaizeDMX Playback Controller: GO, Back, the Grand Master and
     *  STOP (X10) from its layout. */
    void theMaizeDmxSeedsItsDefaults();
    /** The APC64 in Session mode (its DAW port): PLAY GO, RECORD Back,
     *  STOP ALL CLIPS STOP, touch strip 8 the GM; pads on MIDI channel 7
     *  apart from the buttons on 1. */
    void theApc64SeedsItsDefaults();
    /** 053 surfaces on the bench: a board patched with no profile gets the
     *  one made for its device (a profile's Match) -- the init, the map and
     *  the roles follow; a chosen profile and an unknown name are left. */
    void aProfileIsChosenByTheDevice();
    /** The APC mini mk2 is a drawn surface with the APC40's default roles. */
    void theApcMiniMk2SeedsItsDefaults();
    /** A user's copy of a surface profile older than the built-in one (a
     *  legacy link) is not used, and the panel says so; a good copy is. */
    void anOlderCopyOfASurfaceProfileIsNotUsed();
    /** The board holding the core roles is gone when a new one comes
     *  online: the roles move, with a notice and an Undo. */
    void aGoneBoardHandsItsRolesToTheNewOne();
    /** The old board is still here: nothing moves; the new one is given a
     *  role only when the old lets go of it. */
    void aBoardThatIsHereKeepsItsRoles();
    /** A trigger learned on another board does not light a lamp on a drawn
     *  surface that has no such control (an APC pad blinking at 8,2). */
    void aLampForAControlTheBoardLacksIsNotSent();
    /** The MaizeDMX has two pages, each on its own X key. */
    void theMaizeDmxHasAPager();

    /* Setting up a controller (SurfaceSetup, the OpenDeck model in
       simopendeck.h): the device's own config, read and written over SysEx. */
    /** The PMJ's factory LEDs are read and found not under their buttons. */
    void theLampCheckReadsTheBoard();
    /** Align: a full backup first, then each lamp under its own button;
     *  Undo Align puts the factory order back (and keeps the aligned one). */
    void alignBacksUpAndUndoRestores();
    /** In Operate nothing is written unless the person confirmed. */
    void aWriteIsRefusedInOperate();
    /** A board that never answers: said, and nothing written. */
    void aSilentBoardIsSaidAndNotWritten();
    /** Identify: every lamp findable, flashed, paired by a press, the
     *  board put back and the pairing kept. */
    void identifyPairsLampsAndPutsTheBoardBack();
    /** The APC40 mkII's Ableton mode: sent when patched, on a replug, on a wake. */
    void theInitIsSentOnPatchReplugAndWake();
    /** Rescan says when the computer sees no new controller. */
    void aRescanThatFindsNothingSaysSo();
    /** While Identify listens, the board's keys do no job (no GO, no
     *  Back, no trigger); the desk's STOP still stops. */
    void identifyHoldsTheKeysJobs();
    /** While Identify flashes the board's lamps no other lamp writer
     *  reaches it; when it ends, the show's lamps are sent again. */
    void identifyHasTheLampsThenGivesThemBack();
    /** The APC64: its display taken and named on patch and replug, given
     *  back on exit; Session mode checked by a pad press. */
    void theApc64IsNamedAndItsModeChecked();

private:
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    QTemporaryDir m_settings;
    QTemporaryDir m_backups;
};

#endif

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
    /** The MaizeDMX Playback Controller: GO, Back, the Grand Master and
     *  STOP (X10) from its layout. */
    void theMaizeDmxSeedsItsDefaults();
    /** The APC64 in Session mode (its DAW port): PLAY GO, RECORD Back,
     *  STOP ALL CLIPS STOP, touch strip 8 the GM; pads on MIDI channel 7
     *  apart from the buttons on 1. */
    void theApc64SeedsItsDefaults();

private:
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    QTemporaryDir m_settings;
};

#endif

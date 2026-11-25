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
    /** Two identical boards are two universes: a key learned on the
     *  second is not the first's. */
    void twoIdenticalBoardsAreTwoControllers();

private:
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
};

#endif

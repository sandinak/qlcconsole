/*
  Q Light Controller Plus - Unit test
  surfacemap_test.h

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

#ifndef SURFACEMAP_TEST_H
#define SURFACEMAP_TEST_H

#include <QObject>
#include <QTemporaryDir>

class IOPluginStub;
class Doc;

/* The surface map (Connections ▸ Control surfaces, Q311), on simulated
   boards (engine/test/common/surfacesim): a press lights its own control,
   each control's job in its group, the lamps as last sent, a board with no
   layout falls back to the rows, the footer's chip opens its map. */
class SurfaceMap_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /** A press on the APC40 lights that pad; a fader moved shows its value. */
    void aPressLightsItsOwnControl();
    /** The desk's roles, the show's jobs and the free controls, each in
     *  its group; a hit armed only in another cue is dimmer, its place in
     *  the tooltip. */
    void eachJobIsInItsGroup();
    /** "GO is PLAY ...": the board's fixed jobs said in one line; a role
     *  the computer lacks has Set Up…, which starts the desk's Learn. */
    void theFixedJobsAreSaidAndALackingOneIsSetUp();
    /** The PMJ's own jobs (its overlay's) are the board's, Go its GO. */
    void thePmjsOwnJobsAreTheBoards();
    /** A light shows what the app last sent it. */
    void aLampShowsWhatWasSent();
    /** A board with no layout: no drawing, one line, the rows stay. */
    void aBoardWithNoLayoutFallsBackToTheRows();
    /** The footer's chip per board opens its map. */
    void theFooterChipOpensItsMap();
    /** The FaderPort 2 is drawn: its fader, knob and transport keys, its
     *  jobs GO / Back / STOP / GM, and what is pressed lights. */
    void theFaderPort2IsDrawn();
    /** Each built-in board is drawn like the real one: the PMJ's Master, Go
     *  and Back left of the strips, its keys 3 across by 4 down, ten
     *  faders and four round encoders; the FaderPort's one long fader; the
     *  APC40's 8 x 5 clip grid and its eight device knobs; the APC64's 8 x 8
     *  pads between its touch strips. */
    void eachBoardIsDrawnLikeTheRealOne();
    /** The control table: one row per control, the columns, a sort, the
     *  filter box, and a row and its control select each other. */
    void theControlTableAndTheDrawingSelectEachOther();
    /** A click on a control offers its jobs; GO, Back, STOP, GM and the
     *  page keys go to the desk and the pages; No job and Undo; the PIN in Operate. */
    void aClickGivesAControlAJob();
    /** Page ◀ and ▶ the map lets you choose: they step the pages, are kept
     *  for this computer, and the chip's page name picks a page. */
    void pagesHaveKeysTheMapChooses();
    /** A map never clips: under its smallest cell it asks for the width. */
    void theMapScrollsRatherThanClips();
    /** With QLC_SHOT_DIR: the hub with each board's map at three sizes. */
    void shots();

private:
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    QTemporaryDir m_settings;
};

#endif

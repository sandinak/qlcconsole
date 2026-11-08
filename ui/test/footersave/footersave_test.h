/*
  Q Light Controller Plus
  footersave_test.h

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

#ifndef FOOTERSAVE_TEST_H
#define FOOTERSAVE_TEST_H

#include <QObject>
#include <QTemporaryDir>

class App;

class FooterSave_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** Saving says "Saved" where it said "Unsaved changes", and nothing else
     *  in the footer moves: not the stage chip, not the power chip, and no
     *  squeezed chip gets its room back for one save */
    void savingLeavesTheFooterWhereItWas();
    /** The same with a scene's edits held by the programmer, whose chip
     *  turns from red to grey "(saved, revertable)" */
    void savingWithEditsInTheProgrammerLeavesTheFooterWhereItWas();

    /** Routine updates (the poll, a Ready check, the rig network, Review,
     *  Timeline) restyle no chip and move none: each changes only on a change */
    void routineUpdatesChangeNoChipAndMoveNone();
    /** An autosave leaves "Unsaved changes" as it was: the hover says when */
    void anAutosaveDoesNotTurnTheDirtyChipOverAndBack();
    /** The Load chip has room for its stall text, and the Next cue shortens
     *  to a share of the window, so neither pushes the chips after it out */
    void theLoadAndNextChipsKeepTheirRoom();
    /** A jump that only looks keeps the Show Tree's cue playing; leaving the
     *  tab any other way stops it, as it always did */
    void aLookingJumpKeepsThePreviewPlaying();

    /** No text a person reads is UTF-8 read as Latin-1 ("â–¾" for "▾"):
     *  what the main window's widgets say, and no source that would make it
     *  (a QStringLiteral with \\xE2\\x96... bytes is Latin-1 in a QString) */
    void noTextIsUtf8ReadAsLatin1();

private:
    void sweepSaves();

    App *m_app = nullptr;
    QTemporaryDir m_dir;
};

#endif

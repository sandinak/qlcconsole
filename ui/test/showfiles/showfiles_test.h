/*
  Q Light Controller Plus
  showfiles_test.h

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

#ifndef SHOWFILES_TEST_H
#define SHOWFILES_TEST_H

#include <QObject>
#include <QStringList>

class App;

/** Old shows stay whole (data safety): every show file this build has --
 *  the stock QLC+ 4.x shows and the qlcconsole 0.1 show in test-workspaces/,
 *  QLC+'s bundled Sample.qxw, and any other .qxw / .qlcc found there on this
 *  computer -- opened through the app, saved, and opened again, comes back
 *  the same, and the file opened is never written. */
class ShowFiles_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    /** Open a copy, save it, open the save: the same fixtures, groups,
     *  palettes, functions (and what each holds), universes, Virtual
     *  Console, plot, Show Tree and settings; and saving that again writes
     *  the same bytes. A whole show is never taken for a damaged one, and a
     *  QLC+ 4.x show keeps Auto-open shutters off through the round trip. */
    void aShowSavedAndOpenedAgainIsTheSame_data();
    void aShowSavedAndOpenedAgainIsTheSame();

    /** A show saved by a newer build -- elements and attributes this build
     *  has never heard of, at every level -- opens with everything this build
     *  knows intact (it reads past what it doesn't know). */
    void aShowFromANewerBuildOpens_data();
    void aShowFromANewerBuildOpens();

    /** Opening a show (and closing it) never writes the file, or anything
     *  beside it. And opened and not touched, a show is not modified:
     *  closing it asks nothing and no autosave is written. A show whose
     *  trusses have no group yet (help-demo, Office) gets them as the plot
     *  is built -- derived the same at every open, so not an edit (F3). */
    void openingAShowNeverWritesIt_data();
    void openingAShowNeverWritesIt();

    /** A show file cut short opens as far as it can be read, says so (a
     *  warning naming the line, and the footer's load problems), and a
     *  plain Save never writes over the original: it goes to Save As. Saved
     *  under a name of its own, it is an ordinary show again (F2). */
    void aDamagedShowOpensAndIsNotSavedOver_data();
    void aDamagedShowOpensAndIsNotSavedOver();

    /** A QLC+ 4.x show opens with Auto-open shutters off, so it sends the
     *  same DMX here as in QLC+; once saved here the setting is in the file
     *  and opens the same; a new show has it on. (Every other stock show is
     *  checked in aShowSavedAndOpenedAgainIsTheSame.) */
    void aStockShowOpensWithAutoOpenOff();

    /** File ▸ Collect for Transfer…: the folder holds the show and its
     *  venue file, and the show opened from there has its venues. */
    void collectForTransferTakesTheVenues();

    /** Save As into another folder: the truss and tower pictures kept with
     *  the show are copied into the NEW folder's images/ and the file names
     *  them there; the old folder is not written; opened from its new
     *  place, the show finds them (C3). Also when the old folder's name
     *  starts the new one's ("Show 2" -> "Show"). */
    void saveAsToAnotherFolderTakesThePictures();

    /** File ▸ Collect for Transfer…: every picture the show uses (truss
     *  socks, tower faces, plot images), from the show's own
     *  images/ or from anywhere else, is copied into the folder's images/
     *  and the collected show names it there, relatively. Opened from the
     *  folder on a computer without the originals, it finds them all; the
     *  open show keeps its own paths. */
    void collectForTransferTakesThePictures();


private:
    QStringList fingerprint() const;
    void addShowRows(bool stockOnly);

private:
    App *m_app = nullptr;
};

#endif

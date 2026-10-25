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
     *  the same bytes. */
    void aShowSavedAndOpenedAgainIsTheSame_data();
    void aShowSavedAndOpenedAgainIsTheSame();

    /** A show saved by a newer build -- elements and attributes this build
     *  has never heard of, at every level -- opens with everything this build
     *  knows intact (it reads past what it doesn't know). */
    void aShowFromANewerBuildOpens_data();
    void aShowFromANewerBuildOpens();

    /** Opening a show (and closing it) never writes the file, or anything
     *  beside it. */
    void openingAShowNeverWritesIt_data();
    void openingAShowNeverWritesIt();

    /** A QLC+ 4.x show opens with Auto-open shutters off, so it sends the
     *  same DMX here as in QLC+; once saved here the setting is in the file
     *  and opens the same. */
    void aStockShowOpensWithAutoOpenOff_data();
    void aStockShowOpensWithAutoOpenOff();

    /** File ▸ Collect for Transfer…: the folder holds the show and its
     *  venue file, and the show opened from there has its venues. */
    void collectForTransferTakesTheVenues();

private:
    QStringList fingerprint() const;
    void addShowRows(bool stockOnly);

private:
    App *m_app = nullptr;
};

#endif

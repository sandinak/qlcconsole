/*
  Q Light Controller Plus
  showtreenotes_test.h

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

#ifndef SHOWTREENOTES_TEST_H
#define SHOWTREENOTES_TEST_H

#include <QObject>

class Doc;

class ShowTreeNotes_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void notesOnEveryKind();
    void editTickRemoveAreUndoable();
    void cueNoteIsTheFirstEntry();
    void openNoteIsNeedsWork();
    void saveLoadKeepsNotesAndSnapshot();
    /** A show's and a song section's plain-text note is its first Notes
     *  entry, edited both ways, saved and loaded. */
    void showAndSectionNotesAreTheFirstEntry();
    /** Renaming a section or renumbering its song keeps its note; saved after. */
    void aSectionNoteSurvivesRenameAndRenumber();
    /** A hand-edited first-level number carries the section (name, start,
     *  note) to the new key in the same step; undo, save and load; a clash
     *  keeps both. */
    void editingASongsNumberCarriesItsSectionToTheNewKey();
    /** A head with only a note is not pruned when a start is set or cleared. */
    void aNoteOnlySectionHeadIsNeverPruned();
    /** The Target is a stable hash; a show saved with the first (qHash) one
     *  still loads; an unknown note kind is skipped. */
    void sectionNoteTargetsAreStableAndOldOnesStillLoad();
    /** Every note on a song (the user's too) follows it; all songs move
     *  together; a true clash keeps both heads and names every key. */
    void everySectionNoteFollowsItsSongAndSongsMoveTogether();
    void anOlderShowsCueNotesMigrate();
    void deletedItemsNotesKeptForUndoNotSaved();
    void picturesStayOutOfUndoSteps();

private:
    Doc *m_doc = nullptr;
    quint32 m_prod = 0, m_list = 0, m_cue1 = 0, m_cue2 = 0, m_layer = 0, m_palette = 0;
};

#endif

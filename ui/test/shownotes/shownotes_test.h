/*
  Q Light Controller Plus
  shownotes_test.h

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

#ifndef SHOWNOTES_TEST_H
#define SHOWNOTES_TEST_H

#include <QObject>

class StructureStudioView;
class Doc;

class ShowNotes_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void thePaneFollowsTheSelection();
    void anEmptiedPaneDrawsNoOldNoteOverItsEmptyLine();
    void addTickAndUndoFromThePane();
    void rowsWithNotesAreMarked();
    void snapshotAndShowMe();
    void showMeInOperateOnlyMovesTheCamera();
    void theNotesListFiltersAndJumps();
    void exportMakesMarkdownAndPdf();
    void shots();

private:
    Doc *m_doc = nullptr;
    StructureStudioView *m_rig = nullptr;
    quint32 m_prod = 0, m_list = 0, m_cue1 = 0, m_cue2 = 0, m_cue3 = 0, m_layer = 0, m_palette = 0;
};

#endif

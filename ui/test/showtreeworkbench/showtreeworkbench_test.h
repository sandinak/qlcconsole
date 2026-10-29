/*
  Q Light Controller Plus - Test Unit
  showtreeworkbench_test.h

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

#ifndef SHOWTREEWORKBENCH_TEST_H
#define SHOWTREEWORKBENCH_TEST_H

#include <QObject>
#include <QList>

class Doc;

/* W3 (docs/design/WORKFLOW_DESIGN.md §3.4): a Show Tree cue opened in the Workbench,
   docked in the tree -- the TreeCueStack back end, the dock and ↩. */
class ShowTreeWorkbench_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** A cue docks the Workbench in place of the page: own layers and,
     *  grey, what tracks in, each attribute a row with where it comes
     *  from; the cue plays in context. */
    void openShowsTheCue();
    /** A value dropped on a layer changes the output at once; Undo and
     *  Redo (the tree's) take it back and put it back, the rows follow. */
    void editUndoRedo();
    /** Carried-in rows: not removed, cleared or moved; a value set on one
     *  makes it the cue's own (the tree's rule). */
    void trackedRowsReadOnly();
    /** The tree's own pages are lent to the editor pane; a value with a
     *  palette edits in the Look Editor. */
    void editorPane();
    /** ↩ Show Tree: the page back, the cue and the layer selected. */
    void backKeepsSelection();
    /** Build's Workbench is lent and given back loaded as it was; Build
     *  works after; leaving the tab hands it back and coming back docks. */
    void buildAfterHandBack();
    /** `wb`, double-click, ✎, following the tree. */
    void waysIn();
    /** Solo: only the selected layer plays; off, the cue in context. */
    void solo();
    /** A ▣ layer opens in the Workbench (↩ back to the cue); + EFX is one
     *  tree step with its function. */
    void opaqueLayersAndAddNew();
    /** ⇧⌘N with the focus in the docked Workbench: a cue there makes the
     *  tree's next cue (the Workbench follows to it); a collection opened
     *  there gets a new scene layer -- each one undo step. */
    void newHereInTheDockedWorkbench();
    /** Esc backs out (shortcuts.md): a colour palette double-clicked open
     *  in Build ▸ Palettes goes back to the page that was showing, the
     *  rows' selection and the edit kept -- from the list, from the Look
     *  Editor, from inside its colour picker (which no longer hides
     *  itself); ↩ Back does the same. A rename in the list: Esc cancels
     *  only the rename. */
    void escBacksOutOfAPaletteInBuild();
    /** Esc on a group's layout: ← Back to the scene (or to the rows'
     *  page); on a ▣ opened layer: ↩ to the collection, the row selected. */
    void escBacksOutOfALayoutAndAnOpenedLayer();
    /** The Show Tree: Esc in the docked Workbench backs out a level at a
     *  time (a palette, then ↩ Show Tree, the layer selected); undocked,
     *  the Look Editor under the page closes (↩ Back too). */
    void escBacksOutInTheShowTree();

    /** W3_SHOTS=<dir> W3_SHOW_06=<copy of DanceHysteria-0.6.qlcc>
     *  W3_SHOW_01=<copy of DanceHysteria-0.1.qlcc>: real-cue pictures. */
    void shots();

private:
    quint32 dimmer(int level, const QString &name);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif

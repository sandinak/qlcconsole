/*
  Q Light Controller Plus - Test Unit
  selectionsync_test.h

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

#ifndef SELECTIONSYNC_TEST_H
#define SELECTIONSYNC_TEST_H

#include <QObject>
#include <QList>

class Doc;

/* One selection (Q201): fixtures picked in the Lighting Studio, on the 2D
   plot or in the Fixture Manager are the programmer selection, and the
   Workbench, the plot and the Studio follow it; Selection ▸ New Layer from
   Selection / Add to Current Layer. */
class SelectionSync_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** A Studio pick is the programmer selection; the Workbench follows it
     *  with the Build tab hidden. */
    void studioPickWorkbenchFollowsHidden();
    /** A plot pick rings in the Studio; a Studio pick rings on the plot. */
    void plotAndStudioSync();
    /** A structure's editor rings only what is mounted on it. */
    void structureEditorRingsItsOwn();
    /** Fixture Manager rows pick (a group row: its fixtures); a deselect
     *  or a hidden tree doesn't. */
    void fixtureManagerPicks();
    /** One change, one signal: nothing writes back what it follows. */
    void noSignalLoop();
    /** New Layer from Selection in a collection: one undo step. */
    void newLayerFunctionStack();
    /** Add to Current Layer on a scene: one undo step. */
    void addToLayerFunctionStack();
    /** Both in a Show Tree cue: its own layer, the tree's undo. */
    void newLayerTreeCueStack();
    /** The submenu: its items, why one is disabled, the extras. */
    void selectionMenu();
    /** The plot's menu: Selection ▸ with Create Fixture Group, and the map
     *  layer action renamed. */
    void plotMenu();

    /** One group exactly: that group; a part of one: discrete. */
    void exactGroup();
    /** Two whole groups: both; a group and a stray; a parent wins. */
    void twoGroupsAndStray();
    /** Equal groups: the one used most in the show. */
    void mostUsedWins();
    /** Group minus one, band just short, one stray; and no offer. */
    void nearMissHints();
    /** The chip: offered only; Esc, Tab, a click, another pick. */
    void hintChip();
    /** The plot's band: the miss outlined, the selection untouched. */
    void plotBandOutlinesMiss();

private:
    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif

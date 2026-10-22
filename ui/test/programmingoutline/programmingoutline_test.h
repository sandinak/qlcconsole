/*
  Q Light Controller Plus - Test Unit
  programmingoutline_test.h

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

#ifndef PROGRAMMINGOUTLINE_TEST_H
#define PROGRAMMINGOUTLINE_TEST_H

#include <QObject>

class Doc;
class Scene;
class QLCPalette;
class FixtureGroup;
class Fixture;

/* The Programming tab's outline (PROGRAMMING_REDESIGN.md P1). */
class ProgrammingOutline_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** Folders -> scenes -> targets and looks, an effect's own colour under
     *  it; a chaser in a scene folder greyed; nothing yet: one line saying so;
     *  the line over the outline says what it is. */
    void everySceneAndWhatIsInItIsARow();
    /** Badges: ⚠ with what is wrong on hover, ▣ raw values, ⇄ a transition,
     *  ⏱ a speed master; a look's own fade. */
    void badgesSayWhatAndHoverSaysWhy();
    /** A click fills the page: a scene its timing, a look its editor, a
     *  target its channels; currentSceneId()/currentPaletteId() follow. */
    void aClickedRowFillsThePage();
    /** Rows drag between scenes: move by default, ⌥ copies; a look keeps
     *  its fade, a fixed target its raw values, an effect its own colour;
     *  within a scene a drop reorders. The hint says it before the drop. */
    void rowsMoveBetweenScenesAndOptionCopies();
    /** Palettes, groups and fixtures dropped from the sources: a base look,
     *  an effect's own when dropped on the effect, targets. */
    void sourcesDropOntoAScene();
    /** Every edit is one step on the shared history, undone and redone
     *  from either tab: outline moves, renames, removals, the page's timing
     *  and a fader drag (one step). */
    void editsAreStepsOnTheSharedHistory();
    /** Expand all / Collapse all, ⌘→ / ⌘←, ⌥-click a triangle; what was
     *  left open is open again (per show). */
    void expandCollapseAndOptionClick();
    /** Filter, multi-select across scenes and ⌫. */
    void filterAndRemoveSeveral();
    /** QLC_OUTLINE_SHOW (a .qlcc, read only, in memory): every scene once,
     *  every look of each; QLC_OUTLINE_SHOT: pictures of the tab. */
    void aRealShow();

private:
    Scene *scene(const QString &name, const QString &path);
    QLCPalette *palette(int type, const QString &name);
    FixtureGroup *group(const QString &name, int count);

    Doc *m_doc = nullptr;
};

#endif

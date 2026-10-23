/*
  Q Light Controller Plus - Test Unit
  workbenchchaser_test.h

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

#ifndef WORKBENCHCHASER_TEST_H
#define WORKBENCHCHASER_TEST_H

#include <QHash>
#include <QObject>

class Doc;
class Scene;
class Chaser;
class Collection;
class FixtureGroup;
class QTemporaryDir;

/* Chasers in the Workbench and Build ▸ Scenes ▸ Classic
   (WORKFLOW_DESIGN.md §3.3, §3.4, §3.7; W2). */
class WorkbenchChaser_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** ◀ ▶ and the step table walk a chaser: each step's function opened
     *  up as layers, the header saying which step; Open and ↩ come back to
     *  the same step; the step being worked on is what previews. */
    void walkingAChasersSteps();
    /** The step header (fade in / hold / fade out / cue / note) edits the
     *  step, one step each on the shared history, as the Chaser editor's
     *  timing modes allow. Steps added, moved, removed: the same. */
    void theStepHeaderEditsTheStep();
    /** Q60: a time typed in the header goes to the step it was typed for
     *  when the walker moves on (◀ ▶, the table); Esc puts it back. */
    void typedStepTimesGoToTheirStep();
    /** A step's note opens up while it is typed in -- the walker's header,
     *  its step table, the Chaser editor's Notes column: it grows to fit,
     *  Return commits (one step on the history), Shift+Return starts a new
     *  line, Esc puts it back; closed up it is elided, all of it in the
     *  tooltip. */
    void theNoteOpensUpWhileTyped();
    /** A layer added to a collection inside a chaser step changes what the
     *  chaser itself puts out; Undo takes it back, Redo again. */
    void editingAStepsCollectionChangesTheChasersOutput();
    /** Chaser from selection, in the order the scenes were picked; scenes
     *  dropped on a chaser (navigator, step table) are appended as steps. */
    void chaserFromSelectionAndDropsOnAChaser();
    /** A shared layer, and a shared step, split off with Make own copy:
     *  one undo step, the other users untouched. */
    void makeOwnCopySplitsASharedMember();
    /** Classic lists every function the Function Manager does, by type,
     *  with its folders; Folders vs Classic per show. */
    void classicListsWhatTheFunctionManagerDoes();
    /** DanceHysteria-0.1 (a copy): Full Show walked step by step, a step's
     *  collection edited and undone, Classic by default; screenshots. */
    void danceHysteriaFullShow();
    /** Sample.qxw (a copy): every stock chaser walks; Classic matches the
     *  Function Manager. */
    void sampleShowsChasers();
    /** The Functions window is retired (board Q32): everything it did is in
     *  Build ▸ Scenes -- New function ▸ every type (⌘1…⌘9), Function
     *  Wizard…, Startup function…, Delete folder; each new one filed in the
     *  current row's folder and loaded in the Workbench. */
    void functionsWindowToolsLiveInBuild();
    /** A sequence, root or step, edits in the Workbench: its steps over its
     *  bound scene's channels, wired as the Functions window wired them. */
    void aSequenceEditsInTheWorkbench();
    /** ProgrammingManager::openFunction(): from anywhere, Build's Workbench
     *  (Classic when Folders doesn't list it); Edit Chase… on a step loads
     *  the chase in the Workbench the editor is in, ↩ back. */
    void openingAFunctionLandsInTheWorkbench();

private:
    Scene *scene(const QString &name, const QString &path = QString());
    Collection *collection(const QString &name, const QList<quint32> &members);
    FixtureGroup *group(const QString &name, int count);
    /** @a ticks of the engine, then universe 1's first @a channels values. */
    QList<int> settle(int ticks, int channels);
    /** Load a copy of @a path into a fresh Doc (in @a dir). */
    bool loadCopy(const QString &path, QTemporaryDir &dir);

    Doc *m_doc = nullptr;
};

#endif

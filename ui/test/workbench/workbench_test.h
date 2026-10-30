/*
  Q Light Controller Plus - Test Unit
  workbench_test.h

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

#ifndef WORKBENCH_TEST_H
#define WORKBENCH_TEST_H

#include <QObject>

class Doc;
class Scene;
class QLCPalette;
class FixtureGroup;
class Collection;

/* The Build tab's Workbench (docs/design/WORKFLOW_DESIGN.md §3.4, W1) with its
   FunctionStack back end. */
class Workbench_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** A scene loads as one layer, opened up; the header says what and
     *  where; nothing is added to a lone scene. */
    void aSceneIsOneLayer();
    /** A collection's members are its layers: a scene opens up into its
     *  targets and looks, anything else is a ▣ row; the overview says what
     *  it is; a ▣ row's stock editor fills the editor pane. */
    void aCollectionsMembersAreLayers();
    /** Open on a ▣ row loads it; ↩ comes back, the row selected again. */
    void openAndBackKeepTheSelection();
    /** + Scene (existing), ⌫, ▲ ▼ and a scene dragged from Scenes onto the
     *  layers: each one step on the shared history, undone and redone from
     *  either tab. The function stays when its layer goes. */
    void layersAddRemoveReorderOnTheSharedHistory();
    /** + EFX / + RGB Matrix: a new function on the selected layer's groups,
     *  added after it, selected, its editor open; Undo takes the function
     *  away too, Redo brings it back with its id. */
    void newEffectsAreMadeOnTheSelectedLayer();
    /** ⇧⌘N: a collection loaded, a new scene layer (named in place); a
     *  chaser, a new scene as the step after the one walked to, walked to;
     *  each one undo step. A lone scene takes nothing. */
    void newHereMakesAStepOrALayer();
    /** A member used elsewhere says so: "also in N", where on hover. */
    void sharedMembersSayWhereElse();
    /** Design mode: what is loaded plays; a changed collection plays its new
     *  layers; Stop / Play; hidden, nothing plays. */
    void whatIsLoadedPlays();
    /** RC8 u5: a GO from anywhere holds the Build preview -- it stops, the
     *  header says Held, a load keeps it off while the show runs -- and
     *  Play previews over the show. */
    void goHoldsTheBuildPreview();
    void aPickTakesTheStageBackFromAGo();
    /** Operate mode: what is loaded runs as the show's output; to Design,
     *  a function started elsewhere keeps running; hidden, the Workbench
     *  stops only what it started. */
    void operateRunsWhatIsLoadedAndStopsOnlyItsOwn();
    void designPreviewLeavesAnotherRunAlone();
    /** RC8: a preview plays a look, never a Script in it -- a collection's
     *  member or a chaser's step -- which would act on the world (set
     *  channels, start things, run programs). Started from anywhere else
     *  the same Script runs. */
    void aPreviewNeverRunsAScript();
    /** Two layers set one channel: the lower layer wins on the DMX; ▲
     *  swaps them and the output follows; ⌘Z puts it back. */
    void theLowerLayerWinsAndFollowsAReorder();
    /** The navigator: a look in a scene opened up there selects that row on
     *  the Workbench; the Workbench follows; the section switch; control
     *  surfaces' currentSceneId()/currentPaletteId(). */
    void theNavigatorDrivesTheWorkbench();
    /** QLC_WORKBENCH_SHOW (a .qlcc / .qxw, read from a copy, in memory):
     *  every collection loads with its members as layers, nothing
     *  converted; QLC_WORKBENCH_SHOT: pictures of Build ▸ Scenes with a
     *  scene and a collection loaded. */
    void aRealShowsCollectionsLoadAsLayers();

private:
    Scene *scene(const QString &name, const QString &path);
    QLCPalette *palette(int type, const QString &name);
    FixtureGroup *group(const QString &name, int count);
    Collection *collection(const QString &name, const QList<quint32> &members);

    Doc *m_doc = nullptr;
};

#endif

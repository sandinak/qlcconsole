/*
  Q Light Controller Plus - Test Unit
  effectdesigner_test.h

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

#ifndef EFFECTDESIGNER_TEST_H
#define EFFECTDESIGNER_TEST_H

#include <QTemporaryDir>
#include <QObject>

class FixtureGroup;
class Doc;

class EffectDesigner_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /** In the Show Tree's designer an effect's Front and Behind colours are
     *  its own Colour looks: the colour line and the swatches work there too
     *  (no Build scene), and the designer hands back both. */
    void theDesignerHasFrontAndBehindSwatches();
    /** Front and Behind set on an effect layer in a Show Tree cue are kept,
     *  saved with the show, loaded, found again when the layer is reopened
     *  (both in the designer's Its own row), and play front then backfill. */
    void frontAndBehindSurviveSaveAndLoad();

    /** The bench: a new effect is a hidden draft played by a hidden scene
     *  on the Try on groups; Save as palette names and files it, and the
     *  preview goes. */
    void aNewEffectOnTheBenchSavesAsAPalette();
    /** Leaving without an answer leaves the show as it was: no palette, no
     *  scene, not even "modified". */
    void cancelLeavesNoTrace();
    /** The designer says where a save goes ("Save in this show…") and has
     *  Runs: Loop | Once | N times for whatever effect is picked. */
    void theDesignerSetsHowAnEffectRuns();
    /** The preview plays in Design mode and on screen only. */
    void thePreviewPlaysOnlyInDesignOnScreen();
    /** Design effect on a Show Tree layer, Keep on this layer: a one-off
     *  Effect look on the layer, one undo step, played by its scene. */
    void keepOnALayerMakesAOneOff();
    /** Editing the layer's own one-off again writes into it. */
    void aLayersOneOffIsEditedInPlace();
    /** Starting from a named palette designs a copy: the palette changes
     *  only with Update, and then everywhere. */
    void saveAsPaletteAsksNoName();
    void anEffectPlayingTakesEditsLive();
    void aNamedEffectIsACopyUntilUpdated();
    /** An effect of another kind replaces the layer's old one. */
    void anotherKindGoesBesideTheLayersEffect();
    /** The effect's own colour goes into the look, after the effect. */
    void theEffectsOwnColourIsKept();
    /** The Show Tree opens it: Design Effect… in a layer's right-click
     *  menu and in its Effect row's ⋯ menu open it in the DOCKED
     *  WORKBENCH's editor pane (board Q263: no modal window), on that
     *  layer's fixtures, and what is kept lands there. */
    void theShowTreeOpensItOnALayer();
    /** Board Q263: + Effect ▾ → Effect Palette… on a cue layer designs
     *  THAT layer's effect in the pane -- what its tooltip always said --
     *  with the cue still loaded behind it, and what is kept lands there. */
    void theAddEffectBarDesignsOnTheSelectedLayer();
    /** Board Q263: a draft nobody kept is never dropped in silence --
     *  clicking another row asks, and Keep designing puts the selection
     *  back. */
    void leavingADesignerAsksFirst();
    /** Board Q264: the Library's + Palette ▸ Effect and Edit Effect open
     *  the designer (the bench, on the cue's groups; the palette it starts
     *  from); every other palette type stays the Look Editor's. */
    void theLibrarysEffectDoorsOpenTheDesigner();
    /** Pictures for the UX review (QLC_SHOT_DIR), at three window sizes:
     *  a cue's layer in the docked Workbench, its Effect row (the designer
     *  -- the Look Editor before board Q263), and the Library's
     *  + Palette ▸ Effect. Driven through calls that exist either side of
     *  the change, so the same test takes the before and the after. */
    void designerShots();
    /** Q77: no Save as preset -- a palette is the effect kept, Export…
     *  carries it to another show. */
    void thereIsNoSaveAsPreset();
    /** Window ▸ Effects: effect palettes in folders, classic effects marked,
     *  one-offs left out; + New EFX on the Try on groups opens its editor. */
    void theEffectsPageListsEveryEffect();
    /** "in build how does one generate new effects? I don't see an add or
     *  right click when effects are subselected .. only palettes": Build ▸
     *  Effects has + New and a right-click menu, and designs an effect
     *  palette in the Workbench. */
    void buildEffectsMakesAndDesignsEffects();
    /** Esc on the bench: nothing unkept, it leaves at once, back to the
     *  scene it was opened from; an effect designed and not kept, it asks
     *  first (Keep designing / Leave). Clicking away doesn't ask. */
    void escLeavesTheBench();

    /** Build ▸ Effects' right-click Duplicate, Rename, Move and Delete run
     *  on an effect palette and on an EFX: each changes the show and is one
     *  undo step; Delete's question names where the effect is used. */
    void buildEffectsRightClickActionsChangeTheShow();
    /** ...and on an RGB Matrix: Duplicate and Delete are one undo step
     *  each, and undo/redo bring it back with its own id. */
    void buildEffectsRightClickOnAnRgbMatrix();

    /** Make a layer (board Q37), into a Show Tree cue: a saved, unchanged
     *  palette plays by name; a changed design is a one-off copy (the bench
     *  keeps its own). One tree undo step each; undo and redo restore. */
    void makeALayerIntoACue();
    /** Into a plain Build scene: the Try on groups and the effect added,
     *  one undo step; a one-off survives a bake while undo can put it back. */
    void makeALayerIntoAScene();
    /** Into a new scene: made with the groups and the effect, one step. */
    void makeALayerIntoANewScene();
    /** From Build: the bench opened over a loaded scene goes back into it
     *  (the button), the scene loaded with its new look selected; the
     *  Show Tree's selected cue is in the menu, and its new layer is
     *  selected in the tree. */
    void makeALayerFromTheWorkbench();

    /** QLC_EFFECTDESIGNER_SHOW=<a COPY of a show> QLC_EFFECTDESIGNER_SHOTS=<dir>:
     *  pictures of the designer on a real layer and on the bench. */
    void picturesOfARealShow();

private:
    FixtureGroup *group(const QString &name, int from, int count);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
    QTemporaryDir m_home;
};

#endif

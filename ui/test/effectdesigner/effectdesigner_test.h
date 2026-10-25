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

    /** The bench: a new effect is a hidden draft played by a hidden scene
     *  on the Try on groups; Save as palette names and files it, and the
     *  preview goes. */
    void aNewEffectOnTheBenchSavesAsAPalette();
    /** Leaving without an answer leaves the show as it was: no palette, no
     *  scene, not even "modified". */
    void cancelLeavesNoTrace();
    /** The preview plays in Design mode and on screen only. */
    void thePreviewPlaysOnlyInDesignOnScreen();
    /** Design effect on a Show Tree layer, Keep on this layer: a one-off
     *  Effect look on the layer, one undo step, played by its scene. */
    void keepOnALayerMakesAOneOff();
    /** Editing the layer's own one-off again writes into it. */
    void aLayersOneOffIsEditedInPlace();
    /** Starting from a named palette designs a copy: the palette changes
     *  only with Update, and then everywhere. */
    void aNamedEffectIsACopyUntilUpdated();
    /** An effect of another kind replaces the layer's old one. */
    void anotherKindReplacesTheLayersEffect();
    /** The effect's own colour goes into the look, after the effect. */
    void theEffectsOwnColourIsKept();
    /** The Show Tree opens it: Design effect… in a layer's right-click
     *  menu and in its Effect row's ⋯ menu, and what is kept lands there. */
    void theShowTreeOpensItOnALayer();
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

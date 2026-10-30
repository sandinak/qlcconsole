/*
  Q Light Controller Plus
  treecuestack.h

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

#ifndef TREECUESTACK_H
#define TREECUESTACK_H

#include <QPointer>

#include "workbenchstack.h"
#include "showtreemodel.h"

class ShowTreeController;
class ShowTreeView;

/** @addtogroup ui_functions
 * @{
 */

/**
 * The TreeCueStack back end (docs/design/WORKFLOW_DESIGN.md §3.4, W3): a Show Tree cue (or
 * a look of a Looks section, or a sub-cue) in the Workbench.
 *
 * - Its rows are what is on stage in the cue, in order: its own layers and,
 *   grey, the layers carried in (tracked, included, back to the base) with
 *   where they come from; a layer opens up into one row per attribute, an
 *   inherited value grey with its origin. A layer that plays a function as
 *   it is (▣) is a function row: it opens in the Workbench.
 * - Every edit is one step of the tree's (ShowTreeController), then a
 *   re-bake: the generated functions stay locked (D13). Drops and the
 *   attribute menus are the tree's own (ShowTreeView); the editor pane
 *   borrows the tree's cue and layer pages.
 * - It plays itself: the cue in context through the tree's preview (what
 *   tracks in too, over a list running for real), or Solo, only the
 *   selected layer's scene.
 *
 * No file format of its own: it shows the tree and calls its operations.
 */
class TreeCueStack final : public WorkbenchStack
{
    Q_OBJECT

public:
    TreeCueStack(Doc *doc, ShowTreeView *view, quint32 cueId, QObject *parent = nullptr);
    ~TreeCueStack();

    quint32 cueId() const { return m_cue; }
    /** Layer @a index's id in the cue (invalid: carried in) and target. */
    quint32 layerIdAt(int index) const;
    ShowTree::Target targetAt(int index) const;
    /** The row of the cue's layer @a layerId, or of the layer carried in on
     *  @a signature (-1: none). */
    int indexOfLayer(quint32 layerId, const QString &signature = QString()) const;

    QString key() const override;
    WorkbenchStack *reopen(QObject *parent) const override;
    QString title() const override;
    QString titleColour() const override;
    QString kindText() const override;
    QString where() const override;
    QStringList infoLines() const override;
    quint32 previewFunction() const override;
    QList<Layer> layers() const override;

    bool canAddLayers() const override;
    bool canRemoveLayers() const override;
    bool canReorderLayers() const override;
    QString addPlan(const QList<quint32> &functionIds) const override;
    bool addLayers(const QList<quint32> &functionIds, int at) override;
    bool removeLayers(const QList<int> &indexes) override;
    bool moveLayer(int from, int to) override;
    quint32 addNew(Function::Type type, int after, const QList<GroupHead> &heads,
                   quint32 group) override;

    bool takesDrops() const override { return true; }
    QString dropPlanOn(int layer, const QMimeData *mime, bool separate) const override;
    bool dropOn(int layer, const QMimeData *mime, bool separate) override;
    /** Look rows dragged onto layer @a layer (attributeMimeType()): what it
     *  does, "!why" when it can't. */
    QString attributeDropPlan(int layer, const QMimeData *mime, bool copy) const;
    /** Move (copy) them there: one undo step. */
    bool dropAttributes(int layer, const QMimeData *mime, bool copy);
    /** Layer rows dragged onto layer row @a layer (layerMimeType()): they
     *  move above it (reorder), "!why" when they can't. */
    QString layerDropPlan(int layer, const QMimeData *mime) const;
    bool dropLayerRows(int layer, const QMimeData *mime);
    bool showMenu(int layer, int attribute, const QPoint &globalPos) override;
    bool clearAttribute(int layer, int attribute) override;
    QWidget *lendPane(int layer, int attribute) override;
    void returnPane(QWidget *pane) override;
    QList<quint32> layerFixtures(int layer) const override;
    int layerOfFixture(quint32 fixtureId) const override;
    quint32 layerScene(int layer) const override;

    bool playsItself() const override { return true; }
    void startOwnPreview() override;
    void stopOwnPreview() override;
    QString ownPreviewText() const override;
    bool ownPreviewHeld() const override;
    void releaseRunHold() override;
    bool canSolo() const override { return true; }
    bool solo() const override { return m_solo; }
    void setSolo(bool solo) override;
    void setFocusLayer(int layer) override;
    /** The scene Solo plays now (invalid: none). */
    quint32 soloScene() const { return m_soloScene; }

private:
    struct Row
    {
        quint32 layerId = 0xFFFFFFFF;   //!< invalid: carried in
        ShowTree::Target target;
    };
    /** What is on stage in the cue, in order, as rows (the same order as
     *  layers()); @a resolved: that cue resolved. */
    QList<Row> rows(ShowTree::ResolvedCue *resolved = nullptr) const;
    const ShowTree::CueList *list() const;
    const ShowTree::Cue *cue() const;
    ShowTreeController *st() const;
    /** Play the Solo layer's scene (and only it), or stop it. */
    void playSolo();
    void stopSolo();

    QPointer<ShowTreeView> m_view;
    quint32 m_cue;
    bool m_solo = false;
    bool m_playing = false;
    int m_focus = -1;
    QString m_focusSignature;           //!< the selected layer, by its target
    /** The selected layer's place now (-1: none). */
    int focusIndex() const;
    quint32 m_soloScene = 0xFFFFFFFF;
};

/** @} */

#endif

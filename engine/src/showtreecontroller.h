/*
  Q Light Controller Plus
  showtreecontroller.h

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

#ifndef SHOWTREECONTROLLER_H
#define SHOWTREECONTROLLER_H

#include <functional>
#include <QStringList>
#include <QObject>
#include <QHash>
#include <QSet>

#include "qlcengine_export.h"
#include "functionparent.h"
#include "showtreemodel.h"
#include "qlcpalette.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class QTimer;
class Scene;
class Doc;

#define KXMLQLCShowTree QStringLiteral("ShowTree")

/** @addtogroup engine Engine
 * @{
 */

/**
 * The Show Tree's productions, and the functions they bake to
 * (SHOW_TREE_DESIGN.md §4): a cue list is a Chaser, a cue a Collection
 * holding its complete output, a resolved layer a generated Scene --
 * one per distinct layer, shared by every cue where it is the same, so it
 * plays on across GO (ChaserRunner's hand-over).
 *
 * Every edit goes through edit(): one undo step, then a re-bake. A bake is
 * deterministic from the tree, so undo is "put the tree back and bake".
 *
 * Owned by Doc (Doc::showTree()); Doc only forwards load/save/clear.
 */
class QLC_ENGINE_EXPORT ShowTreeController final : public QObject
{
    Q_OBJECT

public:
    explicit ShowTreeController(Doc *doc);
    ~ShowTreeController();

    /************************************************************************
     * The tree
     ************************************************************************/
public:
    const QList<ShowTree::Production> &productions() const { return m_productions; }
    const ShowTree::Production *production(quint32 id) const;
    /** The production and list a list id belongs to (null: none). */
    const ShowTree::CueList *cueList(quint32 listId, quint32 *productionId = nullptr) const;
    /** The list a cue belongs to. */
    const ShowTree::CueList *listOfCue(quint32 cueId) const;
    const ShowTree::Cue *cue(quint32 cueId) const;

    /** A new node id (production, list, cue, layer): unique in the show. */
    quint32 newId();

    /************************************************************************
     * Editing: each is one undo step, and bakes
     ************************************************************************/
public:
    /** Run @a change on the tree as one undo step named @a label, then bake.
     *  @a change returns false to leave everything as it was. */
    bool edit(const QString &label, const std::function<bool(QList<ShowTree::Production> &)> &change);

    quint32 addProduction(const QString &name);
    quint32 addCueList(quint32 productionId, const QString &name,
                       ShowTree::ListMode mode = ShowTree::ListMode::NonTracking);
    /** A new cue after @a afterIndex (-1: at the top), numbered per D23. */
    quint32 addCue(quint32 listId, int afterIndex, const QString &name);
    bool removeCue(quint32 cueId);
    bool moveCue(quint32 cueId, int toIndex);
    bool setCue(const ShowTree::Cue &cue);   //!< its own fields; layers and looks kept as given
    quint32 addLayer(quint32 cueId, const ShowTree::Target &target, const QString &name = QString());
    bool removeLayer(quint32 cueId, quint32 layerId);
    bool setLayerLook(quint32 cueId, quint32 layerId, const ShowTree::Look &look);
    bool setCueLook(quint32 cueId, const ShowTree::Look &look);
    /** Set a look on the cue's own layer for @a target -- made one first if
     *  the layer is only carried in (tracked, included): one undo step. */
    bool setTargetLook(quint32 cueId, const ShowTree::Target &target, const ShowTree::Look &look);
    bool setLayerRelease(quint32 cueId, quint32 layerId, bool release);
    /** Give a layer another target (groups and fixtures): one undo step.
     *  The layer stays the same layer -- a layer it carried on from (tracked,
     *  included) keeps giving it its values, and the same layer set again in
     *  the following cues it tracks into is changed with it, so nothing
     *  lights the old target beside the new one. */
    bool setLayerTarget(quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                        const QString &label = QString());
    /** Add groups / fixtures to a layer's target (dropped onto it). */
    bool addToLayer(quint32 cueId, quint32 layerId, const QList<quint32> &groups,
                    const QList<quint32> &fixtures);
    /** Take a group (or a fixture) out of a layer's target; never its last. */
    bool removeGroupFromLayer(quint32 cueId, quint32 layerId, quint32 groupId);
    bool removeFixtureFromLayer(quint32 cueId, quint32 layerId, quint32 fixtureId);
    bool setListMode(quint32 listId, ShowTree::ListMode mode);
    bool setBaseCue(quint32 listId, quint32 cueId);
    bool renameNode(quint32 nodeId, const QString &name);
    bool removeCueList(quint32 listId);
    bool removeProduction(quint32 productionId);

    /************************************************************************
     * Looks and palettes
     ************************************************************************/
public:
    /** The attribute key a palette sets ("Colour", "Effect.Color"...). */
    QString keyForPalette(quint32 paletteId) const;
    /** An effect palette's kind: "Color", "Dimmer", "Position", "Beam" or
     *  "Other", from its script's fixture types. */
    QString effectKind(const QLCPalette *palette) const;
    /** A look that sets @a paletteId on its attribute. */
    ShowTree::Look lookForPalette(quint32 paletteId) const;

    /** A one-off value (D11, D21): an unnamed palette owned by the tree,
     *  hidden from the palette tree. Returns its id. */
    quint32 createInlinePalette(QLCPalette::PaletteType type, const QVariantList &values,
                                const QString &label);
    bool isInlinePalette(quint32 paletteId) const { return m_inlinePalettes.contains(paletteId); }
    /** Make an inline palette an ordinary named one (Make reusable). */
    bool makeReusable(quint32 paletteId, const QString &name, const QString &path);

    /** How many of @a target's fixtures a look of @a key can reach ("applies
     *  to 4 of 8 fixtures", §3A): @a reach of @a total. */
    void lookReach(const ShowTree::Target &target, const QString &key, int *reach, int *total) const;
    /** The target's fixtures a look of @a key can't reach (they ignore it). */
    QList<quint32> lookMisses(const ShowTree::Target &target, const QString &key) const;

    /** What a person reads for @a target: its groups' and fixtures' names,
     *  or the function's. */
    QString targetLabel(const ShowTree::Target &target) const;
    /** "0.0.4 SHOUT" */
    QString cueLabel(const ShowTree::CueList &list, int index) const;

    /************************************************************************
     * Undo
     ************************************************************************/
public:
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString undoText() const;
    QString redoText() const;
    bool undo();
    bool redo();
    void clearUndo();

private:
    struct Snapshot
    {
        QString label;
        QByteArray xml;
    };
    QByteArray snapshotXml() const;
    void restoreSnapshot(const QByteArray &xml);

    QList<Snapshot> m_undo;
    QList<Snapshot> m_redo;
    static const int kUndoLimit = 200;

    /************************************************************************
     * Baking
     ************************************************************************/
public:
    /** Bake every production into functions; drop what's no longer used.
     *  Deterministic: the same tree always gives the same functions. */
    void bake();

    /** Whether @a functionId was generated by the tree (and so is locked,
     *  D13). */
    bool isGenerated(quint32 functionId) const;
    /** For a generated function: the cue (or list) it plays, "Cue 0.0.4
     *  SHOUT in Full Show"; empty otherwise. Delete/usage dialogs name cues,
     *  not generated scenes. */
    QString describeGenerated(quint32 functionId) const;

    /** The ordered palette list a resolved layer's scene plays (the look
     *  stack: base looks, then effects each with its own). */
    static QList<quint32> scenePalettes(const ShowTree::ResolvedLayer &layer);

    /** Detach a generated scene: a plain copy the tree then plays as an
     *  opaque function layer in @a cueId in place of @a layerId (D13). */
    quint32 detachLayer(quint32 cueId, quint32 layerId);

private:
    QString sceneSignature(const Scene *scene) const;
    static QString sceneSignature(const ShowTree::Target &target, const QList<quint32> &palettes);
    void deleteUnused(const QSet<quint32> &keep, QSet<quint32> &generated);
    void dropUnusedInlinePalettes();

    QSet<quint32> m_generatedScenes;
    QSet<quint32> m_generatedCollections;
    QSet<quint32> m_generatedChasers;
    QSet<quint32> m_inlinePalettes;
    /** What a generated function plays, for describeGenerated(). */
    QHash<quint32, QString> m_generatedLabels;

    /************************************************************************
     * Preview (Design mode): the selected cue on the rig
     ************************************************************************/
public:
    /** Why a wanted preview is (not) on the rig. */
    enum class PreviewState
    {
        Off,            //!< nothing wanted
        Playing,        //!< the wanted cue plays
        NotDesign,      //!< Live (Operate) mode: edits are blind until GO
        ListRunning,    //!< its cue list runs for real: not competed with
        Dark            //!< the cue plays nothing
    };

    /** Play cue @a cueId on the rig as a preview, as the Programming tab
     *  does with its canvas: Design mode only, and never while the cue's own
     *  list is running for real (it would fight the show). The cue's members
     *  (its generated scenes and opaque functions) run under previewParent(),
     *  so a scene two previewed cues share keeps running when the preview
     *  moves on, and every bake re-takes the preview at once: an edit shows
     *  on the rig immediately. Another cue switches the preview. */
    void previewCue(quint32 cueId);
    /** Stop the preview (leaving the tab, nothing selected). */
    void stopPreview();
    /** The cue the preview was asked for (invalid: none). */
    quint32 previewedCue() const { return m_previewCue; }
    PreviewState previewState() const { return m_previewState; }
    /** What the preview runs now. */
    QList<quint32> previewFunctions() const { return m_previewRunning; }
    /** The parent preview functions run under: stopping it never stops
     *  anything a chaser, a VC widget or another tab started. */
    static FunctionParent previewParent();

signals:
    /** The preview started, stopped or moved to another cue. */
    void previewChanged();

private slots:
    void slotModeChanged();
    void slotTimerFunctionStarted(quint32 fid);
    void slotTimerFunctionStopped(quint32 fid);

private:
    /** Bring what runs in line with the wanted cue (after a bake, a mode
     *  change, the list starting or stopping). */
    void syncPreview();

    quint32 m_previewCue = 0xFFFFFFFF;
    PreviewState m_previewState = PreviewState::Off;
    QList<quint32> m_previewRunning;
    /** Sweeps, by a bake, the generated scenes the preview let go of. */
    QTimer *m_sweepTimer = nullptr;

    /************************************************************************
     * Import (§7)
     ************************************************************************/
public:
    struct ImportReport
    {
        quint32 production = 0xFFFFFFFF;
        quint32 list = 0xFFFFFFFF;
        int cues = 0;
        int layers = 0;             //!< scenes read into layers
        int opaqueLayers = 0;       //!< functions kept as they are
        int includes = 0;
        int generatedScenes = 0;    //!< distinct scenes the list bakes to
        QStringList opaqueReasons;  //!< "BackStage: has raw channel values"
        /** Cues where two scenes set the same target: the chaser ran both
         *  (intensity HTP); the tree keeps the later one's values. */
        QStringList htpConflicts;
        QStringList notes;
    };

    /** Import @a chaserId as a cue list in a new production (D12): each
     *  step a cue, each scene of targets + palettes a layer, a collection
     *  that is another step's an include, anything else an opaque function
     *  layer. Non-tracking, so it plays as the chaser did. The original is
     *  untouched. One undo step. */
    ImportReport importChaser(quint32 chaserId);

    /** Read @a scene as a layer; false (and @a why) when it can't be read
     *  without changing what it plays. */
    bool sceneToLayer(const Scene *scene, ShowTree::Layer *layer, QString *why) const;

    /** Whether two look stacks play the same: the same colours in the same
     *  order, the same other static looks, the same effects in the same
     *  order each with the same own looks. */
    bool playsTheSame(const QList<quint32> &a, const QList<quint32> &b) const;

    /************************************************************************
     * Load & save
     ************************************************************************/
public:
    bool loadXML(QXmlStreamReader &root);
    void saveXML(QXmlStreamWriter *doc) const;
    void reset();

private slots:
    void slotFunctionRemoved(quint32 fid);
    void slotPaletteRemoved(quint32 pid);

signals:
    /** The tree changed (an edit, undo, load). */
    void changed();

private:
    void loadState(QXmlStreamReader &root, bool snapshot);
    void saveState(QXmlStreamWriter *doc, bool snapshot) const;

    ShowTree::Cue *findCue(quint32 cueId, ShowTree::CueList **list = nullptr);

    Doc *m_doc;
    QList<ShowTree::Production> m_productions;
    quint32 m_nextId = 1;
    bool m_baking = false;
};

/** @} */

#endif

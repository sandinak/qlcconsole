/*
  Q Light Controller Plus
  showtreeview.h

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

#ifndef SHOWTREEVIEW_H
#define SHOWTREEVIEW_H

#include <functional>
#include <QWidget>
#include <QPair>
#include <QList>
#include <QSet>

#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "qlcpalette.h"

class QStackedWidget;
class QListWidget;
class ShowTreeCommand;
class QDoubleSpinBox;
class QTreeWidgetItem;
class LookEditor;
class QTreeWidget;
class QPushButton;
class QToolButton;
class QGridLayout;
class QComboBox;
class QLineEdit;
class QMimeData;
class QTimer;
class QCheckBox;
class QLabel;
class QMenu;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The Show Tree tab (SHOW_TREE_DESIGN.md §6): the tree of productions, cue
 * lists, cues and layers on the left; the selected cue or layer in the
 * middle (a layer as one row per attribute, inherited values greyed with
 * where they come from); the Library on the right (Recent, Pinned, All),
 * everything in it draggable onto a cue or a layer.
 *
 * Every edit goes through the engine's ShowTreeController (one undo step
 * each, then a bake); this view only shows the tree and turns clicks and
 * drops into those calls.
 */
class ShowTreeView final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(ShowTreeView)

public:
    ShowTreeView(QWidget *parent, Doc *doc);
    ~ShowTreeView();

    enum NodeKind
    {
        NoNode = 0,
        ProductionNode,
        ListNode,
        CueNode,
        LayerNode,          //!< a cue's own layer
        TrackedLayerNode    //!< carried into the cue (tracked, included, base)
    };

    /** Item roles in the tree and the Library. */
    enum
    {
        KindRole = Qt::UserRole,
        IdRole = Qt::UserRole + 1,
        CueRole = Qt::UserRole + 2,     //!< a layer row's cue
        TargetRole = Qt::UserRole + 3,  //!< a tracked layer's target signature
        PathRole = Qt::UserRole + 4     //!< a Library folder's path ("Groups/Movers/Front")
    };

    /** Library item kinds. */
    enum LibraryKind { LibSection = 100, LibFolder, LibGroup, LibFixture, LibPalette, LibFunction, LibSet };

    QTreeWidget *tree() const { return m_tree; }
    QTreeWidget *library() const { return m_library; }

    /** Select the tree row of node @a id (a layer: in cue @a cueId). */
    bool selectNode(quint32 id, quint32 cueId = ShowTree::invalidId());
    NodeKind currentKind() const;
    quint32 currentId() const;
    quint32 currentCueId() const;

    /************************************************************************
     * What the mouse does (drops, pickers) -- public for the tests
     ************************************************************************/
public:
    /** Groups / fixtures / group sets dropped on a cue: one layer on all
     *  of them, or (@a separate, ⌥ held) one layer per group -- a set's
     *  members each a layer of their own -- and one on the fixtures. */
    bool dropTargets(quint32 cueId, const QList<quint32> &groups, const QList<quint32> &fixtures,
                     bool separate = false, const QList<quint32> &sets = QList<quint32>());
    /** Groups / fixtures / sets dropped on a layer: added to its target. */
    bool dropTargetsOnLayer(quint32 cueId, quint32 layerId, const QList<quint32> &groups,
                            const QList<quint32> &fixtures, const QList<quint32> &sets = QList<quint32>());
    /** Groups / fixtures / sets dropped on a layer with ⇧ held: a child
     *  layer on them (§3.5) -- or, @a separate (⌥⇧), one child each. Each
     *  must be part of the layer's fixtures; refused (@a why) if not. */
    bool dropChildLayers(quint32 cueId, quint32 parentId, const QList<quint32> &groups,
                         const QList<quint32> &fixtures, const QList<quint32> &sets = QList<quint32>(),
                         bool separate = false, QString *why = nullptr);
    /** Make set (§1): the groups and fixtures selected in the Library as a
     *  group set named @a name. Invalid: nothing selected. */
    quint32 makeSetFromLibrary(const QString &name);
    /** MIME type of group sets dragged from the Library (a stream of ids). */
    static const char *setMimeType();
    /** Functions dropped on a cue: opaque function layers. */
    bool dropFunctions(quint32 cueId, const QList<quint32> &functions);
    /** Palettes dropped on a layer (or, with no layer, on the cue's looks;
     *  with no layer but a @a target, on a layer only carried into the cue,
     *  which then becomes the cue's own). */
    bool dropPalettes(quint32 cueId, quint32 layerId, const QList<quint32> &palettes,
                      const ShowTree::Target &target = ShowTree::Target());
    /** Groups / fixtures dropped on a layer only carried into the cue
     *  (tracked, included, base): it becomes the cue's own with them added,
     *  keeping what it carried -- one undo step. */
    bool dropTargetsOnTracked(quint32 cueId, const ShowTree::Target &target, const QList<quint32> &groups,
                              const QList<quint32> &fixtures, const QList<quint32> &sets = QList<quint32>());
    /** What a drop of @a mime on (@a cueId, @a layerId) would do, before it
     *  is let go of: "Blue → cue look: reaches 2 of 3 layers". With no layer
     *  but a @a target: onto a layer carried into the cue. */
    QString dropHint(quint32 cueId, quint32 layerId, const QMimeData *mime,
                     bool separate = false, const ShowTree::Target &target = ShowTree::Target(),
                     bool child = false) const;
    bool acceptsDrop(quint32 cueId, quint32 layerId, const QMimeData *mime,
                     const ShowTree::Target &target = ShowTree::Target(), bool child = false) const;
    /** Any drop on (@a cueId, @a layerId or carried-in @a target), as the
     *  tree, the cue page and the layer page take it: palettes set looks,
     *  groups / fixtures make a layer (⌥ @a separate: one each) or join
     *  one, functions play as they are. @a stayOnCue: the cue stays
     *  selected (a drop on the cue page), else the new layer is. */
    bool dropMime(quint32 cueId, quint32 layerId, const QMimeData *mime, bool separate = false,
                  const ShowTree::Target &target = ShowTree::Target(), bool stayOnCue = false,
                  bool child = false);
    /** The cue page's layer rows, as text ("⤳ Towers  Blue · tracked from 1
     *  One"), and the list itself (it takes drops). */
    QStringList cueLayerItems() const;
    QListWidget *cueLayerList() const { return m_cueLayers; }
    /** The target of the layer carried into cue @a cueId with @a signature
     *  (empty: none such). */
    ShowTree::Target carriedTarget(quint32 cueId, const QString &signature) const;
    /** Select a layer carried into cue @a cueId, by its target signature. */
    bool selectTracked(quint32 cueId, const QString &signature);

    /** MIME type of cues dragged within the tree (a stream of cue ids). */
    static const char *cueMimeType();
    /** A cue dropped on another of its list: moved to its place, or (with
        @a include, Alt held) included by it. */
    bool dropCue(quint32 cueId, quint32 onCueId, bool include);
    QString cueDropHint(quint32 cueId, quint32 onCueId, bool include) const;

    /** A value picked (D21 as changed 2026-10-17): the palette that plays
     *  exactly it (In this cue list, then Recent, then any), else a one-off
     *  -- an inline palette on the layer (or cue), "Make reusable" a click
     *  away on its row. */
    bool setOneOffDimmer(quint32 cueId, quint32 layerId, int percent);
    bool setOneOffColour(quint32 cueId, quint32 layerId, const QColor &colour);
    /** The Library's Recent palettes, newest first. */
    QList<quint32> recentPalettes() const;

    /** + Layer: the groups / fixtures selected in the Library as one layer,
     *  or (@a separate: ⌥-click, or its menu) one layer each -- as a drop. */
    bool addLayersFromLibrary(bool separate);
    /** Split a layer into one per group; merge layers of one cue. False /
     *  invalid (and @a why) when refused. */
    bool splitLayer(quint32 cueId, quint32 layerId, QString *why = nullptr);
    quint32 mergeLayers(quint32 cueId, const QList<quint32> &layerIds, QString *why = nullptr);
    /** The cue's own layers selected in the tree, when all are of one cue
     *  (@a cueId); empty otherwise. */
    QList<quint32> selectedLayers(quint32 *cueId = nullptr) const;
    /** Delete the cues and own layers selected in the tree (two or more)
     *  as one undo step; false (nothing done) when fewer, or a cue list
     *  or production is among them. */
    bool deleteSelected();

    /************************************************************************
     * Record as new cue: which stage (selected / running / everything lit)
     ************************************************************************/
public:
    typedef ShowTreeController::RecordSource RecordSource;
    /** QSettings key of the default for Design (@a perform false) or
     *  Perform: "ask" (default), "selected", "running" or "lit". */
    static QString recordSettingKey(bool perform);
    static QString recordSetting(bool perform);
    /** Record as new cue from @a cueId: when the choices differ the
     *  setting picks, or the dialog asks ("Don't ask again" writes the
     *  setting). The new cue, selected (invalid: cancelled). */
    quint32 recordFrom(quint32 cueId);
    /** Pick among @a choices (two or more) as recordFrom() does. */
    bool chooseRecordSource(quint32 cueId, const QList<RecordSource> &choices, RecordSource *chosen);
    /** In place of the dialog (tests): false cancels; @a dontAsk set writes
     *  the choice as the default for the mode. */
    typedef std::function<bool(const QList<RecordSource> &choices, RecordSource *chosen, bool *dontAsk)> RecordAsker;
    void setRecordAsker(const RecordAsker &asker) { m_recordAsker = asker; }
    /** Clear an attribute on the layer/cue (it inherits again), or set it
     *  None (off here). */
    bool clearAttribute(quint32 cueId, quint32 layerId, const QString &key);
    /** An effect's own Colour/Dimmer looks (they apply inside it only). */
    bool setEffectOwn(quint32 cueId, quint32 layerId, const QString &effectKey,
                      const QList<quint32> &palettes,
                      const ShowTree::Target &target = ShowTree::Target());
    bool setAttributeNone(quint32 cueId, quint32 layerId, const QString &key);

    /** The attribute rows the inspector shows for the selected layer, as
     *  text: "Colour | Blue | from cue 1" -- for the tests. */
    QStringList attributeRows() const;
    /** The Library's Recent items, newest first, as text. */
    QStringList recentItems() const;
    /** The Library's "In this cue list" items, most used first, as text
     *  ("Towers ×6"). */
    QStringList inListItems() const;

    /** "Next cue receives", as the cue page shows it. */
    QString nextCueText() const;
    /** The cue page's includes, as text, in order. */
    QStringList includeItems() const;

    /** D22: a tracked value's picker -- where it was set, as a cue label
     *  ("1.0.0 base"); empty when the value is this cue's own. */
    QString updateSourceLabel(quint32 cueId, const ShowTree::Target &target, const QString &key) const;

    /************************************************************************
     * Command line (§6C, optional)
     ************************************************************************/
public:
    /** The command line is always there (bottom of the tab); this gives it
     *  the keyboard, as : in the tree does. */
    void focusCommandLine();
    bool isCommandLineVisible() const;
    /** Run a command as if typed and Enter pressed: true when it ran. */
    bool runCommand(const QString &line);
    /** What the line under the command line says now. */
    QString commandMessage() const;
    QLineEdit *commandLine() const { return m_command; }

    /** Open @a paletteId in the Look Editor under the tree. */
    void editPalette(quint32 paletteId);
    /** The palette the Look Editor shows (invalid: none). */
    quint32 editedPalette() const;
    LookEditor *lookEditor() const { return m_lookEditor; }

    /** What the header says about the preview: "▶ On the rig: 1 One". */
    QString previewText() const;

    /************************************************************************
     * D13: what the tree made is changed in the tree
     ************************************************************************/
public:
    /** The Show Tree tab (null: none). */
    static ShowTreeView *instance();
    /** Bring the tab forward with generated function @a functionId's node
     *  selected (a list's chaser: the list; a cue's collection: the cue; a
     *  layer's scene: the layer). False: not in the tree. */
    bool reveal(quint32 functionId);
    /** A bar saying generated function @a functionId is the tree's
     *  (ShowTreeController::lockedText() and what it plays), with **Show in
     *  Show Tree** and, for a layer's scene, **Detach to plain scene**;
     *  @a detached is told the new scene. Editors of a generated function
     *  show it in place of (or, a chaser's, above) their controls. */
    static QWidget *lockedBanner(QWidget *parent, Doc *doc, quint32 functionId,
                                 const std::function<void(quint32)> &detached = nullptr);
    /** When any of @a functionIds is generated: says so (a message titled
     *  @a title naming them, with Show in Show Tree) and returns true -- the
     *  caller then leaves them alone (deleting, renaming, adding to). */
    static bool refuseGenerated(QWidget *parent, Doc *doc, const QList<quint32> &functionIds,
                                const QString &title);
    /** Detach the layer generated scene @a sceneId is (where it is a layer
     *  of a cue's own): the new plain scene, invalid when there is none. */
    static quint32 detachGenerated(Doc *doc, quint32 sceneId);

    /** Rename the selected tree row in place, as the Programming tab does
     *  (Return or F2 in the tree): a production, cue list, cue (its name,
     *  not its number) or layer (empty: back to its groups' name). Return
     *  commits, Esc cancels; one undo step. False: nothing to rename. */
    bool startRename();
    bool isRenaming() const { return m_renaming; }

    bool isPinned(int kind, quint32 id) const;
    void setPinned(int kind, quint32 id, bool pinned);

public slots:
    void reload();
    void slotNewProduction();
    void slotNewCueList();
    void slotNewCue();
    void slotAddLayer();
    void slotRecordCue();
    void slotDelete();
    /** Merge layers: the layers selected in the tree (two or more, of one
     *  cue), as the context menu's Merge. */
    void slotMergeSelected();
    void slotUndo();
    void slotRedo();
    /** Import a chaser as a cue list (§7): shows the report. */
    void importChaser(quint32 chaserId, bool showReport = true);

protected:
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;

private slots:
    void slotTreeSelectionChanged();
    void slotPreviewChanged();
    void slotTreeContextMenu(const QPoint &pos);
    void slotLibraryContextMenu(const QPoint &pos);
    void slotImportMenuAboutToShow();
    void slotCueEdited();
    void slotListModeChanged(int index);
    void slotListBaseChanged(int index);
    void slotCueBaseToggled(bool on);
    void slotCueMixToggled(bool on);
    void slotLayerReleaseToggled(bool on);
    void slotCommandEntered();
    void slotFilterChanged(const QString &text);
    void slotTreeChanged();
    void reloadLibrary();

private:
    void buildTree();
    void updateInspector();
    void updateButtons();
    void showCuePage(const ShowTree::CueList &list, int index);
    void showLayerPage(const ShowTree::CueList &list, int index, const ShowTree::Layer *own,
                       const ShowTree::ResolvedLayer *resolved);
    /** @a target: a carried-in layer's (set on it makes it the cue's own);
     *  @a rowTarget: the row's layer, own or not (for "Update cue X"). */
    void attributeMenu(quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                       const ShowTree::Target &rowTarget, const QString &key, const QPoint &globalPos);
    void includesMenu(QMenu *menu, quint32 cueId);
    void updateIncludeButtons();
    /** The cue list the selection is in (invalid: none). */
    quint32 currentListId() const;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    /** Set @a look on the layer, on the carried-in @a target, or the cue. */
    bool applyLook(quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                   const ShowTree::Look &look);
    quint32 inlinePalette(quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                          QLCPalette::PaletteType type, const QVariantList &values, const QString &value);
    /** The palette playing exactly @a values to reuse, else inlinePalette(). */
    quint32 valuePalette(quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                         QLCPalette::PaletteType type, const QVariantList &values, const QString &value);
    bool askRecordSource(quint32 cueId, const QList<RecordSource> &choices, RecordSource *chosen,
                         bool *dontAsk);
    QString lookText(const ShowTree::Look &look) const;
    QString originText(const ShowTree::CueList &list, const ShowTree::ResolvedValue &value) const;
    QString paletteName(quint32 id) const;
    /** The Look Editor changed @a paletteId (@a full: its name/icon too). */
    void paletteEdited(quint32 paletteId, bool full);
    void noteRecent(int kind, quint32 id, const QString &label = QString());
    QTreeWidgetItem *findItem(quint32 id, quint32 cueId) const;
    void saveExpanded();
    /** Preview the selected cue (Design mode, tab on screen), else stop. */
    void syncPreview();

    Doc *m_doc;
    ShowTreeController *m_st;

    QToolButton *m_newProduction;
    QToolButton *m_newList;
    QToolButton *m_newCue;
    QToolButton *m_addLayer;
    QToolButton *m_delete;
    QToolButton *m_undo;
    QToolButton *m_redo;
    QToolButton *m_import;
    QToolButton *m_record;
    QToolButton *m_merge;
    QMenu *m_importMenu;
    QLabel *m_hint;
    QLabel *m_previewLabel;

    QTreeWidget *m_tree;

    // Inspector
    QStackedWidget *m_pages;
    QLabel *m_emptyPage;
    QWidget *m_listPage;
    QLabel *m_listTitle;
    QComboBox *m_listMode;
    QComboBox *m_listBase;
    QLabel *m_listInfo;
    QWidget *m_cuePage;
    QLineEdit *m_cueNumber;
    QLineEdit *m_cueName;
    QLineEdit *m_cueNote;
    QComboBox *m_cueMode;
    QCheckBox *m_cueBase;
    QCheckBox *m_cueMix;
    QListWidget *m_cueIncludes;
    QToolButton *m_includeAdd;
    QToolButton *m_includeRemove;
    QToolButton *m_includeUp;
    QToolButton *m_includeDown;
    QDoubleSpinBox *m_cueFadeIn;
    QDoubleSpinBox *m_cueFadeOut;
    QDoubleSpinBox *m_cueHold;
    QLabel *m_cueLooks;
    QListWidget *m_cueLayers;
    QLabel *m_cueDropZone;
    QLabel *m_cueNext;
    QWidget *m_layerPage;
    QLabel *m_layerTitle;
    QCheckBox *m_layerRelease;
    ShowTree::Target m_layerReleaseTarget;     //!< a carried-in layer's, for Release
    QGridLayout *m_rows;
    QWidget *m_rowsHost;
    QStringList m_rowText;

    LookEditor *m_lookEditor = nullptr;

    // Library
    QLineEdit *m_filter;
    QTreeWidget *m_library;
    QList<QPair<int, quint32>> m_recent;
    QList<QPair<int, quint32>> m_pinned;
    /** Library folders open before a filter was typed (it opens more). */
    QSet<QString> m_openWithoutFilter;
    bool m_filtering = false;

    QTimer *m_libraryTimer = nullptr;
    quint32 m_libraryList = 0xFFFFFFFF;    //!< the list "In this cue list" shows

    // Command line
    QWidget *m_commandBar = nullptr;
    QLineEdit *m_command = nullptr;
    QLabel *m_commandMsg = nullptr;
    ShowTreeCommand *m_commander = nullptr;
    QStringList m_commandHistory;
    RecordAsker m_recordAsker;
    int m_historyAt = -1;
    QList<quint32> m_expanded;
    quint32 m_pageCue = 0xFFFFFFFF;      //!< the cue the cue page shows
    bool m_reloading = false;
    bool m_renaming = false;
    void slotTreeItemRenamed(QTreeWidgetItem *item, int column);
    bool m_inspectorUpdating = false;
};

/** @} */

#endif

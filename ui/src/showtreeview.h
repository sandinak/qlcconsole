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

#include <QWidget>
#include <QPair>
#include <QList>
#include <QSet>

#include "showtreemodel.h"
#include "qlcpalette.h"

class QStackedWidget;
class QListWidget;
class ShowTreeCommand;
class QDoubleSpinBox;
class QTreeWidgetItem;
class ShowTreeController;
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
    enum LibraryKind { LibSection = 100, LibFolder, LibGroup, LibFixture, LibPalette, LibFunction };

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
    /** Groups / fixtures dropped on a cue: one layer on all of them, or
     *  (@a separate, ⌥ held) one layer per group (and one on the fixtures). */
    bool dropTargets(quint32 cueId, const QList<quint32> &groups, const QList<quint32> &fixtures,
                     bool separate = false);
    /** Groups / fixtures dropped on a layer: added to its target. */
    bool dropTargetsOnLayer(quint32 cueId, quint32 layerId, const QList<quint32> &groups,
                            const QList<quint32> &fixtures);
    /** Functions dropped on a cue: opaque function layers. */
    bool dropFunctions(quint32 cueId, const QList<quint32> &functions);
    /** Palettes dropped on a layer (or, with no layer, on the cue's looks;
     *  with no layer but a @a target, on a layer only carried into the cue,
     *  which then becomes the cue's own). */
    bool dropPalettes(quint32 cueId, quint32 layerId, const QList<quint32> &palettes,
                      const ShowTree::Target &target = ShowTree::Target());
    /** What a drop of @a mime on (@a cueId, @a layerId) would do, before it
     *  is let go of: "Blue → cue look: reaches 2 of 3 layers". */
    QString dropHint(quint32 cueId, quint32 layerId, const QMimeData *mime,
                     bool separate = false) const;
    bool acceptsDrop(quint32 cueId, quint32 layerId, const QMimeData *mime) const;

    /** MIME type of cues dragged within the tree (a stream of cue ids). */
    static const char *cueMimeType();
    /** A cue dropped on another of its list: moved to its place, or (with
        @a include, Alt held) included by it. */
    bool dropCue(quint32 cueId, quint32 onCueId, bool include);
    QString cueDropHint(quint32 cueId, quint32 onCueId, bool include) const;

    /** A one-off value (D21): an inline palette on the layer (or cue). */
    bool setOneOffDimmer(quint32 cueId, quint32 layerId, int percent);
    bool setOneOffColour(quint32 cueId, quint32 layerId, const QColor &colour);
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
    void setCommandLineVisible(bool visible);
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
    QToolButton *m_commandToggle;
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
    QListWidget *m_cueIncludes;
    QToolButton *m_includeAdd;
    QToolButton *m_includeRemove;
    QToolButton *m_includeUp;
    QToolButton *m_includeDown;
    QDoubleSpinBox *m_cueFadeIn;
    QDoubleSpinBox *m_cueFadeOut;
    QDoubleSpinBox *m_cueHold;
    QLabel *m_cueLooks;
    QLabel *m_cueLayers;
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
    int m_historyAt = -1;
    QList<quint32> m_expanded;
    quint32 m_pageCue = 0xFFFFFFFF;      //!< the cue the cue page shows
    bool m_reloading = false;
    bool m_inspectorUpdating = false;
};

/** @} */

#endif

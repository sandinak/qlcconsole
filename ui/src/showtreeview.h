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

#include "showtreemodel.h"
#include "qlcpalette.h"

class QStackedWidget;
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
        TargetRole = Qt::UserRole + 3   //!< a tracked layer's target signature
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
    /** Groups / fixtures dropped on a cue: one layer each. */
    bool dropTargets(quint32 cueId, const QList<quint32> &groups, const QList<quint32> &fixtures);
    /** Functions dropped on a cue: opaque function layers. */
    bool dropFunctions(quint32 cueId, const QList<quint32> &functions);
    /** Palettes dropped on a layer (or, with no layer, on the cue's looks;
     *  with no layer but a @a target, on a layer only carried into the cue,
     *  which then becomes the cue's own). */
    bool dropPalettes(quint32 cueId, quint32 layerId, const QList<quint32> &palettes,
                      const ShowTree::Target &target = ShowTree::Target());
    /** What a drop of @a mime on (@a cueId, @a layerId) would do, before it
     *  is let go of: "Blue → cue look: reaches 2 of 3 layers". */
    QString dropHint(quint32 cueId, quint32 layerId, const QMimeData *mime) const;
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

    /** Open @a paletteId in the Look Editor under the tree. */
    void editPalette(quint32 paletteId);
    /** The palette the Look Editor shows (invalid: none). */
    quint32 editedPalette() const;
    LookEditor *lookEditor() const { return m_lookEditor; }

    bool isPinned(int kind, quint32 id) const;
    void setPinned(int kind, quint32 id, bool pinned);

public slots:
    void reload();
    void slotNewProduction();
    void slotNewCueList();
    void slotNewCue();
    void slotAddLayer();
    void slotDelete();
    void slotUndo();
    void slotRedo();
    /** Import a chaser as a cue list (§7): shows the report. */
    void importChaser(quint32 chaserId, bool showReport = true);

private slots:
    void slotTreeSelectionChanged();
    void slotTreeContextMenu(const QPoint &pos);
    void slotLibraryContextMenu(const QPoint &pos);
    void slotImportMenuAboutToShow();
    void slotCueEdited();
    void slotListModeChanged(int index);
    void slotLayerReleaseToggled(bool on);
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
    void attributeMenu(quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                       const QString &key, const QPoint &globalPos);
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
    QMenu *m_importMenu;
    QLabel *m_hint;

    QTreeWidget *m_tree;

    // Inspector
    QStackedWidget *m_pages;
    QLabel *m_emptyPage;
    QWidget *m_listPage;
    QLabel *m_listTitle;
    QComboBox *m_listMode;
    QLabel *m_listInfo;
    QWidget *m_cuePage;
    QLineEdit *m_cueNumber;
    QLineEdit *m_cueName;
    QLineEdit *m_cueNote;
    QComboBox *m_cueMode;
    QDoubleSpinBox *m_cueFadeIn;
    QDoubleSpinBox *m_cueFadeOut;
    QDoubleSpinBox *m_cueHold;
    QLabel *m_cueLooks;
    QLabel *m_cueLayers;
    QLabel *m_cueNext;
    QWidget *m_layerPage;
    QLabel *m_layerTitle;
    QCheckBox *m_layerRelease;
    QGridLayout *m_rows;
    QWidget *m_rowsHost;
    QStringList m_rowText;

    LookEditor *m_lookEditor = nullptr;

    // Library
    QLineEdit *m_filter;
    QTreeWidget *m_library;
    QList<QPair<int, quint32>> m_recent;
    QList<QPair<int, quint32>> m_pinned;

    QTimer *m_libraryTimer = nullptr;
    QList<quint32> m_expanded;
    quint32 m_pageCue = 0xFFFFFFFF;      //!< the cue the cue page shows
    bool m_reloading = false;
    bool m_inspectorUpdating = false;
};

/** @} */

#endif

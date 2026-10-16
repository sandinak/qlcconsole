/*
  Q Light Controller Plus
  functionstreewidget.h

  Copyright (c) Massimo Callegari

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


#ifndef FUNCTIONSTREEWIDGET_H
#define FUNCTIONSTREEWIDGET_H

#include <QTreeWidget>

class QLCPalette;
class Function;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * FunctionsTreeWidget represents the tree of QLC+ functions,
 * including categories and folders.
 * It can be used anywhere in QLC+ to display functions organized
 * by categories and folders.
 * If drag & drop flags are turned on, it becomes a full node
 * editor, accessing functions' properties. Basically this mode
 * has to be used only by FunctionManager
 *
 * Data is organized in the following way:
 *
 * |              COL_NAME                     |           COL_PATH             |
 *  ------------------------------------------- --------------------------------
 * | Text: Function/category/folder name       | Text: path of category/folder  |
 * | Data:                                     |       (not set for functions)  |
 * |   Qt::UserRole: function/palette ID       |                                |
 * |                 (or invalid for folders)  |                                |
 * |   Qt::UserRole + 1: category type         |                                |
 * |                     (Function::Type)      |                                |
 * |   Qt::UserRole + 2: node kind             |                                |
 * |                 (FunctionNode/PaletteNode)|                                |
 *  ------------------------------------------- --------------------------------
 *
 * Palettes are not Functions but share the tree: they live under a
 * synthetic "Palettes" category and carry NodeKind == PaletteNode (folders
 * inherit their category's kind), so the Function-assuming code paths skip
 * them exactly like they already skip folders.
 */

class FunctionsTreeWidget final : public QTreeWidget
{
    Q_OBJECT

public:
    /** Whether a tree item represents a Function subtree or a Palette
     *  subtree. Stored at Qt::UserRole + 2. FunctionNode is the default
     *  (0) so all pre-existing items are treated as functions. */
    enum NodeKind { FunctionNode = 0, PaletteNode = 1 };

    /** What the tree displays. ShowAll (default) keeps the classic
     *  Function Manager behaviour; the Programming tab uses FunctionsOnly
     *  for its navigator and PalettesOnly for its palette source. */
    enum DisplayFilter { ShowAll = 0, FunctionsOnly, PalettesOnly };

    FunctionsTreeWidget(Doc* doc, QWidget *parent = 0);

    /** Restrict what updateTree() shows. Call before updateTree(). */
    void setDisplayFilter(DisplayFilter filter);

    /** Order top-level TYPE categories by a fixed role rank (Show · Chaser ·
     *  Collection · RGB Matrix · EFX · Scene · …) instead of alphabetically.
     *  Off by default (classic A–Z); the Programming tab turns it on. */
    void setTypeOrderByRole(bool enable);
    bool typeOrderByRole() const { return m_typeOrderByRole; }

    /** Fixed sort rank for a Function::Type category (lower = earlier). */
    static int typeSortRank(int type);

    /** Show only items whose name contains $text (case-insensitive).
     *  Pass an empty string to show everything. Parent folders are shown
     *  if at least one child matches. */
    void filterByText(const QString &text);

    /** Update all functions to function tree */
    void updateTree();

    void clearTree();

    void functionNameChanged(quint32 fid);

    /** Add the Function with the given ID and returns
     *  a pointer to the created item */
    QTreeWidgetItem* addFunction(quint32 fid);

    /** Return a suitable parent item for the $function's type */
    QTreeWidgetItem* parentItem(const Function* function);

    /** Get the ID of the function represented by $item. */
    quint32 itemFunctionId(const QTreeWidgetItem* item) const;

    /** Get the item that represents the given function. */
    QTreeWidgetItem* functionItem(const Function* function);

    /*********************************************************************
     * Palettes (share the tree, but are not Functions)
     *********************************************************************/
public:
    /** Add the Palette with the given ID and return a pointer to the
     *  created (or existing) item. */
    QTreeWidgetItem* addPalette(quint32 pid);

    /** Get the ID of the palette represented by $item, or
     *  QLCPalette::invalidId() if $item is not a palette leaf. */
    quint32 itemPaletteId(const QTreeWidgetItem* item) const;

    /** Get the item that represents the given palette. */
    QTreeWidgetItem* paletteItem(const QLCPalette* palette);

    /** Node kind for $item (FunctionNode/PaletteNode). */
    static NodeKind itemNodeKind(const QTreeWidgetItem* item);

    /** Folder path that a new sibling/child of $item belongs to: the folder's
     *  own path, a leaf's parent folder, or the "Palettes" root for the
     *  category/null. Used to anchor "New folder". */
    QString paletteFolderPathFor(const QTreeWidgetItem* item) const;

    /** Ensure the palette folder at $fullPath ("Palettes/…") exists, creating
     *  any missing levels, and return its node. */
    QTreeWidgetItem* ensurePaletteFolder(const QString& fullPath);

private:
    /** Update $item's contents from the given $function */
    void updateFunctionItem(QTreeWidgetItem* item, const Function* function);

    /** Update $item's contents from the given $palette */
    void updatePaletteItem(QTreeWidgetItem* item, const QLCPalette* palette);

    /** Return a suitable parent item for the $palette (ensures the
     *  "Palettes" category and any folders along its path exist). */
    QTreeWidgetItem* paletteParentItem(const QLCPalette* palette);

    /** Ensure the synthetic "Palettes" category item exists; returns it. */
    QTreeWidgetItem* palettesCategoryItem();

private:
    Doc* m_doc;

    /*********************************************************************
     * Tree folders
     *********************************************************************/
public:
    void addFolder();

    void deleteFolder(QTreeWidgetItem *item);

    /** True if $item is a user folder (not a type category, the Palettes
     *  category, a function or a palette): something that can be renamed,
     *  deleted or dragged into another folder. */
    bool isUserFolder(const QTreeWidgetItem *item) const;

    /** Number of functions/palettes inside the folder $item, at any depth. */
    int folderContentCount(const QTreeWidgetItem *item) const;

    /** Remove the folder $item but keep what is in it: its functions/palettes
     *  and sub-folders move up into its parent folder (nothing is deleted).
     *  The tree is rebuilt asynchronously (folderContentsMoved()). */
    void removeFolderKeepContents(QTreeWidgetItem *item);

    /** Would dropping $mime on $target (NULL = empty space / top level) move
     *  something into a folder or onto a collection? Drives dragMoveEvent. */
    bool canDropMimeOn(const QMimeData *mime, QTreeWidgetItem *target);

    /** Perform the drop of $mime on $target (external/Programming mode):
     *  functions onto a Collection become members (functionsDroppedOnCollection);
     *  functions, palettes and folders onto a folder/category/empty space move
     *  there (paths change, the show is marked modified, the tree rebuilds
     *  asynchronously and emits folderContentsMoved()). Returns true if the
     *  drop was taken. */
    bool dropMimeOn(const QMimeData *mime, QTreeWidgetItem *target);

private:
    QTreeWidgetItem *folderItem(QString name);

    /** Full, slash-free-ended folder path ("Scene/Act 1", "Palettes/Color")
     *  that a drop on $target lands in for something of $kind/$typeName, or
     *  an empty string if $target can't take it. */
    QString dropFolderPath(QTreeWidgetItem *target, NodeKind kind,
                           const QString &typeName) const;

    /** Move everything under folder $oldPath to $newPath (both full paths,
     *  same category), merging into $newPath if it exists. Returns the
     *  re-rooted paths of the folders that existed under $oldPath (so the
     *  empty ones can be recreated after the rebuild). */
    QStringList reRootFolder(const QString &oldPath, const QString &newPath);

    /** Shared by canDropMimeOn()/dropMimeOn(). */
    bool handleFolderDrop(const QMimeData *mime, QTreeWidgetItem *target, bool apply);

    /** Rebuild the tree on the next event-loop turn (never from inside a drop:
     *  Qt's drag machinery still references the items), recreate $keepFolders
     *  and reveal $revealPath. */
    void scheduleRebuild(const QStringList &keepFolders, const QString &revealPath);

private slots:
    void slotItemChanged(QTreeWidgetItem *item);

    void slotUpdateChildrenPath(QTreeWidgetItem *root);

private:
    QHash <QString, QTreeWidgetItem *> m_foldersMap;

    DisplayFilter m_displayFilter;

    /** When true, top-level type categories sort by typeSortRank() not name. */
    bool m_typeOrderByRole;

    /*********************************************************************
     * Drag & Drop events
     *********************************************************************/
public:
    /** MIME type for function drag/drop to external widgets (e.g., CollectionEditor) */
    static const char* functionDragMimeType();

    /** MIME type for palette drag/drop to external widgets (e.g., the
     *  scene editor's group-looks panel). Payload is a stream of palette
     *  IDs (quint32). */
    static const char* paletteDragMimeType();

    /** MIME type for dragging folders between folders. Payload is a stream
     *  of (int NodeKind, QString full folder path) pairs. */
    static const char* folderDragMimeType();

    /** Enable external drag mode - drags will always use external MIME type without modifier key */
    void setExternalDragMode(bool enable);

signals:
    /** Emitted after functions/palettes/folders were moved into another folder
     *  (by a drop or removeFolderKeepContents()) and the tree was rebuilt. */
    void folderContentsMoved();

    /** Emitted (external/Programming mode) when function(s) are dropped onto a
     *  Collection node — @p containerId is the target collection, @p fids the
     *  dropped functions (scenes, other collections, …). The owner does the
     *  actual membership change (dedup / circular-ref checks live there). */
    void functionsDroppedOnCollection(quint32 containerId, const QList<quint32> &fids);

    /** Emitted when a palette is renamed in-place so canvas tiles can update. */
    void paletteRenamed(quint32 paletteId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    /** Enter/Return/F2 starts an inline rename of the current row (consistent
     *  with the Fixtures/Groups and Layers trees). */
    void keyPressEvent(QKeyEvent *event) override;
    /** In external-drag mode, use a small ghost anchored at the cursor (hotspot
     *  0,0) so dropped items land under the pointer — Qt's default full-row
     *  pixmap grabs at the press point and makes drops land offset to the right. */
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    /** Paints the drop highlight around the folder/collection a drag would
     *  land in (external mode, where Qt's own indicator never shows). */
    void paintEvent(QPaintEvent *event) override;

    /** Override mimeData to provide function IDs for Qt's built-in drag */
    QMimeData* mimeData(const QList<QTreeWidgetItem*> &items) const
        { return buildMimeData(items); }
    QMimeData* mimeData(const QList<QTreeWidgetItem*> items) const
        { return buildMimeData(items); }
private:
    QMimeData* buildMimeData(const QList<QTreeWidgetItem*> &items) const;

    /** Override to specify that external drags are copy operations, not move */
    Qt::DropActions supportedDropActions() const override;

private:
    /** Start an external drag operation with the selected functions */
    void startExternalDrag();

    QList<QTreeWidgetItem *>m_draggedItems;
    QPoint m_dragStartPosition;
    bool m_externalDragMode;
    /** Row highlighted as the drop destination (invalid = none; a valid
     *  root-level drop highlights the whole viewport via m_dropOnRoot). */
    QPersistentModelIndex m_dropHighlight;
    bool m_dropOnRoot;
};

/** @} */

#endif // FUNCTIONSTREEWIDGET_H

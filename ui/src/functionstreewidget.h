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

    FunctionsTreeWidget(Doc* doc, QWidget *parent = 0);

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

private:
    QTreeWidgetItem *folderItem(QString name);

private slots:
    void slotItemChanged(QTreeWidgetItem *item);

    void slotUpdateChildrenPath(QTreeWidgetItem *root);

private:
    QHash <QString, QTreeWidgetItem *> m_foldersMap;

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

    /** Enable external drag mode - drags will always use external MIME type without modifier key */
    void setExternalDragMode(bool enable);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    /** Override mimeData to provide function IDs for Qt's built-in drag */
    QMimeData* mimeData(const QList<QTreeWidgetItem*> items) const override;

    /** Override to specify that external drags are copy operations, not move */
    Qt::DropActions supportedDropActions() const override;

private:
    /** Start an external drag operation with the selected functions */
    void startExternalDrag();

    QList<QTreeWidgetItem *>m_draggedItems;
    QPoint m_dragStartPosition;
    bool m_externalDragMode;
};

/** @} */

#endif // FUNCTIONSTREEWIDGET_H

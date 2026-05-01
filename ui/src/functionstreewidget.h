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
 * |   Qt::UserRole: function ID (or invalid)  |                                |
 * |   Qt::UserRole + 1: category type         |                                |
 * |                     (Function::Type)      |                                |
 *  ------------------------------------------- --------------------------------
 */

class FunctionsTreeWidget final : public QTreeWidget
{
    Q_OBJECT

public:
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

private:
    /** Update $item's contents from the given $function */
    void updateFunctionItem(QTreeWidgetItem* item, const Function* function);

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

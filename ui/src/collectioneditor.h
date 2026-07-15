/*
  Q Light Controller
  collectioneditor.h

  Copyright (c) Heikki Junnila

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

#ifndef COLLECTIONEDITOR_H
#define COLLECTIONEDITOR_H

#include <QWidget>

#include "ui_collectioneditor.h"
#include "function.h"

class FunctionSelection;
class MasterTimer;
class Collection;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

class CollectionEditor final : public QWidget, public Ui_CollectionEditor
{
    Q_OBJECT
    Q_DISABLE_COPY(CollectionEditor)

public:
    CollectionEditor(QWidget* parent, Collection* fc, Doc* doc);
    ~CollectionEditor();

private:
    Doc* m_doc;
    Collection* m_collection; // The Collection being edited
    FunctionSelection* m_functionSelection;

private slots:
    void slotNameEdited(const QString& text);
    void slotAdd();
    void slotRemove();
    void slotMoveUp();
    void slotMoveDown();
    void slotTestClicked();
    /** Right-click menu on the member list: Remove (the existing multi-remove). */
    void slotTreeContextMenu(const QPoint& pos);

private:
    FunctionParent functionParent() const;

private:
    void updateFunctionList();

    /*********************************************************************
     * Drag & Drop
     *********************************************************************/
protected:
    /** Check if the function can be added (no circular references) */
    bool canAddFunction(quint32 fid) const;

    /** Event filter to handle drag/drop on m_tree */
    bool eventFilter(QObject* obj, QEvent* event) override;

    /** Handle drag enter events */
    void handleDragEnterEvent(QDragEnterEvent* event);

    /** Handle drag move events */
    void handleDragMoveEvent(QDragMoveEvent* event);

    /** Handle drop events */
    void handleDropEvent(QDropEvent* event);

    /** Reorder members from an internal drag: move the selected rows to the drop
     *  point and rewrite the collection's function order to match. */
    void reorderFromDrop(QDropEvent* event);
};

/** @} */

#endif


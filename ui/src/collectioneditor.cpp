/*
  Q Light Controller
  collectioneditor.cpp

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

#include <QTreeWidgetItem>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <algorithm>
#include <QTreeWidget>
#include <QDropEvent>
#include <QMimeData>
#include <QSettings>
#include <QLineEdit>
#include <QLabel>
#include <QMenu>
#include <QAction>

#include "functionselection.h"
#include "collectioneditor.h"
#include "mastertimer.h"
#include "collection.h"
#include "function.h"
#include "doc.h"
#include "app.h"

#define PROP_ID Qt::UserRole

// MIME type for function drag/drop
static const char* FUNCTION_DRAG_MIME_TYPE = "application/x-qlcplus-functions";

CollectionEditor::CollectionEditor(QWidget* parent, Collection* fc, Doc* doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_collection(fc)
    , m_functionSelection(NULL)
{
    Q_ASSERT(doc != NULL);
    Q_ASSERT(fc != NULL);

    setupUi(this);

    connect(m_nameEdit, SIGNAL(textEdited(const QString&)),
            this, SLOT(slotNameEdited(const QString&)));
    connect(m_add, SIGNAL(clicked()), this, SLOT(slotAdd()));
    connect(m_remove, SIGNAL(clicked()), this, SLOT(slotRemove()));
    connect(m_moveUp, SIGNAL(clicked()), this, SLOT(slotMoveUp()));
    connect(m_moveDown, SIGNAL(clicked()), this, SLOT(slotMoveDown()));
    connect(m_testButton, SIGNAL(clicked()),
            this, SLOT(slotTestClicked()));

    m_nameEdit->setText(m_collection->name());

    // Enable drag & drop on the function list.
    //   - Internal drag REORDERS a member (drag it to a new position).
    //   - External function drops ADD members (from the nav tree).
    // Enabling drag also stops a plain click-drag on a row from rubber-band
    // selecting across rows — dragging now moves the row, and multi-select is
    // Shift/Ctrl-click (ExtendedSelection), as the user expects.
    m_tree->setAcceptDrops(true);
    m_tree->setDragEnabled(true);
    m_tree->setDragDropMode(QAbstractItemView::DragDrop);
    m_tree->setDefaultDropAction(Qt::MoveAction);
    m_tree->setDropIndicatorShown(true);
    m_tree->viewport()->installEventFilter(this);

    // Right-click to remove selected members (the tree is ExtendedSelection and
    // slotRemove() already handles the whole selection — there just wasn't a
    // context menu to reach it, only the Remove button).
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(slotTreeContextMenu(const QPoint&)));

    updateFunctionList();
}

CollectionEditor::~CollectionEditor()
{
    if (m_functionSelection != NULL)
    {
        m_functionSelection->close();
        delete m_functionSelection;
    }

    if (m_testButton->isChecked())
        m_collection->stopAndWait();
}

void CollectionEditor::slotNameEdited(const QString& text)
{
    m_collection->setName(text);
}

void CollectionEditor::slotAdd()
{
    if (m_functionSelection == NULL)
    {
        m_functionSelection = new FunctionSelection(this, m_doc);
    }

    // Set up disabled functions: the collection itself, anything that would
    // create a circular reference, and every function already in the collection
    // (a collection holds each at most once, so re-picking one is a no-op —
    // grey it out instead of letting the user think it was added again).
    QList<quint32> disabledList;
    disabledList << m_collection->id();
    disabledList << m_collection->functions();
    foreach (Function* func, m_doc->functions())
    {
        if (func->contains(m_collection->id()))
            disabledList << func->id();
    }
    m_functionSelection->setDisabledFunctions(disabledList);

    // Use exec() to show modal dialog and get selection
    if (m_functionSelection->exec() == QDialog::Accepted)
    {
        QList<quint32> ids = m_functionSelection->selection();
        foreach (quint32 id, ids)
        {
            if (canAddFunction(id))
                m_collection->addFunction(id);
        }
        updateFunctionList();
    }
}

void CollectionEditor::slotRemove()
{
    QList <QTreeWidgetItem*> items(m_tree->selectedItems());
    QListIterator <QTreeWidgetItem*> it(items);

    while (it.hasNext() == true)
    {
        QTreeWidgetItem* item(it.next());
        quint32 id = item->data(0, PROP_ID).toUInt();
        m_collection->removeFunction(id);
        delete item;
    }
}

void CollectionEditor::slotTreeContextMenu(const QPoint& pos)
{
    // Right-click a row that isn't selected → select it first, so the menu acts
    // on what the user pointed at.
    QTreeWidgetItem* clicked = m_tree->itemAt(pos);
    if (clicked != NULL && clicked->isSelected() == false)
        m_tree->setCurrentItem(clicked);

    const int count = m_tree->selectedItems().size();
    if (count == 0)
        return;

    QMenu menu(this);
    QAction* removeAction = menu.addAction(
        count > 1 ? tr("Remove %1 functions from collection").arg(count)
                  : tr("Remove from collection"));

    if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == removeAction)
        slotRemove();
}

void CollectionEditor::slotMoveUp()
{
    QList <QTreeWidgetItem*> items(m_tree->selectedItems());
    QListIterator <QTreeWidgetItem*> it(items);

    // Check, whether even one of the items would "bleed" over the edge and
    // cancel the operation if that is the case.
    while (it.hasNext() == true)
    {
        QTreeWidgetItem* item(it.next());
        int index = m_tree->indexOfTopLevelItem(item);
        if (index == 0)
            return;
    }

    // Move the items
    it.toFront();
    while (it.hasNext() == true)
    {
        QTreeWidgetItem* item(it.next());
        int index = m_tree->indexOfTopLevelItem(item);
        m_tree->takeTopLevelItem(index);
        m_tree->insertTopLevelItem(index - 1, item);

        quint32 id = item->data(0, PROP_ID).toUInt();
        m_collection->removeFunction(id);
        m_collection->addFunction(id, index - 1);
    }

    // Select the moved items
    it.toFront();
    while (it.hasNext() == true)
        it.next()->setSelected(true);
}

void CollectionEditor::slotMoveDown()
{
    QList <QTreeWidgetItem*> items(m_tree->selectedItems());
    QListIterator <QTreeWidgetItem*> it(items);

    // Check, whether even one of the items would "bleed" over the edge and
    // cancel the operation if that is the case.
    while (it.hasNext() == true)
    {
        QTreeWidgetItem* item(it.next());
        int index = m_tree->indexOfTopLevelItem(item);
        if (index == m_tree->topLevelItemCount() - 1)
            return;
    }

    // Move the items
    it.toBack();
    while (it.hasPrevious() == true)
    {
        QTreeWidgetItem* item(it.previous());
        int index = m_tree->indexOfTopLevelItem(item);
        m_tree->takeTopLevelItem(index);
        m_tree->insertTopLevelItem(index + 1, item);

        quint32 id = item->data(0, PROP_ID).toUInt();
        m_collection->removeFunction(id);
        m_collection->addFunction(id, index + 1);
    }

    // Select the items
    it.toFront();
    while (it.hasNext() == true)
        it.next()->setSelected(true);
}

void CollectionEditor::slotTestClicked()
{
    if (m_testButton->isChecked() == true)
        m_collection->start(m_doc->masterTimer(), functionParent());
    else
        m_collection->stopAndWait();
}

FunctionParent CollectionEditor::functionParent() const
{
    return FunctionParent::master();
}

void CollectionEditor::updateFunctionList()
{
    m_tree->clear();

    foreach (QVariant fid, m_collection->functions())
    {
        Function* function = m_doc->function(fid.toUInt());
        Q_ASSERT(function != NULL);

        QTreeWidgetItem* item = new QTreeWidgetItem(m_tree);
        item->setText(0, function->name());
        item->setData(0, PROP_ID, function->id());
        item->setIcon(0, function->getIcon());
    }
}

/*****************************************************************************
 * Drag & Drop
 *****************************************************************************/

bool CollectionEditor::canAddFunction(quint32 fid) const
{
    // Cannot add the collection itself
    if (fid == m_collection->id())
        return false;

    // A collection holds each function at most once — reject one already in it.
    // (The engine's addFunction dedups silently, but rejecting here keeps the
    // drop path from claiming success and lets slotAdd give feedback.)
    if (m_collection->functions().contains(fid))
        return false;

    // Cannot add functions that contain this collection (circular reference)
    Function* function = m_doc->function(fid);
    if (function != NULL && function->contains(m_collection->id()))
        return false;

    return true;
}

bool CollectionEditor::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_tree->viewport())
    {
        switch (event->type())
        {
        case QEvent::DragEnter:
            handleDragEnterEvent(static_cast<QDragEnterEvent*>(event));
            return true;
        case QEvent::DragMove:
            handleDragMoveEvent(static_cast<QDragMoveEvent*>(event));
            return true;
        case QEvent::Drop:
            handleDropEvent(static_cast<QDropEvent*>(event));
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void CollectionEditor::handleDragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(FUNCTION_DRAG_MIME_TYPE))
    {
        event->setDropAction(Qt::CopyAction);   // external add
        event->accept();
    }
    else if (event->source() == m_tree)
    {
        event->setDropAction(Qt::MoveAction);   // internal reorder
        event->accept();
    }
    else
        event->ignore();
}

void CollectionEditor::handleDragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(FUNCTION_DRAG_MIME_TYPE))
    {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
    else if (event->source() == m_tree)
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
    else
        event->ignore();
}

void CollectionEditor::handleDropEvent(QDropEvent* event)
{
    // Internal reorder: an item from THIS tree dropped back onto it.
    if (!event->mimeData()->hasFormat(FUNCTION_DRAG_MIME_TYPE))
    {
        if (event->source() == m_tree)
            reorderFromDrop(event);
        else
            event->ignore();
        return;
    }

    // External add: function IDs dragged in from the nav tree.
    QByteArray data = event->mimeData()->data(FUNCTION_DRAG_MIME_TYPE);
    QDataStream stream(&data, QIODevice::ReadOnly);

    bool addedAny = false;
    while (!stream.atEnd())
    {
        quint32 fid;
        stream >> fid;

        if (canAddFunction(fid))
        {
            m_collection->addFunction(fid);
            addedAny = true;
        }
    }

    if (addedAny)
    {
        updateFunctionList();
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void CollectionEditor::reorderFromDrop(QDropEvent* event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const QPoint dropPos = event->pos();
#else
    const QPoint dropPos = event->position().toPoint();
#endif

    // The rows being moved, in their current visual order.
    QList<QTreeWidgetItem*> sel = m_tree->selectedItems();
    if (sel.isEmpty())
    {
        event->ignore();
        return;
    }
    std::sort(sel.begin(), sel.end(), [this](QTreeWidgetItem* a, QTreeWidgetItem* b) {
        return m_tree->indexOfTopLevelItem(a) < m_tree->indexOfTopLevelItem(b);
    });

    QList<quint32> dragged;
    QSet<quint32> draggedSet;
    for (QTreeWidgetItem* it : qAsConst(sel))
    {
        const quint32 fid = it->data(0, PROP_ID).toUInt();
        dragged << fid;
        draggedSet.insert(fid);
    }

    // Current full order.
    QList<quint32> order;
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
        order << m_tree->topLevelItem(i)->data(0, PROP_ID).toUInt();

    // Where to drop: above/below the row under the cursor (end if past the last).
    QTreeWidgetItem* target = m_tree->itemAt(dropPos);
    quint32 targetFid = Function::invalidId();
    bool below = true;
    if (target != NULL)
    {
        targetFid = target->data(0, PROP_ID).toUInt();
        const QRect r = m_tree->visualItemRect(target);
        below = dropPos.y() > r.center().y();
    }

    // Rebuild the order: pull the dragged ids out, then reinsert them as a block
    // at the drop point.
    QList<quint32> reduced;
    for (quint32 fid : qAsConst(order))
        if (!draggedSet.contains(fid))
            reduced << fid;

    int insertAt;
    if (targetFid == Function::invalidId() || draggedSet.contains(targetFid))
        insertAt = reduced.size();               // dropped past the end, or onto itself
    else
    {
        const int ti = reduced.indexOf(targetFid);
        insertAt = (ti < 0) ? reduced.size() : (below ? ti + 1 : ti);
    }

    QList<quint32> newOrder = reduced;
    for (int i = 0; i < dragged.size(); i++)
        newOrder.insert(insertAt + i, dragged.at(i));

    // Accept as a COPY, never a MOVE: we rebuild the list ourselves below, so
    // Qt's own post-drop clearOrRemove() (which fires on a returned MoveAction)
    // must not run — by the time it would, it'd be deleting rebuilt/stale rows.
    event->setDropAction(Qt::CopyAction);
    event->accept();
    if (newOrder == order)
        return;   // dropped back where it started

    // Apply the new order to the collection. The engine has no bulk reorder, so
    // clear the membership and re-add in the target order (same primitive the
    // Move Up/Down buttons use).
    for (quint32 fid : qAsConst(order))
        m_collection->removeFunction(fid);
    for (quint32 fid : qAsConst(newOrder))
        m_collection->addFunction(fid);

    updateFunctionList();

    // Keep the moved rows selected so the user can move them again.
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* it = m_tree->topLevelItem(i);
        if (draggedSet.contains(it->data(0, PROP_ID).toUInt()))
            it->setSelected(true);
    }
}

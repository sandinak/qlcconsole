/*
  Q Light Controller Plus
  functionstreewidget.cpp

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

#include <QContextMenuEvent>
#include <QDebug>
#include <QApplication>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDrag>

#include "functionstreewidget.h"
#include "qlcpalette.h"
#include "function.h"
#include "scene.h"
#include "doc.h"

#define COL_NAME 0
#define COL_PATH 1

// Node-kind discriminator (Qt::UserRole + 2). Absent/0 == FunctionNode, so
// every item created before palettes existed reads back as a function.
#define NODE_KIND_ROLE (Qt::UserRole + 2)

// Synthetic category that hosts all palettes. Must not collide with any
// Function::typeToString() value (it doesn't).
static const char* PALETTE_CATEGORY = "Palettes";

// MIME type for function drag/drop to external widgets
static const char* FUNCTION_DRAG_MIME_TYPE = "application/x-qlcplus-functions";
// MIME type for palette drag/drop to external widgets
static const char* PALETTE_DRAG_MIME_TYPE = "application/x-qlcplus-palettes";

FunctionsTreeWidget::FunctionsTreeWidget(Doc *doc, QWidget *parent) :
    QTreeWidget(parent)
  , m_doc(doc)
  , m_displayFilter(ShowAll)
  , m_externalDragMode(false)
{
    sortItems(COL_NAME, Qt::AscendingOrder);

    QTreeWidgetItem *root = invisibleRootItem();
    root->setFlags(root->flags() & ~Qt::ItemIsDropEnabled);

    connect(this, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
                this, SLOT(slotItemChanged(QTreeWidgetItem*)));
}

void FunctionsTreeWidget::setDisplayFilter(DisplayFilter filter)
{
    m_displayFilter = filter;
}

void FunctionsTreeWidget::updateTree()
{
    blockSignals(true);

    clearTree();

    if (m_displayFilter != PalettesOnly)
    {
        foreach (Function* function, m_doc->functions())
        {
            if (function->isVisible())
                updateFunctionItem(new QTreeWidgetItem(parentItem(function)), function);
        }
    }

    if (m_displayFilter != FunctionsOnly)
    {
        foreach (QLCPalette* palette, m_doc->palettes())
        {
            if (palette == NULL)
                continue;
            QTreeWidgetItem* parent = paletteParentItem(palette);
            if (parent != NULL)
                updatePaletteItem(new QTreeWidgetItem(parent), palette);
        }
    }

    blockSignals(false);
}

void FunctionsTreeWidget::clearTree()
{
    m_foldersMap.clear();
    clear();
}

void FunctionsTreeWidget::functionNameChanged(quint32 fid)
{
    blockSignals(true);
    Function* function = m_doc->function(fid);
    if (function == NULL)
    {
        blockSignals(false);
        return;
    }

    QTreeWidgetItem* item = functionItem(function);
    if (item != NULL)
        updateFunctionItem(item, function);

    blockSignals(false);
}

QTreeWidgetItem *FunctionsTreeWidget::addFunction(quint32 fid)
{
    Function* function = m_doc->function(fid);
    if (function == NULL || function->isVisible() == false)
        return NULL;

    QTreeWidgetItem* item = functionItem(function);
    if (item != NULL)
        return item;

    blockSignals(true);
    QTreeWidgetItem* parent = parentItem(function);
    item = new QTreeWidgetItem(parent);
    updateFunctionItem(item, function);
    if (parent != NULL)
        function->setPath(parent->text(COL_PATH));
    blockSignals(false);
    return item;
}

void FunctionsTreeWidget::updateFunctionItem(QTreeWidgetItem* item, const Function* function)
{
    Q_ASSERT(item != NULL);
    Q_ASSERT(function != NULL);
    item->setText(COL_NAME, function->name());
    item->setIcon(COL_NAME, function->getIcon());
    item->setData(COL_NAME, Qt::UserRole, function->id());
    item->setData(COL_NAME, Qt::UserRole + 1, function->type());
    // Function rows are editable (in-place rename via F2 or click on
    // already-selected). NOT drop-enabled (children would be silly).
    item->setFlags((item->flags() | Qt::ItemIsEditable)
                   & ~Qt::ItemIsDropEnabled);

    // Colour-code scenes by composition: dynamic (look/group-driven), static
    // (baked channel values) or mixed.
    const Scene *scene = qobject_cast<const Scene*>(function);
    if (scene != NULL)
    {
        const bool baked = scene->values().isEmpty() == false;
        const bool dyn = scene->palettes().isEmpty() == false
                      || scene->fixtureGroups().isEmpty() == false;
        if (dyn && !baked)
        {
            item->setForeground(COL_NAME, QColor("#4aa3ff"));
            item->setToolTip(COL_NAME, tr("Dynamic scene — driven by looks / fixture groups"));
        }
        else if (baked && !dyn)
        {
            item->setForeground(COL_NAME, QColor("#9aa0a6"));
            item->setToolTip(COL_NAME, tr("Static scene — baked channel values"));
        }
        else if (baked && dyn)
        {
            item->setForeground(COL_NAME, QColor("#c08af0"));
            item->setToolTip(COL_NAME, tr("Mixed scene — looks + baked values"));
        }
        else
        {
            item->setToolTip(COL_NAME, tr("Empty scene"));
        }
    }
}

QTreeWidgetItem* FunctionsTreeWidget::parentItem(const Function* function)
{
    Q_ASSERT(function != NULL);

    if (function->isVisible() == false)
        return NULL;

    QString basePath = Function::typeToString(function->type());
    if (m_foldersMap.contains(QString(basePath + "/")) == false)
    {
        // Parent item for the given type doesn't exist yet so create one
        QTreeWidgetItem* item = new QTreeWidgetItem(this);
        item->setText(COL_NAME, basePath);
        item->setIcon(COL_NAME, function->getIcon());
        item->setData(COL_NAME, Qt::UserRole, Function::invalidId());
        item->setData(COL_NAME, Qt::UserRole + 1, function->type());
        item->setText(COL_PATH, QString(basePath + "/"));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);
        m_foldersMap[QString(basePath + "/")] = item;
    }

    QTreeWidgetItem *pItem = folderItem(function->path());

    if (pItem != NULL)
    {
        //qDebug() << "Found item for function:" << function->name() << ", path: " << function->path();
        return pItem;
    }

    return NULL;
}

quint32 FunctionsTreeWidget::itemFunctionId(const QTreeWidgetItem* item) const
{
    if (item == NULL || item->parent() == NULL)
        return Function::invalidId();

    // Palette nodes share the tree but are not Functions. Report them as
    // invalid here so every Function-assuming call site skips them exactly
    // like it already skips folders (whose UserRole is invalidId too). The
    // function and palette ID spaces are distinct, so a palette ID must
    // never be handed to m_doc->function().
    if (itemNodeKind(item) == PaletteNode)
        return Function::invalidId();

    QVariant var = item->data(COL_NAME, Qt::UserRole);
    if (var.isValid() == false)
        return Function::invalidId();

    return var.toUInt();
}

QTreeWidgetItem* FunctionsTreeWidget::functionItem(const Function* function)
{
    Q_ASSERT(function != NULL);

    if (function->isVisible() == false)
        return NULL;

    QTreeWidgetItem* parent = parentItem(function);
    Q_ASSERT(parent != NULL);

    for (int i = 0; i < parent->childCount(); i++)
    {
        QTreeWidgetItem* item = parent->child(i);
        if (itemFunctionId(item) == function->id())
            return item;
    }

    return NULL;
}

/*********************************************************************
 * Palettes
 *********************************************************************/

FunctionsTreeWidget::NodeKind FunctionsTreeWidget::itemNodeKind(const QTreeWidgetItem* item)
{
    if (item == NULL)
        return FunctionNode;
    QVariant var = item->data(COL_NAME, NODE_KIND_ROLE);
    if (var.isValid() == false)
        return FunctionNode;
    return static_cast<NodeKind>(var.toInt());
}

QTreeWidgetItem* FunctionsTreeWidget::palettesCategoryItem()
{
    const QString basePath = QString(PALETTE_CATEGORY) + "/";
    if (m_foldersMap.contains(basePath) == false)
    {
        // In a palettes-only tree the whole widget is palettes, so the
        // "Palettes" header is redundant: hang folders/leaves off the
        // invisible root instead of a visible category node.
        if (m_displayFilter == PalettesOnly)
        {
            m_foldersMap[basePath] = invisibleRootItem();
        }
        else
        {
            QTreeWidgetItem* item = new QTreeWidgetItem(this);
            item->setText(COL_NAME, tr(PALETTE_CATEGORY));
            item->setIcon(COL_NAME, QIcon(":/color.png"));
            item->setData(COL_NAME, Qt::UserRole, Function::invalidId());
            item->setData(COL_NAME, Qt::UserRole + 1, Function::Undefined);
            item->setData(COL_NAME, NODE_KIND_ROLE, PaletteNode);
            item->setText(COL_PATH, basePath);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);
            m_foldersMap[basePath] = item;
        }
    }
    return m_foldersMap[basePath];
}

QTreeWidgetItem* FunctionsTreeWidget::paletteParentItem(const QLCPalette* palette)
{
    Q_ASSERT(palette != NULL);

    QTreeWidgetItem* category = palettesCategoryItem();

    const QString path = palette->path();
    const QString basePath = QString(PALETTE_CATEGORY) + "/";
    if (path.isEmpty() || path == basePath || path == PALETTE_CATEGORY)
        return category;

    QTreeWidgetItem* pItem = folderItem(path);
    return pItem != NULL ? pItem : category;
}

void FunctionsTreeWidget::updatePaletteItem(QTreeWidgetItem* item, const QLCPalette* palette)
{
    Q_ASSERT(item != NULL);
    Q_ASSERT(palette != NULL);
    item->setText(COL_NAME, palette->name());
    item->setIcon(COL_NAME, QIcon(palette->iconResource()));
    item->setData(COL_NAME, Qt::UserRole, palette->id());
    item->setData(COL_NAME, Qt::UserRole + 1, Function::Undefined);
    item->setData(COL_NAME, NODE_KIND_ROLE, PaletteNode);
    // Editable for in-place rename; drag-enabled so it can be moved
    // between palette folders. Never drop-enabled (a leaf can't host
    // children).
    item->setFlags((item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled)
                   & ~Qt::ItemIsDropEnabled);
}

QTreeWidgetItem* FunctionsTreeWidget::addPalette(quint32 pid)
{
    QLCPalette* palette = m_doc->palette(pid);
    if (palette == NULL)
        return NULL;

    QTreeWidgetItem* item = paletteItem(palette);
    if (item != NULL)
        return item;

    blockSignals(true);
    QTreeWidgetItem* parent = paletteParentItem(palette);
    item = new QTreeWidgetItem(parent);
    updatePaletteItem(item, palette);
    blockSignals(false);
    return item;
}

quint32 FunctionsTreeWidget::itemPaletteId(const QTreeWidgetItem* item) const
{
    if (item == NULL || item->parent() == NULL)
        return QLCPalette::invalidId();
    if (itemNodeKind(item) != PaletteNode)
        return QLCPalette::invalidId();
    // Folders/category carry a non-empty COL_PATH; only leaf palette items
    // map to a real palette ID.
    if (item->text(COL_PATH).isEmpty() == false)
        return QLCPalette::invalidId();

    QVariant var = item->data(COL_NAME, Qt::UserRole);
    if (var.isValid() == false)
        return QLCPalette::invalidId();
    return var.toUInt();
}

QTreeWidgetItem* FunctionsTreeWidget::paletteItem(const QLCPalette* palette)
{
    Q_ASSERT(palette != NULL);

    QTreeWidgetItem* parent = paletteParentItem(palette);
    if (parent == NULL)
        return NULL;

    // Walk the whole palette subtree (a palette may live in a sub-folder).
    QList<QTreeWidgetItem*> stack;
    stack.append(parent);
    while (stack.isEmpty() == false)
    {
        QTreeWidgetItem* node = stack.takeLast();
        for (int i = 0; i < node->childCount(); i++)
        {
            QTreeWidgetItem* child = node->child(i);
            if (itemPaletteId(child) == palette->id())
                return child;
            if (child->text(COL_PATH).isEmpty() == false)
                stack.append(child);
        }
    }
    return NULL;
}

/*********************************************************************
 * Tree folders
 *********************************************************************/

void FunctionsTreeWidget::addFolder()
{
    blockSignals(true);
    if (selectedItems().isEmpty())
    {
        blockSignals(false);
        return;
    }

    QTreeWidgetItem *item = selectedItems().first();
    if (item->text(COL_PATH).isEmpty())
        item = item->parent();

    int type = item->data(COL_NAME, Qt::UserRole + 1).toInt();
    NodeKind kind = itemNodeKind(item);

    QString fullPath = item->text(COL_PATH);
    if (fullPath.endsWith('/') == false)
        fullPath.append("/");

    QString newName = "New folder";

    int folderCount = 1;

    while (m_foldersMap.contains(fullPath + newName))
    {
        newName = "New Folder " + QString::number(folderCount++);
    }

    fullPath += newName;

    QTreeWidgetItem *folder = new QTreeWidgetItem(item);
    folder->setText(COL_NAME, newName);
    folder->setIcon(COL_NAME, QIcon(":/folder.png"));
    folder->setData(COL_NAME, Qt::UserRole, Function::invalidId());
    folder->setData(COL_NAME, Qt::UserRole + 1, type);
    folder->setData(COL_NAME, NODE_KIND_ROLE, kind);
    folder->setText(COL_PATH, fullPath);
    folder->setFlags(folder->flags() | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);

    m_foldersMap[fullPath] = folder;
    item->setExpanded(true);

    blockSignals(false);

    scrollToItem(folder, QAbstractItemView::PositionAtCenter);
}

void FunctionsTreeWidget::deleteFolder(QTreeWidgetItem *item)
{
    if (item == NULL)
        return;

    QList<QTreeWidgetItem*> childrenList;
    for (int i = 0; i < item->childCount(); i++)
        childrenList.append(item->child(i));

    QListIterator <QTreeWidgetItem*> it(childrenList);
    while (it.hasNext() == true)
    {
        QTreeWidgetItem *child = it.next();
        if (itemNodeKind(child) == PaletteNode && child->text(COL_PATH).isEmpty())
        {
            // Palette leaf: detach from any scenes, then drop it.
            const quint32 pid = itemPaletteId(child);
            if (pid != QLCPalette::invalidId())
            {
                m_doc->deletePalette(pid);
                delete child;
            }
            continue;
        }

        quint32 fid = child->data(COL_NAME, Qt::UserRole).toUInt();
        if (fid != Function::invalidId() && itemNodeKind(child) == FunctionNode)
        {
            m_doc->deleteFunction(fid);
            delete child;
        }
        else
            deleteFolder(child);
    }

    QString name = item->text(COL_PATH);

    if (m_foldersMap.contains(name))
        m_foldersMap.remove(name);

    delete item;
}

QString FunctionsTreeWidget::paletteFolderPathFor(const QTreeWidgetItem* item) const
{
    if (item == NULL)
        return QString(PALETTE_CATEGORY);

    const QTreeWidgetItem* node = item;
    if (itemPaletteId(item) != QLCPalette::invalidId()) // a palette leaf
        node = item->parent();
    if (node == NULL)
        return QString(PALETTE_CATEGORY);

    QString p = node->text(COL_PATH);
    if (p.isEmpty())
        return QString(PALETTE_CATEGORY); // invisible root / category
    if (p.endsWith('/'))
        p.chop(1);
    return p;
}

QTreeWidgetItem* FunctionsTreeWidget::ensurePaletteFolder(const QString& fullPath)
{
    // folderItem() creates every missing level and returns the leaf folder.
    // Clear the selection first so its selection-based fast path can't return
    // an unrelated folder.
    clearSelection();
    return folderItem(fullPath);
}

QTreeWidgetItem *FunctionsTreeWidget::folderItem(QString name)
{
    if (selectedItems().count() > 0)
    {
        QString currFolder = selectedItems().first()->text(COL_PATH);
        if (currFolder.contains(name) && m_foldersMap.contains(currFolder))
            return m_foldersMap[currFolder];
    }

    if (m_foldersMap.contains(name))
        return m_foldersMap[name];

    if (name.endsWith('/'))
        return NULL;

    qDebug() << "Folder" << name << "doesn't exist. Creating it...";

    QTreeWidgetItem *parentNode = NULL;
    int type = Function::Undefined;
    NodeKind kind = FunctionNode;
    QString fullPath;
    QStringList levelsList = name.split("/");
    foreach (QString level, levelsList)
    {
        // the first round is a category node. Just retrieve the item pointer
        // and the type, then skip it.
        if (fullPath.isEmpty())
        {
            if (m_foldersMap.contains(level + "/"))
            {
                parentNode = m_foldersMap[level + "/"];
                type = parentNode->data(COL_NAME, Qt::UserRole + 1).toInt();
                kind = itemNodeKind(parentNode);
            }
            fullPath = level;
            continue;
        }

        fullPath.append("/");
        fullPath.append(level);

        // create only missing levels
        if (m_foldersMap.contains(fullPath) == false)
        {
            QTreeWidgetItem *folder = new QTreeWidgetItem(parentNode);
            folder->setText(COL_NAME, level);
            folder->setIcon(COL_NAME, QIcon(":/folder.png"));
            folder->setData(COL_NAME, Qt::UserRole, Function::invalidId());
            folder->setData(COL_NAME, Qt::UserRole + 1, type);
            folder->setData(COL_NAME, NODE_KIND_ROLE, kind);
            folder->setText(COL_PATH, fullPath);
            folder->setFlags(folder->flags() | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);

            m_foldersMap[fullPath] = folder;
            parentNode = folder;
        }
        else
            parentNode = m_foldersMap[fullPath];
    }

    return m_foldersMap[name];
}

void FunctionsTreeWidget::slotItemChanged(QTreeWidgetItem *item)
{
    blockSignals(true);
    qDebug() << "[FunctionsTreeWidget] TREE item changed";

    // Function items have an empty COL_PATH (the path lives on the
    // parent folder item). When the user renames a function row in
    // place, persist the new caption back onto the Function object so
    // it survives save+reload and shows up in chasers, references, etc.
    if (item->text(COL_PATH).isEmpty())
    {
        const QString newName = item->text(COL_NAME).trimmed();
        if (itemNodeKind(item) == PaletteNode)
        {
            QLCPalette *p = m_doc->palette(itemPaletteId(item));
            if (p != NULL && !newName.isEmpty() && p->name() != newName)
            {
                p->setName(newName);
                m_doc->setModified();
            }
            blockSignals(false);
            return;
        }

        const quint32 fid = item->data(COL_NAME, Qt::UserRole).toUInt();
        if (fid != Function::invalidId())
        {
            Function *fn = m_doc->function(fid);
            if (fn != NULL && !newName.isEmpty() && fn->name() != newName)
            {
                fn->setName(newName);
                m_doc->setModified();
            }
        }
        blockSignals(false);
        return;
    }

    QTreeWidgetItem *parent = item->parent();
    if (parent != NULL)
    {
        QString fullPath = parent->text(COL_PATH);

        if (fullPath.endsWith('/') == false)
            fullPath.append("/");
        fullPath.append(item->text(COL_NAME));

        m_foldersMap.remove(item->text(COL_PATH));
        item->setText(COL_PATH, fullPath);
        m_foldersMap[fullPath] = item;
        slotUpdateChildrenPath(item);
    }
    blockSignals(false);
}

void FunctionsTreeWidget::slotUpdateChildrenPath(QTreeWidgetItem *root)
{
    if (root->childCount() == 0)
        return;
    for (int i = 0; i < root->childCount(); i++)
    {
        QTreeWidgetItem *child = root->child(i);

        // child can be a function/palette leaf or another folder
        QString path = child->text(COL_PATH);
        if (path.isEmpty()) // leaf node
        {
            if (itemNodeKind(child) == PaletteNode)
            {
                QLCPalette *p = m_doc->palette(itemPaletteId(child));
                if (p != NULL)
                    p->setPath(root->text(COL_PATH));
            }
            else
            {
                quint32 fid = child->data(COL_NAME, Qt::UserRole).toUInt();
                Function *func = m_doc->function(fid);
                if (func != NULL)
                    func->setPath(root->text(COL_PATH));
            }
        }
        else
        {
            slotItemChanged(child);
        }
    }
}

const char* FunctionsTreeWidget::functionDragMimeType()
{
    return FUNCTION_DRAG_MIME_TYPE;
}

const char* FunctionsTreeWidget::paletteDragMimeType()
{
    return PALETTE_DRAG_MIME_TYPE;
}

void FunctionsTreeWidget::setExternalDragMode(bool enable)
{
    m_externalDragMode = enable;

    // External mode (Programming tab): drag palettes OUT to the looks canvas,
    // and also accept palette drops onto folders within this tree. DragDrop
    // (not DragOnly) is required to receive those internal drops.
    // Internal mode (Function Manager): classic InternalMove folder editing.
    if (enable)
        setDragDropMode(QAbstractItemView::DragDrop);
    else
        setDragDropMode(QAbstractItemView::InternalMove);
}

Qt::DropActions FunctionsTreeWidget::supportedDropActions() const
{
    // When in external drag mode, only support copy actions
    // This prevents items from being removed from the tree when dragged to external widgets
    if (m_externalDragMode)
        return Qt::CopyAction;

    // For internal drag (folder reordering), support move actions
    return Qt::MoveAction;
}

QMimeData* FunctionsTreeWidget::mimeData(const QList<QTreeWidgetItem*> items) const
{
    // If external drag mode is enabled, provide our custom MIME data
    // This is used by Qt's built-in drag when setDragEnabled(true) is called
    if (m_externalDragMode)
    {
        // Functions and palettes get separate payloads/MIME types so each
        // drop target picks only what it understands (function targets
        // never see palette IDs and vice-versa).
        QByteArray fnData;
        QDataStream fnStream(&fnData, QIODevice::WriteOnly);
        int fnCount = 0;
        QByteArray palData;
        QDataStream palStream(&palData, QIODevice::WriteOnly);
        int palCount = 0;

        foreach (QTreeWidgetItem *item, items)
        {
            if (itemNodeKind(item) == PaletteNode)
            {
                const quint32 pid = itemPaletteId(item);
                if (pid != QLCPalette::invalidId())
                {
                    palStream << pid;
                    palCount++;
                }
                continue;
            }
            QVariant var = item->data(COL_NAME, Qt::UserRole);
            if (var.isValid())
            {
                quint32 fid = var.toUInt();
                if (fid != Function::invalidId())
                {
                    fnStream << fid;
                    fnCount++;
                }
            }
        }

        if (fnCount > 0 || palCount > 0)
        {
            QMimeData *mimeData = new QMimeData();
            if (fnCount > 0)
                mimeData->setData(FUNCTION_DRAG_MIME_TYPE, fnData);
            if (palCount > 0)
                mimeData->setData(PALETTE_DRAG_MIME_TYPE, palData);
            return mimeData;
        }
    }

    // Fall back to default behavior
    return QTreeWidget::mimeData(items);
}

void FunctionsTreeWidget::mousePressEvent(QMouseEvent *event)
{
    QTreeWidget::mousePressEvent(event);

    m_draggedItems = selectedItems();
    m_dragStartPosition = event->pos();
}

void FunctionsTreeWidget::mouseMoveEvent(QMouseEvent *event)
{
    // If external drag mode is enabled, let Qt handle the drag with our mimeData
    if (m_externalDragMode)
    {
        QTreeWidget::mouseMoveEvent(event);
        return;
    }

    // Check if this is a drag motion with sufficient distance
    if (!(event->buttons() & Qt::LeftButton))
    {
        QTreeWidget::mouseMoveEvent(event);
        return;
    }

    if ((event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance())
    {
        QTreeWidget::mouseMoveEvent(event);
        return;
    }

    // Check if Ctrl (or Cmd on Mac) is held for external drag
#ifdef Q_OS_MACOS
    bool modifierPressed = event->modifiers() & Qt::MetaModifier;
#else
    bool modifierPressed = event->modifiers() & Qt::ControlModifier;
#endif

    if (modifierPressed && !m_draggedItems.isEmpty())
    {
        // Start external drag to other widgets (e.g., CollectionEditor)
        startExternalDrag();
    }
    else
    {
        // Let Qt handle internal drag (folder reordering)
        QTreeWidget::mouseMoveEvent(event);
    }
}

void FunctionsTreeWidget::startExternalDrag()
{
    // Collect function and palette IDs from selected items into separate
    // payloads (each drop target reads only the MIME type it understands).
    QByteArray fnData;
    QDataStream fnStream(&fnData, QIODevice::WriteOnly);
    int fnCount = 0;
    QByteArray palData;
    QDataStream palStream(&palData, QIODevice::WriteOnly);
    int palCount = 0;

    foreach (QTreeWidgetItem *item, m_draggedItems)
    {
        if (itemNodeKind(item) == PaletteNode)
        {
            const quint32 pid = itemPaletteId(item);
            if (pid != QLCPalette::invalidId())
            {
                palStream << pid;
                palCount++;
            }
            continue;
        }
        QVariant var = item->data(COL_NAME, Qt::UserRole);
        if (var.isValid())
        {
            quint32 fid = var.toUInt();
            Function *func = m_doc->function(fid);
            if (func != NULL)
            {
                fnStream << fid;
                fnCount++;
            }
        }
    }

    if (fnCount == 0 && palCount == 0)
        return;

    // Create mime data with function and/or palette IDs
    QMimeData *mimeData = new QMimeData();
    if (fnCount > 0)
        mimeData->setData(FUNCTION_DRAG_MIME_TYPE, fnData);
    if (palCount > 0)
        mimeData->setData(PALETTE_DRAG_MIME_TYPE, palData);

    // Create and execute the drag
    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);

    // Set a visual hint - use the first function's icon
    if (!m_draggedItems.isEmpty())
    {
        QTreeWidgetItem *firstItem = m_draggedItems.first();
        QIcon icon = firstItem->icon(COL_NAME);
        if (!icon.isNull())
            drag->setPixmap(icon.pixmap(32, 32));
    }

    drag->exec(Qt::CopyAction);
}

void FunctionsTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_externalDragMode)
    {
        if (event->mimeData()->hasFormat(PALETTE_DRAG_MIME_TYPE))
            event->acceptProposedAction();
        else
            event->ignore();
        return;
    }
    QTreeWidget::dragEnterEvent(event);
}

void FunctionsTreeWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (m_externalDragMode)
    {
        if (event->mimeData()->hasFormat(PALETTE_DRAG_MIME_TYPE) == false)
        {
            event->ignore();
            return;
        }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QTreeWidgetItem *target = itemAt(event->pos());
#else
        QTreeWidgetItem *target = itemAt(event->position().toPoint());
#endif
        // A folder, a palette leaf (join its folder), the Palettes category,
        // or the empty root are valid targets.
        if (target == NULL || itemNodeKind(target) == PaletteNode)
            event->acceptProposedAction();
        else
            event->ignore();
        return;
    }
    QTreeWidget::dragMoveEvent(event);
}

void FunctionsTreeWidget::dropEvent(QDropEvent *event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QTreeWidgetItem *dropItem = itemAt(event->pos());
#else
    QTreeWidgetItem *dropItem = itemAt(event->position().toPoint());
#endif

    // External/Programming mode: move the dragged palettes into the folder
    // under the cursor (don't do any Qt item reparenting; the owner rebuilds).
    if (m_externalDragMode)
    {
        if (event->mimeData()->hasFormat(PALETTE_DRAG_MIME_TYPE) == false)
        {
            event->ignore();
            return;
        }

        // Resolve the destination folder path.
        QString destPath;
        if (dropItem == NULL)
            destPath = QString(); // root
        else if (itemPaletteId(dropItem) != QLCPalette::invalidId())
            destPath = (dropItem->parent() != NULL)
                    ? dropItem->parent()->text(COL_PATH) : QString(); // join sibling's folder
        else
            destPath = dropItem->text(COL_PATH); // a folder (or Palettes root => "")

        QList<quint32> ids;
        QByteArray data = event->mimeData()->data(PALETTE_DRAG_MIME_TYPE);
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            quint32 pid = 0;
            stream >> pid;
            ids << pid;
        }

        bool changed = false;
        foreach (quint32 pid, ids)
        {
            QLCPalette *p = m_doc->palette(pid);
            if (p != NULL && p->path() != destPath)
            {
                p->setPath(destPath);
                changed = true;
            }
        }
        m_draggedItems.clear();
        event->acceptProposedAction();
        if (changed)
            emit paletteDroppedToFolder();
        return;
    }

    if (m_draggedItems.count() == 0 || dropItem == NULL)
        return;

    QVariant var = dropItem->data(COL_NAME, Qt::UserRole + 1);
    if (var.isValid() == false)
        return;

    int dropType = var.toInt();
    NodeKind dropKind = itemNodeKind(dropItem);
    //QString folderName = dropItem->text(COL_PATH);

    foreach (QTreeWidgetItem *item, m_draggedItems)
    {
        NodeKind dragKind = itemNodeKind(item);

        // Never mix functions and palettes: a palette can only land in a
        // palette folder and vice-versa.
        if (dragKind != dropKind)
            continue;

        if (dragKind == PaletteNode)
        {
            // itemPaletteId() is non-invalid only for leaves; folders
            // fall through to the path-propagation branch.
            const quint32 pid = itemPaletteId(item);
            QTreeWidget::dropEvent(event);
            if (pid != QLCPalette::invalidId())
            {
                QLCPalette *p = m_doc->palette(pid);
                if (p != NULL)
                    p->setPath(dropItem->text(COL_PATH));
            }
            else
            {
                // m_draggedItem is a palette folder
                slotItemChanged(item);
            }
            continue;
        }

        quint32 dragFID = item->data(COL_NAME, Qt::UserRole).toUInt();
        Function *dragFunc = m_doc->function(dragFID);
        if (dragFunc != NULL && dragFunc->type() == dropType)
        {
            QTreeWidget::dropEvent(event);
            quint32 fid = item->data(COL_NAME, Qt::UserRole).toUInt();
            Function *func = m_doc->function(fid);
            if (func != NULL)
                func->setPath(dropItem->text(COL_PATH));
        }
        else
        {
            // m_draggedItem is a folder
            int dragType = item->data(COL_NAME, Qt::UserRole + 1).toInt();
            if (dragType == dropType)
                QTreeWidget::dropEvent(event);
            slotItemChanged(item);
        }
    }

    m_draggedItems.clear();
}

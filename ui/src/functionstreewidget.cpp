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
#include <QKeyEvent>
#include <QDrag>
#include <QPainter>
#include <QTimer>

#include "functionstreewidget.h"
#include "showtreecontroller.h"
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

// Resolve a drop target item to the Collection it represents, so a function
// dropped there is added to that collection. The target may be the collection's
// own node, or one of its nested member rows (drop "into" the expanded list) —
// walk up until a Collection is found. Returns invalidId if none.
//
// Reads Qt::UserRole directly rather than itemFunctionId(), which rejects
// top-level items (parent == NULL) and so would miss a collection at the root.
static quint32 collectionForDropTarget(Doc *doc, QTreeWidgetItem *item)
{
    for (QTreeWidgetItem *it = item; it != NULL; it = it->parent())
    {
        const QVariant var = it->data(COL_NAME, Qt::UserRole);
        if (var.isValid() == false)
            continue;
        Function *f = doc->function(var.toUInt());
        if (f != NULL && f->type() == Function::CollectionType)
            return f->id();
    }
    return Function::invalidId();
}
// MIME type for palette drag/drop to external widgets
static const char* PALETTE_DRAG_MIME_TYPE = "application/x-qlcplus-palettes";

// MIME type for dragging folders into other folders (Programming tab / external
// mode). Stream of (int NodeKind, QString full folder path).
static const char* FOLDER_DRAG_MIME_TYPE = "application/x-qlcconsole-folders";

// Folder paths carry a trailing '/' on category nodes ("Scene/") and none on
// sub-folders ("Scene/Act 1"); compare them without it.
static QString strippedPath(QString path)
{
    while (path.endsWith('/'))
        path.chop(1);
    return path;
}

// Full folder path a function lives in, e.g. "Scene" or "Scene/Act 1".
static QString functionFullPath(const Function *f)
{
    const QString rel = strippedPath(f->path(true));
    const QString type = Function::typeToString(f->type());
    return rel.isEmpty() ? type : type + "/" + rel;
}

// Full folder path a palette lives in, e.g. "Palettes" or "Palettes/Color".
static QString paletteFullPath(const QLCPalette *p)
{
    const QString path = strippedPath(p->path());
    return path.isEmpty() ? QString(PALETTE_CATEGORY) : path;
}

static void setFunctionFullPath(Function *f, const QString &full)
{
    // Function::setPath() strips the "Type/" prefix; the bare category needs
    // its slash so that happens ("Scene/" -> "").
    const QString type = Function::typeToString(f->type());
    f->setPath(full == type ? type + "/" : full);
}

static void setPaletteFullPath(QLCPalette *p, const QString &full)
{
    // Stored like new palettes are ("Palettes/Color/"); the bare root is "".
    p->setPath(full == QString(PALETTE_CATEGORY) ? QString() : full + "/");
}

// Is $path the folder $folder itself or inside it?
static bool pathIsUnder(const QString &path, const QString &folder)
{
    return path == folder || path.startsWith(folder + "/");
}

// Top-level function-TYPE category node. When the owning tree has
// type-ordering-by-role enabled (typeOrderByRole()), categories sort by a
// fixed role rank (Show · Chaser · Collection · RGB Matrix · EFX · Scene · …)
// instead of alphabetically; otherwise it falls back to the classic name sort.
// Only the top-level category rows are this subclass, so leaves (plain
// QTreeWidgetItem) keep sorting by name within a category.
class FunctionCategoryItem : public QTreeWidgetItem
{
public:
    explicit FunctionCategoryItem(QTreeWidget *view) : QTreeWidgetItem(view) {}

    bool operator<(const QTreeWidgetItem &other) const override
    {
        const FunctionsTreeWidget *w =
            qobject_cast<const FunctionsTreeWidget *>(treeWidget());
        if (w != NULL && w->typeOrderByRole())
        {
            const int ra = FunctionsTreeWidget::typeSortRank(
                        data(COL_NAME, Qt::UserRole + 1).toInt());
            const int rb = FunctionsTreeWidget::typeSortRank(
                        other.data(COL_NAME, Qt::UserRole + 1).toInt());
            if (ra != rb)
                return ra < rb;
        }
        return text(COL_NAME) < other.text(COL_NAME);
    }
};

int FunctionsTreeWidget::typeSortRank(int type)
{
    // Lower = earlier. Matches the rig-building order the Programming tab wants.
    switch (type)
    {
    case Function::ShowType:       return 0;
    case Function::ChaserType:     return 1;
    case Function::CollectionType: return 2;
    case Function::RGBMatrixType:  return 3;
    case Function::EFXType:        return 4;
    case Function::SceneType:      return 5;
    case Function::ScriptType:     return 6;
    default:                       return 100 + type; // audio/video/unknown last
    }
}

void FunctionsTreeWidget::setTypeOrderByRole(bool enable)
{
    if (m_typeOrderByRole == enable)
        return;
    m_typeOrderByRole = enable;
    // Re-sort the top-level categories under the new comparator.
    sortItems(COL_NAME, Qt::AscendingOrder);
}

FunctionsTreeWidget::FunctionsTreeWidget(Doc *doc, QWidget *parent) :
    QTreeWidget(parent)
  , m_doc(doc)
  , m_displayFilter(ShowAll)
  , m_typeOrderByRole(false)
  , m_externalDragMode(false)
  , m_dropOnRoot(false)
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

void FunctionsTreeWidget::filterByText(const QString &text)
{
    // Walk every top-level category/folder and recursively show/hide items.
    // Returns true if any descendant is visible (so folders stay visible).
    std::function<bool(QTreeWidgetItem*)> applyFilter = [&](QTreeWidgetItem *item) -> bool {
        bool anyChildVisible = false;
        for (int i = 0; i < item->childCount(); ++i)
            anyChildVisible |= applyFilter(item->child(i));

        bool selfMatch = text.isEmpty() ||
                         item->text(COL_NAME).contains(text, Qt::CaseInsensitive);
        bool visible = selfMatch || anyChildVisible;
        item->setHidden(!visible);
        if (!text.isEmpty() && anyChildVisible)
            item->setExpanded(true);
        else if (text.isEmpty())
            item->setExpanded(false);
        return visible;
    };

    for (int i = 0; i < topLevelItemCount(); ++i)
        applyFilter(topLevelItem(i));
}

void FunctionsTreeWidget::updateTree()
{
    blockSignals(true);

    // Snapshot which folder paths are currently expanded so we can restore
    // them after the tree is rebuilt (clearTree wipes m_foldersMap).
    QSet<QString> expandedPaths;
    for (auto it = m_foldersMap.constBegin(); it != m_foldersMap.constEnd(); ++it)
        if (it.value() && it.value()->isExpanded())
            expandedPaths.insert(it.key());

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
            // A Show Tree one-off value: the tree's own, not a palette to file.
            if (m_doc->showTree()->isInlinePalette(palette->id()))
                continue;
            QTreeWidgetItem* parent = paletteParentItem(palette);
            if (parent != NULL)
                updatePaletteItem(new QTreeWidgetItem(parent), palette);
        }
    }

    // Restore expanded state.
    for (auto it = m_foldersMap.constBegin(); it != m_foldersMap.constEnd(); ++it)
        if (it.value() && expandedPaths.contains(it.key()))
            it.value()->setExpanded(true);

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
    // Made by the Show Tree (D13): named and filed by it, so not renamed
    // here. It still drags out to play (a VC Cue List, a show).
    if (m_doc->showTree()->isGenerated(function->id()))
    {
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setToolTip(COL_NAME, tr("%1 It is %2.")
                         .arg(ShowTreeController::lockedText(),
                              m_doc->showTree()->describeGenerated(function->id())));
    }

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
        // Parent item for the given type doesn't exist yet so create one.
        // FunctionCategoryItem so type-order-by-role can control sibling order.
        QTreeWidgetItem* item = new FunctionCategoryItem(this);
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

    // Empty sub-path: function lives at the top of its type category.
    // folderItem("") can't locate the category node (it's keyed as "RGB Matrix/"),
    // so fall back to the category we just ensured exists above.
    if (function->path(true).isEmpty())
        return m_foldersMap.value(QString(basePath + "/"), nullptr);

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

    QString path = palette->path();
    // Paths are stored with a trailing '/' (e.g. "Palettes/Color/") but
    // folderItem() returns NULL for any name ending in '/', so strip it.
    while (path.endsWith('/'))
        path.chop(1);

    const QString basePath = QString(PALETTE_CATEGORY);
    if (path.isEmpty() || path == basePath)
        return category;

    QTreeWidgetItem* pItem = ensurePaletteFolder(path);
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
    if (palette == NULL || m_doc->showTree()->isInlinePalette(pid))
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
    if (item == NULL)
        return QLCPalette::invalidId();
    // In PalettesOnly mode the category IS the invisibleRootItem, so top-level
    // items (parent()==nullptr) are valid palette leaves — don't reject them.
    if (item->parent() == NULL && m_displayFilter != PalettesOnly)
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
            // The Show Tree's own goes with its cue list, there (D13).
            if (m_doc->showTree()->isGenerated(fid))
                continue;
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
                // In a palettes-only tree the "Palettes/" category is the
                // invisible root, which carries no kind: its folders are
                // still palette folders (or palette drops onto them fail).
                kind = (parentNode == invisibleRootItem())
                        ? PaletteNode : itemNodeKind(parentNode);
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
            quint32 pid = itemPaletteId(item);
            QLCPalette *p = m_doc->palette(pid);
            if (p != NULL && !newName.isEmpty() && p->name() != newName)
            {
                p->setName(newName);
                m_doc->setModified();
                emit paletteRenamed(pid);
            }
            blockSignals(false);
            return;
        }

        /* Only a row that really carries a function id. A folder row fires
           itemChanged from its first setText(), before folderItem() has given
           it any id or path -- and an unset id reads as 0, so creating a
           palette or function in a NEW folder renamed function 0 (the
           workspace's first look) to the folder's name. */
        const QVariant idData = item->data(COL_NAME, Qt::UserRole);
        const quint32 fid = idData.isValid() ? idData.toUInt() : Function::invalidId();
        if (fid != Function::invalidId())
        {
            Function *fn = m_doc->function(fid);
            if (fn != NULL && m_doc->showTree()->isGenerated(fid))
            {
                // Named by the Show Tree (D13): the row goes back.
                item->setText(COL_NAME, fn->name());
            }
            else if (fn != NULL && !newName.isEmpty() && fn->name() != newName)
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

        const bool renamed = (item->text(COL_PATH) != fullPath);
        m_foldersMap.remove(item->text(COL_PATH));
        item->setText(COL_PATH, fullPath);
        m_foldersMap[fullPath] = item;
        slotUpdateChildrenPath(item);
        // A folder rename re-files whatever is inside it: save-worthy.
        if (renamed && item->childCount() > 0)
            m_doc->setModified();
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
                // The Show Tree files its own (D13).
                if (func != NULL && !m_doc->showTree()->isGenerated(fid))
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

const char* FunctionsTreeWidget::folderDragMimeType()
{
    return FOLDER_DRAG_MIME_TYPE;
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

QMimeData* FunctionsTreeWidget::buildMimeData(const QList<QTreeWidgetItem*> &items) const
{
    // If external drag mode is enabled, provide our custom MIME data
    // This is used by Qt's built-in drag when setDragEnabled(true) is called
    if (m_externalDragMode)
    {
        // Functions, palettes and folders get separate payloads/MIME types so
        // each drop target picks only what it understands (function targets
        // never see palette IDs and vice-versa).
        QByteArray fnData;
        QDataStream fnStream(&fnData, QIODevice::WriteOnly);
        int fnCount = 0;
        QByteArray palData;
        QDataStream palStream(&palData, QIODevice::WriteOnly);
        int palCount = 0;
        QByteArray folderData;
        QDataStream folderStream(&folderData, QIODevice::WriteOnly);
        int folderCount = 0;

        foreach (QTreeWidgetItem *item, items)
        {
            if (item == NULL)
                continue;
            if (isUserFolder(item))
            {
                folderStream << int(itemNodeKind(item)) << strippedPath(item->text(COL_PATH));
                folderCount++;
                continue;
            }
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
                if (fid != Function::invalidId() && m_doc->function(fid) != NULL)
                {
                    fnStream << fid;
                    fnCount++;
                }
            }
        }

        if (fnCount > 0 || palCount > 0 || folderCount > 0)
        {
            QMimeData *mimeData = new QMimeData();
            if (fnCount > 0)
                mimeData->setData(FUNCTION_DRAG_MIME_TYPE, fnData);
            if (palCount > 0)
                mimeData->setData(PALETTE_DRAG_MIME_TYPE, palData);
            if (folderCount > 0)
                mimeData->setData(FOLDER_DRAG_MIME_TYPE, folderData);
            return mimeData;
        }
        return NULL;
    }

    // Fall back to default behavior
    return QTreeWidget::mimeData(items);
}

void FunctionsTreeWidget::keyPressEvent(QKeyEvent *event)
{
    // Enter/Return/F2 → inline rename of the current row (functions, palettes and
    // folders are all ItemIsEditable; slotItemChanged persists the new name).
    // Activation here is double-click only, so Enter is free to mean "rename".
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
         || event->key() == Qt::Key_F2)
        && state() != QAbstractItemView::EditingState)
    {
        QTreeWidgetItem *it = currentItem();
        if (it != NULL && (it->flags() & Qt::ItemIsEditable)
            && selectedItems().size() <= 1)
        {
            editItem(it, COL_NAME);
            event->accept();
            return;
        }
    }
    QTreeWidget::keyPressEvent(event);
}

void FunctionsTreeWidget::mousePressEvent(QMouseEvent *event)
{
    // Preserve a multi-selection when the user right-clicks INSIDE it. Qt's
    // default collapses the selection to the clicked row on a right-press, which
    // would make a context-menu Delete/Duplicate act on one item instead of the
    // several the user had selected. Remember the selection, let the base class
    // run, then restore it if it was collapsed.
    QList<QTreeWidgetItem*> keepSelection;
    QTreeWidgetItem *rightClicked = NULL;
    if (event->button() == Qt::RightButton)
    {
        rightClicked = itemAt(event->pos());
        if (rightClicked != NULL && rightClicked->isSelected()
                && selectedItems().size() > 1)
            keepSelection = selectedItems();
    }

    QTreeWidget::mousePressEvent(event);

    if (keepSelection.size() > 1 && selectedItems() != keepSelection)
    {
        const bool wasBlocked = blockSignals(true);
        clearSelection();
        const auto &const_keepSelection = keepSelection;
        for (QTreeWidgetItem *it : const_keepSelection)
            it->setSelected(true);
        if (rightClicked != NULL)
            setCurrentItem(rightClicked);
        blockSignals(wasBlocked);
    }

    m_draggedItems = selectedItems();
    m_dragStartPosition = event->pos();
}

void FunctionsTreeWidget::startDrag(Qt::DropActions supportedActions)
{
    // External-drag consumers (Collection/Chaser/Show editors, the show
    // timeline) place a block at the drop point. Qt's default drag pixmap is
    // the whole rendered row with its hotspot at the grab point (often mid-row),
    // so the block lands to the RIGHT of the ghost by the grab offset. Use our
    // own small ghost pinned to the cursor (hotspot 0,0) instead.
    if (m_externalDragMode)
    {
        startExternalDrag();
        return;
    }
    QTreeWidget::startDrag(supportedActions);
}

void FunctionsTreeWidget::mouseMoveEvent(QMouseEvent *event)
{
    // If external drag mode is enabled, let Qt handle the drag with our mimeData
    // (which routes through our startDrag() override for a cursor-anchored ghost).
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
    // Same payload Qt's own drag would carry (buildMimeData), so a drop target
    // never sees a difference between the two ways a drag can start.
    const bool wasExternal = m_externalDragMode;
    m_externalDragMode = true;
    QMimeData *mimeData = buildMimeData(m_draggedItems);
    m_externalDragMode = wasExternal;
    if (mimeData == NULL)
        return;

    // Create and execute the drag
    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);

    // Set a visual hint - use the first function's icon, anchored at the cursor
    // (hotspot at the pixmap's top-left) so the drop lands under the pointer.
    if (!m_draggedItems.isEmpty())
    {
        QTreeWidgetItem *firstItem = m_draggedItems.first();
        QIcon icon = firstItem->icon(COL_NAME);
        if (!icon.isNull())
            drag->setPixmap(icon.pixmap(32, 32));
    }
    drag->setHotSpot(QPoint(0, 0));

    drag->exec(Qt::CopyAction);
}

void FunctionsTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_externalDragMode)
    {
        if (event->mimeData()->hasFormat(PALETTE_DRAG_MIME_TYPE)
                || event->mimeData()->hasFormat(FUNCTION_DRAG_MIME_TYPE)
                || event->mimeData()->hasFormat(FOLDER_DRAG_MIME_TYPE))
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
        // Let the view auto-scroll and auto-expand a hovered folder; it ignores
        // our MIME types itself, so decide acceptance afterwards.
        QTreeWidget::dragMoveEvent(event);

        QTreeWidgetItem *target = itemAt(event->position().toPoint());
        const bool ok = canDropMimeOn(event->mimeData(), target);
        if (ok)
            event->acceptProposedAction();
        else
            event->ignore();
        viewport()->update();
        return;
    }
    QTreeWidget::dragMoveEvent(event);
}

void FunctionsTreeWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_dropHighlight = QPersistentModelIndex();
    m_dropOnRoot = false;
    viewport()->update();
    QTreeWidget::dragLeaveEvent(event);
}

void FunctionsTreeWidget::paintEvent(QPaintEvent *event)
{
    QTreeWidget::paintEvent(event);

    if (m_dropOnRoot == false && m_dropHighlight.isValid() == false)
        return;

    QPainter painter(viewport());
    QColor c = palette().color(QPalette::Highlight);
    painter.setPen(QPen(c, 2));
    c.setAlpha(40);
    painter.setBrush(c);
    QRect r;
    if (m_dropHighlight.isValid())
    {
        r = visualRect(m_dropHighlight);
        r.setLeft(0);
        r.setRight(viewport()->width() - 1);
    }
    else
    {
        r = viewport()->rect();
    }
    painter.drawRoundedRect(r.adjusted(1, 1, -1, -1), 3, 3);
}

QString FunctionsTreeWidget::dropFolderPath(QTreeWidgetItem *target, NodeKind kind,
                                            const QString &typeName) const
{
    // Empty space: the top level of the item's own category.
    if (target == NULL)
        return typeName;

    if (itemNodeKind(target) != kind)
        return QString();

    // A leaf joins its sibling's folder.
    QTreeWidgetItem *folder = target;
    if (target->text(COL_PATH).isEmpty())
    {
        folder = target->parent();
        // Top-level leaf: only palettes (palettes-only tree) live there.
        if (folder == NULL)
            return kind == PaletteNode ? typeName : QString();
        // A nested collection member row isn't a folder.
        if (folder->text(COL_PATH).isEmpty())
            return QString();
    }

    const QString path = strippedPath(folder->text(COL_PATH));
    // Functions stay within their own type's category.
    if (path != typeName && path.startsWith(typeName + "/") == false)
        return QString();
    return path;
}

bool FunctionsTreeWidget::handleFolderDrop(const QMimeData *mime, QTreeWidgetItem *target,
                                           bool apply)
{
    QTreeWidgetItem *highlight = NULL;
    bool any = false;
    bool changed = false;
    QStringList keepFolders;
    QString reveal;

    auto noteDest = [&](const QString &dest) {
        any = true;
        if (reveal.isEmpty())
            reveal = dest;
        if (highlight == NULL)
            highlight = m_foldersMap.value(dest + "/", m_foldersMap.value(dest, NULL));
    };

    if (mime->hasFormat(FUNCTION_DRAG_MIME_TYPE))
    {
        QByteArray data = mime->data(FUNCTION_DRAG_MIME_TYPE);
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            quint32 fid = Function::invalidId();
            stream >> fid;
            Function *f = m_doc->function(fid);
            if (f == NULL)
                continue;
            const QString dest = dropFolderPath(target, FunctionNode,
                                                Function::typeToString(f->type()));
            if (dest.isEmpty())
                continue;
            noteDest(dest);
            if (apply && functionFullPath(f) != dest)
            {
                setFunctionFullPath(f, dest);
                changed = true;
            }
        }
    }

    if (mime->hasFormat(PALETTE_DRAG_MIME_TYPE))
    {
        QByteArray data = mime->data(PALETTE_DRAG_MIME_TYPE);
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            quint32 pid = QLCPalette::invalidId();
            stream >> pid;
            QLCPalette *p = m_doc->palette(pid);
            if (p == NULL)
                continue;
            const QString dest = dropFolderPath(target, PaletteNode, PALETTE_CATEGORY);
            if (dest.isEmpty())
                continue;
            noteDest(dest);
            if (apply && paletteFullPath(p) != dest)
            {
                setPaletteFullPath(p, dest);
                changed = true;
            }
        }
    }

    if (mime->hasFormat(FOLDER_DRAG_MIME_TYPE))
    {
        QByteArray data = mime->data(FOLDER_DRAG_MIME_TYPE);
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            int kind = FunctionNode;
            QString oldPath;
            stream >> kind >> oldPath;
            const int slash = oldPath.lastIndexOf('/');
            if (slash <= 0)
                continue; // a category, never movable
            const QString category = oldPath.left(oldPath.indexOf('/'));
            const QString dest = dropFolderPath(target, NodeKind(kind), category);
            // Not into itself or one of its own sub-folders.
            if (dest.isEmpty() || pathIsUnder(dest, oldPath))
                continue;
            noteDest(dest);
            const QString newPath = dest + oldPath.mid(slash);
            if (apply && newPath != oldPath)
            {
                keepFolders << reRootFolder(oldPath, newPath);
                changed = true;
            }
        }
    }

    if (apply == false)
    {
        m_dropOnRoot = any && highlight == invisibleRootItem();
        m_dropHighlight = (any && highlight != NULL && highlight != invisibleRootItem())
                ? QPersistentModelIndex(indexFromItem(highlight)) : QPersistentModelIndex();
        // Dropping on empty space of a palettes-only tree = its root.
        if (any && target == NULL && m_displayFilter == PalettesOnly)
            m_dropOnRoot = true;
    }
    else if (changed)
    {
        m_doc->setModified();
        scheduleRebuild(keepFolders, reveal);
    }
    return any;
}

bool FunctionsTreeWidget::canDropMimeOn(const QMimeData *mime, QTreeWidgetItem *target)
{
    m_dropHighlight = QPersistentModelIndex();
    m_dropOnRoot = false;
    if (mime == NULL)
        return false;

    // Function(s) onto a Collection node (or into its expanded member list)
    // add them to that collection.
    if (mime->hasFormat(FUNCTION_DRAG_MIME_TYPE))
    {
        const quint32 cid = collectionForDropTarget(m_doc, target);
        if (cid != Function::invalidId())
        {
            QTreeWidgetItem *node = target;
            while (node != NULL && node->data(COL_NAME, Qt::UserRole).toUInt() != cid)
                node = node->parent();
            if (node != NULL)
                m_dropHighlight = QPersistentModelIndex(indexFromItem(node));
            return true;
        }
    }

    // Otherwise into the folder under the cursor.
    return handleFolderDrop(mime, target, false);
}

bool FunctionsTreeWidget::dropMimeOn(const QMimeData *mime, QTreeWidgetItem *target)
{
    m_dropHighlight = QPersistentModelIndex();
    m_dropOnRoot = false;
    viewport()->update();
    if (mime == NULL)
        return false;

    // Function(s) dropped onto a Collection node → add them as members. The
    // owner (ProgrammingManager) performs the membership change and refresh;
    // dedup and circular-reference checks live there.
    if (mime->hasFormat(FUNCTION_DRAG_MIME_TYPE))
    {
        const quint32 containerId = collectionForDropTarget(m_doc, target);
        if (containerId != Function::invalidId())
        {
            QList<quint32> fids;
            QByteArray data = mime->data(FUNCTION_DRAG_MIME_TYPE);
            QDataStream stream(&data, QIODevice::ReadOnly);
            while (stream.atEnd() == false)
            {
                quint32 fid = 0;
                stream >> fid;
                fids << fid;
            }
            if (fids.isEmpty() == false)
                emit functionsDroppedOnCollection(containerId, fids);
            return true;
        }
    }

    // Otherwise: move the dragged functions / palettes / folders into the
    // folder under the cursor (no Qt item reparenting; the tree rebuilds).
    return handleFolderDrop(mime, target, true);
}

QStringList FunctionsTreeWidget::reRootFolder(const QString &oldPath, const QString &newPath)
{
    // Folders under oldPath, re-rooted: recreated after the rebuild so empty
    // ones don't vanish (folders only exist through what's filed in them).
    QStringList keep;
    foreach (const QString &key, m_foldersMap.keys())
    {
        const QString k = strippedPath(key);
        if (key.endsWith('/') == false && pathIsUnder(k, oldPath))
            keep << newPath + k.mid(oldPath.length());
    }

    if (oldPath.startsWith(QString(PALETTE_CATEGORY) + "/"))
    {
        foreach (QLCPalette *p, m_doc->palettes())
        {
            if (p == NULL)
                continue;
            const QString full = paletteFullPath(p);
            if (pathIsUnder(full, oldPath))
                setPaletteFullPath(p, newPath + full.mid(oldPath.length()));
        }
    }
    else
    {
        foreach (Function *f, m_doc->functions())
        {
            const QString full = functionFullPath(f);
            if (pathIsUnder(full, oldPath))
                setFunctionFullPath(f, newPath + full.mid(oldPath.length()));
        }
    }
    return keep;
}

void FunctionsTreeWidget::scheduleRebuild(const QStringList &keepFolders, const QString &revealPath)
{
    QTimer::singleShot(0, this, [this, keepFolders, revealPath]() {
        updateTree();
        clearSelection();
        foreach (const QString &path, keepFolders)
        {
            // Only within a category the tree shows (folderItem() would
            // otherwise invent a top-level node).
            const QString category = path.left(path.indexOf('/'));
            if (m_foldersMap.contains(category + "/") && path.contains('/'))
                folderItem(path);
        }
        QTreeWidgetItem *dest = m_foldersMap.value(revealPath,
                                                   m_foldersMap.value(revealPath + "/", NULL));
        if (dest != NULL && dest != invisibleRootItem())
        {
            for (QTreeWidgetItem *it = dest; it != NULL; it = it->parent())
                it->setExpanded(true);
            scrollToItem(dest);
        }
        emit folderContentsMoved();
    });
}

bool FunctionsTreeWidget::isUserFolder(const QTreeWidgetItem *item) const
{
    if (item == NULL || item == invisibleRootItem())
        return false;
    const QString path = item->text(COL_PATH);
    // Leaves have no path; categories end in '/' ("Scene/", "Palettes/").
    return path.isEmpty() == false && path.endsWith('/') == false;
}

int FunctionsTreeWidget::folderContentCount(const QTreeWidgetItem *item) const
{
    if (isUserFolder(item) == false)
        return 0;
    const QString folder = strippedPath(item->text(COL_PATH));
    int count = 0;
    if (itemNodeKind(item) == PaletteNode)
    {
        foreach (QLCPalette *p, m_doc->palettes())
            if (p != NULL && pathIsUnder(paletteFullPath(p), folder))
                count++;
    }
    else
    {
        foreach (Function *f, m_doc->functions())
            if (pathIsUnder(functionFullPath(f), folder))
                count++;
    }
    return count;
}

void FunctionsTreeWidget::removeFolderKeepContents(QTreeWidgetItem *item)
{
    if (isUserFolder(item) == false)
        return;
    const QString oldPath = strippedPath(item->text(COL_PATH));
    const QString parentPath = oldPath.left(oldPath.lastIndexOf('/'));
    const bool hadContents = folderContentCount(item) > 0;

    // Everything inside moves up one level; the folder itself is dropped from
    // the re-rooted list (it becomes the parent).
    QStringList keep = reRootFolder(oldPath, parentPath);
    keep.removeAll(parentPath);
    if (hadContents)
        m_doc->setModified();
    scheduleRebuild(keep, parentPath);
}

void FunctionsTreeWidget::dropEvent(QDropEvent *event)
{
    QTreeWidgetItem *dropItem = itemAt(event->position().toPoint());

    // External/Programming mode.
    if (m_externalDragMode)
    {
        m_draggedItems.clear();
        // dragMoveEvent() let the base view auto-scroll; end that here as
        // QAbstractItemView::dropEvent() would.
        stopAutoScroll();
        setState(NoState);
        if (dropMimeOn(event->mimeData(), dropItem))
            event->acceptProposedAction();
        else
            event->ignore();
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
        // The Show Tree files its own (D13): not moved to another folder.
        if (dragFunc != NULL && m_doc->showTree()->isGenerated(dragFID))
            continue;
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

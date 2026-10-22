/*
  Q Light Controller Plus
  fixturegroupsource.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QMimeData>
#include <QDataStream>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QTreeWidgetItemIterator>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QMap>
#include <QTimer>

#include <algorithm>
#include <cmath>

#include "createfixturegroup.h"
#include "itemicons.h"
#include "fixturegroupsource.h"
#include "inputoutputmap.h"
#include "universe.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "doc.h"

static const char* FIXTUREGROUP_DRAG_MIME_TYPE = "application/x-qlcplus-fixturegroups";
static const char* FIXTURE_DRAG_MIME_TYPE      = "application/x-qlcplus-fixtures";

FixtureGroupSource::FixtureGroupSource(Doc *doc, QWidget *parent)
    : QTreeWidget(parent)
    , m_doc(doc)
{
    setHeaderHidden(true);
    setRootIsDecorated(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDropIndicatorShown(true);
    // CopyAction so the view never auto-removes the dragged source rows; the
    // "move" is just a group path change (and dragging out to the canvas must
    // also stay non-destructive).
    setDefaultDropAction(Qt::CopyAction);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotContextMenu(QPoint)));
    connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotItemDoubleClicked(QTreeWidgetItem*,int)));
    connect(this, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            this, SLOT(slotItemChanged(QTreeWidgetItem*,int)));

    reload();

    // Don't rebuild while the Doc is tearing down / clearing: it emits
    // fixtureRemoved per fixture during clearContents(), but Doc::fixtures()
    // returns a cache that still holds the already-freed pointers until the
    // loop ends — iterating it here would touch freed memory (shutdown crash).
    connect(m_doc, &Doc::clearing, this, [this]() { m_clearing = true; });
    connect(m_doc, &Doc::loaded,   this, [this]() { m_clearing = false; reload(); });

    // Mirror the Fixture Manager: refresh whenever fixtures or groups change.
    connect(m_doc, SIGNAL(fixtureAdded(quint32)), this, SLOT(reload()));
    connect(m_doc, SIGNAL(fixtureRemoved(quint32)), this, SLOT(reload()));
    connect(m_doc, SIGNAL(fixtureChanged(quint32)), this, SLOT(reload()));
    connect(m_doc, SIGNAL(fixtureGroupAdded(quint32)), this, SLOT(reload()));
    connect(m_doc, SIGNAL(fixtureGroupRemoved(quint32)), this, SLOT(reload()));
    connect(m_doc, SIGNAL(fixtureGroupChanged(quint32)), this, SLOT(reload()));
}

const char* FixtureGroupSource::fixtureGroupMimeType()
{
    return FIXTUREGROUP_DRAG_MIME_TYPE;
}

const char* FixtureGroupSource::fixtureMimeType()
{
    return FIXTURE_DRAG_MIME_TYPE;
}

QTreeWidgetItem *FixtureGroupSource::folderItem(const QString &path)
{
    if (path.isEmpty())
        return invisibleRootItem();
    if (m_folderMap.contains(path))
        return m_folderMap[path];

    const int slash = path.lastIndexOf('/');
    const QString parentPath = (slash < 0) ? QString() : path.left(slash);
    const QString name = (slash < 0) ? path : path.mid(slash + 1);

    QTreeWidgetItem *fi = new QTreeWidgetItem(folderItem(parentPath));
    fi->setText(0, name);
    fi->setIcon(0, QIcon(":/folder.png"));
    fi->setData(0, KindRole, CategoryNode);
    fi->setData(0, PathRole, path); // lets group drops resolve the folder path
    fi->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_folderMap[path] = fi;
    return fi;
}

void FixtureGroupSource::reload()
{
    if (m_clearing)
        return; // doc is tearing down; its fixture list cache holds freed ptrs

    clear();
    m_folderMap.clear();

    const Qt::ItemFlags leafFlags =
        Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;

    // One tree: groups (nested in their folders) each with member fixtures
    // under them. Drag a group -> dynamic target; a fixture -> fixed target.
    QList<FixtureGroup*> groups;
    foreach (FixtureGroup *g, m_doc->fixtureGroups())
        if (g != NULL)
            groups.append(g);
    std::sort(groups.begin(), groups.end(),
              [](FixtureGroup *a, FixtureGroup *b) {
                  return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
              });

    foreach (FixtureGroup *g, groups)
    {
        const QList<quint32> members = g->fixtureList();
        QTreeWidgetItem *gi = new QTreeWidgetItem(folderItem(g->path()));
        gi->setText(0, tr("%1  (%n fixture(s))", "", members.count()).arg(g->name()));
        gi->setIcon(0, ItemIcons::groupIcon(m_doc, g));
        gi->setData(0, IdRole, g->id());
        gi->setData(0, KindRole, GroupNode);
        gi->setData(0, NameRole, g->name());   // plain name for inline rename
        gi->setFlags(leafFlags);               // ItemIsEditable armed on demand

        QList<Fixture*> members2;
        foreach (quint32 fid, members)
        {
            Fixture *f = m_doc->fixture(fid);
            if (f != NULL)
                members2.append(f);
        }
        std::sort(members2.begin(), members2.end(),
                  [](Fixture *a, Fixture *b) {
                      return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
                  });
        foreach (Fixture *f, members2)
            addFixtureLeaf(gi, f);
    }

    // Default organisation: a "Universes" folder with every universe under
    // it, fixtures nested by their universe (so all fixtures are reachable
    // even without a group).
    QMap<quint32, QList<Fixture*> > byUniverse;
    QList<Fixture*> inventory;   // staged, not patched: never filed under a universe
    foreach (Fixture *f, m_doc->fixtures())
    {
        if (f == NULL)
            continue;
        if (f->isPatched())
            byUniverse[f->universe()].append(f);
        else
            inventory.append(f);
    }

    if (byUniverse.isEmpty() == false)
    {
        QTreeWidgetItem *uRoot = new QTreeWidgetItem(this);
        uRoot->setText(0, tr("Universes"));
        uRoot->setIcon(0, QIcon(":/folder.png"));
        uRoot->setData(0, KindRole, CategoryNode);
        uRoot->setFlags(Qt::ItemIsEnabled);

        QMapIterator<quint32, QList<Fixture*> > it(byUniverse);
        while (it.hasNext())
        {
            it.next();
            QString uname = m_doc->inputOutputMap()->getUniverseNameByIndex(int(it.key()));
            if (uname.isEmpty())
                uname = tr("Universe %1").arg(it.key() + 1);
            QTreeWidgetItem *uNode = new QTreeWidgetItem(uRoot);
            uNode->setText(0, uname);
            uNode->setIcon(0, QIcon(":/folder.png"));
            uNode->setData(0, KindRole, UniverseNode);
            uNode->setData(0, IdRole, it.key());
            uNode->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

            QList<Fixture*> fl = it.value();
            std::sort(fl.begin(), fl.end(),
                      [](Fixture *a, Fixture *b) {
                          return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
                      });
            foreach (Fixture *f, fl)
                addFixtureLeaf(uNode, f);
        }
    }

    if (!inventory.isEmpty())
    {
        QTreeWidgetItem *iRoot = new QTreeWidgetItem(this);
        iRoot->setText(0, tr("Inventory"));
        iRoot->setIcon(0, QIcon(":/folder.png"));
        iRoot->setData(0, KindRole, CategoryNode);
        iRoot->setFlags(Qt::ItemIsEnabled);
        std::sort(inventory.begin(), inventory.end(),
                  [](Fixture *a, Fixture *b) {
                      return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
                  });
        foreach (Fixture *f, inventory)
            addFixtureLeaf(iRoot, f);
    }

    // Start collapsed — expand a group/universe to reach its fixtures.
    collapseAll();
    if (!m_filter.isEmpty())
        setFilterText(m_filter);
}

namespace
{
/** Show @p node when it or anything under it holds @p t; a node that holds
 *  it keeps all it contains. Returns whether @p node is shown. */
bool applyFilter(QTreeWidgetItem *node, const QString &t, bool parentMatched)
{
    const bool self = t.isEmpty() || parentMatched || node->text(0).contains(t, Qt::CaseInsensitive);
    bool child = false;
    for (int i = 0; i < node->childCount(); ++i)
        child = applyFilter(node->child(i), t, self && !t.isEmpty()) || child;
    const bool visible = self || child;
    node->setHidden(!visible);
    if (!t.isEmpty() && child)
        node->setExpanded(true);    // open the way down to what matched
    return visible;
}
}

void FixtureGroupSource::setFilterText(const QString &text)
{
    const QString t = text.trimmed();
    const bool cleared = !m_filter.isEmpty() && t.isEmpty();
    m_filter = t;
    for (int i = 0; i < topLevelItemCount(); ++i)
        applyFilter(topLevelItem(i), t, false);
    if (cleared)
        collapseAll();              // back as it opens: collapsed
}

void FixtureGroupSource::addFixtureLeaf(QTreeWidgetItem *parent, Fixture *f)
{
    QTreeWidgetItem *fi = new QTreeWidgetItem(parent);
    fi->setText(0, f->name());
    fi->setIcon(0, ItemIcons::fixtureIcon(f));
    fi->setData(0, IdRole, f->id());
    fi->setData(0, KindRole, FixtureNode);
    fi->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
}

void FixtureGroupSource::slotContextMenu(const QPoint &pos)
{
    // A universe row is a single-item, non-draggable category rather than
    // something that mixes into a fixture/group selection, so it gets its
    // own short menu rather than folding into the collection below.
    QTreeWidgetItem *hovered = itemAt(pos);
    if (hovered != NULL && hovered->data(0, KindRole).toInt() == UniverseNode)
    {
        QMenu uMenu(this);
        QAction *uRename = uMenu.addAction(tr("Rename universe…"));
        QAction *uChosen = uMenu.exec(viewport()->mapToGlobal(pos));
        if (uChosen == uRename)
            beginRename(hovered);
        return;
    }

    // Collect the selection, split by kind. A right-click doesn't change the
    // selection, so also fold in the row under the cursor.
    QList<quint32> groupIds;
    QList<quint32> fixtureIds;
    foreach (QTreeWidgetItem *it, selectedItems())
    {
        const int kind = it->data(0, KindRole).toInt();
        if (kind == GroupNode)
            groupIds << it->data(0, IdRole).toUInt();
        else if (kind == FixtureNode)
            fixtureIds << it->data(0, IdRole).toUInt();
    }

    QTreeWidgetItem *item = itemAt(pos);
    if (item != NULL)
    {
        const int kind = item->data(0, KindRole).toInt();
        const quint32 id = item->data(0, IdRole).toUInt();
        if (kind == GroupNode && groupIds.contains(id) == false)
            groupIds << id;
        else if (kind == FixtureNode && fixtureIds.contains(id) == false)
            fixtureIds << id;
    }

    if (groupIds.isEmpty() && fixtureIds.isEmpty())
        return;

    QMenu menu(this);
    // Creating a group is the fixture-node action; moving to a folder is the
    // group-node action. Both can appear when the selection mixes kinds.
    QAction *create = NULL;
    if (fixtureIds.isEmpty() == false)
        create = menu.addAction(fixtureIds.size() > 1
            ? tr("Create fixture group from %1 fixtures…").arg(fixtureIds.size())
            : tr("Create fixture group from fixture…"));

    QAction *rename = NULL;
    QAction *move = NULL;
    QAction *del = NULL;
    if (groupIds.isEmpty() == false)
    {
        if (groupIds.size() == 1)
            rename = menu.addAction(tr("Rename group…"));
        move = menu.addAction(groupIds.size() > 1
            ? tr("Move %1 groups to folder…").arg(groupIds.size())
            : tr("Move to folder…"));
        menu.addSeparator();
        del = menu.addAction(groupIds.size() > 1
            ? tr("Delete %1 groups").arg(groupIds.size())
            : tr("Delete group"));
    }

    QAction *chosen = menu.exec(viewport()->mapToGlobal(pos));
    if (chosen == NULL)
        return;

    if (chosen == create)
    {
        createGroupFromFixtures(fixtureIds);
        return;
    }
    if (chosen == rename)
    {
        beginRename(groupItemById(groupIds.first()));
        return;
    }
    if (chosen == del)
    {
        const QString msg = groupIds.size() > 1
            ? tr("Delete these %1 fixture groups? (The fixtures themselves are kept.)")
                  .arg(groupIds.size())
            : tr("Delete this fixture group? (The fixtures themselves are kept.)");
        if (QMessageBox::question(this, tr("Delete group"), msg,
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        foreach (quint32 gid, groupIds)
            m_doc->deleteFixtureGroup(gid); // emits fixtureGroupRemoved -> reload()
        m_doc->setModified();
        return;
    }
    if (chosen == move)
    {
        FixtureGroup *first = m_doc->fixtureGroup(groupIds.first());
        const QString cur = (first != NULL) ? first->path() : QString();
        bool ok = false;
        const QString path = QInputDialog::getText(
            this, tr("Move groups to folder"),
            tr("Folder path (e.g. \"Movers/Front\"; empty for none):"),
            QLineEdit::Normal, cur, &ok);
        if (!ok)
            return;

        const QString p = path.trimmed();
        foreach (quint32 gid, groupIds)
        {
            FixtureGroup *g = m_doc->fixtureGroup(gid);
            if (g != NULL)
                g->setPath(p); // emits changed() -> Doc::fixtureGroupChanged -> reload()
        }
        m_doc->setModified();
    }
}

void FixtureGroupSource::slotItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    if (item == NULL || item->data(0, KindRole).toInt() != GroupNode)
        return;
    emit groupDoubleClicked(item->data(0, IdRole).toUInt());
}

QTreeWidgetItem *FixtureGroupSource::groupItemById(quint32 id) const
{
    QTreeWidgetItemIterator it(const_cast<FixtureGroupSource *>(this));
    for (; *it != NULL; ++it)
        if ((*it)->data(0, KindRole).toInt() == GroupNode
            && (*it)->data(0, IdRole).toUInt() == id)
            return *it;
    return NULL;
}

void FixtureGroupSource::beginRename(QTreeWidgetItem *item)
{
    if (item == NULL)
        return;
    const int kind = item->data(0, KindRole).toInt();
    if (kind != GroupNode && kind != UniverseNode)
        return;
    // Arm the row editable. A group's displayed text carries a "(N fixtures)"
    // suffix that must not end up saved as part of the name, so it is dropped
    // back to the plain NameRole text for editing; a universe row has no such
    // suffix, so its own text is already the plain name. blockSignals so
    // arming/relabelling doesn't fire the itemChanged commit path (it only
    // acts on an armed row — see slotItemChanged).
    blockSignals(true);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    if (kind == GroupNode)
        item->setText(0, item->data(0, NameRole).toString());
    blockSignals(false);
    editItem(item, 0);
}

void FixtureGroupSource::slotItemChanged(QTreeWidgetItem *item, int column)
{
    // Only a row ARMED by beginRename() is editable; the setText/setData storm
    // during reload() targets non-editable rows and is ignored here.
    if (column != 0 || item == NULL || !(item->flags() & Qt::ItemIsEditable))
        return;
    // Disarm first so the changes below can't re-enter this handler.
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);

    const int kind = item->data(0, KindRole).toInt();
    const quint32 id = item->data(0, IdRole).toUInt();
    const QString newName = item->text(0).trimmed();
    // Defer: applying the name reloads the tree (which deletes this very item),
    // so must not happen synchronously inside itemChanged.
    if (kind == UniverseNode)
    {
        InputOutputMap *iomap = m_doc->inputOutputMap();
        QTimer::singleShot(0, this, [this, iomap, id, newName]() {
            Universe *uni = iomap ? iomap->universe(id) : NULL;
            if (uni == NULL)
                return;
            if (newName.isEmpty() == false && newName != uni->name())
            {
                iomap->setUniverseName(int(id), newName);
                m_doc->setModified();
            }
            else
            {
                reload();   // no-op/empty edit: restore the previous display
            }
        });
        return;
    }

    QTimer::singleShot(0, this, [this, id, newName]() {
        FixtureGroup *g = m_doc->fixtureGroup(id);
        if (g == NULL)
            return;
        if (newName.isEmpty() == false && newName != g->name())
        {
            g->setName(newName);   // emits changed() -> reload() restores the suffix
            m_doc->setModified();
        }
        else
        {
            reload();              // no-op/empty edit: restore the "Name (N)" display
        }
    });
}

void FixtureGroupSource::keyPressEvent(QKeyEvent *event)
{
    // Enter/Return/F2 on a single selected group or universe row → inline
    // rename (matches the Fixture Manager / Layers trees). Anything else
    // keeps the default behaviour.
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
         || event->key() == Qt::Key_F2)
        && state() != QAbstractItemView::EditingState)
    {
        QTreeWidgetItem *item = currentItem();
        const int kind = item != NULL ? item->data(0, KindRole).toInt() : -1;
        if (item != NULL && (kind == GroupNode || kind == UniverseNode)
            && selectedItems().size() <= 1)
        {
            beginRename(item);
            event->accept();
            return;
        }
    }
    QTreeWidget::keyPressEvent(event);
}

void FixtureGroupSource::createGroupFromFixtures(const QList<quint32> &fixtureIds)
{
    // The ONE shared creator (grid heuristic from the fixtures' 2-D layout,
    // name/size prompt, physical-order arrangement) — same code the Fixture
    // Manager and Studio use, so group creation is consistent app-wide.
    // addFixtureGroup() triggers a tree reload via Doc::fixtureGroupAdded.
    CreateFixtureGroup::createFromFixtures(m_doc, fixtureIds, this);
}

QMimeData* FixtureGroupSource::buildMimeData(const QList<QTreeWidgetItem*> &items) const
{
    // Groups and fixtures get separate payloads/MIME types so each scene
    // drop zone reads only what it understands.
    QByteArray grpData;
    QDataStream grpStream(&grpData, QIODevice::WriteOnly);
    int grpCount = 0;
    QByteArray fxData;
    QDataStream fxStream(&fxData, QIODevice::WriteOnly);
    int fxCount = 0;

    foreach (QTreeWidgetItem *item, items)
    {
        const int kind = item->data(0, KindRole).toInt();
        const quint32 id = item->data(0, IdRole).toUInt();
        if (kind == GroupNode)
        {
            grpStream << id;
            grpCount++;
        }
        else if (kind == FixtureNode)
        {
            fxStream << id;
            fxCount++;
        }
    }

    if (grpCount == 0 && fxCount == 0)
        return QTreeWidget::mimeData(items);

    QMimeData *mime = new QMimeData();
    if (grpCount > 0)
        mime->setData(FIXTUREGROUP_DRAG_MIME_TYPE, grpData);
    if (fxCount > 0)
        mime->setData(FIXTURE_DRAG_MIME_TYPE, fxData);
    return mime;
}

void FixtureGroupSource::dragEnterEvent(QDragEnterEvent *event)
{
    // Only internal group-onto-folder drops are accepted; fixture drags are
    // for external targets (the scene's drop zones), not this tree.
    if (event->mimeData()->hasFormat(FIXTUREGROUP_DRAG_MIME_TYPE))
        event->acceptProposedAction();
    else
        event->ignore();
}

void FixtureGroupSource::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(FIXTUREGROUP_DRAG_MIME_TYPE) == false)
    {
        event->ignore();
        return;
    }
    QTreeWidgetItem *target = itemAt(event->position().toPoint());
    // A folder, a group (join its folder), or the empty root are valid.
    const bool valid = target == NULL
            || target->data(0, PathRole).isValid()
            || target->data(0, KindRole).toInt() == GroupNode;
    if (valid)
        event->acceptProposedAction();
    else
        event->ignore();
}

void FixtureGroupSource::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(FIXTUREGROUP_DRAG_MIME_TYPE) == false)
    {
        event->ignore();
        return;
    }
    QTreeWidgetItem *target = itemAt(event->position().toPoint());

    QString destPath;
    if (target == NULL)
        destPath = QString(); // root
    else if (target->data(0, PathRole).isValid())
        destPath = target->data(0, PathRole).toString();
    else if (target->data(0, KindRole).toInt() == GroupNode)
    {
        FixtureGroup *g = m_doc->fixtureGroup(target->data(0, IdRole).toUInt());
        destPath = (g != NULL) ? g->path() : QString();
    }
    else
    {
        event->ignore(); // universe / fixture -> not a valid target
        return;
    }

    QList<quint32> ids;
    QByteArray data = event->mimeData()->data(FIXTUREGROUP_DRAG_MIME_TYPE);
    QDataStream stream(&data, QIODevice::ReadOnly);
    while (stream.atEnd() == false)
    {
        quint32 gid = 0;
        stream >> gid;
        ids << gid;
    }

    event->acceptProposedAction();

    // Apply AFTER the drop returns: setPath() emits changed -> reload(), which
    // clears/rebuilds this tree. Doing that synchronously inside dropEvent
    // deletes the item Qt's drag machinery is still using -> use-after-free.
    QTimer::singleShot(0, this, [this, ids, destPath]() {
        bool changed = false;
        foreach (quint32 gid, ids)
        {
            FixtureGroup *g = m_doc->fixtureGroup(gid);
            if (g != NULL && g->path() != destPath)
            {
                g->setPath(destPath);
                changed = true;
            }
        }
        if (changed)
            m_doc->setModified();
    });
}

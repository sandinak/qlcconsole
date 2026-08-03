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
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QMap>
#include <QTimer>

#include <algorithm>
#include <cmath>

#include "createfixturegroup.h"
#include "fixturegroupsource.h"
#include "inputoutputmap.h"
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
        gi->setIcon(0, QIcon(":/group.png"));
        gi->setData(0, IdRole, g->id());
        gi->setData(0, KindRole, GroupNode);
        gi->setFlags(leafFlags);

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
    foreach (Fixture *f, m_doc->fixtures())
        if (f != NULL)
            byUniverse[f->universe()].append(f);

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
            uNode->setData(0, KindRole, CategoryNode);
            uNode->setFlags(Qt::ItemIsEnabled);

            QList<Fixture*> fl = it.value();
            std::sort(fl.begin(), fl.end(),
                      [](Fixture *a, Fixture *b) {
                          return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
                      });
            foreach (Fixture *f, fl)
                addFixtureLeaf(uNode, f);
        }
    }

    // Start collapsed — expand a group/universe to reach its fixtures.
    collapseAll();
}

void FixtureGroupSource::addFixtureLeaf(QTreeWidgetItem *parent, Fixture *f)
{
    QTreeWidgetItem *fi = new QTreeWidgetItem(parent);
    fi->setText(0, f->name());
    fi->setIcon(0, QIcon(":/fixture.png"));
    fi->setData(0, IdRole, f->id());
    fi->setData(0, KindRole, FixtureNode);
    fi->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
}

void FixtureGroupSource::slotContextMenu(const QPoint &pos)
{
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
        renameGroupPrompt(groupIds.first());
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

void FixtureGroupSource::renameGroupPrompt(quint32 groupId)
{
    FixtureGroup *g = m_doc->fixtureGroup(groupId);
    if (g == NULL)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename group"),
                             tr("Group name:"), QLineEdit::Normal, g->name(), &ok);
    if (ok && name.trimmed().isEmpty() == false)
    {
        g->setName(name.trimmed()); // emits changed() -> reload()
        m_doc->setModified();
    }
}

void FixtureGroupSource::keyPressEvent(QKeyEvent *event)
{
    // Enter/Return on a single selected group row → rename it (matches the
    // right-click "Rename group…"). Anything else keeps the default behaviour.
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        QTreeWidgetItem *item = currentItem();
        if (item != NULL && item->data(0, KindRole).toInt() == GroupNode
            && selectedItems().size() <= 1)
        {
            renameGroupPrompt(item->data(0, IdRole).toUInt());
            event->accept();
            return;
        }
    }
    QTreeWidget::keyPressEvent(event);
}

void FixtureGroupSource::createGroupFromFixtures(const QList<quint32> &fixtureIds)
{
    if (fixtureIds.isEmpty())
        return;

    // Total head count across the selection sizes a roughly-square grid to
    // drop into (mirrors FixtureManager::addFixtureToGroup).
    int headTotal = 0;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *fxi = m_doc->fixture(fid);
        if (fxi != NULL)
            headTotal += fxi->heads();
    }
    qreal side = sqrt(qMax(1, headTotal));
    if (side != floor(side))
        side += 1; // not a perfect square: give the grid an extra row/column
    if (side < 1)
        side = 4;

    CreateFixtureGroup cfg(this);
    cfg.setSize(QSize(int(side), int(side)));
    if (cfg.exec() != QDialog::Accepted)
        return;

    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName(cfg.name());
    grp->setSize(cfg.size());
    m_doc->addFixtureGroup(grp);

    foreach (quint32 fid, fixtureIds)
        grp->assignFixture(fid); // lays each fixture's heads into the grid

    m_doc->setModified();
    // addFixtureGroup already triggers a reload via Doc::fixtureGroupAdded; the
    // new group appears in the tree ready to drag onto a scene.
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QTreeWidgetItem *target = itemAt(event->pos());
#else
    QTreeWidgetItem *target = itemAt(event->position().toPoint());
#endif
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QTreeWidgetItem *target = itemAt(event->pos());
#else
    QTreeWidgetItem *target = itemAt(event->position().toPoint());
#endif

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

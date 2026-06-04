/*
  Q Light Controller Plus
  fixturegroupsource.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QMimeData>
#include <QDataStream>
#include <QHeaderView>
#include <QMenu>
#include <QInputDialog>
#include <QMap>

#include <algorithm>

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
    setAcceptDrops(false);
    setDragDropMode(QAbstractItemView::DragOnly);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotContextMenu(QPoint)));

    reload();

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
    fi->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_folderMap[path] = fi;
    return fi;
}

void FixtureGroupSource::reload()
{
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
    QTreeWidgetItem *item = itemAt(pos);
    if (item == NULL || item->data(0, KindRole).toInt() != GroupNode)
        return;

    FixtureGroup *g = m_doc->fixtureGroup(item->data(0, IdRole).toUInt());
    if (g == NULL)
        return;

    QMenu menu(this);
    QAction *move = menu.addAction(tr("Move to folder…"));
    if (menu.exec(viewport()->mapToGlobal(pos)) != move)
        return;

    bool ok = false;
    const QString path = QInputDialog::getText(
        this, tr("Move group to folder"),
        tr("Folder path (e.g. \"Movers/Front\"; empty for none):"),
        QLineEdit::Normal, g->path(), &ok);
    if (!ok)
        return;

    // setPath emits changed() -> Doc::fixtureGroupChanged -> reload().
    g->setPath(path.trimmed());
    m_doc->setModified();
}

QMimeData* FixtureGroupSource::mimeData(const QList<QTreeWidgetItem*> items) const
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

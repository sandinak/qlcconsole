/*
  Q Light Controller Plus
  fixturegroupsource.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QMimeData>
#include <QDataStream>
#include <QHeaderView>

#include <algorithm>

#include "fixturegroupsource.h"
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

void FixtureGroupSource::reload()
{
    clear();

    // --- Fixture Groups ---
    QTreeWidgetItem *groupsRoot = new QTreeWidgetItem(this);
    groupsRoot->setText(0, tr("Fixture Groups"));
    groupsRoot->setData(0, KindRole, CategoryNode);
    groupsRoot->setFlags(Qt::ItemIsEnabled);

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
        const int n = g->fixtureList().count();
        QTreeWidgetItem *it = new QTreeWidgetItem(groupsRoot);
        it->setText(0, tr("%1  (%n fixture(s))", "", n).arg(g->name()));
        it->setIcon(0, QIcon(":/group.png"));
        it->setData(0, IdRole, g->id());
        it->setData(0, KindRole, GroupNode);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
    }

    // --- Fixtures ---
    QTreeWidgetItem *fixturesRoot = new QTreeWidgetItem(this);
    fixturesRoot->setText(0, tr("Fixtures"));
    fixturesRoot->setData(0, KindRole, CategoryNode);
    fixturesRoot->setFlags(Qt::ItemIsEnabled);

    QList<Fixture*> fixtures;
    foreach (Fixture *f, m_doc->fixtures())
        if (f != NULL)
            fixtures.append(f);
    std::sort(fixtures.begin(), fixtures.end(),
              [](Fixture *a, Fixture *b) {
                  return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
              });
    foreach (Fixture *f, fixtures)
    {
        QTreeWidgetItem *it = new QTreeWidgetItem(fixturesRoot);
        it->setText(0, f->name());
        it->setData(0, IdRole, f->id());
        it->setData(0, KindRole, FixtureNode);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
    }

    expandAll();
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

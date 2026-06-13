/*
  Q Light Controller Plus
  fixturetreewidget.cpp

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

#include <QDebug>
#include <QHeaderView>
#include <QMimeData>
#include <QDataStream>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QTreeWidgetItemIterator>

#include "fixturetreewidget.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

#define KColumnName 0

static const char* FIXTURE_DRAG_MIME_TYPE = "application/x-qlcplus-fixtures";
static const char* GROUP_DRAG_MIME_TYPE = "application/x-qlcplus-fixturegroups";

const char* FixtureTreeWidget::fixtureDragMimeType()
{
    return FIXTURE_DRAG_MIME_TYPE;
}

const char* FixtureTreeWidget::groupDragMimeType()
{
    return GROUP_DRAG_MIME_TYPE;
}

QTreeWidgetItem* FixtureTreeWidget::groupFolderItem(const QString& path)
{
    if (path.isEmpty())
        return invisibleRootItem();
    if (m_groupFolders.contains(path))
        return m_groupFolders[path];

    const int slash = path.lastIndexOf('/');
    const QString parentPath = (slash < 0) ? QString() : path.left(slash);
    const QString name = (slash < 0) ? path : path.mid(slash + 1);

    QTreeWidgetItem *fi = new QTreeWidgetItem(groupFolderItem(parentPath));
    fi->setText(KColumnName, name);
    fi->setIcon(KColumnName, QIcon(":/folder.png"));
    fi->setData(KColumnName, PROP_FOLDER, path); // lets drops resolve the path
    fi->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);
    m_groupFolders[path] = fi;
    return fi;
}

QMimeData* FixtureTreeWidget::mimeData(const QList<QTreeWidgetItem*> &items) const
{
    QByteArray fxData, grpData;
    QDataStream fxStream(&fxData, QIODevice::WriteOnly);
    QDataStream grpStream(&grpData, QIODevice::WriteOnly);
    int fxCount = 0, grpCount = 0;
    foreach (QTreeWidgetItem *item, items)
    {
        // A group item carries its layout as a whole; drag it to move folders.
        QVariant g = item->data(KColumnName, PROP_GROUP);
        if (g.isValid())
        {
            grpStream << g.toUInt();
            grpCount++;
            continue;
        }
        QVariant v = item->data(KColumnName, PROP_ID); // fixture id (string)
        if (v.isValid() == false)
            continue;
        bool ok = false;
        quint32 fid = v.toString().toUInt(&ok);
        if (ok) { fxStream << fid; fxCount++; }
    }
    if (fxCount == 0 && grpCount == 0)
        return QTreeWidget::mimeData(items);

    QMimeData *mime = new QMimeData();
    if (fxCount > 0)
        mime->setData(FIXTURE_DRAG_MIME_TYPE, fxData);
    if (grpCount > 0)
        mime->setData(GROUP_DRAG_MIME_TYPE, grpData);
    return mime;
}

void FixtureTreeWidget::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && state() != QAbstractItemView::EditingState)
    {
        QTreeWidgetItem *it = currentItem();
        if (it && it->data(KColumnName, PROP_FOLDER).isValid())
        {
            editItem(it, KColumnName);
            return;
        }
    }
    QTreeWidget::keyPressEvent(event);
}

void FixtureTreeWidget::slotItemChanged(QTreeWidgetItem* item, int column)
{
    if (column != KColumnName || !item)
        return;
    const QVariant folderVar = item->data(KColumnName, PROP_FOLDER);
    if (!folderVar.isValid())
        return;

    const QString oldPath = folderVar.toString();
    const QString newLeaf = item->text(KColumnName).trimmed();
    if (newLeaf.isEmpty() || newLeaf == oldPath.section('/', -1))
        return;

    // Build the new full path, preserving any parent path segments.
    const int slash = oldPath.lastIndexOf('/');
    const QString newPath = (slash < 0) ? newLeaf
                                        : oldPath.left(slash + 1) + newLeaf;

    // Update the stored path on this item so drop-targets stay consistent.
    item->setData(KColumnName, PROP_FOLDER, newPath);

    emit groupFolderRenamed(oldPath, newLeaf);
}

void FixtureTreeWidget::dragEnterEvent(QDragEnterEvent* event)
{
    // The tree only accepts internal group-onto-folder drops; fixture drags
    // are for external targets (the group editor grid), not this tree.
    if (event->mimeData()->hasFormat(GROUP_DRAG_MIME_TYPE))
        event->acceptProposedAction();
    else
        event->ignore();
}

void FixtureTreeWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(GROUP_DRAG_MIME_TYPE) == false)
    {
        event->ignore();
        return;
    }

    // Only a group folder, a group (drop alongside it), or the empty root are
    // valid targets for a group drop.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QTreeWidgetItem *target = itemAt(event->pos());
#else
    QTreeWidgetItem *target = itemAt(event->position().toPoint());
#endif
    const bool valid = target == NULL ||
            target->data(KColumnName, PROP_FOLDER).isValid() ||
            target->data(KColumnName, PROP_GROUP).isValid();
    if (valid)
        event->acceptProposedAction();
    else
        event->ignore();
}

void FixtureTreeWidget::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasFormat(GROUP_DRAG_MIME_TYPE) == false)
    {
        event->ignore();
        return;
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QTreeWidgetItem *target = itemAt(event->pos());
#else
    QTreeWidgetItem *target = itemAt(event->position().toPoint());
#endif

    // Resolve the destination folder path.
    QString destPath;
    if (target == NULL)
    {
        destPath = QString(); // dropped on empty space -> root
    }
    else if (target->data(KColumnName, PROP_FOLDER).isValid())
    {
        destPath = target->data(KColumnName, PROP_FOLDER).toString();
    }
    else if (target->data(KColumnName, PROP_GROUP).isValid())
    {
        // Dropped onto a group: join its folder.
        FixtureGroup *grp = m_doc->fixtureGroup(target->data(KColumnName, PROP_GROUP).toUInt());
        destPath = (grp != NULL) ? grp->path() : QString();
    }
    else
    {
        event->ignore(); // universe / fixture / head -> not a valid target
        return;
    }

    QList<quint32> ids;
    QByteArray data = event->mimeData()->data(GROUP_DRAG_MIME_TYPE);
    QDataStream stream(&data, QIODevice::ReadOnly);
    while (stream.atEnd() == false)
    {
        quint32 gid = 0;
        stream >> gid;
        ids << gid;
    }

    if (ids.isEmpty() == false)
        emit groupsDroppedOnFolder(ids, destPath);
    event->acceptProposedAction();
}

FixtureTreeWidget::FixtureTreeWidget(Doc *doc, quint32 flags, QWidget *parent)
    : QTreeWidget(parent)
    , m_doc(doc)
    , m_universesCount(0)
    , m_fixturesCount(0)
    , m_channelsCount(0)
    , m_uniColumn(-1)
    , m_addressColumn(-1)
    , m_typeColumn(-1)
    , m_headsColumn(-1)
    , m_manufColumn(-1)
    , m_modelColumn(-1)
    , m_showGroups(false)
    , m_showHeads(false)
    , m_channelSelection(false)
{
    setFlags(flags);

    setRootIsDecorated(true);
    setAllColumnsShowFocus(true);
    setSortingEnabled(true);
    sortByColumn(KColumnName, Qt::AscendingOrder);

    connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)),
            this, SLOT(slotItemExpanded()));
    connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem*)),
            this, SLOT(slotItemExpanded()));
    connect(this, SIGNAL(itemChanged(QTreeWidgetItem*, int)),
            this, SLOT(slotItemChanged(QTreeWidgetItem*, int)));
}

void FixtureTreeWidget::setFlags(quint32 flags)
{
    // column 0 is always reserved for Fixture name
    int columnIdx = 1;
    QStringList labels;
    labels << tr("Name");

    if (flags & UniverseNumber)
    {
        m_uniColumn = columnIdx++;
        labels << tr("Universe");
    }
    if (flags & AddressRange)
    {
        m_addressColumn = columnIdx++;
        labels << tr("Address");
    }
    if (flags & ChannelType)
    {
        m_typeColumn = columnIdx++;
        labels << tr("Type");
    }
    if (flags & HeadsNumber)
    {
        m_headsColumn = columnIdx++;
        labels << tr("Heads");
    }
    if (flags & Manufacturer)
    {
        m_manufColumn = columnIdx++;
        labels << tr("Manufacturer");
    }
    if (flags & Model)
    {
        m_modelColumn = columnIdx++;
        labels << tr("Model");
    }
    if (flags & ShowGroups)
        m_showGroups = true;

    if (flags & ShowHeads)
        m_showHeads = true;

    if (flags & ChannelSelection)
        m_channelSelection = true;

    setHeaderLabels(labels);
}

/****************************************************************************
 * Disabled items
 ****************************************************************************/

void FixtureTreeWidget::setDisabledFixtures(const QList <quint32>& disabled)
{
    m_disabledHeads.clear();
    m_disabledFixtures = disabled;
}

void FixtureTreeWidget::setDisabledHeads(const QList <GroupHead>& disabled)
{
    m_disabledFixtures.clear();
    m_disabledHeads = disabled;
}

void FixtureTreeWidget::setChannelsMask(QByteArray channels)
{
    m_channelsMask = channels;
}

/****************************************************************************
 * Tree rendering
 ****************************************************************************/

QTreeWidgetItem *FixtureTreeWidget::fixtureItem(quint32 id) const
{
    // Search the whole tree: fixtures now sit two levels deep (Universes ->
    // Universe -> fixture) and may also appear under group nodes.
    QTreeWidgetItemIterator it(const_cast<FixtureTreeWidget*>(this));
    while (*it != NULL)
    {
        QTreeWidgetItem *item = *it;
        QVariant var = item->data(KColumnName, PROP_ID);
        if (var.isValid() == true && var.toUInt() == id &&
            item->data(KColumnName, PROP_HEAD).isValid() == false)
            return item;
        ++it;
    }

    return NULL;
}

QTreeWidgetItem *FixtureTreeWidget::groupItem(quint32 id) const
{
    // Groups are nested under their folder path, so search recursively.
    QTreeWidgetItemIterator it(const_cast<FixtureTreeWidget*>(this));
    while (*it != NULL)
    {
        QTreeWidgetItem *item = *it;
        QVariant var = item->data(KColumnName, PROP_GROUP);
        if (var.isValid() == true && var.toUInt() == id)
            return item;
        ++it;
    }

    return NULL;
}

void FixtureTreeWidget::updateFixtureItem(QTreeWidgetItem* item, Fixture* fixture)
{
    Q_ASSERT(item != NULL);
    if (fixture == NULL)
        return;

    item->setText(KColumnName, fixture->name());
    item->setIcon(KColumnName, fixture->getIconFromType());
    item->setData(KColumnName, PROP_ID, QString::number(fixture->id()));
    if (m_channelSelection)
    {
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
        item->setCheckState(KColumnName, Qt::Unchecked);
    }
    if (m_disabledFixtures.contains(fixture->id()) == true)
    {
        // Disable selection
        item->setFlags(Qt::NoItemFlags);
    }

    if (m_uniColumn > 0)
    {
        item->setText(m_uniColumn, QString("%1").arg(fixture->universe() + 1));
        item->setTextAlignment(m_uniColumn, Qt::AlignHCenter | Qt::AlignVCenter);
    }

    if (m_addressColumn)
    {
        QString s;
        if (fixture->channels() > 1)
        {
            item->setText(m_addressColumn, s.asprintf("%.3d - %.3d", fixture->address() + 1,
                                                      fixture->address() + fixture->channels()));
        }
        else
        {
            item->setText(m_addressColumn, s.asprintf("%.3d", fixture->address() + 1));
        }
    }

    if (m_headsColumn > 0)
        item->setText(m_headsColumn, QString::number(fixture->heads()));

    if (m_manufColumn > 0)
    {
        if (fixture->fixtureDef() == NULL)
            item->setText(m_manufColumn, tr("Generic"));
        else
            item->setText(m_manufColumn, fixture->fixtureDef()->manufacturer());
    }

    if (m_modelColumn > 0)
    {
        if (fixture->fixtureDef() == NULL)
            item->setText(m_modelColumn, tr("Generic"));
        else
            item->setText(m_modelColumn, fixture->fixtureDef()->model());
    }

    if (m_showHeads == true)
    {
        int disabled = 0;

        for (int i = 0; i < fixture->heads(); i++)
        {
            QTreeWidgetItem* headItem = new QTreeWidgetItem(item);
            headItem->setText(KColumnName, QString("%1 %2").arg(tr("Head")).arg(i + 1, 3, 10, QChar('0')));
            headItem->setData(KColumnName, PROP_HEAD, i);
            if (m_disabledHeads.contains(GroupHead(fixture->id(), i)) == true)
            {
                headItem->setFlags(Qt::NoItemFlags); // Disable selection
                disabled++;
            }
        }

        // Disable the whole fixture if all heads are disabled
        if (disabled == fixture->heads())
            item->setFlags(Qt::NoItemFlags);
    }

    if (m_channelSelection == true)
    {
        quint32 baseAddress = fixture->universeAddress();
        for (quint32 c = 0; c < fixture->channels(); c++)
        {
            const QLCChannel* channel = fixture->channel(c);
            QTreeWidgetItem *cItem = new QTreeWidgetItem(item);
            cItem->setText(KColumnName, QString("%1:%2").arg(c + 1)
                          .arg(channel->name()));
            cItem->setIcon(KColumnName, channel->getIcon());
            cItem->setData(KColumnName, PROP_CHANNEL, c);
            if (m_typeColumn > 0)
            {
                if (channel->group() == QLCChannel::Intensity &&
                    channel->colour() != QLCChannel::NoColour)
                    cItem->setText(m_typeColumn, QLCChannel::colourToString(channel->colour()));
                else
                    cItem->setText(m_typeColumn, QLCChannel::groupToString(channel->group()));
            }

            cItem->setFlags(cItem->flags() | Qt::ItemIsUserCheckable);
            if (m_channelsMask.length() > (int)(baseAddress + c) &&
                m_channelsMask.at(baseAddress + c) == 1)
                    cItem->setCheckState(KColumnName, Qt::Checked);
            else
                cItem->setCheckState(KColumnName, Qt::Unchecked);
        }
    }
}

void FixtureTreeWidget::updateGroupItem(QTreeWidgetItem* item, const FixtureGroup* grp)
{
    Q_ASSERT(item != NULL);
    Q_ASSERT(grp != NULL);

    item->setText(KColumnName, grp->name());
    item->setIcon(KColumnName, QIcon(":/group.png"));
    item->setData(KColumnName, PROP_GROUP, grp->id());

    // This should be a safe check because simultaneous add/removal is not possible,
    // which could result in changes in fixtures but with the same fixture count.
    if (item->childCount() != grp->fixtureList().size())
    {
        // Remove existing children
        while (item->childCount() > 0)
            delete item->child(0);

        // Add group's children
        foreach (quint32 id, grp->fixtureList())
        {
            QTreeWidgetItem* grpItem = new QTreeWidgetItem(item);
            updateFixtureItem(grpItem, m_doc->fixture(id));
        }
    }
}

int FixtureTreeWidget::universeCount()
{
    return m_universesCount;
}

int FixtureTreeWidget::fixturesCount()
{
    return m_fixturesCount;
}

int FixtureTreeWidget::channelsCount()
{
    return m_channelsCount;
}

QList<quint32> FixtureTreeWidget::selectedFixtures()
{
    updateSelections();
    return m_selectedFixtures;
}

QList<GroupHead> FixtureTreeWidget::selectedHeads()
{
    updateSelections();
    return m_selectedHeads;
}

void FixtureTreeWidget::updateSelections()
{
    m_selectedFixtures.clear();
    m_selectedHeads.clear();

    QListIterator <QTreeWidgetItem*> it(selectedItems());
    while (it.hasNext() == true)
    {
        QTreeWidgetItem *item = it.next();
        // A selected item can be:
        // 1) a fixture
        // 2) a group
        // 3) a head
        // 4) a universe

        QVariant fxIDVar = item->data(KColumnName, PROP_ID);
        QVariant grpIDVar = item->data(KColumnName, PROP_GROUP);
        QVariant headVar = item->data(KColumnName, PROP_HEAD);
        QVariant uniIDVar = item->data(KColumnName, PROP_UNIVERSE);

        qDebug() << "uni ID:" << uniIDVar;

        // Case 1: is there a valid fixture ID ?
        if (fxIDVar.isValid())
        {
            quint32 fxi = fxIDVar.toUInt();
            m_selectedFixtures << fxi;

            // fill also the non-diabled heads, if present
            if (m_showHeads && item->childCount() > 0)
            {
                for (int h = 0; h < item->childCount(); h++)
                {
                    QTreeWidgetItem *hItem = item->child(h);
                    if (hItem->isDisabled() == false)
                    {
                        QVariant chHeadVar = hItem->data(KColumnName, PROP_HEAD);
                        if (chHeadVar.isValid())
                        {
                            GroupHead gh(fxi, chHeadVar.toInt());
                            if (m_selectedHeads.contains(gh) == false)
                                m_selectedHeads << gh;
                        }
                    }
                }
            }
        }
        // Case 2: is there a valid group ID ?
        else if (grpIDVar.isValid())
        {
            // in this case cycle through the children and get each
            // fixture ID
            for (int i = 0; i < item->childCount(); i++)
            {
                QTreeWidgetItem *child = item->child(i);
                QVariant chFxIDVar = child->data(KColumnName, PROP_ID);
                if (chFxIDVar.isValid() && child->isDisabled() == false)
                    m_selectedFixtures << chFxIDVar.toUInt();
            }
        }
        // Case 3: is there a valid head index ?
        else if (headVar.isValid())
        {
            Q_ASSERT(item->parent() != NULL);
            quint32 fxi = item->parent()->data(KColumnName, PROP_ID).toUInt();
            GroupHead gh(fxi, headVar.toInt());
            if (m_selectedHeads.contains(gh) == false)
                m_selectedHeads << gh;
        }
        // Case 4: is there a valid universe index ?
        else if (uniIDVar.isValid())
        {
            qDebug() << "Valid universe....";
            // in this case cycle through the children and get each
            // fixture ID
            for (int i = 0; i < item->childCount(); i++)
            {
                QTreeWidgetItem *child = item->child(i);
                QVariant chFxIDVar = child->data(KColumnName, PROP_ID);
                if (chFxIDVar.isValid() && child->isDisabled() == false)
                    m_selectedFixtures << chFxIDVar.toUInt();
            }
        }
    }
}

void FixtureTreeWidget::slotItemExpanded()
{
    header()->resizeSections(QHeaderView::ResizeToContents);
}

void FixtureTreeWidget::updateTree()
{
    clear();
    m_groupFolders.clear();
    m_universesCount = 0;
    m_fixturesCount = 0;
    m_channelsCount = 0;

    if (m_showGroups == true)
    {
        foreach (FixtureGroup* grp, m_doc->fixtureGroups())
        {
            // Nest the group under its folder path (if any).
            QTreeWidgetItem* grpItem = new QTreeWidgetItem(groupFolderItem(grp->path()));
            updateGroupItem(grpItem, grp);
        }
    }

    // In the Fixture Manager (groups shown) keep all universes tidily under a
    // single "Universes" folder, separate from the fixture-group folders.
    QTreeWidgetItem *universesRoot = NULL;

    foreach (Fixture* fixture, m_doc->fixtures())
    {
        Q_ASSERT(fixture != NULL);

        if (m_showGroups && universesRoot == NULL)
        {
            universesRoot = new QTreeWidgetItem(this);
            universesRoot->setText(KColumnName, tr("Universes"));
            universesRoot->setIcon(KColumnName, QIcon(":/folder.png"));
            universesRoot->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            universesRoot->setExpanded(true);
        }

        // The universe nodes live under universesRoot (or at top level when
        // groups aren't shown, e.g. in fixture-selection dialogs).
        QTreeWidgetItem *uniParent = universesRoot ? universesRoot : invisibleRootItem();

        QTreeWidgetItem *topItem = NULL;
        quint32 uni = fixture->universe();
        for (int i = 0; i < uniParent->childCount(); i++)
        {
            QTreeWidgetItem* tItem = uniParent->child(i);
            QVariant tVar = tItem->data(KColumnName, PROP_UNIVERSE);
            if (tVar.isValid())
            {
                quint32 tUni = tVar.toUInt();
                if (tUni == uni)
                {
                    topItem = tItem;
                    break;
                }
            }
        }
        // Haven't found this universe node ? Create it.
        if (topItem == NULL)
        {
            topItem = new QTreeWidgetItem(uniParent);
            topItem->setText(KColumnName, m_doc->inputOutputMap()->getUniverseNameByID(uni));
            topItem->setIcon(KColumnName, QIcon(m_showGroups ? ":/folder.png" : ":/group.png"));
            topItem->setData(KColumnName, PROP_UNIVERSE, uni);
            topItem->setExpanded(true);
            if (m_channelSelection)
            {
                topItem->setFlags(topItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
                topItem->setCheckState(KColumnName, Qt::Unchecked);
            }
            m_universesCount++;
        }

        QTreeWidgetItem *fItem = new QTreeWidgetItem(topItem);
        updateFixtureItem(fItem, fixture);
        m_fixturesCount++;
        m_channelsCount += fixture->channels();
    }

    header()->resizeSections(QHeaderView::ResizeToContents);
}



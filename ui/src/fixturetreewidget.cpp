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

#include <QTimer>
#include <QDebug>
#include <QHeaderView>
#include <QMimeData>
#include <QDataStream>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QTreeWidgetItemIterator>

#include <QMap>
#include "fixturetreewidget.h"
#include "monitor/fixturevisualtraits.h"
#include "itemicons.h"
#include "apputil.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "powerdistribution.h"
#include "outputrulekeeper.h"

// Short output-destination description for a universe (by ID), e.g.
// "ArtNet: 172.18.2.221 U:0", or the plugin name for non-network outputs.
static QString universeDestination(Doc *doc, quint32 uniID)
{
    InputOutputMap *io = doc->inputOutputMap();
    for (quint32 i = 0; i < io->universesCount(); i++)
    {
        if (io->getUniverseID(i) != uniID)
            continue;
        OutputPatch *op = io->outputPatch(i);
        if (op == NULL || op->pluginName().isEmpty())
            return QString();
        const QMap<QString, QVariant> p = op->getPluginParameters();
        QString s = op->pluginName();
        // Target address: the explicit target IP if set, else the output line
        // (the interface/port name — for ArtNet that's the sending interface).
        QString addr = p.value("outputIP").toString();
        if (addr.isEmpty())
            addr = op->outputName();
        if (!addr.isEmpty())
            s += QString(": %1").arg(addr);
        // ArtNet/network universe on that target.
        if (p.contains("outputUni"))
            s += QString(" U:%1").arg(p.value("outputUni").toInt());
        return s;
    }
    return QString();
}
#include "doc.h"

#define KColumnName 0

static const char* FIXTURE_DRAG_MIME_TYPE = "application/x-qlcplus-fixtures";
static const char* GROUP_DRAG_MIME_TYPE = "application/x-qlcplus-fixturegroups";
static const char* HEAD_DRAG_MIME_TYPE = "application/x-qlcplus-fixtureheads";

const char* FixtureTreeWidget::fixtureDragMimeType()
{
    return FIXTURE_DRAG_MIME_TYPE;
}

const char* FixtureTreeWidget::groupDragMimeType()
{
    return GROUP_DRAG_MIME_TYPE;
}

const char* FixtureTreeWidget::headDragMimeType()
{
    return HEAD_DRAG_MIME_TYPE;
}

QTreeWidgetItem* FixtureTreeWidget::groupFolderItem(const QString& path)
{
    // updateTree() seeds m_groupFolders[QString()] with the "Fixture Groups"
    // root when groups are shown, so the empty-path case normally resolves
    // there, not to the tree's own invisible root -- falling back to that
    // only covers contexts where no such root was ever seeded (m_showGroups
    // false, e.g. a plain fixture-selection dialog).
    if (m_groupFolders.contains(path))
        return m_groupFolders[path];
    if (path.isEmpty())
        return invisibleRootItem();

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

QMimeData* FixtureTreeWidget::buildMimeData(const QList<QTreeWidgetItem*> &items) const
{
    QByteArray fxData, grpData, headData;
    QDataStream fxStream(&fxData, QIODevice::WriteOnly);
    QDataStream grpStream(&grpData, QIODevice::WriteOnly);
    QDataStream headStream(&headData, QIODevice::WriteOnly);
    int fxCount = 0, grpCount = 0, headCount = 0;
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
        // A HEAD row (ShowHeads mode): carries no PROP_ID of its own -- its
        // fixture id comes from its parent row. Encoded separately so a drop
        // target (the Fixture Group editor's grid) can place just this head
        // instead of the whole fixture's worth.
        QVariant h = item->data(KColumnName, PROP_HEAD);
        if (h.isValid())
        {
            QTreeWidgetItem *parentItem = item->parent();
            QVariant pv = (parentItem != nullptr)
                ? parentItem->data(KColumnName, PROP_ID) : QVariant();
            bool pok = false;
            const quint32 pfid = pv.toString().toUInt(&pok);
            if (pok)
            {
                headStream << pfid << quint32(h.toInt());
                headCount++;
            }
            continue;
        }
        QVariant v = item->data(KColumnName, PROP_ID); // fixture id (string)
        if (v.isValid() == false)
            continue;
        bool ok = false;
        quint32 fid = v.toString().toUInt(&ok);
        if (ok) { fxStream << fid; fxCount++; }
    }
    if (fxCount == 0 && grpCount == 0 && headCount == 0)
        return QTreeWidget::mimeData(items);

    QMimeData *mime = new QMimeData();
    if (fxCount > 0)
        mime->setData(FIXTURE_DRAG_MIME_TYPE, fxData);
    if (grpCount > 0)
        mime->setData(GROUP_DRAG_MIME_TYPE, grpData);
    if (headCount > 0)
        mime->setData(HEAD_DRAG_MIME_TYPE, headData);
    return mime;
}

void FixtureTreeWidget::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
         || event->key() == Qt::Key_F2)
        && state() != QAbstractItemView::EditingState)
    {
        QTreeWidgetItem *it = currentItem();
        if (it && it->data(KColumnName, PROP_FOLDER).isValid())
        {
            editItem(it, KColumnName);
            return;
        }
        // Rename a universe folder in place ("Universe N" under Universes).
        // A universe row carries PROP_UNIVERSE and nothing else identifying;
        // fixtures never set it, so this alone is enough to pick it out.
        if (it && it->data(KColumnName, PROP_UNIVERSE).isValid())
        {
            const quint32 uni = it->data(KColumnName, PROP_UNIVERSE).toUInt();
            blockSignals(true);
            it->setFlags(it->flags() | Qt::ItemIsEditable);
            // Show the plain name for editing -- the displayed label can
            // carry a "  —  destination" suffix (see universeDestination())
            // that must not round-trip into the stored name.
            it->setText(KColumnName, m_doc->inputOutputMap()->getUniverseNameByID(uni));
            blockSignals(false);
            m_armedIndex = indexFromItem(it, KColumnName);
            editItem(it, KColumnName);
            return;
        }
        // Rename a fixture group in place. Same rules as a fixture row below:
        // a plain (non-checkable) tree only, armed without notifying
        // slotItemChanged. Double-click keeps opening the group's editor.
        if (it && m_channelSelection == false
            && it->data(KColumnName, PROP_GROUP).isValid())
        {
            blockSignals(true);
            it->setFlags(it->flags() | Qt::ItemIsEditable);
            blockSignals(false);
            m_armedIndex = indexFromItem(it, KColumnName);
            editItem(it, KColumnName);
            return;
        }
        // Rename a fixture in place. Only in a plain (non-checkable) tree, and
        // only for a real fixture row (has a fixture id, not a head/channel).
        if (it && m_channelSelection == false
            && it->data(KColumnName, PROP_ID).isValid()
            && it->data(KColumnName, PROP_HEAD).isValid() == false
            && it->data(KColumnName, PROP_CHANNEL).isValid() == false)
        {
            // Arm the row as editable, but WITHOUT notifying slotItemChanged:
            // QTreeWidgetItem::setFlags() emits itemChanged() synchronously,
            // which would immediately re-enter slotItemChanged, disarm the row,
            // and leave editItem() below a no-op (no editor would open).
            blockSignals(true);
            it->setFlags(it->flags() | Qt::ItemIsEditable);
            blockSignals(false);
            m_armedIndex = indexFromItem(it, KColumnName);
            editItem(it, KColumnName);
            return;
        }
    }
    QTreeWidget::keyPressEvent(event);
}

void FixtureTreeWidget::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
{
    QTreeWidget::closeEditor(editor, hint);

    // A committed edit has already been through slotItemChanged(), which
    // disarms the row. One cancelled with Esc (or committed unchanged, which
    // fires no itemChanged at all) is still armed: disarm it here, or the row
    // would stay editable and the next double-click would open an editor
    // instead of doing what double-click does on that row.
    const QPersistentModelIndex idx = m_armedIndex;
    m_armedIndex = QPersistentModelIndex();
    QTreeWidgetItem *item = idx.isValid() ? itemFromIndex(idx) : NULL;
    if (item == NULL || (item->flags() & Qt::ItemIsEditable) == 0
        || item->data(KColumnName, PROP_FOLDER).isValid())
        return;

    blockSignals(true);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    if (item->data(KColumnName, PROP_GROUP).isValid())
    {
        FixtureGroup *grp = m_doc->fixtureGroup(item->data(KColumnName, PROP_GROUP).toUInt());
        if (grp != NULL)
            item->setText(KColumnName, grp->name());
    }
    blockSignals(false);
    // A universe row was showing its bare name for the edit; put the
    // "  —  destination" label back.
    if (item->data(KColumnName, PROP_UNIVERSE).isValid())
        updateUniverseLabels();
}

void FixtureTreeWidget::updateUniverseLabels()
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;
    blockSignals(true);
    for (QTreeWidgetItemIterator it(this); *it != NULL; ++it)
    {
        QTreeWidgetItem *item = *it;
        const QVariant uv = item->data(KColumnName, PROP_UNIVERSE);
        // A row being renamed shows its bare name on purpose; leave it be.
        if (uv.isValid() == false || (item->flags() & Qt::ItemIsEditable))
            continue;
        const quint32 uni = uv.toUInt();
        QString uniLabel = iomap->getUniverseNameByID(uni);
        const QString dest = universeDestination(m_doc, uni);
        if (!dest.isEmpty())
            uniLabel += QString("   —   %1").arg(dest);
        item->setText(KColumnName, uniLabel);
    }
    blockSignals(false);
}

void FixtureTreeWidget::slotItemChanged(QTreeWidgetItem* item, int column)
{
    if (column != KColumnName || !item)
        return;

    // Commit an in-place universe rename (armed in keyPressEvent()).
    if ((item->flags() & Qt::ItemIsEditable) && item->data(KColumnName, PROP_UNIVERSE).isValid())
    {
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);

        const quint32 uni = item->data(KColumnName, PROP_UNIVERSE).toUInt();
        InputOutputMap *iomap = m_doc->inputOutputMap();
        const QString oldName = iomap ? iomap->getUniverseNameByID(uni) : QString();
        const QString newName = item->text(KColumnName).trimmed();
        // Universe::nameChanged() has no UI-side listener that would refresh
        // this row on its own, unlike a fixture rename (which some other
        // Doc signal already redraws elsewhere) -- so both the applied and
        // the rejected case set the row's text back explicitly, rather than
        // leaving the bare plain name shown in keyPressEvent() behind.
        QString shownName = oldName;
        if (iomap != NULL && newName.isEmpty() == false && newName != oldName)
        {
            iomap->setUniverseName(int(uni), newName);
            m_doc->setModified();
            shownName = newName;
        }
        QString uniLabel = shownName;
        const QString dest = universeDestination(m_doc, uni);
        if (!dest.isEmpty())
            uniLabel += QString("   —   %1").arg(dest);
        item->setText(KColumnName, uniLabel);
        return;
    }

    // In-place fixture rename (Return / F2 on a fixture row). We only act when
    // the row is ARMED (ItemIsEditable set in keyPressEvent) — fixture items are
    // not otherwise editable, so this cleanly ignores the itemChanged storm that
    // setText()/setData() fire while the tree is being (re)built.
    if ((item->flags() & Qt::ItemIsEditable)
        && item->data(KColumnName, PROP_ID).isValid()
        && item->data(KColumnName, PROP_HEAD).isValid() == false
        && item->data(KColumnName, PROP_CHANNEL).isValid() == false)
    {
        // Disarm FIRST so the setText()/setFlags() below can't re-enter here.
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);

        bool ok = false;
        quint32 fid = item->data(KColumnName, PROP_ID).toString().toUInt(&ok);
        Fixture *fxi = ok ? m_doc->fixture(fid) : NULL;
        if (fxi != NULL)
        {
            const QString newName = item->text(KColumnName).trimmed();
            if (newName.isEmpty() == false && newName != fxi->name()
                && AppUtil::refuseTakenFixtureName(this, m_doc, fid, newName) == false)
            {
                fxi->setName(newName);
                m_doc->setModified();
            }
            else if (item->text(KColumnName) != fxi->name())
            {
                // Reject empty/no-op edits — restore the shown name.
                item->setText(KColumnName, fxi->name());
            }
        }
        return;
    }

    // In-place group rename (Return / F2 on a group row), armed the same way.
    // As everywhere else a group is renamed (its editor, Layers): an empty or
    // unchanged name keeps the old one; group names need not be unique.
    if ((item->flags() & Qt::ItemIsEditable)
        && item->data(KColumnName, PROP_GROUP).isValid())
    {
        // Disarm FIRST so the setText()/setFlags() below can't re-enter here.
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);

        FixtureGroup *grp = m_doc->fixtureGroup(item->data(KColumnName, PROP_GROUP).toUInt());
        if (grp != NULL)
        {
            const QString newName = item->text(KColumnName).trimmed();
            if (newName.isEmpty() == false && newName != grp->name())
            {
                grp->setName(newName);
                m_doc->setModified();
            }
            if (item->text(KColumnName) != grp->name())
                item->setText(KColumnName, grp->name());
        }
        return;
    }

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
    // The tree accepts internal group-onto-folder drops, and — when power
    // circuits are shown — fixture-onto-circuit drops. Fixture drags are
    // otherwise for external targets (the group editor grid), not this tree.
    if (event->mimeData()->hasFormat(GROUP_DRAG_MIME_TYPE) ||
        ((m_showPower || m_showGroups) && event->mimeData()->hasFormat(FIXTURE_DRAG_MIME_TYPE)))
        event->acceptProposedAction();
    else
        event->ignore();
}

void FixtureTreeWidget::dragMoveEvent(QDragMoveEvent* event)
{
    QTreeWidgetItem *target = itemAt(event->position().toPoint());

    // Fixtures (or a group's fixtures) onto a universe: patch them into it.
    if (m_showGroups && dropUniverse(target).isValid()
        && (event->mimeData()->hasFormat(FIXTURE_DRAG_MIME_TYPE)
            || event->mimeData()->hasFormat(GROUP_DRAG_MIME_TYPE)))
    {
        event->acceptProposedAction();
        return;
    }

    if (event->mimeData()->hasFormat(GROUP_DRAG_MIME_TYPE))
    {
        // Only a group folder, a group (drop alongside it), or the empty root
        // are valid targets for a group drop.
        const bool valid = target == NULL ||
                target->data(KColumnName, PROP_FOLDER).isValid() ||
                target->data(KColumnName, PROP_GROUP).isValid();
        if (valid)
            event->acceptProposedAction();
        else
            event->ignore();
        return;
    }

    if (m_showPower && event->mimeData()->hasFormat(FIXTURE_DRAG_MIME_TYPE))
    {
        // A circuit node, a direct-source node, or a plain (multi-circuit)
        // source node are all valid fixture targets — dropped straight on a
        // source with no circuit picked lands on its first circuit. The bare
        // "Power" root (sentinel source index -1) is not a valid target.
        const QVariant srcVar = (target != NULL) ? target->data(KColumnName, PROP_SOURCE) : QVariant();
        const bool valid = target != NULL &&
                (target->data(KColumnName, PROP_CIRCUIT).isValid() ||
                 (srcVar.isValid() && srcVar.toInt() >= 0));
        if (valid)
            event->acceptProposedAction();
        else
            event->ignore();
        return;
    }

    event->ignore();
}

void FixtureTreeWidget::dropEvent(QDropEvent* event)
{
    QTreeWidgetItem *target = itemAt(event->position().toPoint());

    const QVariant dropUni = m_showGroups ? dropUniverse(target) : QVariant();
    if (dropUni.isValid()
        && (event->mimeData()->hasFormat(FIXTURE_DRAG_MIME_TYPE)
            || event->mimeData()->hasFormat(GROUP_DRAG_MIME_TYPE)))
    {
        const QList<quint32> ids = droppedFixtures(event->mimeData());
        if (!ids.isEmpty())
            emit fixturesDroppedOnUniverse(ids, dropUni.toUInt());
        event->acceptProposedAction();
        return;
    }

    const QVariant dropSrcVar = (target != NULL) ? target->data(KColumnName, PROP_SOURCE) : QVariant();
    const bool validPowerTarget = target != NULL &&
            (target->data(KColumnName, PROP_CIRCUIT).isValid() ||
             (dropSrcVar.isValid() && dropSrcVar.toInt() >= 0));

    if (m_showPower && event->mimeData()->hasFormat(FIXTURE_DRAG_MIME_TYPE) && validPowerTarget)
    {
        const int sourceIdx = dropSrcVar.toInt();
        // A circuit-tagged node carries its own circuit index; a plain source
        // node (no PROP_CIRCUIT) means "no circuit picked" -> its first circuit.
        const QVariant cirVar = target->data(KColumnName, PROP_CIRCUIT);
        const int circuitIdx = cirVar.isValid() ? cirVar.toInt() : 0;

        QList<quint32> ids;
        QByteArray data = event->mimeData()->data(FIXTURE_DRAG_MIME_TYPE);
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            quint32 fid = 0;
            stream >> fid;
            ids << fid;
        }

        if (ids.isEmpty() == false)
            emit fixturesDroppedOnCircuit(ids, sourceIdx, circuitIdx);
        event->acceptProposedAction();
        return;
    }

    if (event->mimeData()->hasFormat(GROUP_DRAG_MIME_TYPE) == false)
    {
        event->ignore();
        return;
    }

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

QVariant FixtureTreeWidget::dropUniverse(QTreeWidgetItem *target) const
{
    if (target == NULL)
        return QVariant();
    const QVariant own = target->data(KColumnName, PROP_UNIVERSE);
    if (own.isValid())
        return own;
    // A fixture row (not a head) listed under a universe row.
    if (target->data(KColumnName, PROP_ID).isValid() && !target->data(KColumnName, PROP_HEAD).isValid()
        && target->parent() != NULL)
        return target->parent()->data(KColumnName, PROP_UNIVERSE);
    return QVariant();
}

QList<quint32> FixtureTreeWidget::droppedFixtures(const QMimeData *mime) const
{
    QList<quint32> ids;
    if (mime->hasFormat(FIXTURE_DRAG_MIME_TYPE))
    {
        QByteArray data = mime->data(FIXTURE_DRAG_MIME_TYPE);
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            quint32 fid = 0;
            stream >> fid;
            if (!ids.contains(fid))
                ids << fid;
        }
    }
    else if (mime->hasFormat(GROUP_DRAG_MIME_TYPE))
    {
        // A group stands for its fixtures, in its own head order.
        QByteArray data = mime->data(GROUP_DRAG_MIME_TYPE);
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            quint32 gid = 0;
            stream >> gid;
            FixtureGroup *grp = m_doc->fixtureGroup(gid);
            if (grp == NULL)
                continue;
            // Braces: foreach expands to an if/else, so a braceless if
            // around one is a dangling else.
            foreach (quint32 fid, grp->fixtureList())
            {
                if (!ids.contains(fid))
                    ids << fid;
            }
        }
    }
    return ids;
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
    , m_showPower(false)
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
    if (m_doc != NULL)
    {
        connect(m_doc, &Doc::stageLooksChanged, this, &FixtureTreeWidget::refreshStageLookIcons);
        /* A fixture renamed, given a look or output rules anywhere: its
           rows' names and icons, and its groups' icons, in place -- in
           every list built on this tree, not only the Fixture Manager's. */
        connect(m_doc, &Doc::fixtureChanged, this, &FixtureTreeWidget::refreshFixtureName);
    }
}

void FixtureTreeWidget::refreshStageLookIcons()
{
    const bool was = blockSignals(true);      // not an edit: no itemChanged
    for (QTreeWidgetItemIterator it(this); *it != NULL; ++it)
    {
        QTreeWidgetItem *item = *it;
        if (item->data(KColumnName, PROP_LOOK).isValid())
        {
            Fixture *fxi = m_doc->fixture(item->data(KColumnName, PROP_ID).toUInt());
            if (fxi == NULL)
                continue;
            item->setIcon(KColumnName, ItemIcons::fixtureIcon(fxi));
            item->setData(KColumnName, PROP_LOOK, int(drawnStageLook(fxi)));
        }
        else if (item->data(KColumnName, PROP_GROUP).isValid()
                 && !item->data(KColumnName, PROP_ID).isValid())
        {
            if (FixtureGroup *grp = m_doc->fixtureGroup(item->data(KColumnName, PROP_GROUP).toUInt()))
                item->setIcon(KColumnName, ItemIcons::groupIcon(m_doc, grp));
        }
    }
    blockSignals(was);
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

    if (flags & ShowPower)
        m_showPower = true;

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

void FixtureTreeWidget::refreshFixtureName(quint32 id)
{
    Fixture *fxi = m_doc->fixture(id);
    if (fxi == NULL)
        return;
    const bool was = blockSignals(true);      // not an edit: no itemChanged
    for (QTreeWidgetItemIterator it(this); *it != NULL; ++it)
    {
        QTreeWidgetItem *item = *it;
        const QVariant var = item->data(KColumnName, PROP_ID);
        // A group it is in may now be all one look, or no longer.
        if (!var.isValid() && item->data(KColumnName, PROP_GROUP).isValid())
        {
            if (FixtureGroup *grp = m_doc->fixtureGroup(item->data(KColumnName, PROP_GROUP).toUInt()))
                if (grp->fixtureList().contains(id))
                    item->setIcon(KColumnName, ItemIcons::groupIcon(m_doc, grp));
            continue;
        }
        if (!var.isValid() || var.toUInt() != id
            || item->data(KColumnName, PROP_HEAD).isValid())
            continue;
        if (item->text(KColumnName) != fxi->name())
            item->setText(KColumnName, fxi->name());
        /* Its stage look or its output rules changed (Fixture Properties,
           Stage look ▸, Output rules): its icon. Redone every time -- the
           badge is not in PROP_LOOK. */
        if (item->data(KColumnName, PROP_LOOK).isValid())
        {
            item->setIcon(KColumnName, ItemIcons::fixtureIcon(fxi));
            item->setData(KColumnName, PROP_LOOK, int(drawnStageLook(fxi)));
        }
    }
    blockSignals(was);
}

namespace
{
    bool filterRow(QTreeWidgetItem *item, const QString &needle, int *fixtures)
    {
        bool anyChild = false;
        for (int i = 0; i < item->childCount(); ++i)
            if (filterRow(item->child(i), needle, fixtures))
                anyChild = true;

        bool self = needle.isEmpty();
        for (int c = 0; !self && c < item->columnCount(); ++c)
            self = item->text(c).contains(needle, Qt::CaseInsensitive);
        const bool visible = self || anyChild;
        item->setHidden(!visible);
        if (!needle.isEmpty() && anyChild)
            item->setExpanded(true);   // reveal a match inside a collapsed folder
        if (visible && self && item->data(KColumnName, PROP_ID).isValid()
            && !item->data(KColumnName, PROP_HEAD).isValid())
            ++*fixtures;
        return visible;
    }
}

int FixtureTreeWidget::setTextFilter(const QString &text)
{
    const QString needle = text.trimmed();
    int fixtures = 0;
    for (int i = 0; i < topLevelItemCount(); ++i)
        filterRow(topLevelItem(i), needle, &fixtures);
    return fixtures;
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
    item->setIcon(KColumnName, ItemIcons::fixtureIcon(fixture));
    item->setData(KColumnName, PROP_LOOK, int(drawnStageLook(fixture)));
    // The badge's meaning, in words (Output rules…).
    item->setToolTip(KColumnName, fixture->outputRules() == Fixture::OutputRules()
                         ? QString()
                         : tr("Output rules: %1").arg(OutputRuleKeeper::describe(fixture->outputRules())));
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
        // An inventory fixture's universe is a placeholder, not a fact.
        item->setText(m_uniColumn, fixture->isPatched() ? QString("%1").arg(fixture->universe() + 1)
                                                        : QString::fromUtf8("\xE2\x80\x94"));
        item->setTextAlignment(m_uniColumn, Qt::AlignHCenter | Qt::AlignVCenter);
    }

    if (m_addressColumn && fixture->addressClash())
    {
        item->setText(m_addressColumn, tr("clashes at %1.%2").arg(fixture->universe() + 1)
                                                              .arg(fixture->address() + 1));
        item->setForeground(m_addressColumn, QBrush(QColor(220, 70, 70)));
        Fixture *owner = m_doc->fixture(m_doc->fixtureForAddress(fixture->universeAddress()));
        item->setToolTip(m_addressColumn,
            tr("The show file patches it at universe %1, address %2, which %3 already uses. "
               "It is loaded unpatched so it cannot write over it -- give it a home with "
               "Patch…, or drag it onto a universe.")
                .arg(fixture->universe() + 1).arg(fixture->address() + 1)
                .arg(owner != NULL ? owner->name() : tr("another fixture")));
    }
    else if (m_addressColumn && !fixture->isPatched())
    {
        item->setText(m_addressColumn, fixture->connection() == Fixture::Network
                          ? (fixture->ipAddress().isEmpty() ? tr("unpatched (network)")
                                                            : tr("unpatched (%1)").arg(fixture->ipAddress()))
                          : tr("unpatched"));
    }
    else if (m_addressColumn)
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
    item->setIcon(KColumnName, ItemIcons::groupIcon(m_doc, grp));
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

void FixtureTreeWidget::setShowHeads(bool on)
{
    if (m_showHeads == on)
        return;
    m_showHeads = on;
    // updateTree() clears and repopulates the whole tree, which emits
    // itemSelectionChanged() (nothing selected) partway through. This is
    // always called from within FixtureManager::fixtureGroupSelected() or
    // clearRightPane(), which are ALREADY mid-flight handling that exact
    // signal for the fixture tree -- an unblocked emission here reenters
    // FixtureManager::slotSelectionChanged() while the group editor is
    // still being wired up, which falls through to its "nothing selected"
    // branch and tears the brand-new editor right back down (regression:
    // a fixture group's layout editor would flash open and immediately
    // disappear). Block signals for just this rebuild -- callers already
    // own the selection-driven state change and don't need it echoed back.
    /* DEFERRED, not immediate.
     *
       updateTree() destroys and rebuilds every row. Both callers reach here
       from inside FixtureManager::slotSelectionChanged(), i.e. from inside
       QTreeWidget's own itemSelectionChanged emission -- so the item the user
       just clicked is deleted while Qt is still part-way through handling that
       click. Qt does not crash, it simply carries on with a current-item that
       no longer exists and reassigns selection to whatever now sits in that
       position: "when I click on a fixture group it's not displaying the
       layout .. just jumping around in the tree."

       Running it on the next turn of the event loop lets the click finish
       first. updateTree() restores the selection by id afterwards (see
       currentSelection()/restoreSelection()), so the group stays selected and
       its editor stays open. Signals stay blocked for the rebuild itself for
       the reason described above. */
    QTimer::singleShot(0, this, [this]() {
        const bool wasBlocked = blockSignals(true);
        updateTree();
        blockSignals(wasBlocked);
    });
}

/* Which item is selected, in terms that survive the tree being thrown away and
   rebuilt. Pointers do not; a group id or a fixture id does. */
FixtureTreeWidget::Selection FixtureTreeWidget::currentSelection() const
{
    Selection sel;
    const QList<QTreeWidgetItem *> items = selectedItems();
    if (items.isEmpty())
        return sel;
    QTreeWidgetItem *it = items.first();
    const QVariant grp = it->data(KColumnName, PROP_GROUP);
    const QVariant uni = it->data(KColumnName, PROP_UNIVERSE);
    const QVariant fid = it->data(KColumnName, PROP_ID);
    if (grp.isValid())      { sel.kind = Selection::Group;    sel.id = grp.toUInt(); }
    else if (uni.isValid()) { sel.kind = Selection::Universe; sel.id = uni.toUInt(); }
    else if (fid.isValid() && it->data(KColumnName, PROP_HEAD).isValid() == false)
                            { sel.kind = Selection::Fixture;  sel.id = fid.toUInt(); }
    return sel;
}

/* Put the selection back after a rebuild, WITHOUT telling anyone.
 *
 * updateTree() throws the whole tree away, so whatever was selected is gone and
 * the widget comes back with nothing selected. Callers that rebuild for their
 * own reasons -- setShowHeads() opening a group's layout editor, say -- then sit
 * on a tree whose selection contradicts the pane they just opened, and the next
 * selection signal from ANY source tears that pane down. Blocking the signal
 * during the rebuild (which this file already does) only defers that: it hides
 * the reentry, it does not fix the state. Restoring the selection does.
 */
void FixtureTreeWidget::restoreSelection(const Selection &sel)
{
    if (sel.kind == Selection::None)
        return;

    const int role = (sel.kind == Selection::Group)    ? PROP_GROUP
                   : (sel.kind == Selection::Universe) ? PROP_UNIVERSE
                                                       : PROP_ID;
    QTreeWidgetItemIterator it(this);
    while (*it != nullptr)
    {
        QTreeWidgetItem *item = *it;
        const QVariant v = item->data(KColumnName, role);
        const bool isHead = item->data(KColumnName, PROP_HEAD).isValid();
        if (v.isValid() && v.toUInt() == sel.id
            && (sel.kind != Selection::Fixture || isHead == false))
        {
            const bool was = blockSignals(true);
            setCurrentItem(item);
            item->setSelected(true);
            blockSignals(was);
            return;
        }
        ++it;
    }
}

void FixtureTreeWidget::updateTree()
{
    const Selection keep = currentSelection();
    clear();
    m_groupFolders.clear();
    m_universesCount = 0;
    m_fixturesCount = 0;
    m_channelsCount = 0;

    if (m_showGroups == true)
    {
        // A dedicated "Fixture Groups" root, peer to "Power" and
        // "Universes" below -- always present (even with zero groups yet)
        // so group CREATION has a right-clickable home of its own
        // (FixtureManager::slotContextMenuRequested()'s "New Group..."),
        // instead of a toolbar/menu-bar action. Drop-enabled and tagged with
        // an empty PROP_FOLDER so dragging a group onto it un-files it from
        // any sub-folder, same as dropping onto any other folder node.
        //
        // setData()/setFlags() emit itemChanged() SYNCHRONOUSLY (see the
        // keyPressEvent()/slotItemChanged() comments above for the other
        // place this file already works around it). Every ordinary folder
        // item is immune -- slotItemChanged() only treats a PROP_FOLDER item
        // as user-renamed when its display text differs from its path's
        // last segment, which is never true for a freshly-created folder
        // (both come from the same string). This root breaks that
        // invariant on purpose (text "Fixture Groups", path "") -- without
        // blockSignals, that mismatch fooled slotItemChanged() into treating
        // construction as a rename, which emitted groupFolderRenamed("",
        // "Fixture Groups") into FixtureManager::slotGroupFolderRenamed(),
        // which re-entered updateTree() (via updateView()) *while this very
        // call was still building it* -- clear() there deleted the
        // groupsRoot this call still held a pointer to, and everything after
        // used it dangling. Crashed intermittently (classic use-after-free:
        // heap-state dependent), reported twice with an identical
        // EXC_BAD_ACCESS in FixtureTreeWidget::updateTree() -> ...
        // setFlags() -> QTreeWidgetPrivate::dataChanged().
        blockSignals(true);
        QTreeWidgetItem *groupsRoot = new QTreeWidgetItem(this);
        groupsRoot->setText(KColumnName, tr("Fixture Groups"));
        groupsRoot->setIcon(KColumnName, QIcon(":/folder.png"));
        groupsRoot->setData(KColumnName, PROP_FOLDER, QString());
        groupsRoot->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);
        blockSignals(false);
        groupsRoot->setExpanded(true);
        m_groupFolders[QString()] = groupsRoot;

        foreach (FixtureGroup* grp, m_doc->fixtureGroups())
        {
            // Nest the group under its folder path (if any), under the root.
            QTreeWidgetItem* grpItem = new QTreeWidgetItem(groupFolderItem(grp->path()));
            updateGroupItem(grpItem, grp);
        }
    }

    // The Inventory: fixtures staged in the show but not yet given a universe
    // and address. A peer of Fixture Groups / Power / Universes, always
    // present in the manager so it has a right-clickable home of its own.
    QTreeWidgetItem *inventoryRoot = NULL;
    QTreeWidgetItem *clashRoot = NULL;
    QMap<QString, QTreeWidgetItem *> invManuf;    // manufacturer -> folder
    QMap<QString, QTreeWidgetItem *> invModel;    // manufacturer\x1fmodel -> folder
    auto newInventoryFolder = [this](QTreeWidgetItem *parent, const QString &name) {
        QTreeWidgetItem *f = new QTreeWidgetItem(parent);
        f->setText(KColumnName, name);
        f->setIcon(KColumnName, QIcon(":/folder.png"));
        f->setData(KColumnName, PROP_INVENTORY, 2);
        f->setData(KColumnName, PROP_INVENTORY_NAME, name);
        f->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        f->setExpanded(true);
        return f;
    };
    auto ensureInventoryRoot = [&]() {
        if (inventoryRoot == NULL)
        {
            blockSignals(true);
            inventoryRoot = new QTreeWidgetItem(this);
            inventoryRoot->setText(KColumnName, tr("Inventory"));
            inventoryRoot->setIcon(KColumnName, QIcon(":/folder.png"));
            inventoryRoot->setData(KColumnName, PROP_INVENTORY, 1);
            inventoryRoot->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            blockSignals(false);
            inventoryRoot->setExpanded(true);
        }
        return inventoryRoot;
    };
    if (m_showGroups == true)
        ensureInventoryRoot();

    // Power sources/circuits, peer to the fixture-group folders and the
    // Universes folder below. A source with 0 or 1 circuits feeds fixtures
    // directly (mirrors the "(direct)" case in FixtureManager's right-click
    // circuit menu); a source with 2+ circuits fans out into circuit nodes.
    if (m_showPower == true)
    {
        const QList<PowerSource>& sources = m_doc->powerDistribution()->sources();

        // Always present (even with zero sources yet) so it can be selected
        // to switch the pane to the power view and add the first source there.
        QTreeWidgetItem *powerRoot = new QTreeWidgetItem(this);
        powerRoot->setText(KColumnName, tr("Power"));
        powerRoot->setIcon(KColumnName, QIcon(":/folder.png"));
        powerRoot->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        powerRoot->setData(KColumnName, PROP_SOURCE, -1); // sentinel: "the power tree", not a specific source
        powerRoot->setExpanded(true);

        for (int s = 0; s < sources.size(); s++)
        {
            const PowerSource& src = sources.at(s);
            QTreeWidgetItem *srcItem = new QTreeWidgetItem(powerRoot);
            srcItem->setData(KColumnName, PROP_SOURCE, s);
            srcItem->setIcon(KColumnName, QIcon(":/folder.png"));
            srcItem->setExpanded(true);

            if (src.circuits.size() <= 1)
            {
                // Direct source: fixtures nest right under it (circuit 0,
                // created on demand by PowerDistribution::assignFixture).
                srcItem->setText(KColumnName, tr("%1  (direct)").arg(src.name));
                srcItem->setData(KColumnName, PROP_CIRCUIT, 0);
                if (src.circuits.size() == 1)
                {
                    foreach (quint32 fid, src.circuits.first().fixtures)
                    {
                        Fixture *fxi = m_doc->fixture(fid);
                        if (fxi == NULL)
                            continue;
                        QTreeWidgetItem *fItem = new QTreeWidgetItem(srcItem);
                        updateFixtureItem(fItem, fxi);
                    }
                }
            }
            else
            {
                srcItem->setText(KColumnName, src.name);
                for (int c = 0; c < src.circuits.size(); c++)
                {
                    const PowerCircuit& cir = src.circuits.at(c);
                    QTreeWidgetItem *cirItem = new QTreeWidgetItem(srcItem);
                    cirItem->setText(KColumnName, cir.name);
                    cirItem->setIcon(KColumnName, QIcon(":/folder.png"));
                    cirItem->setData(KColumnName, PROP_SOURCE, s);
                    cirItem->setData(KColumnName, PROP_CIRCUIT, c);
                    cirItem->setExpanded(true);
                    foreach (quint32 fid, cir.fixtures)
                    {
                        Fixture *fxi = m_doc->fixture(fid);
                        if (fxi == NULL)
                            continue;
                        QTreeWidgetItem *fItem = new QTreeWidgetItem(cirItem);
                        updateFixtureItem(fItem, fxi);
                    }
                }
            }
        }
    }

    // In the Fixture Manager (groups shown) keep all universes tidily under a
    // single "Universes" folder, separate from the fixture-group folders --
    // always present (even with zero fixtures patched into it yet), same
    // "always present" convention as Fixture Groups/Power above, so a
    // universe added via the Connections/Devices editor shows up here
    // immediately with a right-clickable row to patch the first fixture
    // into. Previously this whole folder (and every universe row in it) was
    // only ever created by iterating m_doc->fixtures() below, so a universe
    // with no fixture patched into it yet -- exactly the state right after
    // adding one in Connections -- had no row here at all: real bug, not a
    // display gap.
    QTreeWidgetItem *universesRoot = NULL;

    if (m_showGroups == true)
    {
        universesRoot = new QTreeWidgetItem(this);
        universesRoot->setText(KColumnName, tr("Universes"));
        universesRoot->setIcon(KColumnName, QIcon(":/folder.png"));
        universesRoot->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        universesRoot->setExpanded(true);

        for (quint32 uni = 0; uni < m_doc->inputOutputMap()->universesCount(); uni++)
        {
            QTreeWidgetItem *topItem = new QTreeWidgetItem(universesRoot);
            QString uniLabel = m_doc->inputOutputMap()->getUniverseNameByID(uni);
            const QString dest = universeDestination(m_doc, uni);
            if (!dest.isEmpty())
                uniLabel += QString("   —   %1").arg(dest);
            topItem->setText(KColumnName, uniLabel);
            topItem->setIcon(KColumnName, QIcon(":/folder.png"));
            topItem->setData(KColumnName, PROP_UNIVERSE, uni);
            topItem->setExpanded(true);
            if (m_channelSelection)
            {
                topItem->setFlags(topItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
                topItem->setCheckState(KColumnName, Qt::Unchecked);
            }
            m_universesCount++;
        }
    }

    foreach (Fixture* fixture, m_doc->fixtures())
    {
        Q_ASSERT(fixture != NULL);

        // Staged fixtures live under the Inventory, never under a universe --
        // a placeholder 0/0 must not make one look like it sits in universe 1.
        // Channel selection has no use for them: they have no address.
        /* An address clash: patched by the file onto channels another fixture
           holds (see Fixture::addressClash()). Listed on its own, at the top,
           rather than among the inventory -- it is a problem with the show,
           not a fixture waiting to be rigged. */
        if (fixture->addressClash() && m_showGroups && !m_channelSelection)
        {
            if (clashRoot == NULL)
            {
                blockSignals(true);
                clashRoot = new QTreeWidgetItem;
                insertTopLevelItem(0, clashRoot);
                clashRoot->setIcon(KColumnName, QIcon(":/folder.png"));
                clashRoot->setData(KColumnName, PROP_CLASHES, true);
                clashRoot->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                clashRoot->setForeground(KColumnName, QBrush(QColor(220, 70, 70)));
                clashRoot->setToolTip(KColumnName,
                    tr("Fixtures the show file patches onto channels another fixture already "
                       "uses. They are loaded unpatched (they send no DMX) and saved as they "
                       "were, until they are given a home."));
                blockSignals(false);
                clashRoot->setExpanded(true);
            }
            QTreeWidgetItem *cItem = new QTreeWidgetItem(clashRoot);
            updateFixtureItem(cItem, fixture);
            clashRoot->setText(KColumnName, tr("Address clashes (%1)").arg(clashRoot->childCount()));
            m_fixturesCount++;
            continue;
        }

        if (!fixture->isPatched())
        {
            if (m_channelSelection)
                continue;
            // Manufacturer -> model -> fixture, so a big inventory reads like the
            // bill of materials it is.
            const QString manuf = fixture->groupingManufacturer();
            const QString model = fixture->groupingModel();
            QTreeWidgetItem *&mNode = invManuf[manuf];
            if (mNode == NULL)
                mNode = newInventoryFolder(ensureInventoryRoot(), manuf);
            QTreeWidgetItem *&modelNode = invModel[manuf + QLatin1Char('\x1f') + model];
            if (modelNode == NULL)
                modelNode = newInventoryFolder(mNode, model);
            QTreeWidgetItem *iItem = new QTreeWidgetItem(modelNode);
            updateFixtureItem(iItem, fixture);
            m_fixturesCount++;
            continue;
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
        // Not pre-seeded above (m_showGroups == false, e.g. a fixture-
        // selection dialog) ? Create it lazily, same as before.
        if (topItem == NULL)
        {
            topItem = new QTreeWidgetItem(uniParent);
            QString uniLabel = m_doc->inputOutputMap()->getUniverseNameByID(uni);
            const QString dest = universeDestination(m_doc, uni);
            if (!dest.isEmpty())
                uniLabel += QString("   —   %1").arg(dest);
            topItem->setText(KColumnName, uniLabel);
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

    if (inventoryRoot != NULL)
    {
        // Alphabetical at every level, sorted on the bare names before the
        // counts go on.
        inventoryRoot->sortChildren(KColumnName, Qt::AscendingOrder);
        for (int m = 0; m < inventoryRoot->childCount(); m++)
        {
            QTreeWidgetItem *mNode = inventoryRoot->child(m);
            mNode->sortChildren(KColumnName, Qt::AscendingOrder);
            for (int d = 0; d < mNode->childCount(); d++)
                mNode->child(d)->sortChildren(KColumnName, Qt::AscendingOrder);
        }
        refreshInventoryCounts();
    }

    /* Names more than one fixture has -- a show from before names were
       unique, or merged with another ("two AT1-3z lights .. one in the upper
       left corner"). Listed at the top, a row per name, to be numbered apart
       from the Fixture Manager's menu. The fixtures stay where they are. */
    if (m_showGroups && !m_channelSelection)
    {
        const QMap<QString, QList<quint32> > dups = m_doc->duplicateFixtureNames();
        if (!dups.isEmpty())
        {
            blockSignals(true);
            QTreeWidgetItem *dupRoot = new QTreeWidgetItem;
            insertTopLevelItem(0, dupRoot);
            dupRoot->setText(KColumnName, tr("Duplicate names (%1)").arg(dups.size()));
            dupRoot->setIcon(KColumnName, QIcon(":/folder.png"));
            dupRoot->setData(KColumnName, PROP_DUPLICATES, true);
            dupRoot->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            dupRoot->setForeground(KColumnName, QBrush(QColor(224, 160, 48)));
            dupRoot->setToolTip(KColumnName,
                tr("More than one fixture has each of these names. Right-click to number "
                   "them apart: the patched one keeps the name."));
            for (auto it = dups.constBegin(); it != dups.constEnd(); ++it)
            {
                QTreeWidgetItem *row = new QTreeWidgetItem(dupRoot);
                row->setText(KColumnName, tr("%1 — %2 fixtures").arg(it.key()).arg(it.value().size()));
                row->setData(KColumnName, PROP_DUPLICATES, true);
                row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                QStringList where;
                foreach (quint32 fid, it.value())
                {
                    Fixture *f = m_doc->fixture(fid);
                    if (f == NULL)
                        continue;
                    where << (f->isPatched() ? tr("ID %1: universe %2, address %3").arg(fid)
                                                   .arg(f->universe() + 1).arg(f->address() + 1)
                                             : tr("ID %1: in the inventory").arg(fid));
                }
                row->setToolTip(KColumnName, where.join(QStringLiteral("\n")));
            }
            blockSignals(false);
            dupRoot->setExpanded(true);
        }
    }

    header()->resizeSections(QHeaderView::ResizeToContents);

    restoreSelection(keep);
}

void FixtureTreeWidget::refreshInventoryCounts()
{
    QTreeWidgetItem *root = NULL;
    for (int i = 0; i < topLevelItemCount() && root == NULL; i++)
        if (topLevelItem(i)->data(KColumnName, PROP_INVENTORY).toInt() == 1)
            root = topLevelItem(i);
    if (root == NULL)
        return;

    int total = 0;
    for (int m = root->childCount() - 1; m >= 0; m--)
    {
        QTreeWidgetItem *mNode = root->child(m);
        int manufTotal = 0;
        for (int d = mNode->childCount() - 1; d >= 0; d--)
        {
            QTreeWidgetItem *dNode = mNode->child(d);
            if (dNode->childCount() == 0)
            {
                delete dNode;
                continue;
            }
            dNode->setText(KColumnName, QStringLiteral("%1 (%2)")
                               .arg(dNode->data(KColumnName, PROP_INVENTORY_NAME).toString())
                               .arg(dNode->childCount()));
            manufTotal += dNode->childCount();
        }
        if (manufTotal == 0)
        {
            delete mNode;
            continue;
        }
        mNode->setText(KColumnName, QStringLiteral("%1 (%2)")
                           .arg(mNode->data(KColumnName, PROP_INVENTORY_NAME).toString()).arg(manufTotal));
        total += manufTotal;
    }
    // The root stays, empty or not: it is where Add to inventory lives.
    root->setText(KColumnName, total > 0 ? tr("Inventory (%1)").arg(total) : tr("Inventory"));
}



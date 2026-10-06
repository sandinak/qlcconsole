/*
  Q Light Controller Plus
  fixturetreewidget.h

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

#ifndef FIXTURETREEWIDGET_H
#define FIXTURETREEWIDGET_H

#include <QHash>

#include <QTreeWidget>

#include "grouphead.h"

class FixtureGroup;
class Fixture;
class Doc;

#define PROP_ID       Qt::UserRole
#define PROP_UNIVERSE Qt::UserRole + 1
#define PROP_GROUP    Qt::UserRole + 2
#define PROP_HEAD     Qt::UserRole + 3
#define PROP_CHANNEL  Qt::UserRole + 4
#define PROP_FOLDER   Qt::UserRole + 5  //!< group-folder path (string)
#define PROP_SOURCE   Qt::UserRole + 6  //!< power source index (int, into PowerDistribution::sources())
#define PROP_CIRCUIT  Qt::UserRole + 7  //!< power circuit index (int, within its source's circuits())
#define PROP_INVENTORY Qt::UserRole + 8 //!< marks the Inventory root (1) or one of its manufacturer/model folders (2)
#define PROP_INVENTORY_NAME Qt::UserRole + 9 //!< an Inventory folder's bare name, before its "(N)" count
#define PROP_CLASHES  Qt::UserRole + 10 //!< marks the "Address clashes" root

class FixtureTreeWidget final : public QTreeWidget
{
    Q_OBJECT
public:

    FixtureTreeWidget(Doc* doc, quint32 flags, QWidget *parent = 0);

    /** A selection expressed in terms that survive a rebuild. */
    struct Selection
    {
        enum Kind { None, Group, Universe, Fixture };
        Kind   kind = None;
        quint32 id = 0;
    };
    Selection currentSelection() const;
    /** Re-select after a rebuild, without emitting -- callers own the state
     *  change that prompted the rebuild. */
    void restoreSelection(const Selection &sel);

    void updateTree();

    /** Relabel the Inventory root and its folders with their fixture counts,
     *  dropping folders left empty. updateTree() does this after a rebuild; a
     *  caller that removes fixture rows in place calls it to catch up. */
    void refreshInventoryCounts();

    /** Toggle whether each fixture row expands to show its individual heads
     *  as draggable children (see headDragMimeType()) -- off by default even
     *  when constructed without the ShowHeads flag baked in, and meant to be
     *  switched on/off at runtime (e.g. only while a Fixture Group editor
     *  that can consume per-head drops is actually open), since permanently
     *  expanding every fixture's heads would bury the tree for anything with
     *  a large head count (a 96-head pixel bar, say). Rebuilds the tree. */
    void setShowHeads(bool on);

    /** Get a QTreeWidgetItem whose fixture ID is $id */
    QTreeWidgetItem* fixtureItem(quint32 id) const;

    /** Show fixture @p id's current name on every row that stands for it
     *  (a fixture can be listed under its universe and in groups). */
    void refreshFixtureName(quint32 id);

    /** Show only rows whose text (any column: name, manufacturer, model...)
     *  contains @p text, ignoring case; a folder stays shown -- and opens --
     *  while anything inside it matches. Empty shows everything. Returns how
     *  many fixture rows match. */
    int setTextFilter(const QString &text);

    /** Get a QTreeWidgetItem whose group ID is $id */
    QTreeWidgetItem* groupItem(quint32 id) const;

    /** Update a single fixture's data into a QTreeWidgetItem */
    void updateFixtureItem(QTreeWidgetItem* item, Fixture* fixture);

    /** Update a group's data to and under $item */
    void updateGroupItem(QTreeWidgetItem* item, const FixtureGroup* grp);

    /** Return the number of universes added to the tree during the last
     *  updateTree call */
    int universeCount();

    /** Return the number of fixtures added to the tree during the last
     *  updateTree call */
    int fixturesCount();

    /** Return the number of channels added to the tree during the last
     *  updateTree call */
    int channelsCount();

    /** MIME type used when dragging fixtures out of the tree (payload is a
     *  stream of fixture IDs). Matches the Programming tab's fixture MIME so
     *  drop targets can be shared. */
    static const char* fixtureDragMimeType();

    /** MIME type used when dragging fixture GROUPS within the tree (payload is
     *  a stream of group IDs). Used to drop groups onto folders. */
    static const char* groupDragMimeType();

    /** MIME type used when dragging individual fixture HEADS out of the tree
     *  (ShowHeads mode only) -- payload is a stream of (fixtureId, headIndex)
     *  quint32 pairs. Lets a drop target (the Fixture Group editor's grid)
     *  place just the dragged heads instead of a whole fixture's worth. */
    static const char* headDragMimeType();

signals:
    /** Emitted when one or more fixture groups are dropped onto a folder (or
     *  the root). $destPath is the target folder path ("" = root). */
    void groupsDroppedOnFolder(const QList<quint32>& groupIds, const QString& destPath);

    /** Emitted when the user commits an in-place rename of a group folder.
     *  @p oldPath  full old path ("Stage/Left")
     *  @p newName  new leaf name the user typed ("Right") */
    void groupFolderRenamed(const QString& oldPath, const QString& newName);

    /** Emitted when one or more fixtures are dropped onto a Power circuit (or a
     *  direct/wall-socket source) node. $sourceIdx/$circuitIdx index into
     *  Doc::powerDistribution()->sources(). Only emitted with ShowPower set. */
    void fixturesDroppedOnCircuit(const QList<quint32>& fixtureIds, int sourceIdx, int circuitIdx);

    /** Emitted when fixtures -- or fixture groups, standing for their
     *  fixtures -- are dropped onto a universe row (or onto a fixture listed
     *  under one). @p universe is zero-based. Only emitted with ShowGroups
     *  set, where the universe rows are. */
    void fixturesDroppedOnUniverse(const QList<quint32>& fixtureIds, quint32 universe);

protected:
    /** Provide fixture/group IDs for dragging onto drop targets (the group
     *  editor grid for fixtures, folders for groups). */
    QMimeData* mimeData(const QList<QTreeWidgetItem*> &items) const
        { return buildMimeData(items); }
    QMimeData* mimeData(const QList<QTreeWidgetItem*> items) const
        { return buildMimeData(items); }
private:
    /** The universe a drop on @p target means: a universe row's own, or the
     *  one of the universe row a fixture is listed under. Invalid otherwise. */
    QVariant dropUniverse(QTreeWidgetItem *target) const;
    /** Fixture ids carried by a fixture or group drag. */
    QList<quint32> droppedFixtures(const QMimeData *mime) const;

private:
    QMimeData* buildMimeData(const QList<QTreeWidgetItem*> &items) const;

    /** Accept group drops onto folders/root; ignore everything else. */
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

protected slots:
    void slotItemExpanded();
    void slotItemChanged(QTreeWidgetItem* item, int column);

private:
    /** Get/create the folder item for a "/"-separated fixture-group path
     *  (empty = root). Rebuilt each updateTree(). */
    QTreeWidgetItem* groupFolderItem(const QString& path);

private:
    Doc *m_doc;

    QHash<QString, QTreeWidgetItem*> m_groupFolders; //!< group folder nodes

    // counters
    int m_universesCount;
    int m_fixturesCount;
    int m_channelsCount;

    /************************************************************************
     * Tree visualization flags
     ************************************************************************/
public:

    enum TreeFlags
    {
        UniverseNumber   = 1 << 0,
        AddressRange     = 1 << 1,
        ChannelType      = 1 << 2,
        HeadsNumber      = 1 << 3,
        Manufacturer     = 1 << 4,
        Model            = 1 << 5,
        ShowGroups       = 1 << 6,
        ShowHeads        = 1 << 7,
        ChannelSelection = 1 << 8,
        ShowPower        = 1 << 9
    };

    void setFlags(quint32 flags);

    quint32 flags();

private:
    int m_uniColumn;
    int m_addressColumn;
    int m_typeColumn;
    int m_headsColumn;
    int m_manufColumn;
    int m_modelColumn;
    bool m_showGroups;
    bool m_showHeads;
    bool m_channelSelection;
    bool m_showPower;

    /****************************************************************************
     * Disabled items
     ****************************************************************************/
public:
    /** Disable (==prevent selection of) a list of fixtures */
    void setDisabledFixtures(const QList <quint32>& disabled);

    /** Disable (==prevent selection of) a list of heads */
    void setDisabledHeads(const QList <GroupHead>& disabled);

private:
    QList <quint32> m_disabledFixtures;
    QList <GroupHead> m_disabledHeads;

    /************************************************************************
     * Selected items
     ************************************************************************/
public:
    /** List of selected fixtures */
    QList <quint32> selectedFixtures();

    /** Get a list of selected fixture heads (valid only with ShowHeads flag) */
    QList <GroupHead> selectedHeads();

protected:
    void updateSelections();

private:
    QList <quint32> m_selectedFixtures;
    QList <GroupHead> m_selectedHeads;

    /****************************************************************************
     * Channels selection
     ****************************************************************************/
public:
    /** Set the channels that need to be selected in the tree.
     *  $channels is an array of 0/1 values where 0 means deselected
     *  and 1 means selected.
     *  The array indices represent absolute channel addresses
     *  across universes
     */
    void setChannelsMask(QByteArray channels);

private:
    QByteArray m_channelsMask;
};

#endif // FIXTURETREEWIDGET_H

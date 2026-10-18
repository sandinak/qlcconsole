/*
  Q Light Controller
  fixturemanager.h

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

#ifndef FIXTUREMANAGER_H
#define FIXTUREMANAGER_H

#include <QWidget>

#include "function.h"
#include "fixture.h"
#include "doc.h"

class QLCFixtureDefCache;
class FixtureGroupEditor;
class FixtureTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QTextBrowser;
class QTreeWidget;
class QTabWidget;
class OutputMap;
class QSplitter;
class QAction;
class QMenu;
class QToolBar;
class PowerDistributionWidget;
class UniverseUsageWidget;

/** @addtogroup ui_fixtures
 * @{
 */

#define KXMLQLCFixtureManager             QStringLiteral("FixtureManager")
#define KXMLQLCFixtureManagerSplitterSize QStringLiteral("SplitterSize")

class FixtureManager final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(FixtureManager)

    /********************************************************************
     * Initialization
     ********************************************************************/
public:
    FixtureManager(QWidget* parent, Doc* doc);
    ~FixtureManager();

    /** Get the singleton instance */
    static FixtureManager* instance();

    /** Match the main window's icon/text display preference
     *  (workspace/tabLabelMode) — mirrors Monitor::applyToolbarLabelMode(). */
    void applyToolbarLabelMode();

    /** Append the document-wide fixture actions (Import/Export/Remap/Fade
     *  Configuration) to @p menu — used by App to build its "Fixtures" menu
     *  bar entry. These aren't tied to any tree row, so they live in the
     *  menu bar rather than a right-click context menu or the toolbar. */
    void populateFixturesMenu(QMenu *menu);

private:
    /** The singleton FixtureManager instance */
    static FixtureManager* s_instance;

    /** The manager's own toolbar (add/remove/group/move/import/export/...) */
    QToolBar* m_toolbar;

    /********************************************************************
     * Doc signal handlers
     ********************************************************************/
public slots:
    /** Callback for Doc::fixtureRemoved() signals */
    void slotFixtureRemoved(quint32 id);

    /** Callback for Doc::channelsGroupRemoved() signals */
    void slotChannelsGroupRemoved(quint32 id);

    /** Callback that listens to mode change signals */
    void slotModeChanged(Doc::Mode mode);

    /** Callback that listens to fixture group removals */
    void slotFixtureGroupRemoved(quint32 id);

    /** Callback that listens to fixture group modifications */
    void slotFixtureGroupChanged(quint32 id);

    /** Callback that listens to workspace loading */
    void slotDocLoaded();

private:
    Doc* m_doc;

    /********************************************************************
     * Data view
     ********************************************************************/
public:
    /** Update the list of fixtures */
    void updateView();

    /** Update the list of channels group */
    void updateChannelsGroupView();

    /** Update icons on RDM view activation */
    void updateRDMView();

private:
    /** Open a fixture selector to add new fixtures */
    void addFixture();

    /** Stage a fixture in the inventory: no universe, no address. */
    void addToInventory();
    void addFixtureImpl(bool inventory, quint32 likeFixture = UINT_MAX,
                        const QString &manufacturer = QString());

    /** Release these fixtures' universe/address, sending them back to the inventory. */
    void returnToInventory(const QList<quint32> &fixtureIds);

    /** Open the Patch view for @p fixtureIds (empty = every staged fixture). */
    void patchInventory(const QList<quint32> &fixtureIds = QList<quint32>());
    /** Patch / re-address @p fixtureIds -- patched or not -- in the Patch
     *  dialog. With a valid @p universe they start packed into it, first free
     *  channels first (a drop onto that universe's row). */
    void patchOrReaddress(const QList<quint32> &fixtureIds,
                          quint32 universe = UINT_MAX);

    /** Open a channels selector to add new channels group */
    void addChannelsGroup();

    /** Remove a previously created fixture */
    void removeFixture();

    /** Delete a universe outright (only the last one can go -- see the
     *  implementation). Reached from the Universe row's context menu;
     *  renaming that same row is inline (Enter/F2/double-click) instead. */
    void deleteUniverse(quint32 universe);

    /** Universe rows selected in the fixtures list, in the order they were
     *  PICKED (a ctrl-click appends; a shift-click range appends in list
     *  order) -- the order "Patch N selected universes to…" puts them on a
     *  node's ports, as in the Connections tab. */
    QList<quint32> orderedSelectedUniverses();
    /** Bring m_universePickOrder in line with what is selected now. */
    void syncUniversePickOrder();

    /** Remove a previously created channels group */
    void removeChannelsGroup();

    /** Construct the list view and data view */
    void initDataView();

    /** Handle single fixture selection */
    void fixtureSelected(quint32 id);

    /** Handle fixture group selection */
    void fixtureGroupSelected(FixtureGroup* grp);

    /** Never leave the right pane collapsed after swapping its contents --
     *  an editor restored to 0 px wide looks like nothing happened. */
    void ensureRightPaneVisible();

    /** Copy the given fixtures (whole, all heads) and whole fixture groups
     *  (as sub-group blocks, preserving their relative layout) into $target,
     *  appending below whatever it already contains and growing the grid. */
    void copySelectionIntoGroup(FixtureGroup* target,
                                const QList<quint32>& fixtureIds,
                                const QList<quint32>& groupIds);

    /** Group ▸ Create Group(s) from Selection…: the fixtures and groups as
     *  one group (each group a block), or their heads as a group per head
     *  type, or a group per head type for each fixture, through
     *  CreateGroupsDialog. Returns the groups made (none when cancelled). */
    QList<FixtureGroup *> createGroupsFromSelection(const QList<quint32>& fixtureIds,
                                                    const QList<quint32>& groupIds);

    /** Create the text browser for displaying information */
    void createInfo();

    /** Delete whichever widget currently occupies the right pane (info / group
     *  layout editor / power view) so a new one can take splitter index 1. */
    void clearRightPane();

    /** Show the power-distribution view in the right pane (creating it if the
     *  slot currently holds the info browser or a group editor). */
    void showPower();

    /** Show the given universe's address-usage grid in the right pane. */
    void showUniverseUsage(quint32 universe);

private slots:
    /** Callback for fixture list selection changes */
    void slotSelectionChanged();

    /** Callback for channels group selection changes */
    void slotChannelsGroupSelectionChanged();

    /** Callback for mouse double clicks */
    void slotDoubleClicked(QTreeWidgetItem* item);

    /** Callback for channels group mouse double clicks */
    void slotChannelsGroupDoubleClicked(QTreeWidgetItem*);

    /** Callback for tab selection changes */
    void slotTabChanged(int index = 0);

    /** Callback for fixture tree item expand/collapse */
    void slotFixtureItemExpanded();

    void slotDisplayFixtureInfo(QString &info);

    /** Filter the fixtures tree as the search box's text changes
     *  (FixtureTreeWidget::setTextFilter): a folder stays visible whenever
     *  something inside it matches. */
    void slotFixturesSearchChanged(const QString &text);

private:
    /** Select a fixture group */
    void selectGroup(quint32 id);

    /** Get a CSS style sheet & HTML header for fixture info */
    QString fixtureInfoStyleSheetHeader();

    /**
     * Get the fixture instance's status info for Fixture Manager
     *
     * @return A sort-of HTML-RTF-gibberish for Fixture Manager
     */
    QString fixtureInfo(const Fixture *fixture) const;

    /** Get a CSS style sheet & HTML header for channels group info */
    QString channelsGroupInfoStyleSheetHeader();

    /**
     * Get the channels group instance's status info for Fixture Manager
     *
     * @return A sort-of HTML-RTF-gibberish for Fixture Manager
     */
    QString channelsGroupInfo(const ChannelsGroup *channelsGroup) const;

private:
    /** Universe the fixture-list context menu was opened in, so "Add fixture"
     *  defaults to it. invalidUniverse() = no context (toolbar / empty space). */
    quint32 m_ctxUniverse;

    /** Patching a universe row's output: the Connections tab's questions
     *  and rules (shared, not copied). */
    class UniversePatcher *m_patcher = nullptr;
    /** Selected universe rows in the order they were clicked. */
    QList<quint32> m_universePickOrder;

    QSplitter* m_splitter;
    FixtureTreeWidget* m_fixtures_tree;
    QLineEdit* m_fixturesSearch;
    QTreeWidget* m_channel_groups_tree;
    QWidget* m_rdmManager;

    QTextBrowser* m_info;
    FixtureGroupEditor* m_groupEditor;
    quint32 m_groupEditorId; //!< id of the group m_groupEditor is editing
    int m_currentTabIndex;
    PowerDistributionWidget* m_power; //!< rightmost pane: circuits + live load
    UniverseUsageWidget* m_universeUsage; //!< rightmost pane: per-universe address grid

    /********************************************************************
     * Menu & Toolbar & Actions
     ********************************************************************/
private:
    /** Construct actions for toolbar & context menu */
    void initActions();

    /** Update the contents of the group menu */
    void updateGroupMenu();

    /** Construct the toolbar */
    void initToolBar();

    /** Edit properties for the selected fixture */
    void editFixtureProperties();

public:
    /** A fixture's definition and mode (and name, connection, address) in the
     *  stock dialog -- reached from Fixture Properties' "Definition & Mode…"
     *  and the Fixture Manager's menu. True when something changed. */
    static bool editDefinitionAndMode(QWidget *parent, Doc *doc, quint32 fid);

    /** Definitions that can stand in for the one @p fids share (same
     *  manufacturer, another model): every mode they use is there by name,
     *  with the same channels in the same order -- only the heads or the
     *  details may differ ("LED Movinghead+Circle (2 heads)" for "LED
     *  Movinghead+Circle"). Empty when the fixtures' definitions differ. */
    static QList<QLCFixtureDef *> dropInDefinitions(Doc *doc, const QList<quint32> &fids);
    /** Put @p fids on @p def, each in its mode of the same name, keeping
     *  address, name and groups. Returns how many changed. */
    static int changeDefinition(Doc *doc, const QList<quint32> &fids, QLCFixtureDef *def);

private:

    /** Edit properties for the selected channels group */
    void editChannelGroupProperties();

    QString createDialog(bool import);

private slots:
    void slotAddRGBPanel();
    void slotRemove();
    void slotProperties();
    void slotTestFixture();
    void slotLocateFixture();
    void slotResetFixture();
    void slotFadeConfig();
    void slotRemap();
    void slotPatchList();
    void slotUnGroup();
    void slotGroupSelected(QAction* action);
    void slotMoveGroupUp();
    void slotMoveGroupDown();
    void slotImport();
    void slotExport();

    /** Callback for right mouse button clicks over a fixture item */
    void slotContextMenuRequested(const QPoint& pos);

    /** Callback for right mouse button clicks over a channel group item */
    void slotChannelGroupContextMenuRequested(const QPoint& pos);

    /** Move the dropped fixture groups into folder $destPath ("" = root). */
    void slotGroupsDroppedOnFolder(const QList<quint32>& groupIds, const QString& destPath);

    /** Rename a fixture-group folder and update all groups whose path starts
     *  with @p oldPath. */
    void slotGroupFolderRenamed(const QString& oldPath, const QString& newLeaf);

    /** Assign the dropped fixtures to a power circuit (dragged onto it in the
     *  Power section of the tree). */
    void slotFixturesDroppedOnCircuit(const QList<quint32>& fixtureIds, int sourceIdx, int circuitIdx);
    void slotFixturesDroppedOnUniverse(const QList<quint32>& fixtureIds, quint32 universe);

private:
    QAction* m_addAction;
    QAction* m_addInventoryAction;
    QAction* m_patchInventoryAction;
    QAction* m_addRGBAction;
    /** Toolbar-only container action for the "Add" dropdown button (Add
     *  Fixture.../Add RGB Panel...) — toggled visible/hidden per active tab
     *  in slotTabChanged(), separately from m_addAction/m_addRGBAction
     *  themselves, which stay usable from the fixture context menu on
     *  either state. */
    QAction* m_addFixtureButtonAction;
    /** "Add Channel Group..." — the Channel Groups tab's own creation
     *  action, split out from m_addAction so its toolbar label is never
     *  wrong (previously m_addAction did double duty, dispatching by tab in
     *  slotAdd() with only a tooltip hinting at the swap). */
    QAction* m_addChannelsGroupAction;
    QAction* m_removeAction;
    QAction* m_propertiesAction;
    QAction* m_testAction;
    /** Flash-to-identify and send-reset, reachable directly from the context
     *  menu instead of only via the "Test Fixture..." dialog's own buttons —
     *  same single-fixture gating as m_testAction (see slotModeChanged()). */
    QAction* m_locateAction;
    QAction* m_resetAction;
    QAction* m_fadeConfigAction;
    QAction* m_remapAction;
    QAction* m_patchListAction;
    QAction* m_groupAction;
    QAction* m_unGroupAction;
    QAction* m_newGroupAction;

    QAction* m_moveUpAction;
    QAction* m_moveDownAction;

    QAction* m_importAction;
    QAction* m_exportAction;
    QAction* m_expandAllAction;
    QAction* m_collapseAllAction;
    QMenu* m_groupMenu;
};

/** @} */

#endif

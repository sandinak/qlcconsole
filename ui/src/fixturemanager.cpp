/*
  Q Light Controller
  fixturemanager.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QTreeWidgetItem>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QScrollArea>
#include <QMessageBox>
#include <QToolButton>
#include <QFileDialog>
#include <QTabWidget>
#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QString>
#include <QSet>
#include <QSettings>
#include <QRegularExpression>
#include <QDebug>
#include <QIcon>
#include <QMenu>
#include <QInputDialog>
#include <QLineEdit>
#include <QTimer>
#include <QtGui>
#include <climits>

#include "patchlistdialog.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcchannel.h"
#include "qlcfile.h"

#include "createfixturegroup.h"
#include "powerdistributionwidget.h"
#include "powerdistribution.h"
#include "universeusagewidget.h"
#include "fixturegroupeditor.h"
#include "fixturetreewidget.h"
#include "channelsselection.h"
#include "addchannelsgroup.h"
#include <functional>
#include "fixturemanager.h"
#include "measurementunits.h"
#include <QShortcut>
#include "fixtureremap.h"
#include "addrgbpanel.h"
#include "addfixture.h"
#include "patchinventorydialog.h"
#include "fixturecreator.h"
#include "rdmmanager.h"
#include "universe.h"
#include "patchundo.h"
#include "fixture.h"
#include "apputil.h"
#include "doc.h"
#include "programmercontroller.h"
#include "monitor/monitor.h"
#include "universepatcher.h"
#include "outputpatch.h"
#include "qlcioplugin.h"
#include "patchundo.h"
#include "showstatus.h"

// Bumped when the power pane was added as a third section: an older 2-section
// saved state would collapse the new pane to zero width and hide it.
#define SETTINGS_SPLITTER "fixturemanager/splitterstate_pwr"

// List view column numbers
#define KColumnName     0
#define KColumnChannels 1
#define KColumnAddress  2

FixtureManager* FixtureManager::s_instance = NULL;

/*****************************************************************************
 * Initialization
 *****************************************************************************/

FixtureManager::FixtureManager(QWidget* parent, Doc* doc)
    : QWidget(parent)
    , m_toolbar(NULL)
    , m_doc(doc)
    , m_ctxUniverse(InputOutputMap::invalidUniverse())
    , m_splitter(NULL)
    , m_fixtures_tree(NULL)
    , m_fixturesSearch(NULL)
    , m_channel_groups_tree(NULL)
    , m_rdmManager(NULL)
    , m_info(NULL)
    , m_groupEditor(NULL)
    , m_groupEditorId(FixtureGroup::invalidId())
    , m_currentTabIndex(0)
    , m_power(NULL)
    , m_universeUsage(NULL)
    , m_addAction(NULL)
    , m_addInventoryAction(NULL)
    , m_patchInventoryAction(NULL)
    , m_addRGBAction(NULL)
    , m_addFixtureButtonAction(NULL)
    , m_addChannelsGroupAction(NULL)
    , m_removeAction(NULL)
    , m_propertiesAction(NULL)
    , m_testAction(NULL)
    , m_locateAction(NULL)
    , m_resetAction(NULL)
    , m_fadeConfigAction(NULL)
    , m_remapAction(NULL)
    , m_patchListAction(NULL)
    , m_groupAction(NULL)
    , m_unGroupAction(NULL)
    , m_newGroupAction(NULL)
    , m_moveUpAction(NULL)
    , m_moveDownAction(NULL)
    , m_importAction(NULL)
    , m_exportAction(NULL)
    , m_expandAllAction(NULL)
    , m_collapseAllAction(NULL)
    , m_groupMenu(NULL)
{
    Q_ASSERT(s_instance == NULL);
    s_instance = this;

    Q_ASSERT(doc != NULL);

    /* ⌘Z here undoes the rig's last change -- a patch included (the Patch
       dialog records one undo step for what it applied). */
    QShortcut *undo = new QShortcut(QKeySequence::Undo, this);
    undo->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undo, &QShortcut::activated, this, []() {
        if (Monitor *mon = Monitor::instance())
            mon->undo();
    });

    new QVBoxLayout(this);
    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->setSpacing(0);

    initActions();
    initToolBar();
    initDataView();
    updateView();
    updateChannelsGroupView();

    QTreeWidgetItem* grpItem = m_fixtures_tree->topLevelItem(0);
    if (grpItem != NULL)
        grpItem->setExpanded(true);

    /* Connect fixture list change signals from the new document object */
    // A fixture renamed elsewhere (the Studio, Layers) shows here too.
    connect(m_doc, &Doc::fixtureChanged, this, [this](quint32 id) {
        if (m_fixtures_tree != NULL)
            m_fixtures_tree->refreshFixtureName(id);
    });
    connect(m_doc, SIGNAL(fixtureRemoved(quint32)),
            this, SLOT(slotFixtureRemoved(quint32)));

    /* Patching a universe row's output (its context menu) asks what the
       Connections tab asks, through the same code. A patch changed -- here,
       or put back by Undo Patch Change in Connections -- relabels the
       universe rows in place: a rebuild would drop the selection. PatchUndo
       says "changed" before a change as well as after an undo, hence the
       zero-delay hop to read the patch once it has landed. */
    m_patcher = new UniversePatcher(m_doc, this);
    connect(m_patcher, &UniversePatcher::patchChanged, this, [this]() {
        if (m_fixtures_tree != NULL)
            m_fixtures_tree->updateUniverseLabels();
    });
    if (m_doc->inputOutputMap() != NULL && m_doc->inputOutputMap()->patchUndo() != NULL)
        connect(m_doc->inputOutputMap()->patchUndo(), &PatchUndo::changed, this, [this]() {
            QTimer::singleShot(0, this, [this]() {
                if (m_fixtures_tree != NULL)
                    m_fixtures_tree->updateUniverseLabels();
            });
        });
    connect(m_fixtures_tree, &QTreeWidget::itemSelectionChanged,
            this, &FixtureManager::syncUniversePickOrder);

    connect(m_doc, SIGNAL(channelsGroupRemoved(quint32)),
            this, SLOT(slotChannelsGroupRemoved(quint32)));

    connect(m_doc, SIGNAL(modeChanged(Doc::Mode)),
            this, SLOT(slotModeChanged(Doc::Mode)));

    connect(m_doc, SIGNAL(fixtureGroupRemoved(quint32)),
            this, SLOT(slotFixtureGroupRemoved(quint32)));

    connect(m_doc, SIGNAL(fixtureGroupChanged(quint32)),
            this, SLOT(slotFixtureGroupChanged(quint32)));

    // A group ADDED elsewhere (e.g. created in the Lighting Studio) must appear
    // here too — the tree previously only refreshed on remove/change.
    connect(m_doc, &Doc::fixtureGroupAdded, this, [this](quint32) {
        // ...but not once per group while a workspace is loading. Each rebuild
        // walks every group and fixture, so doing one per added group is
        // O(groups x tree) and dominated the parse (1213 of 1801 samples).
        // slotDocLoaded() does the single rebuild they all collapse into.
        if (m_doc->loadStatus() == Doc::Loading)
            return;
        m_fixtures_tree->updateTree();
        updateGroupMenu();
    });

    connect(m_doc, SIGNAL(loaded()),
            this, SLOT(slotDocLoaded()));

    slotModeChanged(m_doc->mode());

    QSettings settings;
    QVariant var = settings.value(SETTINGS_SPLITTER);
    if (var.isValid() == true)
        m_splitter->restoreState(var.toByteArray());
    else
        m_splitter->setSizes(QList <int> () << int(this->width() / 2) << int(this->width() / 2));
}

FixtureManager::~FixtureManager()
{
    QSettings settings;
    settings.setValue(SETTINGS_SPLITTER, m_splitter->saveState());
    FixtureManager::s_instance = NULL;

    s_instance = NULL;
}

FixtureManager* FixtureManager::instance()
{
    return s_instance;
}

/*****************************************************************************
 * Doc signal handlers
 *****************************************************************************/

void FixtureManager::slotFixtureRemoved(quint32 id)
{
    /* Find and drop the fixture's own row wherever it actually lives. It
       used to be a direct child of a top-level folder -- true for a
       fixture group, still -- but a universe's fixtures now sit one level
       further down (Universes -> Universe N -> fixture, see updateTree()'s
       "always present" universesRoot), and this only ever looked at direct
       children. Doc::deleteFixture() had already succeeded by the time this
       runs; the row was simply never found, so it sat there looking exactly
       like the delete had silently done nothing. QTreeWidgetItemIterator
       finds it regardless of how deep the tree's own grouping goes. */
    /* Every row, not just the first: a fixture can be listed in more than one
       place (a fixture group AND the Inventory, say). Collected first --
       deleting while iterating would pull the tree out from under the
       iterator. */
    QList<QTreeWidgetItem *> rows;
    for (QTreeWidgetItemIterator treeIt(m_fixtures_tree); *treeIt != NULL; ++treeIt)
    {
        QVariant fxiVar = (*treeIt)->data(KColumnName, PROP_ID);
        if (fxiVar.isValid() == true && fxiVar.toUInt() == id
            && !(*treeIt)->data(KColumnName, PROP_HEAD).isValid())
            rows << *treeIt;
    }
    qDeleteAll(rows);

    // The Inventory's folders carry fixture counts and vanish when emptied.
    m_fixtures_tree->refreshInventoryCounts();

    /* A top-level folder that just lost its last child: a fixture group
       with nothing left in it. A universe folder is pre-seeded for every
       universe regardless of fixture count (see updateTree()), so this is
       naturally a no-op for those -- its own children are per-universe
       folders, not fixtures, and removing one fixture two levels down
       never empties it. */
    QList<QTreeWidgetItem*> groupsToDelete;
    for (int i = 0; i < m_fixtures_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* grpItem = m_fixtures_tree->topLevelItem(i);
        Q_ASSERT(grpItem != NULL);
        // The Inventory and Power roots stay even when empty: they are where
        // their Add actions live.
        if (grpItem->data(KColumnName, PROP_INVENTORY).isValid()
            || grpItem->data(KColumnName, PROP_SOURCE).isValid())
            continue;
        if (grpItem->childCount() == 0)
            groupsToDelete << grpItem;
    }
    foreach (QTreeWidgetItem* groupToDelete, groupsToDelete)
    {
        QVariant var = groupToDelete->data(KColumnName, PROP_GROUP);
        // If the group is a fixture group, delete it from doc.
        // If not, it is a universe, just "hide" it from the ui.
        if (var.isValid() == true)
            m_doc->deleteFixtureGroup(groupToDelete->data(KColumnName, PROP_GROUP).toUInt());
        else
            delete groupToDelete;
    }
}

void FixtureManager::slotChannelsGroupRemoved(quint32 id)
{
    qDebug() << "Channel group removed: " << id;
    for (int i = 0; i < m_channel_groups_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* grpItem = m_channel_groups_tree->topLevelItem(i);
        Q_ASSERT(grpItem != NULL);
        QVariant var = grpItem->data(KColumnName, PROP_ID);
        if (var.isValid() == true && var.toUInt() == id)
            delete grpItem;
    }
}

void FixtureManager::slotModeChanged(Doc::Mode mode)
{
    // Split out from m_addAction (see its declaration); mirrors the same
    // Design-only gating m_addAction gets below, unconditionally in every
    // Design sub-branch and unconditionally false in the Operate branch.
    m_addChannelsGroupAction->setEnabled(mode == Doc::Design);

    if (mode == Doc::Design)
    {
        int selected = m_fixtures_tree->selectedItems().size();

        QTreeWidgetItem* item = m_fixtures_tree->currentItem();
        if (item == NULL)
        {
            m_addAction->setEnabled(true);
            m_addRGBAction->setEnabled(true);
            m_removeAction->setEnabled(false);
            m_propertiesAction->setEnabled(false);
            m_groupAction->setEnabled(false);
            m_unGroupAction->setEnabled(false);
            m_importAction->setEnabled(true);
        }
        else if (item->data(KColumnName, PROP_ID).isValid() == true)
        {
            // Fixture selected
            m_addAction->setEnabled(true);
            m_addRGBAction->setEnabled(true);
            m_removeAction->setEnabled(true);
            if (selected == 1)
            {
                m_propertiesAction->setEnabled(true);
                m_testAction->setEnabled(true);
                m_resetAction->setEnabled(true);
            }
            else
            {
                m_propertiesAction->setEnabled(false);
                m_testAction->setEnabled(false);
                m_resetAction->setEnabled(false);
            }
            /* Locate is not single-selection like the others. Properties and
               Reset act on one fixture because they have nowhere to put a
               second answer; Locate just flashes, and "which of these twelve
               are the front truss" is the question it exists to answer. Works
               on a multi-selection and on a whole group. */
            m_locateAction->setEnabled(true);
            m_groupAction->setEnabled(true);

            // Don't allow ungrouping from the "All fixtures" group
            if (item->parent()->data(KColumnName, PROP_GROUP).isValid() == true)
                m_unGroupAction->setEnabled(true);
            else
                m_unGroupAction->setEnabled(false);
        }
        else if (item->data(KColumnName, PROP_GROUP).isValid() == true)
        {
            // Fixture group selected
            m_addAction->setEnabled(true);
            m_addRGBAction->setEnabled(true);
            m_removeAction->setEnabled(true);
            m_propertiesAction->setEnabled(false);
            m_groupAction->setEnabled(false);
            m_unGroupAction->setEnabled(false);
        }
        else
        {
            // All fixtures selected
            m_addAction->setEnabled(true);
            m_addRGBAction->setEnabled(true);
            m_removeAction->setEnabled(false);
            m_propertiesAction->setEnabled(false);
            m_groupAction->setEnabled(false);
            m_unGroupAction->setEnabled(false);
        }
        if (m_doc->fixtures().count() > 0)
            m_fadeConfigAction->setEnabled(true);
        else
            m_fadeConfigAction->setEnabled(false);

        // m_groupAction ("Add fixture to group...") is left at whichever
        // per-branch state was set above -- true only when an actual fixture
        // is selected. It used to be force-enabled unconditionally here
        // because its dropdown's "New Group..." entry needed to work even
        // with nothing selected; that's now a right-click on the Fixture
        // Groups root/a group folder instead (slotContextMenuRequested()),
        // so this action's only remaining job -- assigning a selection to an
        // existing group -- genuinely needs a fixture selected.
    }
    else
    {
        m_addAction->setEnabled(false);
        m_addRGBAction->setEnabled(false);
        m_removeAction->setEnabled(false);
        m_propertiesAction->setEnabled(false);
        m_fadeConfigAction->setEnabled(false);
        m_groupAction->setEnabled(false);
        m_unGroupAction->setEnabled(false);
        // Allow testing a fixture in Operate mode too
        QTreeWidgetItem* item = m_fixtures_tree->currentItem();
        bool singleFixture = (item && item->data(KColumnName, PROP_ID).isValid() &&
                              m_fixtures_tree->selectedItems().size() == 1);
        m_testAction->setEnabled(singleFixture);
        m_resetAction->setEnabled(singleFixture);
        // Locate works on any selection, including in Operate mode.
        m_locateAction->setEnabled(m_fixtures_tree->selectedItems().isEmpty() == false);
    }
}

void FixtureManager::slotFixtureGroupRemoved(quint32 id)
{
    // If the open group editor was editing the removed group, close it — its
    // FixtureGroup* is now dangling and any further interaction (Ctrl+Z, drag,
    // context menu, spin) would dereference freed memory.
    if (m_groupEditor != NULL && m_groupEditorId == id)
    {
        delete m_groupEditor;
        m_groupEditor = NULL;
        m_groupEditorId = FixtureGroup::invalidId();
    }

    for (int i = 0; i < m_fixtures_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* item = m_fixtures_tree->topLevelItem(i);
        Q_ASSERT(item != NULL);
        QVariant var = item->data(KColumnName, PROP_GROUP);
        if (var.isValid() && var.toUInt() == id)
        {
            delete item;
            break;
        }
    }

    updateGroupMenu();
}

void FixtureManager::slotFixtureGroupChanged(quint32 id)
{
    QTreeWidgetItem* item = m_fixtures_tree->groupItem(id);
    if (item == NULL)
        return;

    FixtureGroup* grp = m_doc->fixtureGroup(id);
    Q_ASSERT(grp != NULL);
    m_fixtures_tree->updateGroupItem(item, grp);
    updateGroupMenu();
}

void FixtureManager::slotDocLoaded()
{
    // The per-group rebuilds suppressed during the load (see the
    // fixtureGroupAdded connection above) collapse into this one.
    m_fixtures_tree->updateTree();
    updateGroupMenu();

    slotTabChanged(m_currentTabIndex);
}

/*****************************************************************************
 * Data view
 *****************************************************************************/

void FixtureManager::initDataView()
{
    // Create a splitter to divide list view and text view
    m_splitter = new QSplitter(Qt::Horizontal, this);
    layout()->addWidget(m_splitter);
    m_splitter->setSizePolicy(QSizePolicy::Expanding,
                              QSizePolicy::Expanding);

    QTabWidget *tabs = new QTabWidget(this);
    m_splitter->addWidget(tabs);

    /* Create a tree widget to the left part of the splitter */
    quint32 treeFlags = FixtureTreeWidget::UniverseNumber |
                        FixtureTreeWidget::AddressRange |
                        FixtureTreeWidget::ShowGroups |
                        FixtureTreeWidget::ShowPower;

    m_fixtures_tree = new FixtureTreeWidget(m_doc, treeFlags, this);
    m_fixtures_tree->setIconSize(QSize(32, 32));
    m_fixtures_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_fixtures_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fixtures_tree->sortByColumn(KColumnAddress, Qt::AscendingOrder);
    // Allow dragging fixtures out onto the group editor's layout grid, and
    // dragging fixture groups onto folders within the tree.
    m_fixtures_tree->setDragEnabled(true);
    m_fixtures_tree->setDragDropMode(QAbstractItemView::DragDrop);
    m_fixtures_tree->setDropIndicatorShown(true);
    // CopyAction (not Move): the view must not auto-remove the dragged source
    // rows — we handle the "move" ourselves (group path change / fixture add).
    m_fixtures_tree->setDefaultDropAction(Qt::CopyAction);

    connect(m_fixtures_tree, SIGNAL(groupsDroppedOnFolder(QList<quint32>,QString)),
            this, SLOT(slotGroupsDroppedOnFolder(QList<quint32>,QString)));
    connect(m_fixtures_tree, SIGNAL(groupFolderRenamed(QString,QString)),
            this, SLOT(slotGroupFolderRenamed(QString,QString)));
    connect(m_fixtures_tree, SIGNAL(fixturesDroppedOnCircuit(QList<quint32>,int,int)),
            this, SLOT(slotFixturesDroppedOnCircuit(QList<quint32>,int,int)));
    connect(m_fixtures_tree, SIGNAL(fixturesDroppedOnUniverse(QList<quint32>,quint32)),
            this, SLOT(slotFixturesDroppedOnUniverse(QList<quint32>,quint32)));

    connect(m_fixtures_tree, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotSelectionChanged()));

    connect(m_fixtures_tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotDoubleClicked(QTreeWidgetItem*)));

    connect(m_fixtures_tree, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(slotContextMenuRequested(const QPoint&)));

    connect(m_fixtures_tree, SIGNAL(expanded(QModelIndex)),
            this, SLOT(slotFixtureItemExpanded()));

    connect(m_fixtures_tree, SIGNAL(collapsed(QModelIndex)),
            this, SLOT(slotFixtureItemExpanded()));

    // Search box above the tree — filters by row name as you type, hiding
    // everything else (but keeping a folder/group visible whenever something
    // inside it still matches).
    QWidget *fixturesPane = new QWidget(this);
    QVBoxLayout *fixturesPaneLayout = new QVBoxLayout(fixturesPane);
    fixturesPaneLayout->setContentsMargins(0, 0, 0, 0);
    fixturesPaneLayout->setSpacing(0);
    m_fixturesSearch = new QLineEdit(fixturesPane);
    m_fixturesSearch->setPlaceholderText(tr("Search fixtures..."));
    m_fixturesSearch->setClearButtonEnabled(true);
    connect(m_fixturesSearch, &QLineEdit::textChanged,
            this, &FixtureManager::slotFixturesSearchChanged);
    fixturesPaneLayout->addWidget(m_fixturesSearch);
    fixturesPaneLayout->addWidget(m_fixtures_tree, 1);

    // Labeled "Fixtures" (not "Fixture Groups") -- this tree holds the whole
    // patch (fixtures, spatial groups, Power, Universes), not just groups;
    // the old label undersold what's actually in it.
    tabs->setTabToolTip(tabs->addTab(fixturesPane, tr("Fixtures")), tr(
        "The full patch: fixtures, universes, power circuits, and spatial "
        "groups (fixtures arranged on a head-layout grid, used for XY-pad / "
        "per-head effects and as drag-drop dynamic targets in the "
        "Programming tab). Not the same as Channel Groups (next tab)."));

    m_channel_groups_tree = new QTreeWidget(this);
    QStringList chan_labels;
    chan_labels << tr("Name") << tr("Channels");
    m_channel_groups_tree->setHeaderLabels(chan_labels);
    m_channel_groups_tree->setRootIsDecorated(false);
    m_channel_groups_tree->setAllColumnsShowFocus(true);
    m_channel_groups_tree->setIconSize(QSize(32, 32));
    m_channel_groups_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_channel_groups_tree->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_channel_groups_tree, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotChannelsGroupSelectionChanged()));
    connect(m_channel_groups_tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotChannelsGroupDoubleClicked(QTreeWidgetItem*)));
    connect(m_channel_groups_tree, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(slotChannelGroupContextMenuRequested(const QPoint&)));

    tabs->setTabToolTip(tabs->addTab(m_channel_groups_tree, tr("Channel Groups")), tr(
        "DMX-channel groups for Simple Desk (e.g. all the Gobo channels across "
        "several fixtures, grouped for quick access). Not the same as Fixture "
        "Groups (previous tab), which group whole fixtures spatially."));
/*
    m_rdmManager = new RDMManager(this, m_doc);
    tabs->addTab(m_rdmManager, "RDM");
    connect(m_rdmManager, SIGNAL(fixtureInfoReady(QString&)),
            this, SLOT(slotDisplayFixtureInfo(QString&)));
*/
    connect(tabs, SIGNAL(currentChanged(int)), this, SLOT(slotTabChanged(int)));

    /* Create the initial right-hand pane. slotSelectionChanged() then swaps this
     * slot between the info browser, a fixture-group's layout editor, and the
     * power-distribution view depending on what's selected. */
    createInfo();

    slotSelectionChanged();
}

void FixtureManager::updateView()
{
    // Record which top level items are open
    QList <QVariant> openGroups;
    for (int i = 0; i < m_fixtures_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* item = m_fixtures_tree->topLevelItem(i);
        if (item->isExpanded() == true)
            openGroups << item->data(KColumnName, PROP_GROUP);
    }

    if (m_doc->fixtures().count() > 0)
    {
        m_exportAction->setEnabled(true);
        m_remapAction->setEnabled(true);
        m_fadeConfigAction->setEnabled(true);
    }
    else
    {
        m_exportAction->setEnabled(false);
        m_fadeConfigAction->setEnabled(false);
        m_remapAction->setEnabled(false);
    }
    m_addRGBAction->setEnabled(true);
    m_importAction->setEnabled(true);
    m_moveUpAction->setEnabled(false);
    m_moveDownAction->setEnabled(false);

    m_fixtures_tree->updateTree();

    // Reopen groups that were open before update
    for (int i = 0; i < m_fixtures_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* item = m_fixtures_tree->topLevelItem(i);
        QVariant var = item->data(KColumnName, PROP_GROUP);
        if (openGroups.contains(var) == true)
        {
            item->setExpanded(true);
            openGroups.removeAll(var);
        }
    }

    updateGroupMenu();
    slotModeChanged(m_doc->mode());

    m_fixtures_tree->header()->resizeSections(QHeaderView::ResizeToContents);
}

void FixtureManager::updateChannelsGroupView()
{
    quint32 selGroupID = ChannelsGroup::invalidId();

    if (m_channel_groups_tree->selectedItems().size() > 0)
    {
        QTreeWidgetItem *item = m_channel_groups_tree->selectedItems().first();
        selGroupID = item->data(KColumnName, PROP_ID).toUInt();
    }

    if (m_channel_groups_tree->topLevelItemCount() > 0)
        for (int i = m_channel_groups_tree->topLevelItemCount() - 1; i >= 0; i--)
            m_channel_groups_tree->takeTopLevelItem(i);

    foreach (ChannelsGroup *grp, m_doc->channelsGroups())
    {
        QTreeWidgetItem *grpItem = new QTreeWidgetItem(m_channel_groups_tree);
        grpItem->setText(KColumnName, grp->name());
        grpItem->setData(KColumnName, PROP_ID, grp->id());
        grpItem->setText(KColumnChannels, QString("%1").arg(grp->getChannels().count()));
        if (grp->getChannels().count() > 0)
        {
            SceneValue scv = grp->getChannels().at(0);
            Fixture *fxi = m_doc->fixture(scv.fxi);
            if (fxi == NULL)
                continue;

            const QLCChannel *ch = fxi->channel(scv.channel);
            if (ch != NULL)
                grpItem->setIcon(KColumnName, ch->getIcon());
        }
        if (selGroupID == grp->id())
            grpItem->setSelected(true);
    }
    m_addRGBAction->setEnabled(false);
    m_propertiesAction->setEnabled(false);
    m_groupAction->setEnabled(false);
    m_unGroupAction->setEnabled(false);
    m_fadeConfigAction->setEnabled(false);
    m_exportAction->setEnabled(false);
    m_importAction->setEnabled(false);
    m_remapAction->setEnabled(false);

    m_channel_groups_tree->header()->resizeSections(QHeaderView::ResizeToContents);
}

void FixtureManager::updateRDMView()
{
    m_addRGBAction->setEnabled(false);
    m_propertiesAction->setEnabled(false);
    m_groupAction->setEnabled(false);
    m_unGroupAction->setEnabled(false);
    m_fadeConfigAction->setEnabled(false);
    m_exportAction->setEnabled(false);
    m_importAction->setEnabled(false);
    m_remapAction->setEnabled(false);
}

void FixtureManager::fixtureSelected(quint32 id)
{
    Fixture* fxi = m_doc->fixture(id);
    if (fxi == NULL)
        return;

    if (m_info == NULL)
        createInfo();

    m_info->setText(QString("%1<BODY>%2</BODY></HTML>")
                    .arg(fixtureInfoStyleSheetHeader())
                    .arg(fixtureInfo(fxi)));

    // Enable/disable actions
    slotModeChanged(m_doc->mode());
}

void FixtureManager::clearRightPane()
{
    // The right pane (splitter index 1) hosts exactly one of these at a time;
    // delete whichever is present so a new occupant can take the slot.
    if (m_info != NULL)
    {
        delete m_info;
        m_info = NULL;
    }
    if (m_groupEditor != NULL)
    {
        delete m_groupEditor;
        m_groupEditor = NULL;
        m_groupEditorId = FixtureGroup::invalidId();
        // Only relevant while a grid that can consume per-head drops is open
        // — see FixtureTreeWidget::setShowHeads()'s own comment for why this
        // isn't just always on.
        m_fixtures_tree->setShowHeads(false);
    }
    if (m_power != NULL)
    {
        delete m_power;
        m_power = NULL;
    }
    if (m_universeUsage != NULL)
    {
        delete m_universeUsage;
        m_universeUsage = NULL;
    }
}

void FixtureManager::fixtureGroupSelected(FixtureGroup* grp)
{
    if (m_groupEditor != NULL && m_groupEditorId == grp->id())
    {
        /* Already editing it -- but make sure it can be SEEN. Returning early
           with a collapsed right pane means clicking the group again does
           nothing at all, forever: the editor exists, so this path is taken,
           and nothing ever widens the pane back. */
        ensureRightPaneVisible();
        return;
    }

    QByteArray state = m_splitter->saveState();
    clearRightPane();

    m_groupEditor = new FixtureGroupEditor(grp, m_doc, this);
    m_groupEditorId = grp->id();
    m_splitter->insertWidget(1, m_groupEditor);
    // The editor's grid accepts per-head drops (see FixtureGroupEditor::
    // eventFilter()) — show each fixture's heads as draggable children in
    // the source tree while it's open, so a specific head (not just a whole
    // fixture) can actually be dragged onto a cell.
    m_fixtures_tree->setShowHeads(true);

    m_splitter->restoreState(state);
    ensureRightPaneVisible();
}

/* A right pane restored to zero width is the same thing as no right pane.
 *
   fixtureGroupSelected() (and the other pane-swappers) save the splitter state
   BEFORE the new widget exists and restore it after inserting, so the sizes
   being restored describe a different set of widgets. Where the pane was
   collapsed -- by a drag, or by a state persisted from an earlier session --
   the editor comes back 0 px wide: built, correct, and invisible, which reads
   exactly like clicking the group doing nothing at all.
 *
   Only ever widens, and only from collapsed, so a pane the operator has
   deliberately made narrow is left alone. */
void FixtureManager::ensureRightPaneVisible()
{
    if (m_splitter == NULL || m_splitter->count() < 2)
        return;

    QList<int> sizes = m_splitter->sizes();
    if (sizes.size() < 2)
        return;

    const int total = sizes.at(0) + sizes.at(1);
    if (total <= 0)
        return;                       // not laid out yet; nothing to fix
    if (sizes.at(1) > 24)
        return;                       // already visible

    /* Leave the tree something, but scale that reserve to what there IS: a
       fixed 120 px floor goes NEGATIVE on a narrow splitter, and the guard then
       rejected its own result and silently did nothing -- which is how this
       first looked like the guard not running at all. */
    const int minTree = qMin(120, total / 2);
    int right = qMax(240, total / 3);
    right = qMin(right, total - minTree);
    if (right <= 0)
        return;

    sizes[1] = right;
    sizes[0] = total - right;
    m_splitter->setSizes(sizes);
}

void FixtureManager::showPower()
{
    if (m_power != NULL)
    {
        m_power->refresh();
        return; // already showing the power view
    }

    QByteArray state = m_splitter->saveState();
    clearRightPane();

    // Embedded next to the fixture tree, so no built-in fixtures list: assignment
    // is driven from the tree's right-click menu (or dragging onto a circuit).
    m_power = new PowerDistributionWidget(m_doc, false, this);
    m_splitter->insertWidget(1, m_power);

    m_splitter->restoreState(state);
}

void FixtureManager::showUniverseUsage(quint32 universe)
{
    QByteArray state = m_splitter->saveState();
    clearRightPane();

    m_universeUsage = new UniverseUsageWidget(m_doc, universe, this);
    m_splitter->insertWidget(1, m_universeUsage);

    m_splitter->restoreState(state);
}

void FixtureManager::createInfo()
{
    QByteArray state = m_splitter->saveState();
    clearRightPane();

    m_info = new QTextBrowser(this);
    m_splitter->insertWidget(1, m_info);

    m_splitter->restoreState(state);
}

void FixtureManager::slotSelectionChanged()
{
    const QList<QTreeWidgetItem*> items = m_fixtures_tree->selectedItems();
    const int selectedCount = items.size();

    // A single fixture group → its layout editor.
    if (selectedCount == 1)
    {
        const QVariant grpvar = items.first()->data(KColumnName, PROP_GROUP);
        if (grpvar.isValid() == true)
        {
            FixtureGroup *grp = m_doc->fixtureGroup(grpvar.toUInt());
            if (grp != NULL)
                fixtureGroupSelected(grp);
            slotModeChanged(m_doc->mode());
            return;
        }
    }

    // While a group's layout editor is open, keep it as long as the selection is
    // only fixtures — so a fixture can be dragged from the tree onto the layout
    // grid without the editor being torn down mid-drag. A universe row or empty
    // selection still switches away (below).
    if (m_groupEditor != NULL && selectedCount >= 1)
    {
        bool onlyFixtures = true;
        foreach (QTreeWidgetItem *it, items)
        {
            const bool isFixture = it->data(KColumnName, PROP_ID).isValid()
                    && it->data(KColumnName, PROP_HEAD).isValid() == false;
            if (isFixture == false) { onlyFixtures = false; break; }
        }
        if (onlyFixtures)
        {
            slotModeChanged(m_doc->mode());
            return;
        }
    }

    // Empty rig → onboarding hint in the info browser.
    if (selectedCount == 0 && m_fixtures_tree->topLevelItemCount() <= 0)
    {
        if (m_info == NULL)
            createInfo();
        m_info->setText(tr("<HTML><BODY><H1>No fixtures</H1>"
                           "<P>Click <IMG SRC=\"" ":/edit_add.png\">"
                           " to add fixtures.</P></BODY></HTML>"));
        slotModeChanged(m_doc->mode());
        return;
    }

    // A single universe → its address-usage grid.
    if (selectedCount == 1)
    {
        const QVariant univar = items.first()->data(KColumnName, PROP_UNIVERSE);
        if (univar.isValid() == true)
        {
            showUniverseUsage(univar.toUInt());
            slotModeChanged(m_doc->mode());
            return;
        }
    }

    // A power-tree node selected (the Power folder itself, a source, or a
    // circuit) → the power-distribution view. A plain fixture/group selection
    // never lands here — only an explicit selection under Power.
    bool anyPower = false;
    foreach (QTreeWidgetItem *it, items)
    {
        if (it->data(KColumnName, PROP_SOURCE).isValid() ||
            it->data(KColumnName, PROP_CIRCUIT).isValid())
        {
            anyPower = true;
            break;
        }
    }
    if (anyPower)
    {
        showPower();
        slotModeChanged(m_doc->mode());
        return;
    }

    // A single fixture → its info.
    if (selectedCount == 1)
    {
        const QVariant fxivar = items.first()->data(KColumnName, PROP_ID);
        if (fxivar.isValid() == true && items.first()->data(KColumnName, PROP_HEAD).isValid() == false)
        {
            fixtureSelected(fxivar.toUInt());
            slotModeChanged(m_doc->mode());
            return;
        }
    }

    // Nothing selected, or a multi/mixed selection with no single clear
    // target (e.g. several fixtures) → a generic info browser.
    if (m_info == NULL)
        createInfo();
    m_info->setText(selectedCount == 0
        ? tr("<HTML><BODY><H1>Nothing selected</H1>"
             "<P>Select a fixture, group, universe, or power source "
             "from the list.</P></BODY></HTML>")
        : tr("<HTML><BODY><H1>%1 items selected</H1></BODY></HTML>").arg(selectedCount));

    // Enable/disable actions
    slotModeChanged(m_doc->mode());
}

void FixtureManager::slotChannelsGroupSelectionChanged()
{
    if (m_info == NULL)
        createInfo();

    int selectedCount = m_channel_groups_tree->selectedItems().size();

    if (selectedCount == 1)
    {
        QTreeWidgetItem* item = m_channel_groups_tree->selectedItems().first();
        Q_ASSERT(item != NULL);

        // Set the text view's contents
        QVariant grpvar = item->data(KColumnName, PROP_ID);
        if (grpvar.isValid() == true)
        {
            ChannelsGroup *chGroup = m_doc->channelsGroup(grpvar.toUInt());
            if (chGroup != NULL)
                m_info->setText(QString("%1<BODY>%2</BODY></HTML>")
                                .arg(channelsGroupInfoStyleSheetHeader())
                                .arg(channelsGroupInfo(chGroup)));
        }
        m_removeAction->setEnabled(true);
        m_propertiesAction->setEnabled(true);
        int selIdx = m_channel_groups_tree->currentIndex().row();
        if (selIdx == 0)
            m_moveUpAction->setEnabled(false);
        else
            m_moveUpAction->setEnabled(true);
        if (selIdx == m_channel_groups_tree->topLevelItemCount() - 1)
            m_moveDownAction->setEnabled(false);
        else
            m_moveDownAction->setEnabled(true);
    }
    else if (selectedCount > 1)
    {
        m_info->setText(tr("<HTML><BODY><H1>Multiple groups selected</H1>" \
                  "<P>Click <IMG SRC=\"" ":/edit_remove.png\">" \
                  " to remove the selected groups.</P></BODY></HTML>"));
        m_removeAction->setEnabled(true);
        m_propertiesAction->setEnabled(false);
    }
    else
    {
        m_info->setText(tr("<HTML><BODY><H1>Nothing selected</H1>" \
                  "<P>Select a channel group from the list or " \
                  "click <IMG SRC=\"" ":/edit_add.png\">" \
                  " to add a new channels group.</P></BODY></HTML>"));
        m_removeAction->setEnabled(false);
        m_propertiesAction->setEnabled(false);
    }
}

void FixtureManager::slotDoubleClicked(QTreeWidgetItem* item)
{
    if (item == NULL)
        return;

    const QVariant fxivar = item->data(KColumnName, PROP_ID);
    const QVariant grpvar = item->data(KColumnName, PROP_GROUP);

    if (fxivar.isValid() == true)
    {
        /* Double-click a fixture -> Fixture Properties, the same dialog as
           everywhere else (CONSOLIDATION_REVIEW #3); its definition and mode
           are a button in it, and "Definition & Mode..." in the menu. */
        if (m_doc->mode() != Doc::Operate)
        {
            if (Monitor *mon = Monitor::instance())
                mon->editFixtureProperties(fxivar.toUInt());
            else
                slotProperties();
        }
    }
    else if (grpvar.isValid() == true)
    {
        // Double-click a group -> open its editor
        FixtureGroup* grp = m_doc->fixtureGroup(grpvar.toUInt());
        if (grp != NULL)
            fixtureGroupSelected(grp);
    }
    // A universe or a power node just selects (slotSelectionChanged already
    // swaps the right pane to its view) — no extra double-click behaviour.
}

void FixtureManager::slotChannelsGroupDoubleClicked(QTreeWidgetItem*)
{
    slotChannelsGroupSelectionChanged();
    editChannelGroupProperties();
}

void FixtureManager::slotTabChanged(int index)
{
    // Toolbar composition is per-tab: the "Add" dropdown (fixture/RGB panel)
    // only makes sense on the fixtures tab; the Channel Groups tab gets its
    // own single "Add Channel Group..." button.
    const bool onChannelGroupsTab = (index == 1);
    m_addFixtureButtonAction->setVisible(onChannelGroupsTab == false);
    m_addChannelsGroupAction->setVisible(onChannelGroupsTab);

    if (index == 1)
    {
        updateChannelsGroupView();
        slotChannelsGroupSelectionChanged();
    }
    else if (index == 2)
    {
        updateRDMView();
    }
    else
    {
        updateView();
        slotSelectionChanged();
    }

    m_currentTabIndex = index;
}

void FixtureManager::slotFixtureItemExpanded()
{
    m_fixtures_tree->header()->resizeSections(QHeaderView::ResizeToContents);
}

void FixtureManager::slotDisplayFixtureInfo(QString &info)
{
    m_info->setText(info);
}

void FixtureManager::slotFixturesSearchChanged(const QString &text)
{
    m_fixtures_tree->setTextFilter(text);
}

void FixtureManager::selectGroup(quint32 id)
{
    for (int i = 0; i < m_fixtures_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* item = m_fixtures_tree->topLevelItem(i);
        QVariant var = item->data(KColumnName, PROP_GROUP);
        if (var.isValid() == false)
            continue;

        if (var.toUInt() == id)
        {
            m_fixtures_tree->setCurrentItem(item);
            slotSelectionChanged();
            break;
        }
    }
}

QString FixtureManager::fixtureInfoStyleSheetHeader()
{
    QString info;

    QPalette pal;
    QColor hlBack(pal.color(QPalette::Highlight));
    QColor hlText(pal.color(QPalette::HighlightedText));

    info += "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">";
    info += "<HTML><HEAD></HEAD><STYLE>";
    info += QString(".hilite {" \
                    "	background-color: %1;" \
                    "	color: %2;" \
                    "	font-size: x-large;" \
                    "}").arg(hlBack.name()).arg(hlText.name());
    info += QString(".subhi {" \
                    "	background-color: %1;" \
                    "	color: %2;" \
                    "	font-weight: bold;" \
                    "}").arg(hlBack.name()).arg(hlText.name());
    info += QString(".emphasis {" \
                    "	font-weight: bold;" \
                    "}");
    info += QString(".tiny {"\
                    "   font-size: small;" \
                    "}");
    info += QString(".author {" \
                    "	font-weight: light;" \
                    "	font-style: italic;" \
                    "   text-align: right;" \
                    "   font-size: small;"  \
                    "}");
    info += "</STYLE>";
    return info;
}

QString FixtureManager::fixtureInfo(const Fixture *fixture) const
{
    QString info;

    QString title("<TR><TD CLASS='hilite' COLSPAN='3'>%1</TD></TR>");
    QString subTitle("<TR><TD CLASS='subhi' COLSPAN='3'>%1</TD></TR>");
    QString genInfo("<TR><TD CLASS='emphasis'>%1</TD><TD COLSPAN='2'>%2</TD></TR>");

    /********************************************************************
     * General info
     ********************************************************************/

    info += "<TABLE COLS='3' WIDTH='100%'>";

    if (fixture == NULL)
    {
        info += "</TABLE>";
        return info;
    }

    // Fixture title
    info += title.arg(fixture->name());

    const QLCFixtureDef* fixtureDef = fixture->fixtureDef();
    const QLCFixtureMode* fixtureMode = fixture->fixtureMode();

    if (fixtureDef != NULL && fixtureMode != NULL)
    {
        // Manufacturer
        info += genInfo.arg(tr("Manufacturer")).arg(fixtureDef->manufacturer());
        info += genInfo.arg(tr("Model")).arg(fixtureDef->model());
        info += genInfo.arg(tr("Mode")).arg(fixtureMode->name());
        info += genInfo.arg(tr("Type")).arg(fixtureDef->typeToString(fixtureDef->type()));
        info += genInfo.arg(tr("Definition")).arg(fixtureDef->sourceDescription());
    }

    // Connection (only worth stating when it isn't plain DMX)
    if (fixture->connection() == Fixture::Network)
        info += genInfo.arg(tr("Connection")).arg(fixture->ipAddress().isEmpty()
                                ? tr("Network (no IP set)")
                                : tr("Network, unicast to %1").arg(fixture->ipAddress()));

    if (fixture->isPatched())
    {
        // Universe
        info += genInfo.arg(tr("Universe")).arg(fixture->universe() + 1);

        // Address
        QString range = QString("%1 - %2").arg(fixture->address() + 1).arg(fixture->address() + fixture->channels());
        info += genInfo.arg(tr("Address Range")).arg(range);
    }
    else
    {
        info += genInfo.arg(tr("Patch")).arg(tr("Unpatched -- in the inventory"));
    }

    // Channels
    info += genInfo.arg(tr("Channels")).arg(fixture->channels());

    if (fixture->isPatched())
    {
        // Binary address
        QString binaryStr = QString("%1").arg(fixture->address() + 1, 10, 2, QChar('0'));
        QString dipTable("<TABLE COLS='33' cellspacing='0'><TR><TD COLSPAN='33'><IMG SRC=\"" ":/ds_top.png\"></TD></TR>");
        dipTable += "<TR><TD><IMG SRC=\"" ":/ds_border.png\"></TD><TD><IMG SRC=\"" ":/ds_border.png\"></TD>";
        for (int i = 9; i >= 0; i--)
        {
            if (binaryStr.at(i) == '0')
                dipTable += "<TD COLSPAN='3'><IMG SRC=\"" ":/ds_off.png\"></TD>";
            else
                dipTable += "<TD COLSPAN='3'><IMG SRC=\"" ":/ds_on.png\"></TD>";
        }
        dipTable += "<TD><IMG SRC=\"" ":/ds_border.png\"></TD></TR>";
        dipTable += "<TR><TD COLSPAN='33'><IMG SRC=\"" ":/ds_bottom.png\"></TD></TR>";
        dipTable += "</TABLE>";

        info += genInfo.arg(tr("Binary Address (DIP)"))
                .arg(QString("%1").arg(dipTable));
    }

    /********************************************************************
     * Channels
     ********************************************************************/

    // Title row
    info += QString("<TR><TD CLASS='subhi'>%1</TD>").arg(tr("Channel"));
    info += QString("<TD CLASS='subhi'>%1</TD>").arg(tr("DMX"));
    info += QString("<TD CLASS='subhi'>%1</TD></TR>").arg(tr("Name"));

    // Fill table with the fixture's channels
    for (quint32 ch = 0; ch < fixture->channels(); ch++)
    {
        QString chInfo("<TR><TD>%1</TD><TD>%2</TD><TD>%3</TD></TR>");
        info += chInfo.arg(ch + 1)
                .arg(fixture->isPatched() ? QString::number(fixture->address() + ch + 1)
                                          : QString::fromUtf8("\xE2\x80\x94"))
                .arg(fixture->channel(ch)->name());
    }

    /********************************************************************
     * Extended device information
     ********************************************************************/

    if (fixtureMode != NULL)
    {
        const QLCPhysical physical = fixtureMode->physical();
        info += title.arg(tr("Physical"));

        // In the person's unit (Settings -> Units), what the file holds after.
        typedef MeasurementUnits MU;
        const MU::LengthUnit unit = MU::preferredLength();
        const MU::WeightUnit wunit = MU::weightUnitFor(unit);
        auto len = [unit](int mm) {
            return unit == MU::Millimetres ? MU::formatLength(mm, unit)
                   : QStringLiteral("%1 (%2 mm)").arg(MU::formatLength(mm, unit)).arg(mm);
        };
        QString W("%1W");
        info += genInfo.arg(tr("Width")).arg(len(physical.width()));
        info += genInfo.arg(tr("Height")).arg(len(physical.height()));
        info += genInfo.arg(tr("Depth")).arg(len(physical.depth()));
        info += genInfo.arg(tr("Weight")).arg(wunit == MU::Kilograms
                ? MU::formatWeight(physical.weight(), wunit)
                : QStringLiteral("%1 (%2)").arg(MU::formatWeight(physical.weight(), wunit),
                                                MU::formatWeight(physical.weight(), MU::Kilograms)));
        info += genInfo.arg(tr("Power consumption")).arg(W.arg(physical.powerConsumption()));
        info += genInfo.arg(tr("DMX Connector")).arg(physical.dmxConnector());

        // Bulb
        QString K("%1K");
        QString lm("%1lm");
        info += subTitle.arg(tr("Bulb"));
        info += genInfo.arg(tr("Type")).arg(physical.bulbType());
        info += genInfo.arg(tr("Luminous Flux")).arg(lm.arg(physical.bulbLumens()));
        info += genInfo.arg(tr("Colour Temperature")).arg(K.arg(physical.bulbColourTemperature()));

        // Lens
        QString angle1("%1&deg;");
        QString angle2("%1&deg; &ndash; %2&deg;");

        info += subTitle.arg(tr("Lens"));
        info += genInfo.arg(tr("Name")).arg(physical.lensName());

        if (physical.lensDegreesMin() == physical.lensDegreesMax())
        {
            info += genInfo.arg(tr("Beam Angle"))
                .arg(angle1.arg(physical.lensDegreesMin()));
        }
        else
        {
            info += genInfo.arg(tr("Beam Angle"))
                .arg(angle2.arg(physical.lensDegreesMin())
                .arg(physical.lensDegreesMax()));
        }
        if (physical.lensBeamsPerHead() > 1)
            info += genInfo.arg(tr("Beams per Head"))
                .arg(physical.lensBeamsPerHead());

        // Focus
        QString frange("%1&deg;");
        info += subTitle.arg(tr("Head(s)"));
        info += genInfo.arg(tr("Type")).arg(physical.focusType());
        info += genInfo.arg(tr("Pan Range")).arg(frange.arg(physical.focusPanMax()));
        info += genInfo.arg(tr("Tilt Range")).arg(frange.arg(physical.focusTiltMax()));
        if (physical.layoutSize() != QSize(1, 1))
        {
            info += genInfo.arg(tr("Layout"))
                           .arg(QString("%1 x %2").arg(physical.layoutSize().width()).arg(physical.layoutSize().height()));
        }
    }

    // HTML document & table closure
    info += "</TABLE>";

    if (fixtureDef != NULL)
    {
        info += "<HR>";
        info += "<DIV CLASS='author' ALIGN='right'>";
        info += tr("Fixture definition author: ") + fixtureDef->author();
        info += "</DIV>";
    }

    return info;
}

QString FixtureManager::channelsGroupInfoStyleSheetHeader()
{
    QString info;

    QPalette pal;
    QColor hlBack(pal.color(QPalette::Highlight));
    QColor hlBackSmall(pal.color(QPalette::Shadow));
    QColor hlText(pal.color(QPalette::HighlightedText));

    info += "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">";
    info += "<HTML><HEAD></HEAD><STYLE>";
    info += QString(".hilite {" \
                    "	background-color: %1;" \
                    "	color: %2;" \
                    "	font-size: x-large;" \
                    "}").arg(hlBack.name()).arg(hlText.name());
    info += QString(".subhi {" \
                    "	background-color: %1;" \
                    "	color: %2;" \
                    "	font-weight: bold;" \
                    "}").arg(hlBackSmall.name()).arg(hlText.name());
    info += QString(".emphasis {" \
                    "	font-weight: bold;" \
                    "}");
    info += QString(".tiny {"\
                    "   font-size: small;" \
                    "}");
    info += "</STYLE>";
    return info;
}

QString FixtureManager::channelsGroupInfo(const ChannelsGroup *channelsGroup) const
{
    QString info;

    info += "<TABLE COLS='3' WIDTH='100%'>";

    if (channelsGroup != NULL)
    {
        // Fixture title
        const QString title("<TR><TD CLASS='hilite' COLSPAN='3'><CENTER>%1</CENTER></TD></TR>");
        info += title.arg(channelsGroup->name());

        /********************************************************************
         * Channels
         ********************************************************************/

        // Title row
        info += QString("<TR><TD CLASS='subhi'>%1</TD>").arg(tr("Fixture"));
        info += QString("<TD CLASS='subhi'>%1</TD>").arg(tr("Channel"));
        info += QString("<TD CLASS='subhi'>%1</TD></TR>").arg(tr("Description"));

        foreach (const SceneValue value, channelsGroup->getChannels())
        {
            const Fixture *fixture = m_doc->fixture(value.fxi);
            if (fixture == NULL)
                continue;

            const QLCFixtureMode *mode = fixture->fixtureMode();
            const QString chInfo("<TR><TD>%1</TD><TD>%2</TD><TD>%3</TD></TR>");
            if (mode != NULL)
            {
                info += chInfo.arg(fixture->name()).arg(value.channel + 1)
                    .arg(mode->channels().at(value.channel)->name());
            }
            else
            {
                info += chInfo.arg(fixture->name()).arg(value.channel + 1)
                    .arg(QString(tr("Channel %1")).arg(value.channel));
            }
        }
    }

    // HTML document & table closure
    info += "</TABLE>";

    return info;
}

/*****************************************************************************
 * Menu, toolbar and actions
 *****************************************************************************/

void FixtureManager::initActions()
{
    // Fixture actions
    m_addAction = new QAction(QIcon(":/edit_add.png"),
                              tr("Add fixture..."), this);
    connect(m_addAction, &QAction::triggered, this, &FixtureManager::addFixture);

    m_addInventoryAction = new QAction(QIcon(":/edit_add.png"),
                              tr("Add to inventory..."), this);
    m_addInventoryAction->setToolTip(tr("Stage a fixture in the inventory: it can be placed and "
                                        "grouped in the Studio now and given a universe and "
                                        "address later"));
    connect(m_addInventoryAction, &QAction::triggered, this, &FixtureManager::addToInventory);
    // Same Design-only gating as "Add fixture...": mirror it rather than
    // threading a second action through every branch of slotModeChanged().
    connect(m_addAction, &QAction::changed, this, [this]() {
        m_addInventoryAction->setEnabled(m_addAction->isEnabled());
        m_patchInventoryAction->setEnabled(m_addAction->isEnabled());
    });
    m_patchInventoryAction = new QAction(tr("Patch Inventory…"), this);
    m_patchInventoryAction->setToolTip(tr("Give the staged fixtures a real universe and address"));
    connect(m_patchInventoryAction, &QAction::triggered, this, [this]() { patchInventory(); });

    m_addRGBAction = new QAction(QIcon(":/rgbpanel.png"),
                              tr("Add RGB panel..."), this);
    connect(m_addRGBAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAddRGBPanel()));

    // Channel Groups tab's own creation action — split out from m_addAction
    // (see m_addChannelsGroupAction's declaration) so the toolbar never
    // shows the wrong label for the active tab.
    m_addChannelsGroupAction = new QAction(QIcon(":/edit_add.png"),
                              tr("Add Channel Group..."), this);
    connect(m_addChannelsGroupAction, &QAction::triggered, this, &FixtureManager::addChannelsGroup);

    m_removeAction = new QAction(QIcon(":/edit_remove.png"),
                                 tr("Delete…"), this);
    connect(m_removeAction, SIGNAL(triggered(bool)),
            this, SLOT(slotRemove()));

    m_propertiesAction = new QAction(QIcon(":/configure.png"),
                                     tr("Fixture Properties…"), this);
    connect(m_propertiesAction, SIGNAL(triggered(bool)),
            this, SLOT(slotProperties()));

    m_testAction = new QAction(QIcon(":/fixture.png"),
                               tr("Test Fixture…"), this);
    m_testAction->setEnabled(false);
    connect(m_testAction, SIGNAL(triggered(bool)),
            this, SLOT(slotTestFixture()));

    m_locateAction = new QAction(QIcon(":/fixture.png"), tr("Locate"), this);
    m_locateAction->setToolTip(tr("Flash this fixture at full intensity 3 times "
                                  "to identify it on the rig."));
    m_locateAction->setEnabled(false);
    connect(m_locateAction, &QAction::triggered, this, &FixtureManager::slotLocateFixture);

    m_resetAction = new QAction(QIcon(":/fixture.png"), tr("Reset"), this);
    m_resetAction->setToolTip(tr("Send this fixture's reset command (only shown "
                                 "when its definition has one)."));
    m_resetAction->setEnabled(false);
    connect(m_resetAction, &QAction::triggered, this, &FixtureManager::slotResetFixture);

    /* Named "Channels Fade Configuration" it read as fade-only, so the channel
       MODIFIER column -- the tool that remaps a channel's DMX values on the way
       out, e.g. an LM70 whose 0-127 is intensity and 128-255 is strobe -- was
       effectively unfindable. Same dialog, honest name. */
    m_fadeConfigAction = new QAction(QIcon(":/fade.png"),
                                     tr("Channel Behaviour && Modifiers…"), this);
    m_fadeConfigAction->setToolTip(
        tr("Per-channel fade behaviour (HTP/LTP, can-fade) and value modifier "
           "curves, which remap a channel's DMX values as they are sent."));
    connect(m_fadeConfigAction, SIGNAL(triggered(bool)),
            this, SLOT(slotFadeConfig()));

    // Group actions
    m_groupAction = new QAction(QIcon(":/group.png"),
                                tr("Add fixture to group..."), this);
    m_groupAction->setToolTip(tr(
        "Add the selected fixture(s) to an existing group, or create a new "
        "one — click the dropdown arrow and pick \"New Group...\" at the "
        "bottom of the list."));

    m_unGroupAction = new QAction(QIcon(":/ungroup.png"),
                                tr("Remove from Group"), this);
    connect(m_unGroupAction, SIGNAL(triggered(bool)),
            this, SLOT(slotUnGroup()));

    m_newGroupAction = new QAction(tr("New Group..."), this);

    m_moveUpAction = new QAction(QIcon(":/up.png"),
                                 tr("Move channel group up..."), this);
    m_moveUpAction->setEnabled(false);
    connect(m_moveUpAction, SIGNAL(triggered(bool)),
            this, SLOT(slotMoveGroupUp()));

    m_moveDownAction = new QAction(QIcon(":/down.png"),
                                 tr("Move channel group down..."), this);
    m_moveDownAction->setEnabled(false);
    connect(m_moveDownAction, SIGNAL(triggered(bool)),
            this, SLOT(slotMoveGroupDown()));

    m_importAction = new QAction(QIcon(":/fileimport.png"),
                                 tr("Import fixtures..."), this);
    connect(m_importAction, SIGNAL(triggered(bool)),
            this, SLOT(slotImport()));

    m_exportAction = new QAction(QIcon(":/fileexport.png"),
                                 tr("Export fixtures..."), this);

    connect(m_exportAction, SIGNAL(triggered(bool)),
            this, SLOT(slotExport()));

    m_remapAction = new QAction(QIcon(":/remap.png"),
                               tr("Remap fixtures..."), this);
    connect(m_remapAction, SIGNAL(triggered(bool)),
            this, SLOT(slotRemap()));

    m_patchListAction = new QAction(QIcon(":/fileexport.png"), tr("Patch List..."), this);
    m_patchListAction->setToolTip(tr("Every fixture's patch, output and mount, to save as CSV or PDF"));
    connect(m_patchListAction, &QAction::triggered, this, &FixtureManager::slotPatchList);

    // Icon-less, matching ConnectionsTree::appendUniversalMenuActions()'s
    // "Collapse/Expand everything below" -- expand/collapse-all is a tree-
    // state toggle, not an add/remove of anything, so it shouldn't borrow
    // edit_add/edit_remove's create/delete iconography.
    m_expandAllAction = new QAction(tr("Expand All"), this);
    connect(m_expandAllAction, &QAction::triggered, this,
            [this]() { if (m_fixtures_tree) m_fixtures_tree->expandAll(); });

    m_collapseAllAction = new QAction(tr("Collapse All"), this);
    connect(m_collapseAllAction, &QAction::triggered, this,
            [this]() { if (m_fixtures_tree) m_fixtures_tree->collapseAll(); });
}

void FixtureManager::updateGroupMenu()
{
    if (m_groupMenu == NULL)
    {
        m_groupMenu = new QMenu(this);
        connect(m_groupMenu, SIGNAL(triggered(QAction*)),
                this, SLOT(slotGroupSelected(QAction*)));
    }

    foreach (QAction* a, m_groupMenu->actions())
        m_groupMenu->removeAction(a);

    // Put all known fixture groups to the menu. Creating a new one no longer
    // lives here -- it's a right-click on the Fixture Groups root/a group
    // folder instead (slotContextMenuRequested()) -- so every entry from
    // here on is a real existing group with valid data().
    foreach (FixtureGroup* grp, m_doc->fixtureGroups())
    {
        QAction* a = m_groupMenu->addAction(grp->name());
        a->setData((qulonglong) grp);
    }

    // Put the group menu to the group action
    m_groupAction->setMenu(m_groupMenu);
}

void FixtureManager::initToolBar()
{
    QToolBar* toolbar = new QToolBar(tr("Fixture manager"), this);
    m_toolbar = toolbar;
    toolbar->setFloatable(false);
    toolbar->setMovable(false);
    // Was falling back to the platform default (unset here) — with the
    // default TextUnderIcon label mode and several long action labels
    // ("Channels Fade Configuration"), that made the whole toolbar
    // untenably wide. Matches showtimelineeditor.cpp's existing 20x20,
    // extended here to the other manager toolbars that never set one.
    toolbar->setIconSize(QSize(20, 20));
    layout()->setMenuBar(toolbar);

    // Trimmed to creation-only actions (2026-09-03 modernization pass):
    // Properties/Test/Remove/Ungroup/Move Up/Down are all selection- or
    // row-dependent, so they now live only in the tree context menus
    // (slotContextMenuRequested() / slotChannelGroupContextMenuRequested()),
    // matching Connections/Devices' toolbar-free convention. Import/Export/
    // Remap/Fade Configuration are document-wide, not row-dependent either —
    // those moved to App's "Fixtures" menu bar entry (populateFixturesMenu()).
    // Expand/Collapse-all moved into the fixture tree's context menu.
    // "Add fixture to group..." (m_groupAction) is also selection-dependent
    // (it assigns THIS selection to an existing group) -- dropped from the
    // toolbar too, staying only in the fixture context menu. Group CREATION
    // moved off this toolbar entirely, onto the Fixture Groups root/a group
    // folder's own right-click ("New Group...", see
    // slotContextMenuRequested()) instead of a menu-bar/toolbar action.

    // "Add" dropdown (Add Fixture.../Add RGB panel...) — consolidates what
    // used to be two separate always-visible buttons. Fixtures-tab only;
    // hidden on the Channel Groups tab in favor of m_addChannelsGroupAction.
    QToolButton* addBtn = new QToolButton(toolbar);
    addBtn->setText(tr("Add"));
    addBtn->setIcon(QIcon(":/edit_add.png"));
    addBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    addBtn->setPopupMode(QToolButton::InstantPopup);
    QMenu* addMenu = new QMenu(addBtn);
    addMenu->addAction(m_addAction);
    addMenu->addAction(m_addInventoryAction);
    addMenu->addAction(m_patchInventoryAction);
    addMenu->addAction(m_addRGBAction);
    addBtn->setMenu(addMenu);
    m_addFixtureButtonAction = toolbar->addWidget(addBtn);

    // addBtn is a plain QToolButton, not an action-backed toolbar button, so
    // it doesn't automatically gray out when its menu's actions do (e.g.
    // Operate mode, via slotModeChanged()) -- mirror m_addAction's enabled
    // state onto it explicitly. m_addAction and m_addRGBAction are always
    // enabled/disabled together in slotModeChanged(), so tracking either is
    // equivalent.
    connect(m_addAction, &QAction::changed, addBtn, [addBtn, this]() {
        addBtn->setEnabled(m_addAction->isEnabled());
    });
    addBtn->setEnabled(m_addAction->isEnabled());

    toolbar->addAction(m_addChannelsGroupAction);

    // Initial per-tab visibility: the fixtures tab (index 0) is active at
    // startup, and slotTabChanged() isn't guaranteed to fire before this
    // (its QTabWidget::currentChanged connection is only made once both
    // tabs already exist in initDataView(), called after this).
    m_addChannelsGroupAction->setVisible(false);

    // Match the main window's icon/text display preference.
    applyToolbarLabelMode();
}

void FixtureManager::applyToolbarLabelMode()
{
    // Mirror App::TabLabelMode: 0 = Icon+Text (-> text under icon),
    // 1 = Icons only, 2 = Text only. Same "workspace/tabLabelMode" setting.
    Qt::ToolButtonStyle style = Qt::ToolButtonTextUnderIcon;
    const int mode = QSettings().value(QStringLiteral("workspace/tabLabelMode"), 0).toInt();
    if (mode == 1)
        style = Qt::ToolButtonIconOnly;
    else if (mode == 2)
        style = Qt::ToolButtonTextOnly;

    if (m_toolbar)
        m_toolbar->setToolButtonStyle(style);
}

void FixtureManager::populateFixturesMenu(QMenu *menu)
{
    menu->addAction(m_importAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_remapAction);
    menu->addAction(m_patchListAction);
    menu->addSeparator();
    menu->addAction(m_fadeConfigAction);
}

void FixtureManager::slotPatchList()
{
    PatchListDialog dlg(m_doc, this);
    dlg.exec();
}

void FixtureManager::addFixture()
{
    // With the Inventory root selected, "Add fixture" means add to the inventory:
    // asking for a universe and address there is exactly what you are avoiding.
    QTreeWidgetItem *current = m_fixtures_tree->currentItem();
    const bool inInventory = current != NULL
                          && current->data(KColumnName, PROP_INVENTORY).isValid();
    addFixtureImpl(inInventory);
}

void FixtureManager::addToInventory()
{
    addFixtureImpl(true);
}

void FixtureManager::returnToInventory(const QList<quint32> &fixtureIds)
{
    if (QMessageBox::question(this, tr("Return to inventory"),
            tr("Release the universe and address of %n fixture(s)?\n\n"
               "They stay in the show -- placement, groups and looks are kept -- but stop "
               "sending DMX until they are patched again.", "", fixtureIds.size()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    int done = 0;
    foreach (quint32 fid, fixtureIds)
        if (m_doc->unpatchFixture(fid))
            done++;
    if (done > 0)
        updateView();
}

void FixtureManager::patchInventory(const QList<quint32> &fixtureIds)
{
    patchOrReaddress(fixtureIds);
}

void FixtureManager::patchOrReaddress(const QList<quint32> &fixtureIds, quint32 universe)
{
    PatchInventoryDialog dlg(m_doc, fixtureIds, this);
    if (universe != UINT_MAX)
        dlg.placeRows(universe, 0, false);
    if (dlg.exec() == QDialog::Accepted)
        updateView();
}

void FixtureManager::slotFixturesDroppedOnUniverse(const QList<quint32> &fixtureIds, quint32 universe)
{
    /* "clean ways to assign fixtures into universes": dragged onto a universe,
       they are proposed packed into its first free channels -- reviewed and
       applied in the Patch dialog, not moved behind the LD's back. */
    patchOrReaddress(fixtureIds, universe);
}

void FixtureManager::addFixtureImpl(bool inventory, quint32 likeFixture, const QString &manufacturer)
{
    // The group the new fixtures join: the selected fixture's group, or the
    // selected group itself.
    FixtureGroup* addToGroup = NULL;
    QTreeWidgetItem* current = m_fixtures_tree->currentItem();
    if (current != NULL)
    {
        if (current->parent() != NULL)
        {
            // Fixture selected
            QVariant var = current->parent()->data(KColumnName, PROP_GROUP);
            if (var.isValid() == true)
                addToGroup = m_doc->fixtureGroup(var.toUInt());
        }
        else
        {
            // Group selected
            QVariant var = current->data(KColumnName, PROP_GROUP);
            if (var.isValid() == true)
                addToGroup = m_doc->fixtureGroup(var.toUInt());
        }
    }

    const QList<quint32> created =
        FixtureCreator::addFromDialog(this, m_doc, inventory, m_ctxUniverse, addToGroup,
                                      m_doc->fixture(likeFixture), manufacturer);
    if (created.isEmpty())
        return;

    updateView();
    QTreeWidgetItem* selectItem = m_fixtures_tree->fixtureItem(created.last());
    if (selectItem != NULL)
        m_fixtures_tree->setCurrentItem(selectItem);
}

void FixtureManager::addChannelsGroup()
{
    ChannelsGroup *group = new ChannelsGroup(m_doc);

    AddChannelsGroup cs(this, m_doc, group);
    if (cs.exec() == QDialog::Accepted)
    {
        qDebug() << "Channels group added. Count: " << group->getChannels().count();
        m_doc->addChannelsGroup(group, group->id());
        updateChannelsGroupView();
    }
    else
        delete group;
}

void FixtureManager::slotAddRGBPanel()
{
    AddRGBPanel rgb(this, m_doc);
    if (rgb.exec() == QDialog::Accepted)
    {
        int rows = rgb.rows();
        int columns = rgb.columns();
        Fixture::Components components = rgb.components();

        FixtureGroup *grp = new FixtureGroup(m_doc);
        Q_ASSERT(grp != NULL);
        grp->setName(rgb.name());
        QSize panelSize(columns, rows);
        grp->setSize(panelSize);
        m_doc->addFixtureGroup(grp);
        updateGroupMenu();

        int transpose = 0;
        if (rgb.direction() == AddRGBPanel::Vertical)
        {
        	int tmp = columns;
        	columns = rows;
        	rows = tmp;
        	transpose = 1;
        }

        QLCFixtureDef *rowDef = NULL;
        QLCFixtureMode *rowMode = NULL;
        quint32 address = (quint32)rgb.address();
        int uniIndex = rgb.universeIndex();
        int currRow = 0;
        int rowInc = 1;
        int xPosStart = 0;
        int xPosEnd = columns - 1;
        int xPosInc = 1;

        quint32 phyWidth = rgb.physicalWidth();
        quint32 phyHeight = rgb.physicalHeight() / rows;

        if (transpose)
        {
			if (rgb.orientation() == AddRGBPanel::TopRight ||
				rgb.orientation() == AddRGBPanel::BottomRight)
			{
				currRow = rows -1;
				rowInc = -1;
			}
			if (rgb.orientation() == AddRGBPanel::BottomRight ||
				rgb.orientation() == AddRGBPanel::BottomLeft)
			{
				xPosStart = columns - 1;
				xPosEnd = 0;
				xPosInc = -1;
			}
        }
        else
        {
			if (rgb.orientation() == AddRGBPanel::BottomLeft ||
				rgb.orientation() == AddRGBPanel::BottomRight)
			{
				currRow = rows -1;
				rowInc = -1;
			}
			if (rgb.orientation() == AddRGBPanel::TopRight ||
				rgb.orientation() == AddRGBPanel::BottomRight)
			{
				xPosStart = columns - 1;
				xPosEnd = 0;
				xPosInc = -1;
			}
        }

        for (int i = 0; i < rows; i++)
        {
            Fixture *fxi = new Fixture(m_doc);
            Q_ASSERT(fxi != NULL);
            fxi->setName(m_doc->uniqueFixtureName(tr("%1 - Row %2").arg(rgb.name()).arg(i + 1)));
            if (rowDef == NULL)
                rowDef = fxi->genericRGBPanelDef(columns, components, rgb.is16Bit());
            if (rowMode == NULL)
                rowMode = fxi->genericRGBPanelMode(rowDef, components, rgb.is16Bit(), phyWidth, phyHeight);
            fxi->setFixtureDefinition(rowDef, rowMode);

            // Check universe span
            if (address + fxi->channels() > 512)
            {
                if (!rgb.crossUniverse())
                {
                    uniIndex++;
                    address = 0;
                }
            }
            if (m_doc->inputOutputMap()->getUniverseID(uniIndex) == m_doc->inputOutputMap()->invalidUniverse())
            {
                m_doc->inputOutputMap()->addUniverse();
                m_doc->inputOutputMap()->startUniverses();
            }

            fxi->setUniverse(m_doc->inputOutputMap()->getUniverseID(uniIndex));
            if (address + fxi->channels() > 512)
                fxi->setCrossUniverse(rgb.crossUniverse());
            fxi->setAddress(address);
            m_doc->addFixture(fxi, Fixture::invalidId(), rgb.crossUniverse());

            address += fxi->channels();
            if (address >= 512 && rgb.crossUniverse())
            {
                address -= 512;
                uniIndex++;
            }

            if (rgb.type() == AddRGBPanel::ZigZag)
            {
                int xPos = xPosStart;
                for (int h = 0; h < fxi->heads(); h++)
                {
                	if (transpose)
                		grp->assignHead(QLCPoint(currRow, xPos), GroupHead(fxi->id(), h));
                	else
                		grp->assignHead(QLCPoint(xPos, currRow), GroupHead(fxi->id(), h));
                    xPos += xPosInc;
                }
            }
            else if (rgb.type() == AddRGBPanel::Snake)
            {
                if (i%2 == 0)
                {
                    int xPos = xPosStart;
                    for (int h = 0; h < fxi->heads(); h++)
                    {
                    	if (transpose)
                    		grp->assignHead(QLCPoint(currRow, xPos), GroupHead(fxi->id(), h));
                    	else
                    		grp->assignHead(QLCPoint(xPos, currRow), GroupHead(fxi->id(), h));
                        xPos += xPosInc;
                    }
                }
                else
                {
                    int xPos = xPosEnd;
                    for (int h = 0; h < fxi->heads(); h++)
                    {
                    	if (transpose)
                    		grp->assignHead(QLCPoint(currRow, xPos), GroupHead(fxi->id(), h));
                    	else
                    		grp->assignHead(QLCPoint(xPos, currRow), GroupHead(fxi->id(), h));
                        xPos += (-xPosInc);
                    }
                }
            }
            currRow += rowInc;
        }

        updateView();
        m_doc->setModified();
    }
}

void FixtureManager::removeFixture()
{
    // Ask before deletion
    if (QMessageBox::question(this, tr("Delete Fixtures"),
                              tr("Do you want to delete the selected items?"),
                              QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
    {
        return;
    }

    QListIterator <QTreeWidgetItem*> it(m_fixtures_tree->selectedItems());

    // We put items to delete in sets,
    // so no segfault happens when the same fixture is selected twice
    QSet <quint32> groupsToDelete;
    QSet <quint32> fixturesToDelete;
    while (it.hasNext() == true)
    {
        QTreeWidgetItem* item(it.next());
        Q_ASSERT(item != NULL);

        // Is the item a fixture ?
        QVariant var = item->data(KColumnName, PROP_ID);
        if (var.isValid() == true)
            fixturesToDelete << var.toUInt();
        else
        {
            // Is the item a fixture group ?
            var = item->data(KColumnName, PROP_GROUP);
            if (var.isValid() == true)
                groupsToDelete << var.toUInt();
        }
    }

    // delete fixture groups
    foreach (quint32 id, groupsToDelete)
        m_doc->deleteFixtureGroup(id);

    // delete fixtures
    foreach (quint32 id, fixturesToDelete)
    {
        /** @todo This is REALLY bogus here, since Fixture or Doc should do
            this. However, FixtureManager is the only place to destroy fixtures,
            so it's rather safe to reset the fixture's address space here. */
        Fixture* fxi = m_doc->fixture(id);
        if (fxi == NULL)
        {
            /* A selected row's id no longer matching a live fixture means
               something else removed it (or the selection went stale)
               between building this set and getting here -- there is
               nothing left to reset or delete for it, so this used to be a
               hard Q_ASSERT that took the whole app down over one already-
               gone fixture. Skip it and keep going; the others in this same
               batch are still real. */
            qWarning() << Q_FUNC_INFO << "Selected fixture" << id
                       << "no longer exists; skipping its deletion";
            continue;
        }
        QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
        int universe = fxi->universe();
        if (universe < ua.count())
            ua[universe]->reset(fxi->address(), fxi->channels());
        m_doc->inputOutputMap()->releaseUniverses();

        m_doc->deleteFixture(id);
    }
}

void FixtureManager::removeChannelsGroup()
{
    // Ask before deletion
    if (QMessageBox::question(this, tr("Delete Channels Group"),
                              tr("Do you want to delete the selected groups?"),
                              QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
    {
        return;
    }

    disconnect(m_channel_groups_tree, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotChannelsGroupSelectionChanged()));

    QListIterator <QTreeWidgetItem*> it(m_channel_groups_tree->selectedItems());
    while (it.hasNext() == true)
    {
        QTreeWidgetItem* item(it.next());
        Q_ASSERT(item != NULL);

        QVariant var = item->data(KColumnName, PROP_ID);
        if (var.isValid() == true)
            m_doc->deleteChannelsGroup(var.toUInt());
    }
    updateChannelsGroupView();

    connect(m_channel_groups_tree, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotChannelsGroupSelectionChanged()));
}

void FixtureManager::slotRemove()
{
    if (m_currentTabIndex == 1)
        removeChannelsGroup();
    else
        removeFixture();
}

void FixtureManager::editFixtureProperties()
{
    QTreeWidgetItem* item = m_fixtures_tree->currentItem();
    if (item == NULL)
        return;

    QVariant var = item->data(KColumnName, PROP_ID);
    if (var.isValid() == false)
        return;
    editDefinitionAndMode(this, m_doc, var.toUInt());
}

bool FixtureManager::editDefinitionAndMode(QWidget *parent, Doc *doc, quint32 fid)
{
    Fixture* fxi = (doc != NULL) ? doc->fixture(fid) : NULL;
    if (fxi == NULL)
        return false;

    QString manuf;
    QString model;
    QString mode;

    if (fxi->fixtureDef() != NULL)
    {
        manuf = fxi->fixtureDef()->manufacturer();
        model = fxi->fixtureDef()->model();
        mode = fxi->fixtureMode()->name();
    }

    AddFixture af(parent, doc, fxi);
    af.setInventoryMode(!fxi->isPatched());
    af.setWindowTitle(fxi->isPatched() ? tr("Change fixture properties")
                                       : tr("Change inventory fixture"));
    if (af.exec() == QDialog::Accepted)
    {
        if (af.invalidAddress() == false)
        {
            bool changed = false;

            fxi->blockSignals(true);
            if (fxi->name() != af.name())
            {
                fxi->setName(af.name());
                changed = true;
            }
            // An inventory fixture's universe/address are placeholders: the
            // dialog hides them, and they must not be "changed" into a claim.
            if (fxi->isPatched() && fxi->universe() != af.universe())
            {
                fxi->setUniverse(af.universe());
                changed = true;
            }
            if (fxi->isPatched() && fxi->address() != af.address())
            {
                fxi->setAddress(af.address());
                changed = true;
            }
            if (fxi->connection() != af.connection())
            {
                fxi->setConnection(af.connection());
                changed = true;
            }
            if (fxi->ipAddress() != af.ipAddress())
            {
                fxi->setIPAddress(af.ipAddress());
                changed = true;
            }
            if (fxi->transport() != af.transport())
            {
                fxi->setTransport(af.transport());
                changed = true;
            }
            fxi->blockSignals(false);

            if (af.fixtureDef() != NULL && af.mode() != NULL)
            {
                if (af.fixtureDef()->manufacturer() == KXMLFixtureGeneric &&
                    af.fixtureDef()->model() == KXMLFixtureGeneric)
                {
                    if (fxi->channels() != af.channels())
                    {
                        QLCFixtureDef* fixtureDef = fxi->genericDimmerDef(af.channels());
                        QLCFixtureMode* fixtureMode = fxi->genericDimmerMode(fixtureDef, af.channels());
                        fxi->setFixtureDefinition(fixtureDef, fixtureMode);
                    }
                }
                else
                {
                    fxi->setFixtureDefinition(af.fixtureDef(), af.mode());
                }
            }
            else
            {
                /* Generic dimmer */
                fxi->setFixtureDefinition(NULL, NULL);
                fxi->setChannels(af.channels());
            }

            // Emit changed signal
            if (changed)
                fxi->setID(fxi->id());

            if (FixtureManager *fm = FixtureManager::instance())
            {
                fm->updateView();
                fm->slotSelectionChanged();
            }
            return true;
        }
        else
        {
            QMessageBox msg(QMessageBox::Critical, tr("Error"),
                    tr("Please enter a valid address"), QMessageBox::Ok);
            msg.exec();
        }
    }
    return false;
}

void FixtureManager::editChannelGroupProperties()
{
    int selectedCount = m_channel_groups_tree->selectedItems().size();

    if (selectedCount > 0)
    {
        QTreeWidgetItem* current = m_channel_groups_tree->selectedItems().first();
        QVariant var = current->data(KColumnName, PROP_ID);
        if (var.isValid() == true)
        {
            ChannelsGroup *group = m_doc->channelsGroup(var.toUInt());

            AddChannelsGroup cs(this, m_doc, group);
            if (cs.exec() == QDialog::Accepted)
            {
                qDebug() << "CHANNEL GROUP MODIFIED. Count: " << group->getChannels().count();
                m_doc->addChannelsGroup(group, group->id());
                updateChannelsGroupView();
            }
        }
    }
}

void FixtureManager::slotProperties()
{
    if (m_currentTabIndex == 1)
    {
        editChannelGroupProperties();
        return;
    }
    // A fixture: Fixture Properties, the one dialog for a fixture everywhere.
    QTreeWidgetItem *item = m_fixtures_tree->currentItem();
    const QVariant var = item != NULL ? item->data(KColumnName, PROP_ID) : QVariant();
    if (var.isValid() && Monitor::instance() != NULL)
        Monitor::instance()->editFixtureProperties(var.toUInt());
    else
        editFixtureProperties();
}

void FixtureManager::slotTestFixture()
{
    QTreeWidgetItem* item = m_fixtures_tree->currentItem();
    if (!item)
        return;
    QVariant var = item->data(KColumnName, PROP_ID);
    if (!var.isValid())
        return;
    Monitor::createAndShow(this, m_doc);
    if (Monitor::instance())
        Monitor::instance()->showFixturePropertiesById(var.toUInt());
}

void FixtureManager::slotLocateFixture()
{
    /* Every fixture in the selection, expanding any selected GROUP to its
       members -- selecting a group and asking "where is it" should light the
       group, not nothing. Deduplicated, because a group and one of its own
       fixtures can both be selected and flashing one fixture twice would
       cancel it (locateFixture toggles). */
    QList<quint32> ids;
    QSet<quint32> seen;
    foreach (QTreeWidgetItem *item, m_fixtures_tree->selectedItems())
    {
        const QVariant fxVar = item->data(KColumnName, PROP_ID);
        if (fxVar.isValid())
        {
            const quint32 fid = fxVar.toUInt();
            if (!seen.contains(fid)) { seen.insert(fid); ids << fid; }
            continue;
        }

        const QVariant grpVar = item->data(KColumnName, PROP_GROUP);
        if (grpVar.isValid())
        {
            FixtureGroup *grp = m_doc->fixtureGroup(grpVar.toUInt());
            if (grp == NULL)
                continue;
            foreach (quint32 fid, grp->fixtureList())
                if (!seen.contains(fid)) { seen.insert(fid); ids << fid; }
        }
    }

    if (ids.isEmpty())
        return;

    Monitor::createAndShow(this, m_doc);
    if (Monitor::instance())
        Monitor::instance()->locateFixtures(ids);
}

void FixtureManager::slotResetFixture()
{
    QTreeWidgetItem* item = m_fixtures_tree->currentItem();
    if (!item)
        return;
    QVariant var = item->data(KColumnName, PROP_ID);
    if (!var.isValid())
        return;
    Monitor::createAndShow(this, m_doc);
    if (Monitor::instance())
        Monitor::instance()->resetFixture(var.toUInt());
}

void FixtureManager::slotFadeConfig()
{
    ChannelsSelection cfg(m_doc, this, ChannelsSelection::ConfigurationMode);
    /* Arriving from a fixture's own context menu, open on that fixture rather
       than on the whole rig collapsed. The dialog still lists everything --
       the same modifier usually wants setting on several fixtures at once. */
    QTreeWidgetItem *item = m_fixtures_tree->currentItem();
    if (item != NULL)
    {
        const QVariant var = item->data(KColumnName, PROP_ID);
        if (var.isValid())
            cfg.expandFixture(var.toUInt());
    }
    if (cfg.exec() == QDialog::Rejected)
        return; // User pressed cancel
    m_doc->setModified();
}

void FixtureManager::slotRemap()
{
    FixtureRemap fxr(m_doc);
    if (fxr.exec() == QDialog::Rejected)
        return; // User pressed cancel

    updateView();
}

void FixtureManager::slotUnGroup()
{
    if (QMessageBox::question(this, tr("Ungroup fixtures?"),
                              tr("Do you want to ungroup the selected fixtures?"),
                              QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
    {
        return;
    }

    // Because FixtureGroup::resignFixture() emits changed(), which makes the tree
    // update its contents in the middle, invalidating m_tree->selectedItems(),
    // we must pick the list of fixtures and groups first and then resign them in
    // one big bunch.
    QList <QPair<quint32,quint32> > resignList;

    foreach (QTreeWidgetItem* item, m_fixtures_tree->selectedItems())
    {
        if (item->parent() == NULL)
            continue;

        QVariant var = item->parent()->data(KColumnName, PROP_GROUP);
        if (var.isValid() == false)
            continue;
        quint32 grp = var.toUInt();

        var = item->data(KColumnName, PROP_ID);
        if (var.isValid() == false)
            continue;
        quint32 fxi = var.toUInt();

        resignList << QPair <quint32,quint32> (grp, fxi);
    }

    QListIterator <QPair<quint32,quint32> > it(resignList);
    while (it.hasNext() == true)
    {
        QPair <quint32,quint32> pair(it.next());
        FixtureGroup* grp = m_doc->fixtureGroup(pair.first);
        Q_ASSERT(grp != NULL);
        grp->resignFixture(pair.second);
    }
}

void FixtureManager::slotGroupSelected(QAction* action)
{
    // m_groupMenu (this action's origin) only ever lists real existing
    // groups now -- "New Group..." creation moved to a right-click on the
    // Fixture Groups root/a group folder (slotContextMenuRequested()).
    Q_ASSERT(action->data().isValid());
    FixtureGroup* grp = (FixtureGroup*) (action->data().toULongLong());
    Q_ASSERT(grp != NULL);

    // Assign selected fixture items to the group
    foreach (QTreeWidgetItem* item, m_fixtures_tree->selectedItems())
    {
        QVariant var = item->data(KColumnName, PROP_ID);
        if (var.isValid() == false)
            continue;

        grp->assignFixture(var.toUInt());
    }

    updateView();
}

void FixtureManager::slotMoveGroupUp()
{
    if (m_channel_groups_tree->selectedItems().size() > 0)
    {
        QTreeWidgetItem* item = m_channel_groups_tree->selectedItems().first();
        quint32 grpID = item->data(KColumnName, PROP_ID).toUInt();
        m_doc->moveChannelGroup(grpID, -1);
        updateChannelsGroupView();
    }
}

void FixtureManager::slotMoveGroupDown()
{
    if (m_channel_groups_tree->selectedItems().size() > 0)
    {
        QTreeWidgetItem* item = m_channel_groups_tree->selectedItems().first();
        quint32 grpID = item->data(KColumnName, PROP_ID).toUInt();
        m_doc->moveChannelGroup(grpID, 1);
        updateChannelsGroupView();
    }
}

QString FixtureManager::createDialog(bool import)
{
    QString fileName;

    /* Create a file save dialog */
    QFileDialog dialog(this);
    if (import == true)
    {
        dialog.setWindowTitle(tr("Import Fixtures List"));
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
    }
    else
    {
        dialog.setWindowTitle(tr("Export Fixtures List As"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
    }

    /* Append file filters to the dialog */
    QStringList filters;
    filters << tr("Fixtures List (*%1)").arg(KExtFixtureList);
#if defined(WIN32) || defined(Q_OS_WIN)
    filters << tr("All Files (*.*)");
#else
    filters << tr("All Files (*)");
#endif
    dialog.setNameFilters(filters);

    /* Append useful URLs to the dialog */
    QList <QUrl> sidebar;
    sidebar.append(QUrl::fromLocalFile(QDir::homePath()));
    sidebar.append(QUrl::fromLocalFile(QDir::rootPath()));
    dialog.setSidebarUrls(sidebar);

    /* Get file name */
    if (dialog.exec() != QDialog::Accepted)
        return "";

    fileName = dialog.selectedFiles().first();
    if (fileName.isEmpty() == true)
        return "";

    /* Always use the fixture definition suffix */
    if (import == false && fileName.right(5) != KExtFixtureList)
        fileName += KExtFixtureList;

    return fileName;
}

void FixtureManager::slotImport()
{
    QString fileName = createDialog(true);

    QXmlStreamReader *doc = QLCFile::getXMLReader(fileName);
    if (doc == NULL || doc->device() == NULL || doc->hasError())
    {
        qWarning() << Q_FUNC_INFO << "Unable to read from" << fileName;
        return;
    }

    while (!doc->atEnd())
    {
        if (doc->readNext() == QXmlStreamReader::DTD)
            break;
    }
    if (doc->hasError())
    {
        QLCFile::releaseXMLReader(doc);
        return;
    }

    if (doc->dtdName() == KXMLQLCFixturesList)
    {
        doc->readNextStartElement();
        if (doc->name() != KXMLQLCFixturesList)
        {
            qWarning() << Q_FUNC_INFO << "Fixture Definition node not found";
            QLCFile::releaseXMLReader(doc);
            return;
        }

        while (doc->readNextStartElement())
        {
            if (doc->name() == KXMLFixture)
            {
                Fixture* fxi = new Fixture(m_doc);
                Q_ASSERT(fxi != NULL);

                if (fxi->loadXML(*doc, m_doc, m_doc->fixtureDefCache()) == true)
                {
                    if (m_doc->addFixture(fxi /*, fxi->id()*/) == true)
                    {
                        /* Success */
                        qWarning() << Q_FUNC_INFO << "Fixture" << fxi->name() << "successfully created.";
                    }
                    else
                    {
                        /* Doc is full */
                        qWarning() << Q_FUNC_INFO << "Fixture" << fxi->name() << "cannot be created.";
                        delete fxi;
                    }
                }
                else
                {
                    qWarning() << Q_FUNC_INFO << "Fixture" << fxi->name() << "cannot be loaded.";
                    delete fxi;
                }
            }
            else if (doc->name() == KXMLQLCFixtureGroup)
            {
                FixtureGroup* grp = new FixtureGroup(m_doc);
                Q_ASSERT(grp != NULL);

                if (grp->loadXML(*doc) == true)
                {
                    m_doc->addFixtureGroup(grp, grp->id());
                }
                else
                {
                    qWarning() << Q_FUNC_INFO << "FixtureGroup" << grp->name() << "cannot be loaded.";
                    delete grp;
                }
            }
            else
            {
                qWarning() << Q_FUNC_INFO << "Unknown label tag:" << doc->name().toString();
                doc->skipCurrentElement();
            }
        }
        updateView();
    }
    QLCFile::releaseXMLReader(doc);
}

void FixtureManager::slotExport()
{
    QString fileName = createDialog(false);

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly) == false)
        return;

    QXmlStreamWriter doc(&file);
    doc.setAutoFormatting(true);
    doc.setAutoFormattingIndent(1);
    QLCFile::writeXMLHeader(&doc, KXMLQLCFixturesList);

    QListIterator <Fixture*> fxit(m_doc->fixtures());
    while (fxit.hasNext() == true)
    {
        Fixture* fxi(fxit.next());
        Q_ASSERT(fxi != NULL);
        fxi->saveXML(&doc);
    }

    QListIterator <FixtureGroup*>grpit(m_doc->fixtureGroups());
    while (grpit.hasNext() == true)
    {
        FixtureGroup *fxgrp(grpit.next());
        Q_ASSERT(fxgrp != NULL);
        fxgrp->saveXML(&doc);
    }

    doc.writeEndDocument();
    file.close();
}

void FixtureManager::copySelectionIntoGroup(FixtureGroup* target,
                                            const QList<quint32>& fixtureIds,
                                            const QList<quint32>& groupIds)
{
    if (target == NULL)
        return;

    // Silence the per-head changed() storm; one refresh happens at the end.
    target->blockSignals(true);

    // Start appending below whatever the target already holds.
    int curY = 0;
    {
        QMapIterator<QLCPoint, GroupHead> it(target->headsMap());
        while (it.hasNext()) { it.next(); curY = qMax(curY, it.key().y() + 1); }
    }

    // Each source group goes in as a block (preserving its relative layout),
    // tagged as a sub-group so it stays visually distinct and movable as a unit.
    foreach (quint32 gid, groupIds)
    {
        FixtureGroup* src = m_doc->fixtureGroup(gid);
        if (src == NULL || src == target)
            continue;

        const QMap<QLCPoint, GroupHead> srcHeads = src->headsMap();
        if (srcHeads.isEmpty())
            continue;

        // Normalise the source to its top-left corner.
        int minX = INT_MAX, minY = INT_MAX;
        QMapIterator<QLCPoint, GroupHead> nit(srcHeads);
        while (nit.hasNext()) { nit.next(); minX = qMin(minX, nit.key().x()); minY = qMin(minY, nit.key().y()); }

        int blockMaxY = curY;
        QMapIterator<QLCPoint, GroupHead> it(srcHeads);
        while (it.hasNext())
        {
            it.next();
            const int x = it.key().x() - minX;
            const int y = curY + (it.key().y() - minY);
            if (target->assignHead(QLCPoint(x, y), it.value()) == true)
            {
                target->setHeadSubGroup(QLCPoint(x, y), FixtureGroup::subGroupTagFor(src->id()));
                blockMaxY = qMax(blockMaxY, y);
            }
        }
        curY = blockMaxY + 1;
    }

    // Loose fixtures go in a row of their own (all heads of each fixture).
    int col = 0;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture* fxi = m_doc->fixture(fid);
        if (fxi == NULL)
            continue;
        for (int h = 0; h < fxi->heads(); h++)
        {
            if (target->assignHead(QLCPoint(col, curY), GroupHead(fid, h)) == true)
                col++;
        }
    }

    // Grow the grid to fit everything we placed.
    int maxX = 0, maxY = 0;
    QMapIterator<QLCPoint, GroupHead> sz(target->headsMap());
    while (sz.hasNext()) { sz.next(); maxX = qMax(maxX, sz.key().x()); maxY = qMax(maxY, sz.key().y()); }
    if (maxX + 1 > target->size().width() || maxY + 1 > target->size().height())
        target->setSize(QSize(qMax(maxX + 1, target->size().width()),
                              qMax(maxY + 1, target->size().height())));

    target->blockSignals(false);
    target->notifyChanged(); // single refresh
    m_doc->setModified();
}

void FixtureManager::slotContextMenuRequested(const QPoint &pos)
{
    // Which universe was the click in? A universe row carries PROP_UNIVERSE; a
    // fixture row inherits its fixture's universe. "Add fixture" (fired from the
    // menu below) reads m_ctxUniverse to default the dialog to that universe.
    // ctxIsUniverseRow distinguishes the direct hit (offer "Delete universe…")
    // from a fixture merely living in that universe (don't).
    m_ctxUniverse = InputOutputMap::invalidUniverse();
    bool ctxIsUniverseRow = false;
    // A right-click on the Inventory root is a request to work on the inventory:
    // the generic "Add fixture..." (which patches) would be the wrong offer there.
    bool ctxIsInventoryRoot = false;
    if (QTreeWidgetItem *clicked = m_fixtures_tree->itemAt(pos))
    {
        ctxIsInventoryRoot = clicked->data(KColumnName, PROP_INVENTORY).isValid();
        const QVariant uniVar = clicked->data(KColumnName, PROP_UNIVERSE);
        if (uniVar.isValid())
        {
            m_ctxUniverse = uniVar.toUInt();
            ctxIsUniverseRow = true;
        }
        else
        {
            const QVariant fv = clicked->data(KColumnName, PROP_ID);
            if (fv.isValid() && clicked->data(KColumnName, PROP_HEAD).isValid() == false)
            {
                Fixture *fxi = m_doc->fixture(fv.toUInt());
                if (fxi != NULL && fxi->isPatched())
                    m_ctxUniverse = fxi->universe();
            }
        }
    }

    // Was the click anywhere in the Power section? -2 = no; -1 = the "Power"
    // root itself (only "Add power source…" applies); >= 0 = a specific
    // source's index (a direct-source node, a multi-circuit source folder, or
    // one of its circuits — "Add circuit…" adds to that source either way).
    int ctxPowerSource = -2;
    if (QTreeWidgetItem *clicked = m_fixtures_tree->itemAt(pos))
    {
        const QVariant srcVar = clicked->data(KColumnName, PROP_SOURCE);
        if (srcVar.isValid())
            ctxPowerSource = srcVar.toInt();
    }

    QMenu menu(this);

    // Collect the selection so we can offer to move groups to a folder, and to
    // copy whole fixtures and/or whole fixture groups into a new/existing group.
    QList<quint32> selFixtures;
    QList<quint32> selGroups;
    foreach (QTreeWidgetItem *item, m_fixtures_tree->selectedItems())
    {
        const QVariant gv = item->data(KColumnName, PROP_GROUP);
        if (gv.isValid())
        {
            const quint32 gid = gv.toUInt();
            if (selGroups.contains(gid) == false)
                selGroups.append(gid);
            continue;
        }
        // A fixture row (not a head row, which carries PROP_HEAD).
        const QVariant fv = item->data(KColumnName, PROP_ID);
        if (fv.isValid() && item->data(KColumnName, PROP_HEAD).isValid() == false)
        {
            const quint32 fid = fv.toUInt();
            if (selFixtures.contains(fid) == false)
                selFixtures.append(fid);
        }
    }

    /* Laid out by what it is about, the same order whatever is selected:
       the fixture itself, its address, finding it on the rig, organising it,
       deleting it -- then what the clicked row offers (a root, a folder, the
       inventory), then the view. Grouping and power are submenus: each is a
       handful of related moves, and flat they were half the menu. */
    const bool haveFixtureOrGroupSelection = selFixtures.isEmpty() == false
                                           || selGroups.isEmpty() == false;

    // ---- The fixture itself ----
    QAction *editDefinition = NULL;
    if (haveFixtureOrGroupSelection)
    {
        menu.addAction(m_propertiesAction);
        // Its definition and mode: the stock dialog, kept for what Fixture
        // Properties does not change.
        if (selFixtures.size() == 1 && m_doc->mode() != Doc::Operate)
        {
            const quint32 fid = selFixtures.first();
            menu.addAction(tr("Definition && Mode…"), this, [this, fid]() {
                editDefinitionAndMode(this, m_doc, fid);
            });
        }
        /* The definition file itself, in the Fixture Editor -- one real
           fixture with a .qxf; a generic dimmer has none. */
        if (selFixtures.size() == 1 && selGroups.isEmpty())
        {
            Fixture *fxi = m_doc->fixture(selFixtures.first());
            if (fxi != NULL && fxi->fixtureDef() != NULL &&
                fxi->fixtureDef()->definitionSourceFile().isEmpty() == false)
                editDefinition = menu.addAction(tr("Edit Fixture Definition…"));
        }
    }

    // Inventory: staged fixtures (no universe/address yet). The root offers
    // adding one and patching them all; a selection of staged fixtures offers
    // patching just those.
    QAction *addToInv = NULL;
    QAction *patchAll = NULL;
    QList<quint32> stagedSel;
    foreach (quint32 fid, selFixtures)
    {
        Fixture *sf = m_doc->fixture(fid);
        if (sf != NULL && !sf->isPatched())
            stagedSel << fid;
    }
    // A selected manufacturer/model folder stands for every staged fixture in it.
    std::function<void(QTreeWidgetItem *)> collectStaged = [&](QTreeWidgetItem *node) {
        for (int c = 0; c < node->childCount(); c++)
        {
            QTreeWidgetItem *ch = node->child(c);
            const QVariant idv = ch->data(KColumnName, PROP_ID);
            if (idv.isValid() && !ch->data(KColumnName, PROP_HEAD).isValid())
            {
                const quint32 fid = idv.toString().toUInt();
                Fixture *sf = m_doc->fixture(fid);
                if (sf != NULL && !sf->isPatched() && !stagedSel.contains(fid))
                    stagedSel << fid;
            }
            else
                collectStaged(ch);
        }
    };
    foreach (QTreeWidgetItem *sel, m_fixtures_tree->selectedItems())
        if (sel->data(KColumnName, PROP_INVENTORY).toInt() == 2)
            collectStaged(sel);
    // The other direction: patched fixtures can go back to the inventory.
    QAction *returnToInv = NULL;
    QList<quint32> patchedSel;
    foreach (quint32 fid, selFixtures)
    {
        Fixture *pf = m_doc->fixture(fid);
        if (pf != NULL && pf->isPatched())
            patchedSel << fid;
    }

    // ---- Its address ----
    /* Patch: one action, one name, one rule (AppUtil::patchActionText(),
       patchAllowed()) -- giving staged fixtures an address and moving
       patched ones are the same dialog, on the whole selection, so a mixed
       set is placed as one block. */
    QAction *readdress = NULL;
    QList<quint32> readdressIds = patchedSel;
    foreach (quint32 fid, stagedSel)
        if (!readdressIds.contains(fid))
            readdressIds << fid;
    if (!readdressIds.isEmpty())
    {
        menu.addSeparator();
        readdress = menu.addAction(AppUtil::patchActionText(readdressIds.size()));
        readdress->setEnabled(AppUtil::patchAllowed(m_doc, readdressIds));
        readdress->setToolTip(tr("Give a universe and address, or move to another -- as a block "
                                 "keeping their spacing, or packed -- reviewed before it is applied"));
    }
    if (!patchedSel.isEmpty())
    {
        returnToInv = menu.addAction(patchedSel.size() > 1
                                         ? tr("Return %1 Fixtures to Inventory…").arg(patchedSel.size())
                                         : tr("Return to Inventory…"));
        returnToInv->setEnabled(m_doc->mode() == Doc::Design);
        returnToInv->setToolTip(tr("Release its universe and address but keep the fixture, its "
                                   "placement and its groups"));
    }

    // ---- Finding it on the rig ----
    if (haveFixtureOrGroupSelection)
    {
        menu.addSeparator();
        menu.addAction(m_testAction);
        menu.addAction(m_locateAction);
        menu.addAction(m_resetAction);
    }

    // ---- Organising it ----
    QAction *moveToFolder = NULL;
    QAction *ungroupOnly = NULL;
    QAction *copyToNew = NULL;
    QMenu *copyIntoMenu = NULL;
    QAction *rebuildComposite = NULL;
    QMenu *circuitMenu = NULL;
    QAction *removeFromCircuit = NULL;
    if (haveFixtureOrGroupSelection)
    {
        menu.addSeparator();
        QMenu *groupMenu = menu.addMenu(QIcon(":/group.png"), tr("Group"));
        copyToNew = groupMenu->addAction(tr("Create Group from Selection…"));
        const QList<FixtureGroup*> groups = m_doc->fixtureGroups();
        if (groups.isEmpty() == false)
        {
            // Fixtures join it; a group selected comes in as a block.
            copyIntoMenu = groupMenu->addMenu(tr("Add to Group"));
            foreach (FixtureGroup *g, groups)
            {
                if (g == NULL)
                    continue;
                QAction *a = copyIntoMenu->addAction(g->name());
                a->setData(g->id());
            }
        }
        if (selFixtures.isEmpty() == false)
            groupMenu->addAction(m_unGroupAction);
        if (selGroups.isEmpty() == false)
        {
            groupMenu->addSeparator();
            moveToFolder = groupMenu->addAction(selGroups.size() > 1
                ? tr("Move %1 Groups to Folder…").arg(selGroups.size())
                : tr("Move to Folder…"));
            /* Delete the GROUPING, keep the fixtures -- the shared Delete
               also deletes any fixture rows caught in the selection. */
            ungroupOnly = groupMenu->addAction(selGroups.size() > 1
                ? tr("Remove %1 Groups (Keep Fixtures)").arg(selGroups.size())
                : tr("Remove Group (Keep Fixtures)"));
            ungroupOnly->setToolTip(tr("Deletes the grouping only — every fixture in "
                                       "it stays in the workspace, patched as it is."));
            // A composite group rebuilt from the source groups its heads recorded.
            if (selGroups.size() == 1)
            {
                FixtureGroup *g = m_doc->fixtureGroup(selGroups.first());
                if (g != NULL && g->headSubGroupMap().isEmpty() == false)
                    rebuildComposite = groupMenu->addAction(tr("Rebuild Composite from Members"));
            }
        }

        // Power circuits live in the power-distribution pane on the right.
        if (selFixtures.isEmpty() == false)
        {
            QMenu *powerMenu = menu.addMenu(tr("Power"));
            PowerDistribution *pd = m_doc->powerDistribution();
            circuitMenu = powerMenu->addMenu(selFixtures.size() > 1
                ? tr("Add %1 Fixtures to Circuit").arg(selFixtures.size())
                : tr("Add to Circuit"));
            bool haveCircuit = false;
            for (int s = 0; s < pd->sources().size(); s++)
            {
                const PowerSource &src = pd->sources().at(s);
                if (src.circuits.size() <= 1)
                {
                    // A wall socket, single-outlet UPS, or fresh source takes
                    // fixtures directly (its single/implicit circuit).
                    haveCircuit = true;
                    QAction *a = circuitMenu->addAction(tr("%1 (direct)").arg(src.name));
                    a->setData(QPoint(s, 0));
                    continue;
                }
                for (int c = 0; c < src.circuits.size(); c++)
                {
                    haveCircuit = true;
                    QAction *a = circuitMenu->addAction(tr("%1 / %2")
                            .arg(src.name).arg(src.circuits.at(c).name));
                    a->setData(QPoint(s, c));   // (source, circuit) indices
                }
            }
            if (haveCircuit == false)
            {
                QAction *none = circuitMenu->addAction(
                    tr("(no circuits yet — add one in the Power pane)"));
                none->setEnabled(false);
            }
            removeFromCircuit = powerMenu->addAction(tr("Remove from Circuit"));
        }
        menu.addAction(m_fadeConfigAction);

        // ---- Deleting it ----
        menu.addSeparator();
        menu.addAction(m_removeAction);
    }

    // ---- What the clicked row offers ----
    /* Creating something new is a root-ish click's (empty space, a universe,
       the Power or Fixture Groups roots, a folder), never a fixture's. */
    if (!haveFixtureOrGroupSelection && !ctxIsInventoryRoot)
    {
        menu.addSeparator();
        menu.addAction(m_addAction);
        menu.addAction(m_addRGBAction);
    }

    // A universe row itself: where it goes (patch, change, unpatch -- the
    // Connections tab's questions, through UniversePatcher), and deleting
    // it. Renaming is inline.
    QAction *deleteUni = NULL;
    QAction *patchOut = NULL;
    QAction *retargetOut = NULL;
    QAction *unpatchOut = NULL;
    QAction *pluralPatch = NULL;
    QAction *pluralUnpatch = NULL;
    QAction *showInConnections = NULL;
    const quint32 ctxUni = m_ctxUniverse;
    QString ctxPlugin;
    quint32 ctxLine = 0;
    QList<quint32> pickedUnis;
    QList<UniversePatcher::UnpatchLeg> pickedLegs;
    if (ctxIsUniverseRow && m_patcher != NULL)
    {
        menu.addSeparator();

        /* Several universe rows selected: the plural actions, named with
           their count, first -- the same two the Connections tab offers,
           in the order the universes were picked. */
        pickedUnis = orderedSelectedUniverses();
        if (pickedUnis.count() > 1 && pickedUnis.contains(ctxUni))
        {
            pluralPatch = menu.addAction(UniversePatcher::patchSelectionText(pickedUnis.count()));
            pickedLegs = m_patcher->outputLegs(pickedUnis);
            QSet<quint32> withPatch;
            foreach (const UniversePatcher::UnpatchLeg &l, pickedLegs)
                withPatch.insert(l.universe);
            if (withPatch.isEmpty() == false)
                pluralUnpatch = menu.addAction(
                    UniversePatcher::unpatchSelectionText(withPatch.count()));
            menu.addSeparator();
        }

        /* This one universe: where it goes now, said in the menu itself,
           then what can be done about it. */
        InputOutputMap *iomap = m_doc->inputOutputMap();
        Universe *u = iomap != NULL ? iomap->universe(ctxUni) : NULL;
        for (int i = 0; u != NULL && i < u->outputPatchesCount() && ctxPlugin.isEmpty(); i++)
        {
            OutputPatch *op = u->outputPatch(i);
            if (op != NULL && op->plugin() != NULL && op->isPending() == false)
            {
                ctxPlugin = op->plugin()->name();
                ctxLine = op->output();
            }
        }
        const QStringList legs = m_patcher->outputText(ctxUni);
        if (legs.isEmpty())
        {
            patchOut = menu.addAction(tr("Patch output…"));
            patchOut->setToolTip(tr("Choose the protocol, interface and node port this "
                                    "universe goes out on"));
        }
        else
        {
            QAction *where = menu.addAction(tr("Output: %1").arg(legs.join(QStringLiteral(" · "))));
            where->setEnabled(false);
            if (ctxPlugin.isEmpty() == false && m_patcher->supportsTargets(ctxPlugin))
                retargetOut = menu.addAction(tr("Change interface / target…"));
            else
                patchOut = menu.addAction(tr("Patch output…"));
            unpatchOut = menu.addAction(tr("Unpatch output"));
        }
        if (ShowStatus::instance()->actionLabel(QStringLiteral("connections.universe")).isEmpty() == false)
            showInConnections = menu.addAction(tr("Show in Connections"));

        menu.addSeparator();
        deleteUni = menu.addAction(tr("Delete Universe…"));
    }

    // The Power section: a new source from its root, a circuit on a source.
    QAction *addPowerSource = NULL;
    QAction *addCircuit = NULL;
    if (ctxPowerSource != -2)
    {
        menu.addSeparator();
        addPowerSource = menu.addAction(tr("Add Power Source…"));
        if (ctxPowerSource >= 0)
            addCircuit = menu.addAction(tr("Add Circuit…"));
    }

    // "New Group..." on the Fixture Groups root or one of its folders.
    QString newGroupPath;
    bool offerNewGroup = false;
    if (QTreeWidgetItem *clickedItem = m_fixtures_tree->itemAt(pos))
    {
        const QVariant folderVar = clickedItem->data(KColumnName, PROP_FOLDER);
        if (folderVar.isValid())
        {
            offerNewGroup = true;
            newGroupPath = folderVar.toString();
        }
    }
    if (offerNewGroup)
    {
        menu.addSeparator();
        menu.addAction(m_newGroupAction);
    }

    // The Duplicate names root: number them apart.
    QAction *fixDuplicates = NULL;
    if (QTreeWidgetItem *clicked = m_fixtures_tree->itemAt(pos))
    {
        if (clicked->data(KColumnName, PROP_DUPLICATES).isValid()
            && !m_doc->duplicateFixtureNames().isEmpty())
        {
            menu.addSeparator();
            fixDuplicates = menu.addAction(tr("Number the Duplicates"));
            fixDuplicates->setToolTip(tr("Of each name, the patched fixture keeps it; the others "
                                         "become “name #2”, “name #3”…"));
        }
    }

    // The Address clashes root: give every one of them a home.
    QAction *fixClashes = NULL;
    if (QTreeWidgetItem *clicked = m_fixtures_tree->itemAt(pos))
    {
        if (clicked->data(KColumnName, PROP_CLASHES).isValid() && !m_doc->addressClashes().isEmpty())
        {
            menu.addSeparator();
            fixClashes = menu.addAction(tr("Patch the Clashing Fixtures…"));
            fixClashes->setToolTip(tr("Propose free channels for each one, to review and apply"));
        }
    }

    /* Right-clicked a model's folder (or one of its fixtures) in the
       Inventory: adding starts on that model and mode ("when I right click
       on a model .. it should auto select that model from the list for
       adding"); a manufacturer's folder, on that manufacturer. */
    quint32 likeFixture = Fixture::invalidId();
    QString likeManufacturer;
    QTreeWidgetItem *invHit = m_fixtures_tree->itemAt(pos);
    if (invHit != NULL && invHit->data(KColumnName, PROP_INVENTORY).toInt() == 2)
    {
        // A model folder holds fixtures; a manufacturer folder holds models.
        std::function<quint32(QTreeWidgetItem *)> firstFixture = [&](QTreeWidgetItem *n) -> quint32 {
            for (int c = 0; c < n->childCount(); ++c)
            {
                QTreeWidgetItem *ch = n->child(c);
                if (ch->data(KColumnName, PROP_ID).isValid() && !ch->data(KColumnName, PROP_HEAD).isValid())
                    return ch->data(KColumnName, PROP_ID).toString().toUInt();
                const quint32 deeper = firstFixture(ch);
                if (deeper != Fixture::invalidId())
                    return deeper;
            }
            return Fixture::invalidId();
        };
        bool modelFolder = false;
        for (int c = 0; c < invHit->childCount() && !modelFolder; ++c)
            modelFolder = invHit->child(c)->data(KColumnName, PROP_ID).isValid();
        const quint32 any = firstFixture(invHit);
        Fixture *af = m_doc->fixture(any);
        if (modelFolder)
            likeFixture = any;
        else if (af != NULL && af->fixtureDef() != NULL)
            likeManufacturer = af->fixtureDef()->manufacturer();
    }
    else if (invHit != NULL && stagedSel.size() == 1 && selFixtures.size() == 1)
    {
        likeFixture = stagedSel.first();   // a staged fixture: more like it
    }
    Fixture *likeFx = m_doc->fixture(likeFixture);
    QString addLabel = tr("Add to Inventory…");
    if (likeFx != NULL)
        addLabel = tr("Add More %1 to Inventory…").arg(likeFx->groupingModel());   // as its folder reads
    else if (!likeManufacturer.isEmpty())
        addLabel = tr("Add %1 Fixtures to Inventory…").arg(likeManufacturer);

    if (ctxIsInventoryRoot || likeFx != NULL)
    {
        menu.addSeparator();
        addToInv = menu.addAction(addLabel);
        if (ctxIsInventoryRoot)
            patchAll = menu.addAction(tr("Patch Inventory…"));
    }

    // ---- The view ----
    menu.addSeparator();
    menu.addAction(m_expandAllAction);
    menu.addAction(m_collapseAllAction);

    QAction *chosen = menu.exec(QCursor::pos());

    // addFixture() (fired from m_addAction during exec above) has already
    // consumed the context; clear it so a later toolbar Add starts from the
    // global default.
    m_ctxUniverse = InputOutputMap::invalidUniverse();

    if (chosen == NULL)
        return;

    if (patchOut != NULL && chosen == patchOut)
    {
        m_patcher->patchUniverse(ctxUni);
        return;
    }
    if (retargetOut != NULL && chosen == retargetOut)
    {
        m_patcher->retargetPatch(ctxUni, ctxPlugin, ctxLine);
        return;
    }
    if (unpatchOut != NULL && chosen == unpatchOut)
    {
        m_patcher->unpatchLegs(m_patcher->outputLegs(QList<quint32>() << ctxUni));
        return;
    }
    if (pluralPatch != NULL && chosen == pluralPatch)
    {
        m_patcher->patchSelectionTo(pickedUnis);
        return;
    }
    if (pluralUnpatch != NULL && chosen == pluralUnpatch)
    {
        m_patcher->unpatchLegs(pickedLegs);
        return;
    }
    if (showInConnections != NULL && chosen == showInConnections)
    {
        ShowStatus::instance()->runAction(QStringLiteral("connections.universe"), this,
                                          QVariantList() << ctxUni);
        return;
    }
    if (readdress != NULL && chosen == readdress)
    {
        patchOrReaddress(readdressIds);
        return;
    }
    if (fixDuplicates != NULL && chosen == fixDuplicates)
    {
        m_doc->makeFixtureNamesUnique();
        updateView();
        return;
    }
    if (fixClashes != NULL && chosen == fixClashes)
    {
        patchOrReaddress(m_doc->addressClashes());
        return;
    }
    if (returnToInv != NULL && chosen == returnToInv)
    {
        returnToInventory(patchedSel);
        return;
    }
    if (addToInv != NULL && chosen == addToInv)
    {
        addFixtureImpl(true, likeFixture, likeManufacturer);
        return;
    }
    if (patchAll != NULL && chosen == patchAll)
    {
        patchInventory(QList<quint32>());
        return;
    }

    if (offerNewGroup && chosen == m_newGroupAction)
    {
        // Suggest an equilateral grid, same heuristic slotGroupSelected() used
        // for a selection-driven "New Group..." -- no selection is implied
        // here (this is a root/folder click, not a fixture one), so this
        // always takes the empty-group default (a small grid to drop into).
        CreateFixtureGroup cfg(this);
        cfg.setSize(QSize(4, 4));
        if (cfg.exec() == QDialog::Accepted)
        {
            FixtureGroup *grp = new FixtureGroup(m_doc);
            grp->setName(cfg.name());
            grp->setSize(cfg.size());
            grp->setPath(newGroupPath);
            m_doc->addFixtureGroup(grp);
            updateGroupMenu();
            updateView();
        }
        return;
    }

    if (editDefinition != NULL && chosen == editDefinition)
    {
        Fixture *fxi = m_doc->fixture(selFixtures.first());
        if (fxi != NULL && fxi->fixtureDef() != NULL)
            AppUtil::launchFixtureEditor(fxi->fixtureDef()->definitionSourceFile(), this);
        return;
    }

    if (rebuildComposite != NULL && chosen == rebuildComposite)
    {
        const int blocks = m_doc->programmer()->rebuildCompositeGroup(selGroups.first());
        if (blocks > 0)
            updateView();
        return;
    }

    if (chosen == ungroupOnly && selGroups.isEmpty() == false)
    {
        /* Name them: "Remove 3 groups" is not enough to check against when the
           tree selection is not entirely obvious. */
        QStringList gnames;
        foreach (quint32 gid, selGroups)
            if (FixtureGroup *g = m_doc->fixtureGroup(gid))
                gnames << g->name();
        if (QMessageBox::question(this, tr("Remove Group"),
                tr("Remove %n group(s) — %1 — keeping every fixture in them?",
                   "", selGroups.size()).arg(gnames.join(", ")),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;

        foreach (quint32 gid, selGroups)
            m_doc->deleteFixtureGroup(gid);
        updateView();
        return;
    }

    if (chosen == moveToFolder && selGroups.isEmpty() == false)
    {
        FixtureGroup *first = m_doc->fixtureGroup(selGroups.first());
        const QString cur = (first != NULL) ? first->path() : QString();
        bool ok = false;
        const QString path = QInputDialog::getText(
            this, tr("Move groups to folder"),
            tr("Folder path (e.g. \"Movers/Front\"; empty for none):"),
            QLineEdit::Normal, cur, &ok);
        if (ok)
        {
            const QString p = path.trimmed();
            foreach (quint32 gid, selGroups)
            {
                FixtureGroup *g = m_doc->fixtureGroup(gid);
                if (g != NULL)
                    g->setPath(p);
            }
            m_doc->setModified();
            updateView();
        }
    }
    else if (chosen == copyToNew)
    {
        if (selGroups.isEmpty() && !selFixtures.isEmpty())
        {
            // Loose fixtures → the shared creator (grid heuristic + name/size
            // prompt + physical-order arrangement), consistent with the
            // Programming tab and Studio.
            if (CreateFixtureGroup::createFromFixtures(m_doc, selFixtures, this) != NULL)
                updateView();
        }
        else
        {
            // Selection includes existing groups → keep the block-copy behaviour
            // (each source group placed as a sub-group block, grown to fit).
            bool ok = false;
            const QString name = QInputDialog::getText(
                this, tr("Create group from selection"), tr("New group name:"),
                QLineEdit::Normal, tr("New Group"), &ok);
            if (ok)
            {
                FixtureGroup *ng = new FixtureGroup(m_doc);
                ng->setName(name.trimmed().isEmpty() ? tr("New Group") : name.trimmed());
                ng->setSize(QSize(1, 1)); // grown to fit by copySelectionIntoGroup
                m_doc->addFixtureGroup(ng);
                copySelectionIntoGroup(ng, selFixtures, selGroups);
                updateView();
            }
        }
    }
    else if (copyIntoMenu != NULL && copyIntoMenu->actions().contains(chosen))
    {
        FixtureGroup *target = m_doc->fixtureGroup(chosen->data().toUInt());
        if (target != NULL)
        {
            copySelectionIntoGroup(target, selFixtures, selGroups);
            updateView();
        }
    }
    else if (circuitMenu != NULL && circuitMenu->actions().contains(chosen))
    {
        const QPoint sc = chosen->data().toPoint();
        PowerDistribution *pd = m_doc->powerDistribution();
        foreach (quint32 fid, selFixtures)
            pd->assignFixture(fid, sc.x(), sc.y());
        m_doc->setModified();
        if (m_power != NULL)
            m_power->refresh();
    }
    else if (chosen == removeFromCircuit)
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        foreach (quint32 fid, selFixtures)
            pd->unassignFixture(fid);
        m_doc->setModified();
        if (m_power != NULL)
            m_power->refresh();
    }
    else if (chosen == deleteUni)
    {
        // m_ctxUniverse is cleared as soon as the menu closes (above).
        deleteUniverse(ctxUni);
    }
    else if (chosen == addPowerSource)
    {
        // Same defaults slotAddSource() uses in the Power pane itself.
        QSettings settings;
        PowerDistribution *pd = m_doc->powerDistribution();
        PowerSource src;
        src.name = tr("Source %1").arg(pd->sources().size() + 1);
        src.voltage = settings.value(QStringLiteral("power/defaultVoltage"), 120.0).toDouble();
        pd->sources().append(src);
        m_doc->setModified();
        updateView();
        if (m_power != NULL)
            m_power->refresh();
    }
    else if (chosen == addCircuit && ctxPowerSource >= 0)
    {
        QSettings settings;
        PowerDistribution *pd = m_doc->powerDistribution();
        if (ctxPowerSource < pd->sources().size())
        {
            PowerCircuit cir;
            PowerSource &src = pd->sources()[ctxPowerSource];
            cir.name = tr("Circuit %1").arg(src.circuits.size() + 1);
            cir.ratedAmps = settings.value(QStringLiteral("power/defaultBreakerAmps"), 20.0).toDouble();
            cir.deratePercent = settings.value(QStringLiteral("power/deratePercent"), 80).toInt();
            src.circuits.append(cir);
            m_doc->setModified();
            updateView();
            if (m_power != NULL)
                m_power->refresh();
        }
    }
}

/** Mirrors ConnectionsTree::deleteUniverse() -- same rule, same messages,
 *  since this is the same operation reached from a second tree. Not shared
 *  code because the two classes don't share a base to hang it off of; keep
 *  them in sync by hand if the rule ever changes. */
void FixtureManager::deleteUniverse(quint32 universe)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *uni = iomap ? iomap->universe(universe) : NULL;
    if (uni == NULL)
        return;

    // Only the LAST universe can go: removeUniverse() refuses anything else
    // rather than leave a gap in the numbering, which the whole patch API
    // depends on. Say so BEFORE asking, rather than asking a question that
    // cannot be honoured.
    if (universe + 1 != iomap->universesCount())
    {
        QMessageBox::information(this, tr("Delete universe"),
            tr("Only the last universe can be deleted — removing one from the "
               "middle would renumber the others and repoint every patch after "
               "it.\n\nDelete universe %1 first, and work back.")
                .arg(iomap->universesCount()));
        return;
    }

    // Deleting a universe orphans every fixture patched into it -- they keep
    // their universe id and simply lose output -- so this IS undoable, and
    // the warning says which part is recoverable.
    if (QMessageBox::question(
            this, tr("Delete universe"),
            tr("Delete \"%1\" completely?\n\nAny fixtures patched into it will "
               "lose their output until it is restored.").arg(uni->name()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    if (iomap->patchUndo() != NULL)
        iomap->patchUndo()->captureUniverses(tr("delete universe \"%1\"")
                                                 .arg(uni->name()));

    if (iomap->removeUniverse(int(universe)) == false)
    {
        // Guarded above, so reaching here means the rule changed underneath
        // us. Do not leave a spent undo step and a modified flag behind for
        // a deletion that did not happen.
        if (iomap->patchUndo() != NULL)
            iomap->patchUndo()->clear();
        QMessageBox::warning(this, tr("Delete universe"),
                             tr("\"%1\" could not be deleted.").arg(uni->name()));
        return;
    }

    m_doc->setModified();
    updateView();
}

void FixtureManager::syncUniversePickOrder()
{
    if (m_fixtures_tree == NULL)
        return;
    /* What is selected now, in list order; then the picking order of what is
       still selected, with what is new appended -- one ctrl-click adds one, a
       shift-click range several in list order, a plain click starts again. */
    QList<quint32> now;
    for (QTreeWidgetItemIterator it(m_fixtures_tree, QTreeWidgetItemIterator::Selected); *it; ++it)
    {
        const QVariant uv = (*it)->data(KColumnName, PROP_UNIVERSE);
        if (uv.isValid() && now.contains(uv.toUInt()) == false)
            now << uv.toUInt();
    }
    QList<quint32> order;
    foreach (quint32 u, m_universePickOrder)
        if (now.contains(u))
            order << u;
    foreach (quint32 u, now)
        if (order.contains(u) == false)
            order << u;
    m_universePickOrder = order;
}

QList<quint32> FixtureManager::orderedSelectedUniverses()
{
    syncUniversePickOrder();
    return m_universePickOrder;
}

void FixtureManager::slotChannelGroupContextMenuRequested(const QPoint &pos)
{
    Q_UNUSED(pos);                  // the menu opens at the cursor
    QMenu menu(this);

    // Creation is always offered, same as the fixtures tree's Add actions.
    menu.addAction(m_addChannelsGroupAction);

    // Properties/Move Up/Move Down only make sense for a single selected
    // group; Remove works for one or several. Mirrors the enable-state
    // logic slotChannelsGroupSelectionChanged() already maintains for these
    // same actions' toolbar-button days — read fresh here since a menu
    // should omit what doesn't apply, not just gray it out (matching the
    // fixtures tree context menu's convention).
    const int selectedCount = m_channel_groups_tree->selectedItems().size();
    if (selectedCount > 0)
    {
        menu.addSeparator();
        if (selectedCount == 1)
        {
            menu.addAction(m_propertiesAction);
            menu.addAction(m_moveUpAction);
            menu.addAction(m_moveDownAction);
        }
        menu.addAction(m_removeAction);
    }

    menu.exec(QCursor::pos());
}

void FixtureManager::slotGroupFolderRenamed(const QString& oldPath,
                                            const QString& newLeaf)
{
    // Compute new path: keep all parent segments, replace the last one.
    const int slash = oldPath.lastIndexOf('/');
    const QString parentPrefix = (slash < 0) ? QString() : oldPath.left(slash + 1);
    const QString newPath = parentPrefix + newLeaf;

    bool changed = false;
    foreach (FixtureGroup *g, m_doc->fixtureGroups())
    {
        if (g == NULL)
            continue;
        const QString gp = g->path();
        // Match exact folder or any descendant (e.g. "Stage/Left/Sub").
        if (gp == oldPath || gp.startsWith(oldPath + "/"))
        {
            g->setPath(newPath + gp.mid(oldPath.length()));
            changed = true;
        }
    }
    if (changed)
    {
        m_doc->setModified();
        updateView();
    }
}

void FixtureManager::slotGroupsDroppedOnFolder(const QList<quint32>& groupIds,
                                               const QString& destPath)
{
    // Defer: this runs from the tree's dropEvent; setPath()+updateView() would
    // rebuild (delete) the tree items while Qt's drag machinery still uses the
    // dragged item -> use-after-free. Apply once the drop has fully returned.
    QTimer::singleShot(0, this, [this, groupIds, destPath]() {
        bool changed = false;
        foreach (quint32 id, groupIds)
        {
            FixtureGroup *g = m_doc->fixtureGroup(id);
            if (g != NULL && g->path() != destPath)
            {
                g->setPath(destPath);
                changed = true;
            }
        }
        if (changed)
        {
            m_doc->setModified();
            updateView();
        }
    });
}

void FixtureManager::slotFixturesDroppedOnCircuit(const QList<quint32>& fixtureIds,
                                                  int sourceIdx, int circuitIdx)
{
    // Deferred for the same reason as slotGroupsDroppedOnFolder(): this runs
    // from the tree's dropEvent, and updateView() rebuilds (deletes) the tree
    // items while Qt's drag machinery still references the dragged item.
    QTimer::singleShot(0, this, [this, fixtureIds, sourceIdx, circuitIdx]() {
        PowerDistribution *pd = m_doc->powerDistribution();
        foreach (quint32 fid, fixtureIds)
            pd->assignFixture(fid, sourceIdx, circuitIdx);
        m_doc->setModified();
        if (m_power != NULL)
            m_power->refresh();
        updateView();
    });
}

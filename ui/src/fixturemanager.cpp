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
#include "fixturemanager.h"
#include "fixtureremap.h"
#include "addrgbpanel.h"
#include "addfixture.h"
#include "rdmmanager.h"
#include "universe.h"
#include "fixture.h"
#include "apputil.h"
#include "doc.h"
#include "programmercontroller.h"
#include "monitor/monitor.h"

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
    , m_splitter(NULL)
    , m_fixtures_tree(NULL)
    , m_channel_groups_tree(NULL)
    , m_rdmManager(NULL)
    , m_info(NULL)
    , m_groupEditor(NULL)
    , m_groupEditorId(FixtureGroup::invalidId())
    , m_currentTabIndex(0)
    , m_power(NULL)
    , m_universeUsage(NULL)
    , m_ctxUniverse(InputOutputMap::invalidUniverse())
    , m_addAction(NULL)
    , m_addRGBAction(NULL)
    , m_removeAction(NULL)
    , m_propertiesAction(NULL)
    , m_testAction(NULL)
    , m_fadeConfigAction(NULL)
    , m_remapAction(NULL)
    , m_groupAction(NULL)
    , m_unGroupAction(NULL)
    , m_newGroupAction(NULL)
    , m_moveUpAction(NULL)
    , m_moveDownAction(NULL)
    , m_importAction(NULL)
    , m_exportAction(NULL)
    , m_groupMenu(NULL)
{
    Q_ASSERT(s_instance == NULL);
    s_instance = this;

    Q_ASSERT(doc != NULL);

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
    connect(m_doc, SIGNAL(fixtureRemoved(quint32)),
            this, SLOT(slotFixtureRemoved(quint32)));

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
    QList<QTreeWidgetItem*> groupsToDelete;

    for (int i = 0; i < m_fixtures_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* grpItem = m_fixtures_tree->topLevelItem(i);
        Q_ASSERT(grpItem != NULL);
        for (int j = 0; j < grpItem->childCount(); j++)
        {
            QTreeWidgetItem* fxiItem = grpItem->child(j);
            Q_ASSERT(fxiItem != NULL);
            QVariant var = fxiItem->data(KColumnName, PROP_ID);
            if (var.isValid() == true && var.toUInt() == id)
            {
                delete fxiItem;
                break;
            }
        }
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
            }
            else
            {
                m_propertiesAction->setEnabled(false);
                m_testAction->setEnabled(false);
            }
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

        // Always allow creating a group (its menu's "New Group" makes an
        // empty one when no fixtures are selected; you then drag fixtures in).
        m_groupAction->setEnabled(true);
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

    tabs->addTab(m_fixtures_tree, tr("Fixture Groups"));

    m_channel_groups_tree = new QTreeWidget(this);
    QStringList chan_labels;
    chan_labels << tr("Name") << tr("Channels");
    m_channel_groups_tree->setHeaderLabels(chan_labels);
    m_channel_groups_tree->setRootIsDecorated(false);
    m_channel_groups_tree->setAllColumnsShowFocus(true);
    m_channel_groups_tree->setIconSize(QSize(32, 32));
    m_channel_groups_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(m_channel_groups_tree, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotChannelsGroupSelectionChanged()));
    connect(m_channel_groups_tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotChannelsGroupDoubleClicked(QTreeWidgetItem*)));

    tabs->addTab(m_channel_groups_tree, tr("Channel Groups"));
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
        return; // already editing this group's layout

    QByteArray state = m_splitter->saveState();
    clearRightPane();

    m_groupEditor = new FixtureGroupEditor(grp, m_doc, this);
    m_groupEditorId = grp->id();
    m_splitter->insertWidget(1, m_groupEditor);

    m_splitter->restoreState(state);
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
        // Double-click a fixture -> properties
        if (m_doc->mode() != Doc::Operate)
            slotProperties();
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
    if (index == 1)
    {
        m_addAction->setToolTip(tr("Add group..."));
        updateChannelsGroupView();
        slotChannelsGroupSelectionChanged();
    }
    else if (index == 2)
    {
        m_addAction->setToolTip(tr("Add fixture..."));
        updateRDMView();
    }
    else
    {
        m_addAction->setToolTip(tr("Add fixture..."));
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
    }

    // Universe
    info += genInfo.arg(tr("Universe")).arg(fixture->universe() + 1);

    // Address
    QString range = QString("%1 - %2").arg(fixture->address() + 1).arg(fixture->address() + fixture->channels());
    info += genInfo.arg(tr("Address Range")).arg(range);

    // Channels
    info += genInfo.arg(tr("Channels")).arg(fixture->channels());

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
        info += chInfo.arg(ch + 1).arg(fixture->address() + ch + 1)
                .arg(fixture->channel(ch)->name());
    }

    /********************************************************************
     * Extended device information
     ********************************************************************/

    if (fixtureMode != NULL)
    {
        const QLCPhysical physical = fixtureMode->physical();
        info += title.arg(tr("Physical"));

        float mmInch = 0.0393700787;
        float kgLbs = 2.20462262;
        QString mm("%1mm (%2\")");
        QString kg("%1kg (%2 lbs)");
        QString W("%1W");
        info += genInfo.arg(tr("Width")).arg(mm.arg(physical.width()))
                                        .arg(physical.width() * mmInch, 0, 'g', 4);
        info += genInfo.arg(tr("Height")).arg(mm.arg(physical.height()))
                                         .arg(physical.height() * mmInch, 0, 'g', 4);
        info += genInfo.arg(tr("Depth")).arg(mm.arg(physical.depth()))
                                        .arg(physical.depth() * mmInch, 0, 'g', 4);
        info += genInfo.arg(tr("Weight")).arg(kg.arg(physical.weight()))
                                         .arg(physical.weight() * kgLbs, 0, 'g', 4);
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
    connect(m_addAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAdd()));

    m_addRGBAction = new QAction(QIcon(":/rgbpanel.png"),
                              tr("Add RGB panel..."), this);
    connect(m_addRGBAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAddRGBPanel()));

    m_removeAction = new QAction(QIcon(":/edit_remove.png"),
                                 tr("Delete items"), this);
    connect(m_removeAction, SIGNAL(triggered(bool)),
            this, SLOT(slotRemove()));

    m_propertiesAction = new QAction(QIcon(":/configure.png"),
                                     tr("Properties..."), this);
    connect(m_propertiesAction, SIGNAL(triggered(bool)),
            this, SLOT(slotProperties()));

    m_testAction = new QAction(QIcon(":/fixture.png"),
                               tr("Test Fixture..."), this);
    m_testAction->setEnabled(false);
    connect(m_testAction, SIGNAL(triggered(bool)),
            this, SLOT(slotTestFixture()));

    m_fadeConfigAction = new QAction(QIcon(":/fade.png"),
                                     tr("Channels Fade Configuration..."), this);
    connect(m_fadeConfigAction, SIGNAL(triggered(bool)),
            this, SLOT(slotFadeConfig()));

    // Group actions
    m_groupAction = new QAction(QIcon(":/group.png"),
                                tr("Add fixture to group..."), this);

    m_unGroupAction = new QAction(QIcon(":/ungroup.png"),
                                tr("Remove fixture from group"), this);
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

    // Put all known fixture groups to the menu
    foreach (FixtureGroup* grp, m_doc->fixtureGroups())
    {
        QAction* a = m_groupMenu->addAction(grp->name());
        a->setData((qulonglong) grp);
    }

    // Put a new group action to the group menu
    m_groupMenu->addAction(m_newGroupAction);

    // Put the group menu to the group action
    m_groupAction->setMenu(m_groupMenu);
}

void FixtureManager::initToolBar()
{
    QToolBar* toolbar = new QToolBar(tr("Fixture manager"), this);
    m_toolbar = toolbar;
    toolbar->setFloatable(false);
    toolbar->setMovable(false);
    layout()->setMenuBar(toolbar);
    toolbar->addAction(m_addAction);
    toolbar->addAction(m_addRGBAction);
    toolbar->addAction(m_removeAction);
    toolbar->addAction(m_propertiesAction);
    toolbar->addAction(m_testAction);
    toolbar->addAction(m_fadeConfigAction);
    toolbar->addSeparator();
    toolbar->addAction(m_groupAction);
    toolbar->addAction(m_unGroupAction);
    toolbar->addSeparator();
    toolbar->addAction(m_moveUpAction);
    toolbar->addAction(m_moveDownAction);
    toolbar->addSeparator();
    toolbar->addAction(m_importAction);
    toolbar->addAction(m_exportAction);
    toolbar->addAction(m_remapAction);

    // Discrete expand/collapse-all for the group tree.
    toolbar->addSeparator();
    QAction *expandAllAction = new QAction(QIcon(":/edit_add.png"), tr("Expand all groups"), this);
    QAction *collapseAllAction = new QAction(QIcon(":/edit_remove.png"), tr("Collapse all groups"), this);
    connect(expandAllAction, &QAction::triggered, this,
            [this]() { if (m_fixtures_tree) m_fixtures_tree->expandAll(); });
    connect(collapseAllAction, &QAction::triggered, this,
            [this]() { if (m_fixtures_tree) m_fixtures_tree->collapseAll(); });
    toolbar->addAction(expandAllAction);
    toolbar->addAction(collapseAllAction);

    QToolButton* btn = qobject_cast<QToolButton*> (toolbar->widgetForAction(m_groupAction));
    Q_ASSERT(btn != NULL);
    btn->setPopupMode(QToolButton::InstantPopup);

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

void FixtureManager::addFixture()
{
    AddFixture af(this, m_doc);
    // Seed the dialog with the universe the context menu was opened in, so
    // right-clicking inside a universe adds there instead of the global default.
    if (m_ctxUniverse != InputOutputMap::invalidUniverse())
        af.preselectUniverse(m_ctxUniverse);
    if (af.exec() == QDialog::Rejected)
        return;

    if (af.invalidAddress())
    {
        QMessageBox msg(QMessageBox::Critical, tr("Error"),
                tr("Please enter a valid address"), QMessageBox::Ok);
        msg.exec();
        return;
    }

    quint32 latestFxi = Fixture::invalidId();

    QString name = af.name();
    quint32 address = af.address();
    quint32 universe = af.universe();
    quint32 channels = af.channels();
    int gap = af.gap();

    QLCFixtureDef* fixtureDef = af.fixtureDef();
    QLCFixtureMode* mode = af.mode();

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

    /* If an empty name was given use the model instead */
    if (name.simplified().isEmpty())
    {
        if (fixtureDef != NULL)
            name = fixtureDef->model();
        else
            name = tr("Generic Dimmer");
    }

    /* Names already in use, so a multi-add continues past existing fixtures
       and never collides. Seeded once and grown as we assign each name.
       (A single add keeps the typed name verbatim — duplicates allowed.) */
    QSet<QString> usedNames;
    bool nameHasTrailingNumber = false;
    if (af.amount() > 1)
    {
        for (Fixture *ef : m_doc->fixtures())
            if (ef != NULL)
                usedNames.insert(ef->name());
        // If the typed name already ends in a number ("US #4"), treat it as the
        // start of a run and INCREMENT that number (US #4, US #5, …) rather than
        // bolting a second "#NN" counter onto it ("US #4 #1", "US #4 #2").
        nameHasTrailingNumber =
            QRegularExpression(QStringLiteral("\\d+$")).match(name.trimmed()).hasMatch();
    }

    /* Add the rest (if any) WITH address gap */
    for (int i = 0; i < af.amount(); i++)
    {
        QString modname;

        /* If we're adding more than one fixture, derive a unique name */
        if (af.amount() > 1)
        {
            // Numbered base: hand nextUniqueName the SAME name each pass — the
            // accumulating usedNames set makes it land on the next free number
            // (US #4 → US #5 → US #6). Unnumbered base: classic "Name #NN".
            const QString base = nameHasTrailingNumber
                ? name
                : QString("%1 #%2").arg(name)
                      .arg(i + 1, AppUtil::digits(af.amount()), 10, QChar('0'));
            modname = Doc::nextUniqueName(base, [&usedNames](const QString &c) {
                return usedNames.contains(c);
            });
            usedNames.insert(modname);
        }
        else
            modname = name;

        /* Create the fixture */
        Fixture* fxi = new Fixture(m_doc);

        /* Assign the next address AFTER the previous fixture
           address space plus gap. */
        fxi->setAddress(address + (i * channels) + (i * gap));
        fxi->setUniverse(universe);
        fxi->setName(modname);
        /* Set a fixture definition & mode if they were
           selected. Otherwise create a fixture definition
           and mode for a generic dimmer. */
        if (fixtureDef != NULL && mode != NULL)
        {
            fxi->setFixtureDefinition(fixtureDef, mode);
        }
        else
        {
            QLCFixtureDef* genericDef = fxi->genericDimmerDef(channels);
            QLCFixtureMode* genericMode = fxi->genericDimmerMode(genericDef, channels);
            fxi->setFixtureDefinition(genericDef, genericMode);
        }

        m_doc->addFixture(fxi);
        latestFxi = fxi->id();
        if (addToGroup != NULL)
            addToGroup->assignFixture(latestFxi);
    }

    QTreeWidgetItem* selectItem = m_fixtures_tree->fixtureItem(latestFxi);
    if (selectItem != NULL)
        m_fixtures_tree->setCurrentItem(selectItem);

    updateView();
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

void FixtureManager::slotAdd()
{
    if (m_currentTabIndex == 1)
        addChannelsGroup();
    else
        addFixture();
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
            fxi->setName(tr("%1 - Row %2").arg(rgb.name()).arg(i + 1));
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
        Q_ASSERT(fxi != NULL);
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

    quint32 id = var.toUInt();
    Fixture* fxi = m_doc->fixture(id);
    if (fxi == NULL)
        return;

    QString manuf;
    QString model;
    QString mode;

    if (fxi->fixtureDef() != NULL)
    {
        manuf = fxi->fixtureDef()->manufacturer();
        model = fxi->fixtureDef()->model();
        mode = fxi->fixtureMode()->name();
    }

    AddFixture af(this, m_doc, fxi);
    af.setWindowTitle(tr("Change fixture properties"));
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
            if (fxi->universe() != af.universe())
            {
                fxi->setUniverse(af.universe());
                changed = true;
            }
            if (fxi->address() != af.address())
            {
                fxi->setAddress(af.address());
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

            updateView();
            slotSelectionChanged();
        }
        else
        {
            QMessageBox msg(QMessageBox::Critical, tr("Error"),
                    tr("Please enter a valid address"), QMessageBox::Ok);
            msg.exec();
        }
    }
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

int FixtureManager::headCount(const QList <QTreeWidgetItem*>& items) const
{
    int count = 0;
    QListIterator <QTreeWidgetItem*> it(items);
    while (it.hasNext() == true)
    {
        QTreeWidgetItem* item = it.next();
        Q_ASSERT(item != NULL);

        QVariant var = item->data(KColumnName, PROP_ID);
        if (var.isValid() == false)
            continue;

        Fixture* fxi = m_doc->fixture(var.toUInt());
        count += fxi->heads();
    }

    return count;
}

void FixtureManager::slotProperties()
{
    if (m_currentTabIndex == 1)
        editChannelGroupProperties();
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

void FixtureManager::slotFadeConfig()
{
    ChannelsSelection cfg(m_doc, this, ChannelsSelection::ConfigurationMode);
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
    FixtureGroup* grp = NULL;

    if (action->data().isValid() == true)
    {
        // Existing group selected
        grp = (FixtureGroup*) (action->data().toULongLong());
        Q_ASSERT(grp != NULL);
    }
    else
    {
        // New Group selected.

        // Suggest an equilateral grid
        qreal side = sqrt(headCount(m_fixtures_tree->selectedItems()));
        if (side != floor(side))
            side += 1; // Fixture number doesn't provide a full square
        if (side < 1)
            side = 4;  // empty group: a small default grid to drop into

        CreateFixtureGroup cfg(this);
        cfg.setSize(QSize(side, side));
        if (cfg.exec() != QDialog::Accepted)
            return; // User pressed cancel

        grp = new FixtureGroup(m_doc);
        Q_ASSERT(grp != NULL);
        grp->setName(cfg.name());
        grp->setSize(cfg.size());
        m_doc->addFixtureGroup(grp);
        updateGroupMenu();
    }

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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    doc.setCodec("UTF-8");
#endif
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
                target->setHeadSubGroup(QLCPoint(x, y), src->id());
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
    m_ctxUniverse = InputOutputMap::invalidUniverse();
    if (QTreeWidgetItem *clicked = m_fixtures_tree->itemAt(pos))
    {
        const QVariant uniVar = clicked->data(KColumnName, PROP_UNIVERSE);
        if (uniVar.isValid())
        {
            m_ctxUniverse = uniVar.toUInt();
        }
        else
        {
            const QVariant fv = clicked->data(KColumnName, PROP_ID);
            if (fv.isValid() && clicked->data(KColumnName, PROP_HEAD).isValid() == false)
            {
                Fixture *fxi = m_doc->fixture(fv.toUInt());
                if (fxi != NULL)
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
    menu.addAction(m_addAction);
    menu.addAction(m_addRGBAction);
    menu.addAction(m_propertiesAction);
    menu.addAction(m_testAction);
    menu.addAction(m_removeAction);
    menu.addSeparator();
    menu.addAction(m_groupAction);
    menu.addAction(m_unGroupAction);

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

    // "Move to folder…" — applies to every selected group.
    QAction *moveToFolder = NULL;
    if (selGroups.isEmpty() == false)
    {
        menu.addSeparator();
        moveToFolder = menu.addAction(selGroups.size() > 1
            ? tr("Move %1 groups to folder…").arg(selGroups.size())
            : tr("Move to folder…"));
    }

    QAction *copyToNew = NULL;
    QMenu *copyIntoMenu = NULL;
    if (selFixtures.isEmpty() == false || selGroups.isEmpty() == false)
    {
        menu.addSeparator();
        copyToNew = menu.addAction(tr("Create group from selection…"));

        const QList<FixtureGroup*> groups = m_doc->fixtureGroups();
        if (groups.isEmpty() == false)
        {
            copyIntoMenu = menu.addMenu(tr("Copy into group"));
            foreach (FixtureGroup *g, groups)
            {
                if (g == NULL)
                    continue;
                QAction *a = copyIntoMenu->addAction(g->name());
                a->setData(g->id());
            }
        }
    }

    // Assign the selected fixtures to a power circuit (or remove them). Circuits
    // live in the power-distribution model shown in the pane on the right.
    QMenu *circuitMenu = NULL;
    QAction *removeFromCircuit = NULL;
    if (selFixtures.isEmpty() == false)
    {
        menu.addSeparator();
        PowerDistribution *pd = m_doc->powerDistribution();
        circuitMenu = menu.addMenu(selFixtures.size() > 1
            ? tr("Add %1 fixtures to power circuit").arg(selFixtures.size())
            : tr("Add to power circuit"));
        bool haveCircuit = false;
        for (int s = 0; s < pd->sources().size(); s++)
        {
            const PowerSource &src = pd->sources().at(s);
            if (src.circuits.size() <= 1)
            {
                // A wall socket, single-outlet UPS, or fresh source takes fixtures
                // directly (its single/implicit circuit, created on demand).
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
        removeFromCircuit = menu.addAction(tr("Remove from power circuit"));
    }

    // Build out the Power section itself: a new source from the "Power" folder
    // (or from any power node — you don't have to hit the root exactly), a new
    // circuit on whichever source was clicked.
    QAction *addPowerSource = NULL;
    QAction *addCircuit = NULL;
    if (ctxPowerSource != -2)
    {
        menu.addSeparator();
        addPowerSource = menu.addAction(tr("Add power source…"));
        if (ctxPowerSource >= 0)
            addCircuit = menu.addAction(tr("Add circuit…"));
    }

    // Fixture Studio: rebuild a composite group from the source groups recorded
    // in its per-head provenance tags (re-pulls their current layout, ordered by
    // studio geometry). Offered when exactly one group with provenance is picked.
    QAction *rebuildComposite = NULL;
    if (selGroups.size() == 1)
    {
        FixtureGroup *g = m_doc->fixtureGroup(selGroups.first());
        if (g != NULL && g->headSubGroupMap().isEmpty() == false)
        {
            menu.addSeparator();
            rebuildComposite = menu.addAction(tr("Rebuild composite from members"));
        }
    }

    QAction *chosen = menu.exec(QCursor::pos());

    // addFixture() (fired from m_addAction during exec above) has already
    // consumed the context; clear it so a later toolbar Add starts from the
    // global default.
    m_ctxUniverse = InputOutputMap::invalidUniverse();

    if (chosen == NULL)
        return;

    if (rebuildComposite != NULL && chosen == rebuildComposite)
    {
        const int blocks = m_doc->programmer()->rebuildCompositeGroup(selGroups.first());
        if (blocks > 0)
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

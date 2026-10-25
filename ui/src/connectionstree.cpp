/*
  Q Light Controller Plus - qlcconsole
  connectionstree.cpp

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

#include <QTreeWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHostInfo>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
#include <QCheckBox>
#include <QSettings>
#include <QScrollBar>
#include <QPointer>
#include <QPushButton>
#include <QHBoxLayout>
#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QHash>
#include <QSet>
#include <QBrush>
#include <QColor>
#include <QSysInfo>

#include <QStyledItemDelegate>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidgetItemIterator>

#include "connectionstree.h"
#include "shrinkablebar.h"
#include "connectioneditor.h"
#include "networkroute.h"
#include "universeoutputview.h"
#include "deviceprofile.h"
#include "qlcioplugin.h"
#include "ioplugincache.h"
#include "inputoutputmap.h"
#include "patchundo.h"
#include "inputoutputmanager.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "universe.h"
#include "fixture.h"
#include <QLineEdit>
#include "doc.h"
#include "qlcconfig.h"

/* What a row represents, and enough identity to act on it. Kept on the item
   rather than inferred from depth, because "device" and "universe" are both
   children of a line and depth alone cannot tell them apart. */
#define ROLE_KIND       (Qt::UserRole + 1)
#define ROLE_PLUGIN     (Qt::UserRole + 2)
#define ROLE_LINE       (Qt::UserRole + 3)
#define ROLE_UNIVERSE   (Qt::UserRole + 4)
#define ROLE_OUTPUT     (Qt::UserRole + 5)
#define ROLE_ADDRESS    (Qt::UserRole + 6)
#define ROLE_PORTADDR   (Qt::UserRole + 7)
#define ROLE_LINENAME   (Qt::UserRole + 8)

#define KIND_PLUGIN   1
#define KIND_LINE     2
#define KIND_DEVICE   3
#define KIND_PORT     4
#define KIND_UNIVERSE 5
/* A universe that exists but has nothing patched to it at all -- freshly
   added, or simply never wired up. The rest of this tree is rooted at
   plugins, so a universe like that has no branch to hang off; this kind
   marks the synthetic row that gives it one anyway (see refresh()). */
#define KIND_UNPATCHED_UNIVERSE 6
/* A universe that IS patched, but whose patch is pending -- the interface
   it names (an ArtNet IP, most often) is not one this machine currently
   has, most commonly a workspace opened on a different network than it was
   built for (see OutputPatch::isPending()). Deliberately a different kind
   from KIND_UNPATCHED_UNIVERSE: this one has a real mapping worth keeping
   and showing, not an absence to fill in. */
#define KIND_PENDING_UNIVERSE 7
/* The tree's own root row -- "this machine", everything else nests under
   it. Right-clicking it is the one place "add a protocol" belongs: unlike
   a specific protocol's own row, this one always exists. */
#define KIND_HOST 8
/* Show control & sync: the branch, and a row per link (OSC in, MIDI Show
   Control in, MIDI timecode) with its key in ROLE_PLUGIN. Picking one shows
   its settings on the right (ShowControlSyncPage). */
#define KIND_SHOWCONTROL_BRANCH 9
#define KIND_SHOWCONTROL 10

#define COL_NAME    0
#define COL_DETAIL  1
#define COL_CARRIES 2

/** How a protocol is shown to an operator, as opposed to QLCIOPlugin::name()
 *  -- the identity string this same protocol is saved and matched by in the
 *  workspace file and everywhere internally, which stays exactly as it is.
 *  Display-only: a well-known protocol whose common name and its plugin's
 *  own name() differ badly enough to cost someone a "why isn't sACN here"
 *  moment gets that name added in parens, same shape a reader would expect
 *  from a spec citing both. Nothing else changes -- adding an entry here
 *  cannot rename anything the engine or a saved file depends on. */
static QString displayPluginName(const QString &pluginName)
{
    if (pluginName == "E1.31")
        return QObject::tr("E1.31 (sACN)");
    return pluginName;
}

/** A port address the way the port rows and a node's own panel print it. */
static QString portAddressText(quint32 pa)
{
    return UniversePatcher::portAddressText(pa);
}

/* A list whose rows are what gets picked: bringing an item into view moves
   it up or down, never sideways. QTreeView::scrollTo() also scrolls to show
   the item's CELL -- and the rebuild re-sets the current row on column 0, so
   with the list scrolled right to read Detail, every rebuild (the one held
   off until a right-click menu closes, and every five-second one) jumped it
   back to the left ("every time I open the right click menu in connections
   the connections list scrolls right"). Left/Right on a row-selecting tree
   scroll the bar themselves, so nothing here needs scrollTo to go sideways. */
namespace {
class RowTree : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;
    void scrollTo(const QModelIndex &index, ScrollHint hint = EnsureVisible) override
    {
        const int side = horizontalScrollBar()->value();
        QTreeWidget::scrollTo(index, hint);
        horizontalScrollBar()->setValue(side);
    }
};
}

ConnectionsTree::ConnectionsTree(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_showUnused(NULL)
    , m_rescan(NULL)
    , m_undo(NULL)
    , m_status(NULL)
    , m_linesBaselined(false)
    , m_populatedOnce(false)
    , m_shownOnce(false)
    , m_rebuilding(false)
    , m_since(new QElapsedTimer)
    , m_tree(NULL)
    , m_refreshTimer(NULL)
    , m_activityTimer(NULL)
{
    Q_ASSERT(doc != NULL);

    /* Every patch question and change goes through here; a change rebuilds
       this view once, whichever list it was asked from. */
    m_patcher = new UniversePatcher(m_doc, this);
    connect(m_patcher, &UniversePatcher::patchChanged, this, &ConnectionsTree::refresh);

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(6, 6, 6, 6);

    /* One short line, the rest on hover: as a paragraph above everything it
       took the space the list needed ("wasting a lot of space in the
       connections view with the top panel"). */
    QLabel *hint = new QLabel(tr("Right-click a row for what it can do"), this);
    hint->setObjectName(QStringLiteral("connectionsHint"));
    hint->setStyleSheet(QStringLiteral("QLabel { color: #8a8e99; }"));
    hint->setToolTip(
        tr("How this console is wired, following the signal outward: protocol → "
           "interface → devices heard on it → the universes they carry. "
           "Right-click an interface to patch a universe to it or configure "
           "the protocol; right-click a universe to rename it, set its "
           "feedback line or input profile, unpatch it, or delete it. "
           "Select several universes to patch them onto a device's ports in "
           "the order you clicked them, or unpatch them all at once."));

    /* Default to only what is live: configured or discovered. A stock build
       exposes 14 plugins and dozens of lines, nearly all of them irrelevant --
       loopback addresses, protocols with no hardware, interfaces carrying
       nothing. Listing them buries the two lines that actually matter. The
       toggle exists because a fresh workspace has nothing patched yet, and an
       empty tree would be a dead end when you are trying to patch for the
       first time. */
    m_showUnused = new QCheckBox(tr("Show unused protocols and interfaces"), this);
    m_showUnused->setObjectName("connectionsShowUnused");
    m_showUnused->setChecked(false);
    connect(m_showUnused, SIGNAL(toggled(bool)), this, SLOT(refresh()));

    /* "Rescan", not "Scan": discovery is passive and continuous, so this is an
       accelerator, never a required step. Worst case without it is a node
       appearing up to eight seconds late -- three for the next Art-Net poll,
       five for the next rebuild -- which is a long time to stand at a rack
       wondering whether the cable is in. One click sends one poll per
       interface and redraws immediately. */
    m_rescan = new QPushButton(tr("Rescan"), this);
    m_rescan->setToolTip(tr("Ask every interface for devices now. Discovery is "
                            "continuous anyway; this just skips the wait."));
    connect(m_rescan, SIGNAL(clicked()), this, SLOT(slotRescan()));

    /* Named for what it does, not offered as a bare "Undo". The app already
       made this call once -- the capture action is "Undo Last Store" -- and
       for the same reason: an operator who sees plain "Undo" will press it
       after deleting a fixture, get silence, and stop trusting it. This one
       covers patch changes and says so. */
    m_undo = new QPushButton(tr("Undo Patch Change"), this);
    m_undo->setEnabled(false);
    connect(m_undo, SIGNAL(clicked()), this, SLOT(slotUndoPatch()));

    /* "why is there no patched folder in the connections view?" -- the tree
       is rooted at outputs, so a patched universe is only ever under its
       output. This lists them all, in number order, each saying where it
       goes. */
    m_byUniverse = new QCheckBox(tr("By universe"), this);
    m_byUniverse->setObjectName(QStringLiteral("connectionsByUniverse"));
    m_byUniverse->setToolTip(tr("Every universe in number order, with where each goes and what "
                                "it carries -- rather than the wiring, output by output"));
    m_byUniverse->setChecked(QSettings().value(QStringLiteral("connections/byUniverse"), false).toBool());
    // By universe, the same box shows or hides the universes nothing uses.
    auto labelUnused = [this]() {
        m_showUnused->setText(m_byUniverse->isChecked() ? tr("Show unused universes")
                                                        : tr("Show unused protocols and interfaces"));
    };
    connect(m_byUniverse, &QCheckBox::toggled, this, [this, labelUnused](bool on) {
        QSettings().setValue(QStringLiteral("connections/byUniverse"), on);
        labelUnused();
        refresh();
    });
    labelUnused();

    // Scrolls sideways rather than holding Connections wide (ShrinkableBar).
    QWidget *barRow = new QWidget(this);
    QHBoxLayout *bar = new QHBoxLayout(barRow);
    bar->setContentsMargins(0, 0, 0, 0);
    bar->addWidget(m_byUniverse);
    bar->addWidget(m_showUnused);
    bar->addSpacing(12);
    bar->addWidget(hint);
    bar->addStretch(1);
    bar->addWidget(m_undo);
    bar->addWidget(m_rescan);
    lay->addWidget(new ShrinkableBar(barRow, this));

    if (m_doc->inputOutputMap() != NULL
            && m_doc->inputOutputMap()->patchUndo() != NULL)
    {
        connect(m_doc->inputOutputMap()->patchUndo(), SIGNAL(changed()),
                this, SLOT(slotUndoAvailabilityChanged()));
    }

    /* Discovery is passive: nodes announce themselves roughly once a second.
       On first opening the tab the tree is therefore genuinely incomplete for
       a few seconds, and it looks identical to a finished tree that is simply
       missing a node. Saying so is the difference between "wait" and "go
       investigate the rig". Shown even when the tree already has rows, because
       that is exactly the case where the absence is invisible. */
    m_status = new QLabel(tr("Searching for devices…"), this);
    m_status->setStyleSheet("QLabel { color: #a06000; }");
    bar->insertWidget(bar->indexOf(hint) + 1, m_status);   // on the same row

    m_tree = new RowTree(this);
    m_tree->setObjectName("connectionsTree");
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels(QStringList()
                            << tr("Device") << tr("Detail") << tr("Carries"));
    /* Extended selection so several universes can be retargeted in one go --
       the ArtNet block-patch case, where sixteen universes leave by one node
       on consecutive ports. Every single-row action keeps working on the row
       under the pointer, NOT on the selection: right-clicking one row while
       twelve are selected must not quietly act on twelve. Only the explicitly
       plural actions (patch, unpatch, retarget N selected universes) read the
       selection, and each says the count in its own name before it does
       anything. */
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(COL_NAME, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(COL_DETAIL, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(COL_CARRIES, QHeaderView::Stretch);
    connect(m_tree, SIGNAL(itemChanged(QTreeWidgetItem *, int)),
            this, SLOT(slotItemChanged(QTreeWidgetItem *, int)));
    /* Only a row's name is edited in place ("clicking to edit the text should
       only work on the device .. not on detail or carries"): those two are
       what the row IS, worked out from the patch, not text to type over. */
    class NoEdit : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;
        QWidget *createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const override
        {
            return nullptr;
        }
    };
    m_tree->setItemDelegateForColumn(COL_DETAIL, new NoEdit(m_tree));
    m_tree->setItemDelegateForColumn(COL_CARRIES, new NoEdit(m_tree));
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotContextMenu(QPoint)));
    /* A double-click on a universe opens its connection ("double clicking on
       a universe doesn't open its editor. It should."), so it no longer
       renames: a click on the selected row, or F2, still does. */
    m_tree->setEditTriggers(QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &ConnectionsTree::slotItemDoubleClicked);
    /* Beside it, what the selected universe is sending ("it would be nice to
       see what's being sent out on a U if selected on the right hand side"). */
    m_output = new UniverseOutputView(m_doc, this);
    m_output->setObjectName("connectionsOutput");
    QScrollArea *outScroll = new QScrollArea(this);
    outScroll->setWidget(m_output);
    outScroll->setWidgetResizable(false);
    outScroll->setMinimumWidth(320);         // narrower, it scrolls
    m_output->resize(m_output->sizeHint());
    m_side = new QStackedWidget(this);
    m_side->addWidget(outScroll);
    m_outputPage = outScroll;
    QSplitter *split = new QSplitter(Qt::Horizontal, this);
    split->addWidget(m_tree);
    split->addWidget(m_side);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    lay->addWidget(split, 1);           // every spare pixel goes to the list
    /* Recorded as it happens: see m_selectionOrder. A rebuild clears and
       restores the same selection, which must not reshuffle it. */
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        if (!m_rebuilding)
            syncSelectionOrder();
    });
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        if (m_rebuilding)
            return;                 // a rebuild reselects; keep what is shown
        QTreeWidgetItem *it = m_tree->currentItem();
        if (it == nullptr || !it->isSelected())
            return;
        const int kind = it->data(COL_NAME, ROLE_KIND).toInt();
        if (m_showControlPanel != nullptr
                && (kind == KIND_SHOWCONTROL || kind == KIND_SHOWCONTROL_BRANCH))
        {
            m_side->setCurrentWidget(m_showControlPanel);
            emit showControlRowSelected(kind == KIND_SHOWCONTROL
                                        ? it->data(COL_NAME, ROLE_PLUGIN).toString() : QString());
            return;
        }
        m_side->setCurrentWidget(m_outputPage);
        const QVariant uv = it->data(COL_NAME, ROLE_UNIVERSE);
        if (uv.isValid() && uv.toUInt() != Universe::invalid())
            m_output->setUniverse(uv.toUInt());
    });

    /* Art-Net nodes announce themselves roughly once a second, so a node that
       is powered on (or off) mid-session should appear (or stop being current)
       without the operator hunting for a rescan button. Five seconds is slow
       enough to be invisible and fast enough to feel live. */
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(5000);
    connect(m_refreshTimer, SIGNAL(timeout()), this, SLOT(refresh()));
    /* Deliberately NOT started here -- showEvent() owns the timer, so a
       hidden tab costs nothing. */

    /* An output going into or out of trouble shows within a moment rather
       than at the next five-second tick -- coalesced, since one dead node
       can take many universes with it at once. Only while on screen. */
    m_problemRefresh = new QTimer(this);
    QTimer *problemRefresh = m_problemRefresh;
    problemRefresh->setSingleShot(true);
    problemRefresh->setInterval(250);
    connect(problemRefresh, SIGNAL(timeout()), this, SLOT(refresh()));
    /* By name (see App: on Windows QLCIOPlugin's signal has no single
       exporting DLL, and a member-pointer connection never fires there). */
    if (m_doc != NULL && m_doc->ioPluginCache() != NULL)
    {
        foreach (QLCIOPlugin *plugin, m_doc->ioPluginCache()->plugins())
            connect(plugin, SIGNAL(targetProblemsChanged()), this, SLOT(slotTargetProblemsChanged()));
    }

    /* Live input. Devices is the tab you are on when asking "is that surface
       actually sending anything", and it was the only one of the three that
       could not answer -- Overview flashes its input combo, Detailed prints a
       readout, this had no input hookup at all. Tinting the universe row says
       it in the place the question is being asked. */
    if (m_doc->inputOutputMap() != NULL)
    {
        connect(m_doc->inputOutputMap(),
                SIGNAL(inputValueChanged(quint32,quint32,uchar)),
                this, SLOT(slotInputActivity(quint32,quint32,uchar)));
        /* A device coming or going (a controller asleep or back) shows at
           once, not at the next five-second tick. */
        connect(m_doc->inputOutputMap(), SIGNAL(offlinePatchesChanged()),
                this, SLOT(slotTargetProblemsChanged()));
        connect(m_doc->inputOutputMap(), SIGNAL(pluginConfigurationChanged(QString,bool)),
                this, SLOT(slotTargetProblemsChanged()));
    }
    m_activityTimer = new QTimer(this);
    m_activityTimer->setSingleShot(true);
    connect(m_activityTimer, SIGNAL(timeout()), this, SLOT(slotActivityTimeout()));

    m_since->start();
    refresh();
}

/** Tint every row belonging to the universe that just received input.
 *
 *  A universe can occupy several rows at once -- one per output leg, plus a
 *  feedback row under whichever line carries it -- and the input arrived at
 *  the universe, not at any one of them. Lighting all of them is the honest
 *  answer; picking one would be inventing a relationship.
 */
void ConnectionsTree::slotTargetProblemsChanged()
{
    if (isVisible() && m_problemRefresh != nullptr)
        m_problemRefresh->start();
}

void ConnectionsTree::slotInputActivity(quint32 universe, quint32 channel,
                                        uchar value)
{
    /* Free when the tab is not on screen, which is most of the time and every
       MIDI event during a show. */
    if (isVisible() == false)
        return;

    const QString tip = tr("Input active — channel %1 = %2").arg(channel).arg(value);
    bool found = false;
    QList<QTreeWidgetItem *> stack;
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
        stack << m_tree->topLevelItem(i);
    while (stack.isEmpty() == false)
    {
        QTreeWidgetItem *it = stack.takeFirst();
        const QVariant uv = it->data(COL_NAME, ROLE_UNIVERSE);
        if (uv.isValid() && uv.toUInt() == universe)
        {
            it->setBackground(COL_DETAIL, QBrush(QColor(66, 133, 244, 150)));
            it->setToolTip(COL_DETAIL, tip);
            found = true;
        }
        for (int c = 0; c < it->childCount(); c++)
            stack << it->child(c);
    }

    if (found == false)
        return;

    m_activeUniverses.insert(universe);
    /* Restarted on every event, so a stream of MIDI holds the tint steady
       instead of strobing it. */
    m_activityTimer->start(300);
}

void ConnectionsTree::slotActivityTimeout()
{
    if (m_activeUniverses.isEmpty())
        return;

    QList<QTreeWidgetItem *> stack;
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
        stack << m_tree->topLevelItem(i);
    while (stack.isEmpty() == false)
    {
        QTreeWidgetItem *it = stack.takeFirst();
        const QVariant uv = it->data(COL_NAME, ROLE_UNIVERSE);
        if (uv.isValid() && m_activeUniverses.contains(uv.toUInt()))
        {
            /* Back to no tint rather than to a remembered one: refresh()
               owns the row's normal appearance and may have rebuilt it
               underneath us in the meantime. */
            it->setBackground(COL_DETAIL, QBrush());
            it->setToolTip(COL_DETAIL, QString());
        }
        for (int c = 0; c < it->childCount(); c++)
            stack << it->child(c);
    }
    m_activeUniverses.clear();
}

void ConnectionsTree::showEvent(QShowEvent *ev)
{
    QWidget::showEvent(ev);

    /* The constructor may have run at startup, minutes before anyone opened
       this tab, so "how long have we been searching" has to be measured from
       here or the amber banner is already expired on arrival. */
    if (m_shownOnce == false)
    {
        m_shownOnce = true;
        m_since->restart();
        /* One poll on first arrival, so an unpatched interface has had a
           chance to answer before the operator concludes it is empty. */
        slotRescan();
    }
    else
    {
        refresh();
    }

    m_refreshTimer->start();
}

void ConnectionsTree::hideEvent(QHideEvent *ev)
{
    m_refreshTimer->stop();
    /* "New" means new since you last looked. Leaving the tab is the natural
       end of looking; anything still unpatched next time is just unpatched. */
    m_newLines.clear();
    QWidget::hideEvent(ev);
}

void ConnectionsTree::slotUndoAvailabilityChanged()
{
    if (m_undo == NULL || m_doc->inputOutputMap() == NULL)
        return;
    PatchUndo *pu = m_doc->inputOutputMap()->patchUndo();
    if (pu == NULL)
        return;

    m_undo->setEnabled(pu->canUndo());
    /* The label names the specific change, so the button says what pressing it
       will actually put back rather than making the operator remember. */
    m_undo->setToolTip(pu->canUndo()
        ? tr("Put back: %1").arg(pu->summary())
        : tr("Nothing to undo. Covers patch changes only — not adding or "
             "removing universes."));
}

/** The button routes through InputOutputManager's action rather than doing the
 *  work itself: one confirmation, one set of surfaces refreshed afterwards,
 *  and no way for the three tabs to drift apart on either. */
void ConnectionsTree::slotUndoPatch()
{
    if (InputOutputManager::instance() != NULL)
        InputOutputManager::instance()->slotUndoPatch();
}

/** Ask every plugin to go looking, then redraw.
 *
 *  QLCIOPlugin::rescan() is the existing hot-plug hook -- USB plugins
 *  re-enumerate, and ArtNet sends one ArtPoll per interface INCLUDING the
 *  interfaces with nothing patched, which are the ones its periodic poll
 *  never touches. Plugins that have nothing to re-examine inherit a no-op.
 */
void ConnectionsTree::slotRescan()
{
    IOPluginCache *cache = m_doc->ioPluginCache();
    if (cache != NULL)
    {
        foreach (QLCIOPlugin *plugin, cache->plugins())
        {
            if (plugin != NULL)
                plugin->rescan();
        }
    }

    probeUnheardTargets();
    refresh();
}

/** Ask every address we believe in, but have never heard from, whether it is
 *  there.
 *
 *  Discovery is broadcast-shaped, so it answers "what is on my segment" and
 *  cannot answer "is that node alive". A target on another subnet -- declared
 *  by hand, or named by a patch -- never sees a broadcast poll, so it sits
 *  amber and "not heard from" forever whether it is powered up or in a flight
 *  case. Probing each one directly is the difference between the tint meaning
 *  "no answer" and meaning "no question asked".
 *
 *  Only the ones we have NOT heard from: a node already answering broadcasts
 *  needs no unicast, and polling every known node individually would multiply
 *  the traffic this deliberately keeps low.
 */
void ConnectionsTree::probeUnheardTargets()
{
    IOPluginCache *cache = m_doc->ioPluginCache();
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (cache == NULL || iomap == NULL)
        return;

    foreach (QLCIOPlugin *plugin, cache->plugins())
    {
        if (plugin == NULL)
            continue;

        QSet<QString> heard;
        foreach (const QLCIOPlugin::Device &d, plugin->discoveredDevices())
            heard.insert(d.address);

        QSet<QString> wanted;

        /* Hand-declared targets. */
        foreach (const InputOutputMap::ManualTarget &mt, iomap->manualTargets())
        {
            if (mt.plugin == plugin->name())
                wanted.insert(mt.address);
        }

        /* Addresses a patch is aimed at. A workspace built on the rig and
           opened elsewhere is full of these, and they are the ones somebody is
           squinting at wondering whether the rig is plugged in. */
        foreach (Universe *uni, iomap->universes())
        {
            if (uni == NULL)
                continue;
            for (int i = 0; i < uni->outputPatchesCount(); i++)
            {
                OutputPatch *op = uni->outputPatch(i);
                if (op == NULL || op->plugin() == NULL
                        || op->plugin()->name() != plugin->name())
                    continue;
                const QString ip = op->getPluginParameters()
                                   .value("outputIP").toString();
                if (ip.isEmpty() == false)
                    wanted.insert(ip);
            }
        }

        foreach (const QString &addr, wanted)
        {
            if (heard.contains(addr) == false)
                plugin->probeTarget(addr);
        }
    }
}

ConnectionsTree::~ConnectionsTree()
{
    delete m_since;
}

QList<quint32> ConnectionsTree::universesOn(const QString &pluginName, quint32 line,
                                            bool output) const
{
    QList<quint32> list;
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return list;

    foreach (Universe *uni, iomap->universes())
    {
        if (output)
        {
            /* A universe may carry several output patches, so check them all
               rather than only index 0 -- otherwise a universe fanned out to
               two nodes shows under one of them and silently not the other. */
            for (int i = 0; i < uni->outputPatchesCount(); i++)
            {
                OutputPatch *op = uni->outputPatch(i);
                if (op != NULL && op->plugin() != NULL
                        && op->plugin()->name() == pluginName && op->output() == line)
                {
                    list << uni->id();
                    break;
                }
            }
        }
        else
        {
            InputPatch *ip = uni->inputPatch();
            if (ip != NULL && ip->plugin() != NULL
                    && ip->plugin()->name() == pluginName && ip->input() == line)
                list << uni->id();
        }
    }
    return list;
}

QList<quint32> ConnectionsTree::feedbackOn(const QString &pluginName,
                                           quint32 line) const
{
    QList<quint32> list;
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return list;

    foreach (Universe *uni, iomap->universes())
    {
        OutputPatch *fb = iomap->feedbackPatch(uni->id());
        if (fb != NULL && fb->plugin() != NULL
                && fb->plugin()->name() == pluginName && fb->output() == line)
            list << uni->id();
    }
    return list;
}

void ConnectionsTree::refresh()
{
    if (m_tree == NULL || m_doc == NULL)
        return;

    /* Never rebuild under someone's hands. The periodic refresh tears down
       every row, which destroys an open editor mid-word -- so renaming a target
       became a race against a five second timer.
       QAbstractItemView::state() would say this directly but is protected, so
       detect the editor by focus: an open item editor is a focused widget
       parented into the view.
       Excluding m_tree and its viewport themselves matters: the tree's own
       RESTING focus state after any right-click (context menu, then any
       modal dialog it opened) is the tree or its viewport having focus --
       not a child editor -- and every CRUD action here ends in refresh().
       Without this exclusion, that resting state matched the same
       ancestor check and silently skipped the rebuild right after almost
       every action, which is a very different bug from the one this guard
       exists to prevent: the row just never visibly updated at all,
       looking like the action itself had silently failed. Only an ACTUAL
       inline editor -- a QLineEdit parented deeper into the view than the
       viewport, created for an editable cell -- still blocks it. */
    QWidget *focus = QApplication::focusWidget();
    if (focus != NULL && focus != m_tree && focus != m_tree->viewport()
            && m_tree->isAncestorOf(focus))
        return;

    /* Nor while a menu or dialog is up ("I can't select and patch cause it
       jumps"): a right-click menu holds the row it was opened on, and a
       rebuild under it tore that row down, so Patch... acted on nothing.
       The next tick, or the action's own refresh, catches up. */
    if (QApplication::activePopupWidget() != NULL || QApplication::activeModalWidget() != NULL)
        return;

    if (m_byUniverse != NULL && m_byUniverse->isChecked())
    {
        buildUniverseList();
        return;
    }
    m_tree->setRootIsDecorated(true);           // the wiring is a tree

    /* Remember which branches are OPEN, keyed by full path, so the rebuild
       every five seconds restores exactly what was on screen.
       An earlier version saved the expanded set, matched it back with
       findItems() on the DISPLAY NAME, and then called expandToDepth() anyway
       -- so a closed branch reopened itself within five seconds and
       identically-named rows under different parents dragged each other open.
       Full paths fix both: they are unique, and they are compared against
       nothing but themselves. */
    if (m_populatedOnce)
    {
        m_expanded.clear();
        QList<QTreeWidgetItem *> stack;
        for (int i = 0; i < m_tree->topLevelItemCount(); i++)
            stack << m_tree->topLevelItem(i);
        while (stack.isEmpty() == false)
        {
            QTreeWidgetItem *it = stack.takeFirst();
            if (it->childCount() > 0 && it->isExpanded())
                m_expanded.insert(itemPath(it));
            for (int c = 0; c < it->childCount(); c++)
                stack << it->child(c);
        }
    }

    /* ...and where you were and what you had picked. The rebuild every five
       seconds used to drop the selection and scroll back to the top, so a
       universe clicked a moment ago was gone, and the list jumped under the
       pointer. Kept by full path, like the open branches. */
    const QPoint scroll(m_tree->horizontalScrollBar()->value(),
                        m_tree->verticalScrollBar()->value());
    QSet<QString> selectedPaths;
    foreach (QTreeWidgetItem *it, m_tree->selectedItems())
        selectedPaths.insert(itemPath(it));
    const QString currentPath = m_tree->currentItem() != NULL ? itemPath(m_tree->currentItem())
                                                             : QString();

    m_rebuilding = true;
    m_tree->clear();

    /* Root of everything below: a visible "this is all local to THIS
       machine" boundary, and the one place "add a protocol" belongs --
       right-clicking a specific protocol only works once that protocol
       already has a row, which "Show unused" deliberately withholds by
       default (see refresh()'s own comment on that below). This root
       always exists and is always right-clickable, checkbox or not. */
    QTreeWidgetItem *hostItem = new QTreeWidgetItem(m_tree);
    hostItem->setData(COL_NAME, ROLE_KIND, KIND_HOST);
    hostItem->setExpanded(true);

    const bool showAll = (m_showUnused != NULL && m_showUnused->isChecked());

    /* Which lines exist this time round, so the ones that were not here last
       time can be spotted. */
    QSet<QString> curLines;

    IOPluginCache *cache = m_doc->ioPluginCache();
    if (cache == NULL)
    {
        /* Bailing out with m_rebuilding still set would suppress
           slotItemChanged for the rest of the session -- inline renames would
           silently stop committing. */
        m_rebuilding = false;
        return;
    }

    /* "This Host" on its own said nothing useful -- once it had a real job
       (see the KIND_HOST comment above), it earned a real answer to "which
       machine, running what". The identity that actually matters when a
       show file was built somewhere else: which host this is, and which
       I/O plugins it has to work with (their versions, where a plugin
       reports one, live in its own pluginInfo() tooltip already -- shown
       here is just which ones exist to try). */
    {
        const QString hostName = QHostInfo::localHostName();
        hostItem->setText(COL_NAME, hostName.isEmpty() ? tr("This Host") : hostName);

        QStringList pluginNames;
        foreach (QLCIOPlugin *p, cache->plugins())
            if (p != NULL)
                pluginNames << displayPluginName(p->name());
        hostItem->setText(COL_DETAIL,
            tr("qlcconsole %1 · %2 I/O plugin%3")
                .arg(APPVERSION).arg(pluginNames.count())
                .arg(pluginNames.count() == 1 ? QString() : QString("s")));

        setRowTooltip(hostItem, tr(
            "<b>%1</b><br>"
            "qlcconsole %2<br>"
            "Qt %3<br>"
            "%4<br><br>"
            "I/O plugins: %5")
                .arg(hostName.isEmpty() ? tr("(hostname unavailable)") : hostName)
                .arg(APPVERSION)
                .arg(QT_VERSION_STR)
                .arg(QSysInfo::prettyProductName())
                .arg(pluginNames.isEmpty() ? tr("none") : pluginNames.join(", ")));
    }

    /* Every universe with a pending output patch (see
       OutputPatch::isPending()), grouped by which plugin and which missing
       interface identity it is waiting for -- computed up front so the
       per-plugin loop below can give each group a row right where a real
       interface would sit, instead of the universe having nowhere to go
       until a separate, isolated pass at the very end. A protocol problem
       belongs on that protocol's own branch. */
    QMap<QString, QMap<QString, QList<Universe *> > > pendingByPluginThenInterface;
    QSet<quint32> pendingUniverseIds;
    QMap<QString, QMap<QString, QList<Universe *> > > absentInputsByPluginThenDevice;
    if (m_doc->inputOutputMap() != NULL)
    {
        foreach (Universe *u, m_doc->inputOutputMap()->universes())
        {
            if (u == NULL)
                continue;
            for (int i = 0; i < u->outputPatchesCount(); i++)
            {
                OutputPatch *op = u->outputPatch(i);
                if (op != NULL && op->isPending())
                {
                    pendingByPluginThenInterface[op->pluginName()][op->outputName()] << u;
                    pendingUniverseIds.insert(u->id());
                    break;   // one ghost row per universe is enough
                }
            }
            /* A device input kept while its device is away (a pad asleep,
               a MIDI surface unplugged): drawn as that device's line, not
               connected, under its plugin (InputOutputMap::
               followInputDevices()). */
            foreach (const Universe::OfflinePatch &rec, u->offlinePatches())
            {
                if (rec.tag != Universe::InputPatchTag || rec.uid.isEmpty())
                    continue;
                QLCIOPlugin *p = cache->plugin(rec.plugin);
                if (p == NULL || p->linesAreHardware() == false)
                    continue;
                absentInputsByPluginThenDevice[rec.plugin][rec.uid] << u;
                pendingUniverseIds.insert(u->id());
            }
        }
    }

    foreach (QLCIOPlugin *plugin, cache->plugins())
    {
        if (plugin == NULL)
            continue;

        QTreeWidgetItem *pitem = new QTreeWidgetItem(hostItem);
        pitem->setText(COL_NAME, displayPluginName(plugin->name()));
        /* What the plugin says about itself. It was written to be read -- the
           Detailed tab renders it in a browser pane -- and the protocol row is
           where somebody asks "what IS E1.31, and is it working", so hovering
           it should answer rather than sending them to another tab. Same
           source as that pane, so the two cannot disagree. */
        if (m_doc->inputOutputMap() != NULL)
        {
            const QString desc =
                m_doc->inputOutputMap()->pluginDescription(plugin->name());
            if (desc.isEmpty() == false)
                setRowTooltip(pitem, desc);
        }
        pitem->setData(COL_NAME, ROLE_KIND, KIND_PLUGIN);
        pitem->setData(COL_NAME, ROLE_PLUGIN, plugin->name());

        QList<QLCIOPlugin::Device> devices = plugin->discoveredDevices();

        const QStringList outLines = plugin->outputs();
        const QStringList inLines = plugin->inputs();
        const int lineCount = qMax(outLines.count(), inLines.count());

        for (int line = 0; line < lineCount; line++)
        {
            const QString label = line < outLines.count() ? outLines.at(line)
                                  : inLines.at(line);

            QList<quint32> outUnis = universesOn(plugin->name(), quint32(line), true);
            QList<quint32> inUnis = universesOn(plugin->name(), quint32(line), false);
            /* A line can carry nothing but feedback -- a MIDI control surface
               being lit is exactly that -- and counting only in/out patches
               filtered those lines out of the view entirely. */
            QList<quint32> fbUnis = feedbackOn(plugin->name(), quint32(line));

            int devicesHere = 0;
            foreach (const QLCIOPlugin::Device &d, devices)
                if (d.line == quint32(line))
                    devicesHere++;

            /* Hand-declared targets count as devices for the liveness test
               below. Without this a target typed onto an otherwise empty
               interface was stored, and then never drawn: the line carried no
               patch and nothing had been heard on it, so it was filtered out
               before the manual rows were ever reached, taking them with it.
               Declaring something and having it vanish is worse than not being
               able to declare it. */
            if (m_doc->inputOutputMap() != NULL)
            {
                foreach (const InputOutputMap::ManualTarget &mt,
                         m_doc->inputOutputMap()->manualTargets())
                {
                    if (mt.plugin != plugin->name())
                        continue;
                    const quint32 ml = mt.line < quint32(lineCount) ? mt.line : 0;
                    if (ml == quint32(line))
                        devicesHere++;
                }
            }

            /* A line that was not here on the last pass has just been plugged
               in. Only Art-Net implements discoveredDevices(), so a USB widget
               never counts as "something was heard on it" -- it arrives with
               nothing patched, fails the liveness test below, and vanishes
               into the hidden set at the exact moment its owner is standing
               there watching for it. Appearing IS the event; treat it as one.
               It stays visible while the tab is open (hideEvent re-baselines)
               so it does not blink out five seconds later. */
            const QString lineKey = plugin->name() + "|" + label;
            curLines.insert(lineKey);
            if (m_linesBaselined && m_knownLines.contains(lineKey) == false)
                m_newLines.insert(lineKey);
            const bool isNew = m_newLines.contains(lineKey);

            /* "Live" means something is patched to it or something was heard on
               it. An interface that is merely present is not interesting. */
            /* For a plugin whose lines ARE its hardware, existing is enough.
               The DMXKing was enumerated at startup -- the log shows the widget
               found, opened and its serial read -- and then hidden, because it
               had nothing patched and DMX USB has no discoveredDevices() to
               make it count as "heard". Newness could not save it either: it
               was already there when the tree took its first baseline. The
               liveness rule was written for protocols that synthesise a line
               per network interface, and those plugins had simply inherited
               it. */
            if (showAll == false && isNew == false
                    && m_pinnedLines.contains(lineKey) == false
                    && plugin->linesAreHardware() == false
                    && outUnis.isEmpty() && inUnis.isEmpty()
                    && fbUnis.isEmpty() && devicesHere == 0)
                continue;

            QTreeWidgetItem *litem = new QTreeWidgetItem(pitem);
            /* Name the NIC as well as the address. "172.18.2.17" says nothing
               about which cable that is; "vlan0" is the thing an operator can
               act on -- unplug it, check its link light, compare it against
               the switch port. Same "name (identity)" shape as the target
               rows below, and the address stays visible because it is what
               the patch actually stores. */
            const QString ifname = plugin->lineDescription(quint32(line),
                                                           line < outLines.count());
            /* An operator-supplied name wins over the NIC name: "FOH rack" is
               what they call it, "vlan0" is what the OS calls it, and the
               address stays visible either way because that is what the patch
               stores and what gets typed into a node's front panel. */
            const QString lalias = m_doc->inputOutputMap()
                ? m_doc->inputOutputMap()->lineAlias(plugin->name(), label)
                : QString();
            const QString lshown = lalias.isEmpty() ? ifname : lalias;
            litem->setText(COL_NAME, lshown.isEmpty()
                           ? label : tr("%1 (%2)").arg(lshown).arg(label));
            litem->setData(COL_NAME, ROLE_LINENAME, label);
            litem->setFlags(litem->flags() | Qt::ItemIsEditable);
            litem->setData(COL_NAME, ROLE_KIND, KIND_LINE);
            litem->setData(COL_NAME, ROLE_PLUGIN, plugin->name());
            litem->setData(COL_NAME, ROLE_LINE, quint32(line));

            QStringList roles;
            if (line < outLines.count()) roles << tr("output");
            if (line < inLines.count()) roles << tr("input");
            /* Patches follow the network, not the interface (NetworkRoute):
               say which network this line puts them on. */
            const QString lineNet = NetworkRoute::networkForLine(label);
            litem->setText(COL_DETAIL, lineNet.isEmpty() ? roles.join(", ")
                : tr("%1 · network %2").arg(roles.join(", "), lineNet));

            /* Plugins already write a description of a line for their own
               config dialogs -- widget type and serial for DMX USB, the port
               and packet counters for Art-Net. It is the best answer available
               to "what IS this thing", and it was reachable only by opening a
               different dialog. */
            QStringList lineInfo;
            if (m_doc->inputOutputMap() != NULL)
            {
                /* The same per-line status the Detailed pane shows -- packet
                   counters, widget type, what the far end reported. */
                if (line < inLines.count())
                {
                    const QString in = m_doc->inputOutputMap()
                        ->inputPluginStatus(plugin->name(), quint32(line));
                    if (in.isEmpty() == false)
                        lineInfo << in;
                }
                if (line < outLines.count())
                {
                    const QString out = m_doc->inputOutputMap()
                        ->outputPluginStatus(plugin->name(), quint32(line));
                    if (out.isEmpty() == false)
                        lineInfo << out;
                }
            }
            if (lineInfo.isEmpty())
            {
                /* Fall back to what the plugin will say directly, so a plugin
                   the map has nothing on is not left with a blank row. */
                if (line < outLines.count())
                    lineInfo << plugin->outputInfo(quint32(line));
                if (lineInfo.isEmpty() && line < inLines.count())
                    lineInfo << plugin->inputInfo(quint32(line));
            }
            lineInfo.removeAll(QString());
            if (lineInfo.isEmpty() == false)
                setRowTooltip(litem, lineInfo.join("<br>"));

            const int total = outUnis.count() + inUnis.count() + fbUnis.count();
            litem->setText(COL_CARRIES, total == 0 ? tr("nothing patched")
                                                   : tr("%1 universes").arg(total));
            if (isNew)
            {
                litem->setText(COL_DETAIL, roles.isEmpty()
                    ? tr("just appeared")
                    : tr("%1 · just appeared").arg(roles.join(", ")));
                litem->setBackground(COL_NAME, QBrush(QColor(46, 125, 50, 40)));
            }

            /* Port rows, keyed by "node address|port address", so a universe
               can be filed under the port it is actually aimed at. */
            QHash<QString, QTreeWidgetItem *> portRows;
            /* Node rows synthesised for targets nothing has answered from. */
            QHash<QString, QTreeWidgetItem *> ghostNodes;
            /* Port rows under unheard targets, so a configured node has the
               same node -> port -> universe shape as a discovered one. */
            QHash<QString, QTreeWidgetItem *> ghostPorts;

            /* Devices heard on this line. Most plugins report none -- for a
               USB widget or a MIDI port the line already IS the device, and
               inventing a second level for it would only add depth. */
            /* A node can be heard more than once on one line -- it answers on
               every interface it has, and the replies arrive from different
               source addresses while carrying the same node identity. Key on
               what identifies the hardware, not on who delivered the packet. */
            InputOutputMap *iomapForAlias = m_doc->inputOutputMap();
            QSet<QString> seenDevices;
            /* Addresses alone, not the address|MAC key above: a hand-declared
               target has no MAC, so that key could never match and the manual
               row would sit next to the real one as a duplicate. */
            QSet<QString> seenAddresses;
            foreach (const QLCIOPlugin::Device &dev, devices)
            {
                if (dev.line != quint32(line))
                    continue;

                /* Our own console answers ArtPoll like any other node, so it
                   turns up as a device on its own interface -- "ArtNet ->
                   172.18.2.17 -> Q Light Controller Plus on 172.18.2.17".
                   That is just us looking in a mirror; the interface row
                   already represents this machine. */
                if (dev.address == label)
                    continue;

                const QString devKey = dev.address + "|" + dev.hardwareId;
                if (seenDevices.contains(devKey))
                    continue;
                seenDevices.insert(devKey);
                seenAddresses.insert(dev.address);

                QTreeWidgetItem *ditem = new QTreeWidgetItem(litem);
                const QString alias = iomapForAlias
                    ? iomapForAlias->targetAlias(dev.address) : QString();
                const QString shown = alias.isEmpty()
                    ? (dev.name.isEmpty() ? dev.address : dev.name) : alias;
                /* Always carry the address alongside the name. A name is what
                   an operator recognises; the address is what they have to
                   type into a node's front panel or a ping. Showing one
                   without the other means looking somewhere else. Display and
                   edit values differ deliberately, so the inline editor offers
                   the bare name rather than something with "(1.2.3.4)" glued
                   on that would be saved back verbatim. */
                ditem->setText(COL_NAME, shown == dev.address ? shown
                                   : tr("%1 (%2)").arg(shown).arg(dev.address));
                ditem->setData(COL_NAME, ROLE_ADDRESS, dev.address);
                ditem->setFlags(ditem->flags() | Qt::ItemIsEditable);
                ditem->setData(COL_NAME, ROLE_KIND, KIND_DEVICE);

                QStringList detail;
                /* What has to be done first goes first: a node heard but not
                   on any network this computer is on (a new one, still on its
                   factory address) cannot be patched to until its IP is set. */
                if (dev.attention.isEmpty() == false) detail << dev.attention;
                if (dev.address.isEmpty() == false) detail << dev.address;
                if (dev.hardwareId.isEmpty() == false) detail << dev.hardwareId;
                if (dev.detail.isEmpty() == false) detail << dev.detail;
                detail << (dev.rdmCapable ? tr("RDM") : tr("no RDM"));
                ditem->setText(COL_DETAIL, detail.join(" \u00b7 "));
                /* Same tint vocabulary as the universe rows: green means the
                   path is real, amber means it is only configured. Applied to
                   the node as well as its universes so availability reads at
                   the level someone actually asks about it -- "is that node
                   there", not "is that universe reachable". A node needing
                   attention is there but not usable yet: amber. */
                ditem->setBackground(COL_NAME, dev.attention.isEmpty()
                                     ? QBrush(QColor(46, 125, 50, 40))
                                     : QBrush(QColor(245, 166, 35, 40)));
                if (dev.attention.isEmpty() == false)
                    ditem->setForeground(COL_DETAIL, QBrush(QColor("#a06000")));
                setRowTooltip(ditem, propertyTooltip(
                    shown == dev.address ? dev.address
                                         : tr("%1 (%2)").arg(shown).arg(dev.address),
                    dev.properties));

                /* Ports can share a port address -- OLA advertises swOut=0 on
                   all four of its ports until they are configured. A packet
                   carries only the port address, so ports sharing one are
                   genuinely indistinguishable on the wire: anything sent to
                   that address reaches all of them. Say so on the row instead
                   of pretending the choice is meaningful, and file matches
                   under the FIRST such port. Keying the lookup without this
                   silently kept the LAST port, which is why patching to
                   "port 1" landed under "port 4". */
                QSet<quint32> seenPortAddr;
                for (int p = 0; p < dev.portLabels.count(); p++)
                {
                    const quint32 pa = p < dev.portUniverses.count()
                                       ? dev.portUniverses.at(p) : 0;
                    const bool shared = seenPortAddr.contains(pa);
                    seenPortAddr.insert(pa);

                    QTreeWidgetItem *pt = new QTreeWidgetItem(ditem);
                    pt->setText(COL_NAME, tr("port %1").arg(dev.portNumbers.value(p, p + 1)));
                    /* "port" is the socket on the box, numbered as the node
                       numbers it; "universe" is the Art-Net port-address it
                       listens to (the plugin's "ArtNet Universe"), as
                       net:subnet:universe. Two numbers, two names. */
                    const QString uniText = p < dev.portUniverses.count()
                        ? tr("universe %1").arg(portAddressText(pa)) : dev.portLabels.at(p);
                    pt->setText(COL_DETAIL, shared
                        ? tr("%1  (same address as an earlier port)").arg(uniText)
                        : uniText);
                    pt->setData(COL_NAME, ROLE_KIND, KIND_PORT);
                    pt->setData(COL_NAME, ROLE_PLUGIN, plugin->name());
                    pt->setData(COL_NAME, ROLE_LINE, quint32(line));
                    pt->setData(COL_NAME, ROLE_ADDRESS, dev.address);
                    if (p < dev.portUniverses.count())
                    {
                        pt->setData(COL_NAME, ROLE_PORTADDR, pa);
                        const QString key = QString("%1|%2").arg(dev.address).arg(pa);
                        if (portRows.contains(key) == false)
                            portRows.insert(key, pt);
                    }
                }
            }

            /* Targets declared by hand. Rendered with exactly the shape of a
               discovered node -- device > port > universe -- because from the
               operator's side it IS one: something at an address, with ports,
               carrying universes. The only honest difference is that nothing
               has answered from it, which the amber tint already says, so
               giving it a separate visual language would imply a distinction
               that does not exist once the node is plugged in.

               Discovery wins on a clash: once the real node answers, its live
               port list and firmware replace the typed-in guess rather than
               appearing beside it. */
            if (iomapForAlias != NULL)
            {
                foreach (const InputOutputMap::ManualTarget &mt,
                         iomapForAlias->manualTargets())
                {
                    if (mt.plugin != plugin->name())
                        continue;
                    /* A line index saved against a different set of NICs can
                       point past the end; show it rather than lose it. */
                    const quint32 mline = mt.line < quint32(lineCount) ? mt.line : 0;
                    if (mline != quint32(line))
                        continue;
                    if (seenAddresses.contains(mt.address))
                        continue;
                    seenAddresses.insert(mt.address);

                    QTreeWidgetItem *ditem = new QTreeWidgetItem(litem);
                    const QString alias = iomapForAlias->targetAlias(mt.address);
                    ditem->setText(COL_NAME, alias.isEmpty() ? mt.address
                                       : tr("%1 (%2)").arg(alias).arg(mt.address));
                    ditem->setText(COL_DETAIL,
                                   tr("%1 · declared by hand · not heard from")
                                   .arg(mt.address));
                    ditem->setData(COL_NAME, ROLE_KIND, KIND_DEVICE);
                    ditem->setData(COL_NAME, ROLE_ADDRESS, mt.address);
                    ditem->setData(COL_NAME, ROLE_PLUGIN, plugin->name());
                    ditem->setData(COL_NAME, ROLE_LINE, quint32(line));
                    ditem->setFlags(ditem->flags() | Qt::ItemIsEditable);
                    ditem->setBackground(COL_NAME, QBrush(QColor(160, 96, 0, 40)));

                    for (int p = 0; p < mt.ports.count(); p++)
                    {
                        const quint32 pa = mt.ports.at(p);
                        QTreeWidgetItem *pt = new QTreeWidgetItem(ditem);
                        pt->setText(COL_NAME, tr("port %1").arg(p + 1));
                        pt->setText(COL_DETAIL, tr("universe %1").arg(portAddressText(pa)));
                        pt->setData(COL_NAME, ROLE_KIND, KIND_PORT);
                        pt->setData(COL_NAME, ROLE_PLUGIN, plugin->name());
                        pt->setData(COL_NAME, ROLE_LINE, quint32(line));
                        pt->setData(COL_NAME, ROLE_ADDRESS, mt.address);
                        pt->setData(COL_NAME, ROLE_PORTADDR, pa);
                        const QString key = QString("%1|%2").arg(mt.address).arg(pa);
                        if (portRows.contains(key) == false)
                            portRows.insert(key, pt);
                    }
                }
            }

            /* Feedback is a patch to THIS line as much as an output is, so a
               universe fed back here gets a row here -- otherwise a MIDI line
               lighting a control surface shows an empty interface and the
               relationship is only visible as a note on some ArtNet row
               elsewhere. The same universe legitimately appears twice: once
               under the port its DMX leaves by, once under the line its
               feedback returns on. Those are two different cables. */
            foreach (quint32 fbId, fbUnis)
            {
                Universe *fu = m_doc->inputOutputMap()
                               ? m_doc->inputOutputMap()->universe(fbId) : NULL;
                if (fu == NULL)
                    continue;
                QTreeWidgetItem *fitem = new QTreeWidgetItem(litem);
                fitem->setText(COL_NAME, tr("%1: %2").arg(fu->id() + 1).arg(fu->name()));
                fitem->setText(COL_DETAIL, tr("feedback"));
                fitem->setFlags(fitem->flags() | Qt::ItemIsEditable);
                fitem->setData(COL_NAME, ROLE_KIND, KIND_UNIVERSE);
                fitem->setData(COL_NAME, ROLE_UNIVERSE, fbId);
                fitem->setData(COL_NAME, ROLE_PLUGIN, plugin->name());
                fitem->setData(COL_NAME, ROLE_LINE, quint32(line));
                fitem->setData(COL_NAME, ROLE_OUTPUT, true);
            }

            /* Universes as rows, not as a summary string: they are the thing
               you patch and unpatch, so they need to be selectable. */
            InputOutputMap *iomap = m_doc->inputOutputMap();
            for (int pass = 0; pass < 2; pass++)
            {
                const bool output = (pass == 0);
                foreach (quint32 uniId, output ? outUnis : inUnis)
                {
                    Universe *uni = iomap ? iomap->universe(uniId) : NULL;
                    if (uni == NULL)
                        continue;
                    /* A universe belongs under the port it physically leaves
                       by, not beside it: "which socket does universe 5 come
                       out of" is the question this view exists to answer.
                       Only output patches carry a target; inputs and patches
                       aimed at a broadcast address (or at a node we have not
                       heard from) have no port to sit under, so they stay on
                       the interface. */
                    QTreeWidgetItem *parentItem = litem;
                    QString targetText;
                    if (output)
                    {
                        QString addr;
                        quint32 portAddr = 0;
                        if (patchTarget(uniId, plugin->name(), quint32(line),
                                        addr, portAddr))
                        {
                            QTreeWidgetItem *pr =
                                portRows.value(QString("%1|%2").arg(addr).arg(portAddr));
                            if (pr != NULL)
                            {
                                parentItem = pr;
                            }
                            else
                            {
                                /* Aimed at a node nothing has been heard from.
                                   The patch is real, so give the address a row
                                   of its own and hang the universe under it --
                                   the shape then matches a discovered node, and
                                   the difference between "configured" and
                                   "actually there" is a label rather than a
                                   different place in the tree. That difference
                                   is usually the thing being diagnosed: a
                                   universe under a silent node is precisely a
                                   node that is off, unplugged or misaddressed. */
                                QTreeWidgetItem *ghost = ghostNodes.value(addr);
                                if (ghost == NULL)
                                {
                                    ghost = new QTreeWidgetItem(litem);
                                    const QString galias = iomap
                                        ? iomap->targetAlias(addr) : QString();
                                    ghost->setText(COL_NAME, galias.isEmpty() ? addr
                                            : tr("%1 (%2)").arg(galias).arg(addr));
                                    ghost->setData(COL_NAME, ROLE_KIND, KIND_DEVICE);
                                    ghost->setData(COL_NAME, ROLE_ADDRESS, addr);
                                    ghost->setFlags(ghost->flags() | Qt::ItemIsEditable);
                                    /* Lead with the address, as a discovered
                                       node's row does, so both kinds of target
                                       read the same way and the only difference
                                       is whether it answered. */
                                    ghost->setText(COL_DETAIL,
                                        tr("%1 · not heard from").arg(addr));
                                    ghost->setBackground(COL_NAME,
                                        QBrush(QColor(245, 166, 35, 40)));
                                    ghost->setForeground(COL_DETAIL,
                                                         QBrush(QColor("#a06000")));
                                    ghostNodes.insert(addr, ghost);
                                }
                                /* Same three levels as a discovered node. A
                                   node we have heard from showed
                                   node -> port -> universe while a configured
                                   one showed node -> universe, so two things
                                   that are the same shape on the rig looked
                                   different here purely because one had
                                   answered. The port address is known from the
                                   patch, so there is no reason to flatten it. */
                                const QString gpKey =
                                    QString("%1|%2").arg(addr).arg(portAddr);
                                QTreeWidgetItem *gport = ghostPorts.value(gpKey);
                                if (gport == NULL)
                                {
                                    gport = new QTreeWidgetItem(ghost);
                                    /* Which socket on the box is not known --
                                       nothing has answered to say -- so no
                                       "port N" is invented from the address
                                       (that read "port 2" for universe 0:0:1
                                       while the patch said "port 1"). */
                                    gport->setText(COL_NAME,
                                        tr("universe %1").arg(portAddressText(portAddr)));
                                    gport->setData(COL_NAME, ROLE_KIND, KIND_PORT);
                                    gport->setData(COL_NAME, ROLE_PLUGIN, plugin->name());
                                    gport->setData(COL_NAME, ROLE_LINE, quint32(line));
                                    gport->setData(COL_NAME, ROLE_ADDRESS, addr);
                                    gport->setData(COL_NAME, ROLE_PORTADDR, portAddr);
                                    ghostPorts.insert(gpKey, gport);
                                }
                                parentItem = gport;
                            }
                        }
                    }

                    QTreeWidgetItem *uitem = new QTreeWidgetItem(parentItem);
                    uitem->setFlags(uitem->flags() | Qt::ItemIsEditable);
                    /* Name plus id: names are free-form and duplicated across
                       shows, and when someone is checking a patch against a
                       plot they need the authoritative number. */
                    uitem->setText(COL_NAME, tr("%1: %2").arg(uni->id() + 1)
                                                          .arg(uni->name()));
                    /* Show feedback and profile state on the row itself.
                       Both were previously only visible by opening the
                       per-universe editor, which meant the answer to "is LED
                       feedback actually wired up" required going somewhere
                       else. */
                    QStringList udetail;
                    udetail << (output ? tr("output") : tr("input"));
                    if (targetText.isEmpty() == false)
                        udetail << targetText;

                    /* Broadcast vs unicast changes what a patch means: a
                       broadcast target reaches every node on the segment and
                       has nothing to be "unreachable", while a unicast target
                       is a specific box that can be off. Same field, entirely
                       different diagnosis, so name it. */
                    if (output)
                    {
                        QString a;
                        quint32 pa = 0;
                        if (patchTarget(uniId, plugin->name(), quint32(line), a, pa))
                        {
                            const bool bcast = a.endsWith(QLatin1String(".255"))
                                               || a == QLatin1String("255.255.255.255");
                            udetail << (bcast ? tr("broadcast") : tr("unicast"));
                        }
                        else if (plugin->supportsOutputTargets())
                        {
                            /* patchTarget() returning false here means no
                               target was ever set -- which for a protocol
                               that HAS a target concept IS the broadcast
                               default (see its own "broadcast: no single
                               node to sit under" comment), not merely
                               nothing worth saying. Leaving the row blank
                               read as if broadcast were an absence of
                               choice rather than the choice actually in
                               effect. */
                            udetail << tr("broadcast");
                        }

                        /* Transmit mode belongs on the row it applies to: it is
                           the difference between sending on change and sending
                           every tick, which is orders of magnitude of packet
                           rate, and it is set per patch rather than globally. */
                        for (int i = 0; i < uni->outputPatchesCount(); i++)
                        {
                            OutputPatch *op = uni->outputPatch(i);
                            if (op != NULL && op->plugin() != NULL
                                    && op->plugin()->name() == plugin->name()
                                    && op->output() == quint32(line))
                            {
                                const QMap<QString, QVariant> pp =
                                    op->getPluginParameters();
                                if (pp.contains("transmitMode"))
                                    udetail << pp.value("transmitMode").toString().toLower();
                                break;
                            }
                        }
                    }
                    if (output == false)
                    {
                        InputPatch *ip = uni->inputPatch();
                        if (ip != NULL && ip->profileName().isEmpty() == false)
                            udetail << tr("profile: %1").arg(ip->profileName());
                    }
                    /* Deliberately NOT naming the feedback transport here.
                       Writing "feedback: MIDI" on a row sitting under an
                       Art-Net port reads as though MIDI travels over Art-Net --
                       and since MIDI over a network is a real thing (RTP-MIDI),
                       that is a plausible misreading rather than a pedantic
                       one. The feedback relationship is shown where it
                       physically lives: as a row under the MIDI line itself. */
                    uitem->setText(COL_DETAIL, udetail.join(" \u00b7 "));
                    uitem->setData(COL_NAME, ROLE_KIND, KIND_UNIVERSE);
                    uitem->setData(COL_NAME, ROLE_UNIVERSE, uniId);
                    uitem->setData(COL_NAME, ROLE_PLUGIN, plugin->name());
                    uitem->setData(COL_NAME, ROLE_LINE, quint32(line));
                    uitem->setData(COL_NAME, ROLE_OUTPUT, output);

                    /* Colour by whether the path is complete, so a glance
                       separates "this will output" from "this is aimed at
                       something that has never answered". Kept as a background
                       tint rather than text colour: the row already uses text
                       colour for the over-512 warning, and two meanings on one
                       channel is how colour coding stops meaning anything. */
                    if (parentItem != litem)
                    {
                        const bool heard = (parentItem->parent() != NULL)
                            && ghostNodes.values().contains(parentItem->parent()) == false;
                        uitem->setBackground(COL_NAME, QBrush(heard
                            ? QColor(46, 125, 50, 40)      // reachable
                            : QColor(245, 166, 35, 40)));  // configured, silent
                    }

                    /* Same usage figure the Overview grid shows: how full the
                       universe is. A patch row without it answers "where does
                       this go" but not "is there anything in it". */
                    setCarriesFixtures(uitem, uniId);

                    int nfx = 0, nheads = 0, lastCh = 0;
                    foreach (Fixture *fx, m_doc->fixtures())
                    {
                        if (fx == NULL || !fx->isPatched() || fx->universe() != uniId)
                            continue;
                        nfx++;
                        nheads += qMax(1, fx->heads());
                        lastCh = qMax(lastCh, int(fx->address() + fx->channels()));
                    }

                    /* The row can only ever show the ONE patch it sits under.
                       A universe fanned out to three nodes therefore appears
                       three times, each row honestly describing its own leg
                       and none of them saying there are others -- which is
                       exactly the thing you want to know before unpatching
                       one. The tooltip is where the whole universe fits. */
                    QList<QPair<QString, QString> > uprops;
                    typedef QPair<QString, QString> Prop;
                    uprops << Prop(tr("Universe"), QString::number(uniId + 1));
                    uprops << Prop(tr("Name"), uni->name());
                    for (int oi = 0; oi < uni->outputPatchesCount(); oi++)
                    {
                        OutputPatch *op = uni->outputPatch(oi);
                        if (op == NULL)
                            continue;
                        QString where = QString("%1: %2").arg(op->pluginName())
                                        .arg(op->outputName());
                        const QMap<QString, QVariant> pp = op->getPluginParameters();
                        if (pp.contains("outputIP"))
                            where += tr(" → %1").arg(pp.value("outputIP").toString());
                        if (pp.contains("outputUni"))
                            where += tr(" universe %1").arg(portAddressText(pp.value("outputUni").toUInt()));
                        uprops << Prop(uni->outputPatchesCount() > 1
                                       ? tr("Output %1").arg(oi + 1) : tr("Output"),
                                       where);
                    }
                    if (uni->outputPatchesCount() == 0)
                        uprops << Prop(tr("Output"), tr("none"));
                    if (uni->inputPatch() != NULL)
                    {
                        uprops << Prop(tr("Input"),
                                       QString("%1: %2")
                                           .arg(uni->inputPatch()->pluginName())
                                           .arg(uni->inputPatch()->inputName()));
                        if (uni->inputPatch()->profileName().isEmpty() == false)
                            uprops << Prop(tr("Profile"),
                                           uni->inputPatch()->profileName());
                    }
                    if (uni->feedbackPatch() != NULL)
                        uprops << Prop(tr("Feedback"),
                                       QString("%1: %2")
                                           .arg(uni->feedbackPatch()->pluginName())
                                           .arg(uni->feedbackPatch()->outputName()));
                    uprops << Prop(tr("Fixtures"), QString::number(nfx));
                    if (nheads != nfx)
                        uprops << Prop(tr("Heads"), QString::number(nheads));
                    uprops << Prop(tr("Channels used"),
                                   tr("%1 of 512").arg(lastCh));
                    setRowTooltip(uitem, propertyTooltip(
                        tr("%1: %2").arg(uniId + 1).arg(uni->name()), uprops));
                }
            }
        }
        /* A patch pending on THIS plugin (see OutputPatch::isPending()) has
           no real line to hang off -- that is the whole reason it is
           pending -- so give it a ghost one anyway, shaped exactly like a
           real interface row, marked as an error, right where a real one
           would sit. Before the emptiness check below: a protocol with
           nothing BUT pending patches must not have pitem deleted out from
           under the rows just added to it. */
        if (pendingByPluginThenInterface.contains(plugin->name()))
        {
            const QMap<QString, QList<Universe *> > &byIface =
                pendingByPluginThenInterface.value(plugin->name());
            for (QMap<QString, QList<Universe *> >::const_iterator it = byIface.constBegin();
                 it != byIface.constEnd(); ++it)
            {
                QTreeWidgetItem *ghostLine = new QTreeWidgetItem(pitem);
                ghostLine->setText(COL_NAME, it.key());
                QString ghostNet, heldWhy;
                foreach (Universe *u, it.value())
                    for (int i = 0; i < u->outputPatchesCount(); i++)
                        if (u->outputPatch(i) != NULL && u->outputPatch(i)->isPending())
                        {
                            if (ghostNet.isEmpty())
                                ghostNet = u->outputPatch(i)->pendingNetwork();
                            if (heldWhy.isEmpty())
                                heldWhy = u->outputPatch(i)->heldReason();
                        }
                /* Held by the rig network gate (InputOutputMap::
                   transmitGateActive()): the line may well be right here,
                   it is just not on one of the show's rig networks. */
                ghostLine->setText(COL_DETAIL, heldWhy.isEmpty() == false
                    ? tr("held: %1 — kept, resumes on the rig network").arg(heldWhy)
                    : ghostNet.isEmpty()
                    ? tr("not present — kept, will reconnect")
                    : tr("network %1 not present — kept, will reconnect").arg(ghostNet));
                const QBrush errBrush(QColor(220, 90, 90));
                ghostLine->setForeground(COL_NAME, errBrush);
                ghostLine->setForeground(COL_DETAIL, errBrush);
                setRowTooltip(ghostLine, tr(
                    "<b>%1</b><br>No interface on this machine reaches the network "
                    "these universes were patched to -- typically a laptop away from "
                    "the show network. The patches are kept exactly as they are and "
                    "saved unchanged; nothing is sent meanwhile. They reconnect by "
                    "themselves as soon as any interface on that network (or a route "
                    "to its targets) appears -- this machine joining the show VLAN, "
                    "or the show opened on another console there. No re-patching "
                    "needed.").arg(it.key().toHtmlEscaped()));

                /* Same target/port breakout a resolved patch on this
                   interface would show -- device, then port, with the
                   universe folded onto its port -- built from the SAME
                   plugin parameters (outputIP/outputUni) a resolved patch
                   reads. The interface being unreachable does not erase the
                   rest of the address: it is sitting right there on the
                   patch, unused until the network is back. One device row
                   per target address, same as the live/manual-target case
                   above folds multiple ports under one node. */
                QHash<QString, QTreeWidgetItem *> ghostDevices;
                foreach (Universe *u, it.value())
                {
                    OutputPatch *pendingOp = NULL;
                    for (int i = 0; i < u->outputPatchesCount(); i++)
                    {
                        OutputPatch *cand = u->outputPatch(i);
                        if (cand != NULL && cand->isPending())
                        { pendingOp = cand; break; }
                    }

                    const QMap<QString, QVariant> params = pendingOp != NULL
                        ? pendingOp->getPluginParameters() : QMap<QString, QVariant>();
                    const QString targetAddr = params.value("outputIP").toString();

                    QTreeWidgetItem *parentForUniverse = ghostLine;
                    if (targetAddr.isEmpty() == false)
                    {
                        QTreeWidgetItem *ditem = ghostDevices.value(targetAddr);
                        if (ditem == NULL)
                        {
                            ditem = new QTreeWidgetItem(ghostLine);
                            ditem->setText(COL_NAME, targetAddr);
                            ditem->setText(COL_DETAIL, tr("unreachable — interface not present"));
                            ditem->setForeground(COL_NAME, errBrush);
                            ditem->setForeground(COL_DETAIL, errBrush);
                            ghostDevices.insert(targetAddr, ditem);
                        }

                        const quint32 portAddr = params.value("outputUni", u->id()).toUInt();
                        QTreeWidgetItem *pt = new QTreeWidgetItem(ditem);
                        pt->setText(COL_NAME, tr("universe %1").arg(portAddressText(portAddr)));
                        pt->setForeground(COL_NAME, errBrush);
                        parentForUniverse = pt;
                    }

                    QTreeWidgetItem *uitem = new QTreeWidgetItem(parentForUniverse);
                    uitem->setFlags(uitem->flags() | Qt::ItemIsEditable);
                    uitem->setText(COL_NAME, tr("%1: %2").arg(u->id() + 1).arg(u->name()));
                    uitem->setText(COL_DETAIL, targetAddr.isEmpty() ? tr("output · broadcast")
                                                                     : tr("output"));
                    uitem->setData(COL_NAME, ROLE_KIND, KIND_PENDING_UNIVERSE);
                    uitem->setData(COL_NAME, ROLE_UNIVERSE, u->id());
                    setCarriesFixtures(uitem, u->id());
                }
                ghostLine->setText(COL_CARRIES, tr("%1 universe%2")
                                        .arg(it.value().count())
                                        .arg(it.value().count() == 1 ? QString() : QString("s")));
            }
        }

        /* A device a universe's input is patched to, away right now: its
           line, greyed and saying so, where it sits when it is here -- not
           silently missing. It turns back into the real line, universe and
           all, when the device reappears. */
        if (absentInputsByPluginThenDevice.contains(plugin->name()))
        {
            const QMap<QString, QList<Universe *> > &byDevice =
                absentInputsByPluginThenDevice.value(plugin->name());
            const QBrush absentBrush(QColor(138, 142, 153));
            for (QMap<QString, QList<Universe *> >::const_iterator it = byDevice.constBegin();
                 it != byDevice.constEnd(); ++it)
            {
                QTreeWidgetItem *ghostLine = new QTreeWidgetItem(pitem);
                ghostLine->setText(COL_NAME, it.key());
                ghostLine->setText(COL_DETAIL, tr("input · not connected — kept, reconnects by itself"));
                ghostLine->setForeground(COL_NAME, absentBrush);
                ghostLine->setForeground(COL_DETAIL, absentBrush);
                ghostLine->setData(COL_NAME, ROLE_LINENAME, it.key());
                ghostLine->setData(COL_NAME, ROLE_PLUGIN, plugin->name());
                setRowTooltip(ghostLine, tr(
                    "<b>%1</b><br>Not connected: switched off, asleep, out of range or "
                    "unplugged. Its universe stays patched to it -- profile and controller "
                    "roles too -- and picks it up again by itself the moment it is back, "
                    "on whichever line it then appears.").arg(it.key().toHtmlEscaped()));
                foreach (Universe *u, it.value())
                {
                    QTreeWidgetItem *uitem = new QTreeWidgetItem(ghostLine);
                    uitem->setFlags(uitem->flags() | Qt::ItemIsEditable);
                    uitem->setText(COL_NAME, tr("%1: %2").arg(u->id() + 1).arg(u->name()));
                    uitem->setText(COL_DETAIL, tr("input · not connected"));
                    uitem->setForeground(COL_NAME, absentBrush);
                    uitem->setForeground(COL_DETAIL, absentBrush);
                    uitem->setData(COL_NAME, ROLE_KIND, KIND_PENDING_UNIVERSE);
                    uitem->setData(COL_NAME, ROLE_UNIVERSE, u->id());
                    setCarriesFixtures(uitem, u->id());
                }
                ghostLine->setText(COL_CARRIES, tr("%1 universe%2")
                                        .arg(it.value().count())
                                        .arg(it.value().count() == 1 ? QString() : QString("s")));
            }
        }

        /* "Show unused" means what it says: unchecked hides an idle
           protocol entirely, same as it always hid an idle line -- that IS
           the decluttering the checkbox exists for, and it stays off by
           default on purpose. Reaching "Add an interface…"/"Add a
           target…" for a protocol with nothing on it yet is exactly what
           ticking the checkbox is FOR: reveal everything for as long as
           the session needs it, act on what was hidden, untick it again
           when done. That already works -- the fix here is not to bypass
           the checkbox, it is to make sure ticking it is the whole story. */
        if (pitem->childCount() == 0)
        {
            if (showAll == false)
                delete pitem;
            else
                pitem->setText(COL_DETAIL, tr("no lines"));
        }
    }

    /* Depth 2 so discovered devices AND their ports are visible: the port
       row is where the Net:Sub:Universe address lives, which is the whole
       reason for drilling in. */
    /* One port carrying one universe is the overwhelmingly common case, and
       giving it two rows -- a port row saying almost nothing, and a child row
       under it -- spends a whole level of indentation to state a one-to-one
       relationship. Fold the universe onto the port line and use the width
       instead. Ports with several universes keep their children: that is the
       collision case, it is a fault, and it must stay conspicuous rather than
       being flattened into a tidy single line. */
    {
        QList<QTreeWidgetItem *> stack;
        for (int i = 0; i < m_tree->topLevelItemCount(); i++)
            stack << m_tree->topLevelItem(i);
        while (stack.isEmpty() == false)
        {
            QTreeWidgetItem *it = stack.takeFirst();

            /* Decide about this row BEFORE queueing its children. Queueing
               first and then deleting a folded child left that child on the
               stack as a dangling pointer, which crashed on the next
               iteration. */
            QTreeWidgetItem *fold = NULL;
            if (it->data(COL_NAME, ROLE_KIND).toInt() == KIND_PORT
                    && it->childCount() == 1
                    && it->child(0)->data(COL_NAME, ROLE_KIND).toInt() == KIND_UNIVERSE)
                fold = it->child(0);

            for (int c = 0; c < it->childCount(); c++)
                if (it->child(c) != fold)
                    stack << it->child(c);

            if (fold == NULL)
                continue;
            QTreeWidgetItem *u = fold;

            /* Port address stays first in Detail -- it is the port's own
               identity -- with the universe's state appended after it. */
            const QString portAddr = it->text(COL_DETAIL);
            it->setText(COL_NAME, tr("%1 → %2").arg(it->text(COL_NAME))
                                               .arg(u->text(COL_NAME)));
            it->setText(COL_DETAIL, portAddr.isEmpty()
                        ? u->text(COL_DETAIL)
                        : QString("%1 · %2").arg(portAddr).arg(u->text(COL_DETAIL)));
            it->setText(COL_CARRIES, u->text(COL_CARRIES));
            it->setBackground(COL_NAME, u->background(COL_NAME));
            it->setForeground(COL_CARRIES, u->foreground(COL_CARRIES));

            /* Carry the universe's identity so the row still answers to the
               universe actions -- rename, unpatch, delete -- as well as the
               port ones. */
            it->setData(COL_NAME, ROLE_UNIVERSE, u->data(COL_NAME, ROLE_UNIVERSE));
            it->setData(COL_NAME, ROLE_OUTPUT, u->data(COL_NAME, ROLE_OUTPUT));
            delete it->takeChild(0);
        }
    }

    /* Roll each port's universes up into its Carries cell. A port row that
       says nothing while its children say plenty is the sort of gap that makes
       people distrust the whole view. Done after the tree is built, since the
       universes are attached as they are found. */
    {
        QList<QTreeWidgetItem *> stack;
        for (int i = 0; i < m_tree->topLevelItemCount(); i++)
            stack << m_tree->topLevelItem(i);
        while (stack.isEmpty() == false)
        {
            QTreeWidgetItem *it = stack.takeFirst();
            const int k = it->data(COL_NAME, ROLE_KIND).toInt();
            if ((k == KIND_PORT || k == KIND_DEVICE) && it->childCount() > 0)
            {
                QStringList names;
                for (int c = 0; c < it->childCount(); c++)
                    if (it->child(c)->data(COL_NAME, ROLE_KIND).toInt() == KIND_UNIVERSE)
                        names << it->child(c)->text(COL_NAME);
                if (names.isEmpty() == false)
                {
                    it->setText(COL_CARRIES, names.count() > 3
                                ? tr("%1 universes").arg(names.count())
                                : names.join(", "));

                    /* A DEVICE that carries something is wired up. Tint it, very
                       gently, so a patched node reads at a glance in a tree that
                       is otherwise a wall of identical rows -- the question being
                       asked of this page is usually "is that thing actually
                       patched", and answering it should not need reading the
                       third column of every line.
                     *
                       Deliberately faint: a strong fill would fight the
                       foreground colours this tree already uses to say things
                       that matter more (a red CARRIES means two universes are
                       colliding on one port). Subtle enough to scan, quiet
                       enough not to compete. */
                    if (k == KIND_DEVICE)
                    {
                        const QBrush wired(QColor(70, 150, 110, 34));
                        it->setBackground(COL_NAME, wired);
                        it->setBackground(COL_DETAIL, wired);
                        it->setBackground(COL_CARRIES, wired);
                    }

                    /* Two universes aimed at one physical DMX port is a fault,
                       not a layout: both streams arrive at the same socket and
                       whichever the node merges or drops is arbitrary. It is
                       also easy to create by accident and invisible in a
                       per-universe view, since each universe looks fine on its
                       own -- the collision only exists between them. */
                    if (k == KIND_PORT && names.count() > 1)
                    {
                        it->setForeground(COL_CARRIES, QBrush(QColor(220, 90, 90)));
                        it->setToolTip(COL_CARRIES,
                            tr("More than one universe is aimed at this port. "
                               "Both will be sent to the same physical DMX "
                               "output."));
                    }
                }
            }
            for (int c = 0; c < it->childCount(); c++)
                stack << it->child(c);
        }
    }

    /* Summarise each protocol on its own row. The top-level row previously
       said nothing at all, so the one line that is always visible when a
       branch is collapsed carried no information -- which is the line most
       worth reading when scanning for "is anything wrong over there". */

    /* Devices actually HEARD from, as opposed to configured-but-silent
       ghosts. Drives the banner below: "searching" should end when something
       has been found, not when a stopwatch says so. */
    int heardDevices = 0;
    /* Every universe id that already has a row somewhere in the tree above,
       so the "unpatched" pass below can tell what still needs one. */
    QSet<quint32> patchedUniverses;
    for (int i = 0; i < hostItem->childCount(); i++)
    {
        QTreeWidgetItem *top = hostItem->child(i);
        if (top->data(COL_NAME, ROLE_KIND).toInt() != KIND_PLUGIN)
            continue;

        int nodes = 0, unheard = 0;
        int ifaces = 0, unis = 0;
        QList<QTreeWidgetItem *> stack;
        for (int c = 0; c < top->childCount(); c++)
        {
            stack << top->child(c);
            ifaces++;
        }
        while (stack.isEmpty() == false)
        {
            QTreeWidgetItem *it = stack.takeFirst();
            const int k = it->data(COL_NAME, ROLE_KIND).toInt();
            if (k == KIND_DEVICE)
            {
                nodes++;
                if (it->text(COL_DETAIL).contains(tr("not heard from")))
                    unheard++;
            }
            /* A folded row is a port AND a universe, so count it as both. */
            if (k == KIND_UNIVERSE
                    || (k == KIND_PORT && it->data(COL_NAME, ROLE_UNIVERSE).isValid()))
            {
                unis++;
                const QVariant uv = it->data(COL_NAME, ROLE_UNIVERSE);
                if (uv.isValid())
                    patchedUniverses.insert(uv.toUInt());
            }
            /* Pending universes (see refresh()'s ghost-interface pass just
               above this ArtNet-shaped example) sit under this SAME
               protocol's row, real patches against it, just not routable
               yet -- a protocol row that carries nothing but pending
               patches must not roll up to "0 universes". Left out of
               patchedUniverses on purpose: pendingUniverseIds (computed
               earlier in refresh()) already excludes these from the
               "Unpatched" folder; this is a display count, not a second
               tracking set for the same thing. */
            else if (k == KIND_PENDING_UNIVERSE)
            {
                unis++;
            }
            for (int c = 0; c < it->childCount(); c++)
                stack << it->child(c);
        }

        QStringList parts;
        parts << tr("%1 interfaces").arg(ifaces);
        if (nodes > 0)
            parts << (unheard > 0 ? tr("%1 nodes (%2 silent)").arg(nodes).arg(unheard)
                                  : tr("%1 nodes").arg(nodes));
        top->setText(COL_DETAIL, parts.join(" · "));
        top->setText(COL_CARRIES, unis > 0 ? tr("%1 universes").arg(unis) : QString());
        if (unheard > 0)
            top->setForeground(COL_DETAIL, QBrush(QColor("#a06000")));

        heardDevices += (nodes - unheard);
    }

    /* A universe with nothing patched to it never appeared above at all --
       the tree is rooted at plugins, and an unpatched universe has no
       plugin/line/port to be a child of. Without this, Universe 1 (the one
       every new workspace starts with, patched to nothing) and any universe
       just added via "Add Universe" both looked like they had silently
       failed to appear. List what is left over once every id already
       drawn above is excluded -- patched (drawn under its real line),
       pending (drawn under its plugin's ghost line, just above), or
       genuinely never patched, which is what is left here. */
    if (m_doc->inputOutputMap() != NULL)
    {
        QList<Universe *> unpatched;
        foreach (Universe *u, m_doc->inputOutputMap()->universes())
        {
            if (u != NULL && patchedUniverses.contains(u->id()) == false
                    && pendingUniverseIds.contains(u->id()) == false)
                unpatched << u;
        }

        if (unpatched.isEmpty() == false)
        {
            QTreeWidgetItem *folder = new QTreeWidgetItem(hostItem);
            folder->setText(COL_NAME, tr("Unpatched"));
            folder->setText(COL_DETAIL, tr("not connected to any interface"));
            foreach (Universe *u, unpatched)
            {
                QTreeWidgetItem *uitem = new QTreeWidgetItem(folder);
                uitem->setFlags(uitem->flags() | Qt::ItemIsEditable);
                uitem->setText(COL_NAME, tr("%1: %2").arg(u->id() + 1).arg(u->name()));
                QStringList kept;
                foreach (const Universe::OfflinePatch &p, u->offlinePatches())
                    kept << tr("%1 %2 \"%3\"")
                            .arg(p.tag == Universe::InputPatchTag ? tr("input")
                                 : p.tag == Universe::FeedbackPatchTag ? tr("feedback")
                                 : tr("output"))
                            .arg(p.plugin, p.uid.isEmpty() ? p.plugin : p.uid);
                if (kept.isEmpty())
                {
                    uitem->setText(COL_DETAIL, tr("not patched"));
                }
                else
                {
                    uitem->setText(COL_DETAIL, tr("offline: %1 not present — kept, will reconnect")
                                                   .arg(kept.join(", ")));
                    uitem->setForeground(COL_DETAIL, QBrush(QColor(138, 142, 153)));
                    setRowTooltip(uitem, tr("This universe's patch names an interface, device "
                        "or plugin this machine does not have. It is kept and saved "
                        "unchanged, and reconnects by itself when it appears."));
                }
                uitem->setData(COL_NAME, ROLE_KIND, KIND_UNPATCHED_UNIVERSE);
                uitem->setData(COL_NAME, ROLE_UNIVERSE, u->id());
                setCarriesFixtures(uitem, u->id());
            }
            folder->setText(COL_CARRIES, tr("%1 universes").arg(unpatched.count()));
        }
    }

    /* An empty tree is ambiguous -- "nothing is connected" and "the view is
       broken" look identical. Say which. */
    if (hostItem->childCount() == 0)
    {
        QTreeWidgetItem *none = new QTreeWidgetItem(hostItem);
        /* Discovery is passive: nodes announce themselves about once a second,
           so for the first few seconds an empty tree means "not yet", not
           "nothing is there". Saying the latter immediately on opening the tab
           is simply wrong, and it is wrong at exactly the moment someone is
           deciding whether the rig is plugged in. */
        if (m_since->elapsed() < 6000)
        {
            none->setText(COL_NAME, tr("Searching…"));
            none->setText(COL_DETAIL,
                tr("Listening for devices announcing themselves."));
        }
        else
        {
            none->setText(COL_NAME, tr("Nothing connected"));
            none->setText(COL_DETAIL, showAll
                ? tr("No I/O plugins are available.")
                : tr("No interface has a universe patched to it and no devices "
                     "were heard. Tick \"Show unused\" to patch one."));
        }
        none->setFirstColumnSpanned(false);
    }

    /* Depth is protocol > interface > device > port > universe. Open the
       first two levels and stop: that is the whole shape of the rig on one
       screen -- every protocol, every interface, and the count of what hangs
       off each -- without the hundreds of port and universe rows that a real
       show expands to. Opening everything by default meant arriving at a wall
       of leaves and closing your way back out to find the question you came
       in with. Anything the operator opens is remembered and restored below,
       so this is the starting point, not a ceiling.

       Rows that did not exist on the previous pass are not in m_expanded and
       therefore arrive closed, which is the right default for a node that has
       just appeared. */
    addShowControlRows();

    QList<QTreeWidgetItem *> stack;
    QList<int> depths;
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
    {
        stack << m_tree->topLevelItem(i);
        depths << 0;
    }
    while (stack.isEmpty() == false)
    {
        QTreeWidgetItem *it = stack.takeFirst();
        const int depth = depths.takeFirst();
        if (it->childCount() > 0)
        {
            it->setExpanded(m_populatedOnce
                            ? m_expanded.contains(itemPath(it))
                            : depth <= 2);   // was <= 1; "This Host" adds one level
        }
        for (int c = 0; c < it->childCount(); c++)
        {
            stack << it->child(c);
            depths << depth + 1;
        }
    }
    m_knownLines = curLines;
    m_linesBaselined = true;
    m_populatedOnce = true;

    if (selectedPaths.isEmpty() == false || currentPath.isEmpty() == false)
    {
        QList<QTreeWidgetItem *> all;
        for (int i = 0; i < m_tree->topLevelItemCount(); i++)
            all << m_tree->topLevelItem(i);
        while (all.isEmpty() == false)
        {
            QTreeWidgetItem *it = all.takeFirst();
            const QString path = itemPath(it);
            if (path == currentPath)
                m_tree->setCurrentItem(it, 0, QItemSelectionModel::NoUpdate);
            if (selectedPaths.contains(path))
                it->setSelected(true);
            for (int c = 0; c < it->childCount(); c++)
                all << it->child(c);
        }
    }
    markOutputProblems();
    m_rebuilding = false;
    restoreScroll(scroll);

    /* The banner answers exactly one question -- "is an empty-looking tree
       still filling in?" -- so it belongs on screen only while that question
       is open. Keeping it up for a fixed six seconds after devices are
       already listed made a finished tree look unfinished, which is the same
       ambiguity it exists to remove, pointing the other way. */
    if (m_status != NULL)
        m_status->setVisible(heardDevices == 0 && m_since->elapsed() < 6000);
}

void ConnectionsTree::markOutputProblems()
{
    InputOutputMap *io = m_doc != NULL ? m_doc->inputOutputMap() : NULL;
    if (io == NULL)
        return;
    const bool byUniverse = m_byUniverse != NULL && m_byUniverse->isChecked();

    QList<QTreeWidgetItem *> stack;
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
        stack << m_tree->topLevelItem(i);
    while (stack.isEmpty() == false)
    {
        QTreeWidgetItem *it = stack.takeFirst();
        for (int c = 0; c < it->childCount(); c++)
            stack << it->child(c);

        const QVariant uv = it->data(COL_NAME, ROLE_UNIVERSE);
        const int kind = it->data(COL_NAME, ROLE_KIND).toInt();
        if (uv.isValid() == false || it->data(COL_NAME, ROLE_OUTPUT).toBool() == false
                || (kind != KIND_UNIVERSE && kind != KIND_PORT))
            continue;
        Universe *u = io->universe(uv.toUInt());
        if (u == NULL)
            continue;

        /* In the wiring tree a row IS one patch (plugin + line); in the
           universe list it is the universe, every leg of it. */
        const QString pluginName = it->data(COL_NAME, ROLE_PLUGIN).toString();
        const QVariant line = it->data(COL_NAME, ROLE_LINE);
        QStringList problems;
        for (int i = 0; i < u->outputPatchesCount(); i++)
        {
            OutputPatch *op = u->outputPatch(i);
            if (op == NULL || op->plugin() == NULL)
                continue;
            if (byUniverse == false
                    && (op->plugin()->name() != pluginName
                        || (line.isValid() && op->output() != line.toUInt())))
                continue;
            problems << op->plugin()->outputProblems(op->output(), u->id());
        }
        if (problems.isEmpty())
            continue;

        it->setText(COL_DETAIL, it->text(COL_DETAIL).isEmpty()
                    ? problems.join(QStringLiteral(" · "))
                    : QString("%1 · %2").arg(it->text(COL_DETAIL), problems.join(QStringLiteral(" · "))));
        it->setForeground(COL_DETAIL, QBrush(QColor(220, 90, 90)));
        QStringList escaped;
        foreach (const QString &p, problems)
            escaped << p.toHtmlEscaped();
        setRowTooltip(it, escaped.join(QStringLiteral("<br>")));
    }
}

/** Stable identity for a row: its name plus every ancestor's, so two rows that
 *  share a display name under different parents are not confused. */
/* Back to where the list was, both ways (x sideways, y down). The scroll
   range is only known once the new rows are laid out, which can be after
   this returns -- so again then. Sideways matters as much as down: the
   columns size to their contents, so emptying the list for the rebuild
   narrows the range and clamps a list scrolled right to read Detail. */
void ConnectionsTree::restoreScroll(const QPoint &value)
{
    m_tree->horizontalScrollBar()->setValue(value.x());
    m_tree->verticalScrollBar()->setValue(value.y());
    QPointer<QTreeWidget> tree(m_tree);
    QTimer::singleShot(0, this, [tree, value]() {
        if (tree == nullptr)
            return;
        if (tree->horizontalScrollBar()->value() != value.x())
            tree->horizontalScrollBar()->setValue(value.x());
        if (tree->verticalScrollBar()->value() != value.y())
            tree->verticalScrollBar()->setValue(value.y());
    });
}

QString ConnectionsTree::itemPath(QTreeWidgetItem *item)
{
    QStringList parts;
    while (item != NULL)
    {
        parts.prepend(item->text(COL_NAME));
        item = item->parent();
    }
    return parts.join("/");
}

/** Expand or collapse an item and everything beneath it. */
static void setExpandedDeep(QTreeWidgetItem *item, bool expanded)
{
    /* "Below" means the descendants. Collapsing the row you invoked it from
       makes the branch vanish from under the cursor, which is never what was
       asked for -- and leaves nothing to expand back from in place. */
    QList<QTreeWidgetItem *> stack;
    for (int c = 0; c < item->childCount(); c++)
        stack << item->child(c);
    item->setExpanded(true);
    while (stack.isEmpty() == false)
    {
        QTreeWidgetItem *it = stack.takeFirst();
        if (it->childCount() > 0)
            it->setExpanded(expanded);
        for (int c = 0; c < it->childCount(); c++)
            stack << it->child(c);
    }
}

/** True when this plugin can aim a patch at a node/port, not just a line. */
static bool pluginSupportsTargetsIn(Doc *doc, const QString &pluginName)
{
    if (doc == NULL || doc->ioPluginCache() == NULL)
        return false;
    foreach (QLCIOPlugin *p, doc->ioPluginCache()->plugins())
    {
        if (p != NULL && p->name() == pluginName)
            return p->supportsOutputTargets();
    }
    return false;
}

void ConnectionsTree::slotContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (item == NULL)
    {
        // Empty space — "add a universe" isn't a property of any existing
        // row, so this (previously a no-op) is exactly where it belongs.
        // Was a documented "read-only on purpose" surface; Branson asked
        // directly to be able to manage universes from here too.
        InputOutputMap *iom = m_doc->inputOutputMap();
        if (iom == NULL)
            return;
        /* Add only, same reasoning as the universal actions everywhere else
           in this menu (see appendUniversalMenuActions()): "Delete Universe
           'X'…" on a click that has nothing to do with X in particular reads
           as if THIS is what's being deleted, when it silently drops
           whichever universe is last regardless of where the click landed.
           Deleting already has a real, contextual home -- right-click that
           exact universe's own row. */
        QMenu menu(this);
        QAction *add = menu.addAction(QIcon(":/edit_add.png"), tr("Add Universe"));
        QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (chosen == add)
            addUniverse();
        return;
    }

    const int kind = item->data(COL_NAME, ROLE_KIND).toInt();
    // Show control is set up on the right, not by menu.
    if (kind == KIND_SHOWCONTROL || kind == KIND_SHOWCONTROL_BRANCH)
        return;
    const QString plugin = item->data(COL_NAME, ROLE_PLUGIN).toString();
    const quint32 line = item->data(COL_NAME, ROLE_LINE).toUInt();

    QMenu menu(this);

    /* The plural actions, offered above everything else and named with their
       own count so they can never be mistaken for the single-row entries
       below them. Everything else in this menu acts on the row under the
       pointer, whatever else happens to be selected. Handled centrally in
       handleUniversalMenuAction(), so every per-kind branch below honours
       them without having to remember to. */
    m_pluralRetarget = NULL;
    m_pluralPatch = NULL;
    m_pluralUnpatch = NULL;
    m_pluralUniverses = orderedSelectedUniverses();
    m_pluralLegs.clear();
    if (m_pluralUniverses.count() > 1)
    {
        /* "select 4 U and set patch to an artnet device that has 4 ports ..
           it should put them in selected order on those ports" -- and
           "select 4 U and unpatch .. it should remove all of them at the
           same time". Both used to act on the one row under the pointer. */
        m_pluralPatch = menu.addAction(
            UniversePatcher::patchSelectionText(m_pluralUniverses.count()));
        m_pluralLegs = selectedUnpatchLegs();
        QSet<quint32> withPatch;
        foreach (const UnpatchLeg &l, m_pluralLegs)
            withPatch.insert(l.universe);
        if (withPatch.isEmpty() == false)
            m_pluralUnpatch = menu.addAction(
                UniversePatcher::unpatchSelectionText(withPatch.count()));
    }
    {
        QString selPlug;
        QList<quint32> selLines;
        const QList<quint32> selUnis = selectedUniverses(selPlug, selLines);
        if (selUnis.count() > 1 && pluginSupportsTargetsIn(m_doc, selPlug))
            m_pluralRetarget = menu.addAction(tr("Retarget %1 selected universes…")
                                                  .arg(selUnis.count()));
    }
    const bool plural = (m_pluralPatch != NULL || m_pluralUnpatch != NULL
                         || m_pluralRetarget != NULL);
    if (plural)
        menu.addSeparator();

    /* Collapse/Expand and Add/Delete Universe are the same regardless of
       what got right-clicked, so they read as generic, root-level actions
       rather than something this particular row can do. Built here but not
       ADDED to the menu until appendUniversalMenuActions() runs, right
       before whichever exec() below actually fires -- so the menu reads
       top-down by hierarchy: what this exact row offers first, the generic
       stuff last, instead of the other way round. */
    QAction *collapseAll = NULL;
    QAction *expandAll = NULL;
    QAction *addUniv = NULL;
    QAction *delUniv = NULL;

    /* Resolved before the per-kind menus below, each of which exec()s its own
       menu and returns. Universe rows of every kind keep their own single-row
       actions under the plural ones. */
    if (plural && kind != KIND_UNIVERSE && kind != KIND_PORT
            && kind != KIND_UNPATCHED_UNIVERSE && kind != KIND_PENDING_UNIVERSE)
    {
        appendUniversalMenuActions(menu, item, collapseAll, expandAll, addUniv, delUniv);
        QAction *pick = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        handleUniversalMenuAction(pick, item, collapseAll, expandAll, addUniv, delUniv);
        return;
    }

    if (kind == KIND_HOST)
    {
        /* The one place "add a protocol" always works. A specific
           protocol's own row only exists to right-click once it already
           has something under it, or "Show unused" is ticked (see
           refresh()) -- deliberately, that is real decluttering, not a
           bug. This is the way around needing either: list every
           compiled-in protocol here, regardless of whether it currently
           has a row of its own, and reveal whichever one is chosen the
           same way that protocol's own "Add an interface…" would
           (revealInterface() is shared code, not a re-implementation). */
        QMenu *addProtoMenu = NULL;
        /* Keyed by DISPLAY name (see displayPluginName()) so the menu sorts
           the way it reads, not by the internal identity string underneath
           it -- and actionToPlugin resolves the choice back to a plugin by
           the QAction itself, not by re-matching its text, so a display
           name is free to differ from QLCIOPlugin::name() without breaking
           the lookup. */
        QMap<QString, QLCIOPlugin *> protoByDisplayName;
        if (m_doc->ioPluginCache() != NULL)
        {
            foreach (QLCIOPlugin *cand, m_doc->ioPluginCache()->plugins())
            {
                if (cand == NULL)
                    continue;
                const bool candOut = (cand->capabilities() & QLCIOPlugin::Output)
                                     && cand->outputs().isEmpty() == false;
                const bool candIn = (cand->capabilities() & QLCIOPlugin::Input)
                                    && cand->inputs().isEmpty() == false;
                if (candOut || candIn)
                    protoByDisplayName.insert(displayPluginName(cand->name()), cand);
            }
        }
        QMap<QAction *, QLCIOPlugin *> actionToPlugin;
        if (protoByDisplayName.isEmpty() == false)
        {
            addProtoMenu = menu.addMenu(tr("Add a protocol"));
            for (QMap<QString, QLCIOPlugin *>::const_iterator it = protoByDisplayName.constBegin();
                 it != protoByDisplayName.constEnd(); ++it)
                actionToPlugin.insert(addProtoMenu->addAction(it.key()), it.value());
        }

        appendUniversalMenuActions(menu, item, collapseAll, expandAll, addUniv, delUniv);
        QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (handleUniversalMenuAction(chosen, item, collapseAll, expandAll, addUniv, delUniv))
            return;
        if (chosen == NULL)
            return;
        if (actionToPlugin.contains(chosen))
            revealInterface(actionToPlugin.value(chosen));
        return;
    }

    if (kind == KIND_PLUGIN)
    {
        /* This level is protocol CRUD only: what interfaces (by IP/NIC) this
           protocol uses. Nothing about a specific target or a universe
           belongs here -- that starts one level down, on an interface row,
           and goes deeper from there. Everything below was previously
           reachable directly from here as a shortcut; removed so the menu
           at every level only offers what that level actually owns. */
        QLCIOPlugin *p = NULL;
        if (m_doc->ioPluginCache() != NULL)
        {
            foreach (QLCIOPlugin *cand, m_doc->ioPluginCache()->plugins())
                if (cand != NULL && cand->name() == plugin)
                    p = cand;
        }

        /* A network interface is a host NIC that exists whether or not this
           tree currently shows it -- refresh()'s liveness filter hides one
           with nothing patched or heard on it until something changes that.
           There is nothing to fabricate (the interface list IS whatever
           NICs the OS reports, see ArtNetPlugin::outputs()), only something
           to stop hiding, hence "Add" here really means "reveal and keep
           visible" -- m_pinnedLines, checked by that same filter. */
        const bool canOutHere = (p != NULL)
            && (p->capabilities() & QLCIOPlugin::Output) && p->outputs().isEmpty() == false;
        const bool canInHere = (p != NULL)
            && (p->capabilities() & QLCIOPlugin::Input) && p->inputs().isEmpty() == false;
        QAction *addIface = (canOutHere || canInHere)
            ? menu.addAction(tr("Add an interface…")) : NULL;

        /* "Create a connection, then patch a universe to it separately" --
           the same thing "Add a target on this interface…" already does,
           just reachable without first revealing (or already knowing)
           which interface row to right-click. Resolves the interface
           itself (the only one, or asks -- same as "Add an interface…",
           and pins it visible the same way) before asking for the target
           address, so this is genuinely "add interface + add target" in
           one step, not a shortcut around either. Target-capable protocols
           only: a DMX-USB/MIDI line has no target to declare, the line
           itself IS the destination. */
        QAction *addTgtHere = (canOutHere && p != NULL && p->supportsOutputTargets())
            ? menu.addAction(tr("Add a target…")) : NULL;

        /* Superseded by per-row CRUD everywhere below: this dialog is a
           second, differently-shaped editor for the exact same target IP /
           ArtNet universe number / transmit mode that a target or port row
           already edits directly, and now duplicates a strict-hierarchy
           tree instead of standing in front of it. Kept for plugins with
           no per-patch equivalent at all -- DMX-USB/MIDI widget settings
           (speed, mode, ...) are genuinely per PHYSICAL WIDGET, not
           per-patch, and have no tree row to live on. */
        QAction *cfg = (p != NULL && p->canConfigure() && p->supportsOutputTargets() == false)
            ? menu.addAction(tr("Configure %1…").arg(plugin)) : NULL;

        appendUniversalMenuActions(menu, item, collapseAll, expandAll, addUniv, delUniv);
        QAction *pick0 = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (handleUniversalMenuAction(pick0, item, collapseAll, expandAll, addUniv, delUniv))
            return;
        if (pick0 == NULL)
            return;   // menu dismissed -- cfg may be NULL too, and must not "match" this
        if (pick0 == cfg)
            configurePlugin(plugin);
        else if (addIface != NULL && pick0 == addIface)
            revealInterface(p);
        else if (addTgtHere != NULL && pick0 == addTgtHere)
            addTargetOnNewInterface(p);
        return;
    }

    if (kind == KIND_DEVICE)
    {
        const QString addr = item->data(COL_NAME, ROLE_ADDRESS).toString();
        if (addr.isEmpty())
            return;
        InputOutputMap *iom = m_doc->inputOutputMap();
        const bool manual = (iom != NULL && iom->isManualTarget(addr));

        QAction *ren = menu.addAction(tr("Name this target…"));
        /* Network settings of a discovered device (an Art-Net node's IP) --
           the plugin owns the dialog and its confirmation. */
        QTreeWidgetItem *lineItem = item->parent();
        QLCIOPlugin *devPlugin = (lineItem != NULL && m_doc->ioPluginCache() != NULL)
            ? m_doc->ioPluginCache()->plugin(lineItem->data(COL_NAME, ROLE_PLUGIN).toString())
            : NULL;
        QAction *setIp = (devPlugin != NULL && devPlugin->canConfigureDevice(addr))
            ? menu.addAction(tr("Set node IP address…")) : NULL;
        /* Ports can only be added to a target we invented. A discovered node
           reports its own ports in every ArtPollReply, so a hand-added port
           there would be overwritten within seconds -- offering it would be
           offering something that cannot stick. */
        QAction *addPort = manual
            ? menu.addAction(tr("Add a port…")) : NULL;
        QAction *forget = NULL;
        if (manual)
        {
            menu.addSeparator();
            forget = menu.addAction(tr("Forget this target…"));
        }

        appendUniversalMenuActions(menu, item, collapseAll, expandAll, addUniv, delUniv);
        QAction *pick1 = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (handleUniversalMenuAction(pick1, item, collapseAll, expandAll, addUniv, delUniv))
            return;
        if (pick1 == ren)
            renameTarget(addr);
        else if (setIp != NULL && pick1 == setIp)
        {
            devPlugin->configureDevice(addr);
            refresh();
        }
        else if (addPort != NULL && pick1 == addPort)
            addManualPort(addr);
        else if (forget != NULL && pick1 == forget)
            forgetTarget(addr);
        return;
    }

    if (kind == KIND_PORT)
    {
        const QString addr = item->data(COL_NAME, ROLE_ADDRESS).toString();
        const quint32 portAddr = item->data(COL_NAME, ROLE_PORTADDR).toUInt();

        /* A folded row is a port AND the universe on it, so it has to answer
           to both sets of actions -- otherwise collapsing the rows would have
           quietly removed the ability to rename or unpatch. */
        const QVariant uniVar = item->data(COL_NAME, ROLE_UNIVERSE);
        if (uniVar.isValid())
        {
            const quint32 uni = uniVar.toUInt();
            QAction *ren = menu.addAction(tr("Rename universe…"));
            QAction *retgt = (item->data(COL_NAME, ROLE_OUTPUT).toBool()
                              && pluginSupportsTargetsIn(m_doc, plugin))
                ? menu.addAction(UniversePatcher::retargetText(plugin)) : NULL;
            QAction *unp = menu.addAction(tr("Unpatch from this interface"));
            menu.addSeparator();
            QAction *del = menu.addAction(tr("Delete universe entirely…"));
            appendUniversalMenuActions(menu, item, collapseAll, expandAll, addUniv, delUniv);
            QAction *c = menu.exec(m_tree->viewport()->mapToGlobal(pos));
            if (handleUniversalMenuAction(c, item, collapseAll, expandAll, addUniv, delUniv))
                return;
            if (retgt != NULL && c == retgt)
                retargetPatch(uni, plugin, line);
            else if (c == ren)
                renameUniverse(uni);
            else if (c == unp)
                unpatchFromLine(uni, plugin, line,
                                item->data(COL_NAME, ROLE_OUTPUT).toBool());
            else if (c == del)
                deleteUniverse(uni);
            return;
        }

        InputOutputMap *iom = m_doc->inputOutputMap();
        const bool manual = (iom != NULL && iom->isManualTarget(addr));

        QAction *p = menu.addAction(tr("Patch a universe to this port…"));
        QAction *rmPort = NULL;
        if (manual)
        {
            menu.addSeparator();
            rmPort = menu.addAction(tr("Remove this port"));
        }
        appendUniversalMenuActions(menu, item, collapseAll, expandAll, addUniv, delUniv);
        QAction *pick2 = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (handleUniversalMenuAction(pick2, item, collapseAll, expandAll, addUniv, delUniv))
            return;
        if (pick2 == p)
            patchUniverseToPort(plugin, line, addr, portAddr);
        else if (rmPort != NULL && pick2 == rmPort)
        {
            iom->removeManualPort(addr, portAddr);
            m_doc->setModified();
            refresh();
        }
        return;
    }

    if (kind == KIND_LINE)
    {
        /* Only offer what this line can actually carry. A plugin advertises
           its capabilities and its input/output line lists separately, and
           offering an output patch on an input-only line produced a menu entry
           that silently did nothing. */
        QLCIOPlugin *p = NULL;
        if (m_doc->ioPluginCache() != NULL)
        {
            foreach (QLCIOPlugin *cand, m_doc->ioPluginCache()->plugins())
                if (cand != NULL && cand->name() == plugin)
                    p = cand;
        }
        if (p == NULL)
            return;

        const bool canOut = (p->capabilities() & QLCIOPlugin::Output)
                            && int(line) < p->outputs().count();
        const bool canIn = (p->capabilities() & QLCIOPlugin::Input)
                           && int(line) < p->inputs().count();

        /* "Add a target" is how you describe hardware this machine cannot
           discover for itself: a node on a subnet we have no interface on, so
           a broadcast poll never reaches it and it never replies, or a node
           that simply is not plugged in yet. Note what this does NOT claim to
           do -- it cannot create a local interface, because the interface
           list is whatever NICs exist. Reaching a remote node is a unicast
           destination on a patch (ARTNET_OUTPUTIP), which is a property of
           the target, not a new line. So the target is the thing you add,
           and it hangs under the interface the traffic leaves by. */
        /* Both of these ask "which node, and which port on it", which is only
           a question Art-Net can answer -- a patch there carries outputIP and
           outputUni. A DMX USB line has no far end to name: the line IS the
           widget and the cable IS the destination, so prompting for a target
           address was asking something unanswerable and then storing the
           answer where nothing would ever read it. */
        const bool targets = canOut && p->supportsOutputTargets();

        QAction *nameLine = menu.addAction(tr("Name this interface…"));

        /* This level is interface CRUD only, scoped to targets -- a target
           is the thing that hangs off an interface, same as a universe hangs
           off a target's port. Patching a universe directly from here used
           to be offered as a shortcut; removed for a target-capable
           protocol so patching always goes interface -> target -> port,
           never skipping a level. A line with no target concept at all
           (DMX-USB, MIDI: the line IS the endpoint, there is nothing
           further down) keeps patching here instead, since for those
           this IS the bottom of the hierarchy. */
        QAction *addTgt = targets
            ? menu.addAction(tr("Add a target on this interface…")) : NULL;
        QAction *addTgtLib = targets
            ? menu.addAction(tr("Add a target from device library…")) : NULL;
        QAction *pOut = (canOut && targets == false)
            ? menu.addAction(tr("Patch a universe here (output)…")) : NULL;
        QAction *pIn = (canIn && targets == false)
            ? menu.addAction(tr("Patch a universe here (input)…")) : NULL;

        /* "This got bound to lo0 by mistake, move every universe on it to
           the real NIC" used to mean re-patching each affected universe by
           hand, one at a time, through Overview or Detailed. Everything
           else about each patch (target IP, transmit mode, ...) lives on
           the OutputPatch object itself and survives a plain line change
           untouched -- Universe::setOutputPatch() reuses the same patch and
           only updates plugin+line, see OutputPatch::set() -- so only the
           line index needs touching here, for every affected universe at
           once. */
        const QList<quint32> boundHere = canOut ? universesOn(plugin, line, true) : QList<quint32>();
        QAction *reroute = boundHere.isEmpty() == false
            ? menu.addAction(tr("Reroute %1 universe%2 to a different interface…")
                                 .arg(boundHere.count())
                                 .arg(boundHere.count() == 1 ? QString() : QString("s")))
            : NULL;

        /* Symmetric "D" for the protocol row's "Add an interface": once a
           pinned interface is idle again -- nothing patched, nothing heard
           -- it can go back to being hidden by default rather than staying
           pinned forever. Only offered when there is nothing here to lose:
           if anything is genuinely live, the liveness filter keeps it
           visible regardless of the pin, so unpinning would be a no-op. */
        const QString lineName = item->data(COL_NAME, ROLE_LINENAME).toString();
        const bool isPinned = m_pinnedLines.contains(plugin + "|" + lineName);
        const bool idle = boundHere.isEmpty()
            && (canIn == false || universesOn(plugin, line, false).isEmpty());
        QAction *unpin = (isPinned && idle)
            ? menu.addAction(tr("Stop showing this interface")) : NULL;

        appendUniversalMenuActions(menu, item, collapseAll, expandAll, addUniv, delUniv);
        QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (handleUniversalMenuAction(chosen, item, collapseAll, expandAll, addUniv, delUniv))
            return;
        if (chosen == nameLine)
            renameLine(plugin, lineName);
        else if (reroute != NULL && chosen == reroute)
            rerouteLine(plugin, line, boundHere);
        else if (unpin != NULL && chosen == unpin)
        {
            m_pinnedLines.remove(plugin + "|" + lineName);
            refresh();
        }
        else if (addTgt != NULL && chosen == addTgt)
            addManualTarget(plugin, line);
        else if (addTgtLib != NULL && chosen == addTgtLib)
            addTargetFromLibrary(plugin, line);
        else if (pOut != NULL && chosen == pOut)
            patchUniverseTo(plugin, line, true);
        else if (pIn != NULL && chosen == pIn)
            patchUniverseTo(plugin, line, false);
        return;
    }

    if (kind == KIND_UNPATCHED_UNIVERSE)
    {
        /* No plugin/line/port context exists for this row -- that is the
           whole reason it is here -- so "Patch to..." asks for everything
           the interface -> target -> port chain would normally have
           supplied by being clicked through: which protocol, which
           interface, and (for a target-capable protocol) which address and
           port. Same destination as patching from a port row, just reached
           by starting from the universe instead of from the wire. */
        const quint32 uni = item->data(COL_NAME, ROLE_UNIVERSE).toUInt();
        QAction *patchTo = menu.addAction(tr("Edit connection…"));
        menu.setDefaultAction(patchTo);
        menu.addSeparator();
        QAction *ren = menu.addAction(tr("Rename universe…"));
        menu.addSeparator();
        QAction *del = menu.addAction(tr("Delete universe entirely…"));
        appendUniversalMenuActions(menu, item, collapseAll, expandAll, addUniv, delUniv);
        QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (handleUniversalMenuAction(chosen, item, collapseAll, expandAll, addUniv, delUniv))
            return;
        if (chosen == patchTo)
            editConnection(item);
        else if (chosen == ren)
            renameUniverse(uni);
        else if (chosen == del)
            deleteUniverse(uni);
        return;
    }

    if (kind == KIND_PENDING_UNIVERSE)
    {
        /* This universe IS patched -- see refresh()'s "Interface not
           present" folder -- just to an interface not currently reachable.
           Nothing here should look like ordinary patch editing (retarget,
           transmit mode, ...): none of that can mean anything until the
           interface exists again, and offering it invited configuring a
           destination that has no effect. Only "give up on it" is real. */
        const quint32 uni = item->data(COL_NAME, ROLE_UNIVERSE).toUInt();
        /* Patch it to an interface that IS here: the connection window,
           which says what it is waiting for and replaces it. */
        QAction *editConn = menu.addAction(tr("Edit connection…"));
        menu.addSeparator();
        QAction *ren = menu.addAction(tr("Rename universe…"));
        menu.addSeparator();
        QAction *forget = menu.addAction(tr("Forget this patch (unpatch)…"));
        menu.addSeparator();
        QAction *del = menu.addAction(tr("Delete universe entirely…"));
        appendUniversalMenuActions(menu, item, collapseAll, expandAll, addUniv, delUniv);
        QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (handleUniversalMenuAction(chosen, item, collapseAll, expandAll, addUniv, delUniv))
            return;
        if (chosen == editConn)
            editConnection(item);
        else if (chosen == ren)
            renameUniverse(uni);
        else if (chosen == forget)
            forgetPendingPatch(uni);
        else if (chosen == del)
            deleteUniverse(uni);
        return;
    }

    if (kind == KIND_UNIVERSE)
    {
        const quint32 uni = item->data(COL_NAME, ROLE_UNIVERSE).toUInt();
        const bool output = item->data(COL_NAME, ROLE_OUTPUT).toBool();

        /* Everything about this patch -- where it goes, the interface, the
           protocol's own settings (target, transmit mode, MIDI mode, sACN
           priority...), the universe's input, profile, feedback and
           passthrough -- is one window, filled in: "Edit connection…". It
           used to be a dozen entries here, each its own small dialog. */
        QAction *editConn = menu.addAction(tr("Edit connection…"));
        menu.setDefaultAction(editConn);
        menu.addSeparator();
        QAction *ren = menu.addAction(tr("Rename universe…"));
        /* Wording matters here: a universe can be patched to several lines, so
           removing it from THIS one must not read as deleting the universe.
           Conflating the two would quietly destroy a fan-out. */
        QAction *unp = menu.addAction(tr("Unpatch from this interface"));
        menu.addSeparator();
        QAction *del = menu.addAction(tr("Delete universe entirely…"));
        appendUniversalMenuActions(menu, item, collapseAll, expandAll, addUniv, delUniv);
        QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (handleUniversalMenuAction(chosen, item, collapseAll, expandAll, addUniv, delUniv))
            return;
        if (chosen == NULL)
            return;
        if (chosen == editConn)
            editConnection(item);
        else if (chosen == ren)
            renameUniverse(uni);
        else if (chosen == unp)
            unpatchFromLine(uni, plugin, line, output);
        else if (chosen == del)
            deleteUniverse(uni);
        return;
    }

    /* Falls through here for a row with no per-kind menu of its own -- the
       "Unpatched" folder heading is the one example today, added by
       refresh() with no ROLE_KIND at all since it is not a plugin/line/
       device/universe, just a grouping label. Every kind branch above
       exec()s the menu itself and returns; without this, a kind that
       matches none of them built a menu and never showed it -- right-
       clicking the row did nothing, silently. */
    appendUniversalMenuActions(menu, item, collapseAll, expandAll, addUniv, delUniv);
    QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    handleUniversalMenuAction(chosen, item, collapseAll, expandAll, addUniv, delUniv);
}

bool ConnectionsTree::isUniverseRow(const QTreeWidgetItem *item)
{
    if (item == NULL)
        return false;
    const int kind = item->data(COL_NAME, ROLE_KIND).toInt();
    return kind == KIND_UNIVERSE || kind == KIND_UNPATCHED_UNIVERSE
           || kind == KIND_PENDING_UNIVERSE;
}

bool ConnectionsTree::editConnection(QTreeWidgetItem *item)
{
    if (isUniverseRow(item) == false)
        return false;
    const quint32 uni = item->data(COL_NAME, ROLE_UNIVERSE).toUInt();
    QString plugin;
    quint32 line = 0;
    QString section;
    if (item->data(COL_NAME, ROLE_KIND).toInt() == KIND_UNIVERSE)
    {
        /* An output row: that leg. An input or feedback row: the universe's
           connection, opened on its input / feedback. */
        if (item->data(COL_NAME, ROLE_OUTPUT).toBool()
                && item->text(COL_DETAIL) != tr("feedback"))
        {
            plugin = item->data(COL_NAME, ROLE_PLUGIN).toString();
            line = item->data(COL_NAME, ROLE_LINE).toUInt();
        }
        else
            section = item->text(COL_DETAIL) == tr("feedback") ? QStringLiteral("feedback")
                                                                : QStringLiteral("input");
    }
    const bool changed = ConnectionEditor::edit(m_doc, uni, plugin, line, this, section);
    if (changed)
        refresh();
    return changed;
}

void ConnectionsTree::slotItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    if (isUniverseRow(item))
        editConnection(item);
}

bool ConnectionsTree::pickPluginLine(QLCIOPlugin *p, bool output, quint32 &lineOut,
                                     int current)
{
    return m_patcher->pickPluginLine(p, output, lineOut, current);
}

void ConnectionsTree::revealInterface(QLCIOPlugin *p)
{
    if (p == NULL)
        return;

    const bool candOut = (p->capabilities() & QLCIOPlugin::Output) && p->outputs().isEmpty() == false;
    const bool candIn = (p->capabilities() & QLCIOPlugin::Input) && p->inputs().isEmpty() == false;

    bool useOutput = candOut;
    if (candOut && candIn)
    {
        bool ok = false;
        QStringList dirs;
        dirs << tr("Output") << tr("Input");
        const QString d = QInputDialog::getItem(this, tr("Add an interface"),
            tr("Output or input?"), dirs, 0, false, &ok);
        if (ok == false)
            return;
        useOutput = (d == dirs.at(0));
    }

    quint32 ln = 0;
    if (pickPluginLine(p, useOutput, ln) == false)
        return;

    const QStringList lines = useOutput ? p->outputs() : p->inputs();
    const QString label = lines.value(int(ln));
    if (label.isEmpty() == false)
    {
        m_pinnedLines.insert(p->name() + "|" + label);
        refresh();
    }
}

void ConnectionsTree::addTargetOnNewInterface(QLCIOPlugin *p)
{
    if (p == NULL)
        return;

    quint32 ln = 0;
    if (pickPluginLine(p, true, ln) == false)
        return;

    const QString label = p->outputs().value(int(ln));
    if (label.isEmpty() == false)
        m_pinnedLines.insert(p->name() + "|" + label);
    addManualTarget(p->name(), ln);   // prompts for the address; refreshes itself
}

void ConnectionsTree::patchUniverseTo(const QString &pluginName, quint32 line,
                                      bool output)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    QStringList names;
    QList<quint32> ids;
    foreach (Universe *uni, iomap->universes())
    {
        names << uni->name();
        ids << uni->id();
    }
    const QString newLabel = tr("<new universe>");
    names << newLabel;

    bool ok = false;
    const QString pick = QInputDialog::getItem(
        this, tr("Patch universe"),
        tr("Patch which universe to %1?").arg(pluginName),
        names, 0, false, &ok);
    if (ok == false || pick.isEmpty())
        return;

    quint32 uniId = InputOutputMap::invalidUniverse();
    if (pick == newLabel)
    {
        /* Creating a universe changes the LIST, so the whole list is held --
           a patch-only capture could not undo the creation itself. */
        if (iomap->patchUndo() != NULL)
            iomap->patchUndo()->captureUniverses(tr("add a universe"));
        if (iomap->addUniverse() == false)
            return;
        QList<Universe *> all = iomap->universes();
        if (all.isEmpty())
            return;
        uniId = all.last()->id();
    }
    else
    {
        const int idx = names.indexOf(pick);
        if (idx < 0 || idx >= ids.count())
            return;
        uniId = ids.at(idx);
    }

    bool done = false;
    QString targetAddr;
    quint32 targetPort = 0;
    if (output && pluginSupportsTargetsIn(m_doc, pluginName))
    {
        /* Where on this interface it goes, asked before anything changes. */
        if (askTargetPort(tr("Patch universe"), pluginName, line, uniId, true,
                          targetAddr, targetPort) == false)
        {
            refresh();
            return;
        }
    }
    if (output)
    {
        /* Replace, not append.
         *
           This used to add a patch at outputPatchesCount(), on the reasoning
           that a universe may legitimately fan out to several nodes. It can, in
           the engine -- but every other surface shows and edits ONE output per
           universe, so a universe quietly grew a second interface here and
           nothing else in the app ever mentioned it again. A universe now sends
           to one line, which is also what the engine enforces, so appending
           would simply be refused. */
        done = iomap->setOutputPatch(uniId, pluginName, QString(), line, false, 0);
        if (done && targetAddr.isEmpty() == false)
            applyTarget(uniId, pluginName, line, targetAddr, targetPort);
    }
    else
    {
        /* A universe has exactly one input patch, but nothing in the engine
           stops two universes subscribing to the SAME line -- and then every
           event arrives twice, once per universe, with nothing on screen
           explaining why one button press fired two cues. For MIDI that is
           essentially always a mistake; for other input protocols it is at
           least worth knowing. Warn rather than forbid: the engine permits it,
           and inventing a rule the rest of the app does not enforce would only
           move the surprise somewhere else. */
        const QList<quint32> clash = iomap->universesWithInputOn(pluginName, line);
        QStringList clashNames;
        foreach (quint32 cid, clash)
        {
            if (cid == uniId)
                continue;
            Universe *cu = iomap->universe(cid);
            clashNames << (cu != NULL ? tr("%1: %2").arg(cu->id() + 1).arg(cu->name())
                                      : tr("universe %1").arg(cid + 1));
        }
        if (clashNames.isEmpty() == false)
        {
            if (QMessageBox::warning(this, tr("Patch universe"),
                    tr("%1 already feeds %2.\n\nEvery message on this input "
                       "will arrive on both universes, firing anything bound "
                       "to it twice. Patch it anyway?")
                        .arg(pluginName).arg(clashNames.join(", ")),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                    != QMessageBox::Yes)
            {
                refresh();
                return;
            }
        }
        done = iomap->setInputPatch(uniId, pluginName, QString(), line);
    }

    if (done == false)
        QMessageBox::warning(this, tr("Patch universe"),
                             tr("Could not patch that universe to %1.").arg(pluginName));
    else
        m_doc->setModified();
    refresh();
}

void ConnectionsTree::forgetPendingPatch(quint32 universe)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *uni = iomap ? iomap->universe(universe) : NULL;
    if (uni == NULL)
        return;

    if (QMessageBox::question(this, tr("Forget patch"),
            tr("Stop waiting for this universe's interface and unpatch it?\n\n"
               "The interface itself is unaffected -- this only clears what "
               "this universe was pointed at."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    if (iomap->patchUndo() != NULL)
        iomap->patchUndo()->capture(QList<quint32>() << universe,
                                    tr("forget pending patch on universe %1").arg(universe + 1));

    // Pending outputs and any input/feedback patch kept offline alike.
    iomap->forgetOfflinePatches(universe);

    m_doc->setModified();
    refresh();
}

void ConnectionsTree::unpatchFromLine(quint32 universe, const QString &pluginName,
                                      quint32 line, bool output)
{
    Q_UNUSED(pluginName)
    Q_UNUSED(line)

    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    if (iomap->patchUndo() != NULL)
        iomap->patchUndo()->capture(QList<quint32>() << universe,
                                    tr("unpatch universe %1").arg(universe + 1));

    /* Patching to an empty plugin name is how this map clears a patch. */
    if (output)
    {
        Universe *uni = iomap->universe(universe);
        int index = 0;
        if (uni != NULL)
        {
            for (int i = 0; i < uni->outputPatchesCount(); i++)
            {
                OutputPatch *op = uni->outputPatch(i);
                if (op != NULL && op->plugin() != NULL
                        && op->plugin()->name() == pluginName && op->output() == line)
                {
                    index = i;
                    break;
                }
            }
        }
        iomap->setOutputPatch(universe, QString(), QString(), 0, false, index);
    }
    else
    {
        iomap->setInputPatch(universe, QString(), QString(), 0);
    }
    m_doc->setModified();
    refresh();
}

void ConnectionsTree::renameUniverse(quint32 universe)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *uni = iomap ? iomap->universe(universe) : NULL;
    if (uni == NULL)
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Rename universe"), tr("Universe name:"),
        QLineEdit::Normal, uni->name(), &ok);
    if (ok == false || name.isEmpty())
        return;

    iomap->setUniverseName(int(universe), name);
    m_doc->setModified();
    refresh();
}

bool ConnectionsTree::handleUniversalMenuAction(QAction *chosen, QTreeWidgetItem *item,
    QAction *collapseAll, QAction *expandAll, QAction *addUniv, QAction *delUniv)
{
    if (chosen == NULL)
        return false;
    if (m_pluralRetarget != NULL && chosen == m_pluralRetarget)
    {
        retargetSelection(QString());
        return true;
    }
    if (m_pluralPatch != NULL && chosen == m_pluralPatch)
    {
        patchSelectionTo(m_pluralUniverses);
        return true;
    }
    if (m_pluralUnpatch != NULL && chosen == m_pluralUnpatch)
    {
        unpatchLegs(m_pluralLegs);
        return true;
    }
    if (chosen == collapseAll || chosen == expandAll)
    {
        setExpandedDeep(item, chosen == expandAll);
        return true;
    }
    if (chosen == addUniv)
    {
        addUniverse();
        return true;
    }
    if (delUniv != NULL && chosen == delUniv)
    {
        InputOutputMap *iomap = m_doc->inputOutputMap();
        if (iomap != NULL)
            deleteUniverse(iomap->universesCount() - 1);
        return true;
    }
    return false;
}

void ConnectionsTree::appendUniversalMenuActions(QMenu &menu, QTreeWidgetItem *item,
    QAction *&collapseAll, QAction *&expandAll, QAction *&addUniv, QAction *&delUniv)
{
    // delUniv stays NULL from here (see the comment on addUniv below) -- kept
    // as a parameter purely so callers share one signature with
    // handleUniversalMenuAction() and don't need their own special case.
    Q_UNUSED(delUniv)

    if (menu.isEmpty() == false)
        menu.addSeparator();

    /* Deep branches are quick to open and tedious to close one triangle at a
       time, and the plugin and device rows had no menu at all -- so the rows
       most worth collapsing were the ones you could not act on. */
    if (item->childCount() > 0)
    {
        collapseAll = menu.addAction(tr("Collapse everything below"));
        expandAll = menu.addAction(tr("Expand everything below"));
        menu.addSeparator();
    }

    /* Add Universe, on EVERY row, not just empty space -- empty space was
       never a real route to it once every universe is patched to
       something, which is most of the time on a working rig: the tree
       fills its viewport and there is no empty space left to right-click.
       It reads fine anywhere because it does not reference anything
       specific to the row it is on.

       Delete Universe is deliberately NOT offered here, unlike the
       original version of this function -- "Delete Universe 'X'…" showing
       up on an ArtNet row, or any row unrelated to universe X, read as if
       deleting it were somehow an ArtNet action, when it silently deletes
       whichever universe happens to be LAST regardless of what was
       clicked. It already has real, contextual homes: right-click that
       exact universe's own row (KIND_UNIVERSE/UNPATCHED/PENDING all offer
       "Delete universe entirely…" already), or empty space, which still
       offers both Add and Delete together for when there is truly nothing
       else to click. @p delUniv stays NULL from here; kept as a parameter
       so callers do not need their own special case for it. */
    addUniv = menu.addAction(QIcon(":/edit_add.png"), tr("Add Universe"));
}

void ConnectionsTree::rerouteLine(const QString &pluginName, quint32 oldLine,
                                  const QList<quint32> &universes)
{
    if (universes.isEmpty())
        return;

    QLCIOPlugin *p = NULL;
    if (m_doc->ioPluginCache() != NULL)
    {
        foreach (QLCIOPlugin *cand, m_doc->ioPluginCache()->plugins())
            if (cand != NULL && cand->name() == pluginName)
                p = cand;
    }
    if (p == NULL)
        return;

    quint32 newLine = 0;
    if (pickPluginLine(p, true, newLine, int(oldLine)) == false)
        return;
    if (newLine == oldLine)
        return;

    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    const QString oldName = p->lineDescription(oldLine, true);
    const QString newName = p->lineDescription(newLine, true);
    if (QMessageBox::question(this, tr("Reroute interface"),
            tr("Move %1 universe%2 from \"%3\" to \"%4\"?")
                .arg(universes.count())
                .arg(universes.count() == 1 ? QString() : QString("s"))
                .arg(oldName).arg(newName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    if (iomap->patchUndo() != NULL)
        iomap->patchUndo()->capture(universes,
            tr("reroute %1 from \"%2\" to \"%3\"").arg(pluginName).arg(oldName).arg(newName));

    foreach (quint32 uniId, universes)
    {
        Universe *u = iomap->universe(uniId);
        if (u == NULL)
            continue;
        for (int i = 0; i < u->outputPatchesCount(); i++)
        {
            OutputPatch *op = u->outputPatch(i);
            if (op != NULL && op->plugin() != NULL
                    && op->plugin()->name() == pluginName && op->output() == oldLine)
            {
                iomap->setOutputPatch(uniId, pluginName, QString(), newLine, false, i);
                break;
            }
        }
    }

    m_doc->setModified();
    refresh();
}

void ConnectionsTree::addUniverse()
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    // Creating a universe changes the whole list, same as the "new
    // universe" branch of patchUniverseTo() above — a patch-only capture
    // could not undo the creation itself.
    if (iomap->patchUndo() != NULL)
        iomap->patchUndo()->captureUniverses(tr("add a universe"));

    if (iomap->addUniverse() == false)
    {
        if (iomap->patchUndo() != NULL)
            iomap->patchUndo()->clear();
        return;
    }

    iomap->startUniverses();
    m_doc->setModified();
    refresh();
}

void ConnectionsTree::deleteUniverse(quint32 universe)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *uni = iomap ? iomap->universe(universe) : NULL;
    if (uni == NULL)
        return;

    /* Only the LAST universe can go: removeUniverse() refuses anything else
       rather than leave a gap in the numbering, which the whole patch API
       depends on (see InputOutputMap_Test::universeIdAlwaysEqualsItsArrayIndex).
       Say so BEFORE asking, rather than asking a question that cannot be
       honoured -- this used to warn about fixtures losing output, take a Yes,
       spend an undo step, and then do nothing at all, with no feedback. */
    if (universe + 1 != iomap->universesCount())
    {
        QMessageBox::information(this, tr("Delete universe"),
            tr("Only the last universe can be deleted — removing one from the "
               "middle would renumber the others and repoint every patch after "
               "it.\n\nDelete universe %1 first, and work back.")
                .arg(iomap->universesCount()));
        return;
    }

    /* Deleting a universe orphans every fixture patched into it -- they keep
       their universe id and simply lose output -- so this IS undoable, and the
       warning says which part is recoverable. */
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
        /* Guarded above, so reaching here means the rule changed underneath
           us. Do not leave a spent undo step and a modified flag behind for a
           deletion that did not happen. */
        if (iomap->patchUndo() != NULL)
            iomap->patchUndo()->clear();
        QMessageBox::warning(this, tr("Delete universe"),
                             tr("\"%1\" could not be deleted.").arg(uni->name()));
        return;
    }

    m_doc->setModified();
    refresh();
}

void ConnectionsTree::setFeedback(quint32 universe, const QString &pluginName,
                                  quint32 line)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    /* Feedback is an output patch flagged as feedback; an empty plugin name
       clears it. */
    iomap->setOutputPatch(universe, pluginName, QString(), line, true, 0);
    m_doc->setModified();
    refresh();
}

void ConnectionsTree::setProfile(quint32 universe, const QString &profileName)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    if (iomap->setInputProfile(universe, profileName) == false)
        QMessageBox::warning(this, tr("Input profile"),
                             tr("That universe has no input patch to apply a "
                                "profile to."));
    else
        m_doc->setModified();
    refresh();
}

void ConnectionsTree::configurePlugin(const QString &pluginName)
{
    if (m_doc->ioPluginCache() == NULL)
        return;
    foreach (QLCIOPlugin *p, m_doc->ioPluginCache()->plugins())
    {
        if (p != NULL && p->name() == pluginName)
        {
            p->configure();
            refresh();
            return;
        }
    }
}

bool ConnectionsTree::patchTarget(quint32 universe, const QString &pluginName,
                                  quint32 line, QString &address,
                                  quint32 &portAddress) const
{
    return m_patcher->patchTarget(universe, pluginName, line, address, portAddress);
}

/* Nothing to have clicked first, so everything is asked: protocol,
   interface, node and port -- UniversePatcher::patchUniverse(), the same
   flow the Fixture Manager's universe rows use. It rebuilds this view
   through patchChanged(). */
void ConnectionsTree::patchUnpatchedUniverseTo(quint32 universe)
{
    m_patcher->patchUniverse(universe);
}

void ConnectionsTree::patchUniverseToPort(const QString &pluginName, quint32 line,
                                          const QString &deviceAddress,
                                          quint32 portAddress)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    QStringList names;
    QList<quint32> ids;
    foreach (Universe *uni, iomap->universes())
    {
        names << uni->name();
        ids << uni->id();
    }
    const QString newLabel = tr("<new universe>");
    names << newLabel;

    bool ok = false;
    const QString pick = QInputDialog::getItem(
        this, tr("Patch to port"),
        tr("Patch which universe to %1 universe %2?")
            .arg(deviceAddress).arg(portAddressText(portAddress)),
        names, 0, false, &ok);
    if (ok == false || pick.isEmpty())
        return;

    quint32 uniId = InputOutputMap::invalidUniverse();
    if (pick == newLabel)
    {
        /* Creating a universe changes the LIST, so the whole list is held --
           a patch-only capture could not undo the creation itself. */
        if (iomap->patchUndo() != NULL)
            iomap->patchUndo()->captureUniverses(tr("add a universe"));
        if (iomap->addUniverse() == false)
            return;
        QList<Universe *> all = iomap->universes();
        if (all.isEmpty())
            return;
        uniId = all.last()->id();
    }
    else
    {
        const int idx = names.indexOf(pick);
        if (idx < 0 || idx >= ids.count())
            return;
        uniId = ids.at(idx);
    }

    const int index = iomap->outputPatchesCount(uniId);
    if (iomap->setOutputPatch(uniId, pluginName, QString(), line, false, index) == false)
    {
        QMessageBox::warning(this, tr("Patch to port"),
                             tr("Could not patch that universe to %1.").arg(pluginName));
        return;
    }

    /* Patching alone only chooses an interface; without these the data goes
       wherever the plugin defaults to (broadcast) rather than to this node's
       port, and the tree would show it as untargeted. */
    applyTarget(uniId, pluginName, line, deviceAddress, portAddress);
    m_doc->setModified();
    refresh();
}

void ConnectionsTree::setTransmitMode(quint32 universe, const QString &pluginName,
                                      quint32 line, const QString &mode)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *uni = iomap ? iomap->universe(universe) : NULL;
    if (uni == NULL)
        return;

    for (int i = 0; i < uni->outputPatchesCount(); i++)
    {
        OutputPatch *op = uni->outputPatch(i);
        if (op != NULL && op->plugin() != NULL
                && op->plugin()->name() == pluginName && op->output() == line)
        {
            op->setPluginParameter("transmitMode", mode);
            m_doc->setModified();
            break;
        }
    }
    refresh();
}

/** Read a plugin parameter off this universe's patch on this line.
 *
 *  Returns an invalid QVariant when the parameter is not set, which is NOT the
 *  same as zero: absent means the plugin's own default, and a caller that
 *  cannot tell the difference will show a value the rig is not using.
 */
QVariant ConnectionsTree::patchParameter(quint32 universe, const QString &pluginName,
                                         quint32 line, bool output,
                                         const QString &prop) const
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *uni = iomap ? iomap->universe(universe) : NULL;
    if (uni == NULL)
        return QVariant();

    if (output == false)
    {
        InputPatch *ip = uni->inputPatch();
        if (ip == NULL || ip->plugin() == NULL
                || ip->plugin()->name() != pluginName || ip->input() != line)
            return QVariant();
        const QMap<QString, QVariant> params = ip->getPluginParameters();
        return params.contains(prop) ? params.value(prop) : QVariant();
    }

    for (int i = 0; i < uni->outputPatchesCount(); i++)
    {
        OutputPatch *op = uni->outputPatch(i);
        if (op == NULL || op->plugin() == NULL)
            continue;
        if (op->plugin()->name() != pluginName || op->output() != line)
            continue;
        const QMap<QString, QVariant> params = op->getPluginParameters();
        return params.contains(prop) ? params.value(prop) : QVariant();
    }
    return QVariant();
}

/** Set a plugin parameter on this universe's patch on this line. */
void ConnectionsTree::setPatchParameter(quint32 universe, const QString &pluginName,
                                        quint32 line, bool output,
                                        const QString &prop, const QVariant &value)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *uni = iomap ? iomap->universe(universe) : NULL;
    if (uni == NULL)
        return;

    if (iomap->patchUndo() != NULL)
        iomap->patchUndo()->capture(QList<quint32>() << universe,
                                    tr("change %1 on universe %2")
                                        .arg(prop).arg(universe + 1));

    if (output == false)
    {
        InputPatch *ip = uni->inputPatch();
        if (ip != NULL && ip->plugin() != NULL
                && ip->plugin()->name() == pluginName && ip->input() == line)
        {
            ip->setPluginParameter(prop, value);
            m_doc->setModified();
            refresh();
        }
        return;
    }

    for (int i = 0; i < uni->outputPatchesCount(); i++)
    {
        OutputPatch *op = uni->outputPatch(i);
        if (op == NULL || op->plugin() == NULL)
            continue;
        if (op->plugin()->name() != pluginName || op->output() != line)
            continue;
        op->setPluginParameter(prop, value);
        m_doc->setModified();
        refresh();
        return;
    }
}

/** Repoint the feedback patch of a universe.
 *
 *  Feedback stores its destination under the SAME keys as an output patch --
 *  outputIP and outputUni -- because it is an output patch; it just happens to
 *  carry values back to a control surface rather than to a fixture. Which is
 *  why this can reuse applyTarget's sibling logic rather than inventing a
 *  parallel one.
 */
void ConnectionsTree::retargetFeedback(quint32 universe)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *uni = iomap ? iomap->universe(universe) : NULL;
    OutputPatch *fb = uni ? uni->feedbackPatch() : NULL;
    if (fb == NULL)
        return;

    const QMap<QString, QVariant> params = fb->getPluginParameters();

    if (iomap->patchUndo() != NULL)
        iomap->patchUndo()->capture(QList<quint32>() << universe,
                                    tr("retarget feedback on universe %1")
                                        .arg(universe + 1));

    bool ok = false;
    const QString addr = QInputDialog::getText(
        this, tr("Feedback target"),
        tr("Send feedback to which address? Leave empty to broadcast:"),
        QLineEdit::Normal, params.value("outputIP").toString(), &ok).trimmed();
    if (ok == false)
        return;

    if (addr.isEmpty())
    {
        fb->unSetPluginParameter("outputIP");
        fb->unSetPluginParameter("outputUni");
        m_doc->setModified();
        refresh();
        return;
    }

    const quint32 cur = params.value("outputUni", universe).toUInt();
    const QString curTxt = QString("%1:%2:%3").arg((cur >> 8) & 0x7F)
                           .arg((cur >> 4) & 0x0F).arg(cur & 0x0F);
    const QString txt = QInputDialog::getText(
        this, tr("Feedback target"),
        tr("Feedback port address on %1, as net:subnet:universe:").arg(addr),
        QLineEdit::Normal, curTxt, &ok).trimmed();
    if (ok == false)
        return;

    const QStringList parts = txt.split(':');
    bool okNet = false, okSub = false, okUni = false;
    const uint net = parts.value(0).toUInt(&okNet);
    const uint sub = parts.value(1).toUInt(&okSub);
    const uint u = parts.value(2).toUInt(&okUni);
    if (parts.count() != 3 || !okNet || !okSub || !okUni
            || net > 127 || sub > 15 || u > 15)
    {
        QMessageBox::warning(this, tr("Feedback target"),
            tr("\"%1\" is not a port address. Expected net:subnet:universe, "
               "with net 0-127 and subnet and universe 0-15.").arg(txt));
        return;
    }

    fb->setPluginParameter("outputIP", addr);
    fb->setPluginParameter("outputUni", quint32((net << 8) | (sub << 4) | u));
    m_doc->setModified();
    refresh();
}
quint32 ConnectionsTree::universeOnTarget(const QString &pluginName, quint32 line,
                                          const QString &address, quint32 portAddress,
                                          quint32 exceptUniverse) const
{
    return m_patcher->universeOnTarget(pluginName, line, address, portAddress,
                                       exceptUniverse);
}

bool ConnectionsTree::applyTarget(quint32 universe, const QString &pluginName,
                                  quint32 line, const QString &address,
                                  quint32 portAddress)
{
    return m_patcher->applyTarget(universe, pluginName, line, address, portAddress);
}

void ConnectionsTree::retargetPatch(quint32 universe, const QString &pluginName,
                                    quint32 line)
{
    m_patcher->retargetPatch(universe, pluginName, line);
}

/** The universes currently selected, in row order.
 *
 *  Returns them only when they agree on one plugin, and reports each one's
 *  line alongside. A selection spanning two protocols has no single meaning
 *  for "aim these at a node" -- the caller refuses rather than guessing which
 *  half was meant.
 */
QList<quint32> ConnectionsTree::selectedUniverses(QString &pluginName,
                                                  QList<quint32> &lines) const
{
    QList<quint32> unis;
    lines.clear();
    pluginName.clear();

    foreach (QTreeWidgetItem *it, m_tree->selectedItems())
    {
        const QVariant uv = it->data(COL_NAME, ROLE_UNIVERSE);
        if (uv.isValid() == false)
            continue;
        if (it->data(COL_NAME, ROLE_OUTPUT).toBool() == false)
            continue;   // inputs and feedback have no port to number up

        const quint32 u = uv.toUInt();
        if (unis.contains(u))
            continue;   // a universe fanned out appears on several rows

        const QString plug = it->data(COL_NAME, ROLE_PLUGIN).toString();
        if (pluginName.isEmpty())
            pluginName = plug;
        else if (plug != pluginName)
        {
            lines.clear();
            unis.clear();
            pluginName.clear();
            return unis;
        }

        unis << u;
        lines << it->data(COL_NAME, ROLE_LINE).toUInt();
    }
    return unis;
}

/** Aim every selected universe at one node, numbering the ports upward.
 *
 *  This is the whole reason for multi-select. Sixteen universes leaving by one
 *  node on consecutive ports is an ordinary rig and sixteen separate dialogs
 *  is not an ordinary way to describe it. The port address increments per
 *  universe in the order they were selected (see m_selectionOrder).
 *
 *  Undo is captured for the entire set as one step, so a wrong node address
 *  costs one button press rather than sixteen corrections -- which is the
 *  condition on which this feature is worth having at all.
 */
void ConnectionsTree::retargetSelection(const QString &pluginName)
{
    Q_UNUSED(pluginName)

    QString plug;
    QList<quint32> lines;
    QList<quint32> unis = selectedUniverses(plug, lines);
    if (unis.count() < 2 || plug.isEmpty())
        return;

    /* Numbered in the order they were picked, same as "Patch N selected
       universes to…" -- not the order the tree happens to list them in. */
    {
        const QList<quint32> picked = orderedSelectedUniverses();
        QList<quint32> u2, l2;
        foreach (quint32 u, picked)
        {
            const int i = unis.indexOf(u);
            if (i >= 0)
            {
                u2 << u;
                l2 << lines.at(i);
            }
        }
        if (u2.count() == unis.count())
        {
            unis = u2;
            lines = l2;
        }
    }

    bool ok = false;
    /* The devices this interface lists come first, as in every other patch
       question here; the first port to number up from then starts at that
       device's first free one. */
    QString addr;
    QString firstText("0:0:0");
    const QList<TargetChoice> known = visibleTargets(plug, lines.first());
    if (known.isEmpty() == false)
    {
        QStringList labels;
        foreach (const TargetChoice &t, known)
            labels << (t.ports.isEmpty() ? t.label
                       : tr("%1 — %2 port(s)").arg(t.label).arg(t.ports.count()));
        const QString other = tr("Another address…");
        labels << other;
        const QString pick = QInputDialog::getItem(this,
            tr("Retarget %1 universes").arg(unis.count()),
            tr("Send all %1 selected universes to which device?").arg(unis.count()),
            labels, 0, false, &ok);
        if (ok == false)
            return;
        const int idx = labels.indexOf(pick);
        if (idx >= 0 && idx < known.count())
        {
            const TargetChoice &t = known.at(idx);
            addr = t.address;
            foreach (quint32 pa, t.ports)
            {
                const quint32 owner = universeOnTarget(plug, lines.first(), addr, pa,
                                                       InputOutputMap::invalidUniverse());
                if (owner == InputOutputMap::invalidUniverse() || unis.contains(owner))
                {
                    firstText = portAddressText(pa);
                    break;
                }
            }
        }
    }
    if (addr.isEmpty())
    {
        addr = QInputDialog::getText(
            this, tr("Retarget %1 universes").arg(unis.count()),
            tr("Send all %1 selected universes to which address?").arg(unis.count()),
            QLineEdit::Normal, QString(), &ok).trimmed();
        if (ok == false || addr.isEmpty())
            return;
    }

    const QString txt = QInputDialog::getText(
        this, tr("Retarget %1 universes").arg(unis.count()),
        tr("Port address for the FIRST of them, as net:subnet:universe.\n"
           "The rest follow on consecutive ports."),
        QLineEdit::Normal, firstText, &ok).trimmed();
    if (ok == false)
        return;

    const QStringList parts = txt.split(':');
    bool okNet = false, okSub = false, okUni = false;
    const uint net = parts.value(0).toUInt(&okNet);
    const uint sub = parts.value(1).toUInt(&okSub);
    const uint uni = parts.value(2).toUInt(&okUni);
    if (parts.count() != 3 || !okNet || !okSub || !okUni
            || net > 127 || sub > 15 || uni > 15)
    {
        QMessageBox::warning(this, tr("Retarget %1 universes").arg(unis.count()),
            tr("\"%1\" is not a port address. Expected net:subnet:universe, "
               "with net 0-127 and subnet and universe 0-15.").arg(txt));
        return;
    }

    const quint32 first = quint32((net << 8) | (sub << 4) | uni);

    /* Art-Net port addresses are a packed 15-bit field, so incrementing runs
       off the end at 0x7FFF rather than wrapping into a neighbour's net.
       Refuse up front instead of silently aiming the tail of the selection
       somewhere absurd. */
    if (quint32(first + unis.count() - 1) > 0x7FFF)
    {
        QMessageBox::warning(this, tr("Retarget %1 universes").arg(unis.count()),
            tr("Numbering %1 universes up from %2 runs past the last Art-Net "
               "port address. Start lower.").arg(unis.count()).arg(txt));
        return;
    }

    const quint32 last = first + quint32(unis.count() - 1);
    if (QMessageBox::question(this, tr("Retarget %1 universes").arg(unis.count()),
            tr("Aim %1 universes at %2, on ports %3:%4:%5 through %6:%7:%8?")
                .arg(unis.count()).arg(addr)
                .arg((first >> 8) & 0x7F).arg((first >> 4) & 0x0F).arg(first & 0x0F)
                .arg((last >> 8) & 0x7F).arg((last >> 4) & 0x0F).arg(last & 0x0F),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes)
        return;

    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap != NULL && iomap->patchUndo() != NULL)
        iomap->patchUndo()->capture(unis, tr("retarget %1 universes to %2")
                                              .arg(unis.count()).arg(addr));

    int applied = 0, taken = 0;
    for (int i = 0; i < unis.count(); i++)
    {
        const quint32 port = first + quint32(i);
        if (applyTarget(unis.at(i), plug, lines.at(i), addr, port))
            applied++;
        else if (universeOnTarget(plug, lines.at(i), addr, port, unis.at(i))
                 != InputOutputMap::invalidUniverse())
            taken++;
    }

    m_doc->setModified();
    refresh();

    if (applied != unis.count())
    {
        /* A run that collides part-way is the common case -- numbering upward
           from a start that overlaps universes already on the node -- and it is
           worth saying so, because the fix is to start somewhere else rather
           than to go hunting for an unpatched universe. */
        if (taken > 0)
            QMessageBox::warning(this, tr("Retarget %1 universes").arg(unis.count()),
                tr("%1 of %2 were repointed. %3 stopped because another universe "
                   "already sends to that port on %4 -- start the numbering "
                   "somewhere clear of what is already there.")
                    .arg(applied).arg(unis.count()).arg(taken).arg(addr));
        else
            QMessageBox::warning(this, tr("Retarget %1 universes").arg(unis.count()),
                tr("%1 of %2 were repointed; the rest are no longer patched to %3.")
                    .arg(applied).arg(unis.count()).arg(plug));
    }
}

/*********************************************************************
 * Multi-universe patch / unpatch
 *********************************************************************/

QList<ConnectionsTree::TargetChoice>
ConnectionsTree::visibleTargets(const QString &pluginName, quint32 line) const
{
    return m_patcher->visibleTargets(pluginName, line);
}

QList<ConnectionsTree::TargetPortChoice>
ConnectionsTree::targetPortChoices(const QString &pluginName, quint32 line,
                                   quint32 universe) const
{
    return m_patcher->targetPortChoices(pluginName, line, universe);
}

bool ConnectionsTree::askTargetPort(const QString &title, const QString &pluginName,
                                    quint32 line, quint32 universe, bool allowBroadcast,
                                    QString &address, quint32 &port)
{
    return m_patcher->askTargetPort(title, pluginName, line, universe, allowBroadcast,
                                    address, port);
}

void ConnectionsTree::syncSelectionOrder()
{
    /* Everything selected now, in list order. QTreeWidgetItemIterator walks
       in display order, and a fanned-out universe on several rows is still
       one universe. */
    QList<quint32> now;
    for (QTreeWidgetItemIterator it(m_tree, QTreeWidgetItemIterator::Selected); *it; ++it)
    {
        const QVariant uv = (*it)->data(COL_NAME, ROLE_UNIVERSE);
        if (uv.isValid() == false || uv.toUInt() == Universe::invalid())
            continue;
        if (now.contains(uv.toUInt()) == false)
            now << uv.toUInt();
    }

    /* Keep the picking order of what is still selected, then append what is
       new. One ctrl-click adds one; a shift-click range adds several at once,
       which only the list can order. A plain click replaces the selection,
       so the order starts again from it. */
    QList<quint32> order;
    foreach (quint32 u, m_selectionOrder)
        if (now.contains(u))
            order << u;
    foreach (quint32 u, now)
        if (order.contains(u) == false)
            order << u;
    m_selectionOrder = order;
}

QList<quint32> ConnectionsTree::orderedSelectedUniverses()
{
    syncSelectionOrder();
    return m_selectionOrder;
}

ConnectionsTree::OrderedPatch ConnectionsTree::planOrderedPatch(
        const QList<quint32> &universes, const QString &pluginName, quint32 line,
        const QString &address, const QList<quint32> &ports,
        const QStringList &portNames, bool linesAsPorts) const
{
    return m_patcher->planOrderedPatch(universes, pluginName, line, address, ports,
                                       portNames, linesAsPorts);
}

int ConnectionsTree::applyOrderedPatch(const OrderedPatch &plan)
{
    return m_patcher->applyOrderedPatch(plan);
}

void ConnectionsTree::patchSelectionTo(const QList<quint32> &universes)
{
    m_patcher->patchSelectionTo(universes);
}

QList<ConnectionsTree::UnpatchLeg> ConnectionsTree::selectedUnpatchLegs() const
{
    QList<UnpatchLeg> legs;
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return legs;

    for (QTreeWidgetItemIterator it(m_tree, QTreeWidgetItemIterator::Selected); *it; ++it)
    {
        QTreeWidgetItem *item = *it;
        const QVariant uv = item->data(COL_NAME, ROLE_UNIVERSE);
        if (uv.isValid() == false || uv.toUInt() == Universe::invalid())
            continue;
        Universe *u = iomap->universe(uv.toUInt());
        if (u == NULL)
            continue;

        UnpatchLeg leg;
        leg.universe = uv.toUInt();
        const int kind = item->data(COL_NAME, ROLE_KIND).toInt();
        if (kind == KIND_PENDING_UNIVERSE)
        {
            leg.pending = true;
        }
        else if (kind == KIND_UNIVERSE || kind == KIND_PORT)
        {
            /* A feedback row is the universe's route BACK to a controller,
               not its output; "unpatch these universes" is not a request to
               cut that. */
            if (item->text(COL_DETAIL) == tr("feedback"))
                continue;
            leg.pluginName = item->data(COL_NAME, ROLE_PLUGIN).toString();
            leg.line = item->data(COL_NAME, ROLE_LINE).toUInt();
            leg.output = item->data(COL_NAME, ROLE_OUTPUT).toBool();
            bool exists = false;
            if (leg.output)
            {
                for (int i = 0; i < u->outputPatchesCount() && !exists; i++)
                {
                    OutputPatch *op = u->outputPatch(i);
                    exists = (op != NULL && op->plugin() != NULL
                              && op->plugin()->name() == leg.pluginName
                              && op->output() == leg.line);
                }
            }
            else
            {
                exists = (u->inputPatch() != NULL);
            }
            if (exists == false)
                continue;
        }
        else
        {
            continue;       // not patched: nothing to remove
        }

        bool dup = false;
        foreach (const UnpatchLeg &l, legs)
            dup = dup || (l.universe == leg.universe && l.pending == leg.pending
                          && l.pluginName == leg.pluginName && l.line == leg.line
                          && l.output == leg.output);
        if (dup == false)
            legs << leg;
    }
    return legs;
}

void ConnectionsTree::unpatchLegs(const QList<UnpatchLeg> &legs)
{
    m_patcher->unpatchLegs(legs);
}

/** Declare a target discovery cannot find.
 *
 *  The address is the identity -- it is what a patch points at and the only
 *  handle a node that has never answered has -- so it is required and the
 *  name is not. A name typed here is stored as an ordinary target alias, the
 *  same field the inline rename writes, so when the real node does show up it
 *  arrives already labelled instead of reverting to a bare IP.
 */
QString ConnectionsTree::addManualTarget(const QString &pluginName, quint32 line)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return QString();

    bool ok = false;
    const QString addr = QInputDialog::getText(
        this, tr("Add target"),
        tr("Address of the target (an IP, or a hostname this console can "
           "resolve). If the real address isn't known yet, any placeholder "
           "text works too -- retarget the universe once it is:"),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (ok == false)
        return QString();
    if (addr.isEmpty())
    {
        QMessageBox::information(this, tr("Add target"),
            tr("A target needs some address or placeholder text to be known "
               "by -- nothing was declared."));
        return QString();
    }

    if (iomap->isManualTarget(addr))
    {
        QMessageBox::information(this, tr("Add target"),
            tr("%1 is already declared.").arg(addr));
        return QString();
    }

    const QString name = QInputDialog::getText(
        this, tr("Add target"),
        tr("A name for %1 — optional:").arg(addr),
        QLineEdit::Normal, QString(), &ok).trimmed();

    iomap->addManualTarget(pluginName, line, addr);
    if (ok && name.isEmpty() == false)
        iomap->setTargetAlias(addr, name);

    /* Declaring a target is exactly the moment someone wants to know whether
       it is actually there. Leaving that to the next periodic rescan (up to
       five seconds, or however long until "Rescan" is clicked by hand) made
       "add a target by IP" look like it had silently done nothing. */
    if (m_doc->ioPluginCache() != NULL)
    {
        foreach (QLCIOPlugin *p, m_doc->ioPluginCache()->plugins())
            if (p != NULL && p->name() == pluginName && p->supportsOutputTargets())
                p->probeTarget(addr);
    }

    m_doc->setModified();
    refresh();
    return addr;
}

/** Same target-declaration flow as addManualTarget(), except the ports come
 *  from a device profile someone picked instead of "Add port..." typed by
 *  hand once per port -- the two calls a profile saves are exactly the ones
 *  a manual patch-ahead already needed, just batched. */
void ConnectionsTree::addTargetFromLibrary(const QString &pluginName, quint32 line)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    QList<DeviceProfile> matching;
    foreach (const DeviceProfile &dp, loadDeviceProfiles())
        if (dp.protocol == pluginName)
            matching << dp;

    if (matching.isEmpty())
    {
        QMessageBox::information(this, tr("Add target from library"),
            tr("No device profiles are available for %1.").arg(pluginName));
        return;
    }

    QStringList labels;
    foreach (const DeviceProfile &dp, matching)
        labels << tr("%1 %2 (%3 port%4)").arg(dp.manufacturer, dp.model)
                      .arg(dp.ports.count()).arg(dp.ports.count() == 1 ? QString() : QString("s"));

    bool ok = false;
    const QString picked = QInputDialog::getItem(this, tr("Add target from library"),
        tr("Device:"), labels, 0, false, &ok);
    if (ok == false)
        return;

    const int idx = labels.indexOf(picked);
    if (idx < 0)
        return;
    const DeviceProfile &dp = matching.at(idx);

    const QString addr = addManualTarget(pluginName, line);
    if (addr.isEmpty())
        return;

    foreach (quint32 portAddr, dp.ports)
        iomap->addManualPort(addr, portAddr);

    m_doc->setModified();
    refresh();
}

/** Add a port to a hand-declared target.
 *
 *  Asked for in Art-Net's own net:sub:universe terms rather than as a raw
 *  number, because that is what is printed on the node's display and typed
 *  into its front panel; converting in your head from the packed 15-bit form
 *  is a step that exists only because of how the protocol stores it.
 */
void ConnectionsTree::addManualPort(const QString &address)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    bool ok = false;
    const QString txt = QInputDialog::getText(
        this, tr("Add port"),
        tr("Port address on %1, as net:subnet:universe (for example 0:0:1):")
            .arg(address), QLineEdit::Normal, QString("0:0:0"), &ok).trimmed();
    if (ok == false || txt.isEmpty())
        return;

    const QStringList parts = txt.split(':');
    bool okNet = false, okSub = false, okUni = false;
    const uint net = parts.value(0).toUInt(&okNet);
    const uint sub = parts.value(1).toUInt(&okSub);
    const uint uni = parts.value(2).toUInt(&okUni);
    if (parts.count() != 3 || !okNet || !okSub || !okUni
            || net > 127 || sub > 15 || uni > 15)
    {
        QMessageBox::warning(this, tr("Add port"),
            tr("\"%1\" is not a port address. Expected net:subnet:universe, "
               "with net 0-127 and subnet and universe 0-15.").arg(txt));
        return;
    }

    iomap->addManualPort(address, quint32((net << 8) | (sub << 4) | uni));
    m_doc->setModified();
    refresh();
}

/** Drop a hand-declared target.
 *
 *  Patches aimed at this address are left in place on purpose. Removing a
 *  description of where something lives must not silently unpatch a universe
 *  -- that is a different, much larger action, and it has its own menu entry
 *  on the universe itself.
 */
void ConnectionsTree::forgetTarget(const QString &address)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    if (QMessageBox::question(this, tr("Forget target"),
            tr("Forget %1 and its ports?\n\nUniverses patched to it stay "
               "patched; only the description of the target goes away.")
                .arg(address),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes)
        return;

    iomap->removeManualTarget(address);
    m_doc->setModified();
    refresh();
}

/** Render ordered name/value pairs as a tooltip.
 *
 *  A table rather than lines of "key: value" because the values are the part
 *  being scanned -- an operator is looking for the firmware, or for the port
 *  that says SHORT DETECTED -- and ragged left edges make that a search
 *  instead of a glance.
 */
QString ConnectionsTree::propertyTooltip(const QString &title,
                                         const QList<QPair<QString, QString> > &props)
{
    if (props.isEmpty())
        return QString();

    QString html = QString("<b>%1</b><table cellspacing='0' cellpadding='2'>")
                   .arg(title.toHtmlEscaped());
    for (int i = 0; i < props.count(); i++)
    {
        html += QString("<tr><td align='right'><i>%1</i></td><td>%2</td></tr>")
                .arg(props.at(i).first.toHtmlEscaped())
                .arg(props.at(i).second.toHtmlEscaped());
    }
    html += "</table>";
    return html;
}

void ConnectionsTree::setRowTooltip(QTreeWidgetItem *item, const QString &html)
{
    if (item == NULL || html.isEmpty())
        return;
    /* On every column: the pointer lands wherever the row is widest, and a
       tooltip that only exists over one cell reads as no tooltip at all. */
    for (int c = 0; c < 3; c++)
        item->setToolTip(c, html);
}

void ConnectionsTree::setCarriesFixtures(QTreeWidgetItem *item, quint32 universeId) const
{
    if (item == NULL || m_doc == NULL)
        return;

    /* qMax(1, ...) matches how the rest of the app counts heads (see e.g.
       createfixturegroup.cpp) -- a head-less/dimmer fixture still counts
       as one real thing patched in. */
    int nfx = 0, nheads = 0, lastCh = 0;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL || !fx->isPatched() || fx->universe() != universeId)
            continue;
        nfx++;
        nheads += qMax(1, fx->heads());
        lastCh = qMax(lastCh, int(fx->address() + fx->channels()));
    }
    if (nfx == 0)
        return;

    item->setText(COL_CARRIES, nheads != nfx
        ? tr("%1 fx · %2 heads · %3/512").arg(nfx).arg(nheads).arg(lastCh)
        : tr("%1 fx · %2/512").arg(nfx).arg(lastCh));
    if (lastCh > 512)
        item->setForeground(COL_CARRIES, QBrush(QColor(220, 90, 90)));
}

/** Give an interface an operator-facing name. */
void ConnectionsTree::renameLine(const QString &pluginName, const QString &lineName)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL || lineName.isEmpty())
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Name interface"),
        tr("A name for %1 — leave empty to clear:").arg(lineName),
        QLineEdit::Normal, iomap->lineAlias(pluginName, lineName), &ok);
    if (ok == false)
        return;

    iomap->setLineAlias(pluginName, lineName, name.trimmed());
    m_doc->setModified();
    refresh();
}

void ConnectionsTree::renameTarget(const QString &address)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Name target"),
        tr("A name for %1 — leave empty to clear:").arg(address),
        QLineEdit::Normal, iomap->targetAlias(address), &ok);
    if (ok == false)
        return;

    iomap->setTargetAlias(address, name.trimmed());
    m_doc->setModified();
    refresh();
}

void ConnectionsTree::patchToNewTarget(const QString &pluginName, quint32 line)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    bool ok = false;
    const QString addr = QInputDialog::getText(
        this, tr("Patch to a new target"),
        tr("Target address (a node IP, or a broadcast address):"),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (ok == false || addr.isEmpty())
        return;

    const int portAddr = QInputDialog::getInt(
        this, tr("Patch to a new target"),
        tr("Port address on that target:"), 0, 0, 32767, 1, &ok);
    if (ok == false)
        return;

    /* Deliberately no reachability check. A target that is absent on the bench
       is routinely present on the rig, and refusing to configure it here would
       make the console useless for prep. It simply shows as
       "configured - not heard from" until something answers. */
    patchUniverseToPort(pluginName, line, addr, quint32(portAddr));
}

void ConnectionsTree::slotItemChanged(QTreeWidgetItem *item, int column)
{
    /* refresh() rewrites every row, which fires this for each one; without the
       guard a rebuild would write each row's text back as an alias. */
    if (m_rebuilding || item == NULL || column != COL_NAME)
        return;
    const int ekind = item->data(COL_NAME, ROLE_KIND).toInt();
    if (ekind != KIND_DEVICE && ekind != KIND_LINE && ekind != KIND_UNIVERSE
            && ekind != KIND_PENDING_UNIVERSE && ekind != KIND_UNPATCHED_UNIVERSE)
        return;

    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    /* Universe rows (patched, pending, or unpatched -- same "Rename
       universe..." offered on all three from the context menu) show as
       "<id+1>: <name>", the one authoritative-number-plus-free-form-name
       shape used everywhere in the tree. Strip the number back off rather
       than storing it into the name, which would otherwise bake a stale id
       into the name the moment universes are reordered. */
    if (ekind == KIND_UNIVERSE || ekind == KIND_PENDING_UNIVERSE || ekind == KIND_UNPATCHED_UNIVERSE)
    {
        const quint32 uniId = item->data(COL_NAME, ROLE_UNIVERSE).toUInt();
        Universe *uni = iomap->universe(uniId);
        if (uni == NULL)
            return;

        QString name = item->text(COL_NAME).trimmed();
        const QString prefix = QString("%1: ").arg(uni->id() + 1);
        if (name.startsWith(prefix))
            name.remove(0, prefix.length());
        name = name.trimmed();
        if (name.isEmpty() || name == uni->name())
            return;

        iomap->setUniverseName(int(uniId), name);
        m_doc->setModified();
        refresh();
        return;
    }

    /* Interfaces are named the same way targets are -- same decorated
       "Name (identity)" display, same need to strip the identity back off
       before storing -- but keyed by plugin and line string rather than by
       address. */
    if (ekind == KIND_LINE)
    {
        const QString lineName = item->data(COL_NAME, ROLE_LINENAME).toString();
        const QString plug = item->data(COL_NAME, ROLE_PLUGIN).toString();
        if (lineName.isEmpty() || plug.isEmpty())
            return;

        QString lname = item->text(COL_NAME).trimmed();
        const QString lsuffix = QString(" (%1)").arg(lineName);
        if (lname.endsWith(lsuffix))
            lname.chop(lsuffix.length());
        if (lname == lineName)
            lname.clear();
        lname = lname.trimmed();
        if (lname == iomap->lineAlias(plug, lineName))
            return;

        iomap->setLineAlias(plug, lineName, lname);
        m_doc->setModified();
        refresh();
        return;
    }

    const QString addr = item->data(COL_NAME, ROLE_ADDRESS).toString();
    if (addr.isEmpty())
        return;

    /* QTreeWidgetItem stores DisplayRole and EditRole in the same slot, so the
       editor necessarily shows the decorated "Name (1.2.3.4)" text. Strip the
       address back off rather than saving it into the alias, which would
       otherwise accumulate a new set of parentheses on every edit. */
    QString name = item->text(COL_NAME).trimmed();
    const QString suffix = QString(" (%1)").arg(addr);
    if (name.endsWith(suffix))
        name.chop(suffix.length());
    if (name == addr)
        name.clear();
    name = name.trimmed();
    if (name == iomap->targetAlias(addr))
        return;

    iomap->setTargetAlias(addr, name);
    m_doc->setModified();
    refresh();
}

void ConnectionsTree::buildUniverseList()
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    // Kept across the rebuild every few seconds: where you were, what you had.
    const QPoint scroll(m_tree->horizontalScrollBar()->value(),
                        m_tree->verticalScrollBar()->value());
    QSet<quint32> selected;
    foreach (QTreeWidgetItem *it, m_tree->selectedItems())
        selected.insert(it->data(COL_NAME, ROLE_UNIVERSE).toUInt());
    const quint32 current = m_tree->currentItem() != NULL
        ? m_tree->currentItem()->data(COL_NAME, ROLE_UNIVERSE).toUInt() : Universe::invalid();
    QString currentShowControl;
    if (m_tree->currentItem() != NULL && m_tree->currentItem()->isSelected())
    {
        const int ck = m_tree->currentItem()->data(COL_NAME, ROLE_KIND).toInt();
        if (ck == KIND_SHOWCONTROL)
            currentShowControl = m_tree->currentItem()->data(COL_NAME, ROLE_PLUGIN).toString();
        else if (ck == KIND_SHOWCONTROL_BRANCH)
            currentShowControl = QStringLiteral("*");
    }

    m_rebuilding = true;
    m_tree->clear();
    m_tree->setRootIsDecorated(false);

    /* Where an output leg goes, in words (UniversePatcher::outputLegText(),
       which the Fixture Manager's universe menu uses too): "Art-Net → CS1
       (172.18.2.211) · port 1 · universe 0:0:0". */
    QHash<QString, QList<TargetChoice> > targetsByLine;   // asked once per rebuild
    auto legText = [this, &targetsByLine](OutputPatch *op) {
        return m_patcher->outputLegText(op, &targetsByLine);
    };

    /* "an 'active' only view": a universe nothing is patched into and that
       goes nowhere is hidden unless "Show unused universes" is ticked. Only
       hidden -- universes are numbered by position and every patch refers to
       them, so one from the middle can't be removed without renumbering the
       rest. */
    const bool showUnused = m_showUnused != NULL && m_showUnused->isChecked();
    QSet<quint32> carrying;
    foreach (Fixture *fx, m_doc->fixtures())
        if (fx != NULL && fx->isPatched())
            carrying.insert(fx->universe());
    int hidden = 0;
    foreach (Universe *u, iomap->universes())
    {
        if (u == NULL)
            continue;
        const bool inUse = carrying.contains(u->id()) || u->outputPatchesCount() > 0
                           || u->inputPatch() != NULL || u->offlinePatches().isEmpty() == false;
        if (!inUse && !showUnused)
        {
            ++hidden;
            continue;
        }
        QTreeWidgetItem *item = new QTreeWidgetItem(m_tree);
        item->setFlags(item->flags() | Qt::ItemIsEditable);       // rename inline
        item->setText(COL_NAME, tr("%1: %2").arg(u->id() + 1).arg(u->name()));
        item->setData(COL_NAME, ROLE_UNIVERSE, u->id());

        QStringList legs;
        OutputPatch *first = NULL;
        for (int i = 0; i < u->outputPatchesCount(); ++i)
        {
            OutputPatch *op = u->outputPatch(i);
            if (op == NULL)
                continue;
            if (first == NULL)
                first = op;
            legs << legText(op);
        }
        QStringList detail = legs;
        if (InputPatch *ip = u->inputPatch())
            detail << tr("in: %1").arg(ip->pluginName());
        foreach (const Universe::OfflinePatch &p, u->offlinePatches())
            detail << tr("%1 %2 \"%3\" not present — kept, will reconnect")
                          .arg(p.tag == Universe::InputPatchTag ? tr("in:")
                               : p.tag == Universe::FeedbackPatchTag ? tr("feedback:")
                               : tr("out:"))
                          .arg(p.plugin, p.uid.isEmpty() ? p.plugin : p.uid);
        item->setText(COL_DETAIL, detail.isEmpty() ? tr("not patched") : detail.join(QStringLiteral(" · ")));
        if (legs.size() > 1)
            setRowTooltip(item, tr("<b>%1</b> goes out %2 ways:<br>%3")
                                    .arg(u->name().toHtmlEscaped()).arg(legs.size())
                                    .arg(legs.join(QStringLiteral("<br>")).toHtmlEscaped()));

        /* The same actions as in the wiring view: a patched one is its (first)
           output leg's universe row, an unpatched one the Unpatched folder's. */
        if (first != NULL && first->plugin() != NULL)
        {
            item->setData(COL_NAME, ROLE_KIND, KIND_UNIVERSE);
            item->setData(COL_NAME, ROLE_PLUGIN, first->plugin()->name());
            item->setData(COL_NAME, ROLE_LINE, first->output());
            item->setData(COL_NAME, ROLE_OUTPUT, true);
        }
        else
        {
            item->setData(COL_NAME, ROLE_KIND, KIND_UNPATCHED_UNIVERSE);
            item->setForeground(COL_DETAIL, QBrush(QColor(138, 142, 153)));
        }
        setCarriesFixtures(item, u->id());
        if (selected.contains(u->id()))
            item->setSelected(true);
    }
    if (hidden > 0)
    {
        QTreeWidgetItem *note = new QTreeWidgetItem(m_tree);
        note->setText(COL_NAME, tr("%n unused universe(s) hidden", "", hidden));
        note->setText(COL_DETAIL, tr("nothing patched into them, no output -- tick "
                                     "\"Show unused universes\" to see them"));
        note->setFlags(Qt::ItemIsEnabled);
        note->setForeground(COL_NAME, QBrush(QColor(138, 142, 153)));
        note->setForeground(COL_DETAIL, QBrush(QColor(138, 142, 153)));
        note->setData(COL_NAME, ROLE_UNIVERSE, Universe::invalid());
    }

    addShowControlRows();
    if (QTreeWidgetItem *branch = showControlBranch())
    {
        branch->setExpanded(true);          // no expander in this view
        if (currentShowControl == QLatin1String("*"))
        {
            branch->setSelected(true);
            m_tree->setCurrentItem(branch, 0, QItemSelectionModel::NoUpdate);
        }
        for (int c = 0; c < branch->childCount(); c++)
        {
            QTreeWidgetItem *row = branch->child(c);
            if (row->data(COL_NAME, ROLE_PLUGIN).toString() == currentShowControl)
            {
                row->setSelected(true);
                m_tree->setCurrentItem(row, 0, QItemSelectionModel::NoUpdate);
            }
        }
    }

    if (current != Universe::invalid())
    {
        foreach (QTreeWidgetItem *it, m_tree->selectedItems())
        {
            if (it->data(COL_NAME, ROLE_UNIVERSE).toUInt() == current)
                m_tree->setCurrentItem(it, 0, QItemSelectionModel::NoUpdate);
        }
    }
    markOutputProblems();
    m_rebuilding = false;
    m_populatedOnce = true;
    restoreScroll(scroll);
    if (m_status != NULL)
        m_status->hide();
}

/*****************************************************************************
 * Show control & sync
 *****************************************************************************/

void ConnectionsTree::setShowControlPanel(QWidget *panel)
{
    if (m_showControlPanel != nullptr || panel == nullptr)
        return;
    m_showControlPanel = panel;
    m_side->addWidget(panel);
    refresh();
}

void ConnectionsTree::setShowControlRows(const QList<QStringList> &rows)
{
    m_showControlRows = rows;
    QTreeWidgetItem *branch = showControlBranch();
    if (branch == nullptr || branch->childCount() != rows.count())
    {
        refresh();
        return;
    }
    const bool was = m_rebuilding;
    m_rebuilding = true;                // not an alias edit
    for (int c = 0; c < branch->childCount(); c++)
        if (rows.at(c).count() >= 3)
            branch->child(c)->setText(COL_DETAIL, rows.at(c).at(2));
    m_rebuilding = was;
}

void ConnectionsTree::selectShowControlRow(const QString &key)
{
    if (showControlBranch() == nullptr)
        refresh();
    QTreeWidgetItem *branch = showControlBranch();
    if (branch == nullptr)
        return;
    QTreeWidgetItem *pick = branch;
    for (int c = 0; c < branch->childCount(); c++)
        if (branch->child(c)->data(COL_NAME, ROLE_PLUGIN).toString() == key)
            pick = branch->child(c);
    branch->setExpanded(true);
    m_tree->clearSelection();
    m_tree->setCurrentItem(pick);
    pick->setSelected(true);
    m_tree->scrollToItem(pick);
}

void ConnectionsTree::clearShowControlSelection()
{
    m_tree->clearSelection();
    m_side->setCurrentWidget(m_outputPage);
    m_tree->setFocus();
}

QTreeWidgetItem *ConnectionsTree::showControlBranch() const
{
    for (int i = m_tree->topLevelItemCount() - 1; i >= 0; i--)
        if (m_tree->topLevelItem(i)->data(COL_NAME, ROLE_KIND).toInt() == KIND_SHOWCONTROL_BRANCH)
            return m_tree->topLevelItem(i);
    return nullptr;
}

void ConnectionsTree::addShowControlRows()
{
    if (m_showControlPanel == nullptr || m_showControlRows.isEmpty())
        return;

    /* Its own branch after the wiring: these links arrive over the inputs
       above (OSC over the network, MSC and timecode on a patched MIDI
       input), but they are settings of this show, not a plugin's line. */
    QTreeWidgetItem *branch = new QTreeWidgetItem(m_tree);
    branch->setText(COL_NAME, tr("Show control & sync"));
    branch->setText(COL_DETAIL, tr("cue commands and timecode from outside (QLab, a sound desk)"));
    branch->setData(COL_NAME, ROLE_KIND, KIND_SHOWCONTROL_BRANCH);
    branch->setData(COL_NAME, ROLE_UNIVERSE, Universe::invalid());
    branch->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    branch->setIcon(COL_NAME, QIcon(":/input.png"));
    for (const QStringList &r : m_showControlRows)
    {
        if (r.count() < 3)
            continue;
        QTreeWidgetItem *row = new QTreeWidgetItem(branch);
        row->setText(COL_NAME, r.at(1));
        row->setText(COL_DETAIL, r.at(2));
        row->setData(COL_NAME, ROLE_KIND, KIND_SHOWCONTROL);
        row->setData(COL_NAME, ROLE_PLUGIN, r.at(0));
        row->setData(COL_NAME, ROLE_UNIVERSE, Universe::invalid());
        row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
}

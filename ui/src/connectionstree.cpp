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

#include "connectionstree.h"
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

#define COL_NAME    0
#define COL_DETAIL  1
#define COL_CARRIES 2

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

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(6, 6, 6, 6);

    QLabel *hint = new QLabel(
        tr("How this console is wired, following the signal outward: protocol → "
           "interface → devices heard on it → the universes they carry. "
           "Right-click an interface to patch a universe to it or configure "
           "the protocol; right-click a universe to rename it, set its "
           "feedback line or input profile, unpatch it, or delete it."), this);
    hint->setWordWrap(true);
    lay->addWidget(hint);

    /* Default to only what is live: configured or discovered. A stock build
       exposes 14 plugins and dozens of lines, nearly all of them irrelevant --
       loopback addresses, protocols with no hardware, interfaces carrying
       nothing. Listing them buries the two lines that actually matter. The
       toggle exists because a fresh workspace has nothing patched yet, and an
       empty tree would be a dead end when you are trying to patch for the
       first time. */
    m_showUnused = new QCheckBox(tr("Show unused protocols and interfaces"), this);
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

    QHBoxLayout *bar = new QHBoxLayout;
    bar->addWidget(m_showUnused);
    bar->addStretch(1);
    bar->addWidget(m_undo);
    bar->addWidget(m_rescan);
    lay->addLayout(bar);

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
    lay->addWidget(m_status);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels(QStringList()
                            << tr("Device") << tr("Detail") << tr("Carries"));
    /* Extended selection so several universes can be retargeted in one go --
       the ArtNet block-patch case, where sixteen universes leave by one node
       on consecutive ports. Every single-row action keeps working on the row
       under the pointer, NOT on the selection: right-clicking one row while
       twelve are selected must not quietly act on twelve. Only the explicitly
       plural action reads the selection, and it says the count in its own
       name before it does anything. */
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(COL_NAME, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(COL_DETAIL, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(COL_CARRIES, QHeaderView::Stretch);
    connect(m_tree, SIGNAL(itemChanged(QTreeWidgetItem *, int)),
            this, SLOT(slotItemChanged(QTreeWidgetItem *, int)));
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotContextMenu(QPoint)));
    lay->addWidget(m_tree);

    /* Art-Net nodes announce themselves roughly once a second, so a node that
       is powered on (or off) mid-session should appear (or stop being current)
       without the operator hunting for a rescan button. Five seconds is slow
       enough to be invisible and fast enough to feel live. */
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(5000);
    connect(m_refreshTimer, SIGNAL(timeout()), this, SLOT(refresh()));
    /* Deliberately NOT started here -- showEvent() owns the timer, so a
       hidden tab costs nothing. */

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

    quint32 id = 0;
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
        id++;
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
       parented into the view. */
    QWidget *focus = QApplication::focusWidget();
    if (focus != NULL && m_tree->isAncestorOf(focus))
        return;

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

    m_rebuilding = true;
    m_tree->clear();

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

    foreach (QLCIOPlugin *plugin, cache->plugins())
    {
        if (plugin == NULL)
            continue;

        QTreeWidgetItem *pitem = new QTreeWidgetItem(m_tree);
        pitem->setText(COL_NAME, plugin->name());
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
            litem->setText(COL_DETAIL, roles.join(", "));

            /* Plugins already write a description of a line for their own
               config dialogs -- widget type and serial for DMX USB, the port
               and packet counters for Art-Net. It is the best answer available
               to "what IS this thing", and it was reachable only by opening a
               different dialog. */
            QString lineInfo;
            if (line < outLines.count())
                lineInfo = plugin->outputInfo(quint32(line));
            if (lineInfo.isEmpty() && line < inLines.count())
                lineInfo = plugin->inputInfo(quint32(line));
            if (lineInfo.isEmpty() == false)
                setRowTooltip(litem, lineInfo);

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
                if (dev.address.isEmpty() == false) detail << dev.address;
                if (dev.hardwareId.isEmpty() == false) detail << dev.hardwareId;
                if (dev.detail.isEmpty() == false) detail << dev.detail;
                detail << (dev.rdmCapable ? tr("RDM") : tr("no RDM"));
                ditem->setText(COL_DETAIL, detail.join(" \u00b7 "));
                /* Same tint vocabulary as the universe rows: green means the
                   path is real, amber means it is only configured. Applied to
                   the node as well as its universes so availability reads at
                   the level someone actually asks about it -- "is that node
                   there", not "is that universe reachable". */
                ditem->setBackground(COL_NAME, QBrush(QColor(46, 125, 50, 40)));
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
                    pt->setText(COL_NAME, tr("port %1").arg(p + 1));
                    pt->setText(COL_DETAIL, shared
                        ? tr("%1  (same address as an earlier port)")
                              .arg(dev.portLabels.at(p))
                        : dev.portLabels.at(p));
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
                        pt->setText(COL_DETAIL, QString("%1:%2:%3")
                                    .arg((pa >> 8) & 0x7F).arg((pa >> 4) & 0x0F)
                                    .arg(pa & 0x0F));
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
                                    gport->setText(COL_NAME,
                                        tr("port %1").arg((portAddr & 0x0F) + 1));
                                    gport->setText(COL_DETAIL,
                                        QString("%1:%2:%3")
                                            .arg((portAddr >> 8) & 0x7F)
                                            .arg((portAddr >> 4) & 0x0F)
                                            .arg(portAddr & 0x0F));
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
                    int nfx = 0, lastCh = 0;
                    foreach (Fixture *fx, m_doc->fixtures())
                    {
                        if (fx == NULL || fx->universe() != uniId)
                            continue;
                        nfx++;
                        lastCh = qMax(lastCh, int(fx->address() + fx->channels()));
                    }
                    if (nfx > 0)
                    {
                        uitem->setText(COL_CARRIES,
                                       tr("%1 fx · %2/512").arg(nfx).arg(lastCh));
                        if (lastCh > 512)
                            uitem->setForeground(COL_CARRIES, QBrush(QColor(220, 90, 90)));
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
                            where += tr(" port %1").arg(pp.value("outputUni").toInt() & 0x0F);
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
                    uprops << Prop(tr("Channels used"),
                                   tr("%1 of 512").arg(lastCh));
                    setRowTooltip(uitem, propertyTooltip(
                        tr("%1: %2").arg(uniId + 1).arg(uni->name()), uprops));
                }
            }
        }
        if (pitem->childCount() == 0)
        {
            if (showAll == false)
                delete pitem;       // nothing live under this protocol
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
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *top = m_tree->topLevelItem(i);
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
                unis++;
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

    /* An empty tree is ambiguous -- "nothing is connected" and "the view is
       broken" look identical. Say which. */
    if (m_tree->topLevelItemCount() == 0)
    {
        QTreeWidgetItem *none = new QTreeWidgetItem(m_tree);
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
                            : depth <= 1);
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
    m_rebuilding = false;

    /* The banner answers exactly one question -- "is an empty-looking tree
       still filling in?" -- so it belongs on screen only while that question
       is open. Keeping it up for a fixed six seconds after devices are
       already listed made a finished tree look unfinished, which is the same
       ambiguity it exists to remove, pointing the other way. */
    if (m_status != NULL)
        m_status->setVisible(heardDevices == 0 && m_since->elapsed() < 6000);
}

/** Stable identity for a row: its name plus every ancestor's, so two rows that
 *  share a display name under different parents are not confused. */
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
        return;

    const int kind = item->data(COL_NAME, ROLE_KIND).toInt();
    const QString plugin = item->data(COL_NAME, ROLE_PLUGIN).toString();
    const quint32 line = item->data(COL_NAME, ROLE_LINE).toUInt();

    QMenu menu(this);

    /* The one plural action, offered above everything else and named with its
       own count so it can never be mistaken for the single-row entries below
       it. Everything else in this menu acts on the row under the pointer,
       whatever else happens to be selected. */
    QAction *bulk = NULL;
    {
        QString selPlug;
        QList<quint32> selLines;
        const QList<quint32> selUnis = selectedUniverses(selPlug, selLines);
        if (selUnis.count() > 1 && pluginSupportsTargetsIn(m_doc, selPlug))
        {
            bulk = menu.addAction(tr("Retarget %1 selected universes…")
                                      .arg(selUnis.count()));
            menu.addSeparator();
        }
    }

    /* Offered on anything with children, before the row-specific actions.
       Deep branches are quick to open and tedious to close one triangle at a
       time, and the plugin and device rows had no menu at all -- so the rows
       most worth collapsing were the ones you could not act on. */
    QAction *collapseAll = NULL;
    QAction *expandAll = NULL;
    if (item->childCount() > 0)
    {
        collapseAll = menu.addAction(tr("Collapse everything below"));
        expandAll = menu.addAction(tr("Expand everything below"));
        menu.addSeparator();
    }

    /* Resolved before the per-kind menus below, each of which exec()s its own
       menu and returns. */
    if (bulk != NULL && kind != KIND_UNIVERSE && kind != KIND_PORT)
    {
        QAction *pick = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (pick != NULL && (pick == collapseAll || pick == expandAll))
        { setExpandedDeep(item, pick == expandAll); return; }
        if (pick == bulk)
            retargetSelection(QString());
        return;
    }

    if (kind == KIND_PLUGIN)
    {
        /* Protocol-level settings (ArtNet/E1.31 options and so on) live in the
           plugin's own dialog; there is no generic model for them. */
        QLCIOPlugin *p = NULL;
        if (m_doc->ioPluginCache() != NULL)
        {
            foreach (QLCIOPlugin *cand, m_doc->ioPluginCache()->plugins())
                if (cand != NULL && cand->name() == plugin)
                    p = cand;
        }
        QAction *cfg = (p != NULL && p->canConfigure())
            ? menu.addAction(tr("Configure %1…").arg(plugin)) : NULL;
        if (menu.isEmpty())
            return;
        QAction *pick0 = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (pick0 != NULL && (pick0 == collapseAll || pick0 == expandAll))
        { setExpandedDeep(item, pick0 == expandAll); return; }
        if (pick0 == cfg)
            configurePlugin(plugin);
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

        QAction *pick1 = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (pick1 != NULL && (pick1 == collapseAll || pick1 == expandAll))
        { setExpandedDeep(item, pick1 == expandAll); return; }
        if (pick1 == ren)
            renameTarget(addr);
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
                ? menu.addAction(tr("Change target address / port…")) : NULL;
            QAction *unp = menu.addAction(tr("Unpatch from this interface"));
            menu.addSeparator();
            QAction *del = menu.addAction(tr("Delete universe entirely…"));
            QAction *c = menu.exec(m_tree->viewport()->mapToGlobal(pos));
            if (c != NULL && (c == collapseAll || c == expandAll))
            { setExpandedDeep(item, c == expandAll); return; }
            if (bulk != NULL && c == bulk)
            { retargetSelection(QString()); return; }
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
        QAction *pick2 = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (pick2 != NULL && (pick2 == collapseAll || pick2 == expandAll))
        { setExpandedDeep(item, pick2 == expandAll); return; }
        if (bulk != NULL && pick2 == bulk)
        { retargetSelection(QString()); return; }
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

        QAction *addTgt = targets
            ? menu.addAction(tr("Add a target on this interface…")) : NULL;

        QAction *pNew = NULL;
        if (targets)
        {
            pNew = menu.addAction(tr("Patch a universe to a new target…"));
            menu.addSeparator();
        }
        QAction *pOut = canOut
            ? menu.addAction(tr("Patch a universe here (output)…")) : NULL;
        QAction *pIn = canIn
            ? menu.addAction(tr("Patch a universe here (input)…")) : NULL;
        if (menu.isEmpty())
            return;
        QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (chosen != NULL && (chosen == collapseAll || chosen == expandAll))
        { setExpandedDeep(item, chosen == expandAll); return; }
        if (chosen == nameLine)
            renameLine(plugin, item->data(COL_NAME, ROLE_LINENAME).toString());
        else if (addTgt != NULL && chosen == addTgt)
            addManualTarget(plugin, line);
        else if (pNew != NULL && chosen == pNew)
            patchToNewTarget(plugin, line);
        else if (pOut != NULL && chosen == pOut)
            patchUniverseTo(plugin, line, true);
        else if (pIn != NULL && chosen == pIn)
            patchUniverseTo(plugin, line, false);
        return;
    }

    if (kind == KIND_UNIVERSE)
    {
        const quint32 uni = item->data(COL_NAME, ROLE_UNIVERSE).toUInt();
        const bool output = item->data(COL_NAME, ROLE_OUTPUT).toBool();

        QAction *ren = menu.addAction(tr("Rename universe…"));
        /* Repointing an existing patch, rather than unpatching and starting
           over -- which is what a node swapped for a spare, or renumbered on
           its front panel, used to cost. */
        QAction *retgt = (output && pluginSupportsTargetsIn(m_doc, plugin))
            ? menu.addAction(tr("Change target address / port…")) : NULL;

        const bool isMidi = plugin.contains("MIDI", Qt::CaseInsensitive);
        const bool isArtNet = plugin.contains("ArtNet", Qt::CaseInsensitive);

        /* Plugin parameters that were previously reachable only from the
           Overview grid. They are properties of THIS patch, so they belong on
           the row that represents it -- reading a value in one tab and having
           to change it in another is how a rig ends up configured differently
           from how it reads. */
        QMenu *midiModeMenu = NULL;
        QStringList midiModes;
        if (output && isMidi)
        {
            midiModes << "Control Change" << "Note Velocity" << "Program Change";
            midiModeMenu = menu.addMenu(tr("MIDI output mode"));
            const QVariant curV = patchParameter(uni, plugin, line, true, "mode");
            const QString cur = curV.isValid() ? curV.toString() : QString("Control Change");
            foreach (const QString &m, midiModes)
            {
                QAction *a = midiModeMenu->addAction(m);
                a->setCheckable(true);
                a->setChecked(m == cur);
            }
        }

        QMenu *midiChMenu = NULL;
        if (output == false && isMidi)
        {
            /* 0 means "every channel" in the plugin's encoding; the operator
               thinks in 1-16, so the menu counts the way the hardware is
               labelled and the translation happens here. */
            midiChMenu = menu.addMenu(tr("MIDI input channel"));
            const QVariant curV = patchParameter(uni, plugin, line, false, "midichannel");
            const int cur = curV.isValid() ? curV.toInt() : 0;
            QAction *anyCh = midiChMenu->addAction(tr("All channels"));
            anyCh->setCheckable(true);
            anyCh->setChecked(cur == 0);
            anyCh->setData(0);
            midiChMenu->addSeparator();
            for (int ch = 1; ch <= 16; ch++)
            {
                QAction *a = midiChMenu->addAction(tr("Channel %1").arg(ch));
                a->setCheckable(true);
                a->setChecked(cur == ch);
                a->setData(ch);
            }
        }

        QAction *inUni = NULL;
        if (output == false && isArtNet)
            inUni = menu.addAction(tr("Art-Net input universe…"));

        /* A MIDI port carries output and feedback over the same physical
           connection, so a universe uses it as one or the other, never both.
           Overview expresses that as a two-state combo; here it is the one
           action that swaps which side of the patch the line sits on, because
           "unpatch the output, then set the feedback to the same line" is the
           same intent spelled out in three steps. */
        QAction *roleSwap = NULL;
        if (isMidi && output)
            roleSwap = menu.addAction(tr("Use this MIDI port for feedback instead"));

        /* Feedback has a destination of its own, under the same outputIP /
           outputUni keys, and nothing outside the Overview grid could set it. */
        Universe *fbU = m_doc->inputOutputMap()
                        ? m_doc->inputOutputMap()->universe(uni) : NULL;
        QAction *fbTgt = (fbU != NULL && fbU->feedbackPatch() != NULL
                          && pluginSupportsTargetsIn(m_doc,
                                 fbU->feedbackPatch()->pluginName()))
            ? menu.addAction(tr("Change feedback address / port…")) : NULL;

        /* ArtNet transmit mode: Standard sends only on change (plus a periodic
           refresh), Full sends every tick. It changes the packet rate by
           orders of magnitude, so it belongs next to the patch it applies to
           rather than buried in a grid column. */
        QMenu *modeMenu = NULL;
        QStringList modes;
        if (output)
        {
            modes << "Standard" << "Full" << "Partial";
            modeMenu = menu.addMenu(tr("Transmit mode"));
            QString cur = "Standard";
            Universe *u = m_doc->inputOutputMap()
                          ? m_doc->inputOutputMap()->universe(uni) : NULL;
            if (u != NULL)
            {
                for (int i = 0; i < u->outputPatchesCount(); i++)
                {
                    OutputPatch *op = u->outputPatch(i);
                    if (op != NULL && op->plugin() != NULL
                            && op->plugin()->name() == plugin && op->output() == line)
                    {
                        cur = op->getPluginParameters()
                                .value("transmitMode", "Standard").toString();
                        break;
                    }
                }
            }
            foreach (const QString &m, modes)
            {
                QAction *a = modeMenu->addAction(m);
                a->setCheckable(true);
                a->setChecked(m == cur);
            }
        }

        /* Feedback: which output line carries status back to a controller.
           Listed as a submenu of real lines rather than a free-text field so
           an unreachable line cannot be typed in. */
        /* Feedback exists so a control surface can be lit or its motorised
           faders moved when the console changes -- it is sent by UI widgets
           back to whatever provides INPUT for this universe. Offering it from a
           universe's Art-Net OUTPUT row invited the reasonable question of what
           MIDI was doing in an Art-Net patch. Offer it where the controller
           relationship lives: the input row, or the feedback row itself. */
        QMenu *fbMenu = NULL;
        QAction *fbNone = NULL;
        QList<QPair<QString, quint32> > fbTargets;
        const bool feedbackRelevant =
            (output == false) || (item->text(COL_DETAIL) == tr("feedback"));
        if (feedbackRelevant)
        fbMenu = menu.addMenu(tr("Feedback to"));
        if (fbMenu != NULL)
        {
        fbNone = fbMenu->addAction(tr("None"));
        fbNone->setCheckable(true);
        OutputPatch *curFb = m_doc->inputOutputMap()
                             ? m_doc->inputOutputMap()->feedbackPatch(uni) : NULL;
        fbNone->setChecked(curFb == NULL || curFb->plugin() == NULL);
        if (m_doc->ioPluginCache() != NULL)
        {
            foreach (QLCIOPlugin *p, m_doc->ioPluginCache()->plugins())
            {
                /* Feedback is its own capability -- a plugin that can output
                   DMX cannot necessarily send anything back. Listing every
                   output line here offered targets that could never work. */
                if (p == NULL || (p->capabilities() & QLCIOPlugin::Feedback) == 0)
                    continue;
                const QStringList outs = p->outputs();
                for (int i = 0; i < outs.count(); i++)
                {
                    QAction *a = fbMenu->addAction(QString("%1: %2").arg(p->name())
                                                                    .arg(outs.at(i)));
                    a->setCheckable(true);
                    a->setChecked(curFb != NULL && curFb->plugin() != NULL
                                  && curFb->plugin()->name() == p->name()
                                  && curFb->output() == quint32(i));
                    fbTargets << qMakePair(p->name(), quint32(i));
                }
            }
        }
        }

        /* Input profile: how a controller's notes/CCs map to widgets. Only
           meaningful on an input patch, so do not offer it otherwise. */
        QMenu *profMenu = NULL;
        QStringList profNames;
        if (output == false && m_doc->inputOutputMap() != NULL)
        {
            profMenu = menu.addMenu(tr("Input profile"));
            QAction *pNone = profMenu->addAction(tr("None"));
            pNone->setCheckable(true);
            Universe *u = m_doc->inputOutputMap()->universe(uni);
            InputPatch *ip = u ? u->inputPatch() : NULL;
            const QString cur = ip ? ip->profileName() : QString();
            pNone->setChecked(cur.isEmpty());
            profNames = m_doc->inputOutputMap()->profileNames();
            profNames.sort();
            foreach (const QString &pn, profNames)
            {
                QAction *a = profMenu->addAction(pn);
                a->setCheckable(true);
                a->setChecked(pn == cur);
            }
        }

        /* Wording matters here: a universe can be patched to several lines, so
           removing it from THIS one must not read as deleting the universe.
           Conflating the two would quietly destroy a fan-out. */
        QAction *unp = menu.addAction(tr("Unpatch from this interface"));
        menu.addSeparator();
        QAction *del = menu.addAction(tr("Delete universe entirely…"));
        QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (chosen != NULL && (chosen == collapseAll || chosen == expandAll))
        { setExpandedDeep(item, chosen == expandAll); return; }
        if (chosen == NULL)
            return;

        if (fbMenu != NULL && chosen->parentWidget() == fbMenu)
        {
            if (chosen == fbNone)
                setFeedback(uni, QString(), 0);
            else
            {
                const int idx = fbMenu->actions().indexOf(chosen) - 1; // skip "None"
                if (idx >= 0 && idx < fbTargets.count())
                    setFeedback(uni, fbTargets.at(idx).first, fbTargets.at(idx).second);
            }
            return;
        }
        if (modeMenu != NULL && chosen->parentWidget() == modeMenu)
        {
            const int idx = modeMenu->actions().indexOf(chosen);
            if (idx >= 0 && idx < modes.count())
                setTransmitMode(uni, plugin, line, modes.at(idx));
            return;
        }
        if (profMenu != NULL && chosen->parentWidget() == profMenu)
        {
            const int idx = profMenu->actions().indexOf(chosen) - 1; // skip "None"
            setProfile(uni, idx < 0 ? QString() : profNames.value(idx));
            return;
        }

        if (bulk != NULL && chosen == bulk)
        {
            retargetSelection(QString());
            return;
        }
        if (midiModeMenu != NULL && chosen != NULL
                && midiModeMenu->actions().contains(chosen))
        {
            setPatchParameter(uni, plugin, line, true, "mode", chosen->text());
            return;
        }
        if (midiChMenu != NULL && chosen != NULL
                && midiChMenu->actions().contains(chosen))
        {
            setPatchParameter(uni, plugin, line, false, "midichannel",
                              chosen->data().toInt());
            return;
        }
        if (inUni != NULL && chosen == inUni)
        {
            const QVariant curV = patchParameter(uni, plugin, line, false, "inputUni");
            bool ok = false;
            const int v = QInputDialog::getInt(
                this, tr("Art-Net input universe"),
                tr("Which Art-Net universe does this input listen to?"),
                curV.isValid() ? curV.toInt() : 0, 0, 32767, 1, &ok);
            if (ok)
                setPatchParameter(uni, plugin, line, false, "inputUni", v);
            return;
        }
        if (roleSwap != NULL && chosen == roleSwap)
        {
            InputOutputMap *iom = m_doc->inputOutputMap();
            if (iom != NULL)
            {
                if (iom->patchUndo() != NULL)
                    iom->patchUndo()->capture(QList<quint32>() << uni,
                        tr("use MIDI port for feedback on universe %1").arg(uni + 1));

                /* Find this leg's index before dropping it: a universe fanned
                   out to several lines must lose the right one. */
                int index = 0;
                Universe *u = iom->universe(uni);
                if (u != NULL)
                {
                    for (int i = 0; i < u->outputPatchesCount(); i++)
                    {
                        OutputPatch *op = u->outputPatch(i);
                        if (op != NULL && op->plugin() != NULL
                                && op->plugin()->name() == plugin
                                && op->output() == line)
                        {
                            index = i;
                            break;
                        }
                    }
                }
                iom->setOutputPatch(uni, QString(), QString(), 0, false, index);
                iom->setOutputPatch(uni, plugin, QString(), line, true, 0);
                m_doc->setModified();
                refresh();
            }
            return;
        }
        if (fbTgt != NULL && chosen == fbTgt)
        {
            retargetFeedback(uni);
            return;
        }
        if (retgt != NULL && chosen == retgt)
            retargetPatch(uni, plugin, line);
        else if (chosen == ren)
            renameUniverse(uni);
        else if (chosen == unp)
            unpatchFromLine(uni, plugin, line, output);
        else if (chosen == del)
            deleteUniverse(uni);
        return;
    }
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
    if (output)
    {
        /* Append rather than replace: a universe may legitimately fan out to
           several nodes, and patching here should add this interface, not
           silently drop the ones already set. */
        done = iomap->setOutputPatch(uniId, pluginName, QString(), line, false,
                                     iomap->outputPatchesCount(uniId));
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
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *uni = iomap ? iomap->universe(universe) : NULL;
    if (uni == NULL)
        return false;

    for (int i = 0; i < uni->outputPatchesCount(); i++)
    {
        OutputPatch *op = uni->outputPatch(i);
        if (op == NULL || op->plugin() == NULL)
            continue;
        if (op->plugin()->name() != pluginName || op->output() != line)
            continue;

        /* Where a patch actually lands is protocol-specific and lives in the
           plugin parameters, not in the patch itself. For Art-Net that is the
           node IP plus a 15-bit port address encoded Net<<8 | Sub<<4 | Universe
           -- the same encoding ArtPollReply reports per port, which is what
           makes the two correlatable. A patch with neither is untargeted
           (broadcast, or simply never configured). */
        const QMap<QString, QVariant> params =
            const_cast<OutputPatch *>(op)->getPluginParameters();

        /* Absent parameters are not zero -- they are the plugin's defaults, and
           guessing wrong makes this view assert things that are false.
           ArtNetController::addUniverse defaults outputAddress to the BROADCAST
           address and outputUniverse to the universe's own id. Defaulting the
           port address to 0 instead made every patch that had an IP but no
           explicit universe collapse onto port 0, which showed up as several
           universes apparently bound to one physical port -- a configuration
           that would be a real fault if it were true. */
        if (params.contains("outputIP") == false)
            return false;   // broadcast: no single node to sit under
        address = params.value("outputIP").toString();
        portAddress = params.value("outputUni", universe).toUInt();
        return address.isEmpty() == false;
    }
    return false;
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
        tr("Patch which universe to %1 port %2?")
            .arg(deviceAddress).arg(portAddress & 0x0F),
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

/** Write outputIP / outputUni onto one universe's patch on one line.
 *
 *  Shared by the create path (patchUniverseToPort) and the edit path
 *  (retargetPatch) so a patch made by one and changed by the other cannot end
 *  up describing itself differently.
 */
bool ConnectionsTree::applyTarget(quint32 universe, const QString &pluginName,
                                  quint32 line, const QString &address,
                                  quint32 portAddress)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *uni = iomap ? iomap->universe(universe) : NULL;
    if (uni == NULL)
        return false;

    for (int i = 0; i < uni->outputPatchesCount(); i++)
    {
        OutputPatch *op = uni->outputPatch(i);
        if (op == NULL || op->plugin() == NULL)
            continue;
        if (op->plugin()->name() != pluginName || op->output() != line)
            continue;

        op->setPluginParameter("outputIP", address);
        op->setPluginParameter("outputUni", portAddress);
        return true;
    }
    return false;
}

/** Repoint an existing patch at a different node and port.
 *
 *  Until now the only way to change where a patch landed was to unpatch it and
 *  patch it again, because the address and port were only ever asked for while
 *  CREATING one. A node swapped for a spare, or renumbered on its front panel,
 *  is an ordinary evening -- it should not cost the patch and everything else
 *  hanging off it. Prefilled with the current values so the common edit is one
 *  character.
 */
void ConnectionsTree::retargetPatch(quint32 universe, const QString &pluginName,
                                    quint32 line)
{
    QString curAddr;
    quint32 curPort = 0;
    const bool targeted = patchTarget(universe, pluginName, line, curAddr, curPort);

    /* Captured before anything is asked, not after the dialogs are answered:
       an operator who changes their mind halfway leaves the held step
       describing a state that is still current, which is harmless, whereas
       capturing late can miss the change entirely. */
    InputOutputMap *undoMap = m_doc->inputOutputMap();
    if (undoMap != NULL && undoMap->patchUndo() != NULL)
        undoMap->patchUndo()->capture(QList<quint32>() << universe,
                                      tr("retarget universe %1").arg(universe + 1));

    bool ok = false;
    const QString addr = QInputDialog::getText(
        this, tr("Retarget patch"),
        tr("Send this universe to which address? Leave empty to broadcast:"),
        QLineEdit::Normal, targeted ? curAddr : QString(), &ok).trimmed();
    if (ok == false)
        return;

    /* An empty address means broadcast, which is a real and deliberate choice
       -- it is how you drive every node on a segment at once -- so clear the
       parameters rather than refusing. */
    if (addr.isEmpty())
    {
        InputOutputMap *iomap = m_doc->inputOutputMap();
        Universe *uni = iomap ? iomap->universe(universe) : NULL;
        if (uni != NULL)
        {
            for (int i = 0; i < uni->outputPatchesCount(); i++)
            {
                OutputPatch *op = uni->outputPatch(i);
                if (op == NULL || op->plugin() == NULL)
                    continue;
                if (op->plugin()->name() != pluginName || op->output() != line)
                    continue;
                op->unSetPluginParameter("outputIP");
                op->unSetPluginParameter("outputUni");
                break;
            }
        }
        m_doc->setModified();
        refresh();
        return;
    }

    /* Asked as net:subnet:universe, the same terms the port rows show and the
       node's own panel prints, rather than the packed 15-bit number the
       protocol stores. */
    const QString cur = QString("%1:%2:%3").arg((curPort >> 8) & 0x7F)
                        .arg((curPort >> 4) & 0x0F).arg(curPort & 0x0F);
    const QString txt = QInputDialog::getText(
        this, tr("Retarget patch"),
        tr("Port address on %1, as net:subnet:universe:").arg(addr),
        QLineEdit::Normal, targeted ? cur : QString("0:0:0"), &ok).trimmed();
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
        QMessageBox::warning(this, tr("Retarget patch"),
            tr("\"%1\" is not a port address. Expected net:subnet:universe, "
               "with net 0-127 and subnet and universe 0-15.").arg(txt));
        return;
    }

    if (applyTarget(universe, pluginName, line, addr,
                    quint32((net << 8) | (sub << 4) | uni)) == false)
    {
        QMessageBox::warning(this, tr("Retarget patch"),
                             tr("That universe is no longer patched to %1.")
                                 .arg(pluginName));
        return;
    }
    m_doc->setModified();
    refresh();
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
 *  universe in selection order, which is the order they are shown in.
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
    const QList<quint32> unis = selectedUniverses(plug, lines);
    if (unis.count() < 2 || plug.isEmpty())
        return;

    bool ok = false;
    const QString addr = QInputDialog::getText(
        this, tr("Retarget %1 universes").arg(unis.count()),
        tr("Send all %1 selected universes to which address?").arg(unis.count()),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (ok == false || addr.isEmpty())
        return;

    const QString txt = QInputDialog::getText(
        this, tr("Retarget %1 universes").arg(unis.count()),
        tr("Port address for the FIRST of them, as net:subnet:universe.\n"
           "The rest follow on consecutive ports."),
        QLineEdit::Normal, QString("0:0:0"), &ok).trimmed();
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

    int applied = 0;
    for (int i = 0; i < unis.count(); i++)
    {
        if (applyTarget(unis.at(i), plug, lines.at(i), addr, first + quint32(i)))
            applied++;
    }

    m_doc->setModified();
    refresh();

    if (applied != unis.count())
    {
        QMessageBox::warning(this, tr("Retarget %1 universes").arg(unis.count()),
            tr("%1 of %2 were repointed; the rest are no longer patched to %3.")
                .arg(applied).arg(unis.count()).arg(plug));
    }
}

/** Declare a target discovery cannot find.
 *
 *  The address is the identity -- it is what a patch points at and the only
 *  handle a node that has never answered has -- so it is required and the
 *  name is not. A name typed here is stored as an ordinary target alias, the
 *  same field the inline rename writes, so when the real node does show up it
 *  arrives already labelled instead of reverting to a bare IP.
 */
void ConnectionsTree::addManualTarget(const QString &pluginName, quint32 line)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

    bool ok = false;
    const QString addr = QInputDialog::getText(
        this, tr("Add target"),
        tr("Address of the target (an IP, or a hostname this console can "
           "resolve):"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (ok == false || addr.isEmpty())
        return;

    if (iomap->isManualTarget(addr))
    {
        QMessageBox::information(this, tr("Add target"),
            tr("%1 is already declared.").arg(addr));
        return;
    }

    const QString name = QInputDialog::getText(
        this, tr("Add target"),
        tr("A name for %1 — optional:").arg(addr),
        QLineEdit::Normal, QString(), &ok).trimmed();

    iomap->addManualTarget(pluginName, line, addr);
    if (ok && name.isEmpty() == false)
        iomap->setTargetAlias(addr, name);
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
    if (ekind != KIND_DEVICE && ekind != KIND_LINE)
        return;

    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return;

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

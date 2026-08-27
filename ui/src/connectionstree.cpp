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
    , m_status(NULL)
    , m_populatedOnce(false)
    , m_rebuilding(false)
    , m_since(new QElapsedTimer)
    , m_tree(NULL)
    , m_refreshTimer(NULL)
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
    lay->addWidget(m_showUnused);

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
    m_refreshTimer->start();

    m_since->start();
    refresh();
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

    /* Remember what the operator COLLAPSED, keyed by full path.
       The previous version saved the expanded set, matched it back with
       findItems() on the display name, and then called expandToDepth() every
       refresh anyway -- so a closed branch reopened itself within five seconds,
       and identically-named rows under different parents dragged each other
       open. Tracking collapse instead means the default stays "open" without
       fighting the operator. */
    if (m_populatedOnce)
    {
        m_collapsed.clear();
        QList<QTreeWidgetItem *> stack;
        for (int i = 0; i < m_tree->topLevelItemCount(); i++)
            stack << m_tree->topLevelItem(i);
        while (stack.isEmpty() == false)
        {
            QTreeWidgetItem *it = stack.takeFirst();
            if (it->childCount() > 0 && it->isExpanded() == false)
                m_collapsed.insert(itemPath(it));
            for (int c = 0; c < it->childCount(); c++)
                stack << it->child(c);
        }
    }

    m_rebuilding = true;
    m_tree->clear();

    const bool showAll = (m_showUnused != NULL && m_showUnused->isChecked());

    IOPluginCache *cache = m_doc->ioPluginCache();
    if (cache == NULL)
        return;

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

            /* "Live" means something is patched to it or something was heard on
               it. An interface that is merely present is not interesting. */
            if (showAll == false && outUnis.isEmpty() && inUnis.isEmpty()
                    && fbUnis.isEmpty() && devicesHere == 0)
                continue;

            QTreeWidgetItem *litem = new QTreeWidgetItem(pitem);
            litem->setText(COL_NAME, label);
            litem->setData(COL_NAME, ROLE_KIND, KIND_LINE);
            litem->setData(COL_NAME, ROLE_PLUGIN, plugin->name());
            litem->setData(COL_NAME, ROLE_LINE, quint32(line));

            QStringList roles;
            if (line < outLines.count()) roles << tr("output");
            if (line < inLines.count()) roles << tr("input");
            litem->setText(COL_DETAIL, roles.join(", "));

            const int total = outUnis.count() + inUnis.count() + fbUnis.count();
            litem->setText(COL_CARRIES, total == 0 ? tr("nothing patched")
                                                   : tr("%1 universes").arg(total));

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
                if (dev.status.isEmpty() == false)
                    ditem->setToolTip(COL_DETAIL, dev.status);

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
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *top = m_tree->topLevelItem(i);
        if (top->data(COL_NAME, ROLE_KIND).toInt() != KIND_PLUGIN)
            continue;

        int ifaces = 0, nodes = 0, unheard = 0, unis = 0;
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

    /* Depth 3 now: protocol > interface > device > port, with universes
       hanging under the port they leave by. */
    /* Everything opens by default -- the interesting rows are the leaves --
       then re-close whatever the operator had closed. */
    m_tree->expandAll();
    QList<QTreeWidgetItem *> stack;
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
        stack << m_tree->topLevelItem(i);
    while (stack.isEmpty() == false)
    {
        QTreeWidgetItem *it = stack.takeFirst();
        if (m_collapsed.contains(itemPath(it)))
            it->setExpanded(false);
        for (int c = 0; c < it->childCount(); c++)
            stack << it->child(c);
    }
    m_populatedOnce = true;
    m_rebuilding = false;

    if (m_status != NULL)
        m_status->setVisible(m_since->elapsed() < 6000);
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

void ConnectionsTree::slotContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (item == NULL)
        return;

    const int kind = item->data(COL_NAME, ROLE_KIND).toInt();
    const QString plugin = item->data(COL_NAME, ROLE_PLUGIN).toString();
    const quint32 line = item->data(COL_NAME, ROLE_LINE).toUInt();

    QMenu menu(this);

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
        QAction *ren = menu.addAction(tr("Name this target…"));
        QAction *pick1 = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (pick1 != NULL && (pick1 == collapseAll || pick1 == expandAll))
        { setExpandedDeep(item, pick1 == expandAll); return; }
        if (pick1 == ren)
            renameTarget(addr);
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
            QAction *unp = menu.addAction(tr("Unpatch from this interface"));
            menu.addSeparator();
            QAction *del = menu.addAction(tr("Delete universe entirely…"));
            QAction *c = menu.exec(m_tree->viewport()->mapToGlobal(pos));
            if (c != NULL && (c == collapseAll || c == expandAll))
            { setExpandedDeep(item, c == expandAll); return; }
            if (c == ren)
                renameUniverse(uni);
            else if (c == unp)
                unpatchFromLine(uni, plugin, line,
                                item->data(COL_NAME, ROLE_OUTPUT).toBool());
            else if (c == del)
                deleteUniverse(uni);
            return;
        }

        QAction *p = menu.addAction(tr("Patch a universe to this port…"));
        QAction *pick2 = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (pick2 != NULL && (pick2 == collapseAll || pick2 == expandAll))
        { setExpandedDeep(item, pick2 == expandAll); return; }
        if (pick2 == p)
            patchUniverseToPort(plugin, line, addr, portAddr);
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

        QAction *pNew = NULL;
        if (canOut)
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
        if (pNew != NULL && chosen == pNew)
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

        if (chosen == ren)
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

    /* Deleting a universe orphans every fixture patched into it, which is not
       recoverable from this view, so make the consequence explicit. */
    if (QMessageBox::question(
            this, tr("Delete universe"),
            tr("Delete \"%1\" completely?\n\nAny fixtures patched into it will "
               "lose their output.").arg(uni->name()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    iomap->removeUniverse(int(universe));
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
    Universe *uni = iomap->universe(uniId);
    if (uni != NULL)
    {
        for (int i = 0; i < uni->outputPatchesCount(); i++)
        {
            OutputPatch *op = uni->outputPatch(i);
            if (op != NULL && op->plugin() != NULL
                    && op->plugin()->name() == pluginName && op->output() == line)
            {
                op->setPluginParameter("outputIP", deviceAddress);
                op->setPluginParameter("outputUni", portAddress);
                break;
            }
        }
    }
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
    if (item->data(COL_NAME, ROLE_KIND).toInt() != KIND_DEVICE)
        return;

    const QString addr = item->data(COL_NAME, ROLE_ADDRESS).toString();
    if (addr.isEmpty())
        return;

    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
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

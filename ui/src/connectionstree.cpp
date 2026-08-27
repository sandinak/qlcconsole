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
#include <QCheckBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QHash>
#include <QSet>

#include "connectionstree.h"
#include "qlcioplugin.h"
#include "ioplugincache.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "universe.h"
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
    , m_populatedOnce(false)
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

    refresh();
}

ConnectionsTree::~ConnectionsTree()
{
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

void ConnectionsTree::refresh()
{
    if (m_tree == NULL || m_doc == NULL)
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

            int devicesHere = 0;
            foreach (const QLCIOPlugin::Device &d, devices)
                if (d.line == quint32(line))
                    devicesHere++;

            /* "Live" means something is patched to it or something was heard on
               it. An interface that is merely present is not interesting. */
            if (showAll == false && outUnis.isEmpty() && inUnis.isEmpty()
                    && devicesHere == 0)
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

            const int total = outUnis.count() + inUnis.count();
            litem->setText(COL_CARRIES, total == 0 ? tr("nothing patched")
                                                   : tr("%1 universes").arg(total));

            /* Port rows, keyed by "node address|port address", so a universe
               can be filed under the port it is actually aimed at. */
            QHash<QString, QTreeWidgetItem *> portRows;

            /* Devices heard on this line. Most plugins report none -- for a
               USB widget or a MIDI port the line already IS the device, and
               inventing a second level for it would only add depth. */
            /* A node can be heard more than once on one line -- it answers on
               every interface it has, and the replies arrive from different
               source addresses while carrying the same node identity. Key on
               what identifies the hardware, not on who delivered the packet. */
            QSet<QString> seenDevices;
            foreach (const QLCIOPlugin::Device &dev, devices)
            {
                if (dev.line != quint32(line))
                    continue;

                const QString devKey = dev.address + "|" + dev.hardwareId;
                if (seenDevices.contains(devKey))
                    continue;
                seenDevices.insert(devKey);

                QTreeWidgetItem *ditem = new QTreeWidgetItem(litem);
                ditem->setText(COL_NAME, dev.name.isEmpty() ? dev.address : dev.name);
                ditem->setData(COL_NAME, ROLE_KIND, KIND_DEVICE);

                QStringList detail;
                if (dev.address.isEmpty() == false) detail << dev.address;
                if (dev.hardwareId.isEmpty() == false) detail << dev.hardwareId;
                if (dev.detail.isEmpty() == false) detail << dev.detail;
                detail << (dev.rdmCapable ? tr("RDM") : tr("no RDM"));
                ditem->setText(COL_DETAIL, detail.join(" \u00b7 "));
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
                                parentItem = pr;
                            else
                                /* Aimed at a node we have not heard from -- it is
                                   still patched, and where it is aimed is the
                                   whole point, so show it rather than leaving the
                                   row looking unconfigured. */
                                targetText = tr("→ %1 uni %2").arg(addr).arg(portAddr);
                        }
                    }

                    QTreeWidgetItem *uitem = new QTreeWidgetItem(parentItem);
                    uitem->setText(COL_NAME, uni->name());
                    /* Show feedback and profile state on the row itself.
                       Both were previously only visible by opening the
                       per-universe editor, which meant the answer to "is LED
                       feedback actually wired up" required going somewhere
                       else. */
                    QStringList udetail;
                    udetail << (output ? tr("output") : tr("input"));
                    if (targetText.isEmpty() == false)
                        udetail << targetText;
                    if (output == false)
                    {
                        InputPatch *ip = uni->inputPatch();
                        if (ip != NULL && ip->profileName().isEmpty() == false)
                            udetail << tr("profile: %1").arg(ip->profileName());
                    }
                    OutputPatch *fb = iomap->feedbackPatch(uniId);
                    if (fb != NULL && fb->plugin() != NULL)
                        udetail << tr("feedback: %1").arg(fb->plugin()->name());
                    uitem->setText(COL_DETAIL, udetail.join(" \u00b7 "));
                    uitem->setData(COL_NAME, ROLE_KIND, KIND_UNIVERSE);
                    uitem->setData(COL_NAME, ROLE_UNIVERSE, uniId);
                    uitem->setData(COL_NAME, ROLE_PLUGIN, plugin->name());
                    uitem->setData(COL_NAME, ROLE_LINE, quint32(line));
                    uitem->setData(COL_NAME, ROLE_OUTPUT, output);
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
    /* An empty tree is ambiguous -- "nothing is connected" and "the view is
       broken" look identical. Say which. */
    if (m_tree->topLevelItemCount() == 0)
    {
        QTreeWidgetItem *none = new QTreeWidgetItem(m_tree);
        none->setText(COL_NAME, tr("Nothing connected"));
        none->setText(COL_DETAIL, showAll
            ? tr("No I/O plugins are available.")
            : tr("No interface has a universe patched to it and no devices "
                 "were heard. Tick \"Show unused\" to patch one."));
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

void ConnectionsTree::slotContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (item == NULL)
        return;

    const int kind = item->data(COL_NAME, ROLE_KIND).toInt();
    const QString plugin = item->data(COL_NAME, ROLE_PLUGIN).toString();
    const quint32 line = item->data(COL_NAME, ROLE_LINE).toUInt();

    QMenu menu(this);

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
        if (p == NULL || p->canConfigure() == false)
            return;
        QAction *cfg = menu.addAction(tr("Configure %1…").arg(plugin));
        if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == cfg)
            configurePlugin(plugin);
        return;
    }

    if (kind == KIND_PORT)
    {
        const QString addr = item->data(COL_NAME, ROLE_ADDRESS).toString();
        const quint32 portAddr = item->data(COL_NAME, ROLE_PORTADDR).toUInt();
        QAction *p = menu.addAction(tr("Patch a universe to this port…"));
        if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == p)
            patchUniverseToPort(plugin, line, addr, portAddr);
        return;
    }

    if (kind == KIND_LINE)
    {
        QAction *pOut = menu.addAction(tr("Patch a universe here (output)…"));
        QAction *pIn = menu.addAction(tr("Patch a universe here (input)…"));
        QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (chosen == pOut)
            patchUniverseTo(plugin, line, true);
        else if (chosen == pIn)
            patchUniverseTo(plugin, line, false);
        return;
    }

    if (kind == KIND_UNIVERSE)
    {
        const quint32 uni = item->data(COL_NAME, ROLE_UNIVERSE).toUInt();
        const bool output = item->data(COL_NAME, ROLE_OUTPUT).toBool();

        QAction *ren = menu.addAction(tr("Rename universe…"));

        /* Feedback: which output line carries status back to a controller.
           Listed as a submenu of real lines rather than a free-text field so
           an unreachable line cannot be typed in. */
        QMenu *fbMenu = menu.addMenu(tr("Feedback to"));
        QAction *fbNone = fbMenu->addAction(tr("None"));
        fbNone->setCheckable(true);
        OutputPatch *curFb = m_doc->inputOutputMap()
                             ? m_doc->inputOutputMap()->feedbackPatch(uni) : NULL;
        fbNone->setChecked(curFb == NULL || curFb->plugin() == NULL);
        QList<QPair<QString, quint32> > fbTargets;
        if (m_doc->ioPluginCache() != NULL)
        {
            foreach (QLCIOPlugin *p, m_doc->ioPluginCache()->plugins())
            {
                if (p == NULL)
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
        if (chosen == NULL)
            return;

        if (chosen->parentWidget() == fbMenu)
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
        if (params.contains("outputIP") == false)
            return false;
        address = params.value("outputIP").toString();
        portAddress = params.value("outputUni", 0).toUInt();
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

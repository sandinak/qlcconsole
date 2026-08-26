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

#include "connectionstree.h"
#include "qlcioplugin.h"
#include "ioplugincache.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "universe.h"
#include "doc.h"

#define COL_NAME    0
#define COL_DETAIL  1
#define COL_CARRIES 2

ConnectionsTree::ConnectionsTree(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_showUnused(NULL)
    , m_tree(NULL)
    , m_refreshTimer(NULL)
{
    Q_ASSERT(doc != NULL);

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(6, 6, 6, 6);

    QLabel *hint = new QLabel(
        tr("How this console is wired, following the signal outward: protocol → "
           "interface → devices heard on it → the universes they carry. "
           "Read-only; patch in Overview or Detailed."), this);
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

QStringList ConnectionsTree::universesOn(const QString &pluginName, quint32 line,
                                         bool output) const
{
    QStringList list;
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
                    list << uni->name();
                    break;
                }
            }
        }
        else
        {
            InputPatch *ip = uni->inputPatch();
            if (ip != NULL && ip->plugin() != NULL
                    && ip->plugin()->name() == pluginName && ip->input() == line)
                list << uni->name();
        }
        id++;
    }
    return list;
}

void ConnectionsTree::refresh()
{
    if (m_tree == NULL || m_doc == NULL)
        return;

    /* Remember what was expanded so a periodic refresh does not collapse the
       tree under the operator mid-look. */
    QStringList expanded;
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *top = m_tree->topLevelItem(i);
        for (int j = 0; j < top->childCount(); j++)
        {
            if (top->child(j)->isExpanded())
                expanded << top->text(COL_NAME) + "/" + top->child(j)->text(COL_NAME);
        }
        if (top->isExpanded())
            expanded << top->text(COL_NAME);
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
        pitem->setFirstColumnSpanned(false);

        QList<QLCIOPlugin::Device> devices = plugin->discoveredDevices();

        const QStringList outLines = plugin->outputs();
        const QStringList inLines = plugin->inputs();
        const int lineCount = qMax(outLines.count(), inLines.count());

        for (int line = 0; line < lineCount; line++)
        {
            const QString label = line < outLines.count() ? outLines.at(line)
                                  : inLines.at(line);

            QStringList carries = universesOn(plugin->name(), quint32(line), true);
            carries += universesOn(plugin->name(), quint32(line), false);
            carries.removeDuplicates();

            int devicesHere = 0;
            foreach (const QLCIOPlugin::Device &d, devices)
                if (d.line == quint32(line))
                    devicesHere++;

            /* "Live" means something is patched to it or something was heard on
               it. An interface that is merely present is not interesting. */
            if (showAll == false && carries.isEmpty() && devicesHere == 0)
                continue;

            QTreeWidgetItem *litem = new QTreeWidgetItem(pitem);
            litem->setText(COL_NAME, label);

            QStringList roles;
            if (line < outLines.count()) roles << tr("output");
            if (line < inLines.count()) roles << tr("input");
            litem->setText(COL_DETAIL, roles.join(", "));

            /* A busy interface can carry dozens of universes; the full list
               just truncates mid-name and tells you nothing. Summarise, and
               keep the detail one hover away. */
            if (carries.count() > 4)
            {
                litem->setText(COL_CARRIES, tr("%1 universes").arg(carries.count()));
                litem->setToolTip(COL_CARRIES, carries.join("\n"));
            }
            else
            {
                litem->setText(COL_CARRIES, carries.join(", "));
            }

            /* Devices heard on this line. Most plugins report none -- for a
               USB widget or a MIDI port the line already IS the device, and
               inventing a second level for it would only add depth. */
            foreach (const QLCIOPlugin::Device &dev, devices)
            {
                if (dev.line != quint32(line))
                    continue;

                QTreeWidgetItem *ditem = new QTreeWidgetItem(litem);
                ditem->setText(COL_NAME, dev.name.isEmpty() ? dev.address : dev.name);

                QStringList detail;
                if (dev.address.isEmpty() == false) detail << dev.address;
                if (dev.hardwareId.isEmpty() == false) detail << dev.hardwareId;
                if (dev.detail.isEmpty() == false) detail << dev.detail;
                detail << (dev.rdmCapable ? tr("RDM") : tr("no RDM"));
                ditem->setText(COL_DETAIL, detail.join(" · "));
                if (dev.status.isEmpty() == false)
                    ditem->setToolTip(COL_DETAIL, dev.status);

                /* One row per physical port: Art-Net addresses per port and
                   RDM discovery is per port, so collapsing them would hide the
                   level the protocol actually works at. */
                for (int p = 0; p < dev.portLabels.count(); p++)
                {
                    QTreeWidgetItem *pt = new QTreeWidgetItem(ditem);
                    pt->setText(COL_NAME, tr("port %1").arg(p + 1));
                    pt->setText(COL_DETAIL, dev.portLabels.at(p));
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

    m_tree->expandToDepth(2);
    foreach (const QString &path, expanded)
    {
        QList<QTreeWidgetItem *> hits =
            m_tree->findItems(path.section('/', -1), Qt::MatchExactly | Qt::MatchRecursive, COL_NAME);
        foreach (QTreeWidgetItem *it, hits)
            it->setExpanded(true);
    }
}

/*
  Q Light Controller Plus
  monitorinventorypanel.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <algorithm>
#include <QDataStream>
#include <QDrag>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QMap>
#include <QSet>

#include "monitorinventorypanel.h"
#include "monitorgraphicsview.h"
#include "qlcfixturedef.h"
#include "fixture.h"
#include "doc.h"

#define ROLE_FID   (Qt::UserRole)        // a fixture leaf
#define ROLE_MODEL (Qt::UserRole + 1)    // a model node

InventoryTree::InventoryTree(QWidget *parent)
    : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setRootIsDecorated(true);
}

static void collectFixtureIds(QTreeWidgetItem *node, QList<quint32> &ids)
{
    if (node->data(0, ROLE_FID).isValid())
    {
        const quint32 fid = node->data(0, ROLE_FID).toUInt();
        if (!node->isHidden() && !ids.contains(fid))
            ids << fid;
        return;
    }
    for (int c = 0; c < node->childCount(); c++)
        collectFixtureIds(node->child(c), ids);
}

QList<quint32> InventoryTree::selectedFixtureIds() const
{
    // A manufacturer or model folder stands for every fixture beneath it.
    QList<quint32> ids;
    foreach (QTreeWidgetItem *it, selectedItems())
        collectFixtureIds(it, ids);
    return ids;
}

QMimeData *InventoryTree::fixtureMime(const QList<quint32> &ids)
{
    QByteArray data;
    QDataStream s(&data, QIODevice::WriteOnly);
    foreach (quint32 fid, ids)
        s << fid;

    QMimeData *mime = new QMimeData;
    mime->setData(QStringLiteral("application/x-qlc-fid"), data);
    return mime;
}

void InventoryTree::startDrag(Qt::DropActions supportedActions)
{
    const QList<quint32> ids = selectedFixtureIds();
    if (ids.isEmpty())
        return;

    QDrag *drag = new QDrag(this);
    drag->setMimeData(fixtureMime(ids));
    drag->exec(supportedActions, Qt::CopyAction);
}

MonitorInventoryPanel::MonitorInventoryPanel(Doc *doc, MonitorGraphicsView *view, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_view(view)
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    QHBoxLayout *head = new QHBoxLayout;
    QLabel *title = new QLabel(tr("<b>Inventory</b>"), this);
    head->addWidget(title);
    head->addStretch();
    QToolButton *close = new QToolButton(this);
    close->setText(QString::fromUtf8("\xC3\x97"));
    close->setAutoRaise(true);
    close->setToolTip(tr("Hide the inventory"));
    head->addWidget(close);
    root->addLayout(head);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Filter"));
    m_filter->setClearButtonEnabled(true);
    root->addWidget(m_filter);

    m_tree = new InventoryTree(this);
    root->addWidget(m_tree, 1);

    m_count = new QLabel(this);
    m_count->setWordWrap(true);
    root->addWidget(m_count);

    QPushButton *add = new QPushButton(tr("Add to inventory..."), this);
    root->addWidget(add);

    connect(close, &QToolButton::clicked, this, &MonitorInventoryPanel::closeRequested);
    connect(add, &QPushButton::clicked, this, &MonitorInventoryPanel::addRequested);
    connect(m_filter, &QLineEdit::textChanged, this, [this]() { reload(); });

    reload();
}

void MonitorInventoryPanel::reload()
{
    const QString needle = m_filter->text().trimmed();
    const QList<quint32> onPlot = m_view != NULL ? m_view->fixturesID() : QList<quint32>();

    // Remember which folders were collapsed so a reload does not re-open them.
    QSet<QString> collapsed;
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *m = m_tree->topLevelItem(i);
        if (!m->isExpanded())
            collapsed.insert(m->data(0, ROLE_MODEL).toString());
        for (int d = 0; d < m->childCount(); d++)
            if (!m->child(d)->isExpanded())
                collapsed.insert(m->child(d)->data(0, ROLE_MODEL).toString());
    }

    m_tree->clear();

    // manufacturer -> model -> fixtures, all sorted.
    QMap<QString, QMap<QString, QList<Fixture *> > > byManuf;
    int total = 0, network = 0;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL || fx->isPatched() || onPlot.contains(fx->id()))
            continue;
        total++;
        if (fx->connection() == Fixture::Network)
            network++;
        const QString manuf = fx->groupingManufacturer();
        const QString model = fx->groupingModel();
        if (!needle.isEmpty()
            && !fx->name().contains(needle, Qt::CaseInsensitive)
            && !manuf.contains(needle, Qt::CaseInsensitive)
            && !model.contains(needle, Qt::CaseInsensitive))
            continue;
        byManuf[manuf][model].append(fx);
    }

    QMapIterator<QString, QMap<QString, QList<Fixture *> > > mit(byManuf);
    while (mit.hasNext())
    {
        mit.next();
        int manufCount = 0;
        foreach (const QList<Fixture *> &l, mit.value())
            manufCount += l.size();

        QTreeWidgetItem *mNode = new QTreeWidgetItem(m_tree);
        mNode->setText(0, QStringLiteral("%1 (%2)").arg(mit.key()).arg(manufCount));
        mNode->setData(0, ROLE_MODEL, mit.key());
        mNode->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        QMapIterator<QString, QList<Fixture *> > dit(mit.value());
        while (dit.hasNext())
        {
            dit.next();
            QList<Fixture *> fxs = dit.value();
            std::sort(fxs.begin(), fxs.end(), [](Fixture *a, Fixture *b) {
                return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
            });

            const QString key = mit.key() + QLatin1Char('\x1f') + dit.key();
            QTreeWidgetItem *dNode = new QTreeWidgetItem(mNode);
            dNode->setText(0, QStringLiteral("%1 (%2)").arg(dit.key()).arg(fxs.size()));
            dNode->setData(0, ROLE_MODEL, key);
            dNode->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

            foreach (Fixture *fx, fxs)
            {
                QTreeWidgetItem *leaf = new QTreeWidgetItem(dNode);
                leaf->setText(0, fx->connection() == Fixture::Network
                                     ? tr("%1  [network]").arg(fx->name()) : fx->name());
                leaf->setIcon(0, fx->getIconFromType());
                leaf->setData(0, ROLE_FID, fx->id());
                leaf->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
                leaf->setToolTip(0, tr("Drag onto the plot -- onto a truss, riser or tower to mount it"));
            }
            dNode->setExpanded(!collapsed.contains(key));
        }
        mNode->setExpanded(!collapsed.contains(mit.key()));
    }

    if (total == 0)
        m_count->setText(tr("Nothing waiting. Fixtures staged with Add to inventory appear here "
                            "until they are placed."));
    else
        m_count->setText(tr("%1 waiting to be placed (%2 network).").arg(total).arg(network));
}

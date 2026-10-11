/*
  Q Light Controller Plus
  networktopologydialog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "networktopologydialog.h"
#include "feetinchesspinbox.h"
#include "monitorproperties.h"
#include "inputoutputmap.h"
#include "doc.h"

#define COL_PORT     0
#define COL_UNIVERSE 1
#define COL_RECEIVER 2
#define COL_RX_X     3
#define COL_RX_Y     4

NetworkTopologyDialog::NetworkTopologyDialog(Doc *doc, int selectNode, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_nodes(doc->networkTopology()->nodes())
    , m_row(-1)
    , m_loading(false)
{
    setProperty("helpTopic", QStringLiteral("lighting-studio.md#network-nodes-and-cable-runs"));
    setWindowTitle(tr("Network Topology"));
    resize(880, 560);

    QHBoxLayout *root = new QHBoxLayout(this);

    // Left: the nodes.
    QVBoxLayout *left = new QVBoxLayout;
    left->addWidget(new QLabel(tr("Nodes"), this));
    m_list = new QListWidget(this);
    left->addWidget(m_list, 1);
    QHBoxLayout *leftBtns = new QHBoxLayout;
    m_addNodeBtn = new QPushButton(tr("Add node"), this);
    m_removeNodeBtn = new QPushButton(tr("Remove"), this);
    leftBtns->addWidget(m_addNodeBtn);
    leftBtns->addWidget(m_removeNodeBtn);
    left->addLayout(leftBtns);
    root->addLayout(left, 1);

    // Right: the selected node, its ports, and the warnings.
    QVBoxLayout *right = new QVBoxLayout;
    QFormLayout *form = new QFormLayout;
    m_name = new QLineEdit(this);
    form->addRow(tr("Name:"), m_name);

    m_transport = new QComboBox(this);
    m_transport->addItem(tr("ArtNet"), int(NetworkNode::ArtNet));
    m_transport->addItem(tr("sACN"), int(NetworkNode::sACN));
    m_transport->addItem(tr("Interface"), int(NetworkNode::Interface));
    m_transport->addItem(tr("Wireless DMX"), int(NetworkNode::WirelessDMX));
    form->addRow(tr("Transport:"), m_transport);

    m_ip = new QLineEdit(this);
    m_ip->setPlaceholderText(tr("informational, e.g. 172.18.2.31"));
    form->addRow(tr("IP:"), m_ip);
    right->addLayout(form);

    right->addWidget(new QLabel(tr("Ports (each carries one universe). For a wireless node, "
                                   "place each port's receiver -- the box in the rig, not "
                                   "the transmitter."), this));
    m_ports = new QTableWidget(0, 5, this);
    m_ports->setHorizontalHeaderLabels({ tr("Port"), tr("Universe"), tr("Receiver placed"),
                                         tr("Receiver X"), tr("Receiver Y") });
    m_ports->verticalHeader()->hide();
    m_ports->horizontalHeader()->setStretchLastSection(true);
    m_ports->horizontalHeader()->setSectionResizeMode(COL_UNIVERSE, QHeaderView::Stretch);
    m_ports->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ports->setSelectionMode(QAbstractItemView::SingleSelection);
    right->addWidget(m_ports, 2);

    QHBoxLayout *portBtns = new QHBoxLayout;
    m_addPortBtn = new QPushButton(tr("Add port"), this);
    m_removePortBtn = new QPushButton(tr("Remove port"), this);
    portBtns->addWidget(m_addPortBtn);
    portBtns->addWidget(m_removePortBtn);
    portBtns->addStretch();
    right->addLayout(portBtns);

    right->addWidget(new QLabel(tr("Warnings"), this));
    m_warnings = new QListWidget(this);
    m_warnings->setSelectionMode(QAbstractItemView::NoSelection);
    right->addWidget(m_warnings, 1);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    right->addWidget(bb);
    root->addLayout(right, 3);

    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::currentRowChanged, this, &NetworkTopologyDialog::slotNodeRowChanged);
    connect(m_addNodeBtn, &QPushButton::clicked, this, &NetworkTopologyDialog::slotAddNode);
    connect(m_removeNodeBtn, &QPushButton::clicked, this, &NetworkTopologyDialog::slotRemoveNode);
    connect(m_name, &QLineEdit::textEdited, this, &NetworkTopologyDialog::slotNodeFieldEdited);
    connect(m_ip, &QLineEdit::textEdited, this, &NetworkTopologyDialog::slotNodeFieldEdited);
    connect(m_transport, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NetworkTopologyDialog::slotNodeFieldEdited);
    connect(m_addPortBtn, &QPushButton::clicked, this, &NetworkTopologyDialog::slotAddPort);
    connect(m_removePortBtn, &QPushButton::clicked, this, &NetworkTopologyDialog::slotRemovePort);
    connect(m_ports, &QTableWidget::itemChanged, this, &NetworkTopologyDialog::slotPortCellEdited);

    const int first = (selectNode >= 0 && selectNode < m_nodes.size()) ? selectNode : 0;
    reloadList(m_nodes.isEmpty() ? -1 : first);
    refreshWarnings();
}

void NetworkTopologyDialog::reloadList(int selectRow)
{
    m_loading = true;
    m_list->clear();
    for (int i = 0; i < m_nodes.size(); i++)
        m_list->addItem(m_nodes.at(i).name);
    m_loading = false;

    m_row = -1;
    if (selectRow >= 0 && selectRow < m_nodes.size())
        m_list->setCurrentRow(selectRow);   // -> slotNodeRowChanged -> loadNode
    else
        loadNode(-1);
}

void NetworkTopologyDialog::slotNodeRowChanged(int row)
{
    if (m_loading)
        return;
    loadNode(row);
}

void NetworkTopologyDialog::loadNode(int row)
{
    m_loading = true;
    m_row = row;
    const bool have = (row >= 0 && row < m_nodes.size());

    m_name->setEnabled(have);
    m_transport->setEnabled(have);
    m_ip->setEnabled(have);
    m_addPortBtn->setEnabled(have);
    m_removePortBtn->setEnabled(have);
    m_removeNodeBtn->setEnabled(have);

    m_ports->setRowCount(0);
    if (!have)
    {
        m_name->clear();
        m_ip->clear();
        m_loading = false;
        return;
    }

    const NetworkNode &node = m_nodes.at(row);
    m_name->setText(node.name);
    m_transport->setCurrentIndex(m_transport->findData(node.transport));
    m_ip->setText(node.ipHint);

    const bool isFeet = (m_doc->monitorProperties()->gridUnits() == MonitorProperties::Feet);
    const double toDisp = isFeet ? 3.28084 : 1.0;

    m_ports->setRowCount(node.ports.size());
    for (int p = 0; p < node.ports.size(); p++)
    {
        const NetworkPort &port = node.ports.at(p);

        QTableWidgetItem *num = new QTableWidgetItem(QString::number(p + 1));
        num->setFlags(Qt::ItemIsEnabled);
        m_ports->setItem(p, COL_PORT, num);

        m_ports->setCellWidget(p, COL_UNIVERSE, makeUniverseCombo(port.universe));

        QTableWidgetItem *placed = new QTableWidgetItem;
        placed->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        placed->setCheckState(port.receiver.placed ? Qt::Checked : Qt::Unchecked);
        m_ports->setItem(p, COL_RECEIVER, placed);

        for (int col = COL_RX_X; col <= COL_RX_Y; col++)
        {
            QDoubleSpinBox *s = new FeetInchesSpinBox(isFeet, m_ports);
            s->setRange(-500, 500);
            s->setDecimals(2);
            s->setValue((col == COL_RX_X ? port.receiver.posX : port.receiver.posY) * toDisp);
            connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &NetworkTopologyDialog::slotPortCellEdited);
            m_ports->setCellWidget(p, col, s);
        }
    }
    updatePortsEnabled();
    m_loading = false;
    refreshWarnings();
}

QWidget *NetworkTopologyDialog::makeUniverseCombo(quint32 universe)
{
    QComboBox *cb = new QComboBox(m_ports);
    cb->addItem(tr("-- unused --"), InputOutputMap::invalidUniverse());
    InputOutputMap *io = m_doc->inputOutputMap();
    bool found = (universe == InputOutputMap::invalidUniverse());
    for (quint32 i = 0; i < io->universesCount(); i++)
    {
        const quint32 id = io->getUniverseID(i);
        cb->addItem(tr("%1: %2").arg(id + 1).arg(io->getUniverseNameByID(id)), id);
        if (id == universe)
            found = true;
    }
    // A universe the show no longer has must not be silently dropped on OK.
    if (!found)
        cb->addItem(tr("%1: (not in this show)").arg(universe + 1), universe);
    cb->setCurrentIndex(cb->findData(universe));
    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NetworkTopologyDialog::slotPortCellEdited);
    return cb;
}

void NetworkTopologyDialog::updatePortsEnabled()
{
    // QTableWidgetItem::setFlags() emits itemChanged() synchronously, which
    // would re-enter slotPortCellEdited() -> here and recurse without end.
    const bool wasLoading = m_loading;
    m_loading = true;
    const bool wireless = (m_row >= 0 && m_row < m_nodes.size() && m_nodes.at(m_row).isWireless());
    for (int p = 0; p < m_ports->rowCount(); p++)
    {
        QTableWidgetItem *placed = m_ports->item(p, COL_RECEIVER);
        if (placed != NULL)
        {
            Qt::ItemFlags f = Qt::ItemIsEnabled;
            if (wireless)
                f |= Qt::ItemIsUserCheckable;
            placed->setFlags(f);
        }
        const bool placedOn = wireless && placed != NULL && placed->checkState() == Qt::Checked;
        if (QWidget *x = m_ports->cellWidget(p, COL_RX_X)) x->setEnabled(placedOn);
        if (QWidget *y = m_ports->cellWidget(p, COL_RX_Y)) y->setEnabled(placedOn);
    }
    m_loading = wasLoading;
}

void NetworkTopologyDialog::storeNodeFields()
{
    if (m_row < 0 || m_row >= m_nodes.size())
        return;
    NetworkNode &node = m_nodes[m_row];
    node.name = m_name->text().trimmed().isEmpty() ? node.name : m_name->text().trimmed();
    node.transport = m_transport->currentData().toInt();
    node.ipHint = m_ip->text().trimmed();
    if (QListWidgetItem *it = m_list->item(m_row))
        it->setText(node.name);
}

void NetworkTopologyDialog::storePorts()
{
    if (m_row < 0 || m_row >= m_nodes.size())
        return;
    const bool isFeet = (m_doc->monitorProperties()->gridUnits() == MonitorProperties::Feet);
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;

    NetworkNode &node = m_nodes[m_row];
    for (int p = 0; p < node.ports.size() && p < m_ports->rowCount(); p++)
    {
        NetworkPort &port = node.ports[p];
        if (QComboBox *cb = qobject_cast<QComboBox *>(m_ports->cellWidget(p, COL_UNIVERSE)))
            port.universe = cb->currentData().toUInt();
        if (QTableWidgetItem *placed = m_ports->item(p, COL_RECEIVER))
            port.receiver.placed = (placed->checkState() == Qt::Checked);
        if (QDoubleSpinBox *x = qobject_cast<QDoubleSpinBox *>(m_ports->cellWidget(p, COL_RX_X)))
            port.receiver.posX = x->value() * fromDisp;
        if (QDoubleSpinBox *y = qobject_cast<QDoubleSpinBox *>(m_ports->cellWidget(p, COL_RX_Y)))
            port.receiver.posY = y->value() * fromDisp;
    }
}

void NetworkTopologyDialog::slotNodeFieldEdited()
{
    if (m_loading)
        return;
    storeNodeFields();
    updatePortsEnabled();
    refreshWarnings();
}

void NetworkTopologyDialog::slotPortCellEdited()
{
    if (m_loading)
        return;
    storePorts();
    updatePortsEnabled();
    refreshWarnings();
}

void NetworkTopologyDialog::slotAddNode()
{
    NetworkNode node;
    node.name = tr("Node %1").arg(m_nodes.size() + 1);
    node.layerId = m_doc->monitorProperties()->activeLayerId();
    m_nodes.append(node);
    reloadList(m_nodes.size() - 1);
    m_name->setFocus();
    m_name->selectAll();
}

void NetworkTopologyDialog::slotRemoveNode()
{
    if (m_row < 0 || m_row >= m_nodes.size())
        return;
    const int gone = m_row;
    m_nodes.removeAt(gone);
    reloadList(m_nodes.isEmpty() ? -1 : qMin(gone, m_nodes.size() - 1));
    refreshWarnings();
}

void NetworkTopologyDialog::slotAddPort()
{
    if (m_row < 0 || m_row >= m_nodes.size())
        return;
    m_nodes[m_row].ports.append(NetworkPort());
    loadNode(m_row);
}

void NetworkTopologyDialog::slotRemovePort()
{
    if (m_row < 0 || m_row >= m_nodes.size())
        return;
    int p = m_ports->currentRow();
    if (p < 0)
        p = m_nodes.at(m_row).ports.size() - 1;   // no selection: drop the last
    if (p < 0 || p >= m_nodes.at(m_row).ports.size())
        return;
    m_nodes[m_row].ports.removeAt(p);
    loadNode(m_row);
}

void NetworkTopologyDialog::refreshWarnings()
{
    NetworkTopology tmp;
    tmp.nodes() = m_nodes;
    m_warnings->clear();
    const QStringList w = tmp.warnings(m_doc);
    if (w.isEmpty())
        m_warnings->addItem(tr("No problems found."));
    else
        m_warnings->addItems(w);
}

void NetworkTopologyDialog::accept()
{
    storeNodeFields();
    storePorts();
    m_doc->networkTopology()->nodes() = m_nodes;
    m_doc->setModified();
    QDialog::accept();
}

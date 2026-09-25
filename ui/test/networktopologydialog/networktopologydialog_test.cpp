/*
  Q Light Controller Plus - Test Unit
  networktopologydialog_test.cpp

  Copyright (C) Branson Matheson

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

#include <QtTest>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QTableWidget>

#define private public
#include "networktopologydialog.h"
#undef private
#include "networktopology.h"
#include "inputoutputmap.h"
#include "doc.h"

#include "networktopologydialog_test.h"

void NetworkTopologyDialog_Test::init()
{
    m_doc = new Doc(this);
}

void NetworkTopologyDialog_Test::cleanup()
{
    delete m_doc;
}

void NetworkTopologyDialog_Test::emptyShowsNoProblems()
{
    NetworkTopologyDialog dlg(m_doc);
    QCOMPARE(dlg.m_list->count(), 0);
    QVERIFY(!dlg.m_name->isEnabled());          // nothing selected, nothing to edit
    QCOMPARE(dlg.m_warnings->count(), 1);
    QVERIFY(dlg.m_warnings->item(0)->text().contains("No problems"));
}

void NetworkTopologyDialog_Test::addingANodeAndPortsCommitsOnOk()
{
    NetworkTopologyDialog dlg(m_doc);
    dlg.m_addNodeBtn->click();
    QCOMPARE(dlg.m_list->count(), 1);
    QVERIFY(dlg.m_name->isEnabled());

    dlg.m_name->setText("SR ArtNet Node");
    emit dlg.m_name->textEdited("SR ArtNet Node");
    dlg.m_addPortBtn->click();
    dlg.m_addPortBtn->click();
    QCOMPARE(dlg.m_ports->rowCount(), 2);

    QComboBox *uni = qobject_cast<QComboBox *>(dlg.m_ports->cellWidget(1, 1));
    QVERIFY(uni != nullptr);
    // Index 0 is "-- unused --"; the doc has universes, pick the second real one.
    QVERIFY(uni->count() >= 3);
    uni->setCurrentIndex(2);

    dlg.accept();

    QCOMPARE(m_doc->networkTopology()->nodes().size(), 1);
    const NetworkNode &n = m_doc->networkTopology()->nodes().first();
    QCOMPARE(n.name, QString("SR ArtNet Node"));
    QCOMPARE(n.ports.size(), 2);
    QCOMPARE(n.ports[0].universe, InputOutputMap::invalidUniverse());
    QCOMPARE(n.ports[1].universe, m_doc->inputOutputMap()->getUniverseID(1));
    QVERIFY(m_doc->isModified());
}

void NetworkTopologyDialog_Test::cancelLeavesTheDocUntouched()
{
    NetworkTopologyDialog dlg(m_doc);
    dlg.m_addNodeBtn->click();
    dlg.m_addPortBtn->click();
    dlg.reject();
    QCOMPARE(m_doc->networkTopology()->nodes().size(), 0);
}

void NetworkTopologyDialog_Test::duplicateUniverseIsWarnedAbout()
{
    NetworkTopologyDialog dlg(m_doc);
    for (int n = 0; n < 2; n++)
    {
        dlg.m_addNodeBtn->click();
        dlg.m_addPortBtn->click();
        QComboBox *uni = qobject_cast<QComboBox *>(dlg.m_ports->cellWidget(0, 1));
        QVERIFY(uni != nullptr);
        uni->setCurrentIndex(1);                 // same universe on both nodes
    }
    bool found = false;
    for (int i = 0; i < dlg.m_warnings->count(); i++)
        found = found || dlg.m_warnings->item(i)->text().contains("more than one port");
    QVERIFY2(found, "two ports on one universe should raise a warning");
}

void NetworkTopologyDialog_Test::wirelessReceiverFieldsFollowTheTransport()
{
    NetworkTopologyDialog dlg(m_doc);
    dlg.m_addNodeBtn->click();
    dlg.m_addPortBtn->click();

    // Wired: receiver controls stay disabled.
    QVERIFY(!dlg.m_ports->cellWidget(0, 3)->isEnabled());

    dlg.m_transport->setCurrentIndex(dlg.m_transport->findData(int(NetworkNode::WirelessDMX)));
    // Wireless with no receiver placed: warned, coordinates still off.
    bool warned = false;
    for (int i = 0; i < dlg.m_warnings->count(); i++)
        warned = warned || dlg.m_warnings->item(i)->text().contains("no receiver placed");
    QVERIFY(warned);
    QVERIFY(!dlg.m_ports->cellWidget(0, 3)->isEnabled());

    // Place it: coordinates unlock, warning clears, position is committed.
    dlg.m_ports->item(0, 2)->setCheckState(Qt::Checked);
    QVERIFY(dlg.m_ports->cellWidget(0, 3)->isEnabled());
    qobject_cast<QDoubleSpinBox *>(dlg.m_ports->cellWidget(0, 3))->setValue(8.0);
    qobject_cast<QDoubleSpinBox *>(dlg.m_ports->cellWidget(0, 4))->setValue(9.0);
    QCOMPARE(dlg.m_warnings->item(0)->text(), QString("No problems found."));

    dlg.accept();
    const NetworkPort &p = m_doc->networkTopology()->nodes().first().ports.first();
    QVERIFY(m_doc->networkTopology()->nodes().first().isWireless());
    QVERIFY(p.receiver.placed);
    QCOMPARE(p.receiver.posX, 8.0);
    QCOMPARE(p.receiver.posY, 9.0);
}

void NetworkTopologyDialog_Test::removingANodeDropsItFromTheCommit()
{
    NetworkNode a, b;
    a.name = "A";
    b.name = "B";
    m_doc->networkTopology()->nodes() << a << b;

    NetworkTopologyDialog dlg(m_doc, 0);
    QCOMPARE(dlg.m_list->count(), 2);
    dlg.m_removeNodeBtn->click();               // removes the selected node (A)
    dlg.accept();

    QCOMPARE(m_doc->networkTopology()->nodes().size(), 1);
    QCOMPARE(m_doc->networkTopology()->nodes().first().name, QString("B"));
}

void NetworkTopologyDialog_Test::aSavedUniverseIsShownEvenIfTheShowLostIt()
{
    NetworkNode n;
    n.name = "Old Node";
    NetworkPort here, gone;
    here.universe = m_doc->inputOutputMap()->getUniverseID(1);
    gone.universe = 40;                          // the show has no universe 41
    n.ports << here << gone;
    m_doc->networkTopology()->nodes() << n;

    NetworkTopologyDialog dlg(m_doc, 0);
    QComboBox *a = qobject_cast<QComboBox *>(dlg.m_ports->cellWidget(0, 1));
    QComboBox *b = qobject_cast<QComboBox *>(dlg.m_ports->cellWidget(1, 1));
    QVERIFY(a != nullptr && b != nullptr);
    QCOMPARE(a->currentData().toUInt(), here.universe);
    QCOMPARE(b->count(), a->count() + 1);        // one extra entry for the lost universe
    QVERIFY2(b->currentText().contains("41") && b->currentText().contains("not in this show"),
             qPrintable(b->currentText()));

    // OK keeps it rather than silently turning it into "unused".
    dlg.accept();
    QCOMPARE(m_doc->networkTopology()->nodes().first().ports.at(1).universe, quint32(40));
}

void NetworkTopologyDialog_Test::removingAPortDropsTheSelectedOrTheLast()
{
    NetworkNode n;
    n.name = "Three Ports";
    for (quint32 u = 0; u < 3; u++)
    {
        NetworkPort p;
        p.universe = m_doc->inputOutputMap()->getUniverseID(u);
        n.ports << p;
    }
    m_doc->networkTopology()->nodes() << n;
    const quint32 u0 = n.ports.at(0).universe, u2 = n.ports.at(2).universe;

    NetworkTopologyDialog dlg(m_doc, 0);
    QCOMPARE(dlg.m_ports->rowCount(), 3);
    dlg.m_ports->setCurrentCell(1, 0);
    dlg.m_removePortBtn->click();                // the selected (middle) one
    QCOMPARE(dlg.m_nodes.first().ports.size(), 2);
    QCOMPARE(dlg.m_nodes.first().ports.at(1).universe, u2);

    dlg.m_ports->setCurrentCell(-1, -1);
    dlg.m_removePortBtn->click();                // nothing selected: the last one
    QCOMPARE(dlg.m_nodes.first().ports.size(), 1);
    QCOMPARE(dlg.m_nodes.first().ports.at(0).universe, u0);

    dlg.m_ports->setCurrentCell(-1, -1);
    dlg.m_removePortBtn->click();
    dlg.m_removePortBtn->click();                // none left: a no-op, not a crash
    QCOMPARE(dlg.m_nodes.first().ports.size(), 0);
}

void NetworkTopologyDialog_Test::portAndNodeButtonsDoNothingWithNoNodeSelected()
{
    NetworkTopologyDialog dlg(m_doc);
    QCOMPARE(dlg.m_row, -1);
    dlg.slotAddPort();
    dlg.slotRemovePort();
    dlg.slotRemoveNode();
    dlg.storeNodeFields();
    dlg.storePorts();
    QVERIFY(dlg.m_nodes.isEmpty());
    dlg.accept();
    QVERIFY(m_doc->networkTopology()->nodes().isEmpty());
}

QTEST_MAIN(NetworkTopologyDialog_Test)

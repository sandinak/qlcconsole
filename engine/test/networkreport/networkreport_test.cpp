/*
  Q Light Controller Plus - Test Unit
  networkreport_test.cpp

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

#include "networkreport.h"
#include "networktopology.h"
#include "monitorproperties.h"
#include "inputoutputmap.h"
#include "fixture.h"
#include "doc.h"

#include "networkreport_test.h"

/* A patched DMX dimmer; plotted at (xM, yM) metres unless @p onPlot is false. */
static Fixture *dimmer(Doc *doc, const QString &name, quint32 universe, quint32 address,
                       double xM = 0, double yM = 0, bool onPlot = true)
{
    Fixture *f = new Fixture(doc);
    f->setName(name);
    f->setChannels(4);
    f->setUniverse(universe);
    f->setAddress(address);
    if (!doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    if (onPlot)
        doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(xM * 1000, yM * 1000, 0));
    return f;
}

static NetworkNode node(const QString &name, int transport, double x, double y,
                        const QList<quint32> &universes)
{
    NetworkNode n;
    n.name = name;
    n.transport = transport;
    n.placed = true;
    n.posX = x;
    n.posY = y;
    foreach (quint32 u, universes)
    {
        NetworkPort p;
        p.universe = u;
        n.ports << p;
    }
    return n;
}

/* SR node: universe 1 (two plotted, 3 m and 5 m out), universe 2 (one not on
 * the plot), a free port. Beach TX: universe 3 via a receiver 10 m away. */
static void buildRig(Doc *doc)
{
    dimmer(doc, "SR 1", 0, 0, 3, 0);
    dimmer(doc, "SR 2", 0, 4, 5, 0);
    dimmer(doc, "Offstage", 1, 0, 0, 0, false);
    dimmer(doc, "Beach 1", 2, 0, 20, 0);

    NetworkNode sr = node("SR Node", NetworkNode::ArtNet, 0, 0, { 0, 1 });
    sr.ipHint = "172.18.2.10";
    sr.ports << NetworkPort();                        // free
    NetworkNode tx = node("Beach TX", NetworkNode::WirelessDMX, -50, 0, { 2 });
    tx.ports[0].receiver.name = "Rig RX";
    tx.ports[0].receiver.placed = true;
    tx.ports[0].receiver.posX = 10;
    tx.ports[0].receiver.posY = 0;
    doc->networkTopology()->nodes() << sr << tx;
}

void NetworkReport_Test::init()
{
    m_doc = new Doc(this);
}

void NetworkReport_Test::cleanup()
{
    delete m_doc;
}

void NetworkReport_Test::anEmptyShowReportsNothing()
{
    const NetworkReport r = NetworkReport::build(m_doc);
    QCOMPARE(r.nodes.size(), 0);
    QCOMPARE(r.portsTotal(), 0);
    QCOMPARE(r.portsFree(), 0);
    QCOMPARE(r.universesCarried(), 0);
    QCOMPARE(r.longestRun(), -1);
    QVERIFY(r.toText().contains("Nodes: 0"));
    QVERIFY(!r.toText().contains("Longest run"));
    QCOMPARE(r.toCsv().count('\n'), 3);                // two headers and the blank between

    QCOMPARE(NetworkReport::build(nullptr).nodes.size(), 0);
}

void NetworkReport_Test::nodesPortsAndUniversesAreCounted()
{
    buildRig(m_doc);
    const NetworkReport r = NetworkReport::build(m_doc);

    QCOMPARE(r.nodes.size(), 2);
    QCOMPARE(r.portsTotal(), 4);
    QCOMPARE(r.portsUsed(), 3);
    QCOMPARE(r.portsFree(), 1);
    QCOMPARE(r.universesCarried(), 3);

    const NetworkReport::Node &sr = r.nodes.at(0);
    QCOMPARE(sr.transport, QString("ArtNet"));
    QCOMPARE(sr.ip, QString("172.18.2.10"));
    QCOMPARE(sr.ports, 3);
    QCOMPARE(sr.portsUsed, 2);
    QCOMPARE(sr.universes, QList<quint32>({ 0, 1 }));
    QCOMPARE(sr.fixtures, 3);
    QCOMPARE(sr.channels, 12);
    QCOMPARE(r.nodes.at(1).transport, QString("Wireless DMX"));

    const NetworkReport::Port &u1 = r.ports.at(0);
    QVERIFY(u1.used && u1.anchored);
    QCOMPARE(u1.fixtures, 2);
    QCOMPARE(u1.channels, 8);
    QVERIFY2(qAbs(u1.lengthMetres - 5.0) < 0.01, qPrintable(QString::number(u1.lengthMetres)));
    QCOMPARE(r.ports.at(1).unplotted, 1);
    QVERIFY(!r.ports.at(2).used);
    QCOMPARE(r.ports.at(3).receiver, QString("Rig RX"));
}

void NetworkReport_Test::theLongestRunIsNamed()
{
    buildRig(m_doc);
    const NetworkReport r = NetworkReport::build(m_doc);
    // The beach run starts at the receiver 10 m out, not the transmitter at -50.
    QCOMPARE(r.longestRun(), 3);
    QVERIFY2(qAbs(r.ports.at(3).lengthMetres - 10.0) < 0.01, qPrintable(QString::number(r.ports.at(3).lengthMetres)));
    QVERIFY2(r.toText().contains("Longest run: Beach TX, port 1 (universe 3), ~10.0 m"), qPrintable(r.toText()));
}

void NetworkReport_Test::networkFixturesAreListedByIp()
{
    Fixture *b = dimmer(m_doc, "Fence B", 5, 0);
    b->setConnection(Fixture::Network);
    b->setIPAddress("172.18.2.42");
    Fixture *a = dimmer(m_doc, "Fence A", 4, 0);
    a->setConnection(Fixture::Network);
    a->setIPAddress("172.18.2.41");
    Fixture *staged = new Fixture(m_doc);
    staged->setName("Step 1");
    staged->setChannels(3);
    staged->setPatched(false);
    staged->setConnection(Fixture::Network);
    QVERIFY(m_doc->addFixture(staged));
    dimmer(m_doc, "Plain DMX", 0, 0);

    const NetworkReport r = NetworkReport::build(m_doc);
    QCOMPARE(r.networkFixtures.size(), 3);
    QCOMPARE(r.networkFixtures.at(0).name, QString("Step 1"));    // no IP sorts first
    QVERIFY(!r.networkFixtures.at(0).patched);
    QCOMPARE(r.networkFixtures.at(1).name, QString("Fence A"));
    QCOMPARE(r.networkFixtures.at(1).universe, quint32(4));
    QCOMPARE(r.networkFixtures.at(2).ip, QString("172.18.2.42"));

    const QString text = r.toText();
    QVERIFY2(text.contains("172.18.2.41: Fence A, universe 5"), qPrintable(text));
    QVERIFY2(text.contains("(no IP): Step 1, not patched"), qPrintable(text));
}

void NetworkReport_Test::unservedFixturesAndWarningsAreCarriedOver()
{
    buildRig(m_doc);
    dimmer(m_doc, "Nobody's", 7, 0);
    NetworkNode dup = node("Dup", NetworkNode::ArtNet, 1, 1, { 0 });
    m_doc->networkTopology()->nodes() << dup;

    const NetworkReport r = NetworkReport::build(m_doc);
    QCOMPARE(r.unservedNames, QStringList({ "Nobody's" }));
    QVERIFY(!r.warnings.isEmpty());
    const QString text = r.toText();
    QVERIFY(text.contains("UNSERVED"));
    QVERIFY(text.contains("  Nobody's"));
    QVERIFY(text.contains("WARNINGS"));
    QVERIFY(text.contains("more than one port"));
}

void NetworkReport_Test::theTextReadsLikeTheDataPackage()
{
    buildRig(m_doc);
    NetworkNode floating = node("Spare", NetworkNode::sACN, 0, 0, {});
    floating.placed = false;
    m_doc->networkTopology()->nodes() << floating;

    const QString text = NetworkReport::build(m_doc).toText();
    QVERIFY2(text.startsWith("NETWORK SUMMARY\nNodes: 3   Ports: 4 (3 used, 1 free)   Universes carried: 3"),
             qPrintable(text));
    QVERIFY2(text.contains("SR Node -- ArtNet @ 172.18.2.10\n  3 port(s), 2 used; 3 fixture(s), 12 channel(s)"),
             qPrintable(text));
    QVERIFY(text.contains("  Port 1: universe 1, 2 fixture(s), 8/512 ch, run ~5.0 m"));
    QVERIFY(text.contains("  Port 2: universe 2, 1 fixture(s), 4/512 ch (1 not on the plot)"));
    QVERIFY(text.contains("  Port 3: free"));
    QVERIFY(text.contains("  Port 1: universe 3, 1 fixture(s), 4/512 ch, via Rig RX, run ~10.0 m"));
    QVERIFY(text.contains("Spare -- sACN (not on the plot)"));
    QVERIFY(!text.contains("NETWORK FIXTURES"));      // none in this rig
}

void NetworkReport_Test::lengthsFollowThePlotUnits()
{
    buildRig(m_doc);
    const NetworkReport r = NetworkReport::build(m_doc);
    QVERIFY(r.toText(true).contains("run ~16.4 ft"));
    QVERIFY(r.toCsv(true).startsWith("Node,Transport,IP,Port,Universe,Receiver,Fixtures,Channels,Run (ft),"));
    QVERIFY(r.toCsv(true).contains(",16.4,"));
    QVERIFY(r.toCsv(false).contains(",5.0,"));
}

void NetworkReport_Test::theCsvIsSpreadsheetSafe()
{
    buildRig(m_doc);
    m_doc->networkTopology()->nodes()[0].name = "SR, \"upstage\"";
    Fixture *f = dimmer(m_doc, "Fence, west", 4, 0);
    f->setConnection(Fixture::Network);
    f->setIPAddress("172.18.2.41");

    const QStringList lines = NetworkReport::build(m_doc).toCsv().split('\n');
    QCOMPARE(lines.at(1), QString("\"SR, \"\"upstage\"\"\",ArtNet,172.18.2.10,1,1,,2,8,5.0,0"));
    // Its only fixture is off the plot: nothing to measure.
    QCOMPARE(lines.at(2), QString("\"SR, \"\"upstage\"\"\",ArtNet,172.18.2.10,2,2,,1,4,,1"));
    QCOMPARE(lines.at(3), QString("\"SR, \"\"upstage\"\"\",ArtNet,172.18.2.10,3,,,0,0,,0"));   // free port
    QCOMPARE(lines.at(4), QString("Beach TX,Wireless DMX,,1,3,Rig RX,1,4,10.0,0"));
    QCOMPARE(lines.at(5), QString());
    QCOMPARE(lines.at(6), QString("Network fixture,IP,Universe"));
    QCOMPARE(lines.at(7), QString("\"Fence, west\",172.18.2.41,5"));
}

QTEST_GUILESS_MAIN(NetworkReport_Test)

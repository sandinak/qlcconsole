/*
  Q Light Controller Plus - Test Unit
  networktopology_test.cpp

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
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "fakenetplugin.h"
#include "networktopology.h"
#include "networkgearcatalog.h"
#include "networkreport.h"
#include "fixture.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "networktopology_test.h"

void NetworkTopology_Test::defaults()
{
    NetworkReceiver rcv;
    QCOMPARE(rcv.placed, false);

    NetworkPort port;
    QCOMPARE(port.universe, InputOutputMap::invalidUniverse());
    QCOMPARE(port.receiver.placed, false);

    NetworkNode node;
    QCOMPARE(node.name, QString("Node"));
    QCOMPARE(node.transport, int(NetworkNode::ArtNet));
    QCOMPARE(node.placed, false);
    QCOMPARE(node.isWireless(), false);

    NetworkTopology topo;
    QCOMPARE(topo.nodes().count(), 0);
}

void NetworkTopology_Test::nodesAndPorts()
{
    NetworkTopology topo;

    NetworkNode node;
    node.name = "House ArtNet Node";
    NetworkPort p1;
    p1.universe = 0;
    NetworkPort p2;
    p2.universe = 1;
    node.ports << p1 << p2;
    topo.nodes().append(node);

    QCOMPARE(topo.nodes().count(), 1);
    QCOMPARE(topo.nodes()[0].ports.count(), 2);

    topo.reset();
    QCOMPARE(topo.nodes().count(), 0);
}

void NetworkTopology_Test::portForUniverse()
{
    NetworkTopology topo;

    NetworkNode n1;
    n1.ports << NetworkPort();
    n1.ports[0].universe = 5;
    topo.nodes().append(n1);

    NetworkNode n2;
    NetworkPort p;
    p.universe = 7;
    n2.ports << NetworkPort() << p;
    topo.nodes().append(n2);

    int nodeIdx = -1, portIdx = -1;
    topo.portForUniverse(7, nodeIdx, portIdx);
    QCOMPARE(nodeIdx, 1);
    QCOMPARE(portIdx, 1);

    topo.portForUniverse(999, nodeIdx, portIdx);
    QCOMPARE(nodeIdx, -1);
    QCOMPARE(portIdx, -1);
}

void NetworkTopology_Test::proximityAnchorWired()
{
    // A wired node anchors to its own position.
    NetworkNode node;
    node.placed = true;
    node.posX = 10.0;
    node.posY = 20.0;

    NetworkPort port;

    double x = 0, y = 0;
    QVERIFY(NetworkTopology::proximityAnchor(node, port, x, y));
    QCOMPARE(x, 10.0);
    QCOMPARE(y, 20.0);
}

void NetworkTopology_Test::proximityAnchorWireless()
{
    // A wireless node with a placed receiver anchors to the RECEIVER, not
    // the transmitter -- the whole point of the split (transmitter can sit
    // anywhere, e.g. far from the fixtures its universe actually serves).
    NetworkNode node;
    node.transport = NetworkNode::WirelessDMX;
    node.placed = true;
    node.posX = 1000.0;   // the transmitter, deliberately far away
    node.posY = 1000.0;

    NetworkPort port;
    port.receiver.placed = true;
    port.receiver.posX = 5.0;
    port.receiver.posY = 6.0;

    double x = 0, y = 0;
    QVERIFY(NetworkTopology::proximityAnchor(node, port, x, y));
    QCOMPARE(x, 5.0);
    QCOMPARE(y, 6.0);
}

void NetworkTopology_Test::proximityAnchorUnplaced()
{
    // A wireless node whose receiver hasn't been placed yet falls back to the
    // node's own (transmitter) position, same as a wired node.
    NetworkNode node;
    node.transport = NetworkNode::WirelessDMX;
    node.placed = true;
    node.posX = 3.0;
    node.posY = 4.0;

    NetworkPort port; // receiver not placed

    double x = 0, y = 0;
    QVERIFY(NetworkTopology::proximityAnchor(node, port, x, y));
    QCOMPARE(x, 3.0);
    QCOMPARE(y, 4.0);

    // Nothing placed at all -> no anchor.
    NetworkNode unplaced;
    QVERIFY(!NetworkTopology::proximityAnchor(unplaced, port, x, y));
}

void NetworkTopology_Test::transportMatchesPlugin()
{
    QVERIFY(NetworkTopology::transportMatchesPlugin(NetworkNode::ArtNet, "ArtNet"));
    QVERIFY(!NetworkTopology::transportMatchesPlugin(NetworkNode::ArtNet, "E1.31"));
    QVERIFY(NetworkTopology::transportMatchesPlugin(NetworkNode::sACN, "E1.31"));
    QVERIFY(!NetworkTopology::transportMatchesPlugin(NetworkNode::sACN, "ArtNet"));
    // Interface/wireless can sit behind any plugin; unpatched is never a mismatch.
    QVERIFY(NetworkTopology::transportMatchesPlugin(NetworkNode::Interface, "DMX USB"));
    QVERIFY(NetworkTopology::transportMatchesPlugin(NetworkNode::WirelessDMX, "ArtNet"));
    QVERIFY(NetworkTopology::transportMatchesPlugin(NetworkNode::ArtNet, QString()));
}

void NetworkTopology_Test::warningsDuplicateUniverse()
{
    NetworkTopology topo;
    NetworkNode a, b;
    a.name = "A";
    b.name = "B";
    NetworkPort p;
    p.universe = 3;
    a.ports << p;
    b.ports << p;
    topo.nodes() << a << b;

    const QStringList w = topo.warnings(nullptr);
    QCOMPARE(w.size(), 1);
    QVERIFY2(w.first().contains("Universe 4") && w.first().contains("A, port 1")
             && w.first().contains("B, port 1"), qPrintable(w.first()));
}

void NetworkTopology_Test::warningsWirelessWithoutReceiver()
{
    NetworkTopology topo;
    NetworkNode tx;
    tx.name = "Beach TX";
    tx.transport = NetworkNode::WirelessDMX;
    NetworkPort p;
    p.universe = 1;
    tx.ports << p;
    topo.nodes() << tx;

    QStringList w = topo.warnings(nullptr);
    QCOMPARE(w.size(), 1);
    QVERIFY2(w.first().contains("no receiver placed"), qPrintable(w.first()));

    topo.nodes()[0].ports[0].receiver.placed = true;
    QVERIFY(topo.warnings(nullptr).isEmpty());
}

void NetworkTopology_Test::warningsCleanTopologyIsEmpty()
{
    NetworkTopology topo;
    QVERIFY(topo.warnings(nullptr).isEmpty());

    NetworkNode n;
    NetworkPort p1, p2;
    p1.universe = 0;
    p2.universe = 1;
    n.ports << p1 << p2;
    topo.nodes() << n;
    QVERIFY(topo.warnings(nullptr).isEmpty());
}

void NetworkTopology_Test::xmlRoundTrip()
{
    NetworkTopology topo;

    NetworkNode conv;
    conv.name = "SR ArtNet Node";
    conv.transport = NetworkNode::ArtNet;
    conv.placed = true;
    conv.posX = 12.5;
    conv.posY = -3.0;
    NetworkPort cp1;
    cp1.universe = 0;
    NetworkPort cp2;
    cp2.universe = 1;
    conv.ports << cp1 << cp2;
    topo.nodes().append(conv);

    NetworkNode wireless;
    wireless.name = "Beach TX";
    wireless.transport = NetworkNode::WirelessDMX;
    wireless.placed = true;
    wireless.posX = -100.0;
    wireless.posY = -50.0;
    NetworkPort wp;
    wp.universe = 9;
    wp.receiver.name = "Rig RX A";
    wp.receiver.placed = true;
    wp.receiver.posX = 8.0;
    wp.receiver.posY = 9.0;
    wireless.ports << wp;
    topo.nodes().append(wireless);

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    QVERIFY(topo.saveXML(&writer));
    writer.writeEndDocument();

    NetworkTopology topo2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
    {
        if (reader.name() == QStringLiteral("NetworkTopology"))
            QVERIFY(topo2.loadXML(reader, nullptr));
        else
            reader.skipCurrentElement();
    }

    QCOMPARE(topo2.nodes().count(), 2);

    QCOMPARE(topo2.nodes()[0].name, QString("SR ArtNet Node"));
    QCOMPARE(topo2.nodes()[0].transport, int(NetworkNode::ArtNet));
    QCOMPARE(topo2.nodes()[0].placed, true);
    QCOMPARE(topo2.nodes()[0].posX, 12.5);
    QCOMPARE(topo2.nodes()[0].ports.count(), 2);
    QCOMPARE(topo2.nodes()[0].ports[1].universe, quint32(1));

    QCOMPARE(topo2.nodes()[1].name, QString("Beach TX"));
    QCOMPARE(topo2.nodes()[1].isWireless(), true);
    QCOMPARE(topo2.nodes()[1].posX, -100.0);
    QVERIFY(topo2.nodes()[1].ports[0].receiver.placed);
    QCOMPARE(topo2.nodes()[1].ports[0].receiver.name, QString("Rig RX A"));
    QCOMPARE(topo2.nodes()[1].ports[0].receiver.posX, 8.0);
    QCOMPARE(topo2.nodes()[1].ports[0].receiver.posY, 9.0);

    // An empty model writes nothing (no <NetworkTopology> element at all) and
    // a fresh load of that is simply a no-op, not an error state.
    NetworkTopology empty;
    QByteArray buf2;
    QXmlStreamWriter w2(&buf2);
    w2.writeStartDocument();
    QVERIFY(empty.saveXML(&w2));
    w2.writeEndDocument();
    QVERIFY(!buf2.contains("NetworkTopology"));
}



void NetworkTopology_Test::init()
{
    m_doc = new Doc(this);
}

void NetworkTopology_Test::cleanup()
{
    delete m_doc;
}

static Fixture *dimmer(Doc *doc, quint32 universe, quint32 address, int channels,
                       const char *name, bool patched = true,
                       Fixture::Connection c = Fixture::Dmx)
{
    Fixture *f = new Fixture(doc);
    f->setName(name);
    f->setChannels(channels);
    f->setUniverse(universe);
    f->setAddress(address);
    f->setPatched(patched);
    f->setConnection(c);
    if (!doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    return f;
}

static NetworkTopology &operator<<(NetworkTopology &t, const NetworkNode &n)
{
    t.nodes().append(n);
    return t;
}

static NetworkNode nodeAt(const char *name, double x, double y, quint32 universe)
{
    NetworkNode n;
    n.name = name;
    n.placed = true;
    n.posX = x;
    n.posY = y;
    NetworkPort p;
    p.universe = universe;
    n.ports << p;
    return n;
}

void NetworkTopology_Test::runsFollowAddressOrderFromTheNode()
{
    // Added out of address order: the run must chain them by address.
    Fixture *c = dimmer(m_doc, 0, 20, 4, "C");
    Fixture *a = dimmer(m_doc, 0, 0, 4, "A");
    Fixture *b = dimmer(m_doc, 0, 10, 4, "B");

    NetworkTopology topo;
    topo.nodes() << nodeAt("SR Node", 0.0, 0.0, 0);

    QHash<quint32, QVector3D> pos;
    pos.insert(a->id(), QVector3D(3, 0, 0));
    pos.insert(b->id(), QVector3D(3, 4, 0));
    pos.insert(c->id(), QVector3D(0, 4, 0));

    const QList<NetworkRun> runs = topo.runs(m_doc, pos);
    QCOMPARE(runs.size(), 1);
    const NetworkRun &r = runs.first();
    QCOMPARE(r.fixtures, (QList<quint32>{ a->id(), b->id(), c->id() }));
    QCOMPARE(r.plotted, r.fixtures);
    QCOMPARE(r.channels, 12);
    QVERIFY(r.anchored);
    // node (0,0) -> A (3,0) -> B (3,4) -> C (0,4): 3 + 4 + 3
    QCOMPARE(r.points.size(), 4);
    QVERIFY2(qAbs(r.lengthMetres - 10.0) < 1e-4, qPrintable(QString::number(r.lengthMetres)));
    QCOMPARE(r.nodeName, QString("SR Node"));
    QCOMPARE(r.universe, quint32(0));
}

void NetworkTopology_Test::aWirelessRunStartsAtTheReceiverNotTheTransmitter()
{
    Fixture *a = dimmer(m_doc, 2, 0, 4, "A");

    NetworkNode tx = nodeAt("Beach TX", -100.0, -100.0, 2);          // far away
    tx.transport = NetworkNode::WirelessDMX;
    tx.ports[0].receiver.name = "Rig RX";
    tx.ports[0].receiver.placed = true;
    tx.ports[0].receiver.posX = 8.0;
    tx.ports[0].receiver.posY = 6.0;
    NetworkTopology topo;
    topo.nodes() << tx;

    QHash<quint32, QVector3D> pos;
    pos.insert(a->id(), QVector3D(8, 9, 0));
    const NetworkRun r = topo.runs(m_doc, pos).first();

    QVERIFY(r.wireless);
    QCOMPARE(r.receiverName, QString("Rig RX"));
    QCOMPARE(r.points.first(), QVector3D(8, 6, 0));                  // the receiver
    QVERIFY2(qAbs(r.lengthMetres - 3.0) < 1e-4,
             "length must be measured from the receiver, not the far-away transmitter");
}

void NetworkTopology_Test::networkAndStagedFixturesAreNotOnARun()
{
    Fixture *dmx = dimmer(m_doc, 0, 0, 4, "DMX");
    Fixture *net = dimmer(m_doc, 0, 100, 4, "Fence", true, Fixture::Network);
    Fixture *staged = dimmer(m_doc, 0, 0, 4, "Staged", false);
    QVERIFY(dmx && net && staged);

    NetworkTopology topo;
    topo.nodes() << nodeAt("Node", 0, 0, 0);
    const NetworkRun r = topo.runs(m_doc, QHash<quint32, QVector3D>()).first();
    QCOMPARE(r.fixtures, QList<quint32>{ dmx->id() });
    QCOMPARE(r.channels, 4);
}

void NetworkTopology_Test::unplottedFixturesCountButHaveNoVertex()
{
    Fixture *a = dimmer(m_doc, 0, 0, 4, "On the plot");
    Fixture *b = dimmer(m_doc, 0, 10, 8, "Not on the plot");

    NetworkTopology topo;
    topo.nodes() << nodeAt("Node", 0, 0, 0);
    QHash<quint32, QVector3D> pos;
    pos.insert(a->id(), QVector3D(2, 0, 0));
    const NetworkRun r = topo.runs(m_doc, pos).first();

    QCOMPARE(r.fixtures.size(), 2);                                   // both are on the run...
    QCOMPARE(r.channels, 12);                                         // ...and count toward its load
    QCOMPARE(r.plotted, QList<quint32>{ a->id() });                   // only one can be drawn
    QCOMPARE(r.points.size(), 2);
    QVERIFY2(qAbs(r.lengthMetres - 2.0) < 1e-4, qPrintable(QString::number(r.lengthMetres)));
    Q_UNUSED(b)
}

void NetworkTopology_Test::anUnanchoredNodeClaimsNoPolylineOrLength()
{
    Fixture *a = dimmer(m_doc, 0, 0, 4, "A");
    NetworkNode n = nodeAt("Unplaced", 0, 0, 0);
    n.placed = false;                                                 // not on the plot yet
    NetworkTopology topo;
    topo.nodes() << n;
    QHash<quint32, QVector3D> pos;
    pos.insert(a->id(), QVector3D(5, 5, 0));

    const NetworkRun r = topo.runs(m_doc, pos).first();
    QVERIFY(!r.anchored);
    QVERIFY(r.points.isEmpty());
    QCOMPARE(r.lengthMetres, 0.0);
    QCOMPARE(r.fixtures.size(), 1);                                   // still listed
}

void NetworkTopology_Test::unservedFixturesAreThoseOnNoPort()
{
    Fixture *served = dimmer(m_doc, 0, 0, 4, "Served");
    Fixture *orphan = dimmer(m_doc, 5, 0, 4, "Orphan");
    dimmer(m_doc, 5, 100, 4, "Network one", true, Fixture::Network);   // needs no port
    dimmer(m_doc, 5, 200, 4, "Staged", false);                          // has no address
    QVERIFY(served && orphan);

    NetworkTopology topo;
    topo.nodes() << nodeAt("Node", 0, 0, 0);
    QCOMPARE(topo.unservedFixtures(m_doc), QList<quint32>{ orphan->id() });
}

void NetworkTopology_Test::aNodeWhoseTransportDisagreesWithItsUniversesOutputIsFlagged()
{
    (new FakeNetPlugin("E1.31"))->registerIn(m_doc);
    (new FakeNetPlugin("ArtNet"))->registerIn(m_doc);
    m_doc->inputOutputMap()->setOutputPatch(0, "E1.31", "eth0", 0);      // universe 1 is sACN
    m_doc->inputOutputMap()->setOutputPatch(1, "ArtNet", "eth0", 0);     // universe 2 is ArtNet

    NetworkTopology topo;
    NetworkNode wrong = nodeAt("Says ArtNet", 0, 0, 0);                  // but universe 1 is sACN
    NetworkNode wrongToo = nodeAt("Says sACN", 0, 0, 1);
    wrongToo.transport = NetworkNode::sACN;                              // but universe 2 is ArtNet
    topo << wrong << wrongToo;

    const QStringList w = topo.warnings(m_doc);
    QCOMPARE(w.size(), 2);
    QVERIFY2(w[0].contains("Says ArtNet") && w[0].contains("ArtNet") && w[0].contains("E1.31")
             && w[0].contains("universe 1"), qPrintable(w[0]));
    QVERIFY2(w[1].contains("sACN") && w[1].contains("ArtNet") && w[1].contains("universe 2"), qPrintable(w[1]));
}

void NetworkTopology_Test::matchingOrOpenTransportsAreNotFlagged()
{
    (new FakeNetPlugin("ArtNet"))->registerIn(m_doc);
    (new FakeNetPlugin("DMX USB"))->registerIn(m_doc);
    m_doc->inputOutputMap()->setOutputPatch(0, "ArtNet", "eth0", 0);
    m_doc->inputOutputMap()->setOutputPatch(1, "DMX USB", "eth0", 0);
    m_doc->inputOutputMap()->setOutputPatch(2, "ArtNet", "eth1", 0);

    NetworkTopology topo;
    topo << nodeAt("ArtNet on ArtNet", 0, 0, 0);
    NetworkNode usb = nodeAt("Interface on USB", 0, 0, 1);
    usb.transport = NetworkNode::Interface;
    NetworkNode wl = nodeAt("Wireless on ArtNet", 0, 0, 2);
    wl.transport = NetworkNode::WirelessDMX;
    wl.ports[0].receiver.placed = true;
    NetworkNode unpatched = nodeAt("Universe with no output", 0, 0, 3);
    topo << usb << wl << unpatched;

    QVERIFY2(topo.warnings(m_doc).isEmpty(), qPrintable(topo.warnings(m_doc).join("; ")));
}

void NetworkTopology_Test::everyNodeFieldSurvivesAnXmlRoundTrip()
{
    NetworkNode n = nodeAt("Everything", 1.5, -2.5, 4);
    n.transport = NetworkNode::sACN;
    n.ipHint = "172.18.2.31";
    n.locked = true;
    n.layerId = 7;
    n.groupId = 9;
    NetworkTopology topo;
    topo << n;

    QByteArray buf;
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    QVERIFY(topo.saveXML(&w));
    w.writeEndDocument();

    NetworkTopology back;
    QXmlStreamReader r(buf);
    while (r.readNextStartElement())
        if (r.name() == QStringLiteral("NetworkTopology"))
            QVERIFY(back.loadXML(r, m_doc));
        else
            r.skipCurrentElement();

    QCOMPARE(back.nodes().size(), 1);
    const NetworkNode &b = back.nodes().first();
    QCOMPARE(b.name, QString("Everything"));
    QCOMPARE(b.transport, int(NetworkNode::sACN));
    QCOMPARE(b.ipHint, QString("172.18.2.31"));
    QVERIFY(b.locked);
    QCOMPARE(b.layerId, quint32(7));
    QCOMPARE(b.groupId, quint32(9));
    QVERIFY(b.placed);
    QCOMPARE(b.posX, 1.5);
    QCOMPARE(b.posY, -2.5);
    QCOMPARE(b.ports.first().universe, quint32(4));
}

void NetworkTopology_Test::aWrongRootIsRejected()
{
    QXmlStreamReader r(QByteArray("<PowerDistribution><Node/></PowerDistribution>"));
    r.readNextStartElement();
    NetworkTopology topo;
    QVERIFY(!topo.loadXML(r, m_doc));
    QCOMPARE(topo.nodes().size(), 0);
}

void NetworkTopology_Test::unknownXmlIsSkippedNotFatal()
{
    // A newer file may carry tags this version has never heard of: skip them,
    // keep what is understood.
    const QByteArray xml(
        "<NetworkTopology>"
        " <Mystery Foo='1'><Inner/></Mystery>"
        " <Node Transport='1'>"
        "  <Name>Keeper</Name>"
        "  <Extra>ignored</Extra>"
        "  <Port Universe='2'>"
        "   <Receiver PosX='3' PosY='4'><Name>RX</Name><Extra/></Receiver>"
        "   <Unknown/>"
        "  </Port>"
        " </Node>"
        "</NetworkTopology>");
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    NetworkTopology topo;
    QVERIFY(topo.loadXML(r, m_doc));
    QCOMPARE(topo.nodes().size(), 1);
    QCOMPARE(topo.nodes().first().name, QString("Keeper"));
    QCOMPARE(topo.nodes().first().ports.first().universe, quint32(2));
    QCOMPARE(topo.nodes().first().ports.first().receiver.name, QString("RX"));
    QCOMPARE(topo.nodes().first().ports.first().receiver.posX, 3.0);
}

void NetworkTopology_Test::clearContentsResetsTheTopologyAndTheDocPersistsIt()
{
    m_doc->networkTopology()->nodes() << nodeAt("On the doc", 0, 0, 0);

    // Saved with the workspace...
    QByteArray buf;
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    QVERIFY(m_doc->saveXML(&w));
    w.writeEndDocument();
    QVERIFY(buf.contains("NetworkTopology"));
    QVERIFY(buf.contains("On the doc"));

    // ...loaded back with it...
    Doc other(this);
    QXmlStreamReader r(buf);
    while (r.readNextStartElement() && r.name() != QStringLiteral("Engine"))
        ;
    QVERIFY(other.loadXML(r));
    QCOMPARE(other.networkTopology()->nodes().size(), 1);
    QCOMPARE(other.networkTopology()->nodes().first().name, QString("On the doc"));

    // ...and cleared with it.
    m_doc->clearContents();
    QCOMPARE(m_doc->networkTopology()->nodes().size(), 0);
}

void NetworkTopology_Test::unusedPortsAndANullDocYieldNoRuns()
{
    NetworkTopology topo;
    NetworkNode n = nodeAt("Half Used", 0, 0, 0);
    n.ports << NetworkPort();                          // a spare port, no universe
    topo << n;

    const QList<NetworkRun> runs = topo.runs(m_doc, QHash<quint32, QVector3D>());
    QCOMPARE(runs.size(), 1);                          // the spare port is not a run
    QCOMPARE(runs.first().portIndex, 0);

    QVERIFY(topo.runs(nullptr, QHash<quint32, QVector3D>()).isEmpty());
    QVERIFY(topo.unservedFixtures(nullptr).isEmpty());
}

void NetworkTopology_Test::theGearCatalogListsBuiltInAndYourModels()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    NetworkGearCatalog::setUserDirectoryOverride(dir.filePath("NetworkGear"));

    // Built in: sorted by manufacturer then model, every one with ports.
    const QList<NetworkGearModel> built = NetworkGearCatalog::builtIn();
    QVERIFY(built.size() >= 10);
    for (int i = 0; i < built.size(); ++i)
    {
        QVERIFY(built.at(i).ports >= 1);
        QVERIFY(built.at(i).builtIn);
        if (i > 0)
            QVERIFY(built.at(i - 1).manufacturer.compare(built.at(i).manufacturer, Qt::CaseInsensitive) <= 0);
    }
    bool ode = false;
    for (const NetworkGearModel &m : built)
        if (m.displayName() == "ENTTEC ODE Mk3")
        {
            ode = true;
            QCOMPARE(m.ports, 2);
            QCOMPARE(m.transport, int(NetworkNode::ArtNet));
        }
    QVERIFY(ode);

    // Yours: one model per file, or several; a broken one is skipped.
    QVERIFY(NetworkGearCatalog::userModels().isEmpty());
    auto write = [&](const QString &name, const QByteArray &json) {
        QFile f(NetworkGearCatalog::userDirectory().absoluteFilePath(name));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(json);
    };
    write("one.json", R"({"manufacturer":"Acme","model":"Rack 3","transport":"sACN","ports":3})");
    write("two.json", R"([{"manufacturer":"Acme","model":"Box","transport":"Wireless DMX","ports":2},
                          {"manufacturer":"ENTTEC","model":"ODE Mk3","transport":"ArtNet","ports":4},
                          {"manufacturer":"Acme","model":"","transport":"ArtNet","ports":1},
                          {"manufacturer":"Acme","model":"Bad","transport":"Carrier Pigeon","ports":1},
                          {"manufacturer":"Acme","model":"None","transport":"ArtNet","ports":0}])");
    write("broken.json", "{ not json");
    const QList<NetworkGearModel> mine = NetworkGearCatalog::userModels();
    QCOMPARE(mine.size(), 3);
    QCOMPARE(mine.at(0).displayName(), QString("Acme Box"));
    QCOMPARE(mine.at(1).displayName(), QString("Acme Rack 3"));
    QCOMPARE(mine.at(2).displayName(), QString("ENTTEC ODE Mk3"));
    QVERIFY(!mine.at(0).builtIn);
    QCOMPARE(mine.at(0).transport, int(NetworkNode::WirelessDMX));

    // Everything: yours first, and your ODE Mk3 (4 ports) replaces the built-in.
    const QList<NetworkGearModel> all = NetworkGearCatalog::all();
    QCOMPARE(all.first().displayName(), QString("Acme Box"));
    int odes = 0;
    for (const NetworkGearModel &m : all)
        if (m.displayName() == "ENTTEC ODE Mk3")
        {
            ++odes;
            QCOMPARE(m.ports, 4);
            QVERIFY(!m.builtIn);
        }
    QCOMPARE(odes, 1);
    QCOMPARE(all.size(), built.size() + 2);

    // Saving one: a file named after it, read back as yours.
    NetworkGearModel saved;
    saved.manufacturer = "Acme";
    saved.model = "Truss/Node 6";
    saved.transport = NetworkNode::Interface;
    saved.ports = 6;
    const QString path = NetworkGearCatalog::saveUserModel(saved);
    QVERIFY(!path.isEmpty());
    QVERIFY(QFileInfo(path).fileName() == "Acme Truss_Node 6.json");
    bool found = false;
    for (const NetworkGearModel &m : NetworkGearCatalog::userModels())
        if (m.displayName() == "Acme Truss/Node 6")
        {
            found = true;
            QCOMPARE(m.ports, 6);
            QCOMPARE(m.transport, int(NetworkNode::Interface));
        }
    QVERIFY(found);
    NetworkGearModel nameless;
    QVERIFY(NetworkGearCatalog::saveUserModel(nameless).isEmpty());

    // Transport names both ways.
    QCOMPARE(NetworkGearCatalog::transportFromName("Art-Net"), int(NetworkNode::ArtNet));
    QCOMPARE(NetworkGearCatalog::transportFromName("E1.31"), int(NetworkNode::sACN));
    QCOMPARE(NetworkGearCatalog::transportFromName("usb"), int(NetworkNode::Interface));
    QCOMPARE(NetworkGearCatalog::transportFromName("Wireless"), int(NetworkNode::WirelessDMX));
    QCOMPARE(NetworkGearCatalog::transportFromName("smoke"), -1);
    for (int t = NetworkNode::ArtNet; t <= NetworkNode::WirelessDMX; ++t)
        QCOMPARE(NetworkGearCatalog::transportFromName(NetworkGearCatalog::transportToName(t)), t);

    NetworkGearCatalog::setUserDirectoryOverride(QString());
}

void NetworkTopology_Test::aNodeMadeFromAModelHasItsPortsAndKeepsItsModel()
{
    NetworkGearModel storm;
    storm.manufacturer = "ENTTEC";
    storm.model = "Storm 8";
    storm.transport = NetworkNode::ArtNet;
    storm.ports = 8;
    const NetworkNode n = NetworkGearCatalog::makeNode(storm);
    QCOMPARE(n.name, QString("Storm 8"));
    QCOMPARE(n.model, QString("ENTTEC Storm 8"));
    QCOMPARE(n.ports.size(), 8);
    for (const NetworkPort &p : n.ports)
    {
        QCOMPARE(p.universe, InputOutputMap::invalidUniverse());
        QCOMPARE(p.receiver.name, NetworkReceiver().name);   // wired: left as is
        QVERIFY(!p.receiver.placed);
    }
    QCOMPARE(NetworkGearCatalog::makeNode(storm, "  SR Storm ").name, QString("SR Storm"));

    NetworkGearModel radio;
    radio.manufacturer = "Acme";
    radio.model = "Two Way";
    radio.transport = NetworkNode::WirelessDMX;
    radio.ports = 2;
    const NetworkNode w = NetworkGearCatalog::makeNode(radio, "Beach TX");
    QVERIFY(w.isWireless());
    QCOMPARE(w.ports.at(0).receiver.name, QString("Beach TX RX 1"));
    QCOMPARE(w.ports.at(1).receiver.name, QString("Beach TX RX 2"));
    QVERIFY(!w.ports.at(0).receiver.placed);

    // Kept in the workspace, and only written when set.
    NetworkTopology topo;
    topo.nodes() << n;
    NetworkNode hand;
    hand.name = "Hand Built";
    topo.nodes() << hand;
    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    QVERIFY(topo.saveXML(&writer));
    writer.writeEndDocument();
    QCOMPARE(buf.count("Model="), 1);
    NetworkTopology back;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
    {
        if (reader.name() == QStringLiteral("NetworkTopology"))
            QVERIFY(back.loadXML(reader, nullptr));
        else
            reader.skipCurrentElement();
    }
    QCOMPARE(back.nodes().at(0).model, QString("ENTTEC Storm 8"));
    QVERIFY(back.nodes().at(1).model.isEmpty());

    // The report names it.
    m_doc->networkTopology()->nodes() << n;
    const QString text = NetworkReport::build(m_doc).toText();
    QVERIFY2(text.contains("Storm 8 -- ArtNet (ENTTEC Storm 8)"), qPrintable(text));
}

QTEST_GUILESS_MAIN(NetworkTopology_Test)

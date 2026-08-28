/*
  Q Light Controller Plus
  artnet_test.cpp

  Copyright (c) Jano Svitok

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

#include <QTest>
#include <QNetworkInterface>
#include <QUdpSocket>
#include <QSharedPointer>

#define private public
#include "artnet_test.h"
#include "artnetpacketizer.h"
#include "artnetcontroller.h"
#include "artnetplugin.h"
#undef private

/****************************************************************************
 * ArtNet tests
 ****************************************************************************/

void ArtNet_Test::setupArtNetDmx()
{
    ArtNetPacketizer ap;

    QByteArray data;
    const QByteArray empty;
    const QByteArray fifty(50, 10);
    const QByteArray fiftyone(51, 10);
    const QByteArray full(512, 20);

    // empty data
    ap.setupArtNetDmx(data, 0, empty);

    QCOMPARE(data.size(), 20);
    QCOMPARE(data.data(), "Art-Net");

    // full data
    ap.setupArtNetDmx(data, 0, full);

    QCOMPARE(data.size(), 18 + 512);
    QCOMPARE(data.data(), "Art-Net");

    // partial data
    ap.setupArtNetDmx(data, 0, fifty);

    QCOMPARE(data.size(), 18 + 50);
    QCOMPARE(data.data(), "Art-Net");

    ap.setupArtNetDmx(data, 0, fiftyone);

    QCOMPARE(data.size(), 18 + 52);
    QCOMPARE(data.data(), "Art-Net");
}

QTEST_MAIN(ArtNet_Test)

/** A real ArtPollReply, captured off the wire from the rig's CR041R
 *  ArtNet->DMX gateway at 172.18.2.10. Using a genuine packet rather than a
 *  hand-built one is the point: the parser reads two dozen fields at fixed
 *  offsets, and an off-by-one in any of them is invisible against a fixture
 *  built from the same assumptions as the code. The expected values below were
 *  cross-checked against what DMX-Workshop reports for this node. */
static const char cr041rPollReply[] = {
    char(0x41), char(0x72), char(0x74), char(0x2d), char(0x4e), char(0x65), char(0x74), char(0x00), char(0x00), char(0x21), char(0xac), char(0x12),
    char(0x02), char(0x0a), char(0x36), char(0x19), char(0x00), char(0x0e), char(0x00), char(0x00), char(0x00), char(0x22), char(0x00), char(0x00),
    char(0x7a), char(0x70), char(0x43), char(0x52), char(0x30), char(0x34), char(0x31), char(0x52), char(0x5f), char(0x30), char(0x30), char(0x31),
    char(0x00), char(0x00), char(0xdc), char(0x00), char(0x00), char(0x20), char(0x70), char(0xb5), char(0x43), char(0x52), char(0x30), char(0x34),
    char(0x31), char(0x52), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x30), char(0x30), char(0x30), char(0x36), char(0x20), char(0x5b), char(0x38), char(0x34), char(0x34), char(0x33), char(0x5d), char(0x20),
    char(0x41), char(0x72), char(0x74), char(0x4e), char(0x65), char(0x74), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x04), char(0x80), char(0x80), char(0x80), char(0x80), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x80), char(0x80), char(0x80), char(0x80), char(0x00), char(0x01), char(0x02), char(0x03), char(0x00), char(0x01),
    char(0x02), char(0x03), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x02), char(0x4d), char(0x48),
    char(0x12), char(0x02), char(0x0a), char(0xac), char(0x12), char(0x02), char(0x0a), char(0x01), char(0x08), char(0x00)
};

void ArtNet_Test::fillArtPollReplyInfo()
{
    ArtNetPacketizer ap;
    ArtNetNodeInfo info;

    const QByteArray reply(cr041rPollReply, sizeof(cr041rPollReply));
    QCOMPARE(reply.size(), 214);
    QVERIFY(ap.fillArtPollReplyInfo(reply, info) == true);

    QCOMPARE(info.shortName, QString("CR041R_001"));
    QCOMPARE(info.longName, QString("CR041R"));
    QCOMPARE(info.ipAddress, QString("172.18.2.10"));
    QCOMPARE(info.macAddress, QString("02:4D:48:12:02:0A"));
    QCOMPARE(int(info.firmwareVersion), 14);        // DMX-Workshop: "V0.14"
    QCOMPARE(int(info.oemCode), 0x0022);
    QCOMPARE(int(info.estaCode), 0x707A);           // EstaMan is little-endian
    QCOMPARE(info.portsNumber, 4);
    QCOMPARE(int(info.netSwitch), 0);
    QCOMPARE(int(info.subSwitch), 0);

    // Four output ports carrying universes 0..3.
    for (int i = 0; i < 4; i++)
    {
        QVERIFY((info.portTypes[i] & 0x80) != 0);   // output
        QCOMPARE(int(info.swOut[i]), i);
    }

    /* Capability is Status1 bit 1. This node is transmit-only -- DMX-Workshop
       reports "Node is not RDM capable (Unidirectional DMX)" and it never
       answers an ArtTodRequest. Note its GoodOutput bit 3 ("RDM disabled") is
       clear, which reads like RDM support if you check the wrong bit; that
       misreading is exactly what this assertion guards against. */
    QCOMPARE(info.rdmCapable, false);
    QVERIFY((info.goodOutput[0] & 0x08) == 0);

    // Too short to parse must be rejected rather than read past the end.
    QVERIFY(ap.fillArtPollReplyInfo(reply.left(100), info) == false);
    QVERIFY(ap.fillArtPollReplyInfo(QByteArray(), info) == false);
}


/****************************************************************************
 * Packet attribution
 *
 * Which controller a received datagram belongs to. This used to fall back to
 * "the first controller with a pulse" whenever the sender matched no
 * interface's subnet -- and m_IOmapping is sorted by IP string, so first meant
 * 127.0.0.1. Every node the console could not place was therefore filed under
 * loopback, and the tree confidently showed 172.18.2.10 hanging off it.
 ****************************************************************************/

/** Two real interfaces with IPv4 addresses, or an empty list. Real ones
 *  because QNetworkInterface::index() is read-only -- the routing being tested
 *  compares against it, so it cannot be faked. */
static QList<QNetworkInterface> twoIPv4Interfaces()
{
    QList<QNetworkInterface> out;
    foreach (const QNetworkInterface &iface, QNetworkInterface::allInterfaces())
    {
        foreach (const QNetworkAddressEntry &e, iface.addressEntries())
        {
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol
                    && e.ip().isNull() == false)
            {
                out << iface;
                break;
            }
        }
        if (out.count() == 2)
            break;
    }
    return out;
}

static QNetworkAddressEntry firstIPv4(const QNetworkInterface &iface)
{
    foreach (const QNetworkAddressEntry &e, iface.addressEntries())
    {
        if (e.ip().protocol() == QAbstractSocket::IPv4Protocol)
            return e;
    }
    return QNetworkAddressEntry();
}

void ArtNet_Test::packetIsAttributedToTheArrivalInterface()
{
    const QList<QNetworkInterface> ifaces = twoIPv4Interfaces();
    if (ifaces.count() < 2)
        QSKIP("needs two IPv4 interfaces to tell attribution apart");

    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());

    plugin.m_IOmapping.clear();
    for (int i = 0; i < 2; i++)
    {
        ArtNetIO io;
        io.iface = ifaces.at(i);
        io.address = firstIPv4(ifaces.at(i));
        io.controller = new ArtNetController(io.iface, io.address, sock,
                                             quint32(i), &plugin);
        plugin.m_IOmapping.append(io);
    }

    const QByteArray reply(cr041rPollReply, sizeof(cr041rPollReply));

    /* A sender on no interface's subnet -- the L2-adjacent, L3-foreign node
       that show gear ships as. Arrival interface is the SECOND entry, so
       anything that ignores it and falls back to list order lands on the
       first and fails here. */
    plugin.handlePacket(reply, QHostAddress("203.0.113.7"),
                        uint(ifaces.at(1).index()));

    QCOMPARE(plugin.m_IOmapping.at(0).controller->getNodesList().count(), 0);
    QCOMPARE(plugin.m_IOmapping.at(1).controller->getNodesList().count(), 1);

    plugin.m_IOmapping.clear();
}

void ArtNet_Test::offSegmentPacketIsNotFiledUnderLoopback()
{
    const QList<QNetworkInterface> ifaces = twoIPv4Interfaces();
    if (ifaces.count() < 2)
        QSKIP("needs two IPv4 interfaces to tell attribution apart");

    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());

    plugin.m_IOmapping.clear();
    for (int i = 0; i < 2; i++)
    {
        ArtNetIO io;
        io.iface = ifaces.at(i);
        io.address = firstIPv4(ifaces.at(i));
        io.controller = new ArtNetController(io.iface, io.address, sock,
                                             quint32(i), &plugin);
        plugin.m_IOmapping.append(io);
    }

    const QByteArray reply(cr041rPollReply, sizeof(cr041rPollReply));

    /* No arrival interface reported (0) AND no subnet match: there is no
       honest answer, so the packet must be dropped rather than attributed to
       whichever controller happens to sort first. Guessing here is what put
       172.18.2.10 under 127.0.0.1. */
    plugin.handlePacket(reply, QHostAddress("203.0.113.7"), 0);

    QCOMPARE(plugin.m_IOmapping.at(0).controller->getNodesList().count(), 0);
    QCOMPARE(plugin.m_IOmapping.at(1).controller->getNodesList().count(), 0);

    plugin.m_IOmapping.clear();
}

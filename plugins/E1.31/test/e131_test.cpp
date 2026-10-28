/*
  Q Light Controller Plus
  e131_test.cpp

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
#include <QSignalSpy>
#include <QRegularExpression>
#include <QNetworkInterface>
#include <QUdpSocket>

#define private public
#include "e131_test.h"
#include "e131controller.h"
#include "e131plugin.h"
#include "e131packetizer.h"
#undef private

static const qint64 kFrameMs = 23;

/** A controller on loopback with output universes @a universes. Binding its
 *  socket to 127.0.0.1 sends nothing, and nothing here writes to it. */
static E131Controller *testController(QObject *parent, QList<quint32> universes)
{
    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress::LocalHost);
    entry.setPrefixLength(8);
    E131Controller *ctrl = new E131Controller(QNetworkInterface(), entry, 0, parent);
    foreach (quint32 u, universes)
        ctrl->addUniverse(u, E131Controller::Output);
    ctrl->m_testNowMs = 0;
    return ctrl;
}

/** Feed universe @a u one result every @a periodMs for @a forMs, as a send
 *  to its multicast address would; count announcements in @a changes */
static void feed(E131Controller *ctrl, quint32 u, bool ok, qint64 forMs,
                 qint64 periodMs, int &changes)
{
    UniverseInfo &info = ctrl->m_universeMap[u];
    const qint64 end = ctrl->m_testNowMs + forMs;
    for (; ctrl->m_testNowMs < end; ctrl->m_testNowMs += periodMs)
        if (ctrl->reportSendResult(info, u, info.outputMcastAddress, ok))
            changes++;
}

void E131_Test::transientFailuresAreNotReported()
{
    QObject owner;
    E131Controller *ctrl = testController(&owner, { 0 });
    int changes = 0;
    for (int i = 0; i < 5; i++)
    {
        feed(ctrl, 0, true, 2000, kFrameMs, changes);
        feed(ctrl, 0, false, 500, kFrameMs, changes);
    }
    feed(ctrl, 0, true, 2000, kFrameMs, changes);
    QCOMPARE(changes, 0);
    QVERIFY(ctrl->outputProblems().isEmpty());
    QCOMPARE(ctrl->getPacketSentNumber() > 0, true);
}

void E131_Test::sustainedFailureIsReportedOnceAndRecovers()
{
    QObject owner;
    E131Controller *ctrl = testController(&owner, { 2 });
    int changes = 0;
    feed(ctrl, 2, false, 60000, kFrameMs, changes);
    QCOMPARE(changes, 1);

    const QList<QPair<quint32, QString> > p = ctrl->outputProblems();
    QCOMPARE(p.count(), 1);
    QCOMPARE(p.first().first, quint32(2));
    QVERIFY2(QRegularExpression("^E1\\.31 to 239\\.255\\.0\\.3: not reachable since \\d\\d:\\d\\d$")
                 .match(p.first().second).hasMatch(), qPrintable(p.first().second));

    // Working again: cleared after 30 s of good sends, not on the first
    feed(ctrl, 2, true, 29000, kFrameMs, changes);
    QCOMPARE(changes, 1);
    QCOMPARE(ctrl->outputProblems().count(), 1);
    feed(ctrl, 2, true, 2000, kFrameMs, changes);
    QCOMPARE(changes, 2);
    QVERIFY(ctrl->outputProblems().isEmpty());
}

void E131_Test::alternatingResultsDoNotToggle()
{
    QObject owner;
    E131Controller *ctrl = testController(&owner, { 0 });
    UniverseInfo &info = ctrl->m_universeMap[0];
    int changes = 0;
    bool ok = true;
    for (int i = 0; i < 5000; i++, ctrl->m_testNowMs += kFrameMs)
    {
        if (ctrl->reportSendResult(info, 0, info.outputMcastAddress, ok))
            changes++;
        ok = !ok;
    }
    QCOMPARE(changes, 1);
    QCOMPARE(ctrl->outputProblems().count(), 1);
}

void E131_Test::aNewTargetStartsClean()
{
    QObject owner;
    E131Controller *ctrl = testController(&owner, { 0 });
    int changes = 0;
    feed(ctrl, 0, false, 5000, kFrameMs, changes);
    QCOMPARE(ctrl->outputProblems().count(), 1);

    // Retargeted to unicast: the old address's failures say nothing about it
    UniverseInfo &info = ctrl->m_universeMap[0];
    QVERIFY(ctrl->reportSendResult(info, 0, QHostAddress("10.0.0.7"), true));
    QVERIFY(ctrl->outputProblems().isEmpty());
}

void E131_Test::pluginGroupsAProblemsUniverses()
{
    E131Plugin plugin;
    E131Controller *ctrl = testController(&plugin, { 0, 1 });

    // Both universes aimed at one address that can't be reached
    for (quint32 u = 0; u < 2; u++)
        ctrl->m_universeMap[u].outputMcastAddress = QHostAddress("239.255.0.9");
    int changes = 0;
    for (int i = 0; i < 100; i++, ctrl->m_testNowMs += kFrameMs)
        for (quint32 u = 0; u < 2; u++)
            if (ctrl->reportSendResult(ctrl->m_universeMap[u], u, QHostAddress("239.255.0.9"), false))
                changes++;
    QCOMPARE(changes, 2);

    E131IO io;
    io.controller = ctrl;
    plugin.m_IOmapping.clear();
    plugin.m_IOmapping.append(io);

    const QStringList all = plugin.targetProblems();
    QCOMPARE(all.count(), 1);
    QVERIFY2(all.first().startsWith("E1.31 to 239.255.0.9: not reachable since ")
             && all.first().endsWith(" (universes 1, 2)"), qPrintable(all.first()));
    QCOMPARE(plugin.outputProblems(0, 1).count(), 1);
    QVERIFY(plugin.outputProblems(0, 7).isEmpty());
    QVERIFY(plugin.outputProblems(3, 0).isEmpty());

    plugin.m_IOmapping.clear();
}

static int be16(const QByteArray &d, int at)
{
    return (uchar(d.at(at)) << 8) | uchar(d.at(at + 1));
}

void E131_Test::dmxPacketLayout()
{
    E131Packetizer p("02:4D:48:12:02:0A");
    QByteArray values(512, char(0));
    values[0] = char(0x11);
    values[511] = char(0x22);
    QByteArray d;
    p.setupE131Dmx(d, 0x0123, 150, values);

    QCOMPARE(d.size(), 126 + 512);
    QCOMPARE(d.mid(4, 12), QByteArray("ASC-E1.17\0\0\0", 12));
    // Each layer's flags (0x7) and length, to the end of the packet
    QCOMPARE(be16(d, 16), 0x7000 | (d.size() - 16));
    QCOMPARE(be16(d, 38), 0x7000 | (d.size() - 38));
    QCOMPARE(be16(d, 115), 0x7000 | (d.size() - 115));
    QCOMPARE(d.mid(18, 4), QByteArray::fromHex("00000004"));      // root vector
    QCOMPARE(d.mid(40, 4), QByteArray::fromHex("00000002"));      // E1.31 data
    QCOMPARE(d.mid(32, 6), QByteArray::fromHex("024d4812020a"));  // CID ends with the MAC
    QCOMPARE(d.mid(44, 31), QByteArray("Q Light Controller Plus - E1.31"));
    QCOMPARE(d.mid(75, 33), QByteArray(33, char(0)));             // name padded to 64
    QCOMPARE(int(uchar(d.at(108))), 150);                          // priority
    QCOMPARE(be16(d, 113), 0x0123);                                // universe
    QCOMPARE(int(uchar(d.at(117))), 0x02);                         // DMP set property
    QCOMPARE(int(uchar(d.at(118))), 0xa1);
    QCOMPARE(be16(d, 121), 1);                                     // address increment
    QCOMPARE(be16(d, 123), 513);                                   // start code + slots
    QCOMPARE(int(uchar(d.at(125))), 0);                            // DMX start code
    QCOMPARE(d.mid(126), values);
    QVERIFY(p.checkPacket(d));

    // What it sends, it reads back
    QByteArray dmx;
    quint32 universe = 0;
    QVERIFY(p.fillDMXdata(d, dmx, universe));
    QCOMPARE(universe, 0x0123u);
    QCOMPARE(dmx, values);

    // A short frame carries only its slots; sequence counts per universe
    const int seq = uchar(d.at(111));
    p.setupE131Dmx(d, 0x0123, 100, QByteArray(24, char(7)));
    QCOMPARE(d.size(), 126 + 24);
    QCOMPARE(be16(d, 123), 25);
    QCOMPARE(be16(d, 16), 0x7000 | (d.size() - 16));
    QCOMPARE(int(uchar(d.at(108))), 100);
    QCOMPARE(int(uchar(d.at(111))), seq + 1);
    p.setupE131Dmx(d, 7, 100, QByteArray(24, char(7)));
    const int other = uchar(d.at(111));
    p.setupE131Dmx(d, 7, 100, QByteArray(24, char(7)));
    QCOMPARE(int(uchar(d.at(111))), other + 1);

    // A MAC that isn't one: the built-in CID tail
    E131Packetizer noMac("");
    noMac.setupE131Dmx(d, 1, 100, values);
    QCOMPARE(d.mid(32, 6), QByteArray::fromHex("317a07c10052"));

    // Not sACN: not checked further
    QByteArray bad = d;
    bad[4] = 'B';
    QVERIFY(!p.checkPacket(bad));
    bad = d;
    bad[43] = char(0x03);                                         // not a data packet
    QVERIFY(!p.checkPacket(bad));
    bad = d.left(124);
    QVERIFY(!p.checkPacket(bad));
    bad = d;
    bad[125] = char(0xcc);                                        // an RDM start code
    QVERIFY(!p.fillDMXdata(bad, dmx, universe));
    QVERIFY(dmx.isEmpty());
}

void E131_Test::universeDefaultsFollowTheSpec()
{
    QObject owner;
    E131Controller *ctrl = testController(&owner, { 0, 299 });

    UniverseInfo *info = ctrl->getUniverseInfo(0);
    QVERIFY(info != NULL);
    QCOMPARE(info->outputMcastAddress, QHostAddress("239.255.0.1"));
    QCOMPARE(info->inputMcastAddress, QHostAddress("239.255.0.1"));
    QCOMPARE(info->outputUniverse, 1u);
    QCOMPARE(info->inputUniverse, 1u);
    QCOMPARE(info->outputPriority, quint32(E131_PRIORITY_DEFAULT));
    QCOMPARE(info->outputMulticast, true);
    QCOMPARE(int(info->outputUcastPort), E131_DEFAULT_PORT);
    QCOMPARE(info->outputUcastAddress, QHostAddress::LocalHost);
    QCOMPARE(info->outputTransmissionMode, int(E131Controller::Full));

    /* sACN universe 300 is 239.255.1.44 (0x012C): the high byte goes in
       the third octet. It used to be "239.255.0.300", which is no address. */
    info = ctrl->getUniverseInfo(299);
    QCOMPARE(info->outputUniverse, 300u);
    QCOMPARE(info->outputMcastAddress, QHostAddress("239.255.1.44"));
    QCOMPARE(info->inputMcastAddress, QHostAddress("239.255.1.44"));
    QVERIFY(!info->outputMcastAddress.isNull());

    QCOMPARE(int(ctrl->type()), int(E131Controller::Output));
    QCOMPARE(ctrl->universesList(), QList<quint32>({ 0, 299 }));
    ctrl->removeUniverse(299, E131Controller::Output);
    QVERIFY(ctrl->getUniverseInfo(299) == NULL);

    // The saved forms of the multicast address: the legacy last octet, or whole
    ctrl->setOutputMCastAddress(0, "7", true);
    QCOMPARE(ctrl->getUniverseInfo(0)->outputMcastAddress, QHostAddress("239.255.0.7"));
    ctrl->setOutputMCastAddress(0, "239.255.2.9", false);
    QCOMPARE(ctrl->getUniverseInfo(0)->outputMcastAddress, QHostAddress("239.255.2.9"));

    typedef E131Controller EC;
    QCOMPARE(EC::stringToTransmissionMode(EC::transmissionModeToString(EC::Partial)), EC::Partial);
    QCOMPARE(EC::stringToTransmissionMode(EC::transmissionModeToString(EC::Full)), EC::Full);
    QCOMPARE(EC::stringToTransmissionMode("bogus"), EC::Full);

    // Setters on a universe that isn't there change nothing
    ctrl->setOutputUniverse(5, 9);
    ctrl->setOutputPriority(5, 9);
    ctrl->setOutputMulticast(5, false);
    QCOMPARE(ctrl->universesList(), QList<quint32>({ 0 }));
}

void E131_Test::aUniverseSendsWhereItIsTold()
{
    /* Unicast to a socket of our own on 127.0.0.1: the one thing here that
       is really sent, and it never leaves the machine. */
    QUdpSocket rx;
    QVERIFY(rx.bind(QHostAddress::LocalHost, 0));
    QObject owner;
    E131Controller *ctrl = testController(&owner, { 0 });
    ctrl->setOutputMulticast(0, false);
    ctrl->setOutputUCastAddress(0, "127.0.0.1");
    ctrl->setOutputUCastPort(0, rx.localPort());
    ctrl->setOutputUniverse(0, 7);
    ctrl->setOutputPriority(0, 150);

    auto receive = [&rx]() {
        QByteArray d;
        if (rx.hasPendingDatagrams() || rx.waitForReadyRead(5000))
        {
            d.resize(int(rx.pendingDatagramSize()));
            rx.readDatagram(d.data(), d.size());
        }
        return d;
    };

    const quint64 sent = ctrl->getPacketSentNumber();
    ctrl->sendDmx(0, QByteArray::fromHex("010203"));
    QByteArray d = receive();
    QCOMPARE(d.size(), 126 + 512);                 // Full: the whole universe
    QCOMPARE(be16(d, 113), 7);
    QCOMPARE(int(uchar(d.at(108))), 150);
    QCOMPARE(d.mid(126, 4), QByteArray::fromHex("01020300"));
    QCOMPARE(ctrl->getPacketSentNumber(), sent + 1);

    ctrl->setOutputTransmissionMode(0, E131Controller::Partial);
    ctrl->sendDmx(0, QByteArray::fromHex("010203"));
    d = receive();
    QCOMPARE(d.size(), 126 + 3);                   // Partial: what it was given
    QCOMPARE(be16(d, 123), 4);

    // Held off the rig network: nothing
    ctrl->m_held.store(true);
    ctrl->sendDmx(0, QByteArray::fromHex("010203"));
    QVERIFY(!rx.waitForReadyRead(50));
    QCOMPARE(ctrl->getPacketSentNumber(), sent + 2);
}

void E131_Test::pluginAppliesAndKeepsTheParameters()
{
    E131Plugin plugin;
    QNetworkAddressEntry lo;
    lo.setIp(QHostAddress::LocalHost);
    lo.setPrefixLength(8);
    E131IO io;
    io.address = lo;
    io.controller = NULL;
    plugin.m_IOmapping.clear();
    plugin.m_IOmapping.append(io);
    /* A line that isn't there is refused at once: no waiting for
       interfaces (the user's setting), and no rescan to re-sort the lines */
    plugin.m_ifaceWaitTime = 0;
    typedef QLCIOPlugin P;

    QVERIFY(!plugin.openOutput(1, 3));                  // no such line
    QVERIFY(plugin.openOutput(0, 3));
    E131Controller *ctrl = plugin.m_IOmapping.at(0).controller;
    QVERIFY(ctrl != NULL);

    QUdpSocket rx;
    QVERIFY(rx.bind(QHostAddress::LocalHost, 0));
    // As a show's patch is restored: each saved parameter in turn
    plugin.setParameter(3, 0, P::Output, E131_MULTICAST, 0);
    plugin.setParameter(3, 0, P::Output, E131_UCASTIP, "127.0.0.1");
    plugin.setParameter(3, 0, P::Output, E131_UCASTPORT, rx.localPort());
    plugin.setParameter(3, 0, P::Output, E131_UNIVERSE, 9);
    plugin.setParameter(3, 0, P::Output, E131_PRIORITY, 150);
    plugin.setParameter(3, 0, P::Output, E131_TRANSMITMODE, "Partial");
    plugin.setParameter(3, 0, P::Output, E131_MCASTFULLIP, "239.255.0.77");
    UniverseInfo *info = ctrl->getUniverseInfo(3);
    QCOMPARE(info->outputMulticast, false);
    QCOMPARE(info->outputUcastAddress, QHostAddress::LocalHost);
    QCOMPARE(info->outputUniverse, 9u);
    QCOMPARE(info->outputPriority, 150u);
    QCOMPARE(info->outputTransmissionMode, int(E131Controller::Partial));
    QCOMPARE(info->outputMcastAddress, QHostAddress("239.255.0.77"));
    plugin.setParameter(3, 0, P::Output, E131_MCASTIP, "78");     // the legacy form
    QCOMPARE(info->outputMcastAddress, QHostAddress("239.255.0.78"));

    const QMap<QString, QVariant> kept = plugin.getParameters(3, 0, P::Output);
    QCOMPARE(kept.value(E131_UNIVERSE).toUInt(), 9u);
    QCOMPARE(kept.value(E131_PRIORITY).toUInt(), 150u);
    QCOMPARE(kept.value(E131_TRANSMITMODE).toString(), QString("Partial"));
    QVERIFY(plugin.getParameters(3, 1, P::Output).isEmpty());   // another line's

    // What it writes goes there
    plugin.writeUniverse(3, 0, QByteArray::fromHex("0102"), true);
    plugin.writeUniverse(3, 7, QByteArray::fromHex("0102"), true);   // no such line
    QVERIFY(rx.hasPendingDatagrams() || rx.waitForReadyRead(5000));
    QByteArray d(int(rx.pendingDatagramSize()), '\0');
    rx.readDatagram(d.data(), d.size());
    QCOMPARE(be16(d, 113), 9);
    QCOMPARE(int(uchar(d.at(108))), 150);
    QCOMPARE(d.size(), 126 + 2);

    // An input parameter that isn't one: refused, not kept
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("is not a valid E1.31 input parameter"));
    plugin.setParameter(3, 0, P::Input, E131_PRIORITY, 1);
    QVERIFY(plugin.getParameters(3, 0, P::Input).isEmpty());

    // Closed: its parameters go, and the line's controller with its last universe
    plugin.closeOutput(0, 3);
    QVERIFY(plugin.getParameters(3, 0, P::Output).isEmpty());
    QVERIFY(plugin.m_IOmapping.at(0).controller == NULL);
}

QTEST_MAIN(E131_Test)

void E131_Test::dmxPacketsAreBoundedByTheDatagramAndAUniverse()
{
    E131Packetizer p("02:00:00:00:00:01");
    QByteArray full;
    p.setupE131Dmx(full, 0x0102, 100, QByteArray(4, char(0x33)));
    QCOMPARE(full.size(), 126 + 4);

    QByteArray dmx;
    quint32 universe = 0;
    QVERIFY(p.checkPacket(full));
    QVERIFY(p.fillDMXdata(full, dmx, universe));
    QCOMPARE(universe, quint32(0x0102));
    QCOMPARE(dmx, QByteArray(4, char(0x33)));

    /* 125 bytes passes checkPacket() but has no start code. */
    QByteArray b = full.left(125);
    QVERIFY(p.checkPacket(b));
    QVERIFY(!p.fillDMXdata(b, dmx, universe));
    QVERIFY(dmx.isEmpty());

    /* A count of 0 (no start code counted) is no channels, not the rest of
       the datagram; a count past 513 is one universe. */
    QByteArray zero = full;
    zero[123] = char(0x00);
    zero[124] = char(0x00);
    QVERIFY(p.fillDMXdata(zero, dmx, universe));
    QVERIFY(dmx.isEmpty());

    QByteArray big;
    p.setupE131Dmx(big, 1, 100, QByteArray(700, char(0x44)));
    QVERIFY(p.fillDMXdata(big, dmx, universe));
    QCOMPARE(dmx.size(), 512);

    /* Claiming more than it holds: what is there. */
    QByteArray liar = full;
    liar[123] = char(0x02);
    liar[124] = char(0x01);
    QVERIFY(p.fillDMXdata(liar, dmx, universe));
    QCOMPARE(dmx.size(), 4);
}

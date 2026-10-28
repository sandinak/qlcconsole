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
#include <QRegularExpression>
#include <QSignalSpy>
#include <thread>
#include <future>
#include <chrono>

#define private public
#include "artnet_test.h"
#include "artnetpacketizer.h"
#include "artnetcontroller.h"
#include "artnetplugin.h"
#include "artnetnodesetup.h"
#include "rdmprotocol.h"
#undef private

#include <QNetworkDatagram>
#include <QPushButton>
#include <QGroupBox>
#include <QLineEdit>
#include <QLabel>

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
       answers an ArtTodRequest. GoodOutput says nothing about RDM at all (bit 3
       is "merging"; "RDM disabled" lives in GoodOutputB), so it must not be
       read as capability; that misreading is what this assertion guards
       against. */
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

void ArtNet_Test::lineOnSameSubnetResolvesChangedAddress()
{
    ArtNetPlugin plugin;

    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress("172.18.2.42"));
    entry.setPrefixLength(24);

    ArtNetIO io;
    io.controller = NULL;   // lineOnSameSubnet() never touches this
    io.address = entry;
    plugin.m_IOmapping.clear();
    plugin.m_IOmapping.append(io);

    /* Same subnet, different host part -- a DHCP re-lease on this machine,
       or the same workspace opened on different hardware plugged into the
       same physical network. Line 0, even though this line's own address
       (172.18.2.42) is not the one being looked up (172.18.2.17). */
    QCOMPARE(plugin.lineOnSameSubnet("172.18.2.17"), 0);

    // A genuinely different network must not match.
    QCOMPARE(plugin.lineOnSameSubnet("10.0.0.5"), -1);

    // Not an address at all -- must not crash or false-match.
    QCOMPARE(plugin.lineOnSameSubnet("not an address"), -1);

    plugin.m_IOmapping.clear();
}

/****************************************************************************
 * Send failure reporting (SendHealth)
 ****************************************************************************/

/* Frame period while data is changing (~44 Hz), and the idle refresh period
   of Standard mode (SEND_INTERVAL_MS). The rules must hold at both. */
static const qint64 kFrameMs = 23;
static const qint64 kIdleMs = 2000;

namespace
{
/** Feeds an SendHealth a timeline and tallies what it reports */
struct HealthDriver
{
    SendHealth health;
    qint64 now = 0;
    int failingReports = 0;
    int recoveredReports = 0;
    qint64 failingAt = -1;
    qint64 recoveredAt = -1;

    void send(bool ok, bool hard = false)
    {
        switch (health.record(ok, now, hard))
        {
            case SendHealth::StartedFailing:
                failingReports++;
                failingAt = now;
            break;
            case SendHealth::Recovered:
                recoveredReports++;
                recoveredAt = now;
            break;
            case SendHealth::NoChange:
            break;
        }
    }

    /** Send @a ok every @a periodMs for @a forMs */
    void run(bool ok, qint64 forMs, qint64 periodMs)
    {
        const qint64 end = now + forMs;
        for (; now < end; now += periodMs)
            send(ok);
    }
};
}

void ArtNet_Test::sendHealthTransientBurstIsNotReported()
{
    HealthDriver d;

    // Healthy, then a burst of failures shorter than FailAfterMs, then clean
    // sends: three times over. A full send buffer looks like this.
    for (int i = 0; i < 3; i++)
    {
        d.run(true, 2000, kFrameMs);
        d.run(false, 500, kFrameMs);
        d.run(true, 2000, kFrameMs);
    }
    QCOMPARE(d.failingReports, 0);
    QCOMPARE(d.recoveredReports, 0);
    QVERIFY(d.health.isFailing() == false);

    // A single failure at the idle cadence, then clean ones: not reported
    HealthDriver idle;
    idle.run(true, 10000, kIdleMs);
    idle.run(false, 1, kIdleMs);
    idle.run(true, 10000, kIdleMs);
    QCOMPARE(idle.failingReports, 0);
}

void ArtNet_Test::sendHealthSustainedFailureIsReported()
{
    // At frame rate: reported once, after FailAfterMs and not before
    HealthDriver d;
    d.run(true, 1000, kFrameMs);
    const qint64 firstFailure = d.now;
    d.run(false, 60000, kFrameMs);
    QCOMPARE(d.failingReports, 1);
    QVERIFY(d.failingAt - firstFailure >= SendHealth::FailAfterMs);
    QVERIFY(d.failingAt - firstFailure < SendHealth::FailAfterMs + kFrameMs);
    QVERIFY(d.health.isFailing());
    QCOMPARE(d.recoveredReports, 0);
    QVERIFY(d.health.failedSends() > 2000);

    // At the idle refresh: the second consecutive failure reports it
    HealthDriver idle;
    idle.run(false, 60000, kIdleMs);
    QCOMPARE(idle.failingReports, 1);
    QCOMPARE(idle.failingAt, kIdleMs);
}

void ArtNet_Test::sendHealthHardErrorIsReportedAtOnce()
{
    HealthDriver d;
    d.run(true, 1000, kFrameMs);
    const qint64 at = d.now;
    d.send(false, true);
    QCOMPARE(d.failingReports, 1);
    QCOMPARE(d.failingAt, at);
    QVERIFY(d.health.isFailing());

    // ...and further hard errors don't repeat the report
    for (int i = 0; i < 100; i++)
    {
        d.now += kFrameMs;
        d.send(false, true);
    }
    QCOMPARE(d.failingReports, 1);
}

void ArtNet_Test::sendHealthRecoveryClears()
{
    HealthDriver d;
    d.run(false, 5000, kFrameMs);
    QCOMPARE(d.failingReports, 1);
    const quint64 failures = d.health.failedSends();

    const qint64 recoveryStart = d.now;
    d.run(true, 60000, kFrameMs);
    QCOMPARE(d.recoveredReports, 1);
    QVERIFY(d.recoveredAt - recoveryStart >= SendHealth::RecoverAfterMs);
    QVERIFY(d.recoveredAt - recoveryStart < SendHealth::RecoverAfterMs + kFrameMs);
    QVERIFY(d.health.isFailing() == false);
    QCOMPARE(d.health.failedSends(), failures);
    QVERIFY(d.health.failedForMs() >= 5000 - kFrameMs);
    QVERIFY(d.health.failedForMs() <= 5000 + kFrameMs);

    // A failure inside the recovery window restarts it
    HealthDriver r;
    r.run(false, 5000, kIdleMs);
    r.run(true, 20000, kIdleMs);
    r.send(false);
    r.now += kIdleMs;
    const qint64 restart = r.now;
    r.run(true, 60000, kIdleMs);
    QCOMPARE(r.recoveredReports, 1);
    QVERIFY(r.recoveredAt - restart >= SendHealth::RecoverAfterMs);

    // ...and a later sustained failure is reported afresh, counted from zero
    r.run(false, 5000, kIdleMs);
    QCOMPARE(r.failingReports, 2);
    QVERIFY(r.health.failedSends() < 5);
}

void ArtNet_Test::sendHealthAlternatingResultsDoNotToggle()
{
    // Every other send failing, for two minutes: reported failing once
    // (half the frames are being lost) and never "recovered" in between.
    HealthDriver d;
    bool ok = true;
    for (const qint64 end = d.now + 120000; d.now < end; d.now += kFrameMs)
    {
        d.send(ok);
        ok = !ok;
    }
    QCOMPARE(d.failingReports, 1);
    QCOMPARE(d.recoveredReports, 0);
    QVERIFY(d.health.isFailing());

    // Same at the idle cadence
    HealthDriver idle;
    ok = false;
    for (const qint64 end = idle.now + 120000; idle.now < end; idle.now += kIdleMs)
    {
        idle.send(ok);
        ok = !ok;
    }
    QCOMPARE(idle.failingReports, 1);
    QCOMPARE(idle.recoveredReports, 0);
}

void ArtNet_Test::sendHealthArpFlapIsReportedOnce()
{
    /* The case from the 2026-08-25 soak: a unicast target that never answers
       ARP. On macOS sends succeed for ~5 s while ARP is outstanding, then
       fail for host_down_time (20 s), and round again. Per state change that
       was a fail/recover pair every ~25 s per universe; it must be ONE report
       for the whole two minutes, at frame rate and at the idle refresh. */
    const qint64 periods[] = { kFrameMs, kIdleMs };
    for (const qint64 period : periods)
    {
        HealthDriver d;
        for (int cycle = 0; cycle < 5; cycle++)
        {
            d.run(true, 5000, period);
            d.run(false, 20000, period);
        }
        QCOMPARE(d.failingReports, 1);
        QCOMPARE(d.recoveredReports, 0);
        QVERIFY(d.health.isFailing());

        // The node is plugged in: sends succeed and keep succeeding
        d.run(true, 60000, period);
        QCOMPARE(d.recoveredReports, 1);
        QCOMPARE(d.failingReports, 1);
    }
}

void ArtNet_Test::sendHealthHardErrorClassification()
{
    QVERIFY(SendHealth::isHardError(QAbstractSocket::DatagramTooLargeError));
    QVERIFY(SendHealth::isHardError(QAbstractSocket::SocketAccessError));
    QVERIFY(SendHealth::isHardError(QAbstractSocket::SocketAddressNotAvailableError));
    QVERIFY(SendHealth::isHardError(QAbstractSocket::UnsupportedSocketOperationError));

    // Qt reports both "host is down" (ARP, comes and goes) and "no route" as
    // NetworkError, so it must go through the sustained rule
    QVERIFY(SendHealth::isHardError(QAbstractSocket::NetworkError) == false);
    QVERIFY(SendHealth::isHardError(QAbstractSocket::SocketResourceError) == false);
    QVERIFY(SendHealth::isHardError(QAbstractSocket::TemporaryError) == false);
    QVERIFY(SendHealth::isHardError(QAbstractSocket::UnknownSocketError) == false);
}

void ArtNet_Test::controllerLogsOnlyOnStateChange()
{
    /* The controller wiring, without sending anything: results are handed to
       reportSendResult() directly. The socket is never bound or written, and
       the universe goes straight into the map -- addUniverse() would start
       the ArtPoll timer and broadcast a poll. */
    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress::LocalHost);
    entry.setPrefixLength(8);
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    ArtNetController ctrl(QNetworkInterface(), entry, sock, 0);

    UniverseInfo info;
    info.type = ArtNetController::Output;
    info.outputAddress = QHostAddress::LocalHost;
    ctrl.m_universeMap[0] = info;
    UniverseInfo &u = ctrl.m_universeMap[0];

    QTest::failOnWarning(QRegularExpression("ArtNet"));

    ctrl.reportSendResult(u, 0, u.outputAddress, true);
    ctrl.reportSendResult(u, 0, u.outputAddress, true);
    QCOMPARE(ctrl.getPacketSentNumber(), quint64(2));

    // Two back-to-back failures are a transient, not a failing universe
    ctrl.reportSendResult(u, 0, u.outputAddress, false);
    ctrl.reportSendResult(u, 0, u.outputAddress, false);
    QVERIFY(u.sendHealth.isFailing() == false);
    QCOMPARE(ctrl.getPacketSentNumber(), quint64(2));

    ctrl.reportSendResult(u, 0, u.outputAddress, true);
    QCOMPARE(ctrl.getPacketSentNumber(), quint64(3));
}

/****************************************************************************
 * Node health (ArtNetNodeWatch)
 ****************************************************************************/

static const qint64 kPollMs = 3000;   // POLL_INTERVAL_MS

/** An ArtPollReply as parsed: one node, up to four output ports */
static ArtNetNodeInfo nodeReply(const QString &name, const QString &ip,
                                QList<uchar> swOut, QList<uchar> goodOutput,
                                uchar bindIndex = 0, uchar net = 0, uchar sub = 0)
{
    ArtNetNodeInfo n;
    n.shortName = name;
    n.ipAddress = ip;
    n.portsNumber = swOut.count();
    n.netSwitch = net;
    n.subSwitch = sub;
    n.bindIndex = bindIndex;
    for (int p = 0; p < 4; p++)
    {
        n.portTypes[p] = p < swOut.count() ? 0x80 : 0;
        n.swOut[p] = p < swOut.count() ? swOut.at(p) : 0;
        n.goodOutput[p] = p < goodOutput.count() ? goodOutput.at(p) : 0;
        n.goodInput[p] = 0;
        n.swIn[p] = 0;
    }
    return n;
}

namespace
{
/** Polls every 3 s; the node answers each poll (or not) right after it */
struct NodeDriver
{
    ArtNetNodeWatch watch;
    qint64 now = 0;
    int changes = 0;
    QHostAddress addr = QHostAddress("172.18.2.10");
    ArtNetNodeInfo reply = nodeReply("CR041R", "172.18.2.10", { 0, 1, 2, 3 },
                                     { 0x80, 0x80, 0x80, 0x80 });

    /** One poll interval; @a answer says whether the node replies in it */
    void interval(bool answer)
    {
        if (watch.pollSent(now))
            changes++;
        if (answer && watch.replyHeard(addr, reply, now + 20))
            changes++;
        now += kPollMs;
    }

    bool silent() const
    {
        foreach (const ArtNetNodeWatch::Issue &i, watch.issuesFor(addr, false, 0))
            if (i.kind == ArtNetNodeWatch::Issue::NodeSilent)
                return true;
        return false;
    }
};
}

void ArtNet_Test::nodeWatchSilentAfterThreeMissedPolls()
{
    NodeDriver d;
    for (int i = 0; i < 10; i++)
        d.interval(true);
    QCOMPARE(d.changes, 0);
    const qint64 lastReply = d.now - kPollMs + 20;

    // Two missed, then answering again: one lost reply is not a dead node
    d.interval(false);
    d.interval(false);
    d.interval(true);
    d.interval(true);
    QVERIFY(d.silent() == false);
    QCOMPARE(d.changes, 0);

    // Three missed in a row: silent, and since its last reply
    const qint64 lastAnswer = d.now - kPollMs + 20;
    d.interval(false);
    d.interval(false);
    d.interval(false);
    QVERIFY(d.silent() == false);   // the third miss is judged at the next poll
    d.interval(false);
    QVERIFY(d.silent());
    QCOMPARE(d.changes, 1);
    QList<ArtNetNodeWatch::Issue> issues = d.watch.issuesFor(d.addr, false, 0);
    QCOMPARE(issues.count(), 1);
    QCOMPARE(issues.first().sinceMs, lastAnswer);
    QCOMPARE(issues.first().name, QString("CR041R"));
    QVERIFY(lastReply < lastAnswer);

    // Stays silent, without repeating itself
    for (int i = 0; i < 20; i++)
        d.interval(false);
    QCOMPARE(d.changes, 1);
}

void ArtNet_Test::nodeWatchAnsweringAgainNeedsTwoIntervals()
{
    NodeDriver d;
    d.interval(true);
    for (int i = 0; i < 4; i++)
        d.interval(false);
    QVERIFY(d.silent());
    QCOMPARE(d.changes, 1);

    // Answering every other poll: alive but flaky -- no toggling either way
    for (int i = 0; i < 20; i++)
        d.interval(i % 2 == 0);
    QVERIFY(d.silent());
    QCOMPARE(d.changes, 1);

    // Two answered intervals in a row: answering again, once
    d.interval(true);
    d.interval(true);
    d.interval(true);
    QVERIFY(d.silent() == false);
    QCOMPARE(d.changes, 2);
    QVERIFY(d.watch.issuesFor(d.addr, false, 0).isEmpty());
}

void ArtNet_Test::nodeWatchNeverHeardIsNotFlagged()
{
    ArtNetNodeWatch w;
    for (int i = 0; i < 50; i++)
        QVERIFY(w.pollSent(i * kPollMs) == false);
    QVERIFY(w.issuesFor(QHostAddress("172.18.2.50"), false, 0).isEmpty());
    QVERIFY(w.issuesFor(QHostAddress("172.18.2.255"), true, 0).isEmpty());
    QCOMPARE(w.nodeCount(), 0);
}

void ArtNet_Test::nodeWatchPortShortIsDebouncedBothWays()
{
    NodeDriver d;
    d.interval(true);

    // One odd reply is not a short
    d.reply.goodOutput[1] = 0x80 | ArtNetNodeWatch::GoodOutputShort;
    d.interval(true);
    d.reply.goodOutput[1] = 0x80;
    d.interval(true);
    QVERIFY(d.watch.issuesFor(d.addr, false, 1).isEmpty());
    QCOMPARE(d.changes, 0);

    // Two in a row is
    d.reply.goodOutput[1] = 0x80 | ArtNetNodeWatch::GoodOutputShort;
    const qint64 began = d.now + 20;
    d.interval(true);
    d.interval(true);
    QCOMPARE(d.changes, 1);
    QList<ArtNetNodeWatch::Issue> issues = d.watch.issuesFor(d.addr, false, 1);
    QCOMPARE(issues.count(), 1);
    QCOMPARE(int(issues.first().kind), int(ArtNetNodeWatch::Issue::PortShort));
    QCOMPARE(issues.first().port, 2);
    QCOMPARE(issues.first().portLabel, QString("0:0:1"));
    QCOMPARE(issues.first().sinceMs, began);

    // Only the port carrying the universe: port 1 (Port-Address 0) is fine
    QVERIFY(d.watch.issuesFor(d.addr, false, 0).isEmpty());

    // Merging is not a fault
    d.reply.goodOutput[2] = 0x80 | ArtNetNodeWatch::GoodOutputMerging;
    d.interval(true);
    d.interval(true);
    QVERIFY(d.watch.issuesFor(d.addr, false, 2).isEmpty());

    // Clean for less than PortClearAfterMs, then short again: still one fault
    d.reply.goodOutput[1] = 0x80;
    d.interval(true);
    d.interval(true);
    d.reply.goodOutput[1] = 0x80 | ArtNetNodeWatch::GoodOutputShort;
    d.interval(true);
    QCOMPARE(d.changes, 1);
    QCOMPARE(d.watch.issuesFor(d.addr, false, 1).count(), 1);

    // Clean for PortClearAfterMs: cleared, once
    d.reply.goodOutput[1] = 0x80;
    for (int i = 0; i < 6; i++)
        d.interval(true);
    QVERIFY(d.watch.issuesFor(d.addr, false, 1).isEmpty());
    QCOMPARE(d.changes, 2);
}

void ArtNet_Test::nodeWatchNotOutputtingOnlyOnceSeenOutputting()
{
    // A node that never sets "data is being output" is not flagged for it
    NodeDriver quiet;
    quiet.reply.goodOutput[0] = 0;
    for (int i = 0; i < 10; i++)
        quiet.interval(true);
    QVERIFY(quiet.watch.issuesFor(quiet.addr, false, 0).isEmpty());

    // One that did, then stopped, is
    NodeDriver d;
    for (int i = 0; i < 3; i++)
        d.interval(true);
    d.reply.goodOutput[0] = 0;
    d.interval(true);
    d.interval(true);
    QList<ArtNetNodeWatch::Issue> issues = d.watch.issuesFor(d.addr, false, 0);
    QCOMPARE(issues.count(), 1);
    QCOMPARE(int(issues.first().kind), int(ArtNetNodeWatch::Issue::PortNotOutputting));
    QCOMPARE(issues.first().port, 1);
}

void ArtNet_Test::nodeWatchBroadcastAndUnicastPickTheRightNodes()
{
    ArtNetNodeWatch w;
    const QHostAddress a("172.18.2.10"), b("172.18.2.11");
    const quint16 shortBit = ArtNetNodeWatch::GoodOutputShort;
    // a outputs Port-Address 0 and 1, b outputs 1 and 2; both short on 1
    ArtNetNodeInfo ra = nodeReply("A", "172.18.2.10", { 0, 1 }, { 0x80, uchar(0x80 | shortBit) });
    ArtNetNodeInfo rb = nodeReply("B", "172.18.2.11", { 1, 2 }, { uchar(0x80 | shortBit), 0x80 });
    for (int i = 0; i < 2; i++)
    {
        w.pollSent(i * kPollMs);
        w.replyHeard(a, ra, i * kPollMs + 10);
        w.replyHeard(b, rb, i * kPollMs + 10);
    }

    // Broadcast universe 1: both nodes output it, both shorts count
    QCOMPARE(w.issuesFor(QHostAddress("172.18.2.255"), true, 1).count(), 2);
    // Broadcast universe 0: only a carries it, and its port 1 is fine
    QVERIFY(w.issuesFor(QHostAddress("172.18.2.255"), true, 0).isEmpty());
    // Unicast to a, universe 1: a's short only
    QList<ArtNetNodeWatch::Issue> ia = w.issuesFor(a, false, 1);
    QCOMPARE(ia.count(), 1);
    QCOMPARE(ia.first().name, QString("A"));
    // Unicast to an address no node has: nothing, even though the
    // Port-Address is in trouble elsewhere
    QVERIFY(w.issuesFor(QHostAddress("172.18.2.99"), false, 1).isEmpty());

    // b goes quiet: broadcast universe 2 (only b carries it) shows it silent
    for (int i = 2; i < 7; i++)
    {
        w.pollSent(i * kPollMs);
        w.replyHeard(a, ra, i * kPollMs + 10);
    }
    QList<ArtNetNodeWatch::Issue> ib = w.issuesFor(QHostAddress("172.18.2.255"), true, 2);
    QCOMPARE(ib.count(), 1);
    QCOMPARE(int(ib.first().kind), int(ArtNetNodeWatch::Issue::NodeSilent));
    QCOMPARE(ib.first().ip, QString("172.18.2.11"));
}

void ArtNet_Test::nodeWatchBindIndexNumbersPortsPastFour()
{
    ArtNetNodeWatch w;
    const QHostAddress a("172.18.2.10");
    ArtNetNodeInfo first = nodeReply("Big", "172.18.2.10", { 0, 1, 2, 3 }, { 0x80, 0x80, 0x80, 0x80 }, 1);
    ArtNetNodeInfo second = nodeReply("Big", "172.18.2.10", { 4, 5, 6, 7 },
                                      { uchar(0x80 | ArtNetNodeWatch::GoodOutputShort), 0x80, 0x80, 0x80 }, 2);
    for (int i = 0; i < 2; i++)
    {
        w.pollSent(i * kPollMs);
        w.replyHeard(a, first, i * kPollMs + 10);
        w.replyHeard(a, second, i * kPollMs + 11);
    }
    QList<ArtNetNodeWatch::Issue> issues = w.issuesFor(a, false, 4);
    QCOMPARE(issues.count(), 1);
    QCOMPARE(issues.first().port, 5);
    // The first group's replies don't overwrite the second's state
    QVERIFY(w.issuesFor(a, false, 0).isEmpty());
}

/** The captured CR041R reply, with port @a port's GoodOutput set to @a good */
static QByteArray cr041rWith(int port, uchar good)
{
    QByteArray reply(cr041rPollReply, sizeof(cr041rPollReply));
    reply[182 + port] = char(good);
    return reply;
}

/** A controller on loopback with output universes aimed at @a target. Its
 *  socket is never bound or written: nothing here leaves the machine. */
static ArtNetController *testController(QObject *parent, const QHostAddress &target,
                                        QList<quint32> universes)
{
    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress::LocalHost);
    entry.setPrefixLength(8);
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    ArtNetController *ctrl = new ArtNetController(QNetworkInterface(), entry, sock, 0, parent);
    foreach (quint32 u, universes)
    {
        UniverseInfo info;
        info.type = ArtNetController::Output;
        info.outputAddress = target;
        info.outputUniverse = ushort(u);
        info.outputTransmissionMode = ArtNetController::Standard;
        ctrl->m_universeMap[u] = info;
    }
    return ctrl;
}

static QStringList texts(const QList<QPair<quint32, QString> > &problems)
{
    QStringList out;
    typedef QPair<quint32, QString> Problem;
    foreach (const Problem &p, problems)
        out << p.second;
    return out;
}

void ArtNet_Test::controllerReportsAndClearsProblems()
{
    const QHostAddress node("172.18.2.10");
    QObject owner;
    ArtNetController *ctrl = testController(&owner, node, { 1 });
    /* By name, not &ArtNetController::problemsChanged: this test links the
       artnet plugin DLL, and on Windows a member pointer into another DLL is
       an import thunk that moc's signal lookup never matches ("Null signal").
       ArtNetController isn't dllexport'ed (see qlcengine_export.h). */
    QSignalSpy changed(ctrl, SIGNAL(problemsChanged()));
    ctrl->m_testNowMs = 0;

    // Heard, answering, port 2 (Port-Address 1) outputting
    for (int i = 0; i < 3; i++)
    {
        ctrl->nodeIntervalElapsed();
        ctrl->handlePacket(cr041rWith(1, 0x80), node);
        ctrl->m_testNowMs += kPollMs;
    }
    QVERIFY(ctrl->outputProblems().isEmpty());
    QCOMPARE(changed.count(), 0);

    // A short on that port, in two replies
    for (int i = 0; i < 2; i++)
    {
        ctrl->nodeIntervalElapsed();
        ctrl->handlePacket(cr041rWith(1, 0x80 | ArtNetNodeWatch::GoodOutputShort), node);
        ctrl->m_testNowMs += kPollMs;
    }
    QCOMPARE(changed.count(), 1);
    QStringList t = texts(ctrl->outputProblems());
    QCOMPARE(t.count(), 1);
    QVERIFY2(QRegularExpression("^Art-Net node CR041R \\(172\\.18\\.2\\.10\\), port 2 \\(0:0:1\\): "
                                "DMX output short since \\d\\d:\\d\\d$").match(t.first()).hasMatch(),
             qPrintable(t.first()));

    // The node goes quiet: one problem, silence (its port report is stale)
    for (int i = 0; i < 4; i++)
    {
        ctrl->nodeIntervalElapsed();
        ctrl->m_testNowMs += kPollMs;
    }
    QCOMPARE(changed.count(), 2);
    t = texts(ctrl->outputProblems());
    QCOMPARE(t.count(), 1);
    QVERIFY2(t.first().startsWith("Art-Net node CR041R (172.18.2.10): stopped answering since "),
             qPrintable(t.first()));

    // Sends to it failing too: a second, distinct problem
    UniverseInfo &u = ctrl->m_universeMap[1];
    for (int i = 0; i < 3; i++)
    {
        ctrl->reportSendResult(u, 1, node, false);
        ctrl->m_testNowMs += 600;
    }
    t = texts(ctrl->outputProblems(1));
    QCOMPARE(t.count(), 2);
    QVERIFY2(t.first().startsWith("Art-Net to 172.18.2.10: not reachable since "), qPrintable(t.first()));
    QVERIFY(ctrl->outputProblems(0).isEmpty());   // no such universe

    // Back, healthy: answering, port clean for 10 s, sends fine for 30 s
    for (int i = 0; i < 12; i++)
    {
        ctrl->nodeIntervalElapsed();
        ctrl->handlePacket(cr041rWith(1, 0x80), node);
        ctrl->reportSendResult(u, 1, node, true);
        ctrl->m_testNowMs += kPollMs;
    }
    QVERIFY2(ctrl->outputProblems().isEmpty(), qPrintable(texts(ctrl->outputProblems()).join("; ")));
}

void ArtNet_Test::pluginGroupsAProblemsUniverses()
{
    ArtNetPlugin plugin;
    const QHostAddress node("172.18.2.10");
    ArtNetController *ctrl = testController(&plugin, node, { 0, 1, 2 });
    ctrl->m_testNowMs = 0;

    ArtNetIO io;
    io.controller = ctrl;
    plugin.m_IOmapping.clear();
    plugin.m_IOmapping.append(io);

    // Three universes to one target that can't be reached: one problem
    for (int i = 0; i < 3; i++)
    {
        for (quint32 uni = 0; uni < 3; uni++)
            ctrl->reportSendResult(ctrl->m_universeMap[uni], uni, node, false);
        ctrl->m_testNowMs += 600;
    }
    const QStringList all = plugin.targetProblems();
    QCOMPARE(all.count(), 1);
    QVERIFY2(QRegularExpression("^Art-Net to 172\\.18\\.2\\.10: not reachable since \\d\\d:\\d\\d "
                                "\\(universes 1, 2, 3\\)$").match(all.first()).hasMatch(),
             qPrintable(all.first()));

    // Per patch, for the Connections row: no universe suffix
    const QStringList one = plugin.outputProblems(0, 1);
    QCOMPARE(one.count(), 1);
    QVERIFY(one.first().startsWith("Art-Net to 172.18.2.10: not reachable since "));
    QVERIFY(plugin.outputProblems(5, 1).isEmpty());   // no such line


    plugin.m_IOmapping.clear();
}


/****************************************************************************
 * Nodes not on this computer's network, and ArtIpProg
 *
 * A node fresh out of the box sits on its factory address (2.x.x.x for
 * Art-Net, or whatever its vendor ships) and is on no network this computer
 * has an address on. It must show up as a node to configure -- not be
 * dropped, and not look like any other node -- and its IP can only be set by
 * a unicast ArtIpProg, which these tests prove is never broadcast.
 ****************************************************************************/

/** The CR041R capture re-addressed to @a ip with a different MAC */
static QByteArray pollReplyFrom(const QString &ip, uchar macLast = 0x55)
{
    QByteArray reply(cr041rPollReply, sizeof(cr041rPollReply));
    const QStringList parts = ip.split('.');
    for (int i = 0; i < 4; i++)
        reply[10 + i] = char(parts.at(i).toInt());
    reply[206] = char(macLast);
    return reply;
}

/** A real IPv4 interface that can broadcast, or an invalid one */
static QNetworkInterface broadcastInterface()
{
    foreach (const QNetworkInterface &iface, QNetworkInterface::allInterfaces())
    {
        const QNetworkInterface::InterfaceFlags need =
                QNetworkInterface::IsUp | QNetworkInterface::CanBroadcast;
        if ((iface.flags() & need) != need || iface.flags() & QNetworkInterface::IsLoopBack)
            continue;
        foreach (const QNetworkAddressEntry &e, iface.addressEntries())
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol)
                return iface;
    }
    return QNetworkInterface();
}

/** One line on "the show network", 172.18.2.219/24, on a real interface
 *  (QNetworkInterface::index() cannot be faked). Every controller sends into
 *  @a sink. Returns false when the machine has no usable interface. */
static bool showNetLine(ArtNetPlugin &plugin, const QSharedPointer<QUdpSocket> &sock,
                        QList<QNetworkDatagram> *sink, QNetworkInterface *ifaceOut = NULL)
{
    const QNetworkInterface iface = broadcastInterface();
    if (iface.isValid() == false)
        return false;

    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress("172.18.2.219"));
    entry.setPrefixLength(24);
    entry.setBroadcast(QHostAddress("172.18.2.255"));

    ArtNetIO io;
    io.iface = iface;
    io.address = entry;
    io.controller = new ArtNetController(iface, entry, sock, 0, &plugin);
    io.controller->m_testSink = sink;
    // By name: see problemsChanged above (Windows, plugin DLL).
    QObject::connect(io.controller, SIGNAL(ipProgReplied(QString,ArtNetIpProgReply)),
                     &plugin, SIGNAL(ipProgReplied(QString,ArtNetIpProgReply)));
    plugin.m_IOmapping.clear();
    plugin.m_IOmapping.append(io);
    if (ifaceOut != NULL)
        *ifaceOut = iface;
    return true;
}

void ArtNet_Test::ipProgQueryChangesNothing()
{
    ArtNetPacketizer ap;
    QByteArray data;
    ap.setupArtNetIpProg(data, ArtNetIpProgRequest());

    QCOMPARE(data.size(), 34);
    QCOMPARE(data.left(8), QByteArray("Art-Net\0", 8));
    QCOMPARE(uchar(data.at(8)), uchar(0x00));    // OpCode 0xf800, little-endian
    QCOMPARE(uchar(data.at(9)), uchar(0xf8));
    QCOMPARE(uchar(data.at(10)), uchar(0x00));   // ProtVer 14
    QCOMPARE(uchar(data.at(11)), uchar(14));
    // Command 0: bit 7 clear, so a query -- the node answers and changes nothing
    QCOMPARE(uchar(data.at(14)), uchar(0x00));
    QCOMPARE(data.mid(15), QByteArray(19, '\0'));
    QVERIFY(ArtNetIpProgRequest().programs() == false);
}

void ArtNet_Test::ipProgPacketLayout()
{
    ArtNetPacketizer ap;
    QByteArray data;

    ArtNetIpProgRequest r;
    r.setIp = true;
    r.ip = QHostAddress("172.18.2.60");
    r.setMask = true;
    r.mask = QHostAddress("255.255.255.0");
    ap.setupArtNetIpProg(data, r);
    QCOMPARE(data.size(), 34);
    QCOMPARE(uchar(data.at(14)), uchar(0x80 | 0x04 | 0x02));   // enable + IP + mask
    QCOMPARE(data.mid(16, 4), QByteArray("\xac\x12\x02\x3c", 4));   // ProgIp, big-endian
    QCOMPARE(data.mid(20, 4), QByteArray("\xff\xff\xff\x00", 4));   // ProgSm
    QCOMPARE(data.mid(24, 2), QByteArray(2, '\0'));                  // ProgPort: never
    QCOMPARE(data.mid(26, 4), QByteArray(4, '\0'));                  // ProgDg: not asked

    r.setGateway = true;
    r.gateway = QHostAddress("172.18.2.1");
    ap.setupArtNetIpProg(data, r);
    QCOMPARE(uchar(data.at(14)), uchar(0x80 | 0x10 | 0x04 | 0x02));
    QCOMPARE(data.mid(26, 4), QByteArray("\xac\x12\x02\x01", 4));

    // DHCP: bit 6, and the node ignores the rest -- so send none of it
    ArtNetIpProgRequest dhcp;
    dhcp.enableDhcp = true;
    ap.setupArtNetIpProg(data, dhcp);
    QCOMPARE(uchar(data.at(14)), uchar(0xc0));
    QCOMPARE(data.mid(16, 14), QByteArray(14, '\0'));

    ArtNetIpProgRequest reset;
    reset.resetToDefaults = true;
    ap.setupArtNetIpProg(data, reset);
    QCOMPARE(uchar(data.at(14)), uchar(0x88));
}

void ArtNet_Test::ipProgReplyParse()
{
    ArtNetPacketizer ap;
    QByteArray data("Art-Net\0", 8);
    data.append(char(0x00)).append(char(0xf9));      // OpIpProgReply
    data.append(char(0)).append(char(14));
    data.append(QByteArray(4, '\0'));                 // Filler1-4
    data.append(QByteArray("\x02\x00\x00\x05", 4));   // ProgIp
    data.append(QByteArray("\xff\x00\x00\x00", 4));   // ProgSm
    data.append(QByteArray(2, '\0'));                 // ProgPort
    data.append(char(0x40));                          // Status: DHCP enabled
    data.append(char(0));                             // Spare2
    data.append(QByteArray("\x02\x00\x00\x01", 4));   // ProgDg
    data.append(QByteArray(2, '\0'));
    QCOMPARE(data.size(), 34);

    quint16 op = 0;
    QVERIFY(ap.checkPacketAndCode(data, op));
    QCOMPARE(op, quint16(ARTNET_IPREPLY));

    ArtNetIpProgReply reply;
    QVERIFY(ap.fillArtIpProgReply(data, reply));
    QCOMPARE(reply.ip, QHostAddress("2.0.0.5"));
    QCOMPARE(reply.mask, QHostAddress("255.0.0.0"));
    QCOMPARE(reply.dhcpEnabled, true);
    QCOMPARE(reply.hasGateway, true);
    QCOMPARE(reply.gateway, QHostAddress("2.0.0.1"));

    // An Art-Net 3 node stops before the gateway
    ArtNetIpProgReply short3;
    QVERIFY(ap.fillArtIpProgReply(data.left(28), short3));
    QCOMPARE(short3.hasGateway, false);
    QVERIFY(ap.fillArtIpProgReply(data.left(20), short3) == false);
}

void ArtNet_Test::pollReplyDhcpBits()
{
    ArtNetPacketizer ap;
    ArtNetNodeInfo info;
    QByteArray reply(cr041rPollReply, sizeof(cr041rPollReply));

    /* The CR041R's Status2 is 0x08 (15-bit port addresses): static, and not
       DHCP capable -- what DMX-Workshop says of it. */
    QVERIFY(ap.fillArtPollReplyInfo(reply, info));
    QCOMPARE(info.dhcpConfigured, false);
    QCOMPARE(info.dhcpCapable, false);

    reply[212] = char(0x0c);    // capable, static
    QVERIFY(ap.fillArtPollReplyInfo(reply, info));
    QCOMPARE(info.dhcpConfigured, false);
    QCOMPARE(info.dhcpCapable, true);

    reply[212] = char(0x0e);    // capable, and its address came from DHCP
    QVERIFY(ap.fillArtPollReplyInfo(reply, info));
    QCOMPARE(info.dhcpConfigured, true);
    QCOMPARE(info.dhcpCapable, true);
}

void ArtNet_Test::offSubnetReplyIsListedAsNeedingItsIp()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    QNetworkInterface iface;
    if (showNetLine(plugin, sock, &sink, &iface) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");

    /* A new node still on its factory 2.x address answers on the show
       network's cable. It arrived on our interface, so it is ours -- listed,
       not dropped -- but nothing can be patched to it until its IP is set. */
    plugin.handlePacket(pollReplyFrom("2.0.0.5"), QHostAddress("2.0.0.5"),
                        uint(iface.index()));

    const QList<QLCIOPlugin::Device> devs = plugin.discoveredDevices();
    QCOMPARE(devs.count(), 1);
    QCOMPARE(devs.first().address, QString("2.0.0.5"));
    QCOMPARE(devs.first().line, quint32(0));
    QVERIFY2(devs.first().attention.contains("factory"), qPrintable(devs.first().attention));
    QVERIFY(devs.first().attention.contains("set its IP"));
    QVERIFY(plugin.canConfigureDevice("2.0.0.5"));
    QVERIFY(plugin.canConfigureDevice("2.0.0.99") == false);

    // The other ways a node ends up without a usable address
    QVERIFY(ArtNetPlugin::attentionFor(QHostAddress("169.254.10.20"), false).contains("DHCP"));
    QVERIFY(ArtNetPlugin::attentionFor(QHostAddress("192.168.0.50"), false)
                .contains("not on this computer's network"));
    QVERIFY(ArtNetPlugin::attentionFor(QHostAddress("192.168.0.50"), true).isEmpty());

    QVERIFY(sink.isEmpty());   // hearing a node sends nothing
    plugin.m_IOmapping.clear();
}

void ArtNet_Test::sameSubnetNodeIsNotFlagged()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    QNetworkInterface iface;
    if (showNetLine(plugin, sock, &sink, &iface) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");

    plugin.handlePacket(pollReplyFrom("172.18.2.10"), QHostAddress("172.18.2.10"),
                        uint(iface.index()));
    const QList<QLCIOPlugin::Device> devs = plugin.discoveredDevices();
    QCOMPARE(devs.count(), 1);
    QVERIFY(devs.first().attention.isEmpty());
    QVERIFY(plugin.canConfigureDevice("172.18.2.10"));
    plugin.m_IOmapping.clear();
}

void ArtNet_Test::ipProgIsNeverSentToAnUnreachableNode()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    QNetworkInterface iface;
    if (showNetLine(plugin, sock, &sink, &iface) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");

    plugin.handlePacket(pollReplyFrom("2.0.0.5"), QHostAddress("2.0.0.5"),
                        uint(iface.index()));

    /* ArtIpProg names no node; unicast is the only safe way to send it, and
       no line here can unicast to 2.0.0.5 without a router. Refuse -- never
       fall back to a broadcast, which would re-address every node. */
    ArtNetIpProgRequest r;
    r.setIp = true;
    r.ip = QHostAddress("172.18.2.60");
    QString why;
    QVERIFY(plugin.sendIpProg("2.0.0.5", r, &why) == false);
    QVERIFY(why.contains("2.0.0.5"));
    QVERIFY(plugin.sendIpProg("2.0.0.5", ArtNetIpProgRequest()) == false);
    QVERIFY(sink.isEmpty());
    plugin.m_IOmapping.clear();
}

void ArtNet_Test::ipProgIsUnicastToAReachableNode()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    if (showNetLine(plugin, sock, &sink) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");

    ArtNetIpProgRequest r;
    r.setIp = true;
    r.ip = QHostAddress("172.18.2.60");
    r.setMask = true;
    r.mask = QHostAddress("255.255.255.0");
    QVERIFY(plugin.sendIpProg("172.18.2.10", r));
    QCOMPARE(sink.count(), 1);
    QCOMPARE(sink.first().destinationAddress(), QHostAddress("172.18.2.10"));
    QCOMPARE(sink.first().destinationPort(), ARTNET_PORT);
    QCOMPARE(sink.first().data().size(), 34);
    QCOMPARE(uchar(sink.first().data().at(14)), uchar(0x86));
    plugin.m_IOmapping.clear();
}

void ArtNet_Test::ipProgReplyReachesThePlugin()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    QNetworkInterface iface;
    if (showNetLine(plugin, sock, &sink, &iface) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");

    QByteArray data("Art-Net\0", 8);
    data.append(char(0x00)).append(char(0xf9)).append(char(0)).append(char(14));
    data.append(QByteArray(4, '\0'));
    data.append(QByteArray("\xac\x12\x02\x3c", 4));
    data.append(QByteArray("\xff\xff\xff\x00", 4));
    data.append(QByteArray(10, '\0'));          // ProgPort, Status, Spare, ProgDg, Spares
    QCOMPARE(data.size(), 34);

    QSignalSpy spy(&plugin, SIGNAL(ipProgReplied(QString,ArtNetIpProgReply)));
    plugin.handlePacket(data, QHostAddress("172.18.2.60"), uint(iface.index()));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("172.18.2.60"));
    plugin.m_IOmapping.clear();
}

void ArtNet_Test::discoveryPollIsLimitedBroadcastOnTheLine()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    QNetworkInterface iface;
    if (showNetLine(plugin, sock, &sink, &iface) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");

    QVERIFY(plugin.m_IOmapping.at(0).controller->sendDiscoveryPoll());
    QCOMPARE(sink.count(), 1);
    const QNetworkDatagram &dg = sink.first();
    QCOMPARE(dg.destinationAddress(), QHostAddress(QHostAddress::Broadcast));
    QCOMPARE(dg.destinationPort(), ARTNET_PORT);
    // Out of THIS line's interface, from its address -- not the default route
    QCOMPARE(dg.interfaceIndex(), uint(iface.index()));
    QCOMPARE(dg.senderAddress(), QHostAddress("172.18.2.219"));
    QByteArray poll;
    ArtNetPacketizer().setupArtNetPoll(poll);
    QCOMPARE(dg.data(), poll);

    // A loopback line has no cable to broadcast on
    QNetworkAddressEntry lo;
    lo.setIp(QHostAddress::LocalHost);
    lo.setPrefixLength(8);
    ArtNetController loCtrl(QNetworkInterface::interfaceFromName("lo0").isValid()
                                ? QNetworkInterface::interfaceFromName("lo0")
                                : QNetworkInterface::interfaceFromName("lo"),
                            lo, sock, 1);
    loCtrl.m_testSink = &sink;
    QVERIFY(loCtrl.sendDiscoveryPoll() == false);
    QCOMPARE(sink.count(), 1);
    plugin.m_IOmapping.clear();
}

void ArtNet_Test::rescanSendsOneDiscoveryPollPerInterface()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    QNetworkInterface iface;
    if (showNetLine(plugin, sock, &sink, &iface) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");

    // A second address on the same interface (an alias): a line of its own
    QNetworkAddressEntry alias;
    alias.setIp(QHostAddress("2.255.255.250"));
    alias.setPrefixLength(8);
    alias.setBroadcast(QHostAddress("2.255.255.255"));
    ArtNetIO io;
    io.iface = iface;
    io.address = alias;
    io.controller = new ArtNetController(iface, alias, sock, 1, &plugin);
    io.controller->m_testSink = &sink;
    plugin.m_IOmapping.append(io);

    plugin.rescan();

    // One subnet poll per line, one limited broadcast per interface
    int subnet = 0, limited = 0;
    foreach (const QNetworkDatagram &dg, sink)
    {
        if (dg.destinationAddress() == QHostAddress(QHostAddress::Broadcast))
            limited++;
        else
            subnet++;
    }
    QCOMPARE(subnet, 2);
    QCOMPARE(limited, 1);
    QVERIFY(sink.at(0).destinationAddress() == QHostAddress("172.18.2.255"));
    plugin.m_IOmapping.clear();
}

void ArtNet_Test::periodicPollIsUnchanged()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    if (showNetLine(plugin, sock, &sink) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");

    /* The 3 s poll on a line with an output: still exactly one datagram, to
       the line's own broadcast address. The limited broadcast is for Rescan
       only. */
    QVERIFY(QMetaObject::invokeMethod(plugin.m_IOmapping.at(0).controller, "slotPollTick"));
    QCOMPARE(sink.count(), 1);
    QCOMPARE(sink.first().destinationAddress(), QHostAddress("172.18.2.255"));
    QCOMPARE(sink.first().interfaceIndex(), uint(0));
    plugin.m_IOmapping.clear();
}

void ArtNet_Test::nodeSetupHelpers()
{
    QCOMPARE(ArtNetNodeSetup::guessMask(QHostAddress("2.0.0.5")), QHostAddress("255.0.0.0"));
    QCOMPARE(ArtNetNodeSetup::guessMask(QHostAddress("10.1.2.3")), QHostAddress("255.0.0.0"));
    QCOMPARE(ArtNetNodeSetup::guessMask(QHostAddress("169.254.3.4")), QHostAddress("255.255.0.0"));
    QCOMPARE(ArtNetNodeSetup::guessMask(QHostAddress("192.168.0.50")), QHostAddress("255.255.255.0"));

    QCOMPARE(ArtNetNodeSetup::suggestAlias(QHostAddress("2.0.0.5"), QHostAddress("255.0.0.0")),
             QHostAddress("2.255.255.250"));
    QCOMPARE(ArtNetNodeSetup::suggestAlias(QHostAddress("192.168.0.50"), QHostAddress("255.255.255.0")),
             QHostAddress("192.168.0.250"));
    // Never the node's own address
    QCOMPARE(ArtNetNodeSetup::suggestAlias(QHostAddress("192.168.0.250"), QHostAddress("255.255.255.0")),
             QHostAddress("192.168.0.249"));

    const QString add = ArtNetNodeSetup::aliasCommand("vlan0", QHostAddress("2.255.255.250"),
                                                      QHostAddress("255.0.0.0"), true);
    const QString del = ArtNetNodeSetup::aliasCommand("vlan0", QHostAddress("2.255.255.250"),
                                                      QHostAddress("255.0.0.0"), false);
    QVERIFY(add.contains("vlan0") && add.contains("2.255.255.250"));
    QVERIFY(del.contains("vlan0") && del.contains("2.255.255.250"));
#if defined(Q_OS_MACOS)
    QCOMPARE(add, QString("sudo ifconfig vlan0 alias 2.255.255.250 netmask 255.0.0.0"));
    QCOMPARE(del, QString("sudo ifconfig vlan0 -alias 2.255.255.250"));
#elif !defined(Q_OS_WIN)
    QCOMPARE(add, QString("sudo ip addr add 2.255.255.250/8 dev vlan0"));
    QCOMPARE(del, QString("sudo ip addr del 2.255.255.250/8 dev vlan0"));
#endif
}

void ArtNet_Test::nodeSetupOffersTheAliasForAnUnreachableNode()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    QNetworkInterface iface;
    if (showNetLine(plugin, sock, &sink, &iface) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");
    plugin.handlePacket(pollReplyFrom("2.0.0.5"), QHostAddress("2.0.0.5"),
                        uint(iface.index()));

    ArtNetNodeSetup dlg(&plugin, "2.0.0.5");
    bool asked = false;
    dlg.m_confirm = [&asked](const QString &) { asked = true; return true; };

    QCOMPARE(dlg.reachable(), false);
    QVERIFY(dlg.findChild<QPushButton *>("apply")->isEnabled() == false);
    QVERIFY(dlg.findChild<QPushButton *>("query")->isEnabled() == false);
    QVERIFY(dlg.findChild<QGroupBox *>("aliasBox")->isVisibleTo(&dlg));
    const QString add = dlg.findChild<QLineEdit *>("aliasAdd")->text();
    QVERIFY2(add.contains("2.255.255.250"), qPrintable(add));
    QVERIFY2(add.contains(iface.name()) || add.contains(iface.humanReadableName()), qPrintable(add));
    QVERIFY(dlg.findChild<QLabel *>("reach")->text().contains("Not on this network"));
    QVERIFY(dlg.findChild<QLabel *>("header")->text().contains("02:4D:48:12:02:55"));

    // Even called directly, it neither asks nor sends
    QVERIFY(dlg.apply() == false);
    QVERIFY(dlg.query() == false);
    QVERIFY(asked == false);
    QVERIFY(sink.isEmpty());
    plugin.m_IOmapping.clear();
}

void ArtNet_Test::nodeSetupValidatesAndConfirmsBeforeSending()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    QNetworkInterface iface;
    if (showNetLine(plugin, sock, &sink, &iface) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");
    plugin.handlePacket(pollReplyFrom("172.18.2.10"), QHostAddress("172.18.2.10"),
                        uint(iface.index()));
    plugin.handlePacket(pollReplyFrom("172.18.2.11", 0x66), QHostAddress("172.18.2.11"),
                        uint(iface.index()));

    ArtNetNodeSetup dlg(&plugin, "172.18.2.10");
    QString confirmText;
    bool answer = false;
    dlg.m_confirm = [&](const QString &text) { confirmText = text; return answer; };

    QVERIFY(dlg.reachable());
    QVERIFY(dlg.findChild<QGroupBox *>("aliasBox")->isVisibleTo(&dlg) == false);
    QLineEdit *ip = dlg.findChild<QLineEdit *>("ip");
    QLineEdit *mask = dlg.findChild<QLineEdit *>("mask");
    QCOMPARE(mask->text(), QString("255.255.255.0"));   // the line's own mask

    // What is refused before anything is asked
    ip->setText("172.18.2");
    QVERIFY(dlg.validate(NULL).isEmpty() == false);
    ip->setText("172.18.2.219");                         // this computer
    QVERIFY(dlg.validate(NULL).contains("own address"));
    ip->setText("172.18.2.11");                          // another node
    QVERIFY(dlg.validate(NULL).contains("another node"));
    ip->setText("172.18.2.255");
    QVERIFY(dlg.validate(NULL).contains("broadcast"));
    ip->setText("172.18.2.60");
    mask->setText("255.0.255.0");
    QVERIFY(dlg.validate(NULL).contains("mask"));
    mask->setText("255.255.255.0");
    QVERIFY(dlg.validate(NULL).isEmpty());

    // Declined: nothing sent
    confirmText.clear();
    answer = false;
    QVERIFY(dlg.apply() == false);
    QVERIFY(confirmText.contains("172.18.2.10"));
    QVERIFY(confirmText.contains("172.18.2.60 / 255.255.255.0"));
    QVERIFY(confirmText.contains("02:4D:48:12:02:55"));
    QVERIFY(sink.isEmpty());

    // Confirmed: one unicast ArtIpProg to the node's current address
    answer = true;
    QVERIFY(dlg.apply());
    QCOMPARE(sink.count(), 1);
    QCOMPARE(sink.first().destinationAddress(), QHostAddress("172.18.2.10"));
    QCOMPARE(uchar(sink.first().data().at(14)), uchar(0x86));
    QCOMPARE(sink.first().data().mid(16, 4), QByteArray("\xac\x12\x02\x3c", 4));

    // A query asks without confirming, and programs nothing
    sink.clear();
    QVERIFY(dlg.query());
    QCOMPARE(sink.count(), 1);
    QCOMPARE(uchar(sink.first().data().at(14)), uchar(0x00));
    plugin.m_IOmapping.clear();
}

void ArtNet_Test::nodeSetupShowsTheNodesAnswer()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    QNetworkInterface iface;
    if (showNetLine(plugin, sock, &sink, &iface) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");
    plugin.handlePacket(pollReplyFrom("172.18.2.10"), QHostAddress("172.18.2.10"),
                        uint(iface.index()));

    ArtNetNodeSetup dlg(&plugin, "172.18.2.10");
    dlg.m_confirm = [](const QString &) { return true; };
    dlg.findChild<QLineEdit *>("ip")->setText("172.18.2.60");
    QVERIFY(dlg.apply());

    // The node answers from its new address
    ArtNetIpProgReply reply;
    reply.ip = QHostAddress("172.18.2.60");
    reply.mask = QHostAddress("255.255.255.0");
    emit plugin.ipProgReplied("172.18.2.60", reply);
    const QString status = dlg.findChild<QLabel *>("status")->text();
    QVERIFY2(status.contains("172.18.2.60 / 255.255.255.0"), qPrintable(status));
    QVERIFY(status.contains("DHCP off"));
    QVERIFY(status.contains("moved from 172.18.2.10"));

    // An unrelated node's answer is not this one's
    reply.ip = QHostAddress("172.18.2.99");
    emit plugin.ipProgReplied("172.18.2.99", reply);
    QVERIFY(dlg.findChild<QLabel *>("status")->text() == status);
    plugin.m_IOmapping.clear();
}

void ArtNet_Test::rigNetworkGateHoldsEverySend()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    if (showNetLine(plugin, sock, &sink) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");
    ArtNetController *ctrl = plugin.m_IOmapping.at(0).controller;

    // The laptop at school: the show's rig network is somewhere else.
    plugin.setTransmitNetworks(QStringList() << "10.4.0.0/16");
    QVERIFY(plugin.transmitRestricted());
    QVERIFY(ctrl->transmitHeld());

    // Opening an output polls at once -- not from here
    ctrl->addUniverse(0, ArtNetController::Output);

    ctrl->sendDmx(0, QByteArray(512, char(255)), true);
    QMetaObject::invokeMethod(ctrl, "slotSendAllUniverses");
    QVERIFY(QMetaObject::invokeMethod(ctrl, "slotPollTick"));
    QVERIFY(ctrl->sendDiscoveryPoll() == false);
    QVERIFY(ctrl->sendIpProg(QHostAddress("172.18.2.60"), ArtNetIpProgRequest()) == false);

    // A node on that network polling us: no ArtPollReply goes back.
    QByteArray poll;
    ArtNetPacketizer().setupArtNetPoll(poll);
    ctrl->handlePacket(poll, QHostAddress("172.18.2.60"));

    QVERIFY2(sink.isEmpty(), qPrintable(QString("%1 datagram(s) left a held line").arg(sink.count())));
    QCOMPARE(ctrl->getPacketSentNumber(), quint64(0));
    // Nothing failed, either: held is not a send error
    QVERIFY(ctrl->outputProblems().isEmpty());

    // Back on the rig network: it resumes with the very next frame.
    plugin.setTransmitNetworks(QStringList() << "172.18.0.0/20");
    QVERIFY(ctrl->transmitHeld() == false);
    ctrl->sendDmx(0, QByteArray(512, char(255)), true);
    QCOMPARE(sink.count(), 1);
    QCOMPARE(sink.first().destinationAddress(), QHostAddress("172.18.2.255"));

    // No networks at all: as it always was
    plugin.setTransmitNetworks(QStringList());
    QVERIFY(plugin.transmitRestricted() == false);
    QVERIFY(ctrl->transmitHeld() == false);
    plugin.m_IOmapping.clear();
}

void ArtNet_Test::rigNetworkGateAllowsRigAndLoopback()
{
    ArtNetPlugin plugin;
    plugin.setTransmitNetworks(QStringList() << "172.18.2.7/20" << "bogus" << "fe80::/64"
                                             << "10.0.0.0/33" << "172.18.0.0/20");
    // Normalised, de-duplicated, the junk ignored
    QCOMPARE(plugin.transmitNetworks(), QStringList() << "172.18.0.0/20");
    QVERIFY(plugin.transmitAllowedFrom(QString("172.18.2.219")));
    QVERIFY(plugin.transmitAllowedFrom(QString("172.18.15.255")));
    QVERIFY(plugin.transmitAllowedFrom(QString("172.18.16.1")) == false);
    QVERIFY(plugin.transmitAllowedFrom(QString("10.4.12.7")) == false);
    QVERIFY(plugin.transmitAllowedFrom(QString("127.0.0.1")));
    QVERIFY(plugin.transmitAllowedFrom(QString("127.4.5.6")));
    // Not IPv4: never on a rig network, and no crash
    QVERIFY(plugin.transmitAllowedFrom(QString("fe80::1")) == false);
    QVERIFY(plugin.transmitAllowedFrom(QString("wled.local")) == false);
    QVERIFY(plugin.transmitAllowedFrom(QString()) == false);

    // A loopback line is never held
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    QNetworkAddressEntry lo;
    lo.setIp(QHostAddress::LocalHost);
    lo.setPrefixLength(8);
    ArtNetController loCtrl(QNetworkInterface::interfaceFromName("lo0").isValid()
                                ? QNetworkInterface::interfaceFromName("lo0")
                                : QNetworkInterface::interfaceFromName("lo"),
                            lo, sock, 1, &plugin);
    loCtrl.m_testSink = &sink;
    QVERIFY(loCtrl.transmitHeld() == false);
    loCtrl.addUniverse(0, ArtNetController::Output);
    sink.clear();
    loCtrl.sendDmx(0, QByteArray(512, char(1)), true);
    QCOMPARE(sink.count(), 1);
    QCOMPARE(sink.first().destinationAddress(), QHostAddress(QHostAddress::LocalHost));
}

void ArtNet_Test::rigNetworkGateDecisionIsCachedOffTheSendPath()
{
    ArtNetPlugin plugin;
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    QList<QNetworkDatagram> sink;
    if (showNetLine(plugin, sock, &sink) == false)
        QSKIP("needs a broadcast-capable IPv4 interface");
    ArtNetController *ctrl = plugin.m_IOmapping.at(0).controller;
    ctrl->addUniverse(0, ArtNetController::Output);
    sink.clear();

    // setTransmitNetworks() updates the line's cached flag by itself
    QVERIFY(ctrl->m_held.load() == false);
    plugin.setTransmitNetworks(QStringList() << "10.4.0.0/16");
    QVERIFY(ctrl->m_held.load());
    plugin.setTransmitNetworks(QStringList() << "172.18.0.0/20");
    QVERIFY(ctrl->m_held.load() == false);

    // The engine's reconnect tick asks again even with the list unchanged
    // (a setTransmitNetworks() with the same list is no change): a stale
    // flag is put right there.
    QVERIFY(plugin.setTransmitNetworks(QStringList() << "172.18.0.0/20") == false);
    ctrl->m_held.store(true);
    plugin.refreshTransmitGate();
    QVERIFY(ctrl->m_held.load() == false);

    /* The send path takes no gate lock and walks no list: with the plugin's
       gate mutex held elsewhere (as by a setTransmitNetworks() in progress),
       a send from another thread -- the universe thread's side -- still
       completes. Taking the lock would deadlock it: the wait times out and
       the lock is let go so the test fails instead of hanging. */
    auto sendFromAnotherThread = [&]() {
        std::promise<void> done;
        std::future<void> finished = done.get_future();
        plugin.m_transmitMutex.lock();
        std::thread sender([&]() {
            for (int i = 0; i < 1000; i++)
            {
                ctrl->transmitHeld();
                ctrl->sendDmx(0, QByteArray(512, char(i & 0xFF)), true);
            }
            done.set_value();
        });
        const bool inTime = finished.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
        plugin.m_transmitMutex.unlock();
        sender.join();
        return inTime;
    };

    sink.clear();
    QVERIFY2(sendFromAnotherThread(), "an allowed send waited for the gate lock");
    QCOMPARE(sink.count(), 1000);

    plugin.setTransmitNetworks(QStringList() << "10.4.0.0/16");
    sink.clear();
    QVERIFY2(sendFromAnotherThread(), "a held send waited for the gate lock");
    QVERIFY(sink.isEmpty());

    plugin.setTransmitNetworks(QStringList());
    plugin.m_IOmapping.clear();
}

/****************************************************************************
 * The RDM flag, held steady
 ****************************************************************************/

/* Status1 bytes as the office CR041R (172.18.2.10) sent them. Recorded
   2026-10-26 (47 replies to 40 polls, interleaved with other controllers'):
   its Status1 is an uncleared buffer, never the same for long. */
static const uchar kCr041rRecorded[] = {
    0x00, 0x00, 0x00, 0x74, 0x74, 0x74, 0x74, 0x74, 0x00, 0x74, 0x74, 0x74,
    0x74, 0x00, 0x00, 0x74, 0x74, 0x74, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00,
    0x00, 0x74, 0x74, 0x00, 0x00, 0xAD, 0xAD, 0x74, 0x00, 0x74, 0x74, 0x74,
    0x00, 0x00, 0x74, 0x74, 0x74, 0x00, 0x00, 0x00, 0x74, 0x74, 0x74 };

/* The same node's earlier 40-poll run (TODO.md, 2026-10-26): 0x6F x11,
   0x74 x16, 0x00 x11, 0xA9 x3, in runs as it sends them. 0x6F has the RDM
   bit (bit 1) set: about a quarter of the replies claim RDM. */
static const uchar kCr041rWithRdmBit[] = {
    0x00, 0x00, 0x00, 0x6F, 0x6F, 0x6F, 0x74, 0x74, 0x74, 0x74, 0x00, 0x6F,
    0x74, 0x74, 0x00, 0x00, 0x6F, 0x6F, 0xA9, 0x74, 0x74, 0x74, 0x00, 0x6F,
    0x6F, 0x6F, 0x6F, 0x6F, 0x74, 0x74, 0x00, 0x00, 0xA9, 0x74, 0x74, 0x74,
    0x74, 0x00, 0x00, 0xA9, 0x74 };

void ArtNet_Test::rdmFlagCr041rStaysOff()
{
    {
        ArtNetRdmFlag flag;
        for (uchar st : kCr041rRecorded)
        {
            QVERIFY(!flag.replyHeard(st));
            QVERIFY(!flag.rdmCapable());
        }
        QVERIFY(flag.statusIsJunk());
    }

    ArtNetRdmFlag flag;
    int claimed = 0;
    for (uchar st : kCr041rWithRdmBit)
    {
        if (st & 0x02)
            claimed++;
        QVERIFY(!flag.replyHeard(st));      // what is shown never changes
        QVERIFY(!flag.rdmCapable());
    }
    QCOMPARE(claimed, 11);                  // read straight, it flickered
    QVERIFY(flag.statusIsJunk());

    // And again and again: steady.
    for (int pass = 0; pass < 20; pass++)
    {
        for (uchar st : kCr041rWithRdmBit)
        {
            flag.replyHeard(st);
            QVERIFY(!flag.rdmCapable());
        }
    }
}

/* OLA's Status1 is 0xD2, every reply: RDM once it has held for
   StableReplies replies, and it stays. A node without RDM (0xF0, the
   rig's other node) stays off. */
void ArtNet_Test::rdmFlagAnRdmNodeTurnsOnAndHolds()
{
    ArtNetRdmFlag ola;
    for (int i = 1; i <= 30; i++)
    {
        const bool changed = ola.replyHeard(0xD2);
        QCOMPARE(ola.rdmCapable(), i >= ArtNetRdmFlag::StableReplies);
        QCOMPARE(changed, i == ArtNetRdmFlag::StableReplies);
    }
    QVERIFY(!ola.statusIsJunk());

    ArtNetRdmFlag plain;
    for (int i = 0; i < 30; i++)
    {
        QVERIFY(!plain.replyHeard(0xF0));
        QVERIFY(!plain.rdmCapable());
    }
}

/* A node that really turns RDM off (and on again) is followed, once the
   new Status1 has held: one change is a change, not junk. */
void ArtNet_Test::rdmFlagARealChangeIsFollowed()
{
    ArtNetRdmFlag flag;
    for (int i = 0; i < 10; i++)
        flag.replyHeard(0xD2);
    QVERIFY(flag.rdmCapable());

    for (int i = 1; i <= 10; i++)
    {
        flag.replyHeard(0xD0);
        QCOMPARE(flag.rdmCapable(), i < ArtNetRdmFlag::StableReplies);
    }
    for (int i = 1; i <= 10; i++)
    {
        flag.replyHeard(0xD2);
        QCOMPARE(flag.rdmCapable(), i >= ArtNetRdmFlag::StableReplies);
    }
    QVERIFY(!flag.statusIsJunk());

    // Short blips (a reply or two) change nothing.
    flag.replyHeard(0xD0);
    QVERIFY(flag.rdmCapable());
    for (int i = 0; i < 20; i++)
        flag.replyHeard(0xD2);
    QVERIFY(flag.rdmCapable());
}

/* Once a node's Status1 has shown itself to be junk, a long run of the
   RDM bit (eleven 0x6F in a row) is not believed; only a long calm makes
   it trusted again. */
void ArtNet_Test::rdmFlagJunkStaysJunkThroughALongRun()
{
    ArtNetRdmFlag flag;
    for (uchar st : kCr041rWithRdmBit)
        flag.replyHeard(st);
    QVERIFY(flag.statusIsJunk());
    for (int i = 0; i < 11; i++)
    {
        flag.replyHeard(0x6F);
        QVERIFY(!flag.rdmCapable());
    }
    for (int i = 11; i < ArtNetRdmFlag::CalmReplies - 1; i++)
    {
        flag.replyHeard(0x6F);
        QVERIFY(!flag.rdmCapable());
    }
    // A minute and more of the same byte: it means it.
    flag.replyHeard(0x6F);
    QVERIFY(!flag.statusIsJunk());
    QVERIFY(flag.rdmCapable());
}

/* A table of devices from the node is proof, whatever Status1 says. */
void ArtNet_Test::rdmFlagTableOfDevicesIsProof()
{
    ArtNetRdmFlag flag;
    for (uchar st : kCr041rRecorded)
        flag.replyHeard(st);
    QVERIFY(!flag.rdmCapable());
    QVERIFY(flag.todHeard());
    QVERIFY(flag.rdmCapable());
    QVERIFY(!flag.todHeard());              // already known
    for (uchar st : kCr041rWithRdmBit)
    {
        flag.replyHeard(st);
        QVERIFY(flag.rdmCapable());
    }
}

/* Through the controller: the node list (what the Devices row reads)
   says "no RDM" for the CR041R at every reply, RDM for OLA, and RDM for
   the CR041R once it answers with a table of devices. */
void ArtNet_Test::rdmFlagReachesTheNodesList()
{
    QObject owner;
    const QHostAddress cr041r("172.18.2.10");
    const QHostAddress ola("172.18.2.218");
    ArtNetController *ctrl = testController(&owner, cr041r, { 0 });

    for (uchar st : kCr041rWithRdmBit)
    {
        QByteArray reply(cr041rPollReply, sizeof(cr041rPollReply));
        reply[23] = char(st);
        QVERIFY(ctrl->handlePacket(reply, cr041r));
        QVERIFY(ctrl->getNodesList().contains(cr041r));
        QVERIFY(!ctrl->getNodesList().value(cr041r).rdmCapable);
        QCOMPARE(int(ctrl->getNodesList().value(cr041r).status1), int(st));
    }

    for (int i = 1; i <= 8; i++)
    {
        QByteArray reply(cr041rPollReply, sizeof(cr041rPollReply));
        reply[23] = char(0xD2);
        QVERIFY(ctrl->handlePacket(reply, ola));
        QCOMPARE(ctrl->getNodesList().value(ola).rdmCapable,
                 i >= ArtNetRdmFlag::StableReplies);
    }

    /* ArtTodData (OpCode 0x8100), no UIDs: the node answered RDM
       discovery, so it speaks RDM. */
    QByteArray tod(28, char(0));
    tod.replace(0, 8, QByteArray("Art-Net\0", 8));
    tod[8] = char(0x00);
    tod[9] = char(0x81);
    tod[11] = char(14);
    QVERIFY(ctrl->handlePacket(tod, cr041r));
    QVERIFY(ctrl->getNodesList().value(cr041r).rdmCapable);
    QByteArray reply(cr041rPollReply, sizeof(cr041rPollReply));
    reply[23] = char(0x00);
    QVERIFY(ctrl->handlePacket(reply, cr041r));
    QVERIFY(ctrl->getNodesList().value(cr041r).rdmCapable);
}

/*****************************************************************************
 * Malformed packets (fuzz, matrix 2)
 *****************************************************************************/

static QByteArray artHeader(quint16 opCode, int size)
{
    QByteArray b(size, char(0));
    b.replace(0, 8, QByteArray("Art-Net\0", 8));
    b[8] = char(opCode & 0xff);
    b[9] = char(opCode >> 8);
    b[11] = char(14);
    return b;
}

/* An ArtRdm datagram carrying an RDM GET_COMMAND_RESPONSE for pid with
   payload, checksummed as parsePacket checks it (no start code on the
   wire, so 0xCC is counted in). */
static QByteArray artRdmReply(quint16 pid, const QByteArray &payload)
{
    QByteArray rdm;
    rdm.append(char(RDM_SC_SUB_MESSAGE));
    rdm.append(char(24 + payload.size()));                      // message length
    rdm.append(QByteArray::fromHex("7a7000000001"));            // destination UID
    rdm.append(QByteArray::fromHex("4d4812345678"));            // source UID
    rdm.append(char(0x07));                                     // transaction
    rdm.append(char(RESPONSE_TYPE_ACK));
    rdm.append(char(0x00));                                     // message count
    rdm.append(char(0x00)).append(char(0x00));                  // sub-device
    rdm.append(char(GET_COMMAND_RESPONSE));
    rdm.append(char(pid >> 8)).append(char(pid & 0xff));
    rdm.append(char(payload.size()));                           // PDL
    rdm.append(payload);
    quint16 cs = RDM_START_CODE;
    for (char c : rdm)
        cs += uchar(c);
    rdm.append(char(cs >> 8)).append(char(cs & 0xff));

    QByteArray b = artHeader(ARTNET_RDM, 24);
    b[12] = char(0x01);                                         // RDM version
    b[21] = char(0x00);                                         // Net
    b[23] = char(0x03);                                         // Address
    return b + rdm;
}

/* Dispatched the way ArtNetController::handlePacket dispatches a datagram;
   returns what the parser returned. */
static bool parseAsTheControllerWould(const QByteArray &b)
{
    ArtNetPacketizer p;
    quint16 code = 0;
    if (!p.checkPacketAndCode(b, code))
        return false;
    quint32 universe = 0;
    QVariantMap values;
    switch (code)
    {
        case ARTNET_POLLREPLY: { ArtNetNodeInfo info; return p.fillArtPollReplyInfo(b, info); }
        case ARTNET_DMX: { QByteArray dmx; return p.fillDMXdata(b, dmx, universe); }
        case ARTNET_TODDATA: return p.processTODdata(b, universe, values);
        case ARTNET_RDM: return p.processRDMdata(b, universe, values);
        case ARTNET_IPREPLY: { ArtNetIpProgReply reply; return p.fillArtIpProgReply(b, reply); }
        default: return false;
    }
}

void ArtNet_Test::artDmxShorterThanItsHeaderIsDropped()
{
    /* The fuzzer's known/artdmx-12-bytes.bin: a bare header. */
    const QByteArray known = QByteArray::fromHex("4172742d4e6574000050000e");
    ArtNetPacketizer p;
    QByteArray dmx("stale");
    quint32 universe = 77;
    QVERIFY(!p.fillDMXdata(known, dmx, universe));
    QVERIFY(dmx.isEmpty());

    QByteArray full;
    p.setupArtNetDmx(full, 1, QByteArray(4, char(0x11)));
    for (int n = 0; n < 18; n++)
        QVERIFY2(!p.fillDMXdata(full.left(n), dmx, universe), qPrintable(QString::number(n)));
    QVERIFY(p.fillDMXdata(full.left(18), dmx, universe));      // header only: no channels
    QVERIFY(dmx.isEmpty());
    QVERIFY(p.fillDMXdata(full, dmx, universe));
    QCOMPARE(dmx, QByteArray(4, char(0x11)));
    QCOMPARE(universe, quint32(1));
}

void ArtNet_Test::artDmxIsClampedToAUniverse()
{
    ArtNetPacketizer p;
    QByteArray dmx;
    quint32 universe = 0;

    /* A Length of 0xffff over 600 bytes: a universe is 512 channels, and
       the receivers index a 512-byte buffer with the result. */
    QByteArray big = artHeader(ARTNET_DMX, 18) + QByteArray(600, char(0x42));
    big[16] = char(0xff);
    big[17] = char(0xff);
    QVERIFY(p.fillDMXdata(big, dmx, universe));
    QCOMPARE(dmx.size(), 512);

    /* Less than it claims: what is there, no more. */
    QByteArray shortOne = artHeader(ARTNET_DMX, 18) + QByteArray(10, char(0x01));
    shortOne[16] = char(0x02);
    shortOne[17] = char(0x00);
    QVERIFY(p.fillDMXdata(shortOne, dmx, universe));
    QCOMPARE(dmx.size(), 10);

    /* SubUni and Net with the top bit set were sign-extended (and a left
       shift of a negative value, UBSan). Port-Address 0x7f80 is valid. */
    QByteArray high;
    p.setupArtNetDmx(high, 0, QByteArray(2, char(0)));
    high[14] = char(0x80);
    high[15] = char(0x7f);
    QVERIFY(p.fillDMXdata(high, dmx, universe));
    QCOMPARE(universe, quint32(0x7f80));
}

void ArtNet_Test::artTodDataPromisingMoreUidsIsDropped()
{
    /* The fuzzer's known/arttoddata-uidcount-255.bin: 28 bytes, UidCount 255. */
    const QByteArray known = QByteArray::fromHex(
        "4172742d4e6574000081000e010000000000000000000000000000ff");
    QCOMPARE(known.size(), 28);
    ArtNetPacketizer p;
    quint32 universe = 0;
    QVariantMap values;
    QVERIFY(!p.processTODdata(known, universe, values));
    QVERIFY(values.isEmpty());

    /* One UID short of what it promises. */
    QByteArray tod = artHeader(ARTNET_TODDATA, 28) + QByteArray::fromHex("4d4812345678");
    tod[27] = char(2);
    QVERIFY(!p.processTODdata(tod, universe, values));
    QVERIFY(values.isEmpty());
}

void ArtNet_Test::artTodDataStillParses()
{
    QByteArray tod = artHeader(ARTNET_TODDATA, 28)
                     + QByteArray::fromHex("4d4812345678" "7a70deadbeef");
    tod[21] = char(0x01);           // Net
    tod[23] = char(0x93);           // Address, top bit set
    tod[27] = char(2);
    ArtNetPacketizer p;
    quint32 universe = 0;
    QVariantMap values;
    QVERIFY(p.processTODdata(tod, universe, values));
    QCOMPARE(universe, quint32(0x0193));
    QCOMPARE(values.value("DISCOVERY_COUNT").toInt(), 2);
    QCOMPARE(values.value("UID-0").toString(), QString("4D4812345678"));
    QCOMPARE(values.value("UID-1").toString(), QString("7A70DEADBEEF"));

    /* An empty table is a table. */
    QByteArray empty = artHeader(ARTNET_TODDATA, 28);
    values.clear();
    QVERIFY(p.processTODdata(empty, universe, values));
    QCOMPARE(values.value("DISCOVERY_COUNT").toInt(), 0);
}

void ArtNet_Test::rdmValidRepliesStillParse()
{
    ArtNetPacketizer p;
    quint32 universe = 0;
    QVariantMap values;

    /* DEVICE_INFO, 19 bytes (E1.20 table A-3). */
    const QByteArray info = QByteArray::fromHex(
        "0100" "1234" "0101" "00010203" "0010" "02" "05" "0021" "0000" "03");
    QCOMPARE(info.size(), 19);
    QVERIFY(p.processRDMdata(artRdmReply(PID_DEVICE_INFO, info), universe, values));
    QCOMPARE(universe, quint32(0x0003));
    QCOMPARE(values.value("UID_INFO").toString(), QString("4D4812345678"));
    QCOMPARE(values.value("Response").toString(), QString("ACK"));
    QCOMPARE(values.value("PID").toInt(), int(PID_DEVICE_INFO));
    QCOMPARE(values.value("Device model ID").toInt(), 0x1234);
    QCOMPARE(values.value("TYPE").toString(), QString("Fixture Fixed"));
    QCOMPARE(values.value("Software version").toUInt(), 0x00010203u);
    QCOMPARE(values.value("DMX_CHANNELS").toInt(), 16);
    QCOMPARE(values.value("PERSONALITY_INDEX").toInt(), 2);
    QCOMPARE(values.value("PERSONALITY_COUNT").toInt(), 5);
    QCOMPARE(values.value("DMX_START_ADDRESS").toInt(), 0x21);
    QCOMPARE(values.value("Number of sensors").toInt(), 3);  // was the checksum's low byte
    QCOMPARE(values.value("RAW_DATA").toByteArray(), info.toHex(','));

    /* SUPPORTED_PARAMETERS: a list of PIDs. */
    values.clear();
    QVERIFY(p.processRDMdata(artRdmReply(PID_SUPPORTED_PARAMETERS,
                                         QByteArray::fromHex("00e000e10120")), universe, values));
    QCOMPARE(values.value("PID_LIST").value<QVector<quint16>>(),
             QVector<quint16>({ 0x00e0, 0x00e1, 0x0120 }));

    /* DMX_PERSONALITY_DESCRIPTION: index, footprint, text. */
    values.clear();
    QVERIFY(p.processRDMdata(artRdmReply(PID_DMX_PERSONALITY_DESCRIPTION,
                                         QByteArray::fromHex("02000c") + "8ch RGBW"), universe, values));
    QCOMPARE(values.value("PERS_INDEX").toInt(), 2);
    QCOMPARE(values.value("PERS_CHANNELS").toInt(), 12);
    QCOMPARE(values.value("PERS_DESC").toString(), QString("8ch RGBW"));

    /* A bad checksum is still refused. */
    QByteArray bad = artRdmReply(PID_DMX_START_ADDRESS, QByteArray::fromHex("0001"));
    bad[bad.size() - 1] = char(bad.at(bad.size() - 1) ^ 0x01);
    values.clear();
    QVERIFY(!p.processRDMdata(bad, universe, values));

    /* Through the serial (start-code) path, as DMX USB parses it. */
    QByteArray serial = artRdmReply(PID_DMX_START_ADDRESS, QByteArray::fromHex("0001")).mid(24);
    serial.prepend(char(RDM_START_CODE));
    RDMProtocol rdm;
    values.clear();
    QVERIFY(rdm.parsePacket(serial, values));
    QCOMPARE(values.value("DMX_START_ADDRESS").toInt(), 1);
}

void ArtNet_Test::rdmEveryTruncationIsDropped()
{
    /* Every prefix of a valid reply, for each PID the parser decodes, and
       the same replies with a PDL larger than what follows: refused, never
       read past (a Debug build asserts in QByteArray::at). */
    const QList<QPair<quint16, QByteArray>> replies = {
        { PID_DEVICE_INFO, QByteArray(19, char(0x01)) },
        { PID_SUPPORTED_PARAMETERS, QByteArray::fromHex("00e000e1") },
        { PID_PARAMETER_DESCRIPTION, QByteArray(24, char(0x41)) },
        { PID_DMX_PERSONALITY, QByteArray::fromHex("0103") },
        { PID_DMX_PERSONALITY_DESCRIPTION, QByteArray::fromHex("010004") + "abc" },
        { PID_DMX_START_ADDRESS, QByteArray::fromHex("0001") },
        { PID_SLOT_INFO, QByteArray::fromHex("00000000010001000002") },
        { PID_SLOT_DESCRIPTION, QByteArray::fromHex("0000") + "Dim" },
        { PID_MANUFACTURER_LABEL, QByteArray("Maker") },
        { PID_DEVICE_MODEL_DESCRIPTION, QByteArray("Model") },
    };
    ArtNetPacketizer p;
    quint32 universe = 0;
    for (const auto &r : replies)
    {
        const QByteArray full = artRdmReply(r.first, r.second);
        QVariantMap values;
        QVERIFY2(p.processRDMdata(full, universe, values), qPrintable(QString::number(r.first, 16)));
        for (int n = 0; n < full.size(); n++)
        {
            values.clear();
            QVERIFY2(!p.processRDMdata(full.left(n), universe, values),
                     qPrintable(QString("pid %1, %2 bytes").arg(r.first, 0, 16).arg(n)));
        }
        for (int pdl = r.second.size() + 1; pdl < 256; pdl++)
        {
            QByteArray lying = full;
            lying[24 + 22] = char(pdl);                     // PDL byte
            values.clear();
            QVERIFY(!p.processRDMdata(lying, universe, values));
        }
    }

    /* A PDL too short for what the PID decodes is skipped, not read past:
       the packet is still well formed and checksummed. */
    for (quint16 pid : { quint16(PID_DEVICE_INFO), quint16(PID_DMX_PERSONALITY),
                         quint16(PID_DMX_PERSONALITY_DESCRIPTION), quint16(PID_DMX_START_ADDRESS),
                         quint16(PID_SLOT_DESCRIPTION), quint16(PID_SLOT_INFO),
                         quint16(PID_SUPPORTED_PARAMETERS) })
    {
        QVariantMap values;
        QVERIFY(p.processRDMdata(artRdmReply(pid, QByteArray(1, char(0x05))), universe, values));
        QVERIFY(!values.contains("DMX_START_ADDRESS"));
        QVERIFY(!values.contains("PERS_CURRENT"));
        QVERIFY(!values.contains("PERS_DESC"));
        QVERIFY(!values.contains("SLOT_ID"));
        QVERIFY(!values.contains("Number of sensors"));
        if (values.contains("PID_LIST"))
            QVERIFY(values.value("PID_LIST").value<QVector<quint16>>().isEmpty());
        if (values.contains("SLOT_LIST"))
            QVERIFY(values.value("SLOT_LIST").value<QVector<quint16>>().isEmpty());
    }

    /* A UID from too few bytes is no UID. */
    quint16 esta = 1;
    quint32 device = 1;
    QVERIFY(RDMProtocol::byteArrayToUID(QByteArray::fromHex("4d4812"), esta, device).isEmpty());
    QCOMPARE(esta, quint16(0));
    QCOMPARE(device, quint32(0));
}

void ArtNet_Test::rdmFuzzCrashesAreDroppedQuietly()
{
    /* The smallest crashing input per signature from the fuzzer's ArtRdm
       run (dink ~/matrix2/fuzz/out/artnet-rdm-early), named by the line
       it crashed at before the fix. */
    static const char *const crashes[] = {
        /* artnetpacketizer.cpp:468 */ "4172742d4e6574000083010100ffff514c432b000081000e012d",
        /* rdmprotocol.cpp:284 */ "4172742d4e6574000083100000010101010101010101410101",
        /* rdmprotocol.cpp:301 */ "4172742d4e657400008310000001010101010101010101010101010101010101010101007ce7",
        /* rdmprotocol.cpp:304 */ "4172742d4e65740000830000212121212100002d4e657a000101010101011c0101010101010141",
        /* rdmprotocol.cpp:311 */ "4172742d4e657400008310000001010101010101010101010101010101014172742d4e65d6000101",
        /* rdmprotocol.cpp:318 */ "4172742d4e6574000083000e0100000000000000000000010174000081000e0901415f000000000035",
        /* rdmprotocol.cpp:326 */ "4172742d4e657400008310000001010101010101010101010101010101010101010101014172010101417274",
        /* rdmprotocol.cpp:374 */ "4172742d4e657400008310000001010101010101010101010101010101010101010101012e01014172742d4e006001",
        /* rdmprotocol.cpp:375 */ "4172742d4e657400008310002401010101010101010101010101010101010101010101012e01014172742d4e0060010101010172742d4e6574000101",
        /* rdmprotocol.cpp:378 */ "4172742d4e657400008310000001000083010101010101010101006021210001000083010101010101010101006021210000000000000021210101007ce7",
        /* rdmprotocol.cpp:401 */ "4172742d4e657400008310000001000083010101010101010101006021210001000082fe0101010101010101005121010101007ce7",
        /* rdmprotocol.cpp:498 */ "4172742d4e6574000083100000010101010101014e6574000101",
        /* rdmprotocol.cpp:501 */ "4172742d4e657400008310004172742d4e6574000101000001000000",
        /* rdmprotocol.cpp:502 */ "4172742d4e657400008310000001000083100800050101010141720101",
        /* rdmprotocol.cpp:503 */ "4172742d4e65740000836574000018000060000027002100010121212121",
        /* rdmprotocol.cpp:504 */ "4172742d4e6574000083000021010101010101010101010101012101010050",
        /* rdmprotocol.cpp:568 */ "4172742d4e6574000083000e01000000000000000000000101150123456789ab7ff8010907750901010000003104aa",
    };
    for (const char *hex : crashes)
    {
        const QByteArray b = QByteArray::fromHex(hex);
        QVERIFY2(!parseAsTheControllerWould(b), hex);
        /* And every prefix of it. */
        for (int n = 0; n < b.size(); n++)
            QVERIFY(!parseAsTheControllerWould(b.left(n)));
    }
}

void ArtNet_Test::malformedPacketsThroughTheController()
{
    QObject owner;
    const QHostAddress node("172.18.2.10");
    ArtNetController *ctrl = testController(&owner, node, { 0 });

    const QList<QByteArray> packets = {
        QByteArray::fromHex("4172742d4e6574000050000e"),
        QByteArray::fromHex("4172742d4e6574000081000e010000000000000000000000000000ff"),
        QByteArray::fromHex("4172742d4e6574000083100000010101010101010101410101"),
        artHeader(ARTNET_POLLREPLY, 100),
        artHeader(ARTNET_IPREPLY, 20),
    };
    /* handlePacket answers true for a packet it does not recognise; what
       matters is that each one is dealt with, not read past. */
    const quint64 before = ctrl->getPacketReceivedNumber();
    for (const QByteArray &b : packets)
        for (int n = 0; n <= b.size(); n++)
            ctrl->handlePacket(b.left(n), node);
    QCOMPARE(ctrl->getPacketReceivedNumber(), before);
    QVERIFY(ctrl->getNodesList().isEmpty());
}

/*****************************************************************************
 * The controller's send and receive paths, byte for byte (coverage campaign,
 * from the matrix 2 mutation survivors: each test pins what a surviving
 * mutant changed without any test noticing). Every send goes to a test
 * sink; nothing leaves the machine.
 *****************************************************************************/

/** A controller on "the show network", 172.18.2.219/24, with no universes,
 *  sending into @a sink. No real interface and no rig network gate (the
 *  parent is not a plugin). */
static ArtNetController *sinkController(QObject *parent, QList<QNetworkDatagram> *sink)
{
    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress("172.18.2.219"));
    entry.setPrefixLength(24);
    entry.setBroadcast(QHostAddress("172.18.2.255"));
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    ArtNetController *ctrl = new ArtNetController(QNetworkInterface(), entry, sock, 0, parent);
    ctrl->m_testSink = sink;
    return ctrl;
}

static int artOpCode(const QByteArray &d)
{
    return (uchar(d.at(9)) << 8) | uchar(d.at(8));
}

static int artDmxPortAddress(const QByteArray &d)
{
    return (uchar(d.at(15)) << 8) | uchar(d.at(14));
}

static int artDmxLength(const QByteArray &d)
{
    return (uchar(d.at(16)) << 8) | uchar(d.at(17));
}

static const char kArtPollBytes[] = "4172742d4e6574000020000e0200";

void ArtNet_Test::addUniverseMapsItsOwnUniverse()
{
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    QCOMPARE(int(ctrl->type()), int(ArtNetController::Unknown));

    ctrl->addUniverse(5, ArtNetController::Output);
    UniverseInfo *info = ctrl->getUniverseInfo(5);
    QVERIFY(info != NULL);
    QCOMPARE(int(info->outputUniverse), 5);
    QCOMPARE(int(info->inputUniverse), 5);
    QCOMPARE(info->outputAddress, QHostAddress("172.18.2.255"));
    QCOMPARE(info->outputTransmissionMode, int(ArtNetController::Standard));
    QCOMPARE(int(ctrl->type()), int(ArtNetController::Output));

    sink.clear();
    ctrl->sendDmx(5, QByteArray(512, char(1)), true);
    QCOMPARE(sink.count(), 1);
    QCOMPARE(sink.last().destinationAddress(), QHostAddress("172.18.2.255"));
    QCOMPARE(sink.last().destinationPort(), ARTNET_PORT);
    const QByteArray d = sink.last().data();
    QCOMPARE(artOpCode(d), ARTNET_DMX);
    QCOMPARE(artDmxPortAddress(d), 5);
    QCOMPARE(d.mid(18), QByteArray(512, char(1)));

    // A universe that isn't ours sends nothing
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("sendDmx: universe 9 not registered"));
    ctrl->sendDmx(9, QByteArray(512, char(1)), true);
    QCOMPARE(sink.count(), 1);

    // Input on a universe of its own, then on the output one
    ctrl->addUniverse(6, ArtNetController::Input);
    QCOMPARE(int(ctrl->getUniverseInfo(6)->inputUniverse), 6);
    QCOMPARE(ctrl->getUniverseInfo(6)->type, int(ArtNetController::Input));
    ctrl->addUniverse(5, ArtNetController::Input);
    QCOMPARE(ctrl->getUniverseInfo(5)->type, int(ArtNetController::Input | ArtNetController::Output));
    QCOMPARE(int(ctrl->type()), int(ArtNetController::Input | ArtNetController::Output));
    QCOMPARE(ctrl->universesList(), QList<quint32>({ 5, 6 }));
    QVERIFY(ctrl->getUniverseInfo(7) == NULL);

    // Removing one direction keeps the other; removing the last drops it
    ctrl->removeUniverse(5, ArtNetController::Input);
    QCOMPARE(ctrl->getUniverseInfo(5)->type, int(ArtNetController::Output));
    ctrl->removeUniverse(6, ArtNetController::Input);
    QVERIFY(ctrl->getUniverseInfo(6) == NULL);
    QCOMPARE(ctrl->universesList(), QList<quint32>({ 5 }));
}

void ArtNet_Test::transmissionModes()
{
    typedef ArtNetController AC;
    foreach (AC::TransmissionMode m, QList<AC::TransmissionMode>({ AC::Standard, AC::Full, AC::Partial }))
        QCOMPARE(AC::stringToTransmissionMode(AC::transmissionModeToString(m)), m);
    QCOMPARE(AC::transmissionModeToString(AC::Full), QString("Full"));
    QCOMPARE(AC::transmissionModeToString(AC::Partial), QString("Partial"));
    QCOMPARE(AC::transmissionModeToString(AC::Standard), QString("Standard"));
    QCOMPARE(AC::stringToTransmissionMode("bogus"), AC::Standard);

    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    ctrl->addUniverse(0, AC::Output);
    ctrl->addUniverse(1, AC::Output);
    ctrl->addUniverse(2, AC::Output);

    /* The setters say whether the value is the default, which the plugin
       stores as "no parameter": a Full or Partial universe must not read
       as default, or its mode is dropped from the saved workspace. */
    QCOMPARE(ctrl->setTransmissionMode(1, AC::Full), false);
    QCOMPARE(ctrl->setTransmissionMode(2, AC::Partial), false);
    QCOMPARE(ctrl->setTransmissionMode(0, AC::Standard), true);
    QCOMPARE(ctrl->setTransmissionMode(99, AC::Full), false);
    QCOMPARE(ctrl->setOutputUniverse(99, 1), false);
    QCOMPARE(ctrl->setOutputUniverse(0, 0), true);
    QCOMPARE(ctrl->setInputUniverse(99, 1), false);
    QCOMPARE(ctrl->setInputUniverse(0, 0), true);
    QCOMPARE(ctrl->setInputUniverse(0, 4), false);
    QCOMPARE(ctrl->setOutputIPAddress(99, "172.18.2.10"), false);

    const QByteArray some(24, char(0x33));

    // Standard: the first frame goes out as a whole universe, tail zeroed;
    // an unchanged one is left to the refresh timer
    sink.clear();
    ctrl->sendDmx(0, some, true);
    QCOMPARE(sink.count(), 1);
    QByteArray d = sink.last().data();
    QCOMPARE(artDmxLength(d), 512);
    QCOMPARE(d.size(), 18 + 512);
    QCOMPARE(d.mid(18, 24), some);
    QCOMPARE(d.mid(18 + 24), QByteArray(512 - 24, char(0)));
    ctrl->sendDmx(0, some, false);
    QCOMPARE(sink.count(), 1);

    // Full: 512 channels every frame, changed or not
    ctrl->sendDmx(1, some, false);
    ctrl->sendDmx(1, some, false);
    QCOMPARE(sink.count(), 3);
    d = sink.last().data();
    QCOMPARE(artDmxPortAddress(d), 1);
    QCOMPARE(artDmxLength(d), 512);
    QCOMPARE(d.mid(18, 24), some);

    // Partial: only the channels given (padded to an even count)
    ctrl->sendDmx(2, some, true);
    QCOMPARE(sink.count(), 4);
    d = sink.last().data();
    QCOMPARE(artDmxPortAddress(d), 2);
    QCOMPARE(artDmxLength(d), 24);
    QCOMPARE(d.size(), 18 + 24);
    ctrl->sendDmx(2, some.left(23), true);
    d = sink.last().data();
    QCOMPARE(artDmxLength(d), 24);
    QCOMPARE(d.size(), 18 + 24);
}

void ArtNet_Test::standardModeRefreshes()
{
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    ctrl->addUniverse(0, ArtNetController::Output);
    QVERIFY(ctrl->m_sendTimer.isActive());
    QCOMPARE(ctrl->m_sendTimer.interval(), 2000);

    ctrl->addUniverse(1, ArtNetController::Output);
    ctrl->setOutputUniverse(1, 7);
    ctrl->addUniverse(2, ArtNetController::Output);
    ctrl->setTransmissionMode(2, ArtNetController::Full);
    ctrl->addUniverse(3, ArtNetController::Input);

    // Both Standard universes, from an all-zero universe until set
    ctrl->sendDmx(0, QByteArray(3, char(0x7f)), true);
    sink.clear();
    QVERIFY(QMetaObject::invokeMethod(&ctrl->m_sendTimer, "timeout", Qt::DirectConnection));
    QCOMPARE(sink.count(), 2);
    QCOMPARE(artDmxPortAddress(sink.at(0).data()), 0);
    QCOMPARE(sink.at(0).data().mid(18, 4), QByteArray::fromHex("7f7f7f00"));
    QCOMPARE(artDmxPortAddress(sink.at(1).data()), 7);
    QCOMPARE(sink.at(1).data().mid(18), QByteArray(512, char(0)));
    QCOMPARE(sink.at(1).destinationAddress(), QHostAddress("172.18.2.255"));

    // A refresh that fails is announced once, when the universe turns failing
    QSignalSpy changed(ctrl, SIGNAL(problemsChanged()));
    ctrl->m_testNowMs = 0;
    ctrl->m_testSinkFails = true;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("universe 1 output to .172\\.18\\.2\\.255. is failing"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("universe 2 output to .172\\.18\\.2\\.255. is failing"));
    for (int i = 0; i < 4; i++)
    {
        QVERIFY(QMetaObject::invokeMethod(ctrl, "slotSendAllUniverses", Qt::DirectConnection));
        ctrl->m_testNowMs += 600;
    }
    QCOMPARE(changed.count(), 1);
    QCOMPARE(texts(ctrl->outputProblems()).count(), 2);
}

void ArtNet_Test::dmxSequenceRunsOneTo255PerUniverse()
{
    /* Sequence 0 means "not sequenced": a receiver that reorders by it
       would see every packet from 1 to 255 and back to 1, never 0. */
    ArtNetPacketizer ap;
    QByteArray d;
    for (int u : { 0, 5, 0x105 })
    {
        for (int i = 1; i <= 255; i++)
        {
            ap.setupArtNetDmx(d, u, QByteArray(2, char(0)));
            QVERIFY2(uchar(d.at(12)) == i, qPrintable(QString("universe %1 packet %2 had %3")
                                                       .arg(u).arg(i).arg(uchar(d.at(12)))));
        }
        ap.setupArtNetDmx(d, u, QByteArray(2, char(0)));
        QCOMPARE(int(uchar(d.at(12))), 1);
    }
    // Each universe counts on its own
    ap.setupArtNetDmx(d, 1, QByteArray(2, char(0)));
    QCOMPARE(int(uchar(d.at(12))), 1);
    ap.setupArtNetDmx(d, 0, QByteArray(2, char(0)));
    QCOMPARE(int(uchar(d.at(12))), 2);
}

void ArtNet_Test::artDmxInputEmitsChangedChannels()
{
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    ctrl->addUniverse(0, ArtNetController::Input);
    QCOMPARE(ctrl->setInputUniverse(0, 3), false);
    ctrl->addUniverse(1, ArtNetController::Output);   // output only: not an input
    ctrl->setInputUniverse(1, 3);
    QSignalSpy values(ctrl, SIGNAL(valueChanged(quint32,quint32,quint32,uchar)));

    QByteArray dmx(512, char(0));
    dmx[0] = char(1);
    dmx[10] = char(0x80);
    dmx[100] = char(0xff);
    dmx[511] = char(42);
    ArtNetPacketizer ap;
    QByteArray pkt;
    ap.setupArtNetDmx(pkt, 3, dmx);

    const quint64 before = ctrl->getPacketReceivedNumber();
    QVERIFY(ctrl->handlePacket(pkt, QHostAddress("172.18.2.10")));
    QCOMPARE(values.count(), 4);
    const QList<QPair<int, int> > expect = { { 0, 1 }, { 10, 0x80 }, { 100, 0xff }, { 511, 42 } };
    for (int i = 0; i < 4; i++)
    {
        QCOMPARE(values.at(i).at(0).toUInt(), 0u);                  // QLC+ universe
        QCOMPARE(values.at(i).at(1).toUInt(), 0u);                  // line
        QCOMPARE(int(values.at(i).at(2).toUInt()), expect.at(i).first);
        QCOMPARE(int(values.at(i).at(3).value<uchar>()), expect.at(i).second);
    }
    QCOMPARE(ctrl->getPacketReceivedNumber(), before + 1);

    // Nothing changed: nothing said
    QVERIFY(ctrl->handlePacket(pkt, QHostAddress("172.18.2.10")));
    QCOMPARE(values.count(), 4);

    // One channel back to 0
    dmx[10] = char(0);
    ap.setupArtNetDmx(pkt, 3, dmx);
    ctrl->handlePacket(pkt, QHostAddress("172.18.2.10"));
    QCOMPARE(values.count(), 5);
    QCOMPARE(int(values.last().at(2).toUInt()), 10);
    QCOMPARE(int(values.last().at(3).value<uchar>()), 0);

    // Another Art-Net universe isn't ours
    ap.setupArtNetDmx(pkt, 4, QByteArray(512, char(9)));
    ctrl->handlePacket(pkt, QHostAddress("172.18.2.10"));
    QCOMPARE(values.count(), 5);
    QCOMPARE(ctrl->getPacketReceivedNumber(), before + 3);
}

void ArtNet_Test::pollTimerFollowsOutputs()
{
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);

    // An input doesn't poll
    ctrl->addUniverse(9, ArtNetController::Input);
    QVERIFY(!ctrl->m_pollTimer.isActive());
    QVERIFY(sink.isEmpty());

    // The first output polls at once and every 3 s
    ctrl->addUniverse(0, ArtNetController::Output);
    QVERIFY(ctrl->m_pollTimer.isActive());
    QCOMPARE(ctrl->m_pollTimer.interval(), 3000);
    QCOMPARE(sink.count(), 1);
    QCOMPARE(sink.first().data(), QByteArray::fromHex(kArtPollBytes));
    QCOMPARE(sink.first().destinationAddress(), QHostAddress("172.18.2.255"));

    // A second output: no second poll, and one poll per tick
    ctrl->addUniverse(1, ArtNetController::Output);
    QCOMPARE(sink.count(), 1);
    QVERIFY(QMetaObject::invokeMethod(&ctrl->m_pollTimer, "timeout", Qt::DirectConnection));
    QCOMPARE(sink.count(), 2);
    QCOMPARE(sink.last().data(), QByteArray::fromHex(kArtPollBytes));

    // Polling goes on while any output is left
    ctrl->removeUniverse(0, ArtNetController::Output);
    QVERIFY(ctrl->m_pollTimer.isActive());
    QCOMPARE(int(ctrl->type()), int(ArtNetController::Input | ArtNetController::Output));
    ctrl->removeUniverse(9, ArtNetController::Input);
    QVERIFY(ctrl->m_pollTimer.isActive());
    QCOMPARE(int(ctrl->type()), int(ArtNetController::Output));

    // ...and stops with the last one
    ctrl->removeUniverse(1, ArtNetController::Output);
    QVERIFY(!ctrl->m_pollTimer.isActive());
    QCOMPARE(int(ctrl->type()), int(ArtNetController::Unknown));
    sink.clear();
    QVERIFY(QMetaObject::invokeMethod(&ctrl->m_pollTimer, "timeout", Qt::DirectConnection));
    QVERIFY(sink.isEmpty());

    // Opened again: connected once, not twice
    ctrl->addUniverse(1, ArtNetController::Output);
    QCOMPARE(sink.count(), 1);
    QVERIFY(QMetaObject::invokeMethod(&ctrl->m_pollTimer, "timeout", Qt::DirectConnection));
    QCOMPARE(sink.count(), 2);
}

void ArtNet_Test::pollBytes()
{
    QByteArray poll;
    ArtNetPacketizer().setupArtNetPoll(poll);
    QCOMPARE(poll, QByteArray::fromHex(kArtPollBytes));

    quint16 op = 0;
    QVERIFY(ArtNetPacketizer().checkPacketAndCode(poll, op));
    QCOMPARE(int(op), ARTNET_POLL);
    // A bare 12-byte header is a header
    QVERIFY(ArtNetPacketizer().checkPacketAndCode(poll.left(12), op));
    QVERIFY(!ArtNetPacketizer().checkPacketAndCode(poll.left(11), op));
    QByteArray notOurs = poll;
    notOurs[7] = 'x';
    QVERIFY(!ArtNetPacketizer().checkPacketAndCode(notOurs, op));
    QVERIFY(!ArtNetPacketizer().checkPacketAndCode(QByteArray("Art-Nyt") + poll.mid(7), op));
}

void ArtNet_Test::pollReplyLayout()
{
    ArtNetPacketizer ap;
    QByteArray reply;
    ap.setupArtNetPollReply(reply, QHostAddress("172.18.2.219"), "02:4D:48:12:02:0A", 0x23, false);
    QCOMPARE(reply.size(), 239);
    QCOMPARE(reply.left(10), QByteArray::fromHex("4172742d4e6574000021"));
    QCOMPARE(int(uchar(reply.at(14))), 0x36);     // port 6454, little end first
    QCOMPARE(int(uchar(reply.at(15))), 0x19);

    ArtNetNodeInfo info;
    QVERIFY(ap.fillArtPollReplyInfo(reply, info));
    QCOMPARE(info.shortName, QString("QLC+"));
    QCOMPARE(info.longName, QString("Q Light Controller Plus - ArtNet interface"));
    QCOMPARE(info.ipAddress, QString("172.18.2.219"));
    QCOMPARE(info.macAddress, QString("02:4D:48:12:02:0A"));
    QCOMPARE(int(info.firmwareVersion), 0x0420);
    QCOMPARE(int(info.netSwitch), 0);
    QCOMPARE(int(info.subSwitch), 2);
    QCOMPARE(int(info.status1), 0xF0);
    QCOMPARE(int(info.oemCode), 0xFFFF);
    QCOMPARE(int(info.estaCode), 0xFFFF);
    QCOMPARE(info.portsNumber, 1);
    QCOMPARE(int(info.portTypes[0]), 0x80);
    QCOMPARE(int(info.goodOutput[0]), 0x80);
    QCOMPARE(int(info.goodInput[0]), 0);
    QCOMPARE(int(info.swIn[0]), 0);
    // The Port-Address the reply announces: Net, Sub-Net, Universe
    QCOMPARE((info.netSwitch << 8) | (info.subSwitch << 4) | (info.swOut[0] & 0x0f), 0x23);
    for (int i = 1; i < 4; i++)
    {
        QCOMPARE(int(info.portTypes[i]), 0);
        QCOMPARE(int(info.goodOutput[i]), 0);
        QCOMPARE(int(info.swOut[i]), 0);
    }
    QCOMPARE(info.bindIpAddress, QString("0.0.0.0"));
    QCOMPARE(int(info.bindIndex), 0);
    QCOMPARE(int(info.status2), 0);

    // An input port
    ap.setupArtNetPollReply(reply, QHostAddress("172.18.2.219"), "02:4D:48:12:02:0A", 0x23, true);
    QCOMPARE(reply.size(), 239);
    QVERIFY(ap.fillArtPollReplyInfo(reply, info));
    QCOMPARE(int(info.portTypes[0]), 0x40);
    QCOMPARE(int(info.goodInput[0]), 0x80);
    QCOMPARE(int(info.goodOutput[0]), 0);
    QCOMPARE(int(info.swIn[0] & 0x0f), 3);
    QCOMPARE(int(info.swOut[0]), 0);

    /* An interface without a hardware address (a VPN tunnel, some USB
       adapters) still answers with a well-formed reply: the MAC field is
       six bytes whatever the string held, and the fields after it stay
       where they belong. */
    for (const QString &mac : { QString(), QString("02:4D:48") })
    {
        ap.setupArtNetPollReply(reply, QHostAddress("172.18.2.219"), mac, 0x23, false);
        QVERIFY2(reply.size() == 239, qPrintable(QString("MAC '%1': %2 bytes").arg(mac).arg(reply.size())));
        QVERIFY(ap.fillArtPollReplyInfo(reply, info));
        QVERIFY(info.macAddress.startsWith(mac.isEmpty() ? "00:00:00" : mac));
        QVERIFY(info.macAddress.endsWith("00:00:00"));
    }
}

void ArtNet_Test::pollIsAnsweredOncePerUniverse()
{
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    ctrl->addUniverse(0, ArtNetController::Input);
    ctrl->addUniverse(1, ArtNetController::Output);
    ctrl->setOutputUniverse(1, 0x23);
    sink.clear();
    const quint64 sent = ctrl->getPacketSentNumber();
    const quint64 received = ctrl->getPacketReceivedNumber();

    const QHostAddress asker("172.18.2.50");
    QVERIFY(ctrl->handlePacket(QByteArray::fromHex(kArtPollBytes), asker));
    QCOMPARE(sink.count(), 2);
    ArtNetPacketizer ap;
    for (int i = 0; i < 2; i++)
    {
        const QNetworkDatagram &dg = sink.at(i);
        QCOMPARE(dg.destinationAddress(), asker);
        QCOMPARE(dg.destinationPort(), ARTNET_PORT);
        QCOMPARE(dg.data().size(), 239);
        ArtNetNodeInfo info;
        QVERIFY(ap.fillArtPollReplyInfo(dg.data(), info));
        QCOMPARE(info.ipAddress, QString("172.18.2.219"));
    }
    // In universe order: the input, then the output. The second reply is
    // built afresh, not appended to the first.
    QCOMPARE(int(uchar(sink.at(0).data().at(174))), 0x40);
    QCOMPARE(int(uchar(sink.at(1).data().at(174))), 0x80);
    QCOMPARE(ctrl->getPacketSentNumber(), sent + 2);
    QCOMPARE(ctrl->getPacketReceivedNumber(), received + 1);
}

void ArtNet_Test::rdmCommandsGoToThePatchedNode()
{
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    ctrl->addUniverse(0, ArtNetController::Output);
    QCOMPARE(ctrl->setOutputIPAddress(0, "172.18.2.10"), false);
    QCOMPARE(ctrl->setOutputUniverse(0, 0x105), false);
    sink.clear();
    quint64 sent = ctrl->getPacketSentNumber();

    // Discovery: an ArtTodRequest for the mapped Port-Address, to the node
    QVERIFY(ctrl->sendRDMCommand(0, DISCOVERY_COMMAND, QVariantList()));
    QCOMPARE(sink.count(), 1);
    QByteArray d = sink.last().data();
    QCOMPARE(sink.last().destinationAddress(), QHostAddress("172.18.2.10"));
    QCOMPARE(artOpCode(d), ARTNET_TODREQUEST);
    QCOMPARE(d.size(), 25);
    QCOMPARE(int(uchar(d.at(21))), 0x01);   // Net
    QCOMPARE(int(uchar(d.at(22))), 0x00);   // TodFull
    QCOMPARE(int(uchar(d.at(23))), 0x01);   // AddCount
    QCOMPARE(int(uchar(d.at(24))), 0x05);   // Address
    QCOMPARE(ctrl->getPacketSentNumber(), sent + 1);

    // Mute and un-mute mean nothing to Art-Net: not sent
    QVERIFY(!ctrl->sendRDMCommand(0, DISCOVERY_COMMAND, QVariantList({ QString("ffffffffffff"), PID_DISC_MUTE })));
    QCOMPARE(sink.count(), 1);

    // A GET: an ArtRdm to the same place
    QVERIFY(ctrl->sendRDMCommand(0, GET_COMMAND, QVariantList({ QString("7a7000000001"), PID_DMX_START_ADDRESS })));
    QCOMPARE(sink.count(), 2);
    d = sink.last().data();
    QCOMPARE(sink.last().destinationAddress(), QHostAddress("172.18.2.10"));
    QCOMPARE(artOpCode(d), ARTNET_RDM);
    QCOMPARE(int(uchar(d.at(21))), 0x01);
    QCOMPARE(int(uchar(d.at(23))), 0x05);
    QCOMPARE(d.mid(24, 1), QByteArray(1, char(RDM_SC_SUB_MESSAGE)));
    QCOMPARE(d.mid(26, 6), QByteArray::fromHex("7a7000000001"));
    QCOMPARE(ctrl->getPacketSentNumber(), sent + 2);

    // A universe not on this line: the broadcast, its own number
    QVERIFY(ctrl->sendRDMCommand(3, DISCOVERY_COMMAND, QVariantList()));
    QCOMPARE(sink.last().destinationAddress(), QHostAddress("172.18.2.255"));
    QCOMPARE(int(uchar(sink.last().data().at(24))), 3);

    // A held line sends nothing and says so
    ctrl->m_held.store(true);
    QVERIFY(!ctrl->sendRDMCommand(0, DISCOVERY_COMMAND, QVariantList()));
    QCOMPARE(sink.count(), 3);
    ctrl->m_held.store(false);

    // A write that fails is a failure, not counted
    sent = ctrl->getPacketSentNumber();
    ctrl->m_testSinkFails = true;
    QTest::ignoreMessage(QtWarningMsg, "sendDmx failed");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Errno:"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Errmgs:"));
    QVERIFY(!ctrl->sendRDMCommand(0, DISCOVERY_COMMAND, QVariantList()));
    QCOMPARE(ctrl->getPacketSentNumber(), sent);
}

void ArtNet_Test::rdmRepliesReachTheSignal()
{
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    QSignalSpy rdm(ctrl, SIGNAL(rdmValueChanged(quint32,quint32,QVariantMap)));
    const QHostAddress node("172.18.2.10");

    // A table of devices: one emit, with every UID
    QByteArray tod = artHeader(ARTNET_TODDATA, 28)
                     + QByteArray::fromHex("4d4812345678" "7a70deadbeef");
    tod[21] = char(0x01);
    tod[23] = char(0x93);
    tod[27] = char(2);
    QVERIFY(ctrl->handlePacket(tod, node));
    QCOMPARE(rdm.count(), 1);
    QCOMPARE(rdm.last().at(0).toUInt(), 0x0193u);
    QVariantMap values = rdm.last().at(2).toMap();
    QCOMPARE(values.size(), 3);
    QCOMPARE(values.value("DISCOVERY_COUNT").toInt(), 2);
    QCOMPARE(values.value("UID-1").toString(), QString("7A70DEADBEEF"));

    // An empty table: the count alone
    ArtNetPacketizer ap;
    quint32 universe = 0;
    values.clear();
    QVERIFY(ap.processTODdata(artHeader(ARTNET_TODDATA, 28), universe, values));
    QCOMPARE(values.size(), 1);

    // An RDM answer: one emit
    QVERIFY(ctrl->handlePacket(artRdmReply(PID_DMX_START_ADDRESS, QByteArray::fromHex("002a")), node));
    QCOMPARE(rdm.count(), 2);
    QCOMPARE(rdm.last().at(0).toUInt(), 0x0003u);
    QVERIFY(!rdm.last().at(2).toMap().isEmpty());

    // A bad checksum: none
    QByteArray bad = artRdmReply(PID_DMX_START_ADDRESS, QByteArray::fromHex("002a"));
    bad[bad.size() - 1] = char(bad.at(bad.size() - 1) ^ 0x01);
    ctrl->handlePacket(bad, node);
    QCOMPARE(rdm.count(), 2);
}

void ArtNet_Test::pollTickDrivesTheNodeWatchAndOffSubnetPolls()
{
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    ctrl->m_testNowMs = 0;
    ctrl->addUniverse(0, ArtNetController::Output);
    ctrl->setOutputIPAddress(0, "10.0.0.5");            // off this subnet
    ctrl->addUniverse(1, ArtNetController::Output);
    ctrl->setOutputIPAddress(1, "172.18.2.10");         // on it: the CR041R
    ctrl->addUniverse(2, ArtNetController::Input);
    ctrl->setOutputIPAddress(2, "10.0.0.6");            // input only: no poll
    ctrl->addUniverse(3, ArtNetController::Output);     // the broadcast

    // One broadcast poll, plus one unicast poll to the off-subnet target
    sink.clear();
    QVERIFY(QMetaObject::invokeMethod(ctrl, "slotPollTick", Qt::DirectConnection));
    QCOMPARE(sink.count(), 2);
    QCOMPARE(sink.at(0).destinationAddress(), QHostAddress("172.18.2.255"));
    QCOMPARE(sink.at(1).destinationAddress(), QHostAddress("10.0.0.5"));
    QCOMPARE(sink.at(1).data(), QByteArray::fromHex(kArtPollBytes));

    // The node watch runs on the same tick: heard, then silent for 4 polls
    const QHostAddress node("172.18.2.10");
    for (int i = 0; i < 3; i++)
    {
        QVERIFY(QMetaObject::invokeMethod(ctrl, "slotPollTick", Qt::DirectConnection));
        ctrl->handlePacket(cr041rWith(1, 0x80), node);
        ctrl->m_testNowMs += kPollMs;
    }
    QVERIFY(texts(ctrl->outputProblems(1)).isEmpty());
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^\\[ArtNet\\] Art-Net node CR041R \\(172\\.18\\.2\\.10\\): stopped answering"));
    for (int i = 0; i < 4; i++)
    {
        QVERIFY(QMetaObject::invokeMethod(ctrl, "slotPollTick", Qt::DirectConnection));
        ctrl->m_testNowMs += kPollMs;
    }
    const QStringList t = texts(ctrl->outputProblems(1));
    QCOMPARE(t.count(), 1);
    QVERIFY2(t.first().startsWith("Art-Net node CR041R (172.18.2.10): stopped answering since "),
             qPrintable(t.first()));
}

void ArtNet_Test::checkDataMutexIsFree()
{
    m_lockChecks++;
    if (m_lockCheck->m_dataMutex.tryLock())
        m_lockCheck->m_dataMutex.unlock();
    else
        m_lockHeld++;
}

void ArtNet_Test::problemsChangedIsNeverEmittedUnderTheLock()
{
    /* A listener connected directly (the plugin forwards problemsChanged,
       and the Connections tree asks for the problems at once) calls back
       into the controller: emitting with m_dataMutex held deadlocks it. */
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    ctrl->addUniverse(0, ArtNetController::Output);
    m_lockCheck = ctrl;
    m_lockChecks = m_lockHeld = 0;
    QVERIFY(connect(ctrl, SIGNAL(problemsChanged()), this, SLOT(checkDataMutexIsFree()),
                    Qt::DirectConnection));

    ctrl->setOutputIPAddress(0, "");
    ctrl->setOutputIPAddress(0, "172.18.2.10");
    QCOMPARE(m_lockChecks, 2);

    ctrl->m_testNowMs = 0;
    ctrl->m_testSinkFails = true;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("universe 1 output to .172\\.18\\.2\\.10. is failing"));
    for (int i = 0; i < 3; i++)
    {
        QVERIFY(QMetaObject::invokeMethod(ctrl, "slotSendAllUniverses", Qt::DirectConnection));
        ctrl->m_testNowMs += 600;
    }
    QCOMPARE(m_lockChecks, 3);
    ctrl->setOutputIPAddress(0, "172.18.2.11");     // a clean slate
    ctrl->m_testNowMs = 0;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("universe 1 output to .172\\.18\\.2\\.11. is failing"));
    for (int i = 0; i < 3; i++)
    {
        ctrl->sendDmx(0, QByteArray(512, char(i)), true);
        ctrl->m_testNowMs += 600;
    }
    QCOMPARE(m_lockChecks, 5);
    QCOMPARE(m_lockHeld, 0);
    disconnect(ctrl, SIGNAL(problemsChanged()), this, SLOT(checkDataMutexIsFree()));
    m_lockCheck = nullptr;
}

void ArtNet_Test::steadySendsAnnounceNothing()
{
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    ctrl->addUniverse(0, ArtNetController::Output);
    ctrl->m_testNowMs = 0;
    QSignalSpy changed(ctrl, SIGNAL(problemsChanged()));
    for (int i = 0; i < 100; i++)
    {
        ctrl->sendDmx(0, QByteArray(512, char(i)), true);
        ctrl->m_testNowMs += 23;
    }
    QCOMPARE(sink.count(), 1 + 100);    // the first poll, then the frames
    QCOMPARE(changed.count(), 0);
    QVERIFY(!ctrl->reportSendResult(ctrl->m_universeMap[0], 0, QHostAddress("172.18.2.255"), true));
}

void ArtNet_Test::pollReplyOptionalFields()
{
    ArtNetPacketizer ap;
    ArtNetNodeInfo info;
    const QByteArray reply(cr041rPollReply, sizeof(cr041rPollReply));
    QVERIFY(ap.fillArtPollReplyInfo(reply, info));
    QCOMPARE(info.nodeReport, QString("0006 [8443] ArtNet"));
    QCOMPARE(int(info.reportCode), 6);
    QCOMPARE(int(info.bindIndex), 1);
    QCOMPARE(info.bindIpAddress, QString("172.18.2.10"));
    QCOMPARE(int(info.status2), 0x08);

    /* An older node stops after BindIndex: Status2 and its DHCP bits are 0,
       not left as they were */
    info.status2 = 0xff;
    info.dhcpCapable = info.dhcpConfigured = true;
    QVERIFY(ap.fillArtPollReplyInfo(reply.left(212), info));
    QCOMPARE(int(info.bindIndex), 1);
    QCOMPARE(info.bindIpAddress, QString("172.18.2.10"));
    QCOMPARE(int(info.status2), 0);
    QCOMPARE(info.dhcpCapable, false);
    QCOMPARE(info.dhcpConfigured, false);
    QVERIFY(!ap.fillArtPollReplyInfo(reply.left(211), info));

    // A name is cut at its NUL, whatever follows it
    QByteArray junk = reply;
    junk[26] = char(0);
    QVERIFY(ap.fillArtPollReplyInfo(junk, info));
    QVERIFY(info.shortName.isEmpty());

    // A report with a '#' code
    QByteArray hashed = reply;
    const QByteArray rep("#0007 [0001] renamed");
    hashed.replace(108, rep.size(), rep);
    QVERIFY(ap.fillArtPollReplyInfo(hashed, info));
    QCOMPARE(int(info.reportCode), 7);
}

void ArtNet_Test::ipProgReplyBoundaries()
{
    ArtNetPacketizer ap;
    QByteArray data = artHeader(ARTNET_IPREPLY, 34);
    data.replace(16, 4, QByteArray::fromHex("0a000005"));
    data.replace(20, 4, QByteArray::fromHex("ff000000"));
    data[26] = char(0x00);                                   // static
    data.replace(28, 4, QByteArray::fromHex("0a000001"));

    ArtNetIpProgReply reply;
    reply.dhcpEnabled = true;
    QVERIFY(ap.fillArtIpProgReply(data, reply));
    QCOMPARE(reply.dhcpEnabled, false);
    QCOMPARE(reply.ip, QHostAddress("10.0.0.5"));
    QCOMPARE(reply.gateway, QHostAddress("10.0.0.1"));

    QVERIFY(ap.fillArtIpProgReply(data.left(27), reply));    // up to Status
    QCOMPARE(reply.hasGateway, false);
    QVERIFY(!ap.fillArtIpProgReply(data.left(26), reply));
    QVERIFY(ap.fillArtIpProgReply(data.left(32), reply));
    QCOMPARE(reply.hasGateway, true);
    QCOMPARE(reply.gateway, QHostAddress("10.0.0.1"));
    QVERIFY(ap.fillArtIpProgReply(data.left(31), reply));
    QCOMPARE(reply.hasGateway, false);

    // Through the controller: the plugin hears it, the counter goes up
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    QSignalSpy replied(ctrl, SIGNAL(ipProgReplied(QString,ArtNetIpProgReply)));
    const quint64 before = ctrl->getPacketReceivedNumber();
    QVERIFY(ctrl->handlePacket(data, QHostAddress("10.0.0.5")));
    QCOMPARE(replied.count(), 1);
    QCOMPARE(replied.last().at(0).toString(), QString("10.0.0.5"));
    QCOMPARE(ctrl->getPacketReceivedNumber(), before + 1);
}

void ArtNet_Test::legacyPartialOutputIPs()
{
    /* Old workspaces saved only the host part of the output address: it is
       completed from this line's own address. */
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    ctrl->addUniverse(0, ArtNetController::Output);

    QCOMPARE(ctrl->setOutputIPAddress(0, "10"), false);
    QCOMPARE(ctrl->getUniverseInfo(0)->outputAddress, QHostAddress("172.18.2.10"));
    QCOMPARE(ctrl->setOutputIPAddress(0, "3.11"), false);
    QCOMPARE(ctrl->getUniverseInfo(0)->outputAddress, QHostAddress("172.18.3.11"));
    QCOMPARE(ctrl->setOutputIPAddress(0, "255"), true);       // our broadcast
    QCOMPARE(ctrl->getUniverseInfo(0)->outputAddress, QHostAddress("172.18.2.255"));
    QCOMPARE(ctrl->setOutputIPAddress(0, "192.168.1.20"), false);
    QCOMPARE(ctrl->getUniverseInfo(0)->outputAddress, QHostAddress("192.168.1.20"));
    // Not an address at all: nowhere, and no crash (five octets indexed
    // before the start of the line's own four)
    ctrl->setOutputIPAddress(0, "1.2.3.4.5");
    QVERIFY(ctrl->getUniverseInfo(0)->outputAddress.isNull());
    QCOMPARE(ctrl->setOutputIPAddress(0, ""), true);          // back to the default
    QCOMPARE(ctrl->getUniverseInfo(0)->outputAddress, QHostAddress("172.18.2.255"));
}

void ArtNet_Test::failedWritesAreReported()
{
    QObject owner;
    QList<QNetworkDatagram> sink;
    ArtNetController *ctrl = sinkController(&owner, &sink);
    const quint64 sent = ctrl->getPacketSentNumber();

    // Counted when they go
    ArtNetIpProgRequest query;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("ArtIpProg \\(query\\)"));
    QVERIFY(ctrl->sendIpProg(QHostAddress("172.18.2.10"), query));
    QCOMPARE(ctrl->getPacketSentNumber(), sent + 1);
    ctrl->sendPollTo(QHostAddress("10.0.0.5"));
    QCOMPARE(ctrl->getPacketSentNumber(), sent + 2);
    ctrl->sendPollTo(QHostAddress());
    QCOMPARE(sink.count(), 2);

    // Failures: reported to the caller, not counted; the periodic unicast
    // poll stays quiet, an explicit one warns
    ctrl->m_testSinkFails = true;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("ArtIpProg \\(query\\)"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("ArtIpProg to .172\\.18\\.2\\.10. failed"));
    QVERIFY(!ctrl->sendIpProg(QHostAddress("172.18.2.10"), query));
    QTest::failOnWarning(QRegularExpression("Unable to send Poll packet to .10\\.0\\.0\\.6"));
    ctrl->sendPollTo(QHostAddress("10.0.0.6"), true);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unable to send Poll packet to .10\\.0\\.0\\.5"));
    ctrl->sendPollTo(QHostAddress("10.0.0.5"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unable to send Poll packet: errno="));
    ctrl->sendPoll();
    QCOMPARE(ctrl->getPacketSentNumber(), sent + 2);
}

void ArtNet_Test::socketBound()
{
    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress::LocalHost);
    entry.setPrefixLength(8);
    QSharedPointer<QUdpSocket> sock(new QUdpSocket());
    ArtNetController ctrl(QNetworkInterface(), entry, sock, 0);
    QVERIFY(!ctrl.socketBound());
    QVERIFY(sock->bind(QHostAddress::LocalHost, 0));
    QVERIFY(ctrl.socketBound());
}

/** A line of @p plugin on 172.18.<net>.219/24 whose controller sends into
 *  @p sink */
static void addSinkLine(ArtNetPlugin &plugin, QList<QNetworkDatagram> *sink, int net)
{
    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress(QString("172.18.%1.219").arg(net)));
    entry.setPrefixLength(24);
    entry.setBroadcast(QHostAddress(QString("172.18.%1.255").arg(net)));
    ArtNetIO io;
    io.address = entry;
    io.controller = new ArtNetController(QNetworkInterface(), entry,
                                         QSharedPointer<QUdpSocket>(new QUdpSocket()),
                                         quint32(plugin.m_IOmapping.count()), &plugin);
    io.controller->m_testSink = sink;
    plugin.m_IOmapping.append(io);
}

void ArtNet_Test::pluginKeepsOnlyWhatDiffersFromTheDefault()
{
    ArtNetPlugin plugin;
    plugin.m_IOmapping.clear();
    plugin.m_ifaceWaitTime = 0;     // a missing line is refused at once, no rescan
    QList<QNetworkDatagram> sink;
    addSinkLine(plugin, &sink, 2);
    typedef QLCIOPlugin P;

    QVERIFY(plugin.openOutput(0, 4));
    QVERIFY(plugin.openInput(0, 4));
    QVERIFY(!plugin.openOutput(3, 4));                       // no such line
    auto out = [&plugin]() { return plugin.getParameters(4, 0, P::Output); };
    auto in = [&plugin]() { return plugin.getParameters(4, 0, P::Input); };

    plugin.setParameter(4, 0, P::Output, ARTNET_OUTPUTIP, "172.18.2.10");
    QCOMPARE(out().value(ARTNET_OUTPUTIP).toString(), QString("172.18.2.10"));
    plugin.setParameter(4, 0, P::Output, ARTNET_OUTPUTUNI, 9);
    QCOMPARE(out().value(ARTNET_OUTPUTUNI).toUInt(), 9u);
    plugin.setParameter(4, 0, P::Output, ARTNET_TRANSMITMODE, "Partial");
    QCOMPARE(out().value(ARTNET_TRANSMITMODE).toString(), QString("Partial"));
    plugin.setParameter(4, 0, P::Input, ARTNET_INPUTUNI, 7);
    QCOMPARE(in().value(ARTNET_INPUTUNI).toUInt(), 7u);

    // What goes out follows them
    sink.clear();
    plugin.writeUniverse(4, 0, QByteArray(10, char(5)), true);
    plugin.writeUniverse(4, 9, QByteArray(10, char(5)), true);     // no such line
    QCOMPARE(sink.count(), 1);
    QCOMPARE(sink.first().destinationAddress(), QHostAddress("172.18.2.10"));
    QCOMPARE(artDmxPortAddress(sink.first().data()), 9);
    QCOMPARE(artDmxLength(sink.first().data()), 10);

    // Back to the defaults: nothing kept
    plugin.setParameter(4, 0, P::Output, ARTNET_OUTPUTIP, "172.18.2.255");
    plugin.setParameter(4, 0, P::Output, ARTNET_OUTPUTUNI, 4);
    plugin.setParameter(4, 0, P::Output, ARTNET_TRANSMITMODE, "Standard");
    plugin.setParameter(4, 0, P::Input, ARTNET_INPUTUNI, 4);
    QVERIFY(out().isEmpty());
    QVERIFY(in().isEmpty());

    // Not ours: not kept
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("is not a valid ArtNet output parameter"));
    plugin.setParameter(4, 0, P::Output, "colour", "red");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("is not a valid ArtNet input parameter"));
    plugin.setParameter(4, 0, P::Input, ARTNET_OUTPUTIP, "1.2.3.4");
    QVERIFY(out().isEmpty());
    QVERIFY(in().isEmpty());

    // The line's controller goes with its last universe
    plugin.closeOutput(0, 4);
    QVERIFY(plugin.m_IOmapping.at(0).controller != NULL);
    plugin.closeInput(0, 4);
    QVERIFY(plugin.m_IOmapping.at(0).controller == NULL);
}

void ArtNet_Test::closingAUniverseDropsOnlyItsOwnParameters()
{
    /* Universe 0 on line 1, universe 1 on line 0, each with a target. The
       plugins called removeFromMap(line, universe) -- the wrong way round
       -- so unpatching universe 0 from line 1 wiped universe 1's settings
       (gone from the next save, while still live), and unpatching universe
       1 left its own behind for whatever line it was patched to next. */
    ArtNetPlugin plugin;
    plugin.m_IOmapping.clear();
    QList<QNetworkDatagram> sink;
    addSinkLine(plugin, &sink, 2);
    addSinkLine(plugin, &sink, 3);
    typedef QLCIOPlugin P;

    QVERIFY(plugin.openOutput(1, 0));
    QVERIFY(plugin.openOutput(0, 1));
    plugin.setParameter(0, 1, P::Output, ARTNET_OUTPUTIP, "172.18.3.10");
    plugin.setParameter(1, 0, P::Output, ARTNET_OUTPUTIP, "172.18.2.10");

    plugin.closeOutput(1, 0);
    QCOMPARE(plugin.getParameters(1, 0, P::Output).value(ARTNET_OUTPUTIP).toString(),
             QString("172.18.2.10"));
    QVERIFY(plugin.getParameters(0, 1, P::Output).isEmpty());

    // Universe 1 moved to line 1: it starts clean there
    plugin.closeOutput(0, 1);
    QVERIFY(plugin.getParameters(1, 0, P::Output).isEmpty());
    QVERIFY(plugin.openOutput(1, 1));
    QVERIFY(plugin.getParameters(1, 1, P::Output).isEmpty());
    plugin.closeOutput(1, 1);
}

/** The RDM checksum of @p b from its first byte (no start code in @p b) */
static quint16 rdmSum(const QByteArray &b, bool startCodeIncluded)
{
    quint16 cs = startCodeIncluded ? 0 : RDM_START_CODE;
    for (char c : b)
        cs += uchar(c);
    return cs;
}

void ArtNet_Test::rdmCommandsAreBuiltToE120()
{
    RDMProtocol rdm;
    QByteArray b;
    const QString dest("4d4812345678");

    // No destination: nothing to build
    QVERIFY(!rdm.packetizeCommand(GET_COMMAND, QVariantList(), false, b));
    QVERIFY(!rdm.packetizeCommand(GET_COMMAND, QVariantList({ dest }), false, b));

    // A plain GET, no start code: header, PID, PDL 0, checksum
    QVERIFY(rdm.packetizeCommand(GET_COMMAND, QVariantList({ dest, PID_DEVICE_INFO }), false, b));
    QCOMPARE(b.size(), 23 + 2);
    QCOMPARE(int(uchar(b.at(0))), RDM_SC_SUB_MESSAGE);
    QCOMPARE(int(uchar(b.at(1))), 24);                                  // with the start code
    QCOMPARE(b.mid(2, 6), QByteArray::fromHex("4d4812345678"));
    QCOMPARE(b.mid(8, 6), RDMProtocol().UIDToByteArray(QLCPLUS_ESTA_ID, QLCPLUS_DEVICE_ID));
    const int tn = uchar(b.at(14));
    QCOMPARE(int(uchar(b.at(15))), 1);                                   // port
    QCOMPARE(int(uchar(b.at(19))), GET_COMMAND);
    QCOMPARE(RDMProtocol().byteArrayToShort(b, 20), quint16(PID_DEVICE_INFO));
    QCOMPARE(int(uchar(b.at(22))), 0);                                   // PDL
    QCOMPARE(RDMProtocol().byteArrayToShort(b, 23), rdmSum(b.left(23), false));

    // A GET with a 2-byte parameter, with the start code; the next transaction
    QVERIFY(rdm.packetizeCommand(GET_COMMAND,
                                 QVariantList({ dest, PID_SLOT_DESCRIPTION, 2, 0x0102 }), true, b));
    QCOMPARE(int(uchar(b.at(0))), RDM_START_CODE);
    QCOMPARE(int(uchar(b.at(2))), 24 + 2);
    QCOMPARE(int(uchar(b.at(15))), (tn + 1) & 0xff);
    QCOMPARE(int(uchar(b.at(23))), 2);
    QCOMPARE(b.mid(24, 2), QByteArray::fromHex("0102"));
    QCOMPARE(RDMProtocol().byteArrayToShort(b, 26), rdmSum(b.left(26), true));
    for (int size : { 1, 4 })
    {
        QVERIFY(rdm.packetizeCommand(GET_COMMAND,
                                     QVariantList({ dest, PID_SLOT_DESCRIPTION, size, 0x0a0b0c0d }), false, b));
        QCOMPARE(int(uchar(b.at(22))), size);
        QCOMPARE(b.mid(23, size), size == 1 ? QByteArray::fromHex("0d") : QByteArray::fromHex("0a0b0c0d"));
    }

    // Discovery: unique branch with its range, mute with none
    QVERIFY(rdm.packetizeCommand(DISCOVERY_COMMAND,
                                 QVariantList({ RDMProtocol::broadcastAddress(), PID_DISC_UNIQUE_BRANCH,
                                                qulonglong(0), qulonglong(0xffffffffffffULL) }), false, b));
    QCOMPARE(b.mid(2, 6), QByteArray::fromHex("ffffffffffff"));
    QCOMPARE(int(uchar(b.at(19))), DISCOVERY_COMMAND);
    QCOMPARE(int(uchar(b.at(22))), 12);
    QCOMPARE(b.mid(23, 12), QByteArray::fromHex("000000000000ffffffffffff"));
    QVERIFY(rdm.packetizeCommand(DISCOVERY_COMMAND,
                                 QVariantList({ dest, PID_DISC_MUTE }), false, b));
    QCOMPARE(int(uchar(b.at(22))), 0);
    QCOMPARE(b.size(), 25);
    // A unique branch without its range is refused, not read past
    QVERIFY(!rdm.packetizeCommand(DISCOVERY_COMMAND,
                                  QVariantList({ dest, PID_DISC_UNIQUE_BRANCH }), false, b));

    // The helpers
    QCOMPARE(RDMProtocol().longToByteArray(0x01020304), QByteArray::fromHex("01020304"));
    QCOMPARE(RDMProtocol().byteArrayToLong(QByteArray::fromHex("01020304"), 0), 0x01020304u);
    QCOMPARE(RDMProtocol().byteArrayToLong(QByteArray::fromHex("010203"), 0), 0u);
    QCOMPARE(RDMProtocol().byteArrayToShort(QByteArray::fromHex("01"), 0), quint16(0));
    QCOMPARE(RDMProtocol().byteArrayToShort(QByteArray::fromHex("0102"), -1), quint16(0));
    QCOMPARE(RDMProtocol::broadcastAddress().toLower(), QString("ffffffffffff"));
    QCOMPARE(RDMProtocol::responseToString(RESPONSE_TYPE_NACK_REASON), QString("NACK"));
    QCOMPARE(RDMProtocol::responseToString(RESPONSE_TYPE_ACK_TIMER), QString("TIMEOUT"));
    QCOMPARE(RDMProtocol::responseToString(RESPONSE_TYPE_ACK_OVERFLOW), QString("OVERFLOW"));
    QCOMPARE(RDMProtocol::responseToString(0x77), QString("UNKNOWN"));
    QCOMPARE(RDMProtocol::categoryToString(0x0102), QString("Fixture Moving Yoke"));
    QCOMPARE(RDMProtocol::categoryToString(0x1234), QString("Unknown"));
    QCOMPARE(RDMProtocol::pidToString(PID_IDENTIFY_DEVICE), QString("PID_IDENTIFY_DEVICE"));
}

void ArtNet_Test::rdmSetCommandsCarryTheirValues()
{
    RDMProtocol rdm;
    QByteArray b;

    /* SET: (size, value) pairs after the UID and the PID. The loop read
       the UID and the PID as a pair too, so a UID whose text read as 1, 2,
       4 or 99 put the PID into the parameter data. */
    for (const QString &dest : { QString("4d4812345678"), QString("000000000001"),
                                 QString("000000000002"), QString("000000000099") })
    {
        QVERIFY(rdm.packetizeCommand(SET_COMMAND,
                                     QVariantList({ dest, PID_DMX_START_ADDRESS, 1, 5, 2, 0x1234, 4, 0xdeadbeef }),
                                     false, b));
        QVERIFY2(int(uchar(b.at(22))) == 7, qPrintable(dest + ": PDL " + QString::number(uchar(b.at(22)))));
        QCOMPARE(b.mid(23, 7), QByteArray::fromHex("051234deadbeef"));
        QCOMPARE(int(uchar(b.at(1))), 24 + 7);
        QCOMPARE(RDMProtocol().byteArrayToShort(b, 30), rdmSum(b.left(30), false));
    }

    // A byte array (99) is taken whole, and ends the list
    QVERIFY(rdm.packetizeCommand(SET_COMMAND,
                                 QVariantList({ QString("4d4812345678"), PID_DMX_START_ADDRESS,
                                                99, QByteArray("label"), 1, 9 }), false, b));
    QCOMPARE(int(uchar(b.at(22))), 5);
    QCOMPARE(b.mid(23, 5), QByteArray("label"));

    // A size with no value: nothing read past the list
    QVERIFY(rdm.packetizeCommand(SET_COMMAND,
                                 QVariantList({ QString("4d4812345678"), PID_DMX_START_ADDRESS, 2 }), false, b));
    QCOMPARE(int(uchar(b.at(22))), 0);
    QVERIFY(!rdm.packetizeCommand(SET_COMMAND, QVariantList({ QString("4d4812345678") }), false, b));
}

/** A DISC_UNIQUE_BRANCH response for @p uid (E1.20 7.5.4): preamble,
 *  separator, the EUID and its checksum, each byte as b|0xAA, b|0x55 */
static QByteArray discoveryReply(const QByteArray &uid)
{
    QByteArray b(7, char(0xfe));
    b.append(char(0xaa));
    QByteArray euid;
    for (char c : uid)
        euid.append(char(uchar(c) | 0xaa)).append(char(uchar(c) | 0x55));
    quint16 cs = 0;
    for (char c : euid)
        cs += uchar(c);
    b.append(euid);
    b.append(char((cs >> 8) | 0xaa)).append(char((cs >> 8) | 0x55));
    b.append(char((cs & 0xff) | 0xaa)).append(char((cs & 0xff) | 0x55));
    return b;
}

void ArtNet_Test::rdmDiscoveryRepliesAreDecoded()
{
    RDMProtocol rdm;
    QVariantMap values;
    const QByteArray good = discoveryReply(QByteArray::fromHex("4d4812345678"));
    QCOMPARE(good.size(), 24);
    QVERIFY(rdm.parseDiscoveryReply(good, values));
    QCOMPARE(values.value("DISCOVERY_COUNT").toInt(), 1);
    QCOMPARE(values.value("UID-0").toString(), QString("4D4812345678"));

    values.clear();
    QVERIFY(!rdm.parseDiscoveryReply(good.left(23), values));
    QByteArray bad = good;
    bad[3] = char(0x00);                                    // preamble
    QVERIFY(!rdm.parseDiscoveryReply(bad, values));
    bad = good;
    bad[7] = char(0x00);                                    // separator
    QVERIFY(!rdm.parseDiscoveryReply(bad, values));
    bad = good;
    bad[10] = char(uchar(bad.at(10)) ^ 0x01);               // EUID, checksum unchanged
    QVERIFY(!rdm.parseDiscoveryReply(bad, values));
    QVERIFY(values.isEmpty());
}

void ArtNet_Test::rdmMoreRepliesParse()
{
    ArtNetPacketizer p;
    quint32 universe = 0;
    QVariantMap v;

    QVERIFY(p.processRDMdata(artRdmReply(PID_DEVICE_MODEL_DESCRIPTION, "Spot 575"), universe, v));
    QCOMPARE(v.value("MODEL_NAME").toString(), QString("Spot 575"));
    v.clear();
    QVERIFY(p.processRDMdata(artRdmReply(PID_MANUFACTURER_LABEL, "Acme"), universe, v));
    QCOMPARE(v.value("MANUFACTURER").toString(), QString("Acme"));

    v.clear();
    QVERIFY(p.processRDMdata(artRdmReply(PID_DMX_PERSONALITY, QByteArray::fromHex("0204")), universe, v));
    QCOMPARE(v.value("PERS_CURRENT").toInt(), 2);
    QCOMPARE(v.value("PERS_COUNT").toInt(), 4);

    // PARAMETER_DESCRIPTION: 20 fixed bytes, then the text
    v.clear();
    const QByteArray pd = QByteArray::fromHex("8001" "02" "03" "01" "05" "06" "00"
                                              "00000001" "000000ff" "00000010") + "Fan speed";
    QVERIFY(p.processRDMdata(artRdmReply(PID_PARAMETER_DESCRIPTION, pd), universe, v));
    QCOMPARE(v.value("PID_INFO").toInt(), 0x8001);
    QCOMPARE(v.value("PDL Size").toInt(), 2);
    QCOMPARE(v.value("Min Valid Value").toUInt(), 1u);
    QCOMPARE(v.value("Max Valid Value").toUInt(), 255u);
    QCOMPARE(v.value("Default Value").toUInt(), 16u);
    QCOMPARE(v.value("PID_DESC").toString(), QString("Fan speed"));

    // SLOT_INFO: 5 bytes a slot; a part slot is not one
    v.clear();
    QVERIFY(p.processRDMdata(artRdmReply(PID_SLOT_INFO,
                                         QByteArray::fromHex("0000000001" "0001000002" "0002")), universe, v));
    QCOMPARE(v.value("SLOT_LIST").value<QVector<quint16>>(), QVector<quint16>({ 0, 1 }));
    v.clear();
    QVERIFY(p.processRDMdata(artRdmReply(PID_SLOT_DESCRIPTION, QByteArray::fromHex("0003") + "Gobo"),
                             universe, v));
    QCOMPARE(v.value("SLOT_ID").toInt(), 3);
    QCOMPARE(v.value("SLOT_DESC").toString(), QString("Gobo"));

    // Too short for their fields: parsed, the fields left out
    for (quint16 pid : { quint16(PID_DEVICE_INFO), quint16(PID_PARAMETER_DESCRIPTION),
                         quint16(PID_DMX_PERSONALITY), quint16(PID_DMX_START_ADDRESS),
                         quint16(PID_SLOT_DESCRIPTION) })
    {
        v.clear();
        QVERIFY(p.processRDMdata(artRdmReply(pid, QByteArray::fromHex("01")), universe, v));
        QCOMPARE(v.value("PID").toInt(), int(pid));
        QVERIFY(!v.contains("DMX_START_ADDRESS") && !v.contains("PID_INFO")
                && !v.contains("PERS_CURRENT") && !v.contains("SLOT_ID"));
    }

    // Any other PID: the raw data, control bytes shown as #
    v.clear();
    QVERIFY(p.processRDMdata(artRdmReply(PID_IDENTIFY_DEVICE, QByteArray::fromHex("01") + "A"), universe, v));
    QCOMPARE(v.value("RAW_DATA").toByteArray(), QByteArray("01,41"));
    QCOMPARE(v.value("RAW_DATA_STRING").toString(), QString("#A"));

    // Our own request, heard back: not a reply
    RDMProtocol rdm;
    QByteArray own;
    QVERIFY(rdm.packetizeCommand(GET_COMMAND, QVariantList({ QString("4d4812345678"), PID_DEVICE_INFO }), true, own));
    v.clear();
    QVERIFY(!rdm.parsePacket(own, v));
    QVERIFY(!rdm.parsePacket(QByteArray(), v));
}

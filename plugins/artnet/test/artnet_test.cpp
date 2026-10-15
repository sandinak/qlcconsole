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
 * Send failure reporting (ArtNetSendHealth)
 ****************************************************************************/

/* Frame period while data is changing (~44 Hz), and the idle refresh period
   of Standard mode (SEND_INTERVAL_MS). The rules must hold at both. */
static const qint64 kFrameMs = 23;
static const qint64 kIdleMs = 2000;

namespace
{
/** Feeds an ArtNetSendHealth a timeline and tallies what it reports */
struct HealthDriver
{
    ArtNetSendHealth health;
    qint64 now = 0;
    int failingReports = 0;
    int recoveredReports = 0;
    qint64 failingAt = -1;
    qint64 recoveredAt = -1;

    void send(bool ok, bool hard = false)
    {
        switch (health.record(ok, now, hard))
        {
            case ArtNetSendHealth::StartedFailing:
                failingReports++;
                failingAt = now;
            break;
            case ArtNetSendHealth::Recovered:
                recoveredReports++;
                recoveredAt = now;
            break;
            case ArtNetSendHealth::NoChange:
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
    QVERIFY(d.failingAt - firstFailure >= ArtNetSendHealth::FailAfterMs);
    QVERIFY(d.failingAt - firstFailure < ArtNetSendHealth::FailAfterMs + kFrameMs);
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
    QVERIFY(d.recoveredAt - recoveryStart >= ArtNetSendHealth::RecoverAfterMs);
    QVERIFY(d.recoveredAt - recoveryStart < ArtNetSendHealth::RecoverAfterMs + kFrameMs);
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
    QVERIFY(r.recoveredAt - restart >= ArtNetSendHealth::RecoverAfterMs);

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
    QVERIFY(ArtNetSendHealth::isHardError(QAbstractSocket::DatagramTooLargeError));
    QVERIFY(ArtNetSendHealth::isHardError(QAbstractSocket::SocketAccessError));
    QVERIFY(ArtNetSendHealth::isHardError(QAbstractSocket::SocketAddressNotAvailableError));
    QVERIFY(ArtNetSendHealth::isHardError(QAbstractSocket::UnsupportedSocketOperationError));

    // Qt reports both "host is down" (ARP, comes and goes) and "no route" as
    // NetworkError, so it must go through the sustained rule
    QVERIFY(ArtNetSendHealth::isHardError(QAbstractSocket::NetworkError) == false);
    QVERIFY(ArtNetSendHealth::isHardError(QAbstractSocket::SocketResourceError) == false);
    QVERIFY(ArtNetSendHealth::isHardError(QAbstractSocket::TemporaryError) == false);
    QVERIFY(ArtNetSendHealth::isHardError(QAbstractSocket::UnknownSocketError) == false);
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

/*
  Q Light Controller Plus - qlcconsole
  tools/artnetprobe/main.cpp

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

/**
 * A bench instrument for the Art-Net paths that unit tests cannot reach.
 *
 * Three questions this fork's discovery work rests on, none of which a unit
 * test can answer because all three are about what the host's network stack
 * actually does:
 *
 *   1. Does a datagram arriving on a socket bound to 0.0.0.0:6454 carry a
 *      usable interface index? The plugin now routes every received packet by
 *      it (ArtNetPlugin::handlePacket) and DROPS what it cannot place. If this
 *      host reports 0, nodes stop appearing rather than appearing in the wrong
 *      place -- a different failure from the one that was fixed, and one worth
 *      knowing about before a show rather than during one.
 *
 *   2. Does a unicast ArtPoll leave correctly addressed? That is how an
 *      off-segment node -- one that never sees a broadcast poll -- is asked
 *      whether it is alive.
 *
 *   3. Does a universe patched at a specific node and port actually put ArtDMX
 *      on the wire with that port address? Point the console at 127.0.0.1 and
 *      this will say, with no rig present.
 *
 * Deliberately standalone: it binds the same port with the same flags as the
 * plugin, so it can run alongside the console rather than instead of it.
 *
 * TWO LIMITS, both learned the hard way and both about the same thing --
 * ShareAddress means shared, not duplicated:
 *
 *   - BROADCAST datagrams reach every socket bound to the port, so watching
 *     discovery traffic alongside a running console works fine.
 *   - UNICAST datagrams reach exactly ONE of them, chosen by the kernel. So
 *     this cannot see the console's own unicast output to a specific node
 *     while sharing a host with it. To check question 3 -- does a universe
 *     aimed at a node actually put ArtDMX on the wire with that port address
 *     -- run the probe on a DIFFERENT machine from the console.
 *
 * And --poll cannot prove an off-segment node answered THE UNICAST: a node on
 * a segment we can broadcast into answers broadcast polls too, and its reply
 * looks identical. It proves the poll goes out correctly addressed, and that
 * the address is alive. Isolating the off-segment path needs a node on a
 * subnet this host has no interface on.
 */

#include <QCoreApplication>
#include <QNetworkInterface>
#include <QNetworkDatagram>
#include <QCommandLineParser>
#include <QUdpSocket>
#include <QHostAddress>
#include <QTextStream>
#include <QElapsedTimer>
#include <QThread>

#include "artnetpacketizer.h"

#define ARTNET_PORT 6454

static QTextStream out(stdout);

static QString ifaceName(uint index)
{
    if (index == 0)
        return QString("(none reported)");
    const QNetworkInterface iface = QNetworkInterface::interfaceFromIndex(int(index));
    return iface.isValid() ? iface.humanReadableName()
                           : QString("index %1 (unknown)").arg(index);
}

/** Bind exactly as ArtNetPlugin::getUdpSocket() does, so results transfer. */
static bool bindLikeThePlugin(QUdpSocket &sock)
{
    if (sock.bind(ARTNET_PORT, QUdpSocket::ShareAddress
                               | QUdpSocket::ReuseAddressHint))
        return true;
    out << "FAIL  could not bind UDP " << ARTNET_PORT << ": "
        << sock.errorString() << "\n";
    return false;
}

static void describe(const QNetworkDatagram &dg)
{
    const QByteArray d = dg.data();
    QString kind = QString("non-Art-Net (%1 bytes)").arg(d.size());

    if (d.size() >= 10 && d.startsWith("Art-Net"))
    {
        const quint16 opcode = quint16(uchar(d[8])) | (quint16(uchar(d[9])) << 8);
        switch (opcode)
        {
        case 0x2000: kind = "ArtPoll"; break;
        case 0x2100: kind = "ArtPollReply"; break;
        case 0x5000:
            if (d.size() >= 18)
            {
                const quint16 port = quint16(uchar(d[14])) | (quint16(uchar(d[15])) << 8);
                kind = QString("ArtDMX port %1:%2:%3 (0x%4)")
                       .arg((port >> 8) & 0x7F).arg((port >> 4) & 0x0F)
                       .arg(port & 0x0F).arg(port, 4, 16, QChar('0'));
            }
            else
                kind = "ArtDMX (truncated)";
            break;
        default:
            kind = QString("Art-Net opcode 0x%1").arg(opcode, 4, 16, QChar('0'));
        }
    }

    out << "  from " << dg.senderAddress().toString().leftJustified(20)
        << " to " << dg.destinationAddress().toString().leftJustified(20)
        << " via " << ifaceName(uint(dg.interfaceIndex())).leftJustified(22)
        << " " << kind << "\n";
    out.flush();
}

/** Question 1, answerable on any host with no Art-Net gear present. */
static int selfTest()
{
    QUdpSocket listener;
    if (bindLikeThePlugin(listener) == false)
        return 2;

    out << "Sending to myself on every local IPv4 address, then reporting what\n"
           "arrives and whether it says which interface it came in on.\n\n";

    QList<QHostAddress> targets;
    targets << QHostAddress(QHostAddress::LocalHost);
    foreach (const QNetworkInterface &iface, QNetworkInterface::allInterfaces())
    {
        foreach (const QNetworkAddressEntry &e, iface.addressEntries())
        {
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol)
                targets << e.ip();
        }
    }

    QUdpSocket sender;
    ArtNetPacketizer packetizer;
    QByteArray poll;
    packetizer.setupArtNetPoll(poll);

    int sentOk = 0;
    foreach (const QHostAddress &addr, targets)
    {
        if (sender.writeDatagram(poll, addr, ARTNET_PORT) >= 0)
            sentOk++;
        else
            out << "  send to " << addr.toString() << " failed: "
                << sender.errorString() << "\n";
    }
    out << "Sent " << sentOk << " of " << targets.count() << " probes.\n\n";

    /* waitForReadyRead rather than sleeping: it blocks until the stack has
       something, so a slow first datagram is waited for instead of missed.
       Kept short and looped so the whole run stays under a second. */
    int seen = 0, withIndex = 0;
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < 1000)
    {
        if (listener.hasPendingDatagrams() == false)
        {
            if (listener.waitForReadyRead(200) == false)
                continue;
        }
        while (listener.hasPendingDatagrams())
        {
            const QNetworkDatagram dg = listener.receiveDatagram();
            if (dg.isValid() == false)
                continue;
            seen++;
            if (dg.interfaceIndex() != 0)
                withIndex++;
            describe(dg);
        }
    }

    out << "\n" << seen << " datagram(s) received, " << withIndex
        << " reporting an arrival interface.\n";

    if (seen == 0)
    {
        out << "INCONCLUSIVE  nothing came back -- a firewall may be dropping "
               "it, or another process holds the port exclusively.\n";
        return 1;
    }
    if (withIndex == 0)
    {
        out << "PROBLEM  this host reports no arrival interface. "
               "ArtNetPlugin::handlePacket falls back to subnet matching, and "
               "DROPS anything it cannot place -- so an off-segment node would "
               "go missing rather than be misfiled.\n";
        return 1;
    }
    out << "OK  arrival interface is reported, which is what packet "
           "attribution relies on.\n";
    return 0;
}

/** Question 2: does a unicast poll go out correctly addressed. */
static int pollOnce(const QString &address, int waitMs)
{
    const QHostAddress addr(address);
    if (addr.isNull())
    {
        out << "FAIL  \"" << address << "\" is not an IP address.\n";
        return 2;
    }

    QUdpSocket listener;
    if (bindLikeThePlugin(listener) == false)
        return 2;

    ArtNetPacketizer packetizer;
    QByteArray poll;
    packetizer.setupArtNetPoll(poll);

    QUdpSocket sender;
    const qint64 sent = sender.writeDatagram(poll, addr, ARTNET_PORT);
    if (sent < 0)
    {
        out << "FAIL  could not send: " << sender.errorString() << "\n";
        return 2;
    }
    out << "Sent a " << sent << "-byte ArtPoll to " << address
        << ", waiting " << waitMs << " ms for an answer.\n\n";

    QElapsedTimer clock;
    clock.start();
    int replies = 0;
    while (clock.elapsed() < waitMs)
    {
        QCoreApplication::processEvents();
        while (listener.hasPendingDatagrams())
        {
            const QNetworkDatagram dg = listener.receiveDatagram();
            if (dg.isValid() == false)
                continue;
            describe(dg);
            if (dg.senderAddress().isEqual(addr))
                replies++;
        }
        QThread::msleep(20);
    }

    out << "\n";
    if (replies > 0)
    {
        out << "OK  " << address << " answered.\n";
        /* Worth saying every time: on-segment, this proves reachability but
           not that the UNICAST was what reached it. */
        bool onSegment = false;
        foreach (const QNetworkInterface &iface, QNetworkInterface::allInterfaces())
        {
            foreach (const QNetworkAddressEntry &e, iface.addressEntries())
            {
                if (e.ip().protocol() == QAbstractSocket::IPv4Protocol
                        && addr.isInSubnet(e.ip(), e.prefixLength()))
                    onSegment = true;
            }
        }
        if (onSegment)
            out << "NOTE  that address is on a subnet this host has an "
                   "interface on, so it answers broadcast polls too -- this "
                   "does not isolate the unicast path.\n";
    }
    else
    {
        out << "NO ANSWER from " << address
            << " -- it is off, unreachable, or not Art-Net.\n";
    }
    return replies > 0 ? 0 : 1;
}

/** Question 3 (and general use): show everything that arrives. */
static int listen(int seconds)
{
    QUdpSocket listener;
    if (bindLikeThePlugin(listener) == false)
        return 2;

    out << "Listening on UDP " << ARTNET_PORT << " for " << seconds
        << "s. Point the console at a target and watch for ArtDMX with the "
           "port address you patched.\n\n";

    QElapsedTimer clock;
    clock.start();
    int count = 0;
    while (clock.elapsed() < seconds * 1000)
    {
        QCoreApplication::processEvents();
        while (listener.hasPendingDatagrams())
        {
            const QNetworkDatagram dg = listener.receiveDatagram();
            if (dg.isValid() == false)
                continue;
            describe(dg);
            count++;
        }
        QThread::msleep(20);
    }

    out << "\n" << count << " datagram(s) seen.\n";
    return count > 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("artnetprobe");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Art-Net bench instrument for qlcconsole.\n\n"
        "  --selftest      does this host report which interface a datagram\n"
        "                  arrived on? (needs no Art-Net hardware)\n"
        "  --poll <addr>   unicast an ArtPoll at one address and wait\n"
        "  --listen        show every Art-Net packet that arrives");
    parser.addHelpOption();

    QCommandLineOption selfOpt("selftest", "Check arrival-interface reporting.");
    QCommandLineOption pollOpt("poll", "Unicast an ArtPoll to <address>.", "address");
    QCommandLineOption listenOpt("listen", "Show arriving Art-Net packets.");
    QCommandLineOption secsOpt(QStringList() << "s" << "seconds",
                               "How long to wait (default 5).", "seconds", "5");
    parser.addOption(selfOpt);
    parser.addOption(pollOpt);
    parser.addOption(listenOpt);
    parser.addOption(secsOpt);
    parser.process(app);

    const int secs = parser.value(secsOpt).toInt();

    if (parser.isSet(selfOpt))
        return selfTest();
    if (parser.isSet(pollOpt))
        return pollOnce(parser.value(pollOpt), secs * 1000);
    if (parser.isSet(listenOpt))
        return listen(secs);

    parser.showHelp(0);
    return 0;
}

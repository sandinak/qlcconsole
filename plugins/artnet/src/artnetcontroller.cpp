/*
  Q Light Controller Plus
  artnetcontroller.cpp

  Copyright (c) Massimo Callegari

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

#include "artnetcontroller.h"
#include "rdmprotocol.h"

#include <QMutexLocker>
#include <QDateTime>
#include <QStringList>
#include <QDebug>

#define POLL_INTERVAL_MS   3000
#define SEND_INTERVAL_MS   2000

#define TRANSMIT_STANDARD  "Standard"
#define TRANSMIT_FULL      "Full"
#define TRANSMIT_PARTIAL   "Partial"

#define _DEBUG_RECEIVED_PACKETS 0

ArtNetController::ArtNetController(QNetworkInterface const& iface, QNetworkAddressEntry const& address,
                                   QSharedPointer<QUdpSocket> const& udpSocket,
                                   quint32 line, QObject *parent)
    : QObject(parent)
    , m_interface(iface)
    , m_address(address)
    , m_ipAddr(address.ip())
    , m_testSink(NULL)
    , m_testNowMs(-1)
    , m_packetSent(0)
    , m_packetReceived(0)
    , m_line(line)
    , m_udpSocket(udpSocket)
    , m_packetizer(new ArtNetPacketizer())
    , m_pollTimer(NULL)
{
    if (m_ipAddr == QHostAddress::LocalHost)
    {
        m_broadcastAddr = QHostAddress::LocalHost;
        m_MACAddress = "11:22:33:44:55:66";
    }
    else
    {
        m_broadcastAddr = address.broadcast();
        m_MACAddress = iface.hardwareAddress();
    }

    m_clock.start();

    qDebug() << "[ArtNetController] IP Address:" << m_ipAddr.toString() << " Broadcast address:" << m_broadcastAddr.toString() << "(MAC:" << m_MACAddress << ")";
}

ArtNetController::~ArtNetController()
{
    qDebug() << Q_FUNC_INFO;
}

ArtNetController::Type ArtNetController::type()
{
    int type = Unknown;
    foreach (UniverseInfo info, m_universeMap)
    {
        type |= info.type;
    }

    return Type(type);
}

quint32 ArtNetController::line()
{
    return m_line;
}

quint64 ArtNetController::getPacketSentNumber()
{
    return m_packetSent;
}

quint64 ArtNetController::getPacketReceivedNumber()
{
    return m_packetReceived;
}

bool ArtNetController::socketBound() const
{
    return m_udpSocket->state() == QAbstractSocket::BoundState;
}

QString ArtNetController::getNetworkIP()
{
    return m_ipAddr.toString();
}

QHash<QHostAddress, ArtNetNodeInfo> ArtNetController::getNodesList()
{
    return m_nodesList;
}

void ArtNetController::addUniverse(quint32 universe, ArtNetController::Type type)
{
    qDebug() << "[ArtNet] addUniverse - universe" << universe << ", type" << type;
    if (m_universeMap.contains(universe))
    {
        m_universeMap[universe].type |= (int)type;
    }
    else
    {
        UniverseInfo info;
        info.inputUniverse = universe;
        info.outputAddress = m_broadcastAddr;
        info.outputUniverse = universe;
        info.outputTransmissionMode = Standard;
        info.type = type;
        m_universeMap[universe] = info;
    }

    if (type == Output)
    {
        // activate ArtPoll if we open an Output
        if (m_pollTimer.isActive() == false)
        {
            m_pollTimer.setInterval(POLL_INTERVAL_MS);
            connect(&m_pollTimer, SIGNAL(timeout()), this, SLOT(slotPollTick()));
            m_pollTimer.start();

            slotPollTick();
        }
        if (m_sendTimer.isActive() == false &&
            m_universeMap[universe].outputTransmissionMode == Standard)
        {
            m_sendTimer.setInterval(SEND_INTERVAL_MS);
            connect(&m_sendTimer, SIGNAL(timeout()), this, SLOT(slotSendAllUniverses()));
            m_sendTimer.start();
        }
    }
}

void ArtNetController::removeUniverse(quint32 universe, ArtNetController::Type type)
{
    if (m_universeMap.contains(universe))
    {
        if (m_universeMap[universe].type == type)
            m_universeMap.take(universe);
        else
            m_universeMap[universe].type &= ~type;

        if (type == Output && ((this->type() & Output) == 0))
        {
            m_pollTimer.stop();
            disconnect(&m_pollTimer, SIGNAL(timeout()),
                       this, SLOT(slotPollTick()));
        }
    }
}

bool ArtNetController::setInputUniverse(quint32 universe, quint32 artnetUni)
{
    if (!m_universeMap.contains(universe))
        return false;

    QMutexLocker locker(&m_dataMutex);
    m_universeMap[universe].inputUniverse = artnetUni;

    return universe == artnetUni;
}

bool ArtNetController::setOutputIPAddress(quint32 universe, QString address)
{
    if (!m_universeMap.contains(universe))
        return false;

    /* A new target starts with a clean slate: the old one's failures say
       nothing about it. */
    if (address.size() == 0)
    {
        QMutexLocker locker(&m_dataMutex);
        m_universeMap[universe].outputAddress = m_broadcastAddr;
        m_universeMap[universe].sendHealth = SendHealth();
        locker.unlock();
        emit problemsChanged();
        return true;
    }

    QMutexLocker locker(&m_dataMutex);

    QHostAddress hostAddress(address);
    if (hostAddress.isNull() || !address.contains("."))
    {
        // IP addresses are now always fully saved
        qDebug() << "[setOutputIPAddress] Legacy IP style detected:" << address;
        QStringList iFaceIP = m_ipAddr.toString().split(".");
        QStringList addList = address.split(".");

        for (int i = 0; i < addList.count(); i++)
            iFaceIP.replace(4 - addList.count() + i , addList.at(i));

        QString newIP = iFaceIP.join(".");
        hostAddress = QHostAddress(newIP);
    }

    qDebug() << "[setOutputIPAddress] transmit to IP: " << hostAddress.toString();

    if (m_universeMap[universe].outputAddress != hostAddress)
        m_universeMap[universe].sendHealth = SendHealth();
    m_universeMap[universe].outputAddress = hostAddress;
    locker.unlock();
    emit problemsChanged();

    return hostAddress == m_broadcastAddr;
}

bool ArtNetController::setOutputUniverse(quint32 universe, quint32 artnetUni)
{
    if (!m_universeMap.contains(universe))
        return false;

    QMutexLocker locker(&m_dataMutex);
    m_universeMap[universe].outputUniverse = artnetUni;

    return universe == artnetUni;
}

bool ArtNetController::setTransmissionMode(quint32 universe, ArtNetController::TransmissionMode mode)
{
    if (!m_universeMap.contains(universe))
        return false;

    QMutexLocker locker(&m_dataMutex);
    m_universeMap[universe].outputTransmissionMode = int(mode);

    return mode == ArtNetController::Standard;
}

QString ArtNetController::transmissionModeToString(ArtNetController::TransmissionMode mode)
{
    switch (mode)
    {
        default:
        case Standard:
            return QString(TRANSMIT_STANDARD);
        break;
        case Full:
            return QString(TRANSMIT_FULL);
        break;
        case Partial:
            return QString(TRANSMIT_PARTIAL);
        break;
    }
}

ArtNetController::TransmissionMode ArtNetController::stringToTransmissionMode(const QString &mode)
{
    if (mode == QString(TRANSMIT_FULL))
        return Full;
    else if (mode == QString(TRANSMIT_PARTIAL))
        return Partial;
    else
        return Standard;
}

QList<quint32> ArtNetController::universesList()
{
    return m_universeMap.keys();
}

UniverseInfo *ArtNetController::getUniverseInfo(quint32 universe)
{
    if (m_universeMap.contains(universe))
        return &m_universeMap[universe];

    return NULL;
}

/**
 * Report a send result for one universe without flooding the log.
 *
 * Logs once when the universe starts failing and once when it has recovered,
 * as decided by SendHealth -- not per send (thousands of identical lines
 * a minute) and not per flip of the per-send result (a target that never
 * answers ARP alternates, and was reported failing/recovered every ~25 s
 * while never receiving a thing).
 */
bool ArtNetController::reportSendResult(UniverseInfo &info, quint32 universe,
                                        const QHostAddress &dest, bool ok)
{
    if (ok)
        m_packetSent++;

    const bool hard = ok == false && SendHealth::isHardError(m_udpSocket->error());

    switch (info.sendHealth.record(ok, nowMs(), hard))
    {
        case SendHealth::StartedFailing:
            qWarning() << "[ArtNet] universe" << (universe + 1)
                       << "output to" << dest.toString()
                       << "is failing:" << m_udpSocket->errorString()
                       << "- nothing patched to it will transmit."
                       << "Further failures on this universe will be counted, not logged;"
                       << "recovery is reported once sends have succeeded for"
                       << (SendHealth::RecoverAfterMs / 1000) << "s.";
        break;
        case SendHealth::Recovered:
            qWarning() << "[ArtNet] universe" << (universe + 1)
                       << "output to" << dest.toString()
                       << "recovered: sends have succeeded for"
                       << (SendHealth::RecoverAfterMs / 1000) << "s, after"
                       << info.sendHealth.failedSends() << "failed sends over"
                       << (info.sendHealth.failedForMs() / 1000) << "s.";
        break;
        case SendHealth::NoChange:
            return false;
    }
    return true;
}

void ArtNetController::slotSendAllUniverses()
{
    bool changed = false;
    QMutexLocker locker(&m_dataMutex);
    for (QMap<quint32, UniverseInfo>::iterator it = m_universeMap.begin(); it != m_universeMap.end(); ++it)
    {
        UniverseInfo &info = it.value();

        if ((info.type & Output) && info.outputTransmissionMode == Standard)
        {
            QByteArray dmxPacket;
            if (info.outputData.size() == 0)
                info.outputData.fill(0, 512);

            m_packetizer->setupArtNetDmx(dmxPacket, info.outputUniverse, info.outputData);

            qint64 sent = m_udpSocket->writeDatagram(dmxPacket, info.outputAddress, ARTNET_PORT);
            if (reportSendResult(info, it.key(), info.outputAddress, sent >= 0))
                changed = true;
        }
    }
    locker.unlock();
    if (changed)
        emit problemsChanged();
}

void ArtNetController::sendDmx(const quint32 universe, const QByteArray &data, bool dataChanged)
{
    QMutexLocker locker(&m_dataMutex);
    QByteArray dmxPacket;
    QHostAddress outAddress = m_broadcastAddr;
    quint32 outUniverse = universe;
    TransmissionMode transmitMode = Standard;
    UniverseInfo *info = getUniverseInfo(universe);

    if (info == NULL)
    {
        qWarning() << "sendDmx: universe" << universe << "not registered as output!";
        return;
    }

    outAddress = info->outputAddress;
    outUniverse = info->outputUniverse;
    transmitMode = TransmissionMode(info->outputTransmissionMode);

    // if data has not changed since previous tick don't do anything.
    // A timer will refresh all universes every N seconds
    if (transmitMode == Standard && !dataChanged)
        return;

    if (transmitMode == Full || (transmitMode == Standard && dataChanged))
    {
        if (info->outputData.size() == 0)
            info->outputData.fill(0, 512);

        info->outputData.replace(0, data.length(), data);
        m_packetizer->setupArtNetDmx(dmxPacket, outUniverse, info->outputData);
    }
    else
    {
        m_packetizer->setupArtNetDmx(dmxPacket, outUniverse, data);
    }

    qint64 sent = m_udpSocket->writeDatagram(dmxPacket, outAddress, ARTNET_PORT);
    const bool changed = reportSendResult(*info, universe, outAddress, sent >= 0);
    locker.unlock();
    if (changed)
        emit problemsChanged();
}

bool ArtNetController::sendRDMCommand(const quint32 universe, uchar command, QVariantList params)
{
    QByteArray rdmPacket;
    QHostAddress outAddress = m_broadcastAddr;
    quint32 outUniverse = universe;

    bool result = false;

    if (m_universeMap.contains(universe))
    {
        UniverseInfo info = m_universeMap[universe];
        outAddress = info.outputAddress;
        outUniverse = info.outputUniverse;
    }

    if (command == DISCOVERY_COMMAND)
    {
        // ArtNet doesn't care about mute/unmute
        if (params.length() >= 2)
        {
            quint16 pid = params.at(1).toUInt();
            if (pid == PID_DISC_MUTE || pid == PID_DISC_UN_MUTE)
                return false;
        }
        m_packetizer->setupArtNetTodRequest(rdmPacket, outUniverse);
    }
    else
    {
        m_packetizer->setupArtNetRdm(rdmPacket, outUniverse, command, params);
    }

    //qDebug() << "Sending RDM command" << rdmPacket.toHex(',');

    qint64 sent = m_udpSocket->writeDatagram(rdmPacket, outAddress, ARTNET_PORT);
    if (sent < 0)
    {
        qWarning() << "sendDmx failed";
        qWarning() << "Errno: " << m_udpSocket->error();
        qWarning() << "Errmgs: " << m_udpSocket->errorString();
    }
    else
    {
        m_packetSent++;
        result = true;
    }

    return result;
}

bool ArtNetController::handleArtNetPollReply(QByteArray const& datagram, QHostAddress const& senderAddress)
{
    ArtNetNodeInfo newNode;
    if (!m_packetizer->fillArtPollReplyInfo(datagram, newNode))
    {
        qWarning() << "[ArtNet] Bad ArtPollReply received";
        return false;
    }

#if _DEBUG_RECEIVED_PACKETS
    qDebug() << "[ArtNet] ArtPollReply received";
#endif

    /* Always refresh. This used to insert only on first sight, so a node that
       was renamed, re-addressed or changed status kept showing whatever it
       happened to be saying the first time we heard it -- despite announcing
       itself roughly once a second ever since. */
    m_nodesList[senderAddress] = newNode;

    ++m_packetReceived;

    if (m_nodeWatch.replyHeard(senderAddress, newNode, nowMs()))
    {
        logNodeIssueChanges();
        emit problemsChanged();
    }
    return true;
}

bool ArtNetController::handleArtNetPoll(QByteArray const& datagram, QHostAddress const& senderAddress)
{
    Q_UNUSED(datagram);

#if _DEBUG_RECEIVED_PACKETS
    qDebug() << "[ArtNet] ArtPoll received";
#endif
    QByteArray pollReplyPacket;
    for (QMap<quint32, UniverseInfo>::iterator it = m_universeMap.begin(); it != m_universeMap.end(); ++it)
    {
        quint32 universe = it.key();
        UniverseInfo &info = it.value();
        bool isInput = (info.type & Input) ? true : false;

        m_packetizer->setupArtNetPollReply(pollReplyPacket, m_ipAddr, m_MACAddress, universe, isInput);
        m_udpSocket->writeDatagram(pollReplyPacket, senderAddress, ARTNET_PORT);
        ++m_packetSent;
    }
    ++m_packetReceived;
    return true;
}

bool ArtNetController::handleArtNetDmx(QByteArray const& datagram, QHostAddress const& senderAddress)
{
    Q_UNUSED(senderAddress);

    QByteArray dmxData;
    quint32 artnetUniverse;
    if (!m_packetizer->fillDMXdata(datagram, dmxData, artnetUniverse))
    {
        qWarning() << "[ArtNet] Bad DMX packet received";
        return false;
    }

#if _DEBUG_RECEIVED_PACKETS
    qDebug() << "[ArtNet] DMX data received. Universe:" << artnetUniverse << ", Data size:" << dmxData.size()
        << ", data[0]=" << (int)dmxData[0]
        << ", from=" << QHostAddress(senderAddress.toIPv4Address()).toString();
#endif

    for (QMap<quint32, UniverseInfo>::iterator it = m_universeMap.begin(); it != m_universeMap.end(); ++it)
    {
        quint32 universe = it.key();
        UniverseInfo &info = it.value();

        if ((info.type & Input) && info.inputUniverse == artnetUniverse)
        {
            if (info.inputData.size() == 0)
                info.inputData.fill(0, 512);

#if _DEBUG_RECEIVED_PACKETS
            qDebug() << "[ArtNet] -> universe" << (universe + 1);
#endif

            for (int i = 0; i < dmxData.length(); i++)
            {
                if (info.inputData.at(i) != dmxData.at(i))
                {
#if _DEBUG_RECEIVED_PACKETS
                    qDebug() << "[ArtNet] a value differs";
#endif
                    info.inputData.replace(i, 1, (const char *)(dmxData.data() + i), 1);
                    emit valueChanged(universe, m_line, i, (uchar)dmxData.at(i));
                }
            }
            ++m_packetReceived;
            return true;
        }
    }
    return false;
}

bool ArtNetController::handleArtNetTodData(const QByteArray &datagram, const QHostAddress &senderAddress)
{
    QVariantMap values;
    quint32 universe;

    Q_UNUSED(senderAddress);

    if (m_packetizer->processTODdata(datagram, universe, values) == true)
    {
        emit rdmValueChanged(universe, m_line, values);
        return true;
    }

    return false;
}

bool ArtNetController::handleArtNetRDM(const QByteArray &datagram, const QHostAddress &senderAddress)
{
    QVariantMap values;
    quint32 universe;

    Q_UNUSED(senderAddress);

    if (m_packetizer->processRDMdata(datagram, universe, values) == true)
    {
        emit rdmValueChanged(universe, m_line, values);
        return true;
    }

    return false;
}

bool ArtNetController::handleArtNetIpProgReply(const QByteArray &datagram,
                                               const QHostAddress &senderAddress)
{
    ArtNetIpProgReply reply;
    if (m_packetizer->fillArtIpProgReply(datagram, reply) == false)
    {
        qWarning() << "[ArtNet] Bad ArtIpProgReply received from" << senderAddress.toString();
        return false;
    }
    ++m_packetReceived;
    qDebug() << "[ArtNet] ArtIpProgReply from" << senderAddress.toString()
             << "ip" << reply.ip.toString() << "mask" << reply.mask.toString()
             << "dhcp" << reply.dhcpEnabled;
    emit ipProgReplied(QHostAddress(senderAddress.toIPv4Address()).toString(), reply);
    return true;
}

bool ArtNetController::handlePacket(QByteArray const& datagram, QHostAddress const& senderAddress)
{
    //if (senderAddress.toIPv4Address() == m_ipAddr.toIPv4Address())
    //    return false;

#if _DEBUG_RECEIVED_PACKETS
    qDebug() << "Received packet with size: " << datagram.size() << ", host: " << QHostAddress(senderAddress.toIPv4Address()).toString();
#endif
    quint16 opCode = -1;

    if (m_packetizer->checkPacketAndCode(datagram, opCode) == true)
    {
        switch (opCode)
        {
            case ARTNET_POLLREPLY:
                return handleArtNetPollReply(datagram, senderAddress);
            case ARTNET_POLL:
                return handleArtNetPoll(datagram, senderAddress);
            case ARTNET_DMX:
                return handleArtNetDmx(datagram, senderAddress);
            case ARTNET_TODDATA:
                return handleArtNetTodData(datagram, senderAddress);
            case ARTNET_RDM:
                return handleArtNetRDM(datagram, senderAddress);
            case ARTNET_IPREPLY:
                return handleArtNetIpProgReply(datagram, senderAddress);
            default:
                qDebug().nospace().noquote() << "[ArtNet] opCode not supported yet (0x" << QString::number(opCode, 16) << ")";
                break;
        }
    }
    else
        qWarning() << "[ArtNet] Malformed packet received";

    return true;
}

void ArtNetController::sendPollTo(const QHostAddress &address, bool quiet)
{
    if (address.isNull())
        return;

    QByteArray pollPacket;
    m_packetizer->setupArtNetPoll(pollPacket);

    const qint64 sent = m_udpSocket->writeDatagram(pollPacket, address, ARTNET_PORT);
    if (sent < 0)
    {
        /* The periodic one stays quiet: whether this target is reachable is
           for the send health and the node watch to say, once -- not a
           warning every 3 s. */
        if (quiet == false)
            qWarning() << "Unable to send Poll packet to" << address.toString()
                       << ": errno=" << m_udpSocket->error()
                       << "(" << m_udpSocket->errorString() << ")";
    }
    else
    {
        m_packetSent++;
    }
}

void ArtNetController::slotSendPoll()
{
#if 0
    QList<QHostAddress>addressList;

    /* first, retrieve a list of unique output addresses */
    foreach (quint32 universe, universesList())
    {
        UniverseInfo info = m_universeMap[universe];
        if (addressList.contains(info.outputAddress) == false)
            addressList.append(info.outputAddress);
    }

    /* then send a poll to each address collected */
    foreach (QHostAddress addr, addressList)
    {
        QByteArray pollPacket;
        m_packetizer->setupArtNetPoll(pollPacket);
        qint64 sent = m_udpSocket->writeDatagram(pollPacket, addr, ARTNET_PORT);
        if (sent < 0)
            qWarning() << "Unable to send Poll packet: errno=" << m_udpSocket->error() << "(" << m_udpSocket->errorString() << ")";
        else
            m_packetSent++;
    }
#else
    QByteArray pollPacket;
    m_packetizer->setupArtNetPoll(pollPacket);

    qint64 sent = writeDatagram(QNetworkDatagram(pollPacket, m_broadcastAddr, ARTNET_PORT));
    if (sent < 0)
        qWarning() << "Unable to send Poll packet: errno=" << m_udpSocket->error() << "(" << m_udpSocket->errorString() << ")";
    else
        m_packetSent++;
#endif
}

/*********************************************************************
 * Output health
 *********************************************************************/

bool ArtNetController::isBroadcastTarget(const QHostAddress &address) const
{
    return address == m_broadcastAddr
        || address == QHostAddress(QHostAddress::Broadcast);
}

bool ArtNetController::isOnSubnet(const QHostAddress &address) const
{
    if (m_ipAddr.isLoopback())
        return address.isLoopback();
    return m_address.prefixLength() > 0
        && address.isInSubnet(m_ipAddr, m_address.prefixLength());
}

qint64 ArtNetController::writeDatagram(const QNetworkDatagram &dg)
{
    if (m_testSink != NULL)
    {
        m_testSink->append(dg);
        return dg.data().size();
    }
    return m_udpSocket->writeDatagram(dg);
}

bool ArtNetController::sendDiscoveryPoll()
{
    const QNetworkInterface::InterfaceFlags need =
            QNetworkInterface::IsUp | QNetworkInterface::CanBroadcast;
    if (m_ipAddr.isLoopback() || m_interface.index() <= 0
            || (m_interface.flags() & need) != need)
        return false;

    QByteArray pollPacket;
    m_packetizer->setupArtNetPoll(pollPacket);

    /* 255.255.255.255 is routed like any other destination -- out of the
       default route's interface -- unless the datagram names the interface.
       Naming it (IP_PKTINFO) puts it on THIS line's cable, and the source
       address keeps the node's answer coming back to this line. */
    QNetworkDatagram dg(pollPacket, QHostAddress(QHostAddress::Broadcast), ARTNET_PORT);
    dg.setInterfaceIndex(uint(m_interface.index()));
    dg.setSender(m_ipAddr);

    if (writeDatagram(dg) < 0)
    {
        qWarning() << "[ArtNet] Unable to send a discovery poll on"
                   << m_interface.humanReadableName() << ":" << m_udpSocket->errorString();
        return false;
    }
    m_packetSent++;
    return true;
}

bool ArtNetController::sendIpProg(const QHostAddress &node, const ArtNetIpProgRequest &request)
{
    QByteArray packet;
    m_packetizer->setupArtNetIpProg(packet, request);

    QNetworkDatagram dg(packet, node, ARTNET_PORT);
    qWarning() << "[ArtNet]" << (request.programs() ? "ArtIpProg (programming)" : "ArtIpProg (query)")
               << "to" << node.toString() << "via" << m_ipAddr.toString();
    if (writeDatagram(dg) < 0)
    {
        qWarning() << "[ArtNet] ArtIpProg to" << node.toString() << "failed:"
                   << m_udpSocket->errorString();
        return false;
    }
    m_packetSent++;
    return true;
}

void ArtNetController::slotPollTick()
{
    nodeIntervalElapsed();
    slotSendPoll();

    /* A unicast target off this line's subnet never sees the broadcast poll
       above. One unicast ArtPoll per such address per tick -- the same 3 s
       cadence, one small datagram each, and only for addresses a patch
       actually sends to. */
    QSet<QHostAddress> offSubnet;
    {
        QMutexLocker locker(&m_dataMutex);
        for (auto it = m_universeMap.constBegin(); it != m_universeMap.constEnd(); ++it)
        {
            const QHostAddress &addr = it.value().outputAddress;
            if ((it.value().type & Output) == 0 || addr.isNull()
                    || addr.isLoopback() || addr.isMulticast()
                    || isBroadcastTarget(addr))
                continue;
            if (m_address.prefixLength() > 0
                    && addr.isInSubnet(m_ipAddr, m_address.prefixLength()))
                continue;
            offSubnet.insert(addr);
        }
    }
    foreach (const QHostAddress &addr, offSubnet)
        sendPollTo(addr, true);
}

void ArtNetController::nodeIntervalElapsed()
{
    if (m_nodeWatch.pollSent(nowMs()))
    {
        logNodeIssueChanges();
        emit problemsChanged();
    }
}

static QString sinceText(qint64 sinceMs, qint64 nowMs)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime then = now.addMSecs(sinceMs - nowMs);
    return then.date() == now.date() ? then.toString("HH:mm")
                                     : then.toString("MMM d HH:mm");
}

QList<QPair<quint32, QString> > ArtNetController::outputProblems(quint32 universe)
{
    return problems(universe, true);
}

QList<QPair<quint32, QString> > ArtNetController::problems(quint32 universe, bool withSends)
{
    QList<QPair<quint32, QString> > out;
    const qint64 now = nowMs();

    QMutexLocker locker(&m_dataMutex);
    for (auto it = m_universeMap.constBegin(); it != m_universeMap.constEnd(); ++it)
    {
        const UniverseInfo &info = it.value();
        if ((info.type & Output) == 0)
            continue;
        if (universe != UINT_MAX && it.key() != universe)
            continue;

        if (withSends && info.sendHealth.isFailing())
            out << qMakePair(it.key(),
                             tr("Art-Net to %1: not reachable since %2")
                                 .arg(info.outputAddress.toString(),
                                      sinceText(info.sendHealth.failingSinceMs(), now)));

        foreach (const ArtNetNodeWatch::Issue &i,
                 m_nodeWatch.issuesFor(info.outputAddress,
                                       isBroadcastTarget(info.outputAddress),
                                       info.outputUniverse))
        {
            const QString node = i.name.isEmpty()
                ? i.ip : QString("%1 (%2)").arg(i.name, i.ip);
            const QString since = sinceText(i.sinceMs, now);
            QString text;
            switch (i.kind)
            {
                case ArtNetNodeWatch::Issue::NodeSilent:
                    text = tr("Art-Net node %1: stopped answering since %2").arg(node, since);
                break;
                case ArtNetNodeWatch::Issue::PortShort:
                    text = tr("Art-Net node %1, port %2 (%3): DMX output short since %4")
                               .arg(node).arg(i.port).arg(i.portLabel, since);
                break;
                case ArtNetNodeWatch::Issue::PortNotOutputting:
                    text = tr("Art-Net node %1, port %2 (%3): not outputting DMX since %4")
                               .arg(node).arg(i.port).arg(i.portLabel, since);
                break;
            }
            out << qMakePair(it.key(), text);
        }
    }
    return out;
}

void ArtNetController::logNodeIssueChanges()
{
    QSet<QString> now;
    typedef QPair<quint32, QString> Problem;
    foreach (const Problem &p, problems(UINT_MAX, false))
        now.insert(p.second);

    foreach (const QString &text, now)
        if (m_loggedNodeIssues.contains(text) == false)
            qWarning().noquote() << "[ArtNet]" << text;
    foreach (const QString &text, m_loggedNodeIssues)
        if (now.contains(text) == false)
            qWarning().noquote() << "[ArtNet] cleared:" << text;
    m_loggedNodeIssues = now;
}

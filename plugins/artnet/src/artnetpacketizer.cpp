/*
  Q Light Controller Plus
  artnetpacketizer.cpp

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

#include "artnetpacketizer.h"
#include "rdmprotocol.h"

#include <QStringList>
#include <QDebug>

ArtNetPacketizer::ArtNetPacketizer()
{
    // Initialize a commond header.
    // Changing only the tenth byte will create a valid ArtNet header
    m_commonHeader.clear();
    m_commonHeader.append(ARTNET_CODE_STR);
    m_commonHeader.append('\0');

    // empty opcode
    m_commonHeader.append('\0');
    m_commonHeader.append('\0');

    // version 14 by default
    m_commonHeader.append('\0');
    m_commonHeader.append((char)0x0e);

    m_sequence[0] = 1;
    m_sequence[1] = 1;
    m_sequence[2] = 1;
    m_sequence[3] = 1;
}

ArtNetPacketizer::~ArtNetPacketizer()
{
}

/*********************************************************************
 * Sender functions
 *********************************************************************/

void ArtNetPacketizer::setupArtNetPoll(QByteArray& data)
{
    data.clear();
    data.append(m_commonHeader);
    const char opCodeMSB = (ARTNET_POLL >> 8);
    data[9] = opCodeMSB;
    data.append((char)0x02); // TalkToMe
    data.append('\0'); // Priority
}

void ArtNetPacketizer::setupArtNetPollReply(QByteArray &data, QHostAddress ipAddr, QString MACaddr, quint32 universe, bool isInput)
{
    int i = 0;
    data.clear();
    data.append(m_commonHeader);
    data.remove(9, 2);
    const char opCodeMSB = (ARTNET_POLLREPLY >> 8);
    data[9] = opCodeMSB;
    QString ipStr = ipAddr.toString();
    QStringList ipAddrList = ipStr.split(".");
    foreach (QString val, ipAddrList)
        data.append((char)val.toInt()); // IP address[4]
    data.append((char)0x36);     // Port LSB
    data.append((char)0x19);     // Port MSB
    data.append((char)0x04);     // Version MSB
    data.append((char)0x20);     // Version LSB
    data.append((char)((universe >> 8) & 0xFF)); // NetSwitch (universe bits 14-8)Add commentMore actions
    data.append((char)((universe >> 4) & 0x0F)); // SubSwitch (universe bits 7-4)
    data.append((char)0xFF);     // OEM Value MSB
    data.append((char)0xFF);     // OEM Value LSB
    data.append((char)0x00);     // UBEA version
    data.append((char)0xF0);     // Status1 - Ready and booted
    data.append((char)0xFF);     // ESTA Manufacturer MSB
    data.append((char)0xFF);     // ESTA Manufacturer LSB

    data.append("QLC+");   // Short Name
    for (i = 0; i < 14; i++)
        data.append((char)0x00); // 14 bytes of stuffing
    data.append("Q Light Controller Plus - ArtNet interface"); // Long Name
    for (i = 0; i < 22; i++) // 64-42 bytes of stuffing. 42 is the length of the long name
        data.append((char)0x00);

    for (i = 0; i < 64; i++)
        data.append((char)0x00); // Node report
    data.append((char)0x00);     // NumPortsHi
    data.append((char)0x01);     // NumPortsLo
    data.append(isInput ? (char)0x40 : (char)0x80); // PortTypes[0]: can input or output DMX512 data
    data.append((char)0x00);     // PortTypes[1]: nothing
    data.append((char)0x00);     // PortTypes[2]: nothing
    data.append((char)0x00);     // PortTypes[3]: nothing

    data.append(isInput ? (char)0x80 : (char)0x00); // GoodInput[0] - input status port 1
    data.append((char)0x00);     // GoodInput[1] - input status port 2
    data.append((char)0x00);     // GoodInput[2] - input status port 3
    data.append((char)0x00);     // GoodInput[3] - input status port 4

    data.append(isInput ? (char)0x00 : (char)0x80); // GoodOutputA[0] - output status port 1
    data.append((char)0x00);     // GoodOutputA[0] - output status port 2
    data.append((char)0x00);     // GoodOutputA[0] - output status port 3
    data.append((char)0x00);     // GoodOutputA[0] - output status port 4

    data.append(isInput ? (char)universe : (char)0x00); // SwIn[0] - port 1
    data.append((char)0x00);     // SwIn[1] - port 2
    data.append((char)0x00);     // SwIn[2] - port 3
    data.append((char)0x00);     // SwIn[3] - port 4

    data.append(isInput ? (char)0x00 : (char)universe); // SwOut[0] - port 1
    data.append((char)0x00);     // SwOut[1] - port 2
    data.append((char)0x00);     // SwOut[2] - port 3
    data.append((char)0x00);     // SwOut[3] - port 4
    for (i = 0; i < 7; i++)
        data.append((char)0x00);  // SwVideo, SwMacro, SwRemote and 4 spare bytes
    /* Always six bytes: an interface without a hardware address (a VPN
       tunnel, some USB adapters) gave an empty string, one byte here, and a
       reply 5 bytes short with BindIp and Status2 out of place. */
    QStringList MAC = MACaddr.split(":");
    for (i = 0; i < 6; i++)
    {
        bool ok;
        data.append(i < MAC.count() ? (char)MAC.at(i).toInt(&ok, 16) : (char)0x00);
    }
    for (i = 0; i < 32; i++)
        data.append((char)0x00); // bindIp[4], BindIndex, Status2 and filler
}

void ArtNetPacketizer::setupArtNetDmx(QByteArray& data, const int &universe, const QByteArray &values)
{
    data.clear();
    data.append(m_commonHeader);
    const char opCodeMSB = (ARTNET_DMX >> 8);
    data[9] = opCodeMSB;
    /* Sequence runs 1..255 per Port-Address; 0 means "not sequenced". Only
       Port-Addresses 0-3 were seeded with 1, so any other universe's first
       packet went out as 0. */
    if (m_sequence.contains(universe) == false)
        m_sequence[universe] = 1;
    data.append(m_sequence[universe]); // Sequence
    data.append('\0'); // Physical
    data.append((char)(universe & 0x00FF));
    data.append((char)(universe >> 8));
    int padLength = values.isEmpty() ? 2 : (values.length() % 2); // length must be even in the range 2-512
    int len = values.length() + padLength;
    data.append((char)(len >> 8));
    data.append((char)(len & 0x00FF));
    data.append(values);
    data.append(QByteArray(padLength, 0));

    if (m_sequence[universe] == 0xff)
        m_sequence[universe] = 1;
    else
        m_sequence[universe]++;
}

void ArtNetPacketizer::setupArtNetTodRequest(QByteArray &data, const int &universe)
{
    data.clear();
    data.append(m_commonHeader);
    data[9] = char(ARTNET_TODREQUEST >> 8);
    data.append(char(0x00)); // Filler1
    data.append(char(0x00)); // Filler2
    data.append(char(0x00)); // Spare1
    data.append(char(0x00)); // Spare2
    data.append(char(0x00)); // Spare3
    data.append(char(0x00)); // Spare4
    data.append(char(0x00)); // Spare5
    data.append(char(0x00)); // Spare6
    data.append(char(0x00)); // Spare7

    data.append((char)(universe >> 8));     // Net
    data.append(char(0x00));                // Command: TodFull
    data.append(char(0x01));                // AddCount
    data.append((char)(universe & 0x00FF)); // Address
}

void ArtNetPacketizer::setupArtNetRdm(QByteArray &data, const int &universe, uchar command, QVariantList params)
{
    RDMProtocol rdm;
    QByteArray ba;

    data.clear();
    data.append(m_commonHeader);
    data[9] = char(ARTNET_RDM >> 8);
    data.append(char(0x01)); // RDM version 1.0
    data.append(char(0x00)); // Filler1
    data.append(char(0x00)); // Spare1
    data.append(char(0x00)); // Spare2
    data.append(char(0x00)); // Spare3
    data.append(char(0x00)); // Spare4
    data.append(char(0x00)); // Spare5
    data.append(char(0x00)); // Spare6
    data.append(char(0x00)); // Spare7

    data.append((char)(universe >> 8));     // Net
    data.append(char(0x00));                // ArProcess
    data.append((char)(universe & 0x00FF)); // Address

    rdm.packetizeCommand(command, params, false, ba);
    data.append(ba);
}

/*********************************************************************
 * Receiver functions
 *********************************************************************/

bool ArtNetPacketizer::checkPacketAndCode(QByteArray const& data, quint16 &code)
{
    /* An ArtNet header must be at least 12 bytes long */
    if (data.length() < 12)
        return false;

    /* Check "Art-Net" keyword presence */
    if (data.indexOf(ARTNET_CODE_STR) != 0)
        return false;

    if (data.at(7) != 0x00)
        return false;

    code = quint16((quint8(data.at(9)) << 8) | quint8(data.at(8)));

    return true;
}

/** Art-Net's fixed-width name fields are NUL-terminated C strings.
 *
 *  The previous code replaced NULs with spaces and simplified(), which assumes
 *  a node zero-fills the remainder of the field. Real hardware does not always
 *  oblige: the rig's CR041R leaves stale bytes after the terminator, and those
 *  survived into the displayed name as trailing garbage ("CR041R_001 <?> p<?>").
 *  Cut at the first NUL instead, which is what the field actually means. */
static QString artNetName(const QByteArray &raw)
{
    const int nul = raw.indexOf('\0');
    return QString::fromLatin1(nul >= 0 ? raw.left(nul) : raw).trimmed();
}

bool ArtNetPacketizer::fillArtPollReplyInfo(QByteArray const& data, ArtNetNodeInfo& info)
{
    /* A short reply is not necessarily malformed -- the spec has grown over
       time and older nodes send fewer trailing fields -- but everything below
       lives inside the first 212 bytes, so anything shorter cannot be parsed
       safely. */
    if (data.isNull() || data.size() < 212)
        return false;

    QByteArray shortName = data.mid(26, 18);
    QByteArray longName = data.mid(44, 64);
    QByteArray nodeReport = data.mid(108, 64);
    uchar inputStatus = uchar(data.at(178));

    info.shortName = artNetName(shortName);
    info.longName = artNetName(longName);
    info.portsNumber = (uchar(data.at(172)) << 8) + uchar(data.at(173));
    info.isInput = (inputStatus & 0x04) == 0 ? true : false;
    info.isOutput = (inputStatus & 0x04) ? true : false;
    info.universe = (ushort(data.at(18)) << 8) + (ushort(data.at(19)) << 4) + ushort(data.at(186));

    info.ipAddress = QString("%1.%2.%3.%4")
                        .arg(uchar(data.at(10))).arg(uchar(data.at(11)))
                        .arg(uchar(data.at(12))).arg(uchar(data.at(13)));

    info.firmwareVersion = (uchar(data.at(16)) << 8) + uchar(data.at(17));
    info.netSwitch = uchar(data.at(18));
    info.subSwitch = uchar(data.at(19));
    info.oemCode = (uchar(data.at(20)) << 8) + uchar(data.at(21));
    info.status1 = uchar(data.at(23));
    /* EstaMan is the one little-endian field in an otherwise big-endian
       packet. */
    info.estaCode = uchar(data.at(24)) + (uchar(data.at(25)) << 8);

    info.nodeReport = artNetName(nodeReport);
    /* The report is "#xxxx [count] text"; the hex code says whether the node
       accepted the last thing we asked of it -- 0x0006 RcShNameOk and 0x0007
       RcLoNameOk are how an ArtAddress write is acknowledged, since a node may
       report success without its advertised name changing. */
    info.reportCode = 0;
    {
        QString rep = info.nodeReport;
        int hash = rep.indexOf(QLatin1Char('#'));
        if (hash >= 0 && rep.size() >= hash + 5)
            info.reportCode = ushort(rep.mid(hash + 1, 4).toUShort(NULL, 16));
        else if (rep.size() >= 4)
            info.reportCode = ushort(rep.left(4).toUShort(NULL, 16));
    }

    for (int i = 0; i < 4; i++)
    {
        info.portTypes[i]  = uchar(data.at(174 + i));
        info.goodInput[i]  = uchar(data.at(178 + i));
        info.goodOutput[i] = uchar(data.at(182 + i));
        info.swIn[i]       = uchar(data.at(186 + i));
        info.swOut[i]      = uchar(data.at(190 + i));
    }

    /* Capability lives in Status1 bit 1. GoodOutput bit 3 is a different
       question entirely -- "RDM is disabled on this port" -- and reading it as
       a capability flag reports RDM support on nodes that have none. */
    info.rdmCapable = (info.status1 & 0x02) ? true : false;

    info.macAddress = QString("%1:%2:%3:%4:%5:%6")
        .arg(uchar(data.at(201)), 2, 16, QLatin1Char('0'))
        .arg(uchar(data.at(202)), 2, 16, QLatin1Char('0'))
        .arg(uchar(data.at(203)), 2, 16, QLatin1Char('0'))
        .arg(uchar(data.at(204)), 2, 16, QLatin1Char('0'))
        .arg(uchar(data.at(205)), 2, 16, QLatin1Char('0'))
        .arg(uchar(data.at(206)), 2, 16, QLatin1Char('0')).toUpper();

    /* Fields beyond 206 arrived in later Art-Net revisions; older nodes stop
       short, so default rather than read past the end. */
    info.bindIpAddress = QString();
    info.bindIndex = 0;
    info.status2 = 0;
    if (data.size() >= 211)
        info.bindIpAddress = QString("%1.%2.%3.%4")
                                .arg(uchar(data.at(207))).arg(uchar(data.at(208)))
                                .arg(uchar(data.at(209))).arg(uchar(data.at(210)));
    if (data.size() >= 212)
        info.bindIndex = uchar(data.at(211));
    if (data.size() >= 213)
        info.status2 = uchar(data.at(212));
    /* Status2 bit 1 is "the IP came from DHCP", bit 2 is "can do DHCP" (Art-Net
       4). Bit 1 was being read as the capability, so a node with a static
       address that could use DHCP read "static only", and one that was using
       DHCP read "capable" without saying it was in use. */
    info.dhcpConfigured = (info.status2 & 0x02) ? true : false;
    info.dhcpCapable = (info.status2 & 0x04) ? true : false;

    return true;
}


static void appendIPv4(QByteArray &data, const QHostAddress &addr)
{
    const quint32 v = addr.toIPv4Address();
    data.append(char((v >> 24) & 0xff));
    data.append(char((v >> 16) & 0xff));
    data.append(char((v >> 8) & 0xff));
    data.append(char(v & 0xff));
}

static QHostAddress readIPv4(const QByteArray &data, int at)
{
    return QHostAddress((quint32(uchar(data.at(at))) << 24)
                        | (quint32(uchar(data.at(at + 1))) << 16)
                        | (quint32(uchar(data.at(at + 2))) << 8)
                        | quint32(uchar(data.at(at + 3))));
}

void ArtNetPacketizer::setupArtNetIpProg(QByteArray &data, const ArtNetIpProgRequest &request)
{
    data.clear();
    data.append(m_commonHeader);            // ID, OpCode, ProtVer: 0..11
    data[8] = char(ARTNET_IPPROG & 0xff);
    data[9] = char(ARTNET_IPPROG >> 8);
    data.append('\0');                      // Filler1
    data.append('\0');                      // Filler2

    /* Command: bit 7 enables any programming at all, bit 6 DHCP (the node
       then ignores the rest), bit 4 gateway, bit 3 factory defaults, bit 2 IP,
       bit 1 mask, bit 0 port (deprecated, never sent). Zero is a query. */
    uchar command = 0;
    if (request.programs())
    {
        command |= 0x80;
        if (request.enableDhcp)
            command |= 0x40;
        else if (request.resetToDefaults)
            command |= 0x08;
        else
        {
            if (request.setGateway) command |= 0x10;
            if (request.setIp)      command |= 0x04;
            if (request.setMask)    command |= 0x02;
        }
    }
    data.append(char(command));             // Command: 14
    data.append('\0');                      // Filler4: 15
    appendIPv4(data, request.setIp ? request.ip : QHostAddress(quint32(0)));        // 16..19
    appendIPv4(data, request.setMask ? request.mask : QHostAddress(quint32(0)));    // 20..23
    data.append('\0');                      // ProgPortHi (deprecated): 24
    data.append('\0');                      // ProgPortLo: 25
    appendIPv4(data, request.setGateway ? request.gateway : QHostAddress(quint32(0))); // 26..29
    data.append(QByteArray(4, '\0'));       // Spare: 30..33
}

bool ArtNetPacketizer::fillArtIpProgReply(const QByteArray &data, ArtNetIpProgReply &reply)
{
    /* Up to and including Status (26) every revision agrees; ProgDg arrived
       with Art-Net 4, and an Art-Net 3 node sends spares there. */
    if (data.size() < 27)
        return false;

    reply.ip = readIPv4(data, 16);
    reply.mask = readIPv4(data, 20);
    reply.dhcpEnabled = (uchar(data.at(26)) & 0x40) ? true : false;
    reply.hasGateway = data.size() >= 32;
    reply.gateway = reply.hasGateway ? readIPv4(data, 28) : QHostAddress();
    return true;
}

/** ArtDmx: header (0-11), Sequence, Physical, SubUni, Net, LengthHi, Length. */
static const int kArtDmxHeaderSize = 18;

bool ArtNetPacketizer::fillDMXdata(QByteArray const& data, QByteArray &dmx, quint32 &universe)
{
    dmx.clear();
    /* Everything up to the Length field (16-17) must be there: a datagram
       from the network can be any size (fuzz, matrix 2: 12-17 bytes read
       past the end). */
    if (data.size() < kArtDmxHeaderSize)
        return false;
    //char sequence = data.at(12);
    //qDebug() << "Sequence: " << sequence;
    // char physical = data.at(13) // skipped
    universe = (quint32(quint8(data.at(15))) << 8) | quint8(data.at(14));

    unsigned int msb = quint8(data.at(16));
    unsigned int lsb = quint8(data.at(17));
    int length = int((msb << 8) | lsb);

    /* A universe is 512 channels whatever the packet claims; the receivers
       index a 512-byte buffer with what this returns. mid() stops at the
       end of a packet that holds less than it claims. */
    length = qMin(length, 512);

    //qDebug() << "length: " << length;
    dmx.append(data.mid(kArtDmxHeaderSize, length));
    return true;
}

bool ArtNetPacketizer::processTODdata(const QByteArray &data, quint32 &universe, QVariantMap &values)
{
    if (data.isNull() || data.length() < 28)
        return false;

    // 0 - 11 ArtNet header
    // 12 RDM version
    // 13 Port
    // 14 - 20 Spare
    // 21, 23 address
    universe = (quint32(quint8(data.at(21))) << 8) | quint8(data.at(23));
    // 22 Command response
    // 24 - 25 UID total
    //quint16 uidTotal = (quint8(data.at(24)) << 8) + quint8(data.at(25));
    // 26 BlockCount (consider only when total > 200)
    // 27 UID count
    quint8 uidCount = quint8(data.at(27));

    qDebug() << "UID count:" << uidCount;

    /* The count comes from the network: a packet promising more UIDs than
       it carries is malformed and dropped whole (fuzz, matrix 2). */
    if (data.length() < 28 + int(uidCount) * 6)
        return false;

    for (int i = 0; i < uidCount; i++)
    {
        quint16 ESTAId;
        quint32 deviceId;
        QString UID = RDMProtocol::byteArrayToUID(data.mid(28 + (i * 6), 6), ESTAId, deviceId);
        qDebug() << "UID:" << UID;
        values.insert(QString("UID-%1").arg(i), UID);
    }
    values.insert("DISCOVERY_COUNT", uidCount);

    return true;
}

bool ArtNetPacketizer::processRDMdata(const QByteArray &data, quint32 &universe, QVariantMap &values)
{
    if (data.isNull() || data.length() < 24)
        return false;

    // 0 - 11 ArtNet header
    // 12 RDM version
    // 13 - 20 zero fillers
    // 21, 23 address
    universe = (quint32(quint8(data.at(21))) << 8) | quint8(data.at(23));

    RDMProtocol rdm;
    return rdm.parsePacket(data.mid(24), values);
}


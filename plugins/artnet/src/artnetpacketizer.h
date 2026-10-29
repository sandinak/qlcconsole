/*
  Q Light Controller Plus
  artnetpacketizer.h

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

#include <QHostAddress>
#include <QByteArray>
#include <QString>
#include <QHash>

#ifndef ARTNETPACKETIZER_H
#define ARTNETPACKETIZER_H

#define ARTNET_POLL           0x2000
#define ARTNET_POLLREPLY      0x2100
#define ARTNET_DIAGDATA       0x2300
#define ARTNET_COMMAND        0x2400
#define ARTNET_DMX            0x5000
#define ARTNET_NZS            0x5100
#define ARTNET_ADDRESS        0x6000
#define ARTNET_INPUT          0x7000
#define ARTNET_TODREQUEST     0x8000
#define ARTNET_TODDATA        0x8100
#define ARTNET_TODCONTROL     0x8200
#define ARTNET_RDM            0x8300
#define ARTNET_RDMSUB         0x8400
#define ARTNET_VIDEOSTEUP     0xa010
#define ARTNET_VIDEOPALETTE   0xa020
#define ARTNET_VIDEODATA      0xa040
#define ARTNET_MACMASTER      0xf000
#define ARTNET_MACSLAVE       0xf100
#define ARTNET_FIRMWAREMASTER 0xf200
#define ARTNET_FIRMWAREREPLY  0xf300
#define ARTNET_FILETNMASTER   0xf400
#define ARTNET_FILEFNMASTER   0xf500
#define ARTNET_FILEFNREPLY    0xf600
#define ARTNET_IPPROG         0xf800
#define ARTNET_IPREPLY        0xf900
#define ARTNET_MEDIA          0x9000
#define ARTNET_MEDIAPATCH     0x9100
#define ARTNET_MEDIACONTROL   0x9200
#define ARTNET_MEDIACONTROLREPLY 0x9300
#define ARTNET_TIMECODE       0x9700
#define ARTNET_TIMESYNC       0x9800
#define ARTNET_TRIGGER        0x9900
#define ARTNET_DIRECTORY      0x9a00
#define ARTNET_DIRECTORYREPLY 0x9b00

#define ARTNET_CODE_STR "Art-Net"

/** Everything an ArtPollReply tells us about a node.
 *
 *  A node broadcasts this roughly once a second unprompted, and the console
 *  was keeping five fields and discarding the rest -- manufacturer, firmware,
 *  MAC, per-port addressing and status were all being received and thrown
 *  away, which is most of what an operator needs to answer "is that node
 *  configured the way I think it is". */
typedef struct
{
    QString shortName;
    QString longName;
    int portsNumber;
    bool isInput;
    bool isOutput;
    ushort universe;

    QString ipAddress;      //!< as reported by the node itself, not the sender
    QString macAddress;
    QString nodeReport;     //!< last status text, e.g. "0006 [7666] ArtNet"
    ushort reportCode;      //!< numeric part: 0x0006 == RcShNameOk, etc.
    ushort firmwareVersion;
    ushort oemCode;
    ushort estaCode;
    uchar netSwitch;        //!< Port-Address bits 14-8
    uchar subSwitch;        //!< Port-Address bits 7-4
    uchar status1;
    uchar status2;
    bool rdmCapable;        //!< Status1 bit 1 -- NOT GoodOutput bit 3
    bool dhcpConfigured;    //!< Status2 bit 1: its IP came from DHCP
    bool dhcpCapable;       //!< Status2 bit 2: it can use DHCP at all
    uchar bindIndex;
    QString bindIpAddress;
    /* Per physical port, up to 4 per ArtPollReply (a node with more ports
       sends several replies, distinguished by bindIndex). */
    uchar portTypes[4];
    uchar goodInput[4];
    uchar goodOutput[4];
    uchar swIn[4];
    uchar swOut[4];
} ArtNetNodeInfo;

/** What to ask of a node in an ArtIpProg (OpCode 0xf800) -- Art-Net 4's way
 *  of setting a node's IP address, subnet mask, gateway or DHCP, the packet
 *  DMX-Workshop sends when you change a node's address.
 *
 *  Each field is acted on only when its flag says so, so the default -- no
 *  flags -- asks for nothing and is a pure query: the node answers with an
 *  ArtIpProgReply carrying its current settings. */
struct ArtNetIpProgRequest
{
    ArtNetIpProgRequest()
        : setIp(false), setMask(false), setGateway(false)
        , enableDhcp(false), resetToDefaults(false)
    {}

    bool setIp;
    bool setMask;
    bool setGateway;
    bool enableDhcp;        //!< the node ignores the three above when set
    bool resetToDefaults;   //!< back to the node's factory IP / mask / gateway
    QHostAddress ip;
    QHostAddress mask;
    QHostAddress gateway;

    /** Does this change anything on the node (rather than only ask)? */
    bool programs() const
    { return setIp || setMask || setGateway || enableDhcp || resetToDefaults; }
};

/** A node's answer to ArtIpProg: its network settings as they now stand. */
struct ArtNetIpProgReply
{
    ArtNetIpProgReply() : dhcpEnabled(false), hasGateway(false) {}

    QHostAddress ip;
    QHostAddress mask;
    QHostAddress gateway;   //!< only when hasGateway (Art-Net 4 nodes)
    bool dhcpEnabled;
    bool hasGateway;
};

class ArtNetPacketizer final
{
    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    ArtNetPacketizer();
    ~ArtNetPacketizer();

public:
    /*********************************************************************
     * Sender functions
     *********************************************************************/
    /** Prepare an ArtNetPoll packet */
    void setupArtNetPoll(QByteArray& data);

    /** Prepare an ArtNetPollReply packet */
    void setupArtNetPollReply(QByteArray &data, QHostAddress ipAddr,
                              QString MACaddr, quint32 universe, bool isInput);

    /** Prepare an ArtNetDmx packet for @a universe (a Port-Address) sent to
     *  @a node. The Sequence counts per node and Port-Address: what one
     *  node receives runs 1, 2, 3..., however many other nodes this console
     *  also sends the same Port-Address to. */
    void setupArtNetDmx(QByteArray& data, const int& universe, const QByteArray &values,
                        const QHostAddress &node = QHostAddress());

    /** Prepare an ArtTodRequest packet */
    void setupArtNetTodRequest(QByteArray& data, const int& universe);

    /** Prepare an ArtRdm packet */
    void setupArtNetRdm(QByteArray& data, const int &universe, uchar command, QVariantList params);

    /** Prepare an ArtIpProg packet (Art-Net 4, 34 bytes). With a default
     *  request this is a query that changes nothing. */
    void setupArtNetIpProg(QByteArray& data, const ArtNetIpProgRequest &request);

    /*********************************************************************
     * Receiver functions
     *********************************************************************/

    /** Verify the validity of an ArtNet packet and store the opCode in 'code' */
    bool checkPacketAndCode(QByteArray const& data, quint16 &code);

    bool fillArtPollReplyInfo(QByteArray const& data, ArtNetNodeInfo& info);

    /** Process a ArtDmx packet and extract the relevant DMX data */
    bool fillDMXdata(QByteArray const& data, QByteArray& dmx, quint32 &universe);

    /** Process a ArtTodData packet and extract the relevant information */
    bool processTODdata(const QByteArray &data, quint32 &universe, QVariantMap &values);

    /** Process a ArtRdm packet and extract the relevant information */
    bool processRDMdata(const QByteArray &data, quint32 &universe, QVariantMap &values);

    /** Parse an ArtIpProgReply. Art-Net 3 nodes stop before the gateway. */
    bool fillArtIpProgReply(QByteArray const& data, ArtNetIpProgReply &reply);

private:
    QByteArray m_commonHeader;
    /** The next Sequence per (node IPv4 << 16 | Port-Address) */
    QHash<quint64, uchar> m_sequence;
};

#endif

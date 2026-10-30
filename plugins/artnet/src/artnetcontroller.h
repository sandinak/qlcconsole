/*
  Q Light Controller Plus
  artnetcontroller.h

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

#ifndef ARTNETCONTROLLER_H
#define ARTNETCONTROLLER_H

#include <QScopedPointer>
#include <QSharedPointer>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QVariant>
#include <QMutex>
#include <QElapsedTimer>
#include <QTimer>
#include <QMap>
#include <QSet>
#include <QPair>
#include <climits>
#include <atomic>

#include "artnetpacketizer.h"
#include "qlcioplugin.h"
#include "sendhealth.h"
#include "artnetnodewatch.h"
#include "artnetrdmflag.h"

#define ARTNET_PORT      6454

typedef struct _uinfo
{
    /** This is a bitmask to determine the capability
     *  of a Controller. Can be Input, Output or both */
    int type;

    /** When receiving input data, this is the universe
     *  the controller will process */
    ushort inputUniverse;

    /** Keeps the current dmx values to send only the ones that changed */
    /** It holds values for all the handled universes */
    QByteArray inputData;

    /** This is the destination IP address used when
     *  transmitting output data. Can be broadcast or unicast,
     *  including localhost. */
    QHostAddress outputAddress;

    /** When transmitting output data, this is the universe
     *  written on outgoing packets */
    ushort outputUniverse;

    /** This is the mode used to transmit output data.
     *  Enumerated in ArtNetController::TransmissionMode */
    int outputTransmissionMode;

    /** Universe data to be sent depending on the transmission mode */
    QByteArray outputData;

    /** Whether sends for this universe are failing, with hysteresis. A send
     *  can fail forever -- most commonly when the workspace names a bind
     *  interface this machine doesn't have -- and at DMX rates a warning per
     *  send produced thousands of identical lines a minute. A target that
     *  never answers ARP also fails and "succeeds" in turns, which a report
     *  per state change turned into a fail/recover pair every ~25 s.
     *  See SendHealth for the rules. */
    SendHealth sendHealth;
} UniverseInfo;

class ArtNetController final : public QObject
{
    Q_OBJECT

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    enum Type { Unknown = 0x0, Input = 0x01, Output = 0x02 };

    enum TransmissionMode { Standard, Full, Partial };

    ArtNetController(QNetworkInterface const& iface,
                     QNetworkAddressEntry const& address,
                     QSharedPointer<QUdpSocket> const& udpSocket,
                     quint32 line, QObject *parent = 0);

    ~ArtNetController();

    /** Send DMX data to a specific port/universe */
    void sendDmx(const quint32 universe, const QByteArray& data, bool dataChanged);

    /** Return the controller IP address */
    QString getNetworkIP();

    /** Returns the map of Nodes discovered by ArtPoll */
    QHash<QHostAddress, ArtNetNodeInfo> getNodesList();

    /** Add a universe to the map of this controller */
    void addUniverse(quint32 universe, Type type);

    /** Remove a universe from the map of this controller */
    void removeUniverse(quint32 universe, Type type);

    /** Set a specific ArtNet input universe for the given QLC+ universe.
     *  Return true if this restores default input universe */
    bool setInputUniverse(quint32 universe, quint32 artnetUni);

    /** Set a specific output IP address for the given QLC+ universe.
     *  Return true if this restores default output IP address */
    bool setOutputIPAddress(quint32 universe, QString address);

    /** Set a specific ArtNet output universe for the given QLC+ universe.
     *  Return true if this restores default output universe */
    bool setOutputUniverse(quint32 universe, quint32 artnetUni);

    /** Set the transmission mode of the ArtNet DMX packets over the network.
     *  It can be 'Full', which transmits always 512 channels, or
     *  'Partial', which transmits only the channels actually used in a
     *  universe.
     *  Return true if this restores default transmission mode */
    bool setTransmissionMode(quint32 universe, TransmissionMode mode);

    /** Converts a TransmissionMode value into a human readable string */
    static QString transmissionModeToString(TransmissionMode mode);

    /** Converts a human readable string into a TransmissionMode value */
    static TransmissionMode stringToTransmissionMode(const QString& mode);

    /** Return the list of the universes handled by
     *  this controller */
    QList<quint32> universesList();

    /** Return the specific information for the given universe */
    UniverseInfo *getUniverseInfo(quint32 universe);

    /** Return the global type of this controller */
    Type type();

    /** Return the plugin line associated to this controller */
    quint32 line();

    /** Get the number of packets sent by this controller */
    quint64 getPacketSentNumber();

    /** Get the number of packets received by this controller */
    quint64 getPacketReceivedNumber();

    /** Is the UDP socket capable of receiving packets ? */
    bool socketBound() const;

    /** This line's address is not on one of the show's rig networks while
     *  the gate is on (QLCIOPlugin::setTransmitNetworks()): it sends nothing.
     *  Only reads the cached flag: safe and free on any send path. */
    bool transmitHeld() const { return m_held.load(std::memory_order_relaxed); }

    /** Work transmitHeld() out again from the plugin's rig networks. Called
     *  when the line is made, and by ArtNetPlugin::refreshTransmitGate()
     *  (the networks changed; the engine's reconnect tick). */
    void refreshTransmitGate();

    /** Send a RDM command */
    bool sendRDMCommand(const quint32 universe, uchar command, QVariantList params);

    /** What is wrong with where each output universe goes, as
     *  (QLC+ universe, text) pairs -- "Art-Net to 172.18.2.50: not reachable
     *  since 20:14", "Art-Net node CR041R (172.18.2.10): stopped answering
     *  since 20:14", ... Only universes opened for output; @a universe limits
     *  it to one (UINT_MAX: all). Changes are announced by problemsChanged().
     *  See SendHealth and ArtNetNodeWatch for when each is said. */
    QList<QPair<quint32, QString> > outputProblems(quint32 universe = UINT_MAX);

    /** Is @a address one that every node on this line hears (the line's
     *  broadcast address, or the limited broadcast)? */
    bool isBroadcastTarget(const QHostAddress &address) const;

    /** Is @a address on this line's own subnet, i.e. can a unicast to it
     *  leave by this line's interface without a router? */
    bool isOnSubnet(const QHostAddress &address) const;

    /** Send ONE ArtPoll to the limited broadcast (255.255.255.255), out of
     *  this line's interface.
     *
     *  The periodic poll goes to the line's subnet broadcast (172.18.2.255),
     *  and a node that has not been given an address yet -- still on its
     *  factory 2.x.x.x or 10.x.x.x, or self-assigned 169.254.x.x after DHCP
     *  found no server -- is not on that subnet and is entitled to ignore it.
     *  Every IP stack accepts the limited broadcast, so this reaches every
     *  node on the cable whatever its address. Only on an explicit Rescan:
     *  it is the same one datagram per interface the subnet poll is, but it
     *  is not needed every 3 s. Returns false when the line cannot carry it
     *  (loopback). */
    bool sendDiscoveryPoll();

    /** Send an ArtIpProg to the node at @a node -- unicast, as Art-Net 4
     *  says. Only ever on an explicit operator action; ArtNetPlugin checks
     *  the node is on this line's subnet first. Returns false if the send
     *  failed. */
    bool sendIpProg(const QHostAddress &node, const ArtNetIpProgRequest &request);

private:
    /** Feed one send result to the universe's SendHealth and log only
     *  when its reported state changes. Returns true when it did. See the
     *  implementation. */
    bool reportSendResult(UniverseInfo &info, quint32 universe,
                          const QHostAddress &dest, bool ok);

    /** outputProblems(), optionally without the send-health ones (which
     *  reportSendResult() logs itself) */
    QList<QPair<quint32, QString> > problems(quint32 universe, bool withSends);

    /** Close one poll interval in the node watch; log and announce what
     *  changed. Split from the sending so it can be tested without a packet
     *  leaving the machine. */
    void nodeIntervalElapsed();

    /** Log node-watch issues that appeared or cleared since the last call */
    void logNodeIssueChanges();

    /** Write one datagram -- or, under test, record it and send nothing.
     *  Every packet this controller sends goes through here. Returns 0, and
     *  sends nothing, while transmitHeld(). */
    qint64 writeDatagram(const QNetworkDatagram &dg);

    /** The monotonic time SendHealth and ArtNetNodeWatch are fed */
    qint64 nowMs() const { return m_testNowMs >= 0 ? m_testNowMs : m_clock.elapsed(); }

    /** The network interface associated to this controller */
    QNetworkInterface m_interface;
    QNetworkAddressEntry m_address;
    /** The controller IP address as QHostAddress */
    QHostAddress m_ipAddr;

    /** The controller broadcast address as QHostAddress */
    /** This is where all ArtNet packets are sent to */
    QHostAddress m_broadcastAddr;

    QString m_MACAddress;

    /** Monotonic clock for SendHealth and the node watch */
    QElapsedTimer m_clock;

    /** When set, writeDatagram() appends here instead of sending. Tests only:
     *  nothing a test does should reach a real network. */
    QList<QNetworkDatagram> *m_testSink;

    /** With m_testSink: writeDatagram() records nothing and fails (-1), as
     *  a write to an unreachable target does. Tests only. */
    bool m_testSinkFails;

    /** When >= 0, the time nowMs() returns instead of the clock. Tests only. */
    qint64 m_testNowMs;

    /** Nodes heard on this line, and their health. GUI thread only. */
    ArtNetNodeWatch m_nodeWatch;

    /** The node issues last logged, so the log says each once */
    QSet<QString> m_loggedNodeIssues;

    /** Counter for transmitted packets */
    quint64 m_packetSent;

    /** Counter for received packets */
    quint64 m_packetReceived;

    /** QLC+ line to be used when emitting a signal */
    quint32 m_line;

    /** The UDP socket used to send/receive ArtNet packets */
    QSharedPointer<QUdpSocket> m_udpSocket;

    /** Helper class used to create or parse ArtNet packets */
    QScopedPointer<ArtNetPacketizer> m_packetizer;

    /** Map of the ArtNet nodes discovered with ArtPoll */
    QHash<QHostAddress, ArtNetNodeInfo> m_nodesList;

    /** Each node's RDM capability, held steady across its replies (a junk
     *  Status1 made the Devices row flicker) and proven by ArtTodData */
    QHash<QHostAddress, ArtNetRdmFlag> m_rdmFlags;

    /** Map of the QLC+ universes transmitted/received by this
     *  controller, with the related, specific parameters */
    QMap<quint32, UniverseInfo> m_universeMap;

    /** Mutex to handle the change of output IP address or in general
     *  variables that could be used to transmit/receive data */
    QMutex m_dataMutex;

    /** Timer used to send out an ArtPoll packet to
     *  discover available nodes on the network */
    QTimer m_pollTimer;

    /** The plugin whose rig network gate applies (the parent), or NULL */
    const QLCIOPlugin *m_gate;

    /** The gate's decision for this line, cached: written on the main thread
     *  by refreshTransmitGate(), read on every send (any thread) */
    std::atomic<bool> m_held;

    /** Timer used to send output data every N seconds
     *  when data is not changing */
    QTimer m_sendTimer;

private:
    bool handleArtNetPollReply(QByteArray const& datagram, QHostAddress const& senderAddress);
    bool handleArtNetPoll(QByteArray const& datagram, QHostAddress const& senderAddress);
    bool handleArtNetDmx(QByteArray const& datagram, QHostAddress const& senderAddress);
    bool handleArtNetTodData(QByteArray const& datagram, QHostAddress const& senderAddress);
    bool handleArtNetRDM(QByteArray const& datagram, QHostAddress const& senderAddress);
    bool handleArtNetIpProgReply(QByteArray const& datagram, QHostAddress const& senderAddress);
    bool handleArtNetTimeCode(QByteArray const& datagram, QHostAddress const& senderAddress);

public:
    // Handle a packet received to the ArtNet port.
    // Returns true if the packet has been handled,
    // or if the packet should not be handled by another controller.
    bool handlePacket(QByteArray const& datagram, QHostAddress const& senderAddress);

public:
    /** Send a single ArtPoll now, outside the periodic cadence.
     *
     *  Discovery is otherwise driven entirely by the 3 s poll timer, and that
     *  timer only runs on a line carrying an output (see addUniverse). An
     *  operator who has just plugged a node in wants the answer now rather
     *  than at the next tick -- and on an unpatched line there is no next
     *  tick at all. One datagram, on demand.
     */
    void sendPoll() { slotSendPoll(); }

    /** Send one ArtPoll to a specific address rather than the broadcast one.
     *
     *  A node on a subnet this console has no interface on never sees a
     *  broadcast poll, so it can never answer one -- which is exactly the node
     *  somebody had to declare by hand, and exactly the node they most want to
     *  know is alive. Unicast reaches it through the router if anything can. */
    void sendPollTo(const QHostAddress &address, bool quiet = false);

protected slots:
    void slotSendPoll();

    /** The periodic poll: judge the interval that just ended, then poll --
     *  broadcast on this line, plus one unicast ArtPoll to each unicast
     *  output target OFF this line's subnet (a broadcast never reaches those,
     *  so without it they could never be known, let alone missed). */
    void slotPollTick();
    void slotSendAllUniverses();

signals:
    void valueChanged(quint32 universe, quint32 input, quint32 channel, uchar value);

    void rdmValueChanged(quint32 universe, quint32 line, QVariantMap data);

    /** An ArtTimeCode arrived on a line with an input patched: the position
     *  for the show's timecode follow, the same path MTC takes. Emitted
     *  once, on the line's first input universe, however many it has. */
    void timeCodeChanged(quint32 universe, quint32 input, quint32 msPosition, uchar fps);

    /** outputProblems() has changed. May be emitted from the thread that
     *  sends DMX; never with the data mutex held. */
    void problemsChanged();

    /** A node answered an ArtIpProg with its current network settings */
    void ipProgReplied(const QString &sender, const ArtNetIpProgReply &reply);
};

#endif

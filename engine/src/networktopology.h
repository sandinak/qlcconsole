/*
  Q Light Controller Plus
  networktopology.h

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

#ifndef NETWORKTOPOLOGY_H
#define NETWORKTOPOLOGY_H

#include <QStringList>
#include <QString>
#include <QVector3D>
#include <QPointF>
#include <QHash>
#include <QList>

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCNetworkTopology QStringLiteral("NetworkTopology")

/**
 * A wireless DMX receiver: a physically separate box from its transmitting
 * NetworkNode, at its own rig position. Only meaningful on a port belonging
 * to a node with transport == WirelessDMX — see NETWORK_CABLING_DESIGN.md's
 * "Wireless DMX" section for why the receiver needs a position distinct from
 * the transmitter's (e.g. a transmitter at FOH relaying to a receiver
 * actually mounted in the rig).
 */
class NetworkReceiver
{
public:
    NetworkReceiver();

    QString name;
    bool    placed;
    double  posX;
    double  posY;
};

/**
 * A single physical DMX output on a NetworkNode. Carries the universe it
 * breaks out; a wireless port additionally carries its own NetworkReceiver.
 */
class NetworkPort
{
public:
    NetworkPort();

    quint32 universe;          //!< InputOutputMap::invalidUniverse() == unused
    NetworkReceiver receiver;  //!< valid only when receiver.placed (wireless ports)
};

/**
 * An ArtNet/sACN/interface gateway or a wireless DMX transmitter: gear that
 * carries universes out to a run of DMX fixtures. Placed on the 2D stage plot
 * like a PowerSource. Fixtures that speak the network natively are NOT nodes --
 * they carry Fixture::connection == Network and their own IP.
 */
class NetworkNode
{
public:
    /** How this node reaches its universes. */
    enum Transport
    {
        ArtNet      = 0,
        sACN        = 1,
        Interface   = 2,
        WirelessDMX = 3
    };

    NetworkNode();

    QString name;
    int     transport;         //!< Transport
    QString ipHint;              //!< informational only
    QList<NetworkPort> ports;

    // Physical location on the 2D stage plot (metres), same shape as
    // PowerSource's placement fields. For a WirelessDMX node this is the
    // TRANSMITTER's location — see NetworkPort::receiver for the location
    // that actually matters for proximity purposes.
    bool    placed;
    double  posX;
    double  posY;
    bool    locked;
    quint32 layerId;
    quint32 groupId;

    bool isWireless() const { return transport == WirelessDMX; }

    /** Display name for a Transport ("ArtNet", "sACN", "Interface", "Wireless DMX"). */
    static QString transportName(int transport);
};

/**
 * The DMX cable run one port feeds: from its anchor (the node, or for a wireless
 * port the receiver) to the fixtures on its universe, in address order -- the
 * order a daisy chain is patched in. Derived on demand from the topology, the
 * patch and the plot, never stored.
 */
struct NetworkRun
{
    NetworkRun() : nodeIndex(-1), portIndex(-1), universe(0), wireless(false),
                   anchored(false), lengthMetres(0), channels(0) {}

    int     nodeIndex;
    int     portIndex;
    quint32 universe;
    QString nodeName;
    QString receiverName;             //!< wireless ports: the receiver the run starts at
    bool    wireless;
    bool    anchored;                 //!< the node/receiver has a position on the plot

    QList<quint32> fixtures;          //!< every patched DMX fixture on the universe, by address
    QList<quint32> plotted;           //!< those with a position, same order: the polyline's vertices
    QList<QVector3D> points;          //!< anchor (on the floor) then each plotted fixture, metres

    double  lengthMetres;             //!< straight-line estimate along points; real cable is longer
    int     channels;                 //!< channels used on the universe by fixtures
};

/**
 * Per-document physical network/cabling model: a set of nodes, each with
 * ports, each port carrying a universe. Owned by Doc and serialized into the
 * workspace. Derivation-first (see NETWORK_CABLING_DESIGN.md): which
 * fixtures are on a port, run polylines and port loading are computed from
 * this plus Doc::fixtures() wherever they're needed, never stored here.
 */
class NetworkTopology
{
public:
    NetworkTopology();

    /** Drop all nodes (called when a new workspace is created) */
    void reset();

    QList<NetworkNode>& nodes() { return m_nodes; }
    const QList<NetworkNode>& nodes() const { return m_nodes; }

    /** The proximity anchor (metres) for a node/port pair: the port's
     *  wireless receiver position when it's placed, else the node's own
     *  position. Returns false (leaving x/y untouched) if neither is
     *  placed — the caller has nothing to anchor to. */
    static bool proximityAnchor(const NetworkNode &node, const NetworkPort &port,
                                 double &x, double &y);

    /** The node/port carrying a given universe, or -1/-1 if none does. */
    void portForUniverse(quint32 universe, int &nodeIdx, int &portIdx) const;

    /** Whether an output plugin name is consistent with a node transport.
     *  ArtNet expects the "ArtNet" plugin, sACN expects "E1.31". Interface
     *  and WirelessDMX nodes (USB/serial boxes, wireless links) can sit behind
     *  any plugin, and an empty name means the universe isn't patched to an
     *  output yet -- neither is a mismatch. */
    static bool transportMatchesPlugin(int transport, const QString &pluginName);

    /** Human-readable problems with the topology: a universe carried by more
     *  than one port, a node whose transport disagrees with the output plugin
     *  its universe is actually patched to (skipped when @p doc is NULL), and
     *  wireless ports with no receiver placed. Empty when everything is fine. */
    QStringList warnings(Doc *doc) const;

    /** The run each port with a universe feeds. Only patched fixtures whose
     *  connection is DMX are on a run: a network fixture is its own endpoint and
     *  an inventory fixture has no address. @p fixturePositions maps fixture id
     *  to its position in metres for the fixtures that are on the plot; the rest
     *  still count toward the run's fixtures and channels but have no vertex. */
    QList<NetworkRun> runs(Doc *doc, const QHash<quint32, QVector3D> &fixturePositions) const;

    /** Patched DMX fixtures on a universe that no node port carries: they have
     *  no physical path to the console. */
    QList<quint32> unservedFixtures(Doc *doc) const;

    bool loadXML(QXmlStreamReader &root, Doc *doc);
    bool saveXML(QXmlStreamWriter *doc) const;

private:
    QList<NetworkNode> m_nodes;
};

/** @} */

#endif

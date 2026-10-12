/*
  Q Light Controller Plus
  networktopology.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>
#include <algorithm>
#include <QHash>
#include <QSet>

#include "networktopology.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "fixture.h"
#include "doc.h"

#define KXMLQLCNetworkNode              QStringLiteral("Node")
#define KXMLQLCNetworkNodeName          QStringLiteral("Name")
#define KXMLQLCNetworkNodeTransport     QStringLiteral("Transport")
#define KXMLQLCNetworkNodeIpHint        QStringLiteral("IpHint")
#define KXMLQLCNetworkNodeModel         QStringLiteral("Model")
#define KXMLQLCNetworkNodePosX          QStringLiteral("PosX")
#define KXMLQLCNetworkNodePosY          QStringLiteral("PosY")
#define KXMLQLCNetworkNodeLocked        QStringLiteral("Locked")
#define KXMLQLCNetworkNodeLayerId       QStringLiteral("LayerId")
#define KXMLQLCNetworkNodeGroupId       QStringLiteral("GroupId")
#define KXMLQLCNetworkPort              QStringLiteral("Port")
#define KXMLQLCNetworkPortUniverse      QStringLiteral("Universe")
#define KXMLQLCNetworkReceiver          QStringLiteral("Receiver")
#define KXMLQLCNetworkReceiverName      QStringLiteral("Name")
#define KXMLQLCNetworkReceiverPosX      QStringLiteral("PosX")
#define KXMLQLCNetworkReceiverPosY      QStringLiteral("PosY")

/****************************************************************************
 * NetworkReceiver / NetworkPort / NetworkNode
 ****************************************************************************/

NetworkReceiver::NetworkReceiver()
    : name(QStringLiteral("Receiver"))
    , placed(false)
    , posX(0)
    , posY(0)
{
}

NetworkPort::NetworkPort()
    : universe(InputOutputMap::invalidUniverse())
{
}

NetworkNode::NetworkNode()
    : name(QStringLiteral("Node"))
    , transport(ArtNet)
    , placed(false)
    , posX(0)
    , posY(0)
    , locked(false)
    , layerId(0)
    , groupId(0)
{
}

QString NetworkNode::transportName(int transport)
{
    switch (transport)
    {
        case sACN:        return QStringLiteral("sACN");
        case Interface:   return QStringLiteral("Interface");
        case WirelessDMX: return QStringLiteral("Wireless DMX");
        case ArtNet:
        default:          return QStringLiteral("ArtNet");
    }
}

/****************************************************************************
 * NetworkTopology
 ****************************************************************************/

NetworkTopology::NetworkTopology()
{
}

void NetworkTopology::reset()
{
    m_nodes.clear();
}

bool NetworkTopology::proximityAnchor(const NetworkNode &node, const NetworkPort &port,
                                       double &x, double &y)
{
    if (node.isWireless() && port.receiver.placed)
    {
        x = port.receiver.posX;
        y = port.receiver.posY;
        return true;
    }
    if (node.placed)
    {
        x = node.posX;
        y = node.posY;
        return true;
    }
    return false;
}

void NetworkTopology::portForUniverse(quint32 universe, int &nodeIdx, int &portIdx) const
{
    nodeIdx = -1;
    portIdx = -1;
    for (int n = 0; n < m_nodes.size(); n++)
    {
        for (int p = 0; p < m_nodes[n].ports.size(); p++)
        {
            if (m_nodes[n].ports[p].universe == universe)
            {
                nodeIdx = n;
                portIdx = p;
                return;
            }
        }
    }
}

bool NetworkTopology::transportMatchesPlugin(int transport, const QString &pluginName)
{
    if (pluginName.isEmpty())
        return true;
    switch (transport)
    {
        case NetworkNode::ArtNet: return pluginName == QLatin1String("ArtNet");
        case NetworkNode::sACN:   return pluginName == QLatin1String("E1.31");
        default:                  return true;
    }
}

QStringList NetworkTopology::warnings(Doc *doc) const
{
    QStringList out;
    QHash<quint32, QStringList> carriers;   // universe -> "Node / port N"

    for (int n = 0; n < m_nodes.size(); n++)
    {
        const NetworkNode &node = m_nodes.at(n);
        for (int p = 0; p < node.ports.size(); p++)
        {
            const NetworkPort &port = node.ports.at(p);
            const QString where = QStringLiteral("%1, port %2").arg(node.name).arg(p + 1);

            if (node.isWireless() && !port.receiver.placed)
                out << QStringLiteral("%1: no receiver placed -- proximity falls back to the "
                                      "transmitter's position").arg(where);

            if (port.universe == InputOutputMap::invalidUniverse())
                continue;
            carriers[port.universe] << where;

            if (doc == NULL)
                continue;
            InputOutputMap *io = doc->inputOutputMap();
            for (quint32 i = 0; i < io->universesCount(); i++)
            {
                if (io->getUniverseID(i) != port.universe)
                    continue;
                OutputPatch *op = io->outputPatch(i);
                const QString plugin = (op != NULL) ? op->pluginName() : QString();
                if (!transportMatchesPlugin(node.transport, plugin))
                    out << QStringLiteral("%1: node is %2 but universe %3 is patched to the "
                                          "%4 output").arg(where, NetworkNode::transportName(node.transport))
                                                      .arg(port.universe + 1).arg(plugin);
                break;
            }
        }
    }

    QList<quint32> unis = carriers.keys();
    std::sort(unis.begin(), unis.end());
    foreach (quint32 uni, unis)
    {
        if (carriers.value(uni).size() > 1)
            out << QStringLiteral("Universe %1 is carried by more than one port: %2")
                       .arg(uni + 1).arg(carriers.value(uni).join(QStringLiteral("; ")));
    }
    return out;
}

QList<NetworkRun> NetworkTopology::runs(Doc *doc, const QHash<quint32, QVector3D> &fixturePositions) const
{
    QList<NetworkRun> out;
    if (doc == NULL)
        return out;

    for (int n = 0; n < m_nodes.size(); n++)
    {
        const NetworkNode &node = m_nodes.at(n);
        for (int p = 0; p < node.ports.size(); p++)
        {
            const NetworkPort &port = node.ports.at(p);
            if (port.universe == InputOutputMap::invalidUniverse())
                continue;

            NetworkRun run;
            run.nodeIndex = n;
            run.portIndex = p;
            run.universe = port.universe;
            run.nodeName = node.name;
            run.wireless = node.isWireless();
            if (run.wireless && port.receiver.placed)
                run.receiverName = port.receiver.name;

            double ax = 0, ay = 0;
            run.anchored = proximityAnchor(node, port, ax, ay);

            QList<Fixture *> onPort;
            foreach (Fixture *fx, doc->fixtures())
            {
                if (fx == NULL || !fx->isPatched() || fx->connection() != Fixture::Dmx
                    || fx->universe() != port.universe)
                    continue;
                onPort << fx;
            }
            std::sort(onPort.begin(), onPort.end(), [](Fixture *a, Fixture *b) {
                return a->address() != b->address() ? a->address() < b->address() : a->id() < b->id();
            });

            if (run.anchored)
                run.points << QVector3D(float(ax), float(ay), 0.0f);
            foreach (Fixture *fx, onPort)
            {
                run.fixtures << fx->id();
                run.channels += int(fx->channels());
                if (fixturePositions.contains(fx->id()))
                {
                    run.plotted << fx->id();
                    run.points << fixturePositions.value(fx->id());
                }
            }
            // Without an anchor there is nothing to run FROM: the fixtures are
            // listed, but no polyline or length is claimed.
            if (!run.anchored)
                run.points.clear();
            for (int i = 1; i < run.points.size(); i++)
                run.lengthMetres += double((run.points.at(i) - run.points.at(i - 1)).length());
            out << run;
        }
    }
    return out;
}

int NetworkTopology::placeReceivers(int nodeIndex, Doc *doc,
                                    const QHash<quint32, QVector3D> &fixturePositions)
{
    if (nodeIndex < 0 || nodeIndex >= m_nodes.size())
        return 0;
    NetworkNode &node = m_nodes[nodeIndex];
    if (!node.isWireless())
        return 0;
    int placed = 0;
    for (int p = 0; p < node.ports.size(); ++p)
    {
        NetworkPort &port = node.ports[p];
        if (port.receiver.placed)
            continue;
        if (port.receiver.name.isEmpty() || port.receiver.name == QLatin1String("Receiver"))
            port.receiver.name = QStringLiteral("%1 RX %2").arg(node.name).arg(p + 1);

        // Where its universe's fixtures are, when there are any on the plot.
        double sx = 0, sy = 0;
        int n = 0;
        if (doc != nullptr && port.universe != InputOutputMap::invalidUniverse())
        {
            foreach (Fixture *fx, doc->fixtures())
            {
                if (fx == nullptr || !fx->isPatched() || fx->universe() != port.universe
                    || !fixturePositions.contains(fx->id()))
                    continue;
                sx += fixturePositions.value(fx->id()).x();
                sy += fixturePositions.value(fx->id()).y();
                ++n;
            }
        }
        if (n > 0)
        {
            port.receiver.posX = sx / n;
            port.receiver.posY = sy / n;
        }
        else
        {
            // Nothing to be near yet: beside the transmitter, out of its way.
            port.receiver.posX = node.posX + 1.0 + 0.75 * p;
            port.receiver.posY = node.posY;
        }
        port.receiver.placed = true;
        ++placed;
    }
    return placed;
}

QList<quint32> NetworkTopology::unservedFixtures(Doc *doc) const
{
    QList<quint32> out;
    if (doc == NULL)
        return out;
    QSet<quint32> carried;
    foreach (const NetworkNode &node, m_nodes)
        foreach (const NetworkPort &port, node.ports)
            if (port.universe != InputOutputMap::invalidUniverse())
                carried.insert(port.universe);
    foreach (Fixture *fx, doc->fixtures())
        if (fx != NULL && fx->isPatched() && fx->connection() == Fixture::Dmx
            && !carried.contains(fx->universe()))
            out << fx->id();
    return out;
}

/****************************************************************************
 * Load & Save
 ****************************************************************************/

bool NetworkTopology::loadXML(QXmlStreamReader &root, Doc *doc)
{
    Q_UNUSED(doc)

    if (root.name() != KXMLQLCNetworkTopology)
    {
        qWarning() << Q_FUNC_INFO << "Network topology node not found";
        return false;
    }

    m_nodes.clear();

    while (root.readNextStartElement())
    {
        if (root.name() != KXMLQLCNetworkNode)
        {
            qWarning() << Q_FUNC_INFO << "Unknown network tag:" << root.name();
            root.skipCurrentElement();
            continue;
        }

        NetworkNode node;
        QXmlStreamAttributes nattrs = root.attributes();
        if (nattrs.hasAttribute(KXMLQLCNetworkNodeTransport))
            node.transport = nattrs.value(KXMLQLCNetworkNodeTransport).toString().toInt();
        if (nattrs.hasAttribute(KXMLQLCNetworkNodeIpHint))
            node.ipHint = nattrs.value(KXMLQLCNetworkNodeIpHint).toString();
        if (nattrs.hasAttribute(KXMLQLCNetworkNodeModel))
            node.model = nattrs.value(KXMLQLCNetworkNodeModel).toString();
        if (nattrs.hasAttribute(KXMLQLCNetworkNodePosX) &&
            nattrs.hasAttribute(KXMLQLCNetworkNodePosY))
        {
            node.placed = true;
            node.posX = nattrs.value(KXMLQLCNetworkNodePosX).toString().toDouble();
            node.posY = nattrs.value(KXMLQLCNetworkNodePosY).toString().toDouble();
        }
        if (nattrs.hasAttribute(KXMLQLCNetworkNodeLocked))
            node.locked = (nattrs.value(KXMLQLCNetworkNodeLocked).toString() == "true");
        if (nattrs.hasAttribute(KXMLQLCNetworkNodeLayerId))
            node.layerId = nattrs.value(KXMLQLCNetworkNodeLayerId).toString().toUInt();
        if (nattrs.hasAttribute(KXMLQLCNetworkNodeGroupId))
            node.groupId = nattrs.value(KXMLQLCNetworkNodeGroupId).toString().toUInt();

        while (root.readNextStartElement())
        {
            if (root.name() == KXMLQLCNetworkNodeName)
            {
                node.name = root.readElementText();
            }
            else if (root.name() == KXMLQLCNetworkPort)
            {
                NetworkPort port;
                QXmlStreamAttributes pattrs = root.attributes();
                if (pattrs.hasAttribute(KXMLQLCNetworkPortUniverse))
                    port.universe = pattrs.value(KXMLQLCNetworkPortUniverse).toString().toUInt();

                while (root.readNextStartElement())
                {
                    if (root.name() == KXMLQLCNetworkReceiver)
                    {
                        QXmlStreamAttributes rattrs = root.attributes();
                        port.receiver.placed = true;
                        if (rattrs.hasAttribute(KXMLQLCNetworkReceiverPosX))
                            port.receiver.posX = rattrs.value(KXMLQLCNetworkReceiverPosX).toString().toDouble();
                        if (rattrs.hasAttribute(KXMLQLCNetworkReceiverPosY))
                            port.receiver.posY = rattrs.value(KXMLQLCNetworkReceiverPosY).toString().toDouble();

                        while (root.readNextStartElement())
                        {
                            if (root.name() == KXMLQLCNetworkReceiverName)
                                port.receiver.name = root.readElementText();
                            else
                                root.skipCurrentElement();
                        }
                    }
                    else
                    {
                        root.skipCurrentElement();
                    }
                }
                node.ports.append(port);
            }
            else
            {
                root.skipCurrentElement();
            }
        }
        m_nodes.append(node);
    }

    return true;
}

bool NetworkTopology::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    if (m_nodes.isEmpty())
        return true;

    doc->writeStartElement(KXMLQLCNetworkTopology);

    foreach (const NetworkNode &node, m_nodes)
    {
        doc->writeStartElement(KXMLQLCNetworkNode);
        if (node.transport != NetworkNode::ArtNet)
            doc->writeAttribute(KXMLQLCNetworkNodeTransport, QString::number(node.transport));
        if (node.ipHint.isEmpty() == false)
            doc->writeAttribute(KXMLQLCNetworkNodeIpHint, node.ipHint);
        if (node.model.isEmpty() == false)
            doc->writeAttribute(KXMLQLCNetworkNodeModel, node.model);
        if (node.placed)
        {
            doc->writeAttribute(KXMLQLCNetworkNodePosX, QString::number(node.posX));
            doc->writeAttribute(KXMLQLCNetworkNodePosY, QString::number(node.posY));
        }
        if (node.locked)
            doc->writeAttribute(KXMLQLCNetworkNodeLocked, "true");
        if (node.layerId != 0)
            doc->writeAttribute(KXMLQLCNetworkNodeLayerId, QString::number(node.layerId));
        if (node.groupId != 0)
            doc->writeAttribute(KXMLQLCNetworkNodeGroupId, QString::number(node.groupId));
        doc->writeTextElement(KXMLQLCNetworkNodeName, node.name);

        foreach (const NetworkPort &port, node.ports)
        {
            doc->writeStartElement(KXMLQLCNetworkPort);
            if (port.universe != InputOutputMap::invalidUniverse())
                doc->writeAttribute(KXMLQLCNetworkPortUniverse, QString::number(port.universe));

            if (port.receiver.placed)
            {
                doc->writeStartElement(KXMLQLCNetworkReceiver);
                doc->writeAttribute(KXMLQLCNetworkReceiverPosX, QString::number(port.receiver.posX));
                doc->writeAttribute(KXMLQLCNetworkReceiverPosY, QString::number(port.receiver.posY));
                doc->writeTextElement(KXMLQLCNetworkReceiverName, port.receiver.name);
                doc->writeEndElement(); // Receiver
            }

            doc->writeEndElement(); // Port
        }
        doc->writeEndElement(); // Node
    }

    doc->writeEndElement(); // NetworkTopology

    return true;
}

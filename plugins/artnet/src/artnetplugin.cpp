/*
  Q Light Controller Plus
  artnetplugin.cpp

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

#include <QSettings>
#include <algorithm>
#include <QNetworkDatagram>
#include <QDebug>

#include <QHash>

#include "artnetplugin.h"
#include "artnetnodesetup.h"
#include "configureartnet.h"

bool addressCompare(const ArtNetIO &v1, const ArtNetIO &v2)
{
    return v1.address.ip().toString() < v2.address.ip().toString();
}

ArtNetPlugin::~ArtNetPlugin()
{
}

void ArtNetPlugin::init()
{
    /* outputs() and inputs() both re-enter init(), and the UI asks for them
       in bursts -- OutputPatch::outputName() alone calls outputs() twice, and
       a tree or status refresh does that once per universe. On Windows
       QNetworkInterface::allInterfaces() is a GetAdaptersAddresses() walk of
       some 20 ms (measured on a VM), so every burst froze the GUI for whole
       fractions of a second. Interfaces do not come and go that fast: walk
       them at most once a second. requestLine()'s wait loop invalidates this,
       so its retries always look again. */
    if (m_ifaceScan.isValid() && m_ifaceScan.elapsed() < 1000)
        return;
    m_ifaceScan.start();

    QSettings settings;
    QVariant value = settings.value(SETTINGS_IFACE_WAIT_TIME);
    if (value.isValid() == true)
        m_ifaceWaitTime = value.toInt();
    else
        m_ifaceWaitTime = 0;

    /* A line's POSITION is its identity for the rest of the session: a patch
       holds a line index, ArtNetController caches its index as m_line and
       stamps it on every valueChanged / rdmValueChanged it emits, and
       discoveredDevices() reports it so the connections tree knows which
       interface a node hangs off. init() is re-entered constantly -- outputs()
       and inputs() both call it, and the connections tree calls those every
       five seconds -- so sorting the whole list on each pass meant any newly
       appearing interface could renumber every line underneath live
       controllers. On macOS that is not hypothetical: utun and awdl
       interfaces come and go on their own schedule, so a patched universe
       could silently change which NIC it left by, mid-show, with nothing
       touched.

       So: entries already in the list never move. Only genuinely new ones are
       appended, sorted among themselves so a batch still arrives in a sensible
       order. Interfaces that disappear are deliberately left in place rather
       than removed, because removing one would renumber everything after it --
       the exact problem this avoids. A dead line simply carries no traffic. */
    QList<ArtNetIO> discovered;

    foreach (QNetworkInterface iface, QNetworkInterface::allInterfaces())
    {
        foreach (QNetworkAddressEntry entry, iface.addressEntries())
        {
            QHostAddress addr = entry.ip();
            if (addr.protocol() != QAbstractSocket::IPv6Protocol)
            {
                ArtNetIO tmpIO;
                tmpIO.iface = iface;
                tmpIO.address = entry;
                tmpIO.controller = NULL;

                bool alreadyInList = false;
                for (int j = 0; j < m_IOmapping.count(); j++)
                {
                    if (m_IOmapping.at(j).address == tmpIO.address)
                    {
                        alreadyInList = true;
                        break;
                    }
                }
                for (int j = 0; alreadyInList == false && j < discovered.count(); j++)
                {
                    if (discovered.at(j).address == tmpIO.address)
                        alreadyInList = true;
                }
                if (alreadyInList == false)
                {
                    discovered.append(tmpIO);
                }
            }
        }
    }

    std::sort(discovered.begin(), discovered.end(), addressCompare);
    m_IOmapping.append(discovered);
}

QString ArtNetPlugin::name() const
{
    return QString("ArtNet");
}

int ArtNetPlugin::capabilities() const
{
    return QLCIOPlugin::Output | QLCIOPlugin::Input | QLCIOPlugin::Infinite | QLCIOPlugin::RDM;
}

QString ArtNetPlugin::pluginInfo() const
{
    QString str;

    str += QString("<HTML>");
    str += QString("<HEAD>");
    str += QString("<TITLE>%1</TITLE>").arg(name());
    str += QString("</HEAD>");
    str += QString("<BODY>");

    str += QString("<P>");
    str += QString("<H3>%1</H3>").arg(name());
    str += tr("This plugin provides DMX output for devices supporting the ArtNet communication protocol.");
    str += QString("</P>");

    return str;
}

bool ArtNetPlugin::requestLine(quint32 line)
{
    int retryCount = 0;

    while (line >= (quint32)m_IOmapping.length())
    {
        qDebug() << "[ArtNet] cannot open line" << line << "(available:" << m_IOmapping.length() << ")";
        if (m_ifaceWaitTime)
        {
            Sleep(1000);
            m_ifaceScan.invalidate();
            init();
        }
        if (retryCount++ >= m_ifaceWaitTime)
            return false;
    }

    return true;
}

/*********************************************************************
 * Outputs
 *********************************************************************/
QStringList ArtNetPlugin::outputs()
{
    QStringList list;

    init();

    foreach (ArtNetIO line, m_IOmapping)
        list << line.address.ip().toString();

    return list;
}

QString ArtNetPlugin::outputInfo(quint32 output)
{
    if (output >= (quint32)m_IOmapping.length())
        return QString();

    QString str;

    str += QString("<H3>%1 %2</H3>").arg(tr("Output")).arg(outputs()[output]);
    str += QString("<P>");
    ArtNetController *ctrl = m_IOmapping.at(output).controller;
    if (ctrl == NULL || ctrl->type() == ArtNetController::Input)
        str += tr("Status: Not open");
    else
    {
        str += tr("Status: Open");
        str += QString("<BR>");

        QString boundString;
        if (!ctrl->socketBound())
            boundString = QString("<FONT COLOR=\"#aa0000\">%1</FONT>").arg(tr("No"));
        else
           boundString = QString("<FONT COLOR=\"#00aa00\">%1</FONT>").arg(tr("Yes"));
        str += QString("<B>%1:</B> %2").arg(tr("Can receive nodes information")).arg(boundString);
        str += QString("<BR>");

        QHash<QHostAddress, ArtNetNodeInfo> nodes = ctrl->getNodesList();
        str += tr("Nodes discovered: ");
        str += QString("%1").arg(nodes.size());
        str += QString("<BR>");

        /* Everything below already arrives in every ArtPollReply, about once a
           second per node, and used to be parsed and dropped. Showing it means
           an operator can answer "is that node addressed the way I think it
           is" from the console instead of a separate Art-Net tool. */
        if (nodes.isEmpty() == false)
        {
            /* One compact block per node rather than a wide table: this
               panel is a narrow side pane, and a 7-column table wraps every
               cell character-by-character and becomes unreadable. */
            QHashIterator<QHostAddress, ArtNetNodeInfo> it(nodes);
            while (it.hasNext())
            {
                it.next();
                const ArtNetNodeInfo &node = it.value();

                QString name = node.longName.isEmpty() ? node.shortName : node.longName;

                /* A Port-Address is Net:Sub:Universe -- printing the raw
                   SwOut alone is wrong for any node not sitting on net 0. */
                QStringList ports;
                for (int p = 0; p < qMin(node.portsNumber, 4); p++)
                {
                    if (node.portTypes[p] & 0x80)
                        ports << QString("%1:%2:%3").arg(node.netSwitch)
                                 .arg(node.subSwitch).arg(node.swOut[p]);
                }

                str += QString("<BR><B>%1</B>").arg(name.toHtmlEscaped());
                str += QString("<BR>&nbsp;&nbsp;%1: %2").arg(tr("IP")).arg(node.ipAddress);
                if (node.macAddress.isEmpty() == false)
                    str += QString(" &nbsp; %1: %2").arg(tr("MAC")).arg(node.macAddress);
                str += QString("<BR>&nbsp;&nbsp;%1: %2 &nbsp; %3: %4")
                          .arg(tr("Firmware")).arg(node.firmwareVersion)
                          .arg(tr("RDM")).arg(node.rdmCapable ? tr("Yes") : tr("No"));
                if (ports.isEmpty() == false)
                    str += QString("<BR>&nbsp;&nbsp;%1: %2")
                              .arg(tr("Output universes")).arg(ports.join(", "));
                if (node.nodeReport.isEmpty() == false)
                    str += QString("<BR>&nbsp;&nbsp;%1").arg(node.nodeReport.toHtmlEscaped());
                str += QString("<BR>");
            }
        }
        str += tr("Packets sent: ");
        str += QString("%1").arg(ctrl->getPacketSentNumber());
    }
    str += QString("</P>");
    str += QString("</BODY>");
    str += QString("</HTML>");

    return str;
}

/** Nodes heard on any of our controllers.
 *
 *  A node announces itself roughly once a second, so this is current rather
 *  than a cached scan result; the controller refreshes its entry on every
 *  reply. Ports are reported per physical DMX port because that is how
 *  Art-Net addresses them (Net:Sub:Universe) and how RDM discovery works --
 *  per port, not per node.
 */
QList<QLCIOPlugin::Device> ArtNetPlugin::discoveredDevices() const
{
    QList<QLCIOPlugin::Device> list;

    /* m_IOmapping is indexed by plugin line, so the index IS the line. */
    for (int line = 0; line < m_IOmapping.count(); line++)
    {
        ArtNetController *ctrl = m_IOmapping.at(line).controller;
        if (ctrl == NULL)
            continue;

        QHash<QHostAddress, ArtNetNodeInfo> nodes = ctrl->getNodesList();
        QHashIterator<QHostAddress, ArtNetNodeInfo> it(nodes);
        while (it.hasNext())
        {
            it.next();
            const ArtNetNodeInfo &n = it.value();

            QLCIOPlugin::Device dev;
            dev.line = quint32(line);
            dev.name = n.longName.isEmpty() ? n.shortName : n.longName;
            dev.address = n.ipAddress.isEmpty() ? it.key().toString() : n.ipAddress;
            dev.hardwareId = n.macAddress;
            dev.status = n.nodeReport;
            dev.rdmCapable = n.rdmCapable;
            /* A node that answered but sits on no network this computer has
               an address on -- typically one fresh out of the box, still on
               its factory 2.x.x.x -- can be heard but not patched to. Say so
               on its row rather than listing it like any other node. */
            dev.attention = attentionFor(QHostAddress(dev.address),
                                         lineReaching(QHostAddress(dev.address)) >= 0);
            dev.detail = QObject::tr("firmware %1").arg(n.firmwareVersion);

            for (int p = 0; p < qMin(n.portsNumber, 4); p++)
            {
                if ((n.portTypes[p] & 0x80) == 0)
                    continue;   // not an output port
                dev.portLabels << QString("%1:%2:%3").arg(n.netSwitch)
                                  .arg(n.subSwitch).arg(n.swOut[p]);
                dev.portUniverses << quint32((n.netSwitch << 8)
                                             + (n.subSwitch << 4) + n.swOut[p]);
                dev.portNumbers << p + 1;
            }

            /* Everything the reply told us that does not fit the fixed fields.
               All of it arrives in every ArtPollReply about once a second and
               was being parsed and then dropped; the operator questions it
               answers -- "is this the node I think it is", "why is that port
               not outputting", "did somebody set this to DHCP" -- are exactly
               the ones asked while staring at the row. */
            typedef QPair<QString, QString> Prop;
            if (n.shortName.isEmpty() == false)
                dev.properties << Prop(QObject::tr("Short name"), n.shortName);
            if (n.longName.isEmpty() == false)
                dev.properties << Prop(QObject::tr("Long name"), n.longName);
            dev.properties << Prop(QObject::tr("IP address"),
                                   n.ipAddress.isEmpty() ? it.key().toString()
                                                         : n.ipAddress);
            if (n.macAddress.isEmpty() == false)
                dev.properties << Prop(QObject::tr("MAC"), n.macAddress);
            dev.properties << Prop(QObject::tr("Firmware"),
                                   QString::number(n.firmwareVersion));
            dev.properties << Prop(QObject::tr("OEM / ESTA"),
                                   QString("0x%1 / 0x%2")
                                       .arg(n.oemCode, 4, 16, QChar('0'))
                                       .arg(n.estaCode, 4, 16, QChar('0')));
            dev.properties << Prop(QObject::tr("Addressing"),
                                   n.dhcpConfigured ? QObject::tr("DHCP")
                                   : n.dhcpCapable ? QObject::tr("static (DHCP capable)")
                                                   : QObject::tr("static only"));
            if (dev.attention.isEmpty() == false)
                dev.properties << Prop(QObject::tr("Network"), dev.attention);
            dev.properties << Prop(QObject::tr("RDM"),
                                   n.rdmCapable ? QObject::tr("yes")
                                                : QObject::tr("no"));
            dev.properties << Prop(QObject::tr("Ports"),
                                   QString::number(n.portsNumber));

            for (int p = 0; p < qMin(n.portsNumber, 4); p++)
            {
                if ((n.portTypes[p] & 0x80) == 0)
                    continue;
                /* GoodOutput bit 7 is "data is being transmitted", bit 3 is
                   "output is merging", bit 2 is "DMX output short detected"
                   (Art-Net 4 GoodOutputA; libartnet's PORT_STATUS_MERGE 0x08
                   and PORT_STATUS_SHORT 0x04 agree). These two were swapped
                   here, so a merging port read SHORT DETECTED and a real short
                   read "merging". */
                QStringList st;
                st << QString("%1:%2:%3").arg(n.netSwitch).arg(n.subSwitch)
                      .arg(n.swOut[p]);
                if (n.goodOutput[p] & 0x80) st << QObject::tr("transmitting");
                if (n.goodOutput[p] & 0x04) st << QObject::tr("SHORT DETECTED");
                if (n.goodOutput[p] & 0x08) st << QObject::tr("merging");
                dev.properties << Prop(QObject::tr("Port %1").arg(p + 1),
                                       st.join(" · "));
            }

            if (n.nodeReport.isEmpty() == false)
                dev.properties << Prop(QObject::tr("Node report"), n.nodeReport);
            if (n.bindIndex > 1)
                dev.properties << Prop(QObject::tr("Bind"),
                                       QObject::tr("index %1 of %2")
                                           .arg(n.bindIndex).arg(n.bindIpAddress));
            list << dev;
        }
    }
    return list;
}

/** Every output problem on every line, a target's universes on one line:
 *  "Art-Net to 172.18.2.50: not reachable since 20:14 (universes 3, 4)".
 *  Fifty universes behind thirteen dead nodes are thirteen problems, not
 *  fifty. */
QStringList ArtNetPlugin::targetProblems() const
{
    QStringList order;
    QMap<QString, QList<quint32> > universes;
    typedef QPair<quint32, QString> Problem;
    for (int line = 0; line < m_IOmapping.count(); line++)
    {
        ArtNetController *ctrl = m_IOmapping.at(line).controller;
        if (ctrl == NULL)
            continue;
        foreach (const Problem &p, ctrl->outputProblems())
        {
            if (universes.contains(p.second) == false)
                order << p.second;
            if (universes[p.second].contains(p.first) == false)
                universes[p.second] << p.first;
        }
    }

    QStringList out;
    foreach (const QString &text, order)
    {
        QList<quint32> unis = universes.value(text);
        std::sort(unis.begin(), unis.end());
        QStringList nums;
        foreach (quint32 u, unis)
            nums << QString::number(u + 1);
        out << (nums.count() == 1
                ? tr("%1 (universe %2)").arg(text, nums.first())
                : tr("%1 (universes %2)").arg(text, nums.join(", ")));
    }
    return out;
}

QStringList ArtNetPlugin::outputProblems(quint32 output, quint32 universe) const
{
    QStringList out;
    if (output >= quint32(m_IOmapping.count()))
        return out;
    ArtNetController *ctrl = m_IOmapping.at(int(output)).controller;
    if (ctrl == NULL)
        return out;
    typedef QPair<quint32, QString> Problem;
    foreach (const Problem &p, ctrl->outputProblems(universe))
        out << p.second;
    return out;
}

/** Poll every line once, on demand.
 *
 *  ArtPoll normally runs only where an output is open (ArtNetController::
 *  addUniverse), which leaves an unpatched interface permanently dark:
 *  nothing is sent, so nothing replies, so the nodes on it never appear --
 *  and that is exactly the interface you are staring at when you are looking
 *  for something to patch TO. Polling those lines continuously would raise
 *  the steady-state broadcast load on every node on the segment, all day,
 *  for the sake of a question only asked while patching. So: on demand.
 *
 *  A line with no controller gets one here. It is inert -- the constructor
 *  starts no timers and no universe is added, so it neither polls again nor
 *  sends DMX -- but it has to exist in order to RECEIVE: handlePacket()
 *  routes a reply to the controller on the sender's subnet and drops it on
 *  the floor if there is none.
 */
bool ArtNetPlugin::probeTarget(const QString &address)
{
    const QHostAddress addr(address);
    if (addr.isNull())
        return false;   // a hostname, not an address -- nothing to aim at

    /* Prefer the controller whose subnet contains the target: its socket is
       bound to the interface the packet should leave by, and its own address
       is what the node will answer to. */
    for (int line = 0; line < m_IOmapping.count(); line++)
    {
        ArtNetController *ctrl = m_IOmapping.at(line).controller;
        if (ctrl == NULL)
            continue;
        if (addr.isInSubnet(m_IOmapping.at(line).address.ip(),
                            m_IOmapping.at(line).address.prefixLength()))
        {
            ctrl->sendPollTo(addr);
            return true;
        }
    }

    /* Otherwise any controller will do: the target is off-segment, the packet
       goes out by whatever route the host chooses, and that is the whole point
       of probing it rather than waiting for a broadcast it will never see. */
    for (int line = 0; line < m_IOmapping.count(); line++)
    {
        ArtNetController *ctrl = m_IOmapping.at(line).controller;
        if (ctrl != NULL)
        {
            ctrl->sendPollTo(addr);
            return true;
        }
    }

    return false;   // no controller anywhere: nothing is listening for a reply
}

int ArtNetPlugin::lineOnSameSubnet(const QString &identity) const
{
    const QHostAddress saved(identity);
    if (saved.isNull())
        return -1;   // not an address at all -- nothing to compare a subnet to

    for (int line = 0; line < m_IOmapping.count(); line++)
    {
        const QNetworkAddressEntry &here = m_IOmapping.at(line).address;
        if (saved.isInSubnet(here.ip(), here.prefixLength()))
            return line;
    }

    return -1;
}

QString ArtNetPlugin::lineDescription(quint32 line, bool output) const
{
    Q_UNUSED(output)   // in and out are the same interface on Art-Net

    if (line >= quint32(m_IOmapping.count()))
        return QString();

    return m_IOmapping.at(line).iface.humanReadableName();
}

void ArtNetPlugin::rescan()
{
    QSet<int> interfacesPolled;
    for (int line = 0; line < m_IOmapping.count(); line++)
    {
        ArtNetController *controller = ensureController(line);
        controller->sendPoll();

        /* Plus one limited-broadcast poll per physical interface (not per
           address on it), for the node nobody has given an address yet: it
           is not on this line's subnet, so the poll above may never reach
           it. See ArtNetController::sendDiscoveryPoll(). */
        const int ifIndex = m_IOmapping.at(line).iface.index();
        if (interfacesPolled.contains(ifIndex) == false
                && controller->sendDiscoveryPoll())
            interfacesPolled.insert(ifIndex);
    }
}

ArtNetController *ArtNetPlugin::ensureController(int line)
{
    if (m_IOmapping.at(line).controller == NULL)
    {
        ArtNetController *controller = new ArtNetController(m_IOmapping.at(line).iface,
                                                            m_IOmapping.at(line).address,
                                                            getUdpSocket(),
                                                            quint32(line), this);
        connect(controller, SIGNAL(valueChanged(quint32,quint32,quint32,uchar)),
                this, SIGNAL(valueChanged(quint32,quint32,quint32,uchar)));
        connect(controller, SIGNAL(rdmValueChanged(quint32, quint32, QVariantMap)),
                this , SIGNAL(rdmValueChanged(quint32, quint32, QVariantMap)));
        connect(controller, &ArtNetController::problemsChanged,
                this, &QLCIOPlugin::targetProblemsChanged);
        connect(controller, &ArtNetController::ipProgReplied,
                this, &ArtNetPlugin::ipProgReplied);
        m_IOmapping[line].controller = controller;
    }
    return m_IOmapping.at(line).controller;
}

bool ArtNetPlugin::openOutput(quint32 output, quint32 universe)
{
    if (requestLine(output) == false)
        return false;

    qDebug() << "[ArtNet] Open output on address :" << m_IOmapping.at(output).address.ip().toString();

    // if the controller doesn't exist, create it
    ensureController(int(output));

    m_IOmapping[output].controller->addUniverse(universe, ArtNetController::Output);
    addToMap(universe, output, Output);

    return true;
}

void ArtNetPlugin::closeOutput(quint32 output, quint32 universe)
{
    if (output >= (quint32)m_IOmapping.length())
        return;

    removeFromMap(output, universe, Output);
    ArtNetController *controller = m_IOmapping.at(output).controller;
    if (controller != NULL)
    {
        controller->removeUniverse(universe, ArtNetController::Output);
        if (controller->universesList().count() == 0)
        {
            delete m_IOmapping[output].controller;
            m_IOmapping[output].controller = NULL;
        }
    }
    // A closed output has nothing left to be wrong with
    emit targetProblemsChanged();
}

void ArtNetPlugin::writeUniverse(quint32 universe, quint32 output, const QByteArray &data, bool dataChanged)
{
    if (output >= (quint32)m_IOmapping.count())
        return;

    ArtNetController *controller = m_IOmapping.at(output).controller;
    if (controller != NULL)
        controller->sendDmx(universe, data, dataChanged);
}

/*************************************************************************
  * Inputs
  *************************************************************************/
QStringList ArtNetPlugin::inputs()
{
    QStringList list;

    init();

    foreach (ArtNetIO line, m_IOmapping)
        list << line.address.ip().toString();

    return list;
}

bool ArtNetPlugin::openInput(quint32 input, quint32 universe)
{
    if (requestLine(input) == false)
        return false;

    // if the controller doesn't exist, create it.
    // We need to have only one input controller.
    ensureController(int(input));

    m_IOmapping[input].controller->addUniverse(universe, ArtNetController::Input);
    addToMap(universe, input, Input);

    return true;
}

void ArtNetPlugin::closeInput(quint32 input, quint32 universe)
{
    if (input >= (quint32)m_IOmapping.length())
        return;

    removeFromMap(input, universe, Input);
    ArtNetController *controller = m_IOmapping.at(input).controller;
    if (controller != NULL)
    {
        controller->removeUniverse(universe, ArtNetController::Input);
        if (controller->universesList().count() == 0)
        {
            delete m_IOmapping[input].controller;
            m_IOmapping[input].controller = NULL;
        }
    }
}

QString ArtNetPlugin::inputInfo(quint32 input)
{
    if (input >= (quint32)m_IOmapping.length())
        return QString();

    QString str;

    str += QString("<H3>%1 %2</H3>").arg(tr("Input")).arg(inputs()[input]);
    str += QString("<P>");
    ArtNetController *ctrl = m_IOmapping.at(input).controller;
    if (ctrl == NULL || ctrl->type() == ArtNetController::Output)
        str += tr("Status: Not open");
    else
    {
        QString boundString;
        if (!ctrl->socketBound())
            boundString = QString("<FONT COLOR=\"#aa0000\">%1</FONT>").arg(tr("Bind failed"));
        else
           boundString = QString("<FONT COLOR=\"#00aa00\">%1</FONT>").arg(tr("Open"));
        str += QString("<B>%1:</B> %2").arg(tr("Status")).arg(boundString);
        str += QString("<BR>");

        str += tr("Packets received: ");
        str += QString("%1").arg(ctrl->getPacketReceivedNumber());
    }
    str += QString("</P>");
    str += QString("</BODY>");
    str += QString("</HTML>");

    return str;
}

/*********************************************************************
 * Node network settings (ArtIpProg)
 *********************************************************************/

bool ArtNetPlugin::findNode(const QString &address, ArtNetNodeInfo *info, int *line) const
{
    const QHostAddress wanted(address);
    for (int l = 0; l < m_IOmapping.count(); l++)
    {
        ArtNetController *ctrl = m_IOmapping.at(l).controller;
        if (ctrl == NULL)
            continue;
        QHash<QHostAddress, ArtNetNodeInfo> nodes = ctrl->getNodesList();
        for (auto it = nodes.constBegin(); it != nodes.constEnd(); ++it)
        {
            if (it.value().ipAddress == address || it.key().isEqual(wanted))
            {
                if (info != NULL)
                    *info = it.value();
                if (line != NULL)
                    *line = l;
                return true;
            }
        }
    }
    return false;
}

int ArtNetPlugin::lineReaching(const QHostAddress &address) const
{
    if (address.isNull() || address.protocol() != QAbstractSocket::IPv4Protocol)
        return -1;
    for (int line = 0; line < m_IOmapping.count(); line++)
    {
        const QNetworkAddressEntry &e = m_IOmapping.at(line).address;
        if (e.ip().isLoopback() != address.isLoopback())
            continue;
        if (e.prefixLength() > 0 && address.isInSubnet(e.ip(), e.prefixLength()))
            return line;
    }
    return -1;
}

QString ArtNetPlugin::attentionFor(const QHostAddress &ip, bool reachable)
{
    if (reachable)
        return QString();
    if (ip.isNull() || ip == QHostAddress(quint32(0)))
        return tr("has no IP address yet: set its IP");
    if (ip.isInSubnet(QHostAddress("169.254.0.0"), 16))
        return tr("self-assigned address (no DHCP server answered it): set its IP");
    if (ip.isInSubnet(QHostAddress("2.0.0.0"), 8) || ip.isInSubnet(QHostAddress("10.0.0.0"), 8))
        return tr("on its factory Art-Net address, not this network: set its IP");
    return tr("not on this computer's network: set its IP");
}

bool ArtNetPlugin::sendIpProg(const QString &node, const ArtNetIpProgRequest &request,
                              QString *why)
{
    const QHostAddress addr(node);
    const int line = lineReaching(addr);
    if (line < 0)
    {
        if (why != NULL)
            *why = tr("No network on this computer contains %1, so it cannot be "
                      "sent to directly.").arg(node);
        return false;
    }
    ArtNetController *ctrl = ensureController(line);
    if (ctrl->sendIpProg(addr, request) == false)
    {
        if (why != NULL)
            *why = tr("The send failed (see the log).");
        return false;
    }
    return true;
}

bool ArtNetPlugin::canConfigureDevice(const QString &address) const
{
    return findNode(address);
}

void ArtNetPlugin::configureDevice(const QString &address)
{
    if (findNode(address) == false)
        return;
    ArtNetNodeSetup dlg(this, address);
    dlg.exec();
}

/*********************************************************************
 * Configuration
 *********************************************************************/
void ArtNetPlugin::configure()
{
    ConfigureArtNet conf(this);
    conf.exec();
}

bool ArtNetPlugin::canConfigure() const
{
    return true;
}

void ArtNetPlugin::setParameter(quint32 universe, quint32 line, Capability type,
                                QString name, QVariant value)
{
    if (line >= (quint32)m_IOmapping.length())
        return;

    ArtNetController *controller = m_IOmapping.at(line).controller;
    if (controller == NULL)
        return;

    // If the Controller parameter is restored to its default value,
    // unset the corresponding plugin parameter
    bool unset;

    if (type == Input)
    {
        if (name == ARTNET_INPUTUNI)
            unset = controller->setInputUniverse(universe, value.toUInt());
        else
        {
            qWarning() << Q_FUNC_INFO << name << "is not a valid ArtNet input parameter";
            return;
        }
    }
    else // if (type == Output)
    {
        if (name == ARTNET_OUTPUTIP)
            unset = controller->setOutputIPAddress(universe, value.toString());
        else if (name == ARTNET_OUTPUTUNI)
            unset = controller->setOutputUniverse(universe, value.toUInt());
        else if (name == ARTNET_TRANSMITMODE)
            unset = controller->setTransmissionMode(universe, ArtNetController::stringToTransmissionMode(value.toString()));
        else
        {
            qWarning() << Q_FUNC_INFO << name << "is not a valid ArtNet output parameter";
            return;
        }
    }

    if (unset)
        QLCIOPlugin::unSetParameter(universe, line, type, name);
    else
        QLCIOPlugin::setParameter(universe, line, type, name, value);
}

QList<ArtNetIO> ArtNetPlugin::getIOMapping() const
{
    return m_IOmapping;
}

/********************************************************************
 * RDM
 ********************************************************************/

bool ArtNetPlugin::sendRDMCommand(quint32 universe, quint32 line, uchar command, QVariantList params)
{
    qDebug() << "Sending RDM command on universe" << universe << "and line" << line;
    if (line >= (quint32)m_IOmapping.count())
        return false;

    ArtNetController *controller = m_IOmapping.at(line).controller;
    if (controller != NULL)
        return controller->sendRDMCommand(universe, command, params);

    return false;
}

/*********************************************************************
 * ArtNet socket
 *********************************************************************/

QSharedPointer<QUdpSocket> ArtNetPlugin::getUdpSocket()
{
    // Is the socket already present ?
    QSharedPointer<QUdpSocket> udpSocket(m_udpSocket);
    if (udpSocket)
        return udpSocket;

    // Create a new socket
    udpSocket = QSharedPointer<QUdpSocket>(new QUdpSocket());
    m_udpSocket = udpSocket.toWeakRef();

    if (udpSocket->bind(ARTNET_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
        connect(udpSocket.data(), SIGNAL(readyRead()),
                this, SLOT(slotReadyRead()));
    }
    else
    {
        qWarning() << "ArtNet: could not bind socket to address" << QString("0:%2").arg(ARTNET_PORT);
    }
    return udpSocket;
}

void ArtNetPlugin::slotReadyRead()
{
    QUdpSocket* udpSocket = qobject_cast<QUdpSocket*>(sender());
    Q_ASSERT(udpSocket != NULL);

    while (udpSocket->hasPendingDatagrams())
    {
        /* receiveDatagram() rather than readDatagram(): it carries the index
           of the interface the packet actually ARRIVED on, which is the only
           trustworthy answer to "whose node is this". Everything else is
           inference from the sender's address, and show gear is routinely
           L2-adjacent but L3-foreign -- a node still on its factory 2.x.x.x
           address, broadcasting onto a network the console reaches at
           192.168.1.x. Such a packet matches no interface's subnet at all. */
        QNetworkDatagram dg = udpSocket->receiveDatagram();
        if (dg.isValid() == false)
            continue;
        handlePacket(dg.data(), dg.senderAddress(), dg.interfaceIndex());
    }
}

void ArtNetPlugin::handlePacket(QByteArray const& datagram,
                                QHostAddress const& senderAddress,
                                uint interfaceIndex)
{
    /* First and best: the interface it came in on. An interface can hold
       several addresses, so take the first entry on that NIC that is
       listening. */
    if (interfaceIndex != 0)
    {
        foreach (ArtNetIO io, m_IOmapping)
        {
            if (io.controller != NULL
                    && uint(io.iface.index()) == interfaceIndex)
            {
                io.controller->handlePacket(datagram, senderAddress);
                return;
            }
        }
    }

    /* Failing that, the sender's subnet. This also lets the same Art-Net
       universe live on two different interfaces without them colliding. */
    foreach (ArtNetIO io, m_IOmapping)
    {
        if (senderAddress.isInSubnet(io.address.ip(), io.address.prefixLength()))
        {
            if (io.controller != NULL)
                io.controller->handlePacket(datagram, senderAddress);
            return;
        }
    }

    /* And then stop. This used to fall back to "the first controller with a
       pulse", which sorts by IP string and therefore means 127.0.0.1 -- so
       every node the console could not place got filed under loopback, and
       the tree confidently showed 172.18.2.10 hanging off 127.0.0.1. A
       loopback interface cannot receive a packet from a non-loopback sender;
       attributing one to it is not a fallback, it is a wrong answer that
       looks like a right one. Better to drop it and say so. */
    qDebug() << "[ArtNet] Unattributable packet from" << senderAddress.toString()
             << "on interface index" << interfaceIndex << "-- ignored";
}

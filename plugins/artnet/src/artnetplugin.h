/*
  Q Light Controller Plus
  artnetplugin.h

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

#ifndef ARTNETPLUGIN_H
#define ARTNETPLUGIN_H

#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QString>
#include <QHash>
#include <QFile>

#include "qlcioplugin.h"
#include "artnetcontroller.h"

#define SETTINGS_IFACE_WAIT_TIME "ArtNetPlugin/ifacewait"

typedef struct _aio
{
    QNetworkInterface iface;
    QNetworkAddressEntry address;
    ArtNetController* controller;
} ArtNetIO;

#define ARTNET_INPUTUNI "inputUni"
#define ARTNET_OUTPUTIP "outputIP"
#define ARTNET_OUTPUTUNI "outputUni"
#define ARTNET_TRANSMITMODE "transmitMode"

class ArtNetPlugin final : public QLCIOPlugin
{
    Q_OBJECT
    Q_INTERFACES(QLCIOPlugin)
    Q_PLUGIN_METADATA(IID QLCIOPlugin_iid)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    /** @reimp */
    virtual ~ArtNetPlugin();

    /** @reimp */
    void init() override;

    /** @reimp */
    QString name() const override;

    /** @reimp */
    int capabilities() const override;

    /** @reimp */
    QString pluginInfo() const override;

private:
    bool requestLine(quint32 line);

    /*********************************************************************
     * Outputs
     *********************************************************************/
public:
    /** @reimp */
    bool openOutput(quint32 output, quint32 universe) override;

    /** @reimp */
    void closeOutput(quint32 output, quint32 universe) override;

    /** @reimp */
    QStringList outputs() override;

    /** @reimp Art-Net nodes heard via ArtPollReply. */
    QList<Device> discoveredDevices() const override;

    /** @reimp Poll every line once, including lines with nothing patched. */
    void rescan() override;

    /** @reimp The network interface a line lives on, e.g. "en0" or "vlan0". */
    QString lineDescription(quint32 line, bool output) const override;

    /** @reimp Patches carry outputIP + outputUni to aim at a node's port. */
    bool supportsOutputTargets() const override { return true; }

    /** @reimp Outputs that can't send, nodes that stopped answering, and
        ports that report a fault -- see SendHealth and ArtNetNodeWatch. */
    QStringList targetProblems() const override;

    /** @reimp The same, for one patched universe. */
    QStringList outputProblems(quint32 output, quint32 universe) const override;

    /** @reimp Unicast an ArtPoll at one address. */
    bool probeTarget(const QString &address) override;

    /** @reimp Every line already carries its own subnet (address +
        prefix length), used for probeTarget()'s own routing decision. */
    int lineOnSameSubnet(const QString &identity) const override;

    /** @reimp Any Art-Net node heard on a line: its IP can be set. */
    bool canConfigureDevice(const QString &address) const override;

    /** @reimp The node network settings dialog (ArtIpProg). */
    void configureDevice(const QString &address) override;

    /** @reimp */
    QString outputInfo(quint32 output) override;

    /*********************************************************************
     * Node network settings (ArtIpProg)
     *********************************************************************/
public:
    /** The node heard at @a address (its own reported IP, or the address it
     *  sent from), and the line it was heard on. False when none has. */
    bool findNode(const QString &address, ArtNetNodeInfo *info = NULL,
                  int *line = NULL) const;

    /** The line whose own subnet contains @a address, i.e. the one a unicast
     *  to it leaves by without a router; -1 when there is none. */
    int lineReaching(const QHostAddress &address) const;

    /** What a node at @a ip needs before it is usable from here, or empty:
     *  "on its factory Art-Net address (2.x.x.x) -- set its IP" etc. */
    static QString attentionFor(const QHostAddress &ip, bool reachable);

    /** Send one ArtIpProg to the node at @a node, unicast through the line
     *  whose subnet holds it. Refuses (false, @a why set) when no line does:
     *  ArtIpProg carries no MAC or serial to pick one node out, so it must
     *  never be broadcast -- every node on the cable that implements it
     *  would take the new address. The answer arrives as ipProgReplied(). */
    bool sendIpProg(const QString &node, const ArtNetIpProgRequest &request,
                    QString *why = NULL);

signals:
    void ipProgReplied(const QString &sender, const ArtNetIpProgReply &reply);

private:
    /** The line's controller, created (inert: no timers, no universes) if it
     *  has none, so that it can send and receive. */
    ArtNetController *ensureController(int line);

    /** @reimp */
    void writeUniverse(quint32 universe, quint32 output, const QByteArray& data, bool dataChanged) override;

    /*************************************************************************
     * Inputs
     *************************************************************************/
public:
    /** @reimp */
    bool openInput(quint32 input, quint32 universe) override;

    /** @reimp */
    void closeInput(quint32 input, quint32 universe) override;

    /** @reimp */
    QStringList inputs() override;

    /** @reimp */
    QString inputInfo(quint32 input) override;

    /*********************************************************************
     * Configuration
     *********************************************************************/
public:
    /** @reimp */
    void configure() override;

    /** @reimp */
    bool canConfigure() const override;

    /** @reimp */
    void setParameter(quint32 universe, quint32 line, Capability type, QString name, QVariant value) override;

    /** Get a list of the available Input/Output lines */
    QList<ArtNetIO> getIOMapping() const;

private:
    /** Map of the ArtNet plugin Input/Output lines */
    QList<ArtNetIO> m_IOmapping;

    /** Time to wait (in seconds) for interfaces to be ready */
    int m_ifaceWaitTime;

    /** When init() last walked the network interfaces (see init()) */
    QElapsedTimer m_ifaceScan;

    /********************************************************************
     * RDM
     ********************************************************************/
public:
    /** @reimp */
    bool sendRDMCommand(quint32 universe, quint32 line, uchar command, QVariantList params) override;

signals:
    void rdmValueChanged(quint32 universe, quint32 line, QVariantMap data);

    /*********************************************************************
     * ArtNet socket
     *********************************************************************/
private:
    QSharedPointer<QUdpSocket> getUdpSocket();
    void handlePacket(QByteArray const& datagram, QHostAddress const& senderAddress,
                      uint interfaceIndex);

private slots:
    void slotReadyRead();

private:
    QWeakPointer<QUdpSocket> m_udpSocket;
};

#endif

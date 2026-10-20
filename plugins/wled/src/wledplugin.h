/*
  Q Light Controller Plus
  wledplugin.h

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

#ifndef WLEDPLUGIN_H
#define WLEDPLUGIN_H

#include <QJsonObject>
#include <QMutex>
#include <QAtomicInt>
#include <QHash>
#include <QPair>
#include <QVector>

#include "qlcioplugin.h"

class QNetworkAccessManager;
class QTimer;

#define WLED_OUTPUTIP   "outputIP"
#define WLED_ADDRESS    "wledAddress"
#define WLED_CHANNELS   "wledChannels"

/* More devices on the same universe -- a backup lantern mirroring the
   primary -- are numbered from 2: "outputIP2", "outputIP3"..., each
   optionally with its own "wledAddress2"/"wledChannels2" (absent: the
   first device's). "outputIP" stays the first device, so an older build
   reads a show as its primary alone and ignores the rest. */

/**
 * WLED over its own JSON API, for WLED on Wi-Fi.
 *
 * Art-Net streams every channel of every universe whenever anything moves,
 * as UDP: on Wi-Fi a dropped packet is a lantern left on a stale look until
 * the next change, and a fade is dozens of packets a second per lantern.
 * Driven in WLED's effect mode -- the effect runs ON the lantern -- what it
 * needs is its STATE: brightness, effect, speed, palette, colours. So this
 * sends that, as JSON over HTTP (/json/state): only when it changes, at most
 * ten times a second per lantern with WLED's own transition gliding between,
 * one request in flight per lantern (the latest wins). A held look sends
 * nothing at all.
 *
 * One universe per lantern, aimed at its IP ("outputIP", as Art-Net's); the
 * lantern's channels are read at "wledAddress" (1-based, default 1),
 * "wledChannels" of them (15, 4 or 3 -- the WLED definitions' modes).
 *
 * A universe can drive more than one device: a backup lantern mirroring
 * the primary ("outputIP2"..., see above). Each device is its own: its own
 * request in flight, its own last acknowledged state, its own answer -- so
 * a dead one never holds the others back, and one that comes back is sent
 * the look again in full.
 *
 * Each lantern is also asked /json/info every few seconds: its name, its
 * firmware, its Wi-Fi signal -- and whether it answers at all.
 * targetProblems() says which do not, or answer on a weak signal.
 *
 * While the rig view watches a lantern (watchTargetPixels()), it is asked
 * /json/live a few times a second -- what each LED is showing, its effect
 * worked out by the lantern itself -- and only then: sixteen colours is a
 * small answer, but Wi-Fi is the thing this plugin exists to spare.
 */
class WledPlugin final : public QLCIOPlugin
{
    Q_OBJECT
    Q_INTERFACES(QLCIOPlugin)
    Q_PLUGIN_METADATA(IID QLCIOPlugin_iid)

public:
    ~WledPlugin() override;

    void init() override;
    QString name() const override;
    int capabilities() const override;
    QString pluginInfo() const override;

    /*********************************************************************
     * Outputs
     *********************************************************************/
public:
    bool openOutput(quint32 output, quint32 universe) override;
    void closeOutput(quint32 output, quint32 universe) override;
    QStringList outputs() override;
    QString outputInfo(quint32 output) override;
    bool supportsOutputTargets() const override { return true; }
    void writeUniverse(quint32 universe, quint32 output, const QByteArray &data,
                       bool dataChanged) override;
    void setParameter(quint32 universe, quint32 line, Capability type,
                      QString name, QVariant value) override;
    void unSetParameter(quint32 universe, quint32 line, Capability type,
                        QString name) override;
    QStringList targetProblems() const override;
    QStringList outputProblems(quint32 output, quint32 universe) const override;

    /** The device slot a parameter names (0 for "outputIP", 1 for
     *  "outputIP2"...), and which of IP/address/channels it is (0/1/2);
     *  -1 when it is not one of ours. */
    static int parameterSlot(const QString &name, int *kind = nullptr);
    /** How many devices answer on @p universe, of how many have an IP. */
    QPair<int, int> answering(quint32 universe) const;

    void watchTargetPixels(quint32 universe, bool watch) override;
    QVector<quint32> targetPixels(quint32 universe) const override;
    int targetPixelCount(quint32 universe) const override;

    /** How often a watched lantern is asked what its LEDs show (ms). */
    void setLiveInterval(int ms);
    /** An answer older than this (ms) is not shown: the lantern stopped
     *  answering, and a frozen frame would say it was still running. */
    static const int liveStaleMs = 2000;

    /** How often a changed state may go out, per lantern (ms). Tests run it
     *  faster; WLED's transition matches it. */
    void setSendInterval(int ms);
    /** How often each lantern is asked how it is (ms). */
    void setInfoInterval(int ms);

    /** Weaker than this (dBm) is reported: on stage Wi-Fi it will drop. */
    static const int weakSignalDbm = -75;
    /** A device that did not take a state is sent it again after this (ms),
     *  not every tick. */
    static const int retryMs = 1000;

private:
    /** One WLED the universe is sent to. */
    struct Device
    {
        QString ip;
        int address = 0;            ///< 0: the first device's
        int channels = 0;           ///< 0: the first device's
        QJsonObject sent;           ///< last state WLED acknowledged
        bool inFlight = false;
        bool infoInFlight = false;
        bool liveCleared = false;   ///< realtime (Art-Net/E1.31) mode left once
        int answered = -1;          ///< -1 not asked yet, 0 no answer, 1 answered
        int rssi = 0;               ///< dBm, 0 = unknown
        QString deviceName;
        QString version;
        int ledCount = 0;           ///< from /json/info, 0 = not known
        qint64 retryAt = 0;         ///< no answer: not sent again before (ms)
        /** The rig network gate's decision for this device, cached (see
         *  routeAllowed()): worked out when its address is set and by
         *  refreshTransmitGate(), only read by the send/poll ticks. */
        bool gateOpen = true;
    };

    struct Target
    {
        int address = 1;
        int channels = 15;
        QByteArray data;            ///< latest universe, from writeUniverse()
        QVector<Device> devices;    ///< [0] the primary; a slot may be empty
        bool liveInFlight = false;
        QVector<quint32> pixels;    ///< /json/live: what the LEDs show
        qint64 pixelsAt = 0;        ///< when (QDateTime ms)

        int addressOf(const Device &d) const { return d.address > 0 ? d.address : address; }
        int channelsOf(const Device &d) const { return d.channels > 0 ? d.channels : channels; }
        int deviceCount() const;
        /** The device the rig view asks: the first that answers (or has
         *  not been asked yet), else -1. */
        int liveSlot() const;
    };

    void tick();
    void pollInfo();
    void pollLive();
    void send(quint32 universe, int slot);

    /** The rig network gate (QLCIOPlugin::setTransmitNetworks()) for one
     *  device: may anything be sent to @a host? The local address the OS
     *  would send from must be on a rig network -- a device on the show
     *  network reached through a laptop's other (default) route is exactly
     *  what is held. A name rather than an address cannot be checked and is
     *  held while the gate is on. Asks the OS for the route (a UDP connect,
     *  which sends nothing), so never on a send path: its answer is kept in
     *  Device::gateOpen. Called without m_mutex. */
    bool routeAllowed(const QString &host) const;

public:
    /** @reimp: every device's Device::gateOpen, again -- the networks changed,
     *  or (the engine's reconnect tick) a route may have (DHCP, VPN, Wi-Fi) */
    void refreshTransmitGate() override;

private:
    static void setAnswered(Device &d, bool answered);
    Device &slot(Target &t, int index);
    void problemsMaybeChanged();
    QAtomicInt m_problemsCheckPending;     ///< 1 = a check is queued already

    QNetworkAccessManager *m_nam = nullptr;
    QTimer *m_sendTimer = nullptr;
    QTimer *m_infoTimer = nullptr;
    QTimer *m_liveTimer = nullptr;
    int m_sendInterval = 100;
    mutable QMutex m_mutex;         ///< m_targets: writeUniverse() runs on universe threads
    QHash<quint32, Target> m_targets;
    QHash<quint32, int> m_watchers; ///< watchTargetPixels() counts, by universe:
                                    ///< they outlive an output closed and reopened
    QStringList m_lastProblems;
};

#endif

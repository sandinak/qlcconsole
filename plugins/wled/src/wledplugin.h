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
#include <QHash>

#include "qlcioplugin.h"

class QNetworkAccessManager;
class QTimer;

#define WLED_OUTPUTIP   "outputIP"
#define WLED_ADDRESS    "wledAddress"
#define WLED_CHANNELS   "wledChannels"

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
    QStringList targetProblems() const override;

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

private:
    struct Target
    {
        QString ip;
        int address = 1;
        int channels = 15;
        QByteArray data;            ///< latest universe, from writeUniverse()
        QJsonObject sent;           ///< last state WLED acknowledged
        bool inFlight = false;
        bool infoInFlight = false;
        bool liveCleared = false;   ///< realtime (Art-Net/E1.31) mode left once
        int answered = -1;          ///< -1 not asked yet, 0 no answer, 1 answered
        int rssi = 0;               ///< dBm, 0 = unknown
        QString deviceName;
        QString version;
        int ledCount = 0;           ///< from /json/info, 0 = not known
        bool liveInFlight = false;
        QVector<quint32> pixels;    ///< /json/live: what the LEDs show
        qint64 pixelsAt = 0;        ///< when (QDateTime ms)
    };

    void tick();
    void pollInfo();
    void pollLive();
    void send(quint32 universe);
    void setAnswered(Target &t, bool answered);
    void problemsMaybeChanged();

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

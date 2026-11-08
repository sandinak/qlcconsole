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
#include "wledlayoutstore.h"

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
 * sends that, as JSON over HTTP (/json/state): only when it changes, one
 * request in flight per lantern (the latest wins). A held look sends
 * nothing at all.
 *
 * A snap goes at once (UX-73): a change in a lantern's channels wakes the
 * sender (kick()) a couple of milliseconds later -- a burst written in one
 * engine frame is one request -- with no transition when it is a step (on,
 * off, another effect, or the first change after the look stood still).
 * A fade is the app's own frames, sent at most once a send interval (ten a
 * second) with WLED's transition gliding between: the plugin is handed DMX,
 * not the cue, and the frames follow the cue's own time and curve, a
 * crossfade, a pause. Only what changed goes: the segments are left out
 * when they are as the lantern took them, so a GO into a look moved in
 * black is only its brightness.
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
 * Each lantern is also asked /json/si every few seconds -- its state and its
 * info in one answer: its name, its firmware, its Wi-Fi signal, its
 * SEGMENTS -- and whether it answers at all. targetProblems() says which do
 * not, or answer on a weak signal.
 *
 * The look goes to every segment the device has, by id. WLED applies a
 * "seg" array element without an id to the segment at that index, so a
 * lone element reached segment 0 only: on a fountain's string, written as
 * one segment per span (Write Segments), an effect played on the first span
 * alone. The effect's Reverse is relative to each segment's own direction
 * (its layout: every second span runs the other way), never written over
 * it. Nothing is sent to a device before its segments are known (its first
 * answer, one request); one that answers without segments (an old WLED with
 * no /json/si) is sent the single element as before.
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

    /** How often a watched lantern is asked what its LEDs show (ms).
     *  Invokable by name (QMetaObject::invokeMethod) for the UI tests,
     *  which reach the plugin through IOPluginCache. */
    Q_INVOKABLE void setLiveInterval(int ms);
    /** An answer older than this (ms) is not shown: the lantern stopped
     *  answering, and a frozen frame would say it was still running. */
    static const int liveStaleMs = 2000;

    /** How often a changed state may go out, per lantern (ms). Tests run it
     *  faster; WLED's transition matches it. */
    Q_INVOKABLE void setSendInterval(int ms);
    /** How often each lantern is asked how it is, and its segments (ms). */
    Q_INVOKABLE void setInfoInterval(int ms);

    /** Weaker than this (dBm) is reported: on stage Wi-Fi it will drop. */
    static const int weakSignalDbm = -75;
    /** A device that did not take a state is sent it again after this (ms),
     *  not every tick. */
    static const int retryMs = 1000;

private:
    /** One of a device's segments, as it last said them (/json/si). */
    struct Segment
    {
        int id = 0;
        int start = 0;
        int stop = 0;
        /** Its own direction -- the layout's (Write Segments reverses every
         *  second span) -- which the effect's Reverse flips, never replaces. */
        bool layoutRev = false;
        /** The rev it last took from us; -1 none. It says back what it was
         *  sent: a different rev was set by someone else (Write Segments, its
         *  own UI), and that is its layout now. */
        int sentRev = -1;
    };

    /** One WLED the universe is sent to. */
    struct Device
    {
        QString ip;
        int address = 0;            ///< 0: the first device's
        int channels = 0;           ///< 0: the first device's
        QJsonObject sent;           ///< last state WLED acknowledged (decoded, one segment)
        bool inFlight = false;
        bool infoInFlight = false;  ///< /json/si (or /json/info) on its way
        /** Asked next tick, ahead of any state. Sends and asks take turns,
         *  one request at a time: an answer is never older than a state the
         *  device took, so the segments' directions read in it are its own. */
        bool pollDue = true;
        bool layoutKnown = false;   ///< it answered once: segments (if any) known
        bool noSi = false;          ///< an old WLED without /json/si: /json/info only
        QVector<Segment> segments;  ///< by id; empty: the single element, as before
        quint32 layoutGen = 0;      ///< bumped when segments or their directions change
        quint32 sentLayoutGen = 0;  ///< layoutGen of the state WLED acknowledged
        bool liveCleared = false;   ///< realtime (Art-Net/E1.31) mode left once
        int answered = -1;          ///< -1 not asked yet, 0 no answer, 1 answered
        int rssi = 0;               ///< dBm, 0 = unknown
        QString deviceName;
        QString version;
        QString mac;                ///< from its info: what its kept layout is filed by
        int ledCount = 0;           ///< from /json/info, 0 = not known
        qint64 retryAt = 0;         ///< no answer: not sent again before (ms)
        qint64 postedAt = 0;        ///< when the last state went (ms)
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

    /** Every device due a state is sent it, every one due an ask is asked.
     *  @p kicked: woken by a change (kick()), not the send timer -- a state
     *  goes before an ask then, and nothing is asked. */
    void tick(bool kicked = false);
    /** A change in some lantern's channels (writeUniverse(), any thread):
     *  the sender wakes in kickCoalesceMs. */
    void kick();
    /** On the plugin's thread: the sender wakes in @p ms. */
    void kickIn(int ms);
    static const int kickCoalesceMs = 2;
    /** Whether @p data's @p count bytes from @p offset differ from @p old's
     *  (a byte past the end of either is 0). */
    static bool windowChanged(const QByteArray &old, const QByteArray &data, int offset, int count);
    void pollInfo();
    void poll(quint32 universe, int slot);
    void pollLive();
    void send(quint32 universe, int slot);
    /** The body for @a d: @a state's one segment sent to each of its
     *  segments by id, Reverse flipping each one's own direction; @a revs
     *  gets the (id, rev) each is sent. No segments known: @a state as is. */
    static QJsonObject message(const QJsonObject &state, const Device &d,
                               QVector<QPair<int, bool> > *revs = nullptr);
    /** A /json/state "seg" array as segments (the ones in use). */
    /** @p state, to go to a device that last took @p sent at @p postedAt:
     *  with no transition when it is a step rather than part of a fade --
     *  it turns the device on or off, changes its effect, or comes after the
     *  look stood still for two sends (a Locate's flashes, a cut). Only a
     *  level or colour moving send after send glides. */
    static QJsonObject paced(QJsonObject state, const QJsonObject &sent,
                             qint64 postedAt, qint64 now, int intervalMs);
    /** Whether @p state, after @p sent at @p postedAt, is a step (see
     *  paced()): it goes at once, whatever the pace. */
    static bool isStep(const QJsonObject &state, const QJsonObject &sent,
                       qint64 postedAt, qint64 now, int intervalMs);
    /** The body for @p state to a device that took @p sent: without its
     *  segments when they are as it took them (@p sameLayout: its segments
     *  have not changed since). */
    static QJsonObject trimmed(QJsonObject body, const QJsonObject &state,
                               const QJsonObject &sent, bool sameLayout);
    /** Whether @p a and @p b set the same look (their transitions aside). */
    static bool sameLook(QJsonObject a, QJsonObject b);
    static QVector<WledLayoutStore::Segment> segmentsOf(const QJsonArray &seg);
    /** What a device's answer says its segments are now; @p kept, the
     *  layout's own ways from the store when it is that layout (else
     *  null). True when the ways it knows now are to be kept. */
    static bool learnSegments(Device &d, const QJsonArray &seg,
                              const QVector<WledLayoutStore::Segment> *kept);
    /** Every segment of @p devices that plays the effect's Reverse is sent
     *  back its own way -- the output closing, the app quitting -- so the
     *  device is left as its layout, not turned round. Best effort, at most
     *  about a second however many do not answer; no event loop. */
    static void restoreDirections(const QVector<Device> &devices);
    static const int restoreBudgetMs = 1000;

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
    QTimer *m_kickTimer = nullptr;      ///< single shot: a change goes now
    QAtomicInt m_kickPending;           ///< 1 = a kick is queued already
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

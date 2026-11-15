/*
  Q Light Controller
  outputpatch.h

  Copyright (c) Heikki Junnila

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

#ifndef OUTPUTPATCH_H
#define OUTPUTPATCH_H

#include <QObject>
#include <QMap>
#include <climits>
#include <functional>
#include <atomic>

class QRecursiveMutex;

class QLCIOPlugin;

/** @addtogroup engine Engine
 * @{
 */

#define KOutputNone QObject::tr("None")

#define KXMLQLCOutputPatch          QStringLiteral("Patch")
#define KXMLQLCOutputPatchUniverse  QStringLiteral("Universe")
#define KXMLQLCOutputPatchPlugin    QStringLiteral("Plugin")
#define KXMLQLCOutputPatchOutput    QStringLiteral("Output")

class OutputPatch : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(OutputPatch)

    Q_PROPERTY(QString outputName READ outputName NOTIFY outputNameChanged)
    Q_PROPERTY(QString pluginName READ pluginName NOTIFY pluginNameChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool blackout READ blackout WRITE setBlackout NOTIFY blackoutChanged)

    /********************************************************************
     * Initialization
     ********************************************************************/
public:
    OutputPatch(QObject* parent = 0);
    OutputPatch(quint32 universe, QObject* parent = 0);
    virtual ~OutputPatch();

    /********************************************************************
     * Plugin & output
     ********************************************************************/
public:
    /**
     * Set the plugin to use and the plugin line number to output data on
     */
    bool set(QLCIOPlugin* plugin, quint32 output);

    /** Review 12 F7: set() with its target @p params, in one step -- they
     *  reach the plugin before any frame (the caller holds the frame lock),
     *  never the plugin's default target first */
    bool set(QLCIOPlugin *plugin, quint32 output, const QMap<QString, QVariant> &params);

    /**
     * Record a patch to @p plugin whose saved interface identity (@p uid --
     * an ArtNet interface's IP, for instance) does not currently match any
     * line the plugin offers -- a workspace opened on a machine that isn't
     * on the network it was built for, most commonly. Keeps the plugin
     * association and @p uid so the patch round-trips through saveXML()
     * unchanged and re-resolves correctly back on a machine that DOES have
     * that interface, but opens nothing: isPatched() is false and no
     * hardware line is ever opened while pending. See isPending().
     */
    void setPending(QLCIOPlugin *plugin, const QString &uid,
                    quint32 savedLine = UINT_MAX, const QString &network = QString());

    /** The numeric line and network ("172.18.2.0/24", may be empty) this
     *  pending patch was loaded with, written back unchanged by saveXML()
     *  so a show saved while offline is byte-for-byte what it was. */
    quint32 pendingLine() const { return m_pendingLine; }
    QString pendingNetwork() const { return m_pendingNetwork; }

    /** A pending patch found its network again: open @p line and replay
     *  every plugin parameter (target IP, port universe...) onto it. */
    bool resolvePending(quint32 line);

    /** True while this patch remembers an interface identity that did not
     *  resolve to any line the plugin currently offers -- see setPending().
     *  A pending patch is deliberately inert: nothing is opened and nothing
     *  is sent, but the mapping itself is not lost. */
    bool isPending() const;

    /** Pending because the rig network gate holds it (see
     *  InputOutputMap::transmitGateActive()): the line exists, but it is not
     *  on one of the show's rig networks. heldReason() says why. */
    bool isHeld() const { return isPending() && m_heldReason.isEmpty() == false; }
    QString heldReason() const { return m_heldReason; }
    void setHeldReason(const QString &reason) { m_heldReason = reason; }

    /**
     * If a valid plugin and line have been set, close
     * the output line and re-open it again (by its number) -- under the
     * frame lock.
     */
    bool reconnect();

    /*
     * Show-only fixtures (lasers, foggers, flame) need a network DMX output
     * (QLCIOPlugin::isNetworkDmxOutput(): Art-Net, E1.31/sACN, WLED) --
     * reviews 15-16, kept by InputOutputMap. With no Show-only fixture on a
     * universe, everything is exactly as stock QLC+ does it.
     *
     *  N1. A universe with Show-only fixtures may not have an output patch
     *      on any other plugin (USB DMX, DMX4Linux, SPI, OLA, UART, GPIO,
     *      MIDI, HID, Loopback, OSC, anything unknown). Patching one there is
     *      refused, through every path (setOutputPatch(): the I/O tab,
     *      Connections, Patch Planner, a retarget, the web, a venue's patch,
     *      undo and redo), saying InputOutputMap::networkOnlyText(). A
     *      feedback patch is not output: allowed.
     *  N2. A fixture (or a group, a patch of several, a remap) made Show
     *      only in a universe with such a patch holds that patch: every
     *      channel of every fixture being marked is marked first
     *      (InputOutputMap::ShowOnlyMarks), then the line gets its closing
     *      frame (the whole Show-only mask at safe) and a flush, and only
     *      then it is closed -- on a line another universe also uses, it is
     *      only forgotten, no frame. It waits on the line it was open on.
     *  N3. On load, a universe with Show-only fixtures whose saved output is
     *      such a line stays pending, with N1's words -- checked before any
     *      output is let go (InputOutputMap::recheckPatchesAfterLoad()); it
     *      waits on its saved line.
     *  N4. A universe with no Show-only fixture any more (the rule taken off,
     *      the fixture deleted or moved): its patches held by N2/N3 come back
     *      as stock does -- by name (with H5's saved-line preference), by
     *      number when it has no name -- never onto a line another patch has
     *      or another hold reserves.
     *  F3. A held patch whose line has no name (dangling, its interface away
     *      when the show was saved) is held by its plugin and line number:
     *      pending, saved with its line, said as "line N of <plugin>".
     *  F4. While a universe is held by N2/N3, the interface it was held off
     *      (its name -- every line of that name -- or its plugin and line
     *      number) is reserved: no other universe's output opens onto it by
     *      itself (by number on a rescan, by name, on retry or on load), and
     *      a hand patch onto it is refused, saying InputOutputMap::
     *      reservedText(). One kept off it comes back by number once its
     *      line is not reserved. The reservation ends when the held universe
     *      comes back (N4) or is patched elsewhere.
     *
     *  H5 (all universes): on load, a name more than one line carries
     *  resolves to the saved line number when that line carries it, else to
     *  the first, as stock does.
     */

    /** Why a pending patch is held by these rules */
    enum HoldKind
    {
        NotHeld = 0,
        /** N2/N3: its universe has Show-only fixtures */
        HeldShowOnly,
        /** F4: its line is an interface a held universe reserves */
        HeldReserved
    };
    /** Held, @p why said: kept pending on the name of the line it was on
     *  (or by number, F3, when it has none) and @p waitLine (UINT_MAX: the
     *  line it is on). @p closeTheLine: the line's closing frame first
     *  (@p sendClosing), then closed; false: the plugin only forgets this
     *  universe there (another patch on it closes it). @p sharedLine: it
     *  shared that line with another patch when held (review 17: F4 lets it
     *  back there by number, as stock, even if that one is open). */
    void hold(HoldKind kind, const QString &why, bool closeTheLine, bool sendClosing,
              quint32 waitLine = UINT_MAX, bool sharedLine = false);
    /** The line number a show load saved for this patch, when it opened
     *  it (InputOutputMap::loadPatch()); UINT_MAX otherwise. N3's hold
     *  waits there; any other set() forgets it. */
    void setLoadedLine(quint32 line) { m_loadedLine = line; }
    quint32 loadedLine() const { return m_loadedLine; }
    /** A pending patch these rules keep pending: said */
    void markHold(HoldKind kind, const QString &why) { m_holdKind = kind; m_reconnectHeld = why; }
    HoldKind holdKind() const { return m_holdKind; }
    /** Held by N2/N3 */
    bool showOnlyHold() const { return m_holdKind == HeldShowOnly; }
    /** Review 17: its universe has no Show-only fixture left -- plain
     *  pending (by name, H5) from here, its reservation gone */
    void releaseShowOnlyHold() { m_holdKind = NotHeld; m_fromShowOnlyHold = true; m_reconnectHeld.clear(); }
    /** It was held by N2/N3 until its universe lost its Show-only fixtures */
    bool fromShowOnlyHold() const { return m_fromShowOnlyHold; }
    /** See hold() */
    bool sharedLine() const { return m_sharedLine; }
    /** Why this patch is held; empty if not */
    QString reconnectHeld() const { return m_reconnectHeld; }
    /** Say why it waits, its hold unchanged */
    void setReconnectHeld(const QString &why) { m_reconnectHeld = why; }
    /** Pending on its line number only (F3): no name to wait for */
    bool pendingByNumber() const { return isPending() && m_pendingByNumber; }
    void setPendingByNumber(bool on) { m_pendingByNumber = on; }
    /** The interface this patch is on or waits for: its name, or "line N
     *  of <plugin>" (F3) */
    QString deviceText() const;
    static QString deviceText(const QLCIOPlugin *plugin, quint32 line, const QString &name);

    /** Review 18: asked, when an open patch is re-aimed (a target parameter,
     *  isTargetParameter()), whether its universe may send to the new
     *  target (N1); a non-empty answer holds it (HeldShowOnly), the closing
     *  frame going to the old target first. The Universe sets it from the
     *  I/O map; unset, nothing is asked. */
    typedef std::function<QString(const OutputPatch *patch, const QMap<QString, QVariant> &params)> TargetCheck;
    void setTargetCheck(const TargetCheck &check) { m_targetCheck = check; }

    /** Review 19 (A1): whether another open patch of this universe uses
     *  @p line of this plugin -- then a hold or the destructor neither
     *  closes nor forgets (line, universe), which is that patch's too. The
     *  Universe sets it. */
    typedef std::function<bool(const OutputPatch *patch, quint32 line)> SiblingOnLine;
    void setSiblingOnLine(const SiblingOnLine &check) { m_siblingOnLine = check; }

    /** Review 19 (C1): what the plugin itself was set to (a protocol's
     *  settings window, past this patch) taken into this patch's
     *  parameters, so it is saved and re-checked as it is */
    void adoptPluginParameters(const QMap<QString, QVariant> &values);

    /** The line it was open on when it was held (UINT_MAX: never held) */
    quint32 heldOnLine() const { return m_heldOnLine; }

    /** Two parameter values that mean the same ("1" and 1) */
    static bool sameParameterValue(const QVariant &a, const QVariant &b);

    /** The plugin instance that has been assigned to a patch */
    QLCIOPlugin* plugin() const;

    /** What a line is sent last, before it stops receiving this universe
     *  (Show only, S0): @p source fills the frame (and returns true) or
     *  says there is none to send (false). Asked for whenever an open line
     *  is closed for good -- re-patched to another line, put on hold
     *  (setPending()), unpatched, the universe removed, this patch
     *  destroyed -- never for a reconnect() of the same line. The Universe
     *  sets it to its held frame (Universe::closingFrame()); unset, a line
     *  is closed as it always was. */
    void setClosingFrame(const std::function<bool(QByteArray &frame, bool blackout)> &source);

    /** The universe's frame lock (Universe::m_frameMutex): taken around
     *  every change of the line or its target that does not come through
     *  the universe (reconnect(), a retarget), so no frame is sent while
     *  the line moves. Unset: nothing is locked (a patch of no universe). */
    void setFrameLock(QRecursiveMutex *lock) { m_frameLock = lock; }

    /** Friendly name of the plugin assigned to a patch ("None" if none) */
    QString pluginName() const;

    /** An output line provided by the assigned plugin */
    quint32 output() const;

    /** Friendly name of the assigned output line */
    QString outputName() const;

    /** Returns true if a valid plugin line has been set */
    bool isPatched() const;

    /** Set a parameter specific to the patched plugin. One that aims the
     *  line elsewhere (another node, port or universe: isTargetParameter())
     *  on an open line sends the old target its closing frame first. */
    void setPluginParameter(QString prop, QVariant value);

    /** Several at once, under the frame lock: one closing frame for the
     *  old target, and no frame sent half-way (a new address on the old
     *  port). UniversePatcher::applyTarget(). */
    void setPluginParameters(const QMap<QString, QVariant> &values,
                             const QStringList &unset = QStringList());

    /** True for a parameter that changes where an open line's frames land
     *  (Art-Net outputIP/outputUni, E1.31 universe/unicast/multicast, a
     *  WLED device's address or window) */
    static bool isTargetParameter(const QString &prop);

    /**
     * Drop a plugin parameter entirely, as opposed to setting it empty.
     *
     * The difference is real: an absent "outputIP" means the plugin's own
     * default (broadcast, for Art-Net), while an empty one is a configured
     * destination of nowhere. Repointing a patch back to broadcast has to
     * remove the parameter, not blank it.
     */
    void unSetPluginParameter(QString prop);

    /** Retrieve the map of custom parameters set to the patched plugin */
    QMap<QString, QVariant> getPluginParameters();

signals:
    void outputNameChanged();
    void pluginNameChanged();

private:
    /** The reference of the plugin associated by this Output patch */
    QLCIOPlugin* m_plugin;
    /** The plugin line open by this Output patch */
    quint32 m_pluginLine;
    /** The universe that this Output patch is attached to */
    quint32 m_universe;
    /** The interface identity this patch is waiting to find again, or
     *  empty when not pending -- see setPending()/isPending(). */
    QString m_pendingUID;
    quint32 m_pendingLine;
    QString m_pendingNetwork;
    /** Why the rig network gate holds this patch; empty when it does not */
    QString m_heldReason;
    QString m_reconnectHeld;
    HoldKind m_holdKind = NotHeld;
    bool m_pendingByNumber = false;
    bool m_fromShowOnlyHold = false;
    bool m_sharedLine = false;
    quint32 m_loadedLine = UINT_MAX;
    /** The patch parameters cache */
    QMap<QString, QVariant>m_parametersCache;
    /** See setClosingFrame() */
    std::function<bool(QByteArray &, bool)> m_closingFrame;
    TargetCheck m_targetCheck;
    SiblingOnLine m_siblingOnLine;
    quint32 m_heldOnLine = UINT_MAX;
    bool siblingOnLine(quint32 line) const { return m_siblingOnLine && m_siblingOnLine(this, line); }

    /** Close the open line for good: its closing frame first (see
     *  setClosingFrame()), then the plugin's closeOutput() */
    void closeLine();
    /** The closing frame to the line's current target, on the wire
     *  (QLCIOPlugin::flushOutput()) -- the line still open */
    void sendClosingFrame();
    /** The line's name when it was opened: what a hold waits on */
    QString m_lineName;
    QRecursiveMutex *m_frameLock = nullptr;

    /********************************************************************
     * Value dump
     ********************************************************************/
public:
    /** Get/Set the output patch pause state */
    bool paused() const;
    void setPaused(bool paused);

    /** Get/Set the output patch blackout state */
    bool blackout() const;
    void setBlackout(bool blackout);

    /** Write the contents of a 512 channel value buffer to the plugin.
      * Called periodically by OutputMap. No need to call manually. */
    void dump(quint32 universe, const QByteArray &data, bool dataChanged);

signals:
    void pausedChanged(bool paused);
    void blackoutChanged(bool blackout);

private:
    /** A line just opened: its first frame goes out as a changed one (a
     *  plugin that sends only changes must not skip it) */
    std::atomic<bool> m_freshLine { false };
    /** A buffer used when this output patch is paused */
    QByteArray m_pauseBuffer;
    bool m_paused;
    bool m_blackout;
};

/** @} */

#endif

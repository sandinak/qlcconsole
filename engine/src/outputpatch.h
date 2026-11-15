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
     * Hot-plug on hardware lines (QLCIOPlugin::linesAreHardware(): USB,
     * MIDI and the like) with Show-only fixtures -- review 13's rules,
     * kept by InputOutputMap (slotPluginConfigurationChanged(),
     * retryOfflinePatches(), loadPatch()). With no Show-only universe
     * anywhere, everything is exactly as stock QLC+ does it.
     *
     *  H1. A Show-only universe on a hardware line reopens by itself in one
     *      case only: its recorded device (a unique name) is back at the
     *      same line number. Pending from load, the name changed, the device
     *      back at another number, an identical-name count change: it stays
     *      held -- "<device> missing or moved: re-patch to use it" -- until
     *      the operator re-patches it (I/O tab, Connections).
     *  H2. No other universe, ordinary or Show-only, is ever opened by
     *      itself -- by number, by name, by retry or on load -- onto a line
     *      named as the recorded device of a Show-only universe that is
     *      open, held or pending from load. It is held instead, and says
     *      which interface took its line.
     *  H3. A held ordinary universe reopens when its recorded device is
     *      back at its own number, or anywhere if the name is unique and the
     *      line is free and not protected by H2.
     *  H4. There is no "reconnect as before": identical names with a
     *      Show-only universe recorded on that name are held (H1, H2) until
     *      re-patched.
     *  H5. On load, a name that more than one line carries resolves to the
     *      saved line number when that line carries it; otherwise a
     *      Show-only universe stays pending (H1), an ordinary one takes the
     *      first, as stock does. For such names H2 holds by line on load:
     *      nothing loads onto the line a Show-only universe has or was
     *      saved on. (The I/O map is saved before the fixtures: a file
     *      load holds every output until the show is read, then checks each
     *      patch it opened against H1, H2, H5 -- InputOutputMap::
     *      recheckPatchesAfterLoad() -- before the first frame.)
     *  Once no Show-only universe is left, every hold made for these
     *  reasons is let go as stock would have reconnected it (by number).
     */

    /** A hardware line (USB, MIDI) whose name now is not the one it had
     *  when this patch was made: another device took its number */
    bool lineNameChanged() const;
    /** The name of the device this patch was made to (a hardware line):
     *  open, held for a device change, or pending on it from load (its
     *  saved name); empty otherwise */
    QString recordedLineName() const;
    /** How many lines carried that name when the patch was (re)opened */
    int recordedLineNameCount() const { return m_lineNameCount; }
    int lineNameCountNow() const;

    /** How a patch is held for a device change */
    enum DeviceHold
    {
        NoDeviceHold = 0,
        /** Held by the rules above; reconnectHeld() says why */
        HeldDeviceChanged
    };
    /** Held by the rules above, @p why said: kept pending on the name it
     *  was patched with and its number. @p closeTheLine false: another open
     *  patch uses the line, which stays open for it (the plugin only
     *  forgets this universe there). */
    void holdForDeviceChange(const QString &why, bool closeTheLine, bool sendClosing = false);
    /** The line number a show load saved for this patch, when it opened
     *  it (InputOutputMap::loadPatch()); UINT_MAX otherwise. The check after
     *  the load (H1, H5) reads it; any other set() forgets it. */
    void setLoadedLine(quint32 line) { m_loadedLine = line; }
    quint32 loadedLine() const { return m_loadedLine; }
    /** A pending patch (from load, or held) the rules keep pending: said */
    void markDeviceHold(const QString &why) { m_deviceHold = HeldDeviceChanged; m_reconnectHeld = why; }
    DeviceHold deviceHold() const { return m_deviceHold; }
    /** It was open when it was held (a hot-plug): let go, it reopens by
     *  number, as stock would have; else it resolves as a load does */
    bool heldFromOpen() const { return m_heldFromOpen; }
    /** Let go of the hold (no Show-only universe left): plain pending */
    void clearDeviceHold() { m_deviceHold = NoDeviceHold; m_reconnectHeld.clear(); m_heldFromOpen = false; }
    /** Why this patch is held; empty if not */
    QString reconnectHeld() const { return m_reconnectHeld; }

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
    DeviceHold m_deviceHold = NoDeviceHold;
    bool m_heldFromOpen = false;
    quint32 m_loadedLine = UINT_MAX;
    int m_lineNameCount = 0;
    /** The patch parameters cache */
    QMap<QString, QVariant>m_parametersCache;
    /** See setClosingFrame() */
    std::function<bool(QByteArray &, bool)> m_closingFrame;

    /** Close the open line for good: its closing frame first (see
     *  setClosingFrame()), then the plugin's closeOutput() */
    void closeLine();
    /** The closing frame to the line's current target, on the wire
     *  (QLCIOPlugin::flushOutput()) -- the line still open */
    void sendClosingFrame();
    /** The open line's name when it was opened: reconnect() follows it */
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

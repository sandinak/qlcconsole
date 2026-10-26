/*
  Q Light Controller Plus
  inputoutputmap.h

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

#ifndef INPUTOUTPUTMAP_H
#define INPUTOUTPUTMAP_H

#include <QSharedPointer>
#include "qlcengine_export.h"
#include <QObject>
#include <QMutex>
#include <QDir>

#include "qlcinputprofile.h"
#include "universe.h"
#include "grandmaster.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class QElapsedTimer;
class QTimer;
class QLCInputSource;
class AudioCapture;
class QLCIOPlugin;
class OutputPatch;
class InputPatch;
class Universe;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLIOMap               QStringLiteral("InputOutputMap")
#define KXMLIOBeatGenerator     QStringLiteral("BeatGenerator")
#define KXMLIOBeatType          QStringLiteral("BeatType")
#define KXMLIOBeatsPerMinute    QStringLiteral("BPM")
#define KXMLIORigNetwork        QStringLiteral("RigNetwork")

class QLC_ENGINE_EXPORT InputOutputMap final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(InputOutputMap)

    friend class InputPatch;

    /*********************************************************************
     * Initialization
     *********************************************************************/

public:
    /**
     * Create a new InputOutputMap object
     *
     * @param doc The QLC+ project reference
     * @param universes Number of universes
     */
    InputOutputMap(const Doc* doc, quint32 universesCount);

    /**
     * Destroy a InputOutputMap object
     */
    ~InputOutputMap();

private:
    const Doc *m_doc;

    /*********************************************************************
     * Blackout
     *********************************************************************/
public:
    enum BlackoutRequest
    {
        BlackoutRequestNone,
        BlackoutRequestOn,
        BlackoutRequestOff
    };

    /**
     * Toggle blackout between on and off.
     *
     * @return New blackout state (i.e. after toggling)
     */
    bool toggleBlackout();

    /**
     * Set blackout on or off
     *
     * @param blackout If true, set blackout ON, otherwise OFF
     * @return true if blackout state changed
     */
    bool setBlackout(bool blackout);

    /**
     * Schedule blackout on or off
     *
     * Scripts toggling blackout cannot wait for m_UniverseMutex to unlock
     * since they are within locked mutex. The solution is to toggle the blackout
     * later during dumpUniverses()
     *
     * @param blackout If true, set blackout ON, otherwise OFF
     */
    void requestBlackout(BlackoutRequest blackout);

    /**
     * Get blackout state
     *
     * @return true if blackout is ON, otherwise false
     */
    bool blackout() const;

private:
    /** Apply blackout to every universe and push output. The caller MUST hold
     *  m_universeMutex (or otherwise be serialised against the MasterTimer tick).
     *  Returns true if the state changed. Does not emit. */
    bool applyBlackout(bool blackout);

public:

    /**
     * Fork "Blind" support. Inhibits physical output on every universe (the
     * output plugins receive nothing) while leaving universeWritten() intact,
     * so the 2D monitor / preview still reflects the look. Independent of
     * blackout; used by the Programming tab to build looks without the stage
     * rig responding. Not persisted.
     *
     * @param inhibit If true, mute all physical output, otherwise resume
     * @return true if the state changed
     */
    bool setOutputInhibited(bool inhibit);

    /** Get the current Blind (output-inhibited) state. */
    bool outputInhibited() const;

signals:
    /**
     * Signal that is sent when blackout state is changed.
     *
     * @param state true if blackout has been turned on, otherwise false
     */
    void blackoutChanged(bool state);

    /** Sent when the Blind (output-inhibited) state changes. */
    void outputInhibitedChanged(bool state);

private:
    /** Current blackout state */
    bool m_blackout;
    /** Current Blind (output-inhibited) state */
    bool m_outputInhibited = false;

    /*********************************************************************
     * Universes
     *********************************************************************/
public:

    /**
     * Invalid universe number (for comparison etc.)
     */
    static quint32 invalidUniverse();

    /**
     * The most universes a show can have. QLC+ itself sets no limit, but a
     * universe is a set of 512-channel buffers, written every frame, so a
     * damaged or hand-edited show naming universe 65536 made the load add
     * 65537 of them, one at a time, and hang. Far above any rig a console
     * like this drives.
     */
    static quint32 maxUniverses();

    /**
     * Add a new universe and append it at the end of the
     * current universes list. Refused (false) at maxUniverses().
     */
    bool addUniverse(quint32 id = InputOutputMap::invalidUniverse());

    /**
     * Remove the last universe in the current universes list
     */
    bool removeUniverse(int index);

    /**
     * Remove all the universes in the current universes list
     */
    bool removeAllUniverses();

    /**
     * Start all the Universe threads
     */
    void startUniverses();

    /**
     * Get the unique ID of the universe at the given index
     * @param index The universe index
     * @return The universe ID or invalidUniverse()
     */
    quint32 getUniverseID(int index);

    /**
     * Retrieve the friendly name of the universe at the given index
     * @param index The universe index
     * @return The universe name or an empty string
     */
    QString getUniverseNameByIndex(int index);

    /**
     * Retrieve the friendly name of the universe with the given ID
     * @param id The universe unique ID
     * @return The universe name or an empty string
     */
    QString getUniverseNameByID(quint32 id);

    /**
     * Set a friendly name of the universe with the given index
     * @param index The universe index
     * @param name The universe new name
     */
    void setUniverseName(int index, QString name);

    /**
     * Set/unset the universe with the given index in passthrough mode
     * @param index The universe index
     * @param enable true = passthrough, false = normal mode
     */
    void setUniversePassthrough(int index, bool enable);

    /** The slot the universe at @p index is in the show: Tour or House
     *  (VENUES.md section 5). */
    void setUniverseSlotOwner(int index, RigOwner::Owner owner);
    RigOwner::Owner universeSlotOwner(int index) const;

    /**
     * Retrieve the passthrough mode of the universe at the given index
     * @param index The universe index
     * @return true = passthrough, false = normal mode
     */
    bool getUniversePassthrough(int index);

    /**
     * Enable/disable the monitor mode for the universe with the given index
     * @param index The universe index
     * @param enable true = monitor, false = do not monitor
     */
    void setUniverseMonitor(int index, bool enable);

    /**
     * Retrieve the monitor mode of the universe at the given index
     * @param index The universe index
     * @return true = monitor, false = do not monitor
     */
    bool getUniverseMonitor(int index);

    /**
     * Return if a universe is patched with any input, output or
     * feedback line
     * @param index The universe index
     * @return true = patched, false = not patched
     */
    bool isUniversePatched(int index);

    /**
     * Retrieve the number of universes in the input/output map
     */
    quint32 universesCount() const;

    /**
     * Retrieve the list of references of the Universe in the input/output map
     */
    QList<Universe*> universes() const;

    /**
     * Get a reference to a Universe from the given Universe ID
     * Return NULL if no Universe is found
     */
    Universe *universe(quint32 id);

    /**
     * Claim access to a universe. This is declared virtual to make
     * unit testing a bit easier.
     */
    virtual QList<Universe*> claimUniverses();

    /**
     * Release access to a universe. This is declared virtual to make
     * unit testing a bit easier.
     *
     * @param changed Set to true if DMX values were changed
     */
    virtual void releaseUniverses(bool changed = true);

    /**
     * Reset all universes (useful when starting from scratch)
     */
    void resetUniverses();

signals:
    void universeAdded(quint32 id);
    void universeRemoved(quint32 id);
    void universeWritten(quint32 index, const QByteArray& universesData);

private:
    /** The values of all universes */
    QList<Universe *> m_universeArray;

    /** When true, universes are dumped. Otherwise not. */
    bool m_universeChanged;

    /** Mutex guarding m_universeArray */
    QMutex m_universeMutex;

    /*********************************************************************
     * Grand Master
     *********************************************************************/
public:
    /**
     * Set grand master channel mode (intensity or all channels)
     */
    void setGrandMasterChannelMode(GrandMaster::ChannelMode mode);

    /**
     * Get grand master channel mode (intensity or all channels)
     */
    GrandMaster::ChannelMode grandMasterChannelMode();

    /**
     * Set grand master value mode (limit or reduce)
     */
    void setGrandMasterValueMode(GrandMaster::ValueMode mode);

    /**
     * Set grand master value mode (limit or reduce)
     */
    GrandMaster::ValueMode grandMasterValueMode();

    /**
     * Set grand master value (0-255)
     */
    void setGrandMasterValue(uchar value);

    /**
     * Get grand master value (0-255)
     */
    uchar grandMasterValue();

signals:
    void grandMasterValueChanged(uchar value);
    void grandMasterValueModeChanged(GrandMaster::ValueMode mode);

private:
    /** The Grand Master reference */
    GrandMaster *m_grandMaster;

    /*********************************************************************
     * Patch
     *********************************************************************/

public:
    void flushInputs();

    /**
     * Patch the given universe to go through the given input plugin
     *
     * @param universe The input universe to patch
     * @param pluginName The name of the plugin to patch to the universe
     * @param inputUID Unique plugin input line identifier as string
     * @param input An input universe provided by the plugin to patch to
     * @param profileName The name of an input profile
     * @return true if successful, otherwise false
     */
    bool setInputPatch(quint32 universe, const QString& pluginName,
                       const QString& inputUID, quint32 input,
                       const QString& profileName = QString());

    /**
     * Set an input profile to the given universe. If the universe doesn't
     * have an input patch, this method returns false
     *
     * @param universe The universe to patch
     * @param profileName the name of the input profile to set
     * @return true if successful, otherwise false
     */
    bool setInputProfile(quint32 universe, const QString& profileName);

    /**
     * Patch the given universe to go through the given output plugin
     *
     * @param universe The universe to patch
     * @param pluginName The name of the plugin to patch to the universe
     * @param inputUID Unique plugin output line identifier as string
     * @param output A universe provided by the plugin to patch to
     * @param isFeedback Determine if this line is a feedback output
     * @param index The output patch index
     *
     * @return true if successful, otherwise false
     */
    bool setOutputPatch(quint32 universe, const QString& pluginName,
                        const QString& outputUID, quint32 output = 0,
                        bool isFeedback = false, int index = 0);

    int outputPatchesCount(quint32 universe) const;

    /**
     * Get mapping for an input universe.
     *
     * @param universe The internal input universe to get mapping for
     */
    InputPatch* inputPatch(quint32 universe) const;

    /**
     * Get the output mapping for a QLC universe.
     *
     * @param universe The internal universe to get mapping for
     */
    OutputPatch* outputPatch(quint32 universe, int index = 0) const;

    /**
     * A universe whose output patch does not resolve to a real line on this
     * machine, in one of two ways:
     *
     * - PENDING: the patch remembers an interface identity (an ArtNet IP,
     *   say) that this plugin does not currently offer -- a workspace
     *   opened on a machine that is not on the network it was built for.
     *   Deliberately inert (OutputPatch::isPending()): nothing is opened,
     *   nothing is sent, and the mapping is preserved for when the right
     *   network IS present again. missingInterface names what is missing.
     *
     * - out of range: an older patch with no recorded interface identity at
     *   all, only a numeric line index that is out of bounds on this
     *   machine (workspaces store the output line by INDEX, and for network
     *   plugins that index is a position in the local interface list).
     *   missingInterface is empty for this case; line/availableLines are 0
     *   for the pending case above.
     */
    struct DanglingPatch
    {
        quint32 universe;       //!< 0-based universe index
        QString pluginName;
        quint32 line;           //!< the line index the workspace asked for (out-of-range case)
        int availableLines;     //!< how many that plugin actually offers here (out-of-range case)
        QString missingInterface;   //!< the interface identity that did not resolve (pending case)
        QString kind = QStringLiteral("output");   //!< "output", "input" or "feedback"
        bool pluginMissing = false; //!< the plugin itself is not installed here
        QString heldReason;         //!< pending because the rig network gate holds it
    };

    /** Output patches pointing at plugin lines this machine doesn't have.
     *  Empty means every patched universe can actually reach its output. */
    QList<DanglingPatch> danglingOutputPatches() const;

    /**
     * Apply one patch exactly as a workspace stored it (Universe::loadXML).
     *
     * Network lines are resolved by NETWORK, not by interface: see
     * NetworkRoute::resolve(). A patch that cannot be opened on this
     * machine -- interface/network not present, device unplugged, plugin
     * not installed -- is KEPT (an output as OutputPatch::isPending(), an
     * input/feedback or plugin-less one as a Universe::OfflinePatch), saved
     * back unchanged, and reconnected by retryOfflinePatches() as soon as
     * it resolves. It is never silently dropped.
     */
    void loadPatch(quint32 universe, const Universe::OfflinePatch &patch);

    /** A venue's patch on (or the show's back): every output patch of
     *  @p universe -- live, pending, kept -- replaced by @p patches, each
     *  applied as loadPatch() applies one from a file. */
    void replaceOutputPatches(quint32 universe, const QList<Universe::OfflinePatch> &patches);

    /** Try every kept (offline/pending) patch again; returns how many
     *  reconnected. Runs by itself every few seconds while any exist. */
    int retryOfflinePatches();

    /** Are any patches being kept offline right now? */
    bool hasOfflinePatches() const;

    /** The user unpatched an offline universe: forget every kept patch on
     *  it (pending outputs included). */
    void forgetOfflinePatches(quint32 universe);

    /** How a patch with saved identity @p uid would resolve here: a line
     *  index, or -1 when nothing on this machine reaches it. */
    int resolveSavedLine(QLCIOPlugin *plugin, bool output, const QString &uid,
                         const QString &network, const QString &target) const;

    /*********************************************************************
     * Rig network transmit gate
     *********************************************************************/
public:
    /**
     * The show's rig networks ("172.18.0.0/20"), saved with the show: the
     * networks its network output belongs on. While the gate is active
     * (transmitGateActive()), a network output patch -- Art-Net, E1.31/sACN,
     * OSC, WLED: any line named after a local address, or any patch aimed at
     * IPv4 targets -- transmits only from a local interface whose address
     * lies inside one of them. Anything else is HELD: kept as a pending
     * patch (OutputPatch::isHeld()), its plugin line not opened, nothing
     * sent, no unicast through the default route and no broadcast. The
     * offline retry tick re-checks held patches with the reconnect check and
     * they resume by themselves once this machine is on a rig network.
     *
     * Loopback (127.0.0.0/8) is never held, and patches that are not network
     * output (USB DMX, MIDI, ...) are not looked at. With no rig networks
     * set nothing is held -- an older show behaves as it always did -- and
     * transmitGateStatus() says so, for the footer to suggest setting them.
     *
     * The plugins enforce the same list themselves for everything a line
     * sends (QLCIOPlugin::setTransmitNetworks()), which also covers what a
     * patch hold cannot see: an input line answering ArtPolls, a rescan's
     * discovery polls. They keep the decision cached per line or device and
     * work it out again when the list changes and on the retry tick, so the
     * send path only reads a flag.
     */
    QStringList rigNetworks() const;

    /** Set the rig networks; entries that do not parse are dropped (see
     *  RigNetwork::normalise()). Re-applies the gate at once. */
    void setRigNetworks(const QStringList &networks);

    /** A starting suggestion for a show that has none: the distinct saved
     *  Network values of its network patches (open, held, offline). */
    QStringList suggestedRigNetworks() const;

    /** "Only transmit on rig networks" (a preference of this machine,
     *  default on). Re-applies the gate. */
    bool transmitGateEnabled() const;
    void setTransmitGateEnabled(bool enable);

    /** "Transmit anyway, this session": lifts the gate until the app quits
     *  or another show is opened. Never saved. */
    bool transmitOverride() const;
    void setTransmitOverride(bool override);

    /** Enabled, not overridden, and the show has rig networks */
    bool transmitGateActive() const;

    /** Why a patch on @p lineName with @p params may not transmit right now,
     *  or empty when it may (or is not network output at all). */
    QString transmitHoldReason(const QString &lineName,
                               const QMap<QString, QVariant> &params) const;

    struct TransmitGateStatus
    {
        enum State
        {
            Clear,          //!< nothing to say: no network output, or all allowed
            NotSet,         //!< network output, but the show has no rig networks
            Held,           //!< network output is being held
            Off,            //!< the check is switched off on this machine
            Overridden      //!< "Transmit anyway, this session"
        };
        State state = Clear;
        int held = 0;                   //!< network output patches held
        int networkOutputs = 0;         //!< network output patches in all
        QStringList networks;           //!< the rig networks
        QStringList connected;          //!< "en0 10.4.12.7", this machine's own
        QStringList reasons;            //!< "Universe 3: 10.4.12.7 is not on ..."
    };

    /** What the footer (and anything else asking) should say */
    TransmitGateStatus transmitGateStatus() const;

    /** Hold every open network output patch that may not transmit, push
     *  the gate to the plugins, and resume held ones that may again.
     *  Returns how many were newly held. Runs on the retry tick. */
    int applyTransmitGate();

signals:
    /** The gate's settings or what it is holding changed */
    void transmitGateChanged();

private:
    /** Hold output patch @p index of @p universe (open or not) */
    void holdOutputPatch(quint32 universe, int index, QLCIOPlugin *plugin,
                         const QString &uid, quint32 savedLine,
                         const QString &network, const QString &reason);
    /** Push the effective network list into every loaded plugin, and have
     *  each work its cached per-line decisions out again */
    void pushTransmitNetworks();
    /** Is a patch on @p lineName with @p params network output (a line named
     *  after a non-loopback local address, or aimed at IPv4 targets)? */
    bool isNetworkOutput(const QString &lineName, const QMap<QString, QVariant> &params) const;
    /** Has this machine an address on one of the rig networks? */
    bool onRigNetwork() const;
    /** Re-apply the gate after a setting changed, and announce it */
    void reapplyTransmitGate();
    /** Emit transmitGateChanged() when the status summary changed */
    void noteTransmitGateStatus();

    QStringList m_rigNetworks;
    bool m_transmitGateEnabled = true;
    bool m_transmitOverride = false;
    QString m_lastGateSummary;

public:

    /**
     * Get the feedback mapping for a QLC universe.
     *
     * @param universe The internal universe to get mapping for
     */
    OutputPatch* feedbackPatch(quint32 universe) const;

    /**
     * Get a list of available universes.
     */
    QStringList universeNames() const;

    /**
     * Check, whether a certain input in a certain plugin has been mapped
     * to a universe. Returns the mapped universe number or QLCIOPlugin::invalidLine()
     * if not mapped.
     *
     * @param pluginName The name of the plugin to check for
     * @param input The particular input to check for
     * @return Mapped universe number or -1 if not mapped
     */
    quint32 inputMapping(const QString& pluginName, quint32 input) const;

    /**
     * Check, whether a certain output in a certain plugin has been mapped
     * to a universe. Returns the mapped universe number or QLCIOPlugin::invalidLine()
     * if not mapped.
     *
     * @param pluginName The name of the plugin to check for
     * @param output The particular output to check for
     * @return Mapped universe number or -1 if not mapped
     */
    quint32 outputMapping(const QString& pluginName, quint32 output) const;

    /*********************************************************************
     * Plugins
     *********************************************************************/
public:
    /**
     * Get a description text for the given plugin.
     */
    QString pluginDescription(const QString& pluginName);

    /**
     * Get a list of available input plugins as a string list
     * containing the plugins' names
     *
     * @return QStringList containing plugins' names
     */
    QStringList inputPluginNames();

    /**
     * Get a list of available Output output plugins as a string list
     * containing the plugins' names
     *
     * @return QStringList containing plugins' names
     */
    QStringList outputPluginNames();

    /**
     * Get the names of all input lines provided by the given plugin.
     *
     * @param pluginName Name of the plugin, whose input count to get
     * @return A list containing the name of each input line
     *
     */
    QStringList pluginInputs(const QString& pluginName);

    /**
     * Get the names of all output lines provided by the given plugin.
     *
     * @param pluginName Name of the plugin, whose output count to get
     * @return A list containing the name of each output line
     */
    QStringList pluginOutputs(const QString& pluginName);

    /**
     * Check, whether a plugin supports feedback
     *
     * @param pluginName The name of the plugin to check from.
     * @return true if plugin supports feedback. Otherwise false.
     */
    bool pluginSupportsFeedback(const QString& pluginName);

    /**
     * Open a configuration dialog for the given plugin
     *
     * @param pluginName Name of the plugin to configure
     */
    void configurePlugin(const QString& pluginName);

    /**
     * Check, whether a plugin provides additional configuration options.
     *
     * @param pluginName The name of the plugin to check from.
     * @return true if plugin can be configured. Otherwise false.
     */
    bool canConfigurePlugin(const QString& pluginName);

    /**
     * Get a status text for the given plugin.
     *
     * @param pluginName Name of the plugin, whose status to get
     * @param input A specific input identifier
     */
    QString inputPluginStatus(const QString& pluginName, quint32 input);

    /**
     * Get a status text for the given plugin. If no plugin name is
     * given, an overall mapping status of all universes is returned.
     *
     * @param pluginName Name of the plugin, whose status to get
     * @param output Plugin's output line for getting more specific info
     */
    QString outputPluginStatus(const QString& pluginName, quint32 output);

    /**
     * Send feedback value to the input profile e.g. to move a motorized
     * sliders & knobs, set indicator leds etc.
     */
    bool sendFeedBack(quint32 universe, quint32 channel, uchar value, const QVariant &params);

private:
    /** In case of duplicate strings, append a number to make them unique */
    void removeDuplicates(QStringList &list);

    /**
     * Keep input patches on @p plugin's devices with their devices. A
     * device that went away (a Bluetooth pad asleep, a MIDI surface
     * unplugged) leaves its patch KEPT offline -- profile, controller
     * profile and parameters with it -- and retryOfflinePatches() puts it
     * back when the device reappears. One whose line moved (another device
     * left before it) is reopened on its new line. Matched by the line's
     * name (InputPatch::inputUID(): the device, with its serial where it
     * has one), never by the line number alone.
     */
    void followInputDevices(QLCIOPlugin *plugin);

private slots:
   /** Slot that catches plugin configuration change notifications from UIPluginCache */
    void slotPluginConfigurationChanged(QLCIOPlugin* plugin);
    void slotRetryOfflinePatches();

private:
    QTimer *m_offlineRetry;

signals:
    /** Signal emitted when a profile is changed */
    void profileChanged(quint32 universe, const QString& profileName);

    /** Notifies (InputOutputManager) of plugin configuration changes */
    void pluginConfigurationChanged(const QString& pluginName, bool success);

    /** Kept offline patches reconnected (or were forgotten). */
    void offlinePatchesChanged();

    /** The device input @p universe is patched to (@p device, a line of
     *  @p pluginName) went away; the patch is kept offline. */
    void inputDeviceDisconnected(quint32 universe, const QString &pluginName,
                                 const QString &device);
    /** A kept input patch's device came back and the patch reopened on it. */
    void inputDeviceConnected(quint32 universe, const QString &pluginName,
                              const QString &device);

    /** Everyone interested in input data should connect to this signal */
    void inputValueChanged(quint32 universe, quint32 channel, uchar value, const QString& key = 0);

    /** Emitted when an input line delivers an absolute timeline position
     *  (e.g. MIDI Time Code). Consumed by the Doc's timecode source. */
    void inputTimeCodeChanged(quint32 universe, quint32 msPosition, uchar fps);
    /** A SysEx message (MSC) arrived on an input universe. */
    void inputSysExReceived(quint32 universe, const QByteArray &sysEx);

    /*************************************************************************
     * Input profiles
     *************************************************************************/
public:
    /** Load all input profiles from the given directory using QDir filters */
    void loadProfiles(const QDir& dir);

    /** Get a list of available profile names */
    /**
     * A user-supplied name for an output target (an Art-Net node address, for
     * example). Nodes cannot always be renamed over the wire -- plenty of
     * cheap hardware acknowledges an ArtAddress write and discards it -- and a
     * bare IP tells an operator nothing at 2am. Keyed by target address rather
     * than by MAC on purpose: the address is what a patch actually points at,
     * and it is the only identity available for a node nothing has answered
     * from, which is exactly the case that most needs a label.
     */
    QString targetAlias(const QString &address) const;
    void setTargetAlias(const QString &address, const QString &name);
    QMap<QString, QString> targetAliases() const;

    /**
     * A target declared by hand rather than heard on the wire.
     *
     * Discovery only ever tells you about hardware that answers a poll, which
     * leaves two ordinary cases with no row to point at: a node on a subnet
     * this machine cannot broadcast into (reached by unicast through the
     * router, so it never replies to a broadcast poll), and a node that is
     * not plugged in yet because the rig is being patched in the shop the
     * week before. Both need to be describable in advance, and both need to
     * survive a restart, so they are part of the workspace.
     *
     * Keyed by address like the aliases, and for the same reason: the address
     * is what a patch points at and the only identity a silent node has.
     * `line` is the plugin line the target hangs under -- stored as an index
     * because that is what a patch stores too, so it is no more fragile than
     * the patch it exists to serve; a stale index falls back to line 0 rather
     * than dropping the target.
     */
    struct ManualTarget
    {
        ManualTarget() : line(0) {}
        QString plugin;
        quint32 line;
        QString address;
        /** Art-Net style port addresses: (net << 8) | (sub << 4) | universe. */
        QList<quint32> ports;
    };

    /**
     * A user-supplied name for a plugin LINE -- "FOH rack", "dimmer world" --
     * as opposed to targetAlias(), which names something on the far end.
     *
     * Keyed by plugin name plus the line's own identifier string rather than
     * its index, because the index is positional and the string is not: a NIC
     * appearing or a USB widget being unplugged shifts indices, and a label
     * that follows the shift ends up on the wrong cable.
     */
    QString lineAlias(const QString &plugin, const QString &lineName) const;
    void setLineAlias(const QString &plugin, const QString &lineName,
                      const QString &alias);

    /**
     * Universes whose INPUT comes from this plugin line.
     *
     * A universe has exactly one input patch, but nothing stops two universes
     * subscribing to the same line -- and for MIDI that is almost always a
     * mistake, because every event then fires twice, once per universe, with
     * no indication of why. Callers use this to say so before it happens.
     */
    QList<quint32> universesWithInputOn(const QString &plugin, quint32 line) const;

    /**
     * One step of undo for patch changes. Owned here because this is what owns
     * the patches; the UI reaches it through the map rather than keeping its
     * own, so a change made from any surface can be undone from any other.
     */
    class PatchUndo *patchUndo() const;

    QList<ManualTarget> manualTargets() const;
    /** True if this address was declared by hand (so it can be removed). */
    bool isManualTarget(const QString &address) const;
    /** No-op if the address is already declared. */
    void addManualTarget(const QString &plugin, quint32 line, const QString &address);
    /** Drops the target and every port on it. Patches pointing at it are
        deliberately left alone -- removing a label must not silently
        unpatch a universe. */
    void removeManualTarget(const QString &address);
    void addManualPort(const QString &address, quint32 portAddress);
    void removeManualPort(const QString &address, quint32 portAddress);

    QStringList profileNames();

    /** Get a profile by its name */
    QLCInputProfile* profile(const QString& name);

    /** Add a new profile */
    bool addProfile(QLCInputProfile* profile);

    /** Remove an existing profile by its name and delete it */
    bool removeProfile(const QString& name);

    /**
     * Get input source names for the given input universe and channel.
     *
     * @param src (IN) The input source, whose universe & channel names to get
     * @param uniName (OUT) The name of the universe, if available
     * @param chName (OUT) The name of the channel, if available
     *
     * @return true if uniName & chName contain something, otherwise false
     */
    bool inputSourceNames(const QLCInputSource *src,
                          QString& uniName, QString& chName) const;
    bool inputSourceNames(QSharedPointer<QLCInputSource> const& src,
                          QString& uniName, QString& chName) const;

    /**
     * Get the default system input profile directory that contains installed
     * input profiles. The location varies greatly between platforms.
     *
     * @return System profile directory
     */
    static QDir systemProfileDirectory();

    /**
     * Get the user's own default input profile directory that is used to save
     * custom input profiles. The location varies greatly between platforms.
     *
     * @return User profile directory
     */
    static QDir userProfileDirectory();

private:
    /** List that contains all available profiles */
    QList <QLCInputProfile*> m_profiles;

    /** Operator-supplied names for output targets, keyed by address. */
    QMap<QString, QString> m_targetAliases;
    QList<ManualTarget> m_manualTargets;
    class PatchUndo *m_patchUndo;
    /** Keyed "plugin|lineName". */
    QMap<QString, QString> m_lineAliases;

    bool m_localProfilesLoaded;

    /*********************************************************************
     * Beats
     *********************************************************************/
public:
    enum BeatGeneratorType
    {
        Disabled,   //! No one is generating beats
        Internal,   //! MasterTimer is the beat generator
        Plugin,     //! A plugin is the beat generator
        Audio       //! An audio input device is the beat generator
    };

    void setBeatGeneratorType(BeatGeneratorType type);
    BeatGeneratorType beatGeneratorType() const;

    QString beatTypeToString(BeatGeneratorType type) const;
    BeatGeneratorType stringToBeatType(QString str);

    void setBpmNumber(int bpm);
    int bpmNumber() const;

    /** Tap tempo (MasterTimer::tap()): the tempo from the last few taps and
     *  the beat on this one. Ignored while the beat generator is Disabled.
     *  Returns the tempo, fractional. */
    double tapBeat();

    /** Audio input block latency in ms (captureSize / sampleRate) when the beat
     *  generator is Audio and a capture device is live; -1 otherwise. A measured
     *  floor for the auto-tap detection-latency correction. */
    int audioInputBlockMs() const;

protected slots:
    void slotMasterTimerBeat();
    void slotPluginBeat(quint32 universe, quint32 channel, uchar value, const QString &key);
    void slotProcessBeat();

signals:
    void beatGeneratorTypeChanged();
    void bpmNumberChanged(int bpmNumber);
    void beat();

private:
    BeatGeneratorType m_beatGeneratorType;
    int m_currentBPM;
    QElapsedTimer *m_beatTime;
    AudioCapture *m_inputCapture;

    /*********************************************************************
     * Defaults
     *********************************************************************/
public:
    /**
     * Load default settings for input/output mapper from QLC+ global settings
     */
    void loadDefaults();

    /**
     * Save default settings for input/output mapper into QLC+ global settings
     */
    void saveDefaults();

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    /**
     * Load the input/output map contents from the given XML node.
     *
     * @param root An XML subtree containing the input/output map contents
     * @return true if the map was loaded successfully, otherwise false
     */
    bool loadXML(QXmlStreamReader &root);

    /**
     * Save the input/output map instance into an XML document, under the given
     * XML element (tag).
     *
     * @param doc The master XML document to save to.
     * @param wksp_root The workspace root element
     */
    bool saveXML(QXmlStreamWriter *doc) const;

};

/** @} */

#endif // INPUTOUTPUTMAP_H

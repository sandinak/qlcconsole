/*
  Q Light Controller Plus
  universe.h

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


#ifndef UNIVERSE_H
#define UNIVERSE_H

#include <QScopedPointer>
#include "qlcengine_export.h"
#include <QSemaphore>
#include <QByteArray>
#include <QThread>
#include <QSet>
#include <QMap>
#include <QVariant>
#include <climits>
#include <atomic>
#include <memory>

#include "inputpatch.h"
#include "qlcchannel.h"
#include "fixturepower.h"
#include <QMutex>
#include <QHash>
#include <QPair>

class QXmlStreamReader;
class QLCInputProfile;
class ChannelModifier;
class InputOutputMap;
class GenericFader;
class QLCIOPlugin;
class GrandMaster;
class OutputPatch;
class InputPatch;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define UNIVERSE_SIZE 512

#define KXMLQLCUniverse             QStringLiteral("Universe")
#define KXMLQLCUniverseName         QStringLiteral("Name")
#define KXMLQLCUniverseID           QStringLiteral("ID")
#define KXMLQLCUniversePassthrough  QStringLiteral("Passthrough")

#define KXMLQLCUniverseInputPatch    QStringLiteral("Input")
#define KXMLQLCUniverseOutputPatch   QStringLiteral("Output")
#define KXMLQLCUniverseFeedbackPatch QStringLiteral("Feedback")

#define KXMLQLCUniversePlugin           QStringLiteral("Plugin")
#define KXMLQLCUniverseLine             QStringLiteral("Line")
#define KXMLQLCUniverseLineUID          QStringLiteral("UID")
#define KXMLQLCUniverseProfileName      QStringLiteral("Profile")
#define KXMLQLCUniverseHIDProfileName   QStringLiteral("HIDProfile")
#define KXMLQLCUniversePluginParameters QStringLiteral("PluginParameters")
#define KXMLQLCUniverseNetwork          QStringLiteral("Network")

/** Universe class contains input/output data for one DMX universe
 */
class QLC_ENGINE_EXPORT Universe : public QThread
{
    Q_OBJECT
    Q_DISABLE_COPY(Universe)

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(quint32 id READ id CONSTANT)
    Q_PROPERTY(bool passthrough READ passthrough WRITE setPassthrough NOTIFY passthroughChanged)
    Q_PROPERTY(InputPatch *inputPatch READ inputPatch NOTIFY inputPatchChanged)
    Q_PROPERTY(int outputPatchesCount READ outputPatchesCount NOTIFY outputPatchesCountChanged)
    Q_PROPERTY(bool hasFeedback READ hasFeedback NOTIFY hasFeedbackChanged)

public:
    /** Construct a new Universe */
    Universe(quint32 id = invalid(), GrandMaster *gm = NULL, QObject *parent = 0);

    /** Destructor */
    virtual ~Universe();

    /**
     * Channel types.
     * This is a bit mask to facilitate easy AND-mode type filtering
     */
    enum ChannelType
    {
        Undefined  = 0,
        LTP        = 1 << 0,
        HTP        = 1 << 1,
        Intensity  = 1 << 2
    };

    static quint32 invalid() { return UINT_MAX; }

    /**
     * Set a friendly name for this universe
     */
    void setName(QString name);

    /**
     * Retrieve the universe friendly name
     */
    QString name() const;

    /**
     * Set the universe ID (or index)
     */
    void setID(quint32 id);

    /**
     * Retrieve the universe ID
     */
    quint32 id() const;

    /**
     * Returns the number of channels used in this universe
     */
    ushort usedChannels();

    /**
     * Returns the total number of channels in this universe
     */
    ushort totalChannels();

    /**
     * Returns if the universe has changed since the last MasterTimer tick
     */
    bool hasChanged();

    /**
     * Enable or disable the passthrough mode for this universe
     */
    void setPassthrough(bool enable);

    /**
     * Returns if the universe is in passthrough mode
     */
    bool passthrough() const;

    /**
     * Enable or disable the monitor mode for this universe
     */
    void setMonitor(bool enable);

    /**
     * Returns if the universe is in monitor mode
     */
    bool monitor() const;

    /**
     * Fork "Blind" support: when enabled, dumpOutput() skips the physical
     * output plugins while processFaders() still emits universeWritten(), so
     * the 2D monitor / preview keeps updating but the stage rig stays dark.
     * Distinct from blackout (which pushes zeros to the plugins). Not
     * persisted; toggled by the Programming tab's Blind button.
     */
    void setInhibitOutput(bool enable);
    bool inhibitOutput() const;

    uchar applyPassthrough(int channel, uchar value);

protected slots:
    /**
     * Called every time the Grand Master changed value
     */
    void slotGMValueChanged();

protected:
    /**
     * Apply Grand Master to the value.
     *
     * @param channel The channel to apply Grand Master to
     * @param value The value to write
     *
     * @return Value filtered through grand master (if applicable)
     */
    uchar applyGM(int channel, uchar value);

    uchar applyModifiers(int channel, uchar value);
    void updatePostGMValue(int channel);

signals:
    void nameChanged();
    void passthroughChanged();

protected:
    /** The universe ID */
    quint32 m_id;
    /** The universe friendly name */
    QString m_name;
    /** Reference to the Grand Master to perform values scaling */
    GrandMaster *m_grandMaster;
    /** Variable that determine if a universe is in passthrough mode */
    bool m_passthrough;
    /** Flag to monitor the universe changes */
    bool m_monitor;
    /** Fork Blind: when true, dumpOutput() skips the physical plugins. */
    bool m_inhibitOutput = false;

    /************************************************************************
     * Patches
     ************************************************************************/
public:
    /** Returns true if this universe is patched with an input, output OR feedback
     *  otherwise returns false */
    bool isPatched();

    /** Sets an input patch for this Universe, and connect to it to receive signals */
    bool setInputPatch(QLCIOPlugin *plugin, quint32 input,
                       QLCInputProfile *profile = NULL);

    /** Add/Remove/Replace an output patch on this Universe */
    bool setOutputPatch(QLCIOPlugin *plugin, quint32 output, int index = 0);

    /** Add/Replace an output patch at @p index whose saved interface
     *  identity @p pendingUID did not match any line @p plugin currently
     *  offers -- see OutputPatch::setPending(). Never deletes: unlike
     *  setOutputPatch(), there is no "clear this patch" meaning here, only
     *  "remember it, unresolved". */
    bool setOutputPatchPending(QLCIOPlugin *plugin, const QString &pendingUID, int index = 0,
                               quint32 savedLine = UINT_MAX, const QString &network = QString());

    /** Sets a feedback patch for this Universe */
    bool setFeedbackPatch(QLCIOPlugin *plugin, quint32 output);

    /** Flag that indicates if this Universe has a patched feedback line */
    bool hasFeedback() const;

    /**
     * Get the reference to the input plugin associated to this universe.
     * If not present NULL is returned.
     */
    InputPatch *inputPatch() const;

    /**
     * Get the reference to the output plugin associated to this universe.
     * If not present NULL is returned.
     */
    Q_INVOKABLE OutputPatch *outputPatch(int index) const;

    /** Return the number of output patches associated to this Universe */
    int outputPatchesCount() const;

    /**
     * Get the reference to the feedback plugin associated to this universe.
     * If not present NULL is returned.
     */
    OutputPatch *feedbackPatch() const;

    /**
     * This is the actual function that writes data to an output patch
     * A flag indicates if data has changed since previous iteration
     */
    void dumpOutput(const QByteArray& data, bool dataChanged);

    void flushInput();

protected slots:
    /** Slot called every time an input patch sends data */
    void slotInputValueChanged(quint32 universe, quint32 channel, uchar value, const QString& key = 0);

signals:
    /** Everyone interested in input data should connect to this signal */
    void inputValueChanged(quint32 universe, quint32 channel, uchar value, const QString& key = 0);

    /** Notify the listeners that the input patch has changed */
    void inputPatchChanged();

    /** Notify the listeners that one output patch has changed */
    void outputPatchChanged();

    /** Notify the listeners that the number of output patches has changed */
    void outputPatchesCountChanged();

    /** Notify the listeners that a feedback line has been patched/unpatched */
    void hasFeedbackChanged();

private:
    /** Reference to the input patch associated to this universe. */
    InputPatch *m_inputPatch;

    /** List of references to the output patches associated to this universe. */
    QList<OutputPatch*> m_outputPatchList;

    /** Reference to the feedback patch associated to this universe. */
    OutputPatch *m_fbPatch;

private:
    // Connect to inputPatch's valueChanged signal
    void connectInputPatch();
    // Disconnect from inputPatch's valueChanged signal
    void disconnectInputPatch();

    /************************************************************************
     * Channels capabilities and modifiers
     ************************************************************************/
public:
    /**
     * Define the capabilities of a channel in this universe
     *
     * @param channel The channel absolute index in the universe
     * @param group The group this channel belongs to
     * @param isHTP Flag to force HTP/LTP behaviour
     */
    void setChannelCapability(ushort channel, QLCChannel::Group group, ChannelType forcedType = Undefined);

    /** Retrieve the capability mask of the given channel index
     *
     * @param channel The channel absolute index in the universe
     */
    uchar channelCapabilities(ushort channel);

    /**
     * Set the default value of a DMX channel to be considered on reset
     *
     * @param channel The channel absolute index in the universe
     * @param value the default DMX value
     */
    void setChannelDefaultValue(ushort channel, uchar value);

    /** Assign a Channel Modifier to the given channel index
      * $modifier can be NULL if the channel has no modifier */
    void setChannelModifier(ushort channel, ChannelModifier *modifier);

    /** Return the Channel Modifier assigned to the given channel
      * or NULL if none or not valid */
    ChannelModifier *channelModifier(ushort channel);

protected:
    /** An array of each channel's capabilities. This helps to optimize HTP/LTP/Relative checks */
    QScopedPointer<QByteArray> m_channelsMask;

    /** Vector of pointer to ChannelModifier classes. If not NULL, they will modify
     *  a DMX value right before HTP/LTP check and before being assigned to preGM */
    QVector<ChannelModifier*> m_modifiers;

    /** Modified channels with the non-modified value at 0.
     *  This is used for ranged initialization operations. */
    QScopedPointer<QByteArray> m_modifiedZeroValues;

    /************************************************************************
     * Faders
     ************************************************************************/
public:
    enum FaderPriority
    {
        Tracked = -2,    /** Under everything: a cue list's tracked state put back
                            after Back or a jump (ChaserRunner::restack) */
        Background = -1, /** Below Auto: the Show timeline's base layer, so a
                            Virtual Console action at Auto overrides it per-channel */
        Auto = 0,
        Override,
        Flashing, /** Priority to override slider values and running chasers by flash scene */
        SimpleDesk
    };

    /** Request a new GenericFader used to compose the final values of
     *  this Universe. The caller is in charge of adding/removing
     *  FadeChannels and eventually dismiss a fader when no longer needed.
     *  If a fade out transition is needed, this Universe
     *  is in charge of completing it and dismissing the fader. */
    QSharedPointer<GenericFader> requestFader(FaderPriority priority = Auto);

    /** Dismiss a fader requested with requestFader, which is no longer needed */
    void dismissFader(QSharedPointer<GenericFader> fader);

    /** Request a new priority for a fader with the provided intance */
    void requestFaderPriority(QSharedPointer<GenericFader> fader, FaderPriority priority);

    /** Retrieve a modifiable list of the currently active faders */
    QList<QSharedPointer<GenericFader> > faders();

    /** Set every running fader with $functionID as parent,
     *  to the requested pause state */
    void setFaderPause(quint32 functionID, bool enable);

    /** Set a fade out time to every fader of this universe.
     *  This is used from the fadeAndStopAll functionality */
    void setFaderFadeOut(int fadeTime);

public slots:
    void tick();

protected:
    void processFaders();

    /** DMX writer thread worker method */
    void run() override;

    /*********************************************************************
     * Power limits
     *********************************************************************/
public:
    /** A fixture held to a maximum draw: its first channel in this universe
     *  and its draw model (PowerModel). */
    struct PowerLimit
    {
        quint32 fixtureId;
        int base;
        PowerModel model;
    };

    /** The fixtures held to a maximum draw in this universe (PowerLimiter). */
    void setPowerLimits(const QVector<PowerLimit> &limits);

    /** The fixtures held back as of the last output: fixture -> (amps it
     *  asks for, amps it is held to). */
    QHash<quint32, QPair<double, double> > powerHeldBack() const;

private:
    /** Hold each limited fixture in @p out to its maximum draw, scaling its
     *  emitters together so its colour stays. False when none is limited. */
    bool applyPowerLimits(QByteArray &out);

    mutable QMutex m_powerMutex;
    QVector<PowerLimit> m_powerLimits;
    QHash<quint32, QPair<double, double> > m_powerHeldBack;
    QByteArray m_lastLimited;

    /*********************************************************************
     * Auto-open shutters
     *********************************************************************/
public:
    /** One head's level: lit when every gate is above zero and, if there
     *  are any, at least one of anyOf too -- and something gave it that
     *  level lately (a resting default is not a level). */
    struct AutoOpenHead
    {
        QVector<int> gates;
        QVector<int> anyOf;
    };

    /** A shutter channel opened while any of its heads has a level and
     *  nothing sets the shutter itself (ShutterAutoOpen). */
    struct AutoOpenRule
    {
        int shutter;
        uchar open;
        QVector<AutoOpenHead> heads;
    };

    /** The shutters to auto-open in this universe; empty turns it off. Safe
     *  from any thread: applied on the universe's own, on the next frame. */
    void setAutoOpenRules(const QVector<AutoOpenRule> &rules);

    /** A one-shot writer (a slider, a programmer edit) left @p count
     *  channels from @p address where it put them: they count as set until
     *  something else writes them (GenericFader, on auto-removing them). */
    void holdChannels(int address, int count);

    /** True when @p address was written in this frame or the one before,
     *  or is held (holdChannels()). */
    bool isChannelSet(int address) const;

private:
    void markWritten(int address)
    {
        m_writtenFrame[address].store(m_frame.load(std::memory_order_relaxed),
                                      std::memory_order_relaxed);
        m_held[address].store(0, std::memory_order_relaxed);
    }
    bool writtenRecently(int address) const
    {
        return quint32(m_frame.load(std::memory_order_relaxed)
                       - m_writtenFrame[address].load(std::memory_order_relaxed)) <= 1;
    }
    /** After the faders, before the output: open the shutters whose heads
     *  have a level and that nothing sets; put back the ones that no
     *  longer should be. Works on the post-GM values, never the stored
     *  pre-GM ones, so turning it off leaves the show exactly as it was. */
    void applyAutoOpen();

    QMutex m_autoOpenMutex;
    QVector<AutoOpenRule> m_autoOpenRules;
    /** Shutters a previous rule set may have opened: put back next frame */
    QVector<int> m_autoOpenRestore;
    /** The frame each channel was last written in (m_frame) */
    std::unique_ptr<std::atomic<quint32>[]> m_writtenFrame;
    /** Channels a one-shot writer left set (holdChannels()) */
    std::unique_ptr<std::atomic<quint8>[]> m_held;
    /** Counts processFaders() frames; starts past 1 so nothing is "recent" */
    std::atomic<quint32> m_frame { 16 };

signals:
    void universeWritten(quint32 universeID, const QByteArray& universeData);

protected:
    QSemaphore m_semaphore;

    /** Indicated if the DMX writer worker thread is running */
    bool m_running;

    /** IMPORTANT: this is the list of faders that will compose
     *  the Universe values. The order is very important ! */
    QList<QSharedPointer<GenericFader>> m_faders;

    /** Mutex used to protect the access to the m_faders array */
    QRecursiveMutex m_fadersMutex;

    /************************************************************************
     * Values
     ************************************************************************/
public:
    /**
     * Reset all values to 0.
     */
    void reset();

    /**
     * Unapplies Grand Master to the given address range and resets values to 0.
     *
     * @param address Starting address
     * @param range Number of channels, starting from address, to reset
     */
    void reset(int address, int range);

    /**
     * Get the current post-Grand-Master value (used by functions and everyone
     * else INSIDE QLC+) at specified address.
     *
     * @return The current value at address
     */
    uchar postGMValue(int address) const;

    /**
     * Get the current post-Grand-Master values (to be written to output HW)
     * Don't write to the returned array to prevent copying. Not that it would
     * do anything to UniverseArray's internal values, but it would be just
     * pointless waste of CPU time.
     *
     * @return The current values
     */
    const QByteArray *postGMValues() const;

    /**
     * Get the current pre-Grand-Master values (used by functions and everyone
     * else INSIDE QLC). Don't write to the returned array to prevent copying.
     * Not that it would do anything to UniverseArray's internal values, but it
     * would be just pointless waste of CPU time.
     *
     * @return The current values
     */
    const QByteArray preGMValues() const;

    /**
     * Get the current pre-Grand-Master value (used by functions and everyone
     * else INSIDE QLC+) at specified address.
     *
     * @return The current value at address
     */
    uchar preGMValue(int address) const;

    /** Set all intensity channel values to zero */
    void zeroIntensityChannels();

    /** Return a list with intensity channels and their values */
    QHash <int, uchar> intensityChannels();

protected:
    void applyPassthroughValues(int address, int range);

protected:
    /**
     * Number of channels used in this universe to optimize the dump to plugins.
     * This is a dynamic counter that can only increase depending on the
     * channels used in this universe starting from when a workspace
     * is loaded
     */
    ushort m_usedChannels;
    /**
     * Total number of channels used in this Universe.
     * This is set only when a Universe is instructed about Fixture
     * channel capabilities. Basically just set once if loading an
     * existing workspace, or several times when adding/removing
     * Fixtures
     */
    ushort m_totalChannels;
    /**
     *  Flag that holds if the total number of channels have changed.
     *  This is used to inform the output patch (if present) how many
     *  channels to expect
     */
    bool m_totalChannelsChanged;
    /** A list of intensity channels to optimize operations on HTP/LTP channels */
    QVector<int> m_intensityChannels;
    /** A flag set to know when m_intensityChannelsRanges must be updated */
    bool m_intensityChannelsChanged;
    /**
     * Intensity channels sorted as ranges, to further optimize ranged operations
     * (ie set all to zero)
     */
    QVector<int> m_intensityChannelsRanges;
    /** A list of non-intensity channels to optimize operations on HTP/LTP channels */
    QVector<int> m_nonIntensityChannels;
    /** Array of values BEFORE the Grand Master changes */
    QScopedPointer<QByteArray> m_preGMValues;
    /** Channels an Override fader wrote this frame (writeOverride) */
    QByteArray m_overrideWritten;
    /** Array of values AFTER the Grand Master changes (applyGM) */
    QScopedPointer<QByteArray> m_postGMValues;
    /** Array of the last preGM values written before the zeroIntensityChannels call  */
    QScopedPointer<QByteArray> m_lastPostGMValues;
    /** Array of non-intensity only values */
    QScopedPointer<QByteArray> m_blackoutValues;

    /** Array of values from input line, when passtrhough is enabled */
    QScopedPointer<QByteArray> m_passthroughValues;

    /* impl speedup */
    void updateIntensityChannelsRanges();

    /************************************************************************
     * Blend mode
     ************************************************************************/
public:
    enum BlendMode {
        NormalBlend = 0,
        MaskBlend,
        AdditiveBlend,
        SubtractiveBlend
    };

    /** Return a blend mode from a string */
    static BlendMode stringToBlendMode(QString mode);

    /** Return a string from a blend mode, to be saved into a XML */
    static QString blendModeToString(BlendMode mode);

    /************************************************************************
     * Writing
     ************************************************************************/
public:
    /**
     * Write a value to a DMX channel, taking Grand Master and HTP into
     * account, if applicable.
     *
     * @param address The DMX start address to write to
     * @param value The value to write
     * @param forceLTP Force to skip the HTP check
     *
     * @return true if successful, otherwise false
     */
    bool write(int address, uchar value, bool forceLTP = false);

    /**
     * Write a channel from an Override fader (an edit preview over whatever
     * runs): it replaces what lower-priority faders wrote, LTP -- but on an
     * HTP channel two Override faders in the same frame take the higher, as
     * the same functions would played at Auto (the Show Tree's Design
     * preview of a cue that mixes plays what GO plays).
     */
    bool writeOverride(int address, uchar value);

    /**
     * Write a value representing one or multiple channels
     *
     * @param address The DMX start address to write to
     * @param value the DMX value(s) to set
     * @param channelCount number of channels that value represents
     *
     * @return always true
     */
    bool writeMultiple(int address, quint32 value, int channelCount);

    /**
     * Write a relative value to a DMX channel, taking Grand Master and HTP into
     * account, if applicable.
     *
     * @param address The DMX start address to write to
     * @param value The value to write
     * @param channelCount number of channels that value represents
     *
     * @return true if successful, otherwise false
     */
    bool writeRelative(int address, quint32 value, int channelCount);

    /**
     * Write DMX values with the given blend mode.
     * If blend == NormalBlend the generic write method is called
     * and all the HTP/LTP checks are performed
     *
     * @param address The DMX start address to write to
     * @param value The value to write
     * @param channelCount The number of channels that value represents
     * @param blend The blend mode to be used on $value
     *
     * @return true if successful, otherwise false
     */
    bool writeBlended(int address, quint32 value, int channelCount, BlendMode blend);

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:

    enum PatchTagType { InputPatchTag, OutputPatchTag, FeedbackPatchTag };

    /**
     * Load a universe contents from the given XML node.
     *
     * @param root An XML subtree containing the universe contents
     * @param index The QLC+ Universe index
     * @param ioMap Reference to the QLC+ Input/Output map class
     * @return true if the Universe was loaded successfully, otherwise false
     */
    bool loadXML(QXmlStreamReader &root, int index, InputOutputMap *ioMap);

    /**
     * One patch exactly as a workspace described it: plugin, line UID and
     * index, the network it was built on, profile names and every plugin
     * parameter. Used to hand a just-parsed patch to InputOutputMap, and to
     * KEEP one that cannot be opened on this machine -- an input or
     * feedback patch whose interface/device is not present, or any patch
     * whose plugin is not installed here. Kept patches are written back by
     * saveXML() unchanged and retried by InputOutputMap until they resolve.
     * (Output patches whose plugin IS present use OutputPatch::isPending()
     * instead, which the Connections view already shows.)
     */
    struct OfflinePatch
    {
        PatchTagType tag = OutputPatchTag;
        QString plugin;
        QString uid;
        quint32 line = UINT_MAX;
        QString profile;
        QString hidProfile;
        QString network;
        QMap<QString, QVariant> params;
    };

    QList<OfflinePatch> offlinePatches() const;
    void addOfflinePatch(const OfflinePatch &patch);
    void removeOfflinePatch(int index);
    /** Drop every kept patch of @p tag (the user re-patched/unpatched it). */
    void clearOfflinePatches(PatchTagType tag);

private:
    QList<OfflinePatch> m_offlinePatches;

public:

    /**
     * Load an optional tag defining the plugin specific parameters
     * @param root An XML subtree containing the plugin parameters contents
     * @param currentTag The type of Patch where the parameters should be set
     * @param patchIndex Index of the patch to configure (ATM used only for output)
     * @return true if the parameters were loaded successfully, otherwise false
     */
    bool loadXMLPluginParameters(QXmlStreamReader &root, PatchTagType currentTag, int patchIndex);

    /**
     * Save the universe instance into an XML document, under the given
     * XML element (tag).
     *
     * @param doc The master XML document to save to.
     * @param wksp_root The workspace root element
     */
    bool saveXML(QXmlStreamWriter *doc) const;

    /**
     * Save one patch (input/output/feedback)
     *
     * @param doc
     * @param tag
     * @param pluginName
     * @param line
     * @param profileName
     * @param parameters
     */
    void savePatchXML(QXmlStreamWriter *doc,
        QString const & tag,
        QString const & pluginName, const QString &lineName,
        quint32 line,
        QString profileName,
        QMap<QString, QVariant>parameters,
        const QString &hidProfileName = QString(),
        const QString &network = QString()) const;

    /**
     * Save a plugin custom parameters (if available) into a tag nested
     * to the related Input/Output patch
     *
     * @param doc The master XML document to save to.
     * @param wksp_root The workspace root element
     * @param parameters The map of custom parameters to save
     */
    bool savePluginParametersXML(QXmlStreamWriter *doc,
                                 QMap<QString, QVariant>parameters) const;
};

/** @} */

#endif

/*
  Q Light Controller Plus
  doc.h

  Copyright (c) Heikki Junnila
                Massimo Callegari

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

#ifndef DOC_H
#define DOC_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QFile>
#include <QMap>
#include <QSet>
#include <QColor>

#include "qlcfixturedefcache.h"
#include "qlcmodifierscache.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "channelsgroup.h"
#include "fixturegroup.h"
#include "qlcclipboard.h"
#include "mastertimer.h"
#include "qlcpalette.h"
#include "scenevalue.h"
#include "function.h"
#include "fixture.h"

class AudioCapture;
class RGBScriptsCache;
class AudioPluginCache;
class MonitorProperties;
class CaptureManager;
class ProgrammerFlasher;
class ProgrammerController;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCEngine QStringLiteral("Engine")
#define KXMLQLCStartupFunction QStringLiteral("Autostart")

class Doc final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Doc)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    /**
     * Create a new Doc instance for the given parent.
     *
     * @param parent The parent object who owns the Doc instance
     * @param outputUniverses Number of output (DMX) universes
     * @param inputUniverses Number of input universes
     */
    Doc(QObject* parent, int universes = 4);

    /** Destructor */
    ~Doc();

    /** Remove all functions and fixtures from the doc, signalling each removal. */
    void clearContents();

    /**
     * Set the current workspace absolute path. To be used when local files need
     * to be loaded even if the workspace file has been moved
     */
    void setWorkspacePath(QString path);

    /** Retrieve the current workspace absolute path */
    QString workspacePath() const;

    /** If filePath is in the workspace directory or in one of its subdirectories,
     *  return path relative to the workspace directory.
     *  Otherwise return absolute path.
     *
     *  Purpose: saving components of the workspace file (audio, icons,...)
     */
    QString normalizeComponentPath(const QString& filePath) const;

    /** If filePath is relative path, it is resolved relative to the workspace
     *  directory (absolute path is returned).
     *  If filePath is absolute, it is returned unchanged (symlinks and .. are resolved).
     *
     *  Purpose: saving components of the workspace file (audio, icons,...)
     */
    QString denormalizeComponentPath(const QString& filePath) const;

private:
    QString m_workspacePath;

signals:
    /** Emitted when clearContents() is called, before actually doing anything. */
    void clearing();

    /** Emitted when clearContents() has finished. */
    void cleared();

    /** Emitted when the document is being loaded, before actually doing anything. */
    void loading();

    /** Emitted when the document has been completely loaded. */
    void loaded();

    /*********************************************************************
     * Engine components
     *********************************************************************/
public:
    /** Get the fixture definition cache object */
    QLCFixtureDefCache *fixtureDefCache() const;

    /** Set the fixure definition cache reference. This is useful
     *  to share a cache between different Docs.
     *  Note: deletion of an existing cache must be performed before calling
     *        this method, otherwise it creates a memory leak */
    void setFixtureDefinitionCache(QLCFixtureDefCache *cache);

    /** Get the channel modifiers cache object */
    QLCModifiersCache *modifiersCache() const;

    /** Get the RGB scripts cache object */
    RGBScriptsCache *rgbScriptsCache() const;

    /** Get the I/O plugin cache object */
    IOPluginCache *ioPluginCache() const;

    /** Get the audio decoder plugin cache object */
    AudioPluginCache *audioPluginCache() const;

    /** Get the DMX output map object */
    InputOutputMap *inputOutputMap() const;

    /** Get the MasterTimer object that runs the show */
    MasterTimer *masterTimer() const;

    /** Get the CaptureManager that records DMX overrides for live-edit */
    CaptureManager *captureManager() const;

    /** Get the audio input capture object */
    QSharedPointer<AudioCapture> audioInputCapture() const;

    /** Destroy a previously created audio capture instance */
    void destroyAudioCapture();

private:
    QLCFixtureDefCache *m_fixtureDefCache;
    QLCModifiersCache *m_modifiersCache;
    RGBScriptsCache *m_rgbScriptsCache;
    IOPluginCache *m_ioPluginCache;
    AudioPluginCache *m_audioPluginCache;
    MasterTimer *m_masterTimer;
    CaptureManager *m_captureManager;
    InputOutputMap *m_ioMap;
    mutable QSharedPointer<AudioCapture> m_inputCapture;
    MonitorProperties *m_monitorProps;

    /*********************************************************************
     * Main operating mode
     *********************************************************************/
public:
    enum Mode
    {
        Design  = 0, //! Editing allowed
        Operate = 1  //! Running allowed, editing disabled
    };

    /** Change the main operating mode. See enum Mode for more information. */
    void setMode(Mode mode);

    /** Get the main operating mode. See enum Mode for more information. */
    Mode mode() const;

    /**
     * Enable/disable kiosk mode. This doesn't do practically anything by itself.
     * It's mostly a convenient helper for engine components to detect if kiosk
     * mode is on.
     */
    void setKiosk(bool kiosk);

    /** Check, if kiosk mode is enabled or not. */
    bool isKiosk() const;

signals:
    /** Tells that the current operating mode has changed */
    void modeChanged(Doc::Mode mode);

protected:
    Mode m_mode;
    bool m_kiosk;

    /*********************************************************************
     * Modified status
     *********************************************************************/
public:
    enum LoadStatus
    {
        Cleared = 0,
        Loading,
        Loaded
    };

    /** Get the current Doc load status */
    LoadStatus loadStatus() const;

    /** Check, whether Doc has been modified (and is in need of saving) */
    bool isModified() const;

    /** Set Doc into modified state (i.e. it is in need of saving) */
    void setModified();

    /** Reset Doc's modified state (i.e. it is no longer in need of saving) */
    void resetModified();

signals:
    /** Signal that this Doc has been modified (or unmodified) */
    void modified(bool state);
    void needAutosave();

protected:
    /** The current Doc load status */
    LoadStatus m_loadStatus;
    bool m_modified;
    QTimer m_autosaveTimer;

    /*********************************************************************
     * Clipboard
     *********************************************************************/
public:
    /** Get a reference to QLC+ global clipboard*/
    QLCClipboard *clipboard();

private:
    QLCClipboard *m_clipboard;

    /*********************************************************************
     * Programmer selection
     *
     * A global, ordered set of fixture ids that "selected for editing"
     * VC widgets target. Set by SelectFixtures-mode VCButtons; read by
     * Parameter-mode VCSliders and similar widgets that resolve their
     * write target against the current selection rather than a static
     * channel list.
     *
     * The selection is intentionally engine-level so any layer (VC,
     * QML UI, web access, scripts) can participate.
     *********************************************************************/
public:
    /** Current programmer selection in selection-order. */
    QList<quint32> programmerSelection() const;

    /** Replace the selection wholesale. */
    void setProgrammerSelection(const QList<quint32>& fixtureIds);

    /** Append fixtures not already selected. Order is preserved. */
    void addToProgrammerSelection(const QList<quint32>& fixtureIds);

    /** Remove the given fixtures from the selection (no-op for absent). */
    void removeFromProgrammerSelection(const QList<quint32>& fixtureIds);

    /** Toggle each given fixture's membership. */
    void toggleInProgrammerSelection(const QList<quint32>& fixtureIds);

    /** Clear the selection. */
    void clearProgrammerSelection();

    /** Constant-time membership check. */
    bool isInProgrammerSelection(quint32 fixtureId) const;

    /** True if every given id is currently selected. Used by
        SelectFixtures-mode buttons to decide their checked state. */
    bool allInProgrammerSelection(const QList<quint32>& fixtureIds) const;

    /**
     * Programmer-side composite color tracked across R/G/B/W slider
     * moves. Lets fixtures with a Color (wheel) channel but no RGB
     * channels still follow what the RGB sliders are asking for —
     * VCSlider::writeDMXParameter does the nearest-preset lookup.
     */
    QColor programmerColor() const;
    /** Update one component (Red/Green/Blue/White) of the programmer
        color. White is stored in the alpha channel for compactness. */
    void setProgrammerColorComponent(int qlcPrimaryColour, uchar value);

    /*********************************************************************
     * Programmer Values (asserted-by-the-programmer fixture-channel map)
     *
     * Every Parameter-mode write records its (fixture, channel, value)
     * here. Save Programmer reads from this to build a Scene; Clear
     * empties it. The map IS the dirty state — empty == clean.
     *********************************************************************/
public:
    /** Record one programmer-asserted channel value. Triggers
        programmerDirtyChanged(true) on the first non-empty write. */
    void setProgrammerValue(quint32 fixtureId, quint32 channel, uchar value);

    /** Drop all programmer values. Triggers programmerDirtyChanged(false). */
    void clearProgrammerValues();

    /** True iff at least one programmer value is currently held. */
    bool isProgrammerDirty() const;

    /** Snapshot the current programmer values into a brand-new Scene
        with the given name. Returns the new function id, or
        Function::invalidId() on failure. Caller should clear values
        afterward (this method does NOT clear, so callers can sequence
        the dialog/cancel/etc. however they want). */
    quint32 saveProgrammerAsScene(const QString &name);

    /*********************************************************************
     * Smart-save bucketing: classify non-routed programmer values into
     * (fixture group, role category) buckets so Save can land them in
     * sensibly-named scenes filed under category folders.
     *********************************************************************/
    enum SaveCategory {
        SaveCatPosition = 0,   // Pan, Tilt (and their fine bytes)
        SaveCatColor,          // R/G/B/W/Amber/UV/Cyan/Magenta/Yellow + Colour wheel
        SaveCatSpecial,        // Gobo, Shutter, Beam, Effect, Speed, Prism, Maintenance
        SaveCatIntensity,      // Dimmer / Intensity (no specific colour)
        SaveCatUnknown         // anything that doesn't fit above
    };
    Q_ENUM(SaveCategory)

    /** Container for one Save proposal — a single new Scene to be
        created from a (fixture-group, category) slice of the
        programmer values. Names + path are pre-filled with sensible
        defaults; the Save dialog lets the user edit them. */
    struct SaveBucket
    {
        SaveCategory category = SaveCatUnknown;
        quint32 fixtureGroupId = (quint32)~0;       // invalidId if no group fits
        QString groupName;                           // for default naming/path
        QString defaultName;
        QString defaultPath;
        QHash<quint32, QHash<quint32, uchar>> values;
    };

    /** Group the current programmer values by category and return a
        Save proposal per bucket. Read-only — does not modify Doc state.
        Default grouping is "most inclusive": per category, all the
        edited fixtures collapse into the single largest fixture group
        that contains every one of them. If no single group covers them
        all, they fall back to one bucket per smallest-containing-group.
        The UI can split an inclusive bucket via splitBucketByGroup(). */
    QList<SaveBucket> proposedSaveBuckets() const;

    /** Split one (typically inclusive) bucket into sub-buckets, one per
        smallest fixture group containing each of the bucket's fixtures.
        Lets the Save dialog offer "save per contributing group" instead
        of a single inclusive scene. Names/paths are regenerated. */
    QList<SaveBucket> splitBucketByGroup(const SaveBucket &bucket) const;

    /** Persist a SaveBucket as a new Scene with the given (possibly
        user-edited) name and path. Returns the new function id, or
        invalidId on failure. */
    quint32 saveBucketAsScene(const SaveBucket &bucket,
                              const QString &name,
                              const QString &path);

    /** Persist a SaveBucket as a *dynamic group Scene*: builds a
        QLCPalette from the bucket (type derived from its category) and
        a Scene that references the palette + the bucket's fixture group,
        so the look follows group membership at run time. Channels the
        palette can't drive (e.g. a colour-wheel fixture under a Color
        palette) are baked per-fixture into the same Scene as a fallback.
        Requires bucket.fixtureGroupId to be a real group. Returns the
        new function id, or invalidId on failure (caller may then fall
        back to the static saveBucketAsScene). */
    quint32 saveBucketAsGroupScene(const SaveBucket &bucket,
                                   const QString &name,
                                   const QString &path);

    /**
     * Generate a unique name for a copy of @p src by bumping the
     * rightmost numeric segment of its current name (preserving
     * zero-padding) and checking for collisions against other
     * functions in the same Path. Falls back to appending " N" if no
     * numeric pattern is detected. Pattern-agnostic — works for
     * "Verse 1" → "Verse 2", "01-01.02-Reville" → "01-01.03-Reville",
     * and arbitrary names with embedded numbers.
     */
    QString nextDuplicateName(const Function *src) const;

    /**
     * Step the most-recently-started running Chaser forward/backward.
     * Lets a single pair of surface buttons (SHIFT/BANK on the Mk2)
     * scrub through whatever chaser is in focus without binding each
     * chaser explicitly. No-op if no chaser is running.
     */
    void stepCurrentChaser(int direction);  // -1 = back, +1 = forward

    /**
     * Find an existing Scene whose value-set EXACTLY matches @p values
     * (same fixtures, same channels, same DMX values). @p excludeSceneId
     * is skipped — useful when looking for a duplicate of a scene
     * we're considering swapping.
     * Returns the scene id, or Function::invalidId() if no match.
     */
    quint32 findMatchingScene(
        const QHash<quint32, QHash<quint32, uchar>> &values,
        quint32 excludeSceneId = (quint32)~0) const;

    /** Collections that reference @p sceneId as a child. */
    QList<quint32> collectionsContaining(quint32 sceneId) const;

    /** Swap @p oldSceneId for @p newSceneId in @p collectionId at the
        same insertion index. Returns true on success. */
    bool replaceSceneInCollection(quint32 collectionId,
                                  quint32 oldSceneId,
                                  quint32 newSceneId);

    /** Restore @p sceneId to the snapshot taken when it first became
        edited. No-op if the scene wasn't edited. */
    void revertSceneFromSnapshot(quint32 sceneId);

    /** Currently-running Collection if there's exactly one — else
        Function::invalidId(). Used by Save's "Use existing + add to
        running collection" sugar. */
    quint32 singleRunningCollection() const;

    /** Briefly flash @p fixtureId so the user can spot which physical
        fixture they just toggled in pad-grid sub-selection. Inverts
        the current intensity (off→on if dim, on→off if bright) for
        @p durationMs, then releases. */
    void flashFixture(quint32 fixtureId, int durationMs = 180);

    /**
     * Show-mode lock — when true, programmer parameter writes (slider
     * Parameter mode, GrandMaster, route + non-route) are silently
     * dropped. Selection nav, Save, Revert and pad-mode toggles
     * still work. Toggled by App's Tools → Show Mode Lock action.
     */
    bool isShowLocked() const;
    void setShowLocked(bool locked);

signals:
    void showLockedChanged(bool locked);

public:

    /**
     * Route a programmer write to whichever running Scene "owns" this
     * (fixture, channel) — the most-recently-started running Scene
     * that has a value set on the pair (LTP). Updates the scene's
     * value in place and marks the scene as edited. Returns the scene
     * id on success, or Function::invalidId() if no running scene
     * owns the channel — in that case the caller should fall back to
     * setProgrammerValue() so the write becomes part of a new scene
     * on next Save.
     */
    quint32 routeProgrammerEdit(quint32 fid, quint32 ch, uchar value);

    /** Re-attempt routing for every raw programmer value against the
        currently-running scenes/chasers/collections. Values captured
        while nothing owned them (e.g. the user edited a look, then
        started the chase that plays it) fold into the now-running
        scene and drop out of the raw value map. Call this just before
        building the Save dialog so "edit a look then start its chase"
        offers Update rather than only Create-new. Returns the number
        of values that got routed. */
    int rerouteProgrammerValues();

    /** Read-only view of scenes with unsaved programmer edits. */
    QSet<quint32> editedSceneIds() const;

    /*********************************************************************
     * Pad-grid mode + per-fixture sub-selection (5×8 matrix on Mk2)
     *********************************************************************/
    /** Current pad-grid mode. Drives what the 40 clip pads do and
        what their LEDs reflect. Modes:
         - PadModeOff:           pads inert + dark
         - PadModeFixtureSelect: pads toggle fixtures within the
                                  active programmer group
         - PadModeGoboSelect:    (next round) pads = gobo capability
                                  ranges
         - PadModeColorPalette:  (next round) pads = preset colors */
    enum PadMode {
        PadModeOff = 0,
        PadModeFixtureSelect,
        PadModeGoboSelect,
        PadModeColorPalette
    };
    Q_ENUM(PadMode)

    PadMode padMode() const;
    void setPadMode(PadMode mode);

    /** The fixture group whose fixtures EXACTLY match the current
        programmer selection — i.e., the unambiguous "active group"
        the pad grid maps fixtures into. Returns invalidId when the
        selection doesn't pin down a single group. */
    quint32 activeProgrammerGroup() const;

    /** Per-fixture refinement within the active group. Empty means
        "all fixtures in the active group are in scope" (default).
        Non-empty means "only these fixtures get programmer writes". */
    QSet<quint32> programmerSubSelection() const;
    bool isInProgrammerSubSelection(quint32 fid) const;
    void toggleInProgrammerSubSelection(quint32 fid);
    void clearProgrammerSubSelection();

    /** True iff the programmer values map has any entries — i.e.,
        Save would create a brand-new Scene from non-routed channels. */
    bool hasProgrammerValues() const;

    /** Read-only view of the non-routed programmer values, keyed by
        fixture id then channel index. The Save dialog uses this to
        show the user what's about to land in the new scene. */
    QHash<quint32, QHash<quint32, uchar>> programmerValues() const;

    /** Discard all in-flight programmer edits: restore each edited
        scene to the snapshot taken before its first edit, drop the
        programmer values map, fire programmerDirtyChanged(false). */
    void revertProgrammer();

signals:
    /** Emitted whenever the programmer selection changes. Listeners
        should re-read programmerSelection() — no payload, since the
        change can be arbitrary (replace/add/remove/toggle/clear). */
    void programmerSelectionChanged();

    /** Emitted whenever the programmer-values map transitions between
        empty and non-empty. Argument: new dirty state. */
    void programmerDirtyChanged(bool dirty);

    /** Emitted when padMode() changes. */
    void padModeChanged(PadMode mode);

    /** Emitted whenever programmerSubSelection() changes. */
    void programmerSubSelectionChanged();

public:
    /** Fork-owned controller holding all programmer-mode state and
        logic. Doc's programmer methods above are thin forwarders to it.
        QObject child of Doc (auto-deleted). */
    ProgrammerController *programmer() const { return m_programmer; }

private:
    ProgrammerController *m_programmer = nullptr;

    /*********************************************************************
     * Fixture Instances
     *********************************************************************/
public:
    /**
     * Add the given fixture to doc's fixture array.
     * If id == Fixture::invalidId(), doc assigns the fixture a new ID and
     * takes ownership, unless there is no more room for more fixtures.
     *
     * If id != Fixture::invalidId(), doc attempts to put the fixture at
     * that exact index, unless another fixture already occupies it.
     *
     * @param fixture The fixture to add
     * @param id The requested ID for the fixture
     * @return true if the fixture was successfully added to doc,
     *         otherwise false.
     */
    bool addFixture(Fixture* fixture, quint32 id = Fixture::invalidId(), bool crossUniverse = false);

    /**
     * Delete the given fixture instance from Doc
     *
     * @param id The ID of the fixture instance to delete
     */
    bool deleteFixture(quint32 id);

    /**
     * Replace the whole fixtures list with a new one.
     * This is done by remapping. Note that no signal is emitted to
     * avoid losing scenes and all the stuff connected to fixtures.
     * The caller must be aware on this and reassign all the QLC+ project
     * data previously created.
     *
     * @param newFixturesList list of fixtures that will take place
     */
    bool replaceFixtures(QList<Fixture*> newFixturesList);

    /**
     * Update the channels capabilities of an existing fixture with the given ID
     * @param id The ID of the fixture instance
     * @param forcedHTP A list of channel indices forced to act as HTP
     * @param forcedLTP A list of channel indices forced to act as LTP
     */
    bool updateFixtureChannelCapabilities(quint32 id, QList<int>forcedHTP, QList<int>forcedLTP);

    /**
     * Get the fixture instance that has the given ID
     *
     * @param id The ID of the fixture to get
     */
    Fixture* fixture(quint32 id) const;

    /**
     * Get a list of fixtures ordered by ID
     */
    QList<Fixture*> const& fixtures() const;

    /**
     * Get the number of fixtures currently added to the project
     *
     * @return The number of fixtures
     */
    int fixturesCount() const;

    /**
     * Get the fixture that occupies the given DMX address. If multiple fixtures
     * occupy the same address, the one that has been last modified is returned.
     *
     * @param universeAddress The universe & address of the fixture to look for
     * @return The fixture ID or Fixture::invalidId() if not found
     */
    quint32 fixtureForAddress(quint32 universeAddress) const;

    /**
     * Get the total power consumption of all fixtures in the current
     * workspace.
     *
     * @param[out] fuzzy Number of fixtures without power consumption value
     * @return Total power consumption
     */
    int totalPowerConsumption(int& fuzzy) const;

protected:
    /**
     * Create a new fixture ID
     */
    quint32 createFixtureId();

signals:
    /** Signal that a fixture has been added */
    void fixtureAdded(quint32 fxi_id);

    /** Signal that a fixture has been removed */
    void fixtureRemoved(quint32 fxi_id);

    /** Signal that a fixture's properties have changed */
    void fixtureChanged(quint32 fxi_id);

private slots:
    /** Catch fixture property changes */
    void slotFixtureChanged(quint32 fxi_id);

protected:
    /** Fixtures hash: < ID, Fixture instance > */
    QHash <quint32, Fixture*> m_fixtures;

    /** Fixtures list cache */
    bool m_fixturesListCacheUpToDate;
    QList<Fixture*> m_fixturesListCache;

    /** Map of the addresses occupied by fixtures */
    QHash <quint32, quint32> m_addresses;

    /** Latest assigned fixture ID */
    quint32 m_latestFixtureId;

    /*********************************************************************
     * Fixture groups
     *********************************************************************/
public:
    /** Add a new fixture group. Doc takes ownership of the group. */
    bool addFixtureGroup(FixtureGroup* grp, quint32 id = FixtureGroup::invalidId());

    /**
     * Remove and delete a fixture group. Doesn't destroy group's fixtures.
     * The group pointer is invalid after this call.
     */
    bool deleteFixtureGroup(quint32 id);

    /** Get a fixture group by id */
    FixtureGroup* fixtureGroup(quint32 id) const;

    /** Get a list of Doc's fixture groups */
    QList <FixtureGroup*> fixtureGroups() const;

signals:
    void fixtureGroupAdded(quint32 id);
    void fixtureGroupRemoved(quint32 id);
    void fixtureGroupChanged(quint32 id);

private slots:
    /** Catch fixture group property changes */
    void slotFixtureGroupChanged(quint32 id);

private:
    /** Create a new fixture group ID */
    quint32 createFixtureGroupId();

private:
    /** Fixture Groups */
    QMap <quint32,FixtureGroup*> m_fixtureGroups;

    /** Latest assigned fixture group ID */
    quint32 m_latestFixtureGroupId;

    /*********************************************************************
     * Channel groups
     *********************************************************************/
public:
    /** Add a new channels group. Doc takes ownership of the group. */
    bool addChannelsGroup(ChannelsGroup *grp, quint32 id = ChannelsGroup::invalidId());

    /**
     * Remove and delete a channels group.
     * The group pointer is invalid after this call.
     */
    bool deleteChannelsGroup(quint32 id);

    /** Move a channel group of a given direction amount */
    bool moveChannelGroup(quint32 id, int direction);

    /** Get a channels group by id */
    ChannelsGroup* channelsGroup(quint32 id) const;

    /** Get a list of Doc's channels groups */
    QList <ChannelsGroup*> channelsGroups() const;

private:
    /** Create a new channels group ID */
    quint32 createChannelsGroupId();

signals:
    /** Signal that a channels group has been added */
    void channelsGroupAdded(quint32 chgrp_id);

    /** Signal that a channels group has been removed */
    void channelsGroupRemoved(quint32 chgrp_id);

private:
    /** Channel Groups */
    QMap <quint32,ChannelsGroup*> m_channelsGroups;

    /** Hold the Channel Groups IDs ordered as displayed
     *  in the Fixture Manager panel */
    QList <quint32> m_orderedGroups;

    /** Latest assigned channel group ID */
    quint32 m_latestChannelsGroupId;

    /*********************************************************************
     * Palettes
     *********************************************************************/
public:
    /** Add a new palette. Doc takes ownership of it */
    bool addPalette(QLCPalette *palette, quint32 id = QLCPalette::invalidId());

    /**
     * Remove and delete a palette.
     * The Palette pointer is invalid after this call.
     */
    bool deletePalette(quint32 id);

    /** Get a palette by id */
    QLCPalette *palette(quint32 id) const;

    /** Get a list of Doc's palettes */
    QList <QLCPalette*> palettes() const;

private:
    /** Create a new palette ID */
    quint32 createPaletteId();

signals:
    /** Inform the listeners that a new palette has been added */
    void paletteAdded(quint32 id);

    /** Inform the listeners that a new palette has been removed */
    void paletteRemoved(quint32 id);

private:
    /** Palettes */
    QMap <quint32,QLCPalette*> m_palettes;

    /** Latest assigned palette ID */
    quint32 m_latestPaletteId;

    /*********************************************************************
     * Functions
     *********************************************************************/
public:
    /**
     * Add the given function to doc's function array.
     * If id == Function::invalidId(), doc assigns the function a new ID
     * and takes ownership, unless there is no more room for more functions.
     *
     * If id != Function::invalidId(), doc attempts to put the function at
     * that exact index, unless another function already occupies it.
     *
     * @param function The function to add
     * @param id The requested ID for the function
     * @return true if the function was successfully added to doc, otherwise false.
     */
    bool addFunction(Function* function, quint32 id = Function::invalidId());

    /**
     * Get a list of currently available functions
     *
     * @return List of functions
     */
    QList <Function*> functions() const;

    /**
     * Get a list of currently available functions by type
     *
     * @return List of functions by type
     */
    QList <Function*> functionsByType(Function::Type type) const;

    /**
     * Get a pointer to a Function with the given name
     * @param name lookup Function name
     * @return pointer to Function or null if not found
     */
    Function *functionByName(QString name);

    /**
     * Delete the given function
     *
     * @param id The ID of the function to delete
     * @return true if the function was found and deleted
     */
    bool deleteFunction(quint32 id);

    /**
     * Get a function that has the given ID
     *
     * @param id The ID of the function to get
     * @return A function at the given ID or NULL if not found
     */
    Function* function(quint32 id) const;

    /**
     * Get the next Function ID that will be assigned at the
     * creation of a new Function
     */
    quint32 nextFunctionID();

    /**
     * Set the ID of a function to start every time QLC+ goes
     * in operate mode
     *
     * @param fid The ID of the function
     */
    void setStartupFunction(quint32 fid);

    /**
     * Retrieve the QLC+ startup function
     */
    quint32 startupFunction();

    /**
     * Find the usage of a Function with the specified $fid
     * within Doc
     *
     * @param fid the Function ID to look up for
     * @return a list of Function IDs and, if available, the step position
     */
    QList<quint32> getUsage(quint32 fid);

protected:
    /**
     * Create a new function Id
     */
    quint32 createFunctionId();

    /**
     * Assign the given function ID to the function, place the function
     * at the same index in m_functionArray, increase function count and
     * emit functionAdded() signal.
     *
     * @param function The function to assign
     * @param id The ID to assign to the function
     */
    void assignFunction(Function* function, quint32 id);

private slots:
    /** Slot that catches function change signals */
    void slotFunctionChanged(quint32 fid);

    /** Slot that catches function name change signals */
    void slotFunctionNameChanged(quint32 fid);

signals:
    /** Signal that a function has been added */
    void functionAdded(quint32 function);

    /** Signal that a function has been removed */
    void functionRemoved(quint32 function);

    /** Signal that a function has been changed */
    void functionChanged(quint32 function);

    /** Signal that a function has been changed */
    void functionNameChanged(quint32 function);

protected:
    /** Functions */
    QMap <quint32,Function*> m_functions;

    /** Latest assigned function ID */
    quint32 m_latestFunctionId;

    /** Startup function ID */
    quint32 m_startupFunctionId;

    /*********************************************************************
     * Monitor Properties
     *********************************************************************/
public:
    /** Returns a reference to the monitor properties instance */
    MonitorProperties *monitorProperties();

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    /**
     * Load contents from the given XML document
     *
     * @param root The Engine XML root node to load from
     * @param loadIO Parse the InputOutputMap tag too
     * @return true if successful, otherwise false
     */
    bool loadXML(QXmlStreamReader &doc, bool loadIO = true);

    /**
     * Save contents to the given XML file.
     *
     * @param doc The XML document to save to
     * @param wksp_root The workspace root node to save under
     * @return true if successful, otherwise false
     */
    bool saveXML(QXmlStreamWriter *doc);

    /**
     * Append a message to the Doc error log. This can be used to display
     * errors once a project is loaded.
     */
    void appendToErrorLog(QString error);

    /**
     * Clear any previously filled error log
     */
    void clearErrorLog();

    /**
     * Retrieve the error log string, filled during a project load
     */
    QString errorLog();

private:
    /**
     * Calls postLoad() for each Function after everything has been loaded
     * to do post-load cleanup & mappings.
     */
    void postLoad();

    QString m_errorLog;
};

/** @} */

#endif

/*
  Q Light Controller Plus
  programmercontroller.h

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

#ifndef PROGRAMMERCONTROLLER_H
#define PROGRAMMERCONTROLLER_H

#include <QObject>
#include <QList>
#include <QSet>
#include <QHash>
#include <QColor>
#include <QTimer>
#include <QVector3D>

#include "scenevalue.h"
#include "doc.h"

class ProgrammerFlasher;
class HighlightEffect;
class QLCPalette;
class Scene;

/**
 * Fork-owned holder for all "programmer mode" state and logic.
 *
 * Doc keeps thin forwarder methods (its public API is unchanged) that
 * delegate to this controller, so no call sites or connect() sites
 * elsewhere need to change. This keeps the programmer feature off the
 * shared engine file's merge-conflict surface.
 *
 * The controller is a QObject child of Doc (auto-deleted) and reaches
 * Doc only through Doc's public methods via the m_doc back-pointer.
 */
class ProgrammerController final : public QObject
{
    Q_OBJECT

public:
    ProgrammerController(Doc *doc);

    /*********************************************************************
     * Programmer selection
     *********************************************************************/
public:
    QList<quint32> programmerSelection() const;
    void setProgrammerSelection(const QList<quint32>& fixtureIds);
    void addToProgrammerSelection(const QList<quint32>& fixtureIds);
    void removeFromProgrammerSelection(const QList<quint32>& fixtureIds);
    void toggleInProgrammerSelection(const QList<quint32>& fixtureIds);
    void clearProgrammerSelection();
    bool isInProgrammerSelection(quint32 fixtureId) const;
    bool allInProgrammerSelection(const QList<quint32>& fixtureIds) const;

    QColor programmerColor() const;
    void setProgrammerColorComponent(int qlcPrimaryColour, uchar value);

    /*********************************************************************
     * Programmer Values
     *********************************************************************/
public:
    void setProgrammerValue(quint32 fixtureId, quint32 channel, uchar value);
    void clearProgrammerValues();
    bool isProgrammerDirty() const;
    quint32 saveProgrammerAsScene(const QString &name);

    QList<Doc::SaveBucket> proposedSaveBuckets() const;
    QList<Doc::SaveBucket> splitBucketByGroup(const Doc::SaveBucket &bucket) const;
    quint32 saveBucketAsScene(const Doc::SaveBucket &bucket,
                              const QString &name,
                              const QString &path);
    quint32 saveBucketAsGroupScene(const Doc::SaveBucket &bucket,
                                   const QString &name,
                                   const QString &path);

    void stepCurrentChaser(int direction);

    quint32 findMatchingScene(
        const QHash<quint32, QHash<quint32, uchar>> &values,
        quint32 excludeSceneId = (quint32)~0) const;

    QList<quint32> collectionsContaining(quint32 sceneId) const;
    bool replaceSceneInCollection(quint32 collectionId,
                                  quint32 oldSceneId,
                                  quint32 newSceneId);
    void revertSceneFromSnapshot(quint32 sceneId);
    quint32 singleRunningCollection() const;
    void flashFixture(quint32 fixtureId, int durationMs = 180);

    /*********************************************************************
     * Highlight mode
     *********************************************************************/
public:
    /** Toggle highlight on/off. When on, selected fixtures are driven to
     *  full white on every DMX tick (bypasses running functions). */
    bool isHighlightActive() const;
    void setHighlightActive(bool active);

    /*********************************************************************
     * Design-mode joystick (writes pan/tilt into the focused scene)
     *********************************************************************/
public:
    /** Apply current pan/tilt norm values as scene values on the focused scene.
     *  Called in Design mode; no-op if no scene is focused or no fixtures selected. */
    void applyDesignJoystick();

    /** Finalize a design-mode joystick edit: the Aim target was moved live on
     *  every tick, so this just marks the scene edited and clears stage-aim
     *  state.  Called when the user confirms Save in the Programming tab. */
    void commitDesignJoystick();

    /** Joystick speed factor for Design-mode position editing.
     *  1.0 = default: Aim palette moves at ~2 m/s stage-space at full deflection;
     *  PanTilt palette traverses ~50% of range per second at full deflection. */
    float joystickSensitivity() const { return m_joystickSensitivity; }
    void  setJoystickSensitivity(float s);

    /** Global joystick deadzone (fraction of half-range, 0..0.4), captured from
     *  the bound HID profile.  Sent on the "joystick" data channel so effect
     *  scripts honour the same deadzone as the device profile. */
    float joystickDeadzone() const { return m_joystickDeadzone; }

    /*********************************************************************
     * Controller axis binding (program-level; shared across all effects)
     *********************************************************************/
public:
    /** Capture next input event as the pan (X) axis. */
    void bindPanAxis();
    /** Capture next input event as the tilt (Y) axis. */
    void bindTiltAxis();
    /** Clear both axis bindings. */
    void clearAxisBindings();

    bool isPanBound()         const { return m_panBound; }
    bool isTiltBound()        const { return m_tiltBound; }

    /** True if a joystick axis is bound AND a controller is connected, i.e. the
     *  follow-spot can actually be driven. False = "nothing plugged in". */
    bool hasFollowSpotInput() const;
    bool isCapturingPan()     const { return m_capturingPan; }
    bool isCapturingTilt()    const { return m_capturingTilt; }
    bool isBoundFromProfile() const { return m_boundFromProfile; }

    /** Human-readable label for the current pan/tilt binding. */
    QString panAxisLabel()  const;
    QString tiltAxisLabel() const;

    /** Scan all input universes for HID profiles with pan/tilt slots and
     *  auto-bind them.  Called after followspot activation and after workspace
     *  load.  Emits axisBindingChanged() if anything was bound. */
    void autoBindFromProfile();

    bool isShowLocked() const;
    void setShowLocked(bool locked);

    quint32 routeProgrammerEdit(quint32 fid, quint32 ch, uchar value);
    int rerouteProgrammerValues();
    QSet<quint32> editedSceneIds() const;

    /*********************************************************************
     * Look-level routing (palette-aware, scene/palette-focused)
     *********************************************************************/
public:
    /** Called by ProgrammingManager when a scene is loaded into the canvas.
     *  Sets the "focused scene" for look-level routing. */
    void setFocusedScene(quint32 sceneId);
    /** Seed the stage-aim/pin from the scene's Aim target (if any), emitting
     *  followSpotPinChanged so the pin appears on the target. */
    void seedStageAimFromScene(quint32 sceneId);

    /** Called by ProgrammingManager when a specific palette (look) is
     *  selected or deselected in the canvas. When valid, slider edits
     *  target ONLY that palette; when invalidId() any matching palette
     *  in the focused scene is eligible. */
    void setFocusedPalette(quint32 paletteId);

    /** Try to route a parameter slider edit at the palette (look) level.
     *  @param qlcChannelGroup  QLCChannel::Group value (Pan, Tilt, Intensity…)
     *  @param groupIndex       0 for primary, 1 for secondary (e.g. Zoom vs Focus)
     *  @param rawValue         0–255 DMX byte from the slider
     *  @return  owning scene id on success, Function::invalidId() to fall through
     *           to the normal per-fixture DMX path. */
    quint32 routeProgrammerEditByChannelType(int qlcChannelGroup, int groupIndex, uchar rawValue);

    Doc::PadMode padMode() const;
    void setPadMode(Doc::PadMode mode);
    quint32 activeProgrammerGroup() const;

    QSet<quint32> programmerSubSelection() const;
    bool isInProgrammerSubSelection(quint32 fid) const;
    void toggleInProgrammerSubSelection(quint32 fid);
    void clearProgrammerSubSelection();

    bool hasProgrammerValues() const;
    QHash<quint32, QHash<quint32, uchar>> programmerValues() const;
    void revertProgrammer();

signals:
    /** Emitted whenever the programmer selection changes. */
    void programmerSelectionChanged();
    /** Emitted when highlight active state changes. */
    void highlightActiveChanged(bool active);
    /** Emitted when a pan or tilt axis binding is captured or cleared. */
    void axisBindingChanged();
    /** Emitted when a controller button with a named action is pressed.
     *  Action values: "chaser_step_forward", "chaser_step_back",
     *  "followspot_toggle", "highlight_toggle", "programmer_clear". */
    void buttonActionTriggered(const QString &action);
    /** Emitted whenever the programmer-values map transitions between
        empty and non-empty. */
    void programmerDirtyChanged(bool dirty);
    /** Emitted when padMode() changes. */
    void padModeChanged(Doc::PadMode mode);
    /** Emitted whenever programmerSubSelection() changes. */
    void programmerSubSelectionChanged();
    /** Emitted when the show-mode lock changes. */
    void showLockedChanged(bool locked);
    /** Emitted each time the bound pan/tilt axes carry new values.
     *  @p pan and @p tilt are normalised 0.0–1.0 (centre = 0.5). */
    void joystickUpdated(float pan, float tilt);
    /** Emitted by applyDesignJoystick() after it successfully writes
     *  pan/tilt values (to scene or to a PanTilt palette).  The UI uses
     *  this to enable the Save button. */
    void designPositionWritten();

    /** Emitted whenever the followspot beam position changes in Design mode.
     *  @p visible is false when no floor position is computable (hide pin).
     *  @p xMeters, @p yMeters give the floor XY in metres (stage space). */
    void followSpotPinChanged(bool visible, float xMeters, float yMeters);

    /** Emitted when joystick sensitivity changes. */
    void joystickSensitivityChanged(float s);

private slots:
    /** Maintain m_runningScenes as functions start / stop. */
    void slotProgrammerFunctionStarted(quint32 fid);
    void slotProgrammerFunctionStopped(quint32 fid);
    /** Sync highlight / followspot fixtures when selection changes. */
    void slotSyncEffectFixtures();
    /** Route controller input to the joystick data channel and JS effect palettes. */
    void slotControllerInputChanged(quint32 universe, quint32 channel,
                                    uchar value, const QString &key);
    /** Re-centre the joystick and re-seed the aim/pin when the app mode changes. */
    void slotDocModeChanged(Doc::Mode mode);
    /** A fixture's rig assignment changed (mounting / pan-zero facing / truss) —
     *  re-resolve live Aim looks so the new geometry shows immediately. */
    void slotRigPropsChanged(quint32 fid);

private:
    void connectControllerInput();
    /** Push pan/tilt binding into any followspot.js palette so EffectScriptRunner
     *  routes values to them via its normal universe/channel mechanism. */
    void updateFollowspotJsBindings();
    QString axisLabel(quint32 universe, quint32 channel) const;

    /** Fill in defaultName / defaultPath for each bucket (category- and
        group-derived names, with collision-avoiding numeric suffix).
        Shared by proposedSaveBuckets() and splitBucketByGroup(). */
    void finalizeSaveBuckets(QList<Doc::SaveBucket> &buckets) const;

    /** Build a QLCPalette capturing a bucket's look (Color/Dimmer/
        PanTilt/Gobo/Shutter, derived from category + channel groups).
        Returns a heap palette owned by the caller (NOT yet added to the
        Doc), or NULL when the bucket can't be expressed as one palette
        (e.g. a mixed Special bucket) — caller then bakes per-fixture. */
    QLCPalette *buildPaletteFromBucket(const Doc::SaveBucket &bucket) const;

    /** Palette-aware live editing. When the whole of a running group
        scene's group(s) is selected (no pad sub-selection), a live edit
        to a channel that scene drives via a palette updates the PALETTE
        (so the whole group follows) instead of becoming a per-channel
        override. Returns the scene id if it took the edit, else
        invalidId (caller falls through to per-channel routing). */
    quint32 tryRoutePaletteEdit(quint32 fid, quint32 ch, uchar value,
                                const QList<Scene*> &candidates);

    /** True iff the current programmer selection is exactly the union of
        @p groups' fixtures (i.e. the user is editing the whole group). */
    bool selectionCoversGroups(const QList<quint32> &groups) const;

    enum PaletteEditOutcome {
        PaletteCantDerive,   //!< type not back-derivable from a channel edit (e.g. Pan/Tilt)
        PaletteUnchanged,    //!< derivable but value already matches
        PaletteChanged       //!< palette value updated
    };
    /** Push a live channel edit into @p pal's value where the palette
        type can be back-derived (Color from the composite programmer
        color; Dimmer/Gobo/Shutter from the raw value). */
    PaletteEditOutcome updatePaletteForEdit(QLCPalette *pal, uchar value);

private:
    Doc *m_doc;

    QList<quint32> m_programmerSelection;
    QSet<quint32> m_programmerSelectionLookup;
    QColor m_programmerColor;
    QHash<quint32, QHash<quint32, uchar>> m_programmerValues;
    /** Most-recently-started running Scene fids, oldest → newest.
        routeProgrammerEdit walks this in reverse for LTP routing. */
    QList<quint32> m_runningScenes;
    /** Most-recently-started running Chaser fids, oldest → newest.
        stepCurrentChaser drives the last entry. */
    QList<quint32> m_runningChasers;
    /** Most-recently-started running Collection fids. Used for the
        Save dialog's "add to running collection" sugar. */
    QList<quint32> m_runningCollections;
    /** Owns the per-tick flasher DMXSource for fixture flash-to-identify. */
    ProgrammerFlasher *m_programmerFlasher = nullptr;
    /** Persistent highlight DMXSource — holds selected fixtures at white. */
    HighlightEffect *m_highlightEffect = nullptr;
    bool m_highlightActive = false;
    // Controller axis binding (program-wide)
    quint32 m_panUniverse = 0,  m_panChannel  = 0;
    quint32 m_tiltUniverse = 0, m_tiltChannel = 0;
    bool m_panBound  = false;
    bool m_tiltBound = false;
    float m_panNorm  = 0.5f;   // last normalised pan value (0-1, centre=0.5)
    float m_tiltNorm = 0.5f;   // last normalised tilt value
    bool m_boundFromProfile = false; // true when binding came from autoBindFromProfile()
    bool m_capturingPan  = false;
    bool m_capturingTilt = false;
    bool m_controllerInputConnected = false;

    /** Show-mode safety lock. */
    bool m_showLocked = false;
    /** Scenes whose values have been mutated by the programmer
        since the last Save / Revert. */
    QSet<quint32> m_editedScenes;
    /** Pre-edit value snapshot per scene: captured the first time a
        scene becomes "edited" so Revert can restore it. */
    QHash<quint32, QList<SceneValue>> m_sceneSnapshots;
    /** Current pad-grid mode (default Off). */
    Doc::PadMode m_padMode = Doc::PadModeOff;
    /** Per-fixture refinement within the active programmer group. */
    QSet<quint32> m_programmerSubSelection;

    /** Scene currently shown in the Programming canvas (invalidId when none). */
    quint32 m_focusedSceneId = Function::invalidId();
    /** Specific palette selected in the canvas look list (invalidId = any). */
    quint32 m_focusedPaletteId = QLCPalette::invalidId();

    // Design-mode stage-space joystick state (Aim palette path)
    float m_stageAimX = 0.0f;
    float m_stageAimY = 0.0f;
    bool  m_stageAimValid = false;   // true once initialized from target this drag
    float m_joystickSensitivity = 1.0f;
    float m_joystickDeadzone    = 0.05f;  // from HID profile; default matches followspot.js

    // Operate-mode transient follow: in Run the joystick moves the aim target live
    // (so heads converge on the moving spot) but the SAVED target must not change.
    // Record its authored position on Run entry and restore on Run/scene exit.
    quint32   m_aimRestoreTgtId = 0;       // StageTarget id whose pos is saved
    QVector3D m_aimRestoreOrig;            // authored position to restore
    bool      m_aimRestoreValid = false;
    /** Restore the transiently-moved Operate aim target to its authored position. */
    void restoreAim();

    // followMode = lastPosition: the beam's last floor position persists across
    // scene transitions (unlike m_stageAimValid, reset by setFocusedScene) so a
    // new scene can resume the beam where it was rather than snapping to target.
    bool m_beamHeldValid = false;
    /** True if the scene's followspot effect is in lastPosition mode (default),
     *  i.e. the beam should hold across a transition instead of snapping. */
    bool sceneFollowspotLastPosition(Scene *scene) const;

    /** 50 Hz driver so a held joystick keeps integrating (smooth velocity move). */
    QTimer *m_designJoyTimer = nullptr;


private:
    /** Map an QLCChannel::Group to the QLCPalette::PaletteType that represents it.
     *  Returns QLCPalette::Undefined for channel groups that have no palette type. */
    static QLCPalette::PaletteType channelGroupToPaletteType(int qlcChannelGroup);

    /** True iff a palette of the given type can serve a channel-group + groupIndex edit.
     *  Handles the Pan/PanTilt and Tilt/PanTilt overlap. */
    static bool paletteTypeMatchesChannel(QLCPalette::PaletteType palType,
                                          int qlcChannelGroup, int groupIndex);

    /** Update @p pal's abstract value from @p rawValue for the given channel
     *  group + component index. Returns PaletteChanged / PaletteUnchanged /
     *  PaletteCantDerive (same semantics as updatePaletteForEdit). */
    PaletteEditOutcome updatePaletteForChannelType(QLCPalette *pal,
                                                   int qlcChannelGroup,
                                                   int groupIndex,
                                                   uchar rawValue);

    /** Mark @p sceneId as edited (snapshot + dirty flag) and drop its
     *  runtime faders so the palette re-expands on the next tick. */
    void markSceneEdited(quint32 sceneId);
};

#endif // PROGRAMMERCONTROLLER_H

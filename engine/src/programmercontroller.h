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

#include "scenevalue.h"
#include "doc.h"

class ProgrammerFlasher;
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

    bool isShowLocked() const;
    void setShowLocked(bool locked);

    quint32 routeProgrammerEdit(quint32 fid, quint32 ch, uchar value);
    int rerouteProgrammerValues();
    QSet<quint32> editedSceneIds() const;

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
    /** Emitted whenever the programmer-values map transitions between
        empty and non-empty. */
    void programmerDirtyChanged(bool dirty);
    /** Emitted when padMode() changes. */
    void padModeChanged(Doc::PadMode mode);
    /** Emitted whenever programmerSubSelection() changes. */
    void programmerSubSelectionChanged();
    /** Emitted when the show-mode lock changes. */
    void showLockedChanged(bool locked);

private slots:
    /** Maintain m_runningScenes as functions start / stop. */
    void slotProgrammerFunctionStarted(quint32 fid);
    void slotProgrammerFunctionStopped(quint32 fid);

private:
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
};

#endif // PROGRAMMERCONTROLLER_H

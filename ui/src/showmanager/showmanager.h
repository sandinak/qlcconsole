/*
  Q Light Controller
  showmanager.h

  Copyright (C) Massimo Callegari

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

#ifndef SHOWMANAGER_H
#define SHOWMANAGER_H

#include <QGraphicsView>
#include <QWidget>
#include <QList>

#include "multitrackview.h"
#include "sequenceitem.h"
#include "trackitem.h"
#include "scene.h"
#include "show.h"
#include "doc.h"

class QComboBox;
class QCheckBox;
class QSplitter;
class QToolBar;
class QSpinBox;
class QAction;
class QLabel;
class QTimer;
class Doc;
class Function;
class FunctionsTreeWidget;

/** @addtogroup ui_shows
 * @{
 */

class ShowManager final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(ShowManager)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    ShowManager(QWidget* parent, Doc* doc);
    ~ShowManager();

    /** Get the singleton instance */
    static ShowManager* instance();

    /** Start from scratch; clear everything */
    void clearContents();

    /*********************************************************************
     * Timeline control (global coordination — Operate mode)
     *********************************************************************/
public:
    /** True when the timeline is actively driving the rig: Operate mode, a show
     *  running, and not suspended. Drives the footer "under timeline control"
     *  indicator. */
    bool timelineControlActive() const;

    /** True when a running timeline has been suspended (VC takeover). */
    bool timelineSuspended() const;

    /** Suspend/resume the running show's output (keeps the playhead position).
     *  No-op when no show is running. Emits timelineControlChanged(). */
    void setTimelineSuspended(bool enable);

    /** Toggle timeline suspend on the running show (for the MIDI-mappable VC
     *  action / global button). No-op when no show is running. */
    void toggleTimelineSuspended();

    /** Id of the currently-selected show (invalid if none). */
    quint32 currentShowId() const;

    /** Arm/disarm timecode-follow on the current show (global control). */
    void setFollowTimecode(bool enable);
    bool followTimecode() const;
    void toggleFollowTimecode();

signals:
    /** Emitted when the FunctionManager's tab is de/activated */
    void functionManagerActive(bool active);

    /** Emitted when the timeline-control state changes (running/suspended/mode)
     *  so global indicators (footer, toolbar) can refresh. */
    void timelineControlChanged();

    /** Emitted when the current show's timecode-follow arming changes. */
    void followTimecodeChanged(bool enabled);

protected:
    /** @reimp */
    void showEvent(QShowEvent* ev) override;

    /** @reimp */
    void hideEvent(QHideEvent* ev) override;

protected:
    static ShowManager *s_instance;

    Doc *m_doc;
    /** Currently selected show */
    Show *m_show;
    /** Currently selected track */
    Track *m_currentTrack;
    /** Currently selected scene */
    Scene *m_currentScene;
    /** Scene editor instance reference */
    QWidget *m_sceneEditor;
    /** Right editor instance reference (can edit Chaser, Audio, Video) */
    QWidget *m_currentEditor;
    /** ID of the Function currently edited on the right */
    quint32 m_editorFunctionID;

    /** Index of the currently selected Show
     * (basically the m_showsCombo index) */
    int m_selectedShowIndex;

    /** Track if cursor is interactively being moved during pause */
    bool cursorMovedDuringPause;

private:
    void showSceneEditor(Scene *scene);
    void hideRightEditor();
    void showRightEditor(Function *function);

private:
    QSplitter *m_splitter; // main view splitter (horizontal)
    QSplitter *m_vsplitter; // multitrack view splitter (vertical)
    MultiTrackView *m_showview;
    /** Drag source: functions to drop onto timeline tracks */
    FunctionsTreeWidget *m_funcTree;

    /** Add a function to a track at a given start time, creating the right
     *  timeline item for its type (scenes are wrapped in a Sequence). Returns
     *  the created ShowFunction (for collision resolution), or NULL. */
    ShowFunction *addFunctionToTrack(Function *f, Track *track, quint32 startTime);

    /** Stop + wait for the runner before a structural edit, so the timer thread
     *  isn't walking the track/ShowFunction lists we're about to mutate (UAF). */
    void stopForStructuralEdit();

    /*********************************************************************
     * Menus, toolbar & actions
     *********************************************************************/
protected:
    void initActions();
    void initToolbar();
    void updateShowsCombo();
    void updateMultiTrackView();

private:
    bool checkOverlapping(quint32 startTime, quint32 duration);

    QToolBar *m_toolbar;
    QComboBox *m_showsCombo;
    QLabel *m_timeLabel;
    QAction *m_addShowAction;
    QAction *m_renameShowAction;
    QAction *m_deleteShowAction;
    QAction *m_addTrackAction;
    QAction *m_addSequenceAction;
    QAction *m_addAudioAction;
    QAction *m_addVideoAction;
    QAction *m_copyAction;
    QAction *m_pasteAction;
    QAction *m_deleteAction;
    QAction *m_colorAction;
    QAction *m_lockAction;
    QAction *m_timingsAction;
    QAction *m_snapGridAction;
    QAction *m_stopAction;
    QAction *m_playAction;
    QComboBox *m_timeDivisionCombo;
    QSpinBox *m_bpmField;

    /* MIDI Time Code follow */
    QAction *m_followMtcAction;

protected slots:
    /** Slot called when the user selects a show from
     *  the shows combo box */
    void slotShowsComboChanged(int idx);

    /** Slot called when the user request to add a new show */
    void slotAddShow();
    /** Rename the current show (inline dialog). */
    void slotRenameShow();
    /** Delete the current show (with confirmation). */
    void slotDeleteShow();
    /** Undo the last timeline edit (Ctrl-Z). */
    void slotUndo();

    void slotAddItem();
    void slotAddSequence();
    void slotAddAudio();
    void slotAddVideo();

    void slotCopy();
    void slotPaste();
    void slotDelete();

    /*********************************************************************
     * Playback
     *********************************************************************/
protected slots:
    void slotStopPlayback();
    void slotStartPlayback();
    void slotShowStopped();
    void slotShowRunningChanged();

    /*********************************************************************
     * MIDI Time Code follow
     *********************************************************************/
protected slots:
    void slotFollowMtcToggled(bool enable);
    void slotTimecodePosition(quint32 msPosition);
    void slotTimecodeRunningChanged(bool running);

private:
    /*********************************************************************
     * Time division
     *********************************************************************/
protected slots:
    void slotTimeDivisionTypeChanged(int idx);
    void slotBPMValueChanged(int value);

    /*********************************************************************
     * UI events
     *********************************************************************/
protected slots:
    void slotViewClicked(QMouseEvent *event);
    void slotShowItemMoved(ShowItem *item, quint32 time, bool moved);

    void slotUpdateTime(quint32 msec_time);
    void slotUpdateTimeAndCursor(quint32 msec_time);
    void slotTrackClicked(Track *track);
    void slotTrackDoubleClicked(Track *track);
    void slotTrackMoved(Track *track, int direction);
    void slotTrackDelete(Track *track);
    void slotChangeColor();
    void slotChangeLock();
    void slotShowTimingsTool();
    void slotShowItemStartTimeChanged(ShowItem *item, int msec);
    void slotShowItemDurationChanged(ShowItem *item, int msec, bool stretch);
    void slotFunctionDropped(quint32 funcID, quint32 startTime, Track *track);
    void slotAddAtRequested(quint32 startTime, Track *track);
    void slotNewTrackRequested();
    void slotItemDroppedBelowTracks(ShowItem *item);
    void slotShowLockedChanged(bool locked);
    void slotTrackColorChangeRequested(Track *track);
    void slotMarkerAddRequested(quint32 time);
    void slotMarkerEditRequested(quint32 time);
    void slotMarkerDeleteRequested(quint32 time);
    void slotMarkerColorRequested(quint32 time);
    void slotMarkerSetCueList(quint32 time);
    void slotMarkerRelabel(quint32 time, QString label);
    void slotMarkerMoved(quint32 oldStart, quint32 newStart, quint32 newEnd,
                         QString label, QColor color);
    void slotShowLengthChangeRequested(quint32 ms);
    void slotShowEndAtSmpteRequested(quint32 smpteMs);
    void slotToggleSnapToGrid(bool enable);
    void slotChangeSize(int width, int height);
    void slotStepSelectionChanged(int index);

    /*********************************************************************
     * Doc events
     *********************************************************************/
protected slots:
    void slotDocClearing();
    void slotDocLoaded();
    void slotFunctionRemoved(quint32 id);

private:
    FunctionParent functionParent() const;

    /** May the show currently drive DMX output? In Operate mode: always (VC
        coordination handles priority). In Design mode: only when the Show tab
        is the active/visible tab — the active tab owns the rig, so a background
        show doesn't fight the Programming-tab live preview. */
    bool showMayOutput() const;

    /** Enable/disable the show-scoped toolbar actions (rename/delete show, add
     *  track, …) based on whether a show currently exists. */
    void updateShowControls();

    /*********************************************************************
     * Undo (coarse whole-timeline snapshots)
     *********************************************************************/
private:
    /** One ShowFunction placement on a track. */
    struct SFSnapshot
    {
        quint32 functionID;
        quint32 startTime;
        quint32 duration;
        QColor  color;
    };
    /** One track's identity + its placed functions. */
    struct TrackSnapshot
    {
        QString name;
        bool    mute;
        QColor  color;
        quint32 sceneID;
        QList<SFSnapshot> funcs;
    };
    /** The whole timeline of the current show (tracks + markers). */
    struct TimelineSnapshot
    {
        QList<TrackSnapshot> tracks;
        QMap<quint32, ShowMarker> markers;
    };

    /** Capture the current show's timeline. */
    TimelineSnapshot captureSnapshot() const;
    /** Rebuild the current show's timeline from a snapshot + refresh the view. */
    void restoreSnapshot(const TimelineSnapshot &snap);
    /** Push a pre-edit snapshot onto the undo stack (bounded). Call at the TOP
     *  of a mutating slot, before it changes the model. No-op if no show. */
    void pushUndoSnapshot();

    QList<TimelineSnapshot> m_undoStack;
    QAction *m_undoAction;
};

/** @} */

#endif

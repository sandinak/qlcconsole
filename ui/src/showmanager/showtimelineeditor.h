/*
  Q Light Controller Plus
  showtimelineeditor.h

  Fork-owned embeddable Show timeline editor. Wraps a MultiTrackView plus the
  show-side orchestration (create/rename/reorder/colour tracks, place & delete
  clips, section markers incl. manual-cue-list links, transport) so a Show can
  be built AND previewed additively wherever the widget is hosted — notably the
  Programming tab's canvas, alongside Collection/Chaser editors. The full-blown
  Show Manager tab keeps its extra chrome (shows combo, MTC source, footer chips,
  undo stack); this is the compact core.

  It edits the Show it is given directly (tracks + ShowFunctions on the engine
  model), so a show built here is identical to one built in the Show Manager.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef SHOWTIMELINEEDITOR_H
#define SHOWTIMELINEEDITOR_H

#include <QWidget>

#include "function.h"

class MultiTrackView;
class ShowFunction;
class ShowItem;
class QAction;
class Function;
class Track;
class Show;
class Doc;

/** @addtogroup ui_shows
 * @{
 */

class ShowTimelineEditor final : public QWidget
{
    Q_OBJECT

public:
    ShowTimelineEditor(QWidget *parent, Show *show, Doc *doc);
    ~ShowTimelineEditor();

    Show *show() const { return m_show; }

private:
    /** Populate the timeline from the current show's tracks + ShowFunctions. */
    void reload();

    /** Place @p f onto @p track at @p startTime, creating the ShowFunction and
     *  the matching timeline item. Mirrors ShowManager::addFunctionToTrack. */
    ShowFunction *addFunctionToTrack(Function *f, Track *track, quint32 startTime);

    /** FunctionParent used to start/stop the show from this editor. */
    FunctionParent functionParent() const;

    /** Stop + wait for the runner before a structural edit, so the timer thread
     *  isn't walking the track/ShowFunction lists we're about to mutate (UAF). */
    void stopForStructuralEdit();

    /** Sync the play/stop action enabled state + play icon from show state. */
    void updateTransportState();

private slots:
    // Building
    void slotFunctionDropped(quint32 funcID, quint32 startTime, Track *track);
    void slotAddAtRequested(quint32 startTime, Track *track);
    void slotNewTrackRequested();
    void slotDeleteSelected();
    void slotShowItemMoved(ShowItem *item, quint32 time, bool moved);

    // Tracks
    void slotTrackDelete(Track *track);
    void slotTrackDoubleClicked(Track *track);
    void slotTrackMoved(Track *track, int direction);
    void slotTrackColorChangeRequested(Track *track);

    // Markers (section labels + manual-cue-list links = the timecode↔manual seam)
    void slotMarkerAddRequested(quint32 time);
    void slotMarkerEditRequested(quint32 time);
    void slotMarkerColorRequested(quint32 time);
    void slotMarkerDeleteRequested(quint32 time);
    void slotMarkerRelabel(quint32 time, QString label);
    void slotMarkerMoved(quint32 oldStart, quint32 newStart, quint32 newEnd,
                         QString label, QColor color);
    void slotMarkerSetCueList(quint32 time);
    void slotShowLengthChangeRequested(quint32 ms);

    // Transport
    void slotPlayPause();
    void slotStop();
    void slotShowTimeChanged(quint32 ms);
    void slotShowRunningChanged();

private:
    Doc  *m_doc;
    Show *m_show;
    MultiTrackView *m_showview;
    Track *m_currentTrack;

    QAction *m_playAction;
    QAction *m_stopAction;
    QAction *m_deleteAction;
};

/** @} */

#endif

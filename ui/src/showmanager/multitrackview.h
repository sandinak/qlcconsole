/*
  Q Light Controller Plus
  multitrackview.h

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

#ifndef MULTITRACKVIEW_H
#define MULTITRACKVIEW_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QSlider>
#include <QWidget>
#include <QMap>
#include <QColor>

#include "rgbmatrixitem.h"
#include "collectionitem.h"
#include "sequenceitem.h"
#include "headeritems.h"
#include "trackitem.h"
#include "audioitem.h"
#include "efxitem.h"
#include "videoitem.h"
#include "collection.h"
#include "chaser.h"
#include "track.h"

/** @addtogroup ui_shows
 * @{
 */

class QTimer;
class QGraphicsProxyWidget;

class MultiTrackView final : public QGraphicsView
{
    Q_OBJECT

public:
    MultiTrackView(QWidget *parent = 0);

    /** Update tracks horizontal dividers when the view changes */
    void updateTracksDividers();

    /** Set the multitrack view size in pixels */
    void setViewSize(int width, int height);

    /** Provide the section markers (start ms -> ShowMarker) to render. */
    void setMarkers(const QMap<quint32, ShowMarker> &markers);

    /** Start-key of the marker region under the given scene X, else UINT_MAX. */
    quint32 markerAt(qreal sceneX) const;

    /** Auto calculation of view size based on items */
    void updateViewSize();

    /*********************************************************************
     * Contents
     *********************************************************************/

    /** Update the multitrack view with the scene elements */
    void resetView();

    /** Add a new track to the view */
    void addTrack(Track *track);

    /** Add a new sequence item to the given track */
    void addSequence(Chaser *chaser, Track *track = NULL, ShowFunction *sf = NULL);

    /** Add a new audio item to the given track */
    void addAudio(Audio *audio, Track *track = NULL, ShowFunction *sf = NULL);

    /** Add a new RGB Matrix item to the given track */
    void addRGBMatrix(RGBMatrix *rgbm, Track *track = NULL, ShowFunction *sf = NULL);

    /** Add a new Collection item to the given track */
    void addCollection(Collection *collection, Track *track = NULL, ShowFunction *sf = NULL);

    /** Add a new EFX item to the given track */
    void addEFX(EFX *efx, Track *track = NULL, ShowFunction *sf = NULL);

    /** Add a new video item to the given track */
    void addVideo(Video *video, Track *track = NULL, ShowFunction *sf = NULL);

    /** Delete the currently selected item */
    quint32 deleteSelectedItem();

    /** Delete a specific ShowFuntion and related ShowItem from the
     *  given track */
    void deleteShowItem(Track *track, ShowFunction *sf);

    /** Master read-only gate for the whole timeline (e.g. the show lock). */
    void setEditable(bool editable);
    bool isEditable() const { return m_editable; }

    /** Big centred hint drawn over the empty canvas (no show / no tracks).
     *  Empty string clears it. */
    void setEmptyMessage(const QString &msg);

    /** Recompute per-item editability from the master gate + per-track locks. */
    void updateItemsEditability();

    /** Set the given track to active state */
    void activateTrack(Track *track);

    /** Move an existing item onto another track (reassigns its ShowFunction
     *  and resolves overlaps on the destination). */
    void moveItemToTrack(ShowItem *item, Track *dest);

    /** get the selected Show item. If none, returns NULL */
    ShowItem *getSelectedItem() const;

    /** All currently-selected Show items (multi-select via Shift-click or
     *  rubber-band). */
    QList<ShowItem *> selectedItems() const;

    /** DAW-style push: after an item is added/moved/resized, remove any
     *  overlaps on its track by butting the anchor after earlier items and
     *  rippling later items to the right. @param sf identifies the anchor. */
    void resolveCollisions(Track *track, ShowFunction *sf);

private:
    /** Core collision resolver, anchored on the given item. */
    void resolveTrackCollisions(int trackIndex, ShowItem *anchor);

    /** Retrieve the index of the given Track.
     *  If trk is NULL, this function returns the currently
     *  selected track.
     */
    int getTrackIndex(Track *trk) const;

    void setItemCommonProperties(ShowItem *item, ShowFunction *func, int trackNum);

    /*********************************************************************
     * Header
     *********************************************************************/
public:
    /** Set the type of header. Can be Time (seconds) or BPM,
     *  in various forms (4/4, 3/4) */
    void setHeaderType(Show::TimeDivision type);

    Show::TimeDivision getHeaderType() const;

    /** When BPM is selected, this function can set a precise
     *  value of time division */
    void setBPMValue(int value);

    void setSnapToGrid(bool enable);

    /*********************************************************************
     * Cursor
     *********************************************************************/
public:
    /** Move cursor to a given time (immediate). */
    void moveCursor(quint32 timePos);

    /** Smooth-follow target for the playhead: a local 60Hz animator eases the
     *  cursor toward this, decoupled from the (bursty, cross-thread) engine
     *  updates, so the motion stays smooth with a touch of lag. */
    void setPlayheadTarget(quint32 timePos);

    /** Stop the smooth playhead animator (on playback stop). */
    void stopPlayhead();

    /** Reset cursor to initial position */
    void rewindCursor();

    /** Get time in milliseconds of the current cursor position */
    quint32 getTimeFromCursor() const;

    /** Return position in pixel of a given time (in msec) */
    quint32 getPositionFromTime(quint32 time) const;

    /** Return the time (in msec) from a given X position */
    quint32 getTimeFromPosition(qreal pos) const;

private:
    QGraphicsScene *m_scene;
    QSlider *m_timeSlider;
    ShowHeaderItem *m_header;
    ShowCursorItem *m_cursor;
    QGraphicsItem * m_vdivider;
    QList <QGraphicsItem *> m_hdividers;
    QList <TrackItem *> m_tracks;
    QList <ShowItem *>m_items;
    bool m_snapToGrid;
    /** Master editable gate (false = timeline read-only) */
    bool m_editable;
    QString m_emptyMessage;    // centred hint when the canvas has no content

    QRubberBand *m_rubberBand; // marquee selection in the tracks area
    QPoint m_rubberOrigin;     // viewport-space press point for the marquee
    bool m_rubberActive;       // a marquee drag is in progress
    /** Section markers: start (ms) -> ShowMarker */
    QMap<quint32, ShowMarker> m_markers;

    /** Smooth playhead animator (main-thread, decoupled from engine updates) */
    QTimer *m_playheadTimer;
    double m_playheadDisplay;
    quint32 m_playheadTarget;
    int m_playheadIdleFrames;

    /** Marker drag state (in the lane): 0=none 1=move 2=resizeLeft 3=resizeRight */
    int m_markerDragMode;
    quint32 m_markerOrigStart; // original start key when the drag began
    quint32 m_dragStart;       // working start during drag
    quint32 m_dragEnd;         // working end during drag
    QString m_dragLabel;       // working label during drag
    QColor m_dragColor;        // working colour during drag
    qreal m_markerGrabDx;      // px between grab point and marker start (move)

    /** Inline marker rename state */
    QGraphicsProxyWidget *m_markerEditProxy;
    quint32 m_markerEditKey;

protected:
    /** Paints the time-division grid behind all items so the divisions are
     *  always visible (not only when snap-to-grid is enabled). */
    void drawBackground(QPainter *painter, const QRectF &rect) override;

    /** Paints the section-marker lane (labels + drop lines) over the tracks. */
    void drawForeground(QPainter *painter, const QRectF &rect) override;

    /** Accept functions dragged in from a function tree and drop them onto
     *  the track row / time position under the cursor. */
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    /** Right-click on an empty part of the timeline to insert a function at
     *  that track row / time position. */
    void contextMenuEvent(QContextMenuEvent *event) override;

public slots:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    /** Begin an inline (embedded line-edit) rename of a marker. */
    void startMarkerEdit(quint32 key);
private slots:
    void slotMarkerEditCommitted();

protected slots:
    void slotHeaderClicked(QGraphicsSceneMouseEvent *event);
    void slotTimeScaleChanged(int val);
    void slotTrackClicked(TrackItem *track);
    void slotTrackDoubleClicked(TrackItem *track);
    void slotTrackSoloFlagChanged(TrackItem*, bool);
    void slotTrackMuteFlagChanged(TrackItem*, bool);
    void slotTrackLockFlagChanged(TrackItem*, bool);
    void slotViewScrolled(int);

    void slotItemMoved(QGraphicsSceneMouseEvent *event, ShowItem *item);
    void slotItemResized(ShowItem *item, bool leftEdge);
    void slotAlignToCursor(ShowItem *item);
    void slotAnimatePlayhead();

signals:
    /** A function was dragged from a tree and dropped on the timeline.
     *  @param funcID the dropped function id
     *  @param startTime start time (ms) computed from the drop x position
     *  @param track the track row under the drop, or NULL to make a new track */
    void functionDropped(quint32 funcID, quint32 startTime, Track *track);

    /** Emitted when the user asks (via right-click) to insert a function at a
     *  timeline position. @param track NULL to make a new track. */
    void addAtRequested(quint32 startTime, Track *track);

    /** Emitted when the user asks to create a new (empty) track. */
    void newTrackRequested();

    /** Section-marker CRUD requests (handled by the Show Manager). */
    void markerAddRequested(quint32 time);
    void markerEditRequested(quint32 time);
    void markerDeleteRequested(quint32 time);
    void markerColorRequested(quint32 time);
    /** Inline rename committed: set the marker's label. */
    void markerRelabelRequested(quint32 time, QString label);
    /** A marker was dragged/resized: replace oldStart with [newStart,newEnd]. */
    void markerMovedRequested(quint32 oldStart, quint32 newStart, quint32 newEnd,
                              QString label, QColor color);

    /** Emitted when an item is dragged below the last track: the caller should
     *  (after confirming) make a new track and move the item onto it. */
    void itemDroppedBelowTracks(ShowItem *item);

    void showItemMoved(ShowItem *item, quint32 time, bool moved);
    void viewClicked(QMouseEvent * e);
    void timeChanged(quint32 msec);
    void trackClicked(Track *track);
    void trackDoubleClicked(Track *track);
    void trackMoved(Track *, int);
    void trackDelete(Track *);
    void trackModified();
    void trackColorChangeRequested(Track *);
};

/** @} */

#endif

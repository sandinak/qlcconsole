/*
  Q Light Controller Plus
  multitrackview.cpp

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

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QRubberBand>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDataStream>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QMessageBox>
#include <QScrollBar>
#include <QPainter>
#include <QSlider>
#include <QTimer>
#include <QMenu>
#include <QInputDialog>
#include <QLineEdit>
#include <QGraphicsProxyWidget>
#include <QDebug>
#include <algorithm>

#include "showitem.h"

static const char *SM_FUNCTION_MIME = "application/x-qlcplus-functions";

#include "multitrackview.h"
#include "track.h"

#define VIEW_DEFAULT_WIDTH  2000
#define VIEW_DEFAULT_HEIGHT 600
// Z of the frozen left track-header column: above clips (~0) so it occludes
// them as they scroll under it, below the playhead cursor (999) and a dragged
// track (1000).
#define TRACK_HEADER_Z      800

MultiTrackView::MultiTrackView(QWidget *parent) :
        QGraphicsView(parent)
{
    m_scene = new QGraphicsScene();
    m_scene->setSceneRect(0, 0, VIEW_DEFAULT_WIDTH, VIEW_DEFAULT_HEIGHT);
    setSceneRect(0, 0, VIEW_DEFAULT_WIDTH, VIEW_DEFAULT_HEIGHT);
    setScene(m_scene);

    // Cache the division grid (drawBackground) and repaint only changed regions
    // so moving the playhead doesn't re-rasterise the whole grid every frame —
    // that was making the cursor chunky once the grid + markers were added.
    setCacheMode(QGraphicsView::CacheBackground);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);

    m_timeSlider = new QSlider(Qt::Horizontal);
    m_timeSlider->setRange(1, 15);
    m_timeSlider->setValue(3);
    m_timeSlider->setSingleStep(1);
    m_timeSlider->setFixedSize(TRACK_WIDTH - 4, HEADER_HEIGHT);

    m_timeSlider->setStyleSheet("QSlider { background-color: #969696; }"
                          "QSlider::groove:horizontal {"
                          "border: 1px solid #999999;"
                          "height: 10px;"
                          "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #b1b1b1, stop:1 #d4d4d4);"
                          "}"
                          "QSlider::handle:horizontal {"
                          "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #c4c4c4, stop:1 #8f8f8f);"
                          "border: 1px solid #5c5c5c;"
                          "width: 20px;"
                          "margin: -2px 0; /* handle is placed by default on the contents rect of the groove. Expand outside the groove */"
                          "border-radius: 4px;"
                          "}");
    connect(m_timeSlider, SIGNAL(valueChanged(int)), this, SLOT(slotTimeScaleChanged(int)));
    m_sliderProxy = m_scene->addWidget(m_timeSlider);
    if (m_sliderProxy != NULL)
        m_sliderProxy->setZValue(TRACK_HEADER_Z + 1); // sits in the pinned corner

    m_header = new ShowHeaderItem(m_scene->width());
    m_header->setPos(TRACK_WIDTH, 0);
    connect(m_header, SIGNAL(itemClicked(QGraphicsSceneMouseEvent *)),
            this, SLOT(slotHeaderClicked(QGraphicsSceneMouseEvent *)));
    m_scene->addItem(m_header);
    m_snapToGrid = false;

    m_cursor = new ShowCursorItem(m_scene->height());
    m_cursor->setPos(TRACK_WIDTH, 0);
    m_cursor->setZValue(999); // make sure the cursor is always on top of everything else
    m_scene->addItem(m_cursor);
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(slotViewScrolled(int)));

    m_vdivider = NULL;
    m_editable = true;

    // Smooth playhead animator: eases the cursor toward the latest target at
    // 60Hz on the main thread, so motion stays smooth even though the engine's
    // time updates arrive in cross-thread bursts.
    m_playheadDisplay = 0;
    m_playheadTarget = 0;
    m_playheadIdleFrames = 0;
    m_playheadTimer = new QTimer(this);
    m_playheadTimer->setInterval(16);
    connect(m_playheadTimer, SIGNAL(timeout()), this, SLOT(slotAnimatePlayhead()));

    m_rubberBand = NULL;
    m_rubberActive = false;
    m_markerDragMode = 0;
    m_markerOrigStart = UINT_MAX;
    m_dragStart = 0;
    m_dragEnd = 0;
    m_markerGrabDx = 0;
    m_markerEditProxy = NULL;
    m_markerEditKey = UINT_MAX;
    m_configuredLength = 0;
    m_endDrag = false;
    m_endDragValue = 0;
    m_endDragEdge = false;
    m_endDragAnchorMs = 0;
    m_endDragAnchorX = 0;
    m_dragScrollDir = 0;
    m_dragScrollTimer = new QTimer(this);
    m_dragScrollTimer->setInterval(30);   // gentle auto-scroll while extending
    connect(m_dragScrollTimer, &QTimer::timeout, this, &MultiTrackView::slotDragAutoScroll);
    // draw horizontal and vertical lines for tracks
    updateTracksDividers();

    // Accept functions dragged in from the function tree. Drops are delivered
    // to the VIEWPORT, so enabling acceptDrops on the view alone is not enough
    // (the viewport was created with drops disabled) — enable it explicitly.
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
}

void MultiTrackView::updateTracksDividers()
{
    if (m_hdividers.count() > 0)
    {
        int hdivCount = m_hdividers.count();
        for (int c = 0; c < hdivCount; c++)
            m_scene->removeItem(m_hdividers.takeLast());
        m_hdividers.clear();
    }
    if (m_vdivider != NULL)
        m_scene->removeItem(m_vdivider);

    int ypos = TRACKS_TOP + TRACK_HEIGHT;
    // Compact mode draws exactly one divider per existing track (min 1) so an
    // embedded/empty show doesn't render a stack of unused rows. The full tab
    // keeps its roomy 6-row minimum.
    int hDivNum;
    if (m_compact)
        hDivNum = qMax(1, m_tracks.count());
    else
        hDivNum = (m_tracks.count() > 5) ? m_tracks.count() : 6;
    for (int j = 0; j < hDivNum; j++)
    {
        QGraphicsItem *item = m_scene->addRect(0, ypos + (j * TRACK_HEIGHT),
                                               m_scene->width(), 1,
                                               QPen(QColor(150, 150, 150, 255)),
                                               QBrush(QColor(190, 190, 190, 255)));
        item->setZValue(-1);
        m_hdividers.append(item);
    }
    m_vdivider = m_scene->addRect(TRACK_WIDTH - 3, 0, 3, m_scene->height(),
                        QPen(QColor(150, 150, 150, 255)),
                        QBrush(QColor(190, 190, 190, 255)));
    pinLeftColumn(); // freeze the (recreated) divider + header column
}

void MultiTrackView::setViewSize(int width, int height)
{
    m_scene->setSceneRect(0, 0, width, height);
    setSceneRect(0, 0, width, height);
    m_header->setWidth(width);
    if (m_snapToGrid == true)
        m_header->setHeight(height);
    else
        m_header->setHeight(HEADER_HEIGHT);
    updateTracksDividers();
}

void MultiTrackView::updateViewSize()
{
    quint32 gWidth = VIEW_DEFAULT_WIDTH;
    quint32 gHeight = VIEW_DEFAULT_HEIGHT;

    // find leftmost show item
    foreach (ShowItem *item, m_items)
    {
        if (item->x() + item->getWidth() > gWidth)
            gWidth = item->x() + item->getWidth();
    }

    // Ensure the end handle (a configured length dragged past content) stays on
    // the canvas, with headroom so it can always be grabbed and extended.
    if (m_configuredLength > 0)
    {
        quint32 endX = getPositionFromTime(m_configuredLength) + 200;
        if (endX > gWidth)
            gWidth = endX;
    }

    // Compact mode: size the scene height to the actual track count (min 1) so
    // unused rows never render and an empty show shows no vertical scrollbar.
    if (m_compact)
    {
        int rows = qMax(1, m_tracks.count());
        gHeight = TRACKS_TOP + (rows * TRACK_HEIGHT) + 2;
        m_cursor->setHeight(gHeight);
        setViewSize(gWidth + 1000, gHeight);
        return;
    }

    if ((m_tracks.count() * TRACK_HEIGHT) + HEADER_HEIGHT > VIEW_DEFAULT_HEIGHT)
    {
        gHeight = (m_tracks.count() * TRACK_HEIGHT) + HEADER_HEIGHT;
        m_cursor->setHeight(gHeight);
    }

    if (gWidth > VIEW_DEFAULT_WIDTH || gHeight > VIEW_DEFAULT_HEIGHT)
        setViewSize(gWidth + 1000, gHeight);
}

void MultiTrackView::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    updateTracksDividers();
    updateViewSize();
}

void MultiTrackView::resetView()
{
    // removeItem() hands ownership back to us, so DELETE the items (they are
    // re-created on the next populate). Without this every timeline rebuild
    // leaked its TrackItems/ShowItems — invisible in stock (one persistent view)
    // but the fork rebuilds a fresh view per Programming-tab Show open + on every
    // edit (reload() -> resetView()).
    for (int t = 0; t < m_tracks.count(); t++)
    {
        m_scene->removeItem(m_tracks.at(t));
        delete m_tracks.at(t);
    }
    m_tracks.clear();

    for (int i = 0; i < m_items.count(); i++)
    {
        m_scene->removeItem(m_items.at(i));
        delete m_items.at(i);
    }
    m_items.clear();

    rewindCursor();
    this->horizontalScrollBar()->setSliderPosition(0);
    this->verticalScrollBar()->setSliderPosition(0);
    m_scene->update();
}

MultiTrackView::~MultiTrackView()
{
    // QGraphicsView does not own an unparented scene — delete it (which deletes
    // the header/cursor/divider items still in it). Without this, each embedded
    // ShowTimelineEditor leaked its whole scene on teardown.
    delete m_scene;
    m_scene = NULL;
}

void MultiTrackView::addTrack(Track *track)
{
    // check if track already exists
    foreach (TrackItem *item, m_tracks)
    {
        if (item->getTrack()->id() == track->id())
            return;
    }

    TrackItem *trackItem = new TrackItem(track, m_tracks.count());
    trackItem->setName(track->name());
    trackItem->setPos(0, TRACKS_TOP + (TRACK_HEIGHT * m_tracks.count()));
    trackItem->setZValue(TRACK_HEADER_Z); // frozen header column floats above clips
    m_scene->addItem(trackItem);
    m_tracks.append(trackItem);
    activateTrack(track);
    pinLeftColumn(); // freeze at the current horizontal scroll position
    connect(trackItem, SIGNAL(itemClicked(TrackItem*)),
            this, SLOT(slotTrackClicked(TrackItem*)));
    connect(trackItem, SIGNAL(itemDoubleClicked(TrackItem*)),
            this, SLOT(slotTrackDoubleClicked(TrackItem*)));
    connect(trackItem, SIGNAL(itemSoloFlagChanged(TrackItem*,bool)),
            this, SLOT(slotTrackSoloFlagChanged(TrackItem*,bool)));
    connect(trackItem, SIGNAL(itemMuteFlagChanged(TrackItem*,bool)),
            this, SLOT(slotTrackMuteFlagChanged(TrackItem*,bool)));
    connect(trackItem, SIGNAL(itemMoveUpDown(Track*,int)),
            this, SIGNAL(trackMoved(Track*,int)));
    connect(trackItem, SIGNAL(itemRequestDelete(Track*)),
            this, SIGNAL(trackDelete(Track*)));
    connect(trackItem, SIGNAL(itemRequestNewTrack()),
            this, SIGNAL(newTrackRequested()));
    connect(trackItem, SIGNAL(itemLockChanged(TrackItem*,bool)),
            this, SLOT(slotTrackLockFlagChanged(TrackItem*,bool)));
    connect(trackItem, SIGNAL(itemRenamed(Track*)),
            this, SIGNAL(trackModified()));
    connect(trackItem, SIGNAL(itemColorChangeRequested(Track*)),
            this, SIGNAL(trackColorChangeRequested(Track*)));
    connect(trackItem, SIGNAL(itemPropertiesChanged(Track*)),
            this, SIGNAL(trackModified()));
    connect(trackItem, SIGNAL(itemIntensityChanged(Track*,qreal)),
            this, SIGNAL(trackIntensityChanged(Track*,qreal)));
}

void MultiTrackView::slotTrackLockFlagChanged(TrackItem *, bool)
{
    // A track's lock changed: refresh which items are editable.
    updateItemsEditability();
}

void MultiTrackView::setEditable(bool editable)
{
    m_editable = editable;
    updateItemsEditability();
}

void MultiTrackView::updateItemsEditability()
{
    foreach (ShowItem *item, m_items)
    {
        int idx = item->getTrackIndex();
        bool trackLocked = (idx >= 0 && idx < m_tracks.count())
                           ? m_tracks.at(idx)->getTrack()->isLocked() : false;
        item->setEditable(m_editable && !trackLocked);
    }
}

void MultiTrackView::moveItemToTrack(ShowItem *item, Track *dest)
{
    if (item == NULL || dest == NULL)
        return;

    int destIdx = getTrackIndex(dest);
    int oldIdx = item->getTrackIndex();
    Track *oldTrk = (oldIdx >= 0 && oldIdx < m_tracks.count())
                    ? m_tracks.at(oldIdx)->getTrack() : NULL;
    ShowFunction *sf = item->showFunction();
    if (oldTrk != NULL && sf != NULL && oldTrk != dest)
    {
        oldTrk->removeShowFunction(sf, false);
        dest->addShowFunction(sf);
    }
    item->setTrackIndex(destIdx);
    item->setPos(item->x(), TRACKS_TOP + 1 + (destIdx * TRACK_HEIGHT));
    resolveTrackCollisions(destIdx, item);
    m_scene->update();
    updateViewSize();
}

void MultiTrackView::setItemCommonProperties(ShowItem *item, ShowFunction *func, int trackNum)
{
    qDebug() << "[" << func->functionID() << "] Start time:" << func->startTime() << "Duration:" << func->duration();

    item->setTrackIndex(trackNum);

    int timeScale = m_timeSlider->value();

    if (func->startTime() == UINT_MAX)
    {
        item->setStartTime(getTimeFromCursor());
        item->setPos(m_cursor->x() + 2, (TRACKS_TOP + 1) + (trackNum * TRACK_HEIGHT));
    }
    else
        item->setPos(getPositionFromTime(func->startTime()) + 2, (TRACKS_TOP + 1) + (trackNum * TRACK_HEIGHT));

    item->setTimeScale(timeScale);

    connect(item, SIGNAL(itemDropped(QGraphicsSceneMouseEvent *, ShowItem *)),
            this, SLOT(slotItemMoved(QGraphicsSceneMouseEvent *, ShowItem *)));
    connect(item, SIGNAL(itemResized(ShowItem*,bool)),
            this, SLOT(slotItemResized(ShowItem*,bool)));
    connect(item, SIGNAL(alignToCursor(ShowItem*)),
            this, SLOT(slotAlignToCursor(ShowItem*)));
    m_scene->addItem(item);
    m_items.append(item);
    bool trackLocked = (trackNum >= 0 && trackNum < m_tracks.count())
                       ? m_tracks.at(trackNum)->getTrack()->isLocked() : false;
    item->setEditable(m_editable && !trackLocked);
    int new_scene_width = item->x() + item->getWidth();
    if (new_scene_width > VIEW_DEFAULT_WIDTH && new_scene_width > m_scene->width())
        setViewSize(new_scene_width + 500, VIEW_DEFAULT_HEIGHT);
}

void MultiTrackView::addSequence(Chaser *chaser, Track *track, ShowFunction *sf)
{
    if (m_tracks.isEmpty())
        return;

    int trackNum = getTrackIndex(track);

    if (track == NULL)
        track = m_tracks.at(trackNum)->getTrack();

    ShowFunction *func = sf;
    if (func == NULL)
        func = track->createShowFunction(chaser->id());

    SequenceItem *item = new SequenceItem(chaser, func);
    setItemCommonProperties(item, func, trackNum);
}

void MultiTrackView::addAudio(Audio *audio, Track *track, ShowFunction *sf)
{
    if (m_tracks.isEmpty())
        return;

    int trackNum = getTrackIndex(track);

    if (track == NULL)
        track = m_tracks.at(trackNum)->getTrack();

    ShowFunction *func = sf;
    if (func == NULL)
        func = track->createShowFunction(audio->id());

    AudioItem *item = new AudioItem(audio, func);
    setItemCommonProperties(item, func, trackNum);
}

void MultiTrackView::addRGBMatrix(RGBMatrix *rgbm, Track *track, ShowFunction *sf)
{
    if (m_tracks.isEmpty())
        return;

    int trackNum = getTrackIndex(track);

    if (track == NULL)
        track = m_tracks.at(trackNum)->getTrack();

    ShowFunction *func = sf;
    if (func == NULL)
        func = track->createShowFunction(rgbm->id());

    RGBMatrixItem *item = new RGBMatrixItem(rgbm, func);
    setItemCommonProperties(item, func, trackNum);
}

void MultiTrackView::addCollection(Collection *collection, Track *track, ShowFunction *sf)
{
    if (m_tracks.isEmpty())
        return;

    int trackNum = getTrackIndex(track);

    if (track == NULL)
        track = m_tracks.at(trackNum)->getTrack();

    ShowFunction *func = sf;
    if (func == NULL)
    {
        func = track->createShowFunction(collection->id());
        // A collection of scenes reports no finite total duration; give the
        // timeline block an explicit default so it is visible/draggable and
        // the ShowRunner doesn't stop it instantly (stopTime == startTime).
        if (func->duration() == 0 && collection->totalDuration() == 0)
            func->setDuration(10000);
    }

    CollectionItem *item = new CollectionItem(collection, func);
    setItemCommonProperties(item, func, trackNum);
}

void MultiTrackView::addScene(Scene *scene, Track *track, ShowFunction *sf)
{
    if (m_tracks.isEmpty())
        return;

    int trackNum = getTrackIndex(track);

    if (track == NULL)
        track = m_tracks.at(trackNum)->getTrack();

    ShowFunction *func = sf;
    if (func == NULL)
    {
        func = track->createShowFunction(scene->id());
        // A Scene has no intrinsic duration; give the clip a default so it's
        // visible/draggable and the ShowRunner doesn't stop it instantly
        // (stopTime == startTime when duration is 0).
        if (func->duration() == 0)
            func->setDuration(SceneItem::defaultDuration());
    }

    SceneItem *item = new SceneItem(scene, func);
    setItemCommonProperties(item, func, trackNum);
}

void MultiTrackView::addEFX(EFX *efx, Track *track, ShowFunction *sf)
{
    if (m_tracks.isEmpty())
        return;

    int trackNum = getTrackIndex(track);

    if (track == NULL)
        track = m_tracks.at(trackNum)->getTrack();

    ShowFunction *func = sf;
    if (func == NULL)
        func = track->createShowFunction(efx->id());

    EFXItem *item = new EFXItem(efx, func);
    setItemCommonProperties(item, func, trackNum);
}

void MultiTrackView::addVideo(Video *video, Track *track, ShowFunction *sf)
{
    if (m_tracks.isEmpty())
        return;

    int trackNum = getTrackIndex(track);

    if (track == NULL)
        track = m_tracks.at(trackNum)->getTrack();

    ShowFunction *func = sf;
    if (func == NULL)
        func = track->createShowFunction(video->id());

    VideoItem *item = new VideoItem(video, func);
    setItemCommonProperties(item, func, trackNum);
}

quint32 MultiTrackView::deleteSelectedItem()
{
    ShowItem *selectedItem = getSelectedItem();
    if (selectedItem != NULL)
    {
        QString msg = tr("Do you want to DELETE item:") + QString("\n\n") + selectedItem->functionName();

        // Ask for user's confirmation
        if (QMessageBox::question(this, tr("Delete Functions"), msg,
                                  QMessageBox::Yes, QMessageBox::No)
                                  == QMessageBox::Yes)
        {
            quint32 fID = selectedItem->functionID();
            m_scene->removeItem(selectedItem);
            m_items.removeOne(selectedItem);
            return fID;
        }
        return Function::invalidId();
    }

    int trackIndex = 0;
    foreach (TrackItem *item, m_tracks)
    {
        if (item->isActive() == true)
        {
            Track *track = item->getTrack();
            quint32 trackID = track->id();
            QList <ShowFunction *> sfList = track->showFunctions();
            QString msg = tr("Do you want to DELETE track:") + QString("\n\n") + track->name();
            if (sfList.count() > 0)
            {
                msg += QString("\n\n") + tr("This operation will also DELETE:") + QString("\n\n");
                foreach (ShowItem *item, m_items)
                {
                    if (item->getTrackIndex() == trackIndex)
                        msg += item->functionName() + QString("\n");
                }
            }

            // Ask for user's confirmation
            if (QMessageBox::question(this, tr("Delete Track"), msg,
                                  QMessageBox::Yes, QMessageBox::No)
                                  == QMessageBox::Yes)
            {
                m_scene->removeItem(item);
                m_tracks.removeOne(item);
                return trackID;
            }
            return Function::invalidId();
        }
        trackIndex++;
    }

    return Function::invalidId();
}

void MultiTrackView::deleteShowItem(Track *track, ShowFunction *sf)
{
    for (int i = 0; i < m_items.count(); i++)
    {
        if (m_items.at(i)->showFunction() == sf)
        {
            m_scene->removeItem(m_items.at(i));
            break;
        }
    }

    track->removeShowFunction(sf);
}

void MultiTrackView::moveCursor(quint32 timePos)
{
    // Park at the show end: past the configured/content end the cursor stops at
    // the end handle instead of scrolling off into empty timeline. The engine
    // still tracks the real timecode; the footer chip shows any overage.
    const quint32 endMs = effectiveEndMs();
    const bool pastEnd = (endMs > 0 && timePos > endMs);
    const quint32 shown = pastEnd ? endMs : timePos;
    int newPos = getPositionFromTime(shown);
    m_cursor->setPos(newPos, 0);
    m_cursor->setTime(shown);
    m_cursor->setPastEnd(pastEnd);
    // Page the view only when the playhead leaves the visible area: one jump
    // (cursor lands ~10% from the left with room ahead), then it traverses the
    // page. Scrolling every frame caused the choppy follow-scroll.
    QRectF vis = mapToScene(viewport()->rect()).boundingRect();
    if (newPos < vis.left() || newPos > vis.right())
        centerOn(newPos + (vis.width() * 0.4), vis.center().y());
}

void MultiTrackView::setPlayheadTarget(quint32 timePos)
{
    // Interpolate on the UI: the source is often only ~12Hz (MTC follow with the
    // show not running) or bursty (50Hz cross-thread), so a local 60Hz animator
    // eases the cursor toward the latest target for smooth motion. The grid is
    // cached, so each frame's repaint is cheap enough to sustain 60Hz.
    m_playheadTarget = timePos;
    m_playheadIdleFrames = 0;
    if (m_playheadTimer->isActive() == false)
    {
        m_playheadDisplay = timePos; // start locked (no kickoff lag)
        m_playheadTimer->start();
    }
}

void MultiTrackView::stopPlayhead()
{
    if (m_playheadTimer->isActive())
        m_playheadTimer->stop();
}

void MultiTrackView::slotAnimatePlayhead()
{
    double diff = double(m_playheadTarget) - m_playheadDisplay;
    if (qAbs(diff) < 0.5)
    {
        // Caught up: idle briefly, then stop so we don't spin at 60Hz. The next
        // setPlayheadTarget restarts us.
        if (++m_playheadIdleFrames > 45)
            m_playheadTimer->stop();
        return;
    }
    m_playheadIdleFrames = 0;

    if (qAbs(diff) > 2000.0)
        m_playheadDisplay = m_playheadTarget; // seek/locate — snap
    else
        m_playheadDisplay += diff * 0.5;      // snappy ease (~2-frame catch-up)

    moveCursor(quint32(m_playheadDisplay + 0.5));
}

void MultiTrackView::rewindCursor()
{
    stopPlayhead();
    m_playheadDisplay = 0;
    m_playheadTarget = 0;
    m_cursor->setPos(TRACK_WIDTH, 0);
    m_cursor->setTime(0);
}

void MultiTrackView::activateTrack(Track *track)
{
    foreach (TrackItem *item, m_tracks)
    {
        if (item->getTrack()->id() == track->id())
            item->setActive(true);
        else
            item->setActive(false);
    }
}

static bool compareItemStart(ShowItem *a, ShowItem *b)
{
    return a->getStartTime() < b->getStartTime();
}

void MultiTrackView::resolveTrackCollisions(int trackIndex, ShowItem *anchor)
{
    if (anchor == NULL)
        return;

    // Every other item on the same track, in start-time order.
    QList<ShowItem *> others;
    foreach (ShowItem *it, m_items)
    {
        if (it != anchor && it->getTrackIndex() == trackIndex)
            others.append(it);
    }
    std::sort(others.begin(), others.end(), compareItemStart);

    quint32 aStart = anchor->getStartTime();
    quint32 aDur = anchor->getDuration();

    // 1) Butt the anchor after any earlier item it would overlap.
    quint32 newStart = aStart;
    foreach (ShowItem *o, others)
    {
        if (o->getStartTime() <= aStart)
        {
            quint32 oEnd = o->getStartTime() + o->getDuration();
            if (oEnd > newStart)
                newStart = oEnd;
        }
    }
    if (newStart != aStart)
    {
        anchor->setStartTime(newStart);
        anchor->setPos(getPositionFromTime(newStart) + 2, anchor->y());
    }

    // 2) Ripple later items to the right so nothing overlaps.
    quint32 cursor = newStart + aDur;
    foreach (ShowItem *o, others)
    {
        if (o->getStartTime() < newStart)
            continue; // earlier item, already clear of the anchor
        if (o->getStartTime() < cursor)
        {
            o->setStartTime(cursor);
            o->setPos(getPositionFromTime(cursor) + 2, o->y());
        }
        cursor = o->getStartTime() + o->getDuration();
    }

    m_scene->update();
    updateViewSize();
}

void MultiTrackView::resolveCollisions(Track *track, ShowFunction *sf)
{
    ShowItem *anchor = NULL;
    foreach (ShowItem *it, m_items)
    {
        if (it->showFunction() == sf)
        {
            anchor = it;
            break;
        }
    }
    if (anchor != NULL)
        resolveTrackCollisions(getTrackIndex(track), anchor);
}

ShowItem *MultiTrackView::getSelectedItem() const
{
    foreach (ShowItem *item, m_items)
        if (item->isSelected())
            return item;

    return NULL;
}

QList<ShowItem *> MultiTrackView::selectedItems() const
{
    QList<ShowItem *> sel;
    foreach (ShowItem *item, m_items)
        if (item->isSelected())
            sel.append(item);
    return sel;
}

quint32 MultiTrackView::getTimeFromCursor() const
{
    quint32 s_time = (double)(m_cursor->x() - TRACK_WIDTH) *
                     (m_header->getTimeScale() * 1000) /
                     (double)(m_header->getHalfSecondWidth() * 2);
    return s_time;
}

quint32 MultiTrackView::getTimeFromPosition(qreal pos) const
{
    return ((double)(pos - TRACK_WIDTH) *
            (double)(m_header->getTimeScale() * 1000) /
            (double)(m_header->getHalfSecondWidth() * 2));
}

quint32 MultiTrackView::snapTimeMs(quint32 timeMs) const
{
    if (m_snapToGrid == false || m_header == NULL)
        return timeMs;

    // ms per pixel at the current zoom (getTimeFromPosition is linear in x).
    const double msPerPx = double(getTimeFromPosition(TRACK_WIDTH + 200)) / 200.0;
    if (msPerPx <= 0.0)
        return timeMs;

    const quint32 thresh = quint32(8.0 * msPerPx);   // ~8 px snap zone
    quint32 best = timeMs;
    quint32 bestDist = thresh + 1;
    auto consider = [&](quint32 t)
    {
        quint32 d = (t > timeMs) ? (t - timeMs) : (timeMs - t);
        if (d <= thresh && d < bestDist) { bestDist = d; best = t; }
    };

    // Grid line
    const float stepPx = m_header->getTimeStep();
    if (stepPx >= 1.0f)
    {
        const double gridMs = stepPx * msPerPx;
        if (gridMs >= 1.0)
            consider(quint32(qRound64(timeMs / gridMs) * qint64(qRound(gridMs))));
    }
    // Section marker edges
    QMapIterator<quint32, ShowMarker> it(m_markers);
    while (it.hasNext())
    {
        it.next();
        consider(it.key());
        consider(it.value().end);
    }
    // Playhead
    consider(getTimeFromCursor());

    return best;
}

quint32 MultiTrackView::computeEndDragValue(const QPointF &sp) const
{
    quint32 t;
    if (m_endDragEdge)
    {
        // Off-screen chip: adjust RELATIVE to the drag distance so the pinned
        // chip doesn't snap the end to the viewport edge.
        const qint64 deltaMs = qint64(getTimeFromPosition(sp.x()))
                             - qint64(getTimeFromPosition(m_endDragAnchorX));
        const qint64 nt = qint64(m_endDragAnchorMs) + deltaMs;
        t = (nt < 0) ? 0 : quint32(nt);
    }
    else
    {
        t = (sp.x() > TRACK_WIDTH) ? getTimeFromPosition(sp.x()) : 0;
    }
    t = snapTimeMs(t);
    if (t < 250)
        t = 250;   // keep a sane minimum show length
    return t;
}

void MultiTrackView::ensureSceneWidthForDrag(quint32 ms)
{
    const qreal needed = getPositionFromTime(ms) + 1000;
    if (needed > m_scene->width())
        setViewSize(int(needed), int(m_scene->height()));
}

void MultiTrackView::updateDragAutoScroll(const QPoint &vpPos)
{
    const int margin = 40;
    const int w = viewport()->width();
    int dir = 0;
    if (vpPos.x() >= w - margin)
        dir = +1;                                    // near right edge → extend
    else if (vpPos.x() <= TRACK_WIDTH + margin && horizontalScrollBar()->value() > 0)
        dir = -1;                                    // near left edge → back off
    m_dragScrollDir = dir;
    m_dragScrollPos = vpPos;
    if (dir != 0)
    {
        if (m_dragScrollTimer->isActive() == false)
            m_dragScrollTimer->start();
    }
    else
    {
        m_dragScrollTimer->stop();
    }
}

void MultiTrackView::slotDragAutoScroll()
{
    if (m_endDrag == false || m_dragScrollDir == 0)
    {
        m_dragScrollTimer->stop();
        return;
    }

    QScrollBar *h = horizontalScrollBar();
    const int step = 14 * m_dragScrollDir;

    // Extending right: make sure the scene can scroll further before we ask.
    if (m_dragScrollDir > 0)
    {
        const qreal rightScene = mapToScene(viewport()->width() - 1, 0).x();
        if (rightScene + 2 * step + 400 > m_scene->width())
            setViewSize(int(rightScene + 2 * step + 1200), int(m_scene->height()));
    }

    h->setValue(h->value() + step);

    // Recompute the handle value from the (held) mouse point over the now-
    // scrolled scene, and grow the canvas to keep the handle reachable.
    m_endDragValue = computeEndDragValue(mapToScene(m_dragScrollPos));
    ensureSceneWidthForDrag(m_endDragValue);
    viewport()->update();
}

quint32 MultiTrackView::getPositionFromTime(quint32 time) const
{
    if (time == 0)
        return TRACK_WIDTH;
    quint32 xPos = ((double)time / 500) *
                    ((double)m_header->getHalfSecondWidth() /
                     (double)m_header->getTimeScale());
    return TRACK_WIDTH + xPos;
}

int MultiTrackView::getTrackIndex(Track *trk) const
{
    for (int idx = 0; idx < m_tracks.count(); idx++)
    {
        if ((trk == NULL && m_tracks.at(idx)->isActive()) ||
            (trk != NULL && trk == m_tracks.at(idx)->getTrack()))
                return idx;
    }

    return 0;
}

void MultiTrackView::setHeaderType(Show::TimeDivision type)
{
    m_header->setTimeDivisionType(type);
    if (viewport() != NULL)
        viewport()->update();
}

Show::TimeDivision MultiTrackView::getHeaderType() const
{
    return m_header->getTimeDivisionType();
}

void MultiTrackView::setBPMValue(int value)
{
    m_header->setBPMValue(value);
    if (viewport() != NULL)
        viewport()->update();
}

void MultiTrackView::setSnapToGrid(bool enable)
{
    m_snapToGrid = enable;
    if (enable == true)
        m_header->setHeight(m_scene->height());
    else
        m_header->setHeight(HEADER_HEIGHT);
}

void MultiTrackView::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawBackground(painter, rect);

    if (m_header == NULL)
        return;

    float step = m_header->getTimeStep();
    if (step < 1.0f)
        return;

    int hit = m_header->getTimeHit();
    const qreal top = qMax(rect.top(), qreal(TRACKS_TOP));
    const qreal bottom = rect.bottom();
    const qreal right = rect.right();

    // First division at/after the visible-left edge.
    int i = qMax(0, int((rect.left() - TRACK_WIDTH) / step));
    for (;; i++)
    {
        qreal x = TRACK_WIDTH + (i * step) + 1;
        if (x > right)
            break;
        if (x < TRACK_WIDTH)
            continue;
        bool major = (hit > 0) && (i % hit == 0);
        // Subtle light lines over the dark timeline; majors a touch brighter.
        painter->setPen(QPen(QColor(255, 255, 255, major ? 45 : 18), 0));
        painter->drawLine(QPointF(x, top), QPointF(x, bottom));
    }
}

void MultiTrackView::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_editable && event->mimeData()->hasFormat(SM_FUNCTION_MIME))
    {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
    else
        event->ignore();
}

void MultiTrackView::dragMoveEvent(QDragMoveEvent *event)
{
    if (m_editable && event->mimeData()->hasFormat(SM_FUNCTION_MIME))
    {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
    else
        event->ignore();
}

void MultiTrackView::dropEvent(QDropEvent *event)
{
    if (m_editable == false || !event->mimeData()->hasFormat(SM_FUNCTION_MIME))
    {
        event->ignore();
        return;
    }

    QPointF scenePos = mapToScene(event->pos());

    // A drop to the left of the tracks column is not on the timeline.
    if (scenePos.x() < TRACK_WIDTH)
    {
        event->ignore();
        return;
    }

    quint32 startTime = getTimeFromPosition(scenePos.x());

    // Resolve the track row under the drop (NULL => make a new track).
    Track *track = NULL;
    int row = int((scenePos.y() - TRACKS_TOP) / TRACK_HEIGHT);
    if (row >= 0 && row < m_tracks.count())
        track = m_tracks.at(row)->getTrack();

    // Don't drop onto a locked track.
    if (track != NULL && track->isLocked())
    {
        event->ignore();
        return;
    }

    QByteArray data = event->mimeData()->data(SM_FUNCTION_MIME);
    QDataStream stream(&data, QIODevice::ReadOnly);
    quint32 cascade = 0;
    while (!stream.atEnd())
    {
        quint32 fid;
        stream >> fid;
        // Cascade multiple dropped functions in time so they don't stack.
        emit functionDropped(fid, startTime + cascade, track);
        cascade += 2000;
    }

    event->setDropAction(Qt::CopyAction);
    event->accept();
}

void MultiTrackView::contextMenuEvent(QContextMenuEvent *event)
{
    // Let a ShowItem or TrackItem show its own context menu.
    QGraphicsItem *hitItem = itemAt(event->pos());
    if (dynamic_cast<ShowItem *>(hitItem) != NULL || dynamic_cast<TrackItem *>(hitItem) != NULL)
    {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    // Read-only timeline: no add/track menus.
    if (m_editable == false)
        return;

    QPointF scenePos = mapToScene(event->pos());

    // Right-click near the show END handle: fit-to-content (auto) or set an
    // exact length.
    {
        const qreal ex = getPositionFromTime(effectiveEndMs());
        if (qAbs(scenePos.x() - ex) <= 10.0 && scenePos.y() >= HEADER_HEIGHT)
        {
            QMenu emenu;
            QAction *fitAct = emenu.addAction(tr("Fit end to content (auto)"));
            QAction *setAct = emenu.addAction(tr("Set show length…"));
            QAction *smpteAct = emenu.addAction(tr("End at SMPTE…"));
            QAction *chosen = emenu.exec(event->globalPos());
            if (chosen == fitAct)
            {
                emit showLengthChangeRequested(0);
            }
            else if (chosen == setAct)
            {
                bool ok = false;
                const double cur = effectiveEndMs() / 1000.0;
                const double v = QInputDialog::getDouble(this, tr("Show length"),
                            tr("Length (seconds):"), cur, 0.25, 86400.0, 2, &ok);
                if (ok)
                    emit showLengthChangeRequested(quint32(v * 1000.0));
            }
            else if (chosen == smpteAct)
            {
                // Parse hh:mm:ss(:ff) → absolute SMPTE ms; the ShowManager offsets
                // it by the show's timecode start so the end lands on that timecode.
                bool ok = false;
                const QString txt = QInputDialog::getText(this, tr("End at SMPTE"),
                            tr("Timecode (hh:mm:ss or hh:mm:ss:ff):"),
                            QLineEdit::Normal, QStringLiteral("01:00:00:00"), &ok);
                if (ok)
                {
                    const QStringList p = txt.trimmed().split(':');
                    if (p.size() >= 3)
                    {
                        const int hh = p.value(0).toInt();
                        const int mm = p.value(1).toInt();
                        const int ss = p.value(2).toInt();
                        const int ff = (p.size() >= 4) ? p.value(3).toInt() : 0;
                        const quint32 smpteMs = quint32(((hh * 3600 + mm * 60 + ss) * 1000)
                                                        + qRound(ff * (1000.0 / 30.0)));
                        emit showEndAtSmpteRequested(smpteMs);
                    }
                }
            }
            return;
        }
    }

    if (scenePos.x() < TRACK_WIDTH)
    {
        // Blank part of the track-header column (no item here): offer to create a track.
        QMenu hmenu;
        QAction *newAct = hmenu.addAction(tr("New track"));
        if (hmenu.exec(event->globalPos()) == newAct)
            emit newTrackRequested();
        return;
    }

    quint32 startTime = getTimeFromPosition(scenePos.x());

    // Right-click in the marker lane (between the ruler and the tracks):
    // add / rename / delete a section marker.
    if (scenePos.y() < TRACKS_TOP)
    {
        quint32 hit = markerAt(scenePos.x());
        QMenu mmenu;
        QAction *addAct = mmenu.addAction(tr("Add marker here…"));
        QAction *renAct = NULL;
        QAction *colAct = NULL;
        QAction *cueAct = NULL;
        QAction *delAct = NULL;
        if (hit != UINT_MAX)
        {
            mmenu.addSeparator();
            renAct = mmenu.addAction(tr("Rename marker…"));
            colAct = mmenu.addAction(tr("Change colour…"));
            cueAct = mmenu.addAction(tr("Link manual cue list…"));
            delAct = mmenu.addAction(tr("Delete marker"));
        }
        QAction *chosen = mmenu.exec(event->globalPos());
        if (chosen == addAct)
            emit markerAddRequested(startTime);
        else if (chosen != NULL && chosen == renAct)
            emit markerEditRequested(hit);
        else if (chosen != NULL && chosen == colAct)
            emit markerColorRequested(hit);
        else if (chosen != NULL && chosen == cueAct)
            emit markerSetCueListRequested(hit);
        else if (chosen != NULL && chosen == delAct)
            emit markerDeleteRequested(hit);
        return;
    }

    Track *track = NULL;
    int row = int((scenePos.y() - TRACKS_TOP) / TRACK_HEIGHT);
    if (row >= 0 && row < m_tracks.count())
        track = m_tracks.at(row)->getTrack();

    QMenu menu;
    QAction *addAct = menu.addAction(tr("Add function here…"));
    if (menu.exec(event->globalPos()) == addAct)
        emit addAtRequested(startTime, track);
}

void MultiTrackView::setMarkers(const QMap<quint32, ShowMarker> &markers)
{
    m_markers = markers;
    if (viewport() != NULL)
        viewport()->update();
}

void MultiTrackView::setConfiguredLength(quint32 ms)
{
    m_configuredLength = ms;
    updateViewSize();   // grow the scene so a configured end past content is reachable
    if (viewport() != NULL)
        viewport()->update();
}

quint32 MultiTrackView::contentEndMs() const
{
    // End of the last placed item = the auto length. Derived from item geometry
    // so the auto handle tracks edits without the host re-pushing.
    qreal maxX = TRACK_WIDTH;
    foreach (ShowItem *item, m_items)
        maxX = qMax<qreal>(maxX, item->x() + item->getWidth());
    return getTimeFromPosition(maxX);
}

quint32 MultiTrackView::effectiveEndMs() const
{
    return (m_configuredLength > 0) ? m_configuredLength : contentEndMs();
}

quint32 MultiTrackView::markerAt(qreal sceneX) const
{
    // The marker whose [start, end] region (padded a little for point markers)
    // contains sceneX; nearest start wins on overlap.
    quint32 best = UINT_MAX;
    QMapIterator<quint32, ShowMarker> it(m_markers);
    while (it.hasNext())
    {
        it.next();
        qreal sx = getPositionFromTime(it.key());
        qreal ex = getPositionFromTime(it.value().end);
        if (sceneX >= sx - 4 && sceneX <= ex + 4)
            best = it.key();
    }
    return best;
}

void MultiTrackView::setEmptyMessage(const QString &msg)
{
    if (m_emptyMessage == msg)
        return;
    m_emptyMessage = msg;
    if (viewport() != NULL)
        viewport()->update();
}

void MultiTrackView::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);

    // Empty-canvas hint (no show defined / no tracks yet). Pinned to the
    // viewport centre so it's obvious there's nowhere to drop yet.
    if (m_emptyMessage.isEmpty() == false && m_tracks.isEmpty())
    {
        painter->save();
        painter->resetTransform();      // draw in viewport (device) coordinates
        const QRect vp = viewport()->rect();
        QFont f = painter->font();
        f.setPixelSize(18);
        f.setBold(true);
        painter->setFont(f);
        painter->setPen(QColor(150, 165, 180));
        painter->drawText(vp.adjusted(40, 0, -40, 0),
                          Qt::AlignCenter | Qt::TextWordWrap, m_emptyMessage);
        painter->restore();
    }

    QFont f = painter->font();
    f.setPixelSize(11);
    f.setBold(true);
    painter->setFont(f);

    const qreal laneTop = HEADER_HEIGHT;
    const qreal bottom = rect.bottom();

    // Row label ("MARKERS") pinned to the left header column. Only paint it
    // when the exposed region actually covers the header column and lane — so a
    // playhead repaint on the right doesn't redo it every frame.
    qreal leftX = mapToScene(0, 0).x();
    if (rect.left() <= leftX + TRACK_WIDTH && rect.bottom() >= laneTop
        && rect.top() <= laneTop + MARKER_LANE_HEIGHT)
    {
        painter->fillRect(QRectF(leftX, laneTop, TRACK_WIDTH, MARKER_LANE_HEIGHT),
                          QColor(48, 61, 72));
        painter->setPen(QPen(QColor(200, 200, 200), 1));
        painter->drawText(QRectF(leftX + 6, laneTop, TRACK_WIDTH - 8, MARKER_LANE_HEIGHT),
                          Qt::AlignVCenter | Qt::AlignLeft, tr("MARKERS"));
    }

    // Collect the markers to draw (the map is never mutated by dragging; we
    // just substitute the dragged marker's live geometry here).
    QMap<quint32, ShowMarker> toDraw = m_markers;
    if (m_markerDragMode != 0)
    {
        toDraw.remove(m_markerOrigStart);
        toDraw.insert(m_dragStart, ShowMarker(m_dragEnd, m_dragLabel, m_dragColor));
    }

    QMapIterator<quint32, ShowMarker> it(toDraw);
    while (it.hasNext())
    {
        it.next();
        qreal sx = getPositionFromTime(it.key());
        qreal ex = getPositionFromTime(it.value().end);
        if (ex < sx + 4)
            ex = sx + 4; // keep zero-length markers visible
        // Clamp to the content area (right of the FROZEN header column, which
        // floats at leftX..leftX+TRACK_WIDTH) so bands/lines never paint over it.
        if (ex < rect.left() || sx > rect.right() || ex < leftX + TRACK_WIDTH)
            continue;
        sx = qMax<qreal>(sx, leftX + TRACK_WIDTH);

        QColor col = it.value().color.isValid() ? it.value().color : QColor(224, 168, 32);

        // Region band in the lane + boundary lines through the tracks.
        painter->setPen(Qt::NoPen);
        painter->setBrush(col);
        painter->drawRect(QRectF(sx, laneTop + 1, ex - sx, MARKER_LANE_HEIGHT - 2));
        QColor line = col.lighter(130);
        line.setAlpha(120);
        painter->setPen(QPen(line, 1));
        painter->drawLine(QPointF(sx, TRACKS_TOP), QPointF(sx, bottom));
        painter->drawLine(QPointF(ex, TRACKS_TOP), QPointF(ex, bottom));
        QColor fill = col;
        fill.setAlpha(16);
        painter->fillRect(QRectF(sx, TRACKS_TOP, ex - sx, bottom - TRACKS_TOP), fill);

        // Label clipped to the band, in a contrasting colour. A link glyph marks
        // a section that has a manual GO cue list attached (the timecode↔manual
        // seam), so the run-of-show is visible at a glance.
        painter->setPen(QPen(col.lightnessF() > 0.6 ? Qt::black : Qt::white, 1));
        QRectF textRect(sx + 4, laneTop + 1, (ex - sx) - 6, MARKER_LANE_HEIGHT - 2);
        const bool linked = (it.value().cueListId != Function::invalidId());
        const QString lbl = linked ? (QString("\xE2\x8F\xB5 ") + it.value().label)
                                   : it.value().label;
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, lbl);
    }

    // --- Show END handle (Logic-style): a draggable length marker with past-end
    // shading and a warning zone over any content clamped beyond it. ---
    {
        const bool autoLen = (m_configuredLength == 0);
        const quint32 endMs = m_endDrag ? m_endDragValue : effectiveEndMs();
        const quint32 contentMs = contentEndMs();
        const qreal ex = getPositionFromTime(endMs);
        const qreal clampLeft = leftX + TRACK_WIDTH;
        const QColor endCol = autoLen ? QColor(90, 190, 235) : QColor(235, 70, 70);
        // Visible viewport in scene coords: the on-screen handle and the two
        // off-screen edge chips partition the x-axis at [clampLeft, vis.right()],
        // so exactly ONE of them draws (no double "END m:ss" while dragging).
        const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
        const bool endOnScreen = (ex >= clampLeft + 1) && (ex <= vis.right() - 1);

        // Content authored PAST the end (clamped, not played): red warning hatch.
        if (contentMs > endMs)
        {
            const qreal cx = getPositionFromTime(contentMs);
            const qreal a = qMax<qreal>(ex, clampLeft);
            if (cx > a)
                painter->fillRect(QRectF(a, TRACKS_TOP, cx - a, bottom - TRACKS_TOP),
                                  QColor(200, 60, 60, 45));
        }

        // Past-end DEAD ZONE from the handle to the right edge: darken it AND lay
        // diagonal hatch stripes over it (film-leader style). The stripes are the
        // key "this is the end of the track" cue — a plain line reads as a
        // playhead (track on both sides); a hatched void past it does not.
        if (rect.right() > ex)
        {
            const qreal a = qMax<qreal>(ex, clampLeft);
            const QRectF dead(a, TRACKS_TOP, rect.right() - a, bottom - TRACKS_TOP);
            painter->fillRect(dead, QColor(0, 0, 0, 55));
            QColor hatch = endCol;
            hatch.setAlpha(40);
            painter->fillRect(dead, QBrush(hatch, Qt::FDiagPattern));
        }

        const quint32 sec = endMs / 1000;
        const QString etxt = QString("END %1:%2")
                .arg(sec / 60, 2, 10, QChar('0')).arg(sec % 60, 2, 10, QChar('0'));

        // The end line + grip flag + length label. The FULL-HEIGHT line is the
        // element that is always visible — it lives in the track area, which is
        // exposed on every repaint (the header rows can be scrolled off / clipped
        // by partial updates). Draw it thick and saturated so it never reads as
        // just another grid line. The flag + label ride on top for the grab area.
        // The label/flag straddle the line, so allow the block to draw whenever
        // the line's column is anywhere near the exposed strip.
        if (endOnScreen && ex + 120 >= rect.left() && ex <= rect.right() + 120)
        {
            // Bold full-height boundary line, drawn on the CONTENT side so the
            // dead-zone hatch butts right up against it like a wall.
            painter->setPen(QPen(endCol, m_endDrag ? 3 : 2));
            painter->setBrush(Qt::NoBrush);
            painter->drawLine(QPointF(ex, HEADER_HEIGHT), QPointF(ex, bottom));

            // End-cap PENNANT in the marker lane, pointing back into the content
            // (left) — an end bookend, not a symmetric cursor flag. Grab ridges
            // read as draggable; it's wide enough to hit at any zoom.
            painter->setPen(Qt::NoPen);
            painter->setBrush(endCol);
            const qreal ft = HEADER_HEIGHT;
            const qreal fb = HEADER_HEIGHT + MARKER_LANE_HEIGHT;
            const qreal fmid = HEADER_HEIGHT + MARKER_LANE_HEIGHT / 2.0;
            QPolygonF flag;
            flag << QPointF(ex, ft) << QPointF(ex, fb)
                 << QPointF(ex - 30, fb) << QPointF(ex - 42, fmid) << QPointF(ex - 30, ft);
            painter->drawPolygon(flag);
            painter->setPen(QPen(QColor(0, 0, 0, 150), 1));
            for (int gx = -20; gx <= -8; gx += 6)
                painter->drawLine(QPointF(ex + gx, ft + 4), QPointF(ex + gx, fb - 4));

            // A small left-pointing tab at the top of the track band (content is
            // to the LEFT) — anchors the boundary even when the lane is scrolled
            // off, and points away from a playhead-style downward nub.
            painter->setPen(Qt::NoPen);
            painter->setBrush(endCol);
            QPolygonF tab;
            tab << QPointF(ex, TRACKS_TOP) << QPointF(ex - 9, TRACKS_TOP)
                << QPointF(ex, TRACKS_TOP + 9);
            painter->drawPolygon(tab);

            // Length label as a filled chip in the ruler strip ABOVE the marker
            // lane (in the lane it collided with a section marker's own label).
            QString elbl = etxt;
            if (autoLen)
                elbl += tr(" (auto)");
            const QFontMetrics fm(painter->font());
            const int tw = fm.horizontalAdvance(elbl) + 10;
            QRectF chip(ex - 1 - tw, HEADER_HEIGHT - 15, tw, 14);
            painter->setPen(Qt::NoPen);
            QColor chipBg = endCol; chipBg.setAlpha(230);
            painter->setBrush(chipBg);
            painter->drawRect(chip);
            painter->setPen(QPen(endCol.lightnessF() > 0.6 ? Qt::black : Qt::white, 1));
            painter->drawText(chip.adjusted(5, 0, -4, 0),
                              Qt::AlignVCenter | Qt::AlignRight, elbl);
        }

        // Off-screen: pin a bold labeled END indicator to the viewport edge so
        // the show end is never invisible on a long (or empty) show. Click it to
        // jump there. Drawn in the marker lane AND with a track-area arrow so it
        // survives partial repaints of either band. Exclusive with the on-screen
        // handle above (partitioned at clampLeft / vis.right()).
        auto drawEdge = [&](qreal x, bool pointsRight) {
            const QString t = pointsRight ? (etxt + QString::fromUtf8("  \xE2\x96\xB6"))
                                          : (QString::fromUtf8("\xE2\x97\x80  ") + etxt);
            painter->setPen(Qt::NoPen);
            painter->setBrush(endCol);
            painter->drawRect(QRectF(x, HEADER_HEIGHT, 96, MARKER_LANE_HEIGHT));
            // a matching tick down into the track band so it is caught even when
            // the marker lane row is not part of the exposed region.
            painter->drawRect(QRectF(pointsRight ? x + 90 : x, TRACKS_TOP, 6, 10));
            painter->setPen(QPen(endCol.lightnessF() > 0.6 ? Qt::black : Qt::white, 1));
            painter->drawText(QRectF(x + 4, HEADER_HEIGHT, 88, MARKER_LANE_HEIGHT),
                              Qt::AlignVCenter | Qt::AlignLeft, t);
        };
        if (ex > vis.right() - 1)
            drawEdge(vis.right() - 96, true);
        else if (ex < clampLeft + 1)
            drawEdge(clampLeft, false);
    }
}

void MultiTrackView::mousePressEvent(QMouseEvent *e)
{
    QPointF sp = mapToScene(e->pos());

    // Grab the off-screen END edge indicator (in the marker lane). Checked
    // BEFORE marker drag: the chip is drawn on top of the marker lane, so a
    // press on it must act on the handle even if a marker underlies it. The
    // handle itself is off-screen, so the chip is the only way to reach it:
    // DRAG the chip to change the show length (relative to its current value —
    // the chip is pinned to the viewport edge, so mapping its absolute x to a
    // time would jump the length), or a plain CLICK (release without dragging)
    // scrolls the view to the handle so it can be grabbed directly.
    if (e->button() == Qt::LeftButton && m_editable &&
        sp.y() >= HEADER_HEIGHT && sp.y() < TRACKS_TOP)
    {
        const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
        const qreal exAbs = getPositionFromTime(effectiveEndMs());
        const qreal clampL = mapToScene(0, 0).x() + TRACK_WIDTH;
        if ((exAbs > vis.right() - 1 && sp.x() >= vis.right() - 96) ||
            (exAbs < clampL + 1 && sp.x() <= clampL + 96))
        {
            m_endDrag = true;
            m_endDragEdge = true;
            m_endDragValue = effectiveEndMs();
            m_endDragAnchorMs = effectiveEndMs();
            m_endDragAnchorX = sp.x();
            setCursor(Qt::SizeHorCursor);
            viewport()->update();
            e->accept();
            return;
        }
    }

    // Grab the ON-SCREEN show END handle (drag to set the show length). Checked
    // BEFORE marker drag: the handle line + grip flag are drawn on TOP of the
    // marker lane, so a press within the flag must grab the handle, not a marker
    // sitting behind it (e.g. a full-width "Full Show" section). Grabbable in the
    // top ruler/lane strip (where the flag lives) or along its line in an EMPTY
    // tracks area.
    if (e->button() == Qt::LeftButton && m_editable)
    {
        const qreal ex = getPositionFromTime(effectiveEndMs());
        if (qAbs(sp.x() - ex) <= 21.0 &&
            (sp.y() < TRACKS_TOP || dynamic_cast<ShowItem *>(itemAt(e->pos())) == NULL))
        {
            m_endDrag = true;
            m_endDragEdge = false;
            m_endDragValue = effectiveEndMs();
            setCursor(Qt::SizeHorCursor);
            viewport()->update();
            e->accept();
            return;
        }
    }

    // Start a marker move/resize when pressing in the marker lane on a region.
    if (e->button() == Qt::LeftButton && m_editable &&
        sp.x() >= TRACK_WIDTH && sp.y() >= HEADER_HEIGHT && sp.y() < TRACKS_TOP)
    {
        quint32 key = markerAt(sp.x());
        if (key != UINT_MAX)
        {
            ShowMarker m = m_markers.value(key);
            quint32 end = m.end;
            qreal sx = getPositionFromTime(key);
            qreal ex = getPositionFromTime(end);

            // Generous edge zones so stretching is easy to grab.
            const qreal edge = 10.0;
            if (end <= key || qAbs(sp.x() - ex) <= edge)
                m_markerDragMode = 3;                 // resize right (or extend a point)
            else if (qAbs(sp.x() - sx) <= edge)
                m_markerDragMode = 2;                 // resize left
            else
                m_markerDragMode = 1;                 // move

            m_markerOrigStart = key;
            m_dragStart = key;
            m_dragEnd = end;
            m_dragLabel = m.label;
            m_dragColor = m.color;
            m_markerGrabDx = sp.x() - sx;
            viewport()->update();
            e->accept();
            return;
        }
    }

    // Marquee (rubber-band) selection: press on empty space in the tracks area
    // (not on an item). Shift/Ctrl extends the current selection.
    if (e->button() == Qt::LeftButton && m_editable &&
        sp.x() > TRACK_WIDTH && sp.y() >= TRACKS_TOP &&
        dynamic_cast<ShowItem *>(itemAt(e->pos())) == NULL)
    {
        const bool extend = e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier);
        if (extend == false)
            m_scene->clearSelection();
        m_rubberActive = true;
        m_rubberOrigin = e->pos();
        if (m_rubberBand == NULL)
            m_rubberBand = new QRubberBand(QRubberBand::Rectangle, viewport());
        m_rubberBand->setGeometry(QRect(m_rubberOrigin, QSize()));
        m_rubberBand->show();
        e->accept();
        return;
    }

    QGraphicsView::mousePressEvent(e);
}

void MultiTrackView::mouseMoveEvent(QMouseEvent *e)
{
    if (m_rubberActive && m_rubberBand != NULL)
    {
        m_rubberBand->setGeometry(QRect(m_rubberOrigin, e->pos()).normalized());
        e->accept();
        return;
    }

    if (m_endDrag)
    {
        m_endDragValue = computeEndDragValue(mapToScene(e->pos()));
        ensureSceneWidthForDrag(m_endDragValue);
        // Auto-scroll while held near a horizontal edge, so the show can be
        // extended past the right of the viewport without repeated scrolling.
        updateDragAutoScroll(e->pos());
        viewport()->update();
        e->accept();
        return;
    }

    if (m_markerDragMode != 0)
    {
        QPointF sp = mapToScene(e->pos());
        const quint32 minLen = 250;

        if (m_markerDragMode == 1) // move (keep duration)
        {
            quint32 dur = (m_dragEnd > m_dragStart) ? m_dragEnd - m_dragStart : 0;
            qreal startX = sp.x() - m_markerGrabDx;
            quint32 ns = (startX > TRACK_WIDTH) ? getTimeFromPosition(startX) : 0;
            m_dragStart = ns;
            m_dragEnd = ns + dur;
        }
        else if (m_markerDragMode == 2) // resize left edge
        {
            quint32 t = (sp.x() > TRACK_WIDTH) ? getTimeFromPosition(sp.x()) : 0;
            if (t + minLen > m_dragEnd)
                t = (m_dragEnd > minLen) ? m_dragEnd - minLen : 0;
            m_dragStart = t;
        }
        else // resize right edge
        {
            quint32 t = (sp.x() > TRACK_WIDTH) ? getTimeFromPosition(sp.x()) : 0;
            if (t < m_dragStart + minLen)
                t = m_dragStart + minLen;
            m_dragEnd = t;
        }

        viewport()->update();
        e->accept();
        return;
    }

    // Hover feedback over the end handle so it's discoverable as draggable —
    // both the on-screen handle line AND the off-screen edge chip.
    if (m_editable && m_markerDragMode == 0 && m_rubberActive == false && m_endDrag == false)
    {
        const QPointF sp = mapToScene(e->pos());
        const qreal ex = getPositionFromTime(effectiveEndMs());
        const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
        const qreal clampL = mapToScene(0, 0).x() + TRACK_WIDTH;
        const bool onLine = qAbs(sp.x() - ex) <= 21.0 &&
            (sp.y() < TRACKS_TOP || dynamic_cast<ShowItem *>(itemAt(e->pos())) == NULL);
        const bool onEdgeChip = sp.y() >= HEADER_HEIGHT && sp.y() < TRACKS_TOP &&
            ((ex > vis.right() - 1 && sp.x() >= vis.right() - 96) ||
             (ex < clampL + 1 && sp.x() <= clampL + 96));
        if (onLine || onEdgeChip)
        {
            viewport()->setCursor(Qt::SizeHorCursor);
            setToolTip(onEdgeChip
                ? tr("Show end is off-screen — drag to set the length, or click to jump to it")
                : tr("Show end — drag to set the length (right-click: fit / set…)"));
        }
        else if (viewport()->cursor().shape() == Qt::SizeHorCursor)
        {
            viewport()->unsetCursor();
            setToolTip(QString());
        }
    }

    QGraphicsView::mouseMoveEvent(e);
}

void MultiTrackView::mouseReleaseEvent(QMouseEvent * e)
{
    // Commit a marquee selection.
    if (m_rubberActive)
    {
        m_rubberActive = false;
        if (m_rubberBand != NULL)
        {
            const QRect bandVp = m_rubberBand->geometry();
            m_rubberBand->hide();
            // Select every ShowItem whose viewport rect intersects the marquee.
            foreach (ShowItem *item, m_items)
            {
                QRect itemVp = mapFromScene(item->sceneBoundingRect()).boundingRect();
                if (bandVp.intersects(itemVp))
                    item->setSelected(true);
            }
        }
        emit viewClicked(e);   // refresh toolbar enable-state
        e->accept();
        return;
    }

    // Commit an end-handle drag → set the show's configured length.
    if (m_endDrag)
    {
        const bool wasEdge = m_endDragEdge;
        m_endDrag = false;
        m_endDragEdge = false;
        m_dragScrollDir = 0;
        m_dragScrollTimer->stop();
        setCursor(Qt::ArrowCursor);
        // An off-screen-chip press that didn't actually move is a CLICK: scroll
        // the (off-screen) handle into view instead of committing a length that
        // equals the current one.
        if (wasEdge && m_endDragValue == m_endDragAnchorMs)
        {
            centerOn(getPositionFromTime(effectiveEndMs()), mapToScene(e->pos()).y());
            viewport()->update();
            e->accept();
            return;
        }
        emit showLengthChangeRequested(m_endDragValue);
        e->accept();
        return;
    }

    // Commit a marker drag.
    if (m_markerDragMode != 0)
    {
        m_markerDragMode = 0;
        emit markerMovedRequested(m_markerOrigStart, m_dragStart, m_dragEnd,
                                  m_dragLabel, m_dragColor);
        m_markerOrigStart = UINT_MAX;
        QGraphicsView::mouseReleaseEvent(e);
        return;
    }

    if (getSelectedItem() == NULL)
    {
        // Don't handle positions at the left of QLC+ window
        if (mapToScene(e->pos()).x() < 0)
            return;
        quint32 xpos = mapToScene(e->pos()).x();
        // A click in the marker lane shouldn't move the playhead.
        if (xpos > TRACK_WIDTH && mapToScene(e->pos()).y() >= TRACKS_TOP)
        {
            m_cursor->setPos(xpos, 0);
            m_cursor->setTime(getTimeFromCursor());
            emit timeChanged(m_cursor->getTime());
        }
        emit viewClicked(e);
    }

    QGraphicsView::mouseReleaseEvent(e);
    //qDebug() << Q_FUNC_INFO << "View clicked at pos: " << e->pos().x() << e->pos().y();
}

void MultiTrackView::mouseDoubleClickEvent(QMouseEvent *e)
{
    QPointF sp = mapToScene(e->pos());
    // Double-click a marker in the lane => inline rename.
    if (sp.x() >= TRACK_WIDTH && sp.y() >= HEADER_HEIGHT && sp.y() < TRACKS_TOP)
    {
        quint32 key = markerAt(sp.x());
        if (key != UINT_MAX)
        {
            startMarkerEdit(key);
            e->accept();
            return;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(e);
}

void MultiTrackView::startMarkerEdit(quint32 key)
{
    if (m_markerEditProxy != NULL || m_markers.contains(key) == false)
        return;

    m_markerEditKey = key;
    QLineEdit *edit = new QLineEdit(m_markers.value(key).label);
    edit->selectAll();
    m_markerEditProxy = m_scene->addWidget(edit);
    m_markerEditProxy->setZValue(1500);
    m_markerEditProxy->setPos(getPositionFromTime(key) + 3, HEADER_HEIGHT);
    edit->setFixedWidth(140);
    edit->setFixedHeight(MARKER_LANE_HEIGHT);
    edit->setFocus();
    connect(edit, SIGNAL(editingFinished()), this, SLOT(slotMarkerEditCommitted()));
}

void MultiTrackView::slotMarkerEditCommitted()
{
    if (m_markerEditProxy == NULL)
        return;

    QLineEdit *edit = qobject_cast<QLineEdit *>(m_markerEditProxy->widget());
    if (edit != NULL)
        emit markerRelabelRequested(m_markerEditKey, edit->text().trimmed());

    // Tear down (guard against editingFinished firing twice).
    QGraphicsProxyWidget *proxy = m_markerEditProxy;
    m_markerEditProxy = NULL;
    m_markerEditKey = UINT_MAX;
    m_scene->removeItem(proxy);
    proxy->deleteLater();
    viewport()->update();
}

void MultiTrackView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        int zoomValue = m_timeSlider->value();
        if (event->pixelDelta().y() > 0)
            zoomValue++;
        else
            zoomValue--;

        if (zoomValue >= m_timeSlider->minimum() && zoomValue <= m_timeSlider->maximum())
            m_timeSlider->setValue(zoomValue);
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void MultiTrackView::slotHeaderClicked(QGraphicsSceneMouseEvent *event)
{
    m_cursor->setPos(TRACK_WIDTH + event->pos().toPoint().x(), 0);
    m_cursor->setTime(getTimeFromCursor());
    qDebug() << Q_FUNC_INFO << "Cursor moved to time:" << m_cursor->getTime();
    emit timeChanged(m_cursor->getTime());
}

void MultiTrackView::slotTimeScaleChanged(int val)
{
    //int oldScale = m_header->getTimeScale();
    m_header->setTimeScale(val);

    foreach (ShowItem *item, m_items)
    {
        quint32 newXpos = getPositionFromTime(item->getStartTime());
        item->setPos(newXpos + 2, item->y());
        item->setTimeScale(val);
    }

    int newCursorPos = getPositionFromTime(m_cursor->getTime());
    m_cursor->setPos(newCursorPos + 2, m_cursor->y());
    updateViewSize();
    // The division grid is drawn in drawBackground(); repaint it after a zoom.
    if (viewport() != NULL)
        viewport()->update();
}

void MultiTrackView::slotTrackClicked(TrackItem *track)
{
    foreach (TrackItem *item, m_tracks)
    {
        if (item == track)
            item->setActive(true);
        else
            item->setActive(false);
    }
    emit trackClicked(track->getTrack());
}

void MultiTrackView::slotTrackDoubleClicked(TrackItem *track)
{
    emit trackDoubleClicked(track->getTrack());
}

void MultiTrackView::slotTrackSoloFlagChanged(TrackItem* track, bool solo)
{
    // Model-backed solo (non-destructive): the runner silences any track that is
    // neither soloed nor solo-safe — we no longer bake mute into the other
    // tracks' saved state. Solos are additive (several may be soloed at once).
    Track *trk = (track != NULL) ? track->getTrack() : NULL;
    if (trk != NULL)
        trk->setSolo(solo);
    emit trackModified();
}

void MultiTrackView::slotTrackMuteFlagChanged(TrackItem* item, bool mute)
{
    Track *trk = item->getTrack();
    if (trk != NULL)
        trk->setMute(mute);
}

void MultiTrackView::slotViewScrolled(int)
{
    pinLeftColumn();
}

void MultiTrackView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    // Keep the track-header column (and the zoom-slider corner + vertical
    // divider) frozen at the left edge as the timeline scrolls horizontally.
    pinLeftColumn();
}

void MultiTrackView::pinLeftColumn()
{
    // Scene-x at the viewport's left edge (identity transform → scroll offset).
    const qreal x0 = mapToScene(QPoint(0, 0)).x();

    foreach (TrackItem *t, m_tracks)
    {
        if (t == NULL)
            continue;
        t->setX(x0);              // freeze horizontally; vertical scroll unaffected
        if (t->zValue() < TRACK_HEADER_Z)
            t->setZValue(TRACK_HEADER_Z);
    }
    if (m_vdivider != NULL)
    {
        m_vdivider->setX(x0);     // divider rect is drawn at local TRACK_WIDTH-3
        m_vdivider->setZValue(TRACK_HEADER_Z);
    }
    if (m_sliderProxy != NULL)
        m_sliderProxy->setX(x0);
}

void MultiTrackView::slotItemMoved(QGraphicsSceneMouseEvent *event, ShowItem *item)
{
    qDebug() << Q_FUNC_INFO << "event - <" << event->pos().toPoint().x() << "> - <" << event->pos().toPoint().y() << ">";
    // align to the appropriate track
    bool moved = true;
    quint32 s_time = 0;
    int oldTrackNum = item->getTrackIndex();
    int guessRow = qRound((item->y() - double(TRACKS_TOP + 1)) / double(TRACK_HEIGHT));

    // Dropped below the last track: ask the manager to make a new track for it.
    if (m_tracks.count() > 0 && guessRow >= m_tracks.count())
    {
        qreal xpos = qMax<qreal>(TRACK_WIDTH + 2, item->x());
        item->setStartTime(getTimeFromPosition(xpos - 2));
        emit itemDroppedBelowTracks(item);
        return;
    }

    // Destination track resolved from the drop Y position — this is what lets
    // an item be dragged from one track (row) to another.
    int newTrackNum = oldTrackNum;
    if (m_tracks.count() > 0)
        newTrackNum = qBound(0, guessRow, m_tracks.count() - 1);
    bool trackChanged = (newTrackNum != oldTrackNum);

    int ypos = TRACKS_TOP + 1 + (newTrackNum * TRACK_HEIGHT);
    int shift = qAbs(item->getDraggingPos().x() - item->x());

    if (item->x() < TRACK_WIDTH + 2)
    {
        item->setPos(TRACK_WIDTH + 2, ypos); // avoid moving an item too early...
    }
    // a tiny horizontal drag on the SAME track is treated as a click; but a
    // change of track is always a move even with no horizontal shift.
    else if (shift < 3 && trackChanged == false)
    {
        //qDebug() << "Drag too short (" << shift << "px) not allowed!";
        item->setPos(item->getDraggingPos());
        s_time = item->getStartTime();
        moved = false;
    }
    else if (m_snapToGrid == true)
    {
        float step = m_header->getTimeDivisionStep();
        float gridPos = ((int)(item->x() / step) * step);
        item->setPos(gridPos + 2, ypos);
        s_time = getTimeFromPosition(gridPos);
    }
    else
    {
        item->setPos(item->x(), ypos);
        s_time = getTimeFromPosition(item->x() - 2);
    }

    item->setStartTime(s_time);

    // Move the underlying ShowFunction to the destination track in the engine.
    if (trackChanged && oldTrackNum < m_tracks.count() && newTrackNum < m_tracks.count())
    {
        Track *oldTrk = m_tracks.at(oldTrackNum)->getTrack();
        Track *newTrk = m_tracks.at(newTrackNum)->getTrack();
        ShowFunction *sf = item->showFunction();
        if (oldTrk != NULL && newTrk != NULL && sf != NULL && oldTrk != newTrk)
        {
            oldTrk->removeShowFunction(sf, false);
            newTrk->addShowFunction(sf);
            item->setTrackIndex(newTrackNum);
        }
    }

    // DAW-style push: keep the track free of overlaps.
    if (moved)
        resolveTrackCollisions(newTrackNum, item);

    // Multi-select: apply the same track/time delta to every OTHER selected
    // item and commit each to the engine (Qt only reports the grabbed one).
    if (moved)
    {
        qint64 rowDelta = qint64(newTrackNum) - oldTrackNum;
        quint32 oldStart = getTimeFromPosition(
            qMax<qreal>(TRACK_WIDTH + 2, item->getDraggingPos().x()) - 2);
        qint64 timeDelta = qint64(s_time) - qint64(oldStart);

        foreach (ShowItem *o, m_items)
        {
            if (o == item || o->isSelected() == false)
                continue;

            int oldR = o->getTrackIndex();
            int newR = qBound(0, int(oldR + rowDelta),
                              m_tracks.count() > 0 ? m_tracks.count() - 1 : 0);
            qint64 nt = qint64(o->getStartTime()) + timeDelta;
            if (nt < 0)
                nt = 0;

            o->setStartTime(quint32(nt));
            o->setPos(getPositionFromTime(quint32(nt)) + 2,
                      TRACKS_TOP + 1 + (newR * TRACK_HEIGHT));

            if (newR != oldR && oldR < m_tracks.count() && newR < m_tracks.count())
            {
                Track *ot = m_tracks.at(oldR)->getTrack();
                Track *nt2 = m_tracks.at(newR)->getTrack();
                ShowFunction *sf = o->showFunction();
                if (ot != NULL && nt2 != NULL && sf != NULL && ot != nt2)
                {
                    ot->removeShowFunction(sf, false);
                    nt2->addShowFunction(sf);
                    o->setTrackIndex(newR);
                }
            }
            resolveTrackCollisions(newR, o);
        }
    }

    m_scene->update();
    emit showItemMoved(item, getTimeFromPosition(item->x() + event->pos().toPoint().x()), moved);
}

void MultiTrackView::slotItemResized(ShowItem *item, bool leftEdge)
{
    if (item == NULL)
        return;

    // Translate the item's new pixel geometry into start time + duration.
    quint32 startTime = getTimeFromPosition(item->x() - 2);
    quint32 endTime = getTimeFromPosition(item->x() - 2 + item->getWidth());
    if (endTime < startTime)
        endTime = startTime;
    quint32 duration = endTime - startTime;

    if (leftEdge)
    {
        item->setStartTime(startTime);
        // Snap the left edge to the computed start time.
        item->setPos(getPositionFromTime(startTime) + 2, item->y());
    }

    // Commit the new duration to the item/function (recomputes the width).
    item->setDuration(duration, false);

    // DAW-style push: a longer item may now overlap its neighbour.
    resolveTrackCollisions(item->getTrackIndex(), item);

    m_scene->update();
    updateViewSize();
    // Reuse the move path for selection + Doc setModified() bookkeeping.
    emit showItemMoved(item, startTime, true);
}

void MultiTrackView::slotAlignToCursor(ShowItem *item)
{
    item->setX(m_cursor->x());
    item->setStartTime(getTimeFromPosition(item->x()));
    m_scene->update();
}


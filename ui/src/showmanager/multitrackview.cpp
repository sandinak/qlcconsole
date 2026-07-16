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
    m_scene->addWidget(m_timeSlider);

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

    m_markerDragMode = 0;
    m_markerOrigStart = UINT_MAX;
    m_dragStart = 0;
    m_dragEnd = 0;
    m_markerGrabDx = 0;
    m_markerEditProxy = NULL;
    m_markerEditKey = UINT_MAX;
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
    int hDivNum = 6;
    if (m_tracks.count() > 5)
        hDivNum = m_tracks.count();
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

    if ((m_tracks.count() * TRACK_HEIGHT) + HEADER_HEIGHT > VIEW_DEFAULT_HEIGHT)
    {
        gHeight = (m_tracks.count() * TRACK_HEIGHT) + HEADER_HEIGHT;
        m_cursor->setHeight(gHeight);
    }

    if (gWidth > VIEW_DEFAULT_WIDTH || gHeight > VIEW_DEFAULT_HEIGHT)
        setViewSize(gWidth + 1000, gHeight);
}

void MultiTrackView::resetView()
{
    for (int t = 0; t < m_tracks.count(); t++)
        m_scene->removeItem(m_tracks.at(t));
    m_tracks.clear();

    for (int i = 0; i < m_items.count(); i++)
        m_scene->removeItem(m_items.at(i));
    m_items.clear();

    rewindCursor();
    this->horizontalScrollBar()->setSliderPosition(0);
    this->verticalScrollBar()->setSliderPosition(0);
    m_scene->update();
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
    m_scene->addItem(trackItem);
    m_tracks.append(trackItem);
    activateTrack(track);
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
    int newPos = getPositionFromTime(timePos);
    m_cursor->setPos(newPos, 0);
    m_cursor->setTime(timePos);
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
    // Let a ShowItem show its own (delete/lock/align) menu.
    if (dynamic_cast<ShowItem *>(itemAt(event->pos())) != NULL)
    {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    // Read-only timeline: no add/track menus.
    if (m_editable == false)
        return;

    QPointF scenePos = mapToScene(event->pos());
    if (scenePos.x() < TRACK_WIDTH)
    {
        // Blank part of the track-header column: offer to create a track.
        // (TrackItems handle their own right-click menu.)
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
        QAction *delAct = NULL;
        if (hit != UINT_MAX)
        {
            mmenu.addSeparator();
            renAct = mmenu.addAction(tr("Rename marker…"));
            colAct = mmenu.addAction(tr("Change colour…"));
            delAct = mmenu.addAction(tr("Delete marker"));
        }
        QAction *chosen = mmenu.exec(event->globalPos());
        if (chosen == addAct)
            emit markerAddRequested(startTime);
        else if (chosen != NULL && chosen == renAct)
            emit markerEditRequested(hit);
        else if (chosen != NULL && chosen == colAct)
            emit markerColorRequested(hit);
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
        if (ex < rect.left() || sx > rect.right() || ex < TRACK_WIDTH)
            continue;
        sx = qMax<qreal>(sx, TRACK_WIDTH);

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

        // Label clipped to the band, in a contrasting colour.
        painter->setPen(QPen(col.lightnessF() > 0.6 ? Qt::black : Qt::white, 1));
        QRectF textRect(sx + 4, laneTop + 1, (ex - sx) - 6, MARKER_LANE_HEIGHT - 2);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, it.value().label);
    }
}

void MultiTrackView::mousePressEvent(QMouseEvent *e)
{
    QPointF sp = mapToScene(e->pos());

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

    QGraphicsView::mousePressEvent(e);
}

void MultiTrackView::mouseMoveEvent(QMouseEvent *e)
{
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

    QGraphicsView::mouseMoveEvent(e);
}

void MultiTrackView::mouseReleaseEvent(QMouseEvent * e)
{
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
    foreach (TrackItem *item, m_tracks)
    {
        if (item != track)
            item->setFlags(false, solo);
        Track *trk = item->getTrack();
        if (trk != NULL)
            trk->setMute(item->isMute());
    }
}

void MultiTrackView::slotTrackMuteFlagChanged(TrackItem* item, bool mute)
{
    Track *trk = item->getTrack();
    if (trk != NULL)
        trk->setMute(mute);
}

void MultiTrackView::slotViewScrolled(int)
{
    //qDebug() << Q_FUNC_INFO << "Percentage: " << value;
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


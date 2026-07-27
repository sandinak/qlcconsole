/*
  Q Light Controller Plus
  headeritems.h

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

#ifndef HEADERITEM_H
#define HEADERITEM_H

#include <QGraphicsItem>
#include <QObject>
#include <QAction>
#include <QFont>

#include "show.h"

#define HEADER_HEIGHT       35
#define HALF_SECOND_WIDTH   25

/** Height of the marker/label lane drawn between the time ruler and the
 *  tracks, and the resulting Y where tracks begin. */
#define MARKER_LANE_HEIGHT  18
#define TRACKS_TOP          (HEADER_HEIGHT + MARKER_LANE_HEIGHT)

/** @addtogroup ui_functions
 * @{
 */

/**
 *
 * Show Manager Header class. Clickable time line header
 *
 */

class ShowHeaderItem final : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    ShowHeaderItem(int width);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setTimeScale(int val);
    int getTimeScale() const;

    void setTimeDivisionType(Show::TimeDivision type);
    Show::TimeDivision getTimeDivisionType() const;
    void setBPMValue(int value);

    int getHalfSecondWidth() const;
    float getTimeDivisionStep() const;

    /** Pixel distance between adjacent time-division bars (kept current for
     *  the timeline grid, not only computed at paint time). */
    float getTimeStep() const;

    /** How many division bars make up one labelled (major) bar. */
    int getTimeHit() const;

    void setWidth(int);
    void setHeight(int);

private:
    /** Recompute m_timeStep from the current type / BPM / scale. */
    void updateTimeStep();

signals:
    void itemClicked(QGraphicsSceneMouseEvent *);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    /** Total width of the item */
    int m_width;
    /** Total height of the item */
    int m_height;
    /** Distance in pixels between the time division bars */
    float m_timeStep;
    /** Divisor of the time division hit bar (the highest bar) */
    char m_timeHit;
    /** Scale of the time division */
    int m_timeScale;
    /** When BPM mode is active, this holds the number of BPM to display */
    int m_BPMValue;
    /** The type of time division */
    Show::TimeDivision m_type;
};

/**
 *
 * Show Manager Cursor class. Cursor which marks the time position in a scene
 *
 */
class ShowCursorItem final : public QGraphicsItem
{
public:
    ShowCursorItem(int h);

    void setHeight(int height);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setTime(quint32 t);
    quint32 getTime() const;

    /** Tint the cursor when it is parked past the show end (reads as "complete"
     *  rather than a live playhead). */
    void setPastEnd(bool pastEnd);
private:
    int m_height;
    quint32 m_time;
    bool m_pastEnd = false;
};

/** @} */

#endif

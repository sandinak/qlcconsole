/*
  Q Light Controller Plus
  monitorruler.h

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

#ifndef MONITORRULER_H
#define MONITORRULER_H

#include <QWidget>

class MonitorGraphicsView;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

/**
 * A thin graduated ruler drawn along the top or left edge of the 2D monitor.
 * It is a "dumb" painter: all the coordinate math (zoom, pan, POV, origin)
 * lives in MonitorGraphicsView, which this widget queries for tick positions
 * (already mapped to viewport pixels) on every paint. The two rulers share the
 * view's viewport pixel axis, so a tick at viewport-x maps 1:1 to the canvas.
 */
class MonitorRuler final : public QWidget
{
    Q_OBJECT

public:
    enum Orientation { Horizontal, Vertical };

    MonitorRuler(MonitorGraphicsView *view, Orientation orient, QWidget *parent = nullptr);

    /** Thickness (height for horizontal, width for vertical) in pixels. */
    static int thickness() { return 20; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    /** Move the live cursor marker to a viewport pixel along this ruler's axis
     *  (x for horizontal, y for vertical). Pass -1 to hide it. */
    void setCursorPixel(int px);

    /** Highlight the extent of the current selection/dragged item as a band
     *  between two viewport pixels along this ruler's axis. Pass (-1,-1) to
     *  clear. Lets you read where the moving item sits on the ruler. */
    void setItemRange(int pxA, int pxB);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    MonitorGraphicsView *m_view;
    Orientation m_orient;
    int m_cursorPixel = -1;
    int m_rangeA = -1;
    int m_rangeB = -1;
};

/** @} */

#endif // MONITORRULER_H

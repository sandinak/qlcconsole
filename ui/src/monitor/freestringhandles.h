/*
  Q Light Controller Plus
  freestringhandles.h

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

#ifndef FREESTRINGHANDLES_H
#define FREESTRINGHANDLES_H

#include <QGraphicsItem>
#include <QVector>
#include <QPointF>

/** @addtogroup ui_monitor
 * @{
 */

/**
 * The anchors of the selected Free string (PixelPath::FreeString) on the
 * plot: the line it runs along, a handle at each anchor numbered in the
 * order the string runs, a pinned LED's number beside its anchor, and an
 * arrow on each span the way its data runs (turned round where the span is
 * reversed). It is drawn in scene coordinates and takes no clicks itself:
 * the plot works out a press on a handle (handleAt()) and drags, places or
 * offers its menu (MonitorGraphicsView).
 */
class FreeStringHandles final : public QGraphicsItem
{
public:
    explicit FreeStringHandles(quint32 fid);

    quint32 fixtureID() const { return m_fid; }

    /** The anchors on the plot, first first: where each is (scene), the LED
     *  it pins (0 none) and whether the span from it is reversed. @p own
     *  false: the string has no anchors of its own yet (drawn in front of
     *  its fixture), so the handles are drawn hollow. @p radius: a handle's
     *  size in scene units (a constant size on screen). */
    void setAnchors(const QVector<QPointF> &points, const QVector<int> &pins,
                    const QVector<bool> &reverse, bool own, qreal radius);

    int count() const { return m_points.size(); }
    QPointF handlePos(int index) const { return m_points.value(index); }
    bool ownAnchors() const { return m_own; }
    /** The handle at @p scenePos (within its radius and a little), or -1. */
    int handleAt(const QPointF &scenePos) const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    quint32 m_fid;
    QVector<QPointF> m_points;
    QVector<int> m_pins;
    QVector<bool> m_reverse;
    bool m_own = false;
    qreal m_radius = 6.0;
    QRectF m_bounds;
};

/** @} */

#endif

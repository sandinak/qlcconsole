/*
  Q Light Controller Plus
  hangeritem.h

  Copyright (C) Branson Matheson

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

#ifndef HANGERITEM_H
#define HANGERITEM_H

#include <QGraphicsItem>
#include <QPointF>

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

/**
 * A hanger on the 2D plot: its arm out from the truss or tower it is
 * fastened to, its chain down, and the hook a fixture hangs from -- in
 * whichever view the plot is showing. It goes where its structure goes, so
 * it is never dragged; double-click edits it, right-click has Edit and
 * Delete (MonitorGraphicsView, as for any structure).
 */
class HangerItem final : public QGraphicsItem
{
public:
    /** @p fastened, @p armEnd, @p hook: where they are, in scene pixels. */
    HangerItem(quint32 id, const QPointF &fastened, const QPointF &armEnd,
               const QPointF &hook, QGraphicsItem *parent = nullptr);

    quint32 hangerId() const { return m_id; }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

private:
    quint32 m_id;
    QPointF m_fastened, m_armEnd, m_hook;
};

/** @} */

#endif

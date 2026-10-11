/*
  Q Light Controller Plus
  hangeritem.cpp

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

#include <QPainter>
#include <QPainterPathStroker>

#include "hangeritem.h"

namespace
{
const qreal kHookR = 5.0;      // the hook's ring, in screen-ish scene pixels
}

HangerItem::HangerItem(quint32 id, const QPointF &fastened, const QPointF &armEnd,
                       const QPointF &hook, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_id(id)
    , m_fastened(fastened)
    , m_armEnd(armEnd)
    , m_hook(hook)
{
    setFlags(ItemIsSelectable);
    setToolTip(QObject::tr("Hanger — double-click to edit"));
}

QRectF HangerItem::boundingRect() const
{
    QRectF r = QRectF(m_fastened, m_armEnd).normalized().united(QRectF(m_armEnd, m_hook).normalized());
    return r.adjusted(-kHookR - 3, -kHookR - 3, kHookR + 3, kHookR + 3);
}

QPainterPath HangerItem::shape() const
{
    QPainterPath p;
    p.moveTo(m_fastened);
    p.lineTo(m_armEnd);
    p.lineTo(m_hook);
    QPainterPathStroker stroke;
    stroke.setWidth(8.0);           // as easy to grab as it is to see
    QPainterPath out = stroke.createStroke(p);
    out.addEllipse(m_hook, kHookR + 2, kHookR + 2);
    return out;
}

void HangerItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QColor steel = isSelected() ? QColor(255, 200, 60) : QColor(170, 176, 186);
    QPen arm(steel, 2.0);
    arm.setCosmetic(true);
    painter->setPen(arm);
    painter->drawLine(m_fastened, m_armEnd);       // the arm
    QPen chain(steel, 1.0, Qt::DotLine);
    chain.setCosmetic(true);
    painter->setPen(chain);
    painter->drawLine(m_armEnd, m_hook);           // the chain (seen in elevation)
    painter->setPen(arm);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(m_hook, kHookR, kHookR);  // the hook
    painter->setBrush(steel);
    painter->drawEllipse(m_fastened, 2.0, 2.0);    // the clamp
}

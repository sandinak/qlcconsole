/*
  Q Light Controller Plus - qlcconsole
  zoneitem.cpp

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
#include <QPainterPath>
#include <QFontMetricsF>

#include "zoneitem.h"

ZoneItem::ZoneItem(Role role, quint32 id, const QPolygonF &outline, const QString &label,
                   QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_role(role)
    , m_id(id)
    , m_outline(outline)
    , m_label(label)
{
    setAcceptedMouseButtons(Qt::NoButton);
    if (role == Zone)
        setToolTip(QObject::tr("No-light zone '%1' -- double-click to edit").arg(label));
    else
        setToolTip(QObject::tr("Scan window of %1").arg(label));
}

bool ZoneItem::contains(const QPointF &scenePos) const
{
    return m_outline.containsPoint(scenePos, Qt::OddEvenFill);
}

QRectF ZoneItem::boundingRect() const
{
    return m_outline.boundingRect().adjusted(-3, -3, 3, 20);
}

QPainterPath ZoneItem::shape() const
{
    QPainterPath p;
    p.addPolygon(m_outline);
    return p;
}

void ZoneItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (m_role == Zone)
    {
        const QColor red(220, 40, 40);
        painter->setPen(QPen(red, 1.5, Qt::SolidLine));
        painter->setBrush(QBrush(QColor(220, 40, 40, 55), Qt::BDiagPattern));
        painter->drawPolygon(m_outline);
        painter->setBrush(QColor(220, 40, 40, 30));
        painter->drawPolygon(m_outline);
    }
    else
    {
        painter->setPen(QPen(QColor(240, 170, 20), 1.2, Qt::DashLine));
        painter->setBrush(QColor(240, 170, 20, 25));
        painter->drawPolygon(m_outline);
    }
    if (!m_label.isEmpty() && m_role == Zone)
    {
        painter->setPen(QColor(150, 20, 20));
        painter->drawText(m_outline.boundingRect().bottomLeft() + QPointF(2, 13), m_label);
    }
}

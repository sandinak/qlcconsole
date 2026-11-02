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
        setToolTip(QObject::tr("No-light zone '%1' -- drag its outline to move, double-click to edit").arg(label));
    else
        setToolTip(QObject::tr("Scan window of %1").arg(label));
}

bool ZoneItem::contains(const QPointF &scenePos) const
{
    return m_outline.containsPoint(scenePos, Qt::OddEvenFill);
}

QRectF ZoneItem::handleRect() const
{
    const QPointF c = m_outline.boundingRect().center();
    return QRectF(c.x() - 5.0, c.y() - 5.0, 10.0, 10.0);
}

bool ZoneItem::grabAt(const QPointF &scenePos, qreal tol) const
{
    if (m_role != Zone || m_outline.size() < 2)
        return false;
    if (handleRect().adjusted(-tol / 2, -tol / 2, tol / 2, tol / 2).contains(scenePos))
        return true;
    for (int i = 0; i < m_outline.size(); ++i)
    {
        const QPointF a = m_outline.at(i);
        const QPointF b = m_outline.at((i + 1) % m_outline.size());
        const QPointF ab = b - a;
        const qreal len2 = ab.x() * ab.x() + ab.y() * ab.y();
        qreal t = len2 > 0.0 ? ((scenePos.x() - a.x()) * ab.x() + (scenePos.y() - a.y()) * ab.y()) / len2 : 0.0;
        t = qBound<qreal>(0.0, t, 1.0);
        const QPointF d = scenePos - (a + ab * t);
        if (d.x() * d.x() + d.y() * d.y() <= tol * tol)
            return true;
    }
    return false;
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
    if (m_role == Zone)
    {
        painter->setPen(QPen(QColor(220, 40, 40), 1.0));
        painter->setBrush(QColor(255, 255, 255, 200));
        painter->drawRect(handleRect());
    }
    if (!m_label.isEmpty() && m_role == Zone)
    {
        painter->setPen(QColor(150, 20, 20));
        painter->drawText(m_outline.boundingRect().bottomLeft() + QPointF(2, 13), m_label);
    }
}

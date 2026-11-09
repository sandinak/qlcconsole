/*
  Q Light Controller Plus
  freestringhandles.cpp

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

#include <QCoreApplication>
#include <QPainterPath>
#include <QPainter>
#include <QLineF>
#include <QtMath>
#include <utility>

#include "freestringhandles.h"
#include "uprighttext.h"

FreeStringHandles::FreeStringHandles(quint32 fid)
    : m_fid(fid)
{
    // Over every fixture and structure; clicks are the plot's to work out.
    setZValue(9500);
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
}

void FreeStringHandles::setAnchors(const QVector<QPointF> &points, const QVector<int> &pins,
                                   const QVector<bool> &reverse, bool own, qreal radius)
{
    prepareGeometryChange();
    m_points = points;
    m_pins = pins;
    m_reverse = reverse;
    m_own = own;
    m_radius = qMax(1.0, radius);
    QRectF r;
    for (const QPointF &p : std::as_const(m_points))
        r |= QRectF(p, QSizeF(0, 0));
    // Room for the handles, their numbers and a pinned LED's label.
    m_bounds = r.adjusted(-m_radius * 6, -m_radius * 4, m_radius * 10, m_radius * 4);
    update();
}

int FreeStringHandles::handleAt(const QPointF &scenePos) const
{
    int best = -1;
    qreal bestD = m_radius * 1.6;
    for (int i = 0; i < m_points.size(); ++i)
    {
        const qreal d = QLineF(m_points.at(i), scenePos).length();
        if (d <= bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return best;
}

QRectF FreeStringHandles::boundingRect() const
{
    return m_bounds;
}

void FreeStringHandles::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (m_points.isEmpty())
        return;
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QColor line(255, 200, 90);
    const qreal w = m_radius / 3.0;

    // The string's line, anchor to anchor, faint so its LEDs show through:
    // dashed until it has anchors of its own.
    QColor faint = line;
    faint.setAlpha(110);
    QPen pen(faint, w * 0.6);
    pen.setCosmetic(false);
    if (!m_own)
        pen.setStyle(Qt::DashLine);
    painter->setPen(pen);
    for (int i = 1; i < m_points.size(); ++i)
        painter->drawLine(m_points.at(i - 1), m_points.at(i));

    // Each span's way: an arrow at its middle, turned round where it is reversed.
    painter->setBrush(faint);
    painter->setPen(Qt::NoPen);
    for (int i = 1; i < m_points.size(); ++i)
    {
        QLineF seg(m_points.at(i - 1), m_points.at(i));
        if (seg.length() < m_radius * 5)
            continue;
        if (m_reverse.value(i - 1))
            seg = QLineF(seg.p2(), seg.p1());
        const QPointF mid = seg.pointAt(0.5);
        const QPointF u = (seg.p2() - seg.p1()) / seg.length();
        const QPointF n(-u.y(), u.x());
        QPolygonF arrow;
        arrow << mid + u * (m_radius * 1.2) << mid - u * (m_radius * 0.6) + n * (m_radius * 0.8)
              << mid - u * (m_radius * 0.6) - n * (m_radius * 0.8);
        painter->drawPolygon(arrow);
    }

    // The handles: filled when pinned, hollow until it has anchors of its own.
    QFont f = painter->font();
    f.setPointSizeF(qMax(1.0, m_radius * 1.4));
    f.setBold(true);
    painter->setFont(f);
    for (int i = 0; i < m_points.size(); ++i)
    {
        const QPointF p = m_points.at(i);
        const bool pinned = m_pins.value(i) > 0;
        painter->setPen(QPen(QColor(30, 30, 30), w));
        painter->setBrush(pinned ? QColor(255, 140, 40) : m_own ? line : QColor(60, 60, 60, 200));
        painter->drawEllipse(p, m_radius, m_radius);
        painter->setPen(pinned ? QColor(30, 30, 30) : m_own ? QColor(30, 30, 30) : line);
        const QRectF box(p.x() - m_radius, p.y() - m_radius, 2 * m_radius, 2 * m_radius);
        UprightText::draw(painter, box, Qt::AlignCenter, QString::number(i + 1));
        if (pinned)
        {
            painter->setPen(line);
            UprightText::draw(painter, QRectF(p.x() + m_radius * 1.3, p.y() - m_radius * 2.6, m_radius * 9, m_radius * 2),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              QCoreApplication::translate("FreeStringHandles", "LED %1").arg(m_pins.value(i)));
        }
    }
}

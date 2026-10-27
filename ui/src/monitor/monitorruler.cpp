/*
  Q Light Controller Plus
  monitorruler.cpp

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

#include <QPainter>

#include "monitorruler.h"
#include "monitorgraphicsview.h"

MonitorRuler::MonitorRuler(MonitorGraphicsView *view, Orientation orient, QWidget *parent)
    : QWidget(parent)
    , m_view(view)
    , m_orient(orient)
{
    if (m_orient == Horizontal)
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    else
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    // Repaint whenever the view's mapping changes (zoom / pan / resize / POV /
    // grid metrics / origin).
    connect(m_view, &MonitorGraphicsView::rulersChanged,
            this, QOverload<>::of(&QWidget::update));
}

QSize MonitorRuler::sizeHint() const
{
    return (m_orient == Horizontal) ? QSize(200, thickness())
                                    : QSize(thickness(), 200);
}

QSize MonitorRuler::minimumSizeHint() const
{
    return (m_orient == Horizontal) ? QSize(0, thickness())
                                    : QSize(thickness(), 0);
}

void MonitorRuler::setCursorPixel(int px)
{
    if (px == m_cursorPixel)
        return;
    m_cursorPixel = px;
    update();
}

void MonitorRuler::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const bool horiz = (m_orient == Horizontal);

    // Background + edge line facing the canvas.
    p.fillRect(rect(), QColor(26, 26, 26));
    p.setPen(QColor(60, 60, 60));
    if (horiz)
        p.drawLine(0, height() - 1, width(), height() - 1);
    else
        p.drawLine(width() - 1, 0, width() - 1, height());

    if (m_view == nullptr)
        return;

    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);

    const QVector<MonitorGraphicsView::RulerTick> ticks = m_view->rulerTicks(horiz);
    const QColor tickCol(150, 150, 150);
    const QColor textCol(190, 190, 190);
    // The centre line, in the plot's own teal: its tick runs the ruler's depth.
    const QColor centreCol(0, 190, 175);
    const bool fromCentre = m_view->rulerFromCentreLine(horiz);

    for (const MonitorGraphicsView::RulerTick &t : ticks)
    {
        const qreal c = t.pixel;
        const bool centre = fromCentre && t.value == 0.0;
        if (horiz)
        {
            if (c < -20 || c > width() + 20)
                continue;
            p.setPen(centre ? centreCol : tickCol);
            p.drawLine(QLineF(c, centre ? 0.0 : height() - 6, c, height() - 1));
            p.setPen(centre ? centreCol : textCol);
            // The CL label sits beside its full-height tick, not across it.
            p.drawText(centre ? QRectF(c + 2, 1, 30, height() - 7) : QRectF(c - 24, 1, 48, height() - 7),
                       (centre ? Qt::AlignLeft : Qt::AlignHCenter) | Qt::AlignVCenter, t.label);
        }
        else
        {
            if (c < -20 || c > height() + 20)
                continue;
            p.setPen(centre ? centreCol : tickCol);
            p.drawLine(QLineF(centre ? 0.0 : width() - 6, c, width() - 1, c));
            // Vertical text: rotate the painter so labels read bottom-to-top.
            p.save();
            p.translate(0, c);
            p.rotate(-90);
            p.setPen(centre ? centreCol : textCol);
            p.drawText(centre ? QRectF(2, 0, 30, width() - 7) : QRectF(-24, 0, 48, width() - 7),
                       (centre ? Qt::AlignLeft : Qt::AlignHCenter) | Qt::AlignVCenter, t.label);
            p.restore();
        }
    }

    /* Axis label in the near corner. Counting from the centre line, the
       numbers carry no sign: the label says which side is which. */
    p.setPen(QColor(120, 160, 220));
    QString axis = m_view->axisName(horiz) + " (" + m_view->unitSuffix() + ")";
    if (fromCentre)
        axis = (m_view->stageRightFirst(horiz) ? tr("SR | SL") : tr("SL | SR"))
               + " (" + m_view->unitSuffix() + ")";
    if (horiz)
    {
        const QRectF box(0, 0, p.fontMetrics().horizontalAdvance(axis) + 6, height() - 1);
        p.fillRect(box, QColor(26, 26, 26));     // over any tick label under it
        p.drawText(box.adjusted(2, 1, 0, -1), Qt::AlignLeft | Qt::AlignVCenter, axis);
    }

    // Selection / dragged-item extent band (drawn under the cursor marker).
    if (m_rangeA != -1 || m_rangeB != -1)
    {
        /* Unrounded edges: the band's edges are where the item's are, to the
           device pixel. A narrow item still gets a visible band. */
        qreal a = qMin(m_rangeA, m_rangeB);
        qreal b = qMax(m_rangeA, m_rangeB);
        if (b - a < 2.0)
        {
            const qreal mid = (a + b) / 2.0;
            a = mid - 1.0;
            b = mid + 1.0;
        }
        p.fillRect(horiz ? QRectF(a, 0, b - a, height())
                         : QRectF(0, a, width(), b - a),
                   QColor(255, 196, 64, 90));
        QPen edge(QColor(255, 196, 64), 0);     // cosmetic: one device pixel
        p.setPen(edge);
        if (horiz) { p.drawLine(QLineF(a, 0, a, height())); p.drawLine(QLineF(b, 0, b, height())); }
        else       { p.drawLine(QLineF(0, a, width(), a));  p.drawLine(QLineF(0, b, width(), b)); }
    }

    // Live cursor marker.
    if (m_cursorPixel >= 0)
    {
        p.setPen(QPen(QColor(80, 170, 255), 1));
        if (horiz)
            p.drawLine(m_cursorPixel, 0, m_cursorPixel, height());
        else
            p.drawLine(0, m_cursorPixel, width(), m_cursorPixel);
    }
}

void MonitorRuler::setItemRange(qreal pxA, qreal pxB)
{
    if (pxA == m_rangeA && pxB == m_rangeB)
        return;
    m_rangeA = pxA;
    m_rangeB = pxB;
    update();
}

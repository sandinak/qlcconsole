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

    for (const MonitorGraphicsView::RulerTick &t : ticks)
    {
        const int c = int(t.pixel + 0.5);
        if (horiz)
        {
            if (c < -20 || c > width() + 20)
                continue;
            p.setPen(tickCol);
            p.drawLine(c, height() - 6, c, height() - 1);
            p.setPen(textCol);
            p.drawText(QRect(c - 24, 1, 48, height() - 7),
                       Qt::AlignHCenter | Qt::AlignVCenter, t.label);
        }
        else
        {
            if (c < -20 || c > height() + 20)
                continue;
            p.setPen(tickCol);
            p.drawLine(width() - 6, c, width() - 1, c);
            // Vertical text: rotate the painter so labels read bottom-to-top.
            p.save();
            p.translate(0, c);
            p.rotate(-90);
            p.setPen(textCol);
            p.drawText(QRect(-24, 0, 48, width() - 7),
                       Qt::AlignHCenter | Qt::AlignVCenter, t.label);
            p.restore();
        }
    }

    // Axis label in the near corner.
    p.setPen(QColor(120, 160, 220));
    const QString axis = m_view->axisName(horiz) + " (" + m_view->unitSuffix() + ")";
    if (horiz)
        p.drawText(QRect(2, 1, 60, height() - 2), Qt::AlignLeft | Qt::AlignVCenter, axis);

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

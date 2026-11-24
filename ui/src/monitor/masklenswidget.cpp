/*
  Q Light Controller Plus
  masklenswidget.cpp

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
#include <QPainterPath>
#include <QMouseEvent>
#include <QtMath>
#include <algorithm>

#include "masklenswidget.h"

namespace
{
const int kPlot = 140;      // the widget's side
const int kLabelH = 30;     // the label under the picture
const int kMargin = 8;
const double kGrab = 7.0;   // pixels either side of an edge that picks it up
}

MaskLensWidget::MaskLensWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(kPlot, kPlot);
    setMouseTracking(true);
    setAccessibleName(tr("Mask lens"));
    setToolTip(tr("The beam seen from behind the light. Drag a band's edge in from the "
                  "top, bottom, left or right to tape that much off the beam; the shaded "
                  "bands are what the tape covers."));
    refreshText();
}

void MaskLensWidget::setLensAngle(double degrees)
{
    m_halfW = m_halfH = qMax(0.5, degrees / 2.0);
    m_rect = false;
    setMask(m_mask);
}

void MaskLensWidget::setScanWindow(double hDeg, double vDeg)
{
    m_halfW = qMax(0.5, hDeg / 2.0);
    m_halfH = qMax(0.5, vDeg / 2.0);
    m_rect = true;
    setMask(m_mask);
}

void MaskLensWidget::setMask(float top, float bottom, float left, float right)
{
    LightMask m;
    m.top = qBound(0.0f, top, float(m_halfH));
    m.bottom = qBound(0.0f, bottom, float(m_halfH));
    m.left = qBound(0.0f, left, float(m_halfW));
    m.right = qBound(0.0f, right, float(m_halfW));
    m_mask = m;
    refreshText();
    update();
}

void MaskLensWidget::setMask(const LightMask &mask)
{
    setMask(mask.top, mask.bottom, mask.left, mask.right);
}

double MaskLensWidget::percentThrough(double halfW, double halfH, bool rectangle, const LightMask &mask)
{
    // In angle space, the same cut LightRays::cone / window make: the beam is
    // kept where ax is within [-halfW + left, halfW - right] and ay within
    // [-halfH + top, halfH - bottom].
    const double x0 = -halfW + double(mask.left), x1 = halfW - double(mask.right);
    const double y0 = -halfH + double(mask.top), y1 = halfH - double(mask.bottom);
    if (x1 <= x0 || y1 <= y0)
        return 0.0;
    if (rectangle)
        return 100.0 * ((x1 - x0) * (y1 - y0)) / (4.0 * halfW * halfH);
    const double r = halfW;
    const double xa = qMax(x0, -r), xb = qMin(x1, r);
    if (xb <= xa)
        return 0.0;
    const int n = 4000;
    const double dx = (xb - xa) / n;
    double area = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double x = xa + (i + 0.5) * dx;
        const double s = qSqrt(qMax(0.0, r * r - x * x));
        const double lo = qMax(y0, -s), hi = qMin(y1, s);
        if (hi > lo)
            area += (hi - lo) * dx;
    }
    return 100.0 * area / (M_PI * r * r);
}

double MaskLensWidget::percentThrough() const
{
    return percentThrough(m_halfW, m_halfH, m_rect, m_mask);
}

void MaskLensWidget::refreshText()
{
    setAccessibleDescription(tr("%1% of the beam gets through. Tape off: top %2 degrees, "
                                "bottom %3, left %4, right %5.")
                                 .arg(qRound(percentThrough()))
                                 .arg(double(m_mask.top)).arg(double(m_mask.bottom))
                                 .arg(double(m_mask.left)).arg(double(m_mask.right)));
}

QPointF MaskLensWidget::centre() const
{
    return QPointF(width() / 2.0, (height() - kLabelH) / 2.0);
}

double MaskLensWidget::scale() const
{
    const double availW = width() / 2.0 - kMargin;
    const double availH = (height() - kLabelH) / 2.0 - kMargin;
    return qMin(availW / m_halfW, availH / m_halfH);
}

double MaskLensWidget::limit(Side side) const
{
    return (side == Top || side == Bottom) ? m_halfH : m_halfW;
}

QPointF MaskLensWidget::edgePoint(Side side) const
{
    const QPointF c = centre();
    const double k = scale();
    switch (side)
    {
    case Top: return QPointF(c.x(), c.y() + (-m_halfH + double(m_mask.top)) * k);
    case Bottom: return QPointF(c.x(), c.y() + (m_halfH - double(m_mask.bottom)) * k);
    case Left: return QPointF(c.x() + (-m_halfW + double(m_mask.left)) * k, c.y());
    default: return QPointF(c.x() + (m_halfW - double(m_mask.right)) * k, c.y());
    }
}

int MaskLensWidget::hitEdge(const QPointF &p) const
{
    const QPointF c = centre();
    const double k = scale();
    if (qAbs(p.x() - c.x()) > m_halfW * k + kGrab || qAbs(p.y() - c.y()) > m_halfH * k + kGrab)
        return -1;
    int best = -1;
    double bestD = kGrab + 0.001;
    for (int s = Top; s <= Right; ++s)
    {
        const QPointF e = edgePoint(Side(s));
        const double d = (s == Top || s == Bottom) ? qAbs(p.y() - e.y()) : qAbs(p.x() - e.x());
        if (d < bestD)
        {
            bestD = d;
            best = s;
        }
    }
    return best;
}

void MaskLensWidget::dragTo(const QPointF &p)
{
    const QPointF c = centre();
    const double k = scale();
    double v = 0.0;
    switch (m_drag)
    {
    case Top: v = (p.y() - c.y()) / k + m_halfH; break;
    case Bottom: v = m_halfH - (p.y() - c.y()) / k; break;
    case Left: v = (p.x() - c.x()) / k + m_halfW; break;
    case Right: v = m_halfW - (p.x() - c.x()) / k; break;
    default: return;
    }
    v = qBound(0.0, qRound(v * 2.0) / 2.0, limit(Side(m_drag)));
    LightMask m = m_mask;
    float *f = m_drag == Top ? &m.top : m_drag == Bottom ? &m.bottom
             : m_drag == Left ? &m.left : &m.right;
    if (*f == float(v))
        return;
    *f = float(v);
    m_mask = m;
    refreshText();
    update();
    emit maskChanged(m_mask.top, m_mask.bottom, m_mask.left, m_mask.right);
}

void MaskLensWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_drag = hitEdge(e->position());
    if (m_drag >= 0)
        dragTo(e->position());
}

void MaskLensWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_drag >= 0)
    {
        dragTo(e->position());
        return;
    }
    const int h = hitEdge(e->position());
    setCursor(h < 0 ? QCursor() : QCursor((h == Top || h == Bottom) ? Qt::SizeVerCursor : Qt::SizeHorCursor));
}

void MaskLensWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_drag = -1;
}

void MaskLensWidget::leaveEvent(QEvent *)
{
    unsetCursor();
}

void MaskLensWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(24, 24, 24));

    const QPointF c = centre();
    const double k = scale();
    const QRectF shape(c.x() - m_halfW * k, c.y() - m_halfH * k, 2 * m_halfW * k, 2 * m_halfH * k);
    QPainterPath beam;
    if (m_rect)
        beam.addRect(shape);
    else
        beam.addEllipse(shape);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 236, 160, 200));
    p.drawPath(beam);

    // The taped bands: what the mask covers, shaded.
    const QRectF open(c.x() + (-m_halfW + m_mask.left) * k, c.y() + (-m_halfH + m_mask.top) * k,
                      (2 * m_halfW - m_mask.left - m_mask.right) * k,
                      (2 * m_halfH - m_mask.top - m_mask.bottom) * k);
    QPainterPath keep;
    keep.addRect(open.normalized());
    QPainterPath taped = beam.subtracted(keep);
    p.setBrush(QColor(10, 10, 10, 225));
    p.drawPath(taped);
    p.setBrush(QBrush(QColor(200, 70, 70, 120), Qt::BDiagPattern));
    p.drawPath(taped);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(140, 140, 140), 1));
    p.drawPath(beam);

    // The edges that can be dragged, where they are now.
    p.setPen(QPen(QColor(255, 255, 255, 230), 2));
    for (int s = Top; s <= Right; ++s)
    {
        const QPointF e = edgePoint(Side(s));
        if (s == Top || s == Bottom)
            p.drawLine(QPointF(e.x() - 9, e.y()), QPointF(e.x() + 9, e.y()));
        else
            p.drawLine(QPointF(e.x(), e.y() - 9), QPointF(e.x(), e.y() + 9));
    }

    QFont f = font();
    f.setPointSizeF(qMax(7.0, f.pointSizeF() - 1.5));
    p.setFont(f);
    p.setPen(QColor(220, 220, 220));
    p.drawText(QRectF(4, height() - kLabelH, width() - 8, kLabelH - 2), Qt::AlignCenter | Qt::TextWordWrap,
               tr("%1% of the beam gets through").arg(qRound(percentThrough())));
}

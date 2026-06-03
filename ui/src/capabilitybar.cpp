/*
  Q Light Controller Plus
  capabilitybar.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QToolTip>

#include "capabilitybar.h"

#define BAR_MAX 255

CapabilityBar::CapabilityBar(QWidget *parent)
    : QWidget(parent)
    , m_value(0)
{
    setMinimumHeight(44);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
}

void CapabilityBar::setValue(int v)
{
    v = qBound(0, v, BAR_MAX);
    if (v == m_value)
        return;
    m_value = v;
    update();
    emit valueChanged(m_value);
}

void CapabilityBar::setRegions(const QList<Region> &regions)
{
    m_regions = regions;
    update();
}

int CapabilityBar::xForValue(int v) const
{
    const int w = width() - 1;
    return int(qreal(v) / BAR_MAX * w);
}

void CapabilityBar::setFromX(int x)
{
    const int w = width() - 1;
    if (w <= 0)
        return;
    setValue(int(qreal(qBound(0, x, w)) / w * BAR_MAX + 0.5));
}

void CapabilityBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int barTop = 0;
    const int barH = qMax(16, height() - 16); // leave room for labels below

    // Gradient groove.
    QLinearGradient grad(0, 0, width(), 0);
    grad.setColorAt(0.0, QColor(0, 0, 0));
    grad.setColorAt(1.0, QColor(255, 255, 255));
    QRect bar(0, barTop, width() - 1, barH);
    p.fillRect(bar, grad);
    p.setPen(QColor(70, 70, 70));
    p.drawRect(bar);

    // Region boundaries + names (alternate label heights to reduce overlap).
    int li = 0;
    foreach (const Region &r, m_regions)
    {
        const int x = xForValue(r.min);
        p.setPen(QColor(200, 60, 60, 180));
        p.drawLine(x, barTop, x, barTop + barH);
        if (r.name.isEmpty() == false)
        {
            p.setPen(QColor(150, 30, 30));
            const int ty = barTop + barH + (li % 2 ? 12 : 24) - 2;
            p.drawText(x + 2, ty, r.name);
        }
        li++;
    }

    // Value handle.
    const int hx = xForValue(m_value);
    p.setPen(QPen(QColor(220, 40, 40), 2));
    p.drawLine(hx, barTop, hx, barTop + barH);
    QPolygon tri;
    tri << QPoint(hx - 4, barTop) << QPoint(hx + 4, barTop) << QPoint(hx, barTop + 6);
    p.setBrush(QColor(220, 40, 40));
    p.setPen(Qt::NoPen);
    p.drawPolygon(tri);
}

void CapabilityBar::mousePressEvent(QMouseEvent *e)
{
    setFromX(e->pos().x());
}

void CapabilityBar::mouseMoveEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::LeftButton)
        setFromX(e->pos().x());
}

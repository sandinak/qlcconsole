/*
  Q Light Controller Plus
  gradientdirectionwidget.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "gradientdirectionwidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPushButton>
#include <QLinearGradient>
#include <QtMath>

GradientDirectionWidget::GradientDirectionWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(false);

    // Cardinal quick-set buttons, labelled with the screen direction the
    // gradient flows toward (arrow points at the END colour).
    struct { QPushButton **btn; const char *txt; double deg; const char *tip; } defs[] = {
        { &m_right, "→", 0.0,   "Left → Right" },
        { &m_down,  "↓", 90.0,  "Top → Bottom" },
        { &m_left,  "←", 180.0, "Right → Left" },
        { &m_up,    "↑", 270.0, "Bottom → Top" },
    };
    for (auto &d : defs)
    {
        *d.btn = new QPushButton(QString::fromUtf8(d.txt), this);
        (*d.btn)->setToolTip(tr(d.tip));
        (*d.btn)->setFixedHeight(kBtnH - 2);
        const double deg = d.deg;
        connect(*d.btn, &QPushButton::clicked, this, [this, deg] {
            setAngle(deg);
            emit angleChanged(m_angle);
        });
    }

    reposition();
}

void GradientDirectionWidget::setAngle(double deg)
{
    double a = std::fmod(deg, 360.0);
    if (a < 0) a += 360.0;
    if (qFuzzyCompare(a + 1.0, m_angle + 1.0))
        return;
    m_angle = a;
    update();
}

void GradientDirectionWidget::setColors(const QColor &start, const QColor &end)
{
    if (start == m_start && end == m_end)
        return;
    m_start = start;
    m_end   = end;
    update();
}

void GradientDirectionWidget::setStops(const QList<QColor> &stops)
{
    m_stops = stops;
    if (!stops.isEmpty())
    {
        m_start = stops.first();
        m_end   = stops.last();
    }
    update();
}

QRect GradientDirectionWidget::compassRect() const
{
    // Square preview centred in the area above the button strip.
    const int avail = height() - kBtnH;
    const int side  = qMax(20, qMin(width(), avail) - 8);
    const int x = (width() - side) / 2;
    const int y = (avail - side) / 2;
    return QRect(x, y, side, side);
}

void GradientDirectionWidget::reposition()
{
    // Lay the four buttons out in a row across the bottom.
    const int y = height() - kBtnH + 1;
    const int w = width() / 4;
    QPushButton *order[] = { m_left, m_up, m_down, m_right };
    for (int i = 0; i < 4; i++)
        order[i]->setGeometry(i * w, y, w - 2, kBtnH - 2);
}

void GradientDirectionWidget::resizeEvent(QResizeEvent *)
{
    reposition();
}

void GradientDirectionWidget::setAngleFromPos(const QPointF &p, bool snap)
{
    const QRect r = compassRect();
    const QPointF c = QPointF(r.center());
    const double sx = p.x() - c.x();
    const double sy = p.y() - c.y();
    if (qFuzzyIsNull(sx) && qFuzzyIsNull(sy))
        return;
    double deg = qRadiansToDegrees(std::atan2(sy, sx)); // screen: +x right, +y down
    if (deg < 0) deg += 360.0;
    if (snap)
        deg = qRound(deg / 45.0) * 45.0;
    setAngle(deg);
    emit angleChanged(m_angle);
}

void GradientDirectionWidget::mousePressEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton && compassRect().contains(ev->pos()))
        setAngleFromPos(ev->pos(), ev->modifiers() & Qt::ShiftModifier);
    else
        QWidget::mousePressEvent(ev);
}

void GradientDirectionWidget::mouseMoveEvent(QMouseEvent *ev)
{
    if (ev->buttons() & Qt::LeftButton)
        setAngleFromPos(ev->pos(), ev->modifiers() & Qt::ShiftModifier);
}

void GradientDirectionWidget::paintEvent(QPaintEvent *)
{
    QPainter pr(this);
    pr.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = compassRect();
    const QPointF c = QPointF(r.center());
    const double rad = r.width() / 2.0;
    const double a = qDegreesToRadians(m_angle);
    const QPointF dir(std::cos(a), std::sin(a));   // screen-space unit vector

    // --- Gradient preview fill (all stops along the arrow) ---
    QLinearGradient grad(c - dir * rad, c + dir * rad);
    if (m_stops.size() >= 2)
    {
        for (int s = 0; s < m_stops.size(); ++s)
            grad.setColorAt(double(s) / (m_stops.size() - 1), m_stops.at(s));
    }
    else
    {
        grad.setColorAt(0.0, m_start);
        grad.setColorAt(1.0, m_end);
    }
    QPainterPath disc;
    disc.addEllipse(r);
    pr.fillPath(disc, grad);
    pr.setPen(QPen(QColor(0, 0, 0, 120), 1));
    pr.drawPath(disc);

    // --- Direction arrow (points toward the END colour) ---
    const QPointF tip = c + dir * (rad - 6);
    const QPointF tail = c - dir * (rad - 6);
    QColor arrowCol = (m_end.lightness() > 140) ? QColor(20, 20, 20) : QColor(245, 245, 245);
    QPen ap(arrowCol, 2.0);
    pr.setPen(ap);
    pr.drawLine(tail, tip);
    // arrowhead
    const QPointF perp(-dir.y(), dir.x());
    QPainterPath head;
    head.moveTo(tip);
    head.lineTo(tip - dir * 10 + perp * 5);
    head.lineTo(tip - dir * 10 - perp * 5);
    head.closeSubpath();
    pr.fillPath(head, QBrush(arrowCol));
    // centre hub
    pr.setBrush(arrowCol);
    pr.drawEllipse(c, 2.5, 2.5);

    // --- Degree readout ---
    pr.setPen(palette().color(QPalette::Text));
    pr.drawText(r.adjusted(0, 0, 0, -2),
                Qt::AlignBottom | Qt::AlignHCenter,
                QString::number(int(m_angle)) + QString::fromUtf8("°"));
}

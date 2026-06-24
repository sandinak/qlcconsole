/*
  Q Light Controller Plus
  pathdrawwidget.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "pathdrawwidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonArray>
#include <QtMath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

PathDrawWidget::PathDrawWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);

    m_clearBtn  = new QPushButton(tr("Clear"),  this);
    m_circleBtn = new QPushButton(tr("Circle"), this);
    m_fig8Btn   = new QPushButton(tr("Fig-8"),  this);

    m_pts = makeCircle();

    connect(m_clearBtn,  &QPushButton::clicked, this, [this] {
        m_pts.clear();
        m_hover = -1;
        update();
        emitChanged();
    });
    connect(m_circleBtn, &QPushButton::clicked, this, [this] {
        m_pts = makeCircle();
        m_hover = -1;
        update();
        emitChanged();
    });
    connect(m_fig8Btn,   &QPushButton::clicked, this, [this] {
        m_pts = makeFig8();
        m_hover = -1;
        update();
        emitChanged();
    });
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void PathDrawWidget::setPath(const QString &jsonPath)
{
    m_pts.clear();
    QJsonDocument doc = QJsonDocument::fromJson(jsonPath.toUtf8());
    if (doc.isArray())
    {
        for (const QJsonValue &v : doc.array())
        {
            if (!v.isArray() || v.toArray().size() < 2)
                continue;
            double x = qBound(0.0, v.toArray()[0].toDouble(), 1.0);
            double y = qBound(0.0, v.toArray()[1].toDouble(), 1.0);
            m_pts.append(QPointF(x, y));
        }
    }
    if (m_pts.isEmpty())
        m_pts = makeCircle();
    update();
}

QString PathDrawWidget::pathJson() const
{
    QJsonArray arr;
    for (const QPointF &pt : m_pts)
    {
        QJsonArray pair;
        pair.append(pt.x());
        pair.append(pt.y());
        arr.append(pair);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------

QRect PathDrawWidget::canvasRect() const
{
    return QRect(0, 0, width(), height() - kBtnH - 2);
}

void PathDrawWidget::reposition()
{
    const int W  = width();
    const int bw = (W - 4) / 3;
    const int y  = height() - kBtnH;
    m_clearBtn ->setGeometry(0,        y, bw,     kBtnH);
    m_circleBtn->setGeometry(bw + 2,   y, bw,     kBtnH);
    m_fig8Btn  ->setGeometry(2*bw + 4, y, W - (2*bw + 4), kBtnH);
}

void PathDrawWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    reposition();
}

// ---------------------------------------------------------------------------
// Coordinate conversion
// ---------------------------------------------------------------------------

QPointF PathDrawWidget::toWidget(const QPointF &norm) const
{
    QRect cr = canvasRect();
    const int m = 8;
    return QPointF(cr.left() + m + norm.x() * (cr.width()  - 2*m),
                   cr.top()  + m + norm.y() * (cr.height() - 2*m));
}

QPointF PathDrawWidget::toNorm(const QPointF &w) const
{
    QRect cr = canvasRect();
    const int m = 8;
    double x = (w.x() - cr.left() - m) / double(cr.width()  - 2*m);
    double y = (w.y() - cr.top()  - m) / double(cr.height() - 2*m);
    return QPointF(qBound(0.0, x, 1.0), qBound(0.0, y, 1.0));
}

int PathDrawWidget::hitTest(const QPointF &pos) const
{
    for (int i = 0; i < m_pts.size(); ++i)
    {
        QPointF wpt = toWidget(m_pts[i]);
        double dx = pos.x() - wpt.x();
        double dy = pos.y() - wpt.y();
        if (dx*dx + dy*dy <= kHit * kHit)
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Spline
// ---------------------------------------------------------------------------

static QPointF catmullRom(const QPointF &p0, const QPointF &p1,
                           const QPointF &p2, const QPointF &p3, double t)
{
    double t2 = t*t, t3 = t2*t;
    auto cr = [&](double a, double b, double c, double d) {
        return 0.5 * ((-a + 3*b - 3*c + d)*t3 + (2*a - 5*b + 4*c - d)*t2 + (-a + c)*t + 2*b);
    };
    return QPointF(cr(p0.x(), p1.x(), p2.x(), p3.x()),
                   cr(p0.y(), p1.y(), p2.y(), p3.y()));
}

QPainterPath PathDrawWidget::buildSpline() const
{
    QPainterPath path;
    const int n = m_pts.size();
    if (n < 2)
        return path;

    const int steps = 20;
    for (int seg = 0; seg < n; ++seg)
    {
        const QPointF &p0 = m_pts[(seg - 1 + n) % n];
        const QPointF &p1 = m_pts[seg];
        const QPointF &p2 = m_pts[(seg + 1) % n];
        const QPointF &p3 = m_pts[(seg + 2) % n];

        for (int s = 0; s <= steps; ++s)
        {
            double t = double(s) / steps;
            QPointF wpt = toWidget(catmullRom(p0, p1, p2, p3, t));
            if (seg == 0 && s == 0)
                path.moveTo(wpt);
            else
                path.lineTo(wpt);
        }
    }
    path.closeSubpath();
    return path;
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void PathDrawWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect cr = canvasRect();

    // Background
    p.fillRect(cr, QColor(0x18, 0x18, 0x18));

    // Grid — 4×4 divisions
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1));
    const int m = 8;
    const int gx0 = cr.left() + m, gx1 = cr.right() - m;
    const int gy0 = cr.top()  + m, gy1 = cr.bottom() - m;
    for (int i = 1; i < 4; ++i)
    {
        int x = gx0 + (gx1 - gx0) * i / 4;
        int y = gy0 + (gy1 - gy0) * i / 4;
        p.drawLine(x, gy0, x, gy1);
        p.drawLine(gx0, y, gx1, y);
    }
    // Border
    p.setPen(QPen(QColor(0x44, 0x44, 0x44), 1));
    p.drawRect(cr.adjusted(0, 0, -1, -1));

    // Axis labels
    p.setPen(QColor(0x55, 0x55, 0x55));
    p.setFont(QFont(font().family(), 7));
    p.drawText(gx0, gy1 + 2, tr("Pan →"));
    QTransform rot;
    rot.translate(gx0 - 2, gy0);
    rot.rotate(-90);
    p.setTransform(rot);
    p.drawText(0, 0, tr("Tilt ↓"));
    p.resetTransform();

    // Spline
    if (m_pts.size() >= 2)
    {
        p.setPen(QPen(QColor(0x00, 0xc8, 0xff), 2));
        p.drawPath(buildSpline());

        // Direction arrow at midpoint of first segment
        QPointF mid = toWidget(catmullRom(
            m_pts[(0 - 1 + m_pts.size()) % m_pts.size()],
            m_pts[0],
            m_pts[1 % m_pts.size()],
            m_pts[2 % m_pts.size()],
            0.5));
        QPointF nxt = toWidget(catmullRom(
            m_pts[(0 - 1 + m_pts.size()) % m_pts.size()],
            m_pts[0],
            m_pts[1 % m_pts.size()],
            m_pts[2 % m_pts.size()],
            0.55));
        QPointF dir = nxt - mid;
        double len = qSqrt(dir.x()*dir.x() + dir.y()*dir.y());
        if (len > 1e-6)
        {
            dir /= len;
            QPointF perp(-dir.y(), dir.x());
            QPolygonF arrow;
            arrow << mid + dir * 7.0
                  << mid - dir * 4.0 + perp * 4.0
                  << mid - dir * 4.0 - perp * 4.0;
            p.setBrush(QColor(0x00, 0xc8, 0xff));
            p.setPen(Qt::NoPen);
            p.drawPolygon(arrow);
        }
    }

    // Control points
    for (int i = 0; i < m_pts.size(); ++i)
    {
        QPointF wpt = toWidget(m_pts[i]);
        bool hovered = (i == m_hover);
        bool dragged = (i == m_drag);

        QColor fillColor = dragged ? QColor(0xff, 0xe0, 0x00)
                         : hovered ? QColor(0xff, 0xaa, 0x00)
                                   : QColor(0xff, 0xff, 0xff);
        int r = dragged ? kDotR + 2 : kDotR;

        p.setBrush(fillColor);
        p.setPen(QPen(QColor(0x00, 0x00, 0x00), 1.5));
        p.drawEllipse(wpt, r, r);

        // Index label for the first point only (shows path direction start)
        if (i == 0)
        {
            p.setPen(QColor(0xaa, 0xaa, 0xaa));
            p.setFont(QFont(font().family(), 8));
            p.drawText(wpt + QPointF(kDotR + 2, kDotR), QStringLiteral("0"));
        }
    }

    // Help text when empty
    if (m_pts.isEmpty())
    {
        p.setPen(QColor(0x66, 0x66, 0x66));
        p.setFont(QFont(font().family(), 9));
        p.drawText(cr, Qt::AlignCenter,
                   tr("Click to add points\nRight-click to delete"));
    }
}

// ---------------------------------------------------------------------------
// Mouse events
// ---------------------------------------------------------------------------

void PathDrawWidget::mousePressEvent(QMouseEvent *e)
{
    if (!canvasRect().contains(e->pos()))
        return;

    int hit = hitTest(e->pos());

    if (e->button() == Qt::RightButton)
    {
        if (hit >= 0)
        {
            m_pts.removeAt(hit);
            m_hover = -1;
            m_drag  = -1;
            update();
            emitChanged();
        }
        return;
    }

    if (e->button() == Qt::LeftButton)
    {
        if (hit >= 0)
        {
            m_drag = hit;
        }
        else
        {
            // Add new point
            m_pts.append(toNorm(e->pos()));
            m_drag = m_pts.size() - 1;
            emitChanged();
        }
        update();
    }
}

void PathDrawWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!canvasRect().contains(e->pos()))
    {
        if (m_hover != -1) { m_hover = -1; update(); }
        return;
    }

    if (m_drag >= 0 && m_drag < m_pts.size())
    {
        m_pts[m_drag] = toNorm(e->pos());
        update();
        // don't emit on every move pixel — emit on release
    }
    else
    {
        int h = hitTest(e->pos());
        if (h != m_hover)
        {
            m_hover = h;
            setCursor(h >= 0 ? Qt::SizeAllCursor : Qt::CrossCursor);
            update();
        }
    }
}

void PathDrawWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_drag >= 0)
    {
        m_drag = -1;
        emitChanged();
        update();
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void PathDrawWidget::emitChanged()
{
    emit pathChanged(pathJson());
}

QList<QPointF> PathDrawWidget::makeCircle()
{
    QList<QPointF> pts;
    const int N = 8;
    for (int i = 0; i < N; ++i)
    {
        double a = i * 2.0 * M_PI / N;
        pts.append(QPointF(0.5 + 0.35 * qCos(a), 0.5 + 0.35 * qSin(a)));
    }
    return pts;
}

QList<QPointF> PathDrawWidget::makeFig8()
{
    QList<QPointF> pts;
    const int N = 16;
    for (int i = 0; i < N; ++i)
    {
        double t = i * 2.0 * M_PI / N;
        // Lemniscate of Bernoulli (figure-8)
        double denom = 1.0 + qSin(t) * qSin(t);
        double x = 0.5 + 0.38 * qCos(t) / denom;
        double y = 0.5 + 0.28 * qSin(t) * qCos(t) / denom;
        pts.append(QPointF(x, y));
    }
    return pts;
}

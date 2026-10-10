/*
  Q Light Controller Plus
  fountaindiagram.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QPainterPath>
#include <QPainter>
#include <QtMath>

#include "fountaindiagram.h"

FountainDiagram::FountainDiagram(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("fountainDiagram");
}

void FountainDiagram::setValue(const QString &key, double metres, const QString &label)
{
    m_values.insert(key, metres);
    m_labels.insert(key, label);
    update();
}

void FountainDiagram::setFlame(bool on)
{
    m_flame = on;
    update();
}

void FountainDiagram::setHighlight(const QString &key)
{
    m_highlight = key;
    update();
}

void FountainDiagram::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(28, 30, 36));

    const QColor ink(200, 204, 214), dim(120, 124, 134);
    const QColor hot(255, 190, 60);
    const QColor baseFill(96, 92, 86), blockFill(150, 142, 128);
    QFont f = font();
    f.setPointSizeF(qMax(7.0, f.pointSizeF() - 1.0));
    p.setFont(f);
    const QFontMetricsF fm(f);

    const double W = qMax(0.05, value("width")), D = qMax(0.05, value("depth"));
    const double t = qMax(0.005, value("thickness")), g = qMax(0.0, value("gap"));
    const double base = qMax(0.0, value("base"));
    const double outer = qMax(0.0, value("outer")), inner = qMax(0.0, value("inner"));

    /* A dimension: a line with a tick at each end and its label beside it,
       the one being edited in the highlight colour. */
    auto dimension = [&](const QString &key, const QString &name, QPointF a, QPointF b,
                         bool labelAfter) {
        const bool on = (m_highlight == key);
        p.setPen(QPen(on ? hot : ink, on ? 2.2 : 1.1));
        p.drawLine(a, b);
        QPointF d = b - a;
        const double len = qSqrt(d.x() * d.x() + d.y() * d.y());
        if (len > 1e-6)
        {
            const QPointF n(-d.y() / len * 4.0, d.x() / len * 4.0);
            p.drawLine(a - n, a + n);
            p.drawLine(b - n, b + n);
        }
        const QString text = name.isEmpty() ? m_labels.value(key)
                                            : name + QStringLiteral(" ") + m_labels.value(key);
        const QPointF mid = (a + b) / 2.0;
        const bool vertical = qAbs(d.x()) < qAbs(d.y());
        QRectF r(0, 0, fm.horizontalAdvance(text) + 6, fm.height() + 2);
        if (vertical)
            r.moveCenter(mid + QPointF(labelAfter ? r.width() / 2.0 + 6 : -r.width() / 2.0 - 6, 0));
        else
            r.moveCenter(mid + QPointF(0, labelAfter ? fm.height() / 2.0 + 5 : -fm.height() / 2.0 - 5));
        p.fillRect(r, QColor(28, 30, 36, 210));
        p.setPen(on ? hot : ink);
        p.drawText(r, Qt::AlignCenter, text);
    };

    /* From above: the back (open) at the top, the round front at the bottom,
       as on the plot -- audience below. */
    const double margin = 30.0;
    const QRectF planBox(margin + 20, 56, width() - 2 * margin - 100, height() * 0.52 - 60);
    const double s = qMin(planBox.width() / W, planBox.height() / D);
    const double cx = planBox.center().x();
    const double top = planBox.top();
    const double R = W / 2.0;
    const double arcY = top + qMax(0.0, D - R) * s;     // where the round front begins
    p.setPen(dim);
    p.drawText(QPointF(margin, 16), tr("From above (the audience is below)"));

    // The base: the whole outline, round front.
    QPainterPath outline;
    outline.moveTo(cx - R * s, top);
    outline.lineTo(cx - R * s, arcY);
    outline.arcTo(QRectF(cx - R * s, arcY - R * s, 2 * R * s, 2 * R * s), 180.0, 180.0);
    outline.lineTo(cx + R * s, top);
    outline.closeSubpath();
    p.setPen(QPen(m_highlight == "base" ? hot : baseFill.lighter(140), m_highlight == "base" ? 2.0 : 1.0));
    p.setBrush(baseFill);
    p.drawPath(outline);

    // A wall: its middle line, stroked as wide as it is thick.
    auto wall = [&](double inset, bool hi) {
        const double r = R - inset;
        if (r <= 0.0)
            return;
        QPainterPath u;
        u.moveTo(cx - r * s, top);
        u.lineTo(cx - r * s, arcY);
        u.arcTo(QRectF(cx - r * s, arcY - r * s, 2 * r * s, 2 * r * s), 180.0, 180.0);
        u.lineTo(cx + r * s, top);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(blockFill, qMax(1.5, t * s), Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        p.drawPath(u);
        if (hi)
        {
            p.setPen(QPen(hot, 1.6, Qt::DashLine));
            QPainterPathStroker st;
            st.setWidth(qMax(1.5, t * s));
            st.setCapStyle(Qt::FlatCap);
            p.drawPath(st.createStroke(u));
        }
    };
    wall(t / 2.0, m_highlight == "outer");
    wall(t + g + t / 2.0, m_highlight == "inner");

    if (m_flame)
    {
        const QPointF fc(cx, top + (D - (t + g + t / 2.0)) * 0.5 * s);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 120, 30));
        p.drawEllipse(fc, 7, 7);
        p.setBrush(QColor(255, 210, 70));
        p.drawEllipse(fc, 3.5, 3.5);
        p.setPen(dim);
        p.drawText(fc + QPointF(10, 4), tr("flame"));
    }

    // Width across the back, depth down the side, thickness and gap on the left.
    dimension("width", tr("Width"), QPointF(cx - R * s, top - 10), QPointF(cx + R * s, top - 10), false);
    dimension("depth", tr("Depth"), QPointF(cx + R * s + 14, top), QPointF(cx + R * s + 14, top + D * s), true);
    const double sideY = top + qMin(D - R, D) * s * 0.35 + 8;
    dimension("thickness", tr("Wall"), QPointF(cx - R * s, sideY), QPointF(cx - (R - t) * s, sideY), true);
    const double gapY = sideY + fm.height() + 16;
    dimension("gap", tr("Gap"), QPointF(cx - (R - t) * s, gapY), QPointF(cx - (R - t - g) * s, gapY), true);

    /* Cut through the front, seen from the side: the base, the outer wall at
       the edge, the gap, the inner wall, the middle. */
    const double secTop = height() * 0.52 + 10;
    p.setPen(dim);
    p.drawText(QPointF(margin, secTop + 12), tr("Cut through the front"));
    const double floorY = height() - 26;
    const double reach = t + g + t + 0.3;               // edge to well past the inner wall
    const double tall = base + qMax(outer, inner) + 0.25;
    const QRectF secBox(margin + 70, secTop + 24, width() - 2 * margin - 150, floorY - secTop - 30);
    const double k = qMin(secBox.width() / reach, secBox.height() / tall);
    const double x0 = secBox.left();                    // the outside edge
    auto X = [&](double m) { return x0 + m * k; };
    auto Y = [&](double m) { return floorY - m * k; };
    p.setPen(QPen(dim, 1.0));
    p.drawLine(QPointF(margin, floorY), QPointF(width() - margin, floorY));
    p.drawText(QPointF(X(0) - 6 - fm.horizontalAdvance(tr("outside")), floorY + 16), tr("outside"));
    p.drawText(QPointF(X(reach) - fm.horizontalAdvance(tr("middle")), floorY + 16), tr("middle →"));

    p.setPen(QPen(m_highlight == "base" ? hot : baseFill.lighter(140), m_highlight == "base" ? 2.0 : 1.0));
    p.setBrush(baseFill);
    p.drawRect(QRectF(QPointF(X(0), Y(base)), QPointF(X(reach), Y(0))));
    auto block = [&](double from, double h, bool hi) {
        p.setPen(QPen(hi ? hot : blockFill.lighter(130), hi ? 2.0 : 1.0));
        p.setBrush(blockFill);
        p.drawRect(QRectF(QPointF(X(from), Y(base + h)), QPointF(X(from + t), Y(base))));
    };
    block(0.0, outer, m_highlight == "outer" || m_highlight == "thickness");
    block(t + g, inner, m_highlight == "inner" || m_highlight == "thickness");
    if (m_flame)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 120, 30, 200));
        QPainterPath tongue;
        const double fx = X(reach) - 14;
        tongue.moveTo(fx - 8, Y(base));
        tongue.quadTo(fx - 10, Y(base) - 30, fx, Y(base) - 46);
        tongue.quadTo(fx + 10, Y(base) - 30, fx + 8, Y(base));
        tongue.closeSubpath();
        p.drawPath(tongue);
    }

    dimension("base", tr("Base"), QPointF(X(0) - 12, Y(0)), QPointF(X(0) - 12, Y(base)), false);
    dimension("outer", tr("Outer"), QPointF(X(0) - 12, Y(base)), QPointF(X(0) - 12, Y(base + outer)), false);
    dimension("inner", tr("Inner"), QPointF(X(t + g + t) + 12, Y(base)), QPointF(X(t + g + t) + 12, Y(base + inner)), true);
    dimension("gap", QString(), QPointF(X(t), Y(base) - 6), QPointF(X(t + g), Y(base) - 6), true);
}

/*
  Q Light Controller Plus
  linespreadpreview.cpp

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

#include <QContextMenuEvent>
#include <QPainter>
#include <QMenu>
#include <QtGlobal>
#include <climits>
#include <QMap>
#include <QHash>
#include <QPolygonF>
#include <cmath>

#include "linespreadpreview.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "linespread.h"
#include "aimsolver.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

LineSpreadPreview::LineSpreadPreview(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("lineSpreadPreview"));
    setMinimumSize(200, 76);
    setToolTip(tr("Where each light lands on the line: 1 is the light nearest the "
                  "line's start. Lights on one point are stacked. The small squares "
                  "along the top are the lights in the order they hang; a line runs "
                  "from each to its point, so lights that cross are seen crossing. "
                  "Lights that move together (Grouping) share a colour and a bracket."));
}

void LineSpreadPreview::setLook(const Scene *scene, const QLCPalette *look)
{
    m_dots.clear();
    m_form = LineFormation::build(m_doc, scene, look);
    for (const LineFormation::Light &l : m_form.lights())
    {
        Dot d;
        d.number = l.number;
        d.fraction = l.fraction;
        d.name = l.name;
        d.group = l.group;
        d.slot = l.slot;
        m_dots << d;
    }
    update();
}

void LineSpreadPreview::setDimmed(bool dimmed)
{
    if (m_dimmed == dimmed)
        return;
    m_dimmed = dimmed;
    update();
}

double LineSpreadPreview::drawScale() const
{
    return qBound(1.0, qMin(width() / 260.0, height() / 84.0), 3.5);
}

void LineSpreadPreview::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_poppable)
        return;
    QMenu menu(this);
    QAction *open = menu.addAction(tr("Open in a window"));
    if (menu.exec(event->globalPos()) == open)
        emit popOutRequested();
}

void LineSpreadPreview::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(30, 32, 38));
    const double s = drawScale();

    // Room for A and B outside the end dots.
    const double left = 26.0 * s, right = width() - 26.0 * s;
    const double base = height() - 16.0 * s;
    const double hangY = 18.0 * s;
    QFont f = p.font();
    f.setPointSizeF(8.0 * s);
    p.setFont(f);

    // Cross chosen but nothing crosses yet: say so, small, top right.
    if (!m_dots.isEmpty() && m_form.crossIdle() && m_form.grouped())
    {
        QFont small = f;
        small.setPointSizeF(7.0 * s);
        p.setFont(small);
        p.setPen(QColor(150, 150, 160));
        p.drawText(QRectF(left, 0, right - left, 12 * s), Qt::AlignRight | Qt::AlignVCenter,
                   tr("Cross acts below Spread 0"));
        p.setFont(f);
    }

    p.setPen(QPen(QColor(255, 180, 0), 2.0 * s));
    p.drawLine(QPointF(left, base), QPointF(right, base));
    p.setPen(QColor(150, 150, 160));
    p.drawText(QRectF(2 * s, base - 8 * s, 14 * s, 16 * s), Qt::AlignCenter, QStringLiteral("A"));
    p.drawText(QRectF(right + 10 * s, base - 8 * s, 14 * s, 16 * s), Qt::AlignCenter, QStringLiteral("B"));
    // The centre: where everything goes at spread 0.
    p.drawLine(QPointF((left + right) / 2, base - 4 * s), QPointF((left + right) / 2, base + 4 * s));

    const bool grouped = m_form.grouped();
    const QList<LineFormation::Light> &lights = m_form.lights();

    // A bracket under the line per group, from its first light to its last.
    if (grouped)
    {
        QHash<int, QPair<double, double> > span;
        for (const LineFormation::Light &l : lights)
        {
            const double x = left + (right - left) * l.fraction;
            if (!span.contains(l.group))
                span.insert(l.group, qMakePair(x, x));
            else
                span[l.group] = qMakePair(qMin(span[l.group].first, x), qMax(span[l.group].second, x));
        }
        const double y = base + 6.0 * s;
        for (auto it = span.constBegin(); it != span.constEnd(); ++it)
        {
            const double a = it.value().first - 4.0 * s, b = it.value().second + 4.0 * s;
            p.setPen(QPen(LineFormation::groupColour(it.key()), 2.0 * s, Qt::SolidLine, Qt::RoundCap));
            QPolygonF br;
            br << QPointF(a, y - 3.0 * s) << QPointF(a, y) << QPointF(b, y) << QPointF(b, y - 3.0 * s);
            p.drawPolyline(br);
        }
    }

    // Stack lights that share a point.
    QMap<int, int> stacked;
    const double r = 7.0 * s;
    QList<QPointF> centres;
    for (const LineFormation::Light &l : lights)
    {
        const double x = left + (right - left) * l.fraction;
        const int key = int(std::lround(x));
        const int level = stacked.value(key, 0);
        stacked[key] = level + 1;
        centres << QPointF(x, base - r - 2.0 * s - level * (2 * r + s));
    }

    // Where each light hangs, in order, and a connector to its point.
    for (int i = 0; i < lights.count(); ++i)
    {
        const LineFormation::Light &l = lights.at(i);
        const QPointF hang(left + (right - left) * l.slot, hangY);
        QColor c = m_form.colourFor(l);
        QColor wire = c;
        wire.setAlpha(170);
        p.setPen(QPen(wire, 1.2 * s));
        const QPointF to = centres.at(i) - QPointF(0, r);
        if (to.y() > hang.y() + 3.0 * s)
            p.drawLine(hang + QPointF(0, 3.0 * s), to);
        p.setPen(QPen(QColor(20, 20, 24), s));
        p.setBrush(grouped ? c : QColor(170, 170, 180));
        p.drawRect(QRectF(hang - QPointF(3 * s, 3 * s), QSizeF(6 * s, 6 * s)));
    }

    for (int i = 0; i < lights.count(); ++i)
    {
        const LineFormation::Light &l = lights.at(i);
        const QPointF c = centres.at(i);
        p.setPen(QPen(QColor(20, 20, 24), s));
        p.setBrush(m_form.colourFor(l));
        p.drawEllipse(c, r, r);
        p.setPen(grouped ? LineFormation::groupInk() : LineFormation::pointInk());
        p.drawText(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), Qt::AlignCenter,
                   QString::number(l.number));
    }
    if (m_dimmed)
        p.fillRect(rect(), QColor(30, 32, 38, 150));
}

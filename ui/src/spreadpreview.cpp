/*
  Q Light Controller Plus
  spreadpreview.cpp

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
#include <cmath>

#include "spreadpreview.h"
#include "doc.h"

SpreadPreview::SpreadPreview(Doc *doc, quint32 sceneId, quint32 paletteId, QWidget *parent)
    : QWidget(parent)
    , m_instance(new EffectInstance(doc, sceneId, paletteId))
{
    setObjectName(QStringLiteral("spreadPreview"));
    setMinimumSize(200, 80);
    setToolTip(tr("The look's heads where they stand, coloured by where each is in the "
                  "effect's cycle: the same colour, the same moment."));
    refresh();
}

SpreadPreview::~SpreadPreview() = default;

void SpreadPreview::refresh()
{
    m_dots = m_instance->spreadPreview(&m_centre, &m_hasCentre);
    update();
}

void SpreadPreview::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(30, 32, 38));
    if (m_dots.isEmpty())
    {
        p.setPen(QColor(150, 150, 160));
        p.drawText(rect(), Qt::AlignCenter, tr("No heads in this look"));
        return;
    }

    /* On the plan when they're placed; otherwise their head-layout cells. */
    bool placed = false;
    foreach (const EffectInstance::SpreadDot &d, m_dots)
        if (!d.plan.isNull())
            placed = true;
    QList<QPointF> at;
    foreach (const EffectInstance::SpreadDot &d, m_dots)
        at << (placed ? QPointF(d.plan.x(), -d.plan.y())       // upstage is up
                      : QPointF(d.col, d.row));
    QPointF centre(m_centre.x(), -m_centre.y());
    QRectF box(at.first(), QSizeF(0, 0));
    foreach (const QPointF &pt, at)
        box |= QRectF(pt, QSizeF(0, 0));
    if (placed && m_hasCentre)
        box |= QRectF(centre, QSizeF(0, 0));
    box = box.normalized();
    const double span = qMax(qMax(box.width(), box.height()), 1e-6);
    const QRectF area = QRectF(rect()).adjusted(10, 10, -10, -10);
    const double scale = qMin(area.width() / qMax(box.width(), span * 0.05),
                              area.height() / qMax(box.height(), span * 0.05));
    auto map = [&](const QPointF &pt) {
        return QPointF(area.center().x() + (pt.x() - box.center().x()) * scale,
                       area.center().y() + (pt.y() - box.center().y()) * scale);
    };

    const double r = qBound(2.0, 0.35 * qMin(area.width(), area.height()) / std::sqrt(double(at.size())), 7.0);
    for (int i = 0; i < at.size(); ++i)
    {
        double f = m_dots.at(i).phase - std::floor(m_dots.at(i).phase);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor::fromHsvF(f, 0.85, 0.95));
        p.drawEllipse(map(at.at(i)), r, r);
    }
    if (placed && m_hasCentre)
    {
        const QPointF c = map(centre);
        p.setPen(QPen(QColor(240, 240, 240), 1.5));
        p.drawLine(c + QPointF(-6, 0), c + QPointF(6, 0));
        p.drawLine(c + QPointF(0, -6), c + QPointF(0, 6));
    }
}

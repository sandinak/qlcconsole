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

#include <QPainter>
#include <QMap>
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
    setMinimumSize(200, 56);
    setToolTip(tr("Where each light lands on the line: 1 is the light nearest the "
                  "line's start. Lights on one point are stacked."));
}

void LineSpreadPreview::setLook(const Scene *scene, const QLCPalette *look)
{
    m_dots.clear();
    MonitorProperties *props = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    const StageTarget *target = (props != nullptr && look != nullptr)
            ? props->stageTarget(look->stageTargetId()) : nullptr;
    if (look != nullptr)
    {
        const LineSpread::Settings s = LineSpread::fromParams(look->effectParamValues());
        QList<quint32> heads;
        if (scene != nullptr && target != nullptr)
            heads = AimSolver::orderedLineHeads(m_doc, target,
                                                AimSolver::aimHeads(m_doc, scene, look->id()),
                                                s.order);
        const int n = heads.isEmpty() ? 6 : heads.size();
        const QVector<double> fr = LineSpread::fractions(n, s);
        for (int i = 0; i < n; ++i)
        {
            Dot d;
            d.number = i + 1;
            d.fraction = fr.at(i);
            if (!heads.isEmpty())
                if (Fixture *fx = m_doc->fixture(heads.at(i)))
                    d.name = fx->name();
            m_dots << d;
        }
    }
    update();
}

void LineSpreadPreview::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(30, 32, 38));

    const double left = 18.0, right = width() - 18.0;
    const double base = height() - 14.0;
    p.setPen(QPen(QColor(255, 180, 0), 2.0));
    p.drawLine(QPointF(left, base), QPointF(right, base));
    p.setPen(QColor(150, 150, 160));
    QFont f = p.font();
    f.setPointSizeF(8.0);
    p.setFont(f);
    p.drawText(QRectF(2, base - 8, 14, 16), Qt::AlignCenter, QStringLiteral("A"));
    p.drawText(QRectF(right + 2, base - 8, 14, 16), Qt::AlignCenter, QStringLiteral("B"));
    // The centre: where everything goes at spread 0.
    p.drawLine(QPointF((left + right) / 2, base - 4), QPointF((left + right) / 2, base + 4));

    // Stack lights that share a point.
    QMap<int, int> stacked;
    const double r = 7.0;
    foreach (const Dot &d, m_dots)
    {
        const double x = left + (right - left) * d.fraction;
        const int key = int(std::lround(x));
        const int level = stacked.value(key, 0);
        stacked[key] = level + 1;
        const QPointF c(x, base - r - 2.0 - level * (2 * r + 1.0));
        p.setPen(QPen(QColor(20, 20, 24), 1.0));
        p.setBrush(QColor(255, 210, 120));
        p.drawEllipse(c, r, r);
        p.setPen(QColor(20, 20, 24));
        p.drawText(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), Qt::AlignCenter,
                   QString::number(d.number));
    }
}

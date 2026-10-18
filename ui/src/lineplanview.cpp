/*
  Q Light Controller Plus
  lineplanview.cpp

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
#include <QRectF>
#include <cmath>

#include "lineplanview.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "linecurve.h"
#include "linespread.h"
#include "aimsolver.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

namespace
{
QPointF plan(const QVector3D &v)
{
    return QPointF(v.x(), v.z());
}
}

LinePlanView::LinePlanView(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("lineplanView"));
    setMinimumSize(200, 140);
    setToolTip(tr("The plan, from above: the line, each light where it hangs and its beam to its point "
                  "(numbered as the strip above). Rings: where the previous cue put the same lights -- "
                  "the beams sweep from there over the cue's fade."));
}

void LinePlanView::setFormation(const Scene *scene, const QLCPalette *look,
                                const Scene *prevScene, const QLCPalette *prevLook)
{
    m_line.clear();
    m_beams.clear();
    MonitorProperties *props = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    const StageTarget *target = (props != nullptr && look != nullptr)
            ? props->stageTarget(look->stageTargetId()) : nullptr;
    if (target == nullptr || !target->isLine() || scene == nullptr)
    {
        update();
        return;
    }
    const LineCurve curve(props->targetLinePoints(target));
    for (const QVector3D &s : curve.samples())
        m_line << plan(s);

    const QList<quint32> heads = AimSolver::aimHeads(m_doc, scene, look->id());
    const LineSpread::Settings settings = LineSpread::fromParams(look->effectParamValues());
    const QList<quint32> ordered = AimSolver::orderedLineHeads(m_doc, target, heads, settings.order);
    const QHash<quint32, QVector3D> now = AimSolver::linePoints(m_doc, look, target, heads);

    // The previous cue's points for the same lights, when it aimed them at
    // a line too.
    QHash<quint32, QVector3D> before;
    if (prevScene != nullptr && prevLook != nullptr)
    {
        const StageTarget *pt = props->stageTarget(prevLook->stageTargetId());
        if (pt != nullptr && pt->isLine())
            before = AimSolver::linePoints(m_doc, prevLook, pt,
                                           AimSolver::aimHeads(m_doc, prevScene, prevLook->id()));
    }

    for (int i = 0; i < ordered.count(); ++i)
    {
        const quint32 fid = ordered.at(i);
        if (!now.contains(fid))
            continue;
        Beam b;
        b.fixture = fid;
        b.number = i + 1;
        if (const Fixture *fx = m_doc->fixture(fid))
            b.name = fx->name();
        AimSolver::Frame frame;
        b.from = AimSolver::frameFor(m_doc, fid, frame) ? plan(frame.position) : plan(now.value(fid));
        b.to = plan(now.value(fid));
        if (before.contains(fid))
        {
            b.hasPrevious = true;
            b.previous = plan(before.value(fid));
        }
        m_beams << b;
    }
    update();
}

void LinePlanView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(30, 32, 38));
    QFont f = p.font();
    f.setPointSizeF(8.0);
    p.setFont(f);
    p.setPen(QColor(120, 120, 130));
    p.drawText(rect().adjusted(6, 4, -6, -4), Qt::AlignTop | Qt::AlignLeft, tr("Plan (top)"));
    if (m_line.isEmpty())
    {
        p.drawText(rect(), Qt::AlignCenter, tr("Not aimed at a line"));
        return;
    }

    // Fit everything, the same scale both ways, at least 2 m each way.
    QRectF box(m_line.first(), QSizeF(0, 0));
    auto grow = [&box](const QPointF &pt) {
        box = box.united(QRectF(pt, QSizeF(0.0001, 0.0001)));
    };
    for (const QPointF &pt : m_line)
        grow(pt);
    for (const Beam &b : m_beams)
    {
        grow(b.from);
        grow(b.to);
        if (b.hasPrevious)
            grow(b.previous);
    }
    const QPointF c = box.center();
    const double w = qMax(2.0, box.width()) * 1.15, h = qMax(2.0, box.height()) * 1.15;
    const QRectF area = QRectF(rect()).adjusted(12, 16, -12, -10);
    const double scale = qMin(area.width() / w, area.height() / h);
    auto toScreen = [&](const QPointF &pt) {
        return QPointF(area.center().x() + (pt.x() - c.x()) * scale,
                       area.center().y() + (pt.y() - c.y()) * scale);
    };

    // The line, its ends A and B as the strip names them.
    QPolygonF poly;
    for (const QPointF &pt : m_line)
        poly << toScreen(pt);
    p.setPen(QPen(QColor(255, 180, 0), 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(poly);
    p.setPen(QColor(150, 150, 160));
    p.drawText(QRectF(poly.first() - QPointF(14, 14), QSizeF(12, 12)), Qt::AlignCenter, QStringLiteral("A"));
    p.drawText(QRectF(poly.last() + QPointF(2, -14), QSizeF(12, 12)), Qt::AlignCenter, QStringLiteral("B"));

    const double r = 6.0;
    // The previous cue's points: rings, and the way each beam sweeps.
    for (const Beam &b : m_beams)
    {
        if (!b.hasPrevious)
            continue;
        const QPointF was = toScreen(b.previous), is = toScreen(b.to);
        p.setPen(QPen(QColor(140, 170, 255, 170), 1.0, Qt::DashLine));
        if (QLineF(was, is).length() > 1.0)
            p.drawLine(was, is);
        p.setPen(QPen(QColor(140, 170, 255), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(was, r + 2, r + 2);
    }
    // Lights, beams, points.
    for (const Beam &b : m_beams)
    {
        const QPointF from = toScreen(b.from), to = toScreen(b.to);
        p.setPen(QPen(QColor(255, 230, 160, 90), 1.0));
        p.drawLine(from, to);
        p.setPen(QPen(QColor(20, 20, 24), 1.0));
        p.setBrush(QColor(170, 170, 180));
        p.drawRect(QRectF(from - QPointF(3, 3), QSizeF(6, 6)));
        p.setBrush(QColor(255, 210, 120));
        p.drawEllipse(to, r, r);
        p.setPen(QColor(20, 20, 24));
        p.drawText(QRectF(to.x() - r, to.y() - r, 2 * r, 2 * r), Qt::AlignCenter, QString::number(b.number));
    }
}

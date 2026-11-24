/*
  Q Light Controller Plus
  planmapwidget.cpp

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

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

#include "planmapwidget.h"
#include "monitorproperties.h"
#include "measurementunits.h"

static const int kMargin = 8;
static const int kCaptionH = 16;

PlanMapWidget::PlanMapWidget(QWidget *parent)
    : QWidget(parent)
    , m_stage(0, 0, 9000, 6000)
    , m_clX(4500)
    , m_plasterY(6000)
{
    setObjectName(QStringLiteral("planMap"));
    setFixedSize(260, 160);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMouseTracking(false);
    setToolTip(tr("Top-down plan: US at the top, the centre line (CL) and the plaster line "
                  "dashed. X and Y are measured from the plot origin; the dimension lines "
                  "read from CL and the plaster line, as \"On stage\" does. Drag the dot to "
                  "place it (Shift: free of the grid)."));
}

void PlanMapWidget::setStage(const QRectF &stageMm, double centreLineXmm, double plasterLineYmm)
{
    m_stage = stageMm;
    m_clX = centreLineXmm;
    m_plasterY = plasterLineYmm;
    update();
}

void PlanMapWidget::setFromProperties(const MonitorProperties *props)
{
    if (props == nullptr)
        return;
    const double mpu = props->gridUnitMetres();
    const QVector3D g = props->gridSize();
    const double w = g.x() * mpu * 1000.0, d = g.z() * mpu * 1000.0;
    m_stage = QRectF(0, 0, w, d);
    m_clX = props->stageCentre().x() * 1000.0;
    m_plasterY = props->curtainLineY() * 1000.0;
    m_origin = props->stageOrigin() * 1000.0;
    m_grid = mpu * 1000.0;
    update();
}

void PlanMapWidget::setOrigin(const QPointF &worldMm)
{
    m_origin = worldMm;
    update();
}

void PlanMapWidget::setPosition(double xMm, double yMm)
{
    m_pos = QPointF(xMm, yMm);
    update();
}

void PlanMapWidget::setGridStep(double mm)
{
    m_grid = qMax(0.0, mm);
}

void PlanMapWidget::setEditable(bool editable, const QString &reason)
{
    m_editable = editable;
    m_reason = editable ? QString() : reason;
    setCursor(editable ? Qt::OpenHandCursor : Qt::ArrowCursor);
    if (!editable && !reason.isEmpty())
        setToolTip(reason);
    update();
}

QString PlanMapWidget::centreLineText() const
{
    const double dx = worldPos().x() - m_clX;
    const double d = qAbs(dx);
    if (d < 0.5)
        return tr("on CL");
    /* Stage right and left are the performer's, facing the house: the plot
       draws SR on its left, so less than the centre line is SR (as the
       rulers and the readout say it). */
    return (dx < 0 ? tr("%1 SR of CL") : tr("%1 SL of CL")).arg(MeasurementUnits::formatLength(d));
}

QString PlanMapWidget::plasterLineText() const
{
    const double d = qAbs(worldPos().y() - m_plasterY);
    if (d < 0.5)
        return tr("on plaster line");
    return tr("%1 from plaster line").arg(MeasurementUnits::formatLength(d));
}

QString PlanMapWidget::captionText() const
{
    if (!m_editable && !m_reason.isEmpty())
        return m_reason;
    return tr("X / Y count from the plot origin (+)");
}

QRectF PlanMapWidget::mapRect() const
{
    return QRectF(kMargin, kMargin, width() - 2 * kMargin,
                  height() - 2 * kMargin - kCaptionH);
}

QRectF PlanMapWidget::viewWorld() const
{
    QRectF r = m_stage.normalized();
    const QPointF w = worldPos();
    r = r.united(QRectF(w, QSizeF(1, 1)));
    r = r.united(QRectF(m_origin, QSizeF(1, 1)));
    if (r.width() < 1) r.setWidth(1);
    if (r.height() < 1) r.setHeight(1);
    return r;
}

QPointF PlanMapWidget::toPixels(const QPointF &w) const
{
    const QRectF v = viewWorld(), m = mapRect();
    const double s = qMin(m.width() / v.width(), m.height() / v.height());
    const QPointF c = m.center(), vc = v.center();
    return QPointF(c.x() + (w.x() - vc.x()) * s, c.y() + (w.y() - vc.y()) * s);
}

QPointF PlanMapWidget::toWorld(const QPointF &p) const
{
    const QRectF v = viewWorld(), m = mapRect();
    const double s = qMin(m.width() / v.width(), m.height() / v.height());
    const QPointF c = m.center(), vc = v.center();
    return QPointF(vc.x() + (p.x() - c.x()) / s, vc.y() + (p.y() - c.y()) / s);
}

QPointF PlanMapWidget::dotPoint() const
{
    return toPixels(worldPos());
}

void PlanMapWidget::dragTo(const QPointF &px, bool free)
{
    QPointF w = toWorld(px);
    QPointF p = w - m_origin;
    if (!free && m_grid > 0.0) {
        p.setX(qRound64(p.x() / m_grid) * m_grid);
        p.setY(qRound64(p.y() / m_grid) * m_grid);
    } else {
        p.setX(qRound64(p.x()));
        p.setY(qRound64(p.y()));
    }
    // Stay inside the stage (a little beyond its edge reads as "off stage").
    const QRectF s = m_stage.normalized();
    const QPointF lo = s.topLeft() - m_origin, hi = s.bottomRight() - m_origin;
    p.setX(qBound(lo.x(), p.x(), hi.x()));
    p.setY(qBound(lo.y(), p.y(), hi.y()));
    if (p == m_pos)
        return;
    m_pos = p;
    update();
    emit positionChanged(p.x(), p.y());
}

void PlanMapWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !m_editable)
        return;
    m_dragging = true;
    setCursor(Qt::ClosedHandCursor);
    dragTo(e->position(), e->modifiers() & Qt::ShiftModifier);
}

void PlanMapWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging)
        dragTo(e->position(), e->modifiers() & Qt::ShiftModifier);
}

void PlanMapWidget::mouseReleaseEvent(QMouseEvent *)
{
    if (m_dragging) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
    }
}

void PlanMapWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0x1c, 0x1e, 0x26));
    p.setPen(QColor(0x3a, 0x3e, 0x4c));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    QFont f = p.font();
    f.setPixelSize(10);
    p.setFont(f);
    const QFontMetrics fm(f);

    // Stage outline.
    const QRectF st = QRectF(toPixels(m_stage.normalized().topLeft()),
                             toPixels(m_stage.normalized().bottomRight()));
    p.setPen(QPen(QColor(0x6a, 0x70, 0x86), 1));
    p.setBrush(QColor(0x24, 0x27, 0x32));
    p.drawRect(st);

    // Centre line and plaster line.
    QPen dash(QColor(0x7d, 0xa8, 0xd8), 1, Qt::DashLine);
    p.setPen(dash);
    const double clx = toPixels(QPointF(m_clX, 0)).x();
    const double ply = toPixels(QPointF(0, m_plasterY)).y();
    p.drawLine(QPointF(clx, st.top()), QPointF(clx, st.bottom()));
    p.setPen(QPen(QColor(0xd8, 0xb0, 0x6a), 1, Qt::DashLine));
    p.drawLine(QPointF(st.left(), ply), QPointF(st.right(), ply));
    p.setPen(QColor(0x7d, 0xa8, 0xd8));
    p.drawText(QPointF(clx + 3, st.top() + 10), QStringLiteral("CL"));
    p.setPen(QColor(0xd8, 0xb0, 0x6a));
    p.drawText(QPointF(st.left() + 3, ply - 3), tr("PL"));
    p.setPen(QColor(0x80, 0x84, 0x94));
    p.drawText(QPointF(st.right() - fm.horizontalAdvance(tr("US")) - 3, st.top() + 10), tr("US"));
    p.drawText(QPointF(st.right() - fm.horizontalAdvance(tr("DS")) - 3, st.bottom() - 3), tr("DS"));

    // The plot origin: a plus.
    const QPointF o = toPixels(m_origin);
    p.setPen(QPen(QColor(0xe0, 0xe0, 0xe8), 1.5));
    p.drawLine(o + QPointF(-4, 0), o + QPointF(4, 0));
    p.drawLine(o + QPointF(0, -4), o + QPointF(0, 4));

    // Dimension lines from the dot to CL and the plaster line.
    const QPointF d = dotPoint();
    p.setPen(QPen(QColor(0x7d, 0xa8, 0xd8), 1));
    p.drawLine(d, QPointF(clx, d.y()));
    p.setPen(QPen(QColor(0xd8, 0xb0, 0x6a), 1));
    p.drawLine(d, QPointF(d.x(), ply));

    auto label = [&](const QString &t, QPointF at, const QColor &c) {
        const QRectF r(at, QSizeF(fm.horizontalAdvance(t) + 4, fm.height()));
        QRectF q = r;
        q.moveLeft(qBound(2.0, q.left(), width() - q.width() - 2.0));
        q.moveTop(qBound(2.0, q.top(), double(height() - kCaptionH - q.height())));
        p.fillRect(q, QColor(0x1c, 0x1e, 0x26, 220));
        p.setPen(c);
        p.drawText(q, Qt::AlignCenter, t);
    };
    const double midX = (d.x() + clx) / 2;
    label(centreLineText(), QPointF(midX - fm.horizontalAdvance(centreLineText()) / 2 - 2, d.y() - fm.height() - 1),
          QColor(0xa8, 0xc8, 0xee));
    const double midY = (d.y() + ply) / 2;
    label(plasterLineText(), QPointF(d.x() + 4, midY - fm.height() / 2), QColor(0xee, 0xcc, 0x8a));

    // The fixture.
    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(m_editable ? QColor(0x3c, 0xc8, 0x6a) : QColor(0x80, 0x84, 0x94));
    p.drawEllipse(d, 5.0, 5.0);

    // Caption.
    p.setPen(m_editable ? QColor(0x98, 0x9c, 0xac) : QColor(0xe8, 0xb4, 0x6a));
    p.drawText(QRectF(kMargin, height() - kCaptionH - 2, width() - 2 * kMargin, kCaptionH),
               Qt::AlignVCenter | Qt::AlignLeft,
               fm.elidedText(captionText(), Qt::ElideRight, width() - 2 * kMargin));
}

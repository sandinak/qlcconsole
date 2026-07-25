/*
  Q Light Controller Plus
  studioplaneview.cpp

  Copyright (c) Massimo Callegari

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
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QPolygonF>
#include <QtMath>

#include "studioplaneview.h"
#include "doc.h"
#include "fixture.h"
#include "truss.h"
#include "stageplatform.h"
#include "monitorproperties.h"

static const int kDotRadius = 7;

StudioPlaneView::StudioPlaneView(Doc *doc, quint32 groupId, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_groupId(groupId)
{
    setMinimumHeight(220);
    setMouseTracking(false);
    setAutoFillBackground(true);
}

void StudioPlaneView::setPlane(Plane p)
{
    if (m_plane == p)
        return;
    m_plane = p;
    refit();
    update();
}

void StudioPlaneView::reload()
{
    refit();
    update();
}

QList<quint32> StudioPlaneView::members() const
{
    QList<quint32> out;
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (quint32 fid, props->fixtureItemsID())
        if (props->fixtureFrameGroup(fid) == m_groupId)
            out << fid;
    std::sort(out.begin(), out.end());
    return out;
}

// Vertical sign: Top's second axis (Y) runs DOWN the screen; the elevation
// planes' second axis (Z, height) runs UP.
static inline double vFactor(StudioPlaneView::Plane p)
{
    return (p == StudioPlaneView::Top) ? 1.0 : -1.0;
}

QPointF StudioPlaneView::project(const QVector3D &l) const
{
    switch (m_plane)
    {
    case Front: return QPointF(l.x(), l.z());
    case Side:  return QPointF(l.y(), l.z());
    case Top:
    default:    return QPointF(l.x(), l.y());
    }
}

QVector3D StudioPlaneView::unproject(const QPointF &ab, const QVector3D &prev) const
{
    QVector3D l = prev;
    switch (m_plane)
    {
    case Front: l.setX(float(ab.x())); l.setZ(float(ab.y())); break;
    case Side:  l.setY(float(ab.x())); l.setZ(float(ab.y())); break;
    case Top:
    default:    l.setX(float(ab.x())); l.setY(float(ab.y())); break;
    }
    return l;
}

QPointF StudioPlaneView::worldToScreen(const QPointF &ab) const
{
    return QPointF(m_originPx.x() + ab.x() * m_scale,
                   m_originPx.y() + vFactor(m_plane) * ab.y() * m_scale);
}

QPointF StudioPlaneView::screenToWorld(const QPointF &px) const
{
    return QPointF((px.x() - m_originPx.x()) / m_scale,
                   (px.y() - m_originPx.y()) / (vFactor(m_plane) * m_scale));
}

void StudioPlaneView::refit()
{
    const QList<quint32> ids = members();
    MonitorProperties *props = m_doc->monitorProperties();

    double minA = 0, maxA = 0, minB = 0, maxB = 0;   // always include origin
    foreach (quint32 fid, ids)
    {
        const QPointF ab = project(props->fixtureRigProps(fid).groupLocal);
        minA = qMin(minA, ab.x()); maxA = qMax(maxA, ab.x());
        minB = qMin(minB, ab.y()); maxB = qMax(maxB, ab.y());
    }
    const double spanA = qMax(maxA - minA, 0.5);
    const double spanB = qMax(maxB - minB, 0.5);
    const double margin = 36.0;
    const double availW = qMax(1.0, width()  - 2 * margin);
    const double availH = qMax(1.0, height() - 2 * margin);
    m_scale = qBound(12.0, qMin(availW / spanA, availH / spanB), 300.0);

    const double cA = (minA + maxA) / 2.0;
    const double cB = (minB + maxB) / 2.0;
    m_originPx = QPointF(width() / 2.0 - cA * m_scale,
                         height() / 2.0 - vFactor(m_plane) * cB * m_scale);
}

void StudioPlaneView::resizeEvent(QResizeEvent *)
{
    refit();
}

quint32 StudioPlaneView::hitTest(const QPointF &px) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    quint32 best = 0;
    double bestD = (kDotRadius + 4) * (kDotRadius + 4);
    foreach (quint32 fid, members())
    {
        const QPointF s = worldToScreen(project(props->fixtureRigProps(fid).groupLocal));
        const double dx = s.x() - px.x(), dy = s.y() - px.y();
        const double d = dx * dx + dy * dy;
        if (d <= bestD) { bestD = d; best = fid; }
    }
    return best;
}

void StudioPlaneView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(30, 32, 36));

    // --- grid ---------------------------------------------------------------
    // Choose a "nice" grid step (metres) so lines are ~>= 24 px apart.
    double step = 0.1;
    while (step * m_scale < 24.0) step *= (qFuzzyCompare(step * 10, qRound(step * 10)) ? 2.0 : 2.5);
    QPen grid(QColor(52, 55, 60)); grid.setWidth(1);
    p.setPen(grid);
    // vertical grid lines
    const double leftM  = screenToWorld(QPointF(0, 0)).x();
    const double rightM = screenToWorld(QPointF(width(), 0)).x();
    for (double a = qCeil(leftM / step) * step; a <= rightM; a += step)
    {
        const double x = worldToScreen(QPointF(a, 0)).x();
        p.drawLine(QPointF(x, 0), QPointF(x, height()));
    }
    // horizontal grid lines
    const double topM = screenToWorld(QPointF(0, 0)).y();
    const double botM = screenToWorld(QPointF(0, height())).y();
    const double loB = qMin(topM, botM), hiB = qMax(topM, botM);
    for (double b = qCeil(loB / step) * step; b <= hiB; b += step)
    {
        const double y = worldToScreen(QPointF(0, b)).y();
        p.drawLine(QPointF(0, y), QPointF(width(), y));
    }

    // --- origin axes --------------------------------------------------------
    QPen axis(QColor(90, 95, 105)); axis.setWidth(1);
    p.setPen(axis);
    p.drawLine(QPointF(0, m_originPx.y()), QPointF(width(), m_originPx.y()));
    p.drawLine(QPointF(m_originPx.x(), 0), QPointF(m_originPx.x(), height()));

    // --- reference structure (platforms / trusses) --------------------------
    drawStructure(p);

    // --- plane legend (stage-relative) --------------------------------------
    p.setPen(QColor(150, 155, 165));
    QString legend = (m_plane == Top)   ? tr("Plan  ·  stage width →   downstage ↓")
                   : (m_plane == Front) ? tr("Front elevation  ·  stage width →   height ↑")
                                        : tr("Side elevation  ·  downstage →   height ↑");
    const float frot = m_doc->monitorProperties()->group(m_groupId).rotation;
    if (qAbs(frot) > 0.5f)
        legend += tr("   (frame rotated %1°)").arg(double(frot), 0, 'f', 0);
    p.drawText(8, 18, legend);

    // --- members (drawn as LED bars, not points) ----------------------------
    foreach (quint32 fid, members())
        drawFixture(p, fid);
}

void StudioPlaneView::drawFixture(QPainter &p, quint32 fid) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    Fixture *fx = m_doc->fixture(fid);
    const int heads = (fx != nullptr) ? qMax(1, fx->heads()) : 1;
    const QPointF cAB = project(props->fixtureRigProps(fid).groupLocal);
    const bool drag = (fid == m_dragFid);

    // A fixture is a line of heads. Approximate its physical extent from the head
    // count and orient it (Top only) by its yaw relative to the frame; elevation
    // planes draw it along the horizontal axis. ~5 cm per head.
    const double halfLen = qMax(0.10, (heads - 1) * 0.05 / 2.0);   // metres
    double th = 0.0;
    if (m_plane == Top)
        th = qDegreesToRadians(double(props->fixtureRotation(fid, 0, 0).z()
                                      - props->group(m_groupId).rotation));
    const QPointF dir(qCos(th), qSin(th));
    const QPointF a = cAB - dir * halfLen;
    const QPointF b = cAB + dir * halfLen;

    QColor col = props->fixtureGelColor(fid, 0, 0);
    if (!col.isValid() || col == Qt::black)
        col = QColor(90, 160, 235);
    if (drag)
        col = QColor(255, 196, 64);

    // The bar body.
    QPen body(col.darker(140)); body.setWidth(drag ? 4 : 3); body.setCapStyle(Qt::RoundCap);
    p.setPen(body);
    p.drawLine(worldToScreen(a), worldToScreen(b));

    // Individual heads as ticks along it.
    p.setPen(Qt::NoPen);
    p.setBrush(col);
    for (int i = 0; i < heads; ++i)
    {
        const double t = (heads > 1) ? double(i) / (heads - 1) : 0.5;
        const QPointF hpt = worldToScreen(a + (b - a) * t);
        p.drawEllipse(hpt, 2.6, 2.6);
    }

    // Label near the centre.
    if (fx != nullptr)
    {
        p.setPen(QColor(210, 214, 220));
        const QPointF c = worldToScreen(cAB);
        p.drawText(QPointF(c.x() + 8, c.y() - 6), fx->name());
    }
}

void StudioPlaneView::drawStructure(QPainter &p) const
{
    MonitorProperties *props = m_doc->monitorProperties();

    // Platforms: draw the face relevant to the current plane, transformed into
    // the group's LOCAL frame, so you can place fixtures against the riser.
    foreach (StagePlatform *pl, props->platforms())
    {
        if (pl == nullptr)
            continue;
        QColor pc = pl->color();
        if (!pc.isValid())
            pc = QColor(150, 135, 85);
        p.setPen(QPen(pc.darker(115), 1.5));
        p.setBrush(QColor(pc.red(), pc.green(), pc.blue(), 70));
        const float ox = pl->originX(), oy = pl->originY();
        const float w = pl->width(), d = pl->depth(), h = pl->height();
        QVector<QVector3D> face;
        switch (m_plane)
        {
        case Front:   // downstage face (y = oy+d): X across, Z up
            face = QVector<QVector3D>{ QVector3D(ox, oy + d, 0), QVector3D(ox + w, oy + d, 0),
                                       QVector3D(ox + w, oy + d, h), QVector3D(ox, oy + d, h) };
            break;
        case Side:    // stage-right face (x = ox): Y across, Z up
            face = QVector<QVector3D>{ QVector3D(ox, oy, 0), QVector3D(ox, oy + d, 0),
                                       QVector3D(ox, oy + d, h), QVector3D(ox, oy, h) };
            break;
        case Top:
        default:      // footprint (z = 0)
            face = QVector<QVector3D>{ QVector3D(ox, oy, 0), QVector3D(ox + w, oy, 0),
                                       QVector3D(ox + w, oy + d, 0), QVector3D(ox, oy + d, 0) };
            break;
        }
        QPolygonF poly;
        foreach (const QVector3D &c, face)
            poly << worldToScreen(project(props->worldToGroupLocal(m_groupId, c)));
        p.drawPolygon(poly);
    }

    // Trusses: a line from end to end (in the local frame).
    p.setPen(QPen(QColor(95, 140, 165), 3));
    foreach (Truss *t, props->trusses())
    {
        if (t == nullptr)
            continue;
        const QVector3D a = props->worldToGroupLocal(m_groupId, t->positionAt(0));
        const QVector3D b = props->worldToGroupLocal(m_groupId, t->positionAt(t->length()));
        p.drawLine(worldToScreen(project(a)), worldToScreen(project(b)));
    }
}

void StudioPlaneView::wheelEvent(QWheelEvent *e)
{
    const double f = (e->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
    const QPointF cur = e->position();
    const QPointF w = screenToWorld(cur);         // world point under the cursor
    m_scale = qBound(4.0, m_scale * f, 2000.0);
    // Keep that world point pinned under the cursor after the zoom.
    m_originPx = QPointF(cur.x() - w.x() * m_scale,
                         cur.y() - vFactor(m_plane) * w.y() * m_scale);
    update();
    e->accept();
}

void StudioPlaneView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_dragFid = hitTest(e->pos());
    if (m_dragFid != 0)
        update();
}

void StudioPlaneView::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragFid == 0)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(m_dragFid);
    const QPointF ab = screenToWorld(e->pos());
    rp.groupLocal = unproject(ab, rp.groupLocal);
    props->setFixtureRigProps(m_dragFid, rp);
    m_doc->setModified();
    emit memberMoved(m_dragFid);
    emit changed();
    update();
}

void StudioPlaneView::mouseReleaseEvent(QMouseEvent *)
{
    if (m_dragFid != 0)
    {
        m_dragFid = 0;
        update();
    }
}

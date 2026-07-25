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
#include "qlcfixturemode.h"
#include "qlcphysical.h"
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

double StudioPlaneView::fixtureLenM(quint32 fid) const
{
    // Real physical length of the fixture (metres), matching the old Face
    // editor: the declared physical width, else a 0.3 m default (NOT a
    // per-head guess — a 64-cell LED tape is not 3 m long).
    Fixture *fx = m_doc->fixture(fid);
    if (fx != nullptr)
    {
        const QLCFixtureMode *mode = fx->fixtureMode();
        if (mode != nullptr && mode->physical().width() > 0)
            return mode->physical().width() / 1000.0;
    }
    return 0.3;
}

StagePlatform *StudioPlaneView::anchorPlatform() const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const MonitorProperties::MonitorGroup g = props->group(m_groupId);
    if (g.anchorKind == QStringLiteral("platform"))
        return props->platform(g.anchorId);
    return nullptr;
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
    auto include = [&](const QPointF &ab) {
        minA = qMin(minA, ab.x()); maxA = qMax(maxA, ab.x());
        minB = qMin(minB, ab.y()); maxB = qMax(maxB, ab.y());
    };
    foreach (quint32 fid, ids)
        include(project(props->fixtureRigProps(fid).groupLocal));

    // Frame the anchor feature too, so the view is scoped to what you're editing.
    if (StagePlatform *pl = anchorPlatform())
    {
        const float ox = pl->originX(), oy = pl->originY();
        const float w = pl->width(), d = pl->depth(), h = pl->height();
        for (int i = 0; i < 8; ++i)
        {
            const QVector3D corner(ox + ((i & 1) ? w : 0),
                                   oy + ((i & 2) ? d : 0),
                                   (i & 4) ? h : 0);
            include(project(props->worldToGroupLocal(m_groupId, corner)));
        }
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

// Distance (squared) from point p to segment a-b, in screen space.
static double distToSegSq(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    double t = 0.0;
    if (len2 > 1e-9)
        t = qBound(0.0, ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2, 1.0);
    const QPointF proj = a + ab * t;
    const double dx = p.x() - proj.x(), dy = p.y() - proj.y();
    return dx * dx + dy * dy;
}

quint32 StudioPlaneView::hitTest(const QPointF &px) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    quint32 best = 0;
    double bestD = (kDotRadius + 5) * (kDotRadius + 5);
    // Hit the whole LED bar, not just its centre, so wide fixtures (and the
    // horizontal bars drawn in the elevation views) are easy to grab.
    foreach (quint32 fid, members())
    {
        const QPointF cAB = project(props->fixtureRigProps(fid).groupLocal);
        const double halfLen = qMax(0.05, fixtureLenM(fid) / 2.0);
        double th = 0.0;
        if (m_plane == Top)
            th = qDegreesToRadians(double(props->fixtureRotation(fid, 0, 0).z()
                                          - props->group(m_groupId).rotation));
        const QPointF dir(qCos(th), qSin(th));
        const QPointF a = worldToScreen(cAB - dir * halfLen);
        const QPointF b = worldToScreen(cAB + dir * halfLen);
        const double d = distToSegSq(px, a, b);
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

    // A fixture is a line of heads spanning its real physical length. Orient it
    // (Top only) by its yaw relative to the frame; elevation planes draw it along
    // the horizontal axis.
    const double halfLen = qMax(0.05, fixtureLenM(fid) / 2.0);   // metres
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
    const MonitorProperties::MonitorGroup g = props->group(m_groupId);

    // Draw ONLY the feature this group is anchored to (the platform/truss you
    // are working on), transformed into the local frame — not the whole stage.
    if (StagePlatform *pl = anchorPlatform())
    {
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
    else if (g.anchorKind == QStringLiteral("truss"))
    {
        if (Truss *t = props->truss(g.anchorId))
        {
            p.setPen(QPen(QColor(95, 140, 165), 3));
            const QVector3D a = props->worldToGroupLocal(m_groupId, t->positionAt(0));
            const QVector3D b = props->worldToGroupLocal(m_groupId, t->positionAt(t->length()));
            p.drawLine(worldToScreen(project(a)), worldToScreen(project(b)));
        }
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
    {
        update();
    }
    else
    {
        // Clicked empty space → pan the view.
        m_panning = true;
        m_panLast = e->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void StudioPlaneView::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragFid != 0)
    {
        MonitorProperties *props = m_doc->monitorProperties();
        FixtureRigProps rp = props->fixtureRigProps(m_dragFid);
        const QPointF ab = screenToWorld(e->pos());
        QVector3D local = unproject(ab, rp.groupLocal);

        // Keep fixtures within the feature bounds. For a platform-slaved frame
        // the local axes ARE the platform extent: x∈[0,width], y∈[0,depth],
        // z∈[0,height].
        if (StagePlatform *pl = anchorPlatform())
        {
            local.setX(qBound(0.0f, local.x(), pl->width()));
            local.setY(qBound(0.0f, local.y(), pl->depth()));
            local.setZ(qBound(0.0f, local.z(), pl->height()));
        }
        rp.groupLocal = local;
        props->setFixtureRigProps(m_dragFid, rp);
        m_doc->setModified();
        emit memberMoved(m_dragFid);
        emit changed();
        update();
    }
    else if (m_panning)
    {
        m_originPx += (e->pos() - m_panLast);
        m_panLast = e->pos();
        update();
    }
}

void StudioPlaneView::mouseReleaseEvent(QMouseEvent *)
{
    if (m_dragFid != 0 || m_panning)
    {
        m_dragFid = 0;
        m_panning = false;
        unsetCursor();
        update();
    }
}

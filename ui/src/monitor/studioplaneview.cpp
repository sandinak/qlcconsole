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
#include <QtMath>

#include "studioplaneview.h"
#include "doc.h"
#include "fixture.h"
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

    // --- plane legend -------------------------------------------------------
    p.setPen(QColor(150, 155, 165));
    const char *legend = (m_plane == Top)   ? "Top  ·  X →   Y ↓"
                       : (m_plane == Front) ? "Front  ·  X →   Z ↑"
                                            : "Side  ·  Y →   Z ↑";
    p.drawText(8, 18, QString::fromLatin1(legend));

    // --- members ------------------------------------------------------------
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (quint32 fid, members())
    {
        const QPointF s = worldToScreen(project(props->fixtureRigProps(fid).groupLocal));
        const bool drag = (fid == m_dragFid);
        p.setBrush(drag ? QColor(255, 196, 64) : QColor(90, 160, 235));
        p.setPen(QPen(QColor(20, 22, 25), drag ? 2 : 1));
        p.drawEllipse(s, kDotRadius, kDotRadius);

        Fixture *fx = m_doc->fixture(fid);
        if (fx != nullptr)
        {
            p.setPen(QColor(210, 214, 220));
            p.drawText(QPointF(s.x() + kDotRadius + 3, s.y() + 4), fx->name());
        }
    }
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

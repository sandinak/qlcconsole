/*
  Q Light Controller Plus
  structurestudioview.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>

#include "structurestudioview.h"
#include "monitorproperties.h"
#include "pipe.h"
#include "stand.h"
#include "tower.h"
#include "truss.h"
#include "fixture.h"
#include "doc.h"

StructureStudioView::StructureStudioView(Doc *doc, Kind kind, quint32 id, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_kind(kind)
    , m_id(id)
{
    setMinimumSize(360, 320);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    // Top plane reads best for a tower footprint; a boom/truss reads best in a
    // vertical elevation. Default Front (a side rig reads at a glance).
    m_plane = (kind == TowerKind) ? Top : Front;
}

void StructureStudioView::setPlane(Plane p)
{
    if (m_plane == p)
        return;
    m_plane = p;
    refit();
    update();
}

void StructureStudioView::reload()
{
    refit();
    update();
}

/*********************************************************************
 * Projection
 *********************************************************************/

QPointF StructureStudioView::project(const QVector3D &w) const
{
    switch (m_plane)
    {
    case Front: return QPointF(w.x(), w.z());
    case Side:  return QPointF(w.y(), w.z());
    case Top:
    default:    return QPointF(w.x(), w.y());
    }
}

QPointF StructureStudioView::w2s(const QVector3D &w) const
{
    const QPointF ab = project(w);
    const double vSign = (m_plane == Top) ? 1.0 : -1.0;   // Top: Y screen-down; else Z up
    return QPointF(m_originPx.x() + ab.x() * m_scale,
                   m_originPx.y() + vSign * ab.y() * m_scale);
}

/*********************************************************************
 * Structure gathering
 *********************************************************************/

QList<const Pipe *> StructureStudioView::standPipes() const
{
    QList<const Pipe *> out;
    if (m_kind != StandKind)
        return out;
    MonitorProperties *props = m_doc->monitorProperties();
    QList<quint32> booms;
    foreach (Pipe *p, props->pipes())
        if (p->standId() == m_id)
        {
            out << p;
            booms << p->id();
        }
    // Crossbars hung on any of those booms.
    foreach (Pipe *p, props->pipes())
        if (p->isBarOnPipe() && booms.contains(p->parentPipeId()) && !out.contains(p))
            out << p;
    return out;
}

QList<quint32> StructureStudioView::mountedFixtures() const
{
    QList<quint32> out;
    MonitorProperties *props = m_doc->monitorProperties();

    QList<quint32> pipeIds;
    if (m_kind == StandKind)
        foreach (const Pipe *p, standPipes())
            pipeIds << p->id();

    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr)
            continue;
        const FixtureRigProps &rp = props->fixtureRigProps(fx->id());
        bool on = false;
        if (m_kind == StandKind)
            on = (rp.pipeId != Pipe::invalidId() && pipeIds.contains(rp.pipeId));
        else if (m_kind == TowerKind)
            on = (rp.towerId == m_id);
        else if (m_kind == TrussKind)
            on = (rp.trussId == m_id);
        if (on)
            out << fx->id();
    }
    return out;
}

void StructureStudioView::collectPoints(QList<QVector3D> &pts) const
{
    MonitorProperties *props = m_doc->monitorProperties();

    if (m_kind == StandKind)
    {
        if (Stand *s = props->stand(m_id))
        {
            pts << QVector3D(s->originX(), s->originY(), 0.0f);
            pts << s->topPos();
            const float br = s->baseRadius();
            pts << QVector3D(s->originX() - br, s->originY() - br, 0.0f);
            pts << QVector3D(s->originX() + br, s->originY() + br, 0.0f);
        }
        foreach (const Pipe *p, standPipes())
        {
            pts << p->positionAt(0.0f);
            pts << p->positionAt(p->length());
        }
    }
    else if (m_kind == TowerKind)
    {
        if (Tower *t = props->tower(m_id))
        {
            pts << QVector3D(t->originX(), t->originY(), 0.0f);
            pts << QVector3D(t->originX() + t->width(), t->originY() + t->depth(), t->height());
        }
    }
    else if (m_kind == TrussKind)
    {
        if (Truss *t = props->truss(m_id))
        {
            pts << t->origin();
            pts << t->positionAt(t->length());
        }
    }

    foreach (quint32 fid, mountedFixtures())
        pts << props->fixtureRigPosition(fid);
}

void StructureStudioView::refit()
{
    QList<QVector3D> pts;
    collectPoints(pts);

    double minA = 0, maxA = 0, minB = 0, maxB = 0;
    bool first = true;
    foreach (const QVector3D &w, pts)
    {
        const QPointF ab = project(w);
        if (first) { minA = maxA = ab.x(); minB = maxB = ab.y(); first = false; }
        minA = qMin(minA, ab.x()); maxA = qMax(maxA, ab.x());
        minB = qMin(minB, ab.y()); maxB = qMax(maxB, ab.y());
    }
    if (first) { minA = maxA = minB = maxB = 0; }

    const double spanA = qMax(maxA - minA, 0.6);
    const double spanB = qMax(maxB - minB, 0.6);
    const double margin = 42.0;
    const double availW = qMax(1.0, width()  - 2 * margin);
    const double availH = qMax(1.0, height() - 2 * margin);
    m_scale = qBound(10.0, qMin(availW / spanA, availH / spanB), 320.0);

    const double cA = (minA + maxA) / 2.0;
    const double cB = (minB + maxB) / 2.0;
    const double vSign = (m_plane == Top) ? 1.0 : -1.0;
    m_originPx = QPointF(width() / 2.0 - cA * m_scale,
                         height() / 2.0 - vSign * cB * m_scale);
}

/*********************************************************************
 * Paint
 *********************************************************************/

void StructureStudioView::drawGrid(QPainter &p) const
{
    p.fillRect(rect(), QColor(24, 26, 32));

    // Floor line at Z=0 in the elevations; a faint origin cross in Top.
    p.setPen(QPen(QColor(70, 74, 84), 1.0));
    if (m_plane == Top)
    {
        const QPointF o = m_originPx;
        p.drawLine(QPointF(0, o.y()), QPointF(width(), o.y()));
        p.drawLine(QPointF(o.x(), 0), QPointF(o.x(), height()));
    }
    else
    {
        const double y0 = w2s(QVector3D(0, 0, 0)).y();
        p.setPen(QPen(QColor(90, 94, 104), 1.5));
        p.drawLine(QPointF(0, y0), QPointF(width(), y0));
        p.setPen(QColor(150, 154, 165));
        p.drawText(QPointF(6, y0 - 4), tr("floor"));
    }
}

void StructureStudioView::drawPipe(QPainter &p, const Pipe *pipe) const
{
    const QPointF a = w2s(pipe->positionAt(0.0f));
    const QPointF b = w2s(pipe->positionAt(pipe->length()));
    QColor c = pipe->color();
    if (!c.isValid()) c = QColor(150, 200, 220);
    p.setPen(QPen(c, pipe->isVertical() ? 3.0 : 2.5, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(a, b);
    // end caps
    p.setBrush(c.darker(120));
    p.drawEllipse(a, 2.5, 2.5);
    p.drawEllipse(b, 2.5, 2.5);
}

void StructureStudioView::drawStructure(QPainter &p) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const QColor steel(150, 154, 165);
    const QColor plate(120, 126, 140);

    // A clear steel base plate on the floor: a filled disc in Top, a low solid
    // slab (with end ticks) sitting on the floor line in the elevations.
    auto drawFloorPlate = [&](float cx, float cy, float radiusM) {
        p.setPen(QPen(plate.darker(130), 1.4));
        p.setBrush(QColor(96, 100, 114, 200));
        if (m_plane == Top)
        {
            p.drawEllipse(w2s(QVector3D(cx, cy, 0)), radiusM * m_scale, radiusM * m_scale);
        }
        else
        {
            const QPointF l = w2s(QVector3D(cx - radiusM, cy - radiusM, 0));
            const QPointF r = w2s(QVector3D(cx + radiusM, cy + radiusM, 0));
            const double y0 = l.y();
            const double th = 5.0;   // slab thickness in px
            p.drawRect(QRectF(QPointF(l.x(), y0 - th), QPointF(r.x(), y0)));
            p.setPen(QPen(plate.darker(150), 1.4));   // end ticks (feet)
            p.drawLine(l.x(), y0 - th, l.x(), y0 + 3);
            p.drawLine(r.x(), y0 - th, r.x(), y0 + 3);
        }
    };

    if (m_kind == StandKind)
    {
        Stand *s = props->stand(m_id);
        if (s == nullptr) return;
        drawFloorPlate(s->originX(), s->originY(), s->baseRadius());
        // The post from floor to top.
        p.setPen(QPen(steel, 2.5));
        p.drawLine(w2s(QVector3D(s->originX(), s->originY(), 0)), w2s(s->topPos()));
        // Booms/bars on it.
        foreach (const Pipe *pipe, standPipes())
            drawPipe(p, pipe);
    }
    else if (m_kind == TowerKind)
    {
        Tower *t = props->tower(m_id);
        if (t == nullptr) return;
        const float x0 = t->originX(), y0 = t->originY();
        const float x1 = x0 + t->width(), y1 = y0 + t->depth(), h = t->height();
        p.setPen(QPen(steel, 1.8));
        p.setBrush(QColor(90, 100, 120, 60));
        if (m_plane == Top)
        {
            const QPointF a = w2s(QVector3D(x0, y0, 0));
            const QPointF b = w2s(QVector3D(x1, y1, 0));
            p.drawRect(QRectF(a, b).normalized());
        }
        else
        {
            // Box outline in elevation, plus a horizontal line per shelf.
            const QPointF a = w2s(QVector3D(x0, y0, 0));
            const QPointF b = w2s(QVector3D(x1, y1, h));
            p.drawRect(QRectF(a, b).normalized());
            p.setPen(QPen(steel.lighter(140), 2.0));
            for (int i = 0; i < t->shelfCount(); ++i)
            {
                const float z = t->shelfHeight(i);
                p.drawLine(w2s(QVector3D(x0, y0, z)), w2s(QVector3D(x1, y1, z)));
            }
        }
    }
    else if (m_kind == TrussKind)
    {
        Truss *t = props->truss(m_id);
        if (t == nullptr) return;
        // Base plate FIRST (under the truss) so a floor-standing vertical truss
        // reads clearly; the truss body draws on top.
        if (t->type() == Truss::Vertical && t->origin().z() <= 0.05f)
            drawFloorPlate(t->origin().x(), t->origin().y(), qMax(t->width(), 0.3f));

        const QPointF a = w2s(t->origin());
        const QPointF b = w2s(t->positionAt(t->length()));
        const double wpx = qMax(6.0, double(t->width()) * m_scale);
        const QLineF axis(a, b);

        if (axis.length() < 4.0)
        {
            // The run points INTO the screen (a vertical truss seen from Top):
            // draw the box-truss cross-section — a square with an X.
            const double h = wpx / 2.0;
            p.setPen(QPen(steel, 1.6));
            p.setBrush(QColor(150, 154, 165, 45));
            p.drawRect(QRectF(a.x() - h, a.y() - h, wpx, wpx));
            p.drawLine(a.x() - h, a.y() - h, a.x() + h, a.y() + h);
            p.drawLine(a.x() + h, a.y() - h, a.x() - h, a.y() + h);
        }
        else
        {
            // A real truss: two chords + diagonal webbing between them.
            const QPointF dir = (b - a) / axis.length();
            const QPointF perp(-dir.y(), dir.x());
            const QPointF off = perp * (wpx / 2.0);
            const QPointF a1 = a + off, a2 = a - off, b1 = b + off, b2 = b - off;
            p.setPen(QPen(steel, 1.6));
            p.setBrush(QColor(150, 154, 165, 30));
            QPolygonF poly; poly << a1 << b1 << b2 << a2; p.drawPolygon(poly);
            p.setPen(QPen(steel.darker(110), 1.0));   // Warren webbing
            const int n = qMax(1, int(axis.length() / qMax(10.0, wpx)));
            for (int i = 0; i < n; ++i)
            {
                const QPointF p0 = a + (b - a) * (double(i) / n);
                const QPointF p1 = a + (b - a) * (double(i + 1) / n);
                if (i % 2 == 0) p.drawLine(p0 + off, p1 - off);
                else            p.drawLine(p0 - off, p1 + off);
            }
        }
    }
}

void StructureStudioView::drawFixtures(QPainter &p) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    p.setFont(QFont("Arial", 8));
    foreach (quint32 fid, mountedFixtures())
    {
        Fixture *fx = m_doc->fixture(fid);
        const QPointF c = w2s(props->fixtureRigPosition(fid));
        QColor gel = props->fixtureGelColor(fid, 0, 0);
        if (!gel.isValid() || gel == QColor(Qt::black))
            gel = QColor(230, 210, 120);
        p.setPen(QPen(gel.darker(160), 1.2));
        p.setBrush(gel);
        p.drawEllipse(c, 6.0, 6.0);
        p.setPen(QColor(220, 224, 235));
        p.drawText(QPointF(c.x() + 9, c.y() + 4), fx ? fx->name() : QString::number(fid));
    }
}

void StructureStudioView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    drawGrid(p);
    drawStructure(p);
    drawFixtures(p);

    // Plane badge.
    p.setPen(QColor(160, 164, 175));
    const char *names[] = { "Top", "Front", "Side" };
    p.drawText(rect().adjusted(0, 6, -8, 0), Qt::AlignTop | Qt::AlignRight,
               tr("2D — %1").arg(names[int(m_plane)]));
}

/*********************************************************************
 * Interaction (slice 1: pan/zoom + double-click a fixture)
 *********************************************************************/

void StructureStudioView::resizeEvent(QResizeEvent *) { refit(); }

void StructureStudioView::wheelEvent(QWheelEvent *e)
{
    const double f = (e->angleDelta().y() > 0) ? 1.12 : (1.0 / 1.12);
    m_scale = qBound(10.0, m_scale * f, 320.0);
    refit();
    update();
}

void StructureStudioView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton
        || (e->button() == Qt::LeftButton && (e->modifiers() & Qt::ShiftModifier)))
    {
        m_panning = true;
        m_panLast = e->pos();
    }
}

void StructureStudioView::mouseMoveEvent(QMouseEvent *e)
{
    if (m_panning)
    {
        m_originPx += e->pos() - m_panLast;
        m_panLast = e->pos();
        update();
    }
}

void StructureStudioView::mouseReleaseEvent(QMouseEvent *)
{
    m_panning = false;
}

quint32 StructureStudioView::hitTestFixture(const QPointF &px) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (quint32 fid, mountedFixtures())
    {
        const QPointF c = w2s(props->fixtureRigPosition(fid));
        if (QLineF(c, px).length() <= 8.0)
            return fid;
    }
    return 0;
}

void StructureStudioView::mouseDoubleClickEvent(QMouseEvent *e)
{
    const quint32 fid = hitTestFixture(e->pos());
    if (fid != 0)
        emit fixtureActivated(fid);
}

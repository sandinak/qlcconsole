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
#include "stageplatform.h"
#include "fixture.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
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
    // Top plane reads best for a tower/platform footprint; a boom/truss reads
    // best in a vertical elevation. Default Front for the rest.
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

void StructureStudioView::setHighlight(const QList<quint32> &ids)
{
    m_highlight = QSet<quint32>(ids.begin(), ids.end());
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

QPointF StructureStudioView::screenToPlane(const QPointF &px) const
{
    const double vSign = (m_plane == Top) ? 1.0 : -1.0;
    return QPointF((px.x() - m_originPx.x()) / m_scale,
                   (px.y() - m_originPx.y()) / (vSign * m_scale));
}

bool StructureStudioView::dragFixtureTo(quint32 fid, const QPointF &px)
{
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);

    // In a studio FRAME group (the old studio layout model): move it freely in
    // the current plane. Set the two in-plane WORLD components from the mouse,
    // keep the third, then store back as the group-local offset.
    const quint32 fg = props->fixtureFrameGroup(fid);
    if (fg != 0)
    {
        const QVector3D cur = props->fixtureRigPosition(fid);
        const QPointF ab = screenToPlane(px);
        QVector3D w = cur;
        if (m_plane == Top)        { w.setX(float(ab.x())); w.setY(float(ab.y())); }
        else if (m_plane == Front) { w.setX(float(ab.x())); w.setZ(float(ab.y())); }
        else                       { w.setY(float(ab.x())); w.setZ(float(ab.y())); }
        QVector3D lp = props->worldToGroupLocal(fg, w);
        // Keep it snapped to its assigned face: re-pin the out-of-plane component.
        int pinComp; double pinVal; facePin(rp.studioMount, pinComp, pinVal);
        if (pinComp == 0)      lp.setX(float(pinVal));
        else if (pinComp == 1) lp.setY(float(pinVal));
        else                   lp.setZ(float(pinVal));
        rp.groupLocal = lp;
        props->setFixtureRigProps(fid, rp);
        return true;
    }

    // On a pipe (stand boom/bar): slide along the pipe axis → pipeOffset.
    if (rp.pipeId != Pipe::invalidId())
    {
        Pipe *b = props->pipe(rp.pipeId);
        if (b == nullptr) return false;
        const QPointF A = w2s(b->positionAt(0.0f));
        const QPointF B = w2s(b->positionAt(b->length()));
        const QPointF d = B - A;
        const double l2 = d.x() * d.x() + d.y() * d.y();
        if (l2 < 1e-6) return false;
        double u = ((px.x() - A.x()) * d.x() + (px.y() - A.y()) * d.y()) / l2;
        u = qBound(0.0, u, 1.0);
        rp.pipeOffset = float(u * b->length());
        props->setFixtureRigProps(fid, rp);
        return true;
    }

    // On a tower: Top drags U/V across the footprint; elevations pick the nearest
    // shelf (by height) and the in-plane horizontal.
    if (rp.towerId != Tower::invalidId())
    {
        Tower *t = props->tower(rp.towerId);
        if (t == nullptr) return false;
        const QPointF ab = screenToPlane(px);
        if (m_plane == Top)
        {
            rp.towerU = qBound(0.0f, float(ab.x() - t->originX()), t->width());
            rp.towerV = qBound(0.0f, float(ab.y() - t->originY()), t->depth());
        }
        else
        {
            if (m_plane == Front)
                rp.towerU = qBound(0.0f, float(ab.x() - t->originX()), t->width());
            else
                rp.towerV = qBound(0.0f, float(ab.x() - t->originY()), t->depth());
            // Snap to the nearest shelf by height (ab.y == Z).
            int best = rp.towerShelf; double bestd = 1e9;
            for (int i = 0; i < t->shelfCount(); ++i)
            {
                const double dd = qAbs(double(t->shelfHeight(i)) - ab.y());
                if (dd < bestd) { bestd = dd; best = i; }
            }
            rp.towerShelf = best;
        }
        props->setFixtureRigProps(fid, rp);
        return true;
    }

    // On a platform riser face: drag across the face (U) / up-or-into it (V).
    if (rp.riserPlatformId != FixtureRigProps::invalidPlatformId())
    {
        StagePlatform *pl = props->platform(rp.riserPlatformId);
        if (pl == nullptr) return false;
        const QPointF ab = screenToPlane(px);
        if (rp.riserFace == FixtureRigProps::RiserTop)
        {
            // Top surface: U across width (X), V into depth (Y).
            if (m_plane == Top)
            {
                rp.riserU = qBound(0.0f, float(ab.x() - pl->originX()), pl->width());
                rp.riserV = qBound(0.0f, float(ab.y() - pl->originY()), pl->depth());
            }
            else if (m_plane == Front)
                rp.riserU = qBound(0.0f, float(ab.x() - pl->originX()), pl->width());
            else
                rp.riserV = qBound(0.0f, float(ab.x() - pl->originY()), pl->depth());
        }
        else
        {
            // Front face: U across width (X), V up the face height (Z).
            if (m_plane == Front)
            {
                rp.riserU = qBound(0.0f, float(ab.x() - pl->originX()), pl->width());
                rp.riserV = qBound(0.0f, float(ab.y()), pl->height());
            }
            else if (m_plane == Side)
                rp.riserV = qBound(0.0f, float(ab.y()), pl->height());
            else   // Top
                rp.riserU = qBound(0.0f, float(ab.x() - pl->originX()), pl->width());
        }
        props->setFixtureRigProps(fid, rp);
        return true;
    }

    // On a truss: slide along the truss axis → trussOffset.
    if (rp.trussId != Truss::invalidId())
    {
        Truss *t = props->truss(rp.trussId);
        if (t == nullptr) return false;
        const QPointF A = w2s(t->origin());
        const QPointF B = w2s(t->positionAt(t->length()));
        const QPointF d = B - A;
        const double l2 = d.x() * d.x() + d.y() * d.y();
        if (l2 < 1e-6) return false;   // truss runs into the screen; can't slide here
        double u = ((px.x() - A.x()) * d.x() + (px.y() - A.y()) * d.y()) / l2;
        u = qBound(0.0, u, 1.0);
        rp.trussOffset = float(u * t->length());
        props->setFixtureRigProps(fid, rp);
        return true;
    }
    return false;
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
    else if (m_kind == PipeKind)
    {
        pipeIds << m_id;   // this pipe + any crossbars hung on it
        foreach (Pipe *p, props->pipes())
            if (p->isBarOnPipe() && p->parentPipeId() == m_id)
                pipeIds << p->id();
    }

    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr)
            continue;
        const FixtureRigProps &rp = props->fixtureRigProps(fx->id());
        bool on = false;
        if (m_kind == StandKind || m_kind == PipeKind)
            on = (rp.pipeId != Pipe::invalidId() && pipeIds.contains(rp.pipeId));
        else if (m_kind == TowerKind)
            on = (rp.towerId == m_id);
        else if (m_kind == TrussKind)
            on = (rp.trussId == m_id);
        else if (m_kind == PlatformKind)
        {
            on = (rp.riserPlatformId == m_id || rp.deckPlatformId == m_id);
            if (!on)
            {
                // Also include fixtures laid out via a studio FRAME group that is
                // anchored to this platform (the old "Studio Group" window's set),
                // so both views show the same fixtures.
                const quint32 fg = props->fixtureFrameGroup(fx->id());
                if (fg != 0)
                {
                    const MonitorProperties::MonitorGroup g = props->group(fg);
                    if (g.anchorKind == QStringLiteral("platform") && g.anchorId == m_id)
                        on = true;
                }
            }
        }
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
    else if (m_kind == PlatformKind)
    {
        if (StagePlatform *pl = props->platform(m_id))
            for (int i = 0; i < 8; ++i)
                pts << QVector3D(pl->originX() + ((i & 1) ? pl->width() : 0.0f),
                                 pl->originY() + ((i & 2) ? pl->depth() : 0.0f),
                                 (i & 4) ? pl->height() : 0.0f);
    }
    else if (m_kind == PipeKind)
    {
        if (Pipe *p = props->pipe(m_id))
        {
            pts << p->positionAt(0.0f) << p->positionAt(p->length());
            foreach (Pipe *cb, props->pipes())
                if (cb->isBarOnPipe() && cb->parentPipeId() == m_id)
                    pts << cb->positionAt(0.0f) << cb->positionAt(cb->length());
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
    else if (m_kind == PlatformKind)
    {
        StagePlatform *pl = props->platform(m_id);
        if (pl == nullptr) return;
        const float x0 = pl->originX(), y0 = pl->originY();
        const float x1 = x0 + pl->width(), y1 = y0 + pl->depth(), h = pl->height();
        // Use the platform's own colour (its colour-picker value), like the 2D map.
        QColor pc = pl->color().isValid() ? pl->color() : QColor(110, 120, 140);
        QColor fill = pc; fill.setAlpha(70);
        p.setPen(QPen(pc.darker(140), 1.6));
        p.setBrush(fill);
        if (m_plane == Top)
        {
            p.drawRect(QRectF(w2s(QVector3D(x0, y0, 0)), w2s(QVector3D(x1, y1, 0))).normalized());
        }
        else
        {
            // A riser box silhouette: floor rectangle up to its top.
            const QPointF a = w2s(QVector3D(x0, y0, 0));
            const QPointF b = w2s(QVector3D(x1, y1, h));
            p.drawRect(QRectF(a, b).normalized());
            p.setPen(QPen(steel.lighter(140), 1.4));   // deck line
            p.drawLine(w2s(QVector3D(x0, y0, h)), w2s(QVector3D(x1, y1, h)));
        }
    }
    else if (m_kind == PipeKind)
    {
        Pipe *pipe = props->pipe(m_id);
        if (pipe == nullptr) return;
        // The stand post underneath, if this pipe stands on one.
        if (pipe->isStandMounted())
            if (Stand *s = props->stand(pipe->standId()))
            {
                drawFloorPlate(s->originX(), s->originY(), s->baseRadius());
                p.setPen(QPen(steel, 2.0));
                p.drawLine(w2s(QVector3D(s->originX(), s->originY(), 0)), w2s(s->topPos()));
            }
        drawPipe(p, pipe);
        foreach (Pipe *cb, props->pipes())      // crossbars on this pipe
            if (cb->isBarOnPipe() && cb->parentPipeId() == m_id)
                drawPipe(p, cb);
    }
}

double StructureStudioView::fixtureLenM(quint32 fid) const
{
    // Real physical length (metres): declared physical width, else 0.3 m — NOT a
    // per-head guess (a 64-cell tape isn't 3 m long). Matches the old Face editor.
    Fixture *fx = m_doc->fixture(fid);
    if (fx != nullptr)
    {
        const QLCFixtureMode *mode = fx->fixtureMode();
        if (mode != nullptr && mode->physical().width() > 0)
            return mode->physical().width() / 1000.0;
    }
    return 0.3;
}

QVector3D StructureStudioView::fixtureAxisLocal(const FixtureRigProps &rp) const
{
    const double th = qDegreesToRadians(double(rp.studioAngle));
    const double c = qCos(th), s = qSin(th);
    switch (rp.studioMount)
    {
    case 0:  return QVector3D(float(c), float(s), 0.0f);   // Top/deck: X-Y plane
    case 2:  return QVector3D(0.0f, float(c), float(s));   // Side: Y-Z plane
    case 1:
    default: return QVector3D(float(c), 0.0f, float(s));   // Front/face: X-Z plane
    }
}

QVector3D StructureStudioView::fixtureEndA(quint32 fid) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const double half = qMax(0.05, fixtureLenM(fid) / 2.0);
    const quint32 fg = props->fixtureFrameGroup(fid);
    if (fg != 0)
        return props->groupLocalToWorld(fg, rp.groupLocal - fixtureAxisLocal(rp) * float(half));
    // Non-frame fixture: a horizontal bar in the current plane.
    QVector3D c = props->fixtureRigPosition(fid);
    if (m_plane == Side) c.setY(c.y() - float(half)); else c.setX(c.x() - float(half));
    return c;
}

QVector3D StructureStudioView::fixtureEndB(quint32 fid) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const double half = qMax(0.05, fixtureLenM(fid) / 2.0);
    const quint32 fg = props->fixtureFrameGroup(fid);
    if (fg != 0)
        return props->groupLocalToWorld(fg, rp.groupLocal + fixtureAxisLocal(rp) * float(half));
    QVector3D c = props->fixtureRigPosition(fid);
    if (m_plane == Side) c.setY(c.y() + float(half)); else c.setX(c.x() + float(half));
    return c;
}

void StructureStudioView::facePin(int mount, int &pinComp, double &pinVal) const
{
    StagePlatform *pl = (m_kind == PlatformKind) ? m_doc->monitorProperties()->platform(m_id) : nullptr;
    if (mount == 1)      { pinComp = 1; pinVal = pl ? pl->depth()  : 0.0; }  // Front → pin Y
    else if (mount == 2) { pinComp = 0; pinVal = 0.0; }                       // Side  → pin X
    else                 { pinComp = 2; pinVal = pl ? pl->height() : 0.0; }  // Top   → pin Z
}

void StructureStudioView::drawFixtures(QPainter &p) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    p.setFont(QFont("Arial", 8));
    foreach (quint32 fid, mountedFixtures())
    {
        Fixture *fx = m_doc->fixture(fid);
        const int heads = (fx != nullptr) ? qMax(1, fx->heads()) : 1;
        const QPointF a = w2s(fixtureEndA(fid));
        const QPointF b = w2s(fixtureEndB(fid));
        const QPointF c = w2s(props->fixtureRigPosition(fid));
        const bool hi = m_highlight.contains(fid);
        const bool drag = (fid == m_dragFid);

        QColor col = props->fixtureGelColor(fid, 0, 0);
        if (!col.isValid() || col == QColor(Qt::black))
            col = QColor(90, 160, 235);
        if (drag)     col = QColor(255, 196, 64);
        else if (hi)  col = QColor(120, 220, 140);

        if (hi)   // selection halo behind the bar
        {
            QPen halo(QColor(120, 220, 140, 160)); halo.setWidth(9); halo.setCapStyle(Qt::RoundCap);
            p.setPen(halo);
            p.drawLine(a, b);
        }
        QPen body(col.darker(140)); body.setWidth((drag || hi) ? 4 : 3); body.setCapStyle(Qt::RoundCap);
        p.setPen(body);
        p.drawLine(a, b);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        for (int i = 0; i < heads; ++i)   // heads as ticks along the bar
        {
            const double t = (heads > 1) ? double(i) / (heads - 1) : 0.5;
            p.drawEllipse(a + (b - a) * t, 2.6, 2.6);
        }
        if (fx != nullptr)
        {
            p.setPen(QColor(210, 214, 220));
            p.drawText(QPointF(c.x() + 8, c.y() - 6), fx->name());
        }
    }
}

static qlonglong ssvTrailingNum(const QString &s)
{
    int i = s.size();
    while (i > 0 && s[i - 1].isDigit()) --i;
    if (i == s.size()) return -1;
    return s.mid(i).toLongLong();
}

void StructureStudioView::distributeOnFace(const QList<quint32> &sel)
{
    MonitorProperties *props = m_doc->monitorProperties();
    // Frame-group fixtures lay out on a face; others fall back to a param spread.
    QList<quint32> ids, other;
    foreach (quint32 fid, sel.isEmpty() ? mountedFixtures() : sel)
        (props->fixtureFrameGroup(fid) != 0 ? ids : other) << fid;

    // Name order (trailing #N, else natural).
    auto byName = [this](quint32 a, quint32 b) {
        Fixture *fa = m_doc->fixture(a), *fb = m_doc->fixture(b);
        const QString na = fa ? fa->name() : QString::number(a);
        const QString nb = fb ? fb->name() : QString::number(b);
        const qlonglong ta = ssvTrailingNum(na), tb = ssvTrailingNum(nb);
        if (ta >= 0 && tb >= 0 && ta != tb) return ta < tb;
        return na < nb;
    };
    std::sort(ids.begin(), ids.end(), byName);

    const int n = ids.size();
    if (n > 0)
    {
        StagePlatform *pl = (m_kind == PlatformKind) ? props->platform(m_id) : nullptr;
        double W = 1.0, H = 1.0;
        if (pl != nullptr)
        {
            if (m_plane == Front)     { W = pl->width(); H = pl->height(); }
            else if (m_plane == Side) { W = pl->depth(); H = pl->height(); }
            else                      { W = pl->width(); H = pl->depth();  }
        }
        const int mount = int(m_plane);
        int pinComp; double pinVal; facePin(mount, pinComp, pinVal);
        double L = 0.0;
        foreach (quint32 fid, ids) L = qMax(L, fixtureLenM(fid));
        const bool stackVertical = (double(n) * L > W + 1e-6);
        const bool topIsMaxB = (m_plane != Top);   // Z↑ in elevations; Y↓ in Top

        for (int i = 0; i < n; ++i)
        {
            double a, b;
            if (stackVertical)
            {
                a = W * 0.5;                                        // centred across width
                const double frac = topIsMaxB ? (n - i - 0.5) / n : (i + 0.5) / n;
                b = H * frac;                                       // item 0 at the top
            }
            else
            {
                a = W * (i + 0.5) / n;                              // left → right
                b = H * 0.5;                                        // centred vertically
            }
            FixtureRigProps rp = props->fixtureRigProps(ids[i]);
            rp.studioMount = mount;
            QVector3D lp;
            if (m_plane == Front)     lp = QVector3D(float(a), 0.0f, float(b));
            else if (m_plane == Side) lp = QVector3D(0.0f, float(a), float(b));
            else                      lp = QVector3D(float(a), float(b), 0.0f);
            if (pinComp == 0)      lp.setX(float(pinVal));
            else if (pinComp == 1) lp.setY(float(pinVal));
            else                   lp.setZ(float(pinVal));
            rp.groupLocal = lp;
            props->setFixtureRigProps(ids[i], rp);
        }
    }

    // Non-frame fixtures (riser/pipe/truss): even spread along their own param.
    std::sort(other.begin(), other.end(), byName);
    for (int i = 0; i < other.size(); ++i)
    {
        const float t = (i + 0.5f) / float(other.size());
        FixtureRigProps rp = props->fixtureRigProps(other[i]);
        if (rp.riserPlatformId != FixtureRigProps::invalidPlatformId())
        { if (StagePlatform *pl = props->platform(rp.riserPlatformId)) rp.riserU = t * pl->width(); }
        else if (rp.pipeId != Pipe::invalidId())
        { if (Pipe *p = props->pipe(rp.pipeId)) rp.pipeOffset = t * p->length(); }
        else if (rp.trussId != Truss::invalidId())
        { if (Truss *tt = props->truss(rp.trussId)) rp.trussOffset = t * tt->length(); }
        props->setFixtureRigProps(other[i], rp);
    }

    m_doc->setModified();
    reload();
}

void StructureStudioView::setFixtureFace(quint32 fid, int face)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (props->fixtureFrameGroup(fid) == 0) return;   // only frame fixtures have a face
    FixtureRigProps rp = props->fixtureRigProps(fid);
    rp.studioMount = face;
    int pinComp; double pinVal; facePin(face, pinComp, pinVal);
    QVector3D lp = rp.groupLocal;
    if (pinComp == 0)      lp.setX(float(pinVal));
    else if (pinComp == 1) lp.setY(float(pinVal));
    else                   lp.setZ(float(pinVal));
    rp.groupLocal = lp;
    props->setFixtureRigProps(fid, rp);
    m_doc->setModified();
    reload();
}

void StructureStudioView::setFixtureAngle(quint32 fid, float deg)
{
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);
    rp.studioAngle = deg;
    props->setFixtureRigProps(fid, rp);
    m_doc->setModified();
    reload();
}

void StructureStudioView::putOnFace(const QList<quint32> &ids)
{
    MonitorProperties *props = m_doc->monitorProperties();
    QList<quint32> fids = ids.isEmpty() ? mountedFixtures() : ids;
    const int mount = int(m_plane);            // Top/Front/Side ↔ studioMount 0/1/2
    int pinComp; double pinVal; facePin(mount, pinComp, pinVal);
    bool any = false;
    foreach (quint32 fid, fids)
    {
        const quint32 fg = props->fixtureFrameGroup(fid);
        if (fg == 0) continue;                 // only frame-group fixtures have a face
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.studioMount = mount;
        QVector3D lp = rp.groupLocal;
        if (pinComp == 0)      lp.setX(float(pinVal));
        else if (pinComp == 1) lp.setY(float(pinVal));
        else                   lp.setZ(float(pinVal));
        rp.groupLocal = lp;
        props->setFixtureRigProps(fid, rp);
        any = true;
    }
    if (any) { m_doc->setModified(); reload(); }
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
        return;
    }
    if (e->button() == Qt::LeftButton)
    {
        m_dragFid = hitTestFixture(e->pos());   // 0 if empty space
        m_dragged = false;
        if (m_dragFid != 0)
        {
            setCursor(Qt::ClosedHandCursor);
            setHighlight({ m_dragFid });
            emit fixtureSelected(m_dragFid);
        }
    }
}

void StructureStudioView::mouseMoveEvent(QMouseEvent *e)
{
    if (m_panning)
    {
        m_originPx += e->pos() - m_panLast;
        m_panLast = e->pos();
        update();
        return;
    }
    if (m_dragFid != 0)
    {
        if (!m_dragged)
            emit editAboutToStart();   // snapshot for undo before the first change
        if (dragFixtureTo(m_dragFid, e->pos()))
        {
            m_dragged = true;
            update();
        }
    }
}

void StructureStudioView::mouseReleaseEvent(QMouseEvent *)
{
    m_panning = false;
    if (m_dragFid != 0)
    {
        setCursor(Qt::ArrowCursor);
        if (m_dragged)
        {
            m_doc->setModified();
            emit fixtureMoved(m_dragFid);
        }
        m_dragFid = 0;
        m_dragged = false;
    }
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

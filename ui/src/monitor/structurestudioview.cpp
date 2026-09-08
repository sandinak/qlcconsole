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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QContextMenuEvent>
#include <QMimeData>
#include <QDataStream>
#include <QSet>
#include <QtMath>
#include <cmath>

#include "structurestudioview.h"
#include "fixturevisualtraits.h"
#include "monitorproperties.h"
#include "pipe.h"
#include "stand.h"
#include "tower.h"
#include "truss.h"
#include "stageplatform.h"
#include "fixture.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlcphysical.h"
#include "doc.h"
#include "qlceventpos.h"

StructureStudioView::StructureStudioView(Doc *doc, Kind kind, quint32 id, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_kind(kind)
    , m_id(id)
{
    setMinimumSize(360, 320);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);   // fixtures dragged in from the source tree
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
    case Angled:
    {
        /* Orthographic axonometric from azimuth A and elevation E.
         *
         * Derived rather than eyeballed, so it degenerates to the exact Front
         * view at A=E=0 and to the Top view at E=90 -- a projection that does
         * not agree with the flat views at their own angles would make the
         * angled look untrustworthy for judging a rig.
         *
         * The eye looks toward the origin from downstage (+Y is downstage
         * here, see the axis note in monitorproperties.cpp), lifted by E and
         * swung by A:
         *     view dir  d = (-sinA cosE, -cosA cosE, -sinE)
         *     screen right r = (cosA, -sinA, 0)        (horizontal, perp to d)
         *     screen up    u = d x r = (-sinE sinA, -sinE cosA, cosE)
         * and the in-plane coordinates are just the projections onto them. */
        const double A = qDegreesToRadians(m_azimuthDeg);
        const double E = qDegreesToRadians(m_elevationDeg);
        const double cA = qCos(A), sA = qSin(A), cE = qCos(E), sE = qSin(E);
        const double a = double(w.x()) * cA - double(w.y()) * sA;
        const double b = -double(w.x()) * sE * sA
                         - double(w.y()) * sE * cA
                         + double(w.z()) * cE;
        return QPointF(a, b);
    }
    case Top:
    default:    return QPointF(w.x(), w.y());
    }
}

void StructureStudioView::setAngledView(double azimuthDeg, double elevationDeg)
{
    // Elevation is clamped to the quarter that actually looks AT the rig: below
    // 0 you are under the stage, above 90 the view turns over.
    const double az = std::fmod(std::fmod(azimuthDeg, 360.0) + 360.0, 360.0);
    const double el = qBound(0.0, elevationDeg, 90.0);
    if (qFuzzyCompare(az, m_azimuthDeg) && qFuzzyCompare(el, m_elevationDeg))
        return;
    m_azimuthDeg = az;
    m_elevationDeg = el;
    if (m_plane == Angled)
    {
        refit();
        update();
    }
    emit angledViewChanged(m_azimuthDeg, m_elevationDeg);
}

/* The in-plane (a,b) -> screen OFFSET mapping, with the view rotation applied.
 *
 * Rotation is deliberately a VIEW transform and nothing more: it lives here, in
 * the one place world coordinates become pixels, so stored coordinates never
 * move and everything routed through w2s()/screenToPlane() -- drawing, dragging,
 * hit-testing, the rulers -- follows for free. Quarter turns only, so the
 * handedness of the plot is preserved: a rotated plan still tells the truth
 * about which side of the stage a fixture is on. (A MIRROR would not, which is
 * why "downstage at the top with stage right still on the left" is not offered
 * here -- that flips the sense of every direction on the plot.) */
QPointF StructureStudioView::planeToScreenVec(const QPointF &ab) const
{
    // Top reads Y downward like a plan; every other view (including Angled)
    // has "up the screen" as its b axis.
    const double vSign = (m_plane == Top) ? 1.0 : -1.0;
    const QPointF v(ab.x() * m_scale, vSign * ab.y() * m_scale);
    switch (m_rotation & 3)
    {
    case 1:  return QPointF(-v.y(),  v.x());   // 90 deg clockwise
    case 2:  return QPointF(-v.x(), -v.y());   // 180
    case 3:  return QPointF( v.y(), -v.x());   // 270
    default: return v;
    }
}

QPointF StructureStudioView::screenVecToPlane(const QPointF &vIn) const
{
    QPointF v = vIn;
    switch (m_rotation & 3)                    // the inverse turn
    {
    case 1:  v = QPointF( vIn.y(), -vIn.x()); break;
    case 2:  v = QPointF(-vIn.x(), -vIn.y()); break;
    case 3:  v = QPointF(-vIn.y(),  vIn.x()); break;
    default: break;
    }
    const double vSign = (m_plane == Top) ? 1.0 : -1.0;
    return QPointF(v.x() / m_scale, v.y() / (vSign * m_scale));
}

QPointF StructureStudioView::w2s(const QVector3D &w) const
{
    return m_originPx + planeToScreenVec(project(w));
}

QPointF StructureStudioView::screenToPlane(const QPointF &px) const
{
    return screenVecToPlane(px - m_originPx);
}

void StructureStudioView::setRotation(int quarterTurns)
{
    const int r = ((quarterTurns % 4) + 4) % 4;
    if (m_rotation == r)
        return;
    m_rotation = r;
    refit();
    update();
}

bool StructureStudioView::dragFixtureTo(quint32 fid, const QPointF &px)
{
    /* The angled view has no honest inverse: screenToPlane() can only undo a
       projection that DROPPED an axis, and an axonometric keeps all three, so a
       screen point is a ray. Rather than pick a plausible depth and move the
       fixture somewhere the operator did not ask for, refuse -- selection still
       works, and Top/Front/Side are one click away for the actual edit. */
    if (m_plane == Angled)
        return false;

    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);

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

    // On a truss: move it in the plane, resolved onto the two freedoms a truss
    // mount actually has — ALONG the run (trussOffset) and ACROSS it
    // (trussCross, the sideways nudge that keeps a fixture bound while it hangs
    // off the chord).
    //
    // Worked as a DELTA from where the fixture is now, not from the truss
    // origin: fixtureRigPosition() adds the mount-side half-width and
    // mountZOffset on top of positionAt(), and those constants cancel in a
    // difference. The off-plane world component of the target equals the
    // fixture's current one, so it contributes nothing to either dot product —
    // dragging in an elevation cannot disturb a value only the top view can
    // see, and vice versa.
    //
    // This replaces a pure project-onto-the-axis version that (a) ignored
    // trussCross entirely, so a bound fixture could never be positioned just
    // off the bar as it is really mounted, and (b) bailed out whenever the run
    // projected to a point — which is EVERY top view of a vertical truss, where
    // the fixture then could not be moved at all.
    if (rp.trussId != Truss::invalidId())
    {
        Truss *t = props->truss(rp.trussId);
        if (t == nullptr) return false;

        const QVector3D cur = props->fixtureRigPosition(fid);
        const QPointF ab = screenToPlane(px);
        QVector3D w = cur;
        if (m_plane == Top)        { w.setX(float(ab.x())); w.setY(float(ab.y())); }
        else if (m_plane == Front) { w.setX(float(ab.x())); w.setZ(float(ab.y())); }
        else                       { w.setY(float(ab.x())); w.setZ(float(ab.y())); }
        const QVector3D dw = w - cur;

        /* An ORTHONORMAL basis for the mount's three freedoms, so a drag in any
           plane resolves onto whichever of them that plane can see. The third
           axis is stored differently per truss type, because that is what the
           geometry means:

             vertical run  -- the run IS Z, so BOTH horizontals are free around
                              the tower: trussCross (X) and trussCrossY (Y).
             horizontal run -- along the run, across it in the horizontal plane
                              (trussCross), and height, which is the drop
                              (mountZOffset).

           A vertical run used to offer only X across, so a lateral drag in the
           SIDE view (horizontal screen axis = Y) resolved to zero and the
           fixture would not move at all. */
        QVector3D axis, cross, cross2;
        const bool vertical = (t->type() == Truss::Vertical);
        if (vertical)
        {
            axis   = QVector3D(0.0f, 0.0f, t->isChildBar() ? -1.0f : 1.0f);
            cross  = QVector3D(1.0f, 0.0f, 0.0f);   // -> trussCross
            cross2 = QVector3D(0.0f, 1.0f, 0.0f);   // -> trussCrossY
        }
        else
        {
            const QPointF d = t->direction();
            const double dl = std::hypot(d.x(), d.y());
            if (dl < 1e-9) return false;
            axis   = QVector3D(float(d.x() / dl), float(d.y() / dl), 0.0f);
            cross  = QVector3D(float(-d.y() / dl), float(d.x() / dl), 0.0f);
            cross2 = QVector3D(0.0f, 0.0f, 1.0f);   // -> mountZOffset (the drop)
        }

        rp.trussOffset = qBound(0.0f, rp.trussOffset + QVector3D::dotProduct(dw, axis),
                                t->length());
        // Same two-widths zone the plot uses to decide a fixture is still "on"
        // this truss: past it, it is no longer a truss mount at all.
        const float limit = qMax(0.05f, t->width() * 2.0f);
        rp.trussCross = qBound(-limit, rp.trussCross + QVector3D::dotProduct(dw, cross),
                               limit);

        const float d2 = QVector3D::dotProduct(dw, cross2);
        if (vertical)
            rp.trussCrossY = qBound(-limit, rp.trussCrossY + d2, limit);
        else
            rp.mountZOffset += d2;   // the drop: how far it hangs below the bar
        props->setFixtureRigProps(fid, rp);
        return true;
    }

    // Studio FRAME group (the old studio layout model), only once none of the
    // structural mounts above claimed it: move it freely in the current plane.
    // Set the two in-plane WORLD components from the mouse, keep the third,
    // then store back as the group-local offset.
    const quint32 fg = hasStructuralMount(rp) ? 0 : props->fixtureFrameGroup(fid);
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
    return false;
}

/*********************************************************************
 * Structure gathering
 *********************************************************************/


/* StageKind draws the WHOLE rig rather than one object.
 *
 * The per-object drawing, point-gathering and fixture-listing were already
 * written and tested for the single-structure editor; they just assumed the one
 * structure the dialog was opened on. Parameterising them on (kind, id) and
 * looping is all a whole-stage overview needs -- no second renderer to keep in
 * step with the first, which is the thing that would rot. */
QList<QPair<StructureStudioView::Kind, quint32> > StructureStudioView::everyStructure() const
{
    MonitorProperties *props = m_doc->monitorProperties();
    QList<QPair<Kind, quint32> > out;
    foreach (Truss *t, props->trusses())
        if (t != nullptr) out << qMakePair(TrussKind, t->id());
    foreach (StagePlatform *pl, props->platforms())
        if (pl != nullptr) out << qMakePair(PlatformKind, pl->id());
    foreach (Tower *tw, props->towers())
        if (tw != nullptr) out << qMakePair(TowerKind, tw->id());
    foreach (Stand *st, props->stands())
        if (st != nullptr) out << qMakePair(StandKind, st->id());
    foreach (Pipe *pp, props->pipes())
        if (pp != nullptr && !pp->isBarOnPipe()) out << qMakePair(PipeKind, pp->id());
    return out;
}

void StructureStudioView::drawStructure(QPainter &p) const
{
    if (m_kind != StageKind)
    {
        drawOneStructure(p, m_kind, m_id);
        return;
    }
    typedef QPair<Kind, quint32> KindId;
    foreach (const KindId &ki, everyStructure())
        drawOneStructure(p, ki.first, ki.second);
}

void StructureStudioView::collectPoints(QList<QVector3D> &pts) const
{
    if (m_kind != StageKind)
    {
        collectPointsFor(pts, m_kind, m_id);
        return;
    }
    typedef QPair<Kind, quint32> KindId;
    foreach (const KindId &ki, everyStructure())
        collectPointsFor(pts, ki.first, ki.second);
    // Free-standing fixtures are part of the rig too, so the fit must frame them.
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (quint32 fid, props->fixtureItemsID())
        pts << props->fixtureRigPosition(fid);
}

QList<quint32> StructureStudioView::mountedFixtures() const
{
    if (m_kind != StageKind)
        return mountedFixtures(m_kind, m_id);
    // The whole rig: everything that has a place on the plot.
    return m_doc->monitorProperties()->fixtureItemsID();
}

QList<const Pipe *> StructureStudioView::standPipes(quint32 id) const
{
    // No kind guard: the callers ask for a specific stand's pipes, and the
    // whole-rig overview needs them for stands other than the one being edited.
    QList<const Pipe *> out;
    MonitorProperties *props = m_doc->monitorProperties();
    QList<quint32> booms;
    foreach (Pipe *p, props->pipes())
        if (p->standId() == id)
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

QList<quint32> StructureStudioView::mountedFixtures(Kind kind, quint32 id) const
{
    QList<quint32> out;
    MonitorProperties *props = m_doc->monitorProperties();

    QList<quint32> pipeIds;
    if (kind == StandKind)
    {
        foreach (const Pipe *p, standPipes(id))
            pipeIds << p->id();
    }
    else if (kind == PipeKind)
    {
        pipeIds << id;   // this pipe + any crossbars hung on it
        foreach (Pipe *p, props->pipes())
            if (p->isBarOnPipe() && p->parentPipeId() == id)
                pipeIds << p->id();
    }

    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr)
            continue;
        const FixtureRigProps &rp = props->fixtureRigProps(fx->id());
        bool on = false;
        if (kind == StandKind || kind == PipeKind)
            on = (rp.pipeId != Pipe::invalidId() && pipeIds.contains(rp.pipeId));
        else if (kind == TowerKind)
            on = (rp.towerId == id);
        else if (kind == TrussKind)
            on = (rp.trussId == id);
        else if (kind == GroupKind)
            on = (props->fixtureFrameGroup(fx->id()) == id);
        else if (kind == PlatformKind)
        {
            on = (rp.riserPlatformId == id || rp.deckPlatformId == id);
            if (!on)
            {
                // Also include fixtures laid out via a studio FRAME group that is
                // anchored to this platform (the old "Studio Group" window's set),
                // so both views show the same fixtures.
                const quint32 fg = props->fixtureFrameGroup(fx->id());
                if (fg != 0)
                {
                    const MonitorProperties::MonitorGroup g = props->group(fg);
                    if (g.anchorKind == QStringLiteral("platform") && g.anchorId == id)
                        on = true;
                }
            }
        }
        if (on)
            out << fx->id();
    }
    return out;
}

void StructureStudioView::collectPointsFor(QList<QVector3D> &pts, Kind kind, quint32 id) const
{
    MonitorProperties *props = m_doc->monitorProperties();

    if (kind == StandKind)
    {
        if (Stand *s = props->stand(id))
        {
            pts << QVector3D(s->originX(), s->originY(), 0.0f);
            pts << s->topPos();
            const float br = s->baseRadius();
            pts << QVector3D(s->originX() - br, s->originY() - br, 0.0f);
            pts << QVector3D(s->originX() + br, s->originY() + br, 0.0f);
        }
        foreach (const Pipe *p, standPipes(id))
        {
            pts << p->positionAt(0.0f);
            pts << p->positionAt(p->length());
        }
    }
    else if (kind == TowerKind)
    {
        if (Tower *t = props->tower(id))
        {
            pts << QVector3D(t->originX(), t->originY(), 0.0f);
            pts << QVector3D(t->originX() + t->width(), t->originY() + t->depth(), t->height());
        }
    }
    else if (kind == TrussKind)
    {
        if (Truss *t = props->truss(id))
        {
            pts << t->origin();
            pts << t->positionAt(t->length());
        }
    }
    else if (kind == PlatformKind)
    {
        if (StagePlatform *pl = props->platform(id))
            for (int i = 0; i < 8; ++i)
                pts << QVector3D(pl->originX() + ((i & 1) ? pl->width() : 0.0f),
                                 pl->originY() + ((i & 2) ? pl->depth() : 0.0f),
                                 (i & 4) ? pl->height() : 0.0f);
    }
    else if (kind == PipeKind)
    {
        if (Pipe *p = props->pipe(id))
        {
            pts << p->positionAt(0.0f) << p->positionAt(p->length());
            foreach (Pipe *cb, props->pipes())
                if (cb->isBarOnPipe() && cb->parentPipeId() == id)
                    pts << cb->positionAt(0.0f) << cb->positionAt(cb->length());
        }
    }
    else if (kind == GroupKind)
    {
        pts << props->group(id).origin;   // members added below
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

    double spanA = qMax(maxA - minA, 0.6);
    double spanB = qMax(maxB - minB, 0.6);
    // An odd quarter turn puts the 'a' extent on the screen's VERTICAL axis, so
    // the fit has to compare the spans the way they will actually be laid out.
    if (m_rotation & 1)
        qSwap(spanA, spanB);
    const double margin = 42.0;
    const double availW = qMax(1.0, width()  - 2 * margin);
    const double availH = qMax(1.0, height() - 2 * margin);
    m_scale = qBound(10.0, qMin(availW / spanA, availH / spanB), 320.0);

    const double cA = (minA + maxA) / 2.0;
    const double cB = (minB + maxB) / 2.0;
    // Centre the structure by placing its middle at the widget centre: the
    // rotation is baked into planeToScreenVec(), so ask it where the centre
    // lands and subtract.
    m_originPx = QPointF(0, 0);
    m_originPx = QPointF(width() / 2.0, height() / 2.0)
                 - planeToScreenVec(QPointF(cA, cB));
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
    else if (m_plane == Angled)
    {
        /* At an angle the floor is a receding PLANE, not a line across the
           canvas -- a flat horizontal rule cut straight through the rig and
           read as a wall behind it. Draw a ground grid over the rig's own
           footprint so the structures have something to stand on. */
        QList<QVector3D> pts;
        collectPoints(pts);
        if (pts.isEmpty())
            return;
        double x0 = pts.first().x(), x1 = x0, y0 = pts.first().y(), y1 = y0;
        foreach (const QVector3D &w, pts)
        {
            x0 = qMin(x0, double(w.x())); x1 = qMax(x1, double(w.x()));
            y0 = qMin(y0, double(w.y())); y1 = qMax(y1, double(w.y()));
        }
        const double pad = qMax(0.5, qMax(x1 - x0, y1 - y0) * 0.06);
        x0 -= pad; x1 += pad; y0 -= pad; y1 += pad;

        const double step = qMax(0.5, qRound((x1 - x0) / 12.0 * 2.0) / 2.0);
        p.setPen(QPen(QColor(58, 62, 72), 1.0));
        for (double x = x0; x <= x1 + 1e-6; x += step)
            p.drawLine(w2s(QVector3D(float(x), float(y0), 0)),
                       w2s(QVector3D(float(x), float(y1), 0)));
        for (double y = y0; y <= y1 + 1e-6; y += step)
            p.drawLine(w2s(QVector3D(float(x0), float(y), 0)),
                       w2s(QVector3D(float(x1), float(y), 0)));
        p.setPen(QPen(QColor(92, 96, 108), 1.4));
        QPolygonF edge;
        edge << w2s(QVector3D(float(x0), float(y0), 0)) << w2s(QVector3D(float(x1), float(y0), 0))
             << w2s(QVector3D(float(x1), float(y1), 0)) << w2s(QVector3D(float(x0), float(y1), 0));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(edge);
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

/* How far a world point is from the eye, for painter's-algorithm ordering.
 * Only meaningful in the Angled plane; the flat views draw in a fixed order. */
double StructureStudioView::viewDepth(const QVector3D &w) const
{
    const double A = qDegreesToRadians(m_azimuthDeg);
    const double E = qDegreesToRadians(m_elevationDeg);
    // The view direction derived in project(): d = (-sinA cosE, -cosA cosE, -sinE).
    return -(double(w.x()) * -qSin(A) * qCos(E)
             + double(w.y()) * -qCos(A) * qCos(E)
             + double(w.z()) * -qSin(E));
}

/* Paint a solid box from its eight world corners.
 *
 * The flat views can draw a structure as a rectangle because one axis is
 * dropped; at an angle that same rectangle is a BILLBOARD -- it turns to face
 * the viewer however the camera swings, so a truss stayed edge-on flat and a
 * platform stayed a flat pane no matter where you stood. Projecting the real
 * corners and filling the faces back-to-front is what makes a box look like a
 * box. Faces are shaded by orientation so the form reads without any lighting
 * model: tops brightest, then the two side pairs.
 *
 * Corner order is the unit cube: 0-3 the bottom face (CCW), 4-7 the top. */
void StructureStudioView::drawSolidBox(QPainter &p, const QVector3D corner[8],
                                       const QColor &base, const QColor &edge) const
{
    static const int faces[6][4] = {
        { 4, 5, 6, 7 },   // top
        { 0, 1, 2, 3 },   // bottom
        { 0, 1, 5, 4 },   // side
        { 2, 3, 7, 6 },   // side
        { 1, 2, 6, 5 },   // end
        { 3, 0, 4, 7 },   // end
    };
    static const int shade[6] = { 118, 62, 92, 78, 100, 85 };   // % brightness

    // Back to front, so nearer faces cover the ones behind them.
    QVector<QPair<double, int> > order;
    for (int f = 0; f < 6; ++f)
    {
        double d = 0.0;
        for (int k = 0; k < 4; ++k)
            d += viewDepth(corner[faces[f][k]]);
        order << qMakePair(d / 4.0, f);
    }
    std::sort(order.begin(), order.end(),
              [](const QPair<double, int> &a, const QPair<double, int> &b)
              { return a.first > b.first; });

    foreach (const auto &o, order)
    {
        const int f = o.second;
        QPolygonF poly;
        for (int k = 0; k < 4; ++k)
            poly << w2s(corner[faces[f][k]]);
        QColor c = base;
        c = (shade[f] >= 100) ? c.lighter(shade[f]) : c.darker(200 - shade[f]);
        c.setAlpha(255);                       // solid: a deck is not a window
        p.setBrush(c);
        p.setPen(QPen(edge, 1.1));
        p.drawPolygon(poly);
    }
}

/* The eight corners of an axis-aligned world box. */
static void boxCorners(QVector3D out[8], float x0, float y0, float z0,
                       float x1, float y1, float z1)
{
    out[0] = QVector3D(x0, y0, z0); out[1] = QVector3D(x1, y0, z0);
    out[2] = QVector3D(x1, y1, z0); out[3] = QVector3D(x0, y1, z0);
    out[4] = QVector3D(x0, y0, z1); out[5] = QVector3D(x1, y0, z1);
    out[6] = QVector3D(x1, y1, z1); out[7] = QVector3D(x0, y1, z1);
}

void StructureStudioView::drawOneStructure(QPainter &p, Kind kind, quint32 id) const
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

    if (kind == StandKind)
    {
        Stand *s = props->stand(id);
        if (s == nullptr) return;
        drawFloorPlate(s->originX(), s->originY(), s->baseRadius());
        const float br = s->baseRadius();
        // Tripod LEGS so the stand reads as a stand. In an elevation two legs
        // splay from a collar (~⅓ up) to the floor edges; in Top the three feet.
        p.setPen(QPen(steel.darker(110), 1.6));
        if (m_plane == Top)
        {
            const QPointF c = w2s(QVector3D(s->originX(), s->originY(), 0));
            for (int i = 0; i < 3; ++i)
            {
                const double a = M_PI / 2.0 + i * (2.0 * M_PI / 3.0);
                p.drawLine(c, c + QPointF(qCos(a) * br * m_scale, qSin(a) * br * m_scale));
            }
        }
        else
        {
            const QVector3D collar(s->originX(), s->originY(), s->height() * 0.35f);
            p.drawLine(w2s(collar), w2s(QVector3D(s->originX() - br, s->originY() - br, 0)));
            p.drawLine(w2s(collar), w2s(QVector3D(s->originX() + br, s->originY() + br, 0)));
        }
        // The post from floor to top.
        p.setPen(QPen(steel, 2.5));
        p.drawLine(w2s(QVector3D(s->originX(), s->originY(), 0)), w2s(s->topPos()));
        // Booms/bars on it — draw a grab handle at each boom TOP (drag = resize
        // the hangable length).
        foreach (const Pipe *pipe, standPipes(id))
        {
            drawPipe(p, pipe);
            if (pipe->isVertical())
            {
                const QPointF top = w2s(pipe->positionAt(pipe->length()));
                p.setPen(QPen(QColor(0, 190, 255), 1.4));
                p.setBrush(QColor(0, 190, 255, 160));
                p.drawEllipse(top, 4.0, 4.0);
            }
        }
    }
    else if (kind == TowerKind)
    {
        Tower *t = props->tower(id);
        if (t == nullptr) return;
        const float x0 = t->originX(), y0 = t->originY();
        const float x1 = x0 + t->width(), y1 = y0 + t->depth(), h = t->height();
        p.setPen(QPen(steel, 1.8));
        p.setBrush(QColor(90, 100, 120, 60));
        if (m_plane == Angled)
        {
            // A real box, so the tower keeps its footprint as the camera swings
            // instead of turning to face the viewer.
            QVector3D c[8];
            boxCorners(c, x0, y0, 0.0f, x1, y1, h);
            drawSolidBox(p, c, QColor(96, 106, 126), steel.lighter(150));
            // Shelves still read as lines across the front face.
            p.setPen(QPen(steel.lighter(140), 1.6));
            for (int i = 0; i < t->shelfCount(); ++i)
            {
                const float z = t->shelfHeight(i);
                p.drawLine(w2s(QVector3D(x0, y1, z)), w2s(QVector3D(x1, y1, z)));
            }
        }
        else if (m_plane == Top)
        {
            const QPointF a = w2s(QVector3D(x0, y0, 0));
            const QPointF b = w2s(QVector3D(x1, y1, 0));
            p.drawRect(QRectF(a, b).normalized());
        }
        else
        {
            // Box outline in elevation, plus a horizontal line per shelf — each
            // labelled with its number and height so a fixture dragged onto one
            // (see dragFixtureTo()'s "snap to nearest shelf by height") can
            // actually be identified rather than just seen as an unlabelled line.
            const bool isFeet = (props->gridUnits() == MonitorProperties::Feet);
            const double toDisp = isFeet ? 3.28084 : 1.0;
            const QString sfx = isFeet ? tr(" ft") : tr(" m");
            const QPointF a = w2s(QVector3D(x0, y0, 0));
            const QPointF b = w2s(QVector3D(x1, y1, h));
            p.drawRect(QRectF(a, b).normalized());
            p.setPen(QPen(steel.lighter(140), 2.0));
            p.setFont(QFont("Arial", 8));
            for (int i = 0; i < t->shelfCount(); ++i)
            {
                const float z = t->shelfHeight(i);
                const QPointF sA = w2s(QVector3D(x0, y0, z));
                const QPointF sB = w2s(QVector3D(x1, y1, z));
                p.setPen(QPen(steel.lighter(140), 2.0));
                p.drawLine(sA, sB);
                p.setPen(steel.lighter(170));
                p.drawText(sB + QPointF(6, -3), tr("Shelf %1 — %2%3")
                    .arg(i + 1).arg(double(z) * toDisp, 0, 'f', 2).arg(sfx));
            }
        }
    }
    else if (kind == TrussKind)
    {
        Truss *t = props->truss(id);
        if (t == nullptr) return;
        // Base plate FIRST (under the truss) so a floor-standing vertical truss
        // reads clearly; the truss body draws on top.
        if (t->type() == Truss::Vertical && t->origin().z() <= 0.05f)
            drawFloorPlate(t->origin().x(), t->origin().y(), qMax(t->width(), 0.3f));

        if (m_plane == Angled)
        {
            /* An ORIENTED box along the run. The flat-view code below takes the
               perpendicular in SCREEN space, which is a billboard: the truss
               would keep its face turned to the viewer however the camera
               swung, so it always looked flat. Build the cross-section from
               world axes instead and it foreshortens like the solid it is. */
            const QVector3D A = t->origin();
            const QVector3D B = t->positionAt(t->length());
            QVector3D L = B - A;
            if (L.length() > 1e-6f)
            {
                L.normalize();
                // Cross-section axes: for a vertical run the section lies in
                // X/Y; otherwise it is the horizontal normal and straight up.
                QVector3D C, U;
                if (t->type() == Truss::Vertical)
                {
                    C = QVector3D(1, 0, 0);
                    U = QVector3D(0, 1, 0);
                }
                else
                {
                    const QPointF d = t->direction();
                    const double dl = std::hypot(d.x(), d.y());
                    C = (dl > 1e-9) ? QVector3D(float(-d.y() / dl), float(d.x() / dl), 0.0f)
                                    : QVector3D(0, 1, 0);
                    U = QVector3D(0, 0, 1);
                }
                const float hw = qMax(0.05f, t->width()) * 0.5f;
                const QVector3D c1 = C * hw, u1 = U * hw;
                QVector3D corner[8];
                corner[0] = A - c1 - u1; corner[1] = B - c1 - u1;
                corner[2] = B + c1 - u1; corner[3] = A + c1 - u1;
                corner[4] = A - c1 + u1; corner[5] = B - c1 + u1;
                corner[6] = B + c1 + u1; corner[7] = A + c1 + u1;
                drawSolidBox(p, corner, QColor(146, 150, 162), steel.lighter(150));

                // Webbing on whichever long face is nearest, so it still reads
                // as a truss rather than a plain girder.
                const int longFaces[4][4] = { {0,1,5,4}, {2,3,7,6}, {4,5,6,7}, {0,1,2,3} };
                int best = 0; double bestD = 1e18;
                for (int f = 0; f < 4; ++f)
                {
                    double dsum = 0.0;
                    for (int k = 0; k < 4; ++k) dsum += viewDepth(corner[longFaces[f][k]]);
                    if (dsum / 4.0 < bestD) { bestD = dsum / 4.0; best = f; }
                }
                const QVector3D &p0 = corner[longFaces[best][0]];
                const QVector3D &p1 = corner[longFaces[best][1]];
                const QVector3D &p2 = corner[longFaces[best][2]];
                const QVector3D &p3 = corner[longFaces[best][3]];
                const int bays = qMax(1, int(t->length() / qMax(0.35f, t->width() * 2.0f)));
                p.setPen(QPen(steel.lighter(135), 1.1));
                for (int i = 0; i < bays; ++i)
                {
                    const float s0 = float(i) / bays, s1 = float(i + 1) / bays;
                    const QVector3D lo0 = p0 + (p1 - p0) * s0, lo1 = p0 + (p1 - p0) * s1;
                    const QVector3D hi0 = p3 + (p2 - p3) * s0, hi1 = p3 + (p2 - p3) * s1;
                    if (i % 2 == 0) p.drawLine(w2s(lo0), w2s(hi1));
                    else            p.drawLine(w2s(hi0), w2s(lo1));
                }
            }
            return;
        }

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
    else if (kind == PlatformKind)
    {
        StagePlatform *pl = props->platform(id);
        if (pl == nullptr) return;
        const float x0 = pl->originX(), y0 = pl->originY();
        const float x1 = x0 + pl->width(), y1 = y0 + pl->depth(), h = pl->height();
        // Use the platform's own colour (its colour-picker value), like the 2D map.
        QColor pc = pl->color().isValid() ? pl->color() : QColor(110, 120, 140);
        /* Solid. These were alpha 70, so decks read as panes of glass: stacked
           steps showed through each other and you could see the floor through a
           riser. Fixtures are painted after the structures, so they still show
           on top of the deck they sit on. */
        QColor fill = pc; fill.setAlpha(235);
        // Crisp edge: a dark halo under a bright outline so the outline reads on
        // the dark canvas AND where platforms overlap (stacked steps).
        const QColor halo = pc.darker(230);
        const QColor edge = pc.lighter(165);
        // Two-pass outline helper (halo first, bright edge on top).
        auto outline = [&](const QRectF &r) {
            p.setBrush(fill);
            p.setPen(QPen(halo, 3.2));
            p.drawRect(r);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(edge, 1.6));
            p.drawRect(r);
        };
        if (m_plane == Angled)
        {
            // A deck is a solid box, not a pane: project its real corners.
            const float b0 = props->platformBaseZ(id);
            QVector3D c[8];
            boxCorners(c, x0, y0, b0, x1, y1, b0 + h);
            drawSolidBox(p, c, pc, edge);
        }
        else if (m_plane == Top)
        {
            outline(QRectF(w2s(QVector3D(x0, y0, 0)), w2s(QVector3D(x1, y1, 0))).normalized());
        }
        else
        {
            // Riser box silhouette from its BASE (which may sit on a lower
            // platform, not the floor) up to its deck top (base + own thickness).
            const float b0  = props->platformBaseZ(id);
            const float top = b0 + h;
            const QPointF a = w2s(QVector3D(x0, y0, b0));
            const QPointF b = w2s(QVector3D(x1, y1, top));
            outline(QRectF(a, b).normalized());
            p.setPen(QPen(edge.lighter(115), 1.8));   // deck line (bright, on top)
            p.drawLine(w2s(QVector3D(x0, y0, top)), w2s(QVector3D(x1, y1, top)));
        }
    }
    else if (kind == PipeKind)
    {
        Pipe *pipe = props->pipe(id);
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
            if (cb->isBarOnPipe() && cb->parentPipeId() == id)
                drawPipe(p, cb);
    }
    else if (kind == GroupKind)
    {
        // A studio frame group: mark its local-frame origin (fixtures draw on top).
        const QPointF o = w2s(props->group(id).origin);
        p.setPen(QPen(QColor(120, 160, 200, 160), 1.0, Qt::DashLine));
        p.drawLine(o - QPointF(9, 0), o + QPointF(9, 0));
        p.drawLine(o - QPointF(0, 9), o + QPointF(0, 9));
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

bool StructureStudioView::hasStructuralMount(const FixtureRigProps &rp)
{
    return rp.trussId != Truss::invalidId() || rp.onPipe() || rp.onTower()
        || rp.onRiser() || rp.onDeck();
}

QVector3D StructureStudioView::fixtureEndA(quint32 fid) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const double half = qMax(0.05, fixtureLenM(fid) / 2.0);
    const quint32 fg = hasStructuralMount(rp) ? 0 : props->fixtureFrameGroup(fid);
    if (fg != 0)
        return props->groupLocalToWorld(fg, rp.groupLocal - fixtureAxisLocal(rp) * float(half));
    // Non-frame fixture: orient the bar by its studioMount/angle in world too, so
    // Face + Angle work for a truss/riser strip ("which way it runs").
    return props->fixtureRigPosition(fid) - fixtureAxisLocal(rp) * float(half);
}

QVector3D StructureStudioView::fixtureEndB(quint32 fid) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const double half = qMax(0.05, fixtureLenM(fid) / 2.0);
    const quint32 fg = hasStructuralMount(rp) ? 0 : props->fixtureFrameGroup(fid);
    if (fg != 0)
        return props->groupLocalToWorld(fg, rp.groupLocal + fixtureAxisLocal(rp) * float(half));
    return props->fixtureRigPosition(fid) + fixtureAxisLocal(rp) * float(half);
}

void StructureStudioView::facePin(int mount, int &pinComp, double &pinVal) const
{
    if (mount == 1)      pinComp = 1;    // Front → pin Y (depth)
    else if (mount == 2) pinComp = 0;    // Side  → pin X
    else                 pinComp = 2;    // Top   → pin Z (height)

    MonitorProperties *props = m_doc->monitorProperties();
    StagePlatform *pl = (m_kind == PlatformKind) ? props->platform(m_id) : nullptr;
    if (pl != nullptr)
    {
        pinVal = (pinComp == 1) ? pl->depth() : (pinComp == 2) ? pl->height() : 0.0;
        return;
    }
    // A bare group (no platform): pin to the members' AVERAGE on that component.
    double sum = 0.0; int n = 0;
    foreach (quint32 fid, mountedFixtures())
    {
        const QVector3D lp = props->fixtureRigProps(fid).groupLocal;
        sum += (pinComp == 0) ? lp.x() : (pinComp == 1) ? lp.y() : lp.z();
        ++n;
    }
    pinVal = n ? sum / n : 0.0;
}

QRectF StructureStudioView::towerFixtureBodyRect(quint32 fid) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const QPointF c = w2s(props->fixtureRigPosition(fid));
    const double halfBase = qMax(6.0, fixtureLenM(fid) * 0.5 * m_scale);
    const double bodyH = halfBase * 1.4;
    const bool hung = (rp.mountingType == Truss::TopHung);
    // Base (full width) is always flush with the shelf line at c.y(); the body
    // extends away from it -- upward (smaller y) when sitting, downward when hung.
    const double top = hung ? c.y() : c.y() - bodyH;
    return QRectF(c.x() - halfBase, top, halfBase * 2.0, bodyH);
}

// ---------------------------------------------------------------------------
// Fixture visual classification -- so a moving-head mover, a PAR-style can
// and an LED bar actually look different on the canvas instead of every
// fixture drawing as the same generic bar-with-dots regardless of what it
// really is. Everything here reads data ALREADY present in every fixture
// definition (Type, per-channel Group/Colour/Preset, Physical dimensions) --
// no new fixture-def schema, so it applies retroactively to the whole
// existing library, not just fixtures someone hand-tags going forward.
// ---------------------------------------------------------------------------

// FixtureSilhouette / FixtureVisualTraits / classifyFixture() now live in
// fixturevisualtraits.{h,cpp}, shared with MonitorFixtureItem's 2D plan view
// so both renderers agree on what a fixture "is" from the same classifier.

/* How wide a mover unit is on screen, in total.
 *
 * The declared Physical width is the width of the WHOLE fixture, heads
 * included -- a Junman "Two Arm LED Beam" says 510 mm for the entire bar. */
double StructureStudioView::moverWidthPx(const FixtureVisualTraits &traits) const
{
    if (traits.physW > 0.0f)
        return qMax(12.0, double(traits.physW) * m_scale);
    // Nothing declared: a sane default per head.
    return (traits.hasFocus ? 18.0 : 13.0) * qMax(1, traits.headCount);
}

/* The radius of ONE head, which is half its slice of the fixture's width.
 *
 * This used to return half the whole fixture's width and then draw every head
 * at 1.3x that, spaced 2.6x apart -- so an N-head fixture came out about 3.9*N
 * times its real size. The comment beside the drawing code already said each
 * head should sit "in its own equal slice of the fixture's width"; the
 * arithmetic just did not. The eight 3-head UST beams were drawn nearly four
 * times too big because of it. */
double StructureStudioView::moverBaseRadius(const FixtureVisualTraits &traits) const
{
    const int units = qMax(1, traits.headCount);
    return qMax(3.0, moverWidthPx(traits) / (2.0 * units));
}

/* The fixture's own box, projected into the current plane.
 *
 * A fixture is a W x H x D box in its own frame: its LONG axis (fixtureAxisLocal,
 * from studioMount + studioAngle) carries the declared Width, the plane normal of
 * that mount carries the Depth, and what is left carries the Height. Each view
 * sees two of those three, and which two depends on how the fixture is turned --
 * so the on-screen body has to be PROJECTED, not assumed.
 *
 * Without this a fixture was drawn as a line from fixtureEndA() to fixtureEndB()
 * with its across-extent taken from physH regardless of view. Two things went
 * wrong: in the SIDE view of a front-facing panel the long axis points into the
 * screen, so both ends landed on the same pixel and the fixture collapsed to a
 * dot with no body at all; and the TOP view drew it Height-tall when a plan view
 * should see its Depth.
 *
 * Returns the screen-space vectors spanning the full width (@p wPx) and height
 * (@p hPx) of the fixture, plus the axis-aligned screen box that contains the
 * whole solid including its depth. A span that points into the screen comes back
 * near zero, which is exactly right: the pixels along it then stack on top of
 * each other and the depth is what still gives the body its size. */
void StructureStudioView::fixtureBoxPx(quint32 fid, const FixtureVisualTraits &traits,
                                       QPointF &wPx, QPointF &hPx, QRectF &boxPx) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const QPointF c = w2s(props->fixtureRigPosition(fid));

    const QVector3D L = fixtureAxisLocal(rp);          // along the length
    QVector3D N;                                       // the mount plane's normal = depth
    switch (rp.studioMount)
    {
    case 0:  N = QVector3D(0, 0, 1); break;            // laid flat: depth is up
    case 2:  N = QVector3D(1, 0, 0); break;            // side face: depth across stage
    case 1:
    default: N = QVector3D(0, 1, 0); break;            // front face: depth upstage
    }
    QVector3D H = QVector3D::crossProduct(N, L);       // the remaining in-face axis
    if (H.length() < 1e-6f)
        H = QVector3D(0, 0, 1);
    H.normalize();

    const double w = (traits.physW > 0.0f) ? double(traits.physW) : qMax(0.05, fixtureLenM(fid));
    const double h = (traits.physH > 0.0f) ? double(traits.physH) : w * 0.2;
    const double d = (traits.physD > 0.0f) ? double(traits.physD) : h;

    // A world vector's screen delta: w2s() is affine, so the offset from the
    // centre is enough and the origin cancels.
    const QVector3D worldC = props->fixtureRigPosition(fid);
    auto span = [&](const QVector3D &axis, double len) {
        return w2s(worldC + axis * float(len)) - c;
    };

    wPx = span(L, w);
    hPx = span(H, h);
    const QPointF dPx = span(N, d);

    // Half-extents of the projected solid: each edge contributes its absolute
    // screen projection on each axis.
    const double halfX = 0.5 * (qAbs(wPx.x()) + qAbs(hPx.x()) + qAbs(dPx.x()));
    const double halfY = 0.5 * (qAbs(wPx.y()) + qAbs(hPx.y()) + qAbs(dPx.y()));
    boxPx = QRectF(c.x() - halfX, c.y() - halfY,
                   qMax(3.0, halfX * 2.0), qMax(3.0, halfY * 2.0));
}

/* The eight world corners of a fixture's own W x H x D box.
 *
 * fixtureBoxPx() returns the axis-aligned SCREEN rectangle that contains this,
 * which is all a flat view needs. At an angle that rectangle is a billboard: the
 * body stayed square-on to the viewer while its pixels projected properly, so a
 * bar drew as a rectangle with a diagonal strip through it, and a moving head
 * appeared to swivel to follow the camera. Projecting these corners instead
 * makes a fixture sit still in the rig as you orbit around it. */
void StructureStudioView::fixtureBoxCorners(quint32 fid, const FixtureVisualTraits &traits,
                                            QVector3D out[8]) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const QVector3D c = props->fixtureRigPosition(fid);

    const QVector3D L = fixtureAxisLocal(rp);
    QVector3D N;
    switch (rp.studioMount)
    {
    case 0:  N = QVector3D(0, 0, 1); break;
    case 2:  N = QVector3D(1, 0, 0); break;
    case 1:
    default: N = QVector3D(0, 1, 0); break;
    }
    QVector3D H = QVector3D::crossProduct(N, L);
    if (H.length() < 1e-6f)
        H = QVector3D(0, 0, 1);
    H.normalize();

    const double w = (traits.physW > 0.0f) ? double(traits.physW) : qMax(0.05, fixtureLenM(fid));
    const double h = (traits.physH > 0.0f) ? double(traits.physH) : w * 0.2;
    const double d = (traits.physD > 0.0f) ? double(traits.physD) : h;

    const QVector3D l = L * float(w * 0.5);
    const QVector3D u = H * float(h * 0.5);
    const QVector3D n = N * float(d * 0.5);

    out[0] = c - l - u - n; out[1] = c + l - u - n;
    out[2] = c + l - u + n; out[3] = c - l - u + n;
    out[4] = c - l + u - n; out[5] = c + l + u - n;
    out[6] = c + l + u + n; out[7] = c - l + u + n;
}

void StructureStudioView::drawFixtures(QPainter &p) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    p.setFont(QFont("Arial", 8));
    /* Names are useful when a handful of fixtures are on one structure; across
       the WHOLE rig they are a wall of overlapping text that hides the thing
       you opened the overview to look at. Show them only for what is selected
       there. */
    const bool nameEveryone = (m_kind != StageKind);
    foreach (quint32 fid, mountedFixtures())
    {
        Fixture *fx = m_doc->fixture(fid);
        const QPointF c = w2s(props->fixtureRigPosition(fid));
        const bool hi = m_highlight.contains(fid);
        const bool drag = (fid == m_dragFid);

        QColor col = props->fixtureGelColor(fid, 0, 0);
        if (!col.isValid() || col == QColor(Qt::black))
            col = QColor(90, 160, 235);
        if (drag)     col = QColor(255, 196, 64);
        else if (hi)  col = QColor(120, 220, 140);

        // Tower-shelf mount, in an elevation: a fixture resting on (or hung
        // under) a shelf isn't "a bar running along something" the way a
        // truss/pipe mount is — it's a unit sitting ON a surface, or hanging
        // FROM one, and needs to actually read as a body with real size doing
        // that, not a floating bar. Draw a sized trapezoid instead of the
        // generic bar+head-ticks: base flush with the shelf, body rising
        // above it when sitting (FloorMounted), or the SAME shape mirrored —
        // base flush with the shelf, body hanging below — when TopHung. The
        // mirroring is what makes "hung" actually look inverted rather than
        // just "the same icon, slightly lower."
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        const FixtureVisualTraits traits = classifyFixture(fx);

        if (m_plane == Angled)
        {
            /* One rule for every fixture kind here: a solid box in its own
               orientation. The flat views' silhouettes (mover yokes, bar
               tick-marks) are drawn in SCREEN space and would swing round to
               face the camera, which is exactly what "the moving heads follow
               me" was. */
            QVector3D corner[8];
            fixtureBoxCorners(fid, traits, corner);
            drawSolidBox(p, corner, col, col.lighter(150));

            // Pixels on the face that is pointing at us, when a grid is declared.
            if (traits.layout.isValid())
            {
                const int cols = traits.layout.width(), rows = traits.layout.height();
                const QVector3D &f0 = corner[4], &f1 = corner[5];
                const QVector3D &b0 = corner[0], &b1 = corner[1];
                p.setPen(Qt::NoPen);
                p.setBrush(col.lighter(135));
                int placed = 0;
                for (int r = 0; r < rows && placed < traits.headCount; ++r)
                {
                    const float fr = (rows > 1) ? float(r) / (rows - 1) : 0.5f;
                    for (int cx = 0; cx < cols && placed < traits.headCount; ++cx, ++placed)
                    {
                        const float fc = (cols > 1) ? float(cx) / (cols - 1) : 0.5f;
                        const QVector3D lo = b0 + (b1 - b0) * fc;
                        const QVector3D hi = f0 + (f1 - f0) * fc;
                        p.drawEllipse(w2s(lo + (hi - lo) * fr), 1.2, 1.2);
                    }
                }
            }
            if (hi && fx != nullptr)
            {
                p.setPen(QColor(210, 214, 220));
                p.drawText(w2s(corner[6]) + QPointF(6, -4), fx->name());
            }
            continue;
        }

        if (rp.towerId != Tower::invalidId() && (m_plane == Front || m_plane == Side))
        {
            const bool hung = (rp.mountingType == Truss::TopHung);
            const QRectF r = towerFixtureBodyRect(fid);   // shared with hitTestFixture()
            QPainterPath bodyPath;
            if (traits.kind == FixtureSilhouette::Mover)
            {
                // Same yoke silhouette as everywhere else a Mover is drawn,
                // instead of the generic sitting/hanging trapezoid below --
                // "I should see the same figure" in the tower/truss editor.
                bodyPath = moverElevationPath(r, hung, traits.headCount);
            }
            else
            {
                const double inset = r.width() * 0.175;       // tapers toward the free end
                QPolygonF bodyPoly;
                if (!hung)   // base (full width) at the shelf line = rect bottom
                    bodyPoly << r.bottomLeft() << r.bottomRight()
                             << QPointF(r.right() - inset, r.top()) << QPointF(r.left() + inset, r.top());
                else         // base (full width) at the shelf line = rect top
                    bodyPoly << r.topLeft() << r.topRight()
                             << QPointF(r.right() - inset, r.bottom()) << QPointF(r.left() + inset, r.bottom());
                bodyPath.addPolygon(bodyPoly);
                bodyPath.closeSubpath();
            }
            if (hi)
            {
                QPen halo(QColor(120, 220, 140, 160)); halo.setWidth(3);
                p.setPen(halo); p.setBrush(Qt::NoBrush);
                p.drawPath(bodyPath);
            }
            p.setPen(QPen(col.darker(150), 1.4));
            p.setBrush(col);
            p.drawPath(bodyPath);
            if (fx != nullptr)
            {
                p.setPen(QColor(210, 214, 220));
                if (nameEveryone || hi)
                    p.drawText(QPointF(r.right() + 4, c.y() - 6), fx->name());
            }
            continue;
        }

        const QPointF a = w2s(fixtureEndA(fid));
        const QPointF b = w2s(fixtureEndB(fid));

        if (traits.kind == FixtureSilhouette::Mover)
        {
            // A compact head unit, not a bar -- a mover isn't "a bar running
            // along something." Sized from its declared Physical width when
            // there is one, else the hasFocus heuristic (see
            // moverBaseRadius()). Top plane: a round head in a square base
            // footprint ("circles in a square"), one unit per physical head,
            // side by side (e.g. a twin-head wash bar draws as two). Front/
            // Side: the same base+yoke-arms+head silhouette as the
            // tower-shelf case above, one full unit per head, each in its
            // own equal slice of the fixture's width.
            const double baseR = moverBaseRadius(traits);
            const int units = qMax(1, traits.headCount);
            if (m_plane == Top)
            {
                const QPointF dir = b - a;
                const double dlen = qSqrt(dir.x() * dir.x() + dir.y() * dir.y());
                const QPointF unit = (dlen > 1e-6) ? dir / dlen : QPointF(1, 0);
                const double spacing = baseR * 2.0;   // == one slice
                for (int u = 0; u < units; ++u)
                {
                    const double off = (u - (units - 1) * 0.5) * spacing;
                    const QPointF hc = c + unit * off;
                    const double half = baseR;
                    const QPainterPath body = moverPlanPath(QRectF(hc.x() - half, hc.y() - half, half * 2, half * 2));
                    if (hi)
                    {
                        QPen halo(QColor(120, 220, 140, 160)); halo.setWidth(3);
                        p.setPen(halo); p.setBrush(Qt::NoBrush);
                        p.drawPath(body);
                    }
                    p.setPen(QPen(col.darker(150), 1.4));
                    p.setBrush(col);
                    p.drawPath(body);
                }
            }
            else
            {
                const double halfW = moverWidthPx(traits) * 0.5;
                const double totalH = baseR * 3.2;
                const QPainterPath body = moverElevationPath(
                    QRectF(c.x() - halfW, c.y() - totalH * 0.5, halfW * 2, totalH), false, units);
                if (hi)
                {
                    QPen halo(QColor(120, 220, 140, 160)); halo.setWidth(3);
                    p.setPen(halo); p.setBrush(Qt::NoBrush);
                    p.drawPath(body);
                }
                p.setPen(QPen(col.darker(150), 1.4));
                p.setBrush(col);
                p.drawPath(body);
            }
        }
        else if (traits.kind == FixtureSilhouette::Par)
        {
            // A can/wash unit: filled rounded rect sized from its real
            // declared physical footprint when there is one, a sane default
            // otherwise (most bundled defs still leave Width/Height at 0).
            const double wPx = (traits.physW > 0.0f) ? qMax(8.0, double(traits.physW) * m_scale) : 10.0;
            const double hPx = (traits.physH > 0.0f) ? qMax(8.0, double(traits.physH) * m_scale) : wPx * 0.85;
            const QRectF r(c.x() - wPx * 0.5, c.y() - hPx * 0.5, wPx, hPx);
            if (hi)
            {
                QPen halo(QColor(120, 220, 140, 160)); halo.setWidth(3);
                p.setPen(halo); p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(r.adjusted(-3, -3, 3, 3), 3, 3);
            }
            p.setPen(QPen(col.darker(150), 1.4));
            p.setBrush(col);
            p.drawRoundedRect(r, 2, 2);
        }
        else if (traits.kind == FixtureSilhouette::Bar)
        {
            // A genuine matrix/panel (declared height is a meaningful
            // fraction of its width, not just a thin strip, and there are
            // enough pixels to actually form rows) draws as a grid instead
            // of a single line, so a panel actually looks like a panel and a
            // long single-row bar still looks like a bar.
            const bool isMatrix = traits.layout.isValid()
                                 || (traits.physW > 0.0f && traits.physH > traits.physW * 0.15f
                                     && traits.headCount >= 4);
            if (hi)
            {
                QPen halo(QColor(120, 220, 140, 160)); halo.setWidth(9); halo.setCapStyle(Qt::RoundCap);
                p.setPen(halo);
                p.drawLine(a, b);
            }
            if (isMatrix)
            {
                /* Honour the definition's declared grid (an XL-450 says
                   15 x 5); only guess a grid from head count and aspect when
                   there is nothing declared. */
                int rows, cols;
                if (traits.layout.isValid())
                {
                    cols = traits.layout.width();
                    rows = traits.layout.height();
                }
                else
                {
                    const double aspect = double(traits.physH / traits.physW);
                    rows = qBound(2, int(qRound(qSqrt(double(traits.headCount) * aspect))),
                                  traits.headCount);
                    cols = qMax(1, (traits.headCount + rows - 1) / rows);
                }

                /* Place every pixel at its TRUE position in the fixture's box
                   and project that -- so the grid squashes correctly when an
                   axis turns away from the viewer instead of being drawn along
                   the a-b line at a fixed across-extent. */
                QPointF wPx, hPx; QRectF boxPx;
                fixtureBoxPx(fid, traits, wPx, hPx, boxPx);

                p.setPen(QPen(col.darker(140), (drag || hi) ? 2.0 : 1.2));
                p.setBrush(Qt::NoBrush);
                p.drawRect(boxPx);                    // the body, at its real proportions

                p.setPen(Qt::NoPen);
                p.setBrush(col);
                // A pixel's dot: its own cell, never bigger than it should be.
                const double cellW = boxPx.width() / qMax(1, cols);
                const double cellH = boxPx.height() / qMax(1, rows);
                const double rad = qBound(0.8, qMin(cellW, cellH) * 0.42, 3.0);
                int placed = 0;
                for (int r = 0; r < rows && placed < traits.headCount; ++r)
                {
                    const double fy = (rows > 1) ? (double(r) / (rows - 1) - 0.5) : 0.0;
                    for (int cix = 0; cix < cols && placed < traits.headCount; ++cix, ++placed)
                    {
                        const double fx2 = (cols > 1) ? (double(cix) / (cols - 1) - 0.5) : 0.0;
                        p.drawEllipse(c + wPx * fx2 + hPx * fy, rad, rad);
                    }
                }
            }
            else
            {
                QPen body(col.darker(140)); body.setWidth((drag || hi) ? 4 : 3); body.setCapStyle(Qt::RoundCap);
                p.setPen(body);
                p.drawLine(a, b);
                p.setPen(Qt::NoPen);
                p.setBrush(col);
                for (int i = 0; i < traits.headCount; ++i)
                {
                    const double t = (traits.headCount > 1) ? double(i) / (traits.headCount - 1) : 0.5;
                    p.drawEllipse(a + (b - a) * t, 2.6, 2.6);
                }
            }
        }
        else
        {
            // Generic fallback -- unchanged original bar+dots rendering, for
            // any fixture Type this session didn't give a dedicated shape
            // (Laser/Hazer/Smoke/Fan/Flower/Effect/Other).
            if (hi)
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
            for (int i = 0; i < traits.headCount; ++i)
            {
                const double t = (traits.headCount > 1) ? double(i) / (traits.headCount - 1) : 0.5;
                p.drawEllipse(a + (b - a) * t, 2.6, 2.6);
            }
        }

        if (fx != nullptr)
        {
            p.setPen(QColor(210, 214, 220));
            if (nameEveryone || hi)
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
    FixtureRigProps rp = props->fixtureRigProps(fid);
    rp.studioMount = face;
    // A frame-group fixture also re-pins to the face surface; a plain (truss/
    // riser) strip just changes which plane its bar lies in.
    if (props->fixtureFrameGroup(fid) != 0)
    {
        int pinComp; double pinVal; facePin(face, pinComp, pinVal);
        QVector3D lp = rp.groupLocal;
        if (pinComp == 0)      lp.setX(float(pinVal));
        else if (pinComp == 1) lp.setY(float(pinVal));
        else                   lp.setZ(float(pinVal));
        rp.groupLocal = lp;
    }
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

void StructureStudioView::drawDimensions(QPainter &p) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    QList<QVector3D> c;   // structure-only corners (no fixtures)
    if (m_kind == PlatformKind)
    { if (StagePlatform *pl = props->platform(m_id)) for (int i = 0; i < 8; ++i)
        c << QVector3D(pl->originX() + ((i & 1) ? pl->width() : 0.0f),
                       pl->originY() + ((i & 2) ? pl->depth() : 0.0f), (i & 4) ? pl->height() : 0.0f); }
    else if (m_kind == TowerKind)
    { if (Tower *t = props->tower(m_id)) c << QVector3D(t->originX(), t->originY(), 0)
                                           << QVector3D(t->originX() + t->width(), t->originY() + t->depth(), t->height()); }
    else if (m_kind == StandKind)
    { if (Stand *s = props->stand(m_id)) { c << QVector3D(s->originX(), s->originY(), 0) << s->topPos();
        foreach (const Pipe *pp, standPipes(m_id)) c << pp->positionAt(0) << pp->positionAt(pp->length()); } }
    else if (m_kind == TrussKind)
    { if (Truss *t = props->truss(m_id)) c << t->origin() << t->positionAt(t->length()); }
    else if (m_kind == PipeKind)
    { if (Pipe *pp = props->pipe(m_id)) c << pp->positionAt(0) << pp->positionAt(pp->length()); }
    if (c.size() < 2) return;

    double minA = 0, maxA = 0, minB = 0, maxB = 0;   // plane-metric extents
    double sMinX = 0, sMaxX = 0, sMinY = 0, sMaxY = 0;   // screen bbox
    bool first = true;
    foreach (const QVector3D &w, c)
    {
        const QPointF ab = project(w);
        const QPointF s = w2s(w);
        if (first) { minA = maxA = ab.x(); minB = maxB = ab.y();
                     sMinX = sMaxX = s.x(); sMinY = sMaxY = s.y(); first = false; }
        minA = qMin(minA, ab.x()); maxA = qMax(maxA, ab.x());
        minB = qMin(minB, ab.y()); maxB = qMax(maxB, ab.y());
        sMinX = qMin(sMinX, s.x()); sMaxX = qMax(sMaxX, s.x());
        sMinY = qMin(sMinY, s.y()); sMaxY = qMax(sMaxY, s.y());
    }
    const bool feet = props->gridUnits() == MonitorProperties::Feet;
    const double conv = feet ? 3.28084 : 1.0;
    const QString sfx = feet ? QStringLiteral(" ft") : QStringLiteral(" m");
    const double wSpan = (maxA - minA) * conv, hSpan = (maxB - minB) * conv;

    p.setPen(QPen(QColor(150, 160, 180, 200), 1.0));
    p.setFont(QFont("Arial", 8));
    // Horizontal dimension, just below the structure.
    if (wSpan > 0.01)
    {
        const double y = sMaxY + 18.0;
        p.drawLine(QPointF(sMinX, y), QPointF(sMaxX, y));
        p.drawLine(QPointF(sMinX, y - 4), QPointF(sMinX, y + 4));
        p.drawLine(QPointF(sMaxX, y - 4), QPointF(sMaxX, y + 4));
        p.drawText(QRectF(sMinX, y + 2, sMaxX - sMinX, 14), Qt::AlignCenter,
                   QString("%1%2").arg(wSpan, 0, 'f', 1).arg(sfx));
    }
    // Vertical dimension, just left of the structure.
    if (hSpan > 0.01)
    {
        const double x = sMinX - 20.0;
        p.drawLine(QPointF(x, sMinY), QPointF(x, sMaxY));
        p.drawLine(QPointF(x - 4, sMinY), QPointF(x + 4, sMinY));
        p.drawLine(QPointF(x - 4, sMaxY), QPointF(x + 4, sMaxY));
        p.save();
        p.translate(x - 4, (sMinY + sMaxY) / 2.0);
        p.rotate(-90);
        p.drawText(QRectF(-40, -12, 80, 12), Qt::AlignCenter,
                   QString("%1%2").arg(hSpan, 0, 'f', 1).arg(sfx));
        p.restore();
    }
}

double StructureStudioView::structureCentreA() const
{
    MonitorProperties *props = m_doc->monitorProperties();
    QList<QVector3D> c;
    if (m_kind == PlatformKind)
    { if (StagePlatform *pl = props->platform(m_id)) { c << QVector3D(pl->originX(), pl->originY(), 0)
        << QVector3D(pl->originX() + pl->width(), pl->originY() + pl->depth(), pl->height()); } }
    else if (m_kind == TowerKind)
    { if (Tower *t = props->tower(m_id)) { c << QVector3D(t->originX(), t->originY(), 0)
        << QVector3D(t->originX() + t->width(), t->originY() + t->depth(), t->height()); } }
    else if (m_kind == StandKind)
    { if (Stand *s = props->stand(m_id)) c << QVector3D(s->originX(), s->originY(), 0) << s->topPos(); }
    else if (m_kind == TrussKind)
    { if (Truss *t = props->truss(m_id)) c << t->origin() << t->positionAt(t->length()); }
    else if (m_kind == PipeKind)
    { if (Pipe *pp = props->pipe(m_id)) c << pp->positionAt(0) << pp->positionAt(pp->length()); }
    else if (m_kind == GroupKind)
    { foreach (quint32 fid, mountedFixtures()) c << props->fixtureRigPosition(fid);
      if (c.isEmpty()) c << props->group(m_id).origin; }
    if (c.isEmpty()) return 0.0;
    /* Along whichever in-plane axis the horizontal ruler is measuring: an odd
       quarter turn swaps a and b on screen, and a ruler centred on the wrong
       one puts its zero somewhere off the structure. */
    const bool useB = (m_rotation & 1);
    auto comp = [useB](const QPointF &ab) { return useB ? ab.y() : ab.x(); };
    double minA = comp(project(c.first())), maxA = minA;
    foreach (const QVector3D &w, c)
    { const double a = comp(project(w)); minA = qMin(minA, a); maxA = qMax(maxA, a); }
    return (minA + maxA) / 2.0;
}

double StructureStudioView::structureTopZ() const
{
    MonitorProperties *props = m_doc->monitorProperties();
    double z = 0.0;
    if (m_kind == StandKind)
    { if (Stand *s = props->stand(m_id)) { z = s->height();
        foreach (const Pipe *pp, standPipes(m_id)) z = qMax(z, double(pp->positionAt(pp->length()).z())); } }
    else if (m_kind == TowerKind)   { if (Tower *t = props->tower(m_id)) z = t->height(); }
    else if (m_kind == PlatformKind){ if (StagePlatform *pl = props->platform(m_id)) z = pl->height(); }
    else if (m_kind == TrussKind)   { if (Truss *t = props->truss(m_id))
        z = qMax(double(t->origin().z()), double(t->positionAt(t->length()).z())); }
    else if (m_kind == PipeKind)    { if (Pipe *pp = props->pipe(m_id))
        z = qMax(double(pp->positionAt(0).z()), double(pp->positionAt(pp->length()).z())); }
    else if (m_kind == GroupKind)
    { foreach (quint32 fid, mountedFixtures()) z = qMax(z, double(props->fixtureRigPosition(fid).z())); }
    return z;
}

/* A world point that varies along one of the two in-plane axes, so the rulers
 * can ask for "the axis that is vertical on screen right now" instead of
 * assuming it. Which of a/b that is depends on the view rotation. */
QVector3D StructureStudioView::axisWorldPoint(bool useB, double val) const
{
    const float v = float(val);
    if (useB)                                       // b: Top -> Y, elevations -> Z
        return (m_plane == Top) ? QVector3D(0, v, 0) : QVector3D(0, 0, v);
    return (m_plane == Side) ? QVector3D(0, v, 0)   // a: Side -> Y, else X
                             : QVector3D(v, 0, 0);
}

void StructureStudioView::drawRulers(QPainter &p) const
{
    /* The tick rulers measure ONE world axis along each screen edge, which only
       means anything when the view is axis-aligned. In the angled view every
       screen direction mixes two axes, so a ruler would put confident numbers
       on a distance nobody asked about. The dimension annotations below still
       work (they measure the drawn extent), and the axis tripod says which way
       is which. */
    if (m_plane == Angled)
        return;

    // Match the main-window ruler bars: dark band, 9px font, a number at every
    // sensibly-spaced unit, blue axis label, blue cursor.
    MonitorProperties *props = m_doc->monitorProperties();
    const bool feet = props->gridUnits() == MonitorProperties::Feet;
    const double conv = feet ? 3.28084 : 1.0;
    const QString sfx = feet ? QStringLiteral("ft") : QStringLiteral("m");
    const double stepM = 1.0 / conv;                 // one display unit, in metres
    const double pxPerUnit = stepM * m_scale;        // its width in pixels
    // Choose a label interval that keeps ~34 px between numbers.
    int lblEvery = 1;
    for (int cand : { 1, 2, 5, 10, 20, 50, 100 })
        { lblEvery = cand; if (cand * pxPerUnit >= 34.0) break; }

    const QColor band(26, 26, 26), edge(60, 60, 60);
    const QColor tickCol(150, 150, 150), textCol(190, 190, 190), axisCol(120, 160, 220);
    const double GW = 34.0, GH = 22.0;
    QFont f = p.font(); f.setPixelSize(9); p.setFont(f);

    const bool vert = (m_plane != Top);
    if (vert)
    {
        p.fillRect(QRectF(0, 0, GW, height()), band);
        p.setPen(edge); p.drawLine(QPointF(GW - 1, 0), QPointF(GW - 1, height()));
        for (int k = 0; k < 400; ++k)
        {
            const double sy = w2s(axisWorldPoint(!(m_rotation & 1), k * stepM)).y();
            if (sy < 12) break;
            if (sy > height()) continue;
            p.setPen(tickCol); p.drawLine(QPointF(GW - 6, sy), QPointF(GW - 1, sy));
            if (k % lblEvery == 0)
            {
                p.setPen(textCol);
                p.drawText(QRectF(0, sy - 8, GW - 8, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(k));
            }
        }
        p.setPen(axisCol); p.drawText(QRectF(2, 1, GW - 2, 12), Qt::AlignLeft | Qt::AlignTop, sfx);
        const double topZ = structureTopZ();
        if (topZ > 0.01)
        {
            const double sy = w2s(axisWorldPoint(!(m_rotation & 1), topZ)).y();
            const QColor mk(80, 170, 255);   // same blue as the main-window ruler cursor
            p.setPen(QPen(mk, 1.0));          // full-width guide line at the max height
            p.drawLine(QPointF(GW, sy), QPointF(width(), sy));
            p.setBrush(mk);
            QPolygonF tri; tri << QPointF(GW + 1, sy) << QPointF(GW + 8, sy - 4) << QPointF(GW + 8, sy + 4);
            p.drawPolygon(tri);
            p.drawText(QPointF(GW + 11, sy - 3), QString("max %1 %2").arg(topZ * conv, 0, 'f', 1).arg(sfx));
        }
    }

    // Width ruler (0 = structure centre) — bottom band.
    const double centreA = structureCentreA();
    const double by = height() - GH;
    p.fillRect(QRectF(0, by, width(), GH), band);
    p.setPen(edge); p.drawLine(QPointF(0, by), QPointF(width(), by));
    for (int k = -200; k <= 200; ++k)
    {
        const double aVal = centreA + k * stepM;
        const double sx = w2s(axisWorldPoint(m_rotation & 1, aVal)).x();
        if (sx < GW || sx > width() + 20) continue;
        p.setPen((k == 0) ? QPen(QColor(0, 190, 255), 1.2) : QPen(tickCol, 1.0));
        p.drawLine(QPointF(sx, by), QPointF(sx, by + 6));
        if (k % lblEvery == 0)
        {
            p.setPen((k == 0) ? QColor(0, 190, 255) : textCol);
            p.drawText(QRectF(sx - 16, by + 6, 32, 14), Qt::AlignHCenter | Qt::AlignVCenter, QString::number(qAbs(k)));
        }
    }
    p.setPen(axisCol); p.drawText(QRectF(GW + 3, by + 4, 40, 14), Qt::AlignLeft | Qt::AlignVCenter,
                                  QString("↔ %1").arg(sfx));

    // Cursor marker (blue, like the main window) + selected-fixture marker (green).
    if (m_hasCursor)
    {
        p.setPen(QPen(QColor(80, 170, 255), 1.0));
        if (vert) p.drawLine(QPointF(0, m_cursorPx.y()), QPointF(GW, m_cursorPx.y()));
        p.drawLine(QPointF(m_cursorPx.x(), by), QPointF(m_cursorPx.x(), height()));
    }
    if (!m_highlight.isEmpty())
    {
        const QPointF s = w2s(props->fixtureRigPosition(*m_highlight.begin()));
        p.setPen(QPen(QColor(120, 220, 140), 1.0)); p.setBrush(QColor(120, 220, 140));
        if (vert) { QPolygonF t; t << QPointF(GW + 1, s.y()) << QPointF(GW + 7, s.y() - 4) << QPointF(GW + 7, s.y() + 4); p.drawPolygon(t); }
        QPolygonF t2; t2 << QPointF(s.x(), by) << QPointF(s.x() - 4, by + 7) << QPointF(s.x() + 4, by + 7); p.drawPolygon(t2);
    }
}

void StructureStudioView::drawCursorReadout(QPainter &p) const
{
    if (!m_hasCursor) return;
    MonitorProperties *props = m_doc->monitorProperties();
    const bool feet = props->gridUnits() == MonitorProperties::Feet;
    const double conv = feet ? 3.28084 : 1.0;
    const QString sfx = feet ? QStringLiteral("ft") : QStringLiteral("m");
    const QPointF ab = screenToPlane(m_cursorPx);
    const double hM = ab.y();                          // height above floor (elevations)
    const double offM = ab.x() - structureCentreA();   // horizontal offset from centre

    p.setPen(QPen(QColor(0, 190, 255, 110), 0.8, Qt::DashLine));
    p.drawLine(QPointF(0, m_cursorPx.y()), QPointF(width(), m_cursorPx.y()));
    p.drawLine(QPointF(m_cursorPx.x(), 0), QPointF(m_cursorPx.x(), height()));

    const QString t = (m_plane == Top)
        ? QString("X %1  Y %2 %3").arg(offM * conv, 0, 'f', 2).arg(hM * conv, 0, 'f', 2).arg(sfx)
        : QString("H %1  ↔ %2 %3").arg(hM * conv, 0, 'f', 2).arg(offM * conv, 0, 'f', 2).arg(sfx);
    p.setFont(QFont("Arial", 8));
    QRectF r(m_cursorPx.x() + 12, m_cursorPx.y() + 10, 140, 18);
    if (r.right() > width()) r.moveLeft(m_cursorPx.x() - 152);
    p.fillRect(r, QColor(20, 22, 28, 225));
    p.setPen(QColor(200, 220, 240));
    p.drawText(r, Qt::AlignVCenter | Qt::AlignHCenter, t);
}

/* Which way is which. The plane badge names the projection but not its
   orientation, so "Side" gave no clue whether the left of the canvas was
   upstage or down -- you had to drag something and watch which way it went.

   The directions below are taken from how the app actually BEHAVES, not from
   the axis comments, because the two disagree. monitorproperties.cpp's
   barFaceVector() says "+Y = downstage (toward audience)" and maps
   FaceDownstage to (0,+1,0); the geometry headers (pipe.h, tower.h,
   stageplatform.h, stagetarget.h, stand.h, truss.h) and monitor.cpp's
   "X (stage right):" / "Y (upstage):" spin-box labels claim the opposite on
   BOTH axes. Real shows settle it: in stage-structures-demo.qxw the "SR Tower"
   sits at X=0.21 and the "SL Tower" at X=11.61, and the upstage platforms are
   at Y=1.53 with the downstage ones at Y=3.97. So +X is stage LEFT and +Y is
   DOWNSTAGE, which is also what puts the plot in the standard ground-plan
   orientation: audience at the bottom of the page, upstage at the top, stage
   right on the viewer's left.

   Screen mapping, from project() and w2s(): the horizontal screen axis always
   increases with the in-plane 'a' component, and the vertical one increases
   with 'b' EXCEPT in Top, where vSign flips it -- which is exactly what makes
   downstage read downward like a plan drawing. */
/* A little corner tripod for the angled view: the three stage axes drawn as
 * they actually project, each labelled. Edge labels would be a lie here -- in an
 * axonometric no single screen edge IS upstage, the direction runs diagonally --
 * so show the axes themselves and let them be read off. */
void StructureStudioView::drawAxisTripod(QPainter &p) const
{
    const QPointF o(width() - 66.0, height() - 62.0);
    const double len = 26.0;

    struct { QVector3D dir; const char *name; QColor col; } axes[] = {
        { QVector3D(1, 0, 0), "SL", QColor(232, 120, 120) },   // +X is stage left
        { QVector3D(0, 1, 0), "DS", QColor(120, 200, 140) },   // +Y is downstage
        { QVector3D(0, 0, 1), "up", QColor(120, 170, 235) },
    };

    QFont f = p.font(); f.setPixelSize(9); p.setFont(f);
    for (const auto &ax : axes)
    {
        // Project the axis the same way everything else is projected, so the
        // tripod always agrees with the drawing it is describing.
        QPointF v = planeToScreenVec(project(ax.dir));
        const double l = std::hypot(v.x(), v.y());
        if (l < 1e-6)
            continue;                       // dead-on into the screen
        v = QPointF(v.x() / l * len, v.y() / l * len);
        p.setPen(QPen(ax.col, 1.4));
        p.drawLine(o, o + v);
        p.drawText(QRectF(o.x() + v.x() * 1.28 - 12, o.y() + v.y() * 1.28 - 7, 24, 14),
                   Qt::AlignCenter, QString::fromLatin1(ax.name));
    }
    p.setPen(QPen(QColor(150, 155, 165), 1.0));
    p.drawEllipse(o, 1.6, 1.6);
}

void StructureStudioView::drawOrientationLabels(QPainter &p) const
{
    if (m_plane == Angled)
    {
        drawAxisTripod(p);      // no edge of an angled view IS one direction
        return;
    }

    QString leftLbl, rightLbl, topLbl, bottomLbl;
    switch (m_plane)
    {
    case Top:                                  // a = X, b = Y (screen-down)
        leftLbl  = tr("stage right");  rightLbl  = tr("stage left");
        topLbl   = tr("upstage");      bottomLbl = tr("downstage");
        break;
    case Front:                                // a = X, b = Z (screen-up)
        leftLbl  = tr("stage right");  rightLbl  = tr("stage left");
        topLbl   = tr("up");           bottomLbl = tr("floor");
        break;
    case Side:                                 // a = Y, b = Z (screen-up)
        leftLbl  = tr("upstage");      rightLbl  = tr("downstage");
        topLbl   = tr("up");           bottomLbl = tr("floor");
        break;
    }

    /* Follow the view rotation: after a quarter turn the edge that WAS the left
       is the top, and so on. Without this a rotated plot would still claim
       upstage was where it used to be -- worse than no label at all. */
    for (int i = 0; i < (m_rotation & 3); ++i)
    {
        const QString l = leftLbl, t = topLbl, r = rightLbl, b = bottomLbl;
        topLbl = l; rightLbl = t; bottomLbl = r; leftLbl = b;   // 90 deg clockwise
    }

    // Inside the drawing area, clear of the rulers drawn along the left edge
    // and the bottom.
    const double GW = 34.0, GH = 22.0;
    const QRectF area(GW + 4, 4, qMax(1.0, width() - GW - 8),
                      qMax(1.0, height() - GH - 8));

    QFont f = p.font(); f.setPixelSize(9); p.setFont(f);
    p.setPen(QColor(120, 160, 220, 190));      // the rulers' axis blue
    p.drawText(area, Qt::AlignVCenter | Qt::AlignLeft,  QStringLiteral("◀ ") + leftLbl);
    p.drawText(area, Qt::AlignVCenter | Qt::AlignRight, rightLbl + QStringLiteral(" ▶"));
    p.drawText(area, Qt::AlignHCenter | Qt::AlignTop,    QStringLiteral("▲ ") + topLbl);
    p.drawText(area, Qt::AlignHCenter | Qt::AlignBottom, QStringLiteral("▼ ") + bottomLbl);
}

void StructureStudioView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    drawGrid(p);
    drawRulers(p);
    drawStructure(p);
    drawFixtures(p);
    drawCursorReadout(p);

    // Plane badge.
    p.setPen(QColor(160, 164, 175));
    // One entry per Plane -- adding Angled to the enum without adding it here
    // read off the end of the array.
    const char *names[] = { "Top", "Front", "Side", "45°" };
    const int idx = qBound(0, int(m_plane), int(sizeof(names) / sizeof(names[0])) - 1);
    p.drawText(rect().adjusted(0, 6, -8, 0), Qt::AlignTop | Qt::AlignRight,
               (m_plane == Angled) ? tr("%1 — view only").arg(names[idx])
                                   : tr("2D — %1").arg(names[idx]));
    drawOrientationLabels(p);
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
        // A boom's top handle (stand view): drag it to resize — only when unlocked.
        if (!m_locked && m_kind == StandKind)
        {
            foreach (const Pipe *pipe, standPipes(m_id))
            {
                if (!pipe->isVertical()) continue;
                if (QLineF(w2s(pipe->positionAt(pipe->length())), e->pos()).length() <= 7.0)
                {
                    m_resizeBoom = pipe->id() + 1;
                    emit editAboutToStart();
                    setCursor(Qt::SizeVerCursor);
                    return;
                }
            }
        }
        m_dragFid = hitTestFixture(e->pos());   // invalidId() if empty space
        m_dragged = false;

        /* In the angled view a drag on EMPTY canvas swings the camera. Fixture
           dragging is refused there anyway (no honest inverse), so the gesture
           is free -- and orbiting by hand beats reaching for a spin box when you
           just want to see behind something. Pressing ON a fixture still selects
           it, so nothing is lost. */
        if (m_plane == Angled && m_dragFid == Fixture::invalidId())
        {
            m_orbiting = true;
            m_orbitLast = e->pos();
            setCursor(Qt::SizeAllCursor);
            return;
        }
        if (m_dragFid != Fixture::invalidId())
        {
            setCursor((m_locked || m_plane == Angled) ? Qt::ArrowCursor
                                                       : Qt::ClosedHandCursor);
            setHighlight({ m_dragFid });
            emit fixtureSelected(m_dragFid);   // selection always works
        }
    }
}

void StructureStudioView::mouseMoveEvent(QMouseEvent *e)
{
    m_cursorPx = e->pos();   // live ruler readout
    m_hasCursor = true;
    if (m_orbiting)
    {
        const QPointF d = e->pos() - m_orbitLast;
        m_orbitLast = e->pos();
        // The SCENE follows the hand, not the camera: drag right and the rig
        // turns to the right, drag down and you drop toward its level.
        setAngledView(m_azimuthDeg - d.x() * 0.4, m_elevationDeg + d.y() * 0.4);
        return;
    }
    if (m_panning)
    {
        m_originPx += e->pos() - m_panLast;
        m_panLast = e->pos();
        update();
        return;
    }
    if (m_resizeBoom != 0)
    {
        if (Pipe *b = m_doc->monitorProperties()->pipe(m_resizeBoom - 1))
        {
            const QPointF ab = screenToPlane(e->pos());
            b->setLength(float(qMax(0.1, ab.y() - double(b->baseZ()))));  // top follows cursor height
        }
        update();
        return;
    }
    if (m_dragFid != Fixture::invalidId() && !m_locked)   // move only when unlocked
    {
        if (!m_dragged)
            emit editAboutToStart();   // snapshot for undo before the first change
        if (dragFixtureTo(m_dragFid, e->pos()))
            m_dragged = true;
    }
    update();   // keep the ruler crosshair/readout live on hover
}

void StructureStudioView::leaveEvent(QEvent *)
{
    m_hasCursor = false;
    update();
}

void StructureStudioView::mouseReleaseEvent(QMouseEvent *)
{
    if (m_orbiting)
    {
        m_orbiting = false;
        setCursor(Qt::ArrowCursor);
        return;
    }
    m_panning = false;
    if (m_resizeBoom != 0)
    {
        m_resizeBoom = 0;
        setCursor(Qt::ArrowCursor);
        m_doc->setModified();
        emit structureChanged();   // refresh the 2D map's boom + fixtures
        reload();
        return;
    }
    if (m_dragFid != Fixture::invalidId())
    {
        setCursor(Qt::ArrowCursor);
        if (m_dragged)
        {
            m_doc->setModified();
            emit fixtureMoved(m_dragFid);
        }
        m_dragFid = Fixture::invalidId();
        m_dragged = false;
    }
}

quint32 StructureStudioView::hitTestFixture(const QPointF &px) const
{
    // Hit anywhere along the LED bar (nearest within a threshold), not just its
    // centre dot — so clicking the bar selects the fixture. A tower-shelf mount
    // in an elevation draws as a body (towerFixtureBodyRect(), see
    // drawFixtures()), not a bar, so it's hit-tested against that same rect
    // instead — previously this always used the bar-line test, which for a
    // tower fixture didn't correspond to anything actually drawn on screen,
    // making it hard to reliably click/grab (the visible body and the
    // clickable area disagreed).
    MonitorProperties *props = m_doc->monitorProperties();
    // NOT 0: QLC+ hands the FIRST fixture in a workspace id 0 (Doc's
    // m_latestFixtureId starts there), so a 0 sentinel made fixture 0
    // indistinguishable from empty space -- it could never be selected,
    // dragged, double-clicked or right-clicked in this editor. The real
    // "no fixture" marker is Fixture::invalidId().
    quint32 best = Fixture::invalidId(); double bestD = 9.0;
    foreach (quint32 fid, mountedFixtures())
    {
        double d;
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        if (rp.towerId != Tower::invalidId() && (m_plane == Front || m_plane == Side))
        {
            QRectF r = towerFixtureBodyRect(fid);
            r.adjust(-3, -3, 3, 3);   // small grab margin, matching the bar test's threshold
            if (r.contains(px))
            {
                d = 0.0;
            }
            else
            {
                const double dx = qMax(qMax(r.left() - px.x(), px.x() - r.right()), 0.0);
                const double dy = qMax(qMax(r.top() - px.y(), px.y() - r.bottom()), 0.0);
                d = qSqrt(dx * dx + dy * dy);
            }
        }
        else
        {
            // Match whichever shape drawFixtures() actually drew for this
            // fixture -- same reasoning as the tower-shelf case above: a
            // Mover/Par draws as a compact unit centred on its position, not
            // a bar spanning fixtureEndA()..fixtureEndB(), so hit-testing
            // against that line (the old, only, test) could miss the shape
            // entirely or hit empty space well past it.
            const FixtureVisualTraits traits = classifyFixture(m_doc->fixture(fid));
            const QPointF a = w2s(fixtureEndA(fid));
            const QPointF b = w2s(fixtureEndB(fid));
            if (traits.kind == FixtureSilhouette::Mover)
            {
                // Bounding-box test matching whichever silhouette
                // drawFixtures() drew for this plane (square-ish for Top,
                // taller for the Front/Side base+arms+head body) -- a plain
                // circle test undershot the square/base corners.
                const QPointF c = w2s(props->fixtureRigPosition(fid));
                const double baseR = moverBaseRadius(traits);
                const int units = qMax(1, traits.headCount);
                QRectF r;
                if (m_plane == Top)
                {
                    const double half = moverWidthPx(traits) * 0.5;
                    r = QRectF(c.x() - half, c.y() - half, half * 2, half * 2);
                }
                else
                {
                    const double halfW = moverWidthPx(traits) * 0.5;
                    const double totalH = baseR * 3.2;
                    r = QRectF(c.x() - halfW, c.y() - totalH * 0.5, halfW * 2, totalH);
                }
                r.adjust(-3, -3, 3, 3);
                if (r.contains(px))
                {
                    d = 0.0;
                }
                else
                {
                    const double dx = qMax(qMax(r.left() - px.x(), px.x() - r.right()), 0.0);
                    const double dy = qMax(qMax(r.top() - px.y(), px.y() - r.bottom()), 0.0);
                    d = qSqrt(dx * dx + dy * dy);
                }
            }
            else if (traits.kind == FixtureSilhouette::Par)
            {
                const QPointF c = w2s(props->fixtureRigPosition(fid));
                const double wPx = (traits.physW > 0.0f) ? qMax(8.0, double(traits.physW) * m_scale) : 10.0;
                const double hPx = (traits.physH > 0.0f) ? qMax(8.0, double(traits.physH) * m_scale) : wPx * 0.85;
                QRectF r(c.x() - wPx * 0.5, c.y() - hPx * 0.5, wPx, hPx);
                r.adjust(-3, -3, 3, 3);
                if (r.contains(px))
                {
                    d = 0.0;
                }
                else
                {
                    const double dx = qMax(qMax(r.left() - px.x(), px.x() - r.right()), 0.0);
                    const double dy = qMax(qMax(r.top() - px.y(), px.y() - r.bottom()), 0.0);
                    d = qSqrt(dx * dx + dy * dy);
                }
            }
            else
            {
                // Bar (matrix or single-row) and Generic: the drawn shape is
                // still centred on the a-b line, just wider for a matrix --
                // widen the threshold by half its declared physical height
                // so a click on an off-centre row still finds it.
                const QPointF ab = b - a;
                const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
                double t = (len2 > 1e-6) ? ((px.x() - a.x()) * ab.x() + (px.y() - a.y()) * ab.y()) / len2 : 0.0;
                t = qBound(0.0, t, 1.0);
                d = QLineF(a + ab * t, px).length();
                const bool isMatrix = traits.kind == FixtureSilhouette::Bar
                                     && traits.physW > 0.0f && traits.physH > traits.physW * 0.15f
                                     && traits.headCount >= 4;
                if (isMatrix)
                {
                    const double halfHeightPx = qMax(8.0, double(traits.physH) * m_scale) * 0.5;
                    d = qMax(0.0, d - halfHeightPx);
                }
            }
        }
        if (d < bestD) { bestD = d; best = fid; }
    }
    return best;
}

void StructureStudioView::mouseDoubleClickEvent(QMouseEvent *e)
{
    const quint32 fid = hitTestFixture(e->pos());
    if (fid != Fixture::invalidId())
        emit fixtureActivated(fid);
}

void StructureStudioView::contextMenuEvent(QContextMenuEvent *e)
{
    emit canvasContextMenu(e->globalPos(), hitTestFixture(e->pos()));
}

void StructureStudioView::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasFormat(QStringLiteral("application/x-qlc-fid")))
        e->acceptProposedAction();
}

void StructureStudioView::dropEvent(QDropEvent *e)
{
    const QByteArray b = e->mimeData()->data(QStringLiteral("application/x-qlc-fid"));
    if (b.isEmpty()) return;
    QDataStream s(b);
    QList<quint32> fids;
    while (!s.atEnd()) { quint32 fid; s >> fid; if (fid) fids << fid; }
    if (fids.isEmpty()) return;
    e->acceptProposedAction();
    emit fixturesDropped(fids, qlcEventPosF(e));
}

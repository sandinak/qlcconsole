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
#include <QTimer>
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
    m_zoomed = false;   // re-frame: this is a deliberate view change
    refit();
    update();
}

void StructureStudioView::reload()
{
    m_zoomed = false;   // re-frame: this is a deliberate view change
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

void StructureStudioView::setAmbient(double level)
{
    const double v = qBound(0.0, level, 1.0);
    if (qFuzzyCompare(v, m_ambient))
        return;
    m_ambient = v;
    update();
}

void StructureStudioView::setLiveValues(bool on)
{
    if (m_liveValues == on)
        return;
    m_liveValues = on;

    /* Repaint on a TIMER rather than on Fixture::valuesChanged(). This view
       paints the whole rig in one go, so a per-signal repaint would redraw
       every structure and fixture once per changed fixture per DMX frame --
       hundreds of full repaints a second for a running chase. 25 Hz is past
       what anyone can see and costs one repaint per tick regardless of how
       busy the show is. */
    if (on)
    {
        if (m_liveTimer == nullptr)
        {
            m_liveTimer = new QTimer(this);
            connect(m_liveTimer, &QTimer::timeout, this, [this]() {
                if (isVisible())        // hidden window: nothing to draw
                    update();
            });
        }
        m_liveTimer->start(40);
    }
    else if (m_liveTimer != nullptr)
    {
        m_liveTimer->stop();
    }
    update();
}

void StructureStudioView::setRotation(int quarterTurns)
{
    const int r = ((quarterTurns % 4) + 4) % 4;
    if (m_rotation == r)
        return;
    m_rotation = r;
    m_zoomed = false;   // re-frame: this is a deliberate view change
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

            /* An INSIDE fixture also has a height within the box, and an
               elevation is where you would set it. Without this the fixture
               dropped to the floor of the step and stayed there: Inside gave it
               a mountZOffset but nothing could edit one, so the only vertical
               position available was zero. On the deck there is nothing to set
               -- its height IS the deck. */
            if (rp.placement == FixtureRigProps::Inside && m_plane != Top)
            {
                const float base = props->platformBaseZ(pl->id());
                rp.mountZOffset = qBound(0.0f, float(ab.y()) - base, pl->height());
            }
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

    /* Standing ON (or inside) a platform deck.
     *
     * There was no branch for this at all: a deck-mounted fixture fell through
     * to the free-placement fallback, which writes the stored X/Y -- while
     * fixtureRigPosition()'s deck branch DERIVES its Z from the platform. So the
     * horizontal worked and the vertical silently did nothing, which is what
     * "set it to inside and still can't move it around the inside" was. */
    if (rp.onDeck())
    {
        StagePlatform *pl = props->platform(rp.deckPlatformId);
        if (pl == nullptr) return false;
        const QPointF ab = screenToPlane(px);
        const QVector3D cur = props->fixtureRigPosition(fid);

        double xMm = double(cur.x()) * 1000.0;
        double yMm = double(cur.y()) * 1000.0;
        if (m_plane == Top)        { xMm = ab.x() * 1000.0; yMm = ab.y() * 1000.0; }
        else if (m_plane == Front) { xMm = ab.x() * 1000.0; }
        else if (m_plane == Side)  { yMm = ab.x() * 1000.0; }
        else                       { return false; }        // Angled: no inverse
        props->setFixturePosition(fid, 0, 0,
                                  QVector3D(float(xMm), float(yMm), 0.0f));

        /* Height. Inside a step it is measured up from the platform's floor and
           clamped to its thickness; standing on the deck it is a lift above the
           deck top, which is what deckHeightOffset has always meant. */
        if (m_plane != Top)
        {
            const float base = props->platformBaseZ(pl->id());
            if (rp.placement == FixtureRigProps::Inside)
                rp.deckHeightOffset = qBound(0.0f, float(ab.y()) - base, pl->height());
            else
                rp.deckHeightOffset = qMax(0.0f, float(ab.y()) - (base + pl->height()));
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

        /* Normally the out-of-plane component is re-pinned to the assigned
           face, which is what keeps a surface-mounted fixture ON its face while
           you slide it about.
         *
         * But an INSIDE fixture is meant to live in the volume, and pinning is
         * precisely what stopped it: with studioMount 0 the pin is Z = the deck
         * top, so every vertical drag was overwritten with the surface height
         * and the fixture could never be positioned within the step. Clamp it
         * to the structure instead of pinning it to the skin. */
        int pinComp; double pinVal; facePin(rp.studioMount, pinComp, pinVal);
        StagePlatform *inPl = (m_kind == PlatformKind) ? props->platform(m_id) : nullptr;
        if (rp.placement == FixtureRigProps::Inside && inPl != nullptr)
        {
            if (pinComp == 0)      lp.setX(qBound(0.0f, lp.x(), inPl->width()));
            else if (pinComp == 1) lp.setY(qBound(0.0f, lp.y(), inPl->depth()));
            else                   lp.setZ(qBound(0.0f, lp.z(), inPl->height()));
        }
        else if (pinComp == 0) lp.setX(float(pinVal));
        else if (pinComp == 1) lp.setY(float(pinVal));
        else                   lp.setZ(float(pinVal));
        rp.groupLocal = lp;
        props->setFixtureRigProps(fid, rp);
        return true;
    }

    /* FREE-PLACED: no structural mount and no studio frame group.
     *
     * This used to `return false`, so a fixture that simply sits somewhere --
     * an LED bar laid on a step, say -- could not be moved in this editor at
     * all, even though the editor lists it and lets you select it. Move it the
     * way the 2D plot does: take the two in-plane components from the mouse and
     * keep the third.
     *
     * NOTE the mixed units. setFixturePosition() stores X and Y in MILLIMETRES
     * and Z in METRES (see the free-placed branch of fixtureRigPosition, which
     * divides x/y by 1000 and passes z straight through). Writing all three in
     * metres here would move a fixture a thousand times too little. */
    const QVector3D cur = props->fixtureRigPosition(fid);   // metres
    const QPointF ab = screenToPlane(px);
    QVector3D w = cur;
    if (m_plane == Top)        { w.setX(float(ab.x())); w.setY(float(ab.y())); }
    else if (m_plane == Front) { w.setX(float(ab.x())); w.setZ(float(ab.y())); }
    else if (m_plane == Side)  { w.setY(float(ab.x())); w.setZ(float(ab.y())); }
    else                       { return false; }            // Angled: no inverse

    props->setFixturePosition(fid, 0, 0,
                              QVector3D(w.x() * 1000.0f, w.y() * 1000.0f, w.z()));
    return true;
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
    if (m_plane == Angled)
        return;                       // drawn depth-sorted with the fixtures
    foreach (const KindId &ki, everyStructure())
        drawOneStructure(p, ki.first, ki.second);
}

/* The whole rig, in one back-to-front pass.
 *
 * Each object already sorts its OWN faces, but the objects themselves used to
 * paint in list order -- every truss, then every platform, then every tower,
 * then all the fixtures. So a step drew over a tower that was nearer the eye,
 * and a fixture behind a tower drew over it, purely because of where they sat
 * in the list. Sorting everything by depth first is what makes the overview
 * read as a solid scene rather than a pile of stickers. */
void StructureStudioView::drawRigDepthSorted(QPainter &p) const
{
    MonitorProperties *props = m_doc->monitorProperties();

    struct Item { double depth; bool isFixture; Kind kind; quint32 id; };
    QVector<Item> items;

    typedef QPair<Kind, quint32> KindId;
    foreach (const KindId &ki, everyStructure())
    {
        // Depth of the object's own extent, not of some arbitrary corner.
        QList<QVector3D> pts;
        collectPointsFor(pts, ki.first, ki.second);
        if (pts.isEmpty())
            continue;
        double d = 0.0;
        foreach (const QVector3D &w, pts)
            d += viewDepth(w);
        items << Item{ d / pts.size(), false, ki.first, ki.second };
    }
    foreach (quint32 fid, mountedFixtures())
        items << Item{ viewDepth(props->fixtureRigPosition(fid)), true, StageKind, fid };

    /* Deliberately NOT sorted here: ordering happens per PRIMITIVE in
       flushOps(), so a long truss and a wide deck interleave face by face
       instead of one winning outright on where its centre sits. */

    const bool nameEveryone = (m_kind != StageKind);
    p.setFont(QFont("Arial", 8));

    m_ops.clear();
    m_collecting = true;
    foreach (const Item &it, items)
    {
        if (it.isFixture)
            drawOneFixture(p, it.id, nameEveryone);
        else
            drawOneStructure(p, it.kind, it.id);
    }
    m_collecting = false;
    flushOps(p);
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
/* Each of these paints straight away in the flat views, and QUEUES when the
 * angled overview is collecting, so the drawing code below reads the same
 * either way. */
void StructureStudioView::emitPoly(const QPolygonF &poly, double depth, const QColor &fill,
                                   const QColor &pen, double penW, QPainter &p) const
{
    if (m_collecting)
    {
        DrawOp o; o.kind = DrawOp::Poly; o.depth = depth; o.poly = poly;
        o.fill = fill; o.pen = pen; o.penWidth = penW;
        m_ops << o;
        return;
    }
    p.setBrush(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
    p.setPen(pen.isValid() ? QPen(pen, penW) : QPen(Qt::NoPen));
    p.drawPolygon(poly);
}

void StructureStudioView::emitLine(const QPointF &a, const QPointF &b, double depth,
                                   const QColor &pen, double penW, QPainter &p) const
{
    if (m_collecting)
    {
        DrawOp o; o.kind = DrawOp::Line; o.depth = depth;
        o.poly << a << b; o.pen = pen; o.penWidth = penW;
        m_ops << o;
        return;
    }
    p.setPen(QPen(pen, penW));
    p.drawLine(a, b);
}

void StructureStudioView::emitDot(const QPointF &c, double depth, double r,
                                  const QColor &fill, QPainter &p) const
{
    if (m_collecting)
    {
        DrawOp o; o.kind = DrawOp::Dot; o.depth = depth;
        o.poly << c; o.radius = r; o.fill = fill;
        m_ops << o;
        return;
    }
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawEllipse(c, r, r);
}

void StructureStudioView::emitLabel(const QPointF &at, double depth, const QString &text,
                                    const QColor &pen, QPainter &p) const
{
    if (m_collecting)
    {
        DrawOp o; o.kind = DrawOp::Label; o.depth = depth;
        o.poly << at; o.text = text; o.pen = pen;
        m_ops << o;
        return;
    }
    p.setPen(pen);
    p.drawText(at, text);
}

void StructureStudioView::flushOps(QPainter &p) const
{
    // Ascending == farthest first (see viewDepth's convention note).
    std::stable_sort(m_ops.begin(), m_ops.end(),
                     [](const DrawOp &a, const DrawOp &b) { return a.depth < b.depth; });
    foreach (const DrawOp &o, m_ops)
    {
        switch (o.kind)
        {
        case DrawOp::Poly:
            p.setBrush(o.fill.isValid() ? QBrush(o.fill) : QBrush(Qt::NoBrush));
            p.setPen(o.pen.isValid() ? QPen(o.pen, o.penWidth) : QPen(Qt::NoPen));
            p.drawPolygon(o.poly);
            break;
        case DrawOp::Line:
            p.setPen(QPen(o.pen, o.penWidth));
            if (o.poly.size() >= 2) p.drawLine(o.poly.at(0), o.poly.at(1));
            break;
        case DrawOp::Dot:
            p.setPen(Qt::NoPen);
            p.setBrush(o.fill);
            if (!o.poly.isEmpty()) p.drawEllipse(o.poly.at(0), o.radius, o.radius);
            break;
        case DrawOp::Label:
            p.setPen(o.pen);
            if (!o.poly.isEmpty()) p.drawText(o.poly.at(0), o.text);
            break;
        }
    }
    m_ops.clear();
}

double StructureStudioView::viewDepth(const QVector3D &w) const
{
    /* CONVENTION: a LARGER value is NEARER the eye. Painter's algorithm
       therefore draws in ASCENDING order -- smallest (farthest) first. Both
       sorts here originally ran descending while their comments said "back to
       front", so far objects painted over near ones: a step covered the tower
       standing in front of it. */
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
                                       const QColor &base, const QColor &edge,
                                       int topAlpha) const
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

    QVector<QPair<double, int> > order;
    for (int f = 0; f < 6; ++f)
    {
        double d = 0.0;
        for (int k = 0; k < 4; ++k)
            d += viewDepth(corner[faces[f][k]]);
        order << qMakePair(d / 4.0, f);
    }
    /* Every face carries its OWN depth into the global queue. Sorting faces per
       box and boxes by centroid is what made a truss spanning the rig lose to a
       deck whose centre happened to be nearer, and small steps disappear behind
       big ones: a centroid does not describe where a long object overlaps. */
    foreach (const auto &o, order)
    {
        const int f = o.second;
        QPolygonF poly;
        for (int k = 0; k < 4; ++k)
            poly << w2s(corner[faces[f][k]]);
        QColor c = base;
        c = (shade[f] >= 100) ? c.lighter(shade[f]) : c.darker(200 - shade[f]);
        // Face 0 is the top; everything else is opaque.
        c.setAlpha((f == 0) ? qBound(0, topAlpha, 255) : 255);
        if (f == 0 && topAlpha <= 0)
            continue;                          // an open frame has no top at all
        /* Scenery is lit by the ROOM, so this applies whether or not live
           output is being shown: at full work light the deck colours are as
           saturated as the workspace says, and anything less takes them down.
           Gating it on live values left a blackout showing bright red decks,
           and left the static view as vivid as a test card. */
        {
            const double af = 0.18 + 0.82 * m_ambient;
            // Keep the alpha: building a QColor from three ints resets it to
            // opaque, which silently threw away a clear top's transparency.
            c = QColor(qRound(c.red() * af), qRound(c.green() * af),
                       qRound(c.blue() * af), c.alpha());
        }
        emitPoly(poly, o.first, c, edge, 1.1, p);
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

/* An open lattice prism: four chords running end to end, the two end frames,
 * and diagonal bracing across each of the four sides.
 *
 * Trusses and towers are mostly AIR. Drawing them as filled boxes was wrong
 * twice over: it does not look like a truss, and it hides anything rigged
 * inside or standing behind one. Nothing here is filled, so you see through the
 * structure the way you do in the room -- and with no large opaque faces, the
 * depth-sorting artefacts that only affect big filled polygons stop mattering
 * for these at all.
 *
 * @p a and @p b are the two end frames, four corners each, in matching order:
 * a[i] connects to b[i]. */
void StructureStudioView::drawLattice(QPainter &p, const QVector3D a[4], const QVector3D b[4],
                                      const QColor &col, int bays) const
{
    const QColor chord = col.lighter(140);
    const QColor brace = col.lighter(112);

    // The four chords.
    for (int i = 0; i < 4; ++i)
        emitLine(w2s(a[i]), w2s(b[i]), (viewDepth(a[i]) + viewDepth(b[i])) / 2.0,
                 chord, 1.8, p);

    // The end frames.
    for (int i = 0; i < 4; ++i)
    {
        const int j = (i + 1) % 4;
        emitLine(w2s(a[i]), w2s(a[j]), (viewDepth(a[i]) + viewDepth(a[j])) / 2.0,
                 chord, 1.4, p);
        emitLine(w2s(b[i]), w2s(b[j]), (viewDepth(b[i]) + viewDepth(b[j])) / 2.0,
                 chord, 1.4, p);
    }

    // Bracing: a zig-zag along each side, alternating so neighbouring sides do
    // not all lean the same way -- which is what a real truss looks like.
    bays = qMax(1, bays);
    for (int side = 0; side < 4; ++side)
    {
        const int i = side, j = (side + 1) % 4;
        for (int k = 0; k < bays; ++k)
        {
            const float s0 = float(k) / bays, s1 = float(k + 1) / bays;
            const QVector3D lo0 = a[i] + (b[i] - a[i]) * s0;
            const QVector3D lo1 = a[i] + (b[i] - a[i]) * s1;
            const QVector3D hi0 = a[j] + (b[j] - a[j]) * s0;
            const QVector3D hi1 = a[j] + (b[j] - a[j]) * s1;
            const bool up = ((k + side) % 2) == 0;
            const QVector3D &p0 = up ? lo0 : hi0;
            const QVector3D &p1 = up ? hi1 : lo1;
            emitLine(w2s(p0), w2s(p1), (viewDepth(p0) + viewDepth(p1)) / 2.0,
                     brace, 1.0, p);
        }
    }
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
            /* A box-truss tower, drawn as one: the bottom face is the A end
               and the top face the B end, indices already matching. */
            QVector3D c[8];
            boxCorners(c, x0, y0, 0.0f, x1, y1, h);
            const QVector3D endA[4] = { c[0], c[1], c[2], c[3] };
            const QVector3D endB[4] = { c[4], c[5], c[6], c[7] };
            drawLattice(p, endA, endB, steel, qMax(1, int(h / qMax(0.3f, t->width()))));
            // Shelves read as lines across the front face.
            for (int i = 0; i < t->shelfCount(); ++i)
            {
                const float z = t->shelfHeight(i);
                const QVector3D sa(x0, y1, z), sb(x1, y1, z);
                emitLine(w2s(sa), w2s(sb), (viewDepth(sa) + viewDepth(sb)) / 2.0,
                         steel.lighter(140), 1.6, p);
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
                /* Open lattice: chords and bracing, nothing filled. The two
                   end frames are (0,3,7,4) at A and (1,2,6,5) at B, in matching
                   order so chord i runs a[i] -> b[i]. */
                const QVector3D endA[4] = { corner[0], corner[3], corner[7], corner[4] };
                const QVector3D endB[4] = { corner[1], corner[2], corner[6], corner[5] };
                const int bays = qMax(1, int(t->length() / qMax(0.35f, t->width() * 2.0f)));
                drawLattice(p, endA, endB, steel, bays);
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
            /* A deck is a solid box, not a pane -- project its real corners.
               Unless its TOP is clear or open: then you have to be able to see
               (and light) through it, because that is the entire reason for
               rigging fixtures inside a step. */
            const float b0 = props->platformBaseZ(id);
            QVector3D c[8];
            boxCorners(c, x0, y0, b0, x1, y1, b0 + h);
            const int topAlpha = (pl->topMaterial() == StagePlatform::SolidTop) ? 255
                               : (pl->topMaterial() == StagePlatform::ClearTop) ? 70 : 0;
            drawSolidBox(p, c, pc, edge, topAlpha);
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

    /* A surface mount sits ON its surface, not straddling it.
     *
     * fixtureRigPosition() returns the point on the riser face or deck top that
     * the fixture is fastened to, and building the box symmetrically about that
     * point buried half of it inside the scenery and left the other half
     * floating proud -- which is what made face-mounted LED bars read as loose
     * boxes stuck on the front of a step rather than strips lying on it. Push
     * the body out along the surface normal by half its own thickness so its
     * BACK is flush with the surface. */
    /* Seat it on its surface -- along its own mount normal, whatever holds it
       up. studioMount says which plane the fixture lies in (flat, front face or
       side face), so this covers riser and deck mounts AND the free-placed
       strips that just sit on a step edge.
     *
     * NOT for a fixture rigged INSIDE something, though: a unit between a
     * truss's chords, or inside a clear-topped step firing up through it, is
     * meant to be within the volume. Pushing that onto the surface is exactly
     * the wrong answer, which is why placement is a property of its own. */
    const double seat = (rp.placement == FixtureRigProps::Inside) ? 0.0 : 0.5;
    const QVector3D c2 = c + N * float(d * seat);

    out[0] = c2 - l - u - n; out[1] = c2 + l - u - n;
    out[2] = c2 + l - u + n; out[3] = c2 - l - u + n;
    out[4] = c2 - l + u - n; out[5] = c2 + l + u - n;
    out[6] = c2 + l + u + n; out[7] = c2 - l + u + n;
}

/* A moving head as SOLID geometry: base, two yoke arms, and the head slung
 * between them, each an oriented box in the fixture's own frame.
 *
 * The flat views draw a yoke silhouette in screen space, which is why a mover
 * appeared to swivel toward the viewer at an angle. Boxes in the fixture's frame
 * keep still as the camera orbits AND keep the shape recognisable, instead of
 * the plain slab that replaced it. @p aim is the head's pointing direction in
 * world space; a null vector leaves the head in its rest position, so this is
 * ready to be driven by live pan/tilt without changing shape. */
void StructureStudioView::drawMoverSolid(QPainter &p, quint32 fid,
                                         const FixtureVisualTraits &traits,
                                         const QColor &col, const QVector3D &aim) const
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
    if (H.length() < 1e-6f) H = QVector3D(0, 0, 1);
    H.normalize();

    const int units = qMax(1, traits.headCount);
    const double wTot = (traits.physW > 0.0f) ? double(traits.physW) : 0.3;
    const double h    = (traits.physH > 0.0f) ? double(traits.physH) : wTot * 0.6;
    const double d    = (traits.physD > 0.0f) ? double(traits.physD) : h;
    const double slice = wTot / units;   // one head's share of the width

    // A box from a centre and three half-extents along the local axes.
    auto box = [&](const QVector3D &mid, double hw, double hh, double hd,
                   const QVector3D &ax, const QVector3D &up, const QVector3D &no) {
        const QVector3D l = ax * float(hw), u = up * float(hh), n = no * float(hd);
        QVector3D k[8];
        k[0] = mid - l - u - n; k[1] = mid + l - u - n;
        k[2] = mid + l - u + n; k[3] = mid - l - u + n;
        k[4] = mid - l + u - n; k[5] = mid + l + u - n;
        k[6] = mid + l + u + n; k[7] = mid - l + u + n;
        drawSolidBox(p, k, col, col.lighter(150));
    };

    for (int u = 0; u < units; ++u)
    {
        const double off = (u - (units - 1) * 0.5) * slice;
        const QVector3D uc = c + L * float(off);

        // Base: the block that bolts to the truss, at the "down" end of H.
        box(uc - H * float(h * 0.34), slice * 0.46, h * 0.16, d * 0.5, L, H, N);

        // Two arms rising from it, at the outer edges of this head's slice.
        const double armH = h * 0.30;
        const QVector3D armMid = uc - H * float(h * 0.02);
        box(armMid + L * float(slice * 0.36), slice * 0.10, armH, d * 0.22, L, H, N);
        box(armMid - L * float(slice * 0.36), slice * 0.10, armH, d * 0.22, L, H, N);

        // The head, slung between the arms. When an aim is given, offset it
        // along that direction so the fixture visibly points where it is
        // pointing rather than just changing colour.
        QVector3D headMid = uc + H * float(h * 0.10);
        if (!aim.isNull())
            headMid += aim.normalized() * float(h * 0.10);
        box(headMid, slice * 0.26, h * 0.22, d * 0.30, L, H, N);
    }
}

/* One fixture. Split out of drawFixtures() so the whole-rig overview can
 * interleave fixtures and structures in a single depth-sorted pass -- drawing
 * all structures and then all fixtures put a step in front of a tower that was
 * actually nearer the eye. */
void StructureStudioView::drawOneFixture(QPainter &p, quint32 fid,
                                        bool nameEveryone) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    Fixture *fx = m_doc->fixture(fid);
    const QPointF c = w2s(props->fixtureRigPosition(fid));
    const bool hi = m_highlight.contains(fid);
    const bool drag = (fid == m_dragFid);

    QColor col = props->fixtureGelColor(fid, 0, 0);

    if (!col.isValid() || col == QColor(Qt::black))

        col = QColor(90, 160, 235);


        /* Live output, when the rig is actually running. The gel colour above
           is what a fixture looks like UNLIT; showing that while a show plays
           makes the overview a diagram rather than a picture of the rig. */
        if (m_liveValues)
        {
            QColor live = col;
            uchar dim = 0;
            if (fixtureLiveState(fx, live, dim))
            {
                /* A lamp is as bright as it is being driven, FULL STOP -- the
                   room does not dim a light that is on. Room level only decides
                   how visible an UNLIT fixture is: it is an object sitting in
                   the space, faintly seen at blackout and plainly at work
                   light. Adding the two instead of taking the greater made a
                   fixture at full look different depending on the ambient
                   setting, which is backwards. */
                const double emit_ = dim / 255.0;
                const double roomLit = 0.12 + 0.28 * m_ambient;
                const double f = qBound(0.0, qMax(emit_, roomLit), 1.0);
                col = QColor(qRound(live.red() * f), qRound(live.green() * f),
                             qRound(live.blue() * f));
            }
        }
        else
        {
            // Not showing output: the fixture is just an object in the room.
            const double af = 0.30 + 0.70 * m_ambient;
            col = QColor(qRound(col.red() * af), qRound(col.green() * af),
                         qRound(col.blue() * af));
        }
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
        if (traits.kind == FixtureSilhouette::Mover)
            drawMoverSolid(p, fid, traits, col, QVector3D());
        else
            drawSolidBox(p, corner, col, col.lighter(150));

        // Pixels on the face that is pointing at us, when a grid is declared.
        if (traits.layout.isValid())
        {
            const int cols = traits.layout.width(), rows = traits.layout.height();
            /* On whichever long face points AT us. This was hardwired to the
               -n face, so for a strip lying flat the pixels were painted on its
               UNDERSIDE and showed as a field of dots spilling out from beneath
               the body. viewDepth is linear, so comparing corner sums is the
               same as comparing face centres. */
            const bool nearSide =
                viewDepth(corner[2] + corner[6]) > viewDepth(corner[0] + corner[4]);
            const QVector3D &f0 = nearSide ? corner[7] : corner[4];
            const QVector3D &f1 = nearSide ? corner[6] : corner[5];
            const QVector3D &b0 = nearSide ? corner[3] : corner[0];
            const QVector3D &b1 = nearSide ? corner[2] : corner[1];
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
                    const QVector3D at = lo + (hi - lo) * fr;
                    emitDot(w2s(at), viewDepth(at), 1.2, col.lighter(135), p);
                }
            }
        }
        if (hi && fx != nullptr)
            emitLabel(w2s(corner[6]) + QPointF(6, -4), viewDepth(corner[6]),
                      fx->name(), QColor(210, 214, 220), p);
        return;
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
        return;
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
        /* Draw the DEVICE, not a line.
         *
         * This used to decide between "a matrix" and "a bar" and draw the bar
         * as a bare line with dots along it, which told you where a fixture was
         * but nothing about what it is: a 1005 x 65 mm LED bar and a length of
         * rope looked identical. It also meant the flat views and the angled
         * view disagreed about the same fixture. Now every bar gets its real
         * body -- its declared footprint, projected -- with its heads laid out
         * inside, matching what the angled view draws. */
        QPointF wPx, hPx; QRectF boxPx;
        fixtureBoxPx(fid, traits, wPx, hPx, boxPx);

        if (hi)
        {
            p.setPen(QPen(QColor(120, 220, 140, 160), 3));
            p.setBrush(Qt::NoBrush);
            p.drawRect(boxPx.adjusted(-3, -3, 3, 3));
        }

        // The body.
        p.setPen(QPen(col.darker(150), (drag || hi) ? 1.8 : 1.2));
        p.setBrush(col.darker(230));
        p.drawRect(boxPx);

        /* The heads. A declared grid wins; otherwise a single row of however
           many heads there are, which is what a strip actually is. */
        const int cols = traits.layout.isValid() ? traits.layout.width()
                                                 : qMax(1, traits.headCount);
        const int rows = traits.layout.isValid() ? traits.layout.height() : 1;
        const double cellW = boxPx.width() / qMax(1, cols);
        const double cellH = boxPx.height() / qMax(1, rows);
        const double rad = qBound(0.7, qMin(cellW, cellH) * 0.40, 3.0);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        int placed = 0;
        for (int r = 0; r < rows && placed < traits.headCount; ++r)
        {
            const double fy = (rows > 1) ? (double(r) / (rows - 1) - 0.5) : 0.0;
            for (int cx = 0; cx < cols && placed < traits.headCount; ++cx, ++placed)
            {
                const double fx2 = (cols > 1) ? (double(cx) / (cols - 1) - 0.5) : 0.0;
                p.drawEllipse(c + wPx * fx2 + hPx * fy, rad, rad);
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
        drawOneFixture(p, fid, nameEveryone);
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
    if (m_kind == StageKind && m_plane == Angled)
    {
        drawRigDepthSorted(p);      // structures and fixtures interleaved
    }
    else
    {
        drawStructure(p);
        drawFixtures(p);
    }
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

void StructureStudioView::resizeEvent(QResizeEvent *)
{
    /* A resize must not throw away a zoom the operator asked for. Re-fitting on
       every resize is right while the view is still showing the whole rig, but
       once the wheel has been used, refitting silently snaps back to the
       overview -- which reads as the zoom not working at all. Deliberate view
       changes (plane, rotation, reload) clear m_zoomed and refit as before. */
    if (m_zoomed)
    {
        update();
        return;
    }
    refit();
}

void StructureStudioView::wheelEvent(QWheelEvent *e)
{
    /* This used to set m_scale and then call refit(), which RECOMPUTES m_scale
       from the widget size -- so the zoom was thrown away on the line after it
       was set and the wheel did nothing at all. Keep the new scale, and anchor
       the zoom on the CURSOR so whatever is under the pointer stays under it
       rather than drifting away while you chase it. */
    int delta = e->angleDelta().y();
    if (delta == 0)
        delta = e->angleDelta().x();
    if (delta == 0)
    {
        e->ignore();
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QPointF anchor = e->position();
#else
    const QPointF anchor = e->posF();
#endif
    const QPointF before = screenToPlane(anchor);

    const double next = qBound(4.0, m_scale * ((delta > 0) ? 1.12 : 1.0 / 1.12), 4000.0);
    if (qFuzzyCompare(next, m_scale))
    {
        e->accept();
        return;
    }
    m_scale = next;
    m_originPx += anchor - (m_originPx + planeToScreenVec(before));
    m_zoomed = true;          // refit() must not stomp a deliberate zoom
    update();
    e->accept();
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

        if (m_dragFid == Fixture::invalidId())
        {
            /* Empty canvas. What a plain drag SHOULD do depends on the view:
               - Angled: swing the camera. Fixture dragging is refused there
                 anyway (no honest inverse), so the gesture is free, and
                 orbiting by hand beats reaching for a spin box when you just
                 want to see behind something.
               - Flat views: PAN. Once you have zoomed in, grabbing the canvas
                 and pulling is the obvious way to get around, and it was doing
                 nothing at all -- panning was hidden behind shift-drag and the
                 middle button, which you have to be told about.
               Pressing ON a fixture still selects it either way. */
            if (m_plane == Angled)
            {
                m_orbiting = true;
                m_orbitLast = e->pos();
                setCursor(Qt::SizeAllCursor);
            }
            else
            {
                m_panning = true;
                m_panLast = e->pos();
                setCursor(Qt::ClosedHandCursor);
            }
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
        // Same reason as the wheel: having deliberately moved the view, a later
        // refit must not silently recentre it.
        m_zoomed = true;
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
    if (m_panning)
    {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        return;
    }
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

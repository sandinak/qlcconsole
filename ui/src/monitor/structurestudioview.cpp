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
#include <QNativeGestureEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QContextMenuEvent>
#include <QMimeData>
#include <QDataStream>
#include <QSet>
#include <QtMath>
#include <QTimer>
#include <QElapsedTimer>
#include <QSettings>
#include <QThread>
#include <QSemaphore>
#include <QThreadPool>
#include <QDebug>
#include <cmath>

#include "structurestudioview.h"
#include "humanfigure.h"
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
#include "scene.h"
#include "scenevalue.h"
#include "qlcpalette.h"
#include "fixturegroup.h"
#include "function.h"
#include "zraster.h"
#include <QVarLengthArray>

/* The flat grey a fixture HOUSING is drawn in, before room light. Matches the
 * studio's body fill (QColor(33,33,33)) closely enough to read as the same
 * object in both windows, a shade up so it still separates from the deck. */
static const int BODY_GREY = 52;

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

    {
        QSettings s;
        m_supersample = qBound(1, s.value(QStringLiteral("monitor/rig/supersample"), 2).toInt(), 4);
        m_renderThreads = qMax(0, s.value(QStringLiteral("monitor/rig/threads"), 0).toInt());
        m_maxFps = qBound(10, s.value(QStringLiteral("monitor/rig/maxfps"), 30).toInt(), 120);
    }

    /* Re-resolve the selected look when it changes underneath us.
     *
       What a scene SENDS is cached, because resolving palettes per fixture per
       frame is far too expensive -- but a cache of something the user is
       actively editing goes stale the moment they edit it. Changing a look's
       colour from white to blue left the rig view showing white, for exactly
       the same reason dragging a target left the beams pointing at where it
       used to be.
     *
       COALESCED, and for the same reason the studio coalesces its aim lines: a
       single joystick tick emits one functionChanged per fixture per channel,
       and a full re-resolve for each would be dozens of rebuilds for one
       visible change. */
    connect(m_doc, &Doc::functionChanged, this, [this](quint32 fid) {
        if (fid != m_activeSceneId || m_lookRefreshPending)
            return;
        m_lookRefreshPending = true;
        QTimer::singleShot(0, this, [this]() {
            m_lookRefreshPending = false;
            rebuildAimTargets();
            update();
        });
    });
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

    /* The aim association depends on the rig's geometry and on which
       fixtures exist, so it is rebuilt with everything else. */
    rebuildAimTargets();
}

void StructureStudioView::setHighlight(const QList<quint32> &ids)
{
    m_highlight = QSet<quint32>(ids.begin(), ids.end());
    update();
}

void StructureStudioView::setBarHighlight(quint32 barId)
{
    m_selectedBarId = barId;
    update();
}

void StructureStudioView::setPipeBarHighlight(quint32 barId)
{
    m_selectedPipeBarId = barId;
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
        m_liveTimer->start(1000 / m_maxFps);
    }
    // (the interval re-tunes itself from the measured frame cost -- paintEvent)
    else if (m_liveTimer != nullptr)
    {
        m_liveTimer->stop();
    }
    update();
}

void StructureStudioView::setSupersample(int factor)
{
    const int f = qBound(1, factor, 4);
    if (f == m_supersample)
        return;
    m_supersample = f;
    QSettings().setValue(QStringLiteral("monitor/rig/supersample"), f);
    update();
}

void StructureStudioView::setRenderThreads(int threads)
{
    m_renderThreads = qMax(0, threads);
    QSettings().setValue(QStringLiteral("monitor/rig/threads"), m_renderThreads);
    update();
}

void StructureStudioView::setMaxFrameRate(int fps)
{
    m_maxFps = qBound(10, fps, 120);
    QSettings().setValue(QStringLiteral("monitor/rig/maxfps"), m_maxFps);
    if (m_liveTimer != nullptr)
        m_liveTimer->setInterval(1000 / m_maxFps);
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

    /* A saved lock -- the truss/pipe/tower/platform this fixture rides, its
       layer, its group, or the whole layout -- has to win here and not just
       at the mouse handler, or any other caller (and any other test) gets to
       ignore it too. See fixtureMovementLocked(). */
    if (fixtureMovementLocked(fid))
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
            /* Snap to the nearest place on the tower by height (ab.y == Z): a
               shelf, the top cap or the base. Shelves only, it used to be, so
               on a tower whose shelves stop short of its top nothing could be
               dragged up onto the cap -- the one spot a followspot goes. A
               shelf at the very top still wins the tie, being considered
               first. */
            int best = rp.towerShelf;
            int side = rp.towerMountSide;
            double bestd = 1e9;
            auto consider = [&](double z, int s, int shelf) {
                const double dd = qAbs(z - ab.y());
                if (dd < bestd) { bestd = dd; side = s; best = shelf; }
            };
            for (int i = 0; i < t->shelfCount(); ++i)
                consider(double(t->shelfHeight(i)), FixtureRigProps::TowerShelf, i);
            consider(double(t->height()), FixtureRigProps::TowerTop, rp.towerShelf);
            consider(0.0, FixtureRigProps::TowerBottom, rp.towerShelf);
            // On the cap or the base it stands, as the plot's Mount on tower does.
            if (side != FixtureRigProps::TowerShelf && side != rp.towerMountSide)
                rp.mountingType = Truss::FloorMounted;
            rp.towerMountSide = side;
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
        const QVector3D cur = fixtureCentre(fid);

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

        const QVector3D cur = fixtureCentre(fid);
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
        const QVector3D cur = fixtureCentre(fid);
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
    const QVector3D cur = fixtureCentre(fid);   // metres
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

bool StructureStudioView::dragBarTo(quint32 barId, const QPointF &px)
{
    if (m_plane == Angled)
        return false;                    // same "no honest inverse" as fixtures

    MonitorProperties *props = m_doc->monitorProperties();
    Truss *bar = props->truss(barId);
    if (bar == nullptr || !bar->isChildBar())
        return false;
    Truss *parent = props->truss(bar->parentTrussId());
    if (parent == nullptr)
        return false;
    /* The bar's OWN lock only -- NOT its parent's. A bar is a structural
       object in its own right, not a simple attachment like a fixture: the
       main 2D plot already treats a bar's own TrussItem::locked() as the
       whole story for whether IT drags (monitorgraphicsview.cpp's
       refreshItemLayerState(), the Trusses block), never consulting its
       parent. Locking T-1 protects T-1's own geometry; it says nothing about
       whether a bar hung on it can still be repositioned, and inheriting the
       parent's lock here made every bar on this rig's four (deliberately
       locked) main trusses permanently unmovable regardless of the bar's
       own, unlocked state. */
    if (bar->locked())
        return false;

    const QVector3D cur = bar->origin();
    const QPointF ab = screenToPlane(px);
    QVector3D w = cur;
    if (m_plane == Top)        { w.setX(float(ab.x())); w.setY(float(ab.y())); }
    else if (m_plane == Front) { w.setX(float(ab.x())); w.setZ(float(ab.y())); }
    else                       { w.setY(float(ab.x())); w.setZ(float(ab.y())); }
    const QVector3D dw = w - cur;

    // Along the PARENT's own run -- same axis convention as a fixture's own
    // trussOffset drag (structurestudioview.cpp, dragFixtureTo()'s truss
    // branch): vertical run -> Z, horizontal run -> its own direction().
    QVector3D axis;
    if (parent->type() == Truss::Vertical)
    {
        axis = QVector3D(0.0f, 0.0f, 1.0f);
    }
    else
    {
        const QPointF d = parent->direction();
        const double dl = std::hypot(d.x(), d.y());
        if (dl < 1e-9)
            return false;
        axis = QVector3D(float(d.x() / dl), float(d.y() / dl), 0.0f);
    }
    bar->setParentOffset(qBound(0.0f, bar->parentOffset() + QVector3D::dotProduct(dw, axis),
                                parent->length()));

    /* Across the run -- "more on one side than the other" (barCrossShift,
       "shift along the run (0 = centred)", see
       MonitorProperties::recomputeChildTrusses(): newOrigin = base - runVec *
       (length/2 - barCrossShift), so +runVec is +barCrossShift). Only means
       anything for a bar that actually lies across a face (Along/Across);
       one that drops straight down a face (RunDrop) has no "side" to shift
       along -- recomputeChildTrusses() never reads barCrossShift for that
       case either, so applying it there would silently do nothing anyway.
       bar->direction() is already this bar's own current world run
       direction (recomputeChildTrusses() sets it), so there is no need to
       re-derive it from barFace/barRun by hand here. */
    if (bar->barRun() != Truss::RunDrop)
    {
        const QPointF d = bar->direction();
        const double dl = std::hypot(d.x(), d.y());
        if (dl > 1e-9)
        {
            const QVector3D cross(float(d.x() / dl), float(d.y() / dl), 0.0f);
            const float span = qMax(0.05f, bar->length());
            bar->setBarCrossShift(qBound(-span, bar->barCrossShift() + QVector3D::dotProduct(dw, cross),
                                         span));
        }
    }

    props->recomputeChildTrusses();
    return true;
}

bool StructureStudioView::dragPipeBarTo(quint32 barId, const QPointF &px)
{
    if (m_plane == Angled)
        return false;                    // same "no honest inverse" as fixtures/bars

    MonitorProperties *props = m_doc->monitorProperties();
    Pipe *bar = props->pipe(barId);
    if (bar == nullptr || !bar->isBarOnPipe())
        return false;
    Pipe *parent = props->pipe(bar->parentPipeId());
    if (parent == nullptr)
        return false;
    // The crossbar's OWN lock only, same reasoning as dragBarTo() for a
    // truss bar -- a crossbar is its own structural object, not simply
    // attached to its boom.
    if (bar->locked())
        return false;

    const QVector3D cur = bar->positionAt(0.0f);   // == its own origin
    const QPointF ab = screenToPlane(px);
    QVector3D w = cur;
    if (m_plane == Top)        { w.setX(float(ab.x())); w.setY(float(ab.y())); }
    else if (m_plane == Front) { w.setX(float(ab.x())); w.setZ(float(ab.y())); }
    else                       { w.setY(float(ab.x())); w.setZ(float(ab.y())); }
    const QVector3D dw = w - cur;

    // Along the PARENT boom's own run -- same axis convention as dragBarTo().
    // Unlike a truss bar, a pipe crossbar has no cross-shift: its origin is
    // derived from parentPipeOffset alone (recomputePipeAnchors()), so this
    // is the crossbar's only degree of freedom.
    QVector3D axis;
    if (parent->isVertical())
    {
        axis = QVector3D(0.0f, 0.0f, 1.0f);
    }
    else
    {
        const double a = qDegreesToRadians(double(parent->runAngle()));
        axis = QVector3D(float(qCos(a)), float(qSin(a)), 0.0f);
    }
    bar->setParentPipeOffset(qBound(0.0f, bar->parentPipeOffset() + QVector3D::dotProduct(dw, axis),
                                    parent->length()));

    props->recomputePipeAnchors();
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
    if (m_plane == Angled)
        return;                       // drawn depth-sorted with the fixtures
    if (m_kind != StageKind)
    {
        drawOneStructure(p, m_kind, m_id);
        return;
    }
    typedef QPair<Kind, quint32> KindId;
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
    struct Item { double depth; bool isFixture; Kind kind; quint32 id; };
    QVector<Item> items;

    /* The whole rig, or just the one thing an editor is editing. Without this
       scope, opening a truss editor drew the entire stage into it. */
    typedef QPair<Kind, quint32> KindId;
    QList<KindId> structures;
    if (m_kind == StageKind)
        structures = everyStructure();
    else
        structures << qMakePair(m_kind, m_id);

    foreach (const KindId &ki, structures)
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
        items << Item{ viewDepth(fixtureCentre(fid)), true, StageKind, fid };

    /* Deliberately NOT sorted here: ordering happens per PRIMITIVE in
       flushOps(), so a long truss and a wide deck interleave face by face
       instead of one winning outright on where its centre sits. */

    const bool nameEveryone = (m_kind != StageKind);
    p.setFont(QFont("Arial", 8));

    m_ops.clear();
    m_frameBeams.clear();
    m_collecting = true;
    foreach (const Item &it, items)
    {
        if (it.isFixture)
            drawOneFixture(p, it.id, nameEveryone);
        else
            drawOneStructure(p, it.kind, it.id);
    }
    if (m_kind == StageKind)
        drawStageFigures(p);
    m_collecting = false;
    flushOps(p);
}

void StructureStudioView::drawStageFigures(QPainter &p) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const QVector3D eye = toViewer();
    /* The room lights the body from above and in front of the stage, which is
       what gives it form; the beams add their colour on top. */
    const QVector3D roomDir = QVector3D(0.25f, 0.55f, 0.80f).normalized();
    const double roomLevel = 0.18 + 0.82 * m_ambient;
    static const int SEG = 12;

    foreach (StageTarget *t, props->stageTargets())
    {
        if (t == nullptr)
            continue;

        /* An Area or a drum kit: its outline on the deck, in its colour, at
           its real size -- where a beam lands relative to it is the point. */
        if ((t->kind() == StageTarget::Kind::Area || t->kind() == StageTarget::Kind::DrumKit)
            && t->footprint().isEmpty() == false)
        {
            const QVector3D at = props->targetAimPoint(t, true);
            const float ground = props->platformHeightAt(at.x(), at.y()) + 0.004f;
            const float hw = float(t->footprint().width()) * 0.5f;
            const float hd = float(t->footprint().height()) * 0.5f;
            const QColor mark = t->color().isValid() ? t->color() : QColor(255, 180, 0);
            const QVector3D c[4] = {
                QVector3D(at.x() - hw, at.y() - hd, ground), QVector3D(at.x() + hw, at.y() - hd, ground),
                QVector3D(at.x() + hw, at.y() + hd, ground), QVector3D(at.x() - hw, at.y() + hd, ground) };
            for (int k = 0; k < 4; ++k)
                emitLine(w2s(c[k]), w2s(c[(k + 1) % 4]), viewDepth(c[k]), viewDepth(c[(k + 1) % 4]),
                         mark, 2.0, p);
            continue;
        }

        if (t->kind() != StageTarget::Kind::Person)
            continue;
        const float tall = props->targetFigureHeight(t);
        if (tall <= 0.0f)
            continue;

        /* Stood where the aim rule stands it, so the beams meet the chest: the
           aim point less the aim height is the deck under the person. */
        const QVector3D aim = props->targetAimPoint(t, true);
        const QVector3D foot(aim.x(), aim.y(), aim.z() - props->targetAimHeight(t, true));
        const QColor mark = t->color().isValid() ? t->color() : QColor(255, 180, 0);

        /* The target's own mark on the deck, in the target's colour: which
           target this is. The body itself is neutral, so what colour it shows
           is the light on it. */
        {
            static const int MARK_SEG = 24;
            const double r = 0.34 * double(tall) / 1.75;
            QVector3D prev;
            for (int k = 0; k <= MARK_SEG; ++k)
            {
                const double a = 2.0 * M_PI * k / MARK_SEG;
                const QVector3D q = foot + QVector3D(float(r * qCos(a)), float(r * qSin(a)), 0.004f);
                if (k > 0)
                    emitLine(w2s(prev), w2s(q), viewDepth(prev), viewDepth(q), mark, 2.0, p);
                prev = q;
            }
        }

        /* How a beam falls on a point of the body: 1 inside its cone, rolling
           off over a soft edge, 0 outside or past where it lands. */
        auto coverage = [](const FrameBeam &fb, const QVector3D &at) {
            const QVector3D rel = at - fb.apex;
            const double along = double(QVector3D::dotProduct(rel, fb.dir));
            if (along <= 0.0 || along > fb.length + 0.25)
                return 0.0;
            const double off = double((rel - fb.dir * float(along)).length());
            const double radius = along * qTan(fb.halfAngle);
            const double soft = qMax(0.03, radius * 0.25);
            return (off <= radius) ? 1.0
                 : (off >= radius + soft) ? 0.0
                 : 1.0 - (off - radius) / soft;
        };

        /* Exposure, the way a camera exposes for the subject. Twelve white
           heads on one chest add up to twelve times the light of one, and
           added straight the body clipped to a flat white blob -- no form, and
           no colour once every channel was full.
         *
           Measured by how bright the arriving light LOOKS, not by how many
           beams there are: a crowd of deep blue beams is not bright and adds
           up to a vivid blue body, which is right and was never the problem.
           Past about a white beam and a third of brightness it scales back, so
           white stays bright but keeps its shading. */
        const QVector3D chest = foot + QVector3D(0.0f, 0.0f, 0.72f * tall);
        double arrivingLuma = 0.0;
        for (const FrameBeam &fb : m_frameBeams)
        {
            const double luma = 0.30 * fb.colour.redF() + 0.59 * fb.colour.greenF()
                              + 0.11 * fb.colour.blueF();
            arrivingLuma += fb.level * coverage(fb, chest) * luma;
        }
        const double exposure = 1.1 / qMax(1.1, arrivingLuma);

        /* Past a knee, approach full brightness instead of hitting it: what
           is lit hard stays bright without flattening into one value. */
        auto knee = [](double v) {
            const double k = 180.0;
            return (v <= k) ? v : k + 75.0 * (1.0 - qExp(-(v - k) / 75.0));
        };

        auto shade = [&](const QVector3D &at, const QVector3D &n) {
            const double diffuse =
                roomLevel * (0.50 + 0.50 * double(qMax(0.0f, QVector3D::dotProduct(n, roomDir))));
            double r = 150.0 * diffuse, g = 146.0 * diffuse, b = 140.0 * diffuse;
            /* Every beam that passes through this part of the body and falls
               on the side facing it, added: a red spot from the front and a
               blue one from the side leave a red front and a blue side, and a
               face both reach shows the mix. */
            for (const FrameBeam &fb : m_frameBeams)
            {
                const double cover = coverage(fb, at);
                if (cover <= 0.0)
                    continue;
                const double facing = double(qMax(0.0f, -QVector3D::dotProduct(n, fb.dir)));
                const double k = fb.level * cover * facing * 1.15 * exposure;
                r += fb.colour.red() * k;
                g += fb.colour.green() * k;
                b += fb.colour.blue() * k;
            }
            return QColor(qBound(0, qRound(knee(r)), 255), qBound(0, qRound(knee(g)), 255),
                          qBound(0, qRound(knee(b)), 255));
        };

        auto face = [&](const QVector<QVector3D> &pts, const QVector3D &normal) {
            QVector3D n = normal;
            if (n.lengthSquared() < 1e-12f)
                return;
            n.normalize();
            if (QVector3D::dotProduct(n, eye) <= 0.0f)
                return;                          // facing away: hidden anyway
            QPolygonF poly;
            QVector<double> vz;
            QVector3D centre;
            double depth = 0.0;
            for (const QVector3D &q : pts)
            {
                poly << w2s(q);
                const double d = viewDepth(q);
                vz << d;
                depth += d;
                centre += q;
            }
            centre /= float(pts.size());
            emitPoly(poly, depth / pts.size(), shade(centre, n), QColor(), 0.0, p, vz);
        };

        foreach (const FigureLoft &loft, humanFigureLofts())
        {
            QVector<QVector<QVector3D> > rings;
            for (const FigureRing &fr : loft)
            {
                QVector<QVector3D> ring;
                for (int s = 0; s < SEG; ++s)
                {
                    const double a = 2.0 * M_PI * s / SEG;
                    ring << foot + QVector3D((fr.cx + fr.rx * float(qCos(a))) * tall,
                                             (fr.cy + fr.ry * float(qSin(a))) * tall,
                                             fr.z * tall);
                }
                rings << ring;
            }
            for (int i = 0; i + 1 < rings.size(); ++i)
                for (int s = 0; s < SEG; ++s)
                {
                    const QVector3D &a = rings[i][s];
                    const QVector3D &b = rings[i][(s + 1) % SEG];
                    const QVector3D &c2 = rings[i + 1][(s + 1) % SEG];
                    const QVector3D &d = rings[i + 1][s];
                    // Counter-clockwise from above, so this points outward.
                    QVector3D n = QVector3D::crossProduct(b - a, d - a)
                                + QVector3D::crossProduct(c2 - d, c2 - b);
                    face({ a, b, c2, d }, n);
                }
            // Close the ends.
            if (rings.isEmpty() == false)
            {
                QVector<QVector3D> bottom = rings.first();
                std::reverse(bottom.begin(), bottom.end());
                face(bottom, QVector3D(0.0f, 0.0f, -1.0f));
                face(rings.last(), QVector3D(0.0f, 0.0f, 1.0f));
            }
        }
    }
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
        pts << fixtureCentre(fid);
    /* And the people standing on it, feet to crown: a figure is drawn, so a
       fit that ignores it cuts off the head of anyone on a deck. */
    foreach (StageTarget *t, props->stageTargets())
    {
        const float tall = props->targetFigureHeight(t);
        if (tall <= 0.0f)
            continue;
        const QVector3D aim = props->targetAimPoint(t, true);
        const float foot = aim.z() - props->targetAimHeight(t, true);
        pts << QVector3D(aim.x(), aim.y(), foot)
            << QVector3D(aim.x(), aim.y(), foot + tall);
    }
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
        pts << fixtureCentre(fid);
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
        /* The SAME grid the studio draws, not one invented from the rig's
           bounding box.
         *
           This used to size its cells as a twelfth of however wide the rig
           happened to be, so the floor was ruled in some arbitrary fraction of
           a stage and the two windows disagreed about how big a metre is. The
           studio's grid is the stage: same cell size, same extent, so a
           distance you learn to read in one window means the same in the
           other.
         *
           The spacing is uniform front to back -- this is an ORTHOGRAPHIC
           projection, measured at 51.5 px per cell at every depth. Cells look
           bigger toward the back because they are parallelograms, not because
           they are. */
        MonitorProperties *gprops = m_doc->monitorProperties();
        const double cell = (gprops->gridUnits() == MonitorProperties::Feet)
                            ? 0.3048 : 1.0;
        const QVector3D cells = gprops->gridSize();
        double x0 = 0.0, y0 = 0.0;
        double x1 = qMax(1.0, double(cells.x())) * cell;
        double y1 = qMax(1.0, double(cells.z())) * cell;

        /* ...but never smaller than what is actually rigged. A structure
           standing off the end of the declared stage still needs a floor. */
        QList<QVector3D> pts;
        collectPoints(pts);
        foreach (const QVector3D &w, pts)
        {
            x0 = qMin(x0, double(w.x())); x1 = qMax(x1, double(w.x()));
            y0 = qMin(y0, double(w.y())); y1 = qMax(y1, double(w.y()));
        }
        x0 = qFloor(x0 / cell) * cell;  y0 = qFloor(y0 / cell) * cell;
        x1 = qCeil(x1 / cell) * cell;   y1 = qCeil(y1 / cell) * cell;

        /* One line per cell, until that is more ink than floor -- past a few
           hundred lines the grid stops being a ruler and becomes a texture. */
        double step = cell;
        while ((x1 - x0) / step > 80.0 || (y1 - y0) / step > 80.0)
            step *= 2.0;
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
    // Same "selected/dragged reads distinctly" amber drawOneStructure() uses
    // for a truss bar -- only a crossbar (isBarOnPipe()) is ever
    // selected/dragged this way, a plain boom never is.
    const bool selected = pipe->isBarOnPipe()
                         && (pipe->id() == m_selectedPipeBarId || pipe->id() == m_dragPipeBarId);
    QColor c = selected ? QColor(255, 190, 60) : pipe->color();
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
                                   const QColor &pen, double penW, QPainter &p,
                                   const QVector<double> &vertexDepths,
                                   const QVector<double> &vertexAlphas) const
{
    if (m_collecting)
    {
        DrawOp o; o.kind = DrawOp::Poly; o.depth = depth; o.poly = poly;
        o.fill = fill; o.pen = pen; o.penWidth = penW;
        if (vertexDepths.size() == poly.size())
            o.zs = vertexDepths;
        if (vertexAlphas.size() == poly.size())
            o.alphas = vertexAlphas;
        m_ops << o;
        return;
    }
    p.setBrush(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
    p.setPen(pen.isValid() ? QPen(pen, penW) : QPen(Qt::NoPen));
    p.drawPolygon(poly);
}

void StructureStudioView::emitLight(const QPolygonF &poly, double depth, const QColor &colour,
                                    QPainter &p, const QVector<double> &vertexDepths,
                                    const QVector<double> &vertexAlphas) const
{
    if (m_collecting)
    {
        DrawOp o; o.kind = DrawOp::Poly; o.depth = depth; o.poly = poly;
        o.fill = colour; o.light = true;
        if (vertexDepths.size() == poly.size())
            o.zs = vertexDepths;
        if (vertexAlphas.size() == poly.size())
            o.alphas = vertexAlphas;
        m_ops << o;
        return;
    }
    // The painter path has no light buffer: layer it, as it always did.
    emitPoly(poly, depth, colour, QColor(), 0.0, p, vertexDepths, vertexAlphas);
}

void StructureStudioView::emitLine(const QPointF &a, const QPointF &b,
                                   double za, double zb,
                                   const QColor &pen, double penW, QPainter &p) const
{
    if (m_collecting)
    {
        DrawOp o; o.kind = DrawOp::Line; o.depth = (za + zb) / 2.0;
        o.zs << za << zb;
        o.poly << a << b; o.pen = pen; o.penWidth = penW;
        m_ops << o;
        return;
    }
    p.setPen(QPen(pen, penW));
    p.drawLine(a, b);
}

/* Many dots, one op: same depth, same colour, drawn in one state change. */
void StructureStudioView::emitDots(const QVector<QPointF> &pts, double depth, double r,
                                  const QColor &fill, QPainter &p,
                                  const QVector<double> &pointDepths) const
{
    if (pts.isEmpty())
        return;
    if (m_collecting)
    {
        DrawOp o; o.kind = DrawOp::Dot; o.depth = depth; o.radius = r;
        o.fill = fill;
        o.poly = QPolygonF(pts);
        if (pointDepths.size() == pts.size())
            o.zs = pointDepths;
        m_ops << o;
        return;
    }
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    foreach (const QPointF &c, pts)
        p.drawEllipse(c, r, r);
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

/* One queued primitive into the depth buffer. */
void StructureStudioView::rasterOp(const DrawOp &o, bool depthWrite,
                                   int rowFrom, int rowTo) const
{
    const int n = o.poly.size();
    if (n == 0 || m_z == nullptr)
        return;

    // Per-vertex depth if the emitter knew it, otherwise the primitive's own.
    QVarLengthArray<double, 32> zs(n);
    for (int i = 0; i < n; ++i)
        zs[i] = (o.zs.size() == n) ? o.zs.at(i) : o.depth;

    switch (o.kind)
    {
    case DrawOp::Poly:
        if (o.fill.isValid())
            m_z->poly(o.poly.constData(), zs.constData(), n, o.fill, depthWrite,
                      (o.alphas.size() == n) ? o.alphas.constData() : nullptr,
                      rowFrom, rowTo);
        if (o.pen.isValid() && n >= 2)
        {
            /* The outline, edge by edge, each carrying its own two depths. A
               deck's bright rim has to be occluded by whatever stands in front
               of it exactly as its fill is. */
            for (int i = 0; i < n; ++i)
            {
                const int k = (i + 1) % n;
                m_z->thickLine(o.poly.at(i), o.poly.at(k), zs[i], zs[k],
                               qMax(1.0, o.penWidth), o.pen, depthWrite, rowFrom, rowTo);
            }
        }
        break;

    case DrawOp::Line:
        if (n >= 2 && o.pen.isValid())
            m_z->thickLine(o.poly.at(0), o.poly.at(1), zs[0], zs[1],
                           qMax(1.0, o.penWidth), o.pen, depthWrite, rowFrom, rowTo);
        break;

    case DrawOp::Dot:
    {
        /* A pixel is a speck on a surface, so it is square and flat: at the
           couple of pixels these actually occupy, a disc and a square are the
           same speck and the square is far cheaper. */
        const double r = qMax(0.5, o.radius);
        for (int i = 0; i < n; ++i)
        {
            const QPointF c = o.poly.at(i);
            const QPointF q[4] = { QPointF(c.x() - r, c.y() - r),
                                   QPointF(c.x() + r, c.y() - r),
                                   QPointF(c.x() + r, c.y() + r),
                                   QPointF(c.x() - r, c.y() + r) };
            const double qz[4] = { zs[i], zs[i], zs[i], zs[i] };
            m_z->poly(q, qz, 4, o.fill, depthWrite, nullptr, rowFrom, rowTo);
        }
        break;
    }

    case DrawOp::Label:
    case DrawOp::Trace:
        break;          // drawn in screen space after the buffer is resolved
    }
}

void StructureStudioView::flushOps(QPainter &p) const
{
    m_lastOpCount = m_ops.size();

    if (m_useZBuffer == false)
    {
        /* The old renderer: sort whole primitives by one depth and paint back
           to front. Kept behind a switch through the changeover so the two can
           be compared on a real rig rather than argued about, and so there is
           somewhere to fall back to if the depth buffer turns out to have a
           case nobody thought of. */
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
                foreach (const QPointF &c, o.poly)
                    p.drawEllipse(c, o.radius, o.radius);
                break;
            case DrawOp::Label:
                p.setPen(o.pen);
                if (!o.poly.isEmpty()) p.drawText(o.poly.at(0), o.text);
                break;
            case DrawOp::Trace:
                if (o.poly.size() >= 2)
                {
                    p.setPen(QPen(o.pen, o.penWidth));
                    p.drawLine(o.poly.at(0), o.poly.at(1));
                }
                break;
            }
        }
        m_ops.clear();
        return;
    }

    if (m_z == nullptr)
        m_z = new ZRaster();

    /* Transparent, so the floor grid already painted underneath shows through
       wherever the rig does not cover it. */
    m_z->begin(size(), m_supersample, Qt::transparent);

    /* OPAQUE GEOMETRY NEEDS NO SORT AT ALL. That is the whole reason for the
       depth buffer: which surface wins is decided per pixel, so a polygon whose
       depth varies across it can be partly in front of and partly behind
       another. One depth per primitive could never express that, and every
       artefact in this view came from pretending it could.
     *
       Translucency is the exception a depth buffer does NOT solve: blending is
       order-dependent, so beams and clear tops go last, back to front among
       themselves, depth-TESTED against the solid world but not writing depth.
       That sort is over a handful of primitives instead of all of them. */
    QVector<DrawOp> opaque, translucent, labels, lights;
    opaque.reserve(m_ops.size());
    foreach (const DrawOp &o, m_ops)
    {
        if (o.kind == DrawOp::Label || o.kind == DrawOp::Trace)
        {
            labels << o;        // annotation: screen space, after the resolve
            continue;
        }
        if (o.light)
        {
            lights << o;        // added after the solids are in, in any order
            continue;
        }
        const bool seeThrough = (o.fill.isValid() && o.fill.alpha() < 255)
                                || (o.pen.isValid() && o.pen.alpha() < 255);
        if (seeThrough)
            translucent << o;
        else
            opaque << o;
    }
    std::stable_sort(translucent.begin(), translucent.end(),
                     [](const DrawOp &a, const DrawOp &b) { return a.depth < b.depth; });

    /* One band of the frame: solids, then light -- after every solid is in, so
       it is stopped by what stands in front of it, and before the translucent
       surfaces, which do not stop it -- then the translucent surfaces back to
       front. Rows are supersampled; light rows are the final image's. */
    const int ss = m_z->supersample();
    auto fillBand = [&](int rowFrom, int rowTo, QRect *lit) {
        for (const DrawOp &o : opaque)
            rasterOp(o, true, rowFrom, rowTo);
        const int lightFrom = rowFrom / ss;
        const int lightTo = (rowTo == INT_MAX) ? INT_MAX : rowTo / ss;
        for (const DrawOp &o : lights)
        {
            const int n = o.poly.size();
            if (n < 3 || !o.fill.isValid())
                continue;
            QVarLengthArray<double, 8> zs(n);
            for (int i = 0; i < n; ++i)
                zs[i] = (o.zs.size() == n) ? o.zs.at(i) : o.depth;
            m_z->light(o.poly.constData(), zs.constData(), n, o.fill,
                       (o.alphas.size() == n) ? o.alphas.constData() : nullptr,
                       lightFrom, lightTo, lit);
        }
        for (const DrawOp &o : translucent)
            rasterOp(o, false, rowFrom, rowTo);
    };

    /* ACROSS THE CORES. The fill is most of a frame and it used one core.
       Bands of rows never share a pixel, and each band runs the same ops in
       the same order as the whole frame would, so splitting it changes the
       speed and nothing else. Band edges fall on whole final-image rows, so a
       band's light rows and its supersampled rows cover the same stripe. */
    int threads = (m_renderThreads > 0) ? m_renderThreads : QThread::idealThreadCount();
    const int rows = m_z->bufferHeight();
    threads = qBound(1, threads, qMax(1, rows / (8 * ss)));
    if (threads <= 1)
    {
        QRect lit;
        fillBand(0, INT_MAX, &lit);
        m_z->addLit(lit);
    }
    else
    {
        if (m_renderPool == nullptr)
            m_renderPool = new QThreadPool(const_cast<StructureStudioView *>(this));
        if (m_renderPool->maxThreadCount() < threads)
            m_renderPool->setMaxThreadCount(threads);

        const int perBand = ((rows / threads + ss - 1) / ss) * ss;
        QVector<QRect> lit(threads);
        QSemaphore done;
        int started = 0;
        for (int b = 0; b < threads; ++b)
        {
            const int from = b * perBand;
            if (from >= rows)
                break;
            const int to = (b == threads - 1) ? rows : qMin(rows, from + perBand);
            ++started;
            m_renderPool->start([&, b, from, to]() {
                fillBand(from, to, &lit[b]);
                done.release();
            });
        }
        done.acquire(started);
        for (const QRect &r : lit)
            m_z->addLit(r);
    }
    p.drawImage(0, 0, m_z->resolve());
    {
        /* SCREEN, not plus: light over a dark background adds nearly in full,
           over something already bright it closes in on white without
           reaching it. Plus clipped the air behind a crowd of beams, and the
           glow in front of a lit body, straight to white. */
        QPoint at;
        const QImage light = m_z->lightImage(&at);
        if (!light.isNull())
        {
            p.save();
            p.setCompositionMode(QPainter::CompositionMode_Screen);
            p.drawImage(at, light);
            p.restore();
        }
    }

    // Text last, in screen space: depth-testing a label is not meaningful.
    foreach (const DrawOp &o, labels)
    {
        if (o.kind == DrawOp::Trace)
        {
            if (o.poly.size() >= 2)
            {
                p.setPen(QPen(o.pen, o.penWidth));
                p.drawLine(o.poly.at(0), o.poly.at(1));
            }
            continue;
        }
        p.setPen(o.pen);
        if (!o.poly.isEmpty())
            p.drawText(o.poly.at(0), o.text);
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
/* The six faces of a corner-8 box, shared by everything that has to reason
 * about one. */
static const int BOX_FACES[6][4] = {
    { 4, 5, 6, 7 },   // top
    { 0, 1, 2, 3 },   // bottom
    { 0, 1, 5, 4 },   // side
    { 2, 3, 7, 6 },   // side
    { 1, 2, 6, 5 },   // end
    { 3, 0, 4, 7 },   // end
};

/* The depth of the NEAREST face of a box -- what something inside it has to
 * beat to stay visible, given that each face sorts as a single value. */
double StructureStudioView::boxNearFaceDepth(const QVector3D corner[8]) const
{
    double best = -1e30;
    for (int f = 0; f < 6; ++f)
    {
        double d = 0.0;
        for (int k = 0; k < 4; ++k)
            d += viewDepth(corner[BOX_FACES[f][k]]);
        best = qMax(best, d / 4.0);
    }
    return best;
}

/* A fixture body's edge/highlight colour, readable against the dark canvas
 * regardless of how dark the fixture's own gel/body colour is.
 *
 * QColor::lighter() scales multiplicatively, which does nothing useful
 * starting from near-black -- and a black housing is exactly the fixture
 * body colour most likely to be set, since plenty of real fixtures ARE
 * black. col.lighter(150) on near-black stays near-black, and a mover
 * rendered in its own near-invisible outline was unreadable against the
 * canvas ("hard to see some fixtures on the dark background"). Adding a
 * fixed offset instead of a multiplier brightens a dark colour by a
 * meaningful, visible amount while still tracking the base colour's hue for
 * an already-bright fixture. */
static QColor fixtureEdgeColor(const QColor &base)
{
    return QColor(qMin(255, base.red() + 140),
                  qMin(255, base.green() + 140),
                  qMin(255, base.blue() + 140),
                  base.alpha());
}

QColor StructureStudioView::overlapAwareEdgeColor(quint32 fid, const QColor &base) const
{
    static const QColor kOverlapWarning(255, 70, 40);
    return m_overlappingFixtures.contains(fid) ? kOverlapWarning : fixtureEdgeColor(base);
}

bool StructureStudioView::fixtureMovementLocked(quint32 fid) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr)
        return false;
    if (props->layoutLocked())
        return true;
    if (props->layer(props->fixtureLayer(fid)).locked)
        return true;
    if (props->groupChainLocked(props->fixtureGroup(fid)))
        return true;

    // The thing it's actually bolted to -- a truss lock has to reach the
    // fixture riding it, the same way it already reaches the truss itself
    // on the main plot.
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.trussId != Truss::invalidId())
    {
        if (Truss *t = props->truss(rp.trussId))
            return t->locked();
    }
    if (rp.onPipe())
    {
        if (Pipe *p = props->pipe(rp.pipeId))
            return p->locked();
    }
    if (rp.onTower())
    {
        if (Tower *t = props->tower(rp.towerId))
            return t->locked();
    }
    if (rp.onRiser())
    {
        if (StagePlatform *pl = props->platform(rp.riserPlatformId))
            return pl->locked();
    }
    if (rp.onDeck())
    {
        if (StagePlatform *pl = props->platform(rp.deckPlatformId))
            return pl->locked();
    }
    return false;
}

void StructureStudioView::drawSolidBox(QPainter &p, const QVector3D corner[8],
                                       const QColor &base, const QColor &edge,
                                       int topAlpha, bool ambientLit,
                                       double depthBias) const
{
    const int (*faces)[4] = BOX_FACES;
    static const int shade[6] = { 118, 62, 92, 78, 100, 85 };   // % brightness

    QVector<QPair<double, int> > order;
    for (int f = 0; f < 6; ++f)
    {
        double d = 0.0;
        for (int k = 0; k < 4; ++k)
            d += viewDepth(corner[faces[f][k]]);
        order << qMakePair(d / 4.0 + depthBias, f);
    }
    /* Every face carries its OWN depth into the global queue. Sorting faces per
       box and boxes by centroid is what made a truss spanning the rig lose to a
       deck whose centre happened to be nearer, and small steps disappear behind
       big ones: a centroid does not describe where a long object overlaps. */
    foreach (const auto &o, order)
    {
        const int f = o.second;
        QPolygonF poly;
        QVector<double> vz;
        vz.reserve(4);
        for (int k = 0; k < 4; ++k)
        {
            poly << w2s(corner[faces[f][k]]);
            /* The face's REAL depth at each corner. A step face two metres wide
               spans a metre of depth off-axis; describing it with one number is
               what put half of every strip behind its own housing. */
            vz << viewDepth(corner[faces[f][k]]) + depthBias;
        }
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
           and left the static view as vivid as a test card.
         *
         * NOT for an emitting fixture, though: its colour already IS its output,
         * and dimming a lamp by the room is backwards -- the same mistake the
         * non-box path had, fixed there and missed here, which rendered a mover
         * at full white as a near-black box. */
        if (ambientLit)
        {
            const double af = 0.18 + 0.82 * m_ambient;
            // Keep the alpha: building a QColor from three ints resets it to
            // opaque, which silently threw away a clear top's transparency.
            c = QColor(qRound(c.red() * af), qRound(c.green() * af),
                       qRound(c.blue() * af), c.alpha());
        }
        emitPoly(poly, o.first, c, edge, 1.1, p, vz);
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
        emitLine(w2s(a[i]), w2s(b[i]), viewDepth(a[i]), viewDepth(b[i]),
                 chord, 1.8, p);

    // The end frames.
    for (int i = 0; i < 4; ++i)
    {
        const int j = (i + 1) % 4;
        emitLine(w2s(a[i]), w2s(a[j]), viewDepth(a[i]), viewDepth(a[j]),
                 chord, 1.4, p);
        emitLine(w2s(b[i]), w2s(b[j]), viewDepth(b[i]), viewDepth(b[j]),
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
            emitLine(w2s(p0), w2s(p1), viewDepth(p0), viewDepth(p1),
                     brace, 1.0, p);
        }
    }
}

void StructureStudioView::drawChildBars(QPainter &p, quint32 parentId) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (Truss *bar, props->trusses())
        if (bar != nullptr && bar->parentTrussId() == parentId)
            drawOneStructure(p, TrussKind, bar->id());
}

void StructureStudioView::drawOneStructure(QPainter &p, Kind kind, quint32 id) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    /* A selected/dragged bar reads distinctly from an ordinary truss chord --
     * otherwise clicking one gave no feedback at all that anything had
     * happened ("when I select it .. it doesn't hilight"). Amber matches the
     * rest of this view's "something is selected/active" colour (aim traces,
     * the cursor readout), not any fixture's own colour, so it can never be
     * mistaken for the bar's actual gel. */
    const bool barSelected = (kind == TrussKind)
                            && (id == m_selectedBarId || id == m_dragBarId);
    const QColor steel = barSelected ? QColor(255, 190, 60) : QColor(150, 154, 165);
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
                emitLine(w2s(sa), w2s(sb), viewDepth(sa), viewDepth(sb),
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
            /* A bar hung on this truss is a child Truss (its own world origin
               already derived by recomputeChildTrusses()), never drawn before
               now -- present in the "Fixtures and bars" list, selectable there,
               reachable via double-click, but invisible on the very canvas that
               list sits beside, with nothing to click, drag, or even see where
               it actually sits ("still not visible .. how do you keep missing
               this"). Safe to draw right here in Angled: everything in this
               branch is deferred into m_ops for a real depth-sorted flush (see
               drawRigDepthSorted()), so draw ORDER doesn't decide what is in
               front of what -- real depth does. Flat planes are different; see
               paintEvent(), which calls drawChildBars() itself, separately,
               after fixtures. */
            drawChildBars(p, id);
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
        /* Bars hung on this truss are NOT drawn here. A flat plane is plain
           painter's-algorithm (see paintEvent()): whatever paints later sits
           in front. drawStructure() -- which is what calls drawOneStructure(),
           and therefore this whole function -- runs BEFORE drawFixtures(), so
           a bar drawn at this point would always end up hidden behind any
           fixture whose icon happened to overlap it on screen ("truss visible
           .. but behind fixtures that are in the truss .. that shouldn't be
           the case"). paintEvent() calls drawChildBars() itself, separately,
           AFTER fixtures, so a bar reads on top the way something you are
           meant to click and drag has to. */
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

void StructureStudioView::fixtureFrame(const FixtureRigProps &rp,
                                       QVector3D &L, QVector3D &N, QVector3D &H) const
{
    L = fixtureAxisLocal(rp);
    switch (rp.studioMount)
    {
    case 0:  N = QVector3D(0, 0, 1); break;            // laid flat: depth is up
    case 2:  N = QVector3D(1, 0, 0); break;            // side face: depth across stage
    case 1:
    default: N = QVector3D(0, 1, 0); break;            // front face: depth upstage
    }
    H = QVector3D::crossProduct(N, L);
    if (H.length() < 1e-6f)
        H = QVector3D(0, 0, 1);
    H.normalize();

    /* ROLL about the fixture's own length. A bar lying in a step is built on a
       diagonal inside the box so it fires up through the plexi toward the cast,
       not straight up -- studioAngle cannot say that, because it turns the
       fixture WITHIN its face rather than tipping the face. Rotating N and H
       about L keeps the frame orthonormal, so everything downstream (body,
       pixels, beam direction) follows without knowing this exists. */
    if (!qFuzzyIsNull(rp.studioTilt))
    {
        const double t = qDegreesToRadians(double(rp.studioTilt));
        const double c = qCos(t), sn = qSin(t);
        const QVector3D n0 = N, h0 = H;
        // Positive tips the face AWAY from H. For a bar lying on a top face
        // and running stage left-right, H is downstage, so +20 tips it upstage
        // -- which is the direction you would describe it in.
        N = (n0 * float(c) - h0 * float(sn)).normalized();
        H = (h0 * float(c) + n0 * float(sn)).normalized();
    }
}

QVector3D StructureStudioView::fixtureBodyUp(const FixtureRigProps &rp) const
{
    /* fixtureFrame()'s H is an AXIS. Its sign is an accident of the cross
       product: for the default front-facing plane with a fixture running along
       X it comes out (0,0,-1), pointing at the floor. A box does not care --
       it is symmetric about H either way -- but a moving head is base at one
       end and lens at the other, so the whole rig drew its movers assembled
       upside down, base in the air and head slung underneath, whatever they
       were actually rigged as. The elevations never had this: they take the
       same question to mountingType and get a real answer.
     *
       So: ask mountingType here too, and only flip a sign. Hung means the body
       hangs below what holds it; anything else means it stands on it. A
       side-arm clamp has no up or down of its own and reads as standing, which
       is what it did before and is the better of the two guesses. When H is
       horizontal there is nothing to choose and the frame is left alone. */
    QVector3D L, N, H;
    fixtureFrame(rp, L, N, H);

    /* A mover's base sits flat on the face it is mounted to.
     *
       fixtureFrame() describes a fixture lying IN a plane: for a top face
       (studioMount 0) the length runs across the deck, the depth N points up,
       and H -- the only axis left -- lies flat. That is right for a bar or a
       pixel strip laid in a step, and it is what H means here. But a moving
       head is not laid in its face, it is stood on it: taking the in-plane H as
       its up drew every mover on a deck lying on its side, arms out sideways
       and head poking along the floor. On a face that looks upward, up is the
       face's own normal. Front and side faces keep the in-plane answer. */
    if (qAbs(N.z()) > 0.7f && qAbs(H.z()) < 0.7f)
        H = N;

    /* You cannot hang from the top of something you are standing on.
     *
       mountingType defaults to TopHung, so a fixture dropped onto a deck and
       never given an orientation claims to be hanging. fixtureRigPosition()
       ignores that and rightly puts it on the platform TOP -- but seating the
       body along its own up axis then pushed it DOWNWARD from there, into the
       solid platform holding it up. Two fixtures out of eight in one rig looked
       right, and they were exactly the two someone had set to FloorMounted by
       hand.

       So where the mount is a surface with nothing above it, upright is the
       only answer the geometry allows and mountingType does not get a vote. A
       tower SHELF is excluded on purpose: you really can hang under one of
       those, which is why the option exists there at all. */
    const bool standsOnTop =
           rp.onDeck()
        || (rp.onRiser() && rp.riserFace == FixtureRigProps::RiserTop)
        || (rp.onTower() && rp.towerMountSide != FixtureRigProps::TowerShelf);

    const float want = (rp.mountingType == Truss::TopHung && standsOnTop == false)
                       ? -1.0f : 1.0f;
    if (H.z() * want < 0.0f)
        H = -H;
    return H;
}

QVector3D StructureStudioView::fixtureCentre(quint32 fid) const
{
    // The rule itself lives in fixturevisualtraits, so the properties dialog
    // and anything else that reports a position give the same answer.
    return fixtureWorldCentre(m_doc, fid);
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
    return fixtureCentre(fid) - fixtureAxisLocal(rp) * float(half);
}

QVector3D StructureStudioView::fixtureEndB(quint32 fid) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const double half = qMax(0.05, fixtureLenM(fid) / 2.0);
    const quint32 fg = hasStructuralMount(rp) ? 0 : props->fixtureFrameGroup(fid);
    if (fg != 0)
        return props->groupLocalToWorld(fg, rp.groupLocal + fixtureAxisLocal(rp) * float(half));
    return fixtureCentre(fid) + fixtureAxisLocal(rp) * float(half);
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
    const QPointF c = w2s(fixtureCentre(fid));
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
    // Nothing declared (most community/custom defs still leave Width/Height
    // at 0): a nominal per-head footprint -- roughly a compact moving
    // head's real width with a yoke (0.25 m) or a simpler focus-less
    // head (0.18 m) -- run through the SAME scale-and-floor formula the
    // declared-width branch above uses. This used to be a bare pixel
    // constant with no m_scale factor at all, so an undeclared fixture's
    // icon stayed a fixed screen size no matter how far in or out the
    // structure around it was zoomed, instead of growing/shrinking with
    // it like everything else on this canvas.
    const double perHeadPhysW = traits.hasFocus ? 0.25 : 0.18;
    return qMax(12.0, perHeadPhysW * m_scale) * qMax(1, traits.headCount);
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
    const QPointF c = w2s(fixtureCentre(fid));

    QVector3D L, N, H;                                 // length / mount normal / the rest
    fixtureFrame(rp, L, N, H);

    const double w = (traits.physW > 0.0f) ? double(traits.physW) : qMax(0.05, fixtureLenM(fid));
    const double h = (traits.physH > 0.0f) ? double(traits.physH) : w * 0.2;
    const double d = (traits.physD > 0.0f) ? double(traits.physD) : h;

    // A world vector's screen delta: w2s() is affine, so the offset from the
    // centre is enough and the origin cancels.
    const QVector3D worldC = fixtureCentre(fid);
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
    const QVector3D c = fixtureCentre(fid);

    QVector3D L, N, H;
    fixtureFrame(rp, L, N, H);

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
/* How far a beam travels before it lands on something.
 *
 * The stage floor and the TOP of any platform it passes over -- which is what
 * actually catches light on a rig built out of steps. Everything else (walls,
 * performers, haze) is out of scope, so a beam that hits nothing gets a
 * sensible throw rather than running to the horizon. */
/* Does the beam land on anything at all, or is it thrown into the room? */
bool StructureStudioView::beamHitsSomething(const QVector3D &apex,
                                           const QVector3D &dir) const
{
    bool hit = false;
    beamThrow(apex, dir, &hit);
    return hit;
}

double StructureStudioView::beamThrow(const QVector3D &apex, const QVector3D &dir,
                                     bool *hit) const
{
    static const double NO_HIT = 8.0;      // metres, for a beam fired into the air
    static const double MAX_THROW = 24.0;
    static const double MIN_THROW = 0.05;

    /* Distance along dir to a horizontal plane, or -1 for "does not reach it".
       Only downward beams land on anything here; one thrown level or upward
       runs off into the room. */
    auto toPlane = [&](double planeZ) -> double {
        if (dir.z() > -1e-4f)
            return -1.0;
        const double t = (planeZ - double(apex.z())) / double(dir.z());
        return (t > MIN_THROW && t <= MAX_THROW) ? t : -1.0;
    };

    double best = toPlane(0.0);            // the stage floor

    // Platform tops, but only where the beam actually crosses the footprint.
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (StagePlatform *pl, props->platforms())
    {
        if (pl == nullptr)
            continue;
        const double topZ = double(props->platformBaseZ(pl->id())) + double(pl->height());
        if (double(apex.z()) <= topZ)      // already at or below it
            continue;
        const double t = toPlane(topZ);
        if (t < 0.0 || (best > 0.0 && t >= best))
            continue;
        const QVector3D at = apex + dir * float(t);
        if (at.x() < pl->originX() || at.x() > pl->originX() + pl->width())
            continue;
        if (at.y() < pl->originY() || at.y() > pl->originY() + pl->depth())
            continue;
        best = t;
    }

    if (hit != nullptr)
        *hit = (best > 0.0);
    return (best > 0.0) ? best : NO_HIT;
}

/* The screen silhouette of a convex point set -- Andrew's monotone chain.
 *
 * A cone's outline is exactly the hull of its apex plus the ring at the far
 * end, whichever way the camera is looking at it, so there is no need to work
 * out which two ring points are the silhouette edges. */
/* A point plus whatever travels with it -- alpha, here. The hull reorders its
 * input, so anything per-vertex has to be reordered with it. */
struct HullPt
{
    QPointF p;
    double  a;
    double  z = 0.0;     ///< depth, for callers that need it carried along
};

static QVector<HullPt> convexHullWith(QVector<HullPt> pts)
{
    if (pts.size() < 3)
        return pts;
    std::sort(pts.begin(), pts.end(), [](const HullPt &a, const HullPt &b) {
        return (a.p.x() != b.p.x()) ? (a.p.x() < b.p.x()) : (a.p.y() < b.p.y());
    });
    auto cross = [](const HullPt &o, const HullPt &a, const HullPt &b) {
        return (a.p.x() - o.p.x()) * (b.p.y() - o.p.y())
             - (a.p.y() - o.p.y()) * (b.p.x() - o.p.x());
    };
    QVector<HullPt> h(2 * pts.size());
    int k = 0;
    for (int i = 0; i < pts.size(); ++i)
    {
        while (k >= 2 && cross(h[k - 2], h[k - 1], pts[i]) <= 0) --k;
        h[k++] = pts[i];
    }
    for (int i = pts.size() - 2, t = k + 1; i >= 0; --i)
    {
        while (k >= t && cross(h[k - 2], h[k - 1], pts[i]) <= 0) --k;
        h[k++] = pts[i];
    }
    h.resize(qMax(0, k - 1));
    return h;
}

/* A beam: the cone from a head out to whatever it lands on, plus the pool
 * where it lands.
 *
 * Translucent, and deliberately flat rather than gradient-shaded -- the draw
 * queue carries one colour per primitive, and a beam that reads as "light going
 * that way, this colour, this bright" is the whole job here. Sorted at its
 * MIDPOINT: a beam is a volume and no single depth is right for it, but its
 * middle behaves better than either end (an apex depth puts a long throw in
 * front of everything it crosses; a landing depth hides it behind its own
 * fixture). */
/* Which fixtures are aimed at which point, from the active scene's Aim
 * palettes.
 *
 * This is the same association the studio draws as dashed lines: the scene
 * names a StageTarget, and every AIMABLE fixture the scene touches is pointed
 * at it. The rig view was reading live pan/tilt DMX and nothing else, so with
 * nothing driving the rig it had nothing to show -- you could set a target,
 * watch the studio draw the lines, and see the heads in the rig view still
 * pointing wherever they were left. The target is the INTENT; show it. */
void StructureStudioView::rebuildAimTargets()
{
    m_aimTarget.clear();
    m_sceneValues.clear();
    if (m_doc == nullptr || m_activeSceneId == Function::invalidId())
        return;

    Scene *scene = qobject_cast<Scene *>(m_doc->function(m_activeSceneId));
    if (scene == nullptr)
        return;

    MonitorProperties *props = m_doc->monitorProperties();

    // Where the scene is aiming. More than one Aim palette is legal; the first
    // that resolves wins, which is what a single head can actually do.
    quint32 aimTargetId = 0xFFFFFFFF;
    QColor aimColour(255, 180, 0);
    bool haveAim = false;

    /* A follow-spot scene aims at a SUBJECT, not at a mark on the floor: the
       target's XY says where the person is, and the height comes from
       aimSubjectHeight() above whatever they are standing on. QLCPalette does
       exactly this when it resolves an Aim palette, and if the two disagree the
       rig view points somewhere the rig does not. */
    bool subjectMode = false;
    foreach (quint32 pid, scene->palettes())
    {
        QLCPalette *p2 = m_doc->palette(pid);
        if (p2 != nullptr && p2->type() == QLCPalette::Effect
            && p2->scriptPath().contains(QLatin1String("followspot"), Qt::CaseInsensitive))
        {
            subjectMode = true;
            break;
        }
    }
    foreach (quint32 pid, scene->palettes())
    {
        QLCPalette *pal = m_doc->palette(pid);
        if (pal == nullptr || pal->type() != QLCPalette::Aim)
            continue;
        StageTarget *t = props->stageTarget(pal->stageTargetId());
        if (t == nullptr)
            continue;
        aimTargetId = t->id();
        if (t->color().isValid())
            aimColour = t->color();
        haveAim = true;
        break;
    }
    /* What the scene would SEND. A look is a design-time thing: you build it
       with a dimmer, a colour and an aim, and the rig view's job is to show you
       what it does -- whether or not a desk is currently outputting it. Waiting
       for live DMX made the whole thing conditional on a toggle that is really
       about something else ("why does it need to be live? it should work in
       design").
     *
       Resolved into channel-value arrays shaped exactly like live DMX, so
       everything downstream -- colour, level, beam, visibility -- reads it
       without knowing the difference.
     *
       Effect palettes are skipped, and so is anything an effect has taken over
       (a Colour or Dimmer palette listed AFTER an Effect feeds that effect
       rather than painting a static base), which is the same rule Scene::write
       applies. An effect's output is a running thing, not a static look, and
       guessing at it here would be worse than saying nothing. */
    foreach (quint32 pid, scene->palettes())
    {
        QLCPalette *pal = m_doc->palette(pid);
        if (pal == nullptr)
            continue;
        if (pal->type() == QLCPalette::Effect || pal->type() == QLCPalette::Aim)
            continue;
        if (QLCPalette::isEffectScoped(m_doc, scene->palettes(), pid))
            continue;

        const int cOff = QLCPalette::colorSetOffset(m_doc, scene->palettes(), pid);
        QList<SceneValue> vals =
            pal->valuesFromFixtureGroups(m_doc, scene->fixtureGroups(), scene, cOff);
        vals += pal->valuesFromFixtures(m_doc, scene->fixtures(), scene, cOff);

        foreach (const SceneValue &scv, vals)
        {
            Fixture *fx = m_doc->fixture(scv.fxi);
            if (fx == nullptr || scv.channel >= fx->channels())
                continue;
            QByteArray &arr = m_sceneValues[scv.fxi];
            if (arr.size() < int(fx->channels()))
                arr.resize(int(fx->channels()));
            arr[int(scv.channel)] = char(scv.value);
        }
    }

    if (haveAim == false)
        return;

    // Everything the scene touches, directly or through a group.
    QSet<quint32> candidates;
    foreach (quint32 fid, scene->fixtures())
        candidates.insert(fid);
    foreach (quint32 gid, scene->fixtureGroups())
    {
        FixtureGroup *fg = m_doc->fixtureGroup(gid);
        if (fg == nullptr)
            continue;
        foreach (quint32 fid, fg->fixtureList())
            candidates.insert(fid);
    }

    foreach (quint32 fid, candidates)
    {
        Fixture *fx = m_doc->fixture(fid);
        if (fx == nullptr)
            continue;
        /* Only fixtures that can actually aim. A wash or a pixel panel has
           nothing to point, and an Aim palette is a no-op on it -- the studio
           makes the same check before drawing a line. */
        const bool canAim =
            fx->channelNumber(QLCChannel::Pan,  QLCChannel::MSB) != QLCChannel::invalid()
            || fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) != QLCChannel::invalid();
        if (canAim)
        {
            AimSpec spec;
            spec.targetId = aimTargetId;
            spec.colour = aimColour;
            spec.subjectHeight = subjectMode;
            m_aimTarget.insert(fid, spec);
        }
    }
}

/* The point a fixture is currently aimed at, read FRESH: the target may have
 * been dragged since the cache was built. */
bool StructureStudioView::aimPointFor(quint32 fid, QVector3D &out) const
{
    const QHash<quint32, AimSpec>::const_iterator it = m_aimTarget.constFind(fid);
    if (it == m_aimTarget.constEnd())
        return false;

    MonitorProperties *props = m_doc->monitorProperties();
    StageTarget *t = props->stageTarget(it.value().targetId);
    if (t == nullptr)
        return false;

    /* The shared rule -- binding, kind and height all resolved in one place, so
       this view and QLCPalette cannot drift apart again. */
    out = props->targetAimPoint(t, it.value().subjectHeight);
    return true;
}

void StructureStudioView::setActiveScene(quint32 sceneId)
{
    if (m_activeSceneId == sceneId)
        return;
    m_activeSceneId = sceneId;
    rebuildAimTargets();
    update();
}

/* A dashed run from a head to the point it is aimed at.
 *
 * Dashed on purpose: this is an INTENTION, not light. A beam is what the rig is
 * emitting; this is where a look says to point, and the two want telling apart.
 * Segment by segment, because the draw queue carries no pen style.
 *
 * Drawn ON TOP rather than depth-tested, which is deliberate and was NOT the
 * first attempt. Depth-tested is physically right and practically useless: on
 * this rig the movers sit about 0.6 m up and the target about 0.4 m, so the run
 * between them skims horizontally THROUGH a stack of solid decks and almost all
 * of it is correctly hidden. An annotation is not an object in the room -- the
 * studio draws the same lines over everything, and the two views have to agree. */
void StructureStudioView::drawAimTrace(QPainter &p, const QVector3D &from,
                                      const QVector3D &to, const QColor &colour) const
{
    const QVector3D run = to - from;
    const double len = double(run.length());
    if (len < 0.05)
        return;

    QColor c = colour;
    c.setAlpha(150);

    // About a dash every 200 mm, capped so a long throw is not a thousand ops.
    const int dashes = qBound(4, int(len / 0.2), 60);
    for (int i = 0; i < dashes; ++i)
    {
        const double t0 = double(i) / dashes;
        const double t1 = t0 + (0.55 / dashes);      // gap between each
        const QVector3D a = from + run * float(t0);
        const QVector3D b = from + run * float(t1);
        if (m_collecting)
        {
            DrawOp o;
            o.kind = DrawOp::Trace;
            o.depth = (viewDepth(a) + viewDepth(b)) / 2.0;
            o.poly << w2s(a) << w2s(b);
            o.pen = c;
            o.penWidth = 1.6;
            m_ops << o;
        }
        else
        {
            p.setPen(QPen(c, 1.6));
            p.drawLine(w2s(a), w2s(b));
        }
    }
}

void StructureStudioView::setBeams(bool on)
{
    if (m_beams == on)
        return;
    m_beams = on;
    update();
}

void StructureStudioView::drawBeamCone(QPainter &p, const QVector3D &apex,
                                      const QVector3D &dir, double halfAngleRad,
                                      const QColor &colour, double level,
                                      double haze) const
{
    if (level <= 0.004 || dir.isNull())
        return;

    const QVector3D d = dir.normalized();
    /* Out to whatever it actually lands on -- NOT to what it is aimed at.
     *
       A target is a point in the air, not a surface. This used to end the cone
       at the subject, to stop follow-spots aimed up at chest height sailing
       eight metres on into a wash; but light does not stop where it is
       pointed ("why does the beam end on the target .. it won't in reality").
       A beam through a subject carries on to the floor, the set, or into the
       room, and fades out on its own when it meets nothing. */
    /* Did it land on something, or is it thrown into the room? A beam that
       lands keeps its strength to the surface -- a few metres of air takes
       very little out of it -- and puts a pool where it arrives. One thrown
       into the air has nothing to stop it, so it has to fade out on its own,
       or it ends in a hard disc hanging in space. */
    bool lands = false;
    const double len = beamThrow(apex, d, &lands);

    /* Brighter in a dark room, which is both true and useful: at work light you
       want to see the rig, at blackout you want to see the light. */
    const double room = 1.25 - 0.65 * m_ambient;

    /* Strictly PROPORTIONAL to the level, with no floor under it. There used
       to be a constant 22 in here, which meant a head at 3% threw a beam that
       measured 40 against a full beam's 78 -- it barely dimmed at all and then
       snapped off at zero. */
    /* Emitted as LIGHT (see ZRaster::light), which adds up and rolls off short
       of white: layered instead, stacked beams converged on their colour, and
       for white that was a solid glare ("blue worked great .. white .. does
       not .. it's too overdriven"). @p haze shares the glow of beams that
       share their air. */
    const double baseA = 95.0 * level * room * qBound(0.0, haze, 1.0);
    const double poolA = 130.0 * level * room;

    // Where it ends: the pool's shape, and the far end of the cone's outline.
    const QVector<QVector3D> footprint = beamFootprint(apex, d, halfAngleRad, len, lands);

    /* ONE polygon, faded along its length by per-vertex alpha.
     *
       This was sixteen constant-alpha slices stacked end to end, and every seam
       between them read as a ring drawn across the beam -- "the light cone
       looks kinda weird with the multiple circles on a cone". Alpha interpolates
       across a polygon exactly as depth does, so the whole cone is one
       primitive with a smooth gradient and no seams to see. */
    QVector<HullPt> pts;
    pts.reserve(footprint.size() + 1);
    pts << HullPt{ w2s(apex), 1.0, viewDepth(apex) };

    /* Light spreads, so even a beam that lands thins out along the way; one
       going nowhere has to reach zero by the end or it terminates in mid-air. */
    const double farA = lands ? 0.65 : 0.0;
    foreach (const QVector3D &q, footprint)
        pts << HullPt{ w2s(q), farA, viewDepth(q) };

    const QVector<HullPt> hull = convexHullWith(pts);
    if (hull.size() >= 3)
    {
        /* Triangles fanned out from the lens, not one polygon.
         *
           The rasteriser fits a polygon's alpha as a PLANE through its first
           three corners. That is exact for a triangle and wrong for a cone:
           when those corners were all far-end points the fade came out flat,
           and when they straddled the lens it tilted sideways and overshot to
           solid -- which one depended only on which way the beam pointed on
           screen ("some lights are right, some are solid"). Every triangle
           here has the lens at one corner, so each one's fade is exact.
         *
           The lens is either a corner of the outline or inside it (a beam
           pointed at or away from the eye), and a fan from it covers the
           outline exactly once either way. */
        const QPointF lensPx = w2s(apex);
        int lensAt = -1;
        for (int i = 0; i < hull.size(); ++i)
            if (hull.at(i).a >= 1.0 && QLineF(hull.at(i).p, lensPx).length() < 1e-6)
            {
                lensAt = i;
                break;
            }
        QVector<HullPt> rim;
        for (int i = 1; i <= hull.size(); ++i)
        {
            const int k = (lensAt < 0) ? (i - 1) : (lensAt + i) % hull.size();
            if (k != lensAt)
                rim << hull.at(k);
        }
        // Round the far end only when the lens is inside; as a corner, the two
        // outline edges through it close the fan and must not be bridged.
        const int spans = (lensAt < 0) ? rim.size() : rim.size() - 1;

        QColor c = colour;
        c.setAlpha(qBound(0, int(baseA), 190));
        /* Each corner at its own depth. One depth for the whole beam -- its
           middle -- put a beam thrown away from the eye BEHIND the deck it
           starts on, so the first metre of it vanished. A triangle's depth is
           exactly a plane, so the lens and each far point can carry their
           real depths. The nudge toward the eye keeps a beam's far end from
           losing a tie with the surface it lands on. */
        const double lift = 2e-4;
        const double lensZ = viewDepth(apex) + lift;
        const double sortZ = viewDepth(apex + d * float(len * 0.5));
        for (int i = 0; i < spans; ++i)
        {
            const HullPt &a = rim.at(i);
            const HullPt &b = rim.at((i + 1) % rim.size());
            QPolygonF tri;
            tri << lensPx << a.p << b.p;
            emitLight(tri, sortZ, c, p,
                      QVector<double>({ lensZ, a.z + lift, b.z + lift }),
                      QVector<double>({ 1.0, a.a, b.a }));
        }
    }

    /* The pool, only where the beam actually arrives on something -- lying ON
       that surface, so each point carries its own depth: an oval a couple of
       metres long spans real depth, and one number for all of it sank the far
       end into the deck. */
    if (lands)
    {
        QPolygonF poolPoly;
        QVector<double> poolZ;
        double sum = 0.0;
        foreach (const QVector3D &q, footprint)
        {
            poolPoly << w2s(q);
            const double z = viewDepth(q) + 1e-4;
            poolZ << z;
            sum += z;
        }
        QColor poolCol = colour;
        poolCol.setAlpha(qBound(0, int(poolA), 220));
        emitLight(poolPoly, sum / qMax(1, footprint.size()), poolCol, p, poolZ);
    }
}

QVector<QVector3D> StructureStudioView::beamFootprint(const QVector3D &apex, const QVector3D &dir,
                                                     double halfAngleRad, double length,
                                                     bool lands) const
{
    /* Sixteen points round the end of a beam. It is a soft glow, and every
       point is another triangle of light to fill: 24 were indistinguishable
       and cost a third more of the frame's beam work. */
    static const int RING = 16;
    const QVector3D d = dir.normalized();
    const double half = qBound(0.005, halfAngleRad, 1.4);

    QVector3D side = QVector3D::crossProduct(QVector3D(0, 0, 1), d);
    if (side.length() < 1e-3f)
        side = QVector3D(1, 0, 0);
    side.normalize();
    const QVector3D up = QVector3D::crossProduct(d, side).normalized();

    const QVector3D axisEnd = apex + d * float(length);
    const double landZ = double(axisEnd.z());
    /* A beam arriving nearly edge-on to the deck has edge rays that run on for
       tens of metres before they come down, or never do. Cap them: past a few
       times the axis throw the pool is too faint to matter and the polygon
       would swamp the view. */
    const double maxRun = qMin(24.0, length * 4.0);

    QVector<QVector3D> out;
    out.reserve(RING);
    for (int i = 0; i < RING; ++i)
    {
        const double a = 2.0 * M_PI * i / RING;
        const QVector3D across = side * float(qCos(a)) + up * float(qSin(a));
        if (!lands)
        {
            // Nothing to land on: square across the axis, as far as it goes.
            out << axisEnd + across * float(length * qTan(half));
            continue;
        }
        /* One of the cone's edge rays, followed down to the plane the axis
           landed on. The set of them is the conic section the beam makes on
           that surface -- a circle when it arrives square, an oval stretched
           along the beam when it arrives at an angle. */
        const QVector3D ray = (d * float(qCos(half)) + across * float(qSin(half))).normalized();
        double run = maxRun;
        if (ray.z() < -1e-4f)
            run = qMin(maxRun, (landZ - double(apex.z())) / double(ray.z()));
        out << apex + ray * float(qMax(0.0, run));
    }
    return out;
}

void StructureStudioView::drawMoverSolid(QPainter &p, quint32 fid,
                                         const FixtureVisualTraits &traits,
                                         const QColor &col,
                                         const QVector<MoverHeadLook> &heads,
                                         bool ambientLit, double beamAngle) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const QVector3D c = fixtureCentre(fid);

    QVector3D L, N, H;
    fixtureFrame(rp, L, N, H);
    // Base at the bottom, head on top -- which requires knowing which way up
    // the thing is rigged, not just which plane it lies in.
    H = fixtureBodyUp(rp);
    // Depth is whatever is left: when up was taken from the face normal (a
    // head standing on a deck), N would otherwise be up as well and every box
    // would collapse to a sheet.
    const QVector3D depth = QVector3D::crossProduct(L, H);
    if (depth.length() > 1e-4f)
        N = depth.normalized();

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
        drawSolidBox(p, k, col, overlapAwareEdgeColor(fid, col), 255, ambientLit);
    };

    /* Stand it ON what holds it, not THROUGH it.
     *
       fixtureRigPosition() returns the point the fixture is fastened to -- the
       shelf surface, the truss chord -- and building the body symmetrically
       about that point buried the bottom half of it inside the structure, so a
       followspot set on top of a tower was drawn sunk into the tower's top bay
       ("now it's in the tower vs on top of it where I have it set in the
       editor"). The elevations never did this: towerFixtureBodyRect() has
       always put the base flush with the shelf line, which is the other half of
       the same disagreement.

       Along the BODY's own up axis, not the mount plane normal: what "down"
       means for a fixture standing on a shelf is its base, and for one hung
       under a truss it is its lens. fixtureBodyUp() already resolved which.

       Not for a fixture rigged INSIDE something, though -- between a truss's
       chords, or within a clear-topped step firing up through it. That is meant
       to be inside the volume, which is why placement is a property of its own.

       And not for a free-placed fixture either: there is no surface under it to
       stand on, and its plotted position is its CENTRE -- which is what the 2D
       plot, the hit test and every other view already agree it is. Seating one
       just shifts it half a body off the spot it was put on. */
    const double seat = (rp.placement == FixtureRigProps::Inside
                         || hasStructuralMount(rp) == false) ? 0.0 : 0.5;
    const QVector3D seated = c + H * float(h * seat);

    for (int u = 0; u < units; ++u)
    {
        const double off = (u - (units - 1) * 0.5) * slice;
        const QVector3D uc = seated + L * float(off);
        const MoverHeadLook look = heads.isEmpty() ? MoverHeadLook()
                                                   : heads.at(qMin(u, heads.size() - 1));
        const QVector3D &aim = look.aim;

        // Base: the block that bolts to the truss, at the "down" end of H.
        box(uc - H * float(h * 0.34), slice * 0.46, h * 0.16, d * 0.5, L, H, N);

        // Two arms rising from it, at the outer edges of this head's slice.
        const double armH = h * 0.30;
        const QVector3D armMid = uc - H * float(h * 0.02);
        box(armMid + L * float(slice * 0.36), slice * 0.10, armH, d * 0.22, L, H, N);
        box(armMid - L * float(slice * 0.36), slice * 0.10, armH, d * 0.22, L, H, N);

        /* The head, slung between the arms. Given an aim, build it around THAT
           direction rather than the fixture's own axes, so the head visibly
           looks where it is looking -- a box that merely shifts a little reads
           as a glitch, not as a mover pointing somewhere. */
        QVector3D headMid = uc + H * float(h * 0.10);
        if (!aim.isNull())
        {
            const QVector3D beam = aim.normalized();
            // A frame around the beam: any two axes perpendicular to it will do.
            QVector3D side = QVector3D::crossProduct(QVector3D(0, 0, 1), beam);
            if (side.length() < 1e-3f)                 // aiming straight up/down
                side = L;
            side.normalize();
            const QVector3D up = QVector3D::crossProduct(beam, side).normalized();

            headMid += beam * float(h * 0.12);
            box(headMid, slice * 0.26, h * 0.26, d * 0.26, side, beam, up);

            // A stub of beam, so the direction reads at overview scale where the
            // head itself is only a few pixels across. Kept even when the full
            // cone is drawn: it is what makes an UNLIT head's aim readable.
            const QVector3D tip = headMid + beam * float(qMax(0.25, h * 1.4));
            emitLine(w2s(headMid), w2s(tip),
                     viewDepth(headMid), viewDepth(tip),
                     overlapAwareEdgeColor(fid, col), 1.4, p);

            if (m_beams && look.beamLevel > 0.01)
            {
                /* An undeclared lens has to become SOMETHING here -- a beam is
                   the point of the exercise and most definitions in the wild
                   declare 0. 14 degrees is an ordinary spot, narrow enough to
                   read as a beam rather than a wash. beamAngle comes from the
                   caller because it is a LIVE value on a zoom head. */
                const double deg = (beamAngle > 0.0 && beamAngle < 180.0)
                                   ? beamAngle : 14.0;
                /* Every emitter the head carries, in a row across its face on
                   the same axis the head box was just built along. Each is a
                   whole beam in the head's colour: three LEDs behind three
                   lenses are three shafts of light near the fixture, and only
                   merge into one further out. */
                const int beams = qMax(1, traits.beamsPerHead);
                const double pitch = (slice * 0.52) / beams;
                const QVector3D lens = headMid + beam * float(h * 0.28);
                const QColor lightColour = look.beamColour.isValid() ? look.beamColour : col;
                /* One head's beams share almost all of their air: three
                   overlapping cones at full haze read as three times the beam.
                   Each lights what it lands on in full; only the glow in the
                   air is shared out. */
                const double haze = qPow(double(beams), -0.35);
                for (int b = 0; b < beams; ++b)
                {
                    const QVector3D apex = lens + side * float((b - (beams - 1) * 0.5) * pitch);
                    drawBeamCone(p, apex, beam, qDegreesToRadians(deg / 2.0),
                                 lightColour, look.beamLevel, haze);
                    m_frameBeams << FrameBeam{ apex, beam.normalized(),
                                               qDegreesToRadians(deg / 2.0),
                                               beamThrow(apex, beam.normalized()),
                                               lightColour, look.beamLevel };
                }
            }
        }
        else
        {
            box(headMid, slice * 0.26, h * 0.22, d * 0.30, L, H, N);
        }
    }
}

/* One fixture. Split out of drawFixtures() so the whole-rig overview can
 * interleave fixtures and structures in a single depth-sorted pass -- drawing
 * all structures and then all fixtures put a step in front of a tower that was
 * actually nearer the eye. */
/* The emit-vs-room rule, in one place: a lamp is as bright as it is driven,
   and the room level only decides how visible an UNLIT one is. Both the
   fixture body and the individual pixels of a bar go through this. */
/* The neutral BODY of a fixture: the housing, not the lamp.
 *
 * The studio paints a fixture body a flat dark grey and lets its LED heads
 * carry every bit of colour. The overview used to tint the body -- and worse,
 * OUTLINE it in a lightened version -- with the fixture-wide colour. On a
 * 25 mm-tall LED tape that outline is most of the fixture on screen, so a
 * strip whose pixels were mostly dark still drew a solid pale line end to
 * end. That was the "white" in the rig view.
 *
 * Room-lit like any other object in the space, on the same curve as the
 * scenery in drawSolidBox(). */
/* The unit vector pointing from the scene TOWARD the camera.
 *
 * viewDepth() is affine and larger-is-nearer, so its gradient is exactly that
 * direction. Deriving it rather than rebuilding it from the azimuth/elevation
 * means it stays right for every plane and every rotation for free -- and
 * there is no second copy of the projection's sign conventions to get wrong. */
QVector3D StructureStudioView::toViewer() const
{
    const double d0 = viewDepth(QVector3D(0, 0, 0));
    QVector3D g(float(viewDepth(QVector3D(1, 0, 0)) - d0),
                float(viewDepth(QVector3D(0, 1, 0)) - d0),
                float(viewDepth(QVector3D(0, 0, 1)) - d0));
    if (g.length() < 1e-6f)
        return QVector3D(0.0f, 1.0f, 0.0f);
    g.normalize();
    return g;
}

/* Which way a fixture actually throws its light.
 *
 * A mover knows: it is wherever pan and tilt are pointing it. Anything else
 * emits AWAY from whatever is holding it, and the honest way to find that is
 * the vector from the host structure's centre out to the fixture -- no axis
 * convention to get backwards, which matters here because the mount-normal
 * signs in this file are not consistent with each other (see barFaceVector's
 * KNOWN INCONSISTENCY note).
 *
 * Returns false when there is no defensible answer -- a fixture rigged INSIDE
 * a step, for one, where "out" is whatever gap it is aimed through. */
bool StructureStudioView::emitDirection(Fixture *fx, const FixtureRigProps &rp,
                                       const FixtureVisualTraits &traits,
                                       QVector3D &dir) const
{
    if (fx == nullptr)
        return false;
    if (traits.kind == FixtureSilhouette::Mover && fixtureAimDirection(fx, rp, dir))
        return true;
    if (rp.placement == FixtureRigProps::Inside)
        return false;

    MonitorProperties *props = m_doc->monitorProperties();
    QVector3D hostCentre;
    bool haveHost = false;

    const quint32 plId = rp.onRiser() ? rp.riserPlatformId
                       : rp.onDeck()  ? rp.deckPlatformId
                                      : FixtureRigProps::invalidPlatformId();
    if (StagePlatform *host = props->platform(plId))
    {
        const float b0 = props->platformBaseZ(plId);
        hostCentre = QVector3D(host->originX() + host->width() * 0.5f,
                               host->originY() + host->depth() * 0.5f,
                               b0 + host->height() * 0.5f);
        haveHost = true;
    }
    else if (rp.trussId != Truss::invalidId())
    {
        /* A truss runs along its length, so "out" across it is meaningless --
           but a hung fixture points DOWN and a floor-mounted one points up,
           which is the part that matters. */
        dir = (rp.mountingType == Truss::TopHung) ? QVector3D(0.0f, 0.0f, -1.0f)
                                                  : QVector3D(0.0f, 0.0f, 1.0f);
        return true;
    }

    if (haveHost == false)
        return false;

    dir = fixtureCentre(fx->id()) - hostCentre;
    if (dir.length() < 1e-4f)
        return false;
    dir.normalize();
    return true;
}

/* How much of a fixture's OUTPUT reaches the camera, 0..1.
 *
 * Inside its declared cone, all of it. Outside, it tapers to a floor rather
 * than to nothing: a lamp pointing away from you is not invisible, you can
 * still see that it is lit. An undeclared lens (0) means no falloff at all --
 * most definitions in the wild say 0, and dimming those by viewing angle would
 * black out half a rig for no reason. */
double StructureStudioView::beamVisibility(Fixture *fx, const FixtureRigProps &rp,
                                          const FixtureVisualTraits &traits,
                                          double beamDeg) const
{
    /* Only where "the angle you are viewing from" is a real thing. viewDepth()
       is built from the azimuth/elevation camera and means nothing in the flat
       planes, whose eye direction is fixed by the plane itself; dimming a
       fixture in a Front elevation would be a surprise, not a feature. */
    if (m_plane != Angled)
        return 1.0;

    if (beamDeg <= 0.0 || beamDeg >= 360.0)
        return 1.0;
    const double beam = beamDeg;

    QVector3D throwDir;                 // not "emit": that is a Qt keyword macro
    if (emitDirection(fx, rp, traits, throwDir) == false)
        return 1.0;

    const double cosA = double(QVector3D::dotProduct(throwDir, toViewer()));
    const double off = qAcos(qBound(-1.0, cosA, 1.0));
    const double half = qDegreesToRadians(beam / 2.0);
    // Taper over the cone's own width again, or 25 degrees, whichever is more:
    // a 10-degree spot should not snap from full to floor over five degrees.
    const double edge = qMin(M_PI, half + qMax(half, qDegreesToRadians(25.0)));

    static const double FLOOR = 0.15;
    if (off <= half)  return 1.0;
    if (off >= edge)  return FLOOR;
    return 1.0 - (1.0 - FLOOR) * ((off - half) / (edge - half));
}

QColor StructureStudioView::bodyShade() const
{
    const double af = 0.18 + 0.82 * m_ambient;
    const int v = qBound(0, qRound(BODY_GREY * af), 255);
    return QColor(v, v, v);
}

QColor StructureStudioView::shadeLive(const QColor &live, uchar dim,
                                     const QColor &unlit, double visibility) const
{
    /* Two separate things contribute, and both have to stay true: what the
       lamp is EMITTING, and how much of its BODY the room reveals. A lamp at
       full is its own colour whatever the room is doing; a lamp at zero is not
       invisible -- it is an object sitting in the space, faintly seen at
       blackout and plainly under work light.
     *
     * Cross-fading between the two is what holds both. Scaling the EMITTED
     * colour by max(emit, room) -- the previous rule -- left an RGB fixture at
     * zero as pure BLACK, since there is nothing there to scale up: black
     * times anything is still black. That is what turned every idle fixture in
     * the overview into a black rectangle. */
    /* visibility is how much of the output actually reaches the camera. As it
       falls the room takes over, which is right: what you are left looking at
       is the fixture's body, not its beam. */
    const double emit_ = qBound(0.0, dim / 255.0, 1.0) * qBound(0.0, visibility, 1.0);
    const double roomLit = 0.12 + 0.28 * m_ambient;
    auto mix = [&](int e, int u) {
        return qBound(0, qRound(e * emit_ + u * roomLit * (1.0 - emit_)), 255);
    };
    return QColor(mix(live.red(), unlit.red()),
                  mix(live.green(), unlit.green()),
                  mix(live.blue(), unlit.blue()));
}

/* One pixel of a multi-head fixture, in its own colour.
 *
 * Falls back to the fixture-wide colour when live output is off, or when the
 * head has nothing of its own to say -- so a plain bar looks exactly as it
 * did. */
QColor StructureStudioView::pixelColor(Fixture *fx, const QByteArray &values,
                                      int head, const QColor &fallback,
                                      const QColor &unlit, double visibility,
                                      int masterLevel) const
{
    if (!m_liveValues || fx == nullptr)
        return fallback;
    QColor hc = unlit;
    uchar hd = 0;
    if (fixtureHeadLiveState(fx, values, head, hc, hd, masterLevel) == false)
        return fallback;
    return shadeLive(hc, hd, unlit, visibility);
}

void StructureStudioView::drawOneFixture(QPainter &p, quint32 fid,
                                        bool nameEveryone) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    Fixture *fx = m_doc->fixture(fid);
    const QPointF c = w2s(fixtureCentre(fid));
    const bool hi = m_highlight.contains(fid);
    const bool drag = (fid == m_dragFid);

    QColor col = props->fixtureGelColor(fid, 0, 0);
    if (!col.isValid() || col == QColor(Qt::black))
        col = QColor(90, 160, 235);

    /* What the fixture looks like with its lamp OFF. Kept separate from the
       live colour below, because the room lights the body and the lamp lights
       itself, and a fixture at zero still has a body. */
    const QColor unlit = col;

    /* Everything below needs these, and both cost real work: classifyFixture()
       walks the definition, and liveValuesFor() is a hash lookup. Fetch each
       ONCE per fixture per frame -- classify was being run twice, and the
       values were being fetched once per HEAD, which on this rig is six
       thousand lookups a frame for no gain. */
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const FixtureVisualTraits traits = classifyFixture(fx);
    bool valsComplete = true;
    const QByteArray vals = valuesForDrawing(fx, &valsComplete);
    const bool showingOutput = (vals.isEmpty() == false);
    /* Once per fixture, not once per head -- see fixtureMasterLevel(). */
    const int master = showingOutput ? fixtureMasterLevel(fx, vals) : -1;

    /* Live output, when the rig is actually running. The gel colour above is
       what a fixture looks like UNLIT; showing that while a show plays makes
       the overview a diagram rather than a picture of the rig.
     *
       visibility is how much of this fixture's output can reach the camera
       from where we are standing: a head with a fixed cone pointed away should
       not blaze at you. */
    /* What this fixture is throwing RIGHT NOW: a zoom head's cone is a live
       value, not a property of its definition. */
    const double beamNow = showingOutput ? fixtureBeamAngle(fx, vals, traits)
                                         : double(traits.beamMaxDeg);
    const double visibility =
        showingOutput ? beamVisibility(fx, rp, traits, beamNow) : 1.0;

    if (showingOutput)
    {
        QColor live = col;
        uchar dim = 0;
        if (fixtureLiveState(fx, vals, live, dim, valsComplete))
            col = shadeLive(live, dim, unlit, visibility);
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
    if (m_plane == Angled)
    {
        /* One rule for every fixture kind here: a solid box in its own
           orientation. The flat views' silhouettes (mover yokes, bar
           tick-marks) are drawn in SCREEN space and would swing round to
           face the camera, which is exactly what "the moving heads follow
           me" was. */
        QVector3D corner[8];
        fixtureBoxCorners(fid, traits, corner);

        /* A fixture rigged INSIDE a step is inside a solid box whose faces
           each sort as a single depth, so the near face covers it unless it
           happens to sit in front of that face's midpoint. Off-axis that left
           only the nearest one or two visible -- and being able to see what is
           rigged inside a clear-topped step is the entire reason for putting
           it there. Lift it just past the container's nearest face so it is
           visible from every angle, the way its own pixels are lifted past
           their housing. */
        double depthBias = 0.0;
        {
            const quint32 plId = rp.onRiser() ? rp.riserPlatformId
                               : rp.onDeck()  ? rp.deckPlatformId
                                              : FixtureRigProps::invalidPlatformId();
            if (StagePlatform *host = props->platform(plId))
            {
                /* Three layers, working outward from the step: its own faces,
                   then whatever is rigged INSIDE it, then whatever is mounted
                   ON its surface. Lifting only the inside units put them in
                   front of the LED tape on the OUTSIDE of the same step, which
                   is backwards -- from out here the tape is the nearer thing. */
                const double layer =
                    (rp.placement == FixtureRigProps::Inside) ? 1e-4 : 3e-4;
                const float hx0 = host->originX(), hy0 = host->originY();
                const float hb0 = props->platformBaseZ(plId);
                QVector3D hc[8];
                boxCorners(hc, hx0, hy0, hb0,
                           hx0 + host->width(), hy0 + host->depth(), hb0 + host->height());
                depthBias = qMax(0.0, boxNearFaceDepth(hc) + layer
                                      - boxNearFaceDepth(corner));
            }
        }

        if (traits.kind == FixtureSilhouette::Mover)
        {
            /* Point the head where it is actually aimed, while showing live
               output. drawMoverSolid()'s aim parameter was plumbed through when
               the silhouette was built, precisely so this could arrive without
               touching the shape code. */
            /* Where this head is pointed.
             *
               The SELECTED SCENE wins. If it carries an Aim palette naming a
               target, that is where the head is being told to go, and it is
               what the studio is drawing dashed lines to -- the two views have
               to agree or the aim is unreadable.
             *
               This used to prefer live DMX, on the reasoning that the rig's
               actual output beats an intention. That is wrong in the case that
               matters: a head that nothing is driving reads as pan 0, tilt 0,
               and fixtureAimDirection() reports that perfectly happily as a
               real direction. So merely SELECTING a followspot scene left every
               head pointing downstage, because all-zero DMX is not "aimed
               there", it is "nothing is driving this" -- and the two are
               indistinguishable from the channel values alone. When the scene
               is actually running, the DMX aims at the target anyway and the
               two agree. */
            QVector3D aim;
            QVector3D aimPt;
            const bool aimed = aimPointFor(fid, aimPt);
            if (aimed)
            {
                const QVector3D toTarget = aimPt - fixtureCentre(fid);
                if (toTarget.length() > 1e-3f)
                    aim = toTarget.normalized();
            }

            /* One look per head. A two-arm fixture's arms pan, tilt and colour
               on their own channels, so reading the first head for both drew
               them as twins whatever the desk was doing. A scene target still
               aims every head at it. */
            const int units = qMax(1, traits.headCount);
            QVector<MoverHeadLook> looks(units);
            for (int u = 0; u < units; ++u)
            {
                looks[u].aim = aim;
                if (aim.isNull() && m_liveValues
                    && !fixtureAimDirection(fx, rp, looks[u].aim, u))
                    looks[u].aim = QVector3D();
            }

            /* The beam's strength and colour are the fixture's own OUTPUT, not
               the shaded body colour.
             *
               Two things must not leak in here. Room ambient: a beam is light
               in the air, and dimming it by the work lights is backwards --
               drawBeamCone() handles the room itself, by making beams read
               STRONGER as the room goes down. And beamVisibility: that says how
               much of the lens you can see from where you are standing, which
               is the opposite question. A head pointed away from you is
               precisely when its beam is most worth drawing. */
            double beamLevel = 0.0;
            if (showingOutput && m_beams)
            {
                for (int u = 0; u < units; ++u)
                {
                    QColor live = unlit;
                    uchar dim = 0;
                    const bool got = (units > 1)
                        ? fixtureHeadBeamState(fx, vals, u, live, dim, valsComplete)
                        : fixtureLiveState(fx, vals, live, dim, valsComplete);
                    if (got)
                    {
                        looks[u].beamLevel = dim / 255.0;
                        looks[u].beamColour = live;
                        beamLevel = qMax(beamLevel, looks[u].beamLevel);
                    }
                }
            }
            drawMoverSolid(p, fid, traits, col, looks, !showingOutput,
                           beamNow);

            /* Say where it is aimed -- but ONLY when the beam is not already
               saying it.
             *
               A lit head throws a beam, and the beam IS the answer; a dashed
               line beside it is clutter ("rig shouldn't show lines, should show
               the actual beams"). A head that is aimed but dark has nothing to
               show at all, though: at whole-rig zoom a mover is a few pixels
               across, so turning its box toward the target is invisible. That
               is the case the trace is for, and it is what the studio draws. */
            if (aimed && (!m_beams || beamLevel <= 0.01))
                drawAimTrace(p, fixtureCentre(fid), aimPt,
                             m_aimTarget.value(fid).colour);
        }
        else if (traits.headCount > 1)
        {
            // A pixel bar: neutral housing, ambient-lit like any object, with
            // the pixels below carrying the colour.
            const QColor body(BODY_GREY, BODY_GREY, BODY_GREY);
            drawSolidBox(p, corner, (drag || hi) ? col : body,
                         overlapAwareEdgeColor(fid, (drag || hi) ? col : body), 255,
                         !(drag || hi), depthBias);
        }
        else
            drawSolidBox(p, corner, col, overlapAwareEdgeColor(fid, col), 255, !showingOutput,
                         depthBias);

        /* Washes, pars and bars throw light too. Only movers did -- the whole
           beam pass lived inside drawMoverSolid -- so "mounted T1-W, set it to
           white wash, don't see any light from it" was the model saying that a
           fixture which cannot move cannot shine, which is nonsense on a stage
           where most of the light comes from things bolted in one direction.

           One cone per fixture, not one per head: a 64-pixel bar is one
           luminaire washing one area, and sixty-four cones of it would cost the
           frame everything the threading just bought. Where it points is
           emitDirection()'s answer -- down off a truss, out from the riser it
           is buried in -- which is the same vector the view already uses to
           decide how much of the lens you can see. */
        if (traits.kind != FixtureSilhouette::Mover && m_beams && showingOutput)
        {
            QColor live = col;
            uchar dim = 0;
            QVector3D dir;
            if (fixtureLiveState(fx, vals, live, dim, valsComplete)
                && dim > 2
                && emitDirection(fx, rp, traits, dir))
            {
                /* A declared lens wins. Without one, a fixture that cannot
                   move is a wash, not a spot: 40 degrees reads as a spread of
                   light rather than the 14-degree shaft a mover falls back to. */
                const double declared = fixtureBeamAngle(fx, vals, traits);
                const double deg = (declared > 0.0 && declared < 180.0) ? declared : 40.0;
                const QVector3D apex = fixtureCentre(fid);
                const double half = qDegreesToRadians(deg / 2.0);
                drawBeamCone(p, apex, dir, half, live, dim / 255.0, 1.0);
                m_frameBeams << FrameBeam{ apex, dir.normalized(), half,
                                           beamThrow(apex, dir.normalized()),
                                           live, dim / 255.0 };
            }
        }

        /* Pixels on the face pointing at us -- when a grid is declared AND the
           pixels are big enough to see.
         *
         * A 64-pixel Step Row is one fixture but sixty-four primitives, and
         * this rig has ninety-six of them: about 6000 of the frame's ~7800 ops
         * were dots under 2 px across. Culling those costs nothing visually --
         * at that size they merge into the body anyway -- and hands back most
         * of the frame time. */
        /* A declared grid wins; otherwise a single row of however many heads
           there are, which is what a strip actually is. The elevation path has
           always fallen back that way -- requiring a declared layout HERE left
           every multi-head bar whose definition omits <Layout> (the wall
           washers, for instance) as a blank dark housing in the angled view
           while the same fixture showed its heads in the flat views. */
        const int cols = traits.layout.isValid() ? traits.layout.width()
                                                 : qMax(1, traits.headCount);
        const int rows = traits.layout.isValid() ? traits.layout.height() : 1;
        const double pxPerPixel = QLineF(w2s(corner[0]), w2s(corner[1])).length()
                                  / qMax(1, cols);
        if (traits.headCount > 1)
        {
            /* Which of the two long faces points AT us -- a question about the
               fixture's NORMAL and nothing else.
             *
               This used to compare the (+length,+normal) corner against the
               (-length,-normal) one, mixing the length axis into the answer, so
               the choice flipped with the SIGN of the azimuth: right from house
               right, wrong from house left. viewDepth() is a pure linear form,
               so applying it to the edge vector between the two faces asks
               exactly "does the +normal side face the eye" and nothing more. */
            const bool nearSide = viewDepth(corner[2] - corner[1]) > 0.0;
            const QVector3D &f0 = nearSide ? corner[7] : corner[4];
            const QVector3D &f1 = nearSide ? corner[6] : corner[5];
            const QVector3D &b0 = nearSide ? corner[3] : corner[0];
            const QVector3D &b1 = nearSide ? corner[2] : corner[1];

            /* The pixels are PAINTED ON that face, so they have to sort WITH
               it, not against it.
             *
             * drawSolidBox() gives a face one depth -- the average of its four
             * corners -- while each dot carried its own. Off a square-on view a
             * strip's face spans a real depth range, so every dot beyond the
             * face's midpoint sorted BEHIND the housing it sits on and was
             * painted over: half of every step went blank, and the halfway line
             * slid across as the camera came round. Sharing the face's depth
             * (plus a hair, to land just in front of it and to avoid ties in an
             * unstable sort) makes a pixel and the metal it is mounted in one
             * surface, which is what they are. */
            const double faceDepth =
                (viewDepth(f0) + viewDepth(f1) + viewDepth(b0) + viewDepth(b1)) / 4.0
                + 1e-4 + depthBias;
            if (pxPerPixel >= 2.5)
            {
                /* Batched by colour. Every pixel of a strip shares one depth
                   (they are one surface), so they never sort against each
                   other -- and this rig has ninety-six 64-pixel strips, which
                   as one op each was six thousand queue entries and six
                   thousand painter state changes per frame. A solid look
                   collapses to a single op; even a confetti pattern only has a
                   handful of distinct colours. */
                QHash<QRgb, QVector<QPointF> > byColour;
                QHash<QRgb, QVector<double> > byColourZ;
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
                        const QColor pxc =
                            pixelColor(fx, vals, placed, col, unlit, visibility, master).lighter(135);
                        byColour[pxc.rgba()].append(w2s(at));
                        /* Each pixel's OWN depth, plus a millimetre.
                         *
                           The millimetre is a real offset, not an ordering
                           fudge: an LED does sit slightly proud of the housing
                           it is set into, and something exactly coplanar with a
                           surface has no defined winner. That is different in
                           kind from the layering epsilons this replaces, which
                           had to grow to half the width of a step because they
                           were compensating for a depth the renderer could not
                           represent. */
                        byColourZ[pxc.rgba()].append(viewDepth(at) + 0.001 + depthBias);
                    }
                }
                for (QHash<QRgb, QVector<QPointF> >::const_iterator it = byColour.constBegin();
                     it != byColour.constEnd(); ++it)
                    emitDots(it.value(), faceDepth, 1.2, QColor::fromRgba(it.key()), p,
                             byColourZ.value(it.key()));
            }
            else
            {
                /* Too small to resolve one pixel from the next -- but DROPPING
                   them, which is what culling did, turns a lit strip into a
                   dead grey bar. At a whole-rig zoom a 64-pixel step row is
                   about two screen pixels per LED, so this is the normal case,
                   not an edge case: the step fronts simply went dark until you
                   zoomed in.
                 *
                   Paint the emitting face as one band in the MEAN of the head
                   colours instead. One primitive rather than sixty-four, so the
                   frame budget the cull was protecting is still protected, and
                   at this size the pixels merge into exactly this band anyway.
                   The mean, note -- not the fixture-wide per-primary maximum,
                   which washes any mixed pattern out to near-white. */
                double sr = 0.0, sg = 0.0, sb = 0.0;
                int n = 0;
                const int step = qMax(1, traits.headCount / 16);
                for (int h = 0; h < traits.headCount; h += step)
                {
                    const QColor pxc = pixelColor(fx, vals, h, col, unlit, visibility, master);
                    sr += pxc.red(); sg += pxc.green(); sb += pxc.blue();
                    ++n;
                }
                if (n > 0)
                {
                    /* The band lies on the face, so it carries the face's own
                       depth at each corner plus the same millimetre the
                       individual pixels get. A flat depth here put half of it
                       back inside the housing at wide angles -- the merged path
                       has to be as depth-correct as the resolved one. */
                    QPolygonF band;
                    band << w2s(b0) << w2s(b1) << w2s(f1) << w2s(f0);
                    QVector<double> bandZ;
                    bandZ << viewDepth(b0) + 0.001 + depthBias
                          << viewDepth(b1) + 0.001 + depthBias
                          << viewDepth(f1) + 0.001 + depthBias
                          << viewDepth(f0) + 0.001 + depthBias;
                    emitPoly(band, faceDepth,
                             QColor(qRound(sr / n), qRound(sg / n), qRound(sb / n)),
                             QColor(), 0.0, p, bandZ);
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
            // "fixture set as underhung .. visually upright": this is the
            // generic (non-tower) Mover elevation, drawn for anything mounted
            // on a truss/pipe or free-placed. The tower-shelf branch above
            // already reads mountingType to flip the silhouette (base at the
            // top of the rect vs the bottom -- see addMoverUnit()); this one
            // hardcoded `false`, so a fixture set to Underhung on a bar
            // rendered identically to one set to On-top in every flat
            // (Front/Side) view.
            const bool hung = (rp.mountingType == Truss::TopHung);
            const QPainterPath body = moverElevationPath(
                QRectF(c.x() - halfW, c.y() - totalH * 0.5, halfW * 2, totalH), hung, units);
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

        /* The body's true SILHOUETTE: the convex hull of its eight corners as
           projected into this view. boxPx is the axis-aligned box that CONTAINS
           that silhouette -- right for hit-testing, wrong for drawing, because a
           fixture rolled about its length (studioTilt) then drew as a slightly
           larger upright rectangle and the roll was invisible in every flat
           view ("I don't see the tilt here .. how would I know?"). An unrolled
           fixture's hull IS its box, so nothing that was already right moves. */
        QPolygonF silhouette;
        {
            QVector3D corner[8];
            fixtureBoxCorners(fid, traits, corner);
            QVector<HullPt> pts;
            for (int k = 0; k < 8; ++k)
                pts << HullPt{ w2s(corner[k]), 0.0 };
            foreach (const HullPt &hp, convexHullWith(pts))
                silhouette << hp.p;
        }
        if (silhouette.size() < 3)
            silhouette = QPolygonF(boxPx);

        if (hi)
        {
            // Outline the silhouette itself, offset outward from its centre.
            const QPointF mid = silhouette.boundingRect().center();
            QPolygonF halo;
            foreach (const QPointF &q, silhouette)
            {
                QPointF d = q - mid;
                const double len = qSqrt(d.x() * d.x() + d.y() * d.y());
                halo << (len > 1e-6 ? q + d * (3.0 / len) : q);
            }
            p.setPen(QPen(QColor(120, 220, 140, 160), 3));
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(halo);
        }

        /* The body. A pixel bar's HEADS are the light; the box around them is
           housing, so it stays neutral and lets the pixels say everything --
           the same division the studio makes. A single-head fixture is
           different: there the box IS the lamp, so it keeps its colour. */
        const bool pixelBar = (traits.headCount > 1);
        const QColor body = pixelBar ? bodyShade() : col.darker(230);
        const QColor rim = (drag || hi) ? col
                         : pixelBar ? bodyShade().lighter(135) : col.darker(150);
        p.setPen(QPen(rim, (drag || hi) ? 1.8 : 1.0));
        p.setBrush(body);
        p.drawPolygon(silhouette);

        /* The heads. A declared grid wins; otherwise a single row of however
           many heads there are, which is what a strip actually is. */
        const int cols = traits.layout.isValid() ? traits.layout.width()
                                                 : qMax(1, traits.headCount);
        const int rows = traits.layout.isValid() ? traits.layout.height() : 1;
        const double cellW = boxPx.width() / qMax(1, cols);
        const double cellH = boxPx.height() / qMax(1, rows);
        const double rad = qBound(0.7, qMin(cellW, cellH) * 0.40, 3.0);
        p.setPen(Qt::NoPen);
        int placed = 0;
        for (int r = 0; r < rows && placed < traits.headCount; ++r)
        {
            const double fy = (rows > 1) ? (double(r) / (rows - 1) - 0.5) : 0.0;
            for (int cx = 0; cx < cols && placed < traits.headCount; ++cx, ++placed)
            {
                const double fx2 = (cols > 1) ? (double(cx) / (cols - 1) - 0.5) : 0.0;
                p.setBrush(pixelColor(fx, vals, placed, col, unlit, visibility, master));
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

QList<quint32> StructureStudioView::fixturesInDrawOrder() const
{
    MonitorProperties *props = m_doc->monitorProperties();

    /* Depth toward the eye of a flat view, larger = nearer: Front looks
       upstage from downstage (+Y), Side looks from stage right (-X), Top looks
       down (+Z). The same conventions the angled projection degenerates to. */
    auto depthOf = [this](const QVector3D &w) -> double {
        switch (m_plane)
        {
        case Front: return double(w.y());
        case Side:  return -double(w.x());
        case Top:   return double(w.z());
        default:    return viewDepth(w);
        }
    };
    auto hostOf = [](const FixtureRigProps &rp, quint32 fid) -> QString {
        if (rp.trussId != Truss::invalidId())  return QStringLiteral("truss:%1").arg(rp.trussId);
        if (rp.towerId != Tower::invalidId())  return QStringLiteral("tower:%1").arg(rp.towerId);
        if (rp.pipeId != Pipe::invalidId())    return QStringLiteral("pipe:%1").arg(rp.pipeId);
        if (rp.onRiser()) return QStringLiteral("platform:%1").arg(rp.riserPlatformId);
        if (rp.onDeck())  return QStringLiteral("platform:%1").arg(rp.deckPlatformId);
        return QStringLiteral("fixture:%1").arg(fid);
    };

    struct Entry { quint32 fid; QString host; int layer; double depth; };
    QVector<Entry> entries;
    QHash<QString, QPair<double, int> > hostDepth;   // summed depth, count
    foreach (quint32 fid, mountedFixtures())
    {
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        const Entry e{ fid, hostOf(rp, fid),
                       (rp.placement == FixtureRigProps::Inside) ? 0 : 1,
                       depthOf(fixtureCentre(fid)) };
        entries << e;
        QPair<double, int> &hd = hostDepth[e.host];
        hd.first += e.depth;
        hd.second += 1;
    }

    std::stable_sort(entries.begin(), entries.end(), [&](const Entry &a, const Entry &b) {
        if (a.host != b.host)
        {
            const QPair<double, int> ha = hostDepth.value(a.host);
            const QPair<double, int> hb = hostDepth.value(b.host);
            const double da = ha.first / qMax(1, ha.second);
            const double db = hb.first / qMax(1, hb.second);
            if (da != db)
                return da < db;
            return a.host < b.host;
        }
        if (a.layer != b.layer)
            return a.layer < b.layer;
        return a.depth < b.depth;
    });

    QList<quint32> out;
    out.reserve(entries.size());
    foreach (const Entry &e, entries)
        out << e.fid;
    return out;
}

void StructureStudioView::drawFixtures(QPainter &p) const
{
    p.setFont(QFont("Arial", 8));
    /* Names are useful when a handful of fixtures are on one structure; across
       the WHOLE rig they are a wall of overlapping text that hides the thing
       you opened the overview to look at. Show them only for what is selected
       there. */
    const bool nameEveryone = (m_kind != StageKind);
    foreach (quint32 fid, fixturesInDrawOrder())
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
            rp.setStudioMount(mount);
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
    /* setStudioMount(), not the member: on a RISER the face is what the
       position is derived from, so writing the drawing plane alone would leave
       the strip drawn on one surface of the step and placed on another. */
    rp.setStudioMount(face);
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
        rp.setStudioMount(mount);
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
    // Screen-space min/max only means a true physical width/height in a
    // plane that isn't foreshortening anything -- same reason drawRulers()
    // excludes Angled.
    if (m_plane == Angled)
        return;

    MonitorProperties *props = m_doc->monitorProperties();
    QList<QVector3D> c;   // structure-only corners (no fixtures)

    /* "when selecting a feature on the bar.truss editor .. should show the
       fixture width on the horizontal .. and the fixture height on the
       vertical like it does on the studio editor." A selected fixture takes
       over these callouts entirely -- its own real footprint (the same box
       fixtureBoxCorners() builds for the Angled 3D body) is what's actually
       useful once something specific is picked, not the whole structure it
       happens to be sitting on. */
    if (!m_highlight.isEmpty())
    {
        const quint32 fid = *m_highlight.begin();
        Fixture *fx = m_doc->fixture(fid);
        if (fx != nullptr)
        {
            const FixtureVisualTraits traits = classifyFixture(fx);
            QVector3D corner[8];
            fixtureBoxCorners(fid, traits, corner);
            for (int i = 0; i < 8; ++i) c << corner[i];
        }
    }
    else if (m_kind == PlatformKind)
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

    // The bracket is drawn "just below the structure" -- but a fixture
    // mounted on it (icon + name label, drawn earlier in paintEvent()) can
    // reach further down on screen than the structure's own bbox, and the
    // fixed 18px gap below used to land the bracket line and its "N.N ft"
    // text right through the fixture and its label. Clear whichever
    // mounted fixture reaches furthest down too, not just the structure
    // itself -- budgeting for the icon's own radius plus its name label's
    // line below it, rather than re-deriving drawOneFixture()'s exact
    // (much more elaborate) drawn geometry here. Only affects WHERE the
    // line is drawn, not what it measures -- the tick marks still span
    // exactly the structure's own sMinX/sMaxX width.
    double fixtureClearY = sMaxY;
    foreach (quint32 fid, mountedFixtures())
        fixtureClearY = qMax(fixtureClearY, w2s(fixtureCentre(fid)).y() + 22.0);

    p.setPen(QPen(QColor(150, 160, 180, 200), 1.0));
    p.setFont(QFont("Arial", 8));
    // Horizontal dimension, just below the structure (and any fixture on it).
    if (wSpan > 0.01)
    {
        const double y = fixtureClearY + 18.0;
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
    {
        // "when selecing the bar .. should just show the bar on the rulers":
        // this editor's OWN object (m_id) is what the ruler describes by
        // default, but with a bar selected on the canvas the ruler was still
        // silently measuring the PARENT underneath it, so the centre line
        // and the width numbers described a structure that wasn't even the
        // one highlighted. Selecting a bar clears back to no selection
        // (mousePressEvent's empty-canvas/truss-body click), so "a bar is
        // selected" and "the truss itself is" are the only two states here.
        const quint32 rulerTrussId = (m_selectedBarId != Truss::invalidId())
                                     ? m_selectedBarId : m_id;
        if (Truss *t = props->truss(rulerTrussId)) c << t->origin() << t->positionAt(t->length());
    }
    else if (m_kind == PipeKind)
    { if (Pipe *pp = props->pipe(m_id)) c << pp->positionAt(0) << pp->positionAt(pp->length()); }
    else if (m_kind == GroupKind)
    { foreach (quint32 fid, mountedFixtures()) c << fixtureCentre(fid);
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
    else if (m_kind == TrussKind)
    {
        // Same "whichever is actually selected" rule as structureCentreA().
        const quint32 rulerTrussId = (m_selectedBarId != Truss::invalidId())
                                     ? m_selectedBarId : m_id;
        if (Truss *t = props->truss(rulerTrussId))
            z = qMax(double(t->origin().z()), double(t->positionAt(t->length()).z()));
    }
    else if (m_kind == PipeKind)    { if (Pipe *pp = props->pipe(m_id))
        z = qMax(double(pp->positionAt(0).z()), double(pp->positionAt(pp->length()).z())); }
    else if (m_kind == GroupKind)
    { foreach (quint32 fid, mountedFixtures()) z = qMax(z, double(fixtureCentre(fid).z())); }
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
        const QPointF s = w2s(fixtureCentre(*m_highlight.begin()));
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

    /* The LEFT label sits vertical-centred right next to the height ruler's
       own "max H ft" callout (drawRulers(), same corner, same GW+.. x) --
       when the view is framed tight around one short structure (a per-bar
       editor is exactly this: the whole visible Z range is a couple of
       feet), the structure's top and the view's vertical centre land
       within a few pixels of each other and the two texts print on top of
       one another ("scale isn't right" -- what actually made it unreadable
       was this, not the numbers themselves). Nudge the left label clear of
       that line when they would collide; nothing else in this frame sits
       in that corner, so nothing else needs to move. */
    QRectF leftArea = area;
    if (m_plane != Top)
    {
        const double topZ = structureTopZ();
        if (topZ > 0.01)
        {
            const double maxY = w2s(axisWorldPoint(!(m_rotation & 1), topZ)).y();
            if (qAbs(area.center().y() - maxY) < 16.0)
                leftArea.translate(0.0, 16.0);
        }
    }
    p.drawText(leftArea, Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("◀ ") + leftLbl);
    p.drawText(area, Qt::AlignVCenter | Qt::AlignRight, rightLbl + QStringLiteral(" ▶"));
    p.drawText(area, Qt::AlignHCenter | Qt::AlignTop,    QStringLiteral("▲ ") + topLbl);
    p.drawText(area, Qt::AlignHCenter | Qt::AlignBottom, QStringLiteral("▼ ") + bottomLbl);
}

/* The values this frame is drawn from.
 *
 * Falls back to reading the fixture directly if the snapshot has no entry --
 * a fixture added between the snapshot and the draw, say. */
/* The values this fixture is drawn from, and in what order of preference.
 *
 * LIVE OUTPUT WINS, when there is any. The scene resolution exists for design
 * with nothing running: select a look, see what it does. But while live preview
 * is on, the DMX IS that look, updating as it is edited -- and a cached
 * resolution sitting in front of it showed the colour the look had when it was
 * SELECTED, not the one being dragged around the picker. Editing a palette does
 * not announce itself as a scene change, so the cache had no way to know.
 *
 * "Any" means any non-zero channel. All zeros is not "this fixture is off", it
 * is "nothing is driving this fixture", and the two are indistinguishable from
 * the values alone -- so an undriven rig falls through to the selected look
 * rather than reading as a blackout.
 */
QByteArray StructureStudioView::valuesForDrawing(Fixture *fx, bool *complete) const
{
    if (complete != nullptr)
        *complete = true;
    if (fx == nullptr)
        return QByteArray();

    if (m_liveValues)
    {
        const QByteArray live = liveValuesFor(fx);
        for (int i = 0; i < live.size(); ++i)
            if (live.at(i) != 0)
                return live;          // the whole universe: it means what it says
    }
    /* A look resolved from palettes only fills in the channels it SETS. The
       rest are zero because nothing wrote them, not because the look asked for
       zero -- which matters for any channel whose zero is meaningful, a shutter
       above all. */
    if (complete != nullptr)
        *complete = false;
    return m_sceneValues.value(fx->id());
}

QByteArray StructureStudioView::liveValuesFor(Fixture *fx) const
{
    if (fx == nullptr)
        return QByteArray();
    const QHash<quint32, QByteArray>::const_iterator it = m_liveSnapshot.constFind(fx->id());
    return (it != m_liveSnapshot.constEnd()) ? it.value() : fx->channelValues();
}

/* One frame, one instant.
 *
 * A paint walks a hundred-odd fixtures and used to ask each one for its values
 * as it reached it, while the engine kept writing them from the MasterTimer
 * thread. Fixtures drawn early in the frame therefore showed an older moment
 * than fixtures drawn late, and the boundary moved every frame -- the rig
 * appeared to flicker in and out ACROSS the steps rather than showing one
 * coherent picture that changes.
 *
 * QByteArray is copy-on-write, so taking the whole rig's values up front costs
 * a refcount per fixture and gives every frame a single consistent instant. */
void StructureStudioView::takeLiveSnapshot() const
{
    m_liveSnapshot.clear();
    if (m_doc == nullptr)
        return;
    foreach (Fixture *fx, m_doc->fixtures())
        if (fx != nullptr)
            m_liveSnapshot.insert(fx->id(), fx->channelValues());
}

/* Which fixtures physically overlap another fixture's body -- a rough,
 * axis-aligned check, not a true oriented-box intersection: the horizontal
 * half-extent is the LARGER of the fixture's declared width and depth, used
 * for both X and Y, so a truss-mounted bar running across the rig at some
 * arbitrary angle is never under-counted just because its width and depth
 * were declared for a different orientation than it happens to be rigged
 * in. Conservative (can over-warn on a long fixture end-on to another) is
 * the right side to err on for a warning, unlike a hard constraint.
 *
 * O(fixtures^2), but that is a few hundred simple box comparisons, not the
 * per-fixture palette resolution the rest of this view has to be careful
 * about -- cheap enough to redo every frame rather than invalidate on
 * whatever subset of rig-geometry changes might move a fixture. */
void StructureStudioView::rebuildOverlapWarnings() const
{
    m_overlappingFixtures.clear();
    if (m_doc == nullptr)
        return;

    struct Box { quint32 fid; QVector3D lo, hi; };
    QVector<Box> boxes;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr)
            continue;
        const FixtureVisualTraits traits = classifyFixture(fx);
        if (traits.physW <= 0.0f && traits.physH <= 0.0f && traits.physD <= 0.0f)
            continue;   // nothing declared to compare against
        const double horiz = qMax(0.03, qMax(double(traits.physW), double(traits.physD)) * 0.5);
        const double vert  = qMax(0.03, double(traits.physH) * 0.5);
        const QVector3D c = fixtureCentre(fx->id());
        Box b;
        b.fid = fx->id();
        b.lo = QVector3D(c.x() - float(horiz), c.y() - float(horiz), c.z() - float(vert));
        b.hi = QVector3D(c.x() + float(horiz), c.y() + float(horiz), c.z() + float(vert));
        boxes << b;
    }

    /* A small tolerance, not zero: two bars butted end to end (or side by
     * side, the ordinary way to build a continuous run) touch at a shared
     * face. That is not "in the same space" -- only genuine penetration
     * past the touching faces is. */
    const float eps = 0.01f;
    for (int i = 0; i < boxes.size(); ++i)
    {
        for (int j = i + 1; j < boxes.size(); ++j)
        {
            const Box &a = boxes.at(i);
            const Box &b = boxes.at(j);
            const bool overlap =
                   a.lo.x() + eps < b.hi.x() && b.lo.x() + eps < a.hi.x()
                && a.lo.y() + eps < b.hi.y() && b.lo.y() + eps < a.hi.y()
                && a.lo.z() + eps < b.hi.z() && b.lo.z() + eps < a.hi.z();
            if (overlap)
            {
                m_overlappingFixtures.insert(a.fid);
                m_overlappingFixtures.insert(b.fid);
            }
        }
    }
}

void StructureStudioView::paintEvent(QPaintEvent *)
{
    QElapsedTimer frameTimer;
    if (m_liveValues)
    {
        frameTimer.start();
        takeLiveSnapshot();
    }
    else
        m_liveSnapshot.clear();
    rebuildOverlapWarnings();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    drawGrid(p);
    drawRulers(p);
    if (m_plane == Angled)
    {
        /* Every angled view, not just the whole-rig one. The editors used to
           paint their 45-degree view with the old unsorted path, which is why a
           SOLID deck showed its own far edges through itself: nothing was
           deciding which face was in front. */
        drawRigDepthSorted(p);      // structures and fixtures interleaved
    }
    else
    {
        drawStructure(p);
        drawFixtures(p);
        // Bars hung on THIS truss, drawn last so painter's-algorithm order
        // never buries a selectable, draggable bar behind a fixture icon
        // that happens to overlap it on screen. StageKind's flat view needs
        // no equivalent call: everyStructure() already lists every bar as
        // its own top-level entry there, drawn (and thus hidden behind
        // fixtures the same as any other structure) by drawStructure() above
        // -- unchanged, existing behaviour, not this report's complaint.
        if (m_kind == TrussKind)
            drawChildBars(p, m_id);
    }
    drawCursorReadout(p);
    drawDimensions(p);   // selected fixture's own W/H, or the structure's

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

    /* Never ask for frames faster than we can draw them.
     *
     * A fixed 25 Hz was optimistic: measured on a 134-fixture rig the angled
     * overview took 59 ms a frame, so the timer queued repaints faster than
     * they completed and the backlog showed up as the picture stuttering --
     * reported as "just sort of blinking". Culling sub-pixel LED dots brought
     * that to ~34 ms, but a bigger rig or a slower machine would put it back.
     * Re-tune from what a frame ACTUALLY costs here (smoothed, so one slow
     * frame does not lurch the rate), leaving roughly half the budget spare. */
    if (m_liveValues && m_liveTimer != nullptr && frameTimer.isValid())
    {
        const double ms = frameTimer.nsecsElapsed() / 1000000.0;
        m_frameMs = (m_frameMs <= 0.0) ? ms : (m_frameMs * 0.8 + ms * 0.2);
        /* At the chosen rate when frames are cheap enough to allow it, slower
           when they are not -- a quarter of the interval spare, so the event
           loop still breathes between repaints. */
        const int want = int(qBound(1000.0 / m_maxFps, m_frameMs * 1.25, 250.0));
        if (qAbs(m_liveTimer->interval() - want) > 8)
            m_liveTimer->setInterval(want);
    }
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

void StructureStudioView::zoomAt(const QPointF &anchor, double targetScale)
{
    const double next = qBound(4.0, targetScale, 4000.0);
    if (qFuzzyCompare(next, m_scale))
        return;
    const QPointF before = screenToPlane(anchor);
    m_scale = next;
    m_originPx += anchor - (m_originPx + planeToScreenVec(before));
    m_zoomed = true;          // refit() must not stomp a deliberate zoom
    update();
}

bool StructureStudioView::event(QEvent *e)
{
    /* Trackpad pinch: macOS delivers this as a native gesture, not a wheel
       event, and a QWidget (this is one, not a QGraphicsView) is handed
       nothing at all unless it asks -- there is no fallback translation to a
       wheelEvent() to silently catch it. Left unhandled, pinch zoom on this
       view worked only by accident, whichever way the platform happened to
       be routing it that day ("pinch to zoom .. isn't working consistently").
       Same zoomAt() anchor-at-cursor math as the scroll wheel below, driven
       by the gesture's own delta instead of a fixed step, so the two can
       never disagree with each other. */
    if (e->type() == QEvent::NativeGesture)
    {
        QNativeGestureEvent *g = static_cast<QNativeGestureEvent *>(e);
        if (g->gestureType() == Qt::ZoomNativeGesture)
        {
            // QNativeGestureEvent doesn't fit qlceventpos.h's shared template --
            // Qt5 spells the QPointF accessor localPos(), not posF() like the
            // event types that header already covers.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            zoomAt(g->position(), m_scale * (1.0 + g->value()));
#else
            zoomAt(g->localPos(), m_scale * (1.0 + g->value()));
#endif
            return true;
        }
    }
    return QWidget::event(e);
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
    zoomAt(anchor, m_scale * ((delta > 0) ? 1.12 : 1.0 / 1.12));
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
            // Nothing on this truss to click, drag, or even see, before this
            // was wired up ("not selectable at all with mouse to move it in
            // position") -- same press-to-select-and-arm-a-drag treatment as
            // a fixture, just onto m_dragBarId instead of m_dragFid.
            m_dragBarId = hitTestBar(e->pos());
            if (m_dragBarId != UINT_MAX)
            {
                setCursor((m_locked || m_plane == Angled
                           || m_doc->monitorProperties()->truss(m_dragBarId)->locked())
                              ? Qt::ArrowCursor : Qt::ClosedHandCursor);
                setBarHighlight(m_dragBarId);
                setPipeBarHighlight(Pipe::invalidId());
                setHighlight({});   // a bar and a fixture are never selected together
                emit barSelected(m_dragBarId);
                return;
            }

            // Same press-to-select-and-arm-a-drag treatment, for a crossbar
            // hung on a Pipe instead of a Truss (a stand's boom, or a boom
            // viewed directly).
            m_dragPipeBarId = hitTestPipeBar(e->pos());
            if (m_dragPipeBarId != Pipe::invalidId())
            {
                setCursor((m_locked || m_plane == Angled
                           || m_doc->monitorProperties()->pipe(m_dragPipeBarId)->locked())
                              ? Qt::ArrowCursor : Qt::ClosedHandCursor);
                setPipeBarHighlight(m_dragPipeBarId);
                setBarHighlight(UINT_MAX);
                setHighlight({});   // a bar and a fixture are never selected together
                emit pipeBarSelected(m_dragPipeBarId);
                return;
            }

            /* Neither a fixture nor a bar: the truss's own body, or genuinely
               empty space. Whatever was selected before is stale now and
               nothing was clearing it -- the highlight stayed on screen and
               the tree/inspector kept showing the old selection with no way
               to tell it was stale, which is what actually made "click the
               truss to select just the truss" read as needing "2 or 3
               clicks" (each click that also missed left the same leftover
               state, so only a click that happened to hit something NEW
               ever visibly changed anything). Clear and say so. */
            if (!m_highlight.isEmpty() || m_selectedBarId != UINT_MAX || m_selectedPipeBarId != Pipe::invalidId())
            {
                setHighlight({});
                setBarHighlight(UINT_MAX);
                setPipeBarHighlight(Pipe::invalidId());
                emit fixtureSelected(Fixture::invalidId());
            }

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
            setCursor((m_locked || m_plane == Angled || fixtureMovementLocked(m_dragFid))
                          ? Qt::ArrowCursor : Qt::ClosedHandCursor);
            setHighlight({ m_dragFid });
            setBarHighlight(UINT_MAX);   // a bar and a fixture are never selected together
            setPipeBarHighlight(Pipe::invalidId());
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
    if (m_dragFid != Fixture::invalidId() && !m_locked && !fixtureMovementLocked(m_dragFid))
    {
        if (!m_dragged)
            emit editAboutToStart();   // snapshot for undo before the first change
        if (dragFixtureTo(m_dragFid, e->pos()))
            m_dragged = true;
    }
    else if (m_dragBarId != UINT_MAX && !m_locked)
    {
        if (!m_dragged)
            emit editAboutToStart();
        if (dragBarTo(m_dragBarId, e->pos()))
            m_dragged = true;
    }
    else if (m_dragPipeBarId != Pipe::invalidId() && !m_locked)
    {
        if (!m_dragged)
            emit editAboutToStart();
        if (dragPipeBarTo(m_dragPipeBarId, e->pos()))
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
    else if (m_dragBarId != UINT_MAX)
    {
        setCursor(Qt::ArrowCursor);
        if (m_dragged)
        {
            m_doc->setModified();
            emit barMoved(m_dragBarId);
        }
        m_dragBarId = UINT_MAX;
        m_dragged = false;
    }
    else if (m_dragPipeBarId != Pipe::invalidId())
    {
        setCursor(Qt::ArrowCursor);
        if (m_dragged)
        {
            m_doc->setModified();
            emit pipeBarMoved(m_dragPipeBarId);
        }
        m_dragPipeBarId = Pipe::invalidId();
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
    foreach (quint32 fid, fixturesInDrawOrder())
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
                const QPointF c = w2s(fixtureCentre(fid));
                const double baseR = moverBaseRadius(traits);
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
                const QPointF c = w2s(fixtureCentre(fid));
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
        // <=, walking back to front: of two equally close, the one on top wins.
        if (d <= bestD) { bestD = d; best = fid; }
    }
    return best;
}

quint32 StructureStudioView::hitTestBar(const QPointF &px) const
{
    if (m_kind != TrussKind)
        return UINT_MAX;                 // == Truss::invalidId(); only a truss has bars
    MonitorProperties *props = m_doc->monitorProperties();
    quint32 best = UINT_MAX; double bestD = 1e9;
    foreach (Truss *bar, props->trusses())
    {
        if (bar == nullptr || bar->parentTrussId() != m_id)
            continue;
        // Same point-to-segment test as hitTestFixture()'s Bar/Generic case:
        // nearest point on the drawn run, not just its centre or an endpoint.
        const QPointF a = w2s(bar->origin());
        const QPointF b = w2s(bar->positionAt(bar->length()));
        const QPointF ab = b - a;
        const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
        double t = (len2 > 1e-6) ? ((px.x() - a.x()) * ab.x() + (px.y() - a.y()) * ab.y()) / len2 : 0.0;
        t = qBound(0.0, t, 1.0);
        const double d = QLineF(a + ab * t, px).length();
        /* The acceptance radius has to match how WIDE this bar actually
           draws (drawOneStructure()'s TrussKind flat-view branch, same
           wpx formula) -- a flat ~9 px missed almost every click inside a
           bar's own drawn rectangle whenever it rendered wider than that
           (any real truss profile at a normal zoom), which is exactly what
           made it look undraggable ("I canNOT move it along the truss").
           A point-to-CENTRELINE distance still has to clear half that width
           to land inside the rectangle at all. */
        const double halfWidthPx = qMax(6.0, double(bar->width()) * m_scale) / 2.0 + 3.0;
        if (d <= halfWidthPx && d < bestD) { bestD = d; best = bar->id(); }
    }
    return best;
}

quint32 StructureStudioView::hitTestPipeBar(const QPointF &px) const
{
    if (m_kind != StandKind && m_kind != PipeKind)
        return Pipe::invalidId();
    MonitorProperties *props = m_doc->monitorProperties();
    quint32 best = Pipe::invalidId(); double bestD = 1e9;
    foreach (Pipe *bar, props->pipes())
    {
        if (bar == nullptr || !bar->isBarOnPipe())
            continue;
        // Only crossbars belonging to what THIS editor is showing: a
        // stand's own booms (standPipes() already includes their
        // crossbars), or, if opened directly on a boom, that boom's own.
        bool belongsHere = false;
        if (m_kind == StandKind)
            belongsHere = standPipes(m_id).contains(bar);
        else
            belongsHere = (bar->parentPipeId() == m_id);
        if (!belongsHere)
            continue;

        // Same point-to-segment test as hitTestBar().
        const QPointF a = w2s(bar->positionAt(0.0f));
        const QPointF b = w2s(bar->positionAt(bar->length()));
        const QPointF ab = b - a;
        const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
        double t = (len2 > 1e-6) ? ((px.x() - a.x()) * ab.x() + (px.y() - a.y()) * ab.y()) / len2 : 0.0;
        t = qBound(0.0, t, 1.0);
        const double d = QLineF(a + ab * t, px).length();
        // Same acceptance-radius reasoning as hitTestBar(), using the pipe's
        // own physical diameter instead of a truss's width.
        const double halfWidthPx = qMax(6.0, double(bar->diameter()) * m_scale) / 2.0 + 3.0;
        if (d <= halfWidthPx && d < bestD) { bestD = d; best = bar->id(); }
    }
    return best;
}

void StructureStudioView::mouseDoubleClickEvent(QMouseEvent *e)
{
    const quint32 fid = hitTestFixture(e->pos());
    if (fid != Fixture::invalidId())
    {
        emit fixtureActivated(fid);
        return;
    }
    const quint32 barId = hitTestBar(e->pos());
    if (barId != UINT_MAX)
    {
        emit barActivated(barId);
        return;
    }
    const quint32 pipeBarId = hitTestPipeBar(e->pos());
    if (pipeBarId != Pipe::invalidId())
        emit pipeBarActivated(pipeBarId);
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

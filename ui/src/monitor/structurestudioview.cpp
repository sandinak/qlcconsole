/*
  Q Light Controller Plus
  structurestudioview.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QPainter>
#include <QMatrix4x4>
#include <QFontMetricsF>
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
#include <functional>
#include <QSettings>
#include <QThread>
#include <QSemaphore>
#include <QThreadPool>
#include <QDebug>
#include <cmath>

#include "structurestudioview.h"
#include "righeads.h"
#include "humanfigure.h"
#include "fixturevisualtraits.h"
#include "lanternpixels.h"
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
#include "rhiraster.h"
#include "aimsolver.h"
#include "apputil.h"
#include <QScopedValueRollback>
#include <array>
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
    // F1: the whole rig's guide section, or mounting for a structure's editor.
    setProperty("helpTopic", kind == StageKind ? QStringLiteral("lighting-studio.md#angled--the-free-3d-view")
                                               : QStringLiteral("lighting-studio.md#mounting-a-fixture"));
    setMinimumSize(360, 320);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);   // fixtures dragged in from the source tree
    m_lanternPixels = new LanternPixels(doc, this);
    connect(m_lanternPixels, &LanternPixels::changed, this, QOverload<>::of(&QWidget::update));

    {
        QSettings s;
        m_supersample = qBound(1, s.value(QStringLiteral("monitor/rig/supersample"), 2).toInt(), 4);
        m_renderThreads = qMax(0, s.value(QStringLiteral("monitor/rig/threads"), 0).toInt());
        m_maxFps = qBound(10, s.value(QStringLiteral("monitor/rig/maxfps"), 30).toInt(), 120);
        m_gpuWanted = s.value(QStringLiteral("monitor/rig/gpu"), true).toBool();
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
    // Fixture names are drawn beside them: a rename shows at once.
    connect(m_doc, &Doc::fixtureChanged, this, [this](quint32) { update(); });
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
    rigMayHaveMoved();                  // the rig has changed: measure it again
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

const StructureStudioView::Trig &StructureStudioView::angledTrig() const
{
    /* The camera's sines and cosines, worked out when it turns -- not for
       every point projected, which was several hundred thousand times a
       lit frame. */
    if (m_trig.az != m_azimuthDeg || m_trig.el != m_elevationDeg)
    {
        const double A = qDegreesToRadians(m_azimuthDeg);
        const double E = qDegreesToRadians(m_elevationDeg);
        m_trig = Trig{ m_azimuthDeg, m_elevationDeg, qCos(A), qSin(A), qCos(E), qSin(E) };
    }
    return m_trig;
}

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
        const Trig &t = angledTrig();
        const double cA = t.cA, sA = t.sA, cE = t.cE, sE = t.sE;
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

void StructureStudioView::setHaze(double level)
{
    const double v = qBound(0.0, level, 3.0);
    if (qFuzzyCompare(v + 1.0, m_haze + 1.0))
        return;
    m_haze = v;
    update();
}

double StructureStudioView::fixtureOutput(Fixture *fx, bool *known) const
{
    if (known != nullptr)
        *known = false;
    /* "are we gauging the luminosity based on the actual devices" -- we were
       not: a 10-lumen WS2812 step strip threw the light of a 180 W bar, and
       ninety-six of them washed the floor white. */
    if (fx == nullptr || fx->fixtureMode() == nullptr)
        return 1.0;
    const QLCPhysical &phys = fx->fixtureMode()->physical();
    static const double referenceLumens = 3500.0;     // a 100 W LED fixture
    static const double lumensPerWatt = 35.0;         // LED, after its optics
    if (known != nullptr)
        *known = phys.declaresOutput();
    if (phys.bulbLumens() > 0)
        return phys.bulbLumens() / referenceLumens;
    if (phys.powerConsumption() > 0)
        return phys.powerConsumption() * lumensPerWatt / referenceLumens;
    return 1.0;
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
            const QPointF loc = t->toLocal(ab);     // in the tower's own axes
            rp.towerU = qBound(0.0f, float(loc.x() - t->originX()), t->width());
            rp.towerV = qBound(0.0f, float(loc.y() - t->originY()), t->depth());
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
                const QPointF loc = pl->toLocal(ab);    // in the platform's own axes
                rp.riserU = qBound(0.0f, float(loc.x() - pl->originX()), pl->width());
                rp.riserV = qBound(0.0f, float(loc.y() - pl->originY()), pl->depth());
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
                rp.riserU = qBound(0.0f, float(pl->toLocal(ab).x() - pl->originX()), pl->width());
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
            // The tread it is over, on steps; the deck, on a platform.
            const QVector3D at = fixtureCentre(fid);
            const float top = pl->topAt(at.x(), at.y());
            if (rp.placement == FixtureRigProps::Inside)
                rp.deckHeightOffset = qBound(0.0f, float(ab.y()) - base, top);
            else
                rp.deckHeightOffset = qMax(0.0f, float(ab.y()) - (base + top));
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
/* The whole-rig view shows what the Lighting Studio shows: a layer hidden
   there ("follow the viewability of the studio") is hidden here too -- not
   drawn, not framed, and not in the way of a beam. An editor shows its own
   object whatever layer it is on. */
bool StructureStudioView::layerShown(quint32 layerId) const
{
    return m_kind != StageKind || m_doc->monitorProperties()->layer(layerId).visible;
}

bool StructureStudioView::fixtureShown(quint32 fid) const
{
    return layerShown(m_doc->monitorProperties()->fixtureLayer(fid));
}

QList<QPair<StructureStudioView::Kind, quint32> > StructureStudioView::everyStructure() const
{
    MonitorProperties *props = m_doc->monitorProperties();
    QList<QPair<Kind, quint32> > out;
    foreach (Truss *t, props->trusses())
        if (t != nullptr && layerShown(t->layerId())) out << qMakePair(TrussKind, t->id());
    foreach (StagePlatform *pl, props->platforms())
        if (pl != nullptr && layerShown(pl->layerId())) out << qMakePair(PlatformKind, pl->id());
    foreach (Tower *tw, props->towers())
        if (tw != nullptr && layerShown(tw->layerId())) out << qMakePair(TowerKind, tw->id());
    foreach (Stand *st, props->stands())
        if (st != nullptr && layerShown(st->layerId())) out << qMakePair(StandKind, st->id());
    foreach (Pipe *pp, props->pipes())
        if (pp != nullptr && !pp->isBarOnPipe() && layerShown(pp->layerId()))
            out << qMakePair(PipeKind, pp->id());
    foreach (Scenery *sc, props->sceneries())
        if (sc != nullptr && layerShown(sc->layerId())) out << qMakePair(SceneryKind, sc->id());
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
    m_beamBounds.clear();
    m_collecting = true;
    /* Structures first, as they come (they paint some of themselves straight
       away), then the fixtures -- most of a lit frame -- across the cores. */
    QVector<quint32> fids;
    for (const Item &it : std::as_const(items))
    {
        if (it.isFixture)
            fids << it.id;
        else
            drawOneStructure(p, it.kind, it.id);
    }
    drawFixturesInParallel(p, fids, nameEveryone);
    collectInsideLight(p);
    drawSocks(p);
    drawTowerFaces(p);
    if (m_kind == StageKind)
        drawStageFigures(p);
    m_collecting = false;
    flushOps(p);
}

thread_local StructureStudioView::EmitContext *StructureStudioView::s_emit = nullptr;

void StructureStudioView::drawFixturesInParallel(QPainter &p, const QVector<quint32> &fids,
                                                 bool nameEveryone) const
{
    int threads = (m_renderThreads > 0) ? m_renderThreads : QThread::idealThreadCount();
    threads = qMin(threads, int(fids.size() / 8));      // a few: not worth the hand-off
    if (threads <= 1)
    {
        for (quint32 fid : fids)
            drawOneFixture(p, fid, nameEveryone);
        return;
    }

    /* Runs of fixtures, each gathered into a context of its own, then put back
       together in this order. A lantern or omni light is a run of one, drawn
       here on the GUI thread while the others are drawn on the pool. */
    struct Run { int from, to; bool here; };
    QVector<Run> runs;
    const int perRun = qMax(4, int(fids.size() / (threads * 4)));
    for (int i = 0; i < fids.size(); )
    {
        Fixture *fx = m_doc->fixture(fids.at(i));
        bool here = false;
        if (fx != nullptr)
        {
            const FixtureVisualTraits traits = classifyFixture(fx);
            here = (traits.kind == FixtureSilhouette::Lantern || traits.omni);
        }
        if (here)
        {
            runs << Run{ i, i + 1, true };
            ++i;
            continue;
        }
        if (runs.isEmpty() || runs.last().here || runs.last().to - runs.last().from >= perRun)
            runs << Run{ i, i + 1, false };
        else
            runs.last().to = i + 1;
        ++i;
    }

    opticsTime();                       // starts its clock: read-only from here
    QVector<EmitContext> ctx(runs.size());
    if (m_renderPool == nullptr)
        m_renderPool = new QThreadPool(const_cast<StructureStudioView *>(this));
    if (m_renderPool->maxThreadCount() < threads)
        m_renderPool->setMaxThreadCount(threads);

    QSemaphore done;
    int started = 0;
    for (int r = 0; r < runs.size(); ++r)
    {
        if (runs.at(r).here)
            continue;
        ++started;
        m_renderPool->start([&, r]() {
            s_emit = &ctx[r];
            for (int i = runs.at(r).from; i < runs.at(r).to; ++i)
                drawOneFixture(p, fids.at(i), nameEveryone);
            s_emit = nullptr;
            done.release();
        });
    }
    for (int r = 0; r < runs.size(); ++r)
    {
        if (!runs.at(r).here)
            continue;
        s_emit = &ctx[r];
        drawOneFixture(p, fids.at(runs.at(r).from), nameEveryone);
        s_emit = nullptr;
    }
    done.acquire(started);

    /* Back together. What was used of the kept landings and meshes is marked
       before anything new goes in: an insert can move what the marks point
       at. */
    for (const EmitContext &c : std::as_const(ctx))
    {
        for (const Throw *t : c.throwHits)
            if (t->frame != m_throwFrame)
            {
                const_cast<Throw *>(t)->frame = m_throwFrame;
                ++m_throwsUsed;
            }
        for (const BeamMesh *m : c.meshHits)
            const_cast<BeamMesh *>(m)->frame = m_throwFrame;
    }
    for (EmitContext &c : ctx)
    {
        m_ops.append(std::move(c.ops));
        m_frameBeams.append(c.beams);
        if (c.opticsAnimating)
            m_opticsAnimating = true;
        for (const auto &t : std::as_const(c.newThrows))
        {
            m_throws.insert(t.first, t.second);
            ++m_throwsUsed;
        }
        for (const auto &m : std::as_const(c.newMeshes))
            m_beamMeshes.insert(m.first, m.second);
    }
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
        if (t == nullptr || !layerShown(t->layerId()))
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
            arrivingLuma += fb.level * beamCoverage(fb, chest) * luma * lightFactor(fb);
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
            const QVector3D lit = beamLightAt(at, n) * float(exposure);
            r += lit.x(); g += lit.y(); b += lit.z();
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

void StructureStudioView::rigMayHaveMoved()
{
    m_floorPointsAge.invalidate();
    m_overlapAge.invalidate();
}

const QList<QVector3D> &StructureStudioView::floorPoints() const
{
    if (!m_floorPointsAge.isValid() || m_floorPointsAge.elapsed() > 500)
    {
        m_floorPoints.clear();
        collectPoints(m_floorPoints);
        m_floorPointsAge.start();
    }
    return m_floorPoints;
}

void StructureStudioView::collectPoints(QList<QVector3D> &pts) const
{
    if (m_kind != StageKind)
    {
        collectPointsFor(pts, m_kind, m_id);
        foreach (quint32 fid, mountedFixtures())
            pts << fixtureCentre(fid);
        return;
    }
    typedef QPair<Kind, quint32> KindId;
    foreach (const KindId &ki, everyStructure())
        collectPointsFor(pts, ki.first, ki.second);
    // Free-standing fixtures are part of the rig too, so the fit must frame them.
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (quint32 fid, mountedFixtures())
        pts << fixtureCentre(fid);
    /* And the people standing on it, feet to crown: a figure is drawn, so a
       fit that ignores it cuts off the head of anyone on a deck. */
    foreach (StageTarget *t, props->stageTargets())
    {
        if (t == nullptr || !layerShown(t->layerId()))
            continue;
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
    // The whole rig: everything that has a place on the plot, and is shown.
    QList<quint32> out;
    foreach (quint32 fid, m_doc->monitorProperties()->fixtureItemsID())
        if (fixtureShown(fid))
            out << fid;
    return out;
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
        else if (kind == TowerKind || kind == TrussKind)
        {
            on = (kind == TowerKind) ? (rp.towerId == id) : (rp.trussId == id);
            // ...or hung on one of its hangers.
            if (!on && rp.onHanger())
                if (const Hanger *hg = props->hanger(rp.hangerId))
                    on = hg->parentId() == id
                         && hg->parentKind() == (kind == TowerKind ? Hanger::OnTower : Hanger::OnTruss);
        }
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
            foreach (const QPointF &q, t->corners())
                pts << QVector3D(float(q.x()), float(q.y()), 0.0f)
                    << QVector3D(float(q.x()), float(q.y()), t->height());
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
        {
            foreach (const QPointF &q, pl->footprint(8))
                pts << QVector3D(float(q.x()), float(q.y()), 0.0f)
                    << QVector3D(float(q.x()), float(q.y()), pl->height());
        }
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
    else if (kind == SceneryKind)
    {
        if (Scenery *sc = props->scenery(id))
        {
            const float lo = sc->bottom(), hi = sc->bottom() + sc->height();
            for (int i = 0; i <= 8; ++i)
                pts << sc->pointAt(-1.0 + i / 4.0, lo) << sc->pointAt(-1.0 + i / 4.0, hi);
        }
    }
    /* Only the structure's own points. Every fixture's centre was added here
       too, once for each structure the whole-rig view asked about: every
       fixture placed once a structure, every frame -- most of what drawing
       a big show cost. collectPoints() adds the fixtures, once. */
}

/* Where centre stage stands in the angled view: this far down the window. */
static const double kStageCentreDown = 0.6;

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

    /* The angled (3D) view stands on the floor at CENTRE STAGE ("we should
       always start with centering on the floor and center stage"): that
       point sits across the middle and a little below half way down -- a rig
       stands on the floor, so the room above it is where the rig is -- and
       stays there as the view turns, so the view swings about centre stage.
       It used to centre the rig's bounding box, which floated the floor half
       a truss-height below the middle and moved it with every turn. */
    if (m_standOnCentreStage && m_plane == Angled && !first && m_doc != nullptr
        && m_doc->monitorProperties() != nullptr && (m_rotation & 1) == 0)
    {
        const QPointF anchor = project(m_doc->monitorProperties()->stageCentre());
        const QPointF at(width() / 2.0, height() * kStageCentreDown);
        // Room on each side of the anchor, and how far the rig reaches into it.
        const double left = qMax(1e-6, anchor.x() - minA), right = qMax(1e-6, maxA - anchor.x());
        const double up = qMax(1e-6, maxB - anchor.y()), down = qMax(1e-6, anchor.y() - minB);
        const double roomX = qMax(1.0, width() / 2.0 - margin);
        const double roomUp = qMax(1.0, at.y() - margin);
        const double roomDown = qMax(1.0, height() - margin - at.y());
        m_scale = qBound(10.0, qMin(qMin(roomX / left, roomX / right),
                                    qMin(roomUp / up, roomDown / down)), 320.0);
        m_originPx = QPointF(0, 0);
        m_originPx = at - planeToScreenVec(anchor);
    }
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
        foreach (const QVector3D &w, floorPoints())
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
        QPolygonF edge;
        edge << w2s(QVector3D(float(x0), float(y0), 0)) << w2s(QVector3D(float(x1), float(y0), 0))
             << w2s(QVector3D(float(x1), float(y1), 0)) << w2s(QVector3D(float(x0), float(y1), 0));

        /* Drawn once and kept: the same floor every frame until the view or
           the rig's extent changes -- which its corners on screen say -- and
           anti-aliased lines across the whole canvas were a third of what a
           frame of a big show cost. */
        const qreal dpr = devicePixelRatioF();
        QString key = QString("%1x%2@%3/%4/").arg(width()).arg(height()).arg(dpr).arg(step);
        foreach (const QPointF &c, edge)
            key += QString("%1,%2;").arg(c.x(), 0, 'f', 2).arg(c.y(), 0, 'f', 2);
        if (key != m_floorKey || m_floorImage.isNull())
        {
            m_floorKey = key;
            m_floorImage = QImage(size() * dpr, QImage::Format_ARGB32_Premultiplied);
            m_floorImage.setDevicePixelRatio(dpr);
            m_floorImage.fill(Qt::transparent);
            QPainter fp(&m_floorImage);
            fp.setRenderHints(p.renderHints());
            fp.setPen(QPen(QColor(58, 62, 72), 1.0));
            for (double x = x0; x <= x1 + 1e-6; x += step)
                fp.drawLine(w2s(QVector3D(float(x), float(y0), 0)),
                            w2s(QVector3D(float(x), float(y1), 0)));
            for (double y = y0; y <= y1 + 1e-6; y += step)
                fp.drawLine(w2s(QVector3D(float(x0), float(y), 0)),
                            w2s(QVector3D(float(x1), float(y), 0)));
            fp.setPen(QPen(QColor(92, 96, 108), 1.4));
            fp.setBrush(Qt::NoBrush);
            fp.drawPolygon(edge);
        }
        p.drawImage(QPointF(0, 0), m_floorImage);
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
        o.pickable = emitPickable();
        if (vertexDepths.size() == poly.size())
            o.zs = vertexDepths;
        if (vertexAlphas.size() == poly.size())
            o.alphas = vertexAlphas;
        opsOut() << std::move(o);
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
        opsOut() << std::move(o);
        return;
    }
    // The painter path has no light buffer: layer it, as it always did.
    emitPoly(poly, depth, colour, QColor(), 0.0, p, vertexDepths, vertexAlphas);
}

void StructureStudioView::emitLightTri(const QPointF &a, const QPointF &b, const QPointF &c,
                                       double za, double zb, double zc,
                                       double aa, double ab, double ac,
                                       const QColor &colour, QPainter &p) const
{
    if (!m_collecting)
    {
        QPolygonF t;
        t << a << b << c;
        emitLight(t, (za + zb + zc) / 3.0, colour, p,
                  QVector<double>({ za, zb, zc }), QVector<double>({ aa, ab, ac }));
        return;
    }
    QVector<DrawOp> &ops = opsOut();
    if (ops.isEmpty() || !ops.last().mesh || ops.last().fill != colour)
    {
        DrawOp o; o.kind = DrawOp::Poly; o.light = true; o.mesh = true;
        o.fill = colour; o.depth = (za + zb + zc) / 3.0;
        o.poly.reserve(48); o.zs.reserve(48); o.alphas.reserve(48);
        ops << std::move(o);
    }
    DrawOp &o = ops.last();
    o.poly << a << b << c;
    o.zs << za << zb << zc;
    o.alphas << aa << ab << ac;
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
        opsOut() << std::move(o);
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
        opsOut() << std::move(o);
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
        opsOut() << std::move(o);
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
        opsOut() << std::move(o);
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
                      rowFrom, rowTo, o.pickable);
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
                if (o.mesh)
                    for (int t = 0; t + 2 < o.poly.size(); t += 3)
                        p.drawPolygon(o.poly.constData() + t, 3);
                else
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
    OpRefs opaque, translucent, labels, lights;
    opaque.reserve(m_ops.size());
    lights.reserve(m_ops.size());
    for (const DrawOp &o : std::as_const(m_ops))
    {
        if (o.kind == DrawOp::Label || o.kind == DrawOp::Trace)
        {
            labels << &o;        // annotation: screen space, after the resolve
            continue;
        }
        if (o.light)
        {
            lights << &o;        // added after the solids are in, in any order
            continue;
        }
        const bool seeThrough = (o.fill.isValid() && o.fill.alpha() < 255)
                                || (o.pen.isValid() && o.pen.alpha() < 255);
        if (seeThrough)
            translucent << &o;
        else
            opaque << &o;
    }
    std::stable_sort(translucent.begin(), translucent.end(),
                     [](const DrawOp *a, const DrawOp *b) { return a->depth < b->depth; });

    /* On the GPU when there is one (RhiRaster): the same triangles, the same
       passes, multisampled where ZRaster supersamples. What it hands back is
       what ZRaster would have -- the scene and the light -- so everything
       painted round it is unchanged. Picking then asks the CPU for depth
       alone, and only when it is asked (pickDepth()). */
    bool drawn = false;
    m_drewOnGpu = false;
    if (gpuActive())
    {
        drawn = gpuRender(p, opaque, translucent, lights);
        m_drewOnGpu = drawn;
        if (drawn)
        {
            /* Kept for picking: the ops stay where they are (a swap moves
               the lists, not what is in them), so the pointers hold. m_ops
               gets the last frame's, cleared when the next frame starts. */
            m_pickOps.swap(m_ops);
            m_pickOpaque = opaque;
            m_pickSeeThrough.clear();
            for (const DrawOp *o : std::as_const(translucent))
                if (o->pickable)
                    m_pickSeeThrough << o;
            m_pickDirty = true;
        }
    }
    if (!drawn)
    {
    m_pickDirty = false;
    if (m_z == nullptr)
        m_z = new ZRaster();

    /* Transparent, so the floor grid already painted underneath shows through
       wherever the rig does not cover it. */
    m_z->begin(size(), m_supersample, Qt::transparent);


    /* One band of the frame: solids, then light -- after every solid is in, so
       it is stopped by what stands in front of it, and before the translucent
       surfaces, which do not stop it -- then the translucent surfaces back to
       front. Rows are supersampled; light rows are the final image's. */
    const int ss = m_z->supersample();
    auto fillBand = [&](int rowFrom, int rowTo, QRect *lit) {
        for (const DrawOp *o : opaque)
            rasterOp(*o, true, rowFrom, rowTo);
        const int lightFrom = rowFrom / ss;
        const int lightTo = (rowTo == INT_MAX) ? INT_MAX : rowTo / ss;
        for (const DrawOp *op : lights)
        {
            const DrawOp &o = *op;
            const int n = o.poly.size();
            if (n < 3 || !o.fill.isValid())
                continue;
            if (o.mesh)
            {
                for (int t = 0; t + 2 < n; t += 3)
                    m_z->light(o.poly.constData() + t, o.zs.constData() + t, 3, o.fill,
                               o.alphas.constData() + t, lightFrom, lightTo, lit);
                continue;
            }
            QVarLengthArray<double, 8> zs(n);
            for (int i = 0; i < n; ++i)
                zs[i] = (o.zs.size() == n) ? o.zs.at(i) : o.depth;
            m_z->light(o.poly.constData(), zs.constData(), n, o.fill,
                       (o.alphas.size() == n) ? o.alphas.constData() : nullptr,
                       lightFrom, lightTo, lit);
        }
        for (const DrawOp *o : translucent)
            rasterOp(*o, false, rowFrom, rowTo);
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
    }   // !drawn: the CPU

    // Text last, in screen space: depth-testing a label is not meaningful.
    for (const DrawOp *op : std::as_const(labels))
    {
        const DrawOp &o = *op;
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

    if (!drawn)
        m_ops.clear();
}

StructureStudioView::~StructureStudioView()
{
    delete m_z;
    delete m_gpu;
}

bool StructureStudioView::gpuActive() const
{
    return m_useZBuffer && m_gpuWanted && RhiRaster::available();
}

void StructureStudioView::setGpu(bool on)
{
    if (on == m_gpuWanted)
        return;
    m_gpuWanted = on;
    QSettings().setValue(QStringLiteral("monitor/rig/gpu"), on);
    update();
}

bool StructureStudioView::gpuRender(QPainter &p, const OpRefs &opaque,
                                    const OpRefs &translucent, const OpRefs &lights) const
{
    if (m_gpu == nullptr)
        m_gpu = new RhiRaster();

    using V = RhiRaster::Vertex;
    /* A convex polygon as a fan of triangles, each corner its depth and its
       colour premultiplied by its alpha (and, faded, by its own share). */
    auto polyTo = [](QVector<V> &out, const QPointF *pts, const double *zs, int n,
                     const QColor &c, const double *alphas) {
        if (n < 3 || !c.isValid() || c.alpha() == 0)
            return;
        const QRgba64 c64 = c.rgba64();
        const float r = c64.red() / 65535.0f, g = c64.green() / 65535.0f,
                    b = c64.blue() / 65535.0f, ca = c64.alpha() / 65535.0f;
        QVarLengthArray<V, 32> corner(n);
        for (int k = 0; k < n; ++k)
        {
            const float a = ca * (alphas != nullptr ? float(qBound(0.0, alphas[k], 1.0)) : 1.0f);
            corner[k] = V{ float(pts[k].x()), float(pts[k].y()), float(zs[k]),
                           r * a, g * a, b * a, a };
        }
        const qsizetype at = out.size();
        out.resize(at + 3 * (n - 2));
        V *t = out.data() + at;
        for (int i = 1; i + 1 < n; ++i)
        {
            *t++ = corner[0];
            *t++ = corner[i];
            *t++ = corner[i + 1];
        }
    };
    // A line of finite width: a quad with depth down its length (ZRaster::thickLine).
    auto lineTo = [&polyTo](QVector<V> &out, const QPointF &a, const QPointF &b,
                            double za, double zb, double width, const QColor &c) {
        QPointF d = b - a;
        const double len = qSqrt(d.x() * d.x() + d.y() * d.y());
        if (len < 1e-6)
            return;
        d /= len;
        const QPointF nrm(-d.y() * width * 0.5, d.x() * width * 0.5);
        const QPointF q[4] = { a + nrm, b + nrm, b - nrm, a - nrm };
        const double qz[4] = { za, zb, zb, za };
        polyTo(out, q, qz, 4, c, nullptr);
    };
    // One op as rasterOp() draws it.
    auto opTo = [&](QVector<V> &out, const DrawOp &o) {
        const int n = o.poly.size();
        if (n == 0)
            return;
        QVarLengthArray<double, 32> zs(n);
        for (int i = 0; i < n; ++i)
            zs[i] = (o.zs.size() == n) ? o.zs.at(i) : o.depth;
        switch (o.kind)
        {
        case DrawOp::Poly:
            if (o.fill.isValid())
                polyTo(out, o.poly.constData(), zs.constData(), n, o.fill,
                       (o.alphas.size() == n) ? o.alphas.constData() : nullptr);
            if (o.pen.isValid() && n >= 2)
                for (int i = 0; i < n; ++i)
                {
                    const int k = (i + 1) % n;
                    lineTo(out, o.poly.at(i), o.poly.at(k), zs[i], zs[k],
                           qMax(1.0, o.penWidth), o.pen);
                }
            break;
        case DrawOp::Line:
            if (n >= 2 && o.pen.isValid())
                lineTo(out, o.poly.at(0), o.poly.at(1), zs[0], zs[1],
                       qMax(1.0, o.penWidth), o.pen);
            break;
        case DrawOp::Dot:
        {
            const double r = qMax(0.5, o.radius);
            for (int i = 0; i < n; ++i)
            {
                const QPointF c = o.poly.at(i);
                const QPointF q[4] = { QPointF(c.x() - r, c.y() - r), QPointF(c.x() + r, c.y() - r),
                                       QPointF(c.x() + r, c.y() + r), QPointF(c.x() - r, c.y() + r) };
                const double qz[4] = { zs[i], zs[i], zs[i], zs[i] };
                polyTo(out, q, qz, 4, o.fill, nullptr);
            }
            break;
        }
        case DrawOp::Label:
        case DrawOp::Trace:
            break;
        }
    };

    // Kept from frame to frame: cleared, they keep what they had room for.
    QVector<V> &solids = m_gpuSolids, &see = m_gpuSee, &light = m_gpuLight;
    solids.clear();
    see.clear();
    light.clear();
    for (const DrawOp *o : opaque)
        opTo(solids, *o);
    for (const DrawOp *o : translucent)
        opTo(see, *o);
    /* Light adds: each corner carries how much of the colour it adds, on the
       0..1-per-255 scale the light buffer keeps (ZRaster::light()). */
    for (const DrawOp *op : lights)
    {
        const DrawOp &o = *op;
        const int n = o.poly.size();
        if (n < 3 || !o.fill.isValid())
            continue;
        if (o.mesh)
        {
            for (int t = 0; t + 2 < n; t += 3)
                polyTo(light, o.poly.constData() + t, o.zs.constData() + t, 3, o.fill,
                       o.alphas.constData() + t);
            continue;
        }
        QVarLengthArray<double, 8> zs(n);
        for (int i = 0; i < n; ++i)
            zs[i] = (o.zs.size() == n) ? o.zs.at(i) : o.depth;
        polyTo(light, o.poly.constData(), zs.constData(), n, o.fill,
               (o.alphas.size() == n) ? o.alphas.constData() : nullptr);
    }

    if (!m_gpu->render(size(), m_supersample, solids, see, light))
        return false;
    p.drawImage(0, 0, m_gpu->scene());
    const QImage lit = m_gpu->lightImage();
    if (!lit.isNull())
    {
        p.save();
        p.setCompositionMode(QPainter::CompositionMode_Screen);
        p.drawImage(0, 0, lit);
        p.restore();
    }
    return true;
}

double StructureStudioView::pickDepth(const QPoint &px) const
{
    if (!m_useZBuffer)
        return -1e30;
    if (m_pickDirty)
    {
        // The GPU drew the frame: depth alone, once, from what it drew.
        if (m_z == nullptr)
            m_z = new ZRaster();
        m_z->begin(size(), 1, Qt::transparent);
        for (const DrawOp *o : std::as_const(m_pickOpaque))
            rasterOp(*o, true, 0, INT_MAX);
        for (const DrawOp *o : std::as_const(m_pickSeeThrough))
            rasterOp(*o, false, 0, INT_MAX);
        m_pickDirty = false;
    }
    return (m_z != nullptr && m_z->isValid()) ? m_z->depthAt(px) : -1e30;
}

double StructureStudioView::viewDepth(const QVector3D &w) const
{
    /* CONVENTION: a LARGER value is NEARER the eye. Painter's algorithm
       therefore draws in ASCENDING order -- smallest (farthest) first. Both
       sorts here originally ran descending while their comments said "back to
       front", so far objects painted over near ones: a step covered the tower
       standing in front of it. */
    // The view direction derived in project(): d = (-sinA cosE, -cosA cosE, -sinE).
    const Trig &t = angledTrig();
    return -(double(w.x()) * -t.sA * t.cE
             + double(w.y()) * -t.cA * t.cE
             + double(w.z()) * -t.sE);
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
    // A deck top or a fabric is see-through and still where the pointer is.
    QScopedValueRollback<bool> pickable(emitPickable(), true);
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

/* Turn a box's corners about the vertical by what @p toWorld does to a plan
 * point -- a platform's or a tower's own facing. */
template <typename Obj>
static void turnCorners(QVector3D c[8], const Obj *o)
{
    if (o == nullptr || !o->isRotated())
        return;
    for (int i = 0; i < 8; ++i)
    {
        const QPointF w = o->toWorld(QPointF(double(c[i].x()), double(c[i].y())));
        c[i].setX(float(w.x()));
        c[i].setY(float(w.y()));
    }
}

/* A plan point of a turned platform or tower, at height @p z. */
template <typename Obj>
static QVector3D turned(const Obj *o, float x, float y, float z)
{
    const QPointF w = o->toWorld(QPointF(double(x), double(y)));
    return QVector3D(float(w.x()), float(w.y()), z);
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

void StructureStudioView::drawHangersOn(QPainter &p, Kind kind, quint32 id) const
{
    if (kind != TrussKind && kind != TowerKind)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    const QList<Hanger *> hangers = props->hangersOn(kind == TrussKind ? Hanger::OnTruss : Hanger::OnTower, id);
    if (hangers.isEmpty())
        return;
    /* The bracket: a short clamp on the face, the arm out from it, a hook at
       its end, and the chain down from the hook -- what holds a lantern off
       the side of a tower, so the hang reads. */
    const double af = 0.45 + 0.55 * m_ambient;
    const QColor metal(qRound(165 * af), qRound(165 * af), qRound(172 * af));
    for (const Hanger *h : hangers)
    {
        const QVector3D fastened = h->fastened(), end = h->armEnd(), hook = h->hook();
        const QVector3D clampA = fastened + QVector3D(0.0f, 0.0f, 0.05f);
        const QVector3D clampB = fastened - QVector3D(0.0f, 0.0f, 0.05f);
        emitLine(w2s(clampA), w2s(clampB), viewDepth(clampA), viewDepth(clampB), metal, 4.0, p);
        if (h->arm() > 0.0f)
            emitLine(w2s(fastened), w2s(end), viewDepth(fastened), viewDepth(end), metal, 3.0, p);
        if (h->drop() > 0.0f)
        {
            // A chain: short links, not a rod.
            const int links = qMax(2, int(h->drop() / 0.04f));
            for (int i = 0; i < links; i += 2)
            {
                const QVector3D a = end + (hook - end) * float(double(i) / links);
                const QVector3D b = end + (hook - end) * float(double(i + 1) / links);
                emitLine(w2s(a), w2s(b), viewDepth(a), viewDepth(b), metal.darker(110), 1.4, p);
            }
        }
    }
}

void StructureStudioView::drawOneStructure(QPainter &p, Kind kind, quint32 id) const
{
    drawHangersOn(p, kind, id);
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
        float x0 = t->originX(), y0 = t->originY();
        float x1 = x0 + t->width(), y1 = y0 + t->depth();
        const float h = t->height();
        const QList<QPointF> tc = t->corners();     // turned, round from x0y0
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
            turnCorners(c, t);
            const QVector3D endA[4] = { c[0], c[1], c[2], c[3] };
            const QVector3D endB[4] = { c[4], c[5], c[6], c[7] };
            drawLattice(p, endA, endB, steel, qMax(1, int(h / qMax(0.3f, t->width()))));
            // Shelves read as lines across the front face.
            for (int i = 0; i < t->shelfCount(); ++i)
            {
                const float z = t->shelfHeight(i);
                const QVector3D sa = turned(t, x0, y1, z), sb = turned(t, x1, y1, z);
                emitLine(w2s(sa), w2s(sb), viewDepth(sa), viewDepth(sb),
                         steel.lighter(140), 1.6, p);
            }
        }
        else if (m_plane == Top)
        {
            QPolygonF plan;
            foreach (const QPointF &q, tc)
                plan << w2s(QVector3D(float(q.x()), float(q.y()), 0));
            p.drawPolygon(plan);
            if (t->hasFaces())
            {
                // Its faced sides, as thick edges in the panels' colour.
                QColor fc = t->facesColour();
                if (fc.lightness() < 60)
                    fc = fc.lighter(260);           // a black facade, still seen on the dark plan
                p.setPen(QPen(fc, 4.0, Qt::SolidLine, Qt::FlatCap));
                const QPointF fl = plan.at(3), fr = plan.at(2);
                const QPointF bl = plan.at(0), br = plan.at(1);
                if (t->facesSides() & Tower::FrontSide) p.drawLine(fl, fr);
                if (t->facesSides() & Tower::BackSide)  p.drawLine(bl, br);
                if (t->facesSides() & Tower::LeftSide)  p.drawLine(fr, br);
                if (t->facesSides() & Tower::RightSide) p.drawLine(fl, bl);
                p.setPen(QPen(steel, 1.8));
            }
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
            if (t->isRotated())
            {
                // Seen flat, a turned tower is as wide as its corners reach.
                x0 = y0 = 1e9f;
                x1 = y1 = -1e9f;
                foreach (const QPointF &q, tc)
                {
                    x0 = qMin(x0, float(q.x())); x1 = qMax(x1, float(q.x()));
                    y0 = qMin(y0, float(q.y())); y1 = qMax(y1, float(q.y()));
                }
            }
            const QPointF a = w2s(QVector3D(x0, y0, 0));
            const QPointF b = w2s(QVector3D(x1, y1, h));
            p.drawRect(QRectF(a, b).normalized());
            /* Its faces, seen flat: the stretch they cover filled -- the print
               when there is one -- so the shelves and lines still read over it. */
            const int facing = (m_plane == Front) ? int(Tower::FrontSide)
                             : int(Tower::LeftSide | Tower::RightSide);
            if (t->hasFaces() && (t->facesSides() & facing))
            {
                const QRectF band = QRectF(w2s(QVector3D(x0, y0, t->facesFrom())),
                                           w2s(QVector3D(x1, y1, t->facesTo()))).normalized();
                const QImage pattern = patternImage(t->facesPattern());
                p.fillRect(band, t->facesColour());
                if (!pattern.isNull())
                {
                    p.save();
                    p.setOpacity(t->facesPatternOpacity());
                    p.drawImage(band, pattern);
                    p.restore();
                }
            }
            p.setPen(QPen(steel.lighter(140), 2.0));
            p.setFont(QFont("Arial", 8));
            const QFontMetricsF fm(p.font());
            for (int i = 0; i < t->shelfCount(); ++i)
            {
                const float z = t->shelfHeight(i);
                const QPointF sA = w2s(QVector3D(x0, y0, z));
                const QPointF sB = w2s(QVector3D(x1, y1, z));
                p.setPen(QPen(steel.lighter(140), 2.0));
                p.drawLine(sA, sB);
                /* On the LEFT of the tower, right-aligned against it: the
                   fixtures on a shelf are named on the right, and the two
                   used to print over each other ("text stomping on each
                   other .. shelf info on left and light info on right"). */
                p.setPen(steel.lighter(170));
                const QString label = shelfLabel(i, z, toDisp, sfx);
                const double leftX = qMin(sA.x(), sB.x());
                p.drawText(QPointF(leftX - 6 - fm.horizontalAdvance(label), sA.y() - 3), label);
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

        /* Its sock, in a flat view: a band of fabric over the stretch it
           covers, glowing where the bars inside are lit. Drawn first, under
           the lattice lines, so the truss still reads through it. */
        if (t->hasSock() && axis.length() >= 4.0 && t->sockTo() - t->sockFrom() >= 0.01f)
        {
            const float from = t->sockFrom(), to = t->sockTo();
            const int segments = qBound(1, int((to - from) / 0.2f + 0.5f), 200);
            const QVector<QVector3D> glow = sockGlow(t, segments);
            const QColor fabric = t->sockColour();
            const QPointF dir = (b - a) / axis.length();
            const QPointF off = QPointF(-dir.y(), dir.x()) * (wpx / 2.0 + 1.0);
            p.setPen(Qt::NoPen);
            for (int k = 0; k < segments; ++k)
            {
                const QPointF s0 = w2s(t->positionAt(from + (to - from) * k / segments));
                const QPointF s1 = w2s(t->positionAt(from + (to - from) * (k + 1) / segments));
                const QVector3D g = glow.at(k);
                const double lit = qMin(1.0, double(qMax(g.x(), qMax(g.y(), g.z()))) * 1.5);
                const double room = 0.55 * (1.0 - 0.8 * lit);
                QColor c = QColor::fromRgbF(float(qMin(1.0, fabric.redF() * room + g.x())),
                                            float(qMin(1.0, fabric.greenF() * room + g.y())),
                                            float(qMin(1.0, fabric.blueF() * room + g.z())));
                c.setAlpha(210);
                p.setBrush(c);
                QPolygonF band;
                band << s0 + off << s1 + off << s1 - off << s0 - off;
                p.drawPolygon(band);
            }
        }

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
            const int topAlpha = (pl->topMaterial() == StagePlatform::SolidTop) ? 255
                               : (pl->topMaterial() == StagePlatform::ClearTop) ? 70 : 0;
            if (pl->hasRoundedFront())
            {
                /* A rounded front: the deck's outline stood up into its sides,
                   shaded by which way each faces, with the top over them. */
                QScopedValueRollback<bool> pickable(emitPickable(), true);
                const QList<QPointF> fp = pl->footprint(32);
                const float top = b0 + pl->height();
                const double af = 0.18 + 0.82 * m_ambient;
                auto lit = [af](QColor c) {
                    return QColor(qRound(c.red() * af), qRound(c.green() * af), qRound(c.blue() * af), c.alpha());
                };
                for (int i = 0; i < fp.size(); ++i)
                {
                    const QPointF a = fp.at(i), b = fp.at((i + 1) % fp.size());
                    const QVector3D c[4] = { QVector3D(float(a.x()), float(a.y()), b0),
                                             QVector3D(float(b.x()), float(b.y()), b0),
                                             QVector3D(float(b.x()), float(b.y()), top),
                                             QVector3D(float(a.x()), float(a.y()), top) };
                    // Outward is to the right of the way round (clockwise on the plan).
                    const double nx = b.y() - a.y(), ny = -(b.x() - a.x());
                    const double len = qSqrt(nx * nx + ny * ny);
                    const double facing = (len > 0.0) ? (0.5 * nx + 0.8 * ny) / len : 0.0;
                    // 95% facing the house down to 45% facing away, as a box's faces are.
                    const double pct = 70.0 + 25.0 * qBound(-1.0, facing, 1.0);
                    const QColor side = lit(pc.darker(qRound(10000.0 / pct)));
                    QPolygonF poly;
                    QVector<double> vz;
                    double d = 0.0;
                    for (const QVector3D &w : c)
                    {
                        poly << w2s(w);
                        vz << viewDepth(w);
                        d += vz.last();
                    }
                    emitPoly(poly, d / 4.0, side, edge, 1.1, p, vz);
                }
                if (topAlpha > 0)
                {
                    QColor deck = pc.lighter(118);
                    deck.setAlpha(topAlpha);
                    QPolygonF poly;
                    QVector<double> vz;
                    double d = 0.0;
                    for (const QPointF &q : fp)
                    {
                        const QVector3D w(float(q.x()), float(q.y()), top);
                        poly << w2s(w);
                        vz << viewDepth(w);
                        d += vz.last();
                    }
                    emitPoly(poly, d / fp.size(), lit(deck), edge, 1.1, p, vz);
                }
            }
            else
            // A flight of steps is a box per step; a platform is one.
            foreach (const StagePlatform::Block &bk, pl->blocks())
            {
                QVector3D c[8];
                boxCorners(c, bk.x0, bk.y0, b0, bk.x1, bk.y1, b0 + bk.height);
                turnCorners(c, pl);
                drawSolidBox(p, c, pc, edge, topAlpha);
            }
        }
        else if (pl->isSteps())
        {
            /* Steps, flat: every step from the base to its tread, tallest
               first. Seen from the side that IS the staircase profile; seen
               face-on the lower ones draw over the taller, leaving a line at
               each tread; in plan they are the treads. */
            const float b0 = (m_plane == Top) ? 0.0f : props->platformBaseZ(id);
            QList<StagePlatform::Block> bl = pl->blocks();
            std::sort(bl.begin(), bl.end(), [](const StagePlatform::Block &a,
                                               const StagePlatform::Block &b) {
                return a.height > b.height;
            });
            foreach (const StagePlatform::Block &bk, bl)
            {
                const QList<QPointF> cn = pl->corners(bk);
                if (m_plane == Top)
                {
                    // Each tread, turned with the flight.
                    QPolygonF poly;
                    foreach (const QPointF &q, cn)
                        poly << w2s(QVector3D(float(q.x()), float(q.y()), b0));
                    p.setBrush(fill);
                    p.setPen(QPen(halo, 3.2));
                    p.drawPolygon(poly);
                    p.setBrush(Qt::NoBrush);
                    p.setPen(QPen(edge, 1.6));
                    p.drawPolygon(poly);
                    continue;
                }
                float ax = 1e9f, ay = 1e9f, bx = -1e9f, by = -1e9f;
                foreach (const QPointF &q, cn)
                {
                    ax = qMin(ax, float(q.x())); bx = qMax(bx, float(q.x()));
                    ay = qMin(ay, float(q.y())); by = qMax(by, float(q.y()));
                }
                outline(QRectF(w2s(QVector3D(ax, ay, b0)), w2s(QVector3D(bx, by, b0 + bk.height)))
                            .normalized());
            }
        }
        else if (m_plane == Top && (pl->hasRoundedFront() || pl->isRotated()))
        {
            QPolygonF poly;
            foreach (const QPointF &q, pl->footprint(32))
                poly << w2s(QVector3D(float(q.x()), float(q.y()), 0));
            p.setBrush(fill);
            p.setPen(QPen(halo, 3.2));
            p.drawPolygon(poly);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(edge, 1.6));
            p.drawPolygon(poly);
        }
        else if (m_plane == Top)
        {
            outline(QRectF(w2s(QVector3D(x0, y0, 0)), w2s(QVector3D(x1, y1, 0))).normalized());
        }
        else
        {
            // Riser box silhouette from its BASE (which may sit on a lower
            // platform, not the floor) up to its deck top (base + own thickness).
            // Turned, it is as wide as its outline reaches.
            float ax = x0, ay = y0, bx = x1, by = y1;
            if (pl->isRotated())
            {
                ax = ay = 1e9f;
                bx = by = -1e9f;
                foreach (const QPointF &q, pl->footprint(16))
                {
                    ax = qMin(ax, float(q.x())); bx = qMax(bx, float(q.x()));
                    ay = qMin(ay, float(q.y())); by = qMax(by, float(q.y()));
                }
            }
            const float b0  = props->platformBaseZ(id);
            const float top = b0 + h;
            const QPointF a = w2s(QVector3D(ax, ay, b0));
            const QPointF b = w2s(QVector3D(bx, by, top));
            outline(QRectF(a, b).normalized());
            p.setPen(QPen(edge.lighter(115), 1.8));   // deck line (bright, on top)
            p.drawLine(w2s(QVector3D(ax, ay, top)), w2s(QVector3D(bx, by, top)));
        }
    }
    else if (kind == SceneryKind)
    {
        /* The cloth, as a strip of panels so a bowed cyc curves and each
           panel sorts at its own depths. Lit by the room like any scenery --
           the light the rig throws on it arrives as pools on top. A wall has
           two faces, a top and its ends; a drape hangs in folds; a scrim is
           seen through. */
        Scenery *sc = props->scenery(id);
        if (sc == nullptr) return;
        QColor cloth = sc->color().isValid() ? sc->color() : Scenery::defaultColor(sc->kind());
        const double af = 0.18 + 0.82 * m_ambient;
        cloth = QColor(qRound(cloth.red() * af), qRound(cloth.green() * af), qRound(cloth.blue() * af));
        if (sc->isSeeThrough())
            cloth.setAlpha(70);
        const QColor seam = sc->isSeeThrough() ? QColor() : cloth.darker(118);
        const bool folds = sc->isCloth() && sc->kind() != Scenery::Cyclorama && !sc->isSeeThrough();
        const double half = double(sc->thickness()) / 2.0;
        // About how long the line is, for how finely folds are drawn.
        double lineLen = 0.0;
        for (int i = 0; i < 32; ++i)
            lineLen += double((sc->pointAt(-1.0 + 2.0 * (i + 1) / 32, 0) - sc->pointAt(-1.0 + 2.0 * i / 32, 0)).length());

        auto quad = [&](const QVector3D &a, const QVector3D &b, const QVector3D &c, const QVector3D &d,
                        const QColor &fill) {
            QPolygonF poly;
            QVector<double> vz;
            double dsum = 0.0;
            for (const QVector3D &w : { a, b, c, d })
            {
                poly << w2s(w);
                vz << viewDepth(w);
                dsum += vz.last();
            }
            emitPoly(poly, dsum / 4.0, fill, seam, 0.6, p, vz);
        };
        const QList<Scenery::Piece> pieces = sc->pieces();
        const bool gathered = sc->hasState() && (pieces.size() > 1);
        foreach (const Scenery::Piece &pc, pieces)
        {
            const QPair<double, double> sp(pc.u0, pc.u1);
            const float lo = pc.z0, hi = pc.z1;
            int n = qMax(1, int(qCeil(sc->segments() * (sp.second - sp.first) / 2.0)));
            if (folds)
            {
                // A fold every 30 cm of cloth -- tighter where it is gathered up.
                const double len = lineLen * (sp.second - sp.first) / 2.0;
                n = qMax(n, int(qCeil(len / (gathered ? 0.08 : 0.3))));
            }
            const QList<double> faces = (half > 0.0) ? QList<double>{ -half, half } : QList<double>{ 0.0 };
            for (int i = 0; i < n; ++i)
            {
                const double u0 = sp.first + (sp.second - sp.first) * i / n;
                const double u1 = sp.first + (sp.second - sp.first) * (i + 1) / n;
                const QColor fill = (folds && (i % 2)) ? cloth.darker(135) : cloth;
                foreach (double off, faces)
                    quad(sc->pointAt(u0, lo, off), sc->pointAt(u1, lo, off),
                         sc->pointAt(u1, hi, off), sc->pointAt(u0, hi, off), fill);
                if (half > 0.0)     // its top
                    quad(sc->pointAt(u0, hi, -half), sc->pointAt(u1, hi, -half),
                         sc->pointAt(u1, hi, half), sc->pointAt(u0, hi, half), cloth.lighter(112));
                if (half > 0.0 && lo > sc->bottom() + 1e-4f)     // a header's underside
                    quad(sc->pointAt(u0, lo, -half), sc->pointAt(u1, lo, -half),
                         sc->pointAt(u1, lo, half), sc->pointAt(u0, lo, half), cloth.darker(140));
            }
            if (half > 0.0)         // its ends
                for (double u : { sp.first, sp.second })
                    quad(sc->pointAt(u, lo, -half), sc->pointAt(u, lo, half),
                         sc->pointAt(u, hi, half), sc->pointAt(u, hi, -half), cloth.darker(110));
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
    return RigHeads::axis(rp);
}

void StructureStudioView::fixtureFrame(const FixtureRigProps &rp,
                                       QVector3D &L, QVector3D &N, QVector3D &H) const
{
    // Shared with the engine, so an effect's grid is laid out on the same
    // faces this view paints (RigHeads).
    RigHeads::frame(rp, L, N, H);
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

QVector<StructureStudioView::Deck> StructureStudioView::decks() const
{
    /* Each shown platform, its blocks and the height it stands at. Every ray
       of every beam asks; worked out per ray it was every platform's every
       block asking where every other platform is (platformBaseZ()), and a
       rig of step units lit up took a second a frame. So once a frame, while
       painting (m_decksValid); outside a paint, as the platforms are now. */
    if (m_decksValid)
        return m_decks;
    QVector<Deck> out;
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (StagePlatform *pl, props->platforms())
    {
        if (pl == nullptr || !layerShown(pl->layerId()))
            continue;
        Deck d;
        d.platform = pl;
        d.baseZ = double(props->platformBaseZ(pl->id()));
        d.blocks = pl->blocks();
        out << d;
    }
    return out;
}

quint64 StructureStudioView::throwScene() const
{
    QVector<float> f;
    for (const Deck &d : decks())
    {
        const StagePlatform *pl = d.platform;
        f << float(pl->id()) << pl->originX() << pl->originY() << pl->width()
          << pl->depth() << pl->rotation() << pl->frontCurve() << float(d.baseZ);
        for (const StagePlatform::Block &b : d.blocks)
            f << b.x0 << b.y0 << b.x1 << b.y1 << b.height;
    }
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (Scenery *sc, props->sceneries())
    {
        if (sc == nullptr || !layerShown(sc->layerId()))
            continue;
        f << float(sc->id()) << float(sc->kind()) << sc->originX() << sc->originY()
          << sc->width() << sc->height() << sc->bottom() << sc->rotation() << sc->bow()
          << float(sc->outline()) << sc->depth() << sc->thickness() << float(sc->state())
          << sc->openingWidth() << sc->openingHeight();
    }
    return quint64(qHashBits(f.constData(), size_t(f.size()) * sizeof(float), 0x5eed))
         ^ (quint64(f.size()) << 40);
}

double StructureStudioView::beamThrow(const QVector3D &apex, const QVector3D &dir,
                                     bool *hit, QVector3D *landNormal, int *surface) const
{
    /* While painting, a ray thrown before (the same apex, the same way) at
       the same scene lands where it did: no need to ask every deck and every
       piece of scenery again. */
    ThrowKey key;
    if (m_decksValid)
    {
        key = ThrowKey{ { apex.x(), apex.y(), apex.z(), dir.x(), dir.y(), dir.z() } };
        auto it = m_throws.constFind(key);
        if (it != m_throws.constEnd())
        {
            /* On a drawing thread the kept landings are only read: what was
               used is marked once they all are done. */
            if (s_emit != nullptr)
                s_emit->throwHits << &it.value();
            else if (it->frame != m_throwFrame)
            {
                const_cast<Throw &>(it.value()).frame = m_throwFrame;
                ++m_throwsUsed;
            }
            if (hit != nullptr)
                *hit = it->hit;
            if (landNormal != nullptr)
                *landNormal = it->normal;
            if (surface != nullptr)
                *surface = it->surface;
            return it->run;
        }
    }

    bool h = false;
    QVector3D n;
    int sf = 0;
    const double run = beamThrowUncached(apex, dir, &h, &n, &sf);
    if (m_decksValid)
    {
        if (s_emit != nullptr)
            s_emit->newThrows << qMakePair(key, Throw{ run, n, sf, h, m_throwFrame });
        else
        {
            m_throws.insert(key, Throw{ run, n, sf, h, m_throwFrame });
            ++m_throwsUsed;
        }
    }
    if (hit != nullptr)
        *hit = h;
    if (landNormal != nullptr)
        *landNormal = n;
    if (surface != nullptr)
        *surface = sf;
    return run;
}

double StructureStudioView::beamThrowUncached(const QVector3D &apex, const QVector3D &dir,
                                             bool *hit, QVector3D *landNormal, int *surface) const
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
    int what = 0;

    // Platform tops, but only where the beam actually crosses the footprint.
    MonitorProperties *props = m_doc->monitorProperties();
    const QVector<Deck> shown = decks();   // const: shares, never copies
    for (const Deck &deck : shown)
    {
        StagePlatform *pl = deck.platform;
        // Each tread of a flight of steps is a deck of its own height.
        int blockNo = 0;
        for (const StagePlatform::Block &bk : deck.blocks)
        {
            ++blockNo;
            const double topZ = deck.baseZ + double(bk.height);
            if (double(apex.z()) <= topZ)      // already at or below it
                continue;
            const double t = toPlane(topZ);
            if (t < 0.0 || (best > 0.0 && t >= best))
                continue;
            const QVector3D at = apex + dir * float(t);
            const QPointF loc = pl->toLocal(QPointF(double(at.x()), double(at.y())));
            if (loc.x() < bk.x0 || loc.x() > bk.x1 || loc.y() < bk.y0 || loc.y() > bk.y1)
                continue;
            if (!pl->containsPoint(at.x(), at.y()))   // off a rounded front's cut corners
                continue;
            best = t;
            what = int(pl->id()) * 64 + blockNo;
        }
    }
    QVector3D normal(0, 0, 1);                // floor and decks face up

    /* Scenery: a cyc or a drape stands in the way of a beam going level or
       even upward, which is exactly how a cyc is lit. */
    foreach (Scenery *sc, props->sceneries())
    {
        if (sc == nullptr || !layerShown(sc->layerId()))
            continue;
        double t = 0.0;
        QVector3D n;
        if (sc->rayHit(apex, dir, t, best > 0.0 ? best : MAX_THROW, 16, &n) && t > MIN_THROW)
        {
            best = t;
            normal = n;
            what = -1 - int(sc->id());
        }
    }

    if (surface != nullptr)
        *surface = (best > 0.0) ? what : 0;
    if (hit != nullptr)
        *hit = (best > 0.0);
    if (landNormal != nullptr)
        *landNormal = normal;
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
    // What plays: the look's palettes plus one being tried on in the editor.
    const QList<quint32> pals = scene->runtimePalettes();
    bool subjectMode = false;
    foreach (quint32 pid, pals)
    {
        QLCPalette *p2 = m_doc->palette(pid);
        if (p2 != nullptr && p2->type() == QLCPalette::Effect
            && p2->scriptPath().contains(QLatin1String("followspot"), Qt::CaseInsensitive))
        {
            subjectMode = true;
            break;
        }
    }
    // A palette being tried on wins, as it does on the rig (it plays last).
    QList<quint32> aimOrder = pals;
    const int trialAt = scene->palettes().contains(scene->trialPalette())
                        ? -1 : int(aimOrder.indexOf(scene->trialPalette()));
    if (trialAt > 0)
        aimOrder.move(trialAt, 0);
    foreach (quint32 pid, aimOrder)
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
    /* The scene's own channel values first -- what its faders set ("push blue
       up from all the USt bars .. I don't see anything on the rig view"): a
       look built on faders, with no palette, drew nothing unless live DMX was
       flowing. Palettes go over them, as Scene::write does. */
    foreach (const SceneValue &scv, scene->values())
    {
        Fixture *fx = m_doc->fixture(scv.fxi);
        if (fx == nullptr || scv.channel >= fx->channels())
            continue;
        QByteArray &arr = m_sceneValues[scv.fxi];
        // Zero-filled: resize() leaves the new bytes as whatever was in
        // memory -- every channel the look doesn't set drew as junk (on
        // Linux, an old string's bytes lit a pixel bar grey).
        if (arr.size() < int(fx->channels()))
            arr.append(QByteArray(int(fx->channels()) - arr.size(), char(0)));
        arr[int(scv.channel)] = char(scv.value);
    }
    foreach (quint32 pid, pals)
    {
        QLCPalette *pal = m_doc->palette(pid);
        if (pal == nullptr)
            continue;
        if (pal->type() == QLCPalette::Effect || pal->type() == QLCPalette::Aim)
            continue;
        if (QLCPalette::isEffectScoped(m_doc, pals, pid))
            continue;

        const int cOff = QLCPalette::colorSetOffset(m_doc, pals, pid);
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
                arr.append(QByteArray(int(fx->channels()) - arr.size(), char(0)));
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
QVector3D StructureStudioView::moverAim(quint32 fid, int unit) const
{
    Fixture *fx = m_doc != nullptr ? m_doc->fixture(fid) : nullptr;
    if (fx == nullptr)
        return QVector3D();
    // The scene's target while it is only selected; the DMX once it runs.
    const bool sceneDriving = m_liveValues && activeSceneRunning();
    QVector3D aimPt;
    if (!sceneDriving && aimPointFor(fid, aimPt))
    {
        const QVector3D toTarget = aimPt - fixtureCentre(fid);
        if (toTarget.length() > 1e-3f)
            return toTarget.normalized();
    }
    QVector3D dir;
    if (m_liveValues && fixtureAimDirection(fx, m_doc->monitorProperties()->fixtureRigProps(fid), dir, unit))
        return dir;
    return QVector3D();
}

bool StructureStudioView::activeSceneRunning() const
{
    const Function *f = m_doc != nullptr ? m_doc->function(m_activeSceneId) : nullptr;
    return f != nullptr && f->isRunning();
}

bool StructureStudioView::aimPointFor(quint32 fid, QVector3D &out) const
{
    const QHash<quint32, AimSpec>::const_iterator it = m_aimTarget.constFind(fid);
    if (it == m_aimTarget.constEnd())
        return false;

    MonitorProperties *props = m_doc->monitorProperties();
    StageTarget *t = props->stageTarget(it.value().targetId);
    if (t == nullptr)
        return false;

    /* The shared rule -- binding, kind, height and this head's beam all
       resolved in one place, so this view and QLCPalette cannot drift apart
       again. */
    out = AimSolver::aimPointFor(m_doc, fid, t, it.value().subjectHeight);
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
            opsOut() << o;
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

namespace
{
/* How fast a star laser's pattern turns, degrees a second: its prism
   rotation channel ("Prisim Rotation", PrismRotationSlowFast), or any prism
   rotation capability. 0 = still. */
double starTurnRate(const Fixture *fx, const QByteArray &vals)
{
    const QLCFixtureMode *mode = (fx != nullptr) ? fx->fixtureMode() : nullptr;
    if (mode == nullptr)
        return 0.0;
    const int count = qMin(mode->channels().size(), vals.size());
    for (int c = 0; c < count; ++c)
    {
        const QLCChannel *ch = mode->channel(quint32(c));
        if (ch == nullptr)
            continue;
        const int v = uchar(vals.at(c));
        if (ch->preset() == QLCChannel::PrismRotationSlowFast)
            return v > 0 ? 10.0 + 170.0 * v / 255.0 : 0.0;
        if (ch->preset() == QLCChannel::PrismRotationFastSlow)
            return v > 0 ? 10.0 + 170.0 * (255 - v) / 255.0 : 0.0;
        if (ch->group() == QLCChannel::Prism)
        {
            const double s = BeamOptics::rotationSpeed(ch->searchCapability(uchar(v)), v);
            if (s != 0.0)
                return s;
        }
    }
    return 0.0;
}
}

void StructureStudioView::drawLaserBeam(QPainter &p, const QVector3D &apex, const QVector3D &dir,
                                        const QColor &colour, double level) const
{
    if (level <= 0.01 || dir.isNull())
        return;
    const QVector3D d = dir.normalized();
    const QVector3D end = apex + d * float(beamThrow(apex, d));
    const double k = qBound(0.0, level, 1.0);
    // Laser light is saturated and hard: the colour, bright, a little white
    // at its core.
    const QColor hsv = colour.toHsv();
    const QColor core = QColor::fromHsvF(qMax(0.0, double(hsv.hsvHueF())), hsv.hsvSaturationF() * 0.85,
                                         qMin(1.0, 0.35 + 0.65 * k));
    if (m_haze > 0.05)
    {
        QColor glow = colour;
        glow.setAlphaF(qMin(0.5, 0.28 * m_haze * k));
        emitLine(w2s(apex), w2s(end), viewDepth(apex), viewDepth(end), glow, 4.0, p);
    }
    emitLine(w2s(apex), w2s(end), viewDepth(apex), viewDepth(end), core, 1.4, p);
    emitDot(w2s(end), viewDepth(end) + 0.002, 2.4, core, p);
}

void StructureStudioView::drawSilkFlame(QPainter &p, const QVector3D &base, double width,
                                        double height, double level, double seed) const
{
    /* Seven tongues across the silk, each a ribbon from a wide foot to a
       point, its tip swaying and its height breathing with time; drawn in
       three planes crossed at sixty degrees, so it is a flame from wherever
       it is seen. Orange outside, a shorter yellow core inside, both as
       light (they glow), fading to nothing at the tips. */
    const double t = opticsTime();
    const int TONGUES = 7, ROWS = 6;
    const double half = width / 2.0;
    auto noise = [](double x) { return 0.5 * qSin(x) + 0.3 * qSin(2.3 * x + 1.1) + 0.2 * qSin(5.1 * x + 2.7); };

    for (int plane = 0; plane < 3; ++plane)
    {
        const double a = plane * M_PI / 3.0;
        const QVector3D across(float(qCos(a)), float(qSin(a)), 0.0f);
        const QVector3D sway(float(-qSin(a)), float(qCos(a)), 0.0f);
        for (int k = 0; k < TONGUES; ++k)
        {
            const double u = (TONGUES == 1) ? 0.0 : (2.0 * k / (TONGUES - 1) - 1.0);   // -1 .. 1
            const double ph = seed + k * 1.7 + plane * 0.9;
            // Tallest in the middle; each breathing on its own.
            const double tall = height * (0.55 + 0.45 * (1.0 - u * u))
                                * (0.75 + 0.25 * noise(t * 3.1 + ph));
            const double foot = half / TONGUES * 1.6;
            for (int layer = 0; layer < 2; ++layer)
            {
                const bool core = (layer == 1);
                const double hh = core ? tall * 0.6 : tall;
                const double ww = core ? foot * 0.55 : foot;
                QColor c = core ? QColor(255, 215, 70) : QColor(255, 110, 25);
                c.setAlpha(qBound(0, int((core ? 230.0 : 200.0) * level), 255));
                const QVector3D root = base + across * float(u * half * 0.85);
                // The ribbon's two edges, row by row, narrowing to the tip.
                QVector<QVector3D> left, right;
                QVector<double> alpha;
                for (int r = 0; r <= ROWS; ++r)
                {
                    const double f = double(r) / ROWS;
                    const double bend = hh * 0.18 * f * f * noise(t * 2.3 + ph + f * 2.0);
                    const double flick = hh * 0.10 * f * f * noise(t * 4.7 + ph * 1.3);
                    const QVector3D mid = root + QVector3D(0.0f, 0.0f, float(hh * f))
                                          + across * float(bend) + sway * float(flick);
                    const double wd = ww * (1.0 - f) * (1.0 - 0.3 * f);
                    left << mid - across * float(wd);
                    right << mid + across * float(wd);
                    alpha << (1.0 - f) * (1.0 - f * 0.4);
                }
                for (int r = 0; r < ROWS; ++r)
                {
                    const QVector3D q[4] = { left[r], right[r], right[r + 1], left[r + 1] };
                    QPolygonF poly;
                    QVector<double> zs;
                    double zsum = 0.0;
                    for (const QVector3D &w : q)
                    {
                        poly << w2s(w);
                        zs << viewDepth(w) + 2e-4;
                        zsum += zs.last();
                    }
                    emitLight(poly, zsum / 4.0, c, p, zs,
                              QVector<double>({ alpha[r], alpha[r], alpha[r + 1], alpha[r + 1] }));
                }
            }
        }
    }
}

void StructureStudioView::drawStarField(QPainter &p, quint32 fid, const Fixture *fx,
                                        const QByteArray &vals, const QVector3D &apex,
                                        const QVector3D &dir, double halfAngleRad,
                                        const QColor &colour, double level) const
{
    if (level <= 0.01 || dir.isNull())
        return;
    const QVector3D d = dir.normalized();
    QVector3D u = QVector3D::crossProduct(d, QVector3D(0, 0, 1));
    if (u.length() < 1e-3f)
        u = QVector3D(1, 0, 0);
    u.normalize();
    const QVector3D v = QVector3D::crossProduct(d, u).normalized();

    const double rate = starTurnRate(fx, vals);
    if (rate != 0.0)
        markOpticsAnimating();
    const double turn = qDegreesToRadians(rate * opticsTime());
    const double k = qBound(0.0, level, 1.0);
    const QColor hsv = colour.toHsv();
    const QColor star = QColor::fromHsvF(qMax(0.0, double(hsv.hsvHueF())), hsv.hsvSaturationF() * 0.9,
                                         qMin(1.0, 0.35 + 0.65 * k));
    QColor ray = colour;
    ray.setAlphaF(qMin(0.3, 0.10 * m_haze * k));

    /* Spread over the disc evenly (a sunflower spiral), each nudged by a hash
       of its index so it reads as a sky, not a pattern -- and the same sky
       every frame, so it never shimmers. */
    static const int N = 260;
    const double spread = qTan(qMin(halfAngleRad, qDegreesToRadians(80.0)));
    QVector<QPointF> pts;
    QVector<double> depths;
    pts.reserve(N);
    depths.reserve(N);
    double depthSum = 0.0;
    for (int i = 0; i < N; ++i)
    {
        double j = qSin((i + 1) * 12.9898 + fid * 78.233) * 43758.5453;
        j -= qFloor(j);
        const double r = spread * qSqrt((i + 0.5) / N) * (0.85 + 0.3 * j);
        const double a = i * 2.399963 + turn + j * 0.5;
        const QVector3D rd = (d + u * float(r * qCos(a)) + v * float(r * qSin(a))).normalized();
        const QVector3D at = apex + rd * float(beamThrow(apex, rd));
        pts << w2s(at);
        const double z = viewDepth(at) + 0.002;
        depths << z;
        depthSum += z;
        if (m_haze > 0.05 && i % 3 == 0)
            emitLine(w2s(apex), w2s(at), viewDepth(apex), viewDepth(at), ray, 0.6, p);
    }
    emitDots(pts, depthSum / N, 1.6, star, p, depths);
}

void StructureStudioView::drawBeamCone(QPainter &p, const QVector3D &apex,
                                      const QVector3D &dir, double halfAngleRad,
                                      const QColor &colour, double level,
                                      double haze, const BeamOptics::Optics *optics) const
{
    const bool gobo = optics != nullptr && !optics->mask.isNull();
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
    QVector3D landNormal(0, 0, 1);
    int axisSurface = 0;
    const double len = beamThrow(apex, d, &lands, &landNormal, &axisSurface);

    /* A pool lies just off its surface, toward the eye -- unless it is on the
       far side of a cloth. A drape has no thickness, so a pool on its back
       (steps lighting it from upstage) lifted toward the eye sat in front of
       the cloth and showed through it. On the far side it sinks instead. */
    const Trig &tr = angledTrig();
    const QVector3D eye(float(tr.sA * tr.cE), float(tr.cA * tr.cE), float(tr.sE));
    MonitorProperties *props = m_doc->monitorProperties();
    auto poolLift = [&](int surface, const QVector3D &normal) {
        if (surface < 0 && QVector3D::dotProduct(normal, eye) < 0.0f)
        {
            const Scenery *sc = props->scenery(quint32(-1 - surface));
            if (sc != nullptr && !sc->isSeeThrough())
                return -1e-4;
        }
        return 1e-4;
    };
    const double axisLift = poolLift(axisSurface, landNormal);

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
    /* A gobo stops most of the light: the air carries what it lets through
       (the pool below shows WHERE). */
    /* A flood spreads the same light over far more of the room than a spot:
       its glow and its pool are that much fainter (spreadFactor()). Thirty-
       two 150-degree bars at full drew as thirty-two spots and washed the
       whole view out white. */
    const double spread = spreadFactor(halfAngleRad);
    const double baseA = 95.0 * level * room * spread * qBound(0.0, haze, 1.0) * m_haze
                         * (gobo ? qBound(0.05, optics->coverage, 1.0) : 1.0);
    const double poolA = 130.0 * level * room * spread;

    // Where it ends: the pool's shape, and the far end of the cone's outline.
    const QVector<QVector3D> footprint = beamFootprint(apex, d, halfAngleRad, len, lands, landNormal);

    /* How its pool lands ("the XL-450s .. they're profiling as spots, not
       washes"). The beam angle is where a real beam is at half its middle's
       brightness; a wash goes on past it, fading out to its field angle, and
       a profile stops almost at it. So the pool runs out to the field edge
       -- the beam angle times fieldMult -- full in its core and fading to
       nothing at the rim, half way down at the beam angle itself. */
    const double soft = optics != nullptr ? qBound(0.0, optics->softness, 1.5) : 0.0;
    const double fieldMult = 1.0 + 0.4 * soft;
    const double coreR = qMax(0.0, 2.0 / fieldMult - 1.0);  // of the field radius
    auto poolFade = [coreR](double r) {                      // r: 0 axis .. 1 field edge
        if (r <= coreR)
            return 1.0;
        return qBound(0.0, (1.0 - r) / qMax(1e-6, 1.0 - coreR), 1.0);
    };

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
        for (int i = 0; i < spans; ++i)
        {
            const HullPt &a = rim.at(i);
            const HullPt &b = rim.at((i + 1) % rim.size());
            emitLightTri(lensPx, a.p, b.p, lensZ, a.z + lift, b.z + lift,
                         1.0, a.a, b.a, c, p);
        }
    }

    /* Does ALL of it land on the one surface its middle does? The pool used
       to be wherever the middle of the beam landed, as one oval on that
       surface: a follow-spot whose middle went just past the end of the cyc
       washed none of it, though half the beam was on the cloth ("the spots
       aren't both backwashing"). The edge of the beam is asked too; when its
       rays end on different things -- the cyc and the floor beyond, a deck
       and the floor below its edge -- the pool is laid ray by ray instead,
       each part on what it reaches. */
    const double halfA = qBound(0.005, halfAngleRad * fieldMult, 1.4);
    QVector3D side = QVector3D::crossProduct(QVector3D(0, 0, 1), d);
    if (side.length() < 1e-3f)
        side = QVector3D(1, 0, 0);
    side.normalize();
    const QVector3D up = QVector3D::crossProduct(d, side).normalized();
    auto rayAt = [&](double r, double a) {
        const QVector3D across = side * float(qCos(a)) + up * float(qSin(a));
        return (d * float(qCos(halfA * r)) + across * float(qSin(halfA * r))).normalized();
    };
    bool split = false;
    for (int i = 0; i < 16 && !split; ++i)
    {
        bool h = false;
        int s = 0;
        beamThrow(apex, rayAt(1.0, 2.0 * M_PI * i / 16), &h, nullptr, &s);
        split = (h != lands) || (h && s != axisSurface);
    }

    /* The pool, only where the beam actually arrives on something -- lying ON
       that surface, so each point carries its own depth: an oval a couple of
       metres long spans real depth, and one number for all of it sank the far
       end into the deck. */
    if (split)
    {
        /* A mesh of rays across the beam, each thrown to whatever it meets.
           A cell is lit only where its corners all land on the same surface,
           so the pool stops at the cyc's edge or the deck's, and goes on on
           the next thing. A gobo's mask still says how much each point gets. */
        /* As fine as it shows: the cells only need to be small ON SCREEN.
           A pool a few dozen pixels across -- most of them, in a whole-rig
           view -- was 960 triangles at the finest grid, and a lit rig's
           frame was mostly these. Sized from the pool's reach on screen, a
           close-up still gets the full grid (and a gobo always does). */
        int RINGS = 16, SECTORS = 48;
        if (!gobo)
        {
            const QPointF c = w2s(apex + d * float(len));
            double reach = 0.0;
            for (const QVector3D &q : footprint)
                reach = qMax(reach, QLineF(c, w2s(q)).length());
            reach *= fieldMult;                       // out to the field edge
            RINGS = qBound(4, int(reach / 20.0), 8);
            SECTORS = qBound(16, int(2.0 * M_PI * reach / 20.0), 32);
        }
        const int N = (RINGS + 1) * SECTORS;
        QVector<QPointF> px(N);
        QVector<double> pz(N), pt(N);
        QVector<int> surf(N);
        QVector<bool> landed(N);
        /* Where the mesh lands is kept from frame to frame, like each ray
           (m_throws): a beam that hasn't moved lands where it did, and only
           its projection to the screen is asked again. A gobo's mask can
           turn, so a gobo's mesh is worked out every frame. */
        const BeamMesh *kept = nullptr;
        BeamMeshKey key;
        if (!gobo && m_decksValid)
        {
            key = BeamMeshKey{ { apex.x(), apex.y(), apex.z(), d.x(), d.y(), d.z(),
                                 float(halfA), float(coreR) }, RINGS, SECTORS };
            auto it = m_beamMeshes.constFind(key);
            if (it != m_beamMeshes.constEnd())
                kept = &it.value();
        }
        if (kept != nullptr)
        {
            // Marked as used now, or -- on a drawing thread -- once all are done.
            if (s_emit != nullptr)
                s_emit->meshHits << kept;
            else
                const_cast<BeamMesh *>(kept)->frame = m_throwFrame;
            for (int k = 0; k < N; ++k)
            {
                px[k] = w2s(kept->q.at(k));
                pz[k] = viewDepth(kept->q.at(k)) + poolLift(kept->surf.at(k), kept->normal.at(k));
            }
            surf = kept->surf;
            landed = kept->landed;
            pt = kept->fade;
        }
        else
        {
            BeamMesh fresh;
            fresh.q.resize(N);
            fresh.normal.resize(N);
            for (int ri = 0; ri <= RINGS; ++ri)
                for (int si = 0; si < SECTORS; ++si)
                {
                    const double r = double(ri) / RINGS;
                    const double a = 2.0 * M_PI * si / SECTORS;
                    const QVector3D ray = (ri == 0) ? d : rayAt(r, a);
                    bool h = false;
                    int sf = 0;
                    QVector3D nrm(0, 0, 1);
                    const double run = beamThrow(apex, ray, &h, &nrm, &sf);
                    const QVector3D q = apex + ray * float(run);
                    const int k = ri * SECTORS + si;
                    fresh.q[k] = q;
                    fresh.normal[k] = nrm;
                    px[k] = w2s(q);
                    pz[k] = viewDepth(q) + poolLift(sf, nrm);
                    surf[k] = sf;
                    landed[k] = h;
                    const double rs = qMin(r, 0.985);
                    pt[k] = gobo ? BeamOptics::sample(optics->mask, rs * qCos(a), rs * qSin(a))
                                 : poolFade(r);
                }
            if (!gobo && m_decksValid)
            {
                fresh.surf = surf;
                fresh.landed = landed;
                fresh.fade = pt;
                fresh.frame = m_throwFrame;
                if (s_emit != nullptr)
                    s_emit->newMeshes << qMakePair(key, fresh);
                else
                    m_beamMeshes.insert(key, fresh);
            }
        }
        QColor cellCol = colour;
        cellCol.setAlpha(qBound(0, int(poolA), 220));
        auto tri = [&](int i, int j, int k) {
            if (!landed[i] || !landed[j] || !landed[k])
                return;
            if (surf[i] != surf[j] || surf[i] != surf[k])
                return;                                   // straddles an edge
            if (pt[i] < 0.04 && pt[j] < 0.04 && pt[k] < 0.04)
                return;                                   // all metal here
            emitLightTri(px[i], px[j], px[k], pz[i], pz[j], pz[k],
                         pt[i], pt[j], pt[k], cellCol, p);
        };
        for (int ri = 0; ri < RINGS; ++ri)
            for (int si = 0; si < SECTORS; ++si)
            {
                const int s1 = (si + 1) % SECTORS;
                const int i00 = ri * SECTORS + si, i01 = ri * SECTORS + s1;
                const int i10 = (ri + 1) * SECTORS + si, i11 = (ri + 1) * SECTORS + s1;
                if (ri > 0)
                    tri(i00, i10, i01);
                tri(i01, i10, i11);
            }
    }
    else if (lands && gobo)
    {
        /* "set a gobo and projected on the cyc .. and it's not visible." The
           pool is the gobo's image, so it is drawn as a mesh of small cells
           on the surface, each as bright as the mask lets through at that
           point of the beam -- the same edge rays as the plain pool, at a
           fraction of the cone's angle, so it lies on the cyc or the deck
           exactly where the plain one would. */
        static const int RINGS = 16, SECTORS = 48;
        const QVector3D ln = landNormal.isNull() ? QVector3D(0, 0, 1) : landNormal.normalized();
        const double planeD = double(QVector3D::dotProduct(d * float(len), ln));
        const double maxRun = qMin(24.0, len * 4.0);
        auto landAt = [&](double r, double a) {
            const QVector3D ray = rayAt(r, a);
            double run = maxRun;
            const double approach = double(QVector3D::dotProduct(ray, ln));
            if (planeD * approach > 1e-8)
                run = qMin(maxRun, planeD / approach);
            return apex + ray * float(qMax(0.0, run));
        };
        /* The grid of landed points, ring by ring (ring 0 is the axis), each
           with how much light the mask lets through THERE. Alpha is carried
           per corner and each cell is two triangles, whose fade the
           rasteriser fits exactly -- so a gobo's edges come out smooth rather
           than stepped at the cell size. */
        const int N = (RINGS + 1) * SECTORS;
        QVector<QPointF> px(N);
        QVector<double> pz(N), pt(N);
        for (int ri = 0; ri <= RINGS; ++ri)
            for (int si = 0; si < SECTORS; ++si)
            {
                const double r = double(ri) / RINGS;
                const double a = 2.0 * M_PI * si / SECTORS;
                const QVector3D q = landAt(r, a);
                const int k = ri * SECTORS + si;
                px[k] = w2s(q);
                pz[k] = viewDepth(q) + axisLift;
                // Just inside the rim, so the outer ring isn't sampled dark.
                const double rs = qMin(r, 0.985);
                pt[k] = BeamOptics::sample(optics->mask, rs * qCos(a), rs * qSin(a));
            }
        QColor cellCol = colour;
        cellCol.setAlpha(qBound(0, int(poolA), 220));
        auto tri = [&](int i, int j, int k) {
            if (pt[i] < 0.04 && pt[j] < 0.04 && pt[k] < 0.04)
                return;                                   // all metal here
            emitLightTri(px[i], px[j], px[k], pz[i], pz[j], pz[k],
                         pt[i], pt[j], pt[k], cellCol, p);
        };
        for (int ri = 0; ri < RINGS; ++ri)
            for (int si = 0; si < SECTORS; ++si)
            {
                const int s1 = (si + 1) % SECTORS;
                const int i00 = ri * SECTORS + si, i01 = ri * SECTORS + s1;
                const int i10 = (ri + 1) * SECTORS + si, i11 = (ri + 1) * SECTORS + s1;
                if (ri > 0)                    // ring 0 is one point: one triangle
                    tri(i00, i10, i01);
                tri(i01, i10, i11);
            }
    }
    else if (lands)
    {
        /* The core at full, then rings fading out to the field edge: a few
           triangles a ring with their alpha per corner, which the rasteriser
           fades smoothly -- far cheaper than the gobo's mesh, which a plain
           pool doesn't need. */
        QColor poolCol = colour;
        poolCol.setAlpha(qBound(0, int(poolA), 220));
        const double fieldHalf = halfAngleRad * fieldMult;
        static const double kRings[] = { 0.0, 0.5, 1.0 };   // of the fade, core to rim
        QVector<QVector<QPointF>> ringPx;
        QVector<QVector<double>> ringZ;
        QVector<double> ringA;
        for (double f : kRings)
        {
            const double r = coreR + (1.0 - coreR) * f;
            const QVector<QVector3D> ring = (f == 0.0 && coreR >= 0.999)
                ? footprint : beamFootprint(apex, d, fieldHalf * r, len, lands, landNormal);
            QVector<QPointF> px;
            QVector<double> zs;
            for (const QVector3D &q : ring)
            {
                px << w2s(q);
                zs << viewDepth(q) + axisLift;
            }
            ringPx << px;
            ringZ << zs;
            ringA << poolFade(r);
        }
        // The core.
        {
            double sum = 0.0;
            for (double z : ringZ.first())
                sum += z;
            emitLight(QPolygonF(ringPx.first()), sum / qMax(1, ringZ.first().size()),
                      poolCol, p, ringZ.first());
        }
        // The fade, ring to ring.
        for (int k = 0; k + 1 < ringPx.size(); ++k)
        {
            const QVector<QPointF> &in = ringPx.at(k), &out = ringPx.at(k + 1);
            const QVector<double> &zi = ringZ.at(k), &zo = ringZ.at(k + 1);
            const int n = qMin(in.size(), out.size());
            for (int i = 0; i < n; ++i)
            {
                const int j = (i + 1) % n;
                emitLightTri(in[i], in[j], out[i], zi[i], zi[j], zo[i],
                             ringA[k], ringA[k], ringA[k + 1], poolCol, p);
                emitLightTri(in[j], out[j], out[i], zi[j], zo[j], zo[i],
                             ringA[k], ringA[k + 1], ringA[k + 1], poolCol, p);
            }
        }
    }
}

double StructureStudioView::opticsTime() const
{
    if (m_opticsTime >= 0.0)
        return m_opticsTime;
    if (!m_opticsClock.isValid())
        m_opticsClock.start();
    return m_opticsClock.elapsed() / 1000.0;
}

QVector<QVector3D> StructureStudioView::beamFootprint(const QVector3D &apex, const QVector3D &dir,
                                                     double halfAngleRad, double length,
                                                     bool lands, const QVector3D &landNormal) const
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
    // The surface the axis landed on: through axisEnd, facing landNormal (up
    // for the floor or a deck, across the stage for a cyc).
    const QVector3D ln = landNormal.isNull() ? QVector3D(0, 0, 1) : landNormal.normalized();
    const double planeD = double(QVector3D::dotProduct(axisEnd - apex, ln));
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
        // Same sign as the axis's own approach: the edge ray reaches the surface.
        const double approach = double(QVector3D::dotProduct(ray, ln));
        if (planeD * approach > 1e-8)
            run = qMin(maxRun, planeD / approach);
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
       just shifts it half a body off the spot it was put on.

       And not on a truss or pipe. The elevations draw a mover there CENTRED
       on its rig point (and the hit test agrees), so that is where a rigger
       drags one to sit it just under the bar, and the drop (mountZOffset)
       records it. Seating it here as well hung every such head half a body
       lower in the rig view than in the truss editor, clear of the bar it is
       clamped to ("the truss editor shows the fixtures just under the cross
       member .. but the rig view shows them down a bit"). Only what the
       elevations seat is seated: a tower shelf (towerFixtureBodyRect()), a
       deck, a riser -- surfaces a body really stands on or hangs flush from. */
    const bool seatsOnSurface = rp.onTower() || rp.onDeck() || rp.onRiser();
    const double seat = (rp.placement == FixtureRigProps::Inside
                         || seatsOnSurface == false) ? 0.0 : 0.5;
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

            /* A wash's LEDs on its face ("36x18w .. I need to be able to set
               that"): the array behind its lenses, each a point of the beam's
               colour while it is lit, dark glass when it is not. Packed in a
               sunflower spiral -- even for any count, the same every frame --
               and shared out between the heads of a multi-head fixture. */
            int layout = QLCPhysical::ClusterLeds;
            const int faceLeds = RigHeads::lightLeds(m_doc, fid, 0, &layout);
            if (faceLeds > 0 && layout == QLCPhysical::FaceLeds)
            {
                const int perHead = qMax(1, faceLeds / units);
                const QVector3D face = headMid + beam * float(h * 0.26);
                const double radius = qMin(slice, d) * 0.26 * 0.9;
                const double pitch = radius * qSqrt(M_PI / perHead);
                QColor led(46, 46, 50);
                if (look.beamLevel > 0.01)
                {
                    const QColor lc = look.beamColour.isValid() ? look.beamColour : col;
                    const double k = qBound(0.0, look.beamLevel, 1.0);
                    led = QColor::fromRgbF(float(0.18 + (lc.redF() - 0.18) * k),
                                           float(0.18 + (lc.greenF() - 0.18) * k),
                                           float(0.20 + (lc.blueF() - 0.20) * k));
                }
                QVector<QPointF> dots;
                dots.reserve(perHead);
                for (int i = 0; i < perHead; ++i)
                {
                    const double rr = radius * qSqrt((i + 0.5) / perHead);
                    const double a = i * 2.399963;
                    dots << w2s(face + side * float(rr * qCos(a)) + up * float(rr * qSin(a)));
                }
                // Just in front of the face it sits on; behind the head when
                // the face is turned away.
                emitDots(dots, viewDepth(face) + 0.003, qMax(0.8, pitch * 0.36 * m_scale), led, p);
            }

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
                // This head's share of what the fixture puts out.
                bool known = false;
                const double out = fixtureOutput(m_doc->fixture(fid), &known);
                const double headOutput = known ? out / qMax(1, traits.headCount) : 1.0;
                if (traits.stars)
                {
                    // A sky laser on a yoke: its star field, where the head points.
                    Fixture *sfx = m_doc->fixture(fid);
                    const double starDeg = (beamAngle > 0.0 && beamAngle < 170.0) ? beamAngle : 100.0;
                    drawStarField(p, fid, sfx, sfx != nullptr ? sfx->channelValues() : QByteArray(),
                                  lens, beam, qDegreesToRadians(starDeg / 2.0), lightColour,
                                  look.beamLevel * headOutput);
                }
                else if (traits.laser)
                {
                    /* Laser heads: pencil beams, fanned across the lens angle --
                       beams per head of them -- from the one aperture. */
                    for (int b = 0; b < beams; ++b)
                    {
                        const double a = (beams > 1) ? qDegreesToRadians(deg) * ((b / double(beams - 1)) - 0.5)
                                                     : 0.0;
                        drawLaserBeam(p, lens, beam * float(qCos(a)) + side * float(qSin(a)),
                                      lightColour, look.beamLevel * headOutput);
                    }
                }
                else
                for (int b = 0; b < beams; ++b)
                {
                    const QVector3D apex = lens + side * float((b - (beams - 1) * 0.5) * pitch);
                    drawBeamCone(p, apex, beam, qDegreesToRadians(deg / 2.0),
                                 lightColour, look.beamLevel * headOutput, haze, &look.optics);
                    beamsOut() << FrameBeam{ apex, beam.normalized(),
                                               qDegreesToRadians(deg / 2.0),
                                               beamThrow(apex, beam.normalized()),
                                               lightColour, look.beamLevel * headOutput };
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
double StructureStudioView::spreadFactor(double half)
{
    static const double ref = 2.0 * M_PI * (1.0 - qCos(qDegreesToRadians(20.0)));   // 40-degree cone
    const double omega = 2.0 * M_PI * (1.0 - qCos(qBound(0.001, half, M_PI_2 * 0.99)));
    return qBound(0.03, ref / omega, 1.0);
}

double StructureStudioView::lightFactor(const FrameBeam &fb)
{
    /* A lantern spreads its light over a whole sphere: a metre away a surface
       gets about half what a 40-degree spot of the same output puts on its
       subject a few metres off -- and less with the square of the distance
       (beamCoverage()). A hundred-lumen lantern lights its own bracket, not
       the stage. */
    return fb.omni ? 0.5 : spreadFactor(fb.halfAngle);
}

double StructureStudioView::beamCoverage(const FrameBeam &fb, const QVector3D &at)
{
    if (fb.omni)
    {
        const double d = qMax(0.25, double((at - fb.apex).length()));
        return d > fb.length ? 0.0 : qMin(4.0, 1.0 / (d * d));
    }
    if (fb.apertured)
    {
        /* Out through the top or not at all: the walls of the box stop
           everything else. The line from the light to the point has to rise
           through the opening. */
        if (at.z() <= fb.az || fb.apex.z() >= fb.az)
            return 0.0;
        const float s = (fb.az - fb.apex.z()) / (at.z() - fb.apex.z());
        const QVector3D q = fb.apex + (at - fb.apex) * s;
        if (q.x() < fb.ax0 || q.x() > fb.ax1 || q.y() < fb.ay0 || q.y() > fb.ay1)
            return 0.0;
    }
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
}

QVector3D StructureStudioView::beamLightAt(const QVector3D &at, const QVector3D &n) const
{
    /* Beams only ever join m_frameBeams during a frame (it is cleared when
       one starts), so the bounds of the ones not seen yet are added on. */
    if (m_beamBounds.size() > m_frameBeams.size())
        m_beamBounds.clear();
    for (qsizetype i = m_beamBounds.size(); i < m_frameBeams.size(); ++i)
    {
        const FrameBeam &fb = m_frameBeams.at(i);
        BeamBound b;
        if (fb.omni)
        {
            b.centre = fb.apex;
            b.reach2 = float(fb.length * fb.length);
        }
        else if (fb.halfAngle >= 1.5)
        {
            b.centre = fb.apex;                  // near flat: no telling
            b.reach2 = std::numeric_limits<float>::infinity();
        }
        else
        {
            // The cone as far as beamCoverage() reaches, soft edge included.
            const double len = fb.length + 0.25;
            const double rim = len * qTan(fb.halfAngle);
            const double r = rim + qMax(0.03, rim * 0.25);
            b.centre = fb.apex + fb.dir * float(len * 0.5);
            b.reach2 = float(len * len * 0.25 + r * r) * 1.0001f;
        }
        b.r = float(fb.colour.red());
        b.g = float(fb.colour.green());
        b.b = float(fb.colour.blue());
        b.factor = float(fb.level * 1.15 * lightFactor(fb));
        m_beamBounds << b;
    }

    QVector3D sum;
    for (qsizetype i = 0; i < m_frameBeams.size(); ++i)
    {
        const BeamBound &b = m_beamBounds.at(i);
        if ((at - b.centre).lengthSquared() > b.reach2)
            continue;
        const FrameBeam &fb = m_frameBeams.at(i);
        const double cover = beamCoverage(fb, at);
        if (cover <= 0.0)
            continue;
        /* Lambert, to where the light actually comes FROM: a surface facing
           the source takes it, one turned away does not. The beam's axis is a
           fair stand-in for a distant spot, but a light a few centimetres
           under a step is all directions at once -- the front of a shin above
           a bar fired straight up is lit only at a graze. */
        QVector3D toward = (fb.apertured || fb.omni) ? (fb.apex - at) : -fb.dir;
        if (toward.lengthSquared() < 1e-12f)
            continue;
        toward.normalize();
        const double facing = double(qMax(0.0f, QVector3D::dotProduct(n, toward)));
        const float k = b.factor * float(cover * facing);
        sum += QVector3D(b.r, b.g, b.b) * k;
    }
    return sum;
}

void StructureStudioView::collectInsideLight(QPainter &p) const
{
    if (!m_liveValues)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr)
            continue;
        const FixtureRigProps rp = props->fixtureRigProps(fx->id());
        if (rp.placement != FixtureRigProps::Inside)
            continue;
        const quint32 plId = rp.onRiser() ? rp.riserPlatformId
                           : rp.onDeck()  ? rp.deckPlatformId
                                          : FixtureRigProps::invalidPlatformId();
        StagePlatform *pl = props->platform(plId);
        if (pl == nullptr || pl->topMaterial() == StagePlatform::SolidTop)
            continue;                    // a solid lid lets nothing out

        // The block of the platform it is in: a flight of steps has several.
        const QVector3D centre = fixtureCentre(fx->id());
        const float base = props->platformBaseZ(plId);
        StagePlatform::Block blk = pl->blocks().value(0);
        const QPointF loc = pl->toLocal(QPointF(double(centre.x()), double(centre.y())));
        foreach (const StagePlatform::Block &bk, pl->blocks())
            if (loc.x() >= bk.x0 && loc.x() <= bk.x1 && loc.y() >= bk.y0 && loc.y() <= bk.y1)
                blk = bk;
        const float topZ = base + blk.height;

        bool complete = true;
        const QByteArray vals = valuesForDrawing(fx, &complete);
        if (vals.isEmpty())
            continue;
        const FixtureVisualTraits traits = classifyFixture(fx);
        QVector3D L, N, H;
        fixtureFrame(rp, L, N, H);
        if (N.z() <= 0.05f)
            continue;                    // firing down or sideways: not out of the top
        const double declared = fixtureBeamAngle(fx, vals, traits);
        const double half = qDegreesToRadians(((declared >= 170.0) ? 150.0
                                               : (declared > 0.0) ? declared : 40.0) / 2.0);
        const double len = fixtureLenM(fx->id());

        const int heads = qMax(1, traits.headCount);
        const int cols = traits.layout.isValid() ? traits.layout.width() : heads;
        const int rows = traits.layout.isValid() ? traits.layout.height() : 1;
        const QVector<int> cellHead = headsByCell(fx, cols, rows, heads);
        const int master = fixtureMasterLevel(fx, vals);

        /* Per column along the bar: what is lit there, so a bar lit in
           SECTIONS shows each section's footprint, not one span from the
           first lit pixel to the last over the dark ones between. */
        QVector<double> colR(cols, 0.0), colG(cols, 0.0), colB(cols, 0.0), colL(cols, 0.0);
        QVector<int> colN(cols, 0);
        int litCount = 0;
        bool outputKnown = false;
        const double output = fixtureOutput(fx, &outputKnown);
        const double headOutput = outputKnown ? output / heads : 1.0;   // each pixel's share
        for (int cell = 0; cell < cellHead.size(); ++cell)
        {
            const int h = cellHead.at(cell);
            if (h < 0)
                continue;
            QColor c;
            uchar dim = 0;
            if (!fixtureHeadLiveState(fx, vals, h, c, dim, master) || dim <= 2)
                continue;
            const int col = cell % cols;
            const float u = float(((col + 0.5) / cols - 0.5) * len);   // along the bar
            FrameBeam fb{ centre + L * u, N, half, 10.0, c, dim / 255.0 * headOutput };
            fb.apertured = true;
            fb.ax0 = blk.x0; fb.ax1 = blk.x1; fb.ay0 = blk.y0; fb.ay1 = blk.y1; fb.az = topZ;
            beamsOut() << fb;
            /* The colour carries the level; the footprint wants hue x level. */
            const double cmax = qMax(c.redF(), qMax(c.greenF(), c.blueF()));
            if (cmax > 0.0)
            {
                const double k = (dim / 255.0) / cmax;
                colR[col] += c.redF() * k; colG[col] += c.greenF() * k; colB[col] += c.blueF() * k;
            }
            colL[col] += dim / 255.0;
            colN[col]++;
            ++litCount;
        }
        if (litCount == 0)
            continue;
        const float t = (topZ - centre.z()) / N.z();
        if (t < 0.0f)
            continue;
        QVector3D along(L.x(), L.y(), 0.0f);
        if (along.lengthSquared() < 1e-6f)
            along = QVector3D(1, 0, 0);
        along.normalize();
        const QVector3D across(-along.y(), along.x(), 0.0f);
        const float spread = qMax(float(t * qTan(half)), 0.04f);
        const float reach = spread / qMax(0.2f, N.z());           // a tipped cone lands long
        const float pitch = float(len / cols);

        /* Where the light comes through, a stretch at a time: each run of
           lit columns carried along the facing to the top, widened by the
           cone over that throw (and never narrower than the bar), clipped to
           the opening. A bar tipped upstage lights the top a little upstage
           of itself. */
        auto clip = [](const QVector<QPointF> &in, int axis, double lim, bool keepAbove) {
            QVector<QPointF> out;
            for (int i = 0; i < in.size(); ++i)
            {
                const QPointF cur = in.at(i), prev = in.at((i + in.size() - 1) % in.size());
                const double cv = axis == 0 ? cur.x() : cur.y(), pv = axis == 0 ? prev.x() : prev.y();
                const bool cin = keepAbove ? cv >= lim : cv <= lim;
                const bool pin = keepAbove ? pv >= lim : pv <= lim;
                if (cin != pin)
                    out << prev + (cur - prev) * ((lim - pv) / (cv - pv));
                if (cin)
                    out << cur;
            }
            return out;
        };
        int c0 = 0;
        while (c0 < cols)
        {
            if (colN.at(c0) == 0) { ++c0; continue; }
            int c1 = c0;
            double sr = 0, sg = 0, sb = 0, sl = 0;
            int n = 0;
            while (c1 < cols && colN.at(c1) > 0)
            {
                sr += colR.at(c1); sg += colG.at(c1); sb += colB.at(c1);
                sl += colL.at(c1); n += colN.at(c1);
                ++c1;
            }
            // Columns c0 .. c1-1 are lit: their edges along the bar.
            const float uA = float(((double(c0) / cols) - 0.5) * len);
            const float uB = float(((double(c1) / cols) - 0.5) * len);
            c0 = c1;
            const QVector3D a2 = centre + L * uA + N * t, b2 = centre + L * uB + N * t;
            const float grow = qMax(0.0f, spread - 0.5f * pitch);
            QVector<QPointF> quad = {
                QPointF(a2.x() - along.x() * grow - across.x() * reach, a2.y() - along.y() * grow - across.y() * reach),
                QPointF(b2.x() + along.x() * grow - across.x() * reach, b2.y() + along.y() * grow - across.y() * reach),
                QPointF(b2.x() + along.x() * grow + across.x() * reach, b2.y() + along.y() * grow + across.y() * reach),
                QPointF(a2.x() - along.x() * grow + across.x() * reach, a2.y() - along.y() * grow + across.y() * reach) };
            quad = clip(quad, 0, blk.x0, true);
            quad = clip(quad, 0, blk.x1, false);
            quad = clip(quad, 1, blk.y0, true);
            quad = clip(quad, 1, blk.y1, false);
            const double peak = qMax(sr, qMax(sg, sb));
            if (quad.size() < 3 || peak <= 0.0)
                continue;
            const double level = qMin(1.0, sl / n);
            QColor glow = QColor::fromRgbF(float(sr / peak), float(sg / peak), float(sb / peak))
                              .lighter(int(100 + 40 * level));
            glow.setAlpha(qBound(0, int(215 + 35 * level), 250));
            QPolygonF poly;
            QVector<double> vz;
            double depth = 0.0;
            for (const QPointF &q : quad)
            {
                const QVector3D w(float(q.x()), float(q.y()), topZ + 0.002f);
                poly << w2s(w);
                vz << viewDepth(w);
                depth += viewDepth(w) / quad.size();
            }
            emitPoly(poly, depth, glow, QColor(), 0.0, p, vz);

            /* In haze, the air over the opening glows: a sheet of lit air
               rising off the stretch, bright at the top and fading upward --
               by how bright the pixels are, not their lumens. */
            if (m_haze > 0.0)
            {
                const double haze = qMin(1.5, m_haze);
                const double room = 1.25 - 0.65 * m_ambient;
                const float rise = float(0.25 + 0.30 * haze);
                const QVector3D s0 = centre + L * uA + N * t, s1 = centre + L * uB + N * t;
                const QVector3D lift(0.0f, 0.0f, rise);
                const QVector3D sheet[4] = { QVector3D(s0.x(), s0.y(), topZ), QVector3D(s1.x(), s1.y(), topZ),
                                             QVector3D(s1.x(), s1.y(), topZ) + lift,
                                             QVector3D(s0.x(), s0.y(), topZ) + lift };
                QColor air = QColor::fromRgbF(float(sr / peak), float(sg / peak), float(sb / peak));
                air.setAlpha(qBound(0, int(120.0 * level * haze * room), 200));
                QPolygonF sp;
                QVector<double> sz;
                double sd = 0.0;
                for (const QVector3D &v : sheet)
                {
                    sp << w2s(v);
                    sz << viewDepth(v);
                    sd += viewDepth(v) / 4.0;
                }
                emitLight(sp, sd, air, p, sz, QVector<double>{ 1.0, 1.0, 0.0, 0.0 });
            }

            /* And where it lands beyond the top: a cyc upstage of the steps
               catches the upstage edge of their cones. Drawn as light, like
               a beam's pool, when beams are shown. */
            if (m_beams)
            {
                FrameBeam ap{ QVector3D(), N, half, 10.0, QColor(), 0.0 };
                ap.apertured = true;
                ap.ax0 = blk.x0; ap.ax1 = blk.x1; ap.ay0 = blk.y0; ap.ay1 = blk.y1; ap.az = topZ;
                const QVector3D ha = centre + L * uA, hb = centre + L * uB;
                const double room = 1.25 - 0.65 * m_ambient;
                QColor wash = QColor::fromRgbF(float(sr / peak), float(sg / peak), float(sb / peak));
                // The section's share of the bar's output, as for its beams.
                wash.setAlpha(qBound(0, int(130.0 * (outputKnown ? (sl / heads) * output : level) * room), 220));
                foreach (const QVector<QVector3D> &patch, washOnScenery(ha, hb, N, half, ap))
                {
                    if (patch.size() < 3)
                        continue;
                    QPolygonF wp;
                    QVector<double> wz;
                    double sum = 0.0;
                    for (const QVector3D &q : patch)
                    {
                        wp << w2s(q);
                        const double z = viewDepth(q) + 1e-4;
                        wz << z;
                        sum += z;
                    }
                    emitLight(wp, sum / patch.size(), wash, p, wz);
                }
            }
        }
    }
}

QImage StructureStudioView::patternImage(const QString &path) const
{
    if (path.isEmpty())
        return QImage();
    auto it = m_patterns.constFind(path);
    if (it != m_patterns.constEnd())
        return it.value();
    QImage img(path);
    if (!img.isNull())
        img = img.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                 .convertToFormat(QImage::Format_ARGB32);
    m_patterns.insert(path, img);               // a missing file is not retried every frame
    return img;
}

void StructureStudioView::emitPatternedFace(QPainter &p, const QVector3D q[4], const QImage &img,
        const QColor &plain, double opacity,
        const std::function<QColor(const QColor &, const QVector3D &)> &shade,
        double depthBias) const
{
    // Patches about four pixels across on screen, at most what the image has.
    const double wPx = QLineF(w2s(q[0]), w2s(q[1])).length();
    const double hPx = QLineF(w2s(q[0]), w2s(q[3])).length();
    const int nu = img.isNull() ? 1 : qBound(1, int(wPx / 4.0), qMin(48, img.width()));
    const int nv = img.isNull() ? 1 : qBound(1, int(hPx / 4.0), qMin(96, img.height()));
    auto at = [&](double u, double v) {
        const QVector3D lo = q[0] + (q[1] - q[0]) * float(u);
        const QVector3D hi = q[3] + (q[2] - q[3]) * float(u);
        return lo + (hi - lo) * float(v);
    };
    for (int iu = 0; iu < nu; ++iu)
    {
        for (int iv = 0; iv < nv; ++iv)
        {
            const double u0 = double(iu) / nu, u1 = double(iu + 1) / nu;
            const double v0 = double(iv) / nv, v1 = double(iv + 1) / nv;
            const QVector3D quad[4] = { at(u0, v0), at(u1, v0), at(u1, v1), at(u0, v1) };
            const QVector3D mid = at((u0 + u1) / 2, (v0 + v1) / 2);
            QColor albedo = plain;
            if (!img.isNull())
            {
                // The image upright on the face: its top at the face's top.
                const int x = qBound(0, int((u0 + u1) / 2 * img.width()), img.width() - 1);
                const int y = qBound(0, int((1.0 - (v0 + v1) / 2) * img.height()), img.height() - 1);
                // The print over the plain colour: by its opacity, and its own alpha.
                const QColor print = img.pixelColor(x, y);
                const double a = qBound(0.0, opacity * print.alphaF(), 1.0);
                albedo = QColor::fromRgbF(float(plain.redF() * (1 - a) + print.redF() * a),
                                          float(plain.greenF() * (1 - a) + print.greenF() * a),
                                          float(plain.blueF() * (1 - a) + print.blueF() * a));
            }
            QPolygonF poly;
            QVector<double> zs;
            for (const QVector3D &v : quad)
            {
                poly << w2s(v);
                zs << viewDepth(v) + depthBias;
            }
            emitPoly(poly, viewDepth(mid) + depthBias, shade(albedo, mid), QColor(), 0.0, p, zs);
        }
    }
}

void StructureStudioView::drawTowerFaces(QPainter &p) const
{
    // A deck top or a fabric is see-through and still where the pointer is.
    QScopedValueRollback<bool> pickable(emitPickable(), true);
    if (m_plane != Angled)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    QList<Tower *> towers;
    if (m_kind == StageKind)
    {
        foreach (Tower *tw, props->towers())
            if (tw != nullptr && layerShown(tw->layerId()))
                towers << tw;
    }
    else if (m_kind == TowerKind && props->tower(m_id) != nullptr)
        towers << props->tower(m_id);
    const double af = 0.18 + 0.82 * m_ambient;

    foreach (Tower *t, towers)
    {
        if (t == nullptr || !t->hasFaces() || t->facesSides() == 0
            || t->facesTo() - t->facesFrom() < 0.01f)
            continue;
        // A hair outside the steel, so the panels cover it.
        const float pad = 0.004f;
        const float x0 = t->originX() - pad, y0 = t->originY() - pad;
        const float x1 = t->originX() + t->width() + pad, y1 = t->originY() + t->depth() + pad;
        const float z0 = t->facesFrom(), z1 = t->facesTo();
        const QImage pattern = patternImage(t->facesPattern());
        const QColor plain = t->facesColour();

        struct Side { int bit; QVector3D q[4]; QVector3D n; };
        // Each seen from outside: bottom-left, bottom-right, top-right, top-left.
        const Side sides[4] = {
            { Tower::FrontSide, { {x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1} }, { 0, 1, 0 } },
            { Tower::BackSide,  { {x1, y0, z0}, {x0, y0, z0}, {x0, y0, z1}, {x1, y0, z1} }, { 0, -1, 0 } },
            { Tower::LeftSide,  { {x1, y1, z0}, {x1, y0, z0}, {x1, y0, z1}, {x1, y1, z1} }, { 1, 0, 0 } },
            { Tower::RightSide, { {x0, y0, z0}, {x0, y1, z0}, {x0, y1, z1}, {x0, y0, z1} }, { -1, 0, 0 } } };
        const double yaw = qDegreesToRadians(double(t->rotation()));
        for (Side side : sides)
        {
            if (!(t->facesSides() & side.bit))
                continue;
            // Turned with the tower.
            for (QVector3D &q : side.q)
                q = turned(t, q.x(), q.y(), q.z());
            const QVector3D n(float(double(side.n.x()) * qCos(yaw) - double(side.n.y()) * qSin(yaw)),
                              float(double(side.n.x()) * qSin(yaw) + double(side.n.y()) * qCos(yaw)), 0.0f);
            emitPatternedFace(p, side.q, pattern, plain, t->facesPatternOpacity(),
                [&](const QColor &albedo, const QVector3D &at) {
                    // The room on it, and whatever of the rig is pointed at it.
                    const QVector3D lit = beamLightAt(at, n) / 255.0f;
                    const double r = albedo.redF() * (af * 0.9 + double(lit.x()));
                    const double g = albedo.greenF() * (af * 0.9 + double(lit.y()));
                    const double b = albedo.blueF() * (af * 0.9 + double(lit.z()));
                    return QColor::fromRgbF(float(qMin(1.0, r)), float(qMin(1.0, g)), float(qMin(1.0, b)));
                }, 0.0);
        }
    }
}

bool StructureStudioView::underSock(const FixtureRigProps &rp) const
{
    if (rp.placement != FixtureRigProps::Inside || rp.trussId == Truss::invalidId())
        return false;
    const Truss *t = m_doc->monitorProperties()->truss(rp.trussId);
    return t != nullptr && t->sockCovers(rp.trussOffset);
}

QVector<QVector3D> StructureStudioView::sockGlow(Truss *t, int segments) const
{
    QVector<QVector3D> out(qMax(1, segments));
    if (t == nullptr || !t->hasSock())
        return out;
    MonitorProperties *props = m_doc->monitorProperties();

    // The light of every bar rigged Inside under it, as points along it.
    QVector<BarLight> lights;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        const FixtureRigProps rp = props->fixtureRigProps(fx->id());
        if (rp.trussId != t->id() || !underSock(rp))
            continue;
        bool complete = true;
        const QByteArray vals = valuesForDrawing(fx, &complete);
        if (vals.isEmpty())
            continue;
        const FixtureVisualTraits traits = classifyFixture(fx);
        const QVector3D centre = fixtureCentre(fx->id());
        if (traits.headCount > 1)
            lights += barLights(fx, rp, traits, vals, centre);
        else
        {
            QColor c;
            uchar dim = 0;
            if (fixtureLiveState(fx, vals, c, dim, complete) && dim > 2)
                lights << BarLight{ centre, c, dim / 255.0 * fixtureOutput(fx), dim / 255.0 };
        }
    }
    if (lights.isEmpty())
        return out;

    /* Each stretch glows with what is near it: a lit section of a bar lights
       the fabric over it and a little either side, not the whole sock. The
       fabric's own colour filters it. */
    const QColor fabric = t->sockColour();
    const float from = t->sockFrom(), to = t->sockTo();
    for (int k = 0; k < out.size(); ++k)
    {
        const float along = from + (to - from) * (k + 0.5f) / out.size();
        const QVector3D at = t->positionAt(along);
        double r = 0.0, g = 0.0, b = 0.0;
        for (const BarLight &bl : lights)
        {
            const double d = double((bl.at - at).length());
            const double fall = 1.0 / (1.0 + (d / 0.25) * (d / 0.25));
            const double e = bl.level * fall * 2.5;
            r += bl.colour.redF() * e; g += bl.colour.greenF() * e; b += bl.colour.blueF() * e;
        }
        out[k] = QVector3D(float(qMin(1.0, r * fabric.redF())), float(qMin(1.0, g * fabric.greenF())),
                           float(qMin(1.0, b * fabric.blueF())));
    }
    return out;
}

void StructureStudioView::drawSocks(QPainter &p) const
{
    // A deck top or a fabric is see-through and still where the pointer is.
    QScopedValueRollback<bool> pickable(emitPickable(), true);
    if (m_plane != Angled)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    QList<Truss *> trusses;
    if (m_kind == StageKind)
    {
        foreach (Truss *t, props->trusses())
            if (t != nullptr && layerShown(t->layerId()))
                trusses << t;
    }
    else if (m_kind == TrussKind && props->truss(m_id) != nullptr)
        trusses << props->truss(m_id);
    const double af = 0.18 + 0.82 * m_ambient;

    foreach (Truss *t, trusses)
    {
        if (t == nullptr || !t->hasSock() || t->isChildBar() || t->sockTo() - t->sockFrom() < 0.01f)
            continue;
        // The same oriented section as the truss itself (drawOneStructure()).
        const QVector3D A = t->origin();
        QVector3D L = t->positionAt(t->length()) - A;
        if (L.length() < 1e-6f)
            continue;
        L.normalize();
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
            C = (dl > 1e-9) ? QVector3D(float(-d.y() / dl), float(d.x() / dl), 0.0f) : QVector3D(0, 1, 0);
            U = QVector3D(0, 0, 1);
        }
        // A little proud of the chords: fabric stretched over them.
        const float hw = qMax(0.05f, t->width()) * 0.5f * 1.06f;
        const QVector3D c1 = C * hw, u1 = U * hw;
        const QVector3D ring[4] = { -c1 - u1, c1 - u1, c1 + u1, -c1 + u1 };

        const float from = t->sockFrom(), to = t->sockTo();
        const int segments = qBound(1, int((to - from) / 0.2f + 0.5f), 200);
        const QVector<QVector3D> glow = sockGlow(t, segments);
        const QColor fabric = t->sockColour();

        auto shade = [&](const QVector3D &g) {
            /* The room on the fabric, and what glows through it -- the glow
               taking over as it grows, so a red-lit white sock reads red,
               not pink. */
            const double lit = qMin(1.0, double(qMax(g.x(), qMax(g.y(), g.z()))) * 1.5);
            const double room = af * 0.85 * (1.0 - 0.8 * lit);
            const double r = qMin(1.0, fabric.redF() * room + g.x());
            const double gg = qMin(1.0, fabric.greenF() * room + g.y());
            const double b = qMin(1.0, fabric.blueF() * room + g.z());
            QColor c = QColor::fromRgbF(float(r), float(gg), float(b));
            c.setAlpha(232);                    // fabric: the lattice a shadow through it
            return c;
        };
        auto face = [&](const QVector3D quad[4], const QColor &fill) {
            QPolygonF poly;
            QVector<double> zs;
            QVector3D mid;
            for (int i = 0; i < 4; ++i)
            {
                poly << w2s(quad[i]);
                zs << viewDepth(quad[i]) - 2e-4;
                mid += quad[i];
            }
            emitPoly(poly, viewDepth(mid / 4.0f) - 2e-4, fill, QColor(), 0.0, p, zs);
        };
        const QImage pattern = patternImage(t->sockPattern());
        if (!pattern.isNull())
        {
            /* Printed: the image stretched along each side of the sock, the
               fabric's print where its colour was, still glowing from within
               (the glow, filtered by the print). */
            for (int f = 0; f < 4; ++f)
            {
                const QVector3D quad[4] = { t->positionAt(from) + ring[f], t->positionAt(to) + ring[f],
                                            t->positionAt(to) + ring[(f + 1) % 4],
                                            t->positionAt(from) + ring[(f + 1) % 4] };
                emitPatternedFace(p, quad, pattern, fabric, t->sockPatternOpacity(),
                    [&](const QColor &print, const QVector3D &at) {
                        const float along = QVector3D::dotProduct(at - A, L);
                        const int k = qBound(0, int((along - from) / qMax(0.01f, to - from) * segments),
                                             segments - 1);
                        const QVector3D g = glow.at(k);
                        const double lit = qMin(1.0, double(qMax(g.x(), qMax(g.y(), g.z()))) * 1.5);
                        const double room = af * 0.85 * (1.0 - 0.8 * lit);
                        // White fabric's glow is the light; a print filters it.
                        const double fr = fabric.redF() > 0 ? print.redF() / fabric.redF() : 0.0;
                        const double fg = fabric.greenF() > 0 ? print.greenF() / fabric.greenF() : 0.0;
                        const double fb = fabric.blueF() > 0 ? print.blueF() / fabric.blueF() : 0.0;
                        QColor c = QColor::fromRgbF(float(qMin(1.0, print.redF() * room + g.x() * qMin(1.0, fr))),
                                                    float(qMin(1.0, print.greenF() * room + g.y() * qMin(1.0, fg))),
                                                    float(qMin(1.0, print.blueF() * room + g.z() * qMin(1.0, fb))));
                        c.setAlpha(232);
                        return c;
                    }, -2e-4);
            }
        }
        else
        for (int k = 0; k < segments; ++k)
        {
            const QVector3D s0 = t->positionAt(from + (to - from) * k / segments);
            const QVector3D s1 = t->positionAt(from + (to - from) * (k + 1) / segments);
            const QColor fill = shade(glow.at(k));
            for (int f = 0; f < 4; ++f)
            {
                const QVector3D quad[4] = { s0 + ring[f], s1 + ring[f], s1 + ring[(f + 1) % 4],
                                            s0 + ring[(f + 1) % 4] };
                face(quad, fill);
            }
        }
        // Its ends, closed where it stops short of the truss's.
        const QVector3D e0 = t->positionAt(from), e1 = t->positionAt(to);
        const QVector3D cap0[4] = { e0 + ring[0], e0 + ring[1], e0 + ring[2], e0 + ring[3] };
        const QVector3D cap1[4] = { e1 + ring[0], e1 + ring[1], e1 + ring[2], e1 + ring[3] };
        face(cap0, shade(glow.first()));
        face(cap1, shade(glow.last()));
    }
}

QVector<StructureStudioView::Occluder> StructureStudioView::occluders() const
{
    /* What can stand between a light and the eye: opaque scenery as it hangs
       now (a drape's open or flown part is not in its pieces(), so it stops
       nothing; a scrim is seen through), and every block of every platform.
       Each with the box it fits in, so a light can pass over everything not
       within its reach without asking. Once a frame, while painting. */
    if (m_decksValid)
        return m_occluders;
    QVector<Occluder> out;
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (Scenery *sc, props->sceneries())
    {
        if (sc == nullptr || sc->isSeeThrough() || !layerShown(sc->layerId()))
            continue;
        Occluder o;
        o.scenery = sc;
        bool first = true;
        const double half = double(sc->thickness()) / 2.0;
        for (const Scenery::Piece &pc : sc->pieces())
            for (int i = 0; i <= 32; ++i)
            {
                const double u = pc.u0 + (pc.u1 - pc.u0) * i / 32.0;
                for (double off : { -half, half })
                    for (float z : { pc.z0, pc.z1 })
                    {
                        const QVector3D w = sc->pointAt(u, double(z), off);
                        o.lo = first ? w : QVector3D(qMin(o.lo.x(), w.x()), qMin(o.lo.y(), w.y()),
                                                     qMin(o.lo.z(), w.z()));
                        o.hi = first ? w : QVector3D(qMax(o.hi.x(), w.x()), qMax(o.hi.y(), w.y()),
                                                     qMax(o.hi.z(), w.z()));
                        first = false;
                    }
            }
        if (first)
            continue;                       // nothing of it hangs anywhere
        // A bowed cloth bulges between the samples: a little to spare.
        o.lo -= QVector3D(0.05f, 0.05f, 0.05f);
        o.hi += QVector3D(0.05f, 0.05f, 0.05f);
        out << o;
    }
    for (const Deck &deck : decks())
    {
        if (deck.platform == nullptr)
            continue;
        for (const StagePlatform::Block &bk : deck.blocks)
        {
            Occluder o;
            o.platform = deck.platform;
            o.block = bk;
            o.baseZ = deck.baseZ;
            const QList<QPointF> cs = deck.platform->corners(bk);
            double x0 = cs.first().x(), x1 = x0, y0 = cs.first().y(), y1 = y0;
            for (const QPointF &c : cs)
            {
                x0 = qMin(x0, c.x()); x1 = qMax(x1, c.x());
                y0 = qMin(y0, c.y()); y1 = qMax(y1, c.y());
            }
            o.lo = QVector3D(float(x0), float(y0), float(deck.baseZ));
            o.hi = QVector3D(float(x1), float(y1), float(deck.baseZ + double(bk.height)));
            out << o;
        }
    }
    return out;
}

double StructureStudioView::opaqueTowardTheEye(const QVector3D &from, const QVector3D &eye,
                                              double maxT, quint32 hostPlatform,
                                              QVector3D *normal) const
{
    double best = maxT;
    bool hit = false;
    QVector3D bestN;
    const QPointF o2(double(from.x()), double(from.y()));
    const QVector<Occluder> all = occluders();      // const: shares, never copies
    for (const Occluder &oc : all)
    {
        // Out of reach: further from the light than it has to look.
        const double dx = qMax(0.0, qMax(double(oc.lo.x() - from.x()), double(from.x() - oc.hi.x())));
        const double dy = qMax(0.0, qMax(double(oc.lo.y() - from.y()), double(from.y() - oc.hi.y())));
        const double dz = qMax(0.0, qMax(double(oc.lo.z() - from.z()), double(from.z() - oc.hi.z())));
        if (dx * dx + dy * dy + dz * dz >= best * best)
            continue;

        if (oc.scenery != nullptr)
        {
            double t = 0.0;
            QVector3D n;
            if (oc.scenery->rayHit(from, eye, t, best, 16, &n) && t > 1e-4)
            {
                best = t;
                bestN = n;
                hit = true;
            }
            continue;
        }

        /* A platform's block: a box, turned with its platform. Not the one
           the light is fixed to -- its body is drawn past that one's near
           face too (the depth bias in drawOneFixture()). */
        StagePlatform *pl = oc.platform;
        if (pl->id() == hostPlatform)
            continue;
        const StagePlatform::Block &bk = oc.block;
        const QPointF lo = pl->toLocal(o2);
        const QPointF ld = pl->toLocal(o2 + QPointF(double(eye.x()), double(eye.y()))) - lo;
        const double org[3] = { lo.x(), lo.y(), double(from.z()) };
        const double dir[3] = { ld.x(), ld.y(), double(eye.z()) };
        const double mn[3] = { double(bk.x0), double(bk.y0), oc.baseZ };
        const double mx[3] = { double(bk.x1), double(bk.y1), oc.baseZ + double(bk.height) };
        double tNear = -1e30, tFar = 1e30;
        int axis = -1;
        bool miss = false;
        for (int k = 0; k < 3 && !miss; ++k)
        {
            if (qAbs(dir[k]) < 1e-12)
            {
                miss = (org[k] < mn[k] || org[k] > mx[k]);
                continue;
            }
            double t0 = (mn[k] - org[k]) / dir[k], t1 = (mx[k] - org[k]) / dir[k];
            if (t0 > t1)
                qSwap(t0, t1);
            if (t0 > tNear) { tNear = t0; axis = k; }
            tFar = qMin(tFar, t1);
            miss = (tNear > tFar);
        }
        // Starting inside a block says nothing about what is in front.
        if (miss || axis < 0 || tNear <= 1e-4 || tNear >= best)
            continue;
        const QVector3D at = from + eye * float(tNear + 1e-3);
        if (!pl->containsPoint(at.x(), at.y()))   // off a rounded front's cut corners
            continue;
        best = tNear;
        hit = true;
        const double sgn = dir[axis] > 0.0 ? -1.0 : 1.0;   // facing back along the ray
        if (axis == 2)
            bestN = QVector3D(0.0f, 0.0f, float(sgn));
        else
        {
            const QPointF a = pl->toWorld(axis == 0 ? QPointF(1.0, 0.0) : QPointF(0.0, 1.0))
                              - pl->toWorld(QPointF(0.0, 0.0));
            bestN = QVector3D(float(a.x() * sgn), float(a.y() * sgn), 0.0f);
        }
    }
    if (normal != nullptr)
        *normal = bestN.normalized();
    return hit ? best : -1.0;
}

void StructureStudioView::drawHazeGlow(QPainter &p, const QVector<BarLight> &sources,
                                       quint32 hostPlatform) const
{
    if (sources.isEmpty() || m_haze <= 0.0)
        return;
    const double haze = qMin(1.5, m_haze);
    const double room = 1.25 - 0.65 * m_ambient;
    // Overlapping halos add up: a bar's samples share its glow.
    const double share = 1.0 / qSqrt(double(sources.size()));
    const double radius = 0.25 + 0.25 * haze;               // metres of lit air
    const int n = 20;
    // Toward the eye: viewDepth() grows by one for each metre along it.
    const Trig &tr = angledTrig();
    const QVector3D eye(float(tr.sA * tr.cE), float(tr.cA * tr.cE), float(tr.sE));
    for (const BarLight &src : sources)
    {
        const double peak = qMax(src.colour.redF(), qMax(src.colour.greenF(), src.colour.blueF()));
        if (peak <= 0.004 || src.brightness <= 0.01)
            continue;
        QColor glow = QColor::fromRgbF(float(src.colour.redF() / peak), float(src.colour.greenF() / peak),
                                       float(src.colour.blueF() / peak));
        glow.setAlpha(qBound(0, int(150.0 * src.brightness * haze * room * share), 220));
        const QPointF c = w2s(src.at);
        // Its size on screen, the same however the view turns.
        const double rPx = qMax(3.0, 0.5 * (QLineF(c, w2s(src.at + QVector3D(float(radius), 0, 0))).length()
                                            + QLineF(c, w2s(src.at + QVector3D(0, 0, float(radius)))).length()));
        // The lit air between us and it: in front of its body, not inside it.
        const double z = viewDepth(src.at) + radius;   // larger is nearer

        /* ...but not in front of what stands between us and it. Lifted a flat
           half-metre toward the eye, the glow of a row of steps a foot behind
           the closed main drape sorted in front of the drape and shone through
           it. Where something opaque is nearer than that, the glow lies just
           behind its face instead -- following the face's slant across the
           disc, so a drape seen at an angle hides all of it -- and the face
           covers it as it covers the step. Nothing in the way: as it was. */
        QVector3D faceN;
        const double t = opaqueTowardTheEye(src.at, eye, radius, hostPlatform, &faceN);
        bool onFace = false;
        double fd0 = 0.0, fgx = 0.0, fgy = 0.0;      // the face's depth across the screen
        QPointF fs0;
        if (t >= 0.0)
        {
            const QVector3D at = src.at + eye * float(t);
            QVector3D a = QVector3D::crossProduct(faceN, QVector3D(0, 0, 1));
            if (a.lengthSquared() < 1e-6f)
                a = QVector3D(1, 0, 0);
            a.normalize();
            const QVector3D b = QVector3D::crossProduct(faceN, a).normalized();
            fs0 = w2s(at);
            fd0 = viewDepth(at);
            const QPointF s1 = w2s(at + a) - fs0, s2 = w2s(at + b) - fs0;
            const double d1 = viewDepth(at + a) - fd0, d2 = viewDepth(at + b) - fd0;
            const double det = s1.x() * s2.y() - s1.y() * s2.x();
            if (qAbs(det) > 1e-9)
            {
                fgx = (d1 * s2.y() - d2 * s1.y()) / det;
                fgy = (s1.x() * d2 - s2.x() * d1) / det;
                onFace = true;
            }
        }
        const double behind = 0.02;                   // metres behind the face
        auto depthAt = [&](const QPointF &q) {
            if (t < 0.0)
                return z;
            if (!onFace)                                  // the face edge-on: its one depth
                return qMin(z, fd0 - behind);
            return qMin(z, fd0 + fgx * (q.x() - fs0.x()) + fgy * (q.y() - fs0.y()) - behind);
        };
        const double zc = depthAt(c);

        /* Laid behind a face, the disc reaches below the floor where the face
           meets it -- and the floor is not in the depth buffer, so that part
           showed as a strip of glow on the floor in front of the drape. Air
           under the floor is not lit: where the face hides the light, clip the
           disc to what is above the floor. The world height of what is drawn
           at a point of the screen at a depth is affine in the three. */
        QMatrix4x4 toWorld;
        bool clipFloor = false;
        if (t >= 0.0)
        {
            const QPointF ex = w2s(src.at + QVector3D(1, 0, 0)) - c;
            const QPointF ey = w2s(src.at + QVector3D(0, 1, 0)) - c;
            const QPointF ez = w2s(src.at + QVector3D(0, 0, 1)) - c;
            const QMatrix4x4 toScreen(float(ex.x()), float(ey.x()), float(ez.x()), 0.0f,
                                      float(ex.y()), float(ey.y()), float(ez.y()), 0.0f,
                                      eye.x(), eye.y(), eye.z(), 0.0f,
                                      0.0f, 0.0f, 0.0f, 1.0f);
            toWorld = toScreen.inverted(&clipFloor);
        }
        const double z0 = viewDepth(src.at);
        auto heightAt = [&](const QPointF &q, double zq) {
            const QVector3D dw = toWorld.map(QVector3D(float(q.x() - c.x()), float(q.y() - c.y()),
                                                       float(zq - z0)));
            return double(src.at.z()) + double(dw.z());
        };

        // A fan, bright at the source and nothing at its rim.
        QPointF prev(c.x() + rPx, c.y());
        double zPrev = depthAt(prev);
        for (int i = 0; i < n; ++i)
        {
            const double a1 = 2.0 * M_PI * (i + 1) / n;
            const QPointF next(c.x() + rPx * qCos(a1), c.y() + rPx * qSin(a1));
            const double zNext = depthAt(next);
            const QPointF vp[3] = { c, prev, next };
            const double vz[3] = { zc, zPrev, zNext };
            const double va[3] = { 1.0, 0.0, 0.0 };
            prev = next;
            zPrev = zNext;
            double vh[3] = { 1.0, 1.0, 1.0 };
            int below = 0;
            if (clipFloor)
                for (int k = 0; k < 3; ++k)
                    if ((vh[k] = heightAt(vp[k], vz[k])) < 0.0)
                        ++below;
            if (below == 0)
            {
                // One mesh for the whole fan (emitLightTri() joins them).
                emitLightTri(vp[0], vp[1], vp[2], vz[0], vz[1], vz[2], va[0], va[1], va[2], glow, p);
                continue;
            }
            if (below == 3)
                continue;
            QPointF cp[4];
            double cz[4], ca[4];
            int m = 0;
            for (int k = 0; k < 3; ++k)
            {
                const int j = (k + 1) % 3;
                if (vh[k] >= 0.0)
                {
                    cp[m] = vp[k]; cz[m] = vz[k]; ca[m] = va[k];
                    ++m;
                }
                if ((vh[k] >= 0.0) != (vh[j] >= 0.0))      // crosses the floor
                {
                    const double f = vh[k] / (vh[k] - vh[j]);
                    cp[m] = vp[k] + (vp[j] - vp[k]) * f;
                    cz[m] = vz[k] + (vz[j] - vz[k]) * f;
                    ca[m] = va[k] + (va[j] - va[k]) * f;
                    ++m;
                }
            }
            for (int k = 1; k + 1 < m; ++k)
                emitLightTri(cp[0], cp[k], cp[k + 1], cz[0], cz[k], cz[k + 1],
                             ca[0], ca[k], ca[k + 1], glow, p);
        }
    }
}

QVector<StructureStudioView::BarLight> StructureStudioView::barLights(Fixture *fx,
        const FixtureRigProps &rp, const FixtureVisualTraits &traits,
        const QByteArray &vals, const QVector3D &centre) const
{
    QVector<BarLight> out;
    const int heads = qMax(1, traits.headCount);
    const int cols = traits.layout.isValid() ? traits.layout.width() : heads;
    const int rows = traits.layout.isValid() ? traits.layout.height() : 1;
    const QVector<int> cellHead = headsByCell(fx, cols, rows, heads);
    const int master = fixtureMasterLevel(fx, vals);
    QVector3D L, N, H;
    fixtureFrame(rp, L, N, H);
    const double len = fixtureLenM(fx->id());

    // What is lit in each column along the bar (all its rows together).
    QVector<double> cr(cols, 0.0), cg(cols, 0.0), cb(cols, 0.0), cl(cols, 0.0);
    QVector<int> cn(cols, 0);
    for (int cell = 0; cell < cellHead.size(); ++cell)
    {
        const int h = cellHead.at(cell);
        QColor c;
        uchar dim = 0;
        if (h < 0 || !fixtureHeadLiveState(fx, vals, h, c, dim, master) || dim <= 2)
            continue;
        const int col = cell % cols;
        cr[col] += c.redF(); cg[col] += c.greenF(); cb[col] += c.blueF();
        cl[col] += dim / 255.0;
        cn[col]++;
    }
    int c0 = 0;
    while (c0 < cols)
    {
        if (cn.at(c0) == 0) { ++c0; continue; }
        int c1 = c0;
        double r = 0, g = 0, b = 0, lv = 0;
        int n = 0;
        while (c1 < cols && cn.at(c1) > 0)
        {
            r += cr.at(c1); g += cg.at(c1); b += cb.at(c1); lv += cl.at(c1); n += cn.at(c1);
            ++c1;
        }
        // Lit columns c0 .. c1-1: up to six cones spread evenly along them.
        const int span = c1 - c0;
        const int samples = qBound(1, (span + 3) / 4, 6);
        /* The section's share of the bar's light -- how lit its pixels are,
           and how many of the bar's pixels they are -- times what the bar
           puts out (fixtureOutput()), spread over its cones. */
        bool known = false;
        const double output = fixtureOutput(fx, &known);
        const double level = (known ? (lv / heads) * output : lv / n) / samples;
        const QColor colour = QColor::fromRgbF(float(qMin(1.0, r / n)), float(qMin(1.0, g / n)),
                                               float(qMin(1.0, b / n)));
        for (int k = 0; k < samples; ++k)
        {
            const double colPos = c0 + (span * (k + 0.5)) / samples;          // in columns
            const float u = float((colPos / cols - 0.5) * len);
            out << BarLight{ centre + L * u, colour, level, qMin(1.0, lv / n) };
        }
        c0 = c1;
    }
    return out;
}

QVector<QVector<QVector3D> > StructureStudioView::washOnScenery(const QVector3D &a, const QVector3D &b,
                                                                const QVector3D &dir, double half,
                                                                const FrameBeam &aperture) const
{
    QVector<QVector<QVector3D> > out;
    MonitorProperties *props = m_doc->monitorProperties();
    QList<Scenery *> sceneries;
    foreach (Scenery *sc, props->sceneries())
        if (sc != nullptr && layerShown(sc->layerId()))
            sceneries << sc;
    if (sceneries.isEmpty())
        return out;

    // A ray leaves through the top, then lands on the first scenery it meets.
    auto cast = [&](const QVector3D &from, const QVector3D &d, int &which, QVector3D &at) -> bool {
        if (d.z() <= 1e-4f)
            return false;
        const float s = (aperture.az - from.z()) / d.z();
        const QVector3D q = from + d * s;
        if (q.x() < aperture.ax0 || q.x() > aperture.ax1 || q.y() < aperture.ay0 || q.y() > aperture.ay1)
            return false;
        double best = 24.0;
        which = -1;
        for (int i = 0; i < sceneries.size(); ++i)
        {
            double t = 0.0;
            if (sceneries.at(i) != nullptr && sceneries.at(i)->rayHit(from, d, t, best, 16) && t > 0.05)
            {
                best = t;
                which = i;
            }
        }
        if (which < 0)
            return false;
        at = from + d * float(best);
        return true;
    };

    QVector3D u = QVector3D::crossProduct(dir, QVector3D(0, 0, 1));
    if (u.lengthSquared() < 1e-6f)
        u = QVector3D(1, 0, 0);
    u.normalize();
    const QVector3D v = QVector3D::crossProduct(dir, u).normalized();
    auto ray = [&](double r, double ang) {
        const double th = half * r;
        return (dir * float(qCos(th)) + (u * float(qCos(ang)) + v * float(qSin(ang))) * float(qSin(th))).normalized();
    };

    QHash<int, QVector<QVector3D> > hits;      // per piece of scenery
    static const int RINGS = 4, SECTORS = 32;
    const QVector3D apexes[2] = { a, b };
    for (const QVector3D &from : apexes)
    {
        // Rings out to the edge; between a hit and a miss, find the edge.
        QVector<int> lastRing(SECTORS, -2);
        for (int ri = 0; ri <= RINGS; ++ri)
        {
            const double r = double(ri) / RINGS;
            const int n = (ri == 0) ? 1 : SECTORS;
            QVector<int> ring(SECTORS, -2);
            for (int si = 0; si < n; ++si)
            {
                const double ang = 2.0 * M_PI * si / SECTORS;
                int w = -1;
                QVector3D at;
                const bool hit = cast(from, ray(r, ang), w, at);
                if (hit)
                    hits[w] << at;
                ring[si] = hit ? w : -1;
                if (ri == 0)
                    for (int k = 0; k < SECTORS; ++k) ring[k] = ring[0];
                /* Radially (from the ring inside) and around (from the
                   neighbour): where one lands and the other does not, the
                   edge of the patch -- the cyc's top, say -- lies between. */
                auto refine = [&](double r0, double a0, double r1, double a1, bool hit0) {
                    for (int it = 0; it < 6; ++it)
                    {
                        const double rm = 0.5 * (r0 + r1), am = 0.5 * (a0 + a1);
                        int wm = -1;
                        QVector3D atm;
                        if (cast(from, ray(rm, am), wm, atm) == hit0) { r0 = rm; a0 = am; }
                        else { r1 = rm; a1 = am; }
                    }
                    int wm = -1;
                    QVector3D atm;
                    const double rh = hit0 ? r0 : r1, ah = hit0 ? a0 : a1;
                    if (cast(from, ray(rh, ah), wm, atm))
                        hits[wm] << atm;
                };
                if (ri > 0)
                {
                    const bool inner = lastRing[si] >= 0;
                    if (inner != hit)
                        refine(double(ri - 1) / RINGS, ang, r, ang, inner);
                    if (si > 0 && (ring[si - 1] >= 0) != hit)
                        refine(r, 2.0 * M_PI * (si - 1) / SECTORS, r, ang, ring[si - 1] >= 0);
                    if (si == SECTORS - 1 && (ring[0] >= 0) != hit)
                        refine(r, ang, r, 2.0 * M_PI, hit);
                }
            }
            lastRing = ring;
        }
    }

    // Each piece's patch: the outline of where it was hit, on its own surface.
    for (auto it = hits.constBegin(); it != hits.constEnd(); ++it)
    {
        const QVector<QVector3D> &pts = it.value();
        if (pts.size() < 3)
            continue;
        QVector3D c;
        for (const QVector3D &q : pts) c += q;
        c /= float(pts.size());
        // In the cloth's own plane: along it horizontally, and up.
        QVector3D along = pts.first() - c;
        along.setZ(0.0f);
        for (const QVector3D &q : pts)
        {
            QVector3D d = q - c; d.setZ(0.0f);
            if (d.lengthSquared() > along.lengthSquared())
                along = d;
        }
        if (along.lengthSquared() < 1e-8f)
            along = QVector3D(1, 0, 0);
        along.normalize();
        QVector<QPair<QPointF, QVector3D> > p2;
        for (const QVector3D &q : pts)
            p2 << qMakePair(QPointF(double(QVector3D::dotProduct(q - c, along)), double(q.z() - c.z())), q);
        std::sort(p2.begin(), p2.end(), [](const QPair<QPointF, QVector3D> &l, const QPair<QPointF, QVector3D> &r) {
            return l.first.x() < r.first.x() || (l.first.x() == r.first.x() && l.first.y() < r.first.y());
        });
        auto cross = [](const QPointF &o, const QPointF &x, const QPointF &y) {
            return (x.x() - o.x()) * (y.y() - o.y()) - (x.y() - o.y()) * (y.x() - o.x());
        };
        QVector<QPair<QPointF, QVector3D> > h(2 * p2.size());
        int k = 0;
        for (int i = 0; i < p2.size(); ++i)
        {
            while (k >= 2 && cross(h[k - 2].first, h[k - 1].first, p2[i].first) <= 0) --k;
            h[k++] = p2[i];
        }
        for (int i = p2.size() - 2, t = k + 1; i >= 0; --i)
        {
            while (k >= t && cross(h[k - 2].first, h[k - 1].first, p2[i].first) <= 0) --k;
            h[k++] = p2[i];
        }
        QVector<QVector3D> patch;
        for (int i = 0; i < k - 1; ++i)
            patch << h[i].second;
        out << patch;
    }
    return out;
}

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

    /* Out of its FACE. A bar's light leaves the face its pixels are on -- the
       FART bars hang vertically off their truss facing the audience, and
       "hung means pointing down" sent their light to the floor behind the
       truss ("the effect on these bars appears to be going backwards .. are
       the bars set facing downstage?"). Likewise anything whose face was
       chosen. Tilt and flip included (fixtureFrame()). */
    if (traits.kind != FixtureSilhouette::Mover && (traits.headCount > 1 || rp.studioFaceChosen))
    {
        QVector3D L, N, H;
        fixtureFrame(rp, L, N, H);
        if (N.lengthSquared() > 1e-6f)
        {
            dir = N.normalized();
            return true;
        }
    }

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
            /* Only for what is in the box or on its faces. Something standing
               on top of a deck is above it, not in it: lifted to the box's
               near face it drew through whatever stands in front of it on the
               same deck -- a fountain's flame through its walls. */
            const quint32 plId = rp.onRiser() ? rp.riserPlatformId
                               : (rp.onDeck() && rp.placement == FixtureRigProps::Inside) ? rp.deckPlatformId
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
                turnCorners(hc, host);
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
            /* ...UNTIL the scene runs. Then the DMX is the truth: an effect
               that moves the heads round the target (Stage Shape, an orbit
               on an Aim look) sends them somewhere other than the target
               itself, and drawing them all converged on it hid the effect
               completely -- "if I make changes to the effect I don't see
               changes in the rig view". */
            /* One look per head. A two-arm fixture's arms pan, tilt and colour
               on their own channels, so reading the first head for both drew
               them as twins whatever the desk was doing. A scene target still
               aims every head at it. */
            const int units = qMax(1, traits.headCount);
            QVector<MoverHeadLook> looks(units);
            for (int u = 0; u < units; ++u)
                looks[u].aim = moverAim(fid, u);
            // The target, for the dashed trace below -- not once it runs.
            QVector3D aimPt;
            const bool aimed = !(m_liveValues && activeSceneRunning()) && aimPointFor(fid, aimPt);

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
            // Read with beams off too: the LEDs on a wash's face still light.
            if (showingOutput)
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
                        // The gobo and prism it throws through.
                        looks[u].optics = BeamOptics::opticsFor(fx, vals, opticsTime(), m_doc->monitorProperties());
                        // A gobo's picture is sharp, whatever the lens.
                        looks[u].optics.softness = looks[u].optics.mask.isNull() ? traits.edgeSoftness : 0.0;
                        if (looks[u].optics.animated)
                            markOpticsAnimating();
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
        else if (traits.kind == FixtureSilhouette::Lantern)
        {
            QColor live = unlit;
            uchar dim = 0;
            const bool lit = showingOutput && fixtureLiveState(fx, vals, live, dim, valsComplete);
            const double af = 0.30 + 0.70 * m_ambient;
            const QColor cage = (drag || hi) ? col
                              : QColor(qRound(70 * af), qRound(70 * af), qRound(74 * af));
            const QVector<QVector3D> ledPts =
                RigHeads::lightPoints(m_doc, fid, m_lanternPixels->deviceLeds(fx));
            LanternPixels::Frame leds;
            if (lit)
                leds = m_lanternPixels->frame(fx, vals, qMax(1, ledPts.size()), live, dim / 255.0,
                                              valsComplete);
            drawLanternSolid(p, corner, cage, live, lit ? dim / 255.0 : 0.0, depthBias,
                             ledPts, leds.leds, leds.tag, RigHeads::clearGlass(m_doc, fid));
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
        /* In haze, the air round a lit bar or par glows -- by how bright its
           pixels are, so LED strip with few lumens still shows in heavy haze
           ("with haze up and lights down for the leds I am not seeing much
           haze"). A lantern has its own halo; a mover its beam; a bar inside
           something glows where its light gets out (collectInsideLight()). */
        if (m_haze > 0.0 && showingOutput && m_plane == Angled
            && rp.placement != FixtureRigProps::Inside
            && traits.kind != FixtureSilhouette::Mover && !traits.omni && !traits.flame)
        {
            QVector<BarLight> glows;
            if (traits.headCount > 1)
                glows = barLights(fx, rp, traits, vals, fixtureCentre(fid));
            else
            {
                QColor c;
                uchar dim = 0;
                if (fixtureLiveState(fx, vals, c, dim, valsComplete) && dim > 2)
                    glows << BarLight{ fixtureCentre(fid), c, 0.0, dim / 255.0 };
            }
            drawHazeGlow(p, glows, rp.onRiser() ? rp.riserPlatformId
                                 : rp.onDeck() ? rp.deckPlatformId
                                               : FixtureRigProps::invalidPlatformId());
        }

        /* A silk flame: on or off, as its relay is -- past half on -- and when
           on, the silk rising off it in its own orange and yellow, lighting
           what is near it the same warm colour. */
        if (traits.flame)
        {
            QColor live = col;
            uchar dim = 0;
            const bool on = showingOutput && fixtureLiveState(fx, vals, live, dim, valsComplete)
                            && dim >= 128;
            if (on)
            {
                const QLCPhysical phys = (fx->fixtureMode() != nullptr) ? fx->fixtureMode()->physical()
                                                                        : QLCPhysical();
                const double w = (phys.width() > 0) ? phys.width() / 1000.0 : 0.3;
                const double h = (phys.height() > 0) ? phys.height() / 1000.0 : 0.15;
                const QVector3D c = fixtureCentre(fid);
                drawSilkFlame(p, c + QVector3D(0.0f, 0.0f, float(h / 2.0)), w,
                              qMax(0.45, 2.0 * w), 1.0, double(fid) * 1.618);
                FrameBeam fb{ c + QVector3D(0.0f, 0.0f, float(h)), QVector3D(0, 0, -1), M_PI, 6.0,
                              QColor(255, 130, 35), 0.6 * fixtureOutput(fx) };
                fb.omni = true;
                beamsOut() << fb;
                markOpticsAnimating();          // it moves: keep drawing it
            }
        }
        /* All round (a lantern): no cone to draw -- its glow is drawn with its
           body -- but what it lights near it, it lights. */
        else if (traits.omni && showingOutput)
        {
            QColor live = col;
            uchar dim = 0;
            if (fixtureLiveState(fx, vals, live, dim, valsComplete) && dim > 2)
            {
                /* What its LEDs add up to, where it is built of them: an
                   effect lights the truss in what the LEDs are showing, not
                   in the look's colour 1. */
                double level = dim / 255.0;
                const int ledCount = RigHeads::lightLeds(m_doc, fid, m_lanternPixels->deviceLeds(fx));
                if (ledCount > 0)
                {
                    const LanternPixels::Frame leds =
                        m_lanternPixels->frame(fx, vals, ledCount, live, level, valsComplete);
                    double sr = 0.0, sg = 0.0, sb = 0.0;
                    foreach (const QColor &lc, leds.leds)
                    {
                        sr += lc.redF(); sg += lc.greenF(); sb += lc.blueF();
                    }
                    const double n = qMax(1, leds.leds.size());
                    const double peak = qMax(sr, qMax(sg, sb)) / n;
                    if (peak > 0.0)
                    {
                        live = QColor::fromRgbF(float(sr / n / peak), float(sg / n / peak),
                                                float(sb / n / peak));
                        level = peak;
                    }
                    else
                        level = 0.0;
                }
                FrameBeam fb{ fixtureCentre(fid), QVector3D(0, 0, -1), M_PI, 10.0, live,
                              level * fixtureOutput(fx) };
                fb.omni = true;
                beamsOut() << fb;
            }
        }
        else if (traits.kind != FixtureSilhouette::Mover && m_beams && showingOutput
                 && !underSock(rp))   // a socked bar glows through the fabric instead
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
                /* A declared flood (180: half of everything in front of it)
                   is not a cone that can be drawn; a very wide one is. */
                const double deg = (declared >= 170.0) ? 150.0
                                 : (declared > 0.0) ? declared : 40.0;
                const QVector3D apex = fixtureCentre(fid);
                const double half = qDegreesToRadians(deg / 2.0);
                BeamOptics::Optics optics = BeamOptics::opticsFor(fx, vals, opticsTime(), m_doc->monitorProperties());
                optics.softness = optics.mask.isNull() ? traits.edgeSoftness : 0.0;
                if (optics.animated)
                    markOpticsAnimating();
                /* A bar of pixels is not a lamp in its middle ("the source
                   appears centred on the matrix, not using the individual
                   LEDs"): its light comes from the lit pixels, section by
                   section along it, each in its own colour. A row of cones
                   spread along each lit section shares that section's light;
                   a single head is the one cone it always was. */
                const double fixedLevel = dim / 255.0 * fixtureOutput(fx);
                /* A sky laser throws a field of stars, a laser pencil beams
                   -- not a cone of soft light. */
                const bool special = traits.stars || traits.laser;
                if (traits.stars)
                {
                    const double starDeg = (declared > 0.0 && declared < 170.0) ? declared : 100.0;
                    drawStarField(p, fid, fx, vals, apex, dir, qDegreesToRadians(starDeg / 2.0),
                                  live, fixedLevel);
                }
                else if (traits.laser)
                {
                    const int beams = qMax(1, traits.beamsPerHead);
                    QVector3D side = QVector3D::crossProduct(dir, QVector3D(0, 0, 1));
                    if (side.length() < 1e-3f)
                        side = QVector3D(1, 0, 0);
                    side.normalize();
                    for (int b = 0; b < beams; ++b)
                    {
                        const double a = (beams > 1) ? qDegreesToRadians(deg) * ((b / double(beams - 1)) - 0.5)
                                                     : 0.0;
                        drawLaserBeam(p, apex, dir.normalized() * float(qCos(a)) + side * float(qSin(a)),
                                      live, fixedLevel);
                    }
                }
                const QVector<BarLight> sources = (traits.headCount > 1 && !special)
                    ? barLights(fx, rp, traits, vals, apex) : QVector<BarLight>();
                if (sources.isEmpty() && !special)
                {
                    const double level = fixedLevel;
                    drawBeamCone(p, apex, dir, half, live, level, 1.0, &optics);
                    beamsOut() << FrameBeam{ apex, dir.normalized(), half,
                                               beamThrow(apex, dir.normalized()),
                                               live, level };
                }
                for (const BarLight &bl : sources)
                {
                    drawBeamCone(p, bl.at, dir, half, bl.colour, bl.level, 1.0, &optics);
                    beamsOut() << FrameBeam{ bl.at, dir.normalized(), half,
                                               beamThrow(bl.at, dir.normalized()),
                                               bl.colour, bl.level };
                }
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
        const QVector<int> cellHead = headsByCell(fx, cols, rows, traits.headCount);
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
                /* A LIT pixel fills its cell on the face, in its colour, so
                   a lit bar reads as lit ("hard to see they're lit at all"):
                   a 1.2 px dot per LED on a dark housing was all a full-blue
                   step row showed. Neighbours of one colour merge into one
                   shape, so a solid look is a shape per row. Unlit pixels
                   stay dots. */
                auto facePoint = [&](double fc, double fr) {
                    const QVector3D lo = b0 + (b1 - b0) * float(fc);
                    const QVector3D hi = f0 + (f1 - f0) * float(fc);
                    return lo + (hi - lo) * float(fr);
                };
                auto edgeOf = [](int i, int n) {          // cell edge i-0.5, in 0..1
                    return n > 1 ? qBound(0.0, (i - 0.5) / (n - 1), 1.0) : (i <= 0 ? 0.0 : 1.0);
                };
                for (int r = 0; r < rows; ++r)
                {
                    const double fr0 = edgeOf(r, rows), fr1 = edgeOf(r + 1, rows);
                    const float fr = (rows > 1) ? float(r) / (rows - 1) : 0.5f;
                    int runStart = -1;
                    QRgb runColour = 0;
                    auto flushRun = [&](int endExclusive) {
                        if (runStart < 0)
                            return;
                        const double fc0 = edgeOf(runStart, cols), fc1 = edgeOf(endExclusive, cols);
                        QPolygonF cell;
                        QVector<double> cz;
                        for (const QPair<double, double> &c : { qMakePair(fc0, fr0), qMakePair(fc1, fr0),
                                                               qMakePair(fc1, fr1), qMakePair(fc0, fr1) })
                        {
                            const QVector3D w = facePoint(c.first, c.second);
                            cell << w2s(w);
                            cz << viewDepth(w) + 0.001 + depthBias;
                        }
                        emitPoly(cell, faceDepth + 0.001, QColor::fromRgba(runColour), QColor(), 0.0, p, cz);
                        runStart = -1;
                    };
                    for (int cx = 0; cx < cols; ++cx)
                    {
                        const int placed = cellHead.value(r * cols + cx, -1);
                        QColor hc;
                        uchar hd = 0;
                        /* Only where the pixels ARE the face: a bar, a panel.
                           A mover's heads are its arms, which turn; painting
                           them onto its housing would leave them behind. */
                        const bool lit = placed >= 0 && m_liveValues
                                         && traits.kind != FixtureSilhouette::Mover
                                         && fixtureHeadLiveState(fx, vals, placed, hc, hd, master) && hd > 2;
                        if (lit)
                        {
                            const QRgb rgb = pixelColor(fx, vals, placed, col, unlit, visibility, master)
                                                 .lighter(135).rgba();
                            if (runStart >= 0 && rgb != runColour)
                                flushRun(cx);
                            if (runStart < 0) { runStart = cx; runColour = rgb; }
                            continue;
                        }
                        flushRun(cx);
                        if (placed < 0)
                            continue;
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
                    flushRun(cols);
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
    else if (traits.kind == FixtureSilhouette::Lantern)
    {
        // A lantern: its cage as a rounded outline, the lit core inside.
        const double wPx = (traits.physW > 0.0f) ? qMax(8.0, double(traits.physW) * m_scale) : 10.0;
        const double hPx = (traits.physH > 0.0f) ? qMax(10.0, double(traits.physH) * m_scale) : wPx * 1.5;
        const QRectF r(c.x() - wPx * 0.5, c.y() - hPx * 0.5, wPx, hPx);
        if (hi)
        {
            QPen halo(QColor(120, 220, 140, 160)); halo.setWidth(3);
            p.setPen(halo); p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(-3, -3, 3, 3), wPx * 0.4, wPx * 0.4);
        }
        const QVector<QVector3D> ledPts =
            RigHeads::lightPoints(m_doc, fid, m_lanternPixels->deviceLeds(fx));
        const bool clear = RigHeads::clearGlass(m_doc, fid);
        QColor glass(40, 40, 44, 90);
        if (!clear)
        {
            /* Diffusing glass: the body glows in what is lit behind it. */
            QColor live = col;
            uchar dim = 0;
            if (showingOutput && fixtureLiveState(fx, vals, live, dim, valsComplete) && dim > 2)
            {
                const LanternPixels::Frame leds = m_lanternPixels->frame(
                    fx, vals, qMax(1, ledPts.size()), live, dim / 255.0, valsComplete);
                double sr = 0.0, sg = 0.0, sb = 0.0;
                foreach (const QColor &lc, leds.leds)
                {
                    sr += lc.redF(); sg += lc.greenF(); sb += lc.blueF();
                }
                const double n = qMax(1, leds.leds.size());
                const double peak = qMax(sr, qMax(sg, sb)) / n;
                if (peak > 0.004)
                {
                    glass = QColor::fromRgbF(float(sr / n / peak), float(sg / n / peak),
                                             float(sb / n / peak));
                    glass.setAlpha(qBound(60, int(60 + 150 * peak), 210));
                }
            }
            else
                glass = QColor(200, 196, 188, 50);           // frosted
        }
        p.setPen(QPen(QColor(90, 90, 96), 1.4));
        p.setBrush(glass);
        p.drawRoundedRect(r, wPx * 0.4, wPx * 0.4);
        p.setPen(Qt::NoPen);
        if (ledPts.isEmpty())
        {
            p.setBrush(col);
            p.drawEllipse(c, wPx * 0.28, wPx * 0.28);
        }
        else
        {
            // Built of LEDs: each in what it is showing, beads when dark.
            QColor live = col;
            uchar dim = 0;
            LanternPixels::Frame leds;
            if (showingOutput && fixtureLiveState(fx, vals, live, dim, valsComplete))
                leds = m_lanternPixels->frame(fx, vals, ledPts.size(), live, dim / 255.0,
                                              valsComplete);
            const double ledR = qMax(1.0, wPx * 0.05);
            for (int i = 0; i < ledPts.size(); ++i)
            {
                const QColor lc = leds.leds.value(i, QColor(Qt::black));
                const double peak = qMax(lc.redF(), qMax(lc.greenF(), lc.blueF()));
                QColor bead = peak > 0.004
                              ? QColor::fromRgbF(float(lc.redF() / peak), float(lc.greenF() / peak),
                                                 float(lc.blueF() / peak))
                              : QColor(96, 90, 80);
                if (!clear)
                    bead.setAlpha(peak > 0.004 ? 150 : 70);
                p.setBrush(bead);
                p.drawEllipse(w2s(ledPts.at(i)), clear ? ledR : ledR * 1.5, clear ? ledR : ledR * 1.5);
            }
            if (!leds.tag.isEmpty())
            {
                p.setPen(QColor(230, 200, 120));
                p.drawText(QPointF(r.right() + 4, r.bottom()), leds.tag);
                p.setPen(Qt::NoPen);
            }
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
        p.setPen(Qt::NoPen);
        p.setBrush(body);
        p.drawPolygon(silhouette);

        /* An outline you can find. The rim used to be the housing grey a shade
           lighter -- grey on grey, inside a truss drawn in grey -- so a bar in
           a tower was a column of dots with no edges ("really hard to discern
           where the bars actually are"). A dark halo then a rim in the
           fixture's GEL colour -- its static colour, never the live one: the
           housing is not the lamp, and a few lit pixels in mixed colours wash
           the fixture-wide live colour out to near-white, which drew the whole
           bar as though it were lit (aMostlyDarkPixelBarDrawsNoBrightOutline).
           Held to a mid luminance: clearly above the dark canvas and the
           housing, clearly below anything that reads as light, and tinted so
           it stays apart from the structure's neutral lines. */
        QColor rim = m_doc->monitorProperties()->fixtureGelColor(fid, 0, 0);
        if (!rim.isValid() || rim.hslSaturation() < 30)
            rim = QColor(70, 110, 160);
        else
            rim = QColor::fromHsl(rim.hslHue(), qMin(200, rim.hslSaturation()), 110);
        auto luma = [](const QColor &c) { return c.red() * 0.30 + c.green() * 0.59 + c.blue() * 0.11; };
        for (int i = 0; i < 8 && luma(rim) > 80.0; ++i)
            rim = rim.darker(110);
        if (drag || hi)
            rim = rim.lighter(140);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 150), (drag || hi) ? 3.6 : 3.0, Qt::SolidLine,
                      Qt::SquareCap, Qt::MiterJoin));
        p.drawPolygon(silhouette);
        p.setPen(QPen(rim, (drag || hi) ? 1.8 : 1.4, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
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
        const QVector<int> cellHead = headsByCell(fx, cols, rows, traits.headCount);
        for (int r = 0; r < rows; ++r)
        {
            const double fy = (rows > 1) ? (double(r) / (rows - 1) - 0.5) : 0.0;
            for (int cx = 0; cx < cols; ++cx)
            {
                const int placed = cellHead.value(r * cols + cx, -1);
                if (placed < 0)
                    continue;
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

QVector<int> StructureStudioView::headsByCell(Fixture *fx, int cols, int rows, int headCount) const
{
    QVector<int> out(qMax(0, cols * rows), -1);
    MonitorProperties *props = m_doc->monitorProperties();
    if (fx != nullptr && props->hasHeadLayout(fx))
    {
        const HeadLayout map = props->headLayoutFor(fx);
        if (map.size == QSize(cols, rows))
        {
            for (int h = 0; h < map.cells.size() && h < headCount; ++h)
            {
                const QPoint c = map.cellOf(h);
                if (c.x() >= 0 && c.x() < cols && c.y() >= 0 && c.y() < rows)
                    out[c.y() * cols + c.x()] = h;
            }
            return out;
        }
    }
    for (int i = 0; i < out.size() && i < headCount; ++i)
        out[i] = i;
    return out;
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

    /* Non-frame fixtures (riser/pipe/truss): even spread along their own
       param, one structure at a time.
     *
     * Everything on it (or nothing picked): across its whole length, as
     * before. A FEW of them ("if I select .. auto distribute"): evenly between
     * the outermost two, which stay put -- spreading three fixtures over the
     * whole truss would land them on top of the ones that were not selected. */
    std::sort(other.begin(), other.end(), byName);
    // How long the structure it rides is; -1 when it rides none of these.
    auto lengthOf = [props](const FixtureRigProps &rp) -> float {
        if (rp.riserPlatformId != FixtureRigProps::invalidPlatformId())
        { StagePlatform *pl = props->platform(rp.riserPlatformId); return pl ? pl->width() : -1.0f; }
        if (rp.pipeId != Pipe::invalidId())
        { Pipe *p = props->pipe(rp.pipeId); return p ? p->length() : -1.0f; }
        if (rp.trussId != Truss::invalidId())
        { Truss *tt = props->truss(rp.trussId); return tt ? tt->length() : -1.0f; }
        return -1.0f;
    };
    auto value = [](const FixtureRigProps &rp) -> float {
        if (rp.riserPlatformId != FixtureRigProps::invalidPlatformId()) return rp.riserU;
        if (rp.pipeId != Pipe::invalidId()) return rp.pipeOffset;
        return rp.trussOffset;
    };
    auto setValue = [](FixtureRigProps &rp, float v) {
        if (rp.riserPlatformId != FixtureRigProps::invalidPlatformId()) rp.riserU = v;
        else if (rp.pipeId != Pipe::invalidId()) rp.pipeOffset = v;
        else if (rp.trussId != Truss::invalidId()) rp.trussOffset = v;
    };
    auto structureOf = [](const FixtureRigProps &rp) -> QPair<int, quint32> {
        if (rp.riserPlatformId != FixtureRigProps::invalidPlatformId()) return qMakePair(0, rp.riserPlatformId);
        if (rp.pipeId != Pipe::invalidId()) return qMakePair(1, rp.pipeId);
        return qMakePair(2, rp.trussId);
    };
    QMap<QPair<int, quint32>, QList<quint32> > byStructure;
    foreach (quint32 fid, other)
    {
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        if (lengthOf(rp) >= 0.0f)
            byStructure[structureOf(rp)] << fid;
    }
    QSet<quint32> everyMounted;
    foreach (quint32 fid, mountedFixtures())
        everyMounted.insert(fid);
    for (auto it = byStructure.constBegin(); it != byStructure.constEnd(); ++it)
    {
        const QList<quint32> &on = it.value();
        const float len = lengthOf(props->fixtureRigProps(on.first()));
        int othersThere = 0;
        foreach (quint32 fid, everyMounted)
            if (!on.contains(fid) && structureOf(props->fixtureRigProps(fid)) == it.key())
                othersThere++;
        float lo = 0.0f, hi = 0.0f;
        bool between = false;
        if (!sel.isEmpty() && othersThere > 0 && on.size() >= 3)
        {
            lo = hi = value(props->fixtureRigProps(on.first()));
            foreach (quint32 fid, on)
            {
                lo = qMin(lo, value(props->fixtureRigProps(fid)));
                hi = qMax(hi, value(props->fixtureRigProps(fid)));
            }
            between = (hi - lo) > 0.01f;
        }
        /* In a LINE, too: everything but the spacing comes from the middle
           one of them. Each fixture's height and its offset across the truss
           are whatever its last drag left, a few millimetres apart, so evenly
           spaced fixtures still read as ragged ("the vertical height isn't
           quite right consistently"). The median, not the mean: an actual
           position one of them already has, and one stray does not drag the
           rest after it. */
        auto median = [&on, props](std::function<float(const FixtureRigProps &)> get) {
            QList<float> v;
            foreach (quint32 fid, on)
                v << get(props->fixtureRigProps(fid));
            std::sort(v.begin(), v.end());
            return v.at((v.size() - 1) / 2);
        };
        const float midCross  = median([](const FixtureRigProps &r) { return r.trussCross; });
        const float midCrossY = median([](const FixtureRigProps &r) { return r.trussCrossY; });
        const float midDrop   = median([](const FixtureRigProps &r) { return r.mountZOffset; });
        const float midRiserV = median([](const FixtureRigProps &r) { return r.riserV; });
        for (int i = 0; i < on.size(); ++i)
        {
            FixtureRigProps rp = props->fixtureRigProps(on[i]);
            setValue(rp, between ? lo + (hi - lo) * float(i) / float(on.size() - 1)
                                 : (i + 0.5f) / float(on.size()) * len);
            if (it.key().first == 2)            // a truss: across it, and the drop
            {
                rp.trussCross = midCross;
                rp.trussCrossY = midCrossY;
                rp.mountZOffset = midDrop;
            }
            else if (it.key().first == 0)       // a riser face: the same height up it
                rp.riserV = midRiserV;
            props->setFixtureRigProps(on[i], rp);
        }
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

void StructureStudioView::setFixtureFacing(quint32 fid, FixtureRigProps::Face face)
{
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);
    props->setFixtureFace(rp, face);
    // A frame-group fixture also re-pins to that face of its frame.
    if (!hasStructuralMount(rp) && props->fixtureFrameGroup(fid) != 0)
    {
        int pinComp; double pinVal; facePin(rp.studioMount, pinComp, pinVal);
        /* On a platform, the SURFACE that faces that way: the top or the base,
           the downstage or upstage edge, the stage-left (x = width) or
           stage-right (x = 0) end. facePin() knows planes only, and put Side
           on x = 0 -- the stage-right end, facing stage left, into the deck. */
        if (StagePlatform *pl = (m_kind == PlatformKind) ? props->platform(m_id) : nullptr)
        {
            switch (face)
            {
            case FixtureRigProps::FaceTop:        pinVal = pl->height(); break;
            case FixtureRigProps::FaceBottom:     pinVal = 0.0; break;
            case FixtureRigProps::FaceFront:      pinVal = pl->depth(); break;
            case FixtureRigProps::FaceBack:       pinVal = 0.0; break;
            case FixtureRigProps::FaceStageLeft:  pinVal = pl->width(); break;
            case FixtureRigProps::FaceStageRight: pinVal = 0.0; break;
            }
        }
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

/* "when editing a fixture on a truss or bar .. can we have handles to allow for
 * rotation via mouse movement?" The Angle and Tilt spin boxes were the only way
 * to turn one, which means typing numbers and watching what happens.
 *
 * A flat view drops one world axis, so the only rotation a pointer circling a
 * point can describe without guessing is the one ABOUT that dropped axis. That
 * is studioAngle when the view looks straight at the fixture's mounting face,
 * and studioTilt when it looks down the fixture's own length -- never both in
 * the same view, since the length lies in the face. */
StructureStudioView::RotateHandle StructureStudioView::rotateHandle() const
{
    RotateHandle h;
    if (m_plane == Angled || m_locked || m_highlight.size() != 1)
        return h;
    const quint32 fid = *m_highlight.constBegin();
    Fixture *fx = m_doc->fixture(fid);
    if (fx == nullptr || fixtureMovementLocked(fid))
        return h;

    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    // A tower-shelf unit in an elevation is drawn as an upright body that
    // does not follow its frame at all -- a handle would turn nothing visible.
    if (rp.towerId != Tower::invalidId() && (m_plane == Front || m_plane == Side))
        return h;
    /* Only what the flat views draw WITH its orientation. A mover or a par is
       drawn square-on to the screen whatever its frame says, so turning one
       here would change a number and nothing on the canvas. */
    const FixtureVisualTraits traits = classifyFixture(fx);
    if (traits.kind != FixtureSilhouette::Bar && traits.kind != FixtureSilhouette::Generic)
        return h;

    const QVector3D viewAxis = (m_plane == Top)   ? QVector3D(0, 0, 1)
                             : (m_plane == Front) ? QVector3D(0, 1, 0)
                                                  : QVector3D(1, 0, 0);
    QVector3D L, N, H;          // as it is now, roll included
    fixtureFrame(rp, L, N, H);

    const double w = (traits.physW > 0.0f) ? double(traits.physW) : qMax(0.05, fixtureLenM(fid));
    const double ht = (traits.physH > 0.0f) ? double(traits.physH) : w * 0.2;
    const double d = (traits.physD > 0.0f) ? double(traits.physD) : ht;
    const QVector3D c = fixtureCentre(fid);
    QPointF dir;

    if (qAbs(QVector3D::dotProduct(L, viewAxis)) < 0.1f)
    {
        /* Its length lies flat in this view, so turning it here is turning it
           about the axis out of the screen -- which is studioAngle on the face
           the view is looking at. When that IS its mount face this is just the
           Angle. When it is not ("where do I grab that to rotate it .. I see no
           handles": a deck bar, laid on the Top face, seen from the Side), the
           same turn is only expressible on the view's face, so the drag moves
           it there -- with the Angle and Tilt that leave it exactly as it is,
           so the move itself changes nothing you can see. */
        h.mode = RotateHandle::Angle;
        const int view = int(m_plane);          // Top/Front/Side == studioMount 0/1/2
        if (view == rp.studioMount)
            h.value = double(rp.studioAngle);
        else
        {
            const QPointF lp = project(L);       // linear in a flat plane
            const double angle0 = qRadiansToDegrees(qAtan2(lp.y(), lp.x()));
            FixtureRigProps cand = rp;
            cand.studioMount = view;             // not setStudioMount(): see below
            cand.studioAngle = float(angle0);
            cand.studioTilt = 0.0f;
            QVector3D L0, N0, H0;
            fixtureFrame(cand, L0, N0, H0);
            cand.studioTilt = float(qRadiansToDegrees(
                qAtan2(-double(QVector3D::dotProduct(N, H0)),
                        double(QVector3D::dotProduct(N, N0)))));
            /* Only where the face would STAY: a load repairs a deck or a
               horizontal-truss fixture left on the Front face back to Top,
               and a riser fixture to its riser's face -- which would turn it
               to something else the next time the file is opened. */
            FixtureRigProps chk = cand;
            props->repairMountPlane(chk);
            if (chk.studioMount != view)
                return RotateHandle();
            h.reframeMount = view;
            h.reframeTilt = double(cand.studioTilt);
            h.value = angle0;
        }
        // The run as it is DRAWN (a frame group's own rotation included),
        // so the knob sits on the end of the bar you can see.
        dir = w2s(fixtureEndB(fid)) - w2s(fixtureEndA(fid));
        // Constant while it spins, so the ring does not breathe under the hand.
        h.radius = 0.5 * qSqrt(w * w + ht * ht) * m_scale + 16.0;
        // fixtureAxisLocal() is (cos, sin) in every plane's own (a, b): turning
        // counter-clockwise in plane coordinates is +angle.
        h.sign = 1.0;
    }
    else if (traits.kind == FixtureSilhouette::Bar
             && qAbs(QVector3D::dotProduct(L, viewAxis)) > 0.9f)
    {
        // A Generic fixture draws as a line along its length, which a roll
        // about that length cannot change -- only a Bar body shows its tilt.
        h.mode = RotateHandle::Tilt;
        h.value = double(rp.studioTilt);
        dir = w2s(c + N) - w2s(c);   // the knob points the way it fires
        // The body is pushed out along N by its seat; this clears it at any roll.
        h.radius = qSqrt(d * d + ht * ht) * m_scale + 16.0;
        /* fixtureFrame() rolls N toward -H0 as the tilt grows. Which way round
           that is on screen depends on which end of the bar faces the viewer. */
        FixtureRigProps flat = rp;
        flat.studioTilt = 0.0f;
        QVector3D L0, N0, H0;
        fixtureFrame(flat, L0, N0, H0);
        const QPointF n0 = project(c + N0) - project(c);
        const QPointF h0 = project(c + H0) - project(c);
        const double cross = n0.x() * h0.y() - n0.y() * h0.x();
        h.sign = (cross > 0.0) ? -1.0 : 1.0;
    }
    else
        return h;

    const double len = qSqrt(dir.x() * dir.x() + dir.y() * dir.y());
    if (len < 1e-6)
        return RotateHandle();
    h.fid = fid;
    h.centre = w2s(c);
    h.radius = qBound(26.0, h.radius, 600.0);
    h.knob = h.centre + dir / len * h.radius;
    return h;
}

double StructureStudioView::planeAngleAround(const QPointF &centre, const QPointF &px) const
{
    const QPointF v = screenToPlane(px) - screenToPlane(centre);
    return qRadiansToDegrees(qAtan2(v.y(), v.x()));
}

bool StructureStudioView::rotateDragTo(const QPointF &px, bool snap)
{
    if (m_rotate.mode == RotateHandle::None)
        return false;

    /* Accumulate in small wrapped steps rather than taking the pointer's
       absolute bearing: a frame group's own rotation, or a view turned a
       quarter, would otherwise make the bar jump to the cursor on the first
       move. Relative to where it was grabbed, it simply follows the hand. */
    const double now = planeAngleAround(m_rotate.centre, px);
    double step = now - m_rotateLastMouse;
    while (step > 180.0)  step -= 360.0;
    while (step < -180.0) step += 360.0;
    m_rotateLastMouse = now;
    m_rotateTurned += step;

    double v = m_rotate.value + m_rotate.sign * m_rotateTurned;
    v = snap ? qRound(v / 15.0) * 15.0 : double(qRound(v));
    while (v > 180.0)   v -= 360.0;   // the inspector's range, either angle
    while (v <= -180.0) v += 360.0;

    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(m_rotate.fid);
    float &field = (m_rotate.mode == RotateHandle::Angle) ? rp.studioAngle : rp.studioTilt;
    const bool reframe = (m_rotate.reframeMount >= 0 && rp.studioMount != m_rotate.reframeMount);
    if (!reframe && qFuzzyCompare(double(field) + 1000.0, v + 1000.0))
        return false;
    if (!m_dragged)
        emit editAboutToStart();   // snapshot for undo before the first change
    if (m_rotate.reframeMount >= 0)
    {
        // Onto the face being looked at -- see rotateHandle(). Re-applied on
        // every move, so the frame is always the one the drag started from.
        rp.studioMount = m_rotate.reframeMount;
        rp.studioTilt = float(m_rotate.reframeTilt);
    }
    field = float(v);
    props->setFixtureRigProps(m_rotate.fid, rp);
    update();
    return true;
}

void StructureStudioView::drawRotateHandle(QPainter &p) const
{
    const RotateHandle h = rotateHandle();
    if (h.mode == RotateHandle::None)
        return;
    const QColor accent(255, 184, 64);
    const bool active = (m_rotate.mode != RotateHandle::None) || m_hoverKnob;

    p.save();
    p.setBrush(Qt::NoBrush);
    QPen ring(accent, 1.0, Qt::DashLine);
    ring.setColor(QColor(accent.red(), accent.green(), accent.blue(), active ? 220 : 140));
    p.setPen(ring);
    p.drawEllipse(h.centre, h.radius, h.radius);
    p.setPen(QPen(accent, 1.0));
    p.drawLine(h.centre, h.knob);
    p.setPen(QPen(accent.darker(220), 1.2));
    p.setBrush(accent);
    const double r = active ? 6.5 : 5.0;
    p.drawEllipse(h.knob, r, r);

    // Say what it turns, and by how much, beside the knob.
    const QString text = (h.mode == RotateHandle::Angle)
        ? tr("Angle %1°").arg(qRound(h.value))
        : tr("Tilt %1°").arg(qRound(h.value));
    QPointF out = h.knob - h.centre;
    const double ol = qSqrt(out.x() * out.x() + out.y() * out.y());
    out = (ol > 1e-6) ? out / ol : QPointF(1, 0);
    const QFontMetricsF fm(p.font());
    const double tw = fm.horizontalAdvance(text);
    QPointF at = h.knob + out * 12.0;
    if (out.x() < 0.0)
        at.rx() -= tw;               // keep the text on the outside of the ring
    at.ry() += fm.ascent() * 0.5 * (1.0 + out.y());
    p.setPen(accent);
    p.drawText(at, text);
    p.restore();
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
    {
        if (StagePlatform *pl = props->platform(m_id))
        {
            foreach (const QPointF &q, pl->footprint(16))
                c << QVector3D(float(q.x()), float(q.y()), 0.0f) << QVector3D(float(q.x()), float(q.y()), pl->height());
        }
    }
    else if (m_kind == TowerKind)
    {
        if (Tower *t = props->tower(m_id))
        {
            foreach (const QPointF &q, t->corners())
                c << QVector3D(float(q.x()), float(q.y()), 0.0f) << QVector3D(float(q.x()), float(q.y()), t->height());
        }
    }
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
    // Vertical dimension, just left of the structure -- and left of a
    // tower's shelf labels, which are drawn on that side.
    if (hSpan > 0.01)
    {
        const double x = sMinX - 20.0 - shelfLabelsWidth();
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

QString StructureStudioView::shelfLabel(int index, float height, double toDisp,
                                       const QString &unitSuffix) const
{
    return tr("Shelf %1 — %2%3").arg(index + 1)
        .arg(double(height) * toDisp, 0, 'f', 2).arg(unitSuffix);
}

double StructureStudioView::shelfLabelsWidth() const
{
    // Only where they are drawn: a tower's own editor, in an elevation.
    if (m_kind != TowerKind || m_plane == Top || m_plane == Angled)
        return 0.0;
    Tower *t = m_doc->monitorProperties()->tower(m_id);
    if (t == nullptr || t->shelfCount() == 0)
        return 0.0;
    const bool isFeet = (m_doc->monitorProperties()->gridUnits() == MonitorProperties::Feet);
    const QFontMetricsF fm(QFont("Arial", 8));
    double w = 0.0;
    for (int i = 0; i < t->shelfCount(); ++i)
        w = qMax(w, fm.horizontalAdvance(shelfLabel(i, t->shelfHeight(i), isFeet ? 3.28084 : 1.0,
                                                     isFeet ? tr(" ft") : tr(" m"))));
    return w + 8.0;
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

QVector3D StructureStudioView::worldAt(const QPointF &px, int *known, bool *onSurface) const
{
    /* Screen position and depth are both affine in the world point -- the
       projection is orthographic, there is no perspective divide -- so three
       samples give the whole map, and a pixel with its depth inverts it
       exactly. */
    auto S = [this](double x, double y, double z) {
        const QVector3D w{ float(x), float(y), float(z) };
        const QPointF sp = w2s(w);
        return std::array<double, 3>{ sp.x(), sp.y(), viewDepth(w) };
    };
    const std::array<double, 3> s0 = S(0, 0, 0);
    const std::array<double, 3> e[3] = { S(1, 0, 0), S(0, 1, 0), S(0, 0, 1) };
    double m[3][3];                 // column j: how screen x, y, depth move with axis j
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i)
            m[i][j] = e[j][i] - s0[i];

    *known = 0;
    *onSurface = false;
    const double depth = pickDepth(px.toPoint());
    if (depth > -1e29)
    {
        const double r[3] = { px.x() - s0[0], px.y() - s0[1], depth - s0[2] };
        auto det3 = [](const double a[3][3]) {
            return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
                 - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
                 + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
        };
        const double d = det3(m);
        if (qAbs(d) > 1e-12)
        {
            double w[3];
            for (int j = 0; j < 3; ++j)         // Cramer: column j replaced by r
            {
                double mj[3][3];
                for (int i = 0; i < 3; ++i)
                    for (int k = 0; k < 3; ++k)
                        mj[i][k] = (k == j) ? r[i] : m[i][k];
                w[j] = det3(mj) / d;
            }
            *known = 7;
            *onSurface = true;
            return QVector3D(float(w[0]), float(w[1]), float(w[2]));
        }
    }

    // Nothing drawn under it.
    const QPointF ab = screenToPlane(px);
    switch (m_plane)
    {
    case Top:
        *known = 1 | 2;
        return QVector3D(float(ab.x()), float(ab.y()), 0.0f);
    case Front:
        *known = 1 | 4;
        return QVector3D(float(ab.x()), 0.0f, float(ab.y()));
    case Side:
        *known = 2 | 4;
        return QVector3D(0.0f, float(ab.x()), float(ab.y()));
    default:
    {
        // At an angle: the floor (Z = 0) under the pointer.
        const double a = m[0][0], b = m[0][1], c = m[1][0], dd = m[1][1];
        const double det = a * dd - b * c;
        if (qAbs(det) < 1e-12)
            return QVector3D();
        const double rx = px.x() - s0[0], ry = px.y() - s0[1];
        *known = 7;
        return QVector3D(float((rx * dd - b * ry) / det), float((a * ry - rx * c) / det), 0.0f);
    }
    }
}

void StructureStudioView::drawCursorReadout(QPainter &p) const
{
    if (!m_hasCursor) return;
    MonitorProperties *props = m_doc->monitorProperties();
    const bool feet = props->gridUnits() == MonitorProperties::Feet;
    const double conv = feet ? 3.28084 : 1.0;
    const QString sfx = feet ? QStringLiteral("ft") : QStringLiteral("m");

    p.setPen(QPen(QColor(0, 190, 255, 110), 0.8, Qt::DashLine));
    p.drawLine(QPointF(0, m_cursorPx.y()), QPointF(width(), m_cursorPx.y()));
    p.drawLine(QPointF(m_cursorPx.x(), 0), QPointF(m_cursorPx.x(), height()));

    /* H across the stage (stage left +), V up/downstage (downstage +), Z the
       height above the floor -- the plot's own X / Y and a real height, of
       whatever is under the pointer ("can we have the crosshairs hover give
       H, V, and Z"). Over nothing at an angle it is the floor there. */
    int known = 0;
    bool onSurface = false;
    const QVector3D w = worldAt(m_cursorPx, &known, &onSurface);
    auto num = [&](int bit, float v) {
        return (known & bit) ? QString::number(double(v) * conv, 'f', 2) : QStringLiteral("—");
    };
    QString t = QString("H %1  V %2  Z %3 %4").arg(num(1, w.x()), num(2, w.y()), num(4, w.z()), sfx);
    if (!onSurface && m_plane == Angled && known)
        t += tr("  (floor)");
    QFont f("Arial", 8);
    p.setFont(f);
    const double tw = QFontMetricsF(f).horizontalAdvance(t) + 14.0;
    QRectF r(m_cursorPx.x() + 12, m_cursorPx.y() + 10, tw, 18);
    if (r.right() > width()) r.moveLeft(m_cursorPx.x() - 12 - tw);
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
    case Angled:                               // no flat axes to name
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
 * O(fixtures^2), and placing every fixture to compare them was a quarter of
 * what a frame of a 200-fixture show cost -- for warnings that change only
 * when the rig does. So they are worked out at most twice a second (and at
 * once on reload()) rather than invalidated on whatever subset of
 * rig-geometry changes might move a fixture. */
void StructureStudioView::rebuildOverlapWarnings() const
{
    if (m_overlapAge.isValid() && m_overlapAge.elapsed() < 500)
        return;
    m_overlapAge.start();
    m_overlappingFixtures.clear();
    if (m_doc == nullptr)
        return;

    struct Box { quint32 fid; QVector3D lo, hi; };
    QVector<Box> boxes;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr || fx->definitionUnavailable())
            continue;   // a stand-in's size is made up: nothing to warn about
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
    // The decks every beam ray is thrown against, worked out once this frame.
    m_decksValid = false;
    m_decks = decks();
    m_occluders = occluders();
    m_decksValid = true;

    /* Where rays landed last frame still stands if what they land on hasn't
       changed. Rays no longer thrown (a mover that has moved on) are let go
       once they are most of what is kept. */
    const quint64 scene = throwScene();
    if (scene != m_throwScene || m_throws.size() > 400000)
    {
        m_throws.clear();
        m_beamMeshes.clear();
        m_throwScene = scene;
    }
    ++m_throwFrame;
    m_throwsUsed = 0;
    struct DecksDone
    {
        const StructureStudioView *view;
        ~DecksDone()
        {
            view->m_decksValid = false;
            if (view->m_throws.size() > 2 * view->m_throwsUsed + 4096)
            {
                for (auto it = view->m_throws.begin(); it != view->m_throws.end(); )
                    it = (it->frame == view->m_throwFrame) ? std::next(it)
                                                           : view->m_throws.erase(it);
            }
            // Meshes of beams not drawn this frame (moved, or gone).
            for (auto it = view->m_beamMeshes.begin(); it != view->m_beamMeshes.end(); )
                it = (it->frame == view->m_throwFrame) ? std::next(it)
                                                       : view->m_beamMeshes.erase(it);
        }
    } decksDone{ this };

    m_opticsAnimating = false;          // set again by any gobo or prism turning
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
        drawRotateHandle(p);    // on top of everything it might overlap
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

    /* A turning gobo or prism has to be seen turning. The live timer already
       repaints when it runs; otherwise ask for the next frame here, no faster
       than frames are being drawn. */
    const bool liveTicking = m_liveValues && m_liveTimer != nullptr && m_liveTimer->isActive();
    if (m_opticsAnimating && !liveTicking && !m_opticsTickPending)
    {
        m_opticsTickPending = true;
        const int ms = int(qBound(40.0, m_frameMs * 1.25, 250.0));
        QTimer::singleShot(ms, this, [this]() {
            m_opticsTickPending = false;
            update();
        });
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
            zoomAt(g->position(), m_scale * (1.0 + g->value()));
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

    const QPointF anchor = e->position();
    zoomAt(anchor, m_scale * ((delta > 0) ? 1.12 : 1.0 / 1.12));
    e->accept();
}

void StructureStudioView::mousePressEvent(QMouseEvent *e)
{
    /* Shift-drag pans -- but Shift-click on a FIXTURE adds it to the
       selection, as Shift does in the list beside it ("I can't shift select
       multiple fixtures in the display .. but CAN select them on the list"). */
    if (e->button() == Qt::MiddleButton
        || (e->button() == Qt::LeftButton && (e->modifiers() & Qt::ShiftModifier)
            && hitTestFixture(e->pos()) == Fixture::invalidId()))
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
        // The selected fixture's rotation knob, ahead of the fixture itself:
        // the knob often sits over the bar it turns, or over a neighbour.
        const RotateHandle rh = rotateHandle();
        if (rh.mode != RotateHandle::None && QLineF(rh.knob, e->pos()).length() <= 9.0)
        {
            m_rotate = rh;
            m_rotateLastMouse = planeAngleAround(rh.centre, e->pos());
            m_rotateTurned = 0.0;
            m_dragFid = Fixture::invalidId();
            m_dragged = false;
            setCursor(Qt::ClosedHandCursor);
            return;
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
                emit fixturesSelected({});
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
                setCursor(AppUtil::nativeCursorShape(Qt::SizeAllCursor, true));
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
            setBarHighlight(UINT_MAX);   // a bar and a fixture are never selected together
            setPipeBarHighlight(Pipe::invalidId());
            m_dragGroup.clear();
            m_dragGroupStart.clear();
            m_pressedSelected = false;

            /* "select all the fixtures .. and multi-select to move them
               around": Cmd-click (Ctrl elsewhere) or Shift-click adds or
               removes one, and pressing one of several selected keeps them
               all, to drag as a group. */
            if (e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))
            {
                QSet<quint32> sel = m_highlight;
                if (sel.contains(m_dragFid))
                    sel.remove(m_dragFid);
                else
                    sel.insert(m_dragFid);
                const quint32 primary = sel.contains(m_dragFid) ? m_dragFid
                                      : sel.isEmpty() ? Fixture::invalidId()
                                                      : *sel.constBegin();
                QList<quint32> ids = fixturesInDrawOrder();
                for (int i = ids.size() - 1; i >= 0; --i)
                    if (!sel.contains(ids.at(i)))
                        ids.removeAt(i);
                setHighlight(ids);
                m_dragFid = Fixture::invalidId();   // a toggle, not a drag
                emit fixtureSelected(primary);
                emit fixturesSelected(ids);
                return;
            }

            if (m_highlight.size() > 1 && m_highlight.contains(m_dragFid))
            {
                m_pressedSelected = true;
                foreach (quint32 f, fixturesInDrawOrder())
                    if (m_highlight.contains(f))
                    {
                        m_dragGroup << f;
                        m_dragGroupStart.insert(f, w2s(fixtureCentre(f)));
                    }
                m_dragPressPx = e->pos();
            }
            else
                setHighlight({ m_dragFid });
            setCursor((m_locked || m_plane == Angled || fixtureMovementLocked(m_dragFid))
                          ? Qt::ArrowCursor : Qt::ClosedHandCursor);
            emit fixtureSelected(m_dragFid);   // selection always works
            emit fixturesSelected(m_dragGroup.isEmpty() ? QList<quint32>{ m_dragFid } : m_dragGroup);
        }
    }
}

void StructureStudioView::mouseMoveEvent(QMouseEvent *e)
{
    m_cursorPx = e->pos();   // live ruler readout
    if (e->buttons() != Qt::NoButton)
        rigMayHaveMoved();   // a drag here moves things: see it at once
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
    if (m_rotate.mode != RotateHandle::None)
    {
        if (rotateDragTo(e->pos(), e->modifiers() & Qt::ShiftModifier))
            m_dragged = true;
        return;
    }
    if (e->buttons() == Qt::NoButton)
    {
        // Hovering: say the knob can be grabbed.
        const RotateHandle rh = rotateHandle();
        const bool over = rh.mode != RotateHandle::None
                          && QLineF(rh.knob, e->pos()).length() <= 9.0;
        if (over != m_hoverKnob)
        {
            m_hoverKnob = over;
            setCursor(over ? Qt::OpenHandCursor : Qt::ArrowCursor);
        }
    }
    if (m_dragFid != Fixture::invalidId() && !m_locked && !m_dragGroup.isEmpty())
    {
        // Together: each one keeps its offset from the pointer. One that is
        // locked (or on a locked structure) stays; the rest still go.
        const QPointF delta = QPointF(e->pos()) - m_dragPressPx;
        bool any = false;
        foreach (quint32 f, m_dragGroup)
        {
            if (fixtureMovementLocked(f))
                continue;
            if (!m_dragged && !any)
                emit editAboutToStart();   // one snapshot for the whole group
            if (dragFixtureTo(f, m_dragGroupStart.value(f) + delta))
                any = true;
        }
        if (any)
            m_dragged = true;
    }
    else if (m_dragFid != Fixture::invalidId() && !m_locked && !fixtureMovementLocked(m_dragFid))
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
    m_hoverKnob = false;
    update();
}

void StructureStudioView::mouseReleaseEvent(QMouseEvent *)
{
    rigMayHaveMoved();
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
    if (m_rotate.mode != RotateHandle::None)
    {
        const quint32 fid = m_rotate.fid;
        m_rotate = RotateHandle();
        setCursor(m_hoverKnob ? Qt::OpenHandCursor : Qt::ArrowCursor);
        if (m_dragged)
        {
            m_dragged = false;
            m_doc->setModified();
            emit fixtureMoved(fid);      // the plot and the tree follow
            emit fixtureSelected(fid);   // the inspector's Angle / Tilt too
        }
        update();
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
            if (m_dragGroup.isEmpty())
                emit fixtureMoved(m_dragFid);
            else
            {
                foreach (quint32 f, m_dragGroup)
                    emit fixtureMoved(f);
                emit fixturesMoved(m_dragGroup);
            }
        }
        else if (m_pressedSelected)
        {
            // Clicked, not dragged, one of several: just that one now.
            setHighlight({ m_dragFid });
            emit fixtureSelected(m_dragFid);
            emit fixturesSelected({ m_dragFid });
        }
        m_dragFid = Fixture::invalidId();
        m_dragGroup.clear();
        m_dragGroupStart.clear();
        m_pressedSelected = false;
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

quint32 StructureStudioView::hitTestHanger(const QPointF &px) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    quint32 best = UINT_MAX;
    double bestD = 8.0;                       // px: a hanger is drawn thin
    auto segment = [&](const QVector3D &a3, const QVector3D &b3) {
        const QPointF a = w2s(a3), b = w2s(b3), ab = b - a;
        const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
        double t = (len2 > 1e-6) ? ((px.x() - a.x()) * ab.x() + (px.y() - a.y()) * ab.y()) / len2 : 0.0;
        t = qBound(0.0, t, 1.0);
        return QLineF(a + ab * t, px).length();
    };
    foreach (const Hanger *h, props->hangers())
    {
        const bool shown = m_kind == StageKind
            || (m_kind == TrussKind && h->parentKind() == Hanger::OnTruss && h->parentId() == m_id)
            || (m_kind == TowerKind && h->parentKind() == Hanger::OnTower && h->parentId() == m_id);
        if (!shown)
            continue;
        const double d = qMin(qMin(segment(h->fastened() + QVector3D(0, 0, 0.05f),
                                           h->fastened() - QVector3D(0, 0, 0.05f)),
                                   segment(h->fastened(), h->armEnd())),
                              segment(h->armEnd(), h->hook()));
        if (d < bestD) { bestD = d; best = h->id(); }
    }
    return best;
}

void StructureStudioView::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    m_lanternPixels->setActive(true);   // ask devices what they show, while seen
}

void StructureStudioView::hideEvent(QHideEvent *e)
{
    m_lanternPixels->setActive(false);
    QWidget::hideEvent(e);
}

void StructureStudioView::mouseDoubleClickEvent(QMouseEvent *e)
{
    const quint32 fid = hitTestFixture(e->pos());
    if (fid != Fixture::invalidId())
    {
        emit fixtureActivated(fid);
        return;
    }
    // Its bracket, arm or chain: the hanger itself (the lantern on it is the fixture).
    const quint32 hangerId = hitTestHanger(e->pos());
    if (hangerId != UINT_MAX)
    {
        emit hangerActivated(hangerId);
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

float StructureStudioView::offsetAlongTrussAt(quint32 trussId, const QPoint &viewPos) const
{
    Truss *t = m_doc->monitorProperties()->truss(trussId);
    if (t == nullptr)
        return -1.0f;
    const QPointF a = w2s(t->origin());
    const QPointF b = w2s(t->positionAt(t->length()));
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 16.0)                       // under 4 px long: end-on
        return -1.0f;
    const QPointF ap = QPointF(viewPos) - a;
    const double u = qBound(0.0, (ap.x() * ab.x() + ap.y() * ab.y()) / len2, 1.0);
    return float(u) * t->length();
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

void StructureStudioView::drawLanternSolid(QPainter &p, const QVector3D corner[8],
                                           const QColor &cage, const QColor &live,
                                           double level, double depthBias,
                                           const QVector<QVector3D> &ledPts,
                                           const QVector<QColor> &ledCols,
                                           const QString &tag, bool clearGlass) const
{
    /* Corners k and k+4 are the two ends of one vertical edge of the body
       (fixtureBoxCorners(): k+4 is +H). Which end is UP depends on the frame,
       so the caps are taken by height, not by index. */
    auto lerp = [](const QVector3D &a, const QVector3D &b, float t) { return a + (b - a) * t; };
    auto capCorners = [&](float t0, float t1, QVector3D out[8]) {
        for (int k = 0; k < 4; ++k)
        {
            out[k]     = lerp(corner[k], corner[k + 4], t0);
            out[k + 4] = lerp(corner[k], corner[k + 4], t1);
        }
    };
    QVector3D capA[8], capB[8];
    capCorners(0.0f, 0.12f, capA);          // one end
    capCorners(0.88f, 1.0f, capB);          // the other
    drawSolidBox(p, capA, cage, cage.darker(140), 255, true, depthBias);
    drawSolidBox(p, capB, cage, cage.darker(140), 255, true, depthBias);

    // Four posts between the caps, a little in from the corners.
    QVector3D centre;
    for (int i = 0; i < 8; ++i)
        centre += corner[i];
    centre /= 8.0f;
    for (int k = 0; k < 4; ++k)
    {
        const QVector3D a = lerp(lerp(corner[k], corner[k + 4], 0.12f), centre, 0.12f);
        const QVector3D b = lerp(lerp(corner[k], corner[k + 4], 0.88f), centre, 0.12f);
        emitLine(w2s(a), w2s(b), viewDepth(a) + depthBias, viewDepth(b) + depthBias,
                 cage.darker(120), 1.6, p);
    }

    /* Diffusing glass: each side pane glows in what lights it -- the LEDs
       nearest it count most, so an effect still runs round the lantern, but
       as a blur rather than as points. Dark, it is frosted. */
    if (!clearGlass)
    {
        QVector3D glass[8];
        capCorners(0.12f, 0.88f, glass);
        /* Each pane in patches, two across and three up, each lit by the
           LEDs nearest it: a block of colour running round the lantern shows
           as a soft patch moving across the glass. */
        auto glow = [&](const QVector3D &at) {
            double r = 0.0, g = 0.0, b = 0.0, wsum = 0.0;
            if (level > 0.0 && !ledPts.isEmpty())
            {
                for (int i = 0; i < ledPts.size() && i < ledCols.size(); ++i)
                {
                    const double d2 = double((ledPts.at(i) - at).lengthSquared());
                    const double wt = 1.0 / (0.0004 + d2 * d2);
                    r += ledCols.at(i).redF() * wt; g += ledCols.at(i).greenF() * wt;
                    b += ledCols.at(i).blueF() * wt; wsum += wt;
                }
                if (wsum > 0.0) { r /= wsum; g /= wsum; b /= wsum; }
            }
            else if (level > 0.0)
            {
                r = live.redF() * level; g = live.greenF() * level; b = live.blueF() * level;
            }
            const double peak = qMax(r, qMax(g, b));
            QColor pane(200, 196, 188, 60);                 // frosted, unlit
            if (peak > 0.004)
            {
                pane = QColor::fromRgbF(float(r / peak), float(g / peak), float(b / peak));
                pane.setAlpha(qBound(60, int(60 + 150 * peak), 210));
            }
            return pane;
        };
        const int across = 2, up = 3;
        for (int k = 0; k < 4; ++k)
        {
            const int k1 = (k + 1) % 4;
            const QVector3D b0 = glass[k], b1 = glass[k1], t0 = glass[k + 4], t1 = glass[k1 + 4];
            auto at = [&](double u, double v) {
                const QVector3D lo = b0 + (b1 - b0) * float(u), hi = t0 + (t1 - t0) * float(u);
                return lo + (hi - lo) * float(v);
            };
            for (int iu = 0; iu < across; ++iu)
                for (int iv = 0; iv < up; ++iv)
                {
                    const double u0 = double(iu) / across, u1 = double(iu + 1) / across;
                    const double v0 = double(iv) / up, v1 = double(iv + 1) / up;
                    const QVector3D quad[4] = { at(u0, v0), at(u1, v0), at(u1, v1), at(u0, v1) };
                    const QVector3D fc = at((u0 + u1) / 2, (v0 + v1) / 2);
                    QPolygonF poly;
                    QVector<double> zs;
                    for (const QVector3D &v : quad)
                    {
                        poly << w2s(v);
                        zs << viewDepth(v) + depthBias + 1e-4;
                    }
                    emitPoly(poly, viewDepth(fc) + depthBias + 1e-4, glow(fc), QColor(), 0.0, p, zs);
                }
        }
    }

    /* The core: the LEDs clustered in the middle, seen through the glass --
       bright in its own colour when lit, dull glass when not. Sized from the
       body, never smaller than a few pixels so a lit lantern reads at
       whole-rig zoom. */
    const double bodyPx = QLineF(w2s(corner[0]), w2s(corner[1])).length();
    const double r = qMax(2.5, bodyPx * 0.28);
    const QPointF c = w2s(centre);
    const double z = viewDepth(centre) + depthBias + 2e-4;

    /* Its LEDs, where it is built of them: each its own small light, in its
       own colour -- an effect running along the string shows as it runs. Dark,
       they are beads on the wire; the glass round them stays faint. The glow
       in the haze is what they add up to. */
    if (!ledPts.isEmpty())
    {
        const double ledR = qMax(1.2, bodyPx * 0.045);
        double sr = 0.0, sg = 0.0, sb = 0.0;
        for (int i = 0; i < ledPts.size(); ++i)
        {
            const QColor lc = (level > 0.0 && i < ledCols.size()) ? ledCols.at(i) : QColor(Qt::black);
            const double peak = qMax(lc.redF(), qMax(lc.greenF(), lc.blueF()));
            sr += lc.redF(); sg += lc.greenF(); sb += lc.blueF();
            QColor bead(96, 90, 80);
            if (peak > 0.004)
            {
                // Its own hue at full: lifting it toward white read red as pink.
                bead = QColor::fromRgbF(float(lc.redF() / peak), float(lc.greenF() / peak),
                                        float(lc.blueF() / peak));
                const double a = qBound(0.25, peak * 1.6, 1.0);
                bead.setRgbF(float(96 / 255.0 * (1 - a) + bead.redF() * a),
                             float(90 / 255.0 * (1 - a) + bead.greenF() * a),
                             float(80 / 255.0 * (1 - a) + bead.blueF() * a));
            }
            if (!clearGlass)
                bead.setAlpha(peak > 0.004 ? 150 : 70);    // a blur behind the glass
            emitDot(w2s(ledPts.at(i)), viewDepth(ledPts.at(i)) + depthBias + 3e-4,
                    clearGlass ? ledR : ledR * 1.5, bead, p);
        }
        const int n = ledPts.size();
        const QColor avg = QColor::fromRgbF(float(qMin(1.0, sr / n)), float(qMin(1.0, sg / n)),
                                            float(qMin(1.0, sb / n)));
        const double avgPeak = qMax(avg.redF(), qMax(avg.greenF(), avg.blueF()));
        if (avgPeak > 0.004 && m_haze > 0.0)
        {
            const QColor core = QColor::fromRgbF(float(avg.redF() / avgPeak), float(avg.greenF() / avgPeak),
                                                 float(avg.blueF() / avgPeak)).lighter(int(100 + 60 * avgPeak));
            for (int ring = 0; ring < 2; ++ring)
            {
                const double rr = r * (ring == 0 ? 1.9 : 3.4);
                QPolygonF disc;
                QVector<double> zs;
                for (int i = 0; i < 24; ++i)
                {
                    const double a = 2.0 * M_PI * i / 24.0;
                    disc << QPointF(c.x() + rr * qCos(a), c.y() + rr * qSin(a));
                    zs << z;
                }
                QColor halo = core;
                halo.setAlpha(qBound(0, int((ring == 0 ? 90.0 : 40.0) * avgPeak * qMin(1.5, m_haze)), 200));
                emitLight(disc, z, halo, p, zs);
            }
        }
        if (!tag.isEmpty())
            emitLabel(QPointF(c.x() + r + 4, c.y() + r + 10), z, tag, QColor(230, 200, 120), p);
        return;
    }

    if (level <= 0.0)
    {
        QPolygonF glass;
        for (int i = 0; i < 16; ++i)
            glass << QPointF(c.x() + r * qCos(2.0 * M_PI * i / 16.0), c.y() + r * qSin(2.0 * M_PI * i / 16.0));
        emitPoly(glass, z, QColor(120, 112, 96, 140), QColor(), 0.0, p);
        return;
    }
    QColor core = live;
    const double cmax = qMax(core.redF(), qMax(core.greenF(), core.blueF()));
    if (cmax > 0.0)
        core = QColor::fromRgbF(float(core.redF() / cmax), float(core.greenF() / cmax),
                                float(core.blueF() / cmax));
    core = core.lighter(int(100 + 60 * level));
    QPolygonF disc;
    for (int i = 0; i < 16; ++i)
        disc << QPointF(c.x() + r * qCos(2.0 * M_PI * i / 16.0), c.y() + r * qSin(2.0 * M_PI * i / 16.0));
    emitPoly(disc, z, core, QColor(), 0.0, p);

    // A halo in the haze around it: light, so it adds up and never covers.
    if (m_haze > 0.0)
    {
        for (int ring = 0; ring < 2; ++ring)
        {
            const double rr = r * (ring == 0 ? 1.9 : 3.4);
            QPolygonF disc;
            QVector<double> zs;
            for (int i = 0; i < 24; ++i)
            {
                const double a = 2.0 * M_PI * i / 24.0;
                disc << QPointF(c.x() + rr * qCos(a), c.y() + rr * qSin(a));
                zs << z;
            }
            QColor halo = core;
            halo.setAlpha(qBound(0, int((ring == 0 ? 90.0 : 40.0) * level * qMin(1.5, m_haze)), 200));
            emitLight(disc, z, halo, p, zs);
        }
    }
    if (!tag.isEmpty())
        emitLabel(QPointF(c.x() + r + 4, c.y() + r + 10), z, tag, QColor(230, 200, 120), p);
}

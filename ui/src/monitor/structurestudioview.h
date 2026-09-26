/*
  Q Light Controller Plus
  structurestudioview.h

  Lighting Studio — structure canvas. An orthographic Top/Front/Side view of a
  single rigging STRUCTURE (a Stand + its booms/bars, a Tower + its shelves, or a
  Truss + its base plate) with the fixtures mounted on it drawn in place. Unlike
  StudioPlaneView (which works in a studio group's LOCAL frame), this projects
  WORLD coordinates, so it can show any placeable structure and everything on it.

  Projection (metres → in-plane a,b):
    - Top   : a = X (stage right +),  b = Y (upstage +, screen-down)
    - Front : a = X,                  b = Z (height, screen-up)
    - Side  : a = Y,                  b = Z (height, screen-up)

  Slice 1 is a read-only reference drawing; dragging/adding land in later slices.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STRUCTURESTUDIOVIEW_H
#define STRUCTURESTUDIOVIEW_H

#include <QWidget>
#include <QVector3D>
#include <QColor>
#include <QList>
#include <QSet>
#include <QPair>
#include <QVector>
#include <climits>
#include <QElapsedTimer>

#include "beamoptics.h"

#include "fixture.h"   // Fixture::invalidId() -- the "no fixture" marker

class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class StructureStudioView : public QWidget
{
    Q_OBJECT

public:
    enum Kind  { StandKind = 0, TowerKind = 1, TrussKind = 2,
                 PlatformKind = 3, PipeKind = 4, GroupKind = 5,
                 /** The WHOLE rig rather than one object: every structure and
                  *  every placed fixture, for an overview. Read-only. */
                 StageKind = 6,
                 /** A cyclorama or other scenery: a surface, drawn in the
                  *  overview, carrying no fixtures. */
                 SceneryKind = 7 };
    /** Top/Front/Side are the axis-aligned working planes -- each drops one
     *  world axis, which is exactly why a screen point can be turned back into
     *  a world one and fixtures can be DRAGGED in them.
     *
     *  Angled is an axonometric look from above and to the side. It is
     *  VIEW-ONLY: at an angle a screen point maps to a ray rather than a point,
     *  so there is no honest inverse and dragging is refused rather than
     *  silently moving the wrong thing. */
    enum Plane { Top = 0, Front = 1, Side = 2, Angled = 3 };

    StructureStudioView(Doc *doc, Kind kind, quint32 id, QWidget *parent = nullptr);

    void setPlane(Plane p);
    Plane plane() const { return m_plane; }

    /** Turn the view in 90-degree steps (0..3, clockwise). A pure VIEW
     *  transform -- stored coordinates never move -- so a designer who reads a
     *  plan with downstage at the top can have it without the file changing.
     *  Quarter turns keep the handedness honest; see planeToScreenVec(). */
    void setRotation(int quarterTurns);
    int  rotation() const { return m_rotation; }

    /** Camera angles for the Angled plane, in degrees: @p azimuth swings around
     *  the stage (0 = straight from the audience, positive toward stage left),
     *  @p elevation lifts the eye above the floor (0 = a flat front elevation,
     *  90 = straight down, i.e. a plan). Defaults to 45/45. */
    void setAngledView(double azimuthDeg, double elevationDeg);
    double angledAzimuth() const { return m_azimuthDeg; }
    double angledElevation() const { return m_elevationDeg; }

    /** True when the current plane cannot be edited in (see Plane::Angled). */
    bool isViewOnly() const { return m_plane == Angled; }

    /** Show what the rig is DOING: fixture colour and brightness taken from the
     *  DMX being sent, rather than the static gel colour. Repaints are
     *  throttled -- see the timer in the implementation -- because this is one
     *  widget redrawing everything, and a busy chase across a few hundred
     *  fixtures would otherwise repaint far faster than anyone can see. */
    void setLiveValues(bool on);
    bool liveValues() const { return m_liveValues; }

    /** How much light there is in the room, independent of the rig. A stage at
     *  full work-light reads very differently from a blackout with three
     *  fixtures up, and with no ambient at all the rendered colours are as
     *  vivid as a test card. 0 = blackout (only what the rig emits is visible),
     *  1 = full working light. */
    void setAmbient(double level);
    /** Draw the beams moving heads are throwing. Live output only -- an unlit
     *  rig has no beams to show. */
    void setBeams(bool on);

    /* Render fidelity and speed, remembered between sessions.
     *
     * Supersample 1 is the fastest and jagged, 2 the default, 3 and 4 sharper
     * at up to four times the fill. Threads 0 = one per core. The frame rate is
     * a ceiling for live output: the view still slows down on its own when a
     * frame costs more than the rate allows. */
    void setSupersample(int factor);
    int supersample() const { return m_supersample; }
    void setRenderThreads(int threads);
    int renderThreads() const { return m_renderThreads; }
    void setMaxFrameRate(int fps);
    int maxFrameRate() const { return m_maxFps; }
    /** The scene whose Aim palettes decide where movers are POINTED when there
     *  is no live DMX saying otherwise. The studio draws these as dashed lines;
     *  this is the same association, shown as the direction it actually means. */
    void setActiveScene(quint32 sceneId);
    bool beams() const { return m_beams; }
    double ambient() const { return m_ambient; }

    /** Locked = fixtures can be selected but not dragged (like the main plot lock);
     *  unlocked = drag a selected fixture / a boom top to move it. */
    void setLocked(bool on) { m_locked = on; setCursor(Qt::ArrowCursor); }
    bool locked() const { return m_locked; }

    /** Re-gather the structure + its fixtures and repaint (after external edits). */
    void reload();

    /** Fixtures currently mounted on this structure (for the side tree). */
    QList<quint32> mountedFixtures() const;
    QList<quint32> mountedFixtures(Kind kind, quint32 id) const;

    /** mountedFixtures() in the order a FLAT view paints them, back to front.
     *
     *  Grouped by the structure each is mounted on, nearer structures later;
     *  within one structure, what is rigged INSIDE it before what is on its
     *  surface, then by depth. A fixture on a truss face and one inside the
     *  same truss share a centre line, so position alone cannot say which is
     *  in front -- and whichever one is drawn on top is the one a click must
     *  pick, so hitTestFixture() walks the same list. */
    QList<quint32> fixturesInDrawOrder() const;

    /** Ring-highlight a set of fixtures (driven by the tree selection). */
    void setHighlight(const QList<quint32> &ids);
    /** Ring-highlight a single bar (or clear it with UINT_MAX == Truss::invalidId()). */
    void setBarHighlight(quint32 barId);
    /** Same idea as setBarHighlight(), for a Pipe crossbar. */
    void setPipeBarHighlight(quint32 barId);

    /** Assign @p ids to the CURRENT view's face (Top/Front/Side): set their
     *  studioMount and pin them to that face surface. Empty = all mounted. */
    void putOnFace(const QList<quint32> &ids);

    /** Lay @p ids out evenly on the current face (name order). Auto-orients: if
     *  they don't fit side-by-side across the width they STACK vertically,
     *  centred, top→bottom. Empty = all mounted. */
    void distributeOnFace(const QList<quint32> &ids);

    /** Inspector edits for one fixture (frame-group only). */
    void setFixtureFace(quint32 fid, int face);     ///< studioMount + re-pin
    void setFixtureAngle(quint32 fid, float deg);   ///< studioAngle (bar rotation)

    /** Position an already-mounted fixture at a screen point (drop landing). */
    bool placeFixtureAt(quint32 fid, const QPointF &px) { return dragFixtureTo(fid, px); }

signals:
    /** The camera angles changed (an orbit drag), so a dialog can follow. */
    void angledViewChanged(double azimuthDeg, double elevationDeg);

    /** A fixture on the structure was double-clicked (id passed through). */
    void fixtureActivated(quint32 fid);
    /** A fixture on the structure was single-clicked (selection). */
    void fixtureSelected(quint32 fid);
    /** A drag of a fixture is about to change the doc (for undo snapshotting). */
    void editAboutToStart();
    /** A fixture was dragged to a new spot on the structure (rig prop changed). */
    void fixtureMoved(quint32 fid);
    /** Fixtures were dragged in from the source tree and dropped at @p pos. */
    void fixturesDropped(const QList<quint32> &fids, const QPointF &pos);
    /** Right-click on the canvas. @p fidUnder = fixture under the cursor (0 = empty). */
    void canvasContextMenu(const QPoint &globalPos, quint32 fidUnder);
    /** Structure geometry changed on the canvas (e.g. a boom resized) — the host
     *  should refresh the 2D map. */
    void structureChanged();

    /** A bar hung on this truss was double-clicked -- its own editor (Along
     *  truss / Face / Lock) lives elsewhere, this just says which bar. */
    void barActivated(quint32 barId);
    /** A bar was single-clicked (selection, for tree-highlight sync). */
    void barSelected(quint32 barId);
    /** A bar was dragged to a new offset along its parent. */
    void barMoved(quint32 barId);

    /** Same three signals as barActivated/barSelected/barMoved, for a Pipe
     *  crossbar instead of a Truss bar. */
    void pipeBarActivated(quint32 barId);
    void pipeBarSelected(quint32 barId);
    void pipeBarMoved(quint32 barId);

protected:
    bool event(QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QPointF project(const QVector3D &w) const;      ///< world → in-plane (a,b) metres
    QPointF w2s(const QVector3D &w) const;           ///< world → screen pixels
    QPointF screenToPlane(const QPointF &px) const;  ///< screen pixels → in-plane (a,b) metres
    /** Rescale to targetScale, keeping the plane point under `anchor` (screen
     *  pixels) fixed on screen -- the one anchor-at-cursor zoom math, shared by
     *  the scroll wheel and native trackpad pinch so the two can never drift
     *  out of step with each other. Clamped to the same [4, 4000] range either
     *  caller already enforced individually. */
    void zoomAt(const QPointF &anchor, double targetScale);
    /** Remap a dragged fixture's mount param from a screen point (edits pipe
     *  offset / tower U-V-shelf / truss offset in place). Returns true if moved. */
    bool dragFixtureTo(quint32 fid, const QPointF &px);
    /** Same idea as dragFixtureTo(), but for a bar hung on this truss: the
     *  drag's component along the PARENT truss's own run becomes the bar's
     *  new parentOffset. Refuses on a locked parent/bar or in Angled, same
     *  reasoning as the fixture path. */
    bool dragBarTo(quint32 barId, const QPointF &px);
    /** Same idea as dragBarTo(), but for a crossbar hung on a Pipe (a boom's
     *  own bar, not a Truss's): the drag's component along the PARENT
     *  pipe's own run becomes the crossbar's new parentPipeOffset. A pipe
     *  crossbar has no cross-shift concept (recomputePipeAnchors() derives
     *  its origin from parentPipeOffset alone), so unlike dragBarTo() this
     *  is a single degree of freedom. Refuses on a locked bar or in Angled. */
    bool dragPipeBarTo(quint32 barId, const QPointF &px);
    void refit();                                    ///< scale/centre to fit everything
    void collectPoints(QList<QVector3D> &pts) const; ///< every point the fit should frame
    void collectPointsFor(QList<QVector3D> &pts, Kind kind, quint32 id) const;
    /** Every structure in the workspace, for StageKind. */
    QList<QPair<Kind, quint32> > everyStructure() const;

    void drawGrid(QPainter &p) const;
    void drawStructure(QPainter &p) const;
    void drawOneStructure(QPainter &p, Kind kind, quint32 id) const;
    /** Every bar hung on @p parentId, each just another drawOneStructure(Truss)
     *  call. In Angled this is called from inside drawOneStructure() itself
     *  (depth-sorted, so call order doesn't matter); in a flat plane it is
     *  called separately, AFTER fixtures, so a bar is never hidden behind one
     *  ("truss visible .. but behind fixtures .. shouldn't be the case") --
     *  flat planes are plain painter's-algorithm, last drawn wins. */
    void drawChildBars(QPainter &p, quint32 parentId) const;

    /** Distance from the eye, for back-to-front face ordering (Angled only). */
    double viewDepth(const QVector3D &w) const;

    /* ---- Depth-sorted painting (Angled overview) --------------------------
     *
     * Sorting whole OBJECTS by their centroid does not work once they are big
     * and overlapping: a truss spanning the rig has its centroid deep in the
     * middle, so a nearer-centred deck covers it even where the truss really is
     * in front, and a small step loses to a big deck outright. Every piece of
     * geometry is therefore queued as an individual primitive with its own
     * depth, and the whole queue is sorted once before anything is painted. */
    struct DrawOp
    {
        enum Kind { Poly, Line, Dot, Label, Trace };
        double    depth = 0.0;   ///< whole-primitive depth: sorting, and the
                                 ///< fallback when per-vertex depth is unknown
        /** Per-VERTEX depth, parallel to poly. Empty means "use depth for all
         *  of them", which is exactly what one-depth-per-primitive did and is
         *  still right for a dot or a label. A polygon that spans real depth
         *  must fill this in, or it cannot be resolved per pixel. */
        QVector<double> zs;
        /** Per-VERTEX alpha multiplier, parallel to poly. Empty = flat. This is
         *  what lets a beam be one polygon that fades along its length rather
         *  than a stack of slices whose seams read as rings across the light. */
        QVector<double> alphas;
        Kind      kind = Poly;
        QPolygonF poly;          ///< Poly/Line points, or Dot centre in poly[0]
        QColor    fill;          ///< invalid = unfilled
        QColor    pen;
        double    penWidth = 1.0;
        double    radius = 2.0;  ///< Dot
        QString   text;          ///< Label
        bool      light = false; ///< Poly of LIGHT: added, not layered (see ZRaster::light)
    };
    mutable class ZRaster *m_z = nullptr;   ///< the depth-buffered target
    int      m_supersample = 2;   ///< a flat rasteriser has no AA of its own
    int      m_renderThreads = 0; ///< 0 = one per core
    int      m_maxFps = 30;       ///< ceiling on the live repaint rate
    mutable class QThreadPool *m_renderPool = nullptr;
    bool     m_useZBuffer = true; ///< false = the old sorted-primitive painter
    mutable QVector<DrawOp> m_ops;      ///< queue, only while m_collecting
    mutable bool m_collecting = false;

    /** Paint, or queue when collecting. */
    void emitPoly(const QPolygonF &poly, double depth, const QColor &fill,
                  const QColor &pen, double penW, QPainter &p,
                  const QVector<double> &vertexDepths = QVector<double>(),
                  const QVector<double> &vertexAlphas = QVector<double>()) const;
    /** Light rather than a surface: a beam in the air or a pool where it
     *  lands, added to what is behind it instead of layered over it. */
    void emitLight(const QPolygonF &poly, double depth, const QColor &colour, QPainter &p,
                   const QVector<double> &vertexDepths,
                   const QVector<double> &vertexAlphas = QVector<double>()) const;
    /** A line with a depth at EACH end. A truss chord spans the whole rig;
     *  describing it with one averaged depth is the same mistake a face
     *  with one depth was, and leaves webbing punching through a deck. */
    void emitLine(const QPointF &a, const QPointF &b, double za, double zb,
                  const QColor &pen, double penW, QPainter &p) const;
    /** Many dots at one depth in one colour: a strip's pixels are one
     *  surface and never sort against each other. */
    void emitDots(const QVector<QPointF> &pts, double depth, double r,
                  const QColor &fill, QPainter &p,
                  const QVector<double> &pointDepths = QVector<double>()) const;
    void emitDot(const QPointF &c, double depth, double r,
                 const QColor &fill, QPainter &p) const;
    void emitLabel(const QPointF &at, double depth, const QString &text,
                   const QColor &pen, QPainter &p) const;
    /** Sort the queue back-to-front and paint it. */
    void flushOps(QPainter &p) const;
    /** One queued primitive into the depth buffer. */
    void rasterOp(const struct DrawOp &o, bool depthWrite,
                  int rowFrom = 0, int rowTo = INT_MAX) const;

public:
    /** How many primitives the last angled frame drew. Sub-pixel LED grids are
     *  culled, so this falls sharply as you zoom out -- which is what keeps the
     *  live repaint able to keep up on a big rig. */
    int lastPrimitiveCount() const { return m_lastOpCount; }

private:
    /** Paint a solid box from its eight world corners, faces sorted by depth
     *  and shaded by orientation -- what stops a structure being a billboard
     *  that turns to face the viewer. */
    /** @p topAlpha is the opacity of the TOP face (255 = solid, 0 = omitted),
     *  so a clear-topped step shows what is rigged inside it. */
    void drawSolidBox(QPainter &p, const QVector3D corner[8],
                      const QColor &base, const QColor &edge,
                      int topAlpha = 255, bool ambientLit = true,
                      double depthBias = 0.0) const;
    /** An OPEN lattice prism -- chords, end frames and diagonal bracing, with
     *  nothing filled. What a truss or a box-truss tower actually looks like,
     *  and what lets you see the fixtures rigged inside one. */
    void drawLattice(QPainter &p, const QVector3D a[4], const QVector3D b[4],
                     const QColor &col, int bays) const;
    void drawPipe(QPainter &p, const class Pipe *pipe) const;
    void drawFixtures(QPainter &p) const;
    /** One fixture, so the overview can depth-sort fixtures and structures
     *  together in a single pass. */
    /** The emit-vs-room shading rule, shared by fixture bodies and pixels. */
    /** The neutral housing colour of a fixture body, room-lit. A pixel bar's
     *  light comes from its heads, not from tinting the box around them. */
    /** Every fixture's channel values as of the start of this frame, so one
     *  paint shows one instant instead of tearing across the rig. */
    void takeLiveSnapshot() const;
    QByteArray liveValuesFor(class Fixture *fx) const;
    /** What a fixture is DRAWN from: live output when anything is driving
     *  it, otherwise the selected look. */
    QByteArray valuesForDrawing(class Fixture *fx, bool *complete = nullptr) const;
    mutable QHash<quint32, QByteArray> m_liveSnapshot;

    /** Fixtures whose physical body overlaps another fixture's -- rigged in
     *  the same physical space, whether that is two heads stacked on the same
     *  truss offset or a deck fixture dragged into another one's footprint.
     *  Recomputed every frame (see rebuildOverlapWarnings()): cheap, since it
     *  is N^2 simple box comparisons, not the per-fixture palette resolution
     *  that per-frame work elsewhere in this view has to avoid. */
    void rebuildOverlapWarnings() const;
    mutable QSet<quint32> m_overlappingFixtures;
    /** The edge colour a fixture body/beam-stub draws with: the usual
     *  contrast-safe tint, or a flat warning colour when it overlaps another
     *  fixture's -- unmistakable and independent of either fixture's own
     *  colour, which an overlap warning has to be to read as a warning at
     *  all rather than as an odd shade of whatever the fixture already is. */
    QColor overlapAwareEdgeColor(quint32 fid, const QColor &base) const;

    /** Whether fid's PERSISTED locks -- layout, its layer, its group chain, or
     *  the truss/pipe/tower/platform it is mounted on -- forbid moving it.
     *  This is separate from m_locked (this dialog's own throwaway 🔒/🔓
     *  toggle): a fixture tied to a truss that was locked in the Layers panel
     *  or the truss's own context menu used to stay draggable here regardless,
     *  because dragFixtureTo() never looked past m_locked at any of the real,
     *  saved lock state ("if I have a fixture tied to a truss and the truss
     *  is locked .. I can still move it from the studio view"). */
    bool fixtureMovementLocked(quint32 fid) const;

    /** Unit vector from the scene toward the camera, taken from viewDepth()'s
     *  own gradient so there is no second copy of the projection to drift. */
    QVector3D toViewer() const;
    /** Which way a fixture throws its light: a mover's aim, or away from
     *  whatever is holding it. False when there is no defensible answer. */
    bool emitDirection(class Fixture *fx, const struct FixtureRigProps &rp,
                       const struct FixtureVisualTraits &traits, QVector3D &dir) const;
    /** How much of a fixture's output reaches the camera (0..1), from the
     *  definition's declared beam angle. 1.0 when the lens is undeclared. */
    double beamVisibility(class Fixture *fx, const struct FixtureRigProps &rp,
                          const struct FixtureVisualTraits &traits,
                          double beamDeg) const;

    QColor bodyShade() const;
    /** The depth of the nearest face of a corner-8 box. A face sorts as one
     *  value, so this is what anything INSIDE the box has to beat. */
    double boxNearFaceDepth(const QVector3D corner[8]) const;
    QColor shadeLive(const QColor &live, uchar dim, const QColor &unlit,
                     double visibility = 1.0) const;
    /** One head's own live colour, falling back to the fixture-wide colour. */
    QColor pixelColor(Fixture *fx, const QByteArray &values, int head,
                      const QColor &fallback, const QColor &unlit,
                      double visibility, int masterLevel = -2) const;
    void drawOneFixture(QPainter &p, quint32 fid, bool nameEveryone) const;
    /** The whole rig in one back-to-front pass, structures and fixtures
     *  interleaved, so nearer objects cover further ones whatever order they
     *  happen to sit in the workspace. Angled overview only. */
    void drawRigDepthSorted(QPainter &p) const;
    void drawDimensions(QPainter &p) const;   ///< feature width/height labels (ft/m)
    void drawRulers(QPainter &p) const;        ///< height (0=floor) + width (0=centre) rulers
    void drawCursorReadout(QPainter &p) const; ///< crosshair + live height/offset at the pointer
    double structureCentreA() const;           ///< plane-horizontal centre of the structure (metres)
    double structureTopZ() const;              ///< highest point of the structure (metres, for the max marker)
    quint32 hitTestFixture(const QPointF &px) const;
    /** Nearest bar hung on THIS truss (Truss::invalidId() if none/not close
     *  enough), same "distance to the drawn shape" reasoning as hitTestFixture(). */
    quint32 hitTestBar(const QPointF &px) const;
    /** Nearest crossbar hung on the Pipe this editor is showing (a StandKind
     *  editor's own booms, or a PipeKind editor's own boom) -- Pipe::invalidId()
     *  if none/not close enough/wrong kind. Same shape as hitTestBar(), for
     *  a Pipe bar instead of a Truss one. */
    quint32 hitTestPipeBar(const QPointF &px) const;

    /** In-plane (a,b) -> screen offset, and back, with the view rotation
     *  applied. Every world<->pixel path goes through these two. */
    QPointF planeToScreenVec(const QPointF &ab) const;
    QPointF screenVecToPlane(const QPointF &v) const;

    /** A world point varying along the in-plane a (useB=false) or b axis, so a
     *  ruler can measure whichever one the rotation has made vertical. */
    QVector3D axisWorldPoint(bool useB, double val) const;

    /** Edge labels naming which way is which in the current plane (stage
     *  left/right, upstage/downstage, up/floor) -- the plane badge alone does
     *  not say how the projection is oriented. */
    void drawOrientationLabels(QPainter &p) const;

    /** The three stage axes as a small labelled tripod, for the angled view
     *  where no single screen edge corresponds to one stage direction. */
    void drawAxisTripod(QPainter &p) const;

    /** The fixture's own W x H x D box projected into the current plane: the
     *  screen spans of its width and height axes, plus the axis-aligned screen
     *  box containing the whole solid (depth included). See the implementation
     *  for why a view-dependent projection is needed at all. */
    void fixtureBoxPx(quint32 fid, const struct FixtureVisualTraits &traits,
                      QPointF &wPx, QPointF &hPx, QRectF &boxPx) const;
    /** The same box in WORLD space, for the angled view: a fixture has to keep
     *  its orientation as the camera orbits, not turn to face it. */
    void fixtureBoxCorners(quint32 fid, const struct FixtureVisualTraits &traits,
                           QVector3D out[8]) const;
    /** A moving head as base + yoke arms + head, all oriented boxes, so it
     *  keeps both its shape and its bearing as the camera orbits. @p aim is
     *  the head's pointing direction (null = rest). */
    /** How far a beam travels before it lands: the floor, or the top of a
     *  platform it passes over. A sensible throw when it hits nothing. */
    double beamThrow(const QVector3D &apex, const QVector3D &dir,
                     bool *hit = nullptr, QVector3D *landNormal = nullptr) const;
    /** Whether a beam lands on anything, or is thrown into the room. */
    bool beamHitsSomething(const QVector3D &apex, const QVector3D &dir) const;
    /** The cone from a head out to whatever it lands on, plus the pool where
     *  it lands. Translucent; sorted at its midpoint. */
    /** @p haze scales how visible the beam is in the air only -- not how
     *  much it lights what it lands on -- for beams that share their air,
     *  like the several emitters of one head. */
    /** @p optics, when given and not open, is what the gobo wheels and prism
     *  make of the beam (BeamOptics): the pool is drawn through its mask --
     *  the gobo's pattern on whatever it lands on -- and the haze in the air
     *  carries only the share of light it lets through. */
    void drawBeamCone(QPainter &p, const QVector3D &apex, const QVector3D &dir,
                      double halfAngleRad, const QColor &colour, double level,
                      double haze = 1.0,
                      const BeamOptics::Optics *optics = nullptr) const;
    /** Where a beam ends, as a ring of world points: on the surface it lands
     *  on, the cone's own edge rays cut by that surface -- a circle straight
     *  down, an oval stretched along the beam when it arrives at an angle.
     *  A beam that lands on nothing ends square across its axis at @p length. */
    QVector<QVector3D> beamFootprint(const QVector3D &apex, const QVector3D &dir,
                                     double halfAngleRad, double length,
                                     bool lands,
                                     const QVector3D &landNormal = QVector3D(0, 0, 1)) const;
    /** What one head of a mover is doing: where it points (null = rest) and
     *  the beam it throws. A two-arm fixture has two, and they differ. */
    struct MoverHeadLook
    {
        QVector3D aim;
        double beamLevel = 0.0;
        QColor beamColour;
        BeamOptics::Optics optics;   ///< gobo / prism, for the pool it throws
    };
    /** Seconds on the clock gobo and prism rotation turn by; a test may pin
     *  it (m_opticsTime >= 0). Something turning keeps the view redrawing. */
    double opticsTime() const;
    mutable QElapsedTimer m_opticsClock;
    double m_opticsTime = -1.0;
    mutable bool m_opticsAnimating = false;
    bool m_opticsTickPending = false;
    /** The people targets stand for, as figures in the rig -- on the deck
     *  under them, chest at the aim height, facing downstage, and LIT: each
     *  face takes the colour of every beam passing through it that it faces,
     *  added up, over the room light. Only Person targets: every other kind is
     *  a point the light is aimed at, and a point has nothing to draw. */
    void drawStageFigures(QPainter &p) const;

    /** A beam drawn this frame, kept so the figures it passes through can be
     *  lit by it. Filled by drawMoverSolid() during the depth-sorted pass,
     *  which draws fixtures before figures. */
    struct FrameBeam
    {
        QVector3D apex;
        QVector3D dir;
        double    halfAngle;
        double    length;
        QColor    colour;
        double    level;
    };
    mutable QVector<FrameBeam> m_frameBeams;
    /** @p heads holds one look per physical head; a shorter list reuses its
     *  last entry, so a single look drives every head alike. */
    void drawMoverSolid(QPainter &p, quint32 fid,
                        const struct FixtureVisualTraits &traits,
                        const QColor &col, const QVector<MoverHeadLook> &heads,
                        bool ambientLit = true, double beamAngle = 0.0) const;

    double fixtureLenM(quint32 fid) const;             ///< physical length (metres)
    /** A fixture's world CENTRE, correcting the plot's top-left anchor for
     *  positions that come straight from the stored XY. See the definition. */
    QVector3D fixtureCentre(quint32 fid) const;

    QVector3D fixtureAxisLocal(const struct FixtureRigProps &rp) const; ///< unit long axis in the frame

    /** The fixture's own orthonormal frame: @p L along its length, @p N out of
     *  the surface it is mounted on, @p H the remaining in-face axis. Three
     *  places built this by hand from the same studioMount switch. */
    void fixtureFrame(const struct FixtureRigProps &rp,
                      QVector3D &L, QVector3D &N, QVector3D &H) const;

    /** Which way is UP for a fixture's BODY -- the direction its head sits from
     *  its base. fixtureFrame()'s H is an axis, not a direction: its sign is
     *  whatever the cross product happened to produce, which is fine for the
     *  symmetric boxes most fixtures draw as and wrong for anything with a top
     *  and a bottom. Only the mover silhouette can tell the difference, and it
     *  is the one thing in the rig view that visibly has. */
    QVector3D fixtureBodyUp(const struct FixtureRigProps &rp) const;
    /** True when @p rp has a real structural mount (truss/pipe/tower/riser/
     *  deck) set. Attaching a fixture to any of these ALSO, as a side effect,
     *  usually makes it a member of that structure's own auto-created frame
     *  group (purely for Layers-tree/canvas select-together convenience) --
     *  that membership must never be treated as the fixture's actual
     *  positioning mechanism when a more specific one is already set. Used
     *  everywhere this class would otherwise check fixtureFrameGroup() first,
     *  to keep that check consistent with MonitorProperties::
     *  fixtureRigPosition()'s own precedence (structural mount wins; frame
     *  group is the fallback for a fixture with no other mount at all). */
    static bool hasStructuralMount(const struct FixtureRigProps &rp);
    QVector3D fixtureEndA(quint32 fid) const;          ///< world end A of the bar
    QVector3D fixtureEndB(quint32 fid) const;          ///< world end B of the bar
    /** Screen-space bounding rect of a tower-shelf-mounted fixture's drawn
     *  body (see drawFixtures()) in the current Front/Side plane -- the SAME
     *  geometry drawFixtures() paints, so hitTestFixture() can hit-test the
     *  shape actually on screen instead of the (irrelevant here) generic bar
     *  line. Only meaningful when the fixture is tower-shelf-mounted and
     *  m_plane is Front or Side; callers are expected to check that first. */
    QRectF towerFixtureBodyRect(quint32 fid) const;
    /** Radius (screen px) of a Mover's drawn head circle -- shared between
     *  drawFixtures() and hitTestFixture() so the hit area always matches
     *  what's on screen. Sized from the fixture's declared Physical width
     *  when there is one (most bundled/legacy defs still leave it at 0),
     *  otherwise falls back to the hasFocus heuristic (a focus/zoom channel
     *  implies a physically larger lens assembly). */
    double moverBaseRadius(const struct FixtureVisualTraits &traits) const;
    /** Total on-screen width of a mover unit (its declared Physical width). */
    double moverWidthPx(const struct FixtureVisualTraits &traits) const;
    /** For the anchor platform: which local component the given face pins, and to
     *  what value. mount 0=Top(pin Z),1=Front(pin Y),2=Side(pin X). */
    void facePin(int mount, int &pinComp, double &pinVal) const;

    QList<const class Pipe *> standPipes(quint32 id) const;  ///< pipes on that stand

private:
    Doc     *m_doc;
    Kind     m_kind;
    quint32  m_id;
    Plane    m_plane = Front;
    bool     m_beams = true;   ///< draw beam cones for lit movers
    quint32  m_activeSceneId = 0xFFFFFFFF;
    bool     m_lookRefreshPending = false;   ///< coalesces re-resolves
    /** Where a fixture is aimed, and in whose colour to say so.
     *
     *  The target's ID rather than its position, deliberately: a target gets
     *  dragged about with a mouse or a joystick, and caching where it WAS meant
     *  the rig view kept pointing there. The expensive part -- which fixtures a
     *  scene aims, and what it sends them -- is what the cache is for; a
     *  position is a hash lookup away and is read fresh every frame. */
    struct AimSpec
    {
        quint32 targetId = 0xFFFFFFFF;
        QColor  colour;
        /** A follow-spot scene aims at the SUBJECT -- deck height plus
         *  MonitorProperties::aimSubjectHeight() -- not at the target's own Z,
         *  so the beam lands on someone's chest rather than their feet. This is
         *  QLCPalette's rule; the two have to agree or Edit and the rig view
         *  point different ways. */
        bool    subjectHeight = false;
    };
    /** The live world point a fixture is aimed at, or false if it aims at
     *  nothing. Resolved per frame from the target's CURRENT position. */
    bool aimPointFor(quint32 fid, QVector3D &out) const;
    /** fixture id -> the target it is aimed at, from the active scene's Aim
     *  palettes. Rebuilt when the scene or the rig changes, NOT per frame:
     *  resolving it per fixture per frame would be quadratic in the rig. */
    QHash<quint32, AimSpec> m_aimTarget;
    void rebuildAimTargets();
    /** What the selected scene would SEND, per fixture, as a channel-value
     *  array shaped exactly like live DMX -- so every colour, level and beam
     *  rule downstream reads it without knowing the difference. */
    QHash<quint32, QByteArray> m_sceneValues;
    /** A dashed run from a head to where it is aimed -- the same thing the
     *  studio draws, so the two views say it the same way. */
    void drawAimTrace(QPainter &p, const QVector3D &from, const QVector3D &to,
                      const QColor &colour) const;
    int      m_rotation = 0;   ///< view turn, 0..3 quarter turns clockwise
    /* A gentle three-quarter reads as a rigging drawing rather than a drafting
       projection: the truss body shows in three-quarter, fixtures stay legible,
       and upstage/downstage separates without a long truss sweeping steeply off
       across the frame the way a true isometric makes it. */
    double   m_azimuthDeg = 20.0;    ///< Angled plane: swing around the stage
    double   m_elevationDeg = 30.0;  ///< Angled plane: eye height above the floor
    bool     m_liveValues = false;   ///< paint DMX output instead of gel colours
    double   m_ambient = 0.62;       ///< room light, 0 = blackout .. 1 = work light
    class QTimer *m_liveTimer = nullptr;
    double   m_frameMs = 0.0;        ///< smoothed repaint cost, for the live rate
    mutable int m_lastOpCount = 0;   ///< primitives in the last angled frame
    bool     m_zoomed = false;       ///< zoomed or panned by hand: refit() must not stomp it
    bool     m_orbiting = false;     ///< dragging the empty canvas to swing the camera
    QPointF  m_orbitLast;

    double   m_scale = 60.0;    ///< pixels per metre
    QPointF  m_originPx;        ///< where world (a=0,b=0) lands on screen
    bool     m_panning = false;
    QPointF  m_panLast;
    quint32  m_dragFid = Fixture::invalidId();   ///< fixture being dragged
    quint32  m_dragBarId = UINT_MAX;             ///< bar being dragged (== Truss::invalidId())
    quint32  m_dragPipeBarId = UINT_MAX;         ///< crossbar being dragged (== Pipe::invalidId())
    bool     m_dragged = false;
    quint32  m_resizeBoom = 0; ///< +1 boom id whose top is being dragged (0 = none)
    QPointF  m_cursorPx;       ///< last pointer position (for the ruler readout)
    bool     m_hasCursor = false;
    bool     m_locked = true;   ///< movement prevented until unlocked
    QSet<quint32> m_highlight;  ///< ring-highlighted fixtures (tree selection)
    quint32  m_selectedBarId = UINT_MAX;  ///< highlighted bar (canvas/tree selection)
    quint32  m_selectedPipeBarId = UINT_MAX;  ///< highlighted crossbar (canvas/tree selection)
};

/** @} */

#endif // STRUCTURESTUDIOVIEW_H

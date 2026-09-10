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
#include <QList>
#include <QSet>
#include <QPair>
#include <QVector>

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
                 StageKind = 6 };
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

    /** Ring-highlight a set of fixtures (driven by the tree selection). */
    void setHighlight(const QList<quint32> &ids);

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

protected:
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
    /** Remap a dragged fixture's mount param from a screen point (edits pipe
     *  offset / tower U-V-shelf / truss offset in place). Returns true if moved. */
    bool dragFixtureTo(quint32 fid, const QPointF &px);
    void refit();                                    ///< scale/centre to fit everything
    void collectPoints(QList<QVector3D> &pts) const; ///< every point the fit should frame
    void collectPointsFor(QList<QVector3D> &pts, Kind kind, quint32 id) const;
    /** Every structure in the workspace, for StageKind. */
    QList<QPair<Kind, quint32> > everyStructure() const;

    void drawGrid(QPainter &p) const;
    void drawStructure(QPainter &p) const;
    void drawOneStructure(QPainter &p, Kind kind, quint32 id) const;

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
        enum Kind { Poly, Line, Dot, Label };
        double    depth = 0.0;
        Kind      kind = Poly;
        QPolygonF poly;          ///< Poly/Line points, or Dot centre in poly[0]
        QColor    fill;          ///< invalid = unfilled
        QColor    pen;
        double    penWidth = 1.0;
        double    radius = 2.0;  ///< Dot
        QString   text;          ///< Label
    };
    mutable QVector<DrawOp> m_ops;      ///< queue, only while m_collecting
    mutable bool m_collecting = false;

    /** Paint, or queue when collecting. */
    void emitPoly(const QPolygonF &poly, double depth, const QColor &fill,
                  const QColor &pen, double penW, QPainter &p) const;
    void emitLine(const QPointF &a, const QPointF &b, double depth,
                  const QColor &pen, double penW, QPainter &p) const;
    void emitDot(const QPointF &c, double depth, double r,
                 const QColor &fill, QPainter &p) const;
    void emitLabel(const QPointF &at, double depth, const QString &text,
                   const QColor &pen, QPainter &p) const;
    /** Sort the queue back-to-front and paint it. */
    void flushOps(QPainter &p) const;

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
                      int topAlpha = 255, bool ambientLit = true) const;
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
    QColor bodyShade() const;
    QColor shadeLive(const QColor &live, uchar dim, const QColor &unlit) const;
    /** One head's own live colour, falling back to the fixture-wide colour. */
    QColor pixelColor(Fixture *fx, int head, const QColor &fallback,
                      const QColor &unlit) const;
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
    void drawMoverSolid(QPainter &p, quint32 fid,
                        const struct FixtureVisualTraits &traits,
                        const QColor &col, const QVector3D &aim,
                        bool ambientLit = true) const;

    double fixtureLenM(quint32 fid) const;             ///< physical length (metres)
    QVector3D fixtureAxisLocal(const struct FixtureRigProps &rp) const; ///< unit long axis in the frame
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
    bool     m_dragged = false;
    quint32  m_resizeBoom = 0; ///< +1 boom id whose top is being dragged (0 = none)
    QPointF  m_cursorPx;       ///< last pointer position (for the ruler readout)
    bool     m_hasCursor = false;
    bool     m_locked = true;   ///< movement prevented until unlocked
    QSet<quint32> m_highlight;  ///< ring-highlighted fixtures (tree selection)
};

/** @} */

#endif // STRUCTURESTUDIOVIEW_H

/*
  Q Light Controller Plus
  truss.h

  A Truss represents a physical rigging structure (horizontal bar, vertical
  tower, or ground position) that holds fixtures at defined offsets.  The
  truss geometry is used to derive each fixture's 3-D stage position so that
  position palettes can compute per-fixture pan/tilt angles toward named stage
  targets.

  Coordinate convention (same as the 2-D Monitor):
    X = stage left (positive) / stage right (negative)
    Y = downstage (positive) / upstage (negative)
    (These match how the app BEHAVES -- see barFaceVector() in
    monitorproperties.cpp and any real show: in stage-structures-demo.qxw the
    "SR Tower" sits at X=0.21 with the "SL Tower" at X=11.61, and the upstage
    platforms are at Y=1.53 against the downstage ones at Y=3.97. The plot
    draws +X rightward and +Y downward, which is the standard ground plan:
    audience at the bottom of the page, upstage at the top, stage right on the
    viewer's left. These comments used to say the opposite on both axes.)
    Z = height above stage floor (metres, always positive)

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef TRUSS_H
#define TRUSS_H

#include <QObject>
#include <QVector3D>
#include <QtMath>
#include <QPointF>
#include <QString>
#include <QColor>
#include <climits>

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

class Truss final : public QObject
{
    Q_OBJECT

public:
    /** Type determines which coordinate is free (varies per fixture) and
     *  which are fixed by the truss placement. */
    enum TrussType
    {
        Horizontal = 0, ///< Fixed Z (and one of X/Y); free along direction vector
        Vertical,       ///< Fixed X, Y; free in Z (tower / pipe)
        Ground          ///< Z = 0; free X, Y (floor fixtures)
    };

    /** Cross-section shape — display-only in P0; affects side-view rendering. */
    enum Profile
    {
        SquareTruss = 0,
        TriangleTruss,
        IBeam,
        Pipe,
        Other
    };

    /** How a fixture is attached to the truss. Affects tilt-angle convention
     *  when computing pan/tilt from a stage target. */
    enum MountingType
    {
        TopHung = 0,    ///< Fixture hangs below the truss; tilt 0° = beam up
        FloorMounted,   ///< Fixture sits on top / on the floor; tilt 0° = beam up
        SideArm         ///< Fixture extends to the side; needs body-roll (future)
    };

    static quint32 invalidId() { return UINT_MAX; }

    explicit Truss(quint32 id, QObject *parent = nullptr);

    /********************************************************************
     * Identity
     ********************************************************************/
    quint32 id() const { return m_id; }

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    /********************************************************************
     * Geometry
     ********************************************************************/
    TrussType type() const { return m_type; }
    void setType(TrussType t) { m_type = t; }

    /** Origin in stage metres.  For Horizontal trusses the Z component is
     *  the fixture-mounting height.  For Vertical trusses it is the base
     *  height (bottom of tower; fixtures stack upward from here). */
    QVector3D origin() const { return m_origin; }
    void setOrigin(const QVector3D &o) { m_origin = o; }

    /** Unit direction vector in the XY plane (Horizontal trusses only).
     *  Stored normalised; (1,0) = running stage-right, (0,1) = running
     *  upstage, etc. */
    QPointF direction() const { return m_direction; }
    void setDirection(const QPointF &d);   ///< normalises on set

    /** Physical length of the truss in metres.  Used for display bounds and
     *  clamping truss offsets. */
    float length() const { return m_length; }
    void setLength(float l) { m_length = l; }

    /** Physical width / depth of the truss cross-section in metres.
     *  Display-only in P0 — does not affect position computation. */
    float width() const { return m_width; }
    void setWidth(float w) { m_width = w; }

    Profile profile() const { return m_profile; }
    void setProfile(Profile p) { m_profile = p; }

    bool locked() const { return m_locked; }
    void setLocked(bool l) { m_locked = l; }

    /** Organizational layer this item belongs to on the 2D map (0 = Default). */
    quint32 layerId() const { return m_layerId; }
    void setLayerId(quint32 id) { m_layerId = id; }

    /** Group this item belongs to on the 2D map (0 = ungrouped). Grouped items
     *  select and move together. */
    quint32 groupId() const { return m_groupId; }
    void setGroupId(quint32 id) { m_groupId = id; }

    /** Child-bar attachment (truss-LOCAL model). A truss can be a "bar" hung on
     *  a PARENT truss; its full world geometry (origin/direction/type) is DERIVED
     *  by MonitorProperties::recomputeChildTrusses() from these parent-relative
     *  parameters, so it always follows the parent:
     *    - parentOffset : ALONG the parent (metres from the parent's origin).
     *    - barFace      : which face of the truss it rides (stage-relative).
     *    - barStandoff  : distance off that face (0 = on the face).
     *    - barRun       : Along (parallel) / Across (perpendicular pipe) / Drop.
     *  invalidId() parent = a free, independently-placed truss. */
    enum BarFace { FaceBottom = 0, FaceTop = 1, FaceDownstage = 2,
                   FaceUpstage = 3, FaceStageRight = 4, FaceStageLeft = 5 };
    enum BarRun  { RunAlong = 0, RunAcross = 1, RunDrop = 2 };

    quint32 parentTrussId() const { return m_parentTrussId; }
    void setParentTrussId(quint32 id) { m_parentTrussId = id; }
    float parentOffset() const { return m_parentOffset; }        ///< "Along"
    void setParentOffset(float m) { m_parentOffset = m; }
    int barFace() const { return m_barFace; }
    void setBarFace(int f) { m_barFace = f; }
    float barStandoff() const { return m_barStandoff; }
    void setBarStandoff(float m) { m_barStandoff = m; }
    int barRun() const { return m_barRun; }
    void setBarRun(int r) { m_barRun = r; }
    /** Shift (metres) of the bar along its OWN run relative to the attach point:
     *  0 = centred on the attach; used to slide a crossbar left/right so the
     *  truss meets it off-centre. */
    float barCrossShift() const { return m_barCrossShift; }
    void setBarCrossShift(float m) { m_barCrossShift = m; }
    bool isChildBar() const { return m_parentTrussId != invalidId(); }

    /** Stand this truss stands ON (its origin is derived from the stand top in
     *  recomputeChildTrusses). invalid = not stand-mounted. A truss can't be
     *  both a child bar and stand-mounted. */
    quint32 standId() const { return m_standId; }
    void setStandId(quint32 id) { m_standId = id; }
    bool isStandMounted() const { return m_standId != invalidId(); }

    /** The stand its OTHER end rests on: a horizontal truss spanning two
     *  stands (a goal post, a T-bar between two lifts) runs from the first
     *  stand's top to this one's -- its direction and length follow the
     *  stands (recomputeStandMounts()). Never for a vertical truss. */
    quint32 endStandId() const { return m_endStandId; }
    void setEndStandId(quint32 id) { m_endStandId = id; }
    bool spansStands() const
    { return isStandMounted() && m_endStandId != invalidId() && m_endStandId != m_standId
             && m_type != Vertical; }

    /** A SOCK: a diffusing fabric sleeve over part of the truss, lit from
     *  within by the LED bars rigged Inside it -- they glow through it rather
     *  than throwing beams. @c sockColour is the fabric (white by default);
     *  it covers from @c sockFrom to @c sockTo metres along the truss. */
    bool hasSock() const { return m_sock; }
    void setSock(bool on) { m_sock = on; }
    QColor sockColour() const { return m_sockColour; }
    void setSockColour(const QColor &c) { m_sockColour = c.isValid() ? c : QColor(Qt::white); }
    /** Metres along, within the truss; To < 0 (the default) is its far end. */
    float sockFrom() const { return qBound(0.0f, m_sockFrom, m_length); }
    float sockTo() const { return m_sockTo < 0.0f ? m_length : qBound(sockFrom(), m_sockTo, m_length); }
    void setSockRange(float from, float to) { m_sockFrom = qMax(0.0f, from); m_sockTo = to; }
    /** An image file printed on the sock (stretched over each side of it);
     *  empty for plain fabric. */
    QString sockPattern() const { return m_sockPattern; }
    void setSockPattern(const QString &path) { m_sockPattern = path; }
    /** How much of the print shows over the fabric's own colour: 1 (the
     *  default) all of it, less and the fabric shows through. */
    float sockPatternOpacity() const { return m_sockPatternOpacity; }
    void setSockPatternOpacity(float o) { m_sockPatternOpacity = qBound(0.0f, o, 1.0f); }
    /** Whether @p offset (metres along) is under the sock. */
    bool sockCovers(float offset) const
    { return m_sock && offset >= sockFrom() - 1e-4f && offset <= sockTo() + 1e-4f; }

    /** Compute the world-space position of a fixture whose truss-offset
     *  (metres from origin along the truss direction) is @p offset. */
    QVector3D positionAt(float offset) const;

    /********************************************************************
     * String helpers
     ********************************************************************/
    static QString typeToString(TrussType t);
    static TrussType stringToType(const QString &s);
    static QString profileToString(Profile p);
    static Profile stringToProfile(const QString &s);
    static QString mountingToString(MountingType m);
    static MountingType stringToMounting(const QString &s);

    /********************************************************************
     * Load & Save
     ********************************************************************/
    bool loadXML(QXmlStreamReader &root);
    /** With @p mainDocument, the sock pattern is saved relative to the show
     *  when it is in the show's folder (Doc::normalizeComponentPath()). */
    bool saveXML(QXmlStreamWriter *doc, const Doc *mainDocument = nullptr) const;

    /** The show's own trim (origin Z), held while a venue trims it
     *  differently (board Q128): saveXML() writes it in place. */
    void setBaseTrim(float z) { m_hasBaseTrim = true; m_baseTrim = z; }
    void clearBaseTrim() { m_hasBaseTrim = false; }
    bool hasBaseTrim() const { return m_hasBaseTrim; }
    float baseTrim() const { return m_hasBaseTrim ? m_baseTrim : m_origin.z(); }

private:
    quint32   m_id;
    QString   m_name;
    bool      m_hasBaseTrim = false;
    float     m_baseTrim = 0.0f;
    TrussType m_type;
    QVector3D m_origin;
    QPointF   m_direction;  ///< normalised
    float     m_length;
    float     m_width;
    Profile   m_profile;
    bool      m_locked = false;
    quint32   m_layerId = 0;   ///< 2D-map organizational layer (0 = Default)
    quint32   m_groupId = 0;   ///< 2D-map group (0 = ungrouped)
    quint32   m_parentTrussId = invalidId();  ///< parent truss for a child bar
    float     m_parentOffset = 0.0f;          ///< "Along": metres along the parent
    int       m_barFace = FaceBottom;         ///< face of the truss the bar rides
    float     m_barStandoff = 0.0f;           ///< metres off that face
    int       m_barRun = RunAlong;            ///< Along / Across / Drop
    float     m_barCrossShift = 0.0f;         ///< shift along the run (0 = centred)
    quint32   m_standId = invalidId();        ///< stand this truss stands on
    quint32   m_endStandId = invalidId();     ///< stand its other end rests on
    bool      m_sock = false;                 ///< a fabric sleeve over it
    QColor    m_sockColour = QColor(Qt::white);
    float     m_sockFrom = 0.0f;              ///< metres along
    float     m_sockTo = -1.0f;               ///< metres along; < 0 = its far end
    QString   m_sockPattern;                  ///< image printed on it
    float     m_sockPatternOpacity = 1.0f;    ///< 0..1, the print over the fabric
};

/** Per-fixture rig assignment.  Stored alongside MonitorProperties visual
 *  data; one entry per fixture ID. */
struct FixtureRigProps
{
    quint32          trussId      = Truss::invalidId(); ///< invalid = free-placed
    float            trussOffset  = 0.0f;              ///< metres along truss
    Truss::MountingType mountingType = Truss::TopHung;
    /** Stage yaw of pan=0°: degrees clockwise from downstage (0 = fixture
     *  faces directly downstage when pan is at its centre DMX value). */
    float            panZeroDir   = 0.0f;
    /** Additive calibration offsets applied after target-geometry computation. */
    float            panOffsetDeg  = 0.0f;
    float            tiltOffsetDeg = 0.0f;
    /** Invert pan/tilt direction.  Some fixtures physically pan/tilt the opposite
     *  way from the DMX convention assumed by the geometry (increasing DMX = wrong
     *  physical direction).  Enabling invert mirrors the computed DMX value around
     *  the fixture's centre: effective = panMax - computed (before qBound). */
    bool             panInvert  = false;
    bool             tiltInvert = false;

    /** A FIXED light (no pan or tilt channels) aimed like a par on its yoke
     *  (board Q100): panZeroDir is its bearing seen from above, degrees
     *  clockwise from downstage (90 = stage right), and aimDown how far it
     *  points down from level (90 straight down, 0 level, -90 straight up).
     *  Off (the default, and every older show): it points as its mount and
     *  face say. */
    bool             fixedAim = false;
    float            aimDown  = 90.0f;
    /** What a light with a lens range but no zoom channel (a fixed spot or
     *  wash zoomed by hand) is set to, degrees; 0 = not set (its widest).
     *  Held to the definition's lens range (RigHeads::beamAngle()). */
    float            lensDeg  = 0.0f;
    /** The beam of an aimed fixed light, a unit vector (+Y downstage, +Z up).
     *  False when it isn't aimed. */
    bool fixedAimDirection(QVector3D &dir) const
    {
        if (!fixedAim)
            return false;
        const double b = qDegreesToRadians(double(panZeroDir));
        const double d = qDegreesToRadians(double(qBound(-90.0f, aimDown, 90.0f)));
        dir = QVector3D(float(-qSin(b) * qCos(d)), float(qCos(b) * qCos(d)), float(-qSin(d)));
        return true;
    }

    /** Riser (stage-platform) face mount. When riserPlatformId is not invalid,
     *  the fixture is mounted on a platform's face and its world position is
     *  DERIVED from the platform geometry (so it follows the riser). Mutually
     *  exclusive with truss binding in practice.
     *   - riserFace: 0 = front (downstage) face, 1 = top surface.
     *   - riserU: metres across the platform width (X extent), 0..width.
     *   - riserV: front face → height up the face (Z, 0..height);
     *             top face   → depth into the platform (Y, 0..depth). */
    quint32          riserPlatformId = UINT_MAX;   ///< invalid = not riser-mounted
    int              riserFace = 0;                ///< 0 = front, 1 = top
    float            riserU = 0.0f;
    float            riserV = 0.0f;

    /** Vertical side a truss-bound fixture hangs on, relative to the truss
     *  chord. Only affects the fixture's Z (elevation views): under-hung sits
     *  below the truss (the physical norm), top-mounted rests on top, centered
     *  stays on the truss centreline. */
    enum TrussMountSide { UnderHung = 0, TopMounted = 1, Centered = 2 };
    int              trussMountSide = UnderHung;

    /** Horizontal position ACROSS the truss (metres, perpendicular to the run in
     *  the horizontal plane). 0 = on the truss centreline; negative = stage-left
     *  side of the run, positive = stage-right side. Lets a fixture clamp to the
     *  near/far chord (Left/Right) instead of dead-centre while staying attached. */
    float            trussCross = 0.0f;

    /** Second horizontal offset (metres), VERTICAL runs only. A vertical run's
     *  axis IS the Z axis, so BOTH horizontal directions are free around it —
     *  one scalar cannot say where on a tower a fixture is clamped.
     *  @c trussCross is the X offset and this is the Y one. Meaningless for a
     *  horizontal run, whose two non-axis freedoms are the horizontal normal
     *  (@c trussCross) and height (@c mountZOffset). */
    float            trussCrossY = 0.0f;

    /** Fine height nudge (metres, +up) applied on TOP of a structural mount
     *  (truss / pipe / tower) — e.g. to raise a followspot above the truss it
     *  sits on. 0 = flush with the mount. */
    float            mountZOffset = 0.0f;

    /** On a hanger (Hanger: a bracket and chain on a truss or tower): the
     *  fixture hangs from its hook. UINT_MAX = not on one. */
    quint32          hangerId = UINT_MAX;

    /** What makes its light, where the definition can say (QLCPhysical::leds()):
     *  -1 = as the definition says (or as the device reports), 0 = one point,
     *  N = that many LEDs -- this one's real build. @c lightLayout is a
     *  QLCPhysical::LedLayout (hanging cluster, tape), -1 = as defined. */
    int              lightLeds = -1;
    int              lightLayout = -1;
    /** Its glass: -1 = as the definition says, 0 = diffusing, 1 = clear. */
    int              lightGlass = -1;

    /** Deck mount: a fixture standing ON TOP of a stage platform ("floor
     *  mounted"). Unlike a riser FACE mount it keeps its free XY position; only
     *  its Z is derived — the platform's top height plus @c deckHeightOffset
     *  (0 = base sits on the deck). Invalid platform id = not deck-mounted. */
    quint32          deckPlatformId = UINT_MAX;
    float            deckHeightOffset = 0.0f;       ///< metres above the deck top

    /** Studio-group frame mount. When the fixture's monitor group (or an
     *  ancestor) carries a frame (MonitorProperties::MonitorGroup::hasFrame),
     *  its world position is DERIVED as group.origin + Rz(group.rotation) *
     *  groupLocal, where groupLocal is in METRES relative to the group's
     *  local-frame origin. Takes precedence over free/truss/riser placement.
     *  Membership itself lives in the PreviewItem groupId; this only carries
     *  the local offset within that frame. */
    QVector3D        groupLocal;                   ///< metres, group-local offset

    /** Studio long-axis orientation. A fixture (e.g. an LED tape) has a long
     *  axis that lies in one of the frame's planes; this records which plane and
     *  the in-plane angle so the studio editor can project it correctly — full
     *  length where the axis lies in the view, a dot where it is perpendicular.
     *   studioMount: 0 = Top/deck (axis in X-Y), 1 = Front/face (axis in X-Z),
     *                2 = Side (axis in Y-Z).
     *   studioAngle: degrees, 0 = along the horizontal axis of that plane. */
    int              studioMount = 1;              ///< default: front face
    float            studioAngle = 0.0f;

    /** ROLL about the fixture's own long axis, degrees.
     *
     *  studioAngle turns a fixture WITHIN its mounting face -- on a step's top
     *  that spins it flat, like a compass bearing. This tips it instead: the
     *  emitting face rotates about the length of the bar, so a unit lying in a
     *  step can be angled up toward the cast rather than straight up, which is
     *  how they are actually built (bar light on a diagonal inside the box,
     *  firing up through the plexi at an angle).
     *
     *  Positive tips the face AWAY from the in-face axis: for a bar lying on a
     *  TOP face and running stage left-right that is UPSTAGE, away from the
     *  audience (+Y is downstage).
     *  0 = flat in its mounting face, which is every fixture that predates
     *  this, so old files are unchanged. */
    float            studioTilt = 0.0f;
    /** A turn of the whole fixture about the vertical, degrees (+X toward
     *  +Y, as everything on the plot turns): the face it is on, and so its
     *  beam, turned with it. What a group or a platform/tower being turned
     *  adds to everything on it. */
    float            studioYaw = 0.0f;

    /** Faces the OTHER way across its mounting plane: a 180-degree roll about
     *  its own length. studioMount names a plane, not a direction -- the front
     *  plane is faced from downstage (+Y), the side plane from stage left
     *  (+X), the top from above -- so without this nothing could say Back
     *  (upstage), Stage right or Bottom ("no way to put this fixture on the
     *  back? also when I say side .. which side?"). See face()/setFace(). */
    bool             studioFlip = false;
    /** The face was CHOSEN (setFixtureFace(), or read from a file that names
     *  it), not left at the Front default. The load-time repair that moves a
     *  stray default Front onto the Top plane for a deck or a horizontal truss
     *  must not undo a deliberate Front or Back. */
    bool             studioFaceChosen = false;

    /** The six faces a fixture can be on, as a direction: which way it faces
     *  out. Each is a plane (studioMount) and a side of it (studioFlip). */
    enum Face { FaceTop = 0, FaceBottom, FaceFront, FaceBack, FaceStageLeft, FaceStageRight };
    /** The unit vector along its length (its LED grid's columns), from
     *  studioMount and studioAngle -- the same as the rig view draws. */
    QVector3D longAxis() const
    {
        const double th = qDegreesToRadians(double(studioAngle));
        const float c = float(qCos(th)), s = float(qSin(th));
        switch (studioMount)
        {
        case 0:  return QVector3D(c, s, 0.0f);       // Top: X-Y
        case 2:  return QVector3D(0.0f, c, s);       // Side: Y-Z
        default: return QVector3D(c, 0.0f, s);       // Front: X-Z
        }
    }
    /** The unit vector across it, the way its grid's rows run (row 0 at the
     *  top of a front-facing bar): the mount face's normal crossed with the
     *  length, reversed when it faces the other way. Roll (studioTilt) is
     *  left out: it tips the rows into depth, which a flat grid cannot show. */
    QVector3D acrossAxis() const
    {
        const QVector3D n = (studioMount == 0) ? QVector3D(0, 0, 1)
                          : (studioMount == 2) ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
        QVector3D h = QVector3D::crossProduct(n, longAxis());
        if (h.length() < 1e-6f)
            h = QVector3D(0, 0, -1);
        h.normalize();
        return studioFlip ? -h : h;
    }

    Face face() const
    {
        const int m = qBound(0, studioMount, 2);
        return Face(m == 0 ? (studioFlip ? FaceBottom : FaceTop)
                  : m == 1 ? (studioFlip ? FaceBack : FaceFront)
                           : (studioFlip ? FaceStageRight : FaceStageLeft));
    }

    /** Boom mount. When pipeId is valid the fixture rides a pipe's pipe at
     *  pipeOffset metres up from the base, facing pipeAngle degrees around it.
     *  Its world position is DERIVED from the pipe so it follows it. */
    quint32          pipeId     = UINT_MAX;        ///< invalid = not pipe-mounted
    float            pipeOffset = 0.0f;            ///< metres up the pipe from the base
    float            pipeAngle  = 0.0f;            ///< facing degrees around the pipe

    /** Tower mount. When towerId is valid the fixture rides the tower at
     *  (towerU, towerV) metres into the footprint, at a height derived from
     *  @c towerMountSide: a numbered SHELF (towerShelf, as before), the
     *  tower's own TOP cap, or its BOTTOM/base — real, distinct positions
     *  rather than fake shelf indices. Derived from the tower geometry so it
     *  follows. */
    enum TowerMountSide { TowerShelf = 0, TowerTop = 1, TowerBottom = 2 };
    quint32          towerId    = UINT_MAX;        ///< invalid = not tower-mounted
    int              towerMountSide = TowerShelf;
    int              towerShelf = 0;               ///< shelf index (towerMountSide == TowerShelf only)
    float            towerU     = 0.0f;            ///< metres across the footprint (X)
    float            towerV     = 0.0f;            ///< metres into the footprint (Y)

    static quint32 invalidPlatformId() { return UINT_MAX; }
    bool onRiser() const { return riserPlatformId != UINT_MAX; }
    bool onDeck()  const { return deckPlatformId  != UINT_MAX; }
    bool onPipe()  const { return pipeId != UINT_MAX; }
    bool onTower() const { return towerId != UINT_MAX; }
    enum RiserFace { RiserFront = 0, RiserTop = 1 };

    /** WHERE the fixture sits relative to the thing carrying it, as distinct
     *  from WHICH thing that is.
     *
     *  OnSurface: bolted to the outside -- its back flush with the face, its
     *      body proud of it. The normal case, and what a face-mounted LED bar
     *      on a step nosing is.
     *  Inside: within the structure's volume -- a fixture rigged between a
     *      truss's chords, or sitting inside a clear-topped step firing up
     *      through it. Drawn centred on its position rather than pushed out,
     *      and its height within the volume is settable by dragging in an
     *      elevation.
     *
     *  There was briefly a third, Recessed, meaning "let into the surface". It
     *  was render-only -- it sank the drawn body by half its depth and never
     *  touched the position -- so for a 60 mm bar on a 600 mm step it did
     *  nothing anyone could see, and it read as broken. Removed rather than
     *  left as a control that appears not to work. The value 2 is deliberately
     *  NOT reused, so any file written while it existed loads as OnSurface
     *  rather than silently meaning something else. */
    enum MountPlacement { OnSurface = 0, Inside = 1 };
    int              placement = OnSurface;

    /** The structural mounts are MUTUALLY EXCLUSIVE -- a fixture is clamped to
     *  a truss, or riding a pipe, or on a tower shelf, or standing on a deck,
     *  or bolted to a riser face, never two at once. */
    enum MountKind { NoMount = 0, TrussMount, PipeMount, TowerMount,
                     DeckMount, RiserMount, HangerMount };

    bool onHanger() const { return hangerId != UINT_MAX; }

    /** Which structure this fixture is on, in the SAME precedence
     *  MonitorProperties::fixtureRigPosition() uses to derive its position.
     *  Everything that asks "what is this fixture mounted on?" must go through
     *  here: when a stale second mount was left behind, the position resolver
     *  and the plot's double-click each picked a DIFFERENT one, so a fixture
     *  drawn on its truss opened the editor for an unrelated platform. */
    MountKind primaryMount() const
    {
        if (onHanger())                    return HangerMount;
        if (trussId != Truss::invalidId()) return TrussMount;
        if (onPipe())                      return PipeMount;
        if (onTower())                     return TowerMount;
        if (onDeck())                      return DeckMount;
        if (onRiser())                     return RiserMount;
        return NoMount;
    }

    /** riserFace and studioMount are two answers to ONE question -- which
     *  surface of the step a fixture is on -- and they are numbered OPPOSITELY
     *  (RiserFront = 0, but the front DRAWING plane is 1). Nothing enforced the
     *  pairing and every caller inlined the inversion for itself, so a strip
     *  could end up claiming the front face for its position and the tread for
     *  its geometry: correctly placed along the step nosing, yet drawn lying
     *  flat on top of the step and firing upwards. stage-structures-demo.qxw
     *  shipped with exactly that on all eight of DS3's strips, which is what
     *  made that one step look unlike its ten identical neighbours.
     *
     *  So the mapping lives here, once, and the plane is set through
     *  setStudioMount() so the two cannot drift apart again. */
    static int studioMountForRiserFace(int face)
    { return face == RiserTop ? 0 : 1; }

    /** Lay the fixture on a drawing plane (0 = top, 1 = front, 2 = side) and
     *  take a riser mount's own face with it -- on a riser the face is not
     *  decoration, it is what fixtureRigPosition() derives the position from,
     *  so changing one without the other leaves the fixture drawn somewhere it
     *  is not.
     *
     *  Side is deliberately exempt: a strip running along a step's END is a
     *  position the riser model cannot express at all (it has a front and a
     *  top, and nothing else), so the face is left alone rather than forced to
     *  a wrong one. */
    void setStudioMount(int face)
    {
        studioMount = face;
        if (onRiser() && face != 2)
            riserFace = (face == 0) ? RiserTop : RiserFront;
    }

    /** Unrig it from every structure. Call before setting a new mount so the
     *  kinds stay exclusive -- attachFixtureToTruss()/...ToTower() used to set
     *  their own id and leave any previous one in place. */
    void clearMounts()
    {
        trussId          = Truss::invalidId();
        pipeId           = UINT_MAX;
        towerId          = UINT_MAX;
        deckPlatformId   = UINT_MAX;
        riserPlatformId  = UINT_MAX;
        hangerId         = UINT_MAX;
    }
};

/**
 * The part of a fixture's rig props that focusing a show in a venue changes
 * (docs/design/FOCUS_REFINE_DESIGN.md section 5): a moving head's pan and tilt offsets,
 * a fixed light's aim, a hand-zoomed lens. What a venue's corrections hold,
 * and what the show file keeps as its own while a venue is applied.
 */
struct RigCalibration
{
    float panOffsetDeg = 0.0f;
    float tiltOffsetDeg = 0.0f;
    bool  fixedAim = false;
    float panZeroDir = 0.0f;
    float aimDown = 90.0f;
    float lensDeg = 0.0f;

    static RigCalibration of(const FixtureRigProps &rp)
    {
        RigCalibration c;
        c.panOffsetDeg = rp.panOffsetDeg;
        c.tiltOffsetDeg = rp.tiltOffsetDeg;
        c.fixedAim = rp.fixedAim;
        c.panZeroDir = rp.panZeroDir;
        c.aimDown = rp.aimDown;
        c.lensDeg = rp.lensDeg;
        return c;
    }
    void applyTo(FixtureRigProps &rp) const
    {
        rp.panOffsetDeg = panOffsetDeg;
        rp.tiltOffsetDeg = tiltOffsetDeg;
        rp.fixedAim = fixedAim;
        rp.panZeroDir = panZeroDir;
        rp.aimDown = aimDown;
        rp.lensDeg = lensDeg;
    }
    bool operator==(const RigCalibration &o) const
    {
        return qFuzzyCompare(1.0f + panOffsetDeg, 1.0f + o.panOffsetDeg)
            && qFuzzyCompare(1.0f + tiltOffsetDeg, 1.0f + o.tiltOffsetDeg)
            && fixedAim == o.fixedAim
            && qFuzzyCompare(1.0f + panZeroDir, 1.0f + o.panZeroDir)
            && qFuzzyCompare(1.0f + aimDown, 1.0f + o.aimDown)
            && qFuzzyCompare(1.0f + lensDeg, 1.0f + o.lensDeg);
    }
    bool operator!=(const RigCalibration &o) const { return !(*this == o); }
};

/** @} */

#endif // TRUSS_H

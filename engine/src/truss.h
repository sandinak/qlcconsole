/*
  Q Light Controller Plus
  truss.h

  A Truss represents a physical rigging structure (horizontal bar, vertical
  tower, or ground position) that holds fixtures at defined offsets.  The
  truss geometry is used to derive each fixture's 3-D stage position so that
  position palettes can compute per-fixture pan/tilt angles toward named stage
  targets.

  Coordinate convention (same as the 2-D Monitor):
    X = stage right (positive) / stage left (negative)
    Y = upstage (positive) / downstage (negative)
    Z = height above stage floor (metres, always positive)

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef TRUSS_H
#define TRUSS_H

#include <QObject>
#include <QVector3D>
#include <QPointF>
#include <QString>
#include <climits>

class QXmlStreamReader;
class QXmlStreamWriter;

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
    bool saveXML(QXmlStreamWriter *doc) const;

private:
    quint32   m_id;
    QString   m_name;
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

    /** Boom mount. When pipeId is valid the fixture rides a pipe's pipe at
     *  pipeOffset metres up from the base, facing pipeAngle degrees around it.
     *  Its world position is DERIVED from the pipe so it follows it. */
    quint32          pipeId     = UINT_MAX;        ///< invalid = not pipe-mounted
    float            pipeOffset = 0.0f;            ///< metres up the pipe from the base
    float            pipeAngle  = 0.0f;            ///< facing degrees around the pipe

    /** Tower-shelf mount. When towerId is valid the fixture sits on a tower's
     *  shelf at (towerU, towerV) metres into the footprint. Derived from the
     *  tower geometry so it follows. */
    quint32          towerId    = UINT_MAX;        ///< invalid = not tower-mounted
    int              towerShelf = 0;               ///< shelf index
    float            towerU     = 0.0f;            ///< metres across the footprint (X)
    float            towerV     = 0.0f;            ///< metres into the footprint (Y)

    static quint32 invalidPlatformId() { return UINT_MAX; }
    bool onRiser() const { return riserPlatformId != UINT_MAX; }
    bool onDeck()  const { return deckPlatformId  != UINT_MAX; }
    bool onPipe()  const { return pipeId != UINT_MAX; }
    bool onTower() const { return towerId != UINT_MAX; }
    enum RiserFace { RiserFront = 0, RiserTop = 1 };
};

/** @} */

#endif // TRUSS_H

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
        Vertical,       ///< Fixed X, Y; free in Z (tower / boom)
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
};

/** @} */

#endif // TRUSS_H

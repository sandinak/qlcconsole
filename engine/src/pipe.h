/*
  Q Light Controller Plus
  pipe.h

  A Pipe is a discrete length of lighting pipe/bar that hosts fixtures. It is
  the unified object behind a BOOM (a VERTICAL pipe, on a stand or hung from a
  truss) and a house ELECTRIC (a fixed HORIZONTAL pipe). Its orientation and
  mount decide how it reads:
    - Vertical   + stand           → boom stand
    - Vertical   + hung from truss → drop-arm boom
    - Horizontal + fixed/flown     → house electric / free bar

  Coordinate convention (same as Truss / StagePlatform / the 2-D Monitor):
    X = stage right (+) / stage left (-)
    Y = upstage (+) / downstage (-)
    Z = height above stage floor (metres, always >= 0)

  The pipe starts at (originX, originY, baseZ) and runs `length` metres: UP the
  Z axis when Vertical, or along runAngle in the XY plane when Horizontal.
  Fixtures rig at an offset (metres from the start) and a facing angle.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef PIPE_H
#define PIPE_H

#include <QObject>
#include <QVector3D>
#include <QColor>
#include <QString>
#include <QtMath>
#include <climits>

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

class Pipe final : public QObject
{
    Q_OBJECT

public:
    static quint32 invalidId() { return UINT_MAX; }

    /** Vertical = boom; Horizontal = electric / free bar. */
    enum Orientation { Vertical = 0, Horizontal = 1 };

    explicit Pipe(quint32 id, QObject *parent = nullptr);

    /********************************************************************
     * Identity
     ********************************************************************/
    quint32 id()   const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; }

    Orientation orientation() const { return m_orientation; }
    void setOrientation(Orientation o) { m_orientation = o; }
    bool isVertical() const { return m_orientation == Vertical; }

    /********************************************************************
     * Geometry (metres)
     ********************************************************************/
    /** Start position of the pipe on the stage XY plane. */
    float originX() const { return m_originX; }
    void  setOriginX(float x) { m_originX = x; }
    float originY() const { return m_originY; }
    void  setOriginY(float y) { m_originY = y; }

    /** Height of the pipe start above the floor (Vertical: base; Horizontal: the
     *  hang height of the bar). */
    float baseZ() const { return m_baseZ; }
    void  setBaseZ(float z) { m_baseZ = (z >= 0.0f) ? z : 0.0f; }

    /** Length of the pipe (metres). Vertical: height up; Horizontal: run length. */
    float length() const { return m_length; }
    void  setLength(float l) { m_length = (l > 0.0f) ? l : 0.1f; }
    /** Back-compat alias — the boom code called the extent "height". */
    float height() const { return m_length; }
    void  setHeight(float h) { setLength(h); }

    /** For a Horizontal pipe: run direction in the XY plane (degrees; 0 = stage
     *  right +X, 90 = upstage +Y). Ignored when Vertical. */
    float runAngle() const { return m_runAngle; }
    void  setRunAngle(float deg) { m_runAngle = deg; }

    float diameter() const { return m_diameter; }
    void  setDiameter(float d) { m_diameter = (d > 0.0f) ? d : 0.02f; }

    /** Simple built-in stand base radius (0 = no stand). Superseded by a real
     *  Stand object when the pipe is stand-mounted; kept for a free boom. */
    float baseRadius() const { return m_baseRadius; }
    void  setBaseRadius(float r) { m_baseRadius = (r >= 0.0f) ? r : 0.0f; }
    bool  hasStand() const { return m_baseRadius > 0.0f; }

    /** Parent truss for a truss-hung pipe (drop-arm boom). Base derived from the
     *  truss in MonitorProperties::recomputePipeAnchors(). invalid = free. */
    quint32 parentTrussId() const { return m_parentTrussId; }
    void    setParentTrussId(quint32 id) { m_parentTrussId = id; }
    float   trussOffset() const { return m_trussOffset; }
    void    setTrussOffset(float m) { m_trussOffset = m; }
    bool    isTrussHung() const { return m_parentTrussId != invalidId(); }

    /** Stand this pipe stands on (its base is derived from the stand top). */
    quint32 standId() const { return m_standId; }
    void    setStandId(quint32 id) { m_standId = id; }
    bool    isStandMounted() const { return m_standId != invalidId(); }

    /** World position at @p offset metres from the pipe start. */
    QVector3D positionAt(float offset) const
    {
        if (m_orientation == Vertical)
            return QVector3D(m_originX, m_originY, m_baseZ + offset);
        const double a = qDegreesToRadians(double(m_runAngle));
        return QVector3D(m_originX + offset * float(qCos(a)),
                         m_originY + offset * float(qSin(a)), m_baseZ);
    }

    /********************************************************************
     * Appearance / map behaviour
     ********************************************************************/
    QColor color() const { return m_color; }
    void   setColor(const QColor &c) { m_color = c; }
    bool locked() const { return m_locked; }
    void setLocked(bool l) { m_locked = l; }
    quint32 layerId() const { return m_layerId; }
    void setLayerId(quint32 id) { m_layerId = id; }
    quint32 groupId() const { return m_groupId; }
    void setGroupId(quint32 id) { m_groupId = id; }

    /********************************************************************
     * Load & Save
     ********************************************************************/
    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

private:
    quint32     m_id;
    QString     m_name;
    Orientation m_orientation = Vertical;
    float       m_originX    = 0.0f;
    float       m_originY    = 0.0f;
    float       m_baseZ      = 0.0f;
    float       m_length     = 3.0f;
    float       m_runAngle   = 0.0f;
    float       m_diameter   = 0.048f;
    float       m_baseRadius = 0.40f;
    quint32     m_parentTrussId = UINT_MAX;
    float       m_trussOffset   = 0.0f;
    quint32     m_standId     = UINT_MAX;
    QColor      m_color;
    bool        m_locked     = false;
    quint32     m_layerId    = 0;
    quint32     m_groupId    = 0;
};

/** @} */

#endif // PIPE_H

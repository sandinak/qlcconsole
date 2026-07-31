/*
  Q Light Controller Plus
  boom.h

  A Boom is a discrete VERTICAL lighting pipe that hosts fixtures at heights
  along it. It typically stands on a base (tripod / round base) on the floor,
  but can also be hung (baseRadius == 0). Unlike a Truss it is a first-class,
  single-run object with its own stand.

  Coordinate convention (same as Truss / StagePlatform / the 2-D Monitor):
    X = stage right (+) / stage left (-)
    Y = upstage (+) / downstage (-)
    Z = height above stage floor (metres, always >= 0)

  The base sits at (originX, originY, baseZ) and the pipe runs up to
  baseZ + height. Fixtures rig to the pipe at an offset (metres up from the
  base) and a facing angle around it.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef BOOM_H
#define BOOM_H

#include <QObject>
#include <QVector3D>
#include <QColor>
#include <QString>
#include <climits>

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

class Boom final : public QObject
{
    Q_OBJECT

public:
    static quint32 invalidId() { return UINT_MAX; }

    explicit Boom(quint32 id, QObject *parent = nullptr);

    /********************************************************************
     * Identity
     ********************************************************************/
    quint32 id()   const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; }

    /********************************************************************
     * Geometry (metres)
     ********************************************************************/
    /** Base position of the stand on the stage XY plane. */
    float originX() const { return m_originX; }
    void  setOriginX(float x) { m_originX = x; }
    float originY() const { return m_originY; }
    void  setOriginY(float y) { m_originY = y; }

    /** Height of the base above the floor (0 = on the floor; >0 if on a deck). */
    float baseZ() const { return m_baseZ; }
    void  setBaseZ(float z) { m_baseZ = (z >= 0.0f) ? z : 0.0f; }

    /** Length of the vertical pipe above the base. */
    float height() const { return m_height; }
    void  setHeight(float h) { m_height = (h > 0.0f) ? h : 0.1f; }

    /** Pipe diameter (for drawing / mounting; metres). */
    float diameter() const { return m_diameter; }
    void  setDiameter(float d) { m_diameter = (d > 0.0f) ? d : 0.02f; }

    /** Radius of the stand base footprint. 0 = hung boom (no stand). */
    float baseRadius() const { return m_baseRadius; }
    void  setBaseRadius(float r) { m_baseRadius = (r >= 0.0f) ? r : 0.0f; }
    bool  hasStand() const { return m_baseRadius > 0.0f; }

    /** Parent truss for a truss-hung boom (drop-arm). When set, the boom's base
     *  position is DERIVED from the truss at trussOffset metres along it — the
     *  pipe rises to that point and hangs below it (no stand). invalid = a
     *  free-standing boom placed by its own origin. Derived in
     *  MonitorProperties::recomputeBoomAnchors(). */
    quint32 parentTrussId() const { return m_parentTrussId; }
    void    setParentTrussId(quint32 id) { m_parentTrussId = id; }
    float   trussOffset() const { return m_trussOffset; }
    void    setTrussOffset(float m) { m_trussOffset = m; }
    bool    isTrussHung() const { return m_parentTrussId != invalidId(); }

    /** World position at @p offset metres up the pipe from the base. */
    QVector3D positionAt(float offset) const;

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
    quint32 m_id;
    QString m_name;
    float   m_originX    = 0.0f;
    float   m_originY    = 0.0f;
    float   m_baseZ      = 0.0f;
    float   m_height     = 3.0f;    ///< ~10 ft pipe
    float   m_diameter   = 0.048f;  ///< ~1.9" pipe
    float   m_baseRadius = 0.40f;   ///< tripod/round base footprint
    quint32 m_parentTrussId = UINT_MAX;  ///< truss this boom hangs from (invalid = free)
    float   m_trussOffset   = 0.0f;      ///< metres along the parent truss
    QColor  m_color;
    bool    m_locked     = false;
    quint32 m_layerId    = 0;
    quint32 m_groupId    = 0;
};

/** @} */

#endif // BOOM_H

/*
  Q Light Controller Plus
  scenery.h

  Scenery is a SURFACE on stage rather than something to rig on: a cyclorama,
  a back wall, the main drape, legs and borders, a scrim. Light lands on it,
  and (later) it hides what is behind it. The cyclorama is the first kind; the
  rest share the same geometry -- a vertical sheet with a width, a height and a
  bottom trim, standing along a line on the floor, optionally bowed -- and only
  differ in how they look and behave, so they are one object with a kind.

  Coordinate convention (same as the rest of the 2-D Monitor, see stand.h):
    X = stage left (+) / stage right (-)
    Y = downstage (+) / upstage (-)
    Z = height above stage floor (metres, always >= 0)

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef SCENERY_H
#define SCENERY_H

#include <QObject>
#include <QVector3D>
#include <QPointF>
#include <QColor>
#include <QString>
#include <QList>
#include <climits>

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLScenery QStringLiteral("Scenery")

class Scenery final : public QObject
{
    Q_OBJECT

public:
    /** What the sheet is. Only Cyclorama is offered so far; the others are
     *  named now so a saved show keeps them when they arrive. */
    enum Kind
    {
        Cyclorama = 0,
        BackWall,
        MainDrape,
        Leg,
        Border,
        Traveler,
        Scrim
    };

    static quint32 invalidId() { return UINT_MAX; }
    static QString kindName(int kind);

    explicit Scenery(quint32 id, QObject *parent = nullptr);

    quint32 id()   const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; }

    int  kind() const { return m_kind; }
    void setKind(int k) { m_kind = k; }

    /** The middle of the sheet's bottom edge, on the floor plan (metres). */
    float originX() const { return m_originX; }
    void  setOriginX(float x) { m_originX = x; }
    float originY() const { return m_originY; }
    void  setOriginY(float y) { m_originY = y; }

    float width() const { return m_width; }
    void  setWidth(float w) { m_width = (w > 0.1f) ? w : 0.1f; }
    float height() const { return m_height; }
    void  setHeight(float h) { m_height = (h > 0.1f) ? h : 0.1f; }
    /** Height of the bottom edge above the floor (0 for a cyc to the deck;
     *  more for a border or a flown piece). */
    float bottom() const { return m_bottom; }
    void  setBottom(float z) { m_bottom = (z >= 0.0f) ? z : 0.0f; }

    /** Which way the sheet runs: 0 = along X (stage left-right), facing
     *  downstage. Degrees, turning the same way as everything else on the plot. */
    float rotation() const { return m_rotation; }
    void  setRotation(float deg) { m_rotation = deg; }

    /** How far the middle bows AWAY from the audience (metres): a cyc curves
     *  upstage at its centre. 0 is flat. */
    float bow() const { return m_bow; }
    void  setBow(float m) { m_bow = (m >= 0.0f) ? m : 0.0f; }

    QColor color() const { return m_color; }
    void   setColor(const QColor &c) { m_color = c; }

    bool locked() const { return m_locked; }
    void setLocked(bool l) { m_locked = l; }
    quint32 layerId() const { return m_layerId; }
    void setLayerId(quint32 id) { m_layerId = id; }
    quint32 groupId() const { return m_groupId; }
    void setGroupId(quint32 id) { m_groupId = id; }

    /*********************************************************************
     * Geometry
     *********************************************************************/
    /** The unit vector along the sheet (from its stage-right end to its
     *  stage-left end at rotation 0) and the one it FACES, on the floor. */
    QPointF along() const;
    QPointF facing() const;

    /** A point on the sheet: @p u in [-1, 1] from one end to the other,
     *  at height @p z (absolute, metres). The bow pulls the middle back. */
    QVector3D pointAt(double u, double z) const;

    /** The sheet's line on the floor, @p segments + 1 points end to end. */
    QList<QPointF> baseLine(int segments = 16) const;

    /** Where a ray from @p origin along @p dir first meets the sheet, as a
     *  distance along @p dir; false when it misses (or only grazes past
     *  @p maxT). The bowed sheet is taken as @p segments flat panels.
     *  @p normal, when given, receives the (horizontal, unit) normal of the
     *  panel hit, turned to face the ray -- the surface a pool lies on. */
    bool rayHit(const QVector3D &origin, const QVector3D &dir, double &t,
                double maxT = 1e9, int segments = 16, QVector3D *normal = nullptr) const;

    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

private:
    quint32 m_id;
    QString m_name;
    int     m_kind     = Cyclorama;
    float   m_originX  = 0.0f;
    float   m_originY  = 0.0f;
    float   m_width    = 10.0f;
    float   m_height   = 6.0f;
    float   m_bottom   = 0.0f;
    float   m_rotation = 0.0f;
    float   m_bow      = 0.0f;
    QColor  m_color;
    bool    m_locked   = false;
    quint32 m_layerId  = 0;
    quint32 m_groupId  = 0;
};

/** @} */

#endif

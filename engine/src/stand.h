/*
  Q Light Controller Plus
  stand.h

  A Stand is a placeable SUPPORT BASE (tripod / wind-up / rolling base) on the
  stage floor. It holds something up at its top point: a vertical Pipe (a boom
  stand), a horizontal Pipe (a bar on a T-stand), or a small truss/tower. The
  thing it holds derives its base position from the stand top, so it follows the
  stand when the stand moves.

  Coordinate convention (same as the rest of the 2-D Monitor):
    X = stage right (+) / stage left (-)
    Y = upstage (+) / downstage (-)
    Z = height above stage floor (metres, always >= 0)

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STAND_H
#define STAND_H

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

class Stand final : public QObject
{
    Q_OBJECT

public:
    static quint32 invalidId() { return UINT_MAX; }

    explicit Stand(quint32 id, QObject *parent = nullptr);

    quint32 id()   const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; }

    /** Floor position of the stand base. */
    float originX() const { return m_originX; }
    void  setOriginX(float x) { m_originX = x; }
    float originY() const { return m_originY; }
    void  setOriginY(float y) { m_originY = y; }

    /** Height of the stand top above the floor (where things mount). */
    float height() const { return m_height; }
    void  setHeight(float h) { m_height = (h > 0.0f) ? h : 0.1f; }

    /** Radius of the base footprint. */
    float baseRadius() const { return m_baseRadius; }
    void  setBaseRadius(float r) { m_baseRadius = (r > 0.0f) ? r : 0.05f; }

    /** World position of the stand top (mount point). */
    QVector3D topPos() const { return QVector3D(m_originX, m_originY, m_height); }

    QColor color() const { return m_color; }
    void   setColor(const QColor &c) { m_color = c; }
    bool locked() const { return m_locked; }
    void setLocked(bool l) { m_locked = l; }
    quint32 layerId() const { return m_layerId; }
    void setLayerId(quint32 id) { m_layerId = id; }
    quint32 groupId() const { return m_groupId; }
    void setGroupId(quint32 id) { m_groupId = id; }

    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

private:
    quint32 m_id;
    QString m_name;
    float   m_originX    = 0.0f;
    float   m_originY    = 0.0f;
    float   m_height     = 2.4f;    ///< ~8 ft top
    float   m_baseRadius = 0.45f;
    QColor  m_color;
    bool    m_locked     = false;
    quint32 m_layerId    = 0;
    quint32 m_groupId    = 0;
};

/** @} */

#endif // STAND_H

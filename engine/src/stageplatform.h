/*
  Q Light Controller Plus
  stageplatform.h

  A StagePlatform represents a raised stage element (riser, deck section,
  thrust, etc.) that lifts fixtures and targets placed on it to a given
  height above the floor.

  Coordinate convention (same as Truss and the 2-D Monitor):
    X = stage right (+) / stage left (-)
    Y = upstage (+) / downstage (-)
    Z = height above stage floor (metres, always >= 0)

  The platform occupies the floor rectangle [origin.x, origin.x+width] x
  [origin.y, origin.y+depth] and raises everything on it by `height` metres.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STAGEPLATFORM_H
#define STAGEPLATFORM_H

#include <QObject>
#include <QColor>
#include <QString>
#include <climits>

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

class StagePlatform final : public QObject
{
    Q_OBJECT

public:
    static quint32 invalidId() { return UINT_MAX; }

    explicit StagePlatform(quint32 id, QObject *parent = nullptr);

    /********************************************************************
     * Identity
     ********************************************************************/
    quint32 id()   const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; }

    /********************************************************************
     * Geometry  (all values in metres)
     ********************************************************************/

    /** Top-left corner of the platform rectangle in the stage XY plane. */
    float originX() const { return m_originX; }
    void  setOriginX(float x) { m_originX = x; }

    float originY() const { return m_originY; }
    void  setOriginY(float y) { m_originY = y; }

    /** Extent in the X direction (stage-right). */
    float width() const { return m_width; }
    void  setWidth(float w) { m_width = (w > 0.0f) ? w : 0.1f; }

    /** Extent in the Y direction (upstage). */
    float depth() const { return m_depth; }
    void  setDepth(float d) { m_depth = (d > 0.0f) ? d : 0.1f; }

    /** Height of the platform surface above the stage floor. */
    float height() const { return m_height; }
    void  setHeight(float h) { m_height = (h >= 0.0f) ? h : 0.0f; }

    /** Return true if the point (x, y) in metres falls within this platform. */
    bool containsPoint(float x, float y) const;

    /********************************************************************
     * Appearance
     ********************************************************************/
    QColor color() const { return m_color; }
    void   setColor(const QColor &c) { m_color = c; }

    /********************************************************************
     * Load & Save
     ********************************************************************/
    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

private:
    quint32 m_id;
    QString m_name;
    float   m_originX = 0.0f;
    float   m_originY = 0.0f;
    float   m_width   = 2.0f;
    float   m_depth   = 2.0f;
    float   m_height  = 0.5f;
    QColor  m_color;
};

/** @} */

#endif // STAGEPLATFORM_H

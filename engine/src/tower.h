/*
  Q Light Controller Plus
  tower.h

  A Tower is a large vertical box-truss structure (e.g. 16" square x 8' high)
  carrying horizontal SHELVES at chosen heights. Each shelf is a mounting plane
  that hosts fixtures or props. Unlike a Truss the tower is a first-class
  placeable object with its own footprint and a list of shelf heights.

  Coordinate convention (same as the rest of the 2-D Monitor):
    X = stage right (+) / stage left (-)
    Y = upstage (+) / downstage (-)
    Z = height above stage floor (metres)

  The tower occupies [originX, originX+width] x [originY, originY+depth] on the
  floor and rises `height` metres. A shelf at height Z spans that footprint.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef TOWER_H
#define TOWER_H

#include <QObject>
#include <QVector3D>
#include <QColor>
#include <QString>
#include <QList>
#include <climits>

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

class Tower final : public QObject
{
    Q_OBJECT

public:
    static quint32 invalidId() { return UINT_MAX; }

    explicit Tower(quint32 id, QObject *parent = nullptr);

    quint32 id()   const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; }

    float originX() const { return m_originX; }
    void  setOriginX(float x) { m_originX = x; }
    float originY() const { return m_originY; }
    void  setOriginY(float y) { m_originY = y; }

    float width()  const { return m_width; }
    void  setWidth(float w) { m_width = (w > 0.0f) ? w : 0.05f; }
    float depth()  const { return m_depth; }
    void  setDepth(float d) { m_depth = (d > 0.0f) ? d : 0.05f; }
    float height() const { return m_height; }
    void  setHeight(float h) { m_height = (h > 0.0f) ? h : 0.1f; }

    /** Shelf heights above the floor (metres), sorted low → high. */
    QList<float> shelves() const { return m_shelves; }
    int   shelfCount() const { return m_shelves.size(); }
    float shelfHeight(int i) const { return (i >= 0 && i < m_shelves.size()) ? m_shelves.at(i) : 0.0f; }
    void  addShelf(float z);
    void  removeShelf(int i);
    void  clearShelves() { m_shelves.clear(); }

    /** World position of a point on shelf @p i at (u,v) metres into the footprint. */
    QVector3D shelfPos(int i, float u, float v) const
    {
        return QVector3D(m_originX + u, m_originY + v, shelfHeight(i));
    }
    /** Centre of shelf @p i. */
    QVector3D shelfCentre(int i) const { return shelfPos(i, m_width * 0.5f, m_depth * 0.5f); }

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
    quint32      m_id;
    QString      m_name;
    float        m_originX = 0.0f;
    float        m_originY = 0.0f;
    float        m_width   = 0.406f;   ///< 16"
    float        m_depth   = 0.406f;   ///< 16"
    float        m_height  = 2.438f;   ///< 8'
    QList<float> m_shelves;            ///< shelf heights (metres)
    QColor       m_color;
    bool         m_locked  = false;
    quint32      m_layerId = 0;
    quint32      m_groupId = 0;
};

/** @} */

#endif // TOWER_H

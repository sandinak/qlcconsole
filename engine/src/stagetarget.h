/*
  Q Light Controller Plus
  stagetarget.h

  A StageTarget represents a named point in 3-D stage space that lighting
  fixtures aim at.  It is stored in MonitorProperties alongside trusses and
  platforms and rendered as a crosshair marker on the 2-D floor plan.

  A QLCPalette of type PanTilt can optionally reference a StageTarget by ID
  to document "this palette points fixtures at this location."

  Coordinate convention (same as Truss/StagePlatform):
    X = stage right (+) / left (-)
    Y = upstage (+) / downstage (-)
    Z = height above stage floor (metres, >= 0; 0 = floor target)

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STAGETARGET_H
#define STAGETARGET_H

#include <QObject>
#include <QColor>
#include <QVector3D>
#include <QString>
#include <QMutex>
#include <climits>

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

class StageTarget final : public QObject
{
    Q_OBJECT

public:
    static quint32 invalidId() { return UINT_MAX; }

    explicit StageTarget(quint32 id, QObject *parent = nullptr);

    /********************************************************************
     * Identity
     ********************************************************************/
    quint32 id()   const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; }

    /********************************************************************
     * Position (metres)
     ********************************************************************/
    // m_positionMutex guards m_position: applyDesignJoystick() writes it on the
    // GUI thread (50 Hz) while Scene::write()'s Aim expansion reads it on the
    // MasterTimer thread. A QVector3D (3 floats) is not read/written atomically.
    QVector3D position() const { QMutexLocker l(&m_positionMutex); return m_position; }
    void setPosition(const QVector3D &p) { QMutexLocker l(&m_positionMutex); m_position = p; }

    float x() const { QMutexLocker l(&m_positionMutex); return m_position.x(); }
    float y() const { QMutexLocker l(&m_positionMutex); return m_position.y(); }
    float z() const { QMutexLocker l(&m_positionMutex); return m_position.z(); }

    void setX(float v) { QMutexLocker l(&m_positionMutex); m_position.setX(v); }
    void setY(float v) { QMutexLocker l(&m_positionMutex); m_position.setY(v); }
    void setZ(float v) { QMutexLocker l(&m_positionMutex); m_position.setZ(v); }

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
    quint32   m_id;
    QString   m_name;
    QVector3D m_position;
    mutable QMutex m_positionMutex;   ///< guards m_position (GUI joystick vs DMX aim read)
    QColor    m_color;
};

/** @} */

#endif // STAGETARGET_H

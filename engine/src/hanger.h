/*
  Q Light Controller Plus
  hanger.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef HANGER_H
#define HANGER_H

#include <QObject>
#include <QString>
#include <QVector3D>
#include <climits>

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

/**
 * A hanger: a bracket clamped to the side of a truss or a tower, an arm
 * reaching out from it, and a chain (or cable) dropping from the arm's end to
 * a hook. One fixture hangs on the hook -- a lantern off the side of a tower.
 *
 * It is rigging like a bar or a boom: put on a structure, it follows that
 * structure, and a fixture is mounted on it the way it is mounted on anything
 * else (FixtureRigProps::hangerId).
 *
 * Where it is: @c along the parent (metres from the truss's origin -- for a
 * tower truss, the height; for a Tower structure, the height above the floor),
 * reaching toward @c angle in plan (0 downstage, 90 stage left, 180 upstage,
 * 270 stage right), @c arm metres out from the parent's FACE, then @c drop
 * metres down to the hook. The fastening point, the arm's end and the hook are
 * derived (MonitorProperties::recomputeHangers()), so it follows its parent.
 */
class Hanger final : public QObject
{
    Q_OBJECT

public:
    static quint32 invalidId() { return UINT_MAX; }

    /** What it is clamped to. */
    enum ParentKind { OnTruss = 0, OnTower = 1 };

    Hanger(quint32 id, QObject *parent = nullptr);

    quint32 id() const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    ParentKind parentKind() const { return m_parentKind; }
    quint32 parentId() const { return m_parentId; }
    void setParent(ParentKind kind, quint32 id) { m_parentKind = kind; m_parentId = id; }

    float along() const { return m_along; }
    void setAlong(float m) { m_along = m; }
    float angle() const { return m_angle; }
    void setAngle(float deg) { m_angle = deg; }
    float arm() const { return m_arm; }
    void setArm(float m) { m_arm = qMax(0.0f, m); }
    float drop() const { return m_drop; }
    void setDrop(float m) { m_drop = qMax(0.0f, m); }

    /** Unit vector the arm points along, in plan (+X stage left, +Y downstage). */
    QVector3D direction() const;

    /** Derived from the parent (MonitorProperties::recomputeHangers()). */
    QVector3D fastened() const { return m_fastened; }
    QVector3D armEnd() const { return m_fastened + direction() * m_arm; }
    QVector3D hook() const { return armEnd() - QVector3D(0.0f, 0.0f, m_drop); }
    void setFastened(const QVector3D &p) { m_fastened = p; }

    quint32 layerId() const { return m_layerId; }
    void setLayerId(quint32 id) { m_layerId = id; }
    bool locked() const { return m_locked; }
    void setLocked(bool l) { m_locked = l; }

    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

private:
    quint32 m_id;
    QString m_name;
    ParentKind m_parentKind = OnTruss;
    quint32 m_parentId = UINT_MAX;
    float m_along = 2.0f;
    float m_angle = 0.0f;
    float m_arm = 0.3f;
    float m_drop = 0.3f;
    QVector3D m_fastened;
    quint32 m_layerId = 0;
    bool m_locked = false;
};

/** @} */

#endif

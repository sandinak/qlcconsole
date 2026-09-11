/*
  Q Light Controller Plus
  stagetarget.h

  A StageTarget represents a named point in 3-D stage space that lighting
  fixtures aim at.  It is stored in MonitorProperties alongside trusses and
  platforms and rendered as a crosshair marker on the 2-D floor plan.

  A QLCPalette of type PanTilt can optionally reference a StageTarget by ID
  to document "this palette points fixtures at this location."

  Coordinate convention (same as Truss/StagePlatform):
    X = stage left (+) / stage right (-)
    Y = downstage (+) / upstage (-)
    (These match how the app BEHAVES -- see barFaceVector() in
    monitorproperties.cpp and any real show: in stage-structures-demo.qxw the
    "SR Tower" sits at X=0.21 with the "SL Tower" at X=11.61, and the upstage
    platforms are at Y=1.53 against the downstage ones at Y=3.97. The plot
    draws +X rightward and +Y downward, which is the standard ground plan:
    audience at the bottom of the page, upstage at the top, stage right on the
    viewer's left. These comments used to say the opposite on both axes.)
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
#include <QSizeF>
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
    /********************************************************************
     * What this target IS
     ********************************************************************/
public:
    /** What a target stands for.
     *
     *  A kind is only worth having if it changes one of two things: the height
     *  light should be aimed at, or the shape drawn at the point. Anything else
     *  is a label, and a label in a file format is a liability.
     *
     *  Persisted as a STRING, deliberately. Storing an ordinal means a kind can
     *  never be reordered or removed without silently re-meaning every saved
     *  file -- exactly the trap StagePlatform::TopMaterial fell into, where
     *  value 2 is permanently burned because Recessed was withdrawn. An unknown
     *  name read from a newer file degrades to None rather than becoming
     *  whatever now sits at that index. */
    enum class Kind
    {
        None,        ///< a bare mark: aim at its own Z
        Person,      ///< a standing subject
        DrumKit,     ///< a kit: lower, and wide enough that a point lies
        Structure,   ///< bound to a truss/platform/tower -- follows it
        Area         ///< a region of deck, not a point
    };

    static QString kindToString(Kind k);
    static Kind    stringToKind(const QString &s, Kind fallback = Kind::None);

    Kind kind() const { return m_kind; }
    void setKind(Kind k) { m_kind = k; }

    /** Height above whatever the target is standing on that light should be
     *  aimed at, for this kind. 0 means "aim at the target's own Z".
     *
     *  A follow-spot wants a chest, not a pair of feet -- and a drum kit's
     *  chest is not a vocalist's. One global subject height applied to
     *  everything is what made every target the same height whatever was
     *  actually standing there. */
    static float defaultAimHeight(Kind k);

    /** Per-target override of the above, in metres. Negative = use the kind's
     *  default. One override removes the pressure to invent a new Kind for
     *  every one-off, which is how these lists go wrong. */
    float aimHeightOverride() const { return m_aimHeightOverride; }
    void  setAimHeightOverride(float m) { m_aimHeightOverride = m; }
    bool  hasAimHeightOverride() const { return m_aimHeightOverride >= 0.0f; }

    /** The height to aim at above the target's footing, override or default. */
    float effectiveAimHeight() const
    { return hasAimHeightOverride() ? m_aimHeightOverride : defaultAimHeight(m_kind); }

    /********************************************************************
     * Structure binding (Kind::Structure)
     ********************************************************************/
public:
    /** The structure this target follows: "platform", "truss" or "tower", and
     *  its id. A bound target is a RELATIONSHIP, not a coordinate -- move or
     *  restack the step and the target goes with it, which is what actually
     *  happens between load-ins. Empty type = not bound.
     *
     *  Typed by string for the same reason Kind is. */
    QString boundType() const { return m_boundType; }
    quint32 boundId()   const { return m_boundId; }
    void setBinding(const QString &type, quint32 id) { m_boundType = type; m_boundId = id; }
    void clearBinding() { m_boundType.clear(); m_boundId = invalidId(); }
    bool isBound() const { return m_boundType.isEmpty() == false && m_boundId != invalidId(); }

    /********************************************************************
     * Footprint (Kind::Area, and anything else with real extent)
     ********************************************************************/
public:
    /** Width (X) and depth (Y) in metres. A drum kit is not a point, and
     *  neither is a downstage-centre area: knowing a target is 1.6 m across is
     *  what lets you see whether an 8-degree beam actually covers it, which is
     *  the question this view is opened to answer. Zero = a point. */
    QSizeF footprint() const { return m_footprint; }
    void   setFootprint(const QSizeF &s) { m_footprint = s; }

    QColor color() const { return m_color; }
    void   setColor(const QColor &c) { m_color = c; }

    /** When locked, the 2-D Monitor will not let the user drag this target. */
    bool locked() const { return m_locked; }
    void setLocked(bool l) { m_locked = l; }

    /** Organizational layer this item belongs to on the 2D map (0 = Default). */
    quint32 layerId() const { return m_layerId; }
    void setLayerId(quint32 id) { m_layerId = id; }

    /** Group this item belongs to on the 2D map (0 = ungrouped). Grouped items
     *  select and move together. */
    quint32 groupId() const { return m_groupId; }
    void setGroupId(quint32 id) { m_groupId = id; }

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
    Kind      m_kind = Kind::None;
    float     m_aimHeightOverride = -1.0f;   ///< < 0 = use the kind's default
    QString   m_boundType;                   ///< "platform" | "truss" | "tower"
    quint32   m_boundId = UINT_MAX;
    QSizeF    m_footprint;                   ///< metres; empty = a point
    bool      m_locked = false;
    quint32   m_layerId = 0;   ///< 2D-map organizational layer (0 = Default)
    quint32   m_groupId = 0;   ///< 2D-map group (0 = ungrouped)
};

/** @} */

#endif // STAGETARGET_H

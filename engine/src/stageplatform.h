/*
  Q Light Controller Plus
  stageplatform.h

  A StagePlatform represents a raised stage element (riser, deck section,
  thrust, etc.) that lifts fixtures and targets placed on it to a given
  height above the floor.

  Coordinate convention (same as Truss and the 2-D Monitor):
    X = stage left (+) / stage right (-)
    Y = downstage (+) / upstage (-)
    (These match how the app BEHAVES -- see barFaceVector() in
    monitorproperties.cpp and any real show: in stage-structures-demo.qxw the
    "SR Tower" sits at X=0.21 with the "SL Tower" at X=11.61, and the upstage
    platforms are at Y=1.53 against the downstage ones at Y=3.97. The plot
    draws +X rightward and +Y downward, which is the standard ground plan:
    audience at the bottom of the page, upstage at the top, stage right on the
    viewer's left. These comments used to say the opposite on both axes.)
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
#include <QList>
#include <QPointF>
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

    /** What the DECK is made of, which decides whether you can see -- and
     *  light -- through it.
     *
     *  Solid: an ordinary deck. Opaque.
     *  Clear: a glass/perspex top. Fixtures inside the step show through it and
     *      their light broadcasts up, which is the whole point of rigging them
     *      in there.
     *  Open: no top at all (a frame). Fully see-through. */
    enum TopMaterial { SolidTop = 0, ClearTop = 1, OpenTop = 2 };
    TopMaterial topMaterial() const { return m_topMaterial; }
    void setTopMaterial(TopMaterial m) { m_topMaterial = m; }
    void  setHeight(float h) { m_height = (h >= 0.0f) ? h : 0.0f; }

    /** Which way it faces, degrees about its centre (0: square to the stage,
     *  the front downstage; turning +X toward +Y, as everything else on the
     *  plot does). Its own geometry -- the origin, width and depth, blocks(),
     *  frontEdgeAt() -- is LOCAL: the platform as it would be unturned, in
     *  the same plan coordinates. toWorld()/toLocal() go between the two. */
    float rotation() const { return m_rotation; }
    void  setRotation(float deg);
    bool  isRotated() const { return m_rotation != 0.0f; }
    /** The middle of its footprint (turning does not move it). */
    QPointF centre() const;
    QPointF toWorld(const QPointF &local) const;
    QPointF toLocal(const QPointF &world) const;

    /** Return true if the point (x, y) in metres falls within this platform
     *  (inside its rounded front, when it has one). World coordinates. */
    bool containsPoint(float x, float y) const;

    /** A rounded front: how far the middle of the downstage edge reaches past
     *  its corners (metres; 0 is square). The front is an arc across the full
     *  width; depth() is still the depth at the middle, so the outside box --
     *  dragging, overlap, the elevations -- does not change; the corners are
     *  cut back. A box only (steps ignore it). Up to half the width (a half
     *  circle) and no more than the depth. */
    float frontCurve() const;
    void  setFrontCurve(float m) { m_frontCurve = (m > 0.0f) ? m : 0.0f; }
    bool  hasRoundedFront() const { return frontCurve() > 0.0f; }

    /** Where the downstage edge is at @p x (metres): originY() + depth() for
     *  a square front, nearer the corners on a rounded one. */
    float frontEdgeAt(float x) const;

    /** Its outline on the floor, corner to corner round the front --
     *  @p segments panels along a rounded front. Metres, world coordinates
     *  (turned); localFootprint() is the same outline unturned. */
    QList<QPointF> footprint(int segments = 24) const;
    QList<QPointF> localFootprint(int segments = 24) const;

    /********************************************************************
     * Shape: a box, or a flight of steps
     ********************************************************************/

    /** A platform is a box: one deck at height(). Steps are a flight of
     *  @c stepCount() treads, each @c stepRise() above the last and
     *  @c stepRun() deep, the top one at height(). Steps keep the box's
     *  footprint and height -- width() x depth() x height() is still the
     *  outside of the flight -- so everything that only needs the outline
     *  (dragging, overlap, the plan view) is unchanged. */
    enum Shape { BoxShape = 0, StepsShape = 1 };
    Shape shape() const { return m_shape; }
    bool isSteps() const { return m_shape == StepsShape; }

    /** Which way you walk going UP. Upstage (the default): the bottom step is
     *  at the downstage edge, as steps from the house onto the stage are.
     *  (+X is stage left, +Y downstage; originY is the UPSTAGE edge.) */
    enum Climb { ClimbUpstage = 0, ClimbDownstage = 1, ClimbStageLeft = 2, ClimbStageRight = 3 };
    Climb stepsClimb() const { return m_climb; }
    int   stepCount() const { return m_stepCount; }
    float stepRise() const { return m_stepRise; }
    float stepRun() const { return m_stepRun; }
    /** Across the treads: width() climbing up/downstage, depth() across stage. */
    float stepWidth() const;

    /** Make it a flight of @p count steps, @p rise high and @p run deep each,
     *  @p width across, climbing toward @p climb. Sets width/depth/height to
     *  the flight's outside. */
    void setSteps(int count, float rise, float run, float width, Climb climb = ClimbUpstage);
    /** Back to a plain box, keeping the outside dimensions. */
    void setBoxShape() { m_shape = BoxShape; }

    /** Height of the walking surface above the platform's own base at world (x, y)
     *  -- height() for a box, the tread there for steps (the nearest tread
     *  for a point off the platform; callers decide whether it is on it). */
    float topAt(float x, float y) const;

    /** The solid as boxes standing on the base: one for a box, one per step
     *  (each from the base up to its own tread). Metres, LOCAL plan
     *  coordinates; corners() gives a block's four corners in the world. */
    struct Block { float x0, y0, x1, y1, height; };
    QList<Block> blocks() const;
    /** A block's corners on the floor, turned: x0y0, x1y0, x1y1, x0y1. */
    QList<QPointF> corners(const Block &b) const;

    /********************************************************************
     * Appearance
     ********************************************************************/
    QColor color() const { return m_color; }
    void   setColor(const QColor &c) { m_color = c; }

    /** When locked, the 2-D Monitor will not let the user drag this platform. */
    bool locked() const { return m_locked; }
    void setLocked(bool l) { m_locked = l; }

    /** Solid = occupies stage space: two solid platforms can't be dragged to
     *  overlap (blocked at contact). Platforms default solid. */
    bool solid() const { return m_solid; }
    void setSolid(bool s) { m_solid = s; }

    /** Stackable = may sit ON TOP of solid platforms (a step / riser on a deck),
     *  so it's exempt from the no-overlap rule. Off by default. */
    bool stackable() const { return m_stackable; }
    void setStackable(bool s) { m_stackable = s; }

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
    quint32 m_id;
    QString m_name;
    float   m_originX = 0.0f;
    float   m_originY = 0.0f;
    float   m_width   = 2.0f;
    float   m_depth   = 2.0f;
    float   m_height  = 0.5f;
    float   m_frontCurve = 0.0f;
    float   m_rotation = 0.0f;
    TopMaterial m_topMaterial = SolidTop;
    Shape   m_shape = BoxShape;
    Climb   m_climb = ClimbUpstage;
    int     m_stepCount = 2;
    float   m_stepRise = 0.2032f;   ///< 8"
    float   m_stepRun  = 0.2032f;   ///< 8"
    QColor  m_color;
    bool    m_locked  = false;
    bool    m_solid     = true;    ///< occupies space: no-overlap with other solids
    bool    m_stackable = false;   ///< may sit on top of solids (step/riser)
    quint32 m_layerId = 0;   ///< 2D-map organizational layer (0 = Default)
    quint32 m_groupId = 0;   ///< 2D-map group (0 = ungrouped)
};

/** @} */

#endif // STAGEPLATFORM_H

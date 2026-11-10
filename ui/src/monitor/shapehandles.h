/*
  Q Light Controller Plus
  shapehandles.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef SHAPEHANDLES_H
#define SHAPEHANDLES_H

#include <QGraphicsItem>
#include <QBrush>
#include <QPen>
#include <QPolygonF>
#include <QPointF>
#include <QString>
#include <QVector>

#include "../qlcui_export.h"

class MonitorProperties;
class QGraphicsSimpleTextItem;

/** @addtogroup ui_monitor
 * @{
 */

/**
 * A stage feature's footprint as the plot's stretch and rotate handles see
 * it: a box about a middle, turned by an angle, so a platform, a truss and
 * a piece of scenery are handled the same way. Plan metres, world frame:
 * u = (cos angle, sin angle) is its length, v = (-sin, cos) its width.
 *
 * Pure geometry -- no view, no undo -- so it is tested on its own and the
 * plot (MonitorGraphicsView) only maps it to the screen.
 */
struct QLC_UI_EXPORT StageShape
{
    enum Handle
    {
        NoHandle = -1,
        EndPlusU = 0,   ///< the far end of its length
        EndMinusU,      ///< the near end of its length
        SidePlusV,      ///< one side of its width
        SideMinusV,     ///< the other
        CornerPP,       ///< corners: both at once (a box only)
        CornerPM,
        CornerMP,
        CornerMM,
        RotateKnob      ///< the knob that turns it
    };

    QString kind;               ///< "platform", "truss" or "scenery"
    quint32 id = 0;
    QPointF centre;             ///< the middle of its footprint
    double angle = 0.0;         ///< degrees, +X toward +Y as the plot turns
    double length = 0.0;        ///< along u
    double width = 0.0;         ///< along v
    bool stretchU = false;      ///< its length can be stretched
    bool stretchV = false;      ///< its width can be stretched
    bool canRotate = false;
    /** A line on the plan (a truss, a pipe, a wall): ⌥ on an end swings it
     *  about the other end (swung()). */
    bool canSwing = false;
    double minLength = 0.1;     ///< metres: the shortest it can be made

    bool isValid() const { return !kind.isEmpty(); }
    QPointF u() const;
    QPointF v() const;
    QPointF toWorld(double lu, double lv) const;
    /** Its footprint's four corners, world. */
    QPolygonF outline() const;
    /** Where handle @p h is (world); the knob is the -v side's middle, and
     *  @p knobGap metres beyond it. */
    QPointF handlePoint(Handle h, double knobGap = 0.0) const;
    /** The handles it has, in the order they are drawn. */
    QVector<Handle> handles() const;

    /** The shape stretched by dragging @p h to @p pointer (world): the side
     *  opposite stays where it is. @p snapStep (metres, 0 = none) rounds the
     *  new length or width to a whole number of steps. */
    StageShape stretched(Handle h, const QPointF &pointer, double snapStep = 0.0) const;
    /** A line's end @p h (EndPlusU or EndMinusU) put at @p endPoint (world):
     *  the other end stays where it is, so its length and the way it faces
     *  both follow. Never shorter than minLength. */
    StageShape swung(Handle h, const QPointF &endPoint) const;

    /** What swung() makes of a pointer at @p pointer, snapped: onto one of
     *  @p otherEnds within @p magnet metres first (@p caught says so), else
     *  with @p constrain its angle in 15° steps and its length in
     *  @p snapStep metres, else the end on the nearest @p snapStep grid
     *  point (0 = free). */
    StageShape swungSnapped(Handle h, const QPointF &pointer, const QVector<QPointF> &otherEnds,
                            double magnet, double snapStep, bool constrain,
                            bool *caught = nullptr) const;
    /** Where end @p h is (world). */
    QPointF endPoint(Handle h) const { return handlePoint(h); }

    /** The same shape turned to @p angleDeg about its middle. */
    StageShape turnedTo(double angleDeg) const;

    /** The degrees of the line from its middle to @p pointer. */
    double angleTo(const QPointF &pointer) const;

    /** Read @p kind / @p id off the model: invalid when it has no footprint
     *  the handles can change (a vertical truss, a bar, one on a stand). */
    static StageShape of(MonitorProperties *props, const QString &kind, quint32 id);
    /** Write the footprint back into the model (place, size, turn). What
     *  rides on it is the caller's: a truss's fixtures are kept where they
     *  are along it by apply() itself when its near end moves. */
    bool apply(MonitorProperties *props) const;

    /** An angle in (-180, 180]. */
    static double normalised(double deg);
};

/**
 * The handles of the stage feature selected on its own on the plot: its
 * footprint outlined, a square at each end and side it stretches by, the
 * corners of a box, a round knob to turn it, and while a handle is dragged
 * the shape it will take (dashed) and a readout of its size or angle.
 * Drawn in scene coordinates; clicks are the plot's to work out
 * (MonitorGraphicsView::shapePress()).
 */
class QLC_UI_EXPORT ShapeHandles final : public QGraphicsItem
{
public:
    ShapeHandles();

    /** The shape and the polygon it is drawn as (scene), each handle's
     *  scene point, and their size in scene units. */
    void setShape(const StageShape &shape, const QPolygonF &outline,
                  const QVector<QPair<StageShape::Handle, QPointF> > &points,
                  const QPointF &pivot, qreal radius);
    const StageShape &stageShape() const { return m_shape; }
    /** The handle at @p scenePos (within its size and a little), or NoHandle. */
    StageShape::Handle handleAt(const QPointF &scenePos) const;
    QPointF handlePos(StageShape::Handle h) const;
    int count() const { return m_points.size(); }

    /** Selected but held still by a lock: its outline dashed in the lock
     *  colour, no handles, and @p reason (what holds it, where to lift it)
     *  at @p at. */
    void setHeld(const StageShape &shape, const QPolygonF &outline, const QString &reason,
                 const QPointF &at, qreal radius);
    QString heldReason() const { return m_held; }

    /** While dragging: the shape it will become (scene polygon) and the
     *  readout by the pointer. An empty polygon ends it. */
    void setPreview(const QPolygonF &ghost, const QString &readout, const QPointF &at);
    QString readout() const;
    bool previewing() const { return !m_ghost.isEmpty(); }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    StageShape m_shape;
    QPolygonF m_outline;
    QVector<QPair<StageShape::Handle, QPointF> > m_points;
    QPointF m_pivot;
    qreal m_radius = 5.0;
    QPolygonF m_ghost;
    QGraphicsSimpleTextItem *m_label = nullptr;
    QString m_held;             ///< non-empty: locked, outline only
    QRectF m_bounds;
    // Made when the shape or its size changes, never in paint().
    QPen m_edgePen, m_ghostPen, m_stemPen, m_handlePen, m_arcPen, m_heldPen;
    QBrush m_accentBrush, m_ghostBrush, m_handleBrush;
    QRectF m_arc;               ///< the knob's turning arrow, about its middle
    void recomputeBounds();
    void recomputePens();
};

/** @} */

#endif

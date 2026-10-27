/*
  Q Light Controller Plus
  targetitem.h

  Interactive 2-D representation of a StageTarget in the Monitor canvas.
  Renders as a crosshair/bullseye with the target name and Z height as a label.
  Movable when the layout is not locked.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef TARGETITEM_H
#define TARGETITEM_H

#include <QGraphicsItem>
#include <QPainterPath>
#include <QPolygonF>
#include <QVector>
#include <QStringList>
#include <QObject>
#include <functional>

class QGraphicsTextItem;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneContextMenuEvent;
class StageTarget;
class StageMarkLabel;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

/**
 * Crosshair/bullseye marker for a StageTarget on the 2-D Monitor canvas.
 * Radius is fixed in pixels (scale-independent so it stays readable at any
 * zoom level).
 */
class TargetItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    /**
     * @param target  Engine StageTarget this item represents (not owned).
     * @param pxX     Scene-pixel X of the target floor projection.
     * @param pxY     Scene-pixel Y of the target floor projection.
     */
    TargetItem(StageTarget *target, Doc *doc, float pxX, float pxY,
               QGraphicsItem *parent = nullptr);

    quint32      targetId() const;
    StageTarget *target()   const { return m_target; }

    /** Move the item to the given scene-pixel position. */
    void setScenePos(float pxX, float pxY);

    /** Where it is against the stage marks ("SL 2.4 · US 3.1 m"), in a small
     *  label under it: @p text gives the words for the item's scene position,
     *  and is asked again whenever the item moves -- while it is dragged, or
     *  nudged in Focus mode. An empty function takes the label away. */
    void setPositionReadout(std::function<QString(const QPointF &scenePos)> text);
    /** Ask the readout again where it is (a deck under it, or the marks,
     *  moved). */
    void refreshPositionReadout();
    /** The readout as it stands; empty when there is none. */
    QString positionReadout() const;
    /** Is the readout on screen? A line target's shows only while selected. */
    bool readoutShown() const;
    /** Push the readout down @p dy screen px, under another it would cover. */
    void setReadoutStackOffset(qreal dy);
    qreal readoutStackOffset() const;
    /** The readout's box in screen px around its anchor, unstacked. */
    QRectF readoutScreenRect() const;
    /** Where the readout hangs from, in scene coordinates. */
    QPointF readoutAnchorScene() const;

    /** Enable or disable movement (global layout lock). */
    void setMovable(bool movable);

    /** Mark this target as aimed by the scene currently in focus.
     *
     *  A target exists from the moment it is created in the studio, so it is
     *  always drawn. Being AIMED is a different and temporary fact -- this
     *  scene, right now, points lights at it -- and it earns the glow and the
     *  light-path lines. Drawing the two states identically left no way to see
     *  which targets the scene on screen actually uses. */
    void setAimed(bool aimed);
    bool isAimed() const { return m_aimed; }

    /** How many of the heads aimed here cannot reach it, of how many, and
     *  which. A target a scene points lights at that some of them cannot get
     *  to is the thing to fix before the show, not something to discover when
     *  a head sticks at its limit -- so it says so, on the target. 0 clears. */
    void setReachShortfall(int unreachable, int aimed, const QStringList &names);
    int unreachableCount() const { return m_unreachable; }

    /** Draw what the target STANDS FOR, not only where it is.
     *
     *  A Person is a figure: from above, a pair of shoulders and a head sized
     *  by @p pxPerMetre; in an elevation, a silhouette from @p feet up to
     *  @p crown, both in item coordinates. The footing is what the person
     *  stands on -- the deck under them -- which need not be the target's own
     *  Z. @p sideOn narrows the body to its depth. Every other kind keeps the
     *  plain mark, and so does a Person until this is called. */
    void setFigure(const QPointF &feet, const QPointF &crown, double pxPerMetre,
                   bool sideOn);
    /** The figure's outline in item coordinates; empty when there is none. */
    QPainterPath figurePath() const;

    /** The ground this target covers, in scene pixels, centred on the mark:
     *  width x depth from above, or a strip of width (height 0) in an
     *  elevation. An Area or a drum kit is not a point -- whether a beam
     *  covers it is the question -- so it is drawn at its real size. An empty
     *  size draws nothing. */
    void setFootprintPx(const QSizeF &size);
    QRectF footprintRect() const;

    /** A line target (LINE_TARGET_DESIGN.md), in item coordinates -- the
     *  item sits on the line's centre: the sampled curve, how far along
     *  (0..1) each sample is, and the handles (the ends first and last).
     *  Empty draws the plain mark. */
    void setLineGeometry(const QPolygonF &curve, const QVector<double> &fractions,
                         const QVector<QPointF> &handles);
    bool isLine() const { return m_curve.size() >= 2; }
    int lineHandleCount() const { return m_handles.size(); }
    QPointF lineHandle(int index) const { return m_handles.value(index); }
    /** The handle under @p local (item coordinates), or -1. */
    int lineHandleAt(const QPointF &local) const;
    /** True if @p local is on the curve; @p fraction gets how far along. */
    bool lineHitAt(const QPointF &local, double *fraction = nullptr) const;
    /** True if @p local is on the centre mark. */
    bool onCentreMark(const QPointF &local) const;

    /** What the target IS on the stage, in item coordinates: the mark's
     *  ring, an area's footprint, a line's curve, a person's figure -- not
     *  its name, readout, halo or the room boundingRect() keeps for them.
     *  The rulers band this (MonitorGraphicsView::itemGeometryRect()). */
    QRectF geometryRect() const;

    // QGraphicsItem interface
    QRectF   boundingRect() const override;
    /** A line is picked on its curve, handles and mark -- not on the empty
     *  stage its bounding box covers. */
    QPainterPath shape() const override;
    void     paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

public:
    /** Delete this target after naming the scenes that aim at it. */
    void confirmAndDelete();

private:

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

signals:
    void itemDropped(TargetItem *item);
    /** A line's handle is being dragged to @p scenePos. */
    void lineHandleMoved(TargetItem *item, int index, QPointF scenePos,
                         Qt::KeyboardModifiers modifiers);
    /** The drag of a line's handle ended. */
    void lineHandleReleased(TargetItem *item, int index);
    /** Add a point to the line @p fraction of the way along. */
    void lineInsertRequested(TargetItem *item, double fraction);
    /** Remove the line's point @p index (a mid point). */
    void lineRemoveRequested(TargetItem *item, int index);

private:
    static constexpr float kRadius = 14.0f;   // crosshair radius in scene px

    StageTarget       *m_target;
    Doc               *m_doc;
    QGraphicsTextItem *m_label;
    void refreshReadoutVisibility();
    StageMarkLabel    *m_readout = nullptr;
    std::function<QString(const QPointF &)> m_readoutText;
    bool               m_aimed = false;
    int                m_unreachable = 0;
    int                m_aimedHeads = 0;

    QSizeF             m_footprintPx;
    QPointF            m_feet;
    QPointF            m_crown;
    double             m_pxPerMetre = 0.0;   ///< 0 = no figure set
    bool               m_sideOn = false;

    QPolygonF          m_curve;            ///< line: the curve, item coords
    QVector<double>    m_curveFractions;   ///< parallel to m_curve
    QVector<QPointF>   m_handles;          ///< line: ends first and last
    int                m_dragHandle = -1;  ///< the handle being dragged
};

/** @} */

#endif // TARGETITEM_H

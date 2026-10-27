/*
  Q Light Controller Plus
  stagepieceitem.h

  One stage piece on the plot -- the centre line / DSC mark, the curtain
  line, the number line or the apron (MonitorProperties::StagePiece) -- as
  an object: picked by a click (never by a rubber band, which would catch
  the lines running across the whole stage), outlined when selected, and
  dragged up or down stage where it can be (a line, or the apron's front
  edge). It draws only its selection: the lines, ticks and labels are the
  view's (MonitorGraphicsView::updateStageMarks()), rebuilt as it moves.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STAGEPIECEITEM_H
#define STAGEPIECEITEM_H

#include <QGraphicsObject>
#include <QPainterPath>

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class StagePieceItem final : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit StagePieceItem(int piece, QGraphicsItem *parent = nullptr);

    /** Which MonitorProperties::StagePiece it is. */
    int piece() const { return m_piece; }

    /** Where it is picked, in scene pixels (the item stays at 0,0): a band
     *  along a line, or the apron's outline. */
    void setHitShape(const QPainterPath &path);
    /** The part a drag takes hold of: the whole band of a line, the band
     *  along the apron's front. Empty: it is not dragged at all. */
    void setDragShape(const QPainterPath &path) { m_dragShape = path; }
    /** Dragged, or held still (its lock, its layer's, the plot's). */
    void setMovable(bool on);
    bool isMovable() const { return m_movable; }

    /** Select or clear it from code (a click, the Layers list). A rubber
     *  band can't select it. */
    void selectNow(bool on);

    QRectF boundingRect() const override;
    /** What the item IS on the stage, in item coordinates: its own outline,
     *  without the padding, halos and labels boundingRect() adds for painting.
     *  The rulers band this (MonitorGraphicsView::itemGeometryRect()). */
    QRectF geometryRect() const { return m_hit.boundingRect(); }
    QPainterPath shape() const override { return m_hit; }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:
    void dragStarted(int piece, const QPointF &scenePos);
    /** The pointer during a drag, in scene pixels. */
    void dragMoved(int piece, const QPointF &scenePos);
    void dragFinished(int piece, bool moved);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    int m_piece;
    QPainterPath m_hit;
    QPainterPath m_dragShape;
    bool m_movable = false;
    bool m_selectArmed = false;
    bool m_dragging = false;
    bool m_moved = false;
    QPointF m_pressPos;
};

/** @} */

#endif

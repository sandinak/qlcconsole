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
#include <QObject>

class QGraphicsTextItem;
class QGraphicsSceneMouseEvent;
class StageTarget;
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

    /** Enable or disable movement (global layout lock). */
    void setMovable(bool movable);

    // QGraphicsItem interface
    QRectF   boundingRect() const override;
    void     paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

signals:
    void itemDropped(TargetItem *item);

private:
    static constexpr float kRadius = 14.0f;   // crosshair radius in scene px

    StageTarget       *m_target;
    Doc               *m_doc;
    QGraphicsTextItem *m_label;
};

/** @} */

#endif // TARGETITEM_H

/*
  Q Light Controller Plus
  standitem.h

  Interactive 2-D representation of a Stand (support base) in the Monitor.
  Top view: a base disc with a centre post dot. Elevation: a vertical post from
  the floor to the stand top with a base foot. Movable when unlocked.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STANDITEM_H
#define STANDITEM_H

#include <QGraphicsItem>
#include <QObject>

class QGraphicsTextItem;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneContextMenuEvent;
class Stand;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class StandItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    /**
     * @param stand      Engine Stand this item represents (not owned).
     * @param pxX,pxY    Top: base CENTRE. Elevation: base BOTTOM.
     * @param pxBaseR    Base radius in scene pixels.
     * @param elevation  True in Front/Side views.
     * @param pxHeight   Elevation only: stand height in scene pixels.
     */
    StandItem(Stand *stand, Doc *doc,
              float pxX, float pxY, float pxBaseR,
              bool elevation = false, float pxHeight = 0.0f,
              QGraphicsItem *parent = nullptr);

    quint32 standId() const;
    Stand  *stand()   const { return m_stand; }

    void setMovable(bool movable);
    void showLabel(bool visible);

    QRectF boundingRect() const override;
    void   paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

signals:
    void itemDropped(StandItem *item);

private:
    Stand             *m_stand;
    Doc               *m_doc;
    float              m_pxBaseR;
    bool               m_elevation;
    float              m_pxHeight;
    QGraphicsTextItem *m_label;
    bool               m_dragMoved = false;
};

/** @} */

#endif // STANDITEM_H

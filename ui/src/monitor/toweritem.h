/*
  Q Light Controller Plus
  toweritem.h

  Interactive 2-D representation of a Tower (box-truss + shelves) in the Monitor.
  Top view: the footprint square with a shelf-count badge. Elevation: the box
  outline with a horizontal line at each shelf height. Movable when unlocked.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef TOWERITEM_H
#define TOWERITEM_H

#include <QGraphicsItem>
#include <QObject>

class QGraphicsTextItem;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneContextMenuEvent;
class Tower;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class TowerItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    /**
     * @param tower      Engine Tower this item represents (not owned).
     * @param pxX,pxY    Top: footprint top-left. Elevation: base-left.
     * @param pxW,pxH    Top: footprint W×D. Elevation: front width × total height.
     * @param elevation  True in Front/Side views.
     */
    TowerItem(Tower *tower, Doc *doc,
              float pxX, float pxY, float pxW, float pxH,
              bool elevation = false, QGraphicsItem *parent = nullptr);

    quint32 towerId() const;
    Tower  *tower()   const { return m_tower; }

    void setMovable(bool movable);
    void showLabel(bool visible);

    QRectF boundingRect() const override;
    void   paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

signals:
    void itemDropped(TowerItem *item);

private:
    Tower             *m_tower;
    Doc               *m_doc;
    float              m_pxW;
    float              m_pxH;
    bool               m_elevation;
    QGraphicsTextItem *m_label;
};

/** @} */

#endif // TOWERITEM_H

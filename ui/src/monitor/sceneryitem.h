/*
  Q Light Controller Plus
  sceneryitem.h

  The 2-D plot item for a piece of Scenery (a cyclorama, a drape...): its line
  across the floor in Top, the sheet itself in the elevations.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef SCENERYITEM_H
#define SCENERYITEM_H

#include <QGraphicsItem>
#include <QPolygonF>
#include <QObject>

class QGraphicsTextItem;
class QGraphicsSceneMouseEvent;
class Scenery;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class SceneryItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    /**
     * @param scenery   engine Scenery this item represents (not owned)
     * @param anchor    scene position of the middle of its bottom edge
     * @param parts     Top: its floor line (a wall: its footprint, closed);
     *                  elevation: the sheet's outline -- one per stretch it
     *                  hangs in (an open curtain has two), in SCENE coordinates
     * @param elevation true in the Front/Side views (a filled sheet)
     */
    SceneryItem(Scenery *scenery, Doc *doc, const QPointF &anchor,
                const QList<QPolygonF> &parts, bool elevation,
                QGraphicsItem *parent = nullptr);

    quint32  sceneryId() const;
    Scenery *scenery() const { return m_scenery; }

    void setMovable(bool movable);
    void showLabel(bool visible);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void   paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

signals:
    void itemDropped(SceneryItem *item);

private:
    Scenery           *m_scenery;
    QList<QPolygonF>   m_parts;        //!< in item coordinates
    QRectF             m_bounds;       //!< all of them
    bool               m_elevation;
    QGraphicsTextItem *m_label;
    bool               m_dragMoved = false;
};

/** @} */

#endif

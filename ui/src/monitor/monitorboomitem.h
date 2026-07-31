/*
  Q Light Controller Plus
  monitorboomitem.h

  Interactive 2-D representation of a Boom (vertical fixture pipe on a stand)
  in the Monitor canvas. In the top view it draws the stand footprint (a base
  disc) with the pipe as a dot at its centre, labelled with the boom name and
  height. Movable when the layout is not locked.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef MONITORBOOMITEM_H
#define MONITORBOOMITEM_H

#include <QGraphicsItem>
#include <QObject>

class QGraphicsTextItem;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneContextMenuEvent;
class Boom;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class MonitorBoomItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    /**
     * @param boom     Engine Boom this item represents (not owned).
     * @param pxCX     Scene-pixel X of the boom base CENTRE.
     * @param pxCY     Scene-pixel Y of the boom base CENTRE.
     * @param pxBaseR  Stand base radius in scene pixels (0 = hung boom).
     * @param pxPipe   Pipe diameter in scene pixels.
     */
    MonitorBoomItem(Boom *boom, Doc *doc,
                    float pxCX, float pxCY, float pxBaseR, float pxPipe,
                    QGraphicsItem *parent = nullptr);

    quint32 boomId() const;
    Boom   *boom()   const { return m_boom; }

    void setMovable(bool movable);
    void showLabel(bool visible);

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void   paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

signals:
    /** Emitted when the user finishes a drag so the view can persist the move. */
    void itemDropped(MonitorBoomItem *item);

private:
    Boom              *m_boom;
    Doc               *m_doc;
    float              m_pxBaseR;
    float              m_pxPipe;
    QGraphicsTextItem *m_label;
};

/** @} */

#endif // MONITORBOOMITEM_H

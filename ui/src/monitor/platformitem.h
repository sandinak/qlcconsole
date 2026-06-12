/*
  Q Light Controller Plus
  platformitem.h

  Interactive 2-D representation of a StagePlatform in the Monitor canvas.
  Draws a semi-transparent filled rectangle labelled with the platform name
  and height.  Movable when the layout is not locked.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef PLATFORMITEM_H
#define PLATFORMITEM_H

#include <QGraphicsItem>
#include <QObject>

class QGraphicsTextItem;
class QGraphicsSceneMouseEvent;
class StagePlatform;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class PlatformItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    /**
     * @param platform  Engine StagePlatform this item represents (not owned).
     * @param pxX       Scene-pixel X of the platform origin.
     * @param pxY       Scene-pixel Y of the platform origin.
     * @param pxW       Platform width in scene pixels.
     * @param pxD       Platform depth in scene pixels.
     */
    PlatformItem(StagePlatform *platform, Doc *doc,
                 float pxX, float pxY, float pxW, float pxD,
                 QGraphicsItem *parent = nullptr);

    quint32        platformId() const;
    StagePlatform *platform()   const { return m_platform; }

    /** Enable or disable movement (called by MonitorGraphicsView on lock change). */
    void setMovable(bool movable);

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void   paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

signals:
    /** Emitted when the user finishes a drag so the view can persist the move. */
    void itemDropped(PlatformItem *item);

private:
    StagePlatform     *m_platform;
    Doc               *m_doc;
    float              m_pxW;
    float              m_pxD;
    QGraphicsTextItem *m_label;   ///< child item
};

/** @} */

#endif // PLATFORMITEM_H

/*
  Q Light Controller Plus
  monitorimageitem.h

  Interactive 2-D representation of a placeable MonitorProperties::MonitorImage
  on the Monitor canvas. Draws the image scaled to its rectangle; movable when
  unlocked. Only shown in the view matching its plane (Floor/Front/Side).

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef MONITORIMAGEITEM_H
#define MONITORIMAGEITEM_H

#include <QGraphicsItem>
#include <QPixmap>
#include <QObject>

class QGraphicsSceneMouseEvent;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class MonitorImageItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    MonitorImageItem(Doc *doc, quint32 imageId, QGraphicsItem *parent = nullptr);

    quint32 imageId() const { return m_id; }

    /** Position (scene-pixel top-left) and on-screen size (scene pixels). */
    void setGeometry(float pxX, float pxY, float pxW, float pxH);

    /** (Re)load the image file. */
    void setSource(const QString &path);

    /** Rotation in degrees (clockwise), applied about the item's centre. */
    void setImageRotation(float degrees);

    /** Enable/disable dragging (called by MonitorGraphicsView on lock changes). */
    void setMovable(bool movable);

    /** On-screen size in scene pixels (its current rectangle). */
    QSizeF pixelSize() const { return QSizeF(m_pxW, m_pxH); }

    QRectF boundingRect() const override;
    void   paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

signals:
    /** Emitted when the user finishes a move OR a resize; the view persists the
     *  new origin AND size from the item. */
    void itemDropped(MonitorImageItem *item);

private:
    /** Which corner handle (if any) the point (item coords) is over: 0 = none,
     *  1 = TL, 2 = TR, 3 = BL, 4 = BR. */
    int cornerAt(const QPointF &pt) const;

    Doc    *m_doc;
    quint32 m_id;
    float   m_pxW = 0.0f;
    float   m_pxH = 0.0f;
    QPixmap m_pixmap;

    float   m_rotationDeg = 0.0f;    ///< current display rotation
    int     m_resizeCorner = 0;      ///< active resize handle (0 = moving/none)
    QPointF m_resizeCenterScene;     ///< the centre held fixed during a resize
};

/** @} */

#endif // MONITORIMAGEITEM_H

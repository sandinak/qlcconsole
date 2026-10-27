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
class QGraphicsSceneContextMenuEvent;
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

    /** Show or hide the platform's name/size label. */
    void showLabel(bool visible);

    /** Which plot view it is drawn in -- 0 plan, 1 front, 2 side -- so a
     *  flight of steps can draw its treads, or its profile, the right way
     *  round. A box draws the same in all of them. */
    void setView(int view) { m_view = view; update(); }

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    /** What the item IS on the stage, in item coordinates: its own outline,
     *  without the padding, halos and labels boundingRect() adds for painting.
     *  The rulers band this (MonitorGraphicsView::itemGeometryRect()). */
    QRectF geometryRect() const { return QRectF(0, 0, m_pxW, m_pxD); }
    void   paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    void paintSteps(QPainter *painter, const QColor &fill, const QColor &border, bool selected);

signals:
    /** Emitted when the user finishes a drag so the view can persist the move. */
    void itemDropped(PlatformItem *item);

private:
    StagePlatform     *m_platform;
    Doc               *m_doc;
    float              m_pxW;
    float              m_pxD;
    QGraphicsTextItem *m_label;   ///< child item
    bool               m_dragMoved = false;
    int                m_view = 0;    ///< 0 plan, 1 front, 2 side   ///< a click only counts as a move once the pointer moves
};

/** @} */

#endif // PLATFORMITEM_H

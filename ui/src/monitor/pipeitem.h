/*
  Q Light Controller Plus
  pipeitem.h

  Interactive 2-D representation of a Pipe (vertical fixture pipe on a stand)
  in the Monitor canvas. In the top view it draws the stand footprint (a base
  disc) with the pipe as a dot at its centre, labelled with the pipe name and
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
class Pipe;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class PipeItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    /**
     * @param pipe       Engine Pipe this item represents (not owned).
     * @param pxX,pxY    Top view: the base CENTRE. Elevation: the base BOTTOM.
     * @param pxBaseR    Stand base radius in scene pixels (0 = hung / elevation).
     * @param pxPipe     Pipe diameter in scene pixels.
     * @param elevation  True in Front/Side views — draw a vertical pipe instead
     *                   of the plan disc.
     * @param pxHeight   Elevation only: pipe height in scene pixels.
     */
    PipeItem(Pipe *pipe, Doc *doc,
                    float pxX, float pxY, float pxBaseR, float pxPipe,
                    bool elevation = false, float pxHeight = 0.0f,
                    QGraphicsItem *parent = nullptr);

    quint32 pipeId() const;
    Pipe   *pipe()   const { return m_pipe; }

    /** Draw as a horizontal run: a line from the origin to @p pxEnd (relative to
     *  the item position). Used for horizontal pipes (electrics) in any view. */
    void setLine(const QPointF &pxEnd);

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
    void itemDropped(PipeItem *item);

    /** Emitted from the context menu on a vertical boom: hang a crossbar on it. */
    void addBarRequested(quint32 pipeId);

private:
    Pipe              *m_pipe;
    Doc               *m_doc;
    float              m_pxBaseR;
    float              m_pxPipe;
    bool               m_elevation;
    float              m_pxHeight;
    bool               m_horizontal = false;   ///< draw as a line to m_pxEnd
    QPointF            m_pxEnd;
    QGraphicsTextItem *m_label;
};

/** @} */

#endif // MONITORBOOMITEM_H

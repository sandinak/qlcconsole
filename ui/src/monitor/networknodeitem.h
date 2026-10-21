/*
  Q Light Controller Plus
  networknodeitem.h

  Interactive 2-D marker for a NetworkNode (ArtNet/sACN gateway, a network-
  native fixture's own interface, or a wireless DMX transmitter) on the
  Monitor canvas — a small labelled box you drop and drag to record where the
  node physically sits on the deck. Movable when the layout is unlocked.

  Mirrors PowerSourceItem's shape (same drag/lock/context-menu scaffolding);
  see powersourceitem.h.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef NETWORKNODEITEM_H
#define NETWORKNODEITEM_H

#include <QGraphicsItem>
#include "../qlcui_export.h"
#include <QGraphicsPathItem>
#include <QObject>
#include <QString>
#include <QColor>
#include <functional>

class QGraphicsSceneMouseEvent;
class QGraphicsSceneContextMenuEvent;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class QLC_UI_EXPORT NetworkNodeItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    /** @param nodeIndex index into NetworkTopology::nodes() (the model has no
     *         stable id; the view keeps items in sync by index, same as
     *         PowerSourceItem/PowerDistribution::sources()).
     *  @param transport  NetworkNode::Transport — drives the marker colour.
     */
    NetworkNodeItem(int nodeIndex, const QString &name, int transport,
                    bool locked = false, QGraphicsItem *parent = nullptr);

    int nodeIndex() const { return m_index; }
    bool locked() const { return m_locked; }

    /** The middle of the marker, in scene coordinates (where a link line ends). */
    QPointF centre() const;

    /** Enable/disable dragging (view calls this on lock change). */
    void setMovable(bool movable);

    QRectF boundingRect() const override;
    void   paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

signals:
    /** Emitted when the user finishes a drag so the view can persist the move. */
    void itemDropped(NetworkNodeItem *item);

    /** Emitted when the user toggles Lock/Unlock from the context menu. The view
     *  persists the new state to NetworkTopology and rebuilds the item. */
    void lockToggleRequested(int nodeIndex);

private:
    int     m_index;
    QString m_name;
    int     m_transport;
    bool    m_locked;
};

/**
 * Marker for a wireless DMX receiver: the box in the rig that a transmitter's
 * universe actually reaches, drawn where the receiver is rather than where the
 * transmitter sits. Display only -- its position is edited in the Network
 * Topology dialog. A dashed line (owned by the view) ties it back to its
 * transmitter. Drawn centred on its own origin.
 */
class NetworkReceiverItem : public QGraphicsItem
{
public:
    NetworkReceiverItem(int nodeIndex, int portIndex, const QString &name,
                        QGraphicsItem *parent = nullptr);

    int nodeIndex() const { return m_node; }
    int portIndex() const { return m_port; }

    QRectF boundingRect() const override;
    void   paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

    /** Dragged: called while it moves (the link follows) and once where it
     *  is let go (the receiver's new place is stored). */
    std::function<void()> onMoving;
    std::function<void()> onDropped;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    int     m_node;
    int     m_port;
    QString m_name;
};

/**
 * One port's cable run drawn on the plot: a polyline from the node (or the
 * wireless receiver) through the fixtures on its universe, in address order.
 * The tooltip carries the run's stats. The line is thin, so its hover shape is
 * a wider band around it -- otherwise it could hardly be hovered at all.
 */
class NetworkRunItem : public QGraphicsPathItem
{
public:
    NetworkRunItem(const QPainterPath &path, const QColor &colour, const QString &stats,
                   int nodeIndex, int portIndex, QGraphicsItem *parent = nullptr);

    int nodeIndex() const { return m_node; }
    int portIndex() const { return m_port; }
    QString stats() const { return toolTip(); }

    QPainterPath shape() const override;

private:
    int m_node;
    int m_port;
};

/** @} */

#endif // NETWORKNODEITEM_H

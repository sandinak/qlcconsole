/*
  Q Light Controller Plus
  stagepieceitem.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QCursor>

#include "stagepieceitem.h"
#include "monitorproperties.h"

StagePieceItem::StagePieceItem(int piece, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_piece(piece)
{
    setFlag(ItemIsSelectable, true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setData(0, QStringLiteral("stagePiece"));
}

void StagePieceItem::setHitShape(const QPainterPath &path)
{
    prepareGeometryChange();
    m_hit = path;
    update();
}

void StagePieceItem::setMovable(bool on)
{
    m_movable = on;
    if (!on)
        unsetCursor();
}

void StagePieceItem::selectNow(bool on)
{
    m_selectArmed = true;
    setSelected(on);
    m_selectArmed = false;
}

QRectF StagePieceItem::boundingRect() const
{
    return m_hit.boundingRect().adjusted(-2, -2, 2, 2);
}

void StagePieceItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (!(option->state & QStyle::State_Selected))
        return;
    // Selected: its outline, in the plot's selection blue (red when it is held still).
    const QColor edge = m_movable ? QColor(80, 170, 255) : QColor(220, 90, 90);
    QPen pen(edge, 1.6, Qt::SolidLine);
    pen.setCosmetic(true);
    painter->setPen(pen);
    QColor fill = edge;
    fill.setAlpha(m_piece == MonitorProperties::ApronPiece ? 45 : 70);
    painter->setBrush(fill);
    painter->drawPath(m_hit);
}

QVariant StagePieceItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    /* Picked only by a click on it or from code: a rubber band drawn
       anywhere across the stage would catch every line. */
    if (change == ItemSelectedChange && value.toBool() && !m_selectArmed)
        return QVariant(false);
    return QGraphicsObject::itemChange(change, value);
}

void StagePieceItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_selectArmed = true;
    QGraphicsObject::mousePressEvent(event);
    m_selectArmed = false;
    m_dragging = false;
    m_moved = false;
    if (event->button() == Qt::LeftButton && m_movable && m_dragShape.contains(event->scenePos()))
    {
        m_dragging = true;
        m_pressPos = event->scenePos();
        emit dragStarted(m_piece, m_pressPos);
    }
    event->accept();
}

void StagePieceItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_dragging)
    {
        event->accept();
        return;
    }
    if (!m_moved && (event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton)).manhattanLength() < 3)
        return;
    m_moved = true;
    emit dragMoved(m_piece, event->scenePos());
    event->accept();
}

void StagePieceItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragging)
    {
        m_dragging = false;
        emit dragFinished(m_piece, m_moved);
    }
    event->accept();
}

void StagePieceItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (m_movable && m_dragShape.contains(event->scenePos()))
        setCursor(Qt::SizeVerCursor);
    else
        unsetCursor();
    QGraphicsObject::hoverMoveEvent(event);
}

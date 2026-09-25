/*
  Q Light Controller Plus
  sceneryitem.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QStyleOptionGraphicsItem>
#include <QGraphicsTextItem>
#include <QPainterPathStroker>
#include <QPainter>
#include <QCursor>

#include "sceneryitem.h"
#include "scenery.h"

SceneryItem::SceneryItem(Scenery *scenery, Doc *doc, const QPointF &anchor,
                         const QPolygonF &outline, bool elevation, QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsItem(parent)
    , m_scenery(scenery)
    , m_elevation(elevation)
    , m_label(nullptr)
{
    Q_UNUSED(doc)
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    // Behind everything: it is the back of the stage, not something on it.
    setZValue(-1.2);
    setPos(anchor);
    m_outline = outline.translated(-anchor);
    setMovable(true);
    setToolTip(tr("%1: %2").arg(Scenery::kindName(scenery->kind()), scenery->name()));

    m_label = new QGraphicsTextItem(scenery->name().isEmpty()
                                        ? Scenery::kindName(scenery->kind()) : scenery->name(), this);
    m_label->setDefaultTextColor(QColor(220, 220, 228, 200));
    m_label->setFont(QFont("Arial", 8));
    m_label->setZValue(0.1);
    const QRectF b = m_outline.boundingRect();
    m_label->setPos(b.center().x() - m_label->boundingRect().width() / 2.0,
                    m_elevation ? b.top() + 2.0 : b.top() - m_label->boundingRect().height() - 2.0);
}

quint32 SceneryItem::sceneryId() const { return m_scenery->id(); }

void SceneryItem::setMovable(bool movable)
{
    // Moved on the floor plan only, as the stands and towers are.
    const bool can = movable && !m_elevation && !m_scenery->locked();
    setFlag(ItemIsMovable, can);
    setCursor(can ? Qt::SizeAllCursor : Qt::ArrowCursor);
}

void SceneryItem::showLabel(bool visible)
{
    if (m_label != nullptr)
        m_label->setVisible(visible);
}

QRectF SceneryItem::boundingRect() const
{
    return m_outline.boundingRect().adjusted(-6, -6, 6, 6);
}

QPainterPath SceneryItem::shape() const
{
    QPainterPath p;
    if (m_elevation)
    {
        p.addPolygon(m_outline);
        p.closeSubpath();
        return p;
    }
    // A line is only as easy to grab as it is wide.
    p.addPolygon(m_outline);
    QPainterPathStroker stroker;
    stroker.setWidth(10.0);
    return stroker.createStroke(p);
}

void SceneryItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool selected = option->state & QStyle::State_Selected;
    QColor cloth = m_scenery->color().isValid() ? m_scenery->color() : QColor(226, 228, 236);
    const QColor edge = m_scenery->locked() ? QColor(200, 60, 60)
                                            : cloth.darker(selected ? 190 : 150);
    if (m_elevation)
    {
        QColor fill = cloth;
        fill.setAlpha(selected ? 110 : 70);
        painter->setPen(QPen(edge, selected ? 2.0 : 1.2));
        painter->setBrush(fill);
        painter->drawPolygon(m_outline);
        return;
    }
    // Top: the cloth's line, drawn heavy, with a thin edge along it.
    QPen band(cloth, selected ? 7.0 : 5.0, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
    painter->setPen(band);
    painter->drawPolyline(m_outline);
    painter->setPen(QPen(edge, 1.0));
    painter->drawPolyline(m_outline);
}

void SceneryItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_dragMoved = false;
    QGraphicsItem::mousePressEvent(event);
}

void SceneryItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    m_dragMoved = true;
    QGraphicsItem::mouseMoveEvent(event);
}

void SceneryItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    if (m_dragMoved)          // a bare click must not nudge it
        emit itemDropped(this);
}

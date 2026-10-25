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

#include "itemlock.h"
#include "sceneryitem.h"
#include "scenery.h"
#include "apputil.h"

SceneryItem::SceneryItem(Scenery *scenery, Doc *doc, const QPointF &anchor,
                         const QList<QPolygonF> &parts, bool elevation, QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsItem(parent)
    , m_scenery(scenery)
    , m_elevation(elevation)
    , m_label(nullptr)
{
    Q_UNUSED(doc)
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    /* Seen flat it is a sheet behind the stage, behind everything. From above
       it is a line standing on or over the deck -- a drape across the front
       steps, a fountain's wall on its base -- so it is picked before the deck
       under it ("when I double click to edit it .. I get the step nearest it"). */
    setZValue(elevation ? -1.2 : -0.95);
    setPos(anchor);
    foreach (const QPolygonF &part, parts)
    {
        m_parts << part.translated(-anchor);
        m_bounds |= m_parts.last().boundingRect();
    }
    setMovable(true);
    setToolTip(tr("%1: %2").arg(Scenery::kindName(scenery->kind()), scenery->name()));

    m_label = new QGraphicsTextItem(scenery->name().isEmpty()
                                        ? Scenery::kindName(scenery->kind()) : scenery->name(), this);
    m_label->setDefaultTextColor(QColor(220, 220, 228, 200));
    m_label->setFont(QFont("Arial", 8));
    m_label->setZValue(0.1);
    placeLabel();
}

void SceneryItem::placeLabel()
{
    if (m_label == nullptr)
        return;
    const QRectF b = m_bounds;
    m_label->setPos(b.center().x() - m_label->boundingRect().width() / 2.0,
                    m_elevation ? b.top() + 2.0 : b.top() - m_label->boundingRect().height() - 2.0);
}

void SceneryItem::setParts(const QPointF &anchor, const QList<QPolygonF> &parts, bool flown,
                           const Scenery::Pose &pose)
{
    prepareGeometryChange();
    /* Relative to where its model says it is: a drag in progress keeps the
       offset it has given the item until it is dropped. */
    m_parts.clear();
    m_bounds = QRectF();
    foreach (const QPolygonF &part, parts)
    {
        m_parts << part.translated(-anchor);
        m_bounds |= m_parts.last().boundingRect();
    }
    m_flown = flown;
    m_pose = pose;
    m_hasPose = true;
    placeLabel();
    update();
}

quint32 SceneryItem::sceneryId() const { return m_scenery->id(); }

void SceneryItem::setMovable(bool movable)
{
    // Moved on the floor plan only, as the stands and towers are.
    const bool can = movable && !m_elevation && !m_scenery->locked();
    setFlag(ItemIsMovable, can);
    setCursor(can ? AppUtil::nativeCursorShape(Qt::SizeAllCursor) : Qt::ArrowCursor);
}

void SceneryItem::showLabel(bool visible)
{
    if (m_label != nullptr)
        m_label->setVisible(visible);
}

QRectF SceneryItem::boundingRect() const
{
    return m_bounds.adjusted(-6, -6, 6, 6);
}

QPainterPath SceneryItem::shape() const
{
    QPainterPath p;
    const bool filled = m_elevation || m_scenery->thickness() > 0.0f;
    foreach (const QPolygonF &part, m_parts)
    {
        p.addPolygon(part);
        if (filled)
            p.closeSubpath();
    }
    if (filled)
        return p;
    // A line is only as easy to grab as it is wide.
    QPainterPathStroker stroker;
    stroker.setWidth(10.0);
    return stroker.createStroke(p);
}

void SceneryItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool selected = option->state & QStyle::State_Selected;
    QColor cloth = m_scenery->color().isValid() ? m_scenery->color()
                                                : Scenery::defaultColor(m_scenery->kind());
    // Masking black on a dark plot: lift it enough to be seen.
    if (cloth.lightness() < 60)
        cloth = cloth.lighter(260);
    const QColor edge = ItemLock::drawnLocked(this, m_scenery->locked()) ? QColor(200, 60, 60)
                                            : cloth.darker(selected ? 190 : 150).lighter(selected ? 160 : 100);
    // Flown out: only where it hangs from, dashed.
    if (m_flown)
    {
        painter->setPen(QPen(cloth, selected ? 2.0 : 1.2, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        foreach (const QPolygonF &part, m_parts)
            m_elevation ? painter->drawPolygon(part) : painter->drawPolyline(part);
        return;
    }
    if (m_elevation || m_scenery->thickness() > 0.0f)
    {
        QColor fill = cloth;
        fill.setAlpha(m_elevation ? (selected ? 110 : 70) : (selected ? 230 : 190));
        painter->setPen(QPen(edge, selected ? 2.0 : 1.2));
        painter->setBrush(fill);
        foreach (const QPolygonF &part, m_parts)
            painter->drawPolygon(part);
        return;
    }
    // Top: the cloth's line, drawn heavy, with a thin edge along it.
    QPen band(cloth, selected ? 7.0 : 5.0, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
    foreach (const QPolygonF &part, m_parts)
    {
        painter->setPen(band);
        painter->drawPolyline(part);
        painter->setPen(QPen(edge, 1.0));
        painter->drawPolyline(part);
    }
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

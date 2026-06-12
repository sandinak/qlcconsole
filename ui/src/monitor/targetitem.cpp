/*
  Q Light Controller Plus
  targetitem.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QPen>
#include <QFont>

#include "targetitem.h"
#include "stagetarget.h"
#include "doc.h"

TargetItem::TargetItem(StageTarget *target, Doc *doc, float pxX, float pxY,
                       QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsItem(parent)
    , m_target(target)
    , m_doc(doc)
    , m_label(nullptr)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(3.0);   // above fixtures so it's always clickable
    setCursor(Qt::PointingHandCursor);
    setPos(pxX, pxY);

    QString labelText = target->name();
    if (target->z() > 0.0f)
        labelText += QString("\n(Z: %1m)").arg(double(target->z()), 0, 'f', 1);

    m_label = new QGraphicsTextItem(labelText, this);
    QColor textColor = target->color().isValid() ? target->color().lighter(160)
                                                 : QColor(255, 220, 80);
    m_label->setDefaultTextColor(textColor);
    m_label->setFont(QFont("Arial", 7));
    m_label->setZValue(0.1);
    m_label->setPos(kRadius + 3, -kRadius);
}

quint32 TargetItem::targetId() const
{
    return m_target->id();
}

void TargetItem::setScenePos(float pxX, float pxY)
{
    setPos(pxX, pxY);
}

void TargetItem::setMovable(bool movable)
{
    setFlag(ItemIsMovable, movable);
    setCursor(movable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

QRectF TargetItem::boundingRect() const
{
    const qreal pad = 3.0;
    const qreal r = kRadius + pad;
    return QRectF(-r, -r, r * 2, r * 2);
}

void TargetItem::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget * /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = option->state & QStyle::State_Selected;
    QColor col = m_target->color().isValid() ? m_target->color() : QColor(255, 180, 0);

    // Outer ring
    QPen ringPen(col, selected ? 2.5 : 1.5);
    painter->setPen(ringPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF(0, 0), double(kRadius), double(kRadius));

    // Inner dot
    QColor dotColor = col;
    dotColor.setAlpha(selected ? 220 : 160);
    painter->setBrush(QBrush(dotColor));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QPointF(0, 0), 3.0, 3.0);

    // Crosshair lines
    painter->setPen(QPen(col, 1.0));
    const float arm = kRadius * 1.4f;
    const float gap = 4.0f;
    painter->drawLine(QPointF(-arm, 0), QPointF(-gap, 0));
    painter->drawLine(QPointF( gap, 0), QPointF( arm, 0));
    painter->drawLine(QPointF(0, -arm), QPointF(0, -gap));
    painter->drawLine(QPointF(0,  gap), QPointF(0,  arm));

    // Selection glow ring
    if (selected)
    {
        QPen glowPen(col.lighter(130), 3.5);
        glowPen.setStyle(Qt::SolidLine);
        painter->setPen(glowPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0, 0), double(kRadius) + 3.0, double(kRadius) + 3.0);
    }
}

QVariant TargetItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    Q_UNUSED(change)
    return QGraphicsItem::itemChange(change, value);
}

void TargetItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    emit itemDropped(this);
}

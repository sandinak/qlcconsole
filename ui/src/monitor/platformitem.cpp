/*
  Q Light Controller Plus
  platformitem.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QFont>

#include "platformitem.h"
#include "stageplatform.h"
#include "monitorproperties.h"
#include "doc.h"

PlatformItem::PlatformItem(StagePlatform *platform, Doc *doc,
                           float pxX, float pxY, float pxW, float pxD,
                           QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsItem(parent)
    , m_platform(platform)
    , m_doc(doc)
    , m_pxW(qMax(pxW, 8.0f))
    , m_pxD(qMax(pxD, 8.0f))
    , m_label(nullptr)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(-1.0);   // below trusses and fixtures
    setCursor(Qt::SizeAllCursor);
    setPos(pxX, pxY);

    bool isFeet = doc->monitorProperties()->gridUnits() == MonitorProperties::Feet;
    double dispH = double(platform->height()) * (isFeet ? 3.28084 : 1.0);
    QString unitStr = isFeet ? "ft" : "m";
    m_label = new QGraphicsTextItem(
        QString("%1\n(%2 %3)").arg(platform->name()).arg(dispH, 0, 'f', 2).arg(unitStr),
        this);
    m_label->setDefaultTextColor(QColor(240, 240, 240, 200));
    m_label->setFont(QFont("Arial", 7));
    m_label->setZValue(0.1);
    m_label->setPos(4, 4);
}

quint32 PlatformItem::platformId() const
{
    return m_platform->id();
}

void PlatformItem::setMovable(bool movable)
{
    setFlag(ItemIsMovable, movable);
    setCursor(movable ? Qt::SizeAllCursor : Qt::ArrowCursor);
}

QRectF PlatformItem::boundingRect() const
{
    const qreal pad = 2.0;
    return QRectF(-pad, -pad, m_pxW + 2*pad, m_pxD + 2*pad);
}

void PlatformItem::paint(QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget * /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing, false);

    const bool selected = option->state & QStyle::State_Selected;

    QColor fill = m_platform->color();
    fill.setAlpha(selected ? 140 : 80);
    QColor border = fill;
    border.setAlpha(selected ? 255 : 180);

    painter->setPen(QPen(border, selected ? 2.0 : 1.5));
    painter->setBrush(QBrush(fill));
    painter->drawRect(QRectF(0, 0, m_pxW, m_pxD));

    // Hatch lines to distinguish from floor
    QPen hatchPen(border.lighter(140));
    hatchPen.setWidth(1);
    hatchPen.setStyle(Qt::DotLine);
    painter->setPen(hatchPen);
    const float spacing = 12.0f;
    for (float x = spacing; x < m_pxW; x += spacing)
        painter->drawLine(QPointF(x, 0), QPointF(x, m_pxD));
    for (float y = spacing; y < m_pxD; y += spacing)
        painter->drawLine(QPointF(0, y), QPointF(m_pxW, y));
}

QVariant PlatformItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    Q_UNUSED(change)
    return QGraphicsItem::itemChange(change, value);
}

void PlatformItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    emit itemDropped(this);
}

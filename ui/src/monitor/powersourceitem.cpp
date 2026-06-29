/*
  Q Light Controller Plus
  powersourceitem.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QFont>

#include "powersourceitem.h"

#define PSI_W 64.0
#define PSI_H 26.0

PowerSourceItem::PowerSourceItem(int sourceIndex, const QString &name,
                                 QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsItem(parent)
    , m_index(sourceIndex)
    , m_name(name)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(0.5);   // above platforms, below fixtures
    setCursor(Qt::SizeAllCursor);
    setToolTip(QObject::tr("Power source: %1").arg(name));
}

void PowerSourceItem::setMovable(bool movable)
{
    setFlag(ItemIsMovable, movable);
    setCursor(movable ? Qt::SizeAllCursor : Qt::ArrowCursor);
}

void PowerSourceItem::setCircuitColors(const QList<QColor> &colors)
{
    m_circuitColors = colors;
    update();
}

QRectF PowerSourceItem::boundingRect() const
{
    const qreal pad = 2.0;
    return QRectF(-pad, -pad, PSI_W + 2 * pad, PSI_H + 2 * pad);
}

void PowerSourceItem::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget * /*widget*/)
{
    const bool selected = option->state & QStyle::State_Selected;

    QColor fill(247, 181, 41);       // distro amber
    QColor border = fill.darker(160);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(border, selected ? 2.0 : 1.5));
    painter->setBrush(QBrush(fill));
    painter->drawRoundedRect(QRectF(0, 0, PSI_W, PSI_H), 4, 4);

    // ⚡ glyph + name
    painter->setPen(QPen(QColor(40, 30, 0)));
    QFont f("Arial", 9, QFont::Bold);
    painter->setFont(f);
    painter->drawText(QRectF(0, 0, PSI_H, PSI_H), Qt::AlignCenter,
                      QString::fromUtf8("⚡"));
    painter->drawText(QRectF(PSI_H, 0, PSI_W - PSI_H, PSI_H),
                      Qt::AlignVCenter | Qt::AlignLeft, m_name);

    // Power view: a strip of this source's circuit colours along the bottom edge,
    // so the source and the fixtures on each of its circuits read as one colour.
    if (!m_circuitColors.isEmpty())
    {
        const qreal stripH = 5.0;
        const qreal segW = PSI_W / m_circuitColors.size();
        painter->setPen(Qt::NoPen);
        for (int i = 0; i < m_circuitColors.size(); i++)
        {
            painter->setBrush(m_circuitColors.at(i));
            painter->drawRect(QRectF(i * segW, PSI_H - stripH, segW, stripH));
        }
    }
}

void PowerSourceItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    emit itemDropped(this);
}

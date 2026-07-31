/*
  Q Light Controller Plus
  toweritem.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QFont>
#include <QMenu>

#include "toweritem.h"
#include "tower.h"
#include "monitorproperties.h"
#include "doc.h"

TowerItem::TowerItem(Tower *tower, Doc *doc,
                     float pxX, float pxY, float pxW, float pxH,
                     bool elevation, QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsItem(parent)
    , m_tower(tower)
    , m_doc(doc)
    , m_pxW(qMax(pxW, 6.0f))
    , m_pxH(qMax(pxH, 6.0f))
    , m_elevation(elevation)
    , m_label(nullptr)
{
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(-0.9);
    setFlag(ItemIsMovable, !elevation && !tower->locked());
    setCursor((elevation || tower->locked()) ? Qt::ArrowCursor : Qt::SizeAllCursor);
    setPos(pxX, pxY);

    const bool isFeet = doc->monitorProperties()->gridUnits() == MonitorProperties::Feet;
    const double conv = isFeet ? 3.28084 : 1.0;
    const QString unitStr = isFeet ? "ft" : "m";
    m_label = new QGraphicsTextItem(
        QString("%1 (%2 %3 · %4 shelves)").arg(tower->name())
            .arg(double(tower->height()) * conv, 0, 'f', 1).arg(unitStr)
            .arg(tower->shelfCount()),
        this);
    m_label->setDefaultTextColor(QColor(220, 224, 235, 210));
    m_label->setFont(QFont("Arial", 8, QFont::Bold));
    m_label->setZValue(0.1);
    if (m_elevation)
        m_label->setPos(m_pxW + 3.0, -m_pxH);
    else
        m_label->setPos(m_pxW + 3.0, 0);
}

quint32 TowerItem::towerId() const { return m_tower->id(); }

void TowerItem::setMovable(bool movable)
{
    setFlag(ItemIsMovable, movable && !m_elevation);
    setCursor((movable && !m_elevation) ? Qt::SizeAllCursor : Qt::ArrowCursor);
}

void TowerItem::showLabel(bool visible)
{
    if (m_label != nullptr)
        m_label->setVisible(visible);
}

QRectF TowerItem::boundingRect() const
{
    const qreal pad = 3.0;
    if (m_elevation)
        return QRectF(-pad, -m_pxH - pad, m_pxW + 2 * pad, m_pxH + 2 * pad);
    return QRectF(-pad, -pad, m_pxW + 2 * pad, m_pxH + 2 * pad);
}

void TowerItem::paint(QPainter *painter,
                      const QStyleOptionGraphicsItem *option,
                      QWidget * /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing, false);
    const bool selected = option->state & QStyle::State_Selected;

    QColor base = m_tower->color();
    if (!base.isValid())
        base = QColor(90, 100, 120);
    QColor border = m_tower->locked() ? QColor(200, 60, 60)
                                     : base.darker(selected ? 150 : 120);
    QColor fill = base; fill.setAlpha(selected ? 150 : 90);

    if (m_elevation)
    {
        // Box outline width × height, with a horizontal line at each shelf.
        painter->setPen(QPen(border, selected ? 2.0 : 1.4));
        painter->setBrush(fill);
        painter->drawRect(QRectF(0, -m_pxH, m_pxW, m_pxH));
        QPen shelfPen(border.lighter(150), 2.0);
        painter->setPen(shelfPen);
        const float h = m_tower->height();
        for (int i = 0; i < m_tower->shelfCount(); ++i)
        {
            const float frac = (h > 0.0f) ? m_tower->shelfHeight(i) / h : 0.0f;
            const qreal y = -m_pxH * frac;
            painter->drawLine(QPointF(0, y), QPointF(m_pxW, y));
        }
        return;
    }

    // Top: footprint square + diagonal cross (box-truss) + shelf-count badge.
    painter->setPen(QPen(border, selected ? 2.0 : 1.5));
    painter->setBrush(fill);
    painter->drawRect(QRectF(0, 0, m_pxW, m_pxH));
    painter->drawLine(QPointF(0, 0), QPointF(m_pxW, m_pxH));
    painter->drawLine(QPointF(m_pxW, 0), QPointF(0, m_pxH));
}

void TowerItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    emit itemDropped(this);
}

void TowerItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    QAction *lockAct = menu.addAction(
        m_tower->locked() ? tr("Unlock Tower") : tr("Lock Tower"));
    if (menu.exec(event->screenPos()) == lockAct)
    {
        m_tower->setLocked(!m_tower->locked());
        const bool canMove = !m_tower->locked();
        setFlag(ItemIsMovable, canMove && !m_elevation);
        setCursor((canMove && !m_elevation) ? Qt::SizeAllCursor : Qt::ArrowCursor);
        update();
        if (m_doc) m_doc->setModified();
    }
}

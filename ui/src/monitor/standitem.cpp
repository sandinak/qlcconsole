/*
  Q Light Controller Plus
  standitem.cpp

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
#include <QtMath>

#include "standitem.h"
#include "stand.h"
#include "monitorproperties.h"
#include "doc.h"

StandItem::StandItem(Stand *stand, Doc *doc,
                     float pxX, float pxY, float pxBaseR,
                     bool elevation, float pxHeight,
                     QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsItem(parent)
    , m_stand(stand)
    , m_doc(doc)
    , m_pxBaseR(qMax(pxBaseR, 4.0f))
    , m_elevation(elevation)
    , m_pxHeight(qMax(pxHeight, 0.0f))
    , m_label(nullptr)
{
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(-0.8);   // below pipes/fixtures, above platforms
    setFlag(ItemIsMovable, !elevation && !stand->locked());
    setCursor((elevation || stand->locked()) ? Qt::ArrowCursor : Qt::SizeAllCursor);
    setPos(pxX, pxY);

    const bool isFeet = doc->monitorProperties()->gridUnits() == MonitorProperties::Feet;
    const double conv = isFeet ? 3.28084 : 1.0;
    const QString unitStr = isFeet ? "ft" : "m";
    m_label = new QGraphicsTextItem(
        QString("%1 (%2 %3)").arg(stand->name())
            .arg(double(stand->height()) * conv, 0, 'f', 1).arg(unitStr),
        this);
    m_label->setDefaultTextColor(QColor(220, 220, 228, 200));
    m_label->setFont(QFont("Arial", 8));
    m_label->setZValue(0.1);
    if (m_elevation)
        m_label->setPos(6.0, -m_pxHeight);
    else
        m_label->setPos(m_pxBaseR + 3.0f, -m_label->boundingRect().height() / 2.0);
}

quint32 StandItem::standId() const { return m_stand->id(); }

void StandItem::setMovable(bool movable)
{
    setFlag(ItemIsMovable, movable && !m_elevation);
    setCursor((movable && !m_elevation) ? Qt::SizeAllCursor : Qt::ArrowCursor);
}

void StandItem::showLabel(bool visible)
{
    if (m_label != nullptr)
        m_label->setVisible(visible);
}

QRectF StandItem::boundingRect() const
{
    if (m_elevation)
        return QRectF(-m_pxBaseR - 2.0, -m_pxHeight - 2.0, 2 * m_pxBaseR + 4.0, m_pxHeight + 6.0);
    const qreal r = m_pxBaseR + 3.0;
    return QRectF(-r, -r, 2 * r, 2 * r);
}

void StandItem::paint(QPainter *painter,
                      const QStyleOptionGraphicsItem *option,
                      QWidget * /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool selected = option->state & QStyle::State_Selected;

    QColor base = m_stand->color();
    if (!base.isValid())
        base = QColor(120, 120, 130);
    QColor border = m_stand->locked() ? QColor(200, 60, 60)
                                      : base.darker(selected ? 160 : 130);

    if (m_elevation)
    {
        // A slim vertical post + a wider base foot + a small top plate.
        painter->setPen(QPen(border, selected ? 2.0 : 1.4));
        painter->drawLine(QPointF(0, 0), QPointF(0, -m_pxHeight));
        painter->drawLine(QPointF(-m_pxBaseR, 0), QPointF(m_pxBaseR, 0));      // foot
        painter->drawLine(QPointF(-m_pxBaseR * 0.5, -m_pxHeight),
                          QPointF(m_pxBaseR * 0.5, -m_pxHeight));              // top plate
        return;
    }

    // Top: base footprint disc with a small centre post dot.
    QColor plate = base; plate.setAlpha(selected ? 110 : 60);
    painter->setPen(QPen(border, selected ? 2.0 : 1.2));
    painter->setBrush(plate);
    painter->drawEllipse(QPointF(0, 0), m_pxBaseR, m_pxBaseR);
    painter->setBrush(Qt::NoBrush);
    // Legs rotate with the stand's leg orientation so the user can tuck them
    // under a riser. One leg is drawn bolder as a "front" reference.
    const double rot = qDegreesToRadians(double(m_stand->rotation()));
    for (int i = 0; i < 3; ++i)   // tripod legs
    {
        const double a = M_PI / 2.0 + rot + i * (2.0 * M_PI / 3.0);
        const QPointF foot(qCos(a) * m_pxBaseR, qSin(a) * m_pxBaseR);
        painter->setPen(QPen(i == 0 ? border.lighter(150) : border,
                             (i == 0 ? 2.2 : 1.2)));
        painter->drawLine(QPointF(0, 0), foot);
        painter->drawEllipse(foot, 2.0, 2.0);   // foot pad
    }
    painter->setPen(QPen(border, 1.2));
    painter->setBrush(border);
    painter->drawEllipse(QPointF(0, 0), 3.0, 3.0);
}

void StandItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    emit itemDropped(this);
}

void StandItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    QAction *lockAct = menu.addAction(
        m_stand->locked() ? tr("Unlock Stand") : tr("Lock Stand"));
    if (menu.exec(event->screenPos()) == lockAct)
    {
        m_stand->setLocked(!m_stand->locked());
        const bool canMove = !m_stand->locked();
        setFlag(ItemIsMovable, canMove && !m_elevation);
        setCursor((canMove && !m_elevation) ? Qt::SizeAllCursor : Qt::ArrowCursor);
        update();
        if (m_doc) m_doc->setModified();
    }
}

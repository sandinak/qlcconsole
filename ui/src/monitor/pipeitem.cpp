/*
  Q Light Controller Plus
  monitorboomitem.cpp

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

#include "monitorboomitem.h"
#include "boom.h"
#include "monitorproperties.h"
#include "doc.h"

MonitorBoomItem::MonitorBoomItem(Boom *boom, Doc *doc,
                                 float pxX, float pxY, float pxBaseR, float pxPipe,
                                 bool elevation, float pxHeight,
                                 QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsItem(parent)
    , m_boom(boom)
    , m_doc(doc)
    , m_pxBaseR(qMax(pxBaseR, 0.0f))
    , m_pxPipe(qBound(4.0f, pxPipe, 40.0f))
    , m_elevation(elevation)
    , m_pxHeight(qMax(pxHeight, 0.0f))
    , m_label(nullptr)
{
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(-0.5);   // above the floor/platforms, below fixtures
    // Elevation views are display-only (no dragging); top view drags to reposition.
    setFlag(ItemIsMovable, !elevation && !boom->locked());
    setCursor((elevation || boom->locked()) ? Qt::ArrowCursor : Qt::SizeAllCursor);
    setPos(pxX, pxY);

    const bool isFeet = doc->monitorProperties()->gridUnits() == MonitorProperties::Feet;
    const double conv = isFeet ? 3.28084 : 1.0;
    const QString unitStr = isFeet ? "ft" : "m";
    m_label = new QGraphicsTextItem(
        QString("%1 (%2 %3)").arg(boom->name())
            .arg(double(boom->height()) * conv, 0, 'f', 1).arg(unitStr),
        this);
    m_label->setDefaultTextColor(QColor(235, 235, 240, 220));
    m_label->setFont(QFont("Arial", 9, QFont::Bold));
    m_label->setZValue(0.1);
    if (m_elevation)
        m_label->setPos(qMax(3.0f, m_pxPipe) + 2.0f, -m_pxHeight);   // by the top
    else
    {
        const float r = qMax(m_pxBaseR, m_pxPipe);
        m_label->setPos(r + 3.0f, -m_label->boundingRect().height() / 2.0);
    }
}

quint32 MonitorBoomItem::boomId() const
{
    return m_boom->id();
}

void MonitorBoomItem::setMovable(bool movable)
{
    setFlag(ItemIsMovable, movable);
    setCursor(movable ? Qt::SizeAllCursor : Qt::ArrowCursor);
}

void MonitorBoomItem::showLabel(bool visible)
{
    if (m_label != nullptr)
        m_label->setVisible(visible);
}

QRectF MonitorBoomItem::boundingRect() const
{
    if (m_elevation)
    {
        const qreal w = qMax(3.0f, m_pxPipe) / 2.0 + 2.0;
        return QRectF(-w, -m_pxHeight - 2.0, 2 * w, m_pxHeight + 6.0);
    }
    const qreal r = qMax(m_pxBaseR, m_pxPipe) + 3.0;
    return QRectF(-r, -r, 2 * r, 2 * r);
}

void MonitorBoomItem::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget * /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool selected = option->state & QStyle::State_Selected;

    QColor base = m_boom->color();
    if (!base.isValid())
        base = QColor(150, 150, 160);
    QColor border = m_boom->locked() ? QColor(200, 60, 60)
                                     : base.darker(selected ? 160 : 130);

    // Elevation: a vertical pipe from the base (0,0) up to (0, -height), with a
    // small foot if it stands on a base.
    if (m_elevation)
    {
        const qreal w = qMax(3.0f, m_pxPipe);
        QColor pipe = base; pipe.setAlpha(selected ? 230 : 190);
        painter->setPen(QPen(border, selected ? 2.0 : 1.2));
        painter->setBrush(pipe);
        painter->drawRect(QRectF(-w / 2.0, -m_pxHeight, w, m_pxHeight));
        if (m_boom->hasStand())   // stand foot
            painter->drawLine(QPointF(-w * 2.0, 0), QPointF(w * 2.0, 0));
        return;
    }

    // Stand base disc (with three little tripod legs) — only when it has a stand.
    if (m_boom->hasStand() && m_pxBaseR > 1.0f)
    {
        QColor plate = base;
        plate.setAlpha(selected ? 120 : 70);
        painter->setPen(QPen(border, selected ? 2.0 : 1.2));
        painter->setBrush(plate);
        painter->drawEllipse(QPointF(0, 0), m_pxBaseR, m_pxBaseR);
        painter->setBrush(Qt::NoBrush);
        for (int i = 0; i < 3; ++i)
        {
            const double a = M_PI / 2.0 + i * (2.0 * M_PI / 3.0);
            painter->drawLine(QPointF(0, 0),
                              QPointF(qCos(a) * m_pxBaseR, qSin(a) * m_pxBaseR));
        }
    }

    // The pipe seen end-on: a filled dot at the centre.
    QColor pipe = base;
    pipe.setAlpha(255);
    painter->setPen(QPen(border, selected ? 2.0 : 1.2));
    painter->setBrush(pipe);
    const qreal pr = qMax(3.0f, m_pxPipe / 2.0f);
    painter->drawEllipse(QPointF(0, 0), pr, pr);
}

void MonitorBoomItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    emit itemDropped(this);
}

void MonitorBoomItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    QAction *lockAct = menu.addAction(
        m_boom->locked() ? tr("Unlock Boom") : tr("Lock Boom"));

    if (menu.exec(event->screenPos()) == lockAct)
    {
        m_boom->setLocked(!m_boom->locked());
        const bool canMove = !m_boom->locked();
        setFlag(ItemIsMovable, canMove);
        setCursor(canMove ? Qt::SizeAllCursor : Qt::ArrowCursor);
        update();
        if (m_doc)
            m_doc->setModified();
    }
}

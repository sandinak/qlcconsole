/*
  Q Light Controller Plus
  networknodeitem.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QMenu>

#include <QtMath>
#include "networknodeitem.h"
#include "networktopology.h"

#define NNI_W 72.0
#define NNI_H 26.0

static QColor transportColor(int transport)
{
    switch (transport)
    {
        case NetworkNode::sACN:        return QColor(160, 100, 210);   // violet
        case NetworkNode::Interface:   return QColor(150, 150, 150);   // grey
        case NetworkNode::WirelessDMX: return QColor(235, 140, 40);    // amber-orange
        case NetworkNode::ArtNet:
        default:                       return QColor(70, 140, 220);    // blue
    }
}

static QString transportGlyph(int transport)
{
    // Antenna for wireless, globe for everything IP-routed.
    return transport == NetworkNode::WirelessDMX
           ? QString::fromUtf8("\xF0\x9F\x93\xA1")    // 📡
           : QString::fromUtf8("\xF0\x9F\x8C\x90");   // 🌐
}

NetworkNodeItem::NetworkNodeItem(int nodeIndex, const QString &name, int transport,
                                 bool locked, QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsItem(parent)
    , m_index(nodeIndex)
    , m_name(name)
    , m_transport(transport)
    , m_locked(locked)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(0.5);   // above platforms, below fixtures -- same layer as power sources
    setCursor(Qt::SizeAllCursor);
    setToolTip(QObject::tr("Network node: %1").arg(name));
}

void NetworkNodeItem::setMovable(bool movable)
{
    setFlag(ItemIsMovable, movable);
    setCursor(movable ? Qt::SizeAllCursor : Qt::ArrowCursor);
}

QRectF NetworkNodeItem::boundingRect() const
{
    const qreal pad = 2.0;
    return QRectF(-pad, -pad, NNI_W + 2 * pad, NNI_H + 2 * pad);
}

void NetworkNodeItem::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget * /*widget*/)
{
    const bool selected = option->state & QStyle::State_Selected;

    QColor fill = transportColor(m_transport);
    QColor border = m_locked ? QColor(200, 60, 60)   // red border when locked
                             : fill.darker(160);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(border, selected ? 2.0 : 1.5));
    painter->setBrush(QBrush(fill));

    const qreal w = NNI_W;
    const qreal h = NNI_H;
    painter->drawRoundedRect(QRectF(0, 0, w, h), 4, 4);

    // Glyph + name. Counter-rotate so the text stays upright when the plot view is turned in
    // quarter steps -- see MonitorGraphicsView::setViewRotation().
    painter->setPen(QPen(Qt::white));
    QFont f("Arial", 9, QFont::Bold);
    painter->setFont(f);
    const double viewTurn = qRadiansToDegrees(qAtan2(painter->transform().m12(),
                                                     painter->transform().m11()));
    painter->save();
    if (qAbs(viewTurn) > 0.5)
    {
        const QPointF c(w / 2.0, h / 2.0);
        painter->translate(c);
        painter->rotate(-viewTurn);
        painter->translate(-c);
    }
    painter->drawText(QRectF(0, 0, h, h), Qt::AlignCenter, transportGlyph(m_transport));
    painter->drawText(QRectF(h, 0, w - h, h), Qt::AlignVCenter | Qt::AlignLeft, m_name);
    painter->restore();
}

void NetworkNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    emit itemDropped(this);
}

void NetworkNodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    QAction *lockAct = menu.addAction(
        m_locked ? tr("Unlock Network Node") : tr("Lock Network Node"));

    // The model (and persistence) lives in NetworkTopology, which the view
    // owns -- it toggles the flag and rebuilds this item with the new state.
    if (menu.exec(event->screenPos()) == lockAct)
        emit lockToggleRequested(m_index);
}

QPointF NetworkNodeItem::centre() const
{
    return mapToScene(QPointF(NNI_W / 2.0, NNI_H / 2.0));
}

/****************************************************************************
 * NetworkReceiverItem
 ****************************************************************************/

#define NRI_R 10.0

NetworkReceiverItem::NetworkReceiverItem(int nodeIndex, int portIndex, const QString &name,
                                         QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_node(nodeIndex)
    , m_port(portIndex)
    , m_name(name)
{
    setZValue(0.45);   // with the network nodes, under fixtures
    setToolTip(QObject::tr("Wireless receiver %1 (port %2) -- where this port's universe "
                           "reaches the rig. Drag it to where the box is rigged.")
                   .arg(name).arg(portIndex + 1));
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
}

QVariant NetworkReceiverItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged && onMoving)
        onMoving();
    return QGraphicsItem::itemChange(change, value);
}

void NetworkReceiverItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    if (onDropped)
        onDropped();
}

QRectF NetworkReceiverItem::boundingRect() const
{
    return QRectF(-NRI_R - 2, -NRI_R - 2, 2 * NRI_R + 4, 2 * NRI_R + 4);
}

void NetworkReceiverItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                QWidget * /*widget*/)
{
    const bool selected = option->state & QStyle::State_Selected;
    QColor fill = transportColor(NetworkNode::WirelessDMX);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(fill.darker(160), selected ? 2.0 : 1.5));
    painter->setBrush(QBrush(fill));
    painter->drawEllipse(QPointF(0, 0), NRI_R, NRI_R);

    // Counter-rotate so the label stays upright when the plot is turned.
    painter->setPen(QPen(Qt::white));
    painter->setFont(QFont("Arial", 8, QFont::Bold));
    const double viewTurn = qRadiansToDegrees(qAtan2(painter->transform().m12(),
                                                     painter->transform().m11()));
    painter->save();
    if (qAbs(viewTurn) > 0.5)
        painter->rotate(-viewTurn);
    painter->drawText(QRectF(-NRI_R, -NRI_R, 2 * NRI_R, 2 * NRI_R), Qt::AlignCenter,
                      QStringLiteral("RX"));
    painter->restore();
}

/****************************************************************************
 * NetworkRunItem
 ****************************************************************************/

NetworkRunItem::NetworkRunItem(const QPainterPath &path, const QColor &colour, const QString &stats,
                               int nodeIndex, int portIndex, QGraphicsItem *parent)
    : QGraphicsPathItem(path, parent)
    , m_node(nodeIndex)
    , m_port(portIndex)
{
    QPen pen(colour, 2.0);
    pen.setCosmetic(true);
    pen.setJoinStyle(Qt::RoundJoin);
    setPen(pen);
    setZValue(0.3);                 // under nodes, receivers and fixtures
    setToolTip(stats);
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(true);
}

QPainterPath NetworkRunItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(10.0);         // scene pixels: a band you can actually hit
    return stroker.createStroke(path());
}

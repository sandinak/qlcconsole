/*
  Q Light Controller Plus
  monitorimageitem.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QtMath>

#include "monitorimageitem.h"
#include "monitorproperties.h"
#include "doc.h"

static const qreal HANDLE = 9.0;   // corner handle size in scene px

MonitorImageItem::MonitorImageItem(Doc *doc, quint32 imageId, QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsItem(parent)
    , m_doc(doc)
    , m_id(imageId)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    // True backdrop: sit BEHIND every stage object (platforms -1, trusses -0.5,
    // fixtures 1-2, grid lines 1-2) so a background image doesn't cover the plot.
    setZValue(-2.0);
    const MonitorProperties::MonitorImage img = m_doc->monitorProperties()->image(m_id);
    setSource(img.source);
}

void MonitorImageItem::setSource(const QString &path)
{
    m_pixmap = path.isEmpty() ? QPixmap() : QPixmap(path);
    update();
}

void MonitorImageItem::setGeometry(float pxX, float pxY, float pxW, float pxH)
{
    prepareGeometryChange();
    m_pxW = qMax(1.0f, pxW);
    m_pxH = qMax(1.0f, pxH);
    setPos(pxX, pxY);
    // Rotate about the rectangle's centre so move/resize keep the centre stable.
    setTransformOriginPoint(m_pxW / 2.0, m_pxH / 2.0);
    update();
}

void MonitorImageItem::setImageRotation(float degrees)
{
    m_rotationDeg = degrees;
    setTransformOriginPoint(m_pxW / 2.0, m_pxH / 2.0);
    setRotation(degrees);
}

void MonitorImageItem::setMovable(bool movable)
{
    setFlag(ItemIsMovable, movable);
    setCursor(movable ? Qt::SizeAllCursor : Qt::ArrowCursor);
}

QRectF MonitorImageItem::boundingRect() const
{
    return QRectF(0, 0, m_pxW, m_pxH);
}

int MonitorImageItem::cornerAt(const QPointF &pt) const
{
    const QRectF r = boundingRect();
    if (QRectF(r.left(),           r.top(),            HANDLE, HANDLE).contains(pt)) return 1; // TL
    if (QRectF(r.right() - HANDLE, r.top(),            HANDLE, HANDLE).contains(pt)) return 2; // TR
    if (QRectF(r.left(),           r.bottom() - HANDLE, HANDLE, HANDLE).contains(pt)) return 3; // BL
    if (QRectF(r.right() - HANDLE, r.bottom() - HANDLE, HANDLE, HANDLE).contains(pt)) return 4; // BR
    return 0;
}

void MonitorImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                             QWidget *)
{
    const QRectF r = boundingRect();
    if (!m_pixmap.isNull())
        painter->drawPixmap(r.toRect(), m_pixmap);
    else
    {
        // No/broken image: an obvious placeholder so the object is still findable.
        painter->fillRect(r, QColor(60, 60, 74, 180));
        painter->setPen(QPen(QColor(180, 180, 200), 1, Qt::DashLine));
        painter->drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
        painter->drawText(r, Qt::AlignCenter, QObject::tr("(image not found)"));
    }

    const bool locked = m_doc->monitorProperties()->image(m_id).locked;
    const bool selected = option->state & QStyle::State_Selected;
    if (selected)
        painter->setPen(QPen(QColor(80, 170, 255), 2));
    else if (locked)
        painter->setPen(QPen(QColor(200, 60, 60), 1));
    else
        painter->setPen(QPen(QColor(90, 90, 100), 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));

    // Corner resize handles when selected (and not locked).
    if (selected && !locked && (flags() & ItemIsMovable))
    {
        painter->setPen(QPen(QColor(80, 170, 255), 1));
        painter->setBrush(QColor(20, 30, 45));
        const QPointF corners[4] = { r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight() };
        for (const QPointF &c : corners)
            painter->drawRect(QRectF(c.x() - (c.x() > r.center().x() ? HANDLE : 0),
                                     c.y() - (c.y() > r.center().y() ? HANDLE : 0),
                                     HANDLE, HANDLE));
    }
}

QVariant MonitorImageItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    return QGraphicsItem::itemChange(change, value);
}

void MonitorImageItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_resizeCorner = 0;
    if ((flags() & ItemIsMovable) && event->button() == Qt::LeftButton)
    {
        const int c = cornerAt(event->pos());
        if (c != 0)
        {
            m_resizeCorner = c;
            // Hold the CENTRE fixed while resizing (composes cleanly with the
            // rotation, which is also about the centre). With rotation about the
            // centre, the centre's scene pos = pos + (w/2, h/2).
            m_resizeCenterScene = pos() + QPointF(m_pxW / 2.0, m_pxH / 2.0);
            setSelected(true);
            event->accept();
            return;   // don't let the base handler start a move
        }
    }
    QGraphicsItem::mousePressEvent(event);
}

void MonitorImageItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_resizeCorner != 0)
    {
        // Project the cursor offset from the centre onto the image's own (rotated)
        // axes to get half-width/half-height; resize symmetrically about the centre.
        const qreal rad = qDegreesToRadians(qreal(m_rotationDeg));
        const QPointF d = event->scenePos() - m_resizeCenterScene;
        const qreal lx =  d.x() * qCos(rad) + d.y() * qSin(rad);
        const qreal ly = -d.x() * qSin(rad) + d.y() * qCos(rad);
        const qreal newW = qMax(qreal(2 * HANDLE), 2.0 * qAbs(lx));
        const qreal newH = qMax(qreal(2 * HANDLE), 2.0 * qAbs(ly));

        prepareGeometryChange();
        m_pxW = float(newW);
        m_pxH = float(newH);
        setPos(m_resizeCenterScene - QPointF(newW / 2.0, newH / 2.0));
        setTransformOriginPoint(newW / 2.0, newH / 2.0);   // keep rotation centred
        update();
        event->accept();
        return;
    }
    QGraphicsItem::mouseMoveEvent(event);
}

void MonitorImageItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    const bool wasResizing = (m_resizeCorner != 0);
    m_resizeCorner = 0;
    if (!wasResizing)
        QGraphicsItem::mouseReleaseEvent(event);
    emit itemDropped(this);
}

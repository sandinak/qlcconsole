/*
  Q Light Controller Plus
  platformitem.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QStyleOptionGraphicsItem>
#include <QTextDocument>
#include <QTextOption>
#include <QPainter>
#include <QFont>
#include <QMenu>

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
    // Respect a persisted per-platform lock: a locked platform can't be dragged.
    setFlag(ItemIsMovable, !platform->locked());
    setCursor(platform->locked() ? Qt::ArrowCursor : Qt::SizeAllCursor);
    setPos(pxX, pxY);

    bool isFeet = doc->monitorProperties()->gridUnits() == MonitorProperties::Feet;
    const double conv = isFeet ? 3.28084 : 1.0;
    const double dispW = double(platform->width())  * conv;
    const double dispD = double(platform->depth())  * conv;
    const double dispH = double(platform->height()) * conv;
    QString unitStr = isFeet ? "ft" : "m";
    // Full footprint × height so the size is unambiguous (was height only).
    m_label = new QGraphicsTextItem(
        QString("%1\n(%2×%3×%4 %5)").arg(platform->name())
            .arg(dispW, 0, 'f', 1).arg(dispD, 0, 'f', 1).arg(dispH, 0, 'f', 1).arg(unitStr),
        this);
    m_label->setDefaultTextColor(QColor(240, 240, 240, 220));
    m_label->setFont(QFont("Arial", 10, QFont::Bold));
    m_label->setZValue(0.1);

    // Center the label both horizontally and vertically within the platform.
    m_label->setTextWidth(m_pxW);
    QTextOption opt = m_label->document()->defaultTextOption();
    opt.setAlignment(Qt::AlignHCenter);
    m_label->document()->setDefaultTextOption(opt);
    const qreal th = m_label->boundingRect().height();
    m_label->setPos(0, qMax(qreal(0.0), (m_pxD - th) / 2.0));
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

void PlatformItem::showLabel(bool visible)
{
    if (m_label != nullptr)
        m_label->setVisible(visible);
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

    // A locked platform gets a red border so the frozen state reads at a glance.
    if (m_platform->locked())
        border = QColor(200, 60, 60, selected ? 255 : 200);

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
    // No-overlap collision: a SOLID, non-stackable platform can't be dragged to
    // overlap another solid, non-stackable platform — it slides up to the edge and
    // stops ("block at contact"). A stackable platform (a step/riser) is exempt, so
    // it may sit on top. Two items collide only when BOTH are solid and NEITHER is
    // stackable.
    if (change == ItemPositionChange && scene() != nullptr && m_platform != nullptr
        && m_platform->solid() && !m_platform->stackable())
    {
        // Skip collision during a GROUP drag (>1 platform selected): the group
        // moves rigidly, and clamping each member independently would scramble its
        // layout. Only single-item drags block at contact.
        int selPlatforms = 0;
        foreach (QGraphicsItem *gi, scene()->selectedItems())
            if (dynamic_cast<PlatformItem *>(gi) != nullptr && ++selPlatforms > 1)
                break;
        if (selPlatforms > 1)
            return QGraphicsItem::itemChange(change, value);

        const QSizeF sz(m_pxW, m_pxD);
        const QPointF cur = pos();
        const QPointF np  = value.toPointF();
        const QRectF  curRect(cur, sz);

        auto obstacleFor = [&](const QRectF &r) -> QRectF {
            foreach (QGraphicsItem *gi, scene()->items())
            {
                PlatformItem *o = dynamic_cast<PlatformItem *>(gi);
                if (o == nullptr || o == this || o->m_platform == nullptr)
                    continue;
                if (!o->m_platform->solid() || o->m_platform->stackable())
                    continue;                     // that one permits overlap
                if (o->isSelected())
                    continue;                     // moving together (group drag)
                const QRectF orect(o->pos(), QSizeF(o->m_pxW, o->m_pxD));
                if (curRect.intersects(orect))
                    continue;                     // ALREADY overlapping — grandfather it
                                                  // (don't fling the platform out of a
                                                  //  pre-existing overlap; only block NEW ones)
                if (r.intersects(orect))
                    return orect;
            }
            return QRectF();
        };

        // Resolve X then Y so the platform slides along a contacted edge.
        QPointF resolved(np.x(), cur.y());
        QRectF ob = obstacleFor(QRectF(resolved, sz));
        if (!ob.isNull())
            resolved.setX(np.x() > cur.x() ? ob.left() - sz.width() : ob.right());
        resolved.setY(np.y());
        ob = obstacleFor(QRectF(resolved, sz));
        if (!ob.isNull())
            resolved.setY(np.y() > cur.y() ? ob.top() - sz.height() : ob.bottom());
        return resolved;
    }
    return QGraphicsItem::itemChange(change, value);
}

void PlatformItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_dragMoved = false;   // a drag only counts once the pointer actually moves
    QGraphicsItem::mousePressEvent(event);
}

void PlatformItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    m_dragMoved = true;
    QGraphicsItem::mouseMoveEvent(event);
}

void PlatformItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    // Only a real drag persists/snaps — a bare click (or double-click to drill in)
    // must NOT nudge the platform to the grid.
    if (m_dragMoved)
        emit itemDropped(this);
}

void PlatformItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    QAction *lockAct = menu.addAction(
        m_platform->locked() ? tr("Unlock Platform") : tr("Lock Platform"));

    if (menu.exec(event->screenPos()) == lockAct)
    {
        m_platform->setLocked(!m_platform->locked());
        const bool canMove = !m_platform->locked();
        setFlag(ItemIsMovable, canMove);
        setCursor(canMove ? Qt::SizeAllCursor : Qt::ArrowCursor);
        update();
        if (m_doc)
            m_doc->setModified();
    }
}

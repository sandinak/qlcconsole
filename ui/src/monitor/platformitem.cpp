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
#include <QPainterPath>
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

    if (m_platform->isSteps())
    {
        paintSteps(painter, fill, border, selected);
        return;
    }

    painter->setPen(QPen(border, selected ? 2.0 : 1.5));
    painter->setBrush(QBrush(fill));
    if (m_view == 0 && m_platform->hasRoundedFront())
    {
        // Its outline, round the front; the hatching stays inside it.
        QPolygonF outline;
        const double w = double(m_platform->width()), d = double(m_platform->depth());
        foreach (const QPointF &q, m_platform->footprint(32))
            outline << QPointF((q.x() - double(m_platform->originX())) / w * m_pxW,
                               (q.y() - double(m_platform->originY())) / d * m_pxD);
        painter->drawPolygon(outline);
        QPainterPath clip;
        clip.addPolygon(outline);
        painter->setClipPath(clip);
    }
    else
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

void PlatformItem::paintSteps(QPainter *painter, const QColor &fill, const QColor &border,
                              bool selected)
{
    /* Local x runs with world X (plan, front) or world Y (side); local y runs
       with world Y in plan, and DOWN from the top of the flight in an
       elevation. +X is stage left, +Y downstage. */
    const int n = qMax(1, m_platform->stepCount());
    const StagePlatform::Climb climb = m_platform->stepsClimb();
    const bool alongX = (climb == StagePlatform::ClimbStageLeft || climb == StagePlatform::ClimbStageRight);
    const QPen edge(border, selected ? 2.0 : 1.5);
    QPen tread(border.lighter(140), 1.0);

    if (m_view == 0)
    {
        // Plan: the treads, and which way is up.
        painter->setPen(edge);
        painter->setBrush(QBrush(fill));
        painter->drawRect(QRectF(0, 0, m_pxW, m_pxD));
        painter->setPen(tread);
        for (int k = 1; k < n; ++k)
        {
            if (alongX)
                painter->drawLine(QPointF(m_pxW * k / n, 0), QPointF(m_pxW * k / n, m_pxD));
            else
                painter->drawLine(QPointF(0, m_pxD * k / n), QPointF(m_pxW, m_pxD * k / n));
        }
        QPointF from, to;      // bottom step's edge to the top step's
        const QPointF mid(m_pxW / 2.0, m_pxD / 2.0);
        switch (climb)
        {
        case StagePlatform::ClimbDownstage:  from = QPointF(mid.x(), 0);      to = QPointF(mid.x(), m_pxD); break;
        case StagePlatform::ClimbStageLeft:  from = QPointF(0, mid.y());      to = QPointF(m_pxW, mid.y()); break;
        case StagePlatform::ClimbStageRight: from = QPointF(m_pxW, mid.y());  to = QPointF(0, mid.y());     break;
        case StagePlatform::ClimbUpstage:
        default:                             from = QPointF(mid.x(), m_pxD);  to = QPointF(mid.x(), 0);     break;
        }
        const QPointF a = from + (to - from) * 0.15, b = from + (to - from) * 0.85;
        QLineF shaft(a, b);
        painter->setPen(QPen(border.lighter(160), 1.4));
        painter->drawLine(shaft);
        QLineF head1 = QLineF(b, a); head1.setLength(qMin(8.0, shaft.length() * 0.4));
        QLineF head2 = head1;
        head1.setAngle(head1.angle() + 28); head2.setAngle(head2.angle() - 28);
        painter->drawLine(head1);
        painter->drawLine(head2);
        QFont f = painter->font(); f.setPixelSize(9); painter->setFont(f);
        painter->drawText(QRectF(a.x() - 20, a.y() - 14, 40, 12), Qt::AlignCenter, tr("UP"));
        return;
    }

    // Elevation. Is the climb across the screen here, or toward the viewer?
    const bool profile = (m_view == 1) ? alongX : !alongX;
    if (!profile)
    {
        // Face-on: the risers, one above the other.
        painter->setPen(edge);
        painter->setBrush(QBrush(fill));
        painter->drawRect(QRectF(0, 0, m_pxW, m_pxD));
        painter->setPen(tread);
        for (int k = 1; k < n; ++k)
            painter->drawLine(QPointF(0, m_pxD * k / n), QPointF(m_pxW, m_pxD * k / n));
        return;
    }
    // Side-on: the staircase itself. Low end at local x = 0 when climbing
    // toward +X (stage left) in front, or toward +Y (downstage) in side.
    const bool lowAtStart = (m_view == 1) ? (climb == StagePlatform::ClimbStageLeft)
                                          : (climb == StagePlatform::ClimbDownstage);
    QPolygonF stairs;
    stairs << QPointF(0, m_pxD);
    for (int k = 0; k < n; ++k)
    {
        const int step = lowAtStart ? k : n - 1 - k;
        const double y = m_pxD - m_pxD * (step + 1) / n;
        stairs << QPointF(m_pxW * k / n, y) << QPointF(m_pxW * (k + 1) / n, y);
    }
    stairs << QPointF(m_pxW, m_pxD);
    painter->setPen(edge);
    painter->setBrush(QBrush(fill));
    painter->drawPolygon(stairs);
}

QVariant PlatformItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    // No-overlap collision: a SOLID, non-stackable platform can't be dragged to
    // overlap another solid, non-stackable platform — it slides up to the edge and
    // stops ("block at contact"). A stackable platform (a step/riser) is exempt, so
    // it may sit on top. Two items collide only when BOTH are solid and NEITHER is
    // stackable.
    MonitorProperties *mprops = m_doc ? m_doc->monitorProperties() : nullptr;
    // A platform PERMITS overlap when it's non-solid, explicitly Stackable, OR
    // AUTO-stackable — its base sits above the floor because it's on top of a
    // taller platform (a step/riser). So steps never trip the no-overlap rule
    // without the user marking anything.
    auto permitsOverlap = [mprops](StagePlatform *p) -> bool {
        if (p == nullptr) return true;
        if (!p->solid() || p->stackable()) return true;
        return mprops != nullptr && mprops->platformBaseZ(p->id()) > 0.001f;
    };

    if (change == ItemPositionChange && scene() != nullptr && !permitsOverlap(m_platform))
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

        auto obstacleFor = [&](const QRectF &r) -> QRectF {
            foreach (QGraphicsItem *gi, scene()->items())
            {
                PlatformItem *o = dynamic_cast<PlatformItem *>(gi);
                if (o == nullptr || o == this || o->m_platform == nullptr)
                    continue;
                if (permitsOverlap(o->m_platform))
                    continue;                     // that one permits overlap
                if (o->isSelected())
                    continue;                     // moving together (group drag)
                const QRectF orect(o->pos(), QSizeF(o->m_pxW, o->m_pxD));
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

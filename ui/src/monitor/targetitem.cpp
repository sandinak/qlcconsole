/*
  Q Light Controller Plus
  targetitem.cpp

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
#include <QPen>
#include <QFont>
#include <QMenu>

#include <QMessageBox>
#include <QGraphicsScene>
#include <cmath>
#include "monitorgraphicsview.h"
#include "qlcpalette.h"
#include "scene.h"
#include "monitorproperties.h"
#include "targetitem.h"
#include "stagetarget.h"
#include "humanfigure.h"
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
    // Respect a persisted per-target lock: a locked target can't be dragged.
    setFlag(ItemIsMovable, !target->locked());
    setCursor(target->locked() ? Qt::ArrowCursor : Qt::PointingHandCursor);
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

void TargetItem::setFootprintPx(const QSizeF &size)
{
    prepareGeometryChange();
    m_footprintPx = size;
    update();
}

QRectF TargetItem::footprintRect() const
{
    if (m_footprintPx.width() <= 0.0)
        return QRectF();
    /* A strip in an elevation: the ground it covers, seen edge on, given a few
       pixels of thickness so it can be seen at all. */
    const double h = qMax(6.0, m_footprintPx.height());
    return QRectF(-m_footprintPx.width() * 0.5, -h * 0.5, m_footprintPx.width(), h);
}

void TargetItem::setFigure(const QPointF &feet, const QPointF &crown,
                           double pxPerMetre, bool sideOn)
{
    prepareGeometryChange();
    m_feet = feet;
    m_crown = crown;
    m_pxPerMetre = pxPerMetre;
    m_sideOn = sideOn;
    update();
}

QPainterPath TargetItem::figurePath() const
{
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);    // overlapping parts, not holes
    if (m_target == nullptr || m_pxPerMetre <= 0.0
        || m_target->kind() != StageTarget::Kind::Person)
        return path;

    const QPointF up = m_crown - m_feet;
    const double tall = std::hypot(up.x(), up.y());
    if (tall < 4.0)
    {
        /* From above: shoulders across the stage, the head in the middle. A
           floor of a few pixels keeps it a person rather than a dot when the
           whole stage is on screen. */
        const double sw = qMax(12.0, 0.46 * m_pxPerMetre);
        const double sd = qMax(7.0, 0.26 * m_pxPerMetre);
        const double hd = qMax(6.0, 0.20 * m_pxPerMetre);
        path.addEllipse(m_feet, sw * 0.5, sd * 0.5);
        path.addEllipse(m_feet, hd * 0.5, hd * 0.5);
        return path;
    }

    /* Standing: the outline of the same body the rig view draws, each part
       as its left edge up and its right edge back down. From the side the
       body is seen through its depth, so the section's y stands in for x. */
    const QPointF u = up / tall;                  // toward the crown
    const QPointF v(-u.y(), u.x());               // across the body
    auto at = [&](double x, double y) {
        return m_feet + v * (x * tall) + u * (y * tall);
    };
    foreach (const FigureLoft &loft, humanFigureLofts())
    {
        QPolygonF outline;
        for (const FigureRing &r : loft)
        {
            const double c = m_sideOn ? r.cy : r.cx;
            const double w = m_sideOn ? r.ry : r.rx;
            outline << at(c - w, r.z);
        }
        for (int k = loft.size() - 1; k >= 0; --k)
        {
            const FigureRing &r = loft.at(k);
            const double c = m_sideOn ? r.cy : r.cx;
            const double w = m_sideOn ? r.ry : r.rx;
            outline << at(c + w, r.z);
        }
        outline << outline.first();
        path.addPolygon(outline);
    }
    // One outline for the whole body, not a stroke round every part: the
    // seams at the waist and neck read as a mannequin.
    return path.simplified();
}

QRectF TargetItem::boundingRect() const
{
    /* The crosshair arms already reach 1.4x the radius, and the aimed halo
       reaches three rings beyond it. The rect covers the largest of those
       unconditionally rather than changing with state: a bounding rect that
       shrinks when the glow turns off leaves the outer rings smeared on the
       canvas until something else forces a repaint. */
    const qreal pad = 3.0;
    const qreal r = kRadius + 3 * 4.0 + pad;
    QRectF rect(-r, -r, r * 2, r * 2);
    // Room for the "cannot reach" count under the mark, whether shown or not
    // (same reason as above: a rect that shrinks leaves the text smeared).
    rect |= QRectF(-kRadius - pad, kRadius, 90.0, 18.0);
    const QPainterPath fig = figurePath();
    if (fig.isEmpty() == false)
        rect |= fig.boundingRect().adjusted(-pad, -pad, pad, pad);
    const QRectF foot = footprintRect();
    if (foot.isValid())
        rect |= foot.adjusted(-pad, -pad, pad, pad);
    return rect;
}

void TargetItem::setReachShortfall(int unreachable, int aimed, const QStringList &names)
{
    if (m_unreachable == unreachable && m_aimedHeads == aimed)
        return;
    m_unreachable = unreachable;
    m_aimedHeads = aimed;
    setToolTip(unreachable > 0
               ? tr("%1 of %2 heads aimed here cannot reach it:\n%3")
                     .arg(unreachable).arg(aimed).arg(names.join(QStringLiteral(", ")))
               : QString());
    update();
}

void TargetItem::setAimed(bool aimed)
{
    if (m_aimed == aimed)
        return;
    m_aimed = aimed;
    update();
}

void TargetItem::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget * /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = option->state & QStyle::State_Selected;
    QColor col = m_target->color().isValid() ? m_target->color() : QColor(255, 180, 0);
    // A locked target is drawn red so the frozen state reads at a glance.
    if (m_target->locked())
        col = QColor(200, 60, 60);

    /* The ground it covers, under everything else: tinted so the stage under
       it still reads, outlined so its edge is exact. */
    const QRectF foot = footprintRect();
    if (foot.isValid())
    {
        QColor tint = col;
        tint.setAlpha(selected ? 70 : 45);
        QPen edge(col, selected ? 2.0 : 1.4);
        edge.setStyle(Qt::DashLine);
        painter->setPen(edge);
        painter->setBrush(tint);
        painter->drawRoundedRect(foot, 3.0, 3.0);
    }

    /* What stands here, under the mark: the mark still says where it is aimed,
       the figure says at whom. */
    const QPainterPath fig = figurePath();
    if (fig.isEmpty() == false)
    {
        QColor fill = col;
        fill.setAlpha(selected ? 150 : 100);
        painter->setPen(QPen(col, 1.2));
        painter->setBrush(fill);
        painter->drawPath(fig);
    }

    /* Glow for a target the focused scene actually aims at. Every target is
       drawn, because a target exists as soon as it is placed in the studio --
       but "something is pointing at this one right now" is the thing worth
       finding on a crowded stage, so it gets a halo rather than a different
       shape. Concentric fading rings: cheap, does not move the item's centre,
       and still legible against both the light and dark canvas. */
    if (m_aimed)
    {
        painter->setBrush(Qt::NoBrush);
        for (int i = 3; i >= 1; i--)
        {
            QColor halo = col;
            halo.setAlpha(30 + 20 * (3 - i));
            painter->setPen(QPen(halo, 3.0));
            painter->drawEllipse(QPointF(0, 0),
                                 double(kRadius) + i * 4.0,
                                 double(kRadius) + i * 4.0);
        }
    }

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

    /* Heads that cannot reach: a red ring and a count under the mark. Drawn
       whether or not the target is selected, because this is the one thing
       about a target you need to see without looking for it. */
    if (m_unreachable > 0)
    {
        const QColor warn(235, 70, 60);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(warn, 2.5));
        painter->drawEllipse(QPointF(0, 0), double(kRadius) + 2.0, double(kRadius) + 2.0);
        QFont f = painter->font();
        f.setBold(true);
        f.setPointSizeF(8.0);
        painter->setFont(f);
        painter->setPen(warn);
        painter->drawText(QRectF(-kRadius, kRadius + 1.0, 88.0, 16.0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("\u26A0 %1/%2").arg(m_unreachable).arg(m_aimedHeads));
    }

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

void TargetItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    QAction *lockAct = menu.addAction(
        m_target->locked() ? tr("Unlock Target") : tr("Lock Target"));
    menu.addSeparator();
    QAction *deleteAct = menu.addAction(tr("Delete Target…"));

    QAction *chosen = menu.exec(event->screenPos());
    if (chosen == NULL)
        return;

    if (chosen == lockAct)
    {
        m_target->setLocked(!m_target->locked());
        const bool canMove = !m_target->locked();
        setFlag(ItemIsMovable, canMove);
        setCursor(canMove ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
        if (m_doc)
            m_doc->setModified();
        return;
    }

    if (chosen == deleteAct)
        confirmAndDelete();
}

/** Delete this target, saying first what else it will take with it.
 *
 *  A target is not an isolated marker: Aim palettes point at it, and scenes
 *  use those palettes. Deleting one silently would leave those palettes aiming
 *  at an id that no longer resolves -- the lights keep whatever position they
 *  last had and nothing says why. So name the scenes before asking, because
 *  "2 scenes use this" is the fact that decides the answer.
 */
void TargetItem::confirmAndDelete()
{
    if (m_doc == NULL || m_target == NULL)
        return;

    MonitorProperties *props = m_doc->monitorProperties();
    if (props == NULL)
        return;

    const quint32 tid = m_target->id();
    const QString tname = m_target->name();

    // Palettes aiming here, and the scenes that use them.
    QList<quint32> boundPalettes;
    foreach (QLCPalette *pal, m_doc->palettes())
    {
        if (pal != NULL && pal->stageTargetId() == tid)
            boundPalettes << pal->id();
    }

    QStringList sceneNames;
    if (boundPalettes.isEmpty() == false)
    {
        foreach (Function *f, m_doc->functions())
        {
            Scene *sc = qobject_cast<Scene *>(f);
            if (sc == NULL)
                continue;
            foreach (quint32 pid, sc->palettes())
            {
                if (boundPalettes.contains(pid))
                { sceneNames << sc->name(); break; }
            }
        }
    }

    QString question = tr("Delete target \"%1\"?").arg(tname);
    if (sceneNames.isEmpty() == false)
    {
        question += tr("\n\n%n scene(s) aim at it: %1.\n"
                       "They will keep the Aim look but it will no longer "
                       "resolve, so those fixtures hold their last position.",
                       "", sceneNames.size()).arg(sceneNames.join(", "));
    }
    else if (boundPalettes.isEmpty() == false)
    {
        question += tr("\n\n%n Aim palette(s) point at it and will stop "
                       "resolving.", "", boundPalettes.size());
    }

    if (QMessageBox::question(nullptr, tr("Delete target"), question,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
        return;

    props->removeStageTarget(tid);
    m_doc->setModified();

    /* The view owns this item and rebuilds the whole set, so ask it to --
       deleting ourselves from inside our own event handler is not survivable. */
    if (MonitorGraphicsView *view =
            qobject_cast<MonitorGraphicsView *>(scene() ? scene()->views().value(0) : nullptr))
    {
        QMetaObject::invokeMethod(view, "updateTargets", Qt::QueuedConnection);
    }
}

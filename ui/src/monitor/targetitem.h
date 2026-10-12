/*
  Q Light Controller Plus
  targetitem.h

  Interactive 2-D representation of a StageTarget in the Monitor canvas.
  Renders as a crosshair/bullseye with the target name and Z height as a label.
  Movable when the layout is not locked.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef TARGETITEM_H
#define TARGETITEM_H

#include <QGraphicsItem>
#include <QPainterPath>
#include <QStringList>
#include <QObject>

class QGraphicsTextItem;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneContextMenuEvent;
class StageTarget;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

/**
 * Crosshair/bullseye marker for a StageTarget on the 2-D Monitor canvas.
 * Radius is fixed in pixels (scale-independent so it stays readable at any
 * zoom level).
 */
class TargetItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    /**
     * @param target  Engine StageTarget this item represents (not owned).
     * @param pxX     Scene-pixel X of the target floor projection.
     * @param pxY     Scene-pixel Y of the target floor projection.
     */
    TargetItem(StageTarget *target, Doc *doc, float pxX, float pxY,
               QGraphicsItem *parent = nullptr);

    quint32      targetId() const;
    StageTarget *target()   const { return m_target; }

    /** Move the item to the given scene-pixel position. */
    void setScenePos(float pxX, float pxY);

    /** Enable or disable movement (global layout lock). */
    void setMovable(bool movable);

    /** Mark this target as aimed by the scene currently in focus.
     *
     *  A target exists from the moment it is created in the studio, so it is
     *  always drawn. Being AIMED is a different and temporary fact -- this
     *  scene, right now, points lights at it -- and it earns the glow and the
     *  light-path lines. Drawing the two states identically left no way to see
     *  which targets the scene on screen actually uses. */
    void setAimed(bool aimed);
    bool isAimed() const { return m_aimed; }

    /** How many of the heads aimed here cannot reach it, of how many, and
     *  which. A target a scene points lights at that some of them cannot get
     *  to is the thing to fix before the show, not something to discover when
     *  a head sticks at its limit -- so it says so, on the target. 0 clears. */
    void setReachShortfall(int unreachable, int aimed, const QStringList &names);
    int unreachableCount() const { return m_unreachable; }

    /** Draw what the target STANDS FOR, not only where it is.
     *
     *  A Person is a figure: from above, a pair of shoulders and a head sized
     *  by @p pxPerMetre; in an elevation, a silhouette from @p feet up to
     *  @p crown, both in item coordinates. The footing is what the person
     *  stands on -- the deck under them -- which need not be the target's own
     *  Z. @p sideOn narrows the body to its depth. Every other kind keeps the
     *  plain mark, and so does a Person until this is called. */
    void setFigure(const QPointF &feet, const QPointF &crown, double pxPerMetre,
                   bool sideOn);
    /** The figure's outline in item coordinates; empty when there is none. */
    QPainterPath figurePath() const;

    /** The ground this target covers, in scene pixels, centred on the mark:
     *  width x depth from above, or a strip of width (height 0) in an
     *  elevation. An Area or a drum kit is not a point -- whether a beam
     *  covers it is the question -- so it is drawn at its real size. An empty
     *  size draws nothing. */
    void setFootprintPx(const QSizeF &size);
    QRectF footprintRect() const;

    // QGraphicsItem interface
    QRectF   boundingRect() const override;
    void     paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

public:
    /** Delete this target after naming the scenes that aim at it. */
    void confirmAndDelete();

private:

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

signals:
    void itemDropped(TargetItem *item);

private:
    static constexpr float kRadius = 14.0f;   // crosshair radius in scene px

    StageTarget       *m_target;
    Doc               *m_doc;
    QGraphicsTextItem *m_label;
    bool               m_aimed = false;
    int                m_unreachable = 0;
    int                m_aimedHeads = 0;

    QSizeF             m_footprintPx;
    QPointF            m_feet;
    QPointF            m_crown;
    double             m_pxPerMetre = 0.0;   ///< 0 = no figure set
    bool               m_sideOn = false;
};

/** @} */

#endif // TARGETITEM_H

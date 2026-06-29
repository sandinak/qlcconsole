/*
  Q Light Controller Plus
  powersourceitem.h

  Interactive 2-D marker for a PowerSource (distro) on the Monitor canvas — a
  small labelled box you drop and drag to record where the source physically
  sits on the deck. Movable when the layout is unlocked.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef POWERSOURCEITEM_H
#define POWERSOURCEITEM_H

#include <QGraphicsItem>
#include <QObject>
#include <QString>
#include <QColor>
#include <QList>

class QGraphicsSceneMouseEvent;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class PowerSourceItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    /** @param sourceIndex index into PowerDistribution::sources() (the model
     *         has no stable id; the view keeps items in sync by index). */
    PowerSourceItem(int sourceIndex, const QString &name,
                    QGraphicsItem *parent = nullptr);

    int sourceIndex() const { return m_index; }

    /** Enable/disable dragging (view calls this on lock change). */
    void setMovable(bool movable);

    /** Power-view overlay: the colours of this source's circuits (matching the
     *  fixture tints). Empty restores the plain amber marker. */
    void setCircuitColors(const QList<QColor> &colors);

    QRectF boundingRect() const override;
    void   paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

signals:
    /** Emitted when the user finishes a drag so the view can persist the move. */
    void itemDropped(PowerSourceItem *item);

private:
    int           m_index;
    QString       m_name;
    QList<QColor> m_circuitColors;   ///< power-view: this source's circuit colours
};

/** @} */

#endif // POWERSOURCEITEM_H

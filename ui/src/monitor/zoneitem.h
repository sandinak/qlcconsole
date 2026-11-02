/*
  Q Light Controller Plus - qlcconsole
  zoneitem.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef ZONEITEM_H
#define ZONEITEM_H

#include <QGraphicsItem>
#include <QPolygonF>
#include <QString>

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

/**
 * What the plot draws for the lighting-safety overlay: a no-light zone (a red
 * hatched outline with its name) or a laser's scan window (a dashed amber
 * outline where its H x V window lands). Both take no mouse buttons -- the
 * canvas under them stays clickable; MonitorGraphicsView finds a zone by
 * hit-testing (zoneAt()) for double-click and right-click. A zone is dragged
 * by its outline or its centre handle only (grabAt()), never by its inside,
 * so the plot under it stays selectable.
 */
class ZoneItem final : public QGraphicsItem
{
public:
    enum Role { Zone, ScanWindow };

    /** @p outline in scene pixels; @p id the zone's id (or the laser's fixture id). */
    ZoneItem(Role role, quint32 id, const QPolygonF &outline, const QString &label,
             QGraphicsItem *parent = nullptr);

    Role role() const { return m_role; }
    quint32 id() const { return m_id; }
    QPolygonF outline() const { return m_outline; }
    bool contains(const QPointF &scenePos) const;
    /** Whether @a scenePos is on the zone's outline (within @a tol pixels) or
     *  on its centre handle: where a drag takes hold. Only a Zone grabs. */
    bool grabAt(const QPointF &scenePos, qreal tol) const;
    /** The centre handle's rectangle, in scene pixels. */
    QRectF handleRect() const;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

private:
    Role m_role;
    quint32 m_id;
    QPolygonF m_outline;
    QString m_label;
};

/** @} */

#endif

/*
  Q Light Controller Plus
  trussitem.h

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

#ifndef TRUSSITEM_H
#define TRUSSITEM_H

#include <QGraphicsItem>
#include <QObject>

class QGraphicsTextItem;
class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneMouseEvent;
class Doc;
class Truss;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

/**
 * Interactive 2-D representation of a Truss in the Monitor canvas.
 *
 * Horizontal trusses draw a Warren-truss pattern (two chords + diagonal
 * bracing) in silver.  Vertical towers draw a bullseye (circle + crosshairs).
 *
 * The item is movable when neither globally locked nor individually locked.
 * Right-click → context menu to toggle the per-truss lock.
 */
class TrussItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    /**
     * @param truss   Engine Truss this item represents (not owned).
     * @param doc     Engine Doc used to mark the workspace modified.
     * @param pxLen   Rendered length in scene pixels.
     * @param pxWid   Rendered width in scene pixels (≥ 8).
     */
    TrussItem(Truss *truss, Doc *doc, float pxLen, float pxWid,
              QGraphicsItem *parent = nullptr);

    quint32 trussId() const;
    Truss  *truss()   const { return m_truss; }

    /** Enable or disable movement without touching the per-truss lock flag.
     *  Called by MonitorGraphicsView when the global layout lock changes. */
    void setMovable(bool movable);

    /** Rendered pixel width of this truss (used for escape-from-truss threshold). */
    float pxWid() const { return m_pxWid; }

    /** Highlight the truss (e.g. while a fixture is dragged over it). */
    void setHighlighted(bool highlighted);
    bool isHighlighted() const { return m_highlighted; }

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change,
                        const QVariant &value) override;

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

signals:
    /** Emitted when the user finishes a drag so the view can persist the move. */
    void itemDropped(TrussItem *item);

    /** Emitted when the user right-clicks the truss and chooses to add a fixture
     *  at a specific offset (metres from the truss origin) along its length. */
    void addFixtureRequested(quint32 trussId, float offsetMetres);

private:
    Truss              *m_truss;
    Doc                *m_doc;
    float               m_pxLen;
    float               m_pxWid;
    QGraphicsTextItem  *m_label;       ///< child item — moves & rotates with this item
    bool                m_highlighted = false;
};

/** @} */

#endif // TRUSSITEM_H

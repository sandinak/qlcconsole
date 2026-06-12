/*
  Q Light Controller Plus
  trussitem.cpp

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

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QMenu>
#include <QFont>
#include <QtMath>

#include <QCursor>
#include <QPixmap>

#include "trussitem.h"

static QCursor smallOpenHandCursor()
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    const QColor fg(255, 255, 255, 230);
    const QColor bg(0, 0, 0, 200);
    auto finger = [&](int x, int y, int w, int h) {
        p.setPen(QPen(bg, 1));
        p.setBrush(fg);
        p.drawRoundedRect(x, y, w, h, 1, 1);
    };
    finger(2, 2, 2, 6);
    finger(5, 0, 2, 7);
    finger(8, 1, 2, 6);
    finger(11, 2, 2, 5);
    finger(0, 7, 2, 5);
    p.setPen(QPen(bg, 1));
    p.setBrush(fg);
    p.drawRoundedRect(0, 8, 13, 6, 2, 2);
    p.end();
    return QCursor(pm, 5, 0);
}
#include "truss.h"
#include "doc.h"

TrussItem::TrussItem(Truss *truss, Doc *doc, float pxLen, float pxWid,
                     QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsItem(parent)
    , m_truss(truss)
    , m_doc(doc)
    , m_pxLen(pxLen)
    , m_pxWid(qMax(pxWid, 8.0f))
    , m_label(nullptr)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(-0.5);
    setCursor(m_truss->locked() ? Qt::ArrowCursor : smallOpenHandCursor());

    // Label is a child — it rotates with the item, which is acceptable for
    // trusses that run at non-trivial angles.
    m_label = new QGraphicsTextItem(
        QString("%1\n(Z: %2m)").arg(truss->name()).arg(truss->origin().z(), 0, 'f', 1),
        this);
    m_label->setDefaultTextColor(QColor(40, 40, 40));
    m_label->setFont(QFont("Arial", 7));
    m_label->setZValue(0.1);

    if (truss->type() == Truss::Vertical)
        m_label->setPos(m_pxWid / 2.0 + 2, -m_pxWid / 2.0);
    else
        m_label->setPos(2, -(m_pxWid / 2.0 + m_label->boundingRect().height()));

    if (m_truss->locked())
        setFlag(ItemIsMovable, false);
}

quint32 TrussItem::trussId() const
{
    return m_truss->id();
}

void TrussItem::setMovable(bool movable)
{
    // Respect per-truss lock even when global lock is released.
    bool canMove = movable && !m_truss->locked();
    setFlag(ItemIsMovable, canMove);
    setCursor(canMove ? smallOpenHandCursor() : Qt::ArrowCursor);
}

void TrussItem::setHighlighted(bool highlighted)
{
    if (m_highlighted != highlighted)
    {
        m_highlighted = highlighted;
        update();
    }
}

QRectF TrussItem::boundingRect() const
{
    const qreal pad = 2.0;
    if (m_truss->type() == Truss::Vertical)
    {
        qreal r = m_pxWid / 2.0;
        return QRectF(-r - pad, -r - pad, m_pxWid + 2*pad, m_pxWid + 2*pad);
    }
    return QRectF(-pad, -m_pxWid / 2.0 - pad, m_pxLen + 2*pad, m_pxWid + 2*pad);
}

void TrussItem::paint(QPainter *painter,
                      const QStyleOptionGraphicsItem *option,
                      QWidget * /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = option->state & QStyle::State_Selected;
    const bool locked   = m_truss->locked();

    // Silver palette — nearly transparent fill so the background shows through,
    // with solid chord and web lines for structural clarity.
    const QColor chordFill = locked ? QColor(200, 60, 60, 70)   // red tint when locked
                                    : QColor(210, 213, 220, 50); // light silver normally
    const QColor chordLine = locked ? QColor(200, 80, 80)
                                    : QColor(160, 163, 172);
    const QColor diagLine (140, 143, 152);        // medium silver diagonals
    const QColor selColor (255, 180,   0);        // amber when selected
    const QColor lockColor(200,  60,  60);        // red tint when locked

    QPen outerPen(selected ? selColor : (locked ? lockColor : chordLine),
                  selected ? 2.0 : 1.5);
    QPen innerPen(diagLine, 1.0);
    innerPen.setStyle(Qt::SolidLine);

    if (m_truss->type() == Truss::Vertical)
    {
        // Tower footprint: filled circle with crosshairs
        qreal r = m_pxWid / 2.0;
        QBrush circleFill = m_highlighted ? QBrush(QColor(0, 120, 255, 80)) : QBrush(chordFill);
        QPen  circlePen   = m_highlighted ? QPen(QColor(0, 160, 255, 200), 3.0) : outerPen;
        painter->setPen(circlePen);
        painter->setBrush(circleFill);
        painter->drawEllipse(QPointF(0, 0), r, r);
        painter->setPen(innerPen);
        painter->drawLine(QPointF(-r * 0.8, 0), QPointF(r * 0.8, 0));
        painter->drawLine(QPointF(0, -r * 0.8), QPointF(0, r * 0.8));
        return;
    }

    // ---- Horizontal / Ground: Warren truss pattern ----
    //
    // Local coordinate system:
    //   X = 0 .. m_pxLen (along truss direction)
    //   Y = -m_pxWid/2 .. +m_pxWid/2
    //
    const double top    = -m_pxWid / 2.0;
    const double bot    =  m_pxWid / 2.0;
    const double L      =  m_pxLen;

    // Semi-transparent fill between the chords
    painter->setPen(Qt::NoPen);
    painter->setBrush(QBrush(chordFill));
    painter->drawRect(QRectF(0, top, L, m_pxWid));

    // Web spacing: at least 1 cell, at most 30px
    double webSpacing = qBound(6.0, double(m_pxWid) * 2.0, 30.0);
    int nCells = qMax(1, int(qRound(L / webSpacing)));
    webSpacing = L / nCells;

    // Diagonal bracing (Warren / N pattern)
    painter->setPen(innerPen);
    for (int i = 0; i < nCells; ++i)
    {
        double x0 = i * webSpacing;
        double x1 = (i + 1) * webSpacing;
        if (i % 2 == 0)
            painter->drawLine(QPointF(x0, top), QPointF(x1, bot));
        else
            painter->drawLine(QPointF(x0, bot), QPointF(x1, top));
    }

    // Vertical web members at each node
    QPen webPen(chordLine.lighter(160), 0.8);
    painter->setPen(webPen);
    for (int i = 1; i < nCells; ++i)
    {
        double x = i * webSpacing;
        painter->drawLine(QPointF(x, top), QPointF(x, bot));
    }

    // Chord lines on top
    painter->setPen(outerPen);
    painter->drawLine(QPointF(0, top),  QPointF(L, top));  // top chord
    painter->drawLine(QPointF(0, bot),  QPointF(L, bot));  // bottom chord
    painter->drawLine(QPointF(0, top),  QPointF(0, bot));  // left end cap
    painter->drawLine(QPointF(L, top),  QPointF(L, bot));  // right end cap

    // Drop-target highlight: bright blue glow when a fixture hovers over this truss
    if (m_highlighted)
    {
        QPen glowPen(QColor(0, 160, 255, 200), 3.0);
        painter->setPen(glowPen);
        painter->setBrush(QBrush(QColor(0, 120, 255, 40)));
        painter->drawRect(QRectF(0, top, L, m_pxWid));
    }
}

QVariant TrussItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged && scene())
    {
        // Update the Truss engine object with the new scene-space origin.
        // MonitorGraphicsView::slotTrussMoved is responsible for setting
        // doc modified once the drag is complete.
        QPointF sp = value.toPointF();
        // We need the inverse of realPositionToPixels — the view exposes
        // pixelsToRealPosition() for this purpose.
        // The truss Z stays unchanged; only XY origin tracks the drag.
        // The conversion is done in slotTrussMoved after drop to avoid
        // accessing the view's private pixel metrics here.
        // We store the scene position so slotTrussMoved can read it.
        Q_UNUSED(sp)
    }
    return QGraphicsItem::itemChange(change, value);
}

void TrussItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    emit itemDropped(this);
}

void TrussItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;

    // "Add Fixture Here" — compute the offset along the truss at the click point.
    QPointF delta = event->scenePos() - pos();
    float offsetM = 0.0f;
    if (m_truss->type() != Truss::Vertical && m_pxLen > 0.0f)
    {
        float angleRad = float(qDegreesToRadians(double(rotation())));
        QPointF dir(qCos(angleRad), qSin(angleRad));
        float pxOffset = float(delta.x() * dir.x() + delta.y() * dir.y());
        pxOffset = qBound(0.0f, pxOffset, m_pxLen);
        offsetM  = pxOffset * m_truss->length() / m_pxLen;
    }
    QAction *addFixAct = menu.addAction(tr("Add Fixture to Truss here…"));
    menu.addSeparator();
    QAction *lockAct = menu.addAction(
        m_truss->locked() ? tr("Unlock Truss") : tr("Lock Truss"));

    QAction *chosen = menu.exec(event->screenPos());
    if (chosen == addFixAct)
    {
        emit addFixtureRequested(m_truss->id(), offsetM);
    }
    else if (chosen == lockAct)
    {
        m_truss->setLocked(!m_truss->locked());
        bool canMove = !m_truss->locked();
        setFlag(ItemIsMovable, canMove);
        setCursor(canMove ? smallOpenHandCursor() : Qt::ArrowCursor);
        update();
        if (m_doc)
            m_doc->setModified();
    }
}

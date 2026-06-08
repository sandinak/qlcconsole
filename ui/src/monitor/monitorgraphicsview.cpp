/*
  Q Light Controller Plus
  monitorgraphicsview.cpp

  Copyright (C) Massimo Callegari

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

#include <QWheelEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QShortcut>

#include "monitorproperties.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "qlcfixturemode.h"
#include "doc.h"

MonitorGraphicsView::MonitorGraphicsView(Doc *doc, QWidget *parent)
    : QGraphicsView(parent)
    , m_doc(doc)
    , m_unitValue(1000)
    , m_gridEnabled(true)
    , m_gridSubdivisions(1)
    , m_layoutLocked(false)
    , m_snapDivisions(0)
    , m_bgItem(NULL)
{
    m_scene = new QGraphicsScene();
    m_scene->setSceneRect(this->rect());
    setScene(m_scene);

    // dragging on the empty canvas rubber-band selects fixtures.
    // This works both when locked and unlocked.
    setDragMode(QGraphicsView::RubberBandDrag);
    // Shift+wheel zoom centres on the cursor.
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    // Cmd/Ctrl+Z undoes the last fixture move.
    QShortcut *undoSc = new QShortcut(QKeySequence::Undo, this);
    undoSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoSc, &QShortcut::activated, this, &MonitorGraphicsView::undoLastMove);

    m_gridSize = QSize(5, 5);

    updateGrid();
}

void MonitorGraphicsView::wheelEvent(QWheelEvent *event)
{
    // Zoom only with Shift held (centred on the cursor); otherwise let the
    // view scroll normally.
    if ((event->modifiers() & Qt::ShiftModifier) == 0)
    {
        QGraphicsView::wheelEvent(event);
        return;
    }
    const double stepFactor = 1.15;
    const double cur = transform().m11(); // current uniform scale
    // With Shift held, macOS reports the wheel delta on the X axis, so fall
    // back to it when Y is zero.
    int delta = event->angleDelta().y();
    if (delta == 0)
        delta = event->angleDelta().x();
    double target = cur * ((delta > 0) ? stepFactor : 1.0 / stepFactor);
    if (target < 0.25) target = 0.25;
    if (target > 6.0)  target = 6.0;
    if (cur > 0.0 && target != cur)
        scale(target / cur, target / cur);
    event->accept();
}

bool MonitorGraphicsView::viewportEvent(QEvent *event)
{
    // Trackpad pinch: macOS sends a native zoom gesture (no Shift needed),
    // centred on the cursor via AnchorUnderMouse.
    if (event->type() == QEvent::NativeGesture)
    {
        QNativeGestureEvent *g = static_cast<QNativeGestureEvent *>(event);
        if (g->gestureType() == Qt::ZoomNativeGesture)
        {
            const double cur = transform().m11();
            double target = cur * (1.0 + g->value());
            if (target < 0.25) target = 0.25;
            if (target > 6.0)  target = 6.0;
            if (cur > 0.0 && target != cur)
                scale(target / cur, target / cur);
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}

void MonitorGraphicsView::mousePressEvent(QMouseEvent *event)
{
    // Shift + left-drag on an empty spot pans the view; otherwise keep the
    // normal rubber-band / item handling.
    if (event->button() == Qt::LeftButton
        && (event->modifiers() & Qt::ShiftModifier)
        && itemAt(event->pos()) == NULL)
    {
        setDragMode(QGraphicsView::ScrollHandDrag);
        QGraphicsView::mousePressEvent(event);
        return;
    }

    // A left-press on a fixture may begin a move: snapshot current positions so
    // the drop can be undone.
    if (event->button() == Qt::LeftButton && itemAt(event->pos()) != NULL)
        captureMoveUndo();

    QGraphicsView::mousePressEvent(event);
}

MonitorGraphicsView::~MonitorGraphicsView()
{
    clearFixtures();
}

void MonitorGraphicsView::setGridSize(QSize size)
{
    m_gridSize = size;
    updateGrid();
    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        updateFixture(it.key());
    }
    // cell pixels / offsets may have changed: refresh snapping
    applySnapToAllItems();
}

void MonitorGraphicsView::setGridSubdivisions(int subdivisions)
{
    if (subdivisions < 1)
        subdivisions = 1;
    m_gridSubdivisions = subdivisions;
    updateGrid();
}

void MonitorGraphicsView::setLayoutLocked(bool locked)
{
    m_layoutLocked = locked;
    foreach (MonitorFixtureItem *item, m_fixtures)
        item->setMovable(!locked);
}

void MonitorGraphicsView::setSnapDivisions(int divisions)
{
    if (divisions < 0)
        divisions = 0;
    m_snapDivisions = divisions;
    applySnapToAllItems();
}

void MonitorGraphicsView::applySnapToItem(MonitorFixtureItem *item)
{
    if (item != NULL)
        item->setSnap(m_snapDivisions, m_cellPixels, m_xOffset, m_yOffset);
}

void MonitorGraphicsView::applySnapToAllItems()
{
    foreach (MonitorFixtureItem *item, m_fixtures)
        applySnapToItem(item);
}

void MonitorGraphicsView::setGridMetrics(float value)
{
    m_unitValue = value;
    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        updateFixture(it.key());
    }
}

quint32 MonitorGraphicsView::selectedFixtureID()
{
    MonitorFixtureItem *item = getSelectedItem();
    if (item != NULL)
        return item->fixtureID();
    else
        return Fixture::invalidId();
}

QList<quint32> MonitorGraphicsView::fixturesID() const
{
    return m_fixtures.keys();
}

void MonitorGraphicsView::setFixtureGelColor(quint32 id, QColor col)
{
    MonitorFixtureItem *item = m_fixtures[id];
    if (item != NULL)
        item->setGelColor(col);
}

void MonitorGraphicsView::setFixtureRotation(quint32 id, ushort degrees)
{
    MonitorFixtureItem *item = m_fixtures[id];
    if (item != NULL)
        item->setRotation(degrees);
}

void MonitorGraphicsView::showFixturesLabels(bool visible)
{
    foreach (MonitorFixtureItem *item, m_fixtures)
        item->showLabel(visible);
}

QColor MonitorGraphicsView::fixtureGelColor(quint32 id)
{
    MonitorFixtureItem *item = m_fixtures[id];
    if (item == NULL)
        return QColor();

    return item->getColor();
}

QPointF MonitorGraphicsView::realPositionToPixels(qreal xpos, qreal ypos)
{
    qreal realX = m_xOffset + ((xpos * m_cellPixels) / m_unitValue);
    qreal realY = m_yOffset + ((ypos * m_cellPixels) / m_unitValue);

    return QPointF(realX, realY);
}

void MonitorGraphicsView::updateFixture(quint32 id)
{
    Fixture *fxi = m_doc->fixture(id);
    if (fxi == NULL || m_fixtures.contains(id) == false)
        return;

    const QLCFixtureMode *mode = fxi->fixtureMode();
    double width = 0;
    double height = 0;

    if (mode != 0)
    {
        width = mode->physical().width();
        height = mode->physical().height();
    }

    if (width <= 0 || height <= 0)
    {
        // No physical layout declared: most multi-head fixtures like this are
        // linear (LED tape / battens / pixel bars), so assume ONE ROW instead
        // of a square (which would make the heads wrap into a block). A very
        // wide:thin aspect makes setSize() lay the heads out in a single row.
        const int n = qMax(1, fxi->heads());
        width = 300.0;
        height = (n > 1) ? 300.0 / n : 300.0;
    }

    MonitorFixtureItem *item = m_fixtures[id];

    // On-screen size from the physical dimensions, keeping the TRUE aspect
    // ratio (so a thin tape stays thin and its heads lay out in the right
    // number of rows). Only enforce a minimum AREA — scaling both dimensions
    // by the same factor — so very small fixtures stay visible without being
    // squashed toward square (which used to make pixel arrays wrap). Zoom
    // provides per-LED detail.
    double pw = (double(width)  * m_cellPixels) / m_unitValue;
    double ph = (double(height) * m_cellPixels) / m_unitValue;
    const double kMinArea = 16.0 * 16.0;
    const double area = pw * ph;
    if (area > 0.0 && area < kMinArea)
    {
        const double s = sqrt(kMinArea / area);
        pw *= s;
        ph *= s;
    }
    item->setSize(QSize(qMax(1, qRound(pw)), qMax(1, qRound(ph))));

    item->setPos(realPositionToPixels(item->realPosition().x(), item->realPosition().y()));
}

void MonitorGraphicsView::setBackgroundImage(QString filename)
{
    m_backgroundImage = filename;
    if (m_bgItem != NULL)
    {
        m_scene->removeItem(m_bgItem);
        delete m_bgItem;
        m_bgItem = NULL;
    }
    if (filename.isEmpty() == false)
    {
        m_bgPixmap = QPixmap(m_backgroundImage);
        m_bgItem = new QGraphicsPixmapItem(m_bgPixmap);
        m_bgItem->setZValue(0); // make sure it goes on the bacground
        m_scene->addItem(m_bgItem);
    }
    updateGrid();
}

MonitorFixtureItem *MonitorGraphicsView::getSelectedItem()
{
    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        MonitorFixtureItem *item = it.value();
        if (item->isSelected() == true)
            return item;
    }
    return NULL;
}

void MonitorGraphicsView::addFixture(quint32 id, QPointF pos)
{
    if (id == Fixture::invalidId() || m_fixtures.contains(id) == true)
        return;

    if (m_doc->fixture(id) == NULL)
        return;

    MonitorFixtureItem *item = new MonitorFixtureItem(m_doc, id);
    item->setZValue(2);
    item->setRealPosition(pos);
    item->setMovable(!m_layoutLocked);
    m_fixtures[id] = item;
    m_scene->addItem(item);
    updateFixture(id);
    applySnapToItem(item);
    connect(item, SIGNAL(itemDropped(MonitorFixtureItem*)),
            this, SLOT(slotFixtureMoved(MonitorFixtureItem*)));
}

bool MonitorGraphicsView::removeFixture(quint32 id)
{
    MonitorFixtureItem *item = NULL;

    if (id == Fixture::invalidId())
    {
        item = getSelectedItem();
        if (item != NULL)
            id = item->fixtureID();
    }
    else
        item = m_fixtures[id];

    if (item == NULL)
        return false;

    m_scene->removeItem(item);
    m_fixtures.take(id);
    m_doc->monitorProperties()->removeFixture(id);
    delete item;

    return true;
}

void MonitorGraphicsView::clearFixtures()
{
    foreach (MonitorFixtureItem *item, m_fixtures)
        delete item;
    m_fixtures.clear();
}

void MonitorGraphicsView::updateGrid()
{
    int itemsCount = m_gridItems.count();
    for (int i = 0; i < itemsCount; i++)
        m_scene->removeItem((QGraphicsItem *)m_gridItems.takeLast());

    if (m_gridEnabled == true)
    {
        m_xOffset = 0;
        m_yOffset = 0;
        int xInc = this->width() / m_gridSize.width();
        int yInc = this->height() / m_gridSize.height();
        if (yInc < xInc)
        {
            m_cellPixels = yInc;
            m_xOffset = (this->width() - (m_cellPixels * m_gridSize.width())) / 2;
        }
        else if (xInc < yInc)
        {
            m_cellPixels = xInc;
            m_yOffset = (this->height() - (m_cellPixels * m_gridSize.height())) / 2;
        }
        int xPos = m_xOffset;
        int yPos = m_yOffset;

        QPen mainPen(QColor(40, 40, 40, 255));
        QPen subPen(QColor(30, 30, 30, 255));
        subPen.setStyle(Qt::DotLine);

        // draw sub-division lines first (so the main lines sit on top)
        if (m_gridSubdivisions > 1)
        {
            qreal subInc = (qreal)m_cellPixels / m_gridSubdivisions;
            int bottom = this->height() - m_yOffset;
            int right = this->width() - m_xOffset;

            for (int i = 0; i < m_gridSize.width(); i++)
            {
                for (int s = 1; s < m_gridSubdivisions; s++)
                {
                    qreal sx = m_xOffset + i * m_cellPixels + s * subInc;
                    QGraphicsLineItem *item = m_scene->addLine(sx, m_yOffset, sx, bottom, subPen);
                    item->setZValue(1);
                    m_gridItems.append(item);
                }
            }

            for (int i = 0; i < m_gridSize.height(); i++)
            {
                for (int s = 1; s < m_gridSubdivisions; s++)
                {
                    qreal sy = m_yOffset + i * m_cellPixels + s * subInc;
                    QGraphicsLineItem *item = m_scene->addLine(m_xOffset, sy, right, sy, subPen);
                    item->setZValue(1);
                    m_gridItems.append(item);
                }
            }
        }

        for (int i = 0; i < m_gridSize.width() + 1; i++)
        {
            QGraphicsLineItem *item = m_scene->addLine(xPos, m_yOffset, xPos, this->height() - m_yOffset,
                                                       mainPen);
            item->setZValue(1);
            xPos += m_cellPixels;
            m_gridItems.append(item);
        }

        for (int i = 0; i < m_gridSize.height() + 1; i++)
        {
            QGraphicsLineItem *item = m_scene->addLine(m_xOffset, yPos, this->width() - m_xOffset, yPos,
                                                       mainPen);
            item->setZValue(1);
            yPos += m_cellPixels;
            m_gridItems.append(item);
        }

        // Highlight the centre vertical & horizontal axes so the stage centre
        // is obvious.
        QPen centerPen(QColor(0, 150, 140, 220)); // teal accent
        centerPen.setWidth(1);
        const qreal gridW = m_gridSize.width()  * m_cellPixels;
        const qreal gridH = m_gridSize.height() * m_cellPixels;
        const qreal cx = m_xOffset + gridW / 2.0;
        const qreal cy = m_yOffset + gridH / 2.0;
        QGraphicsLineItem *cv = m_scene->addLine(cx, m_yOffset, cx, m_yOffset + gridH, centerPen);
        cv->setZValue(2);
        m_gridItems.append(cv);
        QGraphicsLineItem *chz = m_scene->addLine(m_xOffset, cy, m_xOffset + gridW, cy, centerPen);
        chz->setZValue(2);
        m_gridItems.append(chz);

        if (m_bgItem != NULL)
        {
            m_bgItem->setX(m_xOffset);
            m_bgItem->setY(m_yOffset);
            m_bgItem->setPixmap(m_bgPixmap.scaled(xPos - m_cellPixels - m_xOffset, yPos - m_cellPixels - m_yOffset));
        }
    }

    // Match the scene rect to the viewport so that, once zoomed in, the view
    // has scroll range — which is what makes wheel-zoom anchor under the cursor
    // and Shift-drag panning work.
    if (width() > 0 && height() > 0)
        m_scene->setSceneRect(0, 0, this->width(), this->height());
}

void MonitorGraphicsView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    updateGrid();
    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        updateFixture(it.key());
    }
    applySnapToAllItems();
}

void MonitorGraphicsView::mouseReleaseEvent(QMouseEvent *e)
{
    emit viewClicked(e);

    QGraphicsView::mouseReleaseEvent(e);

    // Restore rubber-band selection after a shift-drag pan.
    if (dragMode() == QGraphicsView::ScrollHandDrag)
        setDragMode(QGraphicsView::RubberBandDrag);
}

void MonitorGraphicsView::slotFixtureMoved(MonitorFixtureItem *item)
{
    // When a multi-selection is dragged, Qt moves all the selected
    // items together. Persist the new position of every selected item,
    // not just the one that received the drop event.
    QList<MonitorFixtureItem *> movedItems;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        MonitorFixtureItem *mfi = dynamic_cast<MonitorFixtureItem *>(gi);
        if (mfi != NULL && m_fixtures.key(mfi, Fixture::invalidId()) != Fixture::invalidId())
            movedItems.append(mfi);
    }

    // make sure the dropped item is always handled even if it
    // somehow ended up unselected
    if (movedItems.contains(item) == false && item != NULL)
        movedItems.append(item);

    // Snap the move as a GROUP: snap the dragged item to the grid, then shift
    // every moved item by the same delta. This keeps the selection's relative
    // layout intact (snapping each item independently would scramble it).
    if (m_snapDivisions > 0 && m_cellPixels > 0 && item != NULL)
    {
        const QPointF snapped = snapScenePos(item->pos());
        const QPointF delta = snapped - item->pos();
        if (delta.isNull() == false)
            foreach (MonitorFixtureItem *mfi, movedItems)
                mfi->setPos(mfi->pos() + delta);
    }

    foreach (MonitorFixtureItem *mfi, movedItems)
    {
        quint32 fid = m_fixtures.key(mfi);

        // Convert the pixel position of the fixture into
        // position in millimeters
        QPointF mmPos;
        mmPos.setX(((mfi->x() - m_xOffset) * m_unitValue) / m_cellPixels);
        mmPos.setY(((mfi->y() - m_yOffset) * m_unitValue) / m_cellPixels);

        // update the fixture item's real position
        mfi->setRealPosition(mmPos);

        emit fixtureMoved(fid, mmPos);
    }

    // Commit the undo snapshot if anything actually moved.
    if (m_pendingMoveUndo.isEmpty() == false)
    {
        QHash<quint32, QPointF> changed;
        QHashIterator<quint32, QPointF> it(m_pendingMoveUndo);
        while (it.hasNext())
        {
            it.next();
            MonitorFixtureItem *mfi = m_fixtures.value(it.key(), NULL);
            if (mfi != NULL && mfi->realPosition() != it.value())
                changed.insert(it.key(), it.value());
        }
        if (changed.isEmpty() == false)
        {
            m_moveUndo.append(changed);
            while (m_moveUndo.count() > 50)
                m_moveUndo.removeFirst();
        }
        m_pendingMoveUndo.clear();
    }
}

QPointF MonitorGraphicsView::snapScenePos(const QPointF &p) const
{
    if (m_snapDivisions <= 0 || m_cellPixels <= 0)
        return p;
    const qreal inc = qreal(m_cellPixels) / m_snapDivisions;
    const qreal sx = m_xOffset + qRound((p.x() - m_xOffset) / inc) * inc;
    const qreal sy = m_yOffset + qRound((p.y() - m_yOffset) / inc) * inc;
    return QPointF(sx, sy);
}

void MonitorGraphicsView::captureMoveUndo()
{
    m_pendingMoveUndo.clear();
    QHashIterator<quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext())
    {
        it.next();
        if (it.value() != NULL)
            m_pendingMoveUndo.insert(it.key(), it.value()->realPosition());
    }
}

void MonitorGraphicsView::undoLastMove()
{
    if (m_moveUndo.isEmpty())
        return;
    const QHash<quint32, QPointF> entry = m_moveUndo.takeLast();
    QHashIterator<quint32, QPointF> it(entry);
    while (it.hasNext())
    {
        it.next();
        MonitorFixtureItem *mfi = m_fixtures.value(it.key(), NULL);
        if (mfi == NULL)
            continue;
        mfi->setRealPosition(it.value());
        updateFixture(it.key());          // reposition the pixel item
        emit fixtureMoved(it.key(), it.value()); // persist to MonitorProperties
    }
}

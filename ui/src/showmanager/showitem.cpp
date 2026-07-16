/*
  Q Light Controller Plus
  showitem.cpp

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

#include <QGraphicsSceneEvent>
#include <QApplication>
#include <QPainter>
#include <QDebug>
#include <QMenu>

#include "headeritems.h"
#include "trackitem.h"
#include "showitem.h"
#include "function.h"

/** Pixels at each end of an item that act as a stretch handle. */
#define EDGE_GRAB 6
/** Minimum item width in pixels while resizing. */
#define MIN_ITEM_WIDTH 8

ShowItem::ShowItem(ShowFunction *function, QObject *)
    : m_color(100, 100, 100)
    , m_locked(false)
    , m_pressed(false)
    , m_width(50)
    , m_timeScale(3)
    , m_trackIdx(-1)
    , m_function(function)
    , m_alignToCursor(NULL)
    , m_lockAction(NULL)
    , m_editable(true)
    , m_resizeEdge(NoEdge)
    , m_resizeStartWidth(0)
{
    Q_ASSERT(function != NULL);

    setCursor(Qt::OpenHandCursor);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);

    m_font = qApp->font();
    m_font.setBold(true);
    m_font.setPixelSize(12);

    setLocked(m_function->isLocked());

    m_alignToCursor = new QAction(tr("Align to cursor"), this);
    connect(m_alignToCursor, SIGNAL(triggered()),
            this, SLOT(slotAlignToCursorClicked()));
    m_lockAction = new QAction(tr("Lock item"), this);
    connect(m_lockAction, SIGNAL(triggered()),
            this, SLOT(slotLockItemClicked()));
}

void ShowItem::updateTooltip()
{
    if (m_function == NULL)
        return;

    setToolTip(QString(tr("Name: %1\nStart time: %2\nDuration: %3\n%4"))
              .arg(functionName())
              .arg(Function::speedToString(m_function->startTime()))
              .arg(Function::speedToString(getDuration()))
              .arg(tr("Click to move this item along the timeline")));
}

QList<QAction *> ShowItem::getDefaultActions() const
{
    QList<QAction *> actions;
    actions.append(m_alignToCursor);

    if (isLocked())
    {
        m_lockAction->setText(tr("Unlock item"));
        m_lockAction->setIcon(QIcon(":/unlock.png"));
    }
    else
    {
        m_lockAction->setText(tr("Lock item"));
        m_lockAction->setIcon(QIcon(":/lock.png"));
    }
    actions.append(m_lockAction);

    return actions;
}

void ShowItem::setTimeScale(int val)
{
    prepareGeometryChange();
    m_timeScale = val;
}

int ShowItem::getTimeScale() const
{
    return m_timeScale;
}

void ShowItem::setStartTime(quint32 time)
{
    if (m_function == NULL)
        return;

    m_function->setStartTime(time);
    updateTooltip();
}

quint32 ShowItem::getStartTime() const
{
    if (m_function)
        return m_function->startTime();
    return 0;
}

void ShowItem::setDuration(quint32 msec, bool stretch)
{
    Q_UNUSED(stretch)

    if (m_function == NULL)
        return;

    m_function->setDuration(msec);
    updateTooltip();
}

quint32 ShowItem::getDuration() const
{
    if (m_function)
        return m_function->duration();
    return 0;
}

void ShowItem::setWidth(int w)
{
    m_width = w;
    updateTooltip();
}

int ShowItem::getWidth() const
{
    return m_width;
}

QPointF ShowItem::getDraggingPos() const
{
    return m_pos;
}

void ShowItem::setTrackIndex(int idx)
{
    m_trackIdx = idx;
}

int ShowItem::getTrackIndex() const
{
    return m_trackIdx;
}

void ShowItem::setColor(QColor col)
{
    m_color = col;
    if (m_function)
        m_function->setColor(col);
    update();
}

QColor ShowItem::getColor() const
{
    return m_color;
}

void ShowItem::setLocked(bool locked)
{
    m_locked = locked;
    if (m_function)
        m_function->setLocked(locked);
    updateMovable();
    update();
}

void ShowItem::setEditable(bool editable)
{
    if (m_editable == editable)
        return;
    m_editable = editable;
    updateMovable();
    update();
}

void ShowItem::updateMovable()
{
    setFlag(QGraphicsItem::ItemIsMovable, m_editable && !m_locked);
}

bool ShowItem::isLocked() const
{
    return m_locked;
}

void ShowItem::setFunctionID(quint32 id)
{
    if (m_function != NULL)
        m_function->setFunctionID(id);
}

quint32 ShowItem::functionID() const
{
    if (m_function != NULL)
        return m_function->functionID();

    return Function::invalidId();
}

ShowFunction *ShowItem::showFunction() const
{
    return m_function;
}

QString ShowItem::functionName() const
{
    return QString();
}

void ShowItem::slotAlignToCursorClicked()
{
    emit alignToCursor(this);
}

void ShowItem::slotLockItemClicked()
{
    setLocked(!isLocked());
    //update();
}

ShowItem::ResizeEdge ShowItem::edgeAt(qreal localX) const
{
    if (localX <= EDGE_GRAB)
        return LeftEdge;
    if (localX >= m_width - EDGE_GRAB)
        return RightEdge;
    return NoEdge;
}

void ShowItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // Begin a stretch-resize when pressing on a handle (unless locked or the
    // timeline is read-only via a track / master lock).
    if (m_locked == false && m_editable && event->button() == Qt::LeftButton)
    {
        ResizeEdge e = edgeAt(event->pos().x());
        if (e != NoEdge)
        {
            m_resizeEdge = e;
            m_resizeStartWidth = m_width;
            m_resizeStartPos = pos();
            // Stop Qt from moving the whole item while we resize.
            setFlag(QGraphicsItem::ItemIsMovable, false);
            m_pressed = true;
            setSelected(true);
            event->accept();
            return;
        }
    }

    QGraphicsItem::mousePressEvent(event);
    m_pos = this->pos();
    if (event->button() == Qt::LeftButton)
        m_pressed = true;
    this->setSelected(true);
}

void ShowItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_resizeEdge == NoEdge)
    {
        QGraphicsItem::mouseMoveEvent(event);
        return;
    }

    qreal dx = event->scenePos().x() - event->buttonDownScenePos(Qt::LeftButton).x();
    prepareGeometryChange();

    if (m_resizeEdge == RightEdge)
    {
        int w = int(m_resizeStartWidth + dx);
        if (w < MIN_ITEM_WIDTH)
            w = MIN_ITEM_WIDTH;
        setWidth(w);
    }
    else // LeftEdge: move the left edge, keep the right edge anchored
    {
        qreal newX = m_resizeStartPos.x() + dx;
        int w = int(m_resizeStartWidth - dx);
        if (w < MIN_ITEM_WIDTH)
        {
            w = MIN_ITEM_WIDTH;
            newX = m_resizeStartPos.x() + (m_resizeStartWidth - MIN_ITEM_WIDTH);
        }
        setPos(newX, m_resizeStartPos.y());
        setWidth(w);
    }

    update();
    event->accept();
}

void ShowItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_resizeEdge != NoEdge)
    {
        bool leftEdge = (m_resizeEdge == LeftEdge);
        m_resizeEdge = NoEdge;
        setFlag(QGraphicsItem::ItemIsMovable, !m_locked);
        m_pressed = false;
        setCursor(Qt::OpenHandCursor);
        emit itemResized(this, leftEdge);
        return;
    }

    QGraphicsItem::mouseReleaseEvent(event);
    qDebug() << Q_FUNC_INFO << "mouse RELEASE event - <" << event->pos().toPoint().x() << "> - <" << event->pos().toPoint().y() << ">";
    setCursor(Qt::OpenHandCursor);
    m_pressed = false;
    emit itemDropped(event, this);
}

QVariant ShowItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene() != NULL)
    {
        // Keep the item locked to a track row (clean multiples of TRACK_HEIGHT
        // from the first row), so it never floats between tracks while dragging.
        QPointF newPos = value.toPointF();
        const qreal base = TRACKS_TOP + 1;
        int row = qMax(0, qRound((newPos.y() - base) / double(TRACK_HEIGHT)));
        newPos.setY(base + row * TRACK_HEIGHT);
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}

void ShowItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (m_locked == false && m_editable && edgeAt(event->pos().x()) != NoEdge)
        setCursor(Qt::SizeHorCursor);
    else if (m_editable && m_locked == false)
        setCursor(Qt::OpenHandCursor);
    else
        setCursor(Qt::ArrowCursor);
    QGraphicsItem::hoverMoveEvent(event);
}

void ShowItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    Q_UNUSED(event)

    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    menu.addAction(m_alignToCursor);
    if (isLocked())
    {
        m_lockAction->setText(tr("Unlock item"));
        m_lockAction->setIcon(QIcon(":/unlock.png"));
    }
    else
    {
        m_lockAction->setText(tr("Lock item"));
        m_lockAction->setIcon(QIcon(":/lock.png"));
    }
    menu.addAction(m_lockAction);
    menu.exec(QCursor::pos());
}


QRectF ShowItem::boundingRect() const
{
    return QRectF(0, 0, m_width, TRACK_HEIGHT - 3);
}

void ShowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (this->isSelected() == true)
        painter->setPen(QPen(Qt::white, 3));
    else
        painter->setPen(QPen(Qt::white, 1));

    // draw item background
    painter->setBrush(QBrush(m_color));
    painter->drawRect(0, 0, m_width, TRACK_HEIGHT - 3);

    painter->setFont(m_font);
}

void ShowItem::postPaint(QPainter *painter)
{
    // draw the function name shadow
    painter->setPen(QPen(QColor(10, 10, 10, 150), 2));
    painter->drawText(QRect(4, 6, m_width - 6, 71), Qt::AlignLeft | Qt::TextWordWrap, functionName());

    // draw the function name
    painter->setPen(QPen(QColor(220, 220, 220, 255), 2));
    painter->drawText(QRect(3, 5, m_width - 5, 72), Qt::AlignLeft | Qt::TextWordWrap, functionName());

    // Stretch handles at each end (hidden when locked or read-only).
    if (m_locked == false && m_editable && m_width > (EDGE_GRAB * 2))
    {
        int h = TRACK_HEIGHT - 3;
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, isSelected() ? 130 : 70));
        painter->drawRect(0, 0, 3, h);
        painter->drawRect(m_width - 3, 0, 3, h);
        // grip ticks
        painter->setPen(QPen(QColor(30, 30, 30, 160), 1));
        for (int gy = (h / 2) - 6; gy <= (h / 2) + 6; gy += 3)
        {
            painter->drawLine(1, gy, 1, gy);
            painter->drawLine(m_width - 2, gy, m_width - 2, gy);
        }
    }

    // Type badge in the top-right corner (DAW-style).
    if (m_iconResource.isEmpty() == false && m_width > 24)
        painter->drawPixmap(m_width - 20, 4, 16, 16,
                            QIcon(m_iconResource).pixmap(16, 16));

    if (m_locked)
        painter->drawPixmap(3, TRACK_HEIGHT >> 1, 24, 24, QIcon(":/lock.png").pixmap(24, 24));

    if (m_pressed)
    {
        quint32 s_time = 0;
        if (x() > TRACK_WIDTH)
            s_time = (double)(x() - TRACK_WIDTH - 2) * (m_timeScale * 500) /
                     (double)(HALF_SECOND_WIDTH);
        painter->drawText(3, TRACK_HEIGHT - 10, Function::speedToString(s_time));
    }
}



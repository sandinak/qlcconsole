/*
  Q Light Controller Plus
  trackitem.cpp

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


#include <QGraphicsSceneMouseEvent>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QApplication>
#include <QLineEdit>
#include <QPainter>
#include <QMenu>

#include "trackitem.h"
#include "track.h"

TrackItem::TrackItem(Track *track, int number)
    : m_number(number)
    , m_isActive(false)
    , m_track(track)
    , m_isMute(false)
    , m_isSolo(false)
    , m_isLocked(false)
    , m_nameProxy(NULL)
    , m_editing(false)
    , m_pressSceneY(0)
    , m_baseY(0)
    , m_dragging(false)
    , m_onButtonRow(false)
    , m_intensityDrag(false)
{
    m_font = qApp->font();
    m_font.setBold(true);
    m_font.setPixelSize(12);

    m_btnFont = qApp->font();
    m_btnFont.setBold(true);
    m_btnFont.setPixelSize(12);

    if (track != NULL)
    {
        m_name = m_track->name();
        m_isMute = m_track->isMute();
        m_isSolo = m_track->isSolo();
        m_isLocked = m_track->isLocked();
        connect(m_track, SIGNAL(changed(quint32)), this, SLOT(slotTrackChanged(quint32)));
    }
    else
        m_name = QString("Track %1").arg(m_number + 1);

    // Buttons left-to-right: Mute, Solo, Lock.
    m_muteRegion = new QRectF(17.0, 10.0, 25.0, 16.0);
    m_soloRegion = new QRectF(45.0, 10.0, 25.0, 16.0);
    m_lockRegion = new QRectF(73.0, 10.0, 25.0, 16.0);
    // Intensity submaster bar (drag horizontally), between the buttons and name.
    // Starts at x=14, clear of the active-indicator bar at x:1-11 (was x=8,
    // which overlapped it) — same right edge as before (TRACK_WIDTH - 12).
    m_intensityRegion = new QRectF(14.0, 30.0, TRACK_WIDTH - 26.0, 9.0);

    m_moveUp = new QAction(QIcon(":/up.png"), tr("Move up"), this);
    connect(m_moveUp, SIGNAL(triggered()),
            this, SLOT(slotMoveUpClicked()));
    m_moveDown = new QAction(QIcon(":/down.png"), tr("Move down"), this);
    connect(m_moveDown, SIGNAL(triggered()),
            this, SLOT(slotMoveDownClicked()));

    m_changeName = new QAction(QIcon(":/editclear.png"), tr("Change name"), this);
    connect(m_changeName, SIGNAL(triggered()),
            this, SLOT(slotChangeNameClicked()));

    m_delete = new QAction(QIcon(":/editdelete.png"), tr("Delete track"), this);
    connect(m_delete, SIGNAL(triggered()),
            this, SLOT(slotDeleteTrackClicked()));

    m_newTrack = new QAction(QIcon(":/edit_add.png"), tr("New track"), this);
    connect(m_newTrack, SIGNAL(triggered()),
            this, SLOT(slotNewTrackClicked()));

    m_color = new QAction(QIcon(":/color.png"), tr("Track colour…"), this);
    connect(m_color, SIGNAL(triggered()),
            this, SLOT(slotColorClicked()));
}

Track *TrackItem::getTrack() const
{
    return m_track;
}

int TrackItem::getTrackNumber() const
{
    return m_number;
}

void TrackItem::setName(QString name)
{
    if (!name.isEmpty())
        m_name = name;
    update();
}

void TrackItem::setActive(bool flag)
{
    m_isActive = flag;
    update();
}

bool TrackItem::isActive() const
{
    return m_isActive;
}

void TrackItem::setFlags(bool solo, bool mute)
{
    m_isSolo = solo;
    m_isMute = mute;
    update();
}

bool TrackItem::isMute() const
{
    return m_isMute;
}

TrackItem::~TrackItem()
{
    delete m_muteRegion;
    delete m_soloRegion;
    delete m_lockRegion;
    delete m_intensityRegion;
}

void TrackItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_isActive = true;
    QGraphicsItem::mousePressEvent(event);
    // The base implementation ignores the event for an item that's neither
    // movable nor selectable (neither flag is set here) — left as ignored,
    // the scene never grabs the mouse for this item, so mouseMoveEvent()
    // below is never delivered on a drag; only the initial press lands,
    // which read as "click-to-position works, dragging doesn't."
    event->accept();

    // Remember whether the press was on a button (solo/mute/lock) row — a drag
    // there must NOT start a reorder.
    const QPoint p = event->pos().toPoint();
    m_onButtonRow = m_soloRegion->contains(p) || m_muteRegion->contains(p) ||
                    m_lockRegion->contains(p) || m_intensityRegion->contains(p);
    m_pressSceneY = event->scenePos().y();
    m_baseY = pos().y();
    m_dragging = false;

    // Intensity submaster: grab the bar and drag to set the level.
    if (m_intensityRegion->contains(event->pos()) && m_track != NULL)
    {
        m_intensityDrag = true;
        const qreal lvl = qBound(qreal(0.0),
            (event->pos().x() - m_intensityRegion->left()) / m_intensityRegion->width(),
            qreal(1.0));
        m_track->setIntensity(lvl);
        update();
        emit itemIntensityChanged(m_track, lvl);
        emit itemClicked(this);
        return;
    }

    if (m_soloRegion->contains(p))
    {
        m_isSolo = !m_isSolo;
        emit itemSoloFlagChanged(this, m_isSolo);
    }
    if (m_muteRegion->contains(p))
    {
        m_isMute = !m_isMute;
        emit itemMuteFlagChanged(this, m_isMute);
    }
    if (m_lockRegion->contains(p))
    {
        m_isLocked = !m_isLocked;
        if (m_track != NULL)
            m_track->setLocked(m_isLocked);
        update();
        emit itemLockChanged(this, m_isLocked);
    }
    emit itemClicked(this);
}

void TrackItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // Dragging the intensity bar scales the submaster live.
    if (m_intensityDrag && m_track != NULL)
    {
        const qreal lvl = qBound(qreal(0.0),
            (event->pos().x() - m_intensityRegion->left()) / m_intensityRegion->width(),
            qreal(1.0));
        m_track->setIntensity(lvl);
        update();
        emit itemIntensityChanged(m_track, lvl);
        return;
    }

    // Vertical drag on the header body reorders the track. Give live feedback by
    // lifting the item with the cursor; the actual swap happens on release.
    if (m_onButtonRow || m_editing || (event->buttons() & Qt::LeftButton) == 0)
        return;

    const qreal dy = event->scenePos().y() - m_pressSceneY;
    if (m_dragging == false && qAbs(dy) < TRACK_HEIGHT / 3)
        return;

    m_dragging = true;
    setY(m_baseY + dy);
    setZValue(1000);        // float above sibling tracks while dragging
}

void TrackItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_intensityDrag)
    {
        m_intensityDrag = false;
        event->accept();
        return;
    }
    if (m_dragging)
    {
        m_dragging = false;
        setZValue(0);
        const qreal dy = event->scenePos().y() - m_pressSceneY;
        int delta = qRound(dy / qreal(TRACK_HEIGHT));
        // Snap back; the view rebuilds at the new order if we actually moved.
        setY(m_baseY);
        if (delta != 0 && m_track != NULL)
            emit itemMoveUpDown(m_track, delta);
        event->accept();
        return;
    }
    QGraphicsItem::mouseReleaseEvent(event);
}

void TrackItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *)
{
    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    menu.addAction(m_newTrack);
    menu.addAction(m_changeName);
    menu.addAction(m_color);
    menu.addSeparator();

    // Per-track flags (act directly on the model; the view saves + repaints).
    QAction *aSoloSafe = NULL, *aHoldLast = NULL;
    QAction *aBg = NULL, *aNormal = NULL, *aOverride = NULL;
    if (m_track != NULL)
    {
        aSoloSafe = menu.addAction(tr("Solo-safe (mute-exempt)"));
        aSoloSafe->setCheckable(true);
        aSoloSafe->setChecked(m_track->isSoloSafe());

        aHoldLast = menu.addAction(tr("Hold last look (stays up between cues)"));
        aHoldLast->setCheckable(true);
        aHoldLast->setChecked(m_track->isHoldLast());

        QMenu *prio = menu.addMenu(tr("Priority / role"));
        aBg = prio->addAction(tr("Background (yields to all)"));
        aNormal = prio->addAction(tr("Normal (last-writer wins)"));
        aOverride = prio->addAction(tr("Override (wins LTP)"));
        for (QAction *a : { aBg, aNormal, aOverride })
            a->setCheckable(true);
        aBg->setChecked(m_track->priority() == Track::Background);
        aNormal->setChecked(m_track->priority() == Track::Normal);
        aOverride->setChecked(m_track->priority() == Track::Override);
        menu.addSeparator();
    }

    if (m_number > 0)
        menu.addAction(m_moveUp);
    menu.addAction(m_moveDown);
    menu.addSeparator();
    menu.addAction(m_delete);

    QAction *chosen = menu.exec(QCursor::pos());
    if (chosen != NULL && m_track != NULL)
    {
        if (chosen == aSoloSafe)        m_track->setSoloSafe(aSoloSafe->isChecked());
        else if (chosen == aHoldLast)   m_track->setHoldLast(aHoldLast->isChecked());
        else if (chosen == aBg)         m_track->setPriority(Track::Background);
        else if (chosen == aNormal)     m_track->setPriority(Track::Normal);
        else if (chosen == aOverride)   m_track->setPriority(Track::Override);
        else return;
        update();
        emit itemPropertiesChanged(m_track);
    }
}

void TrackItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    const QPointF p = event->pos();
    // Double-clicking Mute/Solo/Lock/Intensity is just two ordinary clicks
    // (mousePressEvent already toggled each one) — it must not also fall
    // through to itemDoubleClicked, which opens the track-rename dialog.
    if (m_muteRegion->contains(p) || m_soloRegion->contains(p) ||
        m_lockRegion->contains(p) || m_intensityRegion->contains(p))
        return;

    // Double-clicking the name area (below the buttons) renames inline;
    // elsewhere keeps the old behaviour.
    if (event->pos().y() >= 44)
        startInlineRename();
    else
        emit itemDoubleClicked(this);
}

void TrackItem::startInlineRename()
{
    if (m_nameProxy != NULL || scene() == NULL)
        return;

    m_editing = true;
    update(); // hide the painted name so it doesn't show under the editor

    QLineEdit *edit = new QLineEdit(m_name);
    edit->setAlignment(Qt::AlignLeft);
    edit->selectAll();
    m_nameProxy = scene()->addWidget(edit);
    m_nameProxy->setZValue(1000);
    edit->setFixedWidth(TRACK_WIDTH - 12);
    edit->setFixedHeight(22);
    // The painted name sits at the bottom of the header (local y 47..75,
    // AlignBottom); place the editor over that area so it lines up.
    m_nameProxy->setPos(mapToScene(3, TRACK_HEIGHT - 26));
    edit->setFocus();
    connect(edit, SIGNAL(editingFinished()), this, SLOT(slotRenameCommitted()));
}

void TrackItem::slotRenameCommitted()
{
    if (m_nameProxy == NULL)
        return;

    QLineEdit *edit = qobject_cast<QLineEdit *>(m_nameProxy->widget());
    if (edit != NULL)
    {
        QString newName = edit->text().trimmed();
        if (newName.isEmpty() == false && m_track != NULL && newName != m_name)
        {
            m_name = newName;
            m_track->setName(newName);
            emit itemRenamed(m_track);
        }
    }

    // Tear down the editor (guard re-entrancy: editingFinished can fire twice).
    QGraphicsProxyWidget *proxy = m_nameProxy;
    m_nameProxy = NULL;
    m_editing = false;
    if (scene() != NULL)
        scene()->removeItem(proxy);
    proxy->deleteLater();
    update();
}

QRectF TrackItem::boundingRect() const
{
    return QRectF(0, 0, TRACK_WIDTH - 4, TRACK_HEIGHT);
}

void TrackItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // draw background gradient (tinted by the track's colour if it has one)
    QLinearGradient linearGrad(QPointF(0, 0), QPointF(0, TRACK_HEIGHT));
    QColor tc = (m_track != NULL) ? m_track->color() : QColor();
    if (tc.isValid())
    {
        linearGrad.setColorAt(0, tc.darker(160));
        linearGrad.setColorAt(1, tc);
    }
    else
    {
        linearGrad.setColorAt(0, QColor(50, 64, 75, 255));
        linearGrad.setColorAt(1, QColor(76, 98, 115, 255));
    }
    painter->setBrush(linearGrad);
    painter->drawRect(0, 0, TRACK_WIDTH - 4, TRACK_HEIGHT - 1);

    // Draw left bar that shows if the track is active or not
    painter->setPen(QPen(QColor(48, 61, 72, 255), 1));
    if (m_isActive == true)
        painter->setBrush(QBrush(QColor(0, 255, 0, 255)));
    else
        painter->setBrush(QBrush(QColor(129, 145, 160, 255)));
    painter->drawRoundedRect(1, 1, 10, 40, 2, 2);

    painter->setFont(m_btnFont);

    // draw mute button (leftmost)
    if (m_isMute)
        painter->setBrush(QBrush(QColor(255, 0, 0, 255)));
    else
        painter->setBrush(QBrush(QColor(129, 145, 160, 255)));
    painter->drawRoundedRect(m_muteRegion->toRect(), 3.0, 3.0);
    painter->drawText(25, 23, "M");

    // draw solo button
    if (m_isSolo)
        painter->setBrush(QBrush(QColor(255, 255, 0, 255)));
    else
        painter->setBrush(QBrush(QColor(129, 145, 160, 255)));
    painter->drawRoundedRect(m_soloRegion->toRect(), 3.0, 3.0);
    painter->drawText(53, 23, "S");

    // draw lock button (right of Solo)
    if (m_isLocked)
        painter->setBrush(QBrush(QColor(255, 140, 0, 255)));
    else
        painter->setBrush(QBrush(QColor(129, 145, 160, 255)));
    painter->drawRoundedRect(m_lockRegion->toRect(), 3.0, 3.0);
    painter->drawText(81, 23, "L");

    // Intensity submaster bar: a horizontal fader (drag to scale the track's
    // dimmer output live). Full = 100%.
    {
        const qreal lvl = (m_track != NULL) ? m_track->intensity() : 1.0;
        const QRectF bar = *m_intensityRegion;
        painter->setPen(QPen(QColor(20, 20, 20, 200), 1));
        painter->setBrush(QBrush(QColor(40, 46, 52)));
        painter->drawRoundedRect(bar, 2.0, 2.0);
        QRectF fill = bar.adjusted(1, 1, -1, -1);
        fill.setWidth(fill.width() * lvl);
        // Amber below 100% so a pulled-down track is obvious.
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(lvl >= 0.999 ? QColor(90, 164, 105) : QColor(224, 168, 32)));
        painter->drawRoundedRect(fill, 2.0, 2.0);
        painter->setPen(QPen(QColor(230, 230, 230, 180), 1));
        QFont pf = m_btnFont; pf.setPixelSize(9); painter->setFont(pf);
        painter->drawText(bar.adjusted(3, -1, -3, 0), Qt::AlignRight | Qt::AlignVCenter,
                          QString::number(int(lvl * 100 + 0.5)) + "%");
    }

    // draw bound Scene indicator
    if (m_track != NULL && m_track->getSceneID() != Function::invalidId())
        painter->drawPixmap(TRACK_WIDTH - 33, 5, 24, 24, QIcon(":/scene.png").pixmap(24, 24));

    // Track name (hidden while the inline editor is open).
    if (m_editing == false)
    {
        painter->setFont(m_font);
        // draw shadow
        painter->setPen(QPen(QColor(10, 10, 10, 150), 2));
        painter->drawText(QRect(5, 47, TRACK_WIDTH - 7, 28), Qt::AlignLeft | Qt::TextWordWrap | Qt::AlignBottom, m_name);
        // draw track name
        painter->setPen(QPen(QColor(200, 200, 200, 255), 2));
        painter->drawText(QRect(4, 47, TRACK_WIDTH - 7, 28), Qt::AlignLeft | Qt::TextWordWrap | Qt::AlignBottom, m_name);
    }
}

void TrackItem::slotTrackChanged(quint32 id)
{
    Q_UNUSED(id);

    m_name = m_track->name();
    update();
}

void TrackItem::slotMoveUpClicked()
{
    emit itemMoveUpDown(m_track, -1);
}

void TrackItem::slotMoveDownClicked()
{
    emit itemMoveUpDown(m_track, 1);
}

void TrackItem::slotChangeNameClicked()
{
    startInlineRename();
}

void TrackItem::slotDeleteTrackClicked()
{
    emit itemRequestDelete(m_track);
}

void TrackItem::slotNewTrackClicked()
{
    emit itemRequestNewTrack();
}

void TrackItem::slotColorClicked()
{
    emit itemColorChangeRequested(m_track);
}

/*
  Q Light Controller Plus
  collectionitem.cpp

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

#include <QApplication>
#include <QPainter>
#include <qmath.h>
#include <QDebug>
#include <QMenu>

#include "collectionitem.h"
#include "trackitem.h"

/** Fallback block length (ms) for a Collection whose members report no
 *  finite total duration (e.g. a collection of scenes). Keeps the block
 *  visible/draggable and, crucially, prevents the ShowRunner from stopping
 *  the collection instantly (stopTime == startTime when duration is 0). */
#define COLLECTION_DEFAULT_DURATION 10000

CollectionItem::CollectionItem(Collection *collection, ShowFunction *func)
    : ShowItem(func)
    , m_collection(collection)
{
    Q_ASSERT(collection != NULL);

    if (func->color().isValid())
        setColor(func->color());
    else
        setColor(ShowFunction::defaultColor(Function::CollectionType));

    calculateWidth();
    connect(m_collection, SIGNAL(changed(quint32)), this, SLOT(slotCollectionChanged(quint32)));
    setIconResource(":/collection.png");
}

void CollectionItem::calculateWidth()
{
    int newWidth = 0;
    qint64 collectionDuration = getDuration();

    if (collectionDuration != 0)
        newWidth = ((50 / float(getTimeScale())) * float(collectionDuration)) / 1000;
    else
        newWidth = 100;

    if (newWidth < (50 / m_timeScale))
        newWidth = 50 / m_timeScale;
    setWidth(newWidth);
}

void CollectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    ShowItem::paint(painter, option, widget);
    // A Collection has no per-step timeline; its members fire in parallel,
    // so nothing more than the background block is drawn.
    ShowItem::postPaint(painter);
}

void CollectionItem::setTimeScale(int val)
{
    ShowItem::setTimeScale(val);
    calculateWidth();
}

void CollectionItem::setDuration(quint32 msec, bool stretch)
{
    Q_UNUSED(stretch)

    // The timeline block length IS the ShowFunction duration for a
    // Collection (there is no intrinsic "total duration" to stretch).
    if (m_function)
        m_function->setDuration(msec);
    prepareGeometryChange();
    calculateWidth();
    updateTooltip();
}

quint32 CollectionItem::getDuration() const
{
    if (m_function && m_function->duration())
        return m_function->duration();
    if (m_collection && m_collection->totalDuration())
        return m_collection->totalDuration();
    return COLLECTION_DEFAULT_DURATION;
}

QString CollectionItem::functionName() const
{
    if (m_collection)
        return m_collection->name();
    return QString();
}

Collection *CollectionItem::getCollection() const
{
    return m_collection;
}

void CollectionItem::slotCollectionChanged(quint32)
{
    prepareGeometryChange();
    calculateWidth();
    updateTooltip();
}

void CollectionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *)
{
    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    foreach (QAction *action, getDefaultActions())
        menu.addAction(action);

    menu.exec(QCursor::pos());
}

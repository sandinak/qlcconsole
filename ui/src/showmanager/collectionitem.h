/*
  Q Light Controller Plus
  collectionitem.h

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

#ifndef COLLECTIONITEM_H
#define COLLECTIONITEM_H

#include <QGraphicsItem>
#include <QObject>
#include <QAction>
#include <QFont>

#include "showitem.h"
#include "collection.h"

/** @addtogroup ui_functions
 * @{
 */

/**
 *
 * Collection Item. Clickable and draggable object identifying a Collection
 * function on a Show timeline track. A Collection has no per-step preview
 * (its members run in parallel), so the block simply spans the item's
 * duration on the timeline.
 *
 */
class CollectionItem final : public ShowItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    CollectionItem(Collection *collection, ShowFunction *func);

    /** @reimp */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    /** @reimp */
    void setTimeScale(int val) override;

    /** @reimp */
    void setDuration(quint32 msec, bool stretch) override;

    /** @reimp */
    quint32 getDuration() const override;

    /** @reimp */
    QString functionName() const override;

    /** Return a pointer to the Collection Function associated to this item */
    Collection *getCollection() const;

protected:
    /** @reimp */
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

protected slots:
    void slotCollectionChanged(quint32);

private:
    /** Calculate item width for paint() and boundingRect() */
    void calculateWidth();

private:
    /** Reference to the actual Collection Function */
    Collection *m_collection;
};

/** @} */

#endif

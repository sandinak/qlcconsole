/*
  Q Light Controller Plus
  itemlock.h

  Whether a plot item is drawn locked. An object is held still by its own
  lock or by its layer's; the item knows its own (from its model object)
  and MonitorGraphicsView::refreshItemLayerState() tells it its layer's,
  in the item's data under LayerLockedKey.

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

#ifndef ITEMLOCK_H
#define ITEMLOCK_H

#include <QGraphicsItem>

namespace ItemLock
{
    /** QGraphicsItem::data() key: true while the item's layer is locked. */
    static const int LayerLockedKey = 0x4C4B;

    /** Drawn locked (red): its own lock, or its layer's. */
    inline bool drawnLocked(const QGraphicsItem *item, bool ownLock)
    {
        return ownLock || (item != nullptr && item->data(LayerLockedKey).toBool());
    }
}

#endif // ITEMLOCK_H

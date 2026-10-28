/*
  Q Light Controller Plus
  itemlock.h

  Whether a plot item is drawn locked. An object is held still by its own
  lock or by its layer's; the item knows its own (from its model object)
  and MonitorGraphicsView::refreshItemLayerState() tells it its layer's,
  in the item's data under LayerLockedKey.

  What holds an object still is told apart for the Layers list: its own
  lock, its folder's, its layer's, or the whole plot's (Plot Locked). Each
  is kept separately, so lifting one leaves the others as they were.

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

    /** What holds an object still, as flags: several can hold it at once. */
    enum Hold
    {
        NotHeld    = 0,
        HeldOwn    = 1,   ///< its own lock
        HeldFolder = 2,   ///< a folder it is in (or one above it)
        HeldLayer  = 4,   ///< its layer's lock
        HeldPlot   = 8    ///< the whole plot (Plot Locked)
    };

    inline int holds(bool own, bool folder, bool layer, bool plot)
    {
        return (own ? HeldOwn : 0) | (folder ? HeldFolder : 0)
             | (layer ? HeldLayer : 0) | (plot ? HeldPlot : 0);
    }

    /** The one the Layers list names first: the object's own lock, then the
     *  nearest thing above it that holds it. */
    inline Hold primaryHold(int flags)
    {
        if (flags & HeldOwn)    return HeldOwn;
        if (flags & HeldFolder) return HeldFolder;
        if (flags & HeldLayer)  return HeldLayer;
        if (flags & HeldPlot)   return HeldPlot;
        return NotHeld;
    }
}

#endif // ITEMLOCK_H

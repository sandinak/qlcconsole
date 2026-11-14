/*
  Q Light Controller Plus
  heldprops.h

  Props someone holds (docs/design/BLOCKING.md, "Props someone holds"):
  the edits the Studio makes to hand props, kept out of the views so a
  test drives the same code a click does.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef HELDPROPS_H
#define HELDPROPS_H

#include <QList>
#include <QString>
#include "stagetarget.h"

class MonitorProperties;

/** @addtogroup engine Engine
 * @{
 */

namespace HeldProps
{
    /** "Staff", "Candle": what a hand prop kind is called. */
    QString kindName(StageTarget::Kind kind);

    /** Give each Person in @p people a new hand prop of @p kind, held by
     *  them in @p blocking: one each, in one go (the candles). In a
     *  blocking set the new props are not on stage in Default, so every
     *  cue whose blocking does not say otherwise has none; in Default they
     *  are held in Default. Each prop's own place is beside where its
     *  person stands in Default. Ids that are not people are skipped. The
     *  new props' ids, in @p people's order. Undo is the caller's (one
     *  RigEditSession). */
    QList<quint32> giveEach(MonitorProperties *props, const QList<quint32> &people,
                            StageTarget::Kind kind, quint32 blocking);

    /** Put each hand prop in @p props in @p blocking at @p hold (a
     *  StageTarget::Hold), held by @p holder when that is HeldBy. Not hand
     *  props are skipped. How many changed. */
    int setHold(MonitorProperties *props, const QList<quint32> &propIds, quint32 blocking,
                StageTarget::Hold hold, quint32 holder = UINT_MAX);
}

/** @} */

#endif // HELDPROPS_H

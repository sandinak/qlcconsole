/*
  Q Light Controller Plus
  handprops.h

  Props someone holds on the plot (docs/design/BLOCKING.md, "Props someone
  holds"): how a staff and a candle are drawn, and the plot's right-click
  items that hand them out, take them back and say which remote lights the
  candles.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef HANDPROPS_H
#define HANDPROPS_H

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QList>
#include <functional>

#include "candleremote.h"
#include "stagetarget.h"

class ContextMenu;
class QPainter;
class Doc;

/** @addtogroup ui_monitor
 * @{
 */

namespace HandProps
{
    /** How a candle reads on the plot, from what its remote last sent. */
    enum class Flame { Unknown, Dark, Full, Dimmed, Flicker, FlickerDimmed };
    Flame flameFor(const CandleRemote::State &s);
    /** "lit, flickering" -- the tooltip's and the test's words for it. */
    QString flameText(Flame f);

    /** Draw hand prop @p kind round the item's origin (its point: the hand
     *  when held). @p feet and @p crown are its bottom and top in item
     *  coordinates (the same point, from above); @p ppm pixels a metre.
     *  @p ms drives the flicker. */
    void paint(QPainter *p, StageTarget::Kind kind, Flame flame, const QPointF &feet,
               const QPointF &crown, double ppm, const QColor &colour, bool selected, qint64 ms);
    /** What paint() can cover. */
    QRectF bounds(StageTarget::Kind kind, const QPointF &feet, const QPointF &crown, double ppm);

    /** The plot's right-click on targets @p targets (the one clicked first):
     *  Held By ▸ for hand props, Give Each a Prop ▸ for people, Lit By ▸ for
     *  a candle. Edits are made in blocking @p blocking (the one the Studio
     *  shows) through @p edit, which makes each one undo step. */
    void fillMenu(ContextMenu &cm, Doc *doc, const QList<quint32> &targets, quint32 blocking,
                  const std::function<void(const std::function<void()> &)> &edit);
}

/** @} */

#endif // HANDPROPS_H

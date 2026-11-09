/*
  Q Light Controller Plus
  uprighttext.h

  Text a plot item paints itself, kept upright however the plot is turned
  ("text stays upright and the same size when the view turns", UX rule
  book). The painter's world transform during a scene paint is the item's
  full transform onto the screen -- its own turn and the view's -- so undoing
  that transform's rotation about the text's box leaves the text level on
  screen while the item's shape keeps its turn. Text items get the same from
  MonitorGraphicsView::refreshLabelRotations(); this is for drawText().

  Header-only and free of Q_OBJECT, so it needs no moc pass or CMake entry.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef UPRIGHTTEXT_H
#define UPRIGHTTEXT_H

#include <QPainter>
#include <QtMath>
#include <cmath>

namespace UprightText
{
    /** How far @p painter's transform turns what it draws, in degrees. */
    inline qreal screenTurn(const QPainter *painter)
    {
        const QTransform t = painter->worldTransform();
        return qRadiansToDegrees(std::atan2(t.m12(), t.m11()));
    }

    /** drawText() into @p box, turned back about the box's centre so it
     *  reads level on screen. */
    inline void draw(QPainter *painter, const QRectF &box, int flags, const QString &text)
    {
        const qreal turn = screenTurn(painter);
        if (std::fabs(turn) < 0.5)
        {
            painter->drawText(box, flags, text);
            return;
        }
        painter->save();
        painter->translate(box.center());
        painter->rotate(-turn);
        painter->translate(-box.center());
        painter->drawText(box, flags, text);
        painter->restore();
    }
}

#endif

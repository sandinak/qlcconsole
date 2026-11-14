/*
  Q Light Controller Plus
  structurestudioview_heldprops.cpp

  The rig view's staffs and candles (docs/design/BLOCKING.md, "Props
  someone holds"): drawn where the one reader puts them in the blocking
  the views show -- in a hand, which moves with the person, or standing
  where they were put down -- and not at all when off stage. A candle's
  flame is what its remote was last sent (CandleRemote). The rig view
  draws the flicker as a mark, not a moving flame: a whole-rig repaint
  per flicker frame is not worth it; the plot animates it.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "structurestudioview.h"
#include "monitorproperties.h"
#include "candleremote.h"
#include "stagetarget.h"
#include "doc.h"

void StructureStudioView::drawHandProp(const StageTarget *t, QPainter &p) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr || t == nullptr || !props->targetOnStage(t))
        return;
    const QVector3D at = props->targetPosition(t);       // in the blocking shown
    const StageTarget *holder = props->targetHolder(t);
    const float len = t->propLength();
    float bottom = at.z();
    if (holder != nullptr && t->kind() == StageTarget::Kind::Staff)
        bottom = props->holderFeet(holder).z() + 0.02f;
    const QVector3D b(at.x(), at.y(), bottom);
    const QVector3D top(at.x(), at.y(), bottom + len);

    if (t->kind() == StageTarget::Kind::Staff)
    {
        emitLine(w2s(b), w2s(top), viewDepth(b), viewDepth(top), QColor(205, 150, 90), 3.0, p);
        return;
    }

    // A candle: its body, and its flame as the remote last left it.
    emitLine(w2s(b), w2s(top), viewDepth(b), viewDepth(top), QColor(250, 240, 220), 6.0, p);
    const CandleRemote::State s = CandleRemote::of(m_doc)->stateFor(t);
    const QVector3D flame = top + QVector3D(0.0f, 0.0f, 0.035f);
    if (!s.known || !s.lit)
    {
        emitDot(w2s(top), viewDepth(top), 1.5, QColor(40, 40, 40), p);     // the wick
        return;
    }
    const double r = s.dimmed ? 2.5 : 4.0;
    emitDot(w2s(flame), viewDepth(flame), r, QColor(255, 186, 56), p);
    if (s.flicker)
    {
        // Two sparks either side: flickering, said by shape.
        for (float dx : { -0.04f, 0.04f })
        {
            const QVector3D sp = flame + QVector3D(dx, 0.0f, 0.03f);
            emitDot(w2s(sp), viewDepth(sp), 1.4, QColor(255, 140, 40), p);
        }
    }
}

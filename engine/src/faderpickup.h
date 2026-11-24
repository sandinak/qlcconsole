/*
  Q Light Controller Plus
  faderpickup.h

  Copyright (C) Branson Matheson

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

#ifndef FADERPICKUP_H
#define FADERPICKUP_H

#include <QtGlobal>

#include "qlcengine_export.h"

class QString;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Fader pick-up (soft takeover) for the faders that are not masters
 * (docs/design/CONTROL_SCOPES.md sections 5 and 11, decisions Q299, Q313).
 *
 * An external fader that is not where the control is does nothing until it
 * is moved to, or through, the control's current value; then it follows.
 * This is the rule the Virtual Console slider's "catch values" always had,
 * with one addition for a fader whose position is not known yet (after
 * reset(), so after every start): a first value already within Threshold
 * of the control is taken, any other only records where the fader is. A
 * fader that is anywhere else waits, whichever way it moves: a fader pulled
 * *away* from the control is not taken, so a control never jumps to a fader
 * that was somewhere else.
 *
 * The Grand Master and the Virtual Console Grand / Sub / Group master
 * sliders do not use this: they follow their fader at once, both ways,
 * until their surface type declares them (Q313).
 */
class QLC_ENGINE_EXPORT FaderPickup
{
public:
    /** +/- value range to catch the control, on the 0..255 scale */
    static constexpr int Threshold = 4;

    /** Forget where the fader is (the control changed hands, the widget hid) */
    void reset()
    {
        m_last = -1;
        m_waiting = false;
    }

    /** The input that drives the control is @p universe / @p channel: a
     *  different one than before is a different fader, so start over. */
    void setSource(quint32 universe, quint32 channel)
    {
        const quint64 key = (quint64(universe) << 32) | channel;
        if (key == m_source)
            return;
        m_source = key;
        reset();
    }

    /**
     * An input @p value arrived while the control sits at @p current (both
     * 0..255). Returns true when the control should follow @p value.
     */
    bool accept(int value, int current)
    {
        const int last = m_last;
        m_last = value;
        bool take;
        if (last == -1)
            take = absDiff(value - current) <= Threshold;
        else
            // Still on the same side of the control: it waits; at it, or
            // through it: taken
            take = !((last < current - Threshold && value < current - Threshold) ||
                     (last > current + Threshold && value > current + Threshold));
        m_waiting = !take;
        return take;
    }

    /** The fader has spoken and is not yet where the control is: it is
     *  waiting to be picked up. Clears by itself when the control is moved
     *  (on screen, say) to where the fader is. */
    bool waiting(int current) const
    {
        return m_waiting && m_last >= 0 && absDiff(m_last - current) > Threshold;
    }

    /** Which way to move the fader to pick the control up: +1 up, -1 down,
     *  0 when it is not waiting */
    int direction(int current) const
    {
        if (!waiting(current))
            return 0;
        return m_last < current ? 1 : -1;
    }

    /** Where the fader last said it was, 0..255; -1 when unknown */
    int last() const { return m_last; }

    /** "Grand Master fader: move it to 75% to take over". @p percent is
     *  where on the fader's travel the control is. */
    static QString hintText(const QString &name, int percent);

private:
    static int absDiff(int a) { return a < 0 ? -a : a; }
    int m_last = -1;
    bool m_waiting = false;
    quint64 m_source = ~quint64(0);
};

/** @} */

#endif

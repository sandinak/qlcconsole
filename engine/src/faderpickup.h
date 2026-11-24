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

#include "qlcengine_export.h"

class QString;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Fader pick-up (soft takeover), shared by every absolute fader input
 * (docs/design/CONTROL_SCOPES.md section 5, decision Q299).
 *
 * An external fader that is not where the control is does nothing until it
 * is moved to, or through, the control's current value; then it follows.
 * The first value after reset() only tells where the fader is. This is the
 * rule the Virtual Console slider's "catch values" always had.
 *
 * A motorised fader (one whose device takes feedback) is not made to wait:
 * the caller sends it the current value instead (see accept()).
 */
class QLC_ENGINE_EXPORT FaderPickup
{
public:
    /** +/- value range to catch the control, on the 0..255 scale */
    static constexpr int Threshold = 4;

    /** Forget where the fader is (the control changed hands, the widget hid) */
    void reset() { m_last = -1; m_waiting = false; }

    /**
     * An input @p value arrived while the control sits at @p current (both
     * 0..255). Returns true when the control should follow @p value.
     *
     * With @p motorised, the first value of a take-over that is away from
     * @p current is refused with *sendBack set: the caller sends @p current
     * to the device so it moves its fader, and the fader is never shown as
     * waiting. Later values pass, so a hand on a motor is not fought.
     */
    bool accept(int value, int current, bool motorised = false, bool *sendBack = nullptr)
    {
        if (sendBack != nullptr)
            *sendBack = false;
        const int last = m_last;
        m_last = value;
        if (motorised)
        {
            m_waiting = false;
            if (last == -1 && absDiff(value - current) > Threshold)
            {
                if (sendBack != nullptr)
                    *sendBack = true;
                return false;
            }
            return true;
        }
        if (last == -1 ||
            (last < current - Threshold && value < current - Threshold) ||
            (last > current + Threshold && value > current + Threshold))
        {
            m_waiting = true;
            return false;
        }
        m_waiting = false;
        return true;
    }

    /** The fader has spoken and is not yet where the control is: it is
     *  waiting to be picked up. Clears by itself when the control is moved
     *  (on screen, say) to where the fader is. */
    bool waiting(int current) const
    {
        return m_waiting && m_last >= 0 && absDiff(m_last - current) > Threshold;
    }

    /** Where the fader last said it was, 0..255; -1 when unknown */
    int last() const { return m_last; }

    /** "Grand Master fader: move it to 75% to take over" */
    static QString hintText(const QString &name, int percent);

private:
    static int absDiff(int a) { return a < 0 ? -a : a; }
    int m_last = -1;
    bool m_waiting = false;
};

/** @} */

#endif

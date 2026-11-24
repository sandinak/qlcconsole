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
 * Fader pick-up (soft takeover), shared by every absolute fader input
 * (docs/design/CONTROL_SCOPES.md section 5, decision Q299).
 *
 * An external fader that is not where the control is does nothing until it
 * is moved to, or through, the control's current value; then it follows.
 * This is the rule the Virtual Console slider's "catch values" always had,
 * with two changes for a fader whose position is not known yet (after
 * reset(), so after every start):
 *
 * - a first value already within Threshold of the control is taken;
 * - a fader whose first move goes *away* from the control, by more than
 *   Threshold, is taken as one that sat at the control and is being pulled
 *   (a Grand Master thrown down in an emergency follows on its second
 *   value). A first move *toward* the control turns this off: that fader
 *   was not at the control, and it waits until it gets there.
 *
 * Every fader follows this rule, motorised or not: whether a device's fader
 * has a motor cannot be told from its universe (a feedback line is mostly
 * pad LEDs). When a wait starts, accept() sets *sendBack; a caller with a
 * feedback line sends the current value, so a motor fader drives itself to
 * the control and is picked up when it arrives, and a plain one ignores it.
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
        m_first = -1;
        m_waiting = false;
        m_unsure = false;
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
     *
     * *sendBack is set when this value starts a wait: a caller that can
     * send feedback to the device sends it @p current (a motor fader goes
     * there; see the class comment).
     */
    bool accept(int value, int current, bool *sendBack = nullptr)
    {
        if (sendBack != nullptr)
            *sendBack = false;
        const int last = m_last;
        m_last = value;
        bool take = false;
        if (last == -1)
        {
            // Where the fader is: taken only when it is already there
            m_first = value;
            m_unsure = true;
            take = absDiff(value - current) <= Threshold;
        }
        else if ((last < current - Threshold && value < current - Threshold) ||
                 (last > current + Threshold && value > current + Threshold))
        {
            // Still on the same side of the control
            if (m_unsure)
            {
                const int was = absDiff(last - current);
                const int now = absDiff(value - current);
                if (now < was)
                    m_unsure = false;   // moving toward it: it was not there
                else if (now - absDiff(m_first - current) > Threshold)
                    take = true;        // pulled away from it: it was there
            }
        }
        else
        {
            take = true;                // at it, or through it
        }

        if (take)
        {
            m_waiting = false;
            m_unsure = false;
            return true;
        }
        if (!m_waiting && sendBack != nullptr)
            *sendBack = true;
        m_waiting = true;
        return false;
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
    int m_first = -1;           //!< the first value after reset()
    bool m_waiting = false;
    bool m_unsure = false;      //!< not known yet whether it sat at the control
    quint64 m_source = ~quint64(0);
};

/** @} */

#endif

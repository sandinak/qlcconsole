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
 * with one addition for a fader whose position is not known yet (after
 * reset(), so after every start): a first value already within Threshold
 * of the control is taken. A fader that is anywhere else waits, whichever
 * way it moves: a fader pulled *away* from the control is not taken, so a
 * control never jumps to a fader that was somewhere else.
 *
 * setLowerAtOnce() is the Grand Master's exception: lowering it is always
 * the safe way (an emergency fade-out must never wait), so a fader below
 * the control that is pulled down by more than Threshold is taken at once,
 * wherever it was. The pull is measured from where the fader stopped
 * following (or started to wait), so a parked fader's jitter never is one,
 * and not over reports the fader made above the control, so a motor fader
 * sent down by feedback (the control dipped on screen) never is one either.
 * Raising it still waits for the pick-up. Every master whose lowering is
 * the safe way uses it (the Grand Master, VC Grand / Sub / Group master
 * sliders).
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
        m_lastCurrent = -1;
        m_runTop = -1;
        m_waiting = false;
    }

    /** Take a downward pull below the control at once (the Grand Master:
     *  lowering it never waits). Off by default. */
    void setLowerAtOnce(bool on) { m_lowerAtOnce = on; }
    bool lowerAtOnce() const { return m_lowerAtOnce; }

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
        const int lastCurrent = m_lastCurrent;
        m_last = value;
        m_lastCurrent = current;
        // Where this downward run started (a rise starts a new one). A
        // fall made while the fader was above the control as it then stood
        // is no pull (a motor fader sent down by feedback when the control
        // was dipped on screen): the run starts again here, so only a fall
        // from below the control counts.
        if (last == -1 || value > last || last > lastCurrent + Threshold)
            m_runTop = value;
        bool take = false;
        if (last == -1)
        {
            // Where the fader is: taken only when it is already there
            take = absDiff(value - current) <= Threshold;
        }
        else if ((last < current - Threshold && value < current - Threshold) ||
                 (last > current + Threshold && value > current + Threshold))
        {
            // Still on the same side of the control: it waits, except a
            // pull down below a control that lowers at once (the GM). The
            // run must fall by more than Threshold, so a parked fader's
            // jitter is no move.
            if (m_lowerAtOnce && value < current - Threshold &&
                m_runTop - value > Threshold)
                take = true;
        }
        else
        {
            take = true;                // at it, or through it
        }

        if (take)
        {
            // A downward run is measured from where the fader stopped
            // following: one that followed the control down and was left
            // there is not "pulled" by its jitter once the control is
            // raised elsewhere.
            m_runTop = value;
            m_waiting = false;
            return true;
        }
        if (!m_waiting)
        {
            m_runTop = value;           // the wait starts here
            if (sendBack != nullptr)
                *sendBack = true;
        }
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
    int m_lastCurrent = -1;     //!< where the control was at the last report
    int m_runTop = -1;          //!< where the current downward run started (a take or a wait restarts it)
    bool m_waiting = false;
    bool m_lowerAtOnce = false; //!< a pull down below the control is taken at once
    quint64 m_source = ~quint64(0);
};

/** @} */

#endif

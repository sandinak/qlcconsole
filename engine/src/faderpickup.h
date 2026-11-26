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
 * What a fader is, as its surface type declares it (the input profile's
 * <Facts Fader="..."/>, docs/design/CONTROL_SCOPES.md section 6). Unknown:
 * not declared (no type, a profile written before surface types, a raw
 * MIDI input).
 */
enum class FaderFact
{
    Unknown,
    Absolute,   //!< a plain fader: where it is is where it is
    Motorised,  //!< a motor fader: it can be sent where the control is
    Relative    //!< an endless knob or encoder: it has no position
};

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
 * The fader's declared fact (setFact(), from its surface type) changes
 * the rule:
 *  - Relative: never waits (it has no position; it turns from the value).
 *  - Motorised: waits as above, and when a wait starts accept() asks the
 *    caller to send the control's value back (*sendBack), so the motor
 *    drives the fader there and it is picked up when it arrives.
 *  - Absolute, Unknown: the rule above, unchanged.
 *
 * A master (setMaster(): the Grand Master, VC Grand / Sub / Group master
 * sliders) follows its fader at once, both ways (Q313), unless its type
 * declares it Absolute: then a raise waits to be picked up, and a pull
 * down to or below the master is taken at once -- the first report after
 * a start, and any fall of more than Threshold (a resting fader's jitter)
 * from where the downward run began. A fader below the master pushed up
 * is no pull: it waits until it meets the master. A Motorised master
 * follows at once (it is sent every change, so it is where the master
 * is); Unknown and Relative ones as always.
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
        m_runTop = -1;
        m_waiting = false;
    }

    /** What the fader is (its type's declaration); Unknown by default */
    void setFact(FaderFact fact) { m_fact = fact; }
    FaderFact fact() const { return m_fact; }

    /** The control is a master (lowering it is the safe way): see the
     *  class comment. Off by default. */
    void setMaster(bool master) { m_master = master; }
    bool master() const { return m_master; }

    /** Whether a control with this fact and master flag waits at all:
     *  false for a Relative fader, and for a master not declared Absolute */
    bool picksUp() const
    {
        if (m_fact == FaderFact::Relative)
            return false;
        return !m_master || m_fact == FaderFact::Absolute;
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
     * *sendBack is set when a Motorised fader starts to wait: the caller
     * sends @p current to the device (see the class comment).
     */
    bool accept(int value, int current, bool *sendBack = nullptr)
    {
        if (sendBack != nullptr)
            *sendBack = false;
        const int last = m_last;
        m_last = value;
        if (!picksUp())
        {
            m_waiting = false;          // follows at once (see the class comment)
            return true;
        }
        // Where this downward run started: a rise starts a new one
        if (last == -1 || value > last)
            m_runTop = value;
        /* A master's pull-down never waits: at or below it, the first
           report after a start is taken, and so is a fall of more than a
           resting fader's jitter (Threshold) from where the run started,
           however it got there. A fader below the master that is pushed up
           is no pull: it waits until it meets the master (below). */
        if (m_master && value <= current + Threshold &&
            (last == -1 || m_runTop - value > Threshold))
        {
            m_runTop = value;
            m_waiting = false;
            return true;
        }
        bool take;
        if (last == -1)
            take = absDiff(value - current) <= Threshold;
        else
            // Still on the same side of the control: it waits; at it, or
            // through it: taken
            take = !((last < current - Threshold && value < current - Threshold) ||
                     (last > current + Threshold && value > current + Threshold));
        // A motor fader is sent where the control is once, when it starts to wait
        if (!take && !m_waiting && m_fact == FaderFact::Motorised && sendBack != nullptr)
            *sendBack = true;
        if (take)
            m_runTop = value;
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

    /** "Rate fader: move it to 75% to take over". @p percent is
     *  where on the fader's travel the control is. */
    static QString hintText(const QString &name, int percent);

private:
    static int absDiff(int a) { return a < 0 ? -a : a; }
    int m_last = -1;
    bool m_waiting = false;
    bool m_master = false;
    int m_runTop = -1;          //!< where the fader's current downward run started
    FaderFact m_fact = FaderFact::Unknown;
    quint64 m_source = ~quint64(0);
};

/** @} */

#endif

/*
  Q Light Controller Plus
  grandmasterpickup.h

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

#ifndef GRANDMASTERPICKUP_H
#define GRANDMASTERPICKUP_H

#include <QtGlobal>

/** @addtogroup engine Engine
 * @{
 */

/**
 * The Grand Master fader's pick-up after a load, a replug or a wake (board
 * Q316, amending Q313; docs/design/CONTROL_SURFACES_EVAL.md risk 7). A
 * fader parked at 0 while the show sits at 100 % must not black the stage
 * out on the first brush, and a device that dumps where its controls are
 * when it comes back must not move the master at all:
 *
 *  1. For DumpMs after rearm() every value is ignored: a device's state
 *     dump, not a hand on the fader.
 *  2. Then a pull to 0 always applies at once (the GM is a safety: down
 *     is never held back).
 *  3. Otherwise, after rearm() the fader waits until it is at the master
 *     (within Threshold) or passes through it; then it follows.
 *  4. Not rearmed: it follows at once, as the GM always did.
 *
 * Times are the caller's clock in ms (DeskRecord's), so a test can step
 * them. Header only, no signals: the owner says what waiting means.
 */
class GrandMasterPickup
{
public:
    /** Values in this long after rearm() are a device's dump. */
    static constexpr qint64 DumpMs = 500;
    /** +/- on the 0..255 scale that counts as at the master. */
    static constexpr int Threshold = 4;

    /** A load, a replug or a wake at @p now: wait to pick up again. */
    void rearm(qint64 now)
    {
        m_rearmedAt = now;
        m_waiting = true;
        m_last = -1;
    }

    /** Whether @p value, arriving at @p now while the master is at
     *  @p current (both 0..255), moves the master. */
    bool accept(int value, int current, qint64 now)
    {
        if (m_rearmedAt >= 0 && now - m_rearmedAt < DumpMs)
            return false;                       // the device's dump
        const int last = m_last;
        m_last = value;
        if (value == 0)
        {
            m_waiting = false;                  // down to 0: always, at once
            return true;
        }
        if (!m_waiting)
            return true;
        const bool at = absDiff(value - current) <= Threshold;
        const bool through = last >= 0 && ((last < current && value > current)
                                           || (last > current && value < current));
        if (at || through)
            m_waiting = false;
        return !m_waiting;
    }

    /** It waits: the fader spoke after the dump and is not at the master. */
    bool waiting() const { return m_waiting; }
    /** Where the fader last said it was (-1: not since rearm()). */
    int last() const { return m_last; }
    /** Which way to move the fader to pick up: +1 up, -1 down, 0 none. */
    int direction(int current) const
    {
        if (!m_waiting || m_last < 0)
            return 0;
        return m_last < current ? 1 : -1;
    }

private:
    static int absDiff(int a) { return a < 0 ? -a : a; }
    qint64 m_rearmedAt = -1;
    bool m_waiting = false;
    int m_last = -1;
};

/** @} */

#endif

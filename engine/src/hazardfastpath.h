/*
  Q Light Controller Plus
  hazardfastpath.h

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

#ifndef HAZARDFASTPATH_H
#define HAZARDFASTPATH_H

#include <QtGlobal>
#include <atomic>

#include "qlcengine_export.h"

/**
 * The spotter's Hold let go of, and their Kill, as they arrive from the
 * device -- on the input plugin's own thread, before anything is queued to
 * the main thread (board Q352/Q353, the second review's item 7): a GUI busy
 * or frozen never keeps a flame or a laser out. InputPatch feeds every
 * input value here; only the two learned controls do anything, and only
 * towards safe (a press of Hold still goes the long way, DeskRecord). The
 * hazards' fire layers read the flags each frame (FaderGate).
 */
class QLC_ENGINE_EXPORT HazardFastPath
{
public:
    static constexpr quint64 kNone = ~quint64(0);
    static quint64 key(quint32 universe, quint32 channel) { return (quint64(universe) << 32) | channel; }

    /* One per show (the second review's N2): its InputOutputMap holds it,
       every Universe of that map points at it, and the show's
       OutputRuleKeeper attaches its flags while it lives. Another Doc (an
       import's scratch, Fixture Remap, the CLI) has its own. */
    std::atomic<quint64> holdKey { kNone };
    std::atomic<quint64> killKey { kNone };

    /** The keeper's flags (attach()); until then and after detach(), the
     *  path does nothing. */
    void attach(std::atomic<bool> *hold, std::atomic<bool> *armed, std::atomic<quint32> *epoch)
    {
        m_hold = hold;
        m_armed = armed;
        m_epoch = epoch;
        m_alive.store(true, std::memory_order_release);
    }
    /** Nothing reaches the flags after this returns: a feed in flight on a
     *  plugin's thread is waited for (the quiescence point, N4). */
    void detach();

    /** One input value, any thread: Hold let go -> not held; Kill pressed
     *  -> not held, not armed, every grant closed. */
    void feed(quint32 universe, quint32 channel, uchar value)
    {
        m_users.fetch_add(1, std::memory_order_acq_rel);
        if (m_alive.load(std::memory_order_acquire))
        {
            const quint64 k = key(universe, channel);
            if (value == 0 && k == holdKey.load(std::memory_order_relaxed))
                m_hold->store(false, std::memory_order_release);
            else if (value > 0 && k == killKey.load(std::memory_order_relaxed))
            {
                m_hold->store(false, std::memory_order_release);
                m_armed->store(false, std::memory_order_release);
                m_epoch->fetch_add(1, std::memory_order_acq_rel);
            }
        }
        m_users.fetch_sub(1, std::memory_order_acq_rel);
    }

private:
    std::atomic<bool> *m_hold = nullptr;
    std::atomic<bool> *m_armed = nullptr;
    std::atomic<quint32> *m_epoch = nullptr;
    std::atomic<bool> m_alive { false };
    std::atomic<int> m_users { 0 };
};

#endif

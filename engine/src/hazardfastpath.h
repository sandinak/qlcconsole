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

    std::atomic<quint64> holdKey { kNone };
    std::atomic<quint64> killKey { kNone };
    std::atomic<bool> *hold = nullptr;
    std::atomic<bool> *armed = nullptr;
    /** Moved on by a Kill: every grant closed (FaderGate::epoch) */
    std::atomic<quint32> *epoch = nullptr;

    /** The one in use (OutputRuleKeeper's); nullptr: none. */
    static std::atomic<HazardFastPath *> &current();

    /** One input value, any thread: Hold let go -> not held; Kill pressed
     *  -> not held, not armed, every grant closed. */
    static void feed(quint32 universe, quint32 channel, uchar value)
    {
        HazardFastPath *h = current().load(std::memory_order_acquire);
        if (h == nullptr)
            return;
        const quint64 k = key(universe, channel);
        if (value == 0 && k == h->holdKey.load(std::memory_order_relaxed))
            h->hold->store(false, std::memory_order_release);
        else if (value > 0 && k == h->killKey.load(std::memory_order_relaxed))
        {
            h->hold->store(false, std::memory_order_release);
            h->armed->store(false, std::memory_order_release);
            h->epoch->fetch_add(1, std::memory_order_acq_rel);
        }
    }
};

#endif

/*
  Q Light Controller Plus
  padtriggers.h

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

#ifndef PADTRIGGERS_H
#define PADTRIGGERS_H

#include "qlcengine_export.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * What a DualSense's adaptive triggers should feel like (board Q199, build
 * A). The pad is only asked for the feel; the hardware is the HID plugin's
 * (GCDualSenseAdaptiveTrigger on macOS, nothing elsewhere or on other pads),
 * so what the feel should be is decided here, where a test can read it.
 *
 *  - Pad Run armed (LB held):  off -- the triggers do nothing in Run (0.4.5),
 *    so neither may feel as if it did.
 *  - walking the show:         both triggers click -- one click at the end
 *    of each pull, a cue (or a level) stepped.
 *  - otherwise:                off.
 */
class QLC_ENGINE_EXPORT PadTriggers
{
public:
    enum Feel { Off = 0, Firm, Click };

    struct State
    {
        Feel right = Off;
        Feel left = Off;
        bool operator==(const State &o) const { return right == o.right && left == o.left; }
        bool operator!=(const State &o) const { return !(*this == o); }
    };

    /** The feel for the Run role in this state. Walking wins over the arm
     *  (the walk has no GO); the role off is always Off. */
    static State forState(bool runRole, bool armed, bool walking);

    /** The words the feedback params carry ("off", "firm", "click"). */
    static const char *name(Feel feel);
    /** The other way (anything unknown: Off). */
    static Feel fromName(const char *name);
};

/** @} */

#endif

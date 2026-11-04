/*
  Q Light Controller Plus - qlcconsole
  systemprobe.h

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


#ifndef SYSTEMPROBE_H
#define SYSTEMPROBE_H

#include <QString>

#include "qlcengine_export.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * The operating-system calls behind Settings ▸ Performance (SystemTuning in
 * the UI): what the system is set to that affects how steadily the 50 Hz DMX
 * timer runs, and the one thing the app itself may change -- running at full
 * speed while a show is in Operate.
 *
 * Nothing here needs administrator rights, and nothing here changes the
 * system for any other program. Everything that would (the CPU governor, a
 * power plan, a real-time limit) is the user's to do; SystemTuning tells
 * them how.
 */
namespace SystemProbe
{
    /** macOS Low Power Mode: 1 on, 0 off, -1 unknown (other systems, or a
     *  macOS older than 12). */
    QLC_ENGINE_EXPORT int lowPowerMode();

    /** Linux RLIMIT_RTPRIO, the highest real-time priority this user may
     *  ask for without CAP_SYS_NICE: 0 none, -2 unlimited, -1 unknown
     *  (other systems). */
    QLC_ENGINE_EXPORT long rtprioLimit();

    /** Windows' active power plan: "high", "balanced", "saver", "other"
     *  (a custom or duplicated plan: Ultimate Performance is one), or empty
     *  when unknown (other systems). */
    QLC_ENGINE_EXPORT QString powerPlan();

    /** Whether setFullSpeed() does anything on this system (macOS: App Nap
     *  and idle sleep; Windows: EcoQoS throttling and idle sleep). */
    QLC_ENGINE_EXPORT bool canHoldFullSpeed();

    /**
     * Hold (or let go of) full speed for this process: macOS, an activity
     * that keeps App Nap and timer coalescing off and the computer from idle
     * sleep; Windows, out of EcoQoS power throttling and no idle sleep.
     * @a keepDisplayAwake also keeps the display from sleeping. Call on the
     * main thread. Calling it again with other values replaces the hold.
     */
    QLC_ENGINE_EXPORT void setFullSpeed(bool on, bool keepDisplayAwake, const QString &reason);

    /** Whether setFullSpeed(true, ...) is in force */
    QLC_ENGINE_EXPORT bool fullSpeedHeld();

    /** The cores display work may spread over: Apple silicon's performance
     *  cores (hw.perflevel0.logicalcpu); on Linux the CPUs this process may
     *  run on (isolcpus and cpusets already left out), only the biggest on
     *  big.LITTLE (cpu_capacity); on Windows the highest efficiency class.
     *  QThread::idealThreadCount() where none of that can be read. At
     *  least 1, worked out once. */
    QLC_ENGINE_EXPORT int performanceCores();
}

/** @} */

#endif

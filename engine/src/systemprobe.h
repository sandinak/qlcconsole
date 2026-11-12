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

#include <QDeadlineTimer>
#include <QList>

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

    /*********************************************************************
     * What else is going on: for Settings ▸ Performance. Each is safe on
     * any thread. The ones that take a deadline run a system tool (never
     * through a shell, by absolute path) and give up when it passes.
     *********************************************************************/

    /** The processor's marketing name; empty when the system doesn't say */
    QLC_ENGINE_EXPORT QString cpuName();
    /** This build is being translated for another processor (Rosetta) */
    QLC_ENGINE_EXPORT bool translated();

    /** 1 running on battery, 0 on mains or there is no battery, -1 unknown.
     *  @a percent, when not null: charge, or -1. */
    QLC_ENGINE_EXPORT int onBattery(int *percent = nullptr);
    /** 0 nominal, 1 warm, 2 slowing down, 3 critical, -1 the system doesn't say */
    QLC_ENGINE_EXPORT int thermalState();
    /** 0 fine, 1 low, 2 critical, -1 unknown */
    QLC_ENGINE_EXPORT int memoryPressure();

    struct ProcessUse
    {
        QString name;
        double cpu = 0;     ///< percent of one core
    };
    /** Every other process, with what it has used lately; empty while
     *  toolsAllowed() is off */
    QLC_ENGINE_EXPORT QList<ProcessUse> processes(const QDeadlineTimer &deadline);

    /** Whether the screen asks for a password after sitting idle: 1, 0, -1
     *  unknown. @a idleMinutes: how long it stays on, 0 never, -1 unknown. */
    QLC_ENGINE_EXPORT int screenLock(int *idleMinutes, const QDeadlineTimer &deadline);
    /** 1 a Time Machine backup is running now, 0 not, -1 not macOS / unknown */
    QLC_ENGINE_EXPORT int backupRunning(const QDeadlineTimer &deadline);
    /** 1 the system backs up by itself, 0 it doesn't, -1 unknown */
    QLC_ENGINE_EXPORT int backupScheduled();
    /** 1 the system installs updates by itself, 0 it doesn't, -1 unknown */
    QLC_ENGINE_EXPORT int autoUpdates();
    /** Windows: an update is waiting for a restart */
    QLC_ENGINE_EXPORT bool rebootPending();
    /** Raspberry Pi: vcgencmd get_throttled as a number, -1 unknown */
    QLC_ENGINE_EXPORT int piThrottled(const QDeadlineTimer &deadline);
    /**
     * Whether a look may start a process, or do other heavy probing (every
     * process's CPU time). Off while the show is in Operate: the show wins,
     * and starting a process from a large app can stall it for a moment.
     * runTool() returns empty while it is off. Any thread.
     */
    QLC_ENGINE_EXPORT void setToolsAllowed(bool allowed);
    QLC_ENGINE_EXPORT bool toolsAllowed();
    /** How many processes runTool() has started (tests) */
    QLC_ENGINE_EXPORT int toolsStarted();

#if !defined(Q_OS_WIN)
    /** A tool's output (stdout and stderr together), empty when it failed,
     *  the deadline passed or tools aren't allowed now. It runs at the lowest
     *  priority the system has. No shell; the caller gives the absolute path. */
    QLC_ENGINE_EXPORT QString runTool(const QString &program, const QStringList &args,
                                      const QDeadlineTimer &deadline);
#endif

    /* What the tools print, read: separate so a test can feed them */
    QLC_ENGINE_EXPORT QList<ProcessUse> parseProcessTable(const QString &ps, qint64 selfPid);
    QLC_ENGINE_EXPORT int parseScreenLock(const QString &sysadminctl);
    QLC_ENGINE_EXPORT int parseIdleMinutes(const QString &pmset);
    QLC_ENGINE_EXPORT int parseBackupRunning(const QString &tmutil);
    QLC_ENGINE_EXPORT int parseThrottled(const QString &vcgencmd);
    /** A /proc/<pid>/stat line: its command and CPU ticks; false if malformed */
    QLC_ENGINE_EXPORT bool parseProcStat(const QString &stat, QString *name, qint64 *ticks);
}

/** @} */

#endif

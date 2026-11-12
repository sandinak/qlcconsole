/*
  Q Light Controller Plus - qlcconsole
  systemtuning.h

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

#ifndef SYSTEMTUNING_H
#define SYSTEMTUNING_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QTimer>
#include <QFutureWatcher>
#include <QDateTime>
#include <functional>

#include "mastertimer.h"
#include "systemprobe.h"
#include "universe.h"
#include "doc.h"
#include "qlcui_export.h"

/** @addtogroup ui UI
 * @{
 */

/**
 * Settings ▸ Performance: how steadily this computer can run the 50 Hz DMX
 * timer, item by item, for the operating system it is (docs/features/
 * system-tuning.md).
 *
 * check() turns what was found (a Probe) into a list of Items -- each with a
 * status, one plain line on what it does, and either the in-app setting
 * that covers it or the exact command / steps for the user to do. The app
 * never runs anything that needs administrator rights itself.
 *
 * The one thing done in-process is "full speed during Operate" (macOS: no
 * App Nap, no idle sleep; Windows: no EcoQoS throttling, no idle sleep):
 * the runtime object (one per App) holds it while the show is in Operate,
 * and puts a "system.tuning" entry in Show Status when something marked
 * important is off then. It looks again every 15 seconds during Operate,
 * because a backup can start and a laptop can come unplugged mid-show --
 * the light look only, in-process and cheap. The full look, which runs
 * system tools, never runs in Operate (SystemProbe::toolsAllowed()): it
 * runs outside Operate once a minute, and Operate goes on with its last
 * answers. Either runs on a worker thread (probeHost()), never the UI's.
 */
class QLC_UI_EXPORT SystemTuning final : public QObject
{
    Q_OBJECT

public:
    enum Os { OsLinux, OsMac, OsWindows, OsOther };

    enum Status
    {
        Good,           ///< set up for a steady timer
        CanImprove,     ///< could be better: see the command / steps
        NotAvailable,   ///< not something this computer offers
        Advice          ///< nothing to detect: worth knowing
    };

    /** What was found. probe() fills it from the real system; a test fills
     *  it by hand for any operating system. */
    struct Probe
    {
        Os os = OsOther;
        MasterTimer::TimerScheduling scheduling = MasterTimer::SchedulingUnknown;
        QString schedulingText;
        /** Linux: RLIMIT_RTPRIO (SystemProbe::rtprioLimit()) */
        long rtprioLimit = -1;
        /** Linux: the distinct CPU frequency governors; empty when the
         *  system doesn't say (a VM or container) */
        QStringList governors;
        /** Linux: /sys/kernel/realtime -- 1 on a PREEMPT_RT kernel */
        bool preemptRt = false;
        /** Linux: the board, from the device tree ("Raspberry Pi 5 ...") */
        QString model;
        /** Linux: wireless network interfaces that are up */
        QStringList wirelessUp;
        /** The app's own binary (for setcap) */
        QString binary;
        /** macOS: Low Power Mode, 1 / 0 / -1 unknown */
        int lowPowerMode = -1;
        /** Windows: the power plan (SystemProbe::powerPlan()) */
        QString powerPlan;
        /** Whether this system has a full-speed hold (macOS, Windows) */
        bool fullSpeedSupported = false;
        /** The settings, as they stand */
        bool fullSpeedInOperate = true;
        bool keepDisplayAwake = false;

        /* The machine */
        QString arch;               ///< QSysInfo::currentCpuArchitecture()
        QString buildArch;          ///< what this build is for
        QString cpuName;
        int performanceCores = 0;
        int logicalCores = 0;
        bool translated = false;    ///< running under Rosetta

        /* The engine, with this show */
        int patchedUniverses = 0;
        Universe::ThreadScheduling universeScheduling = Universe::ThreadsNormal;
        MasterTimer::FrameTiming frames;
        double sendP99Ms = -1;      ///< tick to send, all universes; -1 none

        /* The app's own settings that cost speed */
        int rigOperateFps = 15;
        int rigSupersample = 2;
        bool rigGpu = true;

        /* Where the DMX goes */
        QStringList wirelessAddresses;  ///< this computer's on Wi-Fi
        QStringList dmxAddresses;       ///< the patched network outputs' own
        int networkOutputs = 0;         ///< patched Art-Net / sACN outputs
        QStringList outputProblems;     ///< what those report as failing
        int failingOutputs = 0;

        /* What the host is doing: probeHost() */
        int onBattery = -1;
        int batteryPercent = -1;
        int thermal = -1;
        int memory = -1;
        qint64 diskFreeMb = -1;
        int piThrottled = -1;
        int screenLock = -1;
        int idleMinutes = -1;
        int backupRunning = -1;
        int backupScheduled = -1;
        int autoUpdates = -1;
        bool rebootPending = false;
        bool processesKnown = false;
        bool deepLooked = false;    ///< the fields above that need a tool are real
        QList<SystemProbe::ProcessUse> processes;
    };

    struct Item
    {
        QString key;        ///< stable: "timer", "governor", "lowpower", ...
        Status status = Advice;
        QString title;      ///< "Real-time DMX timer"
        QString what;       ///< one plain line: what it does for the output
        QString state;      ///< what it is now ("SCHED_FIFO priority 89")
        QString command;    ///< a command to copy, when there is one
        QString howTo;      ///< steps, when a command isn't the whole answer
        /** Said in Show Status during Operate when it is CanImprove */
        bool important = false;
    };

    /** The real system, now. @a doc for its MasterTimer (may be null). */
    static Probe probe(const Doc *doc);
    /** The part that is quick and reads the Doc: UI thread */
    static Probe probeDoc(const Doc *doc);
    /** The part that may run system tools: any thread, under a second. With
     *  @a deep false, or while SystemProbe::toolsAllowed() is off (Operate),
     *  only what costs no new process. */
    static void probeHost(Probe *p, bool deep);
    /** When the last full (deep) look finished; invalid when none has */
    static QDateTime lastFullCheck();
    /** What @a p means, item by item, in a fixed order. */
    static QList<Item> check(const Probe &p);
    /** The items of @a items that are important and CanImprove */
    static QList<Item> importantProblems(const QList<Item> &items);

    static QString statusText(Status status);

    /*********************************************************************
     * Settings (this computer's: QSettings)
     *********************************************************************/
    /** Hold full speed during Operate (default on) */
    static bool fullSpeedInOperate();
    static void setFullSpeedInOperate(bool on);
    /** Keep the display awake too during Operate (default on) */
    static bool keepDisplayAwakeInOperate();
    static void setKeepDisplayAwakeInOperate(bool on);
    /** Say it in Show Status during Operate when an important item is off
     *  (default on) */
    static bool warnInOperate();
    static void setWarnInOperate(bool on);

    /*********************************************************************
     * Runtime: one, made by App
     *********************************************************************/
    explicit SystemTuning(Doc *doc, QObject *parent = nullptr);
    ~SystemTuning() override;

    /** The App's, or null */
    static SystemTuning *instance();

    /** Settings changed (Settings ▸ Performance OK): hold / let go and
     *  update Show Status for the current mode. */
    void apply();

    /** Whether the full-speed hold is on, as this object asked for it */
    bool isHeld() const { return m_held; }

    /** Tests: what probe() returns, and what holding calls */
    void setProbe(const std::function<Probe()> &probeFn);
    void setHold(const std::function<void(bool on, bool keepDisplayAwake)> &holdFn);
    /** Tests: run this on the worker in place of probeHost() */
    void setHostGather(const std::function<void(Probe *, bool)> &gatherFn);
    /** Tests: one look, as the timer makes: light in Operate, full outside */
    void recheckNow();

    /** A full look's answers are kept; a light one gets the last full
     *  look's answers for what it didn't read (Settings ▸ Performance too) */
    void rememberLook(Probe *p);

    /** The Show Status key */
    static QString statusKey();

private slots:
    void slotModeChanged(Doc::Mode mode);

private:
    /** The items for what the host is doing (systemtuninghost.cpp) */
    static void appendHostItems(const Probe &p, QList<Item> *items);
    static void noteFullCheck();
    void updateStatus();
    void publish(Probe p);

private:
    Doc *m_doc;
    bool m_held = false;
    std::function<Probe()> m_probe;
    std::function<void(bool, bool)> m_hold;
    /** Empty: everything comes from m_probe, at once (tests, offscreen) */
    std::function<void(Probe *, bool)> m_gather;
    QFutureWatcher<Probe> m_watcher;
    bool m_gatherAgain = false;
    QTimer m_recheck;
    bool m_operateLooks = false;    ///< m_recheck runs at Operate's pace
    MasterTimer::FrameTiming m_baseline;
    Probe m_deep;   ///< the last deep look: what the light ones don't re-read
};

/** @} */

#endif

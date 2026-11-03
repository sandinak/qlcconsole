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
#include <functional>

#include "mastertimer.h"
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
 * important is off then.
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
    /** Keep the display awake too during Operate (default off) */
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

    /** The Show Status key */
    static QString statusKey();

private slots:
    void slotModeChanged(Doc::Mode mode);

private:
    void updateStatus();

private:
    Doc *m_doc;
    bool m_held = false;
    std::function<Probe()> m_probe;
    std::function<void(bool, bool)> m_hold;
};

/** @} */

#endif

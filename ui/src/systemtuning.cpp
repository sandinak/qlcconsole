/*
  Q Light Controller Plus - qlcconsole
  systemtuning.cpp

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

#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QDir>

#include "systemtuning.h"
#include "systemprobe.h"
#include "showstatus.h"

#define KEY_FULL_SPEED   QStringLiteral("tuning/fullSpeedInOperate")
#define KEY_DISPLAY      QStringLiteral("tuning/keepDisplayAwakeInOperate")
#define KEY_WARN         QStringLiteral("tuning/warnInOperate")

namespace
{
SystemTuning *s_instance = nullptr;

QString readLine(const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly) == false)
        return QString();
    /* device-tree strings end in a NUL */
    return QString::fromUtf8(f.readAll()).remove(QChar(0)).trimmed();
}
}

/****************************************************************************
 * Probe
 ****************************************************************************/

SystemTuning::Probe SystemTuning::probe(const Doc *doc)
{
    Probe p;
#if defined(Q_OS_LINUX)
    p.os = OsLinux;
#elif defined(Q_OS_MACOS)
    p.os = OsMac;
#elif defined(Q_OS_WIN)
    p.os = OsWindows;
#endif
    if (doc != nullptr && doc->masterTimer() != nullptr)
    {
        p.scheduling = doc->masterTimer()->timerScheduling();
        p.schedulingText = doc->masterTimer()->timerSchedulingText();
    }
    p.binary = QCoreApplication::applicationFilePath();
    p.fullSpeedSupported = SystemProbe::canHoldFullSpeed();
    p.fullSpeedInOperate = fullSpeedInOperate();

#if defined(Q_OS_LINUX)
    p.rtprioLimit = SystemProbe::rtprioLimit();
    const QDir cpus(QStringLiteral("/sys/devices/system/cpu"));
    for (const QString &cpu : cpus.entryList(QStringList(QStringLiteral("cpu[0-9]*")), QDir::Dirs))
    {
        const QString g = readLine(cpus.filePath(cpu + QStringLiteral("/cpufreq/scaling_governor")));
        if (g.isEmpty() == false && p.governors.contains(g) == false)
            p.governors << g;
    }
    p.preemptRt = readLine(QStringLiteral("/sys/kernel/realtime")) == QLatin1String("1");
    p.model = readLine(QStringLiteral("/proc/device-tree/model"));
    const QDir net(QStringLiteral("/sys/class/net"));
    for (const QString &dev : net.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        if (QFile::exists(net.filePath(dev + QStringLiteral("/wireless")))
            && readLine(net.filePath(dev + QStringLiteral("/operstate"))) == QLatin1String("up"))
            p.wirelessUp << dev;
    }
#elif defined(Q_OS_MACOS)
    p.lowPowerMode = SystemProbe::lowPowerMode();
#elif defined(Q_OS_WIN)
    p.powerPlan = SystemProbe::powerPlan();
#endif
    return p;
}

/****************************************************************************
 * Check
 ****************************************************************************/

QString SystemTuning::statusText(Status status)
{
    switch (status)
    {
        case Good:         return tr("OK");
        case CanImprove:   return tr("Can improve");
        case NotAvailable: return tr("Not available");
        case Advice:       return tr("Worth knowing");
    }
    return QString();
}

QList<SystemTuning::Item> SystemTuning::check(const Probe &p)
{
    QList<Item> items;

    /* The DMX timer thread: every system has one */
    {
        Item it;
        it.key = QStringLiteral("timer");
        it.title = tr("Real-time DMX timer");
        it.what = tr("The timer that sends every DMX frame runs ahead of everything else, "
                     "so frames leave on time even when the computer is busy.");
        it.state = p.schedulingText;
        switch (p.scheduling)
        {
            case MasterTimer::SchedulingRealtime:
                it.status = Good;
            break;
            case MasterTimer::SchedulingUnknown:
                it.status = Advice;
                it.state = tr("not running yet");
            break;
            case MasterTimer::SchedulingNormal:
                it.status = NotAvailable;
            break;
            case MasterTimer::SchedulingRefused:
                it.status = CanImprove;
                it.important = true;
                if (p.os == OsLinux)
                {
                    it.what += QLatin1Char(' ')
                            + tr("Linux allows it only to a user with a real-time limit; "
                                 "on a Raspberry Pi 5 it gave 3 to 15 times fewer late frames.");
                    it.command = QStringLiteral(
                        "echo '@audio - rtprio 95' | sudo tee /etc/security/limits.d/95-qlcconsole.conf\n"
                        "sudo usermod -aG audio \"$USER\"");
                    it.howTo = tr("Run these once, then log out and back in. Or give the program "
                                  "itself the right (repeat after each update):\n"
                                  "sudo setcap cap_sys_nice=eip '%1'").arg(p.binary);
                }
                else if (p.os == OsWindows)
                {
                    it.howTo = tr("The Multimedia Class Scheduler service gives the timer its "
                                  "priority. Check it is running: Services (services.msc) ▸ "
                                  "Multimedia Class Scheduler ▸ Startup type Automatic.");
                }
                else
                {
                    it.howTo = tr("macOS refused the timer's real-time policy. Quit and open "
                                  "the app again; if it stays refused, report it (Help ▸ "
                                  "Report a Bug) with this line.");
                }
            break;
        }
        if (p.os == OsLinux && p.rtprioLimit != -1)
        {
            const QString limit = p.rtprioLimit == -2 ? tr("unlimited")
                                                      : QString::number(p.rtprioLimit);
            it.state += (it.state.isEmpty() ? QString() : QStringLiteral("; "))
                     + tr("real-time limit %1").arg(limit);
        }
        items << it;
    }

    /* Full speed during Operate: the one thing done in the app */
    if (p.os == OsMac || p.os == OsWindows)
    {
        Item it;
        it.key = QStringLiteral("fullspeed");
        it.title = p.os == OsMac ? tr("No App Nap during Operate")
                                 : tr("No power throttling during Operate");
        it.what = p.os == OsMac
                ? tr("Behind other windows or with the screen asleep, macOS slows an app's timers "
                     "and input to save power. While the show is in Operate the app asks it not "
                     "to, and the Mac doesn't idle to sleep.")
                : tr("Windows 11 slows an app it thinks is in the background (EcoQoS). While the "
                     "show is in Operate the app opts out, and the PC doesn't idle to sleep.");
        if (p.fullSpeedSupported == false)
        {
            it.status = NotAvailable;
        }
        else if (p.fullSpeedInOperate)
        {
            it.status = Good;
            it.state = tr("on (setting above)");
        }
        else
        {
            it.status = CanImprove;
            it.state = tr("off (setting above)");
            it.howTo = tr("Tick \"Run at full speed\" above.");
        }
        items << it;
    }

    if (p.os == OsLinux)
    {
        /* CPU governor */
        {
            Item it;
            it.key = QStringLiteral("governor");
            it.title = tr("CPU speed (governor)");
            it.what = tr("\"performance\" keeps the processor at full clock. Others slow it down "
                         "when idle and need a moment to speed up again when a busy frame comes.");
            it.state = p.governors.join(QStringLiteral(", "));
            if (p.governors.isEmpty())
            {
                it.status = NotAvailable;
                it.state = tr("set by the host (virtual machine or container)");
            }
            else if (p.governors == QStringList(QStringLiteral("performance")))
            {
                it.status = Good;
            }
            else
            {
                it.status = CanImprove;
                it.command = QStringLiteral(
                    "echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor");
                it.howTo = tr("This lasts until the next restart. To keep it, install cpufrequtils "
                              "and set GOVERNOR=\"performance\" in /etc/default/cpufrequtils. "
                              "It uses more power and runs warmer.");
            }
            items << it;
        }

        /* Wi-Fi */
        {
            Item it;
            it.key = QStringLiteral("wifi");
            it.title = tr("Wi-Fi power saving");
            it.what = tr("Wi-Fi power saving holds packets back for up to a few hundred "
                         "milliseconds. Art-Net or sACN over Wi-Fi then arrives in bursts.");
            if (p.wirelessUp.isEmpty())
            {
                it.status = Good;
                it.state = tr("no Wi-Fi in use");
            }
            else
            {
                it.status = CanImprove;
                it.state = tr("Wi-Fi up: %1").arg(p.wirelessUp.join(QStringLiteral(", ")));
                QStringList cmds;
                for (const QString &dev : p.wirelessUp)
                    cmds << QStringLiteral("sudo iw dev %1 set power_save off").arg(dev);
                it.command = cmds.join(QLatin1Char('\n'));
                it.howTo = tr("Best: send DMX over wired Ethernet. The command lasts until the "
                              "next restart; NetworkManager keeps it with "
                              "wifi.powersave = 2 in /etc/NetworkManager/conf.d/.");
            }
            items << it;
        }

        /* The kernel */
        {
            Item it;
            it.key = QStringLiteral("kernel");
            it.title = tr("Real-time kernel (PREEMPT_RT)");
            it.what = tr("A real-time kernel bounds how long anything can hold the timer up. "
                         "Not needed for 50 Hz DMX with the real-time timer above; consider it "
                         "only when late frames remain.");
            it.status = p.preemptRt ? Good : Advice;
            it.state = p.preemptRt ? tr("real-time kernel") : tr("standard kernel");
            items << it;
        }

        /* The Raspberry Pi */
        if (p.model.contains(QLatin1String("Raspberry Pi")))
        {
            Item it;
            it.key = QStringLiteral("pi");
            it.title = tr("Raspberry Pi power and heat");
            it.what = tr("A Pi with a weak power supply or no cooling slows itself down without "
                         "warning. Overclocking is not needed.");
            it.status = Advice;
            it.state = p.model;
            it.command = QStringLiteral("vcgencmd get_throttled");
            it.howTo = tr("It should print throttled=0x0. Anything else: use the official power "
                          "supply and a fan or heatsink case.");
            items << it;
        }
    }
    else if (p.os == OsMac)
    {
        Item it;
        it.key = QStringLiteral("lowpower");
        it.title = tr("Low Power Mode");
        it.what = tr("Low Power Mode lowers the processor's speed for battery life.");
        if (p.lowPowerMode == 1)
        {
            it.status = CanImprove;
            it.important = true;
            it.state = tr("on");
            it.howTo = tr("System Settings ▸ Battery ▸ Low Power Mode: Never "
                          "(or Only on Battery, and keep the Mac plugged in).");
        }
        else if (p.lowPowerMode == 0)
        {
            it.status = Good;
            it.state = tr("off");
        }
        else
        {
            it.status = NotAvailable;
        }
        items << it;
    }
    else if (p.os == OsWindows)
    {
        Item it;
        it.key = QStringLiteral("powerplan");
        it.title = tr("Power plan");
        it.what = tr("A power-saving plan lowers the processor's speed and parks cores; "
                     "High performance keeps them ready.");
        it.command = QStringLiteral("powercfg /setactive SCHEME_MIN");
        it.howTo = tr("Or: Control Panel ▸ Power Options ▸ High performance. On a laptop "
                      "also set Settings ▸ System ▸ Power & battery ▸ Power mode: Best performance, "
                      "and keep it plugged in.");
        if (p.powerPlan == QLatin1String("high"))
        {
            it.status = Good;
            it.state = tr("High performance");
            it.command.clear();
            it.howTo.clear();
        }
        else if (p.powerPlan == QLatin1String("saver"))
        {
            it.status = CanImprove;
            it.important = true;
            it.state = tr("Power saver");
        }
        else if (p.powerPlan == QLatin1String("balanced"))
        {
            it.status = CanImprove;
            it.state = tr("Balanced");
        }
        else if (p.powerPlan == QLatin1String("other"))
        {
            it.status = Advice;
            it.state = tr("a custom plan (Ultimate Performance is one)");
        }
        else
        {
            it.status = NotAvailable;
            it.command.clear();
            it.howTo.clear();
        }
        items << it;
    }

    return items;
}

QList<SystemTuning::Item> SystemTuning::importantProblems(const QList<Item> &items)
{
    QList<Item> out;
    for (const Item &it : items)
        if (it.important && it.status == CanImprove)
            out << it;
    return out;
}

/****************************************************************************
 * Settings
 ****************************************************************************/

bool SystemTuning::fullSpeedInOperate()
{
    return QSettings().value(KEY_FULL_SPEED, true).toBool();
}

void SystemTuning::setFullSpeedInOperate(bool on)
{
    QSettings().setValue(KEY_FULL_SPEED, on);
}

bool SystemTuning::keepDisplayAwakeInOperate()
{
    return QSettings().value(KEY_DISPLAY, false).toBool();
}

void SystemTuning::setKeepDisplayAwakeInOperate(bool on)
{
    QSettings().setValue(KEY_DISPLAY, on);
}

bool SystemTuning::warnInOperate()
{
    return QSettings().value(KEY_WARN, true).toBool();
}

void SystemTuning::setWarnInOperate(bool on)
{
    QSettings().setValue(KEY_WARN, on);
}

/****************************************************************************
 * Runtime
 ****************************************************************************/

SystemTuning::SystemTuning(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    Q_ASSERT(doc != nullptr);
    m_probe = [this]() { return probe(m_doc); };
    m_hold = [](bool on, bool keepDisplayAwake) {
        SystemProbe::setFullSpeed(on, keepDisplayAwake,
                                  QStringLiteral("qlcconsole: show in Operate, sending DMX"));
    };
    connect(m_doc, &Doc::modeChanged, this, &SystemTuning::slotModeChanged);
    if (s_instance == nullptr)
        s_instance = this;
    apply();
}

SystemTuning::~SystemTuning()
{
    if (m_held && m_hold)
        m_hold(false, false);
    if (s_instance == this)
        s_instance = nullptr;
}

SystemTuning *SystemTuning::instance()
{
    return s_instance;
}

QString SystemTuning::statusKey()
{
    return QStringLiteral("system.tuning");
}

void SystemTuning::setProbe(const std::function<Probe()> &probeFn)
{
    m_probe = probeFn;
}

void SystemTuning::setHold(const std::function<void(bool, bool)> &holdFn)
{
    m_hold = holdFn;
}

void SystemTuning::slotModeChanged(Doc::Mode mode)
{
    Q_UNUSED(mode)
    apply();
}

void SystemTuning::apply()
{
    const bool operate = m_doc->mode() == Doc::Operate;
    const bool want = operate && fullSpeedInOperate();
    if (want || m_held)
    {
        /* Again when held, too: the display setting may have changed */
        m_held = want;
        if (m_hold)
            m_hold(want, want && keepDisplayAwakeInOperate());
    }
    updateStatus();
}

void SystemTuning::updateStatus()
{
    ShowStatus *status = ShowStatus::instance();
    if (m_doc->mode() != Doc::Operate || warnInOperate() == false || !m_probe)
    {
        status->clearStatus(statusKey());
        return;
    }
    const QList<Item> problems = importantProblems(check(m_probe()));
    if (problems.isEmpty())
    {
        status->clearStatus(statusKey());
        return;
    }
    QStringList lines;
    for (const Item &it : problems)
        lines << QStringLiteral("%1: %2").arg(it.title, it.state);
    status->setStatus(statusKey(), ShowStatus::Warning,
                      tr("This computer isn't set up for steady DMX timing"),
                      tr("Frames may leave late or unevenly. Settings ▸ Performance says what "
                         "to change; this can be turned off there."),
                      lines);
}

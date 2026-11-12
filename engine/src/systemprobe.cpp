/*
  Q Light Controller Plus - qlcconsole
  systemprobe.cpp

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
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QThread>

#if defined(Q_OS_LINUX)
#include <sys/resource.h>
#include <sched.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#include <tlhelp32.h>
#elif defined(Q_OS_MACOS)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#endif

#include "systemprobe.h"

#if defined(Q_OS_MACOS)
namespace SystemProbeMac
{
int lowPowerMode();
int thermalState();
int onBattery(int *percent);
void *beginActivity(const QString &reason, bool keepDisplayAwake);
void endActivity(void *token);
}
#endif

namespace
{
bool s_held = false;
#if defined(Q_OS_MACOS)
void *s_token = nullptr;
#endif

#if defined(Q_OS_WIN)
/* Resolved at run time: SetProcessInformation() does not exist before
   Windows 8, PowerGetActiveScheme() lives in powrprof.dll (not linked). */
typedef BOOL (WINAPI *SetProcessInformationFn)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
typedef DWORD (WINAPI *PowerGetActiveSchemeFn)(HKEY, GUID **);

#ifndef PROCESS_POWER_THROTTLING_EXECUTION_SPEED
#define PROCESS_POWER_THROTTLING_EXECUTION_SPEED 0x1
#endif
#ifndef PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION
#define PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION 0x4
#endif

/* EcoQoS: Windows 11 runs a process it thinks is in the background on the
   efficiency cores at a low clock. Opting out is per process and is undone
   by handing the decision back to the system. */
void setEcoQoSAllowed(bool allowed)
{
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (k32 == NULL)
        return;
    SetProcessInformationFn fn = (SetProcessInformationFn)(void *)GetProcAddress(k32, "SetProcessInformation");
    if (fn == NULL)
        return;
    PROCESS_POWER_THROTTLING_STATE state;
    ZeroMemory(&state, sizeof(state));
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    /* ControlMask set + StateMask clear: never throttle. Clear in both: the
       system decides again. The call sets the whole state, so it keeps what
       App::disableTimerResolutionThrottling() set at start: timer
       resolution requests are always honoured. */
    state.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION
                        | (allowed ? 0 : PROCESS_POWER_THROTTLING_EXECUTION_SPEED);
    state.StateMask = 0;
    if (fn(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state)) == FALSE)
        qDebug() << "SystemProbe: power throttling not changed:" << GetLastError();
}
#endif
}

int SystemProbe::lowPowerMode()
{
#if defined(Q_OS_MACOS)
    return SystemProbeMac::lowPowerMode();
#else
    return -1;
#endif
}

long SystemProbe::rtprioLimit()
{
#if defined(Q_OS_LINUX)
    struct rlimit rl;
    if (getrlimit(RLIMIT_RTPRIO, &rl) != 0)
        return -1;
    if (rl.rlim_cur == RLIM_INFINITY)
        return -2;
    return long(rl.rlim_cur);
#else
    return -1;
#endif
}

QString SystemProbe::powerPlan()
{
#if defined(Q_OS_WIN)
    HMODULE lib = LoadLibraryW(L"powrprof.dll");
    if (lib == NULL)
        return QString();
    QString plan;
    PowerGetActiveSchemeFn fn = (PowerGetActiveSchemeFn)(void *)GetProcAddress(lib, "PowerGetActiveScheme");
    GUID *active = NULL;
    if (fn != NULL && fn(NULL, &active) == ERROR_SUCCESS && active != NULL)
    {
        /* The three stock plans (GUID_MIN_POWER_SAVINGS, GUID_TYPICAL_POWER_SAVINGS,
           GUID_MAX_POWER_SAVINGS); anything else is a custom plan */
        static const GUID highPerf = { 0x8c5e7fda, 0xe8bf, 0x4a96, { 0x9a, 0x85, 0xa6, 0xe2, 0x3a, 0x8c, 0x63, 0x5c } };
        static const GUID balanced = { 0x381b4222, 0xf694, 0x41f0, { 0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e } };
        static const GUID saver    = { 0xa1841308, 0x3541, 0x4fab, { 0xbc, 0x81, 0xf7, 0x15, 0x56, 0xf2, 0x0b, 0x4a } };
        if (IsEqualGUID(*active, highPerf))
            plan = QStringLiteral("high");
        else if (IsEqualGUID(*active, balanced))
            plan = QStringLiteral("balanced");
        else if (IsEqualGUID(*active, saver))
            plan = QStringLiteral("saver");
        else
            plan = QStringLiteral("other");
        LocalFree(active);
    }
    FreeLibrary(lib);
    return plan;
#else
    return QString();
#endif
}

bool SystemProbe::canHoldFullSpeed()
{
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    return true;
#else
    return false;
#endif
}

void SystemProbe::setFullSpeed(bool on, bool keepDisplayAwake, const QString &reason)
{
#if defined(Q_OS_MACOS)
    SystemProbeMac::endActivity(s_token);
    s_token = on ? SystemProbeMac::beginActivity(reason, keepDisplayAwake) : nullptr;
    s_held = (s_token != nullptr);
#elif defined(Q_OS_WIN)
    Q_UNUSED(reason)
    setEcoQoSAllowed(!on);
    EXECUTION_STATE es = ES_CONTINUOUS;
    if (on)
        es |= ES_SYSTEM_REQUIRED | (keepDisplayAwake ? ES_DISPLAY_REQUIRED : 0);
    SetThreadExecutionState(es);
    s_held = on;
#else
    Q_UNUSED(on)
    Q_UNUSED(keepDisplayAwake)
    Q_UNUSED(reason)
    s_held = false;
#endif
}

bool SystemProbe::fullSpeedHeld()
{
    return s_held;
}

/*********************************************************************
 * The cores display work may use
 *********************************************************************/

int SystemProbe::performanceCores()
{
    static const int cores = []() {
        int n = 0;
#if defined(Q_OS_MACOS)
        /* Apple silicon: the performance cluster. hw.ncpu counts the
           efficiency cores too, which a busy pool would fill and slow
           everything else on the machine. Intel Macs have no perflevels. */
        int v = 0;
        size_t len = sizeof(v);
        if (sysctlbyname("hw.perflevel0.logicalcpu", &v, &len, nullptr, 0) == 0 && v > 0)
            n = v;
#elif defined(Q_OS_LINUX)
        /* The CPUs this process may run on: isolcpus and a cgroup's cpuset
           are already left out of the affinity mask. On big.LITTLE (ARM
           boards, Intel hybrids) only the biggest: cpu_capacity, where the
           kernel reports it. */
        cpu_set_t set;
        CPU_ZERO(&set);
        if (sched_getaffinity(0, sizeof(set), &set) == 0)
        {
            int best = -1, atBest = 0, all = 0;
            for (int c = 0; c < CPU_SETSIZE; ++c)
            {
                if (!CPU_ISSET(c, &set))
                    continue;
                ++all;
                QFile f(QStringLiteral("/sys/devices/system/cpu/cpu%1/cpu_capacity").arg(c));
                int cap = -1;
                if (f.open(QIODevice::ReadOnly))
                    cap = f.readAll().trimmed().toInt();
                if (cap > best)
                {
                    best = cap;
                    atBest = 1;
                }
                else if (cap == best)
                    ++atBest;
            }
            n = (best > 0) ? atBest : all;
        }
#elif defined(Q_OS_WIN)
        /* Hybrid CPUs (12th-gen Intel and later): the highest efficiency
           class is the performance cores. Without classes, every core. */
        DWORD bytes = 0;
        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);
        if (bytes > 0)
        {
            QByteArray buf(int(bytes), Qt::Uninitialized);
            auto *info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buf.data());
            if (GetLogicalProcessorInformationEx(RelationProcessorCore, info, &bytes))
            {
                int best = -1, atBest = 0;
                for (DWORD off = 0; off < bytes; )
                {
                    auto *p = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buf.data() + off);
                    int logical = 0;
                    for (WORD g = 0; g < p->Processor.GroupCount; ++g)
                    {
                        KAFFINITY m = p->Processor.GroupMask[g].Mask;
                        while (m) { logical += int(m & 1); m >>= 1; }
                    }
                    const int cls = p->Processor.EfficiencyClass;
                    if (cls > best) { best = cls; atBest = logical; }
                    else if (cls == best) atBest += logical;
                    off += p->Size;
                }
                n = atBest;
            }
        }
#endif
        if (n <= 0)
            n = QThread::idealThreadCount();
        return qMax(1, n);
    }();
    return cores;
}

/*********************************************************************
 * What else is going on
 *********************************************************************/

#if !defined(Q_OS_WIN)
QString SystemProbe::runTool(const QString &program, const QStringList &args, const QDeadlineTimer &deadline)
{
    if (deadline.hasExpired())
        return QString();
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    /* The lowest priority there is, so the show always comes first: 20 on
       macOS, Linux stops at 19. Between fork and exec, so only this call. */
    p.setChildProcessModifier([]() { (void)setpriority(PRIO_PROCESS, 0, 20); });
    p.start(program, args, QIODevice::ReadOnly);
    if (p.waitForFinished(int(qMax<qint64>(1, deadline.remainingTime()))) == false)
    {
        p.kill();
        p.waitForFinished(100);
        return QString();
    }
    return QString::fromUtf8(p.readAllStandardOutput());
}
#endif

namespace
{
#if defined(Q_OS_LINUX)
QString readText(const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly) == false)
        return QString();
    return QString::fromUtf8(f.readAll()).remove(QChar(0)).trimmed();
}
#endif

#if defined(Q_OS_LINUX) || defined(Q_OS_WIN)
struct CpuSample
{
    QString name;
    qint64 cpuMs = 0;
};

/** Each process's total CPU time so far, by pid, leaving this one out */
QHash<qint64, CpuSample> sampleProcesses()
{
    QHash<qint64, CpuSample> out;
#if defined(Q_OS_LINUX)
    const qint64 self = qint64(getpid());
    const long hz = sysconf(_SC_CLK_TCK);
    const QDir proc(QStringLiteral("/proc"));
    const QStringList entries = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries)
    {
        bool ok = false;
        const qint64 pid = entry.toLongLong(&ok);
        if (ok == false || pid == self)
            continue;
        QString name;
        qint64 ticks = 0;
        if (SystemProbe::parseProcStat(readText(proc.filePath(entry + QStringLiteral("/stat"))), &name, &ticks))
        {
            CpuSample s;
            s.name = name;
            s.cpuMs = ticks * 1000 / qMax(1L, hz);
            out.insert(pid, s);
        }
    }
#else
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return out;
    PROCESSENTRY32W e;
    ZeroMemory(&e, sizeof(e));
    e.dwSize = sizeof(e);
    for (BOOL more = Process32FirstW(snap, &e); more; more = Process32NextW(snap, &e))
    {
        if (e.th32ProcessID == 0 || e.th32ProcessID == GetCurrentProcessId())
            continue;
        CpuSample s;
        s.name = QString::fromWCharArray(e.szExeFile);
        /* A system process won't open: its name still counts */
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
        if (h != NULL)
        {
            FILETIME created, exited, kernel, user;
            if (GetProcessTimes(h, &created, &exited, &kernel, &user))
            {
                const auto ticks = [](const FILETIME &f) {
                    return (qint64(f.dwHighDateTime) << 32) | qint64(f.dwLowDateTime);
                };
                s.cpuMs = (ticks(kernel) + ticks(user)) / 10000;
            }
            CloseHandle(h);
        }
        out.insert(qint64(e.th32ProcessID), s);
    }
    CloseHandle(snap);
#endif
    return out;
}
#endif
}

QString SystemProbe::cpuName()
{
#if defined(Q_OS_MACOS)
    char buf[256];
    size_t len = sizeof(buf);
    if (sysctlbyname("machdep.cpu.brand_string", buf, &len, nullptr, 0) == 0 && len > 0)
        return QString::fromUtf8(buf).trimmed();
    return QString();
#elif defined(Q_OS_LINUX)
    const QStringList lines = readText(QStringLiteral("/proc/cpuinfo")).split(QLatin1Char('\n'));
    for (const QString &line : lines)
        if (line.startsWith(QLatin1String("model name")))
            return line.section(QLatin1Char(':'), 1).trimmed();
    return QString();
#elif defined(Q_OS_WIN)
    return QSettings(QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"),
                     QSettings::NativeFormat).value(QStringLiteral("ProcessorNameString")).toString().trimmed();
#else
    return QString();
#endif
}

bool SystemProbe::translated()
{
#if defined(Q_OS_MACOS)
    int v = 0;
    size_t len = sizeof(v);
    return sysctlbyname("sysctl.proc_translated", &v, &len, nullptr, 0) == 0 && v == 1;
#else
    return false;
#endif
}

int SystemProbe::onBattery(int *percent)
{
#if defined(Q_OS_MACOS)
    return SystemProbeMac::onBattery(percent);
#elif defined(Q_OS_WIN)
    if (percent != nullptr)
        *percent = -1;
    SYSTEM_POWER_STATUS s;
    if (GetSystemPowerStatus(&s) == FALSE || s.ACLineStatus == 255)
        return -1;
    if (percent != nullptr && s.BatteryLifePercent <= 100)
        *percent = int(s.BatteryLifePercent);
    return s.ACLineStatus == 0 ? 1 : 0;
#elif defined(Q_OS_LINUX)
    if (percent != nullptr)
        *percent = -1;
    const QDir supplies(QStringLiteral("/sys/class/power_supply"));
    int result = 0;
    const QStringList names = supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dev : names)
    {
        if (readText(supplies.filePath(dev + QStringLiteral("/type"))) != QLatin1String("Battery"))
            continue;
        if (readText(supplies.filePath(dev + QStringLiteral("/status"))) == QLatin1String("Discharging"))
            result = 1;
        bool ok = false;
        const int charge = readText(supplies.filePath(dev + QStringLiteral("/capacity"))).toInt(&ok);
        if (ok && percent != nullptr)
            *percent = charge;
    }
    return result;
#else
    if (percent != nullptr)
        *percent = -1;
    return -1;
#endif
}

int SystemProbe::thermalState()
{
#if defined(Q_OS_MACOS)
    return SystemProbeMac::thermalState();
#elif defined(Q_OS_LINUX)
    /* The kernel starts slowing the processor at a zone's passive trip point */
    const QDir zones(QStringLiteral("/sys/class/thermal"));
    int state = -1;
    const QStringList names = zones.entryList(QStringList(QStringLiteral("thermal_zone*")), QDir::Dirs);
    for (const QString &zone : names)
    {
        const QDir z(zones.filePath(zone));
        bool ok = false;
        const int temp = readText(z.filePath(QStringLiteral("temp"))).toInt(&ok);
        if (ok == false)
            continue;
        state = qMax(state, 0);
        const QStringList trips = z.entryList(QStringList(QStringLiteral("trip_point_*_type")), QDir::Files);
        for (const QString &trip : trips)
        {
            const QString type = readText(z.filePath(trip));
            if (type != QLatin1String("passive") && type != QLatin1String("hot")
                && type != QLatin1String("critical"))
                continue;
            QString valueFile = trip;
            valueFile.replace(QLatin1String("_type"), QLatin1String("_temp"));
            const int limit = readText(z.filePath(valueFile)).toInt(&ok);
            if (ok == false || limit <= 0)
                continue;
            if (temp >= limit)
                state = qMax(state, type == QLatin1String("passive") ? 2 : 3);
            else if (temp >= limit - 5000)
                state = qMax(state, 1);
        }
    }
    return state;
#else
    return -1;
#endif
}

int SystemProbe::memoryPressure()
{
#if defined(Q_OS_MACOS)
    int level = 0;
    size_t len = sizeof(level);
    if (sysctlbyname("kern.memorystatus_vm_pressure_level", &level, &len, nullptr, 0) != 0)
        return -1;
    return level >= 4 ? 2 : (level == 2 ? 1 : 0);
#elif defined(Q_OS_LINUX)
    qint64 total = 0, available = -1;
    const QStringList lines = readText(QStringLiteral("/proc/meminfo")).split(QLatin1Char('\n'));
    for (const QString &line : lines)
    {
        const qint64 kb = line.section(QLatin1Char(':'), 1).trimmed().section(QLatin1Char(' '), 0, 0).toLongLong();
        if (line.startsWith(QLatin1String("MemTotal:")))
            total = kb;
        else if (line.startsWith(QLatin1String("MemAvailable:")))
            available = kb;
    }
    if (total <= 0 || available < 0)
        return -1;
    const qint64 percent = available * 100 / total;
    return percent < 5 ? 2 : (percent < 10 ? 1 : 0);
#elif defined(Q_OS_WIN)
    MEMORYSTATUSEX m;
    m.dwLength = sizeof(m);
    if (GlobalMemoryStatusEx(&m) == FALSE)
        return -1;
    return m.dwMemoryLoad > 95 ? 2 : (m.dwMemoryLoad > 90 ? 1 : 0);
#else
    return -1;
#endif
}

QList<SystemProbe::ProcessUse> SystemProbe::processes(const QDeadlineTimer &deadline)
{
#if defined(Q_OS_MACOS)
    return parseProcessTable(runTool(QStringLiteral("/bin/ps"),
                                     QStringList() << QStringLiteral("-Axco") << QStringLiteral("pid,pcpu,comm"),
                                     deadline),
                             QCoreApplication::applicationPid());
#elif defined(Q_OS_LINUX) || defined(Q_OS_WIN)
    /* The system keeps only totals: use is the difference between two looks.
       The very first look has nothing to compare with, so it waits a moment. */
    static QMutex mutex;
    static QHash<qint64, CpuSample> last;
    static QElapsedTimer since;
    static QList<ProcessUse> lastResult;

    QMutexLocker locker(&mutex);
    if (since.isValid() == false)
    {
        last = sampleProcesses();
        since.start();
        QThread::msleep(ulong(qBound(qint64(1), qint64(deadline.remainingTime()), qint64(250))));
    }
    else if (since.elapsed() < 100)
    {
        return lastResult;
    }
    const QHash<qint64, CpuSample> now = sampleProcesses();
    const double elapsedMs = qMax<qint64>(1, since.restart());
    QList<ProcessUse> out;
    for (auto it = now.constBegin(); it != now.constEnd(); ++it)
    {
        ProcessUse use;
        use.name = it.value().name;
        const auto before = last.constFind(it.key());
        if (before != last.constEnd())
            use.cpu = double(it.value().cpuMs - before.value().cpuMs) * 100.0 / elapsedMs;
        out << use;
    }
    last = now;
    lastResult = out;
    return out;
#else
    Q_UNUSED(deadline)
    return QList<ProcessUse>();
#endif
}

int SystemProbe::screenLock(int *idleMinutes, const QDeadlineTimer &deadline)
{
    *idleMinutes = -1;
#if defined(Q_OS_MACOS)
    const int display = parseIdleMinutes(runTool(QStringLiteral("/usr/bin/pmset"),
                                                 QStringList() << QStringLiteral("-g"), deadline));
    bool ok = false;
    const int saverSeconds = runTool(QStringLiteral("/usr/bin/defaults"),
                                     QStringList() << QStringLiteral("-currentHost") << QStringLiteral("read")
                                                   << QStringLiteral("com.apple.screensaver") << QStringLiteral("idleTime"),
                                     deadline).trimmed().toInt(&ok);
    const int saver = ok ? (saverSeconds + 59) / 60 : -1;
    /* Whichever comes first; 0 means that one never does */
    if (display > 0 && saver > 0)
        *idleMinutes = qMin(display, saver);
    else if (display >= 0 || saver >= 0)
        *idleMinutes = qMax(display, saver);
    return parseScreenLock(runTool(QStringLiteral("/usr/sbin/sysadminctl"),
                                   QStringList() << QStringLiteral("-screenLock") << QStringLiteral("status"),
                                   deadline));
#elif defined(Q_OS_LINUX)
    if (QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP")).contains(QLatin1String("GNOME")) == false)
        return -1;
    const QString gsettings = QStringLiteral("/usr/bin/gsettings");
    const QString delay = runTool(gsettings, QStringList() << QStringLiteral("get") << QStringLiteral("org.gnome.desktop.session")
                                  << QStringLiteral("idle-delay"), deadline).trimmed();
    const QString lock = runTool(gsettings, QStringList() << QStringLiteral("get") << QStringLiteral("org.gnome.desktop.screensaver")
                                 << QStringLiteral("lock-enabled"), deadline).trimmed();
    bool ok = false;
    const int seconds = delay.section(QLatin1Char(' '), -1).toInt(&ok);
    if (ok)
        *idleMinutes = (seconds + 59) / 60;
    return lock == QLatin1String("true") ? 1 : (lock == QLatin1String("false") ? 0 : -1);
#elif defined(Q_OS_WIN)
    Q_UNUSED(deadline)
    BOOL active = FALSE, secure = FALSE;
    UINT timeout = 0;
    SystemParametersInfoW(SPI_GETSCREENSAVEACTIVE, 0, &active, 0);
    SystemParametersInfoW(SPI_GETSCREENSAVETIMEOUT, 0, &timeout, 0);
    SystemParametersInfoW(SPI_GETSCREENSAVESECURE, 0, &secure, 0);
    *idleMinutes = active ? int(timeout) / 60 : 0;
    return (active && secure) ? 1 : 0;
#else
    Q_UNUSED(deadline)
    return -1;
#endif
}

int SystemProbe::backupRunning(const QDeadlineTimer &deadline)
{
#if defined(Q_OS_MACOS)
    return parseBackupRunning(runTool(QStringLiteral("/usr/bin/tmutil"),
                                      QStringList() << QStringLiteral("status"), deadline));
#else
    Q_UNUSED(deadline)
    return -1;
#endif
}

int SystemProbe::backupScheduled()
{
#if defined(Q_OS_MACOS)
    const QVariant v = QSettings(QStringLiteral("/Library/Preferences/com.apple.TimeMachine.plist"),
                                 QSettings::NativeFormat).value(QStringLiteral("AutoBackup"));
    return v.isValid() ? (v.toBool() ? 1 : 0) : -1;
#else
    return -1;
#endif
}

int SystemProbe::autoUpdates()
{
#if defined(Q_OS_MACOS)
    const QVariant v = QSettings(QStringLiteral("/Library/Preferences/com.apple.SoftwareUpdate.plist"),
                                 QSettings::NativeFormat).value(QStringLiteral("AutomaticallyInstallMacOSUpdates"));
    return v.isValid() ? (v.toBool() ? 1 : 0) : -1;
#elif defined(Q_OS_LINUX)
    const QString conf = readText(QStringLiteral("/etc/apt/apt.conf.d/20auto-upgrades"));
    if (conf.isEmpty())
        return -1;
    return conf.contains(QRegularExpression(QStringLiteral("Unattended-Upgrade\\s+\"[1-9]"))) ? 1 : 0;
#else
    return -1;
#endif
}

bool SystemProbe::rebootPending()
{
#if defined(Q_OS_WIN)
    return QSettings(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update"),
                     QSettings::NativeFormat).childGroups().contains(QStringLiteral("RebootRequired"));
#else
    return false;
#endif
}

int SystemProbe::piThrottled(const QDeadlineTimer &deadline)
{
#if defined(Q_OS_LINUX)
    return parseThrottled(runTool(QStringLiteral("/usr/bin/vcgencmd"),
                                  QStringList() << QStringLiteral("get_throttled"), deadline));
#else
    Q_UNUSED(deadline)
    return -1;
#endif
}

QList<SystemProbe::ProcessUse> SystemProbe::parseProcessTable(const QString &ps, qint64 selfPid)
{
    QList<ProcessUse> out;
    static const QRegularExpression row(QStringLiteral("^\\s*(\\d+)\\s+([0-9.,]+)\\s+(.+?)\\s*$"));
    const QStringList lines = ps.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines)
    {
        const QRegularExpressionMatch m = row.match(line);
        if (m.hasMatch() == false || m.captured(1).toLongLong() == selfPid)
            continue;
        ProcessUse use;
        use.name = m.captured(3);
        QString cpu = m.captured(2);
        use.cpu = cpu.replace(QLatin1Char(','), QLatin1Char('.')).toDouble();
        out << use;
    }
    return out;
}

int SystemProbe::parseScreenLock(const QString &sysadminctl)
{
    if (sysadminctl.contains(QLatin1String("screenLock is off")))
        return 0;
    if (sysadminctl.contains(QLatin1String("screenLock delay is")))
        return 1;
    return -1;
}

int SystemProbe::parseIdleMinutes(const QString &pmset)
{
    static const QRegularExpression re(QStringLiteral("(?m)^\\s*displaysleep\\s+(\\d+)"));
    const QRegularExpressionMatch m = re.match(pmset);
    return m.hasMatch() ? m.captured(1).toInt() : -1;
}

int SystemProbe::parseBackupRunning(const QString &tmutil)
{
    static const QRegularExpression re(QStringLiteral("\\bRunning\\s*=\\s*(\\d)"));
    const QRegularExpressionMatch m = re.match(tmutil);
    return m.hasMatch() ? (m.captured(1) == QLatin1String("1") ? 1 : 0) : -1;
}

int SystemProbe::parseThrottled(const QString &vcgencmd)
{
    static const QRegularExpression re(QStringLiteral("throttled=0x([0-9a-fA-F]+)"));
    const QRegularExpressionMatch m = re.match(vcgencmd);
    return m.hasMatch() ? m.captured(1).toInt(nullptr, 16) : -1;
}

bool SystemProbe::parseProcStat(const QString &stat, QString *name, qint64 *ticks)
{
    /* pid (name) state ...: the name may hold spaces and brackets, so look
       for the last ')'. utime and stime are fields 11 and 12 after it. */
    const int open = stat.indexOf(QLatin1Char('('));
    const int close = stat.lastIndexOf(QLatin1Char(')'));
    if (open < 0 || close < open)
        return false;
    const QStringList rest = stat.mid(close + 1).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (rest.size() < 13)
        return false;
    *name = stat.mid(open + 1, close - open - 1);
    *ticks = rest.at(11).toLongLong() + rest.at(12).toLongLong();
    return true;
}

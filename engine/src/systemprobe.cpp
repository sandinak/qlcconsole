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


#include <QDebug>
#include <QFile>
#include <QThread>

#if defined(Q_OS_LINUX)
#include <sys/resource.h>
#include <sched.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include "systemprobe.h"

#if defined(Q_OS_MACOS)
namespace SystemProbeMac
{
int lowPowerMode();
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

/*
  Q Light Controller Plus
  mastertimer-win32.cpp

  Copyright (C) Heikki Junnila
                Massimo Callegari

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

// CreateWaitableTimerExW needs Vista or later; this is a Windows 10 app.
#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0A00
  #define WINVER 0x0A00
#endif

// avrt.h uses HANDLE/WINBOOL without including anything that defines them.
#include <windows.h>
#include <mmsystem.h>
#include <avrt.h>

#include <QThread>
#include <QDebug>

#include "mastertimer-win32.h"
#include "mastertimer.h"
#include "qlcmacros.h"

/* Windows 10 1803 and later; older SDK headers don't name it. */
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
  #define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

/****************************************************************************
 * Timer thread priority
 ****************************************************************************/

/**
 * Ask MMCSS (the Multimedia Class Scheduler Service) to treat the timer
 * thread as real-time. Same problem, and the direct Windows sibling of the mac
 * THREAD_TIME_CONSTRAINT_POLICY reasoning in mastertimer-unix.cpp (see that
 * comment): a periodic thread with a hard deadline, competing with everything
 * else on the machine. "Pro Audio" is the class DAWs/pro-audio apps register
 * under for exactly this need -- chosen over THREAD_PRIORITY_TIME_CRITICAL or
 * REALTIME_PRIORITY_CLASS (which Microsoft's own docs caution against)
 * because MMCSS has OS-managed throttling built in.
 *
 * Called once, from the timer thread itself (the boost applies to the calling
 * thread); the returned handle is given back with
 * AvRevertMmThreadCharacteristics() when the thread's loop ends. Advisory:
 * on refusal the thread keeps the HighestPriority it was started with. The
 * refusal is logged once per process, not once per start.
 *
 * This used to run on every thread-pool worker CreateTimerQueueTimer handed a
 * tick to; once ticks overran, the pool kept starting workers and MMCSS
 * refused each one with ERROR_TOO_MANY_THREADS (565) -- 736 warnings in a
 * three-minute soak phase.
 */
static HANDLE setTimerThreadRealtime()
{
    static bool s_refusalLogged = false;

    DWORD taskIndex = 0;
    HANDLE h = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    if (h == NULL)
    {
        if (s_refusalLogged == false)
        {
            s_refusalLogged = true;
            qWarning() << "MasterTimer: MMCSS 'Pro Audio' characteristics refused"
                       << "(" << GetLastError() << ") - staying at high priority";
        }
    }
    else
    {
        qDebug() << "MasterTimer: MMCSS 'Pro Audio' thread characteristics set";
    }
    return h;
}

/****************************************************************************
 * MasterTimerPrivate
 ****************************************************************************/

MasterTimerPrivate::MasterTimerPrivate(MasterTimer* masterTimer)
    : m_masterTimer(masterTimer)
    , m_thread(NULL)
    , m_timer(NULL)
    , m_stopEvent(NULL)
    , m_systemTimerResolution(0)
    , m_run(0)
{
    Q_ASSERT(masterTimer != NULL);
}

MasterTimerPrivate::~MasterTimerPrivate()
{
    stop();
    /* A stop() from the timer thread itself (see there) leaves the thread to
       be collected here */
    if (m_thread != NULL)
    {
        m_thread->wait();
        delete m_thread;
        m_thread = NULL;
        releaseResources();
    }
}

/*
 * The tick used to come from CreateTimerQueueTimer(WT_EXECUTELONGFUNCTION):
 * a thread-pool callback every period, whether or not the previous one had
 * finished. When a tick overran (a heavy show: 94 universes, 2560 pixel
 * fixtures), the pool started a new worker for every overlapping tick and
 * they all queued on the tick lock -- process threads 111 -> 872, private
 * bytes 163 -> 803 MB, ticks up to 40 s late, the web API stopped answering.
 *
 * Now one thread owns the tick, like the unix timer: wait for the deadline,
 * tick, move the deadline on by one period. Deadlines are absolute (kept on
 * the performance counter), so the rate doesn't drift by the time each tick
 * takes. If a tick finishes past the next deadline, the next tick runs at
 * once and the deadline is re-anchored to now -- missed ticks are dropped,
 * never queued -- exactly the unix overrun behaviour.
 */
void MasterTimerPrivate::start()
{
    if (m_thread != NULL)
    {
        if (m_run.loadAcquire() != 0)
            return;
        /* Stopped from its own thread earlier; collect it before restarting */
        m_thread->wait();
        delete m_thread;
        m_thread = NULL;
        releaseResources();
    }

    /* A high-resolution waitable timer (Windows 10 1803+) wakes within about
       half a millisecond without changing the system-wide timer resolution.
       Older systems: a normal waitable timer, and timeBeginPeriod() so that it
       isn't rounded to the default 15.6 ms clock. */
    m_timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                     TIMER_ALL_ACCESS);
    if (m_timer == NULL)
    {
        TIMECAPS ptc;
        if (timeGetDevCaps(&ptc, sizeof(TIMECAPS)) == TIMERR_NOERROR)
        {
            UINT res = MAX(ptc.wPeriodMin, UINT(1));
            if (timeBeginPeriod(res) == TIMERR_NOERROR)
                m_systemTimerResolution = res;
        }
        if (m_systemTimerResolution == 0)
            qWarning() << Q_FUNC_INFO << "Unable to adjust system timer resolution.";

        m_timer = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
        qDebug() << "MasterTimer: no high-resolution waitable timer; using"
                 << m_systemTimerResolution << "ms system timer resolution";
    }

    if (m_timer == NULL)
    {
        qWarning() << Q_FUNC_INFO << "Unable to create a timer:" << GetLastError();
        releaseResources();
        return;
    }

    m_stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (m_stopEvent == NULL)
    {
        qWarning() << Q_FUNC_INFO << "Unable to create the stop event:" << GetLastError();
        releaseResources();
        return;
    }

    m_run.storeRelease(1);
    m_thread = QThread::create([this] { run(); });
    m_thread->setObjectName("MasterTimer");
    m_thread->start(QThread::HighestPriority);
}

void MasterTimerPrivate::stop()
{
    if (m_thread == NULL)
        return;

    m_run.storeRelease(0);
    SetEvent(m_stopEvent);

    /* Stopped from inside a tick: the loop ends when the tick returns, but a
       thread can't wait for itself. The thread object is collected by the
       next start() or the destructor. */
    if (QThread::currentThread() == m_thread)
        return;

    /* Waits for a tick in progress to finish: nothing is freed under it */
    m_thread->wait();
    delete m_thread;
    m_thread = NULL;
    releaseResources();
}

void MasterTimerPrivate::releaseResources()
{
    if (m_timer != NULL)
    {
        CancelWaitableTimer(m_timer);
        CloseHandle(m_timer);
        m_timer = NULL;
    }
    if (m_stopEvent != NULL)
    {
        CloseHandle(m_stopEvent);
        m_stopEvent = NULL;
    }
    if (m_systemTimerResolution != 0)
    {
        timeEndPeriod(m_systemTimerResolution);
        m_systemTimerResolution = 0;
    }
}

bool MasterTimerPrivate::isRunning() const
{
    return m_thread != NULL && m_run.loadAcquire() != 0;
}

void MasterTimerPrivate::timerTick()
{
    m_masterTimer->timerTick(true);
}

void MasterTimerPrivate::run()
{
    HANDLE mmcss = setTimerThreadRealtime();

    LARGE_INTEGER qpf;
    QueryPerformanceFrequency(&qpf);
    const qint64 countsPerSec = qpf.QuadPart;
    const qint64 period = countsPerSec / qint64(MasterTimer::frequency());
    const long budgetUs = long(1000000L / long(MasterTimer::frequency()));

    auto now = []() -> qint64
    {
        LARGE_INTEGER c;
        QueryPerformanceCounter(&c);
        return c.QuadPart;
    };

    HANDLE handles[2] = { m_stopEvent, m_timer };
    qint64 deadline = now();

    while (m_run.loadAcquire() != 0)
    {
        deadline += period;

        qint64 current = now();
        if (current >= deadline)
        {
            /* Running late: the last tick took longer than its period, or the
               thread wasn't scheduled back in time. Same report as the unix
               timer -- "late" vs the previous tick's compute time tells the
               two apart. Tick now, and re-anchor rather than trying to catch
               up: the ticks that were missed are not run. */
            const long lateUs = long((current - deadline) * 1000000LL / countsPerSec);
            qDebug() << Q_FUNC_INFO << "MasterTimer is running late!"
                     << "late_us:" << lateUs
                     << "wall_us:" << long(m_masterTimer->tickComputeMs() * 1000.0)
                     << "cpu_us:" << long(m_masterTimer->tickCpuMs() * 1000.0)
                     << "budget_us:" << budgetUs;
            m_masterTimer->timerTick(true);
            deadline = now();
            continue;
        }

        /* Relative due time, in (negative) 100 ns units */
        LARGE_INTEGER due;
        due.QuadPart = -((deadline - current) * 10000000LL / countsPerSec);
        if (due.QuadPart == 0)
            due.QuadPart = -1;

        if (SetWaitableTimer(m_timer, &due, 0, NULL, NULL, FALSE) == FALSE)
        {
            qWarning() << Q_FUNC_INFO << "SetWaitableTimer failed:" << GetLastError()
                       << "- stopping the timer thread";
            break;
        }

        DWORD w = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (w == WAIT_OBJECT_0)
            break; /* stop() */
        if (w != WAIT_OBJECT_0 + 1)
        {
            qWarning() << Q_FUNC_INFO << "Timer wait failed:" << GetLastError()
                       << "- stopping the timer thread";
            break;
        }

        if (m_run.loadAcquire() == 0)
            break;

        m_masterTimer->timerTick(true);
    }

    /* Also on an error exit, so isRunning() stops claiming a dead timer and
       start() can bring it back */
    m_run.storeRelease(0);

    if (mmcss != NULL)
        AvRevertMmThreadCharacteristics(mmcss);
}

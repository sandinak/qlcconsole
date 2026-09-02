/*
  Q Light Controller Plus
  mastertimer-unix.cpp

  Copyright (C) Heikki Junnila
                Christopher Staite
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

#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <QDebug>

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <pthread.h>
#elif defined(Q_OS_LINUX)
#include <pthread.h>
#include <sched.h>
#include <string.h>
#endif

#include "mastertimer-unix.h"
#include "mastertimer.h"

/****************************************************************************
 * MasterTimerPrivate
 ****************************************************************************/

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
/**
 * Ask the Mach scheduler to treat this thread as real-time.
 *
 * The DMX timer thread has a hard 20 ms deadline (at the default 50 Hz) but was
 * started at ordinary priority, so it competed with every other thread on the
 * machine. Measured over 12.5 h at 51 universes: the tick itself computed in
 * 53 us median -- 0.27% of its budget -- while deadlines were missed by 19 ms
 * median and up to 473 ms, i.e. 23 consecutive ticks. That is the scheduler,
 * not the engine.
 *
 * It matters more than a dropped frame: Function::incrementElapsed() advances
 * by a fixed MasterTimer::tick() per tick rather than by wall clock, and the
 * loop re-anchors after a miss, so skipped ticks are gone for good and every
 * fade, chase and timeline runs permanently slow by the lost time (~21 s over
 * that run).
 *
 * THREAD_TIME_CONSTRAINT_POLICY is what CoreAudio uses for the same problem.
 * We declare: we wake every `period`, need `computation` of CPU, and must be
 * finished within `constraint` of the period start. preemptible stays true --
 * DMX is not audio, and a 20 ms period does not justify blocking the machine.
 *
 * Advisory: if the kernel declines, we warn and keep the ordinary priority
 * behaviour rather than failing to start.
 */
static void setTimerThreadRealtime(int nsTickTime)
{
    mach_timebase_info_data_t tb;
    if (mach_timebase_info(&tb) != KERN_SUCCESS || tb.numer == 0)
    {
        qWarning() << "MasterTimer: no mach timebase; leaving thread priority alone";
        return;
    }

    /* nanoseconds -> mach absolute time units */
    const double toAbs = double(tb.denom) / double(tb.numer);

    /* Headroom over the measured worst case, not a guess: p90 tick compute was
       206 us, so 1 ms of declared computation is ~5x that, and a 2 ms
       constraint bounds how late a tick may start while staying far inside the
       20 ms period. */
    thread_time_constraint_policy_data_t policy;
    policy.period      = uint32_t(double(nsTickTime) * toAbs);
    policy.computation = uint32_t(1000000.0 * toAbs);  /* 1 ms */
    policy.constraint  = uint32_t(2000000.0 * toAbs);  /* 2 ms */
    policy.preemptible = 1;

    kern_return_t kr = thread_policy_set(pthread_mach_thread_np(pthread_self()),
                                         THREAD_TIME_CONSTRAINT_POLICY,
                                         reinterpret_cast<thread_policy_t>(&policy),
                                         THREAD_TIME_CONSTRAINT_POLICY_COUNT);
    if (kr != KERN_SUCCESS)
        qWarning() << "MasterTimer: real-time thread policy refused by the kernel"
                   << "(kern_return" << kr << ") - falling back to normal priority";
    else
        qDebug() << "MasterTimer: real-time thread policy set - period"
                 << nsTickTime / 1000 << "us, computation 1000 us, constraint 2000 us";
}
#elif defined(Q_OS_LINUX)
/**
 * Ask the Linux scheduler to treat this thread as real-time. Same problem
 * and same reasoning as the mac THREAD_TIME_CONSTRAINT_POLICY path above
 * (see that comment) -- Linux has no equivalent declarative period/
 * computation/constraint policy, so the closest match is SCHED_FIFO, the
 * fixed-priority real-time class pro-audio apps (JACK, PipeWire) use for
 * the same kind of periodic, low-latency, must-not-starve-the-system
 * thread.
 *
 * Priority is deliberately NOT sched_get_priority_max() (typically 99):
 * leaving headroom above this thread, the same convention those pro-audio
 * apps follow, keeps it clearly real-time-scheduled without contending
 * with anything genuinely more critical on the box.
 *
 * Advisory: SCHED_FIFO normally needs CAP_SYS_NICE or an rtprio resource
 * limit, which an ordinary launch of this app will not have. Unlike the
 * mac policy above (any user process may request it), that means this
 * commonly DOES fail on a stock install -- expected, not a bug. On
 * refusal we warn with the actual fix (grant the capability or an rtprio
 * limit) and fall back to normal scheduling, exactly like the mac path.
 */
static void setTimerThreadRealtime(int nsTickTime)
{
    Q_UNUSED(nsTickTime)

    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    const int maxPrio = sched_get_priority_max(SCHED_FIFO);
    sp.sched_priority = (maxPrio > 10) ? (maxPrio - 10) : maxPrio;

    int rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    if (rc != 0)
        qWarning() << "MasterTimer: SCHED_FIFO real-time priority refused"
                   << "(" << strerror(rc) << ") - falling back to normal priority."
                   << "Usually needs CAP_SYS_NICE (setcap cap_sys_nice=eip on the"
                   << "binary) or an rtprio resource limit.";
    else
        qDebug() << "MasterTimer: SCHED_FIFO real-time priority set"
                 << "(priority" << sp.sched_priority << ")";
}
#else
/* No known real-time scheduling mechanism on this platform -- ordinary
   thread priority is all we get. */
static void setTimerThreadRealtime(int nsTickTime)
{
    Q_UNUSED(nsTickTime)
}
#endif

MasterTimerPrivate::MasterTimerPrivate(MasterTimer* masterTimer)
    : QThread(masterTimer)
    , m_run(false)
{
    Q_ASSERT(masterTimer != NULL);
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
#endif
}

MasterTimerPrivate::~MasterTimerPrivate()
{
    stop();
}

void MasterTimerPrivate::stop()
{
    m_run = false;
    wait();
}

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
int MasterTimerPrivate::compareTime(mach_timespec_t *time1, mach_timespec_t *time2)
#else
int MasterTimerPrivate::compareTime(struct timespec *time1, struct timespec *time2)
#endif
{
    if (time1->tv_sec < time2->tv_sec)
    {
        qDebug() << "Time is late by" << (time2->tv_sec - time1->tv_sec) << "seconds";
        return -1;
    }
    else if (time1->tv_sec > time2->tv_sec)
        return 1;
    else if (time1->tv_nsec < time2->tv_nsec)
    {
        qDebug() << "Time is late by" << (time2->tv_nsec - time1->tv_nsec) << "nanoseconds";
        return -1;
    }
    else if (time1->tv_nsec > time2->tv_nsec)
        return 1;
    else
        return 0;
}

void MasterTimerPrivate::run()
{
    /* Don't start another thread */
    if (m_run == true)
        return;

    MasterTimer* mt = qobject_cast <MasterTimer*> (parent());
    Q_ASSERT(mt != NULL);

    /* How long to wait each loop, in nanoseconds */
    int nsTickTime = 1000000000L / mt->frequency();

    /* Must be done from inside the thread itself: the policy applies to the
       calling thread. setTimerThreadRealtime() is defined for every branch
       above (mac/Linux/other-unix no-op), so this call is unconditional. */
    setTimerThreadRealtime(nsTickTime);

    /* Allocate this from stack here so that GCC doesn't have
       to do it every time implicitly when gettimeofday() is called */
    int ret = 0;

    /* Allocate all the memory at the start so we don't waste any time */
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    mach_timespec_t* finish = static_cast<mach_timespec_t*> (malloc(sizeof(mach_timespec_t)));
    mach_timespec_t* current = static_cast<mach_timespec_t*> (malloc(sizeof(mach_timespec_t)));
#else
    struct timespec* finish = static_cast<struct timespec*> (malloc(sizeof(struct timespec)));
    struct timespec* current = static_cast<struct timespec*> (malloc(sizeof(struct timespec)));
#endif
    struct timespec* sleepTime = static_cast<struct timespec*> (malloc(sizeof(struct timespec)));
    struct timespec* remainingTime = static_cast<struct timespec*> (malloc(sizeof(struct timespec)));

    sleepTime->tv_sec = 0;

    /* This is the start time for the timer */
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    ret = clock_get_time(cclock, finish);
#else
    ret = clock_gettime(CLOCK_MONOTONIC, finish);
#endif
    if (ret == -1)
    {
        qWarning() << Q_FUNC_INFO << "Unable to get the time accurately:"
                   << strerror(errno) << "- Stopping MasterTimerPrivate";
        m_run = false;
    }
    else
    {
        m_run = true;
    }

    while (m_run == true)
    {
        /* Add nsTickTime to the finish time, to calculate the end timestamp of this loop */
        finish->tv_sec += (finish->tv_nsec + nsTickTime) / 1000000000L;
        finish->tv_nsec = (finish->tv_nsec + nsTickTime) % 1000000000L;

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
        ret = clock_get_time(cclock, current);
#else
        ret = clock_gettime(CLOCK_MONOTONIC, current);
#endif
        if (ret == -1)
        {
            qWarning() << Q_FUNC_INFO << "Unable to get the current time:"
                       << strerror(errno);
            m_run = false;
            break;
        }

        /* Check if we're running late. This means that a tick is not enough
         * to process all the running Functions :'( -- or that the kernel did
         * not schedule this thread back in time. Report BOTH numbers so the
         * two causes can be told apart: "late" is how far past the deadline we
         * woke up, "compute" is how long the previous tick actually took. If
         * compute is a small fraction of the tick budget while late is large,
         * this is scheduling jitter, not engine load. */
        if (compareTime(finish, current) <= 0)
        {
            const long lateUs = long((current->tv_sec - finish->tv_sec) * 1000000L
                                     + (current->tv_nsec - finish->tv_nsec) / 1000L);
            qDebug() << Q_FUNC_INFO << "MasterTimer is running late!"
                     << "late_us:" << lateUs
                     << "wall_us:" << long(mt->tickComputeMs() * 1000.0)
                     << "cpu_us:" << long(mt->tickCpuMs() * 1000.0)
                     << "budget_us:" << (nsTickTime / 1000);
            /* No need to sleep. Immediately process the next tick */
            mt->timerTick();
            /* Now the finish time needs to be recalibrated */
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
            clock_get_time(cclock, finish);
#else
            clock_gettime(CLOCK_MONOTONIC, finish);
#endif
            continue;
        }

        /* Do a rough sleep using the kernel to return control.
           We know that this will never be seconds as we are dealing
           with jumps of under a second every time. */
        sleepTime->tv_sec = finish->tv_sec - current->tv_sec;
        if (finish->tv_nsec < current->tv_nsec)
        {
            sleepTime->tv_nsec = finish->tv_nsec + 1000000000L - current->tv_nsec ;
            sleepTime->tv_sec--; /* Decrease a second. */
        }
        else
            sleepTime->tv_nsec = finish->tv_nsec - current->tv_nsec;

        //qDebug() << Q_FUNC_INFO << "Sleeping ns:" << sleepTime->tv_nsec;

        ret = nanosleep(sleepTime, remainingTime);
        while (ret == -1 && sleepTime->tv_nsec > 100)
        {
            sleepTime->tv_nsec = remainingTime->tv_nsec;
            ret = nanosleep(sleepTime, remainingTime);
        }

#if 0
        /* Now take full CPU for precision (only a few nanoseconds,
           at maximum 100 nanoseconds) */
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
        ret = clock_get_time(cclock, current);
#else
        ret = clock_gettime(CLOCK_MONOTONIC, current);
#endif
        sleepTime->tv_nsec = finish->tv_nsec - current->tv_nsec;

        while (sleepTime->tv_nsec > 5)
        {
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
            ret = clock_get_time(cclock, current);
#else
            ret = clock_gettime(CLOCK_MONOTONIC, current);
#endif
            sleepTime->tv_nsec = finish->tv_nsec - current->tv_nsec;
            qDebug() << "Full CPU wait:" << sleepTime->tv_nsec;
        }
#endif
        /* Execute the next timer event */
        mt->timerTick();
    }

    free(finish);
    free(current);
    free(sleepTime);
    free(remainingTime);
}

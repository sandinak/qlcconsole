/*
  Q Light Controller Plus
  mastertimer-win32.h

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

#ifndef MASTERTIMER_PRIVATE_H
#define MASTERTIMER_PRIVATE_H

#include <Windows.h>
#include <QAtomicInt>

class MasterTimer;
class QThread;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The Windows tick source: one dedicated thread that waits on a waitable
 * timer for an absolute deadline, ticks, and moves the deadline on -- the
 * same shape as the unix timer's nanosleep loop. A tick that overruns its
 * period makes the next one start late; ticks never overlap and never run on
 * any other thread.
 */
class MasterTimerPrivate final
{
public:
    MasterTimerPrivate(MasterTimer* masterTimer);
    ~MasterTimerPrivate();

    void start();
    void stop();
    bool isRunning() const;

    void timerTick();

private:
    /** The timer thread's loop */
    void run();
    /** Close the handles start() opened and undo timeBeginPeriod() */
    void releaseResources();

private:
    MasterTimer* m_masterTimer;
    /** The timer thread; NULL when not started */
    QThread* m_thread;
    /** The waitable timer the thread sleeps on */
    HANDLE m_timer;
    /** Set by stop() to wake the thread out of its wait */
    HANDLE m_stopEvent;
    /** Non-zero when timeBeginPeriod() was needed (no high-resolution timer) */
    UINT m_systemTimerResolution;
    /** Cleared by stop(); the loop runs while it is set */
    QAtomicInt m_run;
};

/** @} */

#endif

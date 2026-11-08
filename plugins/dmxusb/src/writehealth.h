/*
  Q Light Controller Plus
  writehealth.h

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

#ifndef WRITEHEALTH_H
#define WRITEHEALTH_H

#include <QElapsedTimer>
#include <QDateTime>
#include <QString>
#include <QMutex>

/**
 * Whether a DMX USB widget is still taking the frames written to it.
 *
 * A widget pulled out (or one whose driver gave up) mid-session used to be
 * said only in the log, once per frame: the universe kept "sending" and the
 * footer said nothing (UX-67). Each write loop records every frame's write
 * here; the plugin's outputProblems() reads problem().
 *
 * FAILING once writes have failed for FailAfterMs with no write going
 * through in between (a frame or two lost to a busy bus is not a dead
 * widget); fine again once writes have gone through for RecoverAfterMs.
 * Thread-safe: the write loops run on the widgets' own threads, the footer
 * asks from the GUI.
 */
class WriteHealth
{
public:
    static constexpr qint64 FailAfterMs = 1000;
    static constexpr qint64 RecoverAfterMs = 3000;

    WriteHealth();

    /** One frame's write. @p nowMs: a monotonic time (-1: this object's
     *  own clock). Returns true when problem() changed. */
    bool record(bool ok, qint64 nowMs = -1);

    bool isFailing() const;
    /** "not taking data since 20:14:05" while failing, else empty. */
    QString problem() const;

private:
    mutable QMutex m_mutex;
    QElapsedTimer m_clock;
    bool m_failing;
    qint64 m_failSince;     //!< first failure of the current run, -1 when none
    qint64 m_okSince;       //!< first success while failing, -1 when none
    QDateTime m_failWall;   //!< when the current run of failures began
};

#endif

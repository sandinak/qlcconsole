/*
  Q Light Controller Plus
  timecodesource.h

  Copyright (c) Massimo Callegari

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

#ifndef TIMECODESOURCE_H
#define TIMECODESOURCE_H

#include <QObject>

/** Milliseconds of no fresh timecode before a followed show is treated as
 *  HELD/frozen. ONE constant shared by the ShowRunner freeze (showrunner.cpp)
 *  and the watchdog that drives the "holding" footer chip (timecodesource.cpp),
 *  so the rig-freeze and the indicator can never drift apart. Must stay above
 *  normal quarter-frame spacing (~8ms at 30fps) so steady playback never
 *  flickers to "holding". */
// The show cursor free-runs (extrapolates at ~real time) at most this far past
// the last FRESH MTC position before it holds and the running children (scenes/
// chasers) are frozen. This BOUNDS overshoot on a genuine stop to this value and
// keeps in-progress fades tight to the timecode — independent of the longer,
// purely-cosmetic indicator debounce below. Small = tight sync + minimal
// overshoot; the phase-lock snaps back when the next position arrives.
#define SHOW_TC_EXTRAP_CAP_MS 400

// How long the MTC STATUS CHIP keeps reading "rolling" after the last position
// update before flipping to "holding". Deliberately longer than the cap above so
// the indicator does NOT flicker on RTP/network-MIDI jitter (gaps up to ~1 s).
// This governs ONLY the indicator/last-look-hold; the show's own cursor and rig
// response are governed by SHOW_TC_EXTRAP_CAP_MS, so a long value here no longer
// means the timeline runs away.
#define SHOW_TC_HOLD_MS 1000

class QTimer;

/** @addtogroup engine Engine
 * @{
 */

/**
 * @brief Aggregates incoming timeline positions (e.g. MIDI Time Code) into a
 * single authoritative clock the Show timeline can follow.
 *
 * Input plugins push absolute positions via updateTimeCode(). When positions
 * keep arriving the source is "running" (green); when they stop arriving for
 * longer than the watchdog window it goes "not running" (frozen at the last
 * position) — this is what lets a spoken/click-less scene fall back to manual
 * GO: the timeline simply stops advancing and holds.
 *
 * Auto-detect vs override: with no override set (sourceUniverse == -1) any
 * universe's timecode is accepted; set a specific universe to lock onto one
 * source and ignore the rest.
 */
class TimecodeSource : public QObject
{
    Q_OBJECT

public:
    explicit TimecodeSource(QObject *parent = 0);
    ~TimecodeSource();

    /** Last received absolute position, in milliseconds. */
    quint32 positionMs() const;

    /** Nominal frame rate reported by the source. */
    int fps() const;

    /** True while timecode is actively advancing (received within the
     *  watchdog window). False = no source, or the source has stopped. */
    bool isRunning() const;

    /** Override: only accept timecode from this universe. -1 = accept any. */
    qint32 sourceUniverse() const;
    void setSourceUniverse(qint32 universe);

    /** Universe that most recently delivered timecode (-1 if none yet). */
    qint32 lastUniverse() const;

public slots:
    /** Push a new absolute position received on the given universe. */
    void updateTimeCode(quint32 universe, quint32 msPosition, uchar fps);

signals:
    /** Emitted on every accepted position update. */
    void timeChanged(quint32 msPosition);

    /** Emitted when the running/stopped state flips. */
    void runningChanged(bool running);

private slots:
    void slotWatchdogTimeout();

private:
    quint32 m_positionMs;
    int m_fps;
    bool m_running;
    qint32 m_sourceUniverse;
    qint32 m_lastUniverse;
    QTimer *m_watchdog;
};

/** @} */

#endif // TIMECODESOURCE_H

/*
  Q Light Controller Plus
  timecodesource.cpp

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

#include <QTimer>
#include <cmath>

#include "timecodesource.h"

/** No timecode for this long (ms) => the source is considered stopped. A full
 *  SMPTE time assembles roughly every 80 ms (every two frames), but the updates
 *  reach us as queued cross-thread calls on a busy UI thread, so completions
 *  can bunch up. The window must stay above normal quarter-frame spacing
 *  (~8ms at 30fps, ~200ms = ~25 missed frames = a genuine stop) so steady
 *  playback never flickers to "holding". Kept close to ShowRunner's own
 *  freeze threshold (m_msSinceFresh > 150 in showrunner.cpp) so the "holding"
 *  chip flips right around when the rig actually freezes — it used to be 600,
 *  which left the chip green for up to ~450ms after the show had frozen. */
#define TIMECODE_WATCHDOG_MS SHOW_TC_HOLD_MS   // shared with the ShowRunner freeze

TimecodeSource::TimecodeSource(QObject *parent)
    : QObject(parent)
    , m_positionMs(0)
    , m_fps(30)
    , m_running(false)
    , m_sourceUniverse(-1)
    , m_lastUniverse(-1)
    , m_lastUpdateWall(-1)
    , m_rate(1.0)
    , m_gapIdx(0)
    , m_gapCount(0)
{
    m_wall.start();
    for (int i = 0; i < kGapWindow; i++)
        m_gaps[i] = 0.0;
    m_watchdog = new QTimer(this);
    m_watchdog->setSingleShot(true);
    m_watchdog->setInterval(TIMECODE_WATCHDOG_MS);
    connect(m_watchdog, SIGNAL(timeout()), this, SLOT(slotWatchdogTimeout()));
}

TimecodeSource::~TimecodeSource()
{
}

quint32 TimecodeSource::positionMs() const
{
    return m_positionMs;
}

int TimecodeSource::fps() const
{
    return m_fps;
}

bool TimecodeSource::isRunning() const
{
    return m_running;
}

qint32 TimecodeSource::sourceUniverse() const
{
    return m_sourceUniverse;
}

void TimecodeSource::setSourceUniverse(qint32 universe)
{
    m_sourceUniverse = universe;
}

qint32 TimecodeSource::lastUniverse() const
{
    return m_lastUniverse;
}

double TimecodeSource::rateEstimate() const
{
    return m_running ? m_rate : 0.0;
}

double TimecodeSource::avgIntervalMs() const
{
    if (m_gapCount == 0)
        return 0.0;
    double sum = 0.0;
    for (int i = 0; i < m_gapCount; i++)
        sum += m_gaps[i];
    return sum / m_gapCount;
}

double TimecodeSource::jitterMs() const
{
    if (m_gapCount < 2)
        return 0.0;
    const double mean = avgIntervalMs();
    double var = 0.0;
    for (int i = 0; i < m_gapCount; i++)
    {
        const double d = m_gaps[i] - mean;
        var += d * d;
    }
    return std::sqrt(var / m_gapCount);
}

void TimecodeSource::updateTimeCode(quint32 universe, quint32 msPosition, uchar fps)
{
    // Override filter: ignore everything but the chosen source universe.
    if (m_sourceUniverse >= 0 && qint32(universe) != m_sourceUniverse)
        return;

    // Sync-health measurement: inter-update interval (jitter) + advance rate.
    const qint64 now = m_wall.elapsed();
    if (m_lastUpdateWall >= 0)
    {
        const double gap = double(now - m_lastUpdateWall);
        if (gap > 0.0 && gap < 2000.0)   // ignore stop/restart jumps
        {
            m_gaps[m_gapIdx] = gap;
            m_gapIdx = (m_gapIdx + 1) % kGapWindow;
            if (m_gapCount < kGapWindow)
                m_gapCount++;
            const double posDelta = double(qint64(msPosition) - qint64(m_positionMs));
            const double inst = posDelta / gap;      // ms of code per ms of wall
            if (inst > 0.2 && inst < 3.0)            // sane forward advance
                m_rate = 0.85 * m_rate + 0.15 * inst;
        }
    }
    m_lastUpdateWall = now;

    m_lastUniverse = qint32(universe);
    m_positionMs = msPosition;
    if (fps > 0)
        m_fps = fps;

    if (!m_running)
    {
        m_running = true;
        emit runningChanged(true);
    }

    // (Re)arm the watchdog: if nothing arrives before it fires, we stop.
    m_watchdog->start();

    emit timeChanged(m_positionMs);
}

void TimecodeSource::slotWatchdogTimeout()
{
    if (m_running)
    {
        m_running = false;
        emit runningChanged(false);
    }
}

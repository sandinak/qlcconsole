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

#include "timecodesource.h"

/** No timecode for this long (ms) => the source is considered stopped. A full
 *  SMPTE time assembles roughly every 80 ms (every two frames), so a couple of
 *  missed completions comfortably fits here. */
#define TIMECODE_WATCHDOG_MS 200

TimecodeSource::TimecodeSource(QObject *parent)
    : QObject(parent)
    , m_positionMs(0)
    , m_fps(30)
    , m_running(false)
    , m_sourceUniverse(-1)
    , m_lastUniverse(-1)
{
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

void TimecodeSource::updateTimeCode(quint32 universe, quint32 msPosition, uchar fps)
{
    // Override filter: ignore everything but the chosen source universe.
    if (m_sourceUniverse >= 0 && qint32(universe) != m_sourceUniverse)
        return;

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

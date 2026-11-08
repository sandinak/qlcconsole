/*
  Q Light Controller Plus
  writehealth.cpp

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
#include <QMutexLocker>

#include "writehealth.h"

WriteHealth::WriteHealth()
    : m_failing(false)
    , m_failSince(-1)
    , m_okSince(-1)
{
    m_clock.start();
}

bool WriteHealth::record(bool ok, qint64 nowMs)
{
    QMutexLocker locker(&m_mutex);
    const qint64 now = nowMs >= 0 ? nowMs : m_clock.elapsed();
    if (ok)
    {
        m_failSince = -1;
        if (!m_failing)
            return false;
        if (m_okSince < 0)
            m_okSince = now;
        if (now - m_okSince < RecoverAfterMs)
            return false;
        m_failing = false;
        m_okSince = -1;
        return true;
    }

    m_okSince = -1;
    if (m_failing)
        return false;
    if (m_failSince < 0)
    {
        m_failSince = now;
        m_failWall = QDateTime::currentDateTime();
    }
    if (now - m_failSince < FailAfterMs)
        return false;
    m_failing = true;
    return true;
}

bool WriteHealth::isFailing() const
{
    QMutexLocker locker(&m_mutex);
    return m_failing;
}

QString WriteHealth::problem() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_failing)
        return QString();
    return QCoreApplication::translate("DMXUSB", "not taking data since %1")
        .arg(m_failWall.toString(QStringLiteral("HH:mm:ss")));
}

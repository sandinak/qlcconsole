/*
  Q Light Controller Plus
  artnetsendhealth.cpp

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

#include "artnetsendhealth.h"

ArtNetSendHealth::ArtNetSendHealth()
    : m_failing(false)
    , m_pendingFailures(0)
    , m_pendingSince(0)
    , m_okRun(0)
    , m_okRunSince(0)
    , m_failedSends(0)
    , m_failingSince(0)
    , m_failedForMs(0)
{
}

ArtNetSendHealth::Transition ArtNetSendHealth::record(bool ok, qint64 nowMs, bool hardError)
{
    if (ok == false)
    {
        // Any failure breaks a run of successes, in either state
        m_okRun = 0;

        if (m_failing)
        {
            m_failedSends++;
            return NoChange;
        }

        if (m_pendingFailures == 0)
            m_pendingSince = nowMs;
        m_pendingFailures++;

        if (hardError ||
            (m_pendingFailures >= 2 && nowMs - m_pendingSince >= FailAfterMs))
        {
            m_failing = true;
            m_failingSince = m_pendingSince;
            m_failedSends = m_pendingFailures;
            m_failedForMs = 0;
            m_pendingFailures = 0;
            return StartedFailing;
        }
        return NoChange;
    }

    // Nothing in doubt: nothing to decide
    if (m_failing == false && m_pendingFailures == 0)
        return NoChange;

    if (m_okRun == 0)
        m_okRunSince = nowMs;
    m_okRun++;

    const qint64 held = nowMs - m_okRunSince;

    if (m_failing == false)
    {
        // A transient burst, followed by a clean run: forget it unreported
        if (m_okRun >= 2 && held >= ClearSuspicionMs)
        {
            m_pendingFailures = 0;
            m_okRun = 0;
        }
        return NoChange;
    }

    if (m_okRun >= 2 && held >= RecoverAfterMs)
    {
        m_failing = false;
        m_failedForMs = m_okRunSince - m_failingSince;
        m_okRun = 0;
        return Recovered;
    }
    return NoChange;
}

bool ArtNetSendHealth::isHardError(QAbstractSocket::SocketError error)
{
    switch (error)
    {
        case QAbstractSocket::DatagramTooLargeError:
        case QAbstractSocket::SocketAccessError:
        case QAbstractSocket::SocketAddressNotAvailableError:
        case QAbstractSocket::UnsupportedSocketOperationError:
            return true;
        default:
            return false;
    }
}

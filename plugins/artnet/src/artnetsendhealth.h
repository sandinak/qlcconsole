/*
  Q Light Controller Plus
  artnetsendhealth.h

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

#ifndef ARTNETSENDHEALTH_H
#define ARTNETSENDHEALTH_H

#include <QAbstractSocket>
#include <QtGlobal>

/**
 * Whether one universe's output is failing, decided with hysteresis.
 *
 * Fed one result per send (ok or not, plus a monotonic time in ms) and says
 * when the reported state should CHANGE. It owns no socket and reads no clock,
 * so the decision can be driven deterministically from a test.
 *
 * Why hysteresis: reporting on every flip of the per-send result is honest
 * about each packet and dishonest about the universe. The case that showed it
 * is a unicast target that never answers ARP. On macOS a send to it succeeds
 * while the ARP request is outstanding (~5 s, the packet is just queued), then
 * fails with "host is down" for net.link.ether.inet.host_down_time (20 s),
 * then succeeds again while ARP retries -- so a per-state-change log said
 * "is failing" / "recovered" every ~25 s for a node that was never there.
 * Linux behaves similarly on a shorter cycle. Those "successes" deliver
 * nothing; the universe is dark the whole time.
 *
 * The rules, asymmetric on purpose:
 *
 *  - FAILING is declared once failures have kept coming for FailAfterMs
 *    (at least two of them), without a clean run of successes long enough to
 *    clear the suspicion in between. Isolated successes do NOT reset that, so
 *    a link dropping every other packet is reported rather than hidden. A
 *    short burst (a full send buffer, an interface blip) that is followed by
 *    ClearSuspicionMs of clean sends is forgotten and never reported.
 *  - A hard error -- one that retrying cannot fix, see isHardError() -- is
 *    declared on the first send, because waiting adds nothing.
 *  - RECOVERED is declared only after sends have succeeded continuously for
 *    RecoverAfterMs (at least two of them). Any failure in that window restarts
 *    it. RecoverAfterMs is longer than the ARP flap's "success" phase by a wide
 *    margin, which is what stops the flapping.
 *
 * Times are compared as spans between sends, so the decision is the same at
 * 44 Hz (changing data) and at the 2 s idle refresh (Standard mode).
 */
class ArtNetSendHealth
{
public:
    enum Transition
    {
        NoChange,
        StartedFailing,
        Recovered
    };

    /** Failures must persist this long before the universe is called failing. */
    static constexpr qint64 FailAfterMs = 1000;

    /** A run of successes this long forgets failures not yet reported. */
    static constexpr qint64 ClearSuspicionMs = 1000;

    /** Successes must hold this long before a failing universe is called
     *  recovered. Comfortably over the ~5 s ARP "success" phase above. */
    static constexpr qint64 RecoverAfterMs = 30000;

    ArtNetSendHealth();

    /** Record one send. @a nowMs is a monotonic time in ms. @a hardError says
     *  the failure cannot clear by retrying (ignored when @a ok). Returns the
     *  change in the reported state, if any. */
    Transition record(bool ok, qint64 nowMs, bool hardError = false);

    /** The reported state: true from StartedFailing until Recovered */
    bool isFailing() const { return m_failing; }

    /** Failed sends in the current (or, after Recovered, the last) failing
     *  period, including the ones that led up to declaring it */
    quint64 failedSends() const { return m_failedSends; }

    /** How long the last failing period lasted: from its first failure to the
     *  start of the run of successes that ended it. Valid after Recovered. */
    qint64 failedForMs() const { return m_failedForMs; }

    /** A socket error that retrying the same send cannot fix: the datagram is
     *  too big, or the socket/address can't be used at all. NetworkError is
     *  deliberately NOT here -- Qt reports "host is down" (an ARP miss, which
     *  comes and goes) and "no route" alike as NetworkError, so it has to go
     *  through the sustained-failure rule. A gone interface still shows within
     *  FailAfterMs, as every send fails. */
    static bool isHardError(QAbstractSocket::SocketError error);

private:
    bool m_failing;

    /** Healthy only: failures not yet reported, and when the first came */
    quint64 m_pendingFailures;
    qint64 m_pendingSince;

    /** Consecutive successes, and when the first of them came */
    quint64 m_okRun;
    qint64 m_okRunSince;

    /** Failing only (kept after Recovered for the report) */
    quint64 m_failedSends;
    qint64 m_failingSince;
    qint64 m_failedForMs;
};

#endif

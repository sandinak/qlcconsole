/*
  Q Light Controller Plus
  stickfilter.cpp

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

#include <cmath>

#include "stickfilter.h"

void StickFilter::setAxes(const QList<int> &channels)
{
    QMap<int, Axis> axes;
    for (int ch : channels)
        axes.insert(ch, m_axes.value(ch));
    m_axes = axes;
}

void StickFilter::begin(qint64 now)
{
    m_autoUntil = now + kAutoWindowMs;
    m_lastOther = now;
    m_driftSaid = false;
    for (Axis &a : m_axes)
        a.captured = false;
}

void StickFilter::requestCapture(qint64 now)
{
    m_captureUntil = now + kCaptureTimeoutMs;
    m_lastOther = now;
    m_driftSaid = false;
    // Still from now on: a stick pushed as the request was made is not resting.
    for (Axis &a : m_axes)
    {
        a.captured = false;
        a.lo = a.hi = a.raw;
        a.still = now;
    }
}

uchar StickFilter::feed(int channel, uchar raw, qint64 now)
{
    auto it = m_axes.find(channel);
    if (it == m_axes.end())
    {
        // A button or trigger: the controller is in use.
        m_lastOther = now;
        return raw;
    }
    Axis &a = it.value();
    const int v = int(raw);
    if (a.lo < 0)
    {
        a.lo = a.hi = v;
        a.still = now;
    }
    else
    {
        const int lo = qMin(a.lo, v), hi = qMax(a.hi, v);
        if (hi - lo > kSteadyCounts)
        {
            a.lo = a.hi = v;
            a.still = now;
        }
        else
        {
            a.lo = lo;
            a.hi = hi;
        }
    }
    a.raw = v;
    return output(channel);
}

bool StickFilter::steady(const Axis &a, qint64 now) const
{
    return a.raw >= 0 && a.lo >= 0 && now - a.still >= kSteadyMs;
}

StickFilter::Tick StickFilter::tick(qint64 now)
{
    Tick t;
    const bool capturing = m_captureUntil >= 0;
    if (m_autoUntil >= 0 || capturing)
    {
        const double limit = capturing ? kCaptureLimit : kAutoLimit;
        for (auto it = m_axes.begin(); it != m_axes.end(); ++it)
        {
            Axis &a = it.value();
            if (a.captured || !steady(a, now))
                continue;
            const double at = toNorm((a.lo + a.hi) / 2.0);
            if (std::fabs(at) > limit)
                continue;               // held, or not a stick resting
            const uchar before = output(it.key());
            a.rest = at;
            a.learnt = true;
            a.captured = true;
            if (output(it.key()) != before)
                t.learnt << it.key();
        }
        if (m_autoUntil >= 0 && now >= m_autoUntil)
            m_autoUntil = -1;
        if (capturing)
        {
            bool all = true;
            int held = 0;
            for (const Axis &a : std::as_const(m_axes))
            {
                if (a.captured || a.raw < 0)     // never read: nothing to learn
                    continue;
                all = false;
                ++held;
            }
            if (all || now >= m_captureUntil)
            {
                t.captureDone = true;
                t.held = held;
                m_captureUntil = -1;
            }
        }
        return t;
    }

    /* Drift: a stick still past the deadzone, nothing on the controller
       touched for driftTime() -- said once. */
    if (m_driftSaid)
        return t;
    qint64 lastActivity = m_lastOther;
    for (const Axis &a : std::as_const(m_axes))
        if (a.raw >= 0)
            lastActivity = qMax(lastActivity, a.still);
    if (now - lastActivity < m_driftMs)
        return t;
    for (auto it = m_axes.constBegin(); it != m_axes.constEnd(); ++it)
    {
        const Axis &a = it.value();
        if (a.raw < 0)
            continue;
        const double off = correct(toNorm(a.raw), a.rest);
        if (std::fabs(off) > 2.0 * m_deadzone)
        {
            t.drifted = it.key();
            t.driftOffset = off;
            m_driftSaid = true;
            break;
        }
    }
    return t;
}

bool StickFilter::needsTick() const
{
    if (m_autoUntil >= 0 || m_captureUntil >= 0)
        return true;
    if (m_driftSaid)
        return false;
    for (const Axis &a : m_axes)
        if (a.raw >= 0 && std::fabs(correct(toNorm(a.raw), a.rest)) > 2.0 * m_deadzone)
            return true;
    return false;
}

void StickFilter::setRest(int channel, double rest)
{
    auto it = m_axes.find(channel);
    if (it == m_axes.end() || std::fabs(rest) > kCaptureLimit)
        return;
    it->rest = rest;
    it->learnt = true;
}

QMap<int, double> StickFilter::rests() const
{
    QMap<int, double> out;
    for (auto it = m_axes.constBegin(); it != m_axes.constEnd(); ++it)
        if (it->learnt)
            out.insert(it.key(), it->rest);
    return out;
}

uchar StickFilter::output(int channel) const
{
    const auto it = m_axes.constFind(channel);
    if (it == m_axes.constEnd() || it->raw < 0)
        return 128;
    if (!it->learnt || it->rest == 0.0)
        return uchar(it->raw);
    return fromNorm(correct(toNorm(it->raw), it->rest));
}

double StickFilter::toNorm(double value)
{
    return value >= 128.0 ? (value - 128.0) / 127.0 : (value - 128.0) / 128.0;
}

uchar StickFilter::fromNorm(double norm)
{
    norm = qBound(-1.0, norm, 1.0);
    const long v = norm >= 0.0 ? 128 + std::lround(norm * 127.0) : 128 + std::lround(norm * 128.0);
    return uchar(qBound(0L, v, 255L));
}

double StickFilter::correct(double norm, double rest)
{
    rest = qBound(-kCaptureLimit, rest, kCaptureLimit);
    const double out = norm >= rest ? (norm - rest) / (1.0 - rest) : (norm - rest) / (1.0 + rest);
    return qBound(-1.0, out, 1.0);
}

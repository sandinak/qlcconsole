/*
  Q Light Controller Plus
  ltcdecoder.cpp

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

#include <string.h>
#include "ltcdecoder.h"

#define LTC_SYNC_WORD   0x3FFD     // bits 64..79, first sent in the high bit
#define LTC_MIN_LEVEL   150.0f     // below this the input is silence, not code
#define LTC_BIT_RATE_MIN 1800.0f   // 24 fps x 80, with room for varispeed
#define LTC_BIT_RATE_MAX 2600.0f   // 30 fps x 80

LtcDecoder::LtcDecoder()
    : m_rate(0)
    , m_callback(nullptr)
    , m_context(nullptr)
{
    setSampleRate(48000);
}

void LtcDecoder::setSampleRate(int rate)
{
    if (rate <= 0)
        return;
    m_rate = rate;
    m_periodMin = float(rate) / LTC_BIT_RATE_MAX;
    m_periodMax = float(rate) / LTC_BIT_RATE_MIN;
    reset();
}

void LtcDecoder::reset()
{
    m_peak = 0;
    m_high = false;
    m_haveLevel = false;
    m_pos = 0;
    m_lastEdge = 0;
    m_haveEdge = false;
    m_period = float(m_rate) / 2160.0f;     // between 24 and 30 fps
    m_halfPending = false;
    m_valid = 0;
    memset(m_bits, 0, sizeof(m_bits));
    m_head = 0;
    m_last16 = 0;
    m_lastFrameEnd = 0;
    m_haveLastFrame = false;
    m_fps = 0;
}

void LtcDecoder::process(const int16_t *samples, int count, int stride)
{
    for (int i = 0; i < count; i++, m_pos++)
    {
        const float s = float(samples[i * stride]);
        const float a = s < 0 ? -s : s;

        // The signal's level: up at once, down slowly (about a second).
        if (a > m_peak)
            m_peak = a;
        else
            m_peak -= m_peak * (1.0f / float(m_rate));

        if (m_peak < LTC_MIN_LEVEL)
        {
            // Silence: the code is gone, so is what was learned of it.
            if (m_haveEdge || m_valid)
            {
                m_haveEdge = false;
                m_halfPending = false;
                m_valid = 0;
                m_haveLastFrame = false;
                m_high = false;
            }
            continue;
        }

        const float band = m_peak * 0.25f;
        if (!m_high && s > band)
        {
            m_high = true;
            edge(m_pos);
        }
        else if (m_high && s < -band)
        {
            m_high = false;
            edge(m_pos);
        }
    }
}

void LtcDecoder::edge(uint64_t at)
{
    if (!m_haveEdge)
    {
        m_haveEdge = true;
        m_lastEdge = at;
        return;
    }

    const float d = float(at - m_lastEdge);
    m_lastEdge = at;

    // Longer than two bits is a gap, not a bit.
    if (d > m_periodMax * 2.0f)
    {
        m_halfPending = false;
        m_valid = 0;
        m_haveLastFrame = false;
        return;
    }

    if (d >= m_period * 0.75f)
    {
        // A whole bit: a 0. A half bit waiting before it means we were
        // reading the middle of a bit as its start.
        if (m_halfPending)
        {
            m_halfPending = false;
            m_valid = 0;
            return;
        }
        m_period += (d - m_period) * 0.1f;
        if (m_period < m_periodMin) m_period = m_periodMin;
        if (m_period > m_periodMax) m_period = m_periodMax;
        bit(0, at);
    }
    else if (m_halfPending)
    {
        // Two halves: a 1.
        m_halfPending = false;
        bit(1, at);
    }
    else
    {
        m_halfPending = true;
        m_period += (d * 2.0f - m_period) * 0.05f;
        if (m_period < m_periodMin) m_period = m_periodMin;
        if (m_period > m_periodMax) m_period = m_periodMax;
    }
}

void LtcDecoder::bit(int value, uint64_t at)
{
    m_bits[m_head] = uint8_t(value);
    m_head = (m_head + 1) % 80;
    m_last16 = uint16_t((m_last16 << 1) | value);
    if (m_valid < 80)
        m_valid++;

    if (m_valid >= 80 && m_last16 == LTC_SYNC_WORD)
        sync(at);
}

int LtcDecoder::field(int start, int bits) const
{
    // Bit n of the frame is m_bits[(m_head + n) % 80], the oldest being bit 0.
    // Within a field the least significant bit goes first.
    int v = 0;
    for (int i = 0; i < bits; i++)
        v |= int(m_bits[(m_head + start + i) % 80]) << i;
    return v;
}

void LtcDecoder::sync(uint64_t at)
{
    Frame f;
    f.frames = field(0, 4) + 10 * field(8, 2);
    f.dropFrame = field(10, 1) != 0;
    f.seconds = field(16, 4) + 10 * field(24, 3);
    f.minutes = field(32, 4) + 10 * field(40, 3);
    f.hours = field(48, 4) + 10 * field(56, 2);
    f.endSample = at;

    // The rate from the samples since the last whole frame.
    int fps = 0;
    if (m_haveLastFrame)
    {
        // Less than most of a frame since the last one is a false sync, not a frame.
        if (float(at - m_lastFrameEnd) < m_period * 80.0f * 0.8f)
            return;
        const double perSecond = double(m_rate) / double(at - m_lastFrameEnd);
        if (perSecond >= 22.0 && perSecond < 24.5)       fps = 24;
        else if (perSecond >= 24.5 && perSecond < 27.5)  fps = 25;
        else if (perSecond >= 27.5 && perSecond < 32.0)  fps = 30;
    }
    m_lastFrameEnd = at;
    m_haveLastFrame = true;
    if (fps == 0)
        fps = m_fps;           // one missed frame: the rate we had
    else
        m_fps = fps;
    if (fps == 0)
        return;                // the first frame: rate not known yet
    if (f.dropFrame)
        fps = 30;
    f.fps = fps;

    if (f.hours > 23 || f.minutes > 59 || f.seconds > 59 || f.frames >= fps)
        return;

    if (m_callback != nullptr)
        m_callback(m_context, f);
}

uint32_t LtcDecoder::toMilliseconds(const Frame &frame, bool plusOneFrame)
{
    const int fps = frame.fps > 0 ? frame.fps : 30;
    if (frame.dropFrame)
    {
        // 29.97 df: frames 0 and 1 of each minute are skipped except every tenth.
        const int totalMinutes = 60 * frame.hours + frame.minutes;
        int64_t n = 30LL * (3600 * frame.hours + 60 * frame.minutes + frame.seconds) + frame.frames
                    - 2LL * (totalMinutes - totalMinutes / 10);
        if (plusOneFrame)
            n++;
        return uint32_t((n * 1001LL + 15) / 30);
    }
    int64_t n = fps * (3600LL * frame.hours + 60LL * frame.minutes + frame.seconds) + frame.frames;
    if (plusOneFrame)
        n++;
    return uint32_t((n * 1000LL + fps / 2) / fps);
}

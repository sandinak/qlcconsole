/*
  Q Light Controller Plus
  ltcdecoder.h

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

#ifndef LTCDECODER_H
#define LTCDECODER_H

#include <stdint.h>
#include "qlcengine_export.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * @brief SMPTE linear timecode (LTC) biphase-mark decoder.
 *
 * Feed it signed 16-bit audio, one channel, and it calls back with each
 * complete 80-bit frame read forward: 24, 25, 29.97 drop-frame and 30 fps,
 * either polarity (only the edges count). It never allocates, takes no
 * lock and holds no Qt object, so it runs on the audio thread as it is.
 *
 * Edges are the audio crossing zero through a small hysteresis band that
 * follows the signal's level. The gap between two edges is half a bit (a
 * 1) or a whole bit (a 0); the bit period is tracked as it drifts. A frame
 * is the 80 bits that end in the sync word 0011 1111 1111 1101. The frame
 * rate is measured from the samples between two frames, so the first frame
 * after a start or a dropout is held back until the second arrives.
 */
class QLC_ENGINE_EXPORT LtcDecoder
{
public:
    struct Frame
    {
        int hours = 0, minutes = 0, seconds = 0, frames = 0;
        bool dropFrame = false;
        /** Nominal frames per second: 24, 25 or 30 (30 for 29.97 df). */
        int fps = 0;
        /** Sample index (counted since reset) at which the frame's last bit ended. */
        uint64_t endSample = 0;
    };

    typedef void (*FrameCallback)(void *context, const Frame &frame);

    LtcDecoder();

    void setSampleRate(int rate);
    int sampleRate() const { return m_rate; }
    void setCallback(FrameCallback cb, void *context) { m_callback = cb; m_context = context; }

    /** Forget everything learned (level, bit period, frame rate). */
    void reset();

    /** Read @a count samples, @a stride apart (the channel of interleaved audio). */
    void process(const int16_t *samples, int count, int stride = 1);

    /** The position the frame's timecode names, in milliseconds. A frame is
     *  complete at its end, which is the start of the next one, so this is
     *  the code plus one frame: the time it is NOW. */
    static uint32_t toMilliseconds(const Frame &frame, bool plusOneFrame = true);

private:
    void edge(uint64_t at);
    void bit(int value, uint64_t at);
    void sync(uint64_t at);
    int field(int start, int bits) const;

    int m_rate;
    FrameCallback m_callback;
    void *m_context;

    // Level and edge detection
    float m_peak;
    bool m_high;
    bool m_haveLevel;
    uint64_t m_pos;           ///< samples consumed
    uint64_t m_lastEdge;
    bool m_haveEdge;

    // Bit slicing
    float m_period;           ///< one bit, in samples
    float m_periodMin, m_periodMax;
    bool m_halfPending;
    int m_valid;              ///< consecutive bits read cleanly (capped at 80)

    // The last 80 bits, oldest at m_head
    uint8_t m_bits[80];
    int m_head;
    uint16_t m_last16;

    // Frame rate from the spacing of two frames
    uint64_t m_lastFrameEnd;
    bool m_haveLastFrame;
    int m_fps;
};

/** @} */

#endif

/*
  Q Light Controller Plus
  ltcreceiver.h

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

#ifndef LTCRECEIVER_H
#define LTCRECEIVER_H

#include <stdint.h>
#include <QObject>
#include <QWeakPointer>
#include "ltcdecoder.h"
#include "qlcengine_export.h"

class Doc;
class AudioCapture;
class TimecodeSource;

/** @addtogroup engine Engine
 * @{
 */

/**
 * @brief LTC from the audio input as a third timecode source, beside MIDI
 * timecode and Art-Net TimeCode.
 *
 * It listens to the show's TimecodeSource: when the source picked is
 * TimecodeSource::LtcUniverse it takes the audio capture (the input device
 * chosen in the audio settings, shared with audio triggers), decodes the
 * first channel with an LtcDecoder on the capture thread, and pushes each
 * frame to TimecodeSource::updateTimeCode() -- the one follow path, so
 * Show following, the footer chip, sync health and the holding freeze all
 * work as they do for MTC. When another source is picked it lets go of the
 * capture.
 */
class QLC_ENGINE_EXPORT LtcReceiver : public QObject
{
    Q_OBJECT

public:
    /** @param useCapture false: take no audio capture (a test feeds
     *  processBlock() itself) */
    LtcReceiver(Doc *doc, TimecodeSource *source, bool useCapture = true);
    ~LtcReceiver();

    bool isEnabled() const { return m_enabled; }

    /** Audio thread: decode a block of interleaved 16-bit audio, taking
     *  the first channel. Called by AudioCapture under its lock. */
    void processBlock(const int16_t *samples, int frames, int channels, int sampleRate);

signals:
    /** Emitted from the audio thread for each decoded frame; queued to
     *  the main thread, where it reaches the TimecodeSource. */
    void positionDecoded(quint32 msPosition, uchar fps);

private slots:
    void slotSourceChanged();
    void slotPosition(quint32 msPosition, uchar fps);

private:
    static void frameThunk(void *context, const LtcDecoder::Frame &frame);
    void setEnabled(bool enabled);

    Doc *m_doc;
    TimecodeSource *m_source;
    bool m_enabled;
    bool m_useCapture;
    QWeakPointer<AudioCapture> m_capture;
    LtcDecoder m_decoder;
};

/** @} */

#endif

/*
  Q Light Controller Plus
  ltcreceiver.cpp

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

#include <QSharedPointer>
#include "ltcreceiver.h"
#include "timecodesource.h"
#include "audiocapture.h"
#include "doc.h"

LtcReceiver::LtcReceiver(Doc *doc, TimecodeSource *source, bool useCapture)
    : QObject(source)
    , m_doc(doc)
    , m_source(source)
    , m_enabled(false)
    , m_useCapture(useCapture)
{
    m_decoder.setCallback(&LtcReceiver::frameThunk, this);
    connect(this, &LtcReceiver::positionDecoded, this, &LtcReceiver::slotPosition,
            Qt::QueuedConnection);
    connect(source, &TimecodeSource::sourceUniverseChanged, this, &LtcReceiver::slotSourceChanged);
    slotSourceChanged();
}

LtcReceiver::~LtcReceiver()
{
    setEnabled(false);
}

void LtcReceiver::slotSourceChanged()
{
    setEnabled(m_source->effectiveUniverse() == TimecodeSource::LtcUniverse);
}

void LtcReceiver::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;

    if (!m_useCapture)
    {
        m_decoder.reset();
        return;
    }

    if (enabled)
    {
        m_decoder.reset();
        QSharedPointer<AudioCapture> capture = m_doc->audioInputCapture();
        m_capture = capture;
        capture->setLtcSink(this);
    }
    else
    {
        // The capture the sink was given to (a weak hold: it may be gone with the show)
        QSharedPointer<AudioCapture> capture = m_capture.toStrongRef();
        m_capture.clear();
        if (!capture.isNull())
            capture->setLtcSink(nullptr);
    }
}

void LtcReceiver::processBlock(const int16_t *samples, int frames, int channels, int sampleRate)
{
    if (m_decoder.sampleRate() != sampleRate)
        m_decoder.setSampleRate(sampleRate);
    m_decoder.process(samples, frames, channels > 0 ? channels : 1);
}

void LtcReceiver::frameThunk(void *context, const LtcDecoder::Frame &frame)
{
    LtcReceiver *self = static_cast<LtcReceiver *>(context);
    emit self->positionDecoded(quint32(LtcDecoder::toMilliseconds(frame)), uchar(frame.fps));
}

void LtcReceiver::slotPosition(quint32 msPosition, uchar fps)
{
    if (m_enabled)
        m_source->updateTimeCode(quint32(TimecodeSource::LtcUniverse), msPosition, fps);
}

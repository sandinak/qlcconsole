/*
  Q Light Controller Plus
  audiocapture_qt5.cpp

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

#include <QSettings>
#include <QDebug>
#include <QCoreApplication>

#include "audiocapture_qt5.h"
#if defined(Q_OS_MAC)
 #include "audiocapture_macpermission.h"
#endif

AudioCaptureQt5::AudioCaptureQt5(QObject * parent)
    : AudioCapture(parent)
    , m_audioInput(NULL)
    , m_input(NULL)
{
}

AudioCaptureQt5::~AudioCaptureQt5()
{
    stop();
    Q_ASSERT(m_audioInput == NULL);
}

bool AudioCaptureQt5::initialize()
{
#if defined(Q_OS_MAC)
    // macOS gates microphone access behind TCC. If it isn't granted the Qt
    // CoreAudio backend just streams silence with no error — the classic
    // "no signal" symptom. Check/prompt up front and fail loudly instead.
    MacAudioPermission perm = macCheckAudioInputPermission(true);
    if (perm != MacAudioPermissionAuthorized)
    {
        const QString msg = QString::fromUtf8(macAudioPermissionMessage(perm));
        qWarning() << "[AudioCapture]" << msg;
        emit captureError(msg);
        return false;
    }
#endif

    QSettings settings;
    QString devName = "";
    QAudioDeviceInfo audioDevice = QAudioDeviceInfo::defaultInputDevice();

    QVariant var = settings.value(SETTINGS_AUDIO_INPUT_DEVICE);
    if (var.isValid() == true)
    {
        devName = var.toString();
        foreach (const QAudioDeviceInfo &deviceInfo, QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
        {
            if (deviceInfo.deviceName() == devName)
            {
                audioDevice = deviceInfo;
                break;
            }
        }
    }

    m_format.setSampleRate(m_sampleRate);
    m_format.setChannelCount(m_channels);
    m_format.setSampleSize(16);
    m_format.setSampleType(QAudioFormat::SignedInt);
    m_format.setByteOrder(QAudioFormat::LittleEndian);
    m_format.setCodec("audio/pcm");

    if (!audioDevice.isFormatSupported(m_format))
    {
        qWarning() << "Requested format not supported - trying to use nearest";
        m_format = audioDevice.nearestFormat(m_format);
        m_channels = m_format.channelCount();
        m_sampleRate = m_format.sampleRate();
    }

    Q_ASSERT(m_audioInput == NULL);

    m_audioInput = new QAudioInput(audioDevice, m_format);

    if (m_audioInput == NULL)
    {
        qWarning() << "Cannot open audio input stream from device" << audioDevice.deviceName();
        return false;
    }

    m_input = m_audioInput->start();

    if (m_audioInput->state() == QAudio::StoppedState)
    {
        qWarning() << "Could not start input capture on device" << audioDevice.deviceName();
        delete m_audioInput;
        m_audioInput = NULL;
        m_input = NULL;
        return false;
    }

    m_currentReadBuffer.clear();

    return true;
}

void AudioCaptureQt5::uninitialize()
{
    Q_ASSERT(m_audioInput != NULL);

    m_audioInput->stop();
    delete m_audioInput;
    m_audioInput = NULL;
}

qint64 AudioCaptureQt5::latency() const
{
    return 0; // TODO
}

void AudioCaptureQt5::setVolume(qreal volume)
{
    if (volume == m_volume)
        return;

    m_volume = volume;
    if (m_audioInput != NULL)
        m_audioInput->setVolume(volume);

    emit volumeChanged(volume * 100.0);
}

void AudioCaptureQt5::suspend()
{
}

void AudioCaptureQt5::resume()
{
}

bool AudioCaptureQt5::readAudio(int maxSize)
{
    if (m_audioInput == NULL || m_input == NULL)
        return false;

    int bufferSize = maxSize * sizeof(*m_audioBuffer);

    m_currentReadBuffer += m_input->readAll();

    if (m_currentReadBuffer.size() < bufferSize)
        return false; // not a full frame captured yet

    // Bound capture latency. macOS CoreAudio hands us audio in bursts, and a busy
    // GUI thread can stall this capture thread — either way the backlog would grow
    // and, because we only consume the OLDEST frame each call, we'd process ever-
    // staler audio and the reactive output would drift further behind the music
    // every cycle. If more than a couple of frames have queued, drop the stale
    // whole-frames and keep only the most recent one (whole-frame granularity so
    // the channel interleaving in m_audioBuffer stays aligned).
    const int frames = m_currentReadBuffer.size() / bufferSize;
    if (frames > 2)
        m_currentReadBuffer.remove(0, (frames - 1) * bufferSize);

    memcpy(m_audioBuffer, m_currentReadBuffer.constData(), bufferSize);
    m_currentReadBuffer.remove(0, bufferSize);
    return true;
}

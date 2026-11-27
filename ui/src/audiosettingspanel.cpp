/*
  Q Light Controller Plus
  audiosettingspanel.cpp

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

#include <QFormLayout>
#include <QProgressBar>
#include <QToolButton>
#include <QSettings>
#include <QComboBox>
#include <QSharedPointer>
#include <QHBoxLayout>

#include "audiosettingspanel.h"
#include "audioplugincache.h"
#include "audiocapture.h"
#include "audiorenderer.h"
#include "doc.h"

AudioSettingsPanel::AudioSettingsPanel(Doc *doc, QWidget *parent)
    : QGroupBox(tr("Audio"), parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("connAudio"));
    setToolTip(tr("The sound card this computer listens to (for audio triggers and beats) and plays "
                  "through. These are the computer's, not the universe's."));
    QFormLayout *form = new QFormLayout(this);

    m_input = new QComboBox(this);
    m_input->setObjectName(QStringLiteral("connAudioInput"));
    m_output = new QComboBox(this);
    m_output->setObjectName(QStringLiteral("connAudioOutput"));
    m_input->addItem(tr("Default device"), QString());
    m_output->addItem(tr("Default device"), QString());
    QSettings settings;
    const QString inName = settings.value(SETTINGS_AUDIO_INPUT_DEVICE).toString();
    const QString outName = settings.value(SETTINGS_AUDIO_OUTPUT_DEVICE).toString();
    if (m_doc->audioPluginCache() != nullptr)
    {
        foreach (AudioDeviceInfo info, m_doc->audioPluginCache()->audioDevicesList())
        {
            if (info.capabilities & AUDIO_CAP_INPUT)
                m_input->addItem(info.deviceName, info.privateName);
            if (info.capabilities & AUDIO_CAP_OUTPUT)
                m_output->addItem(info.deviceName, info.privateName);
        }
    }
    m_input->setCurrentIndex(qMax(0, m_input->findData(inName)));
    m_output->setCurrentIndex(qMax(0, m_output->findData(outName)));
    form->addRow(tr("Input device:"), m_input);
    form->addRow(tr("Output device:"), m_output);

    m_rate = new QComboBox(this);
    m_rate->setObjectName(QStringLiteral("connAudioRate"));
    foreach (int r, QList<int>() << 8000 << 11025 << 16000 << 22050 << 32000 << 44100 << 48000 << 96000)
        m_rate->addItem(QString::number(r));
    m_rate->setCurrentIndex(qMax(0, m_rate->findText(QString::number(
        settings.value(SETTINGS_AUDIO_INPUT_SRATE, AUDIO_DEFAULT_SAMPLE_RATE).toInt()))));
    form->addRow(tr("Sample rate:"), m_rate);

    m_channels = new QComboBox(this);
    m_channels->setObjectName(QStringLiteral("connAudioChannels"));
    m_channels->addItem(tr("Mono"), 1);
    m_channels->addItem(tr("Stereo"), 2);
    m_channels->setCurrentIndex(settings.value(SETTINGS_AUDIO_INPUT_CHANNELS, AUDIO_DEFAULT_CHANNELS).toInt() == 2 ? 1 : 0);
    form->addRow(tr("Channels:"), m_channels);

    QWidget *meter = new QWidget(this);
    QHBoxLayout *ml = new QHBoxLayout(meter);
    ml->setContentsMargins(0, 0, 0, 0);
    m_level = new QProgressBar(meter);
    m_level->setObjectName(QStringLiteral("connAudioLevel"));
    m_level->setRange(0, 32767);
    m_level->setTextVisible(false);
    m_preview = new QToolButton(meter);
    m_preview->setObjectName(QStringLiteral("connAudioPreview"));
    m_preview->setText(tr("Listen"));
    m_preview->setCheckable(true);
    m_preview->setToolTip(tr("Show the level coming in from the input device"));
    ml->addWidget(m_level, 1);
    ml->addWidget(m_preview);
    form->addRow(tr("Level:"), meter);

    connect(m_input, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioSettingsPanel::slotInput);
    connect(m_output, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioSettingsPanel::slotOutput);
    connect(m_rate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioSettingsPanel::slotRate);
    connect(m_channels, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioSettingsPanel::slotChannels);
    connect(m_preview, &QToolButton::toggled, this, &AudioSettingsPanel::slotPreview);
}

AudioSettingsPanel::~AudioSettingsPanel()
{
    if (m_preview->isChecked())
        m_preview->setChecked(false);
}

void AudioSettingsPanel::changed()
{
    if (m_preview->isChecked())
        m_preview->setChecked(false);
    m_doc->destroyAudioCapture();       // reopened with what was chosen
}

void AudioSettingsPanel::slotInput(int index)
{
    QSettings settings;
    const QString name = m_input->itemData(index).toString();
    if (name.isEmpty())
        settings.remove(SETTINGS_AUDIO_INPUT_DEVICE);
    else
        settings.setValue(SETTINGS_AUDIO_INPUT_DEVICE, name);
    changed();
}

void AudioSettingsPanel::slotOutput(int index)
{
    QSettings settings;
    const QString name = m_output->itemData(index).toString();
    if (name.isEmpty())
        settings.remove(SETTINGS_AUDIO_OUTPUT_DEVICE);
    else
        settings.setValue(SETTINGS_AUDIO_OUTPUT_DEVICE, name);
}

void AudioSettingsPanel::slotRate(int index)
{
    QSettings settings;
    const int rate = m_rate->itemText(index).toInt();
    if (rate == AUDIO_DEFAULT_SAMPLE_RATE)
        settings.remove(SETTINGS_AUDIO_INPUT_SRATE);
    else
        settings.setValue(SETTINGS_AUDIO_INPUT_SRATE, rate);
    changed();
}

void AudioSettingsPanel::slotChannels(int index)
{
    QSettings settings;
    const int channels = m_channels->itemData(index).toInt();
    if (channels == AUDIO_DEFAULT_CHANNELS)
        settings.remove(SETTINGS_AUDIO_INPUT_CHANNELS);
    else
        settings.setValue(SETTINGS_AUDIO_INPUT_CHANNELS, channels);
    changed();
}

void AudioSettingsPanel::slotPreview(bool on)
{
    if (on)
    {
        QSharedPointer<AudioCapture> capture(m_doc->audioInputCapture());
        m_capture = capture.data();
        if (m_capture == nullptr)
            return;
        m_peak = 0;
        m_level->setValue(0);
        connect(m_capture, SIGNAL(dataProcessed(double*,int,double,quint32)),
                this, SLOT(slotLevel(double*,int,double,quint32)));
        m_capture->registerBandsNumber(FREQ_SUBBANDS_DEFAULT_NUMBER);
    }
    else if (m_capture != nullptr)
    {
        m_capture->unregisterBandsNumber(FREQ_SUBBANDS_DEFAULT_NUMBER);
        disconnect(m_capture, SIGNAL(dataProcessed(double*,int,double,quint32)),
                   this, SLOT(slotLevel(double*,int,double,quint32)));
        m_capture = nullptr;
        m_level->setValue(0);
    }
}

void AudioSettingsPanel::slotLevel(double *, int, double, quint32 power)
{
    // A decaying peak, as the Universes tab's meter: a beat pulses visibly.
    m_peak = qMax(int(power), (m_peak * 7) / 8);
    m_level->setValue(qMin(m_peak, m_level->maximum()));
}

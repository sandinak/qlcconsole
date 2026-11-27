/*
  Q Light Controller Plus
  audiosettingspanel.h

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

#ifndef AUDIOSETTINGSPANEL_H
#define AUDIOSETTINGSPANEL_H

#include <QGroupBox>

class QComboBox;
class QProgressBar;
class QToolButton;
class AudioCapture;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The show computer's audio input and output device, and the input's sample
 * rate and channels, with a level meter: the same settings as the Universes
 * tab's Audio page, so Connections ▸ Devices has them too.
 */
class AudioSettingsPanel : public QGroupBox
{
    Q_OBJECT

public:
    AudioSettingsPanel(Doc *doc, QWidget *parent = nullptr);
    ~AudioSettingsPanel();

private slots:
    void slotInput(int index);
    void slotOutput(int index);
    void slotRate(int index);
    void slotChannels(int index);
    void slotPreview(bool on);
    void slotLevel(double *bands, int size, double maxMagnitude, quint32 power);

private:
    void changed();

    Doc *m_doc;
    QComboBox *m_input;
    QComboBox *m_output;
    QComboBox *m_rate;
    QComboBox *m_channels;
    QProgressBar *m_level;
    QToolButton *m_preview;
    AudioCapture *m_capture = nullptr;
    int m_peak = 0;
};

/** @} */

#endif

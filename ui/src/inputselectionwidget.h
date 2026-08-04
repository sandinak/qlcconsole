/*
  Q Light Controller Plus
  inputselectionwidget.h

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

#ifndef INPUTSELECTIONWIDGET_H
#define INPUTSELECTIONWIDGET_H

#include <QKeySequence>
#include <QWidget>

#include "ui_inputselectionwidget.h"
#include "qlcinputsource.h"

class Doc;
class QComboBox;
class QSpinBox;
class QGroupBox;
class QDial;
class QLabel;
class QCheckBox;
class QSlider;

class InputSelectionWidget final : public QWidget, public Ui_InputSelectionWidget
{
    Q_OBJECT

public:
    InputSelectionWidget(Doc *doc, QWidget *parent = 0);
    ~InputSelectionWidget();

    void setKeyInputVisibility(bool visible);
    void setCustomFeedbackVisibility(bool visible);
    void setMonitoringSupport(bool enable);
    void setTitle(QString title);
    void setWidgetPage(int page);
    bool isAutoDetecting();
    void stopAutoDetection();
    void emitOddValues(bool enable);

    void setKeySequence(const QKeySequence& keySequence);
    QKeySequence keySequence() const;

    void setInputSource(QSharedPointer<QLCInputSource> const& source);
    QSharedPointer<QLCInputSource> inputSource() const;

protected slots:
    void slotAttachKey();
    void slotDetachKey();

    void slotAutoDetectInputToggled(bool checked);
    void slotInputValueChanged(quint32 universe, quint32 channel);
    void slotChooseInputClicked();

    void slotCustomFeedbackClicked();

    /** Apply the "Encoder behaviour" combo/step to the current input source. */
    void slotBehaviourChanged();

    /** Live preview: turn the on-screen dial and show raw value + decoded step
     *  as the physical control is moved, so the right encoding is obvious. */
    void slotPreviewInput(quint32 universe, quint32 channel, uchar value);

signals:
    void autoDetectToggled(bool checked);
    void inputValueChanged(quint32 universe, quint32 channel);
    void keySequenceChanged(QKeySequence key);

protected:
    void updateInputSource();

private:
    Doc *m_doc;
    QKeySequence m_keySequence;
    QSharedPointer<QLCInputSource> m_inputSource;
    int m_widgetPage;
    bool m_emitOdd;
    bool m_supportMonitoring;
    quint32 m_signalsReceived;

    // "Encoder behaviour" controls (relative-encoder mapping without a profile).
    QGroupBox *m_relativeGroup;
    QComboBox *m_behaviourCombo;
    QSpinBox  *m_sensitivitySpin;
    QCheckBox *m_invertCheck;    //!< reverse turn direction
    QDial     *m_previewDial;    //!< live "knob" that turns with the physical control
    QSlider   *m_previewSlider;  //!< live fader preview (shown for Absolute inputs)
    QLabel    *m_previewLabel;   //!< raw value + decoded step readout
    int        m_previewAccum;   //!< accumulated preview position (relative modes)
};

#endif // INPUTSELECTIONWIDGET_H

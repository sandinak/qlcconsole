/*
  Q Light Controller Plus
  inputselectionwidget.cpp

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

#include <QDebug>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QDial>

#include "inputoutputmap.h"

#include "customfeedbackdialog.h"
#include "inputselectionwidget.h"
#include "selectinputchannel.h"
#include "qlcinputchannel.h"
#include "qlcinputsource.h"
#include "assignhotkey.h"
#include "inputpatch.h"
#include "doc.h"

InputSelectionWidget::InputSelectionWidget(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_widgetPage(0)
    , m_emitOdd(false)
    , m_supportMonitoring(false)
    , m_signalsReceived(0)
{
    Q_ASSERT(doc != NULL);

    setupUi(this);

    m_customFbButton->setVisible(false);

    connect(m_attachKey, SIGNAL(clicked()), this, SLOT(slotAttachKey()));
    connect(m_detachKey, SIGNAL(clicked()), this, SLOT(slotDetachKey()));

    connect(m_autoDetectInputButton, SIGNAL(toggled(bool)),
            this, SLOT(slotAutoDetectInputToggled(bool)));
    connect(m_chooseInputButton, SIGNAL(clicked()),
            this, SLOT(slotChooseInputClicked()));

    connect(m_customFbButton, SIGNAL(clicked(bool)),
            this, SLOT(slotCustomFeedbackClicked()));

    /* "Encoder behaviour" group — lets an endless/relative knob be mapped to
       this control directly, without hand-editing an input profile. A live
       dial turns with the physical control so the correct encoding is obvious. */
    m_previewAccum = 127;
    m_relativeGroup = new QGroupBox(tr("Encoder behaviour"), this);
    QVBoxLayout *rv = new QVBoxLayout(m_relativeGroup);
    rv->setContentsMargins(6, 4, 6, 4);

    QHBoxLayout *rl = new QHBoxLayout;
    m_behaviourCombo = new QComboBox(m_relativeGroup);
    m_behaviourCombo->addItem(tr("Absolute (fader / knob)"));
    m_behaviourCombo->addItem(tr("Relative — two's complement"));
    m_behaviourCombo->addItem(tr("Relative — sign / magnitude"));
    m_behaviourCombo->addItem(tr("Relative — binary offset"));
    m_behaviourCombo->setToolTip(tr("How this input drives the control. The 'Relative' "
        "modes treat an endless/'free' knob's messages as increments; two's "
        "complement suits most controllers (e.g. OpenDeck)."));
    m_sensitivitySpin = new QSpinBox(m_relativeGroup);
    m_sensitivitySpin->setRange(1, 127);
    m_sensitivitySpin->setValue(4);
    m_sensitivitySpin->setToolTip(tr("Amount the control moves per encoder detent"));
    rl->addWidget(new QLabel(tr("Behaviour:"), m_relativeGroup));
    rl->addWidget(m_behaviourCombo, 1);
    rl->addWidget(new QLabel(tr("Step:"), m_relativeGroup));
    rl->addWidget(m_sensitivitySpin);
    rv->addLayout(rl);

    // Live preview row: turn the physical control and watch this dial + readout.
    QHBoxLayout *pl = new QHBoxLayout;
    m_previewDial = new QDial(m_relativeGroup);
    m_previewDial->setWrapping(true);
    m_previewDial->setNotchesVisible(true);
    m_previewDial->setRange(0, 255);
    m_previewDial->setValue(m_previewAccum);
    m_previewDial->setFixedSize(48, 48);
    m_previewDial->setEnabled(false);   // display only — driven by live input
    m_previewDial->setToolTip(tr("Live preview: turn your control and watch this dial. "
        "Smooth motion in the direction you turn = correct encoding."));
    m_previewLabel = new QLabel(tr("Turn the control to test…"), m_relativeGroup);
    pl->addWidget(new QLabel(tr("Live:"), m_relativeGroup));
    pl->addWidget(m_previewDial);
    pl->addWidget(m_previewLabel, 1);
    rv->addLayout(pl);

    if (QGridLayout *gl = qobject_cast<QGridLayout *>(layout()))
        gl->addWidget(m_relativeGroup, gl->rowCount(), 0, 1, qMax(1, gl->columnCount()));
    else if (layout() != NULL)
        layout()->addWidget(m_relativeGroup);

    connect(m_behaviourCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotBehaviourChanged()));
    connect(m_sensitivitySpin, SIGNAL(valueChanged(int)), this, SLOT(slotBehaviourChanged()));

    // Persistent live-preview tap on raw input (independent of auto-detect).
    connect(m_doc->inputOutputMap(), SIGNAL(inputValueChanged(quint32,quint32,uchar)),
            this, SLOT(slotPreviewInput(quint32,quint32,uchar)));
}

InputSelectionWidget::~InputSelectionWidget()
{
}

void InputSelectionWidget::setKeyInputVisibility(bool visible)
{
    m_keyInputGroup->setVisible(visible);
}

void InputSelectionWidget::setCustomFeedbackVisibility(bool visible)
{
    m_customFbButton->setVisible(visible);
}

void InputSelectionWidget::setMonitoringSupport(bool enable)
{
    m_supportMonitoring = enable;
}

void InputSelectionWidget::setTitle(QString title)
{
    m_extInputGroup->setTitle(title);
}

void InputSelectionWidget::setWidgetPage(int page)
{
    m_widgetPage = page;
}

bool InputSelectionWidget::isAutoDetecting()
{
   return m_autoDetectInputButton->isChecked();
}

void InputSelectionWidget::stopAutoDetection()
{
    if (m_autoDetectInputButton->isChecked())
        m_autoDetectInputButton->toggle();
}

void InputSelectionWidget::emitOddValues(bool enable)
{
    m_emitOdd = enable;
}

void InputSelectionWidget::setKeySequence(const QKeySequence &keySequence)
{
    m_keySequence = QKeySequence(keySequence);
    m_keyEdit->setText(m_keySequence.toString(QKeySequence::NativeText));
}

QKeySequence InputSelectionWidget::keySequence() const
{
    return m_keySequence;
}

void InputSelectionWidget::setInputSource(const QSharedPointer<QLCInputSource> &source)
{
    m_inputSource = source;
    updateInputSource();
}

QSharedPointer<QLCInputSource> InputSelectionWidget::inputSource() const
{
    return m_inputSource;
}

void InputSelectionWidget::slotAttachKey()
{
    AssignHotKey ahk(this, m_keySequence);
    if (ahk.exec() == QDialog::Accepted)
    {
        setKeySequence(QKeySequence(ahk.keySequence()));
        emit keySequenceChanged(m_keySequence);
    }
}

void InputSelectionWidget::slotDetachKey()
{
    setKeySequence(QKeySequence());
    emit keySequenceChanged(m_keySequence);
}

void InputSelectionWidget::slotAutoDetectInputToggled(bool checked)
{
    if (checked == true)
    {
        connect(m_doc->inputOutputMap(),
                SIGNAL(inputValueChanged(quint32,quint32,uchar)),
                this, SLOT(slotInputValueChanged(quint32,quint32)));
    }
    else
    {
        disconnect(m_doc->inputOutputMap(),
                   SIGNAL(inputValueChanged(quint32,quint32,uchar)),
                   this, SLOT(slotInputValueChanged(quint32,quint32)));
    }
    emit autoDetectToggled(checked);
}

void InputSelectionWidget::slotInputValueChanged(quint32 universe, quint32 channel)
{
    if (m_emitOdd == true && m_signalsReceived % 2)
    {
        emit inputValueChanged(universe, (m_widgetPage << 16) | channel);
        m_signalsReceived++;
        return;
    }

    m_inputSource = QSharedPointer<QLCInputSource>(new QLCInputSource(universe, (m_widgetPage << 16) | channel));
    updateInputSource();
    m_signalsReceived++;

    if (m_emitOdd == false)
        emit inputValueChanged(universe, (m_widgetPage << 16) | channel);
}

void InputSelectionWidget::slotChooseInputClicked()
{
    SelectInputChannel sic(this, m_doc->inputOutputMap());
    if (sic.exec() == QDialog::Accepted)
    {
        uchar lowerValue = 0;
        uchar upperValue = 0;
        uchar monitorValue = 0;
        QVariant extraLowerParams;
        QVariant extraUpperParams;
        QVariant extraMonitorParams;
        if (!m_inputSource.isNull())
        {
            lowerValue = m_inputSource->feedbackValue(QLCInputFeedback::LowerValue);
            upperValue = m_inputSource->feedbackValue(QLCInputFeedback::UpperValue);
            monitorValue = m_inputSource->feedbackValue(QLCInputFeedback::MonitorValue);
            extraLowerParams = m_inputSource->feedbackExtraParams(QLCInputFeedback::LowerValue);
            extraUpperParams = m_inputSource->feedbackExtraParams(QLCInputFeedback::UpperValue);
            extraMonitorParams = m_inputSource->feedbackExtraParams(QLCInputFeedback::MonitorValue);
        }
        m_inputSource = QSharedPointer<QLCInputSource>(new QLCInputSource(sic.universe(), (m_widgetPage << 16) | sic.channel()));
        if (!m_inputSource.isNull())
        {
            if (lowerValue != m_inputSource->feedbackValue(QLCInputFeedback::LowerValue))
                m_inputSource->setFeedbackValue(QLCInputFeedback::LowerValue, lowerValue);
            if (upperValue != m_inputSource->feedbackValue(QLCInputFeedback::UpperValue))
                m_inputSource->setFeedbackValue(QLCInputFeedback::UpperValue, upperValue);
            if (monitorValue != m_inputSource->feedbackValue(QLCInputFeedback::MonitorValue))
                m_inputSource->setFeedbackValue(QLCInputFeedback::MonitorValue, monitorValue);
            if (extraLowerParams.isValid())
                m_inputSource->setFeedbackExtraParams(QLCInputFeedback::LowerValue, extraLowerParams);
            if (extraUpperParams.isValid())
                m_inputSource->setFeedbackExtraParams(QLCInputFeedback::UpperValue, extraUpperParams);
            if (extraMonitorParams.isValid())
                m_inputSource->setFeedbackExtraParams(QLCInputFeedback::MonitorValue, extraMonitorParams);
        }
        updateInputSource();
        emit inputValueChanged(sic.universe(), (m_widgetPage << 16) | sic.channel());
    }
}

void InputSelectionWidget::slotCustomFeedbackClicked()
{
    CustomFeedbackDialog cfDialog(m_doc, m_inputSource, this);
    cfDialog.setMonitoringVisibility(m_supportMonitoring);
    cfDialog.exec();
}

void InputSelectionWidget::updateInputSource()
{
    QString uniName;
    QString chName;

    if (!m_inputSource || m_doc->inputOutputMap()->inputSourceNames(m_inputSource, uniName, chName) == false)
    {
        uniName = KInputNone;
        chName = KInputNone;
    }

    m_inputUniverseEdit->setText(uniName);
    m_inputChannelEdit->setText(chName);

    // Reflect the source's relative-encoder behaviour in the combo/step.
    const bool valid = !m_inputSource.isNull() && m_inputSource->isValid();
    m_relativeGroup->setEnabled(valid);

    // Reset the live preview for the new source.
    m_previewAccum = 127;
    if (m_previewDial != NULL)
        m_previewDial->setValue(m_previewAccum);
    if (m_previewLabel != NULL)
        m_previewLabel->setText(valid ? tr("Turn the control to test…")
                                      : tr("Assign an input first"));
    m_behaviourCombo->blockSignals(true);
    m_sensitivitySpin->blockSignals(true);
    int idx = 0;
    if (valid && m_inputSource->workingMode() == QLCInputSource::Encoder)
        idx = 1 + int(m_inputSource->relativeEncoding());   // TwosComplement=0 → index 1
    else if (valid && m_inputSource->workingMode() == QLCInputSource::Relative)
        idx = 1;                                            // legacy relative → show as two's complement
    m_behaviourCombo->setCurrentIndex(idx);
    if (valid)
        m_sensitivitySpin->setValue(qBound(1, m_inputSource->sensitivity(), 127));
    m_behaviourCombo->blockSignals(false);
    m_sensitivitySpin->blockSignals(false);
}

void InputSelectionWidget::slotBehaviourChanged()
{
    if (m_inputSource.isNull())
        return;

    const int idx = m_behaviourCombo->currentIndex();
    if (idx <= 0)
    {
        m_inputSource->setWorkingMode(QLCInputSource::Absolute);
    }
    else
    {
        m_inputSource->setWorkingMode(QLCInputSource::Encoder);
        QLCInputSource::RelativeEncoding enc = QLCInputSource::TwosComplement;
        if (idx == 2)      enc = QLCInputSource::SignedBit;
        else if (idx == 3) enc = QLCInputSource::BinaryOffset;
        m_inputSource->setRelativeEncoding(enc);
    }
    m_inputSource->setSensitivity(m_sensitivitySpin->value());
}

void InputSelectionWidget::slotPreviewInput(quint32 universe, quint32 channel, uchar value)
{
    if (m_inputSource.isNull() || m_inputSource->isValid() == false)
        return;
    // Match this source (ignore the page bits packed into the channel).
    if (m_inputSource->universe() != universe ||
        (m_inputSource->channel() & 0xFFFF) != (channel & 0xFFFF))
        return;

    if (m_behaviourCombo->currentIndex() <= 0)
    {
        // Absolute: the dial simply follows the incoming value.
        m_previewAccum = value;
        m_previewDial->setValue(value);
        m_previewLabel->setText(tr("value %1").arg(value));
        return;
    }

    // Relative: decode the signed step under the selected encoding and move the
    // dial by delta * step, wrapping so an endless knob keeps turning.
    const int delta = m_inputSource->decodeRelativeDelta(value);
    const int step  = qMax(1, m_sensitivitySpin->value());
    int pos = m_previewAccum + delta * step;
    while (pos < 0)   pos += 256;
    while (pos > 255) pos -= 256;
    m_previewAccum = pos;
    m_previewDial->setValue(pos);
    m_previewLabel->setText(tr("raw %1  →  step %2%3")
                            .arg(value >> 1)
                            .arg(delta >= 0 ? QStringLiteral("+") : QString())
                            .arg(delta));
}

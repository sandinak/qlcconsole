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
#include <QPointer>
#include <QDialog>
#include <functional>
#include <QWidget>

#include "ui_inputselectionwidget.h"
#include "qlcinputsource.h"
#include "controlowners.h"
#include "qlcui_export.h"

class Doc;
class QComboBox;
class QSpinBox;
class QGroupBox;
class QDial;
class QLabel;
class QCheckBox;
class QSlider;
class ControlCollisionBox;

class QLC_UI_EXPORT InputSelectionWidget final : public QWidget, public Ui_InputSelectionWidget
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

    /** Who this picks an input for (a Virtual Console widget unless said:
     *  a speed master, a channel group). A control another listener has
     *  is refused, naming it, with Move It Here (ControlOwners, Q297). */
    void setListener(const ControlOwners::Owner &self) { m_self = self; }
    /** The refusal (hidden when there is none). */
    ControlCollisionBox *collisionBox() const { return m_collision; }

    /** Listeners this window stands in for until OK: a speed master's
     *  Inputs… sets all three of its inputs from its pickers, so its own
     *  Tap, Rate and Pause are what the other pickers hold now, not what
     *  the show has. */
    struct LocalOwners
    {
        /** Whether the show's @a owner is one this window sets on OK (it is
         *  then asked of owners(), not of the show). */
        std::function<bool(const ControlOwners::Owner &owner)> covers;
        /** Which of them hold @a channel of @a universe in this window now. */
        std::function<QList<ControlOwners::Owner>(quint32 universe, quint32 channel)> owners;
        /** Takes the control from one of them, in this window only. */
        std::function<void(const ControlOwners::Owner &owner)> release;
    };
    void setLocalOwners(const LocalOwners &local) { m_local = local; }

    /** The show's listeners Move It Here takes the control from when the
     *  window this is in is accepted (empty: none waiting). Outside a
     *  dialog it takes it at once. */
    QList<ControlOwners::Owner> pendingMove() const;

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
    /** Whether another listener has the control: refused (the box shows
     *  it) and true. @a keepFeedback: Choose… keeps the feedback values. */
    bool refuseTaken(quint32 universe, quint32 channelWithPage, bool keepFeedback);
    /** Sets the source to the control, as Choose… or auto-detect does. */
    void takeSource(quint32 universe, quint32 channelWithPage, bool keepFeedback);
    /** The window was accepted: each Move It Here's control is taken from
     *  its owners if the source it set is still kept on it (by this picker
     *  or by the preset it was set for, when one picker edits many). */
    void commitMove();
    bool isLocal(const ControlOwners::Owner &owner) const;
    /** Shows what @a channelWithPage of @a universe does now under the
     *  fields ("Now: Grand Master (this computer's desk)", or "free"),
     *  before anything is accepted. */
    void showJobs(quint32 universe, quint32 channelWithPage);

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

    ControlOwners::Owner m_self;
    ControlCollisionBox *m_collision;
    QLabel *m_jobLabel = nullptr;
    quint64 m_jobsKey = ~quint64(0);
    bool m_pendingKeepFeedback = false;
    LocalOwners m_local;
    /** One Move It Here waiting for OK: the source it set, weakly (a
     *  window that edits many presets with this picker keeps each one's). */
    struct PendingMove
    {
        QList<ControlOwners::Owner> owners;
        quint32 universe = 0;
        quint32 channel = 0;
        QWeakPointer<QLCInputSource> source;
    };
    QList<PendingMove> m_pendingMoves;
    QPointer<QDialog> m_hooked;
};

#endif // INPUTSELECTIONWIDGET_H

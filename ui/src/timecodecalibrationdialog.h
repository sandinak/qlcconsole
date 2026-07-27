/*
  Q Light Controller Plus
  timecodecalibrationdialog.h

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

#ifndef TIMECODECALIBRATIONDIALOG_H
#define TIMECODECALIBRATIONDIALOG_H

#include <QDialog>
#include <QList>

class QDoubleSpinBox;
class QComboBox;
class QSpinBox;
class QLabel;
class QPushButton;
class QListWidget;
class QTimer;
class Doc;
class Show;

/** @addtogroup ui UI
 * @{
 */

/**
 * Assisted calibration of a Show's MTC timeline offset.
 *
 * The per-show timecodeOffset (the timecode value that maps to timeline 0) used
 * to be set only by three coarse presets. This tool lets the operator TAP in
 * time with the music against a chosen reference (timeline start, or any section
 * marker) while the show follows MTC; each tap yields offset = capturedTC −
 * reference. Averaging several taps — typically across a couple of rehearsal
 * rolls — cancels reaction jitter and yields a repeatable suggested offset the
 * operator can Apply (and trim ±10 ms by feel).
 *
 * The feed's live sync-health (rate/jitter) is shown so taps taken against an
 * unstable clock can be recognised.
 */
class TimecodeCalibrationDialog : public QDialog
{
    Q_OBJECT

public:
    TimecodeCalibrationDialog(Doc *doc, Show *show, QWidget *parent = 0);
    ~TimecodeCalibrationDialog();

protected:
    /** Space bar anywhere in the dialog = a tap (unless a combo popup ate it). */
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void slotTap();
    void slotClear();
    void slotApply();
    void slotNudge();        //< sender() carries ±ms in its property
    void slotLiveRefresh();  //< live timecode readout + feed health
    void slotReferenceChanged();
    /** Toggle audio click-track auto-tap: drive the beat generator from the
     *  audio input and sample the offset on every detected onset. */
    void slotListenToggled(bool on);
    /** One detected audio onset → one grid-disambiguated offset sample. */
    void onAudioBeat();
    /** Reference/BPM changed → the beat grid moved; drop stale samples. */
    void slotGridChanged();

private:
    /** Selected reference timeline position, in ms. */
    qint64 referenceMs() const;
    /** Recompute mean/spread from the tap samples + refresh the labels. */
    void refreshStats();
    /** Append one offset sample (shared by manual tap + audio auto-tap). */
    void addSample(qint64 offsetMs, const QString &rowText, bool shaky);
    /** Stop audio auto-tap + restore the previous beat generator. */
    void stopListening();

private:
    Doc *m_doc;
    Show *m_show;

    QComboBox   *m_refCombo;
    QLabel      *m_liveLabel;
    QLabel      *m_healthLabel;
    QLabel      *m_currentOffsetLabel;
    QListWidget *m_tapList;
    QLabel      *m_statsLabel;
    QLabel      *m_suggestLabel;
    QPushButton *m_tapBtn;
    QPushButton *m_applyBtn;
    QTimer      *m_liveTimer;

    // Audio click-track auto-tap.
    QDoubleSpinBox *m_bpmSpin;
    QSpinBox       *m_latencySpin;   //< detection-latency correction (ms)
    QPushButton    *m_listenBtn;
    QLabel         *m_listenInfo;
    bool    m_listening;
    qint64  m_listenCoarse;          //< offset snapshot used to disambiguate beat
    int     m_prevBeatType;          //< beat generator to restore on stop

    QList<qint64> m_offsets;   //< per-tap computed offsets (signed ms)
    qint64 m_suggested;        //< current suggested offset (mean ± nudges)
};

/** @} */

#endif // TIMECODECALIBRATIONDIALOG_H

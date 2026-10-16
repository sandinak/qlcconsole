/*
  Q Light Controller Plus
  showcontrolsyncpage.h

  Copyright (C) Branson Matheson

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

#ifndef SHOWCONTROLSYNCPAGE_H
#define SHOWCONTROLSYNCPAGE_H

#include <QElapsedTimer>
#include <QPointer>
#include <QWidget>
#include "qlcui_export.h"

class ShowControlListener;
class QPushButton;
class QComboBox;
class QCheckBox;
class QGroupBox;
class QSpinBox;
class QLabel;
class Doc;

/** @addtogroup ui_io
 * @{
 */

/**
 * Connections' "Show control & sync" page: every link that controls or
 * clocks the show from outside, in one place -- OSC in, MIDI Show Control
 * in, MIDI timecode, and (later) QLab out.
 *
 * It edits the same settings the Settings dialog used to (QSettings
 * "showcontrol/...", through ShowControlListener) and the same timecode
 * source the footer's MTC chip picks, so either way the change is live.
 * Status follows the listener's and the timecode source's signals.
 */
class QLC_UI_EXPORT ShowControlSyncPage final : public QWidget
{
    Q_OBJECT

public:
    explicit ShowControlSyncPage(Doc *doc, QWidget *parent = nullptr);

    /** The listener that owns OSC and the MSC settings (the App's). Until
     *  there is one, OSC and MSC can't be edited. */
    void setListener(ShowControlListener *listener);

signals:
    /** "Patch a MIDI input…": take the user to where inputs are patched. */
    void patchMidiInputRequested();

protected:
    void showEvent(QShowEvent *ev) override;

private slots:
    void slotListenerSettingsChanged();
    void slotOscStatusChanged();
    void slotLastOscChanged(const QString &what);
    void slotLastMscChanged(const QString &what);
    void slotMtcSourceActivated(int index);
    void slotMscInputActivated(int index);
    void slotTimeChanged();
    void refreshMidiInputs();
    void refreshTimecode();

private:
    QGroupBox *buildOscGroup();
    QGroupBox *buildMscGroup();
    QGroupBox *buildMtcGroup();
    QGroupBox *buildQLabOutGroup();

private:
    Doc *m_doc;
    QPointer<ShowControlListener> m_listener;
    bool m_updating;

    // OSC in
    QCheckBox *m_oscCheck;
    QSpinBox *m_oscPortSpin;
    QLabel *m_oscStatus;
    QLabel *m_oscLast;

    // MSC in
    QCheckBox *m_mscCheck;
    QSpinBox *m_mscDeviceSpin;
    QComboBox *m_mscInputCombo;
    QLabel *m_mscInputs;
    QPushButton *m_patchMidiButton;
    QLabel *m_mscLast;

    // MIDI timecode
    QComboBox *m_mtcSourceCombo;
    QLabel *m_mtcStatus;
    QLabel *m_mtcHealth;
    QElapsedTimer m_mtcThrottle;
};

/** @} */

#endif

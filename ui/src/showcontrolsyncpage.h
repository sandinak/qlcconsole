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
#include <QHash>
#include <QPointer>
#include <QStringList>
#include <QWidget>
#include "qlcui_export.h"

class ShowControlListener;
class QPushButton;
class QComboBox;
class QCheckBox;
class QGroupBox;
class QSpinBox;
class QLabel;
class QLineEdit;
class Doc;
class ControlSurfacesPanel;

/** @addtogroup ui_io
 * @{
 */

/**
 * Connections' "Show control & sync" page: every link that controls or
 * clocks the show from outside, in one place -- OSC in, MIDI Show Control
 * in, MIDI timecode, and web access (the show's own setting: whether
 * opening it starts the web server, and on which port). Shown beside Connections ▸ Devices when one of
 * the show-control rows there is picked (no tab of its own).
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

    /** Show only one link's settings: "surfaces", "osc", "msc", "mtc", "map" or "web"; empty for all
     *  of them ("surfaces": the Control surfaces hub). Connections ▸ Devices calls it for the row picked. */
    void showSection(const QString &key);

    /** One line per link for the Devices rows: {key, name, state}, e.g.
     *  {"osc", "OSC in", "Port 8000 · listening"}; "web" for web access. */
    QList<QStringList> summary() const;

    /** The Control surfaces hub (every place a surface is set up). */
    ControlSurfacesPanel *surfaces() const { return m_surfaces; }
    /** Where a hub row's Set up… goes. */
    void setUpSurface(const QString &key);

signals:
    /** "Patch a MIDI input…": take the user to where inputs are patched. */
    void patchMidiInputRequested();
    /** A board has no single output to pair as feedback: Connections, on @p universe's input. */
    void patchFeedbackRequested(quint32 universe);
    /** What summary() returns has changed. */
    void summaryChanged();
    /** A Control surfaces Set up… wants the row @p key ("map", "web") picked
     *  in Connections ▸ Devices (the page also shows it itself). */
    void sectionRequested(const QString &key);
    /** The Control surfaces hub asks for the Virtual Console tab. */
    void virtualConsoleRequested();

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
    /** refreshMidiInputs() once, when control returns to the event loop. */
    void scheduleMidiInputsRefresh();
    void refreshTimecode();

private:
    QGroupBox *buildOscGroup();
    QGroupBox *buildMscGroup();
    QGroupBox *buildMtcGroup();
    QGroupBox *buildWebGroup();
    QGroupBox *buildMapGroup();
    QGroupBox *buildSurfacesGroup();
    QGroupBox *buildOutGroup();

    /** Show control out: the boxes from the show's settings / the settings
     *  from the boxes (saved with the show). */
    void refreshOut();
    void applyOut();

    /** The web access box from the show's setting and the server's state. */
    void refreshWeb();

    /** Emit summaryChanged() if the summary text is different. */
    void checkSummary();

private:
    Doc *m_doc;
    QPointer<ShowControlListener> m_listener;
    bool m_updating;
    bool m_midiRefreshPending = false;

    // OSC in
    QCheckBox *m_oscCheck;
    QSpinBox *m_oscPortSpin;
    QLabel *m_oscStatus;
    QLabel *m_oscLast;

    // MSC in
    QCheckBox *m_mscCheck;
    QLabel *m_mscRefused = nullptr;
    QLabel *m_mtcRefused = nullptr;
    QSpinBox *m_mscDeviceSpin;
    QComboBox *m_mscInputCombo;
    QLabel *m_mscInputs;
    QPushButton *m_patchMidiButton;
    QLabel *m_mscLast;

    // Show control out (QLab out): the show's own, ShowControl::outSettings()
    QCheckBox *m_outOscCheck = nullptr;
    QLineEdit *m_outHost = nullptr;
    QSpinBox *m_outPort = nullptr;
    QComboBox *m_outPath = nullptr;
    QComboBox *m_outLine = nullptr;
    QCheckBox *m_outMscCheck = nullptr;
    QComboBox *m_outMidi = nullptr;
    QSpinBox *m_outDevice = nullptr;
    QComboBox *m_outCues = nullptr;
    QLabel *m_outLast = nullptr;

    // Web access (the show's own setting, Doc::webAccess())
    QCheckBox *m_webCheck;
    QSpinBox *m_webPortSpin;
    QCheckBox *m_webNetworkCheck;
    QCheckBox *m_webLoginCheck;
    QLabel *m_webWhere;
    QLabel *m_webQr;
    QLabel *m_webStatus;

    // The Control Map's bindings
    class ControlMapPanel *m_controlMap = nullptr;
    ControlSurfacesPanel *m_surfaces = nullptr;

    // MIDI timecode
    QComboBox *m_mtcSourceCombo;
    QLabel *m_mtcStatus;
    QLabel *m_mtcHealth;
    QElapsedTimer m_mtcThrottle;

    /** The settings box for each link, by summary() key. */
    QHash<QString, QGroupBox *> m_groups;
    QList<QStringList> m_lastSummary;
};

/** @} */

#endif

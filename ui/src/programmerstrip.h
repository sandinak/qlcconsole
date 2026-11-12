/*
  Q Light Controller Plus
  programmerstrip.h

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

#ifndef PROGRAMMERSTRIP_H
#define PROGRAMMERSTRIP_H

#include <QToolBar>
#include <functional>
#include "qlcui_export.h"

class QPushButton;
class QToolButton;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class QWidget;
class Workbench;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The programmer's tools (board Q228): Highlight, Flash, Park / Unpark
 * All, Move in Black (Mark / Unmark All, Auto, lead time), the internal
 * Beat with Tap and the Speed Masters, Save Positions, and under More,
 * Snapshot live output into a scene and the jog speed. The followspot's
 * axis bindings ride along, hidden (they are set in I/O ▸ Mapping ▸
 * Joystick).
 *
 * One implementation, in Build and in the Show Tree alike: each tab has a
 * strip of its own, and every strip reads and writes the same state -- the
 * programmer selection (Doc::programmerSelection(), what the 2D plot, the
 * Workbench and the Library pick), the ProgrammerController's park, mark,
 * highlight and move-in-black, the InputOutputMap's beat -- so the two
 * always agree, whichever tab is up.
 */
class QLC_UI_EXPORT ProgrammerStrip final : public QToolBar
{
    Q_OBJECT

public:
    struct Host
    {
        /** The Workbench this tab shows (null: none here) -- Snapshot bakes
         *  into the scene it has open. */
        std::function<Workbench *()> workbench;
        /** What Snapshot says when there is no scene open here. */
        QString noScene;
    };

    ProgrammerStrip(Doc *doc, const Host &host, QWidget *parent = nullptr);
    ~ProgrammerStrip();

    /** The fixtures Flash, Park and Mark act on: the programmer selection. */
    QList<quint32> selection() const;

protected:
    void showEvent(QShowEvent *ev) override;

signals:
    /** Save Positions confirmed (the second click): save the show. */
    void requestSave();

public slots:
    /** Bake the live output into the scene the Workbench has open. */
    void slotSnapshotLive();

private slots:
    void slotHighlightToggled(bool on);
    void slotFlashSelection();
    void slotParkSelection();
    void slotUnparkAll();
    void slotParkChanged();
    void slotMarkSelection();
    void slotUnmarkAll();
    void slotMarkChanged();
    /** The programmer selection changed: Flash, Park and Mark follow. */
    void slotSelectionTools();

    void slotBpmToggled(bool on);
    void slotBpmChanged(int bpm);
    void slotTapBeat();
    /** The beat changed elsewhere (another strip, the footer, a show
     *  loaded): Beat, BPM and Tap say so. */
    void slotBeatFromEngine();

    void slotSavePositions();
    void slotDesignPositionWritten();

    void slotFollowSpotBindX();
    void slotFollowSpotBindY();
    void slotFollowSpotClearBindings();
    void slotFollowSpotBindingChanged();
    void slotJoystickUpdated(float pan, float tilt);

private:
    void updateParkButtonLabel();
    void updateMarkButtonLabel();
    void updateMarkMenuText();
    /** The Beat drop-down says the tempo ("Beat 120", "Beat off"). */
    void updateBeatText();

    Doc *m_doc;
    Host m_host;

    QPushButton *m_highlightBtn = nullptr;
    QPushButton *m_flashBtn = nullptr;
    QPushButton *m_parkBtn = nullptr;
    QPushButton *m_unparkBtn = nullptr;
    QPushButton *m_markBtn = nullptr;
    QPushButton *m_unmarkBtn = nullptr;
    QPushButton *m_autoMibBtn = nullptr;
    QDoubleSpinBox *m_mibGapSpin = nullptr;
    QPushButton *m_snapshotBtn = nullptr;
    QToolButton *m_markMenu = nullptr;
    QToolButton *m_beatMenu = nullptr;

    // BPM / internal beat generator
    QPushButton *m_bpmBtn = nullptr;
    QSpinBox *m_bpmSpin = nullptr;
    QPushButton *m_tapBtn = nullptr;
    bool m_settingBeat = false;     //!< this strip is setting the beat: no echo

    // Followspot bindings (kept for their wiring; never shown here)
    QWidget *m_followSpotPanel = nullptr;
    QPushButton *m_fsBindXBtn = nullptr;
    QPushButton *m_fsBindYBtn = nullptr;
    QPushButton *m_fsClearBtn = nullptr;
    QLabel *m_fsXLabel = nullptr;
    QLabel *m_fsYLabel = nullptr;
    QLabel *m_fsJoyLabel = nullptr;

    // Joystick speed
    QSpinBox *m_speedSpin = nullptr;

    // Save Positions (armed two-press confirmation)
    QPushButton *m_saveBtn = nullptr;
    bool m_saveArmed = false;
};

/** @} */

#endif // PROGRAMMERSTRIP_H

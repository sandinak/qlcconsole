/*
  Q Light Controller Plus
  focusmodepanel.h

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

#ifndef FOCUSMODEPANEL_H
#define FOCUSMODEPANEL_H

#include <QPointer>
#include <QDialog>
#include <QWidget>

#include "controllerscheme.h"

#include "focussession.h"

class FocusControlMap;
class QDoubleSpinBox;
class QPushButton;
class QToolButton;
class QComboBox;
class QCheckBox;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * What Focus mode starts on: which heads, in what order, aimed at what, and
 * what the others do. FOCUS_MODE_DESIGN.md.
 */
class FocusModeSetupDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @p sceneId: the selected scene, offered as the reference when it has
     *  an Aim look. @p selection: the heads selected now (offered first). */
    FocusModeSetupDialog(Doc *doc, quint32 sceneId, const QList<quint32> &selection,
                         QWidget *parent = nullptr);

    FocusSession::Options options() const;
    /** The device chosen to drive the nudges. */
    FocusSession::InputDevice inputDevice() const;

private:
    void fillReferences(QComboBox *combo, bool optional);
    static FocusSession::Reference referenceOf(const QComboBox *combo);

private:
    Doc *m_doc;
    quint32 m_sceneId;
    QList<quint32> m_selection;
    QComboBox *m_whichCombo;
    QComboBox *m_orderCombo;
    QComboBox *m_firstCombo;
    QComboBox *m_secondCombo;
    QComboBox *m_othersCombo;
    QDoubleSpinBox *m_levelSpin;
    QComboBox *m_inputCombo;
};

/**
 * Focus mode's (and Refine's) save status, as a lamp big enough to read
 * from the stage, colour-blind safe (gap 10): blue with a tick, all saved;
 * yellow with "!", the light being focused is unsaved; orange, some OTHER
 * light is. With both, an orange ring round a yellow centre. The number of
 * unsaved lights is on it too. flash() shows blue for a moment (a save).
 */
class FocusStatusLamp final : public QWidget
{
    Q_OBJECT

public:
    enum State { AllSaved, ThisUnsaved, OthersUnsaved, BothUnsaved };

    explicit FocusStatusLamp(QWidget *parent = nullptr);

    State state() const { return m_state; }
    void setState(State state);
    /** How many lights (Refine: changes) are not saved, shown on it. */
    void setCount(int count);
    int count() const { return m_count; }
    /** The shape in its middle: a tick, or "!". */
    QString symbol() const;
    /** Saved's colour for kFlashMs, whatever the state. */
    void flash();
    bool isFlashing() const { return m_flashing; }
    static constexpr int kFlashMs = 600;

    /** The colours it lights in: a state's centre and ring. */
    static QColor savedColor() { return ControllerScheme::savedColour(); }
    static QColor thisColor() { return ControllerScheme::thisUnsavedColour(); }
    static QColor othersColor() { return ControllerScheme::othersUnsavedColour(); }
    /** What it shows now: the ring, and the centre. */
    QColor ringColor() const;
    QColor centreColor() const;

    QSize sizeHint() const override { return QSize(64, 64); }
    QSize minimumSizeHint() const override { return QSize(48, 48); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    State m_state = AllSaved;
    int m_count = 0;
    bool m_flashing = false;
    int m_flashSerial = 0;
};

/**
 * The Focus mode HUD: which head, where it is aimed, its offsets live and
 * saved, the step, progress, the controls -- and the keyboard way to do all
 * of it, so it works without a controller. One at a time; open() raises it.
 */
class FocusModePanel final : public QWidget
{
    Q_OBJECT

public:
    /** Start Focus mode (asking how, through the setup dialog) or raise the
     *  panel if it is running. Returns the panel, or null if it was cancelled
     *  or refused (the reason is shown). */
    static FocusModePanel *open(Doc *doc, QWidget *parent, quint32 sceneId,
                                const QList<quint32> &selection);
    /** Start Focus mode on @p options without asking (open() after its
     *  setup dialog; a test). Null, with @p why, if the session refused. */
    static FocusModePanel *startWith(Doc *doc, QWidget *parent,
                                     const FocusSession::Options &options, QString *why = nullptr,
                                     int input = -1);
    /** The device Focus mode starts on (@p input of startWith() when it is
     *  -1): the controller if one Focus mode can use is connected and the
     *  keyboard was not the last choice on this machine, else the
     *  keyboard. */
    static FocusSession::InputDevice defaultInput(Doc *doc);
    /** Remember @p device as this machine's choice. */
    static void rememberInput(FocusSession::InputDevice device);
    /** The running panel, if any. */
    static FocusModePanel *instance();
    /** The panel for a session something else started (a focus-call cue):
     *  shown, or raised if it is up. Null when no session runs. */
    static FocusModePanel *attach(Doc *doc, QWidget *parent);

    ~FocusModePanel() override;

    /** Leave Focus mode, asking first about unsaved offsets. False if the
     *  answer was Cancel. */
    bool requestExit();

    /** Drive the nudges from @p device (the map follows). @p remember: it
     *  was the user's choice, kept for next time. */
    void setInputDevice(FocusSession::InputDevice device, bool remember = true);
    FocusControlMap *controlMap() const { return m_map; }
    /** "Since the save: ..." as shown; empty when nothing changed. */
    QString deltaLine() const;
    FocusStatusLamp *statusLamp() const { return m_lamp; }
    /** "All saved", "Unsaved: this light", "Unsaved: 2 other lights
     *  (Spot 3, Spot 7)"... as the panel shows it. */
    QString saveStatusText() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    FocusModePanel(Doc *doc, QWidget *parent);
    void refresh();
    /** The lamp and its words, and the plot's unsaved marks. */
    void refreshSaveStatus();
    /** The change since the save, here and as a ghost on the plot. */
    void refreshDelta();
    void showHighlight(quint32 fid);
    void finish();
    /** The controller came, went or changed: the map, the choice, and the
     *  fall back to the keyboard. */
    void refreshController();
    void styleUseButtons();
    /** The keep-alive's pause, said -- unless the note already says it. */
    void refreshPaused();
    /** What a key does here; false for a key Focus mode does not use. */
    bool handleKey(int key, Qt::KeyboardModifiers modifiers);

private:
    Doc *m_doc;
    FocusSession *m_session;
    QByteArray m_rigBefore;
    bool m_finishing = false;

    QLabel *m_titleLabel;
    /** The focus call being run, and its note (Focus calls only). */
    QLabel *m_callLabel = nullptr;
    /** Group or fixture (Y), and where saves go (the venue, or the show). */
    QLabel *m_scopeLabel = nullptr;
    QLabel *m_nameLabel;
    QLabel *m_referenceLabel;
    QLabel *m_offsetLabel;
    QLabel *m_beamLabel = nullptr;
    /** "Since the save: +2.5° pan, −1.0° tilt" (Q126). */
    QLabel *m_deltaLabel = nullptr;   ///< the beam, where it can change (Q103)
    QLabel *m_savedLabel;
    FocusStatusLamp *m_lamp;
    QLabel *m_statusLabel;
    QLabel *m_stepLabel;
    QLabel *m_progressLabel;
    QToolButton *m_useController;
    QToolButton *m_useKeyboard;
    QLabel *m_deviceLabel;
    QLabel *m_noteLabel;
    /** "Controller paused: <reason>" (ControllerKeepAlive). */
    QLabel *m_pausedLabel;
    QPushButton *m_backButton;
    FocusControlMap *m_map;
    /** Switched to the keyboard because the controller went: offer it
     *  back when it returns. */
    bool m_fellBack = false;

    static QPointer<FocusModePanel> s_instance;
};

/** @} */

#endif

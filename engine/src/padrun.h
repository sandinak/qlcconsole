/*
  Q Light Controller Plus
  padrun.h

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

#ifndef PADRUN_H
#define PADRUN_H

#include <QObject>
#include <QColor>
#include <QList>
#include <QSet>

#include "qlcengine_export.h"
#include "controllerscheme.h"
#include "gamepadrole.h"
#include "padnames.h"
#include "showcontrol.h"

class ProgrammerController;
class QTimer;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The pad's Run role (STRATEGY.md section 4, 0.4): run the show from a game
 * controller. While the role is on and no on-stage tool (Focus mode, Venue
 * focus, Refine) has the pad, its buttons are the Run map:
 *
 *   hold LB      arm: nothing moves the show unless it is held
 *   LB + A       GO              LB + B   Back
 *   LB + Y       Sub GO          LB + X   Stop the list
 *   RT / LT      next / previous list (no arm: it only picks)
 *   RS           say the list and what is on standby
 *
 * The same roles on every pad (an Xbox pad's A is a DualSense's Cross), so a
 * controller slot that moves to another pad keeps the map. Every command
 * goes through ShowControl::run() with RoutePad (docs/design/RUN_COMMANDS.md):
 * nothing here calls a runner.
 *
 * decided: GO needs a modifier held (LB), not a double-press (can change).
 * decided: Back, Sub GO and Stop need it too: one rule, "nothing moves the
 * show unless LB is held" (can change).
 *
 * The API is meant to grow into walking a cue's layers from the pad (Q192):
 * actionFor() is the one table, and handle() the one entry.
 */
class QLC_ENGINE_EXPORT PadRun final : public QObject
{
    Q_OBJECT

public:
    PadRun(Doc *doc, ProgrammerController *pc, QObject *parent = nullptr);
    ~PadRun() override;

    /*********************************************************************
     * The map
     *********************************************************************/
public:
    /** What a control does in the Run role. */
    enum Action
    {
        NoAction = 0,
        Arm,                ///< LB held
        Go,                 ///< A
        Back,               ///< B
        SubGo,              ///< Y
        Stop,               ///< X
        NextList,           ///< RT
        PreviousList,       ///< LT
        WhereAmI            ///< RS
    };
    Q_ENUM(Action)

    /** The action of @p role (NoAction for the sticks and anything unused). */
    static Action actionFor(GamepadRole::Role role);
    /** The role that does @p action (None for NoAction). */
    static GamepadRole::Role roleFor(Action action);
    /** True for the actions that only work with the arm held. */
    static bool needsArm(Action action);
    /** What @p action is called on the map and in the help, with control
     *  tokens ("GO ({LB} held)"); expand with PadNames::expand(). */
    static QString label(Action action);
    /** Every action, in map order. */
    static QList<Action> actions();
    /** The whole map, one line a control, in @p family's names: "LB: Hold
     *  to arm...", "A: GO (LB held)" -- or "L1", "Cross" on a PlayStation
     *  pad. For tooltips, the controller test and the help. */
    static QStringList mapLines(PadNames::Family family);
    /** The command an action sends (Go for anything that is not one). */
    static ShowControl::Command commandFor(Action action);

    /** The light bar while armed, and its flash when a GO went. Colour-blind
     *  safe with each other: orange and blue. */
    static QColor armedColour() { return ControllerScheme::othersUnsavedColour(); }
    static QColor wentColour() { return ControllerScheme::savedColour(); }
    static constexpr int wentFlashMs() { return 400; }

    /*********************************************************************
     * The role
     *********************************************************************/
public:
    /** The Run role is on (this computer's choice, kept by the UI). Off: the
     *  pad's buttons are the tools' and the profile's, as before. */
    bool isEnabled() const { return m_enabled; }
    /** @p say: tell the operator (not when the App restores it). */
    void setEnabled(bool on, bool say = true);

    /** A pad control, by gamepad role, @p value as it came (0..255).
     *  True when the Run role took it: in the role, every button and
     *  trigger is the Run map's, used or not, so nothing else fires. */
    bool handle(quint32 universe, GamepadRole::Role role, uchar value);

    /** The arm is held (on any pad). */
    bool isArmed() const { return !m_armedOn.isEmpty(); }
    /** Let go of everything held (the pad went away, a tool took it). */
    void reset();

    /*********************************************************************
     * Commands
     *********************************************************************/
public:
    /** Carry out @p action as its press would, the arm already checked:
     *  the commands through ShowControl::run(r, RoutePad). True when it
     *  did something. */
    bool trigger(Action action);

    /** The cue lists the pad picks between: the current production's lists
     *  (else every production's) that play -- not a Looks section or a
     *  cue's sub-cues -- by their chaser, in order. */
    QList<quint32> lists() const;
    /** The list the commands go to, by its chaser; UINT_MAX: the main list
     *  (as the footer GO). A list that has gone counts as the main one. */
    quint32 listChaser() const;
    /** Pick the next (@p delta 1) or previous (-1) list, wrapping. */
    void stepList(int delta);
    /** The list's name as the operator hears it. */
    QString listName(quint32 chaser) const;
    /** "Main. Next: cue 4." -- what RS says. */
    QString wherePhrase() const;

signals:
    void enabledChanged(bool on);
    void armedChanged(bool armed);
    void listChanged(quint32 chaser);
    /** A command went to run() (@p done: it did something). */
    void commandSent(ShowControl::Command command, bool done);
    /** A press that needs the arm came without it. */
    void refused(PadRun::Action action);

private:
    void showLight();

private:
    Doc *m_doc;
    ProgrammerController *m_pc;
    bool m_enabled = false;
    /** The universes whose arm (LB) is held: arming is per pad. */
    QSet<quint32> m_armedOn;
    /** The list picked (UINT_MAX: the main one). */
    quint32 m_list = UINT_MAX;
    ControllerScheme::TriggerButton m_rt, m_lt;
    /** The light bar's flash after a GO. */
    QTimer *m_flash = nullptr;
};

/** @} */

#endif

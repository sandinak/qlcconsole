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
 *   Menu         walk the show (below)
 *
 * Walking the show (Q192, TODO.md "Walk the show from the stage"): Menu
 * starts Focus this cue (RefineSession) on the cue in hand, with its own
 * map while walking -- the rest of the controller is Focus this cue's:
 *
 *   RT / LT      next / previous cue (Design: previewed, never a GO)
 *   RB / LB      next / previous layer (at Group or Fixture scope: the
 *                groups or fixtures, as in Focus this cue)
 *   View         solo the layer / the whole cue (Design)
 *   View + B     undo the last keep or tag
 *   tap Y        the scope: layer, group, fixture
 *   hold Y       tag the cue Needs work (again: clear it)
 *   Menu         stop walking (back to the Run map)
 *   sticks, D-pad, A, B, X, LS, RS: Focus this cue's
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
        WhereAmI,           ///< RS
        Walk                ///< Menu: walk the show
    };
    Q_ENUM(Action)

    /** What a control does while walking (the walking map). */
    enum WalkAction
    {
        WalkNoAction = 0,
        NextCue,            ///< RT
        PreviousCue,        ///< LT
        NextLayer,          ///< RB
        PreviousLayer,      ///< LB
        Solo,               ///< View
        TagCue,             ///< hold Y (a tap: the scope)
        LeaveWalk           ///< Menu
    };
    Q_ENUM(WalkAction)

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

    /** The walking map, by gamepad role (WalkNoAction: Focus this cue's). */
    static WalkAction walkActionFor(GamepadRole::Role role);
    static GamepadRole::Role roleFor(WalkAction action);
    static QString label(WalkAction action);
    static QList<WalkAction> walkActions();
    /** The walking map, one line a control, in @p family's names; the
     *  rest of the controller is Focus this cue's. */
    static QStringList walkMapLines(PadNames::Family family);
    /** How long Y is held to tag a cue, seconds. */
    static constexpr double tagHoldSeconds() { return 0.8; }

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

    /*********************************************************************
     * Walking the show (Q192)
     *********************************************************************/
public:
    /** Walking: Focus this cue on, started from the pad. */
    bool isWalking() const;
    /** Walk from cue @p cueId (invalid: the cue in hand -- in Design the
     *  cue picked in the Show Tree, else the picked list's standby cue,
     *  else its first; in Operate the cue that list is running). False,
     *  said and @p why, when Focus this cue refuses it. */
    bool startWalk(quint32 cueId = 0xFFFFFFFF, QString *why = nullptr);
    /** Stop walking: Focus this cue ends, the solo ends, the Run map is
     *  back. Refused, and said, while a change is not kept. */
    void stopWalk();
    /** The cue walked (invalid: not walking). */
    quint32 walkCue() const;
    /** The cue in hand for startWalk() (invalid: none). */
    quint32 cueInHand() const;
    /** To the next (@p delta 1) or previous (-1) cue of the cue's list, as
     *  Focus this cue on it. Design only: in Operate the cue moves with GO
     *  (quiet Refine, Q130). Refused, and said, with changes not kept. */
    bool stepCue(int delta);
    /** To the next / previous layer of the cue (wrapping). */
    void stepLayer(int delta);
    /** Solo: only the walked layer of the cue plays (Design: the show
     *  is not running; in Operate refused, and said). */
    bool isSolo() const { return m_solo; }
    bool setSolo(bool on);
    /** Mark the walked cue Needs work, or clear the mark: one Show Tree
     *  step (View + B or the laptop's undo takes it back). */
    bool toggleNeedsWork();
    /** A pad control while walking, from ProgrammerController's routing
     *  (Focus this cue owns the rest): true when the walking map took it. */
    bool handleWalk(GamepadRole::Role role, uchar value);
    /** The controller tick while walking: the hold of Y. */
    void walkTick(double dt);

signals:
    void enabledChanged(bool on);
    void armedChanged(bool armed);
    void listChanged(quint32 chaser);
    /** A command went to run() (@p done: it did something). */
    void commandSent(ShowControl::Command command, bool done);
    /** A press that needs the arm came without it. */
    void refused(PadRun::Action action);
    void walkingChanged(bool on);
    void soloChanged(bool on);

private:
    void showLight();
    /** The preview's solo follows the walked layer. */
    void syncSolo();
    /** Start Focus this cue on @p cueId for the walk, saying nothing more
     *  than it does; false (and @p why) when it refuses. */
    bool tryStart(quint32 cueId, QString *why = nullptr);
    void walkStarted();
    void walkEnded();
    bool refuseWalk(const QString &reason, QString *why = nullptr);
    QString cueLabel(quint32 cueId) const;

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
    /** Walking: Focus this cue was started here and runs. */
    bool m_walking = false;
    /** A cue step restarts Focus this cue: its stop is not the walk's. */
    bool m_restarting = false;
    bool m_solo = false;
    ControllerScheme::TriggerButton m_walkRt, m_walkLt;
    ControllerScheme::LongPress m_holdY { tagHoldSeconds() };
};

/** @} */

#endif

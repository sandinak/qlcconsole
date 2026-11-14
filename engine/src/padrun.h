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

#include <climits>
#include <QObject>
#include <QColor>
#include <QList>
#include <QSet>

#include "qlcengine_export.h"
#include "controllerscheme.h"
#include "gamepadrole.h"
#include "padnames.h"
#include "showcontrol.h"
#include "touchswipe.h"
#include "padtriggers.h"

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
 *   LB + Y       Sub GO
 *   LB + hold X 1 s   Stop the list (a tap of X only says to hold it)
 *   RT / LT      nothing (swallowed): the pad never picks the list
 *   RS           say the list and what is on standby
 *   LB + RB      fire the hit that is armed now (the Hits page, below)
 *   Menu         walk the show (below)
 *   hold View + Menu 1 s   switch the role (Tools / Run), either way
 *
 * Hits (Q154, show-tree.md "Hits"): every hit of the show whose place is
 * live now is armed; the narrowest place wins (cue, then list, production,
 * everywhere), as on a Virtual Console button holding them all
 * (ShowTreeController::armedTier()). LB + RB starts it (a one-shot effect
 * plays again if it has finished; while it runs, the press lets it go) and
 * says its name. Several at the narrowest place: each press takes the
 * next. Needs the arm, as GO does; Operate only, as a VC button.
 *
 * Walking the show (Q192, TODO.md "Walk the show from the stage"): Menu
 * starts Focus this cue (RefineSession) on the cue in hand, with its own
 * map while walking -- the rest of the controller is Focus this cue's:
 *
 *   RT / LT      next / previous cue (Design: previewed, never a GO)
 *   RB / LB      next / previous layer (at Group or Fixture scope: the
 *                groups or fixtures, as in Focus this cue); LB on its
 *                release, as it is also the modifier:
 *   hold LB + RT / LT   level up / down (this cue, A keeps)
 *   hold LB + sticks    the light itself on its target (Fixture scope, a
 *                       light the cue aims): its calibration, used by every
 *                       cue aimed with it; hold LB + A keeps it
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
 * The list is the Run tab's (its "plays" picker, saved with the show:
 * ControlMap::runTabList()), else the main cue list -- never picked from
 * the pad: sight unseen, a wrong list is too easy (0.4.5).
 *
 * decided: GO needs a modifier held (LB), not a double-press (can change).
 * decided: Back, Sub GO and Stop need it too: one rule, "nothing moves the
 * show unless LB is held" (can change).
 * decided: Stop also needs X held for stopHoldMs(): it sits beside GO, and
 * a slip onto it stopped the list (0.4.5).
 * decided: nothing on the pad leaves the Run role but the View + Menu hold.
 *
 * Triggers and swipes (Q199): on a DualSense the arm firms R2 (a GO takes a
 * squeeze, a brush cannot fire) and walking clicks both triggers
 * (PadTriggers); on any pad with a touchpad a swipe steps -- left / right
 * the previous / next cue, up / down the previous / next layer (in Run, not
 * walking: right / left walk from the next / previous cue; up / down do
 * nothing). Never a GO: swipes preview and select only.
 *
 * decided: while walking, one finger on the touchpad swipes; the aim nudge
 * it used to make needs LB held (can change).
 * decided: a swipe in Run, not walking, left / right starts a walk (Design
 * only; in Operate the cue moves with GO) (can change).
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
        Stop,               ///< X, held stopHoldMs()
        WhereAmI,           ///< RS
        Walk,               ///< Menu: walk the show
        FireHit             ///< RB: fire the armed hit
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
    /** Level from the triggers with LB held: 0..255 a second at full pull
     *  (half the range a second). */
    static constexpr float levelRate() { return 127.5f; }

    /** How long X is held, with the arm, to stop the list, ms. */
    static constexpr int stopHoldMs() { return 1000; }
    /** The time the hold takes (tests shorten it). */
    void setStopHoldMs(int ms);
    /** X is held for a Stop that has not gone yet. */
    bool isStopPending() const;

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

    /** The tool that has the pad now, so the Run map hears nothing ("The
     *  tools win"): "Quick focus", "Venue focus", "Focus this cue" or
     *  "Focus this look"; empty when none does, or when it is the pad's own
     *  walk. Shown by the Run tab's button and the footer. */
    QString holder() const;
    /** What the pad's last press could not do, for the screen whatever the
     *  feedback (lights only says nothing aloud): "Hold LB to arm", "GO:
     *  nothing to play"; empty after noticeMs(). */
    QString notice() const { return m_notice; }
    static constexpr int noticeMs() { return 3000; }
    /** The button's and the footer's words: "Pad: Tools", "Pad: Run",
     *  "Pad: ARMED", "Pad: Walk", or the tool holding it ("Pad: Venue
     *  focus"). */
    QString roleText() const;
    /** The tooltip for the Run tab's Pad: button and the footer's Pad chip,
     *  one text for both: the role line, the tool holding the pad (and how
     *  it ends), then the button map as a table in the connected pad's
     *  names (Cross / L1 or A / LB), and the walking map. Rich text. */
    QString helpHtml() const;
    /** Let go of everything held (the pad went away, a tool took it). */
    void reset();

    /*********************************************************************
     * Triggers and swipes (Q199)
     *********************************************************************/
public:
    /** The feel the pad's triggers should have now. */
    PadTriggers::State triggerState() const;
    /** Tell the pad (ProgrammerController::setPadTriggers()); @p force:
     *  even when it is what was last sent (a pad that came back). */
    void updateTriggers(bool force = false);

    /** A touchpad reading / click, kept for the tick. */
    void touchValue(GamepadRole::Role role, float norm);
    void touchClick(bool down);
    /** The controller tick: a swipe made since the last, acted on. */
    void swipeTick(double dt);
    /** Act on @p swipe: the walking steps, or Run's picks. True when it
     *  did something. Never a GO. */
    bool swipe(TouchSwipe::Swipe swipe);

    /*********************************************************************
     * Commands
     *********************************************************************/
public:
    /** Carry out @p action as its press would, the arm already checked:
     *  the commands through ShowControl::run(r, RoutePad). True when it
     *  did something. */
    bool trigger(Action action);

    /** Every hit of the show (a look of a Hits section), in tree order. */
    QList<quint32> allHits() const;
    /** The hits that may fire now at the narrowest armed place (more than
     *  one: they share a place and the presses step through them). */
    QList<quint32> armedHits() const;
    /** Fire the armed hit: start it, or let it go when it runs (a finished
     *  one-shot plays again). Says its name; false, said, when none is
     *  armed or the show is in Design. */
    bool fireHit();

    /** The list the commands go to, by its chaser: the one the Run tab
     *  plays (ControlMap::runTabList(), saved with the show); UINT_MAX: the
     *  main list (as the footer GO), also for a list that has gone. */
    quint32 listChaser() const;
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
    /** The controller tick while walking: the hold of Y, the level. */
    void walkTick(double dt);
    /** LB is held as the walk's modifier (level, the light itself). */
    bool isModifierHeld() const { return m_lbHeld; }
    /** Both bumpers made a chord (the next change not kept): LB was not a
     *  tap, so its release steps nothing. */
    void bumperChord() { m_lbUsed = true; }

    /*********************************************************************
     * The role chord: hold View + Menu to switch Tools / Run
     *********************************************************************/
public:
    /** How long View + Menu are held to switch the role, ms. */
    static constexpr int defaultRoleChordMs() { return 1000; }
    /** Both chord buttons are down: start timing the hold. */
    void roleChordBegin();
    /** Any other button went down: it was no chord. */
    void roleChordCancel();
    /** One of the two went up. True when it was a short press (the chord
     *  neither fired nor was cancelled) and so Tools' View + Menu, Focus
     *  mode, is the caller's to do. */
    bool roleChordEnd();
    bool isRoleChordPending() const { return m_chordPending; }
    /** The chord switched the role and its buttons are not yet up. */
    bool roleChordFinished() const { return m_chordFired; }
    /** The time the hold takes (tests shorten it). */
    void setRoleChordMs(int ms);

signals:
    void enabledChanged(bool on);
    void armedChanged(bool armed);
    /** A command went to run() (@p done: it did something). */
    void commandSent(ShowControl::Command command, bool done);
    /** A press that needs the arm came without it. */
    void refused(PadRun::Action action);
    /** A hit was started (@p on) or let go by the pad. */
    void hitFired(quint32 hitId, bool on);
    void walkingChanged(bool on);
    void soloChanged(bool on);
    /** A tool took the pad or let it go (holder()). */
    void holderChanged();
    /** notice() changed. */
    void noticeChanged();

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
    /** LB let go (or the walk changed): the modifier off, the level said. */
    void releaseModifier();
    QString cueLabel(quint32 cueId) const;
    void roleChordFired();
    /** Say @p text on the screen (notice()) for noticeMs(). */
    void showNotice(const QString &text);
    /** X held long enough with the arm: the Stop goes. */
    void stopHoldFired();

private:
    Doc *m_doc;
    ProgrammerController *m_pc;
    bool m_enabled = false;
    /** The universes whose arm (LB) is held: arming is per pad. */
    QSet<quint32> m_armedOn;
    /** X held for a Stop, and the pad it is held on. */
    QTimer *m_stopHold = nullptr;
    quint32 m_stopUniverse = UINT_MAX;
    /** The next hit a press takes among several at the narrowest place. */
    int m_hitStep = 0;
    QTimer *m_chord = nullptr;
    bool m_chordPending = false;    ///< both down, still timing
    bool m_chordFired = false;      ///< switched: the releases do nothing
    TouchSwipe m_swipe;
    /** The light bar's flash after a GO. */
    QTimer *m_flash = nullptr;
    QTimer *m_noticeTimer = nullptr;
    QString m_notice;
    /** Walking: Focus this cue was started here and runs. */
    bool m_walking = false;
    /** A cue step restarts Focus this cue: its stop is not the walk's. */
    bool m_restarting = false;
    bool m_solo = false;
    ControllerScheme::TriggerButton m_walkRt, m_walkLt;
    ControllerScheme::LongPress m_holdY { tagHoldSeconds() };
    /** LB held as the modifier, and whether anything was done with it (a
     *  tap of LB alone steps back a layer on its release). */
    bool m_lbHeld = false;
    bool m_lbUsed = false;
    int m_lightAsksAt = 0;
    /** The triggers' pull while LB is held, 0..1, and a level moved. */
    float m_levelUp = 0.0f;
    float m_levelDown = 0.0f;
    bool m_levelMoved = false;
};

/** @} */

#endif

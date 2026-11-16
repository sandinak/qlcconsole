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
#include <QHash>
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
class Chaser;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The pad's Run role (STRATEGY.md section 4, 0.4): run the show from a game
 * controller. While the role is on and no on-stage tool (Focus mode, Venue
 * focus, Refine) has the pad, its buttons are the Run map (reworked 0.4.5,
 * the rules below):
 *
 *   hold LB + RB (armChordMs())   ARM; the same again disarms. A latch:
 *                                 nothing has to stay held
 *   LT held              the safety: no cue moves without it
 *   armed, LT held + A   GO            armed, LT held + B   Back
 *   armed, LT held + X   Sub GO        armed, LT held + Y   Sub Back
 *   armed, RT on its own fire the hit that is armed now (not with LT or
 *                        a bumper down: that is the stop in the making;
 *                        one hit for now, several would be RT held + X / Y
 *                        / A / B)
 *   LB + RB + LT + RT    EMERGENCY STOP: everything stops (ALL OFF), in
 *                        the Run role whatever the pad is doing (armed or
 *                        not, walking, a tool holding it; not while the
 *                        Controller Test is open); the arm drops and every
 *                        surface shows red, over a walk or a tool too
 *   (the bumpers are the mode only: the arm, and the stop's four)
 *   RS           say the list and what is on standby
 *   Menu         walk the show (below); not while armed
 *   armed, LT held + Menu   start the venue focus cue that waits (FOCUS)
 *   hold View + Menu 1 s   switch the role (Tools / Run), either way
 *
 * The rules (the operator's, 2026-11-14; every later change keeps them):
 *   1. The pad never picks the list: the screen does (the Run tab).
 *   2. One state, one source: state(), stateColour() and roleText() are
 *      what the Run tab's border and button, the footer and the pad's own
 *      light all show; nothing derives its own.
 *   3. No single control arms or disarms. The arm changes on the LB + RB
 *      chord only -- or with no button at all: the role switched, a tool
 *      took the pad, the pad went away, the emergency stop. The chord is
 *      a fresh one: a trigger down with both bumpers (the stop, or one
 *      on its way) spends it until both bumpers are let go.
 *   4. No cue moves on one control: armed, and the safety (LT) held, and
 *      the button -- one safety for GO, Back, Sub GO and Sub Back. The
 *      hit is the right trigger's own, armed.
 *   5. The pad's light says the state where it has one: the arm's colour
 *      armed, blinking green while a cue fades, red after the emergency
 *      stop, blue in FOCUS, its own colour otherwise. A pad with no light
 *      buzzes: twice armed, once long disarmed, three long for the stop.
 *   6. Venue focus from the pad (2026-11-15): armed, with a venue focus
 *      cue waiting, LT held + Menu starts its calls and the pad is in
 *      FOCUS -- not armed, not disarmed. Menu alone does nothing to the
 *      focus (armed: the walk's refusal, which names LT + Menu); not
 *      armed, the start is refused, saying to arm first. In FOCUS the
 *      focus tool has the pad as it always has, no cue moves and no hit
 *      fires from the pad, and the four-together stop still stops. LT
 *      held + Menu again -- the one way out from the pad; RT past the
 *      last call only says so -- ends the focus (its unsaved-work prompt
 *      kept) and the pad is ARMED again. The list stays on the focus cue
 *      until the operator moves it (LT + A, or GO on the laptop): ending
 *      a venue focus, from anywhere, never moves the list. The pad going
 *      away in FOCUS: back not armed, never FOCUS with no pad.
 *
 * Hits (Q154, show-tree.md "Hits"): every hit of the show whose place is
 * live now is armed; the narrowest place wins (cue, then list, production,
 * everywhere), as on a Virtual Console button holding them all
 * (ShowTreeController::armedTier()). Armed, RT starts
 * it (a one-shot effect plays again if it has finished; while it runs, the
 * press lets it go) and says its name. Several at the narrowest place:
 * each press takes the next. Operate only, as a VC button. Before it
 * fires, every surface names it (hitText(), hitChanged()): the Run tab
 * and the footer while armed, the phone page while its pad is armed.
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
 * decided: the arm is a latch on a two-button chord, not a button held
 * (0.4.5; it was LB held): a held arm let go by one finger disarmed.
 * decided: there is no "stop the list" on the pad (it was LB + X held):
 * the screen stops a list, the pad has the emergency stop.
 * decided: nothing on the pad leaves the Run role but the View + Menu hold.
 *
 * Triggers and swipes (Q199): on a DualSense the arm firms both triggers (the
 * safety and the hit take a squeeze, a brush does neither) and walking clicks them
 * (PadTriggers); on any pad with a touchpad a swipe steps -- left / right
 * the previous / next cue, up / down the previous / next layer (in Run, not
 * walking and not armed: right / left walk from the next / previous cue;
 * up / down do nothing). Never a GO: swipes preview and select only.
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
        Arm,                ///< LB and RB, held armChordMs(): arm / disarm
        Safety,             ///< LT: held for the four cue commands
        Go,                 ///< A, armed, LT held
        Back,               ///< B, armed, LT held
        SubGo,              ///< X, armed, LT held
        SubBack,            ///< Y, armed, LT held
        FireHit,            ///< RT, armed
        WhereAmI,           ///< RS
        Walk,               ///< Menu: walk the show (not armed)
        EmergencyStop       ///< LB + RB + LT + RT: no one control's
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
    /** True for the actions that only work armed. */
    static bool needsArm(Action action);
    /** True for the cue commands, which also need the safety (LT) held. */
    static bool needsSafety(Action action);
    /** The safety: the one control held beside every command's own. */
    static GamepadRole::Role safetyRole() { return GamepadRole::LeftTrigger; }
    /** What @p action is called on the map and in the help, with control
     *  tokens ("GO ({LB} held)"); expand with PadNames::expand(). */
    static QString label(Action action);
    /** Every action that has a control of its own, in map order (not the
     *  emergency stop: four controls, emergencyStopLabel()). */
    static QList<Action> actions();
    /** "Emergency stop: {LB} + {RB} + {LT} + {RT}", with control tokens. */
    static QString emergencyStopLabel();
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

    /** How long LB + RB are held to arm, or to disarm, ms. */
    static constexpr int armChordMs() { return 500; }
    /** The time the chord takes (tests shorten it). */
    void setArmChordMs(int ms);
    /** LB + RB are held for an arm change that has not gone yet. */
    bool isArmChordPending() const;

    /** The colours of the state, on the screen and on the pad's light:
     *  the arm, the emergency stop, and the blink while a cue fades. The
     *  blink carries "playing" by its motion, not by green alone. */
    static QColor armedColour() { return ControllerScheme::othersUnsavedColour(); }
    static QColor stoppedColour() { return QColor(0xD3, 0x2F, 0x2F); }
    static QColor playingColour() { return QColor(0x2E, 0xB8, 0x4E); }
    /** FOCUS (rule 6): a blue, apart from the arm's orange, the stop's red
     *  and the GO's green (black ink reads on it). */
    static QColor focusColour() { return QColor(0x1E, 0x88, 0xE5); }
    /** A GO shows as playing at least this long, however short its fade. */
    static constexpr int wentFlashMs() { return 400; }
    /** Half the blink's period while a cue fades, ms. */
    static constexpr int blinkMs() { return 250; }

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

    /** Armed: latched by the LB + RB chord until the chord again (or the
     *  role, a tool, the pad going away, the emergency stop). */
    bool isArmed() const { return m_armed; }
    /** The pad that armed, whose presses count (UINT_MAX: not armed). */
    quint32 armedUniverse() const { return m_armed ? m_armUniverse : UINT_MAX; }
    /** Arm or disarm as the chord on @p universe would (the chord's own
     *  end, and for tests); refused while a tool has the pad or walking. */
    bool setArmed(bool on, quint32 universe = 0);

    /** The one state every surface shows (rule 2). */
    enum State
    {
        Tools = 0,      ///< the role is off
        Run,            ///< the role is on, not armed
        Armed,
        Stopped,        ///< the emergency stop went; until armed again or the
                        ///< show runs (shown over a walk or a tool too)
        Walking,
        ToolHasPad,     ///< a tool started on the laptop holds the pad
        Focus           ///< a venue focus cue's calls have the pad (rule 6):
                        ///< not armed, not disarmed; LT + Menu leaves
    };
    Q_ENUM(State)
    State state() const;
    /** The colour of state() for a border, a chip and the pad's light:
     *  armedColour(), stoppedColour(), or invalid (nothing to show). */
    QColor stateColour() const;
    /** Black or white, whichever reads on stateColour(). */
    QColor stateInk() const;
    /** A cue of the list is fading now (or a GO just went). */
    bool isPlaying() const { return m_playing; }
    /** What the pad's light shows now (invalid: its own colour). */
    QColor lightColour() const;

    /** The tool that has the pad now, so the Run map hears nothing ("The
     *  tools win"): "Quick focus", "Venue focus", "Focus this cue",
     *  "Focus this look" or "Controller Test"; empty when none does, or
     *  when it is the pad's own walk. Shown by the Run tab's button and
     *  the footer. */
    QString holder() const;
    /** The Controller Test is open: its presses are being tried, so they
     *  must never be a GO, a hit or a role change -- the pad's buttons and
     *  triggers do nothing here (ProgrammerController swallows them) and
     *  the arm drops, as for any tool. The sticks stay the follow spot's. */
    void setTesting(bool on);
    bool isTesting() const { return m_testing; }
    /** What the pad's last press could not do, for the screen whatever the
     *  feedback (lights only says nothing aloud): "Hold LB to arm", "GO:
     *  nothing to play"; empty after noticeMs(). */
    QString notice() const { return m_notice; }
    static constexpr int noticeMs() { return 3000; }
    /** state() in words, for the button and the footer: "Pad: Tools",
     *  "Pad: Run", "Pad: ARMED", "Pad: STOPPED", "Pad: Walk", "Pad: FOCUS",
     *  or the tool holding it ("Pad: Venue focus"). */
    QString roleText() const;
    /** The tooltip for the Run tab's Pad: button and the footer's Pad chip,
     *  one text for both: the role line, the tool holding the pad (and how
     *  it ends), then the button map as a table in the connected pad's
     *  names (Cross / L1 or A / LB), and the walking map. Rich text. */
    QString helpHtml() const;
    /** Every pad control, first of all (CONTROLLER_JOBS.md 4): the
     *  emergency stop's four, which are the stop in the Run role from any
     *  pad whatever it is doing -- a tool, the walk, a venue focus waiting
     *  or held by another pad, the Controller Test open. It is read before
     *  any owner, tool, job or role check, so nothing can swallow it. True
     *  when the four are all down, so nothing else acts on it (the stop
     *  fires once a chord). handle() takes what this read of the same
     *  event, so a press is not read twice. */
    bool watchStop(quint32 universe, GamepadRole::Role role, uchar value);
    /** Let go of everything held and disarm (the pad went away, a tool
     *  took it, the role changed): never a button's doing. */
    void reset();
    /** The controller went away: reset(), and the screen says why the arm
     *  dropped. */
    void controllerLost();
    /** The controller went (@p present false) or came back: in FOCUS with
     *  no pad the state is not FOCUS (rule 6), and the arm the focus would
     *  go back to is dropped, as controllerLost(). */
    void setPadPresent(bool present);
    /*********************************************************************
     * Controller jobs (docs/design/CONTROLLER_JOBS.md)
     *********************************************************************/
    /** The controller holding Run: the one armed, or the one whose arm a
     *  venue focus put by (UINT_MAX: none). One at a time: another pad's
     *  arm is refused with runningCuesText(). */
    quint32 runnerUniverse() const;
    /** Its short name ("DualSense Wireless Controller (…77:93)"); empty
     *  when none holds Run. */
    QString runnerName() const;
    /** "<name> is running cues." -- said aloud and kept as notice(). */
    QString runningCuesText() const;
    /** notice() as the footer's Pad chip shows it beside roleText(): when
     *  the chip already names the runner, a refusal of another pad does
     *  not name it again ("another controller is running cues"). */
    QString chipNotice() const;
    static QString noRunCuesText();
    static QString noFocusText();
    /** Game controllers patched now (roleText() names the runner when
     *  more than one is). */
    int padCount() const;
    /** The one pad whose light shows the state (the runner; in FOCUS the
     *  focus's pad); UINT_MAX: no one pad's (the tools' pad, as before).
     *  Every pad is red after the stop. */
    quint32 lightUniverse() const;
    /** Two universes are one controller: the same, or the same pad seen
     *  twice (one identity, not a name). */
    bool sameController(quint32 a, quint32 b) const;
    /** A controller's jobs changed: a job taken away while in use lets go. */
    void jobsChanged();

    /** The input on @p universe was repatched (its pad unplugged, another
     *  pad in its slot, back again): what it held is forgotten, and an arm
     *  made on it drops as controllerLost() -- its releases may never have
     *  come, so a held safety is no safety on the pad there now. */
    void forgetPad(quint32 universe);

    /*********************************************************************
     * Venue focus from the pad (rule 6)
     *********************************************************************/
public:
    /** The Run role is on and a venue focus cue's calls have begun (the
     *  venue being named, or the calls running): the pad's presses start
     *  or end nothing but by LT + Menu, and play no cue. With the pad gone
     *  this stays true; state() is then not Focus. */
    bool isFocusing() const;
    /** A venue focus cue is armed and waits for its start. */
    bool isFocusCueWaiting() const;
    /** Menu (Start) on @p universe while isFocusing(): with LT held, the
     *  focus ends (the laptop asks about unsaved work); alone it says so.
     *  False (nothing done) when not focusing. */
    bool focusMenu(quint32 universe);
    /** RT past the last call while focusing: the words that say how to
     *  finish (also shown); empty when not focusing (the calls end there,
     *  as in the Tools role). FocusSession's last-call hold. */
    QString lastCallHold();
    /** The venue focus cue changed state (ProgrammerController): FOCUS
     *  begins or ends; ended, the arm it came from is back. */
    void focusCueChanged();

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
    /** Carry out @p action as its press would, the arm and the safety
     *  already checked:
     *  the commands through ShowControl::run(r, RoutePad). True when it
     *  did something. */
    bool trigger(Action action);

    /** Every hit of the show (a look of a Hits section), in tree order. */
    QList<quint32> allHits() const;
    /** The hits that may fire now at the narrowest armed place (more than
     *  one: they share a place and the presses step through them). */
    QList<quint32> armedHits() const;
    /** Everything stops (ALL OFF through run()), the arm drops, the state
     *  is Stopped. */
    void emergencyStop();
    /** Fire the armed hit: start it, or let it go when it runs (a finished
     *  one-shot plays again). Says its name; false, said, when none is
     *  armed or the show is in Design. */
    bool fireHit();

    /** The hit the next pull of RT acts on, from either pad -- this one or
     *  the phone page's (WebAccessRun::hit()): they take one turn between
     *  them. Invalid when a pull fires nothing (no hit armed now, or the
     *  show in Design). @p letsGo: it is on, and the pull lets it go (a
     *  finished one-shot is not: it plays again). @p index (from 1) and
     *  @p count: its place among the hits taking turns. */
    quint32 nextHit(bool *letsGo = nullptr, int *index = nullptr, int *count = nullptr) const;
    /** nextHit() in words: "Hit ▸ Staff Slam", "Hit ▸ Staff Slam (1 of 2)"
     *  when two take turns, "Hit ▸ Staff Slam off" when the pull lets it
     *  go; empty when a pull fires nothing (nothing to say, UX-23). The
     *  phone page shows it while its own pad is armed. */
    QString hitText() const;
    /** hitText() while this pad is armed (state() Armed), else empty: what
     *  the Run tab and the footer's Pad chip show (rule 2: from here,
     *  never derived on a surface). */
    QString armedHitText() const;
    /** Of the hits taking turns now, the one a pull starts when none is
     *  on (invalid: none armed). */
    quint32 hitTurn() const;
    /** A pull started hitTurn(): the turn moves on to the next. */
    void nextHitTurn();

    /** The list the commands go to, by its chaser: the one the Run tab
     *  plays (ControlMap::runTabList(), saved with the show); UINT_MAX: the
     *  main list (as the footer GO), also for a list that has gone. */
    quint32 listChaser() const { return listChaserOf(m_doc); }
    /** The rule of listChaser() for @p doc: every pad's list, the console
     *  pad's and the phone page's (rule 1: the screen picks it). */
    static quint32 listChaserOf(const Doc *doc);
    /** That list's chaser itself (the main one for UINT_MAX); null: none.
     *  What the phone page's commands with no list named play and what it
     *  shows (WebAccessRun). */
    static Chaser *playedList(Doc *doc);
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
    /** state(), stateColour() or roleText() changed: the one signal the
     *  screens follow (rule 2). */
    void stateChanged();
    /** hitText() or armedHitText() changed: the cue moved on, a hit went
     *  on or off, the turn moved, the arm or the mode changed. Every
     *  surface naming the hit follows it (rule 2). */
    void hitChanged();
    void enabledChanged(bool on);
    void armedChanged(bool armed);
    /** A command went to run() (@p done: it did something). */
    void commandSent(ShowControl::Command command, bool done);
    /** A press that needs the arm or the safety came without it. */
    void refused(PadRun::Action action);
    /** A hit was started (@p on) or let go by the pad. */
    void hitFired(quint32 hitId, bool on);
    void walkingChanged(bool on);
    void soloChanged(bool on);
    /** A tool took the pad or let it go (holder()). */
    void holderChanged();
    /** notice() changed. */
    void noticeChanged();
    /** The Controller Test opened (@p on) or closed: what the pad held
     *  across it is not known (ProgrammerController's held buttons). */
    void testingChanged(bool on);

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
    /** LB + RB held long enough: the arm changes. */
    void armChordFired();
    /** Start or stop timing the chord from what @p universe holds now. */
    void armChordUpdate(quint32 universe);
    /** reset() without telling the screens (the caller does). */
    void clear();
    /** Tell a press what it lacks, aloud and on the screen. */
    void refuse(Action action, const QString &text);
    /** The state may have changed: the light, the triggers, stateChanged(). */
    void sync();
    /** The blink's tick while armed: is a cue fading? */
    void lightTick();
    /** A tool took the pad or let it go. */
    void toolChanged();
    /** Follow ShowControl's commands (once it is there): Stopped ends.
     *  Also follows the Show Tree's hits (made after the programmer). */
    void hookShowControl();
    /** hitText() may have changed: hitChanged() once if it did. */
    void syncHit();
    /** m_hitStep for @p tier (the hits armed now): 0 for a set syncHit()
     *  has not seen yet -- a new set starts at 1 of n. */
    int turnIn(const QList<quint32> &tier) const;
    /** Stops a one-shot hit whose run is over (Q239). */
    void letGoFinishedOneShots();
    /** The controller on @p universe has @p job (a ControllerJobs::Job). */
    bool hasJob(quint32 universe, int job) const;
    /** Armed, LT held + Menu with a venue focus cue waiting: start it. */
    bool startFocus(quint32 universe);
    /** Into FOCUS from ARMED, however the calls began: the arm is kept
     *  aside (m_focusArm) for the focus's end. */
    void putArmBy();
    /** Menu with a venue focus cue waiting, pressed on @p universe. */
    void focusStartPress(quint32 universe, bool safetyHeld);
    /** One of the four (LB, RB, LT, RT) read into m_held: the bumpers, the
     *  triggers' hysteresis, the spent chord. True on RT's press. */
    bool noteHeld(quint32 universe, GamepadRole::Role role, uchar value);
    /** The same pad on another universe (one identity) has fired the stop
     *  for the chord now held: one stop per physical pad. */
    bool twinStopFired(quint32 universe) const;

private:
    Doc *m_doc;
    ProgrammerController *m_pc;
    bool m_enabled = false;
    /** What a pad holds now, of the four controls the chords are made of. */
    struct Held
    {
        bool lb = false;
        bool rb = false;
        ControllerScheme::TriggerButton lt, rt;
        bool stopFired = false;     ///< the four made a stop: not again until one is up
        /** A trigger was down with both bumpers (the stop, or one on its
         *  way): no arm until both bumpers are up and squeezed afresh. */
        bool spent = false;
        bool anyTrigger() const { return lt.isDown() || rt.isDown(); }
        bool all() const { return lb && rb && lt.isDown() && rt.isDown(); }
    };
    QHash<quint32, Held> m_held;
    /** The event watchStop() last read (and RT's press in it), for handle()
     *  to take instead of reading it again. */
    struct Noted
    {
        bool valid = false;
        quint32 universe = UINT_MAX;
        int role = 0;
        uchar value = 0;
        bool rtPressed = false;
    } m_noted;
    bool m_armed = false;
    quint32 m_armUniverse = UINT_MAX;
    bool m_stopped = false;
    /** FOCUS was entered from ARMED on this pad (LT + Menu): the arm it
     *  goes back to when the focus ends (UINT_MAX: none). */
    quint32 m_focusArm = UINT_MAX;
    /** The controller is gone (ProgrammerController's presence check). */
    bool m_padGone = false;
    bool m_testing = false;     ///< the Controller Test has the pad
    /** holder() when last seen: a change forgets what was held. */
    QString m_lastHolder;
    bool m_stopping = false;    ///< the stop's own ALL OFF is going through run()
    bool m_heldNow = false;     ///< the double-GO guard held the command trigger() sent
    bool m_showControlHooked = false;
    bool m_showTreeHooked = false;
    /** What the screens were last told of the hit, so hitChanged() goes once. */
    QString m_shownHit;
    QString m_shownArmedHit;
    QTimer *m_armChord = nullptr;
    quint32 m_chordUniverse = UINT_MAX;
    /** What the screens were last told, so stateChanged() goes once. */
    int m_shownState = int(Tools);
    QString m_shownText;
    /** The blink: a cue fading (or the GO's flash), and its phase. */
    QTimer *m_blink = nullptr;
    bool m_playing = false;
    bool m_blinkOn = false;
    /** The next hit a press takes among several at the narrowest place. */
    int m_hitStep = 0;
    QList<quint32> m_armedSet;     // the armed hits m_hitStep counts through
    QTimer *m_oneShotTick = nullptr;
    QTimer *m_chord = nullptr;
    bool m_chordPending = false;    ///< both down, still timing
    bool m_chordFired = false;      ///< switched: the releases do nothing
    TouchSwipe m_swipe;
    /** A GO just went: playing, whatever its fade. */
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

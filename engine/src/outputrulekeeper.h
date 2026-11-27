/*
  Q Light Controller Plus
  outputrulekeeper.h

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

#ifndef OUTPUTRULEKEEPER_H
#define OUTPUTRULEKEEPER_H

#include <QObject>
#include <atomic>
#include <QByteArray>
#include <QList>
#include <QHash>
#include <QPair>
#include <QMutex>
#include <QVector>
#include <QSet>
#include <memory>

#include "fixture.h"
#include "doc.h"
#include "outputlayer.h"
#include "genericfader.h"
#include "qlcengine_export.h"

class QXmlStreamReader;
class QXmlStreamWriter;

#define KXMLQLCOutputRuleSettings QStringLiteral("OutputRuleSettings")

/** @addtogroup engine Engine
 * @{
 */

/**
 * Keeps each universe's output-rule bits (Universe::setOutputRules()) in
 * step with the fixtures' Fixture::OutputRules: power relays and house,
 * work, hall and backstage-blue circuits that Blackout and the Grand Master
 * must not kill, and relays that switch rather than fade. Rebuilt when
 * fixtures are added, removed, changed (patched, moved, unpatched, their
 * rules edited) and when universes come and go -- so a fixture's bits
 * leave its old address with it.
 *
 * Which channels each rule covers:
 *  - ExemptGrandMaster, ExemptBlackout: every channel of the fixture. In
 *    the usual Intensity GM mode only intensity channels are scaled anyway,
 *    so this is the same; in All-channels mode an exempt fixture is exempt,
 *    not half-exempt. Blackout already keeps non-intensity channels, so for
 *    it the difference is the intensity ones holding their live level.
 *  - Switch, OnAtOpen: the fixture's intensity channels (all of them when
 *    it has none -- a relay pack defined some other way). Snapping a pan or
 *    colour channel to 0/255, or driving it to full, would make no sense.
 *
 *  - ShowOnly: every channel. The rules below.
 *
 * Show only: the rules. One mechanism, the stamp, and the live session.
 * Every operator action takes a stamp (GenericFader::stampEpoch(): a
 * number that only grows); a live session starts at a stamp of its own
 * (GenericFader::liveEpoch()); each Show-only channel keeps the stamp it
 * was made Show only at.
 *
 *  S0. The frame. In Operate, every frame, each Show-only channel is built
 *      from scratch (Universe::processFaders()): it starts at its safe
 *      value, and only the writers that count NOW are applied, HTP and LTP
 *      among themselves. A writer counts on a channel while its stamp is
 *      of the current session and later than the channel's own (a fixture
 *      patched or moved onto it, the rule given). A writer that does not
 *      count contributes nothing -- nothing is undone to what was there.
 *      With none, the channel is at its safe value: a cue letting go cuts
 *      it, whatever its channel group (board Q246). A writer that does not
 *      count writes nothing there at all, not even for a moment, and sets
 *      no flag (past the Grand Master): every write entry point drops it.
 *      A safe value goes out as it is, through the channel's modifier,
 *      never scaled by the Grand Master -- held or live alike.
 *      Every send. Every byte that leaves for a Show-only channel comes
 *      from this one builder (Universe::buildShowOnly()): the frame's own
 *      send; a send between two frames (Blackout on or off, Blind on or
 *      off, the emergency stop, a maintenance send, a plugin's own
 *      dumpOutput()) -- taken under the frame lock, never a copy of the
 *      stored values: safe when held or under Blackout, else what the last
 *      frame built there. A fade starting on one starts from that build.
 *      Blind freezes the other channels at the frame from before it (as
 *      stock QLC+ does) but sends one frame on entering it with the
 *      Show-only channels at safe -- what the rig keeps through Blind; All
 *      Off, Blackout and the emergency stop always send, Blind or not.
 *  S1. Outside Operate, and in Blind, every frame sent has a Show-only
 *      channel at its safe value (safeValue(): dark, closed, through the
 *      channel's modifier). Blackout always wins, Ignore Blackout or not.
 *  S2. A live session starts at each switch to Operate, at All Off, when a
 *      show is loaded or cleared, and when Blind ends in Operate (a GO in
 *      Blind is checked on screen: GO again after Blind to fire it).
 *      Input passthrough never reaches a Show-only channel.
 *  S3. Stamps. Every stamp is taken from GenericFader::stampEpoch(): a new
 *      one for a person's action, an automatic writer's own inside its
 *      GenericFader::StampScope.
 *      - A function is stamped by its sources (Function::showStamp()): one
 *        added (or renewed: a GO on a list already playing) is the show,
 *        unless it is a preview, an editor's Test or the Workbench (Play
 *        Live too: a preview). One started by another function takes that
 *        one's stamp -- only while that one is the show and worked out in
 *        this session, so the order the functions are refreshed in never
 *        matters -- and keeps it for the session: a cue a Script launched
 *        still drives after the Script ends. One with no source left (it
 *        is stopping) is held, fade-out and all. A fader reads its
 *        function's stamp every frame: a new session holds it at once,
 *        before any gate is worked out again.
 *      - A Show-only fixture moved (by any number of channels) or re-moded
 *        takes a fresh stamp on all its channels; on a channel it shares
 *        with another fixture Show only wins (no On when not in use, no
 *        Ignore Blackout there).
 *      - A function started by another from outside the MasterTimer's
 *        scope takes its parent's stamp, never a fresh one; the effect
 *        runner's writes on a Show-only channel carry their scene's stamp
 *        and meet the channel's mark as any writer's.
 *      - A Park, Mark or cue stack is stamped when set, started or stepped
 *        by hand; a Park or Mark loaded with the show (or a parked fixture
 *        patched later) is stamped by nothing.
 *      - A Simple Desk channel, a VC slider, XY pad or audio DMX bar when
 *        it is really moved (a changed value from a hand or an input --
 *        never an echo, a feedback re-sync, a rebuild, a mode trip, a load
 *        or a page change: a position set by code is no move); the stamp
 *        is the control's, so a fader rebuilt from a control left up is as
 *        old as the control. A playback slider that starts its function
 *        again (after a stop) is as old as its last move. A VC button's
 *        flash is a hand: its flash fader takes the press's stamp.
 *      - An automatic widget (VC audio triggers, a VC Clock) is the show
 *        only from the stamp the operator armed it with, in Operate: by
 *        switching it on, touching it, or working it by a key, an input or
 *        the web (Function::armAutoSource()); never by its own first
 *        start. Everything it drives -- its functions, its DMX bars, and a
 *        VC button, slider or cue list one of its bars works -- takes that
 *        stamp, never a hand's or a GO's (no GO is noted).
 *      - Timecode: a Show, or a section's linked cue list, that external
 *        timecode starts takes the stamp the Show's timecode link was
 *        armed with (Follow switched on in Operate: Show::timecodeArmEpoch());
 *        a Show Tree cue list's timecode GOs, the stamp its Follow
 *        timecode was switched on (or touched) with in Operate
 *        (ShowTreeController::listTimecodeArm()). Loaded on with the show,
 *        or switched on in Design, it is not armed.
 *      - A last look never holds a Show-only channel. Previews, editors'
 *        Tests, Locate, focus and fixture tests are never stamped;
 *        Identify and Reset are refused for a Show-only fixture and for
 *        any fixture sharing its addresses.
 *
 * The emergency stop (All Off, emergencyStop()):
 *  E1. Every Show-only channel goes to its safe value at once: no fade and
 *      no waiting on the function list before the cut (cut first, then
 *      the functions) -- but a pan or tilt (coarse or fine:
 *      FixtureRest::holdsPosition()) stays at what went out there last, in
 *      every frame, so no head moves (Q326; Universe::holdShowOnlyPositions()).
 *      The next write there that counts (E3) drives it again. Every function is stopped, queued ones too -- on
 *      the main thread, queued there when the stop came from another.
 *      From the cut on, nothing stamped before it counts (S0), stopped
 *      yet or not: a frame processed in the gap adds nothing.
 *  E2. It starts a new live session, exactly as the switch to Operate, a
 *      show loaded and a show cleared do.
 *  E3. A writer is stamped again only by an operator action after it: a GO
 *      or claim (for what it starts, and their children through the
 *      root's gate), a hand control really moved, an automatic widget
 *      re-armed in Operate, the timecode link re-armed.
 *  E4. There is no release: a GO on list B makes B's writers current and
 *      nothing else -- list A's fade-out, a stored level, a definition
 *      default, a last look, an armed trigger, schedule or timecode link, a
 *      rebuilt fader from before the stop all stay held.
 *  E5. A function stopped by it is held for the rest of its fade-out (S3:
 *      no source, held).
 *  E6. A Show-only channel's "nothing drives it" value is its safe value
 *      (Universe::setChannelDefaultValue(), Universe::reset()), so no
 *      definition default can come back.
 *  E7. The cut happens on the calling thread, at once; only the footer is
 *      told later. The Simple Desk and every VC Level slider let go of what
 *      they hold, on any channel, Show only or not, that was not moved
 *      since the stop; it stays out until moved.
 *  E8. Once the functions are stopped, every fixture is put at rest, as
 *      Stop all does, through the same code (FixtureRest::restAfterStop()):
 *      a fogger, a hazer or a WLED effect not marked Show only stops too
 *      ("Everything is off"; UX-70). On the main thread, never waiting for
 *      the MasterTimer.
 *  E9. A focus tool lighting the rig through its own layer (Quick focus,
 *      Venue focus, Focus this cue / the pad's walk) ends with it
 *      (ProgrammerController::emergencyStopStruck(), on the main thread):
 *      what it had not saved is put back, as leaving it unsaved does. The
 *      pad goes Stopped first, whatever route the stop came by
 *      (PadRun::stopStruck()). The follow spot (FollowSpotMover) is not
 *      stopped: it holds a position only, and its light came from the cue
 *      the stop has stopped.
 *
 * When OnAtOpen fixtures come on is the show's setting (saved with it,
 * Settings > Output): when you switch to Operate (the default, the safer
 * one) or as soon as the show is open. Blackout-exempt fixtures also hold
 * through Blind (defaultBlindHoldsExempt(); board Q44).
 */
class QLC_ENGINE_EXPORT OutputRuleKeeper final : public QObject
{
    Q_OBJECT

public:
    /** When OnAtOpen fixtures go to full when nothing sets them */
    enum OnAtOpenWhen
    {
        /** From the first switch to Operate on, until another show is opened
         *  (going back to Design does not drop the house lights) */
        OnAtOperate = 0,
        /** As soon as the show is open (the rig-network gate protects) */
        OnAtShowOpen
    };
    Q_ENUM(OnAtOpenWhen)

    /** A new show's (board Q45: Operate, the safer choice) */
    /* Functions, not static data: a dllimport-ed constexpr data member odr-used
       outside the engine DLL fails to link with MinGW (Windows CI). */
    static constexpr OnAtOpenWhen defaultOnAtOpenWhen() { return OnAtOperate; }
    /** Board Q44: Blackout-exempt fixtures hold through Blind */
    static constexpr bool defaultBlindHoldsExempt() { return true; }

    explicit OutputRuleKeeper(Doc *doc);
    ~OutputRuleKeeper() override;

    /** Rebuild every universe's rule bits now (normally done for you,
     *  coalesced) */
    void rebuild();

    OnAtOpenWhen onAtOpenWhen() const { return m_onAtOpenWhen; }
    /** The show's setting: marks the show modified */
    void setOnAtOpenWhen(OnAtOpenWhen when);

    /** Saved only when it differs from a new show's */
    void saveXML(QXmlStreamWriter *doc) const;
    void loadXML(QXmlStreamReader &root);
    /** True when OnAtOpen fixtures are being held at full right now */
    bool onAtOpenArmed() const;

    bool blindHoldsExempt() const { return m_blindHoldsExempt; }
    /** For this session (tests; the default is defaultBlindHoldsExempt()) */
    void setBlindHoldsExempt(bool hold);

    /** The rule bits of one universe (Universe::ChannelType), UNIVERSE_SIZE
     *  bytes -- what rebuild() hands it */
    QByteArray bitsFor(quint32 universe) const;

    /** How many full rebuilds have run (tests) */
    int rebuildCount() const { return m_rebuilds; }
    /** How many fixtures have any rule (for tests and the UI) */
    int ruleFixtureCount() const { return m_count; }

    /** The Show-only fixtures (patched or not), in the show's order */
    QList<quint32> showOnlyFixtures() const { return m_showOnly; }
    /** Why the Show-only fixtures are held right now: Design, Blind --
     *  empty when nothing holds them (Operate, or there are none). In
     *  Operate the previews are still kept off them; that is not a hold
     *  of the show and says nothing here. */
    enum HeldBy { NotHeld = 0, HeldByDesign, HeldByBlind, HeldByEmergencyStop };
    HeldBy showOnlyHeldBy() const;
    /** "Lasers held: Design", "3 Show-only fixtures held: Blind" -- empty
     *  when not held. For the footer chip and the phone. */
    QString showOnlyHeldText() const;

    /** The emergency stop (ShowControl::AllOff): rules E1-E7. Safe from
     *  any thread (an OSC or MSC plugin's). */
    void emergencyStop();
    /** What the footer says: true from the emergency stop until the first
     *  GO after it (noteGo()), a switch of mode or another show. It holds
     *  nothing itself -- the rules do (E4: no release). */
    bool emergencyStopped() const;
    /** A GO was carried out (ShowControl, a VC cue list, the Show tab).
     *  The first one after a stop lets the stage machinery it held go
     *  (holdStageMachinery()); an automatic GO (a StampScope) does not. */
    void noteGo();

    /** Stage machinery stopped where it is (FixtureRest::freezesOnStop():
     *  a curtain, mid-move or at rest). Stop ALL, Fade and stop and the
     *  emergency stop call it before anything stops: every channel of each
     *  such fixture is held at what it has now, over everything that plays
     *  (Universe::Park, Override), so neither the cue letting go, its fade
     *  out, putting the rig at rest, a list stopped later nor a timer moves
     *  it. A Show-only one is held the way its pan and tilt would be
     *  (Universe::holdShowOnlyPositions()). Held until the operator's next
     *  GO (noteGo()) or releaseStageMachinery(). Safe from any thread (an
     *  OSC or MSC emergency stop): the fixtures are read on the main thread
     *  when the rig changes, the levels from the universes now. */
    void holdStageMachinery();
    /** Let go of what holdStageMachinery() holds: the channels keep the
     *  value they have (LTP) until something drives them. Any thread. */
    void releaseStageMachinery();
    bool stageMachineryHeld() const;
    /** "Curtain stopped at 40% — GO to carry on" (how far open, as the
     *  plot draws it: Scenery::poseFromValues()); "2 curtains stopped —
     *  GO to carry on"; empty when nothing is held. For the footer. */
    QString stageMachineryHeldText() const;

    /** Q350: Design, the Show Tree preview on stage (a cue picked while
     *  editing): stage machinery (FixtureRest::freezesOnStop(), the same
     *  predicate as a stop) stays where it physically is -- every channel
     *  of each such fixture held at what it has now, over everything that
     *  plays, the preview's own scenes too -- while the plot and the Rig
     *  Overview draw where the cue in edit puts it (@p drawn: fixture ->
     *  its channels' values; ShowTreeController works them out). The first
     *  call takes the levels; the next picks only change @p drawn. A Show-
     *  only one is held by Design already and is left to that. Main
     *  thread; stageMachineryChanged() when anything changed. */
    void holdMachineryForEdit(const QHash<quint32, QByteArray> &drawn);
    /** The preview let go of the stage (a GO took it, Operate, the preview
     *  stopped): each curtain travels from where it was held to what plays
     *  under it over @p glideMs (the cue's fade: a motor is never told to
     *  jump), else keeps the value it has (LTP). Main thread. */
    void releaseMachineryForEdit(uint glideMs = 0);
    bool machineryHeldForEdit() const;
    /** True while fixture @p fixtureId is held for editing; @p drawn (when
     *  given) where the cue in edit puts it -- empty when that cue (and the
     *  ones before it) never drives it: drawn where it is. Any thread. */
    bool heldForEdit(quint32 fixtureId, QByteArray *drawn = nullptr) const;

    /** Q350 (Fixture::OneShotBehaviour): a Back from a run. Each one-shot
     *  (a kabuki, a dropping scrim) is held where it is -- dropped stays
     *  dropped -- until the next GO, so going back never fires it again.
     *  Called by Chaser::setAction() for every Back (any route), before
     *  the Back is stored; the Back's own GO after it lets nothing go.
     *  Main thread. */
    void noteBack();
    bool oneShotsHeldForBack() const;

    /*********************************************************************
     * Hazards and arming (board Q351, Q352 A, Q353 A)
     *********************************************************************/
    /** Every patched hazard (FixtureRest::isHazard(): fog, haze, flame,
     *  CO2, confetti, a 3R+ laser), in the show's order.
     *
     *  The rule (board Q352 A / Q353 A, second review): the only thing that
     *  ever puts a value other than safe on a hazard's channel is the value
     *  the cue a run stepped to sets for it, while that step is current.
     *  Three layers sit on top of everything (Park priority, on top of every
     *  Park): one holds every hazard channel at 0, always; over it a fire
     *  layer per kind lays the current step's own values -- read from the
     *  cue (ShowTreeController::cueLevels(), tracking and includes in) or
     *  the step's scene, never from what plays underneath -- and writes
     *  only while its gate is open (FaderGate): the Hold down (Burst) or
     *  the arming on (Running), and the grant still the current step's.
     *  So the Simple Desk, Edit Live, the Virtual Console, the web, input
     *  passthrough, override hits, Focus, the programmer, a fixture on the
     *  same address or a stray function never reach a hazard, armed or not;
     *  a cue that sets it to 0 puts it at 0.
     *
     *  At every step a list reaches -- a GO, Back, a jump, a timecode or
     *  audio step, an auto-follow, a sub-cue, a VC cue list, a chaser's own
     *  step -- what it granted before is closed at once (on the thread the
     *  step comes on) and the new step is granted on the main thread: a
     *  Burst if the Hold was down as it was reached, a Running hazard if
     *  armed then; never in Blind. Re-pressing the Hold within the same cue
     *  fires a Burst again (a flame operator's repeated bursts).
     *
     *  Disarmed on a show opened, the emergency stop, Stop ALL / Fade and
     *  stop, sleep and wake, and the spotter's device lost (DeskRecord); a
     *  Hold still down then counts only once let go of. The Hold let go and
     *  the Kill take effect from the input thread (HazardFastPath), with
     *  the main thread busy or not. A Park wins (parked: held at 0). An edit
     *  pick, Design and a programmer edit after a Design run take back what
     *  a run granted. Per frame: one gate read per fire fader. */
    QList<quint32> hazards() const;
    /** Whether @p fixtureId is a hazard of the Burst kind (main thread) */
    bool isBurstHazard(quint32 fixtureId) const;
    /** Whether hazard @p fixtureId is parked (held at off) */
    bool isParkedHazard(quint32 fixtureId) const;
    /** Whether hazard @p fixtureId may be out now: what a run reached it
     *  with is let through (the hold is not on it). Any thread. */
    bool hazardLive(quint32 fixtureId) const;

    /** The spotter's Hold control down (true) or let go of (false). Let
     *  go: every Burst back to safe at once (from the next frame). After a
     *  disarm a hold already down counts only once it has been let go and
     *  pressed again. Any thread. */
    void setHazardHold(bool down);
    bool hazardHeld() const { return m_hazardHold.load(std::memory_order_relaxed); }
    /** The spotter's Arm control: the Running hazards armed (fired by the
     *  next cue that sets them). Any thread. */
    void armHazards();
    bool hazardsArmed() const { return m_hazardArmed.load(std::memory_order_relaxed); }
    /** The spotter's Kill control: every hazard off at once and disarmed. */
    void killHazards();
    /** Where the spotter's Hold and Kill are (universe, channel), for the
     *  input thread's own path (HazardFastPath); invalid universe: none. */
    void setHazardControls(quint32 holdUniverse, quint32 holdChannel,
                           quint32 killUniverse, quint32 killChannel);
    /** What hazard @p fixtureId sends now (granted and its gate open); empty
     *  when it is at safe. Main thread. */
    QByteArray hazardOutput(quint32 fixtureId) const;
    /** Every hazard safe and disarmed, for @p why ("the emergency stop").
     *  Logged when anything was armed, held or out. Any thread. */
    void disarmHazards(const QString &why);

    /** A hand on a hazard's channel -- a Level slider, the Simple Desk --
     *  while it is held safe: handHeldOnHazard() says why it stays dark,
     *  at most once every few seconds per hazard. Nothing for a light, or
     *  a hazard that is out. Main thread. */
    void noteHandOnHazard(quint32 fixtureId);
    /** The hazards some scene sets above off (a cue's, a VC button's):
     *  what the spotter has to let fire. Main thread. */
    QList<quint32> hazardsCued() const;
    /** "3 hazards stay off until the spotter arms them — Show Review lists
     *  them" for a show just loaded with hazards its scenes fire; empty
     *  without. */
    QString hazardLoadNotice() const;

    /** A GO a few frames on: which hazards its cue reached while not let
     *  fire -- skipped (main thread). Cleared by a GO that skips none, a
     *  stop, a mode change, another show. */
    QList<quint32> hazardsSkipped() const { return m_hazardsSkipped; }
    /** "CO2 jet skipped — not held", "Hazer skipped — not armed", two or
     *  more joined; empty when none was. For the footer. */
    QString hazardSkippedText() const;

    enum HazardState { NoHazards, HazardsDisarmed, HazardsArmed, HazardsHeld };
    /** What the footer chip says: none in the show, DISARMED, ARMED (the
     *  Running ones), HELD (the Hold is down; HELD wins over ARMED). */
    HazardState hazardState() const;
    /** "Hazards: DISARMED" / "ARMED" / "HELD", "Hazards: ARMED · HELD",
     *  then " · " and hazardSkippedText(); empty with no hazard. */
    QString hazardChipText() const;
    /** The hazard log, oldest first, each line with its time: every fire,
     *  skip, arm, kill and disarm this session (at most kHazardLogLines).
     *  Each line goes to the app's log too (qInfo, "hazard:"; kept in
     *  ~/qlcconsole.log with --log), and to the Teaching show's GO log. */
    QStringList hazardLog() const { return m_hazardLog; }
    static const int kHazardLogLines = 500;

    /** The Park changed (ProgrammerController::parkChanged()): Park wins
     *  over every hold here -- a stop's, an edit's, a Back's -- and over
     *  the hazards' hold, so a parked fixture is let go of by each. Main
     *  thread. */
    void parkChanged();

    /** A Show-only channel's safe value: 0 for an intensity channel, and
     *  where nothing drives it (FixtureRest::restValue()) for the rest: a
     *  laser, fogger or hazer all 0, a gobo or colour wheel open. */
    static uchar safeValue(const Fixture *fx, quint32 channel);
    /** The safe values of one universe's Show-only channels (UNIVERSE_SIZE
     *  bytes, 0 elsewhere) -- what rebuild() hands it */
    QByteArray safeValuesFor(quint32 universe) const;
    /** Q326: one universe's Show-only pan and tilt channels
     *  (FixtureRest::holdsPosition(); one byte each, 1 where it is) --
     *  what the emergency stop holds where they are */
    QByteArray positionsFor(quint32 universe) const;

    /** The channel indices of @p fx a rule of @p rule covers (see above) */
    static QList<quint32> coveredChannels(const Fixture *fx, Fixture::OutputRule rule);

    /** A short plain-words line per rule set, for tooltips and badges:
     *  "Ignores Blackout, Grand Master" etc. Empty for none. */
    static QString describe(Fixture::OutputRules rules);

    /** A fixture's Show-only channels marked where it is now, at once.
     *  Doc::applyFixtureToUniverses() calls it before a Show-only fixture's
     *  defaults are written (R9-2), so none of them is ever sent. */
    void markNow(quint32 fixtureId);

signals:
    /** The Show-only fixtures, or why they are held, changed */
    void showOnlyChanged();
    /** The emergency stop struck, starting session @p epoch (the Simple
     *  Desk lets go of what was not moved since; the programmer ends its
     *  focus tools, E9). Emitted on the thread that struck it. */
    void emergencyStopStruck(quint32 epoch);
    /** emergencyStopped() went false: the first GO after the stop (not a
     *  timecode or other automatic one), a new session (Operate, Blind
     *  ending in Operate, a show loaded) or the show cleared. On the main
     *  thread. The pad's red follows it (PadRun::stopEnded()): the pad and
     *  the footer read one source. */
    void emergencyStopEnded();
    /** holdStageMachinery() held, or let go of, the machinery. On the main
     *  thread. */
    void stageMachineryChanged();
    /** A hazard's arming, hold, list or skips changed. Main thread. */
    void hazardsChanged();
    /** A line was added to hazardLog() (with its time). Main thread. */
    void hazardLogged(const QString &line);
    /** noteHandOnHazard(): why it stays dark. Main thread. */
    void handHeldOnHazard(const QString &why);

private:
    void scheduleRebuild();
    /** A new live session (E2): Operate, All Off, a show loaded or cleared */
    void startSession();
    void pushPolicy();
    /** Every universe told whether the show may drive its Show-only
     *  channels (Universe::setShowOnlyLive(): Operate) */
    void pushShowOnlyLive();

    Doc *m_doc;
    bool m_pending = false;
    int m_count = 0;
    OnAtOpenWhen m_onAtOpenWhen = defaultOnAtOpenWhen();
    bool m_blindHoldsExempt = defaultBlindHoldsExempt();
    /** OnAtOperate: Operate has been entered since the show was opened */
    bool m_seenOperate = false;
    QList<quint32> m_showOnly;
    HeldBy m_lastHeldBy = NotHeld;
    /** The session the last emergency stop started (0: none) */
    std::atomic<quint32> m_stopEpoch { 0 };
    /** Where each Show-only fixture's channels are marked (absolute base,
     *  channel count) */
    QHash<quint32, QPair<quint32, quint32> > m_showOnlyAt;
    /** ...and in which mode (a mode change takes fresh marks: R8-3) */
    QHash<quint32, const class QLCFixtureMode *> m_showOnlyMode;
    int m_rebuilds = 0;
    class QTimer *m_rebuildTimer = nullptr;

    /** Stage machinery (FixtureRest::freezesOnStop()) as the rig is now:
     *  worked out on the main thread (rebuild(), a stop there), read by a
     *  stop from any thread. */
    struct Machine
    {
        quint32 fixture = 0;
        quint32 base = 0;       //!< absolute address (universe << 9 | address)
        quint32 channels = 0;
        bool showOnly = false;
        Fixture::Behaviour behaviour = Fixture::MachineryBehaviour;
        /** On when not in use: its full goes out after the faders (never
         *  in the pre-Grand-Master values), so it is read from what is sent */
        bool onAtOpen = false;
        /** Per channel, 1 where it is intensity (HTP): a Show-only one's
         *  hold there is let go at the GO, as a 0 is no write there */
        QByteArray intensity;
    };
    QVector<Machine> machinesNow() const;
    /** What each of @p machines has on the output now (the faders' last
     *  frame, before the Grand Master), and the levels holding it there */
    QHash<quint32, QByteArray> capture(const QVector<Machine> &machines,
                                       QList<OutputLayer::Level> *levels) const;
    static QList<OutputLayer::Level> levelsOf(const QHash<quint32, QByteArray> &held);
    bool isParked(quint32 fixtureId) const;
    /** The hazards read again as the rig and the Park are now (main
     *  thread), then applyHazards() */
    void layHazards();
    /** The three layers laid from the hazards and the grants: the
     *  passthrough held first, then safe everywhere, then the grants. Main
     *  thread (and stops on any thread). */
    void applyHazards();
    /** A list's step changed (any thread): its grant closed now, the new
     *  step granted on the main thread with the Hold / arming / Blind as
     *  they are at this moment. Also a list stopped (@p step -1). */
    void hazardStepReached(quint32 chaserId, int step);
    /** The grants worked out again from every running list's step and what
     *  each step was reached with (main thread); @p stepped: the list whose
     *  step this is for, whose skips the footer says. */
    void grantHazards(quint32 stepped);
    /** What step @p step of list @p chaserId sets the hazards to */
    QHash<quint32, QByteArray> hazardStepLevels(quint32 chaserId, int step) const;
    /** Every grant taken back now (an edit pick, Design, a Kill, a stop). */
    void revokeHazards();
    /** Each hazard's output now, compared with the last: every change logged */
    void logHazardOutputs(const QString &why);
    /** One line in the hazard log (queued to the main thread) */
    void logHazard(const QString &line);
    void emitHazardsChanged();
    /** The rig's guesses changed (a laser class, a scenery, a symbol) */
    void guessesChanged();
    void watchChaser(class Chaser *chaser);
    void emitMachineryChanged();
    mutable QMutex m_machineMutex;
    QVector<Machine> m_machines;
    /** What is held now: fixture -> its channels' values (guarded) */
    QHash<quint32, QByteArray> m_machineHeld;
    /** The machines held (guarded): what a release lets go of */
    QVector<Machine> m_machinesHeld;
    /** Counts holds and releases (guarded): a GO's late release lets go
     *  only of the hold it found */
    quint32 m_machineGen = 0;
    /** The held levels on the output (non-Show-only machinery) */
    std::unique_ptr<OutputLayer> m_machineLayer;
    /** Q350, held while editing (guarded by m_machineMutex): fixture ->
     *  the values held on the output, and where the cue in edit puts it */
    QHash<quint32, QByteArray> m_editHeld;
    QHash<quint32, QByteArray> m_editDrawn;
    std::unique_ptr<OutputLayer> m_editLayer;
    /** One-shots held by a Back until the next GO (guarded) */
    QHash<quint32, QByteArray> m_backHeld;
    quint32 m_backGen = 0;
    /** noteBack() this turn of the event loop (guarded by m_machineMutex) */
    std::atomic<bool> m_backFresh { false };
    std::unique_ptr<OutputLayer> m_backLayer;
    struct Hazard
    {
        quint32 base = 0;       //!< absolute address (universe << 9 | address)
        quint32 channels = 0;
        bool burst = true;
        /** Parked: held at off, never let fire (Park wins, at off) */
        bool parked = false;
        QString name;
    };
    Hazard hazardOf(const Fixture *fx) const;
    /** One fixture added or changed: its hold laid now, not after the
     *  event loop (a repatch mid-show, a show straight into Operate) */
    void layHazardNow(quint32 fixtureId);
    /** The absolute addresses the passthrough is kept off (apply mutex) */
    QSet<quint32> m_passHeld;
    bool m_passForce = false;
    /** Every hazard (written on the main thread, guarded by m_hazardMutex) */
    QHash<quint32, Hazard> m_hazards;
    QList<quint32> m_hazardOrder;
    /** How each running list's current step was reached (guarded) */
    struct Reach
    {
        int step = -1;
        bool held = false;
        bool armed = false;
        bool blind = false;
        quint32 epoch = 0;
    };
    QHash<quint32, Reach> m_reach;
    /** The lists whose step grants something now (guarded): only their
     *  steps close the gates (an effect chaser stepping elsewhere does not) */
    QSet<quint32> m_granting;
    /** What the grants lay (guarded): fixture -> values */
    QHash<quint32, QByteArray> m_burstGrant, m_runningGrant;
    /** What went out last, for the log (main thread) */
    QHash<quint32, QByteArray> m_lastOut;
    mutable QMutex m_hazardMutex;
    /** One applyHazards() at a time, so the layers are laid in order */
    QMutex m_hazardApplyMutex;
    std::atomic<bool> m_hazardHold { false };
    std::atomic<bool> m_hazardArmed { false };
    /** After a disarm: a Hold already down counts once let go of */
    std::atomic<bool> m_hazardHoldNeedsRelease { false };
    /** The step epoch: moved on by every step that closes a grant, a Kill,
     *  a stop; the grant is open only while m_hazardGrant equals it */
    std::atomic<quint32> m_hazardStep { 1 };
    std::atomic<quint32> m_hazardGrant { 0 };
    FaderGate m_burstGate, m_runningGate;
    std::unique_ptr<OutputLayer> m_hazardLayer;
    std::unique_ptr<OutputLayer> m_burstLayer, m_runningLayer;
    /** The Hold as the log last said it (main thread) */
    bool m_holdShown = false;
    std::unique_ptr<class HazardFastPath> m_fast;
    QSet<quint32> m_watched;
    /** Doc::clearing() to cleared(): nothing of the rig is read */
    bool m_clearing = false;
    /** hazardsSkipped() and why each (true: a Burst not held) (main thread) */
    QList<quint32> m_hazardsSkipped;
    QHash<quint32, bool> m_hazardSkippedBurst;
    QStringList m_hazardLog;
    QHash<quint32, qint64> m_handSaidAt;
};

/** @} */

#endif

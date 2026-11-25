/*
  Q Light Controller Plus
  controlmap.h

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

#ifndef CONTROLMAP_H
#define CONTROLMAP_H

#include <QMultiHash>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QSet>

#include "qlcengine_export.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class EncoderBank;
class Chaser;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCControlMap QStringLiteral("ControlMap")
/** This build has the emergency-stop target (the CS1 tests ask). */
#define CONTROLMAP_HAS_EMERGENCY_STOP 1
/** ... and the stop that says when it cannot work (stopHasProblem()). */
#define CONTROLMAP_STOP_REVIEWED 1
/** ... and the press rule (stopProblem(), stopStateChanged()). */
#define CONTROLMAP_STOP_FLAGS 1
/** ... and isInputConnected(), for the tests. */
#define CONTROLMAP_HAS_INPUT_PROBE 1
/** ... and the light that says standby, held and stopped (lamp(), CS3). */
#define CONTROLMAP_HAS_LAMP_STATES 1
/** ... and a place on each trigger (Place, Q295). */
#define CONTROLMAP_HAS_PLACES 1
/** ... and a Back on a cue list (CueListBackTarget, phase F). */
#define CONTROLMAP_HAS_BACK 1

/**
 * The Control Map: a MIDI note or CC, or an OSC message -- any control on
 * any input universe -- bound straight to something to fire, with no
 * Virtual Console widget in between (docs/dev/CONTROLMAP.md). A binding
 * fires a function (a scene, an effect, a chaser...), GOes a Show Tree cue
 * list, GOes one cue of a list, or fires a hit; how a press acts is its
 * mode. The control's light says what the thing it fires is doing (input
 * feedback, as a VC button's does), where the universe has a feedback line:
 * dark when a press would do nothing now, dim when it is ready, bright
 * while it runs, two blinks when the double-GO guard held a command on it
 * (lamp(), CS3). Each of those is read from the one place that owns it --
 * ShowControl and the OutputRuleKeeper -- never worked out here, and never
 * per frame.
 *
 * Saved with the show as <ControlMap>, only when there is a binding, so a
 * show without one saves as before; QLC+ 4 skips the element.
 *
 * A computer key is a control too (docs/features/other-improvements.md,
 * "Keys on the computer keyboard"): its universe is KeyboardUniverse and its
 * channel the key with its modifiers (keyChannel()). The UI's KeyTriggers
 * hands keys to handleKey(), which fires one only where it may: in Operate,
 * or in Design while its target is armed (isArmed(): the Run tab's list,
 * or what is selected in the Show Tree). Saved as a Key attribute in
 * place of Universe/Channel, so a build without keys skips it.
 *
 * A trigger has a place (board Q295, Q296, Q303; docs/design/CONTROL_SCOPES.md
 * phase 3), as a hit has: a cue, a cue list, a production or everywhere. It
 * is armed only while its place is live (the hits' own rule,
 * ShowTreeController::isHitPlaceLive(); in Design also while the place is
 * selected in the Show Tree or is the Run tab's list). One control may have
 * several triggers at different places: a press fires the narrowest armed
 * one (cue > list > production > everywhere). Several at the same place
 * step through, one per press (Q157). A trigger whose place is not live
 * can still let go of what it runs. The emergency stop is always
 * everywhere, alone on its control, and comes before all of it. Saved as
 * Scope= and ScopeOf= on the trigger, only when not everywhere: a show
 * saved before places loads with every trigger everywhere, as it was. A
 * place that goes (its cue, list or production deleted) widens the trigger
 * as a hit's is (fixPlaces()), said in placeNote(); a place from a newer
 * version is kept, saved back as it was, and never armed.
 *
 * GUI thread only: input arrives queued from the input patches, feedback
 * follows the MasterTimer's started/stopped signals queued here. With no
 * binding (and not learning) nothing is connected, so input costs nothing.
 */
class QLC_ENGINE_EXPORT ControlMap final : public QObject
{
    Q_OBJECT

public:
    explicit ControlMap(Doc *doc);
    ~ControlMap();

    /** What a binding fires. */
    enum TargetType
    {
        FunctionTarget, //!< a function, by its id
        CueListTarget,  //!< a Show Tree cue list, by its id: GO
        CueTarget,      //!< a Show Tree cue, by its id: GO that cue
        HitTarget,      //!< a Show Tree hit (a Looks section's cue), by its id
        EmergencyStopTarget, //!< ALL OFF (id 0): the surface's emergency stop, one button
        /** A Show Tree cue list, by its id: Back (the cue before). A target
         *  of its own, so a GO and a Back on one list are two controls. */
        CueListBackTarget
    };

    /** How a press acts. On a cue list or a cue, Toggle and Start only GO,
     *  Momentary GOes on the press and stops the list on the release, and
     *  Stop only stops the list. On a list's Back, Toggle and Start only go
     *  back one cue (each press; the release does nothing) and Stop only
     *  stops the list. */
    enum Mode
    {
        Toggle,     //!< a press starts it, the next press stops it
        Momentary,  //!< on while held
        StartOnly,  //!< a press starts it (never stops)
        StopOnly    //!< a press stops it (never starts)
    };

    /** What a bound control's light says (CS3): the press rule (board
     *  Q269) made visible on the desk, so the operator does not find out
     *  by pressing. One status source each (lamp()). */
    enum Lamp
    {
        LampOff,        //!< dark: a press would do nothing now
        LampStandby,    //!< dim: a press would fire it
        LampRunning,    //!< bright: what it fires is running
        LampHeld,       //!< two blinks: the double-GO guard held a command
        LampStopped     //!< the emergency stop's own light, after a stop
    };
    Q_ENUM(Lamp)

    struct Target
    {
        TargetType type = FunctionTarget;
        quint32 id = 0xFFFFFFFF;

        bool isValid() const { return id != 0xFFFFFFFF; }
        bool operator==(const Target &o) const { return type == o.type && id == o.id; }
        bool operator!=(const Target &o) const { return !(*this == o); }
    };

    /** Where a trigger is armed (Q295): the places a hit has, from the
     *  widest, so a higher value is a narrower place and wins. */
    enum PlaceScope
    {
        Everywhere = 0, //!< always (the old behaviour, and every old show's)
        ProductionPlace,//!< while that production is current (or none is)
        ListPlace,      //!< while that cue list runs (or is its production's main list, nothing running)
        CuePlace,       //!< while that cue is the live cue of its list
        /** A place this build does not know (a show saved by a later one:
         *  a layer, an effect), or one it cannot read: kept as it was
         *  saved and never armed -- never widened to everywhere. */
        NewerPlace = 100
    };

    struct Place
    {
        /* A constructor, not an aggregate: GCC's -Wmissing-field-initializers
           (the Linux leg's -Werror) rejects a brace list that leaves the
           newer-place strings out, and most places name only scope and id. */
        Place() {}
        Place(PlaceScope s, quint32 i) : scope(s), id(i) {}
        Place(PlaceScope s, quint32 i, const QString &ns, const QString &no)
            : scope(s), id(i), newerScope(ns), newerOf(no) {}

        PlaceScope scope = Everywhere;
        quint32 id = 0xFFFFFFFF;    //!< the production, list or cue (not for Everywhere)
        /** NewerPlace: the Scope= and ScopeOf= it was loaded with, saved back as they were. */
        QString newerScope;
        QString newerOf;

        bool operator==(const Place &o) const
        {
            if (scope != o.scope)
                return false;
            if (scope == NewerPlace)
                return newerScope == o.newerScope && newerOf == o.newerOf;
            return scope == Everywhere || id == o.id;
        }
        bool operator!=(const Place &o) const { return !(*this == o); }
    };

    struct Binding
    {
        /* As Place: a binding written { universe, channel, target, mode }
           (every one from before places) means everywhere. */
        /* Overloads, not default arguments: GCC wants Target's and Place's
           member initializers before ControlMap ends, so a default
           argument of either type is refused here. */
        Binding() {}
        Binding(quint32 u, quint32 c) : universe(u), channel(c) {}
        Binding(quint32 u, quint32 c, const Target &t) : universe(u), channel(c), target(t) {}
        Binding(quint32 u, quint32 c, const Target &t, Mode m)
            : universe(u), channel(c), target(t), mode(m) {}
        Binding(quint32 u, quint32 c, const Target &t, Mode m, const Place &p)
            : universe(u), channel(c), target(t), mode(m), place(p) {}

        quint32 universe = 0xFFFFFFFF;
        quint32 channel = 0xFFFFFFFF;
        Target target;
        Mode mode = Toggle;
        Place place;

        bool operator==(const Binding &o) const
        {
            return universe == o.universe && channel == o.channel && target == o.target && mode == o.mode
                   && place == o.place;
        }
    };

    static Target function(quint32 id) { return Target{ FunctionTarget, id }; }
    static Target cueList(quint32 id) { return Target{ CueListTarget, id }; }
    /** Back on cue list @a id (ShowControl's Back, through run()). */
    static Target cueListBack(quint32 id) { return Target{ CueListBackTarget, id }; }
    /** Whether @a type plays a cue list (GO, a cue's GO, Back): its modes
     *  are the list's (modesFor()), and it is armed as the list is. */
    static bool isListType(TargetType type)
    { return type == CueListTarget || type == CueTarget || type == CueListBackTarget; }
    static Target cue(quint32 id) { return Target{ CueTarget, id }; }
    static Target hit(quint32 id) { return Target{ HitTarget, id }; }
    /** The emergency stop (ALL OFF through ShowControl::run(), the one path).
     *  Only one binding of it can exist. */
    static Target emergencyStop() { return Target{ EmergencyStopTarget, 0 }; }

    static Place everywhere() { return Place(); }
    static Place inProduction(quint32 id) { return Place{ ProductionPlace, id, QString(), QString() }; }
    static Place inList(quint32 id) { return Place{ ListPlace, id, QString(), QString() }; }
    static Place inCue(quint32 id) { return Place{ CuePlace, id, QString(), QString() }; }

    /** The universe of a computer-keyboard key's binding (its channel:
     *  keyChannel()). No input universe has this number. */
    static constexpr quint32 KeyboardUniverse = 0xFFFFFFFE;
    static bool isKey(const Binding &b) { return b.universe == KeyboardUniverse; }
    /** A key and its modifiers as a binding's channel (the keypad flag
     *  dropped, so a number is the same key either side). 0: no key, or a
     *  modifier on its own. */
    static quint32 keyChannel(int key, Qt::KeyboardModifiers modifiers);
    /** A key channel in words, as the system writes it ("F5", "⇧1" on a
     *  Mac, "Shift+1" elsewhere). */
    static QString keyName(quint32 channel);

    /** The Encoder bank scheme: a MIDI controller's endless encoders
     *  driving the selection in Build (docs/features/encoder-banks.md).
     *  Saved with this map; off until a device is set up. */
    EncoderBank *encoderBank() const { return m_encoderBank; }

    /*********************************************************************
     * Bindings
     *********************************************************************/
public:
    QList<Binding> bindings() const { return m_bindings; }
    int count() const { return m_bindings.count(); }
    bool isEmpty() const { return m_bindings.isEmpty(); }

    /** The binding firing @a target (null: none). */
    const Binding *binding(const Target &target) const;
    /** The binding a press of a control fires now: of the armed ones
     *  (armedOn()), the next one (they step through). Null: none is armed
     *  (or nothing is bound there). */
    const Binding *bindingOn(quint32 universe, quint32 channel) const;
    /** Every binding on a control, in the table's order. */
    QList<Binding> bindingsOn(quint32 universe, quint32 channel) const;
    /** Whether a control fires something here (at any place). */
    bool isBound(quint32 universe, quint32 channel) const
    { return m_byControl.contains(key(universe, channel)); }

    /** Binds @a b. One thing has one control, so a binding for the same
     *  target is replaced. On the same control, one at the same place is
     *  replaced too -- unless @a stepThrough: then both stay and a press
     *  fires each in turn (Q303). Different places share a control (Q296).
     *  The emergency stop is alone on its control: either one replaces the
     *  other. Marks the show modified. */
    void setBinding(const Binding &b, bool stepThrough = false);
    /** Drops @a target's binding (its light goes off); false: none. */
    bool removeBinding(const Target &target);
    /** Changes how @a target's control acts; false: no binding. */
    bool setMode(const Target &target, Mode mode);
    /** Moves @a target's binding to @a place. False: no binding, the
     *  emergency stop (always everywhere), or another trigger on its
     *  control is at that place (isPlaceTaken(): never replaced from here;
     *  Learn refuses it, or steps through them). */
    bool setPlace(const Target &target, const Place &place);
    /** Whether another binding on @a b's control is at @a place. */
    bool isPlaceTaken(const Binding &b, const Place &place) const;
    /** Whether a computer key is bound. */
    bool hasKeyBindings() const { return m_keyBindings > 0; }
    /** Drops every binding (each light off). */
    void removeAll();

    /** Back to a new show's (no bindings, not learning); doesn't mark it
     *  modified or send feedback. */
    void reset();

    /*********************************************************************
     * Learning
     *********************************************************************/
public:
    /** Listens for the next press on any input universe (a value above 0:
     *  a release or a fader at zero doesn't count): heard() says which. */
    void listen();
    /** listen(), then bind what is heard to @a target with @a mode. */
    void learn(const Target &target, Mode mode = Toggle);
    /** listen() for a button, and only a button (the emergency stop):
     *  the first press is not heard yet, it asks for a second one on the
     *  same control (confirming(), after a release), and heard() comes only
     *  then. A control the profile calls a fader, knob or encoder, aftertouch,
     *  or one whose values are not a button's (anything between the press
     *  and its repeat) is refused (refused() says why) and learning goes on. */
    void listenForButton();
    void stopListening();
    bool isListening() const { return m_listening; }
    /** Whether a control is a button: not a fader, a knob, an encoder or
     *  aftertouch, by its input profile, else by what a MIDI channel number
     *  is. OSC, programs and unknown lines are buttons. */
    bool isButtonControl(quint32 universe, quint32 channel) const;
    /** Test hooks: how long a first press waits for its second, and the gap
     *  after which a non-zero on the stop is a new press, not the same run. */
    void setConfirmMs(int ms) { m_confirmMs = ms; }
    bool isInputConnected() const { return m_inputConnected; }
    void setStopDebounceMs(int ms) { m_estopDebounceMs = ms; }
    /** How long after a device comes back (a replug, a wake) what it sends
     *  may be its state dump (phase R): a stop whose first value then is 0
     *  ignores the ONE non-zero that follows inside this (ms); a second
     *  press, or any press after another 0, fires. */
    void setDumpWindowMs(int ms) { m_dumpWindowMs = ms; }
    /** The state-dump guard for a trigger or a VC button (phase R): inside
     *  the dump window after @a universe's device came back, a non-zero
     *  value acts only when a 0 from that same control came first (a real
     *  release, then press). True: @a value is ignored (the device's dump
     *  of "pressed"), said once per control and replug as @a what
     *  (stateDumpIgnored()). The opposite of the stop's rule on purpose: an
     *  unwanted GO is worse than a missed one, which the operator sees and
     *  presses again. Never for the stop (its own rule) or a fader (pick-up
     *  handles those). A 0 is never ignored; it arms the control. */
    bool isTriggerDump(quint32 universe, quint32 channel, uchar value, const QString &what);
    int dumpWindowMs() const { return m_dumpWindowMs; }
    /** What is wrong with the stop's control, in words (empty: nothing):
     *  its profile calls it a fader or knob (it still fires; said), it is a computer
     *  key (never works), or it has been seen moving like a fader (it still
     *  fires; shown loudly: the panel, one footer line). */
    QString stopProblem() const;
    bool stopHasProblem() const { return !stopProblem().isEmpty(); }
    /** The stop's device, when its universe hears nothing now (phase R):
     *  "'APC MINI' is not connected", "universe 3 has no input patched";
     *  empty when it is there (or no stop is learned, or it is a key). */
    QString stopDeviceMissing() const;

    /** The emergency stop's fader rule, for any run of values: a value
     *  that differs from the last one, with no 0 (a release) between them
     *  and under @a windowMs after it, is a fader moving -- a button sends
     *  the same value again, or a 0 first. */
    static bool movesLikeFader(bool releasedSince, qint64 sinceLastMs, uchar last, uchar value,
                               int windowMs);
    /** What learning heard a control send, since the Learn window began
     *  (kept across its presses -- listen() again after heard() -- and
     *  forgotten by stopListening()). Whether it is a fader for a GO or a
     *  hit: its profile calls it a fader, knob or encoder, or it moved
     *  like one (movesLikeFader()). A computer key never is. */
    bool learnSawFader(quint32 universe, quint32 channel) const;
    /** Whether it sent one value per press, as a latching button does: on,
     *  then 0 a press later, then on again (each a press apart, never a
     *  release inside a press's hold in between), and nothing else. A
     *  press on such a button is a press only every other time. */
    bool learnSawLatching(quint32 universe, quint32 channel) const;
    /** Test hook: the shortest gap between two values of a latching
     *  button's run (a release quicker than it is a momentary button's). */
    void setLatchGapMs(int ms) { m_latchGapMs = ms; }
    /** Test hook: the longest a press is held. A 0 sooner than this after
     *  the on is a release (momentary), never a latching button's 0. */
    void setLatchHoldMs(int ms) { m_latchHoldMs = ms; }

    /*********************************************************************
     * Names
     *********************************************************************/
public:
    /** The control in words: its input profile's name for it, else a MIDI
     *  note or CC ("Note 60 (C4), MIDI ch 1"), else its channel number;
     *  with the universe's name. */
    QString controlName(quint32 universe, quint32 channel) const;
    /** What a target is ("Scene: Warm wash", "Cue list: Act 1",
     *  "Cue 4 of Act 1", "Hit: Stab"); "(gone)" when it isn't there. */
    QString targetName(const Target &target) const;
    /** Whether @a target is still in the show. */
    bool targetExists(const Target &target) const;
    /** The mode in words, as it acts on a target of @a type. */
    static QString modeName(Mode mode, TargetType type = FunctionTarget);
    /** The modes a target of @a type is offered, in the order the Learn
     *  window and the table list them. */
    static QList<Mode> modesFor(TargetType type);

    static QString typeToString(TargetType type);
    static bool stringToType(const QString &str, TargetType *type);
    static QString modeToString(Mode mode);
    static bool stringToMode(const QString &str, Mode *mode);

    /*********************************************************************
     * Places (Q295, Q296, Q303)
     *********************************************************************/
public:
    /** Whether @a place is live now: everywhere always; otherwise the
     *  hits' rule (ShowTreeController::isHitPlaceLive()), and in Design
     *  also while the cue is selected in the Show Tree, or the list is
     *  selected or the Run tab's. A place that is gone never is. */
    bool isPlaceLive(const Place &place) const;
    /** Of the bindings on a control, the armed ones at the narrowest live
     *  place (cue > list > production > everywhere), in the table's order.
     *  A hit counts only while it may fire (surfaceMayFire()). Two places
     *  of that scope live at once: the run list's (isRunListPlace()), else
     *  the first in the table. More than one: they share that one place
     *  and step through. */
    QList<Binding> armedOn(quint32 universe, quint32 channel) const;
    /** Whether @a b is one of the armed ones on its control (armedOn()). */
    bool isArmedOnControl(const Binding &b) const;
    /** The place in words, as a sentence ends: "everywhere", "in cue 2.3
     *  Chorus", "in cue list Main", "in production DanceHysteria". */
    QString placeName(const Place &place) const;
    /** What a control does where, as one line: "Scene: Warm everywhere,
     *  except Hit: Stab in cue 2.3" (narrower places after "except"); two
     *  at one place "... (each press the next)". @a bindings: one control's. */
    QString placesText(const QList<Binding> &bindings) const;
    /** The places a trigger learned at @a where may have, narrowest first:
     *  that place and the ones around it (a cue, its list, its production,
     *  everywhere), then those around the cue live now (or selected in the
     *  Show Tree), each once. Everywhere is always one of them. For a hit
     *  @a target, only its own place and those inside it (placeOfHit()). */
    QList<Place> placeChoices(const Place &where) const;
    QList<Place> placeChoices(const Place &where, const Target &target) const;
    /** Whether @a inner is @a outer or inside it (a cue in a list, a list
     *  in a production; everything is inside everywhere). */
    bool isPlaceWithin(const Place &inner, const Place &outer) const;
    /** Whether @a place is (or holds) the run list: everywhere, its
     *  production, the list itself or one of its cues. */
    bool isRunListPlace(const Place &place) const;
    /** The place of hit @a hitId (where a binding learned on it goes). */
    Place placeOfHit(quint32 hitId) const;

    static QString placeScopeToString(PlaceScope scope);
    /** NewerPlace for anything this build does not know (never everywhere). */
    static PlaceScope stringToPlaceScope(const QString &str);
    /** Whether @a place is in the show now (everywhere always; a newer
     *  version's place never). */
    bool placeExists(const Place &place) const;
    /** What happened to @a target's place, in words (empty: nothing to
     *  say): its cue, list or production was deleted, so it was widened as
     *  a hit is (ShowTreeController's scoped-hit fix-up: to its production,
     *  else everywhere); or its place is from a newer version, so it never
     *  fires here. */
    QString placeNote(const Target &target) const;
    /** placeNote() of every trigger that has one, each as a sentence. */
    QStringList placeNotes() const;

    /*********************************************************************
     * Computer keys: where they may fire
     *********************************************************************/
public:
    /** Key @a channel (keyChannel()) pressed or released. A press fires
     *  its binding when keyMayFire(); a release lets go of a key that
     *  fired (whatever the mode is now). True: it was a key trigger's (the
     *  key is used up); false: not one, or not now. */
    bool handleKey(quint32 channel, bool pressed);
    /** Whether a key held down fired something (its repeats are used up). */
    bool isKeyDown(quint32 channel) const
    { return m_down.contains(key(KeyboardUniverse, channel)); }
    /** Lets go of every key held (the app lost the keyboard). */
    void releaseKeys();
    /** A key may fire @a target now (board Q277): the surface rule
     *  (surfaceMayFire()) for a cue list, cue or hit; a scene or effect only
     *  in Operate (a key is also typing). */
    bool keyMayFire(const Target &target) const;
    /** A MIDI or OSC press may start @a target now (CS1, board Q269): a
     *  function always (Build tries scenes and effects in Design; a
     *  Show-only fixture is held in Design anyway); a cue list or a cue in
     *  Operate, and in Design when isArmed(); a hit only where its place is
     *  live in Operate (ShowTreeController::isHitArmed()), and in Design
     *  when isArmed(). What is running can always be let go or stopped, and
     *  the emergency stop always works. */
    bool surfaceMayFire(const Target &target) const;
    /** Why @a target may not start now, in words (empty: it may). */
    QString refusalFor(const Target &target) const;
    /** Armed for keys in Design: @a target is in the run list (the cue
     *  list the Run tab plays: that list, its cues and the hits living in
     *  it, its cues, its production or everywhere; and any hit whose place
     *  is live) or selected in the Show Tree (a selected list arms itself,
     *  its cues and its hits; a cue itself and its hits; a hit itself). A
     *  scene or effect is never armed: its key fires in Operate only. */
    bool isArmed(const Target &target) const;
    /** The cue list the Run tab follows (invalid: the current production's
     *  main cue list), and the one that is now. The Run tab's pick is the
     *  pad's list too (PadRun) and is saved with the show. */
    void setRunTabList(quint32 listId);
    quint32 runTabList() const { return m_runTabList; }
    quint32 runList() const;
    /** What is selected in the Show Tree (cue lists, cues, hits). */
    void setSelectedTargets(const QList<Target> &targets);
    QList<Target> selectedTargets() const { return m_selected; }

    /*********************************************************************
     * Input and feedback
     *********************************************************************/
public slots:
    /** One input value (InputOutputMap::inputValueChanged). Public for
     *  tests: it is what a control does. */
    void handleInput(quint32 universe, quint32 channel, uchar value);

public:
    /** The function whose running state a binding's light shows (the
     *  function; a hit's or cue's collection; a cue list's chaser).
     *  Invalid: none. */
    quint32 feedbackFunction(const Binding &b) const;
    /** Sends every binding's light as it is now (a controller plugged
     *  back in, a show loaded): every one, whether it changed or not. */
    void resendFeedback();

    /** What a control's light says now: that of the binding a press would
     *  fire (bindingOn()); none armed, that of one whose function runs (its
     *  press lets go); else dark. */
    Lamp controlLamp(quint32 universe, quint32 channel) const;

    /** What @a b's light says now (CS3). Dark when a press would do
     *  nothing: the target is gone, the press would be refused
     *  (surfaceMayFire(), the Q269 rule), a Stop-only on something idle, a
     *  cue list with nothing left to GO (ShowControl::standbyStep()), or
     *  the show is stopped (OutputRuleKeeper::emergencyStopped()). The
     *  emergency stop's own light is lit while the show is stopped. Never
     *  worked out here: each answer comes from the one place that owns it. */
    Lamp lamp(const Binding &b) const;
    /** A light in words, as the Control Map's table says it. */
    static QString lampName(Lamp lamp);
    /** The value a light is sent as, 0..255 (a MIDI velocity is half of
     *  it). @a dim false: a control that is ready stays dark, for a pad
     *  whose LEDs take a colour in the velocity and not a brightness. */
    static uchar lampValue(Lamp lamp, bool dim = true);
    /** Whether a control that is ready glows dim (the default) or stays
     *  dark. Saved with the show (Lights="onoff"). The emergency stop is
     *  never dimmed away by it: a panic button is always findable. */
    bool dimWhenReady() const { return m_dimWhenReady; }
    void setDimWhenReady(bool on);
    /** Test hook: how long each step of the held blink lasts. */
    void setBlinkMs(int ms) { m_blinkMs = ms; }
    /** Whether the held blink is running on @a b now. */
    bool isBlinking(const Binding &b) const;

signals:
    /** A binding was added, removed or changed. */
    void bindingsChanged();
    /** listen(): the control pressed. */
    void heard(quint32 universe, quint32 channel);
    /** listenForButton(): the first press was heard; press it again. */
    void confirming(quint32 universe, quint32 channel);
    /** A MIDI or OSC press was not carried out (not armed in Design, a hit's
     *  place not live), in words: the footer and the Control Map say so. */
    void blocked(const QString &why);
    /** listenForButton(): that control is not a button, in words. */
    void refused(const QString &why);
    /** A control fired its target: what it did, in words. */
    void fired(const QString &what);
    /** The stop's control was seen moving like a fader (or its state
     *  changed): the panel shows it. */
    void stopStateChanged();
    /** A control's first value after its device came back (a replug, a
     *  wake) on @a universe / @a channel was taken for the device's state
     *  dump and ignored (phase R): the stop's (@a what "STOP", at most one
     *  value per replug) or a trigger's or VC button's (@a what "GO of
     *  Main", said once per control and replug; isTriggerDump()).
     *  InputDeviceNotices says so. */
    void stateDumpIgnored(quint32 universe, quint32 channel, const QString &what);
    /** What isArmed() says may have changed (the mode, the Run tab's
     *  list, the Show Tree's selection, a cue list moving on). */
    void armingChanged();
    /** The Run tab's list changed (a pick, a show loaded or cleared). */
    void runTabListChanged(quint32 listId);
    /** A light may say something else now (CS3): the Control Map's table
     *  reads lamp() again. Not per frame: on a GO, a list stopped, a
     *  function started or stopped, the arming, the mode, the stop. */
    void lampsChanged();

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    bool saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &root);

private slots:
    void slotFunctionStarted(quint32 id);
    void slotFunctionStopped(quint32 id);
    void slotFunctionRemoved(quint32 id);
    /** A GO, a list stopped, a function started or stopped, the arming,
     *  the mode, an emergency stop struck or ended, a tree edit: asks for
     *  a light pass. One pass per turn of the event loop, however many
     *  events arrive in it -- a pass asks ShowControl for each cue-list
     *  binding's standby cue, and that takes the Chaser mutexes the
     *  MasterTimer thread holds while it steps. Queued twice over, in
     *  fact: the signals arrive on their own threads. */
    void slotLampsMayHaveChanged();
    /** The Show Tree changed: a trigger whose place went away is widened
     *  (fixPlaces()); not while a show loads (Doc::loaded does it then). */
    void slotTreeChanged();
    /** The double-GO guard held a command on cue list @a chaserId: every
     *  control that GOes it blinks twice (ShowControl::commandHeld). */
    void slotGuardHeld(const QString &text, quint32 chaserId);
    void slotInputDeviceConnected();
    /** A controller went away or came back: a press it held is let go. */
    void slotInputDeviceChanged(quint32 universe);
    /** A controller came back: what it sends first may be its state dump. */
    void slotInputDeviceBack(quint32 universe);

private:
    static quint64 key(quint32 universe, quint32 channel)
    { return (quint64(universe) << 32) | channel; }
    static quint64 targetKey(const Target &t) { return (quint64(t.type) << 32) | t.id; }
    static quint64 placeKey(const Place &p) { return (quint64(p.scope) << 32) | p.id; }
    /** A trigger whose place is gone (its cue, list or production
     *  deleted) moves out, as a scoped hit does: to its production, else
     *  everywhere (a place another trigger on the control has is passed
     *  over while a wider one is free; with none free it is held where it
     *  is, never armed, and not made to step through with that trigger).
     *  Said in placeNote(). A widened one
     *  whose place comes back (Undo) goes back to it. Marks the show
     *  modified when it moved one. */
    void fixPlaces();
    /** Whether a trigger @a b can ever work at @a place: it is in the show,
     *  and for a hit, not apart from where the hit lives now. */
    bool isPlaceUsable(const Binding &b, const Place &place) const;
    /** Remembers each live place's production and name, so a place that
     *  goes can still be widened and named. */
    void rememberPlaces();
    /** Rebuilds the lookups and (dis)connects input and feedback. */
    void changed(bool modified);
    void rebuild();
    void updateConnections();
    /** @return whether the press may start things (its release acts).
     *  @a placeLive false: its place is not live, it may only let go. */
    bool press(const Binding &b, bool placeLive = true);
    /** The binding a press of control @a k fires: the next of the armed
     *  ones; @a advance moves the step on. False: none is armed. */
    bool pick(quint64 k, Binding *out, bool advance) const;
    /** A binding on control @a k whose function runs (narrowest first):
     *  with none armed, a press of it still lets go. Null: none. */
    const Binding *runningOn(quint64 k) const;
    /** Whether binding @a b may have its control now: its place is live,
     *  and a hit may fire. */
    bool holdsControl(const Binding &b) const;
    /** A hit binding on control @a k whose place is live but which may not
     *  fire (its own place is not live). Null: none. */
    const Binding *refusedHitOn(quint64 k) const;
    /** Says why a press of control @a k fired nothing armed. */
    void sayNotLive(quint64 k);
    /** The cue list of cue @a cueId (a sub-cue's: its cue's). */
    quint32 listOfCueId(quint32 cueId) const;
    /** The binding whose light control @a k shows (null: dark). */
    const Binding *lampOwner(quint64 k) const;
    /** The emergency stop's binding when it is on control @a k. */
    const Binding *stopOn(quint64 k) const;
    /** Sends control @a k's light (dark when nothing is bound there now). */
    void sendControlLamp(quint32 universe, quint32 channel, bool force);
    void release(const Binding &b);
    /** The emergency stop: ALL OFF through ShowControl::run() (never held). */
    void emergencyStop(const Binding &b, uchar value);
    /** blocked(), said once per hold of control @a k */
    void say(quint64 k, const QString &why);
    /** listenForButton(): one control heard while learning. */
    bool learnButtonHeard(quint32 universe, quint32 channel, uchar value);
    /** Every value heard while learning: what learnSawFader() and
     *  learnSawLatching() read. */
    void observeLearn(quint64 k, uchar value);
    /** Starts, stops or toggles (start: 1, stop: 0, toggle: -1) a function. */
    void actOnFunction(quint32 fid, int how, const QString &what);
    void goList(const Binding &b, bool stop);
    /** The cue list @a b GOes: for a cue, its step and sub-cue in it
     *  (@a subCue -1: a main cue). Null: it has none (or a sub-cue the
     *  list no longer holds). What goList() sends and what lamp() reads,
     *  so they can never disagree. */
    Chaser *targetChaser(const Binding &b, int *step = nullptr, int *subCue = nullptr) const;
    /** Pushes @a b's light when lamp() says something else than was last
     *  sent to that control (always with @a force: a device plugged back
     *  in). A light nothing changed about sends nothing, so a mode switch
     *  with forty bindings does not flood a MIDI line. */
    void sendLamp(const Binding &b, bool force = false);
    void sendLampValue(const Binding &b, uchar value, bool force);
    /** Every binding's light (sendLamp() each). */
    void sendLamps(bool force);
    /** The light of a control that fires nothing any more: dark. */
    void darkenLamp(const Binding &b);
    /** One step of the held blink (on, off, on, off, then the truth). */
    void blinkStep();

private:
    Doc *m_doc;
    EncoderBank *m_encoderBank = nullptr;
    QList<Binding> m_bindings;
    /** control -> indexes in m_bindings (several: at different places, or
     *  stepping through at one) */
    QHash<quint64, QList<int>> m_byControl;
    /** Q303: per control, the armed ones it last stepped through and the
     *  next one's index; a change of what is armed starts again at the first */
    mutable QHash<quint64, QPair<QList<Target>, int>> m_step;
    /** controls held down (a press is an edge from 0) */
    QSet<quint64> m_down;
    /** the press a learn heard, until its release */
    QSet<quint64> m_swallow;
    /** Momentary functions a press started (the release stops only those) */
    QSet<quint32> m_heldStarted;
    /** presses that started something (their release lets go; a refused
     *  press has nothing to let go) */
    QSet<quint64> m_fired;
    bool m_listening = false;
    bool m_learnBind = false;
    bool m_learnButton = false;
    /** listenForButton(): the control pressed once, the value it sent, and
     *  whether it was let go since */
    quint64 m_pendingKey = 0;
    bool m_pending = false;
    bool m_pendingReleased = false;
    uchar m_pendingValue = 0;
    QElapsedTimer m_pendingAt;
    /** controls heard while learning a button that sent more than a button
     *  does: a fader stays refused even when its sweep passes through 0 */
    QSet<quint64> m_notButtons;
    /** how long a first press waits for its second */
    int m_confirmMs = 15000;
    /** What a control sent while learning (observeLearn()). */
    struct LearnSeen
    {
        uchar last = 0;
        bool any = false;
        QElapsedTimer at;
        bool fader = false;
        uchar on = 0;           //!< a latching run's one "on" value
        int latchSteps = 0;     //!< on, 0, on...: 3 is a latching button
        bool quickRelease = false;  //!< a release under m_latchHoldMs: momentary
    };
    QHash<quint64, LearnSeen> m_learnSeen;
    int m_latchGapMs = 400;
    /** a 0 this soon after the on is a press let go, not a latch's 0 */
    int m_latchHoldMs = 1500;
    /** repeats of the emergency stop's press inside this are one */
    int m_estopDebounceMs = 1000;
    /** What a stop's control last sent: the value, when, whether a 0 (a
     *  release) came since, and whether this run of values moves like a
     *  fader. See emergencyStop(). */
    struct StopHold
    {
        uchar value = 0;
        QElapsedTimer last;
        bool released = false;
        QElapsedTimer lastFired;    //!< the last ALL OFF this control sent (invalid: none)
    };
    QHash<quint64, StopHold> m_estopHeld;
    /** The state-dump guard (phase R): when each universe's device came
     *  back, the stop controls heard since, and those whose first value
     *  was 0 (a button at rest: the next value, if non-zero and inside the
     *  window, is the device's dump, not a press; ignored once, then the
     *  guard is off). */
    QHash<quint32, QElapsedTimer> m_backAt;
    QSet<quint64> m_backHeard;
    QSet<quint64> m_backAtRest;
    /** isTriggerDump(): the controls that sent a 0 since their device came
     *  back (their next press is a press), and those whose ignored dump
     *  was said. */
    QSet<quint64> m_backReleased;
    QSet<quint64> m_backSaid;
    /** slotInputDeviceChanged() letting go of held controls: its 0s are
     *  not the device's, and never arm a control (isTriggerDump()). */
    bool m_releasingLost = false;
    /** What a trigger dump on @a k is called: "GO of Main", "Back of Main",
     *  else targetName(). */
    QString dumpName(quint64 k) const;
    int m_dumpWindowMs = 500;
    /** Whether the stop's value @a value on @a k is part of a state dump
     *  (and is ignored). */
    bool isStateDump(quint64 k, uchar value);
    /** stop controls seen moving like a fader: shown loudly (the panel, one
     *  footer line) until the stop is bound again; never stops them firing */
    QSet<quint64> m_estopFlagged;
    /** a refused press said once per hold (and once a second) */
    QHash<quint64, QElapsedTimer> m_blockedHeld;
    /** when a press that taught a control was swallowed, and the controls
     *  that have sent a value since (a second one ends the swallow) */
    QHash<quint64, QElapsedTimer> m_swallowAt;
    QSet<quint64> m_swallowSeen;
    void endSwallow(quint64 k) { m_swallow.remove(k); m_swallowAt.remove(k); m_swallowSeen.remove(k); }
    /** the binding each held control pressed: its release acts on it, not
     *  on what the control is bound to by then */
    QHash<quint64, Binding> m_pressed;
    Binding m_learning;
    /** key bindings in m_bindings */
    int m_keyBindings = 0;
    quint32 m_runTabList = 0xFFFFFFFF;
    QList<Target> m_selected;
    bool m_inputConnected = false;
    bool m_feedbackConnected = false;
    /** The status sources behind the lights (CS3): ShowControl and the
     *  OutputRuleKeeper, connected only while there is a binding. */
    bool m_stateConnected = false;
    /** The last value sent to each control, so only a change is sent. */
    QHash<quint64, int> m_lampSent;
    /** A control that is ready glows dim; off for a pad whose LEDs take a
     *  colour in the velocity (the APCs) and show nothing at a dim level. */
    bool m_dimWhenReady = true;
    /** Each place a trigger has, while it is in the show: its production
     *  and its name (placeKey()). */
    struct PlaceHome
    {
        quint32 production = 0xFFFFFFFF;
        QString name;
    };
    QHash<quint64, PlaceHome> m_placeHomes;
    /** Triggers widened because their place went (targetKey()): where
     *  they were and what it was called. */
    struct Widened
    {
        Place from;
        QString fromName;
        Place to;
    };
    QHash<quint64, Widened> m_widened;
    /** The bindings blinking because the guard held a command on their
     *  cue list, how many steps of the blink are left (0: none), and
     *  whether a chain is in flight -- which the counter cannot say: it is
     *  0 through the last step's wait, and a hold landing there must not
     *  start a second chain on the same counter. */
    QList<Binding> m_blinking;
    int m_blinkLeft = 0;
    bool m_blinkRunning = false;
    int m_blinkMs = 150;
    /** A light pass is waiting: everything that can change a light (a
     *  function started or stopped, a GO, the arming, a tree edit) asks
     *  for one, and a whole batch of them is one pass. */
    bool m_lampsQueued = false;
};

/** @} */

#endif

/*
  Q Light Controller Plus
  focussession.h

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

#ifndef FOCUSSESSION_H
#define FOCUSSESSION_H

#include <QElapsedTimer>
#include <QObject>
#include <QMutex>
#include <QSet>
#include <QHash>
#include <functional>
#include <QList>
#include <QPair>
#include <QVector3D>
#include <QTimer>

#include "qlcengine_export.h"
#include "controllerscheme.h"
#include "gamepadrole.h"
#include "outputlayer.h"

class VenueCorrections;
class QTimer;
class Doc;
struct FixtureRigProps;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Focus mode: aligning the rig before a show.
 *
 * Steps through a list of moving heads one at a time. The current head is lit
 * and aimed at its REFERENCE -- the point it should hit, from a scene's Aim
 * look or a stage target -- through the aim solver, with its calibration
 * offsets as they stand right now. Nudging changes those offsets live, so the
 * real light moves while the rig views keep drawing the intended aim: for
 * the head being focused they draw intendedDirection(), not its DMX read
 * back. Saving makes a head's offsets the saved ones; reverting puts them
 * back.
 *
 * It is a TOOL, not an effect (FOCUS_MODE_DESIGN.md): it runs on the main
 * thread and its writes to the rig are ordinary edits there. It takes its
 * stick from the same path the follow spot does -- ProgrammerController's
 * bound axes, the HID profile's deadzone and sensitivity, its 50 Hz joystick
 * tick -- and owns the controller while it is on.
 *
 * Its light goes out through an OutputLayer at Universe::Focus, with
 * Override: for the heads it uses it is over whatever else is running
 * (only a park is above it; the Grand Master and Blackout apply), and it
 * lets go when it stops. (It used to write the universes from the
 * MasterTimer tick, where processFaders() zeroed its dimmers: B1.)
 */
class QLC_ENGINE_EXPORT FocusSession final : public QObject
{
    Q_OBJECT

public:
    /** What a head is aimed at. */
    struct Reference
    {
        enum Kind { None, SceneAim, Target };
        Kind kind = None;
        quint32 id = 0xFFFFFFFF;   ///< the scene, or the stage target
        bool isValid() const { return kind != None; }
    };

    /** What the heads in the list that are NOT being focused do. */
    enum Others { OthersDark, OthersDim, OthersAsTheyAre };

    /** Which way round the list is stepped through. */
    enum Order { ByStagePosition, AsGiven };

    /** One focus call, as it is run (FocusCalls makes these from the
     *  show's): these fixtures, to this target. FOCUS_REFINE_DESIGN.md. */
    struct Call
    {
        QString name;              ///< "DH Tspots -> DSC", for the panel
        QString who;               ///< "DH Tspots", for the voice
        QString targetName;        ///< "DSC", for the voice
        QList<quint32> fixtures;
        Reference target;
        QString note;
    };

    /** What the controller works on: the one fixture, or all of the
     *  current call (all of the list, without calls). Y switches. */
    enum Scope { FixtureScope, GroupScope };

    struct Options
    {
        QList<quint32> fixtures;
        /** Focus calls: when not empty, these are run call by call (and
         *  fixtures, first and second are not used). */
        QList<Call> calls;
        Order order = ByStagePosition;
        Reference first;
        Reference second;          ///< optional: Y toggles to it
        Others others = OthersDark;
        float level = 0.8f;        ///< the focused head's intensity, 0..1
        float dimLevel = 0.15f;    ///< the others', with OthersDim
        /** Run in Operate too (a focus-call cue in a running list), and
         *  going into Operate does not end it. */
        bool allowOperate = false;
        /** RT on the last call asks to leave (exitRequested()) instead of
         *  going round to the first: a focus-call cue goes on to the next
         *  cue when the calls are done. */
        bool endAfterLastCall = false;
    };

    /** A head's calibration offsets, degrees. */
    struct Offset
    {
        float pan = 0.0f;
        float tilt = 0.0f;
        bool operator==(const Offset &o) const { return pan == o.pan && tilt == o.tilt; }
        bool operator!=(const Offset &o) const { return !(*this == o); }
    };

    /** What a press does. */
    enum Control
    {
        NoControl,
        Next, Previous,
        Save, Revert, Zero,
        ToggleReference,
        UndoSave,
        Exit,
        StepPanUp, StepPanDown, StepTiltUp, StepTiltDown,
        ZoomWider, ZoomNarrower, FocusNearer, FocusFarther,
        PreviousCall, NextCall,
        CycleScope,
        WhereAmI
    };

    /** The stick a reading came from. */
    enum Stick { Coarse, Fine };

    /** What drives the nudges. The keyboard always works; with
     *  KeyboardInput the controller is ignored altogether, so a resting
     *  controller cannot drift a head. */
    enum InputDevice { ControllerInput, KeyboardInput };

    FocusSession(Doc *doc, QObject *parent = nullptr);
    ~FocusSession() override;

    /*********************************************************************
     * Starting and stopping
     *********************************************************************/
public:
    /** Start on @p options. Refuses (false, with @p why) in Operate mode,
     *  when nothing in the list can be focused, or when none of it has a
     *  reference to aim at. Restarting replaces a running session after
     *  discarding its unsaved offsets. */
    bool start(const Options &options, QString *why = nullptr);
    /** Asked before a start: false, with the reason, while another
     *  controller tool -- or this one -- has unsaved work a start would
     *  throw away (gap 5: never silently discarded). */
    void setStartGuard(const std::function<bool(QString *)> &guard) { m_startGuard = guard; }

    /** Stop, discarding any unsaved offsets (saveAll() first to keep them).
     *  Lets go of the heads. */
    void stop();
    /** The operator leaves (Menu, the panel's Exit, RT past the last
     *  call): stop(), with endedByOperator() true while stopped() is
     *  emitted. Anything else that stops it is "from outside". */
    void finish();
    bool endedByOperator() const { return m_endedByOperator; }

    bool isActive() const { return m_active; }
    const Options &options() const { return m_options; }

    /** The heads @p fixtures holds that Focus mode can work: moving heads
     *  (they can pan or tilt) and fixed lights (see fixedLights()), placed
     *  on the plot. In their order, once each. */
    static QList<quint32> focusable(Doc *doc, const QList<quint32> &fixtures);
    /** The moving heads among focusable(). */
    static QList<quint32> movingHeads(Doc *doc, const QList<quint32> &fixtures);
    /** The FIXED lights among them (board Q102): one head, no pan or tilt,
     *  a light, placed on the plot. Focus mode moves where such a light lands
     *  (at head height) and sets its Pan and Tilt (FixtureRigProps::fixedAim)
     *  from it -- matching the plot to a light focused by hand. */
    static QList<quint32> fixedLights(Doc *doc, const QList<quint32> &fixtures);
    /** How high above the deck a fixed light's landing point is: head height. */
    static constexpr float subjectHeight() { return 1.6f; }
    /** How far a fixed light's landing point moves per degree of nudge
     *  (arrow keys, D-pad, sticks), metres. */
    static constexpr float metresPerDegree() { return 0.5f; }
    /** True when @p fid is a fixed light in this session. */
    bool isFixedLight(quint32 fid) const { return m_heads.value(fid).fixed; }

    /** The beam (board Q103). A light zoomed BY HAND (a lens range in its
     *  definition, no zoom channel) is set once: its FixtureRigProps::lensDeg,
     *  saved and reverted with the rest. A light with a zoom channel is
     *  zoomed live through it while focused, to see the size -- not saved
     *  (cues own the zoom). Either way held to the definition's range. */
    enum BeamKind { NoBeamControl, LensSetByHand, ZoomChannel };
    BeamKind beamKind(quint32 fid) const;
    /** @p fid's beam angle now and the range it may take, degrees. */
    bool beamAngle(quint32 fid, double &deg, double &lo, double &hi) const;
    /** Widen (+) or narrow (-) the current head's beam by @p dDeg. */
    void nudgeBeam(float dDeg);
    /** The triggers, 0 (released) .. 1, read as buttons: LT the previous
     *  focus call, RT the next (ControllerScheme). */
    void setTrigger(int side, float norm);

    /** Focus (the beam's edge), where a light has a focus channel: moved
     *  while focusing, to see the edge, not saved (cues own it). */
    bool hasFocusControl(quint32 fid) const;
    /** The current head's focus, 0..1 (as its channel runs); false if none. */
    bool focusLevel(quint32 fid, double &level) const;
    /** Move the current head's focus by @p d (0..1 scale). */
    void nudgeFocus(float d);

    /** @p fixtures front to back (downstage first), then house left to right
     *  within a row -- rows are heads within 0.75 m of the row's first. */
    static QList<quint32> stageOrder(Doc *doc, const QList<quint32> &fixtures);

    /*********************************************************************
     * Where it is
     *********************************************************************/
public:
    QList<quint32> order() const { return m_order; }
    int count() const { return m_order.size(); }
    int index() const { return m_index; }
    /** The head being focused, or 0xFFFFFFFF when not active. */
    quint32 currentFixture() const;

    void next();
    void previous();
    void setIndex(int index);

    /*********************************************************************
     * Focus calls and scope
     *********************************************************************/
public:
    /** Running the show's focus calls (Options::calls), not a list. */
    bool isRunningCalls() const { return !m_calls.isEmpty(); }
    int callCount() const { return m_calls.size(); }
    int callIndex() const { return m_callIndex; }
    /** The call being focused (an empty one without calls). */
    Call currentCall() const;
    void nextCall();
    void previousCall();
    void setCallIndex(int index);
    /** Every head in the session, all calls, each once. */
    QList<quint32> allFixtures() const { return m_all; }

    Scope scope() const { return m_scope; }
    void setScope(Scope scope);
    void cycleScope();
    /** What the controller works on now: the current fixture, or all of the
     *  current call (list). */
    QList<quint32> scopeFixtures() const;

    /*********************************************************************
     * Telling the operator where they are (FOCUS_REFINE_DESIGN.md s.3)
     *********************************************************************/
public:
    /** What is said on stepping to a call, a fixture or a scope, and for
     *  Where am I? -- the phrases the design lists. */
    QString wherePhrase() const;
    /** "Say the light" (RS): the light's name first, then where it is --
     *  "Spot 3. Call 2 of 5, AT spots to DSC. Fixture 3 of 8." -- said
     *  (sayLight(), whatever the feedback setting: it was asked for) and
     *  what is selected flashed. */
    void whereAmI();
    QString lightPhrase() const;
    /** The rig blinks (Feedback: Lights or Both); off: Voice only. */
    void setLightsFeedback(bool on) { m_lightsFeedback = on; }

    /** The flash that shows what is selected: three blinks, flashOnMs()
     *  lit and flashOffMs() dark, then steady. */
    static constexpr int flashOnMs() { return 120; }
    static constexpr int flashOffMs() { return 120; }
    static constexpr int flashBlinks() { return 3; }
    static constexpr int flashDurationMs() { return flashBlinks() * (flashOnMs() + flashOffMs()); }
    /** Lit @p ms into a flash (steady once it is over). */
    static bool flashLit(qint64 ms);
    /** Something important (a warning, a refusal, the controller lost --
     *  board Q131): one slow, long blink -- dark for warnDarkMs(), then
     *  lit -- unlike the selection's three quick ones and a save's double. */
    static constexpr int warnDarkMs() { return 800; }
    static bool warnLit(qint64 ms) { return ms < 0 || ms >= warnDarkMs(); }
    /** The selection's slow long blink (lights feedback only). */
    void flashWarning();
    bool isWarningFlash() const { return m_warnFlash && isFlashing(); }
    /** Start the flash on what is selected now. */
    void flashSelection();
    bool isFlashing() const;
    /** Flash the selection on a change (on; a test of steady output turns
     *  it off). */
    void setFlashEnabled(bool on) { m_flashEnabled = on; }

    /*********************************************************************
     * Venue (board Q105)
     *********************************************************************/
public:
    /** Where saves go: with a venue applied, to its corrections (the sidecar
     *  file), never the show; without one, the show's calibration. */
    void setVenues(VenueCorrections *venues) { m_venues = venues; }
    VenueCorrections *venues() const { return m_venues; }
    /** True when saves go to a venue rather than the show. */
    bool savesToVenue() const;

    /** True while the second reference is the one aimed at. */
    bool usingSecondReference() const { return m_useSecond; }
    /** Switch between the two references; false (and no change) when there
     *  is no second one for the current head. */
    bool toggleReference();
    /** The point head @p fid is aimed at now (current reference). */
    bool referencePoint(quint32 fid, QVector3D &point) const;
    /** A short name for the reference in use, for the HUD. */
    QString referenceName() const;

    /*********************************************************************
     * What the views draw
     *********************************************************************/
public:
    /** Where head @p fid is MEANT to point while the session aims it -- the
     *  current head, at its reference -- as AimSolver::intendedAngles() gives
     *  a direction: pan and tilt relative to home, calibration and invert
     *  taken out.
     *
     *  The rig views draw this instead of turning the head's DMX back into a
     *  direction. The DMX carries the live, unsaved nudge and is rounded to
     *  whole DMX steps and a frame behind the rig props, so a beam read back
     *  from it crept and jumped with every nudge while the real head moved
     *  smoothly. This is solved with no offset at all, so the drawn beam stays
     *  on the reference however far the head is nudged, before and after
     *  Save, Revert, Zero, Undo or a discard.
     *
     *  False for any other head, or when the session is not running. */
    bool intendedAngles(quint32 fid, double &panRelDeg, double &tiltRelDeg) const;
    /** The same as a world direction (AimSolver::beamDirection()). */
    bool intendedDirection(quint32 fid, QVector3D &dir) const;

    /*********************************************************************
     * Offsets
     *********************************************************************/
public:
    Offset liveOffset(quint32 fid) const;
    Offset savedOffset(quint32 fid) const;
    bool isDirty(quint32 fid) const { return m_heads.contains(fid) && m_heads.value(fid).dirty(); }
    bool hasUnsaved() const;
    QList<quint32> unsavedFixtures() const;
    /** Saved at least once this session. */
    bool isChecked(quint32 fid) const { return m_checked.contains(fid); }
    int checkedCount() const { return m_checked.size(); }

    /** Add to the current head's live offsets (degrees). */
    void nudge(float dPan, float dTilt);
    /** The step the D-pad and arrow keys take, degrees. */
    float stepDegrees() const { return m_step; }
    void setStepDegrees(float step);

    /** The current head: live becomes saved -- one undo step. Marks the show
     *  modified. */
    void save();
    /** The current head back to its saved offsets. */
    void revert();
    /** The current head's live offsets to zero (unsaved until save()). */
    void zero();
    /** Undo the last save(): that head's saved and live offsets go back to
     *  what they were before it. False when there is nothing to undo. */
    bool undoSave();
    bool canUndo() const { return !m_undo.isEmpty(); }

    /** The save confirmation on the rig: after save() keeps a head, that
     *  real fixture dips to dark and back twice (kBlinkPhases phases of
     *  kBlinkPhaseMs each: dark, lit, dark, lit), so whoever is on stage
     *  sees it was kept. Revert, Zero and Undo do not blink. Off (no
     *  blink) with setRigFeedback(false). */
    static constexpr int kBlinkPhaseMs = 150;
    static constexpr int kBlinkPhases = 4;
    /** True while a save's blink is running; the phase is 1..kBlinkPhases
     *  (odd phases are dark), 0 when none is. */
    bool isBlinking() const { return m_blinkPhase > 0; }
    int blinkPhase() const { return m_blinkPhase; }
    quint32 blinkFixture() const { return m_blinkFid; }
    /** The blink's next phase. Its own timer calls this every kBlinkPhaseMs;
     *  a test calls it itself, after setBlinkTimed(false). */
    void advanceBlink();
    void setBlinkTimed(bool timed) { m_blinkTimed = timed; if (!timed) m_blinkTimer.stop(); }

    /** Whether the rig itself says a head was saved (the blink). On by
     *  default; a feedback setting may turn it off. */
    bool rigFeedback() const { return m_rigFeedback; }
    void setRigFeedback(bool on);

    /** Save every head with unsaved offsets (one undo step each). */
    void saveAll();

    /*********************************************************************
     * Where things stand (board Q126)
     *********************************************************************/
public:
    /** The status lamp's (and the pad's light bar's) colour. */
    enum SaveStatus { AllSaved, ThisUnsaved, OthersUnsaved, BothUnsaved };
    SaveStatus saveStatus() const;

    /** Hold A: every unsaved head saved, said ("3 lights kept."). */
    void saveAllHeld();
    /** Hold B: the light being worked back to its last save, whatever the
     *  scope, said ("Spot 3 back to its last save."). */
    void revertLight();
    /** Both bumpers: on to the next head with unsaved changes, in any call
     *  ("Spot 7, not saved."); "All saved." when there is none. From the
     *  head @p after (invalid: the one being worked) -- the head the first
     *  bumper of the pair stepped from. */
    bool nextUnsaved(quint32 after = 0xFFFFFFFF);

    /** Where head @p fid would land with its SAVED calibration, as its
     *  live one lands on the reference (the plot's ghost while nudging);
     *  false when they are the same, or for another head. */
    bool savedGhost(quint32 fid, QVector3D &point) const;
    /** What has changed since the save: "+2.5° pan, −1.0° tilt", or for a
     *  fixed light "0.50 m stage left, 0.25 m upstage"; empty: nothing. */
    QString deltaText(quint32 fid) const;
    /** Put every head back to its saved offsets. */
    void discardAll();

    /*********************************************************************
     * Controller
     *********************************************************************/
public:
    /** What a gamepad button does here (None for anything Focus mode does not
     *  use). Sticks are not controls: see setStick(). */
    static Control controlFor(GamepadRole::Role role);
    /** Do @p control. Exit only asks (exitRequested()), since leaving with
     *  unsaved offsets needs an answer. */
    void apply(Control control);

    /** A controller button went down (@p down) or up. The beam and focus
     *  arms of the D-pad repeat while held: once on the press, again after
     *  kRepeatDelay, then every kRepeatInterval (tick() does the repeats).
     *  Anything else is apply()ed once on the press. */
    void press(Control control, bool down);
    static bool repeats(Control control);
    static constexpr double kRepeatDelay = ControllerScheme::HoldRepeat::delay();
    static constexpr double kRepeatInterval = ControllerScheme::HoldRepeat::interval();

    /** The follow spot's stick response (followspot.js axisVelocity()): a
     *  normalised axis reading @p norm (0..1, 0.5 centred) as a velocity,
     *  -1..1, nothing inside @p deadzone and linear past it. */
    static float stickVelocity(float norm, float deadzone);

    /** The latest reading of a stick, normalised as the controller sends it
     *  (0..1, 0.5 centred; Y 0 = pushed up). */
    void setStick(Stick stick, float xNorm, float yNorm);
    /** The profile's deadzone and sensitivity, as the follow spot gets them. */
    void setStickResponse(float deadzone, float sensitivity);
    /** Integrate the sticks over @p dt seconds: full deflection is
     *  coarseRate() (left) or fineRate() (right) degrees per second, times
     *  the sensitivity. Called from the programmer's joystick tick. */
    void tick(double dt);

    /** Which device drives the nudges (see InputDevice). Switching to the
     *  keyboard centres the sticks. Kept across sessions. */
    InputDevice inputDevice() const { return m_input; }
    void setInputDevice(InputDevice device);
    /** The controller went away (a pad asleep, unplugged): whatever it
     *  held is let go -- both sticks centred, a held D-pad arm and the
     *  triggers released -- so nothing moves until it is touched again. */
    void releaseControls();

    /** Board Q128, the target page (focus calls; LS click, or T): the
     *  sticks move the TARGET of the current call on the floor and every
     *  head aimed at it follows, like an Eos focus palette. A keeps the
     *  target -- into the venue applied (its sidecar), else the show -- B
     *  puts it back. Leaving the page, or stepping calls, with a move not
     *  kept is refused. False (said) when the call has no target. */
    bool toggleTargetPage();
    bool isTargetPage() const { return m_targetPage; }
    /** The target moved and not yet kept. */
    bool targetMoved() const;
    /** Move the call's target @p dx, @p dy metres (on the target page). */
    void nudgeTarget(float dx, float dy);
    bool keepTarget();
    void revertTarget();
    /** A stick or key "degree" on the target page, in metres. */
    static constexpr float targetMetresPerDegree() { return 0.1f; }

    static constexpr float coarseRate() { return 8.0f; }
    static constexpr float fineRate() { return 1.0f; }

    /*********************************************************************
     * DMX
     *********************************************************************/
public:
    /** What it sends, (universe, absolute address) -> value: the focused
     *  head's aim and light, the others' level. For a test or a readout. */
    QHash<QPair<int, int>, uchar> pendingWrites() const;

signals:
    void started();
    void stopped();
    void currentChanged(quint32 fid);
    /** A head's live or saved offsets changed. */
    void offsetChanged(quint32 fid);
    void referenceChanged();
    /** Which heads have unsaved changes changed (unsavedFixtures()). */
    void unsavedChanged();
    /** save() kept head @p fid (not saveAll()). */
    void headSaved(quint32 fid);
    /** Exit was pressed: the UI asks about unsaved offsets, then stop()s. */
    void exitRequested();
    void inputDeviceChanged(FocusSession::InputDevice device);
    /** Stepped to another focus call. */
    void callChanged(int index);
    void scopeChanged(FocusSession::Scope scope);
    /** Something to say to the operator on stage (the voice reads it). */
    void announce(const QString &text, int priority = ControllerScheme::WherePriority);
    /** Said because the operator asked (Say the light): always spoken. */
    void sayLight(const QString &text);

private:
    struct Head
    {
        QVector3D first;
        bool hasFirst = false;
        QVector3D second;
        bool hasSecond = false;
        Offset saved;
        Offset live;
        /* A fixed light: where it lands (first is the live point), as
           saved, and how its rig aimed it before the session. */
        bool fixed = false;
        QVector3D landSaved;
        bool savedIsOriginal = true;
        bool origAim = false;
        float origPan = 0.0f, origDown = 90.0f;
        /* The beam: a lens set by hand (saved), or a zoom channel's angle
           while focused (live only; 0 = not touched). */
        int beam = 0;                 ///< BeamKind
        float lensSaved = 0.0f, lensLive = 0.0f;
        double zoomDeg = 0.0;
        double focus = -1.0;          ///< a focus channel's level while focused; -1 = not touched
        bool dirty() const { return (fixed ? (first != landSaved) : (live != saved))
                                    || (beam == 1 && lensLive != lensSaved); }
    };
    struct UndoEntry
    {
        quint32 fid;
        Offset before;
        bool wasChecked;
        QVector3D landBefore;
        bool wasOriginal = false;
        float lensBefore = 0.0f;
        int batch = 0;            ///< entries saved together undo together
    };
    /** A call as it runs: its heads in order and the point each focuses to. */
    struct CallState
    {
        Call call;
        QList<quint32> order;
        QHash<quint32, QVector3D> points;
    };
    void enterCall(int index);
    void nudgeHead(quint32 fid, float dPan, float dTilt);
    void nudgeBeamOf(quint32 fid, float dDeg);
    void nudgeFocusOf(quint32 fid, float d);
    /** Make @p fid's live state its saved one (an undo entry in @p batch). */
    bool saveHead(quint32 fid, int batch);
    /** A head's saved state has changed: into the venue, or the show. */
    void savedChanged(const QList<quint32> &fids);
    void onFlashTick();
    /** Write a lens-set-by-hand angle into the rig. */
    void applyLens(quint32 fid, float deg);
    /** A fixed light's landing point from its rig, at head height. */
    QVector3D fixedLanding(quint32 fid) const;
    /** Point a fixed light's rig at @p landing (or back to how it was). */
    void applyLanding(quint32 fid, const Head &h, bool original);
    /** A head back to what is saved for it (offsets, or where it lands). */
    void backToSaved(quint32 fid, Head &h);

    void resolveReferences();
    /** The calibration-free aim of @p fid, centred fixture degrees as DMX
     *  would carry them, and its rig props with the offsets zeroed to read
     *  them with. */
    bool intendedCentred(quint32 fid, FixtureRigProps &rp,
                         double &panCentredDeg, double &tiltCentredDeg) const;
    QHash<quint32, QVector3D> resolve(const Reference &ref) const;
    /** Write @p offset into the rig for @p fid (the solver and the views
     *  read it from there). */
    void applyOffset(quint32 fid, const Offset &offset);
    void rebuildWrites();
    void markModified();
    /** emit unsavedChanged() when unsavedFixtures() is not what it was. */
    void checkUnsaved();
    void stopBlink();

private:
    Doc *m_doc;
    bool m_active = false;
    Options m_options;
    QList<quint32> m_order;
    int m_index = -1;
    bool m_useSecond = false;
    QHash<quint32, Head> m_heads;
    QSet<quint32> m_checked;
    QList<UndoEntry> m_undo;
    float m_step = 0.1f;
    InputDevice m_input = ControllerInput;
    ControllerScheme::TriggerButton m_trigger[2];
    /** The held D-pad arm that repeats (ControllerScheme::HoldRepeat, as
     *  Refine's): its key is the Control. */
    ControllerScheme::HoldRepeat m_hold;

    QList<CallState> m_calls;
    int m_callIndex = -1;
    QList<quint32> m_all;
    Scope m_scope = FixtureScope;
    int m_batch = 0;
    VenueCorrections *m_venues = nullptr;
    QTimer *m_flashTimer = nullptr;
    bool m_flashEnabled = true;
    bool m_warnFlash = false;
    std::function<bool(QString *)> m_startGuard;
    bool m_endedByOperator = false;
    bool m_targetPage = false;
    quint32 m_targetId = 0xFFFFFFFF;
    QVector3D m_targetKept;
    /** Re-aim the current call's heads at its target where it is now. */
    void reresolveCall();
    /** On the target page with a move not kept: said, true (refused). */
    bool targetBlocks();
    bool m_lightsFeedback = true;
    QElapsedTimer m_flashClock;

    float m_stick[2][2] = { { 0.5f, 0.5f }, { 0.5f, 0.5f } };
    float m_deadzone = 0.05f;
    float m_sensitivity = 1.0f;
    float m_carryPan = 0.0f;    ///< stick travel below a hundredth, kept
    float m_carryTilt = 0.0f;

    /** The unsaved heads, as last told (unsavedChanged()). */
    QList<quint32> m_lastUnsaved;
    /** The save's blink (advanceBlink()). */
    bool m_rigFeedback = true;
    bool m_blinkTimed = true;
    int m_blinkPhase = 0;
    quint32 m_blinkFid = 0xFFFFFFFF;
    QTimer m_blinkTimer;

    mutable QMutex m_writesMutex;
    QHash<QPair<int, int>, uchar> m_writes;
    /** The Focus fader on each universe */
    OutputLayer m_layer;
};

/** @} */

#endif

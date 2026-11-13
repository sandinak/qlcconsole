/*
  Q Light Controller Plus - Unit test
  markplanner_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef MARKPLANNER_TEST_H
#define MARKPLANNER_TEST_H

#include <QObject>

class Doc;
class MarkEffect;
class MarkPlanner;

class MarkPlanner_Test : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** A marked fixture that the upcoming cue also lights is not dangling. */
    void markedFixtureInUpcomingCueIsNotDangling();
    /** A marked fixture the upcoming cue never lights is dangling. */
    void markedFixtureNotInUpcomingCueIsDangling();
    /** Nothing marked -> never dangling, regardless of upcoming cues. */
    void noMarksIsNeverDangling();
    /** Marked, but nothing is currently driving cues -> dangling (no cue
     *  is ever going to reveal it). */
    void noUpcomingCueMeansMarkedIsDangling();
    /** dangleFixturesChanged() fires on change, not on every tick. */
    void signalFiresOnlyOnChange();
    void aDarkFixtureIsPresetButAStagedOneIsNot();
    /** Through the show's ProgrammerController: one switch + lead time,
     *  every change signalled (the lead time too, so every view follows),
     *  saved and loaded with the show, and a new show starts it off. */
    void theShowsMoveInBlackSavesLoadsAndResets();
    /** "Move in black no longer carries over to the next show": after a
     *  show with it on is opened, opening one saved without the setting
     *  starts it off, at the default lead time. */
    void openingAShowWithoutMoveInBlackStartsItOff();
    /** The "pre-positioning N heads" count follows the planner's pre-sets. */
    void thePresetCountFollowsThePlanner();
    /** Q38: a Show Tree list set On pre-positions with the show's setting
     *  off; a list set Off, or a cue flagged "Don't move in black into this
     *  cue", never does (and releases what was pre-set). */
    void perListAndPerCueMoveInBlack();
    /** The lead time: a cue firing sooner than it is not moved into in
     *  black, one firing at least that far off is, and a pre-set already
     *  made is left be when the cue draws nearer. */
    void aCueTooSoonIsNotMovedInBlack();
    /** CueLookahead (coverage campaign): a running Show timeline is asked
     *  first, its nearest item on an unmuted track after the playhead,
     *  within the horizon */
    void theLookaheadAsksARunningShowFirst();
    /** A chaser step that holds for GO is always ahead; a show-clocked
     *  chaser is the Show's to answer for; past the horizon is nothing */
    void theLookaheadOfAChaser();
    /** M992: a Show Tree cue that waits for GO holds for
     *  Function::infiniteSpeed(); its next cue is ahead without end
     *  (INT_MAX), not "now", so a dark head is moved in black at the
     *  show's own lead time. A timed hold longer than INT_MAX ms does not
     *  wrap round to "now" either. */
    void aCueThatWaitsForGoIsMovedInBlack();
    /** UX-73: a dark WLED's next look -- its effect, speed, palette and
     *  colours, which come from the WLED look, not the scene -- is moved
     *  in black like a mover's position, so the GO sends its brightness
     *  alone. The looks are kept from tick to tick and let go of. */
    void aDarkWledIsMovedInBlackToItsNextLook();
    /** A cue jumped past: what was moved in black for it goes back. */
    void aPresetIsPutBackWhenItsCueIsJumpedPast();

    /* A shutter is a dimmer too (2026-11-12) */
    /** "Closed" is the definition's close capabilities (a ShutterClose
     *  preset, or one named so), never a strobe or a "No function". */
    void closedIsTheDefinitionsCloseCapability();
    /** The dimmer down keeps it dark whatever the shutter: pre-set as
     *  before, the next cue's shutter (open) with the rest -- the dimmer
     *  is what keeps it dark, the GO brings the level. */
    void aDarkDimmerPresetsTheShutterToo();
    /** The dimmer up behind a closed shutter is dark: pre-set, the shutter
     *  left closed (not held open), not handed off while it stays closed,
     *  handed off when the cue opens it. */
    void aClosedShutterIsDarkAndStaysClosed();
    /** A fixture with a shutter and no dimmer: pre-set, the shutter left
     *  closed, released on reveal (the shutter opens). */
    void aShutterOnlyFixtureIsMovedInBlack();
    /** No dimmer, and no shutter that can close (a strobe channel): never
     *  pre-set, and never lit by a cue as far as the planner can tell. */
    void aFixtureWithNeitherIsSkipped();
    /** A shutter auto-open opens (the dimmer up, nothing sets the shutter)
     *  is open on stage: not dark, so not pre-set. */
    void anAutoOpenedShutterIsNotDark();
    /** A manual mark on a fixture dark by its shutter is not the planner's. */
    void aManualMarkIsLeftAlone();
    /** A next cue that closes the shutter does not light the fixture: not
     *  pre-set, and a mark on it dangles. */
    void aCueThatClosesTheShutterDoesNotLightIt();

private:
    Doc *m_doc = nullptr;
    MarkEffect *m_mark = nullptr;
    quint32 m_fixtureA = 0;   //!< will be lit by the upcoming cue
    quint32 m_fixtureB = 0;   //!< will NOT be lit by the upcoming cue

    /** Adds a fixture: Pan, then a Dimmer (@p dimmer), then a Shutter
     *  (closed 0-7, open 8-255) when @p shutter, else a strobe-only
     *  Shutter channel. Returns its id; its channel numbers in order. */
    quint32 addShutterFixture(quint32 address, bool dimmer, bool shutter);
};

#endif // MARKPLANNER_TEST_H

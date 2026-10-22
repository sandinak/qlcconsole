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
    /** The "pre-positioning N heads" count follows the planner's pre-sets. */
    void thePresetCountFollowsThePlanner();
    /** Q38: a Show Tree list set On pre-positions with the show's setting
     *  off; a list set Off, or a cue flagged "Don't move in black into this
     *  cue", never does (and releases what was pre-set). */
    void perListAndPerCueMoveInBlack();

private:
    Doc *m_doc = nullptr;
    MarkEffect *m_mark = nullptr;
    quint32 m_fixtureA = 0;   //!< will be lit by the upcoming cue
    quint32 m_fixtureB = 0;   //!< will NOT be lit by the upcoming cue
};

#endif // MARKPLANNER_TEST_H

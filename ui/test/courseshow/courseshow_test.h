/*
  Q Light Controller Plus
  courseshow_test.h

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

#ifndef COURSESHOW_TEST_H
#define COURSESHOW_TEST_H

#include <QObject>
#include <QString>

class App;
class ShowTreeView;

/* The Show Tree course (docs/course): its teaching show opens on a student's
   computer as it should, and every step of a lesson does what the lesson
   says -- the mouse steps and the typed ones. Also builds the course's show
   files (COURSE_RIG) and takes the lesson pictures (COURSE_SHOTS). */
class CourseShow_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** COURSE_RIG=<rig.qxw from derive-classroom-rig.py>: opens it, adds
     *  the base palettes with the Show Tree's command line, and saves
     *  ShowChoir-Classroom.qlcc and Lesson 1's start and finish (Lesson 1
     *  done with the mouse, as the handout says). Skipped otherwise. */
    void buildTheShows();

    /** The teaching show: no universe goes anywhere (it can't send DMX),
     *  every fixture has its definition from the show itself, and the
     *  groups, palettes and targets the lessons name are there. */
    void theTeachingShowIsSafeAndComplete();

    /** Lesson 1 with the mouse, step by step, from the start file: each
     *  step's result is what the handout's "you should see" says, and the
     *  end is the finish file. */
    void lessonOneWithTheMouse();

    /** Lesson 1's extension 1: a grey (tracked) row set in this cue with
     *  ⋯ → Set in this cue, by the mouse. */
    void lessonOneSetInThisCue();

    /** Lesson 1's extension 2, typed on the command line: the same three cues. */
    void lessonOneTyped();

    /** The finish file plays: GO in the Run tab steps through the three cues,
     *  and the rig shows what each cue says. */
    void lessonOnePlaysInTheRunTab();

    /** Lessons 1.2, 1.3 and 1.4 with the mouse, step by step, each from
     *  its start file: each step checks the handout's "you should see",
     *  each ends as its finish file, and each starts where the one before
     *  finished. */
    void lessonsOneTwoToOneFourWithTheMouse();

    /** Lesson 1.2's extension: ⋯ → Colour… and a colour picked: a one-off
     *  value with Make reusable beside it. */
    void lessonTwoColourPicker();

    /** Lesson 1.3's extension: ⋯ → Set to → Aim Center on the spots'
     *  Position row (and Undo), and a blue back light in the Solo. */
    void lessonThreeExtension();

    /** Lesson 0.8: the Rig Overview's views and room light, as the handout
     *  names them, and GO three times on Lesson 1.1's finish. */
    void lessonZeroEightTheVirtualStage();

    /** Lesson 1.4's fades and hold play in the Run tab: the Pose fades in
     *  over 3 s, the Scream holds 2 s and the Solo follows by itself. */
    void lessonFourPlaysTheFades();

    /** What a student builds is what they keep: Lessons 1.1-1.4 done with
     *  the mouse, saved as a .qlcc (Save As), opened again: every cue the
     *  same, and GO in the Run tab still lights the rig as Lesson 1.1 says. */
    void aMouseBuiltShowSavesOpensAndPlays();

    /** Lesson 1.5, running the show: GO, Back, LOAD (a click: STANDBY),
     *  a jump (double-click) and Stop, in the Run tab. */
    void lessonFiveRunsTheShow();

    /** Lesson 1.5's show time: Operate (LIVE), where a click in the Show
     *  Tree doesn't change the stage; Stop, then Design again. */
    void lessonFiveOperate();

    /** Lesson 1.5's late-cue extension: + Cue after the Chorus is the
     *  point cue 1.0.2.5 Hit; dropped after the Solo it is 1.0.5 at the
     *  end (the drop note says so first); Undo puts it back. */
    void lessonFiveLateCue();

    /** Lesson 3.3, stage looks: the plot's Show menu draws Auto fixtures as
     *  their closest look, and Plot Symbol ▸ Practicals ▸ Lantern on the
     *  Lanterns group, as the handout says. */
    void lessonStageLooks();

    /** Lesson 3.7, house and work lights and a power relay: three generic
     *  dimmers given output rules stay through Blackout and the Grand
     *  Master, and the relay comes on at Operate. */
    void lessonHouseLights();

    /** Lesson 3.6, focus and calibration, on the classroom show saved as a
     *  student's own: the stage marks and where Soloist DS and Center read,
     *  three focus calls and a venue, the calls run on the keys (a nudge, a
     *  save for the venue, the whole call, the next call, spot down), the
     *  scramble practice focused back and scored, the focus report written,
     *  and Refine on a typed solo cue (spot down kept in the cue; one spot
     *  moved as an exception). With COURSE_SHOTS, the lesson's pictures. */
    void lessonFocusAndCalibration();

    /** Lesson 3.10, from Eos: the course's Eos sample through the import
     *  wizard's special circuits, into a new show: groups, rules, the cue
     *  list in the Show Tree and the sub as a Virtual Console fader. */
    void lessonFromEos();

    /** Lesson 1.5's "Stuck?": Design while cues run asks "Really stop
     *  them...?" -- No stays LIVE with the stage lit; Yes stops everything
     *  and goes to Design with the stage dark. */
    void lessonFiveDesignWhileRunningAsksFirst();

    /** COURSE_BUILD=1: makes Lessons 1.2-1.5's start and finish files, each
     *  lesson done with the mouse from the one before's finish. Skipped
     *  otherwise. */
    void buildLessonShows();

    /** COURSE_SHOTS=<folder>: the annotated pictures for the lesson and the
     *  Rig Overview of each cue. Skipped otherwise. */
    void lessonPictures();

    /** Level 2 with the mouse, lesson by lesson, each from its start file
     *  (2.1's is Lesson 1.4's finish): each step checks the handout's "you
     *  should see", and each ends as its finish file. */
    void levelTwoWithTheMouse();

    /** Lesson 2.1's extension: the Pose as the base cue, with dim blue step
     *  tops: the chorus's released step tops go back to them. */
    void lessonTwoOneExtension();

    /** Lesson 2.2's extension: the Library's palettes with Group by type
     *  off (the song's folder first) and on again. */
    void lessonTwoTwoExtension();

    /** Lesson 2.3's extension: two groups dropped together are one layer;
     *  Split makes a layer each, More ▾ → Merge puts them back. */
    void lessonTwoThreeExtension();

    /** Lesson 2.4's extension: an effect on a speed master (the Look
     *  Editor's Speed), shown in the footer. */
    void lessonTwoFourExtension();
    void lessonTwoFourStageChase();

    /** Lesson 2.5's extension: the fan's Spread 0, and Grouping: Pairs. */
    void lessonTwoFiveExtension();

    /** Lesson 2.6, live: Operate, GO to the verse, and a hit fired from
     *  its Virtual Console button over it, then let go. */
    void lessonTwoSixFiresAHit();

    /** Lesson 2.7, played: GO into the chorus, Sub GO for the stomp, its
     *  auto-follow, and GO on to a cue that doesn't receive it. */
    void lessonTwoSevenPlaysTheStomps();

    /** Lesson 2.7's extension: a fourth sub-cue, and its number. */
    void lessonTwoSevenExtension();

    /** COURSE_BUILD=1: makes Level 2's start and finish files, each lesson
     *  done with the mouse from the one before's finish (COURSE_LESSON=2.N:
     *  that lesson only). Skipped otherwise. */
    void buildLevelTwoShows();

    /** COURSE_SHOTS=<folder>: Level 2's pictures (COURSE_LESSON=2.N: that
     *  lesson's only). Skipped otherwise. */
    void levelTwoPictures();

private:
    bool open(const QString &path);
    ShowTreeView *showTree() const;

private:
    App *m_app = nullptr;
};

#endif

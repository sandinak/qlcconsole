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

    /** Lesson 0.2: the Rig Overview's views and room light, as the handout
     *  names them, and GO three times on Lesson 1.1's finish. */
    void lessonZeroTwoTheVirtualStage();

    /** Lesson 1.4's fades and hold play in the Run tab: the Pose fades in
     *  over 3 s, the Scream holds 2 s and the Solo follows by itself. */
    void lessonFourPlaysTheFades();

    /** Lesson 1.5, running the show: GO, Back, LOAD (a click: STANDBY),
     *  a jump (double-click) and Stop, in the Run tab. */
    void lessonFiveRunsTheShow();

    /** COURSE_BUILD=1: makes Lessons 1.2-1.5's start and finish files, each
     *  lesson done with the mouse from the one before's finish. Skipped
     *  otherwise. */
    void buildLessonShows();

    /** COURSE_SHOTS=<folder>: the annotated pictures for the lesson and the
     *  Rig Overview of each cue. Skipped otherwise. */
    void lessonPictures();

private:
    bool open(const QString &path);
    ShowTreeView *showTree() const;

private:
    App *m_app = nullptr;
};

#endif

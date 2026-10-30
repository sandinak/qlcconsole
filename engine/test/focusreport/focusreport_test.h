/*
  Q Light Controller Plus
  focusreport_test.h

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

#ifndef FOCUSREPORT_TEST_H
#define FOCUSREPORT_TEST_H

#include <QTemporaryDir>
#include <QObject>

class Doc;

/** The focus report (board Q134), what a venue remembers for it (who
 *  checked each light, dead lights), a new venue from the last one, and
 *  scramble practice (Q140). */
class FocusReport_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** A save in a venue notes who checked the light and when; a light can
     *  be marked dead; both are in the venue's file and read back. */
    void aVenueRemembersChecksAndDeadLights();
    /** The report: call by call, the target with the stage marks'
     *  position, the correction from the show's own, the checks and flags;
     *  the CSV in Lightwright's columns, quoted where it must be. */
    void theReportListsEveryLight();
    /** A new venue copies the last venue's corrections, not its checks. */
    void aNewVenueStartsFromTheLastOne();
    /** Practice: a scrambled copy, applied without touching the real
     *  venue; focusing it back scores near zero; ending puts the real
     *  venue back; the score is in the report. */
    void practiceScramblesAndScores();
    /** Make Student Handout: the copy's venues leave out practice and its key */
    void handoutLeavesOutThePractice();

    /** Q134: a swapped light is flagged (and needs focus); the lens set by
     *  hand is its column -- rows, CSV and HTML. */
    void aSwappedLightAndTheLensAreReported();

private:
    Doc *m_doc = nullptr;
    QTemporaryDir *m_dir = nullptr;
};

#endif

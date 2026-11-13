/*
  Q Light Controller Plus
  padbenchshow_test.h

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

#ifndef PADBENCHSHOW_TEST_H
#define PADBENCHSHOW_TEST_H

#include <QObject>
#include <QString>

class App;
class QTemporaryDir;

/* test-workspaces/PadBench.qlcc: the show the game-controller checklist
   (Pad Run, the follow spot, focus calls, move in black, Renumber Song) is
   run against on the hardware bench. Built here with the app's own engine,
   saved, opened again and checked; PADBENCH_OUT=<.qlcc> writes it (and its
   venues file beside it) where you say. */
class PadBenchShow_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** Builds the show, saves it, opens it again and checks every item the
     *  checklist names: present, named, and doing what it says. */
    void buildSaveAndCheck();
    /** The committed test-workspaces/PadBench.qlcc (a copy of it) passes
     *  the same checks. */
    void theCommittedShowChecksOut();
    /** Pad Run on the committed show, as the bench runs it: Operate, the
     *  Run tab's Pad: Run, Main cue 1 (the follow spot) playing -- A alone
     *  says to arm, LB arms (Pad: ARMED), LB + A GOes; the footer says the
     *  pad's role beside the follow spot (M994). */
    void padRunWithTheFollowSpotPlaying();

private:
    bool open(const QString &path);
    void build(const QString &path);
    void check(const QString &path);

private:
    App *m_app = nullptr;
    QTemporaryDir *m_pluginDir = nullptr;
};

#endif

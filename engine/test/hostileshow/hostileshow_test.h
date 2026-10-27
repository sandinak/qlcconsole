/*
  Q Light Controller Plus - qlcconsole
  hostileshow_test.h

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

#ifndef HOSTILESHOW_TEST_H
#define HOSTILESHOW_TEST_H

#include <QObject>
#include <QTemporaryDir>

/* A show file is attacker-controlled input: it travels between crews on a USB
   stick, is emailed around and is handed to students. These are the ways a
   crafted or damaged .qxw / .qlcc can crash, hang or take over the console. */
class HostileShow_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    /* Script engine (engine/src/script.cpp) */
    void randomWithEqualBoundsDoesNotCrashTheTimer();
    void setFixtureOnAnUnpatchedFixtureIsSafe();
    void aSystemCommandIsNotRunMerelyByLoadingTheShow();
    /* RC8: "Allow scripts to run programs" (Preferences), off by default */
    void aSystemCommandIsOffUntilTheOperatorTurnsItOn();
    void aShowFromWebAccessNeverRunsPrograms();
    void randomTakesItsBoundsEitherWayRound();

    /* Loader (engine/src/doc.cpp, Qt XML) */
    void aBillionLaughsDocumentDoesNotHangOrOOM();
    void anExternalEntityIsNotResolved();
    void aCollectionThatContainsItselfDoesNotRecurseOnRun();
    void aChaserThatContainsItselfLoadsAndDoesNotRecurse();

private:
    QTemporaryDir m_settingsDir;
};

#endif

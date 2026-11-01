/*
  Q Light Controller Plus - Unit test
  operationregistry_test.h

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

#ifndef OPERATIONREGISTRY_TEST_H
#define OPERATIONREGISTRY_TEST_H

#include <QObject>

#include "showtreecommand.h"

class Doc;
class Fixture;

class OperationRegistry_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void catalogIsWhole();
    void cliRoundTrip_data();
    void cliRoundTrip();
    void unknownOrInvalidRunsNothing();
    void listFixtures();
    void listGroups();
    void listFunctions();
    void describeFunction();
    void findConflictsLeavesTheReviewAlone();
    void findUnusedFunctions();
    void findProblems();
    void listCues();
    void whyOn();
    void showSummary();
    void commandLineGoesThroughTheRegistry();
    void theShowsNamesWin();
    void helpListsTheQuestions();
    void commandNeedsACommandLine();
    void longAnswersAreCut();

    // P2: the change words
    void changeRenders_data();
    void changeRenders();
    void schemaRejectsBadArguments_data();
    void schemaRejectsBadArguments();
    void changeRunsItsGrammarLine();
    void planIsOneUndoStep();
    void planFailingPartWayChangesNothing();
    void planRefusals();

    // 0.4 leftovers: the playing words
    void masterWordMovesAGroupMaster();
    void masterWordRefusals();
    void masterIsPlayingNotEditing();
    void overrideWordMakesTheSelectedHitAnOverride();
    void overrideWordRefusals();

private:
    Fixture *dimmer(const QString &name, quint32 address, quint32 channels = 4);
    /** Towers (2 dimmers), Steps (1), production "P" with list "Song" and
     *  cues 1 and 2; the selection on cue 1. */
    ShowTreeCommand::Context smallShow();
    QByteArray treeXml() const;

    Doc *m_doc = nullptr;
};

#endif

/*
  Q Light Controller Plus - Unit test
  aiassist_test.h

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

#ifndef AIASSIST_TEST_H
#define AIASSIST_TEST_H

#include <QTemporaryDir>
#include <QObject>
#include <QHash>

class FakeModelServer;
class AiAssist;
class Doc;

class AiAssist_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    // The adapters, on recorded bodies (no network)
    void openAiBody();
    void openAiParse();
    void anthropicBody();
    void anthropicParse();
    void endpointPaths();
    void parseModels();

    // Settings and keys
    void settingsDefaultsAndRoundTrip();
    void whereTheModelRuns();
    void keysNeverInSettings();
    void settingsPage();

    // The assistant against a fake model on loopback
    void assistantLines();
    void disabledSendsNothing();
    void findAddressConflicts();
    void refusesAnythingButAReadOnlyOperation();
    void invalidArgumentsGoBackToTheModel();
    void providerDownGreysTheEntry();
    void testConnectionListsModels();
    void cancel();
    void systemPromptContext();
    void commandLineShowsTheAnswer();

    // Design / Operate (section 5a)
    void operateDuringARequest();
    void operateUnloadsALocalModel();
    void modelStillResidentIsAnError();
    void askInOperateSendsNothing();
    void leakedObjectRaisesTheError();
    void guardFires();
    void toggleRepeatedlyWithRequestsInFlight();

    // By hand only: QLC_AI_LIVE_OLLAMA=http://localhost:11434
    void liveOllamaSmoke();

private:
    void enable(bool on = true, const QString &provider = QStringLiteral("ollama"));
    void overlappingPars();

    QTemporaryDir m_settingsDir;
    QHash<QString, QString> m_keys;
    Doc *m_doc = nullptr;
    AiAssist *m_ai = nullptr;
    FakeModelServer *m_server = nullptr;
};

#endif

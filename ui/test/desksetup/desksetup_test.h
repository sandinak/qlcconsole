/*
  Q Light Controller Plus - Unit test
  desksetup_test.h

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

#ifndef DESKSETUP_TEST_H
#define DESKSETUP_TEST_H

#include <QTemporaryDir>
#include <QObject>

class IOPluginStub;
class Doc;

/* Connections ▸ Control surfaces ▸ This computer's desk, and the footer's
   desk chip: four typed learns, the On tick, the show's PIN, export and
   import; green, amber and red, Patch It and its Undo, what GO plays. */
class DeskSetup_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void thePanelLearnsEachRole();
    void anOpenLearnEndsWithThePanel();
    void theOnTickTurnsOneRoleOff();
    void thePinGuardsThePanel();
    void exportAndImportFromThePanel();
    void theChipIsGreenAmberOrRed();
    void patchItFromTheChipIsNamedAndUndone();
    void theChipNamesWhatGoPlays();
    void theHubShowsTheDeskFirst();
    void deskShots();

private:
    void send(quint32 universe, quint32 channel, uchar value);
    void learnGo();

    Doc *m_doc = nullptr;
    /** This test's own settings (never the person's, nor another leg's
     *  copy of this test running at the same time in the gate). */
    QTemporaryDir m_settings;
    IOPluginStub *m_stub = nullptr;
};

#endif

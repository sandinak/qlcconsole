/*
  Q Light Controller Plus - Test Unit
  showcontrol_ui_test.h

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

#ifndef SHOWCONTROL_UI_TEST_H
#define SHOWCONTROL_UI_TEST_H

#include <QTemporaryDir>
#include <QObject>

class Chaser;
class Doc;

class ShowControlUi_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    void oscFromTheNetworkFiresTheCue();
    void theChaserEditorSetsCuesAndTheList();

    // Connections' Show control & sync page
    void thePageShowsTheCurrentSettings();
    void editingThePageChangesTheListener();
    void aCommandReceivedShowsAsLast();
    void aPortInUseShowsAnError();
    void theTimecodeSourceAndHealth();
    /** A show's universes are added one at a time as it loads: the page's
     *  lists are rebuilt once for the run, not once for each (F1). */
    void manyUniversesRebuildTheListOnce();
    void mscOnOneChosenInput();
    void theRowsInConnectionsDevices();

private:
    Chaser *makeList(int steps);

    Doc *m_doc = nullptr;
    QTemporaryDir m_settingsDir;
    QByteArray m_realHome;
    QByteArray m_realProfile;
};

#endif

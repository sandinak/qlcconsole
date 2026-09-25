/*
  Q Light Controller Plus - Test Unit
  stagedoutput_test.h

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

#ifndef STAGEDOUTPUT_TEST_H
#define STAGEDOUTPUT_TEST_H

#include <QObject>

class Doc;
class Fixture;

/**
 * Every engine path that writes raw DMX by fixture address. An inventory
 * (staged) fixture reports universe 1 / address 1 but owns no address, so
 * each writer must pass over it -- else it lights whatever really lives at
 * 1/1. Each test puts a real and a staged fixture at 0/0 side by side.
 */
class StagedOutput_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void parkSkipsStagedFixtures();
    void aParkedFixtureReturnedToInventoryStopsWriting();
    void markSkipsStagedFixtures();
    void aMarkedFixtureReturnedToInventoryStopsWriting();
    void highlightSkipsStagedFixtures();
    void flashSkipsStagedFixtures();
    void effectIntentsSkipStagedFixtures();
    void programmerParkAndMarkSkipStagedFixtures();
    void programmerLiveWritesSkipStagedFixtures();

private:
    Doc *m_doc;
    Fixture *m_real;
    Fixture *m_staged;
};

#endif

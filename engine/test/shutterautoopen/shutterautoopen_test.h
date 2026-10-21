/*
  Q Light Controller Plus - Test Unit
  shutterautoopen_test.h

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

#ifndef SHUTTERAUTOOPEN_TEST_H
#define SHUTTERAUTOOPEN_TEST_H

#include <QObject>

class Universe;
class Fixture;
class Doc;

class ShutterAutoOpen_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void theOpenValueComesFromTheDefinition();
    void aClosedSpotLightsAtFull();
    void turnedOffItStaysDark();
    void anExplicitShutterWins();
    void aHeadWithNoLevelIsUntouched();
    void aRestingDimmerIsNotALevel();
    void aOneShotClosedStaysClosed();
    void aSceneWithOnlyALevelLights();
    void itIsSavedWithTheShow();

private:
    /** One processFaders() frame; the shutter's output value */
    int frame();

    Doc *m_doc = nullptr;
    Fixture *m_spot = nullptr;
    Universe *m_uni = nullptr;
    int m_dimmer = -1;      // universe addresses
    int m_shutter = -1;
};

#endif

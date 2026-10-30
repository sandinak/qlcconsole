/*
  Q Light Controller Plus - Unit test
  controlmap_test.h

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

#ifndef CONTROLMAP_TEST_H
#define CONTROLMAP_TEST_H

#include <QObject>

class IOPluginStub;
class Scene;
class Doc;

class ControlMap_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void learningBindsTheNextPress();
    void toggleStartsAndStops();
    void momentaryIsOnWhileHeld();
    void startOnlyAndStopOnly();
    void oneControlOneTarget();
    void theLightFollowsWhatItFires();
    void aCueListGoesAndACueJumps();
    void aHitFires();
    void savedWithTheShowAndLoadedBack();
    void aShowWithoutBindingsSavesAsBefore();
    void aDeletedFunctionTakesItsBinding();
    void nothingListensWithoutBindings();
    void namesSayWhatAControlIs();

private:
    /** A press (value) or a release (0) on @a channel of input universe 0,
     *  through the stub plugin and the input patch, as a controller sends. */
    void send(quint32 channel, uchar value);
    void tick(int n = 1);
    Scene *scene(const QString &name, int channel);

private:
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
};

#endif

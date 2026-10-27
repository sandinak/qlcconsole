/*
  Q Light Controller - Unit test
  chaser_test.h

  Copyright (c) Heikki Junnila

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

#ifndef CHASER_TEST_H
#define CHASER_TEST_H

#include <QObject>

class Doc;
class Chaser_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void deletingARunningFunctionLetsGoOfItsChannels();
    /** The same with the MasterTimer on its own thread, as in the app: a
     *  chaser deleted mid-step, over and over, never takes the timer down
     *  (the chaser's teardown racing its runner) and lets go of the rig. */
    void deletingAChaserWhileTheTimerRunsIsSafe();
    void cleanup();

    void initial();
    void directionRunOrder();
    void steps();
    void stepAt();
    void functionRemoval();
    void copyFrom();
    void createCopy();
    void speedModes();

    void loadSuccessLegacy();
    void loadSuccess();
    void loadWrongType();
    void loadWrongRoot();
    /** A step Number missing, negative or past the end (fuzz, matrix 2:
     *  QList::insert at an invalid index) appends instead of corrupting. */
    void loadStepNumberOutOfRange();
    void postLoad();
    void save();

    void tap();
    void preRun();
    void writeHTP();
    void writeLTP();
    void postRun();
    /** The runner is made and deleted on the MasterTimer thread: it has no
     *  QObject parent (the GUI-thread Chaser's children were changed from
     *  that thread, TSan M2), lives on the GUI thread, and postRun and
     *  ~Chaser still delete it. */
    void theRunnerHasNoParent();
    void adjustIntensity();

    void quickChaser();

private:
    Doc* m_doc;
};

#endif

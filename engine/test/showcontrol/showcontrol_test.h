/*
  Q Light Controller Plus
  showcontrol_test.h

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

#ifndef SHOWCONTROL_TEST_H
#define SHOWCONTROL_TEST_H

#include <QObject>

class Chaser;
class Doc;

class ShowControl_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void aStepsCueIsItsNumberOrItsPosition();
    void cuesMatchAsNumbersOrAsText();
    void cueNumbersAndTheMainListAreSaved();
    void aCommandFindsItsList();
    void goPlaysTheCueAndThenTheNext();
    void loadPutsACueOnStandby();
    void pauseResumeStopAndAllOff();
    void aCueThatIsntThereDoesNothing();
    void mscIsDecoded();
    void mscIsOnlyForUs();
    void mscFromAPatchedInputFiresTheCue();
    void oscIsDecoded();
    void oscFiresTheCue();
    void removingTheMainListForgetsIt();
    void skippedStepsArePassedOver();
    void autoFollowPassesOverSkippedSteps();
    void standbyIsWhatGoPlaysNext();

private:
    Chaser *makeList(int steps, const QString &name);

    Doc *m_doc = nullptr;
};

#endif

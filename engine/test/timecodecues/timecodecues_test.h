/*
  Q Light Controller Plus - Test Unit
  timecodecues_test.h

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

#ifndef TIMECODECUES_TEST_H
#define TIMECODECUES_TEST_H

#include <QObject>
#include <QList>
#include <QPair>
#include <QString>

class Doc;

/* Cues at a timecode (TimecodeCues): TimecodeSource is fed synthetic
   positions and the run() calls are read off the run handler. */
class TimecodeCues_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void firesEachCueAtItsTime();
    void goesThroughTheOneGoPath();
    void aJumpForwardFiresOnlyTheLatest();
    void aJumpBackRearms();
    void jitterIsNotAJumpBack();
    void aDropoutHolds();
    void cuesWithNoTimeAreSkipped();
    void aManualGoBetweenTimecodeCuesChangesNothing();
    void onlyListsThatFollowFire();
    void nextCueAndCountdown();
    void parseAndFormat();
    void saveAndLoad();
    void anOldShowFollowsNothing();

private:
    quint32 makeList(int cues, const QList<int> &timesMs, bool follow = true);
    void feed(quint32 ms);

    Doc *m_doc = nullptr;
    /** What run() was asked, in order: cue, chaser, route. */
    struct Call { QString cue; quint32 chaser; int route; int command; };
    QList<Call> m_calls;
    QList<quint32> m_cues;
};

#endif

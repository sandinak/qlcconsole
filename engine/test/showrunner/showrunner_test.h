/*
  Q Light Controller Plus - Test Unit
  showrunner_test.h

  Copyright (c) Massimo Callegari

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

#ifndef SHOWRUNNER_TEST_H
#define SHOWRUNNER_TEST_H

#include <QObject>

class Doc;
class Show;
class Track;
class Scene;

class ShowRunner_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void initRunner();
    void intensity();
    void stopRunner();
    void timecodeFollow();
    void timecodeDriveAndFreeze();

    /** With a track soloed only it plays; another track is silent unless it
     *  is solo-safe, and un-soloing brings the others back mid-run. */
    void aSoloTrackPlaysAndTheOthersOnlyIfSoloSafe();
    /** Hold last look: a gap after a hold-last track's item keeps its look
     *  up; a track without the flag goes dark in the same gap. */
    void holdLastKeepsTheLookThroughAGap();
    /** Priority: an Override track wins a channel over a Normal one that
     *  started after it; a Background track yields to a Normal one. */
    void anOverrideTrackWinsAndABackgroundTrackYields();
    /** A track's intensity submaster scales what it plays, set before the
     *  show starts and moved while it runs. */
    void aTrackAtHalfIntensityPlaysAtHalf();
    /** The show's configured length ends playback there, items running past
     *  it or not, and the look parks. */
    void theShowStopsAtItsConfiguredEnd();

private:
    Doc *m_doc;
    Show *m_show;
    Track *m_track;
    Scene *m_scene;
};

#endif

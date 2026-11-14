/*
  Q Light Controller Plus
  candlepresses_test.h

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

#ifndef CANDLEPRESSES_TEST_H
#define CANDLEPRESSES_TEST_H

#include <QObject>
#include <QString>

class Fixture;
class Chaser;
class Scene;
class Doc;

/* The IR candle remote's Dim and Brighten are presses, not states
   (docs/features/stage-targets.md "Candles"): a press never tracks, a Back
   or a later Window change never sends it again, and Show Review's CUE-17
   says where one tracks or repeats. */
class CandlePresses_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** CND-01: Press="true" on a capability: read from the bundled
     *  definition (Dim, Brighten; not On..Light, not Window), kept by
     *  save/load and a copy, ignored where it isn't. */
    void pressComesFromTheDefinition();
    /** CND-02: a channel with a press never fades; its Window does. */
    void aPressChannelNeverFades();
    /** CND-03: a Dim tracked into the next cue sends 0 there, not 5; the
     *  cue that sets it sends 5. */
    void dimDoesNotTrack();
    /** CND-04: On, Off, Candle and Light track as any value -- with no
     *  press in the list, and with one later on; a state older than a
     *  press since is not sent again. */
    void statesTrack();
    /** CND-05: played: GO through a Dim and on, Back, a jump back over
     *  it: Dim is sent once; Back to the Dim cue itself sends it again. */
    void backOverADimDoesNotSendItAgain();
    /** CND-06: a later cue that changes only Window doesn't restart the
     *  press (Select is 0 by then). */
    void aWindowChangeDoesNotRestartAPress();
    /** CND-07: a Release, a base cue that has the press, an include of the
     *  press cue: none send it again. */
    void releaseBaseAndIncludeDoNotResend();
    /** CND-08: a show with no press bakes exactly as before: no guard, the
     *  same functions; a guard is found again after save and load. */
    void olderShowsUnaffected();
    /** CND-09: CUE-17 fires for a tracked Dim and a chase with Brighten,
     *  says so in plain words, and is quiet for states and a released
     *  press. */
    void showReviewWarnsAboutAPressThatTracks();

private:
    bool rig();
    Scene *candles(const QString &name, int select, int window);
    quint32 layer(quint32 cue, const Scene *scene);
    Chaser *chaserOf(quint32 listId) const;
    /** What the cue's collection plays on the remote's channel @a ch
     *  (CueOutput), -1: nothing. */
    int cueSends(quint32 cueId, quint32 ch) const;

    void tick(int n = 1);
    void go(Chaser *c);
    void back(Chaser *c);
    void jump(Chaser *c, int index);
    /** Ticks long enough for a GO, Back or jump to land, watching the
     *  remote's Select and Window the way the blaster does. */
    void land();
    int select() const;
    int window() const;

    Doc *m_doc = nullptr;
    Fixture *m_remote = nullptr;
    Fixture *m_dimmer = nullptr;
    /** The blaster: Select / Window as last seen, and how many presses
     *  (a change of either while Select is Dim or Brighten) it sent. */
    int m_lastSelect = -1, m_lastWindow = -1;
    int m_presses = 0;
};

#endif

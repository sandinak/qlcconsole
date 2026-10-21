/*
  Q Light Controller Plus
  trackback_test.h

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

#ifndef TRACKBACK_TEST_H
#define TRACKBACK_TEST_H

#include <QObject>
#include <QList>
#include <QMap>

class Chaser;
class Doc;

/* The output at a cue is the same however the cue was reached: GO from the
   top, GO GO Back GO, a jump, Back from the cue after. */
class TrackBack_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void backThenGoKeepsTheColourTheCuesBeforeSet();
    void colourTracksTheSameWhicheverWayYouArrive();
    void fadesHoldsAndLoop();
    void blockCueOnlyReleaseIncludesAndMix();
    void aLongList();
    void courseShows();
    void aRealShow();

private:
    enum Step { Go, Back, Jump };
    struct Move
    {
        Step step;
        int cue;     //!< Jump: where to; else unused
    };

    /** The purpose-built list (§ the task): A red, B blue, A green, Block,
     *  a tracking cue. Its chaser. */
    Chaser *colourList();
    /** One with every mode: Block, Cue-only, Release, an include, a cue
     *  that mixes. Its chaser. */
    Chaser *everyModeList();

    bool rig();
    Chaser *chaserOf(quint32 listId) const;
    bool set(quint32 cue, quint32 group, quint32 palette);

    void tick(int n = 1);
    QByteArray settle(int ticks);
    QByteArray land(Chaser *c);
    void act(Chaser *c, const Move &m);
    void stopAll();
    /** What each cue of @a c outputs played straight from the top. */
    QMap<int, QByteArray> straightRun(Chaser *c);
    /** Plays @a moves from a dark stage, comparing every cue landed on
     *  with @a ref: the channels that differ, summed; @a why says where. */
    int walk(Chaser *c, const QMap<int, QByteArray> &ref, const QList<Move> &moves, QString *why);
    /** Every short sequence and a set of random walks over @a c. */
    int everyWay(Chaser *c, int randomWalks, QString *why);
    bool loadShow(const QString &path);

    Doc *m_doc = nullptr;
    QMap<int, QByteArray> m_moving;   //!< per cue of the straight run: channels an effect moves
    int m_settle = 8;                 //!< ticks for the list's longest fade
    quint32 m_list = 0;
    quint32 m_a = 0, m_b = 0, m_all = 0;
    quint32 m_red = 0, m_blue = 0, m_green = 0, m_white = 0;
    quint32 m_full = 0, m_half = 0, m_front = 0, m_side = 0;
};

#endif

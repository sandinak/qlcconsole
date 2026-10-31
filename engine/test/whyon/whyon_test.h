/*
  Q Light Controller Plus
  whyon_test.h

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

#ifndef WHYON_TEST_H
#define WHYON_TEST_H

#include <QObject>
#include <QList>

class FixtureGroup;
class Chaser;
class Doc;

/* "Why is this on?" v1 (WhyOn): the answer, worked out on demand from the
   last frame, names each source in crew words, top-down. Frames are made by
   hand (timerTick + processFaders); nothing is sent. */
class WhyOn_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void aTrackedCueSaysWhereItCameFrom();
    void anEffectAtHalfDepth();
    void anOverrideHitPullsItDown();
    void anHtpFightNamesTheWinner();
    void parkHoldsItPastTheGrandMaster();
    void theGrandMasterScalesIt();
    void blindAndNothingSetsIt();
    void aChannelAndABareAddress();
    void simpleWordingDropsTheMergeWords();

private:
    void tick(int n = 1);
    uchar out(int address);
    quint32 dimmer(int level, const QString &name);
    FixtureGroup *group(const QString &name, const QList<quint32> &fixtures);
    void go(Chaser *c);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif

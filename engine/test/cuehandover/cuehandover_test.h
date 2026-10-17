/*
  Q Light Controller Plus
  cuehandover_test.h

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

#ifndef CUEHANDOVER_TEST_H
#define CUEHANDOVER_TEST_H

#include <QObject>

class Collection;
class Fixture;
class Chaser;
class Scene;
class Doc;

class CueHandover_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void aSceneSharedByTwoCuesKeepsRunningOnGo();
    void aSceneSharedByTwoCuesKeepsRunningOnBack();
    void aSceneSharedByTwoCuesKeepsRunningOnAJump();
    void autoFollowHandsOverToo();
    void theLeavingCuesIntensityIsGivenBack();
    void aCueThatIncludesTheCueBeforeKeepsItRunning();
    void theOldCuesOwnScenesStillStop();
    void stoppingTheListStopsTheSharedScene();
    void cuesWithNothingSharedChangeAsBefore();

private:
    void tick(int n = 1);
    int level(int channel);
    void go(Chaser *c);
    void back(Chaser *c);
    void jump(Chaser *c, int index);
    Scene *scene(const QString &name, int channel, int value, uint fadeIn = 0);
    Collection *cue(const QString &name, const QList<Scene *> &members);
    Chaser *list(const QList<Collection *> &cues, uint fadeIn);

    Doc *m_doc = nullptr;
    Fixture *m_fixture = nullptr;
};

#endif

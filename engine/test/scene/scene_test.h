/*
  Q Light Controller - Unit test
  scene_test.h

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

#ifndef SCENE_TEST_H
#define SCENE_TEST_H

#include <QObject>

class Doc;
class Scene_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void initial();
    void values();
    void colorValue();
    void channelGroup();
    void fixtureRemoval();
    void loadSuccess();
    void loadWrongType();
    void loadWrongRoot();
    void save();
    void paletteFadeTime();
    /** A palette tried on a look (Scene::setTrialPalette) plays after the
     *  look's own, but is never saved, copied or listed as the look's. */
    void aTriedOnPalettePlaysButIsNotKept();
    void lookScope();
    void copyFrom();
    void createCopy();

    void preRunPostRun();

    void flashUnflash();

    void writeHTPZeroTicks();
    void aSceneWithAnUnpatchedFixturePlaysWithoutCrashing();
    void writeHTPTwoTicks();

    /** "an effect on top of the existing effects that allow transitions ..
     *  fade in .. wipe .. from center": a look arriving and leaving by a
     *  transition, each fixture at its own moment. */
    void transitionTimingAndPlaces();
    void aLookWipesInAndOutAcrossTheStage();
    /** A transition doesn't apply when the scene's tempo is Beats: the look
     *  arrives and leaves by its own fades, every fixture together. */
    void aTransitionIsIgnoredWhenTheTempoIsBeats();
    void writeHTPStopWhilePaused();
    void writeHTPTwoTicksIntensity();
    void writeLTPReady();

private:
    Doc* m_doc;
};

#endif

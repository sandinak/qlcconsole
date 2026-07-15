/*
  Q Light Controller Plus - Test Unit
  showrunner_test.cpp

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

#include <QtTest>
#define private public
#include "showrunner.h"
#undef private
#include "show.h"
#include "track.h"
#include "scene.h"
#include "doc.h"
#include "showrunner_test.h"

void ShowRunner_Test::initTestCase()
{
    m_doc = new Doc(this);
    m_show = new Show(m_doc);
    m_doc->addFunction(m_show);
    m_scene = new Scene(m_doc);
    m_doc->addFunction(m_scene);
    m_track = new Track(m_scene->id());
    ShowFunction *sf = new ShowFunction(m_show->getLatestShowFunctionId());
    sf->setFunctionID(m_scene->id());
    sf->setStartTime(0);
    sf->setDuration(1000);
    m_track->addShowFunction(sf);
    m_show->addTrack(m_track);
}

void ShowRunner_Test::cleanupTestCase()
{
    delete m_doc;
}

void ShowRunner_Test::initRunner()
{
    ShowRunner runner(m_doc, m_show->id());
    QCOMPARE(runner.m_timeFunctions.count(), 1);
    QCOMPARE(runner.m_totalRunTime, quint32(1000));
}

void ShowRunner_Test::intensity()
{
    ShowRunner runner(m_doc, m_show->id());
    runner.adjustIntensity(0.5, m_track);
    QCOMPARE(runner.m_intensityMap[m_track->id()], 0.5);
}

void ShowRunner_Test::stopRunner()
{
    ShowRunner runner(m_doc, m_show->id());
    runner.m_elapsedTime = 500;
    runner.m_runningQueue.append(QPair<Function*,quint32>(m_scene,1000));
    runner.stop();
    QCOMPARE(runner.m_elapsedTime, quint32(0));
    QCOMPARE(runner.m_runningQueue.count(), 0);
}

void ShowRunner_Test::timecodeFollow()
{
    ShowRunner runner(m_doc, m_show->id());

    // Off by default.
    QCOMPARE(runner.m_timecodeFollow, false);

    runner.setTimecodeFollow(true);
    QCOMPARE(runner.m_timecodeFollow, true);
    QCOMPARE(runner.m_externalTimeSet, false);

    runner.setExternalTime(5000);
    QCOMPARE(runner.m_externalTime, quint32(5000));
    QCOMPARE(runner.m_externalTimeSet, true);

    // Seek past the only function (start 0, dur 1000): it is skipped so the
    // start phase would not restart it.
    runner.seekTo(5000);
    QCOMPARE(runner.m_elapsedTime, quint32(5000));
    QCOMPARE(runner.m_currentTimeFunctionIndex, runner.m_timeFunctions.count());

    // Seek into the middle of the function: it stays active (index 0).
    runner.seekTo(500);
    QCOMPARE(runner.m_elapsedTime, quint32(500));
    QCOMPARE(runner.m_currentTimeFunctionIndex, 0);

    // Disabling follow clears the external-time latch.
    runner.setTimecodeFollow(false);
    QCOMPARE(runner.m_externalTimeSet, false);
}

QTEST_APPLESS_MAIN(ShowRunner_Test)

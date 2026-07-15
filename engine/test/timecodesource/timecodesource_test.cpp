/*
  Q Light Controller Plus - Test Unit
  timecodesource_test.cpp

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
#include <QSignalSpy>

#include "timecodesource.h"
#include "timecodesource_test.h"

void TimecodeSource_Test::defaults()
{
    TimecodeSource tc;
    QCOMPARE(tc.positionMs(), quint32(0));
    QCOMPARE(tc.isRunning(), false);
    QCOMPARE(tc.sourceUniverse(), qint32(-1));
    QCOMPARE(tc.lastUniverse(), qint32(-1));
}

void TimecodeSource_Test::updateAndRunning()
{
    TimecodeSource tc;
    QSignalSpy runSpy(&tc, SIGNAL(runningChanged(bool)));
    QSignalSpy timeSpy(&tc, SIGNAL(timeChanged(quint32)));

    tc.updateTimeCode(0, 12345, 25);
    QCOMPARE(tc.positionMs(), quint32(12345));
    QCOMPARE(tc.fps(), 25);
    QCOMPARE(tc.isRunning(), true);
    QCOMPARE(tc.lastUniverse(), qint32(0));
    QCOMPARE(runSpy.count(), 1);          // false -> true once
    QCOMPARE(timeSpy.count(), 1);

    // A second update while already running must not re-emit runningChanged.
    tc.updateTimeCode(0, 12385, 25);
    QCOMPARE(runSpy.count(), 1);
    QCOMPARE(timeSpy.count(), 2);
    QCOMPARE(tc.positionMs(), quint32(12385));
}

void TimecodeSource_Test::watchdogStops()
{
    TimecodeSource tc;
    QSignalSpy runSpy(&tc, SIGNAL(runningChanged(bool)));

    tc.updateTimeCode(0, 1000, 30);
    QCOMPARE(tc.isRunning(), true);

    // No further timecode: the watchdog (200 ms) must flip running to false,
    // holding the last position.
    QTRY_VERIFY_WITH_TIMEOUT(tc.isRunning() == false, 2000);
    QCOMPARE(tc.positionMs(), quint32(1000)); // frozen at last position
    QCOMPARE(runSpy.count(), 2);              // true then false
}

void TimecodeSource_Test::overrideFilter()
{
    TimecodeSource tc;
    tc.setSourceUniverse(1); // lock onto universe 1

    // Universe 0 is ignored.
    tc.updateTimeCode(0, 5000, 30);
    QCOMPARE(tc.isRunning(), false);
    QCOMPARE(tc.positionMs(), quint32(0));

    // Universe 1 is accepted.
    tc.updateTimeCode(1, 7000, 30);
    QCOMPARE(tc.isRunning(), true);
    QCOMPARE(tc.positionMs(), quint32(7000));
    QCOMPARE(tc.lastUniverse(), qint32(1));
}

QTEST_MAIN(TimecodeSource_Test)

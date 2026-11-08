/*
  Q Light Controller Plus - Unit test
  dmxusb_test.cpp

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

#include "writehealth.h"
#include "dmxusb_test.h"

/* 44 frames a second: one write every 23 ms. */
static const qint64 Frame = 23;

void DMXUSB_Test::aWidgetThatStopsTakingDataIsAProblem()
{
    WriteHealth h;
    qint64 t = 0;
    for (; t < 2000; t += Frame)
        QVERIFY(!h.record(true, t));
    QVERIFY(h.problem().isEmpty());

    // Unplugged: every write refused from here on.
    int changes = 0;
    const qint64 pulled = t;
    for (; t < pulled + 3000; t += Frame)
        changes += h.record(false, t) ? 1 : 0;
    QCOMPARE(changes, 1);
    QVERIFY(h.isFailing());
    QVERIFY2(h.problem().startsWith("not taking data since "), qPrintable(h.problem()));
}

void DMXUSB_Test::aFewLostFramesAreNot()
{
    WriteHealth h;
    qint64 t = 0;
    for (int i = 0; i < 200; i++, t += Frame)
        QVERIFY(!h.record(i % 50 != 7 && i % 50 != 8, t));    // two refused now and then
    QVERIFY(!h.isFailing());
    QVERIFY(h.problem().isEmpty());
}

void DMXUSB_Test::itClearsOnceWritesHaveGoneThroughAWhile()
{
    WriteHealth h;
    qint64 t = 0;
    for (; t <= WriteHealth::FailAfterMs + Frame; t += Frame)
        h.record(false, t);
    QVERIFY(h.isFailing());

    // Plugged back in: a moment of writes is not enough, a while is.
    const qint64 back = t;
    int changes = 0;
    for (; t < back + WriteHealth::RecoverAfterMs - Frame; t += Frame)
        changes += h.record(true, t) ? 1 : 0;
    QCOMPARE(changes, 0);
    QVERIFY(h.isFailing());
    // A refused write in that while starts it again.
    h.record(false, t);
    t += Frame;
    const qint64 again = t;
    for (; t < again + WriteHealth::RecoverAfterMs + Frame; t += Frame)
        changes += h.record(true, t) ? 1 : 0;
    QCOMPARE(changes, 1);
    QVERIFY(!h.isFailing());
    QVERIFY(h.problem().isEmpty());
}

QTEST_GUILESS_MAIN(DMXUSB_Test)

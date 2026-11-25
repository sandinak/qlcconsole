/*
  Q Light Controller - Unit test
  grandmaster_test.cpp

  Copyright (c) Jano Svitok

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
#include <sys/time.h>

#include "grandmaster_test.h"

#define protected public
#include "grandmaster.h"
#include "grandmasterpickup.h"
#undef protected


void GrandMaster_Test::initTestCase()
{
    m_gm = new GrandMaster(this);
}

void GrandMaster_Test::cleanupTestCase()
{
    delete m_gm;
}

void GrandMaster_Test::initial()
{
    QCOMPARE(m_gm->valueMode(), GrandMaster::Reduce);
    QCOMPARE(m_gm->channelMode(), GrandMaster::Intensity);
    QCOMPARE(m_gm->value(), uchar(255));
    QCOMPARE(m_gm->fraction(), 1.0);
}

void GrandMaster_Test::channelMode()
{
    m_gm->setChannelMode(GrandMaster::AllChannels);
    QCOMPARE(m_gm->channelMode(), GrandMaster::AllChannels);

    m_gm->setChannelMode(GrandMaster::Intensity);
    QCOMPARE(m_gm->channelMode(), GrandMaster::Intensity);

    QCOMPARE(GrandMaster::stringToChannelMode("All"), GrandMaster::AllChannels);
    QCOMPARE(GrandMaster::stringToChannelMode("Intensity"), GrandMaster::Intensity);
    QCOMPARE(GrandMaster::stringToChannelMode("foobar"), GrandMaster::Intensity);

    QCOMPARE(GrandMaster::channelModeToString(GrandMaster::AllChannels), QString("All"));
    QCOMPARE(GrandMaster::channelModeToString(GrandMaster::Intensity), QString("Intensity"));
    // Nothing else to try: both values a ChannelMode can hold are named, and
    // casting 42 to it is undefined behaviour (UBSan).
}

void GrandMaster_Test::valueMode()
{
    m_gm->setValueMode(GrandMaster::Limit);
    QCOMPARE(m_gm->valueMode(), GrandMaster::Limit);

    m_gm->setValueMode(GrandMaster::Reduce);
    QCOMPARE(m_gm->valueMode(), GrandMaster::Reduce);

    QCOMPARE(GrandMaster::stringToValueMode("Limit"), GrandMaster::Limit);
    QCOMPARE(GrandMaster::stringToValueMode("Reduce"), GrandMaster::Reduce);
    QCOMPARE(GrandMaster::stringToValueMode("xyzzy"), GrandMaster::Reduce);

    QCOMPARE(GrandMaster::valueModeToString(GrandMaster::Limit), QString("Limit"));
    QCOMPARE(GrandMaster::valueModeToString(GrandMaster::Reduce), QString("Reduce"));
    // Nothing else to try: both values a ValueMode can hold are named, and
    // casting 31337 to it is undefined behaviour (UBSan).
}

void GrandMaster_Test::value()
{
    for (int i = 0; i < UCHAR_MAX; i++)
    {
        m_gm->setValue(uchar(i));
        QCOMPARE(m_gm->value(), uchar(i));
        QCOMPARE(m_gm->fraction(), (double(i) / double(UCHAR_MAX)));
    }

    m_gm->setValue(0);
    QCOMPARE(m_gm->value(), uchar(0));
    QCOMPARE(m_gm->fraction(), double(0));

    m_gm->setValue(255);
    QCOMPARE(m_gm->value(), uchar(255));
    QCOMPARE(m_gm->fraction(), double(1));
}

void GrandMaster_Test::pickupAfterLoadReplugWake()
{
    GrandMasterPickup p;
    // Never rearmed: it follows at once, both ways (as the GM always did).
    QVERIFY(p.accept(10, 255, 0));
    QVERIFY(p.accept(200, 10, 1));
    QVERIFY(!p.waiting());

    // A load, a replug, a wake: the same rearm.
    for (int round = 0; round < 3; ++round)
    {
        const qint64 t0 = 10000 * (round + 1);
        p.rearm(t0);
        QVERIFY(p.waiting());
        // A fader parked low, brushed with the show at 100 %: waits.
        QVERIFY(!p.accept(5, 255, t0 + 600));
        QVERIFY(!p.accept(40, 255, t0 + 620));
        QVERIFY(p.waiting());
        QCOMPARE(p.direction(255), 1);
        // At the master (within the threshold): picked up, then it follows.
        QVERIFY(p.accept(252, 255, t0 + 700));
        QVERIFY(!p.waiting());
        QVERIFY(p.accept(100, 252, t0 + 710));
        // From above: passing through the master picks it up too.
        p.rearm(t0 + 1000);
        QVERIFY(!p.accept(200, 100, t0 + 1600));
        QVERIFY(p.accept(60, 100, t0 + 1610));
        QVERIFY(!p.waiting());
    }
    // A pull to 0 always applies at once, waiting or not.
    p.rearm(50000);
    QVERIFY(!p.accept(30, 255, 50600));
    QVERIFY(p.accept(0, 255, 50650));
    QVERIFY(!p.waiting());
}

void GrandMaster_Test::deviceDumpIgnored()
{
    GrandMasterPickup p;
    p.rearm(1000);
    // Whatever the device sends in the first 500 ms -- a 0 included -- is its dump.
    QVERIFY(!p.accept(0, 255, 1000));
    QVERIFY(!p.accept(255, 255, 1200));
    QVERIFY(!p.accept(0, 255, 1499));
    QVERIFY(p.waiting());
    QCOMPARE(p.last(), -1);
    // After it, the rules: a 0 applies, a value away from the master waits.
    QVERIFY(!p.accept(90, 255, 1500));
    QVERIFY(p.accept(0, 255, 1501));
    QCOMPARE(GrandMasterPickup::DumpMs, qint64(500));
}

QTEST_APPLESS_MAIN(GrandMaster_Test)

/*
  Q Light Controller Plus - Unit test
  lastlookeffect_test.cpp

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

#include "lastlookeffect_test.h"
#include "lastlookeffect.h"
#include "inputoutputmap.h"
#include "universe.h"
#include "doc.h"

void LastLookEffect_Test::initTestCase()
{
    m_doc = new Doc(this);
}

void LastLookEffect_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = NULL;
}

void LastLookEffect_Test::init()
{
    m_doc->clearContents();
}

void LastLookEffect_Test::cleanup()
{
    m_doc->clearContents();
}

void LastLookEffect_Test::holdAssertsForceLTP()
{
    LastLookEffect ll(m_doc);
    QVERIFY(ll.isActive() == false);

    // Hold two channels on universe 0 at fixed values (fixture 1).
    QList<LastLookEffect::ChannelHold> holds;
    holds.append({1, 0, 10, uchar(200)});   // intensity-ish
    holds.append({1, 0, 11, uchar(128)});   // a position-ish LTP channel
    ll.hold(holds);
    QVERIFY(ll.isActive() == true);

    QList<Universe*> unis = m_doc->inputOutputMap()->universes();
    QVERIFY(unis.size() >= 1);

    // Zero the target channels first (mimic the per-tick intensity reset), then
    // let the holder assert. forceLTP must land the held values into preGM.
    unis.at(0)->reset(10, 2);
    ll.writeDMX(nullptr, unis);
    QCOMPARE(unis.at(0)->preGMValue(10), uchar(200));
    QCOMPARE(unis.at(0)->preGMValue(11), uchar(128));

    // A second tick keeps re-asserting (the hold is persistent).
    unis.at(0)->reset(10, 2);
    ll.writeDMX(nullptr, unis);
    QCOMPARE(unis.at(0)->preGMValue(10), uchar(200));
}

void LastLookEffect_Test::clearReleases()
{
    LastLookEffect ll(m_doc);
    QList<LastLookEffect::ChannelHold> holds;
    holds.append({1, 0, 20, uchar(90)});
    ll.hold(holds);
    QVERIFY(ll.isActive() == true);

    ll.clear();
    QVERIFY(ll.isActive() == false);

    // After clear, a tick must NOT re-assert the value onto a freshly zeroed
    // channel.
    QList<Universe*> unis = m_doc->inputOutputMap()->universes();
    unis.at(0)->reset(20, 1);
    ll.writeDMX(nullptr, unis);
    QCOMPARE(unis.at(0)->preGMValue(20), uchar(0));
}

void LastLookEffect_Test::emptyHoldIsInactive()
{
    LastLookEffect ll(m_doc);
    ll.hold(QList<LastLookEffect::ChannelHold>());
    QVERIFY(ll.isActive() == false);
}

void LastLookEffect_Test::releaseFixtureYieldsOnlyThatFixture()
{
    LastLookEffect ll(m_doc);
    QList<LastLookEffect::ChannelHold> holds;
    holds.append({1, 0, 0, uchar(200)});   // fixture 1
    holds.append({2, 0, 5, uchar(150)});   // fixture 2
    ll.hold(holds);
    QVERIFY(ll.isActive() == true);

    QList<Universe*> unis = m_doc->inputOutputMap()->universes();

    // A new cue claims fixture 1 only: its held channel yields, fixture 2 holds.
    ll.releaseFixtures(QList<quint32>() << 1);
    QVERIFY(ll.isActive() == true);

    unis.at(0)->reset(0, 6);
    ll.writeDMX(nullptr, unis);
    QCOMPARE(unis.at(0)->preGMValue(0), uchar(0));     // fixture 1 released
    QCOMPARE(unis.at(0)->preGMValue(5), uchar(150));   // fixture 2 still held

    // Releasing the last fixture empties the hold.
    ll.releaseFixtures(QList<quint32>() << 2);
    QVERIFY(ll.isActive() == false);

    // Releasing a fixture that isn't held is a no-op.
    ll.hold(holds);
    ll.releaseFixtures(QList<quint32>() << 99);
    QVERIFY(ll.isActive() == true);
}

QTEST_MAIN(LastLookEffect_Test)

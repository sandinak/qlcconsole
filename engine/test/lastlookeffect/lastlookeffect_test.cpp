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
#include <thread>
#include <atomic>

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

void LastLookEffect_Test::registrationCycle()
{
    // Registration must stay consistent with entries across hold/clear/re-hold
    // (the m_registered race fix). isActive() reflects registration state.
    LastLookEffect ll(m_doc);
    QVERIFY(ll.isActive() == false);

    ll.hold(QList<LastLookEffect::ChannelHold>() << LastLookEffect::ChannelHold{1, 0, 0, uchar(200)});
    QVERIFY(ll.isActive() == true);

    ll.clear();
    QVERIFY(ll.isActive() == false);

    // Re-hold after clear must re-register (the stuck-unregistered failure mode).
    ll.addHold(QList<LastLookEffect::ChannelHold>() << LastLookEffect::ChannelHold{2, 0, 5, uchar(90)});
    QVERIFY(ll.isActive() == true);

    // Draining the last fixture leaves it inactive (not stuck-registered-empty).
    ll.releaseFixtures(QList<quint32>() << 2);
    QVERIFY(ll.isActive() == false);
}

void LastLookEffect_Test::addHoldAccumulates()
{
    LastLookEffect ll(m_doc);
    ll.hold(QList<LastLookEffect::ChannelHold>() << LastLookEffect::ChannelHold{1, 0, 0, uchar(100)});
    // A second track's look accumulates (hold-last across tracks), doesn't replace.
    ll.addHold(QList<LastLookEffect::ChannelHold>() << LastLookEffect::ChannelHold{2, 0, 5, uchar(150)});

    QList<Universe*> unis = m_doc->inputOutputMap()->universes();
    unis.at(0)->reset(0, 6);
    ll.writeDMX(nullptr, unis);
    QCOMPARE(unis.at(0)->preGMValue(0), uchar(100));   // fixture 1 still held
    QCOMPARE(unis.at(0)->preGMValue(5), uchar(150));   // fixture 2 added

    // Re-adding fixture 1 with a new value REPLACES its old entry (fresh look).
    ll.addHold(QList<LastLookEffect::ChannelHold>() << LastLookEffect::ChannelHold{1, 0, 0, uchar(40)});
    unis.at(0)->reset(0, 6);
    ll.writeDMX(nullptr, unis);
    QCOMPARE(unis.at(0)->preGMValue(0), uchar(40));    // replaced, not duplicated
    QCOMPARE(unis.at(0)->preGMValue(5), uchar(150));   // fixture 2 unchanged
}

void LastLookEffect_Test::concurrentStress()
{
    // Hammer the holder from three threads (mutate on two, writeDMX on a third)
    // the way the real UI + timer threads do, to shake out the m_entries /
    // m_registered synchronization. Passing = no crash / no data race abort;
    // finishing with a consistent state is asserted at the end.
    LastLookEffect ll(m_doc);
    QList<Universe*> unis = m_doc->inputOutputMap()->universes();
    QVERIFY(unis.size() >= 1);

    std::atomic<bool> stop(false);
    const int N = 20000;

    std::thread t1([&]() {                       // "timer thread" captures + yields
        for (int i = 0; i < N; i++)
        {
            QList<LastLookEffect::ChannelHold> h;
            h.append({quint32(i % 8), 0, i % 400, uchar(i)});
            ll.addHold(h);
            ll.releaseFixtures(QList<quint32>() << quint32(i % 8));
        }
    });
    std::thread t2([&]() {                        // "UI thread" clears / re-holds
        for (int i = 0; i < N; i++)
        {
            QList<LastLookEffect::ChannelHold> h;
            h.append({quint32(4 + (i % 4)), 0, 100 + (i % 200), uchar(i)});
            ll.hold(h);
            if (i % 3 == 0)
                ll.clear();
        }
    });
    std::thread t3([&]() {                        // universe thread ticks writeDMX
        while (!stop.load())
            ll.writeDMX(nullptr, unis);
    });

    t1.join();
    t2.join();
    stop.store(true);
    t3.join();

    // After all mutations settle, one more op must leave a consistent state.
    ll.clear();
    QVERIFY(ll.isActive() == false);
    ll.addHold(QList<LastLookEffect::ChannelHold>() << LastLookEffect::ChannelHold{1, 0, 0, uchar(200)});
    QVERIFY(ll.isActive() == true);   // registration recovered after the storm
    ll.clear();
}

QTEST_MAIN(LastLookEffect_Test)

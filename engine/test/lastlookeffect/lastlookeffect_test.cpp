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

#define private public                  // MasterTimer::timerTick()
#include "lastlookeffect_test.h"
#define protected public                // Universe::processFaders()
#include "universe.h"
#undef protected
#include "lastlookeffect.h"
#include "inputoutputmap.h"
#include "qlcchannel.h"
#include "functionparent.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "mastertimer.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"
#undef private

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
    // run a frame: the holder's fader lands the held values into preGM.
    unis.at(0)->reset(10, 2);
    unis.at(0)->processFaders();
    QCOMPARE(unis.at(0)->preGMValue(10), uchar(200));
    QCOMPARE(unis.at(0)->preGMValue(11), uchar(128));

    // A second frame keeps them (the hold is persistent).
    unis.at(0)->reset(10, 2);
    unis.at(0)->processFaders();
    QCOMPARE(unis.at(0)->preGMValue(10), uchar(200));
}

/* The order a real frame runs in: the MasterTimer ticks the DMX sources
   (the holder writes), then the universe thread's processFaders() zeroes
   the intensity channels, lays the faders on and sends the frame. */
void LastLookEffect_Test::holdSurvivesTheUniverseFrame()
{
    LastLookEffect ll(m_doc);
    QList<LastLookEffect::ChannelHold> holds;
    holds.append({1, 0, 10, uchar(200)});   // a dimmer
    holds.append({1, 0, 11, uchar(128)});   // a pan
    ll.hold(holds);

    QList<Universe*> unis = m_doc->inputOutputMap()->universes();
    unis.at(0)->setChannelCapability(10, QLCChannel::Intensity);
    unis.at(0)->setChannelCapability(11, QLCChannel::Pan);

    for (int frame = 0; frame < 3; ++frame)
        unis.at(0)->processFaders();
    QCOMPARE(unis.at(0)->preGMValue(11), uchar(128));    // an LTP channel holds
    QCOMPARE(unis.at(0)->preGMValue(10), uchar(200));    // and so does the dimmer (B1)
    ll.clear();
    unis.at(0)->processFaders();
    QCOMPARE(unis.at(0)->preGMValue(10), uchar(0));      // let go: the dimmer is free
}

/* The hold is a base under the show (B1): merged like a running function,
   never over one. */
void LastLookEffect_Test::holdIsABaseUnderTheShow()
{
    LastLookEffect ll(m_doc);
    Universe *uni = m_doc->inputOutputMap()->universes().at(0);
    uni->setChannelCapability(30, QLCChannel::Intensity);
    uni->setChannelCapability(31, QLCChannel::Pan);
    uni->setChannelCapability(32, QLCChannel::Intensity);
    ll.hold(QList<LastLookEffect::ChannelHold>()
            << LastLookEffect::ChannelHold{1, 0, 30, uchar(100)}
            << LastLookEffect::ChannelHold{1, 0, 31, uchar(128)});

    QSharedPointer<GenericFader> fader = uni->requestFader();
    auto put = [&](int address, uchar value) {
        GenericFader::ChannelRef fc = fader->getChannelFader(m_doc, uni, Fixture::invalidId(), quint32(address));
        fc->setStart(value);
        fc->setCurrent(value);
        fc->setTarget(value);
    };

    // A function on another channel: both are up.
    put(32, 70);
    uni->processFaders();
    QCOMPARE(uni->preGMValue(30), uchar(100));
    QCOMPARE(uni->preGMValue(31), uchar(128));
    QCOMPARE(uni->preGMValue(32), uchar(70));

    // On the same channels: intensity HTP, the higher level shows; the
    // function takes the LTP channel.
    put(30, 60);
    put(31, 30);
    uni->processFaders();
    QCOMPARE(uni->preGMValue(30), uchar(100));
    QCOMPARE(uni->preGMValue(31), uchar(30));
    put(30, 150);
    uni->processFaders();
    QCOMPARE(uni->preGMValue(30), uchar(150));

    // The function gone, the held dimmer is still there under it.
    uni->dismissFader(fader);
    uni->processFaders();
    QCOMPARE(uni->preGMValue(30), uchar(100));
    QCOMPARE(uni->preGMValue(32), uchar(0));
    ll.clear();
    uni->processFaders();
    QCOMPARE(uni->preGMValue(30), uchar(0));
}

/* A cue started after the show takes the held fixtures it drives outright
   (Doc releases them from the hold), lower level or not; the others keep
   holding. */
void LastLookEffect_Test::aLaterCueTakesItsFixturesFromTheHold()
{
    Fixture *a = new Fixture(m_doc);
    a->setChannels(1);
    a->setUniverse(0);
    a->setAddress(40);
    m_doc->addFixture(a);
    Fixture *b = new Fixture(m_doc);
    b->setChannels(1);
    b->setUniverse(0);
    b->setAddress(41);
    m_doc->addFixture(b);
    Universe *uni = m_doc->inputOutputMap()->universes().at(0);

    LastLookEffect *ll = m_doc->lastLook();
    ll->hold(QList<LastLookEffect::ChannelHold>()
             << LastLookEffect::ChannelHold{a->id(), 0, 40, uchar(200)}
             << LastLookEffect::ChannelHold{b->id(), 0, 41, uchar(150)});
    uni->processFaders();
    QCOMPARE(uni->preGMValue(40), uchar(200));
    QCOMPARE(uni->preGMValue(41), uchar(150));

    Scene *cue = new Scene(m_doc);
    cue->setValue(a->id(), 0, 60);
    m_doc->addFunction(cue);
    cue->start(m_doc->masterTimer(), FunctionParent::master());
    for (int i = 0; i < 3; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QCoreApplication::processEvents();
        uni->processFaders();
    }
    QCOMPARE(uni->preGMValue(40), uchar(60));       // the cue's, not the hold's 200
    QCOMPARE(uni->preGMValue(41), uchar(150));      // still held
    QVERIFY(ll->isActive());

    cue->stop(FunctionParent::master());
    m_doc->masterTimer()->timerTick();
    uni->processFaders();
    QCOMPARE(uni->preGMValue(41), uchar(150));
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

    // After clear, a frame must NOT put the value back on a freshly zeroed
    // channel.
    QList<Universe*> unis = m_doc->inputOutputMap()->universes();
    unis.at(0)->reset(20, 1);
    unis.at(0)->processFaders();
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
    unis.at(0)->processFaders();
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
    unis.at(0)->processFaders();
    QCOMPARE(unis.at(0)->preGMValue(0), uchar(100));   // fixture 1 still held
    QCOMPARE(unis.at(0)->preGMValue(5), uchar(150));   // fixture 2 added

    // Re-adding fixture 1 with a new value REPLACES its old entry (fresh look).
    ll.addHold(QList<LastLookEffect::ChannelHold>() << LastLookEffect::ChannelHold{1, 0, 0, uchar(40)});
    unis.at(0)->reset(0, 6);
    unis.at(0)->processFaders();
    QCOMPARE(unis.at(0)->preGMValue(0), uchar(40));    // replaced, not duplicated
    QCOMPARE(unis.at(0)->preGMValue(5), uchar(150));   // fixture 2 unchanged
}

void LastLookEffect_Test::concurrentStress()
{
    // Hammer the holder from three threads (mutate on two, run frames on a third)
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
    std::thread t3([&]() {                        // the universe thread's frames
        while (!stop.load())
            unis.at(0)->processFaders();
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

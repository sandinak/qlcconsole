/*
  Q Light Controller - Unit test
  genericfader_test.cpp

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

#include <QtTest>
#include <QThread>
#include <atomic>
#if defined(Q_OS_WIN)
#include <windows.h>
#endif

#include "genericfader_test.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "fadechannel.h"
#include "qlcchannel.h"
#include "universe.h"
#include "qlcfile.h"
#include "doc.h"

#define private public
#include "genericfader.h"
#undef private

#include "../common/resource_paths.h"

void GenericFader_Test::initTestCase()
{
    m_doc = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir) == true);
}

void GenericFader_Test::init()
{
    Fixture *fxi = new Fixture(m_doc);
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef("Futurelight", "DJScan250");
    QVERIFY(def != NULL);

    QLCFixtureMode *mode = def->mode("Mode 1");
    QVERIFY(mode != NULL);

    fxi->setFixtureDefinition(def, mode);
    fxi->setAddress(10);
    m_doc->addFixture(fxi);
}

void GenericFader_Test::cleanup()
{
    m_doc->clearContents();
}

void GenericFader_Test::addRemove()
{
    QList<Universe*> ua = m_doc->inputOutputMap()->universes();
    QSharedPointer<GenericFader> fader = QSharedPointer<GenericFader>(new GenericFader());

    FadeChannel fc(m_doc, 0, 0);
    FadeChannel fc1(m_doc, 0, 1);
    FadeChannel fc2(m_doc, 0, 2);
    FadeChannel wrong(m_doc, 0, QLCChannel::invalid());
    quint32 chHash = GenericFader::channelHash(fc.fixture(), fc.channel());

    QCOMPARE(fader->m_channels.count(), 0);
    QVERIFY(fader->m_channels.contains(chHash) == false);

    fader->add(fc);
    QVERIFY(fader->m_channels.contains(chHash) == true);
    QCOMPARE(fader->m_channels.count(), 1);

    fader->remove(&wrong);
    QVERIFY(fader->m_channels.contains(chHash) == true);
    QCOMPARE(fader->m_channels.count(), 1);

    GenericFader::ChannelRef fc3 = fader->getChannelFader(m_doc, ua[0], 0, 0);
    fader->remove(fc3.get());
    fc3.release();
    QVERIFY(fader->m_channels.contains(chHash) == false);
    QCOMPARE(fader->m_channels.count(), 0);

    fader->add(fc);
    QVERIFY(fader->m_channels.contains(chHash) == true);

    fader->add(fc1);
    chHash = GenericFader::channelHash(fc.fixture(), fc.channel());
    QVERIFY(fader->m_channels.contains(chHash) == true);

    fader->add(fc2);
    chHash = GenericFader::channelHash(fc.fixture(), fc.channel());
    QVERIFY(fader->m_channels.contains(chHash) == true);
    QCOMPARE(fader->m_channels.count(), 3);

    fader->removeAll();
    QCOMPARE(fader->m_channels.count(), 0);

    fc.setTarget(127);
    fader->add(fc);
    chHash = GenericFader::channelHash(fc.fixture(), fc.channel());
    QCOMPARE(fader->m_channels.size(), 1);
    QCOMPARE(fader->m_channels[chHash].target(), uchar(127));

    fc.setTarget(63);
    fader->add(fc);
    QCOMPARE(fader->m_channels.size(), 1);
    QCOMPARE(fader->m_channels[chHash].target(), uchar(63));

    fc.setCurrent(63);
    fader->add(fc);
    QCOMPARE(fader->m_channels.size(), 1);
    QCOMPARE(fader->m_channels[chHash].target(), uchar(63));
}

void GenericFader_Test::writeZeroFade()
{
    QList<Universe*> ua = m_doc->inputOutputMap()->universes();
    QSharedPointer<GenericFader> fader = ua[0]->requestFader();

    FadeChannel fc(m_doc, 0, 5);
    fc.setStart(0);
    fc.setTarget(255);
    fc.setFadeTime(0);

    fader->add(fc);
    QCOMPARE(ua[0]->preGMValues()[15], (char) 0);
    fader->write(ua[0]);
    QCOMPARE(ua[0]->preGMValues()[15], (char) 255);
}

void GenericFader_Test::anOverride16BitChannelWritesBothBytes()
{
    // A fader handling secondary channels holds a 16-bit channel (coarse +
    // fine) in one FadeChannel. With the Override flag both bytes must reach
    // the universe, MSB at the first address and LSB at the next -- not only
    // the low byte at the coarse address.
    QList<Universe*> ua = m_doc->inputOutputMap()->universes();
    QSharedPointer<GenericFader> fader = ua[0]->requestFader();

    FadeChannel fc(m_doc, 0, 5);
    fc.addChannel(6);
    QCOMPARE(fc.channelCount(), 2);
    fc.setStart(0x1234);
    fc.setCurrent(0x1234);
    fc.setTarget(0x1234);
    fc.setFadeTime(0);
    fc.addFlag(FadeChannel::Override);
    fader->add(fc);

    // Something lower in the universe that Override has to replace.
    ua[0]->write(15, 0xff);
    ua[0]->write(16, 0xff);

    fader->write(ua[0]);
    QCOMPARE(uchar(ua[0]->preGMValues()[15]), uchar(0x12));
    QCOMPARE(uchar(ua[0]->preGMValues()[16]), uchar(0x34));
    ua[0]->dismissFader(fader);
}

void GenericFader_Test::writeLoop()
{
    QList<Universe*> ua = m_doc->inputOutputMap()->universes();
    QSharedPointer<GenericFader> fader = ua[0]->requestFader();

    FadeChannel fc(m_doc, 0, 5);
    fc.setStart(0);
    fc.setTarget(250);
    fc.setFadeTime(1000);
    fader->add(fc);

    QCOMPARE(ua[0]->preGMValues()[15], (char) 0);

    int expected = 0;
    for (int i = MasterTimer::tick(); i <= 1000; i += MasterTimer::tick())
    {
        ua[0]->zeroIntensityChannels();
        fader->write(ua[0]);

        int actual = uchar(ua[0]->preGMValues()[15]);
        expected += 5;
        QCOMPARE(actual, expected);
    }
}

void GenericFader_Test::adjustIntensity()
{
    QList<Universe*> ua = m_doc->inputOutputMap()->universes();
    QSharedPointer<GenericFader> fader = ua[0]->requestFader();

    FadeChannel fc(m_doc, 0, 5);
    FadeChannel fc1(m_doc, 0, 0);

    // HTP channel
    fc.setStart(0);
    fc.setTarget(250);
    fc.setFadeTime(1000);
    fader->add(fc);

    // LTP channel
    fc1.setStart(0);
    fc1.setTarget(250);
    fc1.setFadeTime(1000);
    fader->add(fc1);

    qreal intensity = 0.5;
    fader->adjustIntensity(intensity);
    QCOMPARE(fader->intensity(), intensity);

    int expected = 0;
    for (int i = MasterTimer::tick(); i <= 1000; i += MasterTimer::tick())
    {
        ua[0]->zeroIntensityChannels();
        fader->write(ua[0]);

        expected += 5;

        // GenericFader should apply intensity only to HTP channels
        int actual = uchar(ua[0]->preGMValues()[15]);
        int expectedWithIntensity = floor((qreal(expected) * intensity) + 0.5);
        QVERIFY(actual == expectedWithIntensity);

        // No intensity adjustment on LTP channels
        actual = uchar(ua[0]->preGMValues()[10]);
        QVERIFY(actual == expected);
    }
}

/* The effect runner add()s/replace()s channels -- it never calls
   getChannelFader() -- so it hands the fader the doc; an unpatched fixture's
   value then goes to the fixture for the rig view, and nothing to DMX. */
void GenericFader_Test::anUnpatchedFixturesValuesAreShownNotWritten()
{
    Fixture *staged = new Fixture(m_doc);
    staged->setChannels(4);
    staged->setPatched(false);
    QVERIFY(m_doc->addFixture(staged));

    QList<Universe*> ua = m_doc->inputOutputMap()->universes();
    QSharedPointer<GenericFader> fader = ua[0]->requestFader(Universe::Override);
    fader->setDoc(m_doc);
    FadeChannel fc(m_doc, staged->id(), 2);
    fc.setTarget(200);
    fc.setCurrent(200);
    fc.setFadeTime(0);
    fader->replace(fc);
    fader->write(ua[0]);

    QCOMPARE(uchar(staged->channelValues().at(2)), uchar(200));
    for (int a = 0; a < 8; ++a)
        QCOMPARE(uchar(ua[0]->preGMValues()[a]), uchar(0));      // nothing sent

    // Gone with the fader: the fixture is dark again.
    fader->removeAll();
    QCOMPARE(uchar(staged->channelValues().at(2)), uchar(0));
}

void GenericFader_Test::replaceAllSwapsTheWholeSet()
{
    Fixture *staged = new Fixture(m_doc);
    staged->setChannels(4);
    staged->setPatched(false);
    QVERIFY(m_doc->addFixture(staged));

    QList<Universe*> ua = m_doc->inputOutputMap()->universes();
    QSharedPointer<GenericFader> fader = ua[0]->requestFader(Universe::Override);
    fader->setDoc(m_doc);

    QList<FadeChannel> first;
    for (quint32 ch = 1; ch <= 2; ++ch)
    {
        FadeChannel fc(m_doc, staged->id(), ch);
        fc.setTarget(150);
        fc.setCurrent(150);
        fc.setFadeTime(0);
        first << fc;
    }
    fader->replaceAll(first);
    QCOMPARE(fader->channelsCount(), 2);
    fader->write(ua[0]);
    QCOMPARE(uchar(staged->channelValues().at(1)), uchar(150));
    QCOMPARE(uchar(staged->channelValues().at(2)), uchar(150));

    // The next set keeps channel 2 lit and drops channel 1: only the dropped
    // one goes dark, and nothing of the old set lingers.
    QList<FadeChannel> second;
    second << first.at(1);
    fader->replaceAll(second);
    QCOMPARE(fader->channelsCount(), 1);
    QCOMPARE(uchar(staged->channelValues().at(1)), uchar(0));
    QCOMPARE(uchar(staged->channelValues().at(2)), uchar(150));

    fader->replaceAll(QList<FadeChannel>());
    QCOMPARE(fader->channelsCount(), 0);
    QCOMPARE(uchar(staged->channelValues().at(2)), uchar(0));
}

/* Universes process their faders on their own threads. A DMX source that
   emptied its fader and then refilled it left a window in which a universe
   could send a blank frame -- a static effect flashed in the rig view and on
   the lights. Rebuilt with replaceAll(), the fader is never seen empty. */
void GenericFader_Test::aFaderRebuiltEveryTickIsNeverSeenEmpty()
{
    QSharedPointer<GenericFader> fader(new GenericFader());
    QList<FadeChannel> set;
    for (quint32 ch = 0; ch < 64; ++ch)
    {
        FadeChannel fc(m_doc, Fixture::invalidId(), ch);
        fc.setTarget(100);
        fc.setCurrent(100);
        set << fc;
    }
    fader->replaceAll(set);

    std::atomic<bool> done(false);
    std::atomic<int> emptySeen(0);
    QThread *universe = QThread::create([&]() {
        while (!done.load())
            if (fader->channelsCount() == 0)
                ++emptySeen;
    });
    universe->start();
    for (int tick = 0; tick < 20000; ++tick)
        fader->replaceAll(set);
    done = true;
    universe->wait();
    delete universe;

    QCOMPARE(emptySeen.load(), 0);
}

/* The soak's heap corruption. A function sets a channel up on the timer
   thread while the universe writes the same fader on its own -- stepping the
   channels and dropping the ones that are done. A QHash moves its entries
   when one is removed or added, so a FadeChannel pointer kept past the lookup
   pointed at another channel, or at a freed slot, by the time it was set. The
   channel handed out stays locked until the caller is done with it. */
void GenericFader_Test::aChannelBeingSetUpStaysPutWhileTheUniverseWrites()
{
    QList<Universe*> ua = m_doc->inputOutputMap()->universes();
    QVERIFY(ua.count() > 0);
    Universe *uni = ua.at(0);
    QSharedPointer<GenericFader> fader(new GenericFader());

    std::atomic<bool> done(false);
    QThread *universe = QThread::create([&]() {
        while (!done.load())
        {
            fader->write(uni);   // each channel reaches its target, and goes
            QThread::yieldCurrentThread();
        }
    });
    universe->start();

    int moved = 0;
    // absolute addresses clear of the fixture init() patches at 10
    for (int i = 0; i < 500000; ++i)
    {
        const quint32 ch = 100 + quint32(i % 256);
        GenericFader::ChannelRef fc = fader->getChannelFader(m_doc, uni, Fixture::invalidId(), ch);
        fc->setStart(fc->current());
        fc->setTarget(uchar(i));
        fc->setFadeTime(0);
        fc->setElapsed(0);
        fc->setReady(false);
        fc->addFlag(FadeChannel::AutoRemove);
        if (fc->channel() != ch)
            ++moved;
    }
    done = true;
    universe->wait();
    delete universe;

    QCOMPARE(moved, 0);
}

/* Faders are made and dropped all the time -- a scene asks for one per
   universe each time it starts. With the channel lock a std::recursive_mutex
   under MinGW, each one left a kernel Event (and a heap block) behind:
   libstdc++ never destroys that mutex, and winpthreads makes both on its
   first lock. A stress show leaked ~5000 handles a second. The handle count
   is Windows' to give; elsewhere this just churns faders. */
void GenericFader_Test::aFaderLeavesNothingBehind()
{
    QList<Universe*> ua = m_doc->inputOutputMap()->universes();
    QVERIFY(ua.count() > 0);
    Universe *uni = ua.at(0);

    auto churn = [&](int n) {
        for (int i = 0; i < n; ++i)
        {
            QSharedPointer<GenericFader> fader(new GenericFader());
            {
                GenericFader::ChannelRef fc = fader->getChannelFader(m_doc, uni, Fixture::invalidId(), 100);
                fc->setTarget(uchar(i));
            }
            fader->write(uni);   // the universe thread's lock, too
        }
    };

    churn(100);   // whatever is made once, on first use
#if defined(Q_OS_WIN)
    DWORD before = 0, after = 0;
    QVERIFY(GetProcessHandleCount(GetCurrentProcess(), &before));
#endif
    churn(5000);
#if defined(Q_OS_WIN)
    QVERIFY(GetProcessHandleCount(GetCurrentProcess(), &after));
    QVERIFY2(after < before + 100,
             qPrintable(QString("handles %1 -> %2 over 5000 faders").arg(before).arg(after)));
#endif
}

QTEST_APPLESS_MAIN(GenericFader_Test)

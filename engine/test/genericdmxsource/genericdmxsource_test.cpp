/*
  Q Light Controller Plus - Unit test
  genericdmxsource_test.cpp

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
#include <QThread>
#include <QElapsedTimer>

#include "../common/stdprelude.h"
#define protected public
#define private public
#include "genericdmxsource_test.h"
#include "genericdmxsource.h"
#include "doc.h"
#include "mastertimer.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "fixture.h"
#undef private
#undef protected

void GenericDMXSource_Test::initTestCase()
{
    m_doc = new Doc(this);
}

void GenericDMXSource_Test::cleanupTestCase()
{
    delete m_doc;
}

void GenericDMXSource_Test::init()
{
    m_doc->clearContents();
    Fixture *fxi = new Fixture(m_doc);
    fxi->setChannels(1);
    fxi->setAddress(0);
    m_doc->addFixture(fxi);
    m_fxiId = fxi->id();
}

void GenericDMXSource_Test::cleanup()
{
    m_doc->clearContents();
}

void GenericDMXSource_Test::setUnset()
{
    GenericDMXSource src(m_doc);
    src.setOutputEnabled(true);
    src.set(m_fxiId, 0, 100);
    QCOMPARE(src.channelsCount(), quint32(1));
    QList<SceneValue> list = src.channels();
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.first().fxi, m_fxiId);
    QCOMPARE(list.first().channel, quint32(0));
    QCOMPARE(list.first().value, uchar(100));

    src.unset(m_fxiId, 0);
    QCOMPARE(src.channelsCount(), quint32(0));
}

void GenericDMXSource_Test::unsetAll()
{
    GenericDMXSource src(m_doc);
    src.setOutputEnabled(true);
    src.set(m_fxiId, 0, 50);
    src.writeDMX(nullptr, m_doc->inputOutputMap()->universes());
    QCOMPARE(src.channelsCount(), quint32(1));
    src.unsetAll();
    src.writeDMX(nullptr, m_doc->inputOutputMap()->universes());
    QCOMPARE(src.channelsCount(), quint32(0));
}

void GenericDMXSource_Test::aStagedFixtureNeverReachesAFader()
{
    // An inventory fixture reports universe 1 / address 1 like the real one
    // above, but has no address: nothing written for it may reach the rig.
    Fixture *staged = new Fixture(m_doc);
    staged->setChannels(1);
    staged->setPatched(false);
    QVERIFY(m_doc->addFixture(staged));

    GenericDMXSource src(m_doc);
    src.setOutputEnabled(true);
    src.set(staged->id(), 0, 200);
    src.writeDMX(nullptr, m_doc->inputOutputMap()->universes());
    QVERIFY(src.m_fadersMap.isEmpty());

    src.set(m_fxiId, 0, 100);
    src.writeDMX(nullptr, m_doc->inputOutputMap()->universes());
    QCOMPARE(src.m_fadersMap.size(), 1);
    const QHash<quint32, FadeChannel> chans = src.m_fadersMap.first()->channels();
    QCOMPARE(chans.size(), 1);
    QCOMPARE(chans.begin().value().fixture(), m_fxiId);
}

/* A source deleted while the MasterTimer is ticking -- a scene editor
   closed during a show -- is never read again: its faders went before it
   left the timer, and a tick still inside writeDMX() read the freed map. */
void GenericDMXSource_Test::aSourceGoneMidTickIsNeverRead()
{
    // A wide fixture: a tick's write is long enough to be caught in.
    Fixture *wide = new Fixture(m_doc);
    wide->setChannels(512);
    wide->setUniverse(1);
    wide->setAddress(0);
    QVERIFY(m_doc->addFixture(wide));
    m_doc->masterTimer()->start();
    QElapsedTimer clock;
    clock.start();
    int fed = 0, withFaders = 0;
    // A count, not a wall-clock window: a slow runner gets the same rounds,
    // just later. The deadline is only a backstop against a hang.
    for (int i = 0; fed < 40 && clock.elapsed() < 120000; ++i)
    {
        // Several at once, as a show with editors open has.
        QList<GenericDMXSource *> srcs;
        for (int k = 0; k < 8; ++k)
        {
            GenericDMXSource *src = new GenericDMXSource(m_doc);
            src->setOutputEnabled(true);
            src->set(m_fxiId, 0, uchar(i + k));
            for (quint32 c = 0; c < 512; ++c)
                src->set(wide->id(), c, uchar(c + k));
            srcs << src;
        }
        QThread::usleep(20000 + quint64(i % 13) * 1500);   // a tick or two, then anywhere in one
        for (GenericDMXSource *src : srcs)
        {
            {
                QMutexLocker locker(&src->m_mutex);
                if (!src->m_fadersMap.isEmpty())
                    ++withFaders;
            }
            src->set(m_fxiId, 0, uchar(i));                 // the next tick writes again
            QThread::usleep(quint64(i % 9) * 2000);
            QThread::usleep(quint64(i % 5) * 400);
            delete src;
        }
        ++fed;
    }
    m_doc->masterTimer()->stop();
    QVERIFY(fed > 10);                              // ASan, or a crash, says otherwise
    QVERIFY2(withFaders > fed, "the timer never wrote the sources: the test tests nothing");
}

QTEST_MAIN(GenericDMXSource_Test)

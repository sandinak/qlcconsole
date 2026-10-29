/*
  Q Light Controller Plus
  hotpath_test.cpp

  Copyright (C) Branson Matheson

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
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <functional>
#if defined(Q_OS_UNIX)
#include <time.h>
#endif

#include "hotpath_test.h"

// The counts read the universes' faders and drive the timer by hand.
#define private public
#define protected public
#include "inputoutputmap.h"
#include "rgbscriptscache.h"
#include "universe.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "rgbalgorithm.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "mastertimer.h"
#include "inputpatch.h"
#include "chaserstep.h"
#include "fixturegroup.h"
#include "rgbmatrix.h"
#include "chaser.h"
#include "scene.h"
#include "qlcfile.h"
#include "doc.h"
#undef protected
#undef private

#include "../common/resource_paths.h"

namespace
{
const int kFixturesPerUniverse = 170;      // Generic RGB: 510 channels

/** An RGB algorithm that counts the maps it is asked for. Every copy the
 *  matrix makes (its run-time clone) counts into the same total. */
class CountingAlgorithm final : public RGBAlgorithm
{
public:
    CountingAlgorithm(Doc *doc, int *counter) : RGBAlgorithm(doc), m_counter(counter) {}
    RGBAlgorithm *clone() const override { return new CountingAlgorithm(doc(), m_counter); }
    int rgbMapStepCount(const QSize &) override { return 4; }
    void rgbMapSetColors(const QVector<uint> &) override {}
    QVector<uint> rgbMapGetColors() override { return QVector<uint>(); }
    void rgbMap(const QSize &size, uint rgb, int step, RGBMap &map) override
    {
        ++*m_counter;
        map.resize(size.height());
        for (int y = 0; y < size.height(); ++y)
        {
            map[y].resize(size.width());
            for (int x = 0; x < size.width(); ++x)
                map[y][x] = (x + step) % 2 ? rgb : 0;
        }
    }
    QString name() const override { return QStringLiteral("Counting"); }
    QString author() const override { return QString(); }
    int apiVersion() const override { return 1; }
    Type type() const override { return Plain; }
    int acceptColors() const override { return 1; }
    bool loadXML(QXmlStreamReader &) override { return true; }
    bool saveXML(QXmlStreamWriter *) const override { return true; }

private:
    int *m_counter;
};

/** This thread's CPU time in ns: what the work costs itself, whatever else
 *  the machine is doing. Wall-clock time where there is no thread clock. */
qint64 cpuNs(const QElapsedTimer &wall)
{
#if defined(Q_OS_UNIX) && defined(CLOCK_THREAD_CPUTIME_ID)
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0)
        return qint64(ts.tv_sec) * 1000000000LL + qint64(ts.tv_nsec);
#endif
    return wall.nsecsElapsed();
}

/** CPU ns per call of @a op: the best of @a batches batches of @a reps calls
 *  (the least disturbed batch is the closest to the code's own cost). */
double nsPerOp(const std::function<void()> &op, int reps, int batches = 5)
{
    QElapsedTimer wall;
    wall.start();
    for (int i = 0; i < qMax(1, reps / 10); ++i)       // warm the caches
        op();
    double best = -1;
    for (int b = 0; b < batches; ++b)
    {
        const qint64 from = cpuNs(wall);
        for (int i = 0; i < reps; ++i)
            op();
        const double per = double(cpuNs(wall) - from) / reps;
        if (best < 0 || per < best)
            best = per;
    }
    return best;
}
}

void HotPath_Test::initTestCase()
{
    m_doc = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir));
    QVERIFY(m_doc->rgbScriptsCache()->load(QDir(INTERNAL_SCRIPTDIR)));

    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef("Generic", "Generic RGB");
    QVERIFY(def != nullptr);
    QLCFixtureMode *mode = def->mode("RGB");
    if (mode == nullptr)
        mode = def->modes().first();
    QCOMPARE(mode->channels().size(), 3);

    for (int u = 0; u < 2; ++u)
    {
        for (int i = 0; i < kFixturesPerUniverse; ++i)
        {
            Fixture *fxi = new Fixture(m_doc);
            fxi->setFixtureDefinition(def, mode);
            fxi->setAddress(quint32(i * 3));
            fxi->setUniverse(quint32(u));
            QVERIFY(m_doc->addFixture(fxi));
            m_fixtures[u] << fxi->id();
        }
    }
}

void HotPath_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = nullptr;
}

void HotPath_Test::cleanup()
{
    // Stop and delete every function, then let the universes let go.
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    frame(3);
    for (Function *f : m_doc->functions())
        m_doc->deleteFunction(f->id());
    frame(2);
}

Scene *HotPath_Test::makeLook(int universe, int fixtures, uchar level)
{
    Scene *s = new Scene(m_doc);
    for (int i = 0; i < fixtures; ++i)
        for (quint32 ch = 0; ch < 3; ++ch)
            s->setValue(m_fixtures[universe].at(i), ch, uchar(level - ch));
    s->setFadeInSpeed(0);
    s->setFadeOutSpeed(0);
    m_doc->addFunction(s);
    return s;
}

/** What the engine does once a frame: the timer's tick (every running
 *  function writes), then each universe computes its frame (its own thread
 *  does that in the app). */
void HotPath_Test::frame(int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        m_doc->masterTimer()->timerTick();
        for (Universe *u : m_doc->inputOutputMap()->universes())
            u->processFaders();
    }
}

/*****************************************************************************
 * Work counts
 *****************************************************************************/

/* A look that doesn't change costs the outputs nothing: the universe sends
   its first frame and then no more, however long it runs. A universe that
   re-sent unchanged frames would load every output plugin, every frame. */
void HotPath_Test::aStaticLookSendsOneFrameThenNone()
{
    Universe *uni = m_doc->inputOutputMap()->universes().at(0);
    int sent = 0;
    QMetaObject::Connection c = connect(uni, &Universe::universeWritten, this,
                                        [&sent](quint32, const QByteArray &) { ++sent; });
    Scene *look = makeLook(0, kFixturesPerUniverse, 200);
    look->start(m_doc->masterTimer(), FunctionParent::master());
    frame(3);
    QVERIFY(look->isRunning());
    QCOMPARE(uchar(uni->preGMValues().at(0)), uchar(200));
    const int afterStart = sent;
    QVERIFY2(afterStart >= 1 && afterStart <= 2, qPrintable(QString::number(afterStart)));

    frame(200);
    QCOMPARE(sent, afterStart);                 // four seconds, nothing new to send
    disconnect(c);
}

/* A running scene is one fader on its universe holding exactly its own
   channels, and it stays that size: nothing is added per frame. */
void HotPath_Test::aRunningSceneHoldsOneFaderOfItsChannels()
{
    Universe *uni = m_doc->inputOutputMap()->universes().at(0);
    const int before = uni->m_faders.size();
    Scene *look = makeLook(0, kFixturesPerUniverse, 180);
    look->start(m_doc->masterTimer(), FunctionParent::master());
    frame(5);

    QCOMPARE(uni->m_faders.size(), before + 1);
    QSharedPointer<GenericFader> fader = uni->m_faders.last();
    QCOMPARE(fader->channelsCount(), kFixturesPerUniverse * 3);

    frame(500);                                  // ten seconds
    QCOMPARE(uni->m_faders.size(), before + 1);
    QCOMPARE(fader->channelsCount(), kFixturesPerUniverse * 3);
    // The other universe has no work at all
    QCOMPARE(m_doc->inputOutputMap()->universes().at(1)->m_faders.size(), 0);
}

/* Starting and stopping a look over and over leaves the universe as it
   found it: a fader left behind per start would make every later frame a
   little slower, the slow kind of degradation a long show finds. */
void HotPath_Test::startStopCyclesLeaveNoFadersBehind()
{
    Universe *uni = m_doc->inputOutputMap()->universes().at(0);
    const int before = uni->m_faders.size();
    Scene *look = makeLook(0, kFixturesPerUniverse, 150);
    for (int i = 0; i < 200; ++i)
    {
        look->start(m_doc->masterTimer(), FunctionParent::master());
        frame(2);
        QVERIFY(uni->m_faders.size() <= before + 1);
        look->stop(FunctionParent::master());
        frame(2);
    }
    QCOMPARE(uni->m_faders.size(), before);
}

/* A looping chaser of looks: every step is a scene starting and the last
   one stopping. Over a hundred steps the universe never holds more than
   the steps in play (the old, fading out, and the new one). */
void HotPath_Test::aChaserKeepsItsFadersBounded()
{
    Universe *uni = m_doc->inputOutputMap()->universes().at(0);
    const int before = uni->m_faders.size();
    Chaser *chaser = new Chaser(m_doc);
    for (int s = 0; s < 4; ++s)
    {
        Scene *look = makeLook(0, kFixturesPerUniverse, uchar(60 + 40 * s));
        // 40 ms fade in, 60 ms hold, 40 ms fade out: a step every 5 frames
        chaser->addStep(ChaserStep(look->id(), 40, 60, 40));
    }
    chaser->setDurationMode(Chaser::PerStep);
    chaser->setFadeInMode(Chaser::PerStep);
    chaser->setFadeOutMode(Chaser::PerStep);
    chaser->setRunOrder(Function::Loop);
    m_doc->addFunction(chaser);
    chaser->start(m_doc->masterTimer(), FunctionParent::master());

    int most = 0;
    for (int i = 0; i < 500; ++i)               // about a hundred steps
    {
        frame();
        most = qMax(most, int(uni->m_faders.size()) - before);
    }
    QVERIFY(chaser->isRunning());
    QVERIFY2(most >= 1 && most <= 3, qPrintable(QString("faders at once: %1").arg(most)));
    QVERIFY(uni->m_faders.size() - before <= 3);
}

/* A matrix computes its map once a step, not once a frame: scripts are the
   most expensive function per instance (docs/performance.md), and a script
   asked every frame would cost 5 to 25 times as much. */
void HotPath_Test::aMatrixComputesAMapPerStepNotPerTick()
{
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setSize(QSize(10, 10));
    for (int i = 0; i < 100; ++i)
        grp->assignFixture(m_fixtures[1].at(i));
    m_doc->addFixtureGroup(grp);

    int maps = 0;
    RGBMatrix *mtx = new RGBMatrix(m_doc);
    mtx->setFixtureGroup(grp->id());
    mtx->setAlgorithm(new CountingAlgorithm(m_doc, &maps));
    mtx->setDuration(MasterTimer::tick() * 10);  // a step every 10 frames
    m_doc->addFunction(mtx);
    mtx->start(m_doc->masterTimer(), FunctionParent::master());

    const int frames = 200;
    frame(frames);
    QVERIFY(mtx->isRunning());
    const int steps = frames / 10;
    QVERIFY2(maps >= steps - 1 && maps <= steps + 2,
             qPrintable(QString("%1 maps in %2 frames, %3 steps").arg(maps).arg(frames).arg(steps)));
    m_doc->deleteFixtureGroup(grp->id());
}

/*****************************************************************************
 * CPU time per operation
 *****************************************************************************/

/* Nothing is asserted here (see the class comment). QLC_PERF_REPORT=<file>
   appends "name,ns_per_op" lines for tools/perf/guard.py;
   QLC_PERF_REPS=<n> scales the repetitions (default 1: a quick pass that
   keeps this test well under a second in the gate). */
void HotPath_Test::cpuPerOperation()
{
    const int scale = qMax(1, qEnvironmentVariableIntValue("QLC_PERF_REPS"));
    QList<QPair<QString, double>> results;
    Universe *uni = m_doc->inputOutputMap()->universes().at(0);

    // 1. A universe frame with 510 channels written by hand (a DMX source).
    results << qMakePair(QStringLiteral("universe_frame_510"), nsPerOp([uni]() {
        for (int ch = 0; ch < kFixturesPerUniverse * 3; ++ch)
            uni->write(ch, uchar(ch & 0xff));
        uni->processFaders();
    }, 200 * scale));

    // 2. A fader writing 510 channels into a universe.
    {
        GenericFader fader;
        for (int i = 0; i < kFixturesPerUniverse; ++i)
        {
            for (quint32 ch = 0; ch < 3; ++ch)
            {
                FadeChannel fc(m_doc, m_fixtures[0].at(i), ch);
                fc.setTarget(uchar(100 + ch));
                fc.setCurrent(uchar(100 + ch));
                fader.add(fc);
            }
        }
        QCOMPARE(fader.channelsCount(), kFixturesPerUniverse * 3);
        results << qMakePair(QStringLiteral("genericfader_write_510"),
                             nsPerOp([&fader, uni]() { fader.write(uni); }, 500 * scale));
    }

    // 3. A whole engine frame with a 170-fixture look running.
    {
        Scene *look = makeLook(0, kFixturesPerUniverse, 220);
        look->start(m_doc->masterTimer(), FunctionParent::master());
        frame(3);
        results << qMakePair(QStringLiteral("scene_frame_170"),
                             nsPerOp([this]() { frame(); }, 200 * scale));
        look->stop(FunctionParent::master());
        frame(2);
    }

    // 4. One step of a scripted matrix: the script's map for 16 x 16 heads.
    {
        RGBAlgorithm *algo = RGBAlgorithm::algorithm(m_doc, "Plasma");
        QVERIFY(algo != nullptr);
        QCOMPARE(algo->type(), RGBAlgorithm::Script);
        RGBMap map;
        int step = 0;
        algo->rgbMap(QSize(16, 16), 0xff0000, 0, map);
        QCOMPARE(map.size(), 16);
        // The script engine compiles a function after its first calls and
        // collects garbage now and then: well warmed, the numbers settle.
        for (int i = 0; i < 300; ++i)
            algo->rgbMap(QSize(16, 16), 0xff0000, i % 50, map);
        results << qMakePair(QStringLiteral("rgbscript_map_16x16"), nsPerOp([algo, &map, &step]() {
            algo->rgbMap(QSize(16, 16), 0xff0000, step++ % 50, map);
        }, 50 * scale, 7));
        delete algo;
    }

    // 5. A frame of input: 512 changed values buffered and flushed.
    {
        InputPatch ip(0, this);
        int got = 0;
        connect(&ip, &InputPatch::inputValueChanged, this,
                [&got](quint32, quint32, uchar, const QString &) { ++got; });
        results << qMakePair(QStringLiteral("inputpatch_frame_512"), nsPerOp([&ip]() {
            {
                QMutexLocker locker(&ip.m_inputBufferMutex);
                for (quint32 ch = 0; ch < 512; ++ch)
                    ip.m_inputBuffer.insert(ch, InputPatch::InputValue(uchar(ch), QString()));
            }
            ip.flush(0);
        }, 100 * scale));
        QVERIFY(got > 0);
        QCOMPARE(got % 512, 0);
    }

    QStringList table;
    for (const auto &r : results)
        table << QString("%1 %2 ns").arg(r.first, -26).arg(r.second, 10, 'f', 0);
    qInfo().noquote() << "CPU time per operation (this thread, best batch):\n  " + table.join("\n  ");

    const QString report = qEnvironmentVariable("QLC_PERF_REPORT");
    if (!report.isEmpty())
    {
        QFile f(report);
        QVERIFY2(f.open(QIODevice::Append | QIODevice::Text), qPrintable(report));
        QTextStream out(&f);
        for (const auto &r : results)
            out << r.first << "," << QString::number(r.second, 'f', 1) << "\n";
    }
}

QTEST_GUILESS_MAIN(HotPath_Test)

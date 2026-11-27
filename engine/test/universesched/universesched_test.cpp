/*
  Q Light Controller Plus
  universesched_test.cpp

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
#include <QColor>
#include <QElapsedTimer>
#include <QFile>
#include <QHostInfo>
#include <QMutex>
#include <QSysInfo>
#include <QTextStream>

#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

#include <sys/resource.h>
#include <stdlib.h>
#if defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#endif

#include "universesched_test.h"
#include "resource_paths.h"

#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "qlcioplugin.h"
#include "qlcpoint.h"
#include "qlcfile.h"
#include "rgbscriptscache.h"
#include "ioplugincache.h"
#include "inputoutputmap.h"
#include "fixturegroup.h"
#include "grouphead.h"
#include "chaserstep.h"
#include "rgbalgorithm.h"
#include "rgbmatrix.h"
#include "efxfixture.h"
#include "mastertimer.h"
#include "universe.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "efx.h"
#include "doc.h"

namespace
{
/* An output that counts what it is given: every universe its own line, so
   the frames go out as they would to a network node, and nothing is kept
   (80 universe threads write here at once). */
class CountingOutput final : public QLCIOPlugin
{
public:
    explicit CountingOutput(int lines) : m_lines(lines) {}

    void init() override {}
    QString name() const override { return QStringLiteral("Counting output"); }
    int capabilities() const override { return QLCIOPlugin::Output; }
    QString pluginInfo() const override { return QString(); }
    QStringList outputs() override
    {
        QStringList list;
        for (int i = 0; i < m_lines; ++i)
            list << QStringLiteral("Line %1").arg(i + 1);
        return list;
    }
    bool openOutput(quint32 output, quint32 universe) override
    {
        addToMap(universe, output, Output);
        return true;
    }
    void closeOutput(quint32 output, quint32 universe) override
    {
        removeFromMap(universe, output, Output);
    }
    void writeUniverse(quint32, quint32, const QByteArray &data, bool) override
    {
        m_writes.fetch_add(1, std::memory_order_relaxed);
        m_bytes.fetch_add(quint64(data.size()), std::memory_order_relaxed);
    }

    const int m_lines;
    std::atomic<quint64> m_writes { 0 };
    std::atomic<quint64> m_bytes { 0 };
};

/* A rig the size and shape of the shows we run (board Q360, the shape of
   test/053-dh-scale's ScaleRig): per copy, 80 universes, 50 of them holding
   one or two devices, 260 fixtures, 26,364 channels --
     0-29   one 96-pixel RGBW bar each (384 ch);
     30-39  two Martin ERA 300 Profiles each (21 ch);
     40-49  two ADJ Micro Wash RGBW each (7 ch);
     50-75  three 42-pixel RGBW bars each (504 ch);
     76-77  27 WLED Lanterns each (15 ch);
     78     12 movers and 12 washes;  79  34 house dimmers.
   Running: a Plasma RGB matrix over every pixel, a circle EFX on every
   mover and three chasers of looks with per-step fades (lanterns, washes,
   house). @a copies > 1 repeats it on the next 80 universes. */
const int kRigUniverses = 80;
const int kRigFixtures = 260;
const int kRigChannels = 26364;

struct Rig
{
    QList<quint32> running;     ///< the functions to start
    int fixtures = 0;
    int channels = 0;
    int sparse = 0;             ///< universes holding at most two fixtures
};

void setLook(Scene *scene, const Fixture *fx, const QColor &colour)
{
    const QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr)
    {
        for (quint32 c = 0; c < fx->channels(); ++c)
            scene->setValue(fx->id(), c, 255);
        return;
    }
    for (int c = 0; c < mode->channels().size(); ++c)
    {
        const QLCChannel *ch = mode->channels().at(c);
        int v = -1;
        if (ch->group() == QLCChannel::Intensity)
        {
            switch (ch->colour())
            {
                case QLCChannel::Red: v = colour.red(); break;
                case QLCChannel::Green: v = colour.green(); break;
                case QLCChannel::Blue: v = colour.blue(); break;
                case QLCChannel::NoColour: v = 255; break;
                default: v = 0; break;
            }
        }
        else if (ch->group() == QLCChannel::Shutter)
        {
            v = 255;
            for (const QLCCapability *cap : ch->capabilities())
                if (cap->preset() == QLCCapability::ShutterOpen)
                {
                    v = cap->min();
                    break;
                }
        }
        if (v >= 0)
            scene->setValue(fx->id(), quint32(c), uchar(v));
    }
}

bool buildRig(Doc *doc, int copies, Rig *rig, QString *why)
{
    QLCFixtureDefCache *defs = doc->fixtureDefCache();
    QLCFixtureDef *moverDef = defs->fixtureDef("Martin", "ERA 300 Profile");
    QLCFixtureDef *washDef = defs->fixtureDef("American DJ", "Micro Wash RGBW");
    QLCFixtureDef *lanternDef = defs->fixtureDef("WLED", "Lantern");
    QLCFixtureMode *mover = moverDef ? moverDef->mode("21 Channel") : nullptr;
    QLCFixtureMode *wash = washDef ? washDef->mode("7 Channel") : nullptr;
    QLCFixtureMode *lantern = lanternDef ? lanternDef->mode("Effect (15ch)") : nullptr;
    if (mover == nullptr || wash == nullptr || lantern == nullptr)
    {
        *why = QStringLiteral("a fixture definition is missing (ERA 300 / Micro Wash / Lantern)");
        return false;
    }
    RGBAlgorithm *probe = RGBAlgorithm::algorithm(doc, "Plasma");
    if (probe == nullptr)
    {
        *why = QStringLiteral("no Plasma RGB script");
        return false;
    }
    delete probe;

    const QList<QColor> colours { QColor(255, 0, 0), QColor(255, 140, 0), QColor(0, 200, 255),
                                  QColor(120, 0, 255), QColor(255, 255, 255), QColor(0, 255, 80) };
    for (int copy = 0; copy < copies; ++copy)
    {
        const int base = copy * kRigUniverses;
        QList<quint32> pixels, movers, washes, lanterns, dimmers;
        int address = 0;
        auto place = [&](Fixture *fx, int universe, const QString &name) {
            fx->setName(QStringLiteral("%1 (%2)").arg(name).arg(copy + 1));
            // Address first, then universe (setAddress() keeps 7 universe bits)
            fx->setAddress(quint32(address));
            fx->setUniverse(quint32(base + universe));
            if (!doc->addFixture(fx))
            {
                delete fx;
                return quint32(Fixture::invalidId());
            }
            address += int(fx->channels());
            return fx->id();
        };
        auto pixelBar = [&](int universe, const QString &name, int count) {
            Fixture *fx = new Fixture(doc);
            QLCFixtureDef *def = fx->genericRGBPanelDef(count, Fixture::RGBW, false);
            fx->setFixtureDefinition(def, fx->genericRGBPanelMode(def, Fixture::RGBW, false, 600, 60));
            return place(fx, universe, name);
        };
        auto library = [&](QLCFixtureMode *mode, int universe, const QString &name) {
            Fixture *fx = new Fixture(doc);
            fx->setFixtureDefinition(mode->fixtureDef(), mode);
            return place(fx, universe, name);
        };

        for (int u = 0; u < 30; ++u)
        {
            address = 0;
            pixels << pixelBar(u, QStringLiteral("UST %1").arg(u + 1), 96);
        }
        for (int u = 30; u < 40; ++u)
        {
            address = 0;
            for (int i = 0; i < 2; ++i)
                movers << library(mover, u, QStringLiteral("Mover %1").arg(movers.size() + 1));
        }
        for (int u = 40; u < 50; ++u)
        {
            address = 0;
            for (int i = 0; i < 2; ++i)
                washes << library(wash, u, QStringLiteral("Wash %1").arg(washes.size() + 1));
        }
        for (int u = 50; u < 76; ++u)
        {
            address = 0;
            for (int i = 0; i < 3; ++i)
                pixels << pixelBar(u, QStringLiteral("Bar %1").arg(pixels.size() - 29), 42);
        }
        for (int u = 76; u < 78; ++u)
        {
            address = 0;
            for (int i = 0; i < 27; ++i)
                lanterns << library(lantern, u, QStringLiteral("Lantern %1").arg(lanterns.size() + 1));
        }
        address = 0;
        for (int i = 0; i < 12; ++i)
            movers << library(mover, 78, QStringLiteral("Mover %1").arg(movers.size() + 1));
        for (int i = 0; i < 12; ++i)
            washes << library(wash, 78, QStringLiteral("Wash %1").arg(washes.size() + 1));
        address = 0;
        for (int i = 0; i < 34; ++i)
        {
            Fixture *fx = new Fixture(doc);
            fx->setChannels(1);
            dimmers << place(fx, 79, QStringLiteral("House %1").arg(i + 1));
        }

        // Every pixel, a row per bar
        FixtureGroup *group = new FixtureGroup(doc);
        group->setName(QStringLiteral("Pixels (%1)").arg(copy + 1));
        group->setSize(QSize(96, int(pixels.size())));
        for (int row = 0; row < pixels.size(); ++row)
        {
            Fixture *fx = doc->fixture(pixels.at(row));
            for (int h = 0; fx != nullptr && h < fx->heads(); ++h)
                group->assignHead(QLCPoint(h, row), GroupHead(fx->id(), h));
        }
        if (!doc->addFixtureGroup(group))
        {
            delete group;
            *why = QStringLiteral("the pixel group was refused");
            return false;
        }

        RGBMatrix *matrix = new RGBMatrix(doc);
        matrix->setName(QStringLiteral("Pixel Plasma (%1)").arg(copy + 1));
        matrix->setFixtureGroup(group->id());
        matrix->setAlgorithm(RGBAlgorithm::algorithm(doc, "Plasma"));
        matrix->setColor(0, QColor(255, 40, 0));
        matrix->setColor(1, QColor(0, 60, 255));
        matrix->setDuration(200);
        doc->addFunction(matrix);
        rig->running << matrix->id();

        EFX *efx = new EFX(doc);
        efx->setName(QStringLiteral("Mover Circle (%1)").arg(copy + 1));
        efx->setAlgorithm(EFX::Circle);
        efx->setWidth(100);
        efx->setHeight(80);
        efx->setPropagationMode(EFX::Serial);
        for (quint32 id : movers)
        {
            EFXFixture *ef = new EFXFixture(efx);
            ef->setHead(GroupHead(id, 0));
            ef->setMode(EFXFixture::PanTilt);
            efx->addFixture(ef);
        }
        efx->setDuration(4000);
        doc->addFunction(efx);
        rig->running << efx->id();

        auto chaser = [&](const QString &name, const QList<quint32> &fixtures, int steps) {
            Chaser *ch = new Chaser(doc);
            ch->setName(QStringLiteral("%1 (%2)").arg(name).arg(copy + 1));
            ch->setDurationMode(Chaser::PerStep);
            ch->setFadeInMode(Chaser::PerStep);
            ch->setFadeOutMode(Chaser::PerStep);
            ch->setRunOrder(Function::Loop);
            for (int s = 0; s < steps; ++s)
            {
                Scene *look = new Scene(doc);
                look->setName(QStringLiteral("%1 step %2").arg(ch->name()).arg(s + 1));
                for (int i = s; i < fixtures.size(); i += steps)
                    setLook(look, doc->fixture(fixtures.at(i)), colours.at((s + i) % colours.size()));
                doc->addFunction(look);
                ch->addStep(ChaserStep(look->id(), 300, 700, 300));
            }
            doc->addFunction(ch);
            rig->running << ch->id();
        };
        chaser(QStringLiteral("Lantern chase"), lanterns, 4);
        chaser(QStringLiteral("Wash chase"), washes, 3);
        chaser(QStringLiteral("House chase"), dimmers, 6);
    }

    QHash<quint32, int> perUniverse;
    for (Fixture *fx : doc->fixtures())
    {
        rig->channels += int(fx->channels());
        perUniverse[fx->universe()]++;
    }
    rig->fixtures = doc->fixtures().size();
    for (int n : perUniverse)
        if (n <= 2)
            rig->sparse++;
    return true;
}

Doc *newDoc(int universes)
{
    Doc *doc = new Doc(nullptr, universes);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    doc->fixtureDefCache()->loadMap(dir);
    doc->rgbScriptsCache()->load(QDir(INTERNAL_SCRIPTDIR));
    return doc;
}

double processCpuSeconds()
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return double(ru.ru_utime.tv_sec + ru.ru_stime.tv_sec)
           + double(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1.0e6;
}

double loadAverage()
{
    double load[1] = { -1.0 };
    return getloadavg(load, 1) == 1 ? load[0] : -1.0;
}

int performanceCores()
{
#if defined(Q_OS_MACOS)
    int n = 0;
    size_t size = sizeof(n);
    if (sysctlbyname("hw.perflevel0.logicalcpu", &n, &size, nullptr, 0) == 0 && n > 0)
        return n;
#endif
    return QThread::idealThreadCount();
}

double percentile(std::vector<double> v, double p)
{
    if (v.empty())
        return 0.0;
    std::sort(v.begin(), v.end());
    const size_t i = size_t(std::min(double(v.size() - 1), std::ceil(p / 100.0 * double(v.size())) - 1));
    return v[i];
}

const char *priorityName(Universe::ThreadPriority p)
{
    switch (p)
    {
        case Universe::PriorityResponsive: return "qos";
        case Universe::PriorityRealtime: return "rt";
        default: return "standard";
    }
}

const char *grantedName(Universe::ThreadScheduling s)
{
    switch (s)
    {
        case Universe::ThreadsElevated: return "qos";
        case Universe::ThreadsRealtime: return "rt";
        case Universe::ThreadsRefused: return "refused";
        default: return "normal";
    }
}

struct RunResult
{
    QString mode, granted;
    double load = 0;
    int ticks = 0;
    double tickP50 = 0, tickP99 = 0, tickP999 = 0;   ///< tick compute (wall), ms
    double gapP99 = 0;                                ///< frame interval, ms
    double stallP99 = 0;                              ///< compute wall - thread CPU, ms
    double sendP99 = 0, sendP999 = 0;                 ///< tick to sent, ms
    double cpu = 0;                                   ///< process, % of one core
    quint64 writes = 0;
    int late = 0;                                     ///< frames > 1.5 periods apart
    double cpuP99 = 0;                                ///< the timer thread's CPU in a tick, ms
    int spikes = 0, spikesAtStep = 0;                 ///< ticks over 5 ms; of them, at a matrix step
    quint64 mapsLate = 0;
};

RunResult measure(int universes, Universe::ThreadPriority priority, int warmupS, int seconds)
{
    RunResult r;
    r.mode = QLatin1String(priorityName(priority));
    Universe::setThreadPriority(priority);

    std::unique_ptr<Doc> doc(newDoc(universes));
    CountingOutput *out = new CountingOutput(universes);
    UniverseSched_Test::adoptOutput(doc.get(), out);
    Rig rig;
    QString why;
    if (!buildRig(doc.get(), universes / kRigUniverses, &rig, &why))
    {
        qWarning() << why;
        return r;
    }
    InputOutputMap *iom = doc->inputOutputMap();
    for (int u = 0; u < universes; ++u)
        iom->setOutputPatch(quint32(u), out->name(), QString(), quint32(u), false);

    MasterTimer *mt = doc->masterTimer();
    const double period = 1000.0 / MasterTimer::frequency();
    QMutex lock;
    std::vector<double> compute, gaps, stalls, cpus;
    int spikes = 0, spikesAtStep = 0;
    const Function *matrixFn = nullptr;
    for (quint32 id : rig.running)
        if (Function *f = doc->function(id))
            if (f->type() == Function::RGBMatrixType && matrixFn == nullptr)
                matrixFn = f;
    compute.reserve(size_t(seconds + 2) * 60);
    std::atomic<bool> recording { false };
    QElapsedTimer clock;
    clock.start();
    qint64 lastNs = 0;
    QObject::connect(mt, &MasterTimer::tickReady, mt, [&]() {
        const qint64 now = clock.nsecsElapsed();
        const double gap = lastNs ? (now - lastNs) / 1.0e6 : -1.0;
        lastNs = now;
        if (!recording.load(std::memory_order_relaxed))
            return;
        const double comp = mt->tickComputeMs();
        const double cpu = mt->tickCpuMs();
        const Function *mf = matrixFn;
        const bool atStep = mf != nullptr && mf->elapsed() <= 2 * MasterTimer::tick();
        QMutexLocker l(&lock);
        cpus.push_back(cpu);
        if (comp > 5.0)
        {
            spikes++;
            if (atStep)
                spikesAtStep++;
        }
        compute.push_back(comp);
        stalls.push_back(qMax(0.0, comp - cpu));
        if (gap >= 0)
            gaps.push_back(gap);
    }, Qt::DirectConnection);

    doc->setMode(Doc::Operate);
    iom->startUniverses();
    mt->start();
    for (quint32 id : rig.running)
        if (Function *f = doc->function(id))
            f->start(mt, FunctionParent::master());

    QTest::qWait(warmupS * 1000);
    {
        QList<Universe *> ua = iom->claimUniverses();
        for (Universe *u : ua)
            u->resetSendLatency();
        iom->releaseUniverses(false);
    }
    r.load = loadAverage();
    out->m_writes.store(0);
    const quint64 late0 = RGBMatrix::mapsLate();
    const double cpu0 = processCpuSeconds();
    QElapsedTimer wall;
    wall.start();
    recording.store(true);
    QTest::qWait(seconds * 1000);
    recording.store(false);
    const double wallS = wall.elapsed() / 1000.0;
    r.cpu = 100.0 * (processCpuSeconds() - cpu0) / qMax(0.001, wallS);
    r.writes = out->m_writes.load();
    r.mapsLate = RGBMatrix::mapsLate() - late0;
    r.granted = QLatin1String(grantedName(Universe::threadScheduling()));

    std::vector<quint64> send(Universe::kSendLatencyBuckets, 0);
    quint64 frames = 0;
    {
        QList<Universe *> ua = iom->claimUniverses();
        for (Universe *u : ua)
        {
            const QVector<quint32> h = u->sendLatencyHistogram();
            for (int i = 0; i < h.size(); ++i)
            {
                send[size_t(i)] += h.at(i);
                frames += h.at(i);
            }
        }
        iom->releaseUniverses(false);
    }
    auto sendPct = [&](double p) {
        const quint64 want = quint64(std::ceil(double(frames) * p / 100.0));
        quint64 seen = 0;
        for (size_t i = 0; i < send.size(); ++i)
        {
            seen += send[i];
            if (seen >= want && seen > 0)
                return double(i + 1) * Universe::kSendLatencyBucketUs / 1000.0;
        }
        return 0.0;
    };
    r.sendP99 = sendPct(99);
    r.sendP999 = sendPct(99.9);

    mt->stopAllFunctions();
    mt->stop();
    {
        QMutexLocker l(&lock);
        r.ticks = int(compute.size());
        r.tickP50 = percentile(compute, 50);
        r.tickP99 = percentile(compute, 99);
        r.tickP999 = percentile(compute, 99.9);
        r.gapP99 = percentile(gaps, 99);
        r.stallP99 = percentile(stalls, 99);
        r.cpuP99 = percentile(cpus, 99);
        r.spikes = spikes;
        r.spikesAtStep = spikesAtStep;
        for (double g : gaps)
            if (g > period * 1.5)
                r.late++;
    }
    doc.reset();
    return r;
}
}

void UniverseSched_Test::adoptOutput(Doc *doc, QLCIOPlugin *plugin)
{
    doc->ioPluginCache()->m_plugins << plugin;  // owned (and deleted) by the cache
}

void UniverseSched_Test::cleanup()
{
    qunsetenv("QLC_UNIVERSE_SCHED");
    Universe::setThreadPriority(Universe::defaultThreadPriority());
}

void UniverseSched_Test::theSettingIsUsedUnlessTheEnvironmentSaysOtherwise()
{
    qunsetenv("QLC_UNIVERSE_SCHED");
    QVERIFY(!Universe::threadPriorityOverridden());
    QVERIFY(Universe::threadPrioritySupported(Universe::PriorityStandard));
    QVERIFY(Universe::threadPrioritySupported(Universe::defaultThreadPriority()));

    Universe::setThreadPriority(Universe::PriorityStandard);
    QCOMPARE(Universe::threadPriority(), Universe::PriorityStandard);
    Universe::setThreadPriority(Universe::PriorityRealtime);
    QCOMPARE(Universe::threadPriority(),
             Universe::threadPrioritySupported(Universe::PriorityRealtime)
             ? Universe::PriorityRealtime : Universe::PriorityStandard);
    // What a platform can't do falls back to Standard, never to nothing
    Universe::setThreadPriority(Universe::PriorityResponsive);
    QCOMPARE(Universe::threadPriority(),
             Universe::threadPrioritySupported(Universe::PriorityResponsive)
             ? Universe::PriorityResponsive : Universe::PriorityStandard);

    // The environment wins over the setting, both ways
    Universe::setThreadPriority(Universe::PriorityRealtime);
    qputenv("QLC_UNIVERSE_SCHED", "standard");
    QVERIFY(Universe::threadPriorityOverridden());
    QCOMPARE(Universe::threadPriority(), Universe::PriorityStandard);
    Universe::setThreadPriority(Universe::PriorityStandard);
    qputenv("QLC_UNIVERSE_SCHED", "rt");
    QCOMPARE(Universe::threadPriority(), Universe::PriorityRealtime);
    qputenv("QLC_UNIVERSE_SCHED", "qos");
    QCOMPARE(Universe::threadPriority(), Universe::PriorityResponsive);
}

void UniverseSched_Test::aRunningUniverseTakesAChangeAtItsNextFrame()
{
    qunsetenv("QLC_UNIVERSE_SCHED");
    Universe::setThreadPriority(Universe::PriorityStandard);
    std::unique_ptr<Doc> doc(new Doc(nullptr, 1));
    InputOutputMap *iom = doc->inputOutputMap();
    iom->startUniverses();
    MasterTimer *mt = doc->masterTimer();
    mt->start();
    auto cleanupGuard = qScopeGuard([&] { mt->stop(); });

    QTRY_COMPARE_WITH_TIMEOUT(Universe::threadScheduling(), Universe::ThreadsNormal, 5000);

    // Real-time where there is one: taken by the thread already running
    Universe::setThreadPriority(Universe::PriorityRealtime);
    if (Universe::threadPrioritySupported(Universe::PriorityRealtime))
    {
#if defined(Q_OS_MACOS)
        QTRY_COMPARE_WITH_TIMEOUT(Universe::threadScheduling(), Universe::ThreadsRealtime, 5000);
#else
        // Linux grants it only with an rtprio limit: either answer, not "normal"
        QTRY_VERIFY_WITH_TIMEOUT(Universe::threadScheduling() != Universe::ThreadsNormal, 5000);
#endif
    }
    // ...and back
    Universe::setThreadPriority(Universe::PriorityStandard);
    QTRY_COMPARE_WITH_TIMEOUT(Universe::threadScheduling(), Universe::ThreadsNormal, 5000);
}

void UniverseSched_Test::theRigIsTheShowsSize()
{
    std::unique_ptr<Doc> doc(newDoc(kRigUniverses));
    Rig rig;
    QString why;
    QVERIFY2(buildRig(doc.get(), 1, &rig, &why), qPrintable(why));
    QCOMPARE(rig.fixtures, kRigFixtures);
    QCOMPARE(rig.channels, kRigChannels);
    QCOMPARE(rig.sparse, 50);
    QCOMPARE(rig.running.size(), 5);    // matrix, EFX, three chasers
}

void UniverseSched_Test::abOnTheShowsRig_data()
{
    QTest::addColumn<int>("universes");
    const QByteArray list = qgetenv("QLC_SCHED_AB_UNIVERSES");
    const QList<QByteArray> sizes = (list.isEmpty() ? QByteArray("80,160") : list).split(',');
    for (const QByteArray &s : sizes)
    {
        const int n = (qMax(1, s.trimmed().toInt()) + kRigUniverses - 1) / kRigUniverses * kRigUniverses;
        QTest::newRow(qPrintable(QStringLiteral("%1 universes").arg(n))) << n;
    }
}

/* The A/B behind Universe::defaultThreadPriority(): the same rig run with
   the universe threads Standard, Responsive (qos) and Real-time (rt), back
   to back in one process, rounds alternating the order; the 1-minute load
   is read before each run. QLC_SCHED_AB=1 runs it (off in the gate):
   QLC_SCHED_AB_ROUNDS (3), _SECONDS (20 measured), _WARMUP (5),
   _UNIVERSES ("80,160"). Rows go to the log and, with QLC_SHOT_DIR, to
   universesched-<host>.csv there. */
void UniverseSched_Test::abOnTheShowsRig()
{
    if (qgetenv("QLC_SCHED_AB").isEmpty())
        QSKIP("the scheduling A/B: QLC_SCHED_AB=1 (minutes per size)");
    QFETCH(int, universes);
    qunsetenv("QLC_UNIVERSE_SCHED");
    auto setting = [](const char *name, int fallback, int least) {
        const int v = qEnvironmentVariableIntValue(name);
        return qMax(least, v > 0 ? v : fallback);
    };
    const int rounds = setting("QLC_SCHED_AB_ROUNDS", 3, 1);
    const int seconds = setting("QLC_SCHED_AB_SECONDS", 20, 2);
    const int warmup = setting("QLC_SCHED_AB_WARMUP", 5, 1);

    QList<Universe::ThreadPriority> modes { Universe::PriorityStandard };
    if (Universe::threadPrioritySupported(Universe::PriorityResponsive))
        modes << Universe::PriorityResponsive;
    if (Universe::threadPrioritySupported(Universe::PriorityRealtime))
        modes << Universe::PriorityRealtime;

    const QString host = QSysInfo::machineHostName().section('.', 0, 0);
    QFile csv;
    const QString shotDir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shotDir.isEmpty())
    {
        QDir().mkpath(shotDir);
        csv.setFileName(QDir(shotDir).filePath(QStringLiteral("universesched-%1.csv").arg(host)));
        const bool fresh = !csv.exists();
        if (csv.open(QIODevice::Append | QIODevice::Text) && fresh)
            csv.write("host,perf_cores,universes,round,mode,granted,load1,ticks,tick_p50,tick_p99,"
                      "tick_p999,gap_p99,stall_p99,send_p99,send_p999,cpu_pct,late,writes,tick_cpu_p99,spikes,spikes_at_step,maps_late\n");
    }

    QMap<QString, QList<RunResult>> byMode;
    for (int round = 0; round < rounds; ++round)
    {
        QList<Universe::ThreadPriority> order = modes;
        if (round % 2 == 1)
            std::reverse(order.begin(), order.end());
        for (Universe::ThreadPriority p : order)
        {
            const RunResult r = measure(universes, p, warmup, seconds);
            QVERIFY2(r.ticks > seconds * 40, qPrintable(QStringLiteral("%1 ticks").arg(r.ticks)));
            QVERIFY2(r.writes > 0, "nothing went to the output");
            byMode[r.mode] << r;
            const QString line = QStringLiteral(
                "%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22")
                .arg(host).arg(performanceCores()).arg(universes).arg(round + 1).arg(r.mode, r.granted)
                .arg(r.load, 0, 'f', 2).arg(r.ticks)
                .arg(r.tickP50, 0, 'f', 2).arg(r.tickP99, 0, 'f', 2).arg(r.tickP999, 0, 'f', 2)
                .arg(r.gapP99, 0, 'f', 2).arg(r.stallP99, 0, 'f', 2)
                .arg(r.sendP99, 0, 'f', 2).arg(r.sendP999, 0, 'f', 2)
                .arg(r.cpu, 0, 'f', 1).arg(r.late).arg(r.writes)
                .arg(r.cpuP99, 0, 'f', 2).arg(r.spikes).arg(r.spikesAtStep).arg(r.mapsLate);
            qInfo().noquote() << "SCHED_AB" << line;
            if (csv.isOpen())
            {
                csv.write(line.toUtf8() + '\n');
                csv.flush();
            }
        }
    }
    // The medians of each mode's runs, for the log
    for (auto it = byMode.cbegin(); it != byMode.cend(); ++it)
    {
        auto med = [&](double RunResult::*m) {
            std::vector<double> v;
            for (const RunResult &r : it.value())
                v.push_back(r.*m);
            return percentile(v, 50);
        };
        qInfo().noquote() << QStringLiteral("SCHED_AB median %1 universes %2: tick p50 %3 p99 %4 p99.9 %5, "
                                            "stall p99 %6, send p99.9 %7, gap p99 %8, CPU %9%")
                             .arg(universes).arg(it.key())
                             .arg(med(&RunResult::tickP50), 0, 'f', 2).arg(med(&RunResult::tickP99), 0, 'f', 2)
                             .arg(med(&RunResult::tickP999), 0, 'f', 2).arg(med(&RunResult::stallP99), 0, 'f', 2)
                             .arg(med(&RunResult::sendP999), 0, 'f', 2).arg(med(&RunResult::gapP99), 0, 'f', 2)
                             .arg(med(&RunResult::cpu), 0, 'f', 1);
    }
}

/* How much of the tick the RGB matrix is (docs/performance.md), ticked by
   hand so no scheduling is in it: the rig's functions with and without the
   matrix, every tick timed, a matrix step (a new map) every tenth tick.
   QLC_SCHED_AB=1, as the A/B. */
void UniverseSched_Test::matrixShareOfTheTick()
{
    if (qgetenv("QLC_SCHED_AB").isEmpty())
        QSKIP("a measurement: QLC_SCHED_AB=1");
    const int ticks = 1000;
    QStringList lines;
    for (int withMatrix = 1; withMatrix >= 0; --withMatrix)
    {
        std::unique_ptr<Doc> doc(newDoc(kRigUniverses));
        Rig rig;
        QString why;
        QVERIFY2(buildRig(doc.get(), 1, &rig, &why), qPrintable(why));
        MasterTimer *mt = doc->masterTimer();
        doc->setMode(Doc::Operate);
        for (quint32 id : rig.running)
        {
            Function *f = doc->function(id);
            if (f != nullptr && (withMatrix || f->type() != Function::RGBMatrixType))
                f->start(mt, FunctionParent::master());
        }
        std::vector<double> ms;
        QElapsedTimer t;
        for (int i = 0; i < ticks + 50; ++i)
        {
            t.start();
            mt->timerTick();
            const double took = t.nsecsElapsed() / 1.0e6;
            if (i >= 50)
                ms.push_back(took);
        }
        double sum = 0;
        for (double v : ms)
            sum += v;
        const QString line = QStringLiteral("SCHED_AB matrix share %1: tick mean %2 p50 %3 p90 %4 p99 %5 max %6 ms")
            .arg(withMatrix ? "with the matrix" : "without it")
            .arg(sum / ms.size(), 0, 'f', 3).arg(percentile(ms, 50), 0, 'f', 3)
            .arg(percentile(ms, 90), 0, 'f', 3).arg(percentile(ms, 99), 0, 'f', 3)
            .arg(percentile(ms, 100), 0, 'f', 3);
        qInfo().noquote() << line;
        lines << line;
        for (quint32 id : rig.running)
            if (Function *f = doc->function(id))
                f->stop(FunctionParent::master());
        for (int i = 0; i < 20 && mt->runningFunctions() > 0; ++i)
            mt->timerTick();
    }
    const QString shotDir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shotDir.isEmpty())
    {
        QDir().mkpath(shotDir);
        QFile f(QDir(shotDir).filePath(QStringLiteral("matrixshare-%1.txt")
                                       .arg(QSysInfo::machineHostName().section('.', 0, 0))));
        if (f.open(QIODevice::Append | QIODevice::Text))
            f.write(lines.join('\n').toUtf8() + '\n');
    }
}

QTEST_GUILESS_MAIN(UniverseSched_Test)

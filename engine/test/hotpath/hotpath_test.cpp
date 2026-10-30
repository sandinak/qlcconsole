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
#include "efx.h"
#include "efxfixture.h"
#include "collection.h"
#include "powerdistribution.h"
#include "powerlimiter.h"
#include "fixturepower.h"
#include "qlcpalette.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "doc.h"
#undef protected
#undef private

#include "showbuilder.h"

#include "../common/resource_paths.h"

namespace
{
/** Measuring for tools/perf/guard.py (QLC_PERF_REPORT set); otherwise the
 *  gate, where these only have to run */
bool reporting()
{
    return !qEnvironmentVariableIsEmpty("QLC_PERF_REPORT");
}

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

enum CpuClock { ThreadCpu, ProcessCpu };

/** This thread's CPU time in ns: what the work costs itself, whatever else
 *  the machine is doing (ProcessCpu: every thread's, for work handed to
 *  another thread). Wall-clock time where there is no such clock. */
qint64 cpuNs(const QElapsedTimer &wall, CpuClock clock = ThreadCpu)
{
#if defined(Q_OS_UNIX) && defined(CLOCK_THREAD_CPUTIME_ID)
    struct timespec ts;
    if (clock_gettime(clock == ThreadCpu ? CLOCK_THREAD_CPUTIME_ID : CLOCK_PROCESS_CPUTIME_ID, &ts) == 0)
        return qint64(ts.tv_sec) * 1000000000LL + qint64(ts.tv_nsec);
#else
    Q_UNUSED(clock)
#endif
    return wall.nsecsElapsed();
}

/** CPU ns per call of @a op: the best of @a batches batches of @a reps calls
 *  (the least disturbed batch is the closest to the code's own cost). */
double nsPerOp(const std::function<void()> &op, int reps, int batches = 5, CpuClock clock = ThreadCpu)
{
    QElapsedTimer wall;
    wall.start();
    if (!reporting())                                   // the gate: run it, once
        reps = batches = 1;
    else
        for (int i = 0; i < qMax(1, reps / 10); ++i)   // warm the caches
            op();
    double best = -1;
    for (int b = 0; b < batches; ++b)
    {
        const qint64 from = cpuNs(wall, clock);
        for (int i = 0; i < reps; ++i)
            op();
        const double per = double(cpuNs(wall, clock) - from) / reps;
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
    if (m_rig != nullptr)
    {
        stopRig();
        delete m_rig;
        m_rig = nullptr;
    }
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

void HotPath_Test::frame(Doc *doc, int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        doc->masterTimer()->timerTick();
        for (Universe *u : doc->inputOutputMap()->universes())
            u->processFaders();
    }
}

namespace
{
const int kRigUniverses = 32;

/** The rig's fixtures of one kind ("Mover", "Par", "Bar", "Strip",
 *  "Dimmer"), by their names */
QList<Fixture *> rigFixtures(Doc *doc, const QString &kind)
{
    QList<Fixture *> out;
    for (Fixture *fx : doc->fixtures())
        if (fx->name().section(' ', 1, 1) == kind)
            out << fx;
    return out;
}

ShowTree::Target groupTarget(quint32 g)
{
    ShowTree::Target t;
    t.groups << g;
    return t;
}
}

Doc *HotPath_Test::rig()
{
    if (m_rig != nullptr)
        return m_rig;
    m_rig = new Doc(this, kRigUniverses);
    ShowSpec spec;
    spec.rig = ShowSpec::MixedRig;
    spec.universes = kRigUniverses;
    spec.scenes = kRigUniverses;        // a group look per position
    spec.chasers = 0;
    spec.matrices = 8;                  // over the stage groups
    spec.efx = 8;                       // pan/tilt on the movers
    spec.collections = 0;
    spec.cuelists = 0;
    m_rigFunctions = buildShow(m_rig, spec, INTERNAL_FIXTUREDIR, INTERNAL_SCRIPTDIR);
    // The power limiter rebuilds on the event loop; here, now.
    m_rig->powerLimiter()->rebuild();
    return m_rig;
}

void HotPath_Test::stopRig()
{
    for (Function *f : m_rig->functions())
        if (f->isRunning())
            f->stop(FunctionParent::master());
    frame(m_rig, 3);
}

/* A tracking cue list over the rig's stage groups: the first cue sets every
   group, each later one changes one group (so the last cue tracks most of
   what it plays from far back), every tenth cue has two sub-cues, the list
   moves dark heads in black. Built in one batch (one bake). */
quint32 HotPath_Test::bigShowTree(int cues)
{
    if (m_rigTree != 0)
        return m_rigTree;
    Doc *doc = rig();
    ShowTreeController *st = doc->showTree();
    QList<quint32> groups;
    for (FixtureGroup *g : doc->fixtureGroups())
        if (g->name().startsWith("Stage"))
            groups << g->id();
    QList<quint32> palettes;
    for (int i = 0; i < 6; ++i)
    {
        QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, doc);
        dim->setName(QString("Level %1").arg(i));
        dim->setValue(40 * i + 15);
        doc->addPalette(dim);
        QLCPalette *col = new QLCPalette(QLCPalette::Color, doc);
        col->setName(QString("Colour %1").arg(i));
        col->setValue(QLCPalette::colorToString(QColor::fromHsv(60 * i, 255, 255), QColor(Qt::black)));
        doc->addPalette(col);
        QLCPalette *pos = new QLCPalette(QLCPalette::PanTilt, doc);
        pos->setName(QString("Position %1").arg(i));
        pos->setValue(30 * i, 20 + 10 * i);
        doc->addPalette(pos);
        palettes << dim->id() << col->id() << pos->id();
    }
    st->beginBatch("Big show");
    const quint32 prod = st->addProduction("Big show");
    const quint32 listId = st->addCueList(prod, "Act 1");
    st->setListMoveInBlack(listId, ShowTree::MoveInBlack::On);
    for (int i = 0; i < cues; ++i)
    {
        const quint32 c = st->addCue(listId, i - 1, QString("Cue %1").arg(i + 1));
        if (i == 0)
        {
            for (int g = 0; g < groups.size(); ++g)
                st->setTargetLook(c, groupTarget(groups.at(g)), st->lookForPalette(palettes.at(g % palettes.size())));
        }
        else
        {
            st->setTargetLook(c, groupTarget(groups.at(i % groups.size())),
                              st->lookForPalette(palettes.at((i * 7) % palettes.size())));
        }
        if (i % 10 == 5)
        {
            const quint32 s1 = st->addSubCue(c, "Hit");
            const quint32 s2 = st->addSubCue(c, "Out");
            st->setTargetLook(s1, groupTarget(groups.at((i + 1) % groups.size())),
                              st->lookForPalette(palettes.at(i % palettes.size())));
            st->setTargetLook(s2, groupTarget(groups.at((i + 2) % groups.size())),
                              st->lookForPalette(palettes.at((i + 3) % palettes.size())));
        }
    }
    st->endBatch(true);
    m_rigTree = st->cueList(listId)->chaser;
    m_rigTreeList = listId;
    return m_rigTree;
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
 * The mixed rig
 *****************************************************************************/

/* An EFX on moving heads drives pan and tilt with 16-bit precision: the fine
   channels move too. A position EFX that wrote only the coarse bytes would
   step a 540-degree pan in 2-degree jumps. */
void HotPath_Test::aPanTiltEfxWritesBothBytesOfEveryMover()
{
    Doc *doc = rig();
    const QList<Fixture *> movers = rigFixtures(doc, "Mover");
    QCOMPARE(movers.size(), kRigUniverses * 4);
    Fixture *mover = movers.first();
    const quint32 pan = mover->channelNumber(QLCChannel::Pan, QLCChannel::MSB);
    const quint32 panFine = mover->channelNumber(QLCChannel::Pan, QLCChannel::LSB);
    const quint32 tiltFine = mover->channelNumber(QLCChannel::Tilt, QLCChannel::LSB);
    QVERIFY(pan != QLCChannel::invalid() && panFine != QLCChannel::invalid() && tiltFine != QLCChannel::invalid());

    EFX *efx = new EFX(doc);
    efx->setAlgorithm(EFX::Circle);
    efx->setDuration(2000);
    for (int i = 0; i < 8; ++i)
    {
        EFXFixture *ef = new EFXFixture(efx);
        ef->setHead(GroupHead(movers.at(i)->id(), 0));
        ef->setMode(EFXFixture::PanTilt);
        efx->addFixture(ef);
    }
    doc->addFunction(efx);
    efx->start(doc->masterTimer(), FunctionParent::master());
    Universe *uni = doc->inputOutputMap()->universes().at(int(mover->universe()));
    QSet<int> coarse, fine, tilts;
    for (int i = 0; i < 100; ++i)
    {
        frame(doc);
        coarse << uchar(uni->preGMValue(int(mover->address() + pan)));
        fine << uchar(uni->preGMValue(int(mover->address() + panFine)));
        tilts << uchar(uni->preGMValue(int(mover->address() + tiltFine)));
    }
    efx->stop(FunctionParent::master());
    frame(doc, 3);
    doc->deleteFunction(efx->id());
    QVERIFY2(coarse.size() > 10, qPrintable(QString("pan coarse took %1 values").arg(coarse.size())));
    QVERIFY2(fine.size() > 10, qPrintable(QString("pan fine took %1 values").arg(fine.size())));
    QVERIFY2(tilts.size() > 10, qPrintable(QString("tilt fine took %1 values").arg(tilts.size())));
}

/* The status bar's power estimate over 960 fixtures of five kinds: dark,
   every fixture its standby (a mover's motors and fans, an LED driver's
   idle, a measured par's own standby); at full, the rated draws -- the pars
   held to their cap, the guessed ones at their guess. */
void HotPath_Test::thePowerEstimateOfABigRigAddsUp()
{
    Doc *doc = rig();
    QCOMPARE(doc->fixtures().size(), kRigUniverses * ShowSpec::mixedFixturesPerUni);
    const QList<Fixture *> movers = rigFixtures(doc, "Mover");
    const QList<Fixture *> pars = rigFixtures(doc, "Par");
    const QList<Fixture *> bars = rigFixtures(doc, "Bar");
    const QList<Fixture *> strips = rigFixtures(doc, "Strip");
    const QList<Fixture *> dimmers = rigFixtures(doc, "Dimmer");
    QCOMPARE(movers.size() + pars.size() + bars.size() + strips.size() + dimmers.size(),
             int(doc->fixtures().size()));

    const double moverW = movers.first()->fixtureMode()->physical().powerConsumption();
    QVERIFY(moverW > 100);
    double barW = bars.first()->fixtureMode()->physical().powerConsumption();
    if (barW <= 0)
        barW = PowerEstimator::FallbackFixtureWatts;
    const double parV = 208.0;                     // the rig's switch
    const PowerProfile pp = pars.first()->powerProfile();
    QVERIFY(pp.maxAmps > 0 && pp.maxAmps < pp.fullAmps);

    QHash<quint32, double> per;
    const double dark = PowerEstimator::estimateWatts(doc, [](quint32, quint32) { return uchar(0); }, &per);
    QCOMPARE(per.size(), int(doc->fixtures().size()));
    const double darkWant = movers.size() * moverW * PowerEstimator::MoverStandbyFraction
                          + pars.size() * pp.standbyAmps * parV
                          + (bars.size() + strips.size() + dimmers.size()) * PowerEstimator::NonMoverStandbyWatts;
    QVERIFY2(qAbs(dark - darkWant) < 0.5, qPrintable(QString("dark %1 W, want %2 W").arg(dark).arg(darkWant)));

    const double full = PowerEstimator::estimateWatts(doc, [](quint32, quint32) { return uchar(255); });
    const double fullWant = movers.size() * moverW + pars.size() * pp.maxAmps * parV
                          + bars.size() * barW + strips.size() * PowerEstimator::FallbackFixtureWatts
                          + dimmers.size() * PowerEstimator::FallbackWattsPerDimmerChannel;
    QVERIFY2(qAbs(full - fullWant) < 0.5, qPrintable(QString("full %1 W, want %2 W").arg(full).arg(fullWant)));

    // Grouped by type: five rows (the strips share one generated definition
    // name), counts adding up to the rig.
    QHash<quint32, double> amps;
    for (auto it = per.constBegin(); it != per.constEnd(); ++it)
        amps.insert(it.key(), it.value() / 120.0);
    const QList<PowerEstimator::TypeLoad> rows = PowerEstimator::byType(doc, per, amps);
    int counted = 0;
    for (const PowerEstimator::TypeLoad &r : rows)
        counted += r.count;
    QCOMPARE(counted, int(doc->fixtures().size()));
}

/* Every par of every universe asks for more than its cap at full: each is
   held to it, on what goes out, every frame. */
void HotPath_Test::theCapHoldsEveryParOfABigRig()
{
    Doc *doc = rig();
    const QList<Fixture *> pars = rigFixtures(doc, "Par");
    QCOMPARE(doc->powerLimiter()->limitedCount(), pars.size());
    Scene *full = new Scene(doc);
    for (Fixture *fx : pars)
        for (quint32 c = 0; c < fx->channels(); ++c)
            full->setValue(fx->id(), c, 255);
    full->setFadeInSpeed(0);
    doc->addFunction(full);
    full->start(doc->masterTimer(), FunctionParent::master());
    frame(doc, 3);
    const double cap = pars.first()->powerProfile().maxAmps;
    for (int u = 0; u < kRigUniverses; ++u)
    {
        Universe *uni = doc->inputOutputMap()->universes().at(u);
        const QHash<quint32, QPair<double, double>> held = uni->powerHeldBack();
        QCOMPARE(held.size(), 8);
        for (auto it = held.constBegin(); it != held.constEnd(); ++it)
        {
            QVERIFY(it.value().first > cap);
            QVERIFY2(it.value().second <= cap + 0.01,
                     qPrintable(QString("fixture %1 held to %2 A").arg(it.key()).arg(it.value().second)));
        }
        // What goes out is scaled; what is stored is not.
        Fixture *par = pars.at(u * 8);
        QCOMPARE(uchar(uni->preGMValue(int(par->address() + 1))), uchar(255));
        QVERIFY(uchar(uni->m_lastLimited.at(int(par->address() + 1))) < 255);
    }
    full->stop(FunctionParent::master());
    frame(doc, 3);
    doc->deleteFunction(full->id());
}

/* GO in a 200-cue tracking list over the rig: the step plays what the bake
   made -- nothing is built or deleted on GO, so its cost does not grow with
   the show. */
void HotPath_Test::aGoInABigShowTreeMakesAndDeletesNothing()
{
    Doc *doc = rig();
    const quint32 chaserId = bigShowTree(200);
    Chaser *list = qobject_cast<Chaser *>(doc->function(chaserId));
    QVERIFY(list != nullptr);
    QCOMPARE(list->stepsCount(), 200);
    int made = 0, deleted = 0;
    QMetaObject::Connection a = connect(doc, &Doc::functionAdded, this, [&made](quint32) { ++made; });
    QMetaObject::Connection r = connect(doc, &Doc::functionRemoved, this, [&deleted](quint32) { ++deleted; });
    ChaserAction go;
    go.m_action = ChaserNextStep;
    go.m_stepIndex = -1;
    go.m_masterIntensity = 1.0;
    go.m_stepIntensity = 1.0;
    go.m_fadeMode = Chaser::FromFunction;
    list->start(doc->masterTimer(), FunctionParent::master());
    frame(doc, 2);
    for (int i = 0; i < 60; ++i)
    {
        list->setAction(go);
        frame(doc, 2);
    }
    QVERIFY(list->isRunning());
    QCOMPARE(list->currentStepIndex(), 60);
    QCOMPARE(made, 0);
    QCOMPARE(deleted, 0);
    disconnect(a);
    disconnect(r);
    stopRig();
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


/* The mixed rig's costs, report only (QLC_PERF_REPORT, as cpuPerOperation);
   tools/perf/guard.py runs it as the point "hotpath-rig". */
void HotPath_Test::cpuPerRigOperation()
{
    const int scale = qMax(1, qEnvironmentVariableIntValue("QLC_PERF_REPS"));
    // QLC_PERF_ONLY=<prefix>: only the operations named so (to profile one)
    const QString only = qEnvironmentVariable("QLC_PERF_ONLY");
    auto want = [&only](const char *name) { return only.isEmpty() || QString(name).startsWith(only); };
    QList<QPair<QString, double>> results;
    Doc *doc = rig();
    stopRig();

    // 1. A whole engine frame of the rig: a group look on every position
    //    (14,720 channels, 256 pars held to their cap).
    if (want("rig_frame"))
    {
        for (Function *f : m_rigFunctions)
            if (f->type() == Function::SceneType)
                f->start(doc->masterTimer(), FunctionParent::master());
        frame(doc, 3);
        results << qMakePair(QStringLiteral("rig_frame_32u_looks"), nsPerOp([doc]() { frame(doc); }, 20 * scale));
        stopRig();
    }

    // 2. Pan/tilt EFX on 64 movers (16-bit), alone.
    if (want("efx_pantilt"))
    {
        const QList<Fixture *> movers = rigFixtures(doc, "Mover");
        EFX *efx = new EFX(doc);
        efx->setAlgorithm(EFX::Eight);
        efx->setDuration(3000);
        efx->setPropagationMode(EFX::Serial);
        for (int i = 0; i < 64; ++i)
        {
            EFXFixture *ef = new EFXFixture(efx);
            ef->setHead(GroupHead(movers.at(i)->id(), 0));
            ef->setMode(EFXFixture::PanTilt);
            efx->addFixture(ef);
        }
        doc->addFunction(efx);
        efx->start(doc->masterTimer(), FunctionParent::master());
        frame(doc, 3);
        results << qMakePair(QStringLiteral("efx_pantilt_frame_64movers"),
                             nsPerOp([doc]() { frame(doc); }, 50 * scale));
        stopRig();
        doc->deleteFunction(efx->id());
    }

    // 3. A matrix over a stage-wide pixel grid (120 x 8, 864 heads on 8
    //    universes, the gaps dark), a new step every frame. The script
    //    runs on its own thread: this one's CPU is the process's.
    if (want("matrix_stage"))
    {
        FixtureGroup *grid = nullptr;
        for (FixtureGroup *g : doc->fixtureGroups())
            if (g->name() == "Stage pixels 1")
                grid = g;
        QVERIFY(grid != nullptr);
        QCOMPARE(grid->headsMap().size(), 8 * (4 * 12 + 2 * 30));
        RGBMatrix *mtx = new RGBMatrix(doc);
        mtx->setFixtureGroup(grid->id());
        mtx->setAlgorithm(RGBAlgorithm::algorithm(doc, "Plasma"));
        mtx->setDuration(MasterTimer::tick());
        doc->addFunction(mtx);
        mtx->start(doc->masterTimer(), FunctionParent::master());
        frame(doc, 30);                      // the script warmed up
        results << qMakePair(QStringLiteral("matrix_stage_frame_120x8"),
                             nsPerOp([doc]() { frame(doc); }, 20 * scale, 5, ProcessCpu));
        stopRig();
        doc->deleteFunction(mtx->id());
    }

    // 4. The power estimate the status bar makes (twice a second in Design
    //    mode while values change): every fixture, then the rows by type.
    if (want("power_estimate"))
    {
        QList<Universe *> ua = doc->inputOutputMap()->universes();
        PowerEstimator::PreGMReader reader = [&ua](quint32 uni, quint32 addr) -> uchar {
            return uni < quint32(ua.size()) ? ua[int(uni)]->preGMValue(int(addr)) : 0;
        };
        results << qMakePair(QStringLiteral("power_estimate_960fx"), nsPerOp([doc, &reader]() {
            QHash<quint32, double> per;
            PowerEstimator::estimateWatts(doc, reader, &per);
            PowerEstimator::byType(doc, per, per);
        }, 10 * scale));
    }

    // 5. One universe's frame with its 8 pars held to their cap (applyPowerLimits).
    if (want("universe_frame_capped"))
    {
        Universe *uni = doc->inputOutputMap()->universes().at(0);
        const QList<Fixture *> pars = rigFixtures(doc, "Par");
        results << qMakePair(QStringLiteral("universe_frame_capped"), nsPerOp([uni, &pars]() {
            for (int i = 0; i < 8; ++i)
                for (quint32 c = 0; c < 5; ++c)
                    uni->write(int(pars.at(i)->address() + c), 255);
            uni->processFaders();
        }, 200 * scale));
    }

    // 6. Show Tree, 200 tracking cues over the stage groups: GO under load
    //    (the rig's looks running), an edit (re-bake + undo step), and
    //    working out what every cue plays (tracking resolved).
    if (want("showtree"))
    {
        const quint32 chaserId = bigShowTree(200);
        ShowTreeController *st = doc->showTree();
        Chaser *list = qobject_cast<Chaser *>(doc->function(chaserId));
        QVERIFY(list != nullptr);
        for (Function *f : m_rigFunctions)
            if (f->type() == Function::SceneType)
                f->start(doc->masterTimer(), FunctionParent::master());
        list->start(doc->masterTimer(), FunctionParent::master());
        frame(doc, 3);
        ChaserAction go;
        go.m_action = ChaserNextStep;
        go.m_stepIndex = -1;
        go.m_masterIntensity = 1.0;
        go.m_stepIntensity = 1.0;
        go.m_fadeMode = Chaser::FromFunction;
        results << qMakePair(QStringLiteral("showtree_go_200cues"), nsPerOp([doc, list, &go]() {
            list->setAction(go);
            frame(doc);
        }, 20 * scale));
        stopRig();

        const ShowTree::CueList *cl = st->cueList(m_rigTreeList);
        QVERIFY(cl != nullptr && cl->cues.size() == 200);
        const quint32 last = cl->cues.last().id;
        ShowTree::ResolvedCue rc;
        results << qMakePair(QStringLiteral("showtree_resolve_200cues"), nsPerOp([st, last, &rc]() {
            st->resolvedCue(last, &rc);
        }, 5 * scale, 3));

        const quint32 mid = cl->cues.at(100).id;
        QList<quint32> stage;
        for (FixtureGroup *g : doc->fixtureGroups())
            if (g->name().startsWith("Stage pars"))
                stage << g->id();
        QList<quint32> levels;
        for (QLCPalette *p : doc->palettes())
            if (p->type() == QLCPalette::Dimmer)
                levels << p->id();
        int n = 0;
        results << qMakePair(QStringLiteral("showtree_edit_200cues"), nsPerOp([st, mid, &stage, &levels, &n]() {
            st->setTargetLook(mid, groupTarget(stage.first()), st->lookForPalette(levels.at(n++ % levels.size())));
        }, 2 * scale, 3));
    }

    QStringList table;
    for (const auto &r : results)
        table << QString("%1 %2 ns").arg(r.first, -26).arg(r.second, 10, 'f', 0);
    qInfo().noquote() << "Mixed rig, CPU time per operation (this thread, best batch):\n  " + table.join("\n  ");

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

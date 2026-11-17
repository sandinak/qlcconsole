/*
  Q Light Controller Plus
  firespreadperf_test.cpp

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
#include <QApplication>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QDir>
#if defined(Q_OS_UNIX)
#include <time.h>
#endif

#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "app.h"
#include "structurestudioview.h"
#include "monitorgraphicsview.h"
#include "monitor.h"
#include "effectinstance.h"
#undef private
#undef protected
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "monitorproperties.h"
#include "effectinstance.h"
#include "effectruns.h"
#include "qlcpalette.h"
#include "scenery.h"
#include "fixture.h"
#include "scene.h"
#include "fixturegroup.h"
#include "effectscriptrunner.h"
#include "doc.h"

#include "firespreadperf_test.h"

static QStringList g_report;
#define REPORT(...) do { const QString l = QString::asprintf(__VA_ARGS__); qInfo("%s", qPrintable(l)); g_report << l; } while (0)

namespace
{
/** The process's CPU time in ns: the view renders on a thread pool, and
 *  every thread's work is the frame's cost */
qint64 cpuNs(const QElapsedTimer &wall)
{
#if defined(Q_OS_UNIX) && defined(CLOCK_PROCESS_CPUTIME_ID)
    struct timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0)
        return qint64(ts.tv_sec) * 1000000000LL + qint64(ts.tv_nsec);
#endif
    return wall.nsecsElapsed();
}

/* The kinds of light Branson put under the fire: the steps' pixel rows,
   the wall washers on the step tops, the pixel bars, the pars. */
bool underTheFire(const Fixture *fx)
{
    const QLCFixtureDef *def = fx->fixtureDef();
    if (def == nullptr)
        return false;
    static const QStringList models {
        QStringLiteral("Step Row 64 Heads"), QStringLiteral("Wall Washer Light Bar"),
        QStringLiteral("PL- 4003 1 Bar"), QStringLiteral("60W RGB Pixel Light Bar"),
        QStringLiteral("Best Par 50") };
    return models.contains(def->model());
}

void settle(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}
}

void FireSpreadPerf_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    m_app->doc()->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures"));
    m_app->clearDocument();
    /* FIRE_PERF_SHOW: a show read where it is (never copied), its Fire
       Spread on the groups FIRE_PERF_GROUPS names (comma-separated) and
       its uplights (FIRE_PERF_UPLIGHTS) measured lit and dark. */
    const QString show = qEnvironmentVariable("FIRE_PERF_SHOW", QStringLiteral(STAGE_SHOW));
    QCOMPARE(m_app->loadXML(show), QFile::NoError);
    Doc *doc = m_app->doc();
    doc->resetModified();
    // The main drape open, as the stage is looked at (no curtain fixture driving it).
    foreach (Scenery *sc, doc->monitorProperties()->sceneries())
        if (sc != nullptr && sc->hasState())
        {
            sc->setDriverFixture(Scenery::invalidId());
            sc->setState(Scenery::Open);
        }

    Scene *look = new Scene(doc);
    QVERIFY(doc->addFunction(look));
    int heads = 0, fixtures = 0;
    const QStringList groups = qEnvironmentVariable("FIRE_PERF_GROUPS").split(QLatin1Char(','), Qt::SkipEmptyParts);
    const QStringList uplights = qEnvironmentVariable("FIRE_PERF_UPLIGHTS").split(QLatin1Char(','), Qt::SkipEmptyParts);
    QSet<quint32> chosen;
    for (FixtureGroup *g : doc->fixtureGroups())
    {
        if (groups.contains(g->name()) || uplights.contains(g->name()))
            for (quint32 id : g->fixtureList())
                chosen << id;
        if (uplights.contains(g->name()))
            for (quint32 id : g->fixtureList())
                m_uplights << id;
    }
    for (Fixture *fx : doc->fixtures())
    {
        fx->setOwnChannelValues(QByteArray(int(fx->channels()), 0));
        if (groups.isEmpty() ? !underTheFire(fx) : !chosen.contains(fx->id()))
            continue;
        look->addFixture(fx->id());
        m_lookFixtures << fx->id();
        heads += fx->heads();
        ++fixtures;
    }
    QLCPalette *fire = new QLCPalette(QLCPalette::Effect, doc);
    fire->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/fire-spread.js");
    // His: Edge 0.354, the default 8 s spread, looping (it keeps burning).
    fire->setEffectParamValue(QStringLiteral("edge"), 0.354);
    fire->setEffectParamValue(QStringLiteral("seed"), 327.0);
    fire->setEffectParamValue(EffectRuns::runsKey(), 0);
    QVERIFY(doc->addPalette(fire));
    look->addPalette(fire->id());
    m_fire = new EffectInstance(doc, look->id(), fire->id());
    QVERIFY2(m_fire->isValid(), qPrintable(m_fire->error()));
    REPORT("Fire Spread over %d fixtures, %d heads", fixtures, heads);
    REPORT("(%d of them uplights)", int(m_uplights.size()));
    QVERIFY(heads > 5000);
}

void FireSpreadPerf_Test::cleanupTestCase()
{
    delete m_fire;
    m_fire = nullptr;
    if (m_app != nullptr)
    {
        m_app->doc()->resetModified();
        delete m_app;
        m_app = nullptr;
    }
}

int FireSpreadPerf_Test::tickTo(qint64 ms, double *tickMs, double *applyMs)
{
    QElapsedTimer wall;
    wall.start();
    m_fire->setOffStageClock(ms);
    const qint64 t0 = wall.nsecsElapsed();
    m_fire->runTick();
    const QList<EffectInstance::DmxWrite> writes = m_fire->dmxWrites();
    const qint64 t1 = wall.nsecsElapsed();
    for (auto it = m_universes.begin(); it != m_universes.end(); ++it)
        it.value().fill(char(0));
    for (const EffectInstance::DmxWrite &w : writes)
    {
        QByteArray &u = m_universes[w.universeId];
        if (u.isEmpty())
            u = QByteArray(512, char(0));
        if (w.addr >= 0 && w.addr < 512)
            u[w.addr] = char(w.value);
    }
    int changed = 0;
    for (Fixture *fx : m_app->doc()->fixtures())
    {
        if (m_uplightsDark && m_uplights.contains(fx->id()))
        {
            fx->setOwnChannelValues(QByteArray(int(fx->channels()), 0));
            continue;
        }
        const auto it = m_universes.constFind(int(fx->universe()));
        if (it != m_universes.constEnd() && fx->setChannelValues(it.value()))
            ++changed;
    }
    const qint64 t2 = wall.nsecsElapsed();
    if (tickMs)
        *tickMs = (t1 - t0) / 1e6;
    if (applyMs)
        *applyMs = (t2 - t1) / 1e6;
    return changed;
}

void FireSpreadPerf_Test::costsWhileTheFireSpreads()
{
    if (qEnvironmentVariableIsEmpty("FIRE_PERF") && qEnvironmentVariableIsEmpty("FIRE_PERF_SHOW"))
        QSKIP("the full measurement runs by hand: FIRE_PERF=1 (or FIRE_PERF_SHOW=<show>)");
    Doc *doc = m_app->doc();
    const bool moveSpeed = StructureStudioView::moveSpeed();
    StructureStudioView::setMoveSpeed(false);
    auto restore = qScopeGuard([moveSpeed]() { StructureStudioView::setMoveSpeed(moveSpeed); });

    // (a) The effect's own tick, and (c) the values reaching the fixtures.
    tickTo(0);
    double spreadTick = 0, spreadApply = 0, burnTick = 0, burnApply = 0;
    int spreadChanged = 0, burnChanged = 0, n = 0;
    for (qint64 ms = 1000; ms <= 7000; ms += 500, ++n)
    {
        double tk = 0, ap = 0;
        spreadChanged += tickTo(ms, &tk, &ap);
        spreadTick += tk;
        spreadApply += ap;
    }
    spreadTick /= n; spreadApply /= n; spreadChanged /= n;
    n = 0;
    for (qint64 ms = 12000; ms <= 15000; ms += 500, ++n)
    {
        double tk = 0, ap = 0;
        burnChanged += tickTo(ms, &tk, &ap);
        burnTick += tk;
        burnApply += ap;
    }
    burnTick /= n; burnApply /= n; burnChanged /= n;
    REPORT("effect tick: spreading %.2f ms, burning %.2f ms", spreadTick, burnTick);
    REPORT("values to fixtures (no view): spreading %.2f ms (%d fixtures changed), burning %.2f ms (%d)",
          spreadApply, spreadChanged, burnApply, burnChanged);

    // (c) with the Lighting Studio's 2D plot listening to every fixture.
    {
        // The App's own Lighting Studio (one at a time), its plot open.
        QWidget parent;
        Monitor *mon = Monitor::instance();
        QScopedPointer<Monitor> made;
        if (mon == nullptr)
        {
            made.reset(new Monitor(&parent, doc));
            mon = made.data();
        }
        QWidget *shown = mon->window();
        shown->resize(1000, 700);
        shown->show();
        QVERIFY(QTest::qWaitForWindowExposed(shown));
        QVERIFY(mon->m_graphicsView != nullptr);
        settle(200);
        double sp = 0, bu = 0;
        int k = 0;
        for (qint64 ms = 1000; ms <= 7000; ms += 1000, ++k)
        {
            double ap = 0;
            tickTo(ms, nullptr, &ap);
            sp += ap;
        }
        sp /= k;
        k = 0;
        for (qint64 ms = 12000; ms <= 15000; ms += 1000, ++k)
        {
            double ap = 0;
            tickTo(ms, nullptr, &ap);
            bu += ap;
        }
        bu /= k;
        REPORT("values to fixtures with the 2D plot open: spreading %.2f ms, burning %.2f ms", sp, bu);
    }

    // (b) A frame of the Rig Overview (45°, centre stage, beams, Live).
    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.setLocked(true);
    v.setLiveValues(true);
    v.setAmbient(0.0);
    v.setPlane(StructureStudioView::Angled);
    v.setStandOnCentreStage(true);
    v.resize(1440, 900);
    v.reload();
    v.show();
    QVERIFY(QTest::qWaitForWindowExposed(&v));
    v.setLiveValues(false);                 // the frames are drawn here, by hand
    settle(100);

    QElapsedTimer wall;
    wall.start();
    auto frameMs = [&](qint64 ms) {
        tickTo(ms);
        v.m_liveDirty = true;
        v.m_valueFrame = true;              // as a live value frame is drawn
        v.m_lastFrameAt.start();
        const qint64 c0 = cpuNs(wall);
        const qint64 w0 = wall.nsecsElapsed();
        v.repaint();
        return QPair<double, double>((cpuNs(wall) - c0) / 1e6, (wall.nsecsElapsed() - w0) / 1e6);
    };
    for (int pass = 0; pass < (m_uplights.isEmpty() ? 2 : 3); ++pass)
    {
        const bool beams = pass != 1;
        m_uplightsDark = pass == 2;
        v.setBeams(beams);
        frameMs(500);                       // warm
        double sc = 0, sw = 0, bc = 0, bw = 0;
        int k = 0;
        for (qint64 ms = 1000; ms <= 7000; ms += 2000, ++k)
        {
            const auto f = frameMs(ms);
            sc += f.first; sw += f.second;
        }
        sc /= k; sw /= k;
        k = 0;
        for (qint64 ms = 12000; ms <= 14000; ms += 2000, ++k)
        {
            const auto f = frameMs(ms);
            bc += f.first; bw += f.second;
        }
        bc /= k; bw /= k;
        REPORT("rig overview frame, beams %s%s: spreading %.1f ms CPU (%.1f wall), burning %.1f ms CPU (%.1f wall)",
              beams ? "on" : "off", m_uplightsDark ? ", uplights dark" : "", sc, sw, bc, bw);
    }
    m_uplightsDark = false;
    v.setLiveValues(false);

    /* (a) in its parts: the script's own tick, the rest of the instance's
       tick (the heads in, the answers out), the publish, a GC. */
    {
        EffectInstance *in = m_fire;
        double cellsMs = 0, buildMs = 0, callMs = 0, parseMs = 0, gcMs = 0;
        int k = 0;
        for (qint64 ms = 1000; ms <= 7000; ms += 500, ++k)
        {
            in->setOffStageClock(ms);
            QElapsedTimer t;
            t.start();
            int cols = 1, rows = 1;
            const QList<EffectInstance::EffectCell> cells = in->effectiveCells(cols, rows);
            const qint64 a = t.nsecsElapsed();
            QJSValue fixtures = in->buildFixturesArray(cells, cols, rows);
            QJSValue inputs = in->buildInputsObject();
            QJSValue palettes = in->buildPalettesObject();
            QJSValue params = in->buildParamsObject();
            QJSValue data = in->buildDataChannelsObject();
            const qint64 b = t.nsecsElapsed();
            QJSValue result = in->m_script.callTick(fixtures, inputs, palettes, params, in->m_state, data);
            const qint64 c = t.nsecsElapsed();
            const QList<EffectInstance::DmxWrite> w = in->parseIntents(result, cells);
            const qint64 d = t.nsecsElapsed();
            in->collectGarbage();
            const qint64 e = t.nsecsElapsed();
            cellsMs += a / 1e6; buildMs += (b - a) / 1e6; callMs += (c - b) / 1e6;
            parseMs += (d - c) / 1e6; gcMs += (e - d) / 1e6;
            QVERIFY(!w.isEmpty());
        }
        REPORT("spreading tick in parts: cells %.2f, heads in %.2f, script %.2f, answers out %.2f, a GC %.2f ms",
               cellsMs / k, buildMs / k, callMs / k, parseMs / k, gcMs / k);
    }

    /* (a) as the app runs it: the look playing, the MasterTimer on its own
       thread, and the effect runner's tick -- every effect's script, then
       its writes published -- on this (the GUI) thread, where the rig view
       paints too. A 2 s spread, then the fire burning. */
    {
        QLCPalette *fast = new QLCPalette(QLCPalette::Effect, doc);
        fast->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/fire-spread.js");
        fast->setEffectParamValue(QStringLiteral("edge"), 0.354);
        fast->setEffectParamValue(QStringLiteral("seed"), 327.0);
        fast->setEffectParamValue(QStringLiteral("spreadTime"), 2.0);
        fast->setEffectParamValue(EffectRuns::runsKey(), 0);
        QVERIFY(doc->addPalette(fast));
        Scene *look = new Scene(doc);
        QVERIFY(doc->addFunction(look));
        for (quint32 fid : std::as_const(m_lookFixtures))
            look->addFixture(fid);
        look->addPalette(fast->id());
        EffectScriptRunner *runner = doc->effectScriptRunner();
        QVERIFY(runner != nullptr);
        look->start(doc->masterTimer(), FunctionParent::master());
        QTRY_VERIFY_WITH_TIMEOUT(runner->instanceForPalette(fast->id()) != nullptr, 3000);
        runner->m_tickTimer.stop();         // its ticks by hand, timed
        QElapsedTimer run;
        run.start();
        double sp = 0, spMax = 0, bu = 0, buMax = 0;
        int spN = 0, buN = 0;
        while (run.elapsed() < 3200)
        {
            const qint64 at = run.elapsed();
            QElapsedTimer f;
            f.start();
            runner->slotTick();
            const double ms = f.nsecsElapsed() / 1e6;
            if (at > 200 && at < 1900) { sp += ms; spMax = qMax(spMax, ms); ++spN; }
            else if (at > 2500) { bu += ms; buMax = qMax(buMax, ms); ++buN; }
            while (f.elapsed() < 20)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
        }
        REPORT("effect runner tick (script + publish + GC, GUI thread): spreading %.2f ms (max %.2f, %d ticks), burning %.2f ms (max %.2f, %d ticks)",
               spN ? sp / spN : 0.0, spMax, spN, buN ? bu / buN : 0.0, buMax, buN);
        runner->m_tickTimer.start();
        look->stop(FunctionParent::master());
        settle(300);
    }

    if (qEnvironmentVariableIsSet("FIRE_PERF_REPORT"))
        QFAIL(qPrintable(g_report.join(QStringLiteral(" | "))));
}

/* Before the front reaches them, Fire Spread leaves thousands of heads to
   what is under them: their answers say nothing, and reading one costs
   next to nothing -- not a head's channels looked up for each, as a burning
   head's are. Old code: an unreached head cost about half a burning one. */
void FireSpreadPerf_Test::anUnreachedHeadCostsNothingToRead()
{
    EffectInstance *in = m_fire;
    auto answersAt = [in](qint64 ms, QList<EffectInstance::EffectCell> &cells, QJSValue &result) {
        in->setOffStageClock(ms);
        int cols = 1, rows = 1;
        cells = in->effectiveCells(cols, rows);
        QJSValue fixtures = in->buildFixturesArray(cells, cols, rows);
        result = in->m_script.callTick(fixtures, in->buildInputsObject(), in->buildPalettesObject(),
                                       in->buildParamsObject(), in->m_state, in->buildDataChannelsObject());
    };
    auto readMs = [in](const QJSValue &result, const QList<EffectInstance::EffectCell> &cells, int *writes) {
        double best = -1;
        for (int i = 0; i < 5; ++i)
        {
            QElapsedTimer t;
            t.start();
            const QList<EffectInstance::DmxWrite> w = in->parseIntents(result, cells);
            const double ms = t.nsecsElapsed() / 1e6;
            *writes = int(w.size());
            if (best < 0 || ms < best)
                best = ms;
        }
        return best;
    };
    QList<EffectInstance::EffectCell> cells;
    QJSValue dark, burning;
    answersAt(0, cells, dark);
    int darkWrites = 0, burningWrites = 0;
    const double darkMs = readMs(dark, cells, &darkWrites);
    answersAt(12000, cells, burning);
    const double burningMs = readMs(burning, cells, &burningWrites);
    REPORT("reading %d answers: none reached %.2f ms (%d writes), all burning %.2f ms (%d writes)",
           int(cells.size()), darkMs, darkWrites, burningMs, burningWrites);
    QVERIFY(darkWrites < burningWrites / 20);
    QVERIFY2(darkMs < 0.25 * burningMs,
             qPrintable(QString("the unreached heads took %1 ms to read, the burning ones %2 ms")
                            .arg(darkMs).arg(burningMs)));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    FireSpreadPerf_Test test;
    return QTest::qExec(&test, argc, argv);
}

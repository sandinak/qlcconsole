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
#include "doc.h"

#include "firespreadperf_test.h"

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
    QCOMPARE(m_app->loadXML(QStringLiteral(STAGE_SHOW)), QFile::NoError);
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
    for (Fixture *fx : doc->fixtures())
    {
        fx->setOwnChannelValues(QByteArray(int(fx->channels()), 0));
        if (!underTheFire(fx))
            continue;
        look->addFixture(fx->id());
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
    qInfo("Fire Spread over %d fixtures, %d heads", fixtures, heads);
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
    qInfo("effect tick: spreading %.2f ms, burning %.2f ms", spreadTick, burnTick);
    qInfo("values to fixtures (no view): spreading %.2f ms (%d fixtures changed), burning %.2f ms (%d)",
          spreadApply, spreadChanged, burnApply, burnChanged);

    // (c) with the Lighting Studio's 2D plot listening to every fixture.
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        parent.resize(1000, 700);
        parent.show();
        QVERIFY(QTest::qWaitForWindowExposed(&parent));
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
        qInfo("values to fixtures with the 2D plot open: spreading %.2f ms, burning %.2f ms", sp, bu);
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
    for (bool beams : { true, false })
    {
        v.setBeams(beams);
        frameMs(500);                       // warm
        double sc = 0, sw = 0, bc = 0, bw = 0;
        int k = 0;
        for (qint64 ms = 1000; ms <= 7000; ms += 1000, ++k)
        {
            const auto f = frameMs(ms);
            sc += f.first; sw += f.second;
        }
        sc /= k; sw /= k;
        k = 0;
        for (qint64 ms = 12000; ms <= 15000; ms += 1000, ++k)
        {
            const auto f = frameMs(ms);
            bc += f.first; bw += f.second;
        }
        bc /= k; bw /= k;
        qInfo("rig overview frame, beams %s: spreading %.1f ms CPU (%.1f wall), burning %.1f ms CPU (%.1f wall)",
              beams ? "on" : "off", sc, sw, bc, bw);
    }
    v.setLiveValues(false);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    FireSpreadPerf_Test test;
    return QTest::qExec(&test, argc, argv);
}

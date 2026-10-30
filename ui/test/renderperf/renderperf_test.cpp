/*
  Q Light Controller Plus
  renderperf_test.cpp

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
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <functional>
#if defined(Q_OS_UNIX)
#include <time.h>
#endif

#include "renderperf_test.h"

// The live timer and the plot are driven by hand.
#define private public
#define protected public
#include "structurestudioview.h"
#include "monitorgraphicsview.h"
#include "monitor.h"
#undef protected
#undef private

#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"
#include "showbuilder.h"
#include "resource_paths.h"

namespace
{
/** Measuring for tools/perf/guard.py (QLC_PERF_REPORT set); otherwise the
 *  gate, where these only have to run */
bool reporting()
{
    return !qEnvironmentVariableIsEmpty("QLC_PERF_REPORT");
}

const int kUniverses = 8;

/** The process's CPU time in ns: the view renders on a thread pool, and
 *  every thread's work is the frame's cost. Wall clock where there is no
 *  process clock. */
qint64 cpuNs(const QElapsedTimer &wall)
{
#if defined(Q_OS_UNIX) && defined(CLOCK_PROCESS_CPUTIME_ID)
    struct timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0)
        return qint64(ts.tv_sec) * 1000000000LL + qint64(ts.tv_nsec);
#endif
    return wall.nsecsElapsed();
}

/** CPU ns per call of @a op, the best of @a batches batches of @a reps */
double nsPerOp(const std::function<void()> &op, int reps, int batches = 3)
{
    QElapsedTimer wall;
    wall.start();
    if (!reporting())                       // the gate: draw it, once
        reps = batches = 1;
    else
        op();                               // warm the caches
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

/** Counts the paints a widget gets */
class PaintCounter final : public QObject
{
public:
    explicit PaintCounter(QWidget *w) : QObject(w) { w->installEventFilter(this); }
    int count = 0;

protected:
    bool eventFilter(QObject *, QEvent *e) override
    {
        if (e->type() == QEvent::Paint)
            ++count;
        return false;
    }
};
}

void RenderPerf_Test::initTestCase()
{
    m_doc = new Doc(this, kUniverses);
    ShowSpec spec;
    spec.rig = ShowSpec::MixedRig;
    spec.universes = kUniverses;
    spec.scenes = spec.chasers = spec.matrices = spec.efx = spec.collections = spec.cuelists = 0;
    buildShow(m_doc, spec, INTERNAL_FIXTUREDIR, INTERNAL_SCRIPTDIR);
    QCOMPARE(int(m_doc->fixtures().size()), kUniverses * ShowSpec::mixedFixturesPerUni);
    // The plot shows the stage the rig hangs over (metres).
    m_doc->monitorProperties()->setGridSize(QVector3D(14, 8, 14));
    changeValues(0);
}

void RenderPerf_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = nullptr;
}

/* Levels, colours and positions move; everything else stays at rest: the
   shutters open, no strobe, no gobo or prism turning -- those animate a
   view by themselves, and a still look has to be still. */
void RenderPerf_Test::changeValues(int step)
{
    QHash<quint32, QByteArray> universes;
    for (Fixture *fx : m_doc->fixtures())
    {
        QByteArray &u = universes[fx->universe()];
        if (u.isEmpty())
            u = QByteArray(512, char(0));
        const QLCFixtureMode *mode = fx->fixtureMode();
        for (quint32 c = 0; c < fx->channels(); ++c)
        {
            const QLCChannel *ch = mode != nullptr ? mode->channel(c) : nullptr;
            int v = 0;
            if (ch == nullptr || ch->group() == QLCChannel::Intensity || ch->group() == QLCChannel::Pan
                || ch->group() == QLCChannel::Tilt)
                v = int((fx->address() + c) * 37 + quint32(step) * 11) & 0xff;
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
            u[int(fx->address() + c)] = char(v);
        }
    }
    for (Fixture *fx : m_doc->fixtures())
        fx->setChannelValues(universes.value(fx->universe()));
}

/* The 3D/rig view, Live on with beams, its own timer running: an idle show
   (nothing changes) draws no frame at all; a change is drawn. */
void RenderPerf_Test::anIdleRigViewDrawsNothing()
{
    StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
    v.resize(800, 520);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setBeams(true);
    v.show();
    QVERIFY(QTest::qWaitForWindowExposed(&v));
    v.setLiveValues(true);
    QVERIFY(v.m_liveTimer != nullptr && v.m_liveTimer->isActive());
    // Whatever the opening needs (the first frames, the half-second caches)
    QTRY_VERIFY_WITH_TIMEOUT(!v.m_settlePending && v.paintCount() > 0, 5000);
    QTest::qWait(700);
    const int painted = v.paintCount();
    QTest::qWait(1000);                     // 25 live ticks
    QVERIFY2(v.paintCount() == painted,
             qPrintable(QString("an idle rig drew %1 frames in a second").arg(v.paintCount() - painted)));

    changeValues(1);
    QTRY_VERIFY(v.paintCount() > painted);
    v.setLiveValues(false);
}

/* The Lighting Studio's 2D plot of the same rig: idle, its viewport gets no
   paint; a value change repaints it. */
void RenderPerf_Test::anIdlePlotDrawsNothing()
{
    QWidget parent;
    Monitor mon(&parent, m_doc);
    parent.resize(1000, 700);
    parent.show();
    QVERIFY(QTest::qWaitForWindowExposed(&parent));
    MonitorGraphicsView *gv = mon.m_graphicsView;
    QVERIFY(gv != nullptr);
    QCOMPARE(int(gv->m_fixtures.size()), int(m_doc->fixtures().size()));
    PaintCounter paints(gv->viewport());
    QTest::qWait(800);                      // the opening frames
    const int painted = paints.count;
    QTest::qWait(1000);
    QVERIFY2(paints.count == painted,
             qPrintable(QString("an idle plot drew %1 frames in a second").arg(paints.count - painted)));

    changeValues(2);
    QTRY_VERIFY(paints.count > painted);
}

/* A running show (values changing the whole time) with Live on: the frame
   budget holds the view well under one core, and when the values stop, a
   full-quality frame follows and then nothing is drawn. */
void RenderPerf_Test::aRunningShowStaysUnderOneCore()
{
    StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
    v.resize(960, 600);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setBeams(true);
    v.show();
    QVERIFY(QTest::qWaitForWindowExposed(&v));
    v.setLiveValues(true);
    QTRY_VERIFY_WITH_TIMEOUT(!v.m_settlePending && v.paintCount() > 0, 5000);
    QTest::qWait(700);

    QElapsedTimer wall;
    wall.start();
    const qint64 cpu0 = cpuNs(wall);
    const int painted0 = v.paintCount();
    int step = 100;
    QTimer show;                            // the engine: new values every 40 ms
    connect(&show, &QTimer::timeout, this, [this, &step]() { changeValues(++step); });
    show.start(40);
    while (wall.elapsed() < 3000)
        QTest::qWait(20);
    show.stop();
    const double secs = wall.elapsed() / 1000.0;
    const double cores = double(cpuNs(wall) - cpu0) / 1e9 / secs;
    m_runningNsPerSec = double(cpuNs(wall) - cpu0) / secs;
    const int frames = v.paintCount() - painted0;
    qInfo("a running show: %d frames in %.1f s, %.2f cores", frames, secs, cores);
    QVERIFY(frames > 0);
    QVERIFY2(cores < 0.75, qPrintable(QString("a running show took %1 cores").arg(cores)));

    // A value-driven frame close on the last is drawn light, then a full one settles.
    v.m_lastFrameAt.start();
    v.m_valueFrame = true;
    v.repaint();
    QVERIFY(v.m_degraded);
    QVERIFY(v.m_degradeSettle != nullptr && v.m_degradeSettle->isActive());
    QTRY_VERIFY_WITH_TIMEOUT(v.m_qualityFrame || !v.m_degraded, 3000);

    // The values stop: one full-quality frame, then still.
    QTRY_VERIFY_WITH_TIMEOUT(!v.m_degraded && (v.m_degradeSettle == nullptr || !v.m_degradeSettle->isActive()), 5000);
    QTest::qWait(1500);
    const int settled = v.paintCount();
    QVERIFY(!v.m_degraded);
    QTest::qWait(500);
    QCOMPARE(v.paintCount(), settled);
    v.setLiveValues(false);
}

/* A gel filters the lamp: white through blue is blue, red through blue is black. */
void RenderPerf_Test::aGelFiltersTheLight()
{
    const QColor blue(0, 0, 255);
    QCOMPARE(StructureStudioView::gelled(QColor(255, 255, 255), blue), blue);
    QCOMPARE(StructureStudioView::gelled(QColor(255, 0, 0), blue), QColor(0, 0, 0));
    QCOMPARE(StructureStudioView::gelled(QColor(200, 100, 50), QColor()), QColor(200, 100, 50));
    QCOMPARE(StructureStudioView::gelled(QColor(255, 255, 255), QColor(255, 128, 0)), QColor(255, 128, 0));
}

/* Nothing asserted (see the class comment). QLC_PERF_REPS scales the
   frames (default 1: a quick pass for the gate). */
void RenderPerf_Test::cpuPerFrame()
{
    const int scale = qMax(1, qEnvironmentVariableIntValue("QLC_PERF_REPS"));
    QList<QPair<QString, double>> results;
    int step = 10;

    {
        StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
        v.resize(960, 600);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setBeams(true);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        v.setLiveValues(true);
        v.m_liveTimer->stop();              // frames by hand below
        QCoreApplication::processEvents();

        // 1. An idle live tick: nothing changed, nothing drawn -- what an
        //    open rig view costs a show that stands still, 25 times a second.
        v.repaint();
        QVERIFY(!v.liveTick());
        results << qMakePair(QStringLiteral("gui_studio_idle_tick_240fx"),
                             nsPerOp([&v]() { v.liveTick(); }, 200 * scale));

        // 2. The angled view with beams, every fixture changed: a frame of
        //    a busy show.
        const int painted = v.paintCount();
        results << qMakePair(QStringLiteral("gui_studio_angled_frame_240fx"), nsPerOp([this, &v, &step]() {
            changeValues(++step);
            v.repaint();
        }, 3 * scale));
        QVERIFY(v.paintCount() > painted);

        // 3. The same rig in plan.
        v.setPlane(StructureStudioView::Top);
        v.repaint();
        results << qMakePair(QStringLiteral("gui_studio_plan_frame_240fx"), nsPerOp([this, &v, &step]() {
            changeValues(++step);
            v.repaint();
        }, 3 * scale));
        v.setLiveValues(false);
    }

    // 4. The Lighting Studio's 2D plot: every fixture changed, a frame.
    {
        QWidget parent;
        Monitor mon(&parent, m_doc);
        parent.resize(1000, 700);
        parent.show();
        QVERIFY(QTest::qWaitForWindowExposed(&parent));
        MonitorGraphicsView *gv = mon.m_graphicsView;
        QCoreApplication::processEvents();
        results << qMakePair(QStringLiteral("gui_plot2d_frame_240fx"), nsPerOp([this, gv, &step]() {
            changeValues(++step);
            QCoreApplication::processEvents();
            gv->viewport()->repaint();
        }, 5 * scale));
    }

    if (m_runningNsPerSec > 0)
        results << qMakePair(QStringLiteral("gui_studio_running_show_cpu_per_s_240fx"), m_runningNsPerSec);

    QStringList table;
    for (const auto &r : results)
        table << QString("%1 %2 ns").arg(r.first, -30).arg(r.second, 12, 'f', 0);
    qInfo().noquote() << "Drawing the rig, process CPU per frame (best batch):\n  " + table.join("\n  ");

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

QTEST_MAIN(RenderPerf_Test)

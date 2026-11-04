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
#include <QApplication>
#include <QTimer>
#include <QScopeGuard>
#include <QMouseEvent>
#include <QFile>
#include <QTextStream>
#include <functional>
#if defined(Q_OS_UNIX)
#include <time.h>
#endif

#include "renderperf_test.h"

// The live timer and the plot are driven by hand.
#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "structurestudioview.h"
#include "monitorgraphicsview.h"
#include "monitor.h"
#undef protected
#undef private

#include "monitorproperties.h"
#include "systemprobe.h"
#include "rhiraster.h"
#include "zraster.h"
#include "qlcfixturedef.h"
#include "scenery.h"
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
    // The cores the view spreads over: some, never more than the machine has.
    QVERIFY(SystemProbe::performanceCores() >= 1);
    QVERIFY(SystemProbe::performanceCores() <= qMax(1, QThread::idealThreadCount()));
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
    // Offscreen there is no graphics card to draw on: the CPU, and it says so.
    QCOMPARE(v.renderBackendLabel(), QStringLiteral("CPU"));
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

/* A running show (values changing the whole time) with Live on. The frame
   budget gives the view a share of the performance cores -- all but two of
   them, about 40% of each (SystemProbe::performanceCores()) -- where it used
   to hold it to 40% of ONE core whatever the machine: a 240-fixture rig drew
   4 frames a second with most cores idle ("we have lots of CPUs .. but only
   using 1"). In Operate, where the DMX comes first, it is held to the old
   share and to 15 frames a second. When the values stop, a full-quality
   frame follows and then nothing is drawn. */
void RenderPerf_Test::aRunningShowStaysWithinItsShare()
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

    int step = 100;
    // Three seconds of a show in @a mode: frames drawn, cores used.
    auto run = [&](Doc::Mode mode, int &frames, double &cores) {
        m_doc->setMode(mode);
        QTest::qWait(300);                  // the rate settles to the mode
        QElapsedTimer wall;
        wall.start();
        const qint64 cpu0 = cpuNs(wall);
        const int painted0 = v.paintCount();
        QTimer show;                        // the engine: new values every 40 ms
        connect(&show, &QTimer::timeout, this, [this, &step]() { changeValues(++step); });
        show.start(40);
        while (wall.elapsed() < 3000)
            QTest::qWait(20);
        show.stop();
        const double secs = wall.elapsed() / 1000.0;
        cores = double(cpuNs(wall) - cpu0) / 1e9 / secs;
        frames = v.paintCount() - painted0;
        if (mode == Doc::Design)
            m_runningNsPerSec = double(cpuNs(wall) - cpu0) / secs;
    };
    int designFrames = 0, operateFrames = 0;
    double designCores = 0, operateCores = 0;
    run(Doc::Operate, operateFrames, operateCores);
    const int operateInterval = v.m_liveTimer->interval();
    run(Doc::Design, designFrames, designCores);
    m_doc->setMode(Doc::Design);
    const int share = qMax(1, SystemProbe::performanceCores() - 2);
    qInfo("a running show: Design %d frames, %.2f cores (share %d); Operate %d frames, %.2f cores",
          designFrames, designCores, share, operateFrames, operateCores);
    QVERIFY(designFrames > 0 && operateFrames > 0);
    // Operate: never faster than its ceiling.
    QVERIFY2(operateInterval >= 1000 / StructureStudioView::operateFrameRate(),
             qPrintable(QString("Operate's live interval %1 ms").arg(operateInterval)));
#if defined(QT_NO_DEBUG) && defined(Q_OS_UNIX) // no process CPU clock elsewhere: cpuNs() is wall time there, always ~1 core. A debug build's frames and the test's own value writes swamp the budget
    QVERIFY2(operateCores < 0.75, qPrintable(QString("in Operate a running show took %1 cores").arg(operateCores)));
    QVERIFY2(designCores < 0.4 * share + 0.35,
             qPrintable(QString("a running show took %1 cores, its share is %2").arg(designCores).arg(share)));
#endif

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

/* In Operate the pointer gets no more frames than the show: twenty moves
   in a few milliseconds over the rig paint once now and once more when the
   show's next frame is due, not twenty times. In Design every move is
   drawn at once. */
void RenderPerf_Test::operatePointerFramesWait()
{
    StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
    v.resize(640, 400);
    v.reload();
    v.setPlane(StructureStudioView::Top);
    v.show();
    QVERIFY(QTest::qWaitForWindowExposed(&v));
    auto moves = [&v]() {
        for (int i = 0; i < 20; ++i)
        {
            const QPointF at(100 + i * 5, 150);
            QMouseEvent mv(QEvent::MouseMove, at, v.mapToGlobal(at), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&v, &mv);
            QCoreApplication::processEvents();
        }
    };
    m_doc->setMode(Doc::Operate);
    auto design = qScopeGuard([this]() { m_doc->setMode(Doc::Design); });
    v.repaint();                            // a frame just drawn
    const int before = v.paintCount();
    moves();
    QVERIFY2(v.paintCount() - before <= 1,
             qPrintable(QString("%1 pointer frames at once in Operate").arg(v.paintCount() - before)));
    QTRY_VERIFY_WITH_TIMEOUT(v.paintCount() > before, 1000);   // the crosshair still follows
    QTRY_VERIFY(!v.m_hoverTickPending);

    m_doc->setMode(Doc::Design);
    QTest::qWait(100);
    const int designBefore = v.paintCount();
    moves();
    QVERIFY(v.paintCount() - designBefore >= 5);
}

/* One heavy frame -- every beam landing again after a GO -- must not hold
   the rate down for long. It went from 40 ms to 450-780 ms and came back by
   a fifth a frame, so a show sat at 1-2 fps for seconds after each GO. */
void RenderPerf_Test::theRateRecoversAfterABurst()
{
    StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
    v.resize(640, 400);
    v.reload();
    v.setPlane(StructureStudioView::Top);   // cheap frames
    v.show();
    QVERIFY(QTest::qWaitForWindowExposed(&v));
    v.setLiveValues(true);
    v.m_liveTimer->stop();
    v.m_frameMs = 600.0;                    // the burst's estimate
    v.m_liveTimer->setInterval(750);
    for (int i = 0; i < 6; ++i)
    {
        changeValues(900 + i);
        v.repaint();
    }
    QVERIFY2(v.m_liveTimer->interval() < 120,
             qPrintable(QString("still %1 ms between frames six frames after a burst")
                        .arg(v.m_liveTimer->interval())));
    // One slow frame among quick ones moves the estimate by three times at most.
    QVERIFY(v.m_frameMs < 100.0);
    QCOMPARE(StructureStudioView::nextFrameEstimate(40.0, 900.0), 40.0 * 0.8 + 120.0 * 0.2);
    QCOMPARE(StructureStudioView::nextFrameEstimate(600.0, 40.0), 320.0);
    QCOMPARE(StructureStudioView::nextFrameEstimate(0.0, 25.0), 25.0);
    v.setLiveValues(false);
}

namespace
{
/** A 6-channel moving head (pan, tilt, dimmer, RGB) with a @a beamDeg lens */
QLCFixtureDef *moverDefinition(double beamDeg)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Head");
    def->setType(QLCFixtureDef::MovingHead);
    const char *nm[] = { "Pan", "Tilt", "Dimmer", "Red", "Green", "Blue" };
    const QLCChannel::Group gp[] = { QLCChannel::Pan, QLCChannel::Tilt,
        QLCChannel::Intensity, QLCChannel::Intensity, QLCChannel::Intensity,
        QLCChannel::Intensity };
    const QLCChannel::PrimaryColour cl[] = { QLCChannel::NoColour, QLCChannel::NoColour,
        QLCChannel::NoColour, QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue };
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("6ch");
    for (int k = 0; k < 6; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(nm[k]); ch->setGroup(gp[k]);
        if (cl[k] != QLCChannel::NoColour) ch->setColour(cl[k]);
        if (k < 2) ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch); mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setWidth(300); ph.setHeight(400); ph.setDepth(300);
    ph.setFocusPanMax(540); ph.setFocusTiltMax(270);
    ph.setLensDegreesMin(beamDeg); ph.setLensDegreesMax(beamDeg);
    mode->setPhysical(ph);
    QLCFixtureHead hd;
    for (int k = 0; k < 6; ++k) hd.addChannel(quint32(k));
    mode->insertHead(-1, hd);
    def->addMode(mode);
    return def;
}
}

/* The shortcuts beams take -- landings kept from frame to frame, a moving
   curtain asked of again by itself, scenery a ray cannot reach not asked at
   all -- draw exactly the picture of every ray thrown afresh against
   everything. Twenty-four heads over a stage with a cyc, a proscenium, a
   wall and a drape its motor opens: the same pixels, the curtain still and
   moving, the heads still and moving. (The mixed rig hangs its fixtures at
   6000 m -- showbuilder.cpp gives Z in mm where it is metres -- so its
   beams land on nothing; this stage is built to be landed on.) */
void RenderPerf_Test::beamShortcutsDrawTheSamePicture()
{
    const bool moveSpeedWas = StructureStudioView::s_moveSpeed;
    StructureStudioView::setMoveSpeed(false);
    auto restore = qScopeGuard([moveSpeedWas]() {
        StructureStudioView::setMoveSpeed(moveSpeedWas);
        StructureStudioView::s_referenceThrows = false;
    });

    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    props->setGridSize(QVector3D(14, 10, 8));
    Scenery *cyc = props->addScenery();
    cyc->setKind(Scenery::Cyclorama);
    cyc->setOriginX(7.0f); cyc->setOriginY(1.0f); cyc->setWidth(12.0f); cyc->setHeight(6.0f);
    Scenery *pros = props->addScenery();
    pros->setKind(Scenery::Proscenium);
    pros->setOriginX(7.0f); pros->setOriginY(8.5f); pros->setWidth(14.0f); pros->setHeight(7.0f);
    pros->setThickness(0.4f); pros->setOpeningWidth(11.0f); pros->setOpeningHeight(5.5f);
    Scenery *wall = props->addScenery();
    wall->setKind(Scenery::Wall);
    wall->setOriginX(4.0f); wall->setOriginY(5.0f); wall->setWidth(3.0f); wall->setHeight(1.0f);
    wall->setThickness(0.3f);
    Scenery *drape = props->addScenery();
    drape->setKind(Scenery::MainDrape);
    drape->setOriginX(7.0f); drape->setOriginY(7.0f); drape->setWidth(14.0f); drape->setHeight(6.0f);
    drape->setOpeningWidth(10.0f);
    drape->setState(Scenery::Open);

    QLCFixtureDef *curtainDef = new QLCFixtureDef();
    curtainDef->setManufacturer("Test"); curtainDef->setModel("Curtain");
    curtainDef->setType(QLCFixtureDef::Other);
    QLCFixtureMode *curtainMode = new QLCFixtureMode(curtainDef);
    curtainMode->setName("Open");
    QLCChannel *openCh = new QLCChannel();
    openCh->setName("Open");
    openCh->setGroup(QLCChannel::Maintenance);
    curtainDef->addChannel(openCh);
    curtainMode->insertChannel(openCh, 0);
    curtainDef->addMode(curtainMode);
    Fixture *motor = new Fixture(&doc);
    motor->setName("Drape motor");
    motor->setFixtureDefinition(curtainDef, curtainMode);
    motor->setAddress(500);
    QVERIFY(doc.addFixture(motor));
    drape->setDriverFixture(motor->id());
    QVERIFY(drape->isDriven());

    QLCFixtureDef *headDef = moverDefinition(18.0);
    QList<Fixture *> heads;
    for (int i = 0; i < 24; ++i)
    {
        Fixture *f = new Fixture(&doc);
        f->setName(QString("Head %1").arg(i));
        f->setFixtureDefinition(headDef, headDef->modes().first());
        f->setAddress(quint32(i * 6));
        QVERIFY(doc.addFixture(f));
        // Two rows over the stage, metres up; X/Y in mm (setFixturePosition()).
        props->setFixturePosition(f->id(), 0, 0,
                                  QVector3D(1000.0f + (i % 12) * 1100.0f, i < 12 ? 3000.0f : 7500.0f, 6.0f));
        heads << f;
    }
    auto look = [&](int step, uchar open) {
        QByteArray u(512, char(0));
        for (int i = 0; i < heads.size(); ++i)
        {
            const int a = i * 6;
            u[a] = char((i * 37 + step * 13) & 0xff);                // pan
            u[a + 1] = char(40 + ((i * 53 + step * 7) % 160));       // tilt
            u[a + 2] = char(255);
            u[a + 3] = char((i * 70) & 0xff);
            u[a + 4] = char(200);
            u[a + 5] = char((255 - i * 40) & 0xff);
        }
        u[500] = char(open);
        for (Fixture *f : heads)
            f->setChannelValues(u);
        motor->setChannelValues(u);
    };

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(900, 560);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setBeams(true);
    v.m_gpuWanted = false;                  // the CPU's picture: exact everywhere
    v.m_supersample = 1;
    v.show();
    QVERIFY(QTest::qWaitForWindowExposed(&v));
    v.setLiveValues(true);
    v.m_liveTimer->stop();
    QTRY_VERIFY_WITH_TIMEOUT(!v.m_settlePending, 5000);

    auto frame = [&](bool reference) {
        StructureStudioView::s_referenceThrows = reference;
        v.m_liveDirty = true;
        v.repaint();                        // twice: kept landings in play
        v.m_liveDirty = true;
        v.repaint();
        return v.grab().toImage();
    };
    int onScenery = 0, onDrape = 0;
    int step = 0;
    for (const uchar open : { uchar(0), uchar(90), uchar(91), uchar(200), uchar(255), uchar(255) })
    {
        look(++step, open);
        const QImage exact = frame(true);
        const QImage fast = frame(false);
        QCOMPARE(fast.size(), exact.size());
        int differ = 0;
        for (int y = 0; y < fast.height(); ++y)
            for (int x = 0; x < fast.width(); ++x)
                if (fast.pixel(x, y) != exact.pixel(x, y))
                    ++differ;
        QVERIFY2(differ == 0, qPrintable(QString("curtain at %1: %2 pixels differ").arg(open).arg(differ)));
        for (auto it = v.m_throws.constBegin(); it != v.m_throws.constEnd(); ++it)
            if (it->fullHit && it->fullSurface < 0 && it->fullSurface > -1000000)
            {
                ++onScenery;
                if (it->fullSurface == -1 - int(drape->id()))
                    ++onDrape;
            }
    }
    // The test is only worth something if beams met the scenery and the drape.
    QVERIFY2(onScenery > 100 && onDrape > 10,
             qPrintable(QString("%1 rays on scenery, %2 on the drape").arg(onScenery).arg(onDrape)));

    /* What a frame costs, the curtain and the heads moving: the display's
       budget (docs/performance.md, "The display"). Release only: a debug
       build is several times slower. */
    StructureStudioView::s_referenceThrows = false;
    QElapsedTimer clock;
    clock.start();
    const qint64 cpu0 = cpuNs(clock);
    for (int i = 0; i < 6; ++i)
    {
        look(100 + i, uchar(40 * i));
        v.m_liveDirty = true;
        v.repaint();
    }
    const double msPerFrame = double(cpuNs(clock) - cpu0) / 6 / 1e6;
    qInfo("a frame with the curtain and the heads moving: %.1f ms of CPU", msPerFrame);
#if defined(QT_NO_DEBUG) && defined(Q_OS_UNIX)
    /* 20 ms on an M4, so a slower gate machine passes and a return of every
       ray asking every piece of scenery does not. */
    const double kMovingFrameBudgetMs = 120.0;
    QVERIFY2(msPerFrame < kMovingFrameBudgetMs,
             qPrintable(QString("a moving frame cost %1 ms of CPU, the budget is %2")
                        .arg(msPerFrame).arg(kMovingFrameBudgetMs)));
#endif
    v.setLiveValues(false);
}

/* Over a GPU frame the cursor readout asks the depth under the pointer:
   one row drawn on the CPU, not the whole frame (10-13 ms on the GUI thread,
   every frame the pointer was over the rig) -- and the same depth. Needs a
   graphics card: skipped where there is none to draw on. */
void RenderPerf_Test::pickingDrawsOneRow()
{
    qputenv("QLC_RIG_GPU", "1");
    auto unset = qScopeGuard([]() { qunsetenv("QLC_RIG_GPU"); });
    if (!RhiRaster::available())
        QSKIP("no graphics card to draw on here");

    StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
    v.resize(800, 500);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setBeams(true);
    v.m_gpuWanted = true;
    v.show();
    QVERIFY(QTest::qWaitForWindowExposed(&v));
    QCOMPARE(v.renderBackendLabel(), QStringLiteral("GPU · %1").arg(RhiRaster::availableBackend()));
    changeValues(1500);
    v.repaint();
    QVERIFY(v.m_drewOnGpu);
    QVERIFY(v.m_pickDirty);

    // Every depth from the frame's ops, as the whole-frame pick drew it.
    ZRaster whole;
    {
        ZRaster *mine = v.m_z;
        v.m_z = &whole;
        whole.begin(v.size(), 1, Qt::transparent);
        for (const StructureStudioView::DrawOp *o : std::as_const(v.m_pickOpaque))
            v.rasterOp(*o, true, 0, INT_MAX);
        for (const StructureStudioView::DrawOp *o : std::as_const(v.m_pickSeeThrough))
            v.rasterOp(*o, false, 0, INT_MAX);
        v.m_z = mine;
    }
    int probed = 0, onSomething = 0;
    for (int y = 20; y < v.height(); y += 37)
        for (int x = 15; x < v.width(); x += 53)
        {
            const QPoint at(x, y);
            const double expected = whole.depthAt(at);
            QCOMPARE(v.pickDepth(at), expected);
            QCOMPARE(v.m_pickRow, y);
            ++probed;
            if (expected > -1e29)
                ++onSomething;
        }
    QVERIFY(probed > 50 && onSomething > 0);
}

/* "Visual artifacts in the rig view ... moving a very small distance back and
   forth when the mouse moves over it." With a show running, the values'
   frames are drawn light (no supersampling) and the crosshair's were drawn
   full, so hovering interleaved the two -- every edge rasterised a little
   differently, frame to frame. Pointer moves now keep the frame's quality and
   change nothing about the camera: the rig under the crosshair is the same
   picture, pixel for pixel. */
void RenderPerf_Test::hoveringARunningShowKeepsTheRigStill()
{
    const bool moveSpeedWas = StructureStudioView::s_moveSpeed;
    StructureStudioView::setMoveSpeed(false);   // heads snap: one value, one picture
    auto restore = qScopeGuard([moveSpeedWas]() { StructureStudioView::setMoveSpeed(moveSpeedWas); });

    StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
    v.resize(800, 520);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setBeams(true);
    v.m_supersample = 2;                    // "Smooth" (not saved: no QSettings write)
    v.show();
    QVERIFY(QTest::qWaitForWindowExposed(&v));
    v.setLiveValues(true);
    v.m_liveTimer->stop();                  // frames by hand below
    QTRY_VERIFY_WITH_TIMEOUT(!v.m_settlePending, 5000);
    changeValues(700);
    v.repaint();

    const double scale = v.m_scale;
    const QPointF origin = v.m_originPx;
    const double az = v.m_azimuthDeg, el = v.m_elevationDeg;
    const QRect geometry = v.geometry();

    auto hover = [&v](const QPoint &at) {
        QMouseEvent move(QEvent::MouseMove, QPointF(at), v.mapToGlobal(QPointF(at)),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&v, &move);
    };
    auto cameraHeld = [&](const QString &where) {
        QVERIFY2(v.m_scale == scale && v.m_originPx == origin && v.m_azimuthDeg == az
                 && v.m_elevationDeg == el && v.geometry() == geometry,
                 qPrintable(QString("%1: the view moved (scale %2 -> %3, origin %4,%5 -> %6,%7)")
                            .arg(where).arg(scale).arg(v.m_scale).arg(origin.x()).arg(origin.y())
                            .arg(v.m_originPx.x()).arg(v.m_originPx.y())));
    };

    // The show is running: a value frame, drawn light.
    v.m_lastFrameAt.start();
    v.m_valueFrame = true;
    const QImage light = v.grab().toImage();
    QVERIFY(v.m_degraded);

    // The pointer sweeps across it; a frame for each move.
    QString firstReading;
    const QPoint last(560, 140);
    for (int i = 0; i <= 8; ++i)
    {
        const QPoint at = (i < 8) ? QPoint(120 + i * 70, 400 - i * 30) : last;
        v.m_lastFrameAt.start();            // frames keep coming, as in a running show
        hover(at);
        QCOMPARE(v.m_cursorPx, QPointF(at));
        if (i < 8)
            v.repaint();
        const QString where = QString("move %1").arg(i);
        cameraHeld(where);
        if (i == 0)
            firstReading = v.cursorReadoutText(v.m_cursorPx);
        if (i < 8)
            QVERIFY2(v.m_degraded, qPrintable(where + ": a pointer frame changed the quality"));
    }
    // The readout follows the pointer all the same.
    QVERIFY(v.m_hasCursor);
    QVERIFY(v.cursorReadoutText(v.m_cursorPx) != firstReading);

    // The rig under the crosshair is the value frame's, pixel for pixel.
    const QImage hovered = v.grab().toImage();
    QVERIFY(v.m_degraded);
    QCOMPARE(hovered.size(), light.size());
    const qreal dpr = hovered.devicePixelRatio();
    int differ = 0;
    for (int y = 0; y < hovered.height(); ++y)
    {
        const double ly = y / dpr;
        if (qAbs(ly - last.y()) <= 3 || (ly >= last.y() + 6 && ly <= last.y() + 32))
            continue;                       // the crosshair's line and its readout
        for (int x = 0; x < hovered.width(); ++x)
        {
            if (qAbs(x / dpr - last.x()) <= 3)
                continue;
            if (hovered.pixel(x, y) != light.pixel(x, y))
                ++differ;
        }
    }
    QVERIFY2(differ == 0, qPrintable(QString("%1 pixels of the rig changed under the pointer").arg(differ)));

    // The values stop: the settle frame is still full quality, pointer or not.
    v.m_qualityFrame = true;
    v.m_lastFrameAt.start();
    hover(QPoint(300, 300));
    v.repaint();
    QVERIFY(!v.m_degraded);
    // ...and hovering a still show draws full quality, as before.
    for (int i = 0; i < 3; ++i)
    {
        v.m_lastFrameAt.start();
        hover(QPoint(200 + i * 40, 260));
        v.repaint();
        QVERIFY(!v.m_degraded);
        cameraHeld(QString("still %1").arg(i));
    }
    // The pointer leaves: the crosshair goes, the view stays.
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&v, &leave);
    v.repaint();
    QVERIFY(!v.m_hasCursor);
    cameraHeld(QStringLiteral("leave"));
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

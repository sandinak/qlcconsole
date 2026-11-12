/*
  Q Light Controller Plus
  followspotfeel_test.cpp

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
#include <QElapsedTimer>
#include <QBuffer>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QMutex>
#include <QThread>
#include <cmath>

#include "followspotfeel_test.h"
#include "../common/stdprelude.h"       // every std header, before the define
#define private public                  // MasterTimer::timerTick(), the controller's state
#define protected public                // Universe::processFaders()
#include "mastertimer.h"
#include "universe.h"
#include "programmercontroller.h"
#include "effectscriptrunner.h"
#undef protected
#undef private
#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcphysical.h"
#include "stagetarget.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "dmxsource.h"
#include "followspotmover.h"
#include "followspotdrive.h"
#include "followspotoperator.h"
#include "venuecorrections.h"
#include "effectinstance.h"
#include "effectscript.h"
#include "aimsolver.h"
#include "fixture.h"
#include "truss.h"
#include "scene.h"
#include "doc.h"

/* How the follow spot feels, measured on the real path: the stick's value
   as the controller's input slot gets it, ProgrammerController's tick, the
   MasterTimer writing the scene, DMX out -- and from that DMX, where each
   beam lands on the stage. Two heads on a pipe 6 m up, 4 m apart: one with
   16-bit pan and tilt, one with 8-bit only. */

namespace
{

const quint32 kStickUniverse = 9;

struct Rig
{
    Fixture *fine = nullptr;     ///< 16-bit pan/tilt
    Fixture *coarse = nullptr;   ///< 8-bit pan/tilt
    StageTarget *subject = nullptr;
    QLCPalette *aim = nullptr;
    QLCPalette *follow = nullptr;
    Scene *targetScene = nullptr;   ///< Aim + Followspot: the stick moves the subject
    Scene *freeScene = nullptr;     ///< Followspot alone: the stick flies the heads
    QVector3D home;
};

QLCFixtureDef *moverDef(bool sixteenBit)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel(sixteenBit ? "Spot 16bit" : "Spot 8bit");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("std");
    struct Ch { const char *name; QLCChannel::Group group; QLCChannel::ControlByte byte; };
    QList<Ch> chans;
    if (sixteenBit)
        chans = { { "Pan", QLCChannel::Pan, QLCChannel::MSB }, { "Pan fine", QLCChannel::Pan, QLCChannel::LSB },
                  { "Tilt", QLCChannel::Tilt, QLCChannel::MSB }, { "Tilt fine", QLCChannel::Tilt, QLCChannel::LSB },
                  { "Dimmer", QLCChannel::Intensity, QLCChannel::MSB } };
    else
        chans = { { "Pan", QLCChannel::Pan, QLCChannel::MSB }, { "Tilt", QLCChannel::Tilt, QLCChannel::MSB },
                  { "Dimmer", QLCChannel::Intensity, QLCChannel::MSB } };
    QLCFixtureHead head;
    for (int k = 0; k < chans.size(); ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(chans[k].name);
        ch->setGroup(chans[k].group);
        ch->setControlByte(chans[k].byte);
        def->addChannel(ch);
        mode->insertChannel(ch, quint32(k));
        if (chans[k].group != QLCChannel::Intensity)
            head.addChannel(quint32(k));
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);
    return def;
}

Rig makeRig(Doc *doc)
{
    Rig rig;
    MonitorProperties *props = doc->monitorProperties();
    Truss *pipe = props->addTruss();
    pipe->setType(Truss::Horizontal);
    pipe->setOrigin(QVector3D(-3.0f, 0.0f, 6.0f));
    pipe->setDirection(QPointF(1.0, 0.0));
    pipe->setLength(6.0f);
    pipe->setWidth(0.29f);

    for (int i = 0; i < 2; ++i)
    {
        QLCFixtureDef *def = moverDef(i == 0);
        Fixture *f = new Fixture(doc);
        f->setName(i == 0 ? "Spot 16" : "Spot 8");
        f->setFixtureDefinition(def, def->modes().first());
        f->setUniverse(0);
        f->setAddress(100 + 10 * quint32(i));
        doc->addFixture(f);
        FixtureRigProps rp;
        rp.trussId = pipe->id();
        rp.trussOffset = i == 0 ? 1.0f : 5.0f;      // x = -2 m and +2 m
        rp.mountingType = Truss::TopHung;
        props->setFixtureRigProps(f->id(), rp);
        (i == 0 ? rig.fine : rig.coarse) = f;
    }

    rig.home = QVector3D(0.0f, 3.0f, 0.0f);
    rig.subject = props->addStageTarget();
    rig.subject->setName("Soloist");
    rig.subject->setPosition(rig.home);

    rig.aim = new QLCPalette(QLCPalette::Aim, doc);
    rig.aim->setName("On Soloist");
    rig.aim->setStageTargetId(rig.subject->id());
    doc->addPalette(rig.aim);
    rig.follow = new QLCPalette(QLCPalette::Effect, doc);
    rig.follow->setName("Followspot");
    rig.follow->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/followspot.js");
    doc->addPalette(rig.follow);

    rig.targetScene = new Scene(doc);
    rig.targetScene->setName("Follow Soloist");
    doc->addFunction(rig.targetScene);
    rig.freeScene = new Scene(doc);
    rig.freeScene->setName("Fly");
    doc->addFunction(rig.freeScene);
    for (Scene *s : { rig.targetScene, rig.freeScene })
    {
        s->addFixture(rig.fine->id());
        s->addFixture(rig.coarse->id());
    }
    rig.targetScene->addPalette(rig.aim->id());
    rig.targetScene->addPalette(rig.follow->id());
    rig.freeScene->addPalette(rig.follow->id());
    return rig;
}

QByteArray dmx(Doc *doc)
{
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    const QByteArray out = ua.first()->preGMValues();
    doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

/* Where a head's beam lands on the plane z = @p planeZ, from its DMX. */
bool landing(Doc *doc, Fixture *f, const QByteArray &d, float planeZ, QVector3D &at)
{
    const QByteArray values = d.mid(int(f->address()), int(f->channels()));
    AimSolver::Frame frame;
    if (!AimSolver::frameFor(doc, f->id(), frame))
        return false;
    QVector3D dir;
    if (!AimSolver::headDirection(f, values, 0, doc->monitorProperties()->fixtureRigProps(f->id()), dir))
        return false;
    if (std::abs(dir.z()) < 1e-6f)
        return false;
    const float t = (planeZ - frame.position.z()) / dir.z();
    if (t <= 0)
        return false;
    at = frame.position + dir * t;
    return true;
}

double mean(const QVector<double> &v)
{
    double s = 0;
    for (double x : v)
        s += x;
    return v.isEmpty() ? 0 : s / v.size();
}

double stdev(const QVector<double> &v)
{
    const double m = mean(v);
    double s = 0;
    for (double x : v)
        s += (x - m) * (x - m);
    return v.size() < 2 ? 0 : std::sqrt(s / (v.size() - 1));
}

/* One run: a stick value held for @p ticks, each tick the controller's
   joystick tick then a MasterTimer frame. */
struct Run
{
    double speed16 = 0;     ///< m/s on the floor, the 16-bit head, steady part
    double speed8 = 0;      ///< the 8-bit head
    double targetSpeed = 0; ///< the subject (target mode)
    double cov16 = 0;       ///< per-frame floor step: stdev / mean (smoothness; 0 = even)
    double cov8 = 0;
    double zeros8 = 0;      ///< share of frames the 8-bit head did not move at all
    double maxStep8 = 0;    ///< its largest single-frame jump, metres
    int dmxSteps16 = 0;     ///< distinct 16-bit pan values seen
    int dmxSteps8 = 0;      ///< distinct 8-bit pan values seen
    double panRate16 = 0;   ///< degrees a second, from DMX
    double panRate8 = 0;
    QVector<double> steps16;
};

struct Harness
{
    Doc *doc;
    Rig rig;
    MasterTimer *timer;
    ProgrammerController *pc;

    void frame()
    {
        timer->timerTick();
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        doc->inputOutputMap()->releaseUniverses(false);
    }

    /* One tick of each clock, in turn: the controller's joystick tick,
       the effect runner's (both main-thread timers in the app) and a
       MasterTimer frame. */
    /* Free-fly: the effect integrates over its own clock (real time), so
       its ticks are spaced as the app spaces them. */
    int spacingMs = 0;

    void tick()
    {
        if (spacingMs > 0)
            QThread::msleep(ulong(spacingMs));
        pc->applyDesignJoystick();
        doc->effectScriptRunner()->slotTick();
        frame();
    }

    void stick(uchar x, uchar y = 127)
    {
        pc->slotControllerInputChanged(kStickUniverse, 0, x, QString());
        pc->slotControllerInputChanged(kStickUniverse, 1, y, QString());
    }

    void bind()
    {
        // A DMX frame is 20 ms here (the real clocks: realTimers()).
        if (FollowSpotMover *m = pc->followSpotMover())
            m->setFixedDt(MasterTimer::tick() / 1000.0);
        pc->bindPanAxis();
        pc->slotControllerInputChanged(kStickUniverse, 0, 127, QString());
        pc->bindTiltAxis();
        pc->slotControllerInputChanged(kStickUniverse, 1, 127, QString());
    }

    void use(Scene *scene)
    {
        for (Scene *s : { rig.targetScene, rig.freeScene })
            if (s != scene && s->isRunning())
                s->stop(FunctionParent::master());
        for (int i = 0; i < 3; ++i)
        {
            frame();
            QCoreApplication::processEvents();
        }
        rig.subject->setPosition(rig.home);
        pc->m_focusedSceneId = scene->id();
        pc->m_stageAimValid = false;
        if (!scene->isRunning())
            scene->start(timer, FunctionParent::master());
        for (int i = 0; i < 10; ++i)
        {
            tick();
            QCoreApplication::processEvents();
        }
    }

    /* Hold the stick at @p x for @p ticks frames and measure the steady
       part (after @p settle frames). */
    Run hold(uchar x, int ticks = 150, int settle = 30, float planeZ = 0.0f)
    {
        Run r;
        stick(x);
        QVector<double> s16, s8;
        QSet<int> pans16, pans8;
        QVector3D prev16, prev8, prevT;
        bool have = false;
        double dist16 = 0, dist8 = 0, distT = 0;
        double pan16First = -1, pan16Last = 0, pan8First = -1, pan8Last = 0;
        int counted = 0;
        for (int i = 0; i < ticks; ++i)
        {
            tick();
            const QByteArray d = dmx(doc);
            QVector3D a16, a8;
            const bool ok = landing(doc, rig.fine, d, planeZ, a16) && landing(doc, rig.coarse, d, planeZ, a8);
            const QVector3D t = rig.subject->position();
            if (i >= settle && ok && have)
            {
                const double d16 = (a16 - prev16).length(), d8 = (a8 - prev8).length();
                s16 << d16;
                s8 << d8;
                dist16 += d16;
                dist8 += d8;
                distT += (t - prevT).length();
                ++counted;
                const int a = int(rig.fine->address());
                const int p16 = (uchar(d.at(a)) << 8) | uchar(d.at(a + 1));
                const int p8 = uchar(d.at(int(rig.coarse->address())));
                pans16.insert(p16);
                pans8.insert(p8);
                pan16Last = 540.0 * p16 / 65535.0;
                pan8Last = 540.0 * p8 / 255.0;
                if (pan16First < 0)
                {
                    pan16First = pan16Last;
                    pan8First = pan8Last;
                }
            }
            if (ok)
            {
                prev16 = a16;
                prev8 = a8;
                prevT = t;
                have = true;
            }
        }
        stick(127);
        const double secs = counted * (MasterTimer::tick() / 1000.0);
        if (secs > 0)
        {
            r.speed16 = dist16 / secs;
            r.speed8 = dist8 / secs;
            r.targetSpeed = distT / secs;
            r.panRate16 = (pan16Last - pan16First) / secs;
            r.panRate8 = (pan8Last - pan8First) / secs;
        }
        r.cov16 = mean(s16) > 0 ? stdev(s16) / mean(s16) : 0;
        r.cov8 = mean(s8) > 0 ? stdev(s8) / mean(s8) : 0;
        int z = 0;
        for (double v : s8)
        {
            if (v < 1e-6)
                ++z;
            r.maxStep8 = qMax(r.maxStep8, v);
        }
        r.zeros8 = s8.isEmpty() ? 0 : double(z) / s8.size();
        r.dmxSteps16 = pans16.size();
        r.dmxSteps8 = pans8.size();
        r.steps16 = s16;
        return r;
    }
};

QString line(const QString &what, const Run &r)
{
    return QString("%1 | floor m/s 16b %2  8b %3  subject %4 | step CoV 16b %5  8b %6 | 8b frames still %7% "
                   "max jump %8 m | DMX pan values 16b %9  8b %10 | pan deg/s 16b %11  8b %12")
        .arg(what, -26)
        .arg(r.speed16, 0, 'f', 3).arg(r.speed8, 0, 'f', 3).arg(r.targetSpeed, 0, 'f', 3)
        .arg(r.cov16, 0, 'f', 3).arg(r.cov8, 0, 'f', 3)
        .arg(r.zeros8 * 100, 0, 'f', 0).arg(r.maxStep8, 0, 'f', 3)
        .arg(r.dmxSteps16).arg(r.dmxSteps8)
        .arg(r.panRate16, 0, 'f', 1).arg(r.panRate8, 0, 'f', 1);
}

}

void FollowSpotFeel_Test::initTestCase()
{
    m_doc = new Doc(this);
    /* The effect runner hears of starts and stops queued, as it does in
       the app (there the MasterTimer is another thread): here a direct
       call would come while the tick holds the universes it claims. */
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    MasterTimer *mt = m_doc->masterTimer();
    QObject::disconnect(mt, SIGNAL(functionStarted(quint32)), runner, SLOT(slotFunctionStarted(quint32)));
    QObject::disconnect(mt, SIGNAL(functionStopped(quint32)), runner, SLOT(slotFunctionStopped(quint32)));
    QObject::connect(mt, SIGNAL(functionStarted(quint32)), runner, SLOT(slotFunctionStarted(quint32)),
                     Qt::QueuedConnection);
    QObject::connect(mt, SIGNAL(functionStopped(quint32)), runner, SLOT(slotFunctionStopped(quint32)),
                     Qt::QueuedConnection);
}

void FollowSpotFeel_Test::cleanupTestCase()
{
    delete m_doc;
}

void FollowSpotFeel_Test::cleanup()
{
    MasterTimer *timer = m_doc->masterTimer();
    timer->m_stopAllFunctions = true;
    timer->timerTick();
    timer->timerTick();
    timer->m_stopAllFunctions = false;
    QCoreApplication::processEvents();
    m_doc->programmer()->clearAxisBindings();
    m_doc->clearContents();
}

/****************************************************************************
 * The feel, on its own
 ****************************************************************************/

/* The same stick throw left and right is the same speed, at every throw;
   inside the deadzone nothing, and no jump at its edge. */
void FollowSpotFeel_Test::theStickIsTheSameBothWays()
{
    for (double expo : { 0.0, 0.4, 1.0 })
    {
        double last = 0.0;
        for (int i = 0; i <= 100; ++i)
        {
            const double d = i / 100.0;
            const double right = FollowSpotDrive::shape(d, 0.1, expo);
            QCOMPARE(FollowSpotDrive::shape(-d, 0.1, expo), -right);
            QVERIFY2(right >= last, qPrintable(QString("not monotonic at %1").arg(d)));
            // Out of the deadzone it starts from nothing: no step at its edge.
            if (d > 0.1 && d < 0.12)
                QVERIFY2(right < 0.03, qPrintable(QString("jumps at the deadzone edge: %1").arg(right)));
            last = right;
        }
        QCOMPARE(FollowSpotDrive::shape(1.0, 0.1, expo), 1.0);
        QCOMPARE(FollowSpotDrive::shape(0.05, 0.1, expo), 0.0);
    }
    // Expo: fine near the centre (half throw well under half speed).
    QVERIFY(FollowSpotDrive::shape(0.55, 0.1, 0.4) < 0.5 * 0.85);

    FollowSpotDrive drive;
    drive.setCoarse(0.6, 0);
    const double r = drive.wanted().x();
    drive.setCoarse(-0.6, 0);
    QCOMPARE(drive.wanted().x(), -r);
}

/* From rest to full and back: the velocity rises and falls smoothly, never
   past what the stick asks (no overshoot), and never reverses on a stop. */
void FollowSpotFeel_Test::startsAndStopsAreEased()
{
    FollowSpotDrive drive;
    const double top = drive.settings().topSpeed;
    const double dt = 0.02;
    drive.setCoarse(1.0, 0);
    double v = 0, maxJump = 0;
    QVector<double> rise;
    for (int i = 0; i < 50; ++i)
    {
        drive.step(dt);
        const double nv = drive.velocity().x();
        QVERIFY2(nv >= v - 1e-12, qPrintable(QString("slowed while speeding up at %1").arg(i)));
        QVERIFY2(nv <= top + 1e-9, qPrintable(QString("overshot: %1 > %2").arg(nv).arg(top)));
        maxJump = qMax(maxJump, nv - v);
        rise << nv;
        v = nv;
    }
    // 95% of the way there within a quarter of a second, and no step bigger
    // than a quarter of Top Speed between two 20 ms frames (it was all of it).
    QVERIFY2(rise.at(12) > 0.95 * top, qPrintable(QString::number(rise.at(12))));
    QVERIFY2(maxJump < 0.25 * top, qPrintable(QString::number(maxJump)));
    QVERIFY(rise.first() > 0.0);        // it starts at once

    drive.setCoarse(0, 0);
    double stopJump = 0;
    for (int i = 0; i < 60; ++i)
    {
        drive.step(dt);
        const double nv = drive.velocity().x();
        QVERIFY2(nv <= v + 1e-12, qPrintable(QString("sped up while stopping at %1").arg(i)));
        QVERIFY2(nv >= 0.0, qPrintable(QString("reversed: %1").arg(nv)));
        stopJump = qMax(stopJump, v - nv);
        v = nv;
    }
    QVERIFY2(stopJump < 0.25 * top, qPrintable(QString::number(stopJump)));
    QCOMPARE(drive.velocity().x(), 0.0);   // stopped, not creeping
    QVERIFY(!drive.isActive());

    // Reversing hard: through zero once, no ringing.
    drive.setCoarse(1.0, 0);
    for (int i = 0; i < 50; ++i)
        drive.step(dt);
    drive.setCoarse(-1.0, 0);
    int signChanges = 0;
    double prev = drive.velocity().x();
    for (int i = 0; i < 80; ++i)
    {
        drive.step(dt);
        const double nv = drive.velocity().x();
        if ((nv < 0) != (prev < 0))
            ++signChanges;
        QVERIFY2(nv >= -top - 1e-9, qPrintable(QString("overshot the other way: %1").arg(nv)));
        prev = nv;
    }
    QCOMPARE(signChanges, 1);
}

/* How far it goes is the time the ticks took, however they are cut up: a
   second of 10 ms ticks, of 20 ms, of 33 ms, or a mixture, all the same. */
void FollowSpotFeel_Test::theTimeATickTookIsTheTimeMoved()
{
    auto travel = [](const QVector<double> &dts) {
        FollowSpotDrive d;
        d.setCoarse(1.0, 0);
        double x = 0, t = 0;
        int i = 0;
        while (t < 2.0 - 1e-9)
        {
            const double dt = qMin(dts.at(i++ % dts.size()), 2.0 - t);
            x += d.step(dt).x();
            t += dt;
        }
        return x;
    };
    const double ref = travel({ 0.02 });
    for (const QVector<double> &dts : { QVector<double>{ 0.01 }, QVector<double>{ 1.0 / 30 },
                                        QVector<double>{ 0.004, 0.036, 0.02, 0.031, 0.009 } })
    {
        const double x = travel(dts);
        QVERIFY2(qAbs(x - ref) < 1e-6 * ref, qPrintable(QString("%1 vs %2").arg(x, 0, 'f', 9).arg(ref, 0, 'f', 9)));
    }
    // Two seconds from rest at full: Top Speed for all but the ease in.
    const double top = FollowSpotDrive::kDefaultTopSpeed;
    // (the ease in costs 2 / kResponse seconds of Top Speed)
    const double eased = 2.0 * top - 2.0 / FollowSpotDrive::kResponse * top;
    QVERIFY2(qAbs(ref - eased) < 0.001, qPrintable(QString("%1, want %2").arg(ref).arg(eased)));

    // A stall is not one big jump; a burst of ticks at once is no time.
    TickClock clock(0.02);
    QCOMPARE(clock.next(), 0.02);       // the first tick: nominal
    QVERIFY(clock.next() < 0.02);       // straight after: next to nothing
    clock.setFixed(0.05);
    QCOMPARE(clock.next(), 0.05);
}

/* The D-pad's steps: x0.25, x0.5, x1, x1.5, x2, held at the ends. */
void FollowSpotFeel_Test::speedSteps()
{
    FollowSpotDrive d;
    QCOMPARE(d.multiplier(), 1.0);
    QVERIFY(d.stepMultiplier(1));
    QCOMPARE(d.multiplier(), 1.5);
    QVERIFY(d.stepMultiplier(1));
    QCOMPARE(d.multiplier(), 2.0);
    QVERIFY(!d.stepMultiplier(1));
    QCOMPARE(d.multiplier(), 2.0);
    for (double want : { 1.5, 1.0, 0.5, 0.25 })
    {
        QVERIFY(d.stepMultiplier(-1));
        QCOMPARE(d.multiplier(), want);
    }
    QVERIFY(!d.stepMultiplier(-1));
    d.setCoarse(1.0, 0);
    QCOMPARE(d.wanted().x(), 0.25 * d.settings().topSpeed);
    QCOMPARE(ProgrammerController::followSpotSpeedText(1.5), QString::fromUtf8("Speed ×1.5"));
    QCOMPARE(ProgrammerController::followSpotSpeedText(0.25), QString::fromUtf8("Speed ×0.25"));
}

/* followspot.js declares the same defaults the host uses when its look
   sets none (Top Speed a brisk walk, 6 ft/s). */
void FollowSpotFeel_Test::theScriptAndTheHostAgree()
{
    EffectScript script;
    QVERIFY(script.load(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/followspot.js"));
    QHash<QString, EffectScript::ParamDef> defs;
    for (const EffectScript::ParamDef &d : script.paramDefs())
        defs.insert(d.name, d);
    QVERIFY(defs.contains("topSpeed") && defs.contains("fineStick") && defs.contains("responseCurve"));
    QCOMPARE(defs.value("topSpeed").unit, QString("cm/s"));
    QVERIFY(qAbs(defs.value("topSpeed").defaultValue / 100.0 - FollowSpotDrive::kDefaultTopSpeed) < 1e-6);
    QVERIFY(qAbs(defs.value("fineStick").defaultValue / 100.0 - FollowSpotDrive::kDefaultFineShare) < 1e-6);
    QVERIFY(qAbs(defs.value("responseCurve").defaultValue / 100.0 - FollowSpotDrive::kDefaultExpo) < 1e-6);
    // 6 ft/s, a brisk walk.
    QVERIFY(qAbs(FollowSpotDrive::kDefaultTopSpeed - 6 * 0.3048) < 1e-9);
    // Title Case where the Show Tree names them (the description up to "(").
    for (const char *name : { "topSpeed", "fineStick", "responseCurve" })
    {
        const QString label = defs.value(name).description.section(QLatin1Char('('), 0, 0).trimmed();
        for (const QString &w : label.split(QLatin1Char(' ')))
            QVERIFY2(w.at(0).isUpper(), qPrintable(label));
    }
}

/****************************************************************************
 * On the real path
 ****************************************************************************/

/* The left stick at full throw: the subject walks at Top Speed (a brisk
   walk), and every DMX frame moves it the same distance. */
void FollowSpotFeel_Test::theSubjectWalksAtTopSpeedEvenly()
{
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();
    h.use(h.rig.targetScene);
    const Run r = h.hold(255, 100, 20);
    const double top = FollowSpotDrive::kDefaultTopSpeed;
    QVERIFY2(qAbs(r.targetSpeed - top) < 0.01 * top, qPrintable(QString::number(r.targetSpeed)));
    QVERIFY2(r.cov16 < 0.03, qPrintable(QString("16-bit head's floor steps uneven: CoV %1").arg(r.cov16)));
}

/* The controller's tick and the MasterTimer are two clocks; whichever runs
   more often, or late, every DMX frame moves the subject once by one
   frame's time (it was: frames with no move, frames with two). */
void FollowSpotFeel_Test::everyFrameMovesOnceWhateverTheClocks()
{
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();
    h.use(h.rig.targetScene);
    h.stick(255);
    // The controller ticks 0, 1, 2 or 3 times between frames, as two
    // drifting 50 Hz clocks (and a busy main thread) do.
    const int pattern[] = { 1, 1, 0, 2, 1, 3, 0, 1, 2, 0, 1, 1 };
    for (int i = 0; i < 40; ++i)            // up to speed
        h.tick();
    QVector<double> steps;
    QVector3D prev = h.rig.subject->position();
    for (int i = 0; i < 120; ++i)
    {
        for (int k = 0; k < pattern[i % 12]; ++k)
            h.pc->applyDesignJoystick();
        h.frame();
        const QVector3D now = h.rig.subject->position();
        steps << double((now - prev).length());
        prev = now;
    }
    h.stick(127);
    const double m = mean(steps), cov = stdev(steps) / m;
    QVERIFY2(cov < 0.01, qPrintable(QString("per-frame step CoV %1 (mean %2 mm)").arg(cov).arg(m * 1000)));
    QVERIFY2(qAbs(m - FollowSpotDrive::kDefaultTopSpeed * 0.02) < 0.001, qPrintable(QString::number(m)));
}

/* Left and right, the same throw: the same speed on the stage, at full,
   half and light throw (within 1%). */
void FollowSpotFeel_Test::leftAndRightAtTheSameSpeed()
{
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();
    for (const QPair<uchar, uchar> &lr : { qMakePair(uchar(0), uchar(255)), qMakePair(uchar(64), uchar(191)),
                                          qMakePair(uchar(100), uchar(155)) })
    {
        h.use(h.rig.targetScene);
        const Run l = h.hold(lr.first, 80, 20);
        h.use(h.rig.targetScene);
        const Run r = h.hold(lr.second, 80, 20);
        QVERIFY2(l.targetSpeed > 0.05, qPrintable(QString::number(l.targetSpeed)));
        QVERIFY2(qAbs(l.targetSpeed - r.targetSpeed) < 0.01 * l.targetSpeed,
                 qPrintable(QString("%1 / %2: %3 vs %4 m/s").arg(lr.first).arg(lr.second)
                            .arg(l.targetSpeed).arg(r.targetSpeed)));
        // The 16-bit head's beam on the floor too.
        QVERIFY2(qAbs(l.speed16 - r.speed16) < 0.02 * l.speed16,
                 qPrintable(QString("floor %1 vs %2").arg(l.speed16).arg(r.speed16)));
    }
}

/* The right stick is fine (a fifth of Top Speed at full throw), the two
   sticks add, and the D-pad's step scales them both. */
void FollowSpotFeel_Test::fineStickAndSpeedStepAdd()
{
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();
    const double top = FollowSpotDrive::kDefaultTopSpeed;
    auto speed = [&](float fine, uchar coarse) {
        h.use(h.rig.targetScene);
        h.pc->m_fineNormX = fine;
        const Run r = h.hold(coarse, 80, 25);
        h.pc->m_fineNormX = 0.5f;
        return r.targetSpeed;
    };
    const double fineOnly = speed(1.0f, 127);
    QVERIFY2(qAbs(fineOnly - 0.2 * top) < 0.01 * top, qPrintable(QString::number(fineOnly)));
    const double both = speed(1.0f, 255);
    QVERIFY2(qAbs(both - 1.2 * top) < 0.01 * top, qPrintable(QString::number(both)));
    const double against = speed(0.0f, 255);
    QVERIFY2(qAbs(against - 0.8 * top) < 0.01 * top, qPrintable(QString::number(against)));

    h.pc->followSpotMover()->setMultiplier(2.0);
    const double doubled = speed(0.5f, 255);
    QVERIFY2(qAbs(doubled - 2.0 * top) < 0.02 * top, qPrintable(QString::number(doubled)));
    h.pc->followSpotMover()->setMultiplier(0.25);
    const double quarter = speed(0.5f, 255);
    QVERIFY2(qAbs(quarter - 0.25 * top) < 0.01 * top, qPrintable(QString::number(quarter)));
    h.pc->followSpotMover()->setMultiplier(1.0);
}

/* Top Speed, Fine Stick and Response Curve are the follow spot look's. */
void FollowSpotFeel_Test::topSpeedIsTheLooks()
{
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.rig.follow->setEffectParamValue("topSpeed", 300.0);     // cm/s
    h.rig.follow->setEffectParamValue("responseCurve", 0.0);
    h.bind();
    h.use(h.rig.targetScene);
    const Run full = h.hold(255, 80, 25);
    QVERIFY2(qAbs(full.targetSpeed - 3.0) < 0.03, qPrintable(QString::number(full.targetSpeed)));
    QCOMPARE(h.pc->followSpotMover()->settings().expo, 0.0);
    // Straight response: half the throw past the deadzone is half the speed.
    h.use(h.rig.targetScene);
    const double dz = h.pc->followSpotMover()->settings().deadzone;
    const uchar half = uchar(qRound(127.5 + 127.5 * (dz + (1 - dz) / 2)));
    const Run r = h.hold(half, 80, 25);
    QVERIFY2(qAbs(r.targetSpeed - 1.5) < 0.05, qPrintable(QString::number(r.targetSpeed)));
}

/* A 16-bit head's beam moves every frame by an even step; an 8-bit one
   can only move by its DMX step, whatever the software does. */
void FollowSpotFeel_Test::aSixteenBitHeadGlides()
{
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();
    h.use(h.rig.targetScene);
    const Run r = h.hold(255, 100, 20);
    QVERIFY2(r.cov16 < 0.03, qPrintable(QString::number(r.cov16)));
    QVERIFY2(r.dmxSteps16 >= 75, qPrintable(QString::number(r.dmxSteps16)));   // a new value nearly every frame
    QVERIFY2(r.zeros8 > 0.3, qPrintable(QString::number(r.zeros8)));          // the 8-bit head holds, then jumps
}

/* Free-fly (no Aim look): the effect pans each head at the host's speed
   over its throw -- Top Speed at full throw, the same both ways -- over
   the time its ticks really took. */
void FollowSpotFeel_Test::freeFlyPansAtTopSpeedBothWays()
{
    Rig rig = makeRig(m_doc);
    EffectInstance inst(m_doc, rig.freeScene->id(), rig.follow->id());
    QVERIFY(inst.isValid());
    const QVector3D centre = m_doc->monitorProperties()->stageCentre();
    auto panRate = [&](double vx) {
        QHash<QString, QVariantMap> ch;
        QVariantMap jd;
        jd["pan"] = 0.5;
        jd["tilt"] = 0.5;
        jd["vx"] = vx;
        jd["vy"] = 0.0;
        jd["cx"] = double(centre.x());
        jd["cy"] = double(centre.y());
        jd["cz"] = double(centre.z());
        ch.insert("joystick", jd);
        inst.setDataChannels(ch);
        qint64 ms = 1000;
        inst.setOffStageClock(ms);
        inst.runTick();
        const double from = inst.lastIntentDegrees().value(rig.fine->id()).x();
        const qint64 start = ms;
        for (int i = 0; i < 25; ++i)
        {
            ms += (i % 3 == 0) ? 30 : 15;       // uneven ticks
            inst.setOffStageClock(ms);
            inst.runTick();
        }
        return (inst.lastIntentDegrees().value(rig.fine->id()).x() - from) / ((ms - start) / 1000.0);
    };
    const double top = FollowSpotDrive::kDefaultTopSpeed;
    const double right = panRate(top);
    const double left = panRate(-top);
    AimSolver::Frame frame;
    QVERIFY(AimSolver::frameFor(m_doc, rig.fine->id(), frame));
    const double throwM = double((frame.position - centre).length());
    const double want = qRadiansToDegrees(top / throwM);
    QVERIFY2(qAbs(right - want) < 0.02 * want, qPrintable(QString("%1 deg/s, want %2").arg(right).arg(want)));
    QVERIFY2(qAbs(left + right) < 0.01 * want, qPrintable(QString("%1 vs %2").arg(left).arg(right)));
}


/****************************************************************************
 * Where the operator stands
 ****************************************************************************/

namespace
{
/* The subject's floor point after the stick is held at (@p x, @p y) for
   @p ticks frames, from the rig's home. */
QVector3D pushed(Harness &h, uchar x, uchar y, int ticks = 40)
{
    h.use(h.rig.targetScene);
    const QVector3D from = h.rig.subject->position();
    h.stick(x, y);
    for (int i = 0; i < ticks; ++i)
        h.tick();
    h.stick(127, 127);
    return h.rig.subject->position() - from;
}

QString str(const QVector3D &v)
{
    return QString("(%1, %2)").arg(double(v.x()), 0, 'f', 3).arg(double(v.y()), 0, 'f', 3);
}
}

/* From every preset the stick pushed up sends the subject the way the
   operator faces -- away from where they stand -- and right is their right. */
void FollowSpotFeel_Test::stickUpGoesAwayFromTheOperator()
{
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();
    MonitorProperties *mp = m_doc->monitorProperties();
    for (FollowSpotOperator::Preset p : FollowSpotOperator::presets())
    {
        const FollowSpotOperator op = FollowSpotOperator::fromPreset(p);
        h.pc->setFollowSpotOperator(op);
        QCOMPARE(h.pc->followSpotOperator(), op);
        QCOMPARE(h.pc->followSpotMover()->facing(), op.facing);
        const double r = qDegreesToRadians(double(op.facing));
        const QVector3D fwd(float(std::sin(r)), float(-std::cos(r)), 0.0f);
        const QVector3D right(float(std::cos(r)), float(std::sin(r)), 0.0f);
        const QPointF at = op.position(mp);
        const QVector3D stands(float(at.x()), float(at.y()), 0.0f);

        const QVector3D home = h.rig.home;
        const QVector3D up = pushed(h, 127, 0);
        const QString name = FollowSpotOperator::presetName(p);
        QVERIFY2(up.length() > 0.2f, qPrintable(name + " " + str(up)));
        QVERIFY2(QVector3D::dotProduct(up.normalized(), fwd) > 0.99f, qPrintable(name + " up " + str(up)));
        // Away from them: further from where they stand.
        QVERIFY2((home + up - stands).length() > (home - stands).length() + 0.2f,
                 qPrintable(name + " up " + str(up) + " from " + str(stands)));
        QVERIFY2(qAbs(up.z()) < 1e-5f, qPrintable(name));

        const QVector3D toRight = pushed(h, 255, 127);
        QVERIFY2(QVector3D::dotProduct(toRight.normalized(), right) > 0.99f,
                 qPrintable(name + " right " + str(toRight)));
    }
    h.pc->setFollowSpotOperator(FollowSpotOperator());
}

/* Front of house -- every show that never sets one -- is as it was across
   (right is stage left, +X) and the stick pushed up goes upstage (-Y),
   away from the house. */
void FollowSpotFeel_Test::frontOfHouseKeepsItsRightAndGoesUpstage()
{
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();
    QVERIFY(h.pc->followSpotOperator().isFrontOfHouse());
    QCOMPARE(h.pc->followSpotMover()->facing(), 0);
    QVERIFY(h.pc->followSpotOperator().viewpointText().isEmpty());
    const QVector3D right = pushed(h, 255, 127);
    QVERIFY2(right.x() > 0.2f && qAbs(right.y()) < 0.01f * right.x(), qPrintable(str(right)));
    const QVector3D left = pushed(h, 0, 127);
    QVERIFY2(qAbs(left.x() + right.x()) < 0.01f * right.x(), qPrintable(str(left) + str(right)));
    const QVector3D up = pushed(h, 127, 0);
    QVERIFY2(up.y() < -0.2f && qAbs(up.x()) < 0.01f * -up.y(), qPrintable(str(up)));   // the last push's glide aside
    // The same speed as across: only the direction is the operator's.
    QVERIFY2(qAbs(up.length() - right.length()) < 0.01f * right.length(), qPrintable(str(up) + str(right)));

    // The turn is the sticks', worked out once: the same as onFloor().
    const QPointF w = FollowSpotOperator::onFloor(0.3, 0.7, 270);
    QVERIFY(qAbs(w.x() - (-0.7)) < 1e-9 && qAbs(w.y() - (-0.3)) < 1e-9);
}

/* Free-fly (no Aim look: each head flown from the stick) is the head's own
   pan and tilt, from wherever it is seen: not turned. */
void FollowSpotFeel_Test::freeFlyIsNotTurned()
{
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();
    h.pc->setFollowSpotOperator(FollowSpotOperator::fromPreset(FollowSpotOperator::StageLeftTower));
    h.use(h.rig.freeScene);
    h.stick(255, 127);
    for (int i = 0; i < 30; ++i)
        h.tick();
    const QPointF v = h.pc->followSpotMover()->velocity();
    h.stick(127, 127);
    QVERIFY2(v.x() > 0.5 && qAbs(v.y()) < 1e-6, qPrintable(QString("%1, %2").arg(v.x()).arg(v.y())));
    h.pc->setFollowSpotOperator(FollowSpotOperator());
}

namespace
{
QByteArray saveShow(Doc *doc)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.writeStartDocument();
    w.writeStartElement("Workspace");
    doc->saveXML(&w);
    w.writeEndElement();
    w.writeEndDocument();
    return xml;
}

bool loadShow(Doc *doc, const QByteArray &xml)
{
    doc->clearContents();
    QXmlStreamReader r(xml);
    r.readNextStartElement();       // Workspace
    r.readNextStartElement();       // Engine
    return doc->loadXML(r, false);
}
}

/* Kept per controller and per venue, with the show: through a save and a
   load each comes back where it was set, and a venue renamed takes its own. */
void FollowSpotFeel_Test::theOperatorIsKeptPerStickAndVenue()
{
    ProgrammerController *pc = m_doc->programmer();
    FollowSpotOperator sl = FollowSpotOperator::fromPreset(FollowSpotOperator::StageLeftTower);
    sl.facing = 255;
    sl.eyeMm = 4572;
    const FollowSpotOperator boh = FollowSpotOperator::fromPreset(FollowSpotOperator::BackOfHouse);
    const FollowSpotOperator us = FollowSpotOperator::fromPreset(FollowSpotOperator::Upstage);
    pc->setFollowSpotOperatorFor("Hall", "DUALSHOCK 4", sl);
    pc->setFollowSpotOperatorFor("Hall", QString(), boh);
    pc->setFollowSpotOperatorFor(QString(), "DualSense", us);
    // The venue's for any stick stands in for a stick it has none for.
    QCOMPARE(pc->followSpotOperatorFor("Hall", "Xbox"), boh);
    QVERIFY(pc->followSpotOperatorFor("Barn", "DUALSHOCK 4").isFrontOfHouse());
    // Front of house set where nothing else was is not kept (the show saves as before).
    pc->setFollowSpotOperatorFor("Barn", "Xbox", FollowSpotOperator());
    QCOMPARE(pc->followSpotOperators().size(), 3);

    const QByteArray xml = saveShow(m_doc);
    QVERIFY2(xml.contains("<FollowSpotOperators>"), xml.constData());
    QVERIFY(xml.contains("Venue=\"Hall\""));
    QVERIFY(xml.contains("Eye=\"4572\""));      // millimetres
    QVERIFY(loadShow(m_doc, xml));
    QCOMPARE(pc->followSpotOperators().size(), 3);
    QCOMPARE(pc->followSpotOperatorFor("Hall", "DUALSHOCK 4"), sl);
    QCOMPARE(pc->followSpotOperatorFor("Hall", QString()), boh);
    QCOMPARE(pc->followSpotOperatorFor(QString(), "DualSense"), us);
    QCOMPARE(pc->followSpotOperatorFor("Hall", "DUALSHOCK 4").viewpointText(),
             QString::fromUtf8("from SL tower, facing 255°"));

    // A venue renamed keeps its operators.
    VenueCorrections *vc = pc->venueCorrections();
    QCOMPARE(vc->addVenue("Hall"), QStringLiteral("Hall"));
    QVERIFY(vc->renameVenue("Hall", "Main Hall"));
    QCOMPARE(pc->followSpotOperatorFor("Main Hall", "DUALSHOCK 4"), sl);
    QVERIFY(pc->followSpotOperatorFor("Hall", "DUALSHOCK 4").isFrontOfHouse());
    QVERIFY(vc->removeVenue("Main Hall"));

    // A new show has none.
    m_doc->clearContents();
    QVERIFY(pc->followSpotOperators().isEmpty());
}

/* A show from before any of this: no <FollowSpotOperators>, nothing new
   written, front of house. */
void FollowSpotFeel_Test::anOldShowIsFrontOfHouse()
{
    ProgrammerController *pc = m_doc->programmer();
    pc->setFollowSpotOperator(FollowSpotOperator::fromPreset(FollowSpotOperator::Upstage));
    QCOMPARE(pc->followSpotMover()->facing(), 180);
    m_doc->clearContents();
    const QByteArray xml = saveShow(m_doc);
    QVERIFY(!xml.contains("FollowSpotOperators"));
    QVERIFY(loadShow(m_doc, xml));
    QVERIFY(pc->followSpotOperators().isEmpty());
    QVERIFY(pc->followSpotOperator().isFrontOfHouse());
    QCOMPARE(pc->followSpotMover()->facing(), 0);
}

/* Hold LS and push toward centre stage: the facing is set from the push,
   to the nearest 15 degrees, and the subject holds while it is. */
void FollowSpotFeel_Test::recalibratingSnapsTo15Degrees()
{
    // The arithmetic.
    QCOMPARE(FollowSpotOperator::snapFacing(7.4), 0);
    QCOMPARE(FollowSpotOperator::snapFacing(7.6), 15);
    QCOMPARE(FollowSpotOperator::snapFacing(-20.0), 345);
    QCOMPARE(FollowSpotOperator::snapFacing(367.0), 0);
    const QPointF centre(5.0, 4.0);
    // Facing centre: pushed straight up.
    QCOMPARE(FollowSpotOperator::facingFromPush(QPointF(5.0, 20.0), centre, 0.0, 1.0), 0);
    QCOMPARE(FollowSpotOperator::facingFromPush(QPointF(12.0, 4.0), centre, 0.0, 1.0), 270);
    // Centre on their right: they face a quarter turn left of it.
    QCOMPARE(FollowSpotOperator::facingFromPush(QPointF(12.0, 4.0), centre, 1.0, 0.0), 180);
    // 20 degrees off up snaps to the next step.
    QCOMPARE(FollowSpotOperator::facingFromPush(QPointF(5.0, 20.0), centre,
                                                std::sin(qDegreesToRadians(20.0)),
                                                std::cos(qDegreesToRadians(20.0))), 345);
    // Too little push says nothing.
    QCOMPARE(FollowSpotOperator::facingFromPush(QPointF(5.0, 20.0), centre, 0.1, 0.2), -1);

    // On the controller: the SL tower, LS held, the stick pushed up and a
    // little left (centre stage ahead, a touch to their left), released.
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();
    h.pc->setFollowSpotOperator(FollowSpotOperator::fromPreset(FollowSpotOperator::StageLeftTower));
    h.use(h.rig.targetScene);
    const QVector3D from = h.rig.subject->position();
    h.pc->m_recalHeld = true;
    h.pc->m_recalX = h.pc->m_recalY = 0.0;
    const double a = qDegreesToRadians(-25.0);       // 25 degrees left of up
    h.stick(uchar(qRound(127.5 + 127.5 * std::sin(a))), uchar(qRound(127.5 - 127.5 * std::cos(a))));
    for (int i = 0; i < 20; ++i)
        h.tick();
    QVERIFY2((h.rig.subject->position() - from).length() < 1e-4f, "the subject moved while recalibrating");
    h.stick(127, 127);
    for (int i = 0; i < 3; ++i)
        h.tick();
    h.pc->finishFacingRecalibration();
    QVERIFY(!h.pc->isRecalibratingFacing());
    // 270 + 25 = 295, the nearest step 300 (25 / 15 = 1.67: two steps).
    QCOMPARE(h.pc->followSpotOperator().facing, 300);
    QCOMPARE(h.pc->followSpotOperator().preset, FollowSpotOperator::StageLeftTower);
    QCOMPARE(h.pc->followSpotMover()->facing(), 300);
    h.pc->setFollowSpotOperator(FollowSpotOperator());
}

/* The rig view from the operator: the angled view swung to look the way
   they face, looking down from their eye; the plan turned so they are at
   its bottom. */
void FollowSpotFeel_Test::theOperatorViewsTurnTheRigView()
{
    QCOMPARE(FollowSpotOperator::viewAzimuth(0), 0.0);       // from the audience
    QCOMPARE(FollowSpotOperator::viewAzimuth(270), 90.0);    // from stage left
    QCOMPARE(FollowSpotOperator::viewAzimuth(90), -90.0);
    QCOMPARE(FollowSpotOperator::viewAzimuth(180), 180.0);
    QCOMPARE(FollowSpotOperator::planTurns(0), 0);
    QCOMPARE(FollowSpotOperator::planTurns(270), 1);
    QCOMPARE(FollowSpotOperator::planTurns(180), 2);
    QCOMPARE(FollowSpotOperator::planTurns(90), 3);
    MonitorProperties *mp = m_doc->monitorProperties();
    const FollowSpotOperator foh = FollowSpotOperator::fromPreset(FollowSpotOperator::FrontOfHouse);
    const FollowSpotOperator boh = FollowSpotOperator::fromPreset(FollowSpotOperator::BackOfHouse);
    const FollowSpotOperator tower = FollowSpotOperator::fromPreset(FollowSpotOperator::StageLeftTower);
    QVERIFY(foh.viewElevation(mp) >= 5.0 && foh.viewElevation(mp) < tower.viewElevation(mp));
    QVERIFY(boh.viewElevation(mp) > 5.0);
    FollowSpotOperator high = tower;
    high.eyeMm = 9000;
    QVERIFY(high.viewElevation(mp) > tower.viewElevation(mp));
}

/* The scene being followed is deleted mid-move: the motion lets go of it
   before it goes (the MasterTimer thread holds it), and nothing ticks on
   into a deleted scene. */
void FollowSpotFeel_Test::deletingTheSceneLetsGo()
{
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();
    h.use(h.rig.targetScene);
    h.stick(255);
    for (int i = 0; i < 10; ++i)
        h.tick();
    FollowSpotMover *mover = h.pc->followSpotMover();
    QCOMPARE(mover->sceneId(), h.rig.targetScene->id());
    const quint32 id = h.rig.targetScene->id();
    h.rig.targetScene->stop(FunctionParent::master());
    h.frame();
    QCoreApplication::processEvents();
    QVERIFY(m_doc->deleteFunction(id));
    QVERIFY(!mover->isOn());
    for (int i = 0; i < 10; ++i)
        h.tick();
    h.stick(127);
    QVERIFY(!mover->isOn());
}

void FollowSpotFeel_Test::report()
{
    if (qEnvironmentVariableIsEmpty("QLC_FSPOT_REPORT"))
        QSKIP("QLC_FSPOT_REPORT=1 for the table");
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();

    struct Case { const char *name; uchar x; };
    /* Symmetric about the middle of the 0..255 reading (127.5); the DS4's
       half throw as the HID plugin reads it (63 / 191 before 0.4.2's
       rounding, 64 / 191 since) is in stickReadingIsSymmetric(). */
    const QList<Case> cases = { { "full left  (0)", 0 }, { "full right (255)", 255 },
                                { "half left  (64)", 64 }, { "half right (191)", 191 },
                                { "light left (100)", 100 }, { "light right (155)", 155 } };
    h.use(h.rig.targetScene);
    for (const Case &c : cases)
    {
        h.use(h.rig.targetScene);
        qInfo().noquote() << line(QString("target  ") + c.name, h.hold(c.x));
    }
    for (const Case &c : cases)
    {
        h.rig.freeScene->stop(FunctionParent::master());
        h.spacingMs = 20;
        h.use(h.rig.freeScene);
        // Tilted off straight down first, so a pan moves the beam on the floor.
        h.stick(127, 255);
        for (int i = 0; i < 60; ++i)
            h.tick();
        h.stick(127, 127);
        for (int i = 0; i < 25; ++i)
            h.tick();
        qInfo().noquote() << line(QString("freefly ") + c.name, h.hold(c.x, 60, 15));
    }
}

/* The real clocks: the MasterTimer thread at 50 Hz and the controller's
   own 20 ms QTimer on the main thread, as the app runs them. Where the
   subject is at each DMX frame (a probe source, ticked by the MasterTimer
   after the functions): how far it moved frame to frame. */
void FollowSpotFeel_Test::realTimers()
{
    if (qEnvironmentVariableIsEmpty("QLC_FSPOT_REPORT"))
        QSKIP("QLC_FSPOT_REPORT=1 to measure on the real clocks");
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();
    h.use(h.rig.targetScene);
    if (FollowSpotMover *m = h.pc->followSpotMover())
        m->setFixedDt(0);       // the real clocks

    struct Probe : public DMXSource
    {
        StageTarget *t;
        QMutex lock;
        QVector<QPair<qint64, QVector3D>> at;
        QElapsedTimer clock;
        void writeDMX(MasterTimer *, QList<Universe *>) override
        {
            QMutexLocker l(&lock);
            at.append({ clock.nsecsElapsed(), t->position() });
        }
    } probe;
    probe.t = h.rig.subject;
    probe.clock.start();

    MasterTimer *mt = m_doc->masterTimer();
    mt->start();
    mt->registerDMXSource(&probe);
    QTest::qWait(300);
    h.stick(255);
    QTest::qWait(500);
    {
        QMutexLocker l(&probe.lock);
        probe.at.clear();
    }
    QTest::qWait(3000);
    mt->unregisterDMXSource(&probe);
    h.stick(127);
    QVector<QPair<qint64, QVector3D>> at;
    {
        QMutexLocker l(&probe.lock);
        at = probe.at;
    }
    mt->stop();

    QVector<double> steps, gaps;
    int zeros = 0, doubles = 0;
    double total = 0;
    for (int i = 1; i < at.size(); ++i)
    {
        const double s = (at[i].second - at[i - 1].second).length();
        steps << s;
        gaps << (at[i].first - at[i - 1].first) / 1.0e6;
        total += s;
    }
    const double m = mean(steps);
    for (double s : steps)
    {
        if (s < m * 0.25)
            ++zeros;
        else if (s > m * 1.6)
            ++doubles;
    }
    const double secs = (at.last().first - at.first().first) / 1.0e9;
    qInfo().noquote() << QString("real clocks, full right: %1 frames in %2 s | MasterTimer frame %3 ms +- %4 | "
                                 "subject %5 m/s | per-frame step mean %6 mm CoV %7 | frames with no step %8 "
                                 "(%9%), with a double step %10")
                             .arg(at.size()).arg(secs, 0, 'f', 2).arg(mean(gaps), 0, 'f', 2)
                             .arg(stdev(gaps), 0, 'f', 2).arg(total / secs, 0, 'f', 3)
                             .arg(m * 1000, 0, 'f', 1).arg(stdev(steps) / m, 0, 'f', 3)
                             .arg(zeros).arg(100.0 * zeros / steps.size(), 0, 'f', 0).arg(doubles);
}

/* Free-fly on the real clocks: the effect runner ticks the script on its
   own 20 ms main-thread timer and the MasterTimer sends what it last
   published -- so a free-flying head's per-frame step is only as even as
   those two clocks are. Measured, not fixed here (every effect shares it). */
void FollowSpotFeel_Test::realTimersFreeFly()
{
    if (qEnvironmentVariableIsEmpty("QLC_FSPOT_REPORT"))
        QSKIP("QLC_FSPOT_REPORT=1 to measure on the real clocks");
    Harness h { m_doc, makeRig(m_doc), m_doc->masterTimer(), m_doc->programmer() };
    h.bind();
    h.spacingMs = 20;
    h.use(h.rig.freeScene);
    if (FollowSpotMover *m = h.pc->followSpotMover())
        m->setFixedDt(0);

    /* What the MasterTimer sends this frame: the runner's last published
       writes (its writeDMX() copies the same list). */
    struct Probe : public DMXSource
    {
        EffectScriptRunner *runner = nullptr;
        quint32 fixture = 0;
        QMutex lock;
        QVector<int> pans;
        void writeDMX(MasterTimer *, QList<Universe *>) override
        {
            QList<EffectInstance::DmxWrite> w;
            {
                QMutexLocker l(&runner->m_writesMutex);
                w = runner->m_publishedWrites;
            }
            int msb = -1, lsb = -1;
            for (const EffectInstance::DmxWrite &d : w)
                if (d.fixtureId == fixture && d.channel == 0)
                    msb = d.value;
                else if (d.fixtureId == fixture && d.channel == 1)
                    lsb = d.value;
            if (msb < 0 || lsb < 0)
                return;
            QMutexLocker l(&lock);
            pans << ((msb << 8) | lsb);
        }
    } probe;
    probe.runner = m_doc->effectScriptRunner();
    probe.fixture = h.rig.fine->id();
    MasterTimer *mt = m_doc->masterTimer();
    mt->start();
    QTest::qWait(200);
    h.stick(127, 255);          // off straight down first
    QTest::qWait(1500);
    h.stick(255, 127);
    QTest::qWait(500);
    mt->registerDMXSource(&probe);
    QTest::qWait(3000);
    mt->unregisterDMXSource(&probe);
    h.stick(127, 127);
    mt->stop();
    QVector<double> steps;
    int zeros = 0, doubles = 0;
    for (int i = 1; i < probe.pans.size(); ++i)
        steps << std::abs(probe.pans[i] - probe.pans[i - 1]);
    const double m = mean(steps);
    for (double st : steps)
    {
        if (st < m * 0.25)
            ++zeros;
        else if (st > m * 1.6)
            ++doubles;
    }
    qInfo().noquote() << QString("real clocks, free-fly full right: %1 frames | per-frame 16-bit pan step mean %2 "
                                 "CoV %3 | frames with no step %4, with a double step %5")
                             .arg(steps.size()).arg(m, 0, 'f', 1).arg(stdev(steps) / m, 0, 'f', 3)
                             .arg(zeros).arg(doubles);
}

QTEST_MAIN(FollowSpotFeel_Test)

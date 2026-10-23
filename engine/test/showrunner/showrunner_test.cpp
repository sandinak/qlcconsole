/*
  Q Light Controller Plus - Test Unit
  showrunner_test.cpp

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
#define private public
#define protected public                // Universe::processFaders()
#include "mastertimer.h"                // MasterTimer::timerTick()
#include "universe.h"
#include "showrunner.h"
#undef protected
#undef private
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "lastlookeffect.h"
#include "fixture.h"
#include "show.h"
#include "track.h"
#include "scene.h"
#include "doc.h"
#include "showrunner_test.h"

void ShowRunner_Test::initTestCase()
{
    m_doc = new Doc(this);
    m_show = new Show(m_doc);
    m_doc->addFunction(m_show);
    m_scene = new Scene(m_doc);
    m_doc->addFunction(m_scene);
    m_track = new Track(m_scene->id());
    ShowFunction *sf = new ShowFunction(m_show->getLatestShowFunctionId());
    sf->setFunctionID(m_scene->id());
    sf->setStartTime(0);
    sf->setDuration(1000);
    m_track->addShowFunction(sf);
    m_show->addTrack(m_track);
}

void ShowRunner_Test::cleanupTestCase()
{
    delete m_doc;
}

void ShowRunner_Test::initRunner()
{
    ShowRunner runner(m_doc, m_show->id());
    QCOMPARE(runner.m_timeFunctions.count(), 1);
    QCOMPARE(runner.m_totalRunTime, quint32(1000));
}

void ShowRunner_Test::intensity()
{
    ShowRunner runner(m_doc, m_show->id());
    // adjustIntensity() now QUEUES the change (thread-safe vs the timer thread);
    // the timer thread applies it via applyPendingIntensity() in write().
    runner.adjustIntensity(0.5, m_track);
    runner.applyPendingIntensity();
    QCOMPARE(runner.m_intensityMap[m_track->id()], 0.5);
}

void ShowRunner_Test::stopRunner()
{
    ShowRunner runner(m_doc, m_show->id());
    runner.m_elapsedTime = 500;
    runner.m_runningQueue.append(QPair<Function*,quint32>(m_scene,1000));
    runner.stop();
    QCOMPARE(runner.m_elapsedTime, quint32(0));
    QCOMPARE(runner.m_runningQueue.count(), 0);
}

void ShowRunner_Test::timecodeFollow()
{
    ShowRunner runner(m_doc, m_show->id());

    // Off by default.
    QCOMPARE(runner.m_timecodeFollow, false);

    runner.setTimecodeFollow(true);
    QCOMPARE(runner.m_timecodeFollow, true);
    QCOMPARE(runner.m_externalTimeSet, false);

    runner.setExternalTime(5000);
    QCOMPARE(runner.m_externalTime, quint32(5000));
    QCOMPARE(runner.m_externalTimeSet, true);

    // Seek past the only function (start 0, dur 1000): it is skipped so the
    // start phase would not restart it.
    runner.seekTo(5000);
    QCOMPARE(runner.m_elapsedTime, quint32(5000));
    QCOMPARE(runner.m_currentTimeFunctionIndex, runner.m_timeFunctions.count());

    // Seek into the middle of the function: it stays active (index 0).
    runner.seekTo(500);
    QCOMPARE(runner.m_elapsedTime, quint32(500));
    QCOMPARE(runner.m_currentTimeFunctionIndex, 0);

    // Disabling follow clears the external-time latch.
    runner.setTimecodeFollow(false);
    QCOMPARE(runner.m_externalTimeSet, false);
}

// Drives the actual write()-based timecode chase → freeze → thaw (the MTC drive
// that was previously only exercised end-to-end on a rig).
void ShowRunner_Test::timecodeDriveAndFreeze()
{
    ShowRunner runner(m_doc, m_show->id());
    runner.setTimecodeFollow(true);
    MasterTimer *mt = m_doc->masterTimer();
    QList<Universe*> unis = m_doc->inputOutputMap()->universes();

    // Fresh code + a tick: the clock resyncs toward it and we are NOT holding.
    runner.setExternalTime(400);
    runner.write(mt, unis);
    QVERIFY(runner.m_elapsedTime > 0);       // chasing the external position
    QCOMPARE(runner.m_tcHolding, false);

    // Keep feeding fresh code: stays live.
    for (int i = 0; i < 5; i++) { runner.setExternalTime(400 + i * 20); runner.write(mt, unis); }
    QCOMPARE(runner.m_tcHolding, false);

    // Stop feeding (transport stops): after SHOW_TC_HOLD_MS of silence it FREEZES.
    int guard = 0;
    while (runner.m_tcHolding == false && guard++ < 1000)
        runner.write(mt, unis);
    QCOMPARE(runner.m_tcHolding, true);      // held on TC-stop

    // Fresh code again thaws it (resume chasing).
    runner.setExternalTime(600);
    runner.write(mt, unis);
    QCOMPARE(runner.m_tcHolding, false);

    runner.stop();
}

/****************************************************************************
 * Track flags at playback (docs/features/shows.md, "A track's flags")
 ****************************************************************************/

namespace
{
/* A show of its own, on a Doc of its own: four dimmers at 0..3 (HTP) and a
   mover's pan at 10 (LTP), so what each track plays reads straight off the
   universe. Ticked by hand: no wall clock anywhere. */
struct Rig
{
    Doc *doc = nullptr;
    Show *show = nullptr;
    QList<quint32> dims;
    quint32 pan = Fixture::invalidId();

    explicit Rig(QObject *parent)
    {
        doc = new Doc(parent);
        for (int i = 0; i < 4; ++i)
        {
            Fixture *fx = new Fixture(doc);
            fx->setChannels(1);
            fx->setUniverse(0);
            fx->setAddress(quint32(i));
            doc->addFixture(fx);
            dims << fx->id();
        }
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test");
        def->setModel("Pan only");
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("1ch");
        QLCChannel *ch = new QLCChannel();
        ch->setName("Pan");
        ch->setGroup(QLCChannel::Pan);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, 0);
        def->addMode(mode);
        Fixture *mover = new Fixture(doc);
        mover->setFixtureDefinition(def, mode);
        mover->setUniverse(0);
        mover->setAddress(10);
        doc->addFixture(mover);
        pan = mover->id();

        show = new Show(doc);
        doc->addFunction(show);
    }

    ~Rig()
    {
        if (show->isRunning())
            show->stop(FunctionParent::master());
        foreach (Function *f, doc->functions())
            if (f->isRunning())
                f->stop(FunctionParent::master());
        tick(5);
        delete doc;
    }

    /** A scene putting @a value on channel 0 of @a fixture. */
    Scene *scene(quint32 fixture, uchar value)
    {
        Scene *s = new Scene(doc);
        s->setValue(fixture, 0, value);
        doc->addFunction(s);
        return s;
    }

    /** A new track, playing @a s from @a start for @a duration ms. */
    Track *track(Scene *s, quint32 start, quint32 duration)
    {
        Track *t = new Track(s->id(), show);
        show->addTrack(t);
        item(t, s, start, duration);
        return t;
    }

    void item(Track *t, Scene *s, quint32 start, quint32 duration)
    {
        ShowFunction *sf = new ShowFunction(show->getLatestShowFunctionId());
        sf->setFunctionID(s->id());
        sf->setStartTime(start);
        sf->setDuration(duration);
        t->addShowFunction(sf);
    }

    void tick(int n)
    {
        for (int i = 0; i < n; ++i)
        {
            doc->masterTimer()->timerTick();
            QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
            ua.first()->processFaders();
            doc->inputOutputMap()->releaseUniverses(false);
        }
    }

    /** Ticks until the show has played @a ms (it starts on the first). */
    void playTo(quint32 ms)
    {
        tick(int(ms / MasterTimer::tick()) + 1);
    }

    int at(int address)
    {
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        const int v = uchar(ua.first()->preGMValues().at(address));
        doc->inputOutputMap()->releaseUniverses(false);
        return v;
    }

    void start()
    {
        show->start(doc->masterTimer(), FunctionParent::master());
    }
};
}

void ShowRunner_Test::aSoloTrackPlaysAndTheOthersOnlyIfSoloSafe()
{
    Rig rig(this);
    Track *solo = rig.track(rig.scene(rig.dims[0], 200), 0, 2000);
    Track *other = rig.track(rig.scene(rig.dims[1], 150), 0, 2000);
    Track *safe = rig.track(rig.scene(rig.dims[2], 100), 0, 2000);
    solo->setSolo(true);
    safe->setSoloSafe(true);
    QVERIFY(!other->isSolo() && !other->isSoloSafe());

    rig.start();
    rig.playTo(200);
    QCOMPARE(rig.at(0), 200);                   // soloed: plays
    QCOMPARE(rig.at(1), 0);                     // not soloed: silent
    QCOMPARE(rig.at(2), 100);                   // solo-safe: plays anyway

    // Solo off mid-run: the other track comes in where the show is.
    solo->setSolo(false);
    rig.tick(3);
    QCOMPARE(rig.at(0), 200);
    QCOMPARE(rig.at(1), 150);
    QCOMPARE(rig.at(2), 100);
}

void ShowRunner_Test::holdLastKeepsTheLookThroughAGap()
{
    Rig rig(this);
    rig.doc->setMode(Doc::Operate);             // the hold is a show-time thing
    QVERIFY(rig.doc->lastLookEnabled());
    Track *held = rig.track(rig.scene(rig.dims[0], 200), 0, 400);
    rig.item(held, rig.scene(rig.dims[0], 60), 1200, 400);
    held->setHoldLast(true);
    Track *plain = rig.track(rig.scene(rig.dims[1], 180), 0, 400);
    rig.track(rig.scene(rig.dims[2], 90), 0, 2000);      // keeps the show running

    rig.start();
    rig.playTo(200);
    QCOMPARE(rig.at(0), 200);
    QCOMPARE(rig.at(1), 180);

    // In the gap (400..1200): the hold-last track's look stays up, the
    // other track's goes dark.
    rig.playTo(400);                            // to 800
    QVERIFY(rig.doc->lastLook()->isActive());   // its look was captured
    QCOMPARE(rig.at(1), 0);
    QEXPECT_FAIL("", "BUG: the hold is captured but its dimmer never reaches the output -- "
                     "LastLookEffect::writeDMX() writes before Universe::processFaders(), whose "
                     "zeroIntensityChannels() clears it every frame (the same bug "
                     "LastLookEffect_Test::holdSurvivesTheUniverseFrame marks)", Continue);
    QCOMPARE(rig.at(0), 200);

    // Its next item takes the fixture back.
    rig.playTo(600);                            // to 1400
    QCOMPARE(rig.at(0), 60);
    rig.doc->setMode(Doc::Design);
}

void ShowRunner_Test::anOverrideTrackWinsAndABackgroundTrackYields()
{
    // Control: two Normal tracks -- the later item wins (LTP), which is what
    // the Override below has to beat.
    {
        Rig rig(this);
        rig.track(rig.scene(rig.pan, 100), 0, 2000);
        rig.track(rig.scene(rig.pan, 200), 200, 1800);
        rig.start();
        rig.playTo(100);
        QCOMPARE(rig.at(10), 100);
        rig.playTo(300);
        QCOMPARE(rig.at(10), 200);
    }
    // Override, started first, still wins over a Normal item that starts later.
    {
        Rig rig(this);
        Track *over = rig.track(rig.scene(rig.pan, 100), 0, 2000);
        over->setPriority(Track::Override);
        rig.track(rig.scene(rig.pan, 200), 200, 1800);
        rig.start();
        rig.playTo(400);
        QCOMPARE(rig.at(10), 100);
    }
    // Background, started later, yields to the Normal item already up.
    {
        Rig rig(this);
        rig.track(rig.scene(rig.pan, 200), 0, 2000);
        Track *back = rig.track(rig.scene(rig.pan, 50), 200, 1800);
        back->setPriority(Track::Background);
        rig.start();
        rig.playTo(400);
        QCOMPARE(rig.at(10), 200);
    }
    // "Override (this track wins)" -- on an intensity channel too, even at a
    // lower level than the Normal track's.
    {
        Rig rig(this);
        Track *over = rig.track(rig.scene(rig.dims[0], 100), 0, 2000);
        over->setPriority(Track::Override);
        rig.track(rig.scene(rig.dims[0], 200), 0, 2000);
        rig.start();
        rig.playTo(200);
        QEXPECT_FAIL("", "BUG: an Override track does not win an HTP (intensity) channel -- "
                         "the fader priority only orders writers, and intensity is still "
                         "blended highest-takes-precedence, so a Normal track at a higher "
                         "level beats it (docs/features/shows.md says Override: this track wins)",
                     Continue);
        QCOMPARE(rig.at(0), 100);
    }
}

void ShowRunner_Test::aTrackAtHalfIntensityPlaysAtHalf()
{
    Rig rig(this);
    Track *half = rig.track(rig.scene(rig.dims[0], 200), 0, 2000);
    rig.track(rig.scene(rig.dims[1], 200), 0, 2000);
    rig.show->setTrackIntensity(half->id(), 0.5);   // the track header's fader
    QCOMPARE(half->intensity(), 0.5);

    rig.start();
    rig.playTo(200);
    QCOMPARE(rig.at(1), 200);                   // the other track: untouched
    QEXPECT_FAIL("", "BUG: a track's intensity submaster set before the show starts is lost -- "
                     "Show::preRun() queues adjustIntensity(getAttributeValue(i)) for every "
                     "track, and those attributes stay 1.0 (Show::setTrackIntensity() never "
                     "updates them), so ShowRunner starts the track at full, not Track::intensity()",
                 Continue);
    QVERIFY2(qAbs(rig.at(0) - 100) <= 2, qPrintable(QString("half of 200 is %1").arg(rig.at(0))));

    // Moved while it plays: the submaster scales it at once.
    rig.show->setTrackIntensity(half->id(), 0.25);
    rig.tick(3);
    QVERIFY2(qAbs(rig.at(0) - 50) <= 2, qPrintable(QString("a quarter of 200 is %1").arg(rig.at(0))));
    rig.show->setTrackIntensity(half->id(), 0.5);
    rig.tick(3);
    QVERIFY2(qAbs(rig.at(0) - 100) <= 2, qPrintable(QString("half of 200 is %1").arg(rig.at(0))));
    QCOMPARE(rig.at(1), 200);
}

void ShowRunner_Test::theShowStopsAtItsConfiguredEnd()
{
    Rig rig(this);
    rig.doc->setMode(Doc::Operate);             // parking is the last look
    Scene *s = rig.scene(rig.dims[0], 200);
    rig.track(s, 0, 3000);                      // authored past the end
    Scene *late = rig.scene(rig.dims[1], 150);
    rig.track(late, 800, 1000);                 // starts after the end: never
    rig.show->setConfiguredDuration(600);
    QSignalSpy finished(rig.show, SIGNAL(showFinished()));

    rig.start();
    rig.playTo(500);
    QVERIFY(rig.show->isRunning());
    QCOMPARE(rig.at(0), 200);
    QCOMPARE(finished.count(), 0);

    rig.playTo(300);                            // to 800, past the end
    QCOMPARE(finished.count(), 1);
    QVERIFY(!rig.show->isRunning());
    QVERIFY(!s->isRunning());
    QVERIFY(!late->isRunning());                // clamped: never started
    QCOMPARE(rig.at(1), 0);
    QVERIFY(rig.doc->lastLook()->isActive());   // parked: the look is held...
    QEXPECT_FAIL("", "BUG: ...but the held dimmer is zeroed every frame -- LastLookEffect::"
                     "writeDMX() writes before Universe::processFaders()'s "
                     "zeroIntensityChannels() (see LastLookEffect_Test::"
                     "holdSurvivesTheUniverseFrame), so a parked show goes dark", Continue);
    QCOMPARE(rig.at(0), 200);                   // ...and stays up
    rig.doc->setMode(Doc::Design);
}

QTEST_APPLESS_MAIN(ShowRunner_Test)

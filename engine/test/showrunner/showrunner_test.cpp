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
#include "parkeffect.h"
#include "highlighteffect.h"
#include "programmerflasher.h"
#include "ioplugincache.h"
#include "qlcfile.h"
#include "../iopluginstub/iopluginstub.h"
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

    /** The test output plugin on universe 1: what goes out on the wire. */
    IOPluginStub *patchStub()
    {
        QDir dir(QStringLiteral("../iopluginstub"));
        dir.setFilter(QDir::Files);
        dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
        doc->ioPluginCache()->load(dir);
        if (doc->ioPluginCache()->plugins().isEmpty())
            return nullptr;
        IOPluginStub *stub = static_cast<IOPluginStub *>(doc->ioPluginCache()->plugins().at(0));
        if (!doc->inputOutputMap()->setOutputPatch(0, stub->name(), QString(), 0))
            return nullptr;
        return stub;
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
    rig.track(rig.scene(rig.dims[1], 180), 0, 400);     // a plain track
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
        QCOMPARE(rig.at(0), 100);
    }
}

void ShowRunner_Test::anOverrideTrackPullsADimmerBelowANormalTrack()
{
    Rig rig(this);
    // The Normal track at full, added (and so started) after the Override one.
    Track *over = rig.track(rig.scene(rig.dims[0], 60), 0, 2000);
    over->setPriority(Track::Override);
    rig.track(rig.scene(rig.dims[0], 255), 0, 2000);
    rig.track(rig.scene(rig.dims[1], 255), 0, 2000);    // a channel it doesn't touch
    rig.start();
    rig.playTo(200);
    QCOMPARE(rig.at(0), 60);
    QCOMPARE(rig.at(1), 255);

    // Its intensity submaster scales it below that, still winning.
    rig.show->setTrackIntensity(over->id(), 0.5);
    rig.tick(3);
    QVERIFY2(qAbs(rig.at(0) - 30) <= 1, qPrintable(QString("half of 60 is %1").arg(rig.at(0))));

    // The Grand Master still applies to it.
    rig.show->setTrackIntensity(over->id(), 1.0);
    rig.doc->inputOutputMap()->setGrandMasterValue(127);
    rig.tick(3);
    QCOMPARE(rig.at(0), 60);
    {
        QList<Universe *> ua = rig.doc->inputOutputMap()->claimUniverses();
        const int post = uchar(ua.first()->postGMValues()->at(0));
        rig.doc->inputOutputMap()->releaseUniverses(false);
        QVERIFY2(qAbs(post - 30) <= 1, qPrintable(QString("60 at half GM is %1").arg(post)));
    }
    rig.doc->inputOutputMap()->setGrandMasterValue(255);
}

void ShowRunner_Test::aParkBeatsAnOverrideTrack()
{
    Rig rig(this);
    Track *over = rig.track(rig.scene(rig.dims[0], 60), 0, 2000);
    over->setPriority(Track::Override);
    rig.track(rig.scene(rig.dims[0], 255), 0, 2000);
    rig.start();
    rig.playTo(200);
    QCOMPARE(rig.at(0), 60);

    {
        ParkEffect park(rig.doc);
        park.parkFixture(rig.dims[0], { { 0, 150 } });
        rig.tick(2);
        QCOMPARE(rig.at(0), 150);           // parked: over the Override track
        park.unparkFixture(rig.dims[0]);
        rig.tick(2);
        QCOMPARE(rig.at(0), 60);            // unparked: the Override track again
    }
}

void ShowRunner_Test::theNormalTrackReturnsWhenTheOverrideItemEnds()
{
    Rig rig(this);
    Track *over = rig.track(rig.scene(rig.dims[0], 60), 0, 600);
    over->setPriority(Track::Override);
    rig.track(rig.scene(rig.dims[0], 200), 0, 2000);
    Track *back = rig.track(rig.scene(rig.dims[1], 220), 0, 2000);
    back->setPriority(Track::Background);
    Track *overBack = rig.track(rig.scene(rig.dims[1], 40), 0, 600);
    overBack->setPriority(Track::Override);
    rig.start();
    rig.playTo(300);
    QCOMPARE(rig.at(0), 60);                // the Override item, over Normal
    QCOMPARE(rig.at(1), 40);                // ...and over Background

    rig.playTo(600);                        // past the Override items' end
    QCOMPARE(rig.at(0), 200);               // the Normal track's level is back
    QCOMPARE(rig.at(1), 220);               // and the Background's

    // The show stopped with an Override track running: nothing is left over.
    Rig rig2(this);
    Track *o2 = rig2.track(rig2.scene(rig2.dims[0], 60), 0, 2000);
    o2->setPriority(Track::Override);
    Scene *normal = rig2.scene(rig2.dims[0], 200);
    rig2.start();
    rig2.playTo(200);
    QCOMPARE(rig2.at(0), 60);
    rig2.show->stop(FunctionParent::master());
    normal->start(rig2.doc->masterTimer(), FunctionParent::master());
    rig2.tick(5);
    QCOMPARE(rig2.at(0), 200);              // a plain cue: no Override stuck to it
    QCOMPARE(normal->fadePriority(), int(Universe::Auto));
}

/* An Override track is the show's own top layer: Blackout still takes it
   out (it is not exempt, and not past the Grand Master either). */
void ShowRunner_Test::blackoutTakesAnOverrideTrackOut()
{
    Rig rig(this);
    IOPluginStub *stub = rig.patchStub();
    QVERIFY(stub != nullptr);
    Track *over = rig.track(rig.scene(rig.dims[0], 60), 0, 2000);
    over->setPriority(Track::Override);
    rig.track(rig.scene(rig.dims[0], 255), 0, 2000);
    rig.start();
    rig.playTo(200);
    QCOMPARE(rig.at(0), 60);
    QCOMPARE(int(uchar(stub->m_universe.at(0))), 60);

    rig.doc->inputOutputMap()->setBlackout(true);
    rig.tick(2);
    QCOMPARE(rig.at(0), 60);                // still played...
    QCOMPARE(int(uchar(stub->m_universe.at(0))), 0);     // ...but not sent
    rig.doc->inputOutputMap()->setBlackout(false);
    rig.tick(2);
    QCOMPARE(int(uchar(stub->m_universe.at(0))), 60);
}

/* "Override (this track wins)": over a cue at full that runs outside the
   show -- started before the show or while it plays -- on an intensity
   channel, at a lower level. The show stopped, the cue has it again. */
void ShowRunner_Test::anOverrideTrackBeatsACueOutsideTheShow()
{
    for (bool cueFirst : { true, false })
    {
        Rig rig(this);
        Scene *cue = rig.scene(rig.dims[0], 255);
        Track *over = rig.track(rig.scene(rig.dims[0], 60), 0, 2000);
        over->setPriority(Track::Override);
        if (cueFirst)
        {
            cue->start(rig.doc->masterTimer(), FunctionParent::master());
            rig.tick(3);
            QCOMPARE(rig.at(0), 255);
        }
        rig.start();
        rig.playTo(200);
        if (!cueFirst)
        {
            cue->start(rig.doc->masterTimer(), FunctionParent::master());
            rig.tick(3);
        }
        QVERIFY(cue->isRunning());
        QCOMPARE(rig.at(0), 60);

        rig.show->stop(FunctionParent::master());
        rig.tick(4);
        QCOMPARE(rig.at(0), 255);
    }
}

/* Highlight and a Flash are over everything that runs, the show's Override
   tracks too; let go, the track has the light again. */
void ShowRunner_Test::highlightAndFlashBeatAnOverrideTrack()
{
    Rig rig(this);
    Track *over = rig.track(rig.scene(rig.dims[0], 60), 0, 4000);
    over->setPriority(Track::Override);
    rig.track(rig.scene(rig.dims[0], 255), 0, 4000);
    rig.start();
    rig.playTo(200);
    QCOMPARE(rig.at(0), 60);

    {
        HighlightEffect highlight(rig.doc);
        highlight.setFixtures({ rig.dims[0] });
        rig.tick(2);
        QCOMPARE(rig.at(0), 255);
        highlight.setFixtures({});
        rig.tick(2);
        QCOMPARE(rig.at(0), 60);
    }
    {
        // Long enough never to end by the clock here: let go by going away.
        ProgrammerFlasher flash(rig.doc);
        flash.flashFixture(rig.dims[0], 600000);
        rig.tick(2);
        QCOMPARE(rig.at(0), 255);           // the track's 60 is dim: flashed up
    }
    rig.tick(2);
    QCOMPARE(rig.at(0), 60);
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

void ShowRunner_Test::theTrackFaderAndItsAttributeAreOneValue()
{
    Rig rig(this);
    Track *a = rig.track(rig.scene(rig.dims[0], 200), 0, 2000);
    Track *b = rig.track(rig.scene(rig.dims[1], 200), 0, 2000);
    const int ia = rig.show->tracks().indexOf(a);
    const int ib = rig.show->tracks().indexOf(b);

    // The fader moves the attribute...
    rig.show->setTrackIntensity(a->id(), 0.5);
    QCOMPARE(rig.show->getAttributeValue(ia), 0.5);
    // ...and the attribute moves the fader; the last one set wins.
    rig.show->adjustAttribute(0.25, ia);
    QCOMPARE(a->intensity(), 0.25);
    rig.show->setTrackIntensity(b->id(), 0.75);
    rig.show->adjustAttribute(0.5, ib);
    QCOMPARE(b->intensity(), 0.5);

    rig.start();
    rig.playTo(200);
    QVERIFY2(qAbs(rig.at(0) - 50) <= 2, qPrintable(QString("a quarter of 200 is %1").arg(rig.at(0))));
    QVERIFY2(qAbs(rig.at(1) - 100) <= 2, qPrintable(QString("half of 200 is %1").arg(rig.at(1))));
}

void ShowRunner_Test::aTrackIntensityIsSavedAndPlaysAfterLoad()
{
    Rig rig(this);
    Track *half = rig.track(rig.scene(rig.dims[0], 200), 0, 2000);
    rig.show->setTrackIntensity(half->id(), 0.5);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    QVERIFY(rig.show->saveXML(&xmlWriter));
    xmlWriter.setDevice(NULL);
    buffer.close();

    // Loaded into a fresh show on the same rig (the scene is still there).
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();
    Show *loaded = new Show(rig.doc);
    QVERIFY(loaded->loadXML(xmlReader));
    rig.doc->deleteFunction(rig.show->id());
    rig.show = loaded;
    rig.doc->addFunction(loaded);

    QCOMPARE(loaded->tracks().count(), 1);
    Track *t = loaded->tracks().first();
    QCOMPARE(t->intensity(), 0.5);
    QCOMPARE(loaded->getAttributeValue(0), 0.5);

    rig.start();
    rig.playTo(200);
    QVERIFY2(qAbs(rig.at(0) - 100) <= 2, qPrintable(QString("half of 200 is %1").arg(rig.at(0))));
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
    QCOMPARE(rig.at(0), 200);                   // ...and stays up
    rig.doc->setMode(Doc::Design);
}

QTEST_APPLESS_MAIN(ShowRunner_Test)

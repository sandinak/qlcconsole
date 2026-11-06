/*
  Q Light Controller Plus - Unit test
  freerotation_test.cpp

  Free (endless) pan/tilt rotation read from real definitions in
  resources/fixtures, and the Free Spin effect writing each fixture's own
  values: "a movement effect that takes advantage of fixtures that have a
  free rotate option, so we can manage that via an effect vs a DMX rule".

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtTest>

#include "freerotation_test.h"
#include "../common/resource_paths.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "qlcfile.h"
#include "effectinstance.h"
#include "effectscriptrunner.h"
#include "freerotation.h"
#include "qlcpalette.h"
#include "scenevalue.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

namespace
{
const QString kSpin = QStringLiteral(QLC_TEST_RESOURCE_ROOT "effectscripts/free-spin.js");

/* What one tick of @p inst writes: channel -> value, for @p fid. */
QHash<quint32, int> written(const EffectInstance &inst, quint32 fid)
{
    QHash<quint32, int> out;
    for (const EffectInstance::DmxWrite &w : inst.dmxWrites())
        if (w.fixtureId == fid)
            out.insert(w.channel, w.value);
    return out;
}
}

void FreeRotation_Test::initTestCase()
{
    m_doc = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir));
}

void FreeRotation_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = nullptr;
}

void FreeRotation_Test::init()
{
}

void FreeRotation_Test::cleanup()
{
    m_doc->clearContents();
}

QLCFixtureDef *FreeRotation_Test::def(const QString &file)
{
    // manufacturer/model from the file, through the cache that owns it
    const QStringList parts = file.split(QLatin1Char('|'));
    return m_doc->fixtureDefCache()->fixtureDef(parts.at(0), parts.at(1));
}

quint32 FreeRotation_Test::addFixture(const QString &file, const QString &modeName, quint32 address)
{
    QLCFixtureDef *d = def(file);
    if (d == nullptr)
        return Fixture::invalidId();
    QLCFixtureMode *mode = d->mode(modeName);
    if (mode == nullptr)
        return Fixture::invalidId();
    Fixture *fx = new Fixture(m_doc);
    fx->setFixtureDefinition(d, mode);
    fx->setUniverse(0);
    fx->setAddress(address);
    fx->setName(d->model());
    if (!m_doc->addFixture(fx))
        return Fixture::invalidId();
    return fx->id();
}

/* The Junman two-arm beam: each arm's Pan is 0-127 point, 128-191 clockwise
   and 192-255 counter-clockwise, both slow to fast. No stop value: it stops
   by pointing. */
void FreeRotation_Test::aBandOnThePanChannelSpinsAndStopsByPointing()
{
    const quint32 id = addFixture("Junman|2 Head", "16 Channel", 0);
    QVERIFY(id != Fixture::invalidId());
    Fixture *fx = m_doc->fixture(id);
    QVERIFY(FreeRotation::canSpin(fx));

    for (int head = 0; head < 2; ++head)
    {
        const FreeRotation::Spin pan = FreeRotation::find(fx, head, FreeRotation::Pan);
        QVERIFY(pan.canSpin());
        QCOMPARE(pan.channel, quint32(head == 0 ? 0 : 8));
        QVERIFY(pan.band);
        QVERIFY(pan.reverses());
        QCOMPARE(pan.stop, -1);
        QCOMPARE(pan.value(0.001), 128);      // clockwise, slowest
        QCOMPARE(pan.value(1.0), 191);        // clockwise, fastest
        QCOMPARE(pan.value(0.5), 160);
        QCOMPARE(pan.value(-0.001), 192);     // counter-clockwise, slowest
        QCOMPARE(pan.value(-1.0), 255);
        QCOMPARE(pan.value(0.0), -1);         // no stop value: point it instead
        QVERIFY(!FreeRotation::find(fx, head, FreeRotation::Tilt).canSpin());
    }
    // Pointing it still only uses 0-127 of the arm's pan.
    const QList<SceneValue> pos = fx->positionToValues(QLCChannel::Pan, 360, false, 0);
    QCOMPARE(pos.first().channel, quint32(0));
    QCOMPARE(int(pos.first().value), 127);
}

/* The Cameo Movo Beam 100 (15 ch): Endless Pan rotation (ch 3) and Endless
   Tilt rotation (ch 6) -- clockwise 6-126 fast to slow, 127-128 stop,
   counter-clockwise 129-255 slow to fast. */
void FreeRotation_Test::aChannelOfItsOwnFastToSlowOneWaySlowToFastTheOther()
{
    const quint32 id = addFixture("Cameo|Movo Beam 100", "15 Channel", 0);
    QVERIFY(id != Fixture::invalidId());
    Fixture *fx = m_doc->fixture(id);

    const FreeRotation::Spin pan = FreeRotation::find(fx, 0, FreeRotation::Pan);
    QVERIFY(pan.canSpin());
    QCOMPARE(pan.channel, quint32(2));
    QVERIFY(!pan.band);
    QCOMPARE(pan.stop, 127);
    QCOMPARE(pan.value(0.0), 127);
    QCOMPARE(pan.value(0.001), 126);          // clockwise slowest: the top of a fast-to-slow range
    QCOMPARE(pan.value(1.0), 6);              // ...fastest: its bottom
    QCOMPARE(pan.value(0.5), 66);
    QCOMPARE(pan.value(-0.001), 129);         // counter-clockwise slowest
    QCOMPARE(pan.value(-1.0), 255);
    QCOMPARE(pan.value(-0.5), 192);
    // Read back the way it was written.
    QVERIFY(qAbs(pan.speedOf(66) - 0.5) < 0.01);
    QVERIFY(qAbs(pan.speedOf(192) + 0.5) < 0.01);
    QCOMPARE(pan.speedOf(127), 0.0);

    const FreeRotation::Spin tilt = FreeRotation::find(fx, 0, FreeRotation::Tilt);
    QVERIFY(tilt.canSpin());
    QCOMPARE(tilt.channel, quint32(5));
    QCOMPARE(tilt.value(0.5), 66);
    QCOMPARE(tilt.value(0.0), 127);

    // The 4-channel mode has no rotation channels: it can't spin.
    const quint32 small = addFixture("Cameo|Movo Beam 100", "4 Channel", 100);
    QVERIFY(!FreeRotation::canSpin(m_doc->fixture(small)));
}

/* The Ayrton Kyalami says it only in words: "Forwards Pan rotation from fast
   to slow" (preset FastToSlow, no way round), "Backwards ... from slow to
   fast", "stop rotation by spinout" 0-63. */
void FreeRotation_Test::rangesSaidOnlyInWordsAreReadFromTheWords()
{
    const quint32 id = addFixture("Ayrton|Kyalami", "Extended", 0);
    QVERIFY(id != Fixture::invalidId());
    Fixture *fx = m_doc->fixture(id);
    const FreeRotation::Spin pan = FreeRotation::find(fx, 0, FreeRotation::Pan);
    QVERIFY(pan.canSpin());
    QCOMPARE(pan.channel, quint32(5));
    QVERIFY(pan.reverses());
    QCOMPARE(pan.stop, 31);
    QCOMPARE(pan.cw.lo, uchar(128));
    QCOMPARE(pan.cw.hi, uchar(189));
    QVERIFY(!pan.cw.slowToFast);
    QCOMPARE(pan.ccw.lo, uchar(194));
    QVERIFY(pan.ccw.slowToFast);
    QCOMPARE(pan.value(1.0), 128);
    QCOMPARE(pan.value(-1.0), 255);
    QCOMPARE(pan.value(0.0), 31);
    const FreeRotation::Spin tilt = FreeRotation::find(fx, 0, FreeRotation::Tilt);
    QCOMPARE(tilt.channel, quint32(6));
}

/* The Robe Robin Spikie tags both its "Forwards" and its "Backwards" range
   RotationClockwise: the words tell them apart. */
void FreeRotation_Test::presetsThatGiveOneWayAreReadFromTheWords()
{
    const quint32 id = addFixture("Robe|Robin Spikie", "Standard 16-bit (RGB)", 0);
    QVERIFY(id != Fixture::invalidId());
    const FreeRotation::Spin pan = FreeRotation::find(m_doc->fixture(id), 0, FreeRotation::Pan);
    QVERIFY(pan.canSpin());
    QCOMPARE(pan.channel, quint32(4));
    QVERIFY(pan.reverses());
    QCOMPARE(int(pan.cw.lo), 128);
    QCOMPARE(int(pan.ccw.lo), 194);
    QCOMPARE(pan.stop, 191);
    QCOMPARE(pan.value(0.001), 189);          // forwards, fast to slow: slowest at its top
    QCOMPARE(pan.value(-0.001), 194);
}

/* The Cameo's 35-channel mode has five heads (its LED rings); the rotation
   channels belong to none of them. Every head spins by them. */
void FreeRotation_Test::aHeadWithItsSpinOutsideItsHeadsUsesTheLooseChannel()
{
    const quint32 id = addFixture("Cameo|Movo Beam 100", "35 Channel", 0);
    QVERIFY(id != Fixture::invalidId());
    Fixture *fx = m_doc->fixture(id);
    QVERIFY(fx->heads() > 1);
    for (int h = 0; h < fx->heads(); ++h)
        QCOMPARE(FreeRotation::find(fx, h, FreeRotation::Pan).channel, quint32(2));
}

/* A definition that lists "Endless Pan rotation" after Pan used to make it
   the head's pan: pointing the head wrote the angle into the spin channel
   and set it turning. The pointing channel is Pan. */
void FreeRotation_Test::aSpinChannelIsNeverTheHeadsPointingChannel()
{
    struct Case { const char *def, *mode; quint32 pan, tilt; };
    const Case cases[] = {
        { "Cameo|Movo Beam 100", "15 Channel", 0, 3 },
        { "Ayrton|Kyalami", "Extended", 0, 2 },
        { "Robe|Robin Spikie", "Standard 16-bit (RGB)", 0, 2 },
        { "GLP|Impression FR1 TM", "8 Channel", 0, 2 },
    };
    quint32 address = 0;
    for (const Case &c : cases)
    {
        const quint32 id = addFixture(c.def, c.mode, address);
        address += 50;
        QVERIFY2(id != Fixture::invalidId(), c.def);
        Fixture *fx = m_doc->fixture(id);
        QCOMPARE(fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB, 0), c.pan);
        QCOMPARE(fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB, 0), c.tilt);
        const QList<SceneValue> pos = fx->positionToValues(QLCChannel::Pan, 90);
        QVERIFY(!pos.isEmpty());
        QVERIFY2(pos.first().channel == c.pan, c.def);
        QVERIFY2(FreeRotation::spinsOnly(fx->channel(FreeRotation::find(fx, 0, FreeRotation::Pan).channel)), c.def);
    }
}

/* The Clay Paky Sharpy rotates its gobos and prism, not its head. */
void FreeRotation_Test::gobosAndPrismsThatRotateAreNotTheHeadTurning()
{
    QLCFixtureDef *d = def("Clay Paky|Sharpy");
    QVERIFY(d != nullptr);
    const quint32 id = addFixture("Clay Paky|Sharpy", d->modes().first()->name(), 0);
    QVERIFY(id != Fixture::invalidId());
    QVERIFY(!FreeRotation::canSpin(m_doc->fixture(id)));
}

void FreeRotation_Test::rpmInTheDefinitionIsRead()
{
    QLCChannel ch;
    ch.setGroup(QLCChannel::Pan);
    ch.addCapability(new QLCCapability(0, 127, "Pan position"));
    QLCCapability *cw = new QLCCapability(128, 190, "Continuous pan rotation CW (fast 60 rpm to slow 2 rpm)");
    ch.addCapability(cw);
    ch.addCapability(new QLCCapability(191, 192, "Stop"));
    ch.addCapability(new QLCCapability(193, 255, "Continuous pan rotation CCW (slow 30rph to fast 60rpm)"));
    const FreeRotation::Spin s = FreeRotation::read(&ch);
    QVERIFY(s.canSpin());
    QVERIFY(s.points);                       // "Pan position": it points too
    QVERIFY(!FreeRotation::spinsOnly(&ch));
    QVERIFY(s.hasRpm());
    QCOMPARE(s.cw.rpmSlow, 2.0);
    QCOMPARE(s.cw.rpmFast, 60.0);
    QVERIFY(!s.cw.slowToFast);
    QCOMPARE(s.ccw.rpmSlow, 0.5);
    QVERIFY(s.ccw.slowToFast);
    QVERIFY(qAbs(s.speedForRpm(31.0) - 0.5) < 1e-6);
    QCOMPARE(s.speedForRpm(500.0), 1.0);
    QCOMPARE(s.stop, 191);
}

/* Free Spin on a mixed set: each fixture gets its own value for the same
   "clockwise at half speed". */
void FreeRotation_Test::freeSpinWritesEachFixturesOwnValues()
{
    const quint32 cameo = addFixture("Cameo|Movo Beam 100", "15 Channel", 0);
    const quint32 junman = addFixture("Junman|2 Head", "16 Channel", 20);
    const quint32 kyalami = addFixture("Ayrton|Kyalami", "Extended", 40);

    QLCPalette *spin = new QLCPalette(QLCPalette::Effect, m_doc);
    spin->setScriptPath(kSpin);
    spin->setEffectParamValue("speed", 50);
    spin->setEffectParamValue("fadeIn", 0);
    spin->setEffectParamValue("axis", 2);     // pan and tilt
    QVERIFY(m_doc->addPalette(spin));
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    for (quint32 id : {cameo, junman, kyalami})
        scene->addFixture(id);
    scene->addPalette(spin->id());

    EffectInstance inst(m_doc, scene->id(), spin->id());
    QVERIFY2(inst.isValid(), qPrintable(inst.error()));
    inst.setOffStageClock(1000);
    inst.runTick();

    const QHash<quint32, int> c = written(inst, cameo);
    QCOMPARE(c.value(2, -1), 66);             // pan: clockwise 6-126, fast to slow
    QCOMPARE(c.value(5, -1), 66);             // tilt, the same
    QVERIFY(!c.contains(0));                  // its pointing pan: the look's
    const QHash<quint32, int> j = written(inst, junman);
    QCOMPARE(j.value(0, -1), 160);            // arm 1: 128-191, slow to fast
    QCOMPARE(j.value(8, -1), 160);            // arm 2
    QVERIFY(!j.contains(2) && !j.contains(10));   // it can't spin its tilt
    const QHash<quint32, int> k = written(inst, kyalami);
    QCOMPARE(k.value(5, -1), 159);            // forwards 128-189, fast to slow
    QCOMPARE(k.value(6, -1), 159);

    // Counter-clockwise.
    spin->setEffectParamValue("direction", 1);
    inst.reloadParamsFromPalette();
    inst.runTick();
    QCOMPARE(written(inst, cameo).value(2, -1), 192);
    QCOMPARE(written(inst, junman).value(0, -1), 224);
    QCOMPARE(written(inst, kyalami).value(5, -1), 225);

    // Full speed.
    spin->setEffectParamValue("speed", 100);
    spin->setEffectParamValue("direction", 0);
    inst.reloadParamsFromPalette();
    inst.runTick();
    QCOMPARE(written(inst, cameo).value(2, -1), 6);
    QCOMPARE(written(inst, junman).value(0, -1), 191);
    QCOMPARE(written(inst, kyalami).value(5, -1), 128);

    // Its fade in: at the start it is at the slowest it goes, not stopped,
    // a second into a two-second fade half way.
    spin->setEffectParamValue("speed", 100);
    spin->setEffectParamValue("fadeIn", 2);
    inst.reloadParamsFromPalette();
    inst.setOffStageClock(1000);
    inst.runTick();
    QCOMPARE(written(inst, cameo).value(2, -1), 66);
    QCOMPARE(written(inst, junman).value(0, -1), 160);
}

void FreeRotation_Test::freeSpinSkipsHeadsThatCannotSpin()
{
    const quint32 cameo = addFixture("Cameo|Movo Beam 100", "15 Channel", 0);
    QLCFixtureDef *sharpyDef = def("Clay Paky|Sharpy");
    QVERIFY(sharpyDef != nullptr);
    const quint32 sharpy = addFixture("Clay Paky|Sharpy", sharpyDef->modes().first()->name(), 100);

    QLCPalette *spin = new QLCPalette(QLCPalette::Effect, m_doc);
    spin->setScriptPath(kSpin);
    QVERIFY(m_doc->addPalette(spin));
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(cameo);
    scene->addFixture(sharpy);
    scene->addPalette(spin->id());

    EffectInstance inst(m_doc, scene->id(), spin->id());
    QCOMPARE(inst.effectiveFixtureIds(), QList<quint32>() << cameo);
    inst.setOffStageClock(5000);
    inst.runTick();
    QVERIFY(written(inst, sharpy).isEmpty());
    QVERIFY(written(inst, cameo).contains(2));
}

/* Stopped (before its stagger, or faded out): its stop value, and -- Return
   -- the look's position. A band can only stop by pointing. */
void FreeRotation_Test::freeSpinStopsAndReturnsToTheLook()
{
    const quint32 cameo = addFixture("Cameo|Movo Beam 100", "15 Channel", 0);
    const quint32 junman = addFixture("Junman|2 Head", "16 Channel", 20);

    QLCPalette *spin = new QLCPalette(QLCPalette::Effect, m_doc);
    spin->setScriptPath(kSpin);
    spin->setEffectParamValue("fadeIn", 0);
    spin->setEffectParamValue("stagger", 10);  // the second fixture starts 10 s after the first
    QVERIFY(m_doc->addPalette(spin));
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(cameo);
    scene->addFixture(junman);
    // The look points the Junman's first arm at DMX 64 of its 0-127 band.
    scene->setValue(junman, 0, 64);
    scene->setValue(cameo, 0, 200);
    scene->addPalette(spin->id());

    EffectInstance inst(m_doc, scene->id(), spin->id());
    inst.setOffStageClock(1000);
    inst.runTick();
    QVERIFY(written(inst, cameo).value(2, -1) > 0);    // spinning
    QHash<quint32, int> j = written(inst, junman);
    QVERIFY2(qAbs(j.value(0, -1) - 64) <= 1,
             qPrintable(QString("the Junman waits where the look points it: %1").arg(j.value(0, -1))));

    // Hold: the Cameo stops (127) and its pointing pan stays the look's.
    spin->setEffectParamValue("stagger", 0);
    spin->setEffectParamValue("stop", 0);
    inst.reloadParamsFromPalette();
    inst.setOffStageClock(0);
    inst.runTick();
    QHash<quint32, int> c = written(inst, cameo);
    QCOMPARE(c.value(2, -1), 127);
    QVERIFY(!c.contains(0));
    // ...but a band stops by pointing, Hold or not.
    j = written(inst, junman);
    QVERIFY(qAbs(j.value(0, -1) - 64) <= 1);

    // Return: stopped and pointed where the look points it.
    spin->setEffectParamValue("stop", 1);
    inst.reloadParamsFromPalette();
    inst.runTick();
    c = written(inst, cameo);
    QCOMPARE(c.value(2, -1), 127);
    QVERIFY2(qAbs(c.value(0, -1) - 200) <= 1, qPrintable(QString::number(c.value(0, -1))));
}

void FreeRotation_Test::freeSpinBouncesAndAlternates()
{
    const quint32 a = addFixture("Cameo|Movo Beam 100", "15 Channel", 0);
    const quint32 b = addFixture("Cameo|Movo Beam 100", "15 Channel", 20);

    QLCPalette *spin = new QLCPalette(QLCPalette::Effect, m_doc);
    spin->setScriptPath(kSpin);
    spin->setEffectParamValue("fadeIn", 0);
    spin->setEffectParamValue("speed", 50);
    spin->setEffectParamValue("direction", 2);   // alternate fixtures
    QVERIFY(m_doc->addPalette(spin));
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(a);
    scene->addFixture(b);
    scene->addPalette(spin->id());

    EffectInstance inst(m_doc, scene->id(), spin->id());
    inst.setOffStageClock(3000);
    inst.runTick();
    QCOMPARE(written(inst, a).value(2, -1), 66);     // clockwise
    QCOMPARE(written(inst, b).value(2, -1), 192);    // counter-clockwise

    // Bounce every 8 s: clockwise at 4 s, counter-clockwise at 12 s, and
    // slowest (not stopped) right at the reversal.
    spin->setEffectParamValue("direction", 3);
    spin->setEffectParamValue("bounce", 8);
    inst.reloadParamsFromPalette();
    inst.setOffStageClock(4000);
    inst.runTick();
    QCOMPARE(written(inst, a).value(2, -1), 66);
    inst.setOffStageClock(12000);
    inst.runTick();
    QCOMPARE(written(inst, a).value(2, -1), 192);
    inst.setOffStageClock(16000);
    inst.runTick();
    const int atReversal = written(inst, a).value(2, -1);
    QVERIFY2(atReversal == 126 || atReversal == 129,
             qPrintable(QString("slowest one way or the other at the reversal: %1").arg(atReversal)));
}

/* A spin value part way to what is under it is another speed, or a
   position: a spin fading out is the script's slowing down, and its
   writes are never mixed (DmxWrite::snap). */
void FreeRotation_Test::freeSpinSpinsAreNeverMixedPartWay()
{
    const quint32 junman = addFixture("Junman|2 Head", "16 Channel", 0);
    QLCPalette *spin = new QLCPalette(QLCPalette::Effect, m_doc);
    spin->setScriptPath(kSpin);
    spin->setEffectParamValue("fadeIn", 0);
    QVERIFY(m_doc->addPalette(spin));
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(junman);
    scene->addPalette(spin->id());

    EffectInstance inst(m_doc, scene->id(), spin->id());
    inst.setOffStageClock(1000);
    inst.runTick();
    bool any = false;
    for (const EffectInstance::DmxWrite &w : inst.dmxWrites())
        if (w.channel == 0)
        {
            any = true;
            QVERIFY(w.snap);
        }
    QVERIFY(any);
}
QTEST_GUILESS_MAIN(FreeRotation_Test)

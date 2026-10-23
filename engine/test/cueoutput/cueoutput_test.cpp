/*
  Q Light Controller Plus - Unit test
  cueoutput_test.cpp

  Verifies CueOutput: the offline "what would this cue output" computation the
  move-in-black planner peeks with — a Scene's baked values + palettes split into
  intensity vs non-intensity, and a Collection unioning its members (last wins).

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>

#define protected public                // Universe::processFaders()
#include "universe.h"
#undef protected
#define private public                  // MasterTimer::timerTick()
#include "mastertimer.h"
#undef private
#include "inputoutputmap.h"

#include "cueoutput_test.h"
#include "doc.h"
#include "fixture.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "scene.h"
#include "collection.h"
#include "cueoutput.h"
#include "qlcpalette.h"
#include "qlcfixturehead.h"

// A minimal mover: Pan, Tilt, Dimmer (master intensity), Red, Green, Blue.
// No heads → the Dimmer (Intensity/NoColour/MSB) auto-detects as master.
static QLCFixtureDef *makeMoverDef()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Mover6");
    QLCFixtureMode *mode = new QLCFixtureMode(def);

    auto add = [&](const QString &name, QLCChannel::Group grp, QLCChannel::PrimaryColour col) {
        QLCChannel *ch = new QLCChannel();
        ch->setName(name); ch->setGroup(grp); ch->setColour(col);
        def->addChannel(ch); mode->insertChannel(ch, def->channels().size() - 1);
    };
    add("Pan",   QLCChannel::Pan,       QLCChannel::NoColour);
    add("Tilt",  QLCChannel::Tilt,      QLCChannel::NoColour);
    add("Dimmer",QLCChannel::Intensity, QLCChannel::NoColour);   // → master
    add("Red",   QLCChannel::Intensity, QLCChannel::Red);
    add("Green", QLCChannel::Intensity, QLCChannel::Green);
    add("Blue",  QLCChannel::Intensity, QLCChannel::Blue);

    mode->cacheHeads();
    def->addMode(mode);
    return def;
}

void CueOutput_Test::init()
{
    m_doc = new Doc(this);

    QLCFixtureDef *def = makeMoverDef();
    QLCFixtureMode *mode = def->modes().first();

    Fixture *fx = new Fixture(m_doc);
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(0);
    fx->setAddress(0);
    m_doc->addFixture(fx);
    m_moverId = fx->id();

    QCOMPARE(fx->masterIntensityChannel(), quint32(2));   // Dimmer is master
}

void CueOutput_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void CueOutput_Test::sceneSplitsIntensityFromRest()
{
    Scene *sc = new Scene(m_doc);
    sc->setName("Lit look");
    sc->setValue(m_moverId, 2, 200);   // Dimmer (intensity)
    sc->setValue(m_moverId, 0, 128);   // Pan
    sc->setValue(m_moverId, 1, 64);    // Tilt
    m_doc->addFunction(sc);

    const QHash<quint32, CueOutput::FixtureCue> cues =
        CueOutput::computeCues(m_doc, sc->id());

    QVERIFY(cues.contains(m_moverId));
    const CueOutput::FixtureCue &fc = cues.value(m_moverId);
    QVERIFY(fc.lit);
    QCOMPARE(fc.intensity, uchar(200));
    QCOMPARE(fc.nonIntensity.value(0), uchar(128));  // Pan
    QCOMPARE(fc.nonIntensity.value(1), uchar(64));   // Tilt
    QVERIFY(!fc.nonIntensity.contains(2));           // intensity is NOT here
}

void CueOutput_Test::unlitFixtureReportsDark()
{
    Scene *sc = new Scene(m_doc);
    sc->setName("Aim only");
    sc->setValue(m_moverId, 0, 255);   // Pan only, no intensity
    m_doc->addFunction(sc);

    const QHash<quint32, CueOutput::FixtureCue> cues =
        CueOutput::computeCues(m_doc, sc->id());

    const CueOutput::FixtureCue &fc = cues.value(m_moverId);
    QVERIFY(!fc.lit);
    QCOMPARE(fc.intensity, uchar(0));
    QCOMPARE(fc.nonIntensity.value(0), uchar(255));  // still has the aim
}

void CueOutput_Test::collectionUnionsMembersLastWins()
{
    Scene *a = new Scene(m_doc);
    a->setName("A");
    a->setValue(m_moverId, 2, 200);   // intensity
    a->setValue(m_moverId, 0, 128);   // pan
    a->setValue(m_moverId, 1, 64);    // tilt
    m_doc->addFunction(a);

    Scene *b = new Scene(m_doc);
    b->setName("B");
    b->setValue(m_moverId, 0, 255);   // pan overrides A
    m_doc->addFunction(b);

    Collection *col = new Collection(m_doc);
    col->setName("Cue");
    col->addFunction(a->id());
    col->addFunction(b->id());        // later → wins shared channels
    m_doc->addFunction(col);

    const QHash<quint32, CueOutput::FixtureCue> cues =
        CueOutput::computeCues(m_doc, col->id());

    const CueOutput::FixtureCue &fc = cues.value(m_moverId);
    QVERIFY(fc.lit);
    QCOMPARE(fc.intensity, uchar(200));              // from A, B didn't set it
    QCOMPARE(fc.nonIntensity.value(0), uchar(255));  // B won the pan
    QCOMPARE(fc.nonIntensity.value(1), uchar(64));   // tilt from A
}

void CueOutput_Test::aDimmerlessFixturesColourIsScaledByTheLooksDimmer()
{
    // An RGB batten: one head of Red, Green, Blue and no dimmer at all.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("RGB3");
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    const QLCChannel::Preset presets[] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                           QLCChannel::IntensityBlue };
    const char *names[] = { "Red", "Green", "Blue" };
    QLCFixtureHead head;
    for (int i = 0; i < 3; i++)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[i]); ch->setPreset(presets[i]);
        def->addChannel(ch); mode->insertChannel(ch, i);
        head.addChannel(quint32(i));
    }
    mode->insertHead(-1, head);
    mode->cacheHeads();
    def->addMode(mode);

    Fixture *batten = new Fixture(m_doc);
    batten->setFixtureDefinition(def, mode);
    batten->setUniverse(0);
    batten->setAddress(20);
    m_doc->addFixture(batten);
    QCOMPARE(batten->masterIntensityChannel(), QLCChannel::invalid());

    QLCPalette *blue = new QLCPalette(QLCPalette::Color, m_doc);
    blue->setValue(QColor(0, 0, 255));
    m_doc->addPalette(blue);
    QLCPalette *half = new QLCPalette(QLCPalette::Dimmer, m_doc);
    half->setValue(128);
    m_doc->addPalette(half);

    Scene *sc = new Scene(m_doc);
    sc->setName("Blue at half");
    sc->addFixture(batten->id());
    sc->addFixture(m_moverId);
    sc->addPalette(blue->id());
    sc->addPalette(half->id());
    m_doc->addFunction(sc);

    const QHash<quint32, QHash<quint32, uchar>> out = CueOutput::compute(m_doc, sc->id());

    // The batten: blue at the look's dimmer.
    QVERIFY(out.contains(batten->id()));
    QCOMPARE(out[batten->id()].value(2), uchar(128));
    QCOMPARE(out[batten->id()].value(0), uchar(0));
    QCOMPARE(out[batten->id()].value(1), uchar(0));

    // And the look-ahead is what the scene actually sends when it plays.
    sc->start(m_doc->masterTimer(), FunctionParent::master());
    for (int i = 0; i < 6; ++i)
        m_doc->masterTimer()->timerTick();
    QList<Universe *> unis = m_doc->inputOutputMap()->claimUniverses();
    for (Universe *u : unis)
        u->processFaders();
    const QByteArray dmx = unis.first()->preGMValues();
    m_doc->inputOutputMap()->releaseUniverses(false);
    sc->stop(FunctionParent::master());
    for (quint32 ch = 0; ch < 3; ++ch)
        QCOMPARE(int(out[batten->id()].value(ch)), int(uchar(dmx.at(20 + int(ch)))));
    for (quint32 ch = 0; ch < 6; ++ch)
        QCOMPARE(int(out[m_moverId].value(ch)), int(uchar(dmx.at(int(ch)))));
    m_doc->masterTimer()->timerTick();

    // The mover has a dimmer of its own: the dimmer takes the level, the
    // colour stays whole.
    QCOMPARE(out[m_moverId].value(2), uchar(128));
    QCOMPARE(out[m_moverId].value(5), uchar(255));

    // Without a Dimmer look the batten's colour is whole.
    sc->removePalette(half->id());
    const QHash<quint32, QHash<quint32, uchar>> full = CueOutput::compute(m_doc, sc->id());
    QCOMPARE(full[batten->id()].value(2), uchar(255));
}

QTEST_GUILESS_MAIN(CueOutput_Test)

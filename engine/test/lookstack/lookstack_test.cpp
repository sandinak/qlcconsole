/*
  Q Light Controller Plus - Unit test
  lookstack_test.cpp

  How a look's list stacks, end to end: static looks, effects, and the
  colours and dimmers that belong to an effect, on four kinds of fixture.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <QtTest>
#include <QFile>

#define protected public                // Universe::processFaders()
#include "universe.h"
#undef protected
#define private public                  // MasterTimer::timerTick()
#include "mastertimer.h"
#undef private
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "inputoutputmap.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "lookstackcheck.h"
#include "fixturegroup.h"
#include "scene.h"
#include "showtreecontroller.h"
#include "collection.h"
#include "doc.h"

#include "lookstack_test.h"

namespace
{
/* Effects whose output is fixed, so every look has one right answer. */
const char *kScripts[][2] = {
    // Takes the look's colour (reads palettes.look): the first one, as is.
    { "lookcolour.js",
      "(function() { var e = {}; e.apiVersion = 1; e.name = 'T Look Colour';"
      " e.fixtureTypes = ['rgb', 'moving', 'colorwheel'];"
      " e.tick = function(fx, inp, pal) {"
      "   var c = (pal.look && pal.look.colors && pal.look.colors.length) ? pal.look.colors[0] : null;"
      "   return fx.map(function() { return c ? { r: c.r, g: c.g, b: c.b } : {}; }); };"
      " return e; })()" },
    // A colour of its own, whatever the look's.
    { "red.js",
      "(function() { var e = {}; e.apiVersion = 1; e.name = 'T Red';"
      " e.fixtureTypes = ['rgb', 'moving', 'colorwheel'];"
      " e.tick = function(fx) { return fx.map(function() { return { r: 255, g: 0, b: 0 }; }); };"
      " return e; })()" },
    // A dimmer effect: half of whatever is there.
    { "half.js",
      "(function() { var e = {}; e.apiVersion = 1; e.name = 'T Half';"
      " e.fixtureTypes = ['dimmer', 'rgb', 'moving'];"
      " e.tick = function(fx) { return fx.map(function() { return { dimmer: 0.5 }; }); };"
      " return e; })()" },
    // Movement only, movers only.
    { "pan90.js",
      "(function() { var e = {}; e.apiVersion = 1; e.name = 'T Pan 90';"
      " e.fixtureTypes = ['moving'];"
      " e.tick = function(fx) { return fx.map(function() { return { pan: 90, tilt: 45 }; }); };"
      " return e; })()" },
    // For a kind of fixture none of the looks here has.
    { "uvonly.js",
      "(function() { var e = {}; e.apiVersion = 1; e.name = 'T UV Only';"
      " e.fixtureTypes = ['uvonly'];"
      " e.tick = function(fx) { return fx.map(function() { return { uv: 255 }; }); };"
      " return e; })()" },
    { "pan180.js",
      "(function() { var e = {}; e.apiVersion = 1; e.name = 'T Pan 180';"
      " e.fixtureTypes = ['moving'];"
      " e.tick = function(fx) { return fx.map(function() { return { pan: 180, tilt: 90 }; }); };"
      " return e; })()" },
};

const QColor kRed(255, 0, 0), kGreen(0, 255, 0), kBlue(0, 0, 255);

QLCChannel *chan(QLCFixtureDef *def, const QString &name, QLCChannel::Preset preset)
{
    QLCChannel *ch = new QLCChannel();
    ch->setName(name);
    ch->setPreset(preset);
    def->addChannel(ch);
    return ch;
}

/* A definition with these channels, the first @p inHead of them its head's
   (the rest, a dimmer, the fixture's master). */
Fixture *make(Doc *doc, const QString &model, QLCFixtureDef::FixtureType type,
              const QList<QLCChannel *> &chans, QLCFixtureDef *def, int inHead,
              quint32 address)
{
    def->setManufacturer("Test");
    def->setModel(model);
    def->setType(type);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Mode");
    for (int c = 0; c < chans.size(); c++)
        mode->insertChannel(chans.at(c), c);
    QLCFixtureHead head;
    for (int c = 0; c < inHead; c++)
        head.addChannel(quint32(c));
    mode->insertHead(-1, head);
    QLCPhysical phy;
    phy.setFocusPanMax(540);
    phy.setFocusTiltMax(270);
    mode->setPhysical(phy);
    mode->cacheHeads();
    def->addMode(mode);
    Fixture *fx = new Fixture(doc);
    fx->setName(model);
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(0);
    fx->setAddress(address);
    doc->addFixture(fx);
    return fx;
}
}

void LookStack_Test::initTestCase()
{
    QVERIFY(m_scripts.isValid());
    for (const auto &s : kScripts)
    {
        QFile f(m_scripts.filePath(QString::fromLatin1(s[0])));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(s[1]);
    }
}

void LookStack_Test::init()
{
    m_doc = new Doc(this);

    QLCFixtureDef *par = new QLCFixtureDef();
    m_par = make(m_doc, "Par", QLCFixtureDef::ColorChanger,
                 { chan(par, "R", QLCChannel::IntensityRed), chan(par, "G", QLCChannel::IntensityGreen),
                   chan(par, "B", QLCChannel::IntensityBlue),
                   chan(par, "Dimmer", QLCChannel::IntensityMasterDimmer) }, par, 3, 0);

    QLCFixtureDef *bat = new QLCFixtureDef();
    m_batten = make(m_doc, "Batten", QLCFixtureDef::ColorChanger,
                    { chan(bat, "R", QLCChannel::IntensityRed), chan(bat, "G", QLCChannel::IntensityGreen),
                      chan(bat, "B", QLCChannel::IntensityBlue), chan(bat, "W", QLCChannel::IntensityWhite) },
                    bat, 4, 10);

    QLCFixtureDef *wd = new QLCFixtureDef();
    QLCChannel *wheel = new QLCChannel();
    wheel->setName("Color wheel");
    wheel->setGroup(QLCChannel::Colour);
    const struct { int lo, hi; QColor c; const char *n; } wheelSlots[] = {
        { 0, 9, QColor(0, 0, 0), "Off" }, { 10, 19, kRed, "Red" }, { 20, 29, kGreen, "Green" },
        { 30, 39, kBlue, "Blue" }, { 40, 49, QColor(255, 255, 255), "White" } };
    for (const auto &sl : wheelSlots)
    {
        QLCCapability *cap = new QLCCapability(uchar(sl.lo), uchar(sl.hi), sl.n);
        cap->setResource(0, sl.c);
        wheel->addCapability(cap);
    }
    wd->addChannel(wheel);
    m_wheel = make(m_doc, "WheelSpot", QLCFixtureDef::MovingHead,
                   { chan(wd, "Pan", QLCChannel::PositionPan), chan(wd, "Tilt", QLCChannel::PositionTilt),
                     wheel, chan(wd, "Dimmer", QLCChannel::IntensityDimmer) }, wd, 4, 20);

    QLCFixtureDef *md = new QLCFixtureDef();
    m_mover = make(m_doc, "Mover", QLCFixtureDef::MovingHead,
                   { chan(md, "Pan", QLCChannel::PositionPan), chan(md, "Tilt", QLCChannel::PositionTilt),
                     chan(md, "R", QLCChannel::IntensityRed), chan(md, "G", QLCChannel::IntensityGreen),
                     chan(md, "B", QLCChannel::IntensityBlue),
                     chan(md, "Dimmer", QLCChannel::IntensityMasterDimmer) }, md, 5, 30);

    QLCFixtureDef *ts = new QLCFixtureDef();
    m_twoSet = make(m_doc, "TwoSet", QLCFixtureDef::ColorChanger,
                    { chan(ts, "R1", QLCChannel::IntensityRed), chan(ts, "G1", QLCChannel::IntensityGreen),
                      chan(ts, "B1", QLCChannel::IntensityBlue), chan(ts, "R2", QLCChannel::IntensityRed),
                      chan(ts, "G2", QLCChannel::IntensityGreen), chan(ts, "B2", QLCChannel::IntensityBlue) },
                    ts, 6, 40);

    // What each is, as the rules need it.
    QVERIFY(m_par->masterIntensityChannel() != QLCChannel::invalid());
    QCOMPARE(m_batten->masterIntensityChannel(), QLCChannel::invalid());
    QCOMPARE(m_wheel->masterIntensityChannel(), QLCChannel::invalid());
    QVERIFY(m_mover->masterIntensityChannel() != QLCChannel::invalid());
}

void LookStack_Test::cleanup()
{
    /* Not deleted: a scene still playing can't be stopped from this thread
       without the timer tick it would need (it holds the universes). One
       Doc a test is a few kilobytes. */
    m_doc = nullptr;
}

QLCPalette *LookStack_Test::colour(const QColor &c)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Color, m_doc);
    p->setValue(c);
    m_doc->addPalette(p);
    return p;
}

QLCPalette *LookStack_Test::dimmer(int value)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
    p->setValue(value);
    m_doc->addPalette(p);
    return p;
}

QLCPalette *LookStack_Test::panTilt(double pan, double tilt)
{
    QLCPalette *p = new QLCPalette(QLCPalette::PanTilt, m_doc);
    p->setValue(pan, tilt);
    m_doc->addPalette(p);
    return p;
}

QLCPalette *LookStack_Test::effect(const QString &file)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Effect, m_doc);
    p->setScriptPath(m_scripts.filePath(file));
    m_doc->addPalette(p);
    return p;
}

QByteArray LookStack_Test::play(const QList<QLCPalette *> &stack, bool asGroup)
{
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    if (asGroup)
    {
        FixtureGroup *grp = new FixtureGroup(m_doc);
        m_doc->addFixtureGroup(grp);
        for (Fixture *fx : { m_par, m_batten, m_wheel, m_mover, m_twoSet })
            grp->assignFixture(fx->id());
        scene->addFixtureGroup(grp->id());
    }
    else
    {
        for (Fixture *fx : { m_par, m_batten, m_wheel, m_mover, m_twoSet })
            scene->addFixture(fx->id());
    }
    for (QLCPalette *p : stack)
        scene->addPalette(p->id());
    m_playing = scene;
    scene->start(m_doc->masterTimer(), FunctionParent::master());
    return tickAndRead();
}

QByteArray LookStack_Test::tickAndRead()
{
    MasterTimer *timer = m_doc->masterTimer();
    for (int i = 0; i < 12; ++i)
    {
        timer->timerTick();
        QTest::qWait(15);                   // the effect runner ticks on its own timer
    }
    QList<Universe *> unis = m_doc->inputOutputMap()->claimUniverses();
    for (Universe *u : unis)
        u->processFaders();
    const QByteArray out = unis.first()->preGMValues().left(64);
    m_doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

static int at(const QByteArray &dmx, int i) { return int(uchar(dmx.at(i))); }

QColor LookStack_Test::rgbOf(const QByteArray &dmx, const Fixture *fx) const
{
    const int a = int(fx->address());
    if (fx == m_par || fx == m_batten)
        return QColor(at(dmx, a), at(dmx, a + 1), at(dmx, a + 2));
    return QColor(at(dmx, a + 2), at(dmx, a + 3), at(dmx, a + 4));        // the mover
}

int LookStack_Test::dimmerOf(const QByteArray &dmx, const Fixture *fx) const
{
    const int a = int(fx->address());
    if (fx == m_par) return at(dmx, a + 3);
    if (fx == m_wheel) return at(dmx, a + 3);
    if (fx == m_mover) return at(dmx, a + 5);
    return -1;
}

int LookStack_Test::wheelOf(const QByteArray &dmx, const Fixture *fx) const
{
    return at(dmx, int(fx->address()) + 2);
}

int LookStack_Test::panOf(const QByteArray &dmx, const Fixture *fx) const
{
    return at(dmx, int(fx->address()));
}

int LookStack_Test::panValue(Fixture *fx, double degrees) const
{
    foreach (const SceneValue &sv, fx->positionToValues(QLCChannel::Pan, float(degrees)))
        if (sv.channel == 0)
            return sv.value;
    return -1;
}

static QString rgb(const QColor &c) { return QString("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue()); }
#define RGB_IS(actual, expected) QVERIFY2((actual) == (expected), \
    qPrintable(QString("%1 is %2, should be %3").arg(#actual, rgb(actual), rgb(expected))))
#define IS(actual, expected) QVERIFY2((actual) == (expected), \
    qPrintable(QString("%1 is %2, should be %3").arg(#actual).arg(actual).arg(expected)))

/* Rule 1: down the list, the lower look wins on what they both set -- but
   Colour looks are numbered, not stacked: the first is colour 1, the next
   colour 2 (a fixture's second colour set, an effect's second colour). A
   fixture with one colour shows colour 1, whole -- never a mix. */
void LookStack_Test::staticLooksTheLowerWins()
{
    QByteArray d = play({ colour(kBlue), colour(kRed) });
    RGB_IS(rgbOf(d, m_par), kBlue);             // colour 1; not blue + red = magenta
    RGB_IS(rgbOf(d, m_mover), kBlue);
    IS(wheelOf(d, m_wheel), 34);                // the blue slot

    init();
    d = play({ colour(kRed), colour(kBlue) });
    RGB_IS(rgbOf(d, m_par), kRed);
    IS(wheelOf(d, m_wheel), 14);

    init();
    d = play({ dimmer(255), dimmer(128) });
    IS(dimmerOf(d, m_par), 128);                // not the higher
    IS(dimmerOf(d, m_wheel), 128);
    IS(dimmerOf(d, m_mover), 128);

    init();
    d = play({ panTilt(180, 90), panTilt(90, 45) });
    IS(panOf(d, m_mover), panValue(m_mover, 90));
    IS(panOf(d, m_wheel), panValue(m_wheel, 90));
}

/* Rule 3: an effect that takes colours, with none of its own, takes the look's. */
void LookStack_Test::aColourEffectTakesTheLooksColour()
{
    const QByteArray d = play({ colour(kBlue), dimmer(255), effect("lookcolour.js") });
    RGB_IS(rgbOf(d, m_par), kBlue);
    RGB_IS(rgbOf(d, m_batten), kBlue);
    IS(wheelOf(d, m_wheel), 34);
    IS(dimmerOf(d, m_par), 255);
}

/* Rule 3: a colour under an effect is the effect's -- its colour, and not
   painted as the look's (the look's blue doesn't show through). */
void LookStack_Test::aColourUnderAnEffectIsThatEffectsOnly()
{
    const QByteArray d = play({ colour(kBlue), dimmer(255), effect("lookcolour.js"), colour(kRed) });
    RGB_IS(rgbOf(d, m_par), kRed);
    RGB_IS(rgbOf(d, m_batten), kRed);
    RGB_IS(rgbOf(d, m_mover), kRed);
    IS(wheelOf(d, m_wheel), 14);
    IS(at(d, int(m_batten->address()) + 3), 0); // white stays out of it
}

/* Rule 1 for effects: two on one colour, the lower in the list wins, each
   with its own colour. */
void LookStack_Test::theLowerOfTwoColourEffectsWins()
{
    QByteArray d = play({ dimmer(255), effect("lookcolour.js"), colour(kGreen),
                          effect("lookcolour.js"), colour(kBlue) });
    RGB_IS(rgbOf(d, m_par), kBlue);
    IS(wheelOf(d, m_wheel), 34);

    init();
    d = play({ dimmer(255), effect("lookcolour.js"), colour(kBlue),
               effect("lookcolour.js"), colour(kGreen) });
    RGB_IS(rgbOf(d, m_par), kGreen);

    init();
    d = play({ dimmer(255), effect("red.js"), effect("lookcolour.js"), colour(kGreen) });
    RGB_IS(rgbOf(d, m_par), kGreen);

    init();
    d = play({ dimmer(255), effect("lookcolour.js"), colour(kGreen), effect("red.js") });
    RGB_IS(rgbOf(d, m_par), kRed);
}

/* Rule 2: an effect's colour replaces the look's on its heads -- not added
   to it (blue under a red effect is not magenta). */
void LookStack_Test::aColourEffectOverridesTheStaticColour()
{
    const QByteArray d = play({ colour(kBlue), dimmer(255), effect("red.js") });
    RGB_IS(rgbOf(d, m_par), kRed);
    RGB_IS(rgbOf(d, m_batten), kRed);
    RGB_IS(rgbOf(d, m_mover), kRed);
    IS(wheelOf(d, m_wheel), 14);
    IS(dimmerOf(d, m_par), 255);
}

/* Rule 2: a dimmer effect scales what is there: a master dimmer, a head's
   own dimmer, or -- with no dimmer -- the colour. Two multiply. */
void LookStack_Test::dimmerEffectsScaleAndMultiply()
{
    QByteArray d = play({ colour(kBlue), dimmer(255), effect("half.js") });
    IS(dimmerOf(d, m_par), 128);
    IS(dimmerOf(d, m_mover), 128);
    IS(dimmerOf(d, m_wheel), 128);              // its head's dimmer
    IS(rgbOf(d, m_batten).blue(), 128);         // no dimmer: its colour
    RGB_IS(rgbOf(d, m_par), kBlue);             // the colour itself left alone
    IS(wheelOf(d, m_wheel), 34);

    init();
    d = play({ colour(kBlue), dimmer(255), effect("half.js"), effect("half.js") });
    IS(dimmerOf(d, m_par), 64);
    IS(dimmerOf(d, m_wheel), 64);
    IS(rgbOf(d, m_batten).blue(), 64);

    init();
    d = play({ colour(kBlue), dimmer(128), effect("half.js") });
    IS(dimmerOf(d, m_par), 64);                 // half of the look's own level
}

/* A movement effect leaves the look's colour and dimmer alone. */
void LookStack_Test::aMovementEffectKeepsTheLooksColourAndDimmer()
{
    const QByteArray d = play({ colour(kBlue), dimmer(255), effect("pan90.js") });
    IS(panOf(d, m_mover), panValue(m_mover, 90));
    IS(panOf(d, m_wheel), panValue(m_wheel, 90));
    RGB_IS(rgbOf(d, m_mover), kBlue);
    IS(dimmerOf(d, m_mover), 255);
    IS(wheelOf(d, m_wheel), 34);
    IS(dimmerOf(d, m_wheel), 255);
    RGB_IS(rgbOf(d, m_par), kBlue);             // not a mover: the look, untouched
}

/* "if you assign colors, dimmers, etc. to effects [they] only apply to that
   effect": under a movement effect, on the heads it moves. */
void LookStack_Test::aColourOrDimmerUnderAMovementEffectIsItsOwn()
{
    const QByteArray d = play({ colour(kBlue), dimmer(255), effect("pan90.js"), colour(kRed), dimmer(128) });
    RGB_IS(rgbOf(d, m_mover), kRed);
    IS(dimmerOf(d, m_mover), 128);
    IS(wheelOf(d, m_wheel), 14);
    IS(dimmerOf(d, m_wheel), 128);
    // The heads it doesn't move keep the look's own.
    RGB_IS(rgbOf(d, m_par), kBlue);
    IS(dimmerOf(d, m_par), 255);
    RGB_IS(rgbOf(d, m_batten), kBlue);
}

void LookStack_Test::aDimmerUnderAMovementEffectIsNotTheLooks()
{
    const QByteArray d = play({ colour(kBlue), effect("pan90.js"), dimmer(255) });
    IS(dimmerOf(d, m_mover), 255);              // the effect's dimmer, on its heads
    IS(dimmerOf(d, m_wheel), 255);
    IS(dimmerOf(d, m_par), 0);                  // not the look's: the par has none
}

/* An Aim or Pan/Tilt is never an effect's: after one, it still points the heads. */
void LookStack_Test::anAimOrPanTiltAfterAnEffectIsStillTheLooks()
{
    const QByteArray d = play({ colour(kBlue), dimmer(255), effect("lookcolour.js"), panTilt(180, 90) });
    IS(panOf(d, m_mover), panValue(m_mover, 180));
    IS(panOf(d, m_wheel), panValue(m_wheel, 180));
    RGB_IS(rgbOf(d, m_mover), kBlue);
}

void LookStack_Test::theLowerOfTwoMovementEffectsWins()
{
    QByteArray d = play({ dimmer(255), effect("pan90.js"), effect("pan180.js") });
    IS(panOf(d, m_mover), panValue(m_mover, 180));

    init();
    d = play({ dimmer(255), effect("pan180.js"), effect("pan90.js") });
    IS(panOf(d, m_mover), panValue(m_mover, 90));
    IS(panOf(d, m_wheel), panValue(m_wheel, 90));
}

/* "How do we stack effects for different things, like both a position and
   an intensity effect?" Both play: the movement on pan/tilt, the dimmer
   effect on the level, in either order. */
void LookStack_Test::aMovementAndADimmerEffectBothPlay()
{
    QByteArray d = play({ colour(kBlue), dimmer(255), effect("pan90.js"), effect("half.js") });
    IS(panOf(d, m_mover), panValue(m_mover, 90));
    IS(dimmerOf(d, m_mover), 128);
    RGB_IS(rgbOf(d, m_mover), kBlue);
    IS(dimmerOf(d, m_par), 128);                // the dimmer effect on every head

    init();
    d = play({ colour(kBlue), dimmer(255), effect("half.js"), effect("pan90.js") });
    IS(panOf(d, m_mover), panValue(m_mover, 90));
    IS(dimmerOf(d, m_mover), 128);
}

/* The same on a Show Tree layer: a Position effect and a Dimmer effect
   side by side (one row each), both on stage. */
void LookStack_Test::aTreeLayerPlaysAMovementAndADimmerEffect()
{
    FixtureGroup *grp = new FixtureGroup(m_doc);
    m_doc->addFixtureGroup(grp);
    for (Fixture *fx : { m_par, m_wheel, m_mover })
        grp->assignFixture(fx->id());
    // The kinds come from the scripts the runner knows, as a real
    // script declares them (effect.fixtureTypes).
    const char *kinds[][2] = {
        { "kindpan.js",
          "(function() { var effect = {}; effect.apiVersion = 1; effect.name = 'T Kind Pan';\n"
          " effect.fixtureTypes = [\"moving\"];\n"
          " effect.tick = function(fx) { return fx.map(function() { return { pan: 90, tilt: 45 }; }); };"
          " return effect; })()" },
        { "kinddim.js",
          "(function() { var effect = {}; effect.apiVersion = 1; effect.name = 'T Kind Half';\n"
          " effect.fixtureTypes = [\"dimmer\"];\n"
          " effect.tick = function(fx) { return fx.map(function() { return { dimmer: 0.5 }; }); };"
          " return effect; })()" } };
    for (const auto &k : kinds)
    {
        QFile f(m_scripts.filePath(QString::fromLatin1(k[0])));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(k[1]);
    }
    m_doc->effectScriptRunner()->cache()->addDirectory(QDir(m_scripts.path()));
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ShowTree::ListMode::NonTracking);
    const quint32 cue = st->addCue(list, -1, "One");
    ShowTree::Target t;
    t.groups << grp->id();
    const quint32 layer = st->addLayer(cue, t);
    QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(colour(kBlue)->id())));
    QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(dimmer(255)->id())));
    const ShowTree::Look pan = st->lookForPalette(effect("kindpan.js")->id());
    const ShowTree::Look half = st->lookForPalette(effect("kinddim.js")->id());
    QVERIFY2(pan.key != half.key, qPrintable(pan.key));
    QVERIFY(st->setLayerLook(cue, layer, pan));
    QVERIFY(st->setLayerLook(cue, layer, half));
    QVERIFY(st->cue(cue)->layer(layer)->look(pan.key) != nullptr);
    QVERIFY(st->cue(cue)->layer(layer)->look(half.key) != nullptr);

    // What the cue plays: its layer's generated scene, both effects in it.
    const Collection *coll = qobject_cast<const Collection *>(m_doc->function(st->cue(cue)->collection));
    QVERIFY(coll != nullptr && !coll->functions().isEmpty());
    Scene *scene = qobject_cast<Scene *>(m_doc->function(coll->functions().first()));
    QVERIFY(scene != nullptr);
    QVERIFY(scene->palettes().contains(pan.palettes.first()));
    QVERIFY(scene->palettes().contains(half.palettes.first()));
    m_playing = scene;
    scene->start(m_doc->masterTimer(), FunctionParent::master());
    const QByteArray d = tickAndRead();
    IS(panOf(d, m_mover), panValue(m_mover, 90));
    IS(dimmerOf(d, m_mover), 128);
    IS(dimmerOf(d, m_par), 128);
}

/* "colors, dimmers, etc. that are above have precedence across all the
   effects": the look's own feed every effect that has none of its own. */
void LookStack_Test::theLooksOwnColoursFeedEveryEffectWithoutItsOwn()
{
    const QByteArray d = play({ colour(kGreen), dimmer(255), effect("lookcolour.js"),
                                effect("half.js"), effect("pan90.js") });
    RGB_IS(rgbOf(d, m_par), kGreen);
    IS(dimmerOf(d, m_par), 128);
    IS(wheelOf(d, m_wheel), 24);
    IS(dimmerOf(d, m_wheel), 128);
    IS(panOf(d, m_mover), panValue(m_mover, 90));
    RGB_IS(rgbOf(d, m_mover), kGreen);
}

/* A Dimmer look on a fixture with no dimmer channel dims its colour -- as
   it does an effect's colour on it. */
void LookStack_Test::aDimmerLookDimsAFixtureWithNoDimmer()
{
    QByteArray d = play({ colour(kBlue), dimmer(128) });
    IS(rgbOf(d, m_batten).blue(), 128);
    IS(dimmerOf(d, m_par), 128);
    IS(rgbOf(d, m_par).blue(), 255);            // a dimmer of its own: the colour stays whole

    init();
    d = play({ colour(kBlue), dimmer(128), effect("lookcolour.js") });
    IS(rgbOf(d, m_batten).blue(), 128);         // the same with an effect's colour
    IS(dimmerOf(d, m_par), 128);
}

/* Colour looks are numbered: colour 1 to a fixture's first colour set,
   colour 2 to its second. */
void LookStack_Test::colourOneAndTwoGoToAFixturesTwoColourSets()
{
    const QByteArray d = play({ colour(kBlue), colour(kRed) });
    const int a = int(m_twoSet->address());
    RGB_IS(QColor(at(d, a), at(d, a + 1), at(d, a + 2)), kBlue);
    RGB_IS(QColor(at(d, a + 3), at(d, a + 4), at(d, a + 5)), kRed);
    RGB_IS(rgbOf(d, m_par), kBlue);             // one set: colour 1
}

/* The same rules when the look is on a fixture group, not fixtures. */
void LookStack_Test::theSameOnAFixtureGroup()
{
    QByteArray d = play({ colour(kBlue), dimmer(255), effect("pan90.js"), colour(kRed), dimmer(128) }, true);
    RGB_IS(rgbOf(d, m_mover), kRed);
    IS(dimmerOf(d, m_mover), 128);
    IS(wheelOf(d, m_wheel), 14);
    RGB_IS(rgbOf(d, m_par), kBlue);
    IS(dimmerOf(d, m_par), 255);

    init();
    d = play({ colour(kBlue), colour(kRed), dimmer(255), dimmer(128) }, true);
    RGB_IS(rgbOf(d, m_par), kBlue);
    IS(dimmerOf(d, m_par), 128);
    IS(wheelOf(d, m_wheel), 34);
    IS(rgbOf(d, m_batten).blue(), 128);

    init();
    d = play({ colour(kBlue), dimmer(255), effect("lookcolour.js"), colour(kRed), effect("half.js") }, true);
    RGB_IS(rgbOf(d, m_par), kRed);
    IS(dimmerOf(d, m_par), 128);
}

/* "dynamic (a group, membership tracked live)" (docs/features/build.md):
   the scene names the group, never its fixtures. */
void LookStack_Test::aLookOnAGroupFollowsItsMembersLive()
{
    FixtureGroup *grp = new FixtureGroup(m_doc);
    m_doc->addFixtureGroup(grp);
    grp->assignFixture(m_par->id());
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixtureGroup(grp->id());
    scene->addPalette(colour(kBlue)->id());
    scene->addPalette(dimmer(200)->id());
    m_playing = scene;
    scene->start(m_doc->masterTimer(), FunctionParent::master());
    QByteArray d = tickAndRead();
    IS(dimmerOf(d, m_par), 200);
    RGB_IS(rgbOf(d, m_par), kBlue);
    IS(dimmerOf(d, m_mover), 0);                // not in the group yet

    // Into the group while it plays: the look reaches it, the scene as it was.
    const QList<SceneValue> before = scene->values();
    grp->assignFixture(m_mover->id());
    d = tickAndRead();
    QCOMPARE(scene->values(), before);
    QCOMPARE(scene->fixtureGroups(), QList<quint32>({ grp->id() }));
    IS(dimmerOf(d, m_par), 200);                // the one already there unchanged
    QVERIFY2(dimmerOf(d, m_mover) == 200 && rgbOf(d, m_mover) == kBlue,
             qPrintable(QString("the new member: dimmer %1, colour %2")
                        .arg(dimmerOf(d, m_mover)).arg(rgb(rgbOf(d, m_mover)))));

    // The group is the target, not a copy of it: started again, the new
    // member has the look.
    auto restart = [&]() {
        scene->stop(FunctionParent::master());
        tickAndRead();
        scene->start(m_doc->masterTimer(), FunctionParent::master());
        return tickAndRead();
    };
    d = restart();
    IS(dimmerOf(d, m_mover), 200);
    RGB_IS(rgbOf(d, m_mover), kBlue);

    // Out again while it plays: it lets go.
    grp->resignFixture(m_mover->id());
    d = tickAndRead();
    IS(dimmerOf(d, m_par), 200);
    QVERIFY2(dimmerOf(d, m_mover) == 0 && rgbOf(d, m_mover) == QColor(0, 0, 0),
             qPrintable(QString("the old member: dimmer %1, colour %2")
                        .arg(dimmerOf(d, m_mover)).arg(rgb(rgbOf(d, m_mover)))));
    d = restart();
    IS(dimmerOf(d, m_mover), 0);
    IS(dimmerOf(d, m_par), 200);
}

/* Joining or leaving a playing scene's group is a fade by the scene's own
   times: in over its fade-in, out over its fade-out (as it stopping would),
   the members that stay untouched. */
void LookStack_Test::aGroupMemberFadesByTheScenesFades()
{
    FixtureGroup *grp = new FixtureGroup(m_doc);
    m_doc->addFixtureGroup(grp);
    grp->assignFixture(m_par->id());
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixtureGroup(grp->id());
    scene->addPalette(dimmer(200)->id());
    scene->setFadeInSpeed(1000);
    scene->setFadeOutSpeed(1000);
    m_playing = scene;
    // Frame by frame: the faders step once a universe frame.
    auto frames = [this](int n) {
        QByteArray out;
        for (int i = 0; i < n; ++i)
        {
            m_doc->masterTimer()->timerTick();
            QList<Universe *> unis = m_doc->inputOutputMap()->claimUniverses();
            for (Universe *u : unis)
                u->processFaders();
            out = unis.first()->preGMValues().left(64);
            m_doc->inputOutputMap()->releaseUniverses(false);
        }
        return out;
    };
    scene->start(m_doc->masterTimer(), FunctionParent::master());
    QByteArray d = frames(70);
    IS(dimmerOf(d, m_par), 200);
    IS(dimmerOf(d, m_mover), 0);

    grp->assignFixture(m_mover->id());
    d = frames(15);
    QVERIFY2(dimmerOf(d, m_mover) > 0 && dimmerOf(d, m_mover) < 200,
             qPrintable(QString("fading in: %1").arg(dimmerOf(d, m_mover))));
    IS(dimmerOf(d, m_par), 200);
    d = frames(60);
    IS(dimmerOf(d, m_mover), 200);

    grp->resignFixture(m_mover->id());
    d = frames(15);
    QVERIFY2(dimmerOf(d, m_mover) > 0 && dimmerOf(d, m_mover) < 200,
             qPrintable(QString("fading out: %1").arg(dimmerOf(d, m_mover))));
    IS(dimmerOf(d, m_par), 200);
    d = frames(60);
    IS(dimmerOf(d, m_mover), 0);
    IS(dimmerOf(d, m_par), 200);

    // Back in while it plays: it fades in again (the let-go channel is kept).
    grp->assignFixture(m_mover->id());
    d = frames(75);
    IS(dimmerOf(d, m_mover), 200);
}

/* "A playing scene follows its group's members live ... one that leaves is
   let go of": of what it had through the group. A pan and a red the scene
   holds on the mover itself are not the group's, and stay. */
void LookStack_Test::aLeavingMembersOwnValuesStay()
{
    FixtureGroup *grp = new FixtureGroup(m_doc);
    m_doc->addFixtureGroup(grp);
    grp->assignFixture(m_par->id());
    grp->assignFixture(m_mover->id());
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixtureGroup(grp->id());
    scene->addPalette(dimmer(200)->id());
    scene->addFixture(m_mover->id());                      // named by the scene too
    scene->setValue(SceneValue(m_mover->id(), 0, 77));     // its pan
    scene->setValue(SceneValue(m_mover->id(), 2, 180));    // its red
    m_playing = scene;
    const int pan = int(m_mover->address()) + 0;
    const int red = int(m_mover->address()) + 2;

    scene->start(m_doc->masterTimer(), FunctionParent::master());
    QByteArray d = tickAndRead();
    IS(dimmerOf(d, m_mover), 200);
    IS(uchar(d.at(pan)), 77);
    IS(uchar(d.at(red)), 180);

    grp->resignFixture(m_mover->id());
    d = tickAndRead();
    d = tickAndRead();
    IS(dimmerOf(d, m_par), 200);                // the rest play on
    IS(uchar(d.at(pan)), 77);                   // the scene's own values stay
    IS(uchar(d.at(red)), 180);
    QVERIFY(scene->isRunning());
}

/* A look edited while it plays is re-painted by the same rules. */
void LookStack_Test::editingALookWhileItPlaysKeepsTheOrder()
{
    QLCPalette *low = dimmer(128);
    QByteArray d = play({ colour(kBlue), dimmer(255), low });
    IS(dimmerOf(d, m_par), 128);
    IS(rgbOf(d, m_batten).blue(), 128);
    low->setValue(64);
    m_playing->requestPaletteRefresh();
    d = tickAndRead();
    IS(dimmerOf(d, m_par), 64);                 // still the lower, now 64
    IS(dimmerOf(d, m_mover), 64);
    IS(rgbOf(d, m_batten).blue(), 64);          // and the batten's colour with it
}

/* The Programming tab tries a palette being edited on the look on the
   canvas: it shows -- a Colour as colour 1 (and an effect's colour when the
   effect has none of its own), anything else as the lowest of the look's
   own, even with an effect in the look. */
void LookStack_Test::aPaletteTriedOnShowsByTheSameRules()
{
    QByteArray d = play({ colour(kBlue), dimmer(255), effect("lookcolour.js") });
    RGB_IS(rgbOf(d, m_par), kBlue);

    QLCPalette *red = colour(kRed);
    m_playing->setTrialPalette(red->id());
    m_playing->requestPaletteRefresh();
    d = tickAndRead();
    RGB_IS(rgbOf(d, m_par), kRed);              // through the effect, as its colour
    IS(wheelOf(d, m_wheel), 14);

    QLCPalette *half = dimmer(128);
    m_playing->setTrialPalette(half->id());
    m_playing->requestPaletteRefresh();
    d = tickAndRead();
    IS(dimmerOf(d, m_par), 128);                // not under the effect: the look's
    RGB_IS(rgbOf(d, m_par), kBlue);

    QLCPalette *aim = panTilt(180, 90);
    m_playing->setTrialPalette(aim->id());
    m_playing->requestPaletteRefresh();
    d = tickAndRead();
    IS(panOf(d, m_mover), panValue(m_mover, 180));
}

/* "can we have the system evaluate the stack looking for inconsistencies
   where some things won't work and let the user know" */
void LookStack_Test::theCheckerSaysWhatWontWork()
{
    auto issuesFor = [this](const QList<QLCPalette *> &stack, bool withTargets = true) {
        Scene *scene = new Scene(m_doc);
        m_doc->addFunction(scene);
        if (withTargets)
            for (Fixture *fx : { m_par, m_batten, m_wheel, m_mover, m_twoSet })
                scene->addFixture(fx->id());
        for (QLCPalette *p : stack)
            scene->addPalette(p->id());
        return LookStackCheck::check(m_doc, scene);
    };
    auto about = [](const QList<LookStackCheck::Issue> &all, QLCPalette *p, const QString &words) {
        for (const LookStackCheck::Issue &i : all)
            if ((p == nullptr || i.paletteId == p->id()) && i.text.contains(words))
                return true;
        return false;
    };
    auto dump = [](const QList<LookStackCheck::Issue> &all) {
        QStringList t;
        for (const LookStackCheck::Issue &i : all)
            t << QString("%1: %2").arg(i.paletteId).arg(i.text);
        return t.join(" | ");
    };

    // A clean look: nothing to say.
    QList<LookStackCheck::Issue> got = issuesFor({ colour(kBlue), dimmer(255), effect("lookcolour.js") });
    QVERIFY2(got.isEmpty(), qPrintable(dump(got)));

    // A static look the one below always covers.
    QLCPalette *full = dimmer(255);
    got = issuesFor({ colour(kBlue), full, dimmer(128) });
    QVERIFY2(about(got, full, "Never shows"), qPrintable(dump(got)));
    QLCPalette *pt = panTilt(90, 45);
    got = issuesFor({ dimmer(255), pt, panTilt(180, 90) });
    QVERIFY2(about(got, pt, "Never shows") && about(got, pt, "position"), qPrintable(dump(got)));

    // An effect the one below covers, on all its heads.
    QLCPalette *upper = effect("lookcolour.js");
    got = issuesFor({ dimmer(255), upper, colour(kGreen), effect("red.js") });
    QVERIFY2(about(got, upper, "Covered"), qPrintable(dump(got)));
    QLCPalette *firstMove = effect("pan90.js");
    got = issuesFor({ dimmer(255), panTilt(0, 0), firstMove, effect("pan180.js") });
    QVERIFY2(about(got, firstMove, "Covered") && about(got, firstMove, "position"), qPrintable(dump(got)));

    // The look's colour replaced by an effect's own.
    QLCPalette *blue = colour(kBlue);
    got = issuesFor({ blue, dimmer(255), effect("lookcolour.js"), colour(kRed) });
    QVERIFY2(about(got, blue, "Replaced"), qPrintable(dump(got)));

    // A dimmer under a movement effect: only its heads get it.
    QLCPalette *ownFull = dimmer(255);
    got = issuesFor({ colour(kBlue), panTilt(0, 0), effect("pan90.js"), ownFull });
    QVERIFY2(about(got, ownFull, "only the 2 head(s)") && about(got, ownFull, "other 3"),
             qPrintable(dump(got)));

    // An effect for fixtures the look hasn't.
    QLCPalette *uv = effect("uvonly.js");
    got = issuesFor({ colour(kBlue), dimmer(255), uv });
    QVERIFY2(about(got, uv, "Does nothing"), qPrintable(dump(got)));

    // A movement effect with nothing to move round.
    QLCPalette *loose = effect("pan90.js");
    got = issuesFor({ dimmer(255), loose });
    QVERIFY2(about(got, loose, "No Aim or Pan/Tilt"), qPrintable(dump(got)));

    // A third colour no fixture or effect can show (the two-set bar takes 2).
    QLCPalette *third = colour(kGreen);
    got = issuesFor({ colour(kBlue), colour(kRed), third, dimmer(255) });
    QVERIFY2(about(got, third, "Colour 3"), qPrintable(dump(got)));

    // A colour saved black (a "White" at #000000000000): it lights nothing,
    // and confetti over it takes black from it.
    QLCPalette *black = new QLCPalette(QLCPalette::Color, m_doc);
    black->setValue(QStringLiteral("#000000000000"));
    m_doc->addPalette(black);
    got = issuesFor({ black, dimmer(62), effect("lookcolour.js") });
    QVERIFY2(about(got, black, "Black"), qPrintable(dump(got)));
    QLCPalette *white = new QLCPalette(QLCPalette::Color, m_doc);
    white->setValue(QStringLiteral("#000000ff0000"));    // RGB off, the white LED on
    m_doc->addPalette(white);
    got = issuesFor({ white, dimmer(62) });
    QVERIFY2(!about(got, white, "Black"), qPrintable(dump(got)));
    // A definition whose only Pan channels are both Fine (a "Pan" saved with
    // Byte="1", as the 36LED mover's was): an Aim can't turn it.
    {
        QLCFixtureDef *fd = new QLCFixtureDef();
        QLCChannel *pan = new QLCChannel();
        pan->setName("Pan");
        pan->setGroup(QLCChannel::Pan);
        pan->setControlByte(QLCChannel::LSB);
        fd->addChannel(pan);
        Fixture *fineOnly = make(m_doc, "FineOnly", QLCFixtureDef::MovingHead,
                                 { pan, chan(fd, "Pan fine", QLCChannel::PositionPanFine),
                                   chan(fd, "Tilt", QLCChannel::PositionTilt),
                                   chan(fd, "Dimmer", QLCChannel::IntensityMasterDimmer) }, fd, 3, 60);
        QCOMPARE(fineOnly->channelNumber(QLCChannel::Pan, QLCChannel::MSB), QLCChannel::invalid());
        QLCPalette *aim = panTilt(90, 45);
        Scene *scene = new Scene(m_doc);
        m_doc->addFunction(scene);
        scene->addFixture(fineOnly->id());
        scene->addFixture(m_mover->id());
        scene->addPalette(dimmer(255)->id());
        scene->addPalette(aim->id());
        got = LookStackCheck::check(m_doc, scene);
        QVERIFY2(about(got, aim, "Can't move 1 fixture") && about(got, aim, "FineOnly's Pan"),
                 qPrintable(dump(got)));
        QVERIFY2(!about(got, aim, "Tilt") && !about(got, aim, "Mover"), qPrintable(dump(got)));
    }

    // The clean look above stays clean: a real colour is not black.
    got = issuesFor({ colour(kBlue), dimmer(255) });
    QVERIFY2(!about(got, nullptr, "Black"), qPrintable(dump(got)));

    // No targets.
    got = issuesFor({ colour(kBlue) }, false);
    QVERIFY2(about(got, nullptr, "No targets"), qPrintable(dump(got)));
}

QTEST_MAIN(LookStack_Test)

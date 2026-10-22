/*
  Q Light Controller Plus - Test Unit
  shutterautoopen_test.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QSignalSpy>
#include <QtTest>

#define private public
#define protected public
#include "universe.h"
#include "mastertimer_stub.h"
#include "scene.h"
#undef private
#undef protected

#include "shutterautoopen_test.h"
#include "shutterautoopen.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "inputoutputmap.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "qlcpalette.h"
#include "scenevalue.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "qlcfile.h"
#include "fixture.h"
#include "doc.h"

#include "../common/resource_paths.h"

/* The AT-Spots of the classroom show and DanceHysteria: an American DJ
   Focus Spot Three Z, whose shutter rests at 0 -- "Off", the beam closed --
   and opens at 8-15. */
void ShutterAutoOpen_Test::initTestCase()
{
    m_doc = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir));

    /* A copy of DanceHysteria's LED Movinghead+Circle: its shutter is a
       strobe-preset channel -- 0 open, 1-255 strobe, nothing that closes. */
    QLCFixtureDef *mover = new QLCFixtureDef();
    QCOMPARE(mover->loadXML(QString(SHUTTER_TEST_DIR) + "Branson-LED-Movinghead-Circle.qxf"),
             QFile::NoError);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(mover));
}

void ShutterAutoOpen_Test::cleanupTestCase()
{
    delete m_doc;
}

void ShutterAutoOpen_Test::init()
{
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef("American DJ", "Focus Spot Three Z");
    QVERIFY(def != nullptr);
    QLCFixtureMode *mode = def->mode("18 Channel");
    QVERIFY(mode != nullptr);
    m_spot = new Fixture(m_doc);
    m_spot->setName("SR AT Spot");
    m_spot->setFixtureDefinition(def, mode);
    m_spot->setAddress(20);
    m_spot->setUniverse(0);
    QVERIFY(m_doc->addFixture(m_spot));

    m_dimmer = 20 + int(m_spot->masterIntensityChannel());
    m_shutter = 20 + int(mode->channelNumber(QLCChannel::Shutter, QLCChannel::MSB));
    QVERIFY(m_dimmer >= 20 && m_shutter >= 20 && m_dimmer != m_shutter);

    m_doc->shutterAutoOpen()->rebuild();
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    m_uni = ua.first();
    m_doc->inputOutputMap()->releaseUniverses(false);
}

void ShutterAutoOpen_Test::cleanup()
{
    m_doc->clearContents();
    m_doc->shutterAutoOpen()->rebuild();
    // Each test starts from a clear universe.
    m_uni->reset();
    for (int i = 0; i < 4; i++)
        frame();
    m_spot = nullptr;
}

int ShutterAutoOpen_Test::frame()
{
    m_uni->processFaders();
    return uchar(m_uni->postGMValues()->at(m_shutter));
}

Fixture *ShutterAutoOpen_Test::addMover(int address)
{
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef("Branson", "LED Movinghead+Circle");
    if (def == nullptr || def->mode("10 Channel") == nullptr)
        return nullptr;
    Fixture *f = new Fixture(m_doc);
    f->setName("T1-S1");
    f->setFixtureDefinition(def, def->mode("10 Channel"));
    f->setAddress(quint32(address));
    f->setUniverse(0);
    if (m_doc->addFixture(f) == false)
    {
        delete f;
        return nullptr;
    }
    return f;
}

static void put(Doc *doc, QSharedPointer<GenericFader> fader, Universe *u, int address, uchar v,
                int extraFlags = 0)
{
    GenericFader::ChannelRef fc = fader->getChannelFader(doc, u, Fixture::invalidId(), quint32(address));
    fc->setCurrent(v);
    fc->setTarget(v);
    if (extraFlags)
        fc->addFlag(extraFlags);
}

void ShutterAutoOpen_Test::theOpenValueComesFromTheDefinition()
{
    const QLCChannel *sh = m_spot->channel(quint32(m_shutter - 20));
    QVERIFY(sh != nullptr);
    QCOMPARE(int(sh->defaultValue()), 0);
    QCOMPARE(sh->searchCapability(0)->preset(), QLCCapability::ShutterClose);
    const int open = ShutterAutoOpen::openValue(sh);
    QVERIFY2(open >= 8 && open <= 15, qPrintable(QString::number(open)));

    const QVector<Universe::AutoOpenRule> rules = ShutterAutoOpen::rulesFor(m_spot);
    QCOMPARE(rules.size(), 1);
    QCOMPARE(rules.first().shutter, m_shutter);
    QCOMPARE(int(rules.first().open), open);
    QCOMPARE(m_doc->shutterAutoOpen()->ruleCount(), 1);

    // A default that is open is the open value itself.
    QLCChannel ch;
    ch.setGroup(QLCChannel::Shutter);
    ch.addCapability(new QLCCapability(0, 9, "Closed"));
    ch.addCapability(new QLCCapability(10, 19, "Shutter open"));
    ch.addCapability(new QLCCapability(20, 255, "Strobe slow to fast"));
    QCOMPARE(ShutterAutoOpen::openValue(&ch), 14);    // by name
    ch.setDefaultValue(12);
    QCOMPARE(ShutterAutoOpen::openValue(&ch), 12);
    // Effects named with "open" are not open; an iris never is.
    QLCChannel fx;
    fx.setGroup(QLCChannel::Shutter);
    fx.addCapability(new QLCCapability(0, 127, "Slow open - fast close"));
    QCOMPARE(ShutterAutoOpen::openValue(&fx), -1);
    QLCChannel iris;
    iris.setPreset(QLCChannel::ShutterIrisMinToMax);
    QCOMPARE(ShutterAutoOpen::openValue(&iris), -1);
}

void ShutterAutoOpen_Test::aClosedSpotLightsAtFull()
{
    QSharedPointer<GenericFader> fader = m_uni->requestFader();
    put(m_doc, fader, m_uni, m_dimmer, 255);
    QSignalSpy spy(m_uni, &Universe::universeWritten);
    const int open = frame();
    QVERIFY2(open >= 8 && open <= 15, qPrintable(QString::number(open)));
    // DMX and the preview get the same: what goes out carries it.
    QVERIFY(spy.count() >= 1);
    QCOMPARE(int(uchar(spy.last().at(1).toByteArray().at(m_shutter))), open);
    // The stored value is untouched: it is applied on output only.
    QCOMPARE(int(m_uni->preGMValue(m_shutter)), 0);

    // The level goes: the shutter closes again.
    m_uni->dismissFader(fader);
    frame();
    frame();
    QCOMPARE(frame(), 0);
}

void ShutterAutoOpen_Test::turnedOffItStaysDark()
{
    QSignalSpy changed(m_doc->shutterAutoOpen(), &ShutterAutoOpen::enabledChanged);
    QSharedPointer<GenericFader> fader = m_uni->requestFader();
    put(m_doc, fader, m_uni, m_dimmer, 255);
    QVERIFY(frame() > 0);

    m_doc->shutterAutoOpen()->setEnabled(false);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(m_doc->shutterAutoOpen()->ruleCount(), 0);
    QCOMPARE(frame(), 0);           // the one it opened goes back
    QCOMPARE(frame(), 0);

    m_doc->shutterAutoOpen()->setEnabled(true);
    QVERIFY(frame() > 0);
    m_uni->dismissFader(fader);
}

void ShutterAutoOpen_Test::anExplicitShutterWins()
{
    QSharedPointer<GenericFader> fader = m_uni->requestFader();
    put(m_doc, fader, m_uni, m_dimmer, 255);
    put(m_doc, fader, m_uni, m_shutter, 0);         // Closed
    QCOMPARE(frame(), 0);
    QCOMPARE(frame(), 0);

    put(m_doc, fader, m_uni, m_shutter, 70);        // Strobe
    QCOMPARE(frame(), 70);
    QCOMPARE(frame(), 70);
    m_uni->dismissFader(fader);
}

void ShutterAutoOpen_Test::aHeadWithNoLevelIsUntouched()
{
    QCOMPARE(frame(), 0);
    QCOMPARE(frame(), 0);

    // Given a level of zero is no level.
    QSharedPointer<GenericFader> fader = m_uni->requestFader();
    put(m_doc, fader, m_uni, m_dimmer, 0);
    QCOMPARE(frame(), 0);
    m_uni->dismissFader(fader);
}

void ShutterAutoOpen_Test::aRestingDimmerIsNotALevel()
{
    // A dimmer that sits at full with nothing driving it does not open the
    // shutter: nothing lights by itself.
    m_uni->setChannelDefaultValue(ushort(m_dimmer), 255);
    QVERIFY(m_uni->postGMValue(m_dimmer) == 255 || m_uni->preGMValue(m_dimmer) == 255);
    QCOMPARE(frame(), 0);
    QCOMPARE(frame(), 0);
    m_uni->setChannelDefaultValue(ushort(m_dimmer), 0);
}

void ShutterAutoOpen_Test::aOneShotClosedStaysClosed()
{
    // A slider or a programmer edit puts the shutter at Closed and lets go
    // (AutoRemove): still an explicit value.
    QSharedPointer<GenericFader> level = m_uni->requestFader();
    put(m_doc, level, m_uni, m_dimmer, 255);
    QVERIFY(frame() > 0);

    QSharedPointer<GenericFader> slider = m_uni->requestFader();
    put(m_doc, slider, m_uni, m_shutter, 0, FadeChannel::AutoRemove);
    QCOMPARE(frame(), 0);
    QCOMPARE(slider->channelsCount(), 0);           // let go
    for (int i = 0; i < 4; i++)
        QCOMPARE(frame(), 0);

    // Something else writing it again ends the hold.
    put(m_doc, slider, m_uni, m_shutter, 0);
    QCOMPARE(frame(), 0);
    m_uni->dismissFader(slider);
    frame();
    frame();
    QVERIFY(frame() > 0);
    m_uni->dismissFader(level);
}

void ShutterAutoOpen_Test::aSceneWithOnlyALevelLights()
{
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    MasterTimerStub timer(m_doc, ua);
    Scene *look = new Scene(m_doc);
    look->setName("01-01.00-Spots Full");
    look->setValue(m_spot->id(), quint32(m_dimmer - 20), 255);
    QVERIFY(m_doc->addFunction(look));

    look->preRun(&timer);
    look->write(&timer, ua);
    const int open = frame();
    QVERIFY2(open >= 8 && open <= 15, qPrintable(QString::number(open)));
    QCOMPARE(int(m_uni->postGMValue(m_dimmer)), 255);

    // The same look with Shutter -> Closed stays dark.
    look->setValue(m_spot->id(), quint32(m_shutter - 20), 0);
    look->postRun(&timer, ua);
    look->preRun(&timer);
    look->write(&timer, ua);
    QCOMPARE(frame(), 0);
    QCOMPARE(frame(), 0);
    look->postRun(&timer, ua);
    m_doc->inputOutputMap()->releaseUniverses(false);
}

void ShutterAutoOpen_Test::itIsSavedWithTheShow()
{
    ShutterAutoOpen *ao = m_doc->shutterAutoOpen();
    QVERIFY(ao->isEnabled());           // a new show's

    QByteArray on;
    {
        QXmlStreamWriter w(&on);
        ao->saveXML(&w);
    }
    QVERIFY(on.isEmpty());              // as before: nothing written

    ao->setEnabled(false);
    QByteArray off;
    {
        QXmlStreamWriter w(&off);
        ao->saveXML(&w);
    }
    QVERIFY(off.contains("ShutterAutoOpen"));

    ao->reset();
    QVERIFY(ao->isEnabled());
    QXmlStreamReader r(off);
    r.readNextStartElement();
    QCOMPARE(r.name().toString(), KXMLQLCShutterAutoOpen);
    ao->loadXML(r);
    QVERIFY(ao->isEnabled() == false);

    // A whole show round trip.
    QByteArray show;
    {
        QXmlStreamWriter w(&show);
        w.writeStartDocument();
        QVERIFY(m_doc->saveXML(&w));
        w.writeEndDocument();
    }
    QVERIFY(show.contains("<ShutterAutoOpen Enabled=\"0\"/>"));
    m_doc->clearContents();
    QVERIFY(ao->isEnabled());
    QXmlStreamReader r2(show);
    r2.readNextStartElement();
    QVERIFY(m_doc->loadXML(r2, false));
    QVERIFY(ao->isEnabled() == false);
    ao->setEnabled(true);
}

/* The value a Shutter look puts on @p fx's shutter channel (-1: none). */
static int shutterOut(Doc *doc, QLCPalette *look, const QList<quint32> &fixtures, const Fixture *fx)
{
    const quint32 ch = QLCPalette::shutterChannel(fx);
    for (const SceneValue &sv : look->valuesFromFixtures(doc, fixtures))
        if (sv.fxi == fx->id() && sv.channel == ch)
            return sv.value;
    return -1;
}

void ShutterAutoOpen_Test::aShutterLookMeansWhatItSays()
{
    // The AT-Spot: Off 0-7 (closed), Open 8-15, Strobing slow - fast 16-131,
    // and Open again at 248-255.
    const QList<quint32> spot = { m_spot->id() };
    const int open = ShutterAutoOpen::openValue(m_spot->channel(QLCPalette::shutterChannel(m_spot)));
    QLCPalette *look = new QLCPalette(QLCPalette::Shutter);
    QVERIFY(m_doc->addPalette(look));

    // No value, or 0 -- every Shutter look made before Closed could be
    // picked, the course shows' "Open" among them -- opens.
    QCOMPARE(look->shutterIntent(m_doc, spot), QLCPalette::ShutterIntentOpen);
    QCOMPARE(shutterOut(m_doc, look, spot, m_spot), open);
    look->setValue(0);
    QCOMPARE(shutterOut(m_doc, look, spot, m_spot), open);

    // Closed is closed. Before, any fixture with an Open capability was
    // sent that, whatever the look said.
    look->setValue(3);
    QCOMPARE(look->shutterIntent(m_doc, spot), QLCPalette::ShutterIntentClosed);
    QCOMPARE(shutterOut(m_doc, look, spot, m_spot), 3);

    // Open and Strobe: as set.
    look->setValue(11);
    QCOMPARE(look->shutterIntent(m_doc, spot), QLCPalette::ShutterIntentOpen);
    QCOMPARE(shutterOut(m_doc, look, spot, m_spot), 11);
    look->setValue(255);            // DanceHysteria's "Open"
    QCOMPARE(look->shutterIntent(m_doc, spot), QLCPalette::ShutterIntentOpen);
    QCOMPARE(shutterOut(m_doc, look, spot, m_spot), 255);
    look->setValue(70);
    QCOMPARE(look->shutterIntent(m_doc, spot), QLCPalette::ShutterIntentStrobe);
    QCOMPARE(shutterOut(m_doc, look, spot, m_spot), 70);
    // An effect of its own: as set too.
    look->setValue(150);            // Slow open - fast close
    QCOMPARE(look->shutterIntent(m_doc, spot), QLCPalette::ShutterIntentOther);
    QCOMPARE(shutterOut(m_doc, look, spot, m_spot), 150);

    // A strobe-only shutter alone never reads the value: it has no closed,
    // and a 255 there is as likely a legacy "Open" as a strobe.
    Fixture *mover = addMover(100);
    QVERIFY(mover != nullptr);
    const QList<quint32> movers = { mover->id() };
    for (int v : { 0, 3, 11, 70, 255 })
    {
        look->setValue(v);
        QCOMPARE(look->shutterIntent(m_doc, movers), QLCPalette::ShutterIntentOpen);
        QCOMPARE(shutterOut(m_doc, look, movers, mover), 0);
    }

    // Names, for definitions that never tagged their capabilities.
    QLCCapability closed(0, 9, "Blackout"), shut(0, 9, "Shutter closed"), on(10, 19, "Open"),
                  strobe(20, 255, "Strobe slow to fast"), off(0, 9, "Strobe off"),
                  effect(0, 9, "Slow open - fast close"), none(0, 9, "No function");
    QCOMPARE(QLCPalette::capabilityShutterIntent(&closed), QLCPalette::ShutterIntentClosed);
    QCOMPARE(QLCPalette::capabilityShutterIntent(&shut), QLCPalette::ShutterIntentClosed);
    QCOMPARE(QLCPalette::capabilityShutterIntent(&on), QLCPalette::ShutterIntentOpen);
    QCOMPARE(QLCPalette::capabilityShutterIntent(&strobe), QLCPalette::ShutterIntentStrobe);
    QCOMPARE(QLCPalette::capabilityShutterIntent(&off), QLCPalette::ShutterIntentOther);
    QCOMPARE(QLCPalette::capabilityShutterIntent(&effect), QLCPalette::ShutterIntentOther);
    QCOMPARE(QLCPalette::capabilityShutterIntent(&none), QLCPalette::ShutterIntentOther);
}

void ShutterAutoOpen_Test::aShutterLookOnAMixedRig()
{
    /* A spot, a strobe-only mover, and a bar whose shutter only names its
       capabilities -- "No function" 0-9, "Strobe (slow to fast)" 10-255,
       like DanceHysteria's 60W bars -- in one group. */
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Named bar");
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCChannel *dim = new QLCChannel();
    dim->setName("Dimmer");
    dim->setPreset(QLCChannel::IntensityMasterDimmer);
    QLCChannel *sh = new QLCChannel();
    sh->setName("Strobe");
    sh->setGroup(QLCChannel::Shutter);
    QVERIFY(sh->addCapability(new QLCCapability(0, 9, "No function")));
    QVERIFY(sh->addCapability(new QLCCapability(10, 255, "Strobe (slow to fast)")));
    def->addChannel(dim);
    def->addChannel(sh);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    mode->insertChannel(dim, 0);
    mode->insertChannel(sh, 1);
    def->addMode(mode);
    Fixture *bar = new Fixture(m_doc);
    bar->setFixtureDefinition(def, mode);
    bar->setAddress(200);
    QVERIFY(m_doc->addFixture(bar));
    Fixture *mover = addMover(100);
    QVERIFY(mover != nullptr);

    QLCPalette *look = new QLCPalette(QLCPalette::Shutter);
    QVERIFY(m_doc->addPalette(look));
    const QList<quint32> orders[] = { { bar->id(), mover->id(), m_spot->id() },
                                      { m_spot->id(), mover->id(), bar->id() } };
    for (const QList<quint32> &rig : orders)
    {
        // "Open" at 255: the spot's tagged Open decides it, whichever
        // comes first; the bar is not strobed at full, the mover opens.
        look->setValue(255);
        QCOMPARE(look->shutterIntent(m_doc, rig), QLCPalette::ShutterIntentOpen);
        QCOMPARE(shutterOut(m_doc, look, rig, m_spot), 255);
        QCOMPARE(shutterOut(m_doc, look, rig, bar), 4);         // No function
        QCOMPARE(shutterOut(m_doc, look, rig, mover), 0);

        // Closed: the spot closes; the others have no closed and stay open.
        look->setValue(3);
        QCOMPARE(look->shutterIntent(m_doc, rig), QLCPalette::ShutterIntentClosed);
        QCOMPARE(shutterOut(m_doc, look, rig, m_spot), 3);
        QCOMPARE(shutterOut(m_doc, look, rig, bar), 4);
        QCOMPARE(shutterOut(m_doc, look, rig, mover), 0);

        // Strobe halfway up the spot's range: each strobes, at that rate.
        look->setValue(16 + (131 - 16) / 2);
        QCOMPARE(look->shutterIntent(m_doc, rig), QLCPalette::ShutterIntentStrobe);
        QCOMPARE(shutterOut(m_doc, look, rig, m_spot), 73);
        QCOMPARE(shutterOut(m_doc, look, rig, bar), 73);        // a strobe there too
        const int m = shutterOut(m_doc, look, rig, mover);
        QVERIFY2(m > 110 && m < 145, qPrintable(QString::number(m)));
    }
}

void ShutterAutoOpen_Test::aShutterLookOnTheWire_data()
{
    QTest::addColumn<int>("look");          // -2: no Shutter look; -1: one with no value
    QTest::addColumn<bool>("autoOpen");
    QTest::addColumn<int>("spot");          // -1: the spot's open value
    QTest::addColumn<int>("mover");

    QTest::newRow("Closed, auto-open on")   << 3   << true  << 3   << 0;
    QTest::newRow("Closed, auto-open off")  << 3   << false << 3   << 0;
    QTest::newRow("Open, auto-open on")     << 11  << true  << 11  << 0;
    QTest::newRow("Open, auto-open off")    << 11  << false << 11  << 0;
    QTest::newRow("Strobe, auto-open on")   << 70  << true  << 70  << 120;
    QTest::newRow("Strobe, auto-open off")  << 70  << false << 70  << 120;
    QTest::newRow("Open look, no value")    << -1  << false << -1  << 0;
    QTest::newRow("No look, auto-open on")  << -2  << true  << -1  << 0;
    QTest::newRow("No look, auto-open off") << -2  << false << 0   << 0;
}

/* What goes out of the universe -- DMX and the preview alike -- for a look
   at full on the spot and a mover, with a Shutter look or none. */
void ShutterAutoOpen_Test::aShutterLookOnTheWire()
{
    QFETCH(int, look);
    QFETCH(bool, autoOpen);
    QFETCH(int, spot);
    QFETCH(int, mover);

    const int open = ShutterAutoOpen::openValue(m_spot->channel(quint32(m_shutter - 20)));
    if (spot < 0)
        spot = open;
    Fixture *mv = addMover(100);
    QVERIFY(mv != nullptr);
    const int mvShutter = 100 + int(QLCPalette::shutterChannel(mv));
    const quint32 mvDimmer = mv->fixtureMode()->channelNumber(QLCChannel::Intensity, QLCChannel::MSB);
    QVERIFY(mvDimmer != QLCChannel::invalid());
    m_doc->shutterAutoOpen()->setEnabled(autoOpen);

    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    MasterTimerStub timer(m_doc, ua);
    Scene *scene = new Scene(m_doc);
    scene->setName("01-01.00-Spots Full");
    scene->setValue(m_spot->id(), quint32(m_dimmer - 20), 255);
    scene->setValue(mv->id(), mvDimmer, 255);
    scene->addFixture(m_spot->id());
    scene->addFixture(mv->id());
    if (look > -2)
    {
        QLCPalette *p = new QLCPalette(QLCPalette::Shutter);
        if (look >= 0)
            p->setValue(look);
        QVERIFY(m_doc->addPalette(p));
        scene->addPalette(p->id());
    }
    QVERIFY(m_doc->addFunction(scene));

    scene->preRun(&timer);
    for (int i = 0; i < 3; i++)
    {
        scene->write(&timer, ua);
        QCOMPARE(frame(), spot);
        // A strobe: at the spot's rate, (70 - 16) / (131 - 16) of the way up.
        QCOMPARE(int(uchar(m_uni->postGMValues()->at(mvShutter))), mover);
    }
    scene->postRun(&timer, ua);
    m_doc->inputOutputMap()->releaseUniverses(false);
    m_doc->shutterAutoOpen()->setEnabled(true);
}

QTEST_MAIN(ShutterAutoOpen_Test)

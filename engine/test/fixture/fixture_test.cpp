/*
  Q Light Controller Plus - Unit test
  fixture_test.cpp

  Copyright (c) Heikki Junnila
                Massimo Callegari

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
#include <QPointer>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "qlcconfig.h"
#include "qlcfile.h"

#include "fixture_test.h"
#include "fixture.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "universe.h"

#include "../common/resource_paths.h"

void Fixture_Test::initTestCase()
{
    m_doc = new Doc(this);

    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir) == true);
}

void Fixture_Test::cleanupTestCase()
{
    delete m_doc;
}

void Fixture_Test::id()
{
    QVERIFY(Fixture::invalidId() == UINT_MAX);

    Fixture fxi(this);
    QVERIFY(fxi.id() == Fixture::invalidId());

    fxi.setID(50);
    QVERIFY(fxi.id() == 50);

    fxi.setID(INT_MAX);
    QVERIFY(fxi.id() == INT_MAX);
}

void Fixture_Test::name()
{
    Fixture fxi(this);
    QVERIFY(fxi.name().isEmpty());

    fxi.setName("MyFixture");
    QVERIFY(fxi.name() == "MyFixture");
}

void Fixture_Test::addressKeepsHighUniverses()
{
    Fixture fxi(this);
    fxi.setChannels(5);

    const quint32 universes[] = { 127, 128, 129, 200, 255, 256, 1023, 100000 };
    for (quint32 uni : universes)
    {
        fxi.setUniverse(uni);
        fxi.setAddress(42);
        QCOMPARE(fxi.universe(), uni);
        QCOMPARE(fxi.address(), quint32(42));
        QCOMPARE(fxi.universeAddress(), (uni << 9) | 42);

        // And a second re-address keeps it too.
        fxi.setAddress(511);
        QCOMPARE(fxi.universe(), uni);
        QCOMPARE(fxi.address(), quint32(511));
    }
}

void Fixture_Test::address()
{
    Fixture fxi(this);
    fxi.setChannels(5);

    QVERIFY(fxi.address() == 0);
    QVERIFY(fxi.universe() == 0);
    QVERIFY(fxi.universeAddress() == 0);

    fxi.setUniverse(1);
    QVERIFY(fxi.address() == 0);
    QVERIFY(fxi.universe() == 1);
    QVERIFY(fxi.universeAddress() == (1 << 9));

    fxi.setUniverse(2);
    QVERIFY(fxi.address() == 0);
    QVERIFY(fxi.universe() == 2);
    QVERIFY(fxi.universeAddress() == (2 << 9));

    fxi.setUniverse(3);
    QVERIFY(fxi.address() == 0);
    QVERIFY(fxi.universe() == 3);
    QVERIFY(fxi.universeAddress() == (3 << 9));

    /* The application might support only 4 universes, but there's no
       reason why Fixture itself couldn't support a million universes,
       as long as it fits into a uint minus 9 bits. */
    fxi.setUniverse(100);
    QVERIFY(fxi.address() == 0);
    QVERIFY(fxi.universe() == 100);
    QVERIFY(fxi.universeAddress() == (100 << 9));

    fxi.setAddress(15);
    fxi.setUniverse(0);
    QVERIFY(fxi.address() == 15);
    QVERIFY(fxi.universe() == 0);
    QVERIFY(fxi.universeAddress() == 15);

    /* Fixture should allow address overflow; maybe the first two channels
       that still fit to the universe here are enough for some fixture,
       who knows? */
    fxi.setAddress(510);
    QVERIFY(fxi.address() == 510);
    QVERIFY(fxi.universe() == 0);
    QVERIFY(fxi.universeAddress() == 510);

    /* Invalid addresses should not be allowed */
    fxi.setAddress(600);
    QVERIFY(fxi.address() == 510);
    QVERIFY(fxi.universe() == 0);
    QVERIFY(fxi.universeAddress() == 510);

    fxi.setAddress(100);
    QVERIFY(fxi.channelAddress(0) == 100);
    QVERIFY(fxi.channelAddress(1) == 101);
    QVERIFY(fxi.channelAddress(2) == 102);
    QVERIFY(fxi.channelAddress(3) == 103);
    QVERIFY(fxi.channelAddress(4) == 104);
    QVERIFY(fxi.channelAddress(5) == QLCChannel::invalid());
    QVERIFY(fxi.channelAddress(20) == QLCChannel::invalid());
}

void Fixture_Test::lessThan()
{
    Fixture fxi1(this);
    Fixture fxi2(this);

    QVERIFY(!(fxi1 < fxi2));
    QVERIFY(!(fxi2 < fxi1));

    fxi1.setAddress(0);
    fxi2.setAddress(1);
    QVERIFY(fxi1 < fxi2);
    QVERIFY(!(fxi2 < fxi1));

    fxi1.setAddress(511);
    fxi2.setAddress(42);
    QVERIFY(fxi2 < fxi1);
    QVERIFY(!(fxi1 < fxi2));
}

void Fixture_Test::type()
{
    Fixture fxi(this);
    QCOMPARE(fxi.typeString(), QString(KXMLFixtureDimmer));
    QCOMPARE(fxi.type(), QLCFixtureDef::Dimmer);
    QCOMPARE(fxi.iconResource(), QString(":/dimmer.png"));

    QLCFixtureDef* fixtureDef;
    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Martin", "MAC250+");
    QVERIFY(fixtureDef != NULL);

    QLCFixtureMode* fixtureMode;
    fixtureMode = fixtureDef->modes().at(0);
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    QCOMPARE(fxi.typeString(), fixtureDef->typeToString(fixtureDef->type()));
    QCOMPARE(fxi.type(), QLCFixtureDef::MovingHead);
    QCOMPARE(fxi.iconResource(), QString(":/movinghead.png"));

    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("SGM", "Colorlab 250");
    QVERIFY(fixtureDef != NULL);
    fixtureMode = fixtureDef->modes().at(0);
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    QCOMPARE(fxi.typeString(), fixtureDef->typeToString(fixtureDef->type()));
    QCOMPARE(fxi.type(), QLCFixtureDef::ColorChanger);
    QCOMPARE(fxi.iconResource(), QString(":/fixture.png"));

    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Chauvet", "Vue 3.1");
    QVERIFY(fixtureDef != NULL);
    fixtureMode = fixtureDef->modes().at(0);
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    QCOMPARE(fxi.typeString(), fixtureDef->typeToString(fixtureDef->type()));
    QCOMPARE(fxi.type(), QLCFixtureDef::Effect);
    QCOMPARE(fxi.iconResource(), QString(":/effect.png"));

    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Cameo", "Storm");
    QVERIFY(fixtureDef != NULL);
    fixtureMode = fixtureDef->modes().at(0);
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    QCOMPARE(fxi.typeString(), fixtureDef->typeToString(fixtureDef->type()));
    QCOMPARE(fxi.type(), QLCFixtureDef::Flower);
    QCOMPARE(fxi.iconResource(), QString(":/flower.png"));

    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Showtec", "Dragon F-350");
    QVERIFY(fixtureDef != NULL);
    fixtureMode = fixtureDef->modes().at(0);
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    QCOMPARE(fxi.typeString(), fixtureDef->typeToString(fixtureDef->type()));
    QCOMPARE(fxi.type(), QLCFixtureDef::Hazer);
    QCOMPARE(fxi.iconResource(true), QString("qrc:/hazer.svg"));

    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("beamZ", "LS-3DRG");
    QVERIFY(fixtureDef != NULL);
    fixtureMode = fixtureDef->modes().at(0);
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    QCOMPARE(fxi.typeString(), fixtureDef->typeToString(fixtureDef->type()));
    QCOMPARE(fxi.type(), QLCFixtureDef::Laser);
    QCOMPARE(fxi.iconResource(), QString(":/laser.png"));

    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("GLP", "PocketScan");
    QVERIFY(fixtureDef != NULL);
    fixtureMode = fixtureDef->modes().at(0);
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    QCOMPARE(fxi.typeString(), fixtureDef->typeToString(fixtureDef->type()));
    QCOMPARE(fxi.type(), QLCFixtureDef::Scanner);
    QCOMPARE(fxi.iconResource(), QString(":/scanner.png"));

    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Robe", "Fog 1500 FT");
    QVERIFY(fixtureDef != NULL);
    fixtureMode = fixtureDef->modes().at(0);
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    QCOMPARE(fxi.typeString(), fixtureDef->typeToString(fixtureDef->type()));
    QCOMPARE(fxi.type(), QLCFixtureDef::Smoke);
    QCOMPARE(fxi.iconResource(true), QString("qrc:/smoke.svg"));

    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Chauvet", "LED Shadow");
    QVERIFY(fixtureDef != NULL);
    fixtureMode = fixtureDef->modes().at(0);
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    QCOMPARE(fxi.typeString(), fixtureDef->typeToString(fixtureDef->type()));
    QCOMPARE(fxi.type(), QLCFixtureDef::Strobe);
    QCOMPARE(fxi.iconResource(), QString(":/strobe.png"));

    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Varytec", "Gigabar II");
    QVERIFY(fixtureDef != NULL);
    fixtureMode = fixtureDef->modes().at(0);
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    QCOMPARE(fxi.typeString(), fixtureDef->typeToString(fixtureDef->type()));
    QCOMPARE(fxi.type(), QLCFixtureDef::LEDBarPixels);
    QCOMPARE(fxi.iconResource(), QString(":/ledbar_pixels.png"));

    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Clay Paky", "Show Batten 100");
    QVERIFY(fixtureDef != NULL);
    fixtureMode = fixtureDef->modes().at(0);
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    QCOMPARE(fxi.typeString(), fixtureDef->typeToString(fixtureDef->type()));
    QCOMPARE(fxi.type(), QLCFixtureDef::LEDBarBeams);
    QCOMPARE(fxi.iconResource(true), QString("qrc:/ledbar_beams.svg"));
}

void Fixture_Test::dimmer()
{
    Fixture fxi(this);

    QVERIFY(fxi.fixtureDef() == NULL);
    QVERIFY(fxi.fixtureMode() == NULL);
    QVERIFY(fxi.channels() == 0);
    QVERIFY(fxi.channel(0) == NULL);
    QVERIFY(fxi.channel(42) == NULL);

    /* All channels point to the same generic channel instance */
    fxi.setChannels(5);
    QVERIFY(fxi.channels() == 5);
    QVERIFY(fxi.channel(0) != NULL);
    const QLCChannel* ch = fxi.channel(0);
    QVERIFY(fxi.channel(1) != fxi.channel(0));
    QVERIFY(fxi.channel(2) != fxi.channel(1));
    QVERIFY(fxi.channel(3) != fxi.channel(2));
    QVERIFY(fxi.channel(4) != fxi.channel(3));
    QVERIFY(fxi.channel(5) == NULL);
    QVERIFY(fxi.channel(42) == NULL);
    QVERIFY(fxi.channel(QLCChannel::invalid()) == NULL);

    QVERIFY(ch->capabilities().count() == 1);
    QVERIFY(ch->capabilities().at(0)->min() == 0);
    QVERIFY(ch->capabilities().at(0)->max() == UCHAR_MAX);
    QVERIFY(ch->capabilities().at(0)->name() == "Intensity");

    /* Although the dimmer fixture HAS a channel with this name, it is
       not returned, because all channels have the same name. */
    QVERIFY(fxi.channel(QLCChannel::Intensity) == 0);
}

void Fixture_Test::rgbPanel()
{
    Fixture fxi(this);
    fxi.setName("RGB Panel");
    QLCFixtureDef *rowDef = fxi.genericRGBPanelDef(10, Fixture::RGBW, false);
    QLCFixtureMode *rowMode = fxi.genericRGBPanelMode(rowDef, Fixture::RGBW, false, 1000, 100);
    fxi.setFixtureDefinition(rowDef, rowMode);

    QVERIFY(fxi.channels() == 40);

    QVERIFY(fxi.channel(0)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(0)->colour() == QLCChannel::Red);
    QVERIFY(fxi.channel(0)->name() == "Red 1");
    QVERIFY(fxi.channel(1)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(1)->colour() == QLCChannel::Green);
    QVERIFY(fxi.channel(1)->name() == "Green 1");
    QVERIFY(fxi.channel(2)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(2)->colour() == QLCChannel::Blue);
    QVERIFY(fxi.channel(2)->name() == "Blue 1");
    QVERIFY(fxi.channel(3)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(3)->colour() == QLCChannel::White);
    QVERIFY(fxi.channel(3)->name() == "White 1");

    QVERIFY(fxi.fixtureMode()->name() == "RGBW");
    QVERIFY(fxi.fixtureMode()->channels().count() == 40);
    QVERIFY(fxi.fixtureMode()->physical().width() == 1000);
    QVERIFY(fxi.fixtureMode()->physical().height() == 100);
    QVERIFY(fxi.fixtureMode()->physical().depth() == 100);
    QVERIFY(fxi.fixtureMode()->heads().count() == 10);

    rowDef = fxi.genericRGBPanelDef(10, Fixture::RGB, false);
    rowMode = fxi.genericRGBPanelMode(rowDef, Fixture::RGB, false, 1000, 100);
    fxi.setFixtureDefinition(rowDef, rowMode);

    QVERIFY(fxi.channels() == 30);
    QVERIFY(fxi.fixtureMode()->name() == "RGB");
    QVERIFY(fxi.fixtureMode()->channels().count() == 30);

    QVERIFY(fxi.channel(0)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(0)->colour() == QLCChannel::Red);
    QVERIFY(fxi.channel(0)->name() == "Red 1");
    QVERIFY(fxi.channel(1)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(1)->colour() == QLCChannel::Green);
    QVERIFY(fxi.channel(1)->name() == "Green 1");
    QVERIFY(fxi.channel(2)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(2)->colour() == QLCChannel::Blue);
    QVERIFY(fxi.channel(2)->name() == "Blue 1");

    rowDef = fxi.genericRGBPanelDef(10, Fixture::RBG, false);
    rowMode = fxi.genericRGBPanelMode(rowDef, Fixture::RBG, false, 1000, 100);
    fxi.setFixtureDefinition(rowDef, rowMode);

    QVERIFY(fxi.channels() == 30);
    QVERIFY(fxi.fixtureMode()->name() == "RBG");
    QVERIFY(fxi.fixtureMode()->channels().count() == 30);

    QVERIFY(fxi.channel(0)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(0)->colour() == QLCChannel::Red);
    QVERIFY(fxi.channel(0)->name() == "Red 1");
    QVERIFY(fxi.channel(1)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(1)->colour() == QLCChannel::Blue);
    QVERIFY(fxi.channel(1)->name() == "Blue 1");
    QVERIFY(fxi.channel(2)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(2)->colour() == QLCChannel::Green);
    QVERIFY(fxi.channel(2)->name() == "Green 1");

    rowDef = fxi.genericRGBPanelDef(10, Fixture::BGR, false);
    rowMode = fxi.genericRGBPanelMode(rowDef, Fixture::BGR, false, 1000, 100);
    fxi.setFixtureDefinition(rowDef, rowMode);

    QVERIFY(fxi.channels() == 30);
    QVERIFY(fxi.fixtureMode()->name() == "BGR");
    QVERIFY(fxi.fixtureMode()->channels().count() == 30);

    QVERIFY(fxi.channel(0)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(0)->colour() == QLCChannel::Blue);
    QVERIFY(fxi.channel(0)->name() == "Blue 1");
    QVERIFY(fxi.channel(1)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(1)->colour() == QLCChannel::Green);
    QVERIFY(fxi.channel(1)->name() == "Green 1");
    QVERIFY(fxi.channel(2)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(2)->colour() == QLCChannel::Red);
    QVERIFY(fxi.channel(2)->name() == "Red 1");

    rowDef = fxi.genericRGBPanelDef(10, Fixture::BRG, false);
    rowMode = fxi.genericRGBPanelMode(rowDef, Fixture::BRG, false, 1000, 100);
    fxi.setFixtureDefinition(rowDef, rowMode);

    QVERIFY(fxi.channels() == 30);
    QVERIFY(fxi.fixtureMode()->name() == "BRG");
    QVERIFY(fxi.fixtureMode()->channels().count() == 30);

    QVERIFY(fxi.channel(0)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(0)->colour() == QLCChannel::Blue);
    QVERIFY(fxi.channel(0)->name() == "Blue 1");
    QVERIFY(fxi.channel(1)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(1)->colour() == QLCChannel::Red);
    QVERIFY(fxi.channel(1)->name() == "Red 1");
    QVERIFY(fxi.channel(2)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(2)->colour() == QLCChannel::Green);
    QVERIFY(fxi.channel(2)->name() == "Green 1");

    rowDef = fxi.genericRGBPanelDef(10, Fixture::GBR, false);
    rowMode = fxi.genericRGBPanelMode(rowDef, Fixture::GBR, false, 1000, 100);
    fxi.setFixtureDefinition(rowDef, rowMode);

    QVERIFY(fxi.channels() == 30);
    QVERIFY(fxi.fixtureMode()->name() == "GBR");
    QVERIFY(fxi.fixtureMode()->channels().count() == 30);

    QVERIFY(fxi.channel(0)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(0)->colour() == QLCChannel::Green);
    QVERIFY(fxi.channel(0)->name() == "Green 1");
    QVERIFY(fxi.channel(1)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(1)->colour() == QLCChannel::Blue);
    QVERIFY(fxi.channel(1)->name() == "Blue 1");
    QVERIFY(fxi.channel(2)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(2)->colour() == QLCChannel::Red);
    QVERIFY(fxi.channel(2)->name() == "Red 1");

    rowDef = fxi.genericRGBPanelDef(10, Fixture::GRB, false);
    rowMode = fxi.genericRGBPanelMode(rowDef, Fixture::GRB, false, 1000, 100);
    fxi.setFixtureDefinition(rowDef, rowMode);

    QVERIFY(fxi.channels() == 30);
    QVERIFY(fxi.fixtureMode()->name() == "GRB");
    QVERIFY(fxi.fixtureMode()->channels().count() == 30);

    QVERIFY(fxi.channel(0)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(0)->colour() == QLCChannel::Green);
    QVERIFY(fxi.channel(0)->name() == "Green 1");
    QVERIFY(fxi.channel(1)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(1)->colour() == QLCChannel::Red);
    QVERIFY(fxi.channel(1)->name() == "Red 1");
    QVERIFY(fxi.channel(2)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(2)->colour() == QLCChannel::Blue);
    QVERIFY(fxi.channel(2)->name() == "Blue 1");
}

void Fixture_Test::rgbPanel16bit()
{
    Fixture fxi(this);
    fxi.setName("RGB Panel 16bit");
    QLCFixtureDef *rowDef = fxi.genericRGBPanelDef(10, Fixture::RGB, true);
    QLCFixtureMode *rowMode = fxi.genericRGBPanelMode(rowDef, Fixture::RGB, true, 1000, 100);
    fxi.setFixtureDefinition(rowDef, rowMode);

    QVERIFY(fxi.channels() == 60);
    QVERIFY(fxi.fixtureMode()->name() == "RGB 16bit");
    QVERIFY(fxi.fixtureMode()->heads().at(0).channels().count() == 6);

    QVERIFY(fxi.channel(0)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(0)->colour() == QLCChannel::Red);
    QVERIFY(fxi.channel(0)->name() == "Red 1");
    QVERIFY(fxi.channel(0)->controlByte() == QLCChannel::MSB);

    QVERIFY(fxi.channel(1)->group() == QLCChannel::Intensity);
    QVERIFY(fxi.channel(1)->colour() == QLCChannel::Red);
    QVERIFY(fxi.channel(1)->name() == "Red Fine 1");
    QVERIFY(fxi.channel(1)->controlByte() == QLCChannel::LSB);
}

void Fixture_Test::fixtureDef()
{
    Fixture fxi(this);

    QVERIFY(fxi.fixtureDef() == NULL);
    QVERIFY(fxi.fixtureMode() == NULL);
    QVERIFY(fxi.channels() == 0);
    QVERIFY(fxi.channel(0) == NULL);
    QCOMPARE(fxi.channelNumber(QLCChannel::Pan, QLCChannel::MSB), QLCChannel::invalid());
    QCOMPARE(fxi.channelNumber(QLCChannel::Tilt, QLCChannel::MSB), QLCChannel::invalid());
    QCOMPARE(fxi.channelNumber(QLCChannel::Pan, QLCChannel::LSB), QLCChannel::invalid());
    QCOMPARE(fxi.channelNumber(QLCChannel::Tilt, QLCChannel::LSB), QLCChannel::invalid());
    QCOMPARE(fxi.masterIntensityChannel(), QLCChannel::invalid());

    QLCFixtureDef* fixtureDef;
    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Martin", "MAC300");
    Q_ASSERT(fixtureDef != NULL);

    fxi.setFixtureDefinition(fixtureDef, NULL);
    QVERIFY(fxi.fixtureDef() == NULL);
    QVERIFY(fxi.fixtureMode() == NULL);

    QLCFixtureMode* fixtureMode;
    fixtureMode = fixtureDef->modes().last();
    Q_ASSERT(fixtureMode != NULL);

    fxi.setFixtureDefinition(NULL, fixtureMode);
    QVERIFY(fxi.fixtureDef() == NULL);
    QVERIFY(fxi.fixtureMode() == NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    QVERIFY(fxi.fixtureDef() != NULL);
    QVERIFY(fxi.fixtureMode() != NULL);
    QVERIFY(fxi.fixtureDef() == fixtureDef);
    QVERIFY(fxi.fixtureMode() == fixtureMode);

    QVERIFY(fxi.channels() == quint32(fixtureMode->channels().count()));
    QVERIFY(fxi.channel(fxi.channels() - 1) != NULL);
    QVERIFY(fxi.channel(fxi.channels()) == NULL);

    QVERIFY(fxi.channel(QLCChannel::Pan) != QLCChannel::invalid());
    const QLCChannel* ch = fxi.channel(fxi.channel(QLCChannel::Pan));
    QVERIFY(ch != NULL);

    QCOMPARE(fxi.channelNumber(QLCChannel::Pan, QLCChannel::MSB), quint32(7));
    QCOMPARE(fxi.channelNumber(QLCChannel::Tilt, QLCChannel::MSB), quint32(9));
    QCOMPARE(fxi.channelNumber(QLCChannel::Pan, QLCChannel::LSB), quint32(8));
    QCOMPARE(fxi.channelNumber(QLCChannel::Tilt, QLCChannel::LSB), quint32(10));
    QCOMPARE(fxi.masterIntensityChannel(), quint32(1));
    QCOMPARE(fxi.rgbChannels(), QVector <quint32> ());
    QCOMPARE(fxi.cmyChannels(), QVector <quint32> () << 2 << 3 << 4);
}

void Fixture_Test::channels()
{
    Fixture fxi(this);
    QLCFixtureDef* fixtureDef = m_doc->fixtureDefCache()->fixtureDef("i-Pix", "BB4");
    QVERIFY(fixtureDef != NULL);
    QLCFixtureMode* fixtureMode = fixtureDef->modes().last();
    QVERIFY(fixtureMode != NULL);
    fxi.setFixtureDefinition(fixtureDef, fixtureMode);

    QCOMPARE(fxi.channel(QLCChannel::Intensity, QLCChannel::Red), quint32(3));

    QSet <quint32> chs;
    chs << 3 << 4 << 21 << 22 << 12 << 13 << 30 << 31;
    QCOMPARE(chs, fxi.channels(QLCChannel::Intensity, QLCChannel::Red));
    chs.clear();
    chs << 5 << 6 << 23 << 24 << 14 << 15 << 32 << 33;
    QCOMPARE(chs, fxi.channels(QLCChannel::Intensity, QLCChannel::Green));
    chs.clear();
    chs << 7 << 8 << 16 << 17 << 25 << 26 << 34 << 35;
    QCOMPARE(chs, fxi.channels(QLCChannel::Intensity, QLCChannel::Blue));
    chs.clear();
    QCOMPARE(chs, fxi.channels(QLCChannel::Colour, QLCChannel::Blue));
}

void Fixture_Test::degrees()
{
    Fixture fxi(this);

    QLCFixtureDef* fixtureDef;
    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Martin", "MAC250+");
    QVERIFY(fixtureDef != NULL);

    QLCFixtureMode* fixtureMode;
    fixtureMode = fixtureDef->modes().at(1);
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);

    QCOMPARE(fxi.degreesRange(0).width(), 540.0);
    QCOMPARE(fxi.degreesRange(0).height(), 270.0);

    // dummy ID just for testing
    fxi.setID(99);

    QList<SceneValue> pos = fxi.positionToValues(QLCChannel::Pan, 90);
    QCOMPARE(pos.count(), 2);
    // verify coarse Pan
    QCOMPARE(pos.at(0).fxi, quint32(99));
    QCOMPARE(pos.at(0).channel, quint32(7));
    QCOMPARE(pos.at(0).value, uchar(42));
    // verify fine Pan
    QCOMPARE(pos.at(1).fxi, quint32(99));
    QCOMPARE(pos.at(1).channel, quint32(8));
    QCOMPARE(pos.at(1).value, uchar(170));

    pos = fxi.positionToValues(QLCChannel::Tilt, 45);
    QCOMPARE(pos.count(), 2);
    // verify coarse Tilt
    QCOMPARE(pos.at(0).fxi, quint32(99));
    QCOMPARE(pos.at(0).channel, quint32(9));
    QCOMPARE(pos.at(0).value, uchar(42));
    // verify fine Tilt
    QCOMPARE(pos.at(1).fxi, quint32(99));
    QCOMPARE(pos.at(1).channel, quint32(10));
    QCOMPARE(pos.at(1).value, uchar(170));
}

void Fixture_Test::indexedPosition()
{
    // The Junman 2 Head maps only DMX 0-127 of its Pan channel to absolute
    // positioning (RotationIndexed) — one full 360-degree CCW sweep; 128-255 is
    // continuous rotation. With PanMax=360, position math must spread 0..360
    // degrees across MSB 0..127, never spilling into the rotation band.
    Fixture fxi(this);

    QLCFixtureDef* fixtureDef;
    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Junman", "2 Head");
    QVERIFY(fixtureDef != NULL);

    QLCFixtureMode* fixtureMode = fixtureDef->modes().first();
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    fxi.setID(7);

    // Helper: pull the MSB value written for the first head's Pan (channel 0).
    // Band is [0, 0x7FFF] => MSB spans 0..127.

    // 0 degrees -> bottom of band
    QList<SceneValue> pos = fxi.positionToValues(QLCChannel::Pan, 0);
    QCOMPARE(pos.at(0).channel, quint32(0));
    QCOMPARE(pos.at(0).value, uchar(0));

    // Full 360 sweep (== PanMax) -> top of the indexed band, MSB 127 (NOT 255)
    pos = fxi.positionToValues(QLCChannel::Pan, 360);
    QCOMPARE(pos.at(0).channel, quint32(0));
    QCOMPARE(pos.at(0).value, uchar(127));
    QCOMPARE(pos.at(1).value, uchar(255));   // fine channel

    // Half sweep (180) -> roughly mid-band: 0.5 * 0x7FFF = 0x3FFF -> MSB 63
    pos = fxi.positionToValues(QLCChannel::Pan, 180);
    QCOMPARE(pos.at(0).channel, quint32(0));
    QCOMPARE(pos.at(0).value, uchar(63));

    // Over-range angle is CLAMPED to the band, never entering the spin zone
    pos = fxi.positionToValues(QLCChannel::Pan, 720);
    QCOMPARE(pos.at(0).value, uchar(127));
    QVERIFY(pos.at(0).value < uchar(128));
}

void Fixture_Test::heads()
{
    Fixture fxi(this);

    QLCFixtureDef* fixtureDef;
    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Equinox", "Photon");
    QVERIFY(fixtureDef != NULL);

    QLCFixtureMode* fixtureMode;
    fixtureMode = fixtureDef->modes().last();
    QVERIFY(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);

    QCOMPARE(fxi.heads(), 6);

    QLCFixtureHead head = fxi.head(0);
    QCOMPARE(head.channels().count(), 4);
    head = fxi.head(1);
    QCOMPARE(head.channels().count(), 4);
    head = fxi.head(2);
    QCOMPARE(head.channels().count(), 4);
    head = fxi.head(3);
    QCOMPARE(head.channels().count(), 4);
    head = fxi.head(4);
    QCOMPARE(head.channels().count(), 4);
    head = fxi.head(5);
    QCOMPARE(head.channels().count(), 4);
}

void Fixture_Test::loadWrongRoot()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Function");
    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    Fixture fxi(this);
    QVERIFY(fxi.loadXML(xmlReader, m_doc, m_doc->fixtureDefCache()) == false);
}

void Fixture_Test::loadFixtureDef()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Fixture");

    xmlWriter.writeTextElement("Channels", "9");
    xmlWriter.writeTextElement("Name", "Foobar");
    xmlWriter.writeTextElement("Universe", "0");
    xmlWriter.writeTextElement("Model", "MAC250+");
    xmlWriter.writeTextElement("Mode", "Mode 1");
    xmlWriter.writeTextElement("Manufacturer", "Martin");
    xmlWriter.writeTextElement("ID", "42");
    xmlWriter.writeTextElement("Address", "21");

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    Fixture fxi(this);
    QVERIFY(fxi.loadXML(xmlReader, m_doc, m_doc->fixtureDefCache()) == true);
    QVERIFY(fxi.name() == "Foobar");
    QVERIFY(fxi.channels() == 9);
    QVERIFY(fxi.address() == 21);
    QVERIFY(fxi.universe() == 0);
    QVERIFY(fxi.fixtureDef() != NULL);
    QVERIFY(fxi.fixtureMode() != NULL);
}

void Fixture_Test::loadFixtureDefWrongChannels()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Fixture");

    xmlWriter.writeTextElement("Channels", "15");
    xmlWriter.writeTextElement("Name", "Foobar");
    xmlWriter.writeTextElement("Universe", "0");
    xmlWriter.writeTextElement("Model", "MAC250+");
    xmlWriter.writeTextElement("Mode", "Mode 1");
    xmlWriter.writeTextElement("Manufacturer", "Martin");
    xmlWriter.writeTextElement("ID", "42");
    xmlWriter.writeTextElement("Address", "21");

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    Fixture fxi(this);
    QVERIFY(fxi.loadXML(xmlReader, m_doc, m_doc->fixtureDefCache()) == true);
    QVERIFY(fxi.name() == "Foobar");
    QVERIFY(fxi.channels() == 9);
    QVERIFY(fxi.address() == 21);
    QVERIFY(fxi.universe() == 0);
    QVERIFY(fxi.fixtureDef() != NULL);
    QVERIFY(fxi.fixtureMode() != NULL);
}

void Fixture_Test::loadDimmer()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Fixture");

    xmlWriter.writeTextElement("Channels", "18");
    xmlWriter.writeTextElement("Name", "Foobar");
    xmlWriter.writeTextElement("Universe", "3");
    xmlWriter.writeTextElement("Model", "Foobar");
    xmlWriter.writeTextElement("Mode", "Foobar");
    xmlWriter.writeTextElement("Manufacturer", "Foobar");
    xmlWriter.writeTextElement("ID", "42");
    xmlWriter.writeTextElement("Address", "21");

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    Fixture fxi(this);
    QVERIFY(fxi.loadXML(xmlReader, m_doc, m_doc->fixtureDefCache()) == true);
    QVERIFY(fxi.name() == "Foobar");
    QVERIFY(fxi.channels() == 18);
    QVERIFY(fxi.address() == 21);
    QVERIFY(fxi.universe() == 3);
    QVERIFY(fxi.fixtureDef() != NULL);
    QVERIFY(fxi.fixtureMode() != NULL);
}

void Fixture_Test::loadWrongAddress()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Fixture");

    xmlWriter.writeTextElement("Channels", "18");
    xmlWriter.writeTextElement("Name", "Foobar");
    xmlWriter.writeTextElement("Universe", "0");
    xmlWriter.writeTextElement("Model", "Foobar");
    xmlWriter.writeTextElement("Mode", "Foobar");
    xmlWriter.writeTextElement("Manufacturer", "Foobar");
    xmlWriter.writeTextElement("ID", "42");
    xmlWriter.writeTextElement("Address", "512");

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    Fixture fxi(this);
    QVERIFY(fxi.loadXML(xmlReader, m_doc, m_doc->fixtureDefCache()) == true);
    QVERIFY(fxi.name() == "Foobar");
    QVERIFY(fxi.channels() == 18);
    QVERIFY(fxi.address() == 0);
    QVERIFY(fxi.universe() == 0);
}

void Fixture_Test::loadWrongUniverse()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Fixture");

    xmlWriter.writeTextElement("Channels", "18");
    xmlWriter.writeTextElement("Name", "Foobar");
    xmlWriter.writeTextElement("Universe", "4");
    xmlWriter.writeTextElement("Model", "Foobar");
    xmlWriter.writeTextElement("Mode", "Foobar");
    xmlWriter.writeTextElement("Manufacturer", "Foobar");
    xmlWriter.writeTextElement("ID", "42");
    xmlWriter.writeTextElement("Address", "25");

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    Fixture fxi(this);
    QVERIFY(fxi.loadXML(xmlReader, m_doc, m_doc->fixtureDefCache()) == true);
    QVERIFY(fxi.name() == "Foobar");
    QVERIFY(fxi.channels() == 18);
    QVERIFY(fxi.address() == 25);
    QVERIFY(fxi.universe() == 4);
}

/* N1 (RC6): a fixture claiming more channels than a universe holds (the
   fuzzer's 63-byte show) kept them all with its address reset to 0, and
   adding it wrote the defaults past the universe's 512-byte buffers. It
   loads unpatched, says so, and the universe is untouched. */
void Fixture_Test::loadMoreChannelsThanAUniverse()
{
    QByteArray xml("<Fixture><ID></ID><Channels>926</Channels></Fixture>");
    QXmlStreamReader xmlReader(xml);
    xmlReader.readNextStartElement();

    QVERIFY(m_doc->fixtures().size() == 0);
    QVERIFY(Fixture::loader(xmlReader, m_doc) == true);
    QCOMPARE(m_doc->fixtures().size(), 1);

    Fixture *fxi = m_doc->fixtures().first();
    QCOMPARE(fxi->channels(), quint32(926));
    QVERIFY(fxi->isPatched() == false);
    QCOMPARE(fxi->address(), quint32(0));
    QVERIFY(m_doc->errorLog().contains("926 channels"));

    QList<Universe *> unis = m_doc->inputOutputMap()->claimUniverses();
    QVERIFY(unis.size() > 0);
    QVERIFY(unis.at(0)->usedChannels() <= 512);
    QVERIFY(unis.at(0)->totalChannels() <= 512);
    m_doc->inputOutputMap()->releaseUniverses(false);

    /* Nor can it be patched afterwards: no address holds it. */
    QVERIFY(m_doc->patchFixture(fxi->id(), 0, 0) == false);
    QVERIFY(fxi->isPatched() == false);

    /* A fixture that fits but at the wrong address still moves to 1, patched. */
    QByteArray fits("<Fixture><ID>5</ID><Channels>512</Channels><Address>3</Address></Fixture>");
    QXmlStreamReader fitsReader(fits);
    fitsReader.readNextStartElement();
    QVERIFY(Fixture::loader(fitsReader, m_doc) == true);
    Fixture *whole = m_doc->fixture(5);
    QVERIFY(whole != NULL);
    QVERIFY(whole->isPatched());
    QCOMPARE(whole->address(), quint32(0));
    QCOMPARE(whole->channels(), quint32(512));

    QVERIFY(m_doc->deleteFixture(5));
    QVERIFY(m_doc->deleteFixture(fxi->id()));
    QCOMPARE(m_doc->fixtures().size(), 0);
}

void Fixture_Test::loadWrongID()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Fixture");

    xmlWriter.writeTextElement("Channels", "9");
    xmlWriter.writeTextElement("Name", "Foobar");
    xmlWriter.writeTextElement("Universe", "0");
    xmlWriter.writeTextElement("Model", "MAC250+");
    xmlWriter.writeTextElement("Mode", "Mode 1");
    xmlWriter.writeTextElement("Manufacturer", "Martin");
    xmlWriter.writeTextElement("ID", QString::number(Fixture::invalidId()));
    xmlWriter.writeTextElement("Address", "21");

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    Fixture fxi(this);
    QVERIFY(fxi.loadXML(xmlReader, m_doc, m_doc->fixtureDefCache()) == false);
}

void Fixture_Test::loader()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Fixture");

    xmlWriter.writeTextElement("Channels", "18");
    xmlWriter.writeTextElement("Name", "Foobar");
    xmlWriter.writeTextElement("Universe", "3");
    xmlWriter.writeTextElement("Model", "Foobar");
    xmlWriter.writeTextElement("Mode", "Foobar");
    xmlWriter.writeTextElement("Manufacturer", "Foobar");
    xmlWriter.writeTextElement("ID", "42");
    xmlWriter.writeTextElement("Address", "21");

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    QVERIFY(m_doc != NULL);
    QVERIFY(m_doc->fixtures().size() == 0);

    QVERIFY(Fixture::loader(xmlReader, m_doc) == true);
    QVERIFY(m_doc->fixtures().size() == 1);
    QVERIFY(m_doc->fixture(0) == NULL); // No ID auto-assignment

    Fixture* fxi = m_doc->fixture(42);
    QVERIFY(fxi != NULL);
    QVERIFY(fxi->name() == "Foobar");
    QVERIFY(fxi->channels() == 18);
    QVERIFY(fxi->address() == 21);
    QVERIFY(fxi->universe() == 3);
    QVERIFY(fxi->fixtureDef() != NULL);
    QVERIFY(fxi->fixtureMode() != NULL);
}

void Fixture_Test::save()
{
    QLCFixtureDef* fixtureDef;
    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Martin", "MAC250+");
    Q_ASSERT(fixtureDef != NULL);

    QLCFixtureMode* fixtureMode;
    fixtureMode = fixtureDef->modes().at(0);
    Q_ASSERT(fixtureMode != NULL);

    Fixture fxi(this);
    fxi.setID(1337);
    fxi.setName("Test Fixture");
    fxi.setUniverse(2);
    fxi.setAddress(438);
    fxi.setFixtureDefinition(fixtureDef, fixtureMode);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("TestRoot");

    QVERIFY(fxi.saveXML(&xmlWriter) == true);

    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();
    QVERIFY(xmlReader.name().toString() == "TestRoot");
    xmlReader.readNextStartElement();
    QVERIFY(xmlReader.name().toString() == "Fixture");

    bool manufacturer = false, model = false, mode = false, name = false,
                                       channels = false, universe = false, address = false, id = false;

    while (xmlReader.readNextStartElement())
    {
        if (xmlReader.name().toString() == "Manufacturer")
        {
            QVERIFY(xmlReader.readElementText() == "Martin");
            manufacturer = true;
        }
        else if (xmlReader.name().toString() == "Model")
        {
            QVERIFY(xmlReader.readElementText() == "MAC250+");
            model = true;
        }
        else if (xmlReader.name().toString() == "Mode")
        {
            QVERIFY(xmlReader.readElementText() == fixtureMode->name());
            mode = true;
        }
        else if (xmlReader.name().toString() == "ID")
        {
            QVERIFY(xmlReader.readElementText() == "1337");
            id = true;
        }
        else if (xmlReader.name().toString() == "Name")
        {
            QVERIFY(xmlReader.readElementText() == "Test Fixture");
            name = true;
        }
        else if (xmlReader.name().toString() == "Universe")
        {
            QVERIFY(xmlReader.readElementText() == "2");
            universe = true;
        }
        else if (xmlReader.name().toString() == "Address")
        {
            QVERIFY(xmlReader.readElementText() == "438");
            address = true;
        }
        else if (xmlReader.name().toString() == "Channels")
        {
            QVERIFY(xmlReader.readElementText().toInt()
                    == fixtureMode->channels().count());
            channels = true;
        }
        else
        {
            QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString())
                  .toLatin1());
            xmlReader.skipCurrentElement();
        }
    }

    QVERIFY(manufacturer == true);
    QVERIFY(model == true);
    QVERIFY(mode == true);
    QVERIFY(id == true);
    QVERIFY(name == true);
    QVERIFY(universe == true);
    QVERIFY(address == true);
    QVERIFY(channels == true);
}

/*void Fixture_Test::status()
{
    // This test is mostly just a stability check since checking lots of
    // detailed HTML formatting is not that useful.
    QString info;

    Fixture fxi(this);
    info = fxi.status();

    fxi.setID(1337);
    info = fxi.status();

    fxi.setName("Test Fixture");
    info = fxi.status();

    fxi.setUniverse(2);
    info = fxi.status();

    fxi.setAddress(438);
    info = fxi.status();

    fxi.setChannels(12);
    info = fxi.status();

    QLCFixtureDef* fixtureDef;
    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Martin", "MAC250+");
    Q_ASSERT(fixtureDef != NULL);

    QLCFixtureMode* fixtureMode;
    fixtureMode = fixtureDef->modes().at(0);
    Q_ASSERT(fixtureMode != NULL);

    fxi.setFixtureDefinition(fixtureDef, fixtureMode);
    info = fxi.status();
}*/

void Fixture_Test::aMissingDefinitionIsKeptNotLostOnSave()
{
    // A show naming a fixture whose definition is nowhere here.
    const QString xml = QStringLiteral(
        "<Fixture><Manufacturer>Warmdance</Manufacturer><Model>XL-450</Model>"
        "<Mode>16 Channel</Mode><ID>42</ID><Name>Mover SL</Name><Universe>0</Universe>"
        "<Address>100</Address><Channels>16</Channels></Fixture>");
    QXmlStreamReader in(xml);
    in.readNextStartElement();
    QVERIFY(Fixture::loader(in, m_doc));
    Fixture *fx = m_doc->fixture(42);
    QVERIFY(fx != nullptr);
    QVERIFY(fx->definitionUnavailable());
    QCOMPARE(fx->unavailableManufacturer(), QString("Warmdance"));
    QCOMPARE(fx->unavailableModel(), QString("XL-450"));
    QCOMPARE(fx->unavailableMode(), QString("16 Channel"));
    QCOMPARE(fx->channels(), 16u);                  // standing in, the same size

    // Saved: as what it really is, never "Generic".
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    QVERIFY(fx->saveXML(&w));
    buf.close();
    const QByteArray out = buf.data();
    QVERIFY2(out.contains("<Manufacturer>Warmdance</Manufacturer>"), out.constData());
    QVERIFY(out.contains("<Model>XL-450</Model>"));
    QVERIFY(out.contains("<Mode>16 Channel</Mode>"));
    QVERIFY(!out.contains("Generic"));

    // Given a real definition: no longer standing in.
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef("Futurelight", "DJScan250");
    QVERIFY(def != nullptr);
    fx->setFixtureDefinition(def, def->modes().first());
    QVERIFY(!fx->definitionUnavailable());
    m_doc->deleteFixture(42);

    // A plain generic dimmer was never standing in for anything.
    QXmlStreamReader g(QStringLiteral("<Fixture><Manufacturer>Generic</Manufacturer><Model>Generic</Model>"
                                      "<Mode>Generic</Mode><ID>43</ID><Name>Dim</Name><Universe>0</Universe>"
                                      "<Address>300</Address><Channels>2</Channels></Fixture>"));
    g.readNextStartElement();
    QVERIFY(Fixture::loader(g, m_doc));
    QVERIFY(!m_doc->fixture(43)->definitionUnavailable());
    m_doc->deleteFixture(43);
}

void Fixture_Test::theShowCacheKeepsDefinitionsOnThisComputer()
{
    // Somewhere that is not this machine's real home.
    QTemporaryDir home;
    const QByteArray realHome = qgetenv("HOME");
    qputenv("HOME", home.path().toUtf8());
#ifdef Q_OS_WIN
    // Windows takes the home folder from USERPROFILE, not HOME.
    const QByteArray realProfile = qgetenv("USERPROFILE");
    qputenv("USERPROFILE", home.path().toUtf8());
#endif

    const QDir cache = QLCFixtureDefCache::showCacheDirectory();
    QVERIFY2(cache.absolutePath().startsWith(QFileInfo(home.path()).canonicalFilePath())
             || cache.absolutePath().startsWith(home.path()), qPrintable(cache.absolutePath()));
    QVERIFY(cache.absolutePath().endsWith("ShowCache/Fixtures"));

    // A user's own definition, loaded from where it lives.
    QTemporaryDir lib;
    QLCFixtureDef *src = m_doc->fixtureDefCache()->fixtureDef("Futurelight", "DJScan250");
    QVERIFY(src != nullptr);
    // Its file, renamed as one of the user's own (a copy of the definition
    // object would share -- and twice free -- its channels).
    QFile shipped(src->definitionSourceFile());
    QVERIFY(shipped.open(QIODevice::ReadOnly));
    QString text = QString::fromUtf8(shipped.readAll());
    text.replace(QRegularExpression("<Manufacturer>[^<]*</Manufacturer>"), "<Manufacturer>Branson</Manufacturer>");
    text.replace(QRegularExpression("<Model>[^<]*</Model>"), "<Model>Laser Moving Head</Model>");
    const QString path = lib.filePath("Branson-Laser-Moving-Head.qxf");
    QFile own(path);
    QVERIFY(own.open(QIODevice::WriteOnly));
    own.write(text.toUtf8());
    own.close();

    QLCFixtureDefCache defs;
    QVERIFY(defs.loadQXF(path, true));
    QLCFixtureDef *loaded = defs.fixtureDef("Branson", "Laser Moving Head");
    QVERIFY(loaded != nullptr);
    QVERIFY(defs.cacheForShows(loaded));
    const QString cached = QLCFixtureDefCache::showCacheFile("Branson", "Laser Moving Head");
    QVERIFY(QFile::exists(cached));
    QVERIFY(cached.startsWith(cache.absolutePath()));

    // Gone from where it lived: the cache still has it, and a new library
    // finds it there.
    QFile::remove(path);
    QLCFixtureDefCache fresh;
    QVERIFY(fresh.fixtureDef("Branson", "Laser Moving Head") == nullptr);
    QVERIFY(fresh.loadFromShowCache("Branson", "Laser Moving Head"));
    QVERIFY(fresh.fixtureDef("Branson", "Laser Moving Head") != nullptr);
    QVERIFY(!fresh.loadFromShowCache("Nobody", "Nothing"));

    // A shipped definition is not copied: it ships with the app.
    QVERIFY(!m_doc->fixtureDefCache()->cacheForShows(src));

    qputenv("HOME", realHome);
#ifdef Q_OS_WIN

    qputenv("USERPROFILE", realProfile);
#endif
}

/*****************************************************************************
 * Stage look
 *****************************************************************************/

void Fixture_Test::stageLookDefaultAndSet()
{
    Fixture fxi(this);
    fxi.setID(7);
    QCOMPARE(fxi.stageLook(), Fixture::AutoLook);

    QSignalSpy spy(&fxi, SIGNAL(changed(quint32)));
    fxi.setStageLook(Fixture::StaffLook);
    QCOMPARE(fxi.stageLook(), Fixture::StaffLook);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toUInt(), quint32(7));

    // The same again says nothing.
    fxi.setStageLook(Fixture::StaffLook);
    QCOMPARE(spy.count(), 1);

    // Out of range is Auto, not a garbage look.
    // One past the last look: a value a StageLook can still hold (999 isn't).
    fxi.setStageLook(Fixture::StageLook(int(Fixture::CurtainLook) + 1));
    QCOMPARE(fxi.stageLook(), Fixture::AutoLook);
    QCOMPARE(spy.count(), 2);
}

void Fixture_Test::stageLookKeys()
{
    const QList<Fixture::StageLook> looks = Fixture::stageLooks();
    // Every value of the enum, once, Auto first.
    QCOMPARE(looks.first(), Fixture::AutoLook);
    QCOMPARE(looks.size(), int(Fixture::CurtainLook) + 1);
    QSet<int> seen;
    QSet<QString> keys;
    int section = -1;
    foreach (Fixture::StageLook look, looks)
    {
        QVERIFY(!seen.contains(int(look)));
        seen.insert(int(look));
        const QString key = Fixture::stageLookToString(look);
        QVERIFY(!keys.contains(key));
        keys.insert(key);
        // Round trip, any case.
        QCOMPARE(Fixture::stringToStageLook(key), look);
        QCOMPARE(Fixture::stringToStageLook(key.toUpper()), look);
        QVERIFY(!Fixture::stageLookName(look).isEmpty());
        // Listed section by section, never going back.
        const int sec = Fixture::stageLookSection(look);
        if (look == Fixture::AutoLook)
        {
            QCOMPARE(sec, -1);
            continue;
        }
        QVERIFY(sec >= section);
        section = sec;
    }
    // The keys a saved show holds: these must never change.
    QCOMPARE(Fixture::stageLookToString(Fixture::FollowspotLook), QString("Followspot"));
    QCOMPARE(Fixture::stageLookToString(Fixture::MovingSquareWashLook), QString("MovingSquareWash"));
    QCOMPARE(Fixture::stageLookToString(Fixture::LedParLook), QString("LedPar"));
    QCOMPARE(Fixture::stageLookToString(Fixture::CycStripLook), QString("CycStrip"));
    QCOMPARE(Fixture::stageLookSection(Fixture::MovingSpotLook), int(Fixture::MovingSection));
    QCOMPARE(Fixture::stageLookSection(Fixture::FresnelLook), int(Fixture::ConventionalSection));
    // Practicals: lights that are part of the set.
    QCOMPARE(Fixture::stageLookSection(Fixture::LanternLook), int(Fixture::PracticalSection));
    QCOMPARE(Fixture::stageLookSection(Fixture::StaffLook), int(Fixture::PracticalSection));
    QCOMPARE(Fixture::stageLookSection(Fixture::StrobeLook), int(Fixture::LedSection));
    // Scenery, last: a curtain marked as one (Q51).
    QCOMPARE(looks.last(), Fixture::CurtainLook);
    QCOMPARE(looks.at(looks.size() - 2), Fixture::StaffLook);
    QCOMPARE(Fixture::stageLookToString(Fixture::CurtainLook), QString("Curtain"));
    QCOMPARE(Fixture::stringToStageLook("curtain"), Fixture::CurtainLook);
    QCOMPARE(Fixture::stageLookSection(Fixture::CurtainLook), int(Fixture::ScenerySection));
    QCOMPARE(Fixture::stageLookSectionName(Fixture::ScenerySection), QString("Scenery"));
    QCOMPARE(Fixture::stageLookName(Fixture::CurtainLook), QString("Curtain"));
    // Appended: the looks before it kept their values.
    QCOMPARE(int(Fixture::StrobeLook), 15);
    QCOMPARE(int(Fixture::CurtainLook), 16);
    // The key a saved show holds did not change with what a lantern is.
    QCOMPARE(Fixture::stageLookToString(Fixture::LanternLook), QString("Lantern"));
    QCOMPARE(Fixture::stageLookSectionName(Fixture::PracticalSection), QString("Practicals"));
}

static QByteArray stageLookSaved(Fixture &fxi)
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    xmlWriter.writeStartElement("TestRoot");
    fxi.saveXML(&xmlWriter);
    xmlWriter.writeEndElement();
    xmlWriter.setDevice(NULL);
    buffer.close();
    return buffer.data();
}

void Fixture_Test::stageLookSaveLoad()
{
    Fixture fxi(this);
    fxi.setID(5);
    fxi.setName("Spot 1");
    fxi.setAddress(10);
    fxi.setChannels(4);
    QLCFixtureDef *def = fxi.genericDimmerDef(4);
    fxi.setFixtureDefinition(def, fxi.genericDimmerMode(def, 4));

    // Auto writes nothing: a show without looks saves as it always has.
    QVERIFY(!stageLookSaved(fxi).contains("StageLook"));

    foreach (Fixture::StageLook look, Fixture::stageLooks())
    {
        if (look == Fixture::AutoLook)
            continue;
        fxi.setStageLook(look);
        const QByteArray xml = stageLookSaved(fxi);
        QVERIFY(xml.contains(QString("<StageLook>%1</StageLook>")
                                 .arg(Fixture::stageLookToString(look)).toUtf8()));

        QXmlStreamReader xmlReader(xml);
        xmlReader.readNextStartElement();       // TestRoot
        xmlReader.readNextStartElement();       // Fixture
        Fixture loaded(this);
        QVERIFY(loaded.loadXML(xmlReader, m_doc, m_doc->fixtureDefCache()) == true);
        QCOMPARE(loaded.stageLook(), look);
        QCOMPARE(loaded.name(), QString("Spot 1"));
        QCOMPARE(loaded.address(), quint32(10));
        QCOMPARE(loaded.channels(), quint32(4));
        // And saved again, byte for byte.
        QCOMPARE(stageLookSaved(loaded), xml);
    }
}

void Fixture_Test::stageLookCurtainSaveLoad()
{
    // A dimmer marked Curtain: saved by its key, read back as one.
    Fixture fxi(this);
    fxi.setID(6);
    fxi.setName("Traveller");
    fxi.setChannels(1);
    QLCFixtureDef *def = fxi.genericDimmerDef(1);
    fxi.setFixtureDefinition(def, fxi.genericDimmerMode(def, 1));
    fxi.setStageLook(Fixture::CurtainLook);
    QCOMPARE(fxi.stageLook(), Fixture::CurtainLook);
    const QByteArray xml = stageLookSaved(fxi);
    QVERIFY(xml.contains("<StageLook>Curtain</StageLook>"));
    QXmlStreamReader xmlReader(xml);
    xmlReader.readNextStartElement();       // TestRoot
    xmlReader.readNextStartElement();       // Fixture
    Fixture loaded(this);
    QVERIFY(loaded.loadXML(xmlReader, m_doc, m_doc->fixtureDefCache()) == true);
    QCOMPARE(loaded.stageLook(), Fixture::CurtainLook);
    QCOMPARE(stageLookSaved(loaded), xml);
    // Back to Auto: nothing saved, as before it was marked.
    loaded.setStageLook(Fixture::AutoLook);
    QVERIFY(!stageLookSaved(loaded).contains("StageLook"));
}

void Fixture_Test::stageLookUnknownKey()
{
    /* A look a newer qlcconsole knows and this one does not: the fixture
       still loads, every other field intact, drawn as Auto. (Upstream QLC+
       skips the whole unknown StageLook tag the same way.) */
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    xmlWriter.writeStartElement("Fixture");
    xmlWriter.writeTextElement("Manufacturer", "Generic");
    xmlWriter.writeTextElement("Model", "Generic");
    xmlWriter.writeTextElement("Mode", "Generic");
    xmlWriter.writeTextElement("ID", "9");
    xmlWriter.writeTextElement("Name", "Future");
    xmlWriter.writeTextElement("StageLook", "Hologram");
    xmlWriter.writeTextElement("Universe", "1");
    xmlWriter.writeTextElement("Address", "100");
    xmlWriter.writeTextElement("Channels", "2");
    xmlWriter.writeEndElement();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();
    Fixture fxi(this);
    QVERIFY(fxi.loadXML(xmlReader, m_doc, m_doc->fixtureDefCache()) == true);
    QCOMPARE(fxi.stageLook(), Fixture::AutoLook);
    QCOMPARE(fxi.name(), QString("Future"));
    QCOMPARE(fxi.universe(), quint32(1));
    QCOMPARE(fxi.address(), quint32(100));
    QCOMPARE(fxi.channels(), quint32(2));
}

void Fixture_Test::stageLookAuto()
{
    // A generic dimmer: a conventional -- an ellipsoidal, not a lantern
    // (a lantern is a practical).
    Fixture dimmer(this);
    QLCFixtureDef *ddef = dimmer.genericDimmerDef(1);
    dimmer.setFixtureDefinition(ddef, dimmer.genericDimmerMode(ddef, 1));
    QCOMPARE(dimmer.autoStageLook(), Fixture::EllipsoidalLook);

    // A moving head with a gobo wheel: a spot.
    QLCFixtureDef *mac = m_doc->fixtureDefCache()->fixtureDef("Martin", "MAC250+");
    QVERIFY(mac != NULL);
    Fixture mover(this);
    mover.setFixtureDefinition(mac, mac->modes().at(0));
    QCOMPARE(mover.autoStageLook(), Fixture::MovingSpotLook);

    // By type: a colour changer is an LED PAR, a bar a bar wash, a strobe a strobe.
    foreach (QLCFixtureDef *def, m_doc->fixtureDefCache()->definitions())
    {
        if (def->modes().isEmpty())
            continue;
        Fixture fx(this);
        fx.setFixtureDefinition(def, def->modes().first());
        switch (def->type())
        {
            case QLCFixtureDef::ColorChanger:
                QCOMPARE(fx.autoStageLook(), Fixture::LedParLook);
                break;
            case QLCFixtureDef::LEDBarBeams:
            case QLCFixtureDef::LEDBarPixels:
                QCOMPARE(fx.autoStageLook(), Fixture::BarWashLook);
                break;
            case QLCFixtureDef::Strobe:
                QCOMPARE(fx.autoStageLook(), Fixture::StrobeLook);
                break;
            case QLCFixtureDef::Dimmer:
                QCOMPARE(fx.autoStageLook(), Fixture::EllipsoidalLook);
                break;
            case QLCFixtureDef::MovingHead:
                QCOMPARE(Fixture::stageLookSection(fx.autoStageLook()), int(Fixture::MovingSection));
                break;
            case QLCFixtureDef::Hazer:
            case QLCFixtureDef::Smoke:
            case QLCFixtureDef::Fan:
                QCOMPARE(fx.autoStageLook(), Fixture::AutoLook);
                break;
            default:
                break;
        }
        fx.setFixtureDefinition(NULL, NULL);
    }
    // Auto is still Auto: it names a look, it does not set one.
    QCOMPARE(mover.stageLook(), Fixture::AutoLook);
}

void Fixture_Test::stageLookIcon()
{
    QLCFixtureDef *mac = m_doc->fixtureDefCache()->fixtureDef("Martin", "MAC250+");
    QVERIFY(mac != NULL);
    Fixture fxi(this);
    fxi.setFixtureDefinition(mac, mac->modes().at(0));
    QCOMPARE(fxi.iconResource(), QString(":/movinghead.png"));
    QCOMPARE(fxi.iconResource(true), QString("qrc:/movinghead.svg"));

    fxi.setStageLook(Fixture::FollowspotLook);
    QCOMPARE(fxi.iconResource(), QString(":/look_followspot.png"));
    // A curtain wears the curtain icon the auto-detected ones do.
    QCOMPARE(Fixture::stageLookIconResource(Fixture::CurtainLook), QString(":/curtain.png"));
    QCOMPARE(Fixture::stageLookIconResource(Fixture::CurtainLook, true), QString("qrc:/curtain.svg"));
    QCOMPARE(fxi.iconResource(true), QString("qrc:/look_followspot.svg"));
    // Its type's icon is still there, for "Auto (...)".
    QCOMPARE(fxi.typeIconResource(), QString(":/movinghead.png"));
    QVERIFY(Fixture::stageLookIconResource(Fixture::AutoLook).isEmpty());

    // Every look's icon is a file the resources ship.
    const QString icons = QString(INTERNAL_FIXTUREDIR) + QStringLiteral("../icons/");
    foreach (Fixture::StageLook look, Fixture::stageLooks())
    {
        if (look == Fixture::AutoLook)
            continue;
        const QString base = Fixture::stageLookIconResource(look).mid(2);   // drop ":/"
        QVERIFY2(QFile::exists(icons + QStringLiteral("png/") + base), qPrintable(base));
        QVERIFY2(QFile::exists(icons + QStringLiteral("svg/")
                                   + Fixture::stageLookIconResource(look, true).mid(5)),
                 qPrintable(base));
    }
}

/* A generic dimmer's definition is made for that fixture alone: every one it
   is given, and the last one when the fixture goes, is deleted. The last one
   leaked (TEST_MATRIX L1, about 50 KB a show open). */
void Fixture_Test::aGenericDimmersDefinitionGoesWithIt()
{
    Fixture *fxi = new Fixture(m_doc);
    fxi->setChannels(4);
    QPointer<QLCChannel> first(fxi->fixtureMode()->channel(0));
    QVERIFY(!first.isNull());

    fxi->setChannels(6);    // another count: another definition, the first one deleted
    QVERIFY(first.isNull());
    QPointer<QLCChannel> second(fxi->fixtureMode()->channel(0));
    QVERIFY(!second.isNull());

    delete fxi;
    QVERIFY2(second.isNull(), "the generic dimmer's definition outlived its fixture");

    // a loaded one too
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    xmlWriter.writeStartElement("Fixture");
    xmlWriter.writeTextElement("Manufacturer", "Generic");
    xmlWriter.writeTextElement("Model", "Generic");
    xmlWriter.writeTextElement("Mode", "2 Channel");
    xmlWriter.writeTextElement("ID", "21");
    xmlWriter.writeTextElement("Name", "Dimmer pack");
    xmlWriter.writeTextElement("Universe", "0");
    xmlWriter.writeTextElement("Address", "20");
    xmlWriter.writeTextElement("Channels", "2");
    xmlWriter.writeEndElement();
    xmlWriter.setDevice(NULL);
    buffer.close();
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();
    fxi = new Fixture(m_doc);
    QVERIFY(fxi->loadXML(xmlReader, m_doc, m_doc->fixtureDefCache()));
    QPointer<QLCChannel> loaded(fxi->fixtureMode()->channel(0));
    QVERIFY(!loaded.isNull());
    delete fxi;
    QVERIFY(loaded.isNull());
}

/* Only its own generic definition goes with a fixture -- one a definition
   file gave is the cache's, even when it reads "Generic Generic". And the
   destructor never asks the definition: the Doc deletes its cache before its
   child fixtures (Windows CI crashed in ~Fixture -> QLCFixtureDef::
   manufacturer() when it did). */
void Fixture_Test::aFixtureNeverDeletesADefinitionItDidNotMake()
{
    QLCFixtureDef *fileDef = new QLCFixtureDef();
    fileDef->setManufacturer(KXMLFixtureGeneric);
    fileDef->setModel(KXMLFixtureGeneric);
    fileDef->setDefinitionSourceFile(QStringLiteral("/defs/Generic-Generic.qxf"));
    QLCChannel *ch = new QLCChannel();
    ch->setName(QStringLiteral("Dimmer"));
    ch->setGroup(QLCChannel::Intensity);
    fileDef->addChannel(ch);
    QLCFixtureMode *mode = new QLCFixtureMode(fileDef);
    mode->setName(QStringLiteral("1 Channel"));
    mode->insertChannel(ch, 0);
    fileDef->addMode(mode);
    QPointer<QLCChannel> watch(ch);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setFixtureDefinition(fileDef, mode);
    delete fxi;
    QVERIFY2(!watch.isNull(), "a fixture deleted a definition it was only given");

    // The Doc's order: the cached definition goes first, then the fixture.
    fxi = new Fixture(m_doc);
    fxi->setFixtureDefinition(fileDef, mode);
    delete fileDef;
    delete fxi;
}

QTEST_APPLESS_MAIN(Fixture_Test)

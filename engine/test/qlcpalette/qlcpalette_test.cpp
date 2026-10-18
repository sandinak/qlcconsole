/*
  Q Light Controller Plus - Unit tests
  qlcpalette_test.cpp

  Copyright (C) Massimo Callegari

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
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "qlcpalette_test.h"
#include "qlcpalette.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "scenevalue.h"
#include "fixture.h"
#include "doc.h"
#include "qlcpoint.h"
#include "qlcfixturehead.h"
#include "grouphead.h"
#include "fixturegroup.h"
#include "qlcfixturedefcache.h"
#include "qlccapability.h"
#include "qlcfile.h"

void QLCPalette_Test::initialization()
{
    QLCPalette p(QLCPalette::Undefined);

    QVERIFY(p.type() == QLCPalette::Undefined);
    QVERIFY(p.id() == QLCPalette::invalidId());
    QVERIFY(p.fanningType() == QLCPalette::Flat);
    QVERIFY(p.fanningLayout() == QLCPalette::XAscending);
    QVERIFY(p.fanningAmount() == 100);
    QVERIFY(p.fanningValue() == QVariant());

    p.setID(42);
    QVERIFY(p.id() == 42);

    QVERIFY(p.name() == QString());
    p.setName("My Palette");
    QVERIFY(p.name() == QString("My Palette"));
}

void QLCPalette_Test::type()
{
    QVERIFY(QLCPalette::typeToString(QLCPalette::Undefined) == QString());
    QVERIFY(QLCPalette::typeToString(QLCPalette::Dimmer) == QString("Dimmer"));
    QVERIFY(QLCPalette::typeToString(QLCPalette::Color) == QString("Color"));
    QVERIFY(QLCPalette::typeToString(QLCPalette::Pan) == QString("Pan"));
    QVERIFY(QLCPalette::typeToString(QLCPalette::Tilt) == QString("Tilt"));
    QVERIFY(QLCPalette::typeToString(QLCPalette::PanTilt) == QString("PanTilt"));
    QVERIFY(QLCPalette::typeToString(QLCPalette::Shutter) == QString("Shutter"));
    QVERIFY(QLCPalette::typeToString(QLCPalette::Gobo) == QString("Gobo"));
    QVERIFY(QLCPalette::typeToString(QLCPalette::Zoom) == QString("Zoom"));

    QVERIFY(QLCPalette::stringToType("Foo") == QLCPalette::Undefined);
    QVERIFY(QLCPalette::stringToType("Dimmer") == QLCPalette::Dimmer);
    QVERIFY(QLCPalette::stringToType("Color") == QLCPalette::Color);
    QVERIFY(QLCPalette::stringToType("Pan") == QLCPalette::Pan);
    QVERIFY(QLCPalette::stringToType("Tilt") == QLCPalette::Tilt);
    QVERIFY(QLCPalette::stringToType("PanTilt") == QLCPalette::PanTilt);
    QVERIFY(QLCPalette::stringToType("Shutter") == QLCPalette::Shutter);
    QVERIFY(QLCPalette::stringToType("Gobo") == QLCPalette::Gobo);
    QVERIFY(QLCPalette::stringToType("Zoom") == QLCPalette::Zoom);
}

void QLCPalette_Test::icon()
{
    QLCPalette p1(QLCPalette::Dimmer);
    QCOMPARE(p1.iconResource(), QString(":/intensity.png"));
    QLCPalette p2(QLCPalette::Color);
    QCOMPARE(p2.iconResource(), QString(":/color.png"));
    QLCPalette p3(QLCPalette::Pan);
    QCOMPARE(p3.iconResource(), QString(":/pan.png"));
    QLCPalette p4(QLCPalette::Tilt);
    QCOMPARE(p4.iconResource(), QString(":/tilt.png"));
    QLCPalette p5(QLCPalette::PanTilt);
    QCOMPARE(p5.iconResource(true), QString("qrc:/position.svg"));
    QLCPalette p6(QLCPalette::Shutter);
    QCOMPARE(p6.iconResource(), QString(":/shutter.png"));
    QLCPalette p7(QLCPalette::Gobo);
    QCOMPARE(p7.iconResource(), QString(":/gobo.png"));
    QLCPalette p8(QLCPalette::Zoom);
    QCOMPARE(p8.iconResource(), QString(":/beam.png"));
}

void QLCPalette_Test::value()
{
    /* test one single integer value */
    QLCPalette p1(QLCPalette::Dimmer);

    QVERIFY(p1.values().count() == 0);

    p1.setValue(128);
    QVERIFY(p1.values().count() == 1);
    QVERIFY(p1.value().toInt() == 128);

    /* test composite color value */
    QLCPalette p2(QLCPalette::Color);
    p2.setValue(QLCPalette::colorToString(QColor(0x11, 0x22, 0x33), QColor(0x44, 0x55, 0x66)));
    QVERIFY(p1.values().count() == 1);

    QColor rgb, wauv;
    QVERIFY(p2.value().toString() == "#112233445566");
    QVERIFY(QLCPalette::stringToColor(p2.value().toString(), rgb, wauv) == true);
    QVERIFY(rgb == QColor(0x11, 0x22, 0x33));
    QVERIFY(wauv == QColor(0x44, 0x55, 0x66));

    /* test 2 integer values */
    QLCPalette p3(QLCPalette::PanTilt);
    p3.setValue(180, 90);

    QVERIFY(p3.values().count() == 2);
    QVERIFY(p3.values().at(0).toInt() == 180);
    QVERIFY(p3.values().at(1).toInt() == 90);
}

void QLCPalette_Test::fanning()
{
    QVERIFY(QLCPalette::fanningTypeToString(QLCPalette::Flat) == "Flat");
    QVERIFY(QLCPalette::fanningTypeToString(QLCPalette::Linear) == "Linear");
    QVERIFY(QLCPalette::fanningTypeToString(QLCPalette::Sine) == "Sine");
    QVERIFY(QLCPalette::fanningTypeToString(QLCPalette::Square) == "Square");
    QVERIFY(QLCPalette::fanningTypeToString(QLCPalette::Saw) == "Saw");

    QVERIFY(QLCPalette::stringToFanningType("Foo") == QLCPalette::Flat);
    QVERIFY(QLCPalette::stringToFanningType("Flat") == QLCPalette::Flat);
    QVERIFY(QLCPalette::stringToFanningType("Linear") == QLCPalette::Linear);
    QVERIFY(QLCPalette::stringToFanningType("Sine") == QLCPalette::Sine);
    QVERIFY(QLCPalette::stringToFanningType("Square") == QLCPalette::Square);
    QVERIFY(QLCPalette::stringToFanningType("Saw") == QLCPalette::Saw);

    QVERIFY(QLCPalette::fanningLayoutToString(QLCPalette::XAscending) == "XAscending");
    QVERIFY(QLCPalette::fanningLayoutToString(QLCPalette::XDescending) == "XDescending");
    QVERIFY(QLCPalette::fanningLayoutToString(QLCPalette::XCentered) == "XCentered");
    QVERIFY(QLCPalette::fanningLayoutToString(QLCPalette::YAscending) == "YAscending");
    QVERIFY(QLCPalette::fanningLayoutToString(QLCPalette::YDescending) == "YDescending");
    QVERIFY(QLCPalette::fanningLayoutToString(QLCPalette::YCentered) == "YCentered");
    QVERIFY(QLCPalette::fanningLayoutToString(QLCPalette::ZAscending) == "ZAscending");
    QVERIFY(QLCPalette::fanningLayoutToString(QLCPalette::ZDescending) == "ZDescending");
    QVERIFY(QLCPalette::fanningLayoutToString(QLCPalette::ZCentered) == "ZCentered");

    QVERIFY(QLCPalette::stringToFanningLayout("Foo") == QLCPalette::XAscending);
    QVERIFY(QLCPalette::stringToFanningLayout("XAscending") == QLCPalette::XAscending);
    QVERIFY(QLCPalette::stringToFanningLayout("XDescending") == QLCPalette::XDescending);
    QVERIFY(QLCPalette::stringToFanningLayout("XCentered") == QLCPalette::XCentered);
    QVERIFY(QLCPalette::stringToFanningLayout("YAscending") == QLCPalette::YAscending);
    QVERIFY(QLCPalette::stringToFanningLayout("YDescending") == QLCPalette::YDescending);
    QVERIFY(QLCPalette::stringToFanningLayout("YCentered") == QLCPalette::YCentered);
    QVERIFY(QLCPalette::stringToFanningLayout("ZAscending") == QLCPalette::ZAscending);
    QVERIFY(QLCPalette::stringToFanningLayout("ZDescending") == QLCPalette::ZDescending);
    QVERIFY(QLCPalette::stringToFanningLayout("ZCentered") == QLCPalette::ZCentered);

    QLCPalette p(QLCPalette::Dimmer);
    p.setFanningAmount(75);

    QVERIFY(p.fanningAmount() == 75);
}

void QLCPalette_Test::colorHelpers()
{
    QColor rgb(0xAA, 0xBB, 0xCC);
    QColor wauv(0x11, 0x22, 0x33);

    QVERIFY(QLCPalette::colorToString(rgb, wauv) == QString("#aabbcc112233"));

    QVERIFY(QLCPalette::stringToColor("#invalid", rgb, wauv) == false);
    QVERIFY(QLCPalette::stringToColor("#11deadbeef22", rgb, wauv) == true);

    QVERIFY(rgb.red() == 0x11);
    QVERIFY(rgb.green() == 0xDE);
    QVERIFY(rgb.blue() == 0xAD);

    QVERIFY(wauv.red() == 0xBE);
    QVERIFY(wauv.green() == 0xEF);
    QVERIFY(wauv.blue() == 0x22);
}

void QLCPalette_Test::load()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Palette");
    xmlWriter.writeAttribute("ID", "1");
    xmlWriter.writeAttribute("Type", "Color");
    xmlWriter.writeAttribute("Name", "Lavender");
    xmlWriter.writeAttribute("Value", "#AABBCCDDEEFF");
    xmlWriter.writeAttribute("Fan", "Linear");
    xmlWriter.writeAttribute("Layout", "LeftToRight");
    xmlWriter.writeAttribute("Amount", "42");
    xmlWriter.writeAttribute("FanValue", "#00ff00");
    xmlWriter.writeEndElement();

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    QLCPalette p(QLCPalette::Undefined);
    QVERIFY(p.loadXML(xmlReader) == true);

    QVERIFY(p.id() == 1);
    QVERIFY(p.type() == QLCPalette::Color);
    QVERIFY(p.name() == "Lavender");
    QVERIFY(p.value().toString() == "#AABBCCDDEEFF");
    QVERIFY(p.fanningType() == QLCPalette::Linear);
    QVERIFY(p.fanningLayout() == QLCPalette::XAscending);
    QVERIFY(p.fanningAmount() == 42);
    QVERIFY(p.fanningValue().toString() == "#00ff00");
}

void QLCPalette_Test::loadWrongRoot()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Politte");
    xmlWriter.writeAttribute("ID", "42");
    xmlWriter.writeAttribute("Type", "Dimmer");
    xmlWriter.writeAttribute("Name", "Just wrong");
    xmlWriter.writeAttribute("Value", "42");
    xmlWriter.writeEndElement();

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    QLCPalette p(QLCPalette::Undefined);
    QVERIFY(p.loadXML(xmlReader) == false);
}

void QLCPalette_Test::save()
{
    QLCPalette p(QLCPalette::PanTilt);
    p.setID(3);
    p.setName("Center up");
    p.setValue(90, 145);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    QVERIFY(p.saveXML(&xmlWriter) == true);

    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);

    xmlReader.readNextStartElement();
    QVERIFY(xmlReader.name().toString() == "Palette");

    QVERIFY(xmlReader.attributes().value("ID").toString() == "3");
    QVERIFY(xmlReader.attributes().value("Name").toString() == "Center up");
    QVERIFY(xmlReader.attributes().value("Type").toString() == "PanTilt");
    QVERIFY(xmlReader.attributes().value("Value").toString() == "90,145");
}

void QLCPalette_Test::aGoboLookReachesTheGoboChannel()
{
    // Dimmer, gobo wheel, gobo rotation -- no <Head>, like most spots.
    QLCFixtureDef def;
    def.setManufacturer("Test");
    def.setModel("Spot");
    QLCFixtureMode *mode = new QLCFixtureMode(&def);
    mode->setName("3ch");
    const QLCChannel::Group groups[] = { QLCChannel::Intensity, QLCChannel::Gobo,
                                         QLCChannel::Gobo };
    const char *names[] = { "Dimmer", "Gobo", "Gobo Rotation" };
    for (int i = 0; i < 3; i++)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[i]);
        ch->setGroup(groups[i]);
        ch->setControlByte(QLCChannel::MSB);
        def.addChannel(ch);
        mode->insertChannel(ch, i);
    }
    def.addMode(mode);

    Doc doc(this);
    Fixture *spot = new Fixture(&doc);
    spot->setFixtureDefinition(&def, mode);
    QVERIFY(doc.addFixture(spot));
    Fixture *plain = new Fixture(&doc);             // generic dimmer: no wheel
    plain->setChannels(1);
    plain->setAddress(10);
    QVERIFY(doc.addFixture(plain));

    QLCPalette gobo(QLCPalette::Gobo);
    gobo.setValue(49);
    const QList<SceneValue> vals =
        gobo.valuesFromFixtures(&doc, QList<quint32>() << spot->id() << plain->id());
    QCOMPARE(vals.size(), 1);
    QCOMPARE(vals.first().fxi, spot->id());
    QCOMPARE(vals.first().channel, quint32(1));     // the wheel, not the rotation
    QCOMPARE(int(vals.first().value), 49);

    spot->setFixtureDefinition(nullptr, nullptr);   // def is on the stack
}

void QLCPalette_Test::aColourLookOnAGroupOnlyLightsTheGroupsHeads()
{
    /* "lighting the white leds as well as the RGB": a bar whose RGB pixels
       and white pixels are separate heads, and a group of only the RGB ones.
       A colour look on the group must not reach the white heads. */
    QLCFixtureDef def;
    def.setManufacturer("Test");
    def.setModel("Bar");
    QLCFixtureMode *mode = new QLCFixtureMode(&def);
    mode->setName("4ch");
    const QLCChannel::Preset presets[] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                           QLCChannel::IntensityBlue, QLCChannel::IntensityWhite };
    for (int i = 0; i < 4; i++)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("ch%1").arg(i));
        ch->setPreset(presets[i]);
        def.addChannel(ch);
        mode->insertChannel(ch, i);
    }
    QLCFixtureHead rgb, white;
    rgb.addChannel(0); rgb.addChannel(1); rgb.addChannel(2);
    white.addChannel(3);
    mode->insertHead(-1, rgb);
    mode->insertHead(-1, white);
    def.addMode(mode);

    Doc doc(this);
    Fixture *bar = new Fixture(&doc);
    bar->setFixtureDefinition(&def, mode);
    QVERIFY(doc.addFixture(bar));
    QCOMPARE(bar->heads(), 2);
    FixtureGroup *grp = new FixtureGroup(&doc);
    grp->setName("Bar RGB");
    grp->setSize(QSize(1, 1));
    QVERIFY(grp->assignHead(QLCPoint(0, 0), GroupHead(bar->id(), 0)));
    QVERIFY(doc.addFixtureGroup(grp));

    auto channelsSet = [](const QList<SceneValue> &vals) {
        QMap<quint32, int> m;
        for (const SceneValue &v : vals)
            m[v.channel] = v.value;
        return m;
    };
    for (const QString &value : { QString("#ffffff"), QString("#ffffffff0000") })
    {
        QLCPalette white(QLCPalette::Color);
        white.setValue(value);
        // The group: its RGB head lit, the white head not touched.
        const QMap<quint32, int> g = channelsSet(white.valuesFromFixtureGroups(&doc, { grp->id() }));
        QVERIFY2(!g.contains(3), qPrintable(value));
        QVERIFY2(g.value(0) == 255 && g.value(1) == 255 && g.value(2) == 255, qPrintable(value));
        // The fixture itself, all its heads: the white head too.
        const QMap<quint32, int> f = channelsSet(white.valuesFromFixtures(&doc, { bar->id() }));
        QVERIFY2(f.value(3) == 255, qPrintable(value));
    }

    bar->setFixtureDefinition(nullptr, nullptr);    // def is on the stack
}

/* Each row: a wheel (gobos ';'-separated; a slot's picture after a '|'),
   the name looked for, the slot it should land on (-1: none), how, and the
   gobos an ambiguous match names. */
void QLCPalette_Test::goboNamesMatchInTiers_data()
{
    QTest::addColumn<QString>("wheel");
    QTest::addColumn<QString>("wanted");
    QTest::addColumn<int>("slot");
    QTest::addColumn<int>("how");
    QTest::addColumn<QString>("ambiguous");

    const int exact = QLCPalette::GoboExact, singular = QLCPalette::GoboSingular,
              partial = QLCPalette::GoboPartial, amb = QLCPalette::GoboAmbiguous,
              none = QLCPalette::GoboNoMatch;
    const QString wheel = "Open;Dots;Dots Shake;Breakup;Gobo 3;Flame|ClayPaky/gobo00006;Bars 2";

    // 1. Exact, ignoring case, spaces, '-' and '_'.
    QTest::newRow("exact") << wheel << "Dots" << 1 << exact << "";
    QTest::newRow("exact, case/space/-/_") << wheel << "dots_SHAKE" << 2 << exact << "";
    QTest::newRow("exact, break-up") << wheel << "Break-Up" << 3 << exact << "";
    QTest::newRow("exact wins over plural") << "Dot;Dots" << "Dots" << 1 << exact << "";
    QTest::newRow("exact picture, folder counts") << wheel << "ClayPaky/gobo00006" << 5 << exact << "";
    QTest::newRow("picture of other glass") << wheel << "Chauvet/gobo00006" << -1 << none << "";
    QTest::newRow("same name twice is one gobo") << "Open;Dots;Open" << "open" << 0 << exact << "";
    QTest::newRow("'Gobo 3' is that slot only") << wheel << "Gobo 3" << 4 << exact << "";
    QTest::newRow("'Gobo 4': no partial on a number") << wheel << "Gobo 4" << -1 << none << "";

    // 2. Singular/plural; the word "gobo" and numbers don't count.
    QTest::newRow("singular finds plural") << wheel << "Dot" << 1 << singular << "";
    QTest::newRow("plural finds singular") << "Open;Dot;Star" << "Dots" << 1 << singular << "";
    QTest::newRow("'Gobo 3 Dots'") << "Open;Dots" << "Gobo 3 Dots" << 1 << singular << "";
    QTest::newRow("'Bar' = 'Bars 2'") << wheel << "Bar" << 6 << singular << "";
    QTest::newRow("'gobo-flames 7'") << wheel << "gobo-flames 7" << 5 << singular << "";
    QTest::newRow("plural, two names") << "Dots 1;Dots 2;Star" << "Dot" << -1 << amb << "Dots 1,Dots 2";
    QTest::newRow("exact beats two plurals") << "Dot;Dots 2;Star" << "Dot" << 0 << exact << "";

    // 3. One slot starting with / containing it.
    QTest::newRow("unique prefix") << "Open;Breakup Rotating;Star" << "Breakup" << 1 << partial << "";
    QTest::newRow("unique contains") << "Open;Big Star;Bars" << "star" << 1 << partial << "";
    QTest::newRow("prefix, only the shake") << "Open;Dots Shake;Star" << "Dots" << 1 << partial << "";
    QTest::newRow("two contain it") << "Open;Dots Shake;Dots Rotating" << "Dots" << -1 << amb << "Dots Shake,Dots Rotating";
    QTest::newRow("too short to contain") << "Open;Tower;Star" << "To" << -1 << none << "";
    QTest::newRow("pictures aren't partial") << wheel << "ClayPaky/gobo" << -1 << none << "";

    QTest::newRow("nothing like it") << wheel << "Cloud" << -1 << none << "";
    QTest::newRow("empty name") << wheel << "  " << -1 << none << "";
}

void QLCPalette_Test::goboNamesMatchInTiers()
{
    QFETCH(QString, wheel);
    QFETCH(QString, wanted);
    QFETCH(int, slot);
    QFETCH(int, how);
    QFETCH(QString, ambiguous);

    QList<QStringList> gobos;
    QStringList names;
    for (const QString &s : wheel.split(';'))
    {
        gobos << s.split('|');
        names << s.split('|').first();
    }
    QLCPalette::GoboMatch got = QLCPalette::GoboPicked;
    QList<int> amb;
    QCOMPARE(QLCPalette::matchGoboName(wanted, gobos, &got, &amb), slot);
    QCOMPARE(int(got), how);
    QStringList ambNames;
    for (int i : amb)
        ambNames << names.at(i);
    QCOMPARE(ambNames.join(','), ambiguous);
}

QTEST_GUILESS_MAIN(QLCPalette_Test)

static QString loadedPath(const QString &type, const QString &path, bool marked)
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter w(&buffer);
    w.writeStartElement("Palette");
    w.writeAttribute("ID", "7");
    w.writeAttribute("Type", type);
    w.writeAttribute("Name", "P");
    w.writeAttribute("Path", path);
    if (marked)
        w.writeAttribute("Folders", "1");
    if (type == "Color")
        w.writeAttribute("Value", "#ff0000");
    else if (type == "Dimmer")
        w.writeAttribute("Value", "100");
    w.writeEndElement();
    w.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader r(&buffer);
    r.readNextStartElement();
    QLCPalette p(QLCPalette::Undefined);
    if (p.loadXML(r) == false)
        return QString("<load failed>");
    return p.path();
}

void QLCPalette_Test::olderShowsTypeFoldersBecomeSharedFolders()
{
    // The rule itself.
    QCOMPARE(QLCPalette::migrateLegacyPath("Palettes/Color/01-Song/", QLCPalette::Color),
             QString("Palettes/01-Song/"));
    QCOMPARE(QLCPalette::migrateLegacyPath("Palettes/Color/", QLCPalette::Color), QString());
    QCOMPARE(QLCPalette::migrateLegacyPath("Palettes/Color", QLCPalette::Color), QString());
    // An effect filed under a colour folder joins the same shared folder --
    // when the whole show is in the old layout (anyTypeFolder); alone, a
    // folder named like ANOTHER type may be the user's: kept.
    QCOMPARE(QLCPalette::migrateLegacyPath("Palettes/Color/01-Song/", QLCPalette::Effect, true),
             QString("Palettes/01-Song/"));
    QCOMPARE(QLCPalette::migrateLegacyPath("Palettes/Color/01-Song/", QLCPalette::Effect),
             QString("Palettes/Color/01-Song/"));
    QCOMPARE(QLCPalette::migrateLegacyPath("Palettes/Color/", QLCPalette::Dimmer),
             QString("Palettes/Color/"));
    // Not a type's name at all: never touched.
    QCOMPARE(QLCPalette::migrateLegacyPath("Palettes/Colour/", QLCPalette::Color, true),
             QString("Palettes/Colour/"));
    // Auto-filed effects (Effect/<Category>/<Engine>) were bookkeeping: unfiled.
    QCOMPARE(QLCPalette::migrateLegacyPath("Palettes/Effect/Color/Sinelon/", QLCPalette::Effect),
             QString());
    QCOMPARE(QLCPalette::migrateLegacyPath("Palettes/Effect/WLED/Fire 2012/", QLCPalette::Effect),
             QString());
    // A user folder under Effect is kept.
    QCOMPARE(QLCPalette::migrateLegacyPath("Palettes/Effect/My Faves/", QLCPalette::Effect),
             QString("Palettes/My Faves/"));
    QCOMPARE(QLCPalette::migrateLegacyPath("Palettes/Aim/01-Song/Verse/", QLCPalette::Aim),
             QString("Palettes/01-Song/Verse/"));
    // Not a type folder / not a palette path / empty: unchanged.
    QCOMPARE(QLCPalette::migrateLegacyPath("Palettes/Warm/", QLCPalette::Color),
             QString("Palettes/Warm/"));
    QCOMPARE(QLCPalette::migrateLegacyPath("Shutters", QLCPalette::Shutter), QString("Shutters"));
    QCOMPARE(QLCPalette::migrateLegacyPath("", QLCPalette::Color), QString());

    // On load: an older show (no Folders marker) migrates...
    QCOMPARE(loadedPath("Color", "Palettes/Color/01-Song/", false), QString("Palettes/01-Song/"));
    QCOMPARE(loadedPath("Dimmer", "Palettes/Dimmer/", false), QString());
    // ...a show saved by this version doesn't: a folder named "Color" stays.
    QCOMPARE(loadedPath("Color", "Palettes/Color/", true), QString("Palettes/Color/"));

    // Saving marks the path, so the round trip keeps it as it is.
    QLCPalette p(QLCPalette::Dimmer);
    p.setID(9);
    p.setName("Full");
    p.setValue(255);
    p.setPath("Palettes/Dimmer/");   // a user folder called "Dimmer"
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter w(&buffer);
    QVERIFY(p.saveXML(&w));
    w.setDevice(NULL);
    buffer.close();
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader r(&buffer);
    r.readNextStartElement();
    QCOMPARE(r.attributes().value("Folders").toString(), QString("1"));
    QLCPalette back(QLCPalette::Undefined);
    QVERIFY(back.loadXML(r));
    QCOMPARE(back.path(), QString("Palettes/Dimmer/"));
}

/* The merge of feat/palette-folders into feat/show-tree (open 25): the old
   per-type layout is migrated only where it is genuinely that. A show all
   in the old layout (every unmarked folder starts with a type's name):
   any type name first goes, so an effect dragged under Color/01-Song
   joins 01-Song. A show with shared folders but no marker yet (saved by
   a build between): only a palette's own type name goes -- a user folder
   "Color" holding dimmers stays, and "Colour" is never touched. */
void QLCPalette_Test::aUserFolderNamedLikeATypeIsKept()
{
    auto load = [](const QList<QStringList> &pals) {
        QString xml = "<Engine>";
        int id = 1;
        for (const QStringList &p : pals)
        {
            xml += QString("<Palette ID=\"%1\" Type=\"%2\" Name=\"P%1\" Path=\"%3\"%4")
                   .arg(id++).arg(p.at(0), p.at(1), p.value(2) == "marked" ? " Folders=\"1\"" : "");
            if (p.at(0) == "Color")
                xml += " Value=\"#ff0000\"";
            else if (p.at(0) == "Dimmer")
                xml += " Value=\"100\"";
            xml += "/>";
        }
        xml += "</Engine>";
        Doc *doc = new Doc(nullptr);
        QXmlStreamReader r(xml);
        r.readNextStartElement();
        doc->loadXML(r, false);
        QStringList out;
        for (int i = 1; i < id; ++i)
            out << (doc->palette(quint32(i)) ? doc->palette(quint32(i))->path() : QString("<none>"));
        delete doc;
        return out;
    };

    // Old layout throughout.
    QCOMPARE(load({ { "Color", "Palettes/Color/01-Song/" },
                    { "Effect", "Palettes/Color/01-Song/" },
                    { "Dimmer", "Palettes/Dimmer/" } }),
             QStringList({ "Palettes/01-Song/", "Palettes/01-Song/", "" }));

    // Shared folders, no marker (a build in between): the user's "Color"
    // (dimmers in it) and "Colour" stay; a colour under its own type's
    // name is still the old layout.
    QCOMPARE(load({ { "Color", "Palettes/01-Song/" },
                    { "Dimmer", "Palettes/Color/" },
                    { "Color", "Palettes/Colour/" },
                    { "Color", "Palettes/Color/Verse/" } }),
             QStringList({ "Palettes/01-Song/", "Palettes/Color/", "Palettes/Colour/", "Palettes/Verse/" }));

    // Marked: never migrated, whatever the name -- and a show with marks
    // was saved with shared folders, so another type's name is the user's.
    QCOMPARE(load({ { "Color", "Palettes/Color/", "marked" },
                    { "Dimmer", "Palettes/Color/01-Song/" } }),
             QStringList({ "Palettes/Color/", "Palettes/Color/01-Song/" }));
}

/*****************************************************************************
 * Colour rings
 *****************************************************************************/

/* Branson's mover with a colour ring, as he made it (one head; the ring in
   it in 12 Channel, in no head in 10 Channel) and as two heads. */
static QLCFixtureDef *ringDef(Doc *doc, bool twoHeads)
{
    QDir dir(TEST_FIXTURE_DIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    if (doc->fixtureDefCache()->fixtureDef("Branson", "LED Movinghead+Circle") == nullptr)
        doc->fixtureDefCache()->load(dir);
    return doc->fixtureDefCache()->fixtureDef("Branson", twoHeads ? "LED Movinghead+Circle (2 heads)"
                                                                  : "LED Movinghead+Circle");
}

static Fixture *addRingFixture(Doc *doc, bool twoHeads, const QString &mode, quint32 address)
{
    QLCFixtureDef *def = ringDef(doc, twoHeads);
    if (def == nullptr)
        return nullptr;
    Fixture *f = new Fixture(doc);
    f->setName(QString("Mover %1").arg(address));
    f->setFixtureDefinition(def, def->mode(mode));
    f->setAddress(address);
    doc->addFixture(f);
    return f;
}

/* Channel -> value of what @p vals sets on @p fid. */
static QMap<quint32, int> valuesOn(const QList<SceneValue> &vals, quint32 fid)
{
    QMap<quint32, int> out;
    for (const SceneValue &sv : vals)
        if (sv.fxi == fid)
            out.insert(sv.channel, sv.value);
    return out;
}

void QLCPalette_Test::slotColoursComeFromTheirNames()
{
    auto colourOf = [](const QString &name) {
        QLCCapability cap(0, 9, name);
        return QLCPalette::capabilityColour(&cap);
    };
    QCOMPARE(colourOf("Blue"), QColor(Qt::blue));
    QCOMPARE(colourOf("off"), QColor(Qt::black));
    QCOMPARE(colourOf("Open"), QColor(Qt::white));
    QCOMPARE(colourOf("Light Blue"), QColor("lightblue"));
    QCOMPARE(colourOf("Red 2"), QColor(Qt::red));
    QVERIFY(colourOf("Purple").isValid());
    // Not one colour: never picked by a colour, only by name.
    QVERIFY(!colourOf("Blue/Purple").isValid());
    QVERIFY(!colourOf("Red/Yellow-Chase").isValid());
    QVERIFY(!colourOf("RGB/Random-Chase").isValid());
    QVERIFY(!colourOf("Rainbow").isValid());
    QVERIFY(!colourOf("").isValid());

    // A picture colour wins; two of them is a split.
    QLCCapability split(0, 9, "Whatever");
    split.setResource(0, QColor(Qt::red));
    QCOMPARE(QLCPalette::capabilityColour(&split), QColor(Qt::red));
    split.setResource(1, QColor(Qt::blue));
    QVERIFY(!QLCPalette::capabilityColour(&split).isValid());
}

void QLCPalette_Test::colourPicksTheNearestSlot_data()
{
    QTest::addColumn<QColor>("colour");
    QTest::addColumn<QString>("ring");      // the ring slot it picks
    QTest::addColumn<int>("ringValue");
    QTest::addColumn<QString>("wheel");     // the wheel slot
    QTest::addColumn<int>("wheelValue");

    // Named colours: their slot (the middle of its range).
    QTest::newRow("blue") << QColor(Qt::blue) << "Blue" << 42 << "Blue" << 34;
    QTest::newRow("red") << QColor(Qt::red) << "Red" << 12 << "Red" << 14;
    QTest::newRow("green") << QColor(Qt::green) << "Green" << 27 << "Green" << 44;
    QTest::newRow("yellow") << QColor(Qt::yellow) << "Yellow" << 57 << "Yellow" << 24;
    QTest::newRow("magenta") << QColor(Qt::magenta) << "Purple" << 72 << "Purple" << 64;
    QTest::newRow("cyan") << QColor(Qt::cyan) << "Cyan" << 87 << "Cyan" << 74;
    QTest::newRow("white") << QColor(Qt::white) << "White" << 102 << "White" << 4;
    // Black: the ring goes off; a wheel has no off and stays open.
    QTest::newRow("black") << QColor(Qt::black) << "off" << 2 << "White" << 4;
    // Arbitrary colours: the nearest hue.
    QTest::newRow("dark orange") << QColor(255, 140, 0) << "Yellow" << 57 << "Orange" << 54;
    QTest::newRow("pink") << QColor(255, 192, 203) << "Red" << 12 << "Red" << 14;
    QTest::newRow("teal") << QColor(0, 128, 128) << "Cyan" << 87 << "Cyan" << 74;
    QTest::newRow("dim blue") << QColor(0, 0, 48) << "Blue" << 42 << "Blue" << 34;
    QTest::newRow("grey") << QColor(128, 128, 128) << "White" << 102 << "White" << 4;
}

void QLCPalette_Test::colourPicksTheNearestSlot()
{
    QFETCH(QColor, colour);
    QFETCH(QString, ring);
    QFETCH(int, ringValue);
    QFETCH(QString, wheel);
    QFETCH(int, wheelValue);

    Doc doc(this);
    QLCFixtureDef *def = ringDef(&doc, true);
    QVERIFY(def != nullptr);
    const QLCChannel *ringCh = def->channel("Circle Color");
    const QLCChannel *wheelCh = def->channel("Color wheel");
    QVERIFY(ringCh != nullptr && wheelCh != nullptr);

    QString name;
    QCOMPARE(QLCPalette::nearestColourValue(ringCh, colour, &name), ringValue);
    QCOMPARE(name, ring);
    QCOMPARE(QLCPalette::nearestColourValue(wheelCh, colour, &name), wheelValue);
    QCOMPARE(name, wheel);
}

void QLCPalette_Test::spotAndRingAreIndependent()
{
    Doc doc(this);
    Fixture *m1 = addRingFixture(&doc, true, "12 Channel", 0);
    Fixture *m2 = addRingFixture(&doc, true, "10 Channel", 20);
    QVERIFY(m1 != nullptr && m2 != nullptr);
    QCOMPARE(m1->heads(), 2);
    QCOMPARE(m2->heads(), 2);

    auto group = [&doc](const QString &name, const QList<GroupHead> &heads) {
        FixtureGroup *g = new FixtureGroup(&doc);
        g->setName(name);
        doc.addFixtureGroup(g);
        g->setSize(QSize(heads.size(), 1));
        for (int i = 0; i < heads.size(); ++i)
            g->assignHead(QLCPoint(i, 0), heads.at(i));
        return g->id();
    };
    const quint32 spots = group("Movers", { GroupHead(m1->id(), 0), GroupHead(m2->id(), 0) });
    const quint32 rings = group("Ring", { GroupHead(m1->id(), 1), GroupHead(m2->id(), 1) });
    const quint32 all = group("All", { GroupHead(m1->id(), 0), GroupHead(m1->id(), 1),
                                       GroupHead(m2->id(), 0), GroupHead(m2->id(), 1) });

    QLCPalette blue(QLCPalette::Color);
    blue.setValue(QLCPalette::colorToString(QColor(Qt::blue), QColor()));

    // 12 Channel: wheel 4, ring 11; 10 Channel: wheel 2, ring 9.
    QList<SceneValue> vals = blue.valuesFromFixtureGroups(&doc, { spots });
    QCOMPARE(valuesOn(vals, m1->id()), (QMap<quint32, int>{ { 4, 34 } }));
    QCOMPARE(valuesOn(vals, m2->id()), (QMap<quint32, int>{ { 2, 34 } }));

    vals = blue.valuesFromFixtureGroups(&doc, { rings });
    QCOMPARE(valuesOn(vals, m1->id()), (QMap<quint32, int>{ { 11, 42 } }));
    QCOMPARE(valuesOn(vals, m2->id()), (QMap<quint32, int>{ { 9, 42 } }));

    // Both heads: each its own nearest.
    vals = blue.valuesFromFixtureGroups(&doc, { all });
    QCOMPARE(valuesOn(vals, m1->id()), (QMap<quint32, int>{ { 4, 34 }, { 11, 42 } }));
    QCOMPARE(valuesOn(vals, m2->id()), (QMap<quint32, int>{ { 2, 34 }, { 9, 42 } }));
    // ...as the fixtures themselves.
    vals = blue.valuesFromFixtures(&doc, { m1->id() });
    QCOMPARE(valuesOn(vals, m1->id()), (QMap<quint32, int>{ { 4, 34 }, { 11, 42 } }));

    // A dimmer look on the ring has nothing to set: the dimmer is the spot's.
    QLCPalette full(QLCPalette::Dimmer);
    full.setValue(255);
    QVERIFY(full.valuesFromFixtureGroups(&doc, { rings }).isEmpty());
    QCOMPARE(valuesOn(full.valuesFromFixtureGroups(&doc, { spots }), m1->id()),
             (QMap<quint32, int>{ { 7, 255 } }));
}

void QLCPalette_Test::oldDefinitionSetsWheelAndRing()
{
    Doc doc(this);
    Fixture *m12 = addRingFixture(&doc, false, "12 Channel", 0);
    Fixture *m10 = addRingFixture(&doc, false, "10 Channel", 20);
    QVERIFY(m12 != nullptr && m10 != nullptr);
    QCOMPARE(m12->heads(), 1);
    QCOMPARE(m10->heads(), 1);

    QLCPalette blue(QLCPalette::Color);
    blue.setValue(QLCPalette::colorToString(QColor(Qt::blue), QColor()));
    QList<SceneValue> vals = blue.valuesFromFixtures(&doc, { m12->id(), m10->id() });
    QCOMPARE(valuesOn(vals, m12->id()), (QMap<quint32, int>{ { 4, 34 }, { 11, 42 } }));
    // The ring in no head is the fixture's: it follows too (it was left alone).
    QCOMPARE(valuesOn(vals, m10->id()), (QMap<quint32, int>{ { 2, 34 }, { 9, 42 } }));

    // Black: the ring off, the wheel open (it was green on both).
    QLCPalette black(QLCPalette::Color);
    black.setValue(QLCPalette::colorToString(QColor(Qt::black), QColor()));
    vals = black.valuesFromFixtures(&doc, { m12->id() });
    QCOMPARE(valuesOn(vals, m12->id()), (QMap<quint32, int>{ { 4, 4 }, { 11, 2 } }));

    // A second colour (for a second RGB set) moves neither.
    QVERIFY(blue.valuesFromFixtures(&doc, { m12->id(), m10->id() }, nullptr, 1).isEmpty());
}

void QLCPalette_Test::aLookCanNameARingMacro()
{
    Doc doc(this);
    Fixture *m = addRingFixture(&doc, true, "12 Channel", 0);
    QVERIFY(m != nullptr);

    QLCPalette chase(QLCPalette::Color);
    chase.setName("Red/Yellow-Chase");
    chase.setValue(QLCPalette::colorToString(QColor(Qt::red), QColor()));
    chase.setColourMacro("red/yellow chase");
    QList<SceneValue> vals = chase.valuesFromFixtures(&doc, { m->id() });
    // The ring plays the chase (110-124); the wheel, which has none, red.
    QCOMPARE(valuesOn(vals, m->id()), (QMap<quint32, int>{ { 4, 14 }, { 11, 117 } }));
    QCOMPARE(QLCPalette::namedSlotValue(m->fixtureMode()->channel(11), "RGB/Random-Chase"), 250);
    QCOMPARE(QLCPalette::namedSlotValue(m->fixtureMode()->channel(11), "Sparkle"), -1);

    // Saved and loaded with it; copied with it.
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    QVERIFY(chase.saveXML(&xmlWriter));
    xmlWriter.setDevice(NULL);
    buffer.close();
    QVERIFY(buffer.data().contains("ColourMacro=\"red/yellow chase\""));
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();
    QLCPalette loaded(QLCPalette::Color);
    QVERIFY(loaded.loadXML(xmlReader));
    QCOMPARE(loaded.colourMacro(), QString("red/yellow chase"));
    QLCPalette *copy = chase.createCopy();
    QCOMPARE(copy->colourMacro(), QString("red/yellow chase"));
    delete copy;
}

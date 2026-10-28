/*
  Q Light Controller Plus - Test Unit
  powerdistribution_test.cpp

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

#include "powerdistribution.h"
#include "powerdistribution_test.h"
#include "qlcfixturehead.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"
#include "venue.h"

#include <QTemporaryDir>

/* A definition of @p type: @p heads heads of @p presets each, after
   @p master (a master dimmer, when not Custom); rated @p watts (0: none). */
static QLCFixtureDef *makeDef(const QString &model, QLCFixtureDef::FixtureType type, int watts,
                              int heads, const QList<QLCChannel::Preset> &presets,
                              QLCChannel::Preset master = QLCChannel::Custom)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel(model);
    def->setType(type);
    QLCPhysical phys;
    phys.setPowerConsumption(watts);
    def->setPhysical(phys);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Mode");
    int n = 0;
    auto add = [&](QLCChannel::Preset preset) {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("ch%1").arg(n));
        ch->setPreset(preset);
        def->addChannel(ch);
        mode->insertChannel(ch, quint32(n));
        return quint32(n++);
    };
    if (master != QLCChannel::Custom)
        add(master);
    for (int h = 0; h < heads; ++h)
    {
        QLCFixtureHead head;
        foreach (QLCChannel::Preset p, presets)
            head.addChannel(add(p));
        mode->insertHead(-1, head);
    }
    def->addMode(mode);
    return def;
}

static Fixture *place(Doc *doc, QLCFixtureDef *def, quint32 address)
{
    Fixture *fx = new Fixture(doc);
    fx->setFixtureDefinition(def, def->modes().first());
    fx->setAddress(address);
    doc->addFixture(fx);
    return fx;
}

static const QList<QLCChannel::Preset> kRGB = {
    QLCChannel::IntensityRed, QLCChannel::IntensityGreen, QLCChannel::IntensityBlue };

void PowerDistribution_Test::defaults()
{
    PowerCircuit cir;
    QCOMPARE(cir.name, QString("Circuit"));
    QCOMPARE(cir.ratedAmps, 20.0);
    QCOMPARE(cir.deratePercent, 80);
    QCOMPARE(cir.deratedLimit(), 16.0); // 20A * 80%
    QCOMPARE(cir.effectiveVoltage(120.0), 120.0); // voltage==0 -> inherit
    cir.voltage = 240.0;
    QCOMPARE(cir.effectiveVoltage(120.0), 240.0); // explicit overrides source

    PowerSource src;
    QCOMPARE(src.name, QString("Source"));
    QCOMPARE(src.type, int(PowerSource::Distro));
    QCOMPARE(src.voltage, 120.0);
    QCOMPARE(src.isUPS(), false);
    QCOMPARE(src.isWallSocket(), false);

    PowerDistribution pd;
    QCOMPARE(pd.sources().count(), 0);
}

void PowerDistribution_Test::sourcesAndCircuits()
{
    PowerDistribution pd;

    PowerSource src;
    src.name = "Distro A";
    pd.sources().append(src);
    QCOMPARE(pd.sources().count(), 1);

    PowerCircuit c1;
    c1.name = "Circuit 1";
    PowerCircuit c2;
    c2.name = "Circuit 2";
    pd.sources()[0].circuits.append(c1);
    pd.sources()[0].circuits.append(c2);
    QCOMPARE(pd.sources()[0].circuits.count(), 2);

    pd.reset();
    QCOMPARE(pd.sources().count(), 0);
}

void PowerDistribution_Test::assignUnassignFixture()
{
    PowerDistribution pd;
    PowerSource src;
    src.circuits.append(PowerCircuit());
    src.circuits.append(PowerCircuit());
    pd.sources().append(src);

    int s = -1, c = -1;
    pd.circuitOf(42, s, c);
    QCOMPARE(s, -1);
    QCOMPARE(c, -1);

    pd.assignFixture(42, 0, 1);
    pd.circuitOf(42, s, c);
    QCOMPARE(s, 0);
    QCOMPARE(c, 1);
    QVERIFY(pd.sources()[0].circuits[1].fixtures.contains(42));
    QVERIFY(pd.sources()[0].circuits[0].fixtures.contains(42) == false);

    pd.unassignFixture(42);
    pd.circuitOf(42, s, c);
    QCOMPARE(s, -1);
    QCOMPARE(c, -1);
    QVERIFY(pd.sources()[0].circuits[1].fixtures.contains(42) == false);

    // Unassigning something never assigned is a safe no-op.
    pd.unassignFixture(999);

    // Assigning to an out-of-range source is a safe no-op.
    pd.assignFixture(42, 5, 0);
    pd.circuitOf(42, s, c);
    QCOMPARE(s, -1);
}

void PowerDistribution_Test::reassignMovesFixture()
{
    // A fixture already on one circuit, assigned to another, ends up on
    // exactly the new one — not both.
    PowerDistribution pd;
    PowerSource src;
    src.circuits.append(PowerCircuit());
    src.circuits.append(PowerCircuit());
    pd.sources().append(src);

    pd.assignFixture(7, 0, 0);
    pd.assignFixture(7, 0, 1);

    QVERIFY(pd.sources()[0].circuits[0].fixtures.contains(7) == false);
    QVERIFY(pd.sources()[0].circuits[1].fixtures.contains(7));
    int s = -1, c = -1;
    pd.circuitOf(7, s, c);
    QCOMPARE(s, 0);
    QCOMPARE(c, 1);
}

void PowerDistribution_Test::directSourceAutoCreatesCircuit()
{
    // A fresh source (wall socket / single-outlet UPS / distro with no
    // circuits yet) gets its first circuit created on demand when a fixture
    // is assigned directly to index 0 — the "(direct)" case in the
    // right-click/tree "Add to power circuit" UI.
    PowerDistribution pd;
    PowerSource src;
    src.name = "Wall Socket";
    src.type = PowerSource::WallSocket;
    pd.sources().append(src);

    QCOMPARE(pd.sources()[0].circuits.count(), 0);
    pd.assignFixture(3, 0, 0);
    QCOMPARE(pd.sources()[0].circuits.count(), 1);
    QVERIFY(pd.sources()[0].circuits[0].fixtures.contains(3));
}

void PowerDistribution_Test::xmlRoundTrip()
{
    PowerDistribution pd;

    PowerSource src;
    src.name = "House Distro";
    src.voltage = 208.0;
    PowerCircuit c1;
    c1.name = "SR Circuit";
    c1.ratedAmps = 20.0;
    c1.fixtures << 1 << 2;
    PowerCircuit c2;
    c2.name = "SL Circuit";
    c2.ratedAmps = 30.0;
    c2.fixtures << 3;
    src.circuits << c1 << c2;
    pd.sources().append(src);

    PowerSource ups;
    ups.name = "Backup UPS";
    ups.type = PowerSource::Battery;
    ups.vaRating = 1500.0;
    pd.sources().append(ups);

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    QVERIFY(pd.saveXML(&writer));
    writer.writeEndDocument();

    PowerDistribution pd2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
    {
        if (reader.name() == QStringLiteral("PowerDistribution"))
            QVERIFY(pd2.loadXML(reader, nullptr));
        else
            reader.skipCurrentElement();
    }

    QCOMPARE(pd2.sources().count(), 2);
    QCOMPARE(pd2.sources()[0].name, QString("House Distro"));
    QCOMPARE(pd2.sources()[0].voltage, 208.0);
    QCOMPARE(pd2.sources()[0].circuits.count(), 2);
    QCOMPARE(pd2.sources()[0].circuits[0].name, QString("SR Circuit"));
    QCOMPARE(pd2.sources()[0].circuits[0].ratedAmps, 20.0);
    QCOMPARE(pd2.sources()[0].circuits[0].fixtures, (QList<quint32>{1, 2}));
    QCOMPARE(pd2.sources()[0].circuits[1].fixtures, (QList<quint32>{3}));
    QCOMPARE(pd2.sources()[1].name, QString("Backup UPS"));
    QCOMPARE(pd2.sources()[1].isUPS(), true);
    QCOMPARE(pd2.sources()[1].vaRating, 1500.0);

    // An empty model writes nothing (no <PowerDistribution> element at all)
    // and a fresh load of that is simply a no-op, not an error state.
    PowerDistribution empty;
    QByteArray buf2;
    QXmlStreamWriter w2(&buf2);
    w2.writeStartDocument();
    QVERIFY(empty.saveXML(&w2));
    w2.writeEndDocument();
    QVERIFY(!buf2.contains("PowerDistribution"));
}


/* A 40-pixel RGB bar with no power in its definition (the V-Show PL-4003 in
   DanceHysteria): 120 colour channels. At 40 W a channel it was 4.8 kW a bar,
   and 32 of them at a 35% blue read 173 A on the status bar. */
void PowerDistribution_Test::aPixelBarWithNoPowerIsGuessedPerFixtureNotPerChannel()
{
    Doc doc(this);
    QScopedPointer<QLCFixtureDef> def(makeDef("Pixel Bar", QLCFixtureDef::LEDBarPixels, 0, 40, kRGB));
    Fixture *bar = place(&doc, def.data(), 0);
    QCOMPARE(bar->channels(), quint32(120));

    QCOMPARE(PowerEstimator::basis(bar), PowerEstimator::Guessed);
    QCOMPARE(PowerEstimator::fixtureFullWatts(bar), PowerEstimator::FallbackFixtureWatts);

    // Blue at 121 on every pixel: a third of the colours at 47%.
    PowerEstimator::PreGMReader blue = [](quint32, quint32 a) -> uchar { return (a % 3 == 2) ? 121 : 0; };
    const double frac = 121.0 / 255.0 / 3.0;
    QCOMPARE(PowerEstimator::intensityFraction(bar, blue), frac);
    const double expect = PowerEstimator::NonMoverStandbyWatts
            + (PowerEstimator::FallbackFixtureWatts - PowerEstimator::NonMoverStandbyWatts) * frac;
    QVERIFY(qAbs(PowerEstimator::fixtureWatts(bar, blue) - expect) < 1e-9);
    QVERIFY(PowerEstimator::fixtureWatts(bar, blue) < 10.0);
}

void PowerDistribution_Test::guessesForMoversDimmerPacksAndUnlitFixtures()
{
    Doc doc(this);
    QScopedPointer<QLCFixtureDef> mover(makeDef("Mover", QLCFixtureDef::MovingHead, 0, 1,
                                                { QLCChannel::IntensityRed, QLCChannel::IntensityGreen },
                                                QLCChannel::IntensityDimmer));
    QScopedPointer<QLCFixtureDef> pack(makeDef("Pack", QLCFixtureDef::Dimmer, 0, 6,
                                               { QLCChannel::IntensityDimmer }));
    QScopedPointer<QLCFixtureDef> fan(makeDef("Fan", QLCFixtureDef::Fan, 0, 1,
                                              { QLCChannel::SpeedPanSlowFast }));
    Fixture *m = place(&doc, mover.data(), 0);
    Fixture *p = place(&doc, pack.data(), 10);
    Fixture *f = place(&doc, fan.data(), 20);

    QCOMPARE(PowerEstimator::fixtureFullWatts(m), PowerEstimator::FallbackMoverWatts);
    // A dimmer pack's channels are each a load of their own.
    QCOMPARE(PowerEstimator::fixtureFullWatts(p), 6 * PowerEstimator::FallbackWattsPerDimmerChannel);
    QCOMPARE(PowerEstimator::basis(p), PowerEstimator::Guessed);
    // Nothing that lights: nothing guessed.
    QCOMPARE(PowerEstimator::basis(f), PowerEstimator::NoLoad);
    QCOMPARE(PowerEstimator::fixtureFullWatts(f), 0.0);
    PowerEstimator::PreGMReader full = [](quint32, quint32) -> uchar { return 255; };
    QCOMPARE(PowerEstimator::fixtureWatts(f, full), 0.0);
}

void PowerDistribution_Test::aRatedFixtureScalesWithItsLevel()
{
    Doc doc(this);
    QScopedPointer<QLCFixtureDef> par(makeDef("Par", QLCFixtureDef::ColorChanger, 100, 1, kRGB,
                                              QLCChannel::IntensityDimmer));
    Fixture *fx = place(&doc, par.data(), 0);
    QCOMPARE(PowerEstimator::basis(fx), PowerEstimator::Rated);
    QCOMPARE(PowerEstimator::fixtureFullWatts(fx), 100.0);

    PowerEstimator::PreGMReader dark = [](quint32, quint32) -> uchar { return 0; };
    PowerEstimator::PreGMReader full = [](quint32, quint32) -> uchar { return 255; };
    // Dimmer at 48 (19%), colours full.
    PowerEstimator::PreGMReader low = [](quint32, quint32 a) -> uchar { return a == 0 ? 48 : 255; };
    QCOMPARE(PowerEstimator::fixtureWatts(fx, dark), PowerEstimator::NonMoverStandbyWatts);
    QCOMPARE(PowerEstimator::fixtureWatts(fx, full), 100.0);
    const double expect = 2.0 + 98.0 * 48.0 / 255.0;
    QVERIFY(qAbs(PowerEstimator::fixtureWatts(fx, low) - expect) < 1e-9);
}

void PowerDistribution_Test::theLoadGroupsByTypeBiggestFirst()
{
    Doc doc(this);
    QScopedPointer<QLCFixtureDef> par(makeDef("Par", QLCFixtureDef::ColorChanger, 100, 1, kRGB,
                                              QLCChannel::IntensityDimmer));
    QScopedPointer<QLCFixtureDef> bar(makeDef("Bar", QLCFixtureDef::LEDBarPixels, 0, 8, kRGB));
    QHash<quint32, double> watts, amps;
    for (int i = 0; i < 3; ++i)
    {
        Fixture *fx = place(&doc, par.data(), quint32(i * 4));
        watts.insert(fx->id(), 60.0);
        amps.insert(fx->id(), 0.5);
    }
    for (int i = 0; i < 2; ++i)
    {
        Fixture *fx = place(&doc, bar.data(), quint32(100 + i * 24));
        watts.insert(fx->id(), 120.0);
        amps.insert(fx->id(), 1.0);
    }

    const QList<PowerEstimator::TypeLoad> rows = PowerEstimator::byType(&doc, watts, amps);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).name, QString("Test Bar"));
    QCOMPARE(rows.at(0).count, 2);
    QCOMPARE(rows.at(0).watts, 240.0);
    QCOMPARE(rows.at(0).amps, 2.0);
    QCOMPARE(rows.at(0).basis, PowerEstimator::Guessed);
    QCOMPARE(rows.at(0).fullWatts, PowerEstimator::FallbackFixtureWatts);
    QCOMPARE(rows.at(1).name, QString("Test Par"));
    QCOMPARE(rows.at(1).count, 3);
    QCOMPARE(rows.at(1).watts, 180.0);
    QCOMPARE(rows.at(1).ratedWatts, 100.0);
    QCOMPARE(rows.at(1).basis, PowerEstimator::Rated);
}

static PowerSource distro()
{
    PowerSource src;
    src.name = "House Distro";
    src.voltage = 208.0;
    PowerCircuit c1;
    c1.name = "SR Circuit";
    c1.ratedAmps = 20.0;
    c1.fixtures << 1 << 2;
    PowerCircuit c2;
    c2.name = "SL Circuit";
    c2.ratedAmps = 30.0;
    c2.fixtures << 3;
    src.circuits << c1 << c2;
    return src;
}

void PowerDistribution_Test::venueFileCarriesCircuitsNotFixtures()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("house.venue");

    Doc house(this);
    house.powerDistribution()->sources().append(distro());
    QVERIFY(Venue::exportToFile(&house, path, Venue::AllSections));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray xml = f.readAll();
    f.close();
    QVERIFY(xml.contains("<Venue Version=\"1\">"));
    QVERIFY(xml.contains("House Distro"));
    QVERIFY(xml.contains("SL Circuit"));
    QCOMPARE(Venue::sectionsInFile(path), QStringList({ "Power distribution" }));

    /* Into another show that has its own distro with fixtures on it: the
       venue's circuits replace it, and none of the venue show's fixture ids
       come along (they mean other fixtures here). */
    Doc show(this);
    PowerSource own;
    own.name = "Tour Distro";
    own.circuits << PowerCircuit();
    own.circuits[0].fixtures << 7;
    show.powerDistribution()->sources().append(own);
    show.resetModified();
    QVERIFY(Venue::importFromFile(&show, path));
    QVERIFY(show.isModified());
    QList<PowerSource> &got = show.powerDistribution()->sources();
    QCOMPARE(got.count(), 1);
    QCOMPARE(got[0].name, QString("House Distro"));
    QCOMPARE(got[0].voltage, 208.0);
    QCOMPARE(got[0].circuits.count(), 2);
    QCOMPARE(got[0].circuits[0].name, QString("SR Circuit"));
    QCOMPARE(got[0].circuits[0].ratedAmps, 20.0);
    QCOMPARE(got[0].circuits[1].ratedAmps, 30.0);
    QVERIFY(got[0].circuits[0].fixtures.isEmpty());
    QVERIFY(got[0].circuits[1].fixtures.isEmpty());

    // No sections: a venue file with nothing in it, which applies nothing
    QVERIFY(Venue::exportToFile(&house, path, 0));
    QVERIFY(Venue::sectionsInFile(path).isEmpty());
    show.resetModified();
    QVERIFY(!Venue::importFromFile(&show, path));
    QVERIFY(!show.isModified());
    QCOMPARE(show.powerDistribution()->sources().count(), 1);
}

void PowerDistribution_Test::venueFileRefusesWhatIsNotAVenue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Doc show(this);
    show.powerDistribution()->sources().append(distro());
    show.resetModified();

    QVERIFY(!Venue::exportToFile(NULL, dir.filePath("x.venue"), Venue::AllSections));
    QVERIFY(!Venue::importFromFile(NULL, dir.filePath("x.venue")));

    // Nowhere to write, nothing to read
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("cannot write venue file"));
    QVERIFY(!Venue::exportToFile(&show, dir.filePath("no/such/dir/x.venue"), Venue::AllSections));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("cannot read venue file"));
    QVERIFY(!Venue::importFromFile(&show, dir.filePath("missing.venue")));
    QVERIFY(Venue::sectionsInFile(dir.filePath("missing.venue")).isEmpty());

    // A show file is not a venue: nothing replaced
    const QString notVenue = dir.filePath("show.venue");
    QFile f(notVenue);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("<?xml version=\"1.0\"?><Workspace><PowerDistribution><PowerSource Name=\"X\"/>"
            "</PowerDistribution></Workspace>");
    f.close();
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("is not a venue file"));
    QVERIFY(!Venue::importFromFile(&show, notVenue));
    QVERIFY(Venue::sectionsInFile(notVenue).isEmpty());
    QVERIFY(!show.isModified());
    QCOMPARE(show.powerDistribution()->sources().count(), 1);
    QCOMPARE(show.powerDistribution()->sources()[0].name, QString("House Distro"));

    // A venue with a section this version doesn't know: skipped, the rest read
    Doc later(this);
    PowerSource l = distro();
    l.name = "Later";
    later.powerDistribution()->sources().append(l);
    QVERIFY(Venue::exportToFile(&later, notVenue, Venue::AllSections));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray xml = f.readAll();
    f.close();
    const int at = xml.indexOf("<PowerDistribution");
    QVERIFY(at > 0);
    xml.insert(at, "<Trusses><Truss Name=\"Upstage\"/></Trusses>");
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(xml);
    f.close();
    QCOMPARE(Venue::sectionsInFile(notVenue), QStringList({ "Power distribution" }));
    QVERIFY(Venue::importFromFile(&show, notVenue));
    QCOMPARE(show.powerDistribution()->sources().count(), 1);
    QCOMPARE(show.powerDistribution()->sources()[0].name, QString("Later"));
    QCOMPARE(show.powerDistribution()->sources()[0].circuits.count(), 2);
}

QTEST_MAIN(PowerDistribution_Test)

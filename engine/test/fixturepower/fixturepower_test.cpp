/*
  Q Light Controller Plus - Test Unit
  fixturepower_test.cpp

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
#undef private
#undef protected
#include "qlcfixturehead.h"
#include "qlcfixturemode.h"
#include "inputoutputmap.h"
#include "fixturepower_test.h"
#include "qlcfixturedef.h"
#include "genericfader.h"
#include "powerlimiter.h"
#include "fixturepower.h"
#include "fadechannel.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

static bool near(double a, double b, double eps = 0.01) { return qAbs(a - b) < eps; }

/* Like the DS1t wall-washer bars in their 64-channel mode: sixteen RGB
   pixels, then sixteen white ones. */
static QLCFixtureDef *barDef(QLCFixtureMode **modeOut)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Bar");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("64ch");
    int n = 0;
    auto add = [&](QLCChannel::Preset preset) {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("ch%1").arg(n));
        ch->setPreset(preset);
        def->addChannel(ch);
        mode->insertChannel(ch, quint32(n));
        return n++;
    };
    for (int p = 0; p < 16; ++p)
    {
        QLCFixtureHead h;
        h.addChannel(quint32(add(QLCChannel::IntensityRed)));
        h.addChannel(quint32(add(QLCChannel::IntensityGreen)));
        h.addChannel(quint32(add(QLCChannel::IntensityBlue)));
        mode->insertHead(-1, h);
    }
    for (int p = 0; p < 16; ++p)
    {
        QLCFixtureHead h;
        h.addChannel(quint32(add(QLCChannel::IntensityWhite)));
        mode->insertHead(-1, h);
    }
    def->addMode(mode);
    *modeOut = mode;
    return def;
}

/* Readings as off a clamp meter: red alone 2 A, green and blue 1.4, white
   1.5, everything at half 1.7, off 0.1 -- 6 A at full. */
static PowerProfile barProfile()
{
    PowerProfile pp;
    pp.colourAmps.insert(int(QLCChannel::Red), 2.0);
    pp.colourAmps.insert(int(QLCChannel::Green), 1.4);
    pp.colourAmps.insert(int(QLCChannel::Blue), 1.4);
    pp.colourAmps.insert(int(QLCChannel::White), 1.5);
    pp.halfAmps = 1.7;
    pp.standbyAmps = 0.1;
    return pp;
}

static int colourOf(const QLCFixtureMode *mode, int offset)
{
    return int(mode->channels().at(offset)->colour());
}

void FixturePower_Test::aBarsDrawIsModelledFromItsReadings()
{
    QLCFixtureMode *mode = nullptr;
    QScopedPointer<QLCFixtureDef> def(barDef(&mode));
    Doc doc(this);
    Fixture *bar = new Fixture(&doc);
    bar->setFixtureDefinition(def.data(), mode);
    QVERIFY(doc.addFixture(bar));
    const PowerModel m = PowerModel::build(bar, barProfile());
    QVERIFY(m.isValid());
    QCOMPARE(m.emitters.size(), 64);
    // 1.6 of 5.9 at half: the curve bends at about 1.88.
    QVERIFY2(near(m.gamma, 1.883, 0.005), qPrintable(QString::number(m.gamma)));

    auto at = [&](std::function<uchar(int)> f) { return m.amps(f); };
    QVERIFY(near(at([](int) { return uchar(0); }), 0.1));
    QVERIFY(near(at([](int) { return uchar(255); }), 6.0));
    QVERIFY(near(at([](int) { return uchar(128); }), 1.7, 0.02));
    // Red alone at full: its reading.
    QVERIFY(near(at([&](int o) { return colourOf(mode, o) == QLCChannel::Red ? uchar(255) : uchar(0); }), 2.0));
    // Half its pixels red: half red's share.
    QVERIFY(near(at([&](int o) { return (o < 24 && colourOf(mode, o) == QLCChannel::Red) ? uchar(255) : uchar(0); }),
                 0.1 + 0.95));
    bar->setFixtureDefinition(nullptr, nullptr);
}

void FixturePower_Test::theCapScalesTheColoursTogether()
{
    QLCFixtureMode *mode = nullptr;
    QScopedPointer<QLCFixtureDef> def(barDef(&mode));
    Doc doc(this);
    Fixture *bar = new Fixture(&doc);
    bar->setFixtureDefinition(def.data(), mode);
    QVERIFY(doc.addFixture(bar));
    PowerProfile pp = barProfile();
    pp.maxAmps = 3.0;
    const PowerModel m = PowerModel::build(bar, pp);
    // Full white on all of it: 6 A against 3 -- every emitter to about 69%,
    // and that is exactly 3 A.
    const double k = m.limitFactor(6.0);
    QVERIFY2(near(k, 0.686, 0.005), qPrintable(QString::number(k)));
    QVERIFY(near(m.amps([k](int) { return uchar(qRound(255 * k)); }), 3.0, 0.03));
    // Red alone at 2 A is already within it: untouched.
    QCOMPARE(m.limitFactor(2.0), 1.0);
    // A cap under standby can only be met dark.
    pp.maxAmps = 0.05;
    QCOMPARE(PowerModel::build(bar, pp).limitFactor(1.0), 0.0);
    bar->setFixtureDefinition(nullptr, nullptr);
}

void FixturePower_Test::withoutEveryColourItFallsBackSafely()
{
    QLCFixtureMode *mode = nullptr;
    QScopedPointer<QLCFixtureDef> def(barDef(&mode));
    Doc doc(this);
    Fixture *bar = new Fixture(&doc);
    bar->setFixtureDefinition(def.data(), mode);
    QVERIFY(doc.addFixture(bar));
    // Nothing measured: nothing to go on.
    QVERIFY(!PowerModel::build(bar, PowerProfile()).isValid());
    // Only everything at full: shared evenly by the four colours.
    PowerProfile full;
    full.fullAmps = 6.0;
    const PowerModel m = PowerModel::build(bar, full);
    QVERIFY(m.isValid());
    QVERIFY(near(m.amps([](int) { return uchar(255); }), 6.0));
    QVERIFY(near(m.amps([&](int o) { return colourOf(mode, o) == QLCChannel::Red ? uchar(255) : uchar(0); }), 1.5));
    // Red measured and the rest not: the rest of the full reading shared by them.
    PowerProfile some;
    some.fullAmps = 6.0;
    some.colourAmps.insert(int(QLCChannel::Red), 3.0);
    const PowerModel m2 = PowerModel::build(bar, some);
    QVERIFY(near(m2.amps([&](int o) { return colourOf(mode, o) == QLCChannel::Green ? uchar(255) : uchar(0); }), 1.0));
    bar->setFixtureDefinition(nullptr, nullptr);
}

void FixturePower_Test::aPlainDimmerIsItsLamp()
{
    Doc doc(this);
    Fixture *dimmer = new Fixture(&doc);
    dimmer->setChannels(1);
    QVERIFY(doc.addFixture(dimmer));
    PowerProfile pp;
    pp.colourAmps.insert(int(QLCChannel::NoColour), 4.0);
    pp.maxAmps = 2.0;
    const PowerModel m = PowerModel::build(dimmer, pp);
    QVERIFY(m.isValid());
    QCOMPARE(m.emitters.size(), 1);
    QVERIFY(near(m.amps([](int) { return uchar(255); }), 4.0));
    QVERIFY(near(m.limitFactor(4.0), 0.5));          // linear without a half reading
}

void FixturePower_Test::theProfileIsSavedWithTheFixture()
{
    QLCFixtureMode *mode = nullptr;
    QScopedPointer<QLCFixtureDef> def(barDef(&mode));
    Doc doc(this);
    Fixture *bar = new Fixture(&doc);
    bar->setFixtureDefinition(def.data(), mode);
    QVERIFY(doc.addFixture(bar));
    PowerProfile pp = barProfile();
    pp.maxAmps = 3.0;
    pp.fullAmps = 6.0;
    bar->setPowerProfile(pp);

    QByteArray buf;
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    QVERIFY(bar->saveXML(&w));
    w.writeEndDocument();
    QVERIFY(buf.contains("<Power"));
    QXmlStreamReader r(buf);
    r.readNextStartElement();
    PowerProfile back;
    while (!r.atEnd())
    {
        if (r.isStartElement() && r.name() == KXMLFixturePower)
        {
            QVERIFY(back.loadXML(r));
            break;
        }
        r.readNext();
    }
    QVERIFY(back == pp);

    // An unmeasured fixture with no cap writes nothing.
    PowerProfile none;
    QByteArray b2;
    QXmlStreamWriter w2(&b2);
    none.saveXML(&w2);
    QVERIFY(b2.isEmpty());
    bar->setFixtureDefinition(nullptr, nullptr);
}

void FixturePower_Test::aUniverseHoldsAFixtureToItsMaximum()
{
    QLCFixtureMode *mode = nullptr;
    QScopedPointer<QLCFixtureDef> def(barDef(&mode));
    Doc doc(this);
    Fixture *bar = new Fixture(&doc);
    bar->setFixtureDefinition(def.data(), mode);
    bar->setAddress(10);
    QVERIFY(doc.addFixture(bar));
    PowerProfile pp = barProfile();
    pp.maxAmps = 3.0;
    bar->setPowerProfile(pp);
    doc.powerLimiter()->rebuild();
    QCOMPARE(doc.powerLimiter()->limitedCount(), 1);

    QList<Universe *> ua = doc.inputOutputMap()->claimUniverses();
    Universe *u = ua.first();
    QSharedPointer<GenericFader> fader = u->requestFader(Universe::Override);
    auto setAll = [&](std::function<uchar(int)> f) {
        for (int o = 0; o < 64; ++o)
        {
            GenericFader::ChannelRef fc = fader->getChannelFader(&doc, u, Fixture::invalidId(), quint32(10 + o));
            fc->setCurrent(f(o));
            fc->setTarget(f(o));
            fc->addFlag(FadeChannel::Override);
        }
    };
    QSignalSpy spy(u, &Universe::universeWritten);

    // Everything at full: sent at about 69%, every channel alike.
    setAll([](int) { return uchar(255); });
    u->processFaders();
    QVERIFY(spy.count() >= 1);
    QByteArray sent = spy.last().at(1).toByteArray();
    QVERIFY(sent.size() >= 74);
    const int v = uchar(sent.at(10));
    QVERIFY2(v >= 172 && v <= 176, qPrintable(QString::number(v)));
    for (int o = 0; o < 64; ++o)
        QCOMPARE(int(uchar(sent.at(10 + o))), v);
    QVERIFY(uchar(u->postGMValues()->at(10)) == 255);   // what was asked is kept
    QHash<quint32, QPair<double, double> > held = u->powerHeldBack();
    QVERIFY(held.contains(bar->id()));
    QVERIFY(near(held.value(bar->id()).first, 6.0) && near(held.value(bar->id()).second, 3.0, 0.03));

    // Red alone: within it, sent as asked.
    setAll([&](int o) { return colourOf(mode, o) == QLCChannel::Red ? uchar(255) : uchar(0); });
    u->processFaders();
    sent = spy.last().at(1).toByteArray();
    QCOMPARE(int(uchar(sent.at(10))), 255);
    QVERIFY(u->powerHeldBack().isEmpty());

    // Being measured: let through as asked.
    setAll([](int) { return uchar(255); });
    doc.inputOutputMap()->releaseUniverses(false);
    doc.powerLimiter()->setMeasuring(bar->id(), true);
    ua = doc.inputOutputMap()->claimUniverses();
    u->processFaders();
    sent = spy.last().at(1).toByteArray();
    QCOMPARE(int(uchar(sent.at(10))), 255);
    u->dismissFader(fader);
    doc.inputOutputMap()->releaseUniverses(false);
    bar->setFixtureDefinition(nullptr, nullptr);
}

QTEST_MAIN(FixturePower_Test)

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

QTEST_MAIN(ShutterAutoOpen_Test)

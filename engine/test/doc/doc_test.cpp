/*
  Q Light Controller Plus - Unit test
  doc_test.cpp

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

#include <QPointer>
#include <QtTest>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#define protected public
#define private public

#include "qlcfixturedefcache.h"
#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "scriptwrapper.h"
#include "qlcphysical.h"
#include "collection.h"
#include "qlcchannel.h"
#include "sequence.h"
#include "qlcfile.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "efx.h"
#include "bus.h"
#include "inputoutputmap.h"
#include "doc.h"

#include "doc_test.h"

#undef protected
#undef private

#include "../common/resource_paths.h"

void Doc_Test::initTestCase()
{
    Bus::init(this);

    m_doc = new Doc(this);

    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir) == true);
    m_currentAddr = 0;
}

void Doc_Test::init()
{
}

void Doc_Test::cleanup()
{
    QSignalSpy spy1(m_doc, SIGNAL(clearing()));
    QSignalSpy spy2(m_doc, SIGNAL(cleared()));
    m_doc->clearContents();
    m_currentAddr = 0;
    QCOMPARE(spy1.size(), 1);
    QCOMPARE(spy2.size(), 1);
    QCOMPARE(m_doc->functions().size(), 0);
    QCOMPARE(m_doc->fixtures().size(), 0);
    QCOMPARE(m_doc->fixtureGroups().size(), 0);
    QCOMPARE(m_doc->m_latestFunctionId, quint32(0));
    QCOMPARE(m_doc->m_latestFixtureId, quint32(0));
    QCOMPARE(m_doc->m_latestFixtureGroupId, quint32(0));
    QCOMPARE(m_doc->m_addresses.size(), 0);
}

void Doc_Test::normalizeComponentPath()
{
     m_doc->setWorkspacePath(QDir("../../../resources/icons/png").absolutePath());

     QCOMPARE(m_doc->normalizeComponentPath(QString()), QString());
     QCOMPARE(m_doc->normalizeComponentPath("qlcconsole.png"), QFileInfo("qlcconsole.png").absoluteFilePath());
     QCOMPARE(m_doc->normalizeComponentPath("../../../resources/icons/png/qlcconsole.png"), QString("qlcconsole.png"));
     QCOMPARE(m_doc->normalizeComponentPath("../../../resources/icons/png/sub/qlcconsole.png"), QString("sub/qlcconsole.png"));
     // An absolute path outside the workspace comes back unchanged. Spelled
     // through QFileInfo: on Windows "/home/..." is only drive-relative.
     QCOMPARE(m_doc->normalizeComponentPath("/home/user/test.png"), QFileInfo("/home/user/test.png").absoluteFilePath());
}

void Doc_Test::denormalizeComponentPath()
{
     m_doc->setWorkspacePath(QDir("../../../resources/icons/png").absolutePath());

     QCOMPARE(m_doc->denormalizeComponentPath(QString()), QString());
     QCOMPARE(m_doc->denormalizeComponentPath("qlcconsole.png"), QFileInfo("../../../resources/icons/png/qlcconsole.png").absoluteFilePath());
     QCOMPARE(m_doc->denormalizeComponentPath("sub/qlcconsole.png"), QFileInfo("../../../resources/icons/png/sub/qlcconsole.png").absoluteFilePath());
     // An absolute path outside the workspace comes back unchanged. Spelled
     // through QFileInfo: on Windows "/home/..." is only drive-relative.
     QCOMPARE(m_doc->denormalizeComponentPath("/home/user/test.png"), QFileInfo("/home/user/test.png").absoluteFilePath());
}

void Doc_Test::defaults()
{
    QVERIFY(m_doc->m_fixtureDefCache != NULL);
    QVERIFY(m_doc->m_modifiersCache != NULL);
    QVERIFY(m_doc->m_audioPluginCache != NULL);
    QVERIFY(m_doc->m_ioMap != NULL);
    QVERIFY(m_doc->m_masterTimer != NULL);
    QVERIFY(m_doc->m_clipboard != NULL);

    QVERIFY(m_doc->fixtureDefCache() != NULL);
    QVERIFY(m_doc->modifiersCache() != NULL);
    QVERIFY(m_doc->audioPluginCache() != NULL);
    QVERIFY(m_doc->ioPluginCache() != NULL);
    QVERIFY(m_doc->masterTimer() != NULL);
    QVERIFY(m_doc->clipboard() != NULL);

    QVERIFY(m_doc->m_loadStatus == Doc::Cleared);
    QVERIFY(m_doc->loadStatus() == Doc::Cleared);
    QVERIFY(m_doc->errorLog() == QString());
    QVERIFY(m_doc->m_modified == false);
    QVERIFY(m_doc->m_latestFixtureId == 0);
    QVERIFY(m_doc->m_fixtures.size() == 0);
    QVERIFY(m_doc->m_latestFunctionId == 0);
    QVERIFY(m_doc->m_functions.size() == 0);
    QVERIFY(m_doc->isKiosk() == false);
}

void Doc_Test::mode()
{
    QSignalSpy spy(m_doc, SIGNAL(modeChanged(Doc::Mode)));
    QCOMPARE(m_doc->mode(), Doc::Design);

    m_doc->setMode(Doc::Operate);
    QCOMPARE(spy.size(), 1);
    m_doc->setMode(Doc::Operate);
    QCOMPARE(spy.size(), 1);
    m_doc->setMode(Doc::Design);
    QCOMPARE(spy.size(), 2);
    m_doc->setMode(Doc::Design);
    QCOMPARE(spy.size(), 2);

    m_doc->setKiosk(true);
    QVERIFY(m_doc->isKiosk() == true);
}

void Doc_Test::createFixtureId()
{
    for (quint32 i = 0; i < 16384; i++)
    {
        quint32 id = m_doc->createFixtureId();
        m_doc->m_fixtures[i] = new Fixture(m_doc);
        m_doc->m_fixtures[i]->setID(id);
        QCOMPARE(id, i);
    }
}

void Doc_Test::addFixture()
{
    QVERIFY(m_doc->isModified() == false);
    QSignalSpy spy(m_doc, SIGNAL(fixtureAdded(quint32)));

    /* Add a completely new fixture */
    Fixture* f1 = new Fixture(m_doc);
    f1->setName("One");
    f1->setChannels(5);
    f1->setAddress(m_currentAddr);
    f1->setUniverse(0);

    QVERIFY(m_doc->addFixture(f1) == true);
    m_currentAddr += f1->channels();
    QVERIFY(f1->id() == 0);
    QVERIFY(m_doc->isModified() == true);
    QVERIFY(spy.size() == 1);
    QVERIFY(spy.at(0).at(0) == f1->id());

    m_doc->resetModified();

    /* Add another fixture but attempt to put assign it an already-assigned
       fixture ID. */
    Fixture* f2 = new Fixture(m_doc);
    f2->setName("Two");
    f2->setChannels(5);
    f2->setAddress(m_currentAddr);
    f2->setUniverse(0);
    QVERIFY(m_doc->addFixture(f2, f1->id()) == false);
    QVERIFY(m_doc->isModified() == false);
    QVERIFY(spy.size() == 1);
    QVERIFY(spy.at(0).at(0) == f1->id());

    /* But, the fixture can be added if we give it an unassigned ID. */
    QVERIFY(m_doc->addFixture(f2, f1->id() + 1) == true);
    m_currentAddr += f2->channels();
    QVERIFY(f1->id() == 0);
    QVERIFY(f2->id() == 1);
    QVERIFY(m_doc->isModified() == true);
    QVERIFY(spy.size() == 2);
    QVERIFY(spy.at(1).at(0) == f2->id());

    m_doc->resetModified();

    /* Add again a completely new fixture, with automatic ID assignment */
    Fixture* f3 = new Fixture(m_doc);
    f3->setName("Three");
    f3->setChannels(5);
    f3->setAddress(f2->address());
    f3->setUniverse(0);
    QVERIFY(m_doc->addFixture(f3) == false); // cannot assign the same address as f2
    f3->setAddress(m_currentAddr);
    QVERIFY(m_doc->addFixture(f3) == true);
    m_currentAddr += f3->channels();
    QVERIFY(f1->id() == 0);
    QVERIFY(f2->id() == 1);
    QVERIFY(f3->id() == 2);
    QVERIFY(m_doc->isModified() == true);
    QVERIFY(spy.size() == 3);
    QVERIFY(spy.at(2).at(0) == f3->id());
}

void Doc_Test::addFixturePastUniverseLimit()
{
    const quint32 max = InputOutputMap::maxUniverses();

    Fixture *past = new Fixture(m_doc);
    past->setName("Past");
    past->setChannels(4);
    past->setUniverse(max);
    past->setAddress(0);
    QVERIFY(m_doc->addFixture(past) == false);
    QCOMPARE(m_doc->fixtures().size(), 0);
    QVERIFY(m_doc->inputOutputMap()->universesCount() <= max);
    delete past;

    /* Starting on the last universe but running over its end into one the
       show can't have: refused too. */
    Fixture *over = new Fixture(m_doc);
    over->setName("Over");
    over->setChannels(20);
    over->setUniverse(max - 1);
    over->setAddress(500);
    QVERIFY(m_doc->addFixture(over, Fixture::invalidId(), true) == false);
    QCOMPARE(m_doc->fixtures().size(), 0);
    delete over;

    /* Wholly on the last universe: fine. */
    Fixture *last = new Fixture(m_doc);
    last->setName("Last");
    last->setChannels(4);
    last->setUniverse(max - 1);
    last->setAddress(508);
    QVERIFY(m_doc->addFixture(last) == true);
    QCOMPARE(last->universe(), max - 1);
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), max);

    /* An unpatched (inventory) fixture claims no universe: it goes in. */
    Fixture *staged = new Fixture(m_doc);
    staged->setName("Staged");
    staged->setChannels(4);
    staged->setUniverse(max + 5);
    staged->setPatched(false);
    QVERIFY(m_doc->addFixture(staged) == true);
}

void Doc_Test::moveFixturePastUniverseLimit()
{
    Fixture *f = new Fixture(m_doc);
    f->setName("Mover");
    f->setChannels(4);
    f->setUniverse(2);
    f->setAddress(10);
    QVERIFY(m_doc->addFixture(f) == true);

    f->setUniverse(InputOutputMap::maxUniverses() + 3);
    QCOMPARE(f->universe(), quint32(2));
    QCOMPARE(f->address(), quint32(10));
    QVERIFY(f->isPatched());
    QVERIFY(m_doc->inputOutputMap()->universesCount() <= InputOutputMap::maxUniverses());
}

void Doc_Test::readdressHighUniverseFixture()
{
    Fixture *f = new Fixture(m_doc);
    f->setName("High");
    f->setChannels(4);
    f->setUniverse(129);
    f->setAddress(0);
    QVERIFY(m_doc->addFixture(f) == true);
    QCOMPARE(f->universe(), quint32(129));

    f->setAddress(100);
    QCOMPARE(f->universe(), quint32(129));
    QCOMPARE(f->address(), quint32(100));
    /* Doc follows it there: the channels it now claims are on 129. */
    Fixture *probe = new Fixture(m_doc);
    probe->setName("Probe");
    probe->setChannels(4);
    probe->setUniverse(1);
    probe->setAddress(100);
    QVERIFY(m_doc->addFixture(probe) == true);   // u2 100 is free: f didn't land there
}

void Doc_Test::deleteFixture()
{
    QSignalSpy spy(m_doc, SIGNAL(fixtureRemoved(quint32)));

    QVERIFY(m_doc->fixtures().size() == 0);
    QVERIFY(m_doc->deleteFixture(0) == false);
    QVERIFY(m_doc->fixtures().size() == 0);
    QVERIFY(m_doc->deleteFixture(1) == false);
    QVERIFY(m_doc->fixtures().size() == 0);
    QVERIFY(m_doc->deleteFixture(Fixture::invalidId()) == false);
    QVERIFY(m_doc->fixtures().size() == 0);

    Fixture* f1 = new Fixture(m_doc);
    f1->setName("One");
    f1->setChannels(5);
    f1->setAddress(m_currentAddr);
    f1->setUniverse(0);
    m_doc->addFixture(f1);
    m_currentAddr += f1->channels();

    Fixture* f2 = new Fixture(m_doc);
    f2->setName("Two");
    f2->setChannels(5);
    f2->setAddress(m_currentAddr);
    f2->setUniverse(0);
    m_doc->addFixture(f2);
    m_currentAddr += f2->channels();

    Fixture* f3 = new Fixture(m_doc);
    f3->setName("Three");
    f3->setChannels(5);
    f3->setAddress(m_currentAddr);
    f3->setUniverse(0);
    m_doc->addFixture(f3);
    m_currentAddr += f3->channels();

    QVERIFY(m_doc->isModified() == true);
    m_doc->resetModified();

    // Nonexistent ID
    QVERIFY(m_doc->deleteFixture(42) == false);
    QVERIFY(m_doc->fixtures().size() == 3);
    QVERIFY(m_doc->isModified() == false);

    // Invalid ID
    QVERIFY(m_doc->deleteFixture(Fixture::invalidId()) == false);
    QVERIFY(m_doc->fixtures().size() == 3);
    QVERIFY(m_doc->isModified() == false);

    // Existing ID
    quint32 id = f2->id();
    QPointer <Fixture> f2ptr(f2);
    QVERIFY(f2ptr != NULL);
    QVERIFY(m_doc->deleteFixture(id) == true);
    QVERIFY(m_doc->fixtures().size() == 2);
    QVERIFY(m_doc->isModified() == true);
    QVERIFY(spy.size() == 1);
    QVERIFY(spy.at(0).at(0) == id);
    QVERIFY(f2ptr == NULL);

    // The same ID we just removed
    QVERIFY(m_doc->deleteFixture(id) == false);
    QVERIFY(m_doc->fixtures().size() == 2);
    QVERIFY(m_doc->isModified() == true);
    QVERIFY(spy.size() == 1);
    QVERIFY(spy.at(0).at(0) == id);

    m_doc->resetModified();

    // Another ID just for repetition
    id = f1->id();
    QPointer <Fixture> f1ptr(f1);
    QVERIFY(f1ptr != NULL);
    QVERIFY(m_doc->deleteFixture(id) == true);
    QVERIFY(m_doc->fixtures().size() == 1);
    QVERIFY(m_doc->isModified() == true);
    QVERIFY(spy.size() == 2);
    QVERIFY(spy.at(1).at(0) == id);
    QVERIFY(f1ptr == NULL);

    // And the last one...
    id = f3->id();
    QPointer <Fixture> f3ptr(f3);
    QVERIFY(f3ptr != NULL);
    QVERIFY(m_doc->deleteFixture(id) == true);
    QVERIFY(m_doc->fixtures().size() == 0);
    QVERIFY(m_doc->isModified() == true);
    QVERIFY(spy.size() == 3);
    QVERIFY(spy.at(2).at(0) == id);
    QVERIFY(f3ptr == NULL);
}

void Doc_Test::replaceFixtures()
{
    Fixture *f1 = new Fixture(m_doc);
    f1->setName("One");
    f1->setChannels(5);
    f1->setAddress(m_currentAddr);
    f1->setUniverse(0);
    m_doc->addFixture(f1);
    m_currentAddr += f1->channels();

    Fixture *f2 = new Fixture(m_doc);
    f2->setName("Two");
    f2->setChannels(5);
    f2->setAddress(m_currentAddr);
    f2->setUniverse(0);
    m_doc->addFixture(f2);
    m_currentAddr += f2->channels();

    Fixture *f3 = new Fixture(m_doc);
    f3->setName("Three");
    f3->setChannels(5);
    f3->setAddress(m_currentAddr);
    f3->setUniverse(0);
    m_doc->addFixture(f3);
    m_currentAddr += f3->channels();

    QVERIFY(m_doc->fixtures().count() == 3);
    QVERIFY(m_doc->fixture(f1->id()) == f1);
    QVERIFY(m_doc->fixture(f2->id()) == f2);
    QVERIFY(m_doc->fixture(f3->id()) == f3);

    Fixture *f4 = new Fixture(m_doc);
    f4->setName("Four");
    f4->setID(0);

    QLCFixtureDef *fixtureDef;
    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Showtec", "MiniMax 250");
    Q_ASSERT(fixtureDef != NULL);
    QLCFixtureMode *fixtureMode;
    fixtureMode = fixtureDef->modes().at(0);
    Q_ASSERT(fixtureMode != NULL);
    f4->setFixtureDefinition(fixtureDef, fixtureMode);
    f4->setAddress(100);
    f4->setUniverse(0);

    Fixture *f5 = new Fixture(m_doc);
    f5->setName("Five");
    f5->setID(1);
    f5->setChannels(5);
    f5->setAddress(200);
    f2->setUniverse(0);

    QList<Fixture *> newFixtures;
    newFixtures << f4 << f5;

    QVERIFY(m_doc->replaceFixtures(newFixtures) == true);

    QVERIFY(m_doc->fixtures().count() == 2);

    /* check that a copy of the new Fixtures has been made */
    QVERIFY(m_doc->fixture(f4->id()) != f4);
    QVERIFY(m_doc->fixture(f5->id()) != f5);

    QVERIFY(m_doc->fixture(f4->id())->name() == "Four");
    QVERIFY(m_doc->fixture(f5->id())->name() == "Five");
}

void Doc_Test::fixture()
{
    Fixture *f1 = new Fixture(m_doc);
    f1->setName("One");
    f1->setChannels(5);
    f1->setAddress(m_currentAddr);
    f1->setUniverse(0);
    m_doc->addFixture(f1);
    m_currentAddr += f1->channels();

    Fixture *f2 = new Fixture(m_doc);
    f2->setName("Two");
    f2->setChannels(5);
    f2->setAddress(m_currentAddr);
    f2->setUniverse(0);
    m_doc->addFixture(f2);
    m_currentAddr += f2->channels();

    Fixture *f3 = new Fixture(m_doc);
    f3->setName("Three");
    f3->setChannels(5);
    f3->setAddress(m_currentAddr);
    f3->setUniverse(0);
    m_doc->addFixture(f3);
    m_currentAddr += f3->channels();

    QVERIFY(m_doc->fixture(f1->id()) == f1);
    QVERIFY(m_doc->fixture(f2->id()) == f2);
    QVERIFY(m_doc->fixture(f3->id()) == f3);
    QVERIFY(m_doc->fixture(f3->id() + 1) == NULL);
    QVERIFY(m_doc->fixture(42) == NULL);
    QVERIFY(m_doc->fixture(Fixture::invalidId()) == NULL);

    QLCFixtureDef *fixtureDef;
    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Showtec", "MiniMax 250");
    Q_ASSERT(fixtureDef != NULL);
    QLCFixtureMode *fixtureMode;
    fixtureMode = fixtureDef->modes().at(0);
    Q_ASSERT(fixtureMode != NULL);

    /* test forced HTP/LTP channels */
    Fixture *f4 = new Fixture(m_doc);
    f4->setName("Four");
    f4->setAddress(m_currentAddr);
    f4->setUniverse(0);
    f4->setFixtureDefinition(fixtureDef, fixtureMode);
    m_doc->addFixture(f4);
    m_currentAddr += f4->channels();

    QVERIFY(m_doc->fixtures().count() == 4);
    QList<int>forcedHTP;
    QList<int>forcedLTP;

    // force color and gobo channel to HTP
    forcedHTP << 3 << 4;
    // force strobe/dimmer to LTP
    forcedLTP << 5;

    QVERIFY(m_doc->updateFixtureChannelCapabilities(42, forcedHTP, forcedLTP) == false);
    QVERIFY(m_doc->updateFixtureChannelCapabilities(f4->id(), forcedHTP, forcedLTP) == true);

    QVERIFY(f4->forcedHTPChannels().count() == 2);
    QVERIFY(f4->forcedLTPChannels().count() == 1);
}

void Doc_Test::totalPowerConsumption()
{
    int fuzzy = 0;

    /* Load Showtec - MiniMax 250 with 250W power consumption */
    QLCFixtureDef *fixtureDef;
    fixtureDef = m_doc->fixtureDefCache()->fixtureDef("Showtec", "MiniMax 250");
    Q_ASSERT(fixtureDef != NULL);
    QLCFixtureMode *fixtureMode;
    fixtureMode = fixtureDef->modes().at(0);
    Q_ASSERT(fixtureMode != NULL);

    /* Add a new fixture */
    Fixture *f1 = new Fixture(m_doc);
    f1->setName("250W (total 250W)");
    f1->setChannels(6);
    f1->setAddress(0);
    f1->setUniverse(0);
    f1->setFixtureDefinition(fixtureDef, fixtureMode);
    QVERIFY(f1->fixtureDef() == fixtureDef);
    QVERIFY(f1->fixtureMode() == fixtureMode);
    QVERIFY(f1->fixtureMode()->physical().powerConsumption() == 250);
    QVERIFY(m_doc->addFixture(f1) == true);
    QVERIFY(m_doc->totalPowerConsumption(fuzzy) == 250);
    QVERIFY(fuzzy == 0);

    /* Add the same fixture once more */
    Fixture *f2 = new Fixture(m_doc);
    f2->setName("250W (total 500W)");
    f2->setChannels(6);
    f2->setAddress(10);
    f2->setUniverse(0);
    f2->setFixtureDefinition(fixtureDef, fixtureMode);
    QVERIFY(f2->fixtureDef() == fixtureDef);
    QVERIFY(f2->fixtureMode() == fixtureMode);
    QVERIFY(f2->fixtureMode()->physical().powerConsumption() == 250);
    QVERIFY(m_doc->addFixture(f2) == true);
    QVERIFY(m_doc->totalPowerConsumption(fuzzy) == 500);
    QVERIFY(fuzzy == 0);

    /* Test generic dimmer and fuzzy */
    Fixture *f3 = new Fixture(m_doc);
    f3->setName("Generic Dimmer");
    f3->setChannels(6);
    f3->setAddress(20);
    f3->setUniverse(0);
    QVERIFY(m_doc->addFixture(f3) == true);
    QVERIFY(m_doc->totalPowerConsumption(fuzzy) == 500);
    QVERIFY(fuzzy == 1);
    // reset fuzzy count
    fuzzy = 0;

    /* Test fuzzy count */
    Fixture *f4 = new Fixture(m_doc);
    f4->setName("Generic Dimmer 2");
    f4->setChannels(6);
    f4->setAddress(30);
    f4->setUniverse(0);
    QVERIFY(m_doc->addFixture(f4) == true);
    QVERIFY(m_doc->totalPowerConsumption(fuzzy) == 500);
    QVERIFY(fuzzy == 2);
}

void Doc_Test::addFixtureGroup()
{
    QSignalSpy spy(m_doc, SIGNAL(fixtureGroupAdded(quint32)));

    QCOMPARE(m_doc->fixtureGroups().size(), 0);
    QCOMPARE(m_doc->m_latestFixtureGroupId, quint32(0));

    FixtureGroup *grp = new FixtureGroup(m_doc);
    QCOMPARE(m_doc->addFixtureGroup(grp), true);
    QCOMPARE(grp->id(), quint32(0));
    QCOMPARE(m_doc->m_latestFixtureGroupId, quint32(0));
    QCOMPARE(m_doc->fixtureGroups().size(), 1);
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy[0].size(), 1);
    QCOMPARE(spy[0][0].toUInt(), quint32(0));

    QCOMPARE(m_doc->addFixtureGroup(grp, 0), false);
    QCOMPARE(m_doc->fixtureGroups().size(), 1);

    grp = new FixtureGroup(m_doc);
    QCOMPARE(m_doc->addFixtureGroup(grp, 0), false);
    QCOMPARE(m_doc->addFixtureGroup(grp, 15), true);
    QCOMPARE(m_doc->m_latestFixtureGroupId, quint32(0));
    QCOMPARE(m_doc->fixtureGroups().size(), 2);
    QCOMPARE(spy.size(), 2);
    QCOMPARE(spy[1].size(), 1);
    QCOMPARE(spy[1][0].toUInt(), quint32(15));

    grp = new FixtureGroup(m_doc);
    QCOMPARE(m_doc->addFixtureGroup(grp), true);
    QCOMPARE(grp->id(), quint32(1));
    QCOMPARE(m_doc->m_latestFixtureGroupId, quint32(1));
    QCOMPARE(m_doc->fixtureGroups().size(), 3);
    QCOMPARE(spy.size(), 3);
    QCOMPARE(spy[2].size(), 1);
    QCOMPARE(spy[2][0].toUInt(), quint32(1));
}

void Doc_Test::removeFixtureGroup()
{
    QSignalSpy spy(m_doc, SIGNAL(fixtureGroupRemoved(quint32)));
    QCOMPARE(m_doc->fixtureGroups().size(), 0);
    QCOMPARE(m_doc->m_latestFixtureGroupId, quint32(0));

    QCOMPARE(m_doc->deleteFixtureGroup(0), false);
    QCOMPARE(spy.size(), 0);

    FixtureGroup* grp = new FixtureGroup(m_doc);
    QCOMPARE(m_doc->addFixtureGroup(grp), true);
    grp = new FixtureGroup(m_doc);
    QCOMPARE(m_doc->addFixtureGroup(grp), true);
    grp = new FixtureGroup(m_doc);
    QCOMPARE(m_doc->addFixtureGroup(grp), true);
    QCOMPARE(m_doc->fixtureGroups().size(), 3);
    QVERIFY(m_doc->fixtureGroup(0) != NULL);
    QVERIFY(m_doc->fixtureGroup(1) != NULL);
    QVERIFY(m_doc->fixtureGroup(2) != NULL);

    QCOMPARE(m_doc->deleteFixtureGroup(0), true);
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy[0].size(), 1);
    QCOMPARE(spy[0][0].toUInt(), quint32(0));
    QCOMPARE(m_doc->fixtureGroups().size(), 2);
    QVERIFY(m_doc->fixtureGroup(0) == NULL);
    QVERIFY(m_doc->fixtureGroup(1) != NULL);
    QVERIFY(m_doc->fixtureGroup(2) != NULL);

    QCOMPARE(m_doc->deleteFixtureGroup(0), false);
    QCOMPARE(spy.size(), 1);

    QCOMPARE(m_doc->deleteFixtureGroup(1), true);
    QCOMPARE(spy.size(), 2);
    QCOMPARE(spy[1].size(), 1);
    QCOMPARE(spy[1][0].toUInt(), quint32(1));
    QCOMPARE(m_doc->fixtureGroups().size(), 1);
    QVERIFY(m_doc->fixtureGroup(0) == NULL);
    QVERIFY(m_doc->fixtureGroup(1) == NULL);
    QVERIFY(m_doc->fixtureGroup(2) != NULL);
}

void Doc_Test::channelGroups()
{
    QSignalSpy spy(m_doc, SIGNAL(channelsGroupAdded(quint32)));
    QSignalSpy spy2(m_doc, SIGNAL(channelsGroupRemoved(quint32)));

    QVERIFY(m_doc->channelsGroups().count() == 0);

    ChannelsGroup *group = new ChannelsGroup(m_doc);
    ChannelsGroup *group2 = new ChannelsGroup(m_doc);
    ChannelsGroup *group3 = new ChannelsGroup(m_doc);

    /* Add a new channel group */
    QVERIFY(m_doc->addChannelsGroup(group) == true);
    QVERIFY(m_doc->channelsGroups().at(0)->id() == 0);
    QVERIFY(m_doc->channelsGroups().count() == 1);
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy[0].size(), 1);
    QCOMPARE(spy[0][0].toUInt(), quint32(0));

    QVERIFY(m_doc->addChannelsGroup(group2) == true);
    QVERIFY(m_doc->channelsGroups().at(0)->id() == 0);
    QVERIFY(m_doc->channelsGroups().at(1)->id() == 1);
    QVERIFY(m_doc->channelsGroups().count() == 2);
    QCOMPARE(spy.size(), 2);
    QCOMPARE(spy[1].size(), 1);
    QCOMPARE(spy[1][0].toUInt(), quint32(1));

    /* delete an invalid channel group */
    QVERIFY(m_doc->deleteChannelsGroup(42) == false);
    QVERIFY(m_doc->channelsGroups().count() == 2);

    /* delete a valid channel group */
    QVERIFY(m_doc->deleteChannelsGroup(0) == true);
    QVERIFY(m_doc->channelsGroups().count() == 1);
    QVERIFY(m_doc->channelsGroups().at(0)->id() == 1);
    QCOMPARE(spy2.size(), 1);
    QCOMPARE(spy2[0].size(), 1);
    QCOMPARE(spy2[0][0].toUInt(), quint32(0));

    /* get an invalid channel group */
    QVERIFY(m_doc->channelsGroup(42) == NULL);

    /* get a valid channel group */
    QVERIFY(m_doc->channelsGroup(1) == group2);

    QVERIFY(m_doc->addChannelsGroup(group3) == true);
    QVERIFY(m_doc->channelsGroups().count() == 2);
    QCOMPARE(spy.size(), 3);
    QCOMPARE(spy[2].size(), 1);
    QCOMPARE(spy[2][0].toUInt(), quint32(2));

    QVERIFY(m_doc->channelsGroups().at(1)->id() == 2);

    /* attempt to perform an invalid move */
    QVERIFY(m_doc->moveChannelGroup(42, 0) == false);
    QVERIFY(m_doc->moveChannelGroup(2, -100) == false);

    /* do a valid move */
    QVERIFY(m_doc->moveChannelGroup(2, -1) == true);
    QVERIFY(m_doc->channelsGroups().count() == 2);
    QVERIFY(m_doc->channelsGroups().at(0)->id() == 2);
    QVERIFY(m_doc->channelsGroups().at(1)->id() == 1);
}

void Doc_Test::palettes()
{
    QSignalSpy spy(m_doc, SIGNAL(paletteAdded(quint32)));
    QSignalSpy spy2(m_doc, SIGNAL(paletteRemoved(quint32)));

    QVERIFY(m_doc->palettes().count() == 0);

    QLCPalette *p1 = new QLCPalette(QLCPalette::Color);
    QLCPalette *p2 = new QLCPalette(QLCPalette::PanTilt);

    QVERIFY(m_doc->addPalette(p1) == true);
    QVERIFY(m_doc->palettes().count() == 1);

    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy[0].size(), 1);
    QCOMPARE(spy[0][0].toUInt(), quint32(0));

    QVERIFY(m_doc->addPalette(p2) == true);
    QVERIFY(m_doc->palettes().count() == 2);

    QCOMPARE(spy.size(), 2);
    QCOMPARE(spy[1].size(), 1);
    QCOMPARE(spy[1][0].toUInt(), quint32(1));

    /* get an invalid palette */
    QVERIFY(m_doc->palette(42) == NULL);

    /* get a valid palette */
    QVERIFY(m_doc->palette(1) == p2);

    /* delete an invalid palette */
    QVERIFY(m_doc->deletePalette(42) == false);
    QVERIFY(m_doc->palettes().count() == 2);

    /* delete a valid palette */
    QVERIFY(m_doc->deletePalette(0) == true);
    QVERIFY(m_doc->palettes().count() == 1);
    QVERIFY(m_doc->palettes().at(0)->id() == 1);
    QCOMPARE(spy2.size(), 1);
    QCOMPARE(spy2[0].size(), 1);
    QCOMPARE(spy2[0][0].toUInt(), quint32(0));

    QVERIFY(m_doc->deletePalette(1) == true);
    QVERIFY(m_doc->palettes().count() == 0);

    QCOMPARE(spy2.size(), 2);
    QCOMPARE(spy2[1].size(), 1);
    QCOMPARE(spy2[1][0].toUInt(), quint32(1));
}

void Doc_Test::monitorProperties()
{
    Fixture *f1 = new Fixture(m_doc);
    f1->setName("One");
    f1->setChannels(2);
    f1->setAddress(0);
    f1->setUniverse(0);
    m_doc->addFixture(f1);

    Chaser *c = new Chaser(m_doc);
    m_doc->addFunction(c);

    Fixture *f2 = new Fixture(m_doc);
    f2->setName("Two");
    f2->setChannels(1);
    f2->setAddress(20);
    f2->setUniverse(1);
    m_doc->addFixture(f2);

    Collection *o = new Collection(m_doc);
    m_doc->addFunction(o);

    Fixture *f3 = new Fixture(m_doc);
    f3->setName("Three");
    f3->setChannels(1);
    f3->setAddress(40);
    f3->setUniverse(2);
    m_doc->addFixture(f3);

    MonitorProperties *props = m_doc->monitorProperties();
    props->setFixturePosition(f1->id(), 0, 0, QVector3D(0, 0, 0));
    props->setFixturePosition(f2->id(), 0, 0, QVector3D(300, 0, 0));
    props->setFixturePosition(f3->id(), 0, 0, QVector3D(600, 0, 0));
}

void Doc_Test::addFunction()
{
    QVERIFY(m_doc->nextFunctionID() == 0);
    QVERIFY(m_doc->functions().size() == 0);

    Scene* s = new Scene(m_doc);
    QVERIFY(s->id() == Function::invalidId());
    QVERIFY(m_doc->addFunction(s) == true);
    QVERIFY(s->id() == 0);
    QVERIFY(m_doc->functions().size() == 1);
    QVERIFY(m_doc->isModified() == true);

    m_doc->resetModified();

    Chaser* c = new Chaser(m_doc);
    QVERIFY(c->id() == Function::invalidId());
    QVERIFY(m_doc->addFunction(c) == true);
    QVERIFY(c->id() == 1);
    QVERIFY(m_doc->functions().size() == 2);
    QVERIFY(m_doc->isModified() == true);

    m_doc->resetModified();

    Collection* o = new Collection(m_doc);
    QVERIFY(o->id() == Function::invalidId());
    QVERIFY(m_doc->addFunction(o, 0) == false);
    QVERIFY(m_doc->isModified() == false);
    QVERIFY(o->id() == Function::invalidId());
    QVERIFY(m_doc->functions().size() == 2);
    QVERIFY(m_doc->addFunction(o, 2) == true);
    QVERIFY(o->id() == 2);
    QVERIFY(m_doc->functions().size() == 3);
    QVERIFY(m_doc->isModified() == true);

    m_doc->resetModified();

    EFX* e = new EFX(m_doc);
    QVERIFY(e->id() == Function::invalidId());
    QVERIFY(m_doc->addFunction(e, 1) == false);
    QVERIFY(e->id() == Function::invalidId());
    QVERIFY(m_doc->addFunction(e) == true);
    QVERIFY(e->id() == 3);
    QVERIFY(m_doc->functions().size() == 4);
    QVERIFY(m_doc->isModified() == true);

    QVERIFY(m_doc->nextFunctionID() == 4);
}

void Doc_Test::deleteFunction()
{
    Scene *s1 = new Scene(m_doc);
    m_doc->addFunction(s1);

    Scene *s2 = new Scene(m_doc);
    m_doc->addFunction(s2);

    Scene *s3 = new Scene(m_doc);
    m_doc->addFunction(s3);

    m_doc->resetModified();

    QPointer <Scene> ptr(s2);
    QVERIFY(ptr != NULL);
    quint32 id = s2->id();
    QVERIFY(m_doc->deleteFunction(id) == true);
    QVERIFY(m_doc->isModified() == true);

    m_doc->resetModified();

    QVERIFY(m_doc->deleteFunction(id) == false);
    QVERIFY(m_doc->deleteFunction(42) == false);
    QVERIFY(m_doc->isModified() == false);
    QVERIFY(ptr == NULL); // m_doc->deleteFunction() should also delete
    QVERIFY(m_doc->m_fixtures.contains(id) == false);

    id = s1->id();
    QVERIFY(m_doc->deleteFunction(id) == true);
    QVERIFY(m_doc->m_fixtures.contains(id) == false);
    QVERIFY(m_doc->isModified() == true);

    id = s3->id();
    QVERIFY(m_doc->deleteFunction(id) == true);
    QVERIFY(m_doc->m_fixtures.contains(id) == false);
    QVERIFY(m_doc->isModified() == true);

    QVERIFY(m_doc->functions().size() == 0);
}

void Doc_Test::function()
{
    Scene *s1 = new Scene(m_doc);
    m_doc->addFunction(s1);

    Scene *s2 = new Scene(m_doc);
    m_doc->addFunction(s2);

    Scene *s3 = new Scene(m_doc);
    m_doc->addFunction(s3);

    QVERIFY(m_doc->function(s1->id()) == s1);
    QVERIFY(m_doc->function(s2->id()) == s2);
    QVERIFY(m_doc->function(s3->id()) == s3);

    quint32 id = s2->id();
    QVERIFY(m_doc->deleteFunction(id) == true);
    QVERIFY(m_doc->function(id) == NULL);

    m_doc->setStartupFunction(s1->id());
    QVERIFY(m_doc->startupFunction() == s1->id());
}

void Doc_Test::usage()
{
    Scene *s1 = new Scene(m_doc);
    m_doc->addFunction(s1);

    Scene *s2 = new Scene(m_doc);
    m_doc->addFunction(s2);

    Scene *s3 = new Scene(m_doc);
    m_doc->addFunction(s3);

    Scene *s4 = new Scene(m_doc);
    m_doc->addFunction(s4);

    Scene *s5 = new Scene(m_doc);
    m_doc->addFunction(s5);
    QVERIFY(m_doc->functions().count() == 5);

    Chaser *c1 = new Chaser(m_doc);
    ChaserStep cs1(s1->id());
    ChaserStep cs2(s5->id());
    c1->addStep(cs1);
    c1->addStep(cs2);
    QVERIFY(c1->stepsCount() == 2);
    m_doc->addFunction(c1);
    QVERIFY(m_doc->functions().count() == 6);


    Collection *col1 = new Collection(m_doc);
    col1->addFunction(s2->id());
    col1->addFunction(s5->id());
    QVERIFY(col1->functions().count() == 2);
    m_doc->addFunction(col1);
    QVERIFY(m_doc->functions().count() == 7);

    Sequence *seq1 = new Sequence(m_doc);
    seq1->setBoundSceneID(s4->id());
    m_doc->addFunction(seq1);
    QVERIFY(m_doc->functions().count() == 8);

    Script *sc1 = new Script(m_doc);
    sc1->appendData(QString("startfunction:%1").arg(c1->id()));
    m_doc->addFunction(sc1);
    QVERIFY(m_doc->functions().count() == 9);

    QList<quint32> usage;

    /* check the usage of an invalid ID */
    qDebug() << "Check the usage of an invalid ID";
    usage = m_doc->getUsage(100);
    QVERIFY(usage.count() == 0);

    /* check the usage of an unused function */
    qDebug() << "Check the usage of an unused function";
    usage = m_doc->getUsage(s3->id());
    QVERIFY(usage.count() == 0);

    /* check usage of a Scene used by a Chaser */
    qDebug() << "Check usage of a Scene used by a Chaser";
    usage = m_doc->getUsage(s1->id());
    QVERIFY(usage.count() == 2);
    QVERIFY(usage.at(0) == c1->id());
    QVERIFY(usage.at(1) == 0); // step 0

    /* check usage of a Scene used by a Sequence */
    qDebug() << "Check usage of a Scene used by a Sequence";
    usage = m_doc->getUsage(s4->id());
    QVERIFY(usage.count() == 2);
    QVERIFY(usage.at(0) == seq1->id());
    QVERIFY(usage.at(1) == 0); // no info

    /* check usage of a Scene used by a Collection */
    qDebug() << "Check usage of a Scene used by a Collection";
    usage = m_doc->getUsage(s2->id());
    QVERIFY(usage.count() == 2);
    QVERIFY(usage.at(0) == col1->id());
    QVERIFY(usage.at(1) == 0); // index 0

    /* check usage of a Chaser used by a Script */
    qDebug() << "Check usage of a Chaser used by a Script";
    usage = m_doc->getUsage(c1->id());
    QVERIFY(usage.count() == 2);
    QVERIFY(usage.at(0) == sc1->id());
    QVERIFY(usage.at(1) == 0); // line 1

    /* check usage of shared function */
    qDebug() << "Check usage of shared function";
    usage = m_doc->getUsage(s5->id());
    QVERIFY(usage.count() == 4);
    QVERIFY(usage.at(0) == c1->id());
    QVERIFY(usage.at(1) == 1); // step 1
    QVERIFY(usage.at(2) == col1->id());
    QVERIFY(usage.at(3) == 1); // index 1

    /* test also the function by type method */
    qDebug() << "Test also the function by type method";
    QList<Function *> byType = m_doc->functionsByType(Function::SceneType);
    QVERIFY(byType.count() == 5);
    QVERIFY(byType.at(0) == s1);
    QVERIFY(byType.at(1) == s2);
    QVERIFY(byType.at(2) == s3);
    QVERIFY(byType.at(3) == s4);
    QVERIFY(byType.at(4) == s5);
}

void Doc_Test::load()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Engine");

    createFixtureNode(xmlWriter, 0, m_currentAddr, 18);
    m_currentAddr += 18;
    createFixtureNode(xmlWriter, 72, m_currentAddr, 18);
    m_currentAddr += 18;
    createFixtureNode(xmlWriter, 15, m_currentAddr, 18);
    m_currentAddr += 18;

    createFixtureGroupNode(xmlWriter, 0);
    createFixtureGroupNode(xmlWriter, 42);
    createFixtureGroupNode(xmlWriter, 72);

    createCollectionNode(xmlWriter, 5);
    createCollectionNode(xmlWriter, 9);
    createCollectionNode(xmlWriter, 1);
    createCollectionNode(xmlWriter, 7);

    createBusNode(xmlWriter, 0, 1);
    createBusNode(xmlWriter, 7, 2);
    createBusNode(xmlWriter, 12, 3);
    createBusNode(xmlWriter, 29, 4);
    createBusNode(xmlWriter, 31, 500);

    xmlWriter.writeStartElement("ExtraTag");
    xmlWriter.writeEndElement();

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    QVERIFY(m_doc->fixtures().size() == 0);
    QVERIFY(m_doc->functions().size() == 0);
    QVERIFY(m_doc->loadXML(xmlReader) == true);
    QVERIFY(m_doc->loadStatus() == Doc::Loaded);
    QVERIFY(m_doc->fixtures().size() == 3);
    QVERIFY(m_doc->functions().size() == 4);
    QVERIFY(m_doc->fixtureGroups().size() == 3);
    QVERIFY(Bus::instance()->value(0) == 1);
    QVERIFY(Bus::instance()->value(7) == 2);
    QVERIFY(Bus::instance()->value(12) == 3);
    QVERIFY(Bus::instance()->value(29) == 4);
    QVERIFY(Bus::instance()->value(31) == 500);
}

void Doc_Test::loadTruncated()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartElement("Engine");
    for (quint32 id = 0; id < 6; id++)
        createFixtureNode(xmlWriter, id, id * 18, 18);
    createCollectionNode(xmlWriter, 1);
    createCollectionNode(xmlWriter, 2);
    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    // Cut in the middle of the fourth fixture.
    const QByteArray whole = buffer.data();
    int cut = 0;
    for (int i = 0; i < 4; i++)
        cut = whole.indexOf("<Fixture>", cut + 1);
    QVERIFY(cut > 0);
    QByteArray part = whole.left(cut + 40);

    QBuffer cutBuffer(&part);
    cutBuffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&cutBuffer);
    xmlReader.readNextStartElement();

    QVERIFY(m_doc->loadXML(xmlReader) == false);
    QVERIFY(xmlReader.hasError());
    QCOMPARE(m_doc->loadStatus(), Doc::Loaded);
    QCOMPARE(m_doc->fixtures().size(), 3);      // what came before the cut
    QCOMPARE(m_doc->functions().size(), 0);
    QVERIFY2(m_doc->errorLog().contains(QStringLiteral("damaged or incomplete (line %1")
                                            .arg(xmlReader.lineNumber())),
             qPrintable(m_doc->errorLog()));

    // The whole file: true, and nothing said.
    m_doc->clearContents();
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader wholeReader(&buffer);
    wholeReader.readNextStartElement();
    QVERIFY(m_doc->loadXML(wholeReader) == true);
    QCOMPARE(m_doc->fixtures().size(), 6);
    QVERIFY(!m_doc->errorLog().contains(QStringLiteral("damaged")));
}

void Doc_Test::loadUniverseOutOfRange_data()
{
    QTest::addColumn<QString>("universe");
    QTest::addColumn<bool>("patched");
    QTest::newRow("in range") << QStringLiteral("3") << true;
    QTest::newRow("the last one") << QString::number(InputOutputMap::maxUniverses() - 1) << true;
    QTest::newRow("the limit") << QString::number(InputOutputMap::maxUniverses()) << false;
    QTest::newRow("65536") << QStringLiteral("65536") << false;
    QTest::newRow("-1") << QStringLiteral("-1") << false;
    QTest::newRow("4294967295") << QStringLiteral("4294967295") << false;
    QTest::newRow("not a number") << QStringLiteral("abc") << false;
    QTest::newRow("empty") << QString() << false;
}

void Doc_Test::loadUniverseOutOfRange()
{
    QFETCH(QString, universe);
    QFETCH(bool, patched);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    xmlWriter.writeStartElement("Engine");
    createFixtureNode(xmlWriter, 0, 0, 4);
    createFixtureNode(xmlWriter, 1, 10, 4, universe);
    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    QElapsedTimer t;
    t.start();
    QVERIFY(m_doc->loadXML(xmlReader) == true);
    QVERIFY2(t.elapsed() < 20000, "a universe out of range must not hold the load up");

    QCOMPARE(m_doc->fixtures().size(), 2);
    Fixture *fxi = m_doc->fixture(1);
    QVERIFY(fxi != NULL);
    QCOMPARE(fxi->isPatched(), patched);
    QVERIFY(m_doc->inputOutputMap()->universesCount() <= InputOutputMap::maxUniverses());
    if (patched)
    {
        QCOMPARE(fxi->universe(), universe.toUInt());
        QVERIFY(!m_doc->errorLog().contains(QStringLiteral("names universe")));
    }
    else
    {
        QCOMPARE(fxi->universe(), 0u);
        QVERIFY2(m_doc->errorLog().contains(QStringLiteral("names universe \"%1\"").arg(universe)),
                 qPrintable(m_doc->errorLog()));
    }
    // The other fixture is where it was.
    QVERIFY(m_doc->fixture(0)->isPatched());
    QCOMPARE(m_doc->fixture(0)->universe(), 3u);

    // Nor can a universe be added past the limit, by number or at the end.
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->addUniverse(InputOutputMap::maxUniverses()) == false);
    QVERIFY(iom->addUniverse(65536) == false);
    while (iom->universesCount() < InputOutputMap::maxUniverses())
        QVERIFY(iom->addUniverse());
    QVERIFY(iom->addUniverse() == false);
    QCOMPARE(iom->universesCount(), InputOutputMap::maxUniverses());

    // Back to the universes the other tests expect.
    m_doc->clearContents();
    while (iom->universesCount() > 4)
        QVERIFY(iom->removeUniverse(int(iom->universesCount()) - 1));
}

void Doc_Test::loadWrongRoot()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Enjine");

    createFixtureNode(xmlWriter, 0, m_currentAddr, 18);
    m_currentAddr += 18;
    createFixtureNode(xmlWriter, 72, m_currentAddr, 18);
    m_currentAddr += 18;
    createFixtureNode(xmlWriter, 15, m_currentAddr, 18);
    m_currentAddr += 18;

    createFixtureGroupNode(xmlWriter, 0);
    createFixtureGroupNode(xmlWriter, 42);
    createFixtureGroupNode(xmlWriter, 72);

    createCollectionNode(xmlWriter, 5);
    createCollectionNode(xmlWriter, 9);
    createCollectionNode(xmlWriter, 1);
    createCollectionNode(xmlWriter, 7);

    createBusNode(xmlWriter, 0, 1);
    createBusNode(xmlWriter, 7, 2);
    createBusNode(xmlWriter, 12, 3);
    createBusNode(xmlWriter, 29, 4);
    createBusNode(xmlWriter, 31, 500);

    xmlWriter.writeStartElement("ExtraTag");
    xmlWriter.writeEndElement();

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    QVERIFY(m_doc->loadXML(xmlReader) == false);
}

void Doc_Test::save()
{
    Scene* s = new Scene(m_doc);
    m_doc->addFunction(s);

    Fixture* f1 = new Fixture(m_doc);
    f1->setName("One");
    f1->setChannels(5);
    f1->setAddress(0);
    f1->setUniverse(0);
    m_doc->addFixture(f1);

    Chaser* c = new Chaser(m_doc);
    m_doc->addFunction(c);

    Fixture* f2 = new Fixture(m_doc);
    f2->setName("Two");
    f2->setChannels(10);
    f2->setAddress(20);
    f2->setUniverse(1);
    m_doc->addFixture(f2);

    Collection* o = new Collection(m_doc);
    m_doc->addFunction(o);

    Fixture* f3 = new Fixture(m_doc);
    f3->setName("Three");
    f3->setChannels(15);
    f3->setAddress(40);
    f3->setUniverse(2);
    m_doc->addFixture(f3);

    EFX* e = new EFX(m_doc);
    m_doc->addFunction(e);

    FixtureGroup* grp = new FixtureGroup(m_doc);
    grp->setName("Group 1");
    m_doc->addFixtureGroup(grp);

    grp = new FixtureGroup(m_doc);
    grp->setName("Group 2");
    m_doc->addFixtureGroup(grp);

    QVERIFY(m_doc->isModified() == true);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    xmlWriter.writeStartElement("TestRoot");

    QVERIFY(m_doc->saveXML(&xmlWriter) == true);

    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);

    xmlReader.readNextStartElement();
    QVERIFY(xmlReader.name().toString() == "TestRoot");
    xmlReader.readNextStartElement();
    QVERIFY(xmlReader.name().toString() == "Engine");

    uint fixtures = 0, groups = 0, functions = 0, ioMap = 0, monitor = 0, moveInBlack = 0;
    uint autoOpen = 0;

    // Merely tests that the start of each hierarchy is found from the XML document.
    // Their contents are tested individually in their own separate tests.
    while (xmlReader.readNextStartElement())
    {
        if (xmlReader.name().toString() == "Fixture")
            fixtures++;
        else if (xmlReader.name().toString() == "Function")
            functions++;
        else if (xmlReader.name().toString() == "FixtureGroup")
            groups++;
        else if (xmlReader.name().toString() == "InputOutputMap")
            ioMap++;
        else if (xmlReader.name().toString() == "Monitor")
            monitor++;
        else if (xmlReader.name().toString() == "MoveInBlack")
            // Programmer-mode Auto-MIB settings; always written since
            // ProgrammerController's MarkPlanner exists from construction.
            moveInBlack++;
        else if (xmlReader.name().toString() == "ShutterAutoOpen")
            // Always written, on or off: a show without it loads by who
            // saved it (ShutterAutoOpen::beginLoad()).
            autoOpen++;
        else if (xmlReader.name().toString() == "Bus")
            QFAIL("Bus tags should not be saved anymore!");
        else
            QFAIL(QString("Unexpected tag: %1")
                  .arg(xmlReader.name().toString()).toLatin1());

        xmlReader.skipCurrentElement();
    }

    QVERIFY(fixtures == 3);
    QVERIFY(groups == 2);
    QVERIFY(functions == 4);
    QVERIFY(ioMap == 1);
    QVERIFY(monitor == 1);
    QVERIFY(moveInBlack == 1);
    QVERIFY(autoOpen == 1);

    /* Saving doesn't implicitly reset modified status */
    QVERIFY(m_doc->isModified() == true);
}

void Doc_Test::createFixtureNode(QXmlStreamWriter &doc, quint32 id, quint32 address, quint32 channels,
                                 const QString &universe)
{
    doc.writeStartElement("Fixture");

    doc.writeTextElement("Channels", QString("%1").arg(channels));
    doc.writeTextElement("Name", QString("Fixture %1").arg(id));
    doc.writeTextElement("Universe", universe);
    doc.writeTextElement("Model", "Foobar");
    doc.writeTextElement("Mode", "Foobar");
    doc.writeTextElement("Manufacturer", "Foobar");
    doc.writeTextElement("ID", QString("%1").arg(id));
    doc.writeTextElement("Address", QString("%1").arg(address));

    /* End the <Fixture> tag */
    doc.writeEndElement();
}

void Doc_Test::createFixtureGroupNode(QXmlStreamWriter &doc, quint32 id)
{
    doc.writeStartElement("FixtureGroup");
    doc.writeAttribute("ID", QString::number(id));

    doc.writeTextElement("Name", QString("Group with ID %1").arg(id));

    /* End the <FixtureGroup> tag */
    doc.writeEndElement();
}

void Doc_Test::createCollectionNode(QXmlStreamWriter &doc, quint32 id)
{
    doc.writeStartElement("Function");
    doc.writeAttribute("Type", "Collection");
    doc.writeAttribute("ID", QString("%1").arg(id));

    doc.writeTextElement("Step", "50");
    doc.writeTextElement("Step", "12");
    doc.writeTextElement("Step", "87");

    /* End the <Function> tag */
    doc.writeEndElement();
}

void Doc_Test::createBusNode(QXmlStreamWriter &doc, quint32 id, quint32 val)
{
    // Used to test that loading legacy Bus tags won't screw up Doc
    doc.writeStartElement("Bus");
    doc.writeAttribute("ID", QString::number(id));

    doc.writeTextElement("Name", QString("Bus %1").arg(id));
    doc.writeTextElement("Value", QString("%1").arg(val));

    /* End the <Bus> tag */
    doc.writeEndElement();
}

/* "two AT1-3z lights .. one in upper left corner .. one on AT1": names are
   unique. Taken is checked against OTHER fixtures; a free name is numbered
   on without touching its own digits; a show that already has duplicates is
   put right with the patched one keeping its name. */
void Doc_Test::fixtureNamesAreUnique()
{
    auto add = [this](const QString &name, bool patched, quint32 addr) {
        Fixture *f = new Fixture(m_doc);
        f->setName(name);
        f->setChannels(1);
        f->setAddress(addr);
        f->setPatched(patched);
        m_doc->addFixture(f);
        return f;
    };
    Fixture *stray = add("AT1-3z", false, 0);    // the inventory copy, lower ID
    Fixture *rigged = add("AT1-3z", true, 447);
    Fixture *other = add("SR Tower 1", true, 10);

    QVERIFY(m_doc->fixtureNameTaken("AT1-3z"));
    QVERIFY(m_doc->fixtureNameTaken("AT1-3z", rigged->id()));     // the stray has it
    QVERIFY(!m_doc->fixtureNameTaken("SR Tower 1", other->id())); // only itself
    QVERIFY(!m_doc->fixtureNameTaken("Nobody"));

    // Numbered on, its own digits kept: not "AT1-4z", not "SR Tower 2".
    QCOMPARE(m_doc->uniqueFixtureName("AT1-3z"), QString("AT1-3z #2"));
    QCOMPARE(m_doc->uniqueFixtureName("SR Tower 1"), QString("SR Tower 1 #2"));
    QCOMPARE(m_doc->uniqueFixtureName("SR Tower 1", other->id()), QString("SR Tower 1"));
    QCOMPARE(m_doc->uniqueFixtureName("Free"), QString("Free"));

    const QMap<QString, QList<quint32> > dups = m_doc->duplicateFixtureNames();
    QCOMPARE(dups.keys(), QStringList({ "AT1-3z" }));
    QCOMPARE(dups.value("AT1-3z"), QList<quint32>({ stray->id(), rigged->id() }));

    // The patched one keeps the name though it was added second.
    QCOMPARE(m_doc->makeFixtureNamesUnique(), 1);
    QCOMPARE(rigged->name(), QString("AT1-3z"));
    QCOMPARE(stray->name(), QString("AT1-3z #2"));
    QVERIFY(m_doc->duplicateFixtureNames().isEmpty());
    QCOMPARE(m_doc->makeFixtureNamesUnique(), 0);
    m_doc->resetModified();        // the tests after this one expect a fresh doc
}

QTEST_APPLESS_MAIN(Doc_Test)

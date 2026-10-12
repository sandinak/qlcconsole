/*
  Q Light Controller Plus - Test Unit
  stagedoutput_test.cpp

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
#include <QJSEngine>

#define private public
#include "programmercontroller.h"
#include "programmerflasher.h"
#include "highlighteffect.h"
#include "effectinstance.h"
#include "universe.h"
#include "fadechannel.h"
#include "genericfader.h"
#include "parkeffect.h"
#include "markeffect.h"
#undef private

#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

#include "stagedoutput_test.h"

static Fixture *dimmer(Doc *doc, const QString &name, bool patched)
{
    Fixture *f = new Fixture(doc);
    QLCFixtureDef *def = f->genericDimmerDef(4);
    f->setFixtureDefinition(def, f->genericDimmerMode(def, 4));
    f->setName(name);
    f->setPatched(patched);
    if (!doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    return f;
}

/* Pan + a Dimmer outside any head, so the Dimmer is the master intensity
 * channel an effect's "dimmer" intent drives. */
static Fixture *mover(Doc *doc, const QString &name, bool patched)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("StagedMover");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    const QList<QPair<QString, QLCChannel::Group>> chans = {
        { "Pan", QLCChannel::Pan }, { "Dimmer", QLCChannel::Intensity } };
    for (const auto &c : chans)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(c.first);
        ch->setGroup(c.second);
        def->addChannel(ch);
        mode->insertChannel(ch, def->channels().size() - 1);
    }
    mode->cacheHeads();
    def->addMode(mode);

    Fixture *f = new Fixture(doc);
    f->setFixtureDefinition(def, mode);
    f->setName(name);
    f->setPatched(patched);
    if (!doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    return f;
}

static const QHash<quint32, uchar> kFull = { {0, 255}, {1, 255} };

void StagedOutput_Test::init()
{
    m_doc = new Doc(this);
    m_real = dimmer(m_doc, "Real", true);
    m_staged = dimmer(m_doc, "Staged", false);
    QVERIFY(m_real != nullptr && m_staged != nullptr);
    // Both claim 1/1 -- only one of them means it.
    QCOMPARE(m_staged->universe(), m_real->universe());
    QCOMPARE(m_staged->address(), m_real->address());
}

void StagedOutput_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void StagedOutput_Test::parkSkipsStagedFixtures()
{
    ParkEffect park(m_doc);
    park.parkFixture(m_staged->id(), kFull);
    QVERIFY(park.m_entries.isEmpty());

    park.parkFixture(m_real->id(), kFull);
    QCOMPARE(park.m_entries.size(), 1);
    QCOMPARE(park.m_entries.first().writes.size(), 2);
}

void StagedOutput_Test::aParkedFixtureReturnedToInventoryStopsWriting()
{
    ParkEffect park(m_doc);
    park.parkFixture(m_real->id(), kFull);
    QCOMPARE(park.m_entries.size(), 1);

    QVERIFY(m_doc->unpatchFixture(m_real->id()));
    park.rebuild();
    QVERIFY(park.m_entries.isEmpty());
    QVERIFY(park.isParked(m_real->id()));             // still parked, just nowhere to write
}

void StagedOutput_Test::markSkipsStagedFixtures()
{
    MarkEffect mark(m_doc);
    mark.markFixture(m_staged->id(), kFull);
    QVERIFY(mark.m_entries.isEmpty());

    mark.markFixture(m_real->id(), kFull);
    QCOMPARE(mark.m_entries.size(), 1);
}

void StagedOutput_Test::aMarkedFixtureReturnedToInventoryStopsWriting()
{
    MarkEffect mark(m_doc);
    mark.markFixture(m_real->id(), kFull);
    QCOMPARE(mark.m_entries.size(), 1);

    QVERIFY(m_doc->unpatchFixture(m_real->id()));
    mark.rebuild();
    QVERIFY(mark.m_entries.isEmpty());
}

void StagedOutput_Test::highlightSkipsStagedFixtures()
{
    HighlightEffect hl(m_doc);
    QVERIFY(hl.buildEntries({ m_staged->id() }).isEmpty());
    QCOMPARE(hl.buildEntries({ m_staged->id(), m_real->id() }).size(), 1);
}

void StagedOutput_Test::flashSkipsStagedFixtures()
{
    ProgrammerFlasher flasher(m_doc);
    flasher.flashFixture(m_staged->id(), 500);
    QVERIFY(flasher.m_entries.isEmpty());

    flasher.flashFixture(m_real->id(), 500);
    QCOMPARE(flasher.m_entries.size(), 1);
}

/* A staged fixture sends NOTHING -- that holds. What changed is where: an
   effect now works out what a staged fixture would do (so the rig view can
   show a look on the inventory before it is patched), and its channels are
   inert, so none of it reaches a universe. Checked through the very fader the
   effect runner writes with. */
void StagedOutput_Test::effectIntentsForStagedFixturesReachNoDmx()
{
    Fixture *realMover = mover(m_doc, "Real Mover", false);   // patched below, clear of m_real
    Fixture *stagedMover = mover(m_doc, "Staged Mover", false);
    QVERIFY(realMover != nullptr && stagedMover != nullptr);
    QVERIFY(m_doc->patchFixture(realMover->id(), 0, 10));
    QVERIFY(realMover->masterIntensityChannel() != QLCChannel::invalid());
    QVERIFY(!stagedMover->isPatched());

    EffectInstance inst(m_doc, Function::invalidId(), Function::invalidId());

    QJSEngine js;
    QJSValue intents = js.evaluate("[{ dimmer: 1.0 }, { dimmer: 1.0 }]");
    QVERIFY(intents.isArray());
    const QList<EffectInstance::EffectCell> cells = {
        { stagedMover->id(), 0, 0, 0 },
        { realMover->id(),   0, 1, 0 },
    };

    const QList<EffectInstance::DmxWrite> writes = inst.parseIntents(intents, cells);
    bool staged = false, real = false;
    for (const EffectInstance::DmxWrite &w : writes)
    {
        staged |= (w.fixtureId == stagedMover->id());
        real |= (w.fixtureId == realMover->id());
    }
    QVERIFY(real);
    QVERIFY2(staged, "the staged fixture's look is worked out, for the rig view");

    // Through the effect runner's own fader: the staged writes reach no DMX.
    QList<Universe *> ua = m_doc->inputOutputMap()->universes();
    QSharedPointer<GenericFader> fader = ua[0]->requestFader(Universe::Override);
    fader->setDoc(m_doc);
    for (const EffectInstance::DmxWrite &w : writes)
    {
        if (w.fixtureId != stagedMover->id())
            continue;
        FadeChannel fc(m_doc, w.fixtureId, w.channel);
        fc.setTarget(w.value);
        fc.setCurrent(w.value);
        fc.setFadeTime(0);
        fader->replace(fc);
    }
    fader->write(ua[0]);
    for (int a = 0; a < 10; ++a)                     // the placeholder 0/0 and on
        QCOMPARE(uchar(ua[0]->preGMValues()[a]), uchar(0));
    const quint32 dim = stagedMover->masterIntensityChannel();
    QVERIFY(dim != QLCChannel::invalid());
    QCOMPARE(uchar(stagedMover->channelValues().at(int(dim))), uchar(255));   // shown, not sent
    ua[0]->dismissFader(fader);
}

void StagedOutput_Test::programmerParkAndMarkSkipStagedFixtures()
{
    ProgrammerController *pc = m_doc->programmer();
    QVERIFY(pc != nullptr);

    pc->parkFixtures({ m_staged->id(), m_real->id() });
    QCOMPARE(pc->parkedFixtures(), QList<quint32>({ m_real->id() }));
    pc->unparkAllFixtures();

    pc->markFixtures({ m_staged->id(), m_real->id() });
    QCOMPARE(pc->markedFixtures(), QList<quint32>({ m_real->id() }));
    pc->unmarkAllFixtures();
}

void StagedOutput_Test::programmerLiveWritesSkipStagedFixtures()
{
    ProgrammerController *pc = m_doc->programmer();
    pc->writeChannelLive(m_staged->id(), 0, 255);
    QVERIFY(pc->m_liveFadersMap.isEmpty());

    pc->writeChannelLive(m_real->id(), 0, 255);
    QCOMPARE(pc->m_liveFadersMap.size(), 1);
}

QTEST_GUILESS_MAIN(StagedOutput_Test)

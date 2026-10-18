/*
  Q Light Controller Plus - Test Unit
  inventory_test.cpp

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

#include "fakenetplugin.h"

#include "patchplanner.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "universe.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "scene.h"
#include "scenevalue.h"
#include "qlcfixturehead.h"
#include "fixture.h"
#include "inputoutputmap.h"
#include "monitorproperties.h"
#include "networktopology.h"
#include "outputpatch.h"
#include "fixturegroup.h"
#include "doc.h"

#include "inventory_test.h"

static Fixture *makeDimmer(Doc *doc, int channels, quint32 universe, quint32 address,
                           bool patched = true)
{
    Fixture *fxi = new Fixture(doc);
    fxi->setChannels(channels);
    fxi->setUniverse(universe);
    fxi->setAddress(address);
    fxi->setPatched(patched);
    if (!doc->addFixture(fxi))
    {
        delete fxi;
        return nullptr;
    }
    return fxi;
}

// Doc lives per-test: the tests below all reason about who owns universe 1's
// first channels, so nothing may leak between them.
static Doc *g_doc = nullptr;

void Inventory_Test::init()
{
    g_doc = new Doc(this);
}

void Inventory_Test::cleanup()
{
    delete g_doc;
    g_doc = nullptr;
}

void Inventory_Test::defaultsAreAPatchedDmxFixture()
{
    // Every fixture in every existing file must load exactly as it always did.
    Fixture fxi(this);
    QVERIFY(fxi.isPatched());
    QCOMPARE(fxi.connection(), Fixture::Dmx);
    QVERIFY(fxi.ipAddress().isEmpty());
}

void Inventory_Test::manyUnpatchedFixturesCanCoexistAtTheDefaultAddress()
{
    // Each holds the placeholder 0/0. If addFixture() still checked overlap for
    // them, only the first could ever be added.
    Fixture *a = makeDimmer(g_doc, 8, 0, 0, false);
    Fixture *b = makeDimmer(g_doc, 8, 0, 0, false);
    Fixture *c = makeDimmer(g_doc, 192, 0, 0, false);
    QVERIFY(a != nullptr && b != nullptr && c != nullptr);
    QCOMPARE(g_doc->fixturesCount(), 3);
}

void Inventory_Test::unpatchedFixtureClaimsNoAddress()
{
    Fixture *real = makeDimmer(g_doc, 4, 0, 0);
    QVERIFY(real != nullptr);
    Fixture *inv = makeDimmer(g_doc, 4, 0, 0, false);
    QVERIFY2(inv != nullptr, "an inventory fixture must not collide with a patched one");

    // The address book still says channel 0 belongs to the real fixture.
    QCOMPARE(g_doc->fixtureForAddress(0), real->id());
    QCOMPARE(g_doc->fixtureForAddress(3), real->id());

    // Touching the unpatched fixture must not re-register its placeholder range
    // (Doc::slotFixtureChanged re-adds ranges on every change).
    inv->setName("renamed");
    inv->setID(inv->id());          // emits changed()
    QCOMPARE(g_doc->fixtureForAddress(0), real->id());
}

void Inventory_Test::unpatchedFixtureNeverReachesTheUniverse()
{
    // THE test: an inventory fixture sitting on the same placeholder 0/0 as a
    // real, patched fixture must never put a byte on the real one's channels.
    Fixture *real = makeDimmer(g_doc, 4, 0, 0);
    Fixture *inv = makeDimmer(g_doc, 4, 0, 0, false);
    QVERIFY(real != nullptr && inv != nullptr);

    QList<Universe *> ua = g_doc->inputOutputMap()->universes();
    QSharedPointer<GenericFader> fader = ua[0]->requestFader();

    FadeChannel invFc(g_doc, inv->id(), 1);
    QVERIFY(invFc.isInert());
    QCOMPARE(invFc.address(), QLCChannel::invalid());
    QCOMPARE(invFc.universe(), Universe::invalid());
    invFc.setStart(0);
    invFc.setTarget(255);
    invFc.setFadeTime(0);
    fader->add(invFc);

    fader->write(ua[0]);
    QCOMPARE(ua[0]->preGMValues()[1], char(0));     // the real fixture's channel: untouched

    // Sanity: the same write for the REAL fixture does land, so the test above
    // is not passing merely because nothing writes.
    FadeChannel realFc(g_doc, real->id(), 1);
    QVERIFY(!realFc.isInert());
    realFc.setStart(0);
    realFc.setTarget(255);
    realFc.setFadeTime(0);
    fader->add(realFc);
    fader->write(ua[0]);
    QCOMPARE(uchar(ua[0]->preGMValues()[1]), uchar(255));
}

void Inventory_Test::patchFixtureAssignsAndRegisters()
{
    Fixture *inv = makeDimmer(g_doc, 4, 0, 0, false);
    QVERIFY(inv != nullptr);

    QVERIFY(g_doc->patchFixture(inv->id(), 1, 20));
    QVERIFY(inv->isPatched());
    QCOMPARE(inv->universe(), quint32(1));
    QCOMPARE(inv->address(), quint32(20));
    QCOMPARE(g_doc->fixtureForAddress((1 << 9) | 20), inv->id());
    QCOMPARE(g_doc->fixtureForAddress((1 << 9) | 23), inv->id());
    QCOMPARE(g_doc->fixtureForAddress((1 << 9) | 24), Fixture::invalidId());
    QVERIFY(g_doc->inputOutputMap()->universesCount() >= 2);
}

void Inventory_Test::patchFixtureRefusesAnOverlap()
{
    Fixture *real = makeDimmer(g_doc, 4, 0, 10);
    Fixture *inv = makeDimmer(g_doc, 4, 0, 0, false);
    QVERIFY(real != nullptr && inv != nullptr);

    QVERIFY(!g_doc->patchFixture(inv->id(), 0, 12));   // overlaps 10-13
    QVERIFY(!inv->isPatched());                        // and changed nothing
    QCOMPARE(g_doc->fixtureForAddress(12), real->id());

    QVERIFY(g_doc->patchFixture(inv->id(), 0, 14));    // the free range next to it
}

void Inventory_Test::patchFixtureRefusesToRunPastTheUniverse()
{
    Fixture *inv = makeDimmer(g_doc, 8, 0, 0, false);
    QVERIFY(inv != nullptr);
    QVERIFY(!g_doc->patchFixture(inv->id(), 0, 508));  // 508..515 > 512
    QVERIFY(!inv->isPatched());
    QVERIFY(g_doc->patchFixture(inv->id(), 0, 504));   // 504..511 fits exactly
    QVERIFY(!g_doc->patchFixture(9999, 0, 0));         // unknown id
}

void Inventory_Test::patchedFixtureWritesAfterPatching()
{
    Fixture *inv = makeDimmer(g_doc, 4, 0, 0, false);
    QVERIFY(inv != nullptr);
    QVERIFY(g_doc->patchFixture(inv->id(), 0, 100));

    QList<Universe *> ua = g_doc->inputOutputMap()->universes();
    QSharedPointer<GenericFader> fader = ua[0]->requestFader();
    FadeChannel fc(g_doc, inv->id(), 2);
    QVERIFY(!fc.isInert());
    fc.setStart(0);
    fc.setTarget(200);
    fc.setFadeTime(0);
    fader->add(fc);
    fader->write(ua[0]);
    QCOMPARE(uchar(ua[0]->preGMValues()[102]), uchar(200));
}

void Inventory_Test::groupingNamesFileFixturesByManufacturerAndModel()
{
    // A generic dimmer has no manufacturer or model of its own: it must not
    // come out as "Generic / Generic", and its channel count tells them apart.
    Fixture *d4 = makeDimmer(g_doc, 4, 0, 0, false);
    Fixture *d8 = makeDimmer(g_doc, 8, 0, 0, false);
    QVERIFY(d4 != nullptr && d8 != nullptr);
    QCOMPARE(d4->groupingManufacturer(), QString("Generic"));
    QCOMPARE(d4->groupingModel(), QString("Dimmer, 4 ch"));
    QCOMPARE(d8->groupingModel(), QString("Dimmer, 8 ch"));

    // A real definition files under its own manufacturer and model.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Martin");
    def->setModel("MAC 250");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Std");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setGroup(QLCChannel::Intensity);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    Fixture *mac = new Fixture(g_doc);
    mac->setFixtureDefinition(def, mode);
    QCOMPARE(mac->groupingManufacturer(), QString("Martin"));
    QCOMPARE(mac->groupingModel(), QString("MAC 250"));
}

void Inventory_Test::unpatchReturnsAFixtureToTheInventory()
{
    Fixture *a = makeDimmer(g_doc, 4, 0, 10);
    QVERIFY(a != nullptr);
    a->setName("Keeper");
    QCOMPARE(g_doc->fixtureForAddress(10), a->id());

    QVERIFY(g_doc->unpatchFixture(a->id()));
    QVERIFY(!a->isPatched());
    QCOMPARE(a->name(), QString("Keeper"));                       // still the same fixture
    QCOMPARE(g_doc->fixtureForAddress(10), Fixture::invalidId()); // channels released

    // Someone else can take the freed channels...
    Fixture *b = makeDimmer(g_doc, 4, 0, 10);
    QVERIFY2(b != nullptr, "the released range should be free");

    // ...and it can never write to them again: a fader channel for it is inert.
    FadeChannel fc(g_doc, a->id(), 0);
    QVERIFY(fc.isInert());
    QCOMPARE(fc.address(), QLCChannel::invalid());

    // It can be patched again, elsewhere.
    QVERIFY(g_doc->patchFixture(a->id(), 0, 100));
    QVERIFY(a->isPatched());
}

void Inventory_Test::unpatchRefusesInOperateModeAndForStagedFixtures()
{
    Fixture *real = makeDimmer(g_doc, 4, 0, 0);
    Fixture *staged = makeDimmer(g_doc, 4, 0, 0, false);
    QVERIFY(real != nullptr && staged != nullptr);

    QVERIFY(!g_doc->unpatchFixture(staged->id()));    // already staged
    QVERIFY(!g_doc->unpatchFixture(424242));          // unknown

    g_doc->setMode(Doc::Operate);
    QVERIFY(!g_doc->unpatchFixture(real->id()));      // a running show holds its channels
    QVERIFY(real->isPatched());
    g_doc->setMode(Doc::Design);
    QVERIFY(g_doc->unpatchFixture(real->id()));
}

static QByteArray saveFixture(Fixture *fxi)
{
    QByteArray buf;
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    w.writeStartElement("Root");
    fxi->saveXML(&w);
    w.writeEndElement();
    w.writeEndDocument();
    return buf;
}

void Inventory_Test::xmlRoundTripKeepsInventoryFields()
{
    Fixture *inv = makeDimmer(g_doc, 4, 0, 0, false);
    QVERIFY(inv != nullptr);
    inv->setName("Fence 3");
    inv->setConnection(Fixture::Network);
    inv->setIPAddress("172.18.2.41");
    const QByteArray xml = saveFixture(inv);
    QVERIFY(xml.contains("<Patched>False</Patched>"));

    Doc doc2(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();               // Root
    r.readNextStartElement();               // Fixture
    QVERIFY(Fixture::loader(r, &doc2));
    QCOMPARE(doc2.fixturesCount(), 1);
    Fixture *back = doc2.fixtures().first();
    QVERIFY(!back->isPatched());
    QCOMPARE(back->connection(), Fixture::Network);
    QCOMPARE(back->ipAddress(), QString("172.18.2.41"));
    QCOMPARE(back->name(), QString("Fence 3"));
    // Loading it must not have claimed its placeholder address either.
    QCOMPARE(doc2.fixtureForAddress(0), Fixture::invalidId());
}

void Inventory_Test::ordinaryFixtureXmlIsUnchanged()
{
    // A fixture that is patched and DMX must serialise with none of the new
    // elements, so every existing workspace round-trips byte for byte.
    Fixture *real = makeDimmer(g_doc, 4, 0, 0);
    QVERIFY(real != nullptr);
    const QByteArray xml = saveFixture(real);
    QVERIFY(!xml.contains("Patched"));
    QVERIFY(!xml.contains("Connection"));
    QVERIFY(!xml.contains("IPAddress"));

    Doc doc2(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();               // Root
    r.readNextStartElement();               // Fixture
    QVERIFY(Fixture::loader(r, &doc2));
    Fixture *back = doc2.fixtures().first();
    QVERIFY(back->isPatched());
    QCOMPARE(back->connection(), Fixture::Dmx);
    QCOMPARE(doc2.fixtureForAddress(0), back->id());
}

static Fixture *makeStaged(Doc *doc, int channels, const QString &name,
                           Fixture::Connection c = Fixture::Dmx, const QString &ip = QString())
{
    Fixture *f = makeDimmer(doc, channels, 0, 0, false);
    if (f != nullptr)
    {
        f->setName(name);
        f->setConnection(c);
        f->setIPAddress(ip);
    }
    return f;
}

/* The plan comes back in inventory order (manufacturer, model, name), not the
 * order the ids were handed in, so tests look each fixture's assignment up. */
static PatchPlanner::Assignment forFixture(const QList<PatchPlanner::Assignment> &plan, quint32 id)
{
    foreach (const PatchPlanner::Assignment &a, plan)
        if (a.fixtureId == id)
            return a;
    return PatchPlanner::Assignment();
}

void Inventory_Test::plannerPacksDmxFirstFitAroundPatchedFixtures()
{
    makeDimmer(g_doc, 10, 0, 0);                       // 1-10 taken
    makeDimmer(g_doc, 10, 0, 20);                      // 21-30 taken
    Fixture *a = makeStaged(g_doc, 10, "A");
    Fixture *b = makeStaged(g_doc, 12, "B");
    Fixture *c = makeStaged(g_doc, 4, "C");

    PatchPlanner pp(g_doc);
    const QList<PatchPlanner::Assignment> plan =
        pp.plan(QList<quint32>() << a->id() << b->id() << c->id());
    QCOMPARE(plan.size(), 3);
    for (const PatchPlanner::Assignment &x : plan)
    {
        QCOMPARE(x.universe, quint32(0));
        QVERIFY(!x.newUniverse);
    }
    // Smaller models first ("Dimmer, 4 ch" < "10" < "12" numerically): C takes
    // the front of the 10-19 gap, A (10 ch) no longer fits the remainder and goes
    // after the second patched fixture, B after A.
    QCOMPARE(forFixture(plan, c->id()).address, quint32(10));
    QCOMPARE(forFixture(plan, a->id()).address, quint32(30));
    QCOMPARE(forFixture(plan, b->id()).address, quint32(40));
}

void Inventory_Test::plannerSpillsIntoANewUniverseWhenFull()
{
    const quint32 existing = g_doc->inputOutputMap()->universesCount();
    QList<quint32> ids;
    for (quint32 u = 0; u < existing; u++)
        makeDimmer(g_doc, 512, u, 0);                  // every universe full
    Fixture *a = makeStaged(g_doc, 100, "A");
    Fixture *b = makeStaged(g_doc, 100, "B");
    ids << a->id() << b->id();

    const QList<PatchPlanner::Assignment> plan = PatchPlanner(g_doc).plan(ids);
    QCOMPARE(plan[0].universe, existing);
    QVERIFY(plan[0].newUniverse);
    QCOMPARE(plan[1].universe, existing);              // B shares the new universe
    QCOMPARE(plan[1].address, quint32(100));
}

void Inventory_Test::plannerGivesEachIpItsOwnUniverse()
{
    Fixture *f1 = makeStaged(g_doc, 3, "Fence 1", Fixture::Network, "172.18.2.41");
    Fixture *f2 = makeStaged(g_doc, 3, "Fence 2", Fixture::Network, "172.18.2.41");   // same IP
    Fixture *f3 = makeStaged(g_doc, 3, "Step 1",  Fixture::Network, "172.18.2.42");
    Fixture *f4 = makeStaged(g_doc, 3, "Step 2",  Fixture::Network);                  // no IP

    const QList<PatchPlanner::Assignment> plan = PatchPlanner(g_doc).plan(
        QList<quint32>() << f1->id() << f2->id() << f3->id() << f4->id());
    QCOMPARE(plan[1].universe, plan[0].universe);      // same IP shares
    QCOMPARE(plan[1].address, quint32(3));
    QVERIFY(plan[2].universe != plan[0].universe);     // a different IP does not
    QVERIFY(plan[3].universe != plan[0].universe);
    QVERIFY(plan[3].universe != plan[2].universe);
    // Each its own -- a free universe or a new one (networkFixturesTakeAFreeBlockFirst).
    QVERIFY2(!plan[3].note.isEmpty(), "a network fixture with no IP should say so");
    QVERIFY(plan[0].note.isEmpty());
}

void Inventory_Test::plannerKeepsDmxOutOfNetworkUniverses()
{
    // A patched network fixture already owns universe 0: DMX must not join it.
    Fixture *net = makeDimmer(g_doc, 3, 0, 0);
    net->setConnection(Fixture::Network);
    net->setIPAddress("172.18.2.41");
    Fixture *dmx = makeStaged(g_doc, 4, "Mover");
    Fixture *more = makeStaged(g_doc, 3, "Fence 9", Fixture::Network, "172.18.2.41");

    const QList<PatchPlanner::Assignment> plan =
        PatchPlanner(g_doc).plan(QList<quint32>() << dmx->id() << more->id());
    QVERIFY2(forFixture(plan, dmx->id()).universe != 0, "DMX fixture landed in a network universe");
    QCOMPARE(forFixture(plan, more->id()).universe, quint32(0));   // same IP joins the existing one
    QCOMPARE(forFixture(plan, more->id()).address, quint32(3));
}

void Inventory_Test::plannerIsPureAndApplyCommitsIt()
{
    Fixture *a = makeStaged(g_doc, 8, "A");
    Fixture *b = makeStaged(g_doc, 8, "B");
    const QList<quint32> ids = QList<quint32>() << a->id() << b->id();

    PatchPlanner pp(g_doc);
    const QList<PatchPlanner::Assignment> plan = pp.plan(ids);
    QVERIFY(!a->isPatched() && !b->isPatched());       // planning changed nothing
    QCOMPARE(g_doc->fixtureForAddress(0), Fixture::invalidId());

    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 2);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
    QVERIFY(a->isPatched() && b->isPatched());
    QCOMPARE(g_doc->fixtureForAddress(0), a->id());
    QCOMPARE(g_doc->fixtureForAddress(8), b->id());

    // Nothing left to plan.
    QVERIFY(pp.plan(ids).isEmpty());
}

void Inventory_Test::plannerSkipsPatchedAndUnknown()
{
    Fixture *real = makeDimmer(g_doc, 4, 0, 0);
    const QList<PatchPlanner::Assignment> plan =
        PatchPlanner(g_doc).plan(QList<quint32>() << real->id() << 424242);
    QVERIFY(plan.isEmpty());
}

void Inventory_Test::plannerKeepsIdenticalFixturesTogether()
{
    // Added interleaved: 4ch, 8ch, 4ch. By id the second 4-channel fixture would
    // land after the 8-channel one; by model the two 4-channels sit side by side.
    Fixture *a = makeStaged(g_doc, 4, "Bar A");
    Fixture *b = makeStaged(g_doc, 8, "Bar B");
    Fixture *c = makeStaged(g_doc, 4, "Bar C");

    const QList<PatchPlanner::Assignment> plan =
        PatchPlanner(g_doc).plan(QList<quint32>() << a->id() << b->id() << c->id());
    QCOMPARE(plan.size(), 3);
    QCOMPARE(plan[0].fixtureId, a->id());
    QCOMPARE(plan[1].fixtureId, c->id());                   // the other 4-channel one
    QCOMPARE(plan[2].fixtureId, b->id());
    QCOMPARE(plan[0].address, quint32(0));
    QCOMPARE(plan[1].address, quint32(4));
    QCOMPARE(plan[2].address, quint32(8));
}

void Inventory_Test::plannerNumbersCompareAsNumbers()
{
    Fixture *ten = makeStaged(g_doc, 3, "Wash 10");
    Fixture *two = makeStaged(g_doc, 3, "Wash 2");
    Fixture *one = makeStaged(g_doc, 3, "Wash 1");

    const QList<PatchPlanner::Assignment> plan =
        PatchPlanner(g_doc).plan(QList<quint32>() << ten->id() << two->id() << one->id());
    QCOMPARE(plan[0].fixtureId, one->id());
    QCOMPARE(plan[1].fixtureId, two->id());
    QCOMPARE(plan[2].fixtureId, ten->id());                 // not "Wash 10" before "Wash 2"
}

void Inventory_Test::modelNamesKeepTheirOwnDigits()
{
    QSet<QString> taken;
    auto isTaken = [&taken](const QString &n) { return taken.contains(n); };

    // Free: kept as it is.
    QCOMPARE(Doc::nextUniqueModelName("LM70", isTaken), QString("LM70"));

    // Taken: a counter is APPENDED. The old rule bumped the model's own digits
    // and produced "LM71", a model that does not exist.
    taken << "LM70";
    QCOMPARE(Doc::nextUniqueModelName("LM70", isTaken), QString("LM70 #2"));
    taken << "LM70 #2";
    QCOMPARE(Doc::nextUniqueModelName("LM70", isTaken), QString("LM70 #3"));
    taken << "Par 64";
    QCOMPARE(Doc::nextUniqueModelName("Par 64", isTaken), QString("Par 64 #2"));   // not "Par 65"

    // A name that carries a counter is still bumped by the counter rule.
    QSet<QString> used;
    used << "US #4";
    QCOMPARE(Doc::nextUniqueName("US #4", [&used](const QString &c) { return used.contains(c); }),
             QString("US #5"));
}

/* A definition with two modes: "Basic" (nCh1 channels, 1 head) and "Extended"
 * (nCh2 channels, heads2 heads, one channel each). Returns the fixture, staged. */
static Fixture *stagedTwoMode(Doc *doc, int nCh1, int nCh2, int heads2, QLCFixtureMode **basicOut,
                              QLCFixtureMode **extOut)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("TwoMode");
    QLCFixtureMode *basic = new QLCFixtureMode(def);
    basic->setName("Basic");
    QLCFixtureMode *ext = new QLCFixtureMode(def);
    ext->setName("Extended");
    const int n = qMax(nCh1, nCh2);
    for (int k = 0; k < n; k++)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("Ch %1").arg(k));
        ch->setGroup(QLCChannel::Intensity);
        def->addChannel(ch);
        if (k < nCh1) basic->insertChannel(ch, k);
        if (k < nCh2) ext->insertChannel(ch, k);
    }
    QLCFixtureHead h1;
    for (int k = 0; k < nCh1; k++) h1.addChannel(quint32(k));
    basic->insertHead(-1, h1);
    for (int hd = 0; hd < heads2; hd++)
    {
        QLCFixtureHead h;
        h.addChannel(quint32(hd));
        ext->insertHead(-1, h);
    }
    def->addMode(basic);
    def->addMode(ext);

    Fixture *f = new Fixture(doc);
    f->setFixtureDefinition(def, basic);
    f->setName("TwoMode 1");
    f->setPatched(false);
    if (!doc->addFixture(f))
        return nullptr;
    *basicOut = basic;
    *extOut = ext;
    return f;
}

void Inventory_Test::modeCanChangeWhenNothingReferencesItsChannels()
{
    QLCFixtureMode *basic = nullptr, *ext = nullptr;
    Fixture *f = stagedTwoMode(g_doc, 4, 8, 1, &basic, &ext);
    QVERIFY(f != nullptr);
    QString why;
    QVERIFY2(g_doc->canChangeFixtureMode(f->id(), ext, &why), qPrintable(why));
    QVERIFY(g_doc->canChangeFixtureMode(f->id(), nullptr, &why));      // dimmer-style change
}

void Inventory_Test::aSceneThatSetsChannelsDirectlyLocksTheMode()
{
    QLCFixtureMode *basic = nullptr, *ext = nullptr;
    Fixture *f = stagedTwoMode(g_doc, 4, 8, 1, &basic, &ext);
    QVERIFY(f != nullptr);

    Scene *sc = new Scene(g_doc);
    sc->setName("Hand Built");
    sc->setValue(f->id(), 2, 200);                    // raw "channel 3 = 200"
    QVERIFY(g_doc->addFunction(sc));

    QString why;
    QVERIFY(!g_doc->canChangeFixtureMode(f->id(), ext, &why));
    QVERIFY2(why.contains("Hand Built"), qPrintable(why));
}

void Inventory_Test::aModeLosingHeadsAGroupUsesIsRefused()
{
    QLCFixtureMode *basic = nullptr, *ext = nullptr;
    Fixture *f = stagedTwoMode(g_doc, 4, 8, 8, &basic, &ext);    // Extended has 8 heads, Basic 1
    QVERIFY(f != nullptr);
    QString why;
    // In no group: a different head count hurts nothing ("change the mode to
    // accommodate where possible").
    QVERIFY2(g_doc->canChangeFixtureMode(f->id(), ext, &why), qPrintable(why));

    // On Extended, in a group that lays out its 8 heads: back to Basic's one
    // would leave the group pointing at heads 2..8.
    f->setFixtureDefinition(f->fixtureDef(), ext);
    FixtureGroup *grp = new FixtureGroup(g_doc);
    grp->setName("Pixels");
    QVERIFY(g_doc->addFixtureGroup(grp));
    grp->setSize(QSize(8, 1));
    for (int h = 0; h < 8; ++h)
        grp->assignHead(QLCPoint(h, 0), GroupHead(f->id(), h));
    QVERIFY(!g_doc->canChangeFixtureMode(f->id(), basic, &why));
    QVERIFY2(why.contains("Pixels") && why.contains("head"), qPrintable(why));

    // Only its first head in the group: Basic keeps that one.
    for (int h = 1; h < 8; ++h)
        grp->resignHead(QLCPoint(h, 0));
    QVERIFY2(g_doc->canChangeFixtureMode(f->id(), basic, &why), qPrintable(why));
    g_doc->deleteFixtureGroup(grp->id());
}

void Inventory_Test::patchTimeModeChangeSetsTheFootprintBeforePatching()
{
    makeDimmer(g_doc, 10, 0, 0);                       // 1-10 taken
    QLCFixtureMode *basic = nullptr, *ext = nullptr;
    Fixture *f = stagedTwoMode(g_doc, 4, 8, 1, &basic, &ext);
    QVERIFY(f != nullptr);
    QCOMPARE(f->channels(), quint32(4));

    // Plan it at the footprint of the mode we are about to choose.
    PatchPlanner pp(g_doc);
    QHash<quint32, quint32> overrides;
    overrides.insert(f->id(), 8);
    QList<PatchPlanner::Assignment> plan = pp.plan(QList<quint32>() << f->id(), overrides);
    QCOMPARE(plan.first().address, quint32(10));
    plan.first().mode = ext;

    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 1);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
    QCOMPARE(f->fixtureMode(), ext);
    QCOMPARE(f->channels(), quint32(8));
    QVERIFY(f->isPatched());
    QCOMPARE(g_doc->fixtureForAddress(17), f->id());       // all 8 channels registered
    QCOMPARE(g_doc->fixtureForAddress(18), Fixture::invalidId());
}

void Inventory_Test::patchTimeDimmerChannelsCanChange()
{
    Fixture *d = makeStaged(g_doc, 1, "Dimmer");
    QVERIFY(d != nullptr);
    QHash<quint32, quint32> overrides;
    overrides.insert(d->id(), 6);

    PatchPlanner pp(g_doc);
    QList<PatchPlanner::Assignment> plan = pp.plan(QList<quint32>() << d->id(), overrides);
    plan.first().dimmerChannels = 6;
    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 1);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
    QCOMPARE(d->channels(), quint32(6));
    QCOMPARE(g_doc->fixtureForAddress(5), d->id());
}

void Inventory_Test::aRefusedModeChangeLeavesTheFixtureUnpatched()
{
    QLCFixtureMode *basic = nullptr, *ext = nullptr;
    Fixture *f = stagedTwoMode(g_doc, 4, 8, 1, &basic, &ext);
    QVERIFY(f != nullptr);
    Scene *sc = new Scene(g_doc);
    sc->setName("Locks It");
    sc->setValue(f->id(), 0, 255);
    QVERIFY(g_doc->addFunction(sc));

    PatchPlanner pp(g_doc);
    QList<PatchPlanner::Assignment> plan = pp.plan(QList<quint32>() << f->id());
    plan.first().mode = ext;
    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 0);
    QVERIFY(!f->isPatched());                          // refused as a whole, not half-applied
    QCOMPARE(f->fixtureMode(), basic);
    QVERIFY2(!problems.isEmpty() && problems.first().contains("Locks It"), qPrintable(problems.join("; ")));
}

void Inventory_Test::aFixtureTooBigForAUniverseIsNotedNotPlaced()
{
    Fixture *f = makeStaged(g_doc, 4, "Pixel Wall");
    QHash<quint32, quint32> overrides;
    overrides.insert(f->id(), 600);                    // mode chosen at patch time

    const QList<PatchPlanner::Assignment> plan =
        PatchPlanner(g_doc).plan(QList<quint32>() << f->id(), overrides);
    QCOMPARE(plan.size(), 1);
    QVERIFY2(plan.first().note.contains("600"), qPrintable(plan.first().note));
    QVERIFY(!plan.first().newUniverse);
}

void Inventory_Test::applyReportsAnAddressTakenSincePlanning()
{
    Fixture *f = makeStaged(g_doc, 8, "Late");
    PatchPlanner pp(g_doc);
    const QList<PatchPlanner::Assignment> plan = pp.plan(QList<quint32>() << f->id());
    QCOMPARE(plan.first().address, quint32(0));

    makeDimmer(g_doc, 4, 0, 2);                        // someone patched into 3-6 meanwhile

    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 0);
    QVERIFY(!f->isPatched());
    QCOMPARE(problems.size(), 1);
    QVERIFY2(problems.first().contains("Late") && problems.first().contains("universe 1")
             && problems.first().contains("address 1"), qPrintable(problems.first()));

    // A null problem list is allowed: the refusal still happens, quietly.
    QCOMPARE(pp.apply(plan, nullptr), 0);
}

void Inventory_Test::applyRefusesADimmerResizeAScenePins()
{
    Fixture *d = makeStaged(g_doc, 4, "Pinned Dimmer");
    Scene *sc = new Scene(g_doc);
    sc->setName("Uses Ch 4");
    sc->setValue(d->id(), 3, 128);
    QVERIFY(g_doc->addFunction(sc));

    PatchPlanner pp(g_doc);
    QList<PatchPlanner::Assignment> plan = pp.plan(QList<quint32>() << d->id());
    plan.first().dimmerChannels = 2;                   // would orphan channel 4
    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 0);
    QVERIFY(!d->isPatched());
    QCOMPARE(d->channels(), quint32(4));
    QVERIFY2(problems.size() == 1 && problems.first().contains("channel count")
             && problems.first().contains("Uses Ch 4"), qPrintable(problems.join("; ")));
}

void Inventory_Test::applySkipsAFixtureDeletedSincePlanning()
{
    Fixture *gone = makeStaged(g_doc, 4, "Gone");
    Fixture *kept = makeStaged(g_doc, 4, "Kept");
    PatchPlanner pp(g_doc);
    const QList<PatchPlanner::Assignment> plan = pp.plan(QList<quint32>() << gone->id() << kept->id());
    QVERIFY(g_doc->deleteFixture(gone->id()));

    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 1);
    QVERIFY(problems.isEmpty());
    QVERIFY(kept->isPatched());
}

void Inventory_Test::applyPointsAnArtNetUniverseAtTheFixtureIp()
{
    FakeNetPlugin *artnet = new FakeNetPlugin("ArtNet");
    artnet->registerIn(g_doc);
    g_doc->inputOutputMap()->setOutputPatch(0, "ArtNet", "eth0", 0);
    Fixture *fence = makeStaged(g_doc, 3, "Fence 1", Fixture::Network, "172.18.2.41");

    PatchPlanner pp(g_doc);
    QList<PatchPlanner::Assignment> plan = pp.plan(QList<quint32>() << fence->id());
    plan.first().universe = 0;                         // the user steered it onto universe 1
    plan.first().address = 0;
    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 1);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
    QCOMPARE(g_doc->inputOutputMap()->outputPatch(0)->pluginName(), QString("ArtNet"));
    QCOMPARE(artnet->m_params.value(0).value("outputIP").toString(), QString("172.18.2.41"));
}

/* A lantern sent to by WLED's own API: its universe goes out through the
   WLED output -- not Art-Net, even with an Art-Net universe there to borrow
   -- aimed at its IP, reading its channels where they are. */
void Inventory_Test::aWledFixtureGetsTheWledOutputAimedAtIt()
{
    FakeNetPlugin *artnet = new FakeNetPlugin("ArtNet");
    artnet->registerIn(g_doc);
    g_doc->inputOutputMap()->setOutputPatch(0, "ArtNet", "eth0", 0);
    FakeNetPlugin *wled = new FakeNetPlugin("WLED");
    wled->registerIn(g_doc);
    makeDimmer(g_doc, 3, 0, 0)->setConnection(Fixture::Network);    // an Art-Net universe exists
    Fixture *lantern = makeStaged(g_doc, 3, "Lantern SL", Fixture::Network, "192.168.20.185");
    lantern->setTransport(Fixture::WledTransport);

    PatchPlanner pp(g_doc);
    const QList<PatchPlanner::Assignment> plan = pp.plan(QList<quint32>() << lantern->id());
    const quint32 uni = plan.first().universe;
    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 1);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
    OutputPatch *op = g_doc->inputOutputMap()->outputPatch(uni);
    QVERIFY(op != nullptr);
    QCOMPARE(op->pluginName(), QString("WLED"));
    QCOMPARE(wled->m_params.value(uni).value("outputIP").toString(), QString("192.168.20.185"));
    QCOMPARE(wled->m_params.value(uni).value("wledAddress").toInt(), int(lantern->address()) + 1);
    QCOMPARE(wled->m_params.value(uni).value("wledChannels").toInt(), 3);
    QVERIFY(!artnet->m_params.value(uni).contains("outputIP"));

    /* A backup lantern on the patch outlives patching again -- and one made
       the primary by hand trades places back, never sent to twice. */
    op->setPluginParameter("outputIP2", "192.168.20.186");
    QVERIFY(g_doc->unpatchFixture(lantern->id()));
    QCOMPARE(pp.apply(plan, &problems), 1);
    QCOMPARE(wled->m_params.value(uni).value("outputIP").toString(), QString("192.168.20.185"));
    QCOMPARE(wled->m_params.value(uni).value("outputIP2").toString(), QString("192.168.20.186"));
    op->setPluginParameter("outputIP", "192.168.20.186");
    op->setPluginParameter("outputIP2", "192.168.20.185");
    QVERIFY(g_doc->unpatchFixture(lantern->id()));
    QCOMPARE(pp.apply(plan, &problems), 1);
    QCOMPARE(wled->m_params.value(uni).value("outputIP").toString(), QString("192.168.20.185"));
    QCOMPARE(wled->m_params.value(uni).value("outputIP2").toString(), QString("192.168.20.186"));
}

void Inventory_Test::theTransportRoundTrips()
{
    Fixture *lantern = makeDimmer(g_doc, 4, 0, 0, false);
    lantern->setConnection(Fixture::Network);
    lantern->setIPAddress("192.168.20.185");
    lantern->setTransport(Fixture::WledTransport);
    const QByteArray xml = saveFixture(lantern);
    QVERIFY(xml.contains("<Transport>WLED</Transport>"));
    Doc doc2(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    r.readNextStartElement();
    QVERIFY(Fixture::loader(r, &doc2));
    QCOMPARE(doc2.fixtures().first()->transport(), Fixture::WledTransport);

    // Art-Net, the default, writes nothing.
    lantern->setTransport(Fixture::ArtNetTransport);
    QVERIFY(!saveFixture(lantern).contains("Transport"));
}

void Inventory_Test::applyBorrowsTheOutputOfAnotherNetworkUniverse()
{
    FakeNetPlugin *sacn = new FakeNetPlugin("E1.31");
    sacn->registerIn(g_doc);
    g_doc->inputOutputMap()->setOutputPatch(0, "E1.31", "eth1", 1);
    makeDimmer(g_doc, 3, 0, 0)->setConnection(Fixture::Network);    // universe 1 is on the network
    Fixture *step = makeStaged(g_doc, 3, "Step 1", Fixture::Network, "172.18.2.42");

    PatchPlanner pp(g_doc);
    const QList<PatchPlanner::Assignment> plan = pp.plan(QList<quint32>() << step->id());
    const quint32 uni = plan.first().universe;
    QVERIFY(uni != 0);                                 // a different IP gets its own universe

    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 1);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
    OutputPatch *op = g_doc->inputOutputMap()->outputPatch(uni);
    QVERIFY(op != nullptr);
    QCOMPARE(op->pluginName(), QString("E1.31"));      // same plugin and interface as universe 1
    QCOMPARE(op->output(), quint32(1));
    QCOMPARE(sacn->m_params.value(uni).value("outputIP").toString(), QString("172.18.2.42"));
}

void Inventory_Test::applyAsksForAnOutputWhenNoNetworkUniverseExists()
{
    Fixture *withIp = makeStaged(g_doc, 3, "Fence 1", Fixture::Network, "172.18.2.41");
    Fixture *noIp = makeStaged(g_doc, 3, "Fence 2", Fixture::Network);

    PatchPlanner pp(g_doc);
    const QList<PatchPlanner::Assignment> plan = pp.plan(QList<quint32>() << withIp->id() << noIp->id());
    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 2);            // patched regardless: output is a Connections job
    QCOMPARE(problems.size(), 2);
    const QString a = forFixture(plan, withIp->id()).universe < forFixture(plan, noIp->id()).universe
                      ? problems.at(0) : problems.at(1);
    const QString b = a == problems.at(0) ? problems.at(1) : problems.at(0);
    QVERIFY2(a.contains("Fence 1") && a.contains("unicast to 172.18.2.41"), qPrintable(a));
    QVERIFY2(b.contains("Fence 2") && !b.contains("unicast"), qPrintable(b));
}

void Inventory_Test::repatchingMovesTheFixturesClaim()
{
    Fixture *f = makeStaged(g_doc, 4, "Mover");
    QVERIFY(g_doc->patchFixture(f->id(), 0, 0));
    QCOMPARE(g_doc->fixtureForAddress(0), f->id());

    QVERIFY(g_doc->patchFixture(f->id(), 1, 100));     // move it
    QCOMPARE(g_doc->fixtureForAddress(0), Fixture::invalidId());
    QCOMPARE(g_doc->fixtureForAddress(3), Fixture::invalidId());
    QCOMPARE(g_doc->fixtureForAddress((1 << 9) | 100), f->id());
    QCOMPARE(g_doc->fixtureForAddress((1 << 9) | 103), f->id());

    // Overlapping its own old range is fine: it is only in its own way.
    QVERIFY(g_doc->patchFixture(f->id(), 1, 102));
    QCOMPARE(g_doc->fixtureForAddress((1 << 9) | 100), Fixture::invalidId());
    QCOMPARE(g_doc->fixtureForAddress((1 << 9) | 105), f->id());
    QVERIFY(!g_doc->patchFixture(99999, 0, 0));
}

void Inventory_Test::anUnknownFixtureCannotChangeMode()
{
    QString why;
    QVERIFY(!g_doc->canChangeFixtureMode(99999, nullptr, &why));
    QCOMPARE(why, QString("no such fixture"));
    QVERIFY(!g_doc->canChangeFixtureMode(99999, nullptr, nullptr));
}

void Inventory_Test::aModelNameThatNeverFreesUpIsReturnedAsIs()
{
    QCOMPARE(Doc::nextUniqueModelName("LM70", [](const QString &) { return true; }), QString("LM70"));
    QCOMPARE(Doc::nextUniqueModelName("", [](const QString &) { return true; }), QString());
}

void Inventory_Test::groupingNamesForGenericAndRgbPanels()
{
    Fixture *d = makeStaged(g_doc, 3, "Dimmer Pack");
    QCOMPARE(d->groupingManufacturer(), QString("Generic"));
    QCOMPARE(d->groupingModel(), QString("Dimmer, 3 ch"));

    QLCFixtureDef *panel = new QLCFixtureDef();
    panel->setManufacturer("");                        // as a hand-built def may leave it
    panel->setModel("RGBPanel");
    QLCFixtureMode *mode = new QLCFixtureMode(panel);
    panel->addMode(mode);
    Fixture *p = new Fixture(g_doc);
    p->setFixtureDefinition(panel, mode);
    p->setPatched(false);
    QVERIFY(g_doc->addFixture(p));
    QCOMPARE(p->groupingManufacturer(), QString("Generic"));
    QCOMPARE(p->groupingModel(), QString("RGB panel"));
}

/* A wired node at (x, y) metres, one port per universe (invalid = free port). */
static NetworkNode proxNode(const char *name, double x, double y, const QList<quint32> &unis)
{
    NetworkNode n;
    n.name = name;
    n.placed = true;
    n.posX = x;
    n.posY = y;
    foreach (quint32 u, unis)
    {
        NetworkPort p;
        p.universe = u;
        n.ports << p;
    }
    return n;
}

/* A staged dimmer placed on the plot at (x, y) metres. */
static Fixture *placedStaged(Doc *doc, int channels, const QString &name, double x, double y)
{
    Fixture *f = makeStaged(doc, channels, name);
    if (f != nullptr)
        doc->monitorProperties()->setFixturePosition(f->id(), 0, 0,
                                                     QVector3D(float(x * 1000), float(y * 1000), 0));
    return f;
}

void Inventory_Test::eachNodeTakesTheFixturesNearIt()
{
    // T1/T2 off the SR node, T3/T4 off the SL node -- the rig in the brief.
    g_doc->networkTopology()->nodes() << proxNode("SR Node", 0, 0, { 0 })
                                      << proxNode("SL Node", 10, 0, { 1 });
    Fixture *t1 = placedStaged(g_doc, 4, "T1", 1, 2);
    Fixture *t2 = placedStaged(g_doc, 4, "T2", 2, 2);
    Fixture *t3 = placedStaged(g_doc, 4, "T3", 8, 2);
    Fixture *t4 = placedStaged(g_doc, 4, "T4", 9, 2);

    const QList<PatchPlanner::Assignment> plan = PatchPlanner(g_doc).plan({ t1->id(), t2->id(), t3->id(), t4->id() });
    QCOMPARE(forFixture(plan, t1->id()).universe, quint32(0));
    QCOMPARE(forFixture(plan, t2->id()).universe, quint32(0));
    QCOMPARE(forFixture(plan, t2->id()).address, quint32(4));
    QCOMPARE(forFixture(plan, t3->id()).universe, quint32(1));
    QCOMPARE(forFixture(plan, t4->id()).universe, quint32(1));
    QCOMPARE(forFixture(plan, t1->id()).nodeIndex, 0);
    QCOMPARE(forFixture(plan, t4->id()).nodeIndex, 1);
    QCOMPARE(forFixture(plan, t4->id()).portIndex, 0);
    QVERIFY(qAbs(forFixture(plan, t4->id()).distanceMetres - qSqrt(5.0)) < 0.01);
    QVERIFY(!forFixture(plan, t4->id()).claimsPort);
}

void Inventory_Test::equallyNearPortsFillLeastLoadedFirst()
{
    makeDimmer(g_doc, 100, 0, 0);                        // universe 1 already carries 100 ch
    g_doc->networkTopology()->nodes() << proxNode("Node", 0, 0, { 0, 1 });
    Fixture *f = placedStaged(g_doc, 4, "Near", 1, 1);
    const PatchPlanner::Assignment a = PatchPlanner(g_doc).plan({ f->id() }).first();
    QCOMPARE(a.universe, quint32(1));                   // the emptier of the node's two ports
    QCOMPARE(a.portIndex, 1);
    QCOMPARE(a.address, quint32(0));
}

void Inventory_Test::aFullPortOverflowsToAFreePortThatApplyClaims()
{
    makeDimmer(g_doc, 510, 0, 0);                        // universe 1: two channels left
    g_doc->networkTopology()->nodes()
        << proxNode("Node", 0, 0, { 0, InputOutputMap::invalidUniverse() });
    Fixture *a1 = placedStaged(g_doc, 4, "A1", 1, 1);
    Fixture *a2 = placedStaged(g_doc, 4, "A2", 1, 2);
    const quint32 existing = g_doc->inputOutputMap()->universesCount();

    PatchPlanner pp(g_doc);
    const QList<PatchPlanner::Assignment> plan = pp.plan({ a1->id(), a2->id() });
    const PatchPlanner::Assignment p1 = forFixture(plan, a1->id());
    const PatchPlanner::Assignment p2 = forFixture(plan, a2->id());
    // A universe of its own, on the free port: the first FREE one (nothing
    // on it, nothing patched to it) before a new one past the last.
    QCOMPARE(p1.universe, quint32(1));
    QVERIFY(!p1.newUniverse && p1.claimsPort);
    QVERIFY(existing > 1);
    QCOMPARE(p1.portIndex, 1);
    QCOMPARE(p2.universe, quint32(1));                   // the next one joins it
    QCOMPARE(p2.address, quint32(4));
    QVERIFY(!p2.claimsPort);

    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 2);
    QCOMPARE(g_doc->networkTopology()->nodes().first().ports.at(1).universe, quint32(1));
    QVERIFY(a1->isPatched() && a2->isPatched());
}

void Inventory_Test::aWirelessPortIsNearWhereItsReceiverIs()
{
    NetworkNode tx = proxNode("Beach TX", -50, 0, { 2 });
    tx.transport = NetworkNode::WirelessDMX;
    tx.ports[0].receiver.placed = true;
    tx.ports[0].receiver.posX = 10;
    tx.ports[0].receiver.posY = 0;
    g_doc->networkTopology()->nodes() << proxNode("SR Node", 0, 0, { 0 }) << tx;
    Fixture *f = placedStaged(g_doc, 4, "Out in the rig", 9, 0);
    const PatchPlanner::Assignment a = PatchPlanner(g_doc).plan({ f->id() }).first();
    QCOMPARE(a.universe, quint32(2));                    // the receiver 1 m away, not the TX 59 m off
    QCOMPARE(a.nodeIndex, 1);
    QVERIFY(qAbs(a.distanceMetres - 1.0) < 0.01);
}

void Inventory_Test::placedFixturesArePlannedBeforeTheRest()
{
    makeDimmer(g_doc, 500, 0, 0);                        // universe 1: 12 channels left
    g_doc->networkTopology()->nodes() << proxNode("Node", 0, 0, { 0 });
    Fixture *loose = makeStaged(g_doc, 10, "A Loose");   // sorts first, not on the plot
    // "nearFx", not "near": windows.h #defines near (and far) to nothing.
    Fixture *nearFx = placedStaged(g_doc, 10, "B Near", 1, 0);
    const QList<PatchPlanner::Assignment> plan = PatchPlanner(g_doc).plan({ loose->id(), nearFx->id() });
    QCOMPARE(plan.at(0).fixtureId, loose->id());         // listed in inventory order still
    QCOMPARE(forFixture(plan, nearFx->id()).universe, quint32(0));
    QCOMPARE(forFixture(plan, nearFx->id()).address, quint32(500));
    QVERIFY(forFixture(plan, loose->id()).universe != 0);
    QCOMPARE(forFixture(plan, loose->id()).nodeIndex, -1);
}

void Inventory_Test::unplacedNetworkAndOptedOutFixturesKeepTheOldRules()
{
    g_doc->networkTopology()->nodes() << proxNode("SL Node", 10, 0, { 1 });
    Fixture *loose = makeStaged(g_doc, 4, "Loose");                         // not on the plot
    Fixture *fence = placedStaged(g_doc, 3, "Fence", 9, 0);
    fence->setConnection(Fixture::Network);
    fence->setIPAddress("172.18.2.41");
    Fixture *nearFx = placedStaged(g_doc, 4, "Near SL", 9, 1);

    PatchPlanner pp(g_doc);
    QList<PatchPlanner::Assignment> plan = pp.plan({ loose->id(), fence->id(), nearFx->id() });
    QCOMPARE(forFixture(plan, loose->id()).universe, quint32(0));            // first-fit
    QCOMPARE(forFixture(plan, loose->id()).nodeIndex, -1);
    // Its own, by IP -- a free one, not loose's nor the SL node's.
    QVERIFY(forFixture(plan, fence->id()).universe > 1);
    QCOMPARE(forFixture(plan, fence->id()).nodeIndex, -1);
    QCOMPARE(forFixture(plan, nearFx->id()).universe, quint32(1));             // the SL node's

    pp.setUseProximity(false);
    QVERIFY(!pp.useProximity());
    plan = pp.plan({ nearFx->id() });
    QCOMPARE(plan.first().universe, quint32(0));
    QCOMPARE(plan.first().nodeIndex, -1);
}

void Inventory_Test::nothingNearWithRoomFallsBackToFirstFit()
{
    makeDimmer(g_doc, 512, 1, 0);                        // universe 2 full, and it is the only port
    g_doc->networkTopology()->nodes() << proxNode("Node", 0, 0, { 1 });
    Fixture *f = placedStaged(g_doc, 4, "Stranded", 1, 0);
    const PatchPlanner::Assignment a = PatchPlanner(g_doc).plan({ f->id() }).first();
    QCOMPARE(a.universe, quint32(0));
    QCOMPARE(a.nodeIndex, -1);
    QVERIFY2(a.note.contains("no node near it has room"), qPrintable(a.note));
}

/* "the lights on T1, 2, 3, 4 .. should all be addressed the same in the
   single U": four like trusses, two bars each, one in its own universe;
   T-2's are off by one. Matched to T-1, each bar takes its counterpart's
   address -- paired by where it hangs, not by when it was added -- in its
   own truss's universe. */
void Inventory_Test::likeStructuresArePatchedAlike()
{
    MonitorProperties *props = g_doc->monitorProperties();
    QList<Truss *> towers;
    QHash<QString, QList<Fixture *> > bars;
    const quint32 universes[4] = { 5, 7, 9, 11 };
    for (int t = 0; t < 4; ++t)
    {
        Truss *truss = props->addTruss();
        truss->setName(QString("T-%1").arg(t + 1));
        towers << truss;
        const quint32 shift = (t == 1) ? 1 : 0;       // T-2, one off
        // Added top bar first on T-2, to show the pairing is by place.
        for (int b : (t == 1 ? QList<int>{ 1, 0 } : QList<int>{ 0, 1 }))
        {
            Fixture *fx = makeDimmer(g_doc, 90, universes[t], quint32(b * 90) + shift);
            QVERIFY(fx != nullptr);
            fx->setName(QString("T%1-B%2").arg(t + 1).arg(b + 1));
            FixtureRigProps rp;
            rp.trussId = truss->id();
            rp.trussOffset = 0.5f + b;
            props->setFixtureRigProps(fx->id(), rp);
            bars[truss->name()] << fx;
        }
    }
    // Something else on a truss: not like them.
    Truss *odd = props->addTruss();
    odd->setName("Bar 1");
    Fixture *lone = makeDimmer(g_doc, 90, 20, 0);
    FixtureRigProps rp;
    rp.trussId = odd->id();
    props->setFixtureRigProps(lone->id(), rp);

    const QString t1 = PatchPlanner::mountKey(g_doc, bars["T-1"].first()->id());
    QCOMPARE(t1, QString("truss:%1").arg(towers.first()->id()));
    QCOMPARE(PatchPlanner::structureName(g_doc, t1), QString("T-1"));
    const QStringList like = PatchPlanner::likeStructures(g_doc, t1);
    QStringList names;
    for (const QString &k : like)
        names << PatchPlanner::structureName(g_doc, k);
    QCOMPARE(names, QStringList({ "T-2", "T-3", "T-4" }));

    PatchPlanner planner(g_doc);
    QStringList problems;
    const QList<PatchPlanner::Assignment> plan = planner.matchLike(t1, like, &problems);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
    QCOMPARE(plan.size(), 6);
    QCOMPARE(planner.apply(plan), 2);                  // only T-2's move
    for (int t = 0; t < 4; ++t)
        for (Fixture *fx : bars[towers.at(t)->name()])
        {
            const quint32 b = fx->name().endsWith("B2") ? 1 : 0;
            QCOMPARE(fx->universe(), universes[t]);
            QCOMPARE(fx->address(), b * 90);
        }

    // Asked to match the odd one out: as far as it goes, and said so.
    problems.clear();
    const QString oddKey = PatchPlanner::mountKey(g_doc, lone->id());
    const QList<PatchPlanner::Assignment> partial = planner.matchLike(t1, { oddKey }, &problems);
    QCOMPARE(partial.size(), 1);
    QCOMPARE(partial.first().universe, quint32(20));
    QVERIFY(!problems.isEmpty());
}

QTEST_GUILESS_MAIN(Inventory_Test)

/*****************************************************************************
 * Re-addressing patched fixtures
 *****************************************************************************/

void Inventory_Test::aBlockCanShiftOntoItsOwnChannels()
{
    // Three 4-channel dimmers at 1, 5, 9 moved up by 2: each lands on channels
    // the next one still holds until it moves. One at a time, the first refuses.
    Fixture *a = makeDimmer(g_doc, 4, 0, 0);
    Fixture *b = makeDimmer(g_doc, 4, 0, 4);
    Fixture *c = makeDimmer(g_doc, 4, 0, 8);
    QVERIFY(a && b && c);
    QVERIFY(!g_doc->patchFixture(a->id(), 0, 2));

    QString why;
    QVERIFY2(g_doc->patchFixtures({ { a->id(), 0, 2 }, { b->id(), 0, 6 }, { c->id(), 0, 10 } }, &why),
             qPrintable(why));
    QCOMPARE(a->address(), quint32(2));
    QCOMPARE(c->address(), quint32(10));
    QCOMPARE(g_doc->fixtureForAddress(0), Fixture::invalidId());
    QCOMPARE(g_doc->fixtureForAddress(2), a->id());
    QCOMPARE(g_doc->fixtureForAddress(13), c->id());
}

void Inventory_Test::patchFixturesIsAllOrNothing()
{
    Fixture *a = makeDimmer(g_doc, 4, 0, 0);
    Fixture *b = makeDimmer(g_doc, 4, 0, 4);
    Fixture *wall = makeDimmer(g_doc, 4, 1, 0);
    QVERIFY(a && b && wall);

    // b onto the wall in universe 2: nothing moves, a included.
    QString why;
    QVERIFY(!g_doc->patchFixtures({ { a->id(), 0, 100 }, { b->id(), 1, 2 } }, &why));
    QVERIFY2(why.contains("overlaps"), qPrintable(why));
    QCOMPARE(a->address(), quint32(0));
    QCOMPARE(b->address(), quint32(4));
    QCOMPARE(g_doc->fixtureForAddress(0), a->id());

    // Two entries onto each other, and past the end, are refused too.
    QVERIFY(!g_doc->patchFixtures({ { a->id(), 0, 50 }, { b->id(), 0, 52 } }));
    QVERIFY(!g_doc->patchFixtures({ { a->id(), 0, 510 } }));
}

void Inventory_Test::movingAPatchedFixtureIsDesignModeOnly()
{
    Fixture *a = makeDimmer(g_doc, 4, 0, 0);
    Fixture *staged = makeDimmer(g_doc, 4, 0, 0, false);
    QVERIFY(a && staged);
    g_doc->setMode(Doc::Operate);
    QString why;
    QVERIFY(!g_doc->patchFixtures({ { a->id(), 0, 20 } }, &why));
    QVERIFY2(why.contains("Design"), qPrintable(why));
    // Patching from the inventory has always worked while running, and still does.
    QVERIFY(g_doc->patchFixtures({ { staged->id(), 0, 40 } }));
    g_doc->setMode(Doc::Design);
    QVERIFY(g_doc->patchFixtures({ { a->id(), 0, 20 } }));
}

void Inventory_Test::findFreeAddressSeesThroughTheFixturesMoving()
{
    Fixture *a = makeDimmer(g_doc, 10, 0, 0);
    Fixture *b = makeDimmer(g_doc, 10, 0, 20);
    QVERIFY(a && b);
    QCOMPARE(g_doc->findFreeAddress(0, 10), quint32(10));
    QCOMPARE(g_doc->findFreeAddress(0, 11), quint32(30));
    QCOMPARE(g_doc->findFreeAddress(0, 4, 12), quint32(12));
    QCOMPARE(g_doc->findFreeAddress(0, 11, 0, { a->id() }), quint32(0));
    QCOMPARE(g_doc->findFreeAddress(0, 600), quint32(UINT_MAX));
    QCOMPARE(g_doc->findFreeAddress(0, 10, 505), quint32(UINT_MAX));
}

void Inventory_Test::aClashingFixtureLoadsUnpatchedAndSavesUnchanged()
{
    // Two fixtures in one file on overlapping channels. The second used to be
    // deleted on load, taking itself out of everything that used it.
    Fixture *first = makeDimmer(g_doc, 8, 2, 10);
    Fixture *second = makeDimmer(g_doc, 8, 2, 30);
    QVERIFY(first && second);
    second->setName("Clash");
    const QByteArray x1 = saveFixture(first);
    QByteArray x2 = saveFixture(second);
    QVERIFY(x2.contains("<Address>30</Address>"));
    x2.replace("<Address>30</Address>", "<Address>14</Address>");   // a hand-edited file

    Doc doc2(this);
    for (const QByteArray &xml : { x1, x2 })
    {
        QXmlStreamReader r(xml);
        r.readNextStartElement();
        r.readNextStartElement();
        QVERIFY(Fixture::loader(r, &doc2));
    }
    QCOMPARE(doc2.fixturesCount(), 2);
    Fixture *back = doc2.fixture(second->id());
    QVERIFY(back != nullptr);
    QVERIFY(!back->isPatched());
    QVERIFY(back->addressClash());
    QCOMPARE(back->universe(), quint32(2));
    QCOMPARE(back->address(), quint32(14));
    QCOMPARE(doc2.addressClashes(), QList<quint32>({ second->id() }));
    // The first keeps its channels: the clash writes nothing over it.
    QCOMPARE(doc2.fixtureForAddress((2 << 9) | 14), first->id());

    // Saved, it still asks for universe 3 address 15 -- the file is unchanged.
    const QByteArray again = saveFixture(back);
    QVERIFY(!again.contains("Patched"));
    QVERIFY(again.contains("<Address>14</Address>"));
}

void Inventory_Test::patchingAClashClearsIt()
{
    Fixture *f = makeDimmer(g_doc, 4, 0, 0, false);
    QVERIFY(f);
    f->setAddressClash(true);
    QCOMPARE(g_doc->addressClashes().size(), 1);
    QVERIFY(g_doc->patchFixtures({ { f->id(), 0, 100 } }));
    QVERIFY(!f->addressClash());
    QVERIFY(g_doc->addressClashes().isEmpty());

    // Returning one to the inventory is a decision too.
    Fixture *g = makeDimmer(g_doc, 4, 0, 0, false);
    g->setPatched(true);            // as if it had been patched...
    QVERIFY(g_doc->patchFixtures({ { g->id(), 0, 200 } }));
    g->setAddressClash(true);       // ...and flagged
    QVERIFY(g_doc->unpatchFixture(g->id()));
    QVERIFY(!g->addressClash());
}

void Inventory_Test::placeKeepsSpacingAsOneBlock()
{
    Fixture *a = makeDimmer(g_doc, 4, 0, 10);
    Fixture *b = makeDimmer(g_doc, 4, 0, 30);       // 20 after a
    Fixture *staged = makeDimmer(g_doc, 6, 0, 0, false);
    QVERIFY(a && b && staged);

    PatchPlanner pp(g_doc);
    const QList<PatchPlanner::Assignment> plan =
        pp.place({ b->id(), staged->id(), a->id() }, 1, 100, true);
    QCOMPARE(plan.size(), 3);
    // List order is kept; the block keeps its 20-channel gap.
    QCOMPARE(plan.at(2).fixtureId, a->id());
    QCOMPARE(plan.at(2).universe, quint32(1));
    QCOMPARE(plan.at(2).address, quint32(100));
    QCOMPARE(plan.at(0).address, quint32(120));
    // The staged one packs on after the block.
    QCOMPARE(plan.at(1).universe, quint32(1));
    QCOMPARE(plan.at(1).address, quint32(124));

    // Placed so it runs off the end: said so, not broken up.
    const QList<PatchPlanner::Assignment> off = pp.place({ a->id(), b->id() }, 0, 510, true);
    QVERIFY(off.at(0).note.contains("runs past"));    // 511..514 of a 512 universe
    QCOMPARE(off.at(1).universe, quint32(1));         // 530 -> universe 2, address 19
    QCOMPARE(off.at(1).address, quint32(18));
}

void Inventory_Test::placePacksAroundOthersAndSpills()
{
    Fixture *wall = makeDimmer(g_doc, 10, 0, 4);      // 5..14 stays put
    Fixture *a = makeDimmer(g_doc, 4, 0, 100);
    Fixture *b = makeDimmer(g_doc, 8, 0, 200);
    Fixture *c = makeDimmer(g_doc, 500, 1, 0);
    QVERIFY(wall && a && b && c);

    PatchPlanner pp(g_doc);
    QList<PatchPlanner::Assignment> plan = pp.place({ a->id(), b->id() }, 0, 0, false);
    QCOMPARE(plan.at(0).address, quint32(0));         // 1..4 fits before the wall
    QCOMPARE(plan.at(1).address, quint32(14));        // 8 does not; after it

    // c (500 ch) after a and b in universe 1 no longer fits: on to universe 2,
    // which c itself is leaving -- so it is free.
    plan = pp.place({ a->id(), b->id(), c->id() }, 0, 400, false);
    QCOMPARE(plan.at(0).address, quint32(400));
    QCOMPARE(plan.at(1).address, quint32(404));
    QCOMPARE(plan.at(2).universe, quint32(1));
    QCOMPARE(plan.at(2).address, quint32(0));
}

void Inventory_Test::applyMovesPatchedFixturesTogether()
{
    Fixture *a = makeDimmer(g_doc, 4, 0, 0);
    Fixture *b = makeDimmer(g_doc, 4, 0, 4);
    Fixture *still = makeDimmer(g_doc, 4, 0, 50);
    QVERIFY(a && b && still);

    PatchPlanner pp(g_doc);
    QList<PatchPlanner::Assignment> plan = pp.place({ a->id(), b->id() }, 0, 2, true);
    PatchPlanner::Assignment same;
    same.fixtureId = still->id(); same.universe = 0; same.address = 50;
    plan << same;                                    // left where it is: skipped
    QStringList problems;
    QCOMPARE(pp.apply(plan, &problems), 2);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
    QCOMPARE(a->address(), quint32(2));
    QCOMPARE(b->address(), quint32(6));

    // A plan made stale by an edit since: nothing at all is moved.
    plan = pp.place({ a->id(), b->id() }, 0, 48, false);
    QVERIFY(g_doc->patchFixtures({ { still->id(), 0, 52 } }));
    QCOMPARE(pp.apply(plan, &problems), 0);
    QVERIFY(problems.last().startsWith("Nothing was patched"));
    QCOMPARE(a->address(), quint32(2));
}

void Inventory_Test::patchingNamesTheUniverseTheShowsWay()
{
    InputOutputMap *io = g_doc->inputOutputMap();
    while (io->universesCount() < 6)
        io->addUniverse();
    io->setUniverseName(0, "U01-T1-Ext");
    io->setUniverseName(1, "U02-T1-Int");
    io->setUniverseName(3, "Front");                    // somebody's own name
    // Universes 3 and 5 (indexes 2, 4) are still "Universe N".

    auto patch = [](Fixture *f, quint32 uni, quint32 addr) {
        PatchPlanner::Assignment a;
        a.fixtureId = f->id();
        a.universe = uni;
        a.address = addr;
        return a;
    };

    // One lantern on its own: the scheme, and what is on it.
    Fixture *lantern = makeStaged(g_doc, 15, "Lantern #1");
    QCOMPARE(PatchPlanner(g_doc).apply({ patch(lantern, 2, 0) }), 1);
    QCOMPARE(io->getUniverseNameByID(2), QString("U03-Lantern-1"));

    // Several: what their names share.
    Fixture *p1 = makeStaged(g_doc, 3, "Par 1");
    Fixture *p2 = makeStaged(g_doc, 3, "Par 2");
    QCOMPARE(PatchPlanner(g_doc).apply({ patch(p1, 4, 0), patch(p2, 4, 3) }), 2);
    QCOMPARE(io->getUniverseNameByID(4), QString("U05-Par"));

    // A name somebody gave is never changed.
    Fixture *wash = makeStaged(g_doc, 3, "Wash 1");
    QCOMPARE(PatchPlanner(g_doc).apply({ patch(wash, 3, 0) }), 1);
    QCOMPARE(io->getUniverseNameByID(3), QString("Front"));

    // A show with no scheme of its own: just what is on it.
    io->setUniverseName(0, QString());
    io->setUniverseName(1, QString());
    io->setUniverseName(4, QString());
    QCOMPARE(PatchPlanner::universeNameFor(g_doc, 2), QString("Lantern-1"));
}

void Inventory_Test::networkFixturesTakeAFreeBlockFirst()
{
    InputOutputMap *io = g_doc->inputOutputMap();
    while (io->universesCount() < 12)
        io->addUniverse();
    // In use: 1 and 5-8 (fixtures). Free: 2, 3, 4 (a gap of three) and
    // 9-12 (a block of four).
    makeDimmer(g_doc, 10, 0, 0);
    for (quint32 u = 4; u < 8; ++u)
        makeDimmer(g_doc, 10, u, 0);

    QList<quint32> ids;
    for (int i = 1; i <= 4; ++i)
        ids << makeStaged(g_doc, 15, QString("Lantern #%1").arg(i), Fixture::Network,
                          QString("172.18.2.%1").arg(210 + i))->id();
    const QList<PatchPlanner::Assignment> plan = PatchPlanner(g_doc).plan(ids);
    QList<quint32> got;
    foreach (const PatchPlanner::Assignment &a, plan)
        got << a.universe;
    foreach (const PatchPlanner::Assignment &a, plan)
        QVERIFY(!a.newUniverse);
    // The block they all fit in, together -- not the gap of three first.
    QCOMPARE(got, QList<quint32>({ 8, 9, 10, 11 }));

    // Two more than any block holds: the free ones in order, then new.
    QList<quint32> more;
    for (int i = 5; i <= 10; ++i)
        more << makeStaged(g_doc, 15, QString("Lantern #%1").arg(i), Fixture::Network,
                           QString("172.18.2.%1").arg(210 + i))->id();
    QList<quint32> six;
    foreach (const PatchPlanner::Assignment &a, PatchPlanner(g_doc).plan(more))
        six << a.universe;
    QCOMPARE(six, QList<quint32>({ 1, 2, 3, 8, 9, 10 }));
}

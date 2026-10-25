/*
  Q Light Controller Plus
  venuerig_test.cpp

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
#include <QBuffer>
#include <QtTest>

#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private
#include "venuerig_test.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "venuecorrections.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "iopluginstub.h"
#include "chaseraction.h"
#include "fixturegroup.h"
#include "mastertimer.h"
#include "outputpatch.h"
#include "qlcpalette.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlcfile.h"
#include "universe.h"
#include "venuerig.h"
#include "rigowner.h"
#include "fixture.h"
#include "chaser.h"
#include "doc.h"

#define TESTPLUGINDIR "../iopluginstub"

namespace
{
VenueCorrections *venues(Doc *doc)
{
    return doc->programmer()->venueCorrections();
}

QByteArray engineXml(Doc *doc)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    doc->saveXML(&w);
    w.writeEndDocument();
    return xml;
}

QByteArray fixtureXml(const Fixture *fx)
{
    QByteArray xml;
    QXmlStreamWriter w(&xml);
    fx->saveXML(&w);
    return xml;
}

FixtureGroup *makeGroup(Doc *doc, const QString &name, const QList<quint32> &fixtures,
                        RigOwner::Owner owner = RigOwner::Unset)
{
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName(name);
    g->setOwner(owner);
    doc->addFixtureGroup(g);
    for (quint32 f : fixtures)
        g->assignFixture(f);
    return g;
}

ShowTree::Target groupTarget(quint32 g)
{
    ShowTree::Target t;
    t.groups << g;
    return t;
}

void go(Chaser *c, Doc *doc)
{
    ChaserAction action;
    action.m_action = ChaserNextStep;
    action.m_stepIndex = -1;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    c->setAction(action);
    if (!c->isRunning())
        c->start(doc->masterTimer(), FunctionParent::master());
}

IOPluginStub *stub(Doc *doc)
{
    return static_cast<IOPluginStub *>(doc->ioPluginCache()->plugin(QStringLiteral("I/O Plugin Stub")));
}
} // namespace

void VenueRig_Test::initTestCase()
{
}

void VenueRig_Test::init()
{
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_doc = new Doc(this);
    QDir dir(TESTPLUGINDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    venues(m_doc)->setShowFile(showFile(), false);
}

void VenueRig_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(3);
    delete m_doc;
    m_doc = nullptr;
    delete m_dir;
    m_dir = nullptr;
}

QString VenueRig_Test::showFile() const
{
    return m_dir->filePath(QStringLiteral("Tour.qlcc"));
}

/* @p count one-channel dimmers from @p address (zero-based), named
   "<name> 1", "<name> 2"...; the first one's id. */
quint32 VenueRig_Test::dimmers(int count, quint32 universe, quint32 address, const QString &name)
{
    quint32 first = Fixture::invalidId();
    for (int i = 0; i < count; ++i)
    {
        Fixture *fx = new Fixture(m_doc);
        fx->setName(QStringLiteral("%1 %2").arg(name).arg(i + 1));
        fx->setUniverse(universe);
        fx->setAddress(address + quint32(i));
        fx->setChannels(1);
        if (!m_doc->addFixture(fx))
        {
            delete fx;
            return Fixture::invalidId();
        }
        if (i == 0)
            first = fx->id();
    }
    return first;
}

void VenueRig_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        for (Universe *u : ua)
            u->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
}

uchar VenueRig_Test::dmx(quint32 universe, quint32 address)
{
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    const QByteArray v = ua.at(int(universe))->preGMValues();
    m_doc->inputOutputMap()->releaseUniverses(false);
    return int(address) < v.size() ? uchar(v.at(int(address))) : 0;
}

/****************************************************************************
 * Owners
 ****************************************************************************/

void VenueRig_Test::ownersRoundTrip()
{
    const quint32 id = dimmers(2, 0, 0, QStringLiteral("Par"));
    Fixture *a = m_doc->fixture(id);
    Fixture *b = m_doc->fixture(id + 1);
    QSignalSpy changed(a, &Fixture::changed);
    a->setOwner(RigOwner::House);
    QCOMPARE(changed.count(), 1);
    a->setOwner(RigOwner::House);
    QCOMPARE(changed.count(), 1);       // the same: nothing said

    const QByteArray ax = fixtureXml(a);
    QVERIFY(ax.contains("<Owner>House</Owner>"));
    QVERIFY(!fixtureXml(b).contains("<Owner>"));     // unset: nothing written

    QXmlStreamReader r(ax);
    r.readNextStartElement();
    Fixture loaded(m_doc);
    QVERIFY(loaded.loadXML(r, m_doc, m_doc->fixtureDefCache()));
    QCOMPARE(loaded.owner(), RigOwner::House);

    FixtureGroup *g = makeGroup(m_doc, QStringLiteral("Rentals"), { id + 1 }, RigOwner::Rental);
    QByteArray gx;
    {
        QXmlStreamWriter w(&gx);
        g->saveXML(&w);
    }
    QVERIFY(gx.contains("<Owner>Rental</Owner>"));
    QXmlStreamReader gr(gx);
    gr.readNextStartElement();
    FixtureGroup lg(m_doc);
    QVERIFY(lg.loadXML(gr));
    QCOMPARE(lg.owner(), RigOwner::Rental);

    QCOMPARE(RigOwner::fromXmlName(QStringLiteral("house")), RigOwner::House);
    QCOMPARE(RigOwner::fromXmlName(QStringLiteral("Lamp")), RigOwner::Unset);
    QCOMPARE(RigOwner::xmlName(RigOwner::Unset), QString());
}

void VenueRig_Test::groupOwnerIsInheritedAndOverridden()
{
    const quint32 f = dimmers(4, 0, 0, QStringLiteral("Par"));
    FixtureGroup *house = makeGroup(m_doc, QStringLiteral("FOH wash"), { f, f + 1 }, RigOwner::House);
    FixtureGroup *rental = makeGroup(m_doc, QStringLiteral("Side L"), { f + 1, f + 2 }, RigOwner::Rental);
    QVERIFY(house->id() < rental->id());

    quint32 from = 0;
    QCOMPARE(RigOwner::effective(m_doc, f, &from), RigOwner::House);
    QCOMPARE(from, house->id());
    // In two groups that disagree: the first (lowest id) wins.
    QCOMPARE(RigOwner::effective(m_doc, f + 1, &from), RigOwner::House);
    QCOMPARE(from, house->id());
    QCOMPARE(RigOwner::effective(m_doc, f + 2), RigOwner::Rental);
    // In no group: Tour.
    QCOMPARE(RigOwner::effective(m_doc, f + 3, &from), RigOwner::Tour);
    QCOMPARE(from, UINT_MAX);

    // A fixture's own owner overrides its group.
    m_doc->fixture(f + 1)->setOwner(RigOwner::Tour);
    QCOMPARE(RigOwner::effective(m_doc, f + 1, &from), RigOwner::Tour);
    QCOMPARE(from, UINT_MAX);
    // A group set back to Unset: its fixtures are Tour again.
    house->setOwner(RigOwner::Unset);
    QCOMPARE(RigOwner::effective(m_doc, f), RigOwner::Tour);

    const QHash<quint32, RigOwner::Owner> all = RigOwner::effectiveAll(m_doc);
    for (const Fixture *fx : m_doc->fixtures())
        QCOMPARE(all.value(fx->id()), RigOwner::effective(m_doc, fx->id()));
}

void VenueRig_Test::slotOwnerRoundTrip()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    iom->setUniverseSlotOwner(1, RigOwner::House);
    QCOMPARE(iom->universeSlotOwner(1), RigOwner::House);
    QCOMPARE(iom->universeSlotOwner(0), RigOwner::Tour);
    // Unset is Tour: a slot is one or the other.
    iom->setUniverseSlotOwner(2, RigOwner::Unset);
    QCOMPARE(iom->universeSlotOwner(2), RigOwner::Tour);

    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        iom->universes().at(1)->saveXML(&w);
    }
    QVERIFY(xml.contains("Owner=\"House\""));
    QByteArray tour;
    {
        QXmlStreamWriter w(&tour);
        iom->universes().at(0)->saveXML(&w);
    }
    QVERIFY(!tour.contains("Owner="));

    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(iom->universes().at(3)->loadXML(r, 3, iom));
    QCOMPARE(iom->universeSlotOwner(3), RigOwner::House);
}

/****************************************************************************
 * The venue's house rig
 ****************************************************************************/

void VenueRig_Test::houseFixturesMoveIntoTheVenue()
{
    VenueCorrections *v = venues(m_doc);
    const quint32 tour = dimmers(1, 0, 0, QStringLiteral("Tour spot"));
    FixtureGroup *foh = makeGroup(m_doc, QStringLiteral("FOH wash"), {}, RigOwner::House);
    QVERIFY(v->setActiveVenue(v->addVenue(QStringLiteral("Lincoln HS"))));

    // A fixture added to a House group with a venue applied: the venue's.
    const quint32 par = dimmers(1, 0, 100, QStringLiteral("Lincoln par"));
    m_doc->monitorProperties()->setFixturePosition(par, 0, 0, QVector3D(1200, 3400, 0));
    QVERIFY(!v->isVenueFixture(par));
    foh->assignFixture(par);
    QVERIFY(v->isVenueFixture(par));
    QVERIFY(m_doc->isVenueFixture(par));
    QVERIFY(!v->isVenueFixture(tour));

    // The show file has none of it: not the fixture, its head, its place.
    const QByteArray show = engineXml(m_doc);
    QVERIFY(show.contains("Tour spot 1"));
    QVERIFY(!show.contains("Lincoln par"));
    QVERIFY(!show.contains(QStringLiteral("Fixture=\"%1\"").arg(par).toUtf8()));
    QVERIFY(!show.contains(QStringLiteral("<FxItem ID=\"%1\"").arg(par).toUtf8()));
    QVERIFY(show.contains("<Owner>House</Owner>"));     // the group's own owner is the show's

    // The venue's file has it, its place and its head in the group.
    v->recordRig();
    QFile side(v->filePath());
    QVERIFY(side.open(QIODevice::ReadOnly));
    const QByteArray sidecar = side.readAll();
    QVERIFY(sidecar.contains("Version=\"2\""));
    QVERIFY(sidecar.contains("<House>"));
    QVERIFY(sidecar.contains("Lincoln par 1"));
    QVERIFY(sidecar.contains(QStringLiteral("<Fill Group=\"%1\" Name=\"FOH wash\">").arg(foh->id()).toUtf8()));
    const VenueCorrections::Venue lv = v->venue(QStringLiteral("Lincoln HS"));
    QCOMPARE(lv.rig.fixtures.size(), 1);
    QCOMPARE(lv.rig.fixtures.first().id, par);
    QCOMPARE(lv.rig.fixtures.first().address, 100u);
    QVERIFY(lv.rig.monitorXml.contains("3400"));

    // Set back to Tour, it comes on tour: the show's again.
    m_doc->fixture(par)->setOwner(RigOwner::Tour);
    QVERIFY(!v->isVenueFixture(par));
    QVERIFY(engineXml(m_doc).contains("Lincoln par 1"));
    QVERIFY(v->venue(QStringLiteral("Lincoln HS")).rig.fixtures.isEmpty());
}

void VenueRig_Test::switchingVenuesSwapsTheHouseRig()
{
    VenueCorrections *v = venues(m_doc);
    const quint32 tour = dimmers(2, 0, 0, QStringLiteral("Tour spot"));
    m_doc->monitorProperties()->setFixturePosition(tour, 0, 0, QVector3D(500, 600, 0));
    FixtureGroup *foh = makeGroup(m_doc, QStringLiteral("FOH wash"), {}, RigOwner::House);
    const QByteArray tourBefore = fixtureXml(m_doc->fixture(tour));

    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    const QString b = v->addVenue(QStringLiteral("Grand Hall"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 pa = dimmers(2, 0, 100, QStringLiteral("Lincoln par"));
    foh->assignFixture(pa);
    foh->assignFixture(pa + 1);
    QVERIFY(v->setActiveVenue(b));
    // Lincoln's house rig is out; nothing else changed.
    QVERIFY(m_doc->fixture(pa) == nullptr);
    QVERIFY(m_doc->fixture(pa + 1) == nullptr);
    QCOMPARE(foh->headList().size(), 0);
    const quint32 pb = dimmers(3, 0, 200, QStringLiteral("Hall par"));
    for (int i = 0; i < 3; ++i)
        foh->assignFixture(pb + quint32(i));
    QCOMPARE(foh->headList().size(), 3);

    m_doc->resetModified();
    QVERIFY(v->setActiveVenue(a));
    QVERIFY(!m_doc->isModified());          // a switch is not an edit to the show
    QCOMPARE(foh->headList().size(), 2);
    QVERIFY(m_doc->fixture(pa) != nullptr);
    QCOMPARE(m_doc->fixture(pa)->name(), QStringLiteral("Lincoln par 1"));
    QCOMPARE(m_doc->fixture(pa)->address(), 100u);
    QVERIFY(m_doc->fixture(pb) == nullptr);
    // The tour package: the same fixtures, ids, addresses and place.
    QCOMPARE(fixtureXml(m_doc->fixture(tour)), tourBefore);
    QCOMPARE(m_doc->monitorProperties()->fixturePosition(tour, 0, 0), QVector3D(500, 600, 0));

    QVERIFY(v->setActiveVenue(b));
    QCOMPARE(foh->headList().size(), 3);
    QCOMPARE(m_doc->fixture(pb + 2)->name(), QStringLiteral("Hall par 3"));

    // None: the show as built -- the tour package only.
    QVERIFY(v->setActiveVenue(QString()));
    QVERIFY(!m_doc->isModified());
    QCOMPARE(m_doc->fixtures().size(), 2);
    QCOMPARE(foh->headList().size(), 0);
    QCOMPARE(fixtureXml(m_doc->fixture(tour)), tourBefore);

    // A restart: the show opened again applies the venue it was left on.
    QVERIFY(v->setActiveVenue(a));
    const QByteArray show = engineXml(m_doc);
    Doc reopened(this);
    {
        QXmlStreamReader r(show);
        r.readNextStartElement();
        QVERIFY(reopened.loadXML(r, false));
    }
    QCOMPARE(reopened.fixtures().size(), 2);
    venues(&reopened)->setShowFile(showFile(), true);
    QCOMPARE(venues(&reopened)->activeVenue(), a);
    QCOMPARE(reopened.fixtures().size(), 4);
    QVERIFY(reopened.fixture(pa + 1) != nullptr);
    FixtureGroup *rfoh = reopened.fixtureGroup(foh->id());
    QVERIFY(rfoh != nullptr);
    QCOMPARE(rfoh->headList().size(), 2);
}

void VenueRig_Test::aGroupCuePlaysOnEachVenuesHouseRig()
{
    VenueCorrections *v = venues(m_doc);
    dimmers(1, 0, 0, QStringLiteral("Tour spot"));
    FixtureGroup *foh = makeGroup(m_doc, QStringLiteral("FOH wash"), {}, RigOwner::House);
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    const QString b = v->addVenue(QStringLiteral("Grand Hall"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 pa = dimmers(1, 0, 100, QStringLiteral("Lincoln par"));
    foh->assignFixture(pa);
    QVERIFY(v->setActiveVenue(b));
    const quint32 pb = dimmers(2, 0, 200, QStringLiteral("Hall par"));
    foh->assignFixture(pb);
    foh->assignFixture(pb + 1);

    // A cue programmed at home, written against the group.
    QVERIFY(v->setActiveVenue(a));
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, m_doc);
    full->setName(QStringLiteral("Full"));
    full->setValue(255);
    m_doc->addPalette(full);
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction(QStringLiteral("Tour"));
    const quint32 list = st->addCueList(prod, QStringLiteral("Act 1"));
    const quint32 cue = st->addCue(list, -1, QStringLiteral("FOH up"));
    st->setLayerLook(cue, st->addLayer(cue, groupTarget(foh->id())), st->lookForPalette(full->id()));
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QVERIFY(chaser != nullptr);

    go(chaser, m_doc);
    tick(6);
    QCOMPARE(int(dmx(0, 100)), 255);       // Lincoln's par
    QCOMPARE(int(dmx(0, 200)), 0);
    QCOMPARE(int(dmx(0, 0)), 0);           // the tour spot is not in the group
    chaser->stop(FunctionParent::master());
    tick(4);

    // The same cue in the next room: whatever house rig fills the group.
    QVERIFY(v->setActiveVenue(b));
    go(chaser, m_doc);
    tick(6);
    QCOMPARE(int(dmx(0, 200)), 255);
    QCOMPARE(int(dmx(0, 201)), 255);
    QCOMPARE(int(dmx(0, 100)), 0);
    chaser->stop(FunctionParent::master());
    tick(4);
}

void VenueRig_Test::aTakenIdIsRenumbered()
{
    VenueCorrections *v = venues(m_doc);
    FixtureGroup *foh = makeGroup(m_doc, QStringLiteral("FOH wash"), {}, RigOwner::House);
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 par = dimmers(1, 0, 100, QStringLiteral("Lincoln par"));
    foh->assignFixture(par);
    m_doc->monitorProperties()->setFixturePosition(par, 0, 0, QVector3D(700, 800, 0));
    QVERIFY(v->setActiveVenue(QString()));

    // The show took that id while the venue was away (edited elsewhere).
    Fixture *intruder = new Fixture(m_doc);
    intruder->setName(QStringLiteral("Tour spot"));
    intruder->setChannels(1);
    intruder->setAddress(0);
    QVERIFY(m_doc->addFixture(intruder, par));

    QVERIFY(v->setActiveVenue(a));
    QCOMPARE(m_doc->fixture(par)->name(), QStringLiteral("Tour spot"));
    const QList<VenueRig::HouseFixture> house = v->venue(a).rig.fixtures;
    QCOMPARE(house.size(), 1);
    const quint32 moved = house.first().id;
    QVERIFY(moved != par);
    QCOMPARE(m_doc->fixture(moved)->name(), QStringLiteral("Lincoln par 1"));
    QVERIFY(v->isVenueFixture(moved));
    QVERIFY(!v->isVenueFixture(par));
    // Its head and its place came with it.
    QVERIFY(foh->fixtureList().contains(moved));
    QVERIFY(!foh->fixtureList().contains(par));
    QCOMPARE(m_doc->monitorProperties()->fixturePosition(moved, 0, 0), QVector3D(700, 800, 0));
    QCOMPARE(v->venue(a).rig.fills.first().heads.first().fixture, moved);
}

void VenueRig_Test::newShowFixturesSkipHouseIds()
{
    VenueCorrections *v = venues(m_doc);
    dimmers(2, 0, 0, QStringLiteral("Tour spot"));
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 par = dimmers(1, 0, 100, QStringLiteral("Lincoln par"));
    m_doc->fixture(par)->setOwner(RigOwner::House);
    QVERIFY(v->isVenueFixture(par));
    QVERIFY(v->setActiveVenue(QString()));
    QVERIFY(m_doc->fixture(par) == nullptr);
    QVERIFY(v->isReservedFixtureId(par));
    // New show fixtures never take a house light's id.
    for (int i = 0; i < 4; ++i)
    {
        const quint32 id = dimmers(1, 1, quint32(i), QStringLiteral("New"));
        QVERIFY(id != par);
    }
    QVERIFY(v->setActiveVenue(a));
    QCOMPARE(m_doc->fixture(par)->name(), QStringLiteral("Lincoln par 1"));
}

/****************************************************************************
 * The sidecar
 ****************************************************************************/

void VenueRig_Test::sidecarRoundTrip()
{
    VenueCorrections *v = venues(m_doc);
    m_doc->inputOutputMap()->setOutputPatch(0, stub(m_doc)->name(), QString(), 0);
    dimmers(1, 0, 0, QStringLiteral("Tour spot"));
    FixtureGroup *foh = makeGroup(m_doc, QStringLiteral("FOH wash"), {}, RigOwner::House);
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 par = dimmers(2, 0, 100, QStringLiteral("Lincoln par"));
    foh->assignFixture(par);
    foh->assignFixture(par + 1);
    VenueRig::NotOurs whole;
    whole.universe = 3;
    VenueRig::NotOurs range;
    range.universe = 0;
    range.from = 400;
    range.to = 511;
    QVERIFY(v->setNotOurs(a, { whole, range }));
    QVERIFY(v->keepPatch());
    QVERIFY(v->recordRig());

    const QByteArray xml = v->toXml();
    QVERIFY(xml.contains("<NotOurs Universe=\"4\"/>"));
    QVERIFY(xml.contains("<NotOurs Universe=\"1\" From=\"401\" To=\"512\"/>"));
    QVERIFY(xml.contains("<Patch Universe=\"0\">"));

    Doc other(this);
    VenueCorrections back(&other);
    QString why;
    QVERIFY2(back.fromXml(xml, &why), qPrintable(why));
    const VenueCorrections::Venue lv = back.venue(a);
    QCOMPARE(lv.rig.fixtures.size(), 2);
    QCOMPARE(lv.rig.fixtures.at(1).name, QStringLiteral("Lincoln par 2"));
    QCOMPARE(lv.rig.fixtures.at(1).address, 101u);
    QCOMPARE(lv.rig.fixtures.at(1).channels, 1u);
    QCOMPARE(lv.rig.fills.size(), 1);
    QCOMPARE(lv.rig.fills.first().groupName, QStringLiteral("FOH wash"));
    QCOMPARE(lv.rig.fills.first().heads.size(), 2);
    QCOMPARE(lv.rig.notOurs.size(), 2);
    QVERIFY(lv.rig.notOurs.contains(whole));
    QVERIFY(lv.rig.notOurs.contains(range));
    QCOMPARE(lv.rig.patch.value(0).size(), 1);
    QCOMPARE(lv.rig.patch.value(0).first().plugin, stub(m_doc)->name());
    QVERIFY(!lv.rig.monitorXml.isEmpty());
    QVERIFY(back.isReservedFixtureId(par + 1));
    // Written again, the same bytes (but for the show it is beside).
    QCOMPARE(back.toXml(), QByteArray(xml).replace(" Show=\"Tour.qlcc\"", ""));
}

void VenueRig_Test::oldShowsAndVenuesAreUnchanged()
{
    VenueCorrections *v = venues(m_doc);
    dimmers(2, 0, 0, QStringLiteral("Tour spot"));
    makeGroup(m_doc, QStringLiteral("Pars"), { 0, 1 });
    const QByteArray show = engineXml(m_doc);
    QVERIFY(!show.contains("Owner"));

    // A venue with only corrections: written as before, version 1.
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    QCOMPARE(engineXml(m_doc), show);           // applying changed nothing
    const QByteArray xml = v->toXml();
    QVERIFY(xml.contains("Version=\"1\""));
    QVERIFY(!xml.contains("<House"));
    QVERIFY(!xml.contains("<Patch"));
    QVERIFY(!xml.contains("<NotOurs"));
    QVERIFY(v->venue(a).rig.isEmpty());
    QVERIFY(v->clashes(a).isEmpty());

    // A version 1 file reads with nothing owned.
    VenueCorrections back(m_doc);
    QVERIFY(back.fromXml("<QlcVenues Version=\"1\" Active=\"Old\"><Venue Name=\"Old\">"
                         "<Fixture ID=\"0\" PanOfs=\"1.00\" TiltOfs=\"0.00\" PanZero=\"0.0\"/>"
                         "</Venue></QlcVenues>"));
    QVERIFY(back.venue(QStringLiteral("Old")).rig.isEmpty());
    QCOMPARE(back.venue(QStringLiteral("Old")).fixtures.size(), 1);
}

/****************************************************************************
 * The clash check, not ours, the patch
 ****************************************************************************/

void VenueRig_Test::theClashCheck()
{
    VenueCorrections *v = venues(m_doc);
    const quint32 tour = dimmers(10, 0, 0, QStringLiteral("Tour"));
    const quint32 onTwo = dimmers(1, 1, 0, QStringLiteral("Tour U2"));
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 par = dimmers(1, 0, 50, QStringLiteral("House par"));
    m_doc->fixture(par)->setOwner(RigOwner::House);
    QVERIFY(v->isVenueFixture(par));
    QVERIFY(v->clashes(a).isEmpty());
    QVERIFY(v->setActiveVenue(QString()));

    // Meanwhile a tour fixture moved onto the house par's address.
    QVERIFY(m_doc->patchFixtures({ Doc::FixturePatch{ tour + 9, 0, 50 } }));

    // A dry run, before going live: the overlap is listed.
    QList<VenueRig::Clash> clashes = v->clashes(a);
    QCOMPARE(clashes.size(), 1);
    QCOMPARE(clashes.first().kind, VenueRig::Clash::Overlap);
    QCOMPARE(clashes.first().universe, 0u);
    QCOMPARE(clashes.first().address, 50u);
    QVERIFY(clashes.first().text.contains(QStringLiteral("House par 1")));
    QVERIFY(clashes.first().text.contains(QStringLiteral("Tour 10")));
    QVERIFY(clashes.first().text.contains(QStringLiteral("U1 51")));

    // Not ours: a tour fixture on the house desk's channels is listed.
    VenueRig::NotOurs keep;
    keep.universe = 0;
    keep.from = 0;
    keep.to = 2;
    QVERIFY(v->setNotOurs(a, { keep }));
    clashes = v->clashes(a);
    int notOurs = 0;
    for (const VenueRig::Clash &c : clashes)
        notOurs += c.kind == VenueRig::Clash::NotOursKind ? 1 : 0;
    QCOMPARE(notOurs, 3);

    // Slots: only once the show has House slots.
    m_doc->inputOutputMap()->setUniverseSlotOwner(1, RigOwner::House);
    clashes = v->clashes(a);
    bool slot = false;
    for (const VenueRig::Clash &c : clashes)
        if (c.kind == VenueRig::Clash::Slot && c.text.contains(QStringLiteral("Tour U2 1")))
            slot = true;
    QVERIFY(slot);
    Q_UNUSED(onTwo)

    // Going live anyway: the house par goes in unpatched, keeping its
    // address, so it never drives the tour fixture's channel; the note says.
    QSignalSpy note(v, &VenueCorrections::note);
    QVERIFY(v->setActiveVenue(a));
    QVERIFY(m_doc->fixture(par) != nullptr);
    QVERIFY(!m_doc->fixture(par)->isPatched());
    QVERIFY(m_doc->fixture(par)->addressClash());
    QCOMPARE(m_doc->fixture(par)->address(), 50u);
    QVERIFY(note.count() >= 1);
    QVERIFY(note.first().first().toString().contains(QStringLiteral("House par 1")));
    QVERIFY(!v->clashes(a).isEmpty());
}

void VenueRig_Test::notOursIsNeverSent()
{
    // How a range is typed: 1-based, a hyphen or an en dash, one address.
    int from = 0, to = 0;
    QVERIFY(VenueRig::parseRange(QStringLiteral("401-512"), from, to));
    QCOMPARE(from, 400);
    QCOMPARE(to, 511);
    QVERIFY(VenueRig::parseRange(QStringLiteral(" 5 \u2013 8 "), from, to));
    QCOMPARE(from, 4);
    QCOMPARE(to, 7);
    QVERIFY(VenueRig::parseRange(QStringLiteral("12"), from, to));
    QCOMPARE(from, 11);
    QCOMPARE(to, 11);
    QVERIFY(!VenueRig::parseRange(QStringLiteral("0-4"), from, to));
    QVERIFY(!VenueRig::parseRange(QStringLiteral("9-3"), from, to));
    QVERIFY(!VenueRig::parseRange(QStringLiteral("500-513"), from, to));
    QVERIFY(!VenueRig::parseRange(QStringLiteral("stage left"), from, to));

    VenueCorrections *v = venues(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    IOPluginStub *plugin = stub(m_doc);
    QVERIFY(plugin != nullptr);
    QVERIFY(iom->setOutputPatch(0, plugin->name(), QString(), 0));
    QVERIFY(iom->setOutputPatch(1, plugin->name(), QString(), 1));
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    VenueRig::NotOurs whole;
    whole.universe = 1;
    VenueRig::NotOurs range;
    range.universe = 0;
    range.from = 4;
    range.to = 7;
    QVERIFY(v->setNotOurs(a, { whole, range }));
    QVERIFY(v->setActiveVenue(a));

    const QByteArray full(512, char(255));
    plugin->m_universe.fill(char(0xAA));
    QList<Universe *> unis = iom->universes();
    unis.at(0)->dumpOutput(full, true);
    unis.at(1)->dumpOutput(full, true);
    // U1: everything but 5-8 (1-based) goes out; those are zero.
    for (int c = 0; c < 4; ++c)
        QCOMPARE(uchar(plugin->m_universe.at(c)), uchar(255));
    for (int c = 4; c <= 7; ++c)
        QCOMPARE(uchar(plugin->m_universe.at(c)), uchar(0));
    QCOMPARE(uchar(plugin->m_universe.at(8)), uchar(255));
    // U2: nothing at all was sent.
    for (int c = 512; c < 1024; ++c)
        QCOMPARE(uchar(plugin->m_universe.at(c)), uchar(0xAA));
    // Blind and Blackout send nothing there either.
    unis.at(1)->setInhibitOutput(true);
    unis.at(1)->dumpOutput(full, true);
    unis.at(1)->setInhibitOutput(false);
    QCOMPARE(uchar(plugin->m_universe.at(600)), uchar(0xAA));
    QVERIFY(unis.at(0)->isNotOurs(5));
    QVERIFY(!unis.at(0)->isNotOurs(8));
    QVERIFY(unis.at(1)->isNotOurs(300));

    // A frame through the engine, a fixture asking for full there.
    Fixture *fx = new Fixture(m_doc);
    fx->setName(QStringLiteral("Stray"));
    fx->setChannels(1);
    fx->setPatched(false);
    QVERIFY(m_doc->addFixture(fx));
    QString why;
    QVERIFY(!m_doc->patchFixtures({ Doc::FixturePatch{ fx->id(), 0, 5 } }, &why));
    QVERIFY(why.contains(QStringLiteral("not ours")));
    QVERIFY(m_doc->patchFixtures({ Doc::FixturePatch{ fx->id(), 0, 9 } }, &why));

    // None: it all goes out again.
    QVERIFY(v->setActiveVenue(QString()));
    unis.at(0)->dumpOutput(full, true);
    unis.at(1)->dumpOutput(full, true);
    QCOMPARE(uchar(plugin->m_universe.at(5)), uchar(255));
    QCOMPARE(uchar(plugin->m_universe.at(600)), uchar(255));
}

void VenueRig_Test::theVenuePatchIsAppliedAndRestored()
{
    VenueCorrections *v = venues(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    IOPluginStub *plugin = stub(m_doc);
    QVERIFY(iom->setOutputPatch(0, plugin->name(), QString(), 0));
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    QVERIFY(!v->hasPatch(a));

    // The room's network: repatched while the venue is applied, it is the
    // venue's patch -- every slot -- and the show keeps its own.
    QVERIFY(iom->setOutputPatch(0, plugin->name(), QString(), 2));
    QCOMPARE(iom->outputPatch(0)->output(), 2u);
    QVERIFY(v->hasPatch(a));
    QCOMPARE(v->venue(a).rig.patch.value(0).first().line, 2u);
    QCOMPARE(v->venue(a).rig.patch.size(), int(iom->universesCount()));
    const QByteArray show = engineXml(m_doc);
    QVERIFY(show.contains("Line=\"0\""));
    QVERIFY(!show.contains("Line=\"2\""));

    QVERIFY(v->setActiveVenue(QString()));
    QCOMPARE(iom->outputPatch(0)->output(), 0u);           // the show's back
    QVERIFY(!iom->universes().at(0)->hasShowOutputBase());
    QVERIFY(v->setActiveVenue(a));
    QCOMPARE(iom->outputPatch(0)->output(), 2u);           // the room's on

    // The sidecar has it; the show file never does.
    QFile side(v->filePath());
    QVERIFY(side.open(QIODevice::ReadOnly));
    QVERIFY(side.readAll().contains("Line=\"2\""));
    QVERIFY(!engineXml(m_doc).contains("Line=\"2\""));

    // Forgotten: the show's patch is on again, the venue has none.
    QVERIFY(v->forgetPatch());
    QCOMPARE(iom->outputPatch(0)->output(), 0u);
    QVERIFY(!v->hasPatch(a));
    QVERIFY(v->setActiveVenue(QString()));
    QCOMPARE(iom->outputPatch(0)->output(), 0u);
}

void VenueRig_Test::moveTheShowsHouseFixturesIn()
{
    VenueCorrections *v = venues(m_doc);
    dimmers(1, 0, 0, QStringLiteral("Tour spot"));
    // House with no venue applied: a label; it stays in the show.
    const quint32 par = dimmers(1, 0, 100, QStringLiteral("Home par"));
    m_doc->fixture(par)->setOwner(RigOwner::House);
    QVERIFY(engineXml(m_doc).contains("Home par 1"));

    // Applying a venue moves nothing by itself.
    const QString a = v->addVenue(QStringLiteral("Home"));
    QVERIFY(v->setActiveVenue(a));
    QVERIFY(!v->isVenueFixture(par));
    QCOMPARE(v->rig()->showHouseFixtures(), QList<quint32>() << par);

    m_doc->resetModified();
    QCOMPARE(v->moveShowHouseIn(), 1);
    QVERIFY(m_doc->isModified());           // the show file loses it
    QVERIFY(v->isVenueFixture(par));
    QVERIFY(!engineXml(m_doc).contains("Home par 1"));
    QCOMPARE(v->venue(a).rig.fixtures.size(), 1);
    QVERIFY(v->rig()->showHouseFixtures().isEmpty());

    QVERIFY(v->setActiveVenue(QString()));
    QVERIFY(m_doc->fixture(par) == nullptr);   // Home's now
}

void VenueRig_Test::aPracticeKeepsTheHouseRig()
{
    VenueCorrections *v = venues(m_doc);
    // A tour mover to scramble.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Test"));
    def->setModel(QStringLiteral("Venue Rig Spot"));
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("3ch"));
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
    QLCFixtureHead head;
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QStringLiteral("Ch %1").arg(k));
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
        head.addChannel(quint32(k));
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));
    Fixture *spot = new Fixture(m_doc);
    spot->setName(QStringLiteral("Tour spot"));
    spot->setFixtureDefinition(def, mode);
    spot->setAddress(0);
    QVERIFY(m_doc->addFixture(spot));

    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 par = dimmers(1, 0, 100, QStringLiteral("Lincoln par"));
    m_doc->fixture(par)->setOwner(RigOwner::House);
    QVERIFY(v->isVenueFixture(par));

    // The room's own lights stay up while the student practises.
    const QString practice = v->startPractice({ spot->id() }, 7);
    QVERIFY(!practice.isEmpty());
    QCOMPARE(v->activeVenue(), practice);
    QVERIFY(m_doc->fixture(par) != nullptr);
    QVERIFY(v->isVenueFixture(par));
    QVERIFY(v->endPractice());
    QCOMPARE(v->activeVenue(), a);
    QVERIFY(m_doc->fixture(par) != nullptr);
    QCOMPARE(v->venue(a).rig.fixtures.size(), 1);
}

QTEST_GUILESS_MAIN(VenueRig_Test)

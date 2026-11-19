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
#include <QRegularExpression>
#include <QSignalSpy>
#include <QBuffer>
#include <QTimer>
#include <QtTest>

#include "../common/stdprelude.h"
#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#include "ioplugincache.h"
#undef protected
#undef private
#include "venuerig_test.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "outputrulekeeper.h"
#include "focussession.h"
#include "stagetarget.h"
#include "scene.h"
#include "venuecorrections.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "iopluginstub.h"
#include "chaseraction.h"
#include "fixturegroup.h"
#include "grouphead.h"
#include "qlcpoint.h"
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
#include "venueimport.h"
#include "../common/pmslikeshow.h"
#include "../common/resource_paths.h"
#include "qlcfixturedefcache.h"
#include "networkroute.h"
#include "qlcioplugin.h"
#include "showembedding.h"
#include "rigowner.h"
#include "fixture.h"
#include "chaser.h"
#include "doc.h"
#include "teachingsettings.h"

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

/* A 540/270 spot (pan, tilt, dimmer) at @p address, on the plot at (x, y)
   metres, hung at 5 m: a light Focus mode and a practice can move. */
Fixture *makeSpot(Doc *doc, const QString &name, quint32 address, float x, float y)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Test"));
    def->setModel(QStringLiteral("Venue Spot ") + name);
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
    if (!doc->fixtureDefCache()->addFixtureDef(def))
    {
        delete def;
        return nullptr;
    }
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(0);
    fx->setAddress(address);
    if (!doc->addFixture(fx))
    {
        delete fx;
        return nullptr;
    }
    doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(x * 1000.0f, y * 1000.0f, 5.0f));
    return fx;
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(data) == data.size();
}

/* Venue @p name's element in a sidecar, as written. */
QByteArray venueSection(const QByteArray &xml, const QString &name)
{
    const QRegularExpression re(QStringLiteral("<Venue Name=\"%1\"[^>]*>.*?</Venue>")
                                    .arg(QRegularExpression::escape(name)),
                                QRegularExpression::DotMatchesEverythingOption);
    return re.match(QString::fromUtf8(xml)).captured(0).toUtf8();
}

/* A scene with every channel of @p fixtures at full. */
Scene *fullScene(Doc *doc, const QList<quint32> &fixtures)
{
    Scene *s = new Scene(doc);
    for (quint32 id : fixtures)
        s->setValue(id, 0, 255);
    doc->addFunction(s);
    return s;
}
/* One fixture of @p channels generic dimmer channels (a head each), named
   @p name exactly, its owner set before it is added (so only the add can
   tell a venue applied of it); null when the doc refused it. */
Fixture *addFixture(Doc *doc, const QString &name, quint32 universe, quint32 address, quint32 channels,
                    RigOwner::Owner owner = RigOwner::Unset, bool patched = true)
{
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    fx->setUniverse(universe);
    fx->setAddress(address);
    fx->setChannels(channels);
    fx->setOwner(owner);
    fx->setPatched(patched);
    if (!doc->addFixture(fx))
    {
        delete fx;
        return nullptr;
    }
    return fx;
}

/* A house fixture as the venue keeps it, for a dry run by hand. */
VenueRig::HouseFixture houseFixture(const QString &name, quint32 universe, quint32 address, quint32 channels)
{
    VenueRig::HouseFixture hf;
    hf.id = 9000 + address + universe * 512;
    hf.name = name;
    hf.universe = universe;
    hf.address = address;
    hf.channels = channels;
    return hf;
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
    // Without it the patch tests would crash, not fail.
    QVERIFY2(stub(m_doc) != nullptr, "the I/O plugin stub (engine/test/iopluginstub) is not built");
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

void VenueRig_Test::aVenueOffLeavesNoVenueFixtures()
{
    VenueCorrections *v = venues(m_doc);
    dimmers(1, 0, 0, QStringLiteral("Tour spot"));
    FixtureGroup *foh = makeGroup(m_doc, QStringLiteral("FOH wash"), {}, RigOwner::House);
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 par = dimmers(1, 0, 100, QStringLiteral("Lincoln par"));
    foh->assignFixture(par);
    QVERIFY(v->isVenueFixture(par));

    // A house fixture deleted while applied: its id is not the venue's any more.
    const quint32 gone = dimmers(1, 0, 110, QStringLiteral("Lincoln gone"));
    foh->assignFixture(gone);
    QVERIFY(v->isVenueFixture(gone));
    QVERIFY(m_doc->deleteFixture(gone));
    QVERIFY(!v->isVenueFixture(gone));
    Fixture *reuse = new Fixture(m_doc);
    reuse->setName(QStringLiteral("Tour reuse"));
    reuse->setUniverse(0);
    reuse->setAddress(120);
    reuse->setChannels(1);
    QVERIFY(m_doc->addFixture(reuse, gone));
    QVERIFY(!v->isVenueFixture(gone));
    QVERIFY(engineXml(m_doc).contains("Tour reuse"));

    // The venue off: its house rig goes, and nothing is the venue's.
    QVERIFY(v->setActiveVenue(QString()));
    QVERIFY(m_doc->fixture(par) == nullptr);
    QVERIFY(!v->isVenueFixture(par));
    QVERIFY(!m_doc->isVenueFixture(par));

    // A show fixture given the old house id is the show's, saved with it.
    Fixture *held = new Fixture(m_doc);
    held->setName(QStringLiteral("Tour held"));
    held->setUniverse(0);
    held->setAddress(200);
    held->setChannels(1);
    QVERIFY(m_doc->addFixture(held, par));
    QVERIFY(!v->isVenueFixture(par));
    QVERIFY(engineXml(m_doc).contains("Tour held"));

    // Set to House with no venue applied: still the show's.
    held->setOwner(RigOwner::House);
    QVERIFY(!v->isVenueFixture(par));
    QVERIFY(engineXml(m_doc).contains("Tour held"));
    held->setOwner(RigOwner::Unset);

    // Applied again, then the show replaced (reset): the same holds.
    QVERIFY(v->setActiveVenue(a));
    const quint32 next = dimmers(1, 0, 300, QStringLiteral("Lincoln next"));
    foh->assignFixture(next);
    QVERIFY(v->isVenueFixture(next));
    v->setShowFile(m_dir->filePath(QStringLiteral("Other.qlcc")), true);
    QVERIFY(!v->isActive());
    QVERIFY(!v->isVenueFixture(next));
    QVERIFY(engineXml(m_doc).contains("Lincoln next"));
    m_doc->fixture(next)->setOwner(RigOwner::House);
    QVERIFY(!v->isVenueFixture(next));
    QVERIFY(engineXml(m_doc).contains("Lincoln next"));
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
    /* Mid-universe, so the end is not clamped: read back 2 too far, it
       grew on every load and save (mutation gap 3, RC6). */
    VenueRig::NotOurs mid;
    mid.universe = 1;
    mid.from = 4;
    mid.to = 7;
    QVERIFY(v->setNotOurs(a, { whole, range, mid }));
    QVERIFY(v->keepPatch());
    QVERIFY(v->recordRig());

    const QByteArray xml = v->toXml();
    QVERIFY(xml.contains("<NotOurs Universe=\"4\"/>"));
    QVERIFY(xml.contains("<NotOurs Universe=\"1\" From=\"401\" To=\"512\"/>"));
    QVERIFY(xml.contains("<NotOurs Universe=\"2\" From=\"5\" To=\"8\"/>"));
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
    QCOMPARE(lv.rig.notOurs.size(), 3);
    QVERIFY(lv.rig.notOurs.contains(whole));
    QVERIFY(lv.rig.notOurs.contains(range));
    QVERIFY(lv.rig.notOurs.contains(mid));
    QCOMPARE(lv.rig.patch.value(0).size(), 1);
    QCOMPARE(lv.rig.patch.value(0).first().plugin, stub(m_doc)->name());
    QVERIFY(!lv.rig.monitorXml.isEmpty());
    QVERIFY(back.isReservedFixtureId(par + 1));
    // Written again, the same bytes (but for the show it is beside).
    QCOMPARE(back.toXml(), QByteArray(xml).replace(" Show=\"Tour.qlcc\"", ""));
    // And again: the range stays 5-8 however often it goes round.
    Doc third(this);
    VenueCorrections again(&third);
    QVERIFY2(again.fromXml(back.toXml(), &why), qPrintable(why));
    QVERIFY(again.venue(a).rig.notOurs.contains(mid));
    QCOMPARE(again.toXml(), back.toXml());
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

    // The sidecar has it; the show file never does. (Closed again before
    // forgetPatch() rewrites it: Windows will not replace an open file.)
    {
        QFile side(v->filePath());
        QVERIFY(side.open(QIODevice::ReadOnly));
        QVERIFY(side.readAll().contains("Line=\"2\""));
    }
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
    m_doc->teachingSettings()->setEnabled(true);
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

/****************************************************************************
 * Not ours on every output path (docs/design/VENUES.md section 5)
 ****************************************************************************/

/* Not ours is zero in every frame we send (CHANGELOG "House rig, tour
   package and universes per venue"): in the frame Blind sends when it holds
   the Blackout-exempt channels (Universe::dumpOutput's blindHoldsExempt
   branch), and under an output patch's blackout -- even on a house light
   that is exempt from both. */
void VenueRig_Test::notOursStaysZeroThroughBlindAndBlackout()
{
    VenueCorrections *v = venues(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    IOPluginStub *plugin = stub(m_doc);
    QVERIFY(plugin != nullptr);
    QVERIFY(iom->setOutputPatch(0, plugin->name(), QString(), 0));
    const quint32 stage = dimmers(1, 0, 0, QStringLiteral("Stage"));
    // Two house lights, Ignore Blackout: one on the house desk's channels.
    const quint32 kept = dimmers(1, 0, 5, QStringLiteral("Kept"));
    const quint32 house = dimmers(1, 0, 10, QStringLiteral("House"));
    for (quint32 id : { kept, house })
        m_doc->fixture(id)->setOutputRules(Fixture::ExemptBlackout);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->outputRuleKeeper()->setBlindHoldsExempt(true);
    fullScene(m_doc, { stage, kept, house })->start(m_doc->masterTimer(), FunctionParent::master());

    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    VenueRig::NotOurs range;
    range.universe = 0;
    range.from = 4;
    range.to = 7;
    QVERIFY(v->setNotOurs(a, { range }));
    QVERIFY(v->setActiveVenue(a));

    auto wire = [plugin](int c) { return int(uchar(plugin->m_universe.at(c))); };
    tick(4);
    QCOMPARE(int(dmx(0, 5)), 255);             // the show asks for it...
    QCOMPARE(wire(0), 255);
    QCOMPARE(wire(5), 0);                      // ...it is not sent
    QCOMPARE(wire(10), 255);

    // Blackout (every output patch's): the exempt house light stays up.
    QVERIFY(iom->setBlackout(true));
    tick(2);
    QCOMPARE(wire(0), 0);
    QCOMPARE(wire(10), 255);
    QCOMPARE(wire(5), 0);
    QVERIFY(iom->setBlackout(false));

    // Blind, holding the exempt channels: a Blackout frame goes out.
    plugin->m_universe.fill(char(0xAA));
    iom->setOutputInhibited(true);
    tick(2);
    QCOMPARE(wire(10), 255);                   // held through Blind
    QCOMPARE(wire(0), 0);
    QCOMPARE(wire(5), 0);                      // not ours: zero, not the live 255
    iom->setOutputInhibited(false);
    tick(2);
    QCOMPARE(wire(0), 255);
    QCOMPARE(wire(5), 0);
}

/* The same through the real tick: the master timer's thread runs the
   functions and wakes each universe's own thread, which computes the frame
   and sends it -- no frame computed by hand. */
void VenueRig_Test::notOursOnTheRealTick()
{
    VenueCorrections *v = venues(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    IOPluginStub *plugin = stub(m_doc);
    QVERIFY(plugin != nullptr);
    QVERIFY(iom->setOutputPatch(0, plugin->name(), QString(), 0));
    QVERIFY(iom->setOutputPatch(1, plugin->name(), QString(), 1));
    const quint32 u1 = dimmers(9, 0, 0, QStringLiteral("U1"));
    const quint32 u2 = dimmers(2, 1, 0, QStringLiteral("U2"));
    QList<quint32> all;
    for (quint32 i = 0; i < 9; ++i)
        all << u1 + i;
    all << u2 << u2 + 1;
    Scene *full = fullScene(m_doc, all);

    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    VenueRig::NotOurs whole;
    whole.universe = 1;
    VenueRig::NotOurs range;
    range.universe = 0;
    range.from = 4;
    range.to = 7;
    QVERIFY(v->setNotOurs(a, { whole, range }));
    QVERIFY(v->setActiveVenue(a));

    plugin->m_universe.fill(char(0xAA));
    QList<Universe *> unis = iom->universes();
    iom->startUniverses();
    QTRY_VERIFY(unis.at(0)->isRunning() && unis.at(1)->isRunning());
    MasterTimer *mt = m_doc->masterTimer();
    mt->start();
    full->start(mt, FunctionParent::master());

    auto wire = [plugin](int c) { return int(uchar(plugin->m_universe.at(c))); };
    QTRY_COMPARE_WITH_TIMEOUT(wire(0), 255, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(wire(8), 255, 5000);
    // A few more frames on the whole-not-ours universe's own thread.
    const quint32 frames0 = unis.at(0)->m_frame.load();
    const quint32 frames1 = unis.at(1)->m_frame.load();
    QTRY_VERIFY_WITH_TIMEOUT(unis.at(0)->m_frame.load() >= frames0 + 3, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(unis.at(1)->m_frame.load() >= frames1 + 3, 5000);

    // Read while it runs (stopping the timer stops every function), a
    // whole frame at a time: each frame zeroes the intensity channels before
    // writing them again, so a single-value read can land in between and
    // see 0 for a dimmer at full (CI 40809651206, macOS).
    QCOMPARE(int(uchar(unis.at(1)->preGMValues().at(0))), 255);  // the show plays there...
    QCOMPARE(int(uchar(unis.at(0)->preGMValues().at(5))), 255);
    for (int c = 4; c <= 7; ++c)
        QCOMPARE(wire(c), 0);                       // ...a range is zero
    QCOMPARE(wire(3), 255);
    for (int c = 512; c < 1024; ++c)                // ...and U2 gets nothing at all
        QCOMPARE(wire(c), 0xAA);
    mt->stop();
}

/* "Patching there refused" (CHANGELOG): the Patch dialog's and the Lighting
   Studio's way in (Doc::patchFixtures) is refused -- notOursIsNeverSent --
   and so must every other way a fixture gets an address on the house desk's
   channels: added with one (the Add Fixture dialog, a RGB panel, Doc::
   addFixture), or a patched one's address changed (the fixture edit dialog,
   Fixture::setAddress). */
void VenueRig_Test::patchingOntoNotOursIsRefusedEveryWay()
{
    VenueCorrections *v = venues(m_doc);
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    VenueRig::NotOurs range;
    range.universe = 0;
    range.from = 4;
    range.to = 7;
    QVERIFY(v->setNotOurs(a, { range }));
    QVERIFY(v->setActiveVenue(a));
    auto onNotOurs = [](const Fixture *fx) {
        return fx != nullptr && fx->isPatched() && fx->universe() == 0
               && fx->address() + fx->channels() > 4 && fx->address() <= 7;
    };

    // Added with an address there.
    Fixture *added = new Fixture(m_doc);
    added->setName(QStringLiteral("Added"));
    added->setChannels(1);
    added->setUniverse(0);
    added->setAddress(5);
    const bool in = m_doc->addFixture(added);
    if (!in)
        delete added;
    QVERIFY(!in);                                   // refused, as an overlap is
    QVERIFY(m_doc->isOnNotOurs(0, 5, 1));
    QVERIFY(!m_doc->isOnNotOurs(0, 8, 4));

    // A patched fixture's address changed onto them.
    const quint32 moved = dimmers(1, 0, 9, QStringLiteral("Moved"));
    QVERIFY(moved != Fixture::invalidId());
    m_doc->fixture(moved)->setAddress(6);
    QVERIFY(!onNotOurs(m_doc->fixture(moved)));
    QVERIFY(m_doc->fixture(moved)->isPatched());    // refused: it stays where it was
    QCOMPARE(m_doc->fixture(moved)->address(), 9u);
    QCOMPARE(m_doc->fixtureForAddress(9), moved);
    QCOMPARE(m_doc->fixtureForAddress(6), Fixture::invalidId());
    // Moved somewhere free, it goes.
    m_doc->fixture(moved)->setAddress(12);
    QCOMPARE(m_doc->fixture(moved)->address(), 12u);
    QCOMPARE(m_doc->fixtureForAddress(12), moved);

    // The refused way, for contrast: still refused, with why.
    const quint32 other = dimmers(1, 0, 20, QStringLiteral("Other"));
    QString why;
    QVERIFY(!m_doc->patchFixtures({ Doc::FixturePatch{ other, 0, 4 } }, &why));
    QVERIFY(why.contains(QStringLiteral("not ours")));
    QCOMPARE(m_doc->fixture(other)->address(), 20u);
}

/* The clash check runs on open (CHANGELOG: "before Apply now, on open and in
   the Patch dialog"): the show reopened after it was edited elsewhere, its
   sidecar's venue applied again, says what clashes -- an overlap, a tour
   fixture on channels the venue keeps, a fixture in the other's slot -- and
   a house fixture that clashes goes in unpatched, whatever the clash
   (docs/design/VENUES.md section 6, Q138): overlapping a tour one, on channels the
   venue keeps, or in a Tour slot. A tour fixture that clashes stays as the
   show has it (nothing is sent on not-ours channels anyway). */
void VenueRig_Test::theClashCheckRunsOnOpen()
{
    VenueCorrections *v = venues(m_doc);
    const quint32 tour = dimmers(10, 0, 0, QStringLiteral("Tour"));
    dimmers(1, 1, 0, QStringLiteral("Tour U2"));
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 par = dimmers(1, 0, 50, QStringLiteral("House par"));
    m_doc->fixture(par)->setOwner(RigOwner::House);
    VenueRig::NotOurs keep;
    keep.universe = 0;
    keep.from = 60;
    keep.to = 62;
    QVERIFY(v->setNotOurs(a, { keep }));
    QVERIFY(v->recordRig());
    QVERIFY(v->clashes(a).isEmpty());
    const QByteArray show = engineXml(m_doc);
    const QByteArray sidecar = readFile(v->filePath());
    QVERIFY(sidecar.contains("Active=\"Lincoln HS\""));
    QVERIFY(sidecar.contains("House par 1"));

    enum Edit { MovedOntoTheHousePar, MovedOntoNotOurs, HouseSlot, HouseParOnNotOurs };
    for (Edit edit : { MovedOntoTheHousePar, MovedOntoNotOurs, HouseSlot, HouseParOnNotOurs })
    {
        QByteArray sc = sidecar;
        if (edit == HouseParOnNotOurs)
        {
            // The venue file edited: the room now keeps the par's channel.
            QVERIFY(sc.contains("From=\"61\" To=\"63\""));
            sc.replace("From=\"61\" To=\"63\"", "From=\"51\" To=\"51\"");
        }
        QVERIFY(writeFile(v->filePath(), sc));
        Doc reopened(this);
        {
            QXmlStreamReader r(show);
            r.readNextStartElement();
            QVERIFY(reopened.loadXML(r, false));
        }
        QVERIFY(reopened.fixture(par) == nullptr);      // the venue's, not the show's
        // Edited elsewhere, meanwhile.
        VenueRig::Clash::Kind kind = VenueRig::Clash::Overlap;
        QString words;
        switch (edit)
        {
            case MovedOntoTheHousePar:
                QVERIFY(reopened.patchFixtures({ Doc::FixturePatch{ tour + 9, 0, 50 } }));
                kind = VenueRig::Clash::Overlap;
                words = QStringLiteral("on the same addresses");
            break;
            case MovedOntoNotOurs:
                QVERIFY(reopened.patchFixtures({ Doc::FixturePatch{ tour + 9, 0, 61 } }));
                kind = VenueRig::Clash::NotOursKind;
                words = QStringLiteral("not ours");
            break;
            case HouseSlot:
                reopened.inputOutputMap()->setUniverseSlotOwner(1, RigOwner::House);
                kind = VenueRig::Clash::Slot;
                words = QStringLiteral("House slot");
            break;
            case HouseParOnNotOurs:
                kind = VenueRig::Clash::NotOursKind;
                words = QStringLiteral("not ours");
            break;
        }

        VenueCorrections *rv = venues(&reopened);
        QSignalSpy note(rv, &VenueCorrections::note);
        rv->setShowFile(showFile(), true);
        QCOMPARE(rv->activeVenue(), a);
        QVERIFY2(note.count() >= 1, qPrintable(QString("edit %1: no note").arg(int(edit))));
        const QString text = note.first().first().toString();
        QVERIFY2(text.contains(a) && text.contains(words), qPrintable(text));
        bool listed = false;
        for (const VenueRig::Clash &c : rv->clashes(a))
            listed = listed || c.kind == kind;
        QVERIFY(listed);

        Fixture *hp = reopened.fixture(par);
        QVERIFY(hp != nullptr);
        QCOMPARE(hp->address(), 50u);
        if (edit == MovedOntoNotOurs)
        {
            QVERIFY(hp->isPatched());               // nothing of its own clashes
            // The tour fixture stays where the show has it.
            QVERIFY(reopened.fixture(tour + 9)->isPatched());
            QCOMPARE(reopened.fixture(tour + 9)->address(), 61u);
        }
        else
        {
            /* In, unpatched, keeping its address: it never drives Tour 10,
               the room's channel, or the tour's slot (HouseSlot: universe 1
               is now a House slot, so the par's universe 0 is a Tour one). */
            QVERIFY2(!hp->isPatched(), qPrintable(QString("edit %1").arg(int(edit))));
            QVERIFY(hp->addressClash());
            QVERIFY(reopened.fixtureForAddress(50) != par);
            if (edit == MovedOntoTheHousePar)
                QCOMPARE(reopened.fixtureForAddress(50), tour + 9);
            // Still listed at the address it asked for, and saved so.
            QVERIFY(rv->recordRig());
            QVERIFY(readFile(rv->filePath()).contains("House par 1"));
        }
    }
}

/* A sidecar from a newer build (docs/design/VENUES.md section 7: the version goes up
   only when a venue owns more) is read for what this build knows: version
   3, unknown elements between and inside the venues, inside <House> and
   <Practice>, unknown attributes -- every correction, trim, target, check,
   the house rig, not ours and the patch still load, from the file on disk,
   and save back. */
void VenueRig_Test::aSidecarFromANewerBuildStillLoads()
{
    VenueCorrections *v = venues(m_doc);
    MonitorProperties *props = m_doc->monitorProperties();
    QVERIFY(m_doc->inputOutputMap()->setOutputPatch(0, stub(m_doc)->name(), QString(), 0));
    Fixture *spot = makeSpot(m_doc, QStringLiteral("Spot"), 0, 2.0f, 2.0f);
    QVERIFY(spot != nullptr);
    FixtureGroup *foh = makeGroup(m_doc, QStringLiteral("FOH wash"), {}, RigOwner::House);
    Truss *pipe = props->addTruss();
    pipe->setOrigin(QVector3D(0.0f, 0.0f, 5.0f));
    StageTarget *mark = props->addStageTarget();
    mark->setName(QStringLiteral("DSC"));
    mark->setPosition(QVector3D(1.0f, 2.0f, 0.0f));
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));

    // Everything a venue keeps.
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
    FixtureRigProps rp = props->fixtureRigProps(spot->id());
    rp.panOffsetDeg = 1.5f;
    rp.tiltOffsetDeg = -0.75f;
    props->setFixtureRigProps(spot->id(), rp);
    QVERIFY(v->record(spot->id()));
    QVERIFY(v->recordTrim(pipe->id()));
    pipe->setOrigin(QVector3D(0.0f, 0.0f, 6.5f));
    QVERIFY(v->recordTrim(pipe->id()));
    QVERIFY(v->recordTarget(mark->id()));
    mark->setPosition(QVector3D(1.5f, 2.0f, 0.0f));
    QVERIFY(v->recordTarget(mark->id()));
    v->setCheckedBy(QStringLiteral("Sam"));
    QVERIFY(v->markChecked({ spot->id() }));
    // Another venue, and a practice kept for its report.
    v->addVenue(QStringLiteral("Grand Hall"));
    m_doc->teachingSettings()->setEnabled(true);
    const QString practice = v->startPractice({ spot->id() }, 7);
    QVERIFY(!practice.isEmpty());
    QVERIFY(v->endPractice());
    QCOMPARE(v->activeVenue(), a);

    const QByteArray known = v->toXml();
    QVERIFY(known.contains("Version=\"2\""));
    QVERIFY(known.contains("<Practice "));
    QVERIFY(known.contains("<House>"));
    QVERIFY(known.contains("<Trim "));
    QVERIFY(known.contains("<Target "));
    QVERIFY(known.contains("<Check "));

    // What a newer build might write around it.
    QString future = QString::fromUtf8(known);
    future.replace(QStringLiteral("Version=\"2\""), QStringLiteral("Version=\"3\" Edition=\"future\""));
    future.replace(QRegularExpression(QStringLiteral("(<Venue )")),
                   QStringLiteral("<Rooms Count=\"3\"><Room/></Rooms>\n\\1Colour=\"teal\" "));
    future.replace(QRegularExpression(QStringLiteral("(<Venue [^>]*[^/]>)")),
                   QStringLiteral("\\1<Weather Rain=\"1\"><Forecast/></Weather>"));
    future.replace(QRegularExpression(QStringLiteral("(<Fixture ID=)")),
                   QStringLiteral("<Haze Level=\"2\"/>\\1"));
    future.replace(QStringLiteral("<Fixture ID="), QStringLiteral("<Fixture Hue=\"4\" ID="));
    future.replace(QStringLiteral("<House>"), QStringLiteral("<House><Rack Name=\"DSR\"><Slot/></Rack>"));
    future.replace(QRegularExpression(QStringLiteral("(<Practice [^>]*[^/]>)")),
                   QStringLiteral("\\1<Coach Name=\"Ana\"><Tip/></Coach>"));
    future.replace(QStringLiteral("</QlcVenues>"), QStringLiteral("<Trailer/></QlcVenues>"));
    QVERIFY(future.contains(QStringLiteral("<Coach ")));
    QVERIFY(future.contains(QStringLiteral("<Rack ")));
    QVERIFY(future.contains(QStringLiteral("<Weather ")));

    const QString futureShow = m_dir->filePath(QStringLiteral("Future.qlcc"));
    QVERIFY(writeFile(VenueCorrections::sidecarPath(futureShow), future.toUtf8()));

    Doc other(this);
    VenueCorrections reference(&other);
    reference.setShowFile(futureShow, false);
    QVERIFY(reference.fromXml(known));
    VenueCorrections back(&other);
    back.setShowFile(futureShow, false);
    QString why;
    QVERIFY2(back.load(&why), qPrintable(why));

    // Everything known, as this build would have written it, and what it
    // doesn't know kept: take those out and it is the reference.
    const QStringList kept({ QStringLiteral("<Rooms Count=\"3\">"), QStringLiteral("<Weather Rain=\"1\">"),
                             QStringLiteral("<Haze Level=\"2\"/>"), QStringLiteral("<Rack Name=\"DSR\">"),
                             QStringLiteral("<Coach Name=\"Ana\">"), QStringLiteral("<Trailer/>") });
    auto withoutKept = [](QString xml) {
        xml.remove(QRegularExpression(QStringLiteral("\\s*<Rooms [^>]*>\\s*<Room/>\\s*</Rooms>")));
        xml.remove(QRegularExpression(QStringLiteral("\\s*<Weather [^>]*>\\s*<Forecast/>\\s*</Weather>")));
        xml.remove(QRegularExpression(QStringLiteral("\\s*<Haze [^>]*/>")));
        xml.remove(QRegularExpression(QStringLiteral("\\s*<Rack [^>]*>\\s*<Slot/>\\s*</Rack>")));
        xml.remove(QRegularExpression(QStringLiteral("\\s*<Coach [^>]*>\\s*<Tip/>\\s*</Coach>")));
        xml.remove(QRegularExpression(QStringLiteral("\\s*<Trailer/>")));
        return xml;
    };
    {
        const QString got = QString::fromUtf8(back.toXml());
        for (const QString &k : kept)
            QVERIFY2(got.contains(k), qPrintable(k));
        QCOMPARE(withoutKept(got), QString::fromUtf8(reference.toXml()));
    }
    QCOMPARE(back.activeVenue(), a);
    QCOMPARE(back.venueNames(), v->venueNames());
    const VenueCorrections::Venue lv = back.venue(a);
    QCOMPARE(lv.fixtures.size(), 1);
    QCOMPARE(lv.fixtures.value(spot->id()).panOffsetDeg, 1.5f);
    QCOMPARE(lv.fixtures.value(spot->id()).tiltOffsetDeg, -0.75f);
    QCOMPARE(lv.trims.value(pipe->id()), 6.5f);
    QCOMPARE(lv.targets.value(mark->id()), QVector3D(1.5f, 2.0f, 0.0f));
    QCOMPARE(lv.checks.value(spot->id()).by, QStringLiteral("Sam"));
    QCOMPARE(lv.rig.fixtures.size(), 2);
    QCOMPARE(lv.rig.fixtures.at(1).name, QStringLiteral("Lincoln par 2"));
    QCOMPARE(lv.rig.fills.size(), 1);
    QCOMPARE(lv.rig.fills.first().heads.size(), 2);
    QCOMPARE(lv.rig.notOurs.size(), 2);
    QVERIFY(lv.rig.notOurs.contains(whole));
    QVERIFY(lv.rig.notOurs.contains(range));
    QCOMPARE(lv.rig.patch.value(0).first().plugin, stub(m_doc)->name());
    const VenueCorrections::Venue pv = back.venue(practice);
    QVERIFY(pv.practice.on);
    QCOMPARE(pv.practice.from, a);
    QVERIFY(pv.practice.truth.contains(spot->id()));
    QVERIFY(back.isReservedFixtureId(par + 1));

    // Saved back and read again: the same, what it doesn't know included
    // (kept through the round trip, not dropped on the next save).
    QVERIFY2(back.save(&why), qPrintable(why));
    const QByteArray saved = readFile(back.filePath());
    QVERIFY(saved.contains("Version=\"2\""));
    for (const QString &k : kept)
        QVERIFY2(saved.contains(k.toUtf8()), qPrintable(k));
    VenueCorrections again(&other);
    again.setShowFile(futureShow, false);
    QVERIFY2(again.load(&why), qPrintable(why));
    QCOMPARE(again.toXml(), back.toXml());
    QCOMPARE(withoutKept(QString::fromUtf8(again.toXml())), QString::fromUtf8(reference.toXml()));
}

/* Scramble practice (docs/design/FOCUS_REFINE_DESIGN.md section 13) moves the lights in
   a practice venue only: the show file keeps the real calibration -- while
   the scramble is on, after the student saves, after the show is saved --
   and the real venue's section of the sidecar is never touched. */
void VenueRig_Test::aPracticeNeverReachesTheShowFile()
{
    VenueCorrections *v = venues(m_doc);
    MonitorProperties *props = m_doc->monitorProperties();
    Fixture *a = makeSpot(m_doc, QStringLiteral("A"), 0, 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, QStringLiteral("B"), 3, 6.0f, 2.0f);
    QVERIFY(a != nullptr && b != nullptr);
    StageTarget *mark = props->addStageTarget();
    mark->setName(QStringLiteral("Mark"));
    mark->setPosition(QVector3D(4.0f, 6.0f, 0.0f));
    // The show's own calibration, and the room's correction on top.
    FixtureRigProps rp = props->fixtureRigProps(a->id());
    rp.panOffsetDeg = 3.0f;
    rp.tiltOffsetDeg = -2.0f;
    props->setFixtureRigProps(a->id(), rp);
    rp = props->fixtureRigProps(b->id());
    rp.tiltOffsetDeg = 1.0f;
    props->setFixtureRigProps(b->id(), rp);
    const QString lincoln = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(lincoln));
    rp = props->fixtureRigProps(a->id());
    rp.panOffsetDeg = 1.0f;
    props->setFixtureRigProps(a->id(), rp);
    QVERIFY(v->record(a->id()));
    QVERIFY(v->hasCorrection(a->id()));

    m_doc->teachingSettings()->setEnabled(true);        // a practice is a Teaching show's
    const QByteArray show = engineXml(m_doc);
    QVERIFY(show.contains("PanOfs=\"3.00\""));
    QVERIFY(show.contains("TiltOfs=\"-2.00\""));
    const QByteArray lincolnBefore = venueSection(readFile(v->filePath()), lincoln);
    QVERIFY(lincolnBefore.contains("PanOfs=\"1.00\""));

    QString why;
    const QString practice = v->startPractice({ a->id(), b->id() }, 42, 2.0f, 8.0f, &why);
    QVERIFY2(!practice.isEmpty(), qPrintable(why));
    for (Fixture *fx : { a, b })
    {
        RigCalibration truth;
        QVERIFY(v->practiceTruth(fx->id(), truth));
        QVERIFY(RigCalibration::of(props->fixtureRigProps(fx->id())) != truth);   // scrambled
    }
    QCOMPARE(engineXml(m_doc), show);
    QCOMPARE(venueSection(readFile(v->filePath()), lincoln), lincolnBefore);

    // The student focuses A and saves it, in the practice.
    FocusSession *s = m_doc->programmer()->focusSession();
    s->setFlashEnabled(false);
    FocusSession::Options o;
    o.fixtures = { a->id(), b->id() };
    o.order = FocusSession::AsGiven;
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = mark->id();
    QVERIFY2(s->start(o, &why), qPrintable(why));
    QCOMPARE(s->currentFixture(), a->id());
    s->nudge(1.0f, -0.5f);
    s->save();
    s->stop();
    QCOMPARE(v->practiceScore(practice).saves, 1);
    QCOMPARE(engineXml(m_doc), show);
    QCOMPARE(venueSection(readFile(v->filePath()), lincoln), lincolnBefore);

    // The show saved mid-practice (its file name told again).
    v->setShowFile(showFile(), false);
    QCOMPARE(engineXml(m_doc), show);
    QCOMPARE(venueSection(readFile(v->filePath()), lincoln), lincolnBefore);

    QVERIFY(v->endPractice());
    QCOMPARE(v->activeVenue(), lincoln);
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 1.0f);
    QCOMPARE(engineXml(m_doc), show);
}

/****************************************************************************
 * Mutation gaps in venuerig.cpp (the RC6 survivors)
 ****************************************************************************/

/* The venue's patch is every universe's, not only the first's: a repatch of
   U2 alone is kept (patchChanged() looks past U1), and put on again when
   the venue is (apply() walks every universe of it). */
void VenueRig_Test::aVenuePatchOnASecondUniverse()
{
    VenueCorrections *v = venues(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    IOPluginStub *plugin = stub(m_doc);
    QVERIFY(iom->setOutputPatch(0, plugin->name(), QString(), 0));
    QVERIFY(iom->setOutputPatch(1, plugin->name(), QString(), 1));
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    QVERIFY(!v->hasPatch(a));

    QVERIFY(iom->setOutputPatch(1, plugin->name(), QString(), 3));
    QVERIFY(v->hasPatch(a));
    QCOMPARE(v->venue(a).rig.patch.value(0).first().line, 0u);
    QCOMPARE(v->venue(a).rig.patch.value(1).first().line, 3u);

    QVERIFY(v->setActiveVenue(QString()));
    QCOMPARE(iom->outputPatch(1)->output(), 1u);            // the show's back
    QVERIFY(v->setActiveVenue(a));
    QCOMPARE(iom->outputPatch(0)->output(), 0u);
    QCOMPARE(iom->outputPatch(1)->output(), 3u);            // the room's on
}

/* The show replaced (VenueCorrections::reset, VenueRig::forget): no
   universe still holds the old show's patch as its base, and nothing is
   not ours -- Universe::reset() clears neither. */
void VenueRig_Test::resetClearsTheUniverses()
{
    VenueCorrections *v = venues(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, stub(m_doc)->name(), QString(), 0));
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    VenueRig::NotOurs range;
    range.universe = 0;
    range.from = 4;
    range.to = 7;
    QVERIFY(v->setNotOurs(a, { range }));
    QVERIFY(v->setActiveVenue(a));
    QVERIFY(v->keepPatch());
    QList<Universe *> unis = iom->universes();
    QVERIFY(unis.at(0)->isNotOurs(5));
    QVERIFY(unis.at(0)->hasShowOutputBase());

    v->setShowFile(m_dir->filePath(QStringLiteral("Other.qlcc")), true);
    QVERIFY(!v->isActive());
    for (Universe *u : iom->universes())
    {
        QVERIFY(!u->hasShowOutputBase());
        for (int c : { 0, 5, 7, 300 })
            QVERIFY(!u->isNotOurs(c));
    }
}

/* Two house fixtures, one of two heads, filling a group at cells that
   differ in x and y. Returns the venue; the fixtures from @p first. */
static QString captureSetup(Doc *doc, FixtureGroup *&foh, quint32 &first, quint32 &duo)
{
    VenueCorrections *v = venues(doc);
    addFixture(doc, QStringLiteral("Tour spot"), 0, 0, 1);
    foh = makeGroup(doc, QStringLiteral("FOH wash"), {});
    makeGroup(doc, QStringLiteral("Side"), {});
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    if (!v->setActiveVenue(a))
        return QString();
    first = addFixture(doc, QStringLiteral("Lincoln par 1"), 0, 100, 1, RigOwner::House)->id();
    addFixture(doc, QStringLiteral("Lincoln par 2"), 0, 101, 1, RigOwner::House);
    addFixture(doc, QStringLiteral("Lincoln par 3"), 0, 102, 1, RigOwner::House);
    duo = addFixture(doc, QStringLiteral("Lincoln duo"), 0, 110, 2, RigOwner::House)->id();
    doc->monitorProperties()->setFixturePosition(first, 0, 0, QVector3D(1200, 3400, 0));
    foh->setSize(QSize(3, 3));
    foh->assignHead(QLCPoint(1, 2), GroupHead(first, 0));
    foh->assignHead(QLCPoint(2, 1), GroupHead(first + 1, 0));
    foh->assignHead(QLCPoint(1, 1), GroupHead(duo, 0));
    v->recordRig();
    return a;
}

/* Nothing changed: capture() says so, however often -- and reads no
   further than the lists it has (a spurious "changed" only rewrote the
   sidecar; reading past them is undefined). */
void VenueRig_Test::captureSeesNoChange()
{
    VenueCorrections *v = venues(m_doc);
    FixtureGroup *foh = nullptr;
    quint32 first = 0, duo = 0;
    const QString a = captureSetup(m_doc, foh, first, duo);
    QVERIFY(!a.isEmpty());

    VenueRig::Data copy = v->venue(a).rig;
    QCOMPARE(copy.fixtures.size(), 4);
    QCOMPARE(copy.fills.size(), 1);
    QCOMPARE(copy.fills.first().heads.size(), 3);
    QVERIFY(!v->rig()->capture(copy));
    QVERIFY(!v->rig()->capture(copy));
    QCOMPARE(copy.fills.first().heads.size(), 3);
}

/* Each change capture() must see, or it is never saved: a house fixture
   moved on the plot only, a head added to a group (and to a second one),
   the group renamed, two heads swapped, a fill head's fixture head. */
void VenueRig_Test::captureSeesEachChange()
{
    VenueCorrections *v = venues(m_doc);
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureGroup *foh = nullptr;
    quint32 first = 0, duo = 0;
    const QString a = captureSetup(m_doc, foh, first, duo);
    QVERIFY(!a.isEmpty());
    FixtureGroup *side = nullptr;
    for (FixtureGroup *g : m_doc->fixtureGroups())
        if (g->name() == QStringLiteral("Side"))
            side = g;
    QVERIFY(side != nullptr);

    // Moved on the plot, nothing else: only its <Monitor> differs.
    VenueRig::Data before = v->venue(a).rig;
    props->setFixturePosition(first, 0, 0, QVector3D(4321, 3400, 0));
    QVERIFY(v->rig()->capture(before));
    QVERIFY(v->recordRig());
    QVERIFY(readFile(v->filePath()).contains("4321"));

    // A head added to the group's fill, after the others: same fill count.
    before = v->venue(a).rig;
    QVERIFY(foh->assignHead(QLCPoint(2, 2), GroupHead(first + 2, 0)));
    QVERIFY(v->rig()->capture(before));
    QCOMPARE(v->venue(a).rig.fills.first().heads.size(), 4);

    // A house head in a second group: a second fill.
    before = v->venue(a).rig;
    QVERIFY(side->assignHead(QLCPoint(1, 1), GroupHead(first, 0)));
    QVERIFY(v->rig()->capture(before));
    QCOMPARE(v->venue(a).rig.fills.size(), 2);

    // The group renamed.
    before = v->venue(a).rig;
    foh->setName(QStringLiteral("FOH front"));
    QVERIFY(v->rig()->capture(before));
    QVERIFY(readFile(v->filePath()).contains("Name=\"FOH front\""));

    // Two heads swapped: the same cells and head indexes, other fixtures.
    before = v->venue(a).rig;
    foh->swap(QLCPoint(1, 2), QLCPoint(2, 1));
    QVERIFY(v->rig()->capture(before));
    QCOMPARE(foh->head(QLCPoint(2, 1)).fxi, first);

    // The other head of the same fixture, in the same cell.
    before = v->venue(a).rig;
    QVERIFY(foh->resignHead(QLCPoint(1, 1)));
    QVERIFY(foh->assignHead(QLCPoint(1, 1), GroupHead(duo, 1)));
    QVERIFY(v->rig()->capture(before));
    QVERIFY(v->recordRig());
    QVERIFY(readFile(v->filePath()).contains(QStringLiteral("Fixture=\"%1\" Head=\"1\" X=\"1\" Y=\"1\"")
                                                 .arg(duo).toUtf8()));
}

/* While a venue is applied, owners are followed on every way in and out:
   a fixture added already House, a house fixture deleted, a House group
   added holding a show fixture, and that group deleted again. */
void VenueRig_Test::ownersFollowAddsAndRemoves()
{
    VenueCorrections *v = venues(m_doc);
    addFixture(m_doc, QStringLiteral("Tour spot"), 0, 0, 1);
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));

    Fixture *par = addFixture(m_doc, QStringLiteral("Lincoln par"), 0, 100, 1, RigOwner::House);
    QVERIFY(par != nullptr);
    const quint32 id = par->id();
    QVERIFY(v->isVenueFixture(id));
    QVERIFY(!engineXml(m_doc).contains("Lincoln par"));
    QVERIFY(m_doc->deleteFixture(id));
    QVERIFY(!v->isVenueFixture(id));

    const quint32 show = addFixture(m_doc, QStringLiteral("Show par"), 0, 200, 1)->id();
    QVERIFY(!v->isVenueFixture(show));
    // Its fixture assigned before the group is the doc's: only the add says.
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName(QStringLiteral("House wash"));
    g->setOwner(RigOwner::House);
    QVERIFY(g->assignFixture(show));
    QVERIFY(m_doc->addFixtureGroup(g));
    QVERIFY(v->isVenueFixture(show));
    QVERIFY(!engineXml(m_doc).contains("Show par"));

    QVERIFY(m_doc->deleteFixtureGroup(g->id()));
    QVERIFY(!v->isVenueFixture(show));
    QVERIFY(engineXml(m_doc).contains("Show par"));
}

/* samePatch() compares every field of every patch, and the size; with
   nothing edited, nothing is pinned: the show saved leaves no venue patch. */
void VenueRig_Test::samePatchComparesEveryField()
{
    Universe::OfflinePatch p;
    p.plugin = QStringLiteral("ArtNet");
    p.uid = QStringLiteral("ArtNet 10.0.0.5");
    p.line = 1;
    p.network = QStringLiteral("show");
    p.params.insert(QStringLiteral("UniverseID"), 3);
    const QList<Universe::OfflinePatch> l{ p };
    QVERIFY(VenueRig::samePatch(l, l));
    QVERIFY(VenueRig::samePatch({}, {}));
    QVERIFY(!VenueRig::samePatch(l, {}));
    QVERIFY(!VenueRig::samePatch({}, l));
    QVERIFY(!VenueRig::samePatch(l, l + l));

    for (int field = 0; field < 5; ++field)
    {
        Universe::OfflinePatch m = p;
        switch (field)
        {
            case 0: m.plugin = QStringLiteral("E1.31"); break;
            case 1: m.uid = QStringLiteral("ArtNet 10.0.0.6"); break;
            case 2: m.line = 2; break;
            case 3: m.network = QStringLiteral("house"); break;
            case 4: m.params.insert(QStringLiteral("UniverseID"), 4); break;
        }
        QVERIFY2(!VenueRig::samePatch(l, { m }), qPrintable(QString("field %1").arg(field)));
        QVERIFY2(!VenueRig::samePatch({ m }, l), qPrintable(QString("field %1").arg(field)));
    }

    VenueCorrections *v = venues(m_doc);
    QVERIFY(m_doc->inputOutputMap()->setOutputPatch(0, stub(m_doc)->name(), QString(), 0));
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    QVERIFY(!v->rig()->patchChanged(v->venue(a).rig));
    v->setShowFile(showFile(), false);
    QVERIFY(!v->hasPatch(a));
}

/* A venue with a house fixture on a universe the show does not have (U5
   of 4) and a patch for U6, past those: the dry run, applying it with a
   House slot in the show, never reads past the show's universes. */
void VenueRig_Test::aVenueBeyondTheShowsUniverses()
{
    VenueCorrections *v = venues(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QCOMPARE(int(iom->universesCount()), 4);
    QVERIFY(iom->setOutputPatch(0, stub(m_doc)->name(), QString(), 0));
    addFixture(m_doc, QStringLiteral("Tour spot"), 0, 0, 1);
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 par = addFixture(m_doc, QStringLiteral("Far par"), 0, 10, 1, RigOwner::House)->id();
    QVERIFY(v->keepPatch());
    QVERIFY(v->recordRig());
    QVERIFY(v->setActiveVenue(QString()));

    // The venue file edited: the par on U5, a patch for U6.
    QByteArray sc = readFile(v->filePath());
    QVERIFY(sc.contains("<Universe>0</Universe>"));
    sc.replace("<Universe>0</Universe>", "<Universe>4</Universe>");
    QVERIFY(sc.contains("<Patch Universe=\"0\">"));
    sc.replace("<Patch Universe=\"0\">", "<Patch Universe=\"5\">");
    QVERIFY(writeFile(v->filePath(), sc));
    v->setShowFile(showFile(), true);
    QVERIFY(!v->isActive());
    QCOMPARE(v->venue(a).rig.fixtures.first().universe, 4u);
    QVERIFY(v->venue(a).rig.patch.contains(5));
    iom->setUniverseSlotOwner(1, RigOwner::House);

    // The dry run: no slot clash for a universe the show lacks.
    for (const VenueRig::Clash &c : v->clashes(a))
        QVERIFY2(c.universe != 4u, qPrintable(c.text));

    QVERIFY(v->setActiveVenue(a));
    QVERIFY(m_doc->fixture(par) != nullptr);
    QCOMPARE(m_doc->fixture(par)->universe(), 4u);
    QVERIFY(m_doc->fixture(par)->isPatched());
    QCOMPARE(int(iom->universesCount()), 5);
    v->clashes(a);
}

/* A house fixture ending just before channels the room keeps goes in
   patched; one unpatched on them is not an address clash. */
void VenueRig_Test::houseFixturesBesideNotOurs()
{
    VenueCorrections *v = venues(m_doc);
    addFixture(m_doc, QStringLiteral("Tour spot"), 0, 20, 1);
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 par = addFixture(m_doc, QStringLiteral("House par"), 0, 3, 1, RigOwner::House)->id();
    const quint32 spare = addFixture(m_doc, QStringLiteral("House spare"), 0, 5, 1, RigOwner::House, false)->id();
    QVERIFY(v->isVenueFixture(par));
    QVERIFY(v->isVenueFixture(spare));
    VenueRig::NotOurs keep;
    keep.universe = 0;
    keep.from = 4;
    keep.to = 6;
    QVERIFY(v->setNotOurs(a, { keep }));
    QVERIFY(v->recordRig());
    QVERIFY(v->setActiveVenue(QString()));
    QVERIFY(v->setActiveVenue(a));

    QVERIFY(m_doc->fixture(par) != nullptr);
    QVERIFY(m_doc->fixture(par)->isPatched());
    QVERIFY(!m_doc->fixture(par)->addressClash());
    QVERIFY(m_doc->fixture(spare) != nullptr);
    QVERIFY(!m_doc->fixture(spare)->isPatched());
    QVERIFY(!m_doc->fixture(spare)->addressClash());
}

/* A fill head keeps its fixture head index and its cell -- captured,
   written, read and put back: the second head of a two-head house fixture
   at (2, 1). */
void VenueRig_Test::fillHeadGeometryRoundTrips()
{
    VenueCorrections *v = venues(m_doc);
    addFixture(m_doc, QStringLiteral("Tour spot"), 0, 0, 1);
    FixtureGroup *foh = makeGroup(m_doc, QStringLiteral("FOH wash"), {});
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    Fixture *duo = addFixture(m_doc, QStringLiteral("Lincoln duo"), 0, 100, 2, RigOwner::House);
    QVERIFY(duo != nullptr);
    QCOMPARE(duo->heads(), 2);
    const quint32 id = duo->id();
    foh->setSize(QSize(3, 2));
    QVERIFY(foh->assignHead(QLCPoint(2, 1), GroupHead(id, 1)));
    QVERIFY(v->recordRig());

    auto check = [id](const VenueRig::Data &d) {
        QCOMPARE(d.fills.size(), 1);
        QCOMPARE(d.fills.first().heads.size(), 1);
        const VenueRig::FillHead h = d.fills.first().heads.first();
        QCOMPARE(h.fixture, id);
        QCOMPARE(h.head, 1);
        QCOMPARE(h.x, 2);
        QCOMPARE(h.y, 1);
    };
    check(v->venue(a).rig);
    if (QTest::currentTestFailed())
        return;

    Doc other(this);
    VenueCorrections back(&other);
    QString why;
    QVERIFY2(back.fromXml(v->toXml(), &why), qPrintable(why));
    check(back.venue(a).rig);
    if (QTest::currentTestFailed())
        return;

    QVERIFY(v->setActiveVenue(QString()));
    QVERIFY(!foh->head(QLCPoint(2, 1)).isValid());
    QVERIFY(v->setActiveVenue(a));
    QCOMPARE(foh->head(QLCPoint(2, 1)).fxi, id);
    QCOMPARE(foh->head(QLCPoint(2, 1)).head, 1);
    QCOMPARE(foh->headList().size(), 1);
}

/* A fill whose group id is gone finds the group again by name; and a fill
   for a group the show deleted while the venue is applied is kept. */
void VenueRig_Test::aFillFindsItsGroupByName()
{
    VenueCorrections *v = venues(m_doc);
    addFixture(m_doc, QStringLiteral("Tour spot"), 0, 0, 1);
    FixtureGroup *foh = makeGroup(m_doc, QStringLiteral("FOH wash"), {});
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 par = addFixture(m_doc, QStringLiteral("Lincoln par"), 0, 100, 1, RigOwner::House)->id();
    QVERIFY(foh->assignFixture(par));
    QCOMPARE(v->venue(a).rig.fills.size(), 1);
    QVERIFY(v->setActiveVenue(QString()));

    // Deleted and made again (another id): the same group to the venue.
    const quint32 oldId = foh->id();
    QVERIFY(m_doc->deleteFixtureGroup(oldId));
    FixtureGroup *again = new FixtureGroup(m_doc);
    again->setName(QStringLiteral("FOH wash"));
    QVERIFY(m_doc->addFixtureGroup(again, oldId + 7));
    QVERIFY(v->setActiveVenue(a));
    QVERIFY(again->fixtureList().contains(par));
    QVERIFY(v->recordRig());
    QCOMPARE(v->venue(a).rig.fills.size(), 1);
    QCOMPARE(v->venue(a).rig.fills.first().group, again->id());

    // Deleted with the venue applied: the fill is kept, it may come back.
    QVERIFY(m_doc->deleteFixtureGroup(again->id()));
    QVERIFY(v->recordRig());
    QCOMPARE(v->venue(a).rig.fills.size(), 1);
    QCOMPARE(v->venue(a).rig.fills.first().groupName, QStringLiteral("FOH wash"));
    QCOMPARE(v->venue(a).rig.fills.first().heads.first().fixture, par);
}

/* The venue's patch in the sidecar keeps the interface's uid and network
   and the plugin's parameters, so it binds to the same interface again. */
void VenueRig_Test::sidecarPatchKeepsUidAndNetwork()
{
    Universe::OfflinePatch p;
    p.plugin = QStringLiteral("ArtNet");
    p.uid = QStringLiteral("ArtNet 10.0.0.5");
    p.line = 1;
    p.network = QStringLiteral("show");
    p.params.insert(QStringLiteral("UniverseID"), QStringLiteral("3"));
    VenueRig::Data d;
    d.patch.insert(2, { p });

    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        w.writeStartElement(QStringLiteral("Venue"));
        VenueRig::writeXml(w, d);
        w.writeEndElement();
    }
    VenueRig::Data back;
    QXmlStreamReader r(xml);
    QVERIFY(r.readNextStartElement());
    while (r.readNextStartElement())
        QVERIFY(VenueRig::readXml(r, back));
    QCOMPARE(back.patch.keys(), QList<quint32>{ 2 });
    const Universe::OfflinePatch q = back.patch.value(2).first();
    QCOMPARE(q.plugin, p.plugin);
    QCOMPARE(q.uid, p.uid);
    QCOMPARE(q.line, 1u);
    QCOMPARE(q.network, p.network);
    QCOMPARE(q.params.value(QStringLiteral("UniverseID")).toString(), QStringLiteral("3"));
    QVERIFY(VenueRig::samePatch(back.patch.value(2), d.patch.value(2)));
}

/* A first output patched on a universe that had none (only its count
   changes) is the venue's too; and forgetting the venue's patch leaves a
   universe added since it was applied as it is. */
void VenueRig_Test::patchEditsOnNewOutputsAndUniverses()
{
    VenueCorrections *v = venues(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    IOPluginStub *plugin = stub(m_doc);
    QVERIFY(iom->setOutputPatch(0, plugin->name(), QString(), 0));
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    QVERIFY(!v->hasPatch(a));

    QVERIFY(iom->outputPatch(2) == nullptr);
    QVERIFY(iom->setOutputPatch(2, plugin->name(), QString(), 1));
    QVERIFY(v->hasPatch(a));
    QCOMPARE(v->venue(a).rig.patch.value(2).size(), 1);
    QCOMPARE(v->venue(a).rig.patch.value(2).first().line, 1u);
    QVERIFY(v->setActiveVenue(QString()));
    QVERIFY(iom->outputPatch(2) == nullptr);
    QVERIFY(v->setActiveVenue(a));
    QCOMPARE(iom->outputPatch(2)->output(), 1u);

    QVERIFY(iom->addUniverse());
    const quint32 added = iom->universesCount() - 1;
    QVERIFY(!iom->universes().at(int(added))->hasShowOutputBase());
    QVERIFY(iom->setOutputPatch(added, plugin->name(), QString(), 2));
    QVERIFY(v->forgetPatch());
    QVERIFY(iom->outputPatch(2) == nullptr);                 // the show's again
    QVERIFY(iom->outputPatch(added) != nullptr);             // not wiped
    QCOMPARE(iom->outputPatch(added)->output(), 2u);
}

/* Fill heads outside the group's grid (shrunk while the venue was away)
   grow it, each axis on its own, so no head is off the grid. */
void VenueRig_Test::fillHeadsOutsideTheGridGrowIt()
{
    VenueCorrections *v = venues(m_doc);
    addFixture(m_doc, QStringLiteral("Tour spot"), 0, 0, 1);
    FixtureGroup *foh = makeGroup(m_doc, QStringLiteral("FOH wash"), {});
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 p = addFixture(m_doc, QStringLiteral("Lincoln par 1"), 0, 100, 1, RigOwner::House)->id();
    const quint32 q = addFixture(m_doc, QStringLiteral("Lincoln par 2"), 0, 101, 1, RigOwner::House)->id();
    foh->setSize(QSize(2, 2));
    QVERIFY(foh->assignHead(QLCPoint(1, 0), GroupHead(p, 0)));
    QVERIFY(foh->assignHead(QLCPoint(0, 1), GroupHead(q, 0)));
    QVERIFY(v->recordRig());
    QVERIFY(v->setActiveVenue(QString()));

    foh->setSize(QSize(1, 1));
    QVERIFY(v->setActiveVenue(a));
    QCOMPARE(foh->head(QLCPoint(1, 0)).fxi, p);
    QCOMPARE(foh->head(QLCPoint(0, 1)).fxi, q);
    QCOMPARE(foh->size(), QSize(2, 2));
}

/* The dry run checks what the venue keeps of each house fixture: its
   universe, its channels and whether it is patched -- captured from the
   rig, and read back from the sidecar. */
void VenueRig_Test::theDryRunReadsTheHouseFixtures()
{
    VenueCorrections *v = venues(m_doc);
    addFixture(m_doc, QStringLiteral("Tour spot"), 0, 0, 1);
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    QVERIFY(addFixture(m_doc, QStringLiteral("House strip"), 1, 10, 3, RigOwner::House) != nullptr);
    QVERIFY(addFixture(m_doc, QStringLiteral("House spare"), 0, 30, 1, RigOwner::House, false) != nullptr);
    QVERIFY(v->recordRig());
    QVERIFY(v->setActiveVenue(QString()));
    // Meanwhile: a tour fixture on the strip's third channel, one where
    // the spare would be if it were patched.
    QVERIFY(addFixture(m_doc, QStringLiteral("Tour U2"), 1, 12, 1) != nullptr);
    QVERIFY(addFixture(m_doc, QStringLiteral("Tour U1"), 0, 30, 1) != nullptr);

    auto check = [](const QList<VenueRig::Clash> &clashes) {
        QCOMPARE(clashes.size(), 1);
        const VenueRig::Clash c = clashes.first();
        QCOMPARE(c.kind, VenueRig::Clash::Overlap);
        QCOMPARE(c.universe, 1u);
        QCOMPARE(c.address, 12u);
        QCOMPARE(c.channels, 1u);
        QVERIFY2(c.text.startsWith(QStringLiteral("U2 13:")), qPrintable(c.text));
        QVERIFY(c.text.contains(QStringLiteral("House strip")));
    };
    auto fields = [](const VenueRig::Data &d) {
        QCOMPARE(d.fixtures.size(), 2);
        for (const VenueRig::HouseFixture &hf : d.fixtures)
        {
            const bool strip = hf.name == QStringLiteral("House strip");
            QCOMPARE(hf.universe, strip ? 1u : 0u);
            QCOMPARE(hf.address, strip ? 10u : 30u);
            QCOMPARE(hf.channels, strip ? 3u : 1u);
            QCOMPARE(hf.patched, strip);
        }
    };
    fields(v->venue(a).rig);
    check(v->clashes(a));
    if (QTest::currentTestFailed())
        return;

    Doc other(this);
    VenueCorrections back(&other);
    QString why;
    QVERIFY2(back.fromXml(v->toXml(), &why), qPrintable(why));
    fields(back.venue(a).rig);
    check(v->rig()->clashes(back.venue(a).rig, false));
}

/* What the clash report says: each clash's universe, first address and
   channel count, and its text, exact -- and nothing for fixtures that only
   touch, or sit on another universe. */
void VenueRig_Test::theClashReportFieldsAndText()
{
    const QString dash = QStringLiteral("–");
    VenueRig::NotOurs whole;
    whole.universe = 3;
    QCOMPARE(VenueRig::notOursText(whole), QStringLiteral("U4 (all of it)"));
    VenueRig::NotOurs top;
    top.universe = 0;
    top.from = 400;
    top.to = 511;
    QCOMPARE(VenueRig::notOursText(top), QStringLiteral("U1 401") + dash + QStringLiteral("512"));

    addFixture(m_doc, QStringLiteral("Tour A"), 0, 50, 3);
    addFixture(m_doc, QStringLiteral("Tour B"), 0, 60, 1);
    addFixture(m_doc, QStringLiteral("Tour D"), 0, 3, 1);      // ends where not ours starts
    addFixture(m_doc, QStringLiteral("Tour E"), 0, 5, 3);      // 5-7 over not ours 4-6
    addFixture(m_doc, QStringLiteral("Tour F"), 0, 19, 2);     // 19-20, ends inside 20-30
    VenueRig::Data d;
    d.fixtures << houseFixture(QStringLiteral("House wide"), 0, 50, 3)     // over Tour A
               << houseFixture(QStringLiteral("House beside"), 0, 59, 1)   // touches Tour B
               << houseFixture(QStringLiteral("House one"), 0, 60, 1)      // on Tour B
               << houseFixture(QStringLiteral("House U2"), 1, 60, 1);      // Tour B's address, U2
    VenueRig::NotOurs keep;
    keep.universe = 0;
    keep.from = 4;
    keep.to = 6;
    VenueRig::NotOurs desk;
    desk.universe = 0;
    desk.from = 20;
    desk.to = 30;
    d.notOurs << keep << desk << whole;

    VenueRig *rig = venues(m_doc)->rig();
    QList<VenueRig::Clash> clashes = rig->clashes(d, false);
    QStringList texts;
    for (const VenueRig::Clash &c : clashes)
        texts << c.text;
    QCOMPARE(clashes.size(), 4);
    int seen = 0;
    for (const VenueRig::Clash &c : clashes)
    {
        QCOMPARE(c.universe, 0u);
        if (c.kind == VenueRig::Clash::Overlap && c.address == 50)
        {
            ++seen;
            QCOMPARE(c.channels, 3u);
            QVERIFY2(c.text.startsWith(QStringLiteral("U1 51") + dash + QStringLiteral("53: ")), qPrintable(c.text));
            QVERIFY(c.text.contains(QStringLiteral("Tour A")) && c.text.contains(QStringLiteral("House wide")));
        }
        else if (c.kind == VenueRig::Clash::Overlap && c.address == 60)
        {
            ++seen;
            QCOMPARE(c.channels, 1u);
            QVERIFY2(c.text.startsWith(QStringLiteral("U1 61: ")), qPrintable(c.text));
            QVERIFY(c.text.contains(QStringLiteral("Tour B")) && c.text.contains(QStringLiteral("House one")));
        }
        else if (c.kind == VenueRig::Clash::NotOursKind && c.address == 20)
        {
            ++seen;
            QCOMPARE(c.channels, 1u);
            QVERIFY2(c.text.startsWith(QStringLiteral("U1 21: Tour F (tour)")), qPrintable(c.text));
        }
        else if (c.kind == VenueRig::Clash::NotOursKind)
        {
            ++seen;
            QCOMPARE(c.address, 5u);
            QCOMPARE(c.channels, 2u);
            QCOMPARE(c.text, QStringLiteral("U1 6") + dash + QStringLiteral("7: Tour E (tour) is on channels this "
                                                                             "venue keeps (not ours: U1 5")
                                 + dash + QStringLiteral("7)"));
        }
    }
    QVERIFY2(seen == 4, qPrintable(texts.join(QLatin1Char('\n'))));

    // A House slot: a house fixture in a Tour slot, where it is.
    m_doc->inputOutputMap()->setUniverseSlotOwner(1, RigOwner::House);
    bool slot = false;
    for (const VenueRig::Clash &c : rig->clashes(d, false))
    {
        if (c.kind != VenueRig::Clash::Slot || !c.text.contains(QStringLiteral("House wide")))
            continue;
        slot = true;
        QCOMPARE(c.universe, 0u);
        QCOMPARE(c.address, 50u);
        QCOMPARE(c.channels, 3u);
        QCOMPARE(c.text, QStringLiteral("U1 51") + dash + QStringLiteral("53: House wide (house) is in a Tour slot"));
    }
    QVERIFY(slot);
}

/* A house fixture's calibration edited (the fixture dialog, the Lighting
   Studio) starts the venue's write timer, and the timer's firing writes
   it into the sidecar -- driven here without waiting for it. */
void VenueRig_Test::aHouseCalibrationEditIsRecordedOnTheTimer()
{
    VenueCorrections *v = venues(m_doc);
    MonitorProperties *props = m_doc->monitorProperties();
    addFixture(m_doc, QStringLiteral("Tour spot"), 0, 0, 1);
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 par = addFixture(m_doc, QStringLiteral("Lincoln par"), 0, 100, 1, RigOwner::House)->id();
    QVERIFY(v->recordRig());
    const QList<QTimer *> timers = v->findChildren<QTimer *>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(timers.size(), 1);
    QTimer *timer = timers.first();
    QVERIFY(!timer->isActive());

    FixtureRigProps rp = props->fixtureRigProps(par);
    rp.panOffsetDeg = 2.25f;
    props->setFixtureRigProps(par, rp);
    QVERIFY(timer->isActive());
    timer->stop();
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout"));
    QVERIFY(readFile(v->filePath()).contains("PanOfs=\"2.25\""));
    QVERIFY(v->venue(a).rig.monitorXml.contains("PanOfs=\"2.25\""));
}

/* A show fixture set to House with a venue applied moves in, and its
   calibration is the room's from then on: the show's base is dropped. */
void VenueRig_Test::aFixtureMovedInLosesItsCalibrationBase()
{
    VenueCorrections *v = venues(m_doc);
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 fx = addFixture(m_doc, QStringLiteral("Tour par"), 0, 0, 1)->id();
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(a));
    QVERIFY(props->hasCalibrationBase(fx));
    m_doc->fixture(fx)->setOwner(RigOwner::House);
    QVERIFY(v->isVenueFixture(fx));
    QVERIFY(!props->hasCalibrationBase(fx));
}

/* A house fixture renumbered because the show took its id takes none of
   another venue's house ids either. */
void VenueRig_Test::aRenumberedIdSkipsOtherVenuesIds()
{
    VenueCorrections *v = venues(m_doc);
    addFixture(m_doc, QStringLiteral("Tour spot"), 0, 0, 1);
    const QString a = v->addVenue(QStringLiteral("Lincoln HS"));
    const QString b = v->addVenue(QStringLiteral("Grand Hall"));
    QVERIFY(v->setActiveVenue(a));
    const quint32 par = addFixture(m_doc, QStringLiteral("Lincoln par"), 0, 100, 1, RigOwner::House)->id();
    QVERIFY(v->setActiveVenue(b));
    const quint32 hall = addFixture(m_doc, QStringLiteral("Hall par"), 0, 200, 1, RigOwner::House)->id();
    // The next id after the show's and the taken one: what a renumber
    // ignoring the other venues would pick.
    QCOMPARE(hall, par + 1);
    QVERIFY(v->setActiveVenue(QString()));

    Fixture *intruder = addFixture(m_doc, QStringLiteral("Tour intruder"), 0, 10, 1);
    QVERIFY(intruder != nullptr);
    QVERIFY(m_doc->deleteFixture(intruder->id()));
    intruder = new Fixture(m_doc);
    intruder->setName(QStringLiteral("Tour intruder"));
    intruder->setChannels(1);
    intruder->setAddress(10);
    QVERIFY(m_doc->addFixture(intruder, par));

    QVERIFY(v->setActiveVenue(a));
    const quint32 moved = v->venue(a).rig.fixtures.first().id;
    QVERIFY(moved != par);
    QVERIFY(moved != hall);
    QVERIFY(!v->venue(b).rig.fixtureIds().contains(moved));
    QCOMPARE(m_doc->fixture(moved)->name(), QStringLiteral("Lincoln par"));
}

/****************************************************************************
 * New Venue from a Show (docs/design/VENUE_FROM_SHOW.md)
 ****************************************************************************/

namespace
{
/* The trimmed PMS-like show written beside the show; read. */
VenueImport::Source pmsSource(QTemporaryDir *dir)
{
    const QString file = dir->filePath(QStringLiteral("PMS Stage.qlcc"));
    writeFile(file, PmsLikeShow::xml());
    VenueImport::Source src;
    QString why;
    if (!VenueImport::read(file, src, &why))
        qWarning() << why;
    return src;
}

/* An Art-Net-like output whose lines are this computer's addresses, as the
   real plugin's are; a test changes them (a cable plugged in). */
class RoutedNetPlugin final : public QLCIOPlugin
{
public:
    void init() override {}
    QString name() const override { return QStringLiteral("ArtNet"); }
    int capabilities() const override { return Output; }
    QString pluginInfo() const override { return QStringLiteral("routed fake"); }
    QStringList outputs() override { return m_lines; }
    bool openOutput(quint32, quint32) override { return true; }
    void closeOutput(quint32, quint32) override {}
    void setParameter(quint32 universe, quint32, Capability, QString name, QVariant value) override
    { m_params[universe][name] = value; }
    void unSetParameter(quint32 universe, quint32, Capability, QString name) override
    { m_params[universe].remove(name); }
    QMap<QString, QVariant> getParameters(quint32 universe, quint32, Capability) const override
    { return m_params.value(universe); }

    QStringList m_lines;
    QMap<quint32, QMap<QString, QVariant>> m_params;
};

/* This computer's addresses, for NetworkRoute: @p ips each a /24. */
void localNetworks(const QStringList &ips)
{
    QList<NetworkRoute::LocalAddress> list;
    for (const QString &ip : ips)
        list << NetworkRoute::LocalAddress{ QStringLiteral("en%1").arg(list.size()), QHostAddress(ip), 24 };
    NetworkRoute::setResolverForTest([list]() { return list; }, [list](const QHostAddress &to) {
        for (const NetworkRoute::LocalAddress &a : list)
            if (to.isInSubnet(a.ip, 24))
                return a.ip;
        return QHostAddress();
    });
}

Universe::OfflinePatch artnetNode(const QString &ip)
{
    Universe::OfflinePatch p;
    p.plugin = QStringLiteral("ArtNet");
    p.line = 0;
    p.params.insert(QStringLiteral("outputIP"), ip);
    return p;
}

const Fixture *fixtureNamed(Doc *doc, const QString &name)
{
    for (const Fixture *fx : doc->fixtures())
        if (fx->name() == name)
            return fx;
    return nullptr;
}

Universe::OfflinePatch usbOutput()
{
    Universe::OfflinePatch p;
    p.plugin = QStringLiteral("DMX USB");
    p.uid = QStringLiteral("DMXking ultraDMX Micro (BG01)");
    p.line = 0;
    return p;
}
}

void VenueRig_Test::aVenueFromAnotherShow()
{
    VenueCorrections *v = venues(m_doc);
    MonitorProperties *props = m_doc->monitorProperties();
    // The touring show: lights on U1 and U2, a group of its own.
    const quint32 tour = dimmers(10, 0, 0, QStringLiteral("Tour par"));
    dimmers(4, 1, 0, QStringLiteral("Step"));
    makeGroup(m_doc, QStringLiteral("Tour wash"), { tour, tour + 1 });
    m_doc->resetModified();
    const QByteArray before = engineXml(m_doc);
    const int showFixtures = m_doc->fixtures().size();

    const VenueImport::Source src = pmsSource(m_dir);
    QCOMPARE(src.fixtures.size(), 7);
    QCOMPARE(src.groups.size(), 3);
    QCOMPARE(src.universes(), QList<quint32>({ 0 }));
    QCOMPARE(VenueRig::outputText(src.fixtureOutputs()), QStringLiteral("Art-Net to 172.18.2.10 (from 172.18.2.219)"));

    // The universe it suggests: the first with nothing on it.
    QCOMPARE(VenueImport::suggestUniverse(m_doc), 2u);
    VenueImport::Options opt;
    opt.universe = 2;
    opt.matches = VenueImport::suggest(src, m_doc);
    opt.output = { usbOutput() };
    const VenueImport::Result res = VenueImport::build(src, m_doc, opt, v->reservedIds());
    QCOMPARE(res.fixtures, 7);
    QCOMPARE(VenueImport::summary(res, 2), QString::fromUtf8("7 fixtures on U3 1\342\200\223109"));
    QString why;
    const QString pms = v->addVenueFromShow(QStringLiteral("PMS Auditeria"), res.data, &why);
    QVERIFY2(!pms.isEmpty(), qPrintable(why));
    QVERIFY(v->addVenueFromShow(QStringLiteral("PMS Auditeria"), res.data, &why).isEmpty());   // taken
    QVERIFY(!m_doc->isModified());
    QVERIFY(v->setActiveVenue(pms));

    // House, on U3, each at the address it had, ids clear of the show's.
    QCOMPARE(v->rig()->venueFixtures().size(), 7);
    QCOMPARE(m_doc->fixtures().size(), showFixtures + 7);
    for (quint32 id : v->rig()->venueFixtures())
    {
        const Fixture *fx = m_doc->fixture(id);
        QVERIFY(fx != nullptr);
        QCOMPARE(fx->owner(), RigOwner::House);
        QCOMPARE(fx->universe(), 2u);
        QVERIFY(fx->isPatched());
        QVERIFY(id >= quint32(showFixtures));
    }
    const Fixture *relay = fixtureNamed(m_doc, QStringLiteral("Ch 3 Relay"));
    const Fixture *foh = fixtureNamed(m_doc, QStringLiteral("FOH 2"));
    QVERIFY(relay != nullptr && foh != nullptr);
    QCOMPARE(relay->address(), 2u);
    QCOMPARE(foh->address(), 103u);
    QCOMPARE(foh->channels(), 3u);
    // A tour light's owner follow does not pull them back into the show.
    m_doc->fixture(tour)->setName(QStringLiteral("Tour par renamed"));
    QCOMPARE(v->rig()->venueFixtures().size(), 7);
    m_doc->fixture(tour)->setName(QStringLiteral("Tour par 1"));
    m_doc->resetModified();

    // On the plot: a relay where it stood, on the Default layer (that show's
    // layer 2 was its own); FOH 2 where it hung, free of any truss, aim kept.
    QCOMPARE(props->fixturePosition(relay->id(), 0, 0), QVector3D(3800.0f, 760.0f, 0.0f));
    QCOMPARE(props->fixtureProperties(relay->id()).m_baseItem.m_layerId, 0u);
    const QVector3D at = props->fixturePosition(foh->id(), 0, 0);
    QVERIFY2(qAbs(at.x() - PmsLikeShow::fohX(1)) < 1.0 && qAbs(at.y() - PmsLikeShow::fohY()) < 1.0
             && qAbs(at.z() - PmsLikeShow::fohZ()) < 0.001,
             qPrintable(QStringLiteral("%1 %2 %3").arg(at.x()).arg(at.y()).arg(at.z())));
    const FixtureRigProps rp = props->fixtureRigProps(foh->id());
    QCOMPARE(rp.trussId, Truss::invalidId());
    QCOMPARE(rp.panZeroDir, 212.1f);
    QCOMPARE(rp.lensDeg, 36.0f);
    QVERIFY(qAbs(props->fixtureRigPosition(foh->id()).x() - at.x() / 1000.0f) < 0.0001f);

    // The venue's own universe goes to the USB interface; the show's are as they were.
    QCOMPARE(VenueRig::outputText(m_doc->inputOutputMap()->universe(2)->outputPatchDescriptions()),
             QStringLiteral("USB DMX: DMXking ultraDMX Micro (BG01)"));

    // The show file has none of it, applied or not.
    QVERIFY(!m_doc->isModified());
    QCOMPARE(engineXml(m_doc), before);
    QVERIFY(v->setActiveVenue(QString()));
    QCOMPARE(m_doc->fixtures().size(), showFixtures);
    QCOMPARE(m_doc->inputOutputMap()->universe(2)->outputPatchDescriptions().size(), 0);

    // The sidecar keeps the room: its universe, the file, what that file sent.
    QVERIFY(v->fromXml(v->toXml()));
    const VenueRig::Room room = v->venue(pms).rig.room;
    QCOMPARE(room.universe, 2u);
    QCOMPARE(room.from, QStringLiteral("PMS Stage.qlcc"));
    QCOMPARE(VenueRig::outputText(room.imported), QStringLiteral("Art-Net to 172.18.2.10 (from 172.18.2.219)"));
    QVERIFY(readFile(v->filePath()).contains("<Room Universe=\"3\" From=\"PMS Stage.qlcc\">"));
    QCOMPARE(v->venue(pms).rig.fixtures.size(), 7);
}

void VenueRig_Test::aVenueFromAnotherShowFillsOnlyConfirmedGroups()
{
    VenueCorrections *v = venues(m_doc);
    const quint32 tour = dimmers(2, 0, 0, QStringLiteral("Tour par"));
    FixtureGroup *foh = makeGroup(m_doc, QStringLiteral("foh"), { tour });
    FixtureGroup *elect = makeGroup(m_doc, QStringLiteral("1st elect"), {});
    makeGroup(m_doc, QStringLiteral("Steps"), { tour + 1 });

    m_doc->resetModified();
    const VenueImport::Source src = pmsSource(m_dir);
    QList<VenueImport::Match> m = VenueImport::suggest(src, m_doc);
    QCOMPARE(m.size(), 3);
    QCOMPARE(m.at(0).group, foh->id());             // "FOH": the same name, case aside
    QCOMPARE(m.at(0).kind, VenueImport::SameName);
    QCOMPARE(m.at(1).group, UINT_MAX);              // "Relays": none
    QCOMPARE(m.at(2).group, elect->id());           // "1st Elect.": similar
    QCOMPARE(m.at(2).kind, VenueImport::SimilarName);
    QCOMPARE(VenueImport::simplified(QStringLiteral(" House & Work ")), QStringLiteral("housework"));
    QCOMPARE(VenueImport::simplified(QStringLiteral("House and work")), QStringLiteral("housework"));
    // Each suggestion waits for his tick.
    QCOMPARE(VenueImport::unconfirmed(m), 2);
    m[0].confirmed = true;
    QCOMPARE(VenueImport::unconfirmed(m), 1);

    VenueImport::Options opt;
    opt.universe = 2;
    opt.matches = m;                                // "1st Elect." left unconfirmed
    const VenueImport::Result res = VenueImport::build(src, m_doc, opt, v->reservedIds());
    QCOMPARE(res.filled, 1);
    QCOMPARE(res.held, 2);
    const QString pms = v->addVenueFromShow(QStringLiteral("PMS Auditeria"), res.data);
    QVERIFY(v->setActiveVenue(pms));

    // FOH holds its tour head and the three house FOH lights; 1st elect none.
    QCOMPARE(foh->headList().size(), 4);
    QStringList names;
    for (const GroupHead &h : foh->headList())
        names << m_doc->fixture(h.fxi)->name();
    names.sort();
    QCOMPARE(names, QStringList({ "FOH 1", "FOH 2", "FOH 3", "Tour par 1" }));
    QCOMPARE(elect->headList().size(), 0);
    QStringList held;
    for (const VenueRig::Fill &g : v->venue(pms).rig.room.groups)
        held << QStringLiteral("%1 (%2)").arg(g.groupName).arg(g.heads.size());
    QCOMPARE(held, QStringList({ "Relays (4)", "1st Elect. (2)" }));
    QVERIFY(readFile(v->filePath()).contains("<Group Name=\"Relays\">"));

    // Matched later, from the venue page: on the rig at once, and kept.
    QVERIFY(!m_doc->isModified());
    QCOMPARE(v->matchRoomGroups(pms, { { 1, elect->id() } }), 1);
    QCOMPARE(elect->headList().size(), 2);
    QVERIFY(!m_doc->isModified());          // a venue's fill is not an edit to the show
    QCOMPARE(v->venue(pms).rig.room.groups.size(), 1);
    QCOMPARE(v->venue(pms).rig.room.groups.first().groupName, QStringLiteral("Relays"));
    QVERIFY(v->setActiveVenue(QString()));
    QCOMPARE(elect->headList().size(), 0);
    QCOMPARE(foh->headList().size(), 1);
    QVERIFY(v->setActiveVenue(pms));
    QCOMPARE(elect->headList().size(), 2);
    QCOMPARE(foh->headList().size(), 4);
    // The show's groups never took a house head into the show file.
    QVERIFY(!engineXml(m_doc).contains("Relay"));
}

void VenueRig_Test::theRoomOutputAndItsClashes()
{
    VenueCorrections *v = venues(m_doc);
    dimmers(4, 0, 0, QStringLiteral("Tour par"));
    m_doc->resetModified();
    const VenueImport::Source src = pmsSource(m_dir);
    VenueImport::Options opt;
    opt.universe = 2;
    opt.matches = VenueImport::suggest(src, m_doc);
    const VenueImport::Result res = VenueImport::build(src, m_doc, opt, v->reservedIds());   // no output yet
    const QString pms = v->addVenueFromShow(QStringLiteral("PMS Auditeria"), res.data);
    auto outputClashes = [&]() {
        QStringList out;
        for (const VenueRig::Clash &c : v->clashes(pms))
            if (c.kind == VenueRig::Clash::Output)
                out << c.text;
        return out;
    };
    QCOMPARE(VenueRig::kindName(VenueRig::Clash::Output), QStringLiteral("Output"));

    // None yet: said, before it is applied (the dry run) and after.
    QStringList out = outputClashes();
    QCOMPARE(out.size(), 1);
    QVERIFY2(out.first().startsWith(QStringLiteral("U3: no output in this room yet")), qPrintable(out.first()));
    QVERIFY(v->setActiveVenue(pms));
    QCOMPARE(outputClashes(), out);

    // The Art-Net the file had, left over: flagged, by the file and the address.
    QVERIFY(v->setRoomOutput(pms, src.fixtureOutputs()));
    out = outputClashes();
    QCOMPARE(out.size(), 1);
    QVERIFY2(out.first().contains(QStringLiteral("U3 still sends what PMS Stage.qlcc had: Art-Net to 172.18.2.10")),
             qPrintable(out.first()));
    QCOMPARE(VenueRig::outputText(m_doc->inputOutputMap()->universe(2)->outputPatchDescriptions()),
             QStringLiteral("Art-Net to 172.18.2.10 (from 172.18.2.219)"));

    // The room's USB interface: on the universe at once, and no clash.
    QVERIFY(v->setRoomOutput(pms, { usbOutput() }));
    QVERIFY(outputClashes().isEmpty());
    QCOMPARE(VenueRig::outputText(m_doc->inputOutputMap()->universe(2)->outputPatchDescriptions()),
             QStringLiteral("USB DMX: DMXking ultraDMX Micro (BG01)"));
    QVERIFY(!m_doc->isModified());
    QVERIFY(!engineXml(m_doc).contains("DMXking"));     // the show keeps its own (none)
    QVERIFY(v->fromXml(v->toXml()));
    QCOMPARE(VenueRig::outputText(v->venue(pms).rig.patch.value(2)), QStringLiteral("USB DMX: DMXking ultraDMX Micro (BG01)"));

    // A setRoomOutput() for a venue with no universe of its own is refused.
    const QString plain = v->addVenue(QStringLiteral("Lincoln HS"));
    QString why;
    QVERIFY(!v->setRoomOutput(plain, { usbOutput() }, &why));
    QVERIFY(why.contains(QStringLiteral("no universe of its own")));

    // A tour fixture put on the venue's universe: said.
    Fixture *stray = addFixture(m_doc, QStringLiteral("Stray spot"), 2, 400, 2);
    QVERIFY(stray != nullptr);
    bool slot = false;
    for (const VenueRig::Clash &c : v->clashes(pms))
        slot = slot || (c.kind == VenueRig::Clash::Slot
                        && c.text == QString::fromUtf8("U3 401\342\200\223402: Stray spot (tour) is on the venue's own universe"));
    QVERIFY(slot);

    // No venue patch for it, and the show sends that universe somewhere: said.
    VenueRig::Data d = v->venue(pms).rig;
    d.patch.remove(2);
    QVERIFY(v->setActiveVenue(QString()));
    Universe::OfflinePatch showOut;
    showOut.plugin = QStringLiteral("ArtNet");
    showOut.uid = QStringLiteral("10.0.0.5");
    showOut.line = 0;
    showOut.params.insert(QStringLiteral("outputIP"), QStringLiteral("10.0.0.50"));
    m_doc->inputOutputMap()->replaceOutputPatches(2, { showOut });
    bool shows = false;
    for (const VenueRig::Clash &c : v->rig()->clashes(d, false))
        shows = shows || (c.kind == VenueRig::Clash::Output
                          && c.text.startsWith(QStringLiteral("U3 goes out the show's own output in this room "
                                                              "(Art-Net to 10.0.0.50")));
    QVERIFY(shows);

    // A venue without a room has none of these (every venue before).
    for (const VenueRig::Clash &c : v->clashes(plain))
        QVERIFY(c.kind != VenueRig::Clash::Output);
}

void VenueRig_Test::anArtNetRoomOutputWaitsForItsNetwork()
{
    VenueCorrections *v = venues(m_doc);
    RoutedNetPlugin *artnet = new RoutedNetPlugin;
    artnet->m_lines = { QStringLiteral("192.168.1.5") };
    m_doc->ioPluginCache()->m_plugins.append(artnet);
    localNetworks({ QStringLiteral("192.168.1.5") });
    InputOutputMap *iom = m_doc->inputOutputMap();
    // The tour's own Art-Net node on U1, open on the office network.
    dimmers(4, 0, 0, QStringLiteral("Tour par"));
    Universe::OfflinePatch tourOut = artnetNode(QStringLiteral("192.168.1.50"));
    tourOut.uid = QStringLiteral("192.168.1.5");
    iom->replaceOutputPatches(0, { tourOut });
    QVERIFY(iom->universe(0)->outputPatchesCount() == 1 && !iom->universe(0)->outputPatch(0)->isPending());
    m_doc->resetModified();

    // The room's node, on a network this computer isn't on yet.
    const VenueImport::Source src = pmsSource(m_dir);
    VenueImport::Options opt;
    opt.universe = 2;
    opt.output = { artnetNode(QStringLiteral("10.0.0.20")) };
    const QString pms = v->addVenueFromShow(QStringLiteral("PMS Auditeria"),
                                            VenueImport::build(src, m_doc, opt, v->reservedIds()).data);
    QVERIFY(v->setActiveVenue(pms));

    // Kept, waiting, and said so: not dropped, not claimed as sending.
    Universe *room = iom->universe(2);
    QCOMPARE(VenueRig::outputText(room->outputPatchDescriptions()), QStringLiteral("Art-Net to 10.0.0.20"));
    QVERIFY(!v->rig()->roomOutputLive(v->venue(pms).rig));
    QStringList out;
    for (const VenueRig::Clash &c : v->clashes(pms))
        if (c.kind == VenueRig::Clash::Output)
            out << c.text;
    QCOMPARE(out, QStringList({ QStringLiteral("U3: Art-Net to 10.0.0.20 is chosen, but nothing on this computer "
                                               "reaches it yet. Waiting for it (check the cable and the network)") }));
    QCOMPARE(iom->retryOfflinePatches(), 0);           // still nothing reaches it
    QCOMPARE(room->offlinePatches().size(), 1);

    // The room's network comes up: it goes out there, by itself.
    artnet->m_lines = { QStringLiteral("192.168.1.5"), QStringLiteral("10.0.0.5") };
    localNetworks({ QStringLiteral("192.168.1.5"), QStringLiteral("10.0.0.5") });
    QVERIFY(iom->retryOfflinePatches() >= 1);
    QVERIFY(v->rig()->roomOutputLive(v->venue(pms).rig));
    QCOMPARE(room->outputPatch(0)->outputName(), QStringLiteral("10.0.0.5"));
    for (const VenueRig::Clash &c : v->clashes(pms))
        QVERIFY2(c.kind != VenueRig::Clash::Output, qPrintable(c.text));

    // Resolved, it is still the venue's choice: an unrelated patch signal
    // keeps nothing new, and never the tour's universes.
    QVERIFY(!v->rig()->patchChanged(v->venue(pms).rig));
    emit iom->universe(0)->outputPatchChanged();
    QCOMPARE(v->venue(pms).rig.patch.keys(), QList<quint32>({ 2 }));
    QVERIFY(v->keepPatch());
    QCOMPARE(v->venue(pms).rig.patch.keys(), QList<quint32>({ 2 }));
    QVERIFY(VenueRig::sameDestination(v->venue(pms).rig.patch.value(2), { artnetNode(QStringLiteral("10.0.0.20")) }));
    QVERIFY(!m_doc->isModified());
    NetworkRoute::setResolverForTest(nullptr, nullptr);
}

void VenueRig_Test::undoingAVenueFromAnotherShow()
{
    VenueCorrections *v = venues(m_doc);
    dimmers(4, 0, 0, QStringLiteral("Tour par"));
    const QString kept = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(kept));       // the room he was in before
    m_doc->resetModified();
    const QByteArray before = engineXml(m_doc);
    auto section = [&kept](const QByteArray &xml) {
        // An empty venue is written as <Venue .../>.
        const QRegularExpression re(QStringLiteral("<Venue Name=\"%1\"[^>]*?(/>|>.*?</Venue>)")
                                        .arg(QRegularExpression::escape(kept)),
                                    QRegularExpression::DotMatchesEverythingOption);
        return re.match(QString::fromUtf8(xml)).captured(0).toUtf8();
    };
    const QByteArray sidecar = section(readFile(v->filePath()));
    QVERIFY(!sidecar.isEmpty());

    const VenueImport::Source src = pmsSource(m_dir);
    VenueImport::Options opt;
    opt.universe = 1;
    opt.matches = VenueImport::suggest(src, m_doc);
    const QString pms = v->addVenueFromShow(QStringLiteral("PMS Auditeria"),
                                            VenueImport::build(src, m_doc, opt, v->reservedIds()).data);
    QVERIFY(v->setActiveVenue(pms));
    QCOMPARE(m_doc->fixtures().size(), 11);
    QVERIFY(!v->reservedIds().isEmpty());

    // Undo: the venue off the rig and gone, the venue before it back on;
    // the show and that venue as they were.
    QVERIFY(v->undoVenueFromShow(pms, kept));
    QCOMPARE(v->activeVenue(), kept);
    QCOMPARE(m_doc->fixtures().size(), 4);
    QVERIFY(v->reservedIds().isEmpty());
    QVERIFY(!m_doc->isModified());
    QCOMPARE(engineXml(m_doc), before);
    const QByteArray after = readFile(v->filePath());
    QVERIFY(!after.contains("PMS Auditeria"));
    QCOMPARE(section(after), sidecar);
}

void VenueRig_Test::aVenueFromAFileThatIsNotAShow()
{
    VenueImport::Source src;
    QString why;
    QVERIFY(!VenueImport::read(m_dir->filePath(QStringLiteral("missing.qlcc")), src, &why));
    QVERIFY(why.contains(QStringLiteral("missing.qlcc")));
    QVERIFY(!VenueImport::readXml("<QlcVenues/>", src, &why));
    QVERIFY(why.contains(QStringLiteral("not a show file")));
    QVERIFY(!VenueImport::readXml("<Workspace><Engine><InputOutputMap/></Engine></Workspace>", src, &why));
    QVERIFY(why.contains(QStringLiteral("no fixtures")));
    QVERIFY(!VenueImport::readXml("<Workspace><Engine><Fixture><ID>1</ID>", src, &why));
    QVERIFY(why.contains(QStringLiteral("can't be read")));
    // A show with fixtures in two universes: all on the venue's, with a warning.
    QByteArray two = PmsLikeShow::xml();
    two.replace("<ID>53</ID><Name>FOH 3</Name><Universe>0</Universe>", "<ID>53</ID><Name>FOH 3</Name><Universe>1</Universe>");
    QVERIFY(VenueImport::readXml(two, src, &why));
    QCOMPARE(src.universes(), QList<quint32>({ 0, 1 }));
    VenueImport::Options opt;
    opt.universe = 3;
    const VenueImport::Result res = VenueImport::build(src, m_doc, opt, QSet<quint32>());
    QVERIFY2(res.warnings.filter(QStringLiteral("The other show has fixtures in 2 universes (U1, U2)")).size() == 1,
             qPrintable(res.warnings.join(QLatin1Char('\n'))));
    QCOMPARE(res.warnings.filter(QStringLiteral("3 fixtures hung on the other show's structure")).size(), 1);
    for (const VenueRig::HouseFixture &hf : res.data.fixtures)
        QCOMPARE(hf.universe, 3u);
}

void VenueRig_Test::aRealShowIntoAVenue()
{
    const QString showPath = qEnvironmentVariable("QLC_VENUE_REAL_SHOW");
    const QString roomPath = qEnvironmentVariable("QLC_VENUE_REAL_ROOM");
    if (showPath.isEmpty() || roomPath.isEmpty())
        QSKIP("QLC_VENUE_REAL_SHOW and QLC_VENUE_REAL_ROOM name a show and a room's show to try");
    // Copies only: the real files are never opened for writing.
    const QString show = m_dir->filePath(QStringLiteral("show-copy.qlcc"));
    const QString room = m_dir->filePath(QFileInfo(roomPath).fileName());
    QVERIFY(QFile::copy(showPath, show));
    QVERIFY(QFile::copy(roomPath, room));

    QDir defs(INTERNAL_FIXTUREDIR);
    QVERIFY(m_doc->fixtureDefCache()->loadMap(defs));
    if (m_doc->showEmbedding()->readWorkspace(show))
        m_doc->showEmbedding()->apply(m_dir->path());
    {
        QFile f(show);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QXmlStreamReader r(&f);
        QVERIFY(r.readNextStartElement());
        bool loaded = false;
        while (r.readNextStartElement())
        {
            if (r.name() == QLatin1String("Engine"))
                loaded = m_doc->loadXML(r);
            else
                r.skipCurrentElement();
        }
        QVERIFY(loaded);
    }
    venues(m_doc)->setShowFile(show, true);
    VenueCorrections *v = venues(m_doc);
    m_doc->resetModified();
    const QByteArray before = engineXml(m_doc);
    const int showFixtures = m_doc->fixtures().size();

    VenueImport::Source src;
    QString why;
    QVERIFY2(VenueImport::read(room, src, &why), qPrintable(why));
    VenueImport::Options opt;
    opt.universe = VenueImport::suggestUniverse(m_doc);
    QVERIFY(opt.universe != UINT_MAX);
    opt.matches = VenueImport::suggest(src, m_doc);
    int same = 0, similar = 0;
    QStringList matched;
    for (int i = 0; i < opt.matches.size(); ++i)
    {
        VenueImport::Match &m = opt.matches[i];
        if (m.kind == VenueImport::SameName)
            ++same;
        else if (m.kind == VenueImport::SimilarName)
            ++similar;
        if (m.group != UINT_MAX)
        {
            m.confirmed = true;     // as if he ticked every suggestion
            matched << QStringLiteral("%1 -> %2").arg(src.groups.at(i).name, m_doc->fixtureGroup(m.group)->name());
        }
    }
    const VenueImport::Result res = VenueImport::build(src, m_doc, opt, v->reservedIds());
    const QList<VenueRig::Clash> dry = v->rig()->clashes(res.data, false);
    const QString made = v->addVenueFromShow(QStringLiteral("Room from a show"), res.data, &why);
    QVERIFY2(!made.isEmpty(), qPrintable(why));
    QVERIFY(v->setActiveVenue(made));
    int patched = 0, unpatched = 0;
    for (quint32 id : v->rig()->venueFixtures())
        (m_doc->fixture(id)->isPatched() ? patched : unpatched)++;
    const QList<VenueRig::Clash> live = v->clashes(made);
    QMap<QString, int> kinds;
    for (const VenueRig::Clash &c : live)
        kinds[VenueRig::kindName(c.kind)]++;
    QStringList heldNames;
    for (const VenueRig::Fill &g : v->venue(made).rig.room.groups)
        heldNames << QStringLiteral("%1 (%2)").arg(g.groupName).arg(g.heads.size());

    QStringList lines;
    lines << QStringLiteral("show fixtures: %1, groups: %2, universes: %3").arg(showFixtures)
                 .arg(m_doc->fixtureGroups().size()).arg(m_doc->inputOutputMap()->universesCount());
    lines << QStringLiteral("room file: %1 fixtures, %2 groups, universes used: %3, its outputs: %4")
                 .arg(src.fixtures.size()).arg(src.groups.size()).arg(src.universes().size())
                 .arg(VenueRig::outputText(src.fixtureOutputs()));
    lines << QStringLiteral("suggested universe: U%1 (%2)").arg(opt.universe + 1)
                 .arg(m_doc->inputOutputMap()->getUniverseNameByIndex(int(opt.universe)));
    lines << QStringLiteral("summary: %1").arg(VenueImport::summary(res, opt.universe));
    lines << QStringLiteral("groups: %1 same name, %2 similar, %3 filled, %4 held").arg(same).arg(similar)
                 .arg(res.filled).arg(res.held);
    lines << QStringLiteral("matched: %1").arg(matched.isEmpty() ? QStringLiteral("none") : matched.join(QStringLiteral("; ")));
    lines << QStringLiteral("held: %1").arg(heldNames.join(QStringLiteral(", ")));
    lines << QStringLiteral("applied: %1 house fixtures on the rig (%2 patched, %3 unpatched); %4 kept unloaded")
                 .arg(v->rig()->venueFixtures().size()).arg(patched).arg(unpatched)
                 .arg(res.fixtures - int(v->rig()->venueFixtures().size()));
    lines << QStringLiteral("clashes before apply: %1; applied: %2").arg(dry.size()).arg(live.size());
    for (auto it = kinds.constBegin(); it != kinds.constEnd(); ++it)
        lines << QStringLiteral("  %1: %2").arg(it.key()).arg(it.value());
    for (int i = 0; i < live.size() && i < 12; ++i)
        lines << QStringLiteral("  - %1").arg(live.at(i).text);
    lines << QStringLiteral("warnings: %1").arg(res.warnings.size());
    for (const QString &w : res.warnings)
        lines << QStringLiteral("  - %1").arg(w);
    lines << QStringLiteral("show modified: %1; show file unchanged: %2")
                 .arg(m_doc->isModified() ? "yes" : "no", engineXml(m_doc) == before ? "yes" : "NO");
    const QString report = lines.join(QLatin1Char('\n'));
    qInfo().noquote() << report;
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!dir.isEmpty())
    {
        QDir().mkpath(dir);
        QVERIFY(writeFile(dir + QStringLiteral("/venue-import-report.txt"), report.toUtf8() + '\n'));
    }
    QVERIFY(!m_doc->isModified());
    QCOMPARE(engineXml(m_doc), before);
    // The real files are as they were.
    QCOMPARE(readFile(show), readFile(showPath));
    QCOMPARE(readFile(room), readFile(roomPath));
}

QTEST_GUILESS_MAIN(VenueRig_Test)

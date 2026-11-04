/*
  Q Light Controller Plus - Unit test
  showreview_test.cpp

  Show Review (docs/design/SHOW_REVIEW.md): every check against a tiny show
  that trips it and the near miss that must not; acceptance saved with the
  show; the CLI.

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
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QRegularExpression>

#include "showreview_test.h"
#include "showreviewcli.h"
#include "teachingsettings.h"
#include "venuecorrections.h"
#include "showembedding.h"
#include "rgbmatrix.h"
#include "chaser.h"
#include "efx.h"
#include "showtreecontroller.h"
#include "programmercontroller.h"
#include "powerdistribution.h"
#include "monitorproperties.h"
#include "venuecorrections.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "fixturegroup.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "focuscalls.h"
#include "collection.h"
#include "venuerig.h"
#include "fixture.h"
#include "qlcfile.h"
#include "scene.h"
#include "doc.h"
#include "movespeed.h"
#include "stagetarget.h"
#include "nolightzone.h"
#include "aimsolver.h"
#include "righeads.h"
#include "scenery.h"
#include "placementcheck.h"
#include "stageplatform.h"
#include "truss.h"

using namespace ShowTree;

QSet<QString> ShowReview_Test::s_fired;
QSet<QString> ShowReview_Test::s_quiet;

namespace
{
QLCChannel *chan(QLCFixtureDef *def, const QString &name, QLCChannel::Preset preset)
{
    QLCChannel *ch = new QLCChannel();
    ch->setName(name);
    ch->setPreset(preset);
    def->addChannel(ch);
    return ch;
}

Fixture *make(Doc *doc, const QString &name, const QString &model, QLCFixtureDef::FixtureType type,
              QLCFixtureDef *def, const QList<QLCChannel *> &chans, int watts,
              quint32 universe, quint32 address,
              QLCPhysical::Emission emission = QLCPhysical::BeamEmission, double lensDeg = 0.0)
{
    def->setManufacturer(QStringLiteral("Test"));
    def->setModel(model);
    def->setType(type);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("Mode"));
    for (int c = 0; c < chans.size(); c++)
        mode->insertChannel(chans.at(c), c);
    QLCFixtureHead head;
    for (int c = 0; c < chans.size(); c++)
        head.addChannel(quint32(c));
    mode->insertHead(-1, head);
    QLCPhysical phy;
    phy.setPowerConsumption(watts);
    phy.setEmission(emission);
    if (lensDeg > 0.0)
    {
        phy.setLensDegreesMin(lensDeg);
        phy.setLensDegreesMax(lensDeg);
    }
    if (type == QLCFixtureDef::MovingHead)
    {
        phy.setFocusPanMax(540);
        phy.setFocusTiltMax(270);
    }
    mode->setPhysical(phy);
    mode->cacheHeads();
    def->addMode(mode);
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(universe);
    fx->setAddress(address);
    if (!doc->addFixture(fx))
        return nullptr;
    return fx;
}

bool loadFixtureXml(Doc *doc, const QString &xml)
{
    QXmlStreamReader r(xml);
    while (!r.atEnd() && !(r.isStartElement() && r.name() == QLatin1String("Fixture")))
        r.readNext();
    return Fixture::loader(r, doc);
}

bool loadFunctionXml(Doc *doc, const QString &xml)
{
    QXmlStreamReader r(xml);
    while (!r.atEnd() && !(r.isStartElement() && r.name() == QLatin1String("Function")))
        r.readNext();
    return Function::loader(r, doc);
}
}

void ShowReview_Test::init()
{
    m_doc = new Doc(this);
}

void ShowReview_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

QList<ReviewFinding> ShowReview_Test::findings(const QString &check, const ShowReview::Options &options)
{
    ShowReview::Options o = options;
    o.checks = QStringList() << check;
    return m_doc->showReview()->run(o).findings;
}

bool ShowReview_Test::fires(const QString &check, const ShowReview::Options &options)
{
    const QList<ReviewFinding> f = findings(check, options);
    if (f.isEmpty())
        return false;
    for (const ReviewFinding &x : f)
    {
        // Every finding: its check, a message and a target to key it by.
        if (x.check != check || x.message.isEmpty() || x.targets.isEmpty() || x.fingerprint.isEmpty())
            return false;
    }
    s_fired.insert(check);
    return true;
}

bool ShowReview_Test::quiet(const QString &check, const ShowReview::Options &options)
{
    if (!findings(check, options).isEmpty())
        return false;
    s_quiet.insert(check);
    return true;
}

Fixture *ShowReview_Test::par(const QString &name, quint32 address, quint32 universe)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    return make(m_doc, name, QStringLiteral("Par"), QLCFixtureDef::ColorChanger, def,
                { chan(def, "R", QLCChannel::IntensityRed), chan(def, "G", QLCChannel::IntensityGreen),
                  chan(def, "B", QLCChannel::IntensityBlue),
                  chan(def, "Dimmer", QLCChannel::IntensityMasterDimmer) }, 100, universe, address);
}

Fixture *ShowReview_Test::dimmer(const QString &name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    return make(m_doc, name, QStringLiteral("Lamp"), QLCFixtureDef::Dimmer, def,
                { chan(def, "Dimmer", QLCChannel::IntensityDimmer) }, 1000, 0, address);
}

Fixture *ShowReview_Test::mover(const QString &name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    QLCChannel *shutter = new QLCChannel();
    shutter->setName(QStringLiteral("Shutter"));
    shutter->setGroup(QLCChannel::Shutter);
    QLCCapability *open = new QLCCapability(0, 9, QStringLiteral("Open"));
    open->setPreset(QLCCapability::ShutterOpen);
    shutter->addCapability(open);
    QLCCapability *strobe = new QLCCapability(10, 255, QStringLiteral("Strobe 1-20 Hz"));
    strobe->setPreset(QLCCapability::StrobeFreqRange);
    strobe->setResource(0, 1.0);
    strobe->setResource(1, 20.0);
    shutter->addCapability(strobe);
    def->addChannel(shutter);
    return make(m_doc, name, QStringLiteral("Spot"), QLCFixtureDef::MovingHead, def,
                { chan(def, "Pan", QLCChannel::PositionPan), chan(def, "Tilt", QLCChannel::PositionTilt),
                  chan(def, "Dimmer", QLCChannel::IntensityMasterDimmer), shutter }, 200, 0, address);
}

FixtureGroup *ShowReview_Test::group(const QString &name, const QList<Fixture *> &members)
{
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName(name);
    m_doc->addFixtureGroup(g);
    for (Fixture *fx : members)
        g->assignFixture(fx->id());
    return g;
}

Fixture *ShowReview_Test::laser(const QString &name, quint32 address, QLCPhysical::Emission emission,
                                double beamDeg, const QVector3D &at, double aimDown)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    Fixture *fx = make(m_doc, name, QStringLiteral("Scan ") + name, QLCFixtureDef::Laser, def,
                       { chan(def, "Dimmer", QLCChannel::IntensityMasterDimmer),
                         chan(def, "Pattern", QLCChannel::Custom) }, 20, 0, address, emission, beamDeg);
    if (fx == nullptr)
        return nullptr;
    MonitorProperties *mp = m_doc->monitorProperties();
    FixturePreviewItem item;
    item.m_baseItem.m_position = at;
    mp->setFixtureProperties(fx->id(), item);
    FixtureRigProps rp = mp->fixtureRigProps(fx->id());
    rp.fixedAim = true;
    rp.panZeroDir = 0;                      // toward +Y: downstage, the audience
    rp.aimDown = float(aimDown);
    mp->setFixtureRigProps(fx->id(), rp);
    return fx;
}

QLCPalette *ShowReview_Test::colour(const QColor &c)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Color, m_doc);
    p->setValue(c);
    p->setName(c.name());
    m_doc->addPalette(p);
    return p;
}

void ShowReview_Test::editTree(const std::function<void(QString &)> &edit)
{
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    {
        QXmlStreamWriter w(&buf);
        w.writeStartDocument();
        m_doc->showTree()->saveXML(&w);
        w.writeEndDocument();
    }
    QString xml = QString::fromUtf8(buf.data());
    edit(xml);
    QXmlStreamReader r(xml);
    while (!r.atEnd() && !(r.isStartElement() && r.name() == KXMLQLCShowTree))
        r.readNext();
    QVERIFY(m_doc->showTree()->loadXML(r));
}

QString ShowReview_Test::saveShow() const
{
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    w.writeStartElement(QStringLiteral("Workspace"));
    m_doc->saveXML(&w);
    w.writeEndElement();
    w.writeEndDocument();
    return QString::fromUtf8(buf.data());
}

void ShowReview_Test::console(const QString &widgets, const QString &properties)
{
    const QString xml = QStringLiteral("<Workspace><VirtualConsole><Frame Caption=\"\">%1</Frame>%2"
                                       "</VirtualConsole></Workspace>").arg(widgets, properties);
    m_doc->showReview()->setVcXml(xml.toUtf8());
}

bool ShowReview_Test::loadShow(Doc *doc, const QString &xml) const
{
    QXmlStreamReader r(xml);
    while (!r.atEnd())
        if (r.readNextStartElement() && r.name() == KXMLQLCEngine)
            return doc->loadXML(r, false);
    return false;
}

/****************************************************************************
 * Patch
 ****************************************************************************/

void ShowReview_Test::pat01()
{
    Fixture *a = par("Par 1", 0);
    Fixture *b = par("Par 2", 10);
    QVERIFY(a != nullptr && b != nullptr);
    QVERIFY(quiet("PAT-01"));
    b->setAddress(2);                       // as a hand-edited file can have it
    QVERIFY(fires("PAT-01"));
    const ReviewFinding f = findings("PAT-01").first();
    QCOMPARE(f.severity, ReviewFinding::Blocker);
    QCOMPARE(f.targets, QStringList() << QStringLiteral("fx:%1").arg(a->id()) << QStringLiteral("fx:%1").arg(b->id()));
    QVERIFY(f.message.contains("Par 1") && f.message.contains("3–4"));
    QCOMPARE(f.jump, QStringLiteral("status:fixtures.clash"));
}

void ShowReview_Test::pat02()
{
    Fixture *a = par("Par 1", 0);
    QVERIFY(quiet("PAT-02"));
    a->setAddress(510);
    QVERIFY(fires("PAT-02"));
    QCOMPARE(findings("PAT-02").first().severity, ReviewFinding::Blocker);
}

void ShowReview_Test::pat03()
{
    Fixture *a = par("Par 1", 0);
    Fixture *b = par("Par 2", 10);
    Scene *s = new Scene(m_doc);
    s->setName("Look");
    s->setValue(a->id(), 3, 255);
    s->setValue(b->id(), 3, 255);
    m_doc->addFunction(s);
    QVERIFY(quiet("PAT-03"));
    QVERIFY(m_doc->unpatchFixture(b->id()));
    QVERIFY(fires("PAT-03"));
    ReviewFinding f = findings("PAT-03").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);   // used, so a warning
    QCOMPARE(f.targets, QStringList() << QStringLiteral("fx:%1").arg(b->id()));
    QVERIFY(m_doc->deleteFunction(s->id()));
    f = findings("PAT-03").first();
    QCOMPARE(f.severity, ReviewFinding::Note);      // unused: inventory is fine
}

void ShowReview_Test::pat04()
{
    Fixture *a = par("Par 1", 400);
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    QVERIFY(m_dir.isValid());
    v->setShowFile(QDir(m_dir.path()).filePath("Tour.qlcc"), false);   // the venue file goes beside it
    const QString venue = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(venue));
    VenueRig::NotOurs keep;
    keep.universe = 0;
    keep.from = 480;
    keep.to = 511;
    QVERIFY(v->setNotOurs(venue, { keep }));
    QVERIFY(quiet("PAT-04"));
    keep.from = 390;                        // the venue changed after the patch
    QVERIFY(v->setNotOurs(venue, { keep }));
    QVERIFY(m_doc->isOnNotOurs(0, a->address(), a->channels()));
    QVERIFY(fires("PAT-04"));
    QCOMPARE(findings("PAT-04").first().severity, ReviewFinding::Blocker);
}

void ShowReview_Test::pat06()
{
    par("Par 1", 0);
    ShowReview::Options o;
    o.outputsLoaded = false;
    o.universesWithOutput = { 0 };
    QVERIFY(quiet("PAT-06", o));
    o.universesWithOutput = { 1 };         // output elsewhere, none on universe 1
    QVERIFY(fires("PAT-06", o));
    QCOMPARE(findings("PAT-06", o).first().severity, ReviewFinding::Warning);
    QCOMPARE(findings("PAT-06", o).first().targets, QStringList() << "uni:0");
    o.universesWithOutput.clear();          // no output anywhere: preview only
    QCOMPARE(findings("PAT-06", o).first().severity, ReviewFinding::Note);
}

/****************************************************************************
 * Fixtures
 ****************************************************************************/

void ShowReview_Test::fix01()
{
    par("Par 1", 0);
    QVERIFY(quiet("FIX-01"));
    QVERIFY(loadFixtureXml(m_doc,
        "<Fixture><Manufacturer>Nobody</Manufacturer><Model>Warmdance XL</Model><Mode>16ch</Mode>"
        "<ID>7</ID><Name>Gone 1</Name><Universe>0</Universe><Address>100</Address><Channels>16</Channels></Fixture>"));
    QVERIFY(m_doc->fixture(7)->definitionUnavailable());
    QVERIFY(fires("FIX-01"));
    const ReviewFinding f = findings("FIX-01").first();
    QCOMPARE(f.severity, ReviewFinding::Blocker);
    QVERIFY(f.message.contains("Nobody Warmdance XL"));
    QCOMPARE(f.targets, QStringList() << "fx:7");
}

void ShowReview_Test::fix05()
{
    Fixture *a = par("Par 1", 0);
    Fixture *b = par("Par 2", 10);
    MonitorProperties *mp = m_doc->monitorProperties();
    QVERIFY(quiet("FIX-05"));               // no plot at all: not asked
    mp->setFixtureProperties(a->id(), FixturePreviewItem());
    QVERIFY(fires("FIX-05"));
    QCOMPARE(findings("FIX-05").first().severity, ReviewFinding::Note);
    FixtureGroup *g = group("Spots", { b });
    FocusCall call;
    call.groups = { g->id() };
    m_doc->programmer()->focusCalls()->append(call);
    QCOMPARE(findings("FIX-05").first().severity, ReviewFinding::Warning);   // focus needs it
    mp->setFixtureProperties(b->id(), FixturePreviewItem());
    QVERIFY(quiet("FIX-05"));
}

void ShowReview_Test::fix07()
{
    par("Par 1", 0);
    QVERIFY(quiet("FIX-07"));
    QLCFixtureDef *def = new QLCFixtureDef();
    make(m_doc, "Silent", "Silent par", QLCFixtureDef::ColorChanger, def,
         { chan(def, "R", QLCChannel::IntensityRed), chan(def, "Dimmer", QLCChannel::IntensityMasterDimmer) },
         0, 0, 20);
    QVERIFY(fires("FIX-07"));
    QCOMPARE(findings("FIX-07").first().severity, ReviewFinding::Note);
}

namespace
{
/* A 510 x 205 x 102 mm bar lying flat on the floor (the UST-2 of the show this
   check came from), on the plot at (xm, ym) metres, at height z. */
Fixture *bar(Doc *doc, const QString &name, quint32 address, double xm, double ym, float z,
             bool hung = false)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    Fixture *fx = make(doc, name, QStringLiteral("Bar"), QLCFixtureDef::LEDBarBeams, def,
                       { chan(def, "Dimmer", QLCChannel::IntensityMasterDimmer) }, 50, 0, address);
    QLCFixtureMode *mode = fx->fixtureMode();
    QLCPhysical phy = mode->physical();
    phy.setWidth(510);
    phy.setHeight(205);
    phy.setDepth(102);
    mode->setPhysical(phy);
    MonitorProperties *mp = doc->monitorProperties();
    mp->setFixturePosition(fx->id(), 0, 0, QVector3D(float(xm * 1000.0), float(ym * 1000.0), z));
    FixtureRigProps rp = mp->fixtureRigProps(fx->id());
    rp.mountingType = hung ? Truss::TopHung : Truss::FloorMounted;
    rp.studioMount = 0;                     // lying flat
    mp->setFixtureRigProps(fx->id(), rp);
    return fx;
}

StagePlatform *deck(Doc *doc, const QString &name, double x, double y, double w, double d, double h)
{
    StagePlatform *pl = doc->monitorProperties()->addPlatform();
    pl->setName(name);
    pl->setOriginX(float(x));
    pl->setOriginY(float(y));
    pl->setWidth(float(w));
    pl->setDepth(float(d));
    pl->setHeight(float(h));
    return pl;
}
}

void ShowReview_Test::fix15()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    StagePlatform *sr = deck(m_doc, QStringLiteral("Deck SR"), 2.0, 2.0, 2.4, 1.2, 0.61);
    Fixture *ust = bar(m_doc, QStringLiteral("UST-2"), 0, 2.2, 2.3, 0.0f);   // on the floor, in the deck
    QVERIFY(fires("FIX-15"));
    const ReviewFinding f = findings("FIX-15").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QCOMPARE(f.targets, QStringList() << QStringLiteral("fx:%1").arg(ust->id()));
    QVERIFY2(f.message.startsWith(QStringLiteral("UST-2 is inside Deck SR")), qPrintable(f.message));
    QVERIFY2(f.message.contains(QStringLiteral("Sit it on Deck SR (0.61 m)")), qPrintable(f.message));
    QCOMPARE(f.jump, QStringLiteral("status:fixtures.buried"));
    QCOMPARE(ShowReview::footerCheckId(QStringLiteral("fixtures.buried")), QStringLiteral("FIX-15"));

    // The fix: standing on the deck, following it.
    const PlacementCheck::Buried b = PlacementCheck::check(m_doc, ust->id());
    QVERIFY(b.buried && b.canSit);
    QCOMPARE(b.sitId, sr->id());
    QVERIFY(PlacementCheck::fix(m_doc, b));
    QVERIFY(mp->fixtureRigProps(ust->id()).onDeck());
    QVERIFY(qAbs(mp->fixtureRigPosition(ust->id()).z() - 0.61f) < 0.001f);
    QVERIFY(quiet("FIX-15"));
    sr->setHeight(1.0f);                    // the deck grows: it still stands on it
    QVERIFY(quiet("FIX-15"));
    QVERIFY(qAbs(mp->fixtureRigPosition(ust->id()).z() - 1.0f) < 0.001f);
}

void ShowReview_Test::fix15Geometry()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    // No platforms at all: nothing to be inside.
    Fixture *a = bar(m_doc, QStringLiteral("A"), 0, 2.2, 2.3, 0.0f);
    QVERIFY(PlacementCheck::buried(m_doc).isEmpty());

    StagePlatform *pl = deck(m_doc, QStringLiteral("Deck"), 2.0, 2.0, 2.4, 1.2, 0.61);
    const quint32 id = a->id();
    QVERIFY(PlacementCheck::check(m_doc, id).buried);                       // in the deck
    mp->setFixturePosition(id, 0, 0, QVector3D(2200.0f, 2300.0f, 0.61f));   // on top
    QVERIFY(!PlacementCheck::check(m_doc, id).buried);
    mp->setFixturePosition(id, 0, 0, QVector3D(2200.0f, 2300.0f, 0.59f));   // 2 cm in: touching
    QVERIFY(!PlacementCheck::check(m_doc, id).buried);
    mp->setFixturePosition(id, 0, 0, QVector3D(2200.0f, 2300.0f, 0.50f));   // 11 cm in
    QVERIFY(PlacementCheck::check(m_doc, id).buried);
    QVERIFY(PlacementCheck::check(m_doc, id).depth > 0.05f);

    // Beside it: the body's far edge 2 cm into the deck's edge is touching,
    // 10 cm in is not. (Footprint is the 510 x 205 plan box from its corner.)
    mp->setFixturePosition(id, 0, 0, QVector3D(1400.0f, 2300.0f, 0.0f));   // right edge at 1.91 m
    QVERIFY(!PlacementCheck::check(m_doc, id).buried);
    mp->setFixturePosition(id, 0, 0, QVector3D(1510.0f, 2300.0f, 0.0f));   // right edge 2.02 m: 2 cm in
    QVERIFY(!PlacementCheck::check(m_doc, id).buried);
    mp->setFixturePosition(id, 0, 0, QVector3D(1600.0f, 2300.0f, 0.0f));   // 10 cm in
    QVERIFY(PlacementCheck::check(m_doc, id).buried);

    // A body of a hung light hangs below its position: at the deck top it
    // is inside; high above it, clear.
    Fixture *h = bar(m_doc, QStringLiteral("Hung"), 10, 3.2, 2.6, 0.65f, true);
    QVERIFY(PlacementCheck::check(m_doc, h->id()).buried);
    mp->setFixturePosition(h->id(), 0, 0, QVector3D(3200.0f, 2600.0f, 1.2f));
    QVERIFY(!PlacementCheck::check(m_doc, h->id()).buried);

    // Hung on a truss that runs through the deck's volume: placed by the truss.
    mp->setFixturePosition(h->id(), 0, 0, QVector3D(3200.0f, 2600.0f, 0.3f));
    QVERIFY(PlacementCheck::check(m_doc, h->id()).buried);
    Truss *t = mp->addTruss();
    t->setOrigin(QVector3D(2.5f, 2.6f, 0.4f));
    t->setLength(1.5f);
    FixtureRigProps rp = mp->fixtureRigProps(h->id());
    rp.trussId = t->id();
    rp.trussOffset = 0.5f;
    mp->setFixtureRigProps(h->id(), rp);
    QVERIFY(!PlacementCheck::check(m_doc, h->id()).buried);

    // A clear-topped step is meant to hold lights.
    mp->setFixturePosition(id, 0, 0, QVector3D(2200.0f, 2300.0f, 0.0f));
    QVERIFY(PlacementCheck::check(m_doc, id).buried);
    pl->setTopMaterial(StagePlatform::ClearTop);
    QVERIFY(!PlacementCheck::check(m_doc, id).buried);
    pl->setTopMaterial(StagePlatform::SolidTop);
    QVERIFY(PlacementCheck::check(m_doc, id).buried);

    // A turned deck: 3 m x 0.6 m turned a quarter, so it runs upstage-downstage
    // through (5, 3). The bar at (5.0 - 0.255, 3.4) is on it; the same spot is
    // clear of the deck unturned.
    StagePlatform *turned = deck(m_doc, QStringLiteral("Turned"), 3.5, 2.7, 3.0, 0.6, 0.4);
    Fixture *r = bar(m_doc, QStringLiteral("R"), 20, 4.745, 3.4, 0.0f);
    QVERIFY(!PlacementCheck::check(m_doc, r->id()).buried);                  // unturned: 3.4 is off the 2.7..3.3 deck
    turned->setRotation(90.0f);
    const PlacementCheck::Buried tb = PlacementCheck::check(m_doc, r->id());
    QVERIFY(tb.buried);
    QCOMPARE(tb.platformId, turned->id());
    // A flight of steps: the tread under the body counts, not the top of the flight.
    turned->setRotation(0.0f);
    turned->setSteps(4, 0.1f, 0.15f, 3.0f, StagePlatform::ClimbDownstage);
    QVERIFY(PlacementCheck::buried(m_doc).count() >= 1);
}

namespace
{
/* A row of eight bars like the one the show came with: UST-1..8 two metres
   upstage of the decks, all but @a floorOne standing on a deck mount. */
QList<Fixture *> ustRow(Doc *doc, int floorOne, bool naming = true, bool allFloor = false)
{
    QList<Fixture *> row;
    for (int i = 1; i <= 8; ++i)
    {
        const double x = 2.3 + 0.8 * (i - 1);
        const QString name = naming ? QStringLiteral("UST-%1").arg(i) : QStringLiteral("Bar%1x").arg(char('a' + i));
        Fixture *fx = bar(doc, name, quint32(i * 10), x, 1.78, 0.0f);
        if (i != floorOne && !allFloor)
        {
            MonitorProperties *mp = doc->monitorProperties();
            FixtureRigProps rp = mp->fixtureRigProps(fx->id());
            // Two decks side by side: ids 0 (x 2..5.2) and 1 (x 5.2..8.4).
            rp.deckPlatformId = x < 5.2 ? 0u : 1u;
            mp->setFixtureRigProps(fx->id(), rp);
        }
        row << fx;
    }
    return row;
}
}

void ShowReview_Test::fix16()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    StagePlatform *usp1 = deck(m_doc, QStringLiteral("USP1"), 2.0, 2.0, 3.2, 1.2, 0.61);
    deck(m_doc, QStringLiteral("USP2"), 5.2, 2.0, 3.2, 1.2, 0.61);
    QCOMPARE(usp1->id(), 0u);
    const QList<Fixture *> row = ustRow(m_doc, 2);      // UST-2 is the one on the floor
    Fixture *ust2 = row.at(1);
    QVERIFY(PlacementCheck::buried(m_doc).isEmpty());   // FIX-15 has nothing to say

    QVERIFY(fires("FIX-16"));
    const QList<ReviewFinding> all = findings("FIX-16");
    QCOMPARE(all.size(), 1);                            // UST-2 only
    const ReviewFinding f = all.first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QCOMPARE(f.targets, QStringList() << QStringLiteral("fx:%1").arg(ust2->id()));
    QVERIFY2(f.message.startsWith(QStringLiteral("UST-2 is on the floor; UST-1, UST-3…UST-8 are on Deck USP1 (0.61 m)")),
             qPrintable(f.message));
    QVERIFY2(f.message.contains(QStringLiteral("Sit it on Deck USP1 (0.61 m)")), qPrintable(f.message));
    QCOMPARE(f.jump, QStringLiteral("status:fixtures.offrow"));
    QCOMPARE(ShowReview::footerCheckId(QStringLiteral("fixtures.offrow")), QStringLiteral("FIX-16"));

    // Sit on Top: the deck mount FIX-15's fix makes; the warning clears.
    const PlacementCheck::OffRow o = PlacementCheck::checkRow(m_doc, ust2->id());
    QVERIFY(o.off);
    QCOMPARE(o.sitId, usp1->id());
    QCOMPARE(o.mates.size(), 7);
    QCOMPARE(o.row, QStringLiteral("name UST-"));
    QVERIFY(PlacementCheck::fix(m_doc, o));
    QVERIFY(mp->fixtureRigProps(ust2->id()).onDeck());
    QVERIFY(qAbs(mp->fixtureRigPosition(ust2->id()).z() - 0.61f) < 0.001f);
    QVERIFY(quiet("FIX-16"));
    QVERIFY(!PlacementCheck::checkRow(m_doc, ust2->id()).off);
    QVERIFY(PlacementCheck::offRow(m_doc).isEmpty());
}

void ShowReview_Test::fix16Rows()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    // No deck at all: nothing to be beside.
    QList<Fixture *> row = ustRow(m_doc, 0, true, true);
    for (Fixture *fx : row)
        QVERIFY(PlacementCheck::deckOf(m_doc, fx->id()) == UINT_MAX);
    QVERIFY(PlacementCheck::offRow(m_doc).isEmpty());
    deck(m_doc, QStringLiteral("USP1"), 2.0, 2.0, 3.2, 1.2, 0.61);
    deck(m_doc, QStringLiteral("USP2"), 5.2, 2.0, 3.2, 1.2, 0.61);

    // A deliberately floor-mounted row: all on the floor, nothing to compare.
    QVERIFY(PlacementCheck::offRow(m_doc).isEmpty());

    // One light off the row on a deck, the rest on the floor: not a majority.
    auto mount = [&](Fixture *fx, bool on) {
        FixtureRigProps rp = mp->fixtureRigProps(fx->id());
        rp.deckPlatformId = on ? (mp->fixtureRigPosition(fx->id()).x() < 5.2f ? 0u : 1u) : UINT_MAX;
        mp->setFixtureRigProps(fx->id(), rp);
    };
    mount(row.at(0), true);
    mount(row.at(1), true);
    QVERIFY(PlacementCheck::offRow(m_doc).isEmpty());       // 2 of 7 mates
    for (int i = 2; i < 5; ++i)
        mount(row.at(i), true);
    // 5 on decks: each of the three on the floor has 5 of 7 mates up.
    QCOMPARE(PlacementCheck::offRow(m_doc).size(), 3);
    for (int i = 5; i < 8; ++i)
        mount(row.at(i), true);
    QVERIFY(PlacementCheck::offRow(m_doc).isEmpty());       // all up: nothing to say

}

void ShowReview_Test::fix16Single()
{
    // A single light beside a deck has no row-mates; two do not make a majority.
    deck(m_doc, QStringLiteral("USP1"), 2.0, 2.0, 3.2, 1.2, 0.61);
    Fixture *lone = bar(m_doc, QStringLiteral("Lone"), 200, 3.0, 1.78, 0.0f);
    QVERIFY(!PlacementCheck::checkRow(m_doc, lone->id()).off);
    Fixture *mate = bar(m_doc, QStringLiteral("Lone 2"), 210, 3.8, 1.78, 0.0f);
    FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(mate->id());
    rp.deckPlatformId = 0;
    m_doc->monitorProperties()->setFixtureRigProps(mate->id(), rp);
    QVERIFY(!PlacementCheck::checkRow(m_doc, lone->id()).off);      // one mate: needs two
    QVERIFY(quiet("FIX-16"));
}

void ShowReview_Test::fix16Sources()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    deck(m_doc, QStringLiteral("Riser"), 2.0, 2.0, 6.0, 1.2, 0.4);
    // Names that say nothing, no group: found as a line (same Y within 0.3 m).
    QList<Fixture *> row = ustRow(m_doc, 3, false);
    QVERIFY(row.at(0)->name() != row.at(1)->name());
    // The decks above are ids 0 and 1; only one exists here.
    for (Fixture *fx : row)
    {
        FixtureRigProps rp = mp->fixtureRigProps(fx->id());
        if (rp.onDeck())
            rp.deckPlatformId = 0;
        mp->setFixtureRigProps(fx->id(), rp);
    }
    // Their names end in a letter ("Bara1x"...): no number, so no series.
    const PlacementCheck::OffRow lineRow = PlacementCheck::checkRow(m_doc, row.at(2)->id());
    QVERIFY(lineRow.off);
    QCOMPARE(lineRow.row, QStringLiteral("line"));
    // Far from the line, the same light is just a floor light.
    mp->setFixturePosition(row.at(2)->id(), 0, 0, QVector3D(4700.0f, 5.0f * 1000.0f, 0.0f));
    QVERIFY(!PlacementCheck::checkRow(m_doc, row.at(2)->id()).off);
    mp->setFixturePosition(row.at(2)->id(), 0, 0, QVector3D(4700.0f, 1780.0f, 0.0f));
    QVERIFY(PlacementCheck::checkRow(m_doc, row.at(2)->id()).off);
    // A group is preferred over the line.
    QList<Fixture *> members = row;
    group("Upstage bars", members);
    QCOMPARE(PlacementCheck::checkRow(m_doc, row.at(2)->id()).row, QStringLiteral("group Upstage bars"));

    // Mixed types in the group: a par on the floor has no bar mates; and the
    // bars' own row is not diluted by it.
    Fixture *p = par("Par 1", 300);
    mp->setFixturePosition(p->id(), 0, 0, QVector3D(4700.0f, 1780.0f, 0.0f));
    FixtureGroup *mixed = group("Mixed", members << p);
    Q_UNUSED(mixed);
    QVERIFY(!PlacementCheck::checkRow(m_doc, p->id()).off);
    QVERIFY(PlacementCheck::checkRow(m_doc, row.at(2)->id()).off);
}

void ShowReview_Test::fix16Save()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    deck(m_doc, QStringLiteral("USP1"), 2.0, 2.0, 3.2, 1.2, 0.61);
    deck(m_doc, QStringLiteral("USP2"), 5.2, 2.0, 3.2, 1.2, 0.61);
    const QList<Fixture *> row = ustRow(m_doc, 2);
    auto dump = [&]() {
        QString xml;
        QXmlStreamWriter w(&xml);
        mp->saveXML(&w, m_doc, true);
        return xml;
    };
    const QString before = dump();
    QVERIFY(PlacementCheck::checkRow(m_doc, row.at(1)->id()).off);
    QVERIFY(!PlacementCheck::offRow(m_doc).isEmpty());
    QCOMPARE(dump(), before);                               // reading it writes nothing
    QVERIFY(!before.contains(QLatin1String("offrow"), Qt::CaseInsensitive));
    QVERIFY(PlacementCheck::fix(m_doc, PlacementCheck::checkRow(m_doc, row.at(1)->id())));
    const QString after = dump();
    QVERIFY(!after.contains(QLatin1String("offrow"), Qt::CaseInsensitive));
    // Saved and read back, the fixed show stays quiet (the deck mount is the whole fix).
    MonitorProperties back;
    QXmlStreamReader r(after);
    while (!r.atEnd() && !(r.isStartElement() && r.name() == QLatin1String("Monitor")))
        r.readNext();
    QVERIFY(back.loadXML(r, m_doc));
    QVERIFY(back.fixtureRigProps(row.at(1)->id()).onDeck());
    QVERIFY(PlacementCheck::offRow(m_doc).isEmpty());
}

void ShowReview_Test::fix14()
{
    par("Par 1", 0);
    Fixture *b = par("Par 2", 10);
    QVERIFY(quiet("FIX-14"));
    b->setName("Par 1");
    QVERIFY(fires("FIX-14"));
}

/****************************************************************************
 * Effects
 ****************************************************************************/

void ShowReview_Test::fx07()
{
    Fixture *a = par("Par 1", 0);
    QLCPalette *fx = new QLCPalette(QLCPalette::Effect, m_doc);
    fx->setName("Sparkle");
    m_doc->addPalette(fx);
    QVERIFY(quiet("FX-07"));                // not in a scene: nothing plays it
    Scene *s = new Scene(m_doc);
    s->addFixture(a->id());
    s->addPalette(fx->id());
    m_doc->addFunction(s);
    QVERIFY(fires("FX-07"));                // no script chosen
    QCOMPARE(findings("FX-07").first().severity, ReviewFinding::Blocker);
    QVERIFY(m_doc->showReview()->designEffectProblems(m_doc).contains(fx->id()));
}

/****************************************************************************
 * Power
 ****************************************************************************/

void ShowReview_Test::pwr01()
{
    Fixture *a = dimmer("Lamp 1", 0);
    Fixture *b = dimmer("Lamp 2", 1);
    PowerDistribution *pd = m_doc->powerDistribution();
    PowerSource src;
    src.name = "Distro";
    src.voltage = 120;
    PowerCircuit cir;
    cir.name = "1";
    cir.ratedAmps = 20;
    cir.deratePercent = 80;
    src.circuits << cir;
    pd->sources() << src;
    pd->assignFixture(a->id(), 0, 0);
    pd->assignFixture(b->id(), 0, 0);

    Scene *s = new Scene(m_doc);
    s->setName("Half");
    s->setValue(a->id(), 0, 127);           // about 500 W: 4 A
    m_doc->addFunction(s);
    QVERIFY(quiet("PWR-01"));
    s->setValue(b->id(), 0, 255);           // + 1000 W: about 12.6 A, over 16 A? no
    QVERIFY(quiet("PWR-01"));
    s->setValue(a->id(), 0, 255);           // 2000 W: 16.7 A, over 80 % of 20 A
    QVERIFY(fires("PWR-01"));
    QCOMPARE(findings("PWR-01").first().severity, ReviewFinding::Warning);
    pd->sources()[0].circuits[0].ratedAmps = 15;   // over the breaker itself
    pd->sources()[0].circuits[0].deratePercent = 80;
    QCOMPARE(findings("PWR-01").first().severity, ReviewFinding::Blocker);
    QVERIFY(findings("PWR-01").first().targets.contains(QStringLiteral("circ:0.0")));
}

void ShowReview_Test::pwr03()
{
    Fixture *a = dimmer("Lamp 1", 0);
    Fixture *b = dimmer("Lamp 2", 1);
    QVERIFY(quiet("PWR-03"));               // power not set up: not asked
    PowerDistribution *pd = m_doc->powerDistribution();
    PowerSource src;
    src.name = "Distro";
    PowerCircuit cir;
    cir.name = "1";
    src.circuits << cir;
    pd->sources() << src;
    pd->assignFixture(a->id(), 0, 0);
    QVERIFY(fires("PWR-03"));
    QCOMPARE(findings("PWR-03").first().targets, QStringList() << QStringLiteral("fx:%1").arg(b->id()));
    pd->assignFixture(b->id(), 0, 0);
    QVERIFY(quiet("PWR-03"));
}

void ShowReview_Test::pwr05()
{
    QList<Fixture *> spots;
    for (int i = 0; i < 4; ++i)
        spots << mover(QStringLiteral("Spot %1").arg(i + 1), quint32(i * 10));
    Scene *s = new Scene(m_doc);
    s->setName("Strobe");
    for (Fixture *fx : spots)
    {
        s->setValue(fx->id(), 2, 255);      // lit
        s->setValue(fx->id(), 3, 30);       // 1 + 19 * 20/245: about 2.6 Hz
    }
    m_doc->addFunction(s);
    QVERIFY(quiet("PWR-05"));
    for (Fixture *fx : spots)
        s->setValue(fx->id(), 3, 200);      // about 15.7 Hz
    QVERIFY(fires("PWR-05"));
    QCOMPARE(findings("PWR-05").first().severity, ReviewFinding::Warning);    // 4 lights
    for (int i = 1; i < 4; ++i)
        s->setValue(spots.at(i)->id(), 3, 0);
    QCOMPARE(findings("PWR-05").first().severity, ReviewFinding::Note);       // one light
    for (Fixture *fx : spots)
        s->setValue(fx->id(), 2, 0);        // dark: nothing flashes
    QVERIFY(quiet("PWR-05"));
}

void ShowReview_Test::pwr06()
{
    Fixture *a = par("Par 1", 0);
    Fixture *house = dimmer("House", 10);
    house->setOutputRules(Fixture::ExemptBlackout | Fixture::ExemptGrandMaster);
    QVERIFY(quiet("PWR-06"));               // a dimmer: a house light
    a->setOutputRules(Fixture::ExemptBlackout | Fixture::OnAtOpen);
    QVERIFY(quiet("PWR-06"));               // a work light, said so
    a->setOutputRules(Fixture::ExemptBlackout);
    QVERIFY(fires("PWR-06"));
}

/****************************************************************************
 * Files
 ****************************************************************************/

void ShowReview_Test::file01()
{
    QVERIFY(m_dir.isValid());
    QDir root(m_dir.path());
    QVERIFY(root.mkpath("show") && root.mkpath("elsewhere"));
    QFile in(root.filePath("show/inside.wav")), out(root.filePath("elsewhere/outside.wav"));
    QVERIFY(in.open(QIODevice::WriteOnly) && out.open(QIODevice::WriteOnly));
    in.close();
    out.close();
    m_doc->setWorkspacePath(root.filePath("show"));
    QVERIFY(loadFunctionXml(m_doc, QStringLiteral("<Function ID=\"1\" Type=\"Audio\" Name=\"In\">"
                                                  "<Source>%1</Source></Function>").arg(in.fileName())));
    QVERIFY(quiet("FILE-01"));
    QVERIFY(loadFunctionXml(m_doc, QStringLiteral("<Function ID=\"2\" Type=\"Audio\" Name=\"Out\">"
                                                  "<Source>%1</Source></Function>").arg(out.fileName())));
    QVERIFY(fires("FILE-01"));
    QCOMPARE(findings("FILE-01").first().targets, QStringList() << "fn:2");
}

void ShowReview_Test::file02()
{
    QVERIFY(m_dir.isValid());
    QFile here(QDir(m_dir.path()).filePath("here.wav"));
    QVERIFY(here.open(QIODevice::WriteOnly));
    here.close();
    m_doc->setWorkspacePath(m_dir.path());
    QVERIFY(loadFunctionXml(m_doc, QStringLiteral("<Function ID=\"1\" Type=\"Audio\" Name=\"Here\">"
                                                  "<Source>%1</Source></Function>").arg(here.fileName())));
    QVERIFY(quiet("FILE-02"));
    QVERIFY(loadFunctionXml(m_doc, QStringLiteral("<Function ID=\"2\" Type=\"Audio\" Name=\"Gone\">"
                                                  "<Source>%1/gone.wav</Source></Function>").arg(m_dir.path())));
    QVERIFY(fires("FILE-02"));
    QCOMPARE(findings("FILE-02").first().severity, ReviewFinding::Blocker);
}

/****************************************************************************
 * Names
 ****************************************************************************/

void ShowReview_Test::name01()
{
    auto scene = [this](const QString &name) {
        Scene *s = new Scene(m_doc);
        s->setName(name);
        s->setPath("Act 1");
        m_doc->addFunction(s);
        return s;
    };
    scene("03-01.01-Verse wash");
    scene("03-01.02-Chorus");
    QVERIFY(quiet("NAME-01"));
    Scene *odd = scene("verse wash");
    QVERIFY(fires("NAME-01"));
    QCOMPARE(findings("NAME-01").first().targets, QStringList() << QStringLiteral("fn:%1").arg(odd->id()));
}

void ShowReview_Test::name02()
{
    Scene *s = new Scene(m_doc);
    s->setName("Warm wash");
    m_doc->addFunction(s);
    QVERIFY(quiet("NAME-02"));
    s->setName("New Scene 4");
    QVERIFY(fires("NAME-02"));
    s->setName("Warm wash (Copy)");
    QVERIFY(fires("NAME-02"));
}

/****************************************************************************
 * Palettes and looks
 ****************************************************************************/

void ShowReview_Test::pal01()
{
    Fixture *a = par("Par 1", 0), *b = par("Par 2", 10), *c = par("Par 3", 20);
    FixtureGroup *left = group("Left", { a, b });
    FixtureGroup *right = group("Right", { c });
    QLCPalette *red = colour(Qt::red), *blue = colour(Qt::blue);
    ShowTreeController *st = m_doc->showTree();
    const quint32 pid = st->addProduction("Show");
    const quint32 lid = st->addCueList(pid, "Act 1");
    const quint32 cue = st->addCue(lid, -1, "One");
    Target tl, tr;
    tl.groups = { left->id() };
    tr.groups = { right->id() };
    const quint32 l1 = st->addLayer(cue, tl);
    const quint32 l2 = st->addLayer(cue, tr);
    Look look;
    look.key = KeyColour;
    look.palettes = { red->id() };
    QVERIFY(st->setLayerLook(cue, l1, look));
    look.palettes = { blue->id() };
    QVERIFY(st->setLayerLook(cue, l2, look));
    QVERIFY(quiet("PAL-01"));               // apart: no fight
    right->assignFixture(b->id());          // Par 2 in both
    QVERIFY(fires("PAL-01"));
    const ReviewFinding f = findings("PAL-01").first();
    QVERIFY(f.message.contains("Par 2"));
    QCOMPARE(f.jump, QStringLiteral("cue"));
}

void ShowReview_Test::pal02()
{
    Fixture *a = par("Par 1", 0);
    QLCPalette *red = colour(Qt::red);
    Scene *s = new Scene(m_doc);
    s->setName("Look");
    s->addPalette(red->id());
    m_doc->addFunction(s);
    QVERIFY(fires("PAL-02"));               // a look with no targets lights nothing
    s->addFixture(a->id());
    QVERIFY(quiet("PAL-02"));
}

void ShowReview_Test::pal03()
{
    Fixture *a = dimmer("Lamp 1", 0);
    Fixture *b = par("Par 1", 10);
    FixtureGroup *g = group("Front", { b });
    QLCPalette *red = colour(Qt::red);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 cue = st->addCue(lid, -1, "One");
    Target t;
    t.groups = { g->id() };
    const quint32 layer = st->addLayer(cue, t);
    Look look;
    look.key = KeyColour;
    look.palettes = { red->id() };
    QVERIFY(st->setLayerLook(cue, layer, look));
    QVERIFY(quiet("PAL-03"));
    g->assignFixture(a->id());              // a lamp has no colour: some
    QVERIFY(fires("PAL-03"));
    QCOMPARE(findings("PAL-03").first().severity, ReviewFinding::Note);
    g->resignFixture(b->id());              // only the lamp: none
    QCOMPARE(findings("PAL-03").first().severity, ReviewFinding::Warning);
}

void ShowReview_Test::pal05()
{
    Fixture *a = par("Par 1", 0);
    FixtureGroup *g = group("Front", { a });
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 cue = st->addCue(lid, -1, "One");
    Target t;
    t.groups = { g->id() };
    st->addLayer(cue, t);
    QVERIFY(quiet("PAL-05"));
    g->resignFixture(a->id());              // a group now empty
    QVERIFY(fires("PAL-05"));
    QVERIFY(findings("PAL-05").first().message.contains("empty"));
}

/****************************************************************************
 * Tracking and cues
 ****************************************************************************/

void ShowReview_Test::cue01()
{
    Fixture *a = par("Par 1", 0);
    FixtureGroup *g = group("Special", { a });
    QLCPalette *red = colour(Qt::red);
    ShowTreeController *st = m_doc->showTree();
    const quint32 pid = st->addProduction("Show");
    QVERIFY(st->setNumbering(pid, Numbering::ShowPartIteration));
    const quint32 lid = st->addCueList(pid, "Main");
    const quint32 c1 = st->addCueNumbered(lid, "1.0.1", "Ballad");
    st->addCueNumbered(lid, "1.0.2", "Ballad 2");
    const quint32 c3 = st->addCueNumbered(lid, "2.0.1", "Finale");
    Target t;
    t.groups = { g->id() };
    const quint32 layer = st->addLayer(c1, t);
    Look look;
    look.key = KeyColour;
    look.palettes = { red->id() };
    QVERIFY(st->setLayerLook(c1, layer, look));
    QVERIFY(fires("CUE-01"));               // song 1's special still up in song 2
    const ReviewFinding f = findings("CUE-01").first();
    QVERIFY(f.targets.contains(QStringLiteral("list:%1/cue:%2").arg(lid).arg(c1)));
    QVERIFY(st->setCueMode(c3, CueMode::Block));
    QVERIFY(quiet("CUE-01"));
}

void ShowReview_Test::cue02()
{
    Fixture *a = par("Par 1", 0);
    FixtureGroup *g = group("Wash", { a });
    QLCPalette *red = colour(Qt::red);
    ShowTreeController *st = m_doc->showTree();
    const quint32 pid = st->addProduction("Show");
    QVERIFY(st->setNumbering(pid, Numbering::ShowPartIteration));
    const quint32 lid = st->addCueList(pid, "Main");
    const quint32 c1 = st->addCueNumbered(lid, "1.0.1", "Ballad");
    const quint32 c2 = st->addCueNumbered(lid, "2.0.1", "Finale");
    Target t;
    t.groups = { g->id() };
    const quint32 layer = st->addLayer(c1, t);
    Look look;
    look.key = KeyColour;
    look.palettes = { red->id() };
    QVERIFY(st->setLayerLook(c1, layer, look));
    QVERIFY(fires("CUE-02"));
    QCOMPARE(findings("CUE-02").first().targets, QStringList() << QStringLiteral("list:%1/cue:%2").arg(lid).arg(c2));
    QVERIFY(st->setCueMode(c2, CueMode::Block));
    QVERIFY(quiet("CUE-02"));
}

void ShowReview_Test::cue04()
{
    Fixture *a = par("Par 1", 0);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 cue = st->addCue(lid, -1, "One");
    QVERIFY(fires("CUE-04"));
    Target t;
    t.fixtures = { a->id() };
    st->addLayer(cue, t);
    QVERIFY(quiet("CUE-04"));
}

void ShowReview_Test::cue05()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    st->addCue(lid, 0, "Two");
    Cue cue = *st->cueList(lid)->cue(c1);
    cue.hold = 0;
    cue.fadeIn = 500;
    QVERIFY(st->setCue(cue));
    QVERIFY(quiet("CUE-05"));
    cue.fadeIn = 0;
    QVERIFY(st->setCue(cue));
    QVERIFY(fires("CUE-05"));
}

void ShowReview_Test::cue06()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    st->addCueNumbered(lid, "14", "One");
    st->addCueNumbered(lid, "15", "Two");
    QVERIFY(quiet("CUE-06"));
    // Typing refuses it; an import or a hand-edited file can have it.
    editTree([](QString &xml) { xml.replace("Number=\"15\"", "Number=\"14\""); });
    QVERIFY(fires("CUE-06"));
    QVERIFY(findings("CUE-06").first().message.contains("14"));
}

void ShowReview_Test::cue07()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    st->addCue(lid, -1, "One");
    const quint32 c2 = st->addCue(lid, 0, "Two");
    QVERIFY(quiet("CUE-07"));
    QVERIFY(st->setCuesSkipped({ c2 }, true));
    QVERIFY(fires("CUE-07"));
}

void ShowReview_Test::cue08()
{
    Fixture *a = par("Par 1", 0);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "Base");
    const quint32 c2 = st->addCue(lid, 0, "Two");
    Target t;
    t.fixtures = { a->id() };
    const quint32 layer = st->addLayer(c2, t);
    QVERIFY(st->setLayerRelease(c2, layer, true));
    QVERIFY(fires("CUE-08"));
    QCOMPARE(findings("CUE-08").first().severity, ReviewFinding::Note);    // no base: to dark
    QVERIFY(st->setBaseCue(lid, c1));
    QVERIFY(quiet("CUE-08"));
    QVERIFY(st->setCuesSkipped({ c1 }, true));
    QCOMPARE(findings("CUE-08").first().severity, ReviewFinding::Warning); // a skipped base
}

void ShowReview_Test::cue11()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    st->addCue(lid, -1, "One");
    QVERIFY(quiet("CUE-11"));
    // An include of a cue that isn't there (a hand-edited file).
    editTree([](QString &xml) { xml.replace("Name=\"One\"", "Name=\"One\" Includes=\"9999\""); });
    QVERIFY(fires("CUE-11"));
    QCOMPARE(findings("CUE-11").first().severity, ReviewFinding::Blocker);
}

/****************************************************************************
 * Hits
 ****************************************************************************/

void ShowReview_Test::vc01()
{
    Fixture *a = par("Par 1", 0);
    FixtureGroup *g = group("All", { a });
    QLCPalette *red = colour(Qt::red), *blue = colour(Qt::blue);
    ShowTreeController *st = m_doc->showTree();
    const quint32 pid = st->addProduction("Show");
    const quint32 lid = st->addCueList(pid, "Main");
    const quint32 cue = st->addCue(lid, -1, "One");
    const quint32 h1 = st->addLook(pid, "Red stab");
    const quint32 h2 = st->addLook(pid, "Blue stab");
    Target t;
    t.groups = { g->id() };
    Look look;
    look.key = KeyColour;
    look.palettes = { red->id() };
    QVERIFY(st->setLayerLook(h1, st->addLayer(h1, t), look));
    look.palettes = { blue->id() };
    QVERIFY(st->setLayerLook(h2, st->addLayer(h2, t), look));
    QVERIFY(st->sameHitPlace(h1, h2));
    QVERIFY(fires("VC-01"));
    QVERIFY(findings("VC-01").first().message.contains("Red stab"));
    QVERIFY(st->setHitScope({ h2 }, HitScope::Cue, cue));   // another place
    QVERIFY(quiet("VC-01"));
}

/****************************************************************************
 * Teaching, lasers, and the rest of the v1 list
 ****************************************************************************/

void ShowReview_Test::teach01()
{
    QVERIFY(quiet("TEACH-01"));
    m_doc->teachingSettings()->setEnabled(true);
    QVERIFY(fires("TEACH-01"));
    ReviewFinding f = findings("TEACH-01").first();
    QCOMPARE(f.severity, ReviewFinding::Note);
    QCOMPARE(f.jump, QStringLiteral("showsettings"));
    // Accepted with a note ("class copy"): quiet; letting output through is a change.
    QVERIFY(m_doc->showReview()->accept(f, QStringLiteral("class copy"), QStringLiteral("Sam")));
    QVERIFY(findings("TEACH-01").first().state == ReviewFinding::Accepted);
    m_doc->teachingSettings()->setAllowOutput(true);
    QVERIFY(findings("TEACH-01").first().state == ReviewFinding::Changed);
    m_doc->teachingSettings()->setEnabled(false);
    QVERIFY(quiet("TEACH-01"));
}

void ShowReview_Test::pwr09()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 8));   // the stage's front edge is 8 m downstage
    // A pencil laser on a truss at 5 m throwing 20 degrees down: it reaches the
    // floor 2.8 m up the room past the edge, in the audience.
    Fixture *l = laser("Laser 1", 0, QLCPhysical::LaserEmission, 10, QVector3D(5, 2, 5), 20);
    QVERIFY(l != nullptr);
    QVERIFY2(fires("PWR-09"), "beam into the house");
    ReviewFinding f = findings("PWR-09").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QVERIFY(f.message.contains("Laser 1") && f.message.contains("beam"));
    // Steep enough that the floor stops it on the stage: quiet.
    mp->setFixtureProperties(l->id(), mp->fixtureProperties(l->id()));
    FixtureRigProps rp = mp->fixtureRigProps(l->id());
    rp.aimDown = 60;
    mp->setFixtureRigProps(l->id(), rp);
    QVERIFY(quiet("PWR-09"));
    // Thrown upstage: quiet.
    rp.aimDown = 20;
    rp.panZeroDir = 180;
    mp->setFixtureRigProps(l->id(), rp);
    QVERIFY(quiet("PWR-09"));
    rp.panZeroDir = 0;
    rp.aimDown = -10;                       // tipped up over the heads
    mp->setFixtureRigProps(l->id(), rp);
    QVERIFY(quiet("PWR-09"));
    // A wide star field on a level mount: its lower rim comes down into the house.
    Fixture *s = laser("Stars", 10, QLCPhysical::StarEmission, 100, QVector3D(5, 2, 5), 0);
    QVERIFY(s != nullptr);
    QVERIFY2(fires("PWR-09"), "star field into the house");
    f = findings("PWR-09").first();
    QVERIFY(f.message.contains("star field"));
    QCOMPARE(f.targets, QStringList() << QStringLiteral("fx:%1").arg(s->id()));
    // Not a laser: a par thrown the same way is not this check's.
    QVERIFY(m_doc->deleteFixture(s->id()));
    Fixture *p = par("Par 1", 20);
    QVERIFY(p != nullptr);
    QVERIFY(quiet("PWR-09"));
}

void ShowReview_Test::cue03()
{
    Fixture *a = par("Par 1", 0);
    QLCPalette *red = colour(Qt::red), *blue = colour(Qt::blue);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    const quint32 c2 = st->addCue(lid, 0, "Two");
    Target t;
    t.fixtures = { a->id() };
    Look look;
    look.key = KeyColour;
    look.palettes = { red->id() };
    const quint32 l1 = st->addLayer(c1, t);
    QVERIFY(st->setLayerLook(c1, l1, look));
    look.palettes = { blue->id() };
    const quint32 l2 = st->addLayer(c2, t);
    QVERIFY(st->setLayerLook(c2, l2, look));
    QVERIFY(quiet("CUE-03"));               // red, then blue: a change
    look.palettes = { red->id() };
    QVERIFY(st->setLayerLook(c2, l2, look));
    QVERIFY(fires("CUE-03"));               // red, then red again
    const ReviewFinding f = findings("CUE-03").first();
    QCOMPARE(f.severity, ReviewFinding::Note);
    QCOMPARE(f.targets, QStringList() << QStringLiteral("list:%1/cue:%2").arg(lid).arg(c2));
}

void ShowReview_Test::cue12()
{
    Fixture *a = par("Par 1", 0);
    Scene *sc = new Scene(m_doc);
    sc->setName("Wash");
    sc->setValue(a->id(), 3, 200);
    m_doc->addFunction(sc);
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(sc->id(), 0, Function::infiniteSpeed(), 0));
    chaser->addStep(ChaserStep(sc->id(), 0, Function::infiniteSpeed(), 0));
    ShowTreeController *st = m_doc->showTree();
    st->importInPlace(chaser->id());
    QVERIFY(quiet("CUE-12"));               // the list plays the show, the chaser is only kept
    // Something added later plays the old chaser again.
    Collection *again = new Collection(m_doc);
    again->setName("Old way");
    again->addFunction(chaser->id());
    m_doc->addFunction(again);
    QVERIFY(fires("CUE-12"));
    const ReviewFinding f = findings("CUE-12").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QVERIFY(f.message.contains("Old way") && f.message.contains("Full Show"));
}

void ShowReview_Test::pat05()
{
    Fixture *a = par("Par 1", 400);
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    QVERIFY(m_dir.isValid());
    v->setShowFile(QDir(m_dir.path()).filePath("Tour.qlcc"), false);
    const QString venue = v->addVenue(QStringLiteral("Lincoln HS"));
    VenueRig::NotOurs keep;
    keep.universe = 0;
    keep.from = 100;
    keep.to = 140;
    QVERIFY(v->setNotOurs(venue, { keep }));
    QVERIFY(quiet("PAT-05"));               // the venue keeps channels the show doesn't use
    keep.from = 395;
    keep.to = 405;
    QVERIFY(v->setNotOurs(venue, { keep }));        // applied nowhere: a dry run says it
    QVERIFY(fires("PAT-05"));
    const ReviewFinding f = findings("PAT-05").first();
    QCOMPARE(f.severity, ReviewFinding::Blocker);
    QVERIFY(f.message.contains("Lincoln HS") && f.message.contains("Par 1"));
    QVERIFY(m_doc->unpatchFixture(a->id()));
    QVERIFY(quiet("PAT-05"));               // no longer patched: no clash
}

void ShowReview_Test::fix02()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    Fixture *a = make(m_doc, "Par 1", "Par", QLCFixtureDef::ColorChanger, def,
                      { chan(def, "R", QLCChannel::IntensityRed), chan(def, "Dimmer", QLCChannel::IntensityMasterDimmer) },
                      100, 0, 0);
    QVERIFY(a != nullptr);
    QVERIFY(quiet("FIX-02"));
    def->setModel(QString());               // an error the fixture editor shows: no model
    QVERIFY(fires("FIX-02"));
    const ReviewFinding f = findings("FIX-02").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QVERIFY(f.message.contains("model"));
    QCOMPARE(f.targets, QStringList() << QStringLiteral("fx:%1").arg(a->id()));
}

void ShowReview_Test::fix08()
{
    Fixture *a = par("Par 1", 0);
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    QVERIFY(m_dir.isValid());
    v->setShowFile(QDir(m_dir.path()).filePath("Tour.qlcc"), false);
    const QString venue = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(venue));
    QVERIFY(v->setDead(a->id(), true));
    QVERIFY(quiet("FIX-08"));               // dead, and nothing lights it
    Scene *s = new Scene(m_doc);
    s->setName("Look");
    s->setValue(a->id(), 3, 255);
    m_doc->addFunction(s);
    QVERIFY(fires("FIX-08"));
    const ReviewFinding f = findings("FIX-08").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QVERIFY(f.message.contains("Par 1") && f.message.contains("Lincoln HS"));
    QVERIFY(v->setDead(a->id(), false));
    QVERIFY(quiet("FIX-08"));
}

void ShowReview_Test::fx02()
{
    Fixture *rgb = par("Par 1", 0);
    Fixture *lamp = dimmer("Lamp", 10);
    FixtureGroup *g = group("Truss", { rgb });
    RGBMatrix *m = new RGBMatrix(m_doc);
    m->setName("Rainbow");
    m_doc->addFunction(m);
    m->setFixtureGroup(g->id());
    QVERIFY(quiet("FX-02"));                // every head mixes colour
    g->assignFixture(lamp->id());
    QVERIFY(fires("FX-02"));
    const ReviewFinding f = findings("FX-02").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QVERIFY(f.message.contains("1 of 2") && f.message.contains("Rainbow"));
}

void ShowReview_Test::fx03()
{
    EFX *e = new EFX(m_doc);
    e->setName("Circle");
    e->setWidth(60);
    e->setHeight(60);
    e->setDuration(4000);
    m_doc->addFunction(e);
    QVERIFY(quiet("FX-03"));
    e->setWidth(0);
    e->setHeight(0);
    QVERIFY(fires("FX-03"));
    QVERIFY(findings("FX-03").first().message.contains("size 0"));
    e->setWidth(60);
    QVERIFY(quiet("FX-03"));
    RGBMatrix *m = new RGBMatrix(m_doc);
    m->setName("Rainbow");
    m->setDuration(500);
    m_doc->addFunction(m);
    QVERIFY(quiet("FX-03"));
    m->setDuration(0);
    QVERIFY(fires("FX-03"));
}

void ShowReview_Test::file03()
{
    QVERIFY(quiet("FILE-03"));              // nothing carried
    // A carried definition whose checksum doesn't match what was saved.
    const QString xml = QStringLiteral(
        "<Embedded><FixtureDefinition Manufacturer=\"Test\" Model=\"Par\" SHA256=\"0000\">"
        "<![CDATA[<FixtureDefinition/>]]></FixtureDefinition></Embedded>");
    QXmlStreamReader r(xml);
    while (!r.atEnd() && !(r.isStartElement() && r.name() == KXMLShowEmbedded))
        r.readNext();
    QVERIFY(m_doc->showEmbedding()->loadXML(r));
    QVERIFY(fires("FILE-03"));
    const ReviewFinding f = findings("FILE-03").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QVERIFY(f.message.contains("altered"));
}

/****************************************************************************
 * Acceptance
 ****************************************************************************/

/****************************************************************************
 * The Virtual Console (VcIndex) and the checks on it
 ****************************************************************************/

namespace
{
QString button(int id, const QString &caption, const QString &function, const QString &key = QString(),
               const QString &input = QString(), const QString &action = QStringLiteral("Toggle"),
               const QString &extra = QString())
{
    return QStringLiteral("<Button Caption=\"%1\" ID=\"%2\"%7><Function ID=\"%3\"/><Action>%4</Action>%5%6</Button>")
        .arg(caption).arg(id).arg(function, action,
                                  key.isEmpty() ? QString() : QStringLiteral("<Key>%1</Key>").arg(key),
                                  input.isEmpty() ? QString() : input, extra);
}

QString inputOf(int universe, int channel)
{
    return QStringLiteral("<Input Universe=\"%1\" Channel=\"%2\"/>").arg(universe).arg(channel);
}
}

void ShowReview_Test::vcIndexReads()
{
    const QString xml = QStringLiteral(
        "<Workspace><Engine/><VirtualConsole><Frame Caption=\"\">"
        "<Button Caption=\"Wash\" ID=\"1\" Page=\"2\"><Function ID=\"7\" Hit=\"31\" SamePlace=\"Step\">"
        "<Hit ID=\"32\"/></Function><Action>Toggle</Action><Key>Ctrl+B</Key>"
        "<Input Universe=\"1\" Channel=\"20\"/></Button>"
        "<Slider Caption=\"Level\" ID=\"2\"><Input Universe=\"0\" Channel=\"3\"/>"
        "<SliderMode>Level</SliderMode><Level><Channel Fixture=\"4\">2</Channel></Level>"
        "<Playback><Function>9</Function></Playback></Slider>"
        "<Slider Caption=\"Fader\" ID=\"3\"><SliderMode>Playback</SliderMode><Playback><Function>9</Function>"
        "<Flash><Key>F</Key></Flash></Playback></Slider>"
        "<Slider Caption=\"Group\" ID=\"4\"><SliderMode>GroupMaster</SliderMode><GroupMaster Group=\"6\"/></Slider>"
        "<CueList Caption=\"Cues\" ID=\"5\"><Chaser>12</Chaser><Next><Key>N</Key><Input Universe=\"1\" Channel=\"5\"/></Next></CueList>"
        "<CueRunner Caption=\"Go\" ID=\"6\" Follow=\"3\"/>"
        "<SoloFrame Caption=\"Solo\" ID=\"7\"><Multipage PagesNum=\"2\" CurrentPage=\"0\"/>"
        "<Button Caption=\"Inner\" ID=\"8\" Page=\"1\"><Function ID=\"4294967295\"/><Action>Flash</Action></Button></SoloFrame>"
        "<Speed Caption=\"x\" ID=\"9\"/>"
        "<SpeedDial Caption=\"Dial\" ID=\"10\"><Function>14</Function></SpeedDial>"
        "<Clock Caption=\"Clk\" ID=\"11\"><Schedule Function=\"15\" Time=\"12:00:00\"/></Clock>"
        "</Frame><Properties><GrandMaster Visible=\"1\"><Input Universe=\"0\" Channel=\"9\"/></GrandMaster></Properties>"
        "</VirtualConsole></Workspace>");
    const VcIndex vc = VcIndex::fromXml(xml.toUtf8());
    QVERIFY(vc.available);
    const VcWidget *b = vc.widget(1);
    QVERIFY(b != nullptr);
    QCOMPARE(b->type, QStringLiteral("Button"));
    QCOMPARE(b->page, 2);
    QCOMPARE(b->function, quint32(7));
    QCOMPARE(b->hits, QList<quint32>() << 31 << 32);
    QCOMPARE(b->action, QStringLiteral("Toggle"));
    QCOMPARE(b->bindings.count(), 1);
    QCOMPARE(b->bindings.first().key, QStringLiteral("Ctrl+B"));
    QVERIFY(b->bindings.first().hasInput);
    QCOMPARE(b->bindings.first().channel, quint32(20));
    QVERIFY(vc.widget(2)->levelChannels.count() == 1 && vc.widget(2)->levelChannels.first().first == 4
            && vc.widget(2)->levelChannels.first().second == 2);
    QCOMPARE(vc.widget(2)->function, 0xFFFFFFFFu);      // only a Playback slider runs one
    QCOMPARE(vc.widget(3)->function, quint32(9));
    QCOMPARE(vc.widget(3)->bindings.first().role, QStringLiteral("Flash"));
    QCOMPARE(vc.widget(4)->group, quint32(6));
    QCOMPARE(vc.widget(5)->chaser, quint32(12));
    QCOMPARE(vc.widget(5)->bindings.first().role, QStringLiteral("Next"));
    QVERIFY(vc.widget(6)->follows);
    QCOMPARE(vc.widget(6)->follow, quint32(3));
    QVERIFY(vc.widget(7)->multipage);
    QCOMPARE(vc.widgets.at(vc.indexOf(8)).parent, vc.indexOf(7));
    QCOMPARE(vc.widget(10)->functions, QList<quint32>() << 14);
    QCOMPARE(vc.widget(11)->functions, QList<quint32>() << 15);
    QVERIFY(vc.functionsUsed().contains(7) && vc.functionsUsed().contains(9) && vc.functionsUsed().contains(12));
    QVERIFY(vc.functionsUsed().contains(14) && vc.functionsUsed().contains(15));
    QVERIFY(!vc.functionsUsed().contains(0xFFFFFFFFu));
    QVERIFY(vc.hitsFired().contains(31) && vc.hitsFired().contains(32));
    const VcWidget *props = vc.widget(VcIndex::propertiesId);
    QVERIFY(props != nullptr && props->bindings.first().channel == 9);
    // A file with no Virtual Console is an empty one; no source is none.
    QVERIFY(VcIndex::fromXml("<Workspace/>").available);
    QVERIFY(VcIndex::fromXml("<Workspace/>").widgets.isEmpty());
    QVERIFY(!m_doc->showReview()->vcIndex().available);
    // The same index from a source the app gives (its saved XML) and from the file.
    m_doc->showReview()->setVcXml(xml.toUtf8());
    QCOMPARE(m_doc->showReview()->vcIndex(), vc);
    QVERIFY(m_dir.isValid());
    const QString path = QDir(m_dir.path()).filePath("vc.qxw");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(xml.toUtf8());
    f.close();
    QCOMPARE(VcIndex::fromFile(path), vc);
    m_doc->showReview()->setVcFile(path);
    QCOMPARE(m_doc->showReview()->vcIndex(), vc);
    QVERIFY(!VcIndex::fromFile(path + ".nope").available);
}

void ShowReview_Test::vc03()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 pid = st->addProduction("Show");
    const quint32 hit = st->addLook(pid, "Confetti");
    QVERIFY(hit != 0xFFFFFFFFu);
    QVERIFY(quiet("VC-03"));                // no Virtual Console to ask: stays quiet
    console(QString());
    QVERIFY(fires("VC-03"));                // on no button
    QVERIFY(findings("VC-03").first().message.contains("Confetti"));
    console(button(1, "Confetti", "4294967295", QString(), QString(), "Toggle",
                   QString()).replace("<Function ID=\"4294967295\"/>",
                                      QStringLiteral("<Function ID=\"4294967295\" Hit=\"%1\"/>").arg(hit)));
    QVERIFY(quiet("VC-03"));                // a button fires it
}

void ShowReview_Test::vc04()
{
    Fixture *a = par("Par 1", 0);
    Scene *sc = new Scene(m_doc);
    sc->setName("Wash");
    m_doc->addFunction(sc);
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Cues");
    m_doc->addFunction(chaser);
    const QString id = QString::number(sc->id());
    console(button(1, "Haze", id));
    QVERIFY(quiet("VC-04"));
    console(button(1, "Haze on", "4294967295"));
    QVERIFY(fires("VC-04"));
    QVERIFY(findings("VC-04").first().message.contains("no function"));
    QCOMPARE(findings("VC-04").first().targets, QStringList() << QStringLiteral("vc:1"));
    console(button(1, "Haze on", "99"));
    QVERIFY(fires("VC-04"));
    QVERIFY(findings("VC-04").first().message.contains("deleted"));
    // Not every button runs a function.
    console(button(1, "Blackout", "4294967295", QString(), QString(), "Blackout"));
    QVERIFY(quiet("VC-04"));
    // A level slider on fixtures that are gone.
    const QString slider = QStringLiteral("<Slider Caption=\"Dim\" ID=\"2\"><SliderMode>Level</SliderMode>"
                                          "<Level><Channel Fixture=\"%1\">0</Channel></Level></Slider>");
    console(slider.arg(a->id()));
    QVERIFY(quiet("VC-04"));
    console(slider.arg(99));
    QVERIFY(fires("VC-04"));
    // A Cue List on a deleted chaser.
    const QString cues = QStringLiteral("<CueList Caption=\"Cues\" ID=\"3\"><Chaser>%1</Chaser></CueList>");
    console(cues.arg(chaser->id()));
    QVERIFY(quiet("VC-04"));
    console(cues.arg(77));
    QVERIFY(fires("VC-04"));
}

void ShowReview_Test::vc05()
{
    console(button(1, "Blackout", "1", "B") + button(2, "Blue stab", "2", "B"));
    QVERIFY(fires("VC-05"));
    const ReviewFinding f = findings("VC-05").first();
    QVERIFY(f.message.contains("Blackout") && f.message.contains("Blue stab"));
    QCOMPARE(f.targets, QStringList() << QStringLiteral("vc:1") << QStringLiteral("vc:2"));
    // Different keys.
    console(button(1, "Blackout", "1", "B") + button(2, "Blue stab", "2", "C"));
    QVERIFY(quiet("VC-05"));
    // The same key on different pages of one frame: never live together.
    const QString pages = QStringLiteral("<Frame Caption=\"Busk\" ID=\"9\"><Multipage PagesNum=\"2\" CurrentPage=\"0\"/>"
                                         "<Button Caption=\"A\" ID=\"1\" Page=\"0\"><Function ID=\"1\"/><Key>B</Key></Button>"
                                         "<Button Caption=\"B\" ID=\"2\" Page=\"%1\"><Function ID=\"2\"/><Key>B</Key></Button></Frame>");
    console(pages.arg(1));
    QVERIFY(quiet("VC-05"));
    console(pages.arg(0));                  // the same page
    QVERIFY(fires("VC-05"));
    // A key Operate answers to.
    console(button(1, "Go", "1", "Ctrl+Return"));
    QVERIFY(fires("VC-05"));
    QVERIFY(findings("VC-05").first().message.contains("Operate"));
    // Spelled differently, the same key.
    console(button(1, "A", "1", "ctrl+b") + button(2, "B", "2", "Ctrl+B"));
    QVERIFY(fires("VC-05"));
}

void ShowReview_Test::vc06()
{
    console(button(1, "Strobe", "1", QString(), inputOf(1, 35))
            + button(2, "Haze", "2", QString(), inputOf(1, 35)));
    QVERIFY(fires("VC-06"));
    const ReviewFinding f = findings("VC-06").first();
    QVERIFY(f.message.contains("Strobe") && f.message.contains("Haze"));
    QVERIFY(f.message.contains("universe 2, channel 36"));      // as the input editor counts
    // Another channel.
    console(button(1, "Strobe", "1", QString(), inputOf(1, 35))
            + button(2, "Haze", "2", QString(), inputOf(1, 36)));
    QVERIFY(quiet("VC-06"));
    // The same job twice is not two jobs.
    console(button(1, "Strobe", "1", QString(), inputOf(1, 35))
            + button(2, "Strobe too", "1", QString(), inputOf(1, 35)));
    QVERIFY(quiet("VC-06"));
}

void ShowReview_Test::vc08()
{
    ShowTreeController *st = m_doc->showTree();
    console(QStringLiteral("<CueRunner Caption=\"Main GO\" ID=\"1\"/>"));
    QVERIFY(fires("VC-08"));                // follows the current production: none
    QVERIFY(findings("VC-08").first().message.contains("no cue list"));
    const quint32 pid = st->addProduction("Show");
    const quint32 lid = st->addCueList(pid, "Main");
    st->addCue(lid, -1, "One");
    QVERIFY(quiet("VC-08"));
    console(QStringLiteral("<CueRunner Caption=\"Main GO\" ID=\"1\" Follow=\"%1\"/>").arg(lid));
    QVERIFY(quiet("VC-08"));
    console(QStringLiteral("<CueRunner Caption=\"Main GO\" ID=\"1\" Follow=\"%1\"/>").arg(lid + 500));
    QVERIFY(fires("VC-08"));
    QVERIFY(findings("VC-08").first().message.contains("deleted"));
}

void ShowReview_Test::cue13()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 pid = st->addProduction("Show");
    const quint32 main = st->addCueList(pid, "Main");
    const quint32 encore = st->addCueList(pid, "Encore");
    st->addCue(main, -1, "One");
    st->addCue(encore, -1, "Two");
    QVERIFY(st->mainList(pid) == main);
    QVERIFY(quiet("CUE-13"));               // no Virtual Console to ask
    console(QString());
    QVERIFY(fires("CUE-13"));
    const ReviewFinding f = findings("CUE-13").first();
    QVERIFY(f.message.contains("Encore"));
    QCOMPARE(f.targets, QStringList() << QStringLiteral("list:%1").arg(encore));
    // A Cue Runner following it, or the list as the main one.
    console(QStringLiteral("<CueRunner Caption=\"Encore GO\" ID=\"1\" Follow=\"%1\"/>").arg(encore));
    QVERIFY(quiet("CUE-13"));
    console(QString());
    QVERIFY(fires("CUE-13"));
    QVERIFY(st->setMainList(encore));
    QVERIFY(fires("CUE-13"));               // now Main is the one nothing plays
    QVERIFY(findings("CUE-13").first().message.contains("Main"));
    QVERIFY(st->setMainList(main));
    // A VC Cue List on its chaser.
    const quint32 chaser = st->cueList(encore)->chaser;
    if (chaser != 0xFFFFFFFFu)
    {
        console(QStringLiteral("<CueList Caption=\"Cues\" ID=\"2\"><Chaser>%1</Chaser></CueList>").arg(chaser));
        QVERIFY(quiet("CUE-13"));
    }
    // A list that follows timecode plays by itself.
    QVERIFY(st->setListFollowTimecode(encore, true));
    console(QString());
    QVERIFY(quiet("CUE-13"));
}

void ShowReview_Test::vc02()
{
    Fixture *a = par("Par 1", 0);
    FixtureGroup *g = group("All", { a });
    QLCPalette *red = colour(Qt::red), *blue = colour(Qt::blue);
    ShowTreeController *st = m_doc->showTree();
    const quint32 pid = st->addProduction("Show");
    const quint32 lid = st->addCueList(pid, "Main");
    const quint32 cue = st->addCue(lid, -1, "One");
    const quint32 h1 = st->addLook(pid, "Warm wash");
    const quint32 h2 = st->addLook(pid, "Solo spot");
    Target t;
    t.groups = { g->id() };
    Look look;
    look.key = KeyColour;
    look.palettes = { red->id() };
    QVERIFY(st->setLayerLook(h1, st->addLayer(h1, t), look));
    look.palettes = { blue->id() };
    QVERIFY(st->setLayerLook(h2, st->addLayer(h2, t), look));
    const QString stab = QStringLiteral("<Button Caption=\"Stab\" ID=\"1\"><Function ID=\"4294967295\" Hit=\"%1\">"
                                        "<Hit ID=\"%2\"/></Function><Action>Toggle</Action></Button>");
    console(stab.arg(h1).arg(h2));
    QVERIFY(quiet("VC-02"));                // the same place: VC-01's
    QVERIFY(st->setHitScope({ h2 }, HitScope::Cue, cue));
    QVERIFY(fires("VC-02"));
    const ReviewFinding f = findings("VC-02").first();
    QCOMPARE(f.severity, ReviewFinding::Note);
    QVERIFY(f.message.contains("Solo spot") && f.message.contains("Warm wash") && f.message.contains("Par 1"));
    // The same hit twice: nothing to choose between.
    console(stab.arg(h1).arg(h1));
    QVERIFY(quiet("VC-02"));
    // Narrower, but on other lights.
    Fixture *b = par("Par 2", 10);
    FixtureGroup *g2 = group("Back", { b });
    t.groups = { g2->id() };
    const quint32 h3 = st->addLook(pid, "Back wash");
    look.palettes = { blue->id() };
    QVERIFY(st->setLayerLook(h3, st->addLayer(h3, t), look));
    QVERIFY(st->setHitScope({ h3 }, HitScope::Cue, cue));
    console(stab.arg(h1).arg(h3));
    QVERIFY(quiet("VC-02"));
}

void ShowReview_Test::vc07()
{
    console(button(1, "Strobe", "1", QString(), inputOf(1, 35))
            + button(2, "Haze", "2", QString(), inputOf(1, 36)) + button(3, "Fog", "3", QString(), inputOf(0, 5)));
    ShowReview::Options o;
    o.outputsLoaded = false;
    o.universesWithInput = { 0, 1 };
    QVERIFY(quiet("VC-07", o));
    o.universesWithInput = { 0 };
    QVERIFY(fires("VC-07", o));
    const ReviewFinding f = findings("VC-07", o).first();
    QVERIFY2(f.message.contains("universe 2") && f.message.contains("Strobe"), qPrintable(f.message));
    QVERIFY(f.message.contains("2 widget"));
    QCOMPARE(f.targets, QStringList() << "vc:1" << "vc:2");
    // With the Doc's own I/O: its universes have no input patched.
    QVERIFY(fires("VC-07"));
    // No Virtual Console to ask.
    m_doc->showReview()->setVcXml(QByteArray());
}

void ShowReview_Test::vc09()
{
    Fixture *a = par("Par 1", 0);
    Scene *sc = new Scene(m_doc);
    sc->setName("Wash");
    sc->setValue(a->id(), 3, 200);
    m_doc->addFunction(sc);
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(sc->id(), 0, Function::infiniteSpeed(), 0));
    chaser->addStep(ChaserStep(sc->id(), 0, Function::infiniteSpeed(), 0));
    m_doc->showTree()->importInPlace(chaser->id());
    const QString cues = QStringLiteral("<CueList Caption=\"Act 1\" ID=\"2\"><Chaser>%1</Chaser></CueList>");
    console(QString());
    QVERIFY(quiet("VC-09"));
    console(cues.arg(chaser->id()));
    QVERIFY(fires("VC-09"));
    const ReviewFinding f = findings("VC-09").first();
    QVERIFY2(f.message.contains("Act 1") && f.message.contains("Full Show"), qPrintable(f.message));
    QVERIFY(f.targets.contains(QStringLiteral("vc:2")));
    QVERIFY(quiet("CUE-12"));               // the Cue List is VC-09's, not said twice
    console(cues.arg(chaser->id() + 500));  // another chaser
    QVERIFY(quiet("VC-09"));
}

void ShowReview_Test::cue15()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 pid = st->addProduction("Show");
    const quint32 lid = st->addCueList(pid, "Main");
    const quint32 cue = st->addCue(lid, -1, "Chorus");
    QVERIFY(st->addSubCue(cue, "Flash") != ShowTree::invalidId());
    const quint32 second = st->addSubCue(cue, "Blackout");
    QVERIFY(second != ShowTree::invalidId());
    st->setSubCueFollow(second, false, 0);       // on Sub GO, as a new sub-cue is
    console(QString());
    QVERIFY(fires("CUE-15"));
    const ReviewFinding f = findings("CUE-15").first();
    QCOMPARE(f.severity, ReviewFinding::Note);
    QVERIFY2(f.message.contains("Chorus") && f.message.contains("1 sub-cue"), qPrintable(f.message));
    // A Cue Runner shows Sub GO unless it is turned off.
    console(QStringLiteral("<CueRunner Caption=\"Main GO\" ID=\"1\"/>"));
    QVERIFY(quiet("CUE-15"));
    console(QStringLiteral("<CueRunner Caption=\"Main GO\" ID=\"1\" SubGo=\"0\"/>"));
    QVERIFY(fires("CUE-15"));
    console(QStringLiteral("<CueRunner Caption=\"Main GO\" ID=\"1\" SubGo=\"0\"><SubGo><Key>J</Key></SubGo></CueRunner>"));
    QVERIFY(quiet("CUE-15"));
    // The second sub-cue follows by itself: nothing waits for Sub GO.
    console(QString());
    QVERIFY(st->setSubCueFollow(second, true, 500));
    QVERIFY(quiet("CUE-15"));
}

/****************************************************************************
 * Venues and focus
 ****************************************************************************/

void ShowReview_Test::ven01()
{
    Fixture *a = par("Par 1", 0);
    FixtureGroup *g = group("Spots", { a });
    FocusCalls *calls = m_doc->programmer()->focusCalls();
    QVERIFY(quiet("VEN-01"));               // no calls
    StageTarget *dsc = m_doc->monitorProperties()->addStageTarget();
    dsc->setName("DSC");
    FocusCall call;
    call.groups = { g->id() };
    calls->append(call);
    QVERIFY(fires("VEN-01"));               // no target
    QVERIFY(findings("VEN-01").first().message.contains("no target"));
    call.target.kind = FocusSession::Reference::Target;
    call.target.id = dsc->id() + 40;
    QVERIFY(calls->replace(0, call));
    QVERIFY(fires("VEN-01"));               // a target that was deleted
    QVERIFY(findings("VEN-01").first().message.contains("deleted"));
    call.target.id = dsc->id();
    QVERIFY(calls->replace(0, call));
    QVERIFY(quiet("VEN-01"));
    call.groups = { g->id() + 40 };
    QVERIFY(calls->replace(0, call));
    QVERIFY(fires("VEN-01"));               // its group is gone
}

void ShowReview_Test::ven02()
{
    Fixture *a = par("Par 1", 0);
    QLCFixtureDef *def = new QLCFixtureDef();
    QLCChannel *fan = chan(def, "Fan", QLCChannel::Custom);
    fan->setGroup(QLCChannel::Nothing);     // nothing that makes light
    Fixture *haze = make(m_doc, "Hazer", "Haze", QLCFixtureDef::Smoke, def, { fan }, 500, 0, 40);
    QVERIFY(haze != nullptr);
    FixtureGroup *g = group("Rig", { a, haze });
    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setFixtureProperties(a->id(), FixturePreviewItem());
    mp->setFixtureProperties(haze->id(), FixturePreviewItem());
    StageTarget *dsc = mp->addStageTarget();
    FocusCall call;
    call.groups = { g->id() };
    call.target.kind = FocusSession::Reference::Target;
    call.target.id = dsc->id();
    m_doc->programmer()->focusCalls()->append(call);
    QVERIFY(fires("VEN-02"));               // a hazer can't be aimed
    const ReviewFinding f = findings("VEN-02").first();
    QVERIFY2(f.message.contains("Hazer") && !f.message.contains("Par 1"), qPrintable(f.message));
    // A light off the plot is FIX-05's to say.
    mp->setFixtureProperties(a->id(), FixturePreviewItem());
    FixtureGroup *g2 = group("Lonely", { a });
    call.groups = { g2->id() };
    QVERIFY(m_doc->programmer()->focusCalls()->replace(0, call));
    QVERIFY(quiet("VEN-02"));               // a par on the plot: a fixed light Quick focus places
    QVERIFY(m_doc->programmer()->focusCalls()->remove(0));
    QVERIFY(quiet("VEN-02"));
}

void ShowReview_Test::ven05()
{
    Fixture *a = par("Par 1", 0);
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    QVERIFY(m_dir.isValid());
    v->setShowFile(QDir(m_dir.path()).filePath("Tour.qlcc"), false);
    const QString venue = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(quiet("VEN-05"));               // no venue applied
    QVERIFY(v->setActiveVenue(venue));
    MonitorProperties *mp = m_doc->monitorProperties();
    FixtureRigProps rp = mp->fixtureRigProps(a->id());
    rp.panOffsetDeg = 3.0f;
    mp->setFixtureRigProps(a->id(), rp);
    QVERIFY(v->record(a->id()));
    QVERIFY(v->hasCorrection(a->id()));
    QVERIFY(quiet("VEN-05"));               // still the light that was focused
    const_cast<QLCFixtureDef *>(a->fixtureDef())->setModel(QStringLiteral("ADJ Focus Spot"));
    QVERIFY(fires("VEN-05"));
    const ReviewFinding f = findings("VEN-05").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QVERIFY2(f.message.contains("Par 1") && f.message.contains("Lincoln HS") && f.message.contains("ADJ Focus Spot"),
             qPrintable(f.message));
    QVERIFY(v->setActiveVenue(QString()));
    QVERIFY(quiet("VEN-05"));
}

void ShowReview_Test::ven09()
{
    par("Par 1", 0);
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    QVERIFY(m_dir.isValid());
    QVERIFY(quiet("VEN-09"));               // the show isn't saved: no file to miss
    v->setShowFile(QDir(m_dir.path()).filePath("Tour.qlcc"), false);
    QVERIFY(quiet("VEN-09"));               // no venues, no file, nothing needs it
    v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY2(QFileInfo::exists(v->filePath()), "a venue is written the moment it is made");
    QVERIFY(quiet("VEN-09"));
    QVERIFY(QFile::remove(v->filePath()));
    QVERIFY(fires("VEN-09"));
    QVERIFY(findings("VEN-09").first().message.contains("Tour.venues.xml"));
    QVERIFY(v->save());
    QVERIFY(quiet("VEN-09"));
    QFile f(v->filePath());
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("<Venues Show=\"Tour.qlcc\"><Venue Name=\"x\"");
    f.close();
    QVERIFY(fires("VEN-09"));
    QVERIFY(findings("VEN-09").first().message.contains("can't be read"));
}

void ShowReview_Test::ven10()
{
    Fixture *m = mover("Spot 1", 0);
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    QVERIFY(m_dir.isValid());
    v->setShowFile(QDir(m_dir.path()).filePath("Tour.qlcc"), false);
    QVERIFY(quiet("VEN-10"));
    QVERIFY(v->setActiveVenue(v->addVenue(QStringLiteral("Lincoln HS"))));
    m_doc->monitorProperties()->setFixtureProperties(m->id(), FixturePreviewItem());
    m_doc->teachingSettings()->setEnabled(true);
    QString why;
    const QString practice = v->startPractice({ m->id() }, 7, 2.0f, 8.0f, &why);
    QVERIFY2(!practice.isEmpty(), qPrintable(why));
    QVERIFY(fires("VEN-10"));
    const ReviewFinding f = findings("VEN-10").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QVERIFY(f.message.contains(practice));
    QVERIFY(v->endPractice());
    QVERIFY(quiet("VEN-10"));               // ended, and a real venue exists
    QVERIFY(v->removeVenue(QStringLiteral("Lincoln HS")));
    QVERIFY(fires("VEN-10"));               // a practice left, and no real venue
    QCOMPARE(findings("VEN-10").first().severity, ReviewFinding::Note);
}

void ShowReview_Test::cue12Vc()
{
    Fixture *a = par("Par 1", 0);
    Scene *sc = new Scene(m_doc);
    sc->setName("Wash");
    sc->setValue(a->id(), 3, 200);
    m_doc->addFunction(sc);
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(sc->id(), 0, Function::infiniteSpeed(), 0));
    chaser->addStep(ChaserStep(sc->id(), 0, Function::infiniteSpeed(), 0));
    m_doc->showTree()->importInPlace(chaser->id());
    console(QString());
    QVERIFY(quiet("CUE-12"));
    console(button(1, "Act 1", QString::number(chaser->id())));
    QVERIFY(fires("CUE-12"));
    const ReviewFinding f = findings("CUE-12").first();
    QVERIFY2(f.message.contains("VC button 'Act 1'") && f.message.contains("Full Show"), qPrintable(f.message));
    QVERIFY(f.targets.contains(QStringLiteral("vc:1")));
    console(button(1, "Act 1", QString::number(sc->id())));     // another function
    QVERIFY(quiet("CUE-12"));
}

void ShowReview_Test::acceptanceRoundTrip()
{
    Fixture *a = par("Par 1", 0);
    Fixture *b = par("Par 2", 10);
    Fixture *c = par("Par 3", 20);
    ShowReview *sr = m_doc->showReview();

    // Accepted, saved with the show and read back: still accepted, who and why kept.
    b->setName("Par 1");
    QList<ReviewFinding> f = findings("FIX-14");
    QCOMPARE(f.count(), 1);
    QVERIFY(sr->accept(f.first(), "Both called Par 1 on the plot", "Ava R."));
    QVERIFY(m_doc->isModified());
    QCOMPARE(findings("FIX-14").first().state, ReviewFinding::Accepted);
    QCOMPARE(sr->lastResult().open[ReviewFinding::Note], 0);
    QCOMPARE(sr->lastResult().accepted[ReviewFinding::Note], 1);
    const QString xml = saveShow();
    QVERIFY(xml.contains("<ShowReview>"));
    QVERIFY(xml.contains("Ava R."));
    Doc other(this);
    QVERIFY(loadShow(&other, xml));
    QCOMPARE(other.showReview()->acceptances().count(), 1);
    ShowReview::Options o;
    o.checks = QStringList() << "FIX-14";
    const QList<ReviewFinding> back = other.showReview()->run(o).findings;
    QCOMPARE(back.count(), 1);
    QCOMPARE(back.first().state, ReviewFinding::Accepted);
    QCOMPARE(back.first().acceptedNote, QStringLiteral("Both called Par 1 on the plot"));
    QCOMPARE(back.first().acceptedBy, QStringLiteral("Ava R."));

    // An accepted overlap: something it didn't read changes, still accepted;
    // what it read changes, open again with the old note.
    b->setAddress(2);
    f = findings("PAT-01");
    QCOMPARE(f.count(), 1);
    QVERIFY(sr->accept(f.first(), "A doubled par, on purpose", "Ava R."));
    QCOMPARE(findings("PAT-01").first().state, ReviewFinding::Accepted);
    c->setAddress(40);
    QCOMPARE(findings("PAT-01").first().state, ReviewFinding::Accepted);
    b->setAddress(1);
    f = findings("PAT-01");
    QCOMPARE(f.first().state, ReviewFinding::Changed);
    QCOMPARE(f.first().acceptedNote, QStringLiteral("A doubled par, on purpose"));
    QCOMPARE(sr->lastResult().open[ReviewFinding::Blocker], 1);
    QCOMPARE(sr->lastResult().changed, 1);

    // The subject gone: its acceptances are dropped at the next save.
    QVERIFY(m_doc->deleteFixture(b->id()));
    QVERIFY(!saveShow().contains("<ShowReview"));
    Q_UNUSED(a)
}

void ShowReview_Test::acceptRules()
{
    par("Par 1", 0);
    Fixture *b = par("Par 2", 10);
    b->setName("Par 1");
    const ReviewFinding f = findings("FIX-14").first();
    ShowReview *sr = m_doc->showReview();
    QVERIFY(!sr->accept(f, "   ", "Ava"));                 // a note is required
    QVERIFY(sr->accept(f, "Renamed at the venue", ""));     // by: this computer's name
    QVERIFY(!sr->acceptances().first().by.isEmpty() || ShowReview::defaultName().isEmpty());
    QCOMPARE(findings("FIX-14").first().state, ReviewFinding::Accepted);
    QVERIFY(sr->reopen(f));
    QCOMPARE(findings("FIX-14").first().state, ReviewFinding::Open);
    QVERIFY(!sr->reopen(f));

    QVERIFY(!sr->turnOff("FIX-14", "", "Ava"));
    QVERIFY(sr->turnOff("FIX-14", "Names don't matter in this rig", "Ava"));
    QVERIFY(sr->isOff("FIX-14"));
    ReviewFinding off = findings("FIX-14").first();
    QCOMPARE(off.state, ReviewFinding::Accepted);
    QVERIFY(off.checkOff);
    QCOMPARE(off.acceptedNote, QStringLiteral("Names don't matter in this rig"));
    QVERIFY(saveShow().contains("<Off Check=\"FIX-14\""));
    QVERIFY(sr->turnOn("FIX-14"));
    QCOMPARE(findings("FIX-14").first().state, ReviewFinding::Open);
}

void ShowReview_Test::xmlIsQuietAndKeepsTheUnknown()
{
    par("Par 1", 0);
    QVERIFY(!saveShow().contains("ShowReview"));            // nothing accepted: nothing written
    const QString xml = saveShow().replace("</Engine>",
        "<ShowReview PlaysOn=\"pi5\"><Accepted Check=\"FIX-14\" Target=\"fx:0\" Hash=\"abc\" By=\"B\" "
        "When=\"2026-10-31T10:00:00\" Future=\"x\">Kept</Accepted><Mystery/></ShowReview></Engine>");
    Doc other(this);
    QVERIFY(loadShow(&other, xml));
    QCOMPARE(other.showReview()->acceptances().count(), 1);
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    other.showReview()->saveXML(&w);
    const QString again = QString::fromUtf8(buf.data());
    QVERIFY(again.contains("PlaysOn=\"pi5\""));
    QVERIFY(again.contains("Future=\"x\""));
    QVERIFY(again.contains(">Kept<"));
    other.clearContents();
    QVERIFY(other.showReview()->acceptances().isEmpty());
}

void ShowReview_Test::fingerprintsAreStable()
{
    Fixture *a = par("Par 1", 0), *b = par("Par 2", 10);
    b->setAddress(2);
    b->setName("Par 1");
    Q_UNUSED(a)
    const QString xml = saveShow();
    auto prints = [this](Doc *doc) {
        QStringList out;
        for (const ReviewFinding &f : doc->showReview()->run().findings)
            out << f.key() + QLatin1Char('=') + f.hash();
        return out;
    };
    Doc one(this), two(this);
    QVERIFY(loadShow(&one, xml));
    QVERIFY(loadShow(&two, xml));
    QVERIFY(!prints(&one).isEmpty());
    QCOMPARE(prints(&one), prints(&two));
}

void ShowReview_Test::footerFoldsUnderCatalogIds()
{
    QCOMPARE(ShowReview::footerCheckId("fixtures.clash"), QStringLiteral("PAT-01"));
    QCOMPARE(ShowReview::footerCheckId("fixtures.unpatched"), QStringLiteral("PAT-03"));
    QCOMPARE(ShowReview::footerCheckId("fixtures.nodefinition"), QStringLiteral("FIX-01"));
    QCOMPARE(ShowReview::footerCheckId("universes.nooutput"), QStringLiteral("PAT-06"));
    QCOMPARE(ShowReview::footerCheckId("effects.failed"), QStringLiteral("FX-07"));
    // Runtime: the footer's only.
    QVERIFY(ShowReview::footerCheckId("output.dangling").isEmpty());
    QVERIFY(ShowReview::footerCheckId("input.devices").isEmpty());
    QVERIFY(ShowReview::footerCheckId("simpledesk.held").isEmpty());
    // Every id a footer entry maps to that is built is in the catalog once.
    QSet<QString> ids;
    for (const ShowReview::Check &c : ShowReview::catalog())
    {
        QVERIFY2(!ids.contains(c.id), qPrintable(c.id));
        ids.insert(c.id);
        QVERIFY(!c.title.isEmpty() && !c.why.isEmpty());
    }
}

void ShowReview_Test::cli()
{
    QVERIFY(m_dir.isValid());
    // A clean show, then one with a blocker.
    par("Par 1", 0);
    Fixture *b = par("Par 2", 10);
    const QString clean = QDir(m_dir.path()).filePath("clean.qxw");
    {
        QFile f(clean);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(saveShow().toUtf8());
    }
    b->setAddress(2);
    const QString broken = QDir(m_dir.path()).filePath("broken.qxw");
    {
        QFile f(broken);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(saveShow().toUtf8());
    }
    const QByteArray before = [&]() { QFile f(broken); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }();
    const QDateTime mtime = QFileInfo(broken).lastModified();

    QString text, errors;
    QTextStream out(&text), err(&errors);
    // The test shows don't use the plot, power or the Show Tree: only the patch speaks.
    QCOMPARE(ShowReviewCli::run({ "--review", clean, "--check", "PAT-*" }, out, err, false), int(ShowReviewCli::Clean));
    QVERIFY(text.contains("0 blocker(s)"));
    text.clear();
    QCOMPARE(ShowReviewCli::run({ "--review", broken, "--check", "PAT-*" }, out, err, false), int(ShowReviewCli::Blockers));
    QVERIFY(text.contains("PAT-01"));
    QVERIFY(text.contains("Blockers"));
    text.clear();
    QCOMPARE(ShowReviewCli::run({ "--review", broken, "--format", "json", "--check", "PAT-01" }, out, err, false),
             int(ShowReviewCli::Blockers));
    const QJsonObject json = QJsonDocument::fromJson(text.toUtf8()).object();
    QCOMPARE(json.value("open").toObject().value("blockers").toInt(), 1);
    QCOMPARE(json.value("findings").toArray().first().toObject().value("check").toString(), QStringLiteral("PAT-01"));
    text.clear();
    QCOMPARE(ShowReviewCli::run({ "--review", QDir(m_dir.path()).filePath("nope.qxw") }, out, err, false),
             int(ShowReviewCli::Unreadable));
    QCOMPARE(ShowReviewCli::run({ "--review", broken, "--bogus" }, out, err, false), int(ShowReviewCli::Unreadable));
    QCOMPARE(ShowReviewCli::run({ "--review", broken, "--min", "loud" }, out, err, false), int(ShowReviewCli::Unreadable));
    // Never writes the show.
    const QByteArray after = [&]() { QFile f(broken); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }();
    QCOMPARE(after, before);
    QCOMPARE(QFileInfo(broken).lastModified(), mtime);

    // The Virtual Console is read from the file, without the UI: a button with no function.
    const QString withVc = QDir(m_dir.path()).filePath("vc.qxw");
    {
        QString xml = saveShow();
        xml.replace(QStringLiteral("</Workspace>"),
                    QStringLiteral("<VirtualConsole><Frame Caption=\"\">") + button(1, "Haze on", "4294967295")
                    + QStringLiteral("</Frame></VirtualConsole></Workspace>"));
        QFile f(withVc);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(xml.toUtf8());
    }
    text.clear();
    QCOMPARE(ShowReviewCli::run({ "--review", withVc, "--check", "VC-*" }, out, err, false), int(ShowReviewCli::Warnings));
    QVERIFY2(text.contains("VC-04") && text.contains("Haze on"), qPrintable(text));
    text.clear();
    QCOMPARE(ShowReviewCli::run({ "--review", broken, "--check", "VC-*" }, out, err, false), int(ShowReviewCli::Clean));

    // A button listening to an input the file doesn't patch (VC-07), and a
    // venue file beside the show that can't be read (VEN-09).
    const QString withInput = QDir(m_dir.path()).filePath("input.qxw");
    {
        QString xml = saveShow();
        xml.replace(QStringLiteral("</Workspace>"),
                    QStringLiteral("<VirtualConsole><Frame Caption=\"\">") + button(1, "Strobe", "1", QString(), inputOf(2, 3))
                    + QStringLiteral("</Frame></VirtualConsole></Workspace>"));
        QFile f(withInput);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(xml.toUtf8());
    }
    text.clear();
    QCOMPARE(ShowReviewCli::run({ "--review", withInput, "--check", "VC-07" }, out, err, false), int(ShowReviewCli::Warnings));
    QVERIFY2(text.contains("universe 3"), qPrintable(text));
    text.clear();
    QCOMPARE(ShowReviewCli::run({ "--review", withInput, "--check", "VEN-*" }, out, err, false), int(ShowReviewCli::Clean));
    {
        QFile f(VenueCorrections::sidecarPath(withInput));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("<Venues Show=\"input.qxw\"><Venue");
    }
    text.clear();
    QCOMPARE(ShowReviewCli::run({ "--review", withInput, "--check", "VEN-*" }, out, err, false), int(ShowReviewCli::Warnings));
    QVERIFY2(text.contains("VEN-09"), qPrintable(text));

    char prog[] = "qlcconsole", rev[] = "--review", file[] = "x.qxw", other[] = "-o";
    char *yes[] = { prog, rev, file };
    char *no[] = { prog, other, file };
    QVERIFY(ShowReviewCli::wanted(3, yes));
    QVERIFY(!ShowReviewCli::wanted(3, no));
}

/****************************************************************************
 * Movement (MOV) and the move speed they use
 ****************************************************************************/

namespace
{
/** A cue with one mover, lit at @a dim, at @a pan / @a tilt degrees. */
void cueHead(Doc *doc, quint32 cue, Fixture *f, int panDeg, int tiltDeg, int dim)
{
    ShowTreeController *st = doc->showTree();
    QLCPalette *pos = new QLCPalette(QLCPalette::PanTilt, doc);
    pos->setValues(QVariantList() << panDeg << tiltDeg);
    pos->setName(QStringLiteral("Pos %1/%2").arg(panDeg).arg(tiltDeg));
    doc->addPalette(pos);
    QLCPalette *level = new QLCPalette(QLCPalette::Dimmer, doc);
    level->setValue(dim);
    level->setName(QStringLiteral("Dim %1").arg(dim));
    doc->addPalette(level);
    Target t;
    t.fixtures = { f->id() };
    const quint32 layer = st->addLayer(cue, t);
    Look look;
    look.key = KeyPosition;
    look.palettes = { pos->id() };
    QVERIFY(st->setLayerLook(cue, layer, look));
    Look d;
    d.key = KeyDimmer;
    d.palettes = { level->id() };
    QVERIFY(st->setLayerLook(cue, layer, d));
}
}

void ShowReview_Test::moveSpeed()
{
    Fixture *spot = mover("Spot 1", 0);
    QCOMPARE(MoveSpeed::secondsPer180(spot), 0.4);          // decided: a typical spot
    QCOMPARE(MoveSpeed::degreesPerSecond(spot), 450.0);
    QCOMPARE(MoveSpeed::panSpanDeg(spot), 540.0);
    QCOMPARE(MoveSpeed::tiltSpanDeg(spot), 270.0);
    // Half of a 540 degree pan is 270 degrees: 0.6 s. Tilt 270 degrees: 0.6 s too.
    QVERIFY(qAbs(MoveSpeed::travelMs(spot, 0, 0, 0.5, 0) - 600.0) < 0.001);
    QVERIFY(qAbs(MoveSpeed::travelMs(spot, 0, 0, 0, 1) - 600.0) < 0.001);
    // Pan and tilt run together: the slower axis, not the sum.
    QVERIFY(qAbs(MoveSpeed::travelMs(spot, 0, 0, 1, 1) - 1200.0) < 0.001);
    QCOMPARE(MoveSpeed::travelMs(spot, 0.3, 0.3, 0.3, 0.3), 0.0);
    QCOMPARE(MoveSpeed::travelMs(nullptr, 0, 0, 1, 1), 0.0);
    // A scanner's mirror is four times quicker.
    QLCFixtureDef *def = new QLCFixtureDef();
    Fixture *scan = make(m_doc, "Scan 1", QStringLiteral("Scan"), QLCFixtureDef::Scanner, def,
                         { chan(def, "Pan", QLCChannel::PositionPan), chan(def, "Tilt", QLCChannel::PositionTilt) },
                         50, 0, 20);
    QVERIFY(scan != nullptr);
    QCOMPARE(MoveSpeed::secondsPer180(scan), 0.1);
    // A follower never outruns the head: 100 ms is 45 degrees of a 540 degree pan.
    MoveSpeed::Follower f;
    QVERIFY(!f.step(spot, 1.0, 0.0, 1000));                 // first sight: it is there
    QCOMPARE(f.pan, 1.0);
    f.step(spot, 0.0, 0.0, 1100);
    QVERIFY(qAbs(f.pan - (1.0 - 45.0 / 540.0)) < 1e-9);
    QVERIFY(f.step(spot, 0.0, 0.0, 1200));                  // still going
    QVERIFY(!f.step(spot, 0.0, 0.0, 2300));                 // 450 degrees more: arrives, and says so
    QCOMPARE(f.pan, 0.0);
    // Looking away for a while (the view hidden) snaps rather than crawls.
    f.step(spot, 1.0, 1.0, 20000);
    QCOMPARE(f.pan, 1.0);
}

/* A head that points within DMX 0-127 and spins above it (RotationIndexed):
   its travel is measured along that band, not the whole channel -- the same
   band the rig views read (Fixture::positionBand()). */
void ShowReview_Test::moveSpeedUsesThePointingBand()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    QLCChannel *pan = chan(def, "Pan", QLCChannel::PositionPan);
    pan->addCapability(new QLCCapability(0, 127, "Indexed position"));
    pan->capabilities().first()->setPreset(QLCCapability::RotationIndexed);
    pan->addCapability(new QLCCapability(128, 255, "Rotation"));
    Fixture *ust = make(m_doc, "UST 1", QStringLiteral("UST"), QLCFixtureDef::MovingHead, def,
                        { pan, chan(def, "Tilt", QLCChannel::PositionTilt) }, 100, 0, 40);
    QVERIFY(ust != nullptr);
    // DMX 0 to 127 (of 255) is the whole 540 degrees: 1.2 s, not half of it.
    QVERIFY(qAbs(MoveSpeed::travelMs(ust, 0, 0, 127.0 / 255.0, 0) - 1200.0) < 0.001);
    QVERIFY(qAbs(MoveSpeed::travelMs(ust, 0, 0, 63.5 / 255.0, 0) - 600.0) < 0.001);
    // Into the spin zone it is clamped at the band's end.
    QVERIFY(qAbs(MoveSpeed::travelMs(ust, 0, 0, 1.0, 0) - 1200.0) < 0.001);
    // Tilt has no band: unchanged.
    QVERIFY(qAbs(MoveSpeed::travelMs(ust, 0, 0, 0, 1) - 600.0) < 0.001);
    // The follower crosses the band at the head's speed: 100 ms is 45 of 540 degrees.
    MoveSpeed::Follower f;
    f.step(ust, 0.0, 0.0, 1000);
    f.step(ust, 127.0 / 255.0, 0.0, 1100);
    QVERIFY(qAbs(f.pan - (45.0 / 540.0) * (127.0 / 255.0)) < 1e-9);
    QVERIFY(f.step(ust, 127.0 / 255.0, 0.0, 1200));
    QVERIFY(!f.step(ust, 127.0 / 255.0, 0.0, 2300));
    QCOMPARE(f.pan, 127.0 / 255.0);
}

void ShowReview_Test::mov01()
{
    Fixture *a = mover("Spot 1", 0);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    const quint32 c2 = st->addCue(lid, 0, "Two");
    cueHead(m_doc, c1, a, 100, 90, 200);
    cueHead(m_doc, c2, a, 100, 90, 200);
    QVERIFY(quiet("MOV-01"));                               // same place: nothing moves
    st->removeLayer(c2, st->cueList(lid)->cue(c2)->layers.last().id);
    cueHead(m_doc, c2, a, 400, 90, 200);
    QVERIFY(fires("MOV-01"));                               // 300 degrees, lit in both
    QVERIFY(findings("MOV-01").first().message.contains("Spot 1"));
    // Dark before: it moves in the dark, which is MOV-02's, not this.
    st->removeLayer(c1, st->cueList(lid)->cue(c1)->layers.last().id);
    cueHead(m_doc, c1, a, 100, 90, 0);
    QVERIFY(quiet("MOV-01"));
}

void ShowReview_Test::mov02()
{
    Fixture *a = mover("Spot 1", 0);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    const quint32 c2 = st->addCue(lid, 0, "Two");
    cueHead(m_doc, c1, a, 100, 90, 0);
    cueHead(m_doc, c2, a, 400, 90, 200);
    auto setHold = [&](uint hold) {
        Cue cue = *st->cueList(lid)->cue(c1);
        cue.hold = hold;
        QVERIFY(st->setCue(cue));
    };
    // Move in black off for the show (the default): it can't hide the move.
    setHold(5000);
    QVERIFY(fires("MOV-02"));
    QVERIFY(findings("MOV-02").first().message.contains("off for the show"));
    m_doc->programmer()->setAutoMoveInBlack(true);
    QVERIFY(quiet("MOV-02"));                               // 5 s of dark, enough
    setHold(Function::infiniteSpeed());
    QVERIFY(quiet("MOV-02"));                               // waits for GO: plenty
    setHold(300);                                           // a 0.3 s dark gap
    QVERIFY(fires("MOV-02"));
    QVERIFY(findings("MOV-02").first().message.contains("0.3"));
    // Longer than the lead time, shorter than the head needs to travel
    // (300 degrees of a 540 degree pan: 0.67 s -- under the 0.8 s lead time).
    setHold(900);
    QVERIFY(quiet("MOV-02"));
    // Over the full pan (about 1 s of travel from 100 degrees) the same gap is too short.
    st->removeLayer(c2, st->cueList(lid)->cue(c2)->layers.last().id);
    cueHead(m_doc, c2, a, 540, 90, 200);
    QVERIFY(fires("MOV-02"));
    QVERIFY(findings("MOV-02").first().message.contains("1.0"));
    // "Don't move in black into this cue" on the cue.
    Cue cue = *st->cueList(lid)->cue(c2);
    cue.noMoveInBlack = true;
    QVERIFY(st->setCue(cue));
    QVERIFY(fires("MOV-02"));
    QVERIFY(findings("MOV-02").first().message.contains("not to move in black"));
}

void ShowReview_Test::mov08()
{
    Fixture *a = mover("Spot 1", 0);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "Opener");
    cueHead(m_doc, c1, a, 270, 135, 200);                   // the middle of both ranges: home
    QVERIFY(quiet("MOV-08"));
    st->removeLayer(c1, st->cueList(lid)->cue(c1)->layers.last().id);
    cueHead(m_doc, c1, a, 60, 40, 200);                     // lit, away from home, nothing before
    QVERIFY(fires("MOV-08"));
    QVERIFY(findings("MOV-08").first().message.contains("Spot 1"));
    // Dark in the opener (a preshow would be the thing): nothing to swing in lit.
    st->removeLayer(c1, st->cueList(lid)->cue(c1)->layers.last().id);
    cueHead(m_doc, c1, a, 60, 40, 0);
    QVERIFY(quiet("MOV-08"));
}

void ShowReview_Test::mov13()
{
    /* Two spots hung 6 m up over a 4 x 2 m area, aimed at it in a cue: each
       its own half (AREA_COVER.md). Narrow, they leave most of it dark. */
    MonitorProperties *mp = m_doc->monitorProperties();
    auto spot = [&](const QString &name, quint32 address, double x, double lens) {
        QLCFixtureDef *def = new QLCFixtureDef();
        Fixture *fx = make(m_doc, name, QStringLiteral("Spot ") + name, QLCFixtureDef::MovingHead, def,
                           { chan(def, "Pan", QLCChannel::PositionPan), chan(def, "Tilt", QLCChannel::PositionTilt),
                             chan(def, "Dimmer", QLCChannel::IntensityMasterDimmer) },
                           200, 0, address, QLCPhysical::BeamEmission, lens);
        mp->setFixturePosition(fx->id(), 0, 0, QVector3D(float(x * 1000.0), 4000.0f, 6.0f));
        FixtureRigProps rp = mp->fixtureRigProps(fx->id());
        rp.mountingType = Truss::TopHung;
        mp->setFixtureRigProps(fx->id(), rp);
        return fx;
    };
    QList<Fixture *> narrow { spot("N1", 0, 1.0, 8.0), spot("N2", 10, 3.0, 8.0) };
    StageTarget *area = mp->addStageTarget();
    area->setName(QStringLiteral("Chorus"));
    area->setKind(StageTarget::Kind::Area);
    area->setPosition(QVector3D(2.0f, 4.0f, 0.0f));
    area->setFootprint(QSizeF(4.0, 2.0));
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, m_doc);
    aim->setName(QStringLiteral("Chorus"));
    aim->setStageTargetId(area->id());
    QVERIFY(m_doc->addPalette(aim));
    QLCPalette *level = new QLCPalette(QLCPalette::Dimmer, m_doc);
    level->setValue(255);
    QVERIFY(m_doc->addPalette(level));

    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "Chorus up");
    auto aimAt = [&](const QList<Fixture *> &who) {
        Target t;
        for (Fixture *fx : who)
            t.fixtures << fx->id();
        const quint32 layer = st->addLayer(c1, t);
        Look look;
        look.key = KeyPosition;
        look.palettes = { aim->id() };
        QVERIFY(st->setLayerLook(c1, layer, look));
        Look d;
        d.key = KeyDimmer;
        d.palettes = { level->id() };
        QVERIFY(st->setLayerLook(c1, layer, d));
    };
    aimAt(narrow);
    QVERIFY(fires("MOV-13"));
    const ReviewFinding f = findings("MOV-13").first();
    QCOMPARE(f.severity, ReviewFinding::Note);
    QVERIFY2(f.message.contains("Chorus") && f.message.contains("lights only")
             && f.message.contains("2 light")
             && f.message.contains("on the deck") && f.message.contains("Zoom them wider"), qPrintable(f.message));
    QVERIFY(f.targets.contains(QStringLiteral("target:%1").arg(area->id())));

    // Wide enough to light it all: nothing to say.
    st->removeLayer(c1, st->cueList(lid)->cue(c1)->layers.last().id);
    aimAt({ spot("W1", 20, 1.0, 60.0), spot("W2", 30, 3.0, 60.0) });
    QVERIFY(quiet("MOV-13"));

    // An area of no size is a point: no cover to speak of.
    st->removeLayer(c1, st->cueList(lid)->cue(c1)->layers.last().id);
    aimAt(narrow);
    area->setFootprint(QSizeF());
    QVERIFY(quiet("MOV-13"));
}

namespace
{
/** A cue layer lighting fixture @a f at @a dim (no position). */
void cueLit(Doc *doc, quint32 cue, Fixture *f, int dim)
{
    ShowTreeController *st = doc->showTree();
    QLCPalette *level = new QLCPalette(QLCPalette::Dimmer, doc);
    level->setValue(dim);
    level->setName(QStringLiteral("Dim %1").arg(dim));
    doc->addPalette(level);
    Target t;
    t.fixtures = { f->id() };
    const quint32 layer = st->addLayer(cue, t);
    Look d;
    d.key = KeyDimmer;
    d.palettes = { level->id() };
    QVERIFY(st->setLayerLook(cue, layer, d));
}
}

void ShowReview_Test::zonesSaveAndLoad()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    const quint32 a = mp->addNoLightZone(2.0f, 3.0f);
    NoLightZone z = mp->noLightZone(a);
    z.name = QStringLiteral("Orchestra pit");
    z.width = 8.0f;
    z.depth = 3.5f;
    z.floorZ = -1.8f;
    z.height = 4.0f;
    z.kind = NoLightZone::NoLasers;
    mp->setNoLightZone(z);
    const quint32 b = mp->addNoLightZone();
    QCOMPARE(mp->noLightZones().count(), 2);
    QVERIFY(a != b);

    QByteArray xml;
    {
        QBuffer buf(&xml);
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        w.writeStartDocument();
        QVERIFY(mp->saveXML(&w, m_doc));
        w.writeEndDocument();
    }
    MonitorProperties other;
    QXmlStreamReader r(xml);
    QVERIFY(r.readNextStartElement());
    QVERIFY(other.loadXML(r, m_doc));
    QCOMPARE(other.noLightZones().count(), 2);
    const NoLightZone back = other.noLightZone(a);
    QCOMPARE(back.name, QStringLiteral("Orchestra pit"));
    QCOMPARE(back.kind, int(NoLightZone::NoLasers));
    QCOMPARE(back.width, 8.0f);
    QCOMPARE(back.depth, 3.5f);
    QCOMPARE(back.floorZ, -1.8f);
    QCOMPARE(back.height, 4.0f);
    QCOMPARE(back.x, 2.0f);
    QCOMPARE(back.y, 3.0f);
    // An undo snapshot keeps them too, and removal is undone with it.
    const QByteArray snap = mp->snapshot(m_doc);
    mp->removeNoLightZone(a);
    QCOMPARE(mp->noLightZones().count(), 1);
    QVERIFY(mp->restoreSnapshot(snap, m_doc));
    QCOMPARE(mp->noLightZones().count(), 2);
}

void ShowReview_Test::zoneGeometry()
{
    NoLightZone z;
    z.x = 4; z.y = 10; z.width = 2; z.depth = 2; z.floorZ = 0; z.height = 6;
    const QVector3D apex(5, 2, 5);
    // Straight at it, level: through it at 5 m up.
    QVERIFY(z.hitByRay(apex, QVector3D(0, 1, 0), 30));
    // Past its far wall within range, or out of range, or aimed away.
    QVERIFY(!z.hitByRay(apex, QVector3D(0, -1, 0), 30));
    QVERIFY(!z.hitByRay(apex, QVector3D(0, 1, 0), 5));
    // The floor stops a ray coming down short of it.
    QVERIFY(!z.hitByRay(apex, QVector3D(0, 1, -1).normalized(), 30));
    QVERIFY(z.hitByRay(apex, QVector3D(0, 1, -0.35f).normalized(), 30));
    // A pit below the stage is reached through the floor line it opens.
    NoLightZone pit;
    pit.x = 0; pit.y = 0; pit.width = 4; pit.depth = 4; pit.floorZ = -2; pit.height = 2.5f;
    QVERIFY(pit.hitByRay(QVector3D(2, 2, 5), QVector3D(0, 0, -1), 30));
    // Kinds: a wash is not a beam; a laser is both.
    NoLightZone beams; beams.kind = NoLightZone::NoBeams;
    NoLightZone lasers; lasers.kind = NoLightZone::NoLasers;
    NoLightZone any;
    QVERIFY(beams.applies(false, 10) && !beams.applies(false, 50) && beams.applies(true, 0));
    QVERIFY(lasers.applies(true, 0) && !lasers.applies(false, 10));
    QVERIFY(any.applies(false, 50));
    QCOMPARE(NoLightZone::stringToKind(NoLightZone::kindToString(NoLightZone::NoBeams)), int(NoLightZone::NoBeams));
}

void ShowReview_Test::laserWindowAndPwr09()
{
    QLCPhysical phy;
    double h, v;
    LightRays::laserWindow(phy, h, v);
    QCOMPARE(h, 40.0);
    QCOMPARE(v, 40.0);
    phy.setLensDegreesMin(12);
    phy.setLensDegreesMax(60);
    LightRays::laserWindow(phy, h, v);
    QCOMPARE(h, 60.0);
    QCOMPARE(v, 12.0);
    // The window is wide, not tall: rays reach 30 degrees sideways, 6 up/down.
    const QVector<QVector3D> rays = LightRays::window(QVector3D(0, 1, 0), h, v);
    double maxX = 0, maxZ = 0;
    for (const QVector3D &r : rays)
    {
        maxX = qMax(maxX, double(qAbs(r.x())));
        maxZ = qMax(maxZ, double(qAbs(r.z())));
    }
    QVERIFY(maxX > 0.45 && maxX < 0.51);
    QVERIFY(maxZ > 0.09 && maxZ < 0.11);
}

void ShowReview_Test::pwr10()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 8));
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    // A wide fan: quiet.
    Fixture *wide = laser("Wide", 0, QLCPhysical::LaserEmission, 40, QVector3D(5000, 2000, 5), 60);
    cueLit(m_doc, c1, wide, 255);
    QVERIFY(quiet("PWR-10"));
    // A pencil (0.5 degrees): static; it stays a note while it hits nobody.
    Fixture *pencil = laser("Pencil", 10, QLCPhysical::LaserEmission, 0.5, QVector3D(5000, 2000, 5), 60);
    cueLit(m_doc, c1, pencil, 255);
    QVERIFY(fires("PWR-10"));
    ReviewFinding f = findings("PWR-10").first();
    QCOMPARE(f.severity, ReviewFinding::Note);
    QVERIFY(f.message.contains("Pencil") && f.message.contains("static"));
    QVERIFY(m_doc->deleteFixture(pencil->id()));
    // A tight 8 x 8 window pointed at a no-lasers zone: a warning.
    Fixture *tight = laser("Tight", 20, QLCPhysical::LaserEmission, 8, QVector3D(5000, 2000, 5), 20);
    cueLit(m_doc, c1, tight, 255);
    QVERIFY(fires("PWR-10"));
    f = findings("PWR-10").first();
    QVERIFY(f.message.contains("tight"));
    QVERIFY(f.severity == ReviewFinding::Warning);    // it also reaches the house
}

void ShowReview_Test::pwr11()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 8));
    Fixture *l = laser("Laser 1", 0, QLCPhysical::LaserEmission, 10, QVector3D(5000, 2000, 5), 20);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    const quint32 c2 = st->addCue(lid, 0, "Two");
    const quint32 c3 = st->addCue(lid, 1, "Three");
    cueLit(m_doc, c1, l, 255);
    // No zones: quiet. The beam lands about 15.7 m downstage, 0.9 m either side of x = 5.
    QVERIFY(quiet("PWR-11"));
    const quint32 zid = mp->addNoLightZone(4.0f, 10.0f);
    NoLightZone z = mp->noLightZone(zid);
    z.name = QStringLiteral("Conductor");
    z.width = 2.0f;
    z.depth = 2.0f;
    z.height = 2.5f;
    mp->setNoLightZone(z);
    QVERIFY2(fires("PWR-11"), "beam through the zone");
    ReviewFinding f = findings("PWR-11").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QVERIFY(f.message.contains("Laser 1") && f.message.contains("Conductor") && f.message.contains("One"));
    QVERIFY(f.targets.contains(QStringLiteral("fx:%1").arg(l->id())));
    QVERIFY(f.targets.contains(QStringLiteral("zone:%1").arg(zid)));
    // Tracked on into cues two and three it is still one finding: the cue it starts in.
    cueLit(m_doc, c2, l, 255);
    cueLit(m_doc, c3, l, 255);
    QCOMPARE(findings("PWR-11").count(), 1);
    // Near miss: the zone moved 3 m across, clear of the window's 0.9 m.
    z.x = 8.0f;
    mp->setNoLightZone(z);
    QVERIFY(quiet("PWR-11"));
    // Back in the way but a pit below the beam's end: still clear above it.
    z.x = 4.0f;
    z.floorZ = -3.0f;
    z.height = 2.5f;                        // tops out at -0.5 m: under the stage
    mp->setNoLightZone(z);
    QVERIFY(quiet("PWR-11"));
    // A no-lasers zone catches a laser; the same zone as no-beams does too (lasers are beams).
    z.floorZ = 0.0f;
    z.kind = NoLightZone::NoLasers;
    mp->setNoLightZone(z);
    QVERIFY(fires("PWR-11"));
    // Dark in the cue: quiet. (A zone is about lit beams.)
    st->removeLayer(c1, st->cueList(lid)->cue(c1)->layers.last().id);
    st->removeLayer(c2, st->cueList(lid)->cue(c2)->layers.last().id);
    st->removeLayer(c3, st->cueList(lid)->cue(c3)->layers.last().id);
    cueLit(m_doc, c1, l, 0);
    cueLit(m_doc, c2, l, 0);
    cueLit(m_doc, c3, l, 0);
    QVERIFY(quiet("PWR-11"));
    // A par through the same place is not a laser: a no-lasers zone leaves it alone.
    Fixture *p = par("Par 1", 20);
    QVERIFY(p != nullptr);
}

void ShowReview_Test::pwr11Mover()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    Fixture *a = mover("Spot 1", 0);
    FixturePreviewItem item;
    item.m_baseItem.m_position = QVector3D(5000, 5000, 6);
    mp->setFixtureProperties(a->id(), item);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    cueHead(m_doc, c1, a, 270, 135, 200);               // the middle of both ranges: its home
    const quint32 zid = mp->addNoLightZone(0.0f, 0.0f);
    NoLightZone z = mp->noLightZone(zid);
    z.name = QStringLiteral("Pit");
    z.width = 20.0f; z.depth = 20.0f; z.height = 2.0f;  // under the stage's whole floor area
    mp->setNoLightZone(z);
    QVERIFY2(fires("PWR-11"), "a hung head at home points down into the zone");
    QVERIFY(findings("PWR-11").first().message.contains("Spot 1"));
    // Near miss: the zone well off to one side.
    z.x = 14.0f; z.width = 2.0f;
    mp->setNoLightZone(z);
    QVERIFY(quiet("PWR-11"));
}

/* A UST points with pan DMX 0-127 and spins above it. The no-light-zone check
   reads that pan across the band, so a head aimed into a zone trips PWR-11 and
   the same head aimed away does not -- and a zone where the whole-channel
   reading would have put the beam stays quiet. */
namespace
{
/** A cyc 10 m wide and 6 m tall across the stage at y = 2 m, facing the house. */
Scenery *addCyc(MonitorProperties *mp, int kind = Scenery::Cyclorama, float y = 2.0f)
{
    Scenery *s = mp->addScenery();
    s->setKind(kind);
    s->setName(kind == Scenery::Cyclorama ? QStringLiteral("Cyc") : Scenery::kindName(kind));
    s->setOriginX(5.0f);
    s->setOriginY(y);
    s->setWidth(10.0f);
    s->setHeight(6.0f);
    if (kind == Scenery::BackWall)
        s->setThickness(0.2f);
    return s;
}

/** A front light at 6 m, 10 m downstage of the cyc, thrown upstage and
 *  @a down degrees down: at 20 it carries on to the cyc, at 70 it lands on
 *  the floor well short of it. */
void aimFront(MonitorProperties *mp, Fixture *fx, float x, float down)
{
    FixturePreviewItem item;
    item.m_baseItem.m_position = QVector3D(x * 1000.0f, 12000.0f, 6.0f);
    mp->setFixtureProperties(fx->id(), item);
    FixtureRigProps rp = mp->fixtureRigProps(fx->id());
    rp.fixedAim = true;
    rp.panZeroDir = 180;                    // toward -Y: upstage
    rp.aimDown = down;
    mp->setFixtureRigProps(fx->id(), rp);
}

QString findingText(const QList<ReviewFinding> &list)
{
    QStringList out;
    for (const ReviewFinding &f : list)
        out << f.message + QLatin1Char('|') + f.targets.join(QLatin1Char(',')) + QLatin1Char('|')
               + QString::fromLatin1(f.fingerprint) + QLatin1Char('|')
               + QString::fromUtf8(QJsonDocument::fromVariant(f.jumpData).toJson(QJsonDocument::Compact));
    return out.join(QLatin1Char('\n'));
}
}

void ShowReview_Test::pwr12Backwash()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 14));
    Fixture *front = par("Front 1", 0);
    aimFront(mp, front, 5.0f, 20.0f);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    const quint32 c2 = st->addCue(lid, 0, "Two");
    const quint32 c3 = st->addCue(lid, 1, "Three");
    cueLit(m_doc, c1, front, 255);
    // No scenery: quiet.
    QVERIFY(quiet("PWR-12"));
    Scenery *sc = addCyc(mp);
    QVERIFY(sc->lightOnPurposeOnly());              // a cyc: on by default
    QVERIFY2(fires("PWR-12"), "a front light carrying on to the cyc");
    ReviewFinding f = findings("PWR-12").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QVERIFY(f.isOpen() && !f.intended);
    QVERIFY(f.message.contains("Front 1") && f.message.contains("'Cyc'") && f.message.contains("One"));
    QVERIFY(f.message.contains("about"));
    QVERIFY(f.targets.contains(QStringLiteral("fx:%1").arg(front->id())));
    QVERIFY(f.targets.contains(QStringLiteral("scenery:%1").arg(sc->id())));
    QVERIFY(ShowReview::canMarkMeant(f));
    // Tracked on (set again in two and three): one finding, every cue of it named.
    cueLit(m_doc, c2, front, 255);
    cueLit(m_doc, c3, front, 255);
    QCOMPARE(findings("PWR-12").count(), 1);
    QCOMPARE(ShowReview::findingCues(findings("PWR-12").first()), QList<quint32>() << c1 << c2 << c3);
    // Deterministic: the same show gives the same findings, word for word.
    const QString once = findingText(findings("PWR-12"));
    for (int i = 0; i < 3; ++i)
        QCOMPARE(findingText(findings("PWR-12")), once);
    // Thrown steeply down it lands on the floor short of the cyc.
    aimFront(mp, front, 5.0f, 70.0f);
    QVERIFY(quiet("PWR-12"));
    aimFront(mp, front, 5.0f, 20.0f);
    // Off the side of the cyc: quiet.
    aimFront(mp, front, 30.0f, 20.0f);
    QVERIFY(quiet("PWR-12"));
    aimFront(mp, front, 5.0f, 20.0f);
    // The cyc switched off "light here only on purpose": quiet.
    sc->setLightOnPurposeOnly(false);
    QVERIFY(quiet("PWR-12"));
    sc->setLightOnPurposeOnly(true);
    // A cyc light is meant to light it.
    front->setStageLook(Fixture::CycStripLook);
    QVERIFY(quiet("PWR-12"));
    front->setStageLook(Fixture::UplightWashLook);
    QVERIFY(quiet("PWR-12"));
    front->setStageLook(Fixture::AutoLook);
    QVERIFY(fires("PWR-12"));
    // A back wall in front of it stops the light: quiet. A scrim there lets it
    // through and takes light itself: both named.
    Scenery *wall = addCyc(mp, Scenery::BackWall, 4.0f);
    QVERIFY(!wall->lightOnPurposeOnly());
    QVERIFY(quiet("PWR-12"));
    mp->removeScenery(wall->id());
    Scenery *scrim = addCyc(mp, Scenery::Scrim, 4.0f);
    QVERIFY(scrim->lightOnPurposeOnly());
    QCOMPARE(findings("PWR-12").count(), 2);
    scrim->setLightOnPurposeOnly(false);
    QCOMPARE(findings("PWR-12").count(), 1);
    QVERIFY(findings("PWR-12").first().targets.contains(QStringLiteral("scenery:%1").arg(sc->id())));
    // Dark in every cue: quiet.
    for (quint32 c : { c1, c2, c3 })
    {
        st->removeLayer(c, st->cueList(lid)->cue(c)->layers.last().id);
        cueLit(m_doc, c, front, 0);
    }
    QVERIFY(quiet("PWR-12"));
}

void ShowReview_Test::pwr12Meant()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 14));
    addCyc(mp);
    Fixture *front = par("Front 1", 0);
    aimFront(mp, front, 5.0f, 20.0f);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    const quint32 c2 = st->addCue(lid, 0, "Two");
    cueLit(m_doc, c1, front, 255);
    cueLit(m_doc, c2, front, 255);
    const quint32 layer1 = st->cueList(lid)->cue(c1)->layers.last().id;
    ReviewFinding f = findings("PWR-12").first();
    QVERIFY(f.isOpen());
    // "Show me" goes to the layer that lights it.
    QCOMPARE(f.jump, QStringLiteral("cue"));
    QCOMPARE(f.jumpData.first().toUInt(), layer1);

    // I meant it: kept on the layer, one undo step; not open any more.
    QVERIFY(ShowReview::setLightMeant(m_doc, f, true));
    QCOMPARE(st->undoText(), QStringLiteral("I meant it"));
    QVERIFY(!findings("PWR-12").first().isOpen());
    QVERIFY(st->undo());                            // one step takes it back
    QVERIFY(findings("PWR-12").first().isOpen());
    QVERIFY(ShowReview::setLightMeant(m_doc, findings("PWR-12").first(), true));
    QCOMPARE(st->cueList(lid)->cue(c1)->layer(layer1)->meantOnScenery, QList<quint32>() << front->id());
    ShowReview::Result r = m_doc->showReview()->run();
    QCOMPARE(r.open[ReviewFinding::Warning], 0);
    QList<ReviewFinding> all = findings("PWR-12");
    QCOMPARE(all.count(), 1);
    QVERIFY(all.first().intended && !all.first().isOpen());
    QVERIFY(ShowReview::canMarkMeant(all.first(), false));
    QVERIFY(!ShowReview::canMarkMeant(all.first(), true));
    // It carries on into cue two, where its layer is set again.
    for (const ReviewFinding &x : all)
        QVERIFY(!x.isOpen());

    // Saved with the show: it stays meant after a save and load.
    editTree([](QString &xml) { QVERIFY(xml.contains(QStringLiteral("MeantOnScenery="))); });
    QCOMPARE(st->cueList(lid)->cue(c1)->layer(layer1)->meantOnScenery, QList<quint32>() << front->id());
    QVERIFY(!findings("PWR-12").first().isOpen());

    // Not meant (Reopen in the panel), and undo of that.
    QVERIFY(ShowReview::setLightMeant(m_doc, findings("PWR-12").first(), false));
    QVERIFY(findings("PWR-12").first().isOpen());
    QVERIFY(st->undo());
    QVERIFY(!findings("PWR-12").first().isOpen());

    // Cleared from the layer (its right-click).
    QVERIFY(ShowReview::clearLayerMeant(m_doc, c1, layer1));
    QVERIFY(!ShowReview::clearLayerMeant(m_doc, c1, layer1));     // nothing left on it
    QVERIFY(findings("PWR-12").first().isOpen());

    // An intended finding stays intended when another acceptance changes.
    QVERIFY(ShowReview::setLightMeant(m_doc, findings("PWR-12").first(), true));
    m_doc->showReview()->run();
    QVERIFY(m_doc->showReview()->turnOff(QStringLiteral("PAT-01"), QStringLiteral("test"), QStringLiteral("me")));
    for (const ReviewFinding &x : m_doc->showReview()->lastResult().findings)
        if (x.check == QLatin1String("PWR-12"))
            QVERIFY(x.intended && !x.isOpen());
}

/* Three states for a warning: open, meant (who, when, the note: saved with
   the show), not a problem (an acceptance). Each change is one answer. */
void ShowReview_Test::pwr12WarnStates()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 14));
    addCyc(mp);
    Fixture *front = par("Front 1", 0);
    aimFront(mp, front, 5.0f, 20.0f);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    cueLit(m_doc, c1, front, 255);
    const quint32 layer1 = st->cueList(lid)->cue(c1)->layers.last().id;

    // Open.
    ReviewFinding f = findings("PWR-12").first();
    QVERIFY(f.isOpen() && !f.intended);

    // Open -> Meant, with who, when and a note; one undo step.
    QVERIFY(ShowReview::setLightMeant(m_doc, f, true, QStringLiteral("  the cyc wants it  "), QStringLiteral("Sam")));
    QCOMPARE(st->undoText(), QStringLiteral("I meant it"));
    const Layer *layer = st->cueList(lid)->cue(c1)->layer(layer1);
    QCOMPARE(layer->meantBy, QStringLiteral("Sam"));
    QCOMPARE(layer->meantNote, QStringLiteral("the cyc wants it"));
    QVERIFY(QDateTime::fromString(layer->meantWhen, Qt::ISODate).isValid());
    f = findings("PWR-12").first();
    QVERIFY(f.intended && !f.isOpen());
    QCOMPARE(f.acceptedBy, QStringLiteral("Sam"));
    QCOMPARE(f.acceptedNote, QStringLiteral("the cyc wants it"));
    QCOMPARE(f.acceptedWhen, layer->meantWhen);
    QVERIFY(st->undo());                                // one step back to open
    QVERIFY(findings("PWR-12").first().isOpen());
    QVERIFY(st->cueList(lid)->cue(c1)->layer(layer1)->meantBy.isEmpty());
    QVERIFY(st->redo());
    QVERIFY(findings("PWR-12").first().intended);

    // Saved with the show: who/when/note round-trip.
    const QString when = findings("PWR-12").first().acceptedWhen;
    editTree([](QString &xml) {
        QVERIFY(xml.contains(QStringLiteral("MeantBy=\"Sam\"")));
        QVERIFY(xml.contains(QStringLiteral("MeantNote=")));
    });
    f = findings("PWR-12").first();
    QVERIFY(f.intended);
    QCOMPARE(f.acceptedBy, QStringLiteral("Sam"));
    QCOMPARE(f.acceptedNote, QStringLiteral("the cyc wants it"));
    QCOMPARE(f.acceptedWhen, when);

    // An older file (a mark with no name) loads as meant, nobody named.
    editTree([](QString &xml) {
        xml.remove(QRegularExpression(QStringLiteral(" Meant(By|When|Note)=\"[^\"]*\"")));
        QVERIFY(!xml.contains(QStringLiteral("MeantBy=")));
        QVERIFY(xml.contains(QStringLiteral("MeantOnScenery=")));
    });
    f = findings("PWR-12").first();
    QVERIFY(f.intended && f.acceptedBy.isEmpty() && f.acceptedNote.isEmpty());

    // Meant -> Open (Not Meant): the who/when go with the mark.
    QVERIFY(ShowReview::setLightMeant(m_doc, f, false));
    QVERIFY(findings("PWR-12").first().isOpen());
    QVERIFY(st->cueList(lid)->cue(c1)->layer(layer1)->meantWhen.isEmpty());

    // Open -> Not a Problem (an acceptance: Accepted, not intended).
    f = findings("PWR-12").first();
    QVERIFY(m_doc->showReview()->accept(f, QStringLiteral("a scrim there"), QStringLiteral("Kim")));
    f = findings("PWR-12").first();
    QVERIFY(f.state == ReviewFinding::Accepted && !f.intended);
    QCOMPARE(f.acceptedBy, QStringLiteral("Kim"));
    // Not a Problem -> Open (Reopen).
    QVERIFY(m_doc->showReview()->reopen(f));
    QVERIFY(findings("PWR-12").first().isOpen());

    // Meant -> Not a Problem: the mark off, the acceptance on.
    QVERIFY(ShowReview::setLightMeant(m_doc, findings("PWR-12").first(), true));
    f = findings("PWR-12").first();
    QVERIFY(f.intended);
    QVERIFY(m_doc->showReview()->accept(f, QStringLiteral("not here"), QStringLiteral("Kim")));
    QVERIFY(ShowReview::setLightMeant(m_doc, f, false));
    m_doc->showReview()->run();
    f = findings("PWR-12").first();
    QVERIFY(f.state == ReviewFinding::Accepted && !f.intended);
    QCOMPARE(f.acceptedNote, QStringLiteral("not here"));
}

void ShowReview_Test::pwr12AimAtCyc()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 14));
    Scenery *sc = addCyc(mp);
    Fixture *spot = mover("Spot 1", 0);
    FixturePreviewItem item;
    item.m_baseItem.m_position = QVector3D(5000, 9000, 6);
    mp->setFixtureProperties(spot->id(), item);
    // A mark on the cyc itself, a quarter of a metre in front of it.
    StageTarget *mark = mp->addStageTarget();
    mark->setName(QStringLiteral("Cyc centre"));
    mark->setPosition(QVector3D(5.0f, 2.25f, 3.0f));
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, m_doc);
    aim->setName(QStringLiteral("At the cyc"));
    aim->setStageTargetId(mark->id());
    QVERIFY(m_doc->addPalette(aim));
    QLCPalette *level = new QLCPalette(QLCPalette::Dimmer, m_doc);
    level->setValue(255);
    level->setName(QStringLiteral("Full"));
    QVERIFY(m_doc->addPalette(level));
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    Target t;
    t.fixtures = { spot->id() };
    const quint32 layer = st->addLayer(c1, t);
    Look pos;
    pos.key = KeyPosition;
    pos.palettes = { aim->id() };
    QVERIFY(st->setLayerLook(c1, layer, pos));
    Look d;
    d.key = KeyDimmer;
    d.palettes = { level->id() };
    QVERIFY(st->setLayerLook(c1, layer, d));
    // Aimed at a target on the cyc: meant.
    sc->setLightOnPurposeOnly(false);
    QVERIFY(quiet("PWR-12"));
    sc->setLightOnPurposeOnly(true);
    QVERIFY2(quiet("PWR-12"), "a look aimed at a target on the cyc is meant");
    // The mark 0.9 m in front of the cyc (more than 0.5 m): the light behind it is not meant.
    mark->setPosition(QVector3D(5.0f, 2.9f, 3.0f));
    QVERIFY2(fires("PWR-12"), "a mark off the cyc does not mean the cyc");
    mark->setPosition(QVector3D(5.0f, 2.25f, 3.0f));
    QVERIFY(quiet("PWR-12"));
    // The same place, but a person standing there: not the cyc, so not meant.
    mark->setKind(StageTarget::Kind::Person);
    QVERIFY2(fires("PWR-12"), "a performer in front of the cyc: the light behind them is not meant");
    QVERIFY(findings("PWR-12").first().message.contains("Spot 1"));
}

/* The curtain (the user's show: "the cyc checker didn't take into account the
   curtain"): a closed drape between a front light and the cyc takes the light;
   a curtain driven by a fixture is where the cue's functions put it. */
void ShowReview_Test::pwr12Curtain()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 14));
    Scenery *cyc = addCyc(mp);
    Fixture *front = par("Front 1", 0);
    aimFront(mp, front, 5.0f, 20.0f);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "Preshow");
    const quint32 c2 = st->addCue(lid, 0, "Curtain");
    cueLit(m_doc, c1, front, 255);
    cueLit(m_doc, c2, front, 255);
    QVERIFY2(fires("PWR-12"), "no curtain: light on the cyc");

    // A main drape between them, 4 m in front of the light, plotted closed: quiet.
    Scenery *drape = mp->addScenery();
    drape->setKind(Scenery::MainDrape);
    drape->setName(QStringLiteral("Main drape"));
    drape->setOriginX(5.0f);
    drape->setOriginY(8.0f);
    drape->setWidth(12.0f);
    drape->setHeight(6.0f);
    drape->setOpeningWidth(8.0f);
    QVERIFY(!drape->lightOnPurposeOnly());
    drape->setState(Scenery::Closed);
    QVERIFY2(quiet("PWR-12"), "a closed curtain takes the light");
    drape->setState(Scenery::Open);
    QVERIFY2(fires("PWR-12"), "through the open curtain");
    drape->setState(Scenery::Flown);
    QVERIFY(fires("PWR-12"));

    // Driven by a curtain fixture (0 closed, 255 open), never moved by the
    // list: as plotted.
    Fixture *rail = new Fixture(m_doc);
    rail->setName(QStringLiteral("Curtain"));
    rail->setChannels(1);
    rail->setAddress(400);
    QVERIFY(m_doc->addFixture(rail));
    rail->setStageLook(Fixture::CurtainLook);
    drape->setDriverFixture(rail->id());
    QVERIFY(drape->isDriven());
    drape->setState(Scenery::Closed);
    QVERIFY(quiet("PWR-12"));
    drape->setState(Scenery::Open);
    QVERIFY(fires("PWR-12"));

    // Moved by the cues: closed in the preshow (at rest: nothing plays on it),
    // opened in cue two by a function layer. Named in cue two only.
    Scene *open = new Scene(m_doc);
    open->setName(QStringLiteral("Curtain Open"));
    open->setValue(rail->id(), 0, 255);
    QVERIFY(m_doc->addFunction(open));
    Scene *closed = new Scene(m_doc);
    closed->setName(QStringLiteral("Curtain Closed"));
    closed->setValue(rail->id(), 0, 0);
    QVERIFY(m_doc->addFunction(closed));
    Target ot;
    ot.function = open->id();
    QVERIFY(st->addLayer(c2, ot) != invalidId());
    QVERIFY2(fires("PWR-12"), "the curtain opens in cue two");
    QCOMPARE(findings("PWR-12").count(), 1);
    ReviewFinding f = findings("PWR-12").first();
    QVERIFY2(f.message.contains("Curtain"), qPrintable(f.message));
    QCOMPARE(ShowReview::findingCues(f), QList<quint32>() << c2);
    QVERIFY(f.targets.contains(QStringLiteral("scenery:%1").arg(cyc->id())));

    // Closed by a scene in the preshow: still quiet there, named in cue two.
    Target ct;
    ct.function = closed->id();
    QVERIFY(st->addLayer(c1, ct) != invalidId());
    QCOMPARE(findings("PWR-12").count(), 1);
    QCOMPARE(ShowReview::findingCues(findings("PWR-12").first()), QList<quint32>() << c2);

    // Inside a collection the cue plays: the same.
    st->removeLayer(c2, st->cueList(lid)->cue(c2)->layers.last().id);
    QVERIFY(quiet("PWR-12"));
    Collection *coll = new Collection(m_doc);
    coll->setName(QStringLiteral("Open it"));
    QVERIFY(m_doc->addFunction(coll));
    QVERIFY(coll->addFunction(open->id()));
    Target colT;
    colT.function = coll->id();
    QVERIFY(st->addLayer(c2, colT) != invalidId());
    QCOMPARE(ShowReview::findingCues(findings("PWR-12").first()), QList<quint32>() << c2);

    // Half open: the 8 m opening is 4 m across, the beam (at x = 5) goes
    // through the middle of it.
    open->setValue(rail->id(), 0, 128);
    QVERIFY(fires("PWR-12"));
    // Barely open (0.4 m): most of the cone lands on the curtain, not the cyc.
    open->setValue(rail->id(), 0, 13);
    const ReviewFinding narrow = findings("PWR-12").isEmpty() ? ReviewFinding() : findings("PWR-12").first();
    open->setValue(rail->id(), 0, 255);
    const ReviewFinding wide = findings("PWR-12").first();
    QVERIFY2(narrow.message != wide.message, "less of the beam reaches the cyc through a narrow gap");
}

void ShowReview_Test::sceneryOnPurposeSaves()
{
    Scenery cyc(0);
    QVERIFY(cyc.lightOnPurposeOnly());
    cyc.setKind(Scenery::BackWall);
    QVERIFY(!cyc.lightOnPurposeOnly());              // follows the kind's default
    cyc.setKind(Scenery::Scrim);
    QVERIFY(cyc.lightOnPurposeOnly());
    cyc.setKind(Scenery::Wall);
    cyc.setLightOnPurposeOnly(true);                 // a painted wall opts in
    auto roundTrip = [](const Scenery &s) {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        {
            QXmlStreamWriter w(&buf);
            w.writeStartDocument();
            s.saveXML(&w);
            w.writeEndDocument();
        }
        Scenery *back = new Scenery(1);
        QXmlStreamReader r(buf.data());
        while (!r.atEnd() && !(r.isStartElement() && r.name() == KXMLScenery))
            r.readNext();
        back->loadXML(r);
        return back;
    };
    Scenery *wall = roundTrip(cyc);
    QVERIFY(wall->lightOnPurposeOnly());
    delete wall;
    Scenery plain(2);
    plain.setLightOnPurposeOnly(false);              // a cyc opted out
    Scenery *off = roundTrip(plain);
    QVERIFY(!off->lightOnPurposeOnly());
    delete off;
    Scenery untouched(3);
    Scenery *same = roundTrip(untouched);
    QVERIFY(same->lightOnPurposeOnly());
    delete same;
}

void ShowReview_Test::pwr11BandedPan()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    QLCFixtureDef *def = new QLCFixtureDef();
    QLCChannel *pan = chan(def, "Pan", QLCChannel::PositionPan);
    pan->addCapability(new QLCCapability(0, 127, "Indexed position"));
    pan->capabilities().first()->setPreset(QLCCapability::RotationIndexed);
    pan->addCapability(new QLCCapability(128, 255, "Rotation"));
    Fixture *ust = make(m_doc, "UST 1", QStringLiteral("UST"), QLCFixtureDef::MovingHead, def,
                        { pan, chan(def, "Tilt", QLCChannel::PositionTilt),
                          chan(def, "Dimmer", QLCChannel::IntensityMasterDimmer) }, 200, 0, 0);
    QVERIFY(ust != nullptr);
    const quint32 fid = ust->id();
    FixturePreviewItem item;
    item.m_baseItem.m_position = QVector3D(5000, 5000, 6);
    mp->setFixtureProperties(fid, item);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    cueHead(m_doc, c1, ust, 270, 180, 200);     // mid band (DMX 63.5), 45 degrees off straight down

    // Where the beam lands on the floor for a pan offset from home, in metres.
    const FixtureRigProps rp = mp->fixtureRigProps(fid);
    const QVector3D apex = RigHeads::fixtureCentre(m_doc, fid);
    auto landing = [&](double panOffsetDeg) {
        QVector3D dir;
        AimSolver::beamDirection(rp, panOffsetDeg, 45.0, dir);
        dir.normalize();
        const float t = apex.z() / -dir.z();
        return QVector3D(apex.x() + dir.x() * t, apex.y() + dir.y() * t, 0);
    };
    // Banded: DMX 63.5 is the middle of the band, pan offset 0. Over the whole
    // channel the same DMX would read 0.249, 135 degrees round.
    uchar lo, hi;
    QVERIFY(Fixture::positionBand(pan, lo, hi));
    QVERIFY(qAbs(Fixture::positionFraction(pan, 63.5) - 0.5) < 1e-9);
    QVERIFY(qAbs(63.5 / 255.0 - 0.5) > 0.2);
    const QVector3D right = landing(0.0), wrong = landing((63.5 / 255.0 - 0.5) * 540.0);
    QVERIFY((right - wrong).length() > 3.0);

    const quint32 zid = mp->addNoLightZone(0.0f, 0.0f);
    auto zoneAt = [&](const QVector3D &p) {
        NoLightZone z = mp->noLightZone(zid);
        z.name = QStringLiteral("Zone");
        z.width = 3.0f; z.depth = 3.0f; z.height = 3.0f; z.floorZ = 0.0f;
        z.x = p.x() - 1.5f; z.y = p.y() - 1.5f;
        mp->setNoLightZone(z);
    };
    // The zone where the head points: PWR-11.
    zoneAt(right);
    QVERIFY2(fires("PWR-11"), "a banded head aimed into the zone");
    // The zone where the whole channel would have put the beam: quiet.
    zoneAt(wrong);
    QVERIFY2(quiet("PWR-11"), "the head does not point there");
}

/* BLK-01 (docs/design/BLOCKING.md): a blocking that puts a target where
   the lights aimed at it, which reach it in Default, can't. */
void ShowReview_Test::blk01()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    Truss *pipe = mp->addTruss();
    pipe->setType(Truss::Horizontal);
    pipe->setOrigin(QVector3D(-3.0f, 0.0f, 6.0f));
    pipe->setDirection(QPointF(1.0, 0.0));
    pipe->setLength(6.0f);

    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Spot BLK");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    for (int k = 0; k < 2; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(k == 0 ? "Pan" : "Tilt");
        ch->setGroup(k == 0 ? QLCChannel::Pan : QLCChannel::Tilt);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1);
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *spot = new Fixture(m_doc);
    spot->setName("Spot");
    spot->setFixtureDefinition(def, mode);
    QVERIFY(m_doc->addFixture(spot));
    FixtureRigProps rp;
    rp.trussId = pipe->id();
    rp.trussOffset = 3.0f;
    rp.mountingType = Truss::TopHung;
    mp->setFixtureRigProps(spot->id(), rp);
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setSize(QSize(1, 1));
    grp->assignFixture(spot->id());
    m_doc->addFixtureGroup(grp);

    StageTarget *rank = mp->addStageTarget();
    rank->setName("Front Rank");
    rank->setPosition(QVector3D(0, 2, 0));
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, m_doc);
    aim->setName("Front Rank warm");
    aim->setStageTargetId(rank->id());
    QVERIFY(m_doc->addPalette(aim));

    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "Main", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    Target t;
    t.groups << grp->id();
    st->setLayerLook(c1, st->addLayer(c1, t), st->lookForPalette(aim->id()));

    QVERIFY(quiet("BLK-01"));                   // no blocking at all
    const quint32 up = mp->addBlockingSet("Flying");
    StageTarget::Blocked b;
    b.hasOffset = true;
    b.offset = QVector3D(0, -2, 14);            // straight over the pipe
    rank->setBlocked(up, b);
    QVERIFY(quiet("BLK-01"));                   // a set no cue plays
    QVERIFY(st->setCuesBlocking({ c2 }, up));
    QVERIFY(fires("BLK-01"));
    const ReviewFinding f = findings("BLK-01").first();
    QVERIFY2(f.message.contains("Flying") && f.message.contains("Front Rank"), qPrintable(f.message));
    QVERIFY(f.targets.contains(QString("blocking:%1").arg(up)));
    QCOMPARE(findings("BLK-01").count(), 1);    // once, though cue 2 is the only one

    // Within reach in that blocking: quiet.
    b.offset = QVector3D(0, 1.5f, 0);
    rank->setBlocked(up, b);
    QVERIFY(quiet("BLK-01"));
}

void ShowReview_Test::everyCheckIsTested()
{
    for (const ShowReview::Check &c : ShowReview::catalog())
    {
        QVERIFY2(s_fired.contains(c.id), qPrintable(QStringLiteral("%1 never fired in a test").arg(c.id)));
        QVERIFY2(s_quiet.contains(c.id), qPrintable(QStringLiteral("%1 was never shown quiet").arg(c.id)));
    }
}


/* Masking: tape over the front of a light. */
void ShowReview_Test::maskCutsTheWindow()
{
    const QVector3D dir(0, -1, 0);
    const QVector<QVector3D> full = LightRays::window(dir, 40, 40);
    QCOMPARE(full.size(), 81);
    LightMask m;
    m.bottom = 15.0f;
    const QVector<QVector3D> cut = LightRays::window(dir, 40, 40, m);
    QCOMPARE(cut.size(), 81);               // the same grid, over what is left
    double lowest = -1e9, lowestFull = -1e9;
    for (const QVector3D &r : cut)
        lowest = qMax(lowest, -double(r.z()));
    for (const QVector3D &r : full)
        lowestFull = qMax(lowestFull, -double(r.z()));
    QVERIFY(lowestFull > 0.3 && lowest < lowestFull - 0.15);
    // Everything taped: nothing leaves it. A cone loses its taped side.
    LightMask all;
    all.top = 25.0f;
    all.bottom = 25.0f;
    QVERIFY(LightRays::window(dir, 40, 40, all).isEmpty());
    LightMask side;
    side.left = 10.0f;
    const QVector<QVector3D> cone = LightRays::cone(dir, qDegreesToRadians(20.0), side);
    QVERIFY(cone.size() > 10 && cone.size() < LightRays::cone(dir, qDegreesToRadians(20.0)).size());
    for (const QVector3D &r : cone)
    {
        double ax, ay;
        LightRays::angles(dir, r, ax, ay);
        QVERIFY(ax > -10.0 + 1e-3 - 0.01);
    }

    // Saved with the show, and back.
    MonitorProperties *mp = m_doc->monitorProperties();
    Fixture *l = laser("Taped", 0, QLCPhysical::LaserEmission, 40, QVector3D(5000, 2000, 5), 20);
    FixtureRigProps rp = mp->fixtureRigProps(l->id());
    rp.maskTop = 1.5f; rp.maskBottom = 15.0f; rp.maskLeft = 2.5f; rp.maskRight = 3.0f; rp.laserClass = 3;
    mp->setFixtureRigProps(l->id(), rp);
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    {
        QXmlStreamWriter w(&buf);
        w.writeStartDocument();
        mp->saveXML(&w, m_doc);
        w.writeEndDocument();
    }
    MonitorProperties back;
    QXmlStreamReader r(buf.data());
    while (!r.atEnd() && !(r.isStartElement() && r.name() == QLatin1String("Monitor")))
        r.readNext();
    QVERIFY(back.loadXML(r, m_doc));
    const FixtureRigProps b = back.fixtureRigProps(l->id());
    QCOMPARE(b.maskTop, 1.5f);
    QCOMPARE(b.maskBottom, 15.0f);
    QCOMPARE(b.maskLeft, 2.5f);
    QCOMPARE(b.maskRight, 3.0f);
    QCOMPARE(b.laserClass, 3);
}

/* Power class: low-power lasers reaching the house are a note, 3B and 4 a warning. */
void ShowReview_Test::laserClassPwr09()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 8));
    Fixture *l = laser("Sky", 0, QLCPhysical::LaserEmission, 40, QVector3D(5000, 2000, 5), 20);
    QVERIFY(fires("PWR-09"));
    QCOMPARE(findings("PWR-09").first().severity, ReviewFinding::Warning);     // unknown: as before
    auto setClass = [&](int c) {
        FixtureRigProps rp = mp->fixtureRigProps(l->id());
        rp.laserClass = c;
        mp->setFixtureRigProps(l->id(), rp);
    };
    setClass(2);
    ReviewFinding f = findings("PWR-09").first();
    QCOMPARE(f.severity, ReviewFinding::Note);
    QVERIFY(f.message.contains("class 2"));
    setClass(3);
    QCOMPARE(findings("PWR-09").first().severity, ReviewFinding::Note);
    QVERIFY(findings("PWR-09").first().message.contains("3R"));
    setClass(4);
    QCOMPARE(findings("PWR-09").first().severity, ReviewFinding::Warning);
    setClass(5);
    f = findings("PWR-09").first();
    QCOMPARE(f.severity, ReviewFinding::Warning);
    QVERIFY(f.message.contains("class 4"));
    // Taped off so it cannot reach the house at all: quiet.
    FixtureRigProps rp = mp->fixtureRigProps(l->id());
    rp.maskBottom = 38.0f;                  // only the level-to-up sliver is left
    mp->setFixtureRigProps(l->id(), rp);
    QVERIFY(quiet("PWR-09"));
}

/* The showband sits between the cyc and the risers (his show: the cyc at Y 1.355,
   11.19 m wide, 6 m high; the lasers upstage-aimed from the truss). The lower
   part of the window crosses the band; Show Review says how much tape clears it. */
void ShowReview_Test::pwr11Band()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setGridSize(QVector3D(16, 3, 12));
    Scenery *cyc = mp->addScenery();
    cyc->setKind(Scenery::Cyclorama);
    cyc->setName("Cyclorama 1");
    cyc->setOriginX(7.555f);
    cyc->setOriginY(1.355f);
    cyc->setWidth(11.192f);
    cyc->setHeight(6.002f);
    Fixture *l = laser("SL-LASER", 0, QLCPhysical::LaserEmission, 40, QVector3D(8000, 8000, 4), 20);
    FixtureRigProps rp = mp->fixtureRigProps(l->id());
    rp.panZeroDir = 180;                    // upstage, at the cyc
    mp->setFixtureRigProps(l->id(), rp);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    cueLit(m_doc, c1, l, 255);
    // No zone: quiet. Fully on the cyc is the point of the laser.
    QVERIFY(quiet("PWR-11"));
    // The band: from the cyc to the risers' upstage edge (Y 3.76), 2.2 m high.
    const quint32 zid = mp->addNoLightZone(2.5f, 1.4f);
    NoLightZone z = mp->noLightZone(zid);
    z.name = QStringLiteral("Showband");
    z.width = 10.0f;
    z.depth = 2.36f;
    z.height = 2.2f;
    z.kind = NoLightZone::NoLasers;
    mp->setNoLightZone(z);
    QVERIFY2(fires("PWR-11"), "the lower window passes through the band");
    ReviewFinding f = findings("PWR-11").first();
    QVERIFY2(f.message.contains("Showband") && f.message.contains("2.2 m") && f.message.contains("tape off about"),
             qPrintable(f.message));
    QCOMPARE(f.fixMask.size(), 2);
    QCOMPARE(f.fixMask.at(0).toUInt(), l->id());
    const double cut = f.fixMask.at(1).toDouble();
    QVERIFY2(cut > 5.0 && cut < 30.0, qPrintable(QString::number(cut)));
    // A little less does not clear it; the suggested cut does.
    rp = mp->fixtureRigProps(l->id());
    rp.maskBottom = float(cut - 1.0);
    mp->setFixtureRigProps(l->id(), rp);
    QVERIFY(fires("PWR-11"));
    rp.maskBottom = float(cut);
    mp->setFixtureRigProps(l->id(), rp);
    QVERIFY2(quiet("PWR-11"), "masked by the suggested cut");
    // Tape the other end instead: the top off does not help.
    rp.maskBottom = 0.0f;
    rp.maskTop = 10.0f;
    mp->setFixtureRigProps(l->id(), rp);
    QVERIFY(fires("PWR-11"));
}

/* A zone BEHIND the cyc is reached only past its edge or over its top. */
void ShowReview_Test::pwr11BehindCyc()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setGridSize(QVector3D(20, 3, 12));
    Scenery *cyc = mp->addScenery();
    cyc->setKind(Scenery::Cyclorama);
    cyc->setOriginX(5.0f);
    cyc->setOriginY(2.0f);
    cyc->setWidth(10.0f);
    cyc->setHeight(6.0f);
    const quint32 zid = mp->addNoLightZone(0.0f, -3.0f);
    NoLightZone z = mp->noLightZone(zid);
    z.name = QStringLiteral("Behind");
    z.width = 16.0f;
    z.depth = 3.0f;
    z.height = 2.2f;
    z.kind = NoLightZone::NoLasers;
    mp->setNoLightZone(z);
    ShowTreeController *st = m_doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(lid, -1, "One");
    // Wholly on the cyc: nothing gets through it to the zone behind.
    Fixture *a = laser("Centre", 0, QLCPhysical::LaserEmission, 40, QVector3D(5000, 12000, 4), 10);
    FixtureRigProps rp = mp->fixtureRigProps(a->id());
    rp.panZeroDir = 180;
    mp->setFixtureRigProps(a->id(), rp);
    cueLit(m_doc, c1, a, 255);
    QVERIFY2(quiet("PWR-11"), "the cyc stops it");
    // Off to one side its window passes the cyc's edge into the zone.
    Fixture *b = laser("Wide", 10, QLCPhysical::LaserEmission, 80, QVector3D(9500, 12000, 4), 10);
    rp = mp->fixtureRigProps(b->id());
    rp.panZeroDir = 180;
    mp->setFixtureRigProps(b->id(), rp);
    cueLit(m_doc, c1, b, 255);
    QVERIFY2(fires("PWR-11"), "past the cyc's edge");
    QVERIFY(findings("PWR-11").first().message.contains("Wide"));
    // Taped off at that side: clear.
    rp = mp->fixtureRigProps(b->id());
    rp.maskLeft = 38.0f;                    // facing upstage, +X is on the left
    mp->setFixtureRigProps(b->id(), rp);
    QVERIFY2(quiet("PWR-11"), "masked");
}

QTEST_MAIN(ShowReview_Test)

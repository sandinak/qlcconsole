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

    char prog[] = "qlcconsole", rev[] = "--review", file[] = "x.qxw", other[] = "-o";
    char *yes[] = { prog, rev, file };
    char *no[] = { prog, other, file };
    QVERIFY(ShowReviewCli::wanted(3, yes));
    QVERIFY(!ShowReviewCli::wanted(3, no));
}

void ShowReview_Test::everyCheckIsTested()
{
    for (const ShowReview::Check &c : ShowReview::catalog())
    {
        QVERIFY2(s_fired.contains(c.id), qPrintable(QStringLiteral("%1 never fired in a test").arg(c.id)));
        QVERIFY2(s_quiet.contains(c.id), qPrintable(QStringLiteral("%1 was never shown quiet").arg(c.id)));
    }
}

QTEST_MAIN(ShowReview_Test)

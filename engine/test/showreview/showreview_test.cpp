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
              quint32 universe, quint32 address)
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
 * Acceptance
 ****************************************************************************/

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

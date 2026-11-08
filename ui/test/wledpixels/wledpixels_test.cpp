/*
  Q Light Controller Plus
  wledpixels_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QTreeWidget>
#include <QUdpSocket>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtTest>

#include "wledpixels_test.h"
#include "../common/fakewled.h"
#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "addfixture.h"
#undef protected
#undef private
#include "monitorproperties.h"
#include "fixturecreator.h"
#include "wledconfigcheck.h"
#include "rgbscriptscache.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "rgbalgorithm.h"
#include "patchplanner.h"
#include "fixturegroup.h"
#include "outputpatch.h"
#include "mastertimer.h"
#include "wledfixture.h"
#include "qlcchannel.h"
#include "rgbmatrix.h"
#include "pixelpath.h"
#include "righeads.h"
#include "fixture.h"
#include "truss.h"
#include "scene.h"
#include "doc.h"

namespace
{
/* The WLED Lantern definition, in @p doc's cache. */
QLCFixtureDef *lanternDef(Doc *doc)
{
    if (QLCFixtureDef *had = doc->fixtureDefCache()->fixtureDef("WLED", "Lantern"))
        return had;
    QLCFixtureDef *def = new QLCFixtureDef();
    if (def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Lantern.qxf") != QFile::NoError
        || !doc->fixtureDefCache()->addFixtureDef(def))
    {
        delete def;
        return nullptr;
    }
    return def;
}

/* A Lantern on the network by Art-Net at @p ip, patched as @p leds LEDs. */
Fixture *pixelWled(Doc *doc, int leds, bool rgbw = false, const QString &ip = "10.99.0.41",
                   quint32 address = 0)
{
    QLCFixtureDef *def = lanternDef(doc);
    if (def == nullptr)
        return nullptr;
    Fixture *fx = new Fixture(doc);
    fx->setName("Fairy string");
    fx->setConnection(Fixture::Network);
    fx->setTransport(Fixture::ArtNetTransport);
    fx->setIPAddress(ip);
    fx->setAddress(address);
    WledFixture::applyPixels(fx, def, leds, rgbw);
    return fx;
}

QJsonObject liveCfg(bool en, int port, int uni, int addr, int mode)
{
    QJsonObject dmx{ { "uni", uni }, { "addr", addr }, { "mode", mode } };
    QJsonObject live{ { "en", en }, { "port", port }, { "timeout", 25 }, { "mso", false },
                      { "maxbri", true }, { "dmx", dmx } };
    return QJsonObject{ { "if", QJsonObject{ { "live", live } } } };
}

/* Run @p fill on the Add Fixture dialog once it is up, then press OK. */
void driveAddDialog(std::function<void(AddFixture *)> fill)
{
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [fill, poll]() {
        AddFixture *dlg = qobject_cast<AddFixture *>(QApplication::activeModalWidget());
        if (dlg == nullptr)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        fill(dlg);
        dlg->m_buttonBox->button(QDialogButtonBox::Ok)->click();
    };
    QTimer::singleShot(0, *poll);
}

/* Closes any message box that comes up while it lives, keeping what it
   said (one nobody expected fails the check after, instead of hanging it). */
struct BoxCloser
{
    QStringList said;
    QTimer timer;
    BoxCloser()
    {
        timer.setInterval(20);
        QObject::connect(&timer, &QTimer::timeout, [this]() {
            if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
            {
                said << box->text();
                box->reject();
            }
        });
        timer.start();
    }
};

/* One Art-Net DMX packet: its universe and its channels. */
bool parseArtDmx(const QByteArray &d, int &universe, QByteArray &data)
{
    if (d.size() < 18 || !d.startsWith(QByteArray("Art-Net\0", 8)))
        return false;
    if (uchar(d.at(8)) != 0x00 || uchar(d.at(9)) != 0x50)
        return false;                              // not OpDmx
    universe = (uchar(d.at(15)) << 8) | uchar(d.at(14));
    const int len = (uchar(d.at(16)) << 8) | uchar(d.at(17));
    data = d.mid(18, len);
    return true;
}

/* The LEDs lit in one universe's channels, @p per channels each, from 0. */
QList<int> litLeds(const QByteArray &data, int per, int leds)
{
    QList<int> out;
    for (int i = 0; i < leds && (i + 1) * per <= data.size(); ++i)
    {
        bool lit = false;
        for (int c = 0; c < per; ++c)
            lit = lit || data.at(i * per + c) != 0;
        if (lit)
            out << i;
    }
    return out;
}
}

void WledPixels_Test::aWledOfFiftyLedsIsFiftyRgbHeadsInOneUniverse()
{
    Doc doc(this);
    Fixture *fx = pixelWled(&doc, 50);
    QVERIFY(fx != nullptr);
    QVERIFY(WledFixture::isPixelMode(fx));
    QCOMPARE(fx->fixtureMode()->name(), QString("Pixels (Multi RGB)"));
    QCOMPARE(fx->fixtureDef()->manufacturer(), QString("WLED"));
    QCOMPARE(fx->fixtureDef()->model(), QString("Lantern"));
    QCOMPARE(fx->heads(), 50);
    QCOMPARE(fx->channels(), quint32(150));
    QVERIFY(!fx->crossUniverse());
    QCOMPARE(WledFixture::universeCount(fx), 1);
    QCOMPARE(WledFixture::dmxModeFor(fx), int(WledFixture::DmxMultiRgb));
    // LED 1 is channels 1-3, red, green, blue; LED 50 the last three.
    QCOMPARE(fx->head(0).channels(), QVector<quint32>({ 0, 1, 2 }));
    QCOMPARE(fx->head(49).channels(), QVector<quint32>({ 147, 148, 149 }));
    QCOMPARE(fx->channel(0)->colour(), QLCChannel::Red);
    QCOMPARE(fx->channel(1)->colour(), QLCChannel::Green);
    QCOMPARE(fx->channel(2)->colour(), QLCChannel::Blue);
    QCOMPARE(fx->channel(0)->group(), QLCChannel::Intensity);
    // No master dimmer: every channel belongs to an LED.
    QCOMPARE(fx->fixtureMode()->masterIntensityChannel(), QLCChannel::invalid());
    QVERIFY(doc.addFixture(fx));
}

void WledPixels_Test::aWledOfFourHundredLedsSpansThreeUniversesAsWledLaysThemOut()
{
    const WledFixture::UniverseSpan span = WledFixture::universeSpan(400, false, 0);
    QCOMPARE(span.leds, QVector<int>({ 170, 170, 60 }));
    QCOMPARE(span.channels, 512 + 512 + 180);

    Doc doc(this);
    Fixture *fx = pixelWled(&doc, 400);
    QVERIFY(fx != nullptr);
    QCOMPARE(fx->heads(), 400);
    QCOMPARE(fx->channels(), quint32(512 + 512 + 180));
    QVERIFY(fx->crossUniverse());
    // LED 171 starts the second universe on its first channel; channels
    // 511 and 512 of the first are left out, as WLED does.
    QCOMPARE(fx->head(169).channels(), QVector<quint32>({ 507, 508, 509 }));
    QCOMPARE(fx->head(170).channels(), QVector<quint32>({ 512, 513, 514 }));
    QCOMPARE(fx->head(340).channels(), QVector<quint32>({ 1024, 1025, 1026 }));
    QCOMPARE(fx->head(399).channels().last(), quint32(1024 + 179));
    QCOMPARE(fx->channel(510)->group(), QLCChannel::Nothing);

    fx->setUniverse(2);
    QVERIFY(doc.addFixture(fx, Fixture::invalidId(), fx->crossUniverse()));
    QCOMPARE(WledFixture::universeCount(fx), 3);
    QVERIFY(doc.inputOutputMap()->universesCount() >= 5);
    // A channel past its first universe is written to the next.
    QCOMPARE(doc.fixtureForAddress((3 << 9) | 0), fx->id());
    QCOMPARE(doc.fixtureForAddress((4 << 9) | 179), fx->id());
    QCOMPARE(doc.fixtureForAddress((4 << 9) | 180), Fixture::invalidId());
}

void WledPixels_Test::aWledFromALaterAddressStartsItsNextUniverseOnAnLed()
{
    // From address 100: (512 - 99) / 3 = 137 LEDs in the first universe.
    const WledFixture::UniverseSpan span = WledFixture::universeSpan(200, false, 99);
    QCOMPARE(span.leds, QVector<int>({ 137, 63 }));
    QCOMPARE(span.channels, (512 - 99) + 63 * 3);

    Doc doc(this);
    Fixture *fx = pixelWled(&doc, 200, false, "10.99.0.41", 99);
    QVERIFY(fx != nullptr);
    QCOMPARE(fx->channels(), quint32(span.channels));
    // LED 138 on the next universe's channel 1: 512 - 99 = 413 in.
    QCOMPARE(fx->head(137).channels().first(), quint32(413));
    QCOMPARE(fx->channelAddress(fx->head(137).channels().first()), quint32(512));

    // Moved to address 1, its gaps follow.
    fx->setAddress(0);
    WledFixture::refitPixels(fx);
    QCOMPARE(fx->head(170).channels().first(), quint32(512));
    QCOMPARE(fx->heads(), 200);

    // And the count it saves comes back from the footprint alone.
    QCOMPARE(WledFixture::ledsForChannels(int(fx->channels()), false, 0, true), 200);
    QCOMPARE(WledFixture::ledsForChannels(span.channels, false, 99, true), 200);
}

void WledPixels_Test::rgbwLedsTakeFourChannelsAndFillAUniverseExactly()
{
    const WledFixture::UniverseSpan span = WledFixture::universeSpan(300, true, 0);
    QCOMPARE(span.leds, QVector<int>({ 128, 128, 44 }));
    QCOMPARE(span.channels, 512 + 512 + 176);

    Doc doc(this);
    Fixture *fx = pixelWled(&doc, 300, true);
    QVERIFY(fx != nullptr);
    QCOMPARE(fx->fixtureMode()->name(), QString("Pixels (Multi RGBW)"));
    QVERIFY(WledFixture::isRgbw(fx));
    QCOMPARE(WledFixture::dmxModeFor(fx), int(WledFixture::DmxMultiRgbw));
    QCOMPARE(fx->head(0).channels().size(), 4);
    QCOMPARE(fx->channel(3)->colour(), QLCChannel::White);
    QCOMPARE(fx->head(128).channels().first(), quint32(512));
    QCOMPARE(fx->channels(), quint32(span.channels));
}

void WledPixels_Test::aPixelWledIsSavedAndLoadsBackAsItsLeds()
{
    QByteArray xml;
    {
        Doc doc(this);
        Fixture *fx = pixelWled(&doc, 400);
        QVERIFY(fx != nullptr);
        fx->setUniverse(1);
        QVERIFY(doc.addFixture(fx, Fixture::invalidId(), true));
        QXmlStreamWriter w(&xml);
        w.writeStartDocument();
        QVERIFY(fx->saveXML(&w));
        w.writeEndDocument();
    }
    QVERIFY(xml.contains("Pixels (Multi RGB)"));
    QVERIFY(xml.contains("<Model>Lantern</Model>"));

    Doc doc(this);
    QVERIFY(lanternDef(&doc) != nullptr);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(Fixture::loader(r, &doc));
    QCOMPARE(doc.fixtures().size(), 1);
    Fixture *back = doc.fixtures().first();
    QVERIFY(WledFixture::isPixelMode(back));
    QCOMPARE(back->heads(), 400);
    QCOMPARE(back->channels(), quint32(512 + 512 + 180));
    QVERIFY(back->crossUniverse());
    QVERIFY(back->isPatched());
    QCOMPARE(back->universe(), quint32(1));
    QCOMPARE(back->head(170).channels().first(), quint32(512));
    QCOMPARE(back->fixtureDef()->model(), QString("Lantern"));
}

void WledPixels_Test::anotherFixtureGivenItsDefinitionGetsACopyOfItsOwn()
{
    Doc doc(this);
    Fixture *a = pixelWled(&doc, 60);
    QVERIFY(a != nullptr);
    QVERIFY(doc.addFixture(a));
    Fixture *b = new Fixture(&doc);
    b->setFixtureDefinition(a->fixtureDef(), a->fixtureMode());
    QVERIFY(b->fixtureDef() != a->fixtureDef());
    QCOMPARE(b->heads(), 60);
    QCOMPARE(b->fixtureMode()->name(), a->fixtureMode()->name());
    b->setAddress(200);
    QVERIFY(doc.addFixture(b));
    // Deleting one leaves the other's whole.
    QVERIFY(doc.deleteFixture(a->id()));
    QCOMPARE(b->heads(), 60);
    QCOMPARE(b->channel(179)->name(), QString("Blue 60"));
}

void WledPixels_Test::aPixelWledIsAStringNotAnAimedLight()
{
    Doc doc(this);
    Fixture *fx = pixelWled(&doc, 30);
    QVERIFY(fx != nullptr);
    QVERIFY(doc.addFixture(fx));
    QVERIFY(WledFixture::isPixelString(fx));
    QVERIFY(!WledFixture::runsEffects(fx));     // its LEDs are computed here
    QVERIFY(WledConfigCheck::isWled(fx));
    QCOMPARE(WledConfigCheck::fixtureLedCount(&doc, fx), 30);
}

void WledPixels_Test::eachLedHeadSitsWhereThePixelPathDrawsIt()
{
    Doc doc(this);
    Fixture *fx = pixelWled(&doc, 40);
    QVERIFY(fx != nullptr);
    QVERIFY(doc.addFixture(fx));
    MonitorProperties *props = doc.monitorProperties();
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(2000, 3000, 1000));
    FixtureRigProps rp = props->fixtureRigProps(fx->id());

    // A straight run: along its body, one head after another.
    const QVector<QVector3D> row = RigHeads::headPositions(&doc, fx->id());
    QCOMPARE(row.size(), 40);

    // Wound round a wand: each head is its LED on the staff, the very points
    // the rig view draws.
    rp.pixelPath = PixelPath::Wand;
    rp.wandLength = 1.2f;
    props->setFixtureRigProps(fx->id(), rp);
    const QVector<QVector3D> wand = RigHeads::headPositions(&doc, fx->id());
    const QVector<QVector3D> drawn = PixelPath::positions(&doc, props, fx->id(), 40);
    QCOMPARE(drawn.size(), 40);
    QCOMPARE(wand, drawn);
    QVERIFY(wand != row);
    // Up the staff: the last LED well above the first.
    QVERIFY(wand.last().z() - wand.first().z() > 0.9f);
}

void WledPixels_Test::fixOnDeviceAsksForMultiRgbAndStreamsOn()
{
    Doc doc(this);
    doc.ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*artnet*"));
    QVERIFY2(doc.ioPluginCache()->plugin("ArtNet") != nullptr, "the Art-Net plugin did not load");
    InputOutputMap *io = doc.inputOutputMap();
    while (io->universesCount() < 4)
        io->addUniverse();
    QVERIFY(io->setOutputPatch(2, "ArtNet", "", 0));
    io->outputPatch(2)->setPluginParameter("outputUni", 15);

    Fixture *fx = pixelWled(&doc, 50);
    QVERIFY(fx != nullptr);
    fx->setUniverse(2);
    QVERIFY(doc.addFixture(fx));

    // A WLED as it comes for its own effects: Effect mode, streams off,
    // main segment only, its own brightness, a timeout of "never".
    QJsonObject cfg = liveCfg(false, 5568, 1, 1, 3);
    QJsonObject live = cfg["if"].toObject()["live"].toObject();
    live["mso"] = true;
    live["maxbri"] = false;
    live["timeout"] = 255;
    cfg["if"] = QJsonObject{ { "live", live } };

    const WledConfigCheck::Finding f = WledConfigCheck::evaluate(&doc, fx, cfg);
    const QJsonObject want = f.fix.value("if").toObject().value("live").toObject();
    QCOMPARE(want.value("en").toBool(), true);
    QCOMPARE(want.value("port").toInt(), 6454);
    QCOMPARE(want.value("dmx").toObject().value("mode").toInt(), 4);     // WLED's Multi RGB
    QCOMPARE(want.value("dmx").toObject().value("uni").toInt(), 15);
    QVERIFY(!want.value("dmx").toObject().contains("addr"));              // it starts at 1 already
    QCOMPARE(want.value("mso").toBool(true), false);
    QCOMPARE(want.value("maxbri").toBool(), true);
    QCOMPARE(want.value("timeout").toInt(), 25);
    const QString said = f.changes.join("; ");
    QVERIFY2(said.contains("Multi RGB"), qPrintable(said));
    QVERIFY2(said.contains("Receive realtime streams: off → on"), qPrintable(said));

    // Set up as patched: nothing to say.
    QVERIFY(WledConfigCheck::evaluate(&doc, fx, liveCfg(true, 6454, 15, 1, 4)).problems.isEmpty());
}

void WledPixels_Test::fixOnDeviceAsksForMultiRgbwForAnRgbwString()
{
    Doc doc(this);
    Fixture *fx = pixelWled(&doc, 50, true);
    QVERIFY(fx != nullptr);
    QVERIFY(doc.addFixture(fx));
    const WledConfigCheck::Finding f = WledConfigCheck::evaluate(&doc, fx, liveCfg(true, 6454, 0, 1, 4));
    QCOMPARE(f.fix.value("if").toObject().value("live").toObject()
                 .value("dmx").toObject().value("mode").toInt(), 6);     // Multi RGBW
}

void WledPixels_Test::oneDeviceDrivenBothWaysIsSaidAndNeverFlipped()
{
    Doc doc(this);
    Fixture *pixels = pixelWled(&doc, 50, false, "10.99.0.42");
    QVERIFY(pixels != nullptr);
    QVERIFY(doc.addFixture(pixels));
    Fixture *api = new Fixture(&doc);
    api->setName("Lantern by API");
    QLCFixtureDef *def = lanternDef(&doc);
    api->setFixtureDefinition(def, def->modes().first());
    api->setConnection(Fixture::Network);
    api->setTransport(Fixture::WledTransport);
    api->setIPAddress("10.99.0.42");
    api->setPatched(false);
    QVERIFY(doc.addFixture(api));

    // Streams on: the API one is told why its looks are ignored, and the
    // setting is not turned off under the pixel one.
    WledConfigCheck::Finding f = WledConfigCheck::evaluate(&doc, api, liveCfg(true, 6454, 0, 1, 4));
    QVERIFY2(f.problems.join(";").contains("Keep one of the two"), qPrintable(f.problems.join(";")));
    QVERIFY(!f.fix.value("if").toObject().value("live").toObject().contains("en"));
    // Streams off: the pixel one is told, and not turned on under the other.
    f = WledConfigCheck::evaluate(&doc, pixels, liveCfg(false, 6454, 0, 1, 4));
    QVERIFY2(f.problems.join(";").contains("Lantern by API"), qPrintable(f.problems.join(";")));
    QVERIFY(!f.fix.value("if").toObject().value("live").toObject().contains("en"));
}

void WledPixels_Test::routingGivesASpanningWledItsOwnRunOfArtNetUniverses()
{
    Doc doc(this);
    doc.ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*artnet*"));
    QVERIFY(doc.ioPluginCache()->plugin("ArtNet") != nullptr);
    InputOutputMap *io = doc.inputOutputMap();
    while (io->universesCount() < 2)
        io->addUniverse();
    // Universe 1: a rig going out to its node (never sent to here).
    QVERIFY(io->setOutputPatch(0, "ArtNet", "", 0));
    io->outputPatch(0)->setPluginParameter("outputIP", "10.99.0.10");
    io->outputPatch(0)->setPluginParameter("outputUni", 7);
    Fixture *par = new Fixture(&doc);
    par->setName("Par");
    par->setChannels(6);
    QVERIFY(doc.addFixture(par));

    Fixture *fx = pixelWled(&doc, 400, false, "10.99.0.43");
    QVERIFY(fx != nullptr);
    fx->setPatched(false);
    QVERIFY(doc.addFixture(fx));
    QCOMPARE(PatchPlanner::wledRoute(&doc, fx), PatchPlanner::WledNotPatched);
    const quint32 before = io->universesCount();

    PatchPlanner planner(&doc);
    QStringList problems;
    std::function<void()> undo;
    QCOMPARE(planner.routeWled({ fx->id() }, &problems, &undo), 1);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("\n")));
    QVERIFY(fx->isPatched());
    QCOMPARE(fx->address(), quint32(0));
    QVERIFY(fx->universe() >= 1);              // never the rig's universe
    const quint32 first = fx->universe();
    for (quint32 k = 0; k < 3; ++k)
    {
        OutputPatch *op = io->outputPatch(first + k);
        QVERIFY(op != nullptr);
        QCOMPARE(op->pluginName(), QString("ArtNet"));
        QCOMPARE(op->getPluginParameters().value("outputIP").toString(), QString("10.99.0.43"));
        QCOMPARE(PatchPlanner::networkUniverseNumber(&doc, first + k), int(first + k));
    }
    QCOMPARE(PatchPlanner::wledRoute(&doc, fx), PatchPlanner::WledRouted);
    // The rig's universe as it was.
    QCOMPARE(io->outputPatch(0)->getPluginParameters().value("outputIP").toString(), QString("10.99.0.10"));
    QCOMPARE(par->universe(), quint32(0));

    // One undo puts it all back.
    QVERIFY(bool(undo));
    undo();
    QVERIFY(!fx->isPatched());
    QCOMPARE(io->universesCount(), before);
}

void WledPixels_Test::universesOutOfOrderOrThroughTheWledOutputAreFound()
{
    Doc doc(this);
    doc.ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*artnet*"));
    QVERIFY(doc.ioPluginCache()->plugin("ArtNet") != nullptr);
    InputOutputMap *io = doc.inputOutputMap();
    while (io->universesCount() < 4)
        io->addUniverse();
    Fixture *fx = pixelWled(&doc, 300, false, "10.99.0.44");
    QVERIFY(fx != nullptr);
    fx->setUniverse(2);
    QVERIFY(doc.addFixture(fx, Fixture::invalidId(), true));

    QCOMPARE(PatchPlanner::wledRoute(&doc, fx), PatchPlanner::WledNoOutput);
    QVERIFY(io->setOutputPatch(2, "ArtNet", "", 0));
    QVERIFY(io->setOutputPatch(3, "ArtNet", "", 0));
    io->outputPatch(2)->setPluginParameter("outputUni", 20);
    io->outputPatch(3)->setPluginParameter("outputUni", 9);
    QString detail;
    QCOMPARE(PatchPlanner::wledRoute(&doc, fx, &detail), PatchPlanner::WledOutOfOrder);
    QCOMPARE(detail, QString("20, 9"));
    // Said once, by wled.route: never also as an Art-Net fixture (network.route).
    QCOMPARE(PatchPlanner::networkRoute(&doc, fx), PatchPlanner::NetRouted);
    io->outputPatch(3)->setPluginParameter("outputUni", 21);
    QCOMPARE(PatchPlanner::wledRoute(&doc, fx), PatchPlanner::WledRouted);
    io->outputPatch(3)->setPluginParameter("outputIP", "10.99.0.99");
    QCOMPARE(PatchPlanner::wledRoute(&doc, fx, &detail), PatchPlanner::WledOtherIp);
    QCOMPARE(detail, QString("10.99.0.99"));

    /* A universe sent to another device is never re-aimed: it moves to a run
       of its own (universes 1 and 2 here, free), and that one is left. */
    PatchPlanner planner(&doc);
    QCOMPARE(planner.routeWled({ fx->id() }), 1);
    QCOMPARE(fx->universe(), quint32(0));
    QCOMPARE(PatchPlanner::wledRoute(&doc, fx), PatchPlanner::WledRouted);
    QCOMPARE(io->outputPatch(3)->getPluginParameters().value("outputIP").toString(), QString("10.99.0.99"));

    // Its own run with one out of order: put right where it is.
    io->outputPatch(1)->setPluginParameter("outputUni", 40);
    QCOMPARE(PatchPlanner::wledRoute(&doc, fx), PatchPlanner::WledOutOfOrder);
    QCOMPARE(planner.routeWled({ fx->id() }), 1);
    QCOMPARE(fx->universe(), quint32(0));
    QCOMPARE(PatchPlanner::networkUniverseNumber(&doc, 1), PatchPlanner::networkUniverseNumber(&doc, 0) + 1);
    QCOMPARE(PatchPlanner::wledRoute(&doc, fx), PatchPlanner::WledRouted);
}

void WledPixels_Test::addFixtureOffersPixelsAndReadsTheLedCountFromTheDevice()
{
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    wled.info["leds"] = QJsonObject{ { "count", 60 }, { "lc", 1 }, { "maxseg", 32 } };

    Doc doc(this);
    doc.ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*artnet*"));
    QVERIFY(doc.ioPluginCache()->plugin("ArtNet") != nullptr);
    QVERIFY(lanternDef(&doc) != nullptr);
    QWidget parent;

    QString offered, info;
    driveAddDialog([&](AddFixture *dlg) {
        // WLED > Lantern, on the network by Art-Net, at the device's IP.
        QList<QTreeWidgetItem *> found = dlg->m_tree->findItems("Lantern", Qt::MatchExactly | Qt::MatchRecursive);
        QVERIFY(!found.isEmpty());
        dlg->m_tree->setCurrentItem(found.first());
        // Not on DMX: the pixel modes come with the network.
        QCOMPARE(dlg->m_modeCombo->findText("Pixels (Multi RGB)"), -1);
        dlg->m_connectionCombo->setCurrentIndex(dlg->m_connectionCombo->findData(
            AddFixture::connectionKey(Fixture::Network, Fixture::ArtNetTransport)));
        dlg->m_ipEdit->setText(wled.address());
        const int idx = dlg->m_modeCombo->findText("Pixels (Multi RGB)");
        QVERIFY(idx > 0);
        offered = dlg->m_modeCombo->itemText(idx);
        dlg->m_modeCombo->setCurrentIndex(idx);
        dlg->slotModeActivated(idx);
        // The count, read from the device.
        QTRY_COMPARE(dlg->m_ledsSpin->value(), 60);
        QVERIFY(dlg->m_ledsSpin->isVisibleTo(dlg));
        info = dlg->m_ledsInfoLabel->text();
        QCOMPARE(dlg->pixelLeds(), 60);
        QVERIFY(!dlg->pixelRgbw());
        QVERIFY(dlg->m_ownUniverseLabel->isVisibleTo(dlg));
    });
    BoxCloser boxes;
    const QList<quint32> made = FixtureCreator::addFromDialog(&parent, &doc, false, 0, nullptr);
    QVERIFY2(boxes.said.isEmpty(), qPrintable(boxes.said.join("\n")));
    QCOMPARE(offered, QString("Pixels (Multi RGB)"));
    QVERIFY2(info.contains("Read from the WLED: 60 LEDs"), qPrintable(info));
    QCOMPARE(made.size(), 1);
    Fixture *fx = doc.fixture(made.first());
    QVERIFY(fx != nullptr);
    QVERIFY(WledFixture::isPixelMode(fx));
    QCOMPARE(fx->heads(), 60);
    QCOMPARE(fx->connection(), Fixture::Network);
    QCOMPARE(fx->transport(), Fixture::ArtNetTransport);
    // On a universe of its own, out as Art-Net (UX-67): reachable.
    QVERIFY(fx->isPatched());
    OutputPatch *op = doc.inputOutputMap()->outputPatch(fx->universe());
    QVERIFY(op != nullptr);
    QCOMPARE(op->pluginName(), QString("ArtNet"));
    QCOMPARE(PatchPlanner::wledRoute(&doc, fx), PatchPlanner::WledRouted);
}

void WledPixels_Test::aChaseGoesOutAsArtNetBytesPerLedAcrossTwoUniverses()
{
    /* The real Art-Net output, aimed at this computer as Art-Net 0:0:15 and
       0:0:16: a One By One chase on a 200-LED string, plus a scene lighting
       LED 181 blue -- on the string's second universe, its 11th LED. */
    QUdpSocket sink;
    QVERIFY(sink.bind(QHostAddress::LocalHost, 6454, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint));

    Doc doc(this);
    doc.ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*artnet*"));
    QLCIOPlugin *artnet = doc.ioPluginCache()->plugin("ArtNet");
    QVERIFY(artnet != nullptr);
    const int line = artnet->outputs().indexOf("127.0.0.1");
    QVERIFY2(line >= 0, qPrintable(artnet->outputs().join(", ")));
    QVERIFY(doc.rgbScriptsCache()->load(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "rgbscripts")));

    InputOutputMap *io = doc.inputOutputMap();
    while (io->universesCount() < 2)
        io->addUniverse();
    Fixture *fx = pixelWled(&doc, 200, false, "127.0.0.1");
    QVERIFY(fx != nullptr);
    QVERIFY(doc.addFixture(fx, Fixture::invalidId(), true));
    for (quint32 k = 0; k < 2; ++k)
    {
        QVERIFY(io->setOutputPatch(k, "ArtNet", "127.0.0.1", quint32(line)));
        io->outputPatch(k)->setPluginParameter("outputIP", "127.0.0.1");
        io->outputPatch(k)->setPluginParameter("outputUni", 15 + int(k));
    }
    QCOMPARE(PatchPlanner::wledRoute(&doc, fx), PatchPlanner::WledRouted);

    FixtureGroup *grp = new FixtureGroup(&doc);
    grp->setName("String");
    grp->setSize(QSize(200, 1));
    QVERIFY(doc.addFixtureGroup(grp));
    QVERIFY(grp->assignFixture(fx->id()));
    QCOMPARE(grp->headsMap().size(), 200);

    RGBMatrix *mtx = new RGBMatrix(&doc);
    mtx->setFixtureGroup(grp->id());
    RGBAlgorithm *algo = RGBAlgorithm::algorithm(&doc, "One By One");
    QVERIFY(algo != nullptr);
    mtx->setAlgorithm(algo);
    mtx->setColor(0, QColor(255, 0, 0));
    mtx->setDuration(60);
    QVERIFY(doc.addFunction(mtx));

    Scene *scene = new Scene(&doc);
    const QVector<quint32> led181 = fx->head(180).channels();
    scene->setValue(fx->id(), led181.at(2), 255);
    QVERIFY(doc.addFunction(scene));

    io->startUniverses();
    doc.masterTimer()->start();
    mtx->start(doc.masterTimer(), FunctionParent::master());
    scene->start(doc.masterTimer(), FunctionParent::master());

    QSet<int> chased;
    int frames15 = 0, bad15 = 0;
    QByteArray last16;
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 2000)
    {
        QTest::qWait(10);
        while (sink.hasPendingDatagrams())
        {
            const QNetworkDatagram d = sink.receiveDatagram();
            int uni = -1;
            QByteArray data;
            if (!parseArtDmx(d.data(), uni, data))
                continue;
            QVERIFY2(uni != 0, "nothing goes out as Art-Net universe 0:0:0");
            if (uni == 15)
            {
                const QList<int> lit = litLeds(data, 3, 170);
                if (lit.isEmpty())
                    continue;
                ++frames15;
                // One LED at a time, red, and the two channels past LED 170 unused.
                if (lit.size() != 1 || uchar(data.at(lit.first() * 3)) != 255
                    || data.at(lit.first() * 3 + 1) != 0 || data.at(lit.first() * 3 + 2) != 0
                    || (data.size() >= 512 && (data.at(510) != 0 || data.at(511) != 0)))
                    ++bad15;
                chased.insert(lit.first());
            }
            else if (uni == 16)
                last16 = data;
        }
    }
    mtx->stop(FunctionParent::master());
    scene->stop(FunctionParent::master());
    doc.masterTimer()->stop();

    qInfo().noquote() << "art-net 0:0:15 frames with a lit LED:" << frames15 << "LEDs chased:" << chased.size();
    QVERIFY(frames15 > 5);
    QCOMPARE(bad15, 0);
    QVERIFY2(chased.size() >= 5, qPrintable(QString::number(chased.size())));
    // The second universe: LED 181 is its 11th, from its first channel.
    QVERIFY(last16.size() >= 33);
    QCOMPARE(uchar(last16.at(30)), uchar(0));
    QCOMPARE(uchar(last16.at(31)), uchar(0));
    QCOMPARE(uchar(last16.at(32)), uchar(255));
    QCOMPARE(litLeds(last16, 3, 30), QList<int>({ 10 }));
}

void WledPixels_Test::aRealOlaProbeSeesTheChase()
{
    /* On the show VLAN only, by hand: WLEDPIXELS_OLA=172.18.2.218 sends a
       50-LED chase as Art-Net 0:0:15 unicast to that OLA probe and reads its
       universe 15 back (http://<ip>:9090/get_dmx?u=15). Never universe 0. */
    const QString ola = qEnvironmentVariable("WLEDPIXELS_OLA");
    if (ola.isEmpty())
        QSKIP("WLEDPIXELS_OLA not set: the real-wire check runs by hand on the show VLAN");

    Doc doc(this);
    doc.ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*artnet*"));
    QLCIOPlugin *artnet = doc.ioPluginCache()->plugin("ArtNet");
    QVERIFY(artnet != nullptr);
    const QString net = ola.section('.', 0, 2) + '.';
    int line = -1;
    for (int i = 0; i < artnet->outputs().size(); ++i)
        if (artnet->outputs().at(i).startsWith(net))
            line = i;
    QVERIFY2(line >= 0, qPrintable("no interface on " + net + "x: " + artnet->outputs().join(", ")));
    QVERIFY(doc.rgbScriptsCache()->load(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "rgbscripts")));

    InputOutputMap *io = doc.inputOutputMap();
    Fixture *fx = pixelWled(&doc, 50, false, ola);
    QVERIFY(fx != nullptr);
    QVERIFY(doc.addFixture(fx));
    QVERIFY(io->setOutputPatch(0, "ArtNet", artnet->outputs().at(line), quint32(line)));
    io->outputPatch(0)->setPluginParameter("outputIP", ola);
    io->outputPatch(0)->setPluginParameter("outputUni", 15);
    QCOMPARE(PatchPlanner::networkUniverseNumber(&doc, 0), 15);

    FixtureGroup *grp = new FixtureGroup(&doc);
    grp->setSize(QSize(50, 1));
    QVERIFY(doc.addFixtureGroup(grp));
    QVERIFY(grp->assignFixture(fx->id()));
    RGBMatrix *mtx = new RGBMatrix(&doc);
    mtx->setFixtureGroup(grp->id());
    mtx->setAlgorithm(RGBAlgorithm::algorithm(&doc, "One By One"));
    mtx->setColor(0, QColor(255, 64, 0));
    mtx->setDuration(200);
    QVERIFY(doc.addFunction(mtx));

    io->startUniverses();
    doc.masterTimer()->start();
    mtx->start(doc.masterTimer(), FunctionParent::master());

    QNetworkAccessManager nam;
    nam.setProxy(QNetworkProxy::NoProxy);
    QSet<int> seen;
    int samples = 0, good = 0;
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 6000)
    {
        QNetworkReply *r = nam.get(QNetworkRequest(QUrl(QString("http://%1:9090/get_dmx?u=15").arg(ola))));
        QTRY_VERIFY_WITH_TIMEOUT(r->isFinished(), 3000);
        const QJsonArray dmx = QJsonDocument::fromJson(r->readAll()).object().value("dmx").toArray();
        r->deleteLater();
        if (dmx.size() < 150)
            continue;
        ++samples;
        QList<int> lit;
        for (int i = 0; i < 50; ++i)
            if (dmx.at(i * 3).toInt() || dmx.at(i * 3 + 1).toInt() || dmx.at(i * 3 + 2).toInt())
                lit << i;
        if (lit.size() == 1 && dmx.at(lit.first() * 3).toInt() == 255
            && dmx.at(lit.first() * 3 + 1).toInt() == 64 && dmx.at(lit.first() * 3 + 2).toInt() == 0)
        {
            ++good;
            seen.insert(lit.first());
        }
        QTest::qWait(150);
    }
    mtx->stop(FunctionParent::master());
    doc.masterTimer()->stop();
    qInfo().noquote() << "OLA" << ola << "universe 15: samples" << samples << "one LED lit (255,64,0)"
                      << good << "distinct LEDs" << seen.size();
    QVERIFY(samples > 5);
    QVERIFY(good * 10 >= samples * 8);
    QVERIFY(seen.size() >= 5);
}

void WledPixels_Test::shotsOfAddFixtureAndFixOnDevice()
{
    /* For review, by hand: WLEDPIXELS_SHOTS=<dir> grabs Add Fixture in the
       Pixels mode at three sizes and Fix on Device's confirmation. */
    const QString dir = qEnvironmentVariable("WLEDPIXELS_SHOTS");
    if (dir.isEmpty())
        QSKIP("WLEDPIXELS_SHOTS not set");
    QDir().mkpath(dir);

    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    wled.info["leds"] = QJsonObject{ { "count", 400 }, { "lc", 1 }, { "maxseg", 32 } };
    QJsonObject cfg = liveCfg(false, 5568, 1, 1, 3);
    QJsonObject live = cfg["if"].toObject()["live"].toObject();
    live["maxbri"] = false;
    cfg["if"] = QJsonObject{ { "live", live } };
    wled.cfg = cfg;

    Doc doc(this);
    doc.ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*artnet*"));
    QVERIFY(lanternDef(&doc) != nullptr);

    const QList<QSize> sizes = { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
    for (const QSize &sz : sizes)
    {
        AddFixture af(nullptr, &doc);
        af.resize(qMin(sz.width(), 1100), sz.height() - 60);
        const QList<QTreeWidgetItem *> found = af.m_tree->findItems("Lantern", Qt::MatchExactly | Qt::MatchRecursive);
        QVERIFY(!found.isEmpty());
        af.m_tree->setCurrentItem(found.first());
        af.m_connectionCombo->setCurrentIndex(af.m_connectionCombo->findData(
            AddFixture::connectionKey(Fixture::Network, Fixture::ArtNetTransport)));
        af.m_ipEdit->setText(wled.address());
        const int idx = af.m_modeCombo->findText("Pixels (Multi RGB)");
        af.m_modeCombo->setCurrentIndex(idx);
        af.slotModeActivated(idx);
        af.show();
        QTRY_COMPARE(af.m_ledsSpin->value(), 400);
        QTest::qWait(50);
        af.grab().save(QDir(dir).filePath(QString("addfixture-pixels-%1x%2.png").arg(sz.width()).arg(sz.height())));
    }

    // Fix on Device: read from the device, the changes shown before a write.
    Fixture *fx = pixelWled(&doc, 400, false, wled.address());
    QVERIFY(fx != nullptr);
    fx->setPatched(false);
    QVERIFY(doc.addFixture(fx));
    PatchPlanner planner(&doc);
    QVERIFY(planner.routeWled({ fx->id() }) == 1);
    WledConfigCheck check(&doc);
    check.checkNow();
    QTRY_VERIFY(!check.finding(fx->id()).problems.isEmpty());
    QWidget parent;
    parent.resize(1280, 800);
    int n = 0;
    QTimer grabber;
    grabber.setInterval(30);
    connect(&grabber, &QTimer::timeout, [&]() {
        if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
        {
            QTest::qWait(30);
            box->grab().save(QDir(dir).filePath(QString("fixondevice-pixels-%1.png").arg(++n)));
            box->reject();
        }
    });
    grabber.start();
    check.confirmFix(&parent, { fx->id() });
    qInfo().noquote() << "shots in" << dir << "finding:" << check.finding(fx->id()).changes.join("; ");
    QVERIFY(n >= 1);
}

QTEST_MAIN(WledPixels_Test)

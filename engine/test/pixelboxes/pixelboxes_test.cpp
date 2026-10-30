/*
  Q Light Controller Plus
  pixelboxes_test.cpp

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

#include <QRegularExpression>
#include <QElapsedTimer>
#include <QtTest>
#include <QFile>
#include <QDir>

#include "pixelboxes_test.h"
#include "showtreecontroller.h"
#include "qlcfixturedefcache.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "mastertimer.h"
#include "showcontrol.h"
#include "showtreemodel.h"
#include "qlcpalette.h"
#include "universe.h"
#include "qlcfile.h"
#include "fixture.h"
#include "doc.h"

#include "resource_paths.h"

using namespace ShowTree;

namespace
{
const QString kClassroom = QStringLiteral(COURSE_DIR) + QStringLiteral("/shows/ShowChoir-Classroom.qlcc");
const QString kScripts = QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("effectscripts/");
/* At most this many samples of what plays: a test stops sampling once it
   has seen what it looks for, so a loaded machine just takes longer. */
const int kSamples = 50;

Target groupTarget(quint32 g)
{
    Target t;
    t.groups << g;
    return t;
}

} // namespace

void PixelBoxes_Test::init()
{
    m_doc = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir));

    // The show's own definitions (the XL-450, the two-arm beam ...), as the
    // app reads them from it -- in memory only, nothing written anywhere.
    QFile f(kClassroom);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(f.readAll());
    f.close();
    static const QRegularExpression cdata(QStringLiteral("<FixtureDefinition [^>]*><!\\[CDATA\\[(.*?)\\]\\]>"),
                                          QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator it = cdata.globalMatch(text);
    int defs = 0;
    while (it.hasNext())
    {
        QLCFixtureDef *def = new QLCFixtureDef();
        QVERIFY(def->loadXMLData(it.next().captured(1).toUtf8()));
        m_doc->fixtureDefCache()->addFixtureDef(def);
        ++defs;
    }
    QCOMPARE(defs, 5);

    // Its thirteen universes (the app reads them before the engine).
    while (m_doc->inputOutputMap()->universesCount() < 13)
        QVERIFY(m_doc->inputOutputMap()->addUniverse());

    QXmlStreamReader *xml = QLCFile::getXMLReader(kClassroom);
    QVERIFY(xml != nullptr);
    while (!xml->atEnd())
        if (xml->readNext() == QXmlStreamReader::DTD)
            break;
    QVERIFY(xml->readNextStartElement());
    while (xml->readNextStartElement())
    {
        if (xml->name() == QLatin1String("Engine"))
            m_doc->loadXML(*xml, false);
        else
            xml->skipCurrentElement();
    }
    QLCFile::releaseXMLReader(xml);
    QVERIFY(fixture("SR Truss Wash 1") != nullptr);
    m_doc->inputOutputMap()->startUniverses();
    m_doc->masterTimer()->start();
}

void PixelBoxes_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    run(40);
    m_doc->masterTimer()->stop();
    delete m_doc;
    m_doc = nullptr;
}

quint32 PixelBoxes_Test::group(const QString &name) const
{
    for (FixtureGroup *g : m_doc->fixtureGroups())
        if (g->name() == name)
            return g->id();
    return FixtureGroup::invalidId();
}

Fixture *PixelBoxes_Test::fixture(const QString &name) const
{
    for (Fixture *fx : m_doc->fixtures())
        if (fx->name() == name)
            return fx;
    return nullptr;
}

quint32 PixelBoxes_Test::palette(const QString &name) const
{
    for (QLCPalette *p : m_doc->palettes())
        if (p->name() == name)
            return p->id();
    return QLCPalette::invalidId();
}

quint32 PixelBoxes_Test::effect(const QString &script, const QString &name)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Effect, m_doc);
    p->setName(name);
    p->setScriptPath(kScripts + script);
    m_doc->addPalette(p);
    return p->id();
}

quint32 PixelBoxes_Test::oneCue()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction(QStringLiteral("Course"));
    const quint32 list = st->addCueList(prod, QStringLiteral("Main"));
    st->setCurrentProduction(prod);
    return st->addCue(list, -1, QStringLiteral("One"));
}

void PixelBoxes_Test::go()
{
    QVERIFY(m_doc->showControl()->execute({ ShowControl::Go, QString(), QString() }));
}

/* @a ms of show time: the master timer on its own thread and the
   effects on the event loop, as the app runs them. */
void PixelBoxes_Test::run(int ms)
{
    QTest::qWait(ms);
}

QByteArray PixelBoxes_Test::sends(const QString &name, int from, int n) const
{
    Fixture *fx = fixture(name);
    if (fx == nullptr)
        return QByteArray();
    QList<Universe *> us = m_doc->inputOutputMap()->claimUniverses();
    const QByteArray v = us.at(int(fx->universe()))->preGMValues().mid(int(fx->address()) + from, n);
    m_doc->inputOutputMap()->releaseUniverses(false);
    return v;
}

/****************************************************************************
 * #15: the truss boxes' white pixels
 ****************************************************************************/

/* The XL-450's 30 white pixels (channels 135-164) light from a look on
   TrussW-W, on the whole Trusses group, with or without a dimmer, scaled
   by it -- and a colour with no white in it leaves them dark. */
void PixelBoxes_Test::whitePixelsLightFromALook_data()
{
    QTest::addColumn<QString>("group");
    QTest::addColumn<QStringList>("looks");
    QTest::addColumn<int>("white");
    QTest::addColumn<int>("red");       // pixel 1's red: the RGB pixels
    QTest::newRow("White and Full") << "TrussW-W" << QStringList({ "White", "Full" }) << 255 << 0;
    QTest::newRow("White and Half") << "TrussW-W" << QStringList({ "White", "Half" }) << 128 << 0;
    QTest::newRow("Trusses colour White") << "Trusses" << QStringList({ "White" }) << 255 << 255;
    QTest::newRow("Warm White") << "TrussW-W" << QStringList({ "Warm White", "Full" }) << 0x82 << 0;
    QTest::newRow("Red") << "Trusses" << QStringList({ "Red", "Full" }) << 0 << 255;
    // decided: no dimmer, so the colour is the light -- as on the RGB pixels.
    QTest::newRow("Full alone") << "Trusses" << QStringList({ "Full" }) << 0 << 0;
}

void PixelBoxes_Test::whitePixelsLightFromALook()
{
    QFETCH(QString, group);
    QFETCH(QStringList, looks);
    QFETCH(int, white);
    QFETCH(int, red);
    ShowTreeController *st = m_doc->showTree();
    const quint32 cue = oneCue();
    const quint32 layer = st->addLayer(cue, groupTarget(this->group(group)));
    for (const QString &l : looks)
        QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(palette(l))));
    go();
    run(200);
    const QByteArray w = sends("SR Truss Wash 1", 135, 30);
    for (char c : w)
        QCOMPARE(int(uchar(c)), white);
    QCOMPARE(int(uchar(sends("SR Truss Wash 1", 0, 1).at(0))), red);
}

/****************************************************************************
 * #17: effects on the truss pixel boxes
 ****************************************************************************/

/* What one channel of each named fixture does over @a ms: how many
   different values it took. */
static int distinct(const QList<QByteArray> &samples, int at)
{
    QSet<int> seen;
    for (const QByteArray &s : samples)
        if (at < s.size())
            seen.insert(uchar(s.at(at)));
    return seen.size();
}

void PixelBoxes_Test::dimmerEffectsMoveThePixelBoxes_data()
{
    QTest::addColumn<QString>("script");
    QTest::addColumn<QString>("group");
    QTest::addColumn<QString>("fixture");
    QTest::newRow("Sparkle") << QStringLiteral("sparkle.js") << "TrussW-RGB" << "SR Truss Wash 1";
    QTest::newRow("Dimmer Phaser") << QStringLiteral("dimmer-phaser.js") << "TrussW-RGB" << "SR Truss Wash 1";
    QTest::newRow("Phaser on the step tops") << QStringLiteral("dimmer-phaser.js") << "StepTops" << "US Step 1 Top SR";
}

void PixelBoxes_Test::dimmerEffectsMoveThePixelBoxes()
{
    QFETCH(QString, script);
    QFETCH(QString, group);
    QFETCH(QString, fixture);
    ShowTreeController *st = m_doc->showTree();
    const quint32 fx = effect(script, script);
    const quint32 cue = oneCue();
    const quint32 layer = st->addLayer(cue, groupTarget(this->group(group)));
    QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(palette("Blue"))));
    QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(palette("Full"))));
    QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(fx)));
    go();
    // Sampled until enough has moved (a loaded machine ticks less often).
    QList<QByteArray> samples;
    int moving = 0;
    for (int i = 0; i < kSamples && moving < 3; ++i)
    {
        run(60);
        samples << sends(fixture, 0, 135);
        moving = 0;
        for (int px = 0; px < 45; ++px)
            if (distinct(samples, px * 3 + 2) > 2)
                ++moving;
    }
    QVERIFY2(moving >= 3, qPrintable(QString("%1 blue pixels moved; last %2")
                                     .arg(moving).arg(QString::fromLatin1(samples.last().toHex(' ')))));
    // Never another colour: only the blue moves.
    for (const QByteArray &s : samples)
        QCOMPARE(int(uchar(s.at(0))), 0);
}

/* Two layers, the same effect palette on each, or one effect on the cue:
   the boxes and the step tops both move. */
void PixelBoxes_Test::oneEffectOverSeveralLayersMovesThePixelBoxes()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 fx = effect(QStringLiteral("dimmer-phaser.js"), QStringLiteral("Phaser"));
    const quint32 cue = oneCue();
    for (const QString &g : { QStringLiteral("TrussW-RGB"), QStringLiteral("StepTops") })
    {
        const quint32 layer = st->addLayer(cue, groupTarget(group(g)));
        QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(palette("Blue"))));
        // Each layer's own level: the boxes at Full, the step tops at 25%.
        QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(palette(g == "StepTops" ? "25%" : "Full"))));
        QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(fx)));
    }
    go();
    QList<QByteArray> box, top;
    int topMoving = 0;
    for (int i = 0; i < kSamples && (topMoving == 0 || distinct(box, 2) <= 2 || i < 12); ++i)
    {
        run(60);
        box << sends("SR Truss Wash 1", 0, 3);
        top << sends("US Step 1 Top SR", 0, 64);
        topMoving = 0;
        for (int c = 0; c < 64; ++c)
            if (distinct(top, c) > 2)
                ++topMoving;
    }
    QVERIFY2(distinct(box, 2) > 2, qPrintable(QString::fromLatin1(box.last().toHex(' '))));
    QVERIFY2(topMoving > 0, qPrintable(QString::fromLatin1(top.last().toHex(' '))));
    // Each dimmed from its own layer's level.
    int boxTop = 0, stepTop = 0;
    for (const QByteArray &b : box)
        boxTop = qMax(boxTop, int(uchar(b.at(2))));
    for (const QByteArray &t : top)
        stepTop = qMax(stepTop, int(uchar(t.at(2))));
    QVERIFY2(boxTop > 200, qPrintable(QString::number(boxTop)));
    QVERIFY2(stepTop > 0 && stepTop <= 70, qPrintable(QString::number(stepTop)));
}

void PixelBoxes_Test::aCueWideEffectMovesThePixelBoxes()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 fx = effect(QStringLiteral("dimmer-phaser.js"), QStringLiteral("Phaser"));
    const quint32 cue = oneCue();
    for (const QString &g : { QStringLiteral("TrussW-RGB"), QStringLiteral("StepTops") })
    {
        const quint32 layer = st->addLayer(cue, groupTarget(group(g)));
        QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(palette("Blue"))));
        QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(palette("Full"))));
    }
    QVERIFY(st->setCueLook(cue, st->lookForPalette(fx)));
    go();
    QList<QByteArray> box, top;
    int topMoving = 0;
    for (int i = 0; i < kSamples && (topMoving == 0 || distinct(box, 2) <= 2 || i < 12); ++i)
    {
        run(60);
        box << sends("SR Truss Wash 1", 0, 3);
        top << sends("US Step 1 Top SR", 0, 64);
        topMoving = 0;
        for (int c = 0; c < 64; ++c)
            if (distinct(top, c) > 2)
                ++topMoving;
    }
    QVERIFY2(distinct(box, 2) > 2, qPrintable(QString::fromLatin1(box.last().toHex(' '))));
    QVERIFY2(topMoving > 0, qPrintable(QString::fromLatin1(top.last().toHex(' '))));
}

/* A Colour Chase's speed is in fixtures a second: over the four truss
   boxes (180 pixels) at 3 a second, the band is on another box within
   a second -- not a pixel or two further on. */
void PixelBoxes_Test::aColourChaseRunsAtItsSpeedOverPixelBoxes()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 fx = effect(QStringLiteral("color-chase.js"), QStringLiteral("Chase"));
    const quint32 cue = oneCue();
    const quint32 layer = st->addLayer(cue, groupTarget(group("TrussW-RGB")));
    QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(fx)));
    QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(palette("Full"))));
    go();
    const QStringList boxes = { "SR Truss Wash 1", "SR Truss Wash 2", "SL Truss Wash 1", "SL Truss Wash 2" };
    QSet<QString> lead;
    for (int i = 0; i < kSamples && lead.size() < 3; ++i)
    {
        run(100);
        QString best;
        int bestSum = 0;
        for (const QString &b : boxes)
        {
            int sum = 0;
            for (char c : sends(b, 0, 135))
                sum += uchar(c);
            if (sum > bestSum)
            {
                bestSum = sum;
                best = b;
            }
        }
        if (!best.isEmpty())
            lead.insert(best);
    }
    QVERIFY2(lead.size() >= 3, qPrintable(QStringList(lead.values()).join(", ")));
}

/****************************************************************************
 * #20: the upstage beams on a line
 ****************************************************************************/

/* Aimed at the Riser Line (on the risers, lower than the beams' heads)
   the eight two-arm beams standing on the deck tilt up to the people on
   it: neither arm is left at tilt 0, level, out over the house -- and they
   fan, each to its own point. */
void PixelBoxes_Test::beamsAimedAtALineTiltUp()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 cue = oneCue();
    const quint32 layer = st->addLayer(cue, groupTarget(group("USTs")));
    QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(palette("Aim Riser Line"))));
    QVERIFY(st->setLayerLook(cue, layer, st->lookForPalette(palette("Full"))));
    go();
    run(200);
    QSet<int> pans;
    for (int i = 1; i <= 8; ++i)
    {
        const QByteArray v = sends(QStringLiteral("UST %1").arg(i), 0, 5);
        QVERIFY2(uchar(v.at(2)) != 0 && uchar(v.at(3)) != 0,
                 qPrintable(QString("UST %1: %2").arg(i).arg(QString::fromLatin1(v.toHex(' ')))));
        pans.insert(uchar(v.at(0)));
    }
    QVERIFY2(pans.size() >= 4, qPrintable(QString::number(pans.size())));
}

QTEST_MAIN(PixelBoxes_Test)

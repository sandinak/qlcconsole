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
#include "effectruns.h"
#include "effectscriptrunner.h"
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

/****************************************************************************
 * A Breathe on the step tops (build 2593): White, 20%, Breathe, Once
 ****************************************************************************/

/* The step tops' white pixels (channels 48-63) and the 20% they are at:
   the breathe's floor is its minDim of that, 5% of 51. */
static const int kTopWhite = 48;
static const int kTwentyPercent = 51;

bool PixelBoxes_Test::breatheOnTheStepTops(double rate, int runs)
{
    ShowTreeController *st = m_doc->showTree();
    QLCPalette *twenty = new QLCPalette(QLCPalette::Dimmer, m_doc);
    twenty->setName(QStringLiteral("20%"));
    twenty->setValue(kTwentyPercent);
    m_doc->addPalette(twenty);
    const quint32 fx = effect(QStringLiteral("dimmer-phaser.js"), QStringLiteral("Breathe"));
    QLCPalette *b = m_doc->palette(fx);
    // resources/effectpresets/Breathe.json
    b->setEffectParamValue(QStringLiteral("waveform"), 0);
    b->setEffectParamValue(QStringLiteral("rate"), rate);
    b->setEffectParamValue(QStringLiteral("minDim"), 0.05);
    b->setEffectParamValue(QStringLiteral("maxDim"), 1.0);
    b->setEffectParamValue(QStringLiteral("spread"), 0);
    if (runs > 0)
        b->setEffectParamValue(EffectRuns::runsKey(), runs);
    const quint32 cue = oneCue();
    const quint32 layer = st->addLayer(cue, groupTarget(group("StepTops")));
    return st->setLayerLook(cue, layer, st->lookForPalette(palette("White")))
        && st->setLayerLook(cue, layer, st->lookForPalette(twenty->id()))
        && st->setLayerLook(cue, layer, st->lookForPalette(fx));
}

/* From the first frame it lights, the breathe is at its floor and rises:
   the look's 20% never shows on its own first (it flashed bright, then
   started the cycle). Sampled as fast as the event loop turns. */
void PixelBoxes_Test::aBreatheStartsAtItsFloor()
{
    QVERIFY(breatheOnTheStepTops(0.2, 0));
    go();
    QElapsedTimer t;
    t.start();
    QList<int> seen;
    // A breath of 5 s: in its first 400 ms it is under 10% of its swing.
    while (t.elapsed() < 400)
    {
        QTest::qWait(2);
        // its white pixel and its first RGB pixel's red: both breathe
        seen << int(uchar(sends("US Step 1 Top SR", kTopWhite, 1).at(0)))
             << int(uchar(sends("US Step 1 Top SR", 0, 1).at(0)));
    }
    int most = 0;
    for (int v : seen)
        most = qMax(most, v);
    QString all;
    for (int v : seen)
        all += QString::number(v) + QLatin1Char(' ');
    QVERIFY2(most <= kTwentyPercent / 4, qPrintable(all));
    // Lit, white and red both: its floor is 5% of the 20%, not black
    QVERIFY2(seen.at(seen.size() - 2) > 0 && seen.last() > 0, qPrintable(all));
}

void PixelBoxes_Test::aBreatheOnceEndsOnItsFloor_data()
{
    QTest::addColumn<double>("rate");
    QTest::addColumn<int>("runs");
    // 1.25 s a breath: the old run (the effect's own 2 s) ended mid-breath
    QTest::newRow("Once, 1.25 s") << 0.8 << 1;
    QTest::newRow("3 times, 0.6 s") << 1.0 / 0.6 << 3;
}

/* Once (or N times) is whole breaths: it ends where a breath ends, on
   its floor, and holds there -- not wherever a set time cut it off. */
void PixelBoxes_Test::aBreatheOnceEndsOnItsFloor()
{
    QFETCH(double, rate);
    QFETCH(int, runs);
    QVERIFY(breatheOnTheStepTops(rate, runs));
    go();
    const int ms = int(1000.0 * runs / rate);
    QElapsedTimer t;
    t.start();
    int top = 0;
    while (t.elapsed() < ms - 100)
    {
        QTest::qWait(10);
        top = qMax(top, int(uchar(sends("US Step 1 Top SR", kTopWhite, 1).at(0))));
    }
    // It breathed: up to the look's 20%
    QVERIFY2(top >= kTwentyPercent - 3, qPrintable(QString::number(top)));
    run(ms / 2 + 300);
    const int floor = qRound(kTwentyPercent * 0.05);
    const int end = int(uchar(sends("US Step 1 Top SR", kTopWhite, 1).at(0)));
    QVERIFY2(qAbs(end - floor) <= 1, qPrintable(QString("ended on %1, floor %2").arg(end).arg(floor)));
    // ...and holds there
    run(300);
    QCOMPARE(int(uchar(sends("US Step 1 Top SR", kTopWhite, 1).at(0))), end);
}

/* Looping, then set to Once while it plays (the Look Editor's Runs): the
   run starts again, one breath from its floor, and holds there. */
void PixelBoxes_Test::aBreatheSetToOncePlaysItAgain()
{
    QVERIFY(breatheOnTheStepTops(1.0, 0));
    go();
    run(1700);                                      // well into its second breath
    QLCPalette *b = nullptr;
    for (QLCPalette *p : m_doc->palettes())
        if (p->name() == QLatin1String("Breathe"))
            b = p;
    QVERIFY(b != nullptr);
    b->setEffectParamValue(EffectRuns::runsKey(), 1);
    m_doc->effectScriptRunner()->updateEffectParams(b->id());
    run(450);                                       // not cut off: mid-breath
    const int mid = int(uchar(sends("US Step 1 Top SR", kTopWhite, 1).at(0)));
    QVERIFY2(mid > kTwentyPercent / 2, qPrintable(QString::number(mid)));
    run(1000);
    const int end = int(uchar(sends("US Step 1 Top SR", kTopWhite, 1).at(0)));
    QVERIFY2(qAbs(end - qRound(kTwentyPercent * 0.05)) <= 1, qPrintable(QString::number(end)));
}

QTEST_MAIN(PixelBoxes_Test)

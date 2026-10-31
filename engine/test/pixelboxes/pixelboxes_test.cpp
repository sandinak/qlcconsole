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
#include "collection.h"
#include "scene.h"
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
    return m_cue = st->addCue(list, -1, QStringLiteral("One"));
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
 * Scoped hits (sch, Q154): a one-shot in a hit under a cue
 ****************************************************************************/

/* Branson's use: a Breathe, Runs: Once, in a hit that lives under a cue,
   fired from a VC button while that cue is live. Each fire breathes from
   its floor afresh -- Toggle on again after it finished, and after the
   cue was left (the hit let go as its place ended) and come back to. The
   button starts and stops the hit's Collection; that is done here. */
void PixelBoxes_Test::aOnceBreatheInACuesHitBreathesEachFire()
{
    const int topRed = 0;               // the step tops' first pixel (White lights its RGB)
    const int twenty = 51;              // 20%: the breathe's floor is 5% of it
    ShowTreeController *st = m_doc->showTree();
    QLCPalette *p20 = new QLCPalette(QLCPalette::Dimmer, m_doc);
    p20->setName(QStringLiteral("Sch 20%"));
    p20->setValue(twenty);
    m_doc->addPalette(p20);
    const quint32 fx = effect(QStringLiteral("dimmer-phaser.js"), QStringLiteral("Sch Breathe"));
    QLCPalette *b = m_doc->palette(fx);
    b->setEffectParamValue(QStringLiteral("waveform"), 0);
    b->setEffectParamValue(QStringLiteral("rate"), 1.0);            // one 1 s breath
    b->setEffectParamValue(QStringLiteral("minDim"), 0.05);
    b->setEffectParamValue(QStringLiteral("maxDim"), 1.0);
    b->setEffectParamValue(QStringLiteral("spread"), 0);
    b->setEffectParamValue(QStringLiteral("__runs"), 1);            // Runs: Once
    const quint32 cue = oneCue();
    const quint32 list = st->listOfCue(cue)->id;
    st->addCue(list, 0, QStringLiteral("Two"));
    const quint32 hit = st->addHit(HitScope::Cue, cue, QStringLiteral("Breathe hit"));
    QVERIFY(hit != invalidId());
    const quint32 layer = st->addLayer(hit, groupTarget(group("StepTops")));
    QVERIFY(st->setLayerLook(hit, layer, st->lookForPalette(palette("White"))));
    QVERIFY(st->setLayerLook(hit, layer, st->lookForPalette(p20->id())));
    QVERIFY(st->setLayerLook(hit, layer, st->lookForPalette(fx)));
    Function *coll = m_doc->function(st->lookFunction(hit));
    QVERIFY(coll != nullptr);
    auto top = [&]() { return int(uchar(sends("US Step 1 Top SR", topRed, 1).at(0))); };
    // One fire: where it is early, mid-breath and after its breath.
    auto fire = [&]() {
        QList<int> v;
        coll->start(m_doc->masterTimer(), FunctionParent::master());
        run(100);
        v << top();
        run(400);
        v << top();
        run(900);
        v << top();
        coll->stop(FunctionParent::master());
        run(150);
        return v;
    };
    // Each fire breathes as the first did: none starts "done". Mid-breath
    // and after it, give or take the timer's jitter under load.
    auto same = [twenty](const QList<int> &a, const QList<int> &b) {
        return b.at(1) > twenty / 2 && qAbs(a.at(1) - b.at(1)) <= 16 && qAbs(a.at(2) - b.at(2)) <= 16;
    };
    go();                                           // cue One live: armed
    run(200);
    QVERIFY(st->isHitArmed(hit));
    const QList<int> first = fire();
    auto text = [](const QList<int> &v) {
        QStringList t;
        for (int x : v)
            t << QString::number(x);
        return t.join(QLatin1Char(' '));
    };
    // It breathes: up mid-breath from where it started.
    QVERIFY2(first.at(1) > twenty / 2 && first.at(1) > first.at(0), qPrintable(text(first)));
    const QList<int> again = fire();
    QVERIFY2(same(first, again), qPrintable(text(first) + " / again " + text(again)));
    // The cue left (disarmed: the button lets it go) and come back to.
    go();
    run(200);
    QVERIFY(!st->isHitArmed(hit));
    QVERIFY(m_doc->showControl()->execute({ ShowControl::Back, QString(), QString() }));
    run(200);
    QVERIFY(st->isHitArmed(hit));
    const QList<int> rearmed = fire();
    QVERIFY2(same(first, rearmed), qPrintable(text(first) + " / re-armed " + text(rearmed)));
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

/* The same Once breathe in three cues in a row, the cue copied in place
   (one effect palette, one engine): each GO breathes again -- not only the
   first, the others left on the first's held floor. */
void PixelBoxes_Test::aOnceBreatheInEachOfThreeCopiedCues()
{
    QVERIFY(breatheOnTheStepTops(1.0, 1));          // one 1 s breath
    ShowTreeController *st = m_doc->showTree();
    const quint32 first = m_cue;
    QVERIFY(st->duplicateCue(first) != 0);
    QVERIFY(st->duplicateCue(first) != 0);
    for (int cue = 1; cue <= 3; ++cue)
    {
        go();
        run(500);                                   // mid-breath: near its top
        const int mid = int(uchar(sends("US Step 1 Top SR", kTopWhite, 1).at(0)));
        QVERIFY2(mid > kTwentyPercent / 2, qPrintable(QString("cue %1: %2").arg(cue).arg(mid)));
        run(900);                                   // done: on its floor
        const int end = int(uchar(sends("US Step 1 Top SR", kTopWhite, 1).at(0)));
        QVERIFY2(qAbs(end - qRound(kTwentyPercent * 0.05)) <= 1,
                 qPrintable(QString("cue %1 ended on %2").arg(cue).arg(end)));
    }
}

/* A hit (a collection fired from a button) carrying the same Once breathe
   as the running cue: started, stopped and started again, it breathes
   each time -- though the cue's scene keeps the effect's engine going. */
void PixelBoxes_Test::aOnceBreatheInAHitFiredAgain()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(breatheOnTheStepTops(1.0, 1));
    quint32 breathe = palette(QStringLiteral("Breathe"));
    // The cue's layer: the truss boxes, with the breathe
    const quint32 layer = st->addLayer(m_cue, groupTarget(group("TrussW-RGB")));
    QVERIFY(st->setLayerLook(m_cue, layer, st->lookForPalette(palette("White"))));
    QVERIFY(st->setLayerLook(m_cue, layer, st->lookForPalette(breathe)));
    // The hit: the step tops, White, 20%, the same breathe
    Scene *hitScene = new Scene(m_doc);
    hitScene->setName(QStringLiteral("Hit"));
    QVERIFY(m_doc->addFunction(hitScene));
    hitScene->addFixtureGroup(group("StepTops"));
    hitScene->addPalette(palette("White"));
    hitScene->addPalette(palette("20%"));
    hitScene->addPalette(breathe);
    Collection *hit = new Collection(m_doc);
    QVERIFY(m_doc->addFunction(hit));
    QVERIFY(hit->addFunction(hitScene->id()));

    go();
    run(1400);                                      // the cue's breath is done
    for (int fire = 1; fire <= 2; ++fire)
    {
        hit->start(m_doc->masterTimer(), FunctionParent::master());
        run(500);
        const int mid = int(uchar(sends("US Step 1 Top SR", kTopWhite, 1).at(0)));
        QVERIFY2(mid > kTwentyPercent / 2, qPrintable(QString("fire %1: %2").arg(fire).arg(mid)));
        run(900);
        hit->stop(FunctionParent::master());
        run(100);
    }
}

/* decided: the next cue that only tracks the layer through (sets nothing
   of its own on it) leaves a done Once done: it doesn't breathe again. */
void PixelBoxes_Test::aOnceBreatheTrackedThroughStaysDone()
{
    QVERIFY(breatheOnTheStepTops(1.0, 1));
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->addCue(st->listOfCue(m_cue)->id, 0, QStringLiteral("Tracked")) != 0);
    go();
    run(1400);                                      // breathed, on its floor
    const int floor = qRound(kTwentyPercent * 0.05);
    go();                                           // the tracking cue
    for (int i = 0; i < 6; ++i)
    {
        run(100);
        const int v = int(uchar(sends("US Step 1 Top SR", kTopWhite, 1).at(0)));
        QVERIFY2(qAbs(v - floor) <= 1, qPrintable(QString::number(v)));
    }
}

QTEST_MAIN(PixelBoxes_Test)

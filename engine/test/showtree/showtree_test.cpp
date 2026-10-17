/*
  Q Light Controller Plus
  showtree_test.cpp

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
#include <QBuffer>
#include <QtTest>
#include <atomic>
#include <thread>

#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private
#include "showtreecontroller.h"
#include "showtreecommand.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "showtreemodel.h"
#include "qlcpalette.h"
#include "scenevalue.h"
#include "collection.h"
#include "chaserstep.h"
#include "resource_paths.h"
#include "qlcfile.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "showtree_test.h"

using namespace ShowTree;

/* The Show Tree (SHOW_TREE_DESIGN.md): the cascade from the §3 worked
   examples as tables, baking into Chaser / Collections / shared Scenes, undo,
   load/save, and importing a chaser so it plays as it did. */

/****************************************************************************
 * Helpers
 ****************************************************************************/

namespace
{
// Palettes in the model-only tests are just ids.
enum : quint32
{
    Blue = 101, Full = 102, Red = 103, D45 = 104, UST0 = 105, White = 106,
    Announcer = 107, BaseDim = 108, Spots0 = 109, Green = 110,
    Confetti = 201, Sinelon = 202, FollowSpot = 203
};
// Groups
enum : quint32
{
    TrussC = 1, Towers = 2, USTs = 3, ATSpots = 4, BackStage = 5, StepTops = 6, ATWAll = 7
};

Look look(const QString &key, QList<quint32> pals, QList<quint32> own = {})
{
    Look l;
    l.key = key;
    l.palettes = pals;
    l.own = own;
    return l;
}

Look noneLook(const QString &key)
{
    Look l;
    l.key = key;
    l.none = true;
    return l;
}

Layer layer(quint32 id, quint32 group, QList<Look> looks, bool release = false)
{
    Layer l;
    l.id = id;
    l.target.groups << group;
    l.looks = looks;
    l.release = release;
    return l;
}

Cue cue(quint32 id, const QString &number, CueMode mode, QList<Layer> layers,
        QList<Look> cueLooks = {}, QList<quint32> includes = {})
{
    Cue c;
    c.id = id;
    c.number = number;
    c.mode = mode;
    c.layers = layers;
    c.looks = cueLooks;
    c.includes = includes;
    return c;
}

Target groupTarget(quint32 g)
{
    Target t;
    t.groups << g;
    return t;
}

const ResolvedValue *value(const ResolvedCue &rc, quint32 group, const QString &key)
{
    const ResolvedLayer *l = rc.layer(groupTarget(group));
    if (l == nullptr || !l->values.contains(key))
        return nullptr;
    return &l->values.find(key).value();
}

// "palette, origin, from cue" in one compare
#define CHECK_VALUE(rc, group, key, pal, org, from)                              \
    do {                                                                          \
        const ResolvedValue *v_ = value(rc, group, key);                          \
        QVERIFY2(v_ != nullptr, qPrintable(QString("no %1 on group %2").arg(key).arg(group))); \
        QCOMPARE(v_->look.palettes.value(0), quint32(pal));                       \
        QCOMPARE(int(v_->origin), int(org));                                      \
        QCOMPARE(v_->fromCue, quint32(from));                                     \
    } while (0)
}

void ShowTree_Test::init()
{
    m_doc = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir));

    m_fixtures.clear();
    for (int i = 0; i < 8; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dim %1").arg(i + 1));
        fxi->setAddress(quint32(i));
        fxi->setUniverse(0);
        fxi->setChannels(1);
        m_doc->addFixture(fxi);
        m_fixtures << fxi->id();
    }
}

void ShowTree_Test::cleanup()
{
    stopAll();
    delete m_doc;
    m_doc = nullptr;
}

void ShowTree_Test::stopAll()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(5);
}

quint32 ShowTree_Test::dimmer(int level, const QString &name)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
    p->setName(name);
    p->setValue(level);
    m_doc->addPalette(p);
    return p->id();
}

quint32 ShowTree_Test::colour(const QString &name)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Color, m_doc);
    p->setName(name);
    p->setValue(QLCPalette::colorToString(QColor(Qt::blue), QColor(Qt::black)));
    m_doc->addPalette(p);
    return p->id();
}

FixtureGroup *ShowTree_Test::group(const QString &name, const QList<quint32> &fixtures)
{
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName(name);
    m_doc->addFixtureGroup(g);
    for (quint32 f : fixtures)
        g->assignFixture(f);
    return g;
}

void ShowTree_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
}

QHash<quint32, uchar> ShowTree_Test::settle(int ticks)
{
    tick(ticks);
    QHash<quint32, uchar> out;
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    for (int i = 0; i < m_fixtures.count(); ++i)
        out.insert(quint32(i), uchar(ua.first()->preGMValues().at(i)));
    m_doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

void ShowTree_Test::go(Chaser *c)
{
    ChaserAction action;
    action.m_action = ChaserNextStep;
    action.m_stepIndex = -1;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    c->setAction(action);
    if (!c->isRunning())
        c->start(m_doc->masterTimer(), FunctionParent::master());
}

/****************************************************************************
 * The model and the cascade
 ****************************************************************************/

/* §3.1: cue looks Blue + Full; Towers runs Confetti in its own Red; USTs set
   45% and UST-0 and run Sinelon in White. */
void ShowTree_Test::globalLayerEffect_3_1()
{
    CueList list;
    list.mode = ListMode::NonTracking;
    list.cues << cue(10, "1.0.0", CueMode::ListDefault,
                     { layer(1, TrussC, {}),
                       layer(2, Towers, { look(effectKey("Color"), { Confetti }, { Red }) }),
                       layer(3, USTs, { look(KeyDimmer, { D45 }), look(KeyPosition, { UST0 }),
                                        look(effectKey("Color"), { Sinelon }, { White }) }) },
                     { look(KeyColour, { Blue }), look(KeyDimmer, { Full }) });

    const QList<ResolvedCue> r = resolve(list);
    QCOMPARE(r.count(), 1);
    const ResolvedCue &c = r.first();
    QCOMPARE(c.layers.count(), 3);

    // | TrussC-RGB | Blue (cue) | Full (cue) | — | — |
    CHECK_VALUE(c, TrussC, KeyColour, Blue, Origin::CueLook, 10);
    CHECK_VALUE(c, TrussC, KeyDimmer, Full, Origin::CueLook, 10);
    QVERIFY(value(c, TrussC, KeyPosition) == nullptr);
    // | Towers | Blue (cue) | Full (cue) | — | Confetti in Red (its own) |
    CHECK_VALUE(c, Towers, KeyColour, Blue, Origin::CueLook, 10);
    CHECK_VALUE(c, Towers, KeyDimmer, Full, Origin::CueLook, 10);
    CHECK_VALUE(c, Towers, effectKey("Color"), Confetti, Origin::Own, 10);
    QCOMPARE(value(c, Towers, effectKey("Color"))->look.own, QList<quint32>({ Red }));
    // | USTs | Blue (cue) | 45% (layer) | UST-0 | Sinelon in White, over Blue |
    CHECK_VALUE(c, USTs, KeyColour, Blue, Origin::CueLook, 10);
    CHECK_VALUE(c, USTs, KeyDimmer, D45, Origin::Own, 10);
    CHECK_VALUE(c, USTs, KeyPosition, UST0, Origin::Own, 10);

    // The generated scene's stack: base looks, then the effect with its own.
    QCOMPARE(ShowTreeController::scenePalettes(*c.layer(groupTarget(USTs))),
             QList<quint32>({ Blue, D45, UST0, Sinelon, White }));
    QCOMPARE(ShowTreeController::scenePalettes(*c.layer(groupTarget(Towers))),
             QList<quint32>({ Blue, Full, Confetti, Red }));
}

/* §3.2 in a Tracking list with a base cue: AT-Spots released in 0.0.3 goes
   back to the base; BackStage tracks; StepTops changes only its dimmer. */
void ShowTree_Test::trackingAcrossThreeCues_3_2()
{
    CueList list;
    list.mode = ListMode::Tracking;
    list.baseCue = 1;
    list.cues << cue(1, "0.0.0", CueMode::Block,
                     { layer(11, ATSpots, { look(KeyDimmer, { BaseDim }), look(KeyColour, { White }),
                                            look(KeyPosition, { Spots0 }) }) });
    list.cues << cue(3, "0.0.3", CueMode::ListDefault,
                     { layer(31, ATSpots, { look(KeyDimmer, { Full }), look(KeyColour, { White }),
                                            look(KeyPosition, { Announcer }),
                                            look(effectKey("Position"), { FollowSpot }) }, true),
                       layer(32, BackStage, { look(KeyColour, { Blue }), look(KeyDimmer, { D45 }) }) });
    list.cues << cue(4, "0.0.4", CueMode::ListDefault,
                     { layer(41, StepTops, { look(KeyColour, { White }), look(KeyDimmer, { BaseDim }) }) });
    list.cues << cue(5, "0.0.5", CueMode::ListDefault,
                     { layer(51, StepTops, { look(KeyDimmer, { D45 }) }) });

    const QList<ResolvedCue> r = resolve(list);
    QCOMPARE(r.count(), 4);

    // 0.0.3: AT-Spots and BackStage its own
    CHECK_VALUE(r[1], ATSpots, KeyDimmer, Full, Origin::Own, 3);
    CHECK_VALUE(r[1], ATSpots, KeyPosition, Announcer, Origin::Own, 3);
    CHECK_VALUE(r[1], BackStage, KeyColour, Blue, Origin::Own, 3);

    // 0.0.4: AT-Spots released -> base; BackStage tracked from 0.0.3
    CHECK_VALUE(r[2], ATSpots, KeyDimmer, BaseDim, Origin::Base, 1);
    CHECK_VALUE(r[2], ATSpots, KeyPosition, Spots0, Origin::Base, 1);
    QVERIFY(value(r[2], ATSpots, effectKey("Position")) == nullptr);   // the follow spot is gone
    CHECK_VALUE(r[2], BackStage, KeyColour, Blue, Origin::Tracked, 3);
    CHECK_VALUE(r[2], BackStage, KeyDimmer, D45, Origin::Tracked, 3);
    CHECK_VALUE(r[2], StepTops, KeyColour, White, Origin::Own, 4);

    // 0.0.5: one value of its own; the rest tracked
    CHECK_VALUE(r[3], ATSpots, KeyDimmer, BaseDim, Origin::Tracked, 1);
    CHECK_VALUE(r[3], BackStage, KeyColour, Blue, Origin::Tracked, 3);
    CHECK_VALUE(r[3], StepTops, KeyColour, White, Origin::Tracked, 4);
    CHECK_VALUE(r[3], StepTops, KeyDimmer, D45, Origin::Own, 5);
    QCOMPARE(r[3].layers.count(), 3);

    // Going darker works as well as brighter: 0.0.5's StepTops plays 45%
    // alone, not next to 0.0.4's level.
    QCOMPARE(ShowTreeController::scenePalettes(*r[3].layer(groupTarget(StepTops))),
             QList<quint32>({ White, D45 }));
}

/* §3.3: 0.0.4 Block starts from nothing; 0.0.5 tracks from 0.0.4. */
void ShowTree_Test::aBlockCue_3_3()
{
    CueList list;
    list.mode = ListMode::Tracking;
    list.cues << cue(3, "0.0.3", CueMode::ListDefault,
                     { layer(32, BackStage, { look(KeyColour, { Blue }), look(KeyDimmer, { D45 }) }) });
    list.cues << cue(4, "0.0.4", CueMode::Block,
                     { layer(41, StepTops, { look(KeyColour, { White }), look(KeyDimmer, { BaseDim }) }) });
    list.cues << cue(5, "0.0.5", CueMode::ListDefault,
                     { layer(51, StepTops, { look(KeyDimmer, { D45 }) }) });
    const QList<ResolvedCue> r = resolve(list);

    QVERIFY(r[1].layer(groupTarget(BackStage)) == nullptr);
    QCOMPARE(r[1].layers.count(), 1);
    QVERIFY(r[2].layer(groupTarget(BackStage)) == nullptr);
    CHECK_VALUE(r[2], StepTops, KeyColour, White, Origin::Tracked, 4);
    CHECK_VALUE(r[2], StepTops, KeyDimmer, D45, Origin::Own, 5);
}

/* §3.4: a Cue-only flash, then the washes are back; a Block cue including
   0.0.3 with one change of its own. */
void ShowTree_Test::cueOnlyAndAnInclude_3_4()
{
    CueList list;
    list.mode = ListMode::Tracking;
    list.cues << cue(3, "0.0.3", CueMode::Block,
                     { layer(31, ATSpots, { look(KeyDimmer, { Full }), look(KeyColour, { White }) }),
                       layer(32, BackStage, { look(KeyColour, { Blue }), look(KeyDimmer, { D45 }) }) });
    list.cues << cue(5, "0.0.5", CueMode::ListDefault,
                     { layer(51, ATWAll, { look(KeyColour, { Blue }), look(KeyDimmer, { D45 }) }) });
    list.cues << cue(6, "0.0.6", CueMode::CueOnly,
                     { layer(61, ATWAll, { look(KeyColour, { White }), look(KeyDimmer, { Full }) }) });
    list.cues << cue(7, "0.0.7", CueMode::ListDefault, {});
    list.cues << cue(9, "0.0.9", CueMode::Block,
                     { layer(91, ATSpots, { look(KeyDimmer, { D45 }) }) }, {}, { 3 });
    const QList<ResolvedCue> r = resolve(list);

    // The flash plays in 0.0.6...
    CHECK_VALUE(r[2], ATWAll, KeyColour, White, Origin::Own, 6);
    CHECK_VALUE(r[2], ATWAll, KeyDimmer, Full, Origin::Own, 6);
    // ...and 0.0.7 starts from 0.0.5's output: the washes are back.
    CHECK_VALUE(r[3], ATWAll, KeyColour, Blue, Origin::Tracked, 5);
    CHECK_VALUE(r[3], ATWAll, KeyDimmer, D45, Origin::Tracked, 5);

    // 0.0.9 Block includes 0.0.3: its whole look, with AT-Spots at 45%.
    CHECK_VALUE(r[4], ATSpots, KeyColour, White, Origin::Included, 3);
    CHECK_VALUE(r[4], ATSpots, KeyDimmer, D45, Origin::Own, 9);
    CHECK_VALUE(r[4], BackStage, KeyColour, Blue, Origin::Included, 3);
    QVERIFY(r[4].layer(groupTarget(ATWAll)) == nullptr);     // Block: nothing tracked
}

/* D6: a cue look reaches the cue's own layers, not one only tracked in. */
void ShowTree_Test::cueLooksReachOnlyOwnLayers()
{
    CueList list;
    list.mode = ListMode::Tracking;
    list.cues << cue(1, "1", CueMode::ListDefault,
                     { layer(11, Towers, { look(KeyColour, { Red }) }) });
    list.cues << cue(2, "2", CueMode::ListDefault,
                     { layer(21, USTs, { look(KeyDimmer, { Full }) }) },
                     { look(KeyColour, { Blue }) });
    const QList<ResolvedCue> r = resolve(list);
    CHECK_VALUE(r[1], USTs, KeyColour, Blue, Origin::CueLook, 2);
    CHECK_VALUE(r[1], Towers, KeyColour, Red, Origin::Tracked, 1);
}

void ShowTree_Test::noneStopsAnInheritedValue()
{
    CueList list;
    list.mode = ListMode::Tracking;
    list.cues << cue(1, "1", CueMode::ListDefault,
                     { layer(11, Towers, { look(KeyColour, { Red }),
                                           look(effectKey("Color"), { Confetti }) }) });
    list.cues << cue(2, "2", CueMode::ListDefault,
                     { layer(21, Towers, { noneLook(effectKey("Color")) }) });
    const QList<ResolvedCue> r = resolve(list);
    QCOMPARE(ShowTreeController::scenePalettes(*r[0].layer(groupTarget(Towers))),
             QList<quint32>({ Red, Confetti }));
    QCOMPARE(ShowTreeController::scenePalettes(*r[1].layer(groupTarget(Towers))),
             QList<quint32>({ Red }));
    QVERIFY(value(r[1], Towers, effectKey("Color"))->look.none);

    // Only None: a dark layer, which bakes to no scene.
    list.cues << cue(3, "3", CueMode::ListDefault, { layer(31, Towers, { noneLook(KeyColour) }) });
    const QList<ResolvedCue> r2 = resolve(list);
    QVERIFY(r2[2].layer(groupTarget(Towers))->isDark());
}

void ShowTree_Test::aNonTrackingListPlaysEachCueAlone()
{
    CueList list;
    list.mode = ListMode::NonTracking;
    list.cues << cue(1, "1", CueMode::ListDefault, { layer(11, Towers, { look(KeyColour, { Red }) }) });
    list.cues << cue(2, "2", CueMode::ListDefault, { layer(21, USTs, { look(KeyColour, { Blue }) }) });
    const QList<ResolvedCue> r = resolve(list);
    QCOMPARE(r[1].layers.count(), 1);
    QVERIFY(r[1].layer(groupTarget(Towers)) == nullptr);
    QCOMPARE(effectiveMode(list, list.cues[1]), CueMode::Block);
    list.mode = ListMode::Tracking;
    QCOMPARE(effectiveMode(list, list.cues[1]), CueMode::Track);
}

void ShowTree_Test::anIncludeThatWouldLoopIsLeftOut()
{
    CueList list;
    list.mode = ListMode::NonTracking;
    list.cues << cue(1, "1", CueMode::ListDefault, { layer(11, Towers, { look(KeyColour, { Red }) }) }, {}, { 2 });
    list.cues << cue(2, "2", CueMode::ListDefault, { layer(21, USTs, { look(KeyColour, { Blue }) }) }, {}, { 1 });
    const QList<ResolvedCue> r = resolve(list);
    // A loop is left out on both sides, whichever cue is read first: each
    // plays only its own (the editor never makes one; a file might hold it).
    QVERIFY(value(r[0], USTs, KeyColour) == nullptr);
    CHECK_VALUE(r[0], Towers, KeyColour, Red, Origin::Own, 1);
    CHECK_VALUE(r[1], USTs, KeyColour, Blue, Origin::Own, 2);
    QCOMPARE(r[0].skippedIncludes, QList<quint32>({ 2 }));
    QCOMPARE(r[1].skippedIncludes, QList<quint32>({ 1 }));

    // A loop through tracking: cue 1 includes 2, and 2 tracks from 1. Cue 2
    // still tracks Towers from cue 1 (not an empty start read half done).
    list.mode = ListMode::Tracking;
    list.cues[1].includes.clear();
    const QList<ResolvedCue> t = resolve(list);
    QCOMPARE(t[0].skippedIncludes, QList<quint32>({ 2 }));
    CHECK_VALUE(t[1], Towers, KeyColour, Red, Origin::Tracked, 1);
    CHECK_VALUE(t[1], USTs, KeyColour, Blue, Origin::Own, 2);
    QVERIFY(dependsOn(list, 2, 1));
    QVERIFY(!dependsOn(list, 1, 2) || list.cues[0].includes.contains(2));
    QString why;
    QVERIFY(!canInclude(list, 1, 1, &why));
    list.cues[0].includes.clear();
    QVERIFY(!dependsOn(list, 1, 2));
    QVERIFY(!canInclude(list, 1, 2, &why));     // 2 tracks from 1: a loop
    QVERIFY(why.contains("loop"));
    QVERIFY(canInclude(list, 2, 1, &why));
}

/* D23: next whole number in the last place, else a point cue; never renumber. */
void ShowTree_Test::numbering()
{
    CueList list;
    list.name = "01-Everybody Scream";
    QCOMPARE(numberForInsert(list, -1), QString("1.0.0"));
    list.name = "Full Show";
    QCOMPARE(numberForInsert(list, -1), QString("1"));

    list.cues << cue(1, "0.0.4", CueMode::ListDefault, {});
    list.cues << cue(2, "0.0.5", CueMode::ListDefault, {});
    QCOMPARE(numberForInsert(list, 1), QString("0.0.6"));
    QCOMPARE(numberForInsert(list, 0), QString("0.0.4.5"));
    list.cues.insert(1, cue(3, "0.0.4.5", CueMode::ListDefault, {}));
    QCOMPARE(numberForInsert(list, 0), QString("0.0.4.1"));    // below 0.0.4.5
    QCOMPARE(numberForInsert(list, 1), QString("0.0.4.6"));

    // Unnumbered cues answer to their position.
    CueList plain;
    plain.cues << cue(1, "", CueMode::ListDefault, {}) << cue(2, "", CueMode::ListDefault, {});
    QCOMPARE(numberForInsert(plain, 1), QString("3"));
    QCOMPARE(numberForInsert(plain, 0), QString("1.5"));
}

void ShowTree_Test::numberedNames()
{
    QString n, name;
    QVERIFY(splitNumberedName("0.0.4-SHOUT", &n, &name));
    QCOMPARE(n, QString("0.0.4"));
    QCOMPARE(name, QString("SHOUT"));
    QVERIFY(splitNumberedName("1-Everbody Scream", &n, &name));
    QCOMPARE(n, QString("1"));
    QCOMPARE(name, QString("Everbody Scream"));
    QVERIFY(splitNumberedName("12.5 Finale", &n, &name));
    QCOMPARE(n, QString("12.5"));
    QCOMPARE(name, QString("Finale"));
    QVERIFY(!splitNumberedName("Preshow", &n, &name));
    QVERIFY(!splitNumberedName("Base 2", &n, &name));
}

static QByteArray productionXml(const Production &p)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.setAutoFormatting(true);
    ShowTree::saveXML(&w, p);
    return xml;
}

void ShowTree_Test::xmlRoundTrip()
{
    Production p;
    p.id = 1;
    p.name = "DanceHysteria";
    CueList list;
    list.id = 2;
    list.name = "Full Show";
    list.mode = ListMode::Tracking;
    list.baseCue = 3;
    list.chaser = 77;
    Cue c = cue(3, "0.0.3", CueMode::CueOnly,
                { layer(4, ATSpots, { look(KeyDimmer, { Full }), look(effectKey("Color"), { Confetti }, { Red, White }) }, true),
                  layer(5, BackStage, { noneLook(KeyColour) }) },
                { look(KeyColour, { Blue, Green }) }, { 9 });
    c.name = "Announcer \"quoted\" & <odd>";
    c.note = "Listen for the first";
    c.fadeIn = 1260;
    c.fadeOut = 0;
    c.hold = Function::infiniteSpeed();
    c.collection = 88;
    c.layers[1].target.fixtures << 12;
    c.layers[1].continues.groups << BackStage;
    Layer opaque;
    opaque.id = 6;
    opaque.target.function = 42;
    c.layers << opaque;
    list.cues << c;
    p.lists << list;

    const QByteArray xml = productionXml(p);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    Production back;
    QVERIFY(ShowTree::loadXML(r, back));
    QCOMPARE(productionXml(back), xml);

    const Cue &bc = back.lists.first().cues.first();
    QCOMPARE(bc.name, c.name);
    QCOMPARE(bc.mode, CueMode::CueOnly);
    QCOMPARE(bc.layers.count(), 3);
    QVERIFY(bc.layers.at(0).release);
    QVERIFY(bc.layers.at(1).looks.first().none);
    QCOMPARE(bc.layers.at(1).continues.groups, QList<quint32>({ BackStage }));
    QCOMPARE(bc.layers.at(1).target.fixtures, QList<quint32>({ 12 }));
    QVERIFY(bc.layers.at(2).target.isOpaque());
    QCOMPARE(bc.layers.at(2).target.function, quint32(42));
    QCOMPARE(bc.looks.first().palettes, QList<quint32>({ Blue, Green }));
    QCOMPARE(bc.includes, QList<quint32>({ 9 }));
    QCOMPARE(back.lists.first().baseCue, quint32(3));
    QCOMPARE(back.lists.first().chaser, quint32(77));
}

/* A layer on two groups is one layer: it tracks as one, whatever order the
   groups were given in. */
void ShowTree_Test::aLayerWithTwoGroupsTracksAcrossCues()
{
    CueList list;
    list.mode = ListMode::Tracking;
    Layer both = layer(11, Towers, { look(KeyColour, { Blue }), look(KeyDimmer, { Full }) });
    both.target.groups << USTs;
    Layer again = layer(31, USTs, { look(KeyDimmer, { D45 }) });
    again.target.groups << Towers;          // the other order
    list.cues << cue(1, "1", CueMode::ListDefault, { both });
    list.cues << cue(2, "2", CueMode::ListDefault, {});
    list.cues << cue(3, "3", CueMode::ListDefault, { again });

    const QList<ResolvedCue> r = resolve(list);
    Target t;
    t.groups << USTs << Towers;
    QCOMPARE(r[1].layers.count(), 1);
    QCOMPARE(r[1].layers.first().values.value(KeyColour).look.palettes.value(0), quint32(Blue));
    QCOMPARE(int(r[1].layers.first().origin), int(Origin::Tracked));
    QCOMPARE(r[2].layers.count(), 1);
    const ResolvedLayer *l3 = r[2].layer(t);
    QVERIFY(l3 != nullptr);
    QVERIFY(l3->own);
    QCOMPARE(l3->values.value(KeyColour).look.palettes.value(0), quint32(Blue));
    QCOMPARE(int(l3->values.value(KeyColour).origin), int(Origin::Tracked));
    QCOMPARE(l3->values.value(KeyDimmer).look.palettes.value(0), quint32(D45));
    QCOMPARE(int(l3->values.value(KeyDimmer).origin), int(Origin::Own));
}

void ShowTree_Test::addingAGroupToATrackedLayer()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    FixtureGroup *b = group("USTs", { m_fixtures[1] });
    const quint32 d50 = dimmer(127, "Half");
    const quint32 d100 = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(d50));
    // Two sets Towers' colour of its own: it takes the tracked layer's place.
    const quint32 blue = colour("Blue");
    QVERIFY(st->setTargetLook(c2, groupTarget(a->id()), st->lookForPalette(blue)));
    const quint32 l2 = st->cue(c2)->layers.first().id;

    // USTs joins the layer in Two: one layer on both, still with the
    // dimmer tracked from One, and that is what Three receives.
    QVERIFY(st->addToLayer(c2, l2, { b->id() }, {}));
    QCOMPARE(st->undoText(), QString("Add USTs to layer"));
    Target both;
    both.groups << a->id() << b->id();
    QList<ResolvedCue> r = resolve(*st->cueList(listId));
    QCOMPARE(r[0].layers.count(), 1);
    QVERIFY(r[0].layer(groupTarget(a->id())) != nullptr);   // One is as it was
    QCOMPARE(r[1].layers.count(), 1);
    const ResolvedLayer *l = r[1].layer(both);
    QVERIFY(l != nullptr);
    QCOMPARE(l->values.value(KeyDimmer).look.palettes.value(0), d50);
    QCOMPARE(int(l->values.value(KeyDimmer).origin), int(Origin::Tracked));
    QCOMPARE(l->values.value(KeyColour).look.palettes.value(0), blue);
    QCOMPARE(r[2].layers.count(), 1);
    QVERIFY(r[2].layer(both) != nullptr);

    // It plays: both groups at half in Two, none of the layer twice.
    const Collection *two = qobject_cast<const Collection *>(m_doc->function(st->cue(c2)->collection));
    QCOMPARE(two->functions().count(), 1);
    st->previewCue(c2);
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(int(v[1]), 127);

    // Taking USTs away again is the tracked layer once more.
    QVERIFY(st->removeGroupFromLayer(c2, l2, b->id()));
    QVERIFY(st->cue(c2)->layers.first().continues.isEmpty());
    r = resolve(*st->cueList(listId));
    QCOMPARE(r[1].layers.count(), 1);
    QVERIFY(r[1].layer(groupTarget(a->id())) != nullptr);
    v = settle();
    QCOMPARE(int(v[1]), 0);
    // ...the last group never goes (that is Delete).
    QVERIFY(!st->removeGroupFromLayer(c2, l2, a->id()));

    // Undo brings USTs back, redo takes it away.
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c2)->layers.first().target.groups.count(), 2);
    QVERIFY(st->redo());
    QCOMPARE(st->cue(c2)->layers.first().target.groups.count(), 1);

    // Where the layer starts, the change carries on through the cues it
    // tracks into, and the same layer set again later follows it.
    const quint32 l1 = st->cue(c1)->layers.first().id;
    QVERIFY(st->setLayerLook(c3, st->addLayer(c3, groupTarget(a->id())), st->lookForPalette(d100)));
    QVERIFY(st->addToLayer(c1, l1, { b->id() }, {}));
    QCOMPARE(st->cue(c2)->layers.first().target.signature(), both.signature());
    QCOMPARE(st->cue(c3)->layers.first().target.signature(), both.signature());
    r = resolve(*st->cueList(listId));
    for (int i = 0; i < 3; ++i)
    {
        QCOMPARE(r[i].layers.count(), 1);
        QVERIFY(r[i].layer(both) != nullptr);
    }
    QCOMPARE(r[2].layer(both)->values.value(KeyColour).look.palettes.value(0), blue);
    QCOMPARE(r[2].layer(both)->values.value(KeyDimmer).look.palettes.value(0), d100);
    st->stopPreview();
}

void ShowTree_Test::aBlockCueKeepsItsOwnLayerWhenAGroupIsAdded()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    FixtureGroup *b = group("USTs", { m_fixtures[1] });
    const quint32 d50 = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    // Non-tracking (every cue a Block, as an import): only that cue changes.
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 l1 = st->addLayer(c1, groupTarget(a->id()));
    st->setLayerLook(c1, l1, st->lookForPalette(d50));
    st->setLayerLook(c2, st->addLayer(c2, groupTarget(a->id())), st->lookForPalette(d50));
    QVERIFY(st->addToLayer(c1, l1, { b->id() }, {}));
    QCOMPARE(st->cue(c1)->layers.first().target.groups.count(), 2);
    QCOMPARE(st->cue(c2)->layers.first().target.groups.count(), 1);
    QVERIFY(st->cue(c1)->layers.first().continues.isEmpty());
}

/****************************************************************************
 * Baking
 ****************************************************************************/

void ShowTree_Test::bakeMakesAChaserCollectionsAndScenes()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *b = group("USTs", { m_fixtures[2], m_fixtures[3] });
    const quint32 d50 = dimmer(127, "Half");
    const quint32 d100 = dimmer(255, "Full");

    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("DanceHysteria");
    const quint32 listId = st->addCueList(prod, "Full Show", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "Preshow");
    const quint32 c2 = st->addCue(listId, 0, "Announcer");
    const quint32 l1 = st->addLayer(c1, groupTarget(a->id()));
    const quint32 l2 = st->addLayer(c2, groupTarget(b->id()));
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(d50)));
    QVERIFY(st->setLayerLook(c2, l2, st->lookForPalette(d100)));

    const CueList *list = st->cueList(listId);
    QVERIFY(list != nullptr);
    QCOMPARE(list->cues.count(), 2);
    QCOMPARE(list->cues[0].number, QString("1"));
    QCOMPARE(list->cues[1].number, QString("2"));

    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(list->chaser));
    QVERIFY(chaser != nullptr);
    QVERIFY(chaser->isVisible());               // VC and the footer pick it
    QCOMPARE(chaser->name(), QString("Full Show"));
    QCOMPARE(chaser->path(true), QString("Productions/DanceHysteria"));
    QCOMPARE(chaser->stepsCount(), 2);
    QCOMPARE(chaser->steps().at(0).fid, list->cues[0].collection);
    QCOMPARE(chaser->steps().at(0).cueNumber, QString("1"));
    QCOMPARE(chaser->steps().at(0).hold, Function::infiniteSpeed());
    QCOMPARE(chaser->steps().at(0).duration, Function::infiniteSpeed());

    Collection *coll = qobject_cast<Collection *>(m_doc->function(list->cues[0].collection));
    QVERIFY(coll != nullptr);
    QVERIFY(!coll->isVisible());
    QCOMPARE(coll->name(), QString("1-Preshow"));
    QCOMPARE(coll->functions().count(), 1);
    Scene *scene = qobject_cast<Scene *>(m_doc->function(coll->functions().first()));
    QVERIFY(scene != nullptr);
    QVERIFY(!scene->isVisible());
    QCOMPARE(scene->name(), QString("1-Preshow · Towers"));
    QCOMPARE(scene->fixtureGroups(), QList<quint32>({ a->id() }));
    QCOMPARE(scene->palettes(), QList<quint32>({ d50 }));

    QVERIFY(st->isGenerated(chaser->id()));
    QVERIFY(st->isGenerated(coll->id()));
    QVERIFY(st->isGenerated(scene->id()));
    QVERIFY(st->describeGenerated(coll->id()).contains("1 Preshow"));

    // And it plays: GO -> Towers at half, GO -> USTs at full, Towers out.
    go(chaser);
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(int(v[2]), 0);
    go(chaser);
    v = settle();
    QCOMPARE(int(v[0]), 0);
    QCOMPARE(int(v[2]), 255);
}

void ShowTree_Test::identicalLayersShareOneScene()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    FixtureGroup *b = group("USTs", { m_fixtures[1] });
    const quint32 d50 = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(d50));
    st->setLayerLook(c2, st->addLayer(c2, groupTarget(a->id())), st->lookForPalette(d50));
    st->setLayerLook(c2, st->addLayer(c2, groupTarget(b->id())), st->lookForPalette(d50));

    const CueList *list = st->cueList(listId);
    const Collection *one = qobject_cast<const Collection *>(m_doc->function(list->cues[0].collection));
    const Collection *two = qobject_cast<const Collection *>(m_doc->function(list->cues[1].collection));
    QCOMPARE(one->functions().count(), 1);
    QCOMPARE(two->functions().count(), 2);
    QCOMPARE(two->functions().first(), one->functions().first());   // the same Scene
    QVERIFY(two->functions().last() != one->functions().first());

    // ...so GO from one to two never restarts it.
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(list->chaser));
    Scene *shared = qobject_cast<Scene *>(m_doc->function(one->functions().first()));
    go(chaser);
    tick(5);
    QSignalSpy stopped(shared, SIGNAL(stopped(quint32)));
    go(chaser);
    tick(5);
    QCOMPARE(stopped.count(), 0);
    QVERIFY(shared->isRunning());
}

static QByteArray functionsXml(Doc *doc)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    for (Function *f : doc->functions())
        f->saveXML(&w);
    return xml;
}

void ShowTree_Test::bakeIsDeterministic()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    const quint32 d50 = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(d50));
    st->setCueLook(c1, st->lookForPalette(colour("Blue")));

    const QByteArray before = functionsXml(m_doc);
    const int count = m_doc->functions().count();
    st->bake();
    st->bake();
    QCOMPARE(m_doc->functions().count(), count);
    QCOMPARE(functionsXml(m_doc), before);
}

void ShowTree_Test::anEditRebakesAndDropsWhatIsUnused()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    const quint32 d50 = dimmer(127, "Half");
    const quint32 d100 = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(d50));
    const quint32 l2 = st->addLayer(c2, groupTarget(a->id()));
    st->setLayerLook(c2, l2, st->lookForPalette(d50));

    const CueList *list = st->cueList(listId);
    const quint32 shared = qobject_cast<Collection *>(m_doc->function(list->cues[0].collection))->functions().first();

    // Cue 2 changes: a scene of its own; the shared one stays for cue 1.
    st->setLayerLook(c2, l2, st->lookForPalette(d100));
    list = st->cueList(listId);
    const Collection *two = qobject_cast<Collection *>(m_doc->function(list->cues[1].collection));
    QVERIFY(two->functions().first() != shared);
    QVERIFY(m_doc->function(shared) != nullptr);

    // Cue 1 goes: its scene is used by nothing now, and goes too.
    const quint32 coll1 = list->cues[0].collection;
    QVERIFY(st->removeCue(c1));
    QVERIFY(m_doc->function(shared) == nullptr);
    QVERIFY(m_doc->function(coll1) == nullptr);
    QCOMPARE(qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser))->stepsCount(), 1);

    // The whole list goes: everything it made goes.
    const quint32 chaserId = st->cueList(listId)->chaser;
    QVERIFY(st->removeCueList(listId));
    QVERIFY(m_doc->function(chaserId) == nullptr);
    for (Function *f : m_doc->functions())
        QVERIFY2(!st->isGenerated(f->id()), qPrintable(f->name()));
}

void ShowTree_Test::undoAndRedo()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    const quint32 d50 = dimmer(127, "Half");
    const quint32 d100 = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 l1 = st->addLayer(c1, groupTarget(a->id()));
    st->setLayerLook(c1, l1, st->lookForPalette(d50));
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(d100)));
    QCOMPARE(st->undoText(), QString("Set Dimmer"));

    auto played = [&]() {
        const CueList *list = st->cueList(listId);
        const Collection *c = qobject_cast<Collection *>(m_doc->function(list->cues[0].collection));
        const Scene *s = qobject_cast<Scene *>(m_doc->function(c->functions().value(0)));
        return s == nullptr ? QList<quint32>() : s->palettes();
    };
    QCOMPARE(played(), QList<quint32>({ d100 }));
    QVERIFY(st->undo());
    QCOMPARE(played(), QList<quint32>({ d50 }));
    QVERIFY(st->undo());
    QCOMPARE(played(), QList<quint32>());          // no look yet: nothing plays
    QVERIFY(st->redo());
    QVERIFY(st->redo());
    QCOMPARE(played(), QList<quint32>({ d100 }));
    QVERIFY(!st->canRedo());

    // A new edit drops what could be redone.
    QVERIFY(st->undo());
    QVERIFY(st->canRedo());
    QVERIFY(st->renameNode(c1, "First"));
    QVERIFY(!st->canRedo());
    QCOMPARE(st->cue(c1)->name, QString("First"));
}

void ShowTree_Test::inlinePalettesComeBackWithUndo()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 l1 = st->addLayer(c1, groupTarget(a->id()));

    const quint32 pid = st->createInlinePalette(QLCPalette::Dimmer, { 77 }, "30% · 1 Towers");
    QVERIFY(st->isInlinePalette(pid));
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(pid)));
    QVERIFY(m_doc->palette(pid) != nullptr);

    QVERIFY(st->undo());
    QVERIFY(m_doc->palette(pid) == nullptr);       // used by nothing: gone
    QVERIFY(st->redo());
    QVERIFY(m_doc->palette(pid) != nullptr);       // back, as it was
    QCOMPARE(m_doc->palette(pid)->value().toInt(), 77);
    QVERIFY(st->isInlinePalette(pid));
    QCOMPARE(st->cue(c1)->layers.first().looks.first().palettes, QList<quint32>({ pid }));

    // Make reusable: an ordinary palette from then on.
    QVERIFY(st->makeReusable(pid, "Thirty", "Palettes/Dimmer"));
    QVERIFY(!st->isInlinePalette(pid));
    QCOMPARE(m_doc->palette(pid)->name(), QString("Thirty"));
}

void ShowTree_Test::aRunningSceneIsNotDeletedUnderTheShow()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    const quint32 d50 = dimmer(127, "Half");
    const quint32 d100 = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 l1 = st->addLayer(c1, groupTarget(a->id()));
    st->setLayerLook(c1, l1, st->lookForPalette(d50));

    const CueList *list = st->cueList(listId);
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(list->chaser));
    const quint32 old = qobject_cast<Collection *>(m_doc->function(list->cues[0].collection))->functions().first();
    go(chaser);
    tick(5);
    QVERIFY(m_doc->function(old)->isRunning());

    st->setLayerLook(c1, l1, st->lookForPalette(d100));
    QVERIFY(m_doc->function(old) != nullptr);      // still on stage: kept for now
    QVERIFY(m_doc->function(old)->isRunning());

    stopAll();
    st->bake();
    QVERIFY(m_doc->function(old) == nullptr);      // let go of: gone
}

static QByteArray engineXml(Doc *doc)
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

void ShowTree_Test::theTreeSavesAndLoadsWithTheShow()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    const quint32 d50 = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 listId = st->addCueList(prod, "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(d50));
    const quint32 inl = st->createInlinePalette(QLCPalette::Dimmer, { 10 }, "4%");
    st->setCueLook(c1, st->lookForPalette(inl));

    const QByteArray xml = engineXml(m_doc);
    QVERIFY(xml.contains("<ShowTree"));
    const QList<Production> saved = st->productions();
    const quint32 chaserId = st->cueList(listId)->chaser;

    Doc *other = new Doc(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other->loadXML(r, false));
    ShowTreeController *ost = other->showTree();
    QCOMPARE(ost->productions().count(), 1);
    QCOMPARE(productionXml(ost->productions().first()), productionXml(saved.first()));
    QVERIFY(ost->isGenerated(chaserId));
    QVERIFY(ost->isInlinePalette(inl));
    QVERIFY(!ost->canUndo());
    // A new node after load doesn't reuse an id.
    QVERIFY(ost->newId() > c1);

    // And a bake on the loaded show changes nothing.
    const QByteArray before = functionsXml(other);
    ost->bake();
    QCOMPARE(functionsXml(other), before);
    delete other;
}

void ShowTree_Test::aShowWithoutATreeSavesAsBefore()
{
    QVERIFY(!engineXml(m_doc).contains("ShowTree"));
}

void ShowTree_Test::lookReachCountsFixturesThatHaveIt()
{
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef("Clay Paky", "Sharpy");
    QVERIFY(def != nullptr);
    Fixture *spot = new Fixture(m_doc);
    spot->setFixtureDefinition(def, def->modes().first());
    spot->setAddress(100);
    m_doc->addFixture(spot);

    Target t;
    t.groups << group("AT-All", { m_fixtures[0], m_fixtures[1], spot->id() })->id();
    int reach = 0, total = 0;
    m_doc->showTree()->lookReach(t, KeyGobo, &reach, &total);
    QCOMPARE(reach, 1);
    QCOMPARE(total, 3);
    m_doc->showTree()->lookReach(t, KeyPosition, &reach, &total);
    QCOMPARE(reach, 1);
    m_doc->showTree()->lookReach(t, KeyDimmer, &reach, &total);
    QCOMPARE(reach, 3);
    QCOMPARE(m_doc->showTree()->lookMisses(t, KeyGobo), QList<quint32>({ m_fixtures[0], m_fixtures[1] }));
    QVERIFY(m_doc->showTree()->lookMisses(t, KeyDimmer).isEmpty());
}

/* §3A / §10.7: a gobo look by name finds that gobo on each type's own wheel
   (or the slot picked for the type); a type without it ignores it. */
void ShowTree_Test::goboPalettesMatchByNamePerFixtureType()
{
    auto spot = [this](const QString &make, const QString &model, quint32 address) {
        QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef(make, model);
        Fixture *f = new Fixture(m_doc);
        f->setName(model);
        f->setFixtureDefinition(def, def->modes().first());
        f->setAddress(address);
        m_doc->addFixture(f);
        return f;
    };
    QVERIFY(m_doc->fixtureDefCache()->fixtureDef("Chauvet", "MiN Spot RGBW") != nullptr);
    Fixture *chauvet = spot("Chauvet", "MiN Spot RGBW", 100);
    Fixture *contest = spot("Contest", "PSC-930", 200);
    Fixture *sharpy = spot("Clay Paky", "Sharpy", 300);
    Fixture *dim = m_doc->fixture(m_fixtures[0]);

    QLCPalette *gobo = new QLCPalette(QLCPalette::Gobo, m_doc);
    gobo->setName("Dots");
    gobo->setValue(71);
    m_doc->addPalette(gobo);

    // As it always was: one DMX value on every gobo channel.
    QVERIFY(!gobo->isGoboByName());
    QCOMPARE(gobo->goboValueFor(chauvet), 71);
    QCOMPARE(gobo->goboValueFor(contest), 71);
    QCOMPARE(gobo->goboValueFor(sharpy), 71);
    QCOMPARE(gobo->goboValueFor(dim), -1);

    // By name: each wheel's own Dots (its middle); the Sharpy has none.
    gobo->setGoboName("dots");
    QCOMPARE(gobo->goboValueFor(chauvet), (65 + 77) / 2);
    QCOMPARE(gobo->goboValueFor(contest), (20 + 29) / 2);   // not "Dots Shake"
    QCOMPARE(gobo->goboValueFor(sharpy), -1);
    QVERIFY(QLCPalette::sameGoboName("Break-up", "break up"));

    // A slot picked for a type wins over the name.
    gobo->setGoboValue(QLCPalette::definitionKey(sharpy), 9);
    QCOMPARE(QLCPalette::definitionKey(sharpy), QString("Clay Paky/Sharpy"));
    QCOMPARE(gobo->goboValueFor(sharpy), 9);
    gobo->setGoboValue(QLCPalette::definitionKey(contest), 25);
    QCOMPARE(gobo->goboValueFor(contest), 25);
    gobo->removeGoboValue(QLCPalette::definitionKey(contest));

    // A picture names its glass too, with its folder: Chauvet's own
    // gobo00006 is another gobo.
    QLCPalette *pic = new QLCPalette(QLCPalette::Gobo, m_doc);
    pic->setGoboName("ClayPaky/gobo00006");
    m_doc->addPalette(pic);
    QCOMPARE(pic->goboValueFor(sharpy), (4 + 7) / 2);
    QCOMPARE(pic->goboValueFor(chauvet), -1);
    pic->setGoboName("claypaky/GOBO00003");
    QCOMPARE(pic->goboValueFor(chauvet), (65 + 77) / 2);       // its Dots is that glass

    // What a scene plays: only the fixtures it applies to, each its slot.
    const QList<SceneValue> vals = gobo->valuesFromFixtures(m_doc, { chauvet->id(), contest->id(),
                                                                     sharpy->id(), dim->id() });
    QHash<quint32, int> got;
    for (const SceneValue &sv : vals)
        got.insert(sv.fxi, sv.value);
    QCOMPARE(got.count(), 3);
    QCOMPARE(got.value(chauvet->id()), 71);
    QCOMPARE(got.value(contest->id()), 24);
    QCOMPARE(got.value(sharpy->id()), 9);

    // "applies to N of M" counts the look itself.
    gobo->removeGoboValue(QLCPalette::definitionKey(sharpy));
    Target t;
    t.groups << group("AT-All", { chauvet->id(), contest->id(), sharpy->id(), dim->id() })->id();
    Look look;
    look.key = KeyGobo;
    look.palettes << gobo->id();
    int reach = 0, total = 0;
    m_doc->showTree()->lookReach(t, look, &reach, &total);
    QCOMPARE(reach, 2);
    QCOMPARE(total, 4);
    QCOMPARE(m_doc->showTree()->lookMisses(t, look), QList<quint32>({ sharpy->id(), dim->id() }));
    m_doc->showTree()->lookReach(t, KeyGobo, &reach, &total);     // the channel alone
    QCOMPARE(reach, 3);

    // Saved and loaded with the show; a plain one saves as before.
    gobo->setGoboValue(QLCPalette::definitionKey(sharpy), 9);
    QByteArray xml;
    {
        QBuffer buffer(&xml);
        buffer.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buffer);
        gobo->saveXML(&w);
    }
    QVERIFY(xml.contains("GoboName=\"dots\""));
    QLCPalette back(QLCPalette::Undefined);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(back.loadXML(r));
    QCOMPARE(back.goboName(), QString("dots"));
    QCOMPARE(back.goboValues(), gobo->goboValues());
    QCOMPARE(back.goboValueFor(contest), 24);
    QScopedPointer<QLCPalette> copy(gobo->createCopy());
    QCOMPARE(copy->goboValueFor(sharpy), 9);

    QLCPalette plain(QLCPalette::Gobo);
    plain.setValue(40);
    QByteArray plainXml;
    {
        QBuffer buffer(&plainXml);
        buffer.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buffer);
        plain.saveXML(&w);
    }
    QVERIFY(!plainXml.contains("Gobo"  "Name"));
    QVERIFY(!plainXml.contains("GoboValue"));
}

/****************************************************************************
 * Preview
 ****************************************************************************/

void ShowTree_Test::previewPlaysTheSelectedCue()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    FixtureGroup *b = group("USTs", { m_fixtures[2] });
    const quint32 d50 = dimmer(127, "Half");
    const quint32 d100 = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(d50));
    st->setLayerLook(c2, st->addLayer(c2, groupTarget(b->id())), st->lookForPalette(d100));

    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Off));
    st->previewCue(c1);
    QCOMPARE(st->previewedCue(), c1);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(int(v[2]), 0);

    // Another cue switches it.
    st->previewCue(c2);
    v = settle();
    QCOMPARE(int(v[0]), 0);
    QCOMPARE(int(v[2]), 255);

    // ...and stopping it lets go of everything; the chaser never ran.
    st->stopPreview();
    QCOMPARE(st->previewedCue(), invalidId());
    QVERIFY(st->previewFunctions().isEmpty());
    v = settle();
    QCOMPARE(int(v[0]), 0);
    QCOMPARE(int(v[2]), 0);
    QVERIFY(!m_doc->function(st->cueList(listId)->chaser)->isRunning());

    // A deleted cue stops its preview.
    st->previewCue(c1);
    QVERIFY(st->removeCue(c1));
    QCOMPARE(st->previewedCue(), invalidId());
    v = settle();
    QCOMPARE(int(v[0]), 0);
}

void ShowTree_Test::anEditShowsOnThePreviewAtOnce()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    FixtureGroup *b = group("USTs", { m_fixtures[2] });
    const quint32 d50 = dimmer(127, "Half");
    const quint32 d100 = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 l1 = st->addLayer(c1, groupTarget(a->id()));
    st->setLayerLook(c1, l1, st->lookForPalette(d50));

    st->previewCue(c1);
    QCOMPARE(int(settle()[0]), 127);
    const quint32 old = st->previewFunctions().first();

    // A changed look bakes a new scene: the preview takes it at once.
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(d100)));
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 255);
    QVERIFY(!st->previewFunctions().contains(old));
    QVERIFY(!m_doc->function(old)->isRunning());

    // A new layer shows too, and undo takes it back off the rig.
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(b->id())), st->lookForPalette(d50));
    QCOMPARE(int(settle()[2]), 127);
    QVERIFY(st->undo());
    QVERIFY(st->undo());
    v = settle();
    QCOMPARE(int(v[2]), 0);
    QCOMPARE(int(v[0]), 255);

    // The scene let go of goes at the next bake (the app sweeps for it).
    st->bake();
    QVERIFY(m_doc->function(old) == nullptr);
}

void ShowTree_Test::aSceneTwoPreviewedCuesShareRunsOn()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    FixtureGroup *b = group("USTs", { m_fixtures[1] });
    const quint32 d50 = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(d50));
    st->setLayerLook(c2, st->addLayer(c2, groupTarget(b->id())), st->lookForPalette(d50));

    st->previewCue(c1);
    tick(5);
    Scene *shared = qobject_cast<Scene *>(m_doc->function(st->previewFunctions().first()));
    QVERIFY(shared != nullptr);
    const quint32 elapsed = shared->elapsed();
    QSignalSpy stopped(shared, SIGNAL(stopped(quint32)));
    st->previewCue(c2);           // Towers tracks into Two: the same scene
    QHash<quint32, uchar> v = settle(5);
    QCOMPARE(stopped.count(), 0);
    QVERIFY(shared->isRunning());
    QVERIFY(shared->elapsed() > elapsed);
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(int(v[1]), 127);
}

void ShowTree_Test::noPreviewInLiveMode()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    const quint32 d50 = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(d50));

    st->previewCue(c1);
    QCOMPARE(int(settle()[0]), 127);

    // Live: the preview goes, and selecting a cue there plays nothing.
    m_doc->setMode(Doc::Operate);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::NotDesign));
    QVERIFY(st->previewFunctions().isEmpty());
    QCOMPARE(int(settle()[0]), 0);
    st->previewCue(c1);
    QCOMPARE(int(settle()[0]), 0);

    // Back in Design it comes back (the app stopped everything first).
    stopAll();
    m_doc->setMode(Doc::Design);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
    QCOMPARE(int(settle()[0]), 127);
}

void ShowTree_Test::inDesignTheEditWinsOverARunningList()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    FixtureGroup *b = group("USTs", { m_fixtures[2] });
    const quint32 d50 = dimmer(127, "Half");
    const quint32 d100 = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 l1 = st->addLayer(c1, groupTarget(a->id()));
    st->setLayerLook(c1, l1, st->lookForPalette(d100));
    st->setLayerLook(c2, st->addLayer(c2, groupTarget(b->id())), st->lookForPalette(d100));
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QCOMPARE(st->listsRunning(), QStringList());

    // The list runs cue One for real (footer GO, a VC cue list).
    go(chaser);
    QCOMPARE(int(settle()[0]), 255);
    QCOMPARE(st->listsRunning(), QStringList() << "L");

    // Selecting the running cue previews it, and an edit DOWN shows: the
    // preview wins over the running cue (HTP alone would keep 255).
    st->previewCue(c1);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
    QVERIFY(!st->previewFunctions().isEmpty());
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(d50)));
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 127);
    QVERIFY(chaser->isRunning());

    // Stopping the preview: the stage is what really runs again -- the list
    // kept going underneath with the cue it started (Live edits land on GO).
    st->stopPreview();
    v = settle();
    QCOMPARE(int(v[0]), 255);
    QVERIFY(chaser->isRunning());

    // Another cue of the running list: it shows on top; the running cue
    // stays on the channels the preview doesn't play.
    st->previewCue(c2);
    v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[2]), 255);
    st->stopPreview();
    v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[2]), 0);

    // The list starting while previewing doesn't take the preview away.
    chaser->stop(FunctionParent::master());
    tick(5);
    QCoreApplication::processEvents();
    st->previewCue(c1);
    QCOMPARE(int(settle()[0]), 127);
    go(chaser);
    tick(2);
    QCoreApplication::processEvents();
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
    QCOMPARE(int(settle()[0]), 127);
    st->stopPreview();
    // GO baked the edit into cue One: the running cue now plays it.
    QCOMPARE(int(settle()[0]), 127);
}

/****************************************************************************
 * Import
 ****************************************************************************/

namespace
{
Scene *groupScene(Doc *doc, const QString &name, quint32 grp, QList<quint32> pals)
{
    Scene *s = new Scene(doc);
    s->setName(name);
    s->addFixtureGroup(grp);
    for (quint32 p : pals)
        s->addPalette(p);
    doc->addFunction(s);
    return s;
}

Collection *collection(Doc *doc, const QString &name, QList<quint32> members)
{
    Collection *c = new Collection(doc);
    c->setName(name);
    doc->addFunction(c);
    for (quint32 m : members)
        c->addFunction(m);
    return c;
}
}

void ShowTree_Test::importReadsScenesIncludesAndOpaqueFunctions()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *b = group("USTs", { m_fixtures[2] });
    const quint32 dA = dimmer(100, "A");
    const quint32 dB = dimmer(200, "B");
    Scene *sA = groupScene(m_doc, "TowersA", a->id(), { dA });
    Scene *sB = groupScene(m_doc, "USTsB", b->id(), { dB });
    Scene *raw = new Scene(m_doc);
    raw->setName("Haze");
    raw->setValue(m_fixtures[5], 0, 90);
    m_doc->addFunction(raw);
    Collection *one = collection(m_doc, "0.0.1-One", { sA->id() });
    Collection *two = collection(m_doc, "0.0.2-Two", { one->id(), sB->id() });
    Collection *three = collection(m_doc, "Three", { raw->id(), sA->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    chaser->setDurationMode(Chaser::PerStep);   // each step holds until GO
    chaser->setGoQuantize(Chaser::GoOnBeat);
    m_doc->addFunction(chaser);
    ChaserStep s1(one->id(), 1000, Function::infiniteSpeed(), 500);
    s1.note = "After setup";
    chaser->addStep(s1);
    chaser->addStep(ChaserStep(two->id(), 0, Function::infiniteSpeed(), 0));
    ChaserStep s3(three->id(), 0, Function::infiniteSpeed(), 0);
    s3.cueNumber = "7";
    chaser->addStep(s3);
    const QByteArray originals = functionsXml(m_doc);

    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importChaser(chaser->id());
    QCOMPARE(rep.cues, 3);
    QCOMPARE(rep.layers, 3);
    QCOMPARE(rep.opaqueLayers, 1);
    QCOMPARE(rep.includes, 1);
    QCOMPARE(rep.generatedScenes, 2);    // Towers A (cues 1, 2 via include, 3) and USTs B
    QCOMPARE(rep.opaqueReasons.count(), 1);
    QVERIFY(rep.opaqueReasons.first().startsWith("Haze"));
    QVERIFY(rep.htpConflicts.isEmpty());

    const CueList *list = st->cueList(rep.list);
    QVERIFY(list != nullptr);
    QCOMPARE(list->mode, ListMode::NonTracking);
    QCOMPARE(st->production(rep.production)->name, QString("Full Show"));
    QCOMPARE(list->cues[0].number, QString("0.0.1"));
    QCOMPARE(list->cues[0].name, QString("One"));
    QCOMPARE(list->cues[0].note, QString("After setup"));
    QCOMPARE(list->cues[0].fadeIn, 1000u);
    QCOMPARE(list->cues[0].fadeOut, 500u);
    QCOMPARE(list->cues[1].includes, QList<quint32>({ list->cues[0].id }));
    QCOMPARE(list->cues[2].number, QString("7"));
    QCOMPARE(list->cues[2].name, QString("Three"));
    QVERIFY(list->cues[2].layers.first().target.isOpaque());
    QCOMPARE(list->cues[2].layers.first().target.function, raw->id());

    // The originals are untouched...
    const QByteArray after = functionsXml(m_doc);
    QVERIFY(after.contains(originals.left(originals.indexOf("</Function>"))));
    QCOMPARE(chaser->stepsCount(), 3);
    QCOMPARE(two->functions(), QList<quint32>({ one->id(), sB->id() }));
    // ...and the new list is a chaser of its own.
    QVERIFY(list->chaser != chaser->id());
    QVERIFY(st->isGenerated(list->chaser));

    // One undo step takes the whole import back.
    const quint32 generatedChaser = list->chaser;
    QVERIFY(st->undo());
    QVERIFY(st->productions().isEmpty());
    QCOMPARE(functionsXml(m_doc), originals);

    // Redo brings it back with the chaser's own settings, not a plain one.
    QVERIFY(st->redo());
    const CueList *again = st->cueList(rep.list);
    QVERIFY(again != nullptr);
    QCOMPARE(again->chaser, generatedChaser);
    const Chaser *c = qobject_cast<const Chaser *>(m_doc->function(generatedChaser));
    QVERIFY(c != nullptr);
    QCOMPARE(c->durationMode(), Chaser::PerStep);
    QCOMPARE(c->runOrder(), chaser->runOrder());
    QCOMPARE(c->goQuantize(), Chaser::GoOnBeat);
    QCOMPARE(c->stepsCount(), 3);
    QVERIFY(st->isGenerated(generatedChaser));
}

void ShowTree_Test::importedListPlaysAsTheChaserDid()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *b = group("USTs", { m_fixtures[2], m_fixtures[3] });
    FixtureGroup *c = group("Steps", { m_fixtures[4] });
    const quint32 d30 = dimmer(80, "Dim");
    const quint32 d70 = dimmer(180, "Mid");
    const quint32 d100 = dimmer(255, "Full");
    Scene *aMid = groupScene(m_doc, "Towers-Mid", a->id(), { d70 });
    Scene *bFull = groupScene(m_doc, "USTs-Full", b->id(), { d100 });
    Scene *cDim = groupScene(m_doc, "Steps-Dim", c->id(), { d30 });
    Scene *cFull = groupScene(m_doc, "Steps-Full", c->id(), { d100 });
    Scene *raw = new Scene(m_doc);
    raw->setName("Haze");
    raw->setValue(m_fixtures[5], 0, 90);
    m_doc->addFunction(raw);
    // DanceHysteria's shape: a cue carried forward as a collection of the one before.
    Collection *one = collection(m_doc, "0.0.1-One", { aMid->id() });
    Collection *two = collection(m_doc, "0.0.2-Two", { aMid->id(), bFull->id(), cDim->id() });
    Collection *three = collection(m_doc, "0.0.3-Three", { two->id(), cFull->id() });
    Collection *four = collection(m_doc, "0.0.4-Four", { three->id(), raw->id() });
    Collection *five = collection(m_doc, "0.0.5-Five", { bFull->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    chaser->setDurationMode(Chaser::PerStep);   // each step holds until GO
    m_doc->addFunction(chaser);
    for (Collection *coll : { one, two, three, four, five })
        chaser->addStep(ChaserStep(coll->id(), 0, Function::infiniteSpeed(), 0));

    // The original, cue by cue.
    QList<QHash<quint32, uchar>> original;
    for (int i = 0; i < 5; ++i)
    {
        go(chaser);
        original << settle(10);
    }
    stopAll();
    QCOMPARE(int(original[3][5]), 90);          // the haze in cue 4
    QCOMPARE(int(original[2][4]), 255);         // Steps-Full beats Steps-Dim (HTP)

    const ShowTreeController::ImportReport rep = m_doc->showTree()->importChaser(chaser->id());
    Chaser *imported = qobject_cast<Chaser *>(m_doc->function(m_doc->showTree()->cueList(rep.list)->chaser));
    QVERIFY(imported != nullptr);
    QCOMPARE(imported->stepsCount(), 5);
    for (int i = 0; i < 5; ++i)
    {
        go(imported);
        const QHash<quint32, uchar> now = settle(10);
        for (quint32 ch = 0; ch < quint32(m_fixtures.count()); ++ch)
            QVERIFY2(now.value(ch) == original[i].value(ch),
                     qPrintable(QString("cue %1, channel %2: %3, the chaser gave %4")
                                .arg(i + 1).arg(ch).arg(now.value(ch)).arg(original[i].value(ch))));
    }
}

void ShowTree_Test::importListsHtpConflicts()
{
    FixtureGroup *a = group("StepTops", { m_fixtures[0] });
    const quint32 dim = dimmer(60, "Dim");
    const quint32 bright = dimmer(250, "Bright");
    Scene *sDim = groupScene(m_doc, "StepTops-Dim", a->id(), { dim });
    Scene *sBright = groupScene(m_doc, "StepTops-Bright", a->id(), { bright });
    Collection *shout = collection(m_doc, "0.0.4-SHOUT", { sDim->id() });
    Collection *who = collection(m_doc, "0.0.5-WHO ARE WE", { shout->id(), sBright->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    chaser->setDurationMode(Chaser::PerStep);   // each step holds until GO
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(shout->id(), 0, Function::infiniteSpeed(), 0));
    chaser->addStep(ChaserStep(who->id(), 0, Function::infiniteSpeed(), 0));

    const ShowTreeController::ImportReport rep = m_doc->showTree()->importChaser(chaser->id());
    QCOMPARE(rep.htpConflicts.count(), 1);
    QVERIFY(rep.htpConflicts.first().contains("0.0.5 WHO ARE WE"));
    QVERIFY(rep.htpConflicts.first().contains("StepTops"));

    // 0.0.5 now plays StepTops at Bright alone: going darker would work too.
    const CueList *list = m_doc->showTree()->cueList(rep.list);
    const QList<ResolvedCue> r = resolve(*list);
    QCOMPARE(ShowTreeController::scenePalettes(*r[1].layer(groupTarget(a->id()))),
             QList<quint32>({ bright }));
}

/* A dry run on a real show: QLC_SHOWTREE_IMPORT=<a COPY of a .qxw/.qlcc>.
   Every chaser in it is imported (in memory only -- nothing is saved) and
   the report printed: layers vs opaque layers, HTP conflicts, generated
   scenes. Skipped without the variable. */
void ShowTree_Test::importARealShow()
{
    const QString path = qEnvironmentVariable("QLC_SHOWTREE_IMPORT");
    if (path.isEmpty())
        QSKIP("set QLC_SHOWTREE_IMPORT to a copy of a show to dry-run the import");

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader r(&file);
    bool loaded = false;
    while (!r.atEnd() && !loaded)
    {
        if (r.readNextStartElement() && r.name() == QLatin1String("Engine"))
            loaded = m_doc->loadXML(r, false);
    }
    QVERIFY(loaded);
    file.close();

    QList<quint32> chasers;
    for (Function *f : m_doc->functions())
        if (f->type() == Function::ChaserType)
            chasers << f->id();
    std::sort(chasers.begin(), chasers.end());
    for (quint32 id : chasers)
    {
        const QString name = m_doc->function(id)->name();
        const ShowTreeController::ImportReport rep = m_doc->showTree()->importChaser(id);
        qInfo().noquote() << QString("IMPORT \"%1\": %2 cues, %3 layers, %4 opaque layers, "
                                     "%5 includes, %6 generated scenes, %7 HTP conflicts")
                             .arg(name).arg(rep.cues).arg(rep.layers).arg(rep.opaqueLayers)
                             .arg(rep.includes).arg(rep.generatedScenes).arg(rep.htpConflicts.count());
        for (const QString &s : rep.opaqueReasons)
            qInfo().noquote() << "  opaque:" << s;
        for (const QString &s : rep.htpConflicts)
            qInfo().noquote() << "  HTP:" << s;
        for (const QString &s : rep.notes)
            qInfo().noquote() << "  note:" << s;
        const CueList *list = m_doc->showTree()->cueList(rep.list);
        if (list == nullptr)
            continue;
        for (int i = 0; i < list->cues.count(); ++i)
            qInfo().noquote() << "  cue" << m_doc->showTree()->cueLabel(*list, i)
                              << "layers" << list->cues[i].layers.count()
                              << "includes" << list->cues[i].includes.count();

        // Play both, cue by cue, and compare every channel of every universe
        // (static looks only: effects run on the effect runner's own timer).
        auto playAll = [this](Chaser *c) {
            QList<QByteArray> states;
            for (int i = 0; i < c->stepsCount(); ++i)
            {
                ChaserAction a;
                a.m_action = ChaserSetStepIndex;
                a.m_stepIndex = i;
                a.m_masterIntensity = 1.0;
                a.m_stepIntensity = 1.0;
                a.m_fadeMode = Chaser::FromFunction;
                c->setAction(a);
                if (!c->isRunning())
                    c->start(m_doc->masterTimer(), FunctionParent::master());
                QByteArray all;
                for (int t = 0; t < 250; ++t)
                {
                    m_doc->masterTimer()->timerTick();
                    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
                    for (Universe *u : ua)
                        u->processFaders();
                    m_doc->inputOutputMap()->releaseUniverses(false);
                }
                QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
                for (Universe *u : ua)
                    all += u->preGMValues();
                m_doc->inputOutputMap()->releaseUniverses(false);
                states << all;
            }
            stopAll();
            tick(100);
            m_doc->inputOutputMap()->resetUniverses();
            return states;
        };
        Chaser *original = qobject_cast<Chaser *>(m_doc->function(id));
        Chaser *imported = qobject_cast<Chaser *>(m_doc->function(list->chaser));
        QVERIFY(original != nullptr && imported != nullptr);
        const QList<QByteArray> a = playAll(original);
        const QList<QByteArray> b = playAll(imported);
        QCOMPARE(a.count(), b.count());
        for (int i = 0; i < a.count(); ++i)
        {
            int lit = 0, differ = 0;
            for (int k = 0; k < a[i].size(); ++k)
            {
                if (a[i].at(k) != 0)
                    ++lit;
                if (k >= b[i].size() || a[i].at(k) != b[i].at(k))
                    ++differ;
            }
            qInfo().noquote() << QString("  play cue %1: %2 channels lit by the chaser, %3 differ in the cue list")
                                 .arg(m_doc->showTree()->cueLabel(*list, i)).arg(lit).arg(differ);
        }
    }
}

/****************************************************************************
 * P2: tracking, Update cue / Record (D22, D23), usage, the command line
 ****************************************************************************/

QList<QHash<quint32, uchar>> ShowTree_Test::playAll(Chaser *c)
{
    // Every cue from the top, as GO would play it.
    stopAll();
    QList<QHash<quint32, uchar>> out;
    for (int i = 0; i < c->stepsCount(); ++i)
    {
        go(c);
        out << settle(10);
    }
    stopAll();
    return out;
}

namespace
{
quint32 ownLayer(const ShowTreeController *st, quint32 cueId, quint32 groupId)
{
    if (const Cue *c = st->cue(cueId))
        for (const Layer &l : c->layers)
            if (l.target.groups == QList<quint32>({ groupId }))
                return l.id;
    return invalidId();
}

QSet<quint32> idSet(const QList<quint32> &ids)
{
    return QSet<quint32>(ids.begin(), ids.end());
}
}

/* The re-bake chain (§4): this cue, the following ones until a Block (a
   Cue-only cue passes on only what it started from), every includer, and
   the cues a released layer goes back to the base in. */
void ShowTree_Test::theRebakeChain()
{
    CueList list;
    list.mode = ListMode::Tracking;
    list.baseCue = 1;
    list.cues << cue(1, "1", CueMode::Block, { layer(11, ATSpots, { look(KeyDimmer, { BaseDim }) }) });
    list.cues << cue(2, "2", CueMode::ListDefault, { layer(21, ATSpots, { look(KeyDimmer, { Full }) }, true) });
    list.cues << cue(3, "3", CueMode::ListDefault, { layer(31, BackStage, { look(KeyDimmer, { D45 }) }) });
    list.cues << cue(4, "4", CueMode::CueOnly, { layer(41, ATWAll, { look(KeyDimmer, { Full }) }) });
    list.cues << cue(5, "5", CueMode::ListDefault, { layer(51, StepTops, { look(KeyDimmer, { Full }) }) });
    list.cues << cue(6, "6", CueMode::Block, { layer(61, StepTops, { look(KeyDimmer, { D45 }) }) });
    list.cues << cue(7, "7", CueMode::ListDefault, { layer(71, Towers, { look(KeyDimmer, { Full }) }) });
    list.cues << cue(8, "8", CueMode::Block, {}, {}, { 3 });

    QCOMPARE(rebakeChain(list, 3), QList<quint32>({ 3, 4, 5, 8 }));
    QCOMPARE(rebakeChain(list, 4), QList<quint32>({ 4 }));        // cue-only: its own only
    QCOMPARE(rebakeChain(list, 6), QList<quint32>({ 6, 7 }));
    QCOMPARE(rebakeChain(list, 7), QList<quint32>({ 7 }));
    QCOMPARE(rebakeChain(list, 1), QList<quint32>({ 1, 2, 3, 4, 5, 8 }));
    QVERIFY(rebakeChain(list, 99).isEmpty());

    // The base reaches past a Block only through a release: 6 releases.
    list.cues[5].layers[0].release = true;
    QCOMPARE(rebakeChain(list, 1), QList<quint32>({ 1, 2, 3, 4, 5, 7, 8 }));
    // An includer of an includer.
    list.cues << cue(9, "9", CueMode::Block, {}, {}, { 8 });
    QCOMPARE(rebakeChain(list, 3), QList<quint32>({ 3, 4, 5, 8, 9 }));
}

/* What a bake really changes is inside the chain -- and is the chain where
   every cue in it plays what changed. */
void ShowTree_Test::aBakeChangesOnlyTheChain()
{
    FixtureGroup *at = group("AT-Spots", { m_fixtures[0] });
    FixtureGroup *bs = group("BackStage", { m_fixtures[1] });
    FixtureGroup *aw = group("AT-W-ALL", { m_fixtures[2] });
    FixtureGroup *stp = group("StepTops", { m_fixtures[3] });
    FixtureGroup *tw = group("Towers", { m_fixtures[4] });
    const quint32 base = dimmer(60, "Base");
    const quint32 d45 = dimmer(115, "45%");
    const quint32 full = dimmer(255, "Full");

    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    QList<quint32> c;
    for (int i = 0; i < 8; ++i)
        c << st->addCue(listId, i - 1, QString("C%1").arg(i + 1));
    auto set = [&](int i, FixtureGroup *g, quint32 pal, bool release = false) {
        const quint32 l = st->addLayer(c[i], groupTarget(g->id()));
        QVERIFY(st->setLayerLook(c[i], l, st->lookForPalette(pal)));
        if (release)
            QVERIFY(st->setLayerRelease(c[i], l, true));
    };
    set(0, at, base);
    set(1, at, full, true);
    set(2, bs, d45);
    set(3, aw, full);
    set(4, stp, full);
    set(5, stp, d45);
    set(6, tw, full);
    QVERIFY(st->setBaseCue(listId, c[0]));
    QVERIFY(st->setCueMode(c[0], CueMode::Block));
    QVERIFY(st->setCueMode(c[3], CueMode::CueOnly));
    QVERIFY(st->setCueMode(c[5], CueMode::Block));
    QVERIFY(st->setCueMode(c[7], CueMode::Block));
    QVERIFY(st->addInclude(c[7], c[2]));

    // BackStage changes in cue 3: 3, 4 (cue-only, starts from 3), 5, and 8
    // (includes 3) play it; 6 and 7 come after a Block.
    const QList<quint32> chain3 = rebakeChain(*st->cueList(listId), c[2]);
    QCOMPARE(chain3, QList<quint32>({ c[2], c[3], c[4], c[7] }));
    QVERIFY(st->setLayerLook(c[2], ownLayer(st, c[2], bs->id()), st->lookForPalette(full)));
    QCOMPARE(st->lastBakeChanged(), chain3);

    // The base's AT-Spots level: cue 2 sets its own, so it doesn't change;
    // everything after the release does.
    const QList<quint32> chain1 = rebakeChain(*st->cueList(listId), c[0]);
    QVERIFY(st->setLayerLook(c[0], ownLayer(st, c[0], at->id()), st->lookForPalette(d45)));
    QVERIFY(idSet(chain1).contains(idSet(st->lastBakeChanged())));
    QCOMPARE(st->lastBakeChanged(), QList<quint32>({ c[0], c[2], c[3], c[4], c[7] }));

    // A cue-only cue's own change stays in it.
    QVERIFY(st->setLayerLook(c[3], ownLayer(st, c[3], aw->id()), st->lookForPalette(d45)));
    QCOMPARE(st->lastBakeChanged(), QList<quint32>({ c[3] }));

    // And it plays that way: GO through the list.
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    const QList<QHash<quint32, uchar>> v = playAll(chaser);
    QCOMPARE(int(v[0][0]), 115);        // the base, at its new level
    QCOMPARE(int(v[1][0]), 255);
    QCOMPARE(int(v[2][0]), 115);        // released: back to the base
    QCOMPARE(int(v[2][1]), 255);
    QCOMPARE(int(v[3][2]), 115);        // cue-only flash
    QCOMPARE(int(v[4][2]), 0);          // ...gone in the next cue
    QCOMPARE(int(v[4][1]), 255);        // BackStage tracked on
    QCOMPARE(int(v[5][1]), 0);          // Block
    QCOMPARE(int(v[5][3]), 115);
    QCOMPARE(int(v[6][3]), 115);
    QCOMPARE(int(v[6][4]), 255);
    QCOMPARE(int(v[7][1]), 255);        // includes cue 3
    QCOMPARE(int(v[7][0]), 115);        // ...with what was on in it
    QCOMPARE(int(v[7][4]), 0);
}

void ShowTree_Test::includesAreCycleProof()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    QVERIFY(st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(full)));
    QVERIFY(st->setCueMode(c3, CueMode::Block));

    QString why;
    QVERIFY(!st->addInclude(c1, c1, &why));
    QVERIFY(!st->addInclude(c1, c2, &why));         // 2 tracks from 1
    QVERIFY(why.contains("loop"));
    QVERIFY(st->addInclude(c3, c1, &why));
    QVERIFY(st->addInclude(c3, c2, &why));
    QVERIFY(!st->addInclude(c3, c2, &why));         // already
    QVERIFY(!st->addInclude(c1, c3, &why));         // 3 includes 1
    QCOMPARE(st->cue(c3)->includes, QList<quint32>({ c1, c2 }));

    // Reorder: later wins.
    QVERIFY(st->moveInclude(c3, c2, 0));
    QCOMPARE(st->cue(c3)->includes, QList<quint32>({ c2, c1 }));
    QVERIFY(!st->moveInclude(c3, c2, 0));
    QCOMPARE(st->undoText(), QString("Move include"));
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c3)->includes, QList<quint32>({ c1, c2 }));

    // setCue can't sneak a loop in either.
    Cue copy = *st->cue(c1);
    copy.includes << c3 << c2;
    QVERIFY(st->setCue(copy));
    QVERIFY(st->cue(c1)->includes.isEmpty());

    QVERIFY(st->removeInclude(c3, c1));
    QCOMPARE(st->cue(c3)->includes, QList<quint32>({ c2 }));
    QVERIFY(!st->removeInclude(c3, c1));
    // Now 1 may include 3: 3 no longer plays from 1 at any depth? It still
    // includes 2, which tracks from 1.
    QVERIFY(!st->addInclude(c1, c3, &why));
}

/* D22: a tracked value's picker offers "Update cue X" -- where it was set,
   so the change tracks on from there. */
void ShowTree_Test::updateCueWhereATrackedValueWasSet()
{
    FixtureGroup *bs = group("BackStage", { m_fixtures[0] });
    FixtureGroup *stp = group("StepTops", { m_fixtures[1] });
    const quint32 d45 = dimmer(115, "45%");
    const quint32 full = dimmer(255, "Full");
    const quint32 blue = colour("Blue");
    const quint32 red = colour("Red");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    const quint32 c4 = st->addCue(listId, 2, "Four");
    const quint32 bl = st->addLayer(c1, groupTarget(bs->id()));
    QVERIFY(st->setLayerLook(c1, bl, st->lookForPalette(d45)));
    QVERIFY(st->setCueLook(c1, st->lookForPalette(blue)));
    QVERIFY(st->setLayerLook(c2, st->addLayer(c2, groupTarget(stp->id())), st->lookForPalette(d45)));
    QVERIFY(st->setCueMode(c4, CueMode::Block));
    QVERIFY(st->addInclude(c4, c2));

    quint32 src = invalidId(), srcLayer = invalidId();
    QVERIFY(st->valueSource(c3, groupTarget(bs->id()), KeyDimmer, &src, &srcLayer));
    QCOMPARE(src, c1);
    QCOMPARE(srcLayer, bl);
    // Set by the cue's look there: no layer.
    QVERIFY(st->valueSource(c3, groupTarget(bs->id()), KeyColour, &src, &srcLayer));
    QCOMPARE(src, c1);
    QCOMPARE(srcLayer, invalidId());
    // Own here: nothing to update elsewhere. Nowhere: neither.
    QVERIFY(!st->valueSource(c1, groupTarget(bs->id()), KeyDimmer, &src));
    QVERIFY(!st->valueSource(c3, groupTarget(bs->id()), KeyGobo, &src));
    // Included: where the included cue got it.
    QVERIFY(st->valueSource(c4, groupTarget(stp->id()), KeyDimmer, &src));
    QCOMPARE(src, c2);

    // Update cue 1 from cue 3: one step, cue 3 still tracks it.
    QVERIFY(st->updateSource(c3, groupTarget(bs->id()), st->lookForPalette(full)));
    QCOMPARE(st->undoText(), QString("Update cue 1 One: Dimmer"));
    QCOMPARE(st->cue(c1)->layer(bl)->look(KeyDimmer)->palettes, QList<quint32>({ full }));
    QVERIFY(st->cue(c3)->layers.isEmpty());
    const QList<ResolvedCue> r = resolve(*st->cueList(listId));
    CHECK_VALUE(r[2], bs->id(), KeyDimmer, full, Origin::Tracked, c1);
    // Through a cue look.
    QVERIFY(st->updateSource(c3, groupTarget(bs->id()), st->lookForPalette(red)));
    QCOMPARE(st->cue(c1)->look(KeyColour)->palettes, QList<quint32>({ red }));
    // Own value: refused (the picker's plain Set covers it).
    QVERIFY(!st->updateSource(c1, groupTarget(bs->id()), st->lookForPalette(d45)));

    QVERIFY(st->undo());
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c1)->layer(bl)->look(KeyDimmer)->palettes, QList<quint32>({ d45 }));
    QCOMPARE(st->cue(c1)->look(KeyColour)->palettes, QList<quint32>({ blue }));
}

/* D22 + D23: Record as new cue freezes what a cue puts on stage into a new
   Block cue, numbered after it; it plays the same scenes. */
void ShowTree_Test::recordAsNewCueFreezesTheStage()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    FixtureGroup *c = group("C", { m_fixtures[2] });
    const quint32 d45 = dimmer(115, "45%");
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    QVERIFY(st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(full)));
    QVERIFY(st->setLayerLook(c2, st->addLayer(c2, groupTarget(b->id())), st->lookForPalette(d45)));
    QVERIFY(st->setLayerLook(c3, st->addLayer(c3, groupTarget(c->id())), st->lookForPalette(full)));
    // "None" on a layer: nothing on stage, so not recorded.
    QVERIFY(st->setTargetLook(c2, groupTarget(a->id()), noneLook(KeyDimmer)));

    const quint32 rec = st->recordCue(c2);
    QVERIFY(rec != invalidId());
    QCOMPARE(st->undoText(), QString("Record as new cue"));
    const CueList *list = st->cueList(listId);
    QCOMPARE(list->indexOf(rec), 2);
    QCOMPARE(list->cue(rec)->number, QString("2.5"));      // 3 is taken: a point cue (D23)
    QCOMPARE(list->cue(rec)->mode, CueMode::Block);
    QCOMPARE(list->cue(rec)->name, QString("Two (recorded)"));
    QCOMPARE(list->cue(rec)->layers.count(), 1);           // B; A is None there
    QVERIFY(list->cue(rec)->layers.first().id != list->cue(c2)->layers.first().id);

    // The same stage as cue 2: the same scenes.
    const Collection *two = qobject_cast<const Collection *>(m_doc->function(list->cue(c2)->collection));
    const Collection *recorded = qobject_cast<const Collection *>(m_doc->function(list->cue(rec)->collection));
    QCOMPARE(idSet(recorded->functions()), idSet(two->functions()));

    // 3 now tracks from the recorded cue, still on.
    const QList<ResolvedCue> r = resolve(*list);
    CHECK_VALUE(r[3], b->id(), KeyDimmer, d45, Origin::Tracked, rec);
    QVERIFY(st->undo());
    QCOMPARE(st->cueList(listId)->cues.count(), 3);
}

void ShowTree_Test::duplicateACue()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    QVERIFY(st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(full)));
    QVERIFY(st->setBaseCue(listId, c1));
    const quint32 d = st->duplicateCue(c1);
    const CueList *list = st->cueList(listId);
    QCOMPARE(list->indexOf(d), 1);
    QCOMPARE(list->cue(d)->number, QString("1.5"));
    QCOMPARE(list->cue(d)->layers.count(), 1);
    QVERIFY(list->cue(d)->layers.first().id != list->cue(c1)->layers.first().id);
    QCOMPARE(list->baseCue, c1);                             // never the base
    QCOMPARE(list->indexOf(c2), 2);
}

/* D20: "In this cue list" -- what the list's cues use, with counts. */
void ShowTree_Test::usageInAList()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 d45 = dimmer(115, "45%");
    const quint32 full = dimmer(255, "Full");
    const quint32 blue = colour("Blue");
    Scene *raw = new Scene(m_doc);
    raw->setValue(m_fixtures[5], 0, 90);
    m_doc->addFunction(raw);
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    QVERIFY(st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(full)));
    QVERIFY(st->setLayerLook(c2, st->addLayer(c2, groupTarget(a->id())), st->lookForPalette(full)));
    QVERIFY(st->setLayerLook(c2, st->addLayer(c2, groupTarget(b->id())), st->lookForPalette(d45)));
    QVERIFY(st->setCueLook(c2, st->lookForPalette(blue)));
    Target t;
    t.function = raw->id();
    QVERIFY(st->addLayer(c1, t) != invalidId());

    const ShowTreeController::Usage u = st->usageInList(listId);
    QCOMPARE(u.groups.value(a->id()), 2);
    QCOMPARE(u.groups.value(b->id()), 1);
    QCOMPARE(u.palettes.value(full), 2);
    QCOMPARE(u.palettes.value(d45), 1);
    QCOMPARE(u.palettes.value(blue), 1);
    QCOMPARE(u.functions.value(raw->id()), 1);
    QVERIFY(st->usageInList(invalidId()).groups.isEmpty());
}

/* Design: previewing a cue shows its whole tracked state. */
void ShowTree_Test::previewShowsTheTrackedState()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 d45 = dimmer(115, "45%");
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    QVERIFY(st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(full)));
    QVERIFY(st->setLayerLook(c2, st->addLayer(c2, groupTarget(b->id())), st->lookForPalette(d45)));
    Q_UNUSED(c3)

    st->previewCue(c3);
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 255);       // tracked from 1
    QCOMPARE(int(v[1]), 115);       // tracked from 2
    // An edit where it was set shows in the previewed cue at once.
    QVERIFY(st->updateSource(c3, groupTarget(a->id()), st->lookForPalette(d45)));
    v = settle();
    QCOMPARE(int(v[0]), 115);
    // Block: only its own (nothing).
    QVERIFY(st->setCueMode(c3, CueMode::Block));
    v = settle();
    QCOMPARE(int(v[0]), 0);
    QCOMPARE(int(v[1]), 0);
    st->stopPreview();
}

/* Import -> Convert manually (P4 will do it for you): the list made
   Tracking by hand plays the §3.2, §3.3 and §3.4 examples, on the rig. */
void ShowTree_Test::convertAnImportToTracking()
{
    FixtureGroup *at = group("AT-Spots", { m_fixtures[0] });
    FixtureGroup *bs = group("BackStage", { m_fixtures[1], m_fixtures[2] });
    FixtureGroup *stp = group("StepTops", { m_fixtures[3] });
    FixtureGroup *aw = group("AT-W-ALL", { m_fixtures[4] });
    const quint32 base = dimmer(60, "Base");
    const quint32 d45 = dimmer(115, "45%");
    const quint32 full = dimmer(255, "Full");
    const quint32 white = colour("White");
    const quint32 blue = colour("Blue");

    // Today's way of writing it: every cue repeats what stays on, and
    // carries looks forward as collections of collections.
    Scene *atBase = groupScene(m_doc, "AT-Base", at->id(), { white, base });
    Scene *atFull = groupScene(m_doc, "AT-Full", at->id(), { white, full });
    Scene *backStage = groupScene(m_doc, "BackStage", bs->id(), { blue, d45 });
    Scene *stepBright = groupScene(m_doc, "StepTops-Bright", stp->id(), { white, full });
    Scene *step45 = groupScene(m_doc, "StepTops-45", stp->id(), { d45 });
    Scene *flash = groupScene(m_doc, "FlashWashes", aw->id(), { white, full });
    Scene *at45 = groupScene(m_doc, "AT-45", at->id(), { d45 });
    Collection *q0 = collection(m_doc, "0.0.0-base", { atBase->id() });
    Collection *q3 = collection(m_doc, "0.0.3-Announcer", { atFull->id(), backStage->id() });
    Collection *q4 = collection(m_doc, "0.0.4-SHOUT", { atBase->id(), backStage->id(), stepBright->id() });
    Collection *q5 = collection(m_doc, "0.0.5-WHO ARE WE", { q4->id(), step45->id() });
    Collection *q6 = collection(m_doc, "0.0.6-STOMP", { q5->id(), flash->id() });
    Collection *q7 = collection(m_doc, "0.0.7-After", { q5->id() });
    Collection *q9 = collection(m_doc, "0.0.9-AnnouncerFinish", { q3->id(), at45->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    chaser->setDurationMode(Chaser::PerStep);
    m_doc->addFunction(chaser);
    for (Collection *coll : { q0, q3, q4, q5, q6, q7, q9 })
        chaser->addStep(ChaserStep(coll->id(), 0, Function::infiniteSpeed(), 0));
    const QList<QHash<quint32, uchar>> original = playAll(chaser);
    QCOMPARE(int(original[3][3]), 255);     // 0.0.5: Bright and 45% both run, the brighter wins

    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importChaser(chaser->id());
    const quint32 listId = rep.list;
    QCOMPARE(rep.cues, 7);
    QCOMPARE(rep.includes, 4);
    QCOMPARE(st->cueList(listId)->mode, ListMode::NonTracking);
    QList<quint32> c;
    for (const Cue &cue : st->cueList(listId)->cues)
        c << cue.id;
    QCOMPARE(st->cue(c[3])->includes, QList<quint32>({ c[2] }));
    Chaser *list = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));

    // As imported it plays as the chaser did, but where two scenes fought
    // over a level (0.0.5, 0.0.6, 0.0.7, 0.0.9): there the later one wins.
    QList<QHash<quint32, uchar>> v = playAll(list);
    for (int i : { 0, 1, 2 })
        for (quint32 ch = 0; ch < 5; ++ch)
            QCOMPARE(int(v[i][ch]), int(original[i][ch]));
    QCOMPARE(int(v[3][3]), 115);

    // Convert by hand: Tracking; 0.0.0 is the base; AT-Spots released in
    // 0.0.3; what only repeats the cue before goes.
    QVERIFY(st->setListMode(listId, ListMode::Tracking));
    QVERIFY(st->setBaseCue(listId, c[0]));
    QVERIFY(st->setCueMode(c[0], CueMode::Block));
    QVERIFY(st->setLayerRelease(c[1], ownLayer(st, c[1], at->id()), true));
    QVERIFY(st->removeLayer(c[2], ownLayer(st, c[2], bs->id())));
    QVERIFY(st->removeLayer(c[2], ownLayer(st, c[2], at->id())));
    QVERIFY(st->removeInclude(c[3], c[2]));
    QVERIFY(st->removeInclude(c[4], c[3]));
    QVERIFY(st->setCueMode(c[4], CueMode::CueOnly));
    QVERIFY(st->removeInclude(c[5], c[3]));
    QVERIFY(st->setCueMode(c[6], CueMode::Block));
    QCOMPARE(st->cue(c[6])->includes, QList<quint32>({ c[1] }));

    // §3.2, as values...
    QList<ResolvedCue> r = resolve(*st->cueList(listId));
    CHECK_VALUE(r[1], at->id(), KeyDimmer, full, Origin::Own, c[1]);
    CHECK_VALUE(r[1], bs->id(), KeyColour, blue, Origin::Own, c[1]);
    CHECK_VALUE(r[2], at->id(), KeyDimmer, base, Origin::Base, c[0]);
    CHECK_VALUE(r[2], bs->id(), KeyColour, blue, Origin::Tracked, c[1]);
    CHECK_VALUE(r[2], bs->id(), KeyDimmer, d45, Origin::Tracked, c[1]);
    CHECK_VALUE(r[2], stp->id(), KeyColour, white, Origin::Own, c[2]);
    CHECK_VALUE(r[3], at->id(), KeyDimmer, base, Origin::Tracked, c[0]);
    CHECK_VALUE(r[3], bs->id(), KeyDimmer, d45, Origin::Tracked, c[1]);
    CHECK_VALUE(r[3], stp->id(), KeyColour, white, Origin::Tracked, c[2]);
    CHECK_VALUE(r[3], stp->id(), KeyDimmer, d45, Origin::Own, c[3]);
    QCOMPARE(st->cue(c[3])->layers.count(), 1);         // one value of its own

    // ...and on the rig. Going darker works: StepTops 255 -> 115.
    v = playAll(list);
    QCOMPARE(int(v[0][0]), 60);
    QCOMPARE(int(v[1][0]), 255);
    QCOMPARE(int(v[1][1]), 115);
    QCOMPARE(int(v[2][0]), 60);         // released to the base
    QCOMPARE(int(v[2][1]), 115);        // BackStage tracked
    QCOMPARE(int(v[2][3]), 255);
    QCOMPARE(int(v[3][3]), 115);        // darker than 0.0.4
    QCOMPARE(int(v[3][1]), 115);
    // §3.4: the flash plays in 0.0.6 (cue-only) and is gone in 0.0.7, with
    // no Release and no "washes off" cue.
    QCOMPARE(int(v[4][4]), 255);
    QCOMPARE(int(v[4][3]), 115);
    QCOMPARE(int(v[5][4]), 0);
    QCOMPARE(int(v[5][3]), 115);
    QCOMPARE(int(v[5][1]), 115);
    // 0.0.9 Block, includes 0.0.3: the announcer look back, AT-Spots at 45%.
    QCOMPARE(int(v[6][0]), 115);
    QCOMPARE(int(v[6][1]), 115);
    QCOMPARE(int(v[6][3]), 0);
    QCOMPARE(int(v[6][4]), 0);

    // §3.3: 0.0.4 Block -- BackStage dark there, StepTops alone; 0.0.5
    // tracks from it. The bake changes 0.0.4 → 0.0.7 and nothing else.
    QVERIFY(st->setCueMode(c[2], CueMode::Block));
    QCOMPARE(st->lastBakeChanged(), QList<quint32>({ c[2], c[3], c[4], c[5] }));
    QCOMPARE(st->lastBakeChanged(), rebakeChain(*st->cueList(listId), c[2]));
    v = playAll(list);
    QCOMPARE(int(v[2][1]), 0);
    QCOMPARE(int(v[2][0]), 0);
    QCOMPARE(int(v[2][3]), 255);
    QCOMPARE(int(v[3][3]), 115);
    QCOMPARE(int(v[3][1]), 0);
    QCOMPARE(int(v[6][1]), 115);        // 0.0.9 unchanged: it includes 0.0.3

    // Change 0.0.3 later and 0.0.9 follows.
    QVERIFY(st->setLayerLook(c[1], ownLayer(st, c[1], bs->id()), st->lookForPalette(full)));
    QVERIFY(st->lastBakeChanged().contains(c[6]));
    v = playAll(list);
    QCOMPARE(int(v[6][1]), 255);
}

/* §6C: the command line calls the same operations; one undo step each. */
void ShowTree_Test::commandLine()
{
    FixtureGroup *tw = group("Towers", { m_fixtures[0] });
    FixtureGroup *us = group("US Truss", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    const quint32 blue = colour("Blue");
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = st->addProduction("P");
    st->clearUndo();

    ShowTreeCommand::Result r = cmd.run("Towers colour Blue", ctx);
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("cue list"));

    r = cmd.run("list \"01-Everybody Scream\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx = r.context;
    QCOMPARE(st->cueList(ctx.list)->name, QString("01-Everybody Scream"));
    QCOMPARE(st->cueList(ctx.list)->mode, ListMode::Tracking);

    r = cmd.run("cue 1.0.0 \"base\" block base", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx = r.context;
    const quint32 c1 = ctx.cue;
    QCOMPARE(st->cue(c1)->number, QString("1.0.0"));
    QCOMPARE(st->cue(c1)->name, QString("base"));
    QCOMPARE(st->cue(c1)->mode, CueMode::Block);
    QCOMPARE(st->cueList(ctx.list)->baseCue, c1);
    QCOMPARE(st->undoText(), QString("cue 1.0.0 \"base\" block base"));   // one step for all of it

    // Names with spaces, a level as a number, a cue look. Two groups named
    // one after the other: ONE layer, as a drop of both is.
    r = cmd.run("Towers US Truss colour Blue dimmer 45", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    const Layer &both = st->cue(c1)->layers.first();
    QCOMPARE(both.target.groups, QList<quint32>({ tw->id(), us->id() }));
    QCOMPARE(both.look(KeyColour)->palettes, QList<quint32>({ blue }));
    // 45 made a named, reusable palette (no palette played 45 %).
    const quint32 d45 = both.look(KeyDimmer)->palettes.first();
    QVERIFY(!st->isInlinePalette(d45));
    QCOMPARE(m_doc->palette(d45)->name(), QString("Dimmer 45%"));
    QCOMPARE(m_doc->palette(d45)->path(), QString("Palettes/Dimmer"));
    QVERIFY(r.message.contains("new palette"));
    QVERIFY(r.layer == both.id);
    // A comma: a layer each, as an ⌥-drop; 45 again reuses the palette.
    r = cmd.run("Towers, US Truss dimmer 45", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(c1)->layers.count(), 3);
    QCOMPARE(st->cue(c1)->layer(ownLayer(st, c1, tw->id()))->look(KeyDimmer)->palettes,
             QList<quint32>({ d45 }));
    QCOMPARE(st->cue(c1)->layer(ownLayer(st, c1, us->id()))->look(KeyDimmer)->palettes,
             QList<quint32>({ d45 }));
    QVERIFY(r.message.contains("reused"));
    QVERIFY(st->removeLayer(c1, ownLayer(st, c1, tw->id())));
    QVERIFY(st->removeLayer(c1, ownLayer(st, c1, us->id())));
    r = cmd.run("cue look dimmer Full", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(c1)->look(KeyDimmer)->palettes, QList<quint32>({ full }));

    // + : a new cue after the selected one (D23), tracking by default.
    r = cmd.run("cue + \"Verse\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx = r.context;
    const quint32 c2 = ctx.cue;
    QCOMPARE(st->cue(c2)->number, QString("1.0.1"));
    r = cmd.run("towers+\"US Truss\" dimmer Full", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(c2)->layers.count(), 1);
    QCOMPARE(st->cue(c2)->layers.first().target.groups, QList<quint32>({ tw->id(), us->id() }));

    // None, release, include, modes.
    r = cmd.run("Towers dimmer none", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(st->cue(c2)->layer(ownLayer(st, c2, tw->id()))->look(KeyDimmer)->none);
    r = cmd.run("release Towers", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(st->cue(c2)->layer(ownLayer(st, c2, tw->id()))->release);
    r = cmd.run("cueonly", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(c2)->mode, CueMode::CueOnly);
    r = cmd.run("block", ctx);
    QVERIFY(r.ok);
    QCOMPARE(st->cue(c2)->mode, CueMode::Block);
    r = cmd.run("include 1.0.0", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(c2)->includes, QList<quint32>({ c1 }));

    // Selecting by number; a loop refused; record; dup.
    r = cmd.run("cue 1.0.0", ctx);
    QVERIFY(r.ok);
    QCOMPARE(r.context.cue, c1);
    ShowTreeCommand::Result loop = cmd.run("include 1.0.1", r.context);
    QVERIFY(!loop.ok);                          // 1.0.1 includes 1.0.0
    QVERIFY(loop.message.contains("loop"));
    ShowTreeCommand::Result notRunning = cmd.run("record running", r.context);
    QVERIFY(!notRunning.ok);                    // nothing of the list runs
    QVERIFY(notRunning.message.contains("running"));
    QVERIFY(!cmd.run("record sideways", r.context).ok);
    r = cmd.run("record", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(r.context.cue)->number, QString("1.0.0.5"));
    r = cmd.run("dup", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cueList(ctx.list)->cues.count(), 4);

    // undo: the last command, whole.
    r = cmd.run("undo", r.context);
    QVERIFY(r.ok);
    QCOMPARE(st->cueList(ctx.list)->cues.count(), 3);
    r = cmd.run("redo", r.context);
    QVERIFY(r.ok);
    QCOMPARE(st->cueList(ctx.list)->cues.count(), 4);
}

/* A command that fails part way leaves the tree as it was, and no undo. */
void ShowTree_Test::commandLineFailsAsAWhole()
{
    group("Towers", { m_fixtures[0] });
    colour("Blue");
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = st->addProduction("P");
    ctx.list = st->addCueList(ctx.production, "L", ListMode::NonTracking);
    ctx.cue = st->addCue(ctx.list, -1, "One");
    const QString undo = st->undoText();
    const int palettes = m_doc->palettes().count();

    // The layer was added, a palette at 30 % made, then "Nope" isn't a
    // colour: the new palette goes too.
    ShowTreeCommand::Result r = cmd.run("Towers dimmer 30 colour Nope", ctx);
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("Nope"));
    QVERIFY(st->cue(ctx.cue)->layers.isEmpty());
    QCOMPARE(st->undoText(), undo);
    QCOMPARE(m_doc->palettes().count(), palettes);

    // A one-off made on the way goes too when a later word fails.
    const quint32 layer = st->addLayer(ctx.cue, groupTarget(m_doc->fixtureGroups().first()->id()));
    const int withLayer = m_doc->palettes().count();
    r = cmd.run("cue look dimmer 30 colour Nope", ctx);
    QVERIFY(!r.ok);
    QCOMPARE(m_doc->palettes().count(), withLayer);
    QVERIFY(st->removeLayer(ctx.cue, layer));
    const QString undoNow = st->undoText();

    r = cmd.run("Nobody colour Blue", ctx);
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("Nobody"));
    r = cmd.run("cue 2 \"Two\" sideways", ctx);
    QVERIFY(!r.ok);
    QCOMPARE(st->cueList(ctx.list)->cues.count(), 1);
    QCOMPARE(st->undoText(), undoNow);
}

void ShowTree_Test::commandLineCompletes()
{
    group("Towers", { m_fixtures[0] });
    group("Tops", { m_fixtures[1] });
    colour("Blue");
    colour("Blush");
    colour("Red");
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = st->addProduction("P");
    ctx.list = st->addCueList(ctx.production, "L", ListMode::NonTracking);
    ctx.cue = st->addCue(ctx.list, -1, "One");

    QStringList options;
    QCOMPARE(cmd.complete("Tow", ctx, &options), QString("Towers "));
    QCOMPARE(cmd.complete("To", ctx, &options), QString("To"));
    QCOMPARE(options, QStringList({ "Tops", "Towers" }));
    QCOMPARE(cmd.complete("Towers colour R", ctx), QString("Towers colour Red "));
    QCOMPARE(cmd.complete("Towers colour Bl", ctx, &options), QString("Towers colour Blu"));
    QCOMPARE(options, QStringList({ "Blue", "Blush" }));
    QCOMPARE(cmd.complete("Towers col", ctx), QString("Towers colour "));
    QCOMPARE(cmd.complete("rec", ctx), QString("record "));
    QCOMPARE(cmd.complete("record ru", ctx), QString("record running "));
    // After a ',' or a '+': the next group's name.
    QCOMPARE(cmd.complete("Towers,To", ctx, &options), QString("Towers,To"));
    QCOMPARE(options, QStringList({ "Tops", "Towers" }));
    QCOMPARE(cmd.complete("Towers+Top", ctx), QString("Towers+Tops "));
    QCOMPARE(cmd.complete("cue ", ctx, &options), QString("cue "));
    QVERIFY(options.contains("1"));
    QCOMPARE(cmd.complete("zzz", ctx, &options), QString("zzz"));
    QVERIFY(options.isEmpty());
    QVERIFY(!ShowTreeCommand::grammar().isEmpty());
}

/* Tab for every kind of word the grammar takes (2026-10-17): a show set up
   with one of each, then a table -- what is typed, what Tab makes of it,
   and a word the options must hold. */
ShowTreeCommand::Context ShowTree_Test::completionShow()
{
    group("Towers", { m_fixtures[0] });
    group("Tops", { m_fixtures[1] });
    group("AT W ALL", { m_fixtures[2] });
    group("AT W Floor", { m_fixtures[3] });
    group("Front, Left", { m_fixtures[4] });
    group("Spots", { m_fixtures[5] })->setPath("Movers/Front");
    m_doc->fixture(m_fixtures[6])->setName("T1-CW");
    colour("Blue");
    colour("Blush");
    colour("Deep Blue");
    dimmer(48, "Base");
    dimmer(255, "Full");
    QLCPalette *pos = new QLCPalette(QLCPalette::PanTilt, m_doc);
    pos->setName("Drummer");
    m_doc->addPalette(pos);
    QLCPalette *zoom = new QLCPalette(QLCPalette::Zoom, m_doc);
    zoom->setName("Narrow");
    m_doc->addPalette(zoom);
    QLCPalette *fx = new QLCPalette(QLCPalette::Effect, m_doc);
    fx->setName("Sinelon");
    m_doc->addPalette(fx);
    Scene *haze = new Scene(m_doc);
    haze->setName("Haze look");
    m_doc->addFunction(haze);
    Scene *hall = new Scene(m_doc);
    hall->setName("House lights");
    m_doc->addFunction(hall);

    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand::Context ctx;
    ctx.production = st->addProduction("P");
    ctx.list = st->addCueList(ctx.production, "01-Opener", ListMode::NonTracking);
    st->addCueList(ctx.production, "02-Verse Two", ListMode::NonTracking);
    ctx.cue = st->addCue(ctx.list, -1, "One");
    st->addCueNumbered(ctx.list, "1.2", "Two");
    st->addCueNumbered(ctx.list, "1.25", "Three");
    return ctx;
}

void ShowTree_Test::commandLineCompletesEveryWord_data()
{
    QTest::addColumn<QString>("typed");
    QTest::addColumn<QString>("completed");
    QTest::addColumn<QString>("offered");      // one of the options (or empty)

    // Command words.
    QTest::newRow("keyword") << "rec" << "record " << "record";
    QTest::newRow("keyword function") << "func" << "function " << "function";
    QTest::newRow("keyword include") << "inc" << "include " << "include";
    // Cue numbers: after cue, after include; several = the common part.
    QTest::newRow("cue number") << "cue 1.2" << "cue 1.2" << "1.25";
    QTest::newRow("cue look") << "cue lo" << "cue look " << "look";
    QTest::newRow("include number") << "include 1.25" << "include 1.25 " << "1.25";
    // Cue lists.
    QTest::newRow("list") << "list 02" << "list 02-Verse Two " << "02-Verse Two";
    QTest::newRow("list several") << "list 0" << "list 0" << "01-Opener";
    // Groups, with spaces, by folder path; a ',' in a name is quoted.
    QTest::newRow("group") << "Tow" << "Towers " << "Towers";
    QTest::newRow("group common") << "To" << "To" << "Tops";
    QTest::newRow("group spaces") << "AT W" << "AT W " << "AT W Floor";
    QTest::newRow("group spaces unique") << "AT W F" << "AT W Floor " << "AT W Floor";
    QTest::newRow("group path") << "Movers/F" << "Movers/Front/Spots " << "Movers/Front/Spots";
    QTest::newRow("group comma quoted") << "Front" << "\"Front, Left\" " << "Front, Left";
    QTest::newRow("group after +") << "Towers+Top" << "Towers+Tops " << "Tops";
    QTest::newRow("group after ,") << "Towers, AT W A" << "Towers, AT W ALL " << "AT W ALL";
    // Fixtures.
    QTest::newRow("fixture") << "T1" << "T1-CW " << "T1-CW";
    // Attributes after a target, palettes by attribute.
    QTest::newRow("attribute") << "Towers col" << "Towers colour " << "colour";
    QTest::newRow("attribute after fixture") << "T1-CW sh" << "T1-CW shutter " << "shutter";
    QTest::newRow("fixture or attribute") << "T1-CW dim" << "T1-CW dim" << "Dim 8";
    QTest::newRow("colour") << "Towers colour Bl" << "Towers colour Blu" << "Blush";
    QTest::newRow("colour spaces") << "Towers colour Deep" << "Towers colour Deep Blue " << "Deep Blue";
    QTest::newRow("colour quoted") << "Towers colour \"Deep B" << "Towers colour \"Deep Blue\" " << "Deep Blue";
    QTest::newRow("dimmer") << "Towers dimmer Ba" << "Towers dimmer Base " << "Base";
    QTest::newRow("dimmer only dimmers") << "Towers dimmer B" << "Towers dimmer Base " << "Base";
    QTest::newRow("position") << "Towers position Dr" << "Towers position Drummer " << "Drummer";
    QTest::newRow("zoom") << "Towers zoom Na" << "Towers zoom Narrow " << "Narrow";
    QTest::newRow("zoom or none") << "Towers zoom N" << "Towers zoom N" << "none";
    QTest::newRow("effect") << "Towers effect Si" << "Towers effect Sinelon " << "Sinelon";
    QTest::newRow("none") << "Towers gobo no" << "Towers gobo none " << "none";
    QTest::newRow("after a value") << "Towers colour Blue dim" << "Towers colour Blue dimmer " << "dimmer";
    QTest::newRow("after a level") << "Towers dimmer 45 col" << "Towers dimmer 45 colour " << "colour";
    QTest::newRow("cue look palette") << "cue look colour Deep" << "cue look colour Deep Blue " << "Deep Blue";
    // Functions.
    QTest::newRow("function") << "function Ha" << "function Haze look " << "Haze look";
    QTest::newRow("function second") << "function Haze look, Hou" << "function Haze look, House lights "
                                     << "House lights";
    // Modes.
    QTest::newRow("mode") << "tr" << "track " << "track";
    QTest::newRow("mode after cue") << "cue 1.2 bl" << "cue 1.2 block " << "block";
    QTest::newRow("mode after new cue") << "cue + \"Verse\" cue" << "cue + \"Verse\" cueonly " << "cueonly";
    QTest::newRow("mode after mode") << "block ba" << "block base " << "base";
    // Record choices.
    QTest::newRow("record") << "record ru" << "record running " << "running";
    QTest::newRow("record list") << "record " << "record " << "lit";
    // Release takes targets only.
    QTest::newRow("release") << "release Tow" << "release Towers " << "Towers";
    // Nothing: a name that isn't, after a word that doesn't read.
    QTest::newRow("unknown") << "zzz" << "zzz" << "";
    QTest::newRow("after unknown") << "Nobody col" << "Nobody col" << "";
}

void ShowTree_Test::commandLineCompletesEveryWord()
{
    QFETCH(QString, typed);
    QFETCH(QString, completed);
    QFETCH(QString, offered);
    const ShowTreeCommand::Context ctx = completionShow();
    ShowTreeCommand cmd(m_doc, m_doc->showTree());
    QStringList options;
    QCOMPARE(cmd.complete(typed, ctx, &options), completed);
    if (offered.isEmpty())
        QVERIFY2(options.isEmpty(), qPrintable(options.join(" · ")));
    else
        QVERIFY2(options.contains(offered), qPrintable(options.join(" · ")));
}

/* Tab again on what Tab gave: the next match, round and round; anything
   typed starts afresh. And what Tab completes, the command runs. */
void ShowTree_Test::commandLineTabCyclesAndRuns()
{
    ShowTreeCommand::Context ctx = completionShow();
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    QStringList options;
    int current = -2;
    QString line = cmd.tab("Towers colour Bl", ctx, &options, &current);
    QCOMPARE(line, QString("Towers colour Blu"));
    QCOMPARE(options, QStringList({ "Blue", "Blush" }));
    QCOMPARE(current, -1);
    line = cmd.tab(line, ctx, &options, &current);
    QCOMPARE(line, QString("Towers colour Blue"));
    QCOMPARE(current, 0);
    line = cmd.tab(line, ctx, &options, &current);
    QCOMPARE(line, QString("Towers colour Blush"));
    QCOMPARE(current, 1);
    line = cmd.tab(line, ctx, &options, &current);
    QCOMPARE(line, QString("Towers colour Blue"));
    // Nothing typed yet: the list, then each in turn.
    QCOMPARE(cmd.tab("record ", ctx, &options), QString("record "));
    QCOMPARE(options, QStringList({ "lit", "running", "selected" }));
    QCOMPARE(cmd.tab("record ", ctx), QString("record lit"));
    QCOMPARE(cmd.tab("record lit", ctx), QString("record running"));
    // Typed on: starts afresh.
    QCOMPARE(cmd.tab("Tow", ctx), QString("Towers "));

    // What it completes, runs: a fixture, a group by its folder path, a
    // name with a ',' quoted, functions, an existing list selected.
    ShowTreeCommand::Result r = cmd.run("T1-CW colour Blue", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(ctx.cue)->layers.last().target.fixtures, QList<quint32>({ m_fixtures[6] }));
    r = cmd.run("Movers/Front/Spots dimmer Base", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(ctx.cue)->layers.last().target.groups.count(), 1);
    QCOMPARE(m_doc->fixtureGroup(st->cue(ctx.cue)->layers.last().target.groups.first())->name(),
             QString("Spots"));
    r = cmd.run(cmd.complete("Front", ctx) + "colour Blush", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    r = cmd.run(cmd.complete("Towers colour \"Deep B", ctx), ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    const int before = st->cue(ctx.cue)->layers.count();
    r = cmd.run("function Haze look, House lights", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(ctx.cue)->layers.count(), before + 2);
    QVERIFY(st->cue(ctx.cue)->layers.last().target.isOpaque());
    QVERIFY(!cmd.run("function Nope", ctx).ok);
    const int lists = st->productions().first().lists.count();
    r = cmd.run("list 02-Verse Two", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->productions().first().lists.count(), lists);       // selected, not made
    QCOMPARE(st->cueList(r.context.list)->name, QString("02-Verse Two"));
}

/****************************************************************************
 * P2 answers (2026-10-17): Record which stage, split/merge, reused values
 ****************************************************************************/

/* Record as new cue: the selected cue, the list's running cue, or
   everything lit -- offered only when they differ. */
void ShowTree_Test::recordTheRunningCueOrEverythingLit()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    QVERIFY(st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(full)));
    QVERIFY(st->setLayerLook(c2, st->addLayer(c2, groupTarget(b->id())), st->lookForPalette(half)));
    using RS = ShowTreeController::RecordSource;

    // Nothing runs: the selected cue is the only choice -- nothing to ask.
    QCOMPARE(st->runningCue(listId), invalidId());
    QCOMPARE(st->recordChoices(c2), QList<RS>({ RS::Selected }));
    QVERIFY(!st->canRecord(c2, RS::Running));
    QVERIFY(!st->canRecord(c2, RS::Lit));
    QCOMPARE(st->recordCue(c2, RS::Running), invalidId());

    // The list runs cue One for real; Two is selected. Everything lit is
    // the running cue here: offered once.
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    go(chaser);
    tick(3);
    QCOMPARE(st->runningCue(listId), c1);
    QCOMPARE(st->recordChoices(c2), QList<RS>({ RS::Selected, RS::Running }));
    QVERIFY(st->canRecord(c2, RS::Lit));
    // Selecting the running cue: one choice again.
    QCOMPARE(st->recordChoices(c1), QList<RS>({ RS::Selected }));

    // A scene lit from elsewhere (a VC button): only "everything lit" has it.
    Scene *other = new Scene(m_doc);
    other->setValue(SceneValue(m_fixtures[3], 0, 200));
    QVERIFY(m_doc->addFunction(other));
    other->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    QCOMPARE(st->recordChoices(c2), QList<RS>({ RS::Selected, RS::Running, RS::Lit }));

    // The running cue: One's stage, after the selected cue, one undo step.
    const quint32 run = st->recordCue(c2, RS::Running);
    QVERIFY(run != invalidId());
    const CueList *list = st->cueList(listId);
    QCOMPARE(list->indexOf(run), 2);
    QCOMPARE(list->cue(run)->name, QString("One (recorded)"));
    QCOMPARE(list->cue(run)->mode, CueMode::Block);
    QCOMPARE(list->cue(run)->layers.count(), 1);
    QCOMPARE(list->cue(run)->layers.first().target, groupTarget(a->id()));
    QVERIFY(st->undoText().contains("running"));
    QVERIFY(st->undo());
    QCOMPARE(st->cueList(listId)->cues.count(), 2);

    // Everything lit: the running cue, and the other scene as it is.
    const quint32 lit = st->recordCue(c2, RS::Lit);
    QVERIFY(lit != invalidId());
    list = st->cueList(listId);
    QCOMPARE(list->cue(lit)->layers.count(), 2);
    QCOMPARE(list->cue(lit)->layers.at(0).target, groupTarget(a->id()));
    QCOMPARE(list->cue(lit)->layers.at(1).target.function, other->id());
    QVERIFY(st->undo());

    // In Design the preview is on stage too, on top: Two's B joins.
    st->previewCue(c2);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
    bool available = false;
    const QList<Layer> layers = st->recordLayers(c2, RS::Lit, &available);
    QVERIFY(available);
    QStringList sigs;
    for (const Layer &l : layers)
        sigs << l.target.signature();
    QVERIFY(sigs.contains(groupTarget(a->id()).signature()));
    QVERIFY(sigs.contains(groupTarget(b->id()).signature()));
    st->stopPreview();
    other->stop(FunctionParent::master());
    tick(5);

    // The command line: the chooser decides when they differ; a word says.
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.list = listId;
    ctx.cue = c2;
    QList<RS> offered;
    cmd.setRecordChooser([&offered](quint32, const QList<RS> &choices, RS *chosen) {
        offered = choices;
        *chosen = RS::Running;
        return true;
    });
    ShowTreeCommand::Result r = cmd.run("record", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(offered, QList<RS>({ RS::Selected, RS::Running }));
    QCOMPARE(st->cue(r.context.cue)->name, QString("One (recorded)"));
    QVERIFY(st->undo());
    r = cmd.run("record selected", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(r.context.cue)->name, QString("Two (recorded)"));
    QVERIFY(st->undo());
    cmd.setRecordChooser([](quint32, const QList<RS> &, RS *) { return false; });
    r = cmd.run("record", ctx);
    QVERIFY(!r.ok);                             // cancelled: nothing recorded
    QCOMPARE(st->cueList(listId)->cues.count(), 2);
}

namespace
{
/** The layers of a cue on @a target. */
QList<const Layer *> layersOn(const ShowTreeController *st, quint32 cueId, const Target &target)
{
    QList<const Layer *> out;
    if (const Cue *c = st->cue(cueId))
        for (const Layer &l : c->layers)
            if (l.target == target)
                out << &l;
    return out;
}

Target groupsTarget(const QList<quint32> &groups)
{
    Target t;
    t.groups = groups;
    return t;
}
}

/* Split a layer on two groups: one layer each, tracking as before -- where
   the same layer is set again later it is split there too, and a layer
   that took a tracked one's place carries on from it in every piece. */
void ShowTree_Test::splitALayerKeepsTracking()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    const quint32 blue = colour("Blue");
    const quint32 red = colour("Red");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    const quint32 c4 = st->addCue(listId, 2, "Four");
    const Target ab = groupsTarget({ a->id(), b->id() });
    const quint32 lab = st->addLayer(c1, ab);
    QVERIFY(st->setLayerLook(c1, lab, st->lookForPalette(full)));
    QVERIFY(st->setLayerLook(c1, lab, st->lookForPalette(blue)));
    QVERIFY(st->setTargetLook(c3, ab, st->lookForPalette(half)));      // set again in 3
    const CueList before = *st->cueList(listId);

    // One group: nothing to split.
    QString why;
    const quint32 la = st->addLayer(c2, groupTarget(a->id()));
    QVERIFY(!st->splitLayer(c2, la, nullptr, &why));
    QVERIFY(why.contains("one group"));
    QVERIFY(st->removeLayer(c2, la));

    QList<quint32> made;
    QVERIFY2(st->splitLayer(c1, lab, &made, &why), qPrintable(why));
    QCOMPARE(made.count(), 2);
    QCOMPARE(made.first(), lab);                 // the first piece keeps the layer's id
    QVERIFY(st->undoText().startsWith("Split layer"));
    const CueList *list = st->cueList(listId);
    QVERIFY(!stageDiffers(before, *list));       // nothing on stage changes
    QCOMPARE(st->cue(c1)->layers.count(), 2);
    QCOMPARE(layersOn(st, c1, groupTarget(a->id())).count(), 1);
    QCOMPARE(layersOn(st, c1, groupTarget(b->id())).first()->look(KeyColour)->palettes, QList<quint32>({ blue }));
    // Set again in 3: split there too, each with its level.
    QCOMPARE(st->cue(c3)->layers.count(), 2);
    QCOMPARE(layersOn(st, c3, groupTarget(b->id())).first()->look(KeyDimmer)->palettes, QList<quint32>({ half }));
    QList<ResolvedCue> r = resolve(*list);
    CHECK_VALUE(r[1], a->id(), KeyDimmer, full, Origin::Tracked, c1);
    CHECK_VALUE(r[3], b->id(), KeyDimmer, half, Origin::Tracked, c3);
    CHECK_VALUE(r[3], b->id(), KeyColour, blue, Origin::Tracked, c1);

    // Now each is its own: A red in 2 leaves B blue, there and onwards.
    QVERIFY(st->setTargetLook(c2, groupTarget(a->id()), st->lookForPalette(red)));
    r = resolve(*st->cueList(listId));
    CHECK_VALUE(r[3], a->id(), KeyColour, red, Origin::Tracked, c2);
    CHECK_VALUE(r[3], b->id(), KeyColour, blue, Origin::Tracked, c1);
    // Update cue: B's level in 4 was set in 3, on B's piece.
    quint32 src = invalidId(), srcLayer = invalidId();
    QVERIFY(st->valueSource(c4, groupTarget(b->id()), KeyDimmer, &src, &srcLayer));
    QCOMPARE(src, c3);
    QCOMPARE(srcLayer, layersOn(st, c3, groupTarget(b->id())).first()->id);
    QVERIFY(st->undo());
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    QCOMPARE(st->cue(c3)->layers.count(), 1);

    // Split where it is set again (3): the pieces both carry on from the
    // tracked layer -- each keeps its colour from 1, nothing lights twice.
    const quint32 l3 = st->cue(c3)->layers.first().id;
    QVERIFY2(st->splitLayer(c3, l3, &made, &why), qPrintable(why));
    QVERIFY(!stageDiffers(before, *st->cueList(listId)));
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    r = resolve(*st->cueList(listId));
    CHECK_VALUE(r[2], a->id(), KeyColour, blue, Origin::Tracked, c1);
    CHECK_VALUE(r[2], b->id(), KeyColour, blue, Origin::Tracked, c1);
    CHECK_VALUE(r[2], b->id(), KeyDimmer, half, Origin::Own, c3);
    QVERIFY(r[2].layer(ab) == nullptr);          // the tracked one was taken over, not lit beside
    CHECK_VALUE(r[3], b->id(), KeyDimmer, half, Origin::Tracked, c3);

    // Saved and loaded, the pieces still carry on from it.
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    {
        QXmlStreamWriter w(&buf);
        saveXML(&w, st->productions().first());
    }
    buf.close();
    buf.open(QIODevice::ReadOnly);
    QXmlStreamReader xr(&buf);
    xr.readNextStartElement();
    Production loaded;
    QVERIFY(loadXML(xr, loaded));
    QVERIFY(!stageDiffers(before, loaded.lists.first()));
}

/* Merge layers: one layer on all their groups. Refused when they set an
   attribute differently, or when the merge would change a later cue. */
void ShowTree_Test::mergeLayersKeepsTracking()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    const quint32 blue = colour("Blue");
    const quint32 red = colour("Red");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    st->addCue(listId, 2, "Four");
    const quint32 la = st->addLayer(c1, groupTarget(a->id()));
    const quint32 lb = st->addLayer(c1, groupTarget(b->id()));
    for (quint32 l : { la, lb })
    {
        QVERIFY(st->setLayerLook(c1, l, st->lookForPalette(full)));
        QVERIFY(st->setLayerLook(c1, l, st->lookForPalette(blue)));
    }
    QVERIFY(st->setTargetLook(c3, groupTarget(a->id()), st->lookForPalette(half)));
    QVERIFY(st->setTargetLook(c3, groupTarget(b->id()), st->lookForPalette(half)));
    const CueList before = *st->cueList(listId);
    const Target ab = groupsTarget({ a->id(), b->id() });

    QString why;
    QCOMPARE(st->mergeLayers(c1, { la }, &why), invalidId());
    QVERIFY(why.contains("two or more"));

    const quint32 m = st->mergeLayers(c1, { la, lb }, &why);
    QVERIFY2(m == la, qPrintable(why));
    QVERIFY(st->undoText().startsWith("Merge layers"));
    QVERIFY(!stageDiffers(before, *st->cueList(listId)));
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    QCOMPARE(st->cue(c1)->layers.first().target, ab);
    QCOMPARE(st->cue(c3)->layers.count(), 1);      // set again together in 3: merged there too
    QCOMPARE(st->cue(c3)->layers.first().target, ab);
    QList<ResolvedCue> r = resolve(*st->cueList(listId));
    QCOMPARE(r[3].layer(ab)->values.value(KeyDimmer).look.palettes, QList<quint32>({ half }));
    QCOMPARE(r[3].layer(ab)->values.value(KeyColour).fromCue, c1);
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c1)->layers.count(), 2);

    // They set Colour differently: refused, nothing changed, no undo step.
    QVERIFY(st->setLayerLook(c1, lb, st->lookForPalette(red)));
    const QString undo = st->undoText();
    QCOMPARE(st->mergeLayers(c1, { la, lb }, &why), invalidId());
    QVERIFY2(why.contains("Colour") && why.contains("Blue") && why.contains("Red"), qPrintable(why));
    QCOMPARE(st->undoText(), undo);
    QVERIFY(st->undo());

    // In 3 only A changes: merged, B would light twice there -- refused.
    QVERIFY(st->removeLayer(c3, ownLayer(st, c3, b->id())));
    const CueList partial = *st->cueList(listId);
    QCOMPARE(st->mergeLayers(c1, { la, lb }, &why), invalidId());
    QVERIFY2(why.contains("Three") && why.contains("A"), qPrintable(why));
    QVERIFY(!stageDiffers(partial, *st->cueList(listId)));
    QVERIFY(st->undo());

    // Merged where both took tracked layers' places (3): it carries on from
    // both, also after a save and load.
    const quint32 m3 = st->mergeLayers(c3, { ownLayer(st, c3, a->id()), ownLayer(st, c3, b->id()) }, &why);
    QVERIFY2(m3 != invalidId(), qPrintable(why));
    QCOMPARE(st->cue(c1)->layers.count(), 2);
    const Layer &merged = st->cue(c3)->layers.first();
    QCOMPARE(merged.continues.isEmpty() ? 0 : 1 + int(merged.continuesAlso.count()), 2);
    QVERIFY(!stageDiffers(before, *st->cueList(listId)));
    r = resolve(*st->cueList(listId));
    QCOMPARE(r[2].layers.count(), 1);            // A and B taken over, not lit beside it
    QCOMPARE(r[2].layer(ab)->values.value(KeyColour).fromCue, c1);

    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    {
        QXmlStreamWriter w(&buf);
        saveXML(&w, st->productions().first());
    }
    buf.close();
    QVERIFY(buf.data().contains("<Continues"));
    buf.open(QIODevice::ReadOnly);
    QXmlStreamReader xr(&buf);
    xr.readNextStartElement();
    Production loaded;
    QVERIFY(loadXML(xr, loaded));
    QVERIFY(!stageDiffers(before, loaded.lists.first()));
    Q_UNUSED(c2)
}

/* A value typed (or picked) reuses the palette that plays exactly it: one in
   this cue list first, then a recent one, then any -- so editing that
   palette adjusts every cue that uses it. */
void ShowTree_Test::aTypedValueReusesAPalette()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    group("C", { m_fixtures[2] });
    const quint32 i45 = dimmer(115, "Intensity 45%");
    m_doc->palette(i45)->setPath("Palettes/Dimmer");
    const quint32 also = dimmer(115, "Also 45");
    const quint32 zed = dimmer(115, "Zed 45");
    ShowTreeController *st = m_doc->showTree();
    const QVariantList v45 { 115 };

    QCOMPARE(st->findPalette(QLCPalette::Dimmer, v45), also);            // any: by name
    QCOMPARE(st->findPalette(QLCPalette::Dimmer, v45, invalidId(), { zed }), zed);   // recent first
    QCOMPARE(st->findPalette(QLCPalette::Dimmer, { 114 }), QLCPalette::invalidId());   // exactly
    QCOMPARE(st->findPalette(QLCPalette::Color, v45), QLCPalette::invalidId());

    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    QVERIFY(st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(i45)));
    // In this cue list beats recent.
    QCOMPARE(st->findPalette(QLCPalette::Dimmer, v45, listId, { zed }), i45);
    // A one-off at that level is never reused (it isn't in All).
    const quint32 oneOff = st->createInlinePalette(QLCPalette::Dimmer, v45, "45% · one-off");
    QVERIFY(oneOff != i45);
    QCOMPARE(st->findPalette(QLCPalette::Dimmer, v45, listId), i45);

    // The command line reuses it, in both cues.
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.list = listId;
    ctx.recentPalettes = { zed };
    ctx.cue = c1;
    ShowTreeCommand::Result r = cmd.run("B dimmer 45", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx.cue = c2;
    r = cmd.run("A dimmer 45%", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(c1)->layer(ownLayer(st, c1, b->id()))->look(KeyDimmer)->palettes, QList<quint32>({ i45 }));
    QCOMPARE(st->cue(c2)->layer(ownLayer(st, c2, a->id()))->look(KeyDimmer)->palettes, QList<quint32>({ i45 }));

    // Adjusting the palette adjusts every cue that uses it.
    st->previewCue(c1);
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[1]), 115);
    st->previewCue(c2);
    QCOMPARE(int(settle()[0]), 115);
    st->stopPreview();
    tick(5);
    m_doc->palette(i45)->setValue(200);
    st->previewCue(c1);
    v = settle();
    QCOMPARE(int(v[0]), 200);
    QCOMPARE(int(v[1]), 200);
    st->stopPreview();
    tick(5);
    st->previewCue(c2);
    QCOMPARE(int(settle()[0]), 200);
    st->stopPreview();
    m_doc->palette(i45)->setValue(115);
    QVERIFY(st->removeLayer(c1, ownLayer(st, c1, b->id())));    // only "Intensity 45%" names a level now
    m_doc->deletePalette(also);
    m_doc->deletePalette(zed);
    ctx.recentPalettes.clear();

    // No palette at 30 %: a new named one, as the show names them, filed
    // with them -- and the next 30 reuses it.
    ctx.cue = c1;
    r = cmd.run("C dimmer 30", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    const quint32 i30 = st->cue(c1)->layers.last().look(KeyDimmer)->palettes.first();
    QCOMPARE(m_doc->palette(i30)->name(), QString("Intensity 30%"));
    QCOMPARE(m_doc->palette(i30)->path(), QString("Palettes/Dimmer"));
    QCOMPARE(m_doc->palette(i30)->values().first().toInt(), 77);
    QVERIFY(!st->isInlinePalette(i30));
    const int count = m_doc->palettes().count();
    ctx.cue = c2;
    r = cmd.run("C dimmer 30", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(m_doc->palettes().count(), count);
    QCOMPARE(st->cue(c2)->layers.last().look(KeyDimmer)->palettes, QList<quint32>({ i30 }));
}

/* A new palette is named from its siblings: one whose name holds its own
   value gives the pattern; colours by name (or hex, as siblings are). */
void ShowTree_Test::aNewPaletteIsNamedAsTheShowsAre()
{
    ShowTreeController *st = m_doc->showTree();
    // No siblings: plain names, the type's own folder.
    QCOMPARE(st->suggestPaletteName(QLCPalette::Dimmer, { 77 }), QString("Dimmer 30%"));
    QCOMPARE(st->suggestPaletteFolder(QLCPalette::Dimmer), QString("Palettes/Dimmer"));
    const QVariantList red { QLCPalette::colorToString(QColor("#ff0000"), QColor(Qt::black)) };
    const QVariantList green { QLCPalette::colorToString(QColor("#33aa55"), QColor(Qt::black)) };
    QCOMPARE(st->suggestPaletteName(QLCPalette::Color, red), QString("Red"));
    QVERIFY(st->suggestPaletteName(QLCPalette::Color, green).endsWith(" #33aa55"));

    // DanceHysteria's: "Intensity 45%" (115), "Full", "Candle Intensity".
    const quint32 i45 = dimmer(115, "Intensity 45%");
    m_doc->palette(i45)->setPath("Palettes/Dimmer/");
    m_doc->palette(dimmer(255, "Full"))->setPath("Palettes/Dimmer");
    m_doc->palette(dimmer(242, "Candle Intensity"))->setPath("Palettes/Dimmer");
    QCOMPARE(st->suggestPaletteName(QLCPalette::Dimmer, { 77 }), QString("Intensity 30%"));
    QCOMPARE(st->suggestPaletteFolder(QLCPalette::Dimmer), QString("Palettes/Dimmer"));
    QCOMPARE(st->suggestPaletteName(QLCPalette::Dimmer, { 115 }), QString("Intensity 45% (2)"));   // never taken

    // Colours named by colour: by name; one already called that, numbered.
    QLCPalette *r = new QLCPalette(QLCPalette::Color, m_doc);
    r->setName("Red");
    r->setPath("Palettes/Color");
    r->setValue(QLCPalette::colorToString(QColor("#ff0331"), QColor(Qt::black)));
    m_doc->addPalette(r);
    QCOMPARE(st->suggestPaletteName(QLCPalette::Color, red), QString("Red (2)"));
    QCOMPARE(st->suggestPaletteFolder(QLCPalette::Color), QString("Palettes/Color"));
    // Colours named by hex: by hex, written as they are.
    QLCPalette *h = new QLCPalette(QLCPalette::Color, m_doc);
    h->setName("Col 0000ff");
    h->setValue(QLCPalette::colorToString(QColor("#0000ff"), QColor(Qt::black)));
    m_doc->addPalette(h);
    QCOMPARE(st->suggestPaletteName(QLCPalette::Color, green), QString("Col 33aa55"));

    // Make reusable in one click: a one-off named and filed so.
    const quint32 one = st->createInlinePalette(QLCPalette::Dimmer, { 77 }, "30% · 1 A");
    QVERIFY(st->makeReusable(one));
    QVERIFY(!st->isInlinePalette(one));
    QCOMPARE(m_doc->palette(one)->name(), QString("Intensity 30%"));
    QCOMPARE(m_doc->palette(one)->path(), QString("Palettes/Dimmer"));
    // A colour pick reuses an exact palette.
    QCOMPARE(st->findPalette(QLCPalette::Color,
                             { QLCPalette::colorToString(QColor("#0000ff"), QColor(Qt::black)) }), h->id());
    QCOMPARE(st->findPalette(QLCPalette::Color, { QString("#0000ff") }), h->id());
}

/* One rule for a new list: Tracking, from the controller and the command
   line alike (+ Cue list in the tab is the view test's); an imported chaser
   starts Non-tracking so it plays as it did (D12). */
void ShowTree_Test::aNewListIsTrackingAnImportedOneIsNot()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 made = st->addCueList(prod, "Made");
    QVERIFY(st->cueList(made)->mode == ListMode::Tracking);

    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = prod;
    const ShowTreeCommand::Result r = cmd.run("list \"Typed\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx = r.context;
    QVERIFY(ctx.list != made && st->cueList(ctx.list) != nullptr);
    QVERIFY(st->cueList(ctx.list)->mode == ListMode::Tracking);

    Scene *sc = new Scene(m_doc);
    sc->setName("Look");
    sc->addFixtureGroup(group("Towers", { m_fixtures[0] })->id());
    sc->addPalette(dimmer(200, "Full"));
    m_doc->addFunction(sc);
    Collection *one = new Collection(m_doc);
    one->setName("0.0.1-One");
    one->addFunction(sc->id());
    m_doc->addFunction(one);
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Old");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(one->id()));
    const ShowTreeController::ImportReport rep = st->importChaser(chaser->id());
    QVERIFY(st->cueList(rep.list) != nullptr);
    QVERIFY(st->cueList(rep.list)->mode == ListMode::NonTracking);
}

/* Make reusable is an undo step (D22): undo makes the palette the one-off it
   was (name, folder, hidden), redo names and files it again. Several at once
   are one step. */
void ShowTree_Test::makeReusableIsOneUndoStep()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0], m_fixtures[1] });
    const quint32 named = dimmer(115, "Intensity 45%");
    m_doc->palette(named)->setPath("Palettes/Dimmer");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 l1 = st->addLayer(c1, groupTarget(towers->id()));
    const quint32 pid = st->createInlinePalette(QLCPalette::Dimmer, { 79 }, "31% · 1 One Towers");
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(pid)));
    QVERIFY(st->isInlinePalette(pid));
    const QString oneOffName = m_doc->palette(pid)->name();
    const QString oneOffPath = m_doc->palette(pid)->path();

    QVERIFY(st->makeReusable(pid));
    QVERIFY(!st->isInlinePalette(pid));
    QCOMPARE(m_doc->palette(pid)->name(), QString("Intensity 31%"));
    QCOMPARE(m_doc->palette(pid)->path(), QString("Palettes/Dimmer"));
    QVERIFY(st->undoText().startsWith("Make reusable"));

    QVERIFY(st->undo());
    QVERIFY(st->isInlinePalette(pid));
    QCOMPARE(m_doc->palette(pid)->name(), oneOffName);
    QCOMPARE(m_doc->palette(pid)->path(), oneOffPath);
    QCOMPARE(st->cue(c1)->layer(l1)->look(KeyDimmer)->palettes, QList<quint32>({ pid }));

    QVERIFY(st->redo());
    QVERIFY(!st->isInlinePalette(pid));
    QCOMPARE(m_doc->palette(pid)->name(), QString("Intensity 31%"));
    QCOMPARE(m_doc->palette(pid)->path(), QString("Palettes/Dimmer"));

    // Named by hand (the Library's Make reusable…): the same.
    QVERIFY(st->undo());
    QVERIFY(st->makeReusable(pid, "Mine", "Palettes/Mine"));
    QCOMPARE(m_doc->palette(pid)->name(), QString("Mine"));
    QVERIFY(st->undo());
    QVERIFY(st->isInlinePalette(pid));
    QCOMPARE(m_doc->palette(pid)->path(), oneOffPath);

    // Two one-offs (colour 1 and 2) at once: one step.
    const quint32 a = st->createInlinePalette(QLCPalette::Color, { "#ff0000" }, "red one-off");
    const quint32 b = st->createInlinePalette(QLCPalette::Color, { "#00ff00" }, "green one-off");
    Look two;
    two.key = KeyColour;
    two.palettes = { a, b };
    QVERIFY(st->setLayerLook(c1, l1, two));
    const QString before = st->undoText();
    QVERIFY(st->makeReusable(QList<quint32>({ a, b })));
    QVERIFY(!st->isInlinePalette(a) && !st->isInlinePalette(b));
    QVERIFY(st->undo());
    QVERIFY(st->isInlinePalette(a) && st->isInlinePalette(b));
    QCOMPARE(st->undoText(), before);
}

/* Detach (D13) is one undo step: undo puts the layer back and deletes the
   plain scene it made; redo brings that scene back as it was, edits too. */
void ShowTree_Test::detachIsOneUndoStep()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0], m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 l1 = st->addLayer(c1, groupTarget(towers->id()));
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(full)));
    const int functions = m_doc->functions().count();

    const quint32 sid = st->detachLayer(c1, l1);
    QVERIFY(sid != Function::invalidId());
    QVERIFY(!st->isGenerated(sid));
    QCOMPARE(st->cue(c1)->layer(l1)->target.function, sid);
    m_doc->function(sid)->setName("Edited by hand");

    QVERIFY(st->undo());
    QVERIFY(m_doc->function(sid) == nullptr);
    QCOMPARE(st->cue(c1)->layer(l1)->target.groups, QList<quint32>({ towers->id() }));
    QCOMPARE(m_doc->functions().count(), functions);

    QVERIFY(st->redo());
    QVERIFY(m_doc->function(sid) != nullptr);
    QCOMPARE(m_doc->function(sid)->name(), QString("Edited by hand"));
    QVERIFY(m_doc->function(sid)->isVisible());
    QCOMPARE(st->cue(c1)->layer(l1)->target.function, sid);
}

/* What the tree made says where it is (D13): the list for its chaser, the
   cue for its collection, the cue and own layer for its scene. */
void ShowTree_Test::generatedFunctionsAreFoundInTheTree()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0], m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 l1 = st->addLayer(c1, groupTarget(towers->id()));
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(full)));

    QVERIFY(ShowTreeController::lockedText().contains("Show Tree"));
    quint32 l = 0, c = 0, layer = 0;
    const quint32 chaser = st->cueList(list)->chaser;
    QVERIFY(st->generatedNode(chaser, &l, &c, &layer));
    QCOMPARE(l, list);
    QCOMPARE(c, invalidId());
    const quint32 coll = st->cue(c2)->collection;
    QVERIFY(st->generatedNode(coll, &l, &c, &layer));
    QCOMPARE(c, c2);
    QCOMPARE(layer, invalidId());
    // The scene c1 sets and c2 plays tracked: found where it is c1's own.
    const Collection *cc = qobject_cast<const Collection *>(m_doc->function(coll));
    QVERIFY(cc != nullptr && cc->functions().count() == 1);
    const quint32 sid = cc->functions().first();
    QVERIFY(st->isGenerated(sid));
    QVERIFY(st->generatedNode(sid, &l, &c, &layer));
    QCOMPARE(c, c1);
    QCOMPARE(layer, l1);
    // A function of the show's own is not the tree's.
    Scene *plain = new Scene(m_doc);
    m_doc->addFunction(plain);
    QVERIFY(!st->generatedNode(plain->id(), &l, &c, &layer));
}

/****************************************************************************
 * P3: a look that leaves fixtures out, and lays out over other scenes
 ****************************************************************************/

/* Scene::excludedFixtures: a parent's level must not show through where a
   lower layer sets it (HTP would keep the higher), and the exclusion is part
   of the scene -- saved, loaded and copied. */
void ShowTree_Test::sceneExcludedFixturesAreNotWritten()
{
    FixtureGroup *steps = group("Steps", { m_fixtures[0], m_fixtures[1], m_fixtures[2] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");

    Scene *parent = new Scene(m_doc);
    parent->addFixtureGroup(steps->id());
    parent->addPalette(full);
    parent->setExcludedFixtures(full, { m_fixtures[1] });
    m_doc->addFunction(parent);
    Scene *child = new Scene(m_doc);
    child->addFixture(m_fixtures[1]);
    child->addPalette(half);
    m_doc->addFunction(child);

    QCOMPARE(parent->excludedFixtures(full), QList<quint32>({ m_fixtures[1] }));
    QCOMPARE(parent->writtenFixtures(full), QList<quint32>({ m_fixtures[0], m_fixtures[2] }));
    QCOMPARE(parent->layoutFixtures(full), QList<quint32>({ m_fixtures[0], m_fixtures[2] }));
    QCOMPARE(parent->writtenFixtures(half), QList<quint32>({ m_fixtures[0], m_fixtures[1], m_fixtures[2] }));

    parent->start(m_doc->masterTimer(), FunctionParent::master());
    child->start(m_doc->masterTimer(), FunctionParent::master());
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[1]), 127);       // the lower layer's level, not HTP's 255
    QCOMPARE(int(v[2]), 255);

    // Saved and read back; copied.
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter w(&buffer);
    QVERIFY(parent->saveXML(&w));
    buffer.close();
    QVERIFY(buffer.data().contains("<Exclude"));
    buffer.open(QIODevice::ReadOnly);
    QXmlStreamReader r(&buffer);
    r.readNextStartElement();
    Scene loaded(m_doc);
    QVERIFY(loaded.loadXML(r));
    QCOMPARE(loaded.excludedFixtures(), parent->excludedFixtures());
    Scene copy(m_doc);
    QVERIFY(copy.copyFrom(parent));
    QCOMPARE(copy.excludedFixtures(), parent->excludedFixtures());

    // Spans: laid out over more than it writes.
    Scene::LayoutSpan span;
    span.groups << steps->id();
    span.fixtures << m_fixtures[5];
    child->setLayoutSpan(half, span);
    QCOMPARE(child->layoutFixtures(half),
             QList<quint32>({ m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[5] }));
    QCOMPARE(child->writtenFixtures(half), QList<quint32>({ m_fixtures[1] }));
    Scene copy2(m_doc);
    QVERIFY(copy2.copyFrom(child));
    QVERIFY(copy2.layoutSpan(half) == span);

    // Cleared: back to every fixture (a new fader map, as a re-take).
    parent->setExcludedFixtures(full, {});
    parent->resetRuntime();
    v = settle();
    QCOMPARE(int(v[1]), 255);
}

/* The DMX thread builds a scene's fader map while the GUI thread changes
   what it leaves out, re-takes it (resetRuntime, as the Design preview
   does on a bake) and moves it on top (setEditOverride): no race, and it
   ends up writing what was asked last. */
void ShowTree_Test::exclusionIsThreadSafeWithLivePreview()
{
    FixtureGroup *steps = group("Steps", { m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[3] });
    const quint32 full = dimmer(255, "Full");
    Scene *s = new Scene(m_doc);
    s->addFixtureGroup(steps->id());
    s->addPalette(full);
    m_doc->addFunction(s);
    s->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);

    std::atomic<bool> stop(false);
    std::atomic<int> ticks(0);
    std::thread dmx([&]() {
        while (!stop.load())
        {
            m_doc->masterTimer()->timerTick();
            QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
            ua.first()->processFaders();
            m_doc->inputOutputMap()->releaseUniverses(false);
            ++ticks;
        }
    });
    for (int i = 0; i < 3000; ++i)
    {
        QList<quint32> out;
        for (int k = 0; k < 4; ++k)
            if ((i >> k) & 1)
                out << m_fixtures[k];
        s->setExcludedFixtures(full, out);
        if (i % 3 == 0)
            s->resetRuntime();
        if (i % 7 == 0)
            s->requestPaletteRefresh();
        if (i % 11 == 0)
            s->setEditOverride(i % 22 == 0);
        (void)s->layoutFixtures(full);
        (void)s->writtenFixtures(full);
    }
    // Let the DMX thread see the last change before it stops.
    while (ticks.load() < 20)
        QThread::yieldCurrentThread();
    stop.store(true);
    dmx.join();

    s->setEditOverride(false);
    s->setExcludedFixtures(full, { m_fixtures[2] });
    s->resetRuntime();
    const QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[1]), 255);
    QCOMPARE(int(v[2]), 0);
    QCOMPARE(int(v[3]), 255);
}

QTEST_MAIN(ShowTree_Test)

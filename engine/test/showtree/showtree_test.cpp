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

#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private
#include "showtreecontroller.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "showtreemodel.h"
#include "qlcpalette.h"
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
    // Cue 1 includes 2 (which can't include 1 back while 1 is resolving).
    CHECK_VALUE(r[0], USTs, KeyColour, Blue, Origin::Included, 2);
    QCOMPARE(r[1].skippedIncludes, QList<quint32>({ 1 }));
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
    QVERIFY(bc.layers.at(2).target.isOpaque());
    QCOMPARE(bc.layers.at(2).target.function, quint32(42));
    QCOMPARE(bc.looks.first().palettes, QList<quint32>({ Blue, Green }));
    QCOMPARE(bc.includes, QList<quint32>({ 9 }));
    QCOMPARE(back.lists.first().baseCue, quint32(3));
    QCOMPARE(back.lists.first().chaser, quint32(77));
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
    const quint32 listId = st->addCueList(prod, "Full Show");
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
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
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
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
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

void ShowTree_Test::theListRunningForRealWins()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    FixtureGroup *b = group("USTs", { m_fixtures[2] });
    const quint32 d50 = dimmer(127, "Half");
    const quint32 d100 = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(d50));
    st->setLayerLook(c2, st->addLayer(c2, groupTarget(b->id())), st->lookForPalette(d100));
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));

    // Running from the footer: selecting cue Two previews nothing.
    go(chaser);
    tick(5);
    st->previewCue(c2);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::ListRunning));
    QVERIFY(st->previewFunctions().isEmpty());
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(int(v[2]), 0);
    // Stopping the (empty) preview leaves the show alone.
    st->stopPreview();
    tick(5);
    QVERIFY(chaser->isRunning());

    // The list stops: a wanted preview comes back.
    st->previewCue(c2);
    chaser->stop(FunctionParent::master());
    tick(5);
    QCoreApplication::processEvents();
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
    v = settle();
    QCOMPARE(int(v[0]), 0);
    QCOMPARE(int(v[2]), 255);

    // It starts while previewing: the preview gives the stage to it.
    go(chaser);
    tick(2);
    QCoreApplication::processEvents();
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::ListRunning));
    v = settle();
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(int(v[2]), 0);
    // GO again: the show moves on, nothing of the preview hangs on.
    go(chaser);
    v = settle();
    QCOMPARE(int(v[0]), 0);
    QCOMPARE(int(v[2]), 255);
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

QTEST_MAIN(ShowTree_Test)

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
#include "showcontrol.h"
#include "paletteops.h"
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
#include "chaserrunner.h"
#include "resource_paths.h"
#include "qlcfile.h"
#include "fixture.h"
#include "chaser.h"
#include "showfunction.h"
#include "track.h"
#include "show.h"
#include "scene.h"
#include "doc.h"

#include "monitorproperties.h"
#include "effectinstance.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "stagetarget.h"
#include "aimsolver.h"
#include "showembedding.h"
#include <QTemporaryDir>

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

static QByteArray productionXml(const Production &p);
static QByteArray engineXml(Doc *doc);

/* The rule a dragged cue (and + Cue) is numbered by: D23 between its
   neighbours. */
void ShowTree_Test::numberBetweenRule()
{
    QCOMPARE(numberBetween("2", "3"), QString("2.5"));
    QCOMPARE(numberBetween("2", "2.5"), QString("2.1"));
    QCOMPARE(numberBetween("2", "2.1"), QString("2.0.5"));      // a level down
    QCOMPARE(numberBetween("2.5", "3"), QString("2.6"));
    QCOMPARE(numberBetween("2", "5"), QString("3"));
    QCOMPARE(numberBetween("1.0.2", "1.0.3"), QString("1.0.2.5"));
    QCOMPARE(numberBetween("2", "3", { "2.5" }), QString("2.6"));
    // The end: the last place + 1.
    QCOMPARE(numberBetween("4", ""), QString("5"));
    QCOMPARE(numberBetween("4.5", ""), QString("4.6"));
    QCOMPARE(numberBetween("0.0.9", ""), QString("0.0.10"));
    // The top.
    QCOMPARE(numberBetween("", "5"), QString("4"));
    QCOMPARE(numberBetween("", "0.0.3"), QString("0.0.2"));
    QCOMPARE(numberBetween("", "2.1"), QString("2"));
    QCOMPARE(numberBetween("", "1"), QString("0.5"));
    QCOMPARE(numberBetween("", "0.0.1"), QString("0.0.0.5"));
    QCOMPARE(numberBetween("", "1.0.0"), QString("0.0.1"));
    QCOMPARE(numberBetween("", "0"), QString());
    // No room three levels below prev.
    QCOMPARE(numberBetween("2", "2.0.0.1"), QString());
    QVERIFY(isCueNumber("0.0.4"));
    QVERIFY(isCueNumber("12"));
    QVERIFY(!isCueNumber("2."));
    QVERIFY(!isCueNumber("2,5"));
    QVERIFY(!isCueNumber(""));
}

static QStringList numbersIn(ShowTreeController *st, quint32 listId)
{
    QStringList out;
    const CueList *l = st->cueList(listId);
    for (int i = 0; i < l->cues.count(); ++i)
        out << st->cueNumber(*l, i);
    return out;
}

static QList<quint32> idsIn(ShowTreeController *st, quint32 listId)
{
    QList<quint32> out;
    for (const Cue &c : st->cueList(listId)->cues)
        out << c.id;
    return out;
}

void ShowTree_Test::aRenumberedCueMovesToItsPlace()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "Main", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    const quint32 c4 = st->addCue(listId, 2, "Four");
    QCOMPARE(numbersIn(st, listId), QStringList({ "1", "2", "3", "4" }));

    // 1 -> 3.5: it moves between 3 and 4, and the chaser follows.
    Cue one = *st->cue(c1);
    one.number = "3.5";
    QVERIFY(st->setCue(one));
    QCOMPARE(numbersIn(st, listId), QStringList({ "2", "3", "3.5", "4" }));
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c2, c3, c1, c4 }));
    const Chaser *chaser = qobject_cast<const Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QCOMPARE(chaser->stepCue(2), QString("3.5"));
    QCOMPARE(chaser->steps().at(2).fid, st->cue(c1)->collection);
    QCOMPARE(m_doc->function(st->cue(c1)->collection)->name(), QString("3.5-One"));

    // A number taken is refused, and said; nothing changes.
    one = *st->cue(c1);
    one.number = "4";
    QString why;
    QVERIFY(!st->setCue(one, &why));
    QCOMPARE(why, QString("4 is taken: cue 4 Four has it."));
    QCOMPARE(numbersIn(st, listId), QStringList({ "2", "3", "3.5", "4" }));
    QVERIFY(!st->checkCueNumber(c1, "2.x", &why));
    QVERIFY(why.contains("isn't a cue number"));
    QVERIFY(!st->checkCueNumber(c1, "", &why));
    QVERIFY(st->checkCueNumber(c1, "3.5"));                 // its own

    // A sub-cue's derived number is taken too: 4.1 is sub-cue 1 of cue 4.
    const quint32 sub = st->addSubCue(c4, "Hit");
    QVERIFY(sub != ShowTree::invalidId());
    QVERIFY(!st->checkCueNumber(c1, "4.1", &why));
    QVERIFY(why.contains("sub-cue of cue 4 Four"));
    // ... and its own sub-cue (4.1) can't land on a cue's number: as 3,
    // its sub-cue would be 3.1 -- with a cue 3.1 there, refused.
    Cue three = *st->cue(c3);
    three.number = "3.1";
    QVERIFY(st->setCue(three));
    QVERIFY(!st->checkCueNumber(c4, "3", &why));
    QCOMPARE(why, QString("Its sub-cue would be 3.1, cue 3.1 Three's number."));
    QVERIFY(st->undo());

    // One undo step each way.
    QVERIFY(st->undo());                                    // the sub-cue
    QVERIFY(st->undo());
    QCOMPARE(numbersIn(st, listId), QStringList({ "1", "2", "3", "4" }));
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c1, c2, c3, c4 }));
    QVERIFY(st->redo());
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c2, c3, c1, c4 }));
    QVERIFY(st->undo());
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c1, c2, c3, c4 }));

    // Renumbering only the name still is one plain edit, in place.
    Cue two = *st->cue(c2);
    two.name = "Deux";
    QVERIFY(st->setCue(two));
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c1, c2, c3, c4 }));

    // Includes and the base go by id: they follow a renumber.
    QVERIFY(st->addInclude(c4, c1));
    QVERIFY(st->setBaseCue(listId, c1));
    one = *st->cue(c1);
    one.number = "3.5";
    QVERIFY(st->setCue(one));
    QCOMPARE(st->cue(c4)->includes, QList<quint32>({ c1 }));
    QCOMPARE(st->cueList(listId)->baseCue, c1);
}

void ShowTree_Test::aDraggedCueIsNumberedBetweenItsNeighbours()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "Main", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    const quint32 c4 = st->addCue(listId, 2, "Four");
    const quint32 c5 = st->addCue(listId, 3, "Five");

    // Four between 2 and 3: 2.5. Nothing else renumbered.
    QCOMPARE(st->moveNumbers({ c4 }, c2, true), QStringList({ "2.5" }));
    QVERIFY(st->moveCues({ c4 }, c2, true));
    QCOMPARE(numbersIn(st, listId), QStringList({ "1", "2", "2.5", "3", "5" }));
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c1, c2, c4, c3, c5 }));
    QCOMPARE(st->undoText(), QString("Move cue"));

    // Five between 2 and 2.5: 2.1.
    QVERIFY(st->moveCues({ c5 }, c4, false));
    QCOMPARE(numbersIn(st, listId), QStringList({ "1", "2", "2.1", "2.5", "3" }));

    // To the end: the last + 1. To the top: one less than the first.
    QVERIFY(st->moveCues({ c1 }, c3, true));
    QCOMPARE(numbersIn(st, listId), QStringList({ "2", "2.1", "2.5", "3", "4" }));
    QCOMPARE(st->cue(c1)->number, QString("4"));
    QVERIFY(st->moveCues({ c3 }, c2, false));
    QCOMPARE(st->cue(c3)->number, QString("1"));
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c3, c2, c5, c4, c1 }));
    // Above 1: 0.5.
    QVERIFY(st->moveCues({ c2 }, c3, false));
    QCOMPARE(st->cue(c2)->number, QString("0.5"));

    // Undo / redo: one step each.
    QVERIFY(st->undo());
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c3, c2, c5, c4, c1 }));
    QCOMPARE(st->cue(c2)->number, QString("2"));
    QVERIFY(st->redo());
    QCOMPARE(st->cue(c2)->number, QString("0.5"));
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c2, c3, c5, c4, c1 }));

    // Dropped where it is: nothing to do.
    QString why;
    QVERIFY(!st->moveCues({ c2 }, c3, false, &why));
    QVERIFY(why.isEmpty());

    // A block: in their order, each in turn numbered after the one before
    // (0.5, 2.5 and 4 dropped after 1, before 2.1: 2, then 2.0.5, 2.0.6 --
    // 2.5 is past 2.1, so a level down).
    QCOMPARE(numbersIn(st, listId), QStringList({ "0.5", "1", "2.1", "2.5", "4" }));
    QCOMPARE(st->moveNumbers({ c1, c2, c4 }, c3, true), QStringList({ "2", "2.0.5", "2.0.6" }));
    QVERIFY(st->moveCues({ c1, c2, c4 }, c3, true));
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c3, c2, c4, c1, c5 }));
    QCOMPARE(numbersIn(st, listId), QStringList({ "1", "2", "2.0.5", "2.0.6", "2.1" }));
    QCOMPARE(st->undoText(), QString("Move cues"));
    QVERIFY(st->undo());
    QCOMPARE(numbersIn(st, listId), QStringList({ "0.5", "1", "2.1", "2.5", "4" }));
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c2, c3, c5, c4, c1 }));
    QVERIFY(st->redo());
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c3, c2, c4, c1, c5 }));

    // No room: nothing fits between 2.0.6 and 2.0.6.0.0.1 three levels
    // down; refused, with Renumber this level offered.
    Cue five = *st->cue(c5);
    five.number = "2.0.6.0.0.1";
    QVERIFY(st->setCue(five));
    QCOMPARE(numbersIn(st, listId), QStringList({ "1", "2", "2.0.5", "2.0.6", "2.0.6.0.0.1" }));
    QVERIFY(st->moveNumbers({ c3 }, c1, true, &why).isEmpty());
    QVERIFY(why.contains("Renumber this level"));
    QVERIFY(!st->moveCues({ c3 }, c1, true, &why));
    QCOMPARE(numbersIn(st, listId), QStringList({ "1", "2", "2.0.5", "2.0.6", "2.0.6.0.0.1" }));

    // moveCue (Move up / down) renumbers the same way.
    QVERIFY(st->moveCue(c2, 0));
    QCOMPARE(st->cue(c2)->number, QString("0.5"));

    // Never onto a sub-cue's number: with cue 1's sub-cues 1.1 to 1.5, a
    // cue dropped between 1 and 1.6 skips them all.
    const quint32 other = st->addCueList(st->productions().first().id, "Other", ListMode::Tracking);
    const quint32 o1 = st->addCue(other, -1, "One");
    const quint32 o2 = st->addCue(other, 0, "Two");
    const quint32 o3 = st->addCue(other, 1, "Three");
    for (int i = 0; i < 5; ++i)
        QVERIFY(st->addSubCue(o1, "Hit") != ShowTree::invalidId());
    Cue two = *st->cue(o2);
    two.number = "1.6";
    QVERIFY(st->setCue(two));
    QCOMPARE(numbersIn(st, other), QStringList({ "1", "1.6", "3" }));
    QCOMPARE(st->moveNumbers({ o3 }, o1, true), QStringList({ "1.0.5" }));
}

void ShowTree_Test::renumberThisLevel()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "01-Song", ListMode::Tracking);
    // 1.0.0, 1.0.1, 1.0.1.5, 1.0.4 and a sub-cue under 1.0.4.
    const quint32 a = st->addCue(listId, -1, "A");
    const quint32 b = st->addCue(listId, 0, "B");
    const quint32 p = st->addCue(listId, 1, "Point");
    const quint32 d = st->addCue(listId, 2, "D");
    Cue c = *st->cue(p);
    c.number = "1.0.1.5";
    QVERIFY(st->setCue(c));
    c = *st->cue(d);
    c.number = "1.0.4";
    QVERIFY(st->setCue(c));
    const quint32 sub = st->addSubCue(d, "Hit");
    QCOMPARE(numbersIn(st, listId), QStringList({ "1.0.0", "1.0.1", "1.0.1.5", "1.0.4" }));

    // From 1, step 1: the level 1, 2, 3; the point cue keeps its .5 under
    // its cue's new number; the sub-cue follows its cue.
    QString why;
    const QList<ShowTreeController::Renumbered> plan = st->renumberLevelPlan(b, 1, 1, &why);
    QCOMPARE(plan.count(), 5);
    QCOMPARE(plan.at(2).from, QString("1.0.1.5"));
    QCOMPARE(plan.at(2).to, QString("1.0.2.5"));
    QCOMPARE(plan.at(0).from, QString("1.0.0"));
    QCOMPARE(plan.at(0).to, QString("1.0.1"));
    QCOMPARE(plan.last().from, QString("1.0.4.1"));
    QCOMPARE(plan.last().to, QString("1.0.3.1"));
    QCOMPARE(plan.last().cue, sub);
    QVERIFY(st->renumberLevel(b, 1, 1));
    QCOMPARE(numbersIn(st, listId), QStringList({ "1.0.1", "1.0.2", "1.0.2.5", "1.0.3" }));
    QCOMPARE(st->cueNumber(*st->cueList(st->subCueList(d)), 0), QString("1.0.3.1"));
    const Chaser *chaser = qobject_cast<const Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QCOMPARE(chaser->stepCue(3), QString("1.0.3"));
    QCOMPARE(st->undoText(), QString("Renumber this level"));

    // From 10 in tens.
    QVERIFY(st->renumberLevel(a, 10, 10));
    QCOMPARE(numbersIn(st, listId), QStringList({ "1.0.10", "1.0.20", "1.0.20.5", "1.0.30" }));
    // Again the same: nothing to do, said.
    QVERIFY(!st->renumberLevel(a, 10, 10, &why));
    QCOMPARE(why, QString("This level is numbered so already."));
    QVERIFY(!st->renumberLevel(a, 1, 0, &why));

    // Undo: one step each.
    QVERIFY(st->undo());
    QCOMPARE(numbersIn(st, listId), QStringList({ "1.0.1", "1.0.2", "1.0.2.5", "1.0.3" }));
    QVERIFY(st->undo());
    QCOMPARE(numbersIn(st, listId), QStringList({ "1.0.0", "1.0.1", "1.0.1.5", "1.0.4" }));
    QVERIFY(st->redo());
    QCOMPARE(numbersIn(st, listId), QStringList({ "1.0.1", "1.0.2", "1.0.2.5", "1.0.3" }));

    // Only that level: the point cue's own level is just itself.
    QVERIFY(st->renumberLevel(p, 7, 1));
    QCOMPARE(numbersIn(st, listId), QStringList({ "1.0.1", "1.0.2", "1.0.2.7", "1.0.3" }));

    // Show control fires the new numbers.
    ShowControl::Request go;
    go.command = ShowControl::Load;
    go.cue = "1.0.3";
    go.chaser = chaser->id();
    QVERIFY(m_doc->showControl()->execute(go));
    QCOMPARE(m_doc->showControl()->loadedStep(chaser->id()), 3);
    go.cue = "1.0.4";
    QVERIFY(!m_doc->showControl()->execute(go));
}

void ShowTree_Test::aMoveKeepsShowControlsStandby()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "Main", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    const quint32 chaser = st->cueList(listId)->chaser;
    ShowControl::Request load;
    load.command = ShowControl::Load;
    load.cue = "3";
    load.chaser = chaser;
    QVERIFY(m_doc->showControl()->execute(load));
    QCOMPARE(m_doc->showControl()->loadedStep(chaser), 2);
    // Three to the top: still the cue on standby, at its new step.
    QVERIFY(st->moveCues({ c3 }, c1, false));
    QCOMPARE(m_doc->showControl()->loadedStep(chaser), 0);
    // One renumbered past Two: Three stays where it is.
    Cue one = *st->cue(c1);
    one.number = "2.5";
    QVERIFY(st->setCue(one));
    QCOMPARE(idsIn(st, listId), QList<quint32>({ c3, c2, c1 }));
    QCOMPARE(m_doc->showControl()->loadedStep(chaser), 0);
}

/* Point cues are the Number attribute as written: they come back as they
   went, and an older show (numbers out of order, none at all) loads as it
   was -- nothing is re-sorted or renumbered on load. */
void ShowTree_Test::pointCuesRoundTripInXml()
{
    Production p;
    p.id = 1;
    p.name = "P";
    CueList list;
    list.id = 2;
    list.name = "L";
    for (const QString &n : { QString("1"), QString("1.5"), QString("1.0.5"), QString("2.25"), QString("0.0.4.5") })
    {
        Cue c;
        c.id = list.cues.count() + 10;
        c.number = n;
        list.cues << c;
    }
    Cue none;
    none.id = 20;
    list.cues << none;
    p.lists << list;
    const QByteArray xml = productionXml(p);
    QVERIFY(xml.contains("Number=\"2.25\""));
    QVERIFY(xml.contains("Number=\"0.0.4.5\""));
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    Production back;
    QVERIFY(ShowTree::loadXML(r, back));
    QCOMPARE(productionXml(back), xml);
    QStringList numbers;
    for (const Cue &c : back.lists.first().cues)
        numbers << c.number;
    QCOMPARE(numbers, QStringList({ "1", "1.5", "1.0.5", "2.25", "0.0.4.5", "" }));

    // Through a whole show: the same.
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "Main", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    QVERIFY(st->moveCues({ c1 }, c2, true));
    QVERIFY(st->addCue(listId, 0, "Point") != ShowTree::invalidId());
    QCOMPARE(numbersIn(st, listId), QStringList({ "2", "2.5", "3" }));
    const QByteArray show = engineXml(m_doc);
    Doc *other = new Doc(this);
    QXmlStreamReader sr(show);
    sr.readNextStartElement();
    QVERIFY(other->loadXML(sr, false));
    QCOMPARE(numbersIn(other->showTree(), listId), QStringList({ "2", "2.5", "3" }));
    QCOMPARE(productionXml(other->showTree()->productions().first()),
             productionXml(st->productions().first()));
    delete other;
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

void ShowTree_Test::removingAFixtureFromALayerIsOneStepAndKeepsTracking()
{
    const quint32 d50 = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");          // tracks One's layer
    Target two;
    two.fixtures << m_fixtures[0] << m_fixtures[1];
    const quint32 l1 = st->addLayer(c1, two);
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(d50)));
    QList<ResolvedCue> r = resolve(*st->cueList(listId));
    QVERIFY(r[1].layer(two) != nullptr);
    QCOMPARE(int(r[1].layer(two)->origin), int(Origin::Tracked));
    st->previewCue(c2);
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(int(v[1]), 127);

    const QString before = st->undoText();
    QVERIFY(st->removeFixtureFromLayer(c1, l1, m_fixtures[1]));
    QCOMPARE(st->undoText(), QString("Remove Dim 2 from layer"));
    QCOMPARE(st->cue(c1)->layer(l1)->target.fixtures, QList<quint32>({ m_fixtures[0] }));

    // Two still tracks the layer, now without it -- and plays that way.
    Target one;
    one.fixtures << m_fixtures[0];
    r = resolve(*st->cueList(listId));
    QCOMPARE(r[1].layers.count(), 1);
    const ResolvedLayer *tracked = r[1].layer(one);
    QVERIFY(tracked != nullptr);
    QCOMPARE(int(tracked->origin), int(Origin::Tracked));
    QCOMPARE(tracked->values.value(KeyDimmer).look.palettes.value(0), d50);
    v = settle();
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(int(v[1]), 0);

    // Not in the layer, or its last fixture: refused, and nothing recorded.
    QVERIFY(!st->removeFixtureFromLayer(c1, l1, m_fixtures[1]));
    QVERIFY(!st->removeFixtureFromLayer(c1, l1, m_fixtures[0]));
    QCOMPARE(st->cue(c1)->layer(l1)->target.fixtures, QList<quint32>({ m_fixtures[0] }));
    QCOMPARE(st->undoText(), QString("Remove Dim 2 from layer"));

    // One undo step puts it back, in both cues.
    QVERIFY(st->undo());
    QCOMPARE(st->undoText(), before);
    QCOMPARE(st->cue(c1)->layer(l1)->target.fixtures, QList<quint32>({ m_fixtures[0], m_fixtures[1] }));
    QVERIFY(resolve(*st->cueList(listId))[1].layer(two) != nullptr);
    v = settle();
    QCOMPARE(int(v[1]), 127);
    QVERIFY(st->redo());
    QCOMPARE(st->cue(c1)->layer(l1)->target.fixtures, QList<quint32>({ m_fixtures[0] }));
    st->stopPreview();
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

void ShowTree_Test::undoKeepsTheLastTwoHundredSteps()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    for (int i = 0; i < 205; ++i)
        QVERIFY(st->renameNode(c1, QString("R%1").arg(i)));
    QCOMPARE(st->cue(c1)->name, QString("R204"));

    int undone = 0;
    while (st->undo())
        ++undone;
    QCOMPARE(undone, 200);
    QVERIFY(!st->canUndo());
    QVERIFY(!st->undo());
    // The 200 kept are the newest: back to just after the fifth rename; the
    // list and cue (and the first renames) are past undoing.
    QCOMPARE(st->cue(c1)->name, QString("R4"));
    QVERIFY(st->cueList(listId) != nullptr);
    int redone = 0;
    while (st->redo())
        ++redone;
    QCOMPARE(redone, 200);
    QCOMPARE(st->cue(c1)->name, QString("R204"));
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

void ShowTree_Test::anOlderReaderPlaysTheBakedFunctions()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *b = group("USTs", { m_fixtures[2] });
    const quint32 d50 = dimmer(127, "Half");
    const quint32 d100 = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(d50));
    st->setLayerLook(c2, st->addLayer(c2, groupTarget(b->id())), st->lookForPalette(d100));
    const CueList *list = st->cueList(listId);
    const quint32 chaserId = list->chaser;
    QList<quint32> collections;
    QSet<quint32> scenes;
    for (const Cue &c : list->cues)
    {
        collections << c.collection;
        for (quint32 f : qobject_cast<Collection *>(m_doc->function(c.collection))->functions())
            scenes.insert(f);
    }
    QCOMPARE(collections.count(), 2);
    QVERIFY(scenes.count() >= 2);

    // What an older build reads: the show with <ShowTree> left out.
    QByteArray xml = engineXml(m_doc);
    const int from = xml.indexOf("<ShowTree");
    QVERIFY(from >= 0);
    int to = xml.indexOf("</ShowTree>", from);
    to = to < 0 ? xml.indexOf("/>", from) + 2 : to + 11;
    xml.remove(from, to - from);
    QVERIFY(!xml.contains("ShowTree"));

    Doc *other = new Doc(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other->loadXML(r, false));
    QVERIFY(other->showTree()->productions().isEmpty());

    // Every baked function is there, as an ordinary one.
    Chaser *oldChaser = qobject_cast<Chaser *>(other->function(chaserId));
    QVERIFY(oldChaser != nullptr);
    QVERIFY(!other->showTree()->isGenerated(chaserId));
    QCOMPARE(oldChaser->stepsCount(), 2);
    for (int i = 0; i < 2; ++i)
    {
        QCOMPARE(oldChaser->steps().at(i).fid, collections.at(i));
        Collection *coll = qobject_cast<Collection *>(other->function(collections.at(i)));
        QVERIFY(coll != nullptr);
        QVERIFY(!other->showTree()->isGenerated(coll->id()));
        QCOMPARE(coll->functions(),
                 qobject_cast<Collection *>(m_doc->function(collections.at(i)))->functions());
    }
    for (quint32 sid : scenes)
    {
        Scene *sc = qobject_cast<Scene *>(other->function(sid));
        QVERIFY(sc != nullptr);
        QVERIFY(!other->showTree()->isGenerated(sid));
    }

    // And it plays the same: GO by GO, the same levels on both.
    auto play = [this](Doc *doc, Chaser *chaser) {
        ChaserAction action;
        action.m_action = ChaserNextStep;
        action.m_stepIndex = -1;
        action.m_masterIntensity = 1.0;
        action.m_stepIntensity = 1.0;
        action.m_fadeMode = Chaser::FromFunction;
        chaser->setAction(action);
        if (!chaser->isRunning())
            chaser->start(doc->masterTimer(), FunctionParent::master());
        QList<int> out;
        for (int t = 0; t < 10; ++t)
        {
            doc->masterTimer()->timerTick();
            QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
            ua.first()->processFaders();
            doc->inputOutputMap()->releaseUniverses(false);
        }
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        for (int i = 0; i < m_fixtures.count(); ++i)
            out << int(uchar(ua.first()->preGMValues().at(i)));
        doc->inputOutputMap()->releaseUniverses(false);
        return out;
    };
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(chaserId));
    const QList<int> newOne = play(m_doc, chaser);
    const QList<int> oldOne = play(other, oldChaser);
    QCOMPARE(newOne.mid(0, 3), QList<int>({ 127, 127, 0 }));
    QCOMPARE(oldOne, newOne);
    const QList<int> newTwo = play(m_doc, chaser);
    const QList<int> oldTwo = play(other, oldChaser);
    QCOMPARE(newTwo.mid(0, 3), QList<int>({ 127, 127, 255 }));       // Towers tracks
    QCOMPARE(oldTwo, newTwo);

    oldChaser->stop(FunctionParent::master());
    for (int t = 0; t < 5; ++t)
        other->masterTimer()->timerTick();
    delete other;
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

void ShowTree_Test::ringGroupsAreLayersOfTheirOwn()
{
    QDir dir(TEST_FIXTURE_DIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->load(dir));
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef("Branson", "LED Movinghead+Circle (2 heads)");
    QVERIFY(def != nullptr);
    QList<Fixture *> movers;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("Mover %1").arg(i + 1));
        f->setFixtureDefinition(def, def->mode("12 Channel"));
        f->setAddress(quint32(100 + 20 * i));
        m_doc->addFixture(f);
        movers << f;
    }
    // As Create Group(s) from Selection makes them, a group per head type.
    auto headGroup = [this, &movers](const QString &name, int head) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        m_doc->addFixtureGroup(g);
        g->setSize(QSize(movers.count(), 1));
        for (int i = 0; i < movers.count(); ++i)
            g->assignHead(QLCPoint(i, 0), GroupHead(movers[i]->id(), head));
        return g;
    };
    FixtureGroup *spots = headGroup(QString::fromUtf8("Mover · Movers"), 0);
    FixtureGroup *rings = headGroup(QString::fromUtf8("Mover · Ring"), 1);
    const quint32 blue = colour("Blue");
    QLCPalette *red = new QLCPalette(QLCPalette::Color, m_doc);
    red->setName("Red");
    red->setValue(QLCPalette::colorToString(QColor(Qt::red), QColor(Qt::black)));
    m_doc->addPalette(red);

    // 12 Channel: the wheel is channel 4, the ring 11.
    auto dmx = [this](quint32 address) {
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        const int v = uchar(ua.first()->preGMValues().at(int(address)));
        m_doc->inputOutputMap()->releaseUniverses(false);
        return v;
    };

    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = st->addProduction("P");
    ShowTreeCommand::Result r = cmd.run("list \"Song\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx = r.context;
    r = cmd.run("cue 1 block", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx = r.context;
    const quint32 c1 = ctx.cue;

    // "Ring" names the one "Mover · Ring"; the full name works as ever.
    QCOMPARE(cmd.groupShortNames().value("ring"), rings->id());
    r = cmd.run("Ring colour Blue", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    r = cmd.run(QString::fromUtf8("\"Mover · Movers\" colour Red"), ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(c1)->layers.count(), 2);
    QCOMPARE(st->cue(c1)->layers.at(0).target.groups, QList<quint32>({ rings->id() }));
    QCOMPARE(st->cue(c1)->layers.at(1).target.groups, QList<quint32>({ spots->id() }));

    // Both reach every fixture of their group.
    int n = 0, total = 0;
    st->lookReach(groupTarget(rings->id()), KeyColour, &n, &total);
    QCOMPARE(n, 2);
    QCOMPARE(total, 2);

    // 2: the ring runs a chase, typed by its slot's name; the wheel tracks.
    r = cmd.run("cue 2", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx = r.context;
    const quint32 c2 = ctx.cue;
    QVERIFY(cmd.colourMacroNames().contains("Red/Yellow-Chase"));
    r = cmd.run("Ring colour Red/Yellow-Chase", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    const Layer &ringLayer = st->cue(c2)->layers.first();
    const QLCPalette *chase = m_doc->palette(ringLayer.look(KeyColour)->palettes.value(0));
    QVERIFY(chase != nullptr);
    QCOMPARE(chase->name(), QString("Red/Yellow-Chase"));
    QCOMPARE(chase->colourMacro(), QString("Red/Yellow-Chase"));
    // Typed again, the same palette.
    r = cmd.run("Ring colour Red/Yellow-Chase", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(c2)->layers.first().look(KeyColour)->palettes.value(0), chase->id());

    const QList<ResolvedCue> rc = st->resolved(*st->cueList(ctx.list));
    QCOMPARE(rc.count(), 2);
    CHECK_VALUE(rc[1], spots->id(), KeyColour, red->id(), Origin::Tracked, c1);
    CHECK_VALUE(rc[1], rings->id(), KeyColour, chase->id(), Origin::Own, c2);

    // The preview plays it: wheel and ring each their own.
    st->previewCue(c1);
    tick(5);
    QCOMPARE(dmx(104), 14);     // Red on the wheel
    QCOMPARE(dmx(111), 42);     // Blue on the ring
    QCOMPARE(dmx(124), 14);
    QCOMPARE(dmx(131), 42);
    st->previewCue(c2);
    tick(5);
    QCOMPARE(dmx(104), 14);     // tracked
    QCOMPARE(dmx(111), 117);    // the chase
    QCOMPARE(dmx(131), 117);

    // 3: Mover 1 alone, Blue: its wheel and its ring; Mover 2's ring keeps
    // the chase tracked from 2.
    r = cmd.run("cue 3", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx = r.context;
    r = cmd.run("\"Mover 1\" colour Blue", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    st->previewCue(ctx.cue);
    tick(5);
    QCOMPARE(dmx(104), 34);     // Blue on Mover 1's wheel
    QCOMPARE(dmx(111), 42);     // ...and on its ring
    QCOMPARE(dmx(124), 14);     // Mover 2: red tracked
    QCOMPARE(dmx(131), 117);    // ...and the chase

    // 4: Mover 1 Red, the ring group under it Blue: the lower layer wins
    // the ring, the upper keeps the wheel.
    r = cmd.run("cue 4", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx = r.context;
    r = cmd.run("\"Mover 1\" colour Red", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    r = cmd.run("Ring colour Blue", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    st->previewCue(ctx.cue);
    tick(5);
    QCOMPARE(dmx(104), 14);
    QCOMPARE(dmx(111), 42);
    QCOMPARE(dmx(131), 42);
    st->stopPreview();
    Q_UNUSED(blue)
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
    // What repeats from step to step is set once and tracks: 0.0.2's
    // include of 0.0.1 and Three's TowersA (USTsB released in 0.0.2).
    QCOMPARE(list->mode, ListMode::Tracking);
    QCOMPARE(rep.trackingCues, 2);
    QCOMPARE(rep.trackedIncludes, 1);
    QCOMPARE(rep.trackedLayers, 1);
    QCOMPARE(rep.trackReleases, 1);
    QCOMPARE(list->cues[0].mode, CueMode::Block);
    QCOMPARE(list->cues[1].mode, CueMode::ListDefault);
    QCOMPARE(rep.differChannels, 0);
    QCOMPARE(st->production(rep.production)->name, QString("Full Show"));
    QCOMPARE(list->cues[0].number, QString("0.0.1"));
    QCOMPARE(list->cues[0].name, QString("One"));
    QCOMPARE(list->cues[0].note, QString("After setup"));
    QCOMPARE(list->cues[0].fadeIn, 1000u);
    QCOMPARE(list->cues[0].fadeOut, 500u);
    QVERIFY(list->cues[1].includes.isEmpty());
    QCOMPARE(list->cues[1].layers.count(), 1);
    QVERIFY(list->cues[1].layers.first().release);
    QCOMPARE(list->cues[2].number, QString("7"));
    QCOMPARE(list->cues[2].name, QString("Three"));
    QCOMPARE(list->cues[2].layers.count(), 1);
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

/* The import's tracking reading: a nested collection (no step itself) in
   steps 1, 2 and 4 is set in cue 1 and tracks into 2; it is released in 2
   (fresh layer, it only tracked in there) since 3 doesn't play it, and set
   again in 4. A scene in steps 2-4 is set in 2 and tracks on; one only in
   step 1 is released there. Every cue plays what its step did. */
void ShowTree_Test::importTracksWhatRepeats()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    FixtureGroup *d = group("D", { m_fixtures[2], m_fixtures[3] });
    const quint32 dim = dimmer(150, "Half");
    const quint32 full = dimmer(255, "Full");
    Scene *sA = groupScene(m_doc, "A-Half", a->id(), { dim });
    Scene *sB = groupScene(m_doc, "B-Full", b->id(), { full });
    Scene *sD = groupScene(m_doc, "D-Half", d->id(), { dim });
    Collection *base = collection(m_doc, "0.0.0-base", { sD->id() });
    Collection *one = collection(m_doc, "0.0.1-One", { base->id(), sA->id() });
    Collection *two = collection(m_doc, "0.0.2-Two", { base->id(), sB->id() });
    Collection *three = collection(m_doc, "0.0.3-Three", { sB->id() });
    Collection *four = collection(m_doc, "0.0.4-Four", { base->id(), sB->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Show");
    m_doc->addFunction(chaser);
    for (Collection *c : { one, two, three, four })
        chaser->addStep(ChaserStep(c->id(), 0, Function::infiniteSpeed(), 0));
    const QList<QByteArray> was = playEveryStep(chaser);

    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importChaser(chaser->id());
    QCOMPARE(rep.differChannels, 0);
    QCOMPARE(rep.flattened, 3);
    QCOMPARE(rep.trackingCues, 3);
    QCOMPARE(rep.trackedLayers, 3);         // D into 0.0.2, B into 0.0.3 and 0.0.4
    QCOMPARE(rep.trackReleases, 2);         // A in 0.0.1, D in 0.0.2
    const CueList *list = st->cueList(rep.list);
    QVERIFY(list != nullptr);
    QCOMPARE(list->mode, ListMode::Tracking);
    const Target tA = groupTarget(a->id()), tB = groupTarget(b->id()), tD = groupTarget(d->id());
    auto targets = [&](int i) {
        QList<Target> out;
        for (const Layer &l : list->cues[i].layers)
            out << l.target;
        return out;
    };
    // 0.0.1: the base and A, set; A leaves with it.
    QCOMPARE(list->cues[0].mode, CueMode::Block);
    QCOMPARE(targets(0), QList<Target>({ tD, tA }));
    QVERIFY(!list->cues[0].layers[0].release);
    QVERIFY(list->cues[0].layers[1].release);
    // 0.0.2: B set; the base tracks in and is released here (0.0.3 has none).
    QCOMPARE(list->cues[1].mode, CueMode::ListDefault);
    QCOMPARE(targets(1), QList<Target>({ tB, tD }));
    QVERIFY(!list->cues[1].layers[0].release);
    QVERIFY(list->cues[1].layers[1].release);
    QVERIFY(list->cues[1].layers[1].looks.isEmpty());
    // 0.0.3: nothing of its own, B tracks in.
    QCOMPARE(list->cues[2].mode, CueMode::ListDefault);
    QVERIFY(list->cues[2].layers.isEmpty());
    // 0.0.4: back after the gap, the base is set again; B still tracks.
    QCOMPARE(list->cues[3].mode, CueMode::ListDefault);
    QCOMPARE(targets(3), QList<Target>({ tD }));
    QVERIFY(!list->cues[3].layers[0].looks.isEmpty());

    QCOMPARE(playEveryStep(qobject_cast<Chaser *>(m_doc->function(list->chaser))), was);

    // A chaser where nothing repeats stays Non-tracking.
    Collection *x = collection(m_doc, "X", { sA->id() });
    Collection *y = collection(m_doc, "Y", { sB->id() });
    Chaser *plain = new Chaser(m_doc);
    plain->setName("Plain");
    m_doc->addFunction(plain);
    plain->addStep(ChaserStep(x->id(), 0, Function::infiniteSpeed(), 0));
    plain->addStep(ChaserStep(y->id(), 0, Function::infiniteSpeed(), 0));
    const ShowTreeController::ImportReport rep2 = st->importChaser(plain->id());
    QCOMPARE(rep2.trackingCues, 0);
    QCOMPARE(st->cueList(rep2.list)->mode, ListMode::NonTracking);
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
    // The report's own check (W0) says what playing it shows below.
    QCOMPARE(rep.checkedCues, 5);
    QCOMPARE(rep.differChannels, 0);
    QVERIFY(rep.differCues.isEmpty());
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

/* DanceHysteria-0.6's Full Show 0.0.5: it includes 0.0.4 (StepTops at
   full) and sets StepTops at 45% itself. The chaser ran both, highest-wins on
   intensity, so full showed; the tree's rule plays the cue's own 45% (own
   over included). The importer makes such a cue mix -- only where the output
   would change -- and then it plays as the chaser did. */
void ShowTree_Test::importAnIncludeBrighterThanTheCuesOwnMixes()
{
    FixtureGroup *tops = group("StepTops", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *bar = group("Bar", { m_fixtures[2] });
    const quint32 full = dimmer(255, "Full");
    const quint32 low = dimmer(115, "Intensity 45%");
    Scene *topsFull = groupScene(m_doc, "StepTops-Full", tops->id(), { full });
    Scene *topsLow = groupScene(m_doc, "StepTops-45", tops->id(), { low });
    Scene *barLow = groupScene(m_doc, "Bar-45", bar->id(), { low });
    Collection *shout = collection(m_doc, "0.0.4-SHOUT", { topsFull->id() });
    // The include at full beside its own StepTops at 45%: the rule changes it.
    Collection *who = collection(m_doc, "0.0.5-WHO ARE WE", { shout->id(), topsLow->id() });
    // An include beside its own scene on other fixtures: the rule plays the same.
    Collection *calm = collection(m_doc, "0.0.6-CALM", { shout->id(), barLow->id() });
    // Only the cue that mixes, included: as a plain include it would be 45%.
    Collection *stomp = collection(m_doc, "0.0.7-STOMP", { who->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    chaser->setDurationMode(Chaser::PerStep);   // each step holds until GO
    m_doc->addFunction(chaser);
    for (Collection *coll : { shout, who, calm, stomp })
        chaser->addStep(ChaserStep(coll->id(), 0, Function::infiniteSpeed(), 0));

    const QList<QHash<quint32, uchar>> original = playAll(chaser);
    QCOMPARE(int(original[1][0]), 255);         // 0.0.5: the include's full won (HTP)
    QCOMPARE(int(original[3][1]), 255);         // 0.0.7, through it

    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importChaser(chaser->id());
    const CueList *list = st->cueList(rep.list);
    QVERIFY(list != nullptr);
    QCOMPARE(list->cues.count(), 4);
    QVERIFY(!list->cues[0].mixOverlap);
    QVERIFY(list->cues[1].mixOverlap);
    QVERIFY(!list->cues[2].mixOverlap);         // only where the output would differ
    QVERIFY(list->cues[3].mixOverlap);
    QCOMPARE(rep.mixedIncludes.count(), 2);
    QVERIFY2(rep.mixedIncludes.first().contains("0.0.5 WHO ARE WE")
             && rep.mixedIncludes.first().contains("0.0.4 SHOUT")
             && rep.mixedIncludes.first().contains("2 channel(s)"), qPrintable(rep.mixedIncludes.first()));
    QVERIFY(rep.notes.contains(rep.mixedIncludes.first()));
    // It plays as the chaser did, so it's no longer listed as a conflict.
    QVERIFY2(rep.htpConflicts.isEmpty(), qPrintable(rep.htpConflicts.join(" | ")));

    Chaser *imported = qobject_cast<Chaser *>(m_doc->function(list->chaser));
    QVERIFY(imported != nullptr);
    const QList<QHash<quint32, uchar>> now = playAll(imported);
    QCOMPARE(now.count(), original.count());
    for (int i = 0; i < now.count(); ++i)
        for (quint32 ch = 0; ch < quint32(m_fixtures.count()); ++ch)
            QVERIFY2(now[i].value(ch) == original[i].value(ch),
                     qPrintable(QString("cue %1, channel %2: %3, the chaser gave %4")
                                .arg(i + 1).arg(ch).arg(now[i].value(ch)).arg(original[i].value(ch))));

    // Its stage holds both levels (HTP), and its own layer is the one found.
    const QList<ResolvedCue> r = resolve(*list);
    QCOMPARE(ShowTreeController::scenePalettes(*r[1].layer(groupTarget(tops->id()))),
             QList<quint32>({ low }));
    QCOMPARE(stageOf(r[1]).value(QString("g:%1").arg(tops->id())).value(KeyDimmer).count(), 2);
    // What it passes on is the one rule's: one layer per target.
    int onTops = 0;
    for (const ResolvedLayer &l : r[1].passOn)
        onTops += l.target == groupTarget(tops->id()) ? 1 : 0;
    QCOMPARE(onTops, 1);

    // On the tree's rule the cue's own 45% plays (what the mix keeps away).
    const quint32 whoId = list->cues[1].id;
    QVERIFY(st->setCueMixOverlap(whoId, false));
    const QList<QHash<quint32, uchar>> rule = playAll(imported);
    QCOMPARE(int(rule[1][0]), 115);
    QVERIFY(st->undo());
    QVERIFY(st->cue(whoId)->mixOverlap);
}

/* The dry run reads the show's own definitions, as App does when it opens
   one: a show whose fixtures' definition lives only in its author's user
   folder compares channel for channel on a computer without it. */
void ShowTree_Test::dryRunUsesTheShowsOwnDefinitions()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    struct Restore
    {
        QByteArray h = qgetenv("HOME"), p = qgetenv("USERPROFILE");
        ~Restore()
        {
            qputenv("HOME", h);
            if (p.isEmpty())
                qunsetenv("USERPROFILE");
            else
                qputenv("USERPROFILE", p);
        }
    } restore;
    // The author's computer: the definition only in the user folder.
    const QString author = root.filePath("author");
    QVERIFY(QDir().mkpath(author));
    qputenv("HOME", author.toUtf8());
    qputenv("USERPROFILE", author.toUtf8());
    const QDir userDefs = QLCFixtureDefCache::userDefinitionDirectory();
    QVERIFY(QDir().mkpath(userDefs.absolutePath()));
    {
        const QByteArray qxf =
            "<?xml version='1.0' encoding='UTF-8'?>\n<!DOCTYPE FixtureDefinition>\n"
            "<FixtureDefinition xmlns=\"http://www.qlcplus.org/FixtureDefinition\">\n"
            " <Creator>\n  <Name>Q Light Controller Plus</Name>\n  <Version>4</Version>\n"
            "  <Author>test</Author>\n </Creator>\n"
            " <Manufacturer>Test Maker</Manufacturer>\n <Model>Step Top</Model>\n"
            " <Type>Color Changer</Type>\n"
            " <Channel Name=\"Dimmer\" Preset=\"IntensityMasterDimmer\"/>\n"
            " <Channel Name=\"Red\" Preset=\"IntensityRed\"/>\n"
            " <Channel Name=\"Green\" Preset=\"IntensityGreen\"/>\n"
            " <Channel Name=\"Blue\" Preset=\"IntensityBlue\"/>\n"
            " <Mode Name=\"4ch\">\n"
            "  <Channel Number=\"0\">Dimmer</Channel>\n  <Channel Number=\"1\">Red</Channel>\n"
            "  <Channel Number=\"2\">Green</Channel>\n  <Channel Number=\"3\">Blue</Channel>\n"
            " </Mode>\n</FixtureDefinition>\n";
        QFile f(userDefs.absoluteFilePath("Test-Maker-Step-Top.qxf"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        QCOMPARE(f.write(qxf), qint64(qxf.size()));
    }

    // The show, saved there as a .qlcc (DanceHysteria's 0.0.4 / 0.0.5).
    const QString showPath = root.filePath("show/Step Show.qlcc");
    QVERIFY(QDir().mkpath(QFileInfo(showPath).absolutePath()));
    {
        Doc *doc = new Doc(this);
        doc->fixtureDefCache()->load(QLCFixtureDefCache::userDefinitionDirectory());
        doc->fixtureDefCache()->loadMap(QDir(INTERNAL_FIXTUREDIR));
        QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef("Test Maker", "Step Top");
        QVERIFY(def != nullptr && def->mode("4ch") != nullptr);
        FixtureGroup *tops = new FixtureGroup(doc);
        tops->setName("StepTops");
        doc->addFixtureGroup(tops);
        for (int i = 0; i < 2; ++i)
        {
            Fixture *fx = new Fixture(doc);
            fx->setName(QString("Top %1").arg(i + 1));
            fx->setUniverse(0);
            fx->setAddress(quint32(i * 4));
            fx->setFixtureDefinition(def, def->mode("4ch"));
            doc->addFixture(fx);
            tops->assignFixture(fx->id());
        }
        auto palette = [doc](QLCPalette::PaletteType type, const QString &name, const QVariant &value) {
            QLCPalette *p = new QLCPalette(type, doc);
            p->setName(name);
            p->setValue(value);
            doc->addPalette(p);
            return p->id();
        };
        const quint32 white = palette(QLCPalette::Color, "White",
                                      QLCPalette::colorToString(QColor(Qt::white), QColor(Qt::black)));
        const quint32 full = palette(QLCPalette::Dimmer, "Full", 255);
        const quint32 low = palette(QLCPalette::Dimmer, "Intensity 45%", 115);
        Scene *shoutScene = groupScene(doc, "StepTops White", tops->id(), { white, full });
        Scene *whoScene = groupScene(doc, "StepTops White 45%", tops->id(), { white, low });
        Collection *shout = collection(doc, "0.0.4-SHOUT", { shoutScene->id() });
        Collection *who = collection(doc, "0.0.5-WHO ARE WE", { shout->id(), whoScene->id() });
        Chaser *chaser = new Chaser(doc);
        chaser->setName("Full Show");
        chaser->setDurationMode(Chaser::PerStep);
        doc->addFunction(chaser);
        chaser->addStep(ChaserStep(shout->id(), 0, Function::infiniteSpeed(), 0));
        chaser->addStep(ChaserStep(who->id(), 0, Function::infiniteSpeed(), 0));

        doc->showEmbedding()->collect();
        QCOMPARE(doc->showEmbedding()->items().count(), 1);
        QFile f(showPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QXmlStreamWriter w(&f);
        w.writeStartDocument();
        w.writeDTD(QStringLiteral("<!DOCTYPE Workspace>"));
        w.writeStartElement(QStringLiteral("Workspace"));
        doc->showEmbedding()->saveXML(&w);
        doc->saveXML(&w);
        w.writeEndElement();
        w.writeEndDocument();
        f.close();
        delete doc;
    }

    // The dry run: no user folder with the definition (it hides this
    // computer's), so the show's copy is the only one.
    const DryRun run = dryRunImport(showPath);
    QVERIFY2(run.loaded, qPrintable(run.problems.join(" | ")));
    QVERIFY2(run.problems.isEmpty(), qPrintable(run.problems.join(" | ")));
    QCOMPARE(run.showDefinitions, 1);
    QCOMPARE(m_doc->fixtures().count(), 2);
    for (Fixture *fx : m_doc->fixtures())
    {
        QVERIFY(fx->fixtureDef() != nullptr);
        QCOMPARE(fx->fixtureDef()->manufacturer(), QString("Test Maker"));
        QCOMPARE(fx->channels(), quint32(4));
    }
    QCOMPARE(run.lists, 1);
    QCOMPARE(run.cues, 2);
    for (const DryRun::Played &p : run.played)
    {
        QVERIFY2(p.lit == 8, qPrintable(QString("%1: %2 lit").arg(p.cue).arg(p.lit)));
        QCOMPARE(p.differ, 0);
    }
    QCOMPARE(run.differ, 0);
    QCOMPARE(run.convertedDiffer, 0);
    QCOMPARE(run.mixedIncludes.count(), 1);
}

/* W0: the import report checks what it made. Two of a cue's own scenes on
   one group, the brighter first: the chaser ran both (HTP, the brighter
   showed), the list keeps the later one's value -- and the check says so,
   for that cue and that channel, which playing the list then shows. */
void ShowTree_Test::importChecksTheOutput()
{
    FixtureGroup *a = group("StepTops", { m_fixtures[0] });
    FixtureGroup *b = group("USTs", { m_fixtures[1] });
    const quint32 dim = dimmer(60, "Dim");
    const quint32 bright = dimmer(250, "Bright");
    Scene *sBright = groupScene(m_doc, "StepTops-Bright", a->id(), { bright });
    Scene *sDim = groupScene(m_doc, "StepTops-Dim", a->id(), { dim });
    Scene *ust = groupScene(m_doc, "USTs-Dim", b->id(), { dim });
    Collection *one = collection(m_doc, "0.0.1-One", { ust->id() });
    Collection *two = collection(m_doc, "0.0.2-Two", { sBright->id(), sDim->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    chaser->setDurationMode(Chaser::PerStep);
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(one->id(), 0, Function::infiniteSpeed(), 0));
    chaser->addStep(ChaserStep(two->id(), 0, Function::infiniteSpeed(), 0));

    go(chaser);
    settle(10);
    go(chaser);
    QCOMPARE(int(settle(10).value(0)), 250);   // HTP: the brighter
    stopAll();

    const ShowTreeController::ImportReport rep = m_doc->showTree()->importChaser(chaser->id());
    QCOMPARE(rep.htpConflicts.count(), 1);
    QCOMPARE(rep.checkedCues, 2);
    QCOMPARE(rep.differChannels, 1);
    QCOMPARE(rep.differCues.count(), 1);
    QVERIFY2(rep.differCues.first().startsWith("Cue 0.0.2 Two: 1 channel"), qPrintable(rep.differCues.first()));
    // Asked again, the same answer.
    QStringList lines;
    int checked = 0;
    QCOMPARE(m_doc->showTree()->importDifferences(chaser->id(), rep.list, &lines, &checked), 1);
    QCOMPARE(checked, 2);
    QCOMPARE(lines, rep.differCues);

    // And it is true: the list plays the later scene's value.
    Chaser *imported = qobject_cast<Chaser *>(m_doc->function(m_doc->showTree()->cueList(rep.list)->chaser));
    QVERIFY(imported != nullptr);
    go(imported);
    settle(10);
    go(imported);
    QCOMPARE(int(settle(10).value(0)), 60);
}

/* W0, Sample.qxw's "201 channels": a looping chaser of raw scenes, each
   with its own 1 s fade, fade mode Default, steps shorter than the fade.
   QLC+ blends a scene from the one before (Scene::setBlendFunctionID); the
   list's steps are Collections, which never blended, so the list ran at
   half the level. A cue Collection of one scene played as it is now blends
   as the scene did: tick for tick the same. */
void ShowTree_Test::importedRawScenesBlendAsTheChaserDid()
{
    auto raw = [this](const QString &name, int a, int b) {
        Scene *s = new Scene(m_doc);
        s->setName(name);
        s->setFadeInSpeed(1000);
        s->setFadeOutSpeed(1000);
        s->setValue(m_fixtures[0], 0, uchar(a));
        s->setValue(m_fixtures[1], 0, uchar(b));
        m_doc->addFunction(s);
        return s;
    };
    Scene *one = raw("par 1", 255, 0);
    Scene *two = raw("par 2", 0, 255);
    Scene *three = raw("par 3", 255, 255);
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Par chaser");
    chaser->setRunOrder(Function::Loop);
    chaser->setDurationMode(Chaser::Common);
    chaser->setFadeInMode(Chaser::Default);
    chaser->setFadeOutMode(Chaser::Default);
    m_doc->addFunction(chaser);
    for (Scene *s : { one, two, three })
        chaser->addStep(ChaserStep(s->id()));

    auto trace = [this](Chaser *c) {
        m_doc->inputOutputMap()->resetUniverses();
        c->start(m_doc->masterTimer(), FunctionParent::master());
        QStringList out;
        for (int t = 0; t < 150; ++t)
        {
            const QHash<quint32, uchar> now = settle(1);
            out << QString("%1/%2").arg(now.value(0)).arg(now.value(1));
        }
        stopAll();
        tick(100);
        return out;
    };
    for (uint duration : { 0u, 300u, 2000u })
    {
        chaser->setDuration(duration);
        const QStringList original = trace(chaser);
        const ShowTreeController::ImportReport rep = m_doc->showTree()->importChaser(chaser->id());
        QCOMPARE(rep.opaqueLayers, 3);
        Chaser *imported = qobject_cast<Chaser *>(m_doc->function(m_doc->showTree()->cueList(rep.list)->chaser));
        QVERIFY(imported != nullptr);
        QCOMPARE(imported->duration(), duration);
        const QStringList list = trace(imported);
        for (int t = 0; t < original.count(); ++t)
            QVERIFY2(list.at(t) == original.at(t),
                     qPrintable(QString("duration %1, tick %2: the list %3, the chaser %4")
                                .arg(duration).arg(t).arg(list.at(t), original.at(t))));
        QVERIFY(m_doc->showTree()->undo());
    }
    // A Collection of its own (not the tree's) still never blends, as in QLC+.
    Collection *mine = collection(m_doc, "mine", { one->id() });
    QVERIFY(ChaserRunner::blendScene(m_doc, mine) == nullptr);
    QCOMPARE(ChaserRunner::blendScene(m_doc, one), one);
}

/* W0: the stock sample (QLC+ 4.0.2, 13 looping chasers of raw scenes, 5
   EFX) imports and plays the same, by the dry run and by the report's own
   check. */
void ShowTree_Test::importTheSampleShow()
{
    const DryRun run = dryRunImport(QStringLiteral(SAMPLE_SHOW));
    QVERIFY2(run.loaded, qPrintable(run.problems.join(" | ")));
    QVERIFY2(run.problems.isEmpty(), qPrintable(run.problems.join(" | ")));
    QCOMPARE(run.lists, 13);
    QCOMPARE(run.cues, 47);
    QCOMPARE(run.differ, 0);
    QCOMPARE(run.staticDiffer, 0);
    QCOMPARE(run.convertedDiffer, 0);
}

/* W0: an undo step can carry what it changed outside the tree (the VC
   buttons an import re-pointed): undone, then redone, with the step. */
void ShowTree_Test::anUndoStepCarriesWhatItChangedOutside()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(!st->attachToLastStep([](bool) {}));
    QVERIFY(st->addProduction("Show") != invalidId());
    QStringList calls;
    QVERIFY(st->attachToLastStep([&calls](bool undone) { calls << (undone ? "undo" : "redo"); }));
    QVERIFY(st->undo());
    QCOMPARE(calls, QStringList({ "undo" }));
    QVERIFY(st->redo());
    QCOMPARE(calls, QStringList({ "undo", "redo" }));
    QVERIFY(st->undo());
    QCOMPARE(calls, QStringList({ "undo", "redo", "undo" }));
}

/****************************************************************************
 * W5: the bridge
 ****************************************************************************/

/* A chaser that waits for GO, numbers its cues, carries notes or is what
   show control plays is a cue list; one that only loops on its own timing
   is an effect, and stays a function. */
void ShowTree_Test::bridgeTellsCueListsFromEffects()
{
    ShowTreeController *st = m_doc->showTree();
    Scene *a = new Scene(m_doc);
    a->setValue(m_fixtures[0], 0, 255);
    m_doc->addFunction(a);
    Scene *b = new Scene(m_doc);
    b->setValue(m_fixtures[1], 0, 255);
    m_doc->addFunction(b);

    Chaser *strobe = new Chaser(m_doc);
    strobe->setName("Strobe");
    strobe->setRunOrder(Function::Loop);
    strobe->setDurationMode(Chaser::Common);
    strobe->setDuration(100);
    m_doc->addFunction(strobe);
    strobe->addStep(ChaserStep(a->id()));
    strobe->addStep(ChaserStep(b->id()));
    ShowTreeController::ChaserClass c = st->classifyChaser(strobe->id());
    QCOMPARE(c.kind, ShowTreeController::ChaserKind::Effect);
    QVERIFY2(c.why.join("; ").contains("loops"), qPrintable(c.why.join("; ")));
    QVERIFY(c.why.join("; ").contains("raw scenes"));
    // ...unless something plays it as a list.
    QCOMPARE(st->classifyChaser(strobe->id(), true).kind, ShowTreeController::ChaserKind::CueList);
    m_doc->showControl()->setMainCueList(strobe->id());
    QCOMPARE(st->classifyChaser(strobe->id()).kind, ShowTreeController::ChaserKind::CueList);
    m_doc->showControl()->setMainCueList(Function::invalidId());

    Chaser *show = new Chaser(m_doc);
    show->setName("Show");
    show->setRunOrder(Function::Loop);        // DanceHysteria's Full Show loops too
    show->setDurationMode(Chaser::PerStep);
    m_doc->addFunction(show);
    show->addStep(ChaserStep(a->id(), 0, Function::infiniteSpeed(), 0));
    show->addStep(ChaserStep(b->id(), 0, 2000, 0));
    c = st->classifyChaser(show->id());
    QCOMPARE(c.kind, ShowTreeController::ChaserKind::CueList);
    QVERIFY2(c.why.join("; ").contains("waits for GO on 1 of 2"), qPrintable(c.why.join("; ")));

    Chaser *noted = new Chaser(m_doc);
    noted->setDurationMode(Chaser::PerStep);
    m_doc->addFunction(noted);
    ChaserStep s(a->id(), 0, 5000, 0);
    s.note = "Wait for the band";
    noted->addStep(s);
    QCOMPARE(st->classifyChaser(noted->id()).kind, ShowTreeController::ChaserKind::CueList);

    // The converter lists the show's own chasers, not the tree's.
    const ShowTreeController::ImportReport rep = st->importChaser(show->id());
    const quint32 generated = st->cueList(rep.list)->chaser;
    QVERIFY(!st->convertibleChasers().contains(generated));
    QVERIFY(st->convertibleChasers().contains(show->id()));
}

/* A collection inside a step that is no step itself played its members as
   the step's own: they become the cue's layers (or ▣ rows for raw scenes);
   one that plays nothing is left out. The cue plays the same. */
void ShowTree_Test::bridgeFlattensCollectionsThatAreNoStep()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *usts = group("USTs", { m_fixtures[2] });
    Scene *sA = groupScene(m_doc, "TowersFull", towers->id(), { dimmer(255, "Full") });
    Scene *sB = groupScene(m_doc, "USTsHalf", usts->id(), { dimmer(127, "Half") });
    Scene *raw = new Scene(m_doc);
    raw->setName("Haze");
    raw->setValue(m_fixtures[5], 0, 90);
    m_doc->addFunction(raw);
    Collection *empty = collection(m_doc, "0.0.0-base", {});
    Collection *inner = collection(m_doc, "Wash", { sB->id(), raw->id() });
    Collection *one = collection(m_doc, "0.0.1-One", { empty->id(), sA->id() });
    Collection *two = collection(m_doc, "0.0.2-Two", { empty->id(), inner->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Show");
    chaser->setDurationMode(Chaser::PerStep);
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(one->id(), 0, Function::infiniteSpeed(), 0));
    chaser->addStep(ChaserStep(two->id(), 0, Function::infiniteSpeed(), 0));

    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importChaser(chaser->id());
    QCOMPARE(rep.dropped, 2);           // 0.0.0-base in both cues
    QCOMPARE(rep.flattened, 1);         // Wash
    QCOMPARE(rep.layers, 2);            // TowersFull, USTsHalf
    QCOMPARE(rep.opaqueLayers, 1);      // Haze, a raw scene, as itself
    QCOMPARE(rep.differChannels, 0);
    QVERIFY(rep.flattenNotes.join("\n").contains("0.0.0-base plays nothing"));
    const CueList *list = st->cueList(rep.list);
    QCOMPARE(list->cues[1].layers.count(), 2);
    QCOMPARE(list->cues[1].layers[0].target, groupTarget(usts->id()));
    QCOMPARE(list->cues[1].layers[1].target.function, raw->id());
    QCOMPARE(list->source, chaser->id());

    // Played, it is what the chaser played.
    const QList<QByteArray> was = playEveryStep(chaser);
    const QList<QByteArray> now = playEveryStep(qobject_cast<Chaser *>(m_doc->function(list->chaser)));
    QCOMPARE(now, was);
}

/* The converter's plan is the import, made without touching the show. */
void ShowTree_Test::bridgePlansWithoutChangingTheShow()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    Scene *sA = groupScene(m_doc, "TowersFull", towers->id(), { dimmer(255, "Full") });
    Collection *empty = collection(m_doc, "base", {});
    Collection *one = collection(m_doc, "0.0.1-One", { empty->id(), sA->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Show");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(one->id(), 0, Function::infiniteSpeed(), 0));

    ShowTreeController *st = m_doc->showTree();
    const QByteArray before = engineXml(m_doc);
    CueList planned;
    const ShowTreeController::ImportReport plan = st->planImport(chaser->id(), &planned);
    QCOMPARE(engineXml(m_doc), before);
    QVERIFY(!st->canUndo());
    QCOMPARE(plan.cues, 1);
    QCOMPARE(plan.layers, 1);
    QCOMPARE(plan.dropped, 1);
    QCOMPARE(plan.checkedCues, 1);
    QCOMPARE(plan.differChannels, 0);
    QCOMPARE(planned.cues.count(), 1);

    const ShowTreeController::ImportReport rep = st->importChaser(chaser->id());
    QCOMPARE(rep.layers, plan.layers);
    QCOMPARE(rep.opaqueLayers, plan.opaqueLayers);
    QCOMPARE(rep.differChannels, plan.differChannels);
}

/* Import in place files the chaser and what only it plays under
   Imported/<chaser> (originals), one undo step with the import; what the
   list plays as it is, and what something else plays, stays put. */
void ShowTree_Test::importInPlaceFilesTheOriginals()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    Scene *sA = groupScene(m_doc, "TowersFull", towers->id(), { dimmer(255, "Full") });
    sA->setPath("Spots");
    Scene *raw = new Scene(m_doc);
    raw->setName("Haze");
    raw->setPath("Washes");
    raw->setValue(m_fixtures[5], 0, 90);
    m_doc->addFunction(raw);
    Scene *shared = groupScene(m_doc, "Shared", towers->id(), { dimmer(100, "Low") });
    Collection *one = collection(m_doc, "0.0.1-One", { sA->id(), raw->id() });
    one->setPath("Show");
    Collection *two = collection(m_doc, "0.0.2-Two", { shared->id() });
    Collection *elsewhere = collection(m_doc, "Busk", { shared->id() });
    Q_UNUSED(elsewhere)
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(one->id(), 0, Function::infiniteSpeed(), 0));
    chaser->addStep(ChaserStep(two->id(), 0, Function::infiniteSpeed(), 0));

    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importInPlace(chaser->id());
    QCOMPARE(rep.differChannels, 0);
    const QString folder = "Imported/Full Show (originals)";
    QCOMPARE(ShowTreeController::originalsFolder("Full Show"), folder);
    QCOMPARE(chaser->path(true), folder);
    QCOMPARE(one->path(true), folder);
    QCOMPARE(two->path(true), folder);
    QCOMPARE(sA->path(true), folder);           // read into a layer: the list doesn't play it
    QCOMPARE(raw->path(true), QString("Washes")); // played as it is: editing it changes the list
    QCOMPARE(shared->path(true), QString());    // Busk plays it too
    QCOMPARE(rep.filed, 4);
    const CueList *list = st->cueList(rep.list);
    QVERIFY(list->inPlace);
    QCOMPARE(list->source, chaser->id());
    QCOMPARE(list->originals.count(), 4);
    QCOMPARE(st->importedInto(chaser->id()), rep.list);
    QCOMPARE(st->importedInto(raw->id()), invalidId());
    QVERIFY(!st->convertibleChasers().contains(chaser->id()));

    // One undo step with the import.
    QCOMPARE(st->undoText(), QString("Import Full Show"));
    QVERIFY(st->undo());
    QVERIFY(st->productions().isEmpty());
    QCOMPARE(chaser->path(true), QString());
    QCOMPARE(one->path(true), QString("Show"));
    QCOMPARE(sA->path(true), QString("Spots"));
    QVERIFY(st->redo());
    QCOMPARE(chaser->path(true), folder);
    QCOMPARE(sA->path(true), folder);
    QVERIFY(st->cueList(rep.list)->inPlace);
    QVERIFY(!st->canRedo());

    // Restore original: the list goes, the originals go back.
    QString why;
    QVERIFY2(st->canRestoreOriginal(rep.list, &why), qPrintable(why));
    QVERIFY(st->restoreOriginal(rep.list));
    QVERIFY(st->cueList(rep.list) == nullptr);
    QVERIFY(st->productions().isEmpty());
    QCOMPARE(chaser->path(true), QString());
    QCOMPARE(one->path(true), QString("Show"));
    QCOMPARE(sA->path(true), QString("Spots"));
    QCOMPARE(st->undoText(), QString("Restore original Full Show"));
    QVERIFY(st->undo());
    QCOMPARE(chaser->path(true), folder);
    QVERIFY(st->cueList(rep.list) != nullptr);
    QVERIFY(st->redo());
    QCOMPARE(chaser->path(true), QString());

    // Not for a list made in the tree, or imported beside its chaser.
    const ShowTreeController::ImportReport beside = st->importChaser(chaser->id());
    QCOMPARE(st->cueList(beside.list)->source, chaser->id());
    QVERIFY(!st->canRestoreOriginal(beside.list, &why));
    QVERIFY(why.contains("beside"));
}

/* What Import in place keeps for Restore original and the list doesn't
   play is an original, view only (Q141), wherever it is filed: the banner
   names the cue that stands for it. What the tree made keeps its own
   words; what the list plays as it is, and what something else plays too,
   is the user's to edit. A scene Capture into palettes leaves behind is an
   original from then on. */
void ShowTree_Test::originalsKeptForRestoreAreViewOnly()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    Scene *sA = groupScene(m_doc, "TowersFull", towers->id(), { dimmer(255, "Full") });
    Scene *sB = groupScene(m_doc, "TowersLow", towers->id(), { dimmer(100, "Low") });
    Scene *raw = new Scene(m_doc);
    raw->setName("Haze");
    raw->setPath("Washes");
    raw->setValue(m_fixtures[2], 0, 90);
    m_doc->addFunction(raw);
    Scene *shared = groupScene(m_doc, "Shared", towers->id(), { dimmer(50, "Half") });
    Collection *one = collection(m_doc, "0.0.1-One", { sA->id(), raw->id() });
    Collection *two = collection(m_doc, "0.0.2-Two", { sB->id(), shared->id() });
    Collection *busk = collection(m_doc, "Busk", { shared->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(one->id(), 0, Function::infiniteSpeed(), 0));
    chaser->addStep(ChaserStep(two->id(), 0, Function::infiniteSpeed(), 0));

    ShowTreeController *st = m_doc->showTree();
    QCOMPARE(st->originalFor(sA->id()), invalidId());       // nothing imported yet
    const ShowTreeController::ImportReport rep = st->importInPlace(chaser->id());
    const CueList *list = st->cueList(rep.list);
    QVERIFY(list != nullptr && list->cues.count() == 2);
    const QString where = "The show plays the Show Tree's cue list Full Show; edit cue %1 there.";

    // Originals kept for Restore original: view only, the cue named.
    quint32 cue = invalidId();
    QCOMPARE(st->originalFor(sA->id(), &cue), rep.list);
    QCOMPARE(cue, list->cues[0].id);
    QCOMPARE(st->originalFor(sB->id(), &cue), rep.list);
    QCOMPARE(cue, list->cues[1].id);
    QCOMPARE(st->originalFor(two->id(), &cue), rep.list);
    QCOMPARE(cue, list->cues[1].id);
    QVERIFY(st->isLocked(sA->id()));
    QVERIFY(!st->isGenerated(sA->id()));
    QCOMPARE(st->originalWhere(sA->id()), QString(where).arg(st->cueLabel(*list, 0)));
    QCOMPARE(st->lockedTextFor(sB->id()),
             ShowTreeController::originalText() + " " + QString(where).arg(st->cueLabel(*list, 1)));
    QVERIFY(st->lockedTextFor(sB->id()).startsWith("Original from the import, kept for Restore original."));
    // The chaser itself: the list, no one cue.
    QCOMPARE(st->originalFor(chaser->id(), &cue), rep.list);
    QCOMPARE(cue, invalidId());
    QCOMPARE(st->originalWhere(chaser->id()),
             QString("The show plays the Show Tree's cue list Full Show; edit it there."));
    // Filed elsewhere since: still an original.
    sA->setPath("Spots");
    QCOMPARE(st->originalFor(sA->id()), rep.list);

    // Played as it is (an opaque layer), or by something else too: the
    // user's, not locked.
    QCOMPARE(st->originalFor(raw->id()), invalidId());
    QVERIFY(!st->isLocked(raw->id()));
    QVERIFY(st->lockedTextFor(raw->id()).isEmpty());
    QCOMPARE(st->originalFor(shared->id()), invalidId());
    QCOMPARE(st->originalFor(busk->id()), invalidId());
    QVERIFY(st->lockedTextFor(shared->id()).isEmpty());

    // The tree's own: its own words.
    const Collection *made = qobject_cast<const Collection *>(m_doc->function(list->cues[0].collection));
    QVERIFY(made != nullptr && st->isGenerated(made->id()));
    QCOMPARE(st->originalFor(list->chaser), invalidId());
    QCOMPARE(st->lockedTextFor(list->chaser), ShowTreeController::lockedText());
    QCOMPARE(st->lockedTextFor(made->id()), ShowTreeController::lockedText());
    for (quint32 fid : made->functions())
        if (st->isGenerated(fid))
        {
            QVERIFY(st->isLocked(fid));
            QCOMPARE(st->lockedTextFor(fid), ShowTreeController::lockedText());
        }

    // Capture into palettes: the list no longer plays the scene, so it is
    // an original too (what an Eos show upgraded and then captured is).
    QVERIFY(st->captureIntoPalettes(raw->id()));
    QCOMPARE(st->originalFor(raw->id(), &cue), rep.list);
    QCOMPARE(cue, list->cues[0].id);
    QVERIFY(st->undo());
    QCOMPARE(st->originalFor(raw->id()), invalidId());

    // Send to Show Tree: refused for the chaser (upgraded already).
    QVERIFY(st->whyNotSend(chaser->id()).startsWith("Upgraded already"));

    // Restore original: the user's again.
    QVERIFY(st->restoreOriginal(rep.list));
    QCOMPARE(st->originalFor(sA->id()), invalidId());
    QCOMPARE(st->originalFor(chaser->id()), invalidId());
    QVERIFY(st->lockedTextFor(sB->id()).isEmpty());
}

/** The first line two XML texts differ at, as a person reads it (empty: none). */
static QString firstDifference(const QByteArray &a, const QByteArray &b)
{
    const QList<QByteArray> x = a.split('\n'), y = b.split('\n');
    for (int i = 0; i < qMax(x.count(), y.count()); ++i)
        if (x.value(i) != y.value(i))
            return QString("line %1: %2 | %3").arg(i + 1).arg(QString::fromUtf8(x.value(i)).trimmed(),
                                                         QString::fromUtf8(y.value(i)).trimmed());
    return QString();
}

/* Restore original after Import in place on DanceHysteria: the show is as
   it was -- every function and palette, XML for XML. */
void ShowTree_Test::restoreOriginalPutsTheShowBack()
{
    QTemporaryDir home;
    HomeAway away(home.path());
    DryRun run;
    QVERIFY2(loadShowFile(QStringLiteral(DH_SHOW), run), qPrintable(run.problems.join(" | ")));
    ShowTreeController *st = m_doc->showTree();
    auto withoutTree = [](QByteArray xml) {
        const int a = xml.indexOf("<ShowTree");
        if (a >= 0)
        {
            int b = xml.indexOf("</ShowTree>", a);
            b = b < 0 ? xml.indexOf("/>", a) + 2 : b + 11;
            xml.remove(a, b - a);
        }
        return xml;
    };
    const QByteArray before = withoutTree(engineXml(m_doc));
    quint32 full = Function::invalidId();
    for (Function *f : m_doc->functions())
        if (f->name() == "Full Show")
            full = f->id();
    QVERIFY(full != Function::invalidId());
    const ShowTreeController::ImportReport rep = st->importInPlace(full);
    QVERIFY(rep.filed > 0);
    QVERIFY(withoutTree(engineXml(m_doc)) != before);
    QVERIFY(st->restoreOriginal(rep.list));
    QString diff = firstDifference(before, withoutTree(engineXml(m_doc)));
    QVERIFY2(diff.isEmpty(), qPrintable(diff));
    // Restore undone, then the import: still the same.
    QVERIFY(st->undo());
    QVERIFY(st->undo());
    diff = firstDifference(before, withoutTree(engineXml(m_doc)));
    QVERIFY2(diff.isEmpty(), qPrintable(diff));
}

/* The link and the filing save with the show; an older build, which reads
   neither, still loads the list and plays it, and keeps the originals
   where they were filed. */
/* CHANGELOG 0.2.2, Upgrade This Show: Import in place "moves VC widgets,
   timelines and show control to the list". A Shows timeline item that
   played the chaser plays the list's chaser, at the same place on its
   track; Undo and Restore original point it back at the chaser. */
void ShowTree_Test::importInPlaceMovesTimelinesToTheList()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    Scene *sA = groupScene(m_doc, "TowersFull", towers->id(), { dimmer(255, "Full") });
    Collection *one = collection(m_doc, "0.0.1-One", { sA->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(one->id(), 0, Function::infiniteSpeed(), 0));

    Show *show = new Show(m_doc);
    show->setName("Timeline");
    QVERIFY(m_doc->addFunction(show));
    Track *track = new Track(Function::invalidId(), show);
    track->setName("Cues");
    QVERIFY(show->addTrack(track));
    ShowFunction *sf = track->createShowFunction(chaser->id());
    sf->setStartTime(2000);
    sf->setDuration(5000);
    const quint32 sfId = sf->id();

    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importInPlace(chaser->id());
    QVERIFY(rep.list != invalidId());
    const quint32 listChaser = st->cueList(rep.list)->chaser;
    QVERIFY(listChaser != Function::invalidId());
    QVERIFY(listChaser != chaser->id());
    QCOMPARE(rep.timelines, QStringList() << "Timeline");
    QCOMPARE(st->lastTimelines(), QList<quint32>() << show->id());

    auto item = [&]() { return show->showFunction(sfId); };
    QVERIFY(item() != nullptr);
    QCOMPARE(item()->functionID(), listChaser);
    QCOMPARE(item()->startTime(), quint32(2000));
    QCOMPARE(item()->duration(), quint32(5000));

    // Undo: the timeline plays the chaser again; Redo: the list.
    QVERIFY(st->undo());
    QCOMPARE(item()->functionID(), chaser->id());
    QVERIFY(st->redo());
    QCOMPARE(item()->functionID(), st->cueList(rep.list)->chaser);

    // Restore original: the chaser again, and its Undo the list.
    QVERIFY(st->restoreOriginal(rep.list));
    QCOMPARE(item()->functionID(), chaser->id());
    QCOMPARE(item()->startTime(), quint32(2000));
    QVERIFY(st->undo());
    QCOMPARE(item()->functionID(), st->cueList(rep.list)->chaser);
}

void ShowTree_Test::theBridgeLinkSavesAndAnOlderReaderSkipsIt()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    Scene *sA = groupScene(m_doc, "TowersFull", towers->id(), { dimmer(255, "Full") });
    Collection *one = collection(m_doc, "0.0.1-One", { sA->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(one->id(), 0, Function::infiniteSpeed(), 0));
    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importInPlace(chaser->id());
    const QByteArray xml = engineXml(m_doc);
    QVERIFY(xml.contains(QString("Source=\"%1\"").arg(chaser->id()).toUtf8()));
    QVERIFY(xml.contains("InPlace=\"1\""));
    QVERIFY(xml.contains("<Original ID="));

    {
        Doc *other = new Doc(this);
        QXmlStreamReader r(xml);
        r.readNextStartElement();
        QVERIFY(other->loadXML(r, false));
        const CueList *l = other->showTree()->cueList(rep.list);
        QVERIFY(l != nullptr);
        QCOMPARE(l->source, chaser->id());
        QVERIFY(l->inPlace);
        QCOMPARE(l->originals.count(), st->cueList(rep.list)->originals.count());
        QCOMPARE(other->showTree()->importedInto(chaser->id()), rep.list);
        QVERIFY(other->showTree()->canRestoreOriginal(rep.list));
        delete other;
    }
    {
        // What an older build reads: no Source / InPlace / <Original>.
        QByteArray old = xml;
        old.replace(QString(" Source=\"%1\"").arg(chaser->id()).toUtf8(), "");
        old.replace(" InPlace=\"1\"", "");
        int at;
        while ((at = old.indexOf("<Original ")) >= 0)
            old.remove(at, old.indexOf("/>", at) + 2 - at);
        QVERIFY(!old.contains("Original"));
        Doc *other = new Doc(this);
        QXmlStreamReader r(old);
        r.readNextStartElement();
        QVERIFY(other->loadXML(r, false));
        const CueList *l = other->showTree()->cueList(rep.list);
        QVERIFY(l != nullptr);
        QCOMPARE(l->source, invalidId());
        QVERIFY(!l->inPlace);
        QCOMPARE(other->function(chaser->id())->path(true), QString("Imported/Full Show (originals)"));
        QCOMPARE(qobject_cast<Chaser *>(other->function(l->chaser))->stepsCount(), 1);
        delete other;
    }
}

/* Capture into palettes: a raw scene read as layers of palettes, the same
   value one palette, checked channel for channel in every cue; the scene
   itself is kept. One undo step. */
void ShowTree_Test::captureARawSceneIntoPalettes()
{
    FixtureGroup *pair = group("Pair", { m_fixtures[0], m_fixtures[1] });
    const quint32 full = dimmer(200, "Bright");
    Scene *raw = new Scene(m_doc);
    raw->setName("Wash");
    raw->setValue(m_fixtures[0], 0, 200);
    raw->setValue(m_fixtures[1], 0, 200);
    raw->setValue(m_fixtures[2], 0, 100);
    m_doc->addFunction(raw);
    Collection *one = collection(m_doc, "0.0.1-One", { raw->id() });
    Collection *two = collection(m_doc, "0.0.2-Two", { raw->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Show");
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(one->id(), 0, Function::infiniteSpeed(), 0));
    chaser->addStep(ChaserStep(two->id(), 0, Function::infiniteSpeed(), 0));
    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importChaser(chaser->id());
    QCOMPARE(rep.opaqueLayers, 2);
    const QList<QByteArray> was = playEveryStep(chaser);
    const QByteArray sceneXml = [&]() {
        QByteArray x;
        QBuffer b(&x);
        b.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&b);
        raw->saveXML(&w);
        return x;
    }();
    const int palettesBefore = int(m_doc->palettes().count());

    const ShowTreeController::CapturePlan plan = st->planCapture(raw->id());
    QVERIFY2(plan.ok, qPrintable(plan.why));
    QCOMPARE(plan.uses.count(), 1);         // set in 0.0.1, tracked into 0.0.2
    QCOMPARE(plan.capturedChannels, 3);
    QVERIFY(plan.leftovers.isEmpty());
    QCOMPARE(plan.layers.count(), 2);
    QCOMPARE(plan.layers[0].target, groupTarget(pair->id()));   // exactly the group's fixtures
    QCOMPARE(plan.layers[1].target.fixtures, QList<quint32>({ m_fixtures[2] }));
    QCOMPARE(plan.palettes.count(), 2);
    QCOMPARE(plan.palettes[0].reuse, full);                       // 200: the show's own "Bright"
    QCOMPARE(plan.palettes[1].reuse, QLCPalette::invalidId());    // 100: made
    QCOMPARE(int(m_doc->palettes().count()), palettesBefore);    // a plan changes nothing

    ShowTreeController::CapturePlan done;
    QVERIFY(st->captureIntoPalettes(raw->id(), &done));
    QCOMPARE(done.captured, 1);             // its one use: 0.0.2 tracks it in
    QVERIFY(done.kept.isEmpty());
    QCOMPARE(done.made.count(), 1);
    QCOMPARE(done.rest, invalidId());
    QCOMPARE(int(m_doc->palettes().count()), palettesBefore + 1);
    const CueList *list = st->cueList(rep.list);
    QCOMPARE(list->cues[1].layers.count(), 0);
    for (const Cue &c : list->cues.mid(0, 1))
    {
        QCOMPARE(c.layers.count(), 2);
        for (const Layer &l : c.layers)
            QVERIFY(!l.target.isOpaque());
    }
    QVERIFY(!st->isInlinePalette(done.made.first()));
    QCOMPARE(playEveryStep(qobject_cast<Chaser *>(m_doc->function(list->chaser))), was);
    // The scene itself is kept as it was.
    QByteArray x;
    QBuffer b(&x);
    b.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&b);
    raw->saveXML(&w);
    QCOMPARE(x, sceneXml);

    QVERIFY(st->undo());
    QCOMPARE(int(m_doc->palettes().count()), palettesBefore);
    QVERIFY(st->cueList(rep.list)->cues[0].layers[0].target.isOpaque());
    QVERIFY(st->redo());
    QVERIFY(!st->cueList(rep.list)->cues[0].layers[0].target.isOpaque());
    QCOMPARE(int(m_doc->palettes().count()), palettesBefore + 1);

    // Not a raw scene, or not in the tree: nothing to capture.
    QVERIFY(!st->planCapture(chaser->id()).ok);
    Scene *loose = new Scene(m_doc);
    loose->setValue(m_fixtures[3], 0, 10);
    m_doc->addFunction(loose);
    QVERIFY(!st->planCapture(loose->id()).ok);
}

/* W5 end to end on a real show: every chaser the converter would offer
   imported in place, played against the chaser; every raw scene the lists
   play as they are captured into palettes, played again; then Restore
   original on each, and the show is XML for XML what it was. */
ShowTree_Test::Bridge ShowTree_Test::bridgeAShow(const QString &path, bool cueListsOnly)
{
    Bridge out;
    QTemporaryDir home;
    HomeAway away(home.path());
    DryRun run;
    if (!loadShowFile(path, run))
    {
        out.problems << run.problems;
        out.problems << QString("can't load %1").arg(path);
        return out;
    }
    ShowTreeController *st = m_doc->showTree();
    auto withoutTree = [](QByteArray xml) {
        const int a = xml.indexOf("<ShowTree");
        if (a >= 0)
        {
            int b = xml.indexOf("</ShowTree>", a);
            b = b < 0 ? xml.indexOf("/>", a) + 2 : b + 11;
            xml.remove(a, b - a);
        }
        return xml;
    };
    const QByteArray before = withoutTree(engineXml(m_doc));
    auto differ = [](const QList<QByteArray> &a, const QList<QByteArray> &b) {
        int n = 0;
        for (int i = 0; i < qMax(a.count(), b.count()); ++i)
        {
            const QByteArray x = a.value(i), y = b.value(i);
            for (int k = 0; k < qMax(x.size(), y.size()); ++k)
                if (k >= x.size() || k >= y.size() || x.at(k) != y.at(k))
                    ++n;
        }
        return n;
    };

    for (quint32 id : st->convertibleChasers())
    {
        const ShowTreeController::ChaserClass cls = st->classifyChaser(id);
        if (cueListsOnly && cls.kind != ShowTreeController::ChaserKind::CueList)
            continue;
        Chaser *original = qobject_cast<Chaser *>(m_doc->function(id));
        const QString name = original->name();
        const ShowTreeController::ImportReport rep = st->importInPlace(id);
        const CueList *list = st->cueList(rep.list);
        if (list == nullptr)
        {
            out.problems << name + ": not imported";
            continue;
        }
        ++out.lists;
        out.cues += rep.cues;
        out.opaqueBefore += rep.opaqueLayers + rep.dropped;
        out.opaqueAfter += rep.opaqueLayers;
        out.filed += rep.filed;
        const QList<QByteArray> a = playEveryStep(original);
        const QList<QByteArray> b = playEveryStep(qobject_cast<Chaser *>(m_doc->function(list->chaser)));
        const int d = differ(a, b);
        out.differImported += d;
        qInfo().noquote() << QString("BRIDGE \"%1\" (%2): %3 cues, %4 layers, %5 opaque (+%6 dropped, %7 "
                                     "flattened), %8 filed, check %9, played %10 channels differ")
                             .arg(name, cls.kind == ShowTreeController::ChaserKind::CueList ? "cue list" : "effect")
                             .arg(rep.cues).arg(rep.layers).arg(rep.opaqueLayers).arg(rep.dropped)
                             .arg(rep.flattened).arg(rep.filed).arg(rep.differChannels).arg(d);

        // Every raw scene the list plays as it is, captured.
        QList<quint32> raws;
        for (const Cue &c : list->cues)
            for (const Layer &l : c.layers)
                if (l.target.isOpaque() && !raws.contains(l.target.function))
                    raws << l.target.function;
        for (quint32 fid : raws)
        {
            ShowTreeController::CapturePlan plan;
            const bool ok = st->captureIntoPalettes(fid, &plan);
            const QString fname = m_doc->function(fid) ? m_doc->function(fid)->name() : QString::number(fid);
            qInfo().noquote() << QString("  capture \"%1\": %2 -- %3 layer(s), %4 channel(s) in palettes, %5 kept "
                                         "raw, %6 use(s) captured, %7 kept")
                                 .arg(fname, ok ? "captured" : "not captured: " + plan.why)
                                 .arg(plan.layers.count()).arg(plan.capturedChannels).arg(plan.leftovers.count())
                                 .arg(plan.captured).arg(plan.kept.count());
            for (const QString &k : plan.kept)
                qInfo().noquote() << "    kept:" << k;
            for (const QString &k : plan.leftoverLines)
                qInfo().noquote() << "    raw:" << k;
            if (ok)
            {
                ++out.scenesCaptured;
                out.captured += plan.captured;
            }
        }
        list = st->cueList(rep.list);
        const QList<QByteArray> c = playEveryStep(qobject_cast<Chaser *>(m_doc->function(list->chaser)));
        const int dc = differ(a, c);
        out.differCaptured += dc;
        qInfo().noquote() << QString("  after capture: played %1 channels differ").arg(dc);

        // Back: the captures undone, then Restore original -- the show as it
        // was before this chaser's import (each chaser from the original).
        while (st->canUndo() && st->undoText().startsWith("Capture"))
            st->undo();
        QString why;
        if (!st->restoreOriginal(rep.list, &why))
            out.problems << name + ": restore: " + why;
        const QString d2 = firstDifference(before, withoutTree(engineXml(m_doc)));
        if (!d2.isEmpty())
            out.problems << name + ": restored differs at " + d2;
    }

    out.restoredEqual = withoutTree(engineXml(m_doc)) == before;
    return out;
}

void ShowTree_Test::bridgeDanceHysteria()
{
    const Bridge b = bridgeAShow(QStringLiteral(DH_SHOW), true);
    QVERIFY2(b.problems.isEmpty(), qPrintable(b.problems.join(" | ")));
    qInfo().noquote() << QString("DANCEHYSTERIA: %1 lists, %2 cues, opaque %3 -> %4, %5 filed, %6 raw scenes "
                                 "captured in %7 cues; differ imported %8, captured %9; restored equal %10")
                         .arg(b.lists).arg(b.cues).arg(b.opaqueBefore).arg(b.opaqueAfter).arg(b.filed)
                         .arg(b.scenesCaptured).arg(b.captured).arg(b.differImported).arg(b.differCaptured)
                         .arg(b.restoredEqual);
    QCOMPARE(b.lists, 2);               // Full Show, New Chaser 8
    QCOMPARE(b.differImported, 0);
    QCOMPARE(b.differCaptured, 0);
    QVERIFY(b.scenesCaptured > 0);
    QVERIFY(b.restoredEqual);
}

/* QLC_BRIDGE_SAVE=<file>: DanceHysteria with Full Show imported in place
   and its raw scenes captured, saved there -- for an older build to open
   (its importARealShow, QLC_SHOWTREE_IMPORT=<file>). Skipped without it. */
void ShowTree_Test::saveABridgedShow()
{
    const QString path = qEnvironmentVariable("QLC_BRIDGE_SAVE");
    if (path.isEmpty())
        QSKIP("set QLC_BRIDGE_SAVE to a file to save DanceHysteria imported in place");
    QTemporaryDir home;
    HomeAway away(home.path());
    DryRun run;
    QVERIFY(loadShowFile(QStringLiteral(DH_SHOW), run));
    ShowTreeController *st = m_doc->showTree();
    quint32 full = Function::invalidId();
    for (Function *f : m_doc->functions())
        if (f->name() == "Full Show")
            full = f->id();
    const ShowTreeController::ImportReport rep = st->importInPlace(full);
    QList<quint32> raws;
    for (const Cue &c : st->cueList(rep.list)->cues)
        for (const Layer &l : c.layers)
            if (l.target.isOpaque() && !raws.contains(l.target.function))
                raws << l.target.function;
    for (quint32 fid : raws)
        QVERIFY(st->captureIntoPalettes(fid));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(engineXml(m_doc));
    f.close();
    qInfo().noquote() << "saved" << path;
}

void ShowTree_Test::bridgeTheSampleShow()
{
    // No cue lists: the converter offers none of its 13 looping chasers.
    {
        QTemporaryDir home;
        HomeAway away(home.path());
        DryRun run;
        QVERIFY(loadShowFile(QStringLiteral(SAMPLE_SHOW), run));
        int effects = 0;
        for (quint32 id : m_doc->showTree()->convertibleChasers())
            if (m_doc->showTree()->classifyChaser(id).kind == ShowTreeController::ChaserKind::Effect)
                ++effects;
        QCOMPARE(effects, 13);
    }
    // The mechanism on all of them anyway (Import chaser… still takes any).
    const Bridge b = bridgeAShow(QStringLiteral(SAMPLE_SHOW), false);
    QVERIFY2(b.problems.isEmpty(), qPrintable(b.problems.join(" | ")));
    qInfo().noquote() << QString("SAMPLE: %1 lists, %2 cues, opaque %3 -> %4, %5 filed, %6 raw scenes "
                                 "captured in %7 cues; differ imported %8, captured %9; restored equal %10")
                         .arg(b.lists).arg(b.cues).arg(b.opaqueBefore).arg(b.opaqueAfter).arg(b.filed)
                         .arg(b.scenesCaptured).arg(b.captured).arg(b.differImported).arg(b.differCaptured)
                         .arg(b.restoredEqual);
    QCOMPARE(b.lists, 13);
    QCOMPARE(b.differImported, 0);
    QCOMPARE(b.differCaptured, 0);
    QVERIFY(b.restoredEqual);
}

/* Upgrade this show (§3.7): the converter's imports and the captures, as
   many steps as they take, grouped into one; one undo puts the whole show
   back XML for XML, redo brings the upgrade back as it was. On a copy. */
QStringList ShowTree_Test::upgradeAsOneStep(const QString &path, bool cueListsOnly, int *steps)
{
    QStringList problems;
    QTemporaryDir home;
    HomeAway away(home.path());
    QTemporaryDir dir;
    const QString copy = dir.filePath(QFileInfo(path).fileName());
    if (!QFile::copy(path, copy))
        return QStringList(QString("can't copy %1").arg(path));
    DryRun run;
    if (!loadShowFile(copy, run))
        return run.problems << QString("can't load %1").arg(copy);
    ShowTreeController *st = m_doc->showTree();
    auto withoutTree = [](QByteArray xml) {
        const int a = xml.indexOf("<ShowTree");
        if (a >= 0)
        {
            int b = xml.indexOf("</ShowTree>", a);
            b = b < 0 ? xml.indexOf("/>", a) + 2 : b + 11;
            xml.remove(a, b - a);
        }
        return xml;
    };
    const QByteArray before = withoutTree(engineXml(m_doc));
    const quint64 since = st->undoStamp();
    *steps = 0;
    for (quint32 id : st->convertibleChasers())
    {
        if (cueListsOnly && st->classifyChaser(id).kind != ShowTreeController::ChaserKind::CueList)
            continue;
        const ShowTreeController::ImportReport rep = st->importInPlace(id);
        const CueList *list = st->cueList(rep.list);
        if (list == nullptr)
        {
            problems << QString("%1 not imported").arg(id);
            continue;
        }
        ++*steps;
        QList<quint32> raws;
        for (const Cue &c : list->cues)
            for (const Layer &l : c.layers)
                if (l.target.isOpaque() && !raws.contains(l.target.function))
                    raws << l.target.function;
        for (quint32 fid : raws)
            if (st->captureIntoPalettes(fid))
                ++*steps;
    }
    // Redo bakes the generated functions again, with new ids: what the show
    // holds is compared, not the ids.
    auto shape = [this, st]() {
        QStringList out;
        for (const Function *f : m_doc->functions())
            out << QString("%1|%2|%3").arg(f->type()).arg(f->name(), f->path(true));
        out << QString("palettes %1").arg(m_doc->palettes().count());
        for (const Production &p : st->productions())
            for (const CueList &l : p.lists)
            {
                int layers = 0;
                for (const Cue &c : l.cues)
                {
                    layers += c.layers.count();
                    for (const Layer &layer : c.layers)
                        if (layer.target.isOpaque())
                        {
                            const Function *f = m_doc->function(layer.target.function);
                            out << QString("%1 %2 plays %3").arg(l.name, c.name,
                                                                 f == nullptr ? QString("(missing)") : f->name());
                        }
                }
                out << QString("list %1: %2 cues, %3 layers, in place %4").arg(l.name).arg(l.cues.count())
                       .arg(layers).arg(l.inPlace);
            }
        out.sort();
        return out;
    };
    const QStringList after = shape();
    if (!st->groupStepsSince(since, "Upgrade this show"))
        return problems << "nothing to group";
    if (st->undoText() != "Upgrade this show")
        problems << "undo text: " + st->undoText();

    if (!st->undo())
        problems << "undo failed";
    if (st->undoStamp() > since)
        problems << "a step of the upgrade is left on the stack";
    QString d = firstDifference(before, withoutTree(engineXml(m_doc)));
    if (!d.isEmpty())
        problems << "undone differs at " + d;
    if (!st->productions().isEmpty())
        problems << "undone, the tree is not empty";
    if (!st->redo())
        problems << "redo failed";
    const QStringList redone = shape();
    if (redone != after)
        for (int i = 0; i < qMax(after.count(), redone.count()); ++i)
            if (after.value(i) != redone.value(i))
            {
                problems << QString("redone differs: %1 | %2").arg(after.value(i), redone.value(i));
                break;
            }
    if (!st->undo())
        problems << "second undo failed";
    d = firstDifference(before, withoutTree(engineXml(m_doc)));
    if (!d.isEmpty())
        problems << "undone again differs at " + d;
    // And once more: redo twice is the same as once.
    if (!st->redo())
        problems << "second redo failed";
    const QStringList twice = shape();
    for (int i = 0; i < qMax(after.count(), twice.count()); ++i)
        if (after.value(i) != twice.value(i))
        {
            problems << QString("redone twice differs: %1 | %2").arg(after.value(i), twice.value(i));
            break;
        }
    if (!st->undo() || withoutTree(engineXml(m_doc)) != before)
        problems << "undone a third time differs";
    return problems;
}

void ShowTree_Test::upgradeAShowIsOneUndoStep()
{
    int steps = 0;
    const QStringList problems = upgradeAsOneStep(QStringLiteral(DH_SHOW), true, &steps);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(" | ")));
    qInfo().noquote() << QString("UPGRADE DANCEHYSTERIA: %1 steps as one").arg(steps);
    QVERIFY(steps > 2);
}

void ShowTree_Test::upgradeTheSampleShowIsOneUndoStep()
{
    int steps = 0;
    const QStringList problems = upgradeAsOneStep(QStringLiteral(SAMPLE_SHOW), false, &steps);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(" | ")));
    qInfo().noquote() << QString("UPGRADE SAMPLE: %1 steps as one").arg(steps);
    QVERIFY(steps >= 13);
}

/* A dry run on a real show: QLC_SHOWTREE_IMPORT=<a COPY of a .qxw/.qlcc>.
   The show is opened as App opens it (its embedded definitions first, the
   user folder kept out); every chaser in it is imported (in memory only --
   nothing is saved), the report printed (layers vs opaque layers, HTP
   conflicts, cues made to mix, generated scenes), and the imported and the
   converted lists played against the chaser: it fails unless 0 channels
   differ in every cue. Skipped without the variable. */
void ShowTree_Test::importARealShow()
{
    const QString path = qEnvironmentVariable("QLC_SHOWTREE_IMPORT");
    if (path.isEmpty())
        QSKIP("set QLC_SHOWTREE_IMPORT to a copy of a show to dry-run the import");
    const DryRun run = dryRunImport(path);
    QVERIFY2(run.loaded, qPrintable(run.problems.join(" | ")));
    qInfo().noquote() << QString("DRY RUN: %1 cues in %2 lists, %3 channels differ imported vs chaser, "
                                 "%4 converted vs imported; %5 definitions from the show")
                         .arg(run.cues).arg(run.lists).arg(run.differ).arg(run.convertedDiffer)
                         .arg(run.showDefinitions);
    QCOMPARE(run.differ, 0);
    QCOMPARE(run.staticDiffer, 0);
    QCOMPARE(run.convertedDiffer, 0);
}

/* The dry run as App opens a show: the show's own definitions first (its
   <Embedded> copies, then its folder's), then its contents -- with this
   computer's user folder out of the way (HOME pointed at an empty folder),
   so it compares the same on any machine, with or without the show's
   author's fixture files. */
ShowTree_Test::DryRun ShowTree_Test::dryRunImport(const QString &path)
{
    DryRun run;
    QTemporaryDir home;
    HomeAway away(home.path());
    if (!loadShowFile(path, run))
        return run;
    return dryRunLoaded(run);
}

ShowTree_Test::HomeAway::HomeAway(const QString &path)
    : home(qgetenv("HOME")), profile(qgetenv("USERPROFILE"))
{
    qputenv("HOME", path.toUtf8());
    qputenv("USERPROFILE", path.toUtf8());
}

ShowTree_Test::HomeAway::~HomeAway()
{
    qputenv("HOME", home);
    if (profile.isEmpty())
        qunsetenv("USERPROFILE");
    else
        qputenv("USERPROFILE", profile);
}

bool ShowTree_Test::loadShowFile(const QString &path, DryRun &run)
{
    stopAll();
    m_doc->clearContents();
    m_fixtures.clear();
    const QString folder = QFileInfo(path).absolutePath();
    m_doc->setWorkspacePath(folder);
    if (m_doc->showEmbedding()->readWorkspace(path))
        m_doc->showEmbedding()->apply(folder);
    for (const ShowEmbedding::Item &it : m_doc->showEmbedding()->items())
        if (it.kind == ShowEmbedding::FixtureDefinition && it.usingShow)
            ++run.showDefinitions;
    run.problems << m_doc->showEmbedding()->problems();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        run.problems << QString("can't read %1").arg(path);
        return false;
    }
    QXmlStreamReader r(&file);
    while (!r.atEnd() && !run.loaded)
    {
        if (r.readNextStartElement() && r.name() == QLatin1String("Engine"))
            run.loaded = m_doc->loadXML(r, false);
    }
    file.close();
    return run.loaded;
}

ShowTree_Test::DryRun ShowTree_Test::dryRunLoaded(DryRun run)
{
    QList<quint32> chasers;
    for (Function *f : m_doc->functions())
        if (f->type() == Function::ChaserType)
            chasers << f->id();

    std::sort(chasers.begin(), chasers.end());
    for (quint32 id : chasers)
    {
        const QString name = m_doc->function(id)->name();
        const ShowTreeController::ImportReport rep = m_doc->showTree()->importChaser(id);
        ++run.lists;
        run.mixedIncludes << rep.mixedIncludes;
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
        qInfo().noquote() << QString("  tracking: %1 cues track, %2 layers and %3 includes not repeated, "
                                     "%4 releases").arg(rep.trackingCues).arg(rep.trackedLayers)
                             .arg(rep.trackedIncludes).arg(rep.trackReleases);
        for (const QString &s : rep.trackNotes)
            qInfo().noquote() << "  track:" << s;
        qInfo().noquote() << QString("  same-output check: %1 cues, %2 channels differ")
                             .arg(rep.checkedCues).arg(rep.differChannels);
        for (const QString &s : rep.differCues)
            qInfo().noquote() << "  differs:" << s;
        run.staticDiffer += rep.differChannels;
        const CueList *list = m_doc->showTree()->cueList(rep.list);
        if (list == nullptr)
            continue;
        for (int i = 0; i < list->cues.count(); ++i)
        {
            qInfo().noquote() << "  cue" << m_doc->showTree()->cueLabel(*list, i)
                              << "layers" << list->cues[i].layers.count()
                              << "includes" << list->cues[i].includes.count();
            if (qEnvironmentVariableIsSet("QLC_SHOWTREE_IMPORT_VERBOSE"))
            {
                const Collection *coll = qobject_cast<const Collection *>(m_doc->function(list->cues[i].collection));
                for (quint32 fid : coll->functions())
                {
                    const Scene *sc = qobject_cast<const Scene *>(m_doc->function(fid));
                    QStringList pals;
                    if (sc != nullptr)
                        for (quint32 pid : sc->palettes())
                            pals << m_doc->palette(pid)->name() + (sc->excludedFixtures(pid).isEmpty() ? QString()
                                    : QString(" (minus %1)").arg(sc->excludedFixtures(pid).count()));
                    qInfo().noquote() << "    plays" << m_doc->function(fid)->name() << pals.join(", ");
                }
            }
        }

        // Play both, cue by cue, and compare every channel of every universe
        // (static looks only: effects run on the effect runner's own timer).
        // Each from the idle rig: universes cleared, then every fixture's
        // channel defaults (a mover's Pan 128) and modifiers put back, as
        // patching does -- clearing alone would leave the first run with
        // the defaults and the next without.
        auto playAll = [this](Chaser *c) { return playEveryStep(c); };
        Chaser *original = qobject_cast<Chaser *>(m_doc->function(id));
        Chaser *imported = qobject_cast<Chaser *>(m_doc->function(list->chaser));
        if (original == nullptr || imported == nullptr)
        {
            run.problems << QString("%1: no imported chaser").arg(name);
            run.differ = qMax(run.differ, 1);
            continue;
        }
        const QList<QByteArray> a = playAll(original);
        const QList<QByteArray> b = playAll(imported);
        if (a.count() != b.count())
        {
            run.problems << QString("%1: %2 steps, the list has %3").arg(name).arg(a.count()).arg(b.count());
            run.differ = qMax(run.differ, 1);
            continue;
        }
        for (int i = 0; i < a.count(); ++i)
        {
            int lit = 0, differ = 0;
            QStringList first;
            for (int k = 0; k < a[i].size(); ++k)
            {
                if (a[i].at(k) != 0)
                    ++lit;
                if (k >= b[i].size() || a[i].at(k) != b[i].at(k))
                {
                    ++differ;
                    if (first.count() < 8)
                        first << QString("u%1/%2: %3 vs %4").arg(k / 512 + 1).arg(k % 512 + 1)
                                 .arg(uchar(a[i].at(k))).arg(k < b[i].size() ? uchar(b[i].at(k)) : -1);
                }
            }
            if (differ > 0)
                qInfo().noquote() << "    first differences (chaser vs list):" << first.join(", ");
            qInfo().noquote() << QString("  play cue %1: %2 channels lit by the chaser, %3 differ in the cue list")
                                 .arg(m_doc->showTree()->cueLabel(*list, i)).arg(lit).arg(differ);
            run.played << DryRun::Played { name, m_doc->showTree()->cueLabel(*list, i), lit, differ };
            ++run.cues;
            run.differ += differ;
        }

        // Convert to tracking (P4) and play it again: the converted list
        // against the imported one, every channel of every universe.
        ShowTreeController::ConvertPlan plan;
        const quint32 listId = rep.list;
        if (!m_doc->showTree()->convertToTracking(listId, &plan))
        {
            qInfo().noquote() << "  convert: not converted --" << plan.why;
            continue;
        }
        qInfo().noquote() << QString("  convert: %1 cues track, %2 stay Block, %3 values and %4 layers taken out, "
                                     "%5 includes made tracking, %6 releases")
                             .arg(plan.tracking).arg(plan.blocks).arg(plan.looksRemoved).arg(plan.layersRemoved)
                             .arg(plan.includesRemoved).arg(plan.releases);
        for (const QString &line : plan.changes)
            qInfo().noquote() << "    " << line;
        const CueList *conv = m_doc->showTree()->cueList(listId);
        Chaser *converted = qobject_cast<Chaser *>(m_doc->function(conv->chaser));
        if (converted == nullptr)
        {
            run.problems << QString("%1: no converted chaser").arg(name);
            run.convertedDiffer = qMax(run.convertedDiffer, 1);
            continue;
        }
        const QList<QByteArray> c = playAll(converted);
        if (c.count() != b.count())
        {
            run.problems << QString("%1: the converted list has %2 cues").arg(name).arg(c.count());
            run.convertedDiffer = qMax(run.convertedDiffer, 1);
            continue;
        }
        int total = 0;
        for (int i = 0; i < c.count(); ++i)
        {
            int differ = 0;
            for (int k = 0; k < qMax(b[i].size(), c[i].size()); ++k)
                if (k >= b[i].size() || k >= c[i].size() || b[i].at(k) != c[i].at(k))
                    ++differ;
            total += differ;
            qInfo().noquote() << QString("  converted cue %1: %2 channels differ from the imported list")
                                 .arg(m_doc->showTree()->cueLabel(*conv, i)).arg(differ);
        }
        run.convertedDiffer += total;
    }
    return run;
}


/* Every step of @a c played from the idle rig, each to rest: every universe's
   channels, step by step (static looks only: effects run on the effect
   runner's own timer). */
QList<QByteArray> ShowTree_Test::playEveryStep(Chaser *c)
{
    auto idle = [this]() {
        m_doc->inputOutputMap()->resetUniverses();
        for (Fixture *fx : m_doc->fixtures())
            m_doc->updateFixtureChannelCapabilities(fx->id(), fx->forcedHTPChannels(),
                                                    fx->forcedLTPChannels());
    };
    idle();
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
    idle();
    return states;
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

    // As imported it plays as the chaser did, every cue: where an include
    // at full and the cue's own lower level fought (0.0.5, 0.0.6, 0.0.7,
    // 0.0.9) the cue mixes, so the brighter still wins.
    QList<QHash<quint32, uchar>> v = playAll(list);
    for (int i = 0; i < 7; ++i)
        for (quint32 ch = 0; ch < 5; ++ch)
            QCOMPARE(int(v[i][ch]), int(original[i][ch]));
    QCOMPARE(rep.mixedIncludes.count(), 4);
    for (int i : { 3, 4, 5, 6 })
        QVERIFY(st->cue(c[i])->mixOverlap);
    QCOMPARE(int(v[3][3]), 255);

    // On the tree's rule instead (the cue page's Overlap box): the later
    // one -- the cue's own -- wins.
    for (int i : { 3, 4, 5, 6 })
        QVERIFY(st->setCueMixOverlap(c[i], false));
    v = playAll(list);
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
    QCOMPARE(m_doc->palette(d45)->path(), QString());     // unfiled: folders hold any type
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
    // Reusables (P4): a layer "BackStage" in the Library (its use in cue
    // One), a cue "Walk-in" (no use).
    st->makeLayerReusable(ctx.cue, st->addLayer(ctx.cue, groupTarget(m_doc->fixtureGroups().at(1)->id())), "BackStage");
    const quint32 tmp = st->addCue(ctx.list, 2, "Walk-in");
    st->makeCueReusable(tmp);
    st->removeCue(tmp);
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
    // No siblings: plain names, unfiled (a folder named after the type
    // would be a folder of its own: folders hold any type).
    QCOMPARE(st->suggestPaletteName(QLCPalette::Dimmer, { 77 }), QString("Dimmer 30%"));
    QCOMPARE(st->suggestPaletteFolder(QLCPalette::Dimmer), QString());
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

/****************************************************************************
 * P3: group sets, child layers and overlap (§1, §3.5)
 ****************************************************************************/

/* A group set is one target of all its members (one scene, each member a
   group of its own, so it keeps its head layout); the set is the layer's
   identity, so changing its members keeps the layer (and what tracks). */
void ShowTree_Test::aTargetIsNamedByItsWholeGroups()
{
    ShowTreeController *st = m_doc->showTree();
    group("FOH", { m_fixtures[0], m_fixtures[1] });
    group("Pair", { m_fixtures[2], m_fixtures[3] });
    Target t;
    t.fixtures = { m_fixtures[0], m_fixtures[1], m_fixtures[2] };
    // FOH whole, then what is left by name; "Pair" isn't all there.
    QCOMPARE(st->targetLabel(t), QString("FOH + %1").arg(m_doc->fixture(m_fixtures[2])->name()));
    t.fixtures << m_fixtures[3];
    QCOMPARE(st->targetLabel(t), QString("FOH + Pair"));
}

void ShowTree_Test::groupSetsAreOneTargetAndKeepTracking()
{
    FixtureGroup *sl = group("SL", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *sr = group("SR", { m_fixtures[2], m_fixtures[3] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();

    const quint32 towers = st->addGroupSet("Towers", { sl->id(), sr->id() });
    QVERIFY(towers != invalidId());
    QCOMPARE(st->groupSet(towers)->name, QString("Towers"));
    QVERIFY(st->canUndo());
    // A second of that name is told apart.
    const quint32 twin = st->addGroupSet("towers", { sl->id() });
    QCOMPARE(st->groupSet(twin)->name, QString("towers (2)"));
    QVERIFY(st->undo());
    QVERIFY(st->groupSet(twin) == nullptr);

    const quint32 listId = st->addCueList(st->addProduction("P"), "L");      // Tracking
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    Target t;
    t.sets << towers;
    QCOMPARE(st->targetLabel(t), QString("Towers"));
    QCOMPARE(st->targetFixtures(t), QList<quint32>({ m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[3] }));
    const quint32 l1 = st->addLayer(c1, t);
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(full)));

    // One scene, on both member groups.
    const Cue *one = st->cue(c1);
    const Collection *coll = qobject_cast<const Collection *>(m_doc->function(one->collection));
    QCOMPARE(coll->functions().count(), 1);
    const Scene *scene = qobject_cast<const Scene *>(m_doc->function(coll->functions().first()));
    QCOMPARE(scene->fixtureGroups(), QList<quint32>({ sl->id(), sr->id() }));

    // Tracked into cue Two as the same layer.
    const QList<ResolvedCue> r = st->resolved(*st->cueList(listId));
    QCOMPARE(int(r.at(1).layers.first().origin), int(Origin::Tracked));
    QVERIFY(r.at(1).layers.first().target == t);

    // Members changed: still the same layer, tracked; the rig follows.
    st->previewCue(c2);
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[2]), 255);
    QVERIFY(st->setGroupSetMembers(towers, { sl->id() }));
    v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[2]), 0);
    QCOMPARE(int(st->resolved(*st->cueList(listId)).at(1).layers.first().origin), int(Origin::Tracked));
    QVERIFY(st->undo());
    QCOMPARE(int(settle()[2]), 255);
    st->stopPreview();

    // In use: not deleted, and says where.
    QString why;
    QVERIFY(!st->removeGroupSet(towers, &why));
    QVERIFY2(why.contains("One"), qPrintable(why));

    // Saved with the show (in the tree), read back.
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter w(&buffer);
    st->saveXML(&w);
    buffer.close();
    QVERIFY(buffer.data().contains("<GroupSet"));
    QVERIFY(buffer.data().contains("Sets=\""));
    buffer.open(QIODevice::ReadOnly);
    QXmlStreamReader rd(&buffer);
    rd.readNextStartElement();
    QVERIFY(st->loadXML(rd));
    QCOMPARE(st->groupSets().count(), 1);
    QCOMPARE(st->groupSet(towers)->groups, QList<quint32>({ sl->id(), sr->id() }));
    QVERIFY(st->cue(c1)->layers.first().target == t);

    // Unused: deleted, one undo step.
    QVERIFY(st->removeLayer(c1, l1));
    QVERIFY(st->removeGroupSet(towers, &why));
    QVERIFY(st->groupSet(towers) == nullptr);
    QVERIFY(st->undo());
    QVERIFY(st->groupSet(towers) != nullptr);
}

/* §3.5: "Steps: White, Base with a child US: Red. US gets Red and inherits
   Base." A child sets only what it sets; its parent lights the rest of it
   and doesn't light what it sets (no HTP show-through). */
void ShowTree_Test::childLayersExcludeTheirFixturesFromTheParent()
{
    FixtureGroup *steps = group("Steps", { m_fixtures[0], m_fixtures[1], m_fixtures[2] });
    FixtureGroup *us = group("US", { m_fixtures[1] });
    FixtureGroup *other = group("Other", { m_fixtures[5] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    const quint32 d45 = dimmer(115, "45");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");      // Tracking
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    const quint32 parent = st->addLayer(c1, groupTarget(steps->id()));
    st->setLayerLook(c1, parent, st->lookForPalette(full));

    // Not part of the parent: refused, with why.
    QString why;
    QCOMPARE(st->addChildLayer(c1, parent, groupTarget(other->id()), &why), invalidId());
    QVERIFY2(why.contains("isn't part of"), qPrintable(why));
    QCOMPARE(st->addChildLayer(c1, parent, groupTarget(steps->id()), &why), invalidId());

    const quint32 child = st->addChildLayer(c1, parent, groupTarget(us->id()), &why);
    QVERIFY2(child != invalidId(), qPrintable(why));
    QCOMPARE(st->childLayers(c1, parent), QList<quint32>({ child }));
    QCOMPARE(st->cue(c1)->layers.at(1).parent, parent);

    // With nothing of its own the child changes nothing.
    st->previewCue(c1);
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[1]), 255);

    // Its own level: the parent's full doesn't show through.
    QVERIFY(st->setLayerLook(c1, child, st->lookForPalette(half)));
    v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[1]), 127);
    QCOMPARE(int(v[2]), 255);
    // The parent's scene leaves US out, for its Dimmer only.
    const Collection *coll = qobject_cast<const Collection *>(m_doc->function(st->cue(c1)->collection));
    QCOMPARE(coll->functions().count(), 2);
    const Scene *ps = qobject_cast<const Scene *>(m_doc->function(coll->functions().first()));
    QCOMPARE(ps->excludedFixtures(full), QList<quint32>({ m_fixtures[1] }));

    // Off in the child: dark there.
    Look off;
    off.key = KeyDimmer;
    off.none = true;
    QVERIFY(st->setLayerLook(c1, child, off));
    QCOMPARE(int(settle()[1]), 0);
    QVERIFY(st->setLayerLook(c1, child, st->lookForPalette(half)));

    // Tracking: cue Two carries both; cue Three sets the parent's level --
    // this cue beats the earlier child's (D5).
    st->previewCue(c2);
    v = settle();
    QCOMPARE(int(v[1]), 127);
    QCOMPARE(int(v[0]), 255);
    const quint32 p3 = st->addLayer(c3, groupTarget(steps->id()));
    QVERIFY(st->setLayerLook(c3, p3, st->lookForPalette(d45)));
    st->previewCue(c3);
    v = settle();
    QCOMPARE(int(v[0]), 115);
    QCOMPARE(int(v[1]), 115);
    // The child, carried in under its parent, still knows its parent.
    const QList<ResolvedCue> r = st->resolved(*st->cueList(listId));
    const ResolvedLayer *tracked = r.at(2).layer(groupTarget(us->id()));
    QVERIFY(tracked != nullptr && !tracked->own);
    QCOMPARE(tracked->depth, 1);
    QVERIFY(tracked->parent == groupTarget(steps->id()));

    // A child's own child: deeper still wins.
    // Removing the parent takes its children with it, one step.
    QVERIFY(st->removeLayer(c1, parent));
    QVERIFY(st->cue(c1)->layers.isEmpty());
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c1)->layers.count(), 2);

    // The parent can't grow or shrink past its children; split / merge
    // say why.
    QVERIFY(!st->setLayerTarget(c1, parent, groupTarget(other->id())));
    QVERIFY(!st->splitLayer(c1, parent, nullptr, &why));
    QVERIFY2(why.contains("child"), qPrintable(why));

    // Saved and read back with its parent.
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter w(&buffer);
    st->saveXML(&w);
    buffer.close();
    QVERIFY(buffer.data().contains(QString("Parent=\"%1\"").arg(parent).toUtf8()));
    st->stopPreview();
}

/* §2 "Overlap": two layers of one cue on fixtures in common -- the lower
   wins what both set (no HTP show-through), each keeps what only it sets;
   a layer set in this cue wins over one tracked in; a layer's own look over
   a cue look. */
void ShowTree_Test::overlappingLayersLowerWins()
{
    FixtureGroup *all = group("All", { m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[3] });
    FixtureGroup *some = group("Some", { m_fixtures[2], m_fixtures[3] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    const quint32 d45 = dimmer(115, "45");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");      // Tracking
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 a = st->addLayer(c1, groupTarget(all->id()));
    const quint32 b = st->addLayer(c1, groupTarget(some->id()));
    st->setLayerLook(c1, a, st->lookForPalette(full));
    st->setLayerLook(c1, b, st->lookForPalette(half));

    st->previewCue(c1);
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[2]), 127);           // the lower layer, not HTP's 255

    // A cue look reaching the lower layer doesn't beat the upper's own.
    QVERIFY(st->setLayerLook(c1, b, Look { KeyDimmer, {}, {}, false, QString() }));
    QVERIFY(st->setCueLook(c1, st->lookForPalette(d45)));
    v = settle();
    QCOMPARE(int(v[2]), 255);           // All's own Full over the cue's 45% on Some
    QCOMPARE(int(v[0]), 255);

    // Tracked: cue Two sets Some's level; All (tracked) keeps the rest.
    QVERIFY(st->setCueLook(c1, Look { KeyDimmer, {}, {}, false, QString() }));
    QVERIFY(st->setLayerLook(c2, st->addLayer(c2, groupTarget(some->id())), st->lookForPalette(half)));
    st->previewCue(c2);
    v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[2]), 127);
    st->stopPreview();
}

/* Live preview: edits to a child layer bake new scenes (with exclusions)
   while the DMX thread is running them -- no race, and the rig ends up as
   the last edit says. */
void ShowTree_Test::childLayerEditsWhileTheDmxThreadRuns()
{
    FixtureGroup *steps = group("Steps", { m_fixtures[0], m_fixtures[1], m_fixtures[2] });
    FixtureGroup *us = group("US", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    const quint32 low = dimmer(40, "Low");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 parent = st->addLayer(c1, groupTarget(steps->id()));
    st->setLayerLook(c1, parent, st->lookForPalette(full));
    const quint32 child = st->addChildLayer(c1, parent, groupTarget(us->id()));
    st->previewCue(c1);
    tick(3);

    std::atomic<bool> stop(false);
    std::thread dmx([&]() {
        while (!stop.load())
        {
            m_doc->masterTimer()->timerTick();
            QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
            ua.first()->processFaders();
            m_doc->inputOutputMap()->releaseUniverses(false);
            QThread::usleep(200);
        }
    });
    for (int i = 0; i < 60; ++i)
    {
        st->setLayerLook(c1, child, st->lookForPalette(i % 3 == 0 ? half : i % 3 == 1 ? low : full));
        QCoreApplication::processEvents();
        QThread::usleep(500);
    }
    st->setLayerLook(c1, child, st->lookForPalette(low));
    QCoreApplication::processEvents();
    QThread::msleep(50);
    stop.store(true);
    dmx.join();
    QCoreApplication::processEvents();

    const QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[1]), 40);
    st->stopPreview();
}

/* An imported cue whose scenes share fixtures and set the same attribute
   mixes them as the chaser did (highest wins on intensity); switched to the
   tree's rule, the lower layer wins. */
void ShowTree_Test::anImportedOverlapMixesAsTheChaserDid()
{
    FixtureGroup *all = group("All", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *one = group("One", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    Scene *a = new Scene(m_doc);
    a->setName("A");
    a->addFixtureGroup(all->id());
    a->addPalette(full);
    m_doc->addFunction(a);
    Scene *b = new Scene(m_doc);
    b->setName("B");
    b->addFixtureGroup(one->id());
    b->addPalette(half);
    m_doc->addFunction(b);
    Collection *coll = new Collection(m_doc);
    coll->setName("1-Both");
    coll->addFunction(a->id());
    coll->addFunction(b->id());
    m_doc->addFunction(coll);
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("C");
    chaser->addStep(ChaserStep(coll->id()));
    m_doc->addFunction(chaser);

    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importChaser(chaser->id());
    QVERIFY2(rep.notes.join(" ").contains("mix as the chaser did"), qPrintable(rep.notes.join(" | ")));
    const quint32 cueId = st->cueList(rep.list)->cues.first().id;
    QVERIFY(st->cue(cueId)->mixOverlap);
    // Played for real (the Design preview writes its scenes LTP to win over
    // a running list, so it can't show a mix).
    Chaser *list = qobject_cast<Chaser *>(m_doc->function(st->cueList(rep.list)->chaser));
    go(list);
    QCOMPARE(int(settle()[1]), 255);        // highest wins, as the chaser
    stopAll();
    QVERIFY(st->setCueMixOverlap(cueId, false));
    go(list);
    QCOMPARE(int(settle()[1]), 127);        // the lower layer wins
    QCOMPARE(int(settle()[0]), 255);
    stopAll();
    QVERIFY(st->undo());
    QVERIFY(st->cue(cueId)->mixOverlap);
}

/****************************************************************************
 * P3: cue-level effects (D10) and line aims (§3B, D16-D19)
 ****************************************************************************/

namespace
{
/* A two-channel spot (pan, tilt), placed at @a x metres across the stage,
   6 m up. */
Fixture *placedSpot(Doc *doc, const QString &name, double x)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt };
    for (int k = 0; k < 2; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(k == 0 ? "Pan" : "Tilt");
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    head.addChannel(0);
    head.addChannel(1);
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(quint32(100 + doc->fixtures().size() * 2));
    if (!doc->addFixture(fxi))
        return nullptr;
    doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0, QVector3D(float(x * 1000.0), 0, 6));
    return fxi;
}

/* The point a scene aims @a fid at (its own Aim look), or a null vector. */
QVector3D aimedAt(Doc *doc, const Scene *scene, quint32 fid)
{
    for (const AimSolver::SceneAim &a : AimSolver::sceneAims(doc, scene))
        if (a.fixtureId == fid)
            return a.point;
    return QVector3D(-99, -99, -99);
}

/* The generated scene a cue plays for the layer on @a target. */
const Scene *sceneOf(ShowTreeController *st, Doc *doc, quint32 cueId, const Target &target)
{
    return qobject_cast<const Scene *>(doc->function(st->layerScene(cueId, target)));
}
}

/* D10: an effect set on the cue is ONE effect across the cue's own layers
   that don't set that effect kind (or None) themselves: each of their
   scenes plays it with a layout over all of them and writes its own; it
   tracks as one until a later cue changes some of them. */
void ShowTree_Test::aCueEffectIsOneAcrossItsLayers()
{
    FixtureGroup *a = group("A", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *b = group("B", { m_fixtures[2], m_fixtures[3] });
    FixtureGroup *c = group("C", { m_fixtures[4] });
    FixtureGroup *d = group("D", { m_fixtures[5] });
    // A script that lights each cell by its place in the layout: 1/n, 2/n…
    QTemporaryDir dir;
    QFile js(dir.filePath("ramp.js"));
    QVERIFY(js.open(QIODevice::WriteOnly));
    js.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'Ramp';"
             " e.tick = function(fixtures) { var out = [];"
             " for (var i = 0; i < fixtures.length; i++) out.push({ dimmer: (i + 1) / fixtures.length });"
             " return out; }; return e; })()");
    js.close();
    QLCPalette *ramp = new QLCPalette(QLCPalette::Effect, m_doc);
    ramp->setName("Ramp");
    ramp->setScriptPath(js.fileName());
    m_doc->addPalette(ramp);
    QLCPalette *other = new QLCPalette(QLCPalette::Effect, m_doc);
    other->setName("Other");
    other->setScriptPath(js.fileName());
    m_doc->addPalette(other);
    const quint32 full = dimmer(255, "Full");

    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");      // Tracking
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    for (FixtureGroup *g : { a, b, c, d })
        st->setLayerLook(c1, st->addLayer(c1, groupTarget(g->id())), st->lookForPalette(full));
    const Look fx = st->lookForPalette(ramp->id());
    QVERIFY(st->setCueLook(c1, fx));
    // C sets that effect kind itself; D turns it off: both left out.
    const Look own = st->lookForPalette(other->id());
    QCOMPARE(own.key, fx.key);
    QVERIFY(st->setLayerLook(c1, st->cue(c1)->layers.at(2).id, own));
    Look none;
    none.key = fx.key;
    none.none = true;
    QVERIFY(st->setLayerLook(c1, st->cue(c1)->layers.at(3).id, none));

    const Scene *sa = sceneOf(st, m_doc, c1, groupTarget(a->id()));
    const Scene *sb = sceneOf(st, m_doc, c1, groupTarget(b->id()));
    const Scene *sc = sceneOf(st, m_doc, c1, groupTarget(c->id()));
    const Scene *sd = sceneOf(st, m_doc, c1, groupTarget(d->id()));
    QVERIFY(sa && sb && sc && sd);
    QVERIFY(sa != sb);
    // A and B: one effect, laid out over both; C its own, D none.
    QCOMPARE(sa->layoutSpan(ramp->id()).groups, QList<quint32>({ a->id(), b->id() }));
    QCOMPARE(sb->layoutSpan(ramp->id()).groups, QList<quint32>({ a->id(), b->id() }));
    QVERIFY(sc->palettes().contains(other->id()) && sc->layoutSpan(other->id()).isEmpty());
    QVERIFY(!sd->palettes().contains(ramp->id()));
    QCOMPARE(st->spanMembers(c1, groupTarget(a->id()), fx.key).count(), 2);

    // The effect runs as one: each scene lays it out over all four heads,
    // in one order, and writes only its own.
    EffectInstance ia(m_doc, sa->id(), ramp->id()), ib(m_doc, sb->id(), ramp->id());
    QVERIFY(ia.isValid() && ib.isValid());
    int cols = 0, rows = 0;
    const QList<EffectInstance::EffectCell> ca = ia.effectiveCells(cols, rows);
    const QList<EffectInstance::EffectCell> cb = ib.effectiveCells(cols, rows);
    QCOMPARE(ca.count(), 4);
    QVERIFY(ca == cb);
    QList<quint32> order;
    for (const EffectInstance::EffectCell &cell : ca)
        order << cell.fixtureId;
    QCOMPARE(order, QList<quint32>({ m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[3] }));
    QCOMPARE(ia.effectiveFixtureIds(), QList<quint32>({ m_fixtures[0], m_fixtures[1] }));
    ib.runTick();
    QVERIFY(!ib.dmxWrites().isEmpty());
    for (const EffectInstance::DmxWrite &w : ib.dmxWrites())
        QVERIFY(w.fixtureId == m_fixtures[2] || w.fixtureId == m_fixtures[3]);

    // Tracked into Two: still one (the same scenes play on).
    QCOMPARE(sceneOf(st, m_doc, c2, groupTarget(a->id())), sa);
    // Three sets B's effect itself: A is on its own now.
    st->setLayerLook(c3, st->addLayer(c3, groupTarget(b->id())), own);
    const Scene *a3 = sceneOf(st, m_doc, c3, groupTarget(a->id()));
    QVERIFY(a3 != nullptr && a3->palettes().contains(ramp->id()));
    QVERIFY(a3->layoutSpan(ramp->id()).isEmpty());
    QVERIFY(st->spanMembers(c3, groupTarget(a->id()), fx.key).isEmpty());
}

/* D18: a cue's Spread / Grouping / Cross reach only layers whose resolved
   Position aims at a line; D16: each is a key of its own, inherited on its
   own. */
void ShowTree_Test::cueSpreadReachesOnlyLineAims()
{
    enum : quint32 { Line = 301, Point = 302 };
    CueList list;
    list.mode = ListMode::Tracking;
    list.cues << cue(1, "1", CueMode::ListDefault,
                     { layer(11, Towers, { look(KeyPosition, { Line }) }),
                       layer(12, USTs, { look(KeyPosition, { Point }) }),
                       layer(13, ATSpots, {}) },
                     { [] { Look l; l.key = KeySpread; l.value = "0"; return l; }(),
                       [] { Look l; l.key = KeyGrouping; l.value = "pairs"; return l; }() });
    list.cues << cue(2, "2", CueMode::ListDefault, { layer(21, Towers, { [] { Look l; l.key = KeyCross; l.value = "mirror"; return l; }() }) });
    const LineAimTest isLine = [](const Look &l) { return l.palettes.value(0) == Line; };
    const QList<ResolvedCue> r = resolve(list, isLine);
    const ResolvedLayer *towers = r.at(0).layer(groupTarget(Towers));
    QCOMPARE(towers->values.value(KeySpread).look.value, QString("0"));
    QCOMPARE(int(towers->values.value(KeySpread).origin), int(Origin::CueLook));
    QCOMPARE(towers->values.value(KeySpread).span, quint32(1));
    QVERIFY(!r.at(0).layer(groupTarget(USTs))->values.contains(KeySpread));
    QVERIFY(!r.at(0).layer(groupTarget(ATSpots))->values.contains(KeyGrouping));
    // Without a line test: none.
    QVERIFY(!resolve(list).at(0).layer(groupTarget(Towers))->values.contains(KeySpread));
    // Cue 2: Cross set on its own; Spread and Grouping tracked, one by one.
    const ResolvedLayer *t2 = r.at(1).layer(groupTarget(Towers));
    QCOMPARE(t2->values.value(KeyCross).look.value, QString("mirror"));
    QCOMPARE(int(t2->values.value(KeyCross).origin), int(Origin::Own));
    QCOMPARE(int(t2->values.value(KeySpread).origin), int(Origin::Tracked));
    QCOMPARE(t2->values.value(KeySpread).fromCue, quint32(1));
    QCOMPARE(t2->values.value(KeyGrouping).look.value, QString("pairs"));
    // The values read as typed.
    QCOMPARE(ShowTreeController::lineParamValue(KeySpread, "+2.53"), QString("2.5"));
    QCOMPARE(ShowTreeController::lineParamValue(KeySpread, "7"), QString());
    QCOMPARE(ShowTreeController::lineParamValue(KeyGrouping, "pair"), QString("pairs"));
    QCOMPARE(ShowTreeController::lineParamValue(KeyCross, "flip"), QString("mirror"));
}

/* D17: an aim at a line set on the cue is ONE formation over the union of
   the cue's own layers, numbered by where the heads hang along the line
   across the whole union -- a group set's towers and the spots between
   them interleave by position on stage, not by group. A layer that sets
   its own Spread leaves and spreads on its own; the formation tracks. */
void ShowTree_Test::aCueAimIsOneFormationByPosition()
{
    Fixture *sl1 = placedSpot(m_doc, "SL1", -4), *sl2 = placedSpot(m_doc, "SL2", -2);
    Fixture *sr1 = placedSpot(m_doc, "SR1", 2), *sr2 = placedSpot(m_doc, "SR2", 4);
    Fixture *at1 = placedSpot(m_doc, "AT1", -3), *at2 = placedSpot(m_doc, "AT2", 3);
    QVERIFY(sl1 && sl2 && sr1 && sr2 && at1 && at2);
    FixtureGroup *sl = group("SL", { sl1->id(), sl2->id() });
    FixtureGroup *sr = group("SR", { sr1->id(), sr2->id() });
    // AT-Spots listed right to left: the order must come from the stage.
    FixtureGroup *at = group("AT-Spots", { at2->id(), at1->id() });
    MonitorProperties *props = m_doc->monitorProperties();
    StageTarget *edge = props->addStageTarget();
    edge->setName("DS-Edge");
    edge->setPosition(QVector3D(0, 3, 0));
    edge->setLinePoints({ QVector3D(-5, 0, 0), QVector3D(5, 0, 0) });
    edge->makeLine();
    QVERIFY(edge->isLine());

    ShowTreeController *st = m_doc->showTree();
    const quint32 towers = st->addGroupSet("Towers", { sl->id(), sr->id() });
    const quint32 aim = st->aimPaletteFor(edge->id());
    QCOMPARE(m_doc->palette(aim)->name(), QString("Aim DS-Edge"));
    QCOMPARE(st->aimPaletteFor(edge->id()), aim);          // reused
    QVERIFY(st->isLineAim(st->lookForPalette(aim)));

    const quint32 listId = st->addCueList(st->addProduction("P"), "L");      // Tracking
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    Target tt;
    tt.sets << towers;
    st->addLayer(c1, tt);
    st->addLayer(c1, groupTarget(at->id()));
    QVERIFY(st->setCueLook(c1, st->lookForPalette(aim)));

    // One formation of six, by X: SL1 AT1 SL2 SR1 AT2 SR2, end to end.
    const Scene *ts = sceneOf(st, m_doc, c1, tt);
    const Scene *as = sceneOf(st, m_doc, c1, groupTarget(at->id()));
    QVERIFY(ts && as);
    const QList<QPair<Fixture *, float>> want = { { sl1, -5.f }, { at1, -3.f }, { sl2, -1.f },
                                                  { sr1, 1.f }, { at2, 3.f }, { sr2, 5.f } };
    for (const auto &w : want)
    {
        const Scene *sc = (w.first == at1 || w.first == at2) ? as : ts;
        const QVector3D p = aimedAt(m_doc, sc, w.first->id());
        QVERIFY2(qAbs(p.x() - w.second) < 0.05f,
                 qPrintable(QString("%1 aims at x=%2, not %3").arg(w.first->name()).arg(p.x()).arg(w.second)));
    }
    QCOMPARE(st->spanMembers(c1, tt, KeyPosition).count(), 2);

    // Cue Two: a cue Spread 0 -- the formation, all on the centre.
    Look spread0 = ShowTreeController::lineParamLook(KeySpread, "0");
    QVERIFY(st->setCueLook(c2, spread0));
    // (A cue look reaches the cue's own layers: bring them in.)
    st->addLayer(c2, tt);
    st->addLayer(c2, groupTarget(at->id()));
    const Scene *ts2 = sceneOf(st, m_doc, c2, tt);
    QVERIFY(ts2 != nullptr);
    QVERIFY(qAbs(aimedAt(m_doc, ts2, sl1->id()).x()) < 0.05f);

    // Cue Three: AT-Spots sets its own Spread +2.5: it leaves and spreads
    // on its own (its two at a quarter and three quarters).
    const quint32 atLayer = st->addLayer(c3, groupTarget(at->id()));
    QVERIFY(st->setLayerLook(c3, atLayer, ShowTreeController::lineParamLook(KeySpread, "2.5")));
    const Scene *as3 = sceneOf(st, m_doc, c3, groupTarget(at->id()));
    QVERIFY(as3 != nullptr);
    QVERIFY2(qAbs(aimedAt(m_doc, as3, at1->id()).x() + 2.5f) < 0.05f,
             qPrintable(QString::number(aimedAt(m_doc, as3, at1->id()).x())));
    QVERIFY(qAbs(aimedAt(m_doc, as3, at2->id()).x() - 2.5f) < 0.05f);
    // The Towers, still one formation at the tracked spread 0: the centre.
    const Scene *ts3 = sceneOf(st, m_doc, c3, tt);
    QVERIFY(ts3 != nullptr);
    QVERIFY(qAbs(aimedAt(m_doc, ts3, sr2->id()).x()) < 0.05f);
    QVERIFY(st->spanMembers(c3, groupTarget(at->id()), KeyPosition).isEmpty());
    // Its own Spread plays an inline Aim palette made for it, hidden.
    quint32 derived = QLCPalette::invalidId();
    for (quint32 pid : as3->palettes())
        if (m_doc->palette(pid)->type() == QLCPalette::Aim)
            derived = pid;
    QVERIFY(derived != aim && st->isInlinePalette(derived));
    QCOMPARE(m_doc->palette(derived)->stageTargetId(), edge->id());
    // Undo: gone with it.
    QVERIFY(st->undo());
    QVERIFY(m_doc->palette(derived) == nullptr);
}

/* D19: a Spread change between cues fades over the cue's fade time (pan
   and tilt fade, so the beams sweep). */
void ShowTree_Test::aSpreadChangeFadesOverTheCueFade()
{
    Fixture *left = placedSpot(m_doc, "Left", -4), *right = placedSpot(m_doc, "Right", 4);
    FixtureGroup *pair = group("Pair", { left->id(), right->id() });
    StageTarget *edge = m_doc->monitorProperties()->addStageTarget();
    edge->setName("Edge");
    edge->setPosition(QVector3D(0, 3, 0));
    edge->setLinePoints({ QVector3D(-5, 0, 0), QVector3D(5, 0, 0) });
    edge->makeLine();
    ShowTreeController *st = m_doc->showTree();
    const quint32 aim = st->aimPaletteFor(edge->id());
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 l1 = st->addLayer(c1, groupTarget(pair->id()));
    st->setLayerLook(c1, l1, st->lookForPalette(aim));
    st->setLayerLook(c1, l1, ShowTreeController::lineParamLook(KeySpread, "0"));
    const quint32 l2 = st->addLayer(c2, groupTarget(pair->id()));
    st->setLayerLook(c2, l2, ShowTreeController::lineParamLook(KeySpread, "5"));
    ShowTree::Cue two = *st->cue(c2);
    two.fadeIn = 2000;
    QVERIFY(st->setCue(two));

    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    const quint32 pan = left->universeAddress();
    auto panNow = [&]() {
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        const int v = uchar(ua.first()->preGMValues().at(int(pan)));
        m_doc->inputOutputMap()->releaseUniverses(false);
        return v;
    };
    go(chaser);
    tick(20);
    const int from = panNow();
    go(chaser);
    tick(50);                              // a second into a two-second fade
    const int mid = panNow();
    tick(100);
    const int to = panNow();
    QVERIFY2(from != to, qPrintable(QString("%1 -> %2").arg(from).arg(to)));
    QVERIFY2((mid > qMin(from, to) && mid < qMax(from, to)),
             qPrintable(QString("pan %1 -> %2 -> %3: no sweep").arg(from).arg(mid).arg(to)));
    stopAll();
}

/****************************************************************************
 * The command line's language: help from the parser's own table
 ****************************************************************************/

/* Every word the parser takes has help -- syntax, what it does, at least
   one example -- and every example reads (Tab's parser takes it to its
   end), in a show with the names the examples use. `help`, `help <word>`
   and "did you mean" come from the same table. */
void ShowTree_Test::everyCommandWordHasHelpAndItsExamplesParse()
{
    for (const char *g : { "Towers", "USTs", "Steps", "US", "SL", "SR" })
        group(g, { m_fixtures[0] });
    colour("Blue");
    colour("Red");
    dimmer(255, "Full");
    QLCPalette *fx = new QLCPalette(QLCPalette::Effect, m_doc);
    fx->setName("Sinelon");
    m_doc->addPalette(fx);
    QLCPalette *gobo = new QLCPalette(QLCPalette::Gobo, m_doc);
    gobo->setName("Breakup");
    m_doc->addPalette(gobo);
    StageTarget *edge = m_doc->monitorProperties()->addStageTarget();
    edge->setName("DS-Edge");
    edge->makeLine();
    Scene *haze = new Scene(m_doc);
    haze->setName("Haze");
    m_doc->addFunction(haze);
    ShowTreeController *st = m_doc->showTree();
    st->aimPaletteFor(edge->id());                    // "Aim DS-Edge"
    ShowTreeCommand::Context ctx;
    ctx.production = st->addProduction("P");
    ctx.list = st->addCueList(ctx.production, "L");
    st->addCueNumbered(ctx.list, "0.0.3", "Announcer");
    ctx.cue = st->addCueNumbered(ctx.list, "1.0.1", "Verse");
    // Reusables for `use`: a layer BackStage, a cue Walk-in.
    st->makeLayerReusable(ctx.cue, st->addLayer(ctx.cue, groupTarget(m_doc->fixtureGroups().first()->id())),
                          "BackStage");
    const quint32 walk = st->addCueNumbered(ctx.list, "1.0.2", "Walk-in");
    st->makeCueReusable(walk);
    st->removeCue(walk);
    ShowTreeCommand cmd(m_doc, st);

    // The words run() dispatches on, and the ones after a group.
    const QStringList parsed = { "cue", "list", "track", "block", "cueonly", "default", "base", "release",
                                 "include", "function", "fn", "record", "dup", "undo", "redo", "set", "name",
                                 "layer", "copy", "paste", "help", "?", "in", "none", "off", "clear",
                                 "colour", "color", "dimmer", "dim", "level", "position", "pos", "aim", "spread",
                                 "group", "grouping", "cross", "gobo", "zoom", "beam", "focus", "frost", "iris",
                                 "shutter", "strobe", "effect", "fx", "reusable", "use", "unlink", "detach",
                                 "convert", "palette", "pal", "look", "vcpage", "adopt" };
    for (const QString &w : parsed)
        QVERIFY2(ShowTreeCommand::word(w) != nullptr, qPrintable(QString("no help for “%1”").arg(w)));
    int examples = 0;
    for (const ShowTreeCommand::Word &w : ShowTreeCommand::words())
    {
        QVERIFY2(!w.syntax.isEmpty() && !w.what.isEmpty(), qPrintable(w.word));
        QVERIFY2(!w.examples.isEmpty(), qPrintable(QString("“%1” has no example").arg(w.word)));
        for (const QString &ex : w.examples)
        {
            QStringList next;
            QVERIFY2(cmd.candidates(ex, ctx, &next), qPrintable(QString("example “%1” of “%2” doesn't read")
                                                                .arg(ex, w.word)));
            ++examples;
        }
        for (const QString &r : w.related)
            QVERIFY2(ShowTreeCommand::word(r) != nullptr, qPrintable(QString("%1 → %2").arg(w.word, r)));
        // Every word is in what Tab offers at the start or after a group.
        if (w.word != "in" && w.word != "none")
            QVERIFY2(ShowTreeCommand::word(w.word) == &w, qPrintable(w.word));
    }
    QVERIFY(examples >= 50);

    // help, help <word>, did you mean.
    ShowTreeCommand::Result r = cmd.run("help", ctx);
    QVERIFY(r.ok);
    for (const char *heading : { "Navigate", "Build", "Looks", "Tracking", "Record and copy", "Undo" })
        QVERIFY2(r.help.contains(heading), heading);
    r = cmd.run("help spread", ctx);
    QVERIFY(r.ok && r.help.contains("-5…+5") && r.help.contains("ex:cue%20look%20spread%20%2B5"));
    QVERIFY(cmd.run("?", ctx).help.contains("Navigate"));
    QVERIFY(cmd.run("help sprad", ctx).help.contains("Did you mean <b>spread</b>"));
    r = cmd.run("recrod", ctx);
    QVERIFY(!r.ok);
    QVERIFY2(r.message.contains("Did you mean “record”"), qPrintable(r.message));
    r = cmd.run("Towers colr Blue", ctx);
    QVERIFY2(r.message.contains("Did you mean “colour”"), qPrintable(r.message));
    // Tab completes the word after help.
    QStringList options;
    QCOMPARE(cmd.complete("help spr", ctx, &options), QString("help spread "));
}

/* ⌘C / ⌘V (and copy / paste typed): a copy of a cue goes right after the
   selected one, numbered by D23 (the next whole number if free, else a
   point cue; never renumbering); several as a run, in order; into another
   list too; one undo step. Layers paste into a cue. */
void ShowTree_Test::copyAndPasteCues()
{
    FixtureGroup *steps = group("Steps", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *us = group("US", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 listId = st->addCueList(prod, "L");
    const quint32 c1 = st->addCueNumbered(listId, "1", "One");
    const quint32 c2 = st->addCueNumbered(listId, "2", "Two");
    const quint32 c3 = st->addCueNumbered(listId, "3", "Three");
    const quint32 p1 = st->addLayer(c1, groupTarget(steps->id()));
    st->setLayerLook(c1, p1, st->lookForPalette(full));
    const quint32 ch1 = st->addChildLayer(c1, p1, groupTarget(us->id()));
    st->setLayerLook(c1, ch1, st->lookForPalette(half));
    st->addInclude(c3, c1);

    // After 1 (2 taken): a point cue; the copy carries its layers, the
    // child's parent its own copy; never the same ids.
    QList<quint32> made = st->pasteCues({ c1 }, c1);
    QCOMPARE(made.count(), 1);
    const Cue *copy = st->cue(made.first());
    QCOMPARE(copy->number, QString("1.5"));
    QCOMPARE(copy->name, QString("One"));
    QCOMPARE(copy->layers.count(), 2);
    QVERIFY(copy->layers.at(0).id != p1);
    QCOMPARE(copy->layers.at(1).parent, copy->layers.at(0).id);
    QCOMPARE(st->cueList(listId)->indexOf(made.first()), 1);
    QCOMPARE(st->undoText(), QString("Paste cue"));
    // After the last: the next whole number.
    made = st->pasteCues({ c3 }, c3);
    QCOMPARE(st->cue(made.first())->number, QString("4"));
    QCOMPARE(st->cue(made.first())->includes, QList<quint32>({ c1 }));
    // Several: a run after 2, in their order (2.5, 2.6), one undo step.
    made = st->pasteCues({ c1, c3 }, c2);
    QCOMPARE(made.count(), 2);
    QCOMPARE(st->cue(made.at(0))->number, QString("2.5"));
    QCOMPARE(st->cue(made.at(1))->number, QString("2.6"));
    QCOMPARE(st->cueList(listId)->indexOf(made.at(1)), st->cueList(listId)->indexOf(c2) + 2);
    const int count = int(st->cueList(listId)->cues.count());
    QVERIFY(st->undo());
    QCOMPARE(int(st->cueList(listId)->cues.count()), count - 2);
    // Never renumbers the others.
    QCOMPARE(st->cue(c2)->number, QString("2"));

    // Into another list: after its cue; an include of the other list's cue
    // is dropped.
    const quint32 other = st->addCueList(prod, "M");
    const quint32 m1 = st->addCueNumbered(other, "10", "Ten");
    made = st->pasteCues({ c3 }, m1);
    QCOMPARE(st->cue(made.first())->number, QString("11"));
    QVERIFY(st->cue(made.first())->includes.isEmpty());
    QCOMPARE(st->listOfCue(made.first())->id, other);
    // The copy plays as the original: the same layers' scenes.
    QCOMPARE(st->layerScene(made.first(), groupTarget(steps->id())), Function::invalidId());   // c3 has none
    made = st->pasteCues({ c1 }, m1);
    QCOMPARE(st->layerScene(made.first(), groupTarget(steps->id())), st->layerScene(c1, groupTarget(steps->id())));

    // Layers: into a cue; onto a cue with that target, its looks set there.
    const QList<quint32> ls = st->pasteLayers({ qMakePair(c1, p1), qMakePair(c1, ch1) }, c2);
    QCOMPARE(ls.count(), 2);
    QCOMPARE(st->cue(c2)->layers.count(), 2);
    QCOMPARE(st->cue(c2)->layers.at(1).parent, st->cue(c2)->layers.at(0).id);
    QVERIFY(st->setLayerLook(c2, st->cue(c2)->layers.at(0).id, st->lookForPalette(half)));
    QCOMPARE(st->pasteLayers({ qMakePair(c1, p1) }, c2).count(), 1);
    QCOMPARE(st->cue(c2)->layers.count(), 2);
    QCOMPARE(st->cue(c2)->layers.at(0).look(KeyDimmer)->palettes, QList<quint32>({ full }));

    // Typed: copy cue / paste, one undo step each.
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = prod;
    ctx.list = listId;
    ctx.cue = c3;
    QVERIFY(cmd.run("copy cue 1 2", ctx).ok);
    QCOMPARE(cmd.clipboard.cues, QList<quint32>({ c1, c2 }));
    const QString before = st->undoText();
    ShowTreeCommand::Result r = cmd.run("paste", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(r.context.cue)->name, QString("Two"));
    QVERIFY(st->undo());
    QCOMPARE(st->undoText(), before);
}

/****************************************************************************
 * P4: reusable layers and cues, Detach a list, Convert to tracking
 ****************************************************************************/

namespace
{
QList<quint32> cueIds(const CueList *l)
{
    QList<quint32> out;
    for (const Cue &c : l->cues)
        out << c.id;
    return out;
}
}

/* Make reusable on a layer: its uses in any cue of any list stay one --
   an edit to any use (looks, groups, name) is an edit to all, and a use
   tracks on as any layer does; Unlink here makes a local copy; a reusable
   in use is deleted only by unlinking its uses. Each is one undo step. */
void ShowTree_Test::reusableLayersAreLiveLinked()
{
    FixtureGroup *bs = group("BackStage", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *steps = group("StepTops", { m_fixtures[2] });
    FixtureGroup *extra = group("Extra", { m_fixtures[3] });
    const quint32 d45 = dimmer(115, "45%");
    const quint32 full = dimmer(255, "Full");
    const quint32 blue = colour("Blue");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 listA = st->addCueList(prod, "A");
    const quint32 a1 = st->addCue(listA, -1, "One");
    const quint32 a2 = st->addCue(listA, 0, "Two");
    const quint32 a3 = st->addCue(listA, 1, "Three");
    const quint32 la1 = st->addLayer(a1, groupTarget(bs->id()));
    QVERIFY(st->setLayerLook(a1, la1, st->lookForPalette(blue)));
    QVERIFY(st->setLayerLook(a1, la1, st->lookForPalette(d45)));
    QVERIFY(st->setCueMode(a3, CueMode::Block));
    QVERIFY(st->setLayerLook(a3, st->addLayer(a3, groupTarget(steps->id())), st->lookForPalette(full)));
    const quint32 listB = st->addCueList(prod, "B");
    const quint32 b1 = st->addCue(listB, -1, "Walk");

    QString why;
    const QString undoBefore = st->undoText();
    const quint32 rid = st->makeLayerReusable(a1, la1, "BackStage Wash", &why);
    QVERIFY2(rid != invalidId(), qPrintable(why));
    QCOMPARE(st->cue(a1)->layer(la1)->reusable, rid);
    QCOMPARE(st->cue(a1)->layer(la1)->name, QString("BackStage Wash"));
    QCOMPARE(st->reusableLayer(rid)->looks.count(), 2);
    QVERIFY(st->undo());                                    // one step
    QCOMPARE(st->undoText(), undoBefore);
    QVERIFY(st->reusableLayers().isEmpty());
    QCOMPARE(st->cue(a1)->layer(la1)->reusable, invalidId());
    QVERIFY(st->redo());
    QCOMPARE(st->cue(a1)->layer(la1)->reusable, rid);
    QCOMPARE(st->makeLayerReusable(a1, la1, "Again", &why), invalidId());   // already
    QVERIFY(why.contains("reusable already"));

    // Uses in a Block cue of the same list and in another list.
    const quint32 la3 = st->insertReusableLayer(a3, rid);
    const quint32 lb1 = st->insertReusableLayer(b1, rid);
    QVERIFY(la3 != invalidId() && lb1 != invalidId());
    QCOMPARE(st->reusableUses(rid).count(), 3);
    QCOMPARE(st->cue(b1)->layer(lb1)->look(KeyDimmer)->palettes, QList<quint32>({ d45 }));

    // Cue 2 tracks it from cue 1: the same layer (its target).
    QList<ResolvedCue> r = st->resolved(*st->cueList(listA));
    QCOMPARE(r[1].cueId, a2);
    CHECK_VALUE(r[1], bs->id(), KeyDimmer, d45, Origin::Tracked, a1);
    QCOMPARE(r[1].layer(groupTarget(bs->id()))->name, QString("BackStage Wash"));

    // An edit to the use in list B: every use.
    QVERIFY(st->setLayerLook(b1, lb1, st->lookForPalette(full)));
    QCOMPARE(st->cue(a1)->layer(la1)->look(KeyDimmer)->palettes, QList<quint32>({ full }));
    QCOMPARE(st->cue(a3)->layer(la3)->look(KeyDimmer)->palettes, QList<quint32>({ full }));
    QCOMPARE(st->reusableLayer(rid)->target.groups, QList<quint32>({ bs->id() }));
    // ...on the rig: list A's three cues (cue 2 tracked) and list B's.
    Chaser *chA = qobject_cast<Chaser *>(m_doc->function(st->cueList(listA)->chaser));
    Chaser *chB = qobject_cast<Chaser *>(m_doc->function(st->cueList(listB)->chaser));
    QList<QHash<quint32, uchar>> v = playAll(chA);
    QCOMPARE(int(v[0][0]), 255);
    QCOMPARE(int(v[1][1]), 255);
    QCOMPARE(int(v[2][0]), 255);
    QCOMPARE(int(v[2][2]), 255);
    QCOMPARE(int(playAll(chB)[0][1]), 255);
    // ...one undo step puts 45% back in every use.
    QVERIFY(st->undo());
    QCOMPARE(st->cue(a1)->layer(la1)->look(KeyDimmer)->palettes, QList<quint32>({ d45 }));
    QCOMPARE(st->cue(a3)->layer(la3)->look(KeyDimmer)->palettes, QList<quint32>({ d45 }));
    QCOMPARE(int(playAll(chB)[0][1]), 115);
    QVERIFY(st->redo());

    // A group added to one use: every use lights it; cue 2 still tracks it.
    QVERIFY(st->addToLayer(a3, la3, { extra->id() }, {}));
    Target both;
    both.groups << bs->id() << extra->id();
    QVERIFY(st->cue(a1)->layer(la1)->target == both);
    QVERIFY(st->cue(b1)->layer(lb1)->target == both);
    r = st->resolved(*st->cueList(listA));
    QVERIFY(r[1].layer(both) != nullptr);
    QCOMPARE(int(r[1].layer(both)->origin), int(Origin::Tracked));
    QCOMPARE(int(playAll(chA)[1][3]), 255);

    // Rename: the reusable, or any use -- every use is called that.
    QVERIFY(st->renameReusable(rid, "Back"));
    QCOMPARE(st->cue(b1)->layer(lb1)->name, QString("Back"));
    QVERIFY(st->renameNode(lb1, "Backline"));
    QCOMPARE(st->reusableLayer(rid)->name, QString("Backline"));
    QCOMPARE(st->cue(a1)->layer(la1)->name, QString("Backline"));

    // Split / merge a use: refused (every use would change), with why.
    QVERIFY(!st->splitLayer(a1, la1, nullptr, &why));
    QVERIFY(why.contains("Unlink"));

    // Unlink here: a3's use is its own now; the others stay linked.
    QVERIFY(st->unlinkLayer(a3, la3));
    QCOMPARE(st->cue(a3)->layer(la3)->reusable, invalidId());
    QVERIFY(st->setLayerLook(a3, la3, st->lookForPalette(d45)));
    QCOMPARE(st->cue(a1)->layer(la1)->look(KeyDimmer)->palettes, QList<quint32>({ full }));
    QCOMPARE(st->reusableUses(rid).count(), 2);

    // Delete: refused while used (the uses named); unlinking them all, one step.
    QVERIFY(!st->removeReusable(rid, false, &why));
    QVERIFY2(why.contains("used in 2"), qPrintable(why));
    QVERIFY(why.contains("A 1"));
    QVERIFY(st->removeReusable(rid, true));
    QVERIFY(st->reusableLayers().isEmpty());
    QCOMPARE(st->cue(a1)->layer(la1)->reusable, invalidId());
    QCOMPARE(st->cue(a1)->layer(la1)->look(KeyDimmer)->palettes, QList<quint32>({ full }));
    QVERIFY(st->undo());
    QCOMPARE(st->reusableUses(rid).count(), 2);
}

/* Make reusable on a cue: a "Blackout" put in two lists. Its layers and
   looks are the same in every use; number, timing and includes each use's
   own. Edits in either list reach both. */
void ShowTree_Test::reusableCuesInSeveralLists()
{
    FixtureGroup *all = group("All", { m_fixtures[0], m_fixtures[1], m_fixtures[2] });
    FixtureGroup *works = group("Works", { m_fixtures[4] });
    const quint32 full = dimmer(255, "Full");
    const quint32 low = dimmer(20, "Low");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 listA = st->addCueList(prod, "A");
    const quint32 a1 = st->addCue(listA, -1, "Song");
    QVERIFY(st->setLayerLook(a1, st->addLayer(a1, groupTarget(all->id())), st->lookForPalette(full)));
    const quint32 a2 = st->addCue(listA, 0, "Walk-in");
    QVERIFY(st->setCueMode(a2, CueMode::Block));
    QVERIFY(st->setLayerLook(a2, st->addLayer(a2, groupTarget(works->id())), st->lookForPalette(low)));
    Cue timed = *st->cue(a2);
    timed.fadeIn = 1500;
    QVERIFY(st->setCue(timed));
    const quint32 listB = st->addCueList(prod, "B");
    const quint32 b1 = st->addCue(listB, -1, "Opener");
    QVERIFY(st->setLayerLook(b1, st->addLayer(b1, groupTarget(all->id())), st->lookForPalette(low)));

    QString why;
    const quint32 rid = st->makeCueReusable(a2, QString(), &why);
    QVERIFY2(rid != invalidId(), qPrintable(why));
    QCOMPARE(st->reusableCue(rid)->name, QString("Walk-in"));
    QCOMPARE(st->reusableCue(rid)->mode, CueMode::Block);
    QCOMPARE(st->reusableCue(rid)->fadeIn, uint(1500));
    // Used in B after its first cue: numbered there, its timing to start.
    const quint32 b2 = st->insertReusableCue(listB, 0, rid);
    QVERIFY(b2 != invalidId());
    QCOMPARE(st->cueList(listB)->indexOf(b2), 1);
    QCOMPARE(st->cue(b2)->number, QString("2"));
    QCOMPARE(st->cue(b2)->fadeIn, uint(1500));
    QCOMPARE(st->cue(b2)->name, QString("Walk-in"));
    QCOMPARE(st->cue(b2)->layers.count(), 1);
    QVERIFY(st->cue(b2)->layers.first().id != st->cue(a2)->layers.first().id);   // ids its own
    // Its own timing per use: not shared.
    Cue t = *st->cue(b2);
    t.fadeIn = 0;
    QVERIFY(st->setCue(t));
    QCOMPARE(st->cue(a2)->fadeIn, uint(1500));

    // A layer added in B's use: A's has it too; a look changed in A: B too.
    QVERIFY(st->setLayerLook(b2, st->addLayer(b2, groupTarget(all->id())), st->lookForPalette(low)));
    QCOMPARE(st->cue(a2)->layers.count(), 2);
    QCOMPARE(st->reusableCue(rid)->layers.count(), 2);
    const quint32 aw = st->cue(a2)->layers.first().id;
    QVERIFY(st->setLayerLook(a2, aw, st->lookForPalette(full)));
    QCOMPARE(st->cue(b2)->layers.first().look(KeyDimmer)->palettes, QList<quint32>({ full }));
    // Mode: the reusable's too.
    QVERIFY(st->setCueMode(b2, CueMode::Track));
    QCOMPARE(st->cue(a2)->mode, CueMode::Track);
    QVERIFY(st->undo());
    QCOMPARE(st->cue(a2)->mode, CueMode::Block);

    // On the rig the use plays the same in both lists (A's fade made the
    // same as B's for the compare).
    Cue noFade = *st->cue(a2);
    noFade.fadeIn = 0;
    QVERIFY(st->setCue(noFade));
    Chaser *chA = qobject_cast<Chaser *>(m_doc->function(st->cueList(listA)->chaser));
    Chaser *chB = qobject_cast<Chaser *>(m_doc->function(st->cueList(listB)->chaser));
    const QHash<quint32, uchar> inA = playAll(chA)[1];
    const QHash<quint32, uchar> inB = playAll(chB)[1];
    QCOMPARE(int(inA[4]), 255);
    QCOMPARE(int(inA[0]), 20);
    QCOMPARE(inA, inB);

    // Make reusable on a layer of a use: refused (its layers are the cue's).
    QCOMPARE(st->makeLayerReusable(b2, st->cue(b2)->layers.first().id, "X", &why), invalidId());
    QVERIFY(why.contains("reusable cue"));
    // Used by: both cues, by list and number.
    const QList<ShowTreeController::ReusableUse> uses = st->reusableUses(rid);
    QCOMPARE(uses.count(), 2);
    QCOMPARE(uses.at(0).cue, a2);
    QCOMPARE(uses.at(1).cue, b2);
    QVERIFY(uses.at(1).label.startsWith("B "));
    // Unlink B's: a copy of its own.
    QVERIFY(st->unlinkCue(b2));
    QVERIFY(st->setLayerLook(b2, st->cue(b2)->layers.first().id, st->lookForPalette(low)));
    QCOMPARE(st->cue(a2)->layers.first().look(KeyDimmer)->palettes, QList<quint32>({ full }));
}

/* Reusables are part of the show and of every undo step. */
void ShowTree_Test::reusablesSaveLoadAndUndo()
{
    FixtureGroup *bs = group("BackStage", { m_fixtures[0] });
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 listId = st->addCueList(prod, "L");
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 l1 = st->addLayer(c1, groupTarget(bs->id()));
    // A one-off value in it: kept while only the reusable holds it.
    const quint32 inl = st->createInlinePalette(QLCPalette::Dimmer, { 33 }, "13%");
    QVERIFY(st->setLayerLook(c1, l1, st->lookForPalette(inl)));
    const quint32 rl = st->makeLayerReusable(c1, l1, "Wash");
    const quint32 rc = st->makeCueReusable(c1, "State");
    QVERIFY(rl != invalidId() && rc != invalidId());
    QCOMPARE(st->reusableCue(rc)->layers.first().reusable, rl);   // a reusable layer inside
    QVERIFY(st->removeCue(c1));
    QVERIFY(m_doc->palette(inl) != nullptr);                      // the reusable holds it
    QVERIFY(st->undo());

    const QByteArray xml = engineXml(m_doc);
    QVERIFY(xml.contains("<ReusableLayer"));
    QVERIFY(xml.contains("<ReusableCue"));
    Doc *other = new Doc(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other->loadXML(r, false));
    ShowTreeController *ost = other->showTree();
    QCOMPARE(ost->reusableLayers().count(), 1);
    QCOMPARE(ost->reusableLayer(rl)->name, QString("Wash"));
    QCOMPARE(ost->reusableCue(rc)->layers.count(), 1);
    QCOMPARE(ost->cue(c1)->reusable, rc);
    QCOMPARE(ost->cue(c1)->layers.first().reusable, rl);
    QVERIFY(ost->newId() > qMax(rl, rc));
    // A use edited after load: the reusable follows.
    QVERIFY(ost->setLayerLook(c1, l1, ost->lookForPalette(half)));
    QCOMPARE(ost->reusableLayer(rl)->looks.first().palettes, QList<quint32>({ half }));
    QCOMPARE(ost->reusableCue(rc)->layers.first().looks.first().palettes, QList<quint32>({ half }));
    delete other;

    // A command that fails puts the reusables back as they were.
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = prod;
    ctx.list = listId;
    ctx.cue = c1;
    st->beginBatch("test");
    QVERIFY(st->renameReusable(rl, "Gone"));
    st->endBatch(false);
    QCOMPARE(st->reusableLayer(rl)->name, QString("Wash"));
}

/* Detach a cue list to a plain chaser: its chaser and collections become
   the show's own (unlocked, shown); the scenes only it plays go with them;
   one another list also plays is copied at once (open 19), so the chaser is
   all its own from the start and the tree keeps its scene. It plays as
   before. One undo step: the list comes back on the same functions (the
   copies swept); redo hands them over and copies again. */
void ShowTree_Test::detachAListToAPlainChaser()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 l1 = st->addCueList(prod, "Song");
    const quint32 c1 = st->addCue(l1, -1, "One");
    const quint32 c2 = st->addCue(l1, 0, "Two");
    QVERIFY(st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(full)));
    QVERIFY(st->setLayerLook(c2, st->addLayer(c2, groupTarget(b->id())), st->lookForPalette(half)));
    const quint32 l2 = st->addCueList(prod, "Other");
    const quint32 o1 = st->addCue(l2, -1, "Same A");
    const quint32 ol = st->addLayer(o1, groupTarget(a->id()));
    QVERIFY(st->setLayerLook(o1, ol, st->lookForPalette(full)));     // shares A·Full with Song

    const quint32 chaserId = st->cueList(l1)->chaser;
    const QList<quint32> colls { st->cue(c1)->collection, st->cue(c2)->collection };
    const quint32 shared = st->layerScene(c1, groupTarget(a->id()));
    const quint32 own = st->layerScene(c2, groupTarget(b->id()));
    QCOMPARE(shared, st->layerScene(o1, groupTarget(a->id())));
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(chaserId));
    const QList<QHash<quint32, uchar>> before = playAll(ch);

    QCOMPARE(st->detachList(l1), chaserId);
    QVERIFY(st->cueList(l1) == nullptr);
    QVERIFY(!st->isGenerated(chaserId));
    for (quint32 c : colls)
    {
        QVERIFY(!st->isGenerated(c));
        QVERIFY(m_doc->function(c)->isVisible());
    }
    QVERIFY(!st->isGenerated(own));
    QVERIFY(m_doc->function(own)->isVisible());
    QVERIFY(st->isGenerated(shared));                   // Other still plays it: the tree's
    QCOMPARE(playAll(ch), before);
    QCOMPARE(ch->stepsCount(), 2);
    // ... and the chaser plays a copy of its own.
    auto copyIn = [&]() {
        const QList<quint32> fids = qobject_cast<Collection *>(m_doc->function(colls.first()))->functions();
        return fids.count() == 1 ? fids.first() : Function::invalidId();
    };
    const quint32 copy = copyIn();
    QVERIFY(copy != shared && m_doc->function(copy) != nullptr);
    QVERIFY(!st->isGenerated(copy));
    QVERIFY(m_doc->function(copy)->isVisible());
    QVERIFY(m_doc->function(copy)->name().contains("One"));   // named as it was in Song
    for (quint32 c : colls)
        QVERIFY(!qobject_cast<Collection *>(m_doc->function(c))->functions().contains(shared));

    // Other changes its scene: the detached chaser doesn't notice.
    QVERIFY(st->setLayerLook(o1, ol, st->lookForPalette(half)));
    QVERIFY(m_doc->function(copy) != nullptr);
    QCOMPARE(copyIn(), copy);
    QCOMPARE(playAll(ch), before);

    // Undo (the look, then the detach): the list is back on its functions.
    QVERIFY(st->undo());
    QVERIFY(st->undo());
    QVERIFY(st->cueList(l1) != nullptr);
    QCOMPARE(st->cueList(l1)->chaser, chaserId);
    QVERIFY(st->isGenerated(chaserId));
    for (quint32 c : colls)
        QVERIFY(st->isGenerated(c));
    QVERIFY(st->isGenerated(own));
    QVERIFY(!m_doc->function(own)->isVisible());
    QVERIFY(st->isGenerated(copyIn()));                 // the tree's scene again
    for (Function *f : m_doc->functions())              // no copy left behind
        if (f->type() == Function::SceneType)
            QVERIFY2(st->isGenerated(f->id()), qPrintable(f->name()));
    QCOMPARE(playAll(ch), before);
    // Redo: plain again, and its own copy again.
    QVERIFY(st->redo());
    QVERIFY(st->cueList(l1) == nullptr);
    QVERIFY(!st->isGenerated(chaserId));
    QVERIFY(!st->isGenerated(own));
    QVERIFY(!st->isGenerated(copyIn()));
    QCOMPARE(playAll(ch), before);
    // A plain chaser can't be detached again; an unknown list neither.
    QCOMPARE(st->detachList(l1), Function::invalidId());
}

/* Convert to tracking (§7) on DanceHysteria's shape: every cue plays the
   same, channel by channel, before and after; the includes of the cue
   before become tracking, values that only repeat go. One undo step. */
void ShowTree_Test::convertToTrackingPlaysTheSame()
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

    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importChaser(chaser->id());
    const quint32 listId = rep.list;
    Chaser *list = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    const QList<quint32> c = cueIds(st->cueList(listId));

    // As imported, the cues where an include at full meets their own lower
    // level mix (as the chaser played them): Convert keeps them as they are.
    {
        const ShowTreeController::ConvertPlan mixed = st->planConvertToTracking(listId);
        QVERIFY2(mixed.ok, qPrintable(mixed.why));
        QCOMPARE(mixed.mixKept.count(), 4);
        QCOMPARE(mixed.mixCleared, 0);
    }
    // The rest is about the tree's rule: put them on it.
    for (int i : { 3, 4, 5, 6 })
    {
        QVERIFY(st->cue(c[i])->mixOverlap);
        QVERIFY(st->setCueMixOverlap(c[i], false));
    }
    const QList<QHash<quint32, uchar>> imported = playAll(list);

    // The plan first: nothing changes yet.
    const QByteArray treeBefore = productionXml(*st->production(rep.production));
    const ShowTreeController::ConvertPlan plan = st->planConvertToTracking(listId);
    QVERIFY2(plan.ok, qPrintable(plan.why));
    QCOMPARE(productionXml(*st->production(rep.production)), treeBefore);
    for (const QString &line : plan.changes)
        qInfo().noquote() << "  plan:" << line;
    QVERIFY(plan.tracking >= 4);
    QCOMPARE(plan.includesRemoved, 2);         // 0.0.5 ⊃ 0.0.4, 0.0.6 ⊃ 0.0.5: the cue before
    QVERIFY(plan.layersRemoved >= 1);           // 0.0.4's BackStage only repeats 0.0.3's
    QVERIFY(plan.looksRemoved >= 2);
    QVERIFY(!plan.changes.isEmpty());

    const QString undoBefore = st->undoText();
    QVERIFY(st->convertToTracking(listId));
    const CueList *after = st->cueList(listId);
    QCOMPARE(after->mode, ListMode::Tracking);
    QCOMPARE(cueIds(after), c);
    QCOMPARE(after->chaser, list->id());
    QVERIFY(after->cue(c[3])->includes.isEmpty());       // 0.0.5: tracks from 0.0.4 now
    QCOMPARE(after->cue(c[6])->includes, QList<quint32>({ c[1] }));   // 0.0.9 still includes 0.0.3
    int looksAfter = 0, looksBefore = 0;
    for (const Cue &cue : after->cues)
        for (const Layer &l : cue.layers)
            looksAfter += l.looks.count();
    QXmlStreamReader pr(treeBefore);
    while (!pr.atEnd())
        if (pr.readNext() == QXmlStreamReader::StartElement && pr.name() == QLatin1String("Look"))
            ++looksBefore;
    QVERIFY2(looksAfter < looksBefore, qPrintable(QString("%1 looks, %2 before").arg(looksAfter).arg(looksBefore)));

    // 0 channels differ, cue by cue.
    const QList<QHash<quint32, uchar>> converted = playAll(list);
    QCOMPARE(converted.count(), imported.count());
    for (int i = 0; i < converted.count(); ++i)
        for (quint32 ch = 0; ch < quint32(m_fixtures.count()); ++ch)
            QVERIFY2(converted[i].value(ch) == imported[i].value(ch),
                     qPrintable(QString("cue %1, channel %2: %3, before %4").arg(i + 1).arg(ch)
                                .arg(converted[i].value(ch)).arg(imported[i].value(ch))));

    // One undo step back to the imported list; converting again: nothing to do.
    QVERIFY(st->undo());
    QCOMPARE(st->undoText(), undoBefore);
    QCOMPARE(st->cueList(listId)->mode, ListMode::NonTracking);
    QCOMPARE(productionXml(*st->production(rep.production)), treeBefore);
    QVERIFY(st->redo());
    const ShowTreeController::ConvertPlan again = st->planConvertToTracking(listId);
    QVERIFY(!again.ok);
    QVERIFY(again.why.contains("Nothing to convert"));
}

/* Convert on lists made to catch it out: overlapping layers, a mixing cue,
   an opaque function, a base cue, a cue that drops a layer. Each cue plays
   the same before and after, and what can't be made tracking stays. */
void ShowTree_Test::convertToTrackingSyntheticLists()
{
    FixtureGroup *wide = group("Wide", { m_fixtures[0], m_fixtures[1], m_fixtures[2] });
    FixtureGroup *mid = group("Mid", { m_fixtures[1] });
    FixtureGroup *side = group("Side", { m_fixtures[3] });
    FixtureGroup *back = group("Back", { m_fixtures[4], m_fixtures[5] });
    const quint32 low = dimmer(40, "Low");
    const quint32 half = dimmer(127, "Half");
    const quint32 full = dimmer(255, "Full");
    Scene *raw = new Scene(m_doc);
    raw->setName("Haze");
    raw->setValue(m_fixtures[6], 0, 90);
    m_doc->addFunction(raw);

    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 listId = st->addCueList(prod, "Synthetic", ListMode::NonTracking);
    QList<quint32> c;
    for (int i = 0; i < 8; ++i)
        c << st->addCue(listId, i - 1, QString("C%1").arg(i + 1));
    auto set = [&](int i, FixtureGroup *g, quint32 pal) {
        quint32 lid = invalidId();
        for (const Layer &l : st->cue(c[i])->layers)
            if (l.target == groupTarget(g->id()))
                lid = l.id;
        if (lid == invalidId())
            lid = st->addLayer(c[i], groupTarget(g->id()));
        QVERIFY(st->setLayerLook(c[i], lid, st->lookForPalette(pal)));
    };
    // C1: Wide full + Side half
    set(0, wide, full); set(0, side, half);
    // C2: the same, and Mid low over Wide (overlap: the lower wins on Mid)
    set(1, wide, full); set(1, side, half); set(1, mid, low);
    // C3: the same again, Back half
    set(2, wide, full); set(2, side, half); set(2, mid, low); set(2, back, half);
    // C4: Side gone (dropped), the haze as it is
    set(3, wide, full); set(3, mid, low); set(3, back, half);
    {
        Target t;
        t.function = raw->id();
        QVERIFY(st->addLayer(c[3], t) != invalidId());
    }
    // C5: mixing overlap (as an import keeps it)
    set(4, wide, half); set(4, mid, full); set(4, back, half);
    QVERIFY(st->setCueMixOverlap(c[4], true));
    // C6: base-like: all different
    set(5, side, low);
    // C7: Side low again and Back
    set(6, side, low); set(6, back, full);
    // C8: mixing where the one rule would change it: Mid low under Wide
    // full mixes to full; the rule gives Mid its own low.
    set(7, wide, full); set(7, mid, low);
    QVERIFY(st->setCueMixOverlap(c[7], true));
    QVERIFY(st->setBaseCue(listId, c[0]));

    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    const QList<QHash<quint32, uchar>> before = playAll(ch);
    ShowTreeController::ConvertPlan plan;
    QVERIFY2(st->convertToTracking(listId, &plan), qPrintable(plan.why));
    for (const QString &line : plan.changes)
        qInfo().noquote() << "  plan:" << line;
    QVERIFY(plan.tracking >= 3);
    QVERIFY(plan.looksRemoved >= 4);
    const QList<QHash<quint32, uchar>> after = playAll(ch);
    QCOMPARE(after.count(), before.count());
    for (int i = 0; i < after.count(); ++i)
        for (quint32 k = 0; k < quint32(m_fixtures.count()); ++k)
            QVERIFY2(after[i].value(k) == before[i].value(k),
                     qPrintable(QString("cue %1, channel %2: %3, before %4").arg(i + 1).arg(k)
                                .arg(after[i].value(k)).arg(before[i].value(k))));
    // One rule (open 13): C5's mix gave Mid full (HTP of half and full),
    // and so does the rule (Mid is lower, full): cleared. C8's would not
    // be the same: it stays Mix, and the plan says so.
    QVERIFY(!st->cue(c[4])->mixOverlap);
    QVERIFY(st->cue(c[7])->mixOverlap);
    QCOMPARE(plan.mixCleared, 1);
    QCOMPARE(plan.mixKept.count(), 1);
    QVERIFY2(plan.mixKept.first().startsWith(st->cueLabel(*st->cueList(listId), 7) + ": stays Mix"),
             qPrintable(plan.mixKept.first()));
    // C2's Mid (overlapping Wide) kept its own value.
    QVERIFY(st->cue(c[1])->layers.count() >= 1);
}

/* reusable / use / unlink / detach / convert typed, with help and Tab. */
void ShowTree_Test::reuseDetachAndConvertTyped()
{
    FixtureGroup *bs = group("BackStage", { m_fixtures[0] });
    group("Steps", { m_fixtures[1] });
    dimmer(255, "Full");
    dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = st->addProduction("P");
    ShowTreeCommand::Result r = cmd.run("list \"Song\"", ctx);
    ctx = r.context;
    r = cmd.run("cue 1 \"One\"", ctx);
    ctx = r.context;
    const quint32 c1 = ctx.cue;
    r = cmd.run("BackStage dimmer Full", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx.layer = r.layer;
    r = cmd.run("reusable \"Back Wash\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->reusableLayers().count(), 1);
    QCOMPARE(st->undoText(), QString("reusable \"Back Wash\""));
    const quint32 rid = st->reusableLayers().first().id;

    ctx.layer = invalidId();
    r = cmd.run("cue 2 \"Two\" block", ctx);
    ctx = r.context;
    QStringList opts;
    QCOMPARE(cmd.complete("use Ba", ctx, &opts), QString("use Back Wash "));
    r = cmd.run("use \"Back Wash\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->reusableUses(rid).count(), 2);
    ctx.layer = r.layer;
    ctx.layers = { qMakePair(ctx.cue, r.layer) };
    r = cmd.run("dimmer Half", ctx);
    ctx.layers.clear();
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(c1)->layers.first().look(KeyDimmer)->palettes, st->cue(ctx.cue)->layers.first().look(KeyDimmer)->palettes);
    r = cmd.run("unlink", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->reusableUses(rid).count(), 1);
    QVERIFY(!cmd.run("unlink", ctx).ok);            // not a use any more

    // The cue as a reusable, used after it.
    ctx.layer = invalidId();
    r = cmd.run("reusable", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->reusableCues().count(), 1);
    QCOMPARE(st->reusableCues().first().name, QString("Two"));
    r = cmd.run("use Two", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cueList(ctx.list)->cues.count(), 3);

    // convert: the plan (nothing changes), convert apply: one step.
    r = cmd.run("list \"Imported\"", ctx);
    ctx = r.context;
    QVERIFY(st->setListMode(ctx.list, ListMode::NonTracking));
    r = cmd.run("cue 1 \"A\"", ctx);
    ctx = r.context;
    QVERIFY(cmd.run("BackStage dimmer Full", ctx).ok);
    r = cmd.run("cue 2 \"B\"", ctx);
    ctx = r.context;
    QVERIFY(cmd.run("BackStage dimmer Full", ctx).ok);
    QVERIFY(cmd.run("Steps dimmer Half", ctx).ok);
    const QString undoBefore = st->undoText();
    r = cmd.run("convert", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(r.help.contains("convert apply"));
    QCOMPARE(st->undoText(), undoBefore);
    QCOMPARE(st->cueList(ctx.list)->mode, ListMode::NonTracking);
    QCOMPARE(cmd.complete("convert a", ctx), QString("convert apply "));
    r = cmd.run("convert apply", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cueList(ctx.list)->mode, ListMode::Tracking);
    QCOMPARE(st->cue(ctx.cue)->layers.count(), 1);   // BackStage tracks in from 1
    Q_UNUSED(bs)

    // detach list: a plain chaser, the list gone.
    const quint32 chaser = st->cueList(ctx.list)->chaser;
    QCOMPARE(cmd.complete("detach l", ctx), QString("detach list "));
    r = cmd.run("detach list", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(!st->isGenerated(chaser));
    QVERIFY(m_doc->function(chaser) != nullptr);
    QVERIFY(cmd.run("undo", r.context).ok);
    QVERIFY(st->isGenerated(chaser));

    // help knows the new words.
    for (const char *w : { "reusable", "use", "unlink", "detach", "convert" })
        QVERIFY2(ShowTreeCommand::helpText(w).contains(QString("<b>%1</b>").arg(w)), w);
}

/****************************************************************************
 * R1/R2: palette housekeeping, Used by, one history
 ****************************************************************************/

/* PaletteOps (PROGRAMMING_ROLE_DESIGN.md §5, P-D8): folders hold any type,
   a path stored the old way (type first) reads as its folder, folder
   rename/delete act on every type, each operation one undo step. */
void ShowTree_Test::paletteOpsFoldersAnyType()
{
    PaletteOps *ops = m_doc->paletteOps();
    // One folder model (feat/palette-folders merged): an older show's
    // per-type paths were migrated on load, so a folder is the path under
    // Palettes -- one named like a type is a folder like any other.
    QCOMPARE(PaletteOps::folderOfPath("Palettes/01-Song/", QLCPalette::Color), QString("01-Song"));
    QCOMPARE(PaletteOps::folderOfPath("Palettes/01-Song/Verse/", QLCPalette::Dimmer), QString("01-Song/Verse"));
    QCOMPARE(PaletteOps::folderOfPath("Palettes/Color/", QLCPalette::Dimmer), QString("Color"));
    QCOMPARE(PaletteOps::folderOfPath("Palettes/Colour", QLCPalette::Color), QString("Colour"));
    QCOMPARE(PaletteOps::folderOfPath(QString(), QLCPalette::Dimmer), QString());
    QCOMPARE(PaletteOps::pathForFolder(" 01-Song / Verse "), QString("Palettes/01-Song/Verse/"));

    // Create by type: a start value, a name nobody has, the path given.
    const quint32 c1 = ops->create(QLCPalette::Color, PaletteOps::pathForFolder("01-Song"));
    QVERIFY(c1 != QLCPalette::invalidId());
    QCOMPARE(m_doc->palette(c1)->name(), QString("New Color"));
    QCOMPARE(m_doc->palette(c1)->values().count(), 1);
    const quint32 c2 = ops->create(QLCPalette::Color, QString());
    QCOMPARE(m_doc->palette(c2)->name(), QString("New Color 2"));
    // An older show's palette: its type was its first folder, dropped as
    // the show loads (QLCPalette::migrateLegacyPath).
    QLCPalette *old = new QLCPalette(QLCPalette::Dimmer, m_doc);
    old->setName("Half");
    old->setValue(127);
    old->setPath(QLCPalette::migrateLegacyPath("Palettes/Dimmer/01-Song/", QLCPalette::Dimmer));
    m_doc->addPalette(old);
    QCOMPARE(PaletteOps::folderOf(old), QString("01-Song"));
    QCOMPARE(ops->folders(), QStringList({ "01-Song" }));
    QCOMPARE(ops->folderCount("01-Song"), 2);

    // Rename the folder: both types follow, stored the new way.
    QVERIFY(ops->renameFolder("01-Song", "02-Song"));
    QCOMPARE(m_doc->palette(c1)->path(), QString("Palettes/02-Song/"));
    QCOMPARE(old->path(), QString("Palettes/02-Song/"));
    QCOMPARE(ops->undoText(), QString("Rename folder 01-Song"));
    QVERIFY(ops->undo());
    QCOMPARE(old->path(), QString("Palettes/01-Song/"));            // exactly as it was
    QCOMPARE(m_doc->palette(c1)->path(), QString("Palettes/01-Song/"));
    QVERIFY(ops->redo());
    QCOMPARE(PaletteOps::folderOf(old), QString("02-Song"));

    // Move, rename, duplicate.
    QVERIFY(ops->move({ c2 }, "02-Song/Verse"));
    QCOMPARE(ops->folders(), QStringList({ "02-Song", "02-Song/Verse" }));
    QVERIFY(!ops->move({ c2 }, "02-Song/Verse"));                   // there already: no step
    QVERIFY(ops->rename(c2, "Amber"));
    QVERIFY(!ops->rename(c2, "Amber"));
    const quint32 dup = ops->duplicate(c2);
    QCOMPARE(m_doc->palette(dup)->name(), QString("Amber (copy)"));
    QCOMPARE(PaletteOps::folderOf(m_doc->palette(dup)), QString("02-Song/Verse"));
    QVERIFY(ops->undo());
    QVERIFY(m_doc->palette(dup) == nullptr);
    QVERIFY(ops->redo());
    QVERIFY(m_doc->palette(dup) != nullptr);
    QCOMPARE(m_doc->palette(dup)->name(), QString("Amber (copy)"));

    // An empty folder; delete a folder: what is in it moves up a level.
    QVERIFY(ops->addFolder("Spare"));
    QVERIFY(ops->folders().contains("Spare"));
    QVERIFY(!ops->addFolder("Spare"));
    QVERIFY(ops->removeFolder("02-Song/Verse"));
    QCOMPARE(PaletteOps::folderOf(m_doc->palette(c2)), QString("02-Song"));
    QVERIFY(ops->removeFolder("02-Song"));
    QCOMPARE(PaletteOps::folderOf(m_doc->palette(c2)), QString());
    QCOMPARE(PaletteOps::folderOf(old), QString());
    QCOMPARE(ops->folders(), QStringList({ "Spare" }));
    QVERIFY(ops->undo());
    QVERIFY(ops->undo());
    QCOMPARE(PaletteOps::folderOf(m_doc->palette(c2)), QString("02-Song/Verse"));

    // The tree's one-off values are the tree's: left alone.
    const quint32 inl = m_doc->showTree()->createInlinePalette(QLCPalette::Dimmer, { 10 }, "4% · 1 Towers");
    QVERIFY(!ops->isManaged(inl));
    QVERIFY(!ops->rename(inl, "x"));
    QVERIFY(!ops->remove({ inl }));
}

/* Delete asks (the UI) and goes; undo brings the palette back with its id,
   at its place in every scene and in the effect slot that bound it. */
void ShowTree_Test::paletteOpsUndoBringsADeletedPaletteBack()
{
    PaletteOps *ops = m_doc->paletteOps();
    const quint32 blue = colour("Blue");
    const quint32 full = dimmer(255, "Full");
    Scene *sc = new Scene(m_doc);
    sc->setName("Old");
    sc->addPalette(full);
    sc->addPalette(blue);
    m_doc->addFunction(sc);
    QLCPalette *fx = new QLCPalette(QLCPalette::Effect, m_doc);
    fx->setName("Chase");
    fx->setEffectPaletteBinding("colour1", blue);
    m_doc->addPalette(fx);

    const QList<PaletteOps::Use> uses = ops->usage(blue);
    QCOMPARE(uses.count(), 2);
    QCOMPARE(uses.at(0).label, QString("Scene “Old” — look 2"));
    QCOMPARE(uses.at(1).label, QString("Effect look “Chase” — its “colour1” slot"));
    QCOMPARE(PaletteOps::usageSummary(uses), QString("1 scene(s), 1 effect slot(s)"));

    QVERIFY(ops->remove({ blue }));
    QVERIFY(m_doc->palette(blue) == nullptr);
    QCOMPARE(sc->palettes(), QList<quint32>({ full }));
    QVERIFY(!fx->effectPaletteBindings().contains("colour1"));
    QCOMPARE(ops->undoText(), QString("Delete Blue"));

    QVERIFY(ops->undo());
    QVERIFY(m_doc->palette(blue) != nullptr);
    QCOMPARE(m_doc->palette(blue)->name(), QString("Blue"));
    QCOMPARE(m_doc->palette(blue)->type(), QLCPalette::Color);
    QCOMPARE(sc->palettes(), QList<quint32>({ full, blue }));
    QCOMPARE(fx->effectPaletteBindings().value("colour1"), blue);
    QVERIFY(ops->redo());
    QVERIFY(m_doc->palette(blue) == nullptr);
    QVERIFY(ops->undo());
    QCOMPARE(sc->palettes(), QList<quint32>({ full, blue }));
}

/* Open 26 (Branson 2026-10-18: "delete"): deleting a used palette takes it
   off everything that uses it -- two cues (a layer, a cue look), a reusable
   layer and its use, a look of the Looks section, a plain scene and an
   effect slot -- nothing left naming it ("(missing)"), a look with nothing
   else left gone (not None). One Undo puts back the palette and every use:
   the show saves byte for byte as before and plays the same; redo deletes
   again. */
void ShowTree_Test::deletingAPaletteClearsEveryUse()
{
    PaletteOps *ops = m_doc->paletteOps();
    ShowTreeController *st = m_doc->showTree();
    FixtureGroup *a = group("A", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *b = group("B", { m_fixtures[2], m_fixtures[3] });
    FixtureGroup *c = group("C", { m_fixtures[4], m_fixtures[5] });
    const quint32 half = dimmer(127, "Half");
    const quint32 hot = dimmer(200, "Hot");
    const quint32 blue = colour("Blue");

    const quint32 prod = st->addProduction("P");
    const quint32 listId = st->addCueList(prod, "Full Show");
    // Cue 1: A at Hot (its only look), B at Half.
    const quint32 c1 = st->addCueNumbered(listId, "1", "One");
    const quint32 c1a = st->addLayer(c1, groupTarget(a->id()));
    QVERIFY(st->setLayerLook(c1, c1a, st->lookForPalette(hot)));
    QVERIFY(st->setLayerLook(c1, st->addLayer(c1, groupTarget(b->id())), st->lookForPalette(half)));
    // Cue 2: Hot as the cue look, reaching B (Blue).
    const quint32 c2 = st->addCueNumbered(listId, "2", "Two");
    QVERIFY(st->setCueLook(c2, st->lookForPalette(hot)));
    QVERIFY(st->setLayerLook(c2, st->addLayer(c2, groupTarget(b->id())), st->lookForPalette(blue)));
    // Cue 3: C at Hot, made a reusable layer; used again in cue 4.
    const quint32 c3 = st->addCueNumbered(listId, "3", "Three");
    const quint32 c3c = st->addLayer(c3, groupTarget(c->id()));
    QVERIFY(st->setLayerLook(c3, c3c, st->lookForPalette(hot)));
    const quint32 reusable = st->makeLayerReusable(c3, c3c, "Side");
    QVERIFY(reusable != invalidId());
    const quint32 c4 = st->addCueNumbered(listId, "4", "Four");
    QVERIFY(st->insertReusableLayer(c4, reusable) != invalidId());
    // A look of the Looks section: A at Hot in Blue (Blue stays).
    const quint32 hit = st->addLook(prod, "Hit 1");
    const quint32 hitA = st->addLayer(hit, groupTarget(a->id()));
    QVERIFY(st->setLayerLook(hit, hitA, st->lookForPalette(blue)));
    QVERIFY(st->setLayerLook(hit, hitA, st->lookForPalette(hot)));
    // A plain scene and an effect slot.
    Scene *old = new Scene(m_doc);
    old->setName("Old");
    old->addPalette(half);
    old->addPalette(hot);
    old->addPalette(blue);
    m_doc->addFunction(old);
    QLCPalette *fx = new QLCPalette(QLCPalette::Effect, m_doc);
    fx->setName("Chase");
    fx->setEffectPaletteBinding("dimmer", hot);
    m_doc->addPalette(fx);

    const QList<PaletteOps::Use> uses = ops->usage(hot);
    QCOMPARE(PaletteOps::usageSummary(uses),
             QString("4 cue(s), 1 hit(s), 1 reusable(s), 1 scene(s), 1 effect slot(s)"));

    const CueList *list = st->cueList(listId);
    QVERIFY(list != nullptr);
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(list->chaser));
    QVERIFY(chaser != nullptr);
    const quint32 hitCollection = st->cue(hit)->collection;
    auto playLook = [&]() {
        stopAll();
        m_doc->function(hitCollection)->start(m_doc->masterTimer(), FunctionParent::master());
        const QHash<quint32, uchar> out = settle(10);
        stopAll();
        return out;
    };
    // Anything in the tree still naming @a pid.
    auto named = [&](quint32 pid) {
        int n = 0;
        auto count = [&](const QList<Look> &looks) {
            for (const Look &l : looks)
                n += int(l.palettes.count(pid) + l.own.count(pid));
        };
        for (const Production &p : st->productions())
            for (const CueList &l : p.lists)
                for (const Cue &cue : l.cues)
                {
                    count(cue.looks);
                    for (const Layer &layer : cue.layers)
                        count(layer.looks);
                }
        for (const ReusableLayer &r : st->reusableLayers())
            count(r.looks);
        return n;
    };

    const QByteArray before = engineXml(m_doc);
    const QList<QHash<quint32, uchar>> playedBefore = playAll(chaser);
    const QHash<quint32, uchar> lookBefore = playLook();
    QCOMPARE(playedBefore.at(0).value(0), uchar(200));
    QCOMPARE(lookBefore.value(0), uchar(200));

    QVERIFY(ops->remove({ hot }));
    QVERIFY(m_doc->palette(hot) == nullptr);
    QCOMPARE(named(hot), 0);                                    // no "(missing)"
    QCOMPARE(ops->undoText(), QString("Delete Hot"));
    QCOMPARE(st->sharedUndoText(), QString("Delete Hot"));     // one step
    // Cue 1's A had only Hot: no look left there (not None); B unchanged.
    QVERIFY(st->cue(c1)->layer(c1a)->looks.isEmpty());
    QCOMPARE(st->cue(c1)->layers.at(1).looks.first().palettes, QList<quint32>({ half }));
    QVERIFY(st->cue(c2)->looks.isEmpty());
    QCOMPARE(st->cue(c2)->layers.first().looks.first().palettes, QList<quint32>({ blue }));
    QVERIFY(st->reusableLayer(reusable)->looks.isEmpty());
    QVERIFY(st->cue(c4)->layers.first().looks.isEmpty());
    // The look keeps its Blue, without a Dimmer key.
    QCOMPARE(st->cue(hit)->layer(hitA)->looks.count(), 1);
    QCOMPARE(st->cue(hit)->layer(hitA)->looks.first().palettes, QList<quint32>({ blue }));
    QVERIFY(!st->cue(hit)->layer(hitA)->looks.first().none);
    QCOMPARE(st->cue(hit)->collection, hitCollection);          // VC buttons keep firing it
    QCOMPARE(old->palettes(), QList<quint32>({ half, blue }));
    QVERIFY(!fx->effectPaletteBindings().contains("dimmer"));
    // No generated scene plays it either.
    for (const Function *f : m_doc->functions())
        if (const Scene *s = qobject_cast<const Scene *>(f))
            QVERIFY2(!s->palettes().contains(hot), qPrintable(s->name()));
    const QByteArray deleted = engineXml(m_doc);
    QVERIFY(playAll(chaser).at(0).value(0) != uchar(200));

    // One Undo: the palette and every use, exactly.
    QVERIFY(st->undoShared());
    QVERIFY(m_doc->palette(hot) != nullptr);
    QCOMPARE(QString::fromUtf8(engineXml(m_doc)), QString::fromUtf8(before));
    QCOMPARE(playAll(chaser), playedBefore);
    QCOMPARE(playLook(), lookBefore);
    QCOMPARE(QString::fromUtf8(engineXml(m_doc)), QString::fromUtf8(before));

    // Redo deletes again, as the first time; undo once more.
    QVERIFY(st->redoShared());
    QVERIFY(m_doc->palette(hot) == nullptr);
    QCOMPARE(named(hot), 0);
    QCOMPARE(QString::fromUtf8(engineXml(m_doc)), QString::fromUtf8(deleted));
    QVERIFY(st->undoShared());
    QCOMPARE(QString::fromUtf8(engineXml(m_doc)), QString::fromUtf8(before));
    QCOMPARE(playAll(chaser), playedBefore);
}

/* Used by (P-D13) names cues and looks -- not the scenes the tree made for
   them -- and plain scenes. */
void ShowTree_Test::paletteUsageNamesCuesLooksAndScenes()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    const quint32 blue = colour("Blue");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "Full Show");
    const quint32 c1 = st->addCueNumbered(list, "1.0.0", "Verse");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(towers->id())), st->lookForPalette(blue));
    const quint32 c2 = st->addCueNumbered(list, "1.0.1", "Chorus");
    st->setCueLook(c2, st->lookForPalette(blue));
    const quint32 hit = st->addLook(prod, "Strobe");
    st->setLayerLook(hit, st->addLayer(hit, groupTarget(towers->id())), st->lookForPalette(blue));

    const QList<PaletteOps::Use> uses = m_doc->paletteOps()->usage(blue);
    QStringList labels;
    for (const PaletteOps::Use &u : uses)
        labels << u.label;
    QCOMPARE(labels, QStringList({ "Cue 1.0.0 Verse in Full Show — Towers", "Cue 1.0.1 Chorus in Full Show — cue look",
                                   "Hit Strobe — Towers" }));
    QCOMPARE(uses.at(2).kind, PaletteOps::Use::Look);
    QCOMPARE(uses.at(2).function, st->lookFunction(hit));
    QCOMPARE(PaletteOps::usageSummary(uses), QString("2 cue(s), 1 hit(s)"));
    // The generated scenes play it, but are never listed: their cues are.
    for (const PaletteOps::Use &u : uses)
        QVERIFY(u.kind != PaletteOps::Use::Scene);
}

/* P-D14: the tab's Undo is one history -- tree steps and palette steps,
   undone newest first, redone in the order they were undone; a new step on
   either side makes the other's redo stale. */
void ShowTree_Test::oneHistoryForTreeAndPalettes()
{
    ShowTreeController *st = m_doc->showTree();
    PaletteOps *ops = m_doc->paletteOps();
    const quint32 prod = st->addProduction("P");                         // tree 1
    const quint32 pid = ops->create(QLCPalette::Dimmer, QString(), "Half"); // palette 1
    const quint32 list = st->addCueList(prod, "L");                       // tree 2
    QVERIFY(ops->rename(pid, "Half 2"));                                   // palette 2

    QCOMPARE(st->sharedUndoText(), QString("Rename Half"));
    QVERIFY(st->undoShared());
    QCOMPARE(m_doc->palette(pid)->name(), QString("Half"));
    QVERIFY(st->cueList(list) != nullptr);
    QVERIFY(st->undoShared());                                             // the list
    QVERIFY(st->cueList(list) == nullptr);
    QVERIFY(m_doc->palette(pid) != nullptr);
    QVERIFY(st->undoShared());                                             // the palette
    QVERIFY(m_doc->palette(pid) == nullptr);
    QVERIFY(st->production(prod) != nullptr);

    // Redo: the one undone last first.
    QVERIFY(st->redoShared());
    QVERIFY(m_doc->palette(pid) != nullptr);
    QVERIFY(st->cueList(list) == nullptr);
    QVERIFY(st->redoShared());
    QVERIFY(st->cueList(list) != nullptr);
    QCOMPARE(m_doc->palette(pid)->name(), QString("Half"));
    QVERIFY(st->canRedoShared());

    // A new tree step: the palette rename can't be redone any more.
    st->addCueList(prod, "M");
    QVERIFY(!st->canRedoShared());
    QVERIFY(!ops->canRedo());
    // ...and a palette step clears the tree's redo.
    QVERIFY(st->undoShared());
    QVERIFY(st->canRedo());
    ops->create(QLCPalette::Color, QString());
    QVERIFY(!st->canRedo());
}

void ShowTree_Test::paletteCommands()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    const quint32 blue = colour("Blue");
    ShowTreeController *st = m_doc->showTree();
    PaletteOps *ops = m_doc->paletteOps();
    ShowTreeCommand::Context ctx;
    ctx.production = st->addProduction("P");
    ctx.list = st->addCueList(ctx.production, "L");
    ctx.cue = st->addCueNumbered(ctx.list, "1.0.0", "One");
    ShowTreeCommand cmd(m_doc, st);

    // A new palette, valued and filed; it opens in the Look Editor.
    ShowTreeCommand::Result r = cmd.run("palette colour \"Amber 2\" #ffb000 in \"01-Song\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QLCPalette *amber = m_doc->palette(r.editPalette);
    QVERIFY(amber != nullptr);
    QCOMPARE(amber->name(), QString("Amber 2"));
    QCOMPARE(amber->type(), QLCPalette::Color);
    QCOMPARE(amber->rgbValue(), QColor("#ffb000"));
    QCOMPARE(PaletteOps::folderOf(amber), QString("01-Song"));
    QVERIFY(r.message.contains("in 01-Song"));
    QVERIFY(!cmd.run("palette colour \"Amber 2\"", ctx).ok);           // that name is taken
    // A dimmer by value, named as the show names them.
    r = cmd.run("palette dimmer 50", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(m_doc->palette(r.editPalette)->name(), QString("Dimmer 50%"));
    QCOMPARE(m_doc->palette(r.editPalette)->values().value(0).toInt(), 128);

    // Rename, move, dup, edit.
    QVERIFY(cmd.run("palette \"Amber 2\" rename \"Amber\"", ctx).ok);
    QCOMPARE(amber->name(), QString("Amber"));
    QVERIFY(cmd.run("palette Amber move \"02-Song\"", ctx).ok);
    QCOMPARE(PaletteOps::folderOf(amber), QString("02-Song"));
    QVERIFY(cmd.run("palette Amber move none", ctx).ok);
    QCOMPARE(PaletteOps::folderOf(amber), QString());
    r = cmd.run("palette Amber dup", ctx);
    QVERIFY(r.ok);
    QCOMPARE(m_doc->palette(r.editPalette)->name(), QString("Amber (copy)"));
    r = cmd.run("palette Blue edit", ctx);
    QVERIFY(r.ok);
    QCOMPARE(r.editPalette, blue);

    // Folders.
    QVERIFY(cmd.run("palette folder \"Spare\"", ctx).ok);
    QVERIFY(ops->folders().contains("Spare"));
    QVERIFY(cmd.run("palette folder Spare rename \"Extra\"", ctx).ok);
    QVERIFY(ops->folders().contains("Extra"));
    QVERIFY(cmd.run("palette folder Extra delete", ctx).ok);
    QVERIFY(!ops->folders().contains("Extra"));

    // Delete: refused while used (it says by what), unless anyway.
    QVERIFY(cmd.run("Towers colour Blue", ctx).ok);
    r = cmd.run("palette Blue delete", ctx);
    QVERIFY(!r.ok);
    QVERIFY2(r.message.contains("1 cue(s)"), qPrintable(r.message));
    QVERIFY(m_doc->palette(blue) != nullptr);
    QVERIFY(cmd.run("palette Blue delete anyway", ctx).ok);
    QVERIFY(m_doc->palette(blue) == nullptr);
    // undo (one history) brings it back.
    r = cmd.run("undo", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(m_doc->palette(blue) != nullptr);
    QCOMPARE(st->cue(ctx.cue)->layers.first().looks.first().palettes, QList<quint32>({ blue }));
    Q_UNUSED(towers)

    // Tab.
    QStringList next;
    QVERIFY(cmd.candidates("palette ", ctx, &next));
    QVERIFY(next.contains("colour") && next.contains("folder") && next.contains("Blue"));
    QVERIFY(cmd.candidates("palette Blue ", ctx, &next));
    QCOMPARE(next, QStringList({ "rename", "move", "dup", "delete", "edit" }));
    QVERIFY(cmd.candidates("palette Blue move ", ctx, &next));
    QVERIFY(next.contains("none"));
    QCOMPARE(cmd.complete("palette Blue d", ctx), QString("palette Blue d"));   // dup / delete
    QCOMPARE(cmd.complete("palette Blue du", ctx), QString("palette Blue dup "));
    QCOMPARE(cmd.complete("palette fo", ctx), QString("palette folder "));
    QVERIFY(ShowTreeCommand::helpText("palette").contains("<b>palette</b>"));
}

/****************************************************************************
 * R3: the Looks section
 ****************************************************************************/

/* A look (P-D5) is built as a cue is, and bakes to one Collection of its
   own: shown (VC function pickers list it by name), filed under
   Productions/<p>/Hits/<folder>, locked; no chaser. Its id stays as it is
   edited, so a VC button keeps firing it. */
void ShowTree_Test::aLookBakesToACollectionOfItsOwn()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0], m_fixtures[1] });
    FixtureGroup *usts = group("USTs", { m_fixtures[2] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("DanceHysteria");
    QCOMPARE(st->looksSection(prod), invalidId());

    const quint32 hit = st->addLook(prod, "Hit 1", "Hits");
    const quint32 section = st->looksSection(prod);
    QVERIFY(section != invalidId());
    QVERIFY(st->isLooks(section));
    QVERIFY(st->isLook(hit));
    QCOMPARE(st->cueList(section)->name, QString("Hits"));
    QCOMPARE(st->undoText(), QString("New hit"));                  // section and hit: one step
    QCOMPARE(st->cueList(section)->chaser, invalidId());

    const quint32 l1 = st->addLayer(hit, groupTarget(towers->id()));
    st->setLayerLook(hit, l1, st->lookForPalette(full));
    const quint32 fid = st->lookFunction(hit);
    Collection *coll = qobject_cast<Collection *>(m_doc->function(fid));
    QVERIFY(coll != nullptr);
    QVERIFY(st->isGenerated(fid));
    QVERIFY(coll->isVisible());
    QCOMPARE(coll->name(), QString("Hit 1"));
    QCOMPARE(coll->path(true), QString("Productions/DanceHysteria/Hits/Hits"));
    QCOMPARE(coll->functions().count(), 1);
    QCOMPARE(st->describeGenerated(fid), QString("Hit Hit 1 in DanceHysteria"));
    QCOMPARE(st->lookOfFunction(fid), hit);
    for (const Function *f : m_doc->functions())
        QVERIFY(f->type() != Function::ChaserType);

    // Edited: the same Collection, new members.
    st->setLayerLook(hit, st->addLayer(hit, groupTarget(usts->id())), st->lookForPalette(half));
    QCOMPARE(st->lookFunction(hit), fid);
    QCOMPARE(coll->functions().count(), 2);
    st->renameNode(hit, "Big Hit");
    QCOMPARE(coll->name(), QString("Big Hit"));
    QVERIFY(st->setLookFolder({ hit }, QString()));
    QCOMPARE(coll->path(true), QString("Productions/DanceHysteria/Hits"));

    // It plays what it has.
    coll->start(m_doc->masterTimer(), FunctionParent(FunctionParent::ManualVCWidget, 7));
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[2]), 127);
    coll->stop(FunctionParent(FunctionParent::ManualVCWidget, 7));
    v = settle();
    QCOMPARE(int(v[0]), 0);

    // Selected in Design: it previews; in Perform nothing does.
    st->previewCue(hit);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
    QCOMPARE(int(settle()[0]), 255);
    m_doc->setMode(Doc::Operate);
    QCoreApplication::processEvents();
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::NotDesign));
    QCOMPARE(int(settle()[0]), 0);
    st->stopPreview();
    m_doc->setMode(Doc::Design);

    // Deleted: its Collection goes with it.
    QVERIFY(st->removeCue(hit));
    QVERIFY(m_doc->function(fid) == nullptr);
}

/* A look never tracks (P-D5): the second look starts from nothing, and the
   operations that would make it track are refused. */
void ShowTree_Test::looksNeverTrack()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 a = st->addLook(prod, "A");
    st->setLayerLook(a, st->addLayer(a, groupTarget(towers->id())), st->lookForPalette(full));
    const quint32 b = st->addLook(prod, "B");
    const quint32 section = st->looksSection(prod);
    QCOMPARE(st->cueList(section)->indexOf(b), 1);
    const QList<ResolvedCue> r = st->resolved(*st->cueList(section));
    QCOMPARE(r.at(1).layers.count(), 0);                        // nothing carried in
    QCOMPARE(effectiveMode(*st->cueList(section), *st->cue(b)), CueMode::Block);

    QVERIFY(!st->setCueMode(b, CueMode::Track));
    QVERIFY(!st->setListMode(section, ListMode::Tracking));
    QVERIFY(!st->setBaseCue(section, a));
    QString why;
    QVERIFY(!st->addInclude(b, a, &why));
    QVERIFY(why.contains("no includes"));
    QVERIFY(!st->planConvertToTracking(section).ok);
    QCOMPARE(st->detachList(section), Function::invalidId());
    QVERIFY(!st->renameNode(section, "Other"));
    // A cue pasted into the section is a look: its mode and includes go.
    const quint32 list = st->addCueList(prod, "L");
    const quint32 c1 = st->addCueNumbered(list, "1", "One");
    const quint32 c2 = st->addCueNumbered(list, "2", "Two");
    st->setCueMode(c2, CueMode::CueOnly);
    st->addInclude(c2, c1);
    const QList<quint32> pasted = st->pasteCues({ c2 }, b);
    QCOMPARE(pasted.count(), 1);
    QCOMPARE(st->cue(pasted.first())->mode, CueMode::ListDefault);
    QVERIFY(st->cue(pasted.first())->includes.isEmpty());
    // And a look pasted into a list leaves its folder behind.
    st->setLookFolder({ a }, "Hits");
    const QList<quint32> back = st->pasteCues({ a }, c2);
    QVERIFY(st->cue(back.first())->folder.isEmpty());
}

/* P-D6: a look fired over a running cue list mixes as any function does --
   intensity HTP, the higher wins; stopped, the list's value is back. */
void ShowTree_Test::aLookPlaysOverARunningListHtpLtp()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(towers->id())), st->lookForPalette(half));
    const quint32 hit = st->addLook(prod, "Hit");
    st->setLayerLook(hit, st->addLayer(hit, groupTarget(towers->id())), st->lookForPalette(full));
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    Function *look = m_doc->function(st->lookFunction(hit));
    const FunctionParent button(FunctionParent::ManualVCWidget, 3);

    go(chaser);
    QCOMPARE(int(settle()[0]), 127);
    look->start(m_doc->masterTimer(), button);
    QCOMPARE(int(settle()[0]), 255);                        // HTP: the look is higher
    look->stop(button);
    QCOMPARE(int(settle()[0]), 127);                        // the list's again
    QVERIFY(chaser->isRunning());

    // A look lower than the list: HTP keeps the list's level.
    st->setLayerLook(hit, st->cue(hit)->layers.first().id, st->lookForPalette(half));
    st->setLayerLook(c1, st->cue(c1)->layers.first().id, st->lookForPalette(full));
    chaser->stop(FunctionParent::master());
    tick(3);
    go(chaser);
    look->start(m_doc->masterTimer(), button);
    QCOMPARE(int(settle()[0]), 255);
    look->stop(button);
    QCOMPARE(int(settle()[0]), 255);
    // listsRunning() and runningCue() never count the Looks section.
    QCOMPARE(st->listsRunning(), QStringList({ "L" }));
}

void ShowTree_Test::lookFoldersSaveLoadAndUndo()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 a = st->addLook(prod, "A", "Hits/Big");
    const quint32 b = st->addLook(prod, "B", "Hits");
    const quint32 c = st->addLook(prod, "C");
    st->setLayerLook(a, st->addLayer(a, groupTarget(towers->id())), st->lookForPalette(full));
    const quint32 section = st->looksSection(prod);
    QCOMPARE(st->lookFolders(section), QStringList({ "Hits", "Hits/Big" }));
    QCOMPARE(st->looksIn(section, "Hits"), QList<quint32>({ a, b }));
    QCOMPARE(st->looksIn(section, QString()), QList<quint32>({ a, b, c }));
    QVERIFY(st->addLookFolder(section, "Ballads"));
    QVERIFY(!st->addLookFolder(section, "Ballads"));
    QCOMPARE(st->lookFolders(section), QStringList({ "Ballads", "Hits", "Hits/Big" }));

    // Rename a folder: what is under it follows.
    QVERIFY(st->renameLookFolder(section, "Hits", "Stabs"));
    QCOMPARE(st->cue(a)->folder, QString("Stabs/Big"));
    QCOMPARE(st->cue(b)->folder, QString("Stabs"));
    QVERIFY(st->undo());
    QCOMPARE(st->cue(a)->folder, QString("Hits/Big"));
    QVERIFY(st->redo());
    // Delete one: its hits move up.
    QVERIFY(st->removeLookFolder(section, "Stabs"));
    QCOMPARE(st->cue(a)->folder, QString("Big"));
    QCOMPARE(st->cue(b)->folder, QString());
    QVERIFY(st->undo());

    // Saved and read back: the section, its folders (empty ones too) and
    // its looks; the Collection is the same.
    const quint32 fid = st->lookFunction(a);
    QByteArray xml;
    {
        QBuffer buf(&xml);
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        w.writeStartDocument();
        st->saveXML(&w);
        w.writeEndDocument();
    }
    QVERIFY(xml.contains("<Looks "));
    QVERIFY(xml.contains("Folder=\"Stabs/Big\""));
    QVERIFY(xml.contains("<Folder Path=\"Ballads\"/>"));
    st->reset();
    QXmlStreamReader r(xml);
    while (!r.atEnd() && !r.isStartElement())
        r.readNext();
    QVERIFY(st->loadXML(r));
    const quint32 section2 = st->looksSection(prod);
    QCOMPARE(section2, section);
    QVERIFY(st->isLook(a));
    QCOMPARE(st->cue(a)->folder, QString("Stabs/Big"));
    QCOMPARE(st->lookFolders(section2), QStringList({ "Ballads", "Stabs", "Stabs/Big" }));
    QCOMPARE(st->lookFunction(a), fid);
    st->bake();
    QCOMPARE(st->lookFunction(a), fid);
}

/* P-D16: a scene of groups + palettes reads as a layer; one of raw values
   is played as it is. The scene stays. */
void ShowTree_Test::adoptASceneAsALook()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");

    Scene *busk = new Scene(m_doc);
    busk->setName("Busk Hit");
    busk->addFixtureGroup(towers->id());
    busk->addPalette(full);
    m_doc->addFunction(busk);
    Scene *raw = new Scene(m_doc);
    raw->setName("Haze");
    raw->setValue(m_fixtures[5], 0, 200);
    m_doc->addFunction(raw);

    QString why;
    const quint32 a = st->adoptAsLook(prod, busk->id(), &why);
    QVERIFY(a != invalidId());
    QVERIFY2(why.isEmpty(), qPrintable(why));
    QCOMPARE(st->cue(a)->name, QString("Busk Hit"));
    QCOMPARE(st->cue(a)->layers.count(), 1);
    QCOMPARE(st->cue(a)->layers.first().target.groups, QList<quint32>({ towers->id() }));
    QCOMPARE(st->cue(a)->layers.first().looks.first().palettes, QList<quint32>({ full }));
    QCOMPARE(st->undoText(), QString("Adopt Busk Hit as a hit"));
    QVERIFY(m_doc->function(busk->id()) != nullptr);

    const quint32 h = st->adoptAsLook(prod, raw->id(), &why);
    QVERIFY(h != invalidId());
    QVERIFY(!why.isEmpty());
    QCOMPARE(st->cue(h)->layers.first().target.function, raw->id());
    // Generated functions are the tree's already.
    QVERIFY(st->adoptAsLook(prod, st->lookFunction(a), &why) == invalidId());

    // Both play as the scenes did.
    Function *f = m_doc->function(st->lookFunction(a));
    f->start(m_doc->masterTimer(), FunctionParent::master());
    QCOMPARE(int(settle()[0]), 255);
}

/* Record as new cue "everything lit" takes a look fired from VC as the
   look's layers, not its generated scenes. */
void ShowTree_Test::recordLitTakesARunningLooksLayers()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "L");
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 hit = st->addLook(prod, "Hit");
    st->setLayerLook(hit, st->addLayer(hit, groupTarget(towers->id())), st->lookForPalette(full));
    QVERIFY(!st->canRecord(c1, ShowTreeController::RecordSource::Lit));
    Function *look = m_doc->function(st->lookFunction(hit));
    look->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    const QList<Layer> layers = st->recordLayers(c1, ShowTreeController::RecordSource::Lit);
    QCOMPARE(layers.count(), 1);
    QCOMPARE(layers.first().target.groups, QList<quint32>({ towers->id() }));
    QVERIFY(!layers.first().target.isOpaque());
    QCOMPARE(layers.first().looks.first().palettes, QList<quint32>({ full }));
}

void ShowTree_Test::lookCommands()
{
    FixtureGroup *towers = group("Towers", { m_fixtures[0] });
    dimmer(255, "Full");
    Scene *old = new Scene(m_doc);
    old->setName("Old Busk");
    old->addFixtureGroup(towers->id());
    m_doc->addFunction(old);
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand::Context ctx;
    ShowTreeCommand cmd(m_doc, st);
    QVERIFY(!cmd.run("look \"Hit 1\"", ctx).ok);              // no production yet
    ctx.production = st->addProduction("P");

    ShowTreeCommand::Result r = cmd.run("look \"Hit 1\" in \"Hits\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    const quint32 hit = r.context.cue;
    QVERIFY(st->isLook(hit));
    QCOMPARE(st->cue(hit)->folder, QString("Hits"));
    // Its layers as on a cue.
    r = cmd.run("Towers dimmer Full", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(hit)->layers.count(), 1);
    // The same name selects it; a new one goes after it, in its folder.
    r = cmd.run("look \"Hit 1\"", ctx);
    QCOMPARE(r.context.cue, hit);
    r = cmd.run("look Hit2", r.context);
    QVERIFY(r.ok);
    QCOMPARE(st->cue(r.context.cue)->folder, QString("Hits"));
    QCOMPARE(st->cueList(st->looksSection(ctx.production))->indexOf(r.context.cue), 1);
    // Mode words don't apply.
    QVERIFY(!cmd.run("track", r.context).ok || st->cue(r.context.cue)->mode == CueMode::ListDefault);
    QCOMPARE(st->cue(r.context.cue)->mode, CueMode::ListDefault);

    // vcpage asks the UI's maker, with the selected look's folder.
    QVERIFY(!cmd.run("vcpage", r.context).ok);                // no VC here
    quint32 madeList = invalidId();
    QString madeFolder("-");
    cmd.setVcPageMaker([&](quint32 listId, const QString &folder, QString *msg) {
        madeList = listId;
        madeFolder = folder;
        *msg = "VC page Looks: 2 buttons";
        return true;
    });
    r = cmd.run("vcpage", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(madeList, st->looksSection(ctx.production));
    QCOMPARE(madeFolder, QString("Hits"));

    // adopt.
    r = cmd.run("adopt \"Old Busk\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(r.context.cue)->name, QString("Old Busk"));

    // Tab.
    QStringList next;
    QVERIFY(cmd.candidates("look ", ctx, &next));
    QVERIFY(next.contains("Hit 1"));
    QVERIFY(cmd.candidates("look \"Hit 1\" in ", ctx, &next));
    QCOMPARE(next, QStringList({ "Hits" }));
    QCOMPARE(cmd.complete("vcp", ctx), QString("vcpage "));
    QCOMPARE(cmd.complete("adopt Old", ctx), QString("adopt Old Busk "));
    for (const char *w : { "look", "vcpage", "adopt", "palette" })
        QVERIFY2(ShowTreeCommand::helpText(w).contains(QString("<b>%1</b>").arg(w)), w);
}

/* CHANGELOG 0.2.2 (Q77): the busking Looks are now Hits -- `hit "name"`
   selects or makes a hit, `look` still works -- and the hit plays as its
   layers say. */
void ShowTree_Test::hitCommandMakesAndPlaysAHit()
{
    group("Towers", { m_fixtures[0] });
    dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand::Context ctx;
    ShowTreeCommand cmd(m_doc, st);
    ctx.production = st->addProduction("P");

    ShowTreeCommand::Result r = cmd.run("hit \"Blast\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    const quint32 hit = r.context.cue;
    QVERIFY(st->isLook(hit));
    QCOMPARE(st->cue(hit)->name, QString("Blast"));
    // Made in a new section, named Hits.
    QCOMPARE(st->cueList(st->looksSection(ctx.production))->name, QString("Hits"));
    r = cmd.run("Towers dimmer Full", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));

    // hit and look name the same one.
    QCOMPARE(cmd.run("hit Blast", ctx).context.cue, hit);
    QCOMPARE(cmd.run("look \"Blast\"", ctx).context.cue, hit);
    QCOMPARE(st->cueList(st->looksSection(ctx.production))->cues.count(), 1);

    // Fired as a VC button fires it: Towers at full.
    const quint32 fired = st->lookFunction(hit);
    QVERIFY(m_doc->function(fired) != nullptr);
    stopAll();
    QCOMPARE(settle(5).value(0), uchar(0));
    m_doc->function(fired)->start(m_doc->masterTimer(), FunctionParent::master());
    QCOMPARE(settle(10).value(0), uchar(255));
    stopAll();
}

/* CHANGELOG 0.2.2 (Q77): "New sections are named Hits; older shows open as
   they were": a section an older build saved as "Looks" keeps that name
   through a load and a save. */
void ShowTree_Test::anOlderLooksSectionKeepsItsName()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    QVERIFY(st->addLook(prod, "Blast") != invalidId());
    const quint32 section = st->looksSection(prod);
    QCOMPARE(st->cueList(section)->name, QString("Hits"));
    // As an older build wrote it.
    QByteArray xml = engineXml(m_doc);
    const QByteArray hits = QByteArray("<Looks ID=\"") + QByteArray::number(section) + "\" Name=\"Hits\"";
    QVERIFY2(xml.contains(hits), xml.constData());
    xml.replace(hits, QByteArray("<Looks ID=\"") + QByteArray::number(section) + "\" Name=\"Looks\"");

    QScopedPointer<Doc> other(new Doc(this));
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other->loadXML(r, false));
    ShowTreeController *ost = other->showTree();
    QCOMPARE(ost->looksSection(prod), section);
    QCOMPARE(ost->cueList(section)->name, QString("Looks"));
    QCOMPARE(ost->cueList(section)->cues.count(), 1);
    // A new hit goes in it, under its old name.
    QVERIFY(ost->addLook(prod, "Second") != invalidId());
    QCOMPARE(ost->looksSection(prod), section);
    QCOMPARE(ost->cueList(section)->name, QString("Looks"));
    // Saved again as it was.
    const QByteArray again = engineXml(other.data());
    QVERIFY(again.contains(QByteArray("<Looks ID=\"") + QByteArray::number(section) + "\" Name=\"Looks\""));
    QVERIFY(!again.contains("Name=\"Hits\""));
}

/* A cue list's text colour reaches its cues (and their sub-cues) that have
   none of their own; a cue's own wins. One undo step each; saved as
   Colour= and read back. Nothing is baked differently. */
void ShowTree_Test::textColoursInheritSaveLoadAndUndo()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    const quint32 sub = st->addSubCue(c2, "Hit");
    QVERIFY(sub != invalidId());
    QCOMPARE(st->cueColour(c1), QString());

    QVERIFY(st->setListColour(listId, "Blue"));
    QCOMPARE(st->undoText(), QString("Cue list colour"));
    QCOMPARE(st->listColour(listId), QString("#1e88e5"));
    QCOMPARE(st->cueColour(c1), QString("#1e88e5"));
    QCOMPARE(st->cueColour(sub), QString("#1e88e5"));     // via its cue

    QVERIFY(st->setCuesColour({ c2 }, "#FF0000"));
    QCOMPARE(st->undoText(), QString("Cue colour"));
    QCOMPARE(st->cueColour(c2), QString("#ff0000"));       // its own wins
    QCOMPARE(st->cueColour(sub), QString("#ff0000"));      // a sub-cue: its cue's
    QCOMPARE(st->cueColour(c3), QString("#1e88e5"));
    QVERIFY(st->setCuesColour({ sub }, "green"));
    QCOMPARE(st->cueColour(sub), QString("#43a047"));
    QVERIFY(!st->setCuesColour({ c2 }, "#ff0000"));       // no change: no step
    QVERIFY(!st->setCuesColour({ c2 }, "not a colour"));

    // Several cues: one step.
    QVERIFY(st->setCuesColour({ c1, c3 }, "Yellow"));
    QCOMPARE(st->undoText(), QString("Colour of 2 cues"));
    QVERIFY(st->undo());
    QCOMPARE(st->cueColour(c1), QString("#1e88e5"));
    QCOMPARE(st->cueColour(c3), QString("#1e88e5"));
    QVERIFY(st->redo());
    QCOMPARE(st->cueColour(c3), QString("#fdd835"));

    // Only how it reads: the chaser and collections are as they were.
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QVERIFY(ch != nullptr);
    QCOMPARE(ch->stepsCount(), 3);

    const QByteArray xml = engineXml(m_doc);
    QVERIFY(xml.contains("Colour=\"#1e88e5\""));
    QVERIFY(xml.contains("Colour=\"#ff0000\""));
    Doc *other = new Doc(this);
    {
        QXmlStreamReader r(xml);
        r.readNextStartElement();
        QVERIFY(other->loadXML(r, false));
    }
    QCOMPARE(other->showTree()->listColour(listId), QString("#1e88e5"));
    QCOMPARE(other->showTree()->cueColour(c2), QString("#ff0000"));
    QCOMPARE(other->showTree()->cueColour(c1), QString("#fdd835"));
    QCOMPARE(other->showTree()->cueColour(sub), QString("#43a047"));
    QCOMPARE(productionXml(other->showTree()->productions().first()), productionXml(st->productions().first()));
    delete other;

    // None: the list's again; the list's none: the theme's.
    QVERIFY(st->setCuesColour({ c2 }, QString()));
    QCOMPARE(st->cueColour(c2), QString("#1e88e5"));
    QVERIFY(st->setListColour(listId, "none"));
    QCOMPARE(st->cueColour(c2), QString());
    QVERIFY(st->undo());
    QCOMPARE(st->cueColour(c2), QString("#1e88e5"));
}

/* An older build doesn't know Colour=: the show it reads is the same show
   but for the colours -- the XML differs by the attributes only, and the
   generated functions not at all. */
void ShowTree_Test::textColoursAnOlderReaderIgnores()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(half));
    const QByteArray plain = engineXml(m_doc);

    QVERIFY(st->setListColour(listId, "Orange"));
    QVERIFY(st->setCuesColour({ c2 }, "Purple"));
    const QByteArray coloured = engineXml(m_doc);
    QVERIFY(coloured != plain);
    QByteArray stripped = coloured;
    stripped.replace(" Colour=\"#fb8c00\"", "");
    stripped.replace(" Colour=\"#8e24aa\"", "");
    QCOMPARE(QString::fromUtf8(stripped), QString::fromUtf8(plain));

    // Read as it is without them: no colour anywhere, the same chaser.
    Doc *older = new Doc(this);
    {
        QXmlStreamReader r(stripped);
        r.readNextStartElement();
        QVERIFY(older->loadXML(r, false));
    }
    for (const Production &p : older->showTree()->productions())
        for (const CueList &l : p.lists)
        {
            QVERIFY(l.colour.isEmpty());
            for (const Cue &c : l.cues)
                QVERIFY(c.colour.isEmpty());
        }
    QCOMPARE(older->showTree()->cueList(listId)->chaser, st->cueList(listId)->chaser);
    delete older;

    // A Colour= that isn't one is left out.
    QByteArray bad = coloured;
    bad.replace("Colour=\"#8e24aa\"", "Colour=\"chartreuse-ish\"");
    Doc *other = new Doc(this);
    {
        QXmlStreamReader r(bad);
        r.readNextStartElement();
        QVERIFY(other->loadXML(r, false));
    }
    QCOMPARE(other->showTree()->cueColour(c2), QString("#fb8c00"));  // the list's
    delete other;
}

/* textcolour red · textcolour blue list · a range · none; one step each,
   Tab offers the colours, then list / cue numbers. */
void ShowTree_Test::textColourTyped()
{
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = st->addProduction("P");
    ShowTreeCommand::Result r = cmd.run("list \"Show\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx = r.context;
    QList<quint32> c;
    for (int i = 1; i <= 4; ++i)
        c << st->addCueNumbered(ctx.list, QString("0.0.%1").arg(i), QString("C%1").arg(i));

    ctx.cue = c[0];
    r = cmd.run("textcolour red", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cueColour(c[0]), QString("#e53935"));
    QCOMPARE(st->undoText(), QString("textcolour red"));

    r = cmd.run("textcolor blue list", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->listColour(ctx.list), QString("#1e88e5"));
    QCOMPARE(st->cueColour(c[0]), QString("#e53935"));
    QCOMPARE(st->cueColour(c[1]), QString("#1e88e5"));

    r = cmd.run("textcolour #33aa55 0.0.2 thru 0.0.4", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    for (int i = 1; i < 4; ++i)
        QCOMPARE(st->cueColour(c[i]), QString("#33aa55"));
    QVERIFY(st->undo());                                 // one step for the range
    for (int i = 1; i < 4; ++i)
        QCOMPARE(st->cueColour(c[i]), QString("#1e88e5"));
    QVERIFY(st->redo());

    r = cmd.run("textcolour none 0.0.3", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cueColour(c[2]), QString("#1e88e5"));

    // Wrong: nothing changes.
    const QString before = st->undoText();
    r = cmd.run("textcolour mauve", ctx);
    QVERIFY(!r.ok);
    r = cmd.run("textcolour", ctx);
    QVERIFY(!r.ok);
    r = cmd.run("textcolour red 9.9.9", ctx);
    QVERIFY(!r.ok);
    QCOMPARE(st->undoText(), before);

    // Tab: the colours, then list or cue numbers (and thru).
    QStringList options;
    QVERIFY(cmd.candidates("textcolour ", ctx, &options));
    QVERIFY(options.contains("red"));
    QVERIFY(options.contains("none"));
    QVERIFY(cmd.candidates("textcolour red ", ctx, &options));
    QVERIFY(options.contains("list"));
    QVERIFY(options.contains("0.0.3"));
    QVERIFY(cmd.candidates("textcolour red 0.0.3 ", ctx, &options));
    QVERIFY(options.contains("thru"));
    QVERIFY(ShowTreeCommand::helpText("textcolour").contains("list"));
    QVERIFY(ShowTreeCommand::grammar().contains("textcolour"));
}

/* Every offered colour, on a white list and a black one (and mid greys):
   itself when it reads, else the same hue lighter or darker until it does
   (4.5:1). */
void ShowTree_Test::textColoursAreMadeReadable()
{
    const QList<QColor> backgrounds = { Qt::white, Qt::black, QColor("#d4d4d4"), QColor("#252526"),
                                        QColor("#150a05"), QColor("#2e2920"), QColor("#808080") };
    for (const TextColour &tc : textColours())
    {
        QString hex;
        QVERIFY(parseTextColour(tc.name.toUpper(), &hex));
        QCOMPARE(hex, tc.hex);
        QCOMPARE(textColourName(tc.hex), tc.name);
        for (const QColor &bg : backgrounds)
        {
            const QColor c = readableOn(QColor(tc.hex), bg);
            QVERIFY2(contrastRatio(c, bg) >= 4.5, qPrintable(QString("%1 on %2: %3").arg(tc.name, bg.name())
                                                             .arg(contrastRatio(c, bg))));
            if (contrastRatio(QColor(tc.hex), bg) >= 4.5)
                QCOMPARE(c, QColor(tc.hex));            // kept as it is when it reads
            else if (QColor(tc.hex).hslSaturationF() > 0.1 && c != QColor(Qt::white) && c != QColor(Qt::black))
                QVERIFY(qAbs(c.hslHueF() - QColor(tc.hex).hslHueF()) < 0.02);   // the same colour
        }
    }
    QString hex = "x";
    QVERIFY(parseTextColour("none", &hex));
    QVERIFY(hex.isEmpty());
    QVERIFY(parseTextColour("#abc", &hex));
    QCOMPARE(hex, QString("#aabbcc"));
    QVERIFY(!parseTextColour("teal-ish", &hex));
    QCOMPARE(textColourName("#123456"), QString("#123456"));
}

QTEST_MAIN(ShowTree_Test)

/****************************************************************************
 * Answers 12-20 (2026-10-18)
 ****************************************************************************/

/* Open 13: one precedence rule up and down the chain. For a fixture and an
   attribute the most specific source wins -- tracked < included < cue look
   < layer < child layer (< an effect's own looks, inside the effect) --
   then, between layers of the same source, the deeper (a child) and then
   the lower one; a later cue's tracked value over an earlier one's. Each
   row is played by GO, as the show plays it; a Mix cue (an import) mixes. */
void ShowTree_Test::oneRuleUpAndDownTheChain()
{
    FixtureGroup *all = group("All", { m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[3] });
    FixtureGroup *some = group("Some", { m_fixtures[2], m_fixtures[3] });
    FixtureGroup *one = group("One", { m_fixtures[2] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    const quint32 d45 = dimmer(115, "45");
    const quint32 low = dimmer(40, "Low");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "Chain");      // Tracking
    QList<quint32> q;
    for (int i = 0; i < 11; ++i)
        q << st->addCue(listId, i - 1, QString("Q%1").arg(i + 1));
    auto own = [&](int i, FixtureGroup *g, quint32 pal) {
        const quint32 l = st->addLayer(q[i], groupTarget(g->id()));
        if (pal != invalidId())
            st->setLayerLook(q[i], l, st->lookForPalette(pal));
        return l;
    };
    // Q1 All half; Q2 One low (tracked: the later cue's value wins on One)
    own(0, all, half);
    own(1, one, low);
    // Q3 Block All full; Q4 includes Q1 (included beats tracked)
    QVERIFY(st->setCueMode(q[2], CueMode::Block));
    own(2, all, full);
    QVERIFY(st->addInclude(q[3], q[0]));
    // Q5 includes Q1 + a cue look 45 on its own (empty) All: the cue look
    // beats the include -- although lower (not HTP)
    QVERIFY(st->addInclude(q[4], q[0]));
    own(4, all, invalidId());
    QVERIFY(st->setCueLook(q[4], st->lookForPalette(d45)));
    // Q6 the layer's own Low beats the cue look 45
    own(5, all, low);
    QVERIFY(st->setCueLook(q[5], st->lookForPalette(d45)));
    // Q7 All full, child One low: the child beats its parent
    {
        const quint32 parent = own(6, all, full);
        const quint32 child = st->addChildLayer(q[6], parent, groupTarget(one->id()));
        QVERIFY(child != invalidId());
        QVERIFY(st->setLayerLook(q[6], child, st->lookForPalette(low)));
    }
    // Q8 Block: All full over Some half (lower sibling wins)
    QVERIFY(st->setCueMode(q[7], CueMode::Block));
    own(7, all, full);
    own(7, some, half);
    // Q9 Block: All low with child One full; Some half below: the child
    // (deeper) beats the lower sibling, the lower sibling beats All
    QVERIFY(st->setCueMode(q[8], CueMode::Block));
    {
        const quint32 parent = own(8, all, low);
        const quint32 child = st->addChildLayer(q[8], parent, groupTarget(one->id()));
        QVERIFY(st->setLayerLook(q[8], child, st->lookForPalette(full)));
        own(8, some, half);
    }
    // Q10 Block: All's own Full over the cue look reaching Some below
    QVERIFY(st->setCueMode(q[9], CueMode::Block));
    own(9, all, full);
    own(9, some, invalidId());
    QVERIFY(st->setCueLook(q[9], st->lookForPalette(d45)));
    // Q11 Block, Mix (an import that would change on the rule): HTP
    QVERIFY(st->setCueMode(q[10], CueMode::Block));
    own(10, all, full);
    own(10, some, half);
    QVERIFY(st->setCueMixOverlap(q[10], true));

    // f0 (All only), f2 (All, Some, One), f3 (All, Some)
    const QList<QList<int>> want = {
        { 127, 127, 127 },      // Q1
        { 127,  40, 127 },      // Q2 later tracked over earlier tracked
        { 255, 255, 255 },      // Q3 Block
        { 127, 127, 127 },      // Q4 included > tracked
        { 115, 115, 115 },      // Q5 cue look > included
        {  40,  40,  40 },      // Q6 layer > cue look
        { 255,  40, 255 },      // Q7 child > layer
        { 255, 127, 127 },      // Q8 lower sibling
        {  40, 255, 127 },      // Q9 deeper over lower, lower over upper
        { 255, 255, 255 },      // Q10 own over a cue look on a lower layer
        { 255, 255, 255 },      // Q11 Mix: highest wins
    };
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QVERIFY(ch != nullptr);
    const QList<QHash<quint32, uchar>> played = playAll(ch);
    QCOMPARE(played.count(), want.count());
    for (int i = 0; i < want.count(); ++i)
    {
        const QList<int> got { played[i].value(0), played[i].value(2), played[i].value(3) };
        QVERIFY2(got == want[i], qPrintable(QString("Q%1: f0/f2/f3 %2/%3/%4, want %5/%6/%7").arg(i + 1)
                                            .arg(got[0]).arg(got[1]).arg(got[2])
                                            .arg(want[i][0]).arg(want[i][1]).arg(want[i][2])));
    }
}

/* Open 16: a child's Dimmer dims the colour its parent gives fixtures with
   no dimmer channel (an RGB bar): the child plays the parent's colour there
   itself, scaled by its own Dimmer; the parent leaves those heads out. */
void ShowTree_Test::aChildDimmerDimsItsParentsColourWithoutADimmer()
{
    QList<quint32> bars;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *bar = new Fixture(m_doc);
        bar->setName(QString("Bar %1").arg(i + 1));
        QLCFixtureDef *def = bar->genericRGBPanelDef(1, Fixture::RGB, false);
        QLCFixtureMode *mode = bar->genericRGBPanelMode(def, Fixture::RGB, false, 100, 100);
        bar->setFixtureDefinition(def, mode);
        bar->setUniverse(0);
        bar->setAddress(quint32(100 + i * 3));
        m_doc->addFixture(bar);
        QVERIFY(!QLCPalette::fixtureHasDimmer(bar));
        bars << bar->id();
    }
    FixtureGroup *row = group("Bars", bars);
    FixtureGroup *first = group("Bar 1", { bars[0] });
    const quint32 blue = colour("Blue");             // B = 255
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 parent = st->addLayer(c1, groupTarget(row->id()));
    QVERIFY(st->setLayerLook(c1, parent, st->lookForPalette(blue)));
    QVERIFY(st->setLayerLook(c1, parent, st->lookForPalette(full)));
    const quint32 child = st->addChildLayer(c1, parent, groupTarget(first->id()));
    QVERIFY(st->setLayerLook(c1, child, st->lookForPalette(half)));

    auto blueOf = [this](int bar) {
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        const int v = uchar(ua.first()->preGMValues().at(100 + bar * 3 + 2));
        m_doc->inputOutputMap()->releaseUniverses(false);
        return v;
    };
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    go(ch);
    tick(10);
    QCOMPARE(blueOf(0), 127);              // the child's Half dims the parent's Blue
    QCOMPARE(blueOf(1), 255);              // the parent's own Full
    stopAll();

    // A lower sibling's Dimmer, the same rule: it dims the Blue above it.
    QVERIFY(st->removeLayer(c1, child));
    const quint32 sib = st->addLayer(c1, groupTarget(first->id()));
    QVERIFY(st->setLayerLook(c1, sib, st->lookForPalette(half)));
    ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    go(ch);
    tick(10);
    QCOMPARE(blueOf(0), 127);
    QCOMPARE(blueOf(1), 255);
    stopAll();
}

/* Open 15: the Design preview puts out what GO plays, cue for cue --
   overlapping layers, a child, a cue look, a Mix cue (HTP among the
   preview's own scenes, though they win over a running list). */
void ShowTree_Test::previewPlaysWhatGoPlays()
{
    FixtureGroup *all = group("All", { m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[3] });
    FixtureGroup *some = group("Some", { m_fixtures[2], m_fixtures[3] });
    FixtureGroup *one = group("One", { m_fixtures[2] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(127, "Half");
    const quint32 low = dimmer(40, "Low");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
    QList<quint32> q;
    for (int i = 0; i < 4; ++i)
        q << st->addCue(listId, i - 1, QString("Q%1").arg(i + 1));
    auto own = [&](int i, FixtureGroup *g, quint32 pal) {
        const quint32 l = st->addLayer(q[i], groupTarget(g->id()));
        st->setLayerLook(q[i], l, st->lookForPalette(pal));
        return l;
    };
    own(0, all, full);                              // Q1
    own(1, some, low);                              // Q2: overlap, tracked All
    {                                               // Q3: a child
        const quint32 p = own(2, all, half);
        const quint32 c = st->addChildLayer(q[2], p, groupTarget(one->id()));
        st->setLayerLook(q[2], c, st->lookForPalette(full));
    }
    QVERIFY(st->setCueMode(q[3], CueMode::Block));  // Q4: Mix, HTP between its scenes
    own(3, all, full);                              // the upper brighter: HTP ≠ the later
    own(3, some, low);
    QVERIFY(st->setCueMixOverlap(q[3], true));
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    const QList<QHash<quint32, uchar>> played = playAll(ch);
    QCOMPARE(played.count(), 4);
    QCOMPARE(int(played[3].value(2)), 255);
    for (int i = 0; i < q.count(); ++i)
    {
        st->previewCue(q[i]);
        const QHash<quint32, uchar> pv = settle(20);
        for (quint32 f = 0; f < quint32(m_fixtures.count()); ++f)
            QVERIFY2(pv.value(f) == played[i].value(f),
                     qPrintable(QString("Q%1 fixture %2: preview %3, GO %4").arg(i + 1).arg(f)
                                .arg(pv.value(f)).arg(played[i].value(f))));
        st->stopPreview();
        stopAll();
        tick(20);
    }

    // Over a list running for real (at Q1, All Full), the preview still
    // shows what is in edit: Q2's Some Low wins there (Design, open 6).
    go(ch);
    tick(10);
    st->previewCue(q[1]);
    const QHash<quint32, uchar> over = settle(20);
    QCOMPARE(int(over.value(0)), 255);
    QCOMPARE(int(over.value(2)), 40);
    st->stopPreview();
}

/****************************************************************************
 * Skipped cues (2026-10-21)
 ****************************************************************************/

/* Branson: "in the show-tree we need a way to disable cues inline and skip
   them". A skipped cue tracks as if it weren't in the list: every cue left
   plays exactly what it plays with the skipped ones deleted -- for every
   choice of cues to skip, whatever their mode (Block, Cue-only, Release). */
void ShowTree_Test::skippedCuesTrackAsIfNotThere()
{
    CueList full;
    full.mode = ListMode::Tracking;
    full.cues << cue(1, "1", CueMode::ListDefault,
                     { layer(11, Towers, { look(KeyColour, { Blue }), look(KeyDimmer, { Full }) }) });
    full.cues << cue(2, "2", CueMode::ListDefault,
                     { layer(21, USTs, { look(KeyColour, { Red }) }),
                       layer(22, Towers, { look(KeyDimmer, { D45 }) }, true) });
    full.cues << cue(3, "3", CueMode::Block, { layer(31, ATSpots, { look(KeyDimmer, { Full }) }) });
    full.cues << cue(4, "4", CueMode::CueOnly, { layer(41, Towers, { look(KeyColour, { Green }) }) });
    full.cues << cue(5, "5", CueMode::ListDefault, { layer(51, USTs, { look(KeyDimmer, { D45 }) }) },
                     { look(KeyColour, { White }) });
    full.cues << cue(6, "6", CueMode::ListDefault, { layer(61, StepTops, { look(KeyColour, { White }) }) });
    const int n = int(full.cues.count());

    for (int mask = 1; mask < (1 << n); ++mask)
    {
        CueList skipped = full, deleted = full;
        for (int k = n - 1; k >= 0; --k)
            if (mask & (1 << k))
            {
                skipped.cues[k].skip = true;
                deleted.cues.removeAt(k);
            }
        const QList<ResolvedCue> rs = resolve(skipped), rd = resolve(deleted);
        QCOMPARE(rs.count(), n);
        int d = 0;
        for (int k = 0; k < n; ++k)
        {
            if (mask & (1 << k))
                continue;
            QVERIFY2(stageOf(rs.at(k)) == stageOf(rd.at(d)),
                     qPrintable(QString("skip mask %1: cue %2 plays differently").arg(mask).arg(k + 1)));
            QVERIFY(stageOf(ResolvedCue { 0, rs.at(k).passOn, {}, {} })
                    == stageOf(ResolvedCue { 0, rd.at(d).passOn, {}, {} }));
            ++d;
        }
        // The cue after a skipped one tracks from a cue that plays.
        for (int k = 0; k < n; ++k)
            for (const ResolvedLayer &rl : rs.at(k).layers)
                if (!rl.own)
                    QVERIFY(!skipped.cue(rl.fromCue)->skip);
    }

    // What a skipped cue itself plays (a jump to it): from the cue before
    // it that plays; it passes on what it was handed.
    CueList list = full;
    list.cues[1].skip = true;           // 2 skipped
    list.cues[2].mode = CueMode::Track; // 3 tracks
    list.cues[2].skip = true;           // and is skipped too
    const QList<ResolvedCue> r = resolve(list);
    CHECK_VALUE(r[1], Towers, KeyColour, Blue, Origin::Tracked, 1);
    CHECK_VALUE(r[1], USTs, KeyColour, Red, Origin::Own, 2);
    CHECK_VALUE(r[2], Towers, KeyColour, Blue, Origin::Tracked, 1);     // past 2, also skipped
    QVERIFY(r[2].layer(groupTarget(USTs)) == nullptr);
    CHECK_VALUE(r[3], Towers, KeyColour, Green, Origin::Own, 4);
    CHECK_VALUE(r[3], Towers, KeyDimmer, Full, Origin::Tracked, 1);     // 2's release never happened
    QCOMPARE(r[1].passOn.count(), r[0].passOn.count());

    // The first played, the next played; a skipped Block cue is no block.
    QCOMPARE(playedBefore(list, 3), 0);
    QCOMPARE(playedAfter(list, 0), 3);
    QCOMPARE(playedBefore(list, 0), -1);
    QCOMPARE(playedAfter(list, 5), -1);
    list.cues[0].skip = true;
    QCOMPARE(playedBefore(list, 3), -1);

    // The rebake chain: an edit to a cue reaches the skipped ones after it
    // and the next one played; one to a skipped cue only itself.
    CueList chain;
    chain.mode = ListMode::Tracking;
    for (quint32 id = 1; id <= 4; ++id)
        chain.cues << cue(id, QString::number(id), CueMode::ListDefault,
                          { layer(id * 10, Towers, { look(KeyDimmer, { Full }) }) });
    chain.cues[1].skip = true;
    QCOMPARE(rebakeChain(chain, 1), QList<quint32>({ 1, 2, 3, 4 }));
    QCOMPARE(rebakeChain(chain, 2), QList<quint32>({ 2 }));
    QVERIFY(!dependsOn(chain, 3, 2));
    QVERIFY(dependsOn(chain, 3, 1));

    // A skipped base cue is still what Release goes back to.
    CueList based;
    based.mode = ListMode::Tracking;
    based.baseCue = 1;
    based.cues << cue(1, "1", CueMode::Block, { layer(11, Towers, { look(KeyDimmer, { BaseDim }) }) });
    based.cues << cue(2, "2", CueMode::Block, { layer(21, Towers, { look(KeyDimmer, { Full }) }, true) });
    based.cues << cue(3, "3", CueMode::ListDefault, {});
    based.cues[0].skip = true;
    const QList<ResolvedCue> rbased = resolve(based);
    CHECK_VALUE(rbased[2], Towers, KeyDimmer, BaseDim, Origin::Base, 1);

    // Sub-cues: a skipped one is passed over too (4.3 from 4.1).
    CueList subs;
    subs.subOf = 99;
    subs.mode = ListMode::Tracking;
    subs.cues << cue(1, "", CueMode::ListDefault, { layer(11, Towers, { look(KeyColour, { Red }) }) });
    subs.cues << cue(2, "", CueMode::ListDefault, { layer(21, Towers, { look(KeyColour, { Green }) }) });
    subs.cues << cue(3, "", CueMode::ListDefault, { layer(31, USTs, { look(KeyColour, { Blue }) }) });
    subs.cues[1].skip = true;
    const QList<ResolvedCue> rsub = resolve(subs);
    CHECK_VALUE(rsub[2], Towers, KeyColour, Red, Origin::Tracked, 1);
    // ... and the first skipped: the next starts from what its cue plays.
    subs.cues[1].skip = false;
    subs.cues[0].skip = true;
    ResolvedLayer parentLayer;
    parentLayer.target = groupTarget(StepTops);
    parentLayer.values.insert(KeyDimmer, ResolvedValue { look(KeyDimmer, { Full }), Origin::Own, 50 });
    const QList<ResolvedLayer> start { parentLayer };
    const QList<ResolvedCue> rsub2 = resolve(subs, LineAimTest(), &start);
    QVERIFY(rsub2[1].layer(groupTarget(StepTops)) != nullptr);
    QVERIFY(rsub2[1].layer(groupTarget(StepTops))->values.contains(KeyDimmer));
    QVERIFY(rsub2[1].layer(groupTarget(Towers))->values.value(KeyColour).look.palettes == QList<quint32>({ Green }));

    // Every cue skipped: each still resolves (a jump plays it), nothing
    // passes on.
    CueList all = full;
    for (Cue &c : all.cues)
        c.skip = true;
    const QList<ResolvedCue> rall = resolve(all);
    for (const ResolvedCue &rc : rall)
        QVERIFY(rc.passOn.isEmpty());
    QVERIFY(rall[0].layer(groupTarget(Towers)) != nullptr);
}

/* A jump to a skipped cue plays it -- what it has, from the cue before it
   that plays; a cue that includes it plays its output as before. */
void ShowTree_Test::aSkippedCueStillPlaysWhenJumpedToAndIncluded()
{
    CueList list;
    list.mode = ListMode::Tracking;
    list.cues << cue(1, "1", CueMode::ListDefault, { layer(11, Towers, { look(KeyDimmer, { Full }) }) });
    list.cues << cue(2, "2", CueMode::Block, { layer(21, USTs, { look(KeyColour, { Red }) }) });
    list.cues << cue(3, "3", CueMode::ListDefault, {}, {}, { 2 });
    list.cues[1].skip = true;
    const QList<ResolvedCue> r = resolve(list);
    CHECK_VALUE(r[1], USTs, KeyColour, Red, Origin::Own, 2);
    QVERIFY(r[1].layer(groupTarget(Towers)) == nullptr);                // Block: from nothing
    CHECK_VALUE(r[2], USTs, KeyColour, Red, Origin::Included, 2);       // the include plays it
    CHECK_VALUE(r[2], Towers, KeyDimmer, Full, Origin::Tracked, 1);     // tracks past it
    QVERIFY(canInclude(list, 1, 2));
}

/* Q38: effective = the cue says no ? no : the list's if set, else the show's. */
void ShowTree_Test::moveInBlackDecisionMatrix()
{
    for (bool show : { false, true })
        for (bool cueOff : { false, true })
        {
            QCOMPARE(effectiveMoveInBlack(show, MoveInBlack::Show, cueOff), !cueOff && show);
            QCOMPARE(effectiveMoveInBlack(show, MoveInBlack::On, cueOff), !cueOff);
            QCOMPARE(effectiveMoveInBlack(show, MoveInBlack::Off, cueOff), false);
        }
}

/* The controller: one undo step per change, nothing for no change, Looks
   left alone; the planner's lookup by a running list's step or by a Show
   item's function; a sub-cue follows its cue and its cue's list. */
void ShowTree_Test::moveInBlackPerListAndCue()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 listId = st->addCueList(prod, "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    QVERIFY(!st->hasMoveInBlackOverrides());
    QVERIFY(!st->showMoveInBlack());
    QVERIFY(!st->moveInBlackInto(c2));
    QVERIFY(st->moveInBlackInto(c2, true));

    QVERIFY(st->setListMoveInBlack(listId, MoveInBlack::On));
    QCOMPARE(st->undoText(), QString("Move in black: On"));
    QVERIFY(!st->setListMoveInBlack(listId, MoveInBlack::On));     // no change, no step
    QCOMPARE(st->listMoveInBlack(listId), MoveInBlack::On);
    QVERIFY(st->anyListMoveInBlackOn());
    QVERIFY(st->moveInBlackInto(c2, false));                        // On whatever the show says
    QVERIFY(st->setCuesNoMoveInBlack({ c2 }, true));
    QCOMPARE(st->undoText(), QString("Don't move in black into cue"));
    QVERIFY(!st->setCuesNoMoveInBlack({ c2 }, true));
    QVERIFY(st->isCueNoMoveInBlack(c2));
    QVERIFY(!st->moveInBlackInto(c2, true));                        // the cue wins
    QVERIFY(st->moveInBlackInto(c1, false));
    int lists = 0, cues = 0;
    QVERIFY(st->hasMoveInBlackOverrides(&lists, &cues));
    QCOMPARE(lists, 1);
    QCOMPARE(cues, 1);

    // The planner's lookup: a running list's next step, a Show's next item.
    const CueList *l = st->cueList(listId);
    QVERIFY(st->moveInBlackForNext(l->chaser, 0, invalidId(), false));            // step 0: One
    QVERIFY(!st->moveInBlackForNext(l->chaser, 1, invalidId(), true));            // step 1: Two, flagged
    QVERIFY(st->moveInBlackForNext(invalidId(), -1, l->cues.at(0).collection, false));
    QVERIFY(!st->moveInBlackForNext(invalidId(), -1, l->cues.at(1).collection, true));
    QVERIFY(st->moveInBlackForNext(invalidId(), -1, l->chaser, false));           // starts at One
    QVERIFY(!st->moveInBlackForNext(invalidId(), -1, 12345, false));               // not the tree's:
    QVERIFY(st->moveInBlackForNext(invalidId(), -1, 12345, true));                 // the show's

    QVERIFY(st->setListMoveInBlack(listId, MoveInBlack::Off));
    QVERIFY(!st->moveInBlackInto(c1, true));
    QVERIFY(!st->anyListMoveInBlackOn());

    // A sub-cue: its cue's flag and its cue's list.
    const quint32 sub = st->addSubCue(c1, "Sub");
    QVERIFY(sub != invalidId());
    QVERIFY(st->setListMoveInBlack(listId, MoveInBlack::Show));
    QVERIFY(st->moveInBlackInto(sub, true));
    QVERIFY(st->setCuesNoMoveInBlack({ c1 }, true));
    QVERIFY(!st->moveInBlackInto(sub, true));
    QVERIFY(st->setCuesNoMoveInBlack({ c1 }, false));
    QCOMPARE(st->undoText(), QString("Move in black into cue"));
    QVERIFY(st->setListMoveInBlack(st->subCueList(c1), MoveInBlack::Off));     // = its cue's list
    QCOMPARE(st->listMoveInBlack(listId), MoveInBlack::Off);
    QVERIFY(!st->moveInBlackInto(sub, true));

    // A Looks section has neither.
    const quint32 look = st->addLook(prod, "Hit");
    if (look != invalidId())
    {
        QVERIFY(!st->setCuesNoMoveInBlack({ look }, true));
        QVERIFY(!st->setListMoveInBlack(st->listOfCue(look)->id, MoveInBlack::On));
    }
}

/* Saved as optional attributes (nothing when they're the show's), undone and
   redone as one step each; a reader that doesn't know them -- an older build,
   or a show from before -- reads the same tree, minus them. */
void ShowTree_Test::moveInBlackSaveLoadUndo()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    QVERIFY(!engineXml(m_doc).contains("MoveInBlack=\""));        // the show's own <MoveInBlack> only

    QVERIFY(st->setListMoveInBlack(listId, MoveInBlack::Off));
    QVERIFY(st->setCuesNoMoveInBlack({ c2 }, true));
    QVERIFY(st->undo());
    QVERIFY(!st->isCueNoMoveInBlack(c2));
    QCOMPARE(st->listMoveInBlack(listId), MoveInBlack::Off);        // one step each
    QVERIFY(st->undo());
    QCOMPARE(st->listMoveInBlack(listId), MoveInBlack::Show);
    QVERIFY(st->redo());
    QVERIFY(st->redo());
    QCOMPARE(st->listMoveInBlack(listId), MoveInBlack::Off);
    QVERIFY(st->isCueNoMoveInBlack(c2));

    const QByteArray xml = engineXml(m_doc);
    QVERIFY(xml.contains("MoveInBlack=\"Off\""));
    QVERIFY(xml.contains("NoMoveInBlack=\"1\""));
    Doc *other = new Doc(this);
    {
        QXmlStreamReader r(xml);
        r.readNextStartElement();
        QVERIFY(other->loadXML(r, false));
    }
    QCOMPARE(other->showTree()->listMoveInBlack(listId), MoveInBlack::Off);
    QVERIFY(other->showTree()->isCueNoMoveInBlack(c2));
    QVERIFY(!other->showTree()->isCueNoMoveInBlack(c1));
    QCOMPARE(productionXml(other->showTree()->productions().first()), productionXml(st->productions().first()));
    delete other;

    // On also round-trips.
    QVERIFY(st->setListMoveInBlack(listId, MoveInBlack::On));
    QVERIFY(engineXml(m_doc).contains("MoveInBlack=\"On\""));

    // Without the attributes (what an older reader sees of them: nothing):
    // the same tree, the show's setting everywhere.
    QByteArray old = xml;
    old.replace(" MoveInBlack=\"Off\"", "");
    old.replace(" NoMoveInBlack=\"1\"", "");
    QVERIFY(!old.contains("MoveInBlack=\""));
    Doc *older = new Doc(this);
    {
        QXmlStreamReader r(old);
        r.readNextStartElement();
        QVERIFY(older->loadXML(r, false));
    }
    Production expect = st->productions().first();
    for (CueList &el : expect.lists)
    {
        el.moveInBlack = MoveInBlack::Show;
        for (Cue &ec : el.cues)
            ec.noMoveInBlack = false;
    }
    QCOMPARE(older->showTree()->listMoveInBlack(listId), MoveInBlack::Show);
    QCOMPARE(productionXml(older->showTree()->productions().first()), productionXml(expect));
    delete older;
}

/* mib on|off|show [list], nomib / allowmib <cues>: one undo step each;
   Tab offers on/off/show, then list; cue numbers after nomib. */
void ShowTree_Test::moveInBlackTyped()
{
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = st->addProduction("P");
    ShowTreeCommand::Result r = cmd.run("list \"Show\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx = r.context;
    QList<quint32> c;
    for (int i = 1; i <= 4; ++i)
        c << st->addCueNumbered(ctx.list, QString("0.0.%1").arg(i), QString("C%1").arg(i));
    ctx.cue = c[0];

    r = cmd.run("mib off list", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->listMoveInBlack(ctx.list), MoveInBlack::Off);
    QCOMPARE(st->undoText(), QString("mib off list"));
    r = cmd.run("mib on", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->listMoveInBlack(ctx.list), MoveInBlack::On);
    r = cmd.run("mib", ctx);                                // says what it is
    QVERIFY2(r.ok && r.message.contains("On"), qPrintable(r.message));
    r = cmd.run("mib show list", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->listMoveInBlack(ctx.list), MoveInBlack::Show);

    r = cmd.run("nomib", ctx);                              // the selected cue
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(st->isCueNoMoveInBlack(c[0]));
    r = cmd.run("nomib 0.0.2 thru 0.0.4", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    for (int i = 1; i < 4; ++i)
        QVERIFY(st->isCueNoMoveInBlack(c[i]));
    QVERIFY(st->undo());                                    // one step for the range
    QVERIFY(!st->isCueNoMoveInBlack(c[2]));
    QVERIFY(st->redo());
    r = cmd.run("allowmib 0.0.3, 0.0.1", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(!st->isCueNoMoveInBlack(c[0]) && !st->isCueNoMoveInBlack(c[2]));
    QVERIFY(st->isCueNoMoveInBlack(c[1]));

    // Wrong: nothing changes.
    const QString before = st->undoText();
    QVERIFY(!cmd.run("mib sideways", ctx).ok);
    QVERIFY(!cmd.run("mib on cue", ctx).ok);
    QVERIFY(!cmd.run("nomib 9.9.9", ctx).ok);
    QCOMPARE(st->undoText(), before);

    QStringList options;
    QVERIFY(cmd.candidates("mib ", ctx, &options));
    QCOMPARE(options, QStringList({ "on", "off", "show" }));
    QVERIFY(cmd.candidates("mib off ", ctx, &options));
    QCOMPARE(options, QStringList({ "list" }));
    QVERIFY(cmd.candidates("nomib ", ctx, &options));
    QVERIFY(options.contains("0.0.3"));
    QVERIFY(cmd.candidates("nomib 0.0.3 ", ctx, &options));
    QVERIFY(options.contains("thru"));
    QVERIFY(cmd.candidates("", ctx, &options));
    QVERIFY(options.contains("mib") && options.contains("nomib") && options.contains("allowmib"));
    QVERIFY(ShowTreeCommand::helpText("nomib").contains("Don't move in black"));
    QVERIFY(ShowTreeCommand::grammar().contains("nomib"));
}

/* Baked: the cue's chaser step is skipped, so GO, Back and auto-follow pass
   over it (the runner) and a jump plays it; GO from stopped with the first
   skipped starts at the second; every cue skipped, GO does nothing. */
void ShowTree_Test::skippedCuesBakeToSkippedSteps()
{
    QList<FixtureGroup *> g;
    QList<quint32> d;
    for (int i = 0; i < 4; ++i)
    {
        g << group(QString("G%1").arg(i), { m_fixtures[i] });
        d << dimmer(50 * (i + 1), QString("D%1").arg(i));
    }
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    QList<quint32> c;
    for (int i = 0; i < 4; ++i)
    {
        c << st->addCue(listId, i - 1, QString("C%1").arg(i + 1));
        QVERIFY(st->setLayerLook(c[i], st->addLayer(c[i], groupTarget(g[i]->id())), st->lookForPalette(d[i])));
    }
    QVERIFY(st->setCuesSkipped({ c[2] }, true));
    QCOMPARE(st->undoText(), QString("Skip cue"));
    QVERIFY(!st->setCuesSkipped({ c[2] }, true));       // already: nothing, no step
    QVERIFY(st->isCueSkipped(c[2]));
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QVERIFY(ch != nullptr);
    QCOMPARE(ch->stepsCount(), 4);
    QVERIFY(!ch->isStepSkipped(0) && !ch->isStepSkipped(1) && ch->isStepSkipped(2) && !ch->isStepSkipped(3));
    // The skipped cue's collection is still there (a jump plays it).
    QVERIFY(m_doc->function(st->cue(c[2])->collection) != nullptr);

    // GO, GO, GO: 1, 2, 4 -- 3 passed over, its level never on stage.
    go(ch);
    QHash<quint32, uchar> out = settle(6);
    QCOMPARE(ch->currentStepIndex(), 0);
    go(ch);
    out = settle(6);
    QCOMPARE(ch->currentStepIndex(), 1);
    go(ch);
    out = settle(6);
    QCOMPARE(ch->currentStepIndex(), 3);
    QCOMPARE(int(out.value(2)), 0);
    QCOMPARE(int(out.value(3)), 200);
    QCOMPARE(int(out.value(1)), 100);               // tracked through
    // Back: 2, not 3.
    {
        ChaserAction back;
        back.m_action = ChaserPreviousStep;
        back.m_masterIntensity = 1.0;
        back.m_stepIntensity = 1.0;
        back.m_fadeMode = Chaser::FromFunction;
        ch->setAction(back);
        settle(6);
        QCOMPARE(ch->currentStepIndex(), 1);
    }
    // A jump to it (a double-click, show control's GO 3): it plays.
    {
        ChaserAction jump;
        jump.m_action = ChaserSetStepIndex;
        jump.m_stepIndex = 2;
        jump.m_masterIntensity = 1.0;
        jump.m_stepIntensity = 1.0;
        jump.m_fadeMode = Chaser::FromFunction;
        ch->setAction(jump);
        out = settle(6);
        QCOMPARE(ch->currentStepIndex(), 2);
        QCOMPARE(int(out.value(2)), 150);
    }
    // Show control names it: played, and says so.
    {
        QSignalSpy said(m_doc->showControl(), &ShowControl::commandHandled);
        ShowControl::Request req;
        req.command = ShowControl::Go;
        req.chaser = ch->id();
        req.cue = QStringLiteral("3");
        QVERIFY(m_doc->showControl()->execute(req));
        QVERIFY(!said.isEmpty());
        QVERIFY2(said.last().first().toString().contains("skipped"), qPrintable(said.last().first().toString()));
    }
    stopAll();

    // Auto-follow passes over it too: 2 follows on by itself, into 4.
    {
        Cue two = *st->cue(c[1]);
        two.hold = 100;
        QVERIFY(st->setCue(two));
        ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
        go(ch);
        settle(4);
        go(ch);
        settle(30);
        QCOMPARE(ch->currentStepIndex(), 3);
        stopAll();
        QVERIFY(st->undo());
    }

    // The first skipped: GO from stopped plays the second.
    QVERIFY(st->setCuesSkipped({ c[0] }, true));
    ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    go(ch);
    out = settle(6);
    QCOMPARE(ch->currentStepIndex(), 1);
    QCOMPARE(int(out.value(0)), 0);
    stopAll();

    // Every cue skipped: GO plays nothing (show control says why).
    QVERIFY(st->setCuesSkipped(c, true));
    QCOMPARE(st->undoText(), QString("Skip 4 cues"));
    ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    {
        ShowControl::Request req;
        req.command = ShowControl::Go;
        req.chaser = ch->id();
        QVERIFY(!m_doc->showControl()->execute(req));
    }
    // Put back, all at once: one step.
    QVERIFY(st->setCuesSkipped(c, false));
    QCOMPARE(st->undoText(), QString("Unskip 4 cues"));
    for (int i = 0; i < 4; ++i)
        QVERIFY(!ch->isStepSkipped(i));

    // A look of a Looks section isn't skipped (it is fired, not GOne through).
    const quint32 look = st->addLook(st->productions().first().id, "Hit");
    if (look != invalidId())
    {
        QVERIFY(!st->setCuesSkipped({ look }, true));
        QVERIFY(!st->isCueSkipped(look));
    }
}

/* Saved with the show (the cue's Skip and its step's), undone and redone
   as one step; a show from before loads with nothing skipped. */
void ShowTree_Test::skippedCuesSaveLoadAndUndo()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    const quint32 half = dimmer(127, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::Tracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    st->setLayerLook(c1, st->addLayer(c1, groupTarget(a->id())), st->lookForPalette(half));
    Q_UNUSED(c3);

    QVERIFY(st->setCuesSkipped({ c2 }, true));
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QVERIFY(st->undo());
    QVERIFY(!st->isCueSkipped(c2));
    QVERIFY(!ch->isStepSkipped(1));
    QVERIFY(st->redo());
    QVERIFY(st->isCueSkipped(c2));
    QVERIFY(ch->isStepSkipped(1));

    const QByteArray xml = engineXml(m_doc);
    QVERIFY(xml.contains("Skip=\"1\""));
    Doc *other = new Doc(this);
    {
        QXmlStreamReader r(xml);
        r.readNextStartElement();
        QVERIFY(other->loadXML(r, false));
    }
    QVERIFY(other->showTree()->isCueSkipped(c2));
    QVERIFY(!other->showTree()->isCueSkipped(c1));
    Chaser *och = qobject_cast<Chaser *>(other->function(ch->id()));
    QVERIFY(och != nullptr && och->isStepSkipped(1));
    QCOMPARE(productionXml(other->showTree()->productions().first()), productionXml(st->productions().first()));
    delete other;

    // Before skipping existed: no Skip anywhere, nothing skipped.
    QByteArray old = xml;
    old.replace(" Skip=\"1\"", "");
    Doc *older = new Doc(this);
    {
        QXmlStreamReader r(old);
        r.readNextStartElement();
        QVERIFY(older->loadXML(r, false));
    }
    for (const Production &p : older->showTree()->productions())
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
                QVERIFY(!c.skip);
    delete older;
}

/* skip 0.0.3 · skip 0.0.3 thru 0.0.5 · skip 0.0.1, 0.0.6 · unskip · on a
   sub-cue; Tab offers cue numbers and thru. One undo step each. */
void ShowTree_Test::skipAndUnskipTyped()
{
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = st->addProduction("P");
    ShowTreeCommand::Result r = cmd.run("list \"Show\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    ctx = r.context;
    QList<quint32> c;
    for (int i = 1; i <= 6; ++i)
        c << st->addCueNumbered(ctx.list, QString("0.0.%1").arg(i), QString("C%1").arg(i));
    auto skipped = [&]() {
        QList<int> out;
        for (int i = 0; i < c.count(); ++i)
            if (st->isCueSkipped(c[i]))
                out << i + 1;
        return out;
    };

    r = cmd.run("skip 0.0.3", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(skipped(), QList<int>({ 3 }));
    QCOMPARE(st->undoText(), QString("skip 0.0.3"));

    r = cmd.run("skip 0.0.3 thru 0.0.5", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(skipped(), QList<int>({ 3, 4, 5 }));
    QVERIFY(st->undo());                                // one step for the range
    QCOMPARE(skipped(), QList<int>({ 3 }));
    QVERIFY(st->redo());

    r = cmd.run("unskip 0.0.4", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(skipped(), QList<int>({ 3, 5 }));

    r = cmd.run("skip 0.0.1, 0.0.6", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(skipped(), QList<int>({ 1, 3, 5, 6 }));

    r = cmd.run("unskip 0.0.6 thru 0.0.1", ctx);       // either way round
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(skipped(), QList<int>());

    // None named: the selected cue.
    ctx.cue = c[1];
    r = cmd.run("skip", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(skipped(), QList<int>({ 2 }));
    r = cmd.run("unskip", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(skipped(), QList<int>());
    r = cmd.run("unskip", ctx);
    QVERIFY(r.ok);
    QVERIFY(r.message.contains("not skipped"));

    // Wrong: nothing changes.
    const QString before = st->undoText();
    r = cmd.run("skip 9.9.9", ctx);
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("no cue 9.9.9"));
    r = cmd.run("skip 0.0.2 thru", ctx);
    QVERIFY(!r.ok);
    QCOMPARE(st->undoText(), before);
    QCOMPARE(skipped(), QList<int>());

    // A sub-cue by its number: its sub-chaser's step.
    const quint32 sub1 = st->addSubCue(c[1], "Hit");
    const quint32 sub2 = st->addSubCue(c[1], "Sweep", sub1);
    QVERIFY(sub2 != invalidId());
    r = cmd.run("skip 0.0.2.2", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(st->isCueSkipped(sub2));
    const CueList *subs = st->cueList(st->subCueList(c[1]));
    QVERIFY(subs != nullptr);
    Chaser *subChaser = qobject_cast<Chaser *>(m_doc->function(subs->chaser));
    QVERIFY(subChaser != nullptr && subChaser->isStepSkipped(1));

    // Tab: cue numbers, then thru.
    QStringList options;
    QVERIFY(cmd.candidates("skip ", ctx, &options));
    QVERIFY(options.contains("0.0.3"));
    QVERIFY(cmd.candidates("skip 0.0.3 ", ctx, &options));
    QVERIFY(options.contains("thru"));
    QVERIFY(ShowTreeCommand::helpText("skip").contains("thru"));
    QVERIFY(ShowTreeCommand::grammar().contains("skip 0.0.3"));
}

/* Import: a step skipped in the chaser is a skipped cue. Convert to
   tracking keeps it, and every cue plays as before. */
void ShowTree_Test::importAndConvertKeepTheSkip()
{
    FixtureGroup *a = group("Towers", { m_fixtures[0] });
    FixtureGroup *b = group("USTs", { m_fixtures[1] });
    const quint32 dA = dimmer(100, "A");
    const quint32 dB = dimmer(200, "B");
    Scene *sA = groupScene(m_doc, "TowersA", a->id(), { dA });
    Scene *sB = groupScene(m_doc, "USTsB", b->id(), { dB });
    Collection *one = collection(m_doc, "0.0.1-One", { sA->id() });
    Collection *two = collection(m_doc, "0.0.2-Two", { sA->id(), sB->id() });
    Collection *three = collection(m_doc, "0.0.3-Three", { sA->id() });
    Chaser *chaser = new Chaser(m_doc);
    chaser->setName("Full Show");
    chaser->setDurationMode(Chaser::PerStep);
    m_doc->addFunction(chaser);
    chaser->addStep(ChaserStep(one->id(), 0, Function::infiniteSpeed(), 0));
    ChaserStep s2(two->id(), 0, Function::infiniteSpeed(), 0);
    s2.skip = true;
    chaser->addStep(s2);
    chaser->addStep(ChaserStep(three->id(), 0, Function::infiniteSpeed(), 0));

    ShowTreeController *st = m_doc->showTree();
    const ShowTreeController::ImportReport rep = st->importChaser(chaser->id());
    const CueList *list = st->cueList(rep.list);
    QVERIFY(list != nullptr);
    QCOMPARE(list->cues.count(), 3);
    QVERIFY(!list->cues.at(0).skip);
    QVERIFY(list->cues.at(1).skip);
    QVERIFY(!list->cues.at(2).skip);
    for (const QString &note : rep.notes)
        QVERIFY2(!note.contains("skipped"), qPrintable(note));
    Chaser *made = qobject_cast<Chaser *>(m_doc->function(list->chaser));
    QVERIFY(made != nullptr && made->isStepSkipped(1));

    const quint32 skippedId = list->cues.at(1).id;
    ShowTreeController::ConvertPlan plan = st->planConvertToTracking(rep.list);
    QVERIFY2(plan.ok, qPrintable(plan.why));
    QVERIFY(st->convertToTracking(rep.list, &plan));
    QVERIFY(st->isCueSkipped(skippedId));
    list = st->cueList(rep.list);
    QCOMPARE(list->mode, ListMode::Tracking);
    made = qobject_cast<Chaser *>(m_doc->function(list->chaser));
    QVERIFY(made != nullptr && made->isStepSkipped(1));
}

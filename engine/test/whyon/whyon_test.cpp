/*
  Q Light Controller Plus
  whyon_test.cpp

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

#include <QtTest>

#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef private
#undef protected

#include "whyon_test.h"
#include "showtreecontroller.h"
#include "programmercontroller.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "showtreemodel.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "mastertimer.h"
#include "fixturegroup.h"
#include "groupmasters.h"
#include "chaseraction.h"
#include "qlcpalette.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "collection.h"
#include "universe.h"
#include "fixture.h"
#include "chaser.h"
#include "whyon.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
Target groupTarget(quint32 g)
{
    Target t;
    t.groups << g;
    return t;
}
}

void WhyOn_Test::init()
{
    m_doc = new Doc(this);
    m_fixtures.clear();
    for (int i = 0; i < 4; ++i)
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

void WhyOn_Test::cleanup()
{
    WhyOn::setWidgetNamer(nullptr);
    m_doc->programmer()->unparkAllFixtures();
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(5);
    delete m_doc;
    m_doc = nullptr;
}

void WhyOn_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
}

uchar WhyOn_Test::out(int address)
{
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    const uchar v = ua.first()->postGMValue(address);
    m_doc->inputOutputMap()->releaseUniverses(false);
    return v;
}

quint32 WhyOn_Test::dimmer(int level, const QString &name)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
    p->setName(name);
    p->setValue(level);
    m_doc->addPalette(p);
    return p->id();
}

FixtureGroup *WhyOn_Test::group(const QString &name, const QList<quint32> &fixtures)
{
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName(name);
    m_doc->addFixtureGroup(g);
    for (quint32 f : fixtures)
        g->assignFixture(f);
    return g;
}

void WhyOn_Test::go(Chaser *c)
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

/* Cue 1 sets the dimmer; cue 2 doesn't touch it: it plays on, and the
   answer names cue 2 (what runs) and where the level came from. */
void WhyOn_Test::aTrackedCueSaysWhereItCameFrom()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    const quint32 half = dimmer(128, "Half");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance");
    const quint32 listId = st->addCueList(prod, "Song");
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    QVERIFY(st->setTargetLook(c1, groupTarget(a->id()), st->lookForPalette(full)));
    QVERIFY(st->setTargetLook(c2, groupTarget(b->id()), st->lookForPalette(half)));
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QVERIFY(chaser != nullptr);

    go(chaser);
    tick(5);
    WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[0]);
    QVERIFY(w.isValid());
    QCOMPARE(w.subject, QString("Dim 1 · Dimmer #1"));
    QCOMPARE(w.lines.count(), 1);
    QCOMPARE(w.lines.first().kind, WhyOn::Kind::Cue);
    QCOMPARE(w.lines.first().verdict, WhyOn::Verdict::Wins);
    QCOMPARE(w.lines.first().cueId, c1);
    QCOMPARE(w.lines.first().text, QString("Cue 1 One · layer A sets 100 %"));

    go(chaser);
    tick(5);
    QCOMPARE(st->runningCue(listId), c2);
    QCOMPARE(int(out(0)), 255);
    w = WhyOn::explain(m_doc, m_fixtures[0]);
    QCOMPARE(w.texts(), QStringList() << "Cue 2 Two · layer A sets 100 %, tracked from cue 1 One");
    QCOMPARE(w.lines.first().cueId, c2);
    QCOMPARE(w.headline, QString("At 100 %. Cue 2 Two · layer A sets 100 %, tracked from cue 1 One."));
    QVERIFY(!w.approximate);

    // Cue 2's own layer on B is its own, not tracked.
    w = WhyOn::explain(m_doc, m_fixtures[1]);
    QCOMPARE(w.texts(), QStringList() << "Cue 2 Two · layer B sets 50 %");
}

/* An effect part way in (its fader's Mix at 0.5, as the effect runner sets
   it while it fades): it moves the dimmer half way from what is under it. */
void WhyOn_Test::anEffectAtHalfDepth()
{
    Scene *s = new Scene(m_doc);
    s->setName("Base");
    s->setValue(m_fixtures[0], 0, 100);
    QVERIFY(m_doc->addFunction(s));
    s->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);

    Universe *uni = m_doc->inputOutputMap()->universe(0);
    QSharedPointer<GenericFader> fader = uni->requestFader(Universe::Override);
    fader->setName(QStringLiteral("EffectScript"));
    FadeChannel fc(m_doc, m_fixtures[0], 0);
    fc.setTarget(200);
    fc.setCurrent(200);
    fc.setFadeTime(0);
    fc.addFlag(FadeChannel::Override | FadeChannel::Mix);
    fc.setMix(0.5f, 1.0f);
    fader->replace(fc);
    tick(2);
    QCOMPARE(int(out(0)), 150);

    const WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[0]);
    QCOMPARE(w.texts(), QStringList()
             << "An effect moves it to 78 %, at 50 % depth (fading)"
             << "Scene “Base” sets 39 % (replaced by what is above it)");
    QCOMPARE(w.lines.at(0).kind, WhyOn::Kind::Effect);
    QCOMPARE(w.lines.at(0).verdict, WhyOn::Verdict::Wins);
    QCOMPARE(w.lines.at(1).functionId, s->id());
    QCOMPARE(int(w.out), 150);
    QVERIFY(w.headline.startsWith("At 59 %."));
    uni->dismissFader(fader);
}

/* An override hit at 0 over a running cue: it holds the dimmer down, and
   the cue is still there under it. */
void WhyOn_Test::anOverrideHitPullsItDown()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    const quint32 outLevel = dimmer(0, "Out");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance");
    const quint32 listId = st->addCueList(prod, "Song");
    const quint32 c1 = st->addCue(listId, -1, "One");
    QVERIFY(st->setTargetLook(c1, groupTarget(a->id()), st->lookForPalette(full)));
    const quint32 hit = st->addHit(HitScope::Production, prod, "Blackout");
    const quint32 hitLayer = st->addLayer(hit, groupTarget(a->id()));
    st->setLayerLook(hit, hitLayer, st->lookForPalette(outLevel));
    QVERIFY(st->setHitsOverride({ hit }, true));
    Collection *coll = qobject_cast<Collection *>(m_doc->function(st->cue(hit)->collection));
    QVERIFY(coll != nullptr);
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    go(chaser);
    tick(5);
    coll->start(m_doc->masterTimer(), FunctionParent::master());
    tick(5);
    QCOMPARE(int(out(0)), 0);

    const WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[0]);
    QCOMPARE(w.texts(), QStringList()
             << "Override hit “Blackout” · layer A holds it at 0 %"
             << "Cue 1 One · layer A sets 100 % (replaced by what is above it)");
    QCOMPARE(w.lines.at(0).kind, WhyOn::Kind::Hit);
    QCOMPARE(w.lines.at(0).cueId, hit);
    QCOMPARE(w.lines.at(0).layerId, hitLayer);
    QCOMPARE(w.lines.at(1).kind, WhyOn::Kind::Cue);
    QCOMPARE(w.lines.at(1).verdict, WhyOn::Verdict::Replaced);
    // Dark: the reason it isn't on is the hit (no output only comes last
    // in the headline: the screens show the light without one).
    QCOMPARE(w.headline, QString("At 0 %. Not on: Override hit “Blackout” · layer A holds it at 0 %."));
    QCOMPARE(w.whyNotTexts(), QStringList()
             << "Universe 1 has no output: nothing leaves the computer for it (Connections)"
             << "Override hit “Blackout” · layer A holds it at 0 %");
}

/* Two scenes on one HTP dimmer: the higher wins, the other is named as
   lower -- and the order is the stack's, the last one up first. */
void WhyOn_Test::anHtpFightNamesTheWinner()
{
    Scene *warm = new Scene(m_doc);
    warm->setName("Warm");
    warm->setValue(m_fixtures[0], 0, 200);
    QVERIFY(m_doc->addFunction(warm));
    Scene *cool = new Scene(m_doc);
    cool->setName("Cool");
    cool->setValue(m_fixtures[0], 0, 128);
    QVERIFY(m_doc->addFunction(cool));
    warm->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    cool->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    QCOMPARE(int(out(0)), 200);

    WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[0]);
    QCOMPARE(w.texts(), QStringList()
             << "Scene “Cool” sets 50 % (lower: on this channel the highest level wins, HTP)"
             << "Scene “Warm” sets 78 %");
    QCOMPARE(w.lines.at(0).verdict, WhyOn::Verdict::Lower);
    QCOMPARE(w.lines.at(1).verdict, WhyOn::Verdict::Wins);
    QCOMPARE(w.lines.at(1).functionId, warm->id());

    // Started by a VC widget: the UI names it.
    WhyOn::setWidgetNamer([](quint32 id) { return QString("VC button %1").arg(id); });
    cool->stop(FunctionParent::master());
    tick(3);
    cool->start(m_doc->masterTimer(), FunctionParent(FunctionParent::ManualVCWidget, 7));
    cool->setValue(m_fixtures[0], 0, 255);
    tick(3);
    w = WhyOn::explain(m_doc, m_fixtures[0]);
    QCOMPARE(w.texts(), QStringList()
             << "Scene “Cool” sets 100 % (started by VC button 7)"
             << "Scene “Warm” sets 78 % (lower: on this channel the highest level wins, HTP)");
    QCOMPARE(w.lines.at(0).widgetId, quint32(7));
}

/* A group master at 50 % scales the dimmer before the Grand Master; the
   two multiply. */
void WhyOn_Test::aGroupMasterAtHalf()
{
    FixtureGroup *front = group("Front", { m_fixtures[0] });
    front->setMaster(true);
    front->setMasterLevel(127);
    m_doc->groupMasters()->rebuild();
    Scene *s = new Scene(m_doc);
    s->setName("Look");
    s->setValue(m_fixtures[0], 0, 255);
    QVERIFY(m_doc->addFunction(s));
    s->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    QCOMPARE(int(out(0)), 127);

    WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[0]);
    QCOMPARE(w.texts(), QStringList()
             << "Scene \u201CLook\u201D sets 100 %"
             << "Group master \u201CFront\u201D at 50 % scales it to 50 %");
    QCOMPARE(w.lines.at(1).kind, WhyOn::Kind::GroupMaster);
    QCOMPARE(w.lines.at(1).groupId, front->id());
    QCOMPARE(w.headline, QString("At 50 %. Scene \u201CLook\u201D sets 100 %."));

    // With the Grand Master at 50 % too: both, in the order they apply.
    m_doc->inputOutputMap()->setGrandMasterValue(127);
    tick(2);
    w = WhyOn::explain(m_doc, m_fixtures[0]);
    QCOMPARE(int(w.out), 63);
    QCOMPARE(w.texts(), QStringList()
             << "Scene \u201CLook\u201D sets 100 %"
             << "Group master \u201CFront\u201D at 50 % scales it to 50 %"
             << "Grand Master at 50 % scales it to 25 %");
}

/* Park over a running scene, with the Grand Master down: Park wins and goes
   past the Grand Master. */
void WhyOn_Test::parkHoldsItPastTheGrandMaster()
{
    Scene *s = new Scene(m_doc);
    s->setName("Look");
    s->setValue(m_fixtures[0], 0, 128);
    QVERIFY(m_doc->addFunction(s));
    s->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    m_doc->programmer()->parkFixtures({ m_fixtures[0] });
    s->setValue(m_fixtures[0], 0, 255);
    m_doc->inputOutputMap()->setGrandMasterValue(127);
    tick(3);
    QCOMPARE(int(out(0)), 128);

    const WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[0]);
    QCOMPARE(w.texts(), QStringList()
             << "Park holds it at 50 %"
             << "Scene “Look” sets 100 % (replaced by what is above it)"
             << "Grand Master is at 50 %, but Park goes past it");
    QCOMPARE(w.lines.at(0).kind, WhyOn::Kind::Park);
    QCOMPARE(w.lines.at(2).kind, WhyOn::Kind::GrandMaster);
    QCOMPARE(w.headline, QString("At 50 %. Park holds it at 50 %."));
}

void WhyOn_Test::theGrandMasterScalesIt()
{
    Scene *s = new Scene(m_doc);
    s->setName("Look");
    s->setValue(m_fixtures[0], 0, 255);
    QVERIFY(m_doc->addFunction(s));
    s->start(m_doc->masterTimer(), FunctionParent::master());
    m_doc->inputOutputMap()->setGrandMasterValue(127);
    tick(3);
    QCOMPARE(int(out(0)), 127);

    const WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[0]);
    QCOMPARE(w.texts(), QStringList()
             << "Scene “Look” sets 100 %"
             << "Grand Master at 50 % scales it to 50 %");
    QCOMPARE(w.lines.at(1).verdict, WhyOn::Verdict::Scales);
    QCOMPARE(w.headline, QString("At 50 %. Scene “Look” sets 100 %."));
}

void WhyOn_Test::blindAndNothingSetsIt()
{
    tick(2);
    WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[2]);
    QCOMPARE(w.texts(), QStringList()
             << "Nothing sets it now: an intensity is dark unless something sets it");
    QCOMPARE(w.lines.first().kind, WhyOn::Kind::Nothing);

    Scene *s = new Scene(m_doc);
    s->setName("Look");
    s->setValue(m_fixtures[2], 0, 255);
    QVERIFY(m_doc->addFunction(s));
    s->start(m_doc->masterTimer(), FunctionParent::master());
    m_doc->inputOutputMap()->setOutputInhibited(true);
    tick(3);
    w = WhyOn::explain(m_doc, m_fixtures[2]);
    QCOMPARE(w.texts(), QStringList()
             << "Scene “Look” sets 100 %"
             << "Blind is on: the rig doesn't get it (the screens still show it)");
    QVERIFY(!w.sent);
    QCOMPARE(w.headline, QString("At 100 %. Not sent (see below). Scene “Look” sets 100 %."));
    m_doc->inputOutputMap()->setOutputInhibited(false);
}

void WhyOn_Test::aChannelAndABareAddress()
{
    Scene *s = new Scene(m_doc);
    s->setName("Look");
    s->setValue(m_fixtures[3], 0, 255);
    QVERIFY(m_doc->addFunction(s));
    s->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    // The Simple Desk's universe mode asks by address: the fixture there.
    const WhyOn::Answer w = WhyOn::explainAddress(m_doc, 0, 3);
    QCOMPARE(w.fixtureId, m_fixtures[3]);
    QCOMPARE(w.channel, quint32(0));
    QCOMPARE(w.texts(), QStringList() << "Scene “Look” sets 100 %");
    // An address no fixture has.
    const WhyOn::Answer bare = WhyOn::explainAddress(m_doc, 0, 100);
    QCOMPARE(bare.subject, QString("Universe 1, channel 101"));
    // Nothing for no fixture, a channel past its end, a bad universe.
    QVERIFY(!WhyOn::explain(m_doc, 9999).isValid());
    QVERIFY(!WhyOn::explain(m_doc, m_fixtures[0], 5).isValid());
    QVERIFY(!WhyOn::explainAddress(m_doc, 99, 0).isValid());
    QVERIFY(w.toText().contains("Scene “Look” sets 100 %"));
}

void WhyOn_Test::simpleWordingDropsTheMergeWords()
{
    Scene *warm = new Scene(m_doc);
    warm->setName("Warm");
    warm->setValue(m_fixtures[0], 0, 200);
    QVERIFY(m_doc->addFunction(warm));
    Scene *cool = new Scene(m_doc);
    cool->setName("Cool");
    cool->setValue(m_fixtures[0], 0, 128);
    QVERIFY(m_doc->addFunction(cool));
    warm->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    cool->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    const WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[0], QLCChannel::invalid(), WhyOn::Wording::Simple);
    QCOMPARE(w.texts(), QStringList()
             << "Scene “Cool” sets 50 % (lower, so it doesn't show)"
             << "Scene “Warm” sets 78 %");
    QCOMPARE(w.headline, QString("It's at 78 %. Scene “Warm” sets 78 %."));
}

Fixture *WhyOn_Test::addSpot(quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Why spot");
    def->setType(QLCFixtureDef::MovingHead);
    auto channel = [def](const QString &name, QLCChannel::Preset preset) {
        QLCChannel *ch = new QLCChannel();
        ch->setPreset(preset);
        ch->setName(name);
        def->addChannel(ch);
        return ch;
    };
    QLCChannel *dim = channel("Dimmer", QLCChannel::IntensityMasterDimmer);
    QLCChannel *r = channel("Red", QLCChannel::IntensityRed);
    QLCChannel *g = channel("Green", QLCChannel::IntensityGreen);
    QLCChannel *b = channel("Blue", QLCChannel::IntensityBlue);
    QLCChannel *sh = new QLCChannel();
    sh->setName("Shutter");
    sh->setGroup(QLCChannel::Shutter);
    sh->addCapability(new QLCCapability(0, 9, "Closed"));
    sh->addCapability(new QLCCapability(10, 255, "Open"));
    def->addChannel(sh);
    QLCChannel *pan = channel("Pan", QLCChannel::PositionPan);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("6ch");
    int i = 0;
    for (QLCChannel *ch : { dim, r, g, b, sh, pan })
        mode->insertChannel(ch, i++);
    def->addMode(mode);
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Spot");
    fxi->setFixtureDefinition(def, mode);
    fxi->setAddress(address);
    fxi->setUniverse(0);
    m_doc->addFixture(fxi);
    return fxi;
}

/* v2: a pan nothing sets keeps the value its last writer left -- named,
   whether the writer has gone (a scene stopped) or still runs on other
   channels (the programmer, its edit auto-removed at its target). */
void WhyOn_Test::aValueLeftBehindNamesItsWriter()
{
    Fixture *spot = addSpot(20);
    Scene *aim = new Scene(m_doc);
    aim->setName("Aim");
    aim->setValue(spot->id(), 5, 100);
    QVERIFY(m_doc->addFunction(aim));
    aim->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    aim->stop(FunctionParent::master());
    tick(50);
    QCOMPARE(int(out(25)), 100);

    WhyOn::Answer w = WhyOn::explain(m_doc, spot->id(), 5);
    QCOMPARE(w.lines.count(), 1);
    const WhyOn::Line &l = w.lines.first();
    QCOMPARE(l.kind, WhyOn::Kind::LeftBehind);
    QCOMPARE(l.functionId, aim->id());
    QVERIFY2(l.text.startsWith("Nothing sets it now: Scene “Aim” left it at 39 %, "), qPrintable(l.text));
    QVERIFY2(l.text.endsWith(" s ago (it keeps the last value it was given)"), qPrintable(l.text));
    QVERIFY(w.whyNot.isEmpty());            // a pan: no "why not"

    // The programmer's edit, auto-removed when it got there: its fader lives on.
    m_doc->programmer()->writeChannelLive(spot->id(), 5, 200);
    tick(5);
    QCOMPARE(int(out(25)), 200);
    w = WhyOn::explain(m_doc, spot->id(), 5);
    QCOMPARE(w.lines.first().kind, WhyOn::Kind::LeftBehind);
    QVERIFY2(w.lines.first().text.startsWith("Nothing sets it now: the programmer left it at 78 %, 0."),
             qPrintable(w.lines.first().text));

    // Plain words.
    w = WhyOn::explain(m_doc, spot->id(), 5, WhyOn::Wording::Simple);
    QVERIFY2(w.lines.first().text.endsWith(" s ago, and it stays there"), qPrintable(w.lines.first().text));
}

/* Written since by something with no fader (a direct write): the writer
   that let go no longer explains it, and the answer says only that it kept
   its last value. */
void WhyOn_Test::aWriteSinceForgetsTheWriter()
{
    Fixture *spot = addSpot(20);
    m_doc->programmer()->writeChannelLive(spot->id(), 5, 200);
    tick(5);
    Universe *uni = m_doc->inputOutputMap()->universe(0);
    QVERIFY(uni->letGo(25).isValid());
    QVERIFY(uni->letGo(25).alive);
    QCOMPARE(uni->letGo(25).name, QString("Programmer"));
    tick(1);
    uni->write(25, 60, true);               // a maintenance button's one-shot
    QVERIFY(!uni->letGo(25).isValid());
    tick(30);
    const WhyOn::Answer w = WhyOn::explain(m_doc, spot->id(), 5);
    QCOMPARE(w.lines.first().kind, WhyOn::Kind::Nothing);
    QVERIFY2(w.lines.first().text.startsWith("Nothing sets it now: it kept the last value it was given"),
             qPrintable(w.lines.first().text));
    QVERIFY(!uni->letGo(-1).isValid());
    QVERIFY(!uni->letGo(512).isValid());
}

/* A move in black is a fader now ("Mark", under everything): named, and
   it holds no intensity -- the light stays dark. */
void WhyOn_Test::aMarkIsAFaderWithAName()
{
    Fixture *spot = addSpot(20);
    Scene *aim = new Scene(m_doc);
    aim->setName("Aim");
    aim->setValue(spot->id(), 5, 100);
    aim->setValue(spot->id(), 1, 255);      // red
    QVERIFY(m_doc->addFunction(aim));
    aim->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    m_doc->programmer()->markFixtures({ spot->id() });
    aim->stop(FunctionParent::master());
    tick(5);
    QCOMPARE(int(out(25)), 100);            // the pan held
    QCOMPARE(int(out(21)), 0);              // no colour held: dark

    WhyOn::Answer w = WhyOn::explain(m_doc, spot->id(), 5);
    QCOMPARE(w.texts(), QStringList() << "A move in black (Mark) presets it to 39 %, while the light is dark");
    QCOMPARE(w.lines.first().kind, WhyOn::Kind::Mark);
    QCOMPARE(w.lines.first().verdict, WhyOn::Verdict::Wins);

    // Let go: the mark's fader leaves, and is named for what it left.
    m_doc->programmer()->unmarkFixtures({ spot->id() });
    tick(3);
    QCOMPARE(int(out(25)), 100);
    w = WhyOn::explain(m_doc, spot->id(), 5);
    QCOMPARE(w.lines.first().kind, WhyOn::Kind::LeftBehind);
    QVERIFY2(w.lines.first().text.startsWith("Nothing sets it now: a move in black (Mark) left it at 39 %"),
             qPrintable(w.lines.first().text));
}

/* v3: the dimmer is up, the light is dark anyway -- the shutter is closed
   and the colour is at black. Each reason, the blocking one first; the
   headline gives the first that isn't "no output". */
void WhyOn_Test::whyNotListsTheBlockersInOrder()
{
    Fixture *spot = addSpot(20);
    Scene *look = new Scene(m_doc);
    look->setName("Look");
    look->setValue(spot->id(), 0, 255);
    look->setValue(spot->id(), 4, 0);
    QVERIFY(m_doc->addFunction(look));
    look->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    QCOMPARE(int(out(20)), 255);

    WhyOn::Answer w = WhyOn::explain(m_doc, spot->id());
    QCOMPARE(w.texts(), QStringList() << "Scene “Look” sets 100 %");
    QCOMPARE(w.whyNotTexts(), QStringList()
             << "Universe 1 has no output: nothing leaves the computer for it (Connections)"
             << "Its shutter is closed (Shutter at 0: “Closed”): Scene “Look” sets 0 %"
             << "Its colour is at black: Red, Green, Blue are all at 0 %");
    QCOMPARE(w.whyNot.at(0).kind, WhyOn::Kind::NoOutput);
    QCOMPARE(w.whyNot.at(1).kind, WhyOn::Kind::Shutter);
    QCOMPARE(w.whyNot.at(1).functionId, look->id());
    QCOMPARE(w.whyNot.at(2).kind, WhyOn::Kind::ColourBlack);
    QCOMPARE(w.headline, QString("At 100 %. Not on: Its shutter is closed (Shutter at 0: “Closed”): "
                                 "Scene “Look” sets 0 %."));
    QVERIFY(w.toText().contains("  ✖ Its colour is at black"));

    // Open and red: it's on, no "why not".
    look->setValue(spot->id(), 4, 255);
    look->setValue(spot->id(), 1, 255);
    tick(3);
    w = WhyOn::explain(m_doc, spot->id());
    QVERIFY(w.whyNot.isEmpty());
    QCOMPARE(w.headline, QString("At 100 %. Scene “Look” sets 100 %."));
    // A pan is never asked "why not".
    QVERIFY(WhyOn::explain(m_doc, spot->id(), 5).whyNot.isEmpty());
}

/* The masters at 0 and Blind, in the order the universe applies them,
   before what holds the dimmer. */
void WhyOn_Test::whyNotTheMastersAndBlind()
{
    FixtureGroup *front = group("Front", { m_fixtures[0] });
    front->setMaster(true);
    front->setMasterLevel(0);
    m_doc->groupMasters()->rebuild();
    Scene *s = new Scene(m_doc);
    s->setName("Look");
    s->setValue(m_fixtures[0], 0, 255);
    QVERIFY(m_doc->addFunction(s));
    s->start(m_doc->masterTimer(), FunctionParent::master());
    m_doc->inputOutputMap()->setGrandMasterValue(0);
    m_doc->inputOutputMap()->setOutputInhibited(true);
    m_doc->inputOutputMap()->setBlackout(true);
    tick(3);
    QCOMPARE(int(out(0)), 0);

    const WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[0]);
    QCOMPARE(w.whyNotTexts(), QStringList()
             << "Universe 1 has no output: nothing leaves the computer for it (Connections)"
             << "Blind is on: the rig gets nothing (the screens still show it)"
             << "Blackout is on"
             << "The Grand Master is at 0 %"
             << "Group master “Front” is at 0 %");
    QCOMPARE(w.whyNot.at(4).groupId, front->id());
    QVERIFY(w.headline.endsWith("Not on: Blind is on: the rig gets nothing (the screens still show it)."));
    m_doc->inputOutputMap()->setOutputInhibited(false);
    m_doc->inputOutputMap()->setBlackout(false);
    m_doc->inputOutputMap()->setGrandMasterValue(255);
}

void WhyOn_Test::whyNotNothingSetsItAndNotPatched()
{
    tick(2);
    WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[2]);
    QCOMPARE(w.whyNotTexts(), QStringList()
             << "Universe 1 has no output: nothing leaves the computer for it (Connections)"
             << "Nothing sets its dimmer: no cue, scene, desk or slider is on it");
    QCOMPARE(w.headline, QString("At 0 %. Not on: Nothing sets its dimmer: no cue, scene, desk or slider is on it."));

    Fixture *loose = new Fixture(m_doc);
    loose->setName("Loose");
    loose->setChannels(1);
    loose->setPatched(false);               // in the inventory
    m_doc->addFixture(loose);
    w = WhyOn::explain(m_doc, loose->id());
    QCOMPARE(w.whyNotTexts(), QStringList() << "It isn't patched: give it an address first (Fixture Manager)");
    QCOMPARE(w.whyNot.first().kind, WhyOn::Kind::NotPatched);
}

void WhyOn_Test::whyNotParkedAtZero()
{
    tick(2);
    m_doc->programmer()->parkFixtures({ m_fixtures[1] });
    tick(3);
    const WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[1]);
    QCOMPARE(w.whyNotTexts(), QStringList()
             << "Universe 1 has no output: nothing leaves the computer for it (Connections)"
             << "It's parked at 0 % (Park beats everything)");
    QCOMPARE(w.whyNot.at(1).kind, WhyOn::Kind::Park);
}

/* Cue 1 lights A and releases it leaving; cue 2 lights B: under cue 2, A
   is dark because its layer was released. */
void WhyOn_Test::whyNotAReleasedLayer()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance");
    const quint32 listId = st->addCueList(prod, "Song");
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    QVERIFY(st->setTargetLook(c1, groupTarget(a->id()), st->lookForPalette(full)));
    QVERIFY(st->setTargetLook(c2, groupTarget(b->id()), st->lookForPalette(full)));
    QVERIFY(st->setTargetRelease(c1, groupTarget(a->id()), true));
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    go(chaser);
    tick(5);
    QVERIFY(WhyOn::explain(m_doc, m_fixtures[0]).whyNot.isEmpty());
    go(chaser);
    tick(5);
    QCOMPARE(st->runningCue(listId), c2);
    QCOMPARE(int(out(0)), 0);

    const WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[0]);
    QCOMPARE(w.whyNotTexts(), QStringList()
             << "Universe 1 has no output: nothing leaves the computer for it (Connections)"
             << "Cue 2 Two runs, and layer A was released leaving cue 1 One");
    QCOMPARE(w.whyNot.at(1).kind, WhyOn::Kind::Released);
    QCOMPARE(w.whyNot.at(1).cueId, c1);
}

/* Cue 2 would light A but is skipped tonight: GO passes over it, so under
   cue 3 A is dark, and the answer says the skipped cue never played. */
void WhyOn_Test::whyNotASkippedCue()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance");
    const quint32 listId = st->addCueList(prod, "Song");
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    QVERIFY(st->setTargetLook(c1, groupTarget(b->id()), st->lookForPalette(full)));
    QVERIFY(st->setTargetLook(c2, groupTarget(a->id()), st->lookForPalette(full)));
    QVERIFY(st->setTargetLook(c3, groupTarget(b->id()), st->lookForPalette(full)));
    QVERIFY(st->setCuesSkipped({ c2 }, true));
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    go(chaser);
    tick(5);
    go(chaser);
    tick(5);
    QCOMPARE(st->runningCue(listId), c3);
    QCOMPARE(int(out(0)), 0);

    const WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[0]);
    QCOMPARE(w.whyNotTexts(), QStringList()
             << "Universe 1 has no output: nothing leaves the computer for it (Connections)"
             << "Cue 3 Three runs, and cue 2 Two is skipped tonight, so its look (layer A) never played");
    QCOMPARE(w.whyNot.at(1).kind, WhyOn::Kind::Skipped);
    QCOMPARE(w.whyNot.at(1).cueId, c2);

    // Put back, it is an ordinary cue that doesn't light A any more.
    QVERIFY(st->setCuesSkipped({ c2 }, false));
    const WhyOn::Answer back = WhyOn::explain(m_doc, m_fixtures[0]);
    QVERIFY(!back.whyNotTexts().join('\n').contains("skipped"));
}

/* Design preview with Solo on layer A: B, in the same cue, stays dark and
   the answer says Solo did it. */
void WhyOn_Test::whyNotSolo()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance");
    const quint32 listId = st->addCueList(prod, "Song");
    const quint32 c1 = st->addCue(listId, -1, "One");
    QVERIFY(st->setTargetLook(c1, groupTarget(a->id()), st->lookForPalette(full)));
    QVERIFY(st->setTargetLook(c1, groupTarget(b->id()), st->lookForPalette(full)));
    m_doc->setMode(Doc::Design);
    st->previewCue(c1);
    tick(5);
    QCOMPARE(int(out(1)), 255);
    QVERIFY(WhyOn::explain(m_doc, m_fixtures[1]).whyNot.isEmpty());

    ShowTree::ResolvedCue rc;
    QVERIFY(st->resolvedCue(c1, &rc));
    quint32 layerA = 0xFFFFFFFF;
    for (const ShowTree::ResolvedLayer &rl : rc.layers)
        if (rl.target == groupTarget(a->id()))
            layerA = rl.layerId;
    QVERIFY(layerA != 0xFFFFFFFF);
    st->setPreviewSolo(c1, layerA);
    tick(5);
    QCOMPARE(int(out(0)), 255);
    QCOMPARE(int(out(1)), 0);

    const WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[1]);
    QCOMPARE(w.whyNotTexts(), QStringList()
             << "Universe 1 has no output: nothing leaves the computer for it (Connections)"
             << "Solo is on: only layer A of cue 1 One plays, so this light stays dark");
    QCOMPARE(w.whyNot.at(1).kind, WhyOn::Kind::Solo);
    QCOMPARE(w.whyNot.at(1).cueId, c1);
    QCOMPARE(w.whyNot.at(1).layerId, layerA);
    // The soloed layer's own light isn't dark.
    QVERIFY(WhyOn::explain(m_doc, m_fixtures[0]).whyNot.isEmpty());

    st->setPreviewSolo(0xFFFFFFFF, 0xFFFFFFFF);
    tick(5);
    QVERIFY(WhyOn::explain(m_doc, m_fixtures[1]).whyNot.isEmpty());
}

/* In Operate the cue open for edit is blind: it lights B only on GO. The
   reason comes after the masters (here the Grand Master at 0). */
void WhyOn_Test::whyNotAnOperateEditIsBlind()
{
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance");
    const quint32 listId = st->addCueList(prod, "Song");
    const quint32 c1 = st->addCue(listId, -1, "One");
    QVERIFY(st->setTargetLook(c1, groupTarget(b->id()), st->lookForPalette(full)));
    m_doc->setMode(Doc::Operate);
    st->pickCue(c1);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::NotDesign));
    m_doc->inputOutputMap()->setGrandMasterValue(0);
    tick(5);
    QCOMPARE(int(out(1)), 0);

    const WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[1]);
    QCOMPARE(w.whyNotTexts(), QStringList()
             << "Universe 1 has no output: nothing leaves the computer for it (Connections)"
             << "The Grand Master is at 0 %"
             << "Cue 1 One is being edited blind (Operate): the edit reaches the stage only on GO");
    QCOMPARE(w.whyNot.at(2).kind, WhyOn::Kind::BlindEdit);
    QCOMPARE(w.whyNot.at(2).cueId, c1);
    // A light the cue doesn't touch isn't blamed on it.
    QCOMPARE(WhyOn::explain(m_doc, m_fixtures[2]).whyNot.last().kind, WhyOn::Kind::Nothing);
    m_doc->inputOutputMap()->setGrandMasterValue(255);
}

QTEST_MAIN(WhyOn_Test)

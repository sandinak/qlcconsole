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
#include "chaseraction.h"
#include "qlcpalette.h"
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
    QCOMPARE(w.headline, QString("At 0 %. Override hit “Blackout” · layer A holds it at 0 %."));
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

QTEST_MAIN(WhyOn_Test)

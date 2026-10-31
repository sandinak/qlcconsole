/*
  Q Light Controller Plus - Unit test
  blindeffects_test.cpp

  The Blind view's effects (docs/design/LIVE_BLIND.md): an effect in the
  cue in edit plays for the rig views, on the Blind view's own clock, and
  the stage does not change by a byte.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>
#include <QtMath>

#include "blindeffects_test.h"
#define private public                  // MasterTimer::timerTick(), the runner's instances
#define protected public                // Universe::processFaders()
#include "doc.h"
#include "mastertimer.h"
#include "universe.h"
#include "inputoutputmap.h"
#include "effectscriptrunner.h"
#undef protected
#undef private
#include "blindeffects.h"
#include "collection.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "qlcchannel.h"
#include "qlcfixturedef.h"
#include "qlcfixturehead.h"
#include "qlcfixturemode.h"
#include "qlcpalette.h"
#include "scene.h"
#include "showtreecontroller.h"

void BlindEffects_Test::init()
{
    m_doc = new Doc(this);
    m_dimmers.clear();
    for (int i = 0; i < 3; ++i)
    {
        Fixture *fx = new Fixture(m_doc);
        fx->setName(QString("Dim %1").arg(i + 1));
        fx->setUniverse(0);
        fx->setAddress(quint32(i));
        fx->setChannels(1);
        QVERIFY(m_doc->addFixture(fx));
        m_dimmers << fx->id();
    }
}

void BlindEffects_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    delete m_doc;
    m_doc = nullptr;
}

Fixture *BlindEffects_Test::mover()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Mover");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    const QLCChannel::Preset pr[2] = { QLCChannel::PositionPan, QLCChannel::PositionTilt };
    for (int c = 0; c < 2; c++)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(c == 0 ? "Pan" : "Tilt");
        ch->setPreset(pr[c]);
        def->addChannel(ch);
        mode->insertChannel(ch, c);
    }
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1);
    mode->insertHead(-1, head);
    mode->cacheHeads();
    def->addMode(mode);
    Fixture *fx = new Fixture(m_doc);
    fx->setName("Mover");
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(0); fx->setAddress(10);
    if (!m_doc->addFixture(fx))
    {
        delete fx;
        return nullptr;
    }
    return fx;
}

QLCPalette *BlindEffects_Test::effect(const QString &script)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Effect, m_doc);
    p->setName(script);
    p->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/" + script);
    m_doc->addPalette(p);
    return p;
}

Scene *BlindEffects_Test::sceneWith(const QList<quint32> &fixtures, quint32 paletteId)
{
    Scene *s = new Scene(m_doc);
    m_doc->addFunction(s);
    for (quint32 fid : fixtures)
        s->addFixture(fid);
    s->addPalette(paletteId);
    return s;
}

QByteArray BlindEffects_Test::stageFrame()
{
    m_doc->masterTimer()->timerTick();
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    ua.first()->processFaders();
    const QByteArray frame = *ua.first()->postGMValues();
    m_doc->inputOutputMap()->releaseUniverses(false);
    return frame;
}

void BlindEffects_Test::aDimmerEffectPulsesBlind()
{
    QLCPalette *pulse = effect("dimmer-phaser.js");    // a sine, once a second
    pulse->setEffectParamValue("rate", 1.0);
    Scene *scene = sceneWith(m_dimmers, pulse->id());

    BlindEffects blind(m_doc);
    blind.setScenes({ scene->id() });
    QVERIFY(blind.hasEffects());
    QCOMPARE(blind.clockMs(), qint64(0));

    // A quarter of a second a frame: up from dark to full and down again,
    // as 0.5 - 0.5 cos(2 pi t) says.
    const int expected[] = { 0, 128, 255, 128, 0 };
    for (int f = 0; f < 5; ++f)
    {
        if (f > 0)
            blind.advance(250);
        QCOMPARE(blind.clockMs(), qint64(250 * f));
        const QHash<quint32, QByteArray> levels = blind.apply({});
        for (quint32 fid : m_dimmers)
        {
            QVERIFY2(levels.contains(fid), qPrintable(QString("frame %1: no level for %2").arg(f).arg(fid)));
            const int v = uchar(levels.value(fid).at(0));
            QVERIFY2(qAbs(v - expected[f]) <= 2,
                     qPrintable(QString("frame %1 (%2 ms): %3, not %4").arg(f).arg(250 * f).arg(v).arg(expected[f])));
        }
    }

    // Over the cue's own level, an intensity is the higher of the two.
    QHash<quint32, QByteArray> base;
    base.insert(m_dimmers.first(), QByteArray(1, char(200)));
    blind.advance(125);                               // an eighth: about 37
    QCOMPARE(uchar(blind.apply(base).value(m_dimmers.first()).at(0)), uchar(200));
    QVERIFY(uchar(blind.apply(base).value(m_dimmers.last()).at(0)) < 60);
}

void BlindEffects_Test::aPositionEffectMovesBlind()
{
    Fixture *mv = mover();
    QVERIFY(mv != nullptr);
    QLCPalette *bally = effect("ballyhoo.js");
    Scene *scene = sceneWith({ mv->id() }, bally->id());

    BlindEffects blind(m_doc);
    blind.setScenes({ scene->id() });
    QVERIFY(blind.hasEffects());

    QList<QPair<int, int> > path;
    for (int f = 0; f < 6; ++f)
    {
        if (f > 0)
            blind.advance(200);
        const QByteArray v = blind.apply({}).value(mv->id());
        QCOMPARE(v.size(), 2);
        path << qMakePair(int(uchar(v.at(0))), int(uchar(v.at(1))));
    }
    // It moves: no two frames in a row in the same place.
    for (int f = 1; f < path.size(); ++f)
        QVERIFY2(path.at(f) != path.at(f - 1),
                 qPrintable(QString("frame %1 stood still at %2/%3").arg(f)
                            .arg(path.at(f).first).arg(path.at(f).second)));

    // Its own clock, not the wall's: the same time is the same place.
    BlindEffects again(m_doc);
    again.setScenes({ scene->id() });
    again.advance(400);
    const QByteArray v = again.apply({}).value(mv->id());
    QCOMPARE(qMakePair(int(uchar(v.at(0))), int(uchar(v.at(1)))), path.at(2));
}

void BlindEffects_Test::theStageIsUntouchedWhileBlindEffectsRun()
{
    Fixture *mv = mover();
    QVERIFY(mv != nullptr);

    // On stage: a plain look, the first dimmer at 200 and the mover parked.
    Scene *live = new Scene(m_doc);
    m_doc->addFunction(live);
    live->setValue(m_dimmers.first(), 0, 200);
    live->setValue(mv->id(), 0, 30);
    live->setValue(mv->id(), 1, 40);
    live->start(m_doc->masterTimer(), FunctionParent::master());

    // In edit, blind: a pulse over the same dimmers and a sweep on the mover.
    QLCPalette *pulse = effect("dimmer-phaser.js");
    QLCPalette *bally = effect("ballyhoo.js");
    Scene *blindScene = sceneWith(m_dimmers, pulse->id());
    blindScene->addFixture(mv->id());
    blindScene->addPalette(bally->id());

    QByteArray before;
    for (int i = 0; i < 4; ++i)
        before = stageFrame();
    QCOMPARE(uchar(before.at(0)), uchar(200));
    QCOMPARE(uchar(before.at(10)), uchar(30));

    BlindEffects blind(m_doc);
    blind.setScenes({ blindScene->id() });
    QVERIFY(blind.hasEffects());
    QSet<QByteArray> seen;
    for (int f = 0; f < 12; ++f)
    {
        blind.advance(90);
        const QHash<quint32, QByteArray> levels = blind.apply({});
        seen.insert(levels.value(m_dimmers.at(1)) + levels.value(mv->id()));
        QCOMPARE(stageFrame(), before);                // byte for byte
    }
    QVERIFY2(seen.size() >= 10, "the blind effects should be animating meanwhile");
    // The stage's effect engine never had them.
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    QVERIFY(runner == nullptr || runner->m_instances.isEmpty());
    QVERIFY(!blindScene->isRunning());

    blind.clear();
    QVERIFY(!blind.hasEffects());
    QCOMPARE(stageFrame(), before);
    live->stop(FunctionParent::master());
}

void BlindEffects_Test::aCuesEffectsPlayAndKeepTheirTime()
{
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Dims");
    m_doc->addFixtureGroup(g);
    for (quint32 fid : m_dimmers)
        g->assignFixture(fid);
    QLCPalette *pulse = effect("dimmer-phaser.js");
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, m_doc);
    full->setName("Full");
    full->setValue(255);
    m_doc->addPalette(full);

    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ShowTree::ListMode::NonTracking);
    const quint32 fx = st->addCue(listId, -1, "Pulse");
    const quint32 still = st->addCue(listId, 0, "Still");
    ShowTree::Target t;
    t.groups << g->id();
    st->setLayerLook(fx, st->addLayer(fx, t), st->lookForPalette(pulse->id()));
    st->setLayerLook(still, st->addLayer(still, t), st->lookForPalette(full->id()));

    BlindEffects blind(m_doc);
    blind.setCue(fx);
    QVERIFY(blind.hasEffects());
    blind.advance(250);
    QVERIFY(uchar(blind.apply({}).value(m_dimmers.first()).at(0)) > 100);

    // The same cue again (the view's refresh): the same effects, their time kept.
    blind.setCue(fx);
    QCOMPARE(blind.clockMs(), qint64(250));
    // ...and a setting edited shows at once.
    pulse->setEffectParamValue("maxDim", 0.2);
    blind.setCue(fx);
    blind.advance(250);                                // the top of the sine
    const int top = uchar(blind.apply({}).value(m_dimmers.first()).at(0));
    QVERIFY2(qAbs(top - 51) <= 3, qPrintable(QString("capped at 20 %: %1").arg(top)));

    // A cue with no effect: nothing to run; the view is its levels alone.
    blind.setCue(still);
    QVERIFY(!blind.hasEffects());
    QCOMPARE(blind.clockMs(), qint64(0));
    blind.setCue(ShowTree::invalidId());
    QVERIFY(!blind.hasEffects());
    // The effect cue again starts it again.
    blind.setCue(fx);
    QVERIFY(blind.hasEffects());
    QCOMPARE(blind.clockMs(), qint64(0));
}

QTEST_GUILESS_MAIN(BlindEffects_Test)

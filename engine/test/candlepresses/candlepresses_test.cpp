/*
  Q Light Controller Plus
  candlepresses_test.cpp

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

#include "../common/stdprelude.h"
#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private
#include "candlepresses_test.h"
#include "showtreecontroller.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "showtreemodel.h"
#include "chaseraction.h"
#include "fadechannel.h"
#include "showreview.h"
#include "candleremote.h"
#include "collection.h"
#include "qlcchannel.h"
#include "chaserstep.h"
#include "cueoutput.h"
#include "presses.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
QLCFixtureDef *remoteDef()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    // Not __FILE__: Ninja compiles with a relative one (Linux CI).
    const QString path = QStringLiteral(QLC_TEST_RESOURCE_ROOT "fixtures/Generic/Generic-IR-Candle-Remote.qxf");
    if (def->loadXML(path) != QFile::NoError)
    {
        delete def;
        return nullptr;
    }
    return def;
}

Target functionTarget(quint32 fid)
{
    Target t;
    t.function = fid;
    return t;
}

const int kSelect = 0, kWindow = 1;
} // namespace

void CandlePresses_Test::init()
{
    m_doc = new Doc(this);
    m_remote = nullptr;
    m_dimmer = nullptr;
    m_lastSelect = m_lastWindow = -1;
    m_presses = 0;
}

void CandlePresses_Test::cleanup()
{
    if (m_doc != nullptr)
    {
        for (Function *f : m_doc->functions())
            f->stop(FunctionParent::master());
        tick(5);
    }
    delete m_doc;
    m_doc = nullptr;
}

/****************************************************************************
 * Helpers
 ****************************************************************************/

/* The remote on universe 1 at 1 (Select 1, Window 2) and a plain dimmer at
   10, so a list can light something beside the candles. */
bool CandlePresses_Test::rig()
{
    QLCFixtureDef *def = remoteDef();
    if (def == nullptr || !m_doc->fixtureDefCache()->addFixtureDef(def))
        return false;
    m_remote = new Fixture(m_doc);
    m_remote->setName("Candles");
    m_remote->setFixtureDefinition(def, def->modes().first());
    m_remote->setUniverse(0);
    m_remote->setAddress(0);
    if (!m_doc->addFixture(m_remote))
        return false;
    m_dimmer = new Fixture(m_doc);
    m_dimmer->setName("Special");
    m_dimmer->setChannels(1);
    m_dimmer->setUniverse(0);
    m_dimmer->setAddress(9);
    return m_doc->addFixture(m_dimmer);
}

/* A scene the cue plays as a layer (+ Scene): Select and Window on the
   remote (-1: not set). */
Scene *CandlePresses_Test::candles(const QString &name, int select, int window)
{
    Scene *s = new Scene(m_doc);
    s->setName(name);
    if (select >= 0)
        s->setValue(m_remote->id(), kSelect, uchar(select));
    if (window >= 0)
        s->setValue(m_remote->id(), kWindow, uchar(window));
    if (!m_doc->addFunction(s))
    {
        delete s;
        return nullptr;
    }
    return s;
}

quint32 CandlePresses_Test::layer(quint32 cue, const Scene *scene)
{
    return m_doc->showTree()->addLayer(cue, functionTarget(scene->id()));
}

Chaser *CandlePresses_Test::chaserOf(quint32 listId) const
{
    const CueList *list = m_doc->showTree()->cueList(listId);
    return list == nullptr ? nullptr : qobject_cast<Chaser *>(m_doc->function(list->chaser));
}

int CandlePresses_Test::cueSends(quint32 cueId, quint32 ch) const
{
    const ShowTree::Cue *c = m_doc->showTree()->cue(cueId);
    if (c == nullptr)
        return -2;
    const QHash<quint32, QHash<quint32, uchar>> out = CueOutput::compute(m_doc, c->collection);
    const QHash<quint32, uchar> fx = out.value(m_remote->id());
    return fx.contains(ch) ? int(fx.value(ch)) : -1;
}

void CandlePresses_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
        if (m_remote == nullptr)
            continue;
        // The blaster (DMX-CHART.md): a change of Select or Window while
        // Select is Dim or Brighten starts that button's window again.
        const int sel = select(), win = window();
        if ((sel != m_lastSelect || win != m_lastWindow) && (sel == CandleRemote::Dim || sel == CandleRemote::Brighten))
            ++m_presses;
        m_lastSelect = sel;
        m_lastWindow = win;
    }
}

static ChaserAction stepAction(ChaserActionType type, int index = -1)
{
    ChaserAction action;
    action.m_action = type;
    action.m_stepIndex = index;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    return action;
}

void CandlePresses_Test::go(Chaser *c)
{
    {
        ChaserAction a = stepAction(ChaserNextStep);
        c->setAction(a);
    }
    if (!c->isRunning())
        c->start(m_doc->masterTimer(), FunctionParent::master());
    land();
}

void CandlePresses_Test::back(Chaser *c)
{
    {
        ChaserAction a = stepAction(ChaserPreviousStep);
        c->setAction(a);
    }
    land();
}

void CandlePresses_Test::jump(Chaser *c, int index)
{
    {
        ChaserAction a = stepAction(ChaserSetStepIndex, index);
        c->setAction(a);
    }
    if (!c->isRunning())
        c->start(m_doc->masterTimer(), FunctionParent::master());
    land();
}

void CandlePresses_Test::land()
{
    tick(12);
}

int CandlePresses_Test::select() const
{
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    const int v = uchar(ua.first()->preGMValues().at(int(m_remote->address()) + kSelect));
    m_doc->inputOutputMap()->releaseUniverses(false);
    return v;
}

int CandlePresses_Test::window() const
{
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    const int v = uchar(ua.first()->preGMValues().at(int(m_remote->address()) + kWindow));
    m_doc->inputOutputMap()->releaseUniverses(false);
    return v;
}

/****************************************************************************
 * Tests
 ****************************************************************************/

void CandlePresses_Test::pressComesFromTheDefinition()
{
    QLCFixtureDef *def = remoteDef();
    QVERIFY(def != nullptr);
    const QLCChannel *sel = def->modes().first()->channel(0);
    const QLCChannel *win = def->modes().first()->channel(1);
    QVERIFY(sel->hasPress());
    QVERIFY(!win->hasPress());
    for (int v = 0; v <= 255; ++v)
        QCOMPARE(sel->isPressValue(uchar(v)), v == 5 || v == 6);
    QVERIFY(!win->isPressValue(5));

    // Saved and read back; copied.
    QLCCapability *dim = sel->searchCapability(uchar(5));
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    QVERIFY(dim->saveXML(&w));
    w.writeEndDocument();
    QVERIFY(buf.data().contains("Press=\"true\""));
    QXmlStreamReader r(buf.data());
    while (!r.atEnd() && !r.isStartElement())
        r.readNext();
    QLCCapability back;
    QVERIFY(back.loadXML(r));
    QVERIFY(back.isPress());
    QCOMPARE(back.name(), QString("Dim"));
    QLCCapability *copy = dim->createCopy();
    QVERIFY(copy->isPress());
    delete copy;
    QLCChannel *chCopy = const_cast<QLCChannel *>(sel)->createCopy();
    QVERIFY(chCopy->hasPress());
    QLCChannel assigned;
    assigned = *sel;
    QVERIFY(assigned.hasPress());
    // Taking the presses away: no longer a press channel.
    for (QLCCapability *c : chCopy->capabilities())
        if (c->isPress())
            chCopy->removeCapability(c);
    QVERIFY(!chCopy->hasPress());
    delete chCopy;

    // A capability without the attribute (every other definition): no press.
    const QByteArray plain("<Capability Min=\"5\" Max=\"5\">Dim</Capability>");
    QXmlStreamReader r2(plain);
    r2.readNextStartElement();
    QLCCapability other;
    QVERIFY(other.loadXML(r2));
    QVERIFY(!other.isPress());
    delete def;
}

void CandlePresses_Test::aPressChannelNeverFades()
{
    QVERIFY(rig());
    QVERIFY(!FadeChannel(m_doc, m_remote->id(), kSelect).canFade());
    QVERIFY(FadeChannel(m_doc, m_remote->id(), kWindow).canFade());
    QVERIFY(FadeChannel(m_doc, m_dimmer->id(), 0).canFade());

    // A cue with a 2 s fade from On to Dim: Select goes straight there,
    // never through Off, Candle or Light.
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(listId, -1, "On");
    QVERIFY(layer(c1, candles("On", CandleRemote::On, 20)) != invalidId());
    const quint32 c2 = st->addCue(listId, 0, "Dim");
    QVERIFY(layer(c2, candles("Dim", CandleRemote::Dim, 20)) != invalidId());
    Scene *special = new Scene(m_doc);
    special->setValue(m_dimmer->id(), 0, 255);
    QVERIFY(m_doc->addFunction(special));
    QVERIFY(layer(c2, special) != invalidId());
    ShowTree::Cue cue2 = *st->cue(c2);
    cue2.fadeIn = 2000;
    QVERIFY(st->setCue(cue2));
    Chaser *c = chaserOf(listId);
    QVERIFY(c != nullptr);
    go(c);
    QCOMPARE(select(), int(CandleRemote::On));
    {
        ChaserAction a = stepAction(ChaserNextStep);
        c->setAction(a);
    }
    QSet<int> seen;
    bool dimmerFaded = false;
    for (int i = 0; i < 150; ++i)
    {
        tick();
        seen.insert(select());
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        const int d = uchar(ua.first()->preGMValues().at(int(m_dimmer->address())));
        m_doc->inputOutputMap()->releaseUniverses(false);
        dimmerFaded = dimmerFaded || (d > 0 && d < 255);
    }
    QVERIFY(dimmerFaded);                               // the cue did fade
    QCOMPARE(seen, QSet<int>({ int(CandleRemote::Dim) }));
}

void CandlePresses_Test::dimDoesNotTrack()
{
    QVERIFY(rig());
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(listId, -1, "Dim");
    const Scene *dim = candles("Dim", CandleRemote::Dim, 20);
    QVERIFY(layer(c1, dim) != invalidId());
    const quint32 c2 = st->addCue(listId, 0, "Next");
    Scene *special = new Scene(m_doc);
    special->setValue(m_dimmer->id(), 0, 255);
    QVERIFY(m_doc->addFunction(special));
    QVERIFY(layer(c2, special) != invalidId());
    const quint32 c3 = st->addCue(listId, 1, "And on");

    // The Dim layer tracks (cue 2 still plays it) ...
    const QList<ResolvedCue> rcs = st->resolved(*st->cueList(listId));
    QVERIFY(rcs.at(1).layer(functionTarget(dim->id())) != nullptr);
    QVERIFY(!rcs.at(1).layer(functionTarget(dim->id()))->own);
    // ... but its press doesn't: 5 in cue 1, 0 after.
    QCOMPARE(cueSends(c1, kSelect), int(CandleRemote::Dim));
    QCOMPARE(cueSends(c2, kSelect), 0);
    QCOMPARE(cueSends(c3, kSelect), 0);
    QCOMPARE(cueSends(c2, kWindow), 20);                // the rest tracks as ever

    const QList<QHash<quint64, uchar>> guards = st->pressGuards(*st->cueList(listId));
    QCOMPARE(guards.count(), 3);
    QVERIFY(guards.at(0).isEmpty());                    // newly programmed here
    const quint64 k = Presses::key(m_remote->id(), kSelect);
    QCOMPARE(guards.at(1), (QHash<quint64, uchar>{ { k, 0 } }));
    QCOMPARE(guards.at(2), (QHash<quint64, uchar>{ { k, 0 } }));
    // One guard scene, shared by the cues it is the same in (it plays on
    // across GO), last in each cue.
    const Collection *coll2 = qobject_cast<const Collection *>(m_doc->function(st->cue(c2)->collection));
    const Collection *coll3 = qobject_cast<const Collection *>(m_doc->function(st->cue(c3)->collection));
    QVERIFY(coll2 != nullptr && coll3 != nullptr);
    QCOMPARE(coll2->functions().last(), coll3->functions().last());
    QVERIFY(st->isGenerated(coll2->functions().last()));
    QVERIFY(coll2->functions().contains(dim->id()));

    // Played: GO GO GO -- one Dim, and Select 0 in cues 2 and 3.
    Chaser *c = chaserOf(listId);
    go(c);
    QCOMPARE(select(), int(CandleRemote::Dim));
    QCOMPARE(m_presses, 1);
    go(c);
    QCOMPARE(select(), 0);
    go(c);
    QCOMPARE(select(), 0);
    QCOMPARE(m_presses, 1);

    // Brighten the same.
    const quint32 c4 = st->addCue(listId, 2, "Brighten");
    QVERIFY(layer(c4, candles("Brighten", CandleRemote::Brighten, 20)) != invalidId());
    const quint32 c5 = st->addCue(listId, 3, "After");
    QCOMPARE(cueSends(c4, kSelect), int(CandleRemote::Brighten));
    QCOMPARE(cueSends(c5, kSelect), 0);
}

void CandlePresses_Test::statesTrack()
{
    QVERIFY(rig());
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Show");
    for (int state : { int(CandleRemote::On), int(CandleRemote::Off), int(CandleRemote::Candle),
                       int(CandleRemote::Light) })
    {
        // A list with no press: no guard at all; the state tracks.
        const quint32 listId = st->addCueList(prod, QStringLiteral("States %1").arg(state));
        const quint32 c1 = st->addCue(listId, -1, "Set");
        QVERIFY(layer(c1, candles("State", state, 30)) != invalidId());
        const quint32 c2 = st->addCue(listId, 0, "Tracks");
        QCOMPARE(cueSends(c1, kSelect), state);
        QCOMPARE(cueSends(c2, kSelect), state);
        for (const QHash<quint64, uchar> &g : st->pressGuards(*st->cueList(listId)))
            QVERIFY(g.isEmpty());
        const Collection *coll2 = qobject_cast<const Collection *>(m_doc->function(st->cue(c2)->collection));
        QCOMPARE(coll2->functions().count(), 1);        // just the tracked layer

        // A Dim later in the list: the state still tracks up to it ...
        const quint32 c3 = st->addCue(listId, 1, "Dim");
        const quint32 dimLayer = layer(c3, candles("Dim", CandleRemote::Dim, 20));
        QVERIFY(dimLayer != invalidId());
        QVERIFY(st->setLayerRelease(c3, dimLayer, true));
        const quint32 c4 = st->addCue(listId, 2, "After");
        QCOMPARE(cueSends(c2, kSelect), state);
        QCOMPARE(cueSends(c3, kSelect), int(CandleRemote::Dim));
        // ... and after it the older state, still tracking, isn't sent again
        // (On would undo the Dim): nothing.
        QVERIFY(st->resolved(*st->cueList(listId)).at(3).layer(
                    functionTarget(m_doc->showTree()->cue(c1)->layers.first().target.function)) != nullptr);
        QCOMPARE(cueSends(c4, kSelect), 0);

        // A state set again after the Dim plays and tracks.
        const quint32 c5 = st->addCue(listId, 3, "Again");
        QVERIFY(layer(c5, candles("Again", state, 31)) != invalidId());
        const quint32 c6 = st->addCue(listId, 4, "Tracks again");
        QCOMPARE(cueSends(c5, kSelect), state);
        QCOMPARE(cueSends(c6, kSelect), state);
    }
}

void CandlePresses_Test::backOverADimDoesNotSendItAgain()
{
    QVERIFY(rig());
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(listId, -1, "On");
    QVERIFY(layer(c1, candles("On", CandleRemote::On, 30)) != invalidId());
    const quint32 c2 = st->addCue(listId, 0, "Dim");
    QVERIFY(layer(c2, candles("Dim", CandleRemote::Dim, 20)) != invalidId());
    const quint32 c3 = st->addCue(listId, 1, "Three");
    const quint32 c4 = st->addCue(listId, 2, "Four");
    Q_UNUSED(c3)
    Q_UNUSED(c4)

    Chaser *c = chaserOf(listId);
    QVERIFY(c != nullptr);
    go(c);                                  // 1: On
    QCOMPARE(select(), int(CandleRemote::On));
    go(c);                                  // 2: Dim
    QCOMPARE(m_presses, 1);
    go(c);                                  // 3
    go(c);                                  // 4
    QCOMPARE(c->currentStepIndex(), 3);
    back(c);                                // 4 -> 3: tracked into, nothing sent
    QCOMPARE(c->currentStepIndex(), 2);
    QCOMPARE(select(), 0);
    QCOMPARE(m_presses, 1);
    jump(c, 3);                             // a jump (a re-run) into a cue it tracked into
    jump(c, 2);
    QCOMPARE(select(), 0);
    QCOMPARE(m_presses, 1);
    jump(c, 0);                             // back over it to the top: On, no Dim
    QCOMPARE(select(), int(CandleRemote::On));
    QCOMPARE(m_presses, 1);
    jump(c, 2);
    QCOMPARE(m_presses, 1);

    // Back onto the Dim cue itself plays it: that is what CUE-17 warns of.
    back(c);
    QCOMPARE(c->currentStepIndex(), 1);
    QCOMPARE(select(), int(CandleRemote::Dim));
    QCOMPARE(m_presses, 2);
}

void CandlePresses_Test::aWindowChangeDoesNotRestartAPress()
{
    QVERIFY(rig());
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(listId, -1, "Dim");
    QVERIFY(layer(c1, candles("Dim", CandleRemote::Dim, 20)) != invalidId());
    const quint32 c2 = st->addCue(listId, 0, "Longer window");
    QVERIFY(layer(c2, candles("Window", -1, 40)) != invalidId());
    QCOMPARE(cueSends(c2, kSelect), 0);
    QCOMPARE(cueSends(c2, kWindow), 40);

    Chaser *c = chaserOf(listId);
    go(c);
    QCOMPARE(m_presses, 1);
    go(c);
    QCOMPARE(select(), 0);
    QCOMPARE(window(), 40);
    QCOMPARE(m_presses, 1);
    back(c);                                    // the Dim cue itself: again
    QCOMPARE(m_presses, 2);
    go(c);
    QCOMPARE(m_presses, 2);
}

void CandlePresses_Test::releaseBaseAndIncludeDoNotResend()
{
    QVERIFY(rig());
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("Show"), "Act 1");
    // 1: the base cue, which dims. 2: dims again (its own), released after
    // -- back to the base's Dim layer. 3: that released cue. 4: a Block
    // that includes cue 1.
    const quint32 c1 = st->addCue(listId, -1, "Base");
    const Scene *dim = candles("Dim", CandleRemote::Dim, 20);
    QVERIFY(layer(c1, dim) != invalidId());
    QVERIFY(st->setBaseCue(listId, c1));
    const quint32 c2 = st->addCue(listId, 0, "Dim again");
    const quint32 dim2 = layer(c2, dim);
    QVERIFY(dim2 != invalidId());
    QVERIFY(st->setLayerRelease(c2, dim2, true));
    const quint32 c3 = st->addCue(listId, 1, "Released");
    const QList<ResolvedCue> rcs = st->resolved(*st->cueList(listId));
    const ResolvedLayer *back = rcs.at(2).layer(functionTarget(dim->id()));
    QVERIFY(back != nullptr);
    QCOMPARE(int(back->origin), int(Origin::Base));
    QCOMPARE(cueSends(c2, kSelect), int(CandleRemote::Dim));   // its own: newly programmed
    QCOMPARE(cueSends(c3, kSelect), 0);

    const quint32 c4 = st->addCue(listId, 2, "Includes the base");
    QVERIFY(st->setCueMode(c4, CueMode::Block));
    QVERIFY(st->addInclude(c4, c1));
    QCOMPARE(cueSends(c4, kSelect), 0);

    // A Release After This Cue on the press: the next cue sends 0 there
    // (nothing plays it), so a Window change can't restart it.
    const quint32 list2 = st->addCueList(st->addProduction("Two"), "Act 2");
    const quint32 d1 = st->addCue(list2, -1, "Dim");
    const quint32 dl = layer(d1, dim);
    QVERIFY(st->setLayerRelease(d1, dl, true));
    const quint32 d2 = st->addCue(list2, 0, "Window");
    QVERIFY(layer(d2, candles("Window", -1, 40)) != invalidId());
    QCOMPARE(cueSends(d2, kSelect), 0);

    // A cue that sets On and then Dim (the later layer wins): what it sent
    // was the Dim, so On tracking on is not sent again after it.
    const quint32 list3 = st->addCueList(st->addProduction("Three"), "Act 3");
    const quint32 e1 = st->addCue(list3, -1, "On, then Dim");
    QVERIFY(layer(e1, candles("On", CandleRemote::On, 30)) != invalidId());
    const quint32 el = layer(e1, dim);
    QVERIFY(st->setLayerRelease(e1, el, true));
    const quint32 e2 = st->addCue(list3, 0, "After");
    QCOMPARE(cueSends(e1, kSelect), int(CandleRemote::Dim));
    QCOMPARE(cueSends(e2, kSelect), 0);
}

void CandlePresses_Test::olderShowsUnaffected()
{
    QVERIFY(rig());
    ShowTreeController *st = m_doc->showTree();
    // No press anywhere: the remote's states and a special, as before.
    const quint32 listId = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(listId, -1, "On");
    QVERIFY(layer(c1, candles("On", CandleRemote::On, 30)) != invalidId());
    Scene *special = new Scene(m_doc);
    special->setValue(m_dimmer->id(), 0, 200);
    QVERIFY(m_doc->addFunction(special));
    const quint32 c2 = st->addCue(listId, 0, "Special");
    QVERIFY(layer(c2, special) != invalidId());
    const int functions = m_doc->functions().count();
    st->forgetPlans();
    st->bake();
    QCOMPARE(m_doc->functions().count(), functions);
    for (quint32 cid : { c1, c2 })
    {
        const Collection *coll = qobject_cast<const Collection *>(m_doc->function(st->cue(cid)->collection));
        for (quint32 fid : coll->functions())
            QVERIFY2(!st->isGenerated(fid), "a function layer only: nothing generated beside it");
    }

    // With a Dim: a guard; saved and loaded, the same one is found again.
    const quint32 c3 = st->addCue(listId, 1, "Dim");
    QVERIFY(layer(c3, candles("Dim", CandleRemote::Dim, 20)) != invalidId());
    const quint32 c4 = st->addCue(listId, 2, "After");
    const Collection *coll4 = qobject_cast<const Collection *>(m_doc->function(st->cue(c4)->collection));
    const quint32 guard = coll4->functions().last();
    QVERIFY(st->isGenerated(guard));
    QVERIFY(!st->describeGenerated(guard).isEmpty());

    QByteArray xml;
    {
        QBuffer buf(&xml);
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        w.writeStartDocument();
        QVERIFY(m_doc->saveXML(&w));
        w.writeEndDocument();
    }
    const int before = m_doc->functions().count();

    Doc *other = new Doc(this);
    QLCFixtureDef *def = remoteDef();
    QVERIFY(def != nullptr && other->fixtureDefCache()->addFixtureDef(def));
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other->loadXML(r, false));
    other->showTree()->bake();
    QCOMPARE(other->functions().count(), before);
    QVERIFY(other->showTree()->isGenerated(guard));
    const Collection *again = qobject_cast<const Collection *>(
        other->function(other->showTree()->cue(c4)->collection));
    QVERIFY(again != nullptr);
    QCOMPARE(again->functions().last(), guard);
    // A hidden scene is saved with its values as 0: the guard holding On in
    // cue 2 is set to it again, the same function.
    const quint32 guard2 = qobject_cast<const Collection *>(m_doc->function(st->cue(c2)->collection))->functions().last();
    const Collection *again2 = qobject_cast<const Collection *>(
        other->function(other->showTree()->cue(c2)->collection));
    QCOMPARE(again2->functions().last(), guard2);
    QCOMPARE(int(CueOutput::compute(other, again2->id()).value(m_remote->id()).value(kSelect, 99)),
             int(CandleRemote::On));
    QCOMPARE(int(CueOutput::compute(other, again->id()).value(m_remote->id()).value(kSelect, 99)), 0);
    delete other;
}

void CandlePresses_Test::showReviewWarnsAboutAPressThatTracks()
{
    QVERIFY(rig());
    ShowTreeController *st = m_doc->showTree();
    ShowReview::Options opt;
    opt.checks << QStringLiteral("CUE-17");
    opt.record = false;
    auto findings = [&]() {
        QList<ReviewFinding> out;
        for (const ReviewFinding &f : m_doc->showReview()->run(opt).findings)
            if (f.check == QLatin1String("CUE-17"))
                out << f;
        return out;
    };
    QVERIFY(ShowReview::check(QStringLiteral("CUE-17")) != nullptr);

    const quint32 listId = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 c1 = st->addCue(listId, -1, "On");
    QVERIFY(layer(c1, candles("On", CandleRemote::On, 30)) != invalidId());
    const quint32 c2 = st->addCue(listId, 0, "Steady");
    QVERIFY(layer(c2, candles("Steady", CandleRemote::Light, 30)) != invalidId());
    st->addCue(listId, 1, "After");
    QVERIFY(findings().isEmpty());                      // states track quietly

    const quint32 c4 = st->addCue(listId, 2, "Dim");
    const quint32 dimLayer = layer(c4, candles("Dim", CandleRemote::Dim, 20));
    QVERIFY(dimLayer != invalidId());
    st->addCue(listId, 3, "Next");
    QList<ReviewFinding> fs = findings();
    QCOMPARE(fs.count(), 1);
    QCOMPARE(fs.first().severity, ReviewFinding::Warning);
    QVERIFY2(fs.first().message.startsWith(QStringLiteral("Cue ")), qPrintable(fs.first().message));
    QVERIFY2(fs.first().message.contains(QStringLiteral("Dim (Act 1) presses Dim on the candles; it fires once per "
                                                        "GO — a Back or a re-run dims them again.")),
             qPrintable(fs.first().message));
    QVERIFY(fs.first().targets.contains(QStringLiteral("list:%1/cue:%2").arg(listId).arg(c4)));
    QCOMPARE(fs.first().jumpData.value(0).toUInt(), c4);

    // Released after its cue (the way to use one): quiet.
    QVERIFY(st->setLayerRelease(c4, dimLayer, true));
    QVERIFY(findings().isEmpty());
    // The last cue of a list has nowhere to track to: quiet.
    const quint32 last = st->addCue(listId, 4, "Last");
    QVERIFY(layer(last, candles("Dim at the end", CandleRemote::Dim, 20)) != invalidId());
    QVERIFY(findings().isEmpty());

    // A chase that presses Brighten every time round.
    Chaser *chase = new Chaser(m_doc);
    chase->setName("Flicker up");
    QVERIFY(m_doc->addFunction(chase));
    chase->addStep(ChaserStep(candles("Up", CandleRemote::Brighten, 10)->id()));
    chase->addStep(ChaserStep(candles("Rest", 0, 10)->id()));
    const quint32 c6 = st->addCue(listId, 4, "Chase");
    Target t;
    t.function = chase->id();
    const quint32 chaseLayer = st->addLayer(c6, t);
    QVERIFY(chaseLayer != invalidId());
    QVERIFY(st->setLayerRelease(c6, chaseLayer, true));
    fs = findings();
    QCOMPARE(fs.count(), 1);
    QVERIFY2(fs.first().message.contains(QStringLiteral("plays the chase 'Flicker up', which presses Brighten on the "
                                                        "candles every time it comes round: each pass brightens them again")),
             qPrintable(fs.first().message));
}

QTEST_MAIN(CandlePresses_Test)

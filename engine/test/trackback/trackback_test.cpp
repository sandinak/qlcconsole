/*
  Q Light Controller Plus
  trackback_test.cpp

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
#include <QBuffer>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QtTest>
#include <future>

#include "../common/stdprelude.h"
#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#include "genericfader.h"
#include "outputrulekeeper.h"
#undef protected
#undef private
#include "outputlayer.h"
#include "programmercontroller.h"
#include "trackback_test.h"
#include "showtreecontroller.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "qlcfixturedefcache.h"
#include "showembedding.h"
#include "showcontrol.h"
#include "fixturegroup.h"
#include "showtreemodel.h"
#include "chaseraction.h"
#include "qlcpalette.h"
#include "chaserstep.h"
#include "resource_paths.h"
#include "qlcfile.h"
#include "fixture.h"
#include "scene.h"
#include "scenery.h"
#include "fixturerest.h"
#include "outputrulekeeper.h"
#include "showcontrol.h"
#include "qlcchannel.h"
#include "chaser.h"
#include "doc.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcphysical.h"
#include "showreview.h"

using namespace ShowTree;

namespace
{

Target groupTarget(quint32 g)
{
    Target t;
    t.groups << g;
    return t;
}

} // namespace

void TrackBack_Test::init()
{
    m_doc = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir));
}

void TrackBack_Test::cleanup()
{
    stopAll();
    delete m_doc;
    m_doc = nullptr;
}

/****************************************************************************
 * Helpers
 ****************************************************************************/

void TrackBack_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        for (Universe *u : ua)
            u->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
}

/* Every universe's values once the fades are over. */
QByteArray TrackBack_Test::settle(int ticks)
{
    tick(ticks);
    QByteArray out;
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    for (Universe *u : ua)
        out += *u->postGMValues();
    m_doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

/* Stopped, and the universes cleared: a dark stage, nothing left on an LTP
   channel from the walk before. */
void TrackBack_Test::stopAll()
{
    if (m_doc == nullptr)
        return;
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(5);
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    for (Universe *u : ua)
        u->reset();
    m_doc->inputOutputMap()->releaseUniverses(false);
    // An unpatched fixture's drawing, dark too.
    for (Fixture *f : m_doc->fixtures())
        if (!f->isPatched())
            for (quint32 i = 0; i < f->channels(); ++i)
                f->setPreviewValue(i, 0);
}

/* As the footer and show control drive a list (ShowControl::go / Back). */
void TrackBack_Test::act(Chaser *c, const Move &m)
{
    ShowControl *sc = m_doc->showControl();
    sc->setMainCueList(c->id());
    switch (m.step)
    {
        case Go:
        {
            ShowControl::Request r;
            r.command = ShowControl::Go;
            r.chaser = c->id();
            sc->execute(r);     // at the end of the list: nothing
        }
        break;
        case Back:
        {
            ShowControl::Request r;
            r.command = ShowControl::Back;
            r.chaser = c->id();
            sc->execute(r);     // stopped: nothing
        }
        break;
        case Jump:
        {
            ChaserAction action;
            action.m_action = ChaserSetStepIndex;
            action.m_stepIndex = m.cue;
            action.m_masterIntensity = 1.0;
            action.m_stepIntensity = 1.0;
            action.m_fadeMode = Chaser::FromFunction;
            c->setAction(action);
            if (!c->isRunning())
                c->start(m_doc->masterTimer(), FunctionParent::master());
        }
        break;
    }
}

QMap<int, QByteArray> TrackBack_Test::straightRun(Chaser *c)
{
    // Long enough for the longest fade to end.
    uint longest = 0;
    for (const ChaserStep &step : c->steps())
        for (uint t : { step.fadeIn, step.fadeOut })
            if (t <= 10000)
                longest = qMax(longest, t);
    m_settle = int(longest / MasterTimer::tick()) + 8;

    stopAll();
    m_moving.clear();
    QMap<int, QByteArray> ref;
    for (int guard = 0; guard < 2 * c->stepsCount(); ++guard)
    {
        act(c, { Go, 0 });
        const QByteArray out = land(c);
        const int at = c->currentStepIndex();
        if (!c->isRunning())
            break;
        // A channel an effect moves has no one value to compare: watched
        // for a while, the ones that change are left out.
        QByteArray moving(out.size(), char(0));
        for (int s = 0; s < 8; ++s)
        {
            const QByteArray later = settle(7);
            for (int ch = 0; ch < qMin(out.size(), later.size()); ++ch)
                if (later.at(ch) != out.at(ch))
                    moving[ch] = 1;
        }
        // A cue that follows on by itself before it is still: left out.
        if (c->isRunning() && c->currentStepIndex() == at && !ref.contains(at))
        {
            ref.insert(at, out);
            m_moving.insert(at, moving);
        }
        if (!c->isRunning() || c->currentStepIndex() >= c->stepsCount() - 1)
            break;
    }
    return ref;
}

/* Settled after a GO, Back or jump: the fades over, and again after a cue
   that followed on by itself meanwhile. */
QByteArray TrackBack_Test::land(Chaser *c)
{
    tick(2);                            // the GO, Back or jump taken
    int at = c->currentStepIndex();
    QByteArray out = settleCue(c);
    for (int guard = 0; guard < 8 && c->isRunning() && c->currentStepIndex() != at; ++guard)
    {
        at = c->currentStepIndex();
        out = settleCue(c);
    }
    return out;
}

/* m_settle is the list's longest fade: most cues are still long before it
   (a snap in a list with a 3 s fade somewhere), and ticking the whole rig
   on to it every time was most of courseShows' minute. Stops once still()
   has held for three ticks with the stage unchanged -- what the rest of
   m_settle would end on (checked against the full settle on every landing
   of this file's tests: no difference) -- and never on a cue that follows
   on by itself. */
QByteArray TrackBack_Test::settleCue(Chaser *c)
{
    const int least = 4, held = 3;
    QByteArray last;
    int same = 0;
    for (int t = 1; t <= m_settle; ++t)
    {
        const QByteArray out = settle(1);
        if (t >= least - held && out == last && still(c))
        {
            if (++same >= held && t >= least)
                return out;
        }
        else
        {
            same = 0;
        }
        last = out;
    }
    return last;
}

bool TrackBack_Test::still(Chaser *c) const
{
    // The cue holds until a GO: a timed one follows on by itself later.
    if (c->isRunning())
    {
        const int at = c->currentStepIndex();
        uint hold = 0;
        if (c->durationMode() == Chaser::PerStep && at >= 0 && at < c->stepsCount())
            hold = c->steps().at(at).hold;
        else if (c->durationMode() == Chaser::Common)
            hold = c->duration();
        if (hold != Function::infiniteSpeed())
            return false;
    }
    // Every channel of every fader at its target: none is fading, or
    // waiting out a delay to.
    for (Universe *u : m_doc->inputOutputMap()->universes())
    {
        QMutexLocker locker(&u->m_fadersMutex);
        for (const QSharedPointer<GenericFader> &fader : u->m_faders)
        {
            if (fader.isNull())
                continue;
            const QHash<quint32, FadeChannel> channels = fader->channels();
            for (auto it = channels.cbegin(); it != channels.cend(); ++it)
                if (it.value().current() != it.value().target())
                    return false;
        }
    }
    return true;
}

int TrackBack_Test::walk(Chaser *c, const QMap<int, QByteArray> &ref, const QList<Move> &moves, QString *why)
{
    stopAll();
    int differ = 0;
    QStringList path;
    for (const Move &m : moves)
    {
        // GO from the last cue of a Loop list goes round: the second time
        // through carries the first's end on, which a run from the top
        // doesn't have.
        if (m.step == Go && c->isRunning() && c->currentStepIndex() == c->stepsCount() - 1)
            break;
        act(c, m);
        path << (m.step == Go ? QStringLiteral("GO") : m.step == Back ? QStringLiteral("BACK")
                                                     : QStringLiteral("JUMP %1").arg(m.cue + 1));
        const QByteArray out = land(c);
        const int at = c->currentStepIndex();
        // GO past the last cue stops the list: a GO after starts it again
        // from whatever the stage has, which the run from the top didn't.
        if (!c->isRunning())
            break;
        if (!ref.contains(at))
            continue;
        const QByteArray &want = ref[at];
        const QByteArray &moving = m_moving[at];
        int here = 0;
        QStringList chans;
        for (int i = 0; i < qMin(out.size(), want.size()); ++i)
            if (out.at(i) != want.at(i) && moving.at(i) == 0)
            {
                ++here;
                if (chans.count() < 6)
                {
                    QString what;
                    for (Fixture *f : m_doc->fixtures())
                    {
                        const int base = int(f->universeAddress());
                        if (i < base || i >= base + int(f->channels()))
                            continue;
                        const QLCChannel *ch = f->channel(quint32(i - base));
                        what = QStringLiteral(" [%1 %2]").arg(f->name(), ch ? ch->name() : QString());
                    }
                    chans << QStringLiteral("ch%1 %2 (straight run %3)%4").arg(i + 1)
                                 .arg(uchar(out.at(i))).arg(uchar(want.at(i))).arg(what);
                }
            }
        if (here > 0 && qEnvironmentVariableIsSet("TRACKBACK_VERBOSE"))
            qWarning().noquote() << "MISMATCH" << path.join(QLatin1Char(' ')) << "-> cue" << at + 1 << chans.join(", ");
        if (here > 0 && why != nullptr && why->isEmpty())
            *why = QStringLiteral("%1 -> cue %2: %3 channels differ: %4")
                   .arg(path.join(QLatin1Char(' '))).arg(at + 1).arg(here).arg(chans.join(QStringLiteral(", ")));
        differ += here;
    }
    return differ;
}

int TrackBack_Test::everyWay(Chaser *c, int randomWalks, QString *why)
{
    const QMap<int, QByteArray> ref = straightRun(c);
    const int n = c->stepsCount();
    if (ref.isEmpty())
        return 0;
    int differ = 0;
    auto gos = [](int k) {
        QList<Move> ms;
        for (int i = 0; i < k; ++i)
            ms << Move { Go, 0 };
        return ms;
    };

    for (int target = 0; target < n; ++target)
    {
        // A jump from a dark stage, and from every other cue.
        differ += walk(c, ref, { { Jump, target } }, why);
        for (int from = 0; from < n; ++from)
            differ += walk(c, ref, { { Jump, from }, { Jump, target } }, why);
        // Back from the cue after.
        if (target + 1 < n)
            differ += walk(c, ref, gos(target + 2) << Move { Back, 0 }, why);
        // GO up to here, Back some, then GO on to the end.
        for (int backs = 1; backs <= target; ++backs)
        {
            QList<Move> ms = gos(target + 1);
            for (int b = 0; b < backs; ++b)
                ms << Move { Back, 0 };
            ms << gos(n - 1 - (target - backs));
            differ += walk(c, ref, ms, why);
        }
    }

    // Random walks: GO, Back and jumps, from a fixed seed.
    QRandomGenerator rng(20261021);
    for (int w = 0; w < randomWalks; ++w)
    {
        QList<Move> ms { { Go, 0 } };
        int at = 0;
        for (int s = 0; s < 3 * n + 4; ++s)
        {
            const int r = int(rng.bounded(10));
            if (r < 5 && at + 1 < n)
            {
                ms << Move { Go, 0 };
                ++at;
            }
            else if (r < 8 && at > 0)
            {
                ms << Move { Back, 0 };
                --at;
            }
            else
            {
                at = int(rng.bounded(n));
                ms << Move { Jump, at };
            }
        }
        differ += walk(c, ref, ms, why);
    }
    return differ;
}

/****************************************************************************
 * Shows
 ****************************************************************************/

bool TrackBack_Test::rig()
{
    QLCFixtureDef *spot = m_doc->fixtureDefCache()->fixtureDef("Clay Paky", "Sharpy");
    QLCFixtureDef *par = m_doc->fixtureDefCache()->fixtureDef("Generic", "Generic RGB");
    if (spot == nullptr || par == nullptr)
        return false;
    quint32 address = 0;
    auto fixture = [&](QLCFixtureDef *def, const QString &mode) {
        Fixture *f = new Fixture(m_doc);
        f->setName(def->model());
        f->setFixtureDefinition(def, def->mode(mode));
        f->setAddress(address);
        address += f->channels();
        m_doc->addFixture(f);
        return f->id();
    };
    auto group = [&](const QString &name, const QList<quint32> &fixtures) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        m_doc->addFixtureGroup(g);
        for (quint32 f : fixtures)
            g->assignFixture(f);
        return g->id();
    };
    auto colour = [&](const QString &name, const QColor &c) {
        QLCPalette *p = new QLCPalette(QLCPalette::Color, m_doc);
        p->setName(name);
        p->setValue(QLCPalette::colorToString(c, QColor(Qt::black)));
        m_doc->addPalette(p);
        return p->id();
    };
    auto dimmer = [&](const QString &name, int level) {
        QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
        p->setName(name);
        p->setValue(level);
        m_doc->addPalette(p);
        return p->id();
    };
    auto position = [&](const QString &name, int pan, int tilt) {
        QLCPalette *p = new QLCPalette(QLCPalette::PanTilt, m_doc);
        p->setName(name);
        p->setValue(pan, tilt);
        m_doc->addPalette(p);
        return p->id();
    };

    const QList<quint32> a { fixture(spot, "Standard"), fixture(spot, "Standard"),
                             fixture(par, "RGB Dimmer"), fixture(par, "RGB Dimmer") };
    const QList<quint32> b { fixture(spot, "Standard"), fixture(spot, "Standard"),
                             fixture(par, "RGB Dimmer"), fixture(par, "RGB Dimmer") };
    m_a = group("A", a);
    m_b = group("B", b);
    m_all = group("All", a + b);
    m_red = colour("Red", Qt::red);
    m_blue = colour("Blue", Qt::blue);
    m_green = colour("Green", Qt::green);
    m_white = colour("White", Qt::white);
    m_full = dimmer("Full", 255);
    m_half = dimmer("Half", 128);
    m_front = position("Front", 90, 45);
    m_side = position("Side", 200, 120);
    return true;
}

Chaser *TrackBack_Test::chaserOf(quint32 listId) const
{
    const CueList *list = m_doc->showTree()->cueList(listId);
    return list == nullptr ? nullptr : qobject_cast<Chaser *>(m_doc->function(list->chaser));
}

bool TrackBack_Test::set(quint32 cue, quint32 group, quint32 palette)
{
    ShowTreeController *st = m_doc->showTree();
    return st->setTargetLook(cue, groupTarget(group), st->lookForPalette(palette));
}

Chaser *TrackBack_Test::colourList()
{
    if (!rig())
        return nullptr;
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("Colour"), "Colour list");
    m_list = listId;
    auto cue = [&](int after, const QString &name) { return st->addCue(listId, after, name); };

    // 1: A red. 2: B blue (A tracks). 3: A green (B tracks). 4: Block, A
    // only at half: its colour is whatever the stage has. 5: tracks; B in
    // red. 6: A blue.
    const quint32 c1 = cue(-1, "A red");
    set(c1, m_a, m_red);
    set(c1, m_a, m_full);
    const quint32 c2 = cue(0, "B blue");
    set(c2, m_b, m_blue);
    set(c2, m_b, m_full);
    const quint32 c3 = cue(1, "A green");
    set(c3, m_a, m_green);
    const quint32 c4 = cue(2, "Block");
    st->setCueMode(c4, CueMode::Block);
    set(c4, m_a, m_half);
    const quint32 c5 = cue(3, "Tracks");
    set(c5, m_b, m_red);
    set(c5, m_b, m_half);
    const quint32 c6 = cue(4, "A blue");
    set(c6, m_a, m_blue);
    return chaserOf(listId);
}

Chaser *TrackBack_Test::everyModeList()
{
    if (!rig())
        return nullptr;
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("Modes"), "Modes list");
    m_list = listId;
    auto cue = [&](int after, const QString &name) { return st->addCue(listId, after, name); };

    // 1: A red front, B blue side.
    const quint32 c1 = cue(-1, "Open");
    set(c1, m_a, m_red);
    set(c1, m_a, m_full);
    set(c1, m_a, m_front);
    set(c1, m_b, m_blue);
    set(c1, m_b, m_full);
    set(c1, m_b, m_side);
    // 2: Cue-only: A green side; cue 3 starts from cue 1's A again.
    const quint32 c2 = cue(0, "Cue-only");
    st->setCueMode(c2, CueMode::CueOnly);
    set(c2, m_a, m_green);
    set(c2, m_a, m_side);
    // 3: B white, released when left.
    const quint32 c3 = cue(1, "Release");
    set(c3, m_b, m_white);
    st->setTargetRelease(c3, groupTarget(m_b), true);
    // 4: B only at half (no colour of its own: what the stage has).
    const quint32 c4 = cue(2, "B half");
    set(c4, m_b, m_half);
    // 5: All red; includes cue 2 (A green side) under it.
    const quint32 c5 = cue(3, "Include");
    st->addInclude(c5, c2);
    set(c5, m_all, m_red);
    set(c5, m_all, m_half);
    // 6: mixes: A blue over All red (tracked).
    const quint32 c6 = cue(4, "Mix");
    st->setCueMixOverlap(c6, true);
    set(c6, m_a, m_blue);
    // 7: Block: A blue, B half.
    const quint32 c7 = cue(5, "Block");
    st->setCueMode(c7, CueMode::Block);
    st->setCueMixOverlap(c7, true);
    set(c7, m_a, m_blue);
    set(c7, m_a, m_full);
    set(c7, m_b, m_half);
    // 8: A front only.
    const quint32 c8 = cue(6, "Front");
    set(c8, m_a, m_front);
    return chaserOf(listId);
}

/****************************************************************************
 * Tests
 ****************************************************************************/

void TrackBack_Test::colourTracksTheSameWhicheverWayYouArrive()
{
    Chaser *c = colourList();
    QVERIFY(c != nullptr);
    QCOMPARE(c->stepsCount(), 6);
    QString why;
    const int differ = everyWay(c, 40, &why);
    QVERIFY2(differ == 0, qPrintable(QStringLiteral("%1 in all; first: %2").arg(differ).arg(why)));
}

/* The report: forward it was right; Back, then GO, the colours were off.
   Cue 4 is a Block cue lighting A at half with no colour of its own, so A
   shows the colour the cues before left it: green (cue 3). Played from cue
   6 (A blue) Back to cue 4, A stayed blue, and GO to cue 5 kept it blue. */
void TrackBack_Test::backThenGoKeepsTheColourTheCuesBeforeSet()
{
    Chaser *c = colourList();
    QVERIFY(c != nullptr);
    const QMap<int, QByteArray> ref = straightRun(c);
    QCOMPARE(ref.count(), 6);
    // A's first Sharpy: its colour wheel is channel 1.
    const int wheel = 0;
    const uchar green = uchar(ref[2].at(wheel)), blue = uchar(ref[5].at(wheel));
    QVERIFY(green != blue);
    QCOMPARE(uchar(ref[3].at(wheel)), green);     // cue 4: what cue 3 left
    QCOMPARE(uchar(ref[4].at(wheel)), green);     // cue 5: tracks it on

    stopAll();
    for (int i = 0; i < 6; ++i)
    {
        act(c, { Go, 0 });
        land(c);
    }
    QCOMPARE(c->currentStepIndex(), 5);
    QCOMPARE(uchar(settle(1).at(wheel)), blue);
    act(c, { Back, 0 });
    land(c);
    act(c, { Back, 0 });
    QByteArray out = land(c);
    QCOMPARE(c->currentStepIndex(), 3);
    QCOMPARE(uchar(out.at(wheel)), green);
    QCOMPARE(out, ref[3]);
    act(c, { Go, 0 });
    out = land(c);
    QCOMPARE(c->currentStepIndex(), 4);
    QCOMPARE(uchar(out.at(wheel)), green);
    QCOMPARE(out, ref[4]);
    // A jump from cue 6 straight to cue 5: the same.
    act(c, { Jump, 5 });
    land(c);
    act(c, { Jump, 4 });
    out = land(c);
    QCOMPARE(out, ref[4]);
}

/* Fades, a hold that follows on, a Loop list: the same at every cue. Back
   plays the cue's own fade; what it doesn't set goes back under the leaving
   cue's fade-out. */
void TrackBack_Test::fadesHoldsAndLoop()
{
    Chaser *c = colourList();
    QVERIFY(c != nullptr);
    ShowTreeController *st = m_doc->showTree();
    const CueList *list = st->cueList(m_list);
    QVERIFY(list != nullptr);
    QList<Cue> cues = list->cues;
    cues[1].fadeIn = 400;
    cues[1].fadeOut = 400;
    cues[2].hold = 300;           // cue 3 follows on into cue 4
    cues[4].fadeIn = 600;
    cues[5].fadeOut = 200;
    for (const Cue &cue : cues)
        QVERIFY(st->setCue(cue));
    c = chaserOf(m_list);
    QVERIFY(c != nullptr);
    QCOMPARE(c->steps().at(2).hold, 300u);

    QString why;
    int differ = everyWay(c, 20, &why);
    QVERIFY2(differ == 0, qPrintable(QStringLiteral("Single Shot: %1 in all; first: %2").arg(differ).arg(why)));

    c->setRunOrder(Function::Loop);
    why.clear();
    differ = everyWay(c, 20, &why);
    QVERIFY2(differ == 0, qPrintable(QStringLiteral("Loop: %1 in all; first: %2").arg(differ).arg(why)));
}

void TrackBack_Test::blockCueOnlyReleaseIncludesAndMix()
{
    Chaser *c = everyModeList();
    QVERIFY(c != nullptr);
    QCOMPARE(c->stepsCount(), 8);
    QString why;
    const int differ = everyWay(c, 60, &why);
    QVERIFY2(differ == 0, qPrintable(QStringLiteral("%1 in all; first: %2").arg(differ).arg(why)));
}

void TrackBack_Test::skippedCuesPlayAsIfDeleted()
{
    for (int which = 0; which < 2; ++which)
    {
        if (which > 0)
        {
            cleanup();
            init();
        }
        Chaser *c = which == 0 ? colourList() : everyModeList();
        QVERIFY(c != nullptr);
        ShowTreeController *st = m_doc->showTree();
        const QList<Cue> cues = st->cueList(m_list)->cues;
        const int n = int(cues.count());
        const QMap<int, QByteArray> plain = straightRun(c);

        // Each cue alone, and each two in a row -- not one another cue
        // includes (deleting it would take the include away too).
        QList<QList<int>> sets;
        for (int k = 0; k < n; ++k)
            sets << QList<int>({ k });
        for (int k = 0; k + 1 < n; ++k)
            sets << QList<int>({ k, k + 1 });
        for (const QList<int> &skip : sets)
        {
            bool included = false;
            QList<quint32> ids;
            for (int k : skip)
            {
                ids << cues.at(k).id;
                for (const Cue &other : cues)
                    included = included || other.includes.contains(cues.at(k).id);
            }
            if (included)
                continue;
            const QString what = QStringLiteral("%1 list, cues %2 skipped").arg(which == 0 ? "colour" : "every-mode")
                                 .arg(QString::number(skip.first() + 1) + (skip.count() > 1 ? "+" : ""));
            QVERIFY(st->setCuesSkipped(ids, true));
            c = chaserOf(m_list);
            const QMap<int, QByteArray> skipped = straightRun(c);
            for (int k : skip)
                QVERIFY2(!skipped.contains(k), qPrintable(what));

            // A jump straight to the (first) skipped cue from a dark stage
            // plays it, as the cues before it left the stage: what the
            // list played there with nothing skipped.
            if (plain.contains(skip.first()))
            {
                stopAll();
                act(c, { Jump, skip.first() });
                const QByteArray out = land(c);
                QCOMPARE(c->currentStepIndex(), skip.first());
                int differ = 0;
                for (int i = 0; i < qMin(out.size(), plain[skip.first()].size()); ++i)
                    differ += out.at(i) != plain[skip.first()].at(i) ? 1 : 0;
                QVERIFY2(differ == 0, qPrintable(QStringLiteral("%1: jump: %2 channels differ").arg(what).arg(differ)));
            }

            // GO, Back and jumps -- into the skipped cues too -- land on
            // every cue as GO from the top does.
            if (skip.count() == 1)
            {
                QString why;
                const int differ = everyWay(c, 6, &why);
                QVERIFY2(differ == 0, qPrintable(QStringLiteral("%1: %2 in all; first: %3").arg(what).arg(differ).arg(why)));
            }

            // Deleted instead: the same, channel for channel.
            for (quint32 id : ids)
                QVERIFY(st->removeCue(id));
            c = chaserOf(m_list);
            const QMap<int, QByteArray> deleted = straightRun(c);
            int d = 0, differ = 0;
            for (int k = 0; k < n; ++k)
            {
                if (skip.contains(k))
                    continue;
                QVERIFY2(skipped.contains(k) == deleted.contains(d), qPrintable(what));
                if (skipped.contains(k))
                    for (int i = 0; i < qMin(skipped[k].size(), deleted[d].size()); ++i)
                        differ += skipped[k].at(i) != deleted[d].at(i) ? 1 : 0;
                ++d;
            }
            QVERIFY2(differ == 0, qPrintable(QStringLiteral("%1: %2 channels differ from the list without them")
                                             .arg(what).arg(differ)));
            for (int u = 0; u < ids.count() + 1; ++u)
                QVERIFY(st->undo());
            QCOMPARE(st->cueList(m_list)->cues.count(), n);
            for (const Cue &cue : st->cueList(m_list)->cues)
                QVERIFY(!cue.skip);
            c = chaserOf(m_list);
        }
    }
}

Chaser *TrackBack_Test::houseAndCurtainList(bool showOnly, bool operate, bool unpatched)
{
    if (!rig())
        return nullptr;
    QLCFixtureDef *curtainDef = m_doc->fixtureDefCache()->fixtureDef("Generic", "Stage Curtain");
    if (curtainDef == nullptr || curtainDef->mode("Open + Fly") == nullptr)
        return nullptr;
    quint32 address = 0;
    for (Fixture *f : m_doc->fixtures())
        address = qMax(address, f->address() + f->channels());
    auto group = [&](const QString &name, const QList<quint32> &fixtures) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        m_doc->addFixtureGroup(g);
        for (quint32 f : fixtures)
            g->assignFixture(f);
        return g->id();
    };
    auto dimmer = [&](const QString &name) {
        Fixture *f = new Fixture(m_doc);
        f->setName(name);
        f->setChannels(1);
        f->setAddress(address);
        address += 1;
        m_doc->addFixture(f);
        return f;
    };

    // The main drape's curtain fixture (Open, Fly: LTP).
    Fixture *drape = new Fixture(m_doc);
    drape->setName("Main drape curtain");
    drape->setFixtureDefinition(curtainDef, curtainDef->mode("Open + Fly"));
    drape->setAddress(address);
    address += drape->channels();
    m_doc->addFixture(drape);
    // A traveller on a relay: the Curtain plot symbol, its intensity opens it.
    Fixture *traveller = dimmer("Traveller relay");
    traveller->setStageLook(Fixture::CurtainLook);
    // The house: two dimmers and a relay that switches.
    Fixture *house1 = dimmer("House 1");
    Fixture *house2 = dimmer("House 2");
    Fixture *houseRelay = dimmer("House relay");
    houseRelay->setOutputRules(Fixture::Switch);
    const quint32 house = group("House Lights", { house1->id(), house2->id(), houseRelay->id() });
    const quint32 travellers = group("Traveller", { traveller->id() });
    if (showOnly)
    {
        drape->setOutputRules(Fixture::ShowOnly);
        traveller->setOutputRules(Fixture::ShowOnly);
        houseRelay->setOutputRules(Fixture::Switch | Fixture::ShowOnly);
    }
    // No motor yet: the curtains only drawn (an unpatched fixture shows
    // what plays on it, Fixture::setPreviewValue()). Before Operate, which
    // refuses a patch change.
    if (unpatched && (!m_doc->unpatchFixture(drape->id()) || !m_doc->unpatchFixture(traveller->id())))
        return nullptr;
    if (operate)
        m_doc->setMode(Doc::Operate);

    Scene *open = new Scene(m_doc);
    open->setName("Curtain Open");
    open->setValue(drape->id(), 0, 255);
    open->setValue(drape->id(), 1, 0);
    m_doc->addFunction(open);
    QLCPalette *out = new QLCPalette(QLCPalette::Dimmer, m_doc);
    out->setName("Out");
    out->setValue(0);
    m_doc->addPalette(out);

    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("House"), "House list");
    m_list = listId;
    auto cue = [&](int after, const QString &name) { return st->addCue(listId, after, name); };
    // 1: House to half (the relay on: half is over its switch point).
    const quint32 c1 = cue(-1, "House to half");
    set(c1, house, m_half);
    // 2: House out; the curtains open: the drape by a layer playing the
    // Curtain Open scene, the traveller at full.
    const quint32 c2 = cue(0, "House out, curtains open");
    set(c2, house, out->id());
    Target drapeLayer;
    drapeLayer.function = open->id();
    if (st->addLayer(c2, drapeLayer) == ShowTree::invalidId())
        return nullptr;
    set(c2, travellers, m_full);
    // 3, 4: the stage; nothing touches the house or the curtains.
    const quint32 c3 = cue(1, "A red");
    set(c3, m_a, m_red);
    set(c3, m_a, m_full);
    const quint32 c4 = cue(2, "B blue");
    set(c4, m_b, m_blue);
    set(c4, m_b, m_full);
    return chaserOf(listId);
}

void TrackBack_Test::curtainRows()
{
    QTest::addColumn<bool>("showOnly");
    QTest::addColumn<bool>("operate");
    QTest::addColumn<bool>("unpatched");
    QTest::newRow("Design") << false << false << false;
    QTest::newRow("Operate") << false << true << false;
    QTest::newRow("Operate, Show only") << true << true << false;
    QTest::newRow("Design, unpatched curtains") << false << false << true;
    QTest::newRow("Operate, unpatched curtains") << false << true << true;
}

void TrackBack_Test::curtainsHouseAndRelaysTrackOnAJump_data()
{
    curtainRows();
}

void TrackBack_Test::curtainsHouseAndRelaysTrackOnAJump()
{
    QFETCH(bool, showOnly);
    QFETCH(bool, operate);
    QFETCH(bool, unpatched);
    Chaser *c = houseAndCurtainList(showOnly, operate, unpatched);
    QVERIFY(c != nullptr);
    QCOMPARE(c->stepsCount(), 4);
    const QMap<int, QByteArray> ref = straightRun(c);
    QCOMPARE(ref.count(), 4);

    const Fixture *drape = nullptr, *traveller = nullptr, *house1 = nullptr, *relay = nullptr;
    for (const Fixture *f : m_doc->fixtures())
    {
        if (f->name() == "Main drape curtain")
            drape = f;
        else if (f->name() == "Traveller relay")
            traveller = f;
        else if (f->name() == "House 1")
            house1 = f;
        else if (f->name() == "House relay")
            relay = f;
    }
    QVERIFY(drape && traveller && house1 && relay);
    // A patched fixture's output; an unpatched one's what plays on it.
    auto at = [](const QByteArray &out, const Fixture *f, int ch = 0) {
        if (!f->isPatched())
            return int(uchar(const_cast<Fixture *>(f)->channelValues().at(ch)));
        return int(uchar(out.at(int(f->universeAddress()) + ch)));
    };
    QStringList bad;
    auto want = [&](const QString &where, const Fixture *f, int got, bool on) {
        // The run from the top keeps the universes only: an unpatched
        // fixture's drawing is checked where it is read live.
        if (!f->isPatched() && where.startsWith(QLatin1String("top")))
            return;
        if ((got > 0) != on)
            bad << QStringLiteral("%1: %2 at %3").arg(where, f->name()).arg(got);
    };
    // From the top: the house at half (the relay on), then out with the
    // curtains open, and they stay open through cues 3 and 4.
    want("top 1", house1, at(ref[0], house1), true);
    want("top 1", relay, at(ref[0], relay), true);
    want("top 1", drape, at(ref[0], drape), false);
    want("top 1", traveller, at(ref[0], traveller), false);
    for (int k = 1; k < 4; ++k)
    {
        const QString where = QStringLiteral("top %1").arg(k + 1);
        want(where, house1, at(ref[k], house1), false);
        want(where, relay, at(ref[k], relay), false);
        want(where, drape, at(ref[k], drape), true);
        want(where, traveller, at(ref[k], traveller), true);
    }

    // The report: load cue 3 (or 4) from a dark stage, or from cue 1:
    // the curtains open, the house out.
    for (int from : { -1, 0 })
        for (int target : { 2, 3 })
        {
            stopAll();
            if (from >= 0)
            {
                act(c, { Jump, from });
                land(c);
            }
            act(c, { Jump, target });
            const QByteArray o = land(c);
            const QString where = QStringLiteral("jump %1 -> %2").arg(from + 1).arg(target + 1);
            want(where, drape, at(o, drape), true);
            want(where, traveller, at(o, traveller), true);
            want(where, house1, at(o, house1), false);
            want(where, relay, at(o, relay), false);
        }
    // Back to the pre-show from the stage: the house back at half, the
    // relay on again, the curtains closed.
    stopAll();
    act(c, { Jump, 3 });
    land(c);
    act(c, { Jump, 0 });
    QByteArray o = land(c);
    want("jump 4 -> 1", house1, at(o, house1), true);
    want("jump 4 -> 1", relay, at(o, relay), true);
    want("jump 4 -> 1", drape, at(o, drape), false);
    want("jump 4 -> 1", traveller, at(o, traveller), false);
    // Back from 4 to 3: still open.
    stopAll();
    for (int i = 0; i < 4; ++i)
    {
        act(c, { Go, 0 });
        land(c);
    }
    act(c, { Back, 0 });
    o = land(c);
    want("back 4 -> 3", drape, at(o, drape), true);
    want("back 4 -> 3", traveller, at(o, traveller), true);
    QVERIFY2(bad.isEmpty(), qPrintable(bad.join(QStringLiteral("; "))));

    QString why;
    const int differ = everyWay(c, 20, &why);
    QVERIFY2(differ == 0, qPrintable(QStringLiteral("%1 in all; first: %2").arg(differ).arg(why)));
}

void TrackBack_Test::aJumpDuringTheOpeningFadeEndsOpen_data()
{
    curtainRows();
}

void TrackBack_Test::aJumpDuringTheOpeningFadeEndsOpen()
{
    QFETCH(bool, showOnly);
    QFETCH(bool, operate);
    QFETCH(bool, unpatched);
    Chaser *c = houseAndCurtainList(showOnly, operate, unpatched);
    QVERIFY(c != nullptr);
    ShowTreeController *st = m_doc->showTree();
    // Cue 2 opens the curtains over 2 s.
    Cue opening = st->cueList(m_list)->cues.at(1);
    opening.fadeIn = 2000;
    QVERIFY(st->setCue(opening));
    c = chaserOf(m_list);
    QVERIFY(c != nullptr);
    const QMap<int, QByteArray> ref = straightRun(c);
    QCOMPARE(ref.count(), 4);

    Fixture *drape = nullptr, *traveller = nullptr;
    for (Fixture *f : m_doc->fixtures())
    {
        if (f->name() == "Main drape curtain")
            drape = f;
        else if (f->name() == "Traveller relay")
            traveller = f;
    }
    QVERIFY(drape && traveller);
    // What the plot draws: the fixture's values from the output, as the
    // App hands them over, read by Scenery.
    auto pose = [](Fixture *f, const QByteArray &out) {
        if (f->isPatched())
            f->setChannelValues(out);
        return Scenery::poseFromValues(f->fixtureMode(), f->channelValues()).open;
    };
    const int fadeTicks = int(2000 / MasterTimer::tick());

    QStringList bad;
    // GO GO (the opening starts), part way through: jump to 3 / 4, or GO
    // and Back; and from cue 1 straight to cue 2 by a jump, then on.
    struct Way { QString name; bool goToOpening; QList<Move> then; int lands; };
    const QList<Way> ways = {
        { "GO to 2, jump to 4", true, { { Jump, 3 } }, 3 },
        { "GO to 2, jump to 3", true, { { Jump, 2 } }, 2 },
        { "jump to 2, jump to 4", false, { { Jump, 3 } }, 3 },
        { "GO to 2, GO, Back", true, { { Go, 0 }, { Back, 0 } }, 1 },
    };
    for (const Way &w : ways)
    {
        stopAll();
        act(c, { Go, 0 });
        land(c);
        act(c, w.goToOpening ? Move { Go, 0 } : Move { Jump, 1 });
        tick(fadeTicks / 3);
        const QByteArray mid = settle(0);
        const int midOpen = traveller->isPatched() ? int(uchar(mid.at(int(traveller->universeAddress()))))
                                                   : int(uchar(traveller->channelValues().at(0)));
        if (midOpen <= 0 || midOpen >= 255)
            bad << QStringLiteral("%1: not mid-fade (%2)").arg(w.name).arg(midOpen);
        for (const Move &m : w.then)
        {
            act(c, m);
            tick(2);
        }
        // Past the opening's own time: whatever it was doing is over.
        const QByteArray out = settle(fadeTicks + 10);
        if (c->currentStepIndex() != w.lands)
            bad << QStringLiteral("%1: at cue %2").arg(w.name).arg(c->currentStepIndex() + 1);
        for (Fixture *f : { drape, traveller })
        {
            const int v = f->isPatched() ? int(uchar(out.at(int(f->universeAddress()))))
                                         : int(uchar(f->channelValues().at(0)));
            const double open = pose(f, out);
            if (v != 255 || open < 0.999)
                bad << QStringLiteral("%1: %2 at %3 (drawn %4 open)").arg(w.name, f->name()).arg(v).arg(open);
        }
        if (out != ref[w.lands])
            bad << QStringLiteral("%1: the stage is not cue %2's").arg(w.name).arg(w.lands + 1);
    }
    QVERIFY2(bad.isEmpty(), qPrintable(bad.join(QStringLiteral("; "))));
}

Fixture *TrackBack_Test::fixtureNamed(const QString &name) const
{
    for (Fixture *f : m_doc->fixtures())
        if (f->name() == name)
            return f;
    return nullptr;
}

Chaser *TrackBack_Test::movingCurtainList(bool showOnly, int fadeMs)
{
    if (houseAndCurtainList(showOnly, true) == nullptr)
        return nullptr;
    Fixture *drape = fixtureNamed("Main drape curtain");
    Fixture *traveller = fixtureNamed("Traveller relay");
    if (drape == nullptr || traveller == nullptr)
        return nullptr;
    quint32 address = 0;
    for (Fixture *f : m_doc->fixtures())
        address = qMax(address, f->address() + f->channels());
    Fixture *flame = new Fixture(m_doc);
    flame->setName("Flame relay");
    flame->setChannels(1);
    flame->setAddress(address);
    flame->setOutputRules(Fixture::Switch | Fixture::ShowOnly);
    m_doc->addFixture(flame);
    FixtureGroup *flames = new FixtureGroup(m_doc);
    flames->setName("Flames");
    m_doc->addFixtureGroup(flames);
    flames->assignFixture(flame->id());
    quint32 house = 0, travellers = 0;
    for (FixtureGroup *g : m_doc->fixtureGroups())
    {
        if (g->name() == "House Lights")
            house = g->id();
        else if (g->name() == "Traveller")
            travellers = g->id();
    }
    quint32 out = 0;
    for (QLCPalette *p : m_doc->palettes())
        if (p->name() == "Out")
            out = p->id();

    auto scene = [&](const QString &name, quint32 channel, uchar value) {
        Scene *s = new Scene(m_doc);
        s->setName(name);
        s->setValue(drape->id(), channel, value);
        m_doc->addFunction(s);
        return s->id();
    };
    ShowTreeController *st = m_doc->showTree();
    Target close;
    close.function = scene("Curtain Close", 0, 0);
    Target fly;
    fly.function = scene("Fly Out", 1, 255);
    const quint32 c5 = st->addCue(m_list, 3, "Curtains close");
    st->setCueMode(c5, CueMode::Block);
    if (st->addLayer(c5, close) == ShowTree::invalidId())
        return nullptr;
    set(c5, travellers, out);
    set(c5, house, m_half);
    set(c5, flames->id(), m_full);
    const quint32 c6 = st->addCue(m_list, 4, "Fly out");
    if (st->addLayer(c6, fly) == ShowTree::invalidId())
        return nullptr;
    QList<Cue> cues = st->cueList(m_list)->cues;
    for (int i : { 1, 4, 5 })
    {
        cues[i].fadeIn = uint(fadeMs);
        if (!st->setCue(cues.at(i)))
            return nullptr;
    }
    // Cue 4's traveller goes out under its own fade out into cue 5.
    cues = st->cueList(m_list)->cues;
    cues[3].fadeOut = uint(fadeMs);
    if (!st->setCue(cues.at(3)))
        return nullptr;
    // The rig as it is (the keeper rebuilds on the event loop).
    m_doc->outputRuleKeeper()->rebuild();
    return chaserOf(m_list);
}

void TrackBack_Test::stopAllFromControl(int fadeMs)
{
    /* Stop all waits for the timer thread to stop the functions: here it
       runs beside this thread, which plays the timer. */
    MasterTimer *mt = m_doc->masterTimer();
    Doc *doc = m_doc;
    std::future<void> done = std::async(std::launch::async, [doc, fadeMs]() {
        FixtureRest::stopAll(doc, fadeMs);
    });
    QElapsedTimer guard;
    guard.start();
    while (!mt->m_stopAllFunctions && done.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready
           && guard.elapsed() < 10000)
        QThread::msleep(1);
    while (done.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready && guard.elapsed() < 20000)
        tick(1);
    QVERIFY(done.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
}

void TrackBack_Test::aStopLeavesACurtainWhereItIs_data()
{
    QTest::addColumn<int>("how");       // 0 emergency stop, 1 Stop ALL, 2 Fade and stop
    QTest::addColumn<bool>("showOnly");
    QTest::newRow("emergency stop") << 0 << false;
    QTest::newRow("Stop ALL") << 1 << false;
    QTest::newRow("Fade and stop") << 2 << false;
    QTest::newRow("emergency stop, Show only") << 0 << true;
    QTest::newRow("Stop ALL, Show only") << 1 << true;
}

void TrackBack_Test::aStopLeavesACurtainWhereItIs()
{
    QFETCH(int, how);
    QFETCH(bool, showOnly);
    const int fadeMs = 2000;
    Chaser *c = movingCurtainList(showOnly, fadeMs);
    QVERIFY(c != nullptr);
    QCOMPARE(c->stepsCount(), 6);
    Fixture *drape = fixtureNamed("Main drape curtain");
    Fixture *traveller = fixtureNamed("Traveller relay");
    Fixture *house1 = fixtureNamed("House 1");
    Fixture *flame = fixtureNamed("Flame relay");
    QVERIFY(drape && traveller && house1 && flame);
    ShowControl *sc = m_doc->showControl();
    OutputRuleKeeper *rules = m_doc->outputRuleKeeper();
    const int fadeTicks = int(uint(fadeMs) / MasterTimer::tick());
    auto value = [](const QByteArray &out, const Fixture *f, int ch) {
        return int(uchar(out.at(int(f->universeAddress()) + ch)));
    };

    struct Motion { const char *name; int cue; QList<QPair<Fixture *, int> > moving; int target; };
    const QList<Motion> motions = {
        { "opening", 1, { { drape, 0 }, { traveller, 0 } }, 255 },
        { "closing", 4, { { drape, 0 }, { traveller, 0 } }, 0 },
        { "flying out", 5, { { drape, 1 } }, 255 },
    };
    QStringList bad;
    for (const Motion &m : motions)
    {
        stopAll();
        rules->releaseStageMachinery();
        m_settle = 8;
        for (int i = 0; i < m.cue; ++i)
        {
            act(c, { Go, 0 });
            settle(fadeTicks + 10);
        }
        act(c, { Go, 0 });
        tick(fadeTicks / 3);
        QCOMPARE(c->currentStepIndex(), m.cue);
        const QByteArray before = settle(0);
        QList<int> at;
        for (const QPair<Fixture *, int> &fc : m.moving)
        {
            at << value(before, fc.first, fc.second);
            if (at.last() <= 0 || at.last() >= 255)
                bad << QStringLiteral("%1: %2 not moving (%3)").arg(m.name, fc.first->name()).arg(at.last());
        }

        if (how == 0)
        {
            ShowControl::Request off;
            off.command = ShowControl::AllOff;
            sc->execute(off);
        }
        else
        {
            stopAllFromControl(how == 2 ? 1000 : 0);
        }
        if (!rules->stageMachineryHeld())
            bad << QStringLiteral("%1: not held").arg(m.name);

        // Every frame of the stop and its fade, and well after: still.
        auto still = [&](const QString &when) {
            const QByteArray o = settle(0);
            for (int k = 0; k < m.moving.count(); ++k)
            {
                const int v = value(o, m.moving.at(k).first, m.moving.at(k).second);
                if (v != at.at(k))
                {
                    bad << QStringLiteral("%1, %2: %3 moved %4 -> %5").arg(m.name, when, m.moving.at(k).first->name())
                           .arg(at.at(k)).arg(v);
                    return false;
                }
            }
            return true;
        };
        for (int i = 0; i < fadeTicks + 60; ++i)
        {
            tick(1);
            if (!still(QStringLiteral("frame %1 of the stop").arg(i)))
                break;
        }
        // Putting the rig at rest after the fades (a timer), a look
        // started and let go of, a list told to stop: nothing moves it.
        QTest::qWait(300);
        tick(4);
        still(QStringLiteral("at rest"));
        Scene *other = new Scene(m_doc);
        other->setValue(drape->id(), 0, 0);
        other->setValue(drape->id(), 1, 0);
        other->setValue(traveller->id(), 0, 0);
        m_doc->addFunction(other);
        other->start(m_doc->masterTimer(), FunctionParent::master());
        tick(5);
        still(QStringLiteral("a look on it"));
        other->stop(FunctionParent::master());
        tick(5);
        ShowControl::Request stop;
        stop.command = ShowControl::Stop;
        stop.chaser = c->id();
        sc->execute(stop);
        QTest::qWait(300);
        tick(5);
        still(QStringLiteral("a look let go of"));
        m_doc->deleteFunction(other->id());
        /* The lights are out, the flame off; the house (a Switch, board
           Q350/Q351) holds what it had when the stop came. */
        const QByteArray dark = settle(60);
        if (value(dark, house1, 0) != value(before, house1, 0))
            bad << QStringLiteral("%1: the house at %2, was %3").arg(m.name).arg(value(dark, house1, 0))
                   .arg(value(before, house1, 0));
        if (value(dark, flame, 0) != 0)
            bad << QStringLiteral("%1: the flame at %2").arg(m.name).arg(value(dark, flame, 0));

        // GO (the moving cue loaded): it carries on to where it goes.
        const CueList *list = m_doc->showTree()->cueList(m_list);
        ShowControl::Request load;
        load.command = ShowControl::Load;
        load.chaser = c->id();
        load.cue = m_doc->showTree()->cueNumber(*list, m.cue);
        sc->execute(load);
        ShowControl::Request go;
        go.command = ShowControl::Go;
        go.chaser = c->id();
        sc->execute(go);
        tick(2);
        QTest::qWait(int(4 * MasterTimer::tick()) + 100);
        if (rules->stageMachineryHeld())
            bad << QStringLiteral("%1: still held after the GO").arg(m.name);
        const QByteArray after = settle(fadeTicks + 10);
        for (const QPair<Fixture *, int> &fc : m.moving)
            if (value(after, fc.first, fc.second) != m.target)
                bad << QStringLiteral("%1: after the GO %2 at %3").arg(m.name, fc.first->name())
                       .arg(value(after, fc.first, fc.second));
    }
    QVERIFY2(bad.isEmpty(), qPrintable(bad.join(QStringLiteral("; "))));
}

void TrackBack_Test::theFooterSaysWhereTheCurtainStopped()
{
    const int fadeMs = 2000;
    Chaser *c = movingCurtainList(false, fadeMs);
    QVERIFY(c != nullptr);
    // The drape alone: the traveller gone.
    Fixture *traveller = fixtureNamed("Traveller relay");
    QVERIFY(traveller != nullptr);
    QVERIFY(m_doc->deleteFixture(traveller->id()));
    m_doc->outputRuleKeeper()->rebuild();
    c = chaserOf(m_list);
    QVERIFY(c != nullptr);
    OutputRuleKeeper *rules = m_doc->outputRuleKeeper();
    QCOMPARE(rules->stageMachineryHeldText(), QString());
    stopAll();
    act(c, { Go, 0 });
    settle(8);
    act(c, { Go, 0 });
    // 40% of the way: the drape's Open at 102 of 255.
    const int ticks = int(uint(fadeMs) / MasterTimer::tick());
    tick(int(ticks * 0.4) + 1);
    Fixture *drape = fixtureNamed("Main drape curtain");
    const int open = int(uchar(settle(0).at(int(drape->universeAddress()))));
    QVERIFY2(open > 90 && open < 115, qPrintable(QString::number(open)));
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    m_doc->showControl()->execute(off);
    // The house (two dimmers and a relay: Switch) is held too.
    QCOMPARE(rules->stageMachineryHeldText(),
             QStringLiteral("Curtain stopped at %1% · 3 more held — GO to carry on").arg(qRound(open * 100.0 / 255.0)));
    QVERIFY(rules->stageMachineryHeldText().startsWith(QStringLiteral("Curtain stopped at 4")));
    // The next GO: gone.
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = c->id();
    m_doc->showControl()->execute(go);
    tick(2);
    QTest::qWait(int(4 * MasterTimer::tick()) + 100);
    QCOMPARE(rules->stageMachineryHeldText(), QString());
}

/****************************************************************************
 * Behaves as (board Q350, Q351)
 ****************************************************************************/

Chaser *TrackBack_Test::behavesList(int fadeMs)
{
    quint32 address = 0;
    auto dimmer = [&](const QString &name) {
        Fixture *f = new Fixture(m_doc);
        f->setName(name);
        f->setChannels(1);
        f->setAddress(address++);
        m_doc->addFixture(f);
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        m_doc->addFixtureGroup(g);
        g->assignFixture(f->id());
        return g->id();
    };
    auto level = [&](const QString &name, int v) {
        QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
        p->setName(name);
        p->setValue(v);
        m_doc->addPalette(p);
        return p->id();
    };
    const quint32 wash = dimmer("Stage wash");
    const quint32 relay = dimmer("House relay");
    m_doc->fixture(m_doc->fixtureGroup(relay)->fixtureList().first())->setOutputRules(Fixture::Switch);
    const quint32 drop = dimmer("Kabuki drop");
    const quint32 jet = dimmer("CO2 jet");
    const quint32 full = level("Full", 255), half = level("Half", 128), out = level("Out", 0);

    ShowTreeController *st = m_doc->showTree();
    m_list = st->addCueList(st->addProduction("Behaves"), "Behaves list");
    m_c1 = st->addCue(m_list, -1, "Preset");
    set(m_c1, wash, full);
    set(m_c1, relay, out);
    set(m_c1, drop, out);
    set(m_c1, jet, out);
    m_c2 = st->addCue(m_list, 0, "The drop");
    set(m_c2, wash, half);
    set(m_c2, relay, full);
    set(m_c2, drop, full);
    set(m_c2, jet, full);
    m_c3 = st->addCue(m_list, 1, "After");
    set(m_c3, wash, full);
    if (fadeMs > 0)
    {
        QList<Cue> cues = st->cueList(m_list)->cues;
        for (Cue &c : cues)
        {
            c.fadeIn = uint(fadeMs);
            st->setCue(c);
        }
    }
    m_doc->outputRuleKeeper()->rebuild();
    return chaserOf(m_list);
}

int TrackBack_Test::at(const char *fixture, int ch)
{
    const Fixture *f = fixtureNamed(QString::fromLatin1(fixture));
    if (f == nullptr)
        return -1;
    const QByteArray out = settle(0);
    const int a = int(f->universeAddress()) + ch;
    return a < out.size() ? int(uchar(out.at(a))) : -1;
}

void TrackBack_Test::switchesAndDropsHoldWhileEditing()
{
    QVERIFY(rig());
    Chaser *c = behavesList();
    QVERIFY(c != nullptr);
    QCOMPARE(m_doc->mode(), Doc::Design);
    ShowTreeController *st = m_doc->showTree();
    // Picked while editing: the wash previews, the relay and the drop stay
    // where they are (out), the jet never fires.
    st->pickCue(m_c2);
    settle(20);
    QCOMPARE(at("Stage wash"), 128);
    QCOMPARE(at("House relay"), 0);
    QCOMPARE(at("Kabuki drop"), 0);
    QCOMPARE(at("CO2 jet"), 0);
    st->pickCue(m_c3);
    settle(20);
    QCOMPARE(at("Stage wash"), 255);
    QCOMPARE(at("House relay"), 0);
    QCOMPARE(at("Kabuki drop"), 0);
    st->stopPreview();
    settle(10);
    // A run moves them: GO to the drop, from the Run, in Design too.
    act(c, { Go, 0 });
    settle(10);
    act(c, { Go, 0 });
    settle(20);
    QCOMPARE(c->currentStepIndex(), 1);
    QCOMPARE(at("House relay"), 255);
    QCOMPARE(at("Kabuki drop"), 255);
    // Picking the preset while it plays: they stay (dropped stays dropped).
    st->pickCue(m_c1);
    settle(20);
    QCOMPARE(at("Stage wash"), 255);
    QCOMPARE(at("House relay"), 255);
    QCOMPARE(at("Kabuki drop"), 255);
    st->stopPreview();
}

void TrackBack_Test::aBackNeverFiresAOneShotAgain()
{
    QVERIFY(rig());
    Chaser *c = behavesList();
    QVERIFY(c != nullptr);
    m_doc->setMode(Doc::Operate);
    for (int i = 0; i < 3; ++i)
    {
        act(c, { Go, 0 });
        settle(10);
    }
    QCOMPARE(c->currentStepIndex(), 2);
    QCOMPARE(at("Kabuki drop"), 255);
    act(c, { Back, 0 });            // to the drop: it is dropped
    settle(10);
    QCOMPARE(c->currentStepIndex(), 1);
    QCOMPARE(at("Kabuki drop"), 255);
    act(c, { Back, 0 });            // to before it: dropped stays dropped
    settle(10);
    QCOMPARE(c->currentStepIndex(), 0);
    QCOMPARE(at("Stage wash"), 255);
    QCOMPARE(at("House relay"), 0);  // a switch plays the cue (a Back is a run)
    QCOMPARE(at("Kabuki drop"), 255);
    // The next GO plays on: the drop's cue keeps it down, and it is the
    // cues' again.
    act(c, { Go, 0 });
    QTest::qWait(int(4 * MasterTimer::tick()) + 100);
    settle(10);
    QCOMPARE(c->currentStepIndex(), 1);
    QCOMPARE(at("Kabuki drop"), 255);
    m_doc->setMode(Doc::Design);
}

void TrackBack_Test::aHazardIsNeverFiredYet()
{
    QVERIFY(rig());
    Chaser *c = behavesList();
    QVERIFY(c != nullptr);
    // Design: a pick never fires it.
    m_doc->showTree()->pickCue(m_c2);
    settle(20);
    QCOMPARE(at("Stage wash"), 128);
    QCOMPARE(at("CO2 jet"), 0);
    m_doc->showTree()->stopPreview();
    // Operate: nor a GO, until it can be armed.
    m_doc->setMode(Doc::Operate);
    act(c, { Go, 0 });
    settle(10);
    act(c, { Go, 0 });
    settle(20);
    QCOMPARE(at("Stage wash"), 128);
    QCOMPARE(at("Kabuki drop"), 255);
    QCOMPARE(at("CO2 jet"), 0);
    // Parked, it parks off.
    Fixture *jet = fixtureNamed("CO2 jet");
    m_doc->programmer()->parkFixtures({ jet->id() });
    settle(5);
    QCOMPARE(at("CO2 jet"), 0);
    m_doc->programmer()->unparkFixtures({ jet->id() });
    settle(5);
    QCOMPARE(at("CO2 jet"), 0);
    m_doc->setMode(Doc::Design);
}

void TrackBack_Test::aStopHoldsSwitchesAndDrops()
{
    QVERIFY(rig());
    Chaser *c = behavesList();
    QVERIFY(c != nullptr);
    m_doc->setMode(Doc::Operate);
    act(c, { Go, 0 });
    settle(10);
    act(c, { Go, 0 });
    settle(20);
    QCOMPARE(at("House relay"), 255);
    QCOMPARE(at("Kabuki drop"), 255);
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    m_doc->showControl()->execute(off);
    QTest::qWait(300);
    settle(20);
    QCOMPARE(at("Stage wash"), 0);
    QCOMPARE(at("CO2 jet"), 0);
    QCOMPARE(at("House relay"), 255);
    QCOMPARE(at("Kabuki drop"), 255);
    m_doc->setMode(Doc::Design);
}

void TrackBack_Test::aGoAfterAStopMidGlideDropsTheGlide()
{
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef("Generic", "Stage Curtain");
    QVERIFY(def != nullptr && def->mode("Open + Fly") != nullptr);
    Fixture *curtain = new Fixture(m_doc);
    curtain->setName("Main drape curtain");
    curtain->setFixtureDefinition(def, def->mode("Open + Fly"));
    curtain->setAddress(0);
    QVERIFY(m_doc->addFixture(curtain));
    Scene *open = new Scene(m_doc);
    open->setName("Curtain Open");
    open->setValue(curtain->id(), 0, 255);
    QVERIFY(m_doc->addFunction(open));
    ShowTreeController *st = m_doc->showTree();
    m_list = st->addCueList(st->addProduction("Show"), "Show list");
    const quint32 shut = st->addCue(m_list, -1, "Shut");
    const quint32 opens = st->addCue(m_list, 0, "Opens");
    Target t;
    t.function = open->id();
    QVERIFY(st->addLayer(opens, t) != ShowTree::invalidId());
    {
        Cue timed = *st->cue(opens);
        timed.fadeIn = 20000;               // a long travel: the glide is still on
        QVERIFY(st->setCue(timed));
    }
    Q_UNUSED(shut);
    m_doc->outputRuleKeeper()->rebuild();
    Chaser *c = chaserOf(m_list);
    QVERIFY(c != nullptr);
    OutputRuleKeeper *rules = m_doc->outputRuleKeeper();
    const int ch = 0;

    // Picked while editing: held shut. Then a GO lets go over the cue's fade.
    st->pickCue(opens);
    settle(10);
    QCOMPARE(at("Main drape curtain", ch), 0);
    QVERIFY(rules->machineryHeldForEdit());
    ShowControl *sc = m_doc->showControl();
    sc->setMainCueList(c->id());
    ShowControl::Request load;
    load.command = ShowControl::Load;
    load.chaser = c->id();
    load.cue = st->cueNumber(*st->cueList(m_list), 1);
    sc->execute(load);
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = c->id();
    sc->execute(go);
    QTest::qWait(50);
    settle(1);
    QVERIFY(!rules->machineryHeldForEdit());
    QVERIFY2(!rules->m_editLayer->channelCounts().isEmpty(), "the let-go glides");
    settle(int(3000 / MasterTimer::tick()));      // part way
    const int moving = at("Main drape curtain", ch);
    QVERIFY2(moving > 0 && moving < 255, qPrintable(QString::number(moving)));

    // The emergency stop mid-glide: frozen there.
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    sc->execute(off);
    QTest::qWait(100);
    settle(5);
    const int frozen = at("Main drape curtain", ch);
    QVERIFY2(qAbs(frozen - moving) <= 3, qPrintable(QStringLiteral("%1 -> %2").arg(moving).arg(frozen)));
    QVERIFY(rules->stageMachineryHeld());

    // GO: the glide is dropped; the cue owns it, from the frozen value.
    sc->execute(load);
    sc->execute(go);
    settle(2);
    QTest::qWait(int(4 * MasterTimer::tick()) + 150);
    settle(2);
    QVERIFY(!rules->stageMachineryHeld());
    QVERIFY2(rules->m_editLayer->channelCounts().isEmpty(), "the glide's fader is gone");
    const int first = at("Main drape curtain", ch);
    QVERIFY2(qAbs(first - frozen) <= 8, qPrintable(QStringLiteral("frozen %1, after the GO %2").arg(frozen).arg(first)));
    settle(int(21000 / MasterTimer::tick()));
    QCOMPARE(at("Main drape curtain", ch), 255);
}

void TrackBack_Test::parkWinsOverHoldsAndCues()
{
    QVERIFY(rig());
    Chaser *c = behavesList();
    QVERIFY(c != nullptr);
    ProgrammerController *pc = m_doc->programmer();
    Fixture *relay = fixtureNamed("House relay");
    Fixture *wash = fixtureNamed("Stage wash");
    // Parked out: no cue, pick or stop turns it on.
    pc->parkFixtures({ relay->id(), wash->id() });
    settle(2);
    QCOMPARE(at("House relay"), 0);
    m_doc->showTree()->pickCue(m_c2);
    settle(20);
    QCOMPARE(at("House relay"), 0);
    QCOMPARE(at("Stage wash"), 0);
    m_doc->showTree()->stopPreview();
    m_doc->setMode(Doc::Operate);
    act(c, { Go, 0 });
    settle(10);
    act(c, { Go, 0 });
    settle(20);
    QCOMPARE(at("House relay"), 0);
    QCOMPARE(at("Kabuki drop"), 255);
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    m_doc->showControl()->execute(off);
    QTest::qWait(200);
    act(c, { Go, 0 });
    QTest::qWait(int(4 * MasterTimer::tick()) + 150);
    settle(20);
    QCOMPARE(at("House relay"), 0);
    QVERIFY(pc->isFixtureParked(relay->id()));
    // Unparked: the cues' again.
    pc->unparkFixtures({ relay->id(), wash->id() });
    act(c, { Back, 0 });
    settle(20);
    act(c, { Go, 0 });
    QTest::qWait(int(4 * MasterTimer::tick()) + 150);
    settle(20);
    QCOMPARE(c->currentStepIndex(), 1);
    QCOMPARE(at("House relay"), 255);
    QCOMPARE(at("Stage wash"), 128);
    m_doc->setMode(Doc::Design);
}

void TrackBack_Test::behavesAsIsGuessedSetAndSaved()
{
    QVERIFY(rig());
    QVERIFY(behavesList() != nullptr);
    QLCFixtureDef *curtainDef = m_doc->fixtureDefCache()->fixtureDef("Generic", "Stage Curtain");
    QVERIFY(curtainDef != nullptr);
    Fixture *curtain = new Fixture(m_doc);
    curtain->setName("Main drape curtain");
    curtain->setFixtureDefinition(curtainDef, curtainDef->mode("Open + Fly"));
    curtain->setAddress(100);
    QVERIFY(m_doc->addFixture(curtain));
    auto named = [&](const char *n) { return fixtureNamed(QString::fromLatin1(n)); };
    // The guesses, from the name, the rules, the symbol and the definition.
    QCOMPARE(FixtureRest::behaviour(named("Stage wash")), Fixture::LightBehaviour);
    QCOMPARE(FixtureRest::behaviour(named("House relay")), Fixture::SwitchBehaviour);
    QCOMPARE(FixtureRest::behaviour(named("Kabuki drop")), Fixture::OneShotBehaviour);
    QCOMPARE(FixtureRest::behaviour(named("CO2 jet")), Fixture::HazardBehaviour);
    QCOMPARE(FixtureRest::behaviour(curtain), Fixture::MachineryBehaviour);
    for (Fixture *f : m_doc->fixtures())
        if (f->fixtureDef() != nullptr && f->fixtureDef()->model() == "Sharpy")
            QCOMPARE(FixtureRest::behaviour(f), Fixture::LightBehaviour);
    // A silk flame is a light; a real one a hazard.
    Fixture *silk = new Fixture(m_doc);
    silk->setName("Silk flame (relay)");
    silk->setChannels(1);
    silk->setAddress(200);
    QVERIFY(m_doc->addFixture(silk));
    QCOMPARE(FixtureRest::behaviour(silk), Fixture::SwitchBehaviour);
    silk->setName("Flame projector");
    QCOMPARE(FixtureRest::behaviour(silk), Fixture::HazardBehaviour);
    QVERIFY(m_doc->deleteFixture(silk->id()));

    // A laser: its definition's class picks. None or 1-2M a light; 3R and
    // up a hazard -- and set to Light, Show Review lists it.
    QLCFixtureDef *laserDef = new QLCFixtureDef();
    laserDef->setManufacturer("Test");
    laserDef->setModel("Sky Laser");
    laserDef->setType(QLCFixtureDef::Laser);
    QLCChannel *lch = new QLCChannel();
    lch->setName("Intensity");
    lch->setGroup(QLCChannel::Intensity);
    laserDef->addChannel(lch);
    QLCFixtureMode *lmode = new QLCFixtureMode(laserDef);
    lmode->setName("1 Channel");
    lmode->insertChannel(lch, 0);
    laserDef->addMode(lmode);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(laserDef));
    Fixture *laser = new Fixture(m_doc);
    laser->setName("Sky laser");
    laser->setFixtureDefinition(laserDef, lmode);
    laser->setAddress(300);
    QVERIFY(m_doc->addFixture(laser));
    QCOMPARE(FixtureRest::behaviour(laser), Fixture::LightBehaviour);       // none said
    QLCPhysical phys = lmode->physical();
    phys.setLaserClass(QLCPhysical::LaserClass1);
    lmode->setPhysical(phys);
    QCOMPARE(FixtureRest::behaviour(laser), Fixture::LightBehaviour);
    phys.setLaserClass(QLCPhysical::LaserClass3B);
    lmode->setPhysical(phys);
    QCOMPARE(FixtureRest::behaviour(laser), Fixture::HazardBehaviour);
    QVERIFY(!FixtureRest::isHazardousLaserAsLight(laser));
    laser->setBehaviourSetting(Fixture::LightBehaviour);
    QVERIFY(FixtureRest::isHazardousLaserAsLight(laser));
    // The .qxf: the class round-trips under <Physical>.
    {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        phys.saveXML(&w);
        buf.close();
        QVERIFY(buf.data().contains("<Laser Class=\"3B\"/>"));
        QXmlStreamReader r(buf.data());
        r.readNextStartElement();
        QLCPhysical back;
        QVERIFY(back.loadXML(r));
        QCOMPARE(back.laserClass(), QLCPhysical::LaserClass3B);
    }

    // Set by hand: saved; guessed: nothing written (old shows load as before).
    Fixture *drop = named("Kabuki drop");
    Fixture *wash = named("Stage wash");
    drop->setBehaviourSetting(Fixture::MachineryBehaviour);
    QCOMPARE(FixtureRest::behaviour(drop), Fixture::MachineryBehaviour);
    auto roundTrip = [](Fixture *f, Doc *doc) {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        f->saveXML(&w);
        buf.close();
        QXmlStreamReader r(buf.data());
        r.readNextStartElement();
        Fixture *back = new Fixture(doc);
        back->loadXML(r, doc, doc->fixtureDefCache());
        const int set = back->behaviourSetting();
        delete back;
        return qMakePair(buf.data(), set);
    };
    const auto dropXml = roundTrip(drop, m_doc);
    QVERIFY(dropXml.first.contains("<BehavesAs>Machinery</BehavesAs>"));
    QCOMPARE(dropXml.second, int(Fixture::MachineryBehaviour));
    const auto washXml = roundTrip(wash, m_doc);
    QVERIFY(!washXml.first.contains("BehavesAs"));
    QCOMPARE(washXml.second, -1);

    // Show Review: FIX-20 for the drop, FIX-21 for the laser.
    ShowReview::Options o;
    o.checks = QStringList() << QStringLiteral("FIX-20") << QStringLiteral("FIX-21");
    o.record = false;
    const ShowReview::Result res = m_doc->showReview()->run(o);
    QStringList got;
    for (const ReviewFinding &f : res.findings)
        got << f.check + QLatin1Char(':') + f.targets.join(QLatin1Char(','));
    QVERIFY2(got.contains(QStringLiteral("FIX-20:fx:%1").arg(drop->id())), qPrintable(got.join(' ')));
    QVERIFY2(got.contains(QStringLiteral("FIX-21:fx:%1").arg(laser->id())), qPrintable(got.join(' ')));
    QCOMPARE(got.size(), 2);

    // The footer: a GO that would fire the hazard says why it did not.
    drop->setBehaviourSetting(-1);
    m_doc->deleteFixture(laser->id());
    m_doc->outputRuleKeeper()->rebuild();
    Chaser *c = chaserOf(m_list);
    m_doc->setMode(Doc::Operate);
    OutputRuleKeeper *rules = m_doc->outputRuleKeeper();
    QCOMPARE(rules->hazardRefusedText(), QString());
    act(c, { Go, 0 });
    settle(5);
    act(c, { Go, 0 });
    settle(5);
    QTRY_COMPARE_WITH_TIMEOUT(rules->hazardRefusedText(), QStringLiteral("Hazard — needs arming (coming)"), 2000);
    QCOMPARE(rules->hazardsRefused(), QList<quint32>() << named("CO2 jet")->id());
    act(c, { Go, 0 });          // cue 3 tracks the jet on: still refused
    settle(5);
    QTest::qWait(300);
    QCOMPARE(rules->hazardsRefused().size(), 1);
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    m_doc->showControl()->execute(off);
    QTest::qWait(100);
    QCOMPARE(rules->hazardRefusedText(), QString());
    m_doc->setMode(Doc::Design);
}

bool TrackBack_Test::loadShow(const QString &path)
{
    stopAll();
    m_doc->clearContents();
    const QString folder = QFileInfo(path).absolutePath();
    m_doc->setWorkspacePath(folder);
    if (m_doc->showEmbedding()->readWorkspace(path))
        m_doc->showEmbedding()->apply(folder);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QXmlStreamReader r(&file);
    while (!r.atEnd())
        if (r.readNextStartElement() && r.name() == QLatin1String("Engine"))
            return m_doc->loadXML(r, false);
    return false;
}

/* A long list where every cue sets a colour and a position nothing else
   does (no scene shared to save work on): a jump to the end and a Back
   walk the whole list back up, in one DMX tick, and land as the run from
   the top does. */
void TrackBack_Test::aLongList()
{
    QVERIFY(rig());
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("Long"), "Long list");
    const int count = 300;
    st->beginBatch("long");
    for (int i = 0; i < count; ++i)
    {
        QLCPalette *colour = new QLCPalette(QLCPalette::Color, m_doc);
        colour->setName(QString("Colour %1").arg(i));
        colour->setValue(QLCPalette::colorToString(QColor::fromHsv(i % 360, 255, 255), QColor(Qt::black)));
        m_doc->addPalette(colour);
        QLCPalette *position = new QLCPalette(QLCPalette::PanTilt, m_doc);
        position->setName(QString("Position %1").arg(i));
        position->setValue(i % 360, (i * 7) % 180);
        m_doc->addPalette(position);
        const quint32 c = st->addCue(listId, i - 1, QString("Cue %1").arg(i + 1));
        const quint32 g = i % 2 ? m_a : m_b;
        set(c, g, colour->id());
        set(c, g, position->id());
        set(c, g, m_full);
    }
    st->endBatch(true);
    Chaser *c = chaserOf(listId);
    QVERIFY(c != nullptr);
    QCOMPARE(c->stepsCount(), count);
    m_settle = 8;

    stopAll();
    QByteArray last, beforeLast;
    for (int i = 0; i < count; ++i)
    {
        act(c, { Go, 0 });
        beforeLast = last;
        last = land(c);
    }
    QCOMPARE(c->currentStepIndex(), count - 1);

    stopAll();
    act(c, { Jump, 10 });
    land(c);
    QElapsedTimer t;
    act(c, { Jump, count - 1 });
    t.start();
    tick(1);
    const qint64 jumpMs = t.elapsed();
    QCOMPARE(land(c), last);
    act(c, { Back, 0 });
    t.start();
    tick(1);
    const qint64 backMs = t.elapsed();
    QCOMPARE(land(c), beforeLast);
    qInfo().noquote() << QString("%1 cues: the jump's tick %2 ms, the Back's %3 ms").arg(count).arg(jumpMs).arg(backMs);
}

/* One row per show: Qt's watchdog (300 s) counts each row on its own. A
   lesson's start is the lesson before's finish, byte for byte: the same
   show is walked once, under its first name. */
static const int kShards = 4;

void TrackBack_Test::courseShows_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<int>("shard");
    QDir dir(QStringLiteral(TRACKBACK_SOURCE_DIR "/docs/course/shows"));
    const QStringList shows = dir.entryList({ QStringLiteral("lesson-1-*.qlcc") }, QDir::Files, QDir::Name);
    QVERIFY(!shows.isEmpty());
    QSet<QByteArray> seen;
    for (const QString &name : shows)
    {
        QFile file(dir.filePath(name));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(name));
        const QByteArray sum = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
        if (seen.contains(sum))
            continue;
        seen.insert(sum);
        // Shards: each row walks every kShards-th list, so a show's walk
        // is several short rows (Qt's 300 s watchdog counts each on its own)
        // and a gate that overlaps another still finishes every row.
        for (int shard = 0; shard < kShards; ++shard)
            QTest::newRow(qPrintable(QStringLiteral("%1 #%2").arg(name).arg(shard))) << dir.filePath(name) << shard;
    }
}

void TrackBack_Test::courseShows()
{
    QFETCH(QString, path);
    QFETCH(int, shard);
    const QString name = QFileInfo(path).fileName();
    QVERIFY2(loadShow(path), qPrintable(name));
    int index = 0;
    for (Function *f : m_doc->functions())
    {
        Chaser *c = qobject_cast<Chaser *>(f);
        if (c == nullptr || !m_doc->showTree()->isGenerated(c->id()) || c->stepsCount() < 2)
            continue;
        if (index++ % kShards != shard)
            continue;
        QString why;
        const int differ = everyWay(c, 3, &why);
        QVERIFY2(differ == 0, qPrintable(QStringLiteral("%1, %2: %3").arg(name, c->name(), why)));
    }
}

void TrackBack_Test::aRealShow()
{
    const QString path = qEnvironmentVariable("QLC_TRACKBACK_SHOW");
    if (path.isEmpty())
        QSKIP("set QLC_TRACKBACK_SHOW to a copy of a show (outputs stripped)");
    QVERIFY(loadShow(path));
    int lists = 0;
    for (Function *f : m_doc->functions())
    {
        Chaser *c = qobject_cast<Chaser *>(f);
        if (c == nullptr || !m_doc->showTree()->isGenerated(c->id()) || c->stepsCount() < 2)
            continue;
        ++lists;
        QString why;
        const int differ = everyWay(c, 20, &why);
        QVERIFY2(differ == 0, qPrintable(QStringLiteral("%1: %2").arg(c->name(), why)));
    }
    QVERIFY(lists > 0);
}

void TrackBack_Test::hisCurtain()
{
    const QString path = qEnvironmentVariable("QLC_CURTAIN_SHOW");
    if (path.isEmpty())
        QSKIP("set QLC_CURTAIN_SHOW");
    QVERIFY(loadShow(path));
    Chaser *c = nullptr;
    for (Function *f : m_doc->functions())
        if (f->name() == "Production Show" && f->type() == Function::ChaserType)
            c = qobject_cast<Chaser *>(f);
    QVERIFY(c != nullptr);
    Fixture *cur = fixtureNamed("Main drape 2 curtain");
    QVERIFY(cur != nullptr);
    auto idx = [&](const QString &cue) {
        for (int i = 0; i < c->stepsCount(); ++i)
            if (c->steps().at(i).note == cue || c->stepCue(i) == cue)
                return i;
        return -1;
    };
    const int s101 = idx("1.0.1"), s1021 = idx("1.0.2.1"), s111 = idx("1.1.1");
    qInfo() << "steps" << s101 << s1021 << s111 << "uni" << cur->universe() << "addr" << cur->address();
    m_settle = 250;
    auto open = [&](const char *what) {
        const QByteArray o = settle(0);
        const int a = int(cur->universeAddress());
        qInfo().noquote() << what << "step" << c->currentStepIndex() << "Open" << (a < o.size() ? int(uchar(o.at(a))) : -1)
                          << "fixture values" << cur->channelValues().toHex();
    };
    stopAll();
    for (int i = 0; i <= s111 && i < 40; ++i) { act(c, { Go, 0 }); land(c); if (c->currentStepIndex() >= s111) break; }
    open("GO from the top to 1.1.1:");
    stopAll(); act(c, { Jump, s111 }); land(c); open("jump to 1.1.1 from dark:");
    stopAll(); act(c, { Jump, s1021 }); land(c); open("jump to 1.0.2.1 from dark:");
    stopAll(); act(c, { Jump, s101 - 1 }); land(c); act(c, { Go, 0 }); tick(40); open("mid 1.0.1 fade:");
    act(c, { Jump, s111 }); land(c); open("then jump to 1.1.1:");
    stopAll(); act(c, { Jump, s101 }); land(c); open("jump to 1.0.1:");
    act(c, { Jump, s111 }); land(c); open("then jump to 1.1.1:");
    // Design: a cue picked in the Run or the Show Tree previews it.
    stopAll();
    ShowTreeController *st = m_doc->showTree();
    quint32 listId = ShowTree::invalidId();
    st->productionOfChaser(c->id(), &listId);
    const CueList *list = st->cueList(listId);
    QVERIFY(list != nullptr);
    for (const char *n : { "1.0.1", "1.1.1", "1.0.2.1" })
        for (int i = 0; i < list->cues.count(); ++i)
            if (st->cueNumber(*list, i) == QLatin1String(n))
            {
                st->pickCue(list->cues.at(i).id);
                settle(200);
                open(qPrintable(QStringLiteral("preview %1:").arg(n)));
            }
    st->stopPreview();
    // Design: GO 1.0.1, and while it opens pick 1.1.1 (the Run's click).
    auto cueId = [&](const char *n) {
        for (int i = 0; i < list->cues.count(); ++i)
            if (st->cueNumber(*list, i) == QLatin1String(n))
                return list->cues.at(i).id;
        return ShowTree::invalidId();
    };
    stopAll(); act(c, { Jump, s101 - 1 }); land(c); act(c, { Go, 0 }); tick(40); open("Design mid 1.0.1:");
    st->pickCue(cueId("1.1.1")); settle(200); open("then pick 1.1.1:");
    st->stopPreview(); settle(50); open("preview stopped:");
    // Operate: Load 1.1.1 then GO, from 1.0.1 mid-fade, as the Run does.
    st->stopPreview();
    stopAll();
    m_doc->setMode(Doc::Operate);
    act(c, { Jump, s101 - 1 }); land(c); act(c, { Go, 0 }); tick(40); open("Operate mid 1.0.1:");
    ShowControl::Request load; load.command = ShowControl::Load; load.chaser = c->id(); load.cue = "1.1.1";
    qInfo() << "load" << m_doc->showControl()->run(load, ShowControl::RouteRunTab);
    ShowControl::Request go; go.command = ShowControl::Go; go.chaser = c->id();
    qInfo() << "go" << m_doc->showControl()->run(go, ShowControl::RouteRunTab);
    land(c); open("then Load 1.1.1 + GO:");
    stopAll();
    load.cue = "1.1.1"; m_doc->showControl()->run(load, ShowControl::RouteRunTab);
    m_doc->showControl()->run(go, ShowControl::RouteRunTab);
    land(c); open("Operate from dark Load 1.1.1 + GO:");
}

QTEST_MAIN(TrackBack_Test)

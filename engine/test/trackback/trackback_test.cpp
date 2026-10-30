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
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QtTest>

#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#include "genericfader.h"
#undef protected
#undef private
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
#include "qlcchannel.h"
#include "chaser.h"
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

QTEST_MAIN(TrackBack_Test)

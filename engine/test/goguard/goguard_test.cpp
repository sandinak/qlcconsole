/*
  Q Light Controller Plus
  goguard_test.cpp

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
#include <QSignalSpy>
#include <QBuffer>
#include <QtEndian>
#include <QtTest>

// MasterTimer::timerTick() is private: driven by hand, as showcontrol_test does.
#include "../common/stdprelude.h"
#define private public
#include "mastertimer.h"
#undef private
#include "programmercontroller.h"
#include "outputrulekeeper.h"
#include "controllerscheme.h"
#include "functionparent.h"
#include "showcontrol.h"
#include "showtreemodel.h"
#include "chaserstep.h"
#include "padrun.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "goguard_test.h"

namespace
{
/** An OSC message with no arguments. */
QByteArray osc(const QString &address)
{
    auto pad = [](QByteArray &b) {
        do
            b.append('\0');
        while (b.size() % 4 != 0);
    };
    QByteArray b = address.toUtf8();
    pad(b);
    b.append(',');
    pad(b);
    return b;
}

QString name(ShowControl::Route route)
{
    return ShowControl::routeName(route);
}

/** Rule 1's words: a GO held while cue @a cue (that @a route's GO started)
 *  is still starting. */
QString starting(const QString &cue, ShowControl::Route route)
{
    return QStringLiteral("Second GO held: cue %1 still starting (%2's GO)").arg(cue, ShowControl::sourceName(route));
}
}

void GoGuard_Test::init()
{
    m_doc = new Doc(this);
    m_now = 100000;
    m_clockReads = 0;
    ShowControl *sc = m_doc->showControl();
    sc->setGuardClock([this]() {
        ++m_clockReads;
        return m_now;
    });
    sc->setGoGuardMs(ShowControl::defaultGoGuardMs());
}

void GoGuard_Test::cleanup()
{
    stopAll();
    delete m_doc;
    m_doc = nullptr;
}

Chaser *GoGuard_Test::makeList(int steps, const QString &listName)
{
    Chaser *c = new Chaser(m_doc);
    c->setName(listName);
    // Steps that hold until the next GO.
    c->setDurationMode(Chaser::Common);
    c->setDuration(Function::infiniteSpeed());
    m_doc->addFunction(c);
    for (int i = 0; i < steps; ++i)
    {
        Scene *s = new Scene(m_doc);
        s->setName(QString("%1 step %2").arg(listName).arg(i + 1));
        m_doc->addFunction(s);
        c->addStep(ChaserStep(s->id()));
    }
    return c;
}

Chaser *GoGuard_Test::makeSubCueList()
{
    // Cue 2 has three sub-cues on Sub GO (as showcontrol_test's).
    Chaser *in = makeList(3, "Inner");
    Chaser *list = makeList(3, "Main");
    ChaserStep step(in->id());
    step.subCues = ChaserStep::SubGo;
    list->replaceStep(step, 1);
    m_doc->showControl()->setMainCueList(list->id());
    return list;
}

void GoGuard_Test::tick()
{
    for (int i = 0; i < 3; ++i)
        m_doc->masterTimer()->timerTick();
}

void GoGuard_Test::stopAll()
{
    if (m_doc == nullptr)
        return;
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    for (int i = 0; i < 5; ++i)
        m_doc->masterTimer()->timerTick();
}

QList<ShowControl::Route> GoGuard_Test::sources()
{
    return { ShowControl::RouteKey,        ShowControl::RouteRunTab,     ShowControl::RouteVcRunner,
             ShowControl::RoutePad,        ShowControl::RouteVcCueList,  ShowControl::RouteControlMap,
             ShowControl::RouteFocusCall,  ShowControl::RouteOsc,        ShowControl::RouteMsc,
             ShowControl::RouteWeb,        ShowControl::RouteOther };
}

bool GoGuard_Test::send(ShowControl::Command command, ShowControl::Route route, Chaser *list,
                        const QString &cue)
{
    ShowControl::Request r;
    r.command = command;
    r.chaser = list != nullptr ? list->id() : UINT_MAX;
    r.cue = cue;
    const bool done = m_doc->showControl()->run(r, route);
    tick();
    return done;
}

/*****************************************************************************
 * Rule 1
 *****************************************************************************/

void GoGuard_Test::aSecondGoInTheWindowIsHeldFromEverySource()
{
    Chaser *list = makeList(4, "Main");
    m_doc->showControl()->setMainCueList(list->id());
    ShowControl *sc = m_doc->showControl();
    QSignalSpy held(sc, &ShowControl::commandHeld);

    for (ShowControl::Route first : sources())
        for (ShowControl::Route second : sources())
        {
            const QString pair = name(first) + " then " + name(second);
            stopAll();
            m_now += 10000;
            held.clear();
            QVERIFY2(send(ShowControl::Go, first, list), qPrintable(pair + ": the first GO played"));
            QCOMPARE(list->currentStepIndex(), 0);
            m_now += 200;
            QVERIFY2(!send(ShowControl::Go, second, list), qPrintable(pair + ": the second GO played"));
            QVERIFY2(list->currentStepIndex() == 0, qPrintable(pair + ": the cue advanced"));
            QVERIFY2(held.count() == 1, qPrintable(pair + ": not said once"));
            const QString want = starting(QStringLiteral("1"), first);
            QCOMPARE(held.first().at(0).toString(), want);
            QCOMPARE(held.first().at(1).toUInt(), list->id());
            QCOMPARE(held.first().at(2).value<ShowControl::Route>(), second);
            QCOMPARE(sc->heldNotice(), want);
            // A GO with no list named (the main one) is the same list.
            QVERIFY2(!send(ShowControl::Go, second, nullptr), qPrintable(pair + ": by the main list"));
            QCOMPARE(list->currentStepIndex(), 0);
            // And a GO naming a cue is a GO too.
            QVERIFY2(!send(ShowControl::Go, second, list, "3"), qPrintable(pair + ": a named cue"));
            QCOMPARE(list->currentStepIndex(), 0);
        }
}

void GoGuard_Test::theWindowRunsFromTheGoThatPlayed()
{
    Chaser *list = makeList(5, "Main");
    m_doc->showControl()->setMainCueList(list->id());
    for (ShowControl::Route source : sources())
    {
        stopAll();
        m_now += 10000;
        QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
        // The cue has begun (the runner is on it): held to goBegunMs() only.
        m_now += ShowControl::goBegunMs() - 1;
        QVERIFY2(!send(ShowControl::Go, source, list), qPrintable(name(source) + " at 249 ms"));
        QCOMPARE(list->currentStepIndex(), 0);
        m_now += 1;                                         // 250 ms: begun
        QVERIFY2(send(ShowControl::Go, source, list), qPrintable(name(source) + " at 250 ms"));
        QCOMPARE(list->currentStepIndex(), 1);
        // From that GO: one at +150 is held, and does not move the window on.
        m_now += 150;
        QVERIFY(!send(ShowControl::Go, source, list));
        m_now += 100;
        QVERIFY2(send(ShowControl::Go, source, list), qPrintable(name(source) + ": a held GO lengthened it"));
        QCOMPARE(list->currentStepIndex(), 2);
    }

    // A longer setting is a longer cap, not a longer hold once a cue has begun.
    m_doc->showControl()->setGoGuardMs(2500);
    stopAll();
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 300;
    QVERIFY(send(ShowControl::Go, ShowControl::RoutePad, list));
    QCOMPARE(m_doc->showControl()->goGuardMs(), 2500);
    m_doc->showControl()->setGoGuardMs(99999);
    QCOMPARE(m_doc->showControl()->goGuardMs(), ShowControl::maxGoGuardMs());
}

void GoGuard_Test::zeroTurnsItOff()
{
    // The engine alone: off, until the console sets it from Settings.
    {
        Doc bare(nullptr);
        QCOMPARE(bare.showControl()->goGuardMs(), 0);
    }
    Chaser *list = makeList(4, "Main");
    m_doc->showControl()->setMainCueList(list->id());
    QSignalSpy held(m_doc->showControl(), &ShowControl::commandHeld);
    m_doc->showControl()->setGoGuardMs(0);
    for (ShowControl::Route source : sources())
    {
        stopAll();
        m_now += 10000;
        QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
        m_now += 1;
        QVERIFY2(send(ShowControl::Go, source, list), qPrintable(name(source) + " held with the guard off"));
        QCOMPARE(list->currentStepIndex(), 1);
    }
    QCOMPARE(held.count(), 0);
    QVERIFY(m_doc->showControl()->heldNotice().isEmpty());
}

/*****************************************************************************
 * Rule 3
 *****************************************************************************/

void GoGuard_Test::backSubCuesAndStopAreHeldTheSame()
{
    Chaser *list = makeSubCueList();
    ShowControl *sc = m_doc->showControl();
    QSignalSpy held(sc, &ShowControl::commandHeld);
    const QList<ShowControl::Command> others = { ShowControl::Back, ShowControl::SubGo,
                                                 ShowControl::SubBack, ShowControl::Stop };

    // After a GO: each of the others held, from every source.
    for (ShowControl::Command command : others)
        for (ShowControl::Route source : sources())
        {
            const QString what = ShowControl::commandName(command) + " by " + name(source);
            stopAll();
            m_now += 10000;
            QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));            // cue 1
            m_now += 2000;
            QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));            // cue 2, its sub-cue 2.1
            QCOMPARE(list->currentStepIndex(), 1);
            const int sub = list->currentSubCueIndex();
            held.clear();
            m_now += 300;
            QVERIFY2(!send(command, source, list), qPrintable(what + " played"));
            QVERIFY2(list->isRunning() && list->currentStepIndex() == 1 && list->currentSubCueIndex() == sub,
                     qPrintable(what + ": the list moved"));
            QCOMPARE(held.count(), 1);
            QCOMPARE(held.first().at(0).toString(),
                     QStringLiteral("%1 held: Laptop's GO took it 0.3 s ago").arg(ShowControl::commandName(command)));
        }

    // Each of them, carried out, holds a GO after it (a GO right after a Back too).
    for (ShowControl::Command command : others)
        for (ShowControl::Route source : sources())
        {
            const QString what = ShowControl::commandName(command) + " by " + name(source);
            stopAll();
            m_now += 10000;
            QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));            // cue 1
            m_now += 2000;
            QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));            // cue 2.1
            if (command == ShowControl::SubBack)
            {
                m_now += 2000;
                QVERIFY(send(ShowControl::SubGo, ShowControl::RouteKey, list));     // 2.2, for Sub Back to leave
            }
            m_now += 2000;
            QVERIFY2(send(command, source, list), qPrintable(what + " outside the window"));
            const bool running = list->isRunning();
            const int step = list->currentStepIndex();
            const int sub = list->currentSubCueIndex();
            held.clear();
            m_now += 300;
            QVERIFY2(!send(ShowControl::Go, ShowControl::RouteRunTab, list), qPrintable("GO after " + what));
            QCOMPARE(list->isRunning(), running);
            if (running)
            {
                QCOMPARE(list->currentStepIndex(), step);
                QCOMPARE(list->currentSubCueIndex(), sub);
            }
            QCOMPARE(held.first().at(0).toString(),
                     QStringLiteral("GO held: %1's %2 took it 0.3 s ago")
                         .arg(ShowControl::sourceName(source), ShowControl::commandName(command)));
        }

    // A command that did nothing (Back on a stopped list) opens no window.
    stopAll();
    m_now += 10000;
    QVERIFY(!send(ShowControl::Back, ShowControl::RouteKey, list));
    m_now += 100;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
}

void GoGuard_Test::theEmergencyStopIsNeverHeld()
{
    Chaser *list = makeList(4, "Main");
    m_doc->showControl()->setMainCueList(list->id());
    QSignalSpy held(m_doc->showControl(), &ShowControl::commandHeld);
    for (ShowControl::Route source : sources())
    {
        stopAll();
        m_now += 10000;
        QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
        m_now += 50;
        QVERIFY2(send(ShowControl::AllOff, source, nullptr), qPrintable("ALL OFF by " + name(source) + " held"));
        tick();
        QVERIFY(!list->isRunning());
        // ALL OFF again at once: still never held.
        QVERIFY(send(ShowControl::AllOff, source, nullptr));
        // And it cleared the window: the GO after it plays.
        m_now += 50;
        QVERIFY2(send(ShowControl::Go, source, list), qPrintable("GO after ALL OFF by " + name(source)));
        QVERIFY(list->isRunning());
    }
    QCOMPARE(held.count(), 0);

    // Pause, Resume, Load and Reset are not held, and open no window.
    for (ShowControl::Route source : sources())
    {
        stopAll();
        m_now += 10000;
        QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
        m_now += 100;
        QVERIFY2(send(ShowControl::Pause, source, list), qPrintable("PAUSE by " + name(source)));
        m_now += 100;
        QVERIFY2(send(ShowControl::Resume, source, list), qPrintable("RESUME by " + name(source)));
        m_now += 100;
        QVERIFY2(send(ShowControl::Load, source, list, "3"), qPrintable("LOAD by " + name(source)));
        m_now += 100;
        QVERIFY2(send(ShowControl::Reset, source, list), qPrintable("RESET by " + name(source)));
        // The GO's window is over: none of them opened one of their own.
        m_now += 700;
        QVERIFY2(send(ShowControl::Go, source, list), qPrintable("GO after RESET by " + name(source)));
    }
    QCOMPARE(held.count(), 0);
}

/*****************************************************************************
 * Rule 4
 *****************************************************************************/

void GoGuard_Test::cueListsAreIndependent()
{
    Chaser *a = makeList(4, "A");
    Chaser *b = makeList(4, "B");
    m_doc->showControl()->setMainCueList(a->id());
    for (ShowControl::Route first : sources())
        for (ShowControl::Route second : sources())
        {
            const QString pair = name(first) + " on A, " + name(second) + " on B";
            stopAll();
            m_now += 10000;
            QVERIFY(send(ShowControl::Go, first, a));
            m_now += 100;
            QVERIFY2(send(ShowControl::Go, second, b), qPrintable(pair));
            QCOMPARE(b->currentStepIndex(), 0);
            m_now += 100;
            QVERIFY2(send(ShowControl::Back, second, b) == false, qPrintable(pair + ": B's own window"));
            QVERIFY2(!send(ShowControl::Go, second, a), qPrintable(pair + ": A's window"));
            QCOMPARE(a->currentStepIndex(), 0);
        }
}

void GoGuard_Test::timecodeIsNeverHeld()
{
    Chaser *list = makeList(5, "Main");
    m_doc->showControl()->setMainCueList(list->id());
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 50;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteTimecode, list, "2"));      // its time came
    QCOMPARE(list->currentStepIndex(), 1);
    m_now += 50;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteTimecode, list, "3"));
    QCOMPARE(list->currentStepIndex(), 2);
    // The laptop's GO's window still runs: the clock's GOs did not move it.
    m_now += 50;
    QVERIFY(!send(ShowControl::Go, ShowControl::RoutePad, list));

    // A timecode GO opens no window.
    stopAll();
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteTimecode, list, "1"));
    for (ShowControl::Route source : sources())
    {
        m_now += 10;
        QVERIFY2(send(ShowControl::Go, ShowControl::RouteTimecode, list), qPrintable(name(source)));
        m_now += 10;
        QVERIFY(m_doc->showControl()->goGuardMs() > 0);
        stopAll();
        QVERIFY2(send(ShowControl::Go, source, list), qPrintable(name(source) + " after a timecode GO"));
        stopAll();
        m_now += 10000;
        QVERIFY(send(ShowControl::Go, ShowControl::RouteTimecode, list, "1"));
    }
}

/*****************************************************************************
 * Rule 2
 *****************************************************************************/

void GoGuard_Test::theHoldIsSaidInOneText()
{
    QCOMPARE(ShowControl::heldText(ShowControl::Go, ShowControl::Go, ShowControl::RouteKey, 400),
             QStringLiteral("Second GO held: Laptop's GO took it 0.4 s ago"));
    QCOMPARE(ShowControl::heldText(ShowControl::Go, ShowControl::Go, ShowControl::RoutePad, 700),
             QStringLiteral("Second GO held: Pad's GO took it 0.7 s ago"));
    QCOMPARE(ShowControl::heldText(ShowControl::Back, ShowControl::Go, ShowControl::RouteWeb, 300),
             QStringLiteral("BACK held: Phone's GO took it 0.3 s ago"));
    // Rule 1's: why, not when (Q328).
    QCOMPARE(ShowControl::heldText(ShowControl::Go, ShowControl::Go, ShowControl::RouteKey, 100, "3"),
             QStringLiteral("Second GO held: cue 3 still starting (Laptop's GO)"));
    // Never "0.0 s": that reads as no time at all.
    QCOMPARE(ShowControl::heldText(ShowControl::Go, ShowControl::Go, ShowControl::RouteRunTab, 10),
             QStringLiteral("Second GO held: Laptop's GO took it 0.1 s ago"));
    // Every source has a name of its own.
    QStringList names;
    for (ShowControl::Route r : sources())
        names << ShowControl::sourceName(r);
    QCOMPARE(names.count(), sources().count());
    QVERIFY(!names.contains(QString()));

    Chaser *list = makeList(4, "Main");
    m_doc->showControl()->setMainCueList(list->id());
    ShowControl *sc = m_doc->showControl();
    QSignalSpy handled(sc, &ShowControl::commandHandled);
    QSignalSpy from(sc, &ShowControl::commandFrom);
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 200;
    QVERIFY(!send(ShowControl::Go, ShowControl::RouteWeb, list));
    const QString want = starting(QStringLiteral("1"), ShowControl::RouteKey);
    // The console's own line for a command (the show-control page, the web page's poll) says it too.
    QCOMPARE(handled.last().at(0).toString(), want);
    QCOMPARE(handled.last().at(1).toBool(), false);
    QCOMPARE(from.last().at(2).toString(), want);
    // For its seconds, then gone.
    QCOMPARE(sc->heldNotice(), want);
    m_now += ShowControl::heldNoticeMs() - 1;
    QCOMPARE(sc->heldNotice(), want);
    m_now += 1;
    QVERIFY(sc->heldNotice().isEmpty());
    // Another show: nothing carried over.
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 100;
    QVERIFY(!send(ShowControl::Go, ShowControl::RouteKey, list));
    sc->reset();
    QVERIFY(sc->heldNotice().isEmpty());
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
}

void GoGuard_Test::thePadBuzzesLongForEveryHold()
{
    Chaser *list = makeList(5, "Main");
    m_doc->showControl()->setMainCueList(list->id());
    ProgrammerController *pc = m_doc->programmer();
    PadRun *pad = pc->padRun();
    pad->setEnabled(true, false);
    QSignalSpy buzz(pc, &ProgrammerController::padRumbled);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    // Every other source's GO held after the pad's, and the pad's after each.
    for (ShowControl::Route source : sources())
    {
        /* The pad's GO first: the other's is held and said -- but the pad
           buzzes only for its own press (build 4910: Back on the PMJ, held,
           rumbled the DualShock nobody pressed). */
        stopAll();
        m_now += 10000;
        QVERIFY(pad->trigger(PadRun::Go));
        tick();
        buzz.clear();
        said.clear();
        m_now += 200;
        QVERIFY2(!send(ShowControl::Go, source, list), qPrintable(name(source) + " after the pad"));
        QCOMPARE(list->currentStepIndex(), 0);
        const QString afterPad = starting(QStringLiteral("1"), ShowControl::RoutePad);
        const int want = source == ShowControl::RoutePad ? 1 : 0;
        QVERIFY2(buzz.count() == want, qPrintable(name(source) + ": buzzed " + QString::number(buzz.count())));
        if (want == 1)
        {
            QCOMPARE(buzz.first().at(0).toInt(), 1);
            QCOMPARE(buzz.first().at(1).toInt(), ControllerScheme::rumbleLongMs());
        }
        QCOMPARE(said.count(), 1);
        QCOMPARE(said.first().at(0).toString(), afterPad);

        // The other's GO first: the pad's own is held, said once (no "nothing to play").
        stopAll();
        m_now += 10000;
        QVERIFY(send(ShowControl::Go, source, list));
        buzz.clear();
        said.clear();
        m_now += 200;
        QVERIFY(!pad->trigger(PadRun::Go));
        tick();
        QCOMPARE(list->currentStepIndex(), 0);
        const QString afterOther = starting(QStringLiteral("1"), source);
        QVERIFY2(buzz.count() == 1, qPrintable("the pad after " + name(source)));
        QCOMPARE(buzz.first().at(1).toInt(), ControllerScheme::rumbleLongMs());
        QCOMPARE(said.count(), 1);
        QCOMPARE(said.first().at(0).toString(), afterOther);
        // Back, Sub GO and Sub Back from the pad: held the same way.
        for (PadRun::Action a : { PadRun::Back, PadRun::SubGo, PadRun::SubBack })
        {
            buzz.clear();
            QVERIFY(!pad->trigger(a));
            QCOMPARE(buzz.count(), 1);
            QCOMPARE(buzz.first().at(1).toInt(), ControllerScheme::rumbleLongMs());
        }
    }

    // The emergency stop from the pad inside the window: never held.
    stopAll();
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 100;
    pad->emergencyStop();
    tick();
    QVERIFY(!list->isRunning());
    QCOMPARE(pad->state(), PadRun::Stopped);

    // In Tools the pad isn't running the show: no buzz.
    pad->setEnabled(false, false);
    stopAll();
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    buzz.clear();
    m_now += 200;
    QVERIFY(!send(ShowControl::Go, ShowControl::RouteKey, list));
    QCOMPARE(buzz.count(), 0);
}

/*****************************************************************************
 * Rules 5 and 6
 *****************************************************************************/

void GoGuard_Test::heldBeforeTheRunHandler()
{
    Chaser *list = makeList(4, "Main");
    ShowControl *sc = m_doc->showControl();
    sc->setMainCueList(list->id());
    QList<ShowControl::Route> reached;
    sc->setRunHandler([&reached, sc](const ShowControl::Request &r, ShowControl::Route route) {
        reached << route;
        return sc->execute(r);
    });
    QSignalSpy from(sc, &ShowControl::commandFrom);

    for (ShowControl::Route source : sources())
    {
        stopAll();
        m_now += 10000;
        reached.clear();
        QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
        QCOMPARE(reached.count(), 1);
        m_now += 200;
        QVERIFY(!send(ShowControl::Go, source, list));
        QVERIFY2(reached.count() == 1, qPrintable(name(source) + " reached the handler"));
    }

    // OSC's own path: held, and its line says where it came from.
    stopAll();
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 200;
    reached.clear();
    QCOMPARE(sc->handleOsc(osc("/go")), 0);
    tick();
    QVERIFY(reached.isEmpty());
    QCOMPARE(list->currentStepIndex(), 0);
    QCOMPARE(from.last().at(0).value<ShowControl::Source>(), ShowControl::Osc);
    QCOMPARE(from.last().at(2).toString(), starting(QStringLiteral("1"), ShowControl::RouteKey));
    // OSC first, the laptop held.
    stopAll();
    m_now += 10000;
    QCOMPARE(sc->handleOsc(osc("/go")), 1);
    tick();
    m_now += 200;
    QVERIFY(!send(ShowControl::Go, ShowControl::RouteKey, list));
    QCOMPARE(sc->heldNotice(), starting(QStringLiteral("1"), ShowControl::RouteOsc));

    // The pad's own path: held before the handler too.
    PadRun *pad = m_doc->programmer()->padRun();
    pad->setEnabled(true, false);
    stopAll();
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    reached.clear();
    m_now += 200;
    QVERIFY(!pad->trigger(PadRun::Go));
    QVERIFY(reached.isEmpty());
    pad->setEnabled(false, false);
    sc->setRunHandler(ShowControl::RunHandler());
}

void GoGuard_Test::nothingIsTimedPerFrame()
{
    Chaser *list = makeList(4, "Main");
    m_doc->showControl()->setMainCueList(list->id());
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    m_clockReads = 0;
    for (int i = 0; i < 500; ++i)
        m_doc->masterTimer()->timerTick();
    QCOMPARE(m_clockReads, 0);
    // A command reads it; the frames after it do not.
    m_now += 2000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    QVERIFY(m_clockReads > 0);
    m_clockReads = 0;
    for (int i = 0; i < 500; ++i)
        m_doc->masterTimer()->timerTick();
    QCOMPARE(m_clockReads, 0);
}

/*****************************************************************************
 * The emergency stop and the guard
 *****************************************************************************/

void GoGuard_Test::aHeldGoNeverLetsGoOfTheEmergencyStop()
{
    Chaser *list = makeList(5, "Main");
    ShowControl *sc = m_doc->showControl();
    sc->setMainCueList(list->id());
    OutputRuleKeeper *rules = m_doc->outputRuleKeeper();
    QVERIFY(rules != nullptr);
    m_doc->setMode(Doc::Operate);

    // ALL OFF, then a GO: it plays (ALL OFF cleared every window) and lets
    // the latch go; a second GO inside its window is held.
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 100;
    QVERIFY(send(ShowControl::AllOff, ShowControl::RoutePad, nullptr));
    QVERIFY(rules->emergencyStopped());
    m_now += 100;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    QVERIFY(!rules->emergencyStopped());
    m_now += 200;
    QVERIFY(!send(ShowControl::Go, ShowControl::RoutePad, list));

    /* The latch set while a window is open (here straight on the keeper, as
       ALL OFF would, which also clears the window): every held command,
       from every source, leaves it set. */
    rules->emergencyStop();
    QVERIFY(rules->emergencyStopped());
    const QList<ShowControl::Command> held = { ShowControl::Go, ShowControl::Back, ShowControl::SubGo,
                                               ShowControl::SubBack, ShowControl::Stop };
    for (ShowControl::Route source : sources())
        for (ShowControl::Command command : held)
        {
            QVERIFY2(!send(command, source, list),
                     qPrintable(ShowControl::commandName(command) + " by " + name(source) + " played"));
            QVERIFY2(rules->emergencyStopped(),
                     qPrintable(ShowControl::commandName(command) + " by " + name(source) + " let go of the latch"));
        }
    // The pad's own GO, held: the latch stays.
    PadRun *pad = m_doc->programmer()->padRun();
    pad->setEnabled(true, false);
    QVERIFY(!pad->trigger(PadRun::Go));
    QVERIFY(rules->emergencyStopped());
    pad->setEnabled(false, false);
    // The window over, the next GO plays and lets it go.
    m_now += 1000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    QVERIFY(!rules->emergencyStopped());

    // A fresh ALL OFF right after a GO: never held, latched, and the GO
    // after it is not held either (the window went with ALL OFF).
    m_now += 100;
    QVERIFY(send(ShowControl::AllOff, ShowControl::RouteWeb, nullptr));
    QVERIFY(rules->emergencyStopped());
    m_now += 100;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteWeb, list));
    QVERIFY(!rules->emergencyStopped());
    m_doc->setMode(Doc::Design);
}

void GoGuard_Test::aVcStepOnAPlainChaserIsHeld()
{
    // Two chasers, neither a Show Tree list: the VC's step steps the last
    // started directly (ProgrammerController::stepCurrentChaser()).
    Chaser *list = makeList(5, "Plain");
    makeList(2, "Other");
    ShowControl *sc = m_doc->showControl();
    QVERIFY(!sc->isTreeGoList(list->id()));
    QSignalSpy heldSpy(sc, &ShowControl::commandHeld);
    m_doc->setMode(Doc::Operate);
    list->start(m_doc->masterTimer(), FunctionParent::master());
    tick();
    QCOMPARE(list->currentStepIndex(), 0);

    m_now += 10000;
    m_doc->stepCurrentChaser(+1);
    tick();
    QCOMPARE(list->currentStepIndex(), 1);
    m_now += 200;
    m_doc->stepCurrentChaser(+1);
    tick();
    QCOMPARE(list->currentStepIndex(), 1);          // held
    QCOMPARE(heldSpy.count(), 1);
    // A direct step names no cue of its own: held to goBegunMs(), said by when.
    QCOMPARE(heldSpy.last().at(0).toString(), QStringLiteral("Second GO held: VC Cue List's GO took it 0.2 s ago"));
    m_doc->stepCurrentChaser(-1);
    tick();
    QCOMPARE(list->currentStepIndex(), 1);          // Back held too
    // Its window is the one run() reads: a GO from the laptop is held.
    QVERIFY(!send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 1000;
    m_doc->stepCurrentChaser(+1);
    tick();
    QCOMPARE(list->currentStepIndex(), 2);
    // And the laptop's window holds the step.
    m_now += 2000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    QCOMPARE(list->currentStepIndex(), 3);
    m_now += 200;
    m_doc->stepCurrentChaser(+1);
    tick();
    QCOMPARE(list->currentStepIndex(), 3);
    m_doc->setMode(Doc::Design);
}

/*****************************************************************************
 * Q328: the guard follows the cue
 *****************************************************************************/

namespace
{
void markFast(Chaser *list, int index, bool fast = true)
{
    ChaserStep step = list->steps().at(index);
    step.fast = fast;
    list->replaceStep(step, index);
}
}

void GoGuard_Test::theGuardFollowsTheCue()
{
    Chaser *list = makeList(5, "Main");
    ShowControl *sc = m_doc->showControl();
    sc->setMainCueList(list->id());
    QSignalSpy held(sc, &ShowControl::commandHeld);

    // A 0-fade cue: begun at once, so a GO 300 ms on goes (it was held).
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    QCOMPARE(list->currentStepIndex(), 0);
    m_now += 300;
    QVERIFY2(send(ShowControl::Go, ShowControl::RoutePad, list), "a GO 300 ms after a 0-fade cue was held");
    QCOMPARE(list->currentStepIndex(), 1);
    // One 100 ms on is held, and says why.
    held.clear();
    m_now += 100;
    QVERIFY(!send(ShowControl::Go, ShowControl::RouteKey, list));
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(held.count(), 1);
    QCOMPARE(held.first().at(0).toString(), starting(QStringLiteral("2"), ShowControl::RoutePad));
    QCOMPARE(sc->heldNotice(), starting(QStringLiteral("2"), ShowControl::RoutePad));

    // A 2 s fade: its fade has begun, so a GO at 400 ms goes.
    stopAll();
    list->setFadeInMode(Chaser::Common);
    list->setFadeInSpeed(2000);
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    QCOMPARE(list->currentStepIndex(), 0);
    m_now += 400;
    QVERIFY2(send(ShowControl::Go, ShowControl::RouteRunTab, list), "a GO 400 ms into a 2 s fade was held");
    QCOMPARE(list->currentStepIndex(), 1);
    // ... and one 200 ms into the next is still held.
    m_now += 200;
    QVERIFY(!send(ShowControl::Go, ShowControl::RouteRunTab, list));
    QCOMPARE(list->currentStepIndex(), 1);
}

void GoGuard_Test::aFastCueHasNoGuard()
{
    Chaser *list = makeList(5, "Main");
    ShowControl *sc = m_doc->showControl();
    sc->setMainCueList(list->id());
    markFast(list, 0);
    QVERIFY(list->isStepFast(0));
    QVERIFY(!list->isStepFast(1));
    QSignalSpy held(sc, &ShowControl::commandHeld);

    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));            // cue 1, Fast
    QCOMPARE(list->currentStepIndex(), 0);
    // At once: no guard after a Fast cue.
    QVERIFY2(send(ShowControl::Go, ShowControl::RouteKey, list), "the GO after a Fast cue was held");
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(held.count(), 0);
    // Cue 2 isn't Fast: guarded again.
    m_now += 10;
    QVERIFY(!send(ShowControl::Go, ShowControl::RouteKey, list));
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(held.count(), 1);

    // Fast takes the guard off a GO only: a Back after a Fast cue's GO is
    // held as before (rule 3).
    stopAll();
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));            // cue 1, Fast
    QCOMPARE(list->currentStepIndex(), 0);
    m_now += 100;
    QVERIFY2(!send(ShowControl::Back, ShowControl::RouteKey, list), "a Back after a Fast cue's GO played");
    QCOMPARE(list->currentStepIndex(), 0);
}

void GoGuard_Test::theCapHolds()
{
    Chaser *list = makeList(5, "Main");
    ShowControl *sc = m_doc->showControl();
    sc->setMainCueList(list->id());
    QSignalSpy held(sc, &ShowControl::commandHeld);

    // A GO whose cue the runner has not begun (no frame since): held ...
    m_now += 10000;
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = list->id();
    QVERIFY(sc->run(go, ShowControl::RouteKey));
    m_now += 400;
    QVERIFY2(!sc->run(go, ShowControl::RoutePad), "a GO before its cue began played");
    QCOMPARE(held.last().at(0).toString(), starting(QStringLiteral("1"), ShowControl::RouteKey));
    m_now += ShowControl::defaultGoGuardMs() - 401;
    QVERIFY(!sc->run(go, ShowControl::RoutePad));
    // ... to the setting, and no longer.
    m_now += 1;
    QVERIFY2(sc->run(go, ShowControl::RoutePad), "held past the setting");
    tick();
    QVERIFY(list->isRunning());

    // A setting under the floor is the cap too: 100 ms, begun or not.
    sc->setGoGuardMs(100);
    stopAll();
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 99;
    QVERIFY(!send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 1;
    QVERIFY2(send(ShowControl::Go, ShowControl::RouteKey, list), "held past a 100 ms setting");
    QCOMPARE(list->currentStepIndex(), 1);

    // Back keeps the whole window (rule 3): held at 900 ms of 1 s.
    sc->setGoGuardMs(ShowControl::defaultGoGuardMs());
    stopAll();
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 2000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 900;
    QVERIFY(!send(ShowControl::Back, ShowControl::RouteKey, list));
    QCOMPARE(list->currentStepIndex(), 1);
    m_now += 100;
    QVERIFY(send(ShowControl::Back, ShowControl::RouteKey, list));
    QCOMPARE(list->currentStepIndex(), 0);
}

void GoGuard_Test::everyRouteSharesTheCueRule()
{
    Chaser *list = makeList(5, "Main");
    ShowControl *sc = m_doc->showControl();
    sc->setMainCueList(list->id());
    for (ShowControl::Route first : sources())
        for (ShowControl::Route second : sources())
        {
            const QString pair = name(first) + " then " + name(second);
            markFast(list, 1, false);
            stopAll();
            m_now += 10000;
            QVERIFY2(send(ShowControl::Go, first, list), qPrintable(pair + ": the first GO"));
            m_now += 100;
            QVERIFY2(!send(ShowControl::Go, second, list), qPrintable(pair + ": played at 100 ms"));
            QCOMPARE(sc->heldNotice(), starting(QStringLiteral("1"), first));
            m_now += 200;
            QVERIFY2(send(ShowControl::Go, second, list), qPrintable(pair + ": held at 300 ms"));
            QCOMPARE(list->currentStepIndex(), 1);
            // Cue 2 made Fast: the GO after it goes at once, by either.
            markFast(list, 1);
            stopAll();
            m_now += 10000;
            QVERIFY(send(ShowControl::Go, first, list, "2"));
            QVERIFY2(send(ShowControl::Go, second, list), qPrintable(pair + ": held after a Fast cue"));
            QCOMPARE(list->currentStepIndex(), 2);
        }
    // A direct step (a VC button on a plain chaser) is the same rule.
    markFast(list, 1, false);
    stopAll();
    m_now += 10000;
    QVERIFY(send(ShowControl::Go, ShowControl::RouteKey, list));
    m_now += 100;
    QVERIFY(!sc->admitDirect(list->id(), ShowControl::Go, ShowControl::RouteVcCueList));
    m_now += 200;
    QVERIFY(sc->admitDirect(list->id(), ShowControl::Go, ShowControl::RouteVcCueList));
}

void GoGuard_Test::fastIsSavedAndLoaded()
{
    // The chaser step: Fast="1" when on, nothing when off.
    auto stepXml = [](const ChaserStep &step) {
        QByteArray xml;
        QBuffer buffer(&xml);
        buffer.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buffer);
        step.saveXML(&w, 0, false);
        return xml;
    };
    auto stepBack = [this](const QByteArray &xml) {
        QXmlStreamReader r(xml);
        r.readNextStartElement();
        ChaserStep back;
        int number = -1;
        back.loadXML(r, number, m_doc);
        return back;
    };
    ChaserStep step(42);
    QVERIFY(!step.fast);
    QVERIFY(!stepXml(step).contains("Fast="));
    QVERIFY(!stepBack(stepXml(step)).fast);
    step.fast = true;
    QVERIFY(stepXml(step).contains("Fast=\"1\""));
    QVERIFY(stepBack(stepXml(step)).fast);
    QVERIFY(ChaserStep::fromVariant(step.toVariant()).fast);
    ChaserStep copy(step);
    QVERIFY(copy.fast);

    // The Show Tree cue: Fast="1", read back; a cue without it is Off.
    ShowTree::Production p;
    p.id = 1;
    p.name = "P";
    ShowTree::CueList l;
    l.id = 2;
    l.name = "L";
    ShowTree::Cue fast;
    fast.id = 3;
    fast.number = "1";
    fast.fast = true;
    ShowTree::Cue plain;
    plain.id = 4;
    plain.number = "2";
    l.cues << fast << plain;
    p.lists << l;
    QByteArray xml;
    {
        QBuffer buffer(&xml);
        buffer.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buffer);
        ShowTree::saveXML(&w, p);
    }
    QCOMPARE(QString::fromUtf8(xml).count(QStringLiteral("Fast=\"1\"")), 1);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    ShowTree::Production back;
    QVERIFY(ShowTree::loadXML(r, back));
    QCOMPARE(back.lists.count(), 1);
    QCOMPARE(back.lists.first().cues.count(), 2);
    QVERIFY(back.lists.first().cues.at(0).fast);
    QVERIFY(!back.lists.first().cues.at(1).fast);
}

QTEST_MAIN(GoGuard_Test)

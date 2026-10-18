/*
  Q Light Controller Plus
  cuerunner_test.cpp

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
#include <QTreeWidget>
#include <QToolButton>
#include <QComboBox>
#include <QBuffer>
#include <QtTest>

#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private

#include "showtreecontroller.h"
#include "timecodesource.h"
#include "inputoutputmap.h"
#include "virtualconsole.h"
#include "fixturegroup.h"
#include "showcontrol.h"
#include "vccuerunner.h"
#include "cuerunner.h"
#include "qlcpalette.h"
#include "vccuelist.h"
#include "vcframe.h"
#include "fixture.h"
#include "chaser.h"
#include "doc.h"

#include "cuerunner_test.h"

using namespace ShowTree;

namespace
{
Target groupTarget(quint32 g)
{
    Target t;
    t.groups << g;
    return t;
}

QTreeWidgetItem *rowFor(QTreeWidget *tree, const QString &cue)
{
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->text(1) == cue)
            return *it;
    return nullptr;
}

struct ScopedVc
{
    explicit ScopedVc(Doc *doc) { new VirtualConsole(nullptr, doc); }
    ~ScopedVc() { delete VirtualConsole::instance(); }
};
} // namespace

void CueRunner_Test::init()
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

void CueRunner_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    settle(3);
    delete m_doc;
    m_doc = nullptr;
}

QHash<quint32, uchar> CueRunner_Test::settle(int ticks)
{
    for (int i = 0; i < ticks; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
    QHash<quint32, uchar> out;
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    for (int i = 0; i < m_fixtures.count(); ++i)
        out.insert(quint32(i), uchar(ua.first()->preGMValues().at(i)));
    m_doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

QList<quint32> CueRunner_Test::show()
{
    auto dimmer = [this](int level, const QString &name) {
        QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
        p->setName(name);
        p->setValue(level);
        m_doc->addPalette(p);
        return p->id();
    };
    auto group = [this](const QString &name, quint32 fixture) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        m_doc->addFixtureGroup(g);
        g->assignFixture(fixture);
        return g->id();
    };
    const quint32 a = group("A", m_fixtures[0]);
    const quint32 b = group("B", m_fixtures[1]);
    const quint32 half = dimmer(127, "Half");
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance");
    const quint32 song = st->addCueList(prod, "Song");
    const quint32 c1 = st->addCue(song, -1, "One");
    const quint32 c2 = st->addCue(song, 0, "Two");
    const quint32 c3 = st->addCue(song, 1, "Three");
    st->setTargetLook(c1, groupTarget(a), st->lookForPalette(half));
    st->setTargetLook(c2, groupTarget(a), st->lookForPalette(full));
    st->setTargetLook(c2, groupTarget(b), st->lookForPalette(half));
    const quint32 s1 = st->addSubCue(c2, "Hit");
    const quint32 s2 = st->addSubCue(c2, "Down");
    st->setTargetLook(s1, groupTarget(b), st->lookForPalette(full));
    st->setTargetLook(s2, groupTarget(a), st->lookForPalette(half));
    const quint32 corp = st->addProduction("Corporate");
    const quint32 keynote = st->addCueList(corp, "Keynote");
    const quint32 k1 = st->addCue(keynote, -1, "Walk-in");
    st->setTargetLook(k1, groupTarget(b), st->lookForPalette(full));
    st->setCurrentProduction(prod);
    return { song, c1, c2, c3, s1, s2, keynote };
}

/****************************************************************************
 * Tests
 ****************************************************************************/

void CueRunner_Test::theRunnerShowsTheListAndItsSubCues()
{
    const QList<quint32> ids = show();
    CueRunner runner(m_doc, CueRunner::Full);
    QCOMPARE(runner.headerText(), QString("Dance · Song"));
    QTreeWidget *tree = runner.cueTree();
    QVERIFY(tree != nullptr);
    QCOMPARE(tree->topLevelItemCount(), 3);
    QTreeWidgetItem *two = tree->topLevelItem(1);
    QCOMPARE(two->text(1), QString("2"));
    QCOMPARE(two->text(2), QString("Two"));
    QCOMPARE(two->childCount(), 2);
    QCOMPARE(two->child(0)->text(1), QString("2.1"));
    QCOMPARE(two->child(0)->text(3), QString("with GO"));
    QCOMPARE(two->child(1)->text(3), QString("Sub GO"));
    // Stopped: GO plays cue 1.
    QCOMPARE(runner.nowText(), QString("Stopped"));
    QCOMPARE(runner.nextText(), QString("NEXT 1 One: A in → Half"));
    QCOMPARE(tree->topLevelItem(0)->text(0), QString("NEXT"));
    // An edit in the tree shows here.
    QVERIFY(m_doc->showTree()->setSubCueFollow(ids[5], true, 500));
    runner.refresh();
    QCOMPARE(tree->topLevelItem(1)->child(1)->text(3), QString("auto 0.5s"));
    // The compact one: no list, GO and Sub GO only.
    CueRunner compact(m_doc, CueRunner::Compact);
    QVERIFY(compact.cueTree() == nullptr);
    QVERIFY(compact.findChild<QToolButton *>("crGo")->isVisibleTo(&compact));
    QVERIFY(!compact.findChild<QToolButton *>("crBack")->isVisibleTo(&compact));
}

void CueRunner_Test::goSubGoBackAndWhatChanges()
{
    show();
    CueRunner runner(m_doc, CueRunner::Full);
    QToolButton *go = runner.findChild<QToolButton *>("crGo");
    QToolButton *subGo = runner.findChild<QToolButton *>("crSubGo");
    QToolButton *subBack = runner.findChild<QToolButton *>("crSubBack");
    QToolButton *back = runner.findChild<QToolButton *>("crBack");
    QToolButton *pause = runner.findChild<QToolButton *>("crPause");
    QVERIFY(!subGo->isEnabled());

    go->click();
    QHash<quint32, uchar> v = settle();
    runner.refresh();
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(runner.nowText(), QString("▶ 1 One"));
    // GO into a cue with sub-cues plays its first: what changes is 2.1's.
    QCOMPARE(runner.nextText(), QString("NEXT 2 Two: A → Full; B in → Full"));

    go->click();
    v = settle();
    runner.refresh();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[1]), 255);
    QCOMPARE(runner.nowText(), QString("▶ 2 Two · 2.1 Hit"));
    QCOMPARE(runner.subNextText(), QString("SUB NEXT 2.2 Down: A → Half"));
    // From 2.1 (A Full, B Full) to cue 3 (cue 2's own state): only B.
    QCOMPARE(runner.nextText(), QString("NEXT 3 Three: B → Half"));
    QTreeWidget *tree = runner.cueTree();
    QCOMPARE(tree->topLevelItem(0)->text(0), QString());     // done
    QCOMPARE(tree->topLevelItem(1)->text(0), QString("▶"));
    QCOMPARE(tree->topLevelItem(1)->child(0)->text(0), QString("▶"));
    QCOMPARE(tree->topLevelItem(1)->child(1)->text(0), QString("SUB NEXT"));
    QCOMPARE(tree->topLevelItem(2)->text(0), QString("NEXT"));
    QVERIFY(subGo->isEnabled());
    QVERIFY(!subBack->isEnabled());

    subGo->click();
    v = settle();
    runner.refresh();
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(runner.subNextText(), QString("SUB: end of 2"));
    QVERIFY(!subGo->isEnabled());
    QVERIFY(subBack->isEnabled());
    subBack->click();
    v = settle();
    QCOMPARE(int(v[0]), 255);

    // Pause holds; Resume.
    pause->click();
    settle(2);
    runner.refresh();
    QVERIFY(runner.chaser()->isPaused());
    QCOMPARE(pause->text(), QString("Resume"));
    QVERIFY(runner.nowText().endsWith("(paused)"));
    pause->click();
    settle(2);
    runner.refresh();
    QVERIFY(!runner.chaser()->isPaused());

    // Back: the previous main cue.
    back->click();
    v = settle();
    runner.refresh();
    QCOMPARE(runner.chaser()->currentStepIndex(), 0);
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(int(v[1]), 0);
    runner.findChild<QToolButton *>("crStop")->click();
    settle();
    QVERIFY(!runner.chaser()->isRunning());
}

void CueRunner_Test::clickLoadsDoubleClickJumps()
{
    show();
    CueRunner runner(m_doc, CueRunner::Full);
    runner.resize(800, 600);
    runner.show();
    QVERIFY(QTest::qWaitForWindowExposed(&runner));
    QTreeWidget *tree = runner.cueTree();
    Chaser *ch = runner.chaser();

    // Click 2.2: LOAD -- STANDBY, and GO plays it (its cue first).
    QTreeWidgetItem *s22 = rowFor(tree, "2.2");
    QVERIFY(s22 != nullptr);
    emit tree->itemClicked(s22, 1);
    runner.refresh();
    QCOMPARE(m_doc->showControl()->loadedStep(ch->id()), 1);
    QCOMPARE(m_doc->showControl()->loadedSubCue(ch->id()), 1);
    QCOMPARE(rowFor(tree, "2.2")->text(0), QString("▷ STANDBY"));
    QVERIFY(runner.nextText().startsWith("▷ STANDBY 2.2 Down: "));
    runner.go();
    settle();
    QCOMPARE(ch->currentStepIndex(), 1);
    QCOMPARE(ch->currentSubCueIndex(), 1);

    // Double-click cue 3: there now.
    emit tree->itemDoubleClicked(rowFor(tree, "3"), 1);
    settle();
    QCOMPARE(ch->currentStepIndex(), 2);
    // Double-click 2.1: its cue, then it.
    emit tree->itemDoubleClicked(rowFor(tree, "2.1"), 1);
    settle();
    QCOMPARE(ch->currentStepIndex(), 1);
    QCOMPARE(ch->currentSubCueIndex(), 0);
    runner.hide();
}

/* Two runners (the Run tab and a VC widget), the footer's ⌘↩ and QLab
   (ShowControl) are one runner: whatever drives it, all show it. */
void CueRunner_Test::runnersAndShowControlMirrorOneState()
{
    show();
    CueRunner full(m_doc, CueRunner::Full);
    CueRunner compact(m_doc, CueRunner::Compact);
    compact.go();
    settle();
    full.refresh();
    compact.refresh();
    QCOMPARE(full.nowText(), QString("▶ 1 One"));
    QCOMPARE(compact.nowText(), full.nowText());
    // As the footer and ⌘↩ do (no list: the main one).
    QVERIFY(m_doc->showControl()->execute({ ShowControl::Go, QString(), QString() }));
    settle();
    full.refresh();
    compact.refresh();
    QCOMPARE(full.nowText(), QString("▶ 2 Two · 2.1 Hit"));
    QCOMPARE(compact.nowText(), full.nowText());
    // OSC /subgo.
    QByteArray osc("/subgo");
    osc.append('\0');
    while (osc.size() % 4)
        osc.append('\0');
    osc.append(",\0\0\0", 4);
    QCOMPARE(m_doc->showControl()->handleOsc(osc), 1);
    settle();
    full.refresh();
    compact.refresh();
    QCOMPARE(full.nowText(), QString("▶ 2 Two · 2.2 Down"));
    QCOMPARE(compact.nowText(), full.nowText());
}

void CueRunner_Test::aRunnerFollowsOneListOfAnyProduction()
{
    const QList<quint32> ids = show();
    ShowTreeController *st = m_doc->showTree();
    CueRunner runner(m_doc, CueRunner::List);
    QComboBox *follow = runner.findChild<QComboBox *>("crFollow");
    QVERIFY(follow != nullptr);
    QCOMPARE(follow->itemText(0), QString("Current production (Dance)"));
    QCOMPARE(follow->count(), 3);       // current + Song + Keynote
    QSignalSpy changed(&runner, &CueRunner::followChanged);
    const int at = follow->findData(ids[6]);
    follow->setCurrentIndex(at);
    emit follow->activated(at);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(runner.follow(), ids[6]);
    QCOMPARE(runner.headerText(), QString("Corporate · Keynote"));
    runner.go();
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[1]), 255);               // Keynote's cue
    QVERIFY(qobject_cast<Chaser *>(m_doc->function(st->cueList(ids[6])->chaser))->isRunning());
    // The main cue list is still Dance's: ⌘↩ plays Song.
    QVERIFY(!qobject_cast<Chaser *>(m_doc->function(st->cueList(ids[0])->chaser))->isRunning());
    QVERIFY(m_doc->showControl()->execute({ ShowControl::Go, QString(), QString() }));
    settle();
    QVERIFY(qobject_cast<Chaser *>(m_doc->function(st->cueList(ids[0])->chaser))->isRunning());

    // Following the current production, it moves with it.
    runner.setFollow(invalidId());
    QVERIFY(st->setCurrentProduction(st->productions().at(1).id));
    runner.refresh();
    QCOMPARE(runner.headerText(), QString("Corporate · Keynote"));
    QCOMPARE(follow->itemText(0), QString("Current production (Corporate)"));
}

void CueRunner_Test::liveDesignAndTimecode()
{
    show();
    CueRunner runner(m_doc, CueRunner::Full);
    m_doc->setMode(Doc::Operate);
    runner.refresh();
    QCOMPARE(runner.modeText(), QString("LIVE"));
    m_doc->setMode(Doc::Design);
    runner.refresh();
    QCOMPARE(runner.modeText(), QString("DESIGN"));
    QCOMPARE(runner.timecodeText(), QString("TC —"));
    m_doc->timecodeSource()->updateTimeCode(0, 3723500, 30);   // 01:02:03, half a second
    runner.refresh();
    QCOMPARE(runner.timecodeText(), QString("TC 01:02:03:15"));
    // A production with a source of its own: the runner reads what is
    // followed (the source filters, the readout stays).
    QVERIFY(m_doc->showTree()->setProductionTimecode(m_doc->currentProduction(), 2));
    m_doc->timecodeSource()->updateTimeCode(0, 0, 30);         // another universe: ignored
    runner.refresh();
    QCOMPARE(runner.timecodeText(), QString("TC 01:02:03:15"));
    m_doc->setMode(Doc::Design);
}

void CueRunner_Test::spaceIsGoInTheRunTab()
{
    show();
    CueRunner runner(m_doc, CueRunner::Full);
    runner.resize(800, 600);
    runner.show();
    QVERIFY(QTest::qWaitForWindowExposed(&runner));
    runner.activateWindow();
    runner.cueTree()->setFocus();
    QTest::keyClick(runner.cueTree(), Qt::Key_Space);
    settle();
    QVERIFY(runner.chaser()->isRunning());
    QCOMPARE(runner.chaser()->currentStepIndex(), 0);
    runner.hide();
}

void CueRunner_Test::theVcWidgetSavesLoadsAndCopies()
{
    const QList<quint32> ids = show();
    ScopedVc vc(m_doc);
    VCFrame *contents = VirtualConsole::instance()->contents();
    VCCueRunner *w = new VCCueRunner(contents, m_doc);
    VirtualConsole::instance()->placeWidget(w, contents);
    QCOMPARE(w->type(), int(VCWidget::CueRunnerWidget));
    QCOMPARE(w->runnerSize(), CueRunner::Compact);
    w->setRunnerSize(CueRunner::List);
    w->setFollow(ids[6]);
    w->setCaption("Keynote runner");

    QByteArray xml;
    {
        QBuffer buf(&xml);
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter wr(&buf);
        QVERIFY(w->saveXML(&wr));
    }
    QVERIFY(xml.contains("<CueRunner"));
    QVERIFY(xml.contains(QString("Follow=\"%1\"").arg(ids[6]).toUtf8()));
    QVERIFY(xml.contains("Size=\"List\""));

    VCCueRunner *loaded = new VCCueRunner(contents, m_doc);
    QBuffer in(&xml);
    in.open(QIODevice::ReadOnly);
    QXmlStreamReader r(&in);
    r.readNextStartElement();
    QVERIFY(loaded->loadXML(r));
    QCOMPARE(loaded->follow(), ids[6]);
    QCOMPARE(loaded->runnerSize(), CueRunner::List);
    QCOMPARE(loaded->caption(), QString("Keynote runner"));

    VCWidget *copy = w->createCopy(contents);
    VCCueRunner *rc = qobject_cast<VCCueRunner *>(copy);
    QVERIFY(rc != nullptr);
    QCOMPARE(rc->follow(), ids[6]);
    QCOMPARE(rc->runnerSize(), CueRunner::List);
    // Full is the Run tab's: a widget is at most List.
    rc->setRunnerSize(CueRunner::Full);
    QCOMPARE(rc->runnerSize(), CueRunner::List);
    delete copy;
    delete loaded;
}

/* Swap widget: a VC Cue List playing a tree list becomes a Cue Runner on it
   in place, and back; a plain chaser's Cue List stays (nothing to swap). */
void CueRunner_Test::swapWidgetOnAPage()
{
    const QList<quint32> ids = show();
    ShowTreeController *st = m_doc->showTree();
    ScopedVc vc(m_doc);
    VirtualConsole *console = VirtualConsole::instance();
    VCFrame *contents = console->contents();

    VCCueList *cl = new VCCueList(contents, m_doc);
    console->placeWidget(cl, contents);
    cl->setChaser(st->cueList(ids[6])->chaser);
    cl->setGeometry(20, 30, 260, 180);
    cl->setCaption("Keynote");

    VCWidget *made = console->swapWidget(cl);
    VCCueRunner *cr = qobject_cast<VCCueRunner *>(made);
    QVERIFY(cr != nullptr);
    QCOMPARE(cr->geometry(), QRect(20, 30, 260, 180));
    QCOMPARE(cr->caption(), QString("Keynote"));
    QCOMPARE(cr->follow(), ids[6]);
    QCOMPARE(cr->runnerSize(), CueRunner::List);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(contents->findChildren<VCCueList *>().isEmpty());

    // The current production's main list: the runner follows "current".
    VCCueList *main = new VCCueList(contents, m_doc);
    console->placeWidget(main, contents);
    main->setChaser(st->cueList(ids[0])->chaser);
    VCCueRunner *cur = qobject_cast<VCCueRunner *>(console->swapWidget(main));
    QVERIFY(cur != nullptr);
    QCOMPARE(cur->follow(), invalidId());

    // And back: a Cue List of the runner's chaser.
    VCCueList *again = qobject_cast<VCCueList *>(console->swapWidget(cr));
    QVERIFY(again != nullptr);
    QCOMPARE(again->chaserID(), st->cueList(ids[6])->chaser);
    QCOMPARE(again->geometry(), QRect(20, 30, 260, 180));

    // A plain chaser's Cue List: not swapped.
    Chaser *plain = new Chaser(m_doc);
    m_doc->addFunction(plain);
    VCCueList *pl = new VCCueList(contents, m_doc);
    console->placeWidget(pl, contents);
    pl->setChaser(plain->id());
    QVERIFY(console->swapWidget(pl) == nullptr);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

QTEST_MAIN(CueRunner_Test)

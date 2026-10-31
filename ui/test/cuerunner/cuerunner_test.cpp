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
#include <QPushButton>
#include <QSignalSpy>
#include <QAction>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QTabWidget>
#include <QBuffer>
#include <QFile>
#include <QDir>
#include <QScopeGuard>
#include <QHeaderView>
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
#include "vcbutton.h"
#include "vcframe.h"
#include "fixturegroup.h"
#include "showcontrol.h"
#include "vccuerunnerproperties.h"
#include "inputselectionwidget.h"
#include "qlcinputsource.h"
#include "vccuerunner.h"
#include "cuerunner.h"
#include "padrun.h"
#include "programmercontroller.h"
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

QByteArray saved(VCWidget *w)
{
    QByteArray xml;
    QBuffer buf(&xml);
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter wr(&buf);
    w->saveXML(&wr);
    return xml;
}

bool loadInto(VCWidget *w, QByteArray xml)
{
    QBuffer in(&xml);
    in.open(QIODevice::ReadOnly);
    QXmlStreamReader r(&in);
    r.readNextStartElement();
    return w->loadXML(r);
}

/* A drag of @a by on @a handle, as the mouse does it. */
void drag(QWidget *handle, const QPoint &by)
{
    const QPoint at = handle->rect().center();
    const QPoint g = handle->mapToGlobal(at);
    QMouseEvent press(QEvent::MouseButtonPress, at, g, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(handle, &press);
    QMouseEvent move(QEvent::MouseMove, at + by, g + by, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(handle, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, at + by, g + by, Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(handle, &release);
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

/* Q157 (4): while running, the Run tab names the VC buttons whose hit for
   this moment fires instead of a wider one. */
void CueRunner_Test::theRunTabShowsHitsThatOverride()
{
    const QList<quint32> ids = show();
    ScopedVc vc(m_doc);
    ShowTreeController *st = m_doc->showTree();
    quint32 prod = 0;
    st->cueList(ids[0], &prod);
    const quint32 blackout = st->addHit(ShowTree::HitScope::Global, prod, "Blackout");
    const quint32 strobe = st->addHit(ShowTree::HitScope::Cue, ids[1], "Strobe");
    VCButton *b = new VCButton(VirtualConsole::instance()->contents(), m_doc);
    b->setCaption("HIT 1");
    b->setHits({ blackout, strobe });
    CueRunner runner(m_doc, CueRunner::Full);
    QLabel *hits = runner.findChild<QLabel *>("crHitsNow");
    QVERIFY(hits != nullptr);
    QVERIFY(!hits->isVisibleTo(&runner));
    runner.go();
    settle();
    QCoreApplication::processEvents();
    QCOMPARE(st->runningCue(ids[0]), ids[1]);
    QVERIFY(hits->isVisibleTo(&runner));
    QCOMPARE(hits->text(), QString("HIT 1: Strobe \u21b3 overrides Blackout"));
    runner.go();
    settle();
    QCoreApplication::processEvents();
    QVERIFY(!hits->isVisibleTo(&runner));
    runner.stop();
    settle();
    delete b;
}

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
    // The compact one: no list; GO and Sub GO, Back and Stop small under
    // them (Sub Back and Pause: the list size's).
    CueRunner compact(m_doc, CueRunner::Compact);
    QVERIFY(compact.cueTree() == nullptr);
    QVERIFY(compact.findChild<QToolButton *>("crGo")->isVisibleTo(&compact));
    QVERIFY(compact.findChild<QToolButton *>("crBack")->isVisibleTo(&compact));
    QVERIFY(compact.findChild<QToolButton *>("crStop")->isVisibleTo(&compact));
    QVERIFY(!compact.findChild<QToolButton *>("crSubBack")->isVisibleTo(&compact));
    QVERIFY(!compact.findChild<QToolButton *>("crPause")->isVisibleTo(&compact));
    compact.setShowSecondary(false);
    QVERIFY(!compact.findChild<QToolButton *>("crBack")->isVisibleTo(&compact));
}

/* Q62 (UX review problem 4): a 4.x show has no main cue list. The Run tab
   says why in a line, offers Upgrade This Show… (the File menu's action),
   and GO and the rest are disabled -- grey, not a live-looking green. With
   a main list, Run is as it was. */
void CueRunner_Test::noMainListGreysGoAndOffersUpgrade()
{
    QAction upgrade(QStringLiteral("Upgrade This Show…"), nullptr);
    QSignalSpy upgraded(&upgrade, &QAction::triggered);
    CueRunner::setUpgradeAction(&upgrade);
    {
        CueRunner runner(m_doc, CueRunner::Full);
        runner.resize(900, 600);
        runner.show();
        runner.refresh();
        QCOMPARE(runner.headerText(), QString("No main cue list"));
        QVERIFY(runner.isEmptyStateShown());
        QLabel *why = runner.findChild<QLabel *>("crEmptyText");
        QVERIFY(why != nullptr && why->isVisible());
        QCOMPARE(why->text(), QString("This show has no cue list yet. Upgrade turns its chasers into one."));
        QVERIFY(!runner.cueTree()->isVisible());
        for (const char *name : { "crGo", "crSubGo", "crBack", "crSubBack", "crPause", "crStop" })
            QVERIFY2(!runner.findChild<QToolButton *>(name)->isEnabled(), name);
        QVERIFY(!runner.canGo());
        // Disabled reads grey: the green is GO's enabled look only.
        QToolButton *go = runner.goButton();
        QVERIFY(go->styleSheet().contains("QToolButton:disabled"));
        const QImage shot = go->grab().toImage();
        const QColor mid = shot.pixelColor(4, shot.height() / 2);
        QVERIFY2(!(mid.green() > mid.red() + 40 && mid.green() > mid.blue() + 40),
                 qPrintable(mid.name()));
        // Space does nothing either.
        runner.go();
        QVERIFY(!runner.canGo());
        // The button runs the File menu's action.
        QPushButton *button = runner.upgradeButton();
        QVERIFY(button != nullptr && button->isVisible());
        QCOMPARE(button->text(), QString("Upgrade This Show…"));
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(upgraded.count(), 1);
    }
    // No action (no App): no button, the line stays.
    CueRunner::setUpgradeAction(nullptr);
    {
        CueRunner runner(m_doc, CueRunner::Full);
        runner.show();
        runner.refresh();
        QVERIFY(runner.isEmptyStateShown());
        QVERIFY(!runner.upgradeButton()->isVisible());
    }
    // With a main list: as before -- the list, GO green and live.
    CueRunner::setUpgradeAction(&upgrade);
    show();
    CueRunner runner(m_doc, CueRunner::Full);
    runner.resize(900, 600);
    runner.show();
    runner.refresh();
    QVERIFY(!runner.isEmptyStateShown());
    QVERIFY(runner.cueTree()->isVisible());
    QVERIFY(runner.goButton()->isEnabled());
    const QImage shot = runner.goButton()->grab().toImage();
    const QColor green = shot.pixelColor(4, shot.height() / 2);
    QVERIFY2(green.green() > green.red() + 40, qPrintable(green.name()));
    CueRunner::setUpgradeAction(nullptr);
}

void CueRunner_Test::thePadRoleShowsOnTheRunTab()
{
    CueRunner runner(m_doc, CueRunner::Full);
    QToolButton *pad = runner.findChild<QToolButton *>("crPadRun");
    QVERIFY(pad != nullptr);
    PadRun *run = m_doc->programmer()->padRun();
    QVERIFY(!run->isEnabled());
    QCOMPARE(pad->text(), QString("Pad: Tools"));
    QVERIFY(!pad->isChecked());

    pad->click();
    QVERIFY(run->isEnabled());
    QCOMPARE(pad->text(), QString("Pad: Run"));
    QVERIFY2(pad->toolTip().contains("A: GO (LB held)"), qPrintable(pad->toolTip()));

    // Armed from the pad: the button says so.
    run->handle(0, GamepadRole::LB, 255);
    QCOMPARE(pad->text(), QString("Pad: ARMED"));
    run->handle(0, GamepadRole::LB, 0);
    QCOMPARE(pad->text(), QString("Pad: Run"));

    // Switched elsewhere (the App restoring it): the button follows.
    run->setEnabled(false, false);
    QVERIFY(!pad->isChecked());
    QCOMPARE(pad->text(), QString("Pad: Tools"));
    // A compact runner (the VC's) has none.
    CueRunner compact(m_doc, CueRunner::Compact);
    QVERIFY(compact.findChild<QToolButton *>("crPadRun") == nullptr);
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
    // The Mac's 13 pt, as the course pictures are taken (offscreen's own is
    // smaller, and "▷ STANDBY" fitted the default column there anyway).
    const QFont appFont = QApplication::font();
    auto restoreFont = qScopeGuard([appFont]() { QApplication::setFont(appFont); });
    QFont mac = appFont;
    mac.setPointSize(13);
    QApplication::setFont(mac);
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
    // Q115: all of it, a sub-cue's indent and bold included -- not "▷ STA…".
    {
        const QFontMetrics fm(s22->font(0));
        const int room = tree->header()->sectionSize(0) - 2 * tree->indentation();
        const int need = fm.horizontalAdvance(s22->text(0)) + 6;
        QVERIFY2(room >= need, qPrintable(QString("%1 < %2").arg(room).arg(need)));
    }
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

/* LIVE_BLIND.md: in Design a cue clicked in the Run is what the stage
   shows (and on standby: GO plays the same); in Operate a click only loads
   -- only the run is live. Leaving the Run lets go of its pick. */
void CueRunner_Test::aClickInDesignShowsTheCue()
{
    const QList<quint32> ids = show();
    ShowTreeController *st = m_doc->showTree();
    CueRunner runner(m_doc, CueRunner::Full);
    runner.resize(800, 600);
    runner.show();
    QVERIFY(QTest::qWaitForWindowExposed(&runner));
    QTreeWidget *tree = runner.cueTree();
    Chaser *ch = runner.chaser();

    emit tree->itemClicked(rowFor(tree, "2"), 1);
    QCOMPARE(st->previewedCue(), ids[2]);
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[1]), 127);
    QVERIFY(!ch->isRunning());
    QCOMPARE(m_doc->showControl()->loadedStep(ch->id()), 1);
    // GO: the cue, now the run's (its first sub-cue, Hit, plays with it).
    runner.go();
    settle(2);
    QCoreApplication::processEvents();
    v = settle();
    QCOMPARE(ch->currentStepIndex(), 1);
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[1]), 255);
    // Another click: that cue, over the run.
    emit tree->itemClicked(rowFor(tree, "1"), 1);
    v = settle();
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(int(v[1]), 255);           // the run's cue Two underneath
    // Leaving the Run: the run alone.
    runner.hide();
    v = settle();
    QCOMPARE(int(v[0]), 255);

    // Operate: a click only loads.
    runner.show();
    m_doc->setMode(Doc::Operate);
    emit tree->itemClicked(rowFor(tree, "3"), 1);
    v = settle();
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[1]), 255);
    QCOMPARE(m_doc->showControl()->loadedStep(ch->id()), 2);
    m_doc->setMode(Doc::Design);
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
    QVERIFY(!follow->isHidden());       // two to choose from
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

/* COURSE_PLAN issue 9: a show opened after another whose production had
   the same id but another name. The box names the new one (it is refilled
   on every tree change, a load included), not the last show's. */
void CueRunner_Test::theBoxNamesTheOpenedShowsProduction()
{
    ShowTreeController *st = m_doc->showTree();
    st->setCurrentProduction(st->addProduction("Spring Show"));
    QByteArray xml;
    {
        QBuffer buf(&xml);
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        w.writeStartDocument();
        st->saveXML(&w);
        w.writeEndDocument();
    }
    st->reset();
    const quint32 qualities = st->addProduction("Qualities");
    st->setCurrentProduction(qualities);
    CueRunner runner(m_doc, CueRunner::Full);
    QComboBox *follow = runner.findChild<QComboBox *>("crFollow");
    QVERIFY(follow != nullptr);
    QCOMPARE(follow->itemText(0), QString("Current production (Qualities)"));

    QXmlStreamReader r(xml);
    QVERIFY(r.readNextStartElement());
    QVERIFY(st->loadXML(r));
    QCOMPARE(st->currentProduction(), qualities);       // the same id
    QCOMPARE(follow->itemText(0), QString("Current production (Spring Show)"));
}

/* Q75: one production with one list: GO plays it, nothing to choose -- no
   Current production drop-down. A second production brings it back. */
void CueRunner_Test::oneListHasNothingToChoose()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Spring Show");
    const quint32 list = st->addCueList(prod, "01-Everybody Scream");
    st->addCue(list, -1, "Pose");
    st->setCurrentProduction(prod);
    CueRunner runner(m_doc, CueRunner::Full);
    QComboBox *follow = runner.findChild<QComboBox *>("crFollow");
    QVERIFY(follow != nullptr);
    QVERIFY(follow->isHidden());
    const quint32 corp = st->addProduction("Corporate");
    st->addCueList(corp, "Keynote");
    QVERIFY(!follow->isHidden());
    QCOMPARE(follow->count(), 3);
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
    // No source: no readout, as the footer's MTC chip (Q98).
    QLabel *tcLabel = runner.findChild<QLabel *>("crTimecode");
    QVERIFY(tcLabel != nullptr && tcLabel->isHidden());
    m_doc->timecodeSource()->updateTimeCode(0, 3723500, 30);   // 01:02:03, half a second
    runner.refresh();
    QCOMPARE(runner.timecodeText(), QString("TC 01:02:03:15"));
    QVERIFY(!tcLabel->isHidden());
    // A production with a source of its own: the runner reads what is
    // followed (the source filters, the readout stays).
    QVERIFY(m_doc->showTree()->setProductionTimecode(m_doc->currentProduction(), 2));
    m_doc->timecodeSource()->updateTimeCode(0, 0, 30);         // another universe: ignored
    runner.refresh();
    QCOMPARE(runner.timecodeText(), QString("TC 01:02:03:15"));
    m_doc->setMode(Doc::Design);
}

void CueRunner_Test::theNextCueAtATimecodeAndItsCountdown()
{
    show();
    CueRunner runner(m_doc, CueRunner::Full);
    ShowTreeController *st = m_doc->showTree();
    quint32 lid = ShowTree::invalidId();
    QVERIFY(runner.chaser() != nullptr);
    st->productionOfChaser(runner.chaser()->id(), &lid);
    const ShowTree::CueList *l = st->cueList(lid);
    QVERIFY(l != nullptr && l->cues.size() >= 2);
    QLabel *next = runner.findChild<QLabel *>("crTimecodeNext");
    QVERIFY(next != nullptr);
    m_doc->timecodeSource()->updateTimeCode(0, 4000, 30);
    runner.refresh();
    QVERIFY(next->isHidden());                          // the list doesn't follow timecode
    QVERIFY(st->setListFollowTimecode(lid, true));
    QVERIFY(st->setCueTimecode(st->cueList(lid)->cues[1].id, 10000));
    runner.refresh();
    QVERIFY(!next->isHidden());
    QVERIFY2(next->text().contains("00:00:10:00") && next->text().contains("in 6 s"), qPrintable(next->text()));
    m_doc->timecodeSource()->updateTimeCode(0, 9500, 30);
    runner.refresh();
    QVERIFY2(next->text().contains("in 1 s"), qPrintable(next->text()));
    m_doc->timecodeSource()->updateTimeCode(0, 11000, 30);   // passed: nothing left
    runner.refresh();
    QVERIFY(next->isHidden());
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

/* Every action fires from a mapped input source and from its key -- in
   Operate only, as any VC widget's. */
void CueRunner_Test::vcActionsFromInputsAndKeys()
{
    show();
    ScopedVc vc(m_doc);
    VCFrame *contents = VirtualConsole::instance()->contents();
    VCCueRunner *w = new VCCueRunner(contents, m_doc);
    VirtualConsole::instance()->placeWidget(w, contents);
    using A = VCCueRunner;
    for (int a = 0; a < A::ActionCount; ++a)
    {
        w->setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(0, quint32(10 + a))), quint8(a));
        w->setKeySequence(A::Action(a), QKeySequence(QString("Shift+F%1").arg(a + 1)));
    }
    InputOutputMap *io = m_doc->inputOutputMap();
    auto press = [&](A::Action a) {
        emit io->inputValueChanged(0, quint32(10 + a), 255);
        emit io->inputValueChanged(0, quint32(10 + a), 0);
        settle();
        w->runner()->refresh();
    };
    auto key = [&](A::Action a) {
        emit VirtualConsole::instance()->keyPressed(w->keySequence(a));
        settle();
        w->runner()->refresh();
    };
    Chaser *ch = w->runner()->chaser();
    ShowControl *sc = m_doc->showControl();

    // Design mode: nothing fires.
    press(A::Go);
    key(A::Go);
    QVERIFY(!ch->isRunning());

    m_doc->setMode(Doc::Operate);
    press(A::Go);
    QVERIFY(ch->isRunning());
    QCOMPARE(ch->currentStepIndex(), 0);
    key(A::Go);
    QCOMPARE(ch->currentStepIndex(), 1);
    QCOMPARE(ch->currentSubCueIndex(), 0);
    press(A::SubGo);
    QCOMPARE(ch->currentSubCueIndex(), 1);
    key(A::SubBack);
    QCOMPARE(ch->currentSubCueIndex(), 0);
    key(A::SubGo);
    QCOMPARE(ch->currentSubCueIndex(), 1);
    press(A::SubBack);
    QCOMPARE(ch->currentSubCueIndex(), 0);
    key(A::Pause);
    QVERIFY(ch->isPaused());
    press(A::Pause);
    QVERIFY(!ch->isPaused());
    press(A::Back);
    QCOMPARE(ch->currentStepIndex(), 0);
    key(A::Go);
    QCOMPARE(ch->currentStepIndex(), 1);
    key(A::Back);
    QCOMPARE(ch->currentStepIndex(), 0);

    // LOAD next / previous move the standby, main cues only.
    key(A::LoadNext);
    QCOMPARE(sc->loadedStep(ch->id()), 2);
    press(A::LoadPrevious);
    QCOMPARE(sc->loadedStep(ch->id()), 1);
    key(A::LoadPrevious);
    QCOMPARE(sc->loadedStep(ch->id()), 0);
    press(A::LoadNext);
    QCOMPARE(sc->loadedStep(ch->id()), 1);

    key(A::Stop);
    QVERIFY(!ch->isRunning());
    press(A::Go);
    QVERIFY(ch->isRunning());
    press(A::Stop);
    QVERIFY(!ch->isRunning());

    // A fader held up fires once, not on every value.
    emit io->inputValueChanged(0, 10, 200);
    emit io->inputValueChanged(0, 10, 220);
    settle();
    QCOMPARE(ch->currentStepIndex(), 0);
    emit io->inputValueChanged(0, 10, 0);
    m_doc->setMode(Doc::Design);
}

/* Feedback: a mapped control is lit while its action can do something,
   and follows the cue as it changes. */
void CueRunner_Test::vcFeedbackFollowsTheCue()
{
    show();
    ScopedVc vc(m_doc);
    VCFrame *contents = VirtualConsole::instance()->contents();
    VCCueRunner *w = new VCCueRunner(contents, m_doc);
    VirtualConsole::instance()->placeWidget(w, contents);
    using A = VCCueRunner;
    for (A::Action a : { A::Go, A::SubGo, A::Pause, A::Stop, A::LoadNext })
    {
        QSharedPointer<QLCInputSource> src(new QLCInputSource(0, quint32(20 + a)));
        src->setFeedbackValue(QLCInputFeedback::UpperValue, 200);
        src->setFeedbackValue(QLCInputFeedback::LowerValue, 5);
        w->setInputSource(src, quint8(a));
    }
    // Design: nothing is sent.
    w->updateFeedback();
    QCOMPARE(w->lastFeedback(A::Go), -1);

    m_doc->setMode(Doc::Operate);
    QCOMPARE(w->lastFeedback(A::Go), 200);          // cue 1 to GO to
    QCOMPARE(w->lastFeedback(A::SubGo), 5);
    QCOMPARE(w->lastFeedback(A::Stop), 5);
    QCOMPARE(w->lastFeedback(A::LoadNext), 200);
    QCOMPARE(w->lastFeedback(A::Back), -1);         // not mapped

    w->runner()->go();
    settle();
    w->runner()->refresh();
    QCOMPARE(w->lastFeedback(A::Stop), 200);
    QCOMPARE(w->lastFeedback(A::SubGo), 5);
    w->runner()->go();                              // into cue 2: 2.1, 2.2 waits
    settle();
    w->runner()->refresh();
    QCOMPARE(w->lastFeedback(A::SubGo), 200);
    QCOMPARE(w->lastFeedback(A::LoadNext), 5);      // cue 3 is the last
    w->runner()->subGo();
    settle();
    w->runner()->refresh();
    QCOMPARE(w->lastFeedback(A::SubGo), 5);
    w->runner()->pauseResume();
    settle(2);
    w->runner()->refresh();
    QCOMPARE(w->lastFeedback(A::Pause), 200);
    w->runner()->pauseResume();
    settle(2);
    w->runner()->refresh();
    QCOMPARE(w->lastFeedback(A::Pause), 5);
    w->runner()->stop();
    settle();
    w->runner()->refresh();
    QCOMPARE(w->lastFeedback(A::Stop), 5);
    QCOMPARE(w->lastFeedback(A::Go), 200);
    m_doc->setMode(Doc::Design);
}

/* Bigger: the info grows (a larger cue name, more of what's coming), the
   buttons keep their height. Smaller: down to a minimum, buttons usable. */
void CueRunner_Test::vcResizeGrowsTheInfo()
{
    show();
    ScopedVc vc(m_doc);
    VirtualConsole *console = VirtualConsole::instance();
    VCFrame *contents = console->contents();
    VCCueRunner *w = new VCCueRunner(contents, m_doc);
    console->placeWidget(w, contents);
    w->move(QPoint(0, 0));
    console->resize(900, 700);
    console->show();
    QVERIFY(QTest::qWaitForWindowExposed(console));
    m_doc->setMode(Doc::Operate);
    CueRunner *r = w->runner();
    QWidget *info = r->infoArea();
    QToolButton *go = r->goButton();
    QVERIFY(info != nullptr);

    w->resize(QSize(300, 160));
    QTest::qWait(20);
    const int goSmall = go->height();
    const int infoSmall = info->height();
    const int nowSmall = r->findChild<QLabel *>("crNow")->font().pixelSize();
    QVERIFY(goSmall >= CueRunner::kMinButtonHeight);
    QVERIFY(goSmall <= CueRunner::kMaxButtonHeight);

    w->resize(QSize(600, 440));
    QTest::qWait(20);
    QCOMPARE(go->height(), goSmall);                // the buttons stay
    QVERIFY(info->height() > infoSmall + 200);      // the info takes it
    QVERIFY(r->findChild<QLabel *>("crNow")->font().pixelSize() > nowSmall);
    // Stopped: NEXT is cue 1, then 2 and 3.
    QCOMPARE(r->upcomingTexts(), QStringList({ "then 2 Two", "then 3 Three" }));
    r->setUpcomingCount(1);
    QCOMPARE(r->upcomingTexts(), QStringList({ "then 2 Two" }));

    // Down to its minimum: no smaller, the buttons still there.
    w->resize(QSize(40, 40));
    QTest::qWait(20);
    QCOMPARE(w->size(), w->minimumSize());
    QVERIFY(w->minimumSize().width() >= 160);
    QVERIFY(go->height() >= 24);
    QVERIFY(info->height() > 0);
    m_doc->setMode(Doc::Design);
    console->hide();
}

/* The handles: dragged in Design (and saved), hidden and inert in Operate. */
void CueRunner_Test::vcHandlesSizeAndPersist()
{
    show();
    ScopedVc vc(m_doc);
    VirtualConsole *console = VirtualConsole::instance();
    VCFrame *contents = console->contents();
    VCCueRunner *w = new VCCueRunner(contents, m_doc);
    console->placeWidget(w, contents);
    w->move(QPoint(0, 0));
    w->resize(QSize(400, 300));
    console->resize(900, 700);
    console->show();
    QVERIFY(QTest::qWaitForWindowExposed(console));
    CueRunner *r = w->runner();
    QWidget *infoHandle = r->infoHandle();
    QWidget *goHandle = r->goHandle();

    // Design: shown, and the buttons let the mouse through to the widget.
    QVERIFY(infoHandle->isVisible());
    QVERIFY(goHandle->isVisible());
    QVERIFY(r->goButton()->testAttribute(Qt::WA_TransparentForMouseEvents));
    const int height = r->goButton()->height();
    const int share = r->goShare();
    m_doc->resetModified();
    drag(infoHandle, QPoint(0, -30));               // up: taller buttons
    QTest::qWait(20);
    QCOMPARE(r->buttonHeight(), height + 30);
    QCOMPARE(r->goButton()->height(), height + 30);
    drag(goHandle, QPoint(-40, 0));                 // left: GO narrower
    QVERIFY(r->goShare() < share);
    QVERIFY(m_doc->isModified());
    // Clamped.
    drag(infoHandle, QPoint(0, -2000));
    QCOMPARE(r->buttonHeight(), int(CueRunner::kMaxButtonHeight));
    r->setButtonHeight(height + 30);

    VCCueRunner *loaded = new VCCueRunner(contents, m_doc);
    QVERIFY(loadInto(loaded, saved(w)));
    QCOMPARE(loaded->runner()->buttonHeight(), height + 30);
    QCOMPARE(loaded->runner()->goShare(), r->goShare());
    VCCueRunner *copy = qobject_cast<VCCueRunner *>(w->createCopy(contents));
    QCOMPARE(copy->runner()->buttonHeight(), height + 30);
    QCOMPARE(copy->runner()->goShare(), r->goShare());
    delete copy;
    delete loaded;

    // Operate: hidden, and a drag does nothing; the buttons are live.
    m_doc->setMode(Doc::Operate);
    QVERIFY(!infoHandle->isVisible());
    QVERIFY(!goHandle->isVisible());
    QVERIFY(!r->goButton()->testAttribute(Qt::WA_TransparentForMouseEvents));
    const int before = r->buttonHeight();
    const int shareBefore = r->goShare();
    drag(infoHandle, QPoint(0, -30));
    drag(goHandle, QPoint(30, 0));
    QCOMPARE(r->buttonHeight(), before);
    QCOMPARE(r->goShare(), shareBefore);
    m_doc->setMode(Doc::Design);
    QVERIFY(infoHandle->isVisible());
    console->hide();
}

/* The properties: what it shows, each action's key and input -- through the
   dialog, saved and loaded, copied. */
void CueRunner_Test::vcPropertiesRoundTrip()
{
    show();
    ScopedVc vc(m_doc);
    VCFrame *contents = VirtualConsole::instance()->contents();
    VCCueRunner *w = new VCCueRunner(contents, m_doc);
    VirtualConsole::instance()->placeWidget(w, contents);
    CueRunner *r = w->runner();
    QCOMPARE(r->upcomingCount(), 2);
    QVERIFY(r->showTimecode() && r->showSecondary() && r->showSubGo());

    {
        VCCueRunnerProperties prop(w, m_doc);
        QCOMPARE(prop.tabs()->count(), 2);
        prop.findChild<QSpinBox *>("crPropUpcoming")->setValue(4);
        prop.findChild<QCheckBox *>("crPropTimecode")->setChecked(false);
        prop.findChild<QCheckBox *>("crPropBackStop")->setChecked(false);
        prop.findChild<QCheckBox *>("crPropSubGo")->setChecked(false);
        prop.inputWidget(VCCueRunner::Go)->setKeySequence(QKeySequence("G"));
        prop.inputWidget(VCCueRunner::Go)->setInputSource(
            QSharedPointer<QLCInputSource>(new QLCInputSource(1, 42)));
        prop.inputWidget(VCCueRunner::LoadPrevious)->setKeySequence(QKeySequence("Shift+P"));
        prop.accept();
    }
    QCOMPARE(r->upcomingCount(), 4);
    QVERIFY(!r->showTimecode());
    QVERIFY(!r->showSecondary());
    QVERIFY(!r->showSubGo());
    QVERIFY(!r->subGoButton()->isVisibleTo(r));
    QVERIFY(!r->findChild<QToolButton *>("crBack")->isVisibleTo(r));
    QCOMPARE(w->keySequence(VCCueRunner::Go), QKeySequence("G"));
    QCOMPARE(w->inputSource(VCCueRunner::Go)->channel(), quint32(42));
    QCOMPARE(w->keySequence(VCCueRunner::LoadPrevious), QKeySequence("Shift+P"));

    const QByteArray xml = saved(w);
    QVERIFY(xml.contains("Upcoming=\"4\""));
    QVERIFY(xml.contains("<Go>"));
    QVERIFY(xml.contains("<LoadPrevious>"));
    QVERIFY(!xml.contains("<Stop>"));               // nothing mapped: not written
    VCCueRunner *loaded = new VCCueRunner(contents, m_doc);
    QVERIFY(loadInto(loaded, xml));
    CueRunner *lr = loaded->runner();
    QCOMPARE(lr->upcomingCount(), 4);
    QVERIFY(!lr->showTimecode());
    QVERIFY(!lr->showSecondary());
    QVERIFY(!lr->showSubGo());
    QCOMPARE(loaded->keySequence(VCCueRunner::Go), QKeySequence("G"));
    QCOMPARE(loaded->inputSource(VCCueRunner::Go)->universe(), quint32(1));
    QCOMPARE(loaded->inputSource(VCCueRunner::Go)->channel(), quint32(42));
    QCOMPARE(loaded->keySequence(VCCueRunner::LoadPrevious), QKeySequence("Shift+P"));

    VCCueRunner *copy = qobject_cast<VCCueRunner *>(w->createCopy(contents));
    QCOMPARE(copy->runner()->upcomingCount(), 4);
    QVERIFY(!copy->runner()->showSubGo());
    QCOMPARE(copy->keySequence(VCCueRunner::Go), QKeySequence("G"));
    QCOMPARE(copy->inputSource(VCCueRunner::Go)->channel(), quint32(42));
    delete copy;

    // An older workspace (none of these): the defaults.
    VCCueRunner *old = new VCCueRunner(contents, m_doc);
    QVERIFY(loadInto(old, "<CueRunner Size=\"Compact\"/>"));
    QCOMPARE(old->runner()->upcomingCount(), 2);
    QVERIFY(old->runner()->showSubGo());
    QCOMPARE(old->runner()->buttonHeight(), 64);
    QVERIFY(old->keySequence(VCCueRunner::Go).isEmpty());
    delete old;
    delete loaded;
}

/* Pictures of the VC widget on a real show, small and large, Design and
   Operate -- only when asked (QLC_CUERUNNER_SHOTS = the folder,
   QLC_CUERUNNER_WS = a copy of a workspace, QLC_CUERUNNER_SHOTS_TAG = a
   prefix). Offscreen, no output plugins: nothing is sent. */
/* A cue's text colour (its own, else its list's; a sub-cue its cue's) in
   the Run tab's list, on NOW / NEXT / SUB NEXT, and on the VC Cue Runner's
   upcoming lines -- made readable on the theme; done rows stay grey. */
void CueRunner_Test::textColoursOnEveryLine()
{
    const QList<quint32> ids = show();       // song, c1, c2, c3, s1, s2, keynote
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setListColour(ids[0], "Blue"));
    QVERIFY(st->setCuesColour({ ids[1] }, "Red"));
    QVERIFY(st->setCuesColour({ ids[4] }, "Green"));

    CueRunner runner(m_doc, CueRunner::Full);
    runner.refresh();
    QTreeWidget *tree = runner.cueTree();
    const QPalette tp = tree->palette();
    auto onTree = [&tp](const char *hex) {
        return readableOn(readableOn(QColor(hex), tp.color(QPalette::Base)), tp.color(QPalette::AlternateBase));
    };
    const QPalette rp = runner.palette();
    auto onRunner = [&rp](const char *hex) { return readableOn(QColor(hex), rp.color(QPalette::Window)); };
    QTreeWidgetItem *one = tree->topLevelItem(0);
    QTreeWidgetItem *two = tree->topLevelItem(1);
    QCOMPARE(one->foreground(2).color(), onTree("#e53935"));
    QCOMPARE(one->foreground(1).color(), onTree("#e53935"));
    QCOMPARE(two->foreground(2).color(), onTree("#1e88e5"));
    QCOMPARE(two->child(0)->foreground(2).color(), onTree("#43a047"));
    QCOMPARE(two->child(1)->foreground(2).color(), onTree("#1e88e5"));   // its cue's: the list's
    QCOMPARE(tree->topLevelItem(2)->foreground(2).color(), onTree("#1e88e5"));
    // Stopped: NEXT is cue 1, in red.
    QCOMPARE(runner.textColour("next"), onRunner("#e53935"));

    runner.go();
    settle();
    runner.refresh();
    QCOMPARE(runner.textColour("now"), onRunner("#e53935"));
    // GO into 2 plays 2.1 first: NEXT in 2.1's green.
    QCOMPARE(runner.textColour("next"), onRunner("#43a047"));
    runner.go();
    settle();
    runner.refresh();
    QCOMPARE(runner.textColour("now"), onRunner("#43a047"));
    QCOMPARE(runner.textColour("subnext"), onRunner("#1e88e5"));         // 2.2: its cue's
    // Done: grey, whatever its colour.
    QCOMPARE(tree->topLevelItem(0)->foreground(2).color(), runner.palette().color(QPalette::Disabled, QPalette::Text));

    // None anywhere: the theme's again.
    QVERIFY(st->setListColour(ids[0], QString()));
    runner.refresh();
    QCOMPARE(runner.textColour("subnext"), runner.palette().color(QPalette::WindowText));
    QCOMPARE(tree->topLevelItem(2)->foreground(2).color(), runner.palette().color(QPalette::Text));

    // The VC Cue Runner: NEXT and the upcoming lines.
    runner.stop();
    settle(3);
    QVERIFY(st->setListColour(ids[0], "Purple"));
    ScopedVc vc(m_doc);
    VirtualConsole *console = VirtualConsole::instance();
    VCFrame *contents = console->contents();
    VCCueRunner *w = new VCCueRunner(contents, m_doc);
    console->placeWidget(w, contents);
    w->resize(QSize(320, 260));
    w->runner()->setUpcomingCount(2);
    console->resize(700, 500);
    console->show();
    QVERIFY(QTest::qWaitForWindowExposed(console));
    w->runner()->refresh();
    CueRunner *compact = w->runner();
    const QPalette ip = compact->infoArea()->palette();
    QCOMPARE(compact->textColour("next"), readableOn(QColor("#e53935"), ip.color(QPalette::Window)));
    QCOMPARE(compact->upcomingTexts().count(), 2);
    QCOMPARE(compact->textColour("upcoming0"), readableOn(QColor("#8e24aa"), ip.color(QPalette::Window)));
    QCOMPARE(compact->textColour("upcoming1"), readableOn(QColor("#8e24aa"), ip.color(QPalette::Window)));
    for (const char *part : { "next", "upcoming0" })
        QVERIFY(contrastRatio(compact->textColour(part), ip.color(QPalette::Window)) >= 4.5);

    const QString dir = qEnvironmentVariable("QLC_CUECOLOUR_SHOTS");
    if (!dir.isEmpty())
    {
        QVERIFY(st->setCuesColour({ ids[3] }, "Orange"));
        compact->refresh();
        QTest::qWait(50);
        QVERIFY(w->grab().save(dir + "/vc-cuerunner.png"));
        runner.resize(900, 520);
        runner.show();
        QVERIFY(QTest::qWaitForWindowExposed(&runner));
        runner.go();
        settle();
        runner.refresh();
        QTest::qWait(50);
        QVERIFY(runner.grab().save(dir + "/run.png"));
        // The VC Cue List on the same list.
        VCCueList *cl = new VCCueList(contents, m_doc);
        console->placeWidget(cl, contents);
        cl->move(QPoint(330, 0));
        cl->resize(QSize(360, 260));
        cl->setChaser(runner.chaser()->id());
        QTest::qWait(100);
        QVERIFY(cl->grab().save(dir + "/vc-cuelist.png"));
        runner.hide();
    }
    console->hide();
}

void CueRunner_Test::screenshots()
{
    const QString dir = qEnvironmentVariable("QLC_CUERUNNER_SHOTS");
    const QString ws = qEnvironmentVariable("QLC_CUERUNNER_WS");
    if (dir.isEmpty() || ws.isEmpty())
        QSKIP("QLC_CUERUNNER_SHOTS / QLC_CUERUNNER_WS not set");
    const QString tag = qEnvironmentVariable("QLC_CUERUNNER_SHOTS_TAG", "shot");
    QDir().mkpath(dir);

    delete m_doc;
    m_doc = new Doc(this);
    QFile file(ws);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader r(&file);
    bool loaded = false;
    if (r.readNextStartElement())
    {
        while (r.readNextStartElement())
        {
            if (r.name() == QLatin1String("Engine"))
            {
                loaded = m_doc->loadXML(r, false);
                break;
            }
            r.skipCurrentElement();
        }
    }
    QVERIFY(loaded);

    ScopedVc vc(m_doc);
    VirtualConsole *console = VirtualConsole::instance();
    VCFrame *contents = console->contents();
    VCCueRunner *w = new VCCueRunner(contents, m_doc);
    console->placeWidget(w, contents);
    w->move(QPoint(0, 0));
    console->resize(900, 700);
    console->show();
    QVERIFY(QTest::qWaitForWindowExposed(console));
    w->runner()->go();                      // the first cue plays, more to come
    settle(3);
    const QList<QSize> sizes { QSize(300, 140), QSize(560, 380) };
    for (Doc::Mode mode : { Doc::Design, Doc::Operate })
    {
        m_doc->setMode(mode);
        for (const QSize &sz : sizes)
        {
            w->resize(sz);
            w->runner()->refresh();
            QTest::qWait(50);
            const QString name = QStringLiteral("%1/%2-%3-%4x%5.png").arg(dir, tag,
                                 mode == Doc::Design ? "design" : "operate")
                                 .arg(sz.width()).arg(sz.height());
            QVERIFY(w->grab().save(name));
        }
    }
    // The list size, large.
    w->setRunnerSize(CueRunner::List);
    w->resize(sizes.last());
    w->runner()->refresh();
    QTest::qWait(50);
    QVERIFY(w->grab().save(QStringLiteral("%1/%2-operate-list-%3x%4.png").arg(dir, tag)
                           .arg(sizes.last().width()).arg(sizes.last().height())));
    m_doc->setMode(Doc::Design);
    console->hide();
}

QTEST_MAIN(CueRunner_Test)

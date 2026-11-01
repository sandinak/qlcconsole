/*
  Q Light Controller Plus
  whyonpanel_test.cpp

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

#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <QPointer>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#include "whyonpanel.h"
#undef private
#undef protected

#include "whyonpanel_test.h"
#include "simpledeskengine.h"
#include "teachingsettings.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "programmercontroller.h"
#include "fixture.h"
#include <QPushButton>
#include "scene.h"
#include "doc.h"
#include <QCheckBox>

void WhyOnPanel_Test::init()
{
    m_doc = new Doc(this);
    m_fixtures.clear();
    for (int i = 0; i < 3; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dim %1").arg(i + 1));
        fxi->setAddress(quint32(i));
        fxi->setUniverse(0);
        fxi->setChannels(1);
        m_doc->addFixture(fxi);
        m_fixtures << fxi->id();
    }
    // A three-channel generic fixture at 10..12, for the channel picker.
    Fixture *rgb = new Fixture(m_doc);
    rgb->setName("Par");
    rgb->setAddress(10);
    rgb->setUniverse(0);
    rgb->setChannels(3);
    m_doc->addFixture(rgb);
    m_fixtures << rgb->id();
}

void WhyOnPanel_Test::cleanup()
{
    WhyOnPanel::setFollowSelection(false);
    WhyOnPanel::setJumpOverride(nullptr);
    delete WhyOnPanel::current();
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(3);
    delete m_doc;
    m_doc = nullptr;
}

void WhyOnPanel_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
}

void WhyOnPanel_Test::listsTheChainAndAClickGoesThere()
{
    Scene *warm = new Scene(m_doc);
    warm->setName("Warm");
    warm->setValue(m_fixtures[0], 0, 200);
    QVERIFY(m_doc->addFunction(warm));
    Scene *cool = new Scene(m_doc);
    cool->setName("Cool");
    cool->setValue(m_fixtures[0], 0, 100);
    QVERIFY(m_doc->addFunction(cool));
    warm->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    cool->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);

    WhyOnPanel *p = WhyOnPanel::showFor(m_doc, m_fixtures[0]);
    QVERIFY(p != nullptr);
    QCOMPARE(WhyOnPanel::current(), p);
    QCOMPARE(p->windowTitle(), QString("Why is this on?"));
    QListWidget *list = p->findChild<QListWidget *>("whyOnLines");
    QVERIFY(list != nullptr);
    QCOMPARE(list->count(), 2);
    QCOMPARE(list->item(0)->text(),
             QString("Scene “Cool” sets 39 % (lower: on this channel the highest level wins, HTP)"));
    QCOMPARE(list->item(1)->text(), QString("Scene “Warm” sets 78 %"));
    QVERIFY(list->item(1)->font().bold());
    QVERIFY(!list->item(0)->font().bold());
    QLabel *head = p->findChild<QLabel *>("whyOnHeadline");
    QCOMPARE(head->text(), QString("At 78 %. Scene “Warm” sets 78 %."));
    // One fixture channel: no picker.
    QVERIFY(!p->findChild<QComboBox *>("whyOnChannel")->isVisibleTo(p));

    // A click goes to what the line names.
    QList<WhyOn::Line> went;
    WhyOnPanel::setJumpOverride([&went](const WhyOn::Line &l) { went << l; });
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      list->visualItemRect(list->item(1)).center());
    QCOMPARE(went.count(), 1);
    QCOMPARE(went.first().kind, WhyOn::Kind::Function);
    QCOMPARE(went.first().functionId, warm->id());

    // Asking about another light moves the same window.
    QCOMPARE(WhyOnPanel::showFor(m_doc, m_fixtures[1]), p);
    QCOMPARE(p->answer().fixtureId, m_fixtures[1]);
    // Dark: why it isn't on comes first, then what sets it.
    QCOMPARE(list->item(list->count() - 1)->text(),
             QString("Nothing sets it now: an intensity is dark unless something sets it"));
}

/* The real Simple Desk (its engine names its fader): it overrides what is
   under it -- a scene's higher level too -- and it's named. */
void WhyOnPanel_Test::theSimpleDeskBeatsAScene()
{
    Scene *s = new Scene(m_doc);
    s->setName("Wash");
    s->setValue(m_fixtures[2], 0, 100);
    QVERIFY(m_doc->addFunction(s));
    s->start(m_doc->masterTimer(), FunctionParent::master());
    SimpleDeskEngine *desk = new SimpleDeskEngine(m_doc);
    desk->setValue(2, 255);
    tick(4);

    WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[2]);
    QCOMPARE(w.texts(), QStringList()
             << "Simple Desk sets 100 %"
             << "Scene \u201CWash\u201D sets 39 % (replaced by what is above it)");
    QCOMPARE(w.lines.first().kind, WhyOn::Kind::SimpleDesk);
    QCOMPARE(w.lines.first().verdict, WhyOn::Verdict::Wins);

    // Lower than the scene: the desk still holds it (an override, not HTP).
    desk->setValue(2, 20);
    tick(4);
    QCOMPARE(int(w.out), 255);
    w = WhyOn::explain(m_doc, m_fixtures[2]);
    QCOMPARE(int(w.out), 20);
    QCOMPARE(w.texts(), QStringList()
             << "Simple Desk sets 8 %"
             << "Scene \u201CWash\u201D sets 39 % (replaced by what is above it)");
    delete desk;
}

void WhyOnPanel_Test::aBareAddressAndTheChannelPicker()
{
    // The desk's universe mode asks by address: the fixture there, its channel.
    WhyOnPanel *p = WhyOnPanel::showForAddress(m_doc, 0, 11);
    QVERIFY(p != nullptr);
    QCOMPARE(p->answer().fixtureId, m_fixtures[3]);
    QCOMPARE(p->answer().channel, quint32(1));
    QComboBox *ch = p->findChild<QComboBox *>("whyOnChannel");
    QVERIFY(ch->isVisibleTo(p));
    QCOMPARE(ch->count(), 3);
    QCOMPARE(ch->currentIndex(), 1);
    ch->setCurrentIndex(2);
    emit ch->activated(2);
    QCOMPARE(p->answer().channel, quint32(2));

    // No fixture there: universe and channel.
    p = WhyOnPanel::showForAddress(m_doc, 0, 300);
    QCOMPARE(p->answer().subject, QString("Universe 1, channel 301"));
    QVERIFY(!ch->isVisibleTo(p));
}

void WhyOnPanel_Test::plainWordsInATeachingShow()
{
    m_doc->teachingSettings()->setEnabled(true);
    WhyOnPanel *p = WhyOnPanel::showFor(m_doc, m_fixtures[0]);
    QVERIFY(p->findChild<QCheckBox *>("whyOnPlain")->isChecked());
    QCOMPARE(p->wording(), WhyOn::Wording::Simple);
    QVERIFY(p->answer().headline.startsWith("It's at 0 %."));
    p->setWording(WhyOn::Wording::Crew);
    QVERIFY(p->answer().headline.startsWith("At 0 %."));
    m_doc->teachingSettings()->setEnabled(false);
}

/* Shown, it asks again on a UI timer; hidden, the timer stops: nothing
   runs for it at all. */
void WhyOnPanel_Test::asksNothingWhileHidden()
{
    WhyOnPanel *p = WhyOnPanel::showFor(m_doc, m_fixtures[0]);
    QTimer *t = p->m_timer;
    QVERIFY(t->isActive());
    p->hide();
    QVERIFY(!t->isActive());
    p->show();
    QVERIFY(t->isActive());
    p->close();             // deletes itself
    QTRY_VERIFY(WhyOnPanel::current() == nullptr);
}

/* v3: a dark light -- why it isn't on, under its own header, before what
   sets it; a header row goes nowhere. */
void WhyOnPanel_Test::aDarkLightSaysWhyNotFirst()
{
    tick(2);
    WhyOnPanel *p = WhyOnPanel::showFor(m_doc, m_fixtures[1]);
    QListWidget *list = p->lines();
    QStringList rows;
    for (int i = 0; i < list->count(); ++i)
        rows << list->item(i)->text();
    QCOMPARE(rows, QStringList()
             << "Why it isn't on:"
             << "✖ Universe 1 has no output: nothing leaves the computer for it (Connections)"
             << "✖ Nothing sets its dimmer: no cue, scene, desk or slider is on it"
             << "What sets it:"
             << "Nothing sets it now: an intensity is dark unless something sets it");
    QVERIFY(p->lineAt(0) == nullptr);
    QVERIFY(p->lineAt(3) == nullptr);
    QCOMPARE(p->lineAt(1)->kind, WhyOn::Kind::NoOutput);
    QCOMPARE(p->lineAt(4)->kind, WhyOn::Kind::Nothing);
    QList<WhyOn::Line> went;
    WhyOnPanel::setJumpOverride([&went](const WhyOn::Line &l) { went << l; });
    p->activate(0);
    QVERIFY(went.isEmpty());
    p->activate(2);
    QCOMPARE(went.count(), 1);
    QCOMPARE(went.first().kind, WhyOn::Kind::Nothing);
    QCOMPARE(p->findChild<QLabel *>("whyOnHeadline")->text(),
             QString("At 0 %. Not on: Nothing sets its dimmer: no cue, scene, desk or slider is on it."));
}

/* A parked light: its Park line offers Unpark, in Design mode only. */
void WhyOnPanel_Test::unparkFromALine()
{
    Scene *s = new Scene(m_doc);
    s->setName("Look");
    s->setValue(m_fixtures[0], 0, 200);
    QVERIFY(m_doc->addFunction(s));
    s->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    m_doc->programmer()->parkFixtures({ m_fixtures[0] });
    tick(3);
    WhyOnPanel *p = WhyOnPanel::showFor(m_doc, m_fixtures[0]);
    QCOMPARE(p->answer().texts().first(), QString("Park holds it at 78 %"));
    QPushButton *act = p->actionButton();
    QVERIFY(!act->isVisibleTo(p));
    p->lines()->setCurrentRow(0);
    QVERIFY(act->isVisibleTo(p));
    QCOMPARE(act->text(), QString("Unpark"));

    // Operate: the window only explains.
    m_doc->setMode(Doc::Operate);
    p->refresh();
    QVERIFY(!act->isEnabled());
    QVERIFY(!p->runAction(0));
    QVERIFY(m_doc->programmer()->isFixtureParked(m_fixtures[0]));
    m_doc->setMode(Doc::Design);
    p->refresh();
    QVERIFY(act->isEnabled());

    QTest::mouseClick(act, Qt::LeftButton);
    QVERIFY(!m_doc->programmer()->isFixtureParked(m_fixtures[0]));
    tick(3);
    p->refresh();
    QCOMPARE(p->answer().texts(), QStringList() << "Scene “Look” sets 78 %");
    // A scene's line offers Stop; a line with nothing to do offers nothing.
    p->lines()->setCurrentRow(0);
    QVERIFY(act->isVisibleTo(p));
    QCOMPARE(act->text(), QString("Stop the function"));
}

void WhyOnPanel_Test::theActionsALineOffers()
{
    WhyOn::Line l;
    l.kind = WhyOn::Kind::Park;
    QCOMPARE(WhyOnPanel::actionFor(l), WhyOnPanel::Action::Unpark);
    l.kind = WhyOn::Kind::Mark;
    QCOMPARE(WhyOnPanel::actionFor(l), WhyOnPanel::Action::LetGoMark);
    QCOMPARE(WhyOnPanel::actionName(WhyOnPanel::Action::LetGoMark), QString("Let go of the move in black"));
    l.kind = WhyOn::Kind::Cue;
    QCOMPARE(WhyOnPanel::actionFor(l), WhyOnPanel::Action::NoAction);

    // The real Simple Desk's line knows its channel: Release.
    SimpleDeskEngine *desk = new SimpleDeskEngine(m_doc);
    desk->setValue(2, 255);
    tick(4);
    const WhyOn::Answer w = WhyOn::explain(m_doc, m_fixtures[2]);
    QCOMPARE(w.lines.first().kind, WhyOn::Kind::SimpleDesk);
    QCOMPARE(w.lines.first().absAddress, quint32(2));
    QCOMPARE(WhyOnPanel::actionFor(w.lines.first()), WhyOnPanel::Action::Release);
    QCOMPARE(WhyOnPanel::actionName(WhyOnPanel::Action::Release), QString("Release the desk channel"));
    delete desk;
}

/* ⌘I: on the light picked last in any view; none (or gone), a hint. */
void WhyOnPanel_Test::commandIOpensOnTheSelection()
{
    QCOMPARE(WhyOnPanel::shortcut(), QKeySequence(Qt::CTRL | Qt::Key_I));
    WhyOnPanel::noteSelection({ m_fixtures[2], m_fixtures[0] });
    WhyOnPanel::noteSelection({});          // an emptied selection keeps the last
    WhyOnPanel *p = WhyOnPanel::showForSelection(m_doc);
    QVERIFY(p != nullptr);
    QCOMPARE(p->answer().fixtureId, m_fixtures[2]);

    WhyOnPanel::noteSelection({ 9999 });    // a light since deleted
    p = WhyOnPanel::showForSelection(m_doc);
    QVERIFY(!p->answer().isValid());
    QVERIFY(p->findChild<QLabel *>("whyOnHeadline")->text().startsWith("Select a light"));
    QCOMPARE(p->lines()->count(), 0);
}

/* "Follow selection": off until asked, kept for the next window, and while
   on, the window moves to the light selected next. */
void WhyOnPanel_Test::followSelectionIsOffAndRemembered()
{
    QVERIFY(!WhyOnPanel::followSelection());
    WhyOnPanel *p = WhyOnPanel::showFor(m_doc, m_fixtures[0]);
    QVERIFY(p != nullptr);
    QCheckBox *follow = p->followBox();
    QCOMPARE(follow->text(), QString("Follow selection"));
    QVERIFY(!follow->isChecked());

    // Off: a new selection leaves the window where it is.
    WhyOnPanel::noteSelection({ m_fixtures[1] });
    QCOMPARE(p->answer().fixtureId, m_fixtures[0]);

    follow->setChecked(true);
    QVERIFY(WhyOnPanel::followSelection());
    WhyOnPanel::noteSelection({ 9999, m_fixtures[1] });       // a gone light is passed over
    QCOMPARE(p->answer().fixtureId, m_fixtures[1]);
    WhyOnPanel::noteSelection({});                              // emptied: stays
    QCOMPARE(p->answer().fixtureId, m_fixtures[1]);
    WhyOnPanel::noteSelection({ m_fixtures[2] });
    QCOMPARE(p->answer().fixtureId, m_fixtures[2]);

    // Hidden, it does not move.
    p->hide();
    WhyOnPanel::noteSelection({ m_fixtures[0] });
    QCOMPARE(p->answer().fixtureId, m_fixtures[2]);

    // Remembered: the next window comes up with it on.
    delete p;
    p = WhyOnPanel::showFor(m_doc, m_fixtures[0]);
    QVERIFY(p->followBox()->isChecked());
    p->followBox()->setChecked(false);
    QVERIFY(!WhyOnPanel::followSelection());
}

/* A running function's line offers Stop, in Design mode only. */
void WhyOnPanel_Test::stopAFunctionFromALine()
{
    Scene *s = new Scene(m_doc);
    s->setName("Wash");
    s->setValue(m_fixtures[0], 0, 200);
    QVERIFY(m_doc->addFunction(s));
    s->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    QVERIFY(s->isRunning());
    WhyOnPanel *p = WhyOnPanel::showFor(m_doc, m_fixtures[0]);
    QCOMPARE(p->lineAt(0)->kind, WhyOn::Kind::Function);
    QCOMPARE(WhyOnPanel::actionFor(*p->lineAt(0)), WhyOnPanel::Action::StopFunction);
    QCOMPARE(WhyOnPanel::actionName(WhyOnPanel::Action::StopFunction), QString("Stop the function"));
    p->lines()->setCurrentRow(0);
    QCOMPARE(p->actionButton()->text(), QString("Stop the function"));

    // Operate: the window only explains.
    m_doc->setMode(Doc::Operate);
    p->refresh();
    QVERIFY(!p->actionButton()->isEnabled());
    QVERIFY(!p->runAction(0));
    QVERIFY(s->isRunning());

    m_doc->setMode(Doc::Design);
    p->refresh();
    QVERIFY(p->actionButton()->isEnabled());
    QTest::mouseClick(p->actionButton(), Qt::LeftButton);
    for (int i = 0; i < 20 && s->isRunning(); ++i)
        tick(1);
    QVERIFY(!s->isRunning());
    p->refresh();
    QVERIFY(p->answer().lines.isEmpty() || p->answer().lines.first().kind != WhyOn::Kind::Function
            || p->answer().lines.first().functionId != s->id());
}

/* Open, the window adds nothing to the engine (no fader, no probe); closed,
   its timer is gone with it and nothing asks again. */
void WhyOnPanel_Test::closingTheWindowStopsAllWork()
{
    Universe *uni = m_doc->inputOutputMap()->universe(0);
    const int faders = uni->faders().count();
    WhyOnPanel *p = WhyOnPanel::showFor(m_doc, m_fixtures[0]);
    QCOMPARE(uni->faders().count(), faders);
    QPointer<QTimer> timer = p->m_timer;
    QVERIFY(timer->isActive());
    int asked = 0;
    QObject::connect(timer.data(), &QTimer::timeout, [&asked]() { ++asked; });
    QTRY_VERIFY(asked > 0);
    p->close();
    QTRY_VERIFY(WhyOnPanel::current() == nullptr);
    QVERIFY(timer.isNull());
    const int before = asked;
    QTest::qWait(600);
    QCOMPARE(asked, before);
    QCOMPARE(uni->faders().count(), faders);
}

QTEST_MAIN(WhyOnPanel_Test)

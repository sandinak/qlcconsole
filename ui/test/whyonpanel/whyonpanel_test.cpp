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
#include <QtTest>

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
#include "fixture.h"
#include "scene.h"
#include "doc.h"

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
    QCOMPARE(list->item(0)->text(),
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

QTEST_MAIN(WhyOnPanel_Test)

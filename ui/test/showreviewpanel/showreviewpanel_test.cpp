/*
  Q Light Controller Plus - Unit test
  showreviewpanel_test.cpp

  The Review panel (docs/features/show-review.md), offscreen.

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
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>

#include "showreviewpanel_test.h"
#include "showstatusdialog.h"
#include "showreviewpanel.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "showstatus.h"
#include "showreview.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

namespace
{
Fixture *dimmer(Doc *doc, const QString &name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Lamp");
    def->setType(QLCFixtureDef::Dimmer);
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setPreset(QLCChannel::IntensityDimmer);
    def->addChannel(ch);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Mode");
    mode->insertChannel(ch, 0);
    QLCPhysical phy;
    phy.setPowerConsumption(500);
    mode->setPhysical(phy);
    def->addMode(mode);
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    fx->setFixtureDefinition(def, mode);
    fx->setAddress(address);
    doc->addFixture(fx);
    return fx;
}

QTreeWidgetItem *rowOf(QTreeWidget *tree, const QString &check)
{
    QTreeWidgetItemIterator it(tree);
    while (*it)
    {
        if ((*it)->text(0) == check)
            return *it;
        ++it;
    }
    return nullptr;
}

int topCount(QTreeWidget *tree, const QString &prefix)
{
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        if (tree->topLevelItem(i)->text(0).startsWith(prefix))
            return tree->topLevelItem(i)->childCount();
    return -1;
}
}

void ShowReviewPanel_Test::init()
{
    m_doc = new Doc(this);
    // A blocker (an overlap) and a note (two names alike).
    dimmer(m_doc, "Lamp 1", 0);
    Fixture *b = dimmer(m_doc, "Lamp 2", 5);
    b->setAddress(0);
    b->setName("Lamp 1");
}

void ShowReviewPanel_Test::cleanup()
{
    for (const ShowStatus::Entry &e : ShowStatus::instance()->allEntries())
        ShowStatus::instance()->clearStatus(e.key);
    delete m_doc;
    m_doc = nullptr;
}

void ShowReviewPanel_Test::listsBySeverity()
{
    ShowReviewPanel panel(m_doc);
    QVERIFY(panel.summary()->text().contains("Not checked"));
    panel.recheck();
    QCOMPARE(topCount(panel.tree(), "Blockers"), 1);
    QVERIFY(topCount(panel.tree(), "Notes") >= 1);    // and "no output anywhere"
    QVERIFY(rowOf(panel.tree(), "PAT-01") != nullptr);
    QVERIFY(rowOf(panel.tree(), "FIX-14") != nullptr);
    QVERIFY(panel.summary()->text().contains("1 blockers"));
    // Why? is on the row: the check's title and paragraph.
    QVERIFY(rowOf(panel.tree(), "PAT-01")->toolTip(1).contains("fight"));
}

void ShowReviewPanel_Test::acceptAndReopen()
{
    ShowReviewPanel panel(m_doc);
    panel.recheck();
    QVERIFY(panel.selectCheck("PAT-01"));
    QVERIFY(panel.acceptButton()->isEnabled());
    QVERIFY(!panel.reopenButton()->isEnabled());
    QVERIFY(!panel.acceptSelected("  ", "Ava"));            // a note is required
    QVERIFY(panel.acceptSelected("Doubled lamp on purpose", "Ava R."));
    QCOMPARE(m_doc->showReview()->acceptances().count(), 1);
    QCOMPARE(m_doc->showReview()->acceptances().first().by, QStringLiteral("Ava R."));
    // Gone from Open, there under Accepted with its note.
    QVERIFY(rowOf(panel.tree(), "PAT-01") == nullptr);
    panel.setFilter(ShowReviewPanel::AcceptedOnly);
    QTreeWidgetItem *row = rowOf(panel.tree(), "PAT-01");
    QVERIFY(row != nullptr);
    QVERIFY(row->text(1).contains("Doubled lamp on purpose"));
    QVERIFY(row->text(1).contains("Ava R."));
    QVERIFY(panel.summary()->text().contains("1 accepted"));
    QVERIFY(panel.selectCheck("PAT-01"));
    QVERIFY(panel.reopenButton()->isEnabled());
    QVERIFY(panel.reopenSelected());
    QVERIFY(m_doc->showReview()->acceptances().isEmpty());
}

void ShowReviewPanel_Test::filters()
{
    ShowReviewPanel panel(m_doc);
    panel.recheck();
    QVERIFY(panel.selectCheck("FIX-14"));
    QVERIFY(panel.acceptSelected("Fine", "Ava"));
    panel.setFilter(ShowReviewPanel::OpenOnly);
    QVERIFY(rowOf(panel.tree(), "FIX-14") == nullptr);
    QVERIFY(rowOf(panel.tree(), "PAT-01") != nullptr);
    panel.setFilter(ShowReviewPanel::All);
    QVERIFY(rowOf(panel.tree(), "FIX-14") != nullptr);
    QVERIFY(rowOf(panel.tree(), "PAT-01") != nullptr);
    panel.setSearch("share");
    QVERIFY(rowOf(panel.tree(), "FIX-14") == nullptr);
    QVERIFY(rowOf(panel.tree(), "PAT-01") != nullptr);
}

void ShowReviewPanel_Test::fixGoesToThePlace()
{
    QStringList ran;
    ShowStatus::instance()->setAction("fixtures.duplicates", "Number Them",
        [&ran](QWidget *, const QVariantList &items) { ran << items.first().toString(); });
    QString jumped;
    ShowReviewPanel::setJumper([&jumped](QWidget *, const QString &jump, const QVariantList &) {
        jumped = jump;
        return true;
    });
    ShowReviewPanel panel(m_doc);
    panel.recheck();
    QVERIFY(panel.selectCheck("FIX-14"));
    QVERIFY(panel.fixButton()->isEnabled());
    QVERIFY(panel.fixSelected());
    QCOMPARE(ran, QStringList() << "Lamp 1");              // the footer's own fix
    ShowReviewPanel::setJumper(ShowReviewPanel::Jumper());
}

void ShowReviewPanel_Test::footerEntriesFoldIn()
{
    ShowReviewPanel panel(m_doc);
    panel.recheck();
    QVERIFY(rowOf(panel.tree(), "FIX-03") == nullptr);
    // A design entry of the footer that no check reads yet: under its id.
    ShowStatus::instance()->setStatus("definitions.outofdate", ShowStatus::Warning,
                                      "2 fixture definitions are out of date");
    QTreeWidgetItem *row = rowOf(panel.tree(), "FIX-03");
    QVERIFY(row != nullptr);
    QVERIFY(row->text(1).contains("out of date"));
    // A runtime one stays the footer's.
    ShowStatus::instance()->setStatus("outputs.problems", ShowStatus::Warning, "Node not answering");
    QTreeWidgetItemIterator it(panel.tree());
    while (*it)
    {
        QVERIFY(!(*it)->text(1).contains("Node not answering"));
        ++it;
    }
    // A footer entry a check reads is not listed twice.
    ShowStatus::instance()->setStatus("fixtures.clash", ShowStatus::Warning, "1 fixture clashes");
    int pat01 = 0;
    QTreeWidgetItemIterator again(panel.tree());
    while (*again)
    {
        if ((*again)->text(0) == "PAT-01")
            ++pat01;
        ++again;
    }
    QCOMPARE(pat01, 1);
}

void ShowReviewPanel_Test::readinessDialogLeadsHere()
{
    ShowStatus::instance()->setStatus("fixtures.clash", ShowStatus::Warning, "1 fixture clashes");
    ShowStatus::instance()->setStatus("outputs.problems", ShowStatus::Warning, "Node not answering");
    QWidget host;
    ShowReviewPanel::setShared(m_doc, &host);
    ShowStatusDialog dlg;
    bool sawId = false, sawRuntime = false;
    for (int i = 0; i < dlg.tree()->topLevelItemCount(); ++i)
    {
        const QString t = dlg.tree()->topLevelItem(i)->text(0);
        sawId = sawId || t == QStringLiteral("PAT-01 · 1 fixture clashes");
        sawRuntime = sawRuntime || t == QStringLiteral("Node not answering");
    }
    QVERIFY(sawId);
    QVERIFY(sawRuntime);
    QPushButton *review = dlg.findChild<QPushButton *>("showStatusReview");
    QVERIFY(review != nullptr);
    review->click();
    QVERIFY(ShowReviewPanel::shared() != nullptr);
    QVERIFY(ShowReviewPanel::shared()->isVisible());
    QVERIFY(m_doc->showReview()->lastResult().ran);         // opened: checked
    ShowReviewPanel::shared()->close();
    ShowReviewPanel::setShared(nullptr, nullptr);
}

QTEST_MAIN(ShowReviewPanel_Test)

/*
  Q Light Controller Plus - Test Unit
  fixtureselection_test.cpp

  Copyright (c) Massimo Callegari

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
#include <QLineEdit>
#include <QTreeWidgetItemIterator>
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "fixture.h"

#define protected public
#define private public
#include "fixtureselection.h"
#include "fixturetreewidget.h"
#include "doc.h"
#undef private
#undef protected

#include "fixtureselection_test.h"

void FixtureSelection_Test::init()
{
    m_doc = new Doc(this);
}

void FixtureSelection_Test::cleanup()
{
    delete m_doc;
}

void FixtureSelection_Test::execDialog()
{
    FixtureSelection dlg(NULL, m_doc);
    QTimer::singleShot(100, &dlg, SLOT(reject()));
    QCOMPARE(dlg.exec(), int(QDialog::Rejected));
}

void FixtureSelection_Test::searchFiltersAndPicksASingleMatch()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Chauvet");
    def->setModel("Rogue R1 Spot");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("1ch");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setGroup(QLCChannel::Intensity);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    def->addMode(mode);

    auto add = [this](const QString &name, quint32 addr) {
        Fixture *f = new Fixture(m_doc);
        f->setName(name);
        f->setChannels(1);
        f->setAddress(addr);
        m_doc->addFixture(f);
        return f;
    };
    Fixture *wash1 = add("Wash 1", 0);
    Fixture *wash2 = add("Wash 2", 1);
    Fixture *spot = new Fixture(m_doc);
    spot->setName("SR Mover");
    spot->setFixtureDefinition(def, mode);
    spot->setAddress(10);
    QVERIFY(m_doc->addFixture(spot));

    {
        FixtureSelection dlg(NULL, m_doc);
        dlg.setMultiSelection(true);
        dlg.setDisabledFixtures(QList<quint32>() << wash2->id());   // already on the plot
        dlg.m_tree->updateTree();
        QVERIFY(dlg.m_search != NULL);
        auto shown = [&dlg]() {
            QStringList out;
            for (QTreeWidgetItemIterator it(dlg.m_tree); *it != NULL; ++it)
            {
                QTreeWidgetItem *item = *it;
                bool vis = !item->isHidden();
                for (QTreeWidgetItem *up = item->parent(); vis && up != NULL; up = up->parent())
                    vis = !up->isHidden();
                if (vis && item->data(0, PROP_ID).isValid() && !item->data(0, PROP_HEAD).isValid())
                    out << item->text(0);
            }
            out.sort();
            return out;
        };
        auto picked = [&dlg]() {
            QStringList out;
            foreach (QTreeWidgetItem *it, dlg.m_tree->selectedItems())
                out << it->text(0);
            return out;
        };
        QCOMPARE(shown(), QStringList() << "SR Mover" << "Wash 1" << "Wash 2");

        // By name: two show; Wash 2 is already placed, so the one you CAN
        // add is picked for you.
        dlg.m_search->setText("wash");
        QCOMPARE(shown(), QStringList() << "Wash 1" << "Wash 2");
        QCOMPARE(picked(), QStringList() << "Wash 1");
        // "s" is in all three names: all shown.
        dlg.m_search->setText("s");
        QVERIFY(shown().size() == 3);
        QCOMPARE(dlg.m_tree->setTextFilter("WASH"), 2);    // any case, and counted

        // By model (a column other than the name): the one match is picked.
        dlg.m_search->setText("rogue");
        QCOMPARE(shown(), QStringList() << "SR Mover");
        QCOMPARE(picked(), QStringList() << "SR Mover");
        // ...and by manufacturer.
        dlg.m_search->setText("chauvet");
        QCOMPARE(shown(), QStringList() << "SR Mover");

        // A single match that can't be picked (already placed) is not.
        dlg.m_search->setText("wash 2");
        QCOMPARE(shown(), QStringList() << "Wash 2");
        QVERIFY(picked().isEmpty());

        // Nothing matching: nothing shown, and a hidden pick is dropped.
        dlg.m_search->setText("rogue");
        dlg.m_search->setText("zzz");
        QVERIFY(shown().isEmpty());
        QVERIFY(picked().isEmpty());

        // Cleared: everything back.
        dlg.m_search->clear();
        QCOMPARE(shown().size(), 3);

        // Down from the search goes into the list.
        dlg.m_search->setText("wash");
        dlg.show();
        dlg.m_search->setFocus();
        QTest::keyClick(dlg.m_search, Qt::Key_Down);
        QVERIFY(dlg.m_tree->currentItem() != NULL);
        QVERIFY(dlg.m_tree->currentItem()->text(0).startsWith("Wash") ||
                dlg.m_tree->currentItem()->childCount() > 0);

        // Return adds the single match.
        dlg.m_search->setText("SR Mo");
        QTest::keyClick(dlg.m_search, Qt::Key_Return);
        QCOMPARE(dlg.result(), int(QDialog::Accepted));
        QCOMPARE(dlg.selection(), QList<quint32>() << spot->id());
    }
    Q_UNUSED(wash1)
    m_doc->deleteFixture(spot->id());
    delete def;
}

QTEST_MAIN(FixtureSelection_Test)

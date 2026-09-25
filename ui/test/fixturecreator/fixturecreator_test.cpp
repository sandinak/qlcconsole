/*
  Q Light Controller Plus - Test Unit
  fixturecreator_test.cpp

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

#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <functional>
#include <memory>
#include <QSpinBox>
#include <QTimer>
#include <QMessageBox>
#include <QTreeWidgetItemIterator>

#define private public
#define protected public
#include "addfixture.h"
#include "fixturetreewidget.h"
#undef protected
#undef private
#include "fixturecreator.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcchannel.h"
#include "fixturegroup.h"
#include "inputoutputmap.h"
#include "fixture.h"
#include "doc.h"

#include "fixturecreator_test.h"

namespace
{
/* Run @p fill on the AddFixture dialog once it is up, then press OK. The dialog
 * is modal and blocks addFromDialog(), so this has to come from a timer. */
void driveAddDialog(std::function<void(AddFixture *)> fill)
{
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [fill, poll]() {
        AddFixture *dlg = qobject_cast<AddFixture *>(QApplication::activeModalWidget());
        if (dlg == nullptr)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        fill(dlg);
        dlg->m_buttonBox->button(QDialogButtonBox::Ok)->click();
    };
    QTimer::singleShot(0, *poll);
}

QTreeWidgetItem *rootWith(FixtureTreeWidget &tree, int role)
{
    for (QTreeWidgetItemIterator it(&tree); *it != nullptr; ++it)
        if ((*it)->data(0, role).isValid())
            return *it;
    return nullptr;
}
} // namespace

void FixtureCreator_Test::init()
{
    m_doc = new Doc(this);
}

void FixtureCreator_Test::cleanup()
{
    delete m_doc;
}

void FixtureCreator_Test::addToInventoryCreatesAnUnpatchedFixture()
{
    driveAddDialog([](AddFixture *dlg) {
        dlg->m_nameEdit->setText("Fence Segment");
        dlg->slotNameEdited("Fence Segment");   // what typing does; the dialog reads it from here
    });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                              InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(made.size(), 1);
    Fixture *fxi = m_doc->fixture(made.first());
    QVERIFY(fxi != nullptr);
    QVERIFY2(!fxi->isPatched(), "a fixture added to the inventory must come out unpatched");
    QCOMPARE(fxi->name(), QString("Fence Segment"));
}

void FixtureCreator_Test::aStagedFixtureAppearsInNoUniverseAnywhere()
{
    driveAddDialog([](AddFixture *) {});
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                              InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(made.size(), 1);

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber | FixtureTreeWidget::AddressRange
                                  | FixtureTreeWidget::ShowGroups);
    tree.updateTree();

    // It is under the Inventory root...
    QTreeWidgetItem *inv = rootWith(tree, PROP_INVENTORY);
    QVERIFY(inv != nullptr);
    QCOMPARE(inv->childCount(), 1);                           // one manufacturer
    QTreeWidgetItem *leaf = inv;
    while (leaf->childCount() > 0)                            // ...down to the fixture
        leaf = leaf->child(0);
    QVERIFY(leaf->data(0, PROP_ID).isValid());

    // ...and under NO universe: every universe node in the tree is empty.
    for (QTreeWidgetItemIterator it(&tree); *it != nullptr; ++it)
    {
        if ((*it)->data(0, PROP_UNIVERSE).isValid())
            QVERIFY2((*it)->childCount() == 0,
                     qPrintable(QString("universe row \"%1\" holds a staged fixture").arg((*it)->text(0))));
    }
    QVERIFY(!leaf->text(1).contains(QRegularExpression("^\\d+$")));   // universe column is not "1"
}

void FixtureCreator_Test::aStagedFixtureNeverOccupiesAnAddress()
{
    driveAddDialog([](AddFixture *) {});
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                              InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(made.size(), 1);
    QCOMPARE(m_doc->fixtureForAddress(0), Fixture::invalidId());

    // A second one goes in without any address complaint.
    driveAddDialog([](AddFixture *) {});
    QCOMPARE(FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                           InputOutputMap::invalidUniverse(), nullptr).size(), 1);
    QCOMPARE(m_doc->fixturesCount(), 2);
}

void FixtureCreator_Test::ordinaryAddStillPatches()
{
    driveAddDialog([](AddFixture *) {});
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false,
                                                              InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(made.size(), 1);
    Fixture *fxi = m_doc->fixture(made.first());
    QVERIFY(fxi->isPatched());
    QCOMPARE(m_doc->fixtureForAddress(fxi->universeAddress()), fxi->id());
}

void FixtureCreator_Test::networkFixtureKeepsItsIp()
{
    driveAddDialog([](AddFixture *dlg) {
        dlg->m_connectionCombo->setCurrentIndex(dlg->m_connectionCombo->findData(int(Fixture::Network)));
        dlg->m_ipEdit->setText("172.18.2.41");
    });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                              InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(made.size(), 1);
    Fixture *fxi = m_doc->fixture(made.first());
    QCOMPARE(fxi->connection(), Fixture::Network);
    QCOMPARE(fxi->ipAddress(), QString("172.18.2.41"));
    QVERIFY(!fxi->isPatched());
}

void FixtureCreator_Test::aMultiAddStagesEveryFixtureUnpatched()
{
    driveAddDialog([](AddFixture *dlg) {
        dlg->m_nameEdit->setText("Step");
        dlg->slotNameEdited("Step");
        dlg->m_amountSpin->setValue(4);
    });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                              InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(made.size(), 4);
    foreach (quint32 id, made)
        QVERIFY2(!m_doc->fixture(id)->isPatched(), "every fixture of a multi-add must be staged");
    QCOMPARE(m_doc->fixtureForAddress(0), Fixture::invalidId());
}

static QStringList namesOf(Doc *doc, const QList<quint32> &ids)
{
    QStringList out;
    foreach (quint32 id, ids)
        out << doc->fixture(id)->name();
    return out;
}

void FixtureCreator_Test::aMultiAddOfANumberedModelAppendsACounter()
{
    driveAddDialog([](AddFixture *dlg) {
        dlg->m_nameEdit->setText("LM70");
        dlg->slotNameEdited("LM70");
        dlg->m_amountSpin->setValue(3);
    });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                              InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(namesOf(m_doc, made), (QStringList{ "LM70 #1", "LM70 #2", "LM70 #3" }));

    // A model like "Par 64" must not become "Par 65" either.
    driveAddDialog([](AddFixture *dlg) {
        dlg->m_nameEdit->setText("Par 64");
        dlg->slotNameEdited("Par 64");
        dlg->m_amountSpin->setValue(2);
    });
    const QList<quint32> par = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                             InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(namesOf(m_doc, par), (QStringList{ "Par 64 #1", "Par 64 #2" }));
}

void FixtureCreator_Test::anExplicitCounterStillContinues()
{
    driveAddDialog([](AddFixture *dlg) {
        dlg->m_nameEdit->setText("US #4");
        dlg->slotNameEdited("US #4");
        dlg->m_amountSpin->setValue(3);
    });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                              InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(namesOf(m_doc, made), (QStringList{ "US #4", "US #5", "US #6" }));
}

void FixtureCreator_Test::aTakenDefaultNameGetsACounterNotBumpedDigits()
{
    // The dialog seeds the name from the selection ("Dimmers" for a generic
    // dimmer). Adding twice must give "Dimmers" then "Dimmers #2".
    driveAddDialog([](AddFixture *) {});
    const QList<quint32> first = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                               InputOutputMap::invalidUniverse(), nullptr);
    driveAddDialog([](AddFixture *) {});
    const QList<quint32> second = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                                InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(namesOf(m_doc, first), QStringList{ "Dimmers" });
    QCOMPARE(namesOf(m_doc, second), QStringList{ "Dimmers #2" });
}


void FixtureCreator_Test::aPreselectedUniverseIsWhereTheFixtureLands()
{
    driveAddDialog([](AddFixture *dlg) {
        QCOMPARE(dlg->m_universeCombo->currentIndex(), 2);
    });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 2, nullptr);
    QCOMPARE(made.size(), 1);
    QCOMPARE(m_doc->fixture(made.first())->universe(), quint32(2));

    // Inventory ignores it: nothing to preselect for a fixture with no universe.
    driveAddDialog([](AddFixture *dlg) {
        QCOMPARE(dlg->m_universeCombo->currentIndex(), 0);
    });
    QCOMPARE(FixtureCreator::addFromDialog(nullptr, m_doc, true, 2, nullptr).size(), 1);
}

void FixtureCreator_Test::anInvalidAddressIsRefusedWithAMessage()
{
    bool sawMessage = false;
    auto closeBox = std::make_shared<std::function<void()>>();
    *closeBox = [&sawMessage, closeBox]() {
        QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (box == nullptr)
        {
            QTimer::singleShot(10, *closeBox);
            return;
        }
        sawMessage = box->text().contains("valid address");
        box->accept();
    };
    driveAddDialog([closeBox](AddFixture *dlg) {
        dlg->m_invalidAddressFlag = true;
        QTimer::singleShot(0, *closeBox);
    });
    QVERIFY(FixtureCreator::addFromDialog(nullptr, m_doc, false,
                                          InputOutputMap::invalidUniverse(), nullptr).isEmpty());
    QVERIFY(sawMessage);
    QCOMPARE(m_doc->fixturesCount(), 0);
}

void FixtureCreator_Test::aCancelledDialogCreatesNothing()
{
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [poll]() {
        AddFixture *dlg = qobject_cast<AddFixture *>(QApplication::activeModalWidget());
        if (dlg == nullptr)
            QTimer::singleShot(10, *poll);
        else
            dlg->reject();
    };
    QTimer::singleShot(0, *poll);
    QVERIFY(FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                          InputOutputMap::invalidUniverse(), nullptr).isEmpty());
    QCOMPARE(m_doc->fixturesCount(), 0);
}

void FixtureCreator_Test::anEmptyNameFallsBackToTheModelOrGenericDimmer()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Blinder 4");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("4ch");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setGroup(QLCChannel::Intensity);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));

    driveAddDialog([def, mode](AddFixture *dlg) {
        dlg->m_fixtureDef = def;
        dlg->m_mode = mode;
        dlg->m_nameValue.clear();
    });
    QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                        InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(made.size(), 1);
    QCOMPARE(m_doc->fixture(made.first())->name(), QString("Blinder 4"));
    QCOMPARE(m_doc->fixture(made.first())->fixtureMode(), mode);

    driveAddDialog([](AddFixture *dlg) {
        dlg->m_fixtureDef = nullptr;
        dlg->m_mode = nullptr;
        dlg->m_nameValue = "   ";
    });
    made = FixtureCreator::addFromDialog(nullptr, m_doc, true, InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(made.size(), 1);
    QCOMPARE(m_doc->fixture(made.first())->name(), QString("Generic Dimmer"));
}

void FixtureCreator_Test::newFixturesJoinTheGroupTheyWereAddedFrom()
{
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Fence");
    grp->setSize(QSize(4, 1));
    QVERIFY(m_doc->addFixtureGroup(grp));

    driveAddDialog([](AddFixture *dlg) {
        dlg->m_amountSpin->setValue(3);
        dlg->m_amountValue = 3;
    });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                              InputOutputMap::invalidUniverse(), grp);
    QCOMPARE(made.size(), 3);
    for (quint32 id : made)
        QVERIFY2(grp->fixtureList().contains(id), qPrintable(QString("fixture %1 not in the group").arg(id)));
}

void FixtureCreator_Test::aFixtureTheDocRefusesIsNotLeaked()
{
    // Someone already sits at 1/1; force the dialog's address onto it.
    Fixture *taken = new Fixture(m_doc);
    taken->setChannels(4);
    QVERIFY(m_doc->addFixture(taken));

    driveAddDialog([](AddFixture *dlg) {
        dlg->m_universeValue = 0;
        dlg->m_addressValue = 0;
        dlg->m_invalidAddressFlag = false;
    });
    QVERIFY(FixtureCreator::addFromDialog(nullptr, m_doc, false,
                                          InputOutputMap::invalidUniverse(), nullptr).isEmpty());
    QCOMPARE(m_doc->fixturesCount(), 1);
}

QTEST_MAIN(FixtureCreator_Test)

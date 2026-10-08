/*
  Q Light Controller
  addfixture_test.cpp

  Copyright (c) Heikki Junnila

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
#include <QPushButton>
#include <QDialogButtonBox>
#include <QList>

#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "outputpatch.h"
#include "qlcfile.h"
#include "fixture.h"
#include "doc.h"

#include "addfixture_test.h"
#define protected public
#include "addfixture.h"
#undef protected

#include "../../../engine/test/common/resource_paths.h"

void AddFixture_Test::initTestCase()
{
    m_doc = new Doc(this);

    QSettings settings;
    m_expanded = settings.value(SETTINGS_EXPANDED);

    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir) == true);
}

void AddFixture_Test::cleanupTestCase()
{
    QSettings settings;
    if (m_expanded.isValid())
    {
        settings.setValue(SETTINGS_EXPANDED, m_expanded);
    }
    else
    {
        settings.remove(SETTINGS_EXPANDED);
    }

    delete m_doc;
}

void AddFixture_Test::findAddress()
{
    QList <Fixture*> fixtures;

    /* All addresses are available (except for fixtures taking more than
       one complete universe). */
    QVERIFY(AddFixture::findAddress(15, fixtures, 4) == 0);
    QVERIFY(AddFixture::findAddress(0, fixtures, 4) == QLCChannel::invalid());
    QVERIFY(AddFixture::findAddress(512, fixtures, 4) == 0);
    QVERIFY(AddFixture::findAddress(513, fixtures, 4) == QLCChannel::invalid());

    Fixture* f1 = new Fixture(m_doc);
    f1->setChannels(15);
    f1->setAddress(10);
    f1->setID(1);
    fixtures << f1;

    /* There's a fixture taking 15 channels (10-24) */
    QVERIFY(AddFixture::findAddress(10, fixtures, 4) == 0);
    QVERIFY(AddFixture::findAddress(11, fixtures, 4) == 25);

    Fixture* f2 = new Fixture(m_doc);
    f2->setChannels(15);
    f2->setAddress(10);
    f2->setID(2);
    fixtures << f2;

    /* Now there are two fixtures at the same address, with all channels
       overlapping. */
    QVERIFY(AddFixture::findAddress(10, fixtures, 4) == 0);
    QVERIFY(AddFixture::findAddress(11, fixtures, 4) == 25);

    /* Now only some channels overlap (f2: 0-14, f1: 10-24) */
    f2->setAddress(0);
    QVERIFY(AddFixture::findAddress(1, fixtures, 4) == 25);
    QVERIFY(AddFixture::findAddress(10, fixtures, 4) == 25);
    QVERIFY(AddFixture::findAddress(11, fixtures, 4) == 25);

    Fixture* f3 = new Fixture(m_doc);
    f3->setChannels(5);
    f3->setAddress(30);
    f3->setID(3);
    fixtures << f3;

    /* Next free slot for max 5 channels is between 25 and 30 */
    QVERIFY(AddFixture::findAddress(1, fixtures, 4) == 25);
    QVERIFY(AddFixture::findAddress(5, fixtures, 4) == 25);
    QVERIFY(AddFixture::findAddress(6, fixtures, 4) == 35);
    QVERIFY(AddFixture::findAddress(11, fixtures, 4) == 35);

    /* Next free slot is found only from the next universe */
    QVERIFY(AddFixture::findAddress(500, fixtures, 4) == 512);

    /* ignore fixture f2 (@0) */
    QVERIFY(AddFixture::findAddress(0, 10, fixtures) > 0);
    QVERIFY(AddFixture::findAddress(0, 10, fixtures, f2->id()) == 0);

    while (fixtures.isEmpty() == false)
        delete fixtures.takeFirst();
}

void AddFixture_Test::initialNoFixture()
{
    AddFixture af(NULL, m_doc);
    QVERIFY(m_doc == af.m_doc);
    QVERIFY(af.fixtureDef() == NULL);
    QVERIFY(af.mode() == NULL);
    QCOMPARE(af.name(), tr("Dimmers"));
    QVERIFY(af.address() == 0);
    QVERIFY(af.universe() == 0);
    QVERIFY(af.amount() == 1);
    QVERIFY(af.gap() == 0);
    QVERIFY(af.channels() == 1);
    QVERIFY(af.m_tree->columnCount() == 2);      // Fixture Model, Source

    // Check that all makes & models are put to the tree
    QStringList makers(m_doc->fixtureDefCache()->manufacturers());
    QVERIFY(makers.isEmpty() == false);
    for (int i = 0; i < af.m_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* top = af.m_tree->topLevelItem(i);

        if (top->text(0) != KXMLFixtureGeneric)
        {
            QStringList models(m_doc->fixtureDefCache()->models(top->text(0)));
            for (int j = 0; j < top->childCount(); j++)
            {
                QTreeWidgetItem* child = top->child(j);
                QCOMPARE(child->childCount(), 0);
                QCOMPARE(models.removeAll(child->text(0)), 1);
            }

            QCOMPARE(makers.removeAll(top->text(0)), 1);
        }
        else
        {
            QCOMPARE(i, af.m_tree->topLevelItemCount() - 1); // Generic should be last
            QCOMPARE(top->childCount(), 4);
            QCOMPARE(top->child(0)->text(0), QString(KXMLFixtureGeneric));

            QStringList models(m_doc->fixtureDefCache()->models(top->text(0)));
            for (int j = 0; j < top->childCount(); j++)
            {
                QTreeWidgetItem* child = top->child(j);
                QCOMPARE(child->childCount(), 0);
                QCOMPARE(models.removeAll(child->text(0)), child->text(0) == KXMLFixtureGeneric ? 0 : 1);
            }

            QCOMPARE(makers.removeAll(top->text(0)), 1);
        }
    }
    QVERIFY(makers.isEmpty() == true);

    // Generic / Generic should be selected by default
    QVERIFY(af.m_tree->currentItem() != NULL);
    QCOMPARE(af.m_tree->currentItem()->text(0), QString(KXMLFixtureGeneric));
    QVERIFY(af.m_tree->currentItem()->parent() != NULL);
    QCOMPARE(af.m_tree->currentItem()->parent()->text(0), QString(KXMLFixtureGeneric));

    QVERIFY(af.m_modeCombo->isEnabled() == false);
    QCOMPARE(af.m_modeCombo->count(), 1);
    QCOMPARE(af.m_modeCombo->itemText(0), QString(/*KXMLFixtureGeneric*/));

    QVERIFY(af.m_universeCombo->isEnabled() == true);
    QCOMPARE(af.m_universeCombo->currentIndex(), 0);
    QCOMPARE(af.m_universeCombo->count(), 4);

    QVERIFY(af.m_addressSpin->isEnabled() == true);
    QCOMPARE(af.m_addressSpin->value(), 1);
    QCOMPARE(af.m_addressSpin->minimum(), 1);
    QCOMPARE(af.m_addressSpin->maximum(), 512);

    QVERIFY(af.m_channelsSpin->isEnabled() == true);
    QCOMPARE(af.m_channelsSpin->value(), 1);

    QVERIFY(af.m_nameEdit->isEnabled() == true);
    QCOMPARE(af.m_nameEdit->text(), QString(KXMLFixtureDimmer + QString("s")));
    QVERIFY(af.m_nameEdit->isModified() == false);

    QVERIFY(af.m_multipleGroup->isEnabled() == true);
    QVERIFY(af.m_gapSpin->isEnabled() == true);
    QCOMPARE(af.m_gapSpin->value(), 0);
    QVERIFY(af.m_amountSpin->isEnabled() == true);
    QCOMPARE(af.m_amountSpin->value(), 1);
}

void AddFixture_Test::initialDimmer()
{
    Fixture* fxi = new Fixture(m_doc);
    fxi->setChannels(6);
    fxi->setName("My dimmer");
    fxi->setUniverse(2);
    fxi->setAddress(484);
    m_doc->addFixture(fxi);

    AddFixture af(NULL, m_doc, fxi);
    QVERIFY(m_doc == af.m_doc);
    QVERIFY(af.fixtureDef() != NULL);
    QVERIFY(af.mode() != NULL);
    QVERIFY(af.name() == QString("My dimmer"));
    QVERIFY(af.address() == 484);
    QVERIFY(af.universe() == 2);
    QVERIFY(af.amount() == 1);
    QVERIFY(af.gap() == 0);
    QVERIFY(af.channels() == 6);

    // Check that all makes & models are put to the tree
    QStringList makers(m_doc->fixtureDefCache()->manufacturers());
    QVERIFY(makers.isEmpty() == false);
    for (int i = 0; i < af.m_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* top = af.m_tree->topLevelItem(i);

        if (top->text(0) != KXMLFixtureGeneric)
        {
            QStringList models(m_doc->fixtureDefCache()->models(top->text(0)));
            for (int j = 0; j < top->childCount(); j++)
            {
                QTreeWidgetItem* child = top->child(j);
                QCOMPARE(child->childCount(), 0);
                QCOMPARE(models.removeAll(child->text(0)), 1);
            }

            QCOMPARE(makers.removeAll(top->text(0)), 1);
        }
        else
        {
            QCOMPARE(i, af.m_tree->topLevelItemCount() - 1); // Generic should be last
            QCOMPARE(top->childCount(), 4);
            QCOMPARE(top->child(0)->text(0), QString(KXMLFixtureGeneric));

            QStringList models(m_doc->fixtureDefCache()->models(top->text(0)));
            for (int j = 0; j < top->childCount(); j++)
            {
                QTreeWidgetItem* child = top->child(j);
                QCOMPARE(child->childCount(), 0);
                QCOMPARE(models.removeAll(child->text(0)), child->text(0) == KXMLFixtureGeneric ? 0 : 1);
            }

            QCOMPARE(makers.removeAll(top->text(0)), 1);
        }
    }
    QVERIFY(makers.isEmpty() == true);

    // Generic / Generic should be selected for dimmers
    QVERIFY(af.m_tree->currentItem() != NULL);
    QCOMPARE(af.m_tree->currentItem()->text(0), QString(KXMLFixtureGeneric));
    QVERIFY(af.m_tree->currentItem()->parent() != NULL);
    QCOMPARE(af.m_tree->currentItem()->parent()->text(0), QString(KXMLFixtureGeneric));

    QVERIFY(af.m_modeCombo->isEnabled() == false);
    QCOMPARE(af.m_modeCombo->count(), 1);
    QCOMPARE(af.m_modeCombo->itemText(0), QString("6 Channel"));

    QVERIFY(af.m_universeCombo->isEnabled() == true);
    QCOMPARE(af.m_universeCombo->currentIndex(), 2);
    QCOMPARE(af.m_universeCombo->count(), 4);

    QVERIFY(af.m_addressSpin->isEnabled() == true);
    QCOMPARE(af.m_addressSpin->value(), 485);
    QCOMPARE(af.m_addressSpin->minimum(), 1);
    QCOMPARE(af.m_addressSpin->maximum(), int(513 - fxi->channels()));

    QVERIFY(af.m_channelsSpin->isEnabled() == true);
    QCOMPARE(af.m_channelsSpin->value(), 6);

    QVERIFY(af.m_nameEdit->isEnabled() == true);
    QCOMPARE(af.m_nameEdit->text(), QString("My dimmer"));
    QVERIFY(af.m_nameEdit->isModified() == true);

    QVERIFY(af.m_multipleGroup->isEnabled() == false);
    QVERIFY(af.m_gapSpin->isEnabled() == false);
    QCOMPARE(af.m_gapSpin->value(), 0);

    QVERIFY(af.m_amountSpin->isEnabled() == false);
    QCOMPARE(af.m_amountSpin->value(), 1);
}

void AddFixture_Test::initialScanner()
{
    Fixture* fxi = new Fixture(m_doc);
    fxi->setName("My scanner");

    QLCFixtureDef* def = m_doc->fixtureDefCache()->fixtureDef("Martin", "MAC300");
    Q_ASSERT(def != NULL);
    Q_ASSERT(def != NULL);
    Q_ASSERT(def->channels().size() > 0);
    QLCFixtureMode* mode = def->modes().first();
    Q_ASSERT(def->modes().size() > 1);

    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(3);
    fxi->setAddress(484);
    m_doc->addFixture(fxi);

    AddFixture af(NULL, m_doc, fxi);
    QVERIFY(m_doc == af.m_doc);
    QVERIFY(af.fixtureDef() == def);
    QVERIFY(af.mode() == mode);
    QVERIFY(af.name() == QString("My scanner"));
    QVERIFY(af.address() == 484);
    QVERIFY(af.universe() == 3);
    QVERIFY(af.amount() == 1);
    QVERIFY(af.gap() == 0);
    QVERIFY(af.channels() == fxi->channels());

    // Check that all makes & models are put to the tree
    QStringList makers(m_doc->fixtureDefCache()->manufacturers());
    QVERIFY(makers.isEmpty() == false);
    for (int i = 0; i < af.m_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* top = af.m_tree->topLevelItem(i);

        if (top->text(0) != KXMLFixtureGeneric)
        {
            QStringList models(m_doc->fixtureDefCache()->models(top->text(0)));
            for (int j = 0; j < top->childCount(); j++)
            {
                QTreeWidgetItem* child = top->child(j);
                QCOMPARE(child->childCount(), 0);
                QCOMPARE(models.removeAll(child->text(0)), 1);
            }

            QCOMPARE(makers.removeAll(top->text(0)), 1);
        }
        else
        {
            QCOMPARE(i, af.m_tree->topLevelItemCount() - 1); // Generic should be last
            QCOMPARE(top->childCount(), 4);
            QCOMPARE(top->child(0)->text(0), QString(KXMLFixtureGeneric));

            QStringList models(m_doc->fixtureDefCache()->models(top->text(0)));
            for (int j = 0; j < top->childCount(); j++)
            {
                QTreeWidgetItem* child = top->child(j);
                QCOMPARE(child->childCount(), 0);
                QCOMPARE(models.removeAll(child->text(0)), child->text(0) == KXMLFixtureGeneric ? 0 : 1);
            }

            QCOMPARE(makers.removeAll(top->text(0)), 1);
        }
    }
    QVERIFY(makers.isEmpty() == true);

    // Generic / Generic should be selected for dimmers
    QVERIFY(af.m_tree->currentItem() != NULL);
    QCOMPARE(af.m_tree->currentItem()->text(0), def->model());
    QVERIFY(af.m_tree->currentItem()->parent() != NULL);
    QCOMPARE(af.m_tree->currentItem()->parent()->text(0), def->manufacturer());

    QVERIFY(af.m_modeCombo->isEnabled() == true);
    QCOMPARE(af.m_modeCombo->count(), def->modes().size());
    QCOMPARE(af.m_modeCombo->itemText(0), mode->name());

    QVERIFY(af.m_universeCombo->isEnabled() == true);
    QCOMPARE(af.m_universeCombo->currentIndex(), 3);
    QCOMPARE(af.m_universeCombo->count(), 4);

    QVERIFY(af.m_addressSpin->isEnabled() == true);
    QCOMPARE(af.m_addressSpin->value(), 485);

    QVERIFY(af.m_channelsSpin->isEnabled() == false);
    QCOMPARE(af.m_channelsSpin->value(), (int) fxi->channels());

    QVERIFY(af.m_nameEdit->isEnabled() == true);
    QCOMPARE(af.m_nameEdit->text(), QString("My scanner"));
    QVERIFY(af.m_nameEdit->isModified() == true);

    QVERIFY(af.m_multipleGroup->isEnabled() == false);
    QVERIFY(af.m_gapSpin->isEnabled() == false);
    QCOMPARE(af.m_gapSpin->value(), 0);

    QVERIFY(af.m_amountSpin->isEnabled() == false);
    QCOMPARE(af.m_amountSpin->value(), 1);
}

void AddFixture_Test::selectionNothing()
{
    AddFixture af(NULL, m_doc);

    af.m_tree->setCurrentItem(NULL);
    QVERIFY(af.m_tree->currentItem() == NULL);

    QVERIFY(af.fixtureDef() == NULL);
    QVERIFY(af.mode() == NULL);
    QVERIFY(af.name() == "");
    QVERIFY(af.address() == 0);
    QVERIFY(af.universe() == 0);
    QVERIFY(af.amount() == 1);
    QVERIFY(af.gap() == 0);
    QVERIFY(af.channels() == 1);

    QVERIFY(af.m_modeCombo->isEnabled() == false);
    QCOMPARE(af.m_modeCombo->count(), 1);
    QCOMPARE(af.m_modeCombo->itemText(0), QString());

    QVERIFY(af.m_universeCombo->isEnabled() == false);
    QCOMPARE(af.m_universeCombo->currentIndex(), 0);
    QCOMPARE(af.m_universeCombo->count(), 4);

    QVERIFY(af.m_addressSpin->isEnabled() == false);
    QCOMPARE(af.m_addressSpin->value(), 1);
    QCOMPARE(af.m_addressSpin->minimum(), 1);
    QCOMPARE(af.m_addressSpin->maximum(), 512);

    QVERIFY(af.m_channelsSpin->isEnabled() == false);
    QCOMPARE(af.m_channelsSpin->value(), 1);

    QVERIFY(af.m_nameEdit->isEnabled() == false);

    QVERIFY(af.m_multipleGroup->isEnabled() == false);
    QVERIFY(af.m_gapSpin->isEnabled() == false);
    QCOMPARE(af.m_gapSpin->value(), 0);
    QVERIFY(af.m_amountSpin->isEnabled() == false);
}

void AddFixture_Test::selectionGeneric()
{
    AddFixture af(NULL, m_doc);

    // Select the last item which should be Generic - Generic
    QTreeWidgetItem* item = af.m_tree->topLevelItem(af.m_tree->topLevelItemCount() - 1);
    QVERIFY(item != NULL);
    // First, select the parent node so that selectionChanged() fires
    QCOMPARE(item->childCount(), 4);
    af.m_tree->setCurrentItem(item);
    // Then, select the child to fire again
    item = item->child(0);
    QVERIFY(item != NULL);
    af.m_tree->setCurrentItem(item);
    QVERIFY(af.m_tree->currentItem() == item);

    QVERIFY(af.m_modeCombo->isEnabled() == false);
    QCOMPARE(af.m_modeCombo->count(), 1);
    QCOMPARE(af.m_modeCombo->itemText(0), QString(/*KXMLFixtureGeneric*/));
    QVERIFY(af.fixtureDef() == NULL);
    QVERIFY(af.mode() == NULL);

    QVERIFY(af.m_universeCombo->isEnabled() == true);
    QCOMPARE(af.m_universeCombo->currentIndex(), 0);
    QCOMPARE(af.m_universeCombo->count(), 4);
    QVERIFY(af.universe() == 0);

    QVERIFY(af.m_addressSpin->isEnabled() == true);
    QCOMPARE(af.m_addressSpin->value(), 1);
    QCOMPARE(af.m_addressSpin->minimum(), 1);
    QCOMPARE(af.m_addressSpin->maximum(), 512);
    QVERIFY(af.address() == 0);

    QVERIFY(af.m_channelsSpin->isEnabled() == true);
    QCOMPARE(af.m_channelsSpin->value(), 1);
    QVERIFY(af.channels() == 1);

    QVERIFY(af.m_nameEdit->isEnabled() == true);
    QCOMPARE(af.name(), QString(KXMLFixtureDimmer) + QString("s"));

    QVERIFY(af.m_multipleGroup->isEnabled() == true);
    QVERIFY(af.m_gapSpin->isEnabled() == true);
    QCOMPARE(af.m_gapSpin->value(), 0);
    QVERIFY(af.gap() == 0);

    QVERIFY(af.m_amountSpin->isEnabled() == true);
    QVERIFY(af.m_amountSpin->value() == 1);
    QVERIFY(af.amount() == 1);
}


void AddFixture_Test::rememberExpanded()
{
    {
        AddFixture af(NULL, m_doc);
        af.m_tree->invisibleRootItem()->child(5)->setExpanded(true);
    }

    {
        AddFixture af(NULL, m_doc);
        QVERIFY(af.m_tree->invisibleRootItem()->child(5)->isExpanded());
    }
}

void AddFixture_Test::aTakenNameIsRefusedWhenEditingAndNumberedWhenAdding()
{
    Fixture *a = new Fixture(m_doc);
    a->setName("AT1-3z");
    a->setChannels(1);
    QVERIFY(m_doc->addFixture(a));
    Fixture *b = new Fixture(m_doc);
    b->setName("AT1-36-1");
    b->setChannels(1);
    b->setAddress(10);
    QVERIFY(m_doc->addFixture(b));
    {
        // Changing b's properties: renaming it to a's name is refused.
        AddFixture af(NULL, m_doc, b);
        QPushButton *ok = af.m_buttonBox->button(QDialogButtonBox::Ok);
        QVERIFY(ok != NULL && ok->isEnabled());
        af.m_nameEdit->setText("AT1-3z");
        af.slotNameEdited("AT1-3z");
        QVERIFY(!af.m_nameErrorLabel->isHidden());
        QVERIFY(af.m_nameErrorLabel->text().contains("Another fixture is already called this"));
        QVERIFY(!af.m_buttonBox->button(QDialogButtonBox::Ok)->isEnabled());
        // Its own name is fine.
        af.m_nameEdit->setText("AT1-36-1");
        af.slotNameEdited("AT1-36-1");
        QVERIFY(af.m_nameErrorLabel->isHidden());
        QVERIFY(af.m_buttonBox->button(QDialogButtonBox::Ok)->isEnabled());
    }
    {
        // Adding one: allowed, and the label says the name it will get.
        AddFixture af(NULL, m_doc);
        af.m_nameEdit->setText("AT1-3z");
        af.slotNameEdited("AT1-3z");
        QVERIFY(!af.m_nameErrorLabel->isHidden());
        QVERIFY2(af.m_nameErrorLabel->text().contains("AT1-3z #2"), qPrintable(af.m_nameErrorLabel->text()));
    }
    m_doc->deleteFixture(a->id());
    m_doc->deleteFixture(b->id());
}

QTEST_MAIN(AddFixture_Test)

void AddFixture_Test::inventoryModeHidesAddressAndIgnoresOverlap()
{
    // Occupy universe 1's first channels so a normal add there would overlap.
    Fixture *taken = new Fixture(m_doc);
    taken->setChannels(512);
    QVERIFY(m_doc->addFixture(taken));

    AddFixture af(NULL, m_doc);
    af.setInventoryMode(true);
    QVERIFY(af.inventoryMode());
    QVERIFY(af.m_universeCombo->isHidden());
    QVERIFY(af.m_addressSpin->isHidden());
    QVERIFY(af.m_gapSpin->isHidden());
    QVERIFY(af.windowTitle().contains("Inventory"));
    // The channel count is a footprint decision: made when patching, not here.
    QVERIFY(af.m_channelsSpin->isHidden());
    QVERIFY(af.m_channelsLabel->isHidden());

    // A staged fixture claims no address, so a full universe is no obstacle.
    af.m_addressSpin->setValue(1);
    QVERIFY(!af.invalidAddress());

    m_doc->deleteFixture(taken->id());
}

void AddFixture_Test::connectionAndIpAreReported()
{
    AddFixture af(NULL, m_doc);
    QCOMPARE(af.connection(), Fixture::Dmx);
    QVERIFY(af.ipAddress().isEmpty());
    QVERIFY(!af.m_ipEdit->isEnabled());              // only a network fixture has an IP

    af.m_ipEdit->setText("172.18.2.41");
    QVERIFY2(af.ipAddress().isEmpty(), "a DMX fixture must not carry an IP");

    af.m_connectionCombo->setCurrentIndex(af.m_connectionCombo->findData(
        AddFixture::connectionKey(Fixture::Network, Fixture::ArtNetTransport)));
    QCOMPARE(af.connection(), Fixture::Network);
    QVERIFY(af.m_ipEdit->isEnabled());
    QCOMPARE(af.ipAddress(), QString("172.18.2.41"));
}

void AddFixture_Test::findAddressIgnoresUnpatchedFixtures()
{
    // An inventory fixture sits on the placeholder 0/0 and claims nothing: a
    // new fixture still goes to address 1, not after the placeholder.
    QList <Fixture*> fixtures;
    Fixture *staged = new Fixture(m_doc);
    staged->setChannels(24);
    staged->setPatched(false);
    fixtures << staged;
    QCOMPARE(AddFixture::findAddress(8, fixtures, 4), quint32(0));
    staged->setPatched(true);
    QCOMPARE(AddFixture::findAddress(8, fixtures, 4), quint32(24));
    delete staged;
}

void AddFixture_Test::inventoryQuantityIsNotLimitedByAUniverse()
{
    // "why can't I add more than 1 fixture here?": a 165-channel fixture with
    // the (hidden) address where the auto-search put it left room for one.
    AddFixture af(NULL, m_doc);
    af.m_channelsSpin->setValue(165);
    af.m_addressSpin->setValue(300);
    QCOMPARE(af.m_amountSpin->maximum(), 1);          // patching: one fits there

    af.setInventoryMode(true);
    QVERIFY(af.m_amountSpin->maximum() >= 100);
    af.m_amountSpin->setValue(16);
    QCOMPARE(af.amount(), 16);

    // And it stays lifted when the hidden address or footprint moves.
    af.m_addressSpin->setValue(400);
    af.m_channelsSpin->setValue(200);
    QCOMPARE(af.amount(), 16);
}

void AddFixture_Test::addingMoreStartsOnTheSameModelAndMode()
{
    // "when I right click on a model .. it should auto select that model from
    // the list for adding": the dialog starts on the model AND the mode.
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef("Martin", "MAC300");
    QVERIFY(def != NULL);
    QVERIFY(def->modes().size() > 1);
    QLCFixtureMode *second = def->modes().at(1);
    Fixture *like = new Fixture(m_doc);
    like->setFixtureDefinition(def, second);

    AddFixture af(NULL, m_doc);
    QVERIFY(af.fixtureDef() != def);                  // Generic by default
    af.preselectLike(like);
    af.setInventoryMode(true);
    QCOMPARE(af.fixtureDef(), def);
    QCOMPARE(af.mode(), second);

    // A manufacturer's folder: that manufacturer, opened.
    AddFixture bf(NULL, m_doc);
    bf.preselectManufacturer("Martin");
    QTreeWidgetItem *martin = NULL;
    for (int i = 0; i < bf.m_tree->topLevelItemCount(); ++i)
        if (bf.m_tree->topLevelItem(i)->text(0) == "Martin")
            martin = bf.m_tree->topLevelItem(i);
    QVERIFY(martin != NULL);
    QVERIFY(martin->isExpanded());

    delete like;
}

/* Included, Local, or (one day) Downloadable: each model in the list says
   where its definition comes from. */
void AddFixture_Test::eachModelSaysWhereItsDefinitionComesFrom()
{
    QLCFixtureDef *local = new QLCFixtureDef();
    local->setManufacturer("Zz Test");
    local->setModel("My Own Bar");
    local->setSource(QLCFixtureDef::Local);
    local->setOverridesIncluded(true);
    QLCFixtureMode *mode = new QLCFixtureMode(local);
    mode->setName("1ch");
    local->addMode(mode);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(local));

    AddFixture af(NULL, m_doc);
    QCOMPARE(af.m_tree->columnCount(), 2);
    QList<QTreeWidgetItem *> found = af.m_tree->findItems("My Own Bar", Qt::MatchExactly | Qt::MatchRecursive);
    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first()->text(1), QString("Local — replaces the included one"));
    found = af.m_tree->findItems("MAC 250", Qt::MatchStartsWith | Qt::MatchRecursive);
    QVERIFY(!found.isEmpty());
    QCOMPARE(found.first()->text(1), QString("Included"));
}

/* Network fixtures go as Art-Net -- or, for WLED on Wi-Fi, through WLED's own
   API. The choice is offered, read back, and shown again when edited. */
void AddFixture_Test::aNetworkFixtureCanBeSentToByWled()
{
    {
        AddFixture af(NULL, m_doc);
        const int wled = af.m_connectionCombo->findText("Network (WLED, JSON over Wi-Fi)");
        QVERIFY(wled >= 0);
        af.m_connectionCombo->setCurrentIndex(wled);
        QCOMPARE(af.connection(), Fixture::Network);
        QCOMPARE(af.transport(), Fixture::WledTransport);
        QVERIFY(af.m_ipEdit->isEnabled());
        af.m_connectionCombo->setCurrentIndex(af.m_connectionCombo->findText("DMX"));
        QCOMPARE(af.connection(), Fixture::Dmx);
        QVERIFY(!af.m_ipEdit->isEnabled());
    }
    Fixture *lantern = new Fixture(m_doc);
    lantern->setName("Lantern SL");
    lantern->setChannels(4);
    lantern->setAddress(100);
    lantern->setConnection(Fixture::Network);
    lantern->setIPAddress("192.168.20.185");
    lantern->setTransport(Fixture::WledTransport);
    QVERIFY(m_doc->addFixture(lantern));
    AddFixture edit(NULL, m_doc, lantern);
    QCOMPARE(edit.transport(), Fixture::WledTransport);
    QCOMPARE(edit.ipAddress(), QString("192.168.20.185"));
}

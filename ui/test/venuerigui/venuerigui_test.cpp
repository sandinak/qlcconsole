/*
  Q Light Controller Plus
  venuerigui_test.cpp

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

#include <QTableWidget>
#include <QListWidget>
#include <QPushButton>
#include <QComboBox>
#include <QPointer>
#include <QTimer>
#include <QLabel>
#include <QtTest>

#include "venuerigui_test.h"
#include "programmercontroller.h"
#include "fixturegroupeditor.h"
#include "venuecorrections.h"
#include "focuscallsdialog.h"
#include "inputoutputmap.h"
#include "venuerigdialog.h"
#include "fixturegroup.h"
#include "rigowner.h"
#include "fixture.h"
#include "doc.h"

namespace
{
quint32 dimmer(Doc *doc, const QString &name, quint32 universe, quint32 address)
{
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    fx->setUniverse(universe);
    fx->setAddress(address);
    fx->setChannels(1);
    if (!doc->addFixture(fx))
    {
        delete fx;
        return Fixture::invalidId();
    }
    return fx->id();
}

VenueCorrections *venues(Doc *doc)
{
    return doc->programmer()->venueCorrections();
}
}

void VenueRigUi_Test::init()
{
    qputenv("QLC_NO_VOICE", "1");
    m_dir = new QTemporaryDir();
    m_doc = new Doc(this);
    venues(m_doc)->setShowFile(m_dir->filePath(QStringLiteral("Tour.qlcc")), false);
}

void VenueRigUi_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
    delete m_dir;
    m_dir = nullptr;
}

void VenueRigUi_Test::aGroupsOwnerIsSetInItsEditor()
{
    const quint32 a = dimmer(m_doc, QStringLiteral("Lincoln par 1"), 0, 100);
    const quint32 b = dimmer(m_doc, QStringLiteral("Lincoln par 2"), 0, 101);
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName(QStringLiteral("FOH wash"));
    m_doc->addFixtureGroup(grp);
    grp->assignFixture(a);
    grp->assignFixture(b);
    QVERIFY(venues(m_doc)->setActiveVenue(venues(m_doc)->addVenue(QStringLiteral("Lincoln HS"))));
    m_doc->resetModified();

    FixtureGroupEditor editor(grp, m_doc, nullptr);
    QComboBox *owner = editor.findChild<QComboBox *>(QStringLiteral("fgOwner"));
    QVERIFY(owner != nullptr);
    QCOMPARE(owner->currentData().toInt(), int(RigOwner::Unset));
    QCOMPARE(owner->count(), 4);     // Not set, Tour, House, Rental

    owner->setCurrentIndex(owner->findData(int(RigOwner::House)));
    QCOMPARE(grp->owner(), RigOwner::House);
    QVERIFY(m_doc->isModified());
    // Its fixtures follow it, into the venue applied.
    QVERIFY(venues(m_doc)->isVenueFixture(a));
    QVERIFY(venues(m_doc)->isVenueFixture(b));
    QCOMPARE(venues(m_doc)->venue(QStringLiteral("Lincoln HS")).rig.fixtures.size(), 2);

    // Changed elsewhere, the editor follows.
    grp->setOwner(RigOwner::Rental);
    QCOMPARE(owner->currentData().toInt(), int(RigOwner::Rental));
    QVERIFY(!venues(m_doc)->isVenueFixture(a));
}

void VenueRigUi_Test::theVenuePageShowsTheUniverses()
{
    VenueCorrections *v = venues(m_doc);
    const quint32 tour = dimmer(m_doc, QStringLiteral("Tour spot"), 0, 0);
    const QString venue = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(venue));
    const quint32 par = dimmer(m_doc, QStringLiteral("Lincoln par"), 0, 100);
    m_doc->fixture(par)->setOwner(RigOwner::House);
    QVERIFY(v->isVenueFixture(par));

    VenueRigDialog page(m_doc, venue, nullptr);
    QLabel *house = page.findChild<QLabel *>(QStringLiteral("vrHouse"));
    QVERIFY(house != nullptr);
    QVERIFY(house->text().contains(QStringLiteral("Lincoln par")));
    QTableWidget *table = page.universeTable();
    QCOMPARE(table->rowCount(), int(m_doc->inputOutputMap()->universesCount()));
    QVERIFY(page.clashList()->count() == 0);

    // A slot set here is the show's.
    QComboBox *slot = qobject_cast<QComboBox *>(table->cellWidget(1, VenueRigDialog::SlotCol));
    QVERIFY(slot != nullptr);
    m_doc->resetModified();
    slot->setCurrentIndex(slot->findData(int(RigOwner::House)));
    QCOMPARE(m_doc->inputOutputMap()->universeSlotOwner(1), RigOwner::House);
    QVERIFY(m_doc->isModified());

    // Not ours: typed in the table, kept in the venue, and the tour spot on
    // those channels is a clash.
    table->item(0, VenueRigDialog::NotOursCol)->setText(QStringLiteral("1-3"));
    QCOMPARE(v->notOurs(venue).size(), 1);
    QCOMPARE(v->notOurs(venue).first().from, 0);
    QCOMPARE(v->notOurs(venue).first().to, 2);
    QVERIFY(page.clashList()->count() >= 1);
    QVERIFY(page.clashList()->item(0)->text().contains(QStringLiteral("Tour spot")));
    Q_UNUSED(tour)
    // Something that is not a range is refused, nothing kept.
    table->item(0, VenueRigDialog::NotOursCol)->setText(QStringLiteral("stage left"));
    QCOMPARE(v->notOurs(venue).size(), 1);
    table->item(0, VenueRigDialog::NotOursCol)->setText(QString());
    QVERIFY(v->notOurs(venue).isEmpty());
    // What is left: the house par in universe 1, a Tour slot now that the
    // show has a House slot.
    QCOMPARE(page.clashList()->count(), 1);
    QVERIFY(page.clashList()->item(0)->text().contains(QStringLiteral("Tour slot")));
    table->item(2, VenueRigDialog::NotOursCol)->setText(QStringLiteral("all"));
    QCOMPARE(v->notOurs(venue).size(), 1);
    QVERIFY(v->notOurs(venue).first().whole());

    // The patch buttons.
    QPushButton *keep = page.findChild<QPushButton *>(QStringLiteral("vrKeepPatch"));
    QPushButton *forget = page.findChild<QPushButton *>(QStringLiteral("vrForgetPatch"));
    QVERIFY(keep != nullptr && forget != nullptr);
    QVERIFY(keep->isEnabled());
    QVERIFY(!forget->isEnabled());
    keep->click();
    QVERIFY(v->hasPatch(venue));
    QVERIFY(forget->isEnabled());
    forget->click();
    QVERIFY(!v->hasPatch(venue));

    // A venue not applied: its page says so, and the patch is not changed from it.
    const QString other = v->addVenue(QStringLiteral("Grand Hall"));
    VenueRigDialog away(m_doc, other, nullptr);
    QVERIFY(!away.findChild<QPushButton *>(QStringLiteral("vrKeepPatch"))->isEnabled());
    QVERIFY(away.findChild<QLabel *>(QStringLiteral("vrState"))->text().contains(QStringLiteral("not applied")));
}

void VenueRigUi_Test::theVenueManagerOpensIt()
{
    VenueCorrections *v = venues(m_doc);
    const QString venue = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(venue));
    FocusCallsDialog d(m_doc, nullptr);
    d.setVenueChoice(venue);
    QPushButton *rig = d.findChild<QPushButton *>(QStringLiteral("fcVenueRig"));
    QVERIFY(rig != nullptr);
    rig->click();
    QPointer<VenueRigDialog> page = d.findChild<VenueRigDialog *>();
    QVERIFY(page != nullptr);
    QCOMPARE(page->venue(), venue);
    page->close();
    QTRY_VERIFY(page.isNull());
}

QTEST_MAIN(VenueRigUi_Test)

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
#include "../common/fixtureeditor.h"
#include <QListWidget>
#include <QPushButton>
#include <QComboBox>
#include <QPointer>
#include <QTimer>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QDir>
#include <QtTest>

#include "venuerigui_test.h"
#include "programmercontroller.h"
#include "fixturegroupeditor.h"
#include "venuecorrections.h"
#include "focuscallsdialog.h"
#include "inputoutputmanager.h"
#include "inputoutputmap.h"
#include "venuerigdialog.h"
#include "venuefromshowdialog.h"
#include "roomoutputpicker.h"
#include "venuemenu.h"
#include "venueimport.h"
#include "../common/shotdark.h"
#include "../../../engine/test/common/pmslikeshow.h"
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

/* @p w grabbed at the three review sizes into $QLC_SHOT_DIR (nothing when
   it is unset), named @p name-WxH.png. */
void shoot(QWidget *w, const QString &name)
{
    const QString dir = QString::fromLocal8Bit(qgetenv("QLC_SHOT_DIR"));
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    const QSize was = w->size();
    for (const QSize &sz : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
    {
        w->resize(sz);
        QApplication::processEvents();
        w->grab().save(QStringLiteral("%1/%2-%3x%4.png").arg(dir, name).arg(sz.width()).arg(sz.height()));
    }
    w->resize(was);
    QApplication::processEvents();
}

QString pmsFile(QTemporaryDir *dir)
{
    const QString path = dir->filePath(QStringLiteral("PMS Stage.qlcc"));
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(PmsLikeShow::xml());
    return path;
}

QStringList items(const QListWidget *list)
{
    QStringList out;
    for (int i = 0; i < list->count(); ++i)
        out << list->item(i)->text();
    return out;
}
}

void VenueRigUi_Test::init()
{
    qputenv("QLC_NO_VOICE", "1");
    if (!qEnvironmentVariableIsEmpty("QLC_SHOT_DIR"))
        shotUseDarkTheme();
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

void VenueRigUi_Test::theConnectionsSlotPicker()
{
    InputOutputManager mgr(nullptr, m_doc);
    QComboBox *slot = mgr.findChild<QComboBox *>(QStringLiteral("uniSlotOwner"));
    QVERIFY(slot != nullptr);
    QCOMPARE(slot->count(), 2);
    QCOMPARE(slot->currentData().toInt(), int(RigOwner::Tour));
    m_doc->resetModified();
    slot->setCurrentIndex(slot->findData(int(RigOwner::House)));
    QCOMPARE(m_doc->inputOutputMap()->universeSlotOwner(0), RigOwner::House);
    QVERIFY(m_doc->isModified());
    QCOMPARE(m_doc->inputOutputMap()->universeSlotOwner(1), RigOwner::Tour);
}

/* docs/design/VENUES.md section 6 and the changelog: "A clash check ... runs before
   Apply now". Switching to a venue whose house rig clashes with the show
   asks first; Cancel leaves the venue off and the patch as it was; Apply
   anyway goes live with the clashing house fixture unpatched. */
void VenueRigUi_Test::aClashingVenueAsksBeforeItIsApplied()
{
    VenueCorrections *v = venues(m_doc);
    const quint32 tour = dimmer(m_doc, QStringLiteral("Tour 1"), 0, 10);
    const QString lincoln = v->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(v->setActiveVenue(lincoln));
    const quint32 par = dimmer(m_doc, QStringLiteral("House par"), 0, 50);
    m_doc->fixture(par)->setOwner(RigOwner::House);
    QVERIFY(v->isVenueFixture(par));
    QVERIFY(v->setActiveVenue(QString()));
    QVERIFY(m_doc->fixture(par) == nullptr);     // it went with its venue
    // Meanwhile a tour fixture moved onto the house par's address.
    QVERIFY(m_doc->patchFixtures({ Doc::FixturePatch{ tour, 0, 50 } }));
    QCOMPARE(v->clashes(lincoln).size(), 1);

    FocusCallsDialog d(m_doc, nullptr);
    d.setVenueChoice(lincoln);
    QPushButton *apply = d.findChild<QPushButton *>(QStringLiteral("fcApplyVenue"));
    QVERIFY(apply != nullptr);

    /* Each box answered from a timer, back in its own event loop: the
       clash box by @p anyway, anything else accepted (and noted). */
    QStringList asked, otherBoxes;
    auto answer = [&](bool anyway) {
        QTimer *t = new QTimer(&d);
        t->setInterval(20);
        connect(t, &QTimer::timeout, &d, [&asked, &otherBoxes, t, anyway]() {
            QMessageBox *box = qobject_cast<QMessageBox *>(modalOrFixtureEditor());
            if (box == nullptr)
                return;
            if (box->objectName() != QLatin1String("vrClashConfirm"))
            {
                otherBoxes << box->text();
                box->accept();
                return;
            }
            asked << box->text() + QLatin1Char('\n') + box->informativeText();
            QAbstractButton *pick = box->button(QMessageBox::Cancel);
            if (anyway)
                for (QAbstractButton *b : box->buttons())
                    if (b->text() == QLatin1String("Apply anyway"))
                        pick = b;
            t->stop();
            t->deleteLater();
            pick->click();
        });
        t->start();
    };

    // Cancel: nothing changes.
    answer(false);
    apply->click();
    QCOMPARE(asked.size(), 1);
    QVERIFY2(asked.first().contains(QStringLiteral("Lincoln HS clashes with the show")), qPrintable(asked.first()));
    QVERIFY2(asked.first().contains(QStringLiteral("House par")), qPrintable(asked.first()));
    QVERIFY(otherBoxes.isEmpty());
    QVERIFY(v->activeVenue().isEmpty());
    QVERIFY(m_doc->fixture(par) == nullptr);
    QVERIFY(m_doc->fixture(tour)->isPatched());
    QCOMPARE(m_doc->fixture(tour)->address(), 50u);

    // Start focus calls into it asks the same (it went live unchecked).
    QPushButton *start = d.findChild<QPushButton *>(QStringLiteral("fcStart"));
    QVERIFY(start != nullptr);
    QPushButton *addCall = d.findChild<QPushButton *>(QStringLiteral("fcAdd"));
    QVERIFY(addCall != nullptr);
    addCall->click();                           // a call to start
    QVERIFY(start->isEnabled());
    answer(false);
    start->click();
    QCOMPARE(asked.size(), 2);
    QVERIFY(otherBoxes.isEmpty());
    QVERIFY(v->activeVenue().isEmpty());
    QVERIFY(m_doc->fixture(par) == nullptr);
    QVERIFY(d.result() != QDialog::Accepted);

    // Apply anyway: live, the house par in but unpatched; the tour's alone.
    answer(true);
    apply->click();
    QCOMPARE(asked.size(), 3);
    QVERIFY2(otherBoxes.isEmpty(), qPrintable(otherBoxes.join('\n')));
    QCOMPARE(v->activeVenue(), lincoln);
    QVERIFY(m_doc->fixture(par) != nullptr);
    QVERIFY(!m_doc->fixture(par)->isPatched());
    QCOMPARE(m_doc->fixture(par)->address(), 50u);
    QVERIFY(m_doc->fixture(tour)->isPatched());
    QCOMPARE(m_doc->fixture(tour)->address(), 50u);
}

/****************************************************************************
 * New Venue from a Show (docs/design/VENUE_FROM_SHOW.md)
 ****************************************************************************/

void VenueRigUi_Test::aVenueFromAShowEndToEnd()
{
    VenueCorrections *v = venues(m_doc);
    // The touring show: lights on U1, a group named as the room's is.
    const quint32 tour = dimmer(m_doc, QStringLiteral("Tour par"), 0, 0);
    FixtureGroup *foh = new FixtureGroup(m_doc);
    foh->setName(QStringLiteral("FOH"));
    m_doc->addFixtureGroup(foh);
    foh->assignFixture(tour);
    m_doc->resetModified();
    const QString file = pmsFile(m_dir);

    // New Venue... offers From a Show..., the name typed carried over.
    QWidget window;
    window.resize(1024, 640);
    QPointer<QInputDialog> ask = VenueMenu::askNewVenue(m_doc, &window);
    QVERIFY(ask != nullptr);
    QPushButton *fromShow = ask->findChild<QPushButton *>(QStringLiteral("venueNewFromShow"));
    QVERIFY(fromShow != nullptr);
    QVERIFY(fromShow->isVisible());
    ask->setTextValue(QStringLiteral("PMS Auditeria"));
    fromShow->click();
    QTRY_VERIFY(ask.isNull());
    QPointer<VenueFromShowDialog> d = window.findChild<VenueFromShowDialog *>();
    QVERIFY(d != nullptr);
    QCOMPARE(d->nameEdit()->text(), QStringLiteral("PMS Auditeria"));
    QVERIFY(v->venueNames().isEmpty());                  // nothing made yet
    QVERIFY(!d->makeButton()->isEnabled());
    QCOMPARE(d->summaryLabel()->text(), QStringLiteral("Choose the room's show file."));

    // A file that is not a show: said on screen.
    const QString bad = m_dir->filePath(QStringLiteral("notes.qlcc"));
    QFile nf(bad);
    QVERIFY(nf.open(QIODevice::WriteOnly));
    nf.write("<QlcVenues/>");
    nf.close();
    QVERIFY(!d->setFile(bad));
    QVERIFY(d->summaryLabel()->text().contains(QStringLiteral("not a show file")));
    QVERIFY(!d->makeButton()->isEnabled());

    // The room's file: read, its universe suggested (U2, nothing on it).
    QVERIFY(d->setFile(file));
    QCOMPARE(d->universeBox()->currentData().toUInt(), 1u);
    QVERIFY(d->universeBox()->currentText().startsWith(QStringLiteral("U2 ")));
    QCOMPARE(d->summaryLabel()->text(),
             QString::fromUtf8("From PMS Stage.qlcc: 7 fixtures on U2 1\342\200\223109. Groups: 0 fill this "
                            "show's, 3 kept for later."));
    // No USB interface on this computer: Not yet, and the clash check says so.
    QCOMPARE(d->outputPicker()->kind(), RoomOutputPicker::NotYet);
    QVERIFY2(items(d->clashList()).filter(QString::fromUtf8("Output \342\200\224 U2: no output in this room yet")).size() == 1,
             qPrintable(items(d->clashList()).join('\n')));

    // FOH is suggested by its name and waits for the tick.
    GroupMatchTable *table = d->groupTable();
    QCOMPARE(table->rowCount(), 3);
    QCOMPARE(table->item(0, GroupMatchTable::NameCol)->text(), QStringLiteral("FOH"));
    QCOMPARE(table->groupBox(0)->currentText(), QStringLiteral("FOH (same name)"));
    QCOMPARE(table->item(0, GroupMatchTable::ConfirmCol)->checkState(), Qt::Unchecked);
    QCOMPARE(table->groupBox(1)->currentText(), QStringLiteral("Not matched (kept for later)"));
    QVERIFY(!d->makeButton()->isEnabled());
    QCOMPARE(d->makeButton()->text(), QStringLiteral("Make Venue (Confirm each match: 1 to go)"));
    shoot(d, QStringLiteral("venue-from-show-confirm"));
    table->setConfirmed(0, true);
    QVERIFY(d->makeButton()->isEnabled());
    QCOMPARE(d->makeButton()->text(), QStringLiteral("Make Venue"));
    QVERIFY(d->summaryLabel()->text().endsWith(QStringLiteral("Groups: 1 fill this show's, 2 kept for later.")));

    // An Art-Net node for the room: the address must read; then no Output clash.
    QComboBox *kind = d->outputPicker()->findChild<QComboBox *>(QStringLiteral("roomOutputKind"));
    QLineEdit *ip = d->outputPicker()->findChild<QLineEdit *>(QStringLiteral("roomOutputIp"));
    QVERIFY(kind != nullptr && ip != nullptr);
    // What the file had is offered, never chosen by itself, and flagged.
    QCOMPARE(kind->itemText(3), QStringLiteral("As PMS Stage.qlcc had it: Art-Net to 172.18.2.10 (from 172.18.2.219)"));
    kind->setCurrentIndex(3);
    QVERIFY(items(d->clashList()).filter(QStringLiteral("U2 still sends what PMS Stage.qlcc had")).size() == 1);
    kind->setCurrentIndex(kind->findData(int(RoomOutputPicker::ArtNet)));
    ip->setText(QStringLiteral("10.0.0"));
    QVERIFY(!d->makeButton()->isEnabled());
    ip->setText(QStringLiteral("10.0.0.20"));
    QVERIFY(d->makeButton()->isEnabled());
    QVERIFY(items(d->clashList()).filter(QStringLiteral("Output")).isEmpty());
    QCOMPARE(items(d->clashList()), QStringList({ QStringLiteral("No clashes.") }));
    shoot(d, QStringLiteral("venue-from-show-ready"));

    // Make Venue: made, applied; the show not changed; the note offers Undo.
    d->makeButton()->click();
    QTRY_VERIFY(d.isNull());
    QCOMPARE(v->activeVenue(), QStringLiteral("PMS Auditeria"));
    QCOMPARE(v->rig()->venueFixtures().size(), 7);
    QCOMPARE(foh->headList().size(), 4);
    QVERIFY(!m_doc->isModified());
    QCOMPARE(VenueRig::outputText(m_doc->inputOutputMap()->universe(1)->outputPatchDescriptions()),
             QStringLiteral("Art-Net to 10.0.0.20"));
    QPointer<QMessageBox> note = window.findChild<QMessageBox *>(QStringLiteral("vfsDone"));
    QVERIFY(note != nullptr);
    QVERIFY2(note->text().startsWith(QStringLiteral("PMS Auditeria made from PMS Stage.qlcc: 7 fixtures on U2")),
             qPrintable(note->text()));
    shoot(note, QStringLiteral("venue-from-show-done"));
    QPushButton *undo = note->findChild<QPushButton *>(QStringLiteral("vfsUndo"));
    QVERIFY(undo != nullptr);
    undo->click();
    QTRY_VERIFY(note.isNull());
    QVERIFY(!v->hasVenue(QStringLiteral("PMS Auditeria")));
    QVERIFY(!v->isActive());
    QCOMPARE(m_doc->fixtures().size(), 1);
    QCOMPARE(foh->headList().size(), 1);
    QVERIFY(!m_doc->isModified());
}

void VenueRigUi_Test::theVenuePageChoosesTheRoomOutput()
{
    VenueCorrections *v = venues(m_doc);
    dimmer(m_doc, QStringLiteral("Tour par"), 0, 0);
    FixtureGroup *wash = new FixtureGroup(m_doc);
    wash->setName(QStringLiteral("House wash"));
    m_doc->addFixtureGroup(wash);
    m_doc->resetModified();
    VenueImport::Source src;
    QVERIFY(VenueImport::read(pmsFile(m_dir), src));
    VenueImport::Options opt;
    opt.universe = 1;
    const QString pms = v->addVenueFromShow(QStringLiteral("PMS Auditeria"),
                                            VenueImport::build(src, m_doc, opt, v->reservedIds()).data);
    QVERIFY(!pms.isEmpty());
    QVERIFY(v->setActiveVenue(pms));

    VenueRigDialog page(m_doc, pms, nullptr);
    page.resize(1024, 640);
    page.show();
    QWidget *room = page.findChild<QWidget *>(QStringLiteral("vrRoom"));
    QLabel *text = page.findChild<QLabel *>(QStringLiteral("vrRoomText"));
    QLabel *waiting = page.findChild<QLabel *>(QStringLiteral("vrWaiting"));
    QVERIFY(room != nullptr && text != nullptr && waiting != nullptr);
    QVERIFY(room->isVisible());
    QVERIFY2(text->text().contains(QStringLiteral("is PMS Auditeria's own universe (its rig from PMS Stage.qlcc). "
                                                  "Its output here: none.")), qPrintable(text->text()));
    QCOMPARE(waiting->text(), QStringLiteral("Waiting for a match: FOH (3), Relays (4), 1st Elect. (2)"));
    QVERIFY(items(page.clashList()).filter(QStringLiteral("U2: no output in this room yet")).size() == 1);
    shoot(&page, QStringLiteral("venue-page-room-before"));

    // Chosen here: on the universe at once, the clash gone, kept in the venue.
    QComboBox *kind = page.roomOutput()->findChild<QComboBox *>(QStringLiteral("roomOutputKind"));
    QLineEdit *ip = page.roomOutput()->findChild<QLineEdit *>(QStringLiteral("roomOutputIp"));
    kind->setCurrentIndex(kind->findData(int(RoomOutputPicker::ArtNet)));
    QVERIFY(!page.useRoomOutputButton()->isEnabled());
    ip->setText(QStringLiteral("10.0.0.20"));
    QVERIFY(page.useRoomOutputButton()->isEnabled());
    page.useRoomOutputButton()->click();
    QCOMPARE(VenueRig::outputText(m_doc->inputOutputMap()->universe(1)->outputPatchDescriptions()),
             QStringLiteral("Art-Net to 10.0.0.20"));
    QVERIFY(items(page.clashList()).filter(QStringLiteral("Output")).isEmpty());
    QVERIFY(text->text().contains(QStringLiteral("Its output here: Art-Net to 10.0.0.20")));
    // No Art-Net plugin here: chosen, and said to be waiting, not sending.
    QVERIFY2(text->text().endsWith(QStringLiteral("Art-Net to 10.0.0.20, waiting: nothing on this computer reaches "
                                                  "it yet.")), qPrintable(text->text()));
    QCOMPARE(page.universeTable()->item(1, VenueRigDialog::PatchCol)->text(), QStringLiteral("Art-Net to 10.0.0.20"));
    QCOMPARE(VenueRig::outputText(v->venue(pms).rig.patch.value(1)), QStringLiteral("Art-Net to 10.0.0.20"));
    QVERIFY(!m_doc->isModified());

    // Match Groups...: FOH chosen by hand into House wash (his choice: confirmed).
    QPointer<QDialog> match = page.openMatchGroups();
    QVERIFY(match != nullptr);
    GroupMatchTable *table = match->findChild<GroupMatchTable *>();
    QPushButton *ok = match->findChild<QPushButton *>(QStringLiteral("vrMatchGroupsOk"));
    QVERIFY(table != nullptr && ok != nullptr);
    QVERIFY(!ok->isEnabled());                           // nothing chosen yet
    table->groupBox(0)->setCurrentIndex(table->groupBox(0)->findData(wash->id()));
    QCOMPARE(table->item(0, GroupMatchTable::ConfirmCol)->checkState(), Qt::Checked);
    QVERIFY(ok->isEnabled());
    shoot(match, QStringLiteral("venue-page-match-groups"));
    ok->click();
    QTRY_VERIFY(match.isNull());
    QCOMPARE(wash->headList().size(), 3);
    QVERIFY(!m_doc->isModified());          // the fill is the venue's, not an edit to the show
    QCOMPARE(waiting->text(), QStringLiteral("Waiting for a match: Relays (4), 1st Elect. (2)"));
    shoot(&page, QStringLiteral("venue-page-room-after"));

    // Use the Show's Patch: the room universe goes out the show's own, and says so.
    page.findChild<QPushButton *>(QStringLiteral("vrForgetPatch"))->click();
    QVERIFY2(text->text().endsWith(QStringLiteral("Its output here: the show's own (none).")), qPrintable(text->text()));

    // A venue not made from a show: no such section.
    const QString plain = v->addVenue(QStringLiteral("Lincoln HS"));
    VenueRigDialog other(m_doc, plain, nullptr);
    other.show();
    QVERIFY(!other.findChild<QWidget *>(QStringLiteral("vrRoom"))->isVisible());
}

void VenueRigUi_Test::aVenueFromAShowWithEveryUniverseInUse()
{
    VenueCorrections *v = venues(m_doc);
    for (quint32 u = 0; u < m_doc->inputOutputMap()->universesCount(); ++u)
        dimmer(m_doc, QStringLiteral("Tour %1").arg(u + 1), u, 0);
    m_doc->resetModified();
    QWidget window;
    VenueFromShowDialog d(m_doc, QStringLiteral("PMS Auditeria"), &window);
    d.show();
    QVERIFY(d.setFile(pmsFile(m_dir)));

    // None free: nothing chosen for him, and nothing can be made.
    QCOMPARE(d.universeBox()->currentData().toUInt(), uint(UINT_MAX));
    QCOMPARE(d.universeBox()->currentText(), QString::fromUtf8("Choose a universe\342\200\246"));
    QVERIFY(!d.makeButton()->isEnabled());
    QCOMPARE(d.makeButton()->text(), QStringLiteral("Make Venue (Choose a universe)"));
    QVERIFY(d.summaryLabel()->text().contains(QStringLiteral("every one has the show's fixtures on it now")));
    QVERIFY(!d.universeConfirm()->isVisible());

    // A universe with the show's fixtures: only on a tick that says what it does.
    d.universeBox()->setCurrentIndex(d.universeBox()->findData(1u));
    QVERIFY(d.universeConfirm()->isVisible());
    QCOMPARE(d.universeConfirm()->text(),
             QStringLiteral("Use it anyway: it has 1 of the show's fixtures, and this replaces the show's output "
                            "for U2 while the venue is applied"));
    QVERIFY(!d.makeButton()->isEnabled());
    QCOMPARE(d.makeButton()->text(), QStringLiteral("Make Venue (Confirm the universe: it has the show's fixtures)"));
    shoot(&d, QStringLiteral("venue-from-show-universe-in-use"));
    d.universeConfirm()->setChecked(true);
    d.groupTable()->setConfirmed(0, false);
    QVERIFY(d.makeButton()->isEnabled());
    // Another universe: confirmed again, for itself.
    d.universeBox()->setCurrentIndex(d.universeBox()->findData(2u));
    QVERIFY(!d.universeConfirm()->isChecked());
    QVERIFY(!d.makeButton()->isEnabled());
    QVERIFY(v->venueNames().isEmpty());
}

QTEST_MAIN(VenueRigUi_Test)

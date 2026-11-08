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
#include <QElapsedTimer>
#include <QDateTime>
#include <QMessageBox>
#include <QTreeWidgetItemIterator>
#include <QLabel>
#include <QDir>
#include <QTreeWidget>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "addfixture.h"
#include "fixturetreewidget.h"
#include "fixturemanager.h"
#undef protected
#undef private
#include "fixturecreator.h"
#include "iopluginstub.h"
#include "ioplugincache.h"
#include "patchplanner.h"
#include "outputpatch.h"
#include "showstatus.h"
#include "showstatusdialog.h"
#include "monitor.h"
#include "apputil.h"
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

/* Q205 (answered 2026-11-03): a fixture the engine refuses past the last
   universe is not added silently -- a message names the limit. */
void FixtureCreator_Test::addPastTheUniverseLimitSaysSo()
{
    driveAddDialog([](AddFixture *dlg) { dlg->m_universeValue = InputOutputMap::maxUniverses(); });
    QString text;
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [&text, poll]() {
        QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (box == nullptr || box->objectName() != QLatin1String("fixtureLimitBox"))
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        text = box->text();
        box->accept();
    };
    QTimer::singleShot(0, *poll);
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false,
                                                              InputOutputMap::invalidUniverse(), nullptr);
    QVERIFY(made.isEmpty());
    QCOMPARE(m_doc->fixturesCount(), 0);
    QVERIFY2(text.contains(QString::number(InputOutputMap::maxUniverses())), qPrintable(text));
    QVERIFY(text.contains("universe", Qt::CaseInsensitive));
}

void FixtureCreator_Test::networkFixtureKeepsItsIp()
{
    driveAddDialog([](AddFixture *dlg) {
        dlg->m_connectionCombo->setCurrentIndex(dlg->m_connectionCombo->findData(
            AddFixture::connectionKey(Fixture::Network, Fixture::ArtNetTransport)));
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

/* Each network fixture is its own device: four lanterns added at once from
   172.18.2.60 get .60, .61, .62 and .63 -- not one IP between them -- and
   the dialog says so before OK. */
void FixtureCreator_Test::aBatchOfNetworkFixturesGetsOneIpEach()
{
    QString shown;
    driveAddDialog([&shown](AddFixture *dlg) {
        dlg->m_connectionCombo->setCurrentIndex(dlg->m_connectionCombo->findData(
            AddFixture::connectionKey(Fixture::Network, Fixture::WledTransport)));
        dlg->m_ipEdit->setText("172.18.2.60");
        dlg->m_amountSpin->setValue(4);
        shown = dlg->m_ipRangeLabel->isHidden() ? QString() : dlg->m_ipRangeLabel->text();
    });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                              InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(shown, QString("One each: 172.18.2.60 to 172.18.2.63"));
    QCOMPARE(made.size(), 4);
    QStringList ips;
    foreach (quint32 id, made)
    {
        ips << m_doc->fixture(id)->ipAddress();
        QCOMPARE(m_doc->fixture(id)->transport(), Fixture::WledTransport);
    }
    QCOMPARE(ips, QStringList() << "172.18.2.60" << "172.18.2.61" << "172.18.2.62" << "172.18.2.63");

    // Past .255 it carries into the octet before; nonsense gets nothing.
    QCOMPARE(AppUtil::nthAddress("10.0.1.254", 3), QString("10.0.2.1"));
    QVERIFY(AppUtil::nthAddress("lantern.local", 1).isEmpty());
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

/* Names are unique: adding one "AT1-3z" when there is one already makes
   "AT1-3z #2", not a second "AT1-3z". */
void FixtureCreator_Test::aSingleAddWithATakenNameIsNumbered()
{
    Fixture *existing = new Fixture(m_doc);
    existing->setName("AT1-3z");
    existing->setChannels(1);
    QVERIFY(m_doc->addFixture(existing));

    driveAddDialog([](AddFixture *dlg) {
        dlg->m_fixtureDef = nullptr;
        dlg->m_mode = nullptr;
        dlg->m_nameValue = "AT1-3z";
    });
    QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, true,
                                                        InputOutputMap::invalidUniverse(), nullptr);
    QCOMPARE(made.size(), 1);
    QCOMPARE(m_doc->fixture(made.first())->name(), QString("AT1-3z #2"));
    QCOMPARE(existing->name(), QString("AT1-3z"));
    QVERIFY(m_doc->duplicateFixtureNames().isEmpty());
}

/*****************************************************************************
 * WLED by its own API
 *****************************************************************************/

namespace
{
/* Test WLEDs sit on loopback addresses: nothing leaves this computer. */
const char *const WledIp = "127.0.0.100";

/* Answers the next message box (or none), keeping what it said. */
void answerBoxes(QStringList *said)
{
    auto poll = std::make_shared<std::function<void()>>();
    auto left = std::make_shared<int>(200);
    *poll = [said, poll, left]() {
        if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
        {
            *said << box->text();
            box->accept();
            return;
        }
        if (--*left > 0)
            QTimer::singleShot(10, *poll);
    };
    QTimer::singleShot(0, *poll);
}

void pickWled(AddFixture *dlg, const QString &ip, int amount = 1)
{
    dlg->m_connectionCombo->setCurrentIndex(dlg->m_connectionCombo->findData(
        AddFixture::connectionKey(Fixture::Network, Fixture::WledTransport)));
    dlg->m_ipEdit->setText(ip);
    if (amount > 1)
        dlg->m_amountSpin->setValue(amount);
}
} // namespace

void FixtureCreator_Test::officeShow()
{
    m_doc->ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*wled*"));
    m_doc->ioPluginCache()->load(QDir(TEST_STUB_DIR, "*iopluginstub*"));
    QLCIOPlugin *wled = m_doc->ioPluginCache()->plugin("WLED");
    QVERIFY2(wled != nullptr, "the WLED output plugin did not load");
    // It never gets round to asking or sending in a test.
    QVERIFY(QMetaObject::invokeMethod(wled, "setSendInterval", Q_ARG(int, 3600000)));
    QVERIFY(QMetaObject::invokeMethod(wled, "setInfoInterval", Q_ARG(int, 3600000)));
    QLCIOPlugin *stub = nullptr;
    foreach (QLCIOPlugin *p, m_doc->ioPluginCache()->plugins())
        if (p != wled)
            stub = p;
    QVERIFY2(stub != nullptr, "the I/O plugin stub did not load");
    static_cast<IOPluginStub *>(stub)->m_nameOverride = "ArtNet";

    InputOutputMap *io = m_doc->inputOutputMap();
    QVERIFY(io->setOutputPatch(0, "ArtNet", QString(), 0));
    io->outputPatch(0)->setPluginParameter("outputIP", "172.18.2.10");
    io->setUniverseName(0, "U01-Office");
    Fixture *bar = new Fixture(m_doc);
    bar->setName("Office Bar");
    bar->setChannels(60);
    bar->setUniverse(0);
    bar->setAddress(0);
    QVERIFY(m_doc->addFixture(bar));
}

void FixtureCreator_Test::aWledGetsAUniverseOfItsOwn()
{
    officeShow();
    InputOutputMap *io = m_doc->inputOutputMap();
    const quint32 barId = m_doc->fixtureForAddress(0);

    bool fieldsOff = false, offBefore = true;
    QString said;
    driveAddDialog([&](AddFixture *dlg) {
        offBefore = !dlg->m_universeCombo->isEnabled() || !dlg->m_ownUniverseLabel->isHidden();
        dlg->m_nameEdit->setText("WLED Test");
        dlg->slotNameEdited("WLED Test");
        pickWled(dlg, WledIp);
        fieldsOff = !dlg->m_universeCombo->isEnabled() && !dlg->m_addressSpin->isEnabled()
                    && !dlg->m_gapSpin->isEnabled();
        said = dlg->m_ownUniverseLabel->isHidden() ? QString() : dlg->m_ownUniverseLabel->text();
    });
    // Opened in the Office universe, as a right-click there does.
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QVERIFY2(!offBefore, "DMX picked: the universe and address are asked as ever");
    QVERIFY(fieldsOff);
    QCOMPARE(said, QString("Gets a universe of its own, sent to its IP."));

    QCOMPARE(made.size(), 1);
    Fixture *fx = m_doc->fixture(made.first());
    QVERIFY(fx->isPatched());
    QVERIFY2(fx->universe() != 0, "a WLED must never land on the Art-Net universe");
    OutputPatch *op = io->outputPatch(fx->universe());
    QVERIFY(op != nullptr);
    QCOMPARE(op->pluginName(), QString("WLED"));
    QCOMPARE(op->getPluginParameters().value("outputIP").toString(), QString(WledIp));
    QCOMPARE(op->getPluginParameters().value("wledAddress").toInt(), int(fx->address()) + 1);
    QCOMPARE(op->getPluginParameters().value("wledChannels").toInt(), int(fx->channels()));
    QCOMPARE(PatchPlanner::wledRoute(m_doc, fx), PatchPlanner::WledRouted);

    // The Office universe is untouched: its output, its name, its bar.
    QCOMPARE(io->outputPatch(0)->pluginName(), QString("ArtNet"));
    QCOMPARE(io->outputPatch(0)->getPluginParameters().value("outputIP").toString(), QString("172.18.2.10"));
    QCOMPARE(io->getUniverseNameByID(0), QString("U01-Office"));
    QCOMPARE(m_doc->fixture(barId)->universe(), 0u);
    QCOMPARE(m_doc->fixture(barId)->address(), 0u);
}

void FixtureCreator_Test::aBatchOfWledsGetsAUniverseEach()
{
    officeShow();
    InputOutputMap *io = m_doc->inputOutputMap();
    driveAddDialog([](AddFixture *dlg) { pickWled(dlg, WledIp, 4); });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QCOMPARE(made.size(), 4);
    QSet<quint32> universes;
    QStringList sentTo;
    foreach (quint32 id, made)
    {
        Fixture *fx = m_doc->fixture(id);
        QVERIFY(fx->isPatched());
        QVERIFY(fx->universe() != 0);
        universes.insert(fx->universe());
        OutputPatch *op = io->outputPatch(fx->universe());
        QVERIFY(op != nullptr && op->pluginName() == "WLED");
        QCOMPARE(op->getPluginParameters().value("outputIP").toString(), fx->ipAddress());
        sentTo << fx->ipAddress();
    }
    QCOMPARE(universes.size(), 4);
    sentTo.sort();
    QCOMPARE(sentTo, QStringList({ "127.0.0.100", "127.0.0.101", "127.0.0.102", "127.0.0.103" }));
    QCOMPARE(io->outputPatch(0)->pluginName(), QString("ArtNet"));
}

void FixtureCreator_Test::aWledReusesTheUniverseSentToItsIp()
{
    officeShow();
    InputOutputMap *io = m_doc->inputOutputMap();
    driveAddDialog([](AddFixture *dlg) { pickWled(dlg, WledIp); });
    QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QCOMPARE(made.size(), 1);
    const quint32 uni = m_doc->fixture(made.first())->universe();
    const int count = int(io->universesCount());
    QVERIFY(m_doc->deleteFixture(made.first()));

    // Added again (another mode, say): back on the universe already sent there.
    driveAddDialog([](AddFixture *dlg) { pickWled(dlg, WledIp); });
    made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QCOMPARE(made.size(), 1);
    QCOMPARE(m_doc->fixture(made.first())->universe(), uni);
    QCOMPARE(int(io->universesCount()), count);
}

void FixtureCreator_Test::aWledNeverFollowsItsIpOntoArtNet()
{
    officeShow();
    // Left by the old Add Fixture on the Office universe, at the same IP.
    Fixture *old = new Fixture(m_doc);
    old->setName("WLED Test");
    old->setChannels(15);
    old->setUniverse(0);
    old->setAddress(60);
    old->setConnection(Fixture::Network);
    old->setTransport(Fixture::WledTransport);
    old->setIPAddress(WledIp);
    QVERIFY(m_doc->addFixture(old));

    driveAddDialog([](AddFixture *dlg) { pickWled(dlg, WledIp); });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QCOMPARE(made.size(), 1);
    QVERIFY(m_doc->fixture(made.first())->universe() != 0);
    QCOMPARE(PatchPlanner::wledRoute(m_doc, m_doc->fixture(made.first())), PatchPlanner::WledRouted);
    QCOMPARE(old->universe(), 0u);                      // left for the finding to say
    QCOMPARE(m_doc->inputOutputMap()->outputPatch(0)->pluginName(), QString("ArtNet"));
}

void FixtureCreator_Test::addingAWledIsOneUndo()
{
    officeShow();
    InputOutputMap *io = m_doc->inputOutputMap();
    QWidget parent;
    Monitor *mon = new Monitor(&parent, m_doc);
    const int universes = int(io->universesCount());
    const int fixtures = m_doc->fixtures().size();

    driveAddDialog([](AddFixture *dlg) { pickWled(dlg, WledIp); });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QCOMPARE(made.size(), 1);
    const quint32 uni = m_doc->fixture(made.first())->universe();
    QVERIFY(io->outputPatch(uni) != nullptr);
    QVERIFY(mon->canUndo());

    mon->undo();
    QVERIFY(m_doc->fixture(made.first()) == nullptr);
    QCOMPARE(m_doc->fixtures().size(), fixtures);
    QCOMPARE(int(io->universesCount()), universes);
    if (uni < io->universesCount())
        QVERIFY(io->outputPatch(uni) == nullptr);       // a free universe, free again
    QCOMPARE(io->outputPatch(0)->pluginName(), QString("ArtNet"));
    delete mon;
}

void FixtureCreator_Test::aWledOnAnArtNetUniverseIsAFindingWithAFix()
{
    officeShow();
    InputOutputMap *io = m_doc->inputOutputMap();
    // As the old Add Fixture left it: on the Office universe at 61.
    Fixture *fx = new Fixture(m_doc);
    fx->setName("WLED Test");
    fx->setChannels(15);
    fx->setUniverse(0);
    fx->setAddress(60);
    fx->setConnection(Fixture::Network);
    fx->setTransport(Fixture::WledTransport);
    fx->setIPAddress(WledIp);
    QVERIFY(m_doc->addFixture(fx));
    const quint32 fid = fx->id();

    QString detail;
    QCOMPARE(PatchPlanner::wledRoute(m_doc, fx, &detail), PatchPlanner::WledOtherOutput);
    QCOMPARE(detail, QString("ArtNet"));

    AppUtil::updateFixtureReadiness(m_doc);
    ShowStatus::Entry found;
    foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
        if (e.key == "wled.route")
            found = e;
    QCOMPARE(found.severity, ShowStatus::Warning);
    QCOMPARE(found.items, QStringList({ "WLED Test is on U01-Office, which goes out as Art-Net: "
                                        "nothing reaches the WLED" }));
    QCOMPARE(found.itemData, QVariantList() << fid);

    // The fix: a universe of its own; the Office universe as it was.
    PatchPlanner planner(m_doc);
    QStringList problems;
    std::function<void()> undo;
    QCOMPARE(planner.routeWled({ fid }, &problems, &undo), 1);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
    QVERIFY(fx->universe() != 0);
    QCOMPARE(PatchPlanner::wledRoute(m_doc, fx), PatchPlanner::WledRouted);
    QCOMPARE(io->outputPatch(fx->universe())->pluginName(), QString("WLED"));
    QCOMPARE(io->outputPatch(0)->pluginName(), QString("ArtNet"));
    QCOMPARE(io->outputPatch(0)->getPluginParameters().value("outputIP").toString(), QString("172.18.2.10"));
    QCOMPARE(m_doc->fixture(m_doc->fixtureForAddress(0))->name(), QString("Office Bar"));
    AppUtil::updateFixtureReadiness(m_doc);
    foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
        QVERIFY2(e.key != "wled.route", "fixed, the finding goes");

    // Nothing more to do the second time.
    QCOMPARE(planner.routeWled({ fid }), 0);

    // Undo: back on the Office universe at 61, as it was.
    const quint32 madeUni = fx->universe();
    undo();
    QCOMPARE(fx->universe(), 0u);
    QCOMPARE(fx->address(), 60u);
    QVERIFY(madeUni >= io->universesCount() || io->outputPatch(madeUni) == nullptr);
    QCOMPARE(PatchPlanner::wledRoute(m_doc, fx), PatchPlanner::WledOtherOutput);

    // The footer's own action does the same, through the app's helper.
    QCOMPARE(FixtureCreator::routeWled(nullptr, m_doc, { fid }, false), 1);
    QCOMPARE(PatchPlanner::wledRoute(m_doc, fx), PatchPlanner::WledRouted);
    ShowStatus::instance()->clearStatus("wled.route");
}

void FixtureCreator_Test::propertiesMakingItAWledMovesIt()
{
    officeShow();
    InputOutputMap *io = m_doc->inputOutputMap();
    // A plain dimmer on the Office universe at 61, then made a WLED by its API.
    Fixture *fx = new Fixture(m_doc);
    fx->setName("Corner");
    fx->setChannels(1);
    fx->setUniverse(0);
    fx->setAddress(60);
    QVERIFY(m_doc->addFixture(fx));
    const quint32 fid = fx->id();

    bool fieldsOff = false;
    driveAddDialog([&fieldsOff](AddFixture *dlg) {
        pickWled(dlg, WledIp);
        fieldsOff = !dlg->m_universeCombo->isEnabled() && !dlg->m_addressSpin->isEnabled();
    });
    QStringList said;
    answerBoxes(&said);
    QVERIFY(FixtureManager::editDefinitionAndMode(nullptr, m_doc, fid));
    QVERIFY(fieldsOff);
    QCOMPARE(fx->transport(), Fixture::WledTransport);
    QVERIFY(fx->isPatched());
    QVERIFY(fx->universe() != 0);
    QCOMPARE(PatchPlanner::wledRoute(m_doc, fx), PatchPlanner::WledRouted);
    QCOMPARE(io->outputPatch(0)->pluginName(), QString("ArtNet"));
    QVERIFY2(said.isEmpty(), qPrintable(said.join(" | ")));
}

void FixtureCreator_Test::wledShotsForReview()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set: no shots");
    QDir().mkpath(dir);
    officeShow();
    const QList<QSize> sizes = { QSize(760, 560), QSize(960, 680), QSize(1280, 860) };
    auto grab = [&dir, &sizes](QWidget *w, const QString &name) {
        for (const QSize &sz : sizes)
        {
            w->resize(sz);
            QApplication::processEvents();
            w->grab().save(QStringLiteral("%1/%2-%3x%4.png").arg(dir, name).arg(sz.width()).arg(sz.height()));
        }
    };

    // Add Fixture, WLED picked: before (as main left the fields) and after.
    {
        AddFixture dlg(nullptr, m_doc);
        dlg.preselectUniverse(0);
        dlg.m_nameEdit->setText("WLED Test");
        dlg.slotNameEdited("WLED Test");
        pickWled(&dlg, WledIp);
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        grab(&dlg, "after-addfixture-wled");
        dlg.m_universeCombo->setEnabled(true);
        dlg.m_addressSpin->setEnabled(true);
        dlg.m_diptoolButton->setEnabled(true);
        dlg.m_gapSpin->setEnabled(true);
        dlg.m_gapLabel->setEnabled(true);
        dlg.m_universeLabel->setEnabled(true);
        dlg.m_addressLabel->setEnabled(true);
        dlg.m_ownUniverseLabel->hide();
        grab(&dlg, "before-addfixture-wled");
    }

    // The finding, on a show like the one that found it.
    Fixture *fx = new Fixture(m_doc);
    fx->setName("WLED Test");
    fx->setChannels(15);
    fx->setUniverse(0);
    fx->setAddress(60);
    fx->setConnection(Fixture::Network);
    fx->setTransport(Fixture::WledTransport);
    fx->setIPAddress(WledIp);
    QVERIFY(m_doc->addFixture(fx));
    ShowStatus *status = ShowStatus::instance();
    status->setAction("wled.route", "Give It Its Own Universe", [](QWidget *, const QVariantList &) {});
    AppUtil::updateFixtureReadiness(m_doc);
    const ShowStatus::Entry found = [status]() {
        foreach (const ShowStatus::Entry &e, status->allEntries())
            if (e.key == "wled.route")
                return e;
        return ShowStatus::Entry();
    }();
    {
        ShowStatusDialog dlg;
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        const QList<QTreeWidgetItem *> rows = dlg.tree()->findItems(found.items.value(0),
                                                                   Qt::MatchExactly | Qt::MatchRecursive);
        QVERIFY(!rows.isEmpty());
        rows.first()->setSelected(true);
        dlg.tree()->setCurrentItem(rows.first());
        grab(&dlg, "after-finding");
    }
    status->clearStatus("wled.route");
    {
        ShowStatusDialog dlg;
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        grab(&dlg, "before-finding");
    }
}

/*****************************************************************************
 * Art-Net / sACN network fixtures (UX-67)
 *****************************************************************************/

namespace
{
/* Test devices sit on loopback addresses: nothing leaves this computer
   (and the stub sends nothing anyway). */
const char *const NetIp = "127.0.0.50";

void pickNetwork(AddFixture *dlg, const QString &ip, int address = 0)
{
    dlg->m_connectionCombo->setCurrentIndex(dlg->m_connectionCombo->findData(
        AddFixture::connectionKey(Fixture::Network, Fixture::ArtNetTransport)));
    dlg->m_ipEdit->setText(ip);
    if (address > 0)
        dlg->m_addressSpin->setValue(address);
}

int nodePortsHeard(QLCIOPlugin *art, const QString &ip)
{
    int n = 0;
    foreach (const QLCIOPlugin::Device &d, art->discoveredDevices())
        if (d.address == ip)
            n = qMax(n, int(d.portUniverses.size()));
    return n;
}

ShowStatus::Entry findingFor(const QString &key)
{
    foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
        if (e.key == key)
            return e;
    return ShowStatus::Entry();
}
} // namespace

IOPluginStub *FixtureCreator_Test::networkShow(const QString &plugin)
{
    m_doc->ioPluginCache()->load(QDir(TEST_STUB_DIR, "*iopluginstub*"));
    IOPluginStub *stub = nullptr;
    foreach (QLCIOPlugin *p, m_doc->ioPluginCache()->plugins())
        if (IOPluginStub *s = qobject_cast<IOPluginStub *>(p))
            stub = s;
    if (stub == nullptr)
        return nullptr;
    stub->m_nameOverride = plugin;
    stub->m_devices.clear();

    InputOutputMap *io = m_doc->inputOutputMap();
    io->setOutputPatch(0, plugin, QString(), 0);
    if (plugin == "E1.31")
    {
        io->outputPatch(0)->setPluginParameter("multicast", 0);
        io->outputPatch(0)->setPluginParameter("ucastIP", "172.18.2.10");
    }
    else
        io->outputPatch(0)->setPluginParameter("outputIP", "172.18.2.10");
    io->setUniverseName(0, "U01-Office");
    Fixture *bar = new Fixture(m_doc);
    bar->setName("Office Bar");
    bar->setChannels(60);
    bar->setUniverse(0);
    bar->setAddress(0);
    m_doc->addFixture(bar);
    return stub;
}

void FixtureCreator_Test::anArtNetFixtureGoesWhereItIsHeard()
{
    QVERIFY(networkShow("ArtNet") != nullptr);
    InputOutputMap *io = m_doc->inputOutputMap();
    const quint32 barId = m_doc->fixtureForAddress(0);

    QString said;
    bool addressOn = false, gapOff = false;
    driveAddDialog([&](AddFixture *dlg) {
        dlg->m_nameEdit->setText("Fence");
        dlg->slotNameEdited("Fence");
        // Address 1, where the Office Bar is: on its own universe, no clash.
        pickNetwork(dlg, NetIp, 1);
        said = dlg->m_ownUniverseLabel->isHidden() ? QString() : dlg->m_ownUniverseLabel->text();
        addressOn = dlg->m_addressSpin->isEnabled() && dlg->m_universeCombo->isEnabled();
        gapOff = !dlg->m_gapSpin->isEnabled();
    });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QCOMPARE(said, QString("U01-Office goes to 172.18.2.10, not this IP: it goes on a universe "
                           "sent to its IP, at this address."));
    QVERIFY2(addressOn, "the device listens at an address: it is still asked");
    QVERIFY(gapOff);

    QCOMPARE(made.size(), 1);
    Fixture *fx = m_doc->fixture(made.first());
    QVERIFY(fx->isPatched());
    QVERIFY2(fx->universe() != 0, "never left on a universe sent to another IP");
    QCOMPARE(fx->address(), 0u);                        // the address typed
    OutputPatch *op = io->outputPatch(fx->universe());
    QVERIFY(op != nullptr);
    QCOMPARE(op->pluginName(), QString("ArtNet"));
    QCOMPARE(op->getPluginParameters().value("outputIP").toString(), QString(NetIp));
    QCOMPARE(PatchPlanner::networkRoute(m_doc, fx), PatchPlanner::NetRouted);

    // The Office universe is as it was.
    QCOMPARE(io->outputPatch(0)->getPluginParameters().value("outputIP").toString(), QString("172.18.2.10"));
    QCOMPARE(io->getUniverseNameByID(0), QString("U01-Office"));
    QCOMPARE(m_doc->fixture(barId)->universe(), 0u);

    // A second one at the same IP joins it, at its own address.
    driveAddDialog([](AddFixture *dlg) { pickNetwork(dlg, NetIp, 101); });
    const QList<quint32> more = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QCOMPARE(more.size(), 1);
    QCOMPARE(m_doc->fixture(more.first())->universe(), fx->universe());
    QCOMPARE(m_doc->fixture(more.first())->address(), 100u);
}

void FixtureCreator_Test::anArtNetFixtureTheUniverseReachesStaysPut()
{
    QVERIFY(networkShow("ArtNet") != nullptr);
    const int universes = int(m_doc->inputOutputMap()->universesCount());
    bool labelHidden = false;
    driveAddDialog([&labelHidden](AddFixture *dlg) {
        pickNetwork(dlg, "172.18.2.10", 61);
        labelHidden = dlg->m_ownUniverseLabel->isHidden();
    });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QVERIFY(labelHidden);
    QCOMPARE(made.size(), 1);
    QCOMPARE(m_doc->fixture(made.first())->universe(), 0u);
    QCOMPARE(m_doc->fixture(made.first())->address(), 60u);
    QCOMPARE(int(m_doc->inputOutputMap()->universesCount()), universes);
}

void FixtureCreator_Test::addingANetworkFixtureElsewhereIsOneUndo()
{
    QVERIFY(networkShow("ArtNet") != nullptr);
    InputOutputMap *io = m_doc->inputOutputMap();
    QWidget parent;
    Monitor *mon = new Monitor(&parent, m_doc);
    const int universes = int(io->universesCount());
    const int fixtures = m_doc->fixtures().size();

    driveAddDialog([](AddFixture *dlg) { pickNetwork(dlg, NetIp, 1); });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QCOMPARE(made.size(), 1);
    const quint32 uni = m_doc->fixture(made.first())->universe();
    QVERIFY(uni != 0);
    QVERIFY(mon->canUndo());

    mon->undo();
    QVERIFY(m_doc->fixture(made.first()) == nullptr);
    QCOMPARE(m_doc->fixtures().size(), fixtures);
    QCOMPARE(int(io->universesCount()), universes);
    if (uni < io->universesCount())
        QVERIFY(io->outputPatch(uni) == nullptr);
    QCOMPARE(io->outputPatch(0)->getPluginParameters().value("outputIP").toString(), QString("172.18.2.10"));
    delete mon;
}

void FixtureCreator_Test::anSacnFixtureIsReallyUnicast()
{
    QVERIFY(networkShow("E1.31") != nullptr);
    InputOutputMap *io = m_doc->inputOutputMap();
    driveAddDialog([](AddFixture *dlg) { pickNetwork(dlg, NetIp, 1); });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QCOMPARE(made.size(), 1);
    Fixture *fx = m_doc->fixture(made.first());
    QVERIFY(fx->universe() != 0);
    OutputPatch *op = io->outputPatch(fx->universe());
    QVERIFY(op != nullptr);
    QCOMPARE(op->pluginName(), QString("E1.31"));
    const QMap<QString, QVariant> pp = op->getPluginParameters();
    QCOMPARE(pp.value("multicast").toInt(), 0);
    QCOMPARE(pp.value("ucastIP").toString(), QString(NetIp));
    QVERIFY2(!pp.contains("outputIP"), "E1.31 has no outputIP: it would only warn and multicast");
    QCOMPARE(PatchPlanner::networkRoute(m_doc, fx), PatchPlanner::NetRouted);

    // Multicast on again (by hand): its IP is no longer the target.
    op->setPluginParameter("multicast", 1);
    QCOMPARE(PatchPlanner::networkRoute(m_doc, fx), PatchPlanner::NetRouted);   // multicast reaches its network
    op->setPluginParameter("multicast", 0);
    op->setPluginParameter("ucastIP", "127.0.0.99");
    QString detail;
    QCOMPARE(PatchPlanner::networkRoute(m_doc, fx, &detail), PatchPlanner::NetOtherIp);
    QCOMPARE(detail, QString("127.0.0.99"));
}

void FixtureCreator_Test::aNetworkFixtureOnTheWrongUniverseIsAFindingWithAFix()
{
    QVERIFY(networkShow("ArtNet") != nullptr);
    InputOutputMap *io = m_doc->inputOutputMap();
    // Moved by hand onto the Office universe, which goes to 172.18.2.10.
    Fixture *fx = new Fixture(m_doc);
    fx->setName("Fence");
    fx->setChannels(3);
    fx->setUniverse(0);
    fx->setAddress(100);
    fx->setConnection(Fixture::Network);
    fx->setIPAddress(NetIp);
    QVERIFY(m_doc->addFixture(fx));
    const quint32 fid = fx->id();

    AppUtil::updateFixtureReadiness(m_doc);
    ShowStatus::Entry found = findingFor("network.route");
    QCOMPARE(found.severity, ShowStatus::Warning);
    QCOMPARE(found.items, QStringList({ "Fence is on U01-Office, which is sent to 172.18.2.10, "
                                        "not to its IP 127.0.0.50" }));
    QCOMPARE(found.itemData, QVariantList() << fid);

    // A universe with no output at all says so too.
    io->addUniverse();
    QVERIFY(m_doc->patchFixtures({ Doc::FixturePatch{ fid, 1, 100 } }));
    AppUtil::updateFixtureReadiness(m_doc);
    QCOMPARE(findingFor("network.route").items,
             QStringList({ "Fence is on Universe 2, which has no output: nothing reaches 127.0.0.50" }));
    QVERIFY(m_doc->patchFixtures({ Doc::FixturePatch{ fid, 0, 100 } }));

    // The footer's fix (App's action calls this helper): a universe sent to
    // its IP, same address; the Office universe as it was.
    QWidget parent;
    Monitor *mon = new Monitor(&parent, m_doc);
    QCOMPARE(FixtureCreator::routeNetwork(nullptr, m_doc, { fid }, false), 1);
    QVERIFY(fx->universe() != 0);
    QCOMPARE(fx->address(), 100u);
    QCOMPARE(PatchPlanner::networkRoute(m_doc, fx), PatchPlanner::NetRouted);
    QCOMPARE(io->outputPatch(0)->getPluginParameters().value("outputIP").toString(), QString("172.18.2.10"));
    AppUtil::updateFixtureReadiness(m_doc);
    QVERIFY2(findingFor("network.route").key.isEmpty(), "fixed, the finding goes");

    // Nothing more to do the second time; one undo puts it back.
    QCOMPARE(FixtureCreator::routeNetwork(nullptr, m_doc, { fid }, false), 0);
    QVERIFY(mon->canUndo());
    mon->undo();
    QCOMPARE(fx->universe(), 0u);
    QCOMPARE(fx->address(), 100u);
    QCOMPARE(PatchPlanner::networkRoute(m_doc, fx), PatchPlanner::NetOtherIp);
    delete mon;
    ShowStatus::instance()->clearStatus("network.route");
}

void FixtureCreator_Test::aNodeListeningOnAnotherPortIsAFindingWithAFix()
{
    IOPluginStub *stub = networkShow("ArtNet");
    QVERIFY(stub != nullptr);
    InputOutputMap *io = m_doc->inputOutputMap();
    // A node that answers at .218 with one port on 0:0:15 (the OLA probe's).
    QLCIOPlugin::Device node;
    node.name = "OLA Probe";
    node.address = "127.0.0.218";
    node.portLabels << "0:0:15";
    node.portUniverses << 15;
    stub->m_devices << node;
    // Universe 2 sent to it, on the Port-Address its number gives: 0:0:1.
    io->addUniverse();
    QVERIFY(io->setOutputPatch(1, "ArtNet", QString(), 0));
    io->outputPatch(1)->setPluginParameter("outputIP", "127.0.0.218");
    io->setUniverseName(1, "U02-Probe");

    AppUtil::updateFixtureReadiness(m_doc);
    ShowStatus::Entry found = findingFor("artnet.ports");
    QCOMPARE(found.severity, ShowStatus::Warning);
    QCOMPARE(found.items, QStringList({ "U02-Probe goes to OLA Probe (127.0.0.218) on 0:0:1, "
                                        "but its ports listen on 0:0:15" }));
    QCOMPARE(found.itemData, QVariantList() << 1u);
    // The Office universe, sent to a node not heard, is not flagged.
    QCOMPARE(found.items.size(), 1);

    std::function<void()> undo;
    QCOMPARE(AppUtil::sendOnNodePorts(m_doc, found.itemData, &undo), 1);
    QVERIFY(bool(undo));
    QCOMPARE(io->outputPatch(1)->getPluginParameters().value("outputUni").toUInt(), 15u);
    AppUtil::updateFixtureReadiness(m_doc);
    QVERIFY2(findingFor("artnet.ports").key.isEmpty(), "fixed, the finding goes");

    undo();
    QCOMPARE(io->outputPatch(1)->getPluginParameters().value("outputUni", 1).toUInt(), 1u);
    ShowStatus::instance()->clearStatus("artnet.ports");
}

void FixtureCreator_Test::anArtNetFixtureIsSentOnThePortItsNodeListensTo()
{
    IOPluginStub *stub = networkShow("ArtNet");
    QVERIFY(stub != nullptr);
    QLCIOPlugin::Device node;
    node.name = "OLA Probe";
    node.address = "127.0.0.218";
    node.portUniverses << 15;
    stub->m_devices << node;

    driveAddDialog([](AddFixture *dlg) { pickNetwork(dlg, "127.0.0.218", 1); });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QCOMPARE(made.size(), 1);
    Fixture *fx = m_doc->fixture(made.first());
    OutputPatch *op = m_doc->inputOutputMap()->outputPatch(fx->universe());
    QVERIFY(op != nullptr);
    QCOMPARE(op->getPluginParameters().value("outputIP").toString(), QString("127.0.0.218"));
    QCOMPARE(op->getPluginParameters().value("outputUni").toUInt(), 15u);
    AppUtil::updateFixtureReadiness(m_doc);
    QVERIFY(findingFor("artnet.ports").key.isEmpty());
    QVERIFY(findingFor("network.route").key.isEmpty());
    stub->m_devices.clear();
}

void FixtureCreator_Test::networkShotsForReview()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set: no shots");
    QDir().mkpath(dir);
    IOPluginStub *stub = networkShow("ArtNet");
    QVERIFY(stub != nullptr);
    const QList<QSize> sizes = { QSize(760, 560), QSize(960, 680), QSize(1280, 860) };
    auto grab = [&dir, &sizes](QWidget *w, const QString &name) {
        for (const QSize &sz : sizes)
        {
            w->resize(sz);
            QApplication::processEvents();
            w->grab().save(QStringLiteral("%1/%2-%3x%4.png").arg(dir, name).arg(sz.width()).arg(sz.height()));
        }
    };

    // Add Fixture, Network (Art-Net/sACN) at an IP the universe doesn't reach.
    {
        AddFixture dlg(nullptr, m_doc);
        dlg.preselectUniverse(0);
        dlg.m_nameEdit->setText("Fence");
        dlg.slotNameEdited("Fence");
        pickNetwork(&dlg, NetIp, 1);
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        grab(&dlg, "after-addfixture-artnet");
        // As main left it: no word, the gap on, and the address a clash.
        dlg.m_ownUniverseLabel->hide();
        dlg.m_gapSpin->setEnabled(true);
        dlg.m_gapLabel->setEnabled(true);
        dlg.m_addrErrorLabel->show();
        grab(&dlg, "before-addfixture-artnet");
    }

    // The two findings, on the office show.
    Fixture *fx = new Fixture(m_doc);
    fx->setName("Fence");
    fx->setChannels(3);
    fx->setUniverse(0);
    fx->setAddress(100);
    fx->setConnection(Fixture::Network);
    fx->setIPAddress(NetIp);
    QVERIFY(m_doc->addFixture(fx));
    QLCIOPlugin::Device node;
    node.name = "OLA Probe";
    node.address = "127.0.0.218";
    node.portUniverses << 15;
    stub->m_devices << node;
    InputOutputMap *io = m_doc->inputOutputMap();
    io->addUniverse();
    QVERIFY(io->setOutputPatch(1, "ArtNet", QString(), 0));
    io->outputPatch(1)->setPluginParameter("outputIP", "127.0.0.218");
    io->setUniverseName(1, "U02-Probe");

    ShowStatus *status = ShowStatus::instance();
    status->setAction("network.route", "Send to Its IP", [](QWidget *, const QVariantList &) {});
    status->setAction("artnet.ports", "Send on Its Port", [](QWidget *, const QVariantList &) {});
    AppUtil::updateFixtureReadiness(m_doc);
    foreach (const QString &key, QStringList({ "network.route", "artnet.ports" }))
    {
        const ShowStatus::Entry found = findingFor(key);
        QVERIFY(!found.items.isEmpty());
        ShowStatusDialog dlg;
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        const QList<QTreeWidgetItem *> rows = dlg.tree()->findItems(found.items.value(0),
                                                                   Qt::MatchExactly | Qt::MatchRecursive);
        QVERIFY(!rows.isEmpty());
        rows.first()->setSelected(true);
        dlg.tree()->setCurrentItem(rows.first());
        grab(&dlg, QStringLiteral("after-finding-%1").arg(QString(key).replace('.', '-')));
    }
    status->clearStatus("network.route");
    status->clearStatus("artnet.ports");
    {
        ShowStatusDialog dlg;
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        grab(&dlg, "before-finding-network");
    }
    stub->m_devices.clear();
}

void FixtureCreator_Test::realWireToTheProbe()
{
    const QString probe = qEnvironmentVariable("QLC_REAL_WIRE_PROBE");
    if (probe.isEmpty())
        QSKIP("QLC_REAL_WIRE_PROBE not set: nothing goes on the wire");
    m_doc->ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*artnet*"));
    m_doc->ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*e131*"));
    QLCIOPlugin *art = m_doc->ioPluginCache()->plugin("ArtNet");
    QLCIOPlugin *sacn = m_doc->ioPluginCache()->plugin("E1.31");
    QVERIFY(art != nullptr && sacn != nullptr);
    InputOutputMap *io = m_doc->inputOutputMap();
    const int line = art->lineOnSameSubnet(probe);
    QVERIFY2(line >= 0, "no interface on the probe's network");

    auto send = [&io](quint32 uni, const QList<int> &values, int ms) {
        QByteArray data(512, 0);
        for (int i = 0; i < values.size(); i++)
            data[i] = char(values.at(i));
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < ms)
        {
            io->universes().at(int(uni))->dumpOutput(data, true);
            QTest::qWait(50);
        }
    };

    /* Art-Net. Universe 1 is already unicast to the probe on 0:0:0 (its
       other port), so polling has heard it; Add Fixture's universe should
       go to it on the port left: 0:0:15. */
    QVERIFY(io->setOutputPatch(0, "ArtNet", art->outputs().at(line), quint32(line)));
    io->outputPatch(0)->setPluginParameter("outputIP", probe);
    io->outputPatch(0)->setPluginParameter("outputUni", 0);
    Fixture *bar = new Fixture(m_doc);
    bar->setName("Probe 0");
    bar->setChannels(4);
    bar->setUniverse(0);
    bar->setAddress(0);
    QVERIFY(m_doc->addFixture(bar));
    // Universe 2: a DMX bar with no output -- where Add Fixture is opened.
    io->addUniverse();
    Fixture *dmxBar = new Fixture(m_doc);
    dmxBar->setName("Bench");
    dmxBar->setChannels(4);
    dmxBar->setUniverse(1);
    dmxBar->setAddress(0);
    QVERIFY(m_doc->addFixture(dmxBar));
    for (int i = 0; i < 80 && nodePortsHeard(art, probe) < 2; i++)
        QTest::qWait(100);
    qInfo().noquote() << "heard at" << probe << ":" << nodePortsHeard(art, probe) << "port(s)";

    driveAddDialog([&probe](AddFixture *dlg) {
        dlg->m_channelsSpin->setValue(4);
        pickNetwork(dlg, probe, 1);
    });
    QList<quint32> made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 1, nullptr);
    QCOMPARE(made.size(), 1);
    Fixture *fx = m_doc->fixture(made.first());
    QVERIFY(fx->universe() != 0);
    QMap<QString, QVariant> pp = io->outputPatch(fx->universe())->getPluginParameters();
    qInfo().noquote() << "ARTNET universe" << fx->universe() + 1 << "plugin"
                      << io->outputPatch(fx->universe())->pluginName() << "outputIP"
                      << pp.value("outputIP").toString() << "outputUni"
                      << pp.value("outputUni").toString();
    QCOMPARE(pp.value("outputIP").toString(), probe);
    QCOMPARE(pp.value("outputUni").toUInt(), 15u);
    qInfo() << "PHASE artnet 11 22 33 44" << QDateTime::currentDateTime().toString("HH:mm:ss");
    send(fx->universe(), { 11, 22, 33, 44 }, 6000);
    send(fx->universe(), { 0, 0, 0, 0 }, 500);
    const QString artLine = art->outputs().at(line);

    // A show of its own for sACN: nothing of the Art-Net one to borrow.
    delete m_doc;
    m_doc = new Doc(this);
    m_doc->ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*e131*"));
    sacn = m_doc->ioPluginCache()->plugin("E1.31");
    QVERIFY(sacn != nullptr);
    io = m_doc->inputOutputMap();

    /* sACN. Universe 1 goes out as sACN unicast to another IP (loopback);
       Add Fixture's universe borrows it and should unicast to the probe. A
       multicast group the probe doesn't join proves it is unicast; the
       probe reads sACN universe 15 (set as in Connections). */
    Fixture *loopBar = new Fixture(m_doc);
    loopBar->setName("Loop");
    loopBar->setChannels(4);
    loopBar->setUniverse(0);
    loopBar->setAddress(0);
    QVERIFY(m_doc->addFixture(loopBar));
    const QStringList sl = sacn->outputs();
    const int sline = sl.indexOf(artLine);
    QVERIFY2(sline >= 0, "no sACN line on the probe's network");
    QVERIFY(io->setOutputPatch(0, "E1.31", sl.at(sline), quint32(sline)));
    io->outputPatch(0)->setPluginParameter("multicast", 0);
    io->outputPatch(0)->setPluginParameter("ucastIP", "127.0.0.1");
    driveAddDialog([&probe](AddFixture *dlg) {
        dlg->m_channelsSpin->setValue(4);
        pickNetwork(dlg, probe, 1);
    });
    made = FixtureCreator::addFromDialog(nullptr, m_doc, false, 0, nullptr);
    QCOMPARE(made.size(), 1);
    fx = m_doc->fixture(made.first());
    OutputPatch *op = io->outputPatch(fx->universe());
    QCOMPARE(op->pluginName(), QString("E1.31"));
    op->setPluginParameter("universe", 15);
    op->setPluginParameter("mcastFullIP", "239.255.0.99");
    pp = op->getPluginParameters();
    qInfo().noquote() << "SACN universe" << fx->universe() + 1 << "multicast"
                      << pp.value("multicast").toString() << "ucastIP" << pp.value("ucastIP").toString()
                      << "outputIP" << pp.value("outputIP").toString();
    QCOMPARE(pp.value("multicast").toInt(), 0);
    QCOMPARE(pp.value("ucastIP").toString(), probe);
    qInfo() << "PHASE sacn-unicast 55 66 77 88" << QDateTime::currentDateTime().toString("HH:mm:ss");
    send(fx->universe(), { 55, 66, 77, 88 }, 6000);
    // Multicast on (to the group nobody joined): the probe must hear nothing new.
    op->setPluginParameter("multicast", 1);
    qInfo() << "PHASE sacn-multicast 99 99 99 99" << QDateTime::currentDateTime().toString("HH:mm:ss");
    send(fx->universe(), { 99, 99, 99, 99 }, 6000);
    qInfo() << "PHASE done" << QDateTime::currentDateTime().toString("HH:mm:ss");
}

QTEST_MAIN(FixtureCreator_Test)

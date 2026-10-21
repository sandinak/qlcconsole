/*
  Q Light Controller Plus - Test Unit
  patchinventorydialog_test.cpp

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
#include <QStandardItemModel>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QLabel>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <functional>
#include <memory>

#define private public
#include "patchinventorydialog.h"
#include "universecombo.h"
#undef private
#include "scene.h"
#include <QCheckBox>
#include "monitorproperties.h"
#include "networktopology.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlcfixturehead.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "doc.h"

#include "patchinventorydialog_test.h"

static Fixture *staged(Doc *doc, int channels, const QString &name)
{
    Fixture *f = new Fixture(doc);
    f->setChannels(channels);
    f->setPatched(false);
    f->setName(name);
    doc->addFixture(f);
    return f;
}

void PatchInventoryDialog_Test::init()
{
    m_doc = new Doc(this);
}

void PatchInventoryDialog_Test::cleanup()
{
    delete m_doc;
}

void PatchInventoryDialog_Test::opensOnAProposalForEveryStagedFixture()
{
    staged(m_doc, 8, "A");
    staged(m_doc, 8, "B");
    staged(m_doc, 8, "C");

    PatchInventoryDialog dlg(m_doc);
    QCOMPARE(dlg.m_table->rowCount(), 3);
    const QList<PatchPlanner::Assignment> plan = dlg.currentPlan();
    QCOMPARE(plan[0].address, quint32(0));
    QCOMPARE(plan[1].address, quint32(8));
    QCOMPARE(plan[2].address, quint32(16));
    QVERIFY(dlg.m_apply->isEnabled());
    QVERIFY2(dlg.m_summary->text().contains("3 fixture"), qPrintable(dlg.m_summary->text()));
}

void PatchInventoryDialog_Test::onlyTheGivenFixturesAreListed()
{
    Fixture *a = staged(m_doc, 8, "A");
    staged(m_doc, 8, "B");

    PatchInventoryDialog dlg(m_doc, QList<quint32>() << a->id());
    QCOMPARE(dlg.m_table->rowCount(), 1);
    QCOMPARE(dlg.currentPlan().first().fixtureId, a->id());
}

void PatchInventoryDialog_Test::universesArePickedByNameInNameOrder()
{
    // "select which U by a drop down vs an id .. sorted by location (which
    // should be by name)": numbers in names compare as numbers.
    InputOutputMap *io = m_doc->inputOutputMap();
    while (io->universesCount() < 4)
        QVERIFY(io->addUniverse());
    io->setUniverseName(0, "U12-SL");
    io->setUniverseName(1, "U3-SR");
    io->setUniverseName(2, "U03-Cyc");
    io->setUniverseName(3, "Booth");
    Fixture *a = staged(m_doc, 8, "A");

    PatchInventoryDialog dlg(m_doc);
    UniverseCombo *uni = qobject_cast<UniverseCombo *>(dlg.m_table->cellWidget(0, 5));
    QVERIFY(uni != nullptr);
    QStringList names;
    for (int i = 0; i < uni->count(); i++)
        names << uni->itemText(i);
    QCOMPARE(names.mid(0, 4), QStringList() << "Booth" << "U03-Cyc" << "U3-SR" << "U12-SL");
    QVERIFY2(names.at(4).startsWith("New universe 5"), qPrintable(names.join(", ")));

    // Chosen by name, patched to that universe.
    uni->setCurrentIndex(uni->findText("U3-SR"));
    QCOMPARE(dlg.currentPlan().first().universe, quint32(1));
    dlg.accept();
    QCOMPARE(a->universe(), quint32(1));

    // The Place control lists them the same way.
    PatchInventoryDialog again(m_doc);
    QCOMPARE(again.m_placeUni->itemText(0), QString("Booth"));
}

void PatchInventoryDialog_Test::editingIntoACollisionFlagsItAndBlocksApply()
{
    staged(m_doc, 8, "A");
    staged(m_doc, 8, "B");
    PatchInventoryDialog dlg(m_doc);
    QVERIFY(dlg.m_apply->isEnabled());

    // Put B on top of A.
    qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(1, 6))->setValue(1);
    QVERIFY2(dlg.m_table->item(1, 8)->text().contains("CONFLICT"), qPrintable(dlg.m_table->item(1, 8)->text()));
    QVERIFY(!dlg.m_apply->isEnabled());
    QVERIFY(dlg.m_summary->text().contains("1 conflict"));

    // Move it clear again: applying is possible once more.
    qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(1, 6))->setValue(9);
    QVERIFY(dlg.m_apply->isEnabled());
}

void PatchInventoryDialog_Test::applyPatchesTheFixtures()
{
    Fixture *a = staged(m_doc, 8, "A");
    Fixture *b = staged(m_doc, 8, "B");
    PatchInventoryDialog dlg(m_doc);
    qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(1, 6))->setValue(101);   // hand-edit B

    dlg.accept();
    QVERIFY(a->isPatched() && b->isPatched());
    QCOMPARE(a->address(), quint32(0));
    QCOMPARE(b->address(), quint32(100));
    QCOMPARE(m_doc->fixtureForAddress(100), b->id());
}

void PatchInventoryDialog_Test::replanDiscardsEdits()
{
    staged(m_doc, 8, "A");
    staged(m_doc, 8, "B");
    PatchInventoryDialog dlg(m_doc);
    qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(1, 6))->setValue(300);
    QCOMPARE(dlg.currentPlan()[1].address, quint32(299));
    dlg.slotReplan();
    QCOMPARE(dlg.currentPlan()[1].address, quint32(8));
}

static Fixture *stagedTwoMode(Doc *doc, int nCh1, int nCh2, int heads2, QLCFixtureMode **basicOut,
                              QLCFixtureMode **extOut)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("TwoMode");
    QLCFixtureMode *basic = new QLCFixtureMode(def);
    basic->setName("Basic");
    QLCFixtureMode *ext = new QLCFixtureMode(def);
    ext->setName("Extended");
    for (int k = 0; k < qMax(nCh1, nCh2); k++)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("Ch %1").arg(k));
        ch->setGroup(QLCChannel::Intensity);
        def->addChannel(ch);
        if (k < nCh1) basic->insertChannel(ch, k);
        if (k < nCh2) ext->insertChannel(ch, k);
    }
    QLCFixtureHead h1;
    for (int k = 0; k < nCh1; k++) h1.addChannel(quint32(k));
    basic->insertHead(-1, h1);
    for (int hd = 0; hd < heads2; hd++)
    {
        QLCFixtureHead h;
        h.addChannel(quint32(hd));
        ext->insertHead(-1, h);
    }
    def->addMode(basic);
    def->addMode(ext);
    Fixture *f = new Fixture(doc);
    f->setFixtureDefinition(def, basic);
    f->setName("TwoMode 1");
    f->setPatched(false);
    if (!doc->addFixture(f))
        return nullptr;
    *basicOut = basic;
    *extOut = ext;
    return f;
}

void PatchInventoryDialog_Test::aGenericDimmersChannelsAreChosenHere()
{
    Fixture *a = staged(m_doc, 4, "A");
    staged(m_doc, 4, "B");
    PatchInventoryDialog dlg(m_doc);
    QCOMPARE(dlg.currentPlan()[1].address, quint32(4));
    QVERIFY(dlg.m_apply->isEnabled());

    // Make A eight channels: it now runs into B, and the dialog says so.
    QSpinBox *sp = qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(0, 2));
    QVERIFY2(sp != nullptr, "a generic dimmer should offer a channel count");
    QCOMPARE(sp->value(), 4);
    sp->setValue(8);
    QCOMPARE(dlg.m_table->item(0, 3)->text(), QString("8"));
    QVERIFY2(dlg.m_table->item(1, 8)->text().contains("CONFLICT"), qPrintable(dlg.m_table->item(1, 8)->text()));
    QVERIFY(!dlg.m_apply->isEnabled());

    // Re-plan keeps the footprint decision and moves B clear of it.
    dlg.slotReplan();
    QCOMPARE(qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(0, 2))->value(), 8);
    QCOMPARE(dlg.currentPlan()[1].address, quint32(8));
    QVERIFY(dlg.m_apply->isEnabled());

    dlg.accept();
    QCOMPARE(a->channels(), quint32(8));
    QVERIFY(a->isPatched());
    QCOMPARE(m_doc->fixtureForAddress(7), a->id());
}

void PatchInventoryDialog_Test::aModeIsChosenFromTheDefinitionsModes()
{
    QLCFixtureMode *basic = nullptr, *ext = nullptr;
    Fixture *f = stagedTwoMode(m_doc, 4, 8, 1, &basic, &ext);
    QVERIFY(f != nullptr);

    PatchInventoryDialog dlg(m_doc);
    QComboBox *cb = qobject_cast<QComboBox *>(dlg.m_table->cellWidget(0, 2));
    QVERIFY2(cb != nullptr, "a multi-mode fixture should offer its modes");
    QCOMPARE(cb->count(), 2);
    QCOMPARE(dlg.m_table->item(0, 3)->text(), QString("4"));

    cb->setCurrentIndex(1);                                    // Extended
    QCOMPARE(dlg.m_table->item(0, 3)->text(), QString("8"));

    dlg.accept();
    QCOMPARE(f->fixtureMode(), ext);
    QCOMPARE(f->channels(), quint32(8));
    QVERIFY(f->isPatched());
}

void PatchInventoryDialog_Test::modesTheFixtureCannotMoveToAreOfferedButDisabled()
{
    QLCFixtureMode *basic = nullptr, *ext = nullptr;
    Fixture *f = stagedTwoMode(m_doc, 4, 8, 8, &basic, &ext);   // Extended: 8 heads vs 1
    QVERIFY(f != nullptr);
    // On Extended, with a group laying out its 8 heads: Basic would lose 2..8.
    f->setFixtureDefinition(f->fixtureDef(), ext);
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Pixels");
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->setSize(QSize(8, 1));
    for (int h = 0; h < 8; ++h)
        grp->assignHead(QLCPoint(h, 0), GroupHead(f->id(), h));

    PatchInventoryDialog dlg(m_doc);
    QComboBox *cb = qobject_cast<QComboBox *>(dlg.m_table->cellWidget(0, 2));
    QVERIFY(cb != nullptr);
    QStandardItemModel *cm = qobject_cast<QStandardItemModel *>(cb->model());
    QVERIFY(cm != nullptr);
    QVERIFY(cm->item(1)->isEnabled());                          // where it already is
    QVERIFY2(!cm->item(0)->isEnabled(), "a mode losing heads a group uses must be disabled");
    QVERIFY(cm->item(0)->toolTip().contains("Pixels"));
}

void PatchInventoryDialog_Test::aSceneThatSetsChannelsLocksTheChannelCount()
{
    Fixture *a = staged(m_doc, 4, "Locked");
    Scene *sc = new Scene(m_doc);
    sc->setName("Hand Built");
    sc->setValue(a->id(), 1, 255);
    QVERIFY(m_doc->addFunction(sc));

    PatchInventoryDialog dlg(m_doc);
    QSpinBox *sp = qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(0, 2));
    QVERIFY(sp != nullptr);
    QVERIFY2(!sp->isEnabled(), "a scene sets this fixture's channels by number");
    QVERIFY(sp->toolTip().contains("Hand Built"));
}

void PatchInventoryDialog_Test::replanKeepsTheModesChosen()
{
    QLCFixtureMode *basic = nullptr, *ext = nullptr;
    Fixture *f = stagedTwoMode(m_doc, 4, 8, 1, &basic, &ext);
    QVERIFY(f != nullptr);
    Fixture *d = staged(m_doc, 2, "Dimmer");

    PatchInventoryDialog dlg(m_doc);
    const int modeRow = dlg.currentPlan()[0].fixtureId == f->id() ? 0 : 1;
    qobject_cast<QComboBox *>(dlg.m_table->cellWidget(modeRow, 2))->setCurrentIndex(1);
    qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(1 - modeRow, 2))->setValue(6);

    dlg.slotReplan();
    QCOMPARE(qobject_cast<QComboBox *>(dlg.m_table->cellWidget(modeRow, 2))->currentIndex(), 1);
    QCOMPARE(dlg.m_table->item(modeRow, 3)->text(), QString("8"));
    QCOMPARE(qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(1 - modeRow, 2))->value(), 6);
    QCOMPARE(dlg.rowChannels(1 - modeRow), quint32(6));
    Q_UNUSED(d)
}

void PatchInventoryDialog_Test::changingOneRowsModeLeavesTheOthers()
{
    staged(m_doc, 2, "A");
    staged(m_doc, 2, "B");
    PatchInventoryDialog dlg(m_doc);
    qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(1, 2))->setValue(12);   // B only
    QCOMPARE(dlg.m_table->item(0, 3)->text(), QString("2"));
    QCOMPARE(dlg.m_table->item(1, 3)->text(), QString("12"));

    // While the table is being filled, mode widgets settling do not re-enter.
    dlg.m_filling = true;
    qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(0, 2))->setValue(20);
    QCOMPARE(dlg.m_table->item(0, 3)->text(), QString("2"));
    dlg.m_filling = false;
}

void PatchInventoryDialog_Test::aSingleModeFixtureShowsItsModeAsText()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("OneMode");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Only");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setGroup(QLCChannel::Intensity);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    Fixture *f = new Fixture(m_doc);
    f->setFixtureDefinition(def, mode);
    f->setPatched(false);
    QVERIFY(m_doc->addFixture(f));

    PatchInventoryDialog dlg(m_doc);
    QCOMPARE(dlg.m_table->cellWidget(0, 2), nullptr);          // nothing to choose
    QCOMPARE(dlg.m_table->item(0, 2)->text(), QString("Only"));
    QCOMPARE(dlg.m_table->item(0, 1)->text(), QString("OneMode"));
    QVERIFY(dlg.currentPlan().first().mode == nullptr);
    QCOMPARE(dlg.currentPlan().first().dimmerChannels, quint32(0));
}

void PatchInventoryDialog_Test::aCollisionWithTheShowIsFlaggedToo()
{
    Fixture *show = new Fixture(m_doc);
    show->setChannels(10);                                       // 1-10 already patched
    QVERIFY(m_doc->addFixture(show));
    staged(m_doc, 4, "New");

    PatchInventoryDialog dlg(m_doc);
    QCOMPARE(dlg.currentPlan().first().address, quint32(10));  // planned clear of it
    QVERIFY(dlg.m_apply->isEnabled());

    qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(0, 6))->setValue(8);   // onto 8-11
    QVERIFY(dlg.m_table->item(0, 8)->text().contains("CONFLICT"));
    QVERIFY(!dlg.m_apply->isEnabled());
}

void PatchInventoryDialog_Test::newUniversesAndNotesAreSpelledOut()
{
    Fixture *f1 = staged(m_doc, 3, "Fence 1");
    f1->setConnection(Fixture::Network);
    f1->setIPAddress("172.18.2.41");
    Fixture *f2 = staged(m_doc, 3, "Fence 2");
    f2->setConnection(Fixture::Network);                          // no IP: the planner notes it
    // Every universe the show has is in use: a universe of its own is a new one.
    for (quint32 u = 0; u < m_doc->inputOutputMap()->universesCount(); ++u)
    {
        Fixture *busy = new Fixture(m_doc);
        busy->setChannels(1);
        busy->setUniverse(u);
        QVERIFY(m_doc->addFixture(busy));
    }

    PatchInventoryDialog dlg(m_doc);
    QCOMPARE(dlg.m_table->rowCount(), 2);
    for (int r = 0; r < 2; r++)
    {
        const QString st = dlg.m_table->item(r, 8)->text();
        QVERIFY2(st.startsWith("new universe"), qPrintable(st));
        const bool noIp = dlg.m_table->item(r, 0)->text() == "Fence 2";
        QCOMPARE(st.contains(" -- network fixture has no IP"), noIp);
        QCOMPARE(dlg.m_table->item(r, 4)->text(), noIp ? QString("Network") : QString("Network 172.18.2.41"));
    }
    QVERIFY2(dlg.m_summary->text().contains("2 new universe"), qPrintable(dlg.m_summary->text()));
}

void PatchInventoryDialog_Test::problemsDuringApplyAreReported()
{
    // A network fixture with nothing to carry it: it patches, but the user is
    // told its universe needs an ArtNet/sACN output.
    Fixture *f = staged(m_doc, 3, "Fence 1");
    f->setConnection(Fixture::Network);
    f->setIPAddress("172.18.2.41");

    QString shown;
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [&shown, poll]() {
        QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (box == nullptr)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        shown = box->text();
        box->accept();
    };
    QTimer::singleShot(0, *poll);

    PatchInventoryDialog dlg(m_doc);
    dlg.accept();
    QVERIFY(f->isPatched());
    QVERIFY2(shown.contains("Patched 1 fixture") && shown.contains("no ArtNet/sACN output"),
             qPrintable(shown));
    QCOMPARE(dlg.result(), int(QDialog::Accepted));
}

static NetworkNode nodeAt(const char *name, double x, double y, const QList<quint32> &unis)
{
    NetworkNode n;
    n.name = name;
    n.placed = true;
    n.posX = x;
    n.posY = y;
    foreach (quint32 u, unis)
    {
        NetworkPort p;
        p.universe = u;
        n.ports << p;
    }
    return n;
}

static Fixture *placed(Doc *doc, const QString &name, double x, double y)
{
    Fixture *f = staged(doc, 4, name);
    doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(float(x * 1000), float(y * 1000), 0));
    return f;
}

static int rowOf(PatchInventoryDialog &dlg, quint32 fid)
{
    for (int r = 0; r < dlg.m_table->rowCount(); r++)
        if (dlg.m_table->item(r, 0)->data(Qt::UserRole).toUInt() == fid)
            return r;
    return -1;
}

void PatchInventoryDialog_Test::nearestNodeProposalsShowWhereTheyGoAndApply()
{
    m_doc->networkTopology()->nodes() << nodeAt("SR Node", 0, 0, { 0 })
                                      << nodeAt("SL Node", 10, 0, { 1 });
    Fixture *t1 = placed(m_doc, "T1", 1, 0);
    Fixture *t4 = placed(m_doc, "T4", 9, 0);

    PatchInventoryDialog dlg(m_doc);
    QVERIFY(dlg.m_proximity->isEnabled() && dlg.m_proximity->isChecked());
    const int r1 = rowOf(dlg, t1->id()), r4 = rowOf(dlg, t4->id());
    QVERIFY2(dlg.m_table->item(r1, 7)->text() == QString::fromUtf8("SR Node · port 1 · 1.0 m"),
             qPrintable(dlg.m_table->item(r1, 7)->text()));
    QVERIFY2(dlg.m_table->item(r4, 7)->text().startsWith(QString::fromUtf8("SL Node · port 1")),
             qPrintable(dlg.m_table->item(r4, 7)->text()));

    dlg.accept();
    QCOMPARE(t1->universe(), quint32(0));
    QCOMPARE(t4->universe(), quint32(1));
}

void PatchInventoryDialog_Test::untickingNearestNodeProposesFirstFit()
{
    m_doc->networkTopology()->nodes() << nodeAt("SL Node", 10, 0, { 1 });
    Fixture *t4 = placed(m_doc, "T4", 9, 0);

    PatchInventoryDialog dlg(m_doc);
    QCOMPARE(dlg.currentPlan().first().universe, quint32(1));
    dlg.m_proximity->setChecked(false);                  // re-proposes on its own
    QCOMPARE(dlg.currentPlan().first().universe, quint32(0));
    QCOMPARE(dlg.currentPlan().first().nodeIndex, -1);
    QCOMPARE(dlg.m_table->item(rowOf(dlg, t4->id()), 7)->text(), QString::fromUtf8("—"));
}

void PatchInventoryDialog_Test::aHandEditedUniverseDropsTheProposedPort()
{
    // The node's only port is free: the proposal gives it a new universe.
    m_doc->networkTopology()->nodes()
        << nodeAt("SL Node", 10, 0, { 1, InputOutputMap::invalidUniverse() });
    Fixture *full = new Fixture(m_doc);
    full->setChannels(512);
    full->setUniverse(1);
    QVERIFY(m_doc->addFixture(full));                    // universe 2 is full
    Fixture *first = new Fixture(m_doc);
    first->setChannels(1);
    first->setUniverse(0);
    first->setAddress(511);
    QVERIFY(m_doc->addFixture(first));                   // universe 1 is in use (not free), at its end
    Fixture *t4 = placed(m_doc, "T4", 9, 0);

    PatchInventoryDialog dlg(m_doc);
    const int r = rowOf(dlg, t4->id());
    PatchPlanner::Assignment a = dlg.currentPlan().at(r);
    QVERIFY(a.claimsPort && a.portIndex == 1);
    QVERIFY2(dlg.m_table->item(r, 7)->text().contains("assigns the port"), qPrintable(dlg.m_table->item(r, 7)->text()));

    // Moved by hand to universe 1: no longer the free port's, and says what carries it.
    qobject_cast<UniverseCombo *>(dlg.m_table->cellWidget(r, 5))->setUniverse(0);
    a = dlg.currentPlan().at(r);
    QVERIFY(!a.claimsPort);
    QCOMPARE(a.nodeIndex, -1);
    QCOMPARE(dlg.m_table->item(r, 7)->text(), QString::fromUtf8("—"));   // nothing carries universe 1
    qobject_cast<UniverseCombo *>(dlg.m_table->cellWidget(r, 5))->setUniverse(1);
    QCOMPARE(dlg.m_table->item(r, 7)->text(), QString::fromUtf8("SL Node · port 1"));

    // Back to 1 and apply: the free port stays free.
    qobject_cast<UniverseCombo *>(dlg.m_table->cellWidget(r, 5))->setUniverse(0);
    dlg.accept();
    QCOMPARE(m_doc->networkTopology()->nodes().first().ports.at(1).universe,
             InputOutputMap::invalidUniverse());
}

void PatchInventoryDialog_Test::withNoNodesOnThePlotThereIsNothingToTick()
{
    NetworkNode unplaced = nodeAt("Somewhere", 0, 0, { 0 });
    unplaced.placed = false;
    m_doc->networkTopology()->nodes() << unplaced;
    placed(m_doc, "T1", 1, 0);
    PatchInventoryDialog dlg(m_doc);
    QVERIFY(!dlg.m_proximity->isEnabled());
    QVERIFY(!dlg.m_proximity->isChecked());
    QVERIFY(dlg.m_proximity->toolTip().contains("Place network nodes"));
}

/* Two like trusses, T-2's bars one channel off T-1's: Match on T-1 puts
   them at T-1's addresses in T-2's own universe, and Apply makes it so. */
void PatchInventoryDialog_Test::likeStructuresAreMatched()
{
    MonitorProperties *props = m_doc->monitorProperties();
    QList<quint32> ids;
    QHash<int, QList<Fixture *> > bars;
    QList<Truss *> trusses;
    for (int t = 0; t < 2; ++t)
    {
        Truss *truss = props->addTruss();
        truss->setName(QString("T-%1").arg(t + 1));
        trusses << truss;
        for (int b = 0; b < 2; ++b)
        {
            Fixture *f = new Fixture(m_doc);
            f->setChannels(90);
            f->setUniverse(quint32(5 + 2 * t));
            f->setAddress(quint32(b * 90 + t));      // T-2 one off
            f->setName(QString("T%1-B%2").arg(t + 1).arg(b + 1));
            QVERIFY(m_doc->addFixture(f));
            FixtureRigProps rp;
            rp.trussId = truss->id();
            rp.trussOffset = 0.5f + b;
            props->setFixtureRigProps(f->id(), rp);
            bars[t] << f;
            ids << f->id();
        }
    }
    PatchInventoryDialog dlg(m_doc, ids);
    QComboBox *pattern = dlg.findChild<QComboBox *>("matchPattern");
    QVERIFY2(pattern != nullptr, "fixtures on two structures should offer Match");
    QCOMPARE(pattern->count(), 2);
    const QString t1 = QString("truss:%1").arg(trusses.first()->id());
    QCOMPARE(dlg.matchStructures(t1), 2);
    dlg.accept();
    QCOMPARE(bars[1].at(0)->universe(), quint32(7));
    QCOMPARE(bars[1].at(0)->address(), quint32(0));
    QCOMPARE(bars[1].at(1)->address(), quint32(90));
    QCOMPARE(bars[0].at(1)->address(), quint32(90));  // the pattern stays put
}

QTEST_MAIN(PatchInventoryDialog_Test)

/*****************************************************************************
 * Re-addressing fixtures that are already patched
 *****************************************************************************/

// The dialog's column numbers, as the tests above spell them.
static const int COL_MODE = 2;
static const int COL_STATUS = 8;

static Fixture *patched(Doc *doc, int channels, quint32 uni, quint32 addr, const QString &name)
{
    Fixture *f = new Fixture(doc);
    f->setChannels(channels);
    f->setUniverse(uni);
    f->setAddress(addr);
    f->setName(name);
    doc->addFixture(f);
    return f;
}

void PatchInventoryDialog_Test::patchedFixturesStartWhereTheyAre()
{
    Fixture *b = patched(m_doc, 4, 0, 40, "B");
    Fixture *a = patched(m_doc, 4, 0, 20, "A");
    Fixture *s = staged(m_doc, 4, "S");
    PatchInventoryDialog dlg(m_doc, { b->id(), s->id(), a->id() });
    QCOMPARE(dlg.windowTitle(), QString("Patch / Re-address"));
    QCOMPARE(dlg.m_table->rowCount(), 3);
    const QList<PatchPlanner::Assignment> plan = dlg.currentPlan();
    // Patched ones first, in address order, where they are.
    QCOMPARE(plan[0].fixtureId, a->id());
    QCOMPARE(plan[0].address, quint32(20));
    QCOMPARE(plan[1].fixtureId, b->id());
    QCOMPARE(plan[2].fixtureId, s->id());
    QCOMPARE(dlg.m_table->item(0, COL_STATUS)->text(), QString("unchanged"));
    // Their mode can change here too ("change the mode to accommodate").
    QVERIFY(dlg.m_table->cellWidget(0, COL_MODE) != nullptr);
    // The staged one still has somewhere to go, so there is something to apply.
    QVERIFY(dlg.m_apply->isEnabled());

    PatchInventoryDialog onlyPatched(m_doc, { a->id(), b->id() });
    QVERIFY2(!onlyPatched.m_apply->isEnabled(), "nothing moves, nothing to apply");
}

void PatchInventoryDialog_Test::placeMovesTheSelectionAsABlock()
{
    Fixture *a = patched(m_doc, 4, 0, 20, "A");
    Fixture *b = patched(m_doc, 4, 0, 40, "B");
    Fixture *c = patched(m_doc, 4, 0, 60, "C");
    PatchInventoryDialog dlg(m_doc, { a->id(), b->id(), c->id() });

    // A and B (rows 0, 1) to universe 3, address 101, keeping their 20 apart.
    dlg.placeRows(2, 100, true, { 0, 1 });
    const QList<PatchPlanner::Assignment> plan = dlg.currentPlan();
    QCOMPARE(plan[0].universe, quint32(2));
    QCOMPARE(plan[0].address, quint32(100));
    QCOMPARE(plan[1].address, quint32(120));
    QCOMPARE(plan[2].universe, quint32(0));          // C was not selected
    QCOMPARE(plan[2].address, quint32(60));
    QVERIFY(dlg.m_table->item(0, COL_STATUS)->text().startsWith("moves from 1.21"));
    QVERIFY(dlg.m_apply->isEnabled());
}

void PatchInventoryDialog_Test::placePacksAroundTheOtherRows()
{
    Fixture *a = patched(m_doc, 4, 0, 20, "A");
    Fixture *b = patched(m_doc, 4, 0, 40, "B");
    Fixture *c = patched(m_doc, 4, 0, 60, "C");
    PatchInventoryDialog dlg(m_doc, { a->id(), b->id(), c->id() });

    // C first, by hand, to 1..4 -- then pack A and B from 1: they go round it,
    // not onto where C will be (nor treat C's old 61..64 as taken).
    dlg.placeRows(0, 0, false, { 2 });
    dlg.placeRows(0, 0, false, { 0, 1 });
    const QList<PatchPlanner::Assignment> plan = dlg.currentPlan();
    QCOMPARE(plan[2].address, quint32(0));
    QCOMPARE(plan[0].address, quint32(4));
    QCOMPARE(plan[1].address, quint32(8));
    QCOMPARE(dlg.m_summary->text().contains("0 conflict"), true);
}

void PatchInventoryDialog_Test::applyMovesABlockOntoItsOwnChannels()
{
    Fixture *a = patched(m_doc, 4, 0, 0, "A");
    Fixture *b = patched(m_doc, 4, 0, 4, "B");
    PatchInventoryDialog dlg(m_doc, { a->id(), b->id() });
    dlg.placeRows(0, 2, true);                       // up by two, onto themselves
    QVERIFY(dlg.m_apply->isEnabled());
    dlg.accept();
    QCOMPARE(dlg.result(), int(QDialog::Accepted));
    QCOMPARE(a->address(), quint32(2));
    QCOMPARE(b->address(), quint32(6));
    QCOMPARE(m_doc->fixtureForAddress(0), Fixture::invalidId());
}

void PatchInventoryDialog_Test::aClashSaysWhereItWantedToBe()
{
    Fixture *owner = patched(m_doc, 8, 1, 10, "Owner");
    Fixture *clash = staged(m_doc, 8, "Clash");
    clash->blockSignals(true);
    clash->setUniverse(1);
    clash->setAddress(12);
    clash->blockSignals(false);
    clash->setAddressClash(true);

    // Opened with no list: every unpatched fixture, the clash among them.
    PatchInventoryDialog dlg(m_doc);
    QCOMPARE(dlg.m_table->rowCount(), 1);
    const QString note = dlg.m_table->item(0, COL_STATUS)->text();
    QVERIFY2(note.contains("universe 2, address 13") && note.contains(owner->name()),
             qPrintable(note));
    dlg.accept();
    QVERIFY(clash->isPatched());
    QVERIFY(!clash->addressClash());
}

void PatchInventoryDialog_Test::placeCountingUpGivesEachTheNextUniverse()
{
    QList<Fixture *> lanterns;
    for (int i = 1; i <= 3; ++i)
    {
        lanterns << staged(m_doc, 15, QString("Lantern #%1").arg(i));
    }
    Fixture *busy = new Fixture(m_doc);                  // universe 7 is in use
    busy->setChannels(4);
    busy->setUniverse(6);
    QVERIFY(m_doc->addFixture(busy));

    PatchInventoryDialog dlg(m_doc);
    dlg.placeCountingUp(19, 0);                          // from universe 20, address 1
    QHash<quint32, quint32> uni;
    foreach (const PatchPlanner::Assignment &a, dlg.currentPlan())
        uni.insert(a.fixtureId, a.universe);
    QCOMPARE(uni.value(lanterns.at(0)->id()), quint32(19));
    QCOMPARE(uni.value(lanterns.at(1)->id()), quint32(20));
    QCOMPARE(uni.value(lanterns.at(2)->id()), quint32(21));
    QVERIFY(dlg.m_apply->isEnabled());

    // Counting up into a universe something else uses: said, not applied.
    dlg.placeCountingUp(4, 0);                           // 5, 6, 7 -- and 7 is busy
    bool conflict = false;
    for (int r = 0; r < dlg.m_table->rowCount(); ++r)
        conflict = conflict || dlg.m_table->item(r, 8)->text().contains("CONFLICT");
    QVERIFY(conflict);
    QVERIFY(!dlg.m_apply->isEnabled());

    // Back up clear and apply: patched where they were counted.
    dlg.placeCountingUp(19, 0);
    dlg.accept();
    QCOMPARE(lanterns.at(2)->universe(), quint32(21));
    QVERIFY(lanterns.at(2)->isPatched());
}

/* "when I tried to patch the rest of the lights on T1 to the t1-int universe
   .. it tried to create conflicts vs using what was available": two bars in
   it already, two more lights to come, one of them patched elsewhere. */
void PatchInventoryDialog_Test::fitKeepsWhatIsThereAndFillsTheGaps()
{
    Fixture *b1 = patched(m_doc, 90, 5, 0, "T1-B1");
    Fixture *b2 = patched(m_doc, 90, 5, 90, "T1-B2");
    Fixture *sw = patched(m_doc, 165, 0, 106, "T1-SW");
    Fixture *cw = staged(m_doc, 165, "T1-CW");
    m_doc->inputOutputMap()->addUniverse();          // enough universes for 5
    while (m_doc->inputOutputMap()->universesCount() < 7)
        m_doc->inputOutputMap()->addUniverse();
    PatchInventoryDialog dlg(m_doc, { b1->id(), b2->id(), sw->id(), cw->id() });

    QComboBox *how = dlg.findChild<QComboBox *>("placeHow");
    QVERIFY(how != nullptr);
    QCOMPARE(how->currentIndex(), 0);                // into the free space, by default
    QCOMPARE(dlg.fitRows(5, 0), 0);
    QHash<quint32, PatchPlanner::Assignment> at;
    foreach (const PatchPlanner::Assignment &a, dlg.currentPlan())
        at.insert(a.fixtureId, a);
    QCOMPARE(at[b1->id()].address, quint32(0));      // the bars stay put
    QCOMPARE(at[b2->id()].address, quint32(90));
    QCOMPARE(at[sw->id()].universe, quint32(5));     // the rest fill the gap
    QCOMPARE(at[cw->id()].universe, quint32(5));
    QList<quint32> starts = { at[sw->id()].address, at[cw->id()].address };
    std::sort(starts.begin(), starts.end());
    QCOMPARE(starts, QList<quint32>({ 180, 345 }));
    QVERIFY(dlg.findChild<QWidget *>("fitBar")->isHidden());
    QVERIFY2(dlg.m_apply->isEnabled(), "no conflicts: it can be applied");
    dlg.accept();
    QCOMPARE(sw->universe(), quint32(5));
    QCOMPARE(b1->address(), quint32(0));
}

/* "flag if it goes over 1 U .. tho I want the option to continue that patch" */
void PatchInventoryDialog_Test::whatHasNoRoomIsFlaggedAndCanCarryOn()
{
    patched(m_doc, 400, 1, 0, "Big");                // not in the dialog: stays
    Fixture *a = staged(m_doc, 60, "A");
    Fixture *b = staged(m_doc, 80, "B");
    while (m_doc->inputOutputMap()->universesCount() < 3)
        m_doc->inputOutputMap()->addUniverse();
    PatchInventoryDialog dlg(m_doc, { a->id(), b->id() });
    const QList<PatchPlanner::Assignment> before = dlg.currentPlan();

    QCOMPARE(dlg.fitRows(1, 0), 1);                  // 112 free: A fits, B doesn't
    QWidget *bar = dlg.findChild<QWidget *>("fitBar");
    QVERIFY(!bar->isHidden());
    QHash<quint32, PatchPlanner::Assignment> at;
    foreach (const PatchPlanner::Assignment &p, dlg.currentPlan())
        at.insert(p.fixtureId, p);
    QCOMPARE(at[a->id()].universe, quint32(1));
    QCOMPARE(at[a->id()].address, quint32(400));
    foreach (const PatchPlanner::Assignment &p, before)
        if (p.fixtureId == b->id())
        {
            QCOMPARE(at[b->id()].universe, p.universe);   // left where it was
            QCOMPARE(at[b->id()].address, p.address);
        }
    int row = -1;
    for (int r = 0; r < dlg.m_table->rowCount(); r++)
        if (dlg.m_table->item(r, 0)->data(Qt::UserRole).toUInt() == b->id())
            row = r;
    QVERIFY(dlg.m_table->item(row, COL_STATUS)->text().contains("no room"));

    dlg.findChild<QPushButton *>("fitCarryOn")->click();
    at.clear();
    foreach (const PatchPlanner::Assignment &p, dlg.currentPlan())
        at.insert(p.fixtureId, p);
    QCOMPARE(at[b->id()].universe, quint32(2));      // on into the next
    QCOMPARE(at[b->id()].address, quint32(0));
    QCOMPARE(at[a->id()].address, quint32(400));     // A stays
    QVERIFY(bar->isHidden());
}

/* "I want to be able to change the mode to accommodate where possible. That
   should update the fixture" */
void PatchInventoryDialog_Test::fitByChangingModesShrinksTheBiggestFirst()
{
    QLCFixtureMode *big1 = nullptr, *small1 = nullptr, *big2 = nullptr, *small2 = nullptr;
    Fixture *p = stagedTwoMode(m_doc, 200, 50, 1, &big1, &small1);   // Basic 200, Extended 50
    Fixture *q = stagedTwoMode(m_doc, 180, 40, 1, &big2, &small2);
    QVERIFY(p != nullptr && q != nullptr);
    QVERIFY(m_doc->patchFixtures({ { q->id(), 0, 300 } }));          // q already patched
    patched(m_doc, 200, 1, 0, "Taken");              // 312 free in universe 2
    while (m_doc->inputOutputMap()->universesCount() < 2)
        m_doc->inputOutputMap()->addUniverse();
    PatchInventoryDialog dlg(m_doc, { p->id(), q->id() });

    QCOMPARE(dlg.fitRows(1, 0), 1);                  // 200 + 180 > 312
    QCOMPARE(dlg.fitByChangingModes(), 0);           // the biggest (p) steps down: 50 + 180
    QHash<quint32, PatchPlanner::Assignment> at;
    foreach (const PatchPlanner::Assignment &a, dlg.currentPlan())
        at.insert(a.fixtureId, a);
    QCOMPARE(at[p->id()].mode, small1);
    QVERIFY(at[q->id()].mode == big2 || at[q->id()].mode == nullptr);
    QVERIFY(dlg.findChild<QWidget *>("fitBar")->isHidden());
    QVERIFY(dlg.m_apply->isEnabled());

    dlg.accept();                                    // the fixtures change
    QCOMPARE(p->fixtureMode(), small1);
    QCOMPARE(p->channels(), quint32(50));
    QCOMPARE(q->fixtureMode(), big2);
    QCOMPARE(q->universe(), quint32(1));

    // A patched fixture's mode is chosen here too, and applied.
    PatchInventoryDialog again(m_doc, { q->id() });
    QComboBox *cb = qobject_cast<QComboBox *>(again.m_table->cellWidget(0, COL_MODE));
    QVERIFY(cb != nullptr);
    cb->setCurrentIndex(cb->findText("Extended", Qt::MatchStartsWith));
    QCOMPARE(again.m_table->item(0, COL_STATUS)->text(), QString("mode changes"));
    QVERIFY(again.m_apply->isEnabled());
    again.accept();
    QCOMPARE(q->fixtureMode(), small2);
    QCOMPARE(q->channels(), quint32(40));
}

/* "still not patching the same all the time .. ordered by fixture name ..
   then the addressing should be in that order as well" */
void PatchInventoryDialog_Test::theSameGroupLandsTheSameWayInNameOrder()
{
    while (m_doc->inputOutputMap()->universesCount() < 3)
        m_doc->inputOutputMap()->addUniverse();
    // Scattered to start with: one in the target already, out of order.
    Fixture *sw = patched(m_doc, 30, 2, 0, "T1-SW");
    Fixture *b10 = patched(m_doc, 20, 2, 200, "T1-B10");
    Fixture *b2 = staged(m_doc, 20, "T1-B2");
    Fixture *cw = patched(m_doc, 30, 0, 50, "T1-CW");
    patched(m_doc, 10, 2, 100, "Other");             // not in the group: worked around

    auto run = [&]() {
        PatchInventoryDialog dlg(m_doc, { sw->id(), cw->id(), b10->id(), b2->id() });
        dlg.setAddressOrder(PatchPlanner::ByName);
        QStringList rows;
        for (int r = 0; r < dlg.m_table->rowCount(); r++)
            rows << dlg.m_table->item(r, 0)->text();
        QCOMPARE(rows, QStringList({ "T1-B2", "T1-B10", "T1-CW", "T1-SW" }));   // numbers as numbers
        QCOMPARE(dlg.fitRows(2, 0), 0);
        QHash<quint32, quint32> at;
        foreach (const PatchPlanner::Assignment &a, dlg.currentPlan())
            at.insert(a.fixtureId, a.address);
        QCOMPARE(at[b2->id()], quint32(0));          // in name order, around "Other" at 100
        QCOMPARE(at[b10->id()], quint32(20));
        QCOMPARE(at[cw->id()], quint32(40));
        QCOMPARE(at[sw->id()], quint32(70));
        dlg.accept();
    };
    run();
    // Shuffle them about and do it again: the same.
    QVERIFY(m_doc->patchFixtures({ { sw->id(), 2, 300 }, { cw->id(), 2, 400 }, { b2->id(), 1, 7 } }));
    run();
}

void PatchInventoryDialog_Test::moversFirstThenByKind()
{
    auto typed = [this](QLCFixtureDef::FixtureType type, const QString &name) {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test");
        def->setModel(name + " model");
        def->setType(type);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("Mode");
        QLCChannel *ch = new QLCChannel();
        ch->setName("Dimmer");
        def->addChannel(ch);
        mode->insertChannel(ch, 0);
        def->addMode(mode);
        Fixture *f = new Fixture(m_doc);
        f->setFixtureDefinition(def, mode);
        f->setName(name);
        f->setPatched(false);
        m_doc->addFixture(f);
        return f;
    };
    Fixture *wash = typed(QLCFixtureDef::ColorChanger, "A Wash");
    Fixture *bar = typed(QLCFixtureDef::LEDBarPixels, "B Bar");
    Fixture *spot = typed(QLCFixtureDef::MovingHead, "Z Spot");
    Fixture *beam = typed(QLCFixtureDef::MovingHead, "C Beam");
    PatchInventoryDialog dlg(m_doc, { wash->id(), bar->id(), spot->id(), beam->id() });
    dlg.setAddressOrder(PatchPlanner::ByKind);
    QStringList rows;
    for (int r = 0; r < dlg.m_table->rowCount(); r++)
        rows << dlg.m_table->item(r, 0)->text();
    QCOMPARE(rows, QStringList({ "C Beam", "Z Spot", "B Bar", "A Wash" }));
    QCOMPARE(dlg.fitRows(0, 0), 0);
    QHash<quint32, quint32> at;
    foreach (const PatchPlanner::Assignment &a, dlg.currentPlan())
        at.insert(a.fixtureId, a.address);
    QCOMPARE(at[beam->id()], quint32(0));
    QCOMPARE(at[spot->id()], quint32(1));
    QCOMPARE(at[bar->id()], quint32(2));
    QCOMPARE(at[wash->id()], quint32(3));
    dlg.setAddressOrder(PatchPlanner::ByName);       // back, for the next test's dialog
}

/*
  Q Light Controller Plus - Test Unit
  fixturetreewidget_test.cpp

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
#include <QTreeWidgetItemIterator>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QKeyEvent>
#include <QMimeData>

#define protected public
#define private public
#include "fixturetreewidget.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "inputoutputmap.h"
#include "doc.h"
#undef private
#undef protected

#include "fixturetreewidget_test.h"

void FixtureTreeWidget_Test::init()
{
    m_doc = new Doc(this);
}

void FixtureTreeWidget_Test::cleanup()
{
    delete m_doc;
}

void FixtureTreeWidget_Test::treeCounts()
{
    Fixture* fxi = new Fixture(m_doc);
    fxi->setChannels(4);
    m_doc->addFixture(fxi);

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber);
    tree.updateTree();
    QVERIFY(tree.universeCount() >= 1);
    QVERIFY(tree.fixturesCount() >= 1);
    QVERIFY(tree.channelsCount() >= 4);
}

QTEST_MAIN(FixtureTreeWidget_Test)

void FixtureTreeWidget_Test::selectionSurvivesARebuild()
{
    /* updateTree() throws the whole tree away and builds it again. Anything
       that rebuilds for its own reasons -- setShowHeads(), which the Fixture
       Manager calls when it opens a group's LAYOUT EDITOR -- used to come back
       with nothing selected. The manager then sat on a tree whose selection
       contradicted the pane it had just opened, and the next selection signal
       from any source fell through to "nothing selected" and tore the editor
       down. That is the "open a group for layout and it jumps straight back to
       the tree" report, and it was intermittent precisely because it needed a
       second, unrelated event to land.

       Blocking signals during the rebuild (which the widget also does) only
       hides the immediate reentry. The state still has to be right afterwards. */
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Probe");
    fxi->setChannels(4);
    QVERIFY(m_doc->addFixture(fxi));

    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Layout Group");
    grp->assignFixture(fxi->id());
    m_doc->addFixtureGroup(grp);

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber
                                  | FixtureTreeWidget::ShowGroups);
    tree.updateTree();

    // Find and select the group's row.
    QTreeWidgetItem *groupItem = nullptr;
    QTreeWidgetItemIterator it(&tree);
    while (*it != nullptr)
    {
        if ((*it)->data(0, PROP_GROUP).isValid()
            && (*it)->data(0, PROP_GROUP).toUInt() == grp->id())
        {
            groupItem = *it;
            break;
        }
        ++it;
    }
    QVERIFY2(groupItem != nullptr, "the group never appeared in the tree");
    groupItem->setSelected(true);
    QCOMPARE(tree.selectedItems().size(), 1);

    /* Exactly what opening a layout editor does. */
    tree.setShowHeads(true);

    QCOMPARE(tree.selectedItems().size(), 1);
    const QVariant still = tree.selectedItems().first()->data(0, PROP_GROUP);
    QVERIFY2(still.isValid() && still.toUInt() == grp->id(),
             "the group stopped being selected when the tree rebuilt -- the "
             "Fixture Manager will tear its layout editor down on the next "
             "selection signal from anywhere");

    // And back again, which is what closing the editor does.
    tree.setShowHeads(false);
    QCOMPARE(tree.selectedItems().size(), 1);
    QCOMPARE(tree.selectedItems().first()->data(0, PROP_GROUP).toUInt(), grp->id());
}

/** Find the "Universe N" row for the given universe id, or nullptr. */
static QTreeWidgetItem *findUniverseItem(FixtureTreeWidget &tree, quint32 uni)
{
    QTreeWidgetItemIterator it(&tree);
    while (*it != nullptr)
    {
        if ((*it)->data(0, PROP_UNIVERSE).isValid()
            && (*it)->data(0, PROP_UNIVERSE).toUInt() == uni)
            return *it;
        ++it;
    }
    return nullptr;
}

void FixtureTreeWidget_Test::enterOnAUniverseRowRenamesIt()
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    QVERIFY(iomap->addUniverse());
    const quint32 uni = 0;

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber
                                  | FixtureTreeWidget::ShowGroups);
    tree.updateTree();

    QTreeWidgetItem *uniItem = findUniverseItem(tree, uni);
    QVERIFY2(uniItem != nullptr, "the universe never appeared in the tree");

    tree.setCurrentItem(uniItem);
    QKeyEvent enterEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    tree.keyPressEvent(&enterEvent);

    QVERIFY2((uniItem->flags() & Qt::ItemIsEditable) != 0,
             "Enter did not arm the universe row for editing");
    QCOMPARE(uniItem->text(0), iomap->getUniverseNameByID(uni));

    // What the real inline editor does on commit: change the text, which
    // fires itemChanged(), wired to slotItemChanged() in the constructor.
    uniItem->setText(0, "Renamed Universe");

    QCOMPARE(iomap->getUniverseNameByID(uni), QString("Renamed Universe"));
    QVERIFY2((uniItem->flags() & Qt::ItemIsEditable) == 0,
             "the row stayed armed after the edit committed");
}

void FixtureTreeWidget_Test::emptyUniverseEditIsRejected()
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    QVERIFY(iomap->addUniverse());
    const quint32 uni = 0;
    iomap->setUniverseName(int(uni), "Original Name");

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber
                                  | FixtureTreeWidget::ShowGroups);
    tree.updateTree();

    QTreeWidgetItem *uniItem = findUniverseItem(tree, uni);
    QVERIFY2(uniItem != nullptr, "the universe never appeared in the tree");

    tree.setCurrentItem(uniItem);
    QKeyEvent enterEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    tree.keyPressEvent(&enterEvent);

    uniItem->setText(0, "");   // cleared the field, then committed

    QCOMPARE(iomap->getUniverseNameByID(uni), QString("Original Name"));
    QCOMPARE(uniItem->text(0), QString("Original Name"));
}

static QTreeWidgetItem *findByRole(FixtureTreeWidget &tree, int role, const QVariant &v = QVariant())
{
    QTreeWidgetItemIterator it(&tree);
    while (*it != nullptr)
    {
        const QVariant d = (*it)->data(0, role);
        if (d.isValid() && (!v.isValid() || d == v))
            return *it;
        ++it;
    }
    return nullptr;
}

void FixtureTreeWidget_Test::inventoryFixturesGetTheirOwnRootNotAUniverse()
{
    Fixture *real = new Fixture(m_doc);
    real->setName("Real");
    real->setChannels(4);
    QVERIFY(m_doc->addFixture(real));

    Fixture *inv = new Fixture(m_doc);
    inv->setName("Staged");
    inv->setChannels(4);
    inv->setPatched(false);
    QVERIFY(m_doc->addFixture(inv));

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber | FixtureTreeWidget::ShowGroups);
    tree.updateTree();

    QTreeWidgetItem *root = findByRole(tree, PROP_INVENTORY);
    QVERIFY2(root != nullptr, "no Inventory root");
    QVERIFY(root->text(0).startsWith("Inventory"));
    // Inventory -> manufacturer -> model -> fixture.
    QCOMPARE(root->childCount(), 1);
    QTreeWidgetItem *manuf = root->child(0);
    QCOMPARE(manuf->text(0), QString("Generic (1)"));
    QCOMPARE(manuf->childCount(), 1);
    QCOMPARE(manuf->child(0)->text(0), QString("Dimmer, 4 ch (1)"));
    QCOMPARE(manuf->child(0)->childCount(), 1);
    QCOMPARE(manuf->child(0)->child(0)->text(0), QString("Staged"));

    // The universe node holds the real fixture only.
    QTreeWidgetItem *uni0 = findByRole(tree, PROP_UNIVERSE, 0u);
    QVERIFY(uni0 != nullptr);
    QCOMPARE(uni0->childCount(), 1);
    QCOMPARE(uni0->child(0)->text(0), QString("Real"));

    // And the staged row says so in the universe column instead of "1".
    QVERIFY(manuf->child(0)->child(0)->text(1) != QString("1"));
}

void FixtureTreeWidget_Test::inventoryRootIsAlwaysPresentInTheManager()
{
    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber | FixtureTreeWidget::ShowGroups);
    tree.updateTree();
    QTreeWidgetItem *root = findByRole(tree, PROP_INVENTORY);
    QVERIFY(root != nullptr);
    QCOMPARE(root->text(0), QString("Inventory"));
    QCOMPARE(root->childCount(), 0);
}

void FixtureTreeWidget_Test::channelSelectionSkipsInventory()
{
    Fixture *inv = new Fixture(m_doc);
    inv->setChannels(4);
    inv->setPatched(false);
    QVERIFY(m_doc->addFixture(inv));

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::ChannelSelection);
    tree.updateTree();
    QCOMPARE(tree.topLevelItemCount(), 0);
}

static Fixture *stagedModel(Doc *doc, const char *manuf, const char *model, const char *name)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(manuf);
    def->setModel(model);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Std");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setGroup(QLCChannel::Intensity);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    def->addMode(mode);

    Fixture *f = new Fixture(doc);
    f->setFixtureDefinition(def, mode);
    f->setName(name);
    f->setPatched(false);
    return doc->addFixture(f) ? f : nullptr;
}

void FixtureTreeWidget_Test::inventoryIsOrganisedByManufacturerAndModel()
{
    // Added out of order on purpose: the tree must sort, not keep insertion order.
    QVERIFY(stagedModel(m_doc, "Robe", "Pointe", "Pointe 2"));
    QVERIFY(stagedModel(m_doc, "Martin", "MAC Viper", "Viper 1"));
    QVERIFY(stagedModel(m_doc, "Robe", "Pointe", "Pointe 1"));
    QVERIFY(stagedModel(m_doc, "Martin", "MAC Aura", "Aura 1"));
    QVERIFY(stagedModel(m_doc, "Robe", "Robin 600", "Robin 1"));

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber | FixtureTreeWidget::ShowGroups);
    tree.updateTree();

    QTreeWidgetItem *root = findByRole(tree, PROP_INVENTORY);
    QVERIFY(root != nullptr);
    QCOMPARE(root->text(0), QString("Inventory (5)"));
    QCOMPARE(root->childCount(), 2);

    QTreeWidgetItem *martin = root->child(0);
    QTreeWidgetItem *robe = root->child(1);
    QCOMPARE(martin->text(0), QString("Martin (2)"));
    QCOMPARE(robe->text(0), QString("Robe (3)"));

    QCOMPARE(martin->child(0)->text(0), QString("MAC Aura (1)"));       // alphabetical models
    QCOMPARE(martin->child(1)->text(0), QString("MAC Viper (1)"));
    QCOMPARE(robe->child(0)->text(0), QString("Pointe (2)"));
    QCOMPARE(robe->child(1)->text(0), QString("Robin 600 (1)"));

    QTreeWidgetItem *pointe = robe->child(0);
    QCOMPARE(pointe->child(0)->text(0), QString("Pointe 1"));            // alphabetical fixtures
    QCOMPARE(pointe->child(1)->text(0), QString("Pointe 2"));

    // Folders are marked so the manager can tell them from a fixture row.
    QCOMPARE(martin->data(0, PROP_INVENTORY).toInt(), 2);
    QVERIFY(!martin->data(0, PROP_ID).isValid());
    QVERIFY(pointe->child(0)->data(0, PROP_ID).isValid());
}

void FixtureTreeWidget_Test::addressClashesGetTheirOwnRootAtTheTop()
{
    Fixture *real = new Fixture(m_doc);
    real->setName("Owner");
    real->setChannels(8);
    QVERIFY(m_doc->addFixture(real));

    // What Fixture::loader() makes of a fixture the file puts on 3..10.
    Fixture *clash = new Fixture(m_doc);
    clash->setName("Clash");
    clash->setChannels(8);
    clash->setAddress(2);
    clash->setPatched(false);
    clash->setAddressClash(true);
    QVERIFY(m_doc->addFixture(clash));

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber | FixtureTreeWidget::AddressRange
                                  | FixtureTreeWidget::ShowGroups);
    tree.updateTree();

    QTreeWidgetItem *root = findByRole(tree, PROP_CLASHES);
    QVERIFY2(root != nullptr, "no Address clashes root");
    QCOMPARE(tree.topLevelItem(0), root);
    QCOMPARE(root->text(0), QString("Address clashes (1)"));
    QCOMPARE(root->childCount(), 1);
    QTreeWidgetItem *row = root->child(0);
    QCOMPARE(row->text(0), QString("Clash"));
    QVERIFY2(row->text(tree.m_addressColumn).contains("clashes at 1.3"),
             qPrintable(row->text(tree.m_addressColumn)));
    QVERIFY(row->toolTip(tree.m_addressColumn).contains("Owner"));

    // Not also counted as inventory waiting to be rigged.
    QTreeWidgetItem *inv = findByRole(tree, PROP_INVENTORY, 1);
    QVERIFY(inv != nullptr);
    QCOMPARE(inv->text(0), QString("Inventory"));

    // Once it has a home, the root goes.
    QVERIFY(m_doc->patchFixtures({ { clash->id(), 0, 100 } }));
    tree.updateTree();
    QVERIFY(findByRole(tree, PROP_CLASHES) == nullptr);
}

void FixtureTreeWidget_Test::aDropOnAUniverseNamesItAndItsFixtures()
{
    Fixture *a = new Fixture(m_doc);
    a->setName("A");
    a->setChannels(4);
    QVERIFY(m_doc->addFixture(a));
    Fixture *b = new Fixture(m_doc);
    b->setName("B");
    b->setChannels(4);
    b->setAddress(4);
    QVERIFY(m_doc->addFixture(b));
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Pair");
    grp->setSize(QSize(2, 1));
    m_doc->addFixtureGroup(grp);
    grp->assignFixture(b->id());
    grp->assignFixture(a->id());

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber | FixtureTreeWidget::ShowGroups);
    tree.updateTree();

    QTreeWidgetItem *uni = findByRole(tree, PROP_UNIVERSE, 0u);
    QVERIFY(uni != nullptr && uni->childCount() == 2);
    QCOMPARE(tree.dropUniverse(uni), QVariant(0u));
    QCOMPARE(tree.dropUniverse(uni->child(0)), QVariant(0u));        // a fixture in it
    QVERIFY(!tree.dropUniverse(findByRole(tree, PROP_INVENTORY, 1)).isValid());
    QVERIFY(!tree.dropUniverse(nullptr).isValid());

    // A fixture drag carries its fixtures; a group drag stands for its own.
    QMimeData fx;
    {
        QByteArray data;
        QDataStream out(&data, QIODevice::WriteOnly);
        out << a->id() << b->id() << a->id();
        fx.setData(FixtureTreeWidget::fixtureDragMimeType(), data);
    }
    QCOMPARE(tree.droppedFixtures(&fx), QList<quint32>({ a->id(), b->id() }));
    QMimeData gr;
    {
        QByteArray data;
        QDataStream out(&data, QIODevice::WriteOnly);
        out << grp->id();
        gr.setData(FixtureTreeWidget::groupDragMimeType(), data);
    }
    QCOMPARE(tree.droppedFixtures(&gr), QList<quint32>({ b->id(), a->id() }));
}

/* Two fixtures called "AT1-3z" (a show from before names were unique):
   listed at the top. Renaming another fixture to a taken name is refused. */
void FixtureTreeWidget_Test::duplicateNamesAreListedAndARenameToATakenNameIsRefused()
{
    Fixture *rigged = new Fixture(m_doc);
    rigged->setName("AT1-3z");
    rigged->setChannels(1);
    rigged->setAddress(447);
    QVERIFY(m_doc->addFixture(rigged));
    Fixture *stray = new Fixture(m_doc);
    stray->setName("AT1-3z");
    stray->setChannels(1);
    stray->setPatched(false);
    QVERIFY(m_doc->addFixture(stray));
    Fixture *other = new Fixture(m_doc);
    other->setName("SR 1");
    other->setChannels(1);
    other->setAddress(20);
    QVERIFY(m_doc->addFixture(other));

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber | FixtureTreeWidget::AddressRange
                                  | FixtureTreeWidget::ShowGroups);
    tree.updateTree();
    QTreeWidgetItem *root = findByRole(tree, PROP_DUPLICATES);
    QVERIFY2(root != nullptr, "no Duplicate names root");
    QCOMPARE(tree.topLevelItem(0), root);
    QCOMPARE(root->text(0), QString("Duplicate names (1)"));
    QCOMPARE(root->childCount(), 1);
    QCOMPARE(root->child(0)->text(0), QString("AT1-3z — 2 fixtures"));
    QVERIFY(root->child(0)->toolTip(0).contains("address 448"));
    QVERIFY(root->child(0)->toolTip(0).contains("in the inventory"));

    // Rename "SR 1" to "AT1-3z" in place: refused, with a note; name kept.
    QTreeWidgetItem *row = nullptr;
    for (QTreeWidgetItemIterator it(&tree); *it; ++it)
        if ((*it)->data(0, PROP_ID).isValid() && (*it)->data(0, PROP_ID).toUInt() == other->id()
            && !(*it)->data(0, PROP_HEAD).isValid() && !(*it)->data(0, PROP_CHANNEL).isValid())
            row = *it;
    QVERIFY(row != nullptr);
    QString note;
    QTimer watch;                       // the note is modal: answer it once it shows
    QObject::connect(&watch, &QTimer::timeout, [&note, &watch]() {
        if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
        {
            note = box->text();
            box->accept();
            watch.stop();
        }
    });
    watch.start(20);
    tree.blockSignals(true);                               // armed, as Return does
    row->setFlags(row->flags() | Qt::ItemIsEditable);
    tree.blockSignals(false);
    row->setText(0, "AT1-3z");
    watch.stop();
    QCOMPARE(other->name(), QString("SR 1"));
    QVERIFY2(note.contains("already called “AT1-3z”"), qPrintable(note));

    // Numbered apart: the root goes.
    QCOMPARE(m_doc->makeFixtureNamesUnique(), 1);
    tree.updateTree();
    QVERIFY(findByRole(tree, PROP_DUPLICATES) == nullptr);
}

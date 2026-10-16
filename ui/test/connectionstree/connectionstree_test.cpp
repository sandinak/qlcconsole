/*
  Q Light Controller Plus - Test Unit
  connectionstree_test.cpp

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

#include <QTreeWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <QtTest>
#include <QScrollBar>
#include <QMenu>
#include <QSettings>
#include <QSplitter>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QMessageBox>

#define protected public
#define private public

#include "connectionstree_test.h"
#include "connectionstree.h"
#include "universeoutputview.h"
#include "fixture.h"
#include "iopluginstub.h"
#include "ioplugincache.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "patchundo.h"
#include "universe.h"
#include "qlcfile.h"
#include "doc.h"
#include "apputil.h"
#include "showstatus.h"

#undef private
#undef protected

/* The tree stores its row metadata in item data roles defined privately in
   connectionstree.cpp. Mirrored here rather than exported: they are an
   internal encoding, and a test that needs them is by definition reaching
   inside. Kept adjacent in value order so a drift shows up as a failure
   rather than as silently reading the wrong role. */
#define ROLE_KIND       (Qt::UserRole + 1)
#define ROLE_PLUGIN     (Qt::UserRole + 2)
#define ROLE_LINE       (Qt::UserRole + 3)
#define ROLE_UNIVERSE   (Qt::UserRole + 4)
#define ROLE_OUTPUT     (Qt::UserRole + 5)
#define ROLE_ADDRESS    (Qt::UserRole + 6)
#define ROLE_PORTADDR   (Qt::UserRole + 7)
#define ROLE_LINENAME   (Qt::UserRole + 8)

#define KIND_PLUGIN   1
#define KIND_LINE     2
#define KIND_DEVICE   3
#define KIND_PORT     4
#define KIND_UNIVERSE 5

#define COL_NAME    0
#define COL_DETAIL  1
#define COL_CARRIES 2

#define TESTPLUGINDIR "../../../engine/test/iopluginstub"

static QDir testPluginDir()
{
    QDir dir(TESTPLUGINDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    return dir;
}

/** Every item in the tree, depth first. */
static QList<QTreeWidgetItem *> allItems(QTreeWidget *tree)
{
    QList<QTreeWidgetItem *> out, stack;
    for (int i = 0; i < tree->topLevelItemCount(); i++)
        stack << tree->topLevelItem(i);
    while (stack.isEmpty() == false)
    {
        QTreeWidgetItem *it = stack.takeFirst();
        out << it;
        for (int c = 0; c < it->childCount(); c++)
            stack << it->child(c);
    }
    return out;
}

static QList<QTreeWidgetItem *> itemsOfKind(QTreeWidget *tree, int kind)
{
    QList<QTreeWidgetItem *> out;
    foreach (QTreeWidgetItem *it, allItems(tree))
    {
        if (it->data(COL_NAME, ROLE_KIND).toInt() == kind)
            out << it;
    }
    return out;
}

static QTreeWidgetItem *itemWithAddress(QTreeWidget *tree, const QString &addr)
{
    foreach (QTreeWidgetItem *it, allItems(tree))
    {
        if (it->data(COL_NAME, ROLE_KIND).toInt() == KIND_DEVICE
                && it->data(COL_NAME, ROLE_ADDRESS).toString() == addr)
            return it;
    }
    return NULL;
}

static IOPluginStub *stubOf(Doc *doc)
{
    if (doc->ioPluginCache() == NULL || doc->ioPluginCache()->plugins().isEmpty())
        return NULL;
    return static_cast<IOPluginStub *>(doc->ioPluginCache()->plugins().at(0));
}

/** A discovered node on the given line. */
static QLCIOPlugin::Device makeDevice(quint32 line, const QString &name,
                                      const QString &address)
{
    QLCIOPlugin::Device d;
    d.line = line;
    d.name = name;
    d.address = address;
    d.hardwareId = "AA:BB:CC:DD:EE:FF";
    d.detail = "firmware 14";
    d.rdmCapable = true;
    return d;
}

void ConnectionsTree_Test::initTestCase()
{
    m_doc = NULL;
}

void ConnectionsTree_Test::cleanupTestCase()
{
}

void ConnectionsTree_Test::init()
{
    m_doc = new Doc(this);
    m_doc->ioPluginCache()->load(testPluginDir());
    QVERIFY2(m_doc->ioPluginCache()->plugins().isEmpty() == false,
             "the I/O plugin stub did not load -- check TESTPLUGINDIR");
    IOPluginStub *stub = stubOf(m_doc);
    QVERIFY(stub != NULL);
    stub->init();   // reset the scenario knobs between tests
}

void ConnectionsTree_Test::cleanup()
{
    /* "By universe" is remembered across sessions; a test that turned it on
       must not leave every later test looking at the universe list. */
    QSettings().setValue(QStringLiteral("connections/byUniverse"), false);
    delete m_doc;
    m_doc = NULL;
}

/****************************************************************************
 * Construction and the rebuild contract
 ****************************************************************************/

void ConnectionsTree_Test::initial()
{
    ConnectionsTree tree(m_doc);

    QVERIFY(tree.m_tree != NULL);
    QVERIFY(tree.m_status != NULL);
    QVERIFY(tree.m_showUnused != NULL);
    QVERIFY(tree.m_rescan != NULL);
    QVERIFY(tree.m_showUnused->isChecked() == false);

    // The constructor refreshes once, so the tree is usable before it is shown.
    QVERIFY(tree.m_populatedOnce == true);

    // The timer belongs to showEvent, not the constructor: a tab nobody has
    // opened must not be rebuilding itself every five seconds.
    QVERIFY(tree.m_refreshTimer != NULL);
    QVERIFY(tree.m_refreshTimer->isActive() == false);
}

void ConnectionsTree_Test::refreshLeavesEditingEnabled()
{
    ConnectionsTree tree(m_doc);

    // m_rebuilding suppresses slotItemChanged while rows are rewritten. If a
    // refresh ever returns with it still set, every inline rename silently
    // stops committing for the rest of the session.
    QVERIFY(tree.m_rebuilding == false);
    tree.refresh();
    QVERIFY(tree.m_rebuilding == false);
}

/****************************************************************************
 * Visibility gating
 ****************************************************************************/

void ConnectionsTree_Test::timerOnlyRunsWhileVisible()
{
    ConnectionsTree tree(m_doc);
    QVERIFY(tree.m_refreshTimer->isActive() == false);

    tree.show();
    QVERIFY(tree.m_refreshTimer->isActive() == true);

    tree.hide();
    QVERIFY(tree.m_refreshTimer->isActive() == false);

    tree.show();
    QVERIFY(tree.m_refreshTimer->isActive() == true);
}

void ConnectionsTree_Test::firstShowRescansAndRestartsTheClock()
{
    IOPluginStub *stub = stubOf(m_doc);
    ConnectionsTree tree(m_doc);

    QCOMPARE(stub->m_rescanCalled, 0);
    QVERIFY(tree.m_shownOnce == false);

    tree.show();

    // Arriving at the tab is the moment to go looking: an interface with
    // nothing patched is never polled otherwise.
    QVERIFY(tree.m_shownOnce == true);
    QVERIFY(stub->m_rescanCalled > 0);

    // A second show must not re-poll; only the first arrival is an event.
    const int after = stub->m_rescanCalled;
    tree.hide();
    tree.show();
    QCOMPARE(stub->m_rescanCalled, after);
}

/****************************************************************************
 * Rescan
 ****************************************************************************/

void ConnectionsTree_Test::rescanReachesEveryPlugin()
{
    IOPluginStub *stub = stubOf(m_doc);
    ConnectionsTree tree(m_doc);

    QCOMPARE(stub->m_rescanCalled, 0);
    tree.slotRescan();

    // The button drives QLCIOPlugin::rescan() on every plugin. USB plugins
    // implement it and used to be skipped entirely, so freshly plugged
    // hardware never appeared without restarting the app.
    QCOMPARE(stub->m_rescanCalled, 1);
}

/****************************************************************************
 * Which lines are worth showing
 ****************************************************************************/

void ConnectionsTree_Test::unpatchedLineIsHiddenByDefault()
{
    IOPluginStub *stub = stubOf(m_doc);
    stub->m_linesAreHardware = false;

    ConnectionsTree tree(m_doc);
    tree.refresh();

    // Nothing patched, nothing heard, not hardware: a stock build exposes
    // dozens of such lines and listing them buries the ones that matter.
    QCOMPARE(itemsOfKind(tree.m_tree, KIND_LINE).count(), 0);
}

void ConnectionsTree_Test::hardwareLineIsShownWithoutAPatch()
{
    IOPluginStub *stub = stubOf(m_doc);
    stub->m_linesAreHardware = true;

    ConnectionsTree tree(m_doc);
    tree.refresh();

    // For a plugin whose lines ARE its hardware, existing is the discovery.
    // This is the DMXKing case: enumerated at startup, then filtered away.
    QCOMPARE(itemsOfKind(tree.m_tree, KIND_LINE).count(), stub->m_lineCount);
}

void ConnectionsTree_Test::newlyAppearedLineIsShown()
{
    IOPluginStub *stub = stubOf(m_doc);
    stub->m_linesAreHardware = false;
    stub->m_lineCount = 2;

    ConnectionsTree tree(m_doc);
    tree.refresh();     // baseline: two lines, both filtered away
    QCOMPARE(itemsOfKind(tree.m_tree, KIND_LINE).count(), 0);
    QVERIFY(tree.m_linesBaselined == true);

    stub->m_lineCount = 3;
    tree.refresh();

    // Appearing IS the event. Exactly the new line shows; the two that were
    // already there stay filtered.
    QList<QTreeWidgetItem *> lines = itemsOfKind(tree.m_tree, KIND_LINE);
    QCOMPARE(lines.count(), 1);
    QCOMPARE(lines.at(0)->data(COL_NAME, ROLE_LINENAME).toString(),
             QString("3: Stub 3"));

    // ...and it stays visible rather than blinking out on the next tick.
    tree.refresh();
    QCOMPARE(itemsOfKind(tree.m_tree, KIND_LINE).count(), 1);

    // Leaving the tab is the end of "new".
    tree.show();
    tree.hide();
    QVERIFY(tree.m_newLines.isEmpty());
}

void ConnectionsTree_Test::showUnusedRevealsEverything()
{
    IOPluginStub *stub = stubOf(m_doc);
    stub->m_linesAreHardware = false;

    ConnectionsTree tree(m_doc);
    QCOMPARE(itemsOfKind(tree.m_tree, KIND_LINE).count(), 0);

    tree.m_showUnused->setChecked(true);   // toggled() is wired to refresh()
    QCOMPARE(itemsOfKind(tree.m_tree, KIND_LINE).count(), stub->m_lineCount);
}

/****************************************************************************
 * Discovered and hand-declared targets
 ****************************************************************************/

void ConnectionsTree_Test::discoveredDeviceAppearsUnderItsLine()
{
    IOPluginStub *stub = stubOf(m_doc);
    stub->m_devices << makeDevice(1, "CR041R", "172.18.2.10");

    ConnectionsTree tree(m_doc);
    tree.refresh();

    QTreeWidgetItem *dev = itemWithAddress(tree.m_tree, "172.18.2.10");
    QVERIFY(dev != NULL);

    // Under the line it was heard on, which is under its protocol. A node
    // filed under the wrong interface is worse than no node at all.
    QVERIFY(dev->parent() != NULL);
    QCOMPARE(dev->parent()->data(COL_NAME, ROLE_KIND).toInt(), KIND_LINE);
    QCOMPARE(dev->parent()->data(COL_NAME, ROLE_LINE).toUInt(), quint32(1));
    QVERIFY(dev->parent()->parent() != NULL);
    QCOMPARE(dev->parent()->parent()->data(COL_NAME, ROLE_KIND).toInt(), KIND_PLUGIN);
}

void ConnectionsTree_Test::manualTargetIsRendered()
{
    IOPluginStub *stub = stubOf(m_doc);
    m_doc->inputOutputMap()->addManualTarget(stub->name(), 0, "172.18.2.99");

    ConnectionsTree tree(m_doc);
    tree.refresh();

    QTreeWidgetItem *dev = itemWithAddress(tree.m_tree, "172.18.2.99");
    QVERIFY2(dev != NULL, "a hand-declared target must render even though "
                          "nothing has ever answered from it");
    QVERIFY(dev->text(COL_DETAIL).contains("not heard from"));
}

void ConnectionsTree_Test::discoveredTargetWinsOverManualOne()
{
    IOPluginStub *stub = stubOf(m_doc);
    m_doc->inputOutputMap()->addManualTarget(stub->name(), 0, "172.18.2.10");
    stub->m_devices << makeDevice(0, "CR041R", "172.18.2.10");

    ConnectionsTree tree(m_doc);
    tree.refresh();

    // One row, not two. A hand-declared target has no MAC, so the address|MAC
    // dedup key could never match the discovered one -- the manual entry would
    // otherwise sit next to the real node as a duplicate.
    int count = 0;
    foreach (QTreeWidgetItem *it, itemsOfKind(tree.m_tree, KIND_DEVICE))
    {
        if (it->data(COL_NAME, ROLE_ADDRESS).toString() == "172.18.2.10")
            count++;
    }
    QCOMPARE(count, 1);

    // And it is the live one: the discovered name, not the bare address.
    QTreeWidgetItem *dev = itemWithAddress(tree.m_tree, "172.18.2.10");
    QVERIFY(dev != NULL);
    QVERIFY(dev->text(COL_NAME).contains("CR041R"));
}

void ConnectionsTree_Test::manualTargetPortsAreRendered()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    iom->addManualTarget(stub->name(), 0, "172.18.2.99");
    iom->addManualPort("172.18.2.99", 0x011);   // net 0, sub 1, universe 1
    iom->addManualPort("172.18.2.99", 0x012);

    ConnectionsTree tree(m_doc);
    tree.refresh();

    QTreeWidgetItem *dev = itemWithAddress(tree.m_tree, "172.18.2.99");
    QVERIFY(dev != NULL);
    QCOMPARE(dev->childCount(), 2);

    // Shown in Art-Net's own net:subnet:universe terms, which is what is
    // printed on the node's panel, not the packed 15-bit number.
    QCOMPARE(dev->child(0)->text(COL_DETAIL), QString("0:1:1"));
    QCOMPARE(dev->child(0)->data(COL_NAME, ROLE_PORTADDR).toUInt(), quint32(0x011));
    QCOMPARE(dev->child(1)->text(COL_DETAIL), QString("0:1:2"));
}

/****************************************************************************
 * Labels
 ****************************************************************************/

void ConnectionsTree_Test::lineAliasIsShownOnTheRow()
{
    IOPluginStub *stub = stubOf(m_doc);
    stub->m_linesAreHardware = true;
    stub->m_lineDescriptions.insert(0, "en0");

    ConnectionsTree tree(m_doc);
    tree.refresh();

    QList<QTreeWidgetItem *> lines = itemsOfKind(tree.m_tree, KIND_LINE);
    QVERIFY(lines.isEmpty() == false);

    // With no alias, the plugin's own description leads.
    QVERIFY(lines.at(0)->text(COL_NAME).startsWith("en0 ("));
    // The identity stays visible: it is what the patch stores.
    QVERIFY(lines.at(0)->text(COL_NAME).contains("1: Stub 1"));

    m_doc->inputOutputMap()->setLineAlias(stub->name(), "1: Stub 1", "FOH rack");
    tree.refresh();

    lines = itemsOfKind(tree.m_tree, KIND_LINE);
    QVERIFY(lines.at(0)->text(COL_NAME).startsWith("FOH rack ("));
    QVERIFY(lines.at(0)->text(COL_NAME).contains("1: Stub 1"));
}

void ConnectionsTree_Test::targetAliasIsShownOnTheRow()
{
    IOPluginStub *stub = stubOf(m_doc);
    stub->m_devices << makeDevice(0, "CR041R", "172.18.2.10");
    m_doc->inputOutputMap()->setTargetAlias("172.18.2.10", "Stage left rack");

    ConnectionsTree tree(m_doc);
    tree.refresh();

    QTreeWidgetItem *dev = itemWithAddress(tree.m_tree, "172.18.2.10");
    QVERIFY(dev != NULL);
    // The operator's name beats the node's own, and the address stays put.
    QCOMPARE(dev->text(COL_NAME), QString("Stage left rack (172.18.2.10)"));
}

/****************************************************************************
 * Default expansion and its memory
 ****************************************************************************/

void ConnectionsTree_Test::defaultExpansionStopsBelowTheInterface()
{
    IOPluginStub *stub = stubOf(m_doc);
    QLCIOPlugin::Device d = makeDevice(0, "CR041R", "172.18.2.10");
    d.portLabels << "0:0:0" << "0:0:1";
    d.portUniverses << 0x000 << 0x001;
    stub->m_devices << d;

    ConnectionsTree tree(m_doc);   // the constructor's refresh applies defaults

    QList<QTreeWidgetItem *> plugins = itemsOfKind(tree.m_tree, KIND_PLUGIN);
    QList<QTreeWidgetItem *> lines = itemsOfKind(tree.m_tree, KIND_LINE);
    QVERIFY(plugins.isEmpty() == false);
    QVERIFY(lines.isEmpty() == false);

    // Protocol and interface open: that is the shape of the rig on one screen.
    QVERIFY(plugins.at(0)->isExpanded() == true);
    QVERIFY(lines.at(0)->isExpanded() == true);

    // The device and everything under it closed: a real show expands to
    // hundreds of port and universe rows, and arriving at a wall of leaves
    // means closing your way back out to find the question you came in with.
    QTreeWidgetItem *dev = itemWithAddress(tree.m_tree, "172.18.2.10");
    QVERIFY(dev != NULL);
    QVERIFY(dev->childCount() > 0);
    QVERIFY(dev->isExpanded() == false);
}

void ConnectionsTree_Test::operatorExpansionSurvivesRebuild()
{
    IOPluginStub *stub = stubOf(m_doc);
    QLCIOPlugin::Device d = makeDevice(0, "CR041R", "172.18.2.10");
    d.portLabels << "0:0:0" << "0:0:1";
    d.portUniverses << 0x000 << 0x001;
    stub->m_devices << d;

    ConnectionsTree tree(m_doc);

    QTreeWidgetItem *dev = itemWithAddress(tree.m_tree, "172.18.2.10");
    QVERIFY(dev != NULL);
    QVERIFY(dev->isExpanded() == false);

    dev->setExpanded(true);
    tree.refresh();     // the five-second rebuild must not fight the operator

    dev = itemWithAddress(tree.m_tree, "172.18.2.10");
    QVERIFY(dev != NULL);
    QVERIFY2(dev->isExpanded() == true,
             "a branch the operator opened reclosed itself on the next rebuild");

    // And closing it again sticks, which the old collapse-only memory could
    // not express once the default stopped being "everything open".
    dev->setExpanded(false);
    tree.refresh();
    dev = itemWithAddress(tree.m_tree, "172.18.2.10");
    QVERIFY(dev != NULL);
    QVERIFY(dev->isExpanded() == false);
}

/****************************************************************************
 * The "still searching" banner
 ****************************************************************************/

void ConnectionsTree_Test::bannerHidesOnceSomethingIsHeard()
{
    IOPluginStub *stub = stubOf(m_doc);

    {
        // Nothing heard and barely any time passed: the tree is genuinely
        // incomplete and saying "nothing is connected" would be wrong.
        ConnectionsTree empty(m_doc);
        empty.show();
        QVERIFY(empty.m_status->isVisibleTo(&empty) == true);
    }

    stub->m_devices << makeDevice(0, "CR041R", "172.18.2.10");

    ConnectionsTree found(m_doc);
    found.show();
    found.refresh();

    // Searching ends when something is found, not when a stopwatch says so.
    // Keeping the banner up for a fixed six seconds after devices are already
    // listed made a finished tree look unfinished.
    QVERIFY(found.m_status->isVisibleTo(&found) == false);
}

/****************************************************************************
 * Repointing an existing patch
 ****************************************************************************/

void ConnectionsTree_Test::retargetWritesTargetOntoTheExistingPatch()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, stub->name(), "", 0, false, 0) == true);

    ConnectionsTree tree(m_doc);

    // applyTarget is the single writer both the create and the edit path go
    // through, so a patch made by one and changed by the other cannot end up
    // describing itself two different ways.
    QVERIFY(tree.applyTarget(0, stub->name(), 0, "172.18.2.10", 0x011) == true);

    QString addr;
    quint32 port = 0;
    QVERIFY(tree.patchTarget(0, stub->name(), 0, addr, port) == true);
    QCOMPARE(addr, QString("172.18.2.10"));
    QCOMPARE(port, quint32(0x011));

    // Repointing overwrites in place rather than adding a second patch: a node
    // swapped for a spare must not cost the patch and everything under it.
    QVERIFY(tree.applyTarget(0, stub->name(), 0, "172.18.2.11", 0x022) == true);
    QCOMPARE(iom->outputPatchesCount(0), 1);
    QVERIFY(tree.patchTarget(0, stub->name(), 0, addr, port) == true);
    QCOMPARE(addr, QString("172.18.2.11"));
    QCOMPARE(port, quint32(0x022));

    // A line the universe is not patched to is not silently created.
    QVERIFY(tree.applyTarget(0, stub->name(), 3, "10.0.0.1", 0) == false);
}

void ConnectionsTree_Test::retargetToBroadcastRemovesTheParameters()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, stub->name(), "", 0, false, 0) == true);

    ConnectionsTree tree(m_doc);
    QVERIFY(tree.applyTarget(0, stub->name(), 0, "172.18.2.10", 0x011) == true);

    OutputPatch *op = iom->universe(0)->outputPatch(0);
    QVERIFY(op != NULL);
    QVERIFY(op->getPluginParameters().contains("outputIP"));

    // Broadcast is an ABSENT parameter, not an empty one: absent means the
    // plugin's own default, empty means a configured destination of nowhere.
    op->unSetPluginParameter("outputIP");
    op->unSetPluginParameter("outputUni");
    QVERIFY(op->getPluginParameters().contains("outputIP") == false);

    QString addr;
    quint32 port = 0;
    QVERIFY(tree.patchTarget(0, stub->name(), 0, addr, port) == false);
}

/****************************************************************************
 * Plugin parameters
 ****************************************************************************/

void ConnectionsTree_Test::patchParameterDistinguishesAbsentFromZero()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, stub->name(), "", 0, false, 0) == true);

    ConnectionsTree tree(m_doc);

    // Absent is not zero. "midichannel 0" means every channel and "mode
    // unset" means the plugin's default -- a reader that cannot tell the
    // difference shows a value the rig is not actually using, and a menu
    // built from it puts a checkmark on the wrong row.
    QVERIFY(tree.patchParameter(0, stub->name(), 0, true, "mode").isValid() == false);

    tree.setPatchParameter(0, stub->name(), 0, true, "mode", QString("Note Velocity"));
    const QVariant v = tree.patchParameter(0, stub->name(), 0, true, "mode");
    QVERIFY(v.isValid());
    QCOMPARE(v.toString(), QString("Note Velocity"));

    // A line this universe is not patched to has no parameters to report.
    QVERIFY(tree.patchParameter(0, stub->name(), 2, true, "mode").isValid() == false);
}

void ConnectionsTree_Test::setPatchParameterWritesOutputAndInputPatches()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, stub->name(), "", 0, false, 0) == true);
    QVERIFY(iom->setInputPatch(0, stub->name(), "", 1) == true);

    ConnectionsTree tree(m_doc);

    // Output and input patches are separate objects under one universe, and a
    // parameter written to the wrong one is silently lost.
    tree.setPatchParameter(0, stub->name(), 1, false, "midichannel", 5);
    QCOMPARE(tree.patchParameter(0, stub->name(), 1, false, "midichannel").toInt(), 5);
    QVERIFY(tree.patchParameter(0, stub->name(), 0, true, "midichannel").isValid() == false);

    tree.setPatchParameter(0, stub->name(), 0, true, "transmitMode", QString("Full"));
    QCOMPARE(tree.patchParameter(0, stub->name(), 0, true, "transmitMode").toString(),
             QString("Full"));
    QVERIFY(tree.patchParameter(0, stub->name(), 1, false, "transmitMode").isValid() == false);
}

/****************************************************************************
 * Bulk retarget
 ****************************************************************************/

void ConnectionsTree_Test::selectedUniversesRejectsAMixedPluginSelection()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, stub->name(), "", 0, false, 0) == true);
    QVERIFY(iom->setOutputPatch(1, stub->name(), "", 1, false, 0) == true);

    ConnectionsTree tree(m_doc);
    tree.refresh();

    QList<QTreeWidgetItem *> unis;
    foreach (QTreeWidgetItem *it, allItems(tree.m_tree))
    {
        if (it->data(COL_NAME, ROLE_UNIVERSE).isValid()
                && it->data(COL_NAME, ROLE_OUTPUT).toBool())
            unis << it;
    }
    QVERIFY(unis.count() >= 2);

    foreach (QTreeWidgetItem *it, unis)
        it->setSelected(true);

    QString plug;
    QList<quint32> lines;
    QCOMPARE(tree.selectedUniverses(plug, lines).count(), unis.count());
    QCOMPARE(plug, stub->name());

    // Two protocols in one selection have no single meaning for "aim these at
    // a node" -- refusing beats guessing which half was meant.
    unis.at(0)->setData(COL_NAME, ROLE_PLUGIN, QString("Some Other Protocol"));
    QVERIFY(tree.selectedUniverses(plug, lines).isEmpty());
    QVERIFY(plug.isEmpty());
}

/* The five-second rebuild dropped the selection and scrolled back to the top:
   the universe you had just clicked was gone and the list jumped. */
void ConnectionsTree_Test::aRefreshKeepsTheSelectionAndThePlace()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (int i = 0; i < 40; ++i)
        QVERIFY(iom->addUniverse());
    ConnectionsTree tree(m_doc);
    tree.resize(900, 300);                       // short: the list scrolls
    tree.show();
    tree.refresh();
    tree.m_tree->expandAll();
    QApplication::processEvents();

    QTreeWidgetItem *row = nullptr;
    foreach (QTreeWidgetItem *it, allItems(tree.m_tree))
        if (it->data(COL_NAME, ROLE_UNIVERSE).isValid() && it->data(COL_NAME, ROLE_UNIVERSE).toUInt() == 30)
            row = it;
    QVERIFY(row != nullptr);
    tree.m_tree->setCurrentItem(row);
    QVERIFY(tree.m_tree->verticalScrollBar()->maximum() > 0);
    const int scroll = tree.m_tree->verticalScrollBar()->value();
    QVERIFY(scroll > 0);                         // scrolled down to it

    tree.refresh();
    QApplication::processEvents();
    QTreeWidgetItem *cur = tree.m_tree->currentItem();
    QVERIFY(cur != nullptr);
    QCOMPARE(cur->data(COL_NAME, ROLE_UNIVERSE).toUInt(), quint32(30));
    QVERIFY(cur->isSelected());
    QCOMPARE(tree.m_tree->selectedItems().size(), 1);
    QCOMPARE(tree.m_tree->verticalScrollBar()->value(), scroll);
    QCOMPARE(tree.m_output->universe(), quint32(30));   // still showing it
}

/* A right-click menu holds the row it opened on; a rebuild under it tore that
   row down, so Patch... acted on nothing. */
void ConnectionsTree_Test::noRefreshUnderAnOpenMenu()
{
    ConnectionsTree tree(m_doc);
    tree.resize(900, 500);
    tree.show();
    tree.refresh();
    QVERIFY(tree.m_tree->topLevelItemCount() > 0);
    QTreeWidgetItem *first = tree.m_tree->topLevelItem(0);

    QMenu menu;
    menu.addAction("Patch...");
    menu.popup(tree.mapToGlobal(QPoint(10, 10)));
    QTRY_VERIFY(QApplication::activePopupWidget() != nullptr);
    tree.refresh();
    QCOMPARE(tree.m_tree->topLevelItem(0), first);      // not rebuilt
    menu.close();
    QTRY_VERIFY(QApplication::activePopupWidget() == nullptr);
    tree.refresh();
    QVERIFY(tree.m_tree->topLevelItemCount() > 0);
}

void ConnectionsTree_Test::onlyTheNameIsEditedAndASelectedUniverseShowsWhatItSends()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(1, stub->name(), "", 1, false, 0) == true);
    Fixture *fx = new Fixture(m_doc);
    fx->setName("Front Par");
    fx->setUniverse(1);
    fx->setAddress(10);
    fx->setChannels(3);
    QVERIFY(m_doc->addFixture(fx));

    ConnectionsTree tree(m_doc);
    tree.resize(1200, 700);
    tree.show();
    tree.refresh();
    QTreeWidgetItem *row = nullptr;
    foreach (QTreeWidgetItem *it, allItems(tree.m_tree))
        if (it->data(COL_NAME, ROLE_UNIVERSE).isValid() && it->data(COL_NAME, ROLE_UNIVERSE).toUInt() == 1
            && it->data(COL_NAME, ROLE_OUTPUT).toBool())
            row = it;
    QVERIFY(row != nullptr);

    // "clicking to edit the text should only work on the device .. not on
    // detail or carries"
    auto editors = [&tree]() {
        QApplication::processEvents();
        return tree.m_tree->viewport()->findChildren<QLineEdit *>().size();
    };
    tree.m_tree->editItem(row, COL_DETAIL);
    QCOMPARE(editors(), 0);
    tree.m_tree->editItem(row, COL_CARRIES);
    QCOMPARE(editors(), 0);
    if (row->flags() & Qt::ItemIsEditable)
    {
        tree.m_tree->editItem(row, COL_NAME);
        QCOMPARE(editors(), 1);
        tree.m_tree->closePersistentEditor(row, COL_NAME);
        QApplication::processEvents();
    }

    // Selected, its output is shown on the right: what it sends, as it sends it.
    QCOMPARE(tree.m_output->universe(), Universe::invalid());
    tree.m_tree->setCurrentItem(row);
    QCOMPARE(tree.m_output->universe(), quint32(1));
    QByteArray out(16, char(0));
    out[10] = char(200);
    out[12] = char(17);
    emit iom->universeWritten(1, out);
    emit iom->universeWritten(2, QByteArray(16, char(99)));     // another universe: not this one
    QCOMPARE(int(uchar(tree.m_output->values().at(10))), 200);
    QCOMPARE(int(uchar(tree.m_output->values().at(12))), 17);
    QCOMPARE(int(uchar(tree.m_output->values().at(0))), 0);
    QCOMPARE(tree.m_output->values().size(), 512);
    // Drawn without trouble, a fixture's span named.
    QVERIFY(!tree.m_output->grab().isNull());
}

void ConnectionsTree_Test::selectedUniversesDeduplicatesFannedOutUniverses()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    // One universe, two output legs -- so it occupies two rows.
    QVERIFY(iom->setOutputPatch(0, stub->name(), "", 0, false, 0) == true);
    QVERIFY(iom->setOutputPatch(0, stub->name(), "", 1, false, 1) == true);

    ConnectionsTree tree(m_doc);
    tree.refresh();

    int rows = 0;
    foreach (QTreeWidgetItem *it, allItems(tree.m_tree))
    {
        if (it->data(COL_NAME, ROLE_UNIVERSE).isValid()
                && it->data(COL_NAME, ROLE_UNIVERSE).toUInt() == 0
                && it->data(COL_NAME, ROLE_OUTPUT).toBool())
        {
            it->setSelected(true);
            rows++;
        }
    }
    QVERIFY2(rows >= 2, "a fanned-out universe should occupy several rows");

    // Selecting both legs is still one universe. Counting it twice would make
    // the auto-increment skip a port for every extra leg.
    QString plug;
    QList<quint32> lines;
    QCOMPARE(tree.selectedUniverses(plug, lines).count(), 1);
    QCOMPARE(lines.count(), 1);
}

/* Two universes pointed at the same node AND port both write it every tick, so
   the fixture shows whichever arrived last: a patch that reads as correct on
   every screen and is wrong on the floor. Nothing stopped it.

   The rule is keyed on the destination, not the plugin line -- for Art-Net a
   line is a network interface and sharing one is normal, which is what the
   first half of this test pins down. */
void ConnectionsTree_Test::twoUniversesCannotTargetTheSameNodePort()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 u = 0; u < 3; u++)
        QVERIFY(iom->setOutputPatch(u, stub->name(), "", 0, false, 0) == true);

    ConnectionsTree tree(m_doc);

    // Universe 0 takes 172.18.2.10 port 0x011.
    QVERIFY(tree.applyTarget(0, stub->name(), 0, "172.18.2.10", 0x011) == true);

    // Same NIC, DIFFERENT port: ordinary Art-Net, and it must keep working.
    QVERIFY(tree.applyTarget(1, stub->name(), 0, "172.18.2.10", 0x012) == true);

    // Same NIC, same node, SAME port: refused.
    QVERIFY(tree.applyTarget(2, stub->name(), 0, "172.18.2.10", 0x011) == false);

    // ...and the refusal left universe 2 alone rather than half-applying it.
    QString a;
    quint32 p = 0;
    if (tree.patchTarget(2, stub->name(), 0, a, p) == true)
        QVERIFY2(!(a == QString("172.18.2.10") && p == 0x011),
                 "the refused target was written anyway");

    // A different NODE on the same port address is a different destination.
    QVERIFY(tree.applyTarget(2, stub->name(), 0, "172.18.2.99", 0x011) == true);

    // Re-applying a universe's OWN target is not a self-collision.
    QVERIFY(tree.applyTarget(0, stub->name(), 0, "172.18.2.10", 0x011) == true);

    // And the owner is reported for the message the UI shows.
    QCOMPARE(tree.universeOnTarget(stub->name(), 0, "172.18.2.10", 0x011, 2), quint32(0));
}

void ConnectionsTree_Test::bulkRetargetNumbersPortsUpwardAndIsOneUndoStep()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 u = 0; u < 3; u++)
        QVERIFY(iom->setOutputPatch(u, stub->name(), "", 0, false, 0) == true);

    ConnectionsTree tree(m_doc);

    // Drive applyTarget the way retargetSelection does, so the numbering rule
    // is under test without a modal dialog in the way.
    const QList<quint32> unis = QList<quint32>() << 0 << 1 << 2;
    iom->patchUndo()->capture(unis, "retarget 3 universes");

    const quint32 first = 0x011;    // 0:1:1
    for (int i = 0; i < unis.count(); i++)
        QVERIFY(tree.applyTarget(unis.at(i), stub->name(), 0, "172.18.2.10",
                                 first + quint32(i)) == true);

    for (int i = 0; i < unis.count(); i++)
    {
        QString a;
        quint32 p = 0;
        QVERIFY(tree.patchTarget(unis.at(i), stub->name(), 0, a, p) == true);
        QCOMPARE(a, QString("172.18.2.10"));
        QCOMPARE(p, first + quint32(i));
    }

    // One step for the whole set: a wrong node address costs one press, not
    // three corrections. That is the condition the feature is worth having on.
    QVERIFY(iom->patchUndo()->canUndo() == true);
    QVERIFY(iom->patchUndo()->undo() == true);
    for (int i = 0; i < unis.count(); i++)
    {
        QString a;
        quint32 p = 0;
        QVERIFY2(tree.patchTarget(unis.at(i), stub->name(), 0, a, p) == false,
                 "undo left a target behind on one of the universes");
    }
    QVERIFY(iom->patchUndo()->canUndo() == false);
}

/****************************************************************************
 * Multi-universe patch / unpatch
 ****************************************************************************/

/** The By-universe list, every universe showing, on screen so it can be
    clicked. */
static void showUniverseList(ConnectionsTree &tree)
{
    tree.resize(1000, 600);
    tree.m_showUnused->setChecked(true);
    tree.m_byUniverse->setChecked(true);
    tree.show();
    tree.refresh();
    QApplication::processEvents();
}

static QTreeWidgetItem *universeRow(QTreeWidget *tree, quint32 id)
{
    foreach (QTreeWidgetItem *it, allItems(tree))
        if (it->data(COL_NAME, ROLE_UNIVERSE).isValid()
                && it->data(COL_NAME, ROLE_UNIVERSE).toUInt() == id)
            return it;
    return NULL;
}

/** Click a universe's row the way a hand does, with @p mod held. */
static void clickUniverse(ConnectionsTree &tree, quint32 id,
                          Qt::KeyboardModifiers mod = Qt::NoModifier)
{
    QTreeWidgetItem *row = universeRow(tree.m_tree, id);
    QVERIFY(row != NULL);
    const QPoint at = tree.m_tree->visualItemRect(row).center();
    QTest::mouseClick(tree.m_tree->viewport(), Qt::LeftButton, mod, at);
    QApplication::processEvents();
}

/** Pick @p text (by prefix) from the context menu opened on @p id's row. */
static QStringList contextMenuOn(ConnectionsTree &tree, quint32 id, const QString &pick)
{
    QStringList seen;
    QTimer::singleShot(0, [&seen, pick]() {
        QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (m == NULL)
            return;
        QAction *chosen = NULL;
        foreach (QAction *a, m->actions())
        {
            seen << a->text();
            if (chosen == NULL && pick.isEmpty() == false && a->text().startsWith(pick))
                chosen = a;
        }
        if (chosen != NULL)
        {
            m->setActiveAction(chosen);
            QTest::keyClick(m, Qt::Key_Return);
        }
        else
            m->close();
    });
    QTreeWidgetItem *row = universeRow(tree.m_tree, id);
    tree.slotContextMenu(tree.m_tree->visualItemRect(row).center());
    QApplication::processEvents();
    return seen;
}

static IOPluginStub *fourPortNode(Doc *doc)
{
    IOPluginStub *stub = stubOf(doc);
    stub->m_supportsTargets = true;
    QLCIOPlugin::Device node = makeDevice(0, "Quad", "172.18.2.50");
    for (quint32 p = 0; p < 4; p++)
    {
        node.portLabels << QString("0:0:%1").arg(p);
        node.portUniverses << p;
    }
    stub->m_devices << node;
    InputOutputMap *iom = doc->inputOutputMap();
    while (iom->universesCount() < 8)
        iom->addUniverse();
    return stub;
}

void ConnectionsTree_Test::selectionRemembersTheOrderUniversesWerePicked()
{
    fourPortNode(m_doc);
    ConnectionsTree tree(m_doc);
    showUniverseList(tree);

    // Listed 1..8; picked 3, then 1, 4, 2. A selection model can only ever
    // say "1, 2, 3, 4" -- the order is what has to be remembered.
    clickUniverse(tree, 2);
    clickUniverse(tree, 0, Qt::ControlModifier);
    clickUniverse(tree, 3, Qt::ControlModifier);
    clickUniverse(tree, 1, Qt::ControlModifier);
    QCOMPARE(tree.orderedSelectedUniverses(), QList<quint32>() << 2 << 0 << 3 << 1);

    // The five-second rebuild clears and restores the selection: not a
    // reason to forget the order.
    tree.refresh();
    QApplication::processEvents();
    QCOMPARE(tree.orderedSelectedUniverses(), QList<quint32>() << 2 << 0 << 3 << 1);

    // Ctrl-clicking one off drops just that one.
    clickUniverse(tree, 0, Qt::ControlModifier);
    QCOMPARE(tree.orderedSelectedUniverses(), QList<quint32>() << 2 << 3 << 1);

    // A plain click starts again; a shift-click range adds in list order.
    clickUniverse(tree, 6);
    clickUniverse(tree, 4, Qt::ShiftModifier);
    QCOMPARE(tree.orderedSelectedUniverses(), QList<quint32>() << 6 << 4 << 5);
}

void ConnectionsTree_Test::multiPatchPutsThemOnTheDevicePortsInPickedOrder()
{
    IOPluginStub *stub = fourPortNode(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    // An unselected universe, patched elsewhere: must not move.
    QVERIFY(iom->setOutputPatch(7, stub->name(), "", 1, false, 0));

    ConnectionsTree tree(m_doc);
    showUniverseList(tree);

    // Five picked for a four-port node, out of list order: 3, 1, 4, 2, 6.
    clickUniverse(tree, 2);
    clickUniverse(tree, 0, Qt::ControlModifier);
    clickUniverse(tree, 3, Qt::ControlModifier);
    clickUniverse(tree, 1, Qt::ControlModifier);
    clickUniverse(tree, 5, Qt::ControlModifier);
    const QList<quint32> picked = tree.orderedSelectedUniverses();
    QCOMPARE(picked, QList<quint32>() << 2 << 0 << 3 << 1 << 5);

    // The menu now offers it for the whole selection, by count.
    const QStringList offered = contextMenuOn(tree, 0, QString());
    QVERIFY2(offered.contains("Patch 5 selected universes to…"), qPrintable(offered.join(" | ")));

    const QList<quint32> ports = QList<quint32>() << 0 << 1 << 2 << 3;
    const ConnectionsTree::OrderedPatch plan = tree.planOrderedPatch(
        picked, stub->name(), 0, "172.18.2.50", ports, QStringList(), false);
    QCOMPARE(plan.entries.count(), 4);
    QCOMPARE(plan.overflow, QList<quint32>() << 5);   // said, not wrapped or dropped
    QVERIFY(plan.conflicts.isEmpty());

    int rebuilds = 0;
    connect(tree.m_tree->model(), &QAbstractItemModel::modelReset, [&rebuilds]() { rebuilds++; });
    QCOMPARE(tree.applyOrderedPatch(plan), 4);
    QCOMPARE(rebuilds, 1);                             // one rebuild, not one per universe

    // Port k carries the k-th one PICKED.
    for (int k = 0; k < 4; k++)
    {
        QString a;
        quint32 p = 99;
        QVERIFY(tree.patchTarget(picked.at(k), stub->name(), 0, a, p));
        QCOMPARE(a, QString("172.18.2.50"));
        QCOMPARE(p, quint32(k));
    }
    // The one that did not fit, and the one not selected, are as they were.
    QCOMPARE(iom->outputPatchesCount(5), 0);
    QCOMPARE(iom->outputPatchesCount(7), 1);
    QCOMPARE(iom->outputPatch(7)->output(), quint32(1));

    // Re-ordering the same four across the same four ports is not a clash
    // with themselves.
    const QList<quint32> reversed = QList<quint32>() << 1 << 3 << 0 << 2;
    const ConnectionsTree::OrderedPatch again = tree.planOrderedPatch(
        reversed, stub->name(), 0, "172.18.2.50", ports, QStringList(), false);
    QVERIFY(again.conflicts.isEmpty());
    QCOMPARE(tree.applyOrderedPatch(again), 4);
    for (int k = 0; k < 4; k++)
    {
        QString a;
        quint32 p = 99;
        QVERIFY(tree.patchTarget(reversed.at(k), stub->name(), 0, a, p));
        QCOMPARE(p, quint32(k));
    }

    // One undo step for the whole set.
    QVERIFY(iom->patchUndo()->undo());
    for (int k = 0; k < 4; k++)
    {
        QString a;
        quint32 p = 99;
        QVERIFY(tree.patchTarget(picked.at(k), stub->name(), 0, a, p));
        QCOMPARE(p, quint32(k));
    }
}

void ConnectionsTree_Test::multiPatchSkipsAPortAnotherUniverseHolds()
{
    IOPluginStub *stub = fourPortNode(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    ConnectionsTree tree(m_doc);
    // Universe 8 already sends to the node's port 3.
    QVERIFY(iom->setOutputPatch(7, stub->name(), "", 0, false, 0));
    QVERIFY(tree.applyTarget(7, stub->name(), 0, "172.18.2.50", 2));

    const QList<quint32> picked = QList<quint32>() << 3 << 2 << 1 << 0;
    const ConnectionsTree::OrderedPatch plan = tree.planOrderedPatch(
        picked, stub->name(), 0, "172.18.2.50",
        QList<quint32>() << 0 << 1 << 2 << 3, QStringList(), false);

    // The third one picked would land on universe 8's port: skipped, and who
    // holds it is said. The rest keep their positions -- port k is still
    // the k-th pick, nothing shuffles up into the gap.
    QCOMPARE(plan.conflicts.count(), 1);
    QCOMPARE(plan.conflicts.first().universe, quint32(1));
    QCOMPARE(plan.conflicts.first().owner, quint32(7));
    QCOMPARE(tree.applyOrderedPatch(plan), 3);
    QCOMPARE(iom->outputPatchesCount(1), 0);
    QString a;
    quint32 p = 99;
    QVERIFY(tree.patchTarget(0, stub->name(), 0, a, p));
    QCOMPARE(p, quint32(3));
    QVERIFY(tree.patchTarget(7, stub->name(), 0, a, p));
    QCOMPARE(p, quint32(2));                           // untouched
}

void ConnectionsTree_Test::multiUnpatchRemovesEveryOneInOneStep()
{
    IOPluginStub *stub = fourPortNode(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 u = 0; u < 5; u++)
        QVERIFY(iom->setOutputPatch(u, stub->name(), "", u % 4, false, 0));

    ConnectionsTree tree(m_doc);
    showUniverseList(tree);
    clickUniverse(tree, 3);
    clickUniverse(tree, 0, Qt::ControlModifier);
    clickUniverse(tree, 2, Qt::ControlModifier);
    clickUniverse(tree, 1, Qt::ControlModifier);

    int rebuilds = 0;
    connect(tree.m_tree->model(), &QAbstractItemModel::modelReset, [&rebuilds]() { rebuilds++; });

    // From the menu, as Branson would: all four go at once.
    const QStringList offered = contextMenuOn(tree, 1, "Unpatch 4 selected universes");
    QVERIFY2(offered.contains("Unpatch 4 selected universes"), qPrintable(offered.join(" | ")));
    // ...and the single-row action is still there for the row under the pointer.
    QVERIFY(offered.contains("Unpatch from this interface"));

    for (quint32 u = 0; u < 4; u++)
        QCOMPARE(iom->outputPatchesCount(u), 0);
    QCOMPARE(iom->outputPatchesCount(4), 1);           // not selected: kept
    QCOMPARE(rebuilds, 1);

    // One press of Undo Patch Change puts all four back.
    QVERIFY(iom->patchUndo()->canUndo());
    QVERIFY(iom->patchUndo()->undo());
    for (quint32 u = 0; u < 4; u++)
    {
        QCOMPARE(iom->outputPatchesCount(u), 1);
        QCOMPARE(iom->outputPatch(u)->output(), u % 4);
    }
}

/****************************************************************************
 * Reachability probing
 ****************************************************************************/

void ConnectionsTree_Test::rescanProbesUnheardTargetsOnly()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();

    // One target we have heard from, one we have only been told about.
    stub->m_devices << makeDevice(0, "CR041R", "172.18.2.10");
    iom->addManualTarget(stub->name(), 0, "172.18.2.99");

    ConnectionsTree tree(m_doc);
    stub->m_probed.clear();
    tree.slotRescan();

    // The silent one is asked directly -- otherwise its amber tint means "no
    // question asked" rather than "no answer".
    QVERIFY2(stub->m_probed.contains("172.18.2.99"),
             "a hand-declared target that has never answered was not probed");

    // The one already answering broadcasts is not: it needs no unicast, and
    // probing every known node would multiply the traffic this keeps low.
    QVERIFY2(stub->m_probed.contains("172.18.2.10") == false,
             "a node already heard from should not be probed as well");
}

void ConnectionsTree_Test::rescanProbesAddressesNamedByPatches()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();

    // A patch aimed at a node nothing has answered from -- what a workspace
    // built on the rig looks like when it is opened anywhere else.
    QVERIFY(iom->setOutputPatch(0, stub->name(), "", 0, false, 0) == true);
    iom->outputPatch(0, 0)->setPluginParameter("outputIP", "10.9.9.9");

    ConnectionsTree tree(m_doc);
    stub->m_probed.clear();
    tree.slotRescan();

    QVERIFY2(stub->m_probed.contains("10.9.9.9"),
             "an address a patch points at was not probed");
}

/****************************************************************************
 * Live input activity
 ****************************************************************************/

void ConnectionsTree_Test::inputActivityTintsTheUniverseRow()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), "", 0) == true);

    ConnectionsTree tree(m_doc);
    tree.show();
    tree.refresh();

    QList<QTreeWidgetItem *> unis = itemsOfKind(tree.m_tree, KIND_UNIVERSE);
    if (unis.isEmpty())
    {
        // Folded rows are a port AND a universe, so look for the universe role
        // rather than the kind.
        foreach (QTreeWidgetItem *it, allItems(tree.m_tree))
        {
            if (it->data(COL_NAME, ROLE_UNIVERSE).isValid())
                unis << it;
        }
    }
    QVERIFY2(unis.isEmpty() == false, "no universe row to light up");

    tree.slotInputActivity(0, 9, 127);

    bool tinted = false;
    foreach (QTreeWidgetItem *it, allItems(tree.m_tree))
    {
        if (it->data(COL_NAME, ROLE_UNIVERSE).isValid()
                && it->data(COL_NAME, ROLE_UNIVERSE).toUInt() == 0
                && it->toolTip(COL_DETAIL).contains("channel 9 = 127"))
            tinted = true;
    }
    QVERIFY2(tinted, "the universe that received input was not marked");

    // Another universe's traffic must not light this one.
    tree.slotActivityTimeout();
    tree.slotInputActivity(3, 1, 1);
    foreach (QTreeWidgetItem *it, allItems(tree.m_tree))
    {
        if (it->data(COL_NAME, ROLE_UNIVERSE).isValid()
                && it->data(COL_NAME, ROLE_UNIVERSE).toUInt() == 0)
            QVERIFY(it->toolTip(COL_DETAIL).isEmpty());
    }

    // The tint decays rather than latching on forever.
    tree.slotActivityTimeout();
    QVERIFY(tree.m_activeUniverses.isEmpty());
}

/****************************************************************************
 * Capabilities recovered from the Detailed tab
 ****************************************************************************/

void ConnectionsTree_Test::passthroughIsReadableAndSettableHere()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->universesCount() > 0);

    QVERIFY(iom->getUniversePassthrough(0) == false);

    /* The tree's menu action reads and writes exactly this pair, and this was
       the only setting in the application with a single point of access -- one
       reorganisation of the Detailed tab and it became unreachable. */
    iom->setUniversePassthrough(0, true);
    QVERIFY(iom->getUniversePassthrough(0) == true);

    iom->setUniversePassthrough(0, false);
    QVERIFY(iom->getUniversePassthrough(0) == false);
}

void ConnectionsTree_Test::protocolRowCarriesThePluginDescription()
{
    IOPluginStub *stub = stubOf(m_doc);
    stub->m_linesAreHardware = true;   // so the protocol row survives filtering

    ConnectionsTree tree(m_doc);
    tree.refresh();

    QList<QTreeWidgetItem *> plugins = itemsOfKind(tree.m_tree, KIND_PLUGIN);
    QVERIFY(plugins.isEmpty() == false);

    /* Same source the Detailed pane renders, so the two cannot disagree about
       what a protocol is or whether it is working. */
    const QString expected =
        m_doc->inputOutputMap()->pluginDescription(stub->name());
    if (expected.isEmpty())
        QSKIP("the stub plugin reports no description to show");

    QVERIFY2(plugins.at(0)->toolTip(COL_NAME).isEmpty() == false,
             "the protocol row carries no description tooltip");
    QCOMPARE(plugins.at(0)->toolTip(COL_NAME), expected);
}

/****************************************************************************
 * Tooltips
 ****************************************************************************/

void ConnectionsTree_Test::propertyTooltipRendersPairs()
{
    QList<QPair<QString, QString> > props;
    QVERIFY(ConnectionsTree::propertyTooltip("CR041R", props).isEmpty());

    props << QPair<QString, QString>("Firmware", "14");
    props << QPair<QString, QString>("Port 1", "0:0:1 · SHORT DETECTED");

    const QString html = ConnectionsTree::propertyTooltip("CR041R", props);
    QVERIFY(html.contains("CR041R"));
    QVERIFY(html.contains("Firmware"));
    QVERIFY(html.contains("SHORT DETECTED"));
    QVERIFY(html.contains("<table"));

    // Values are escaped, so a node whose name contains markup cannot inject
    // it into the tooltip.
    QList<QPair<QString, QString> > evil;
    evil << QPair<QString, QString>("Name", "<b>not bold</b>");
    const QString safe = ConnectionsTree::propertyTooltip("x", evil);
    QVERIFY(safe.contains("&lt;b&gt;not bold&lt;/b&gt;"));
}

void ConnectionsTree_Test::deviceRowCarriesItsPropertiesAsATooltip()
{
    IOPluginStub *stub = stubOf(m_doc);
    QLCIOPlugin::Device d = makeDevice(0, "CR041R", "172.18.2.10");
    d.properties << QPair<QString, QString>("Firmware", "14");
    d.properties << QPair<QString, QString>("Port 1", "0:0:1 · SHORT DETECTED");
    stub->m_devices << d;

    ConnectionsTree tree(m_doc);
    tree.refresh();

    QTreeWidgetItem *dev = itemWithAddress(tree.m_tree, "172.18.2.10");
    QVERIFY(dev != NULL);

    // On every column: the pointer lands wherever the row is widest, and a
    // tooltip that only exists over one cell reads as no tooltip at all.
    for (int c = COL_NAME; c <= COL_CARRIES; c++)
    {
        QVERIFY(dev->toolTip(c).contains("Firmware"));
        QVERIFY(dev->toolTip(c).contains("SHORT DETECTED"));
    }
}

/* "wasting a lot of space in the connections view with the top panel": the
   explanation is one line beside the controls (the rest on hover), and the
   list and output take the height. */
void ConnectionsTree_Test::theListTakesTheHeightNotTheHeader()
{
    ConnectionsTree tree(m_doc);
    tree.resize(1200, 900);
    tree.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tree));
    QLabel *hint = tree.findChild<QLabel *>("connectionsHint");
    QVERIFY(hint != nullptr);
    QVERIFY(!hint->wordWrap());
    QVERIFY2(hint->toolTip().contains("following the signal outward"), "the full explanation on hover");
    QVERIFY2(hint->height() < 40, qPrintable(QString("hint is %1 px tall").arg(hint->height())));
    QVERIFY2(tree.m_tree->height() > 900 - 120,
             qPrintable(QString("the list is %1 px of 900").arg(tree.m_tree->height())));
}

/* "for universes can we have a way to refactor the layout to remove
   universes that aren't used .. or .. an 'active' only view?" */
void ConnectionsTree_Test::byUniverseShowsOnlyTheUniversesInUse()
{
    InputOutputMap *io = m_doc->inputOutputMap();
    while (io->universesCount() < 4)
        io->addUniverse();
    Fixture *fx = new Fixture(m_doc);
    fx->setChannels(3);
    fx->setUniverse(1);                              // universe 2 carries a fixture
    fx->setAddress(0);
    QVERIFY(m_doc->addFixture(fx));

    ConnectionsTree tree(m_doc);
    tree.m_byUniverse->setChecked(true);
    QCOMPARE(tree.m_showUnused->text(), QString("Show unused universes"));
    tree.m_showUnused->setChecked(false);
    tree.refresh();
    QStringList shown;
    QString note;
    for (int i = 0; i < tree.m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *it = tree.m_tree->topLevelItem(i);
        if (it->data(0, Qt::UserRole + 1).toUInt() == 0 && it->text(0).contains("hidden"))
            note = it->text(0);
        else
            shown << it->text(0);
    }
    QVERIFY2(shown.join(",").contains("2:"), qPrintable(shown.join(",")));
    for (const QString &s : shown)
        QVERIFY2(!s.startsWith("3:") && !s.startsWith("4:"), qPrintable(shown.join(",")));
    QVERIFY2(note.contains("hidden"), qPrintable(note));

    tree.m_showUnused->setChecked(true);             // all of them again
    tree.refresh();
    QStringList all;
    for (int i = 0; i < tree.m_tree->topLevelItemCount(); ++i)
        all << tree.m_tree->topLevelItem(i)->text(0);
    QVERIFY2(all.join(",").contains("3:") && all.join(",").contains("4:"), qPrintable(all.join(",")));
    QVERIFY(!all.join(",").contains("hidden"));
    tree.m_byUniverse->setChecked(false);
}

QTEST_MAIN(ConnectionsTree_Test)

void ConnectionsTree_Test::byUniverseListsEveryUniverseWithWhereItGoes()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, stub->name(), "", 0, false, 0) == true);
    iom->setUniverseName(0, "U01-CS1");
    iom->setTargetAlias("172.18.2.211", "CS1");

    ConnectionsTree tree(m_doc);
    QVERIFY(tree.applyTarget(0, stub->name(), 0, "172.18.2.211", 0) == true);
    tree.m_showUnused->setChecked(true);         // every one, used or not
    tree.m_byUniverse->setChecked(true);

    // Every universe, in number order -- patched or not.
    QCOMPARE(tree.m_tree->topLevelItemCount(), int(iom->universesCount()));
    QTreeWidgetItem *first = tree.m_tree->topLevelItem(0);
    QCOMPARE(first->text(COL_NAME), QString("1: U01-CS1"));
    QVERIFY2(first->text(COL_DETAIL).contains("CS1 (172.18.2.211)"), qPrintable(first->text(COL_DETAIL)));
    QCOMPARE(first->data(COL_NAME, ROLE_KIND).toInt(), KIND_UNIVERSE);   // the wiring view's actions
    QCOMPARE(first->data(COL_NAME, ROLE_PLUGIN).toString(), stub->name());
    QTreeWidgetItem *second = tree.m_tree->topLevelItem(1);
    QCOMPARE(second->text(COL_DETAIL), QString("not patched"));
    QCOMPARE(second->data(COL_NAME, ROLE_KIND).toInt(), 6);              // KIND_UNPATCHED_UNIVERSE

    // Renamed inline, as in the wiring view.
    second->setText(COL_NAME, "2: Spare");
    QCOMPARE(iom->getUniverseNameByID(1), QString("Spare"));

    // Back to the wiring: a tree again.
    tree.m_byUniverse->setChecked(false);
    QVERIFY(tree.m_tree->rootIsDecorated());
    QVERIFY(!itemsOfKind(tree.m_tree, KIND_PLUGIN).isEmpty());
}

/****************************************************************************
 * Output problems
 ****************************************************************************/

static QList<QTreeWidgetItem *> rowsWithText(QTreeWidget *tree, const QString &text)
{
    QList<QTreeWidgetItem *> out;
    foreach (QTreeWidgetItem *it, allItems(tree))
        if (it->text(COL_DETAIL).contains(text))
            out << it;
    return out;
}

void ConnectionsTree_Test::outputProblemsShowOnTheRowAndClear()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, stub->name(), "", 0, false, 0) == true);

    const QString problem("Art-Net to 172.18.2.50: not reachable since 20:14");
    stub->m_outputProblems[0] = QStringList() << problem;

    ConnectionsTree tree(m_doc);
    tree.refresh();

    QList<QTreeWidgetItem *> rows = rowsWithText(tree.m_tree, problem);
    QCOMPARE(rows.count(), 1);
    QVERIFY(rows.first()->data(COL_NAME, ROLE_UNIVERSE).toUInt() == 0);
    QCOMPARE(rows.first()->foreground(COL_DETAIL).color(), QColor(220, 90, 90));
    QVERIFY(rows.first()->toolTip(COL_NAME).contains("not reachable since 20:14"));

    // The universe list says it too
    tree.m_byUniverse->setChecked(true);
    rows = rowsWithText(tree.m_tree, problem);
    QCOMPARE(rows.count(), 1);
    QCOMPARE(rows.first()->text(COL_NAME).left(2), QString("1:"));

    // Fixed: the plugin says so, the tree (on screen) catches up by itself
    tree.m_byUniverse->setChecked(false);
    tree.show();
    stub->m_outputProblems.clear();
    stub->emitTargetProblemsChanged();
    QTRY_VERIFY(rowsWithText(tree.m_tree, problem).isEmpty());
    foreach (QTreeWidgetItem *it, allItems(tree.m_tree))
        QVERIFY(it->text(COL_DETAIL).contains("not reachable") == false);
}

void ConnectionsTree_Test::outputProblemsReachTheFooterOncePerProblem()
{
    IOPluginStub *stub = stubOf(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    while (iom->universesCount() < 4)
        QVERIFY(iom->addUniverse());
    for (quint32 u = 0; u < 4; u++)
        QVERIFY(iom->setOutputPatch(u, stub->name(), "", 0, false, 0) == true);

    // One dead node behind universes 2 and 3; universe 4 has its own trouble
    const QString dead("Art-Net node CR041R (172.18.2.10): stopped answering since 20:14");
    const QString shorted("Art-Net node CS1 (172.18.2.211), port 1 (0:0:3): DMX output short since 20:15");
    stub->m_outputProblems[1] = QStringList() << dead;
    stub->m_outputProblems[2] = QStringList() << dead;
    stub->m_outputProblems[3] = QStringList() << shorted;

    AppUtil::updateFixtureReadiness(m_doc);
    ShowStatus::Entry e;
    foreach (const ShowStatus::Entry &x, ShowStatus::instance()->allEntries())
        if (x.key == "outputs.problems")
            e = x;
    QCOMPARE(e.key, QString("outputs.problems"));
    QCOMPARE(e.items.count(), 2);
    QCOMPARE(e.items.at(0), dead + " (universes 2, 3)");
    QCOMPARE(e.items.at(1), shorted + " (universe 4)");
    // The fix opens the first universe each one hits
    QCOMPARE(e.itemData, (QVariantList{ 1u, 3u }));

    // Cleared once the plugin stops reporting it
    stub->m_outputProblems.clear();
    AppUtil::updateFixtureReadiness(m_doc);
    foreach (const ShowStatus::Entry &x, ShowStatus::instance()->allEntries())
        QVERIFY(x.key != "outputs.problems");
}

/** Right-click @p at in the tree's viewport the way a hand does: press,
    the context menu (closed again unpicked), release. */
static void rightClickAt(ConnectionsTree &tree, const QPoint &at)
{
    QWidget *vp = tree.m_tree->viewport();
    QTest::mousePress(vp, Qt::RightButton, Qt::NoModifier, at);
    QTimer::singleShot(0, []() {
        if (QWidget *m = QApplication::activePopupWidget())
            m->close();
    });
    QContextMenuEvent ev(QContextMenuEvent::Mouse, at, vp->mapToGlobal(at));
    QApplication::sendEvent(vp, &ev);
    QTest::mouseRelease(vp, Qt::RightButton, Qt::NoModifier, at);
    /* The menu closing hands focus back to the list -- what the offscreen
       platform, with no active window, does not do by itself. */
    QFocusEvent back(QEvent::FocusIn, Qt::PopupFocusReason);
    QApplication::sendEvent(tree.m_tree, &back);
    QApplication::processEvents();
}

/* "every time I open the right click menu in connections the connections
   list scrolls right": the menu holds the rebuild off, and the rebuild that
   follows (and every five-second one) put the current row back with
   setCurrentItem(row, 0) -- which Qt answers by scrolling sideways to show
   that cell, back to the left edge of the list. Rows are what is picked
   here, so neither a click (either button), the menu nor a rebuild moves the
   list sideways; going up and down still follows the current row. */
void ConnectionsTree_Test::clickingARowDoesNotScrollSideways()
{
    fourPortNode(m_doc);
    ConnectionsTree tree(m_doc);
    showUniverseList(tree);
    QSplitter *split = qobject_cast<QSplitter *>(tree.m_tree->parentWidget());
    QVERIFY(split != nullptr);
    split->setSizes(QList<int>() << 160 << 840);   // narrower than its columns
    QApplication::processEvents();
    QScrollBar *h = tree.m_tree->horizontalScrollBar();
    QVERIFY(h->maximum() > 0);

    const int y = tree.m_tree->visualItemRect(universeRow(tree.m_tree, 2)).center().y();
    const int right = tree.m_tree->viewport()->width() - 4;

    foreach (const int start, QList<int>() << 0 << h->maximum() / 2 << h->maximum())
    {
        h->setValue(start);
        rightClickAt(tree, QPoint(right, y));
        QCOMPARE(h->value(), start);
        rightClickAt(tree, QPoint(4, y));
        QCOMPARE(h->value(), start);
        QTest::mouseClick(tree.m_tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                          QPoint(right, y));
        QApplication::processEvents();
        QCOMPARE(h->value(), start);
        QTest::mouseClick(tree.m_tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                          QPoint(4, y));
        QApplication::processEvents();
        QCOMPARE(h->value(), start);
        QCOMPARE(tree.m_tree->currentItem(), universeRow(tree.m_tree, 2));

        tree.refresh();                             // the periodic rebuild
        QApplication::processEvents();
        QCOMPARE(h->value(), start);
    }

    // Arrow keys still bring the current row into view, up and down.
    split->setSizes(QList<int>() << 160 << 840);
    tree.resize(1000, 150);                        // short: the list scrolls
    QApplication::processEvents();
    QScrollBar *v = tree.m_tree->verticalScrollBar();
    QVERIFY(v->maximum() > 0);
    v->setValue(0);
    h->setValue(h->maximum() / 2);
    const int side = h->value();
    tree.m_tree->setFocus();
    QTest::keyClick(tree.m_tree, Qt::Key_End);
    QApplication::processEvents();
    QCOMPARE(v->value(), v->maximum());
    QCOMPARE(h->value(), side);
}

/** Answer the dialogs the code under test is about to open, in order. An
    answer to a list is the first item starting with it ("#N" picks the Nth);
    to a text box, the text itself; "<cancel>" cancels. Every list offered
    lands in @p lists, every message box's text in @p messages. */
class DialogAnswers : public QObject
{
public:
    DialogAnswers(const QStringList &answers) : m_answers(answers)
    {
        m_timer.setInterval(5);
        connect(&m_timer, &QTimer::timeout, this, [this]() { step(); });
        m_timer.start();
    }
    QList<QStringList> lists;
    QStringList messages;
    int left() const { return m_answers.count(); }

private:
    void step()
    {
        QWidget *w = QApplication::activeModalWidget();
        if (w == nullptr)
            return;
        if (QMessageBox *mb = qobject_cast<QMessageBox *>(w))
        {
            messages << mb->text();
            mb->accept();
            return;
        }
        QInputDialog *d = qobject_cast<QInputDialog *>(w);
        if (d == nullptr || m_answers.isEmpty())
            return;
        const QString a = m_answers.takeFirst();
        if (a == "<cancel>")
        {
            d->reject();
            return;
        }
        const QStringList items = d->comboBoxItems();
        if (items.isEmpty() == false)
        {
            lists << items;
            QString pick;
            if (a.startsWith('#'))
                pick = items.value(a.mid(1).toInt());
            else
                foreach (const QString &it, items)
                    if (pick.isEmpty() && it.startsWith(a))
                        pick = it;
            if (pick.isEmpty())
            {
                d->reject();
                return;
            }
            d->setTextValue(pick);
        }
        else
            d->setTextValue(a);
        d->accept();
    }
    QStringList m_answers;
    QTimer m_timer;
};

/** The four-port node, with universe 8 already sending to its second port. */
static IOPluginStub *nodeWithOnePortTaken(Doc *doc, ConnectionsTree &tree)
{
    IOPluginStub *stub = fourPortNode(doc);
    InputOutputMap *iom = doc->inputOutputMap();
    iom->setOutputPatch(7, stub->name(), "", 0, false, 0);
    tree.applyTarget(7, stub->name(), 0, "172.18.2.50", 1);
    return stub;
}

/* "when patching .. and target address comes up .. should be able to select
   an unused Artnet->DMX or other interface if they're visible". */
void ConnectionsTree_Test::targetPickerListsFreePortsFirst()
{
    ConnectionsTree tree(m_doc);
    IOPluginStub *stub = nodeWithOnePortTaken(m_doc, tree);
    InputOutputMap *iom = m_doc->inputOutputMap();
    // Declared by hand, with two ports; and one only a patch names.
    iom->addManualTarget(stub->name(), 0, "10.0.0.7");
    iom->addManualPort("10.0.0.7", 16);
    iom->addManualPort("10.0.0.7", 17);
    QVERIFY(iom->setOutputPatch(6, stub->name(), "", 0, false, 0));
    QVERIFY(tree.applyTarget(6, stub->name(), 0, "10.0.0.9", 5));

    const QList<ConnectionsTree::TargetPortChoice> c =
        tree.targetPortChoices(stub->name(), 0, 0);
    QStringList texts;
    foreach (const ConnectionsTree::TargetPortChoice &x, c)
        texts << x.text;
    QCOMPARE(texts, QStringList()
        << "Quad (172.18.2.50) — port 1 · universe 0:0:0 — free"
        << "Quad (172.18.2.50) — port 3 · universe 0:0:2 — free"
        << "Quad (172.18.2.50) — port 4 · universe 0:0:3 — free"
        << "10.0.0.7 — port 1 · universe 0:1:0 — free"
        << "10.0.0.7 — port 2 · universe 0:1:1 — free"
        << "Quad (172.18.2.50) — port 2 · universe 0:0:1 — used by 8: Universe 8"
        << "10.0.0.9 — universe 0:0:5 — used by 7: Universe 7");

    // Asked for universe 8 itself, its own port is not "used".
    bool ownFree = false;
    foreach (const ConnectionsTree::TargetPortChoice &x,
             tree.targetPortChoices(stub->name(), 0, 7))
        if (x.port == 1 && x.address == "172.18.2.50")
            ownFree = (x.owner == InputOutputMap::invalidUniverse());
    QVERIFY(ownFree);

    // Another line lists none of them.
    QVERIFY(tree.targetPortChoices(stub->name(), 1, 0).isEmpty());
}

void ConnectionsTree_Test::patchToPicksAVisiblePort()
{
    ConnectionsTree tree(m_doc);
    IOPluginStub *stub = nodeWithOnePortTaken(m_doc, tree);
    InputOutputMap *iom = m_doc->inputOutputMap();

    DialogAnswers answers(QStringList() << "#0" << "Quad (172.18.2.50) — port 3");
    tree.patchUnpatchedUniverseTo(0);
    QCOMPARE(answers.left(), 0);
    QCOMPARE(answers.lists.count(), 2);
    QVERIFY(answers.lists.at(1).contains("Another address…"));
    QVERIFY(answers.lists.at(1).contains("Broadcast (no target)"));
    QString a;
    quint32 p = 99;
    QVERIFY(tree.patchTarget(0, stub->name(), 0, a, p));
    QCOMPARE(a, QString("172.18.2.50"));
    QCOMPARE(p, quint32(2));
    QCOMPARE(iom->outputPatchesCount(0), 1);
}

void ConnectionsTree_Test::patchToAUsedPortChangesNothing()
{
    ConnectionsTree tree(m_doc);
    nodeWithOnePortTaken(m_doc, tree);
    InputOutputMap *iom = m_doc->inputOutputMap();

    {
        DialogAnswers answers(QStringList() << "#0" << "Quad (172.18.2.50) — port 2");
        tree.patchUnpatchedUniverseTo(0);
        QCOMPARE(answers.messages.count(), 1);
        QVERIFY2(answers.messages.first().contains("Universe 8 already sends"),
                 qPrintable(answers.messages.first()));
        QCOMPARE(iom->outputPatchesCount(0), 0);       // never half-patched
    }

    // Cancelling the target question leaves it unpatched too.
    DialogAnswers cancel(QStringList() << "#0" << "<cancel>");
    tree.patchUnpatchedUniverseTo(0);
    QCOMPARE(iom->outputPatchesCount(0), 0);
}

void ConnectionsTree_Test::patchToAnAddressTypedByHand()
{
    ConnectionsTree tree(m_doc);
    IOPluginStub *stub = nodeWithOnePortTaken(m_doc, tree);

    {
        DialogAnswers answers(QStringList() << "#0" << "Another address…"
                                            << "10.1.1.1" << "0:1:2");
        tree.patchUnpatchedUniverseTo(0);
        QCOMPARE(answers.left(), 0);
    }
    QString a;
    quint32 p = 99;
    QVERIFY(tree.patchTarget(0, stub->name(), 0, a, p));
    QCOMPARE(a, QString("10.1.1.1"));
    QCOMPARE(p, quint32(0x12));

    // Typing a port another universe holds is refused the same way.
    DialogAnswers clash(QStringList() << "#0" << "Another address…"
                                      << "172.18.2.50" << "0:0:1");
    tree.patchUnpatchedUniverseTo(1);
    QCOMPARE(clash.messages.count(), 1);
    QCOMPARE(m_doc->inputOutputMap()->outputPatchesCount(1), 0);
}

void ConnectionsTree_Test::retargetOffersTheVisiblePortsWithTheCurrentOne()
{
    ConnectionsTree tree(m_doc);
    IOPluginStub *stub = nodeWithOnePortTaken(m_doc, tree);

    DialogAnswers answers(QStringList() << "Quad (172.18.2.50) — port 4");
    tree.retargetPatch(7, stub->name(), 0);
    QCOMPARE(answers.left(), 0);
    QVERIFY(answers.lists.first().contains(
        "Quad (172.18.2.50) — port 2 · universe 0:0:1 — free  (current)"));
    QString a;
    quint32 p = 99;
    QVERIFY(tree.patchTarget(7, stub->name(), 0, a, p));
    QCOMPARE(p, quint32(3));
}

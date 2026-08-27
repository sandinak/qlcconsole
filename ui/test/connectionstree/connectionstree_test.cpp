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
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <QtTest>

#define protected public
#define private public

#include "connectionstree_test.h"
#include "connectionstree.h"
#include "iopluginstub.h"
#include "ioplugincache.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "universe.h"
#include "qlcfile.h"
#include "doc.h"

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

QTEST_MAIN(ConnectionsTree_Test)

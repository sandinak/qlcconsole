/*
  Q Light Controller Plus - Unit test
  inputoutputmap_test.cpp

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
#include <QSignalSpy>
#include <QtTest>
#include <QBuffer>

#define private public
#include "iopluginstub.h"
#include "inputoutputmap_test.h"
#include "inputoutputmap.h"
#include "qlcinputsource.h"
#include "grandmaster.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "qlcconfig.h"
#include "universe.h"
#include "qlcfile.h"
#include "doc.h"
#include "networkroute.h"
#include "rignetwork.h"
#undef private

#define TESTPLUGINDIR "../iopluginstub"
#define ENGINEDIR "../../src"
#include "../common/resource_paths.h"

static QDir testPluginDir()
{
    QDir dir(TESTPLUGINDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    return dir;
}

void InputOutputMap_Test::initTestCase()
{
    m_doc = new Doc(this);
    m_doc->ioPluginCache()->load(testPluginDir());
    QVERIFY(m_doc->ioPluginCache()->plugins().size() != 0);
}

void InputOutputMap_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = NULL;
}

void InputOutputMap_Test::initial()
{
    InputOutputMap im(m_doc, 4);
    QVERIFY(im.universesCount() == 4);
    QVERIFY(im.m_universeArray.count() == 4);
    QVERIFY(im.universeNames().count() == 4);
    QVERIFY(im.m_profiles.size() == 0);
    QVERIFY(im.profileNames().size() == 0);
}

void InputOutputMap_Test::pluginNames()
{
    InputOutputMap im(m_doc, 4);
    QCOMPARE(im.outputPluginNames().size(), 1);
    QCOMPARE(im.outputPluginNames().at(0), QString("I/O Plugin Stub"));
}

void InputOutputMap_Test::pluginInputs()
{
    InputOutputMap im(m_doc, 4);

    QVERIFY(im.pluginInputs("Foo").size() == 0);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QVERIFY(im.pluginInputs(stub->name()).size() == 4);
    QVERIFY(im.pluginInputs(stub->name()) == stub->inputs());
    QVERIFY(im.inputPluginNames().count() == 1);
    QVERIFY(im.inputPluginNames().at(0) == stub->name());
    QVERIFY(im.pluginSupportsFeedback(stub->name()) == false);
}

void InputOutputMap_Test::pluginOutputs()
{
    InputOutputMap om(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QStringList ls(om.pluginOutputs(stub->name()));
    QVERIFY(ls == stub->outputs());

    QVERIFY(om.pluginOutputs("Foobar").isEmpty() == true);
}

void InputOutputMap_Test::configurePlugin()
{
    InputOutputMap im(m_doc, 4);

    QCOMPARE(im.canConfigurePlugin("Foo"), false);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QCOMPARE(im.canConfigurePlugin("Foo"), false);
    QCOMPARE(im.canConfigurePlugin(stub->name()), false);
    stub->m_canConfigure = true;
    QCOMPARE(im.canConfigurePlugin(stub->name()), true);

    /* Must be able to call multiple times */
    im.configurePlugin(stub->name());
    QVERIFY(stub->m_configureCalled == 1);
    im.configurePlugin(stub->name());
    QVERIFY(stub->m_configureCalled == 2);
    im.configurePlugin(stub->name());
    QVERIFY(stub->m_configureCalled == 3);
}

void InputOutputMap_Test::inputPluginStatus()
{
    InputOutputMap im(m_doc, 4);

    QVERIFY(im.inputPluginStatus("Foo", QLCIOPlugin::invalidLine()).contains("Nothing selected"));
    QVERIFY(im.inputPluginStatus("Bar", 0).contains("Nothing selected"));
    QVERIFY(im.inputPluginStatus("Baz", 1).contains("Nothing selected"));
    QVERIFY(im.inputPluginStatus("Xyzzy", 2).contains("Nothing selected"));
    QVERIFY(im.inputPluginStatus("AYBABTU", 3).contains("Nothing selected"));

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QVERIFY(im.inputPluginStatus(stub->name(), QLCIOPlugin::invalidLine()) == stub->inputInfo(QLCIOPlugin::invalidLine()));
    QVERIFY(im.inputPluginStatus(stub->name(), 0) == stub->inputInfo(0));
    QVERIFY(im.inputPluginStatus(stub->name(), 1) == stub->inputInfo(1));
    QVERIFY(im.inputPluginStatus(stub->name(), 2) == stub->inputInfo(2));

    QVERIFY(im.pluginDescription("Foo") == "");
    QVERIFY(im.pluginDescription(stub->name()) == stub->pluginInfo());
}

void InputOutputMap_Test::outputPluginStatus()
{
    InputOutputMap om(m_doc, 4);

    QVERIFY(om.outputPluginStatus("Foo", QLCIOPlugin::invalidLine()).contains("Nothing selected"));
    QVERIFY(om.outputPluginStatus("Bar", 0).contains("Nothing selected"));
    QVERIFY(om.outputPluginStatus("Baz", 1).contains("Nothing selected"));
    QVERIFY(om.outputPluginStatus("Xyzzy", 2).contains("Nothing selected"));
    QVERIFY(om.outputPluginStatus("AYBABTU", 3).contains("Nothing selected"));

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QVERIFY(om.outputPluginStatus(stub->name(), 4) == stub->outputInfo(QLCIOPlugin::invalidLine()));
    QVERIFY(om.outputPluginStatus(stub->name(), 0) == stub->outputInfo(0));
    QVERIFY(om.outputPluginStatus(stub->name(), 1) == stub->outputInfo(1));
    QVERIFY(om.outputPluginStatus(stub->name(), 2) == stub->outputInfo(2));
}

void InputOutputMap_Test::danglingOutputPatchDetected()
{
    InputOutputMap iom(m_doc, 4);
    // The stub offers 4 outputs (lines 0-3). Line 9 is what a workspace saved
    // on a machine with more network interfaces looks like when opened here:
    // still "patched", plugin still accepts writes, nothing ever transmits.
    QVERIFY(iom.setOutputPatch(0, "I/O Plugin Stub", "", 9));

    QList<InputOutputMap::DanglingPatch> bad = iom.danglingOutputPatches();
    QCOMPARE(bad.size(), 1);
    QCOMPARE(bad.first().universe, quint32(0));
    QCOMPARE(bad.first().line, quint32(9));
    QCOMPARE(bad.first().availableLines, 4);
}

void InputOutputMap_Test::validOutputPatchNotFlagged()
{
    InputOutputMap iom(m_doc, 4);
    QVERIFY(iom.setOutputPatch(0, "I/O Plugin Stub", "", 0));
    QVERIFY(iom.danglingOutputPatches().isEmpty());
}

void InputOutputMap_Test::unresolvedInterfaceIdentityGoesPendingNotWrongIndex()
{
    InputOutputMap iom(m_doc, 4);
    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    // Line 1 is a real, in-range interface on this machine -- exactly the
    // trap: an unmatched UID used to be treated as "use the numeric line
    // anyway", silently binding to whatever line 1 happens to be here even
    // though it is not the interface the workspace actually asked for.
    QVERIFY(iom.setOutputPatch(0, stub->name(),
                               "an interface this machine does not have", 1));

    OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY(op != NULL);
    QVERIFY(op->isPending() == true);
    QVERIFY(op->isPatched() == false);
    QCOMPARE(op->outputName(), QString("an interface this machine does not have"));
    QVERIFY(stub->m_openOutputs.isEmpty());

    QList<InputOutputMap::DanglingPatch> bad = iom.danglingOutputPatches();
    QCOMPARE(bad.size(), 1);
    QCOMPARE(bad.first().universe, quint32(0));
    QCOMPARE(bad.first().missingInterface,
             QString("an interface this machine does not have"));
}

void InputOutputMap_Test::noneSentinelDoesNotGoPending()
{
    InputOutputMap iom(m_doc, 4);
    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    // "None" (KOutputNone) is OutputPatch::outputName()'s own fallback for
    // "no line was ever resolved" -- exactly what an incomplete/never-
    // finished patch saves as its UID. It must not be treated as a real
    // interface identity worth remembering as pending: line 1 here is a
    // real, in-range interface, so the OLD index-based behavior should
    // apply and this should resolve normally, not go pending under a ghost
    // interface literally named "None".
    QVERIFY(iom.setOutputPatch(0, stub->name(), KOutputNone, 1));

    OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY(op != NULL);
    QVERIFY(op->isPending() == false);
    QVERIFY(op->isPatched() == true);
    QCOMPARE(op->output(), quint32(1));
}

/*****************************************************************************
 * Patches follow the network, and are never lost offline
 *****************************************************************************/

namespace
{
    NetworkRoute::LocalAddress localAddr(const QString &iface, const QString &ip, int prefix)
    {
        NetworkRoute::LocalAddress la;
        la.iface = iface;
        la.ip = QHostAddress(ip);
        la.prefix = prefix;
        return la;
    }

    /* Model a machine: its addresses, and what the OS would route each
       target through ("*" = the default route). Nothing touches the network. */
    void mockMachine(const QList<NetworkRoute::LocalAddress> &locals,
                     const QMap<QString, QString> &routes)
    {
        NetworkRoute::setResolverForTest(
            [locals]() { return locals; },
            [routes](const QHostAddress &target) -> QHostAddress
            {
                const QString t = target.toString();
                if (routes.contains(t))
                    return QHostAddress(routes.value(t));
                // Longest stored prefix "a.b.c." match, then the default.
                QString best;
                for (auto it = routes.constBegin(); it != routes.constEnd(); ++it)
                    if (it.key().endsWith('.') && t.startsWith(it.key()) && it.key().size() > best.size())
                        best = it.key();
                if (best.isEmpty() == false)
                    return QHostAddress(routes.value(best));
                return routes.contains("*") ? QHostAddress(routes.value("*")) : QHostAddress();
            });
    }

    bool loadIOMap(InputOutputMap &iom, const QString &xml)
    {
        QXmlStreamReader reader(xml);
        reader.readNextStartElement();
        return iom.loadXML(reader);
    }

    QString saveIOMap(const InputOutputMap &iom)
    {
        QString out;
        QXmlStreamWriter writer(&out);
        writer.setAutoFormatting(true);
        writer.setAutoFormattingIndent(1);
        iom.saveXML(&writer);
        return out;
    }

    /* Every <Input>/<Output>/<Feedback> element, one per entry, each line
       trimmed so the comparison is exact apart from nesting depth. */
    QStringList patchElements(const QString &xml)
    {
        QStringList out;
        QString current;
        foreach (const QString &raw, xml.split('\n'))
        {
            const QString line = raw.trimmed();
            const bool opens = line.startsWith("<Output ") || line.startsWith("<Input ")
                               || line.startsWith("<Feedback ");
            if (opens)
                current = line;
            else if (current.isEmpty() == false)
                current += "\n" + line;
            else
                continue;
            if (line.endsWith("/>") && opens) { out << current; current.clear(); }
            else if (line.startsWith("</Output") || line.startsWith("</Input")
                     || line.startsWith("</Feedback")) { out << current; current.clear(); }
        }
        return out;
    }

    IOPluginStub *namedStub(Doc *doc, const QString &name, const QStringList &lines)
    {
        IOPluginStub *stub = static_cast<IOPluginStub*>(doc->ioPluginCache()->plugins().at(0));
        stub->m_nameOverride = name;
        stub->m_lineNames = lines;
        stub->m_openOutputs.clear();
        stub->m_openInputs.clear();
        return stub;
    }

    const char *kArtNetUniverse =
        "<InputOutputMap>\n"
        " <Universe Name=\"U05-T1-Ext\" ID=\"0\">\n"
        "  <Output Plugin=\"ArtNet\" UID=\"172.18.2.219\" Line=\"1\">\n"
        "   <PluginParameters outputIP=\"172.18.2.60\" outputUni=\"0\"/>\n"
        "  </Output>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";
}

void InputOutputMap_Test::cleanup()
{
    NetworkRoute::setResolverForTest(nullptr, nullptr);
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_nameOverride.clear();
    stub->m_lineNames.clear();
    stub->m_linesAreHardware = false;
    stub->m_openOutputs.clear();
    stub->m_openInputs.clear();
    stub->setTransmitNetworks(QStringList());
    stub->m_gateRefreshes = 0;
    stub->m_gateAllowed.clear();
}

void InputOutputMap_Test::offlineShowSavesPatchesByteIdentical()
{
    /* Branson's show (DanceHysteria 0.5, before it lost its patches): 68
       Art-Net outputs on 172.18.2.219 and 6 WLED outputs, opened on a laptop
       that is NOT on the show network. The WLED plugin is not loaded here at
       all, so this covers "plugin not installed" too. */
    QFile file(QFINDTESTDATA("dancehysteria-iomap.xml"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString original = QString::fromUtf8(file.readAll());
    const QStringList before = patchElements(original);
    QCOMPARE(before.count(), 74);

    mockMachine(QList<NetworkRoute::LocalAddress>()
                    << localAddr("lo0", "127.0.0.1", 8)
                    << localAddr("en0", "192.168.20.166", 23),
                QMap<QString, QString>{ { "127.", "127.0.0.1" }, { "*", "192.168.20.166" } });
    IOPluginStub *stub = namedStub(m_doc, "ArtNet",
                                   QStringList() << "127.0.0.1" << "192.168.20.166");

    InputOutputMap iom(m_doc, 4);
    QVERIFY(loadIOMap(iom, original));

    // Nothing opened -- not on 127.0.0.1, not on the home Wi-Fi.
    QVERIFY(stub->m_openOutputs.isEmpty());
    QVERIFY(iom.hasOfflinePatches());

    const QString saved = saveIOMap(iom);
    const QStringList after = patchElements(saved);
    QCOMPARE(after.count(), before.count());
    for (int i = 0; i < before.count(); i++)
        QCOMPARE(after.at(i), before.at(i));

    // Aliases and manual targets survive too.
    QCOMPARE(saved.count("<TargetAlias "), original.count("<TargetAlias "));
    QCOMPARE(saved.count("<ManualTarget "), original.count("<ManualTarget "));

    /* And when the laptop rejoins the show VLAN (any address on it), every
       Art-Net patch reconnects with its parameters; WLED stays kept. */
    mockMachine(QList<NetworkRoute::LocalAddress>()
                    << localAddr("lo0", "127.0.0.1", 8)
                    << localAddr("en0", "192.168.20.166", 23)
                    << localAddr("en5", "172.18.2.157", 24),
                QMap<QString, QString>{ { "127.", "127.0.0.1" }, { "172.18.2.", "172.18.2.157" },
                                        { "*", "192.168.20.166" } });
    stub->m_lineNames << "172.18.2.157";
    QCOMPARE(iom.retryOfflinePatches(), 68);
    int patched = 0;
    for (quint32 u = 0; u < iom.universesCount(); u++)
        for (int i = 0; i < iom.outputPatchesCount(u); i++)
            if (iom.outputPatch(u, i)->isPatched())
            {
                QCOMPARE(iom.outputPatch(u, i)->output(), quint32(2));
                patched++;
            }
    QCOMPARE(patched, 68);
    QCOMPARE(iom.outputPatch(4, 0)->getPluginParameters().value("outputIP").toString(),
             QString("172.18.2.60"));
    QCOMPARE(iom.danglingOutputPatches().count(), 6);   // the WLED ones
}

void InputOutputMap_Test::offlinePatchIsInertAndListed()
{
    mockMachine(QList<NetworkRoute::LocalAddress>() << localAddr("en0", "192.168.1.5", 24),
                QMap<QString, QString>{ { "*", "192.168.1.5" } });
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "192.168.1.5");

    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, kArtNetUniverse));

    OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY(op != NULL);
    QVERIFY(op->isPending());
    QCOMPARE(op->outputName(), QString("172.18.2.219"));
    QCOMPARE(op->getPluginParameters().value("outputIP").toString(), QString("172.18.2.60"));
    QVERIFY(stub->m_openOutputs.isEmpty());

    const QList<InputOutputMap::DanglingPatch> bad = iom.danglingOutputPatches();
    QCOMPARE(bad.count(), 1);
    QCOMPARE(bad.first().missingInterface, QString("172.18.2.219"));

    // Retrying while still away changes nothing.
    QCOMPARE(iom.retryOfflinePatches(), 0);
    QVERIFY(op->isPending());
}

/* Backup lanterns ("outputIP2"...) on a WLED patch kept because the WLED
   output is not here: every device is written back as it was loaded. */
void InputOutputMap_Test::wledBackupsKeptOfflineUnchanged()
{
    mockMachine(QList<NetworkRoute::LocalAddress>() << localAddr("en0", "192.168.1.5", 24),
                QMap<QString, QString>{ { "*", "192.168.1.5" } });
    namedStub(m_doc, "ArtNet", QStringList() << "192.168.1.5");

    const QString original =
        "<InputOutputMap>\n"
        " <Universe Name=\"Lantern SL\" ID=\"0\">\n"
        "  <Output Plugin=\"WLED\" UID=\"WLED (JSON over Wi-Fi)\" Line=\"0\">\n"
        "   <PluginParameters outputIP=\"172.18.2.50\" outputIP2=\"172.18.2.51\" "
        "outputIP3=\"172.18.2.52\" wledAddress=\"1\" wledAddress3=\"16\" "
        "wledChannels=\"15\" wledChannels3=\"4\"/>\n"
        "  </Output>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";
    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, original));
    QVERIFY(iom.hasOfflinePatches());

    const QStringList before = patchElements(original);
    const QStringList after = patchElements(saveIOMap(iom));
    QCOMPARE(before.count(), 1);
    QCOMPARE(after, before);
}

void InputOutputMap_Test::followsNetworkToAnotherMachine()
{
    /* Same show opened on kronk: 172.18.2.197 on the show VLAN, not .219. */
    mockMachine(QList<NetworkRoute::LocalAddress>()
                    << localAddr("en0", "192.168.1.5", 24)
                    << localAddr("en5", "172.18.2.197", 24),
                QMap<QString, QString>{ { "172.18.2.", "172.18.2.197" }, { "*", "192.168.1.5" } });
    IOPluginStub *stub = namedStub(m_doc, "ArtNet",
                                   QStringList() << "192.168.1.5" << "172.18.2.197");

    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, kArtNetUniverse));

    OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY(op->isPatched());
    QCOMPARE(op->output(), quint32(1));
    QCOMPARE(stub->m_openOutputs, QList<quint32>() << 1);
    QCOMPARE(op->getPluginParameters().value("outputIP").toString(), QString("172.18.2.60"));
    QCOMPARE(op->getPluginParameters().value("outputUni").toString(), QString("0"));

    // Saved back with this machine's compatible UID plus the network.
    const QStringList saved = patchElements(saveIOMap(iom));
    QCOMPARE(saved.count(), 1);
    QVERIFY(saved.first().startsWith(
        "<Output Plugin=\"ArtNet\" UID=\"172.18.2.197\" Line=\"1\" Network=\"172.18.2.0/24\">"));
    QVERIFY(saved.first().contains("outputIP=\"172.18.2.60\""));
}

void InputOutputMap_Test::followsRoutedPath()
{
    /* The show network is reachable only through a VPN (utun, its own /32):
       the OS routes 172.18.2.60 there, not out the default route. */
    mockMachine(QList<NetworkRoute::LocalAddress>()
                    << localAddr("en0", "192.168.1.5", 24)
                    << localAddr("utun4", "10.8.0.2", 32),
                QMap<QString, QString>{ { "172.18.2.", "10.8.0.2" }, { "*", "192.168.1.5" } });
    namedStub(m_doc, "ArtNet", QStringList() << "192.168.1.5" << "10.8.0.2");

    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, kArtNetUniverse));
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(1));
}

void InputOutputMap_Test::defaultRouteIsNotTheShowNetwork()
{
    /* At home the OS would happily "route" 172.18.2.60 out the Wi-Fi to the
       internet gateway. That is not the show network: stay offline. */
    mockMachine(QList<NetworkRoute::LocalAddress>() << localAddr("en0", "192.168.1.5", 24),
                QMap<QString, QString>{ { "*", "192.168.1.5" } });
    namedStub(m_doc, "ArtNet", QStringList() << "192.168.1.5");

    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, kArtNetUniverse));
    QVERIFY(iom.outputPatch(0, 0)->isPending());
}

void InputOutputMap_Test::storedNetworkResolves()
{
    /* A broadcast patch (no unicast target) with a stored network, opened on
       a machine whose show-net address and netmask both differ. */
    mockMachine(QList<NetworkRoute::LocalAddress>()
                    << localAddr("en0", "192.168.1.5", 24)
                    << localAddr("en7", "10.20.30.40", 16),
                QMap<QString, QString>{ { "*", "192.168.1.5" } });
    namedStub(m_doc, "ArtNet", QStringList() << "192.168.1.5" << "10.20.30.40");

    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom,
        "<InputOutputMap><Universe Name=\"A\" ID=\"0\">"
        "<Output Plugin=\"ArtNet\" UID=\"10.20.1.1\" Line=\"3\" Network=\"10.20.0.0/16\"/>"
        "</Universe></InputOutputMap>"));
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(1));
}

void InputOutputMap_Test::offlinePatchReconnectsWhenNetworkAppears()
{
    mockMachine(QList<NetworkRoute::LocalAddress>() << localAddr("en0", "192.168.1.5", 24),
                QMap<QString, QString>{ { "*", "192.168.1.5" } });
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "192.168.1.5");

    InputOutputMap iom(m_doc, 1);
    QSignalSpy spy(&iom, SIGNAL(offlinePatchesChanged()));
    QVERIFY(loadIOMap(iom, kArtNetUniverse));
    QVERIFY(iom.outputPatch(0, 0)->isPending());

    // The laptop joins the show VLAN with the very address it was built on.
    mockMachine(QList<NetworkRoute::LocalAddress>()
                    << localAddr("en0", "192.168.1.5", 24)
                    << localAddr("vlan0", "172.18.2.219", 24),
                QMap<QString, QString>{ { "172.18.2.", "172.18.2.219" }, { "*", "192.168.1.5" } });
    stub->m_lineNames << "172.18.2.219";

    QCOMPARE(iom.retryOfflinePatches(), 1);
    QCOMPARE(spy.count(), 1);
    OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY(op->isPending() == false);
    QVERIFY(op->isPatched());
    QCOMPARE(op->output(), quint32(1));
    QCOMPARE(stub->m_openOutputs, QList<quint32>() << 1);
    // Parameters replayed onto the plugin once there is a line.
    QCOMPARE(op->getPluginParameters().value("outputIP").toString(), QString("172.18.2.60"));
    QVERIFY(iom.danglingOutputPatches().isEmpty());
    QVERIFY(iom.hasOfflinePatches() == false);
}

/*****************************************************************************
 * Device inputs follow their device (a Bluetooth pad asleep and awake)
 *****************************************************************************/

namespace
{
    const QString kPad = QStringLiteral("Sony DualSense Wireless Controller (aa-bb-cc)");

    /** The stub as a device plugin offering @p lines; returns it. */
    IOPluginStub *devicePlugin(Doc *doc, const QStringList &lines)
    {
        IOPluginStub *stub = static_cast<IOPluginStub*>(doc->ioPluginCache()->plugins().at(0));
        stub->m_linesAreHardware = true;
        stub->m_lineNames = lines;
        stub->m_openInputs.clear();
        return stub;
    }

    /** Its lines changed: as a plugin says after a rescan. */
    void rescanned(IOPluginStub *stub, const QStringList &lines)
    {
        stub->m_lineNames = lines;
        stub->configure();      // emits configurationChanged()
    }
}

void InputOutputMap_Test::inputDeviceGoesAndComesBack()
{
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << "Keyboard" << kPad);
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setInputPatch(1, stub->name(), QString(), 1));
    InputPatch *ip = iom.inputPatch(1);
    QVERIFY(ip != NULL);
    QCOMPARE(ip->inputUID(), kPad);
    ip->setHidProfileName("DualSense");
    ip->setPluginParameter("mode", "pad");

    QSignalSpy gone(&iom, SIGNAL(inputDeviceDisconnected(quint32,QString,QString)));
    QSignalSpy back(&iom, SIGNAL(inputDeviceConnected(quint32,QString,QString)));
    QSignalSpy offline(&iom, SIGNAL(offlinePatchesChanged()));
    QSignalSpy config(&iom, SIGNAL(pluginConfigurationChanged(QString,bool)));

    // The pad goes to sleep: its line goes.
    rescanned(stub, QStringList() << "Keyboard");
    QCOMPARE(gone.count(), 1);
    QCOMPARE(gone.at(0).at(0).toUInt(), quint32(1));
    QCOMPARE(gone.at(0).at(1).toString(), stub->name());
    QCOMPARE(gone.at(0).at(2).toString(), kPad);
    QCOMPARE(back.count(), 0);
    QVERIFY(offline.count() >= 1);
    QCOMPARE(config.count(), 1);
    // Not patched to whatever is on its old line now; kept, as it was.
    QVERIFY(iom.inputPatch(1) == NULL);
    QCOMPARE(iom.universe(1)->offlinePatches().count(), 1);
    const Universe::OfflinePatch rec = iom.universe(1)->offlinePatches().first();
    QCOMPARE(rec.tag, Universe::InputPatchTag);
    QCOMPARE(rec.uid, kPad);
    QCOMPARE(rec.hidProfile, QString("DualSense"));
    QCOMPARE(rec.params.value("mode").toString(), QString("pad"));
    // Saved while it is away: still the pad, by name.
    QVERIFY2(saveIOMap(iom).contains(kPad), qPrintable(saveIOMap(iom)));
    // Said once: the retry tick does not say it again.
    QCOMPARE(iom.retryOfflinePatches(), 0);
    QCOMPARE(gone.count(), 1);

    // It wakes -- first in the list this time.
    rescanned(stub, QStringList() << kPad << "Keyboard");
    QCOMPARE(back.count(), 1);
    QCOMPARE(back.at(0).at(0).toUInt(), quint32(1));
    QCOMPARE(back.at(0).at(2).toString(), kPad);
    ip = iom.inputPatch(1);
    QVERIFY(ip != NULL);
    QCOMPARE(ip->input(), quint32(0));
    QCOMPARE(ip->inputUID(), kPad);
    QCOMPARE(ip->hidProfileName(), QString("DualSense"));
    QCOMPARE(ip->getPluginParameters().value("mode").toString(), QString("pad"));
    QVERIFY(iom.hasOfflinePatches() == false);
    QVERIFY(stub->m_openInputs.contains(0));

    // And its input arrives on the universe again.
    QSignalSpy values(&iom, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)));
    stub->emitValueChanged(UINT_MAX, 0, 5, 200);
    iom.flushInputs();
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.at(0).at(0).toUInt(), quint32(1));
    QCOMPARE(values.at(0).at(1).toUInt(), quint32(5));
}

void InputOutputMap_Test::inputDeviceFollowedToANewLine()
{
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << "Mouse" << kPad);
    InputOutputMap iom(m_doc, 1);
    QVERIFY(iom.setInputPatch(0, stub->name(), QString(), 1));
    InputPatch *ip = iom.inputPatch(0);
    QSignalSpy gone(&iom, SIGNAL(inputDeviceDisconnected(quint32,QString,QString)));

    // A device before it goes: the pad is still here, one line up.
    rescanned(stub, QStringList() << kPad);
    QCOMPARE(gone.count(), 0);
    QCOMPARE(iom.inputPatch(0), ip);
    QCOMPARE(ip->input(), quint32(0));
    QCOMPARE(ip->inputName(), kPad);

    QSignalSpy values(&iom, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)));
    stub->emitValueChanged(UINT_MAX, 1, 3, 9);     // the old line: nothing
    stub->emitValueChanged(UINT_MAX, 0, 3, 10);
    iom.flushInputs();
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.at(0).at(2).toUInt(), uint(10));
}

void InputOutputMap_Test::inputDeviceAbsentAtLoadAttachesWhenItAppears()
{
    // The show opens with the pad switched off.
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << "Keyboard");
    InputOutputMap iom(m_doc, 2);
    const QString xml = QString(
        "<InputOutputMap>\n"
        " <Universe Name=\"Pad\" ID=\"0\"/>\n"
        " <Universe Name=\"Pad\" ID=\"1\">\n"
        "  <Input Plugin=\"%1\" UID=\"%2\" Line=\"1\" HIDProfile=\"DualSense\"/>\n"
        " </Universe>\n"
        "</InputOutputMap>\n").arg(stub->name(), kPad);
    QVERIFY(loadIOMap(iom, xml));
    QVERIFY(iom.inputPatch(1) == NULL);
    QVERIFY(iom.hasOfflinePatches());

    // Switched on: the plugin's rescan alone reattaches it, no tick needed.
    QSignalSpy back(&iom, SIGNAL(inputDeviceConnected(quint32,QString,QString)));
    rescanned(stub, QStringList() << "Keyboard" << kPad);
    QCOMPARE(back.count(), 1);
    QVERIFY(iom.inputPatch(1) != NULL);
    QCOMPARE(iom.inputPatch(1)->input(), quint32(1));
    QCOMPARE(iom.inputPatch(1)->hidProfileName(), QString("DualSense"));
    QVERIFY(iom.hasOfflinePatches() == false);
}

void InputOutputMap_Test::inputOnANonDevicePluginIsLeftAlone()
{
    /* A protocol's lines are interfaces, not devices: renaming one is not
       a device leaving, and its patch stays where it is (as before). */
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << "10.0.0.1" << "10.0.0.2");
    stub->m_linesAreHardware = false;
    InputOutputMap iom(m_doc, 1);
    QVERIFY(iom.setInputPatch(0, stub->name(), QString(), 1));
    QSignalSpy gone(&iom, SIGNAL(inputDeviceDisconnected(quint32,QString,QString)));
    rescanned(stub, QStringList() << "10.0.0.1");
    QCOMPARE(gone.count(), 0);
    QVERIFY(iom.inputPatch(0) != NULL);
    QCOMPARE(iom.inputPatch(0)->input(), quint32(1));
}

void InputOutputMap_Test::userUnpatchOfflineRemovesIt()
{
    mockMachine(QList<NetworkRoute::LocalAddress>() << localAddr("en0", "192.168.1.5", 24),
                QMap<QString, QString>{ { "*", "192.168.1.5" } });
    namedStub(m_doc, "ArtNet", QStringList() << "192.168.1.5");

    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, kArtNetUniverse));
    QVERIFY(iom.outputPatch(0, 0)->isPending());

    iom.forgetOfflinePatches(0);
    QCOMPARE(iom.outputPatchesCount(0), 0);
    QVERIFY(iom.hasOfflinePatches() == false);
    QVERIFY(patchElements(saveIOMap(iom)).isEmpty());

    // Re-patching an offline universe by hand works normally.
    QVERIFY(loadIOMap(iom, kArtNetUniverse));
    QVERIFY(iom.setOutputPatch(0, "ArtNet", "192.168.1.5", 0, false, 0));
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    QVERIFY(iom.hasOfflinePatches() == false);
}

void InputOutputMap_Test::inputAndFeedbackKeptOffline()
{
    mockMachine(QList<NetworkRoute::LocalAddress>() << localAddr("en0", "192.168.1.5", 24),
                QMap<QString, QString>{ { "*", "192.168.1.5" } });
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "192.168.1.5");

    const QString xml =
        "<InputOutputMap>\n"
        " <Universe Name=\"Desk\" ID=\"0\">\n"
        "  <Input Plugin=\"ArtNet\" UID=\"172.18.2.219\" Line=\"1\" Profile=\"Some Desk\">\n"
        "   <PluginParameters inputUni=\"3\"/>\n"
        "  </Input>\n"
        "  <Feedback Plugin=\"ArtNet\" UID=\"172.18.2.219\" Line=\"1\"/>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";
    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, xml));

    // Not bound to the wrong line 1 / line 0 of this machine.
    QVERIFY(iom.inputPatch(0) == NULL);
    QVERIFY(iom.feedbackPatch(0) == NULL);
    QVERIFY(stub->m_openInputs.isEmpty());
    QCOMPARE(iom.danglingOutputPatches().count(), 2);

    QCOMPARE(patchElements(saveIOMap(iom)), patchElements(xml));

    // The network appears: both reconnect.
    mockMachine(QList<NetworkRoute::LocalAddress>()
                    << localAddr("en0", "192.168.1.5", 24)
                    << localAddr("en5", "172.18.2.197", 24),
                QMap<QString, QString>{ { "172.18.2.", "172.18.2.197" }, { "*", "192.168.1.5" } });
    stub->m_lineNames << "172.18.2.197";
    QCOMPARE(iom.retryOfflinePatches(), 2);
    QVERIFY(iom.inputPatch(0) != NULL);
    QCOMPARE(iom.inputPatch(0)->input(), quint32(1));
    QVERIFY(iom.feedbackPatch(0) != NULL);
    QCOMPARE(iom.feedbackPatch(0)->output(), quint32(1));
    QVERIFY(iom.hasOfflinePatches() == false);
}

void InputOutputMap_Test::missingPluginKept()
{
    const QString xml =
        "<InputOutputMap>\n"
        " <Universe Name=\"Ext\" ID=\"0\">\n"
        "  <Output Plugin=\"Not Installed Here\" UID=\"Some Device\" Line=\"2\">\n"
        "   <PluginParameters foo=\"bar\"/>\n"
        "  </Output>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";
    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, xml));
    QCOMPARE(iom.outputPatchesCount(0), 0);
    const QList<InputOutputMap::DanglingPatch> bad = iom.danglingOutputPatches();
    QCOMPARE(bad.count(), 1);
    QVERIFY(bad.first().pluginMissing);
    QCOMPARE(patchElements(saveIOMap(iom)), patchElements(xml));
}

void InputOutputMap_Test::routeLookupLoopback()
{
    // The one real OS lookup: loopback only, nothing is ever sent.
    QCOMPARE(NetworkRoute::sourceFor(QHostAddress("127.0.0.1")), QHostAddress("127.0.0.1"));
    QCOMPARE(NetworkRoute::networkOf(QHostAddress("172.18.2.219"), 24), QString("172.18.2.0/24"));
    QCOMPARE(NetworkRoute::targetFromParameters(
                 QMap<QString, QVariant>{ { "ucastIP", "10.0.0.9" } }), QString("10.0.0.9"));
    QVERIFY(NetworkRoute::isAddressIdentity("WLED (JSON over Wi-Fi)") == false);
}

void InputOutputMap_Test::universeNames()
{
    InputOutputMap iom(m_doc, 4);

    QCOMPARE(quint32(iom.universeNames().size()), iom.universesCount());
    QVERIFY(iom.universeNames().at(0).contains("Universe"));
    QVERIFY(iom.universeNames().at(1).contains("Universe"));
    QVERIFY(iom.universeNames().at(2).contains("Universe"));
    QVERIFY(iom.universeNames().at(3).contains("Universe"));

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    iom.setOutputPatch(0, stub->name(), "", 3);
    QCOMPARE(quint32(iom.universeNames().size()), iom.universesCount());
    QCOMPARE(iom.universeNames().at(0), QString("Universe 1"));
    QCOMPARE(iom.universeNames().at(1), QString("Universe 2"));
    QCOMPARE(iom.universeNames().at(2), QString("Universe 3"));
    QCOMPARE(iom.universeNames().at(3), QString("Universe 4"));

    iom.setOutputPatch(3, stub->name(), "", 2);
    QCOMPARE(quint32(iom.universeNames().size()), iom.universesCount());
    QCOMPARE(iom.universeNames().at(0), QString("Universe 1"));
    QCOMPARE(iom.universeNames().at(1), QString("Universe 2"));
    QCOMPARE(iom.universeNames().at(2), QString("Universe 3"));
    QCOMPARE(iom.universeNames().at(3), QString("Universe 4"));

    iom.setUniverseName(1, "Name Changed");
    iom.setUniverseName(42, "This is not the Universe you're looking for");

    QCOMPARE(iom.getUniverseNameByIndex(1), QString("Name Changed"));
    QCOMPARE(iom.getUniverseNameByIndex(2), QString("Universe 3"));
    QCOMPARE(iom.getUniverseNameByIndex(42), QString());
    QCOMPARE(iom.getUniverseNameByID(3), QString("Universe 4"));
}

void InputOutputMap_Test::addUniverse()
{
    InputOutputMap im(m_doc, 4);
    QVERIFY(im.universesCount() == 4);
    QVERIFY(im.addUniverse() == true);
    QVERIFY(im.universesCount() == 5);
    QVERIFY(im.getUniverseID(4) == 4);
    QVERIFY(im.getUniverseID(42) == Universe::invalid());

    /* try to add an existing universe */
    QVERIFY(im.addUniverse(3) == false);
    QVERIFY(im.universesCount() == 5);

    /* add a universe with high id and check that
     * there's no gaps */
    QVERIFY(im.addUniverse(8) == true);
    QVERIFY(im.universesCount() == 9);
}

void InputOutputMap_Test::removeUniverse()
{
    InputOutputMap im(m_doc, 4);
    QVERIFY(im.universesCount() == 4);

    // Creating a gap in the universe list is forbidden
    QVERIFY(im.removeUniverse(1) == false);
    QVERIFY(im.universesCount() == 4);

    // Removing the last universe is OK
    QVERIFY(im.removeUniverse(3) == true);
    QVERIFY(im.universesCount() == 3);

    QVERIFY(im.removeUniverse(7) == false);
    im.removeAllUniverses();
    QVERIFY(im.universesCount() == 0);
}

void InputOutputMap_Test::universe()
{
    InputOutputMap im(m_doc, 4);
    QVERIFY(im.universes().count() == 4);

    im.setUniversePassthrough(1, true);
    QVERIFY(im.getUniversePassthrough(1) == true);
    im.setUniversePassthrough(42, true);
    QVERIFY(im.getUniversePassthrough(42) == false);

    im.setUniverseMonitor(2, true);
    QVERIFY(im.getUniverseMonitor(2) == true);
    im.setUniverseMonitor(42, true);
    QVERIFY(im.getUniverseMonitor(42) == false);
}

void InputOutputMap_Test::profiles()
{
    InputOutputMap im(m_doc, 4);
    QVERIFY(im.m_profiles.size() == 0);

    QLCInputProfile* prof = new QLCInputProfile();
    prof->setManufacturer("Foo");
    prof->setModel("Bar");

    QVERIFY(im.addProfile(prof) == true);
    QVERIFY(im.m_profiles.size() == 1);
    QVERIFY(im.addProfile(prof) == false);
    QVERIFY(im.m_profiles.size() == 1);

    QVERIFY(im.profileNames().size() == 1);
    QVERIFY(im.profileNames().at(0) == prof->name());
    QVERIFY(im.profile(prof->name()) == prof);
    QVERIFY(im.profile("Foobar") == NULL);

    QVERIFY(im.removeProfile("Foobar") == false);
    QVERIFY(im.m_profiles.size() == 1);
    QVERIFY(im.removeProfile(prof->name()) == true);
    QVERIFY(im.m_profiles.size() == 0);
}

void InputOutputMap_Test::repatchAfterUnpatchByName()
{
    InputOutputMap im(m_doc, 4);
    IOPluginStub* stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    QVERIFY(im.setInputPatch(0, stub->name(), "", 0));
    QVERIFY(im.inputPatch(0) != NULL);
    im.setInputPatch(0, QString(), QString(), 0);              // no plugin, line 0: left pluginless
    QVERIFY(im.setInputPatch(0, stub->name(), "", 0));         // crashed before
    QVERIFY(im.inputPatch(0) != NULL);
    QVERIFY(im.inputPatch(0)->plugin() == stub);
    QVERIFY(im.setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine()));
}

void InputOutputMap_Test::setInputPatch()
{
    InputOutputMap im(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QLCInputProfile* prof = new QLCInputProfile();
    prof->setManufacturer("Foo");
    prof->setModel("Bar");
    im.addProfile(prof);

    QVERIFY(im.inputPatch(0) == NULL);
    QVERIFY(im.inputPatch(1) == NULL);
    QVERIFY(im.inputPatch(2) == NULL);
    QVERIFY(im.inputPatch(3) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 0) == InputOutputMap::invalidUniverse());
    QVERIFY(im.inputMapping(stub->name(), 1) == InputOutputMap::invalidUniverse());
    QVERIFY(im.inputMapping(stub->name(), 2) == InputOutputMap::invalidUniverse());
    QVERIFY(im.inputMapping(stub->name(), 3) == InputOutputMap::invalidUniverse());
    QVERIFY(im.isUniversePatched(0) == false);
    QVERIFY(im.isUniversePatched(42) == false);

    QVERIFY(im.setInputPatch(0, "Foobar", "", 0, prof->name()) == true);
    QVERIFY(im.inputPatch(0) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 0) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(1) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 1) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(2) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 2) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(3) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 3) == InputOutputMap::invalidUniverse());

    QVERIFY(im.setInputPatch(0, stub->name(), stub->inputs().at(0), 0) == true);
    QVERIFY(im.inputPatch(0)->plugin() == stub);
    QVERIFY(im.inputPatch(0)->input() == 0);
    QVERIFY(im.inputPatch(0)->profile() == NULL);
    QVERIFY(im.inputMapping(stub->name(), 0) == 0);
    QVERIFY(im.isUniversePatched(0) == true);

    QVERIFY(im.inputPatch(1) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 1) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(2) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 2) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(3) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 3) == InputOutputMap::invalidUniverse());

    QVERIFY(im.setInputPatch(2, stub->name(), stub->inputs().at(3), 3, prof->name()) == true);
    QVERIFY(im.inputPatch(0)->plugin() == stub);
    QVERIFY(im.inputPatch(0)->input() == 0);
    QVERIFY(im.inputPatch(0)->profile() == NULL);
    QVERIFY(im.inputMapping(stub->name(), 0) == 0);

    QVERIFY(im.inputPatch(1) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 1) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(2)->plugin() == stub);
    QVERIFY(im.inputPatch(2)->input() == 3);
    QVERIFY(im.inputPatch(2)->profile() == prof);
    QVERIFY(im.inputMapping(stub->name(), 2) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(3) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 3) == 2);

    // Universe out of bounds
    QVERIFY(im.setInputPatch(im.universesCount(), stub->name(), stub->inputs().at(0), 0) == false);
}


void InputOutputMap_Test::setOutputPatch()
{
    InputOutputMap iom(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QVERIFY(iom.setOutputPatch(0, "Foobar", "", 0) == false);
    QVERIFY(iom.outputPatch(0) == NULL);
    QVERIFY(iom.outputPatch(1) == NULL);
    QVERIFY(iom.outputPatch(2) == NULL);
    QVERIFY(iom.outputPatch(3) == NULL);

    QVERIFY(iom.setOutputPatch(4, stub->name(), "", 0) == false);
    QVERIFY(iom.outputPatch(0) == NULL);
    QVERIFY(iom.outputPatch(1) == NULL);
    QVERIFY(iom.outputPatch(2) == NULL);
    QVERIFY(iom.outputPatch(3) == NULL);

    QVERIFY(iom.setOutputPatch(4, stub->name(), "", 4) == false);
    QVERIFY(iom.outputPatch(0) == NULL);
    QVERIFY(iom.outputPatch(1) == NULL);
    QVERIFY(iom.outputPatch(2) == NULL);
    QVERIFY(iom.outputPatch(3) == NULL);

    QVERIFY(iom.setOutputPatch(3, stub->name(), stub->outputs().at(0), 0) == true);
    QVERIFY(iom.outputPatch(3)->plugin() == stub);
    QVERIFY(iom.outputPatch(3)->output() == 0);

    QVERIFY(iom.setOutputPatch(2, stub->name(), stub->outputs().at(1), 1) == true);
    QVERIFY(iom.outputPatch(2)->plugin() == stub);
    QVERIFY(iom.outputPatch(2)->output() == 1);

    QVERIFY(iom.setOutputPatch(1, stub->name(), stub->outputs().at(2), 2) == true);
    QVERIFY(iom.outputPatch(1)->plugin() == stub);
    QVERIFY(iom.outputPatch(1)->output() == 2);

    QVERIFY(iom.setOutputPatch(0, stub->name(), stub->outputs().at(3), 3) == true);
    QVERIFY(iom.outputPatch(0)->plugin() == stub);
    QVERIFY(iom.outputPatch(0)->output() == 3);

    QVERIFY(iom.outputMapping("Foo", 42) == QLCIOPlugin::invalidLine());
    QVERIFY(iom.outputMapping(stub->name(), 0) == 3);

    QVERIFY(iom.feedbackPatch(42) == NULL);
    QVERIFY(iom.feedbackPatch(0) == NULL);
}

void InputOutputMap_Test::setMultipleOutputPatches()
{
    InputOutputMap iom(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    // add an output patch
    QVERIFY(iom.setOutputPatch(1, stub->name(), "", 0, false, 0) == true);
    QVERIFY(iom.outputPatchesCount(1) == 1);
    QVERIFY(iom.outputPatch(1, 0)->plugin() == stub);
    QVERIFY(iom.outputPatch(1, 0)->output() == 0);

    // add another output patch
    QVERIFY(iom.setOutputPatch(1, stub->name(), "", 0, false, 1) == true);
    QVERIFY(iom.outputPatchesCount(1) == 2);
    QVERIFY(iom.outputPatch(1, 1)->plugin() == stub);
    QVERIFY(iom.outputPatch(1, 1)->output() == 0);

    // remove the first output patch
    QVERIFY(iom.setOutputPatch(1, stub->name(), "", QLCIOPlugin::invalidLine(), false, 0) == true);
    QVERIFY(iom.outputPatchesCount(1) == 1);
    QVERIFY(iom.outputPatch(1, 0)->plugin() == stub);
    QVERIFY(iom.outputPatch(1, 0)->output() == 0);
    QVERIFY(iom.outputPatch(1, 1) == NULL);

    // remove the first output patch again
    QVERIFY(iom.setOutputPatch(1, stub->name(), "", QLCIOPlugin::invalidLine(), false, 0) == true);
    QVERIFY(iom.outputPatchesCount(1) == 0);
}

void InputOutputMap_Test::slotValueChanged()
{
    InputOutputMap im(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QVERIFY(im.setInputPatch(0, stub->name(), stub->inputs().at(0), 0) == true);
    QVERIFY(im.inputPatch(0)->plugin() == stub);
    QVERIFY(im.inputPatch(0)->input() == 0);

    QSignalSpy spy(&im, SIGNAL(inputValueChanged(quint32, quint32, uchar, const QString&)));
    stub->emitValueChanged(UINT_MAX, 0, 15, UCHAR_MAX);
    QVERIFY(spy.size() == 0);
    im.flushInputs();
    QVERIFY(spy.size() == 1);
    QVERIFY(spy.at(0).at(0) == 0);
    QVERIFY(spy.at(0).at(1) == 15);
    QVERIFY(spy.at(0).at(2) == UCHAR_MAX);

    /* Invalid mapping for this plugin -> no signal */
    stub->emitValueChanged(UINT_MAX, 3, 15, UCHAR_MAX);
    QVERIFY(spy.size() == 1);
    im.flushInputs();
    QVERIFY(spy.size() == 1);
    QVERIFY(spy.at(0).at(0) == 0);
    QVERIFY(spy.at(0).at(1) == 15);
    QVERIFY(spy.at(0).at(2) == UCHAR_MAX);

    /* Invalid mapping for this plugin -> no signal */
    stub->emitValueChanged(UINT_MAX, 1, 15, UCHAR_MAX);
    QVERIFY(spy.size() == 1);
    im.flushInputs();
    QVERIFY(spy.size() == 1);
    QVERIFY(spy.at(0).at(0) == 0);
    QVERIFY(spy.at(0).at(1) == 15);
    QVERIFY(spy.at(0).at(2) == UCHAR_MAX);

    stub->emitValueChanged(UINT_MAX, 0, 5, 127);
    QVERIFY(spy.size() == 1);
    im.flushInputs();
    QVERIFY(spy.size() == 2);
    QVERIFY(spy.at(0).at(0) == 0);
    QVERIFY(spy.at(0).at(1) == 15);
    QVERIFY(spy.at(0).at(2) == UCHAR_MAX);
    QVERIFY(spy.at(1).at(0) == 0);
    QVERIFY(spy.at(1).at(1) == 5);
    QVERIFY(spy.at(1).at(2) == 127);

    stub->emitValueChanged(UINT_MAX, 0, 2, 0);
    QVERIFY(spy.size() == 2);
    stub->emitValueChanged(UINT_MAX, 0, 2, UCHAR_MAX);
    QVERIFY(spy.size() == 3);
    QVERIFY(spy.at(0).at(0) == 0);
    QVERIFY(spy.at(0).at(1) == 15);
    QVERIFY(spy.at(0).at(2) == UCHAR_MAX);
    QVERIFY(spy.at(1).at(0) == 0);
    QVERIFY(spy.at(1).at(1) == 5);
    QVERIFY(spy.at(1).at(2) == 127);
    QVERIFY(spy.at(2).at(0) == 0);
    QVERIFY(spy.at(2).at(1) == 2);
    QVERIFY(spy.at(2).at(2) == 0);
    im.flushInputs();
    QVERIFY(spy.size() == 4);
    QVERIFY(spy.at(0).at(0) == 0);
    QVERIFY(spy.at(0).at(1) == 15);
    QVERIFY(spy.at(0).at(2) == UCHAR_MAX);
    QVERIFY(spy.at(1).at(0) == 0);
    QVERIFY(spy.at(1).at(1) == 5);
    QVERIFY(spy.at(1).at(2) == 127);
    QVERIFY(spy.at(2).at(0) == 0);
    QVERIFY(spy.at(2).at(1) == 2);
    QVERIFY(spy.at(2).at(2) == 0);
    QVERIFY(spy.at(3).at(0) == 0);
    QVERIFY(spy.at(3).at(1) == 2);
    QVERIFY(spy.at(3).at(2) == UCHAR_MAX);
}

void InputOutputMap_Test::slotConfigurationChanged()
{
    InputOutputMap im(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QSignalSpy spy(&im, SIGNAL(pluginConfigurationChanged(QString, bool)));
    stub->configure();
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.at(0).size(), 2);
    QCOMPARE(spy.at(0).at(0).toString(), QString(stub->name()));
}

void InputOutputMap_Test::loadInputProfiles()
{
    InputOutputMap im(m_doc, 4);

    // No profiles in a nonexistent directory
    QDir dir("/path/to/a/nonexistent/place/beyond/this/universe");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtInputProfile));
    im.loadProfiles(dir);
    QVERIFY(im.profileNames().isEmpty() == true);

    // No profiles in an existing directory
    dir = testPluginDir();
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtInputProfile));
    im.loadProfiles(dir);
    QVERIFY(im.profileNames().isEmpty() == true);

    // Should be able to load profiles
    dir.setPath(INTERNAL_PROFILEDIR);
    im.loadProfiles(dir);
    QStringList names(im.profileNames());
    QVERIFY(names.size() > 0);

    // Shouldn't load duplicates
    im.loadProfiles(dir);
    QCOMPARE(names, im.profileNames());
}

void InputOutputMap_Test::inputSourceNames()
{
    InputOutputMap im(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*> (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QDir dir(INTERNAL_PROFILEDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtInputProfile));
    im.loadProfiles(dir);

    // Allow unpatched universe
    QString uni, ch;
    QVERIFY(im.inputSourceNames(new QLCInputSource(0, 0), uni, ch) == true);
    QCOMPARE(uni, QString("%1 -UNPATCHED-").arg(1));
    QCOMPARE(ch, QString("%1: ?").arg(1));

    // Don't allow unexisting universe
    QVERIFY(im.inputSourceNames(new QLCInputSource(100, 0), uni, ch) == false);

    QVERIFY(im.setInputPatch(0, stub->name(), stub->inputs().at(0), 0, QString("Generic MIDI")) == true);
    QVERIFY(im.inputSourceNames(new QLCInputSource(0, 0), uni, ch) == true);
    QCOMPARE(uni, tr("%1: Generic MIDI").arg(1));
    QCOMPARE(ch, tr("%1: Bank select MSB").arg(1));

    uni.clear();
    ch.clear();
    QVERIFY(im.inputSourceNames(new QLCInputSource(0, 50000), uni, ch) == true);
    QCOMPARE(uni, tr("%1: Generic MIDI").arg(1));
    QCOMPARE(ch, tr("%1: ?").arg(50001));

    QVERIFY(im.setInputPatch(0, stub->name(), stub->inputs().at(0), 0, QString()) == true);

    uni.clear();
    ch.clear();
    QVERIFY(im.inputSourceNames(new QLCInputSource(0, 0), uni, ch) == true);
    QCOMPARE(uni, tr("%1: %2").arg(1).arg(stub->name()));
    QCOMPARE(ch, tr("%1: ?").arg(1));

    QVERIFY(im.inputSourceNames(new QLCInputSource(0, QLCInputSource::invalidChannel), uni, ch) == false);
    QVERIFY(im.inputSourceNames(new QLCInputSource(InputOutputMap::invalidUniverse(), 0), uni, ch) == false);
    QVERIFY(im.inputSourceNames(new QLCInputSource(), uni, ch) == false);
}

void InputOutputMap_Test::profileDirectories()
{
    QDir dir = InputOutputMap::systemProfileDirectory();
    QDir ipDir;
#if defined(__APPLE__) || defined(Q_OS_MAC)
    // QLCFile::systemDirectory() resolves this relative to the app bundle's
    // executable (Contents/MacOS/<app> -> ../Resources/...), not the process's
    // CWD — mirror that here rather than assuming a flat CWD-relative path
    // (which only holds on Linux/Windows, where systemDirectory() doesn't
    // consult applicationDirPath() at all).
    ipDir.setPath(QString("%1/../%2").arg(QCoreApplication::applicationDirPath())
                                      .arg(INPUTPROFILEDIR));
#else
    ipDir.setPath(INPUTPROFILEDIR);
#endif
    QVERIFY(dir.filter() & QDir::Files);
    QVERIFY(dir.nameFilters().contains(QString("*%1").arg(KExtInputProfile)));
    QCOMPARE(dir.absolutePath(), ipDir.absolutePath());

    dir = InputOutputMap::userProfileDirectory();
#ifndef SKIP_TEST
    QVERIFY(dir.exists() == true);
#endif
    QVERIFY(dir.filter() & QDir::Files);
    QVERIFY(dir.nameFilters().contains(QString("*%1").arg(KExtInputProfile)));
    QVERIFY(dir.absolutePath().contains(USERINPUTPROFILEDIR));
}

void InputOutputMap_Test::claimReleaseDumpReset()
{
    InputOutputMap iom(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    iom.setOutputPatch(0, stub->name(), stub->outputs().at(0), 0);
    iom.setOutputPatch(1, stub->name(), stub->outputs().at(1), 1);
    iom.setOutputPatch(2, stub->name(), stub->outputs().at(2), 2);
    iom.setOutputPatch(3, stub->name(), stub->outputs().at(3), 3);

    QList<Universe*> unis = iom.claimUniverses();
    for (int i = 0; i < 512; i++)
        unis[0]->write(i, 'a');
    for (int i = 0; i < 512; i++)
        unis[1]->write(i, 'b');
    for (int i = 0; i < 512; i++)
        unis[2]->write(i, 'c');
    for (int i = 0; i < 512; i++)
        unis[3]->write(i, 'd');
    iom.releaseUniverses();

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    for (int i = 0; i < 512; i++)
        QCOMPARE(stub->m_universe.data()[i], 'a');

    for (int i = 512; i < 1024; i++)
        QCOMPARE(stub->m_universe.data()[i], 'b');

    for (int i = 1024; i < 1536; i++)
        QCOMPARE(stub->m_universe.data()[i], 'c');

    for (int i = 1536; i < 2048; i++)
        QCOMPARE(stub->m_universe.data()[i], 'd');

    iom.resetUniverses();
    for (int u = 0; u < iom.m_universeArray.size(); u++)
    {
        for (quint32 i = 0; i < 512; i++)
            QVERIFY(iom.m_universeArray.at(u)->preGMValues().data()[i] == 0);
    }
}

void InputOutputMap_Test::blackout()
{
    InputOutputMap iom(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    iom.setOutputPatch(0, stub->name(), stub->outputs().at(0), 0);
    iom.setOutputPatch(1, stub->name(), stub->outputs().at(1), 1);
    iom.setOutputPatch(2, stub->name(), stub->outputs().at(2), 2);
    iom.setOutputPatch(3, stub->name(), stub->outputs().at(3), 3);

    QList<Universe*> unis = iom.claimUniverses();
    unis[0]->setChannelCapability(42, QLCChannel::Intensity);
    unis[1]->setChannelCapability(42, QLCChannel::Intensity);
    unis[2]->setChannelCapability(42, QLCChannel::Intensity);
    unis[3]->setChannelCapability(42, QLCChannel::Intensity);

    for (int i = 0; i < 512; i++)
        unis[0]->write(i, 'a');
    for (int i = 0; i < 512; i++)
        unis[1]->write(i, 'b');
    for (int i = 0; i < 512; i++)
        unis[2]->write(i, 'c');
    for (int i = 0; i < 512; i++)
        unis[3]->write(i, 'd');
    iom.releaseUniverses();

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    iom.setBlackout(true);
    QVERIFY(iom.blackout() == true);

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    int offset = 0;

    for (int u = 0; u < 4; u++)
    {
        for (int i = 0; i < 512; i++)
        {
            if (i == 42)
                QVERIFY(stub->m_universe[offset + i] == (char) 0);
            else if (u == 0)
                QVERIFY(stub->m_universe[offset + i] == (char) 'a');
            else if (u == 1)
                QVERIFY(stub->m_universe[offset + i] == (char) 'b');
            else if (u == 2)
                QVERIFY(stub->m_universe[offset + i] == (char) 'c');
            else if (u == 3)
                QVERIFY(stub->m_universe[offset + i] == (char) 'd');
        }

        offset += 512;
    }

    iom.setBlackout(true);
    QVERIFY(iom.blackout() == true);

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    offset = 0;

    for (int u = 0; u < 4; u++)
    {
        for (int i = 0; i < 512; i++)
        {
            if (i == 42)
                QVERIFY(stub->m_universe[offset + i] == (char) 0);
            else if (u == 0)
                QVERIFY(stub->m_universe[offset + i] == (char) 'a');
            else if (u == 1)
                QVERIFY(stub->m_universe[offset + i] == (char) 'b');
            else if (u == 2)
                QVERIFY(stub->m_universe[offset + i] == (char) 'c');
            else if (u == 3)
                QVERIFY(stub->m_universe[offset + i] == (char) 'd');
        }

        offset += 512;
    }

    iom.toggleBlackout();
    QVERIFY(iom.blackout() == false);

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    for (int i = 0; i < 512; i++)
        QVERIFY(stub->m_universe[i] == 'a');
    for (int i = 512; i < 1024; i++)
        QVERIFY(stub->m_universe[i] == 'b');
    for (int i = 1024; i < 1536; i++)
        QVERIFY(stub->m_universe[i] == 'c');
    for (int i = 1536; i < 2048; i++)
        QVERIFY(stub->m_universe[i] == 'd');

    iom.setBlackout(false);
    QVERIFY(iom.blackout() == false);

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    for (int i = 0; i < 512; i++)
        QVERIFY(stub->m_universe[i] == 'a');
    for (int i = 512; i < 1024; i++)
        QVERIFY(stub->m_universe[i] == 'b');
    for (int i = 1024; i < 1536; i++)
        QVERIFY(stub->m_universe[i] == 'c');
    for (int i = 1536; i < 2048; i++)
        QVERIFY(stub->m_universe[i] == 'd');

    iom.toggleBlackout();
    QVERIFY(iom.blackout() == true);

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    offset = 0;

    for (int u = 0; u < 4; u++)
    {
        for (int i = 0; i < 512; i++)
        {
            if (i == 42)
                QVERIFY(stub->m_universe[offset + i] == (char) 0);
            else if (u == 0)
                QVERIFY(stub->m_universe[offset + i] == (char) 'a');
            else if (u == 1)
                QVERIFY(stub->m_universe[offset + i] == (char) 'b');
            else if (u == 2)
                QVERIFY(stub->m_universe[offset + i] == (char) 'c');
            else if (u == 3)
                QVERIFY(stub->m_universe[offset + i] == (char) 'd');
        }

        offset += 512;
    }
}

void InputOutputMap_Test::grandMaster()
{
    InputOutputMap iom(m_doc, 4);

    QVERIFY(iom.grandMasterChannelMode() == GrandMaster::Intensity);
    QVERIFY(iom.grandMasterValueMode() == GrandMaster::Reduce);
    QVERIFY(iom.grandMasterValue() == 255);

    iom.setGrandMasterValue(100);
    QVERIFY(iom.grandMasterValue() == 100);

    iom.setGrandMasterChannelMode(GrandMaster::AllChannels);
    QVERIFY(iom.grandMasterChannelMode() == GrandMaster::AllChannels);

    iom.setGrandMasterValueMode(GrandMaster::Limit);
    QVERIFY(iom.grandMasterValueMode() == GrandMaster::Limit);
}

/****************************************************************************
 * Fork additions: operator labels, hand-declared targets, input conflicts
 ****************************************************************************/

void InputOutputMap_Test::targetAliases()
{
    InputOutputMap iom(m_doc, 4);

    QVERIFY(iom.targetAlias("172.18.2.10").isEmpty());

    iom.setTargetAlias("172.18.2.10", "CR041R");
    QCOMPARE(iom.targetAlias("172.18.2.10"), QString("CR041R"));
    QCOMPARE(iom.targetAliases().count(), 1);

    // An empty name CLEARS rather than storing an empty label, so the row
    // falls back to the bare address instead of showing " ()".
    iom.setTargetAlias("172.18.2.10", "");
    QVERIFY(iom.targetAlias("172.18.2.10").isEmpty());
    QCOMPARE(iom.targetAliases().count(), 0);
}

void InputOutputMap_Test::lineAliases()
{
    InputOutputMap iom(m_doc, 4);

    QVERIFY(iom.lineAlias("ArtNet", "172.18.2.17").isEmpty());

    iom.setLineAlias("ArtNet", "172.18.2.17", "FOH rack");
    QCOMPARE(iom.lineAlias("ArtNet", "172.18.2.17"), QString("FOH rack"));

    // Keyed by plugin AND line: the same line string under another protocol
    // is a different cable and must not inherit the label.
    QVERIFY(iom.lineAlias("E1.31", "172.18.2.17").isEmpty());

    iom.setLineAlias("ArtNet", "172.18.2.17", "");
    QVERIFY(iom.lineAlias("ArtNet", "172.18.2.17").isEmpty());
}

void InputOutputMap_Test::manualTargets()
{
    InputOutputMap iom(m_doc, 4);

    QVERIFY(iom.manualTargets().isEmpty());
    QVERIFY(iom.isManualTarget("172.18.2.99") == false);

    iom.addManualTarget("ArtNet", 1, "172.18.2.99");
    QCOMPARE(iom.manualTargets().count(), 1);
    QVERIFY(iom.isManualTarget("172.18.2.99"));
    QCOMPARE(iom.manualTargets().at(0).plugin, QString("ArtNet"));
    QCOMPARE(iom.manualTargets().at(0).line, quint32(1));

    // Declaring the same address twice is a no-op, not a duplicate row.
    iom.addManualTarget("ArtNet", 2, "172.18.2.99");
    QCOMPARE(iom.manualTargets().count(), 1);
    QCOMPARE(iom.manualTargets().at(0).line, quint32(1));

    // An empty address has no identity to key on and must not be stored.
    iom.addManualTarget("ArtNet", 0, "");
    QCOMPARE(iom.manualTargets().count(), 1);

    iom.removeManualTarget("172.18.2.99");
    QVERIFY(iom.manualTargets().isEmpty());
    QVERIFY(iom.isManualTarget("172.18.2.99") == false);
}

void InputOutputMap_Test::manualTargetPorts()
{
    InputOutputMap iom(m_doc, 4);

    iom.addManualTarget("ArtNet", 0, "172.18.2.99");
    iom.addManualPort("172.18.2.99", 0x001);
    iom.addManualPort("172.18.2.99", 0x002);
    QCOMPARE(iom.manualTargets().at(0).ports.count(), 2);

    // The same port address twice would be one port drawn twice, not two ports.
    iom.addManualPort("172.18.2.99", 0x002);
    QCOMPARE(iom.manualTargets().at(0).ports.count(), 2);

    // Ports on an address nobody declared go nowhere rather than creating one.
    iom.addManualPort("10.0.0.1", 0x003);
    QCOMPARE(iom.manualTargets().count(), 1);

    iom.removeManualPort("172.18.2.99", 0x001);
    QCOMPARE(iom.manualTargets().at(0).ports.count(), 1);
    QCOMPARE(iom.manualTargets().at(0).ports.at(0), quint32(0x002));

    // Forgetting the target takes its ports with it.
    iom.removeManualTarget("172.18.2.99");
    QVERIFY(iom.manualTargets().isEmpty());
}

void InputOutputMap_Test::universesWithInputOn()
{
    InputOutputMap iom(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QVERIFY(iom.universesWithInputOn(stub->name(), 0).isEmpty());

    QVERIFY(iom.setInputPatch(0, stub->name(), "", 0) == true);
    QCOMPARE(iom.universesWithInputOn(stub->name(), 0).count(), 1);

    // The engine happily lets a second universe subscribe to the same line --
    // which is why the connections tree has to ask before doing it. Prove the
    // query sees both, or the warning would never fire.
    QVERIFY(iom.setInputPatch(1, stub->name(), "", 0) == true);
    QCOMPARE(iom.universesWithInputOn(stub->name(), 0).count(), 2);

    // A different line is a different cable and no conflict at all.
    QVERIFY(iom.universesWithInputOn(stub->name(), 1).isEmpty());
    QVERIFY(iom.universesWithInputOn("Nonexistent", 0).isEmpty());
}

void InputOutputMap_Test::aliasesAndTargetsSurviveSaveLoad()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter writer(&buffer);

    {
        InputOutputMap iom(m_doc, 4);
        iom.setTargetAlias("172.18.2.10", "CR041R");
        iom.setLineAlias("ArtNet", "172.18.2.17", "FOH rack");
        iom.addManualTarget("ArtNet", 2, "172.18.2.99");
        iom.addManualPort("172.18.2.99", 0x011);
        iom.addManualPort("172.18.2.99", 0x012);
        QVERIFY(iom.saveXML(&writer) == true);
    }

    buffer.close();
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader reader(&buffer);
    reader.readNextStartElement();

    InputOutputMap loaded(m_doc, 4);
    QVERIFY(loaded.loadXML(reader) == true);

    QCOMPARE(loaded.targetAlias("172.18.2.10"), QString("CR041R"));
    QCOMPARE(loaded.lineAlias("ArtNet", "172.18.2.17"), QString("FOH rack"));
    QCOMPARE(loaded.manualTargets().count(), 1);

    const InputOutputMap::ManualTarget mt = loaded.manualTargets().at(0);
    QCOMPARE(mt.plugin, QString("ArtNet"));
    QCOMPARE(mt.line, quint32(2));
    QCOMPARE(mt.address, QString("172.18.2.99"));
    QCOMPARE(mt.ports.count(), 2);
    QCOMPARE(mt.ports.at(0), quint32(0x011));
    QCOMPARE(mt.ports.at(1), quint32(0x012));
}

/* Stock QLC+ 4.14.4 keeps its network-server settings in the IO map; a
   4.14.4 show saved here keeps them (TEST_MATRIX C6), and the next show
   opened doesn't inherit them. */
void InputOutputMap_Test::networkServerSurvivesASave()
{
    const QString stock = QStringLiteral(
        "<InputOutputMap>"
        "<BeatGenerator BeatType=\"Disabled\" BPM=\"0\"/>"
        "<NetworkServer Type=\"Native\" AutoStart=\"True\" Name=\"FOH\" Password=\"pw\"/>"
        "</InputOutputMap>");

    InputOutputMap iom(m_doc, 4);
    QVERIFY(loadIOMap(iom, stock));
    QCOMPARE(iom.networkServerType(), InputOutputMap::NativeServer);
    QCOMPARE(iom.networkServerAutoStart(), true);
    QCOMPARE(iom.networkServerName(), QString("FOH"));
    QCOMPARE(iom.networkServerPassword(), QString("pw"));

    InputOutputMap again(m_doc, 4);
    QVERIFY(loadIOMap(again, saveIOMap(iom)));
    QCOMPARE(again.networkServerAutoStart(), true);
    QCOMPARE(again.networkServerName(), QString("FOH"));
    QCOMPARE(again.networkServerPassword(), QString("pw"));

    // A web server has no name or password of its own
    QVERIFY(loadIOMap(again, QStringLiteral(
        "<InputOutputMap><NetworkServer Type=\"Web\" AutoStart=\"False\"/></InputOutputMap>")));
    QCOMPARE(again.networkServerType(), InputOutputMap::WebServer);
    QVERIFY(saveIOMap(again).contains("Name=") == false);

    // A show without the element gets the defaults, not the last show's
    QVERIFY(loadIOMap(iom, QStringLiteral("<InputOutputMap/>")));
    QCOMPARE(iom.networkServerType(), InputOutputMap::NativeServer);
    QCOMPARE(iom.networkServerAutoStart(), false);
    QVERIFY(iom.networkServerName().isEmpty());
    QVERIFY(iom.networkServerPassword().isEmpty());
}

/** The invariant the whole patch API rests on.
 *
 *  universe(id) resolves by ID, while outputPatch(), setOutputPatch() and
 *  setInputPatch() take the same argument as an ARRAY INDEX. Everything works
 *  only because the two are always equal, and that is enforced deliberately at
 *  both ends: addUniverse() assigns the next index as the id, refuses an id
 *  already present, and fills any gap so a higher id still lands at its own
 *  index; removeUniverse() refuses anything but the last entry rather than
 *  leave a hole.
 *
 *  Nothing states that contract anywhere, so this test is where it is written
 *  down. If either rule is ever relaxed -- to allow deleting a universe from
 *  the middle, say -- this fails immediately, instead of the application
 *  quietly repatching the wrong universe.
 */
void InputOutputMap_Test::universeIdAlwaysEqualsItsArrayIndex()
{
    InputOutputMap iom(m_doc, 4);

    for (quint32 i = 0; i < iom.universesCount(); i++)
    {
        QCOMPARE(iom.universes().at(int(i))->id(), i);
        QCOMPARE(iom.getUniverseID(int(i)), i);
        QVERIFY(iom.universe(i) == iom.universes().at(int(i)));
    }

    // Appending keeps it.
    QVERIFY(iom.addUniverse() == true);
    QCOMPARE(iom.universes().last()->id(), quint32(4));

    // An id that already exists is refused rather than duplicated.
    QVERIFY(iom.addUniverse(2) == false);
    QCOMPARE(iom.universesCount(), quint32(5));

    // A higher id fills the gap, so it still lands at its own index -- the
    // list never acquires a hole between id and position.
    QVERIFY(iom.addUniverse(7) == true);
    QCOMPARE(iom.universesCount(), quint32(8));
    for (quint32 i = 0; i < iom.universesCount(); i++)
        QCOMPARE(iom.universes().at(int(i))->id(), i);

    // Removal is last-only, so it cannot open a hole either.
    QVERIFY(iom.removeUniverse(3) == false);
    QCOMPARE(iom.universesCount(), quint32(8));
    QVERIFY(iom.removeUniverse(7) == true);
    QCOMPARE(iom.universesCount(), quint32(7));
    for (quint32 i = 0; i < iom.universesCount(); i++)
        QCOMPARE(iom.universes().at(int(i))->id(), i);
}

// Not APPLESS: profileDirectories() exercises QLCFile::systemDirectory(),
// which calls QCoreApplication::applicationDirPath() on macOS/iOS/Windows —
// that needs a real QCoreApplication instance to resolve correctly (without
// one it warns and returns an empty path, failing the path comparison).
/*****************************************************************************
 * Rig network transmit gate
 *****************************************************************************/

namespace
{
    /* The show on a school laptop: one Art-Net universe saved on the show
       VLAN, rig network 172.18.0.0/20. */
    const char *kRigShow =
        "<InputOutputMap>\n"
        " <RigNetwork>172.18.0.0/20</RigNetwork>\n"
        " <Universe Name=\"U01\" ID=\"0\">\n"
        "  <Output Plugin=\"ArtNet\" UID=\"172.18.2.219\" Line=\"1\" Network=\"172.18.2.0/24\">\n"
        "   <PluginParameters outputIP=\"172.18.2.60\" outputUni=\"0\"/>\n"
        "  </Output>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";

    /* A universe patched at school, to the school interface itself. */
    const char *kSchoolPatch =
        "<InputOutputMap>\n"
        "%1"
        " <Universe Name=\"U01\" ID=\"0\">\n"
        "  <Output Plugin=\"ArtNet\" UID=\"10.4.12.7\" Line=\"1\"/>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";

    void schoolLaptop()
    {
        mockMachine(QList<NetworkRoute::LocalAddress>()
                        << localAddr("lo0", "127.0.0.1", 8)
                        << localAddr("en0", "10.4.12.7", 16),
                    QMap<QString, QString>{ { "127.", "127.0.0.1" }, { "*", "10.4.12.7" } });
    }
}

void InputOutputMap_Test::rigNetworkParsing()
{
    QString why;
    QCOMPARE(RigNetwork::normalise("172.18.2.5/24"), QString("172.18.2.0/24"));
    QCOMPARE(RigNetwork::normalise(" 172.18.0.0/20 "), QString("172.18.0.0/20"));
    QCOMPARE(RigNetwork::normalise("10.1.2.3/32"), QString("10.1.2.3/32"));
    QVERIFY(RigNetwork::normalise("172.18.2.0", &why).isEmpty());
    QVERIFY(why.isEmpty() == false);
    QVERIFY(RigNetwork::normalise("172.18/20").isEmpty());
    QVERIFY(RigNetwork::normalise("300.1.1.1/8").isEmpty());
    QVERIFY(RigNetwork::normalise("10.0.0.0/0").isEmpty());
    QVERIFY(RigNetwork::normalise("10.0.0.0/33").isEmpty());
    QVERIFY(RigNetwork::normalise("fe80::/64").isEmpty());   // IPv6: not a rig network
    QVERIFY(RigNetwork::normalise("").isEmpty());

    QStringList bad;
    QCOMPARE(RigNetwork::parseList("172.18.0.0/20, 10.0.5.9/24;172.18.3.1/20\n bogus", &bad),
             QStringList() << "172.18.0.0/20" << "10.0.5.0/24");
    QCOMPARE(bad, QStringList() << "bogus");

    const QStringList nets = QStringList() << "172.18.0.0/20";
    QVERIFY(RigNetwork::contains(nets, QHostAddress("172.18.2.219")));
    QVERIFY(RigNetwork::contains(nets, QHostAddress("172.18.15.1")));
    QVERIFY(RigNetwork::contains(nets, QHostAddress("172.18.16.1")) == false);
    QVERIFY(RigNetwork::contains(nets, QHostAddress("10.4.12.7")) == false);
    // IPv6 is ignored for matching, and does not crash
    QVERIFY(RigNetwork::contains(nets, QHostAddress("fe80::1")) == false);
    QVERIFY(RigNetwork::contains(nets, QHostAddress("::ffff:172.18.2.1")) == false);
    QVERIFY(RigNetwork::contains(nets, QHostAddress()) == false);
    QVERIFY(RigNetwork::contains(QStringList() << "fe80::/64", QHostAddress("fe80::1")) == false);

    // Setting them normalises and drops junk
    InputOutputMap iom(m_doc, 1);
    iom.setRigNetworks(QStringList() << "172.18.2.5/20" << "nope" << "172.18.0.0/20" << "::1/128");
    QCOMPARE(iom.rigNetworks(), QStringList() << "172.18.0.0/20");
}

void InputOutputMap_Test::rigNetworkHeldOffTheRigNetwork()
{
    schoolLaptop();
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");

    // Saved on the show VLAN: nothing here reaches it -- kept, and HELD.
    {
        InputOutputMap iom(m_doc, 1);
        QVERIFY(loadIOMap(iom, kRigShow));
        QVERIFY(iom.transmitGateActive());
        QVERIFY(stub->m_openOutputs.isEmpty());
        OutputPatch *op = iom.outputPatch(0, 0);
        QVERIFY(op->isPending());
        QVERIFY(op->isHeld());
        // The plugins were told too
        QCOMPARE(stub->transmitNetworks(), QStringList() << "172.18.0.0/20");

        const InputOutputMap::TransmitGateStatus st = iom.transmitGateStatus();
        QCOMPARE(int(st.state), int(InputOutputMap::TransmitGateStatus::Held));
        QCOMPARE(st.held, 1);
        QCOMPARE(st.networkOutputs, 1);
        QCOMPARE(st.networks, QStringList() << "172.18.0.0/20");
        QCOMPARE(st.connected, QStringList() << "en0 10.4.12.7");
        QCOMPARE(iom.danglingOutputPatches().count(), 1);
        QVERIFY(iom.danglingOutputPatches().first().heldReason.isEmpty() == false);
    }

    /* Patched at school to the school interface: the line is right here,
       and without the gate it would open and broadcast on it. Held. */
    stub->m_openOutputs.clear();
    {
        InputOutputMap iom(m_doc, 1);
        QVERIFY(loadIOMap(iom, QString(kSchoolPatch).arg(" <RigNetwork>172.18.0.0/20</RigNetwork>\n")));
        QVERIFY(stub->m_openOutputs.isEmpty());
        OutputPatch *op = iom.outputPatch(0, 0);
        QVERIFY(op->isHeld());
        QVERIFY2(op->heldReason().contains("10.4.12.7"), qPrintable(op->heldReason()));

        // Nothing reaches the plugin while held
        stub->m_universe.fill(0);
        op->dump(0, QByteArray(512, char(200)), true);
        QCOMPARE(stub->m_universe.count(char(200)), 0);

        // The tick keeps it held
        QCOMPARE(iom.retryOfflinePatches(), 0);
        QVERIFY(op->isHeld());
        QVERIFY(stub->m_openOutputs.isEmpty());

        // Saved exactly as loaded
        QCOMPARE(patchElements(saveIOMap(iom)),
                 patchElements(QString(kSchoolPatch).arg(QString())));
    }
}

void InputOutputMap_Test::rigNetworkHeldPatchResumesOnTheRigNetwork()
{
    schoolLaptop();
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");

    InputOutputMap iom(m_doc, 1);
    QSignalSpy gate(&iom, SIGNAL(transmitGateChanged()));
    QVERIFY(loadIOMap(iom, kRigShow));
    QVERIFY(iom.outputPatch(0, 0)->isHeld());

    // The laptop gets to the venue: a cable onto the show VLAN.
    mockMachine(QList<NetworkRoute::LocalAddress>()
                    << localAddr("lo0", "127.0.0.1", 8)
                    << localAddr("en0", "10.4.12.7", 16)
                    << localAddr("en5", "172.18.2.157", 24),
                QMap<QString, QString>{ { "127.", "127.0.0.1" }, { "172.18.2.", "172.18.2.157" },
                                        { "*", "10.4.12.7" } });
    stub->m_lineNames << "172.18.2.157";
    gate.clear();

    // The retry tick: it reconnects by itself, on the rig line
    QMetaObject::invokeMethod(&iom, "slotRetryOfflinePatches");
    OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY(op->isPending() == false);
    QVERIFY(op->isHeld() == false);
    QCOMPARE(op->output(), quint32(2));
    QCOMPARE(stub->m_openOutputs, QList<quint32>() << 2);
    QCOMPARE(op->getPluginParameters().value("outputIP").toString(), QString("172.18.2.60"));
    QCOMPARE(int(iom.transmitGateStatus().state), int(InputOutputMap::TransmitGateStatus::Clear));
    QVERIFY(gate.count() >= 1);
    QVERIFY(iom.danglingOutputPatches().isEmpty());
}

void InputOutputMap_Test::rigNetworkLivePatchAndSettingsHoldAndRelease()
{
    schoolLaptop();
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");

    InputOutputMap iom(m_doc, 1);
    // No rig networks yet: patching to the school interface opens it
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1));
    QCOMPARE(stub->m_openOutputs, QList<quint32>() << 1);
    QCOMPARE(int(iom.transmitGateStatus().state), int(InputOutputMap::TransmitGateStatus::NotSet));

    // Setting them holds it at once: closed, kept
    iom.setRigNetworks(QStringList() << "172.18.0.0/20");
    QVERIFY(stub->m_openOutputs.isEmpty());
    QVERIFY(iom.outputPatch(0, 0)->isHeld());
    QCOMPARE(iom.outputPatch(0, 0)->outputName(), QString("10.4.12.7"));

    // "Transmit anyway, this session": back on
    iom.setTransmitOverride(true);
    QVERIFY(iom.transmitGateActive() == false);
    QCOMPARE(stub->m_openOutputs, QList<quint32>() << 1);
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    QVERIFY(stub->transmitNetworks().isEmpty());
    QCOMPARE(int(iom.transmitGateStatus().state), int(InputOutputMap::TransmitGateStatus::Overridden));

    iom.setTransmitOverride(false);
    QVERIFY(stub->m_openOutputs.isEmpty());
    QVERIFY(iom.outputPatch(0, 0)->isHeld());

    // The check switched off on this machine
    iom.setTransmitGateEnabled(false);
    QCOMPARE(stub->m_openOutputs, QList<quint32>() << 1);
    QCOMPARE(int(iom.transmitGateStatus().state), int(InputOutputMap::TransmitGateStatus::Off));
    iom.setTransmitGateEnabled(true);
    QVERIFY(iom.outputPatch(0, 0)->isHeld());

    // A patch made by hand while the gate is on: kept, never opened
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 1));
    QVERIFY(stub->m_openOutputs.isEmpty());
    QVERIFY(iom.outputPatch(0, 1)->isHeld());

    // File -> New keeps the patch, so it keeps where that patch belongs
    iom.resetUniverses();
    QCOMPARE(iom.rigNetworks(), QStringList() << "172.18.0.0/20");
    QVERIFY(iom.outputPatch(0, 0)->isHeld());
}

void InputOutputMap_Test::rigNetworkNoneSetBehavesAsBefore()
{
    schoolLaptop();
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");

    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, QString(kSchoolPatch).arg(QString())));
    QVERIFY(iom.transmitGateActive() == false);
    QCOMPARE(stub->m_openOutputs, QList<quint32>() << 1);
    QVERIFY(iom.outputPatch(0, 0)->isHeld() == false);
    QVERIFY(stub->transmitNetworks().isEmpty());

    // ...with a warning to set them
    const InputOutputMap::TransmitGateStatus st = iom.transmitGateStatus();
    QCOMPARE(int(st.state), int(InputOutputMap::TransmitGateStatus::NotSet));
    QCOMPARE(st.held, 0);
}

void InputOutputMap_Test::rigNetworkLoopbackAndNonNetworkUnaffected()
{
    schoolLaptop();
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");

    InputOutputMap iom(m_doc, 2);
    iom.setRigNetworks(QStringList() << "172.18.0.0/20");
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 0));     // 127.0.0.1
    QCOMPARE(stub->m_openOutputs, QList<quint32>() << 0);
    QVERIFY(iom.outputPatch(0, 0)->isHeld() == false);
    QVERIFY(iom.transmitHoldReason("127.0.0.1", QMap<QString, QVariant>()).isEmpty());
    QVERIFY(iom.transmitHoldReason("127.0.0.1", QMap<QString, QVariant>()).isEmpty());

    // Not network output at all (a USB widget, MIDI): never looked at
    QVERIFY(iom.transmitHoldReason("FT232R USB UART", QMap<QString, QVariant>()).isEmpty());
    // An IPv6-named line is not on a rig network -- and nothing crashes
    QVERIFY(iom.transmitHoldReason("fe80::1", QMap<QString, QVariant>()).isEmpty());

    // Loopback output is not "network output": no warning about it
    QCOMPARE(int(iom.transmitGateStatus().state), int(InputOutputMap::TransmitGateStatus::Clear));
    QCOMPARE(iom.transmitGateStatus().networkOutputs, 0);
}

void InputOutputMap_Test::rigNetworkDeviceTargetsFollowTheRoute()
{
    /* WLED: one line, the device addresses in the parameters. A request
       leaves from wherever the OS routes it. */
    schoolLaptop();
    IOPluginStub *stub = namedStub(m_doc, "WLED", QStringList() << "WLED (JSON over Wi-Fi)");
    const char *xml =
        "<InputOutputMap>\n"
        " <RigNetwork>172.18.0.0/20</RigNetwork>\n"
        " <Universe Name=\"U01\" ID=\"0\">\n"
        "  <Output Plugin=\"WLED\" UID=\"WLED (JSON over Wi-Fi)\" Line=\"0\">\n"
        "   <PluginParameters outputIP=\"172.18.2.60\" outputIP2=\"172.18.2.61\"/>\n"
        "  </Output>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";

    {
        // At school the device "routes" out through the default route: held
        InputOutputMap iom(m_doc, 1);
        QVERIFY(loadIOMap(iom, xml));
        QVERIFY(stub->m_openOutputs.isEmpty());
        QVERIFY(iom.outputPatch(0, 0)->isHeld());
        QVERIFY2(iom.outputPatch(0, 0)->heldReason().contains("172.18.2.60"),
                 qPrintable(iom.outputPatch(0, 0)->heldReason()));
        QCOMPARE(iom.transmitGateStatus().held, 1);

        // On the show Wi-Fi it is reached from a rig address: it resumes
        mockMachine(QList<NetworkRoute::LocalAddress>()
                        << localAddr("en0", "172.18.2.40", 24),
                    QMap<QString, QString>{ { "*", "172.18.2.40" } });
        QCOMPARE(iom.retryOfflinePatches(), 1);
        QCOMPARE(stub->m_openOutputs, QList<quint32>() << 0);
        QVERIFY(iom.outputPatch(0, 0)->isHeld() == false);
    }

    // A name cannot be checked: held while the gate is on
    InputOutputMap iom(m_doc, 1);
    iom.setRigNetworks(QStringList() << "172.18.0.0/20");
    QMap<QString, QVariant> params;
    params["outputIP"] = "wled-bar.local";
    QVERIFY(iom.transmitHoldReason("WLED (JSON over Wi-Fi)", params).isEmpty() == false);
    params["outputIP"] = "127.0.0.1";
    QVERIFY(iom.transmitHoldReason("WLED (JSON over Wi-Fi)", params).isEmpty());
}

void InputOutputMap_Test::rigNetworkSaveLoadRoundTrip()
{
    schoolLaptop();
    namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");

    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, kRigShow));
    iom.setRigNetworks(QStringList() << "172.18.0.0/20" << "10.20.0.0/16");
    const QString saved = saveIOMap(iom);
    QCOMPARE(saved.count("<RigNetwork>"), 2);
    // Ahead of the universes, so a load knows them before opening a patch
    QVERIFY(saved.indexOf("<RigNetwork>") < saved.indexOf("<Universe"));
    // A held patch saves exactly as it was loaded
    QCOMPARE(patchElements(saved), patchElements(kRigShow));

    InputOutputMap again(m_doc, 1);
    QVERIFY(loadIOMap(again, saved));
    QCOMPARE(again.rigNetworks(), QStringList() << "172.18.0.0/20" << "10.20.0.0/16");
    QCOMPARE(saveIOMap(again), saved);

    // An older show, without any: none, and loading it clears the last show's
    again.setTransmitOverride(true);
    QVERIFY(loadIOMap(again, kArtNetUniverse));
    QVERIFY(again.rigNetworks().isEmpty());
    QVERIFY(again.transmitOverride() == false);
    QVERIFY(saveIOMap(again).contains("<RigNetwork") == false);
}

void InputOutputMap_Test::rigNetworkSuggestedFromPatchesNotWritten()
{
    schoolLaptop();
    namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");
    const char *xml =
        "<InputOutputMap>\n"
        " <Universe Name=\"U01\" ID=\"0\">\n"
        "  <Output Plugin=\"ArtNet\" UID=\"172.18.2.219\" Line=\"1\" Network=\"172.18.2.0/24\"/>\n"
        " </Universe>\n"
        " <Universe Name=\"U02\" ID=\"1\">\n"
        "  <Output Plugin=\"ArtNet\" UID=\"172.18.2.219\" Line=\"1\" Network=\"172.18.2.0/24\"/>\n"
        "  <Output Plugin=\"ArtNet\" UID=\"10.20.1.5\" Line=\"1\" Network=\"10.20.1.0/24\"/>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";
    InputOutputMap iom(m_doc, 2);
    QVERIFY(loadIOMap(iom, xml));
    QCOMPARE(iom.suggestedRigNetworks(), QStringList() << "172.18.2.0/24" << "10.20.1.0/24");
    // Only offered: nothing set, nothing saved
    QVERIFY(iom.rigNetworks().isEmpty());
    QVERIFY(saveIOMap(iom).contains("<RigNetwork") == false);
}

void InputOutputMap_Test::rigNetworkDecisionRefreshedOnChangeAndTick()
{
    /* The plugins decide once and cache it (no per-packet lookup): the
       engine has them decide again when the networks or the setting change,
       and on every reconnect tick even when nothing it knows changed -- an
       address can move under it (DHCP, VPN, Wi-Fi). */
    schoolLaptop();
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");
    stub->m_gateRefreshes = 0;

    InputOutputMap iom(m_doc, 1);
    iom.setRigNetworks(QStringList() << "172.18.0.0/20");
    QVERIFY(stub->m_gateRefreshes >= 1);
    QCOMPARE(stub->m_gateAllowed.value("10.4.12.7", true), false);
    QCOMPARE(stub->m_gateAllowed.value("127.0.0.1", false), true);

    // The session override: every line allowed at once
    iom.setTransmitOverride(true);
    QCOMPARE(stub->m_gateAllowed.value("10.4.12.7", false), true);
    iom.setTransmitOverride(false);
    QCOMPARE(stub->m_gateAllowed.value("10.4.12.7", true), false);

    // The setting switched off on this machine
    iom.setTransmitGateEnabled(false);
    QCOMPARE(stub->m_gateAllowed.value("10.4.12.7", false), true);
    iom.setTransmitGateEnabled(true);
    QCOMPARE(stub->m_gateAllowed.value("10.4.12.7", true), false);

    // The tick, with nothing changed: still asked, and the answer kept
    int before = stub->m_gateRefreshes;
    QVERIFY(QMetaObject::invokeMethod(&iom, "slotRetryOfflinePatches"));
    QCOMPARE(stub->m_gateRefreshes, before + 1);
    QCOMPARE(stub->m_gateAllowed.value("10.4.12.7", true), false);

    // The laptop moved onto the rig network: the next tick catches it
    stub->m_lineNames = QStringList() << "127.0.0.1" << "172.18.2.219";
    before = stub->m_gateRefreshes;
    QVERIFY(QMetaObject::invokeMethod(&iom, "slotRetryOfflinePatches"));
    QCOMPARE(stub->m_gateRefreshes, before + 1);
    QCOMPARE(stub->m_gateAllowed.value("172.18.2.219", false), true);
    QVERIFY(stub->m_gateAllowed.contains("10.4.12.7") == false);
}

void InputOutputMap_Test::aNetworkPatchWithOnlyALineNumberIsNotGuessed()
{
    mockMachine(QList<NetworkRoute::LocalAddress>()
                    << localAddr("lo0", "127.0.0.1", 8)
                    << localAddr("en0", "192.168.20.166", 23)
                    << localAddr("en5", "172.18.2.157", 24),
                QMap<QString, QString>{ { "127.", "127.0.0.1" }, { "172.18.2.", "172.18.2.157" },
                                        { "*", "192.168.20.166" } });
    IOPluginStub *stub = namedStub(m_doc, "ArtNet",
                                   QStringList() << "127.0.0.1" << "192.168.20.166" << "172.18.2.157");

    const QString bare =
        "<InputOutputMap>\n"
        " <Universe Name=\"Universe 9\" ID=\"0\">\n"
        "  <Output Plugin=\"ArtNet\" UID=\"None\" Line=\"1\"/>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";
    {
        InputOutputMap iom(m_doc, 4);
        QVERIFY(loadIOMap(iom, bare));
        QVERIFY2(stub->m_openOutputs.isEmpty(), "line 1 (the home Wi-Fi here) was opened on a guess");
        QVERIFY(iom.hasOfflinePatches());
        const QStringList saved = patchElements(saveIOMap(iom));
        QCOMPARE(saved.count(), 1);
        QVERIFY2(saved.first().contains("UID=\"None\"") && !saved.first().contains("Network="),
                 qPrintable(saved.first()));
    }

    // With a target it can follow: the line that reaches it
    const QString targeted =
        "<InputOutputMap>\n"
        " <Universe Name=\"Universe 9\" ID=\"0\">\n"
        "  <Output Plugin=\"ArtNet\" UID=\"None\" Line=\"1\">\n"
        "   <PluginParameters outputIP=\"172.18.2.60\" outputUni=\"0\"/>\n"
        "  </Output>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";
    stub->m_openOutputs.clear();
    InputOutputMap iom(m_doc, 4);
    QVERIFY(loadIOMap(iom, targeted));
    QVERIFY(iom.outputPatch(0, 0) != nullptr && iom.outputPatch(0, 0)->isPatched());
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(2));
}

QTEST_GUILESS_MAIN(InputOutputMap_Test)

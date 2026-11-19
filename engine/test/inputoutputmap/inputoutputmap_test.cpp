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
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QElapsedTimer>

#include "../common/stdprelude.h"
#define private public
#include "iopluginstub.h"
#include "inputoutputmap_test.h"
#include "inputoutputmap.h"
#include "qlcinputsource.h"
#include "qlcinputchannel.h"
#include "qlcinputprofile.h"
#include "grandmaster.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "qlcconfig.h"
#include "universe.h"
#include "qlcfile.h"
#include "doc.h"
#include "fixture.h"
#include "outputrulekeeper.h"
#include "patchundo.h"
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
    stub->m_networkDmx = true;
    stub->m_openOutputs.clear();
    stub->m_openInputs.clear();
    stub->setTransmitNetworks(QStringList());
    stub->m_gateRefreshes = 0;
    stub->m_gateAllowed.clear();
    stub->m_lineChannelRoles.clear();
    stub->m_lineDeviceKind.clear();
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

void InputOutputMap_Test::teachingHoldsNetworkOutput()
{
    mockMachine(QList<NetworkRoute::LocalAddress>() << localAddr("en0", "192.168.1.5", 24),
                QMap<QString, QString>{ { "*", "192.168.1.5" } });
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "192.168.1.5");
    const char *xml =
        "<InputOutputMap>\n"
        " <Universe Name=\"Stage\" ID=\"0\">\n"
        "  <Output Plugin=\"ArtNet\" UID=\"192.168.1.5\" Line=\"0\">\n"
        "   <PluginParameters outputIP=\"192.168.1.60\" outputUni=\"0\"/>\n"
        "  </Output>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";

    // A show that is not a teaching show: the patch opens as before.
    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, xml));
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    QVERIFY(!iom.teachingHold());
    QCOMPARE(iom.transmitGateStatus().held, 0);

    // Teaching on: the open patch is held, nothing sent, and said so.
    iom.setTeachingHold(true);
    QVERIFY(iom.teachingHold());
    QVERIFY(iom.outputPatch(0, 0)->isHeld());
    QVERIFY(iom.outputPatch(0, 0)->heldReason().contains("Teaching"));
    const InputOutputMap::TransmitGateStatus st = iom.transmitGateStatus();
    QVERIFY(st.teaching);
    QCOMPARE(st.held, 1);
    QCOMPARE(st.state, InputOutputMap::TransmitGateStatus::Held);
    // The patch is kept as saved
    QCOMPARE(patchElements(saveIOMap(iom)).count(), 1);

    // The teacher lets it through: it reopens by itself.
    iom.setTeachingHold(false);
    QVERIFY(!iom.outputPatch(0, 0)->isHeld());
    QVERIFY(iom.outputPatch(0, 0)->isPatched());

    // A teaching show opened: held from the start, never opened.
    stub->m_openOutputs.clear();
    InputOutputMap held(m_doc, 1);
    held.setTeachingHold(true);
    QVERIFY(loadIOMap(held, xml));
    QVERIFY(held.outputPatch(0, 0)->isHeld());
    QVERIFY(stub->m_openOutputs.isEmpty());
}

void InputOutputMap_Test::teachingHoldsUsbOutput()
{
    mockMachine(QList<NetworkRoute::LocalAddress>() << localAddr("en0", "192.168.1.5", 24),
                QMap<QString, QString>{ { "*", "192.168.1.5" } });
    IOPluginStub *stub = namedStub(m_doc, "DMX USB", QStringList() << "FT232R USB UART");
    const char *xml =
        "<InputOutputMap>\n"
        " <Universe Name=\"Stage\" ID=\"0\">\n"
        "  <Output Plugin=\"DMX USB\" UID=\"FT232R USB UART\" Line=\"0\"/>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";

    // Not a teaching show: the dongle opens as before.
    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, xml));
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    QCOMPARE(iom.transmitGateStatus().held, 0);

    // Teaching: held, kept as saved, counted as held but not as network output
    iom.setTeachingHold(true);
    QVERIFY(iom.outputPatch(0, 0)->isHeld());
    QVERIFY(iom.outputPatch(0, 0)->heldReason().contains("Teaching"));
    InputOutputMap::TransmitGateStatus st = iom.transmitGateStatus();
    QCOMPARE(st.held, 1);
    QCOMPARE(st.networkOutputs, 0);
    QCOMPARE(st.state, InputOutputMap::TransmitGateStatus::Held);
    QCOMPARE(patchElements(saveIOMap(iom)).count(), 1);

    // The teacher lets it through: it reopens
    iom.setTeachingHold(false);
    QVERIFY(!iom.outputPatch(0, 0)->isHeld());
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    st = iom.transmitGateStatus();
    QCOMPARE(st.held, 0);

    // A teaching show opened: never opens the dongle
    stub->m_openOutputs.clear();
    InputOutputMap held(m_doc, 1);
    held.setTeachingHold(true);
    QVERIFY(loadIOMap(held, xml));
    QVERIFY(held.outputPatch(0, 0)->isHeld());
    QVERIFY(stub->m_openOutputs.isEmpty());

    // Loopback and Dummy never leave the computer: not held
    namedStub(m_doc, "Loopback", QStringList() << "Loopback 1");
    const char *loop =
        "<InputOutputMap>\n"
        " <Universe Name=\"Stage\" ID=\"0\">\n"
        "  <Output Plugin=\"Loopback\" UID=\"Loopback 1\" Line=\"0\"/>\n"
        " </Universe>\n"
        "</InputOutputMap>\n";
    InputOutputMap local(m_doc, 1);
    local.setTeachingHold(true);
    QVERIFY(loadIOMap(local, loop));
    QVERIFY(local.outputPatch(0, 0)->isPatched());
    QCOMPARE(local.transmitGateStatus().held, 0);
}

/** Counts the queued calls an object is sent */
namespace {
class CallCounter : public QObject
{
public:
    int calls = 0;
    bool eventFilter(QObject *, QEvent *e) override
    {
        if (e->type() == QEvent::MetaCall)
            calls++;
        return false;
    }
};
}

// Fixed: heavy Art-Net input starved the GUI -- one hand-over per universe per
// frame, not one per value.
void InputOutputMap_Test::heavyInputIsHandedOverOncePerUniversePerFrame()
{
    const int universes = 16, values = 100;
    InputOutputMap im(m_doc, universes);
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    CallCounter counter;
    for (int u = 0; u < universes; u++)
    {
        QVERIFY(im.setInputPatch(u, stub->name(), "", 0));
        im.inputPatch(u)->installEventFilter(&counter);
    }

    QSignalSpy spy(&im, SIGNAL(inputValueChanged(quint32, quint32, uchar, const QString&)));
    for (int u = 0; u < universes; u++)
        for (int c = 0; c < values; c++)
            stub->emitValueChanged(u, 0, c, uchar(1 + (c + u) % 200));
    QCOMPARE(spy.size(), 0);   // nothing is emitted as the values arrive

    // The universe threads flush a frame: not on the GUI thread
    QThread *t = QThread::create([&im]() { im.flushInputs(); });
    t->start();
    QVERIFY(t->wait(5000));
    delete t;

    QCOMPARE(spy.size(), 0);   // still waiting for the GUI thread
    QCoreApplication::processEvents();
    QCOMPARE(counter.calls, universes);          // one queued call per universe
    QCOMPARE(spy.size(), universes * values);    // and every value came through

    // A quiet frame hands nothing over
    counter.calls = 0;
    QThread *q = QThread::create([&im]() { im.flushInputs(); });
    q->start();
    QVERIFY(q->wait(5000));
    delete q;
    QCoreApplication::processEvents();
    QCOMPARE(counter.calls, 0);
}

// Fixed: unpatching a universe wiped another universe's output settings -- the
// map closes (universe, line) in the order every plugin expects.
void InputOutputMap_Test::unpatchingAUniverseKeepsTheOthersOutputSettings()
{
    InputOutputMap iom(m_doc, 4);
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    typedef QLCIOPlugin P;

    // Universe 0 on line 1, universe 1 on line 0, each with a setting
    QVERIFY(iom.setOutputPatch(0, stub->name(), "", 1));
    QVERIFY(iom.setOutputPatch(1, stub->name(), "", 0));
    iom.outputPatch(0)->setPluginParameter("target", "10.0.0.1");
    iom.outputPatch(1)->setPluginParameter("target", "10.0.0.2");
    QCOMPARE(stub->getParameters(0, 1, P::Output).value("target").toString(), QString("10.0.0.1"));
    QCOMPARE(stub->getParameters(1, 0, P::Output).value("target").toString(), QString("10.0.0.2"));

    // Unpatch universe 0: universe 1 keeps its own
    QVERIFY(iom.setOutputPatch(0, QString(), QString(), P::invalidLine()));
    QVERIFY(stub->getParameters(0, 1, P::Output).isEmpty());
    QCOMPARE(stub->getParameters(1, 0, P::Output).value("target").toString(), QString("10.0.0.2"));

    // Same for inputs
    QVERIFY(iom.setInputPatch(2, stub->name(), "", 3));
    QVERIFY(iom.setInputPatch(3, stub->name(), "", 2));
    iom.inputPatch(2)->setPluginParameter("p", 1);
    iom.inputPatch(3)->setPluginParameter("p", 2);
    QVERIFY(iom.setInputPatch(2, QString(), QString(), P::invalidLine()));
    QCOMPARE(stub->getParameters(3, 2, P::Input).value("p").toInt(), 2);
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

/* A universe created while Blind is on -- added, or recreated by a show
   loaded in Blind -- starts with its output inhibited, as the others are. */
void InputOutputMap_Test::addUniverseInBlind()
{
    InputOutputMap im(m_doc, 2);
    QVERIFY(im.setOutputInhibited(true));
    QVERIFY(im.addUniverse());
    QVERIFY(im.addUniverse(5));             // fills the gap: 3, 4 and 5
    QCOMPARE(im.universesCount(), quint32(6));
    for (Universe *u : im.universes())
        QVERIFY(u->inhibitOutput());

    // A show loaded in Blind: every universe made again
    im.removeAllUniverses();
    QVERIFY(im.addUniverse(0));
    QVERIFY(im.addUniverse(1));
    for (Universe *u : im.universes())
        QVERIFY(u->inhibitOutput());

    QVERIFY(im.setOutputInhibited(false));
    QVERIFY(im.addUniverse());
    for (Universe *u : im.universes())
        QVERIFY(u->inhibitOutput() == false);
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

/* Review 10: "Transmit anyway" lifted the rig-network check and the show
   ran on this line (a Show-only laser on). The check back on holds the
   line (setPending): R9-1 says it is sent its closing frame first. A network
   plugin (Art-Net, E1.31) is told the networks BEFORE the patches are held
   (applyTransmitGate(): pushTransmitNetworks() first), so its own transmit
   hold is already on and drops that frame. Pass 0: a plugin with no hold of
   its own (the stub as it was) -- the frame arrives. Pass 1: one that holds
   like Art-Net -- it does not, and the node keeps the laser on. */
void InputOutputMap_Test::review10_rigGateBackOnSendsTheClosingFrame()
{
    schoolLaptop();
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");
    const int Laser = 14;
    for (int pass = 0; pass < 2; ++pass)
    {
        stub->m_holdLikeNetwork = pass == 1;
        stub->m_gateAllowed.clear();
        stub->m_universe.fill(char(0));
        int last = -1;
        bool held = false;
        {
            InputOutputMap iom(m_doc, 1);
            iom.setRigNetworks(QStringList() << "172.18.0.0/20");
            iom.setTransmitOverride(true);                  // "Transmit anyway, this session"
            if (iom.setOutputPatch(0, "ArtNet", QString(), 1) == false)
                QFAIL("could not patch");
            iom.universe(0)->markShowOnly(Laser, true, 0);
            stub->m_universe[512 + Laser] = char(255);      // the node has the laser on
            iom.setTransmitOverride(false);                 // the check back on
            held = iom.outputPatch(0, 0) != nullptr && iom.outputPatch(0, 0)->isHeld();
            last = uchar(stub->m_universe.at(512 + Laser));
            iom.setTransmitOverride(true);
        }
        stub->m_holdLikeNetwork = false;
        stub->m_universe.fill(char(0));
        QVERIFY(held);
        QVERIFY2(last == 0, qPrintable(QString("pass %1 (%2): the held line's last frame has the laser at %3 "
                                               "-- the closing frame was dropped by the plugin's own hold")
                                       .arg(pass).arg(pass == 1 ? "a plugin that holds like Art-Net" : "no plugin hold")
                                       .arg(last)));
    }
}

/* Review 10 H2, every way the check comes on mid-show -- "Transmit anyway"
   ended, the check switched on, the rig networks set, Teaching's hold --
   with a plugin that has no hold of its own and with one that holds as
   Art-Net does: the held line's last frame has the laser at safe. */
void InputOutputMap_Test::everyRigCheckHoldsWithTheClosingFrameFirst()
{
    schoolLaptop();
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");
    const int Laser = 14;
    enum Trigger { OverrideEnds, GateSwitchedOn, NetworksSet, TeachingOn };
    const QList<QPair<Trigger, const char *> > triggers = {
        { OverrideEnds, "Transmit anyway ended" },
        { GateSwitchedOn, "the rig check switched on" },
        { NetworksSet, "the rig networks set" },
        { TeachingOn, "Teaching holds real output" },
    };
    for (int pass = 0; pass < 2; ++pass)
    for (const auto &t : triggers)
    {
        const QString what = QString("%1, %2").arg(QString::fromLatin1(t.second),
                                                   pass == 1 ? "a plugin that holds like Art-Net" : "no plugin hold");
        stub->m_holdLikeNetwork = pass == 1;
        stub->m_gateAllowed.clear();
        stub->setTransmitNetworks(QStringList());
        stub->m_universe.fill(char(0));
        int last = -1;
        bool held = false;
        {
            InputOutputMap iom(m_doc, 1);
            // Before: the show runs on the line, nothing holds it.
            switch (t.first)
            {
                case OverrideEnds:
                    iom.setRigNetworks(QStringList() << "172.18.0.0/20");
                    iom.setTransmitOverride(true);
                break;
                case GateSwitchedOn:
                    iom.setTransmitGateEnabled(false);
                    iom.setRigNetworks(QStringList() << "172.18.0.0/20");
                break;
                case NetworksSet:
                case TeachingOn:
                break;
            }
            if (iom.setOutputPatch(0, "ArtNet", QString(), 1) == false)
                QFAIL("could not patch");
            QVERIFY2(iom.outputPatch(0, 0) != nullptr && iom.outputPatch(0, 0)->isPatched(), qPrintable(what));
            iom.universe(0)->markShowOnly(Laser, true, 0);
            stub->m_universe[512 + Laser] = char(255);      // the node has the laser on
            switch (t.first)
            {
                case OverrideEnds: iom.setTransmitOverride(false); break;
                case GateSwitchedOn: iom.setTransmitGateEnabled(true); break;
                case NetworksSet: iom.setRigNetworks(QStringList() << "172.18.0.0/20"); break;
                case TeachingOn: iom.setTeachingHold(true); break;
            }
            held = iom.outputPatch(0, 0) != nullptr && iom.outputPatch(0, 0)->isHeld();
            last = uchar(stub->m_universe.at(512 + Laser));
            iom.setTeachingHold(false);
            iom.setTransmitGateEnabled(true);
            iom.setRigNetworks(QStringList());
        }
        stub->m_holdLikeNetwork = false;
        stub->setTransmitNetworks(QStringList());
        stub->m_universe.fill(char(0));
        QVERIFY2(held, qPrintable(what));
        QVERIFY2(last == 0, qPrintable(QString("%1: the held line's last frame has the laser at %2")
                                       .arg(what).arg(last)));
    }
}

/* Review 11: an ordinary unplug and replug of one of two USB interfaces,
   NO Show-only fixtures anywhere. Before d82506c298 each universe came back
   on its own line (reconnect() by number). */
void InputOutputMap_Test::review11_unplugReplugOneOfTwoComesBackOnItsOwnDevice()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << "Pro (S/N: A1)" << "Pro (S/N: B2)";
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 0));     // A1
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 1));     // B2
    QVERIFY(iom.universe(0)->showOnlyChannelCount() == 0);
    QVERIFY(iom.universe(1)->showOnlyChannelCount() == 0);

    // A1 unplugged (cable bump): B2 is line 0 now.
    stub->m_lineNames = QStringList() << "Pro (S/N: B2)";
    stub->configure();
    // Two universes must never be opened on one device.
    QVERIFY2(!(iom.outputPatch(0, 0)->isPatched() && iom.outputPatch(1, 0)->isPatched()
               && iom.outputPatch(0, 0)->output() == iom.outputPatch(1, 0)->output()),
             "universe 1 and universe 2 are both sending to B2");

    // A1 plugged back in, where it was.
    stub->m_lineNames = QStringList() << "Pro (S/N: A1)" << "Pro (S/N: B2)";
    stub->configure();
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(0));
    QCOMPARE(iom.outputPatch(0, 0)->outputName(), QString("Pro (S/N: A1)"));
    QCOMPARE(iom.outputPatch(1, 0)->output(), quint32(1));
    QCOMPARE(iom.outputPatch(1, 0)->outputName(), QString("Pro (S/N: B2)"));
}

/* Review 11: two of one model whose line name carries no serial (Eurolite
   USB-DMX512 PRO, NanoDMX, Stageprofi name their line name() only). One is
   unplugged and plugged back in; no Show-only fixtures. */
void InputOutputMap_Test::review11_twoIdenticalDevicesUnplugReplugKeepTwoLines()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString e("Eurolite USB-DMX512 PRO");
    stub->m_lineNames = QStringList() << e << e;
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 0));
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 1));

    // Nothing unplugged: a configuration change keeps both where they are.
    stub->configure();
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(0));
    QCOMPARE(iom.outputPatch(1, 0)->output(), quint32(1));

    stub->m_lineNames = QStringList() << e;          // one unplugged
    stub->configure();
    stub->m_lineNames = QStringList() << e << e;     // and back
    stub->configure();
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(0));
    QCOMPARE(iom.outputPatch(1, 0)->output(), quint32(1));
}

/* Review 11: the universe whose device went has Show-only fixtures; the
   other universe's device is still there and only moved down a line. The
   kept-pending path closes the line it had -- which the other universe was
   just reopened on. */
void InputOutputMap_Test::review11_keptPendingDoesNotCloseTheLineAnotherUniverseMovedTo()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << "Pro (S/N: A1)" << "Pro (S/N: B2)";
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 1));     // universe 1 -> B2
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 0));     // universe 2 -> A1
    iom.universe(1)->markShowOnly(14, true, 0);                     // a laser on A1's rig

    stub->m_lineNames = QStringList() << "Pro (S/N: B2)";          // A1 unplugged
    stub->configure();

    OutputPatch *b = iom.outputPatch(0, 0);
    QVERIFY(b->isPatched());
    // Wherever universe 1 is patched now, that line is open in the plugin.
    QVERIFY2(stub->m_openOutputs.contains(b->output()),
             qPrintable(QString("universe 1 is patched on line %1 (%2), which is closed")
                        .arg(b->output()).arg(b->outputName())));
}

/* H5: a name two lines carry loads on the saved number when that line
   carries it; when it does not, an ordinary universe takes the first, as
   stock does. A Show-only universe stays pending on any hardware line
   (N1/N3, review 15). */
void InputOutputMap_Test::h5_identicalNamesLoadOnTheirSavedLines()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString e("Eurolite USB-DMX512 PRO");
    stub->m_lineNames = QStringList() << e << e << "Pro (S/N: A1)";
    InputOutputMap iom(m_doc, 3);
    Universe::OfflinePatch p;
    p.plugin = stub->name();
    p.uid = e;
    p.line = 1;
    iom.loadPatch(0, p);                                            // ordinary, saved on #2
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(1));
    p.line = 2;                                                     // saved where an A1 is now
    iom.loadPatch(1, p);                                            // ordinary: the first, as stock
    QVERIFY(iom.outputPatch(1, 0)->isPatched());
    QCOMPARE(iom.outputPatch(1, 0)->output(), quint32(0));
    iom.universe(2)->markShowOnly(14, true, 0);
    iom.loadPatch(2, p);                                            // Show-only: pending (N3)
    QVERIFY(iom.outputPatch(2, 0)->isPending());
    QCOMPARE(iom.outputPatch(2, 0)->reconnectHeld(), InputOutputMap::networkOnlyText(e));
}

/* A Doc's own I/O map, cleared: no fixture, no patch on universes 1-2,
   the output rules rebuilt now (a removed fixture's marks otherwise go only
   on the queued rebuild). */
static void clearDocPatches(Doc *doc)
{
    doc->clearContents();
    InputOutputMap *io = doc->inputOutputMap();
    for (quint32 u = 0; u < 2 && u < io->universesCount(); u++)
        while (io->outputPatchesCount(u) > 0)
            io->setOutputPatch(u, QString(), QString(), QLCIOPlugin::invalidLine());
    doc->outputRuleKeeper()->rebuild();
}

/* N1 (outputpatch.h): a universe with Show-only fixtures has no output on
   a hardware line -- refused, said, by line and by name; a venue's patch
   onto one is kept pending, said; its feedback line and a network output
   are fine; an ordinary universe is as stock. */
void InputOutputMap_Test::n1_aShowOnlyUniverseHasNoHardwareOutput()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString a1("Pro (S/N: A1)"), b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << a1 << b2;
    InputOutputMap iom(m_doc, 2);
    iom.universe(0)->markShowOnly(14, true, 0);
    const QString why = InputOutputMap::networkOnlyText(b2);
    QCOMPARE(why, QString("Lasers, foggers and flame need a network output in this version — Art-Net, sACN "
                          "(E1.31) or WLED: Pro (S/N: B2) can't send to them. Patch this universe to one of "
                          "those, or take the Show-only rule off."));

    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 1) == false);
    QCOMPARE(iom.lastPatchRefusal(), why);
    QCOMPARE(iom.outputPatchesCount(0), 0);
    QVERIFY(stub->m_openOutputs.isEmpty());
    // By name, the interface not here: no pending hardware patch either.
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString("Pro (S/N: C3)"), 2) == false);
    QCOMPARE(iom.lastPatchRefusal(), InputOutputMap::networkOnlyText("Pro (S/N: C3)"));
    QCOMPARE(iom.outputPatchesCount(0), 0);
    // Every call starts afresh, an out-of-range one too.
    QVERIFY(iom.setOutputPatch(99, stub->name(), QString(), 0) == false);
    QVERIFY(iom.lastPatchRefusal().isEmpty());

    // A feedback line is not output: allowed. An ordinary universe: as stock.
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 1, true));
    QVERIFY(iom.feedbackPatch(0) != NULL && iom.feedbackPatch(0)->isPatched());
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 0));
    QVERIFY(iom.outputPatch(1, 0)->isPatched());

    // A venue's patch onto USB: kept pending, said; the retry tick keeps it.
    Universe::OfflinePatch venue;
    venue.plugin = stub->name();
    venue.uid = b2;
    venue.line = 1;
    iom.replaceOutputPatches(0, QList<Universe::OfflinePatch>() << venue);
    QCOMPARE(iom.outputPatchesCount(0), 1);
    QVERIFY(iom.outputPatch(0, 0)->isPending());
    QVERIFY(iom.outputPatch(0, 0)->showOnlyHold());
    QCOMPARE(iom.outputPatch(0, 0)->reconnectHeld(), why);
    iom.retryOfflinePatches();
    QVERIFY(iom.outputPatch(0, 0)->isPending());

    // A network output: patched.
    stub->m_linesAreHardware = false;
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 1));
    QVERIFY(iom.lastPatchRefusal().isEmpty());
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    QVERIFY(iom.outputPatch(0, 0)->showOnlyHold() == false);
}

/* N1 through undo: the Devices tab's undo of a patch change puts a
   universe back on USB -- not once it has Show-only fixtures. */
void InputOutputMap_Test::n1_undoCannotPutAShowOnlyUniverseBackOnUsb()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << "Pro (S/N: A1)";
    InputOutputMap iom(m_doc, 1);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 0));     // ordinary, on USB
    QVERIFY(iom.patchUndo() != NULL);
    iom.patchUndo()->capture(QList<quint32>() << 0, "unpatch universe 1");
    QVERIFY(iom.setOutputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine()));
    QVERIFY(iom.outputPatchesCount(0) == 0 || iom.outputPatch(0, 0)->isPatched() == false);

    iom.universe(0)->markShowOnly(14, true, 0);                     // a laser now
    stub->m_openOutputs.clear();
    iom.patchUndo()->undo();
    QVERIFY(iom.outputPatchesCount(0) == 0 || iom.outputPatch(0, 0)->isPatched() == false);
    QVERIFY(stub->m_openOutputs.isEmpty());
    QVERIFY(stub->mapped(0, 0) == false);
}

/* N2 (review 15 C1): a fixture made Show only on a universe patched to USB.
   Every one of its channels is marked first; then the line gets its
   closing frame, flushed (a USB widget sends from its own thread), with
   all of them at safe -- the last frame the laser ever gets on it -- and
   only then is it closed. The patch says N1's words. */
void InputOutputMap_Test::n2_markingALaserOnUsbLeavesEveryChannelSafeOnTheWire()
{
    Doc *doc = m_doc;
    IOPluginStub *stub = static_cast<IOPluginStub*>(doc->ioPluginCache()->plugins().at(0));
    InputOutputMap *io = doc->inputOutputMap();
    clearDocPatches(doc);
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << "Pro (S/N: A1)";
    stub->m_universe.fill(char(0));
    stub->m_async = true;
    QVERIFY(io->setOutputPatch(0, stub->name(), QString(), 0));
    Fixture *laser = new Fixture(doc);
    laser->setName("Laser");
    laser->setChannels(2);
    laser->setUniverse(0);
    laser->setAddress(14);
    QVERIFY(doc->addFixture(laser));
    // Being focused in Program: both its channels up.
    Universe *uni = io->universe(0);
    uni->write(14, 255, true);
    uni->write(15, 255, true);
    const int flushes = stub->m_flushes;

    laser->setOutputRules(Fixture::ShowOnly);                       // Fixture ▸ Output rules
    const OutputPatch *op = io->outputPatch(0, 0);
    const bool held = op->isPending() && op->showOnlyHold();
    const QString why = op->reconnectHeld();
    const int c14 = uchar(stub->m_universe.at(14));
    const int c15 = uchar(stub->m_universe.at(15));
    const bool closed = stub->m_openOutputs.contains(0) == false;
    const int flushed = stub->m_flushes - flushes;
    stub->m_async = false;
    clearDocPatches(doc);
    stub->m_universe.fill(char(0));
    QVERIFY(held);
    QCOMPARE(why, InputOutputMap::networkOnlyText("Pro (S/N: A1)"));
    QVERIFY(closed);
    QCOMPARE(flushed, 1);
    QVERIFY2(c14 == 0 && c15 == 0,
             qPrintable(QString("the held line's last frame has the Show-only laser at %1 / %2 (safe is 0 / 0)")
                        .arg(c14).arg(c15)));
}

/* N2 for a group (Output Rules on a group: OutputRulesDialog::apply()
   marks its fixtures inside InputOutputMap::ShowOnlyMarks): the USB line is
   held once, after the last fixture is marked -- its closing frame has
   every channel of every fixture at safe. */
void InputOutputMap_Test::n2_markingAGroupHoldsOnceEveryFixtureIsSafe()
{
    Doc *doc = m_doc;
    IOPluginStub *stub = static_cast<IOPluginStub*>(doc->ioPluginCache()->plugins().at(0));
    InputOutputMap *io = doc->inputOutputMap();
    clearDocPatches(doc);
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << "Pro (S/N: A1)" << "Pro (S/N: B2)";
    stub->m_universe.fill(char(0));
    QVERIFY(io->setOutputPatch(0, stub->name(), QString(), 1));
    QList<Fixture *> group;
    for (int k = 0; k < 2; k++)
    {
        Fixture *fx = new Fixture(doc);
        fx->setName(QString("Fogger %1").arg(k + 1));
        fx->setChannels(2);
        fx->setUniverse(0);
        fx->setAddress(quint32(20 + 10 * k));
        QVERIFY(doc->addFixture(fx));
        group << fx;
    }
    Universe *uni = io->universe(0);
    for (int c : { 20, 21, 30, 31 })
        uni->write(c, 255, true);
    const int flushes = stub->m_flushes;
    {
        InputOutputMap::ShowOnlyMarks marks(io);
        for (Fixture *fx : group)
            fx->setOutputRules(Fixture::ShowOnly);
        QVERIFY(io->outputPatch(0, 0)->isPatched());                // not before the last
    }
    const bool held = io->outputPatch(0, 0)->isPending();
    const int flushed = stub->m_flushes - flushes;
    QList<int> wire;
    for (int c : { 20, 21, 30, 31 })
        wire << uchar(stub->m_universe.at(512 + c));
    clearDocPatches(doc);
    stub->m_universe.fill(char(0));
    QVERIFY(held);
    QCOMPARE(flushed, 1);
    QCOMPARE(wire, QList<int>() << 0 << 0 << 0 << 0);
}

/* N2 on a line another universe sends on too (review 17, finding 1): that
   patch is held too (F4, the plugin forgets it), then the held universe's
   closing frame goes out on the line, then the line is closed. The held
   patch unpatched clears its plugin entry (review 12 F6). */
void InputOutputMap_Test::n2_aHoldOnASharedLineHoldsEveryPatchOnIt()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    stub->m_forgotten.clear();
    const QString b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << "Pro (S/N: A1)" << b2;
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 1));     // to be the laser rig
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 1));     // another universe on B2 too
    stub->m_universe.fill(char(0x77));                              // what B2 was last sent
    iom.universe(0)->markShowOnly(14, true, 0);
    QVERIFY(iom.outputPatch(0, 0)->isPending() && iom.outputPatch(0, 0)->showOnlyHold());
    const OutputPatch *other = iom.outputPatch(1, 0);
    QVERIFY(other->isPending());
    QCOMPARE(other->holdKind(), OutputPatch::HeldReserved);
    QCOMPARE(other->reconnectHeld(), InputOutputMap::reservedText(b2, iom.universe(0)->name()));
    QVERIFY(stub->m_forgotten.contains(qMakePair(quint32(1), quint32(1))));    // the other: forgotten
    QCOMPARE(uchar(stub->m_universe.at(512 + 14)), uchar(0));                  // the closing frame
    QVERIFY(stub->m_openOutputs.contains(1) == false);                         // then closed
    stub->m_universe.fill(char(0));

    stub->m_forgotten.clear();
    QVERIFY(iom.setOutputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine()));
    QVERIFY(stub->m_forgotten.contains(qMakePair(quint32(1), quint32(0))));
    QVERIFY(stub->mapped(0, 1) == false);
}

/* N3: a real show file -- dimmers and lasers each on a USB interface (the
   lasers' held there since they were made Show only) -- opened on a
   machine whose interfaces moved. The I/O map is read before the fixtures;
   once they are, the lasers' universe is held, before anything is sent,
   waiting on its saved name. N4: the rule taken off, it comes back by its
   name, as stock. */
void InputOutputMap_Test::n3_aShowLoadedWithLasersOnUsbKeepsThemPending()
{
    const QString a1("Pro (S/N: A1)"), b2("Pro (S/N: B2)");
    Doc *doc = m_doc;
    IOPluginStub *stub = static_cast<IOPluginStub*>(doc->ioPluginCache()->plugins().at(0));
    InputOutputMap *io = doc->inputOutputMap();
    clearDocPatches(doc);
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << a1 << b2;
    QVERIFY(io->setOutputPatch(0, stub->name(), QString(), 0));     // dimmers -> A1
    QVERIFY(io->setOutputPatch(1, stub->name(), QString(), 1));     // lasers -> B2
    Fixture *dim = new Fixture(doc);
    dim->setName("Dimmer");
    dim->setChannels(1);
    dim->setUniverse(0);
    dim->setAddress(0);
    QVERIFY(doc->addFixture(dim));
    Fixture *laser = new Fixture(doc);
    laser->setName("Laser");
    laser->setChannels(1);
    laser->setUniverse(1);
    laser->setAddress(0);
    laser->setOutputRules(Fixture::ShowOnly);
    QVERIFY(doc->addFixture(laser));
    QVERIFY(io->outputPatch(1, 0)->isPending());                    // N2, where it was built
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    QVERIFY(doc->saveXML(&w));
    w.writeEndDocument();
    const QByteArray xml = buf.data();

    clearDocPatches(doc);
    QVERIFY(io->universe(1)->showOnlyChannelCount() == 0);
    stub->m_lineNames = QStringList() << b2 << a1;                  // moved, here
    stub->m_openOutputs.clear();
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(doc->loadXML(r));
    io = doc->inputOutputMap();
    QVERIFY(io->universe(1)->showOnlyChannelCount() > 0);
    QVERIFY(io->universe(1)->loadHold() == false);
    const OutputPatch *las = io->outputPatch(1, 0);
    QVERIFY(las != NULL && las->isPending() && las->showOnlyHold());
    QCOMPARE(las->reconnectHeld(), InputOutputMap::networkOnlyText(b2));
    QCOMPARE(las->outputName(), b2);                                // saved back as it was
    QVERIFY(stub->mapped(1, 0) == false && stub->m_openOutputs.contains(0) == false);
    QVERIFY(io->outputPatch(0, 0)->isPatched());                    // the dimmers: as stock
    QCOMPARE(io->outputPatch(0, 0)->output(), quint32(1));
    io->retryOfflinePatches();
    QVERIFY(io->outputPatch(1, 0)->isPending());

    // N4: the rule off -- back on B2 by its name.
    Fixture *loaded = NULL;
    foreach (Fixture *fx, doc->fixtures())
        if (fx->name() == QLatin1String("Laser"))
            loaded = fx;
    QVERIFY(loaded != NULL);
    loaded->setOutputRules(Fixture::OutputRules());
    QCoreApplication::processEvents();
    const bool back = io->outputPatch(1, 0)->isPatched() && io->outputPatch(1, 0)->output() == 0;
    clearDocPatches(doc);
    QVERIFY(back);
}

/* N4 (review 17): a universe left with no Show-only fixture -- at once its
   hold is plain pending and reserves nothing, and it comes back by its name
   as stock (the interface moved meanwhile). */
void InputOutputMap_Test::n4_aHoldComesBackByNameAsStock()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString a1("Pro (S/N: A1)"), b2("Pro (S/N: B2)"), c3("Pro (S/N: C3)");
    stub->m_lineNames = QStringList() << a1 << b2 << c3;
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 1));     // B2
    iom.universe(0)->markShowOnly(14, true, 0);
    QVERIFY(iom.outputPatch(0, 0)->showOnlyHold());
    QVERIFY(iom.outputPatchRefusal(1, stub->name(), 1).isEmpty() == false);   // B2 reserved

    stub->m_lineNames = QStringList() << b2 << a1 << c3;            // B2 moved
    iom.universe(0)->markShowOnly(14, false, 0);                    // the rule off
    QVERIFY(iom.outputPatch(0, 0)->showOnlyHold() == false);        // at once
    QVERIFY(iom.outputPatchRefusal(1, stub->name(), 0).isEmpty());  // nothing reserved
    QCoreApplication::processEvents();                              // its queued retry
    const OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY(op->isPatched());
    QCOMPARE(op->output(), quint32(0));                             // by name, as stock
    QVERIFY(op->reconnectHeld().isEmpty());
}

/* Review 15 C2, under N1/N2 and F4 (review 16): a laser rig's universe on
   B2 and the dimmers on A1; marking it Show only holds it, and B2 is
   reserved for it. A1 unplugged and plugged back after B2, the rule taken
   off and put back -- (a) the retry tick at once, (b) the queued one: the
   laser universe is never open on a USB interface, and the dimmers stay
   off B2 (they reconnected onto it by number before review 16); they come
   back by number once their line is not reserved. */
void InputOutputMap_Test::review15_aLaserUniverseIsNeverOnUsbThroughASwapUnmarkAndMark()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString a1("Pro (S/N: A1)"), b2("Pro (S/N: B2)");
    for (int pass = 0; pass < 2; pass++)
    {
        stub->m_lineNames = QStringList() << a1 << b2;
        stub->m_openOutputs.clear();
        InputOutputMap iom(m_doc, 2);
        QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 0));     // dimmers -> A1
        QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 1));     // laser rig -> B2
        iom.universe(1)->markShowOnly(14, true, 0);
        QVERIFY(iom.outputPatch(1, 0)->isPending());                    // N2

        stub->m_lineNames = QStringList() << b2;                        // A1 unplugged
        stub->configure();
        stub->m_lineNames = QStringList() << b2 << a1;                  // back, after B2
        stub->configure();
        iom.retryOfflinePatches();
        QVERIFY(iom.outputPatch(1, 0)->isPending());

        iom.universe(1)->markShowOnly(14, false, 0);                    // rule off, to focus it
        if (pass == 0)
            iom.retryOfflinePatches();
        else
            QCoreApplication::processEvents();
        iom.universe(1)->markShowOnly(14, true, 0);                     // and back on
        iom.retryOfflinePatches();
        stub->configure();                                              // a rescan, nothing changed

        const OutputPatch *las = iom.outputPatch(1, 0);
        QVERIFY2(las->isPatched() == false,
                 qPrintable(QString("pass %1: the Show-only universe is open on %2, a USB interface")
                            .arg(pass).arg(las->outputName())));
        QCOMPARE(las->reconnectHeld(), InputOutputMap::networkOnlyText(b2));
        const OutputPatch *dim = iom.outputPatch(0, 0);
        QVERIFY2(!(dim->isPatched() && dim->outputName() == b2),
                 qPrintable(QString("pass %1: the dimmers are open on %2, the lasers' interface").arg(pass).arg(b2)));
        QVERIFY(dim->isPending());
        QCOMPARE(dim->reconnectHeld(), InputOutputMap::reservedText(b2, iom.universe(1)->name()));

        stub->m_lineNames = QStringList() << a1 << b2;                  // their number is A1 again
        stub->configure();
        iom.retryOfflinePatches();
        QVERIFY(iom.outputPatch(0, 0)->isPatched());                    // back, by number
        QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(0));
        QVERIFY(iom.outputPatch(1, 0)->isPending());
    }
}

/* Review 15 H1, under N3/N4: a load hold lets go as stock opens a loaded
   patch -- by its name, not by the number it was saved on. */
void InputOutputMap_Test::review15_n4AfterALoadHoldOpensByNameAsStock()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString a1("Pro (S/N: A1)"), b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << b2 << a1;                      // here: B2 first
    InputOutputMap iom(m_doc, 1);
    Universe::OfflinePatch laser;
    laser.plugin = stub->name();
    laser.uid = b2;
    laser.line = 1;                                                     // saved: B2 second
    iom.loadPatch(0, laser);
    iom.universe(0)->markShowOnly(14, true, 0);                         // the fixtures read
    iom.recheckPatchesAfterLoad();
    QVERIFY(iom.outputPatch(0, 0)->isPending());                        // N2, at the mark
    QCOMPARE(iom.outputPatch(0, 0)->pendingLine(), quint32(0));         // the line it was open on

    iom.universe(0)->markShowOnly(14, false, 0);                        // no Show-only left
    iom.retryOfflinePatches();
    const OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY(op->isPatched());
    QVERIFY2(op->outputName() == b2,
             qPrintable(QString("N4 reopened the universe on line %1 (%2), not on %3, its own interface, "
                                "where stock loads it").arg(op->output() + 1).arg(op->outputName(), b2)));
}

/* Review 15 M1, under N1/N2: a laser universe is never on USB, so
   Teaching's hold and let-go never open it there; once the rule is off it
   is back on its interface. */
void InputOutputMap_Test::review15_teachingNeverOpensALaserUniverseOnUsb()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << b2;
    InputOutputMap iom(m_doc, 1);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 0));
    iom.universe(0)->markShowOnly(14, true, 0);
    QVERIFY(iom.outputPatch(0, 0)->isPending());                        // N2

    iom.setTeachingHold(true);
    iom.setTeachingHold(false);
    iom.retryOfflinePatches();
    const OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY(op->isPatched() == false);
    QCOMPARE(op->reconnectHeld(), InputOutputMap::networkOnlyText(b2));

    iom.universe(0)->markShowOnly(14, false, 0);                        // the rule off
    iom.retryOfflinePatches();
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(0));
}

/* Review 15 M2, under N1/N3: two laser universes loaded onto USB are held
   once their fixtures are read; a laser universe's feedback line is not
   output, and stays open where it was loaded. */
void InputOutputMap_Test::review15_aLaserUniversesFeedbackLineIsAllowedItsOutputHeld()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString a1("Pro (S/N: A1)"), b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << a1 << b2;
    InputOutputMap iom(m_doc, 2);
    Universe::OfflinePatch la;
    la.plugin = stub->name();
    la.uid = a1;
    la.line = 0;
    iom.loadPatch(0, la);                                               // laser rig A -> A1
    Universe::OfflinePatch lb = la;
    lb.uid = b2;
    lb.line = 1;
    iom.loadPatch(1, lb);                                               // laser rig B -> B2
    Universe::OfflinePatch fb = lb;
    fb.tag = Universe::FeedbackPatchTag;
    iom.loadPatch(0, fb);                                               // A's feedback -> B2
    iom.universe(0)->markShowOnly(14, true, 0);                         // the fixtures read
    iom.universe(1)->markShowOnly(14, true, 0);
    QCOMPARE(iom.recheckPatchesAfterLoad(), 0);                         // held as they were marked
    QVERIFY(iom.outputPatch(0, 0)->isPending());
    QVERIFY(iom.outputPatch(1, 0)->isPending());
    QCOMPARE(iom.outputPatch(1, 0)->reconnectHeld(), InputOutputMap::networkOnlyText(b2));
    const OutputPatch *f = iom.feedbackPatch(0);
    QVERIFY(f != NULL && f->isPatched());
    QCOMPARE(f->output(), quint32(1));
}

/* Review 16 (pre-existing, outside 8fc1ceb500): Fixtures > Remap
   (FixtureRemap::accept()) hands the remapped fixtures to
   Doc::replaceFixtures(), which emits nothing -- no fixtureAdded/Changed/
   Removed -- so the OutputRuleKeeper never marks the laser where it now is.
   Its universe's USB patch stays open and the laser's channels are ordinary
   ones on it: a desk value goes straight out to the laser in Design. */
void InputOutputMap_Test::r16_fixtureRemapLeavesALaserUnheldOnUsb()
{
    Doc *doc = m_doc;
    IOPluginStub *stub = static_cast<IOPluginStub*>(doc->ioPluginCache()->plugins().at(0));
    InputOutputMap *io = doc->inputOutputMap();
    clearDocPatches(doc);
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << "Pro (S/N: A1)";
    stub->m_universe.fill(char(0));
    QVERIFY(io->setOutputPatch(0, stub->name(), QString(), 0));     // universe 1 on USB
    // The remap's target show: the laser lands on universe 1, address 15.
    Fixture *target = new Fixture(doc);
    target->setID(7);
    target->setName("Laser");
    target->setChannels(2);
    target->setUniverse(0);
    target->setAddress(14);
    target->setOutputRules(Fixture::ShowOnly);
    QVERIFY(doc->replaceFixtures(QList<Fixture *>() << target));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    Universe *uni = io->universe(0);
    const bool marked = uni->showOnlyChannelCount() > 0;
    const bool open = io->outputPatch(0, 0) != NULL && io->outputPatch(0, 0)->isPatched();
    uni->write(14, 255, true);                                      // a desk channel, in Design
    const QByteArray postGM = uni->postGMValues()->mid(0, uni->usedChannels());
    uni->dumpOutput(postGM, true);
    const int wire = uchar(stub->m_universe.at(14));
    delete target;
    clearDocPatches(doc);
    stub->m_universe.fill(char(0));
    QVERIFY2(marked, "after a remap the Show-only laser is not Show only on its universe");
    QVERIFY2(open == false, "after a remap the Show-only laser's universe is still open on USB (N1)");
    QVERIFY2(wire == 0, qPrintable(QString("the remapped laser went out on USB at %1 in Design").arg(wire)));
}

/* Review 16: an ordinary universe whose USB interface was unplugged when
   the show was saved (outputName() "None", line 2 -- stock writes the same)
   and is still away when it is opened: a dangling patch, stock. A fixture
   made Show only there holds it (N2) on the name it was opened with -- none
   -- so it is neither patched nor pending: lost, saved as nothing, and N4
   never brings it back once the rule is off and the interface is back. */
void InputOutputMap_Test::r16_holdingADanglingPatchKeepsIt()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString a1("Pro (S/N: A1)"), b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << a1;                        // B2 not plugged in
    InputOutputMap iom(m_doc, 1);
    Universe::OfflinePatch p;
    p.plugin = stub->name();
    p.uid = KOutputNone;
    p.line = 1;
    iom.loadPatch(0, p);
    QCOMPARE(iom.outputPatchesCount(0), 1);
    iom.universe(0)->markShowOnly(14, true, 0);                     // a fogger made Show only
    const OutputPatch *op = iom.outputPatch(0, 0);
    const bool kept = op->isPatched() || op->isPending();
    const QList<Universe::OfflinePatch> saved = iom.universe(0)->outputPatchDescriptions();
    const quint32 savedLine = saved.isEmpty() ? UINT_MAX : saved.first().line;

    iom.universe(0)->markShowOnly(14, false, 0);                    // the rule off
    QCoreApplication::processEvents();
    stub->m_lineNames = QStringList() << a1 << b2;                  // B2 back
    stub->configure();
    iom.retryOfflinePatches();
    const bool back = op->isPatched() && op->output() == 1;
    QVERIFY2(kept, "N2 held a dangling patch on an empty name: neither patched nor pending");
    QVERIFY2(savedLine == 1, qPrintable(QString("the held patch saves as line %1, not its line 2").arg(savedLine)));
    QVERIFY2(back, "N4: the rule off and B2 back, the universe is not on B2 again (stuck)");
}

/* Review 16, N2 on paths that mark one fixture at a time: two lasers moved
   onto a USB universe in one Doc::patchFixtures() (the Inventory's Patch,
   a move of several). The first one's mark holds the line at once -- its
   closing frame, the last the interface keeps sending, has the second
   laser's channels at whatever was there, not at safe. */
void InputOutputMap_Test::r16_patchingTwoLasersOntoUsbClosesWithBothSafe()
{
    Doc *doc = m_doc;
    IOPluginStub *stub = static_cast<IOPluginStub*>(doc->ioPluginCache()->plugins().at(0));
    InputOutputMap *io = doc->inputOutputMap();
    clearDocPatches(doc);
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << "Pro (S/N: A1)";
    stub->m_universe.fill(char(0));
    stub->m_async = true;
    QVERIFY(io->setOutputPatch(0, stub->name(), QString(), 0));     // universe 1 on USB
    QList<Fixture *> lasers;
    for (int k = 0; k < 2; k++)
    {
        Fixture *fx = new Fixture(doc);
        fx->setName(QString("Laser %1").arg(k + 1));
        fx->setChannels(2);
        fx->setUniverse(1);                                         // universe 2: no output
        fx->setAddress(quint32(10 * k));
        fx->setOutputRules(Fixture::ShowOnly);
        QVERIFY(doc->addFixture(fx));
        lasers << fx;
    }
    Universe *uni = io->universe(0);
    uni->write(30, 255, true);          // what was on 31-32 before (a desk channel, a fixture moved off)
    uni->write(31, 255, true);
    QList<Doc::FixturePatch> moves;
    moves << Doc::FixturePatch{ lasers.at(0)->id(), 0, 14 } << Doc::FixturePatch{ lasers.at(1)->id(), 0, 30 };
    QString why;
    const bool patched = doc->patchFixtures(moves, &why);
    const bool held = io->outputPatch(0, 0) != NULL && io->outputPatch(0, 0)->isPending();
    QList<int> wire;
    for (int c : { 14, 15, 30, 31 })
        wire << uchar(stub->m_universe.at(c));
    stub->m_async = false;
    clearDocPatches(doc);
    stub->m_universe.fill(char(0));
    QVERIFY2(patched, qPrintable(why));
    QVERIFY(held);
    QCOMPARE(wire, QList<int>() << 0 << 0 << 0 << 0);
}

/* Review 16, design (S-H3 removed): the laser rig's universe is held off B2
   (N2) -- the lasers are still cabled to B2. A1 unplugged: the dimmers'
   universe reconnects by number, as stock, onto line 1 = B2 now, and its
   values reach the lasers' channels with no Show-only rule on them. */
void InputOutputMap_Test::r16_noOrdinaryUniverseRenumbersOntoAHeldLaserInterface()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    stub->m_universe.fill(char(0));
    const QString a1("Pro (S/N: A1)"), b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << a1 << b2;
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 0));     // dimmers -> A1
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 1));     // laser rig -> B2
    iom.universe(1)->markShowOnly(14, true, 0);
    QVERIFY(iom.outputPatch(1, 0)->isPending());                    // N2

    stub->m_lineNames = QStringList() << b2;                        // A1 unplugged (cable bump)
    stub->configure();
    Universe *dim = iom.universe(0);
    dim->write(14, 255, true);                                      // a dimmer at full
    const QByteArray postGM = dim->postGMValues()->mid(0, dim->usedChannels());
    dim->dumpOutput(postGM, true);
    const OutputPatch *op = iom.outputPatch(0, 0);
    const bool onLaserInterface = op->isPatched() && op->outputName() == b2;
    const int wire = uchar(stub->m_universe.at(14));
    stub->m_universe.fill(char(0));
    QVERIFY2(onLaserInterface == false,
             qPrintable(QString("the dimmers' universe is open on %1, the interface the laser rig was held "
                                "off; the laser's channel 15 gets %2 there").arg(b2).arg(wire)));
}

/* F3 (review 16): a held patch with no line name -- dangling, or saved
   with its interface away ("None", a line number) -- is held by its plugin
   and line: said as "line N of <plugin>", never "None"; N3 on load keeps it
   pending by that number, saved with it, and N4 brings it back there. */
void InputOutputMap_Test::f3_aHoldWithNoNameWaitsOnItsLineNumber()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << "Pro (S/N: A1)";
    InputOutputMap iom(m_doc, 1);
    iom.universe(0)->markShowOnly(14, true, 0);                     // known before the patch
    Universe::OfflinePatch p;
    p.plugin = stub->name();
    p.uid = KOutputNone;
    p.line = 2;
    iom.loadPatch(0, p);                                            // N3
    QCOMPARE(iom.outputPatchesCount(0), 1);
    const OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY(op->isPending() && op->showOnlyHold() && op->pendingByNumber());
    const QString where = QString("line 3 of %1").arg(stub->name());
    QCOMPARE(op->deviceText(), where);
    QCOMPARE(op->reconnectHeld(), InputOutputMap::networkOnlyText(where));
    QVERIFY(op->reconnectHeld().contains(KOutputNone) == false);
    QCOMPARE(iom.universe(0)->outputPatchDescriptions().value(0).line, quint32(2));

    iom.universe(0)->markShowOnly(14, false, 0);                    // the rule off
    iom.retryOfflinePatches();
    QVERIFY(iom.outputPatch(0, 0)->isPending());                    // line 3 not here yet
    stub->m_lineNames = QStringList() << "Pro (S/N: A1)" << "Pro (S/N: B2)" << "Pro (S/N: C3)";
    iom.retryOfflinePatches();
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(2));          // by its number
}

/* F4 (review 16): the interface a universe was held off is reserved for
   it. A hand patch of another universe onto it is refused, said (every
   line of an identical name too); InputOutputMap::outputPatchRefusal()
   says so before anything changes (the I/O tab: no undo step). Patched
   elsewhere, the held universe frees it. */
void InputOutputMap_Test::f4_aHandPatchOntoAHeldLaserInterfaceIsRefused()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString e("uDMX");
    stub->m_lineNames = QStringList() << e << e << "Pro (S/N: C3)";
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 1));     // the lasers' universe -> uDMX #2
    iom.universe(0)->markShowOnly(14, true, 0);
    QVERIFY(iom.outputPatch(0, 0)->isPending());
    const QString why = InputOutputMap::reservedText(e, iom.universe(0)->name());
    QCOMPARE(why, QString("Kept off uDMX: it is cabled to %1's lasers, which need a network output. "
                          "Remove the USB output from %1 to free it.").arg(iom.universe(0)->name()));

    for (quint32 line : { 0u, 1u })                                 // either uDMX: nobody can tell
    {
        QCOMPARE(iom.outputPatchRefusal(1, stub->name(), line), why);
        QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), line) == false);
        QCOMPARE(iom.lastPatchRefusal(), why);
        QCOMPARE(iom.outputPatchesCount(1), 0);
    }
    QVERIFY(iom.outputPatchRefusal(1, stub->name(), 2).isEmpty());  // C3 is free
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 2));

    // The lasers' universe patched elsewhere (a network output): uDMX free again.
    stub->m_linesAreHardware = false;
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 2));
    stub->m_linesAreHardware = true;
    QVERIFY(iom.outputPatchRefusal(1, stub->name(), 0).isEmpty());
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 0));
}

/* F4 on load: a show's other universe saved on the interface a laser
   universe is held off is not opened there -- kept pending, said, the
   retry tick neither -- and opens as stock once the laser universe comes
   back (N4) and the reservation ends. */
void InputOutputMap_Test::f4_aLoadOntoAHeldLaserInterfaceIsKeptPending()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString a1("Pro (S/N: A1)"), b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << a1 << b2;
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 1));     // lasers -> B2
    iom.universe(0)->markShowOnly(14, true, 0);
    QVERIFY(iom.outputPatch(0, 0)->isPending());

    Universe::OfflinePatch other;
    other.plugin = stub->name();
    other.uid = b2;
    other.line = 1;
    iom.loadPatch(1, other);
    QVERIFY(iom.outputPatch(1, 0)->isPending());
    QCOMPARE(iom.outputPatch(1, 0)->reconnectHeld(), InputOutputMap::reservedText(b2, iom.universe(0)->name()));
    iom.retryOfflinePatches();
    QVERIFY(iom.outputPatch(1, 0)->isPending());

    iom.universe(0)->markShowOnly(14, false, 0);                    // N4: the lasers' universe back
    iom.retryOfflinePatches();
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(1));
    // The reservation ended: the show's own patch opens as stock does (by name).
    QVERIFY(iom.outputPatch(1, 0)->isPatched());
    QCOMPARE(iom.outputPatch(1, 0)->output(), quint32(1));
}


/* Review 17 (79af4289e9, "a hold on a shared line sends no closing frame"):
   a fogger's universe and another universe on one USB line (stock allows
   it). The fogger runs at full as an ordinary channel; the Show-only rule
   is put on it. N2 holds its universe but, the line being shared, sends no
   closing frame: the interface (an Enttec Pro keeps its buffer and replaces
   only the bytes of each frame it is given, as this stub does) keeps the
   fogger at full -- and nothing of its universe (Blackout, the e-stop)
   reaches it again. Before 79af4289e9 the closing frame went out. */
void InputOutputMap_Test::r17_aSharedLineHoldLeavesTheFoggerOnTheWire()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    stub->m_universe.fill(char(0));
    const QString a1("Pro (S/N: A1)"), b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << a1 << b2;
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 1));     // the fogger's universe -> B2
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 1));     // house lights 1-3 -> B2 too
    Universe *fog = iom.universe(0);
    Universe *house = iom.universe(1);
    house->write(2, 100, true);
    house->dumpOutput(house->postGMValues()->mid(0, house->usedChannels()), true);
    fog->write(14, 255, true);                                      // the fogger at full
    fog->dumpOutput(fog->postGMValues()->mid(0, fog->usedChannels()), true);
    QCOMPARE(uchar(stub->m_universe.at(512 + 14)), uchar(255));

    fog->markShowOnly(14, true, 0);                                 // the rule on: safe is 0
    QVERIFY(iom.outputPatch(0, 0)->isPending());                    // N2 held it
    house->write(2, 120, true);                                     // the house universe goes on
    house->dumpOutput(house->postGMValues()->mid(0, house->usedChannels()), true);
    fog->sendHeld();                                                // the e-stop's send
    const int wire = uchar(stub->m_universe.at(512 + 14));
    stub->m_universe.fill(char(0));
    QVERIFY2(wire == 0, qPrintable(QString("the Show-only fogger is left at %1 on %2 after N2 "
                                           "held its universe (no closing frame on a shared line)")
                                   .arg(wire).arg(b2)));
}

/* Review 17, F4 on a shared line: the lasers' universe and the dimmers'
   both on B2. Marking holds the lasers' (N2); a rescan (any USB plug)
   keeps the dimmers off B2, reserved (F4). The rule taken off, the lasers'
   universe comes back on B2 (N4) -- and the dimmers', held "never onto a
   line another patch has", never do: an ordinary universe stuck for good,
   still saying it is kept off for lasers that are no longer there. */
void InputOutputMap_Test::r17_anOrdinaryUniverseKeptOffASharedLineNeverComesBack()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString a1("Pro (S/N: A1)"), b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << a1 << b2;
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 1));     // lasers -> B2
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 1));     // dimmers -> B2 too
    iom.universe(0)->markShowOnly(14, true, 0);
    QVERIFY(iom.outputPatch(0, 0)->isPending());                    // N2
    stub->configure();                                              // a rescan
    QVERIFY(iom.outputPatch(1, 0)->isPending());                    // F4, by design

    iom.universe(0)->markShowOnly(14, false, 0);                    // the rule off: no laser left
    QCoreApplication::processEvents();
    iom.retryOfflinePatches();
    iom.retryOfflinePatches();
    QVERIFY(iom.outputPatch(0, 0)->isPatched());                    // N4: back on B2
    const OutputPatch *dim = iom.outputPatch(1, 0);
    QVERIFY2(dim->isPatched() && dim->output() == 1,
             qPrintable(QString("the dimmers' universe is stuck off B2 with no laser in the show: %1")
                        .arg(dim->reconnectHeld())));
}

/* Review 17, F4 after a failed N4: two identical uDMX (no serial); the
   lasers' universe and another share the first. The rule off, N4 can't
   reopen it (its line has another patch) -- it waits, still a Show-only
   hold, so it keeps reserving every "uDMX" line. With no Show-only fixture
   left in the show, a third universe can't be patched onto the free second
   uDMX: refused, "cabled to Universe 1's lasers", for as long as the other
   universe stays on the first. */
void InputOutputMap_Test::r17_aFailedN4KeepsReservingWithNoLaserLeft()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString e("uDMX");
    stub->m_lineNames = QStringList() << e << e;
    InputOutputMap iom(m_doc, 3);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 0));     // lasers -> uDMX #1
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 0));     // another universe on it too
    iom.universe(0)->markShowOnly(14, true, 0);
    QVERIFY(iom.outputPatch(0, 0)->isPending());                    // N2
    iom.universe(0)->markShowOnly(14, false, 0);                    // the rule off
    QCoreApplication::processEvents();
    iom.retryOfflinePatches();
    /* Review 18 (decision B): it shared uDMX #1 before the hold, so it comes
       back there by number with the other (it used to wait, "line used"). */
    QVERIFY(iom.outputPatch(0, 0)->isPatched() && iom.outputPatch(0, 0)->output() == 0);
    QCOMPARE(iom.universe(0)->showOnlyChannelCount(), 0);

    const QString refusal = iom.outputPatchRefusal(2, stub->name(), 1);
    const bool patched = iom.setOutputPatch(2, stub->name(), QString(), 1);   // the free uDMX #2
    QVERIFY2(refusal.isEmpty() && patched,
             qPrintable(QString("no laser in the show, yet uDMX #2 is refused: %1").arg(iom.lastPatchRefusal())));
}

/* F5 (review 16): only Art-Net, sACN (E1.31) and WLED may send to a
   universe with Show-only fixtures. Loopback, OSC and a plugin nobody
   knows are refused like USB, and a patch already on one is held when a
   fixture there is made Show only; its interface is reserved as USB's is. */
void InputOutputMap_Test::f5_onlyArtNetSacnAndWledMaySendToShowOnly()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_networkDmx = false;                                     // not a network DMX output
    for (const QString &name : { QString("Loopback"), QString("Some Plugin") })
    {
        stub->m_nameOverride = name;
        stub->m_lineNames = QStringList() << QString("%1 1").arg(name) << QString("%1 2").arg(name);
        InputOutputMap iom(m_doc, 2);
        iom.universe(0)->markShowOnly(14, true, 0);
        QVERIFY2(iom.setOutputPatch(0, name, QString(), 0) == false, qPrintable(name));
        QCOMPARE(iom.lastPatchRefusal(), InputOutputMap::networkOnlyText(QString("%1 1").arg(name)));
        QCOMPARE(iom.outputPatchesCount(0), 0);

        QVERIFY(iom.setOutputPatch(1, name, QString(), 1));         // an ordinary universe: as stock
        iom.universe(1)->markShowOnly(14, true, 0);                 // made Show only: held (N2)
        QVERIFY2(iom.outputPatch(1, 0)->isPending(), qPrintable(name));
        QVERIFY(iom.setOutputPatch(0, name, QString(), 1, true));    // feedback: allowed
    }
    stub->m_nameOverride.clear();
    stub->m_networkDmx = true;
    stub->m_lineNames.clear();
    InputOutputMap iom(m_doc, 1);
    iom.universe(0)->markShowOnly(14, true, 0);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 0));     // a network DMX output
}

/* Review 17, finding 1: a shared line is held alike at runtime (a fixture
   made Show only) and on load (the check once the fixtures are read): the
   Show-only universe's patch HeldShowOnly, the other's HeldReserved, the
   line closed. */
void InputOutputMap_Test::r17n_aSharedLineIsHeldAlikeAtRuntimeAndOnLoad()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << "Pro (S/N: A1)" << b2;
    struct State { int laserKind, otherKind; QString laserWhy, otherWhy; bool open; };
    auto state = [stub](InputOutputMap &iom) {
        const OutputPatch *l = iom.outputPatch(0, 0), *o = iom.outputPatch(1, 0);
        return State{ l->isPending() ? int(l->holdKind()) : -1, o->isPending() ? int(o->holdKind()) : -1,
                      l->reconnectHeld(), o->reconnectHeld(), stub->m_openOutputs.contains(1) };
    };

    stub->m_openOutputs.clear();
    InputOutputMap run(m_doc, 2);                                   // at runtime
    QVERIFY(run.setOutputPatch(0, stub->name(), QString(), 1));
    QVERIFY(run.setOutputPatch(1, stub->name(), QString(), 1));
    run.universe(0)->markShowOnly(14, true, 0);
    const State a = state(run);

    stub->m_openOutputs.clear();
    InputOutputMap load(m_doc, 2);                                  // on load
    load.setLoadHold(true);
    Universe::OfflinePatch p;
    p.plugin = stub->name();
    p.uid = b2;
    p.line = 1;
    load.loadPatch(0, p);
    load.loadPatch(1, p);
    QVERIFY(load.outputPatch(0, 0)->isPatched() && load.outputPatch(1, 0)->isPatched());
    load.universe(0)->markShowOnly(14, true, 0);                    // the fixtures read
    load.recheckPatchesAfterLoad();
    load.setLoadHold(false);
    const State b = state(load);

    QCOMPARE(a.laserKind, int(OutputPatch::HeldShowOnly));
    QCOMPARE(a.otherKind, int(OutputPatch::HeldReserved));
    QCOMPARE(b.laserKind, a.laserKind);
    QCOMPARE(b.otherKind, a.otherKind);
    QCOMPARE(b.laserWhy, a.laserWhy);
    QCOMPARE(b.otherWhy, a.otherWhy);
    QVERIFY(a.open == false && b.open == false);
}

/* Show only, scope (after review 20): there is no loopback clause. A
   universe with Show-only fixtures may send Art-Net or sACN to this
   computer -- a visualiser -- on the loopback line, to 127.0.0.1 or to one
   of its own addresses: only the plugin counts (N1, isNetworkDmxOutput()).
   One aimed here and then made Show only is not held, nor is one re-aimed
   here; its frames go there. */
void InputOutputMap_Test::scope_aShowOnlyUniverseMaySendToAVisualiserHere()
{
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");
    {
        InputOutputMap iom(m_doc, 2);
        Universe *fog = iom.universe(0);
        fog->markShowOnly(14, true, 0);
        QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 0));         // the loopback line
        QVERIFY(iom.lastPatchRefusal().isEmpty());
        QVERIFY(iom.outputPatchRefusal(0, "ArtNet", 0).isEmpty());
        QMap<QString, QVariant> here;
        here.insert("outputIP", "127.0.0.1");                           // a visualiser on this computer
        QVERIFY2(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 0, here),
                 qPrintable(iom.lastPatchRefusal()));
        QVERIFY(iom.outputPatch(0, 0)->isPatched());
        stub->m_lastByTarget.clear();
        fog->write(14, 255, true);
        fog->dumpOutput(fog->postGMValues()->mid(0, fog->usedChannels()), true);
        QVERIFY(stub->m_lastByTarget.contains("127.0.0.1/1"));         // sent there

        iom.outputPatch(0, 0)->setPluginParameter("outputIP", "10.4.12.7");   // re-aimed: its own address
        QVERIFY(iom.outputPatch(0, 0)->isPatched());
        QCOMPARE(iom.outputPatch(0, 0)->holdKind(), OutputPatch::NotHeld);

        // An ordinary universe aimed here, then made Show only: not held (N2 is by plugin).
        QVERIFY(iom.setOutputPatch(1, "ArtNet", QString(), 1, false, 0, here));
        iom.universe(1)->markShowOnly(14, true, 0);
        QVERIFY(iom.outputPatch(1, 0)->isPatched());
        QCOMPARE(iom.outputPatch(1, 0)->holdKind(), OutputPatch::NotHeld);
        iom.retryOfflinePatches();
        QVERIFY(iom.outputPatch(0, 0)->isPatched() && iom.outputPatch(1, 0)->isPatched());
    }
    {
        namedStub(m_doc, "E1.31", QStringList() << "10.4.12.7");    // sACN unicast to this computer
        InputOutputMap iom(m_doc, 1);
        iom.universe(0)->markShowOnly(14, true, 0);
        QMap<QString, QVariant> aim;
        aim.insert("multicast", 0);
        aim.insert("ucastIP", "127.0.0.1");
        QVERIFY(iom.setOutputPatch(0, "E1.31", QString(), 0, false, 0, aim));
        QVERIFY(iom.outputPatch(0, 0)->isPatched());
    }
    stub->m_lastByTarget.clear();
    stub->m_nameOverride.clear();
}

/* Review 17, finding 5 and the lows: what the held patches say. A patch
   held off a reserved interface lists its reason in the dangling list (not
   "missing interface"), and the reserved text says how to free it; a hold
   by number reads "line N of <plugin>" there and in Connections, never
   "None"; an unknown line reads "an unknown line of <plugin>"; a load kept
   off a reserved interface loses that reason when the reservation ends. */
void InputOutputMap_Test::r17n_heldPatchesSayWhy()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString a1("Pro (S/N: A1)"), b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << a1 << b2;
    InputOutputMap iom(m_doc, 3);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 1));     // lasers -> B2
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 1));     // another universe on B2
    iom.universe(0)->markShowOnly(14, true, 0);
    const QString reserved = InputOutputMap::reservedText(b2, iom.universe(0)->name());
    QVERIFY(reserved.contains(QString("Remove the USB output from %1 to free it").arg(iom.universe(0)->name())));
    bool listed = false;
    foreach (const InputOutputMap::DanglingPatch &d, iom.danglingOutputPatches())
        if (d.universe == 1)
        {
            listed = true;
            QCOMPARE(d.heldReason, reserved);
        }
    QVERIFY(listed);

    Universe::OfflinePatch none;                                    // a hold by number
    none.plugin = stub->name();
    none.uid = KOutputNone;
    none.line = 4;
    iom.universe(2)->markShowOnly(14, true, 0);
    iom.loadPatch(2, none);
    const QString where = QString("line 5 of %1").arg(stub->name());
    foreach (const InputOutputMap::DanglingPatch &d, iom.danglingOutputPatches())
        if (d.universe == 2)
            QCOMPARE(d.missingInterface, where);
    QCOMPARE(OutputPatch::deviceText(stub, UINT_MAX, QString()),
             QString("an unknown line of %1").arg(stub->name()));

    // A load kept off B2, B2 then unplugged and the lasers' rule taken off.
    InputOutputMap other(m_doc, 2);
    QVERIFY(other.setOutputPatch(0, stub->name(), QString(), 1));
    other.universe(0)->markShowOnly(14, true, 0);
    Universe::OfflinePatch p;
    p.plugin = stub->name();
    p.uid = b2;
    p.line = 1;
    other.loadPatch(1, p);
    QCOMPARE(other.outputPatch(1, 0)->reconnectHeld(), InputOutputMap::reservedText(b2, other.universe(0)->name()));
    stub->m_lineNames = QStringList() << a1;
    other.universe(0)->markShowOnly(14, false, 0);
    other.retryOfflinePatches();
    QVERIFY(other.outputPatch(1, 0)->isPending());
    QVERIFY2(other.outputPatch(1, 0)->reconnectHeld().isEmpty(), qPrintable(other.outputPatch(1, 0)->reconnectHeld()));
}

/* Review 18, finding A (rewritten when the loopback clause went): a
   Show-only universe held off a line another universe also sends on --
   here a plugin that is not a network DMX output (N2) -- gets its closing
   frame, flushed, and is closed there; never only forgotten, which would
   leave the fogger at full on that line. The other universe on it is held
   (F4, forgotten). */
void InputOutputMap_Test::r18_aSharedLineHoldSendsTheClosingFrame()
{
    IOPluginStub *stub = namedStub(m_doc, "OSC", QStringList() << "OSC 1" << "OSC 2");
    stub->m_networkDmx = false;                                     // not Art-Net, sACN or WLED
    stub->m_universe.fill(char(0));
    stub->m_forgotten.clear();
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, "OSC", QString(), 0));            // the fogger's universe
    QVERIFY(iom.setOutputPatch(1, "OSC", QString(), 0));            // another universe there too
    Universe *fog = iom.universe(0);
    fog->write(14, 255, true);
    fog->dumpOutput(fog->postGMValues()->mid(0, fog->usedChannels()), true);
    QCOMPARE(uchar(stub->m_universe.at(14)), uchar(255));

    fog->markShowOnly(14, true, 0);                                 // the rule on: safe is 0
    QVERIFY(iom.outputPatch(0, 0)->isPending());                    // held (N2: not a network output)
    QCOMPARE(iom.outputPatch(1, 0)->holdKind(), OutputPatch::HeldReserved);
    const int wire = uchar(stub->m_universe.at(14));
    const bool onlyForgotten = stub->m_forgotten.contains(qMakePair(quint32(0), quint32(0)));
    const bool stillMapped = stub->mapped(0, 0);
    stub->m_universe.fill(char(0));
    stub->m_forgotten.clear();
    QVERIFY2(wire == 0 && onlyForgotten == false && stillMapped == false,
             qPrintable(QString("the held Show-only universe is left at %1 on its shared line and %2")
                        .arg(wire).arg(onlyForgotten ? "only forgotten" : stillMapped ? "still open" : "closed")));
}

/* Review 18, finding B: two identical uDMX (no serial); the lasers'
   universe and another share the first, as stock allows. The rule taken
   off, the other universe returns first (phase 0, sharedLine) and then the
   former laser universe -- now an ordinary one -- never may: "a name other
   lines carry too is not taken while another patch is on it". It waits
   for good on the very line it shared before the hold. */
void InputOutputMap_Test::r18_aFormerLaserOnASharedIdenticalUsbLineComesBack()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString e("uDMX");
    stub->m_lineNames = QStringList() << e << e;
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, stub->name(), QString(), 0));     // lasers -> uDMX #1
    QVERIFY(iom.setOutputPatch(1, stub->name(), QString(), 0));     // another universe on it too
    iom.universe(0)->markShowOnly(14, true, 0);
    QVERIFY(iom.outputPatch(0, 0)->isPending());                    // N2
    iom.universe(0)->markShowOnly(14, false, 0);                    // the rule off: no laser left
    QCoreApplication::processEvents();
    for (int i = 0; i < 5; i++)                                     // retry ticks, a rescan
        iom.retryOfflinePatches();
    stub->configure();
    QCoreApplication::processEvents();
    QVERIFY(iom.outputPatch(1, 0)->isPatched());
    const OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY2(op->isPatched() && op->output() == 0,
             qPrintable(QString("an ordinary universe (no Show-only left) stays off the uDMX it shared, "
                                "for good: '%1'").arg(op->reconnectHeld())));
}

/* Review 18, decision A (rewritten when the loopback clause went): no rule
   holds a network output any more -- a Show-only universe on an Art-Net
   line it shares with another stays open, both sending -- but a hold of a
   network patch (OutputPatch::hold()) closes only that universe's output
   (Art-Net, sACN and WLED close per universe), its closing frame first,
   never only forgotten, and the others on the line keep sending. */
void InputOutputMap_Test::r18n_aNetworkHoldClosesOnlyThatUniverse()
{
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");
    stub->m_forgotten.clear();
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1));                   // the fogger's universe
    QVERIFY(iom.setOutputPatch(1, "ArtNet", QString(), 1));                   // others on that NIC
    iom.universe(0)->markShowOnly(14, true, 0);
    QVERIFY(iom.outputPatch(0, 0)->isPatched());                    // a network output: not held
    QCOMPARE(iom.outputPatch(0, 0)->holdKind(), OutputPatch::NotHeld);

    iom.outputPatch(0, 0)->hold(OutputPatch::HeldShowOnly, QStringLiteral("held"), true, true);
    QVERIFY(iom.outputPatch(0, 0)->isPending());
    QVERIFY(stub->m_forgotten.isEmpty());                           // closed, not forgotten
    QVERIFY(stub->mapped(0, 1) == false);
    QVERIFY(iom.outputPatch(1, 0)->isPatched());                    // the other universe: untouched
    QVERIFY(stub->mapped(1, 1));
    QCOMPARE(iom.outputPatch(1, 0)->holdKind(), OutputPatch::NotHeld);
}

/* Review 19, finding B1 (high, regression of decision B): a show saved with
   the lasers' universe and another on "uDMX" as line 2, opened where the
   uDMX is line 1 (H5: by name, the first) and line 2 is another device. N3
   holds the lasers' universe waiting on its SAVED number (loadedLine) with
   sharedLine true; the rule taken off, decision B brings it back "by
   number" -- onto line 2, a device it never was on (another universe's),
   instead of the uDMX it shared. Before review 18 it came back by name. */
void InputOutputMap_Test::r19_aFormerLaserLoadedByNameComesBackOnTheLineItShared()
{
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_linesAreHardware = true;
    const QString udmx("uDMX"), b2("Pro (S/N: B2)");
    stub->m_lineNames = QStringList() << udmx << b2;
    InputOutputMap iom(m_doc, 3);
    iom.setLoadHold(true);
    Universe::OfflinePatch p;
    p.plugin = stub->name();
    p.uid = udmx;
    p.line = 1;                                                     // saved: the uDMX was line 2 there
    iom.loadPatch(0, p);                                            // lasers
    iom.loadPatch(1, p);                                            // another universe on the uDMX too
    Universe::OfflinePatch q = p;
    q.uid = b2;
    q.line = 1;
    iom.loadPatch(2, q);                                            // universe 3 on the B2
    QVERIFY(iom.outputPatch(0, 0)->isPatched() && iom.outputPatch(0, 0)->output() == 0);   // H5: by name
    QVERIFY(iom.outputPatch(1, 0)->isPatched() && iom.outputPatch(1, 0)->output() == 0);
    QVERIFY(iom.outputPatch(2, 0)->isPatched() && iom.outputPatch(2, 0)->output() == 1);
    iom.universe(0)->markShowOnly(14, true, 0);                     // the fixtures read
    iom.recheckPatchesAfterLoad();
    iom.setLoadHold(false);
    QVERIFY(iom.outputPatch(0, 0)->isPending());                    // N3

    iom.universe(0)->markShowOnly(14, false, 0);                    // the rule off: no laser left
    QCoreApplication::processEvents();
    for (int i = 0; i < 3; i++)
        iom.retryOfflinePatches();
    const OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY2(op->isPatched() && op->output() == 0,
             qPrintable(QString("the former laser universe came back on line %1 (%2), not on the uDMX it "
                                "shared (line 1); universe 3 is on line %3")
                        .arg(op->isPatched() ? int(op->output()) + 1 : -1).arg(op->outputName())
                        .arg(int(iom.outputPatch(2, 0)->output()) + 1)));
}

/* Review 19, finding A1 (rewritten when the loopback clause went): a held
   patch removed while another open output of the same universe uses the
   line it was held off. ~OutputPatch() forgets the universe on that line
   -- the very entry the open patch uses (the plugin keys by universe), so
   it would send nothing while it reads as patched. Here an ordinary
   universe kept off a line a laser universe shares (F4, HeldReserved),
   given a second output on that line (straight on the universe: by hand,
   F4 refuses it), then the held one removed. */
void InputOutputMap_Test::r19_removingAHeldPatchStopsTheUniversesOpenOne()
{
    IOPluginStub *stub = namedStub(m_doc, "OSC", QStringList() << "OSC 1" << "OSC 2");
    stub->m_networkDmx = false;                                     // not a network DMX output
    stub->m_forgotten.clear();
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setOutputPatch(0, "OSC", QString(), 1));            // the lasers' universe
    QVERIFY(iom.setOutputPatch(1, "OSC", QString(), 1, false, 0));  // another universe there too
    iom.universe(0)->markShowOnly(14, true, 0);                     // N2: universe 2 kept off (F4)
    QVERIFY(iom.outputPatch(1, 0)->isPending());
    QCOMPARE(iom.outputPatch(1, 0)->holdKind(), OutputPatch::HeldReserved);
    QVERIFY(iom.universe(1)->setOutputPatch(stub, 1, 1));           // a second output on that line
    QVERIFY(iom.outputPatch(1, 1)->isPatched());
    QVERIFY(stub->mapped(1, 1));
    stub->m_forgotten.clear();
    QVERIFY(iom.setOutputPatch(1, QString(), QString(), QLCIOPlugin::invalidLine(), false, 0));   // the held one removed
    const OutputPatch *op = iom.outputPatch(1, 0);
    QVERIFY(op != NULL && op->isPatched());
    const bool forgotten = stub->m_forgotten.contains(qMakePair(quint32(1), quint32(1)));
    stub->m_forgotten.clear();
    QVERIFY2(forgotten == false && stub->mapped(1, 1),
             "the open output of universe 2 on line 2 was forgotten by the plugin when its held sibling was removed");
}

/* Review 20, finding 3 (high, ordinary DMX): A1 guards a HELD patch's
   destructor, but an open patch's ~OutputPatch() still closeLine()s
   (line, universe) -- the very entry another open leg of the universe on
   that line uses. The remaining leg reads as patched and sends nothing
   (Art-Net/E1.31 drop a universe they no longer have). */
void InputOutputMap_Test::r20_removingOneOfTwoOpenLegsStopsTheOther()
{
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");
    mockMachine(QList<NetworkRoute::LocalAddress>()
                    << localAddr("lo0", "127.0.0.1", 8)
                    << localAddr("en0", "10.4.12.7", 24),
                QMap<QString, QString>());
    InputOutputMap iom(m_doc, 1);
    QMap<QString, QVariant> a, b;
    a.insert("outputIP", "10.4.12.99");
    b.insert("outputIP", "10.4.12.98");
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 0, a));
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 1, b));
    QVERIFY(iom.outputPatch(0, 0)->isPatched() && iom.outputPatch(0, 1)->isPatched());
    QVERIFY(iom.setOutputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine(), false, 1));   // one leg removed
    const OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY(op != NULL && op->isPatched());
    QVERIFY2(stub->mapped(0, 1),
             "removing one open leg closed (line 2, universe 1) under the other open leg: it reads patched, sends nothing");
}

/* Review 21 (independent), finding 2: the scope commit reverted the
   Art-Net / E1.31 unSetParameter() overrides because PatchUndo sent closing
   frames to broadcast / Port-Address 0:0:0. It still does: restoreOne()
   clears every parameter, re-sets the same line (closeOutput() then
   openOutput(): the plugin's default target, broadcast on the universe's
   own Port-Address) and only then sets the captured target, one parameter
   at a time -- the first is a retarget, and its closing frame goes to that
   default. The stub keys a frame by the outputIP the plugin has as it goes
   out; "default" is none: broadcast. */
void InputOutputMap_Test::r21_undoOfAReAimSendsTheClosingFrameToBroadcast()
{
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "10.4.12.7");
    InputOutputMap iom(m_doc, 1);
    Universe *fog = iom.universe(0);
    fog->markShowOnly(14, true, 0);
    QMap<QString, QVariant> node;
    node.insert("outputIP", "10.4.12.5");
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 0, false, 0, node));
    fog->write(0, 200, true);
    fog->dumpOutput(fog->postGMValues()->mid(0, fog->usedChannels()), true);
    iom.patchUndo()->capture(QList<quint32>() << 0, QStringLiteral("re-aim"));
    stub->m_lastByTarget.clear();
    iom.outputPatch(0, 0)->setPluginParameter("outputIP", "10.4.12.6");   // re-aimed
    QVERIFY(stub->m_lastByTarget.contains("10.4.12.5/0"));                  // its closing frame: the old node
    stub->m_lastByTarget.clear();
    QVERIFY(iom.patchUndo()->undo());
    const QStringList got = stub->m_lastByTarget.keys();
    stub->m_lastByTarget.clear();
    QCOMPARE(iom.outputPatch(0, 0)->getPluginParameters().value("outputIP").toString(), QString("10.4.12.5"));
    QVERIFY2(got.contains("default/0") == false,
             qPrintable("undo of a re-aim sent a frame to broadcast (the plugin's default target): " + got.join(", ")));
}

/* Review 21 (independent), finding 3: review 20 keeps (line, universe) open
   for the leg left when its sibling on the same line is removed -- but the
   plugin keeps one target per universe, the last one set (the removed
   leg's), and nothing gives the remaining leg its own back. It reads
   10.4.12.99 and sends to 10.4.12.98, the node just removed. */
void InputOutputMap_Test::r21_theLegLeftAfterItsSiblingIsRemovedSendsToItsOwnNode()
{
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");
    InputOutputMap iom(m_doc, 1);
    QMap<QString, QVariant> a, b;
    a.insert("outputIP", "10.4.12.99");
    b.insert("outputIP", "10.4.12.98");
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 0, a));
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 1, b));
    QVERIFY(iom.setOutputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine(), false, 1));   // leg 2 removed
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    Universe *u = iom.universe(0);
    u->write(0, 77, true);
    stub->m_lastByTarget.clear();
    u->dumpOutput(u->postGMValues()->mid(0, u->usedChannels()), true);
    const QStringList got = stub->m_lastByTarget.keys();
    stub->m_lastByTarget.clear();
    QCOMPARE(iom.outputPatch(0, 0)->getPluginParameters().value("outputIP").toString(), QString("10.4.12.99"));
    QVERIFY2(got.contains("10.4.12.99/1") && got.contains("10.4.12.98/1") == false,
             qPrintable("the leg left (10.4.12.99) sends to: " + got.join(", ")));
}

/* Review 21 (independent), a check (expected green): SiblingOnLine on the
   open close paths never leaves (line, universe) open with no patch on it
   -- removing both legs, moving both, setPending() on both, and the
   universe deleted with both. */
void InputOutputMap_Test::r21_siblingLegsNeverLeaveALineOpen()
{
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "127.0.0.1" << "10.4.12.7");
    {
        InputOutputMap iom(m_doc, 1);
        QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 0));
        QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 1));
        QVERIFY(iom.setOutputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine(), false, 0));
        QVERIFY(stub->mapped(0, 1));
        QVERIFY(iom.setOutputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine(), false, 0));
        QVERIFY2(stub->mapped(0, 1) == false, "both legs removed: line 2 still open for universe 1");
    }
    {
        InputOutputMap iom(m_doc, 1);
        QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 0));
        QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 1));
        QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 0, false, 0));    // leg 1 moved to line 1
        QVERIFY(iom.universe(0)->setOutputPatchPending(stub, "10.4.12.7", 1, 1));   // leg 2 pending
        QVERIFY(iom.outputPatch(0, 1)->isPending());
        QVERIFY2(stub->m_openOutputs.contains(1) == false, "moved and pending: line 2 still open for universe 1");
    }
    {
        InputOutputMap iom(m_doc, 1);
        QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 0));
        QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 1));
    }
    QVERIFY2(stub->mapped(0, 1) == false, "the universe deleted with two legs on line 2: still open");
}

/* Review of c54477406 (independent), V1: a venue switch
   (VenueRig -> InputOutputMap::replaceOutputPatches() -> loadPatch()) opens
   the leg with setOutputPatch() and no parameters -- the plugin's default
   target, broadcast on the universe's own Port-Address -- and only then sets
   the saved target one parameter at a time. The first is a retarget: its
   closing frame goes to the default. Same pattern restoreOne() had. */
void InputOutputMap_Test::r21r_venueSwitchSendsNoClosingFrameToTheDefault()
{
    mockMachine(QList<NetworkRoute::LocalAddress>() << localAddr("en0", "10.4.12.7", 24),
                QMap<QString, QString>{ { "10.4.12.", "10.4.12.7" } });
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "10.4.12.7");
    InputOutputMap iom(m_doc, 1);
    Universe *fog = iom.universe(0);
    fog->markShowOnly(14, true, 0);
    QMap<QString, QVariant> node;
    node.insert("outputIP", "10.4.12.5");
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 0, false, 0, node));
    fog->write(0, 200, true);
    fog->dumpOutput(fog->postGMValues()->mid(0, fog->usedChannels()), true);
    Universe::OfflinePatch venue;
    venue.tag = Universe::OutputPatchTag;
    venue.plugin = "ArtNet";
    venue.uid = "10.4.12.7";
    venue.line = 0;
    venue.params.insert("outputIP", "10.4.12.6");
    venue.params.insert("outputUni", 3);
    stub->m_lastByTarget.clear();
    iom.replaceOutputPatches(0, QList<Universe::OfflinePatch>() << venue);
    const QStringList got = stub->m_lastByTarget.keys();
    stub->m_lastByTarget.clear();
    QVERIFY(iom.outputPatch(0, 0) != NULL && iom.outputPatch(0, 0)->isPatched());
    QCOMPARE(iom.outputPatch(0, 0)->getPluginParameters().value("outputIP").toString(), QString("10.4.12.6"));
    QVERIFY2(got.contains("default/0") == false,
             qPrintable("a venue switch sent a frame to broadcast (the plugin's default target): " + got.join(", ")));
}

/* Review of c54477406 (independent), V2: an Art-Net feedback patch on the
   universe's own output line shares the plugin's one entry for that
   universe. Clearing the feedback target (ConnectionsTree::retargetFeedback,
   the Connection editor with no feedback address) is now
   setPluginParameters({}, {outputIP, outputUni}): it unsets the keys the
   PLUGIN has (the output's), and Art-Net's unSetParameter() now puts the
   controller on broadcast at once -- the output leg leaves its node. */
void InputOutputMap_Test::r21r_clearingTheFeedbackTargetKeepsTheOutputOnItsNode()
{
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "10.4.12.7");
    InputOutputMap iom(m_doc, 1);
    QMap<QString, QVariant> node;
    node.insert("outputIP", "10.4.12.5");
    node.insert("outputUni", 16);
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 0, false, 0, node));
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 0, true, 0));       // feedback, same line, no target
    OutputPatch *fb = iom.feedbackPatch(0);
    QVERIFY(fb != NULL && fb->isPatched());
    fb->setPluginParameters({}, { QStringLiteral("outputIP"), QStringLiteral("outputUni") });
    Universe *u = iom.universe(0);
    u->write(0, 77, true);
    stub->m_lastByTarget.clear();
    u->dumpOutput(u->postGMValues()->mid(0, u->usedChannels()), true);
    const QStringList got = stub->m_lastByTarget.keys();
    stub->m_lastByTarget.clear();
    QVERIFY2(iom.outputPatch(0, 0)->getPluginParameters().value("outputIP").toString() == QString("10.4.12.5"),
             qPrintable("clearing the feedback target cleared the OUTPUT's: it reads "
                        + iom.outputPatch(0, 0)->getPluginParameters().value("outputIP").toString()));
    QVERIFY2(got.contains("10.4.12.5/0") && got.contains("default/0") == false,
             qPrintable("the output leg (10.4.12.5) sends to: " + got.join(", ")));
}

/* Review of c54477406 (independent), V3 (pre-existing, the feedback
   target): the feedback patch is not in the Universe's patch list, so
   SiblingOnLine never sees the output leg on its line: removing the
   feedback closes (line, universe) under the open output leg. */
void InputOutputMap_Test::r21r_removingFeedbackOnTheOutputsLineKeepsTheOutputOpen()
{
    IOPluginStub *stub = namedStub(m_doc, "ArtNet", QStringList() << "10.4.12.7");
    InputOutputMap iom(m_doc, 1);
    QMap<QString, QVariant> node;
    node.insert("outputIP", "10.4.12.5");
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 0, false, 0, node));
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 0, true, 0));
    QVERIFY(iom.setOutputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine(), true, 0));   // feedback removed
    QVERIFY(iom.outputPatch(0, 0)->isPatched());
    QVERIFY2(stub->mapped(0, 0),
             "removing the feedback patch closed (line 1, universe 1) under the open output leg");
}

/* Review of c54477406 (independent), V4: PatchUndo does not round-trip a
   pending leg. captureOne() records it as (plugin, invalid line, its
   cached target); restoreOne() calls setOutputPatch(..., invalid line) --
   which DELETES the leg -- then applies the pending leg's target to the
   leg that shifted into its index (another node's leg, on another line),
   and appends a new leg for that one. */
void InputOutputMap_Test::r21r_undoKeepsAPendingLegAndTheOthersTarget()
{
    namedStub(m_doc, "ArtNet", QStringList() << "10.4.12.7" << "10.4.13.7");
    IOPluginStub *stub = static_cast<IOPluginStub*>(m_doc->ioPluginCache()->plugins().at(0));
    InputOutputMap iom(m_doc, 1);
    QMap<QString, QVariant> a, b;
    a.insert("outputIP", "10.4.12.50");
    b.insert("outputIP", "10.4.13.60");
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 0, false, 0, a));
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 1, b));
    // Leg 1's interface goes away (another computer, a NIC unplugged): pending.
    QVERIFY(iom.universe(0)->setOutputPatchPending(stub, "10.4.12.7", 0, 0));
    QVERIFY(iom.outputPatch(0, 0)->isPending());
    iom.patchUndo()->capture(QList<quint32>() << 0, QStringLiteral("edit"));
    QVERIFY(iom.patchUndo()->undo());
    QCOMPARE(iom.outputPatchesCount(0), 2);
    QVERIFY2(iom.outputPatch(0, 0)->isPending(),
             qPrintable(QString("undo: the pending leg is now %1 on line %2")
                        .arg(iom.outputPatch(0, 0)->isPatched() ? "open" : "gone")
                        .arg(iom.outputPatch(0, 0)->output())));
    QCOMPARE(iom.outputPatch(0, 1)->output(), quint32(1));
    QCOMPARE(iom.outputPatch(0, 1)->getPluginParameters().value("outputIP").toString(), QString("10.4.13.60"));
}

/* Review of c54477406 (independent), V6 (pre-existing; what undo and the
   save read): QLCIOPlugin keeps ONE output line and one parameter map per
   universe. A universe with two Art-Net legs on two interfaces (show VLAN
   and a second NIC) reads the first leg's parameters as {} -- the map is
   the second line's -- so the show saves the first leg without its target,
   and PatchUndo captures {} for it and, on undo, clears its cached target
   (Art-Net's unSetParameter() now puts that line on broadcast at once). */
void InputOutputMap_Test::r21r_twoLegsOnTwoLinesEachKeepTheirTarget()
{
    namedStub(m_doc, "ArtNet", QStringList() << "10.4.12.7" << "10.4.13.7");
    InputOutputMap iom(m_doc, 1);
    QMap<QString, QVariant> a, b;
    a.insert("outputIP", "10.4.12.50");
    b.insert("outputIP", "10.4.13.60");
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 0, false, 0, a));
    QVERIFY(iom.setOutputPatch(0, "ArtNet", QString(), 1, false, 1, b));
    QCOMPARE(iom.outputPatch(0, 1)->getPluginParameters().value("outputIP").toString(), QString("10.4.13.60"));
    QVERIFY2(iom.outputPatch(0, 0)->getPluginParameters().value("outputIP").toString() == QString("10.4.12.50"),
             qPrintable("leg 1 (line 1) reads outputIP '"
                        + iom.outputPatch(0, 0)->getPluginParameters().value("outputIP").toString()
                        + "': the show saves it, and undo restores it, without its node"));
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

/*****************************************************************************
 * Controller slots: one input, any of several devices
 *****************************************************************************/

namespace
{
    const QString kXbox = QStringLiteral("Microsoft Xbox Wireless Controller (11-22)");
    const QString kDs4 = QStringLiteral("Sony DualShock 4 Wireless Controller (33-44)");

    /** The standard order (the stock profiles' layout), as a line's roles. */
    QMap<quint32, QString> standardRoles()
    {
        QMap<quint32, QString> roles;
        const QStringList std = InputOutputMap::standardChannelRoles();
        for (int i = 0; i < std.count(); ++i)
            if (std.at(i).isEmpty() == false)
                roles.insert(quint32(i), std.at(i));
        return roles;
    }

    /** A pad that lists its controls in its own order (a PlayStation pad
     *  read raw): Cross/Circle and Square/Triangle elsewhere. */
    QMap<quint32, QString> ownOrderRoles()
    {
        QMap<quint32, QString> roles = standardRoles();
        roles[6] = "b"; roles[7] = "a";     // A <-> B
        roles[8] = "y"; roles[9] = "x";     // X <-> Y
        return roles;
    }

    Universe::InputSlotDevice slotDevice(const QString &plugin, const QString &uid)
    {
        Universe::InputSlotDevice d;
        d.plugin = plugin;
        d.uid = uid;
        return d;
    }
}

void InputOutputMap_Test::slotUsesTheFirstConnectedDevice()
{
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << "Keyboard" << kXbox);
    stub->m_lineDeviceKind[1] = "xbox";
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setInputPatch(1, stub->name(), QString(), 1));
    iom.inputPatch(1)->setHidProfileName("Microsoft Xbox Controller");

    // Away from home: the DualShock 4 is remembered for the same input.
    QSignalSpy changed(&iom, SIGNAL(inputSlotChanged(quint32)));
    QVERIFY(iom.addInputSlotDevice(1, stub->name(), kDs4));
    QVERIFY(changed.count() >= 1);
    QCOMPARE(iom.inputSlot(1).count(), 2);
    QCOMPARE(iom.inputSlot(1).at(0).uid, kXbox);
    QCOMPARE(iom.inputSlot(1).at(0).hidProfile, QString("Microsoft Xbox Controller"));
    QCOMPARE(iom.inputSlot(1).at(1).uid, kDs4);
    QCOMPARE(iom.inputSlotLive(1), 0);
    // The same device twice is not added.
    QVERIFY(iom.addInputSlotDevice(1, stub->name(), kDs4) == false);

    QSignalSpy back(&iom, SIGNAL(inputDeviceConnected(quint32,QString,QString)));
    QSignalSpy gone(&iom, SIGNAL(inputDeviceDisconnected(quint32,QString,QString)));

    // The Xbox pad is off, the DS4 on: the DS4 drives universe 2.
    rescanned(stub, QStringList() << "Keyboard" << kDs4);
    QCOMPARE(iom.inputSlotLive(1), 1);
    QVERIFY(iom.inputPatch(1) != NULL);
    QCOMPARE(iom.inputPatch(1)->inputUID(), kDs4);
    QCOMPARE(iom.inputPatch(1)->input(), quint32(1));
    QCOMPARE(back.count(), 1);
    QCOMPARE(back.at(0).at(2).toString(), kDs4);
    QCOMPARE(gone.count(), 0);      // switched, not lost
    // The Xbox pad's own profile stays with its entry.
    QCOMPARE(iom.inputSlot(1).at(0).hidProfile, QString("Microsoft Xbox Controller"));

    // Both on: the first in the list wins.
    rescanned(stub, QStringList() << kDs4 << "Keyboard" << kXbox);
    QCOMPARE(iom.inputSlotLive(1), 0);
    QCOMPARE(iom.inputPatch(1)->inputUID(), kXbox);
    QCOMPARE(iom.inputPatch(1)->input(), quint32(2));
    QCOMPARE(iom.inputPatch(1)->hidProfileName(), QString("Microsoft Xbox Controller"));
    QCOMPARE(back.count(), 2);

    // Its line moves (a device before it went): still it, on its new line.
    rescanned(stub, QStringList() << "Keyboard" << kXbox);
    QCOMPARE(iom.inputSlotLive(1), 0);
    QCOMPARE(iom.inputPatch(1)->input(), quint32(1));
    QCOMPARE(back.count(), 2);

    // Its input reaches the universe.
    QSignalSpy values(&iom, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)));
    stub->emitValueChanged(UINT_MAX, 1, 7, 255);
    iom.flushInputs();
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.at(0).at(0).toUInt(), quint32(1));
}

void InputOutputMap_Test::slotNoneConnectedKeepsTheFirstOffline()
{
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << "Keyboard" << kXbox);
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setInputPatch(1, stub->name(), QString(), 1));
    QVERIFY(iom.addInputSlotDevice(1, stub->name(), kDs4));
    rescanned(stub, QStringList() << "Keyboard" << kDs4);
    QCOMPARE(iom.inputSlotLive(1), 1);

    QSignalSpy gone(&iom, SIGNAL(inputDeviceDisconnected(quint32,QString,QString)));
    rescanned(stub, QStringList() << "Keyboard");
    QCOMPARE(iom.inputSlotLive(1), -1);
    QVERIFY(iom.inputPatch(1) == NULL);
    QCOMPARE(gone.count(), 1);
    QCOMPARE(gone.at(0).at(2).toString(), kDs4);   // the one that went
    // Kept as its first device's patch, as a lone device's input is.
    QCOMPARE(iom.universe(1)->offlinePatches().count(), 1);
    QCOMPARE(iom.universe(1)->offlinePatches().first().uid, kXbox);
    // The retry tick does not say it again, nor attach anything.
    QCOMPARE(iom.retryOfflinePatches(), 0);
    iom.applyInputSlots();
    QCOMPARE(gone.count(), 1);
    QCOMPARE(iom.universe(1)->offlinePatches().count(), 1);

    // Any of them back: picked up.
    rescanned(stub, QStringList() << kDs4);
    QCOMPARE(iom.inputSlotLive(1), 1);
    QCOMPARE(iom.inputPatch(1)->inputUID(), kDs4);
    QVERIFY(iom.universe(1)->offlinePatches().isEmpty());
}

void InputOutputMap_Test::slotSavesAndLoads()
{
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << "Keyboard" << kDs4);
    InputOutputMap iom(m_doc, 2);
    QList<Universe::InputSlotDevice> devs;
    devs << slotDevice(stub->name(), kXbox) << slotDevice(stub->name(), kDs4);
    devs[0].hidProfile = "Microsoft Xbox Controller";
    devs[1].hidProfile = "Sony PlayStation Controller";
    devs[1].params.insert("mode", "pad");
    Universe::InputSlotDevice any;
    any.plugin = stub->name();
    any.anyGamepad = true;
    devs << any;
    iom.setInputSlot(1, devs);
    QCOMPARE(iom.inputSlotLive(1), 1);      // the DS4 is here

    const QString saved = saveIOMap(iom);
    // Older builds open the first device, from the <Input> they know.
    QVERIFY2(saved.contains(QString("<Input Plugin=\"%1\" UID=\"%2\"").arg(stub->name(), kXbox)),
             qPrintable(saved));
    QVERIFY2(saved.contains("<InputSlot"), qPrintable(saved));
    QVERIFY(saved.contains("Any=\"True\""));
    QVERIFY(saved.indexOf(kXbox) < saved.indexOf(kDs4));

    // Read back on a machine with only the Xbox pad (one plugin for both
    // maps here: what the first left on the stub is not the show's).
    stub->unSetParameter(1, 1, QLCIOPlugin::Input, "mode");
    stub->m_lineNames = QStringList() << kXbox;
    InputOutputMap other(m_doc, 2);
    QVERIFY(loadIOMap(other, saved));
    const QList<Universe::InputSlotDevice> loaded = other.inputSlot(1);
    QCOMPARE(loaded.count(), 3);
    QCOMPARE(loaded.at(0).uid, kXbox);
    QCOMPARE(loaded.at(1).uid, kDs4);
    QCOMPARE(loaded.at(1).hidProfile, QString("Sony PlayStation Controller"));
    QCOMPARE(loaded.at(1).params.value("mode").toString(), QString("pad"));
    QVERIFY(loaded.at(2).anyGamepad);
    QCOMPARE(other.inputSlotLive(1), 0);
    QCOMPARE(other.inputPatch(1)->inputUID(), kXbox);
    QCOMPARE(other.inputPatch(1)->hidProfileName(), QString("Microsoft Xbox Controller"));
    QCOMPARE(other.universe(1)->inputSlotRoles(), iom.universe(1)->inputSlotRoles());

    // Saved again: the same.
    QCOMPARE(saveIOMap(other), saved);

    // An older build's view: <InputSlot> skipped, the first device opened.
    QString older = saved;
    const int from = older.indexOf("<InputSlot");
    const int to = older.indexOf("</InputSlot>") + int(strlen("</InputSlot>"));
    older.remove(from, to - from);
    InputOutputMap old(m_doc, 2);
    QVERIFY(loadIOMap(old, older));
    QVERIFY(old.inputSlot(1).isEmpty());
    QCOMPARE(old.inputPatch(1)->inputUID(), kXbox);
}

void InputOutputMap_Test::singleDevicePatchSavesAsBefore()
{
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << "Keyboard" << kXbox);
    InputOutputMap iom(m_doc, 2);
    const QString xml = QString(
        "<InputOutputMap>\n"
        " <Universe Name=\"Universe 1\" ID=\"0\"/>\n"
        " <Universe Name=\"Pad\" ID=\"1\">\n"
        "  <Input Plugin=\"%1\" UID=\"%2\" Line=\"1\" HIDProfile=\"Microsoft Xbox Controller\"/>\n"
        " </Universe>\n"
        "</InputOutputMap>\n").arg(stub->name(), kXbox);
    QVERIFY(loadIOMap(iom, xml));
    QVERIFY(iom.inputSlot(1).isEmpty());
    QCOMPARE(iom.inputSlotLive(1), -1);
    QCOMPARE(iom.inputPatch(1)->inputUID(), kXbox);
    QVERIFY(iom.inputPatch(1)->channelMap().isEmpty());
    const QString saved = saveIOMap(iom);
    QVERIFY(saved.contains("InputSlot") == false);

    // Another pad turning up is never taken for it, slot or not.
    rescanned(stub, QStringList() << "Keyboard" << kDs4);
    QVERIFY(iom.inputPatch(1) == NULL);
    QCOMPARE(iom.universe(1)->offlinePatches().first().uid, kXbox);
    QVERIFY(iom.inputSlot(1).isEmpty());
    QCOMPARE(saveIOMap(iom), saved);
}

void InputOutputMap_Test::slotMapsControlsByRole()
{
    // Statically: by role, the rest where it is.
    QVector<quint32> map = InputOutputMap::slotChannelMap(
        QStringList() << "lx" << "ly" << "a" << "b",
        QStringList() << "lx" << "ly" << "b" << "a" << "" << "guide");
    QCOMPARE(map, QVector<quint32>() << 0 << 1 << 3 << 2 << 4 << 5);
    // Same layout: nothing to do, nothing in the way.
    QVERIFY(InputOutputMap::slotChannelMap(InputOutputMap::standardChannelRoles(),
                                           InputOutputMap::standardChannelRoles()).isEmpty());
    QVERIFY(InputOutputMap::slotChannelMap(QStringList(), QStringList() << "a").isEmpty());

    // Live: the show was made on the Xbox pad (standard order); a pad that
    // lists its own order stands in.
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << kXbox << kDs4);
    stub->m_lineChannelRoles[0] = standardRoles();
    stub->m_lineChannelRoles[1] = ownOrderRoles();
    InputOutputMap iom(m_doc, 1);
    QVERIFY(iom.setInputPatch(0, stub->name(), QString(), 0));
    QVERIFY(iom.addInputSlotDevice(0, stub->name(), kDs4));
    QCOMPARE(iom.universe(0)->inputSlotRoles().value(6), QString("a"));
    QVERIFY(iom.inputPatch(0)->channelMap().isEmpty());     // the Xbox: as it is

    rescanned(stub, QStringList() << "Mouse" << kDs4);
    QCOMPARE(iom.inputSlotLive(0), 1);
    InputPatch *ip = iom.inputPatch(0);
    QVERIFY(ip->channelMap().isEmpty() == false);

    // Its "A" (its channel 7) fires the show's A (channel 6), and so on.
    QSignalSpy values(&iom, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)));
    stub->emitValueChanged(UINT_MAX, 1, 7, 255);
    iom.flushInputs();
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.at(0).at(1).toUInt(), quint32(6));
    values.clear();
    stub->emitValueChanged(UINT_MAX, 1, 9, 255);    // its X
    stub->emitValueChanged(UINT_MAX, 1, 0, 30);     // the left stick: where it was
    iom.flushInputs();
    QCOMPARE(values.count(), 2);
    QList<quint32> channels;
    for (const QList<QVariant> &v : std::as_const(values))
        channels << v.at(1).toUInt();
    std::sort(channels.begin(), channels.end());
    QCOMPARE(channels, QList<quint32>() << 0 << 8);
    // Asked the other way (the role of the show's channel): the pad's own.
    QCOMPARE(ip->deviceChannel(6), quint32(7));
    QCOMPARE(ip->showChannel(7), quint32(6));
    QCOMPARE(ip->deviceChannel(40), quint32(40));

    // Back to the Xbox pad: no mapping.
    rescanned(stub, QStringList() << kXbox << kDs4);
    QCOMPARE(iom.inputSlotLive(0), 0);
    QVERIFY(iom.inputPatch(0)->channelMap().isEmpty());
}

// "A universe's input from a list of game controllers": when the second pad of the slot is the
// live one, its input reaches the universe; the pad that is off, and an unrelated line, do not.
void InputOutputMap_Test::theSecondPadOfASlotDrivesTheInputAndNoOtherDoes()
{
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << "Keyboard" << kXbox);
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setInputPatch(1, stub->name(), QString(), 1));      // universe 2: the Xbox pad
    QVERIFY(iom.addInputSlotDevice(1, stub->name(), kDs4));

    // Only the DS4 is here, on line 2 behind the keyboard and a mouse.
    rescanned(stub, QStringList() << "Keyboard" << "Mouse" << kDs4);
    QCOMPARE(iom.inputSlotLive(1), 1);
    QCOMPARE(iom.inputPatch(1)->input(), quint32(2));

    QSignalSpy values(&iom, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)));
    stub->emitValueChanged(UINT_MAX, 2, 5, 200);        // the live second pad
    iom.flushInputs();
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.at(0).at(0).toUInt(), quint32(1));  // universe 2
    QCOMPARE(values.at(0).at(1).toUInt(), quint32(5));
    QCOMPARE(values.at(0).at(2).toUInt(), quint32(200));
    values.clear();

    // The mouse's line and the Xbox pad's old line 1 are not this input.
    stub->emitValueChanged(UINT_MAX, 1, 5, 99);
    stub->emitValueChanged(UINT_MAX, 0, 5, 98);
    iom.flushInputs();
    QCOMPARE(values.count(), 0);

    // The Xbox pad returns and takes the slot: the DS4's line is no longer heard.
    rescanned(stub, QStringList() << "Keyboard" << kXbox << kDs4);
    QCOMPARE(iom.inputSlotLive(1), 0);
    QCOMPARE(iom.inputPatch(1)->input(), quint32(1));
    stub->emitValueChanged(UINT_MAX, 2, 5, 77);
    iom.flushInputs();
    QCOMPARE(values.count(), 0);
    stub->emitValueChanged(UINT_MAX, 1, 5, 66);
    iom.flushInputs();
    QCOMPARE(values.count(), 1);
}

void InputOutputMap_Test::slotMappingLatency()
{
    /* What a controller slot adds to each input value: one bounds check and
       one array read, under the lock the value takes anyway. Measured here
       against the same patch with no map. */
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << kXbox << kDs4);
    stub->m_lineChannelRoles[0] = standardRoles();
    stub->m_lineChannelRoles[1] = ownOrderRoles();
    InputOutputMap iom(m_doc, 1);
    QVERIFY(iom.setInputPatch(0, stub->name(), QString(), 1));
    InputPatch *ip = iom.inputPatch(0);

    const int kEvents = 200000;
    auto run = [&]() {
        QElapsedTimer t;
        t.start();
        for (int i = 0; i < kEvents; ++i)
        {
            stub->emitValueChanged(UINT_MAX, 1, quint32(6 + (i % 8)), uchar(i & 0x7F) | 1);
            if ((i & 1023) == 1023)
                iom.flushInputs();
        }
        iom.flushInputs();
        QCoreApplication::processEvents();
        return double(t.nsecsElapsed()) / kEvents;
    };

    run();      // warm up
    double plain = 1e9, mapped = 1e9;
    for (int round = 0; round < 3; ++round)
    {
        ip->setChannelMap(QVector<quint32>());
        plain = qMin(plain, run());
        ip->setChannelMap(InputOutputMap::slotChannelMap(InputOutputMap::standardChannelRoles(),
                              InputOutputMap::channelRoles(stub, 1)));
        QVERIFY(ip->channelMap().isEmpty() == false);
        mapped = qMin(mapped, run());
    }
    qDebug("controller slot mapping: %.0f ns per input value plain, %.0f ns mapped (+%.0f ns)",
           plain, mapped, mapped - plain);
    // Generous: a few tens of nanoseconds expected; a regression is microseconds.
    QVERIFY2(mapped - plain < 1000.0, qPrintable(QString::number(mapped - plain)));
}

void InputOutputMap_Test::slotAnyGameController()
{
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << "Keyboard" << kXbox);
    stub->m_lineDeviceKind[1] = "xbox";
    InputOutputMap iom(m_doc, 2);
    QVERIFY(iom.setInputPatch(0, stub->name(), QString(), 1));     // universe 1 has the Xbox pad

    Universe::InputSlotDevice any;
    any.plugin = stub->name();
    any.anyGamepad = true;
    QList<Universe::InputSlotDevice> devs;
    devs << slotDevice(stub->name(), "Some Pad Never Seen") << any;
    iom.setInputSlot(1, devs);
    // The only pad here is universe 1's: not taken.
    QCOMPARE(iom.inputSlotLive(1), -1);
    QVERIFY(iom.inputPatch(1) == NULL);

    // A DualSense turns up: any game controller, so universe 2 has it.
    stub->m_lineDeviceKind[2] = "dualsense";
    rescanned(stub, QStringList() << "Keyboard" << kXbox << kPad);
    QCOMPARE(iom.inputSlotLive(1), 1);
    QCOMPARE(iom.inputPatch(1)->inputUID(), kPad);
    QCOMPARE(iom.inputPatch(0)->inputUID(), kXbox);
    // The keyboard is not a game controller: never taken.
    rescanned(stub, QStringList() << "Keyboard" << kXbox);
    QCOMPARE(iom.inputSlotLive(1), -1);
}

void InputOutputMap_Test::slotOfferAndAdopt()
{
    // The show was made with the Xbox pad; here there is a DS4.
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << "Keyboard");
    InputOutputMap iom(m_doc, 3);
    const QString xml = QString(
        "<InputOutputMap>\n"
        " <Universe Name=\"A\" ID=\"0\"/>\n"
        " <Universe Name=\"Pad\" ID=\"1\">\n"
        "  <Input Plugin=\"%1\" UID=\"%2\" Line=\"1\" HIDProfile=\"Microsoft Xbox Controller\"/>\n"
        " </Universe>\n"
        " <Universe Name=\"C\" ID=\"2\"/>\n"
        "</InputOutputMap>\n").arg(stub->name(), kXbox);
    QVERIFY(loadIOMap(iom, xml));
    QVERIFY(iom.inputSlotOffers().isEmpty());       // nothing to offer yet

    stub->m_lineDeviceKind[1] = "dualshock4";
    rescanned(stub, QStringList() << "Keyboard" << kDs4);
    const QList<InputOutputMap::InputSlotOffer> offers = iom.inputSlotOffers();
    QCOMPARE(offers.count(), 1);
    QCOMPARE(offers.first().universe, quint32(1));
    QCOMPARE(offers.first().device, kDs4);
    QVERIFY(iom.inputPatch(1) == NULL);             // not taken without asking

    // "Use it for universe 2 (and remember it)"
    QVERIFY(iom.addInputSlotDevice(1, offers.first().plugin, offers.first().device));
    QCOMPARE(iom.inputSlot(1).count(), 2);
    QCOMPARE(iom.inputSlot(1).at(0).uid, kXbox);
    QCOMPARE(iom.inputSlot(1).at(0).hidProfile, QString("Microsoft Xbox Controller"));
    QCOMPARE(iom.inputSlotLive(1), 1);
    QCOMPARE(iom.inputPatch(1)->inputUID(), kDs4);
    QVERIFY(iom.inputSlotOffers().isEmpty());
    // Saved: <Input> still the Xbox pad, for older builds.
    const QString saved = saveIOMap(iom);
    QVERIFY(saved.contains(QString("<Input Plugin=\"%1\" UID=\"%2\"").arg(stub->name(), kXbox)));
}

void InputOutputMap_Test::slotHandPatchDropsIt()
{
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << "Keyboard" << kXbox << kDs4);
    InputOutputMap iom(m_doc, 1);
    QVERIFY(iom.setInputPatch(0, stub->name(), QString(), 1));
    QVERIFY(iom.addInputSlotDevice(0, stub->name(), kDs4));
    QCOMPARE(iom.inputSlotLive(0), 0);

    // Patched by hand to one of its own: kept, that one live.
    QVERIFY(iom.setInputPatch(0, stub->name(), QString(), 2));
    QCOMPARE(iom.inputSlot(0).count(), 2);
    QCOMPARE(iom.inputSlotLive(0), 1);

    // To something else: the slot goes, and stays gone.
    QVERIFY(iom.setInputPatch(0, stub->name(), QString(), 0));
    QVERIFY(iom.inputSlot(0).isEmpty());
    iom.applyInputSlots();
    QCOMPARE(iom.inputPatch(0)->inputName(), QString("Keyboard"));
}

void InputOutputMap_Test::slotRemoveAndMove()
{
    IOPluginStub *stub = devicePlugin(m_doc, QStringList() << kXbox << kDs4);
    InputOutputMap iom(m_doc, 1);
    QVERIFY(iom.setInputPatch(0, stub->name(), QString(), 0));
    QVERIFY(iom.addInputSlotDevice(0, stub->name(), kDs4));
    QVERIFY(iom.addInputSlotDevice(0, stub->name(), kPad));
    QCOMPARE(iom.inputSlotLive(0), 0);

    // The DS4 first: it takes over (both are on).
    QVERIFY(iom.moveInputSlotDevice(0, 1, 0));
    QCOMPARE(iom.inputSlot(0).at(0).uid, kDs4);
    QCOMPARE(iom.inputSlotLive(0), 0);
    QCOMPARE(iom.inputPatch(0)->inputUID(), kDs4);

    // Removed: the Xbox pad (now second) takes over.
    QVERIFY(iom.removeInputSlotDevice(0, 0));
    QCOMPARE(iom.inputSlot(0).count(), 2);
    QCOMPARE(iom.inputPatch(0)->inputUID(), kXbox);
    QCOMPARE(iom.inputSlotLive(0), 0);

    // One left: an ordinary patch on it.
    QVERIFY(iom.removeInputSlotDevice(0, 1));
    QVERIFY(iom.inputSlot(0).isEmpty());
    QCOMPARE(iom.inputPatch(0)->inputUID(), kXbox);
    QVERIFY(saveIOMap(iom).contains("InputSlot") == false);

    QVERIFY(iom.moveInputSlotDevice(0, 0, 1) == false);
    QVERIFY(iom.removeInputSlotDevice(0, 0) == false);
}

void InputOutputMap_Test::stockGamepadInputProfiles()
{
    /* A pad added to a slot gets the stock input profile for its kind: one
       per kind, named as the slot asks, its channels the layout's. */
    InputOutputMap iom(m_doc, 1);
    iom.loadProfiles(QDir(INTERNAL_PROFILEDIR));
    const QStringList roles = InputOutputMap::standardChannelRoles();
    for (const char *kind : { "xbox", "dualshock4", "dualsense" })
    {
        const QString name = iom.stockInputProfile(kind);
        QVERIFY2(name.isEmpty() == false, kind);
        QLCInputProfile *p = iom.profile(name);
        QVERIFY(p != NULL);
        QCOMPARE(p->type(), QLCInputProfile::HID);
        QVERIFY(p->channels().count() >= 22);
        QCOMPARE(p->channel(0)->name(), QString("Left Stick X"));
        QCOMPARE(p->channel(0)->type(), QLCInputChannel::Slider);
        QCOMPARE(p->channel(6)->type(), QLCInputChannel::Button);
    }
    QCOMPARE(iom.profile(iom.stockInputProfile("dualsense"))->channel(12)->name(), QString("Create"));
    QCOMPARE(iom.profile(iom.stockInputProfile("dualshock4"))->channel(12)->name(), QString("Share"));
    QCOMPARE(iom.profile(iom.stockInputProfile("dualsense"))->channel(6)->name(), QString("Cross"));
    QCOMPARE(iom.profile(iom.stockInputProfile("xbox"))->channel(16)->name(), QString("Xbox"));
    QVERIFY(iom.stockInputProfile("joystick").isEmpty());
}

QTEST_GUILESS_MAIN(InputOutputMap_Test)

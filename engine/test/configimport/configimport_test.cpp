/*
  Q Light Controller Plus - Unit test
  configimport_test.cpp

  Configuration import (docs/design/AI_ASSIST.md section 5b, P2b): universes
  and I/O patches taken from another show file, with a mapping. Fixture
  .qxw in this directory (evo26.qxw: three universes -- Art-Net outputs on
  2.0.0.50, an input with a profile, an output on a plugin this machine
  lacks, a feedback). No AI anywhere in this file: the import works
  without it; the assistant only proposes the same operation.

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
#include <QXmlStreamWriter>
#include <QJsonArray>
#include <QDir>

#include "configimport_test.h"
#include "operationregistry.h"
#include "showtreecontroller.h"
#include "showtreecommand.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "iopluginstub.h"
#include "configimport.h"
#include "networkroute.h"
#include "universe.h"
#include "qlcfile.h"
#include "fixture.h"
#include "doc.h"

typedef OperationRegistry R;

namespace
{
QDir testPluginDir()
{
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    return dir;
}

} // namespace

void ConfigImport_Test::initTestCase()
{
    m_doc = new Doc(this);
    m_doc->ioPluginCache()->load(testPluginDir());
    QVERIFY(m_doc->ioPluginCache()->plugins().size() != 0);
}

void ConfigImport_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = nullptr;
}

void ConfigImport_Test::init()
{
    // The stub stands in for Art-Net (the only plugin here), on a laptop
    // with the house network only.
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_nameOverride = QStringLiteral("ArtNet");
    stub->m_lineNames = QStringList { QStringLiteral("192.168.1.5") };
    m_doc->setMode(Doc::Design);
    InputOutputMap *io = m_doc->inputOutputMap();
    io->removeAllUniverses();
    io->addUniverse();
    showRoute(QStringLiteral("none"));
}

void ConfigImport_Test::cleanup()
{
    NetworkRoute::setResolverForTest(nullptr, nullptr);
    m_doc->setMode(Doc::Design);
    for (Fixture *fx : m_doc->fixtures())
        m_doc->deleteFixture(fx->id());
    if (m_doc->showTree() != nullptr)
        m_doc->showTree()->clearUndo();
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_nameOverride.clear();
    stub->m_lineNames.clear();
}

QString ConfigImport_Test::sample(const QString &name) const
{
    return QStringLiteral(CONFIGIMPORT_SAMPLES) + QLatin1Char('/') + name;
}

QJsonObject ConfigImport_Test::args(const QJsonObject &extra) const
{
    QJsonObject a { { "file", sample() }, { "onMissingPlugin", "keep" } };
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
    {
        if (it.value().isNull())
            a.remove(it.key());
        else
            a.insert(it.key(), it.value());
    }
    return a;
}

QString ConfigImport_Test::universeXml(int index) const
{
    InputOutputMap *io = m_doc->inputOutputMap();
    QByteArray xml;
    QXmlStreamWriter w(&xml);
    io->universe(io->getUniverseID(index))->saveXML(&w);
    return QString::fromUtf8(xml);
}

// "none": the house network only; "default": 2.x leaves by the default
// route; "show": a route to 2.0.0.0/8 through the show NIC.
void ConfigImport_Test::showRoute(const QString &kind)
{
    typedef NetworkRoute::LocalAddress LA;
    QList<LA> locals;
    LA house; house.iface = "en0"; house.ip = QHostAddress("192.168.1.5"); house.prefix = 24;
    locals << house;
    if (kind == QLatin1String("show"))
    {
        LA nic; nic.iface = "en5"; nic.ip = QHostAddress("2.0.0.1"); nic.prefix = 8;
        locals << nic;
    }
    NetworkRoute::setResolverForTest(
        [locals]() { return locals; },
        [kind](const QHostAddress &target) {
            if (target.isInSubnet(QHostAddress("2.0.0.0"), 8) && kind == QLatin1String("show"))
                return QHostAddress("2.0.0.1");
            if (kind == QLatin1String("none") && !target.isInSubnet(QHostAddress("192.168.1.0"), 24)
                && !target.isInSubnet(QHostAddress("198.51.100.0"), 24))
                return QHostAddress();          // no route at all
            return QHostAddress("192.168.1.5"); // the default route
        });
}

/*****************************************************************************/

void ConfigImport_Test::readsTheSource()
{
    const ConfigImport::Source s = ConfigImport::readSource(sample());
    QVERIFY2(s.ok, qPrintable(s.error));
    QCOMPARE(s.universes.count(), 3);
    QCOMPARE(s.universes.at(0).name, QStringLiteral("Front"));
    QVERIFY(s.universes.at(1).passthrough);
    QCOMPARE(s.universes.at(1).patches.count(), 2);   // an input and an output
    const Universe::OfflinePatch &in = s.universes.at(1).patches.first();
    QCOMPARE(in.tag, Universe::InputPatchTag);
    QCOMPARE(in.profile, QStringLiteral("Launchpad X"));
    QCOMPARE(in.network, QStringLiteral("2.0.0.0/8"));
    const Universe::OfflinePatch &out = s.universes.at(0).patches.first();
    QCOMPARE(out.uid, QStringLiteral("2.0.0.50"));
    QCOMPARE(out.params.value("outputIP").toString(), QStringLiteral("2.0.0.50"));
    QCOMPARE(s.universes.at(2).patches.last().tag, Universe::FeedbackPatchTag);
    QCOMPARE(s.fixtures, 1);
    QCOMPARE(s.groups, 1);
    QCOMPARE(s.palettes, 1);
    const QString text = ConfigImport::sourceText(s);
    QVERIFY(text.contains("Universe 2 \"Truss\" (passthrough)"));
    QVERIFY(text.contains("ENTTEC Gone"));
    QVERIFY(text.contains("1 fixture(s), 1 group(s), 1 palette(s)"));
}

void ConfigImport_Test::notAShowFile()
{
    QVERIFY(!ConfigImport::readSource(sample("notashow.qxw")).ok);
    QVERIFY(!ConfigImport::readSource(sample("nope.qxw")).ok);
    QVERIFY(!ConfigImport::readSource(sample("CMakeLists.txt")).ok);
    ConfigImport::Options o;
    o.file = sample("nope.qxw");
    QVERIFY(!ConfigImport::plan(m_doc, o).ok());
}

void ConfigImport_Test::offNetworkImportSucceedsWithTheNoRouteWarning()
{
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args(), &o, &why));
    ConfigImport::Plan p = ConfigImport::plan(m_doc, o);
    QVERIFY2(p.ok(), qPrintable(p.problems.join("; ")));
    bool noRoute = false;
    for (const QString &w : p.warnings)
        noRoute = noRoute || (w.contains("2.0.0.50") && w.contains("not currently routable"));
    QVERIFY2(noRoute, qPrintable(p.warnings.join("\n")));
    const ConfigImport::Applied a = ConfigImport::apply(m_doc, o);
    QVERIFY2(a.ok, qPrintable(a.message));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(3));
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(0), QStringLiteral("Front"));
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(1), QStringLiteral("Truss"));
    QVERIFY(m_doc->inputOutputMap()->getUniversePassthrough(1));
    QVERIFY(!m_doc->inputOutputMap()->getUniversePassthrough(0));
}

void ConfigImport_Test::routeRequirementsAreCopiedIntact()
{
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args(), &o, &why));
    QVERIFY(ConfigImport::apply(m_doc, o).ok);
    // Off the show network the patch is kept, with the file's UID, network
    // and target exactly as they were: it waits for the network.
    const QString xml = universeXml(0);
    QVERIFY2(xml.contains("UID=\"2.0.0.50\""), qPrintable(xml));
    QVERIFY2(xml.contains("Network=\"2.0.0.0/8\""), qPrintable(xml));
    QVERIFY2(xml.contains("outputIP=\"2.0.0.50\""), qPrintable(xml));
    QVERIFY2(xml.contains("outputUni=\"0\""), qPrintable(xml));
    QVERIFY2(universeXml(1).contains("outputUni=\"1\""), qPrintable(universeXml(1)));
    QVERIFY(m_doc->inputOutputMap()->hasOfflinePatches());
}

void ConfigImport_Test::defaultRouteIsAProminentWarning()
{
    showRoute(QStringLiteral("default"));
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args(), &o, &why));
    const ConfigImport::Plan p = ConfigImport::plan(m_doc, o);
    QVERIFY2(p.ok(), qPrintable(p.problems.join("; ")));
    bool def = false, none = false;
    for (const QString &w : p.warnings)
    {
        def = def || (w.contains("DEFAULT route") && w.contains("2.0.0.50") && w.contains("en0"));
        none = none || w.contains("not currently routable");
    }
    QVERIFY2(def, qPrintable(p.warnings.join("\n")));
    QVERIFY(!none);
    QVERIFY(ConfigImport::apply(m_doc, o).ok);     // a warning, never a blocker
}

void ConfigImport_Test::aRealRouteWarnsNothing()
{
    showRoute(QStringLiteral("show"));
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args(), &o, &why));
    const ConfigImport::Plan p = ConfigImport::plan(m_doc, o);
    QVERIFY2(p.ok(), qPrintable(p.problems.join("; ")));
    for (const QString &w : p.warnings)
        QVERIFY2(!w.contains("route") && !w.contains("routable"), qPrintable(w));
}

void ConfigImport_Test::mappingMovesUniversesAndAddresses()
{
    // "universe 2 on 1, Art-Net destination 2.0.0.50 -> 2.0.0.60"
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args({ { "universes", QJsonArray { 2 } },
                                             { "universeMap", QJsonObject { { "2", "1" } } },
                                             { "addressMap", QJsonObject { { "2.0.0.50", "2.0.0.60" } } } }),
                                      &o, &why));
    const ConfigImport::Plan p = ConfigImport::plan(m_doc, o);
    QVERIFY2(p.ok(), qPrintable(p.problems.join("; ")));
    QVERIFY(p.text().contains("Universe 2 \"Truss\" → universe 1"));
    QVERIFY(p.text().contains("2.0.0.60"));
    QVERIFY(ConfigImport::apply(m_doc, o).ok);
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(0), QStringLiteral("Truss"));
    const QString xml = universeXml(0);
    QVERIFY2(xml.contains("outputIP=\"2.0.0.60\""), qPrintable(xml));
    QVERIFY2(xml.contains("UID=\"2.0.0.60\""), qPrintable(xml));
    QVERIFY2(!xml.contains("2.0.0.50"), qPrintable(xml));

    // Bad maps are problems, not guesses.
    QVERIFY(ConfigImport::optionsFrom(args({ { "universeMap", QJsonObject { { "9", "1" } } } }), &o, &why));
    QVERIFY(!ConfigImport::plan(m_doc, o).ok());
    QVERIFY(ConfigImport::optionsFrom(args({ { "universeMap", QJsonObject { { "1", "0" } } } }), &o, &why));
    QVERIFY(!ConfigImport::plan(m_doc, o).ok());
    QVERIFY(ConfigImport::optionsFrom(args({ { "universeMap", QJsonObject { { "1", "5" }, { "2", "5" } } } }), &o, &why));
    QVERIFY(ConfigImport::plan(m_doc, o).problems.join(" ").contains("Two universes"));
    QVERIFY(!ConfigImport::optionsFrom(args({ { "universeMap", QJsonObject { { "x", "5" } } } }), &o, &why));
}

void ConfigImport_Test::sectionsAndUniverseSubset()
{
    // Outputs of universe 1 only: our universe keeps its name, and a name of
    // ours is no collision when names aren't being imported.
    m_doc->inputOutputMap()->setUniverseName(0, QStringLiteral("Mine"));
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args({ { "universes", QJsonArray { 1 } }, { "sections", QJsonArray { "outputs" } } }),
                                      &o, &why));
    const ConfigImport::Plan p = ConfigImport::plan(m_doc, o);
    QVERIFY2(p.ok(), qPrintable(p.problems.join("; ")));
    QVERIFY(ConfigImport::apply(m_doc, o).ok);
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(0), QStringLiteral("Mine"));
    QVERIFY(universeXml(0).contains("outputIP=\"2.0.0.50\""));
    // A section that isn't one.
    QVERIFY(ConfigImport::optionsFrom(args({ { "sections", QJsonArray { "fixtures" } } }), &o, &why));
    QVERIFY(ConfigImport::plan(m_doc, o).problems.join(" ").contains("isn't a section"));
}

void ConfigImport_Test::collisionNeedsAChoice()
{
    m_doc->inputOutputMap()->setUniverseName(0, QStringLiteral("House"));
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args({ { "universes", QJsonArray { 1 } } }), &o, &why));
    ConfigImport::Plan p = ConfigImport::plan(m_doc, o);
    QVERIFY(!p.ok());
    QVERIFY2(p.problems.join(" ").contains("already has a setup")
             && p.problems.join(" ").contains("skip") && p.problems.join(" ").contains("replace")
             && p.problems.join(" ").contains("renumber"),
             qPrintable(p.problems.join("; ")));
    // Nothing applied without the choice.
    const ConfigImport::Applied a = ConfigImport::apply(m_doc, o);
    QVERIFY(!a.ok);
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(0), QStringLiteral("House"));
    QVERIFY(universeXml(0).contains("Name=\"House\""));
    // An empty universe of ours (a fresh show's) is not a collision.
    m_doc->inputOutputMap()->setUniverseName(0, QStringLiteral("Universe 1"));
    QVERIFY(ConfigImport::plan(m_doc, o).ok());
}

void ConfigImport_Test::collisionChoices_data()
{
    QTest::addColumn<QString>("choice");
    QTest::addColumn<QString>("name0");
    QTest::addColumn<int>("count");
    QTest::addColumn<QString>("name1");
    QTest::newRow("skip") << "skip" << "House" << 1 << QString();
    QTest::newRow("replace") << "replace" << "Front" << 1 << QString();
    QTest::newRow("renumber") << "renumber" << "House" << 2 << "Front";
}

void ConfigImport_Test::collisionChoices()
{
    QFETCH(QString, choice);
    QFETCH(QString, name0);
    QFETCH(int, count);
    QFETCH(QString, name1);
    m_doc->inputOutputMap()->setUniverseName(0, QStringLiteral("House"));
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args({ { "universes", QJsonArray { 1 } }, { "onCollision", choice } }), &o, &why));
    const ConfigImport::Applied a = ConfigImport::apply(m_doc, o);
    QVERIFY2(a.ok, qPrintable(a.message));
    QCOMPARE(int(m_doc->inputOutputMap()->universesCount()), count);
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(0), name0);
    if (!name1.isEmpty())
        QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(1), name1);
    if (choice == QLatin1String("skip"))
        QVERIFY(!universeXml(0).contains("2.0.0.50"));
    else
        QVERIFY(m_doc->inputOutputMap()->hasOfflinePatches());
    // And it undoes exactly.
    a.undo(true);
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(0), QStringLiteral("House"));
    QVERIFY(!universeXml(0).contains("2.0.0.50"));
    a.undo(false);
    QCOMPARE(int(m_doc->inputOutputMap()->universesCount()), count);
}

void ConfigImport_Test::missingPluginNeedsAChoice()
{
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args({ { "universes", QJsonArray { 3 } }, { "onMissingPlugin", QJsonValue::Null } }),
                                      &o, &why));
    const ConfigImport::Plan p = ConfigImport::plan(m_doc, o);
    QVERIFY(!p.ok());
    const QString problems = p.problems.join(" ");
    QVERIFY2(problems.contains("ENTTEC Gone") && problems.contains("skip") && problems.contains("keep")
             && problems.contains("pluginMap"), qPrintable(problems));
    QVERIFY(!ConfigImport::apply(m_doc, o).ok);
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
}

void ConfigImport_Test::missingPluginChoices()
{
    ConfigImport::Options o;
    QString why;
    // keep: the patch waits offline, written back as it was.
    QVERIFY(ConfigImport::optionsFrom(args({ { "universes", QJsonArray { 3 } }, { "onMissingPlugin", "keep" } }), &o, &why));
    const ConfigImport::Plan p = ConfigImport::plan(m_doc, o);
    QVERIFY2(p.ok(), qPrintable(p.problems.join("; ")));
    QVERIFY(p.warnings.join(" ").contains("kept, offline"));
    QVERIFY(ConfigImport::apply(m_doc, o).ok);
    QVERIFY2(universeXml(2).contains("Plugin=\"ENTTEC Gone\""), qPrintable(universeXml(2)));
    QVERIFY(universeXml(2).contains("DMX USB PRO"));
    // skip: left out, and the report says so.
    m_doc->inputOutputMap()->removeAllUniverses();
    m_doc->inputOutputMap()->addUniverse();
    QVERIFY(ConfigImport::optionsFrom(args({ { "universes", QJsonArray { 3 } }, { "onMissingPlugin", "skip" } }), &o, &why));
    const ConfigImport::Applied a = ConfigImport::apply(m_doc, o);
    QVERIFY(a.ok);
    QVERIFY(a.report.join("\n").contains("skipped (plugin not available)"));
    QVERIFY(!universeXml(2).contains("ENTTEC Gone"));
}

void ConfigImport_Test::pluginMapRemaps()
{
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args({ { "universes", QJsonArray { 3 } }, { "onMissingPlugin", QJsonValue::Null },
                                             { "pluginMap", QJsonObject { { "ENTTEC Gone", "ArtNet" } } } }),
                                      &o, &why));
    const ConfigImport::Plan p = ConfigImport::plan(m_doc, o);
    QVERIFY2(p.ok(), qPrintable(p.problems.join("; ")));
    QVERIFY(ConfigImport::apply(m_doc, o).ok);
    QVERIFY(!universeXml(2).contains("ENTTEC Gone"));
    QVERIFY(universeXml(2).contains("Plugin=\"ArtNet\""));
}

void ConfigImport_Test::profileMissingWarns()
{
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args({ { "universes", QJsonArray { 2 } } }), &o, &why));
    const ConfigImport::Plan p = ConfigImport::plan(m_doc, o);
    QVERIFY2(p.ok(), qPrintable(p.problems.join("; ")));
    QVERIFY(p.warnings.join("\n").contains("input profile \"Launchpad X\" isn't installed here"));
}

void ConfigImport_Test::nothingAppliedWhenAProblem()
{
    // Universe 3's plugin needs a choice; universe 1 is fine: still nothing.
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args({ { "onMissingPlugin", QJsonValue::Null } }), &o, &why));
    QVERIFY(!ConfigImport::apply(m_doc, o).ok);
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(0), QStringLiteral("Universe 1"));
    QVERIFY(!m_doc->inputOutputMap()->hasOfflinePatches());
}

void ConfigImport_Test::operateRefused()
{
    m_doc->setMode(Doc::Operate);
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args(), &o, &why));
    QVERIFY(ConfigImport::plan(m_doc, o).problems.join(" ").contains("Operate"));
    QVERIFY(!ConfigImport::apply(m_doc, o).ok);
    R reg(m_doc);
    QVERIFY(reg.check("import_config", args()).contains("Operate"));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
    m_doc->setMode(Doc::Design);
}

void ConfigImport_Test::fixturesAreNotTouched()
{
    Fixture *fx = new Fixture(m_doc);
    fx->setName("Mine");
    fx->setChannels(3);
    fx->setUniverse(0);
    fx->setAddress(0);
    QVERIFY(m_doc->addFixture(fx));
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args(), &o, &why));
    QVERIFY(ConfigImport::apply(m_doc, o).ok);
    QCOMPARE(m_doc->fixtures().count(), 1);
    QCOMPARE(m_doc->fixtures().first()->name(), QStringLiteral("Mine"));
    QVERIFY(m_doc->palettes().isEmpty());
    QVERIFY(m_doc->fixtureGroups().isEmpty());
}

/*****************************************************************************
 * Through the registry: one undo step, the command line, the assistant's tools
 *****************************************************************************/

void ConfigImport_Test::oneUndoStepThroughAPlan()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st != nullptr);
    ShowTreeCommand cmd(m_doc, st);
    R reg(m_doc, &cmd);
    const QString undoBefore = st->sharedUndoText();
    QCOMPARE(reg.check("import_config", args()), QString());
    const R::Result r = reg.runPlan({ { "import_config", args() } }, ShowTreeCommand::Context(), "AI Assist: bring in evo26");
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(3));
    QCOMPARE(st->sharedUndoText(), QStringLiteral("AI Assist: bring in evo26"));
    QVERIFY(m_doc->inputOutputMap()->hasOfflinePatches());

    QVERIFY(st->undoShared());
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(0), QStringLiteral("Universe 1"));
    QVERIFY(!m_doc->inputOutputMap()->hasOfflinePatches());
    QCOMPARE(st->sharedUndoText(), undoBefore);

    QVERIFY(st->redoShared());
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(3));
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(2), QStringLiteral("Movers"));
    QVERIFY(universeXml(0).contains("outputIP=\"2.0.0.50\""));
}

void ConfigImport_Test::failingPlanPutsEverythingBack()
{
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    R reg(m_doc, &cmd);
    const QString undoBefore = st->sharedUndoText();
    // The second step is refused when it runs (no such group).
    const R::Result r = reg.runPlan({ { "import_config", args() },
                                      { "set_intensity", QJsonObject { { "targets", QJsonArray { "Nobody" } }, { "level", 5 } } } },
                                    ShowTreeCommand::Context(), "test");
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("Nothing changed"));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(0), QStringLiteral("Universe 1"));
    QVERIFY(!m_doc->inputOutputMap()->hasOfflinePatches());
    QCOMPARE(st->sharedUndoText(), undoBefore);
    QVERIFY(!st->inBatch());
    // A plan whose import has a problem runs nothing at all.
    const R::Result q = reg.runPlan({ { "import_config", args({ { "onMissingPlugin", QJsonValue::Null } }) } },
                                    ShowTreeCommand::Context(), "test");
    QVERIFY(!q.ok);
    QVERIFY(q.message.startsWith("Step 1"));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
}

void ConfigImport_Test::commandLineFormPreviewsThenApplies()
{
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    R reg(m_doc, &cmd);
    const QString file = R::quoted(sample());
    // Without "apply" it only says what it would do.
    ShowTreeCommand::Result r = reg.runLine(QStringLiteral("import config %1 missing keep").arg(file), ShowTreeCommand::Context());
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(r.help.contains("Universe 1"));
    QVERIFY(r.help.contains("not currently routable"));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
    // A problem is shown, not applied.
    r = reg.runLine(QStringLiteral("import config %1").arg(file), ShowTreeCommand::Context());
    QVERIFY(r.ok);
    QVERIFY(r.help.contains("Problems"));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
    // With it, it is made: one undo step.
    r = reg.runLine(QStringLiteral("import config %1 universes 1,2 map 2=3 address 2.0.0.50=2.0.0.60 missing keep apply").arg(file),
                    ShowTreeCommand::Context());
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(3));
    QVERIFY(universeXml(2).contains("outputIP=\"2.0.0.60\""));
    QVERIFY(universeXml(0).contains("outputIP=\"2.0.0.60\""));
    QVERIFY(st->undoShared());
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
    // Mistakes in the words say how it goes.
    r = reg.runLine(QStringLiteral("import config %1 map 2").arg(file), ShowTreeCommand::Context());
    QVERIFY(!r.ok);
    r = reg.runLine(QStringLiteral("import config %1 collision maybe apply").arg(file), ShowTreeCommand::Context());
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("skip, replace, renumber"));
}

void ConfigImport_Test::roundTrip()
{
    R reg(m_doc);
    const QJsonObject a = args({ { "sections", QJsonArray { "universes", "outputs" } },
                                 { "universes", QJsonArray { 1, 3 } },
                                 { "universeMap", QJsonObject { { "3", "2" } } },
                                 { "addressMap", QJsonObject { { "2.0.0.50", "2.0.0.60" } } },
                                 { "pluginMap", QJsonObject { { "ENTTEC Gone", "ArtNet" } } },
                                 { "onCollision", "renumber" }, { "onMissingPlugin", "skip" } });
    const R::Operation *op = R::operation("import_config");
    QVERIFY(op != nullptr);
    const QString line = op->cliSyntax(a);
    QVERIFY(line.startsWith("import config "));
    QVERIFY(line.endsWith(" apply"));
    // What runLine would resolve.
    QJsonObject back;
    QString why;
    QCOMPARE(reg.resolveLine(line, &back, &why), QStringLiteral("import_config"));
    QVERIFY2(why.isEmpty(), qPrintable(why));
    QCOMPARE(back.value("apply").toBool(), true);
    back.remove("apply");
    QJsonObject expect = a;
    // Maps and lists come back as text/number the same way toArgs writes them.
    QCOMPARE(back.value("universes"), expect.value("universes"));
    QCOMPARE(back.value("sections"), expect.value("sections"));
    QCOMPARE(back.value("universeMap"), expect.value("universeMap"));
    QCOMPARE(back.value("addressMap"), expect.value("addressMap"));
    QCOMPARE(back.value("pluginMap"), expect.value("pluginMap"));
    QCOMPARE(back.value("onCollision"), expect.value("onCollision"));
    QCOMPARE(back.value("onMissingPlugin"), expect.value("onMissingPlugin"));
    QCOMPARE(back.value("file"), expect.value("file"));
    // Options -> args -> Options is stable.
    ConfigImport::Options o, o2;
    QVERIFY(ConfigImport::optionsFrom(a, &o, &why));
    QVERIFY(ConfigImport::optionsFrom(ConfigImport::toArgs(o), &o2, &why));
    QCOMPARE(ConfigImport::toArgs(o2), ConfigImport::toArgs(o));
}

void ConfigImport_Test::readAndPreviewAreQuestions()
{
    ShowTreeCommand cmd(m_doc, m_doc->showTree());
    R reg(m_doc, &cmd);
    const R::Operation *read = R::operation("read_config");
    const R::Operation *preview = R::operation("preview_config_import");
    QVERIFY(read != nullptr && preview != nullptr);
    QVERIFY(read->readOnly && preview->readOnly);
    QVERIFY(R::readOnlyOperations().contains(read));
    QVERIFY(R::readOnlyOperations().contains(preview));
    QVERIFY(R::changeOperations().contains(R::operation("import_config")));
    const R::Result r = reg.run("read_config", QJsonObject { { "file", sample() } });
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(r.text.contains("Universe 1 \"Front\""));
    const R::Result p = reg.run("preview_config_import", args());
    QVERIFY2(p.ok, qPrintable(p.message));
    QVERIFY(p.text.contains("Changes:"));
    QVERIFY(p.text.contains("Warnings"));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(1));
    QVERIFY(!reg.run("read_config", QJsonObject { { "file", sample("nope.qxw") } }).ok);
    // The typed form.
    const ShowTreeCommand::Result t = reg.runLine(QStringLiteral("config %1").arg(R::quoted(sample())), ShowTreeCommand::Context());
    QVERIFY(t.ok);
    QVERIFY(t.help.contains("Front"));
}

void ConfigImport_Test::worksWithoutAnyAi()
{
    // Nothing of AI Assist is linked into this test: the import is the
    // engine's, and a bare registry (no command line, no assistant) can run
    // it on its own as one operation.
    R reg(m_doc);
    ConfigImport::Options o;
    QString why;
    QVERIFY(ConfigImport::optionsFrom(args(), &o, &why));
    QVERIFY(ConfigImport::apply(m_doc, o).ok);
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), quint32(3));
}

QTEST_MAIN(ConfigImport_Test)

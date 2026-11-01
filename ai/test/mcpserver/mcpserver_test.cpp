/*
  Q Light Controller Plus - qlcconsole
  mcpserver_test.cpp

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
#include <QJsonDocument>
#include <QJsonArray>
#include <QLocalSocket>
#include <QTextBrowser>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLineEdit>
#include <QSettings>
#include <QLabel>
#include <QXmlStreamWriter>
#include <QRegularExpression>
#include <memory>

#include "mcpserver_test.h"
#include "operationregistry.h"
#include "aisettingspage.h"
#include "aicommandline.h"
#include "aisettings.h"
#include "mcpserver.h"
#include "mcpbridge.h"
#include "aiassist.h"
#include "showtreecontroller.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

namespace
{
/** The Show Tree's command line, as the view builds it. */
struct Bar
{
    QWidget bar;
    QHBoxLayout *row;
    QLineEdit *line;
    QTextBrowser pane;
    QLabel *msg;
    std::unique_ptr<AiCommandLine> cl;
    Bar()
    {
        row = new QHBoxLayout(&bar);
        line = new QLineEdit(&bar);
        row->addWidget(line);
        msg = new QLabel(&bar);
        cl.reset(new AiCommandLine(line, &pane, msg, row, nullptr));
    }
};
}

/****************************************************************************
 * Fixture
 ****************************************************************************/

void McpServer_Test::initTestCase()
{
    // Never the user's settings, never the user's socket.
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    QCoreApplication::setApplicationName(QStringLiteral("mcpserver_test"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    m_socket = QStringLiteral("qlcmcp-test-%1").arg(QCoreApplication::applicationPid());
    qputenv("QLC_MCP_SOCKET", m_socket.toLocal8Bit());
    AiAssist::setAssertOnViolation(false);
}

void McpServer_Test::init()
{
    QSettings().clear();
    m_doc = new Doc(this);
    m_ai = new AiAssist(m_doc, this);
    m_mcp = new McpServer(m_doc, this);
    m_bridge = new McpBridge(McpBridge::defaultServerName(), this);
}

void McpServer_Test::cleanup()
{
    delete m_bridge;
    m_bridge = nullptr;
    delete m_mcp;
    m_mcp = nullptr;
    delete m_ai;
    m_ai = nullptr;
    delete m_doc;
    m_doc = nullptr;
    AiResources::destroyAll();
}

void McpServer_Test::enable(bool ai, bool mcp, bool changes)
{
    AiSettings s;
    s.enabled = ai;
    s.mcpEnabled = mcp;
    s.mcpChanges = changes;
    s.save();
    m_ai->settingsChanged();
}

QJsonObject McpServer_Test::call(const QString &method, const QJsonObject &params)
{
    const int id = m_nextId++;
    QJsonObject msg { { "jsonrpc", "2.0" }, { "id", id }, { "method", method } };
    if (!params.isEmpty())
        msg.insert("params", params);
    QSignalSpy spy(m_bridge, &McpBridge::output);
    m_bridge->send(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    for (int round = 0; round < 50; ++round)
    {
        for (const QList<QVariant> &args : spy)
        {
            const QJsonObject answer = QJsonDocument::fromJson(args.first().toByteArray()).object();
            if (answer.value("id").toInt() == id && !answer.contains("method"))
                return answer;
        }
        spy.wait(100);
    }
    return QJsonObject { { "timeout", true } };
}

QString McpServer_Test::toolText(const QJsonObject &answer) const
{
    QString out;
    for (const QJsonValue &c : answer.value("result").toObject().value("content").toArray())
        out += c.toObject().value("text").toString();
    return out;
}

ShowTreeCommand::Context McpServer_Test::smallShow()
{
    auto dimmer = [this](const QString &name, quint32 address) {
        Fixture *fx = new Fixture(m_doc);
        fx->setName(name);
        fx->setChannels(4);
        fx->setUniverse(0);
        fx->setAddress(address);
        m_doc->addFixture(fx);
        return fx;
    };
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Towers");
    m_doc->addFixtureGroup(g);
    for (Fixture *fx : { dimmer("T1", 0), dimmer("T2", 10) })
        g->assignFixture(fx->id());
    QLCPalette *blue = new QLCPalette(QLCPalette::Color, m_doc);
    blue->setName("Blue");
    blue->setValue(QLCPalette::colorToString(QColor(Qt::blue), QColor(Qt::black)));
    m_doc->addPalette(blue);

    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = st->addProduction("P");
    for (const char *line : { "list \"Song\"", "cue 1 \"Verse\"", "cue 2 \"Chorus\"", "cue 1" })
    {
        const ShowTreeCommand::Result r = cmd.run(QLatin1String(line), ctx);
        if (!r.ok)
            qWarning() << line << r.message;
        ctx = r.context;
    }
    return ctx;
}

QByteArray McpServer_Test::treeXml() const
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.writeStartDocument();
    m_doc->showTree()->saveXML(&w);
    w.writeEndDocument();
    return QString::fromUtf8(out).remove(QRegularExpression(" NextId=\"\\d+\"")).toUtf8();
}

/****************************************************************************
 * Tests
 ****************************************************************************/

void McpServer_Test::offByDefault()
{
    // Nothing listens until both AI Assist and the server are on.
    QVERIFY(!m_mcp->isListening());
    QLocalSocket probe;
    probe.connectToServer(m_mcp->serverName());
    QVERIFY(!probe.waitForConnected(300));

    // The client still hears why, at once, and is never left hanging.
    const QJsonObject a = call("initialize", QJsonObject { { "protocolVersion", "2025-06-18" } });
    QCOMPARE(a.value("error").toObject().value("code").toInt(), -32000);
    QVERIFY(a.value("error").toObject().value("message").toString().contains("isn't taking MCP requests"));
    QCOMPARE(call("ping").value("result").toObject(), QJsonObject());

    // AI Assist alone isn't enough; the server alone isn't either.
    enable(true, false);
    QVERIFY(!m_mcp->isListening());
    enable(false, true);
    QVERIFY(!m_mcp->isListening());
    // And a request that got in anyway is refused.
    const QJsonObject r = m_mcp->handle(QJsonObject { { "jsonrpc", "2.0" }, { "id", 1 }, { "method", "tools/list" } });
    QVERIFY(r.value("error").toObject().value("message").toString().contains("off"));

    enable(true, true);
    QVERIFY(m_mcp->isListening());
    enable(false, true);
    QVERIFY(!m_mcp->isListening());
}

void McpServer_Test::listsTheToolsWithTheirSchemas()
{
    enable(true, true);
    QVERIFY(m_mcp->isListening());

    const QJsonObject init = call("initialize", QJsonObject { { "protocolVersion", "2025-06-18" },
                                                             { "clientInfo", QJsonObject { { "name", "claude-code" } } } });
    const QJsonObject res = init.value("result").toObject();
    QCOMPARE(res.value("protocolVersion").toString(), QStringLiteral("2025-06-18"));
    QCOMPARE(res.value("serverInfo").toObject().value("name").toString(), QStringLiteral("qlcconsole"));
    QVERIFY(res.value("capabilities").toObject().contains("tools"));
    QVERIFY(m_bridge->isConnected());
    QCOMPARE(m_mcp->connectionCount(), 1);
    // A version it doesn't know: its own newest.
    QCOMPARE(call("initialize", QJsonObject { { "protocolVersion", "1999-01-01" } })
                 .value("result").toObject().value("protocolVersion").toString(),
             McpServer::protocolVersions().last());

    const QJsonArray tools = call("tools/list").value("result").toObject().value("tools").toArray();
    QStringList names;
    for (const QJsonValue &v : tools)
    {
        const QJsonObject t = v.toObject();
        names << t.value("name").toString();
        const OperationRegistry::Operation *op = OperationRegistry::operation(t.value("name").toString());
        QVERIFY(op != nullptr);
        // The registry's own schema, unchanged: one registry, three front ends.
        QCOMPARE(t.value("inputSchema").toObject(), op->schema);
        QCOMPARE(t.value("description").toString(), op->description);
        QVERIFY(t.value("annotations").toObject().value("readOnlyHint").toBool());
    }
    QStringList readOnly;
    for (const OperationRegistry::Operation *op : OperationRegistry::readOnlyOperations())
        readOnly << op->name;
    QCOMPARE(names, readOnly);
    QVERIFY(names.contains("find_conflicts"));
    QVERIFY(names.contains("list_fixtures"));
    QVERIFY(!names.contains("command"));
    QVERIFY(!names.contains("set_look"));

    QCOMPARE(call("no/such").value("error").toObject().value("code").toInt(), -32601);
}

void McpServer_Test::readOnlyToolRuns()
{
    enable(true, true);
    for (int i = 0; i < 2; ++i)
    {
        Fixture *fx = new Fixture(m_doc);
        fx->setName(QStringLiteral("Par %1").arg(i + 1));
        fx->setChannels(4);
        fx->setUniverse(0);
        fx->setAddress(i * 10);
        QVERIFY(m_doc->addFixture(fx));
        if (i == 1)
            fx->setAddress(2);
    }
    const QJsonObject a = call("tools/call", QJsonObject { { "name", "find_conflicts" } });
    QVERIFY2(!a.value("result").toObject().value("isError").toBool(), qPrintable(toolText(a)));
    const QString text = toolText(a);
    QVERIFY2(text.contains("Par 1") && text.contains("Par 2"), qPrintable(text));

    const QString fixtures = toolText(call("tools/call", QJsonObject { { "name", "list_fixtures" },
                                                                       { "arguments", QJsonObject() } }));
    QVERIFY2(fixtures.contains("Par 2"), qPrintable(fixtures));

    // Arguments that don't fit the schema: a tool error, nothing run.
    const QJsonObject bad = call("tools/call", QJsonObject { { "name", "describe_function" },
                                                             { "arguments", QJsonObject() } });
    QVERIFY(bad.value("result").toObject().value("isError").toBool());
    // A tool that isn't one: the protocol's error.
    QCOMPARE(call("tools/call", QJsonObject { { "name", "command" },
                                              { "arguments", QJsonObject { { "line", "undo" } } } })
                 .value("error").toObject().value("code").toInt(),
             -32602);
}

void McpServer_Test::changeIsProposedNotAppliedUntilConfirmed()
{
    enable(true, true, true);
    const ShowTreeCommand::Context ctx = smallShow();
    ShowTreeController *st = m_doc->showTree();
    Bar b;
    b.cl->setSelectionProvider([ctx]() { return ctx; });
    const QByteArray before = treeXml();
    const QString undoBefore = st->sharedUndoText();

    // The change tools are offered now (never `command`).
    QStringList names;
    for (const QJsonValue &v : call("tools/list").value("result").toObject().value("tools").toArray())
        names << v.toObject().value("name").toString();
    QVERIFY(names.contains("set_intensity"));
    QVERIFY(names.contains("set_look"));
    QVERIFY(!names.contains("command"));

    call("initialize", QJsonObject { { "protocolVersion", "2025-06-18" },
                                     { "clientInfo", QJsonObject { { "name", "claude-code" } } } });
    const QJsonObject a = call("tools/call", QJsonObject {
        { "name", "set_look" },
        { "arguments", QJsonObject { { "targets", QJsonArray { "Towers" } },
                                     { "looks", QJsonArray { QJsonObject { { "attribute", "colour" }, { "value", "Blue" } } } } } } });
    QVERIFY2(!a.value("result").toObject().value("isError").toBool(), qPrintable(toolText(a)));
    QVERIFY2(toolText(a).startsWith("Proposed as step 1"), qPrintable(toolText(a)));
    const QJsonObject a2 = call("tools/call", QJsonObject {
        { "name", "set_intensity" },
        { "arguments", QJsonObject { { "targets", QJsonArray { "Towers" } }, { "level", 50 } } } });
    QVERIFY2(toolText(a2).startsWith("Proposed as step 2"), qPrintable(toolText(a2)));

    // Proposed, not applied: the show is as it was, the proposal waits in
    // the command line for the operator.
    QCOMPARE(treeXml(), before);
    QVERIFY(m_ai->hasProposal());
    QVERIFY(m_ai->isExternalProposal());
    QCOMPARE(m_ai->proposal().count(), 2);
    const QString html = b.pane.toHtml();
    QVERIFY(html.contains("Towers colour Blue"));
    QVERIFY(html.contains("Towers dimmer 50"));
    QVERIFY(html.contains("claude-code"));
    QVERIFY(html.contains("ai:apply"));

    // A bad step is refused, the proposal stays as it was.
    const QJsonObject bad = call("tools/call", QJsonObject {
        { "name", "set_intensity" },
        { "arguments", QJsonObject { { "targets", QJsonArray { "Towers" } }, { "level", 140 } } } });
    QVERIFY(bad.value("result").toObject().value("isError").toBool());
    QCOMPARE(m_ai->proposal().count(), 2);

    // The operator applies: both steps, one undo step.
    QVERIFY(b.cl->applyProposal());
    QVERIFY(!m_ai->hasProposal());
    const ShowTree::Cue *c = st->cue(ctx.cue);
    QCOMPARE(c->layers.count(), 1);
    QVERIFY(c->layers.first().look(ShowTree::KeyColour) != nullptr);
    QVERIFY(c->layers.first().look(ShowTree::KeyDimmer) != nullptr);
    QVERIFY(st->sharedUndoText().startsWith("AI Assist: claude-code"));
    QVERIFY(st->undoShared());
    QCOMPARE(treeXml(), before);
    QCOMPARE(st->sharedUndoText(), undoBefore);

    // Discarded instead: nothing changes.
    call("tools/call", QJsonObject {
        { "name", "set_intensity" },
        { "arguments", QJsonObject { { "targets", QJsonArray { "Towers" } }, { "level", 20 } } } });
    QVERIFY(m_ai->isExternalProposal());
    QVERIFY(b.cl->cancel());
    QVERIFY(!m_ai->hasProposal());
    QCOMPARE(treeXml(), before);
}

void McpServer_Test::changesOffMeansNoChangeTools()
{
    enable(true, true, false);
    const ShowTreeCommand::Context ctx = smallShow();
    Q_UNUSED(ctx)
    const QByteArray before = treeXml();
    const QJsonObject a = call("tools/call", QJsonObject {
        { "name", "set_intensity" },
        { "arguments", QJsonObject { { "targets", QJsonArray { "Towers" } }, { "level", 50 } } } });
    QCOMPARE(a.value("error").toObject().value("code").toInt(), -32602);
    QVERIFY(!m_ai->hasProposal());
    QCOMPARE(treeXml(), before);
    // Straight to the assistant, too.
    QString why;
    QCOMPARE(m_ai->proposeExternal("set_intensity", QJsonObject { { "targets", QJsonArray { "Towers" } }, { "level", 50 } },
                                   "x", &why), 0);
    QVERIFY(why.contains("off"));
}

void McpServer_Test::refusedInOperate()
{
    enable(true, true, true);
    smallShow();
    QVERIFY(!call("tools/call", QJsonObject { { "name", "list_fixtures" } }).value("result").toObject().value("isError").toBool());
    QCOMPARE(m_mcp->connectionCount(), 1);

    m_doc->setMode(Doc::Operate);
    // Gone at once: no socket, no connection, nothing for the audit to find.
    QVERIFY(!m_mcp->isListening());
    QCOMPARE(m_mcp->connectionCount(), 0);
    for (const QString &live : AiResources::live())
        QVERIFY2(!live.startsWith("MCP"), qPrintable(live));
    QTRY_VERIFY(!m_bridge->isConnected());
    QLocalSocket probe;
    probe.connectToServer(m_mcp->serverName());
    QVERIFY(!probe.waitForConnected(300));

    // The client is told why and nothing runs.
    const QJsonObject a = call("tools/call", QJsonObject {
        { "name", "set_intensity" },
        { "arguments", QJsonObject { { "targets", QJsonArray { "Towers" } }, { "level", 50 } } } });
    QVERIFY2(a.value("error").toObject().value("message").toString().contains("Operate"),
             qPrintable(QJsonDocument(a).toJson()));
    QVERIFY(!m_ai->hasProposal());
    // Anything that reached the server in Operate is refused.
    const QJsonObject r = m_mcp->handle(QJsonObject { { "jsonrpc", "2.0" }, { "id", 9 }, { "method", "tools/call" },
                                                      { "params", QJsonObject { { "name", "list_fixtures" } } } });
    QVERIFY(r.value("error").toObject().value("message").toString().contains("Operate"));
    QString why;
    QCOMPARE(m_ai->proposeExternal("set_intensity", QJsonObject { { "targets", QJsonArray { "Towers" } }, { "level", 50 } },
                                   "x", &why), 0);
    QVERIFY(why.contains("Operate"));

    // Back in Design it listens again; the bridge reconnects on its own.
    m_doc->setMode(Doc::Design);
    QVERIFY(m_mcp->isListening());
    const QJsonObject again = call("tools/call", QJsonObject { { "name", "list_fixtures" } });
    QVERIFY2(toolText(again).contains("T1"), qPrintable(QJsonDocument(again).toJson()));
}

void McpServer_Test::localOnly()
{
    enable(true, true);
    QVERIFY(m_mcp->isListening());
    // A local socket (a named pipe on Windows), never an address and port.
    QVERIFY(!m_mcp->serverName().contains(':') || m_mcp->serverName().startsWith("\\\\"));
#ifndef Q_OS_WIN
    // This user only.
    QLocalSocket s;
    s.connectToServer(m_mcp->serverName());
    QVERIFY(s.waitForConnected(1000));
    const QString path = s.fullServerName();
    s.abort();
    const QFileInfo fi(path);
    QVERIFY2(fi.exists(), qPrintable(path));
    QVERIFY(!(fi.permissions() & (QFile::ReadOther | QFile::WriteOther | QFile::ReadGroup | QFile::WriteGroup)));
    // The default name is in a folder of this user's.
    qunsetenv("QLC_MCP_SOCKET");
    const QString def = McpBridge::defaultServerName();
    qputenv("QLC_MCP_SOCKET", m_socket.toLocal8Bit());
    QVERIFY(def.startsWith('/'));
    QVERIFY(def.toLocal8Bit().size() <= 104);
#endif
}

void McpServer_Test::anotherInstanceKeepsTheSocket()
{
    enable(true, true);
    QVERIFY(m_mcp->isListening());
    McpServer second(m_doc);
    QVERIFY(!second.isListening());
    QVERIFY(second.problem().contains("Another qlcconsole"));
    QVERIFY(second.statusText().contains("Another qlcconsole"));
    // The first still answers.
    QVERIFY(!call("tools/call", QJsonObject { { "name", "show_summary" } }).value("result").toObject().value("isError").toBool());
}

void McpServer_Test::settingsPage()
{
    {
        AiSettingsPage page;
        QCheckBox *mcp = page.findChild<QCheckBox *>("aiMcp");
        QCheckBox *changes = page.findChild<QCheckBox *>("aiMcpChanges");
        QLabel *note = page.findChild<QLabel *>("aiMcpNote");
        QVERIFY(mcp && changes && note);
        // Off by default, and only with AI Assist on.
        QVERIFY(!mcp->isChecked());
        QVERIFY(!changes->isChecked());
        QVERIFY(!mcp->isEnabled());
        page.findChild<QCheckBox *>("aiEnabled")->setChecked(true);
        QVERIFY(mcp->isEnabled());
        QVERIFY(!changes->isEnabled());
        mcp->setChecked(true);
        QVERIFY(changes->isEnabled());
        QVERIFY(note->text().contains("--mcp"));
        QVERIFY(note->text().contains("claude mcp add qlcconsole"));
        QVERIFY(!m_mcp->isListening());
        page.apply();
    }
    // OK: it listens.
    QVERIFY(AiSettings::load().mcpEnabled);
    QVERIFY(!AiSettings::load().mcpChanges);
    QVERIFY(m_mcp->isListening());
    AiSettingsPage page;
    QVERIFY(page.findChild<QLabel *>("aiMcpNote")->text().contains("Listening on this computer only"));
}

QTEST_MAIN(McpServer_Test)

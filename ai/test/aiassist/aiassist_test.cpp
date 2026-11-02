/*
  Q Light Controller Plus - Unit test
  aiassist_test.cpp

  AI Assist P1 (docs/design/AI_ASSIST.md): the provider adapters on
  recorded bodies, the settings and keys, the read-only assistant against a
  fake model server on loopback (never a real model, never the network),
  and the Design/Operate gating of section 5a.

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
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextBrowser>
#include <QToolButton>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QLabel>
#include <QThread>
#include <QXmlStreamWriter>
#include <QRegularExpression>

#include "aiassist_test.h"
#include "aisettingspage.h"
#include "aicommandline.h"
#include "aiprovider.h"
#include "aisettings.h"
#include "aiassist.h"
#include "showtreecontroller.h"
#include "inputoutputmap.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

/****************************************************************************
 * A model server on 127.0.0.1: answers each request with what the test
 * queued for its path, or holds it open (a model that is thinking).
 ****************************************************************************/

class FakeModelServer : public QTcpServer
{
public:
    struct Request
    {
        QString method;
        QString path;
        QByteArray body;
        QByteArray headers;
        QJsonObject json() const { return QJsonDocument::fromJson(body).object(); }
    };
    struct Response
    {
        int status = 200;
        QByteArray body;
        bool hold = false;
    };

    FakeModelServer()
    {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *s = nextPendingConnection())
            {
                connect(s, &QTcpSocket::readyRead, this, [this, s]() { readFrom(s); });
                connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });
    }

    QString endpoint() const { return QStringLiteral("http://127.0.0.1:%1").arg(serverPort()); }

    /** Answers for @a path, in order; the last one repeats. */
    void queue(const QString &path, const Response &r) { m_queue[path] << r; }
    void queueJson(const QString &path, const QByteArray &json) { queue(path, Response { 200, json, false }); }
    void hold(const QString &path) { queue(path, Response { 200, QByteArray(), true }); }
    void clearQueue() { m_queue.clear(); }

    QList<Request> requests;
    int count(const QString &path) const
    {
        int n = 0;
        for (const Request &r : requests)
            n += r.path == path ? 1 : 0;
        return n;
    }

private:
    void readFrom(QTcpSocket *s)
    {
        QByteArray &buf = m_buffers[s];
        buf += s->readAll();
        const int end = buf.indexOf("\r\n\r\n");
        if (end < 0)
            return;
        const QByteArray head = buf.left(end);
        int length = 0;
        for (const QByteArray &line : head.split('\n'))
            if (line.toLower().startsWith("content-length:"))
                length = line.mid(15).trimmed().toInt();
        if (buf.size() < end + 4 + length)
            return;
        Request req;
        const QList<QByteArray> first = head.split('\n').first().trimmed().split(' ');
        req.method = QString::fromLatin1(first.value(0));
        req.path = QString::fromLatin1(first.value(1));
        req.headers = head;
        req.body = buf.mid(end + 4, length);
        m_buffers.remove(s);
        requests << req;

        Response r { 404, QByteArray("{\"error\":\"nothing queued\"}"), false };
        QList<Response> &q = m_queue[req.path];
        if (!q.isEmpty())
            r = q.count() > 1 ? q.takeFirst() : q.first();
        if (r.hold)
            return;
        QByteArray out = "HTTP/1.1 " + QByteArray::number(r.status) + " X\r\nContent-Type: application/json\r\n"
                         "Connection: close\r\nContent-Length: " + QByteArray::number(r.body.size()) + "\r\n\r\n" + r.body;
        s->write(out);
        s->disconnectFromHost();
    }

    QHash<QTcpSocket *, QByteArray> m_buffers;
    QHash<QString, QList<Response>> m_queue;
};

namespace
{
const QString kChat = QStringLiteral("/v1/chat/completions");
const QString kModel = QStringLiteral("qwen3.6:35b-a3b-mtp-q4_K_M");

/** What Ollama's /v1 really sent back for "find address conflicts"
 *  (recorded 2026-10-31 from qwen3.6 on ender). */
const QByteArray kRecordedToolCall =
    "{\"id\":\"chatcmpl-105\",\"object\":\"chat.completion\",\"created\":1793487842,"
    "\"model\":\"qwen3.6:35b-a3b-mtp-q4_K_M\",\"system_fingerprint\":\"fp_ollama\",\"choices\":[{\"index\":0,"
    "\"message\":{\"role\":\"assistant\",\"content\":\"\",\"tool_calls\":[{\"id\":\"call_gcwm67i2\",\"index\":0,"
    "\"type\":\"function\",\"function\":{\"name\":\"find_conflicts\",\"arguments\":\"{}\"}}]},"
    "\"finish_reason\":\"tool_calls\"}],\"usage\":{\"prompt_tokens\":273,\"completion_tokens\":15}}";

QByteArray textReply(const QString &text)
{
    return QJsonDocument(QJsonObject { { "choices", QJsonArray { QJsonObject {
        { "message", QJsonObject { { "role", "assistant" }, { "content", text } } } } } } }).toJson();
}

QByteArray toolReply(const QString &name, const QJsonObject &args)
{
    const QJsonObject call { { "id", "call_1" }, { "type", "function" },
                             { "function", QJsonObject { { "name", name },
                                                         { "arguments", QString::fromUtf8(QJsonDocument(args).toJson()) } } } };
    return QJsonDocument(QJsonObject { { "choices", QJsonArray { QJsonObject {
        { "message", QJsonObject { { "role", "assistant" }, { "content", "" },
                                   { "tool_calls", QJsonArray { call } } } } } } } }).toJson();
}

/** Several tool calls in one reply (a plan). */
QByteArray toolsReply(const QList<QPair<QString, QJsonObject>> &calls, const QString &text = QString())
{
    QJsonArray arr;
    int n = 0;
    for (const auto &c : calls)
        arr << QJsonObject { { "id", QStringLiteral("call_%1").arg(++n) }, { "type", "function" },
                             { "function", QJsonObject { { "name", c.first },
                                                         { "arguments", QString::fromUtf8(QJsonDocument(c.second).toJson()) } } } };
    return QJsonDocument(QJsonObject { { "choices", QJsonArray { QJsonObject {
        { "message", QJsonObject { { "role", "assistant" }, { "content", text }, { "tool_calls", arr } } } } } } }).toJson();
}

QJsonObject look(const QString &attribute, const QJsonValue &value)
{
    return QJsonObject { { "attribute", attribute }, { "value", value } };
}

AiSettings ollamaAt(const QString &endpoint)
{
    AiSettings s;
    s.enabled = true;
    s.endpoint = endpoint;
    return s;
}
}

/****************************************************************************
 * Fixture
 ****************************************************************************/

void AiAssist_Test::initTestCase()
{
    // Never the user's settings, never the real keychain.
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    QCoreApplication::setApplicationName(QStringLiteral("aiassist_test"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    AiKeychain::setBackend([this](const QString &p) { return m_keys.value(p); },
                           [this](const QString &p, const QString &k) {
                               if (k.isEmpty())
                                   m_keys.remove(p);
                               else
                                   m_keys.insert(p, k);
                               return true;
                           });
    AiAssist::setAssertOnViolation(false);
}

void AiAssist_Test::init()
{
    QSettings().clear();
    m_keys.clear();
    m_doc = new Doc(this);
    m_server = new FakeModelServer();
    m_ai = new AiAssist(m_doc, this);
}

void AiAssist_Test::cleanup()
{
    delete m_ai;
    m_ai = nullptr;
    delete m_server;
    m_server = nullptr;
    delete m_doc;
    m_doc = nullptr;
    AiResources::destroyAll();
}

void AiAssist_Test::enable(bool on, const QString &provider)
{
    AiSettings s;
    s.enabled = on;
    s.provider = provider;
    s.endpoint = m_server->endpoint();
    s.model = kModel;
    s.save();
    m_ai->settingsChanged();
}

void AiAssist_Test::overlappingPars()
{
    for (int i = 0; i < 2; ++i)
    {
        Fixture *fx = new Fixture(m_doc);
        fx->setName(QStringLiteral("Par %1").arg(i + 1));
        fx->setChannels(4);
        fx->setUniverse(0);
        fx->setAddress(i * 10);
        QVERIFY(m_doc->addFixture(fx));
        if (i == 1)
            fx->setAddress(2);               // Par 2 on Par 1's 3-4, as a hand-edited file can
    }
}

ShowTreeCommand::Context AiAssist_Test::smallShow()
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
    auto group = [this](const QString &name, const QList<Fixture *> &fixtures) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        m_doc->addFixtureGroup(g);
        for (Fixture *fx : fixtures)
            g->assignFixture(fx->id());
    };
    group("Towers", { dimmer("T1", 0), dimmer("T2", 10) });
    group("Steps", { dimmer("S1", 20) });
    QLCPalette *blue = new QLCPalette(QLCPalette::Color, m_doc);
    blue->setName("Blue");
    blue->setValue(QLCPalette::colorToString(QColor(Qt::blue), QColor(Qt::black)));
    m_doc->addPalette(blue);
    Scene *haze = new Scene(m_doc);
    haze->setName("Haze");
    m_doc->addFunction(haze);

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

QByteArray AiAssist_Test::treeXml() const
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.writeStartDocument();
    m_doc->showTree()->saveXML(&w);
    w.writeEndDocument();
    // The id counter only counts up (an undo never hands an id out twice).
    return QString::fromUtf8(out).remove(QRegularExpression(" NextId=\"\\d+\"")).toUtf8();
}

/****************************************************************************
 * The adapters
 ****************************************************************************/

void AiAssist_Test::openAiBody()
{
    AiSettings s = ollamaAt("http://localhost:11434");
    std::unique_ptr<AiProvider> p = AiProvider::create(s, QString());
    AiMessage q;
    q.text = "find address conflicts";
    AiMessage said;
    said.role = AiMessage::Assistant;
    said.calls << AiToolCall { "call_1", "find_conflicts", QJsonObject() };
    AiMessage result;
    result.role = AiMessage::Tool;
    result.toolCallId = "call_1";
    result.toolName = "find_conflicts";
    result.text = "No address conflicts";
    const QJsonObject body = QJsonDocument::fromJson(
        p->chatBody("system words", { q, said, result }, AiAssist::tools())).object();
    QCOMPARE(body.value("model").toString(), kModel);
    QCOMPARE(body.value("stream").toBool(true), false);
    QCOMPARE(body.value("reasoning_effort").toString(), QStringLiteral("none"));
    const QJsonArray msgs = body.value("messages").toArray();
    QCOMPARE(msgs.count(), 4);
    QCOMPARE(msgs.at(0).toObject().value("role").toString(), QStringLiteral("system"));
    QCOMPARE(msgs.at(2).toObject().value("tool_calls").toArray().first().toObject()
                 .value("function").toObject().value("name").toString(), QStringLiteral("find_conflicts"));
    QCOMPARE(msgs.at(3).toObject().value("role").toString(), QStringLiteral("tool"));
    QCOMPARE(msgs.at(3).toObject().value("tool_call_id").toString(), QStringLiteral("call_1"));
    // Every read-only operation is a tool; nothing that writes.
    const QJsonArray tools = body.value("tools").toArray();
    QStringList names;
    for (const QJsonValue &t : tools)
        names << t.toObject().value("function").toObject().value("name").toString();
    QVERIFY(names.contains("find_conflicts"));
    QVERIFY(names.contains("show_summary"));
    QVERIFY(!names.contains("command"));
    // A generic OpenAI server doesn't get Ollama's switch.
    s.provider = "openai-compatible";
    const QJsonObject generic = QJsonDocument::fromJson(AiProvider::create(s, "k")->chatBody(QString(), { q }, {})).object();
    QVERIFY(!generic.contains("reasoning_effort"));
    QVERIFY(!generic.contains("tools"));
}

void AiAssist_Test::openAiParse()
{
    std::unique_ptr<AiProvider> p = AiProvider::create(ollamaAt("http://localhost:11434"), QString());
    AiTurn t = p->parseChat(kRecordedToolCall);
    QVERIFY(t.ok);
    QCOMPARE(t.calls.count(), 1);
    QCOMPARE(t.calls.first().name, QStringLiteral("find_conflicts"));
    QCOMPARE(t.calls.first().id, QStringLiteral("call_gcwm67i2"));
    QVERIFY(t.calls.first().args.isEmpty());
    t = p->parseChat(textReply("All clear."));
    QVERIFY(t.ok && t.calls.isEmpty());
    QCOMPARE(t.text, QStringLiteral("All clear."));
    t = p->parseChat("{\"error\":{\"message\":\"model not found\"}}");
    QVERIFY(!t.ok);
    QCOMPARE(t.error, QStringLiteral("model not found"));
    t = p->parseChat("<html>");
    QVERIFY(!t.ok);
}

void AiAssist_Test::anthropicBody()
{
    AiSettings s;
    s.enabled = true;
    s.provider = "anthropic";
    s.endpoint = AiSettings::defaultEndpoint("anthropic");
    s.model = "some-model";
    std::unique_ptr<AiProvider> p = AiProvider::create(s, "secret-key");
    const QNetworkRequest req = p->chatRequest();
    QCOMPARE(req.url().toString(), QStringLiteral("https://api.anthropic.com/v1/messages"));
    QCOMPARE(req.rawHeader("x-api-key"), QByteArray("secret-key"));
    QCOMPARE(req.rawHeader("anthropic-version"), QByteArray("2023-06-01"));

    AiMessage q;
    q.text = "how many fixtures?";
    AiMessage said;
    said.role = AiMessage::Assistant;
    said.text = "Looking.";
    said.calls << AiToolCall { "tu_1", "show_summary", QJsonObject() }
               << AiToolCall { "tu_2", "list_groups", QJsonObject() };
    AiMessage r1;
    r1.role = AiMessage::Tool;
    r1.toolCallId = "tu_1";
    r1.text = "Fixtures: 2";
    AiMessage r2 = r1;
    r2.toolCallId = "tu_2";
    r2.text = "0 groups";
    const QByteArray raw = p->chatBody("sys", { q, said, r1, r2 }, AiAssist::tools());
    QVERIFY(!raw.contains("secret-key"));
    const QJsonObject body = QJsonDocument::fromJson(raw).object();
    QCOMPARE(body.value("system").toString(), QStringLiteral("sys"));
    QVERIFY(body.value("max_tokens").toInt() > 0);
    const QJsonArray msgs = body.value("messages").toArray();
    QCOMPARE(msgs.count(), 3);              // user, assistant, ONE user of both results
    const QJsonArray blocks = msgs.at(1).toObject().value("content").toArray();
    QCOMPARE(blocks.count(), 3);
    QCOMPARE(blocks.at(1).toObject().value("type").toString(), QStringLiteral("tool_use"));
    const QJsonArray results = msgs.at(2).toObject().value("content").toArray();
    QCOMPARE(results.count(), 2);
    QCOMPARE(results.at(1).toObject().value("tool_use_id").toString(), QStringLiteral("tu_2"));
    QVERIFY(body.value("tools").toArray().first().toObject().contains("input_schema"));
}

void AiAssist_Test::anthropicParse()
{
    AiSettings s;
    s.provider = "anthropic";
    std::unique_ptr<AiProvider> p = AiProvider::create(s, "k");
    const QByteArray reply = "{\"content\":[{\"type\":\"text\",\"text\":\"Checking.\"},"
                             "{\"type\":\"tool_use\",\"id\":\"tu_9\",\"name\":\"describe_function\","
                             "\"input\":{\"function\":\"Song\"}}],\"stop_reason\":\"tool_use\"}";
    const AiTurn t = p->parseChat(reply);
    QVERIFY(t.ok);
    QCOMPARE(t.text, QStringLiteral("Checking."));
    QCOMPARE(t.calls.count(), 1);
    QCOMPARE(t.calls.first().args.value("function").toString(), QStringLiteral("Song"));
    QVERIFY(!p->parseChat("{\"type\":\"error\",\"error\":{\"message\":\"bad key\"}}").ok);
}

void AiAssist_Test::endpointPaths()
{
    AiSettings s = ollamaAt("http://box:8080/v1/");
    s.provider = "openai-compatible";
    std::unique_ptr<AiProvider> p = AiProvider::create(s, "k");
    QCOMPARE(p->chatRequest().url().toString(), QStringLiteral("http://box:8080/v1/chat/completions"));
    QCOMPARE(p->modelsRequest().url().toString(), QStringLiteral("http://box:8080/v1/models"));
    QCOMPARE(p->chatRequest().rawHeader("Authorization"), QByteArray("Bearer k"));
    p = AiProvider::create(ollamaAt("http://localhost:11434"), QString());
    QCOMPARE(p->modelsRequest().url().toString(), QStringLiteral("http://127.0.0.1:11434/api/tags"));
    QVERIFY(p->chatRequest().rawHeader("Authorization").isEmpty());
    // A key never follows a redirect.
    QCOMPARE(p->chatRequest().attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
             int(QNetworkRequest::ManualRedirectPolicy));
}

void AiAssist_Test::parseModels()
{
    std::unique_ptr<AiProvider> p = AiProvider::create(ollamaAt("http://localhost:11434"), QString());
    QString err;
    QCOMPARE(p->parseModels("{\"models\":[{\"name\":\"a\"},{\"name\":\"b\"}]}", &err), QStringList({ "a", "b" }));
    QVERIFY(err.isEmpty());
    QCOMPARE(p->parseModels("{\"data\":[{\"id\":\"x\"}]}", &err), QStringList({ "x" }));
    QVERIFY(p->parseModels("{\"error\":\"nope\"}", &err).isEmpty());
    QCOMPARE(err, QStringLiteral("nope"));
}

/****************************************************************************
 * Settings and keys
 ****************************************************************************/

void AiAssist_Test::settingsDefaultsAndRoundTrip()
{
    const AiSettings d = AiSettings::load();
    QCOMPARE(d.enabled, false);              // off by default
    QCOMPARE(d.provider, QStringLiteral("ollama"));
    QCOMPARE(d.endpoint, QStringLiteral("http://localhost:11434"));
    QCOMPARE(d.model, kModel);
    QCOMPARE(d.sendShowContext, QStringLiteral("summary"));
    QCOMPARE(d.timeoutSec, 60);
    AiSettings s = d;
    s.enabled = true;
    s.provider = "anthropic";
    s.endpoint = "https://api.anthropic.com";
    s.model = "m";
    s.sendShowContext = "none";
    s.timeoutSec = 30;
    s.save();
    QVERIFY(AiSettings::load() == s);
    QSettings().setValue("ai/provider", "teapot");
    QCOMPARE(AiSettings::load().provider, QStringLiteral("ollama"));
    QVERIFY(QSettings().allKeys().contains("ai/enabled"));
}

void AiAssist_Test::whereTheModelRuns()
{
    AiSettings s = ollamaAt("http://localhost:11434");
    QVERIFY(s.isLocalOllama());
    QVERIFY(!s.sendsOffMachine());
    s.endpoint = "http://127.0.0.1:11434";
    QVERIFY(s.isLocalOllama());
    s.endpoint = "http://[::1]:11434";
    QVERIFY(s.isLocalOllama());
    s.endpoint = "http://studio.local:11434";
    QVERIFY(!s.isLocalOllama());
    QVERIFY(s.sendsOffMachine());
    s.provider = "anthropic";
    s.endpoint = "http://localhost:1";
    QVERIFY(s.sendsOffMachine());
    QVERIFY(!s.isLocalOllama());
}

void AiAssist_Test::keysNeverInSettings()
{
    AiSettingsPage page;
    page.findChild<QCheckBox *>("aiEnabled")->setChecked(true);
    QComboBox *provider = page.findChild<QComboBox *>("aiProvider");
    provider->setCurrentIndex(provider->findData("anthropic"));
    QLineEdit *key = page.findChild<QLineEdit *>("aiKey");
    QTest::keyClicks(key, "sk-test-123");
    page.apply();
    QCOMPARE(m_keys.value("anthropic"), QStringLiteral("sk-test-123"));
    QSettings s;
    for (const QString &k : s.allKeys())
        QVERIFY2(!s.value(k).toString().contains("sk-test-123"), qPrintable(k));
    QCOMPARE(AiKeychain::read("anthropic"), QStringLiteral("sk-test-123"));
    // Apply again without typing: the key stays as it was.
    page.apply();
    QCOMPARE(m_keys.value("anthropic"), QStringLiteral("sk-test-123"));
}

void AiAssist_Test::settingsPage()
{
    AiSettingsPage page;
    QCheckBox *enabled = page.findChild<QCheckBox *>("aiEnabled");
    QComboBox *provider = page.findChild<QComboBox *>("aiProvider");
    QLineEdit *endpoint = page.findChild<QLineEdit *>("aiEndpoint");
    QLineEdit *key = page.findChild<QLineEdit *>("aiKey");
    QLabel *note = page.findChild<QLabel *>("aiCloudNote");
    QWidget *test = page.findChild<QWidget *>("aiTest");
    QVERIFY(enabled && provider && endpoint && key && note && test);
    QVERIFY(!enabled->isChecked());
    QVERIFY(!test->isEnabled());             // off: nothing can be sent
    enabled->setChecked(true);
    QVERIFY(test->isEnabled());
    QVERIFY(key->isHidden());                // Ollama: no key
    QVERIFY(note->text().contains("nothing leaves"));
    provider->setCurrentIndex(provider->findData("anthropic"));
    QCOMPARE(endpoint->text(), QStringLiteral("https://api.anthropic.com"));   // followed the provider
    QVERIFY(!key->isHidden());
    QVERIFY(note->text().contains("leave this one"));
    endpoint->setText("http://my.server:9000");
    provider->setCurrentIndex(provider->findData("ollama"));
    QCOMPARE(endpoint->text(), QStringLiteral("http://my.server:9000"));      // typed: kept
    QVERIFY(note->text().contains("another computer"));
    page.apply();
    QCOMPARE(AiSettings::load().endpoint, QStringLiteral("http://my.server:9000"));
    QVERIFY(AiSettings::load().enabled);
}

/****************************************************************************
 * The assistant
 ****************************************************************************/

void AiAssist_Test::assistantLines()
{
    QVERIFY(!AiAssist::isAssistantLine("? find address conflicts"));   // off
    enable();
    QVERIFY(AiAssist::isAssistantLine("? find address conflicts"));
    QVERIFY(AiAssist::isAssistantLine("?how many movers"));
    QVERIFY(!AiAssist::isAssistantLine("?"));                   // the grammar's help
    QVERIFY(!AiAssist::isAssistantLine("? spread"));            // one word's help
    QVERIFY(!AiAssist::isAssistantLine("help spread"));
    QVERIFY(!AiAssist::isAssistantLine("Towers colour Blue"));
}

void AiAssist_Test::disabledSendsNothing()
{
    enable(false);
    overlappingPars();
    QString why;
    QVERIFY(!m_ai->ask("find address conflicts", ShowTreeCommand::Context(), &why));
    QVERIFY(why.contains("off"));
    QVERIFY(!m_ai->testConnection(AiSettings::load(), QString()));
    QVERIFY(!m_ai->isAvailable());
    QTest::qWait(50);
    QCOMPARE(m_server->requests.count(), 0);
    QVERIFY(AiResources::live().isEmpty());  // not even a network manager
}

void AiAssist_Test::findAddressConflicts()
{
    // P1's acceptance on a fake model: the recorded tool call, then the
    // model's answer from what the console said.
    enable();
    overlappingPars();
    m_server->queueJson(kChat, kRecordedToolCall);
    m_server->queueJson(kChat, textReply("Par 2 overlaps Par 1 on channels 3-4. Re-patch Par 2."));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(m_ai->ask("find address conflicts", ShowTreeCommand::Context()));
    QVERIFY(m_ai->isBusy());
    QVERIFY(answered.wait(5000));
    const AiAssist::Answer a = answered.first().first().value<AiAssist::Answer>();
    QVERIFY2(a.ok, qPrintable(a.text));
    QVERIFY(a.text.contains("Par 2 overlaps Par 1"));
    QCOMPARE(a.commands, QStringList { "conflicts" });
    QVERIFY(a.rejected.isEmpty());
    QCOMPARE(m_server->count(kChat), 2);
    // The second request carries the console's own answer back.
    const QJsonArray msgs = m_server->requests.at(1).json().value("messages").toArray();
    const QJsonObject tool = msgs.last().toObject();
    QCOMPARE(tool.value("role").toString(), QStringLiteral("tool"));
    QVERIFY2(tool.value("content").toString().contains("PAT-01"), qPrintable(tool.value("content").toString()));
    QVERIFY(tool.value("content").toString().contains("Par 1"));
    // The show summary went with the question (sendShowContext = summary).
    QVERIFY(msgs.first().toObject().value("content").toString().contains("Par 2"));
    QVERIFY(!m_ai->isBusy());
}

void AiAssist_Test::refusesAnythingButAReadOnlyOperation()
{
    enable();
    m_server->queueJson(kChat, toolReply("patch_fixture", QJsonObject { { "address", 1 } }));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(m_ai->ask("patch a spot at 1", ShowTreeCommand::Context()));
    QVERIFY(answered.wait(5000));
    AiAssist::Answer a = answered.first().first().value<AiAssist::Answer>();
    QVERIFY(!a.ok);
    QCOMPARE(a.rejected.count(), 1);
    QVERIFY(a.rejected.first().startsWith("patch_fixture"));
    QCOMPARE(m_server->count(kChat), 1);    // nothing run, nothing sent back

    // The command line's own operation (it writes) is refused the same way.
    answered.clear();
    m_server->requests.clear();
    m_server->clearQueue();
    m_server->queueJson(kChat, toolReply("command", QJsonObject { { "line", "undo" } }));
    QVERIFY(m_ai->ask("undo that", ShowTreeCommand::Context()));
    QVERIFY(answered.wait(5000));
    a = answered.first().first().value<AiAssist::Answer>();
    QVERIFY(!a.ok);
    QVERIFY(a.rejected.first().startsWith("command"));
}

void AiAssist_Test::invalidArgumentsGoBackToTheModel()
{
    enable();
    m_server->queueJson(kChat, toolReply("describe_function", QJsonObject { { "function", "Nope" } }));
    m_server->queueJson(kChat, textReply("There is no function called Nope."));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(m_ai->ask("what is Nope", ShowTreeCommand::Context()));
    QVERIFY(answered.wait(5000));
    const AiAssist::Answer a = answered.first().first().value<AiAssist::Answer>();
    QVERIFY(a.ok);
    QVERIFY(a.commands.isEmpty());          // it didn't run
    const QJsonArray msgs = m_server->requests.at(1).json().value("messages").toArray();
    QVERIFY(msgs.last().toObject().value("content").toString().startsWith("error:"));
}

void AiAssist_Test::providerDownGreysTheEntry()
{
    // A port nothing listens on.
    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
    const quint16 port = probe.serverPort();
    probe.close();
    AiSettings s = ollamaAt(QStringLiteral("http://127.0.0.1:%1").arg(port));
    s.save();
    m_ai->settingsChanged();
    QSignalSpy avail(m_ai, &AiAssist::availabilityChanged);
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(m_ai->ask("anything", ShowTreeCommand::Context()));
    QVERIFY(answered.wait(5000));
    QVERIFY(!answered.first().first().value<AiAssist::Answer>().ok);
    QVERIFY(m_ai->isUnreachable());
    QVERIFY(m_ai->unreachableReason().contains(QString::number(port)));
    QVERIFY(avail.count() >= 1);

    // The command line keeps saying it takes plain words: a down provider
    // doesn't take that away (a retry is one Enter).
    QWidget bar;
    QHBoxLayout *row = new QHBoxLayout(&bar);
    QLineEdit *line = new QLineEdit(&bar);
    row->addWidget(line);
    QTextBrowser pane;
    QLabel msg(&bar);
    AiCommandLine cl(line, &pane, &msg, &bar);
    QVERIFY(line->placeholderText().contains("AI Assist"));
    // Back up: a good connection test lifts it.
    s.endpoint = m_server->endpoint();
    s.save();
    m_ai->settingsChanged();
    QVERIFY(line->placeholderText().contains("AI Assist"));
}

void AiAssist_Test::testConnectionListsModels()
{
    enable();
    m_server->queueJson("/api/tags", "{\"models\":[{\"name\":\"qwen3.6:35b-a3b-mtp-q4_K_M\"},{\"name\":\"llama\"}]}");
    QSignalSpy tested(m_ai, &AiAssist::connectionTested);
    QVERIFY(m_ai->testConnection(AiSettings::load(), QString()));
    QVERIFY(tested.wait(5000));
    QVERIFY(tested.first().at(0).toBool());
    QCOMPARE(tested.first().at(1).toStringList().count(), 2);
    QVERIFY(tested.first().at(2).toString().startsWith("Connected"));
    // A model the server doesn't have: said.
    AiSettings other = AiSettings::load();
    other.model = "missing";
    tested.clear();
    QVERIFY(m_ai->testConnection(other, QString()));
    QVERIFY(tested.wait(5000));
    QVERIFY(tested.first().at(2).toString().contains("missing"));
}

void AiAssist_Test::testFromUnsavedPage()
{
    // He ticks Enabled and presses Test without OK: nothing saved, AI Assist off.
    QVERIFY(!AiSettings::load().enabled);
    m_server->queueJson("/api/tags", "{\"models\":[{\"name\":\"llama\"}]}");
    AiSettingsPage page;
    QCheckBox *enabled = page.findChild<QCheckBox *>("aiEnabled");
    QLineEdit *endpoint = page.findChild<QLineEdit *>("aiEndpoint");
    QPushButton *test = page.findChild<QPushButton *>("aiTest");
    QLabel *result = page.findChild<QLabel *>("aiTestResult");
    QVERIFY(enabled && endpoint && test && result);
    enabled->setChecked(true);
    endpoint->setText(m_server->endpoint());
    test->click();
    QVERIFY(!test->isEnabled());
    QTRY_VERIFY_WITH_TIMEOUT(test->isEnabled(), 5000);
    QVERIFY2(result->text().startsWith("Connected"), qPrintable(result->text()));
    QVERIFY(!AiSettings::load().enabled);           // still unsaved
}

void AiAssist_Test::testAgainstASilentServer()
{
    // A server that accepts and never answers: a result inside the test's own cap.
    m_server->hold("/api/tags");
    AiSettingsPage page;
    QCheckBox *enabled = page.findChild<QCheckBox *>("aiEnabled");
    QLineEdit *endpoint = page.findChild<QLineEdit *>("aiEndpoint");
    QPushButton *test = page.findChild<QPushButton *>("aiTest");
    QLabel *result = page.findChild<QLabel *>("aiTestResult");
    enabled->setChecked(true);
    endpoint->setText(m_server->endpoint());
    AiAssist::setTestCapMs(300);
    test->click();
    QVERIFY(!test->isEnabled());
    QTRY_VERIFY_WITH_TIMEOUT(test->isEnabled(), 3000);
    QVERIFY2(result->text().contains("Can't reach") || result->text().contains("No answer"),
             qPrintable(result->text()));
    AiAssist::setTestCapMs(0);
}

void AiAssist_Test::abortedTestSaysSo()
{
    // Whatever drops the network manager mid-test, the page hears back.
    m_server->hold("/api/tags");
    AiSettingsPage page;
    page.findChild<QCheckBox *>("aiEnabled")->setChecked(true);
    page.findChild<QLineEdit *>("aiEndpoint")->setText(m_server->endpoint());
    QPushButton *test = page.findChild<QPushButton *>("aiTest");
    QLabel *result = page.findChild<QLabel *>("aiTestResult");
    test->click();
    QVERIFY(!test->isEnabled());
    QTRY_COMPARE_WITH_TIMEOUT(m_server->count("/api/tags"), 1, 3000);
    // Saved as off while it runs, then "Off" applied: abortAll.
    AiSettings on = AiSettings::load();
    on.enabled = true;
    on.endpoint = m_server->endpoint();
    on.save();
    m_ai->settingsChanged();
    on.enabled = false;
    on.save();
    m_ai->settingsChanged();
    QVERIFY2(test->isEnabled(), "the Test button stayed greyed");
    QVERIFY2(result->text().contains("ancel"), qPrintable(result->text()));
}

void AiAssist_Test::testNeverReadsTheKeychainForOllama()
{
    bool asked = false;
    AiKeychain::setBackend([&asked](const QString &) { asked = true; return QString(); },
                           [](const QString &, const QString &) { return true; });
    m_server->queueJson("/api/tags", "{\"models\":[]}");
    AiSettingsPage page;
    page.findChild<QCheckBox *>("aiEnabled")->setChecked(true);
    page.findChild<QLineEdit *>("aiEndpoint")->setText(m_server->endpoint());
    QPushButton *test = page.findChild<QPushButton *>("aiTest");
    test->click();
    QTRY_VERIFY_WITH_TIMEOUT(test->isEnabled(), 5000);
    QVERIFY(!asked);
    AiKeychain::setBackend([this](const QString &p) { return m_keys.value(p); },
                           [this](const QString &p, const QString &k) { m_keys.insert(p, k); return true; });
}

void AiAssist_Test::localhostFallsBackToIpv4()
{
    // Ollama listens on 127.0.0.1 only; "localhost" may resolve to ::1 first.
    m_server->queueJson("/api/tags", "{\"models\":[{\"name\":\"llama\"}]}");
    AiSettings s;
    s.enabled = true;
    s.endpoint = QStringLiteral("http://localhost:%1").arg(m_server->serverPort());
    QSignalSpy tested(m_ai, &AiAssist::connectionTested);
    QVERIFY(m_ai->testConnection(s, QString()));
    QVERIFY(tested.wait(8000));
    QVERIFY2(tested.first().at(0).toBool(), qPrintable(tested.first().at(2).toString()));
}

void AiAssist_Test::cancel()
{
    enable();
    m_server->hold(kChat);
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QSignalSpy busy(m_ai, &AiAssist::busyChanged);
    QVERIFY(m_ai->ask("slow question", ShowTreeCommand::Context()));
    QTRY_COMPARE(m_server->count(kChat), 1);
    QString why;
    QVERIFY(!m_ai->ask("another", ShowTreeCommand::Context(), &why));   // one at a time
    QVERIFY(why.contains("Esc"));
    m_ai->cancel();
    QCOMPARE(answered.count(), 1);
    QCOMPARE(answered.first().first().value<AiAssist::Answer>().text, QStringLiteral("Cancelled"));
    QVERIFY(!m_ai->isBusy());
    QCOMPARE(busy.last().first().toBool(), false);
}

void AiAssist_Test::systemPromptContext()
{
    enable();
    overlappingPars();
    QString p = m_ai->systemPrompt(ShowTreeCommand::Context());
    QVERIFY(p.contains("Par 1"));
    QVERIFY(p.contains("Fixtures: 2"));
    QVERIFY(!p.contains("<?xml"));           // never the show file
    QSettings().setValue("ai/sendShowContext", "none");
    m_ai->settingsChanged();
    p = m_ai->systemPrompt(ShowTreeCommand::Context());
    QVERIFY(!p.contains("Par 1"));
}

void AiAssist_Test::commandLineShowsTheAnswer()
{
    QWidget bar;
    QHBoxLayout *row = new QHBoxLayout(&bar);
    QLineEdit *line = new QLineEdit(&bar);
    row->addWidget(line);
    QTextBrowser pane;
    QLabel msg(&bar);
    AiCommandLine cl(line, &pane, &msg, &bar);
    QVERIFY(!line->placeholderText().contains("AI Assist"));   // off: no chrome
    QVERIFY(!cl.handle("? find address conflicts", ShowTreeCommand::Context()));   // off: the grammar's

    enable();
    QVERIFY(line->placeholderText().contains("AI Assist"));    // the indicator

    overlappingPars();
    m_server->queueJson(kChat, kRecordedToolCall);
    m_server->queueJson(kChat, textReply("Par 2 overlaps Par 1."));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(cl.handle("? find address conflicts", ShowTreeCommand::Context()));
    QVERIFY(msg.text().startsWith("Asking"));
    QVERIFY(answered.wait(5000));
    QVERIFY(pane.toHtml().contains("Par 2 overlaps Par 1."));
    QVERIFY(pane.toHtml().contains("ex:conflicts"));     // a link: the command to type
    QVERIFY(msg.text().contains("nothing in the show changed"));

    // Operate: the indicator goes.
    m_doc->setMode(Doc::Operate);
    QVERIFY(!line->placeholderText().contains("AI Assist"));
    QVERIFY(cl.handle("? anything", ShowTreeCommand::Context()));
    QVERIFY(msg.text().contains("unavailable in Operate mode"));
    m_doc->setMode(Doc::Design);
    QVERIFY(line->placeholderText().contains("AI Assist"));
}

/****************************************************************************
 * Design / Operate
 ****************************************************************************/

void AiAssist_Test::operateDuringARequest()
{
    // Not a local Ollama (the provider is a generic server): no unload,
    // only the audit of what is ours.
    enable(true, "openai-compatible");
    m_server->hold(kChat);
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QSignalSpy done(m_ai, &AiAssist::teardownFinished);
    QVERIFY(m_ai->ask("slow", ShowTreeCommand::Context()));
    QTRY_COMPARE(m_server->count(kChat), 1);
    QVERIFY(!AiResources::live().isEmpty());

    QElapsedTimer t;
    t.start();
    m_doc->setMode(Doc::Operate);
    const qint64 ms = t.elapsed();
    QVERIFY2(ms < 100, qPrintable(QString::number(ms)));    // never delays the switch
    QCOMPARE(m_doc->mode(), Doc::Operate);
    // Aborted, said, and nothing left alive -- at once.
    QCOMPARE(answered.count(), 1);
    QVERIFY(answered.first().first().value<AiAssist::Answer>().text.contains("Operate"));
    QVERIFY2(AiResources::live().isEmpty(), qPrintable(AiResources::live().join(",")));
    QVERIFY(!m_ai->isAvailable());
    QVERIFY(done.wait(2000));
    QVERIFY(done.first().first().toBool());
    QVERIFY(m_ai->resourceErrorText().isEmpty());
    QCOMPARE(m_server->count("/api/generate"), 0);
}

void AiAssist_Test::operateUnloadsALocalModel()
{
    enable();                                // Ollama on 127.0.0.1: local
    m_server->queueJson(kChat, textReply("Hi."));
    m_server->queueJson("/api/generate", "{\"done\":true}");
    m_server->queueJson("/api/ps", "{\"models\":[]}");
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(m_ai->ask("hello", ShowTreeCommand::Context()));
    QVERIFY(answered.wait(5000));
    QSignalSpy done(m_ai, &AiAssist::teardownFinished);
    m_doc->setMode(Doc::Operate);
    QVERIFY(done.wait(5000));
    QVERIFY(done.first().first().toBool());
    QCOMPARE(m_server->count("/api/generate"), 1);
    QCOMPARE(m_server->count("/api/ps"), 1);
    for (const FakeModelServer::Request &r : m_server->requests)
        if (r.path == QLatin1String("/api/generate"))
        {
            QCOMPARE(r.json().value("model").toString(), kModel);
            QCOMPARE(r.json().value("keep_alive").toInt(-1), 0);
        }
    QVERIFY(AiResources::live().isEmpty());
}

void AiAssist_Test::modelStillResidentIsAnError()
{
    enable();
    m_server->queueJson(kChat, textReply("Hi."));
    m_server->queueJson("/api/generate", "{\"done\":true}");
    m_server->queueJson("/api/ps", QStringLiteral("{\"models\":[{\"name\":\"%1\",\"model\":\"%1\"}]}").arg(kModel).toUtf8());
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(m_ai->ask("hello", ShowTreeCommand::Context()));
    QVERIFY(answered.wait(5000));
    QSignalSpy err(m_ai, &AiAssist::resourceError);
    QSignalSpy done(m_ai, &AiAssist::teardownFinished);
    m_doc->setMode(Doc::Operate);
    QCOMPARE(m_doc->mode(), Doc::Operate);   // never rolled back
    QVERIFY(done.wait(5000));
    QVERIFY(!done.first().first().toBool());
    QCOMPARE(err.count(), 1);
    QVERIFY(err.first().first().toString().startsWith("AI Assist resources still active in Operate mode"));
    QVERIFY(err.first().first().toString().contains("still loaded"));
    QSignalSpy cleared(m_ai, &AiAssist::resourceErrorCleared);
    m_doc->setMode(Doc::Design);
    QCOMPARE(cleared.count(), 1);
    QVERIFY(m_ai->resourceErrorText().isEmpty());
}

void AiAssist_Test::askInOperateSendsNothing()
{
    enable();
    m_doc->setMode(Doc::Operate);
    const int violations = AiAssist::guardViolations();
    QVERIFY(AiAssist::isAssistantLine("? find address conflicts"));
    QString why;
    QVERIFY(!m_ai->ask("find address conflicts", ShowTreeCommand::Context(), &why));
    QCOMPARE(why, QStringLiteral("AI Assist is unavailable in Operate mode"));
    QVERIFY(!m_ai->testConnection(AiSettings::load(), QString()));
    QTest::qWait(50);
    QCOMPARE(m_server->requests.count(), 0);
    QVERIFY(AiResources::live().isEmpty());
    QCOMPARE(AiAssist::guardViolations(), violations);   // refused before any AI path
}

void AiAssist_Test::leakedObjectRaisesTheError()
{
    enable();
    QPointer<QObject> leak = new QObject();
    AiResources::track(leak, "leaked timer");
    QSignalSpy err(m_ai, &AiAssist::resourceError);
    QSignalSpy done(m_ai, &AiAssist::teardownFinished);
    m_doc->setMode(Doc::Operate);
    QVERIFY(done.wait(2000));
    QVERIFY(!done.first().first().toBool());
    QCOMPARE(err.count(), 1);
    QVERIFY(err.first().first().toString().contains("leaked timer"));
    QVERIFY(leak.isNull());                  // force-terminated
    QVERIFY(AiResources::live().isEmpty());
}

void AiAssist_Test::guardFires()
{
    enable();
    const int before = AiAssist::guardViolations();
    QVERIFY(m_ai->checkGuard("test"));
    m_doc->setMode(Doc::Operate);
    QVERIFY(!m_ai->checkGuard("test"));
    QCOMPARE(AiAssist::guardViolations(), before + 1);
    m_doc->setMode(Doc::Design);
    // Off the GUI thread (a MasterTimer or universe thread would be).
    bool ok = true;
    QThread *th = QThread::create([this, &ok]() { ok = m_ai->checkGuard("thread"); });
    th->start();
    QVERIFY(th->wait(5000));
    delete th;
    QVERIFY(!ok);
    QCOMPARE(AiAssist::guardViolations(), before + 2);
}

void AiAssist_Test::toggleRepeatedlyWithRequestsInFlight()
{
    enable(true, "openai-compatible");
    m_server->hold(kChat);
    QSignalSpy done(m_ai, &AiAssist::teardownFinished);
    qint64 worst = 0;
    for (int i = 0; i < 10; ++i)
    {
        QVERIFY(m_ai->ask(QStringLiteral("question %1").arg(i), ShowTreeCommand::Context()));
        QTRY_COMPARE(m_server->count(kChat), i + 1);
        QElapsedTimer t;
        t.start();
        m_doc->setMode(Doc::Operate);
        worst = qMax(worst, t.elapsed());
        QVERIFY(AiResources::live().isEmpty());
        QTRY_COMPARE(done.count(), i + 1);
        QVERIFY(done.last().first().toBool());
        m_doc->setMode(Doc::Design);
    }
    QVERIFY2(worst < 100, qPrintable(QString::number(worst)));
}

/****************************************************************************
 * P2: changes -- proposed, shown, applied on confirm as one undo step
 ****************************************************************************/

namespace
{
/** The model proposes with @a reply, then (asked again with "proposed
 *  as step N") answers @a text: done. */
void propose(FakeModelServer *server, const QByteArray &reply, const QString &text = QString())
{
    server->clearQueue();
    server->queueJson(kChat, reply);
    server->queueJson(kChat, textReply(text));
}

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
        cl.reset(new AiCommandLine(line, &pane, msg, nullptr));
    }
};
}

void AiAssist_Test::proposalShowsAppliesAndUndoesInOneStep()
{
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    ShowTreeController *st = m_doc->showTree();
    const QByteArray before = treeXml();
    const QString undoBefore = st->sharedUndoText();
    Bar b;
    propose(m_server, toolsReply({ { "set_look", QJsonObject { { "targets", QJsonArray { "Towers" } },
                                                                 { "looks", QJsonArray { look("colour", "Blue") } } } },
                                     { "set_intensity", QJsonObject { { "targets", QJsonArray { "Towers" } }, { "level", 50 } } } }),
            "Towers blue at half: apply it to make it so.");
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(b.cl->handle("? towers blue at 50", ctx));
    QVERIFY(answered.wait(5000));
    const AiAssist::Answer a = answered.first().first().value<AiAssist::Answer>();
    QVERIFY2(a.ok, qPrintable(a.text));
    QCOMPARE(a.text, QStringLiteral("Towers blue at half: apply it to make it so."));
    QCOMPARE(a.proposal.count(), 2);
    QCOMPARE(a.proposal.at(0).command, QStringLiteral("Towers colour Blue"));
    QCOMPARE(a.proposal.at(1).command, QStringLiteral("Towers dimmer 50"));
    // Shown, not run: the show is as it was; the model was told so.
    QCOMPARE(treeXml(), before);
    QCOMPARE(m_server->count(kChat), 2);
    const QJsonArray told = m_server->requests.at(1).json().value("messages").toArray();
    QVERIFY(told.last().toObject().value("content").toString().startsWith("proposed as step 2 (not run yet"));
    QVERIFY(m_ai->hasProposal());
    const QString html = b.pane.toHtml();
    QVERIFY(html.contains("Towers colour Blue"));
    QVERIFY(html.contains("ai:apply"));
    QVERIFY(html.contains("ai:discard"));
    QVERIFY(b.msg->text().contains("Enter applies"));
    // The tools offered carried the change operations, never `command`.
    QStringList offered;
    for (const QJsonValue &t : m_server->requests.first().json().value("tools").toArray())
        offered << t.toObject().value("function").toObject().value("name").toString();
    QVERIFY(offered.contains("set_look"));
    QVERIFY(offered.contains("find_conflicts"));
    QVERIFY(!offered.contains("command"));

    // Enter on the empty line applies: both steps, one undo step.
    QSignalSpy applied(b.cl.get(), &AiCommandLine::applied);
    QVERIFY(b.cl->handle("", ctx));
    QCOMPARE(applied.count(), 1);
    QVERIFY(!m_ai->hasProposal());
    const ShowTree::Cue *c = st->cue(ctx.cue);
    QCOMPARE(c->layers.count(), 1);
    QVERIFY(c->layers.first().look(ShowTree::KeyColour) != nullptr);
    QVERIFY(c->layers.first().look(ShowTree::KeyDimmer) != nullptr);
    QVERIFY(st->sharedUndoText().startsWith("AI Assist: towers blue at 50"));
    QVERIFY(b.msg->text().contains("one undo step"));
    QVERIFY(st->undoShared());
    QCOMPARE(treeXml(), before);
    QCOMPARE(st->sharedUndoText(), undoBefore);
}

/* One command line: a line that is not a command goes to the assistant with
   the parser's error and the syntax reference; the answer is words, or a
   corrected command as a proposal (nothing runs). */
void AiAssist_Test::oneLineFallbackSendsTheParserError()
{
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    Bar b;
    m_server->clearQueue();
    m_server->queueJson(kChat, textReply("The Towers are the front truss. Say: Towers colour Blue."));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QString note;
    QVERIFY(b.cl->fallback("make the towers a nice blue", "Don't know “make”.", ctx, &note));
    QVERIFY(b.msg->text().contains("Don't know “make”."));
    QVERIFY(b.msg->text().contains("Asking"));
    QVERIFY(answered.wait(5000));
    QVERIFY(b.pane.toHtml().contains("front truss"));
    QCOMPARE(m_server->count(kChat), 1);
    // The request carries what the model needs: the line, the parser's
    // error, the syntax reference.
    const QJsonObject body = m_server->requests.first().json();
    const QString all = QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact));
    QVERIFY(all.contains("make the towers a nice blue"));
    QVERIFY(all.contains("Don't know"));
    QVERIFY(all.contains("cueonly"));              // the syntax reference
    QVERIFY(all.contains("refused"));
}

void AiAssist_Test::oneLineFallbackProposesACorrectedCommand()
{
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    ShowTreeController *st = m_doc->showTree();
    const QByteArray before = treeXml();
    Bar b;
    propose(m_server, toolsReply({ { "set_look", QJsonObject { { "targets", QJsonArray { "Towers" } },
                                                                 { "looks", QJsonArray { look("colour", "Blue") } } } } }),
            "Towers colour Blue: Enter applies it.");
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(b.cl->fallback("paint towers blue", "Don't know “paint”.", ctx));
    QVERIFY(answered.wait(5000));
    // A proposal, in the line as it would be typed; nothing has changed.
    QVERIFY(m_ai->hasProposal());
    QCOMPARE(b.line->text(), QStringLiteral("Towers colour Blue"));
    QCOMPARE(treeXml(), before);
    QVERIFY(b.msg->text().contains("Enter applies"));
    // Enter applies it, one undo step.
    QVERIFY(b.cl->handle("Towers colour Blue", ctx));
    QVERIFY(!m_ai->hasProposal());
    QVERIFY(st->cue(ctx.cue)->layers.first().look(ShowTree::KeyColour) != nullptr);
    QVERIFY(st->undoShared());
    QCOMPARE(treeXml(), before);
}

void AiAssist_Test::oneLineFallbackOffOrOperateAsksNothing()
{
    const ShowTreeCommand::Context ctx = smallShow();
    Bar b;
    // Off in Settings: nothing is sent; plain words say where to turn it on.
    enable(false);
    QString note;
    QVERIFY(!b.cl->fallback("make the towers blue", "Don't know “make”.", ctx, &note));
    QVERIFY(note.contains("Settings ▸ AI Assist"));
    note.clear();
    QVERIFY(!b.cl->fallback("xyzzy", "Don't know “xyzzy”.", ctx, &note));   // one word: no hint
    QVERIFY(note.isEmpty());
    // On, but Operate: still nothing.
    enable();
    m_doc->setMode(Doc::Operate);
    note.clear();
    QVERIFY(!b.cl->fallback("make the towers blue", "Don't know “make”.", ctx, &note));
    QVERIFY(note.isEmpty());
    m_doc->setMode(Doc::Design);
    QTest::qWait(50);
    QCOMPARE(m_server->requests.count(), 0);
}

void AiAssist_Test::decliningChangesNothing()
{
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    ShowTreeController *st = m_doc->showTree();
    const QByteArray before = treeXml();
    const QString undoBefore = st->sharedUndoText();
    Bar b;
    const QByteArray reply = toolReply("duplicate_cue", QJsonObject());
    propose(m_server, reply);
    QSignalSpy answered(m_ai, &AiAssist::answered);

    // A single step goes in the line as typed; Esc discards it.
    QVERIFY(b.cl->handle("? duplicate this cue", ctx));
    QVERIFY(answered.wait(5000));
    QCOMPARE(b.line->text(), QStringLiteral("dup"));
    QVERIFY(b.cl->cancel());
    QVERIFY(!m_ai->hasProposal());
    QVERIFY(b.line->text().isEmpty());
    QVERIFY(b.msg->text().contains("nothing in the show changed"));
    QVERIFY(!b.cl->cancel());                // nothing left to discard
    QVERIFY(!b.cl->handle("", ctx));         // Enter now: the command line's again

    // The Discard link does the same.
    answered.clear();
    propose(m_server, reply);
    QVERIFY(b.cl->handle("? duplicate this cue", ctx));
    QVERIFY(answered.wait(5000));
    emit b.pane.anchorClicked(QUrl("ai:discard"));
    QVERIFY(!m_ai->hasProposal());

    // Typing something else drops it: the line runs as typed.
    answered.clear();
    propose(m_server, reply);
    QVERIFY(b.cl->handle("? duplicate this cue", ctx));
    QVERIFY(answered.wait(5000));
    QVERIFY(!b.cl->handle("cue 2", ctx));
    QVERIFY(!m_ai->hasProposal());

    // A new question drops it too.
    answered.clear();
    propose(m_server, reply);
    QVERIFY(b.cl->handle("? duplicate this cue", ctx));
    QVERIFY(answered.wait(5000));
    m_server->clearQueue();
    m_server->queueJson(kChat, textReply("ok"));
    QVERIFY(b.cl->handle("? anything else", ctx));
    QVERIFY(!m_ai->hasProposal());
    QVERIFY(answered.wait(5000));

    QCOMPARE(treeXml(), before);
    QCOMPARE(st->sharedUndoText(), undoBefore);
}

void AiAssist_Test::leavingAStepOut()
{
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    ShowTreeController *st = m_doc->showTree();
    Bar b;
    propose(m_server, toolsReply({ { "set_intensity", QJsonObject { { "targets", QJsonArray { "Towers" } }, { "level", 50 } } },
                                     { "set_intensity", QJsonObject { { "targets", QJsonArray { "Steps" } }, { "level", 20 } } } }));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(b.cl->handle("? towers 50 steps 20", ctx));
    QVERIFY(answered.wait(5000));
    QCOMPARE(answered.first().first().value<AiAssist::Answer>().text, QStringLiteral("Proposed 2 change(s)."));
    emit b.pane.anchorClicked(QUrl("ai:toggle:1"));
    QCOMPARE(b.cl->included(), QList<bool>({ true, false }));
    QVERIFY(b.pane.toHtml().contains("line-through"));
    emit b.pane.anchorClicked(QUrl("ai:apply"));
    QVERIFY(!m_ai->hasProposal());
    QCOMPARE(st->cue(ctx.cue)->layers.count(), 1);   // Towers only

    // Every step left out: nothing to apply, the proposal stays.
    answered.clear();
    propose(m_server, toolReply("duplicate_cue", QJsonObject()));
    QVERIFY(b.cl->handle("? duplicate it", ctx));
    QVERIFY(answered.wait(5000));
    QCOMPARE(b.cl->included().count(), 1);
    b.cl->toggleStep(0);
    QVERIFY(!b.cl->applyProposal());
    QVERIFY(m_ai->hasProposal());
}

void AiAssist_Test::nothingChangesInOperate()
{
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    const QByteArray before = treeXml();
    Bar b;
    propose(m_server, toolReply("duplicate_cue", QJsonObject()));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(b.cl->handle("? duplicate it", ctx));
    QVERIFY(answered.wait(5000));
    QVERIFY(m_ai->hasProposal());
    const int violations = AiAssist::guardViolations();

    // Going to Operate drops the proposal: the line and the checklist go.
    m_doc->setMode(Doc::Operate);
    QVERIFY(!m_ai->hasProposal());
    QVERIFY(b.line->text().isEmpty());
    QVERIFY(b.pane.isHidden());
    ShowTreeCommand::Result r;
    QVERIFY(!m_ai->apply(QList<int>(), nullptr, &r));
    QVERIFY(!b.cl->applyProposal());
    QVERIFY(!b.cl->handle("", ctx));                 // Enter: the command line's
    QVERIFY(b.cl->handle("? duplicate it", ctx));
    QVERIFY(b.msg->text().contains("unavailable in Operate mode"));
    QCOMPARE(treeXml(), before);
    QCOMPARE(m_server->count(kChat), 2);             // nothing more was sent
    QCOMPARE(AiAssist::guardViolations(), violations);
    m_doc->setMode(Doc::Design);
    QVERIFY(!m_ai->hasProposal());                   // it doesn't come back
    QCOMPARE(treeXml(), before);
}

void AiAssist_Test::badArgumentsAreRefusedBeforeAnythingRuns()
{
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    const QByteArray before = treeXml();
    // A good step and a bad one in the same reply: neither is proposed;
    // the model is told why and gives up.
    m_server->queueJson(kChat, toolsReply({ { "duplicate_cue", QJsonObject() },
                                            { "set_intensity", QJsonObject { { "targets", QJsonArray { "Towers" } }, { "level", 140 } } } }));
    m_server->queueJson(kChat, textReply("Levels go up to 100."));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(m_ai->ask("towers at 140", ctx));
    QVERIFY(answered.wait(5000));
    const AiAssist::Answer a = answered.first().first().value<AiAssist::Answer>();
    QVERIFY(a.proposal.isEmpty());
    QVERIFY(!m_ai->hasProposal());
    QCOMPARE(m_server->count(kChat), 2);
    const QJsonArray msgs = m_server->requests.at(1).json().value("messages").toArray();
    const QString bad = msgs.last().toObject().value("content").toString();
    QVERIFY2(bad.startsWith("error: level: 140 is above 100"), qPrintable(bad));
    QVERIFY(msgs.at(msgs.count() - 2).toObject().value("content").toString().startsWith("not taken"));
    QCOMPARE(treeXml(), before);

    // An argument the operation doesn't take: refused the same way.
    answered.clear();
    m_server->clearQueue();
    m_server->requests.clear();
    m_server->queueJson(kChat, toolReply("record_cue", QJsonObject { { "source", "lit" }, { "fade", 3 } }));
    m_server->queueJson(kChat, textReply("Sorry."));
    QVERIFY(m_ai->ask("record it with a fade", ctx));
    QVERIFY(answered.wait(5000));
    QVERIFY(!m_ai->hasProposal());
    const QJsonArray msgs2 = m_server->requests.at(1).json().value("messages").toArray();
    QVERIFY(msgs2.last().toObject().value("content").toString().contains("fade: not an argument"));
    QCOMPARE(treeXml(), before);
}

void AiAssist_Test::aPlanOverSeveralReplies()
{
    // As qwen3.6 really does it: one step a reply, then text. One plan.
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    const QByteArray before = treeXml();
    m_server->queueJson(kChat, toolReply("add_cue", QJsonObject { { "name", "Bridge" }, { "mode", "block" } }));
    m_server->queueJson(kChat, toolReply("set_intensity", QJsonObject { { "targets", QJsonArray { "Steps" } }, { "level", 30 } }));
    m_server->queueJson(kChat, textReply("A Bridge cue with Steps at 30."));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(m_ai->ask("a Bridge cue with steps at 30", ctx));
    QVERIFY(answered.wait(5000));
    const AiAssist::Answer a = answered.first().first().value<AiAssist::Answer>();
    QCOMPARE(a.proposal.count(), 2);
    QCOMPARE(a.proposal.at(1).command, QStringLiteral("Steps dimmer 30"));
    QCOMPARE(m_server->count(kChat), 3);
    QCOMPARE(treeXml(), before);
    ShowTreeCommand::Result r;
    QVERIFY2(m_ai->apply(QList<int>(), nullptr, &r), qPrintable(r.message));
    const ShowTree::Cue *bridge = m_doc->showTree()->cue(r.context.cue);
    QVERIFY(bridge != nullptr);
    QCOMPARE(bridge->name, QStringLiteral("Bridge"));
    QCOMPARE(bridge->layers.count(), 1);         // Steps went in the new cue
    QVERIFY(m_doc->showTree()->undoShared());
    QCOMPARE(treeXml(), before);
}

void AiAssist_Test::aRefusedStepAppliesNothing()
{
    // Every schema fits, but the show has no "Nobody": step 2 is refused
    // when applied, and step 1 is taken back with it.
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    const QByteArray before = treeXml();
    const QString undoBefore = m_doc->showTree()->sharedUndoText();
    Bar b;
    propose(m_server, toolsReply({ { "add_cue", QJsonObject { { "name", "New" } } },
                                     { "set_intensity", QJsonObject { { "targets", QJsonArray { "Nobody" } }, { "level", 40 } } } }));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(b.cl->handle("? new cue nobody at 40", ctx));
    QVERIFY(answered.wait(5000));
    QVERIFY(!b.cl->applyProposal());
    QVERIFY2(b.msg->text().contains("Step 2"), qPrintable(b.msg->text()));
    QVERIFY(b.msg->text().contains("Nothing changed"));
    QCOMPARE(treeXml(), before);
    QCOMPARE(m_doc->showTree()->sharedUndoText(), undoBefore);
    QVERIFY(m_ai->hasProposal());            // still there to read; Esc drops it
}

/****************************************************************************
 * P2b: configuration import through the assistant (fake provider only)
 ****************************************************************************/

void AiAssist_Test::configImportProposedThenApplied()
{
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    const QString file = QStringLiteral(AIASSIST_SAMPLES "/evo26.qxw");
    const quint32 universes = m_doc->inputOutputMap()->universesCount();
    const QString undoBefore = m_doc->showTree()->sharedUndoText();
    // "take the universes and Art-Net setup from evo26.qxw, put universe 2 on 1":
    // it reads the file (a question, run as asked), then proposes the import.
    m_server->queueJson(kChat, toolReply("read_config", QJsonObject { { "file", file } }));
    m_server->queueJson(kChat, toolReply("import_config",
                                         QJsonObject { { "file", file }, { "universes", QJsonArray { 2 } },
                                                       { "universeMap", QJsonObject { { "2", "1" } } },
                                                       { "addressMap", QJsonObject { { "2.0.0.50", "2.0.0.60" } } },
                                                       { "onMissingPlugin", "keep" } }));
    m_server->queueJson(kChat, textReply("Universe 2 (Truss) of evo26 goes on universe 1, Art-Net at 2.0.0.60."));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(m_ai->ask("take universe 2 and the Art-Net setup from evo26.qxw, put it on 1, destination 2.0.0.60", ctx));
    QVERIFY(answered.wait(5000));
    const AiAssist::Answer a = answered.first().first().value<AiAssist::Answer>();
    QVERIFY2(a.ok, qPrintable(a.text));
    QCOMPARE(a.proposal.count(), 1);
    QVERIFY2(a.proposal.first().command.startsWith("import config "), qPrintable(a.proposal.first().command));
    QVERIFY(a.proposal.first().command.endsWith(" apply"));
    // The file was read for the model (its summary went back), nothing applied.
    const QJsonArray msgs = m_server->requests.at(1).json().value("messages").toArray();
    QVERIFY(msgs.last().toObject().value("content").toString().contains("Universe 2 \"Truss\""));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), universes);
    QVERIFY(!m_doc->inputOutputMap()->hasOfflinePatches());
    QVERIFY(m_ai->hasProposal());

    // Confirmed: one undo step, undone exactly.
    ShowTreeCommand::Result r;
    QVERIFY2(m_ai->apply(QList<int>(), nullptr, &r), qPrintable(r.message));
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(0), QStringLiteral("Truss"));
    QVERIFY(m_doc->inputOutputMap()->hasOfflinePatches());
    QVERIFY(m_doc->showTree()->sharedUndoText().startsWith("AI Assist:"));
    QVERIFY(m_doc->showTree()->undoShared());
    QCOMPARE(m_doc->showTree()->sharedUndoText(), undoBefore);
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(0), QStringLiteral("Universe 1"));
    QVERIFY(!m_doc->inputOutputMap()->hasOfflinePatches());
}

void AiAssist_Test::configImportChoiceGoesBackToTheModel()
{
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    const QString file = QStringLiteral(AIASSIST_SAMPLES "/evo26.qxw");
    m_doc->inputOutputMap()->setUniverseName(0, QStringLiteral("House"));
    // No onCollision for a universe that has a setup: not proposed; the
    // model is told what to choose, and asks again with it.
    m_server->queueJson(kChat, toolReply("import_config", QJsonObject { { "file", file }, { "universes", QJsonArray { 1 } } }));
    m_server->queueJson(kChat, toolReply("import_config", QJsonObject { { "file", file }, { "universes", QJsonArray { 1 } },
                                                                        { "onCollision", "renumber" }, { "onMissingPlugin", "keep" } }));
    m_server->queueJson(kChat, textReply("Put on a new universe, as universe 1 is in use."));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(m_ai->ask("bring in universe 1 of evo26.qxw", ctx));
    QVERIFY(answered.wait(5000));
    const AiAssist::Answer a = answered.first().first().value<AiAssist::Answer>();
    QCOMPARE(a.proposal.count(), 1);
    const QJsonArray msgs = m_server->requests.at(1).json().value("messages").toArray();
    const QString told = msgs.last().toObject().value("content").toString();
    QVERIFY2(told.contains("already has a setup") && told.contains("renumber"), qPrintable(told));
    const quint32 before = m_doc->inputOutputMap()->universesCount();
    ShowTreeCommand::Result r;
    QVERIFY2(m_ai->apply(QList<int>(), nullptr, &r), qPrintable(r.message));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), before + 1);
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(0), QStringLiteral("House"));
    QCOMPARE(m_doc->inputOutputMap()->getUniverseNameByIndex(int(before)), QStringLiteral("Front"));
    QVERIFY(m_doc->showTree()->undoShared());
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), before);
}

void AiAssist_Test::configImportNeverInOperate()
{
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    const QString file = QStringLiteral(AIASSIST_SAMPLES "/evo26.qxw");
    m_server->queueJson(kChat, toolReply("import_config", QJsonObject { { "file", file }, { "onMissingPlugin", "keep" } }));
    m_server->queueJson(kChat, textReply("ok"));
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(m_ai->ask("import evo26", ctx));
    QVERIFY(answered.wait(5000));
    QVERIFY(m_ai->hasProposal());
    const quint32 universes = m_doc->inputOutputMap()->universesCount();
    // Operate drops the proposal and refuses the change.
    m_doc->setMode(Doc::Operate);
    QVERIFY(!m_ai->hasProposal());
    ShowTreeCommand::Result r;
    QVERIFY(!m_ai->apply(QList<int>(), nullptr, &r));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), universes);
    QVERIFY(!m_doc->inputOutputMap()->hasOfflinePatches());
    m_doc->setMode(Doc::Design);
}

void AiAssist_Test::goldenPrompts_data()
{
    // Prompt -> the model's tool calls -> the commands proposed. Written in
    // the shape Ollama's /v1 sends (kRecordedToolCall is a real one), on
    // smallShow(); each applies and undoes on it.
    QTest::addColumn<QString>("prompt");
    QTest::addColumn<QByteArray>("reply");
    QTest::addColumn<QStringList>("commands");
    const QJsonArray towers { "Towers" };
    auto row = [](const char *name, const QString &prompt, const QList<QPair<QString, QJsonObject>> &calls,
                  const QStringList &commands) {
        QTest::newRow(name) << prompt << toolsReply(calls) << commands;
    };
    row("blue", "towers blue", { { "set_look", QJsonObject { { "targets", towers }, { "looks", QJsonArray { look("colour", "Blue") } } } } },
        { "Towers colour Blue" });
    row("level", "steps at 75", { { "set_intensity", QJsonObject { { "targets", QJsonArray { "Steps" } }, { "level", 75 } } } },
        { "Steps dimmer 75" });
    row("hex", "towers deep red", { { "set_look", QJsonObject { { "targets", towers }, { "looks", QJsonArray { look("colour", "#8b0000") } } } } },
        { "Towers colour #8b0000" });
    row("both", "towers and steps blue as one layer",
        { { "set_look", QJsonObject { { "targets", QJsonArray { "Towers", "Steps" } }, { "looks", QJsonArray { look("colour", "Blue") } } } } },
        { "Towers Steps colour Blue" });
    row("each", "towers and steps at 40 each",
        { { "set_look", QJsonObject { { "targets", QJsonArray { "Towers", "Steps" } }, { "each", true },
                                      { "looks", QJsonArray { look("dimmer", 40) } } } } },
        { "Towers, Steps dimmer 40" });
    row("cue named", "make cue 3 called Bridge, block",
        { { "make_cue", QJsonObject { { "number", "3" }, { "name", "Bridge" }, { "mode", "block" } } } },
        { "cue 3 \"Bridge\" block" });
    row("cue and look", "cue 3 towers blue",
        { { "make_cue", QJsonObject { { "number", "3" } } },
          { "set_look", QJsonObject { { "targets", towers }, { "looks", QJsonArray { look("colour", "Blue") } } } } },
        { "cue 3", "Towers colour Blue" });
    row("cue after", "a new cue after this one called Outro with towers at 20",
        { { "add_cue", QJsonObject { { "name", "Outro" } } },
          { "set_intensity", QJsonObject { { "targets", towers }, { "level", 20 } } } },
        { "cue + \"Outro\"", "Towers dimmer 20" });
    row("mode", "make this cue cue-only", { { "set_cue_mode", QJsonObject { { "mode", "cueonly" } } } }, { "cueonly" });
    row("skip", "skip cue 2", { { "skip_cues", QJsonObject { { "cues", QJsonArray { "2" } } } } }, { "skip 2" });
    row("dup", "duplicate this cue", { { "duplicate_cue", QJsonObject() } }, { "dup" });
    row("record", "record what's selected as a new cue", { { "record_cue", QJsonObject { { "source", "selected" } } } },
        { "record selected" });
    row("name", "towers at full and call the layer Hero",
        { { "set_intensity", QJsonObject { { "targets", towers }, { "level", 100 } } },
          { "rename_selected", QJsonObject { { "name", "Hero" } } } },
        { "Towers dimmer 100", "name \"Hero\"" });
    row("function", "play Haze in this cue", { { "play_function", QJsonObject { { "functions", QJsonArray { "Haze" } } } } },
        { "function Haze" });
    row("set", "make a set All of towers and steps",
        { { "make_group_set", QJsonObject { { "name", "All" }, { "groups", QJsonArray { "Towers", "Steps" } } } } },
        { "set \"All\" Towers Steps" });
    row("list", "a new cue list Encore with cue 1",
        { { "make_cue_list", QJsonObject { { "name", "Encore" } } }, { "make_cue", QJsonObject { { "number", "1" } } } },
        { "list \"Encore\"", "cue 1" });
    row("release", "towers at 60 and release them after",
        { { "set_intensity", QJsonObject { { "targets", towers }, { "level", 60 } } },
          { "release_layer", QJsonObject { { "targets", towers } } } },
        { "Towers dimmer 60", "release Towers" });
}

void AiAssist_Test::goldenPrompts()
{
    QFETCH(QString, prompt);
    QFETCH(QByteArray, reply);
    QFETCH(QStringList, commands);
    enable();
    const ShowTreeCommand::Context ctx = smallShow();
    ShowTreeController *st = m_doc->showTree();
    const QByteArray before = treeXml();
    const QString undoBefore = st->sharedUndoText();
    propose(m_server, reply);
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QVERIFY(m_ai->ask(prompt, ctx));
    QVERIFY(answered.wait(5000));
    const AiAssist::Answer a = answered.first().first().value<AiAssist::Answer>();
    QVERIFY2(a.ok, qPrintable(a.text));
    QStringList got;
    for (const AiAssist::Step &s : a.proposal)
        got << s.command;
    QCOMPARE(got, commands);
    QCOMPARE(treeXml(), before);
    ShowTreeCommand::Result r;
    QVERIFY2(m_ai->apply(QList<int>(), nullptr, &r), qPrintable(r.message));
    QVERIFY(treeXml() != before);
    QVERIFY(st->sharedUndoText().startsWith("AI Assist"));
    // One undo step takes the whole plan back, exactly.
    QVERIFY(st->undoShared());
    QCOMPARE(treeXml(), before);
    QCOMPARE(st->sharedUndoText(), undoBefore);
}

/* A real model, run by hand only (the gate never sets it):
   QLC_AI_LIVE_OLLAMA=http://localhost:11434 [QLC_AI_LIVE_MODEL=…] ./aiassist_test liveOllamaSmoke
   Asks the P1 acceptance question on a show with one overlap, then goes to
   Operate and checks the model was unloaded. */
void AiAssist_Test::liveOllamaSmoke()
{
    const QString endpoint = qEnvironmentVariable("QLC_AI_LIVE_OLLAMA");
    if (endpoint.isEmpty())
        QSKIP("set QLC_AI_LIVE_OLLAMA to run against a real Ollama");
    AiSettings s;
    s.enabled = true;
    s.endpoint = endpoint;
    if (qEnvironmentVariableIsSet("QLC_AI_LIVE_MODEL"))
        s.model = qEnvironmentVariable("QLC_AI_LIVE_MODEL");
    s.timeoutSec = 300;
    s.save();
    m_ai->settingsChanged();
    overlappingPars();
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QElapsedTimer t;
    t.start();
    QVERIFY(m_ai->ask("find address conflicts", ShowTreeCommand::Context()));
    QVERIFY(answered.wait(300000));
    const AiAssist::Answer a = answered.first().first().value<AiAssist::Answer>();
    qInfo().noquote() << "model:" << s.model << "in" << t.elapsed() << "ms";
    qInfo().noquote() << "commands:" << a.commands.join(", ");
    qInfo().noquote() << "answer:" << a.text;
    QVERIFY2(a.ok, qPrintable(a.text));
    QVERIFY(a.commands.contains("conflicts"));
    QSignalSpy done(m_ai, &AiAssist::teardownFinished);
    QSignalSpy err(m_ai, &AiAssist::resourceError);
    t.restart();
    m_doc->setMode(Doc::Operate);
    qInfo().noquote() << "switch to Operate took" << t.elapsed() << "ms";
    QVERIFY(done.wait(30000));
    qInfo().noquote() << "teardown + audit done in" << t.elapsed() << "ms; clean:" << done.first().first().toBool()
                      << (err.isEmpty() ? QString() : err.first().first().toString());
    QVERIFY(done.first().first().toBool());
}

/* P2 against a real model, by hand only (same rules as liveOllamaSmoke):
   asks for a change on smallShow(), checks a proposal comes back that
   changes nothing until applied, applies it and undoes it in one step. */
void AiAssist_Test::liveOllamaChangeSmoke()
{
    const QString endpoint = qEnvironmentVariable("QLC_AI_LIVE_OLLAMA");
    if (endpoint.isEmpty())
        QSKIP("set QLC_AI_LIVE_OLLAMA to run against a real Ollama");
    AiSettings s;
    s.enabled = true;
    s.endpoint = endpoint;
    if (qEnvironmentVariableIsSet("QLC_AI_LIVE_MODEL"))
        s.model = qEnvironmentVariable("QLC_AI_LIVE_MODEL");
    s.timeoutSec = 300;
    s.save();
    m_ai->settingsChanged();
    const ShowTreeCommand::Context ctx = smallShow();
    ShowTreeController *st = m_doc->showTree();
    const QByteArray before = treeXml();
    QSignalSpy answered(m_ai, &AiAssist::answered);
    QElapsedTimer t;
    t.start();
    const QString prompt = qEnvironmentVariable("QLC_AI_LIVE_PROMPT",
                                                QStringLiteral("In this cue, set Towers to the Blue palette at 50 percent."));
    QVERIFY(m_ai->ask(prompt, ctx));
    QVERIFY(answered.wait(300000));
    const AiAssist::Answer a = answered.first().first().value<AiAssist::Answer>();
    qInfo().noquote() << "model:" << s.model << "in" << t.elapsed() << "ms";
    qInfo().noquote() << "read:" << a.commands.join(", ");
    for (const AiAssist::Step &step : a.proposal)
        qInfo().noquote() << "proposed:" << step.command << "--" << step.description;
    qInfo().noquote() << "answer:" << a.text;
    QVERIFY2(a.ok, qPrintable(a.text));
    QVERIFY(!a.proposal.isEmpty());
    QCOMPARE(treeXml(), before);
    ShowTreeCommand::Result r;
    QVERIFY2(m_ai->apply(QList<int>(), nullptr, &r), qPrintable(r.message));
    qInfo().noquote() << "applied:" << r.message;
    QVERIFY(st->undoShared());
    QCOMPARE(treeXml(), before);
    QSignalSpy done(m_ai, &AiAssist::teardownFinished);
    m_doc->setMode(Doc::Operate);
    QVERIFY(done.wait(30000));
    qInfo().noquote() << "Operate teardown clean:" << done.first().first().toBool();
    QVERIFY(done.first().first().toBool());
}

QTEST_MAIN(AiAssist_Test)

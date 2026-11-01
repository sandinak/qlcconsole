/*
  Q Light Controller Plus - Unit test
  operationregistry_test.cpp

  The Operation Registry (docs/design/AI_ASSIST.md section 4, P0/P1): the
  catalog is whole, every operation's command-line text parses back to the
  same call, each read-only question answers right on a tiny show, and the
  Show Tree command line runs through it with its behaviour unchanged.

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
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>

#include "operationregistry_test.h"
#include "operationregistry.h"
#include "showtreecontroller.h"
#include "showtreecommand.h"
#include "fixturegroup.h"
#include "showreview.h"
#include "collection.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

typedef OperationRegistry R;

void OperationRegistry_Test::init()
{
    m_doc = new Doc(this);
}

void OperationRegistry_Test::cleanup()
{
    R::setFunctionUseProbe(std::function<bool(quint32)>());
    delete m_doc;
    m_doc = nullptr;
}

Fixture *OperationRegistry_Test::dimmer(const QString &name, quint32 address, quint32 channels)
{
    Fixture *fx = new Fixture(m_doc);
    fx->setName(name);
    fx->setChannels(channels);
    fx->setUniverse(0);
    fx->setAddress(address);
    if (!m_doc->addFixture(fx))
        return nullptr;
    return fx;
}

void OperationRegistry_Test::catalogIsWhole()
{
    QSet<QString> names, words;
    for (const R::Operation &op : R::catalog())
    {
        QVERIFY2(!names.contains(op.name), qPrintable(op.name));
        names.insert(op.name);
        QVERIFY(!op.title.isEmpty() && !op.description.isEmpty());
        QCOMPARE(op.schema.value("type").toString(), QStringLiteral("object"));
        QVERIFY(op.validate && op.execute && op.cliSyntax && op.describe);
        if (op.name == QLatin1String("command"))
        {
            QVERIFY(!op.readOnly);
            continue;
        }
        if (!op.readOnly && op.name == QLatin1String("import_config"))
        {
            // P2b: the configuration import also has a command-line form
            // (`import config <file> ... apply`): it previews until "apply".
            QVERIFY(op.cliWord == QLatin1String("import") && op.parse);
            QVERIFY(op.schema.value("additionalProperties") == QJsonValue(false));
            QVERIFY(ShowTreeCommand::word(op.cliWord) == nullptr);
            continue;
        }
        if (!op.readOnly && (op.name == QLatin1String("set_group_master") || op.name == QLatin1String("set_hit_override")))
        {
            // 0.4 leftovers: the playing words (`master Backs 50`, `override`)
            QVERIFY(op.parse && !op.cliWord.isEmpty() && op.cliHelp.startsWith(op.cliWord));
            QVERIFY(op.schema.value("additionalProperties") == QJsonValue(false));
            QVERIFY(ShowTreeCommand::word(op.cliWord) == nullptr);
            QVERIFY(!words.contains(op.cliWord));
            words.insert(op.cliWord);
            QCOMPARE(op.live, op.name == QLatin1String("set_group_master"));
            continue;
        }
        if (!op.readOnly)
        {
            // P2: a change word -- strict schema, no command-line word of
            // its own (a typed line is the grammar's, unchanged).
            QVERIFY2(op.cliWord.isEmpty() && !op.parse, qPrintable(op.name));
            QVERIFY2(op.schema.value("additionalProperties") == QJsonValue(false), qPrintable(op.name));
            continue;
        }
        // P1: every other operation is a read-only question with a word.
        QVERIFY2(op.readOnly, qPrintable(op.name));
        if (op.name == QLatin1String("preview_config_import"))
            continue;       // P2b: the assistant's preview; the typed form is `import config`
        QVERIFY2(!op.cliWord.isEmpty() && op.parse && op.cliHelp.startsWith(op.cliWord), qPrintable(op.name));
        QVERIFY2(!words.contains(op.cliWord), qPrintable(op.cliWord));
        words.insert(op.cliWord);
        // A registry word never shadows a word of the Show Tree grammar.
        QVERIFY2(ShowTreeCommand::word(op.cliWord) == nullptr, qPrintable(op.cliWord));
    }
    for (const char *n : { "command", "list_fixtures", "list_groups", "list_functions", "describe_function",
                           "list_cues", "find_conflicts", "find_unused_functions", "find_problems", "show_summary",
                           "read_config", "preview_config_import" })
        QVERIFY2(names.contains(QLatin1String(n)), n);
    for (const char *n : { "make_cue", "add_cue", "make_cue_list", "set_look", "set_cue_look", "set_intensity",
                           "set_cue_mode", "release_layer", "skip_cues", "unskip_cues", "record_cue",
                           "rename_selected", "duplicate_cue", "include_cue", "make_group_set", "play_function",
                           "set_effect_fade",
                           "import_config", "set_group_master", "set_hit_override" })
        QVERIFY2(names.contains(QLatin1String(n)), n);
    // A group master is playing, not editing: not a step of an AI plan.
    int live = 0;
    for (const R::Operation &op : R::catalog())
        live += op.live ? 1 : 0;
    QCOMPARE(live, 1);
    QCOMPARE(R::readOnlyOperations().count() + R::changeOperations().count(), R::catalog().count() - 1 - live);
    for (const R::Operation *op : R::changeOperations())
        QVERIFY(op->name != QLatin1String("command") && !op->live);
}

void OperationRegistry_Test::cliRoundTrip_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QJsonObject>("args");
    QTest::newRow("fixtures") << "list_fixtures" << QJsonObject();
    QTest::newRow("fixtures filter") << "list_fixtures" << QJsonObject { { "filter", "Front Wash" } };
    QTest::newRow("groups") << "list_groups" << QJsonObject();
    QTest::newRow("functions") << "list_functions" << QJsonObject();
    QTest::newRow("functions type") << "list_functions" << QJsonObject { { "type", "chaser" } };
    QTest::newRow("functions both") << "list_functions"
                                    << QJsonObject { { "type", "scene" }, { "filter", "Verse 2" } };
    QTest::newRow("describe") << "describe_function" << QJsonObject { { "function", "Big Look" } };
    QTest::newRow("cues") << "list_cues" << QJsonObject();
    QTest::newRow("cues production") << "list_cues" << QJsonObject { { "production", "Act One" } };
    QTest::newRow("why") << "why_on" << QJsonObject { { "fixture", "Spot 3" } };
    QTest::newRow("conflicts") << "find_conflicts" << QJsonObject();
    QTest::newRow("unused") << "find_unused_functions" << QJsonObject();
    QTest::newRow("review") << "find_problems" << QJsonObject();
    QTest::newRow("review min") << "find_problems" << QJsonObject { { "min", "warning" } };
    QTest::newRow("summary") << "show_summary" << QJsonObject();
    QTest::newRow("master") << "set_group_master" << QJsonObject { { "group", "Backs" }, { "level", 50 } };
    QTest::newRow("master quoted") << "set_group_master" << QJsonObject { { "group", "Front Wash" }, { "level", 0 } };
    QTest::newRow("override") << "set_hit_override" << QJsonObject();
    QTest::newRow("override off") << "set_hit_override" << QJsonObject { { "on", false } };
}

void OperationRegistry_Test::cliRoundTrip()
{
    QFETCH(QString, name);
    QFETCH(QJsonObject, args);
    const R::Operation *op = R::operation(name);
    QVERIFY(op != nullptr);
    R reg(m_doc);
    const QString line = op->cliSyntax(args);
    QVERIFY(line.startsWith(op->cliWord));
    QJsonObject back;
    QString why;
    QCOMPARE(reg.resolveLine(line, &back, &why), name);
    QVERIFY2(why.isEmpty(), qPrintable(why));
    QCOMPARE(back, args);
    QVERIFY(!op->describe(args).isEmpty());
}

void OperationRegistry_Test::unknownOrInvalidRunsNothing()
{
    R reg(m_doc);
    R::Result r = reg.run("no_such_op", QJsonObject());
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("no_such_op"));
    r = reg.run("describe_function", QJsonObject { { "function", "Nothing Here" } });
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("Nothing Here"));
    r = reg.run("list_functions", QJsonObject { { "type", "teapot" } });
    QVERIFY(!r.ok);
    // A command-line word that doesn't parse: said, nothing run.
    QJsonObject args;
    QString why;
    QCOMPARE(reg.resolveLine("conflicts now", &args, &why), QStringLiteral("find_conflicts"));
    QVERIFY(!why.isEmpty());
    const ShowTreeCommand::Result res = reg.runLine("conflicts now", ShowTreeCommand::Context());
    QVERIFY(!res.ok);
    QVERIFY(res.message.startsWith("conflicts:"));
    QVERIFY(res.help.isEmpty());
}

void OperationRegistry_Test::listFixtures()
{
    QVERIFY(dimmer("Front Wash 1", 0) != nullptr);
    QVERIFY(dimmer("Back Light", 10) != nullptr);
    R reg(m_doc);
    R::Result r = reg.run("list_fixtures", QJsonObject());
    QVERIFY(r.ok);
    QCOMPARE(r.text.split('\n').count(), 2);
    QVERIFY(r.text.contains("Front Wash 1"));
    QVERIFY(r.text.contains("universe 1, 001-004"));
    QVERIFY(r.text.contains("universe 1, 011-014"));
    r = reg.run("list_fixtures", QJsonObject { { "filter", "back" } });
    QCOMPARE(r.text.split('\n').count(), 1);
    QVERIFY(r.text.startsWith("Back Light"));
}

void OperationRegistry_Test::listGroups()
{
    Fixture *a = dimmer("A", 0);
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Towers");
    g->setPath("Stage/Left");
    m_doc->addFixtureGroup(g);
    g->assignFixture(a->id());
    R reg(m_doc);
    const R::Result r = reg.run("list_groups", QJsonObject());
    QVERIFY(r.ok);
    QVERIFY2(r.text.contains("Stage/Left/Towers"), qPrintable(r.text));
    QVERIFY(r.text.contains("1 fixture"));
}

void OperationRegistry_Test::listFunctions()
{
    Scene *s = new Scene(m_doc);
    s->setName("Verse Wash");
    m_doc->addFunction(s);
    Chaser *c = new Chaser(m_doc);
    c->setName("Verse Chase");
    m_doc->addFunction(c);
    R reg(m_doc);
    R::Result r = reg.run("list_functions", QJsonObject());
    QVERIFY(r.ok);
    QCOMPARE(r.text.split('\n').count(), 2);
    r = reg.run("list_functions", QJsonObject { { "type", "chaser" } });
    QCOMPARE(r.text.split('\n').count(), 1);
    QVERIFY(r.text.startsWith("Verse Chase"));
    r = reg.run("list_functions", QJsonObject { { "filter", "wash" } });
    QVERIFY(r.text.startsWith("Verse Wash"));
}

void OperationRegistry_Test::describeFunction()
{
    Scene *a = new Scene(m_doc);
    a->setName("Look A");
    m_doc->addFunction(a);
    Scene *b = new Scene(m_doc);
    b->setName("Look B");
    m_doc->addFunction(b);
    Chaser *c = new Chaser(m_doc);
    c->setName("Song");
    m_doc->addFunction(c);
    c->addStep(ChaserStep(a->id()));
    c->addStep(ChaserStep(b->id()));
    R reg(m_doc);

    R::Result r = reg.run("describe_function", QJsonObject { { "function", "song" } });
    QVERIFY(r.ok);
    QVERIFY2(r.text.contains("2 steps"), qPrintable(r.text));
    QVERIFY(r.text.contains("1. Look A"));
    QVERIFY(r.text.contains("2. Look B"));
    r = reg.run("describe_function", QJsonObject { { "function", "Look A" } });
    QVERIFY(r.text.contains("Used by: Song"));
    r = reg.run("describe_function", QJsonObject { { "function", QStringLiteral("#%1").arg(b->id()) } });
    QVERIFY(r.ok);
    QVERIFY(r.text.contains("Look B"));

    // Two with one name: asks for the number.
    Scene *twin = new Scene(m_doc);
    twin->setName("Look A");
    m_doc->addFunction(twin);
    r = reg.run("describe_function", QJsonObject { { "function", "Look A" } });
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("#"));
}

void OperationRegistry_Test::findConflictsLeavesTheReviewAlone()
{
    Fixture *a = dimmer("Par 1", 0);
    Fixture *b = dimmer("Par 2", 10);
    QVERIFY(a != nullptr && b != nullptr);
    R reg(m_doc);
    R::Result r = reg.run("find_conflicts", QJsonObject());
    QVERIFY(r.ok);
    QCOMPARE(r.message, QStringLiteral("No address conflicts"));
    b->setAddress(2);                        // overlaps Par 1's 3-4
    QSignalSpy reviewed(m_doc->showReview(), &ShowReview::reviewed);
    r = reg.run("find_conflicts", QJsonObject());
    QVERIFY(r.ok);
    QVERIFY2(r.text.contains("PAT-01"), qPrintable(r.text));
    QVERIFY(r.text.contains("Par 1") && r.text.contains("Par 2"));
    QVERIFY(r.message.contains("1"));
    // A question: the panel's result (and the footer's) stays as it was.
    QCOMPARE(reviewed.count(), 0);
    QVERIFY(!m_doc->showReview()->lastResult().ran);
}

void OperationRegistry_Test::findUnusedFunctions()
{
    Scene *used = new Scene(m_doc);
    used->setName("In Chase");
    m_doc->addFunction(used);
    Scene *alone = new Scene(m_doc);
    alone->setName("Alone");
    m_doc->addFunction(alone);
    Scene *onButton = new Scene(m_doc);
    onButton->setName("On A Button");
    m_doc->addFunction(onButton);
    Scene *inCue = new Scene(m_doc);
    inCue->setName("In A Cue");
    m_doc->addFunction(inCue);
    Collection *col = new Collection(m_doc);
    col->setName("Holder");
    m_doc->addFunction(col);
    col->addFunction(used->id());

    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Show");
    const quint32 list = st->addCueList(prod, "Main");
    const quint32 cue = st->addCue(list, -1, "One");
    ShowTree::Target t;
    t.function = inCue->id();
    QVERIFY(st->addLayer(cue, t) != 0xFFFFFFFF);

    R reg(m_doc);
    R::Result r = reg.run("find_unused_functions", QJsonObject());
    QVERIFY(r.ok);
    QVERIFY2(r.text.contains("Alone"), qPrintable(r.text));
    QVERIFY(!r.text.contains("In Chase"));
    QVERIFY(!r.text.contains("In A Cue"));
    QVERIFY(r.text.contains("Virtual Console widgets weren't checked"));

    R::setFunctionUseProbe([onButton](quint32 id) { return id == onButton->id(); });
    r = reg.run("find_unused_functions", QJsonObject());
    QVERIFY(!r.text.contains("On A Button"));
    QVERIFY(!r.text.contains("weren't checked"));
    QVERIFY(r.text.contains("Alone"));

    // The Virtual Console as the VcIndex sees it: no probe needed.
    R::setFunctionUseProbe(std::function<bool(quint32)>());
    m_doc->showReview()->setVcXml(QStringLiteral(
        "<Workspace><VirtualConsole><Frame Caption=\"\"><Button Caption=\"B\" ID=\"1\">"
        "<Function ID=\"%1\"/><Action>Toggle</Action></Button></Frame></VirtualConsole></Workspace>")
        .arg(onButton->id()).toUtf8());
    r = reg.run("find_unused_functions", QJsonObject());
    QVERIFY(!r.text.contains("On A Button"));
    QVERIFY(!r.text.contains("weren't checked"));
    QVERIFY(r.text.contains("Alone"));
    m_doc->showReview()->setVcXml(QByteArray("<Workspace><VirtualConsole/></Workspace>"));
    r = reg.run("find_unused_functions", QJsonObject());
    QVERIFY(r.text.contains("On A Button"));            // an empty console: nothing runs it
}

void OperationRegistry_Test::findProblems()
{
    dimmer("Twin", 0);
    dimmer("Twin", 20);
    R reg(m_doc);
    R::Result r = reg.run("find_problems", QJsonObject());
    QVERIFY(r.ok);
    QVERIFY2(r.text.contains("FIX-14"), qPrintable(r.text));    // duplicate names
    r = reg.run("find_problems", QJsonObject { { "min", "blocker" } });
    QVERIFY(r.ok);
    QVERIFY(!r.text.contains("FIX-14"));
    QVERIFY(!m_doc->showReview()->lastResult().ran);
}

void OperationRegistry_Test::listCues()
{
    R reg(m_doc);
    R::Result r = reg.run("list_cues", QJsonObject());
    QVERIFY(!r.ok);                          // no Show Tree yet

    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Act One");
    const quint32 list = st->addCueList(prod, "Main");
    st->addCue(list, -1, "Opening");
    st->addCue(list, 0, "Blackout");
    r = reg.run("list_cues", QJsonObject());
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY2(r.text.contains("Opening") && r.text.contains("Blackout"), qPrintable(r.text));
    QVERIFY(r.text.indexOf("Opening") < r.text.indexOf("Blackout"));
    QVERIFY(r.message.contains("Act One"));
    r = reg.run("list_cues", QJsonObject { { "production", "act one" } });
    QVERIFY(r.ok);
    r = reg.run("list_cues", QJsonObject { { "production", "Act Two" } });
    QVERIFY(!r.ok);
}

void OperationRegistry_Test::whyOn()
{
    Fixture *a = dimmer("Spot 1", 0);
    QVERIFY(a != nullptr);
    R reg(m_doc);
    R::Result r = reg.run("why_on", QJsonObject { { "fixture", "spot 1" } });
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(!r.text.isEmpty());
    r = reg.run("why_on", QJsonObject { { "fixture", "Spot 9" } });
    QVERIFY(!r.ok);
}

void OperationRegistry_Test::showSummary()
{
    dimmer("A", 0);
    dimmer("B", 4);
    Scene *s = new Scene(m_doc);
    s->setName("S");
    m_doc->addFunction(s);
    R reg(m_doc);
    const R::Result r = reg.run("show_summary", QJsonObject());
    QVERIFY(r.ok);
    QVERIFY2(r.text.contains("Fixtures: 2 (2 patched, on 1 universe"), qPrintable(r.text));
    QVERIFY(r.text.contains("1 Scene"));
    QVERIFY(r.text.contains("Mode: Design"));
}

void OperationRegistry_Test::commandLineGoesThroughTheRegistry()
{
    ShowTreeCommand direct(m_doc, m_doc->showTree());
    ShowTreeCommand viaRegistry(m_doc, m_doc->showTree());
    R reg(m_doc, &viaRegistry);
    const QStringList lines { "undo", "redo", "help spread", "colour Blue", "nonsense words here",
                              "list \"Main\"" };
    for (const QString &line : lines)
    {
        QJsonObject args;
        QCOMPARE(reg.resolveLine(line, &args), QStringLiteral("command"));
        const ShowTreeCommand::Result a = direct.run(line, ShowTreeCommand::Context());
        const ShowTreeCommand::Result b = reg.runLine(line, ShowTreeCommand::Context());
        QCOMPARE(b.ok, a.ok);
        QCOMPARE(b.message, a.message);
        QCOMPARE(b.help, a.help);
        QCOMPARE(b.context.list, a.context.list);
    }

    // A registry word: its operation, the answer in the help pane.
    dimmer("Par 1", 0);
    const ShowTreeCommand::Result r = reg.runLine("fixtures", ShowTreeCommand::Context());
    QVERIFY(r.ok);
    QVERIFY(r.keepSelection);
    QVERIFY(r.help.contains("Par 1"));
}

void OperationRegistry_Test::theShowsNamesWin()
{
    // A group called "Summary" is the grammar's target, as before.
    Fixture *a = dimmer("A", 0);
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Summary");
    m_doc->addFixtureGroup(g);
    g->assignFixture(a->id());
    ShowTreeCommand cmd(m_doc, m_doc->showTree());
    R reg(m_doc, &cmd);
    QJsonObject args;
    QCOMPARE(reg.resolveLine("summary colour Red", &args), QStringLiteral("command"));
    QCOMPARE(reg.resolveLine("summary", &args), QStringLiteral("command"));
    QCOMPARE(reg.resolveLine("conflicts", &args), QStringLiteral("find_conflicts"));
}

void OperationRegistry_Test::helpListsTheQuestions()
{
    ShowTreeCommand cmd(m_doc, m_doc->showTree());
    R reg(m_doc, &cmd);
    ShowTreeCommand::Result r = reg.runLine("help", ShowTreeCommand::Context());
    QVERIFY(r.ok);
    QVERIFY(r.help.contains("Questions"));
    QVERIFY(r.help.contains("ex:conflicts"));
    r = reg.runLine("?", ShowTreeCommand::Context());
    QVERIFY(r.help.contains("ex:summary"));
    // One word's help is the grammar's alone.
    r = reg.runLine("help spread", ShowTreeCommand::Context());
    QVERIFY(!r.help.contains("Questions"));
}

void OperationRegistry_Test::commandNeedsACommandLine()
{
    R reg(m_doc);
    const R::Result r = reg.run("command", QJsonObject { { "line", "undo" } });
    QVERIFY(!r.ok);
    QVERIFY(!r.message.isEmpty());
}

void OperationRegistry_Test::longAnswersAreCut()
{
    for (int i = 0; i < 400; ++i)
        QVERIFY(dimmer(QStringLiteral("A fixture with a long name number %1").arg(i), i, 1) != nullptr);
    R reg(m_doc);
    const R::Result r = reg.run("list_fixtures", QJsonObject());
    QVERIFY(r.ok);
    QVERIFY(r.text.size() <= R::MaxText + 80);
    QVERIFY(r.text.endsWith("(cut: the answer was longer)"));
}

/****************************************************************************
 * P2: the change words
 ****************************************************************************/

ShowTreeCommand::Context OperationRegistry_Test::smallShow()
{
    auto group = [this](const QString &name, const QList<Fixture *> &fixtures) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        m_doc->addFixtureGroup(g);
        for (Fixture *fx : fixtures)
            g->assignFixture(fx->id());
    };
    group("Towers", { dimmer("T1", 0), dimmer("T2", 10) });
    group("Steps", { dimmer("S1", 20) });
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

QByteArray OperationRegistry_Test::treeXml() const
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.writeStartDocument();
    m_doc->showTree()->saveXML(&w);
    w.writeEndDocument();
    // The id counter only counts up (an undo never hands an id out twice).
    return QString::fromUtf8(out).remove(QRegularExpression(" NextId=\"\\d+\"")).toUtf8();
}

void OperationRegistry_Test::changeRenders_data()
{
    // The grammar line each call stands for: what the preview shows and
    // what runs.
    QTest::addColumn<QString>("name");
    QTest::addColumn<QJsonObject>("args");
    QTest::addColumn<QString>("line");
    QTest::newRow("cue") << "make_cue" << QJsonObject { { "number", "1.0.1" } } << "cue 1.0.1";
    QTest::newRow("cue named") << "make_cue" << QJsonObject { { "number", "3" }, { "name", "Big Finish" }, { "mode", "block" } }
                               << "cue 3 \"Big Finish\" block";
    QTest::newRow("cue +") << "add_cue" << QJsonObject { { "name", "Chorus" } } << "cue + \"Chorus\"";
    QTest::newRow("cue + bare") << "add_cue" << QJsonObject() << "cue +";
    QTest::newRow("list") << "make_cue_list" << QJsonObject { { "name", "01-Opener" } } << "list \"01-Opener\"";
    QTest::newRow("look") << "set_look"
                          << QJsonObject { { "targets", QJsonArray { "Towers" } },
                                           { "looks", QJsonArray { QJsonObject { { "attribute", "colour" }, { "value", "Blue" } },
                                                                   QJsonObject { { "attribute", "dimmer" }, { "value", 45 } } } } }
                          << "Towers colour Blue dimmer 45";
    QTest::newRow("look one layer") << "set_look"
                                    << QJsonObject { { "targets", QJsonArray { "Towers", "Front Wash" } },
                                                     { "looks", QJsonArray { QJsonObject { { "attribute", "colour" }, { "value", "Deep Blue" } } } } }
                                    << "Towers \"Front Wash\" colour \"Deep Blue\"";
    QTest::newRow("look each") << "set_look"
                               << QJsonObject { { "targets", QJsonArray { "Towers", "Steps" } }, { "each", true },
                                                { "looks", QJsonArray { QJsonObject { { "attribute", "gobo" }, { "value", "none" } } } } }
                               << "Towers, Steps gobo none";
    QTest::newRow("cue look") << "set_cue_look"
                              << QJsonObject { { "looks", QJsonArray { QJsonObject { { "attribute", "spread" }, { "value", "+5" } } } } }
                              << "cue look spread +5";
    QTest::newRow("intensity") << "set_intensity" << QJsonObject { { "targets", QJsonArray { "Movers" } }, { "level", 50 } }
                               << "Movers dimmer 50";
    QTest::newRow("mode") << "set_cue_mode" << QJsonObject { { "mode", "cueonly" } } << "cueonly";
    QTest::newRow("release") << "release_layer" << QJsonObject { { "targets", QJsonArray { "Towers" } } } << "release Towers";
    QTest::newRow("skip") << "skip_cues" << QJsonObject { { "cues", QJsonArray { "0.0.3", "0.0.7" } } } << "skip 0.0.3, 0.0.7";
    QTest::newRow("skip selected") << "skip_cues" << QJsonObject() << "skip";
    QTest::newRow("unskip") << "unskip_cues" << QJsonObject { { "cues", QJsonArray { "2" } } } << "unskip 2";
    QTest::newRow("record") << "record_cue" << QJsonObject { { "source", "running" } } << "record running";
    QTest::newRow("name") << "rename_selected" << QJsonObject { { "name", "Hero" } } << "name \"Hero\"";
    QTest::newRow("name auto") << "rename_selected" << QJsonObject { { "name", "" } } << "name \"\"";
    QTest::newRow("dup") << "duplicate_cue" << QJsonObject() << "dup";
    QTest::newRow("include") << "include_cue" << QJsonObject { { "number", "0.0.3" } } << "include 0.0.3";
    QTest::newRow("set") << "make_group_set" << QJsonObject { { "name", "Towers" }, { "groups", QJsonArray { "SL", "SR" } } }
                         << "set \"Towers\" SL SR";
    QTest::newRow("effect fade both") << "set_effect_fade"
                                      << QJsonObject { { "effect", "Sinelon" }, { "targets", QJsonArray { "Towers" } },
                                                       { "fade_in", 2 }, { "fade_out", 0.5 } }
                                      << "Towers effect Sinelon fade in 2 out 0.5";
    QTest::newRow("effect fade cue") << "set_effect_fade"
                                     << QJsonObject { { "effect", "Big Sine" }, { "with", "cue" } }
                                     << "cue look effect \"Big Sine\" fade cue";
    QTest::newRow("function") << "play_function" << QJsonObject { { "functions", QJsonArray { "Haze", "Big Look" } } }
                              << "function Haze, \"Big Look\"";
}

void OperationRegistry_Test::changeRenders()
{
    QFETCH(QString, name);
    QFETCH(QJsonObject, args);
    QFETCH(QString, line);
    const R::Operation *op = R::operation(name);
    QVERIFY(op != nullptr);
    QVERIFY2(R::checkArgs(op->schema, args).isEmpty(), qPrintable(R::checkArgs(op->schema, args)));
    QCOMPARE(op->cliSyntax(args), line);
    QVERIFY(!op->describe(args).isEmpty());
}

void OperationRegistry_Test::schemaRejectsBadArguments_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QJsonObject>("args");
    QTest::addColumn<QString>("why");
    const QJsonArray towers { "Towers" };
    QTest::newRow("above max") << "set_intensity" << QJsonObject { { "targets", towers }, { "level", 140 } } << "above 100";
    QTest::newRow("not integer") << "set_intensity" << QJsonObject { { "targets", towers }, { "level", 4.5 } } << "integer";
    QTest::newRow("string level") << "set_intensity" << QJsonObject { { "targets", towers }, { "level", "half" } } << "integer";
    QTest::newRow("missing") << "set_intensity" << QJsonObject { { "targets", towers } } << "level: missing";
    QTest::newRow("no targets") << "set_intensity" << QJsonObject { { "targets", QJsonArray() }, { "level", 5 } } << "at least 1";
    QTest::newRow("extra") << "set_intensity" << QJsonObject { { "targets", towers }, { "level", 5 }, { "fade", 3 } }
                           << "fade: not an argument";
    QTest::newRow("enum") << "set_cue_mode" << QJsonObject { { "mode", "loop" } } << "one of track";
    QTest::newRow("cue number") << "make_cue" << QJsonObject { { "number", "1; undo" } } << "right form";
    QTest::newRow("attribute") << "set_look"
                               << QJsonObject { { "targets", towers },
                                                { "looks", QJsonArray { QJsonObject { { "attribute", "smell" }, { "value", "x" } } } } }
                               << "looks[0].attribute";
    QTest::newRow("quote") << "rename_selected" << QJsonObject { { "name", "a\" record" } } << "double quote";
    QTest::newRow("record source") << "record_cue" << QJsonObject() << "source: missing";
    QTest::newRow("no function") << "play_function" << QJsonObject { { "functions", QJsonArray { "Nope" } } } << "Nope";
    QTest::newRow("fade bad") << "set_effect_fade" << QJsonObject { { "effect", "Sinelon" }, { "fade_in", 700 } } << "above 600";
    QTest::newRow("fade no effect") << "set_effect_fade" << QJsonObject { { "effect", "Nope" }, { "with", "cue" } } << "No effect";
    QTest::newRow("fade enum") << "set_effect_fade" << QJsonObject { { "effect", "Sinelon" }, { "with", "slow" } } << "one of cue";
    QTest::newRow("unknown") << "patch_fixture" << QJsonObject() << "No operation";
}

void OperationRegistry_Test::schemaRejectsBadArguments()
{
    QFETCH(QString, name);
    QFETCH(QJsonObject, args);
    QFETCH(QString, why);
    const ShowTreeCommand::Context ctx = smallShow();
    const QByteArray before = treeXml();
    const QString undoBefore = m_doc->showTree()->sharedUndoText();
    ShowTreeCommand cmd(m_doc, m_doc->showTree());
    R reg(m_doc, &cmd);
    QVERIFY(reg.check(name, args).contains(why));
    const R::Result r = reg.run(name, args, ctx);
    QVERIFY(!r.ok);
    QVERIFY2(r.message.contains(why), qPrintable(r.message));
    // Refused before it ran: nothing changed, no undo step.
    QCOMPARE(treeXml(), before);
    QCOMPARE(m_doc->showTree()->sharedUndoText(), undoBefore);
    // In a plan the same: nothing at all runs.
    const R::Result p = reg.runPlan({ { "make_cue", QJsonObject { { "number", "9" } } }, { name, args } }, ctx, "test");
    QVERIFY(!p.ok);
    QVERIFY2(p.message.startsWith("Step 2"), qPrintable(p.message));
    QCOMPARE(treeXml(), before);
}

void OperationRegistry_Test::changeRunsItsGrammarLine()
{
    // One change on its own is the line it renders: one undo step.
    const ShowTreeCommand::Context ctx = smallShow();
    ShowTreeCommand cmd(m_doc, m_doc->showTree());
    R reg(m_doc, &cmd);
    const R::Result r = reg.run("set_intensity", QJsonObject { { "targets", QJsonArray { "Towers" } }, { "level", 50 } }, ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(r.command.layer != ShowTree::invalidId());
    QCOMPARE(m_doc->showTree()->cue(ctx.cue)->layers.count(), 1);
    // Without a command line: refused, as `command` is.
    R bare(m_doc);
    QVERIFY(!bare.run("duplicate_cue", QJsonObject(), ctx).ok);
}

void OperationRegistry_Test::effectFadesByWords()
{
    const ShowTreeCommand::Context ctx = smallShow();
    for (const char *n : { "Sinelon", "Big Sine" })
    {
        QLCPalette *fx = new QLCPalette(QLCPalette::Effect, m_doc);
        fx->setName(n);
        m_doc->addPalette(fx);
    }
    QLCPalette *red = new QLCPalette(QLCPalette::Color, m_doc);
    red->setName("Red");
    red->setValue(QLCPalette::colorToString(QColor(Qt::red), QColor(Qt::black)));
    m_doc->addPalette(red);
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    R reg(m_doc, &cmd);
    auto effectLook = [&]() -> ShowTree::Look {
        const ShowTree::Cue *c = st->cue(ctx.cue);
        for (const ShowTree::Layer &l : c->layers)
            for (const ShowTree::Look &lk : l.looks)
                if (ShowTree::isEffectKey(lk.key))
                    return lk;
        return ShowTree::Look();
    };
    // A new effect follows the cue's fades.
    ShowTreeCommand::Result made = cmd.run("Towers effect Sinelon", ctx);
    QVERIFY2(made.ok, qPrintable(made.message));
    QCOMPARE(effectLook().fadeIn, int(ShowTree::Look::FadeFollowCue));

    // Own times: one undo step; the effect is kept.
    const QString undoBefore = st->sharedUndoText();
    R::Result r = reg.run("set_effect_fade", QJsonObject { { "effect", "Sinelon" }, { "targets", QJsonArray { "Towers" } },
                                                           { "fade_in", 2 }, { "fade_out", 0.5 } }, ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(effectLook().fadeIn, 2000);
    QCOMPARE(effectLook().fadeOut, 500);
    QVERIFY(st->undoShared());
    QCOMPARE(effectLook().fadeIn, int(ShowTree::Look::FadeFollowCue));
    QCOMPARE(st->sharedUndoText(), undoBefore);
    QVERIFY(st->redoShared());
    QCOMPARE(effectLook().fadeIn, 2000);

    // One side alone leaves the other.
    r = reg.run("set_effect_fade", QJsonObject { { "effect", "Sinelon" }, { "targets", QJsonArray { "Towers" } },
                                                 { "fade_out", 1 } }, ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(effectLook().fadeIn, 2000);
    QCOMPARE(effectLook().fadeOut, 1000);

    // At once, then the cue's again.
    r = reg.run("set_effect_fade", QJsonObject { { "effect", "Sinelon" }, { "targets", QJsonArray { "Towers" } },
                                                 { "with", "atonce" } }, ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(effectLook().fadeIn, int(ShowTree::Look::FadeSnap));
    QCOMPARE(effectLook().fadeOut, int(ShowTree::Look::FadeSnap));
    r = reg.run("set_effect_fade", QJsonObject { { "effect", "Sinelon" }, { "targets", QJsonArray { "Towers" } },
                                                 { "with", "cue" } }, ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(effectLook().fadeOut, int(ShowTree::Look::FadeFollowCue));

    // Typed, after the effect and its own looks; fades alone keep the colour.
    QVERIFY(cmd.run("Towers effect Sinelon colour Red fade 3", ctx).ok);
    QCOMPARE(effectLook().fadeIn, 3000);
    QCOMPARE(effectLook().fadeOut, 3000);
    const int ownBefore = effectLook().own.count();
    QVERIFY(cmd.run("Towers effect Sinelon fade in 1 out 4", ctx).ok);
    QCOMPARE(effectLook().own.count(), ownBefore);
    QCOMPARE(effectLook().fadeIn, 1000);
    QCOMPARE(effectLook().fadeOut, 4000);

    // The whole cue.
    r = reg.run("set_effect_fade", QJsonObject { { "effect", "Big Sine" }, { "fade_in", 1.5 } }, ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    bool found = false;
    for (const ShowTree::Look &lk : st->cue(ctx.cue)->looks)
        if (ShowTree::isEffectKey(lk.key))
        {
            found = true;
            QCOMPARE(lk.fadeIn, 1500);
        }
    QVERIFY(found);

    // Tab offers fade after an effect, and reads its words.
    QStringList cands;
    QVERIFY(cmd.candidates("Towers effect Sinelon ", ctx, &cands));
    QVERIFY(cands.contains("fade"));
    QVERIFY(cmd.candidates("Towers effect Sinelon fade ", ctx, &cands));
    QVERIFY(cands.contains("atonce"));
    QVERIFY(cmd.candidates("Towers effect Sinelon fade in 2 out 1 ", ctx, &cands));

    // 'with' or times, one of them.
    QVERIFY(reg.check("set_effect_fade", QJsonObject { { "effect", "Sinelon" } }).contains("either"));
    QVERIFY(reg.check("set_effect_fade", QJsonObject { { "effect", "Sinelon" }, { "with", "cue" }, { "fade_in", 1 } })
                .contains("either"));

    // Words that don't fit: refused, nothing changed.
    QVERIFY(!cmd.run("Towers effect Sinelon fade", ctx).ok);
    QVERIFY(!cmd.run("Towers effect Sinelon fade in x", ctx).ok);
    QVERIFY(!cmd.run("Towers colour Red fade 2", ctx).ok);

    // Design only.
    m_doc->setMode(Doc::Operate);
    const QByteArray operateBefore = treeXml();
    r = reg.runPlan({ { "set_effect_fade", QJsonObject { { "effect", "Sinelon" }, { "with", "cue" } } } }, ctx, "t");
    QVERIFY(!r.ok);
    QCOMPARE(treeXml(), operateBefore);
    m_doc->setMode(Doc::Design);
}

void OperationRegistry_Test::planIsOneUndoStep()
{
    const ShowTreeCommand::Context ctx = smallShow();
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    R reg(m_doc, &cmd);
    const QByteArray before = treeXml();
    const QString undoBefore = st->sharedUndoText();
    const int cuesBefore = st->cueList(ctx.list)->cues.count();

    const QList<R::Call> plan {
        { "add_cue", QJsonObject { { "name", "Bridge" }, { "mode", "block" } } },
        { "set_look", QJsonObject { { "targets", QJsonArray { "Towers" } },
                                    { "looks", QJsonArray { QJsonObject { { "attribute", "dimmer" }, { "value", 80 } } } } } },
        { "rename_selected", QJsonObject { { "name", "Hero" } } },
        { "set_intensity", QJsonObject { { "targets", QJsonArray { "Steps" } }, { "level", 30 } } },
    };
    const R::Result r = reg.runPlan(plan, ctx, "AI Assist: bridge");
    QVERIFY2(r.ok, qPrintable(r.message));
    // The steps ran where the one before left the selection: in the new cue.
    const quint32 bridge = r.command.context.cue;
    QVERIFY(bridge != ctx.cue);
    QCOMPARE(st->cueList(ctx.list)->cues.count(), cuesBefore + 1);
    const ShowTree::Cue *c = st->cue(bridge);
    QCOMPARE(c->name, QStringLiteral("Bridge"));
    QCOMPARE(c->layers.count(), 2);
    QCOMPARE(c->layers.first().name, QStringLiteral("Hero"));
    // ONE undo step, named for the plan.
    QCOMPARE(st->sharedUndoText(), QStringLiteral("AI Assist: bridge"));
    QVERIFY(st->undoShared());
    QCOMPARE(st->sharedUndoText(), undoBefore);
    QCOMPARE(treeXml(), before);
    // ...and redo puts it all back.
    QVERIFY(st->redoShared());
    QCOMPARE(st->cueList(ctx.list)->cues.count(), cuesBefore + 1);
}

void OperationRegistry_Test::planFailingPartWayChangesNothing()
{
    const ShowTreeCommand::Context ctx = smallShow();
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    R reg(m_doc, &cmd);
    const QByteArray before = treeXml();
    const QString undoBefore = st->sharedUndoText();
    const int palettes = m_doc->palettes().count();
    // Fits every schema; the grammar refuses step 3 (no such group).
    const R::Result r = reg.runPlan({ { "add_cue", QJsonObject { { "name", "New" } } },
                                      { "set_intensity", QJsonObject { { "targets", QJsonArray { "Towers" } }, { "level", 70 } } },
                                      { "set_intensity", QJsonObject { { "targets", QJsonArray { "Nobody" } }, { "level", 70 } } } },
                                    ctx, "test");
    QVERIFY(!r.ok);
    QVERIFY2(r.message.startsWith("Step 3"), qPrintable(r.message));
    QVERIFY(r.message.contains("Nothing changed"));
    QCOMPARE(treeXml(), before);
    QCOMPARE(st->sharedUndoText(), undoBefore);
    QVERIFY(!st->inBatch());
    QCOMPARE(m_doc->palettes().count(), palettes);   // the level's one-off palette went too
}

void OperationRegistry_Test::planRefusals()
{
    const ShowTreeCommand::Context ctx = smallShow();
    ShowTreeCommand cmd(m_doc, m_doc->showTree());
    R reg(m_doc, &cmd);
    const QByteArray before = treeXml();
    const QList<R::Call> plan { { "duplicate_cue", QJsonObject() } };
    // A question isn't a change; nor is the raw command line.
    QVERIFY(!reg.runPlan({ { "find_conflicts", QJsonObject() } }, ctx, "t").ok);
    QVERIFY(!reg.runPlan({ { "command", QJsonObject { { "line", "dup" } } } }, ctx, "t").ok);
    QVERIFY(!reg.runPlan({}, ctx, "t").ok);
    // No command line: refused.
    QVERIFY(!R(m_doc).runPlan(plan, ctx, "t").ok);
    // Operate: never.
    m_doc->setMode(Doc::Operate);
    const R::Result r = reg.runPlan(plan, ctx, "t");
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("Operate"));
    m_doc->setMode(Doc::Design);
    QCOMPARE(treeXml(), before);
    QVERIFY(reg.runPlan(plan, ctx, "t").ok);
}

QTEST_MAIN(OperationRegistry_Test)

/****************************************************************************
 * 0.4 leftovers: the playing words
 ****************************************************************************/

void OperationRegistry_Test::masterWordMovesAGroupMaster()
{
    smallShow();
    FixtureGroup *towers = m_doc->fixtureGroups().first();
    QCOMPARE(towers->name(), QStringLiteral("Towers"));
    towers->setMaster(true);
    R reg(m_doc);
    ShowTreeCommand::Result r = reg.runLine("master Towers 50", ShowTreeCommand::Context());
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(int(towers->masterLevel()), 128);
    QVERIFY(r.message.contains("50%"));
    QVERIFY(reg.runLine("master towers 25%", ShowTreeCommand::Context()).ok);
    QCOMPARE(int(towers->masterLevel()), 64);
    QVERIFY(reg.runLine("master Towers off", ShowTreeCommand::Context()).ok);
    QCOMPARE(int(towers->masterLevel()), 0);
    QVERIFY(reg.runLine("master Towers full", ShowTreeCommand::Context()).ok);
    QCOMPARE(int(towers->masterLevel()), 255);
    // a quoted name with a space, and one typed without quotes
    FixtureGroup *wash = new FixtureGroup(m_doc);
    wash->setName(QStringLiteral("Front Wash"));
    m_doc->addFixtureGroup(wash);
    wash->setMaster(true);
    QVERIFY(reg.runLine("master \"Front Wash\" 10", ShowTreeCommand::Context()).ok);
    QCOMPARE(int(wash->masterLevel()), 26);
    QVERIFY(reg.runLine("master Front Wash 100", ShowTreeCommand::Context()).ok);
    QCOMPARE(int(wash->masterLevel()), 255);
    // as an operation, the same
    QVERIFY(reg.run("set_group_master", QJsonObject { { "group", "Towers" }, { "level", 100 } }).ok);
    // moving it is not an edit: no undo step on the Show Tree's history
    const QString undo = m_doc->showTree()->sharedUndoText();
    QVERIFY(reg.runLine("master Towers 30", ShowTreeCommand::Context()).ok);
    QCOMPARE(m_doc->showTree()->sharedUndoText(), undo);
}

void OperationRegistry_Test::masterWordRefusals()
{
    smallShow();
    R reg(m_doc);
    ShowTreeCommand::Result r = reg.runLine("master Towers 50", ShowTreeCommand::Context());
    QVERIFY(!r.ok);                 // a group without a master: said, not guessed
    QVERIFY2(r.message.contains("no master"), qPrintable(r.message));
    r = reg.runLine("master Nobody 50", ShowTreeCommand::Context());
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("No group called"));
    r = reg.runLine("master Towers", ShowTreeCommand::Context());
    QVERIFY(!r.ok);
    QVERIFY(r.message.startsWith("master:"));
    r = reg.runLine("master Towers loud", ShowTreeCommand::Context());
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("isn't a level"));
    m_doc->fixtureGroups().first()->setMaster(true);
    r = reg.runLine("master Towers 140", ShowTreeCommand::Context());
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("above 100"));
    QCOMPARE(int(m_doc->fixtureGroups().first()->masterLevel()), 255);
    // the show's own names win: a group called "master" is the grammar's
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName(QStringLiteral("master"));
    m_doc->addFixtureGroup(g);
    QJsonObject args;
    QCOMPARE(reg.resolveLine("master 50", &args), QStringLiteral("command"));
}

void OperationRegistry_Test::masterIsPlayingNotEditing()
{
    smallShow();
    FixtureGroup *towers = m_doc->fixtureGroups().first();
    towers->setMaster(true);
    R reg(m_doc);
    // Operate mode locks editing, not playing
    m_doc->setMode(Doc::Operate);
    QVERIFY(reg.runLine("master Towers 20", ShowTreeCommand::Context()).ok);
    QCOMPARE(int(towers->masterLevel()), 51);
    m_doc->setMode(Doc::Design);
    // and an AI plan can't carry it
    ShowTreeCommand cmd(m_doc, m_doc->showTree());
    R withCmd(m_doc, &cmd);
    const R::Result p = withCmd.runPlan({ { "set_group_master", QJsonObject { { "group", "Towers" }, { "level", 5 } } } },
                                        ShowTreeCommand::Context(), "test");
    QVERIFY(!p.ok);
    QVERIFY(p.message.contains("not a change operation"));
    QCOMPARE(int(towers->masterLevel()), 51);
}

void OperationRegistry_Test::overrideWordMakesTheSelectedHitAnOverride()
{
    ShowTreeCommand::Context ctx = smallShow();
    ShowTreeController *st = m_doc->showTree();
    const quint32 hit = st->addHit(ShowTree::HitScope::Production, ctx.production, "Blackout");
    QVERIFY(st->isLook(hit));
    QVERIFY(!st->isOverrideHit(hit));
    ctx.cue = hit;
    ShowTreeCommand cmd(m_doc, st);
    R reg(m_doc, &cmd);
    ShowTreeCommand::Result r = reg.runLine("override", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(st->isOverrideHit(hit));
    QCOMPARE(r.context.cue, hit);        // the selection stays
    // again: said, nothing to do
    r = reg.runLine("override on", ctx);
    QVERIFY(r.ok);
    QVERIFY(r.message.contains("already"));
    // one undo step takes it back
    st->undo();
    QVERIFY(!st->isOverrideHit(hit));
    QVERIFY(reg.runLine("override", ctx).ok);
    QVERIFY(reg.runLine("override off", ctx).ok);
    QVERIFY(!st->isOverrideHit(hit));
    // a plan can carry it (it is an edit, one undo step)
    const R::Result p = reg.runPlan({ { "set_hit_override", QJsonObject() } }, ctx, "Override");
    QVERIFY2(p.ok, qPrintable(p.message));
    QVERIFY(st->isOverrideHit(hit));
    st->undo();
    QVERIFY(!st->isOverrideHit(hit));
}

void OperationRegistry_Test::overrideWordRefusals()
{
    ShowTreeCommand::Context ctx = smallShow();
    ShowTreeController *st = m_doc->showTree();
    R reg(m_doc);
    // a cue of a cue list is not a hit
    ShowTreeCommand::Result r = reg.runLine("override", ctx);
    QVERIFY(!r.ok);
    QVERIFY2(r.message.contains("Select a hit"), qPrintable(r.message));
    ctx.cue = 0xFFFFFFFF;
    QVERIFY(!reg.runLine("override", ctx).ok);
    r = reg.runLine("override maybe", ctx);
    QVERIFY(!r.ok);
    QVERIFY(r.message.startsWith("override:"));
    // not in Operate: editing is locked
    ctx.cue = st->addHit(ShowTree::HitScope::Production, ctx.production, "Blackout");
    m_doc->setMode(Doc::Operate);
    r = reg.runLine("override", ctx);
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains("Operate"));
    QVERIFY(!st->isOverrideHit(ctx.cue));
    m_doc->setMode(Doc::Design);
}

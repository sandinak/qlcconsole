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
#include <QJsonObject>
#include <QSet>

#include "operationregistry_test.h"
#include "operationregistry.h"
#include "showtreecontroller.h"
#include "showtreecommand.h"
#include "fixturegroup.h"
#include "showreview.h"
#include "collection.h"
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
        // P1: every other operation is a read-only question with a word.
        QVERIFY2(op.readOnly, qPrintable(op.name));
        QVERIFY2(!op.cliWord.isEmpty() && op.parse && op.cliHelp.startsWith(op.cliWord), qPrintable(op.name));
        QVERIFY2(!words.contains(op.cliWord), qPrintable(op.cliWord));
        words.insert(op.cliWord);
        // A registry word never shadows a word of the Show Tree grammar.
        QVERIFY2(ShowTreeCommand::word(op.cliWord) == nullptr, qPrintable(op.cliWord));
    }
    for (const char *n : { "command", "list_fixtures", "list_groups", "list_functions", "describe_function",
                           "list_cues", "find_conflicts", "find_unused_functions", "find_problems", "show_summary" })
        QVERIFY2(names.contains(QLatin1String(n)), n);
    QCOMPARE(R::readOnlyOperations().count(), R::catalog().count() - 1);
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

QTEST_MAIN(OperationRegistry_Test)

/*
  Q Light Controller
  script_test.cpp

  Copyright (C) Heikki Junnila

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

#define private public
#define protected public

#include "qlcfixturedefcache.h"
#include "mastertimer.h"
#include "script_test.h"
#include "universe.h"
#include "script.h"
#include "doc.h"
#include "scene.h"
#include "fixture.h"
#include "inputoutputmap.h"
#include "qlcfile.h"

#undef private
#undef protected

static QString script0(
"// Comment over there\n"
"startfunction:12\r"
"stopfunction:33 paska\n"
"waitkey:\"SHIFT+K\"\n"
"startfunction:\"54\"\r\n"
"wait:1.05\n"
"setfixture:99 value:255 channel:1\n"
"jump:label1\n"
"wait:1.05\n"
"label:label1\n"
"blackout:on\n"
"systemcommand:echo arg:asdf\n"
"blackout:off\n"
);

void Script_Test::initTestCase()
{
    /* Script::programsAllowed() is a QSettings value: a settings file of
       the test's own, never the user's */
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
}

void Script_Test::init()
{
    m_doc = new Doc(this);
}

void Script_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}
void Script_Test::initial()
{
    Doc doc(this);
    GrandMaster *gm = new GrandMaster();
    QList<Universe*> ua;
    ua.append(new Universe(0, gm));
    ua.append(new Universe(1, gm));
    ua.append(new Universe(2, gm));
    ua.append(new Universe(3, gm));

    Script scr(&doc);
    scr.setData(script0);
    scr.start(doc.masterTimer(), FunctionParent::master());
    scr.preRun(doc.masterTimer());

    scr.write(doc.masterTimer(), ua);

    scr.postRun(doc.masterTimer(), ua);
}

/* The level of address @p address in universe 0, after the faders ran */
static int level(Doc *doc, int address)
{
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    ua.first()->processFaders();
    const int v = uchar(ua.first()->preGMValues().at(address));
    doc->inputOutputMap()->releaseUniverses(false);
    return v;
}

static void tick(Doc *doc, int n = 1)
{
    for (int i = 0; i < n; i++)
        doc->masterTimer()->timerTick();
}

void Script_Test::linesAreTokenizedAndBadOnesNamed()
{
    Script scr(m_doc);
    scr.setData("// a comment\n"
                "startfunction:3\r\n"
                "wait:1.5 // the rest is a comment\n"
                "\n"
                "systemcommand:http://example.com arg:\"two words\"\n"
                "nocolon\n"
                "waitkey:\"unterminated\n"
                "setfixture:7 ch:2 val:200\n"
                "wait:250\n");

    // Line numbers count from 1, blank lines included
    QCOMPARE(scr.syntaxErrorsLines(), QList<int>({ 6, 7 }));
    QCOMPARE(scr.dataLines().count(), 9);
    QCOMPARE(scr.dataLines().at(1), QString("startfunction:3"));

    // A URL's // is not a comment; quotes hold spaces
    const QList<QStringList> sys = scr.m_lines.at(4 - 1);
    QCOMPARE(sys.count(), 2);
    QCOMPARE(sys.at(0), QStringList({ "systemcommand", "http://example.com" }));
    QCOMPARE(sys.at(1), QStringList({ "arg", "two words" }));
    QCOMPARE(scr.m_lines.at(2 - 1).count(), 1);       // the comment is gone
    QVERIFY(scr.m_lines.at(0).first().isEmpty());     // a comment line is empty

    // What it uses: (id, line) pairs
    QCOMPARE(scr.functionList(), QList<quint32>({ 3, 1 }));
    QCOMPARE(scr.fixtureList(), QList<quint32>({ 7, 6 }));
    QCOMPARE(scr.totalDuration(), 1500u + 250u);
}

void Script_Test::aScriptSavesAndLoadsAsWritten()
{
    Script scr(m_doc);
    scr.setName("Loop & \"quote\"");
    const QString text = "label:top\n"
                         "systemcommand:/bin/echo arg:\"100% <done> & more\"\n"
                         "setfixture:0 ch:0 val:random(10,20)\n"
                         "wait:0.5\n"
                         "jump:top\n";
    scr.setData(text);

    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    QVERIFY(scr.saveXML(&w));
    w.writeEndDocument();
    buf.close();

    QXmlStreamReader r(buf.data());
    r.readNextStartElement();
    Script back(m_doc);
    QVERIFY(back.loadXML(r));
    QCOMPARE(back.dataLines(), scr.dataLines());
    QCOMPARE(back.data(), text);
    QCOMPARE(back.m_labels.value("top", -1), 0);       // jumps work after a load
    QCOMPARE(back.totalDuration(), 500u);

    // Not a script: refused
    QXmlStreamReader notScript(QByteArray("<Function Type=\"Scene\" ID=\"1\"/>"));
    notScript.readNextStartElement();
    Script other(m_doc);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("is not a script"));
    QVERIFY(!other.loadXML(notScript));
    QXmlStreamReader notFunction(QByteArray("<Scene/>"));
    notFunction.readNextStartElement();
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Function node not found"));
    QVERIFY(!other.loadXML(notFunction));

    // A copy is the same script
    Script *copy = qobject_cast<Script *>(scr.createCopy(m_doc, false));
    QVERIFY(copy != nullptr);
    QCOMPARE(copy->data(), text);
    delete copy;
}

void Script_Test::functionsStartStopAndStopOnExit()
{
    Fixture *fxi = new Fixture(m_doc);
    fxi->setChannels(2);
    m_doc->addFixture(fxi);
    Scene *a = new Scene(m_doc);
    a->setValue(fxi->id(), 0, 100);
    Scene *b = new Scene(m_doc);
    b->setValue(fxi->id(), 1, 100);
    m_doc->addFunction(a);
    m_doc->addFunction(b);

    const uint t = MasterTimer::tick();
    Script *scr = new Script(m_doc);
    m_doc->addFunction(scr);
    scr->setData(QString("startfunction:%1\nstartfunction:%2\nstopfunction:%2\n"
                         "startfunction:999\nstopfunction:abc\nwait:%3\n")
                 .arg(a->id()).arg(b->id()).arg(t * 5));
    scr->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 3);
    QVERIFY(a->isRunning());
    QVERIFY(!b->isRunning());
    QCOMPARE(level(m_doc, 0), 100);
    // Its last line done, the script ends and stops what it started
    tick(m_doc, 8);
    QVERIFY(!scr->isRunning());
    QVERIFY(!a->isRunning());

    // stoponexit:false leaves it running
    scr->setData(QString("stoponexit:false\nstartfunction:%1\nwait:%2\n").arg(a->id()).arg(t));
    scr->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 6);
    QVERIFY(!scr->isRunning());
    QVERIFY(a->isRunning());
    a->stop(FunctionParent::master());
    tick(m_doc, 2);
}

void Script_Test::setFixtureWritesItsChannel()
{
    Fixture *fxi = new Fixture(m_doc);
    fxi->setAddress(10);
    fxi->setUniverse(0);
    fxi->setChannels(4);
    m_doc->addFixture(fxi);
    Fixture *staged = new Fixture(m_doc);
    staged->setChannels(1);
    staged->setUniverse(7);                          // no such universe here
    m_doc->addFixture(staged);

    Script *scr = new Script(m_doc);
    m_doc->addFunction(scr);
    scr->setData(QString("setfixture:%1 ch:2 val:200\n"
                         "setfixture:%1 ch:0 val:random(7,7)\n"
                         "setfixture:%1 ch:9 val:50\n"               // past its channels
                         "setfixture:%2 ch:0 val:50\n"               // not patched here
                         "setfixture:4242 ch:0 val:50\n"             // no such fixture
                         "setfixture:%1 ch:1 colour:50\n"            // a syntax error
                         "setfixture:%1 chanel:0 val:255\n"          // a typo
                         "wait:1s\n").arg(fxi->id()).arg(staged->id()));
    QCOMPARE(scr->syntaxErrorsLines(), QList<int>({ 6, 7 }));
    scr->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 3);
    QCOMPARE(level(m_doc, 12), 200);
    /* A line with a syntax error doesn't run: it used to run what came
       before the error, a setfixture with no ch/val -- channel 0 to 0 */
    QCOMPARE(level(m_doc, 10), 7);
    QCOMPARE(level(m_doc, 11), 0);
    QCOMPARE(level(m_doc, 13), 0);
    QCOMPARE(level(m_doc, 0), 0);

    // Stopped: its channels let go
    scr->stop(FunctionParent::master());
    tick(m_doc, 3);
    QVERIFY(!scr->isRunning());
    QCOMPARE(level(m_doc, 12), 0);
}

void Script_Test::waitsJumpsAndBlackout()
{
    Fixture *fxi = new Fixture(m_doc);
    fxi->setAddress(0);
    fxi->setUniverse(0);
    fxi->setChannels(1);
    m_doc->addFixture(fxi);

    const uint t = MasterTimer::tick();
    Script *scr = new Script(m_doc);
    m_doc->addFunction(scr);
    scr->setData(QString("setfixture:%1 ch:0 val:10\n"
                         "wait:%2\n"
                         "setfixture:%1 ch:0 val:20\n"
                         "blackout:on\n"
                         "label:again\n"
                         "wait:%3\n"
                         "jump:again\n").arg(fxi->id()).arg(t * 4).arg(t * 2));
    scr->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc);
    QCOMPARE(level(m_doc, 0), 10);
    tick(m_doc, 3);
    QCOMPARE(level(m_doc, 0), 10);                  // still waiting
    QVERIFY(!m_doc->inputOutputMap()->blackout());
    tick(m_doc, 3);
    QCOMPARE(level(m_doc, 0), 20);
    QVERIFY(m_doc->inputOutputMap()->blackout());

    // The jump loops on the wait: still running long after
    tick(m_doc, 50);
    QVERIFY(scr->isRunning());
    QCOMPARE(scr->m_currentCommand >= 4, true);

    scr->setData("blackout:off\nblackout:maybe\njump:nowhere\n");
    scr->stop(FunctionParent::master());
    tick(m_doc, 2);
    scr->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 6);
    QVERIFY(!m_doc->inputOutputMap()->blackout());
    QVERIFY(!scr->isRunning());                     // a bad jump goes on, then ends
}

void Script_Test::waitingForAFunction()
{
    Fixture *fxi = new Fixture(m_doc);
    fxi->setAddress(0);
    fxi->setUniverse(0);
    fxi->setChannels(2);
    m_doc->addFixture(fxi);
    Scene *a = new Scene(m_doc);
    a->setValue(fxi->id(), 1, 100);                 // something to hold it running
    m_doc->addFunction(a);

    Script *scr = new Script(m_doc);
    m_doc->addFunction(scr);
    scr->setData(QString("waitfunctionstart:%1\n"
                         "setfixture:%2 ch:0 val:30\n"
                         "waitfunctionstop:%1\n"
                         "setfixture:%2 ch:0 val:40\n"
                         "wait:1s\n").arg(a->id()).arg(fxi->id()));
    scr->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 5);
    QCOMPARE(level(m_doc, 0), 0);                   // waits for the start
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 3);
    QCOMPARE(level(m_doc, 0), 30);                  // ...then for the stop
    tick(m_doc, 3);
    QCOMPARE(level(m_doc, 0), 30);
    a->stop(FunctionParent::master());
    tick(m_doc, 3);
    QCOMPARE(level(m_doc, 0), 40);

    // A function already running is not waited for; no such one is skipped
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 2);
    scr->stop(FunctionParent::master());
    tick(m_doc, 2);
    scr->setData(QString("waitfunctionstart:%1\nwaitfunctionstart:999\nsetfixture:%2 ch:0 val:50\nwait:1s\n")
                 .arg(a->id()).arg(fxi->id()));
    scr->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 4);
    QCOMPARE(level(m_doc, 0), 50);
    scr->stop(FunctionParent::master());
    a->stop(FunctionParent::master());
    tick(m_doc, 2);
}

void Script_Test::anUnreadableWaitDoesNotHang()
{
    /* "random(5)" has no upper bound: getValueFromString says it couldn't
       read it, and the wait used the -1 it returned anyway -- 49 days. */
    Script *scr = new Script(m_doc);
    m_doc->addFunction(scr);
    scr->setData("wait:random(5)\n");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("error:Invalid wait time"));
    scr->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 4);
    QVERIFY(!scr->isRunning());

    // random() either way round, and a plain one, still wait
    bool ok = false;
    const quint32 r = scr->getValueFromString("random(300,100)", &ok);
    QVERIFY(ok);
    QVERIFY(r >= 100 && r <= 300);
    QCOMPARE(scr->getValueFromString("2s", &ok), 2000u);
    QCOMPARE(scr->totalDuration(), 0u);
}

void Script_Test::programsRunOnlyWhenAllowed()
{
    Script::setProgramsAllowed(false);
    QVERIFY(!Script::programsAllowed());
    QSignalSpy refused(m_doc, SIGNAL(showActionRefused(QString)));

    Script *scr = new Script(m_doc);
    scr->setName("Lights up");
    m_doc->addFunction(scr);
    scr->setData("systemcommand:/no/such/program arg:x\n");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("did not run /no/such/program: running programs is off"));
    scr->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 3);
    QCOMPARE(refused.count(), 1);
    QVERIFY(refused.first().first().toString().contains("Lights up"));

    // Allowed, but the show came in over web access: still refused
    Script::setProgramsAllowed(true);
    QVERIFY(Script::programsAllowed());
    m_doc->setLoadedFromWeb(true);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("came in over web access"));
    scr->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 3);
    QCOMPARE(refused.count(), 2);
    m_doc->setLoadedFromWeb(false);
    Script::setProgramsAllowed(false);
}

QTEST_MAIN(Script_Test)

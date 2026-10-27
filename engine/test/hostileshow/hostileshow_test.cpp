/*
  Q Light Controller Plus - qlcconsole
  hostileshow_test.cpp

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
#include <QBuffer>
#include <QTemporaryFile>
#include <QXmlStreamReader>
#include <QSettings>
#include <QSignalSpy>

#include "hostilecase.h"

#include "mastertimer.h"
#include "grandmaster.h"
#include "universe.h"
#include "function.h"
#include "collection.h"
#include "chaser.h"
#include "chaserstep.h"
#include "scene.h"
#define private public
#define protected public
#include "script.h"
#undef private
#undef protected
#include "fixture.h"
#include "doc.h"

#include "hostileshow_test.h"

namespace
{
    QList<Universe *> universes(GrandMaster *gm, int n)
    {
        QList<Universe *> ua;
        for (int i = 0; i < n; i++)
            ua.append(new Universe(i, gm));
        return ua;
    }

    void freeUniverses(QList<Universe *> &ua)
    {
        qDeleteAll(ua);
        ua.clear();
    }
}

void HostileShow_Test::initTestCase()
{
    /* Settings of our own: the tests turn "Allow scripts to run programs" on
       and off, never in the user's real preferences. */
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    Script::setProgramsAllowed(false);
}

/* random(min,max) is generate() % ((max+1)-min). With min == max+1 (random(1,0)
   for example) that is a modulo by zero: SIGFPE on x86 (Linux, Windows, Intel
   Mac) -- the app dies mid-show. A crafted script value or fade time hits it.
   FAILS today on x86; arm64 returns 0 (undefined, but no trap). */
void HostileShow_Test::randomWithEqualBoundsDoesNotCrashTheTimer()
{
    HOSTILE_CASE(8000);
    Doc doc(nullptr);
    GrandMaster *gm = new GrandMaster();
    QList<Universe *> ua = universes(gm, 4);

    Fixture *fxi = new Fixture(&doc);
    fxi->setChannels(1);
    fxi->setUniverse(0);
    fxi->setAddress(0);
    doc.addFixture(fxi);

    Script scr(&doc);
    scr.setData(QStringLiteral("setfixture:%1 ch:0 val:random(1,0)").arg(fxi->id()));
    scr.executeCommand(0, doc.masterTimer(), ua);

    freeUniverses(ua);
    delete gm;
}

/* An unpatched fixture (inventory/staged, or an address clash) keeps its
   universe() but does NOT grow the universe list. A script setfixture on it
   indexes universes[universe] out of range -- a wild write on the timer
   thread. FAILS today. */
void HostileShow_Test::setFixtureOnAnUnpatchedFixtureIsSafe()
{
    HOSTILE_CASE(8000);
    Doc doc(nullptr);
    GrandMaster *gm = new GrandMaster();
    QList<Universe *> ua = universes(gm, 4);   // show has 4 universes

    Fixture *fxi = new Fixture(&doc);
    fxi->setChannels(1);
    fxi->setUniverse(40);                       // far past the 4 that exist
    fxi->setAddress(0);
    doc.addFixture(fxi);

    Script scr(&doc);
    scr.setData(QStringLiteral("setfixture:%1 ch:0 val:255").arg(fxi->id()));
    scr.executeCommand(0, doc.masterTimer(), ua);

    freeUniverses(ua);
    delete gm;
}

/* Loading a show must never, on its own, run a systemcommand. Loading is not
   consent to execute. (A separate, stronger ask is that systemcommand be off
   unless the operator turns it on; this test pins the minimum.) We mark a
   file the command would create and check it is absent after the load. */
void HostileShow_Test::aSystemCommandIsNotRunMerelyByLoadingTheShow()
{
    QTemporaryDir tmp;
    const QString marker = tmp.filePath(QStringLiteral("ran.marker"));
    QVERIFY(!QFile::exists(marker));

    const QString xml = QStringLiteral(
        "<!DOCTYPE Workspace>\n"
        "<Workspace>\n"
        " <Engine>\n"
        "  <Function ID=\"5\" Type=\"Script\" Name=\"Init\">\n"
        "   <Command>systemcommand:/usr/bin/touch arg:%1</Command>\n"
        "  </Function>\n"
        " </Engine>\n"
        "</Workspace>\n").arg(marker);

    Doc doc(nullptr);
    QBuffer buf;
    buf.setData(xml.toUtf8());
    buf.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader r(&buf);
    while (!r.atEnd())
        if (r.readNext() == QXmlStreamReader::DTD)
            break;
    doc.loadXML(r);

    QTest::qWait(500);
    QVERIFY2(!QFile::exists(marker),
             "loading a show ran its script's systemcommand");
}

#if defined(Q_OS_UNIX)
static QString touchScript(const QString &marker)
{
    return QStringLiteral("systemcommand:/usr/bin/touch arg:%1").arg(marker);
}
#endif

/* A systemcommand line runs a program only when this computer's operator
   has turned on "Allow scripts to run programs"; off (the default) it is
   skipped and the footer told why (Doc::showActionRefused). */
void HostileShow_Test::aSystemCommandIsOffUntilTheOperatorTurnsItOn()
{
#if !defined(Q_OS_UNIX)
    QSKIP("runs /usr/bin/touch");
#else
    QTemporaryDir tmp;
    const QString marker = tmp.filePath(QStringLiteral("ran.marker"));
    Doc doc(nullptr);
    GrandMaster *gm = new GrandMaster();
    QList<Universe *> ua = universes(gm, 1);
    QSignalSpy refused(&doc, &Doc::showActionRefused);

    Script scr(&doc);
    scr.setName(QStringLiteral("Init"));
    QVERIFY(scr.setData(touchScript(marker)));

    QVERIFY(Script::programsAllowed() == false);        // the default
    scr.executeCommand(0, doc.masterTimer(), ua);
    QTest::qWait(500);
    QVERIFY2(!QFile::exists(marker), "a systemcommand ran with running programs off");
    QCOMPARE(refused.count(), 1);
    QVERIFY(refused.first().first().toString().contains(QStringLiteral("Init")));

    Script::setProgramsAllowed(true);
    scr.executeCommand(0, doc.masterTimer(), ua);
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(marker), 5000);
    QCOMPARE(refused.count(), 1);
    Script::setProgramsAllowed(false);

    freeUniverses(ua);
    delete gm;
#endif
}

/* A show that came in over web access never runs programs, even with the
   setting on; a show opened afresh is trusted again. */
void HostileShow_Test::aShowFromWebAccessNeverRunsPrograms()
{
#if !defined(Q_OS_UNIX)
    QSKIP("runs /usr/bin/touch");
#else
    QTemporaryDir tmp;
    const QString marker = tmp.filePath(QStringLiteral("ran.marker"));
    Doc doc(nullptr);
    GrandMaster *gm = new GrandMaster();
    QList<Universe *> ua = universes(gm, 1);
    QSignalSpy refused(&doc, &Doc::showActionRefused);
    Script::setProgramsAllowed(true);

    doc.setLoadedFromWeb(true);
    Script scr(&doc);
    QVERIFY(scr.setData(touchScript(marker)));
    scr.executeCommand(0, doc.masterTimer(), ua);
    QTest::qWait(500);
    QVERIFY2(!QFile::exists(marker), "a show from web access ran a program");
    QCOMPARE(refused.count(), 1);

    doc.clearContents();
    QVERIFY(doc.loadedFromWeb() == false);
    Script::setProgramsAllowed(false);

    freeUniverses(ua);
    delete gm;
#endif
}

/* random(a,b) with b < a is the same range; random(n,n) is n. */
void HostileShow_Test::randomTakesItsBoundsEitherWayRound()
{
    Doc doc(nullptr);
    Script scr(&doc);
    for (int i = 0; i < 200; i++)
    {
        bool ok = false;
        const quint32 v = scr.getValueFromString(QStringLiteral("random(10,5)"), &ok);
        QVERIFY(ok);
        QVERIFY2(v >= 5 && v <= 10, qPrintable(QString::number(v)));
        QCOMPARE(scr.getValueFromString(QStringLiteral("random(7,7)"), &ok), quint32(7));
    }
}

/* A deeply nested entity definition (billion laughs) must not hang the loader
   or exhaust memory. Qt's reader should stop with an error. */
void HostileShow_Test::aBillionLaughsDocumentDoesNotHangOrOOM()
{
    HOSTILE_CASE(15000);
    QString xml = QStringLiteral("<?xml version=\"1.0\"?>\n<!DOCTYPE Workspace [\n");
    xml += QStringLiteral("<!ENTITY a0 \"loloolloloolloloolloloolloloo\">\n");
    for (int i = 1; i <= 12; i++)
        xml += QStringLiteral("<!ENTITY a%1 \"&a%2;&a%2;&a%2;&a%2;&a%2;&a%2;&a%2;&a%2;&a%2;&a%2;\">\n")
                   .arg(i).arg(i - 1);
    xml += QStringLiteral("]>\n<Workspace><Engine><Function ID=\"1\" Type=\"Scene\" Name=\"&a12;\"/></Engine></Workspace>\n");

    Doc doc(nullptr);
    QBuffer buf;
    buf.setData(xml.toUtf8());
    buf.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader r(&buf);
    while (!r.atEnd())
        r.readNext();                 // drive the whole document
    // Reaching here (within the timeout, without OOM) is the point.
    QVERIFY(true);
}

/* An external entity pointing at a local file must not be resolved into the
   document (classic XXE file read). */
void HostileShow_Test::anExternalEntityIsNotResolved()
{
    QTemporaryFile secret;
    QVERIFY(secret.open());
    secret.write("TOP-SECRET-PASSWORD");
    secret.flush();

    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\"?>\n"
        "<!DOCTYPE Workspace [ <!ENTITY xxe SYSTEM \"file://%1\"> ]>\n"
        "<Workspace><Engine><Function ID=\"1\" Type=\"Scene\" Name=\"&xxe;\"/></Engine></Workspace>\n")
            .arg(secret.fileName());

    Doc doc(nullptr);
    QBuffer buf;
    buf.setData(xml.toUtf8());
    buf.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader r(&buf);
    QString allText;
    while (!r.atEnd())
    {
        r.readNext();
        allText += r.text().toString();
    }
    foreach (Function *f, doc.functions())
        allText += f->name();
    QVERIFY2(!allText.contains("TOP-SECRET-PASSWORD"),
             "an external entity was resolved -- XXE file read");
}

/* A collection whose member list includes its own ID: running it must not
   recurse without end. Function::start guards against re-entry; this pins it. */
void HostileShow_Test::aCollectionThatContainsItselfDoesNotRecurseOnRun()
{
    HOSTILE_CASE(8000);
    Doc doc(nullptr);
    GrandMaster *gm = new GrandMaster();
    QList<Universe *> ua = universes(gm, 1);

    Collection *c = new Collection(&doc);
    c->setName(QStringLiteral("loop"));
    doc.addFunction(c);              // gives it an ID
    c->addFunction(c->id());         // member = itself
    c->addFunction(c->id());

    c->start(doc.masterTimer(), FunctionParent::master());
    c->preRun(doc.masterTimer());
    c->write(doc.masterTimer(), ua);
    c->postRun(doc.masterTimer(), ua);

    freeUniverses(ua);
    delete gm;
}

/* A chaser whose step points at itself: loading and a tick must not recurse. */
void HostileShow_Test::aChaserThatContainsItselfLoadsAndDoesNotRecurse()
{
    HOSTILE_CASE(8000);
    Doc doc(nullptr);

    Chaser *ch = new Chaser(&doc);
    ch->setName(QStringLiteral("selfloop"));
    doc.addFunction(ch);
    ChaserStep step(ch->id());
    ch->addStep(step);
    ch->addStep(ChaserStep(ch->id()));

    QBuffer out;
    out.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter w(&out);
    ch->saveXML(&w);
    out.close();

    out.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader r(&out);
    r.readNextStartElement();
    Chaser *back = new Chaser(&doc);
    back->loadXML(r);
    delete back;
    QVERIFY(true);
}

QTEST_GUILESS_MAIN(HostileShow_Test)

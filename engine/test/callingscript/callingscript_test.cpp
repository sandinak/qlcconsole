/*
  Q Light Controller Plus
  callingscript_test.cpp

  Copyright (C) Branson Matheson

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QTemporaryDir>
#include <QBuffer>
#include <QtTest>

#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "callingscript.h"
#include "doc.h"

#include "callingscript_test.h"

using namespace ShowTree;

/* The calling script: lyrics or a script as text, cues placed on it by id.
   Built offscreen from a small show numbered {S}.{P}.{II}. */

namespace
{
const char *kScript = "# Everybody Scream\n"      // 0..18, line 0
                      "Everybody scream\n"         // line 1
                      "Scream out loud\n"          // line 2
                      "\n"                         // line 3
                      "Hold the note\n";           // line 4, 5 empty

int offsetOf(const QString &needle)
{
    return QString::fromLatin1(kScript).indexOf(needle);
}
} // namespace

void CallingScript_Test::init()
{
    m_doc = new Doc(this);
    ShowTreeController *st = m_doc->showTree();
    m_prod = st->addProduction("Dance Hysteria");
    m_list = st->addCueList(m_prod, "Main");
    QVERIFY(st->setNumbering(m_prod, Numbering::ShowPartIteration));
    m_cues[0] = st->addSection(m_list, "Preshow");              // 0.0.1
    m_cues[1] = st->addCue(m_list, 0, "Walk-in");               // 0.0.2
    m_cues[2] = st->addSection(m_list, "Everybody Scream");     // 1.0.1
    m_cues[3] = st->addCue(m_list, 2, "Hold");                  // 1.0.2
    const char *names[4] = { "House", "Walk-in", "Big hit", "Hold" };
    for (int i = 0; i < 4; ++i)
    {
        Cue c = *st->cue(m_cues[i]);
        c.name = names[i];
        QVERIFY(st->setCue(c));
    }
    QVERIFY(st->setScriptText(m_prod, QString::fromLatin1(kScript), "act1.txt"));
}

void CallingScript_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void CallingScript_Test::aCueIsCalledOnALine()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->anchorCue(m_prod, m_cues[2], offsetOf("Scream out"), ShowTreeController::ScriptMark::Go));
    QVERIFY(st->anchorCue(m_prod, m_cues[2], offsetOf("Everybody scream"), ShowTreeController::ScriptMark::Standby));
    // Refused: no such cue, past the end of the text.
    QVERIFY(!st->anchorCue(m_prod, 9999, 0));
    QVERIFY(!st->anchorCue(m_prod, m_cues[0], 100000));
    // The same place again changes nothing (no undo step).
    QVERIFY(!st->anchorCue(m_prod, m_cues[2], offsetOf("Scream out"), ShowTreeController::ScriptMark::Go));

    const CallingScriptSheet sheet = CallingScriptSheet::build(m_doc, m_prod);
    QCOMPARE(sheet.title(), QString("act1.txt"));
    QCOMPARE(sheet.lines().count(), 6);
    QCOMPARE(sheet.lineOfCue(m_cues[2], true), 2);
    QCOMPARE(sheet.lineOfCue(m_cues[2], false), 1);
    QCOMPARE(sheet.markerCount(), 1);
    const CallingScriptSheet::Marker &m = sheet.lines().at(2).markers.first();
    QVERIFY(m.go);
    QCOMPARE(m.number, QString("1.0.1"));
    QCOMPARE(m.name, QString("Big hit"));

    // Moving it is one call; a mark taken off leaves the other.
    QVERIFY(st->anchorCue(m_prod, m_cues[2], offsetOf("Hold"), ShowTreeController::ScriptMark::Go));
    QCOMPARE(CallingScriptSheet::build(m_doc, m_prod).lineOfCue(m_cues[2], true), 4);
    QVERIFY(st->unanchorCue(m_prod, m_cues[2], ShowTreeController::ScriptMark::Go));
    const CallingScriptSheet after = CallingScriptSheet::build(m_doc, m_prod);
    QCOMPARE(after.lineOfCue(m_cues[2], true), -1);
    QCOMPARE(after.lineOfCue(m_cues[2], false), 1);
    QVERIFY(st->unanchorCue(m_prod, m_cues[2], ShowTreeController::ScriptMark::Standby));
    QVERIFY(st->callingScript(m_prod).anchors.isEmpty());
}

void CallingScript_Test::anchorsSurviveRenumberMoveAndDelete()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->anchorCue(m_prod, m_cues[0], offsetOf("Everybody scream")));
    QVERIFY(st->anchorCue(m_prod, m_cues[1], offsetOf("Scream out")));
    QVERIFY(st->anchorCue(m_prod, m_cues[3], offsetOf("Hold")));

    // Renumbered: the marker stays on its line and shows the new number.
    Cue c = *st->cue(m_cues[3]);
    c.number = "1.0.7";
    QVERIFY(st->setCue(c));
    CallingScriptSheet sheet = CallingScriptSheet::build(m_doc, m_prod);
    QCOMPARE(sheet.lineOfCue(m_cues[3]), 4);
    QCOMPARE(sheet.lines().at(4).markers.first().number, QString("1.0.7"));

    // The whole level renumbered (the list moves cues about): same lines.
    QVERIFY(st->renumberLevel(m_cues[0], 5, 5));
    sheet = CallingScriptSheet::build(m_doc, m_prod);
    QCOMPARE(sheet.lineOfCue(m_cues[0]), 1);
    QCOMPARE(sheet.lineOfCue(m_cues[1]), 2);
    QCOMPARE(sheet.lineOfCue(m_cues[3]), 4);

    // Moved in its list.
    QVERIFY(st->moveCue(m_cues[3], 0));
    sheet = CallingScriptSheet::build(m_doc, m_prod);
    QCOMPARE(sheet.lineOfCue(m_cues[3]), 4);
    QCOMPARE(sheet.markerCount(), 3);

    // Deleted: the marker is gone, the others stay; undo puts it back.
    QVERIFY(st->removeCue(m_cues[1]));
    sheet = CallingScriptSheet::build(m_doc, m_prod);
    QCOMPARE(sheet.lineOfCue(m_cues[1]), -1);
    QCOMPARE(sheet.markerCount(), 2);
    QVERIFY(sheet.lines().at(2).markers.isEmpty());
    QCOMPARE(st->callingScript(m_prod).anchors.count(), 2);
    QVERIFY(st->undo());
    QCOMPARE(CallingScriptSheet::build(m_doc, m_prod).lineOfCue(m_cues[1]), 2);
}

void CallingScript_Test::editingTheTextKeepsCuesOnTheirWords()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->anchorCue(m_prod, m_cues[0], offsetOf("Scream out")));
    QVERIFY(st->anchorCue(m_prod, m_cues[1], offsetOf("Hold")));
    QString text = QString::fromLatin1(kScript);

    // A line added above: both move down with their words (CRLF made LF).
    QVERIFY(st->setScriptText(m_prod, QString("Intro\r\n") + text));
    QCOMPARE(st->callingScript(m_prod).text, QString("Intro\n") + text);
    CallingScriptSheet sheet = CallingScriptSheet::build(m_doc, m_prod);
    QCOMPARE(sheet.lineOfCue(m_cues[0]), 3);
    QCOMPARE(sheet.lineOfCue(m_cues[1]), 5);
    QCOMPARE(sheet.title(), QString("act1.txt"));           // the title stays

    // The words the cue sits on rewritten: it goes to the start of the change.
    QString edited = st->callingScript(m_prod).text;
    edited.replace("Scream out loud", "Shout it out");
    QVERIFY(st->setScriptText(m_prod, edited));
    sheet = CallingScriptSheet::build(m_doc, m_prod);
    QCOMPARE(sheet.lineOfCue(m_cues[0]), 3);
    QCOMPARE(sheet.lineOfCue(m_cues[1]), 5);

    // The same text again is no step.
    QVERIFY(!st->setScriptText(m_prod, edited));

    // Cut down to nothing: the offsets stay inside the text.
    QVERIFY(st->setScriptText(m_prod, "Short"));
    for (const ScriptAnchor &a : st->callingScript(m_prod).anchors)
        QVERIFY(a.go <= 5);

    QVERIFY(st->clearScript(m_prod));
    QVERIFY(st->callingScript(m_prod).isEmpty());
    QVERIFY(CallingScriptSheet::build(m_doc, m_prod).isEmpty());
}

void CallingScript_Test::importReadsTextAndMarkdown()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("lyrics.md");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("# Song\r\nLa la \xc3\xa9\r\nla\r\n");
    f.close();
    ShowTreeController *st = m_doc->showTree();
    QString error;
    QVERIFY2(st->importScript(m_prod, path, &error), qPrintable(error));
    const CallingScript s = st->callingScript(m_prod);
    QCOMPARE(s.title, QString("lyrics.md"));
    QCOMPARE(s.text, QString::fromUtf8("# Song\nLa la \xc3\xa9\nla\n"));
    QVERIFY(!st->importScript(m_prod, dir.filePath("nothing.txt"), &error));
    QVERIFY(!error.isEmpty());
}

void CallingScript_Test::savedWithTheShowAbsentMeansNone()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->anchorCue(m_prod, m_cues[0], offsetOf("Everybody scream")));
    QVERIFY(st->anchorCue(m_prod, m_cues[2], offsetOf("Hold"), ShowTreeController::ScriptMark::Standby));
    QVERIFY(st->anchorCue(m_prod, m_cues[3], offsetOf("Hold")));

    auto roundTrip = [](const Production &p) {
        QByteArray xml;
        QBuffer buffer(&xml);
        buffer.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buffer);
        w.writeStartDocument();
        saveXML(&w, p);
        w.writeEndDocument();
        QXmlStreamReader r(xml);
        Production out;
        while (!r.atEnd())
            if (r.readNextStartElement() && loadXML(r, out))
                break;
        return qMakePair(out, QString::fromUtf8(xml));
    };

    const auto saved = roundTrip(*st->production(m_prod));
    QVERIFY(saved.second.contains("<CallingScript"));
    const CallingScript &s = saved.first.script;
    QCOMPARE(s.title, QString("act1.txt"));
    QCOMPARE(s.text, QString::fromLatin1(kScript));
    QCOMPARE(s.anchors.count(), 3);
    QCOMPARE(s.anchor(m_cues[0])->go, offsetOf("Everybody scream"));
    QCOMPARE(s.anchor(m_cues[2])->standby, offsetOf("Hold"));
    QCOMPARE(s.anchor(m_cues[2])->go, -1);

    // A deleted cue leaves nothing behind in the file.
    QVERIFY(st->removeCue(m_cues[3]));
    const auto pruned = roundTrip(*st->production(m_prod));
    QCOMPARE(pruned.first.script.anchors.count(), 2);
    QVERIFY(pruned.first.script.anchor(m_cues[3]) == nullptr);

    // No script: no element, and one loads as none.
    QVERIFY(st->clearScript(m_prod));
    const auto none = roundTrip(*st->production(m_prod));
    QVERIFY(!none.second.contains("CallingScript"));
    QVERIFY(none.first.script.isEmpty());
}

void CallingScript_Test::headingsFromSectionsAndMarkdown()
{
    ShowTreeController *st = m_doc->showTree();
    // A section's name on a line of its own is a heading, so is a Markdown one.
    QVERIFY(st->setScriptText(m_prod, "Act One\n# Notes\neverybody scream\nThe words\n1 — Everybody Scream\n"));
    const CallingScriptSheet sheet = CallingScriptSheet::build(m_doc, m_prod);
    QCOMPARE(sheet.lines().count(), 6);
    QVERIFY(!sheet.lines().at(0).heading);
    QVERIFY(sheet.lines().at(1).heading);           // "# Notes"
    QVERIFY(sheet.lines().at(2).heading);           // the section's name, any case
    QVERIFY(!sheet.lines().at(3).heading);
    QVERIFY(sheet.lines().at(4).heading);           // its title as the Show Tree reads it
}

void CallingScript_Test::pdfAndHtml()
{
    ShowTreeController *st = m_doc->showTree();
    Cue c = *st->cue(m_cues[2]);
    c.note = "Blinders <fast>";
    QVERIFY(st->setCue(c));
    QVERIFY(st->anchorCue(m_prod, m_cues[2], offsetOf("Scream out")));
    QVERIFY(st->anchorCue(m_prod, m_cues[2], offsetOf("Everybody scream"), ShowTreeController::ScriptMark::Standby));
    const CallingScriptSheet sheet = CallingScriptSheet::build(m_doc, m_prod);

    const QString html = sheet.toHtml("Calling script — Dance Hysteria");
    QVERIFY(html.contains("<b>GO 1.0.1</b> Big hit"));
    QVERIFY(html.contains("<b>STBY 1.0.1</b>"));
    QVERIFY(html.contains("Blinders &lt;fast&gt;"));         // notes escaped
    QVERIFY(html.contains("Scream out loud"));
    QVERIFY(html.indexOf("STBY 1.0.1") < html.indexOf("GO 1.0.1"));     // standby is called first

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString pdf = dir.filePath("script.pdf");
    QString error;
    QVERIFY2(sheet.writePdf(pdf, "Calling script", &error), qPrintable(error));
    QFile f(pdf);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.read(5) == "%PDF-");
    QVERIFY(f.size() > 1000);
    // A file that can't be written says so.
    QVERIFY(!sheet.writePdf(dir.filePath("no/such/dir/x.pdf"), "x", &error));
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(CallingScript_Test)

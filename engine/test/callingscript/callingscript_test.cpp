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
#include <QRegularExpression>
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

/****************************************************************************
 * Cues from the script (Q237)
 ****************************************************************************/

namespace
{
typedef ShowTreeController::ScriptCue ScriptCue;

/* The proposal for script line @a line (null: none). */
const ScriptCue *proposalAt(const QList<ScriptCue> &plan, int line)
{
    for (const ScriptCue &sc : plan)
        if (sc.direction.line == line)
            return &sc;
    return nullptr;
}

/* "line:number" for every proposal, "line:-" for one with no number. */
QStringList numbered(const QList<ScriptCue> &plan)
{
    QStringList out;
    for (const ScriptCue &sc : plan)
        out << QStringLiteral("%1:%2").arg(sc.direction.line).arg(sc.made() ? sc.number : QStringLiteral("-"));
    return out;
}

/* The lines of the excerpt that are stage directions. */
const QList<int> kDirectionLines = { 5, 8, 9, 11, 13, 15, 16, 18, 21, 23, 24, 25, 30, 31, 35, 36 };
} // namespace

void CallingScript_Test::importExcerpt()
{
    ShowTreeController *st = m_doc->showTree();
    QString error;
    QVERIFY2(st->importScript(m_prod, QStringLiteral(CALLINGSCRIPT_TEST_DIR "/script-excerpt.txt"), &error),
             qPrintable(error));
    QVERIFY(st->addSection(m_list, "Lets Dance") != invalidId());       // 2.0.1
}

void CallingScript_Test::scriptDirectionsAreFoundInAnExport()
{
    importExcerpt();
    const QString text = m_doc->showTree()->callingScript(m_prod).text;
    QVERIFY(!text.contains('\r'));
    const QStringList lines = text.split('\n');
    const QList<ShowTreeController::ScriptDirection> dirs = ShowTreeController::scriptDirections(text);
    QList<int> at;
    for (const auto &d : dirs)
        at << d.line;
    // Not "SCENE:" with nothing after it, a wrapped "LiGHTING," line, CHOREO: or WITCH:.
    QCOMPARE(at, kDirectionLines);

    QCOMPARE(dirs.at(0).kind, QString("SCENE"));
    QCOMPARE(dirs.at(0).text, QString("CURTAIN closed, LIGHTS on Curtain and Announcer"));
    QCOMPARE(dirs.at(3).kind, QString("LIGHTING"));
    // The zero-width space after the colon is gone; "SCENE :" is one too.
    QCOMPARE(dirs.at(6).text, QString("CREW Help with screams?"));
    QCOMPARE(dirs.at(11).kind, QString("SCENE"));
    QVERIFY(dirs.at(11).text.startsWith("VOCAL HIT ( Bum da dum"));
    QCOMPARE(dirs.at(12).kind, QString("LIGHTS"));
    QCOMPARE(dirs.at(12).text, QString("Orange, Green"));
    // The GO goes on the keyword's first letter, past the zero-width space.
    for (const auto &d : dirs)
    {
        QVERIFY(d.offset >= 0 && d.offset < text.length());
        QVERIFY2(text.mid(d.offset).startsWith(d.kind, Qt::CaseInsensitive), qPrintable(lines.at(d.line)));
    }

    // Title lines: capitals or Markdown, never a line with a colon or lower case.
    QCOMPARE(ShowTreeController::scriptTitleLine("EVERYBODY SCREAM"), QString("EVERYBODY SCREAM"));
    QCOMPARE(ShowTreeController::scriptTitleLine(QString("APPLAUSE") + QChar(0x200B)), QString("APPLAUSE"));
    QCOMPARE(ShowTreeController::scriptTitleLine("## Act 1"), QString("Act 1"));
    QVERIFY(ShowTreeController::scriptTitleLine("Script").isEmpty());
    QVERIFY(ShowTreeController::scriptTitleLine("CHOREO: DANCE BREAK").isEmpty());
    QVERIFY(ShowTreeController::scriptTitleLine("D").isEmpty());        // a watermark's letter

    // Names: the first phrase, at most 32 characters, cut at a word.
    QCOMPARE(ShowTreeController::scriptCueName(dirs.at(0).text), QString("CURTAIN closed"));
    QCOMPARE(ShowTreeController::scriptCueName(dirs.at(1).text), QString("Lanterns light"));
    QCOMPARE(ShowTreeController::scriptCueName(dirs.at(3).text), QString("Primary Blue/Purple/Yellow"));
    QCOMPARE(ShowTreeController::scriptCueName(dirs.at(5).text), QString("UPLIGHTS come up on groups for"));
    QCOMPARE(ShowTreeController::scriptCueName(dirs.at(11).text), QString("VOCAL HIT"));
    QCOMPARE(ShowTreeController::scriptCueName(dirs.at(13).text), QString("WITCH SLAMS STAFF on STOP"));
    QCOMPARE(ShowTreeController::scriptCueName("Minimal lighting .. WITCH is CS"), QString("Minimal lighting"));
    QCOMPARE(ShowTreeController::scriptCueName("TOWN1 reaches for torch — LANTERN 1 ignites"),
             QString("TOWN1 reaches for torch"));
}

void CallingScript_Test::scriptCuesAreNumberedSongBySong()
{
    importExcerpt();
    ShowTreeController *st = m_doc->showTree();
    const QList<ScriptCue> plan = st->proposeScriptCues(m_prod, m_list);
    QCOMPARE(plan.count(), kDirectionLines.count());
    // Before the first song: the preshow (0), after its last cue. A song's
    // name in the script (any case) is that song; a title line with a
    // LIGHTING: line under it is the next song, named from the script.
    // DANCE BREAK, APPLAUSE and TIME SKIP are not songs.
    QCOMPARE(numbered(plan), QStringList({ "5:0.0.3", "8:0.0.4", "9:0.0.5",
                                           "11:1.0.3", "13:1.0.4", "15:1.0.5", "16:1.0.6", "18:1.0.7", "21:1.0.8",
                                           "23:2.0.2", "24:2.0.3", "25:2.0.4",
                                           "30:3.0.1", "31:3.0.2",
                                           "35:4.0.1", "36:4.0.2" }));
    const ScriptCue *a = proposalAt(plan, 11);
    QCOMPARE(a->song, QString("1"));
    QCOMPARE(a->songTitle, QString("Everybody Scream"));
    QVERIFY(!a->newSong);
    QCOMPARE(a->name, QString("Primary Blue/Purple/Yellow"));
    QCOMPARE(a->direction.text, QString("Primary Blue/Purple/Yellow Hi-lights"));
    const ScriptCue *b = proposalAt(plan, 30);
    QCOMPARE(b->song, QString("3"));
    QCOMPARE(b->songTitle, QString("Heads Will Roll"));
    QVERIFY(b->newSong);
    QCOMPARE(proposalAt(plan, 35)->songTitle, QString("Life And Death Reprise"));
    // Nothing changed: four cues and the one made for song 2.
    QCOMPARE(st->cueList(m_list)->cues.count(), 5);
    QVERIFY(st->callingScript(m_prod).anchors.isEmpty());

    // An earlier song's name as a lyric further on doesn't take the script back.
    QVERIFY(st->setScriptText(m_prod, "EVERYBODY SCREAM\nSCENE: one\nLETS DANCE\nEverybody scream\nSCENE: two\n"));
    QCOMPARE(numbered(st->proposeScriptCues(m_prod, m_list)), QStringList({ "1:1.0.3", "4:2.0.2" }));
}

void CallingScript_Test::scriptLinesWithACueAreSkipped()
{
    importExcerpt();
    ShowTreeController *st = m_doc->showTree();
    const QString text = st->callingScript(m_prod).text;
    const QList<ShowTreeController::ScriptDirection> dirs = ShowTreeController::scriptDirections(text);
    // "Big hit" (1.0.1) called on the curtain line: that line is skipped,
    // and the script is in song 1 from there. "Hold" (1.0.2) is not on the
    // script: it is taken to follow 1.0.1, so the next cue is 1.0.3.
    QVERIFY(st->anchorCue(m_prod, m_cues[2], dirs.at(2).offset + 7));
    const QList<ScriptCue> plan = st->proposeScriptCues(m_prod, m_list);
    const ScriptCue *cued = proposalAt(plan, 9);
    QVERIFY(!cued->made());
    QCOMPARE(cued->cuedBy, m_cues[2]);
    QCOMPARE(cued->cuedAs, QString("1.0.1 Big hit"));
    QCOMPARE(numbered(plan).mid(0, 5), QStringList({ "5:0.0.3", "8:0.0.4", "9:-", "11:1.0.3", "13:1.0.4" }));
}

void CallingScript_Test::scriptCuesNumberAroundCuesOnTheScript()
{
    importExcerpt();
    ShowTreeController *st = m_doc->showTree();
    const QList<ShowTreeController::ScriptDirection> dirs =
        ShowTreeController::scriptDirections(st->callingScript(m_prod).text);
    // "Hold" (1.0.2) called on line 15: the song's lines before it go in
    // front of it, those after it after it.
    QVERIFY(st->anchorCue(m_prod, m_cues[3], dirs.at(5).offset));
    const QList<ScriptCue> plan = st->proposeScriptCues(m_prod, m_list);
    QCOMPARE(numbered(plan).mid(3, 6), QStringList({ "11:1.0.1.5", "13:1.0.1.6", "15:-", "16:1.0.3", "18:1.0.4",
                                                     "21:1.0.5" }));
}

void CallingScript_Test::untickedLinesAreLeftOutAndTheRestRenumbered()
{
    importExcerpt();
    ShowTreeController *st = m_doc->showTree();
    const QList<ScriptCue> plan = st->proposeScriptCues(m_prod, m_list, QSet<int>({ 8, 30 }));
    QVERIFY(proposalAt(plan, 8)->leftOut);
    QVERIFY(!proposalAt(plan, 8)->made());
    QCOMPARE(proposalAt(plan, 9)->number, QString("0.0.4"));
    QCOMPARE(proposalAt(plan, 31)->number, QString("3.0.1"));

    const QList<quint32> ids = st->makeScriptCues(m_prod, m_list, QSet<int>({ 8, 30 }));
    QCOMPARE(ids.count(), kDirectionLines.count() - 2);
    const CallingScriptSheet sheet = CallingScriptSheet::build(m_doc, m_prod);
    QVERIFY(sheet.lines().at(8).markers.isEmpty());
    QVERIFY(sheet.lines().at(30).markers.isEmpty());
    // The ones left out are offered again; the rest have their cue.
    const QList<ScriptCue> again = st->proposeScriptCues(m_prod, m_list);
    QCOMPARE(numbered(again).filter(QRegularExpression(":\\d")).count(), 2);
    QCOMPARE(proposalAt(again, 8)->number, QString("0.0.3.5"));       // between 0.0.3 and 0.0.4
    const QString thirty = proposalAt(again, 30)->number;               // in song 3, before 3.0.1
    QCOMPARE(sectionKey(Numbering::ShowPartIteration, thirty), QString("3"));
    QVERIFY(compareCueNumbers(thirty, "3.0.1") < 0);
}

void CallingScript_Test::scriptCuesAreMadeInOneUndoStep()
{
    importExcerpt();
    ShowTreeController *st = m_doc->showTree();
    const int undoBefore = int(st->undoSteps().count());
    QString message;
    const QList<quint32> ids = st->makeScriptCues(m_prod, m_list, QSet<int>(), &message);
    QCOMPARE(ids.count(), kDirectionLines.count());
    QVERIFY2(message.contains("16"), qPrintable(message));
    QCOMPARE(int(st->undoSteps().count()), undoBefore + 1);
    QCOMPARE(st->undoText(), QString("Cues from the script"));

    // Each in number order, its note the line's words, its GO on its line.
    const CueList *list = st->cueList(m_list);
    QCOMPARE(list->cues.count(), 5 + kDirectionLines.count());
    for (int i = 1; i < list->cues.count(); ++i)
        QVERIFY(compareCueNumbers(list->cues.at(i - 1).number, list->cues.at(i).number) < 0);
    const CallingScriptSheet sheet = CallingScriptSheet::build(m_doc, m_prod);
    const QList<ScriptCue> plan = st->proposeScriptCues(m_prod, m_list);
    for (int i = 0; i < ids.count(); ++i)
    {
        const Cue *c = st->cue(ids.at(i));
        QVERIFY(c != nullptr);
        QCOMPARE(sheet.lineOfCue(ids.at(i)), kDirectionLines.at(i));
        QCOMPARE(c->note, ShowTreeController::scriptDirections(st->callingScript(m_prod).text).at(i).text);
        QCOMPARE(proposalAt(plan, kDirectionLines.at(i))->cuedBy, ids.at(i));
    }
    QCOMPARE(st->cue(ids.first())->number, QString("0.0.3"));
    QCOMPARE(st->cue(ids.first())->name, QString("CURTAIN closed"));
    // The new songs are named from the script.
    QStringList titles;
    for (const Section &s : st->sections(m_list))
        titles << sectionTitle(s);
    QCOMPARE(titles, QStringList({ "0 — Preshow", "1 — Everybody Scream", "2 — Lets Dance", "3 — Heads Will Roll",
                                   "4 — Life And Death Reprise" }));

    // Again: every line has its cue, nothing is made, and it says so.
    QVERIFY(st->makeScriptCues(m_prod, m_list, QSet<int>(), &message).isEmpty());
    QCOMPARE(message, QString("Every SCENE and LIGHTING line already has a cue."));
    QCOMPARE(int(st->undoSteps().count()), undoBefore + 1);

    // One undo takes them all back, the songs' names and the script's marks too.
    QVERIFY(st->undo());
    QCOMPARE(st->cueList(m_list)->cues.count(), 5);
    QVERIFY(st->callingScript(m_prod).anchors.isEmpty());
    QCOMPARE(st->sections(m_list).count(), 3);
    for (const SectionHead &h : st->cueList(m_list)->sections)
        QVERIFY(h.key != "3" && h.key != "4");
    QVERIFY(st->redo());
    QCOMPARE(st->cueList(m_list)->cues.count(), 5 + kDirectionLines.count());
    QCOMPARE(CallingScriptSheet::build(m_doc, m_prod).markerCount(), kDirectionLines.count());

    // A list that isn't one of the production's cue lists makes nothing.
    QVERIFY(st->makeScriptCues(m_prod, 99999, QSet<int>(), &message).isEmpty());
    QVERIFY(!message.isEmpty());
}

void CallingScript_Test::scriptCuesWithoutASchemeFollowOn()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Plain");
    const quint32 list = st->addCueList(prod, "Main");
    st->addCue(list, -1, "One");                    // 1
    st->addCue(list, 0, "Two");                     // 2
    QString error;
    QVERIFY2(st->importScript(prod, QStringLiteral(CALLINGSCRIPT_TEST_DIR "/script-excerpt.txt"), &error),
             qPrintable(error));
    const QList<ScriptCue> plan = st->proposeScriptCues(prod, list);
    QCOMPARE(plan.count(), kDirectionLines.count());
    for (int i = 0; i < plan.count(); ++i)
    {
        QCOMPARE(plan.at(i).number, QString::number(3 + i));
        QVERIFY(plan.at(i).song.isEmpty());
    }
    // No script: nothing proposed, and making says why.
    QVERIFY(st->clearScript(prod));
    QVERIFY(st->proposeScriptCues(prod, list).isEmpty());
    QString message;
    QVERIFY(st->makeScriptCues(prod, list, QSet<int>(), &message).isEmpty());
    QCOMPARE(message, QString("The script has no SCENE: or LIGHTING: lines."));
}

QTEST_MAIN(CallingScript_Test)

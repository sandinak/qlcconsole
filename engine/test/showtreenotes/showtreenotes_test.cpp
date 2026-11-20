/*
  Q Light Controller Plus
  showtreenotes_test.cpp

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

#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QSignalSpy>
#include <QBuffer>
#include <QtTest>

#include "../common/stdprelude.h"
#define private public
#include "showtreecontroller.h"
#include "showtreenotes.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "chaser.h"
#include "doc.h"

#include "showtreenotes_test.h"

using namespace ShowTree;

/* Notes beside the Show Tree (Q212): entries on each kind of item, their
   undo, the cue note as the first entry, Needs work as "an open note", and
   save / load with the snapshot's picture. */

static QByteArray showXml(Doc *doc)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.writeStartDocument();
    doc->saveXML(&w);
    w.writeEndDocument();
    return xml;
}

static Doc *loadShow(QObject *parent, const QByteArray &xml)
{
    Doc *other = new Doc(parent);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    if (!other->loadXML(r, false))
    {
        delete other;
        return nullptr;
    }
    return other;
}

void ShowTreeNotes_Test::init()
{
    m_doc = new Doc(this);
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Dim 1");
    fxi->setAddress(0);
    fxi->setUniverse(0);
    fxi->setChannels(1);
    m_doc->addFixture(fxi);
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Towers");
    m_doc->addFixtureGroup(g);
    g->assignFixture(fxi->id());
    QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
    p->setName("Half");
    p->setValue(127);
    m_doc->addPalette(p);
    m_palette = p->id();

    ShowTreeController *st = m_doc->showTree();
    m_prod = st->addProduction("Show");
    m_list = st->addCueList(m_prod, "Main", ListMode::Tracking);
    m_cue1 = st->addCue(m_list, -1, "Opener");
    m_cue2 = st->addCue(m_list, 0, "Verse");
    Target t;
    t.groups << g->id();
    m_layer = st->addLayer(m_cue1, t);
    QVERIFY(m_layer != invalidId());
    st->clearUndo();
}

void ShowTreeNotes_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void ShowTreeNotes_Test::notesOnEveryKind()
{
    ShowTreeController *st = m_doc->showTree();
    QSignalSpy spy(st, &ShowTreeController::notesChanged);
    const quint32 a = st->addNote(NoteKind::Production, m_prod, "Bring the hazer", NoteView(), invalidId(), "Ann");
    const quint32 b = st->addNote(NoteKind::List, m_list, "Act 1 runs long");
    const quint32 c = st->addNote(NoteKind::Cue, m_cue1, "Too blue on the DS edge");
    const quint32 d = st->addNote(NoteKind::Layer, m_layer, "Towers late", NoteView(), m_cue1);
    const quint32 e = st->addNote(NoteKind::Palette, m_palette, "Half reads hot on camera");
    for (quint32 id : { a, b, c, d, e })
        QVERIFY(id != invalidId());
    QCOMPARE(spy.count(), 5);

    QCOMPARE(st->notes(NoteKind::Production, m_prod).count(), 1);
    QCOMPARE(st->note(a)->author, QString("Ann"));
    QVERIFY(!st->note(b)->author.isEmpty());        // Your name, else the account
    QVERIFY(!st->note(c)->when.isEmpty());
    QVERIFY(!st->note(c)->done);
    QCOMPARE(st->note(d)->cue, m_cue1);
    QCOMPARE(st->noteTargetLabel(*st->note(e)), QString("Palette Half"));
    QVERIFY(st->noteTargetLabel(*st->note(d)).startsWith("Layer "));
    QCOMPARE(st->allNotes().count(), 5);

    // Nothing to note on what isn't there, nor an empty note.
    QCOMPARE(st->addNote(NoteKind::Cue, 99999, "x"), invalidId());
    QCOMPARE(st->addNote(NoteKind::Layer, m_layer, "x", NoteView(), m_cue2), invalidId());
    QCOMPARE(st->addNote(NoteKind::Cue, m_cue2, "   "), invalidId());

    // Newest first.
    const quint32 c2 = st->addNote(NoteKind::Cue, m_cue1, "Second");
    const QList<Note> onCue = st->notes(NoteKind::Cue, m_cue1);
    QCOMPARE(onCue.count(), 2);
    QCOMPARE(onCue.first().id, c2);
    QCOMPARE(st->latestNote(NoteKind::Cue, m_cue1)->id, c2);
    int open = 0;
    QCOMPARE(st->noteCount(NoteKind::Cue, m_cue1, &open), 2);
    QCOMPARE(open, 2);
}

void ShowTreeNotes_Test::editTickRemoveAreUndoable()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 n = st->addNote(NoteKind::Layer, m_layer, "Towers late", NoteView(), m_cue1);
    QVERIFY(st->canUndo());
    QCOMPARE(st->undoText(), QString("Add note"));

    QVERIFY(st->setNoteText(n, "Towers late by a beat"));
    QVERIFY(!st->setNoteText(n, "Towers late by a beat"));     // nothing changed
    QVERIFY(st->setNoteDone(n, true));
    QVERIFY(st->note(n)->done);
    QVERIFY(st->removeNote(n));
    QVERIFY(st->note(n) == nullptr);

    QVERIFY(st->undo());
    QVERIFY(st->note(n) != nullptr);
    QVERIFY(st->note(n)->done);
    QVERIFY(st->undo());
    QVERIFY(!st->note(n)->done);
    QVERIFY(st->undo());
    QCOMPARE(st->note(n)->text, QString("Towers late"));
    QVERIFY(st->undo());
    QVERIFY(st->note(n) == nullptr);
    QVERIFY(st->redo());
    QCOMPARE(st->note(n)->text, QString("Towers late"));

    // A palette's note too.
    const quint32 p = st->addNote(NoteKind::Palette, m_palette, "Hot");
    QVERIFY(st->setNoteDone(p, true));
    QVERIFY(st->undo());
    QVERIFY(!st->note(p)->done);
}

void ShowTreeNotes_Test::cueNoteIsTheFirstEntry()
{
    ShowTreeController *st = m_doc->showTree();
    // The cue page's note: its first entry.
    Cue c = *st->cue(m_cue1);
    c.note = "Wait for the actor";
    QVERIFY(st->setCue(c));
    QList<Note> list = st->notes(NoteKind::Cue, m_cue1);
    QCOMPARE(list.count(), 1);
    QVERIFY(list.first().cueNote);
    QCOMPARE(list.first().text, QString("Wait for the actor"));
    QVERIFY(list.first().done);             // a standing note, not a to-do
    QVERIFY(!st->cue(m_cue1)->needsWork);

    // A later entry is newer: the cue note stays last (the first entry).
    const quint32 later = st->addNote(NoteKind::Cue, m_cue1, "Fix the fade");
    list = st->notes(NoteKind::Cue, m_cue1);
    QCOMPARE(list.count(), 2);
    QCOMPARE(list.first().id, later);
    QVERIFY(list.last().cueNote);

    // Edited from the pane: the cue's note follows, and bakes into its step.
    const quint32 entry = list.last().id;
    QVERIFY(st->setNoteText(entry, "Wait for the actor to sit"));
    QCOMPARE(st->cue(m_cue1)->note, QString("Wait for the actor to sit"));
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(m_list)->chaser));
    QVERIFY(chaser != nullptr);
    QCOMPARE(chaser->stepAt(st->cueList(m_list)->indexOf(m_cue1))->note, QString("Wait for the actor to sit"));

    // Edited on the cue page: the entry follows.
    c = *st->cue(m_cue1);
    c.note = "Sit first";
    QVERIFY(st->setCue(c));
    QCOMPARE(st->note(entry)->text, QString("Sit first"));

    // The entry deleted: the note goes; undo brings both back.
    QVERIFY(st->removeNote(entry));
    QVERIFY(st->cue(m_cue1)->note.isEmpty());
    QVERIFY(st->undo());
    QCOMPARE(st->cue(m_cue1)->note, QString("Sit first"));
    QCOMPARE(st->note(entry)->text, QString("Sit first"));

    // Cleared on the cue page: the entry goes.
    c = *st->cue(m_cue1);
    c.note.clear();
    QVERIFY(st->setCue(c));
    QVERIFY(st->note(entry) == nullptr);
    QCOMPARE(st->notes(NoteKind::Cue, m_cue1).count(), 1);
}

void ShowTreeNotes_Test::openNoteIsNeedsWork()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(!st->isCueNeedsWork(m_cue2));
    const quint32 n = st->addNote(NoteKind::Cue, m_cue2, "Chase too fast");
    QVERIFY(st->isCueNeedsWork(m_cue2));
    QCOMPARE(st->needsWorkCues(), QList<quint32>({ m_cue2 }));
    QVERIFY(st->setNoteDone(n, true));
    QVERIFY(!st->isCueNeedsWork(m_cue2));
    QVERIFY(st->undo());
    QVERIFY(st->isCueNeedsWork(m_cue2));

    // Cleared from the stage (the pad, the cue page): the notes are done.
    QVERIFY(st->setCuesNeedsWork({ m_cue2 }, false));
    QVERIFY(st->note(n)->done);
    QVERIFY(!st->isCueNeedsWork(m_cue2));

    // Set from the stage: an open note says so.
    QVERIFY(st->setCuesNeedsWork({ m_cue2 }, true));
    int open = 0;
    QCOMPARE(st->noteCount(NoteKind::Cue, m_cue2, &open), 2);
    QCOMPARE(open, 1);
    QCOMPARE(st->latestNote(NoteKind::Cue, m_cue2)->text, QString("Needs work"));
    // One undo step takes both away.
    QVERIFY(st->undo());
    QVERIFY(!st->isCueNeedsWork(m_cue2));
    QCOMPARE(st->noteCount(NoteKind::Cue, m_cue2), 1);

    // A layer's open note is not its cue's Needs work.
    st->addNote(NoteKind::Layer, m_layer, "Later", NoteView(), m_cue1);
    QVERIFY(!st->isCueNeedsWork(m_cue1));
}

void ShowTreeNotes_Test::saveLoadKeepsNotesAndSnapshot()
{
    ShowTreeController *st = m_doc->showTree();
    QImage img(48, 30, QImage::Format_RGB32);
    img.fill(Qt::darkCyan);
    QByteArray png;
    {
        QBuffer b(&png);
        b.open(QIODevice::WriteOnly);
        QVERIFY(img.save(&b, "PNG"));
    }
    NoteView v;
    v.valid = true;
    v.plane = 3;
    v.rotation = 1;
    v.azimuth = 37.5;
    v.elevation = 22.25;
    v.zoomed = true;
    v.scale = 88.5;
    v.originX = 410.5;
    v.originY = -12.0;
    v.width = 900;
    v.height = 600;
    v.cue = m_cue1;
    v.cueLabel = "1 Opener";
    v.image = st->addNoteImage(png);
    QVERIFY(!v.image.isEmpty());
    QCOMPARE(st->addNoteImage(png), v.image);        // the same bytes, the same picture

    const quint32 snap = st->addNote(NoteKind::Cue, m_cue2, "Look at the floor", v);
    const quint32 pal = st->addNote(NoteKind::Palette, m_palette, "Hot");
    QVERIFY(st->setNoteDone(pal, true));
    const quint32 lay = st->addNote(NoteKind::Layer, m_layer, "Line\nbreak & <tags>", NoteView(), m_cue1);
    QCOMPARE(st->noteImageBytes(), qint64(png.size()));

    const QByteArray xml = showXml(m_doc);
    QVERIFY(xml.contains("<Notes>"));
    QVERIFY(xml.contains("<NoteImage"));
    Doc *other = loadShow(this, xml);
    QVERIFY(other != nullptr);
    ShowTreeController *ost = other->showTree();
    QCOMPARE(ost->allNotes().count(), 3);
    for (quint32 id : { snap, pal, lay })
        QCOMPARE(*ost->note(id), *st->note(id));
    QCOMPARE(ost->note(lay)->text, QString("Line\nbreak & <tags>"));
    QVERIFY(ost->note(pal)->done);
    QCOMPARE(ost->noteImage(v.image), png);
    QVERIFY(ost->isCueNeedsWork(m_cue2));
    // A new note after load doesn't reuse an id.
    const quint32 next = ost->addNote(NoteKind::Cue, m_cue1, "New");
    QVERIFY(next > lay);
    delete other;

    // A picture no note uses any more is not saved.
    QVERIFY(st->setNoteView(snap, NoteView()));
    QVERIFY(!showXml(m_doc).contains("<NoteImage"));
}

void ShowTreeNotes_Test::showAndSectionNotesAreTheFirstEntry()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setNumbering(m_prod, Numbering::ShowPartIteration));
    st->addSection(m_list, "Opener");                       // 0.0.1
    const quint32 song = st->addSection(m_list, "Entrance");        // 1.0.1
    QVERIFY(song != invalidId());
    Section sec;
    QVERIFY(st->sectionOfCue(song, &sec));
    st->clearUndo();

    // The show's own note and a song's: each one entry, marked as its own.
    QVERIFY(st->setListNote(m_list, "Warm washes, cool the chorus"));
    QVERIFY(st->setSectionNote(m_list, sec, "Slow build; moving heads late"));
    QCOMPARE(st->notes(NoteKind::List, m_list).count(), 1);
    QVERIFY(st->notes(NoteKind::List, m_list).first().cueNote);
    const quint32 target = sectionNoteTarget(m_list, sectionId(sec));
    QCOMPARE(st->notes(NoteKind::Section, target).count(), 1);
    QCOMPARE(st->notes(NoteKind::Section, target).first().text, QString("Slow build; moving heads late"));
    QVERIFY(st->noteTargetLabel(st->notes(NoteKind::Section, target).first()).contains("Entrance"));

    // The pane edits it back into the section's own field.
    QVERIFY(st->setNoteText(st->notes(NoteKind::Section, target).first().id, "Edited in the pane"));
    QCOMPARE(st->cueList(m_list)->sections.last().note, QString("Edited in the pane"));

    // Saved with the show, and it loads again.
    const QByteArray xml = showXml(m_doc);
    Doc *other = loadShow(this, xml);
    QVERIFY(other != nullptr);
    ShowTreeController *ost = other->showTree();
    QCOMPARE(ost->cueList(m_list)->note, QString("Warm washes, cool the chorus"));
    QCOMPARE(ost->notes(NoteKind::List, m_list).count(), 1);
    QCOMPARE(ost->notes(NoteKind::Section, target).count(), 1);
    QCOMPARE(ost->notes(NoteKind::Section, target).first().text, QString("Edited in the pane"));
    delete other;

    // One undo step; clearing removes the entry.
    QVERIFY(st->setListNote(m_list, QString()));
    QCOMPARE(st->notes(NoteKind::List, m_list).count(), 0);
    st->undo();
    QCOMPARE(st->notes(NoteKind::List, m_list).count(), 1);

    // An older show (no Note attribute) loads with none.
    QByteArray old = xml;
    old.replace(" Note=\"Warm washes, cool the chorus\"", "");
    Doc *older = loadShow(this, old);
    QVERIFY(older != nullptr);
    QVERIFY(older->showTree()->cueList(m_list)->note.isEmpty());
    delete older;
}

void ShowTreeNotes_Test::aSectionNoteSurvivesRenameAndRenumber()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setNumbering(m_prod, Numbering::ShowPartIteration));
    const quint32 song = st->addSection(m_list, "Entrance");
    QVERIFY(song != invalidId());
    st->addCue(m_list, st->cueList(m_list)->indexOf(song), "Verse");
    Section sec;
    QVERIFY(st->sectionOfCue(song, &sec));
    QVERIFY(st->setSectionNote(m_list, sec, "Slow build"));
    const quint32 target = sectionNoteTarget(m_list, sectionId(sec));
    auto haveNote = [&](ShowTreeController *s) {
        const QList<Note> n = s->notes(NoteKind::Section, target);
        return n.count() == 1 && n.first().text == "Slow build" && s->noteTargetExists(n.first());
    };
    QVERIFY(haveNote(st));

    // Rename: the head's name changes, its note stays.
    QVERIFY(st->sectionOfCue(song, &sec));
    QVERIFY(st->setSectionName(m_list, sec, "Grand Entrance"));
    QVERIFY(haveNote(st));
    // Renumber the song (its cues, not its place in the show): same section.
    QVERIFY(st->sectionOfCue(song, &sec));
    QString why;
    QVERIFY2(st->renumberSong(m_list, sec, 1, 2, true, &why), qPrintable(why));
    QVERIFY(st->sectionOfCue(song, &sec));
    QCOMPARE(sectionNoteTarget(m_list, sectionId(sec)), target);
    QVERIFY(haveNote(st));
    // Saved and loaded after the renumber.
    Doc *other = loadShow(this, showXml(m_doc));
    QVERIFY(other != nullptr);
    QVERIFY(haveNote(other->showTree()));
    QCOMPARE(other->showTree()->noteTargetLabel(other->showTree()->notes(NoteKind::Section, target).first())
                 .contains("Grand Entrance"), true);
    delete other;
}

void ShowTreeNotes_Test::editingASongsNumberCarriesItsSectionToTheNewKey()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setNumbering(m_prod, Numbering::ShowPartIteration));
    st->addSection(m_list, "Opener");                               // 0.0.1
    const quint32 song = st->addSection(m_list, "Entrance");        // 1.0.1
    const quint32 verse = st->addCue(m_list, st->cueList(m_list)->indexOf(song), "Verse");
    Section sec;
    QVERIFY(st->sectionOfCue(song, &sec));
    const QString oldKey = sec.key;
    QVERIFY(st->setSectionNote(m_list, sec, "Slow build"));
    QVERIFY(st->sectionOfCue(song, &sec));
    st->setSectionStart(m_list, sec, 4000);
    QVERIFY(st->sectionOfCue(song, &sec));
    const qint32 start = sec.start;
    const QString oldNumber = st->cue(song)->number;
    st->clearUndo();
    const quint32 oldTarget = sectionNoteTarget(m_list, oldKey);
    const quint32 newTarget = sectionNoteTarget(m_list, "9");

    // By hand: the first-level number of every cue of the song, one step.
    st->beginBatch("Renumber by hand");
    for (quint32 id : { song, verse })
    {
        Cue c = *st->cue(id);
        c.number = "9" + c.number.mid(c.number.indexOf('.'));
        QVERIFY(st->setCue(c));
    }
    st->endBatch(true);
    QVERIFY(st->sectionOfCue(song, &sec));
    QCOMPARE(sec.key, QString("9"));
    QCOMPARE(sec.name, QString("Entrance"));
    QCOMPARE(sec.start, start);
    QCOMPARE(st->notes(NoteKind::Section, newTarget).count(), 1);
    QCOMPARE(st->notes(NoteKind::Section, newTarget).first().text, QString("Slow build"));
    QCOMPARE(st->notes(NoteKind::Section, oldTarget).count(), 0);
    const QByteArray after = showXml(m_doc);

    // Undo: back under the old key, with its name and note.
    QVERIFY(st->undo());
    QVERIFY(st->sectionOfCue(song, &sec));
    QCOMPARE(sec.key, oldKey);
    QCOMPARE(sec.name, QString("Entrance"));
    QCOMPARE(st->notes(NoteKind::Section, oldTarget).count(), 1);
    QCOMPARE(st->cue(song)->number, oldNumber);
    const QByteArray before = showXml(m_doc);

    // Both save and load.
    for (const QByteArray &xml : { after, before })
    {
        const bool renumbered = xml == after;
        Doc *other = loadShow(this, xml);
        QVERIFY(other != nullptr);
        ShowTreeController *ost = other->showTree();
        Section s;
        QVERIFY(ost->sectionOfCue(song, &s));
        QCOMPARE(s.key, renumbered ? QString("9") : oldKey);
        QCOMPARE(s.name, QString("Entrance"));
        QCOMPARE(ost->notes(NoteKind::Section, renumbered ? newTarget : oldTarget).count(), 1);
        delete other;
    }

    // Onto another song's number: both are kept, and it is said.
    const quint32 finale = st->addSection(m_list, "Finale");
    QVERIFY(finale != invalidId());
    Section fin;
    QVERIFY(st->sectionOfCue(finale, &fin));
    QSignalSpy spy(st, &ShowTreeController::sectionNumberClash);
    st->beginBatch("Onto 2");
    for (quint32 id : { song, verse })
    {
        Cue c = *st->cue(id);
        c.number = fin.key + (id == song ? ".5.1" : ".5.2");
        QVERIFY(st->setCue(c));
    }
    st->endBatch(true);
    QCOMPARE(spy.count(), 1);
    int named = 0;
    for (const SectionHead &h : st->cueList(m_list)->sections)
        named += (h.name == "Entrance" || h.name == "Finale") ? 1 : 0;
    QCOMPARE(named, 2);
}

void ShowTreeNotes_Test::aNoteOnlySectionHeadIsNeverPruned()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setNumbering(m_prod, Numbering::ShowPartIteration));
    st->addSection(m_list, "Opener");
    const quint32 song = st->addSection(m_list, "Entrance");
    Section sec;
    QVERIFY(st->sectionOfCue(song, &sec));
    QVERIFY(st->setSectionNote(m_list, sec, "Slow build"));
    QVERIFY(st->sectionOfCue(song, &sec));
    QVERIFY(st->setSectionName(m_list, sec, QString()));            // a note-only head now
    QVERIFY(st->sectionOfCue(song, &sec));
    const quint32 target = sectionNoteTarget(m_list, sectionId(sec));
    QCOMPARE(st->notes(NoteKind::Section, target).count(), 1);
    // Setting and clearing a start prunes heads with nothing to say.
    QVERIFY(st->setSectionStart(m_list, sec, 4000));
    QVERIFY(st->sectionOfCue(song, &sec));
    QVERIFY(st->setSectionStart(m_list, sec, -1));
    QCOMPARE(st->notes(NoteKind::Section, target).count(), 1);
    QCOMPARE(st->notes(NoteKind::Section, target).first().text, QString("Slow build"));
}

void ShowTreeNotes_Test::sectionNoteTargetsAreStableAndOldOnesStillLoad()
{
    // FNV-1a of "7:3": the same on every Qt build and CPU.
    QCOMPARE(sectionNoteTarget(7, "3"), quint32(0x0fd65235u));
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setNumbering(m_prod, Numbering::ShowPartIteration));
    st->addSection(m_list, "Opener");
    const quint32 song = st->addSection(m_list, "Entrance");
    Section sec;
    QVERIFY(st->sectionOfCue(song, &sec));
    QVERIFY(st->setSectionNote(m_list, sec, "Slow build"));
    const quint32 target = sectionNoteTarget(m_list, sectionId(sec));
    // A show saved with the first build's qHash target still finds its note.
    QByteArray xml = showXml(m_doc);
    const quint32 legacy = legacySectionNoteTarget(m_list, sectionId(sec));
    QVERIFY(xml.contains(QByteArray("Target=\"") + QByteArray::number(target) + "\""));
    if (legacy != target)
        xml.replace(QByteArray("Target=\"") + QByteArray::number(target) + "\"",
                    QByteArray("Target=\"") + QByteArray::number(legacy) + "\"");
    Doc *other = loadShow(this, xml);
    QVERIFY(other != nullptr);
    QCOMPARE(other->showTree()->notes(NoteKind::Section, target).count(), 1);
    QCOMPARE(other->showTree()->notes(NoteKind::Section, target).first().text, QString("Slow build"));
    delete other;
    // A note of a kind this build does not know is skipped, not fatal.
    QByteArray odd = showXml(m_doc);
    odd.replace("Kind=\"Section\"", "Kind=\"Bogus\"");
    Doc *tolerant = loadShow(this, odd);
    QVERIFY(tolerant != nullptr);
    delete tolerant;
}

void ShowTreeNotes_Test::everySectionNoteFollowsItsSongAndSongsMoveTogether()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setNumbering(m_prod, Numbering::ShowPartIteration));
    const quint32 list = st->addCueList(m_prod, "Songs", ListMode::Tracking);
    const quint32 ca = st->addSection(list, "A");
    const quint32 cb = st->addSection(list, "B");
    const quint32 cc = st->addSection(list, "C");
    Section sa, sb, sc;
    QVERIFY(st->sectionOfCue(ca, &sa) && st->sectionOfCue(cb, &sb) && st->sectionOfCue(cc, &sc));
    for (const Section &s : { sa, sb, sc })
        QVERIFY(st->setSectionNote(list, s, "Own " + s.name));
    QVERIFY(st->sectionOfCue(ca, &sa));
    const quint32 userNote = st->addNote(NoteKind::Section, sectionNoteTarget(list, sectionId(sa)), "Try red",
                                         NoteView(), list);
    QVERIFY(userNote != invalidId());
    QVERIFY(st->setNoteDone(userNote, true));
    const int keyA = sa.key.toInt();

    // One edit shifts every song up by one: each keeps its name and notes,
    // nothing clashes.
    const QList<Production> old = st->m_productions;
    QList<Production> work = old;
    for (Production &p : work)
        if (CueList *l = p.list(list))
            for (Cue &cue : l->cues)
            {
                const QString first = cue.number.section('.', 0, 0);
                cue.number = QString::number(first.toInt() + 1) + cue.number.mid(first.length());
            }
    QVERIFY(st->rekeySections(old, work).isEmpty());
    st->m_productions = work;
    QVERIFY(st->sectionOfCue(ca, &sa) && st->sectionOfCue(cb, &sb) && st->sectionOfCue(cc, &sc));
    QCOMPARE(sa.key.toInt(), keyA + 1);
    QCOMPARE(sa.name, QString("A"));
    QCOMPARE(sb.name, QString("B"));
    QCOMPARE(sc.name, QString("C"));
    const QList<Note> aNotes = st->notes(NoteKind::Section, sectionNoteTarget(list, sectionId(sa)));
    QCOMPARE(aNotes.count(), 2);                        // its own, and the one the user added
    QVERIFY(st->note(userNote)->done);                  // id and done flag kept
    QCOMPARE(st->note(userNote)->target, sectionNoteTarget(list, sectionId(sa)));
    QCOMPARE(st->notes(NoteKind::Section, sectionNoteTarget(list, sectionId(sc))).first().text, QString("Own C"));

    // Two songs truly landing on one key: both heads are kept, every
    // clashing key is reported, one is parked at its old key.
    const QList<Production> before = st->m_productions;
    QList<Production> clash = before;
    for (Production &p : clash)
        if (CueList *l = p.list(list))
            for (Cue &cue : l->cues)
                if (cue.id == ca)
                    cue.number = sc.key + cue.number.mid(cue.number.section('.', 0, 0).length());
    const QStringList keys = st->rekeySections(before, clash);
    QCOMPARE(keys, QStringList({ sc.key }));
    int heads = 0;
    for (Production &p : clash)
        if (CueList *l = p.list(list))
            heads = l->sections.count();
    QCOMPARE(heads, 3);
}

void ShowTreeNotes_Test::anOlderShowsCueNotesMigrate()
{
    ShowTreeController *st = m_doc->showTree();
    Cue c = *st->cue(m_cue1);
    c.note = "Old note";
    QVERIFY(st->setCue(c));
    c = *st->cue(m_cue2);
    c.note = "Fix the snap";
    QVERIFY(st->setCue(c));
    QVERIFY(st->setCuesNeedsWork({ m_cue2 }, true));

    // As an older build wrote it: Note= and NeedsWork= only.
    QByteArray xml = showXml(m_doc);
    {
        QString s = QString::fromUtf8(xml);
        s.remove(QRegularExpression("<Notes>.*</Notes>", QRegularExpression::DotMatchesEverythingOption));
        xml = s.toUtf8();
    }
    QVERIFY(!xml.contains("<Notes>"));
    QVERIFY(xml.contains("Note=\"Old note\""));
    QVERIFY(xml.contains("NeedsWork=\"1\""));

    Doc *other = loadShow(this, xml);
    QVERIFY(other != nullptr);
    ShowTreeController *ost = other->showTree();
    QList<Note> one = ost->notes(NoteKind::Cue, m_cue1);
    QCOMPARE(one.count(), 1);
    QVERIFY(one.first().cueNote);
    QCOMPARE(one.first().text, QString("Old note"));
    QVERIFY(one.first().when.isEmpty());           // undated: from before
    QVERIFY(one.first().done);
    QVERIFY(!ost->isCueNeedsWork(m_cue1));
    // Needs work with a note: the note is what needs doing, open.
    QList<Note> two = ost->notes(NoteKind::Cue, m_cue2);
    QCOMPARE(two.count(), 1);
    QCOMPARE(two.first().text, QString("Fix the snap"));
    QVERIFY(!two.first().done);
    QVERIFY(ost->isCueNeedsWork(m_cue2));
    QVERIFY(!ost->canUndo());

    // Saved again: the entries are in the file, the old attributes too.
    const QByteArray again = showXml(other);
    QVERIFY(again.contains("<Notes>"));
    QVERIFY(again.contains("CueNote=\"1\""));
    QVERIFY(again.contains("Note=\"Old note\""));
    QVERIFY(again.contains("NeedsWork=\"1\""));
    Doc *third = loadShow(this, again);
    QCOMPARE(third->showTree()->notes(NoteKind::Cue, m_cue1).count(), 1);
    QCOMPARE(third->showTree()->notes(NoteKind::Cue, m_cue2).count(), 1);
    delete third;
    delete other;
}

void ShowTreeNotes_Test::deletedItemsNotesKeptForUndoNotSaved()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 n = st->addNote(NoteKind::Cue, m_cue2, "Gone soon");
    QVERIFY(st->removeCue(m_cue2));
    QVERIFY(st->note(n) != nullptr);               // kept, for the delete's undo
    QVERIFY(!st->noteTargetExists(*st->note(n)));
    QCOMPARE(st->allNotes().count(), 0);
    QVERIFY(!showXml(m_doc).contains("Gone soon"));
    QVERIFY(st->undo());
    QCOMPARE(st->allNotes().count(), 1);
    QVERIFY(st->isCueNeedsWork(m_cue2));
}

void ShowTreeNotes_Test::picturesStayOutOfUndoSteps()
{
    ShowTreeController *st = m_doc->showTree();
    QByteArray big(200 * 1024, 'x');
    NoteView v;
    v.valid = true;
    v.image = st->addNoteImage(big);
    st->addNote(NoteKind::Cue, m_cue1, "Big", v);
    // The undo snapshot names the picture, never carries it.
    const QByteArray state = st->outsideUndoState();
    QVERIFY(state.size() < 100 * 1024);
    QVERIFY(st->noteImageBytes() >= big.size());
    QVERIFY(ShowTreeController::noteImageWarnBytes() >= 2 * 1024 * 1024);
}

QTEST_GUILESS_MAIN(ShowTreeNotes_Test)

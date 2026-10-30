/*
  Q Light Controller Plus
  showtreesections_test.cpp

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
#include <QBuffer>
#include <QtTest>

#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "chaser.h"
#include "doc.h"

#include "showtreesections_test.h"

using namespace ShowTree;

/* Song sections (Q190): one cue list per show, its songs sections of it,
   derived from the show's numbering scheme; Merge lists into one… for a show
   built as a list per song. */

namespace
{
Look look(const QString &key, quint32 palette)
{
    Look l;
    l.key = key;
    l.palettes << palette;
    return l;
}

Layer layer(quint32 id, quint32 group, QList<Look> looks)
{
    Layer l;
    l.id = id;
    l.target.groups << group;
    l.looks = looks;
    return l;
}

Cue cue(quint32 id, const QString &number, QList<Layer> layers = {}, CueMode mode = CueMode::ListDefault)
{
    Cue c;
    c.id = id;
    c.number = number;
    c.layers = layers;
    c.mode = mode;
    return c;
}

CueList listOf(quint32 id, const QString &name, const QStringList &numbers, quint32 firstCue)
{
    CueList l;
    l.id = id;
    l.name = name;
    l.mode = ListMode::Tracking;
    for (int i = 0; i < numbers.count(); ++i)
        l.cues << cue(firstCue + quint32(i), numbers.at(i));
    return l;
}

QStringList numbersOf(const CueList &l)
{
    QStringList out;
    for (int i = 0; i < l.cues.count(); ++i)
        out << cueNumberAt(l, i);
    return out;
}

QStringList titles(const QList<Section> &sections)
{
    QStringList out;
    for (const Section &s : sections)
        out << QStringLiteral("%1[%2-%3]").arg(sectionTitle(s)).arg(s.first).arg(s.last);
    return out;
}

QByteArray productionXml(const Production &p)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    saveXML(&w, p);
    return xml;
}

Target groupTarget(quint32 g)
{
    Target t;
    t.groups << g;
    return t;
}

// Groups and palettes of the model-only tests: just ids.
enum : quint32 { Front = 1, Back = 2, Blue = 50, Red = 51, Half = 60, Full = 61 };
} // namespace

void ShowTreeSections_Test::init()
{
    m_doc = new Doc(this);
    m_fixtures.clear();
    for (int i = 0; i < 4; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dim %1").arg(i + 1));
        fxi->setAddress(quint32(i));
        fxi->setUniverse(0);
        fxi->setChannels(1);
        m_doc->addFixture(fxi);
        m_fixtures << fxi->id();
    }
}

void ShowTreeSections_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

/****************************************************************************
 * The scheme
 ****************************************************************************/

void ShowTreeSections_Test::schemeKeysForEachPreset()
{
    // {S}.{P}.{II}: the first level, whatever its size (period-separated).
    QCOMPARE(sectionKey(Numbering::ShowPartIteration, "0.0.1"), QString("0"));
    QCOMPARE(sectionKey(Numbering::ShowPartIteration, "1.0.1"), QString("1"));
    QCOMPARE(sectionKey(Numbering::ShowPartIteration, "12.10.2.5"), QString("12"));
    QCOMPARE(sectionKey(Numbering::ShowPartIteration, "3"), QString("3"));
    // Eos / MA: the hundred.
    QCOMPARE(sectionKey(Numbering::Hundreds, "100"), QString("100"));
    QCOMPARE(sectionKey(Numbering::Hundreds, "199.5"), QString("100"));
    QCOMPARE(sectionKey(Numbering::Hundreds, "215"), QString("200"));
    QCOMPARE(sectionKey(Numbering::Hundreds, "57"), QString("0"));
    // QLab (by hand) and None: the number says nothing.
    QCOMPARE(sectionKey(Numbering::Manual, "1.0.1"), QString());
    QCOMPARE(sectionKey(Numbering::None, "1.0.1"), QString());
    QCOMPARE(sectionKey(Numbering::ShowPartIteration, "intro"), QString());
    // Saved by name; an unknown one (an older or newer build's) is None.
    for (Numbering n : { Numbering::None, Numbering::ShowPartIteration, Numbering::Hundreds, Numbering::Manual })
        QCOMPARE(stringToNumbering(numberingToString(n)), n);
    QCOMPARE(stringToNumbering("Bogus"), Numbering::None);
}

void ShowTreeSections_Test::sectionsGroupByTheScheme()
{
    CueList l = listOf(1, "Show", { "0.0.1", "0.0.2", "0.0.3", "1.0.1", "1.1.1", "1.1.2", "2.0.1" }, 10);
    SectionHead h;
    h.key = "1";
    h.name = "Everybody Scream";
    l.sections << h;

    QCOMPARE(titles(sectionsOf(l, Numbering::ShowPartIteration)),
             QStringList({ "0[0-2]", "1 — Everybody Scream[3-5]", "2[6-6]" }));
    const QList<Section> s = sectionsOf(l, Numbering::ShowPartIteration);
    QCOMPARE(s.at(1).headCue, quint32(13));
    QCOMPARE(sectionOfIndex(s, 4), 1);
    QCOMPARE(sectionOfIndex(s, 9), -1);
    // None: no sections; a Looks section and sub-cues never have them.
    QVERIFY(sectionsOf(l, Numbering::None).isEmpty());
    CueList looks = l;
    looks.looks = true;
    QVERIFY(sectionsOf(looks, Numbering::ShowPartIteration).isEmpty());

    // Hundreds per song.
    const CueList eos = listOf(2, "Eos", { "1", "50", "100", "101.5", "150", "200", "201" }, 20);
    QCOMPARE(titles(sectionsOf(eos, Numbering::Hundreds)),
             QStringList({ "0[0-1]", "100[2-4]", "200[5-6]" }));
}

void ShowTreeSections_Test::manualSectionsBeginAtTheirCues()
{
    CueList l = listOf(1, "QLab", { "1", "2", "3", "4", "5" }, 10);
    SectionHead a;
    a.cue = 11;
    a.name = "Opener";
    SectionHead b;
    b.cue = 13;
    l.sections << b << a;   // in any order
    // Cue 1 is in none; Opener 2..3; an unnamed one 4..5.
    QCOMPARE(titles(sectionsOf(l, Numbering::Manual)), QStringList({ "Opener[1-2]", "Section[3-4]" }));
    // The same heads mean nothing under a scheme (sections follow numbers).
    QCOMPARE(titles(sectionsOf(l, Numbering::ShowPartIteration)),
             QStringList({ "1[0-0]", "2[1-1]", "3[2-2]", "4[3-3]", "5[4-4]" }));
}

void ShowTreeSections_Test::aSchemeIsSuggestedFromTheNumbers()
{
    Production p;
    p.lists << listOf(1, "Show", { "0.0.1", "0.0.2", "1.0.1", "1.1.1" }, 10);
    QCOMPARE(suggestNumbering(p), Numbering::ShowPartIteration);
    Production eos;
    eos.lists << listOf(1, "Show", { "100", "101", "101.5", "200", "201" }, 10);
    QCOMPARE(suggestNumbering(eos), Numbering::Hundreds);
    // One hundred only, or plain 1, 2, 3: nothing to suggest.
    Production one;
    one.lists << listOf(1, "Show", { "100", "101", "102" }, 10);
    QCOMPARE(suggestNumbering(one), Numbering::None);
    Production plain;
    plain.lists << listOf(1, "Show", { "1", "2", "3", "4" }, 10);
    QCOMPARE(suggestNumbering(plain), Numbering::None);
    QCOMPARE(suggestNumbering(Production()), Numbering::None);
}

void ShowTreeSections_Test::aCueAddedInASectionStaysInIt()
{
    const CueList l = listOf(1, "Show", { "0.0.1", "1.0.1", "1.0.3", "2.0.1" }, 10);
    const Numbering spi = Numbering::ShowPartIteration;
    // At the end of song 1: the next iteration, not song 2's.
    QCOMPARE(numberInSection(l, spi, 2, "1"), QString("1.0.4"));
    QCOMPARE(numberForInsertIn(l, spi, 2), QString("1.0.4"));
    // At the top of song 2: before 2.0.1, still song 2.
    QCOMPARE(numberInSection(l, spi, 2, "2"), QString("2"));
    // A new section after the last: {S}.0.1, music starts.
    QString key, number;
    QVERIFY(nextSection(l, spi, &key, &number));
    QCOMPARE(key, QString("3"));
    QCOMPARE(number, QString("3.0.1"));
    // An empty list starts at 0 (setup, preshow).
    QVERIFY(nextSection(CueList(), spi, &key, &number));
    QCOMPARE(number, QString("0.0.1"));
    // One level only: "1" .. "3" in section 1 is a point cue, not 2.
    const CueList flat = listOf(2, "Flat", { "1", "3" }, 20);
    QCOMPARE(numberForInsert(flat, 0), QString("2"));
    QCOMPARE(numberForInsertIn(flat, spi, 0), QString("1.5"));
    // Hundreds: after 199, before 200 -> 199.5; the next song 300.
    const CueList eos = listOf(3, "Eos", { "100", "199", "200" }, 30);
    QCOMPARE(numberForInsertIn(eos, Numbering::Hundreds, 1), QString("199.5"));
    QVERIFY(nextSection(eos, Numbering::Hundreds, &key, &number));
    QCOMPARE(number, QString("300"));
    // No scheme: as before.
    QCOMPARE(numberForInsertIn(flat, Numbering::None, 0), QString("2"));
    QVERIFY(!nextSection(l, Numbering::Manual, &key, &number));
}

/****************************************************************************
 * Merge lists into one
 ****************************************************************************/

/* Three lists, one per song: tracking, tracking, non-tracking. Merged, the
   cues are in order with their numbers, each later list's start is Block,
   and every cue puts on stage what it did. */
void ShowTreeSections_Test::mergeKeepsOrderNumbersAndTracking()
{
    Production p;
    p.id = 100;
    p.name = "Dance Hysteria";
    CueList pre = listOf(1, "Preshow", {}, 0);
    pre.cues << cue(10, "0.0.1", { layer(101, Front, { look(KeyDimmer, Half) }) })
             << cue(11, "0.0.2", { layer(111, Back, { look(KeyColour, Blue) }) });
    CueList song = listOf(2, "Everybody Scream", {}, 0);
    song.cues << cue(20, "1.0.1", { layer(201, Back, { look(KeyDimmer, Full) }) })
              << cue(21, "1.1.1", { layer(211, Front, { look(KeyColour, Red) }) });
    CueList end = listOf(3, "End", {}, 0);
    end.mode = ListMode::NonTracking;
    end.cues << cue(30, "2.0.1", { layer(301, Front, { look(KeyDimmer, Full) }) })
             << cue(31, "2.0.2", { layer(311, Back, { look(KeyDimmer, Half) }) });
    p.lists << pre << song << end;
    p.mainList = 2;

    QHash<quint32, Stage> before;
    for (const CueList &l : p.lists)
        for (const ResolvedCue &rc : resolve(l))
            before.insert(rc.cueId, stageOf(rc));

    MergeReport report;
    QString why;
    QVERIFY2(mergeLists(p, { 1, 2, 3 }, &report, &why), qPrintable(why));
    QCOMPARE(p.lists.count(), 1);
    const CueList &m = p.lists.first();
    QCOMPARE(m.id, quint32(1));
    QCOMPARE(m.name, QString("Dance Hysteria"));
    QCOMPARE(p.mainList, quint32(1));
    QCOMPARE(numbersOf(m), QStringList({ "0.0.1", "0.0.2", "1.0.1", "1.1.1", "2.0.1", "2.0.2" }));
    QList<quint32> ids;
    for (const Cue &c : m.cues)
        ids << c.id;
    QCOMPARE(ids, QList<quint32>({ 10, 11, 20, 21, 30, 31 }));
    QVERIFY(report.renumbered.isEmpty());
    QVERIFY2(report.notes.isEmpty(), qPrintable(report.notes.join("; ")));
    // Block at each former list start; the non-tracking list's cues keep
    // playing Block in a tracking list.
    QCOMPARE(m.cues.at(2).mode, CueMode::Block);
    QCOMPARE(m.cues.at(3).mode, CueMode::ListDefault);
    QCOMPARE(m.cues.at(4).mode, CueMode::Block);
    QCOMPARE(m.cues.at(5).mode, CueMode::Block);
    QCOMPARE(report.blocked, QList<quint32>({ 20 }));     // 30 was Block already
    // Tracking: every cue plays as it did.
    for (const ResolvedCue &rc : resolve(m))
        QVERIFY2(stageOf(rc) == before.value(rc.cueId), qPrintable(QString::number(rc.cueId)));
    // The scheme the numbers show, and a section per list named after it.
    QCOMPARE(p.numbering, Numbering::ShowPartIteration);
    QCOMPARE(titles(sectionsOf(m, p.numbering)),
             QStringList({ "0 — Preshow[0-1]", "1 — Everybody Scream[2-3]", "2 — End[4-5]" }));

    // Refused: one list, a list twice, a Looks section.
    Production q;
    q.lists << listOf(1, "A", { "1" }, 10) << listOf(2, "B", { "2" }, 20);
    QVERIFY(!mergeLists(q, { 1 }, nullptr, &why));
    QVERIFY(!mergeLists(q, { 1, 1 }, nullptr, &why));
    q.lists[1].looks = true;
    QVERIFY(!mergeLists(q, { 1, 2 }, nullptr, &why));
    QCOMPARE(q.lists.count(), 2);
}

/* Two songs both numbered from 1: the second's clashing numbers change
   (and are said), the rest keep theirs; the order is the lists'. With
   numbers that say nothing, the sections are made by hand (Manual). */
void ShowTreeSections_Test::mergeRenumbersOnlyOnAClash()
{
    Production p;
    p.lists << listOf(1, "Opener", { "1", "2", "3" }, 10) << listOf(2, "Closer", { "3", "7", "8" }, 20);
    MergeReport report;
    QVERIFY(mergeLists(p, { 1, 2 }, &report));
    const CueList &m = p.lists.first();
    QCOMPARE(m.cues.count(), 6);
    QCOMPARE(m.cues.at(3).id, quint32(20));
    const QStringList numbers = numbersOf(m);
    QCOMPARE(numbers.mid(0, 3), QStringList({ "1", "2", "3" }));
    QVERIFY(numbers.at(3) != "3");
    QVERIFY(compareCueNumbers(numbers.at(3), "3") > 0 && compareCueNumbers(numbers.at(3), "7") < 0);
    QCOMPARE(numbers.mid(4), QStringList({ "7", "8" }));
    QCOMPARE(report.renumbered, QStringList({ QString("Closer 3 → %1").arg(numbers.at(3)) }));
    QCOMPARE(p.numbering, Numbering::Manual);
    QCOMPARE(titles(sectionsOf(m, p.numbering)), QStringList({ "Opener[0-2]", "Closer[3-5]" }));

    // A cue numbered as another's sub-cue clashes too (show control would
    // fire the cue for the sub-cue).
    Production s;
    s.lists << listOf(1, "A", { "1", "2" }, 10) << listOf(2, "B", { "2.1", "5" }, 20);
    CueList subs;
    subs.id = 9;
    subs.subOf = 11;
    subs.cues << cue(90, "1");
    s.lists << subs;
    QVERIFY(mergeLists(s, { 1, 2 }, &report));
    QCOMPARE(report.renumbered.count(), 1);
    QVERIFY(numbersOf(s.lists.first()).at(2) != "2.1");
}

/* A list starting with a Cue-only cue: it played from nothing and passed
   nothing on, so the cue after it started from nothing too. Both start
   from nothing in the merged list. */
void ShowTreeSections_Test::mergeKeepsACueOnlyStart()
{
    Production p;
    CueList a = listOf(1, "A", {}, 0);
    a.cues << cue(10, "1", { layer(101, Front, { look(KeyDimmer, Full) }) });
    CueList b = listOf(2, "B", {}, 0);
    b.cues << cue(20, "2", { layer(201, Back, { look(KeyDimmer, Full) }) }, CueMode::CueOnly)
           << cue(21, "3", { layer(211, Back, { look(KeyColour, Blue) }) })
           << cue(22, "4", { layer(221, Front, { look(KeyColour, Red) }) });
    p.lists << a << b;
    QHash<quint32, Stage> before;
    for (const CueList &l : p.lists)
        for (const ResolvedCue &rc : resolve(l))
            before.insert(rc.cueId, stageOf(rc));
    MergeReport report;
    QVERIFY(mergeLists(p, { 1, 2 }, &report));
    QCOMPARE(report.blocked, QList<quint32>({ 20, 21 }));
    for (const ResolvedCue &rc : resolve(p.lists.first()))
        QVERIFY2(stageOf(rc) == before.value(rc.cueId), qPrintable(QString::number(rc.cueId)));
    QVERIFY(report.notes.isEmpty());
}

/****************************************************************************
 * The controller
 ****************************************************************************/

void ShowTreeSections_Test::controllerSectionsAndBlock()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Show");
    const quint32 list = st->addCueList(prod, "Show");
    QVERIFY(st->sections(list).isEmpty());
    QVERIFY(st->addSection(list, "Preshow") == invalidId());    // None: no sections
    QVERIFY(st->setNumbering(prod, Numbering::ShowPartIteration));
    QVERIFY(!st->setNumbering(prod, Numbering::ShowPartIteration));     // no change, no step
    QCOMPARE(st->numbering(prod), Numbering::ShowPartIteration);

    // + Section: 0.0.1 (setup), then 1.0.1.
    const quint32 setup = st->addSection(list, "Preshow");
    QCOMPARE(st->cue(setup)->number, QString("0.0.1"));
    const quint32 song = st->addSection(list, "Everybody Scream");
    QCOMPARE(st->cue(song)->number, QString("1.0.1"));
    // + Cue after the preshow's last cue stays in the preshow.
    const quint32 announcer = st->addCue(list, 0, "Announcer");
    QCOMPARE(st->cue(announcer)->number, QString("0.0.2"));
    QList<Section> s = st->sections(list);
    QCOMPARE(titles(s), QStringList({ "0 — Preshow[0-1]", "1 — Everybody Scream[2-2]" }));
    // A cue at the top of the song.
    const quint32 top = st->addCueInSection(list, s.at(1), "Count-in");
    QCOMPARE(st->cue(top)->number, QString("1"));
    Section in;
    QVERIFY(st->sectionOfCue(top, &in));
    QCOMPARE(in.key, QString("1"));

    // Rename; empty takes the name away.
    s = st->sections(list);
    QVERIFY(st->setSectionName(list, s.at(1), "Everybody Scream (encore)"));
    QCOMPARE(sectionTitle(st->sections(list).at(1)), QString("1 — Everybody Scream (encore)"));
    QVERIFY(st->setSectionName(list, st->sections(list).at(1), ""));
    QCOMPARE(sectionTitle(st->sections(list).at(1)), QString("1"));
    QVERIFY(st->undo());
    QCOMPARE(sectionTitle(st->sections(list).at(1)), QString("1 — Everybody Scream (encore)"));

    // Block at section start: the section's first cue.
    s = st->sections(list);
    QVERIFY(!st->sectionBlocks(list, s.at(1)));
    QVERIFY(st->setSectionBlock(list, s.at(1), true));
    QCOMPARE(st->cue(top)->mode, CueMode::Block);
    QVERIFY(st->sectionBlocks(list, s.at(1)));
    QVERIFY(st->setSectionBlock(list, s.at(1), false));
    QCOMPARE(st->cue(top)->mode, CueMode::ListDefault);

    // By hand (QLab): a section begins at a cue; removing it joins it up.
    QVERIFY(st->setNumbering(prod, Numbering::Manual));
    QVERIFY(st->sections(list).isEmpty());
    QCOMPARE(st->addSection(list, "Song", song), song);
    QCOMPARE(titles(st->sections(list)), QStringList({ "Song[3-3]" }));
    const QList<Section> manual = st->sections(list);
    const quint32 first = st->addCueInSection(list, manual.first(), "Intro");
    QCOMPARE(st->sections(list).first().headCue, first);
    QVERIFY(st->removeSection(list, first));
    QVERIFY(st->sections(list).isEmpty());
}

/* The user's case: a list per song, GO played only the first. Merged, the
   one list's chaser has every cue, the others' chasers are gone, Undo puts
   them back. */
void ShowTreeSections_Test::mergeInTheShowPlaysEveryList()
{
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Front");
    m_doc->addFixtureGroup(g);
    g->assignFixture(m_fixtures[0]);
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, m_doc);
    full->setName("Full");
    full->setValue(255);
    m_doc->addPalette(full);

    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance Hysteria");
    const quint32 pre = st->addCueList(prod, "00-Preshow");
    const quint32 song = st->addCueList(prod, "01-Everybody Scream");
    const quint32 p1 = st->addCue(pre, -1, "Setup");
    const quint32 s1 = st->addCue(song, -1, "Music starts");
    const quint32 s2 = st->addCue(song, 0, "Chorus");
    st->setTargetLook(p1, groupTarget(g->id()), st->lookForPalette(full->id()));
    st->setTargetLook(s1, groupTarget(g->id()), st->lookForPalette(full->id()));
    QCOMPARE(st->mainList(prod), pre);
    const quint32 songChaser = st->cueList(song)->chaser;
    QVERIFY(m_doc->function(songChaser) != nullptr);
    QCOMPARE(qobject_cast<Chaser *>(m_doc->function(st->cueList(pre)->chaser))->stepsCount(), 1);

    MergeReport report;
    QString why;
    QVERIFY2(st->mergeLists(prod, { pre, song }, &report, &why), qPrintable(why));
    QCOMPARE(st->playableLists(prod), QList<quint32>({ pre }));
    QCOMPARE(st->mainList(prod), pre);
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(pre)->chaser));
    QVERIFY(ch != nullptr);
    QCOMPARE(ch->stepsCount(), 3);
    QCOMPARE(st->cueOfStep(pre, 1), s1);
    QCOMPARE(st->cueOfStep(pre, 2), s2);
    QVERIFY(m_doc->function(songChaser) == nullptr);
    QCOMPARE(st->numbering(prod), Numbering::ShowPartIteration);
    QCOMPARE(titles(st->sections(pre)),
             QStringList({ "0 — 00-Preshow[0-0]", "1 — 01-Everybody Scream[1-2]" }));
    QCOMPARE(st->undoText(), QString("Merge lists into one"));

    QVERIFY(st->undo());
    QCOMPARE(st->playableLists(prod).count(), 2);
    QCOMPARE(st->cueList(song)->cues.count(), 2);
    QCOMPARE(st->numbering(prod), Numbering::None);
}

void ShowTreeSections_Test::sectionsSaveAndLoad()
{
    Production p;
    p.id = 1;
    p.name = "Show";
    p.numbering = Numbering::Hundreds;
    CueList l = listOf(2, "Show", { "100", "200" }, 10);
    SectionHead h;
    h.key = "200";
    h.name = "Everybody Scream";
    SectionHead m;
    m.cue = 10;
    m.name = "By hand";
    l.sections << h << m;
    p.lists << l;
    const QByteArray xml = productionXml(p);
    QVERIFY(xml.contains("Numbering=\"Hundreds\""));
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    Production back;
    QVERIFY(ShowTree::loadXML(r, back));
    QCOMPARE(back.numbering, Numbering::Hundreds);
    QCOMPARE(back.lists.first().sections.count(), 2);
    QCOMPARE(back.lists.first().sections.at(0).key, QString("200"));
    QCOMPARE(back.lists.first().sections.at(1).cue, quint32(10));
    QCOMPARE(productionXml(back), xml);

    // An older show: no Numbering, no sections -- as it was.
    Production old;
    old.id = 1;
    old.name = "Old";
    old.lists << listOf(2, "L", { "1", "2" }, 10);
    const QByteArray oldXml = productionXml(old);
    QVERIFY(!oldXml.contains("Numbering"));
    QVERIFY(!oldXml.contains("<Section"));
    QXmlStreamReader r2(oldXml);
    r2.readNextStartElement();
    QVERIFY(ShowTree::loadXML(r2, back));
    QCOMPARE(back.numbering, Numbering::None);
    QVERIFY(back.lists.first().sections.isEmpty());
}

/* Pure: songs 0,1,2 (cues 0.0.1 | 1.0.1 1.0.2 | 2.0.1), played 2,0,1 with 1
   left out; a new song 3 follows the song before it in number order. */
void ShowTreeSections_Test::runningOrderPureRules()
{
    CueList l = listOf(2, "Show", { "0.0.1", "1.0.1", "1.0.2", "2.0.1" }, 10);
    const Numbering n = Numbering::ShowPartIteration;
    QCOMPARE(playOrder(l, n, {}), QList<int>({ 0, 1, 2, 3 }));

    auto song = [](const char *id, bool skip = false) {
        RunningSong r;
        r.id = QString::fromLatin1(id);
        r.skip = skip;
        return r;
    };
    QList<bool> skipped;
    QCOMPARE(playOrder(l, n, { song("2"), song("0"), song("1", true) }, &skipped), QList<int>({ 3, 0, 1, 2 }));
    QCOMPARE(skipped, QList<bool>({ false, true, true, false }));

    // A song the order doesn't know goes after the one before it.
    const QList<Section> secs = sectionsOf(l, n);
    QStringList ids;
    for (const RunningSong &r : resolveRunningOrder(secs, { song("2"), song("0") }))
        ids << r.id;
    QCOMPARE(ids, QStringList({ "2", "0", "1" }));
    // A song that is gone is dropped; no scheme: number order.
    QCOMPARE(resolveRunningOrder(secs, { song("9"), song("1") }).count(), 3);
    QCOMPARE(playOrder(l, Numbering::None, { song("2"), song("0") }), QList<int>({ 0, 1, 2, 3 }));
}

/* The chaser's steps come in the running order, a song left out is skipped,
   no cue moves or is renumbered, and cueOfStep follows. */
void ShowTreeSections_Test::runningOrderPlaysInTheChaser()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Show");
    const quint32 list = st->addCueList(prod, "Show");
    const quint32 a = st->addCue(list, -1, "Opener");
    QVERIFY(st->setNumbering(prod, Numbering::Hundreds));
    QString why;
    Cue c = *st->cue(a);
    c.number = "100";
    QVERIFY2(st->setCue(c, &why), qPrintable(why));
    const quint32 b = st->addCue(list, 0, "Song A");
    c = *st->cue(b);
    c.number = "200";
    QVERIFY(st->setCue(c, &why));
    const quint32 d = st->addCue(list, 1, "Song B");
    c = *st->cue(d);
    c.number = "300";
    QVERIFY(st->setCue(c, &why));

    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QVERIFY(ch != nullptr);
    QCOMPARE(ch->stepsCount(), 3);
    QVERIFY(!st->hasRunningOrder(prod));

    QList<RunningSong> order = st->runningOrder(prod);
    QCOMPARE(order.count(), 3);
    QCOMPARE(order.at(0).id, QString("100"));
    std::swap(order[1], order[2]);                  // 100, 300, 200
    order[2].skip = true;                           // 200 not tonight
    QVERIFY(st->setRunningOrder(prod, order));
    QCOMPARE(st->undoText(), QString("Running order"));
    QVERIFY(st->hasRunningOrder(prod));

    QCOMPARE(st->cueOfStep(list, 0), a);
    QCOMPARE(st->cueOfStep(list, 1), d);
    QCOMPARE(st->cueOfStep(list, 2), b);
    QCOMPARE(ch->stepCue(1), QString("300"));
    QVERIFY(!ch->isStepSkipped(1));
    QVERIFY(ch->isStepSkipped(2));
    QCOMPARE(ch->nextPlayableStep(0, 1), 1);
    QCOMPARE(ch->nextPlayableStep(1, 1), -1);
    // The cues themselves stay in number order, numbers untouched.
    QCOMPARE(st->cueList(list)->cues.at(1).number, QString("200"));
    QVERIFY(!st->cueList(list)->cues.at(1).skip);
    QCOMPARE(st->runningCue(list), ShowTree::invalidId());

    // Back to number order.
    QVERIFY(st->resetRunningOrder(prod));
    QVERIFY(!st->hasRunningOrder(prod));
    QCOMPARE(st->cueOfStep(list, 1), b);
    QVERIFY(!ch->isStepSkipped(2));
    QCOMPARE(ch->stepCue(1), QString("200"));
    // The same order as number order with every song in is none.
    QVERIFY(!st->setRunningOrder(prod, st->runningOrder(prod)));
    QVERIFY(!st->hasRunningOrder(prod));
    // Undo: the order again.
    QVERIFY(st->undo());
    QVERIFY(st->hasRunningOrder(prod));
    QVERIFY(ch->isStepSkipped(2));
}

void ShowTreeSections_Test::runningOrderSavesLoadsAndResets()
{
    Production p;
    p.id = 1;
    p.name = "Show";
    p.numbering = Numbering::Hundreds;
    p.lists << listOf(2, "Show", { "100", "200" }, 10);
    QVERIFY(!productionXml(p).contains("RunningOrder"));
    RunningSong a, b;
    a.id = "200";
    b.id = "100";
    b.skip = true;
    p.runningOrder << a << b;
    const QByteArray xml = productionXml(p);
    QVERIFY(xml.contains("<RunningOrder>"));
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    Production back;
    QVERIFY(ShowTree::loadXML(r, back));
    QCOMPARE(back.runningOrder.count(), 2);
    QCOMPARE(back.runningOrder.at(0).id, QString("200"));
    QVERIFY(!back.runningOrder.at(0).skip);
    QVERIFY(back.runningOrder.at(1).skip);
    QCOMPARE(back.lists.count(), 1);
    QCOMPARE(productionXml(back), xml);
}

QTEST_GUILESS_MAIN(ShowTreeSections_Test)

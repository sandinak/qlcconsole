/*
  Q Light Controller Plus
  cuesheet_test.cpp

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

#include <QTemporaryDir>
#include <QtTest>

#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "cuesheet.h"
#include "doc.h"

#include "cuesheet_test.h"

using namespace ShowTree;

/* The stage manager's paper: the cue sheet in running order and the tracking
   sheet, built offscreen from a small show. */

namespace
{
Look look(const QString &key, quint32 palette)
{
    Look l;
    l.key = key;
    l.palettes << palette;
    return l;
}

QStringList column(const QList<QStringList> &t, int c)
{
    QStringList out;
    for (const QStringList &line : t)
        out << (c < line.size() ? line.at(c) : QString());
    return out;
}
} // namespace

void CueSheet_Test::init()
{
    m_doc = new Doc(this);
}

void CueSheet_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

/* Preshow: 0.0.1 "House" (Front half), 0.0.2 "Walk-in" (Front full).
   Song 1 "Everybody Scream": 1.0.1 "Hit it" (Front red + Back full, 2s in,
   auto after 4s), 1.0.2 "Hold" (Back off the stage by Release; Front
   dimmer changes to half, the red tracks). */
void CueSheet_Test::buildShow()
{
    QList<quint32> fixtures;
    for (int i = 0; i < 4; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dim %1").arg(i + 1));
        fxi->setAddress(quint32(i));
        fxi->setUniverse(0);
        fxi->setChannels(1);
        m_doc->addFixture(fxi);
        fixtures << fxi->id();
    }
    auto group = [&](const QString &name, int a, int b) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        m_doc->addFixtureGroup(g);
        g->assignFixture(fixtures.at(a));
        g->assignFixture(fixtures.at(b));
        return g->id();
    };
    m_front = group("Front", 0, 1);
    m_back = group("Back", 2, 3);
    auto dimmer = [&](const QString &name, int level) {
        QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
        p->setName(name);
        p->setValue(level);
        m_doc->addPalette(p);
        return p->id();
    };
    m_half = dimmer("Half", 128);
    m_full = dimmer("Full", 255);
    QLCPalette *r = new QLCPalette(QLCPalette::Color, m_doc);
    r->setName("Red");
    r->setValue(QColor(Qt::red));
    m_doc->addPalette(r);
    m_red = r->id();

    ShowTreeController *st = m_doc->showTree();
    m_prod = st->addProduction("Dance Hysteria");
    m_list = st->addCueList(m_prod, "Main");
    QVERIFY(st->setNumbering(m_prod, Numbering::ShowPartIteration));
    m_cues[0] = st->addSection(m_list, "Preshow");              // 0.0.1
    m_cues[1] = st->addCue(m_list, 0, "Walk-in");               // 0.0.2
    m_cues[2] = st->addSection(m_list, "Everybody Scream");     // 1.0.1
    m_cues[3] = st->addCue(m_list, 2, "Hold");                  // 1.0.2

    for (int i = 0; i < 4; ++i)
    {
        Cue named = *st->cue(m_cues[i]);
        if (i == 0)
            named.name = "House";
        if (i == 2)
            named.name = "Hit it";
        QVERIFY(st->setCue(named));
    }

    Target front, back;
    front.groups << m_front;
    back.groups << m_back;
    const quint32 l0 = st->addLayer(m_cues[0], front);
    QVERIFY(st->setLayerLook(m_cues[0], l0, look(KeyDimmer, m_half)));
    const quint32 l1 = st->addLayer(m_cues[1], front);
    QVERIFY(st->setLayerLook(m_cues[1], l1, look(KeyDimmer, m_full)));
    const quint32 l2 = st->addLayer(m_cues[2], front);
    QVERIFY(st->setLayerLook(m_cues[2], l2, look(KeyColour, m_red)));
    const quint32 l2b = st->addLayer(m_cues[2], back);
    QVERIFY(st->setLayerLook(m_cues[2], l2b, look(KeyDimmer, m_full)));
    const quint32 l3 = st->addLayer(m_cues[3], front);
    QVERIFY(st->setLayerLook(m_cues[3], l3, look(KeyDimmer, m_half)));
    const quint32 l3b = st->addLayer(m_cues[3], back);
    QVERIFY(st->setLayerRelease(m_cues[3], l3b, true));
}

void CueSheet_Test::cueSheetRowsFollowTheRunningOrder()
{
    buildShow();
    ShowTreeController *st = m_doc->showTree();

    Cue c = *st->cue(m_cues[2]);
    c.fadeIn = 2000;
    c.fadeOut = 1000;
    c.hold = 4000;
    c.note = "Strobes at the drop";
    QVERIFY(st->setCue(c));

    const CueSheet sheet = CueSheet::build(m_doc, m_prod);
    QCOMPARE(sheet.productionName(), QString("Dance Hysteria"));
    QCOMPARE(sheet.lists().count(), 1);
    const CueSheet::ListBlock &b = sheet.lists().first();
    QCOMPARE(b.name, QString("Main"));

    // Production -> list -> song -> cue: a heading for each song, its cues under it.
    QStringList shape;
    for (const CueSheet::Row &r : b.rows)
        shape << (r.kind == CueSheet::Row::Section ? QString("[%1]").arg(r.name) : r.number + " " + r.name);
    QCOMPARE(shape, QStringList({ "[0 — Preshow]", "0.0.1 House", "0.0.2 Walk-in",
                                  "[1 — Everybody Scream]", "1.0.1 Hit it", "1.0.2 Hold" }));
    QCOMPARE(sheet.cueCount(), 4);

    // The columns of the song's first cue.
    const CueSheet::Row &hit = b.rows.at(4);
    QCOMPARE(hit.cueId, m_cues[2]);
    QCOMPARE(hit.section, QString("1 — Everybody Scream"));
    QCOMPARE(hit.fadeIn, QString("2s"));
    QCOMPARE(hit.fadeOut, QString("1s"));
    QCOMPARE(hit.follow, QString("auto after 4s"));
    QCOMPARE(hit.note, QString("Strobes at the drop"));
    // A cue that waits for GO says so; no fades is "0".
    const CueSheet::Row &hold = b.rows.at(5);
    QCOMPARE(hold.follow, QString("GO"));
    QCOMPARE(hold.fadeIn, QString("0"));

    // The sheet as a table: one header, a line per cue, in that order.
    const QList<QStringList> t = sheet.cueTable();
    QCOMPARE(t.count(), 5);
    QCOMPARE(t.first(), CueSheet::header());
    QCOMPARE(column(t, 2), QStringList({ "Cue", "0.0.1", "0.0.2", "1.0.1", "1.0.2" }));
    QCOMPARE(column(t, 0).at(3), QString("Main"));
    QCOMPARE(column(t, 1).at(3), QString("1 — Everybody Scream"));
}

void CueSheet_Test::needsWorkIsAColumn()
{
    buildShow();
    QCOMPARE(CueSheet::header().last(), QString("Needs work"));
    const CueSheet before = CueSheet::build(m_doc, m_prod);
    for (const QStringList &line : before.cueTable().mid(1))
        QCOMPARE(line.last(), QString());

    // Tagged from the stage (the pad) or the cue page: that cue's line says so.
    QVERIFY(m_doc->showTree()->setCuesNeedsWork({ m_cues[3] }, true));
    const CueSheet sheet = CueSheet::build(m_doc, m_prod);
    const QList<QStringList> t = sheet.cueTable();
    QCOMPARE(column(t, 2), QStringList({ "Cue", "0.0.1", "0.0.2", "1.0.1", "1.0.2" }));
    QCOMPARE(column(t, t.first().count() - 1), QStringList({ "Needs work", "", "", "", "yes" }));
    QVERIFY(sheet.cueHtml("T").contains("<td>yes</td>"));
    QVERIFY(sheet.cueCsv().contains("Needs work"));
}

void CueSheet_Test::subCuesHitsAndTimes()
{
    buildShow();
    ShowTreeController *st = m_doc->showTree();

    // A skipped cue is marked, its sub-cues follow it, hits show where they are armed.
    QVERIFY(st->setCuesSkipped({ m_cues[1] }, true));
    const quint32 sub = st->addSubCue(m_cues[2], "Strobe");
    QVERIFY(sub != invalidId());
    QVERIFY(st->setSubCueFollow(sub, true, 500));
    const quint32 cueHit = st->addHit(HitScope::Cue, m_cues[2], "Flash");
    const quint32 listHit = st->addHit(HitScope::List, m_list, "Blinder");
    const quint32 globalHit = st->addHit(HitScope::Global, m_prod, "Blackout");
    QVERIFY(cueHit != invalidId() && listHit != invalidId() && globalHit != invalidId());

    const CueSheet sheet = CueSheet::build(m_doc, m_prod);
    const CueSheet::ListBlock &b = sheet.lists().first();
    QStringList numbers;
    for (const CueSheet::Row &r : b.rows)
        if (r.kind != CueSheet::Row::Section)
            numbers << r.number;
    QCOMPARE(numbers, QStringList({ "0.0.1", "0.0.2", "1.0.1", "1.0.1.1", "1.0.2" }));

    const CueSheet::Row *skip = nullptr, *sr = nullptr, *flash = nullptr;
    for (const CueSheet::Row &r : b.rows)
    {
        if (r.cueId == m_cues[1])
            skip = &r;
        if (r.cueId == sub)
            sr = &r;
        if (r.cueId == m_cues[2])
            flash = &r;
    }
    QVERIFY(skip && sr && flash);
    QVERIFY(skip->skipped);
    QCOMPARE(sr->kind, CueSheet::Row::SubCue);
    QCOMPARE(sr->follow, QString("auto after 500ms"));
    QCOMPARE(flash->hits, QString("Flash"));
    QVERIFY(b.hitsThroughout.contains("Blinder"));
    QVERIFY(b.hitsThroughout.contains("Blackout"));
    QVERIFY(!b.hitsThroughout.contains("Flash"));

    // The table says so, too.
    const QList<QStringList> t = sheet.cueTable();
    QVERIFY(column(t, 7).at(2).startsWith("SKIPPED"));
    QCOMPARE(column(t, 8).at(3), QString("Flash"));
}

void CueSheet_Test::trackingMarksWhatChanges()
{
    buildShow();
    const CueSheet sheet = CueSheet::build(m_doc, m_prod);
    QCOMPARE(sheet.tracking().count(), 1);
    const CueSheet::TrackingBlock &tb = sheet.tracking().first();

    QStringList numbers;
    for (const CueSheet::TrackingColumn &c : tb.columns)
        numbers << c.number;
    QCOMPARE(numbers, QStringList({ "0.0.1", "0.0.2", "1.0.1", "1.0.2" }));
    // Rows: Front, then Back (first lit in the song).
    QCOMPARE(tb.rows.count(), 2);
    QVERIFY(tb.rows.at(0).label.contains("Front"));
    QVERIFY(tb.rows.at(1).label.contains("Back"));

    auto cell = [&](int row, int col) { return tb.rows.at(row).cells.at(col); };

    // Front: Half set; Full changes it; the colour arrives and the dimmer
    // tracks; then the dimmer changes while the colour tracks.
    QCOMPARE(cell(0, 0).values.count(), 1);
    QVERIFY(cell(0, 0).values.first().changed);
    QCOMPARE(cell(0, 0).values.first().text, QString("Half"));
    QVERIFY(cell(0, 1).values.first().changed);
    QCOMPARE(cell(0, 1).values.first().text, QString("Full"));

    const CueSheet::Cell song = cell(0, 2);
    QCOMPARE(song.values.count(), 2);
    int changed = 0, tracked = 0;
    for (const CueSheet::Value &v : song.values)
    {
        if (v.text == "Red")
        {
            QVERIFY(v.changed);
            changed++;
        }
        else
        {
            QCOMPARE(v.text, QString("Full"));
            QVERIFY(!v.changed);                // tracked in from 0.0.2
            tracked++;
        }
    }
    QCOMPARE(changed, 1);
    QCOMPARE(tracked, 1);
    QVERIFY(!song.allTracked());

    QStringList last;
    for (const CueSheet::Value &v : cell(0, 3).values)
        last << (v.changed ? v.text : "(" + v.text + ")");
    QCOMPARE(last, QStringList({ "(Red)", "Half" }));
    QCOMPARE(CueSheet::cellText(cell(0, 3)), QString("(Red) · Half"));

    // Back: not on stage before the song, new in it, released after.
    QVERIFY(cell(1, 0).isEmpty());
    QVERIFY(cell(1, 1).isEmpty());
    QCOMPARE(cell(1, 2).values.first().text, QString("Full"));
    QVERIFY(cell(1, 2).values.first().changed);
    // Released in 1.0.2: still on stage for it (tracked, and marked), out after.
    QCOMPARE(cell(1, 3).values.count(), 2);
    QCOMPARE(cell(1, 3).values.first().text, QString("Full"));
    QVERIFY(!cell(1, 3).values.first().changed);
    QCOMPARE(cell(1, 3).values.last().text, QString("release"));
    QVERIFY(cell(1, 3).values.last().changed);
    ShowTreeController *st = m_doc->showTree();
    const quint32 after = st->addCue(m_list, 3, "Black");
    const CueSheet more = CueSheet::build(m_doc, m_prod);
    const CueSheet::TrackingRow &backRow = more.tracking().first().rows.at(1);
    QCOMPARE(more.tracking().first().columns.last().cueId, after);
    QCOMPARE(backRow.cells.last().values.count(), 1);
    QCOMPARE(backRow.cells.last().values.first().text, QString("out"));
    QVERIFY(backRow.cells.last().values.first().changed);

    // A cue that changes nothing for a layer is all tracked: set Full in 0.0.2
    // again on cue 1.0.2's Front would not be a change either.
    const QList<QStringList> t = sheet.trackingTable();
    QCOMPARE(t.at(0), QStringList({ "Main" }));
    QCOMPARE(t.at(1), QStringList({ "Layer", "0.0.1", "0.0.2", "1.0.1", "1.0.2" }));
    QCOMPARE(t.at(3).at(4), QString("(Red) · Half"));
}

void CueSheet_Test::runningOrderMovesSongsNotTracking()
{
    buildShow();
    ShowTreeController *st = m_doc->showTree();
    // Tonight: song 1 first, the preshow after.
    QList<RunningSong> order;
    RunningSong a, b;
    const QList<Section> sections = st->sections(m_list);
    QCOMPARE(sections.count(), 2);
    a.id = sectionId(sections.at(1));
    b.id = sectionId(sections.at(0));
    order << a << b;
    QVERIFY(st->setRunningOrder(m_prod, order));

    const CueSheet sheet = CueSheet::build(m_doc, m_prod);
    QStringList numbers;
    for (const CueSheet::Row &r : sheet.lists().first().rows)
        if (r.kind != CueSheet::Row::Section)
            numbers << r.number;
    QCOMPARE(numbers, QStringList({ "1.0.1", "1.0.2", "0.0.1", "0.0.2" }));

    // Tracking resolves in number order whatever tonight's order is.
    QStringList cols;
    for (const CueSheet::TrackingColumn &c : sheet.tracking().first().columns)
        cols << c.number;
    QCOMPARE(cols, QStringList({ "0.0.1", "0.0.2", "1.0.1", "1.0.2" }));

    // A song left out tonight is marked.
    order[1].skip = true;
    QVERIFY(st->setRunningOrder(m_prod, order));
    const CueSheet left = CueSheet::build(m_doc, m_prod);
    int skippedRows = 0;
    for (const CueSheet::Row &r : left.lists().first().rows)
        if (r.kind == CueSheet::Row::Cue && r.skipped)
            skippedRows++;
    QCOMPARE(skippedRows, 2);
}

void CueSheet_Test::csvAndXlsxAndPdf()
{
    buildShow();
    ShowTreeController *st = m_doc->showTree();
    Cue c = *st->cue(m_cues[3]);
    c.note = "=SUM(A1), \"quoted\"";
    QVERIFY(st->setCue(c));

    const CueSheet sheet = CueSheet::build(m_doc, m_prod);
    const QString csv = sheet.cueCsv();
    QStringList lines = csv.split('\n', Qt::SkipEmptyParts);
    QCOMPARE(lines.count(), 5);
    QVERIFY(lines.first().startsWith("Cue list,Song,Cue,Name"));
    // Defused and quoted, as the other reports do.
    QVERIFY(lines.last().contains("\"'=SUM(A1), \"\"quoted\"\"\""));

    const QString tcsv = sheet.trackingCsv();
    QVERIFY(tcsv.contains("(Red) · Half"));
    QVERIFY(tcsv.startsWith("Main\nLayer,0.0.1,0.0.2,1.0.1,1.0.2\n"));

    const QByteArray xlsx = sheet.cueXlsx();
    QVERIFY(xlsx.startsWith("PK"));
    QVERIFY(sheet.trackingXlsx().startsWith("PK"));

    const QString html = sheet.trackingHtml("Tracking");
    QVERIFY(html.contains("class=\"tracked\">(Red)"));
    QVERIFY(html.contains("<b>Half</b>"));
    QVERIFY(sheet.cueHtml("Cues").contains("Everybody Scream"));
    // Wider than a page: more tables, same layer names.
    QCOMPARE(sheet.trackingHtml("T", 2).count("<table"), 2);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    QVERIFY2(sheet.writeCuePdf(dir.filePath("cues.pdf"), "Cues", &error), qPrintable(error));
    QVERIFY2(sheet.writeTrackingPdf(dir.filePath("tracking.pdf"), "Tracking", &error), qPrintable(error));
    QVERIFY2(sheet.writeCueXlsx(dir.filePath("cues.xlsx"), &error), qPrintable(error));
    QVERIFY2(sheet.writeTrackingXlsx(dir.filePath("tracking.xlsx"), &error), qPrintable(error));
    for (const char *name : { "cues.pdf", "tracking.pdf" })
    {
        QFile f(dir.filePath(name));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(f.readAll().startsWith("%PDF"));
    }
    // A path that can't be written says why.
    QVERIFY(!sheet.writeCuePdf(dir.filePath("nope/none.pdf"), "x", &error));
    QVERIFY(!error.isEmpty());
}

void CueSheet_Test::anEmptyShowMakesEmptySheets()
{
    const CueSheet none = CueSheet::build(m_doc, 12345);
    QVERIFY(none.isEmpty());
    QCOMPARE(none.cueCount(), 0);
    QCOMPARE(none.cueTable().count(), 1);               // just the header
    QVERIFY(none.trackingTable().isEmpty());

    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Empty");
    st->addCueList(prod, "Main");
    const CueSheet sheet = CueSheet::build(m_doc, prod);
    QCOMPARE(sheet.lists().count(), 1);
    QCOMPARE(sheet.cueCount(), 0);
}

QTEST_MAIN(CueSheet_Test)

/*
  Q Light Controller Plus
  patchlabels_test.cpp

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
#include <QTemporaryDir>
#include <QtTest>

#include "monitorproperties.h"
#include "inputoutputmap.h"
#include "patchlabels.h"
#include "patchlist.h"
#include "fixture.h"
#include "doc.h"

#include "patchlabels_test.h"

static PatchList::Row row(const QString &name, quint32 universe, quint32 address,
                          quint32 channels = 4, bool patched = true)
{
    PatchList::Row r;
    r.name = name;
    r.universe = universe;
    r.address = address;
    r.channels = channels;
    r.patched = patched;
    return r;
}

static QList<PatchList::Row> rows(int count)
{
    QList<PatchList::Row> out;
    for (int i = 0; i < count; ++i)
        out << row(QStringLiteral("Par %1").arg(i + 1), 0, quint32(i * 4));
    return out;
}

/* The pages in a PDF: its page objects ("/Type /Page", not "/Pages"). */
static int pdfPages(const QByteArray &pdf)
{
    static const QRegularExpression page(QStringLiteral("/Type\\s*/Page(?![a-zA-Z])"));
    int n = 0;
    QRegularExpressionMatchIterator it = page.globalMatch(QString::fromLatin1(pdf));
    while (it.hasNext())
    {
        it.next();
        n++;
    }
    return n;
}

static QByteArray readAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

static bool sameMm(qreal a, qreal b)
{
    return qAbs(a - b) < 0.01;
}

void PatchLabels_Test::theStocksAreThere()
{
    const QList<PatchLabels::Sheet> all = PatchLabels::sheets();
    QVERIFY(all.size() >= 2);
    QCOMPARE(all.first().key, QStringLiteral("avery5160"));
    QCOMPARE(PatchLabels::defaultSheetKey(), QStringLiteral("avery5160"));
    QStringList keys;
    foreach (const PatchLabels::Sheet &s, all)
    {
        QVERIFY(s.isValid());
        QVERIFY(!s.name.isEmpty());
        QVERIFY(!keys.contains(s.key));
        keys << s.key;
        // Every label of a page is on the page.
        for (int i = 0; i < s.perPage(); ++i)
        {
            const QRectF r = PatchLabels::labelRect(s, i);
            QVERIFY2(QRectF(QPointF(0, 0), s.page).adjusted(-0.01, -0.01, 0.01, 0.01).contains(r),
                     qPrintable(QStringLiteral("%1 label %2").arg(s.key).arg(i)));
        }
    }
    QVERIFY(keys.contains(QStringLiteral("roll62")));
}

void PatchLabels_Test::anUnknownStockIsTheDefault()
{
    QCOMPARE(PatchLabels::sheet(QStringLiteral("roll62")).key, QStringLiteral("roll62"));
    QCOMPARE(PatchLabels::sheet(QStringLiteral("no such")).key, QStringLiteral("avery5160"));
    QCOMPARE(PatchLabels::sheet(QString()).key, QStringLiteral("avery5160"));
}

void PatchLabels_Test::avery5160PlacesThirtyAPage()
{
    const PatchLabels::Sheet s = PatchLabels::sheet(QStringLiteral("avery5160"));
    QCOMPARE(s.perPage(), 30);
    QVERIFY(sameMm(s.page.width(), 215.9) && sameMm(s.page.height(), 279.4));     // Letter
    QVERIFY(sameMm(s.label.width(), 66.675) && sameMm(s.label.height(), 25.4)); // 2⅝" × 1"

    int page = -1;
    QRectF r = PatchLabels::labelRect(s, 0, &page);
    QCOMPARE(page, 0);
    QVERIFY(sameMm(r.left(), 4.7625) && sameMm(r.top(), 12.7));

    // Across first: the second label is one column over (2¾").
    r = PatchLabels::labelRect(s, 1, &page);
    QCOMPARE(page, 0);
    QVERIFY(sameMm(r.left(), 4.7625 + 69.85) && sameMm(r.top(), 12.7));

    // The fourth starts the second row.
    r = PatchLabels::labelRect(s, 3, &page);
    QVERIFY(sameMm(r.left(), 4.7625) && sameMm(r.top(), 12.7 + 25.4));

    // The thirtieth is bottom right, and ends half an inch from the bottom.
    r = PatchLabels::labelRect(s, 29, &page);
    QCOMPARE(page, 0);
    QVERIFY(sameMm(r.left(), 4.7625 + 2 * 69.85));
    QVERIFY(sameMm(r.bottom(), 279.4 - 12.7));
    QVERIFY(sameMm(r.right(), 215.9 - 4.7625));

    // The thirty-first starts page two, top left.
    r = PatchLabels::labelRect(s, 30, &page);
    QCOMPARE(page, 1);
    QVERIFY(sameMm(r.left(), 4.7625) && sameMm(r.top(), 12.7));
    PatchLabels::labelRect(s, 59, &page);
    QCOMPARE(page, 1);
    PatchLabels::labelRect(s, 60, &page);
    QCOMPARE(page, 2);
}

void PatchLabels_Test::theRollIsOneAPage()
{
    const PatchLabels::Sheet s = PatchLabels::sheet(QStringLiteral("roll62"));
    QCOMPARE(s.perPage(), 1);
    QVERIFY(sameMm(s.page.width(), 62) && sameMm(s.page.height(), 29));
    for (int i = 0; i < 3; ++i)
    {
        int page = -1;
        const QRectF r = PatchLabels::labelRect(s, i, &page);
        QCOMPARE(page, i);
        QCOMPARE(r, QRectF(0, 0, 62, 29));
    }
}

void PatchLabels_Test::pageCounts()
{
    const PatchLabels::Sheet avery = PatchLabels::sheet(QStringLiteral("avery5160"));
    QCOMPARE(PatchLabels::pageCount(avery, 0), 0);
    QCOMPARE(PatchLabels::pageCount(avery, 1), 1);
    QCOMPARE(PatchLabels::pageCount(avery, 30), 1);
    QCOMPARE(PatchLabels::pageCount(avery, 31), 2);
    QCOMPARE(PatchLabels::pageCount(avery, 61), 3);
    const PatchLabels::Sheet roll = PatchLabels::sheet(QStringLiteral("roll62"));
    QCOMPARE(PatchLabels::pageCount(roll, 7), 7);
}

void PatchLabels_Test::aPatchedLabel()
{
    PatchList::Row r = row(QStringLiteral("Wash 3"), 2, 100, 16);
    r.mode = QStringLiteral("16-channel");
    r.mountedOn = QStringLiteral("Truss FOH · 2.5 m along");
    const PatchLabels::Label l = PatchLabels::label(r);
    QCOMPARE(l.name, QStringLiteral("Wash 3"));
    QCOMPARE(l.address, QStringLiteral("U3 · 101"));            // one-based, both
    QCOMPARE(l.detail, QStringLiteral("16-channel · 16 ch"));
    QCOMPARE(l.mount, QStringLiteral("Truss FOH · 2.5 m along"));

    // No mode: just the channels.
    r.mode.clear();
    QCOMPARE(PatchLabels::label(r).detail, QStringLiteral("16 ch"));
}

void PatchLabels_Test::unpatchedAndClashLabels()
{
    QCOMPARE(PatchLabels::label(row(QStringLiteral("Spare"), 0, 0, 4, false)).address,
             QStringLiteral("not patched"));
    PatchList::Row clash = row(QStringLiteral("Clash"), 0, 11);
    clash.patched = false;
    clash.clash = true;
    QCOMPARE(PatchLabels::label(clash).address, QStringLiteral("clash · U1 · 12"));
}

void PatchLabels_Test::aNetworkLabelCarriesItsIp()
{
    PatchList::Row r = row(QStringLiteral("Strip"), 4, 0, 3);
    r.mode = QStringLiteral("RGB");
    QVERIFY(PatchLabels::label(r).network.isEmpty());        // DMX: no line for it

    r.network = true;
    r.transport = QStringLiteral("WLED");
    r.ip = QStringLiteral("172.18.2.50");
    // A line of its own, not the mode's.
    QCOMPARE(PatchLabels::label(r).detail, QStringLiteral("RGB · 3 ch"));
    QCOMPARE(PatchLabels::label(r).network, QStringLiteral("WLED · 172.18.2.50"));

    // The name it goes by, when it has one: the name alone.
    r.host = QStringLiteral("lantern-2");
    QCOMPARE(PatchLabels::label(r).network, QStringLiteral("WLED · lantern-2"));

    // A host name for its address.
    r.host.clear();
    r.ip = QStringLiteral("lantern-2.local");
    QCOMPARE(PatchLabels::label(r).network, QStringLiteral("WLED · lantern-2.local"));

    r.ip.clear();
    QCOMPARE(PatchLabels::label(r).network, QStringLiteral("WLED · (no IP)"));
}

void PatchLabels_Test::theUniverseIsNamed()
{
    PatchList::Row r = row(QStringLiteral("Wash"), 2, 0);
    QVERIFY(PatchLabels::label(r).universe.isEmpty());       // the default name says nothing
    r.universeName = QStringLiteral("Stage left");
    QCOMPARE(PatchLabels::label(r).address, QStringLiteral("U3 · 1"));
    QCOMPARE(PatchLabels::label(r).universe, QStringLiteral("Stage left"));

    // Not patched: no universe to name.
    r.patched = false;
    QVERIFY(PatchLabels::label(r).universe.isEmpty());
}

void PatchLabels_Test::aLabelFromTheShow()
{
    Doc doc(nullptr);
    Fixture *f = new Fixture(&doc);
    f->setName(QStringLiteral("Front 1"));
    f->setChannels(6);
    f->setUniverse(1);
    f->setAddress(40);
    f->setPatched(true);
    QVERIFY(doc.addFixture(f));
    Fixture *lantern = new Fixture(&doc);
    lantern->setName(QStringLiteral("Lantern 2"));
    lantern->setChannels(3);
    lantern->setUniverse(2);
    lantern->setAddress(0);
    lantern->setPatched(true);
    lantern->setConnection(Fixture::Network);
    lantern->setTransport(Fixture::WledTransport);
    lantern->setIPAddress(QStringLiteral("172.18.2.51"));
    QVERIFY(doc.addFixture(lantern));
    doc.inputOutputMap()->setUniverseName(1, QStringLiteral("Stage left"));
    doc.inputOutputMap()->setTargetAlias(QStringLiteral("172.18.2.51"), QStringLiteral("lantern-2"));

    const QList<PatchList::Row> rs = PatchList::build(&doc).rows();
    QCOMPARE(rs.size(), 2);
    PatchLabels::Label l = PatchLabels::label(rs.at(0));
    QCOMPARE(l.name, QStringLiteral("Front 1"));
    QCOMPARE(l.address, QStringLiteral("U2 · 41"));
    QCOMPARE(l.universe, QStringLiteral("Stage left"));
    QVERIFY2(l.detail.endsWith(QStringLiteral("6 ch")), qPrintable(l.detail));
    QVERIFY(l.network.isEmpty());
    QCOMPARE(l.mount, doc.monitorProperties()->mountDescription(f->id()));

    l = PatchLabels::label(rs.at(1));
    QCOMPARE(l.address, QStringLiteral("U3 · 1"));
    QVERIFY(l.universe.isEmpty());
    QCOMPARE(l.network, QStringLiteral("WLED · lantern-2"));
}

void PatchLabels_Test::thePdfHasAPagePerSheet()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const PatchLabels::Sheet s = PatchLabels::sheet(QStringLiteral("avery5160"));

    QList<PatchList::Row> rs = rows(31);
    // A name far too long for a label still draws (shrunk, then elided).
    rs[0].name = QString(200, QLatin1Char('W'));
    // And every line there is, the universe's name and a network line too.
    rs[1].universeName = QString(80, QLatin1Char('S'));
    rs[1].network = true;
    rs[1].transport = QStringLiteral("WLED");
    rs[1].ip = QStringLiteral("172.18.2.50");
    rs[1].host = QString(80, QLatin1Char('h'));
    const QString path = dir.filePath("labels.pdf");
    QString error;
    QVERIFY2(PatchLabels::writePdf(path, rs, s, QStringLiteral("Labels"), &error), qPrintable(error));
    QByteArray pdf = readAll(path);
    QVERIFY(pdf.startsWith("%PDF"));
    QCOMPARE(pdfPages(pdf), 2);

    QVERIFY(PatchLabels::writePdf(path, rows(30), s, QStringLiteral("Labels")));
    QCOMPARE(pdfPages(readAll(path)), 1);
    QVERIFY(PatchLabels::writePdf(path, rows(1), s, QStringLiteral("Labels")));
    QCOMPARE(pdfPages(readAll(path)), 1);
}

void PatchLabels_Test::theRollPdfIsALabelAPage()
{
    QTemporaryDir dir;
    const PatchLabels::Sheet s = PatchLabels::sheet(QStringLiteral("roll62"));
    const QString path = dir.filePath("roll.pdf");
    QVERIFY(PatchLabels::writePdf(path, rows(5), s, QStringLiteral("Labels")));
    const QByteArray pdf = readAll(path);
    QCOMPARE(pdfPages(pdf), 5);

    // The page is the label, the long side across: 62 × 29 mm in points.
    static const QRegularExpression box(QStringLiteral("/MediaBox\\s*\\[\\s*([-\\d.]+)\\s+([-\\d.]+)\\s+([-\\d.]+)\\s+([-\\d.]+)"));
    const QRegularExpressionMatch m = box.match(QString::fromLatin1(pdf));
    QVERIFY(m.hasMatch());
    const qreal w = m.captured(3).toDouble() - m.captured(1).toDouble();
    const qreal h = m.captured(4).toDouble() - m.captured(2).toDouble();
    QVERIFY2(qAbs(w - 62 / 25.4 * 72) < 1.5 && qAbs(h - 29 / 25.4 * 72) < 1.5,
             qPrintable(QStringLiteral("%1 × %2 pt").arg(w).arg(h)));
}

void PatchLabels_Test::nothingToLabelIsAnError()
{
    QTemporaryDir dir;
    QString error;
    QVERIFY(!PatchLabels::writePdf(dir.filePath("none.pdf"), QList<PatchList::Row>(),
                                   PatchLabels::sheet(QString()), QStringLiteral("Labels"), &error));
    QVERIFY(!error.isEmpty());
}

void PatchLabels_Test::anUnwritablePathIsAnError()
{
    QString error;
    QVERIFY(!PatchLabels::writePdf(QStringLiteral("/nonexistent-dir/labels.pdf"), rows(2),
                                   PatchLabels::sheet(QString()), QStringLiteral("Labels"), &error));
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(PatchLabels_Test)

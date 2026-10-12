/*
  Q Light Controller Plus
  patchlist_test.cpp

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

#include "monitorproperties.h"
#include "inputoutputmap.h"
#include "patchlist.h"
#include "fixture.h"
#include "doc.h"

#include "patchlist_test.h"

static Fixture *dimmer(Doc *doc, const QString &name, quint32 universe, quint32 address,
                       quint32 channels = 4, bool patched = true)
{
    Fixture *f = new Fixture(doc);
    f->setName(name);
    f->setChannels(channels);
    f->setUniverse(universe);
    f->setAddress(address);
    f->setPatched(patched);
    if (!doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    return f;
}

static QStringList names(const PatchList &list)
{
    QStringList out;
    foreach (const PatchList::Row &r, list.rows())
        out << r.name;
    return out;
}

void PatchList_Test::init()
{
    m_doc = new Doc(this);
}

void PatchList_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void PatchList_Test::anEmptyShowHasNoRows()
{
    const PatchList list = PatchList::build(m_doc);
    QVERIFY(list.isEmpty());
    QCOMPARE(list.toCsv().count(QLatin1Char('\n')), 1);     // the header alone
}

void PatchList_Test::rowsAreInPatchOrderTheInventoryLast()
{
    dimmer(m_doc, QStringLiteral("U2 first"), 1, 0);
    dimmer(m_doc, QStringLiteral("Zed spare"), 0, 0, 4, false);
    dimmer(m_doc, QStringLiteral("U1 at 20"), 0, 20);
    dimmer(m_doc, QStringLiteral("Able spare"), 0, 0, 4, false);
    dimmer(m_doc, QStringLiteral("U1 at 0"), 0, 0);

    QCOMPARE(names(PatchList::build(m_doc)),
             QStringList({ QStringLiteral("U1 at 0"), QStringLiteral("U1 at 20"),
                           QStringLiteral("U2 first"),
                           QStringLiteral("Able spare"), QStringLiteral("Zed spare") }));
}

void PatchList_Test::addressesReadOneBased()
{
    m_doc->inputOutputMap()->setUniverseName(0, QStringLiteral("Stage left"));
    dimmer(m_doc, QStringLiteral("Par"), 0, 11, 12);
    dimmer(m_doc, QStringLiteral("Single"), 1, 0, 1);
    const QList<PatchList::Row> rows = PatchList::build(m_doc).rows();

    QCOMPARE(rows.at(0).firstChannel(), quint32(12));
    QCOMPARE(rows.at(0).lastChannel(), quint32(23));
    QCOMPARE(rows.at(0).patchText(), QStringLiteral("1: Stage left · 12–23"));
    // A default universe name adds nothing to its number.
    QVERIFY(rows.at(1).universeName.isEmpty());
    QCOMPARE(rows.at(1).patchText(), QStringLiteral("2 · 1"));
}

void PatchList_Test::theMountIsNamed()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    Truss *t = mp->addTruss();
    t->setName(QStringLiteral("FOH"));
    t->setLength(6.0f);
    Fixture *hung = dimmer(m_doc, QStringLiteral("Hung"), 0, 0);
    Fixture *loose = dimmer(m_doc, QStringLiteral("Loose"), 0, 10);

    FixtureRigProps rp;
    rp.trussId = t->id();
    rp.trussOffset = 2.0f;
    mp->setFixtureRigProps(hung->id(), rp);

    const QList<PatchList::Row> rows = PatchList::build(m_doc).rows();
    QCOMPARE(rows.at(0).mountedOn, mp->mountDescription(hung->id()));
    QVERIFY2(rows.at(0).mountedOn.contains(QStringLiteral("FOH")), qPrintable(rows.at(0).mountedOn));
    QVERIFY(rows.at(0).onPlot);
    QCOMPARE(rows.at(1).mountedOn, mp->mountDescription(loose->id()));
    QVERIFY(rows.at(1).onPlot == false);
}

void PatchList_Test::networkFixturesCarryTheirIp()
{
    Fixture *f = dimmer(m_doc, QStringLiteral("Strip"), 0, 0);
    f->setConnection(Fixture::Network);
    f->setIPAddress(QStringLiteral("172.18.2.50"));
    f->setTransport(Fixture::WledTransport);
    dimmer(m_doc, QStringLiteral("Wired"), 0, 10);

    const QList<PatchList::Row> rows = PatchList::build(m_doc).rows();
    QVERIFY(rows.at(0).network);
    QCOMPARE(rows.at(0).ip, QStringLiteral("172.18.2.50"));
    QCOMPARE(rows.at(0).transport, QStringLiteral("WLED"));
    QVERIFY(rows.at(1).network == false);
    QVERIFY(rows.at(1).ip.isEmpty());
}

void PatchList_Test::aClashIsListedWhereItAsked()
{
    Fixture *f = dimmer(m_doc, QStringLiteral("Clash"), 0, 40, 4, false);
    f->setAddressClash(true);
    dimmer(m_doc, QStringLiteral("Spare"), 0, 0, 4, false);
    dimmer(m_doc, QStringLiteral("Before"), 0, 0);

    const PatchList list = PatchList::build(m_doc);
    QCOMPARE(names(list), QStringList({ QStringLiteral("Before"), QStringLiteral("Clash"),
                                        QStringLiteral("Spare") }));
    QVERIFY(list.rows().at(1).patchText().startsWith(QStringLiteral("clash")));
    QVERIFY(list.toCsv().contains(QStringLiteral("address clash")));
}

void PatchList_Test::onlyTheIdsAskedFor()
{
    Fixture *a = dimmer(m_doc, QStringLiteral("A"), 0, 0);
    dimmer(m_doc, QStringLiteral("B"), 0, 10);
    QCOMPARE(names(PatchList::build(m_doc, { a->id() })), QStringList({ QStringLiteral("A") }));
}

void PatchList_Test::theCsvIsSpreadsheetSafe()
{
    dimmer(m_doc, QStringLiteral("Par, \"big\" one"), 0, 0);
    const QStringList lines = PatchList::build(m_doc).toCsv().split(QLatin1Char('\n'));
    QVERIFY(lines.at(0).startsWith(QStringLiteral("Fixture,ID,Manufacturer")));
    QVERIFY2(lines.at(1).startsWith(QStringLiteral("\"Par, \"\"big\"\" one\",")), qPrintable(lines.at(1)));
    // Universe 1, address 1, last channel 4.
    QVERIFY2(lines.at(1).contains(QStringLiteral(",4,1,,1,4,DMX,")), qPrintable(lines.at(1)));
}

void PatchList_Test::theCsvFollowsThePlotUnits()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    Fixture *f = dimmer(m_doc, QStringLiteral("Par"), 0, 0);
    // The plot keeps X/Y in millimetres and Z in metres.
    mp->setFixturePosition(f->id(), 0, 0, QVector3D(1000, 2000, 3.0f));

    QString csv = PatchList::build(m_doc).toCsv();
    QVERIFY2(csv.contains(QStringLiteral("X (m)")) && csv.contains(QStringLiteral(",1.0,2.0,3.0,")),
             qPrintable(csv));

    mp->setGridUnits(MonitorProperties::Feet);
    csv = PatchList::build(m_doc).toCsv();
    QVERIFY2(csv.contains(QStringLiteral("X (ft)")) && csv.contains(QStringLiteral(",3.3,6.6,9.8,")),
             qPrintable(csv));
}

void PatchList_Test::theHtmlHasATablePerUniverse()
{
    dimmer(m_doc, QStringLiteral("One"), 0, 0);
    dimmer(m_doc, QStringLiteral("Two"), 1, 0);
    dimmer(m_doc, QStringLiteral("Spare <b>"), 0, 0, 4, false);
    const QString html = PatchList::build(m_doc).toHtml(QStringLiteral("Patch list — Test"));

    QCOMPARE(html.count(QStringLiteral("<table")), 3);
    QVERIFY(html.contains(QStringLiteral("<h2>Universe 1</h2>")));
    QVERIFY(html.contains(QStringLiteral("<h2>Universe 2</h2>")));
    QVERIFY(html.contains(QStringLiteral("Not patched (in the inventory)")));
    // No output configured: the crew is told it goes nowhere.
    QVERIFY(html.contains(QStringLiteral("<b>nowhere</b>")));
    QVERIFY(html.contains(QStringLiteral("Spare &lt;b&gt;")));
}

void PatchList_Test::thePdfIsWritten()
{
    for (quint32 i = 0; i < 120; ++i)                    // more than a page
        dimmer(m_doc, QStringLiteral("Par %1").arg(i + 1), i / 40, (i % 40) * 4);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("patch.pdf"));
    QString error;
    QVERIFY2(PatchList::build(m_doc).writePdf(path, QStringLiteral("Patch list"), &error),
             qPrintable(error));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray pdf = f.readAll();
    QVERIFY(pdf.startsWith("%PDF"));
    QVERIFY2(pdf.count("/Type /Page\n") + pdf.count("/Type /Page\r") + pdf.count("/Type /Page ") > 1
             || pdf.contains("/Count 2") || pdf.contains("/Count 3") || pdf.contains("/Count 4"),
             "120 fixtures should run past one page");
}

void PatchList_Test::anUnwritablePdfSaysWhy()
{
    dimmer(m_doc, QStringLiteral("Par"), 0, 0);
    QString error;
    QVERIFY(PatchList::build(m_doc).writePdf(QStringLiteral("/nonexistent-dir/x.pdf"),
                                             QStringLiteral("t"), &error) == false);
    QVERIFY(error.isEmpty() == false);
}

QTEST_MAIN(PatchList_Test)

/*
  Q Light Controller Plus - qlcconsole
  eosimportwizard_test.cpp

  Licensed under the Apache License, Version 2.0.
*/

#include <QtTest>

#include "eosimportwizard.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "fixture.h"
#include "qlcfile.h"
#include "doc.h"

#include "eosimportwizard_test.h"

static const QString kSample = QStringLiteral(EOS_SAMPLES_DIR "otbpa-eos-3.3.6.asc");

void EosImportWizard_Test::initTestCase()
{
    m_doc = new Doc(this);
    QDir dir(EOS_FIXTURE_DIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir));
}

void EosImportWizard_Test::cleanupTestCase()
{
    delete m_doc;
}

void EosImportWizard_Test::needsAFile()
{
    EosImportWizard w(nullptr, m_doc);
    w.setRemember(false);
    QString err;
    QVERIFY(!w.readAscii(&err));
    QVERIFY(err.contains(".asc"));

    EosImportWizard w2(nullptr, m_doc, QStringLiteral(EOS_SAMPLES_DIR "NOTICE"));
    QVERIFY(!w2.readAscii(&err));
}

void EosImportWizard_Test::readsTheExport()
{
    EosImportWizard w(nullptr, m_doc, kSample);
    w.setRemember(false);
    QString err;
    QVERIFY2(w.readAscii(&err), qPrintable(err));
    const QString s = w.summary();
    QVERIFY2(s.contains("OTBPA DRESS TUES"), qPrintable(s));
    QVERIFY(s.contains("44 channel(s)"));
    QVERIFY(s.contains("1 cue list(s) of 66 cue(s)"));
    QVERIFY(s.contains("$Effect × 18"));
    // One row per Eos type the patch uses (one personality is unused).
    QCOMPARE(w.rowKeys().count(), 4);
    QVERIFY(w.rowKeys().contains("ETC_Fixtures|ColorSource_SPOT_Jr_DB_5ch"));
}

void EosImportWizard_Test::choicesGoToTheMapper()
{
    EosImportWizard w(nullptr, m_doc, kSample);
    w.setRemember(false);
    QVERIFY(w.readAscii());
    const int row = int(w.rowKeys().indexOf("ETC_Fixtures|ColorSource_SPOT_Jr_DB_5ch"));
    QVERIFY(row >= 0);

    EosFixtureChoice spot;
    spot.manufacturer = "ETC";
    spot.model = "ColorSource Spot";
    spot.mode = "5-Channel";
    w.setRowChoice(row, spot);
    QCOMPARE(w.rowChoice(row).model, QString("ColorSource Spot"));
    w.applyChoices();

    EosPersonality p;
    p.manufacturer = "ETC_Fixtures";
    p.model = "ColorSource_SPOT_Jr_DB_5ch";
    p.footprint = 5;
    const EosFixtureChoice c = w.mapper()->resolve(p);
    QCOMPARE(c.source, EosFixtureChoice::Saved);
    QCOMPARE(c.mode, QString("5-Channel"));
}

void EosImportWizard_Test::loadsWithTheChoices()
{
    EosImportWizard w(nullptr, m_doc, kSample);
    w.setRemember(false);
    QVERIFY(w.readAscii());
    const int row = int(w.rowKeys().indexOf("ETC_Fixtures|ColorSource_SPOT_Jr_DB_5ch"));
    EosFixtureChoice spot;
    spot.manufacturer = "ETC";
    spot.model = "ColorSource Spot";
    spot.mode = "5-Channel";
    w.setRowChoice(row, spot);
    w.applyChoices();
    QVERIFY(w.toShowTree());

    EosImportResult r;
    Doc *scratch = new Doc(this);
    QDir dir(EOS_FIXTURE_DIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    scratch->fixtureDefCache()->loadMap(dir);
    QCOMPARE(EosImporter::load(w.bundle(), r, w.mapper(), scratch), scratch);
    QCOMPARE(r.fixturesParsed, 44);
    Fixture *ch1 = scratch->fixture(1);
    QVERIFY(ch1 != nullptr);
    QCOMPARE(ch1->fixtureDef()->model(), QString("ColorSource Spot"));
    delete scratch;
}

QTEST_MAIN(EosImportWizard_Test)

/*
  Q Light Controller Plus - qlcconsole
  eosimportwizard_test.cpp

  Licensed under the Apache License, Version 2.0.
*/

#include <QtTest>

#include <QTemporaryDir>

#include "eosimportwizard.h"
#include "virtualconsole.h"
#include "qxwimporter.h"
#include "vcslider.h"
#include "vcframe.h"
#include "fixturegroup.h"
#include "scene.h"
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

void EosImportWizard_Test::specialCircuitsPage()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath("circuits.asc");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("Ident 3:0\n"
            "$Patch 1 0 1 0 1\n Text Relay FOH\n"
            "$Patch 2 0 2 0 1\n Text Relay Linear House Lights\n"
            "$Patch 3 0 3 0 1\n Text Specials\n $$Options X\n"
            "$Patch 4 0 4 0 1\n Text Cyc\n"
            "$Patch 5 0 5 0 1\n Text Blue Back Hall\n");
    f.close();

    EosImportWizard w(nullptr, m_doc, path);
    w.setRemember(false);
    QVERIFY(w.readAscii());
    QCOMPARE(w.circuitChannels(), QList<int>() << 1 << 2 << 3 << 4 << 5);
    QCOMPARE(w.circuitRole(1), int(EosCircuit::Relay));
    QCOMPARE(w.circuitRole(2), int(EosCircuit::House));
    QCOMPARE(w.circuitRole(3), int(EosCircuit::Normal));
    QCOMPARE(w.circuitRole(4), int(EosCircuit::Normal));
    QCOMPARE(w.circuitRole(5), int(EosCircuit::Backstage));

    // A selection set at once; a relay taken back to Normal.
    w.setCircuitRoles(QList<int>() << 3 << 4, EosCircuit::Work);
    w.setCircuitRoles(QList<int>() << 1, EosCircuit::Normal);
    const EosImportBundle b = w.bundle();
    QCOMPARE(b.circuitRoles.value(3), int(EosCircuit::Work));
    QCOMPARE(b.circuitRoles.value(4), int(EosCircuit::Work));
    QCOMPARE(b.circuitRoles.value(1), int(EosCircuit::Normal));

    EosImportResult r;
    Doc *scratch = new Doc(this);
    QCOMPARE(EosImporter::load(b, r, w.mapper(), scratch), scratch);
    QVERIFY(!r.circuits.contains(1));
    QCOMPARE(r.circuits.value(3).role, EosCircuit::Work);
    QVERIFY(r.circuits.value(3).gmExempt);
    QCOMPARE(r.circuitCount(EosCircuit::Work), 2);
    QStringList groups;
    for (FixtureGroup *g : scratch->fixtureGroups())
        groups << g->name();
    groups.sort();
    QCOMPARE(groups, QStringList() << "Backstage" << "House & Work");
    delete scratch;
}

void EosImportWizard_Test::subFaders()
{
    Doc *doc = new Doc(this);
    new VirtualConsole(nullptr, doc);
    QVERIFY(EosImportWizard::addSubFaders(doc, QList<EosImportedSub>()) == nullptr);

    QList<EosImportedSub> subs;
    for (const char *name : { "Sub 1 test", "Sub 40 Pod SR" })
    {
        Scene *s = new Scene(doc);
        s->setName(name);
        QVERIFY(doc->addFunction(s));
        EosImportedSub is;
        is.function = s->id();
        is.caption = s->name();
        subs << is;
    }
    EosImportedSub gone;
    gone.function = 999;
    gone.caption = "Sub 7";
    subs << gone;                               // left out of the merge: no fader

    QStringList lines;
    VCFrame *frame = EosImportWizard::addSubFaders(doc, subs, &lines);
    QVERIFY(frame != nullptr);
    QCOMPARE(frame->caption(), QString("Eos subs"));
    QCOMPARE(frame->parentWidget(), static_cast<QWidget *>(VirtualConsole::instance()->contents()));
    const QList<VCSlider *> sliders = frame->findChildren<VCSlider *>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(sliders.count(), 2);
    QList<VCSlider *> ordered = sliders;
    std::sort(ordered.begin(), ordered.end(), [](VCSlider *a, VCSlider *b) { return a->x() < b->x(); });
    QCOMPARE(ordered.at(0)->caption(), QString("Sub 1 test"));
    QCOMPARE(ordered.at(1)->caption(), QString("Sub 40 Pod SR"));
    for (int i = 0; i < 2; ++i)
    {
        QCOMPARE(ordered.at(i)->sliderMode(), VCSlider::Playback);
        QCOMPARE(ordered.at(i)->playbackFunction(), subs.at(i).function);
        QCOMPARE(ordered.at(i)->y(), ordered.at(0)->y());    // in a row
        QVERIFY(frame->rect().contains(ordered.at(i)->geometry()));
    }
    QVERIFY2(lines.join("\n").contains("2 sub fader(s)"), qPrintable(lines.join("\n")));

    // A second import puts its frame below the first.
    VCFrame *again = EosImportWizard::addSubFaders(doc, subs.mid(0, 1));
    QVERIFY(again != nullptr);
    QVERIFY(again->y() > frame->geometry().bottom());

    delete VirtualConsole::instance();
    delete doc;
}

void EosImportWizard_Test::showFromSampleDir()
{
    // Real shows that can't live in the repo (QLC_EOS_SAMPLE_DIR, as the
    // engine's eosimporter_test): the wizard's circuits, then the subs as
    // VC faders after a full merge.
    const QString dirPath = qEnvironmentVariable("QLC_EOS_SAMPLE_DIR");
    if (dirPath.isEmpty())
        QSKIP("QLC_EOS_SAMPLE_DIR not set");
    QDir dir(dirPath);
    const QStringList ascs = dir.entryList(QStringList() << "*.asc" << "*.ASC", QDir::Files);
    QVERIFY2(!ascs.isEmpty(), "no .asc in QLC_EOS_SAMPLE_DIR");
    for (const QString &asc : ascs)
    {
        EosImportWizard w(nullptr, m_doc, dir.absoluteFilePath(asc));
        w.setRemember(false);
        QVERIFY(w.readAscii());
        QMap<int, QList<int>> byRole;
        for (int ch : w.circuitChannels())
            byRole[w.circuitRole(ch)] << ch;
        for (auto it = byRole.constBegin(); it != byRole.constEnd(); ++it)
            if (it.key() != EosCircuit::Normal)
            {
                QStringList chans;
                for (int ch : it.value())
                    chans << QString::number(ch);
                qInfo().noquote() << asc << EosCircuit::roleName(EosCircuit::Role(it.key())) << ":"
                                  << chans.join(" ");
            }

        Doc *scratch = new Doc(this);
        QDir fx(EOS_FIXTURE_DIR);
        fx.setFilter(QDir::Files);
        fx.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
        scratch->fixtureDefCache()->loadMap(fx);
        EosImportResult r;
        QCOMPARE(EosImporter::load(w.bundle(), r, w.mapper(), scratch), scratch);

        Doc *target = new Doc(this);
        target->fixtureDefCache()->loadMap(fx);
        new VirtualConsole(nullptr, target);
        QList<quint32> fixtures, groups, functions, palettes;
        for (Fixture *f : scratch->fixtures())
            fixtures << f->id();
        for (FixtureGroup *g : scratch->fixtureGroups())
            groups << g->id();
        for (Function *f : scratch->functions())
            functions << f->id();
        for (QLCPalette *pl : scratch->palettes())
            palettes << pl->id();
        const QxwImportResult m = QxwImporter::import(scratch, target, fixtures, groups, functions, palettes);
        QList<EosImportedSub> subs;
        for (EosImportedSub s : r.subs)
        {
            s.function = m.functionMap.value(s.function);
            subs << s;
        }
        QStringList lines;
        VCFrame *frame = EosImportWizard::addSubFaders(target, subs, &lines);
        const int faders = frame == nullptr ? 0
                           : int(frame->findChildren<VCSlider *>(QString(), Qt::FindDirectChildrenOnly).count());
        QCOMPARE(faders, int(r.subs.count()));
        qInfo().noquote() << asc << QString("faders %1:").arg(faders) << lines.join(" ");
        QStringList roleGroups;
        for (FixtureGroup *g : target->fixtureGroups())
            if (g->name().startsWith("Relays") || g->name() == "House & Work" || g->name() == "Backstage")
                roleGroups << QString("%1/%2 (%3)").arg(g->path(), g->name()).arg(g->fixtureList().count());
        qInfo().noquote() << asc << "role groups:" << roleGroups.join(", ");
        delete VirtualConsole::instance();
        delete target;
        delete scratch;
    }
}

QTEST_MAIN(EosImportWizard_Test)

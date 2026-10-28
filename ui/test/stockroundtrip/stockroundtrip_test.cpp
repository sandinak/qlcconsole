/*
  Q Light Controller Plus - Unit test
  stockroundtrip_test.cpp

  0.3.1 (k) 7: a QLC+ 4 user's trust rests on this -- a .qxw saved by
  qlcconsole opens in stock QLC+ 4 with its core data whole. The show is
  saved by the app's own App::saveXML, then read by stock QLC+ 4.14.4's
  engine, compiled from the pinned upstream source in stock/ into
  stockroundtrip_load, and the two fingerprints must match.

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
#include <QProcess>
#include <QtTest>
#include <QColor>
#include <QFile>

#include "stockroundtrip_test.h"
#include "fingerprint.h"
#include "../../../engine/test/common/resource_paths.h"

#include "qlcfixturedefcache.h"
#include "showtreecontroller.h"
#include "speedmasters.h"
#include "stockqlccheck.h"
#include "showfunction.h"
#include "rgbplain.h"
#include "efxfixture.h"
#define private public
#include "app.h"
#undef private

namespace
{

QStringList readLines(const QString &path)
{
    QStringList lines;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    return lines;
}

/* Stock QLC+ 4.14.4 reads @p qxw; its fingerprint, or a failure message. */
bool stockReads(const QString &qxw, const QString &out, QString *why)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(QStringLiteral(STOCK_LOADER),
            { qxw, QStringLiteral(QLC_TEST_RESOURCE_ROOT), out });
    if (!p.waitForFinished(60000))
    {
        *why = QStringLiteral("the stock loader did not finish: %1").arg(p.errorString());
        p.kill();
        return false;
    }
    const QString said = QString::fromUtf8(p.readAll());
    if (!said.trimmed().isEmpty())
        qDebug().noquote() << said.trimmed();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
    {
        *why = QStringLiteral("stock QLC+ 4.14.4 could not load it (exit %1): %2").arg(p.exitCode()).arg(said);
        return false;
    }
    return true;
}

QString diff(const QStringList &want, const QStringList &got)
{
    QStringList out;
    for (const QString &l : want)
        if (!got.contains(l))
            out << QStringLiteral("  lost:   ") + l;
    for (const QString &l : got)
        if (!want.contains(l))
            out << QStringLiteral("  gained: ") + l;
    return out.join(QLatin1Char('\n'));
}

}

void StockRoundTrip_Test::initTestCase()
{
    QVERIFY2(QFile::exists(QStringLiteral(STOCK_LOADER)), STOCK_LOADER);
    m_app = new App();
    m_app->init();
    m_app->doc()->fixtureDefCache()->loadMap(QDir(INTERNAL_FIXTUREDIR));
}

void StockRoundTrip_Test::cleanupTestCase()
{
    delete m_app;
    m_app = nullptr;
}

void StockRoundTrip_Test::aQxwSavedHereOpensInStockQlcPlus4()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    QLCFixtureDefCache *defs = doc->fixtureDefCache();

    /* The rig: dimmers with no definition, and definitions stock QLC+ 4.14.4
       ships the same -- RGB pars, a multi-head bar and a mover. */
    Fixture *dimmers = new Fixture(doc);
    dimmers->setName("Front Dimmers");
    dimmers->setChannels(6);
    dimmers->setUniverse(0);
    dimmers->setAddress(0);
    QVERIFY(doc->addFixture(dimmers));
    QList<Fixture *> pars;
    QLCFixtureDef *rgbDef = defs->fixtureDef("Generic", "Generic RGB");
    QVERIFY(rgbDef != nullptr);
    for (int i = 0; i < 4; i++)
    {
        Fixture *par = new Fixture(doc);
        par->setName(QString("Par %1").arg(i + 1));
        par->setFixtureDefinition(rgbDef, rgbDef->mode("RGB"));
        par->setUniverse(0);
        par->setAddress(10 + i * 3);
        QVERIFY(doc->addFixture(par));
        pars << par;
    }
    QLCFixtureDef *barDef = defs->fixtureDef("Eurolite", "LED CBB-3 COB RBG 3x15W Bar");
    QVERIFY(barDef != nullptr);
    Fixture *bar = new Fixture(doc);
    bar->setName("Cyc Bar");
    bar->setFixtureDefinition(barDef, barDef->mode("9-Channel"));
    bar->setUniverse(1);
    bar->setAddress(0);
    QVERIFY(doc->addFixture(bar));
    QVERIFY(bar->heads() > 1);
    QLCFixtureDef *moverDef = defs->fixtureDef("IMG Stageline", "BEAM-40 WS/RGBW");
    QVERIFY(moverDef != nullptr);
    Fixture *mover = new Fixture(doc);
    mover->setName("Mover SL");
    mover->setFixtureDefinition(moverDef, moverDef->mode("WS - 9 Channel"));
    mover->setUniverse(1);
    mover->setAddress(100);
    QVERIFY(doc->addFixture(mover));

    FixtureGroup *grid = new FixtureGroup(doc);
    grid->setName("Pars");
    grid->setSize(QSize(2, 2));
    QVERIFY(doc->addFixtureGroup(grid));
    for (Fixture *par : pars)
        grid->assignFixture(par->id());
    ChannelsGroup *cg = new ChannelsGroup(doc);
    cg->setName("Front Intensity");
    cg->addChannel(dimmers->id(), 0);
    cg->addChannel(dimmers->id(), 1);
    QVERIFY(doc->addChannelsGroup(cg));

    QLCPalette *half = new QLCPalette(QLCPalette::Dimmer);
    half->setName("Half");
    half->setValue(128);
    QVERIFY(doc->addPalette(half));
    QLCPalette *amber = new QLCPalette(QLCPalette::Color);
    amber->setName("Amber");
    amber->setValue(QColor(255, 160, 0));
    QVERIFY(doc->addPalette(amber));

    /* The functions QLC+ 4 has. */
    Scene *warm = new Scene(doc);
    warm->setName("01-01.01-Warm");
    warm->setPath("Act 1");
    warm->setValue(dimmers->id(), 0, 200);
    warm->setValue(pars.at(0)->id(), 0, 255);
    warm->setValue(pars.at(0)->id(), 1, 120);
    QVERIFY(doc->addFunction(warm));
    Scene *cool = new Scene(doc);
    cool->setName("01-01.02-Cool");
    cool->setValue(pars.at(1)->id(), 2, 255);
    QVERIFY(doc->addFunction(cool));
    Chaser *chase = new Chaser(doc);
    chase->setName("Chase");
    QVERIFY(chase->addStep(ChaserStep(warm->id(), 500, 1000, 250)));
    QVERIFY(chase->addStep(ChaserStep(cool->id(), 0, 2000, 0)));
    QVERIFY(doc->addFunction(chase));
    Collection *both = new Collection(doc);
    both->setName("Both");
    both->addFunction(warm->id());
    both->addFunction(cool->id());
    QVERIFY(doc->addFunction(both));
    EFX *circle = new EFX(doc);
    circle->setName("Circle");
    circle->setAlgorithm(EFX::Circle);
    circle->setWidth(90);
    QVERIFY(circle->addFixture(mover->id(), 0));
    QVERIFY(doc->addFunction(circle));
    RGBMatrix *matrix = new RGBMatrix(doc);
    matrix->setName("Wash Matrix");
    matrix->setFixtureGroup(grid->id());
    matrix->setAlgorithm(new RGBPlain(doc));
    QVERIFY(doc->addFunction(matrix));
    Sequence *seq = new Sequence(doc);
    seq->setName("Sequence");
    seq->setBoundSceneID(warm->id());
    seq->addStep(ChaserStep(warm->id(), 0, 1000, 0));
    QVERIFY(doc->addFunction(seq));
    Script *script = new Script(doc);
    script->setName("House");
    QVERIFY(script->setData("startfunction:" + QString::number(warm->id()) + "\nwait:1s\n"));
    QVERIFY(doc->addFunction(script));
    Show *show = new Show(doc);
    show->setName("Timeline");
    QVERIFY(doc->addFunction(show));
    Track *track = new Track(warm->id(), show);
    track->setName("Looks");
    QVERIFY(show->addTrack(track));
    track->createShowFunction(warm->id());

    /* And what only qlcconsole has: a Show Tree with a cue and a hit, the
       speed masters. QLC+ 4 should read past these, not choke on them. */
    ShowTreeController *st = doc->showTree();
    const quint32 prod = st->addProduction("Spring");
    st->addCue(st->addCueList(prod, "Act 1"), -1, "One");
    st->addLook(prod, "Blast");
    doc->speedMasters()->setName(0, "Song");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString qxw = dir.filePath("roundtrip.qxw");
    QCOMPARE(m_app->saveXML(qxw), QFile::NoError);
    const QStringList want = StockRoundTrip::fingerprint(doc);
    QVERIFY(want.count() > 20);

    QString why;
    QVERIFY2(stockReads(qxw, dir.filePath("stock.txt"), &why), qPrintable(why));
    const QStringList got = readLines(dir.filePath("stock.txt"));
    QVERIFY2(got == want, qPrintable(QStringLiteral("stock QLC+ 4.14.4 read it differently:\n")
                                     + diff(want, got)));
    m_app->clearDocument();
}

void StockRoundTrip_Test::theNoticeNamesWhatStockLeavesOut()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    Fixture *fx = new Fixture(doc);
    fx->setChannels(2);
    QVERIFY(doc->addFixture(fx));
    ShowTreeController *st = doc->showTree();
    st->addCue(st->addCueList(st->addProduction("P"), "L"), -1, "One");

    QTemporaryDir dir;
    const QString qxw = dir.filePath("tree.qxw");
    QCOMPARE(m_app->saveXML(qxw), QFile::NoError);
    const QStringList losses = StockQlcCheck::lossesInFile(qxw);
    QVERIFY2(losses.count() == 1 && losses.first().contains("Show Tree"), qPrintable(losses.join(" | ")));
    // Stock reads the rest whole: the fixture and the tree's own functions.
    QString why;
    QVERIFY2(stockReads(qxw, dir.filePath("stock.txt"), &why), qPrintable(why));
    QCOMPARE(readLines(dir.filePath("stock.txt")), StockRoundTrip::fingerprint(doc));
    m_app->clearDocument();
}

QTEST_MAIN(StockRoundTrip_Test)

/*
  Q Light Controller Plus
  mountview_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QDir>
#include <QLabel>
#include <QtTest>
#include "../../../engine/test/common/stdprelude.h"
#include "mountinsituview.h"
#include "structurestudioview.h"
#include "measurementunits.h"
#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "truss.h"
#include "tower.h"
#include "pipe.h"
#include "fixture.h"
#include "doc.h"

#include "mountview_test.h"

namespace
{
/* A par: one dimmer channel, 200 x 250 x 200 mm. */
Fixture *addPar(Doc *doc, const QString &name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel(QStringLiteral("Par %1").arg(name));
    def->setType(QLCFixtureDef::Dimmer);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("1ch");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dim");
    ch->setGroup(QLCChannel::Intensity);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    QLCPhysical ph;
    ph.setWidth(200); ph.setHeight(250); ph.setDepth(200);
    mode->setPhysical(ph);
    def->addMode(mode);
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(0);
    fx->setAddress(address);
    if (!doc->addFixture(fx))
    {
        delete fx;
        return nullptr;
    }
    doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(0, 0, 0));
    return fx;
}

struct Rig
{
    QScopedPointer<Doc> docOwner;
    Doc *doc;
    MonitorProperties *props;
    Truss *truss = nullptr;
    Pipe *pipe = nullptr;
    Tower *tower = nullptr;
    quint32 onTruss = 0, onPipe = 0, onTower = 0, free = 0;

    explicit Rig(QObject *parent)
        : docOwner(new Doc(parent))
        , doc(docOwner.data())
        , props(doc->monitorProperties())
    {
        truss = props->addTruss();
        truss->setName("TRUSS-1");
        truss->setType(Truss::Horizontal);
        truss->setOrigin(QVector3D(2.0f, 3.0f, 4.0f));
        truss->setDirection(QPointF(1.0, 0.0));
        truss->setLength(4.0f);
        truss->setWidth(0.3f);

        pipe = props->addPipe();
        pipe->setName("PIPE-1");
        pipe->setLength(3.0f);

        tower = props->addTower();
        tower->setName("TOWER-1");

        Fixture *a = addPar(doc, "On truss", 0);
        FixtureRigProps ra;
        ra.trussId = truss->id();
        ra.trussOffset = 1.5f;
        ra.mountZOffset = 0.0f;
        props->setFixtureRigProps(a->id(), ra);
        onTruss = a->id();

        Fixture *b = addPar(doc, "On pipe", 10);
        FixtureRigProps rb;
        rb.pipeId = pipe->id();
        props->setFixtureRigProps(b->id(), rb);
        onPipe = b->id();

        Fixture *c = addPar(doc, "On tower", 20);
        FixtureRigProps rc;
        rc.towerId = tower->id();
        rc.towerMountSide = FixtureRigProps::TowerTop;
        props->setFixtureRigProps(c->id(), rc);
        onTower = c->id();

        Fixture *d = addPar(doc, "Free", 30);
        free = d->id();
    }

    void setOffset(quint32 fid, float metres)
    {
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.mountZOffset = metres;
        props->setFixtureRigProps(fid, rp);
    }
};
}

void MountView_Test::cleanup()
{
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
}

void MountView_Test::aFixtureOnATrussShowsTheTruss()
{
    Rig rig(this);
    MountInSituView v(rig.doc, rig.onTruss);
    QVERIFY(v.isMounted());
    QVERIFY(v.view() != nullptr);
    QCOMPARE(v.view()->plane(), StructureStudioView::Front);
    QVERIFY(v.view()->locked());
    QVERIFY(!v.view()->beams());
    QCOMPARE(v.view()->highlighted(), QList<quint32>({ rig.onTruss }));
    QVERIFY2(v.caption().contains(QStringLiteral("truss TRUSS-1")), qPrintable(v.caption()));
    QVERIFY2(v.caption().contains(QStringLiteral("hung below")), qPrintable(v.caption()));
    QCOMPARE(v.sizeHint(), QSize(260, 160));
}

void MountView_Test::aFixtureOnAPipeShowsThePipe()
{
    Rig rig(this);
    MountInSituView v(rig.doc, rig.onPipe);
    QVERIFY(v.isMounted());
    QVERIFY2(v.caption().contains(QStringLiteral("pipe PIPE-1")), qPrintable(v.caption()));
    QCOMPARE(v.view()->highlighted(), QList<quint32>({ rig.onPipe }));
}

void MountView_Test::aFixtureOnATowerShowsTheTower()
{
    Rig rig(this);
    MountInSituView v(rig.doc, rig.onTower);
    QVERIFY(v.isMounted());
    QVERIFY2(v.caption().contains(QStringLiteral("tower TOWER-1")), qPrintable(v.caption()));
    QVERIFY2(v.caption().contains(QStringLiteral("tower top")), qPrintable(v.caption()));
    QCOMPARE(v.view()->highlighted(), QList<quint32>({ rig.onTower }));
}

void MountView_Test::anUnmountedFixtureSaysSo()
{
    Rig rig(this);
    MountInSituView v(rig.doc, rig.free);
    QVERIFY(!v.isMounted());
    QVERIFY(v.view() == nullptr);
    QVERIFY2(v.caption().contains(QStringLiteral("Not on a structure")), qPrintable(v.caption()));
    QVERIFY(v.caption().contains(QStringLiteral("free on the plot")));
    QVERIFY(v.heightText().isEmpty());
}

void MountView_Test::refreshFollowsTheHeightOffsetInFeetAndMetres()
{
    Rig rig(this);
    MountInSituView v(rig.doc, rig.onTruss);
    QVERIFY2(v.heightText().contains(QStringLiteral("flush")), qPrintable(v.heightText()));

    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
    rig.setOffset(rig.onTruss, 0.6858f);                 // 2' 3"
    const QString before = v.heightText();
    v.refresh();
    QVERIFY(v.heightText() != before);
    QCOMPARE(v.heightText(), QStringLiteral("%1 above the truss")
             .arg(MeasurementUnits::formatLength(685.8, MeasurementUnits::Feet)));
    QVERIFY2(v.heightText().contains(QLatin1Char('\'')), qPrintable(v.heightText()));

    MeasurementUnits::setPreferredLength(MeasurementUnits::Metres);
    v.refresh();
    QVERIFY2(!v.heightText().contains(QLatin1Char('\'')), qPrintable(v.heightText()));
    QVERIFY2(v.heightText().contains(QStringLiteral("0.6858")), qPrintable(v.heightText()));

    rig.setOffset(rig.onTruss, -0.25f);
    v.refresh();
    QVERIFY2(v.heightText().contains(QStringLiteral("below the truss")), qPrintable(v.heightText()));
}

void MountView_Test::refreshMovesBetweenMountedAndFree()
{
    Rig rig(this);
    MountInSituView v(rig.doc, rig.onTruss);
    QVERIFY(v.isMounted());
    FixtureRigProps rp = rig.props->fixtureRigProps(rig.onTruss);
    rp.trussId = Truss::invalidId();
    rig.props->setFixtureRigProps(rig.onTruss, rp);
    v.refresh();
    QVERIFY(!v.isMounted());
    QVERIFY(v.caption().contains(QStringLiteral("Not on a structure")));
    rp.pipeId = rig.pipe->id();
    rig.props->setFixtureRigProps(rig.onTruss, rp);
    v.refresh();
    QVERIFY(v.isMounted());
    QVERIFY(v.caption().contains(QStringLiteral("pipe PIPE-1")));
}

void MountView_Test::opensQuickly()
{
    Rig rig(this);
    const quint32 fids[] = { rig.onTruss, rig.onPipe, rig.onTower, rig.onTruss, rig.onPipe, rig.onTower };
    qint64 cold = 0, warmWorst = 0;
    for (int i = 0; i < 6; ++i)
    {
        QElapsedTimer t;
        t.start();
        MountInSituView v(rig.doc, fids[i]);
        v.resize(260, 160);
        v.show();
        QImage img(v.size(), QImage::Format_ARGB32);
        v.render(&img);
        const qint64 ms = t.elapsed();
        qInfo("MountInSituView open #%d: %lld ms", i, ms);
        if (i == 0)
            cold = ms;          // the first ever: fonts, caches, raster set-up
        else
            warmWorst = qMax(warmWorst, ms);
    }
    qInfo("MountInSituView open + first paint (offscreen): first %lld ms, warm worst %lld ms",
          cold, warmWorst);
    QVERIFY2(warmWorst < 150, qPrintable(QStringLiteral("a warm open took %1 ms").arg(warmWorst)));
    QVERIFY2(cold < 600, qPrintable(QStringLiteral("the first open took %1 ms").arg(cold)));
}

void MountView_Test::picturesForReview()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    Rig rig(this);
    rig.setOffset(rig.onTruss, 0.6858f);
    struct Shot { const char *name; quint32 fid; } shots[] = {
        { "truss", rig.onTruss }, { "pipe", rig.onPipe },
        { "tower", rig.onTower }, { "free", rig.free } };
    for (const Shot &s : shots)
    {
        MountInSituView v(rig.doc, s.fid);
        v.resize(260, 160);
        v.show();
        QTest::qWait(30);
        QVERIFY(v.grab().save(QDir(dir).filePath(QStringLiteral("mountview-%1.png").arg(s.name))));
    }
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    MountView_Test test;
    return QTest::qExec(&test, argc, argv);
}

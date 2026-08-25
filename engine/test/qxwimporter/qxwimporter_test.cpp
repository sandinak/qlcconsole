/*
  Q Light Controller Plus - qlcconsole
  qxwimporter_test.cpp

  Licensed under the Apache License, Version 2.0.
*/

#include <QtTest>

#define protected public
#define private public
#include "doc.h"
#include "scene.h"
#include "qlcpalette.h"
#include "qxwimporter.h"
#include "monitorproperties.h"
#include "powerdistribution.h"
#include "fixture.h"
#undef protected
#undef private

#include "qxwimporter_test.h"

void QxwImporter_Test::initTestCase() { }
void QxwImporter_Test::cleanupTestCase() { }

void QxwImporter_Test::init()
{
    m_source = new Doc(this);
    m_target = new Doc(this);
}

void QxwImporter_Test::cleanup()
{
    delete m_source; m_source = NULL;
    delete m_target; m_target = NULL;
}

/** Palette + Scene that uses it, in the source doc. Returns the scene id. */
static quint32 makeSceneWithPalette(Doc *doc, quint32 paletteId, quint32 sceneId,
                                    const QString &pname)
{
    QLCPalette *pal = new QLCPalette(QLCPalette::Color);
    pal->setName(pname);
    doc->addPalette(pal, paletteId);

    Scene *sc = new Scene(doc);
    sc->setName("Look");
    doc->addFunction(sc, sceneId);
    sc->addPalette(paletteId);
    return sc->id();
}

void QxwImporter_Test::paletteFollowsScene()
{
    const quint32 sceneId = makeSceneWithPalette(m_source, 7, 3, "Amber");
    QCOMPARE(m_target->palettes().size(), 0);

    QxwImportResult r = QxwImporter::import(m_source, m_target,
                                            QList<quint32>(), QList<quint32>(),
                                            QList<quint32>() << sceneId);

    QCOMPARE(r.functionsImported, 1);
    QCOMPARE(r.palettesImported, 1);
    // Nothing collided, so the ids carry over untouched.
    QVERIFY(m_target->palette(7) != NULL);
    QCOMPARE(m_target->palette(7)->name(), QString("Amber"));

    Scene *imported = qobject_cast<Scene *>(m_target->function(3));
    QVERIFY(imported != NULL);
    QCOMPARE(imported->palettes(), QList<quint32>() << 7);
}

void QxwImporter_Test::collidingPaletteIsRemapped()
{
    const quint32 sceneId = makeSceneWithPalette(m_source, 7, 3, "Amber");

    // Target already owns palette id 7 -- a DIFFERENT palette.
    QLCPalette *occupant = new QLCPalette(QLCPalette::Color);
    occupant->setName("Existing");
    QVERIFY(m_target->addPalette(occupant, 7));

    QxwImportResult r = QxwImporter::import(m_source, m_target,
                                            QList<quint32>(), QList<quint32>(),
                                            QList<quint32>() << sceneId);
    QCOMPARE(r.palettesImported, 1);
    QVERIFY(r.idsRemapped >= 1);

    // The occupant must be untouched...
    QCOMPARE(m_target->palette(7)->name(), QString("Existing"));

    // ...the import landed under a different id...
    Scene *imported = qobject_cast<Scene *>(m_target->function(3));
    QVERIFY(imported != NULL);
    QCOMPARE(imported->palettes().size(), 1);
    const quint32 newPid = imported->palettes().first();
    QVERIFY(newPid != 7);

    // ...and the Scene points at the copy, which is the one it came with.
    QVERIFY(m_target->palette(newPid) != NULL);
    QCOMPARE(m_target->palette(newPid)->name(), QString("Amber"));
}

void QxwImporter_Test::paletteFadeOverrideFollowsRemap()
{
    const quint32 sceneId = makeSceneWithPalette(m_source, 7, 3, "Amber");
    Scene *src = qobject_cast<Scene *>(m_source->function(sceneId));
    src->setPaletteFade(7, 250, 1500);

    QLCPalette *occupant = new QLCPalette(QLCPalette::Color);
    QVERIFY(m_target->addPalette(occupant, 7));

    QxwImporter::import(m_source, m_target, QList<quint32>(), QList<quint32>(),
                        QList<quint32>() << sceneId);

    Scene *imported = qobject_cast<Scene *>(m_target->function(3));
    QVERIFY(imported != NULL);
    const quint32 newPid = imported->palettes().first();
    QVERIFY(newPid != 7);
    // The override must have moved to the NEW id, not been left on the old one.
    QCOMPARE(imported->paletteFadeIn(newPid), 250);
    QCOMPARE(imported->paletteFadeOut(newPid), 1500);
}

void QxwImporter_Test::paletteOrderPreserved()
{
    QLCPalette *a = new QLCPalette(QLCPalette::Color); a->setName("A");
    QLCPalette *b = new QLCPalette(QLCPalette::Color); b->setName("B");
    QLCPalette *c = new QLCPalette(QLCPalette::Color); c->setName("C");
    QVERIFY(m_source->addPalette(a, 10));
    QVERIFY(m_source->addPalette(b, 11));
    QVERIFY(m_source->addPalette(c, 12));

    Scene *sc = new Scene(m_source);
    sc->setName("Look");
    QVERIFY(m_source->addFunction(sc, 3));
    sc->addPalette(11);
    sc->addPalette(10);
    sc->addPalette(12);

    QxwImporter::import(m_source, m_target, QList<quint32>(), QList<quint32>(),
                        QList<quint32>() << 3);

    Scene *imported = qobject_cast<Scene *>(m_target->function(3));
    QVERIFY(imported != NULL);
    QCOMPARE(imported->palettes().size(), 3);
    // Order is precedence: later entries win on shared channels.
    QStringList names;
    foreach (quint32 pid, imported->palettes())
        names << m_target->palette(pid)->name();
    QCOMPARE(names, QStringList() << "B" << "A" << "C");
}


/** A plain 2-channel dimmer -- no fixture definition needed. */
static quint32 makeFixture(Doc *doc, int address)
{
    Fixture *fxi = new Fixture(doc);
    fxi->setChannels(2);
    fxi->setAddress(address);
    doc->addFixture(fxi);
    return fxi->id();
}

void QxwImporter_Test::mapPlacementFollowsFixture()
{
    const quint32 fid = makeFixture(m_source, 0);
    // Value-initialise: PreviewItem only default-initialises m_layerId and
    // m_groupId, so a plain declaration leaves position/rotation/scale/zoom/
    // flags indeterminate and GCC rightly refuses it under -Werror.
    PreviewItem item = PreviewItem();
    item.m_position = QVector3D(1200, 0, 3400);
    item.m_rotation = QVector3D(0, 90, 0);
    item.m_color = QColor(Qt::magenta);
    m_source->monitorProperties()->setFixtureItem(fid, 0, 0, item);

    QxwImportResult r = QxwImporter::import(m_source, m_target,
                                            QList<quint32>() << fid,
                                            QList<quint32>(), QList<quint32>());
    QCOMPARE(r.fixturesImported, 1);
    QCOMPARE(r.fixturesPlaced, 1);

    QVERIFY(m_target->monitorProperties()->containsFixture(fid));
    PreviewItem got = m_target->monitorProperties()->fixtureItem(fid, 0, 0);
    QCOMPARE(got.m_position, QVector3D(1200, 0, 3400));
    QCOMPARE(got.m_rotation, QVector3D(0, 90, 0));
    QCOMPARE(got.m_color, QColor(Qt::magenta));
}

void QxwImporter_Test::mapLayerAndGroupAreReset()
{
    const quint32 fid = makeFixture(m_source, 0);
    // Value-initialise: PreviewItem only default-initialises m_layerId and
    // m_groupId, so a plain declaration leaves position/rotation/scale/zoom/
    // flags indeterminate and GCC rightly refuses it under -Werror.
    PreviewItem item = PreviewItem();
    item.m_position = QVector3D(500, 0, 500);
    item.m_layerId = 9;
    item.m_groupId = 42;
    m_source->monitorProperties()->setFixtureItem(fid, 0, 0, item);

    QxwImporter::import(m_source, m_target, QList<quint32>() << fid,
                        QList<quint32>(), QList<quint32>());

    PreviewItem got = m_target->monitorProperties()->fixtureItem(fid, 0, 0);
    // Position survives; the references into id spaces we did not import
    // do not, or ensureGroup() would invent a phantom group for id 42.
    QCOMPARE(got.m_position, QVector3D(500, 0, 500));
    QCOMPARE(got.m_layerId, quint32(0));
    QCOMPARE(got.m_groupId, quint32(0));
}

void QxwImporter_Test::powerPatchFollowsFixture()
{
    const quint32 fid = makeFixture(m_source, 0);
    PowerSource src;
    src.name = "Distro A";
    PowerCircuit cir;
    cir.name = "C1";
    cir.ratedAmps = 20;
    src.circuits.append(cir);
    m_source->powerDistribution()->sources().append(src);
    m_source->powerDistribution()->assignFixture(fid, 0, 0);

    QxwImportResult r = QxwImporter::import(m_source, m_target,
                                            QList<quint32>() << fid,
                                            QList<quint32>(), QList<quint32>());
    QCOMPARE(r.fixturesPowerPatched, 1);
    QCOMPARE(r.powerSourcesCreated, 1);

    const QList<PowerSource> &dst = m_target->powerDistribution()->sources();
    QCOMPARE(dst.size(), 1);
    QCOMPARE(dst.at(0).name, QString("Distro A"));
    QCOMPARE(dst.at(0).circuits.size(), 1);
    QCOMPARE(dst.at(0).circuits.at(0).name, QString("C1"));
    QVERIFY(dst.at(0).circuits.at(0).fixtures.contains(fid));
}

void QxwImporter_Test::powerSourceMatchedByName()
{
    const quint32 fid = makeFixture(m_source, 0);
    PowerSource src;
    src.name = "Distro A";
    PowerCircuit cir; cir.name = "C1";
    src.circuits.append(cir);
    m_source->powerDistribution()->sources().append(src);
    m_source->powerDistribution()->assignFixture(fid, 0, 0);

    // Target already owns a distro of the same name.
    PowerSource existing;
    existing.name = "Distro A";
    PowerCircuit ec; ec.name = "C1";
    existing.circuits.append(ec);
    m_target->powerDistribution()->sources().append(existing);

    QxwImportResult r = QxwImporter::import(m_source, m_target,
                                            QList<quint32>() << fid,
                                            QList<quint32>(), QList<quint32>());
    QCOMPARE(r.fixturesPowerPatched, 1);
    // Reused, not duplicated.
    QCOMPARE(r.powerSourcesCreated, 0);
    const QList<PowerSource> &dst = m_target->powerDistribution()->sources();
    QCOMPARE(dst.size(), 1);
    QVERIFY(dst.at(0).circuits.at(0).fixtures.contains(fid));
}

QTEST_APPLESS_MAIN(QxwImporter_Test)

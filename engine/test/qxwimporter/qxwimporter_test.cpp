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
#include "collection.h"
#include "chaser.h"
#include "chaserstep.h"
#include "efx.h"
#include "efxfixture.h"
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
    // Position survives. The layer/group ids here are DANGLING -- nothing in
    // the source registry defines 9 or 42 -- so they must resolve to
    // default/ungrouped rather than being invented in the target.
    // MonitorProperties::layer() resolves unknown ids TO the Default layer, so
    // a mapper that trusted it would create a second "Default" here.
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

void QxwImporter_Test::mapLayerAndGroupAreTranslated()
{
    const quint32 fid = makeFixture(m_source, 0);
    MonitorProperties *sp = m_source->monitorProperties();
    const quint32 srcLayer = sp->addLayer("Overhead");
    const quint32 srcGroup = sp->nextGroupId();
    sp->createGroup(srcGroup, "Movers", srcLayer, 0);

    PreviewItem item = PreviewItem();
    item.m_position = QVector3D(100, 0, 200);
    item.m_layerId = srcLayer;
    item.m_groupId = srcGroup;
    sp->setFixtureItem(fid, 0, 0, item);

    QxwImportResult r = QxwImporter::import(m_source, m_target,
                                            QList<quint32>() << fid,
                                            QList<quint32>(), QList<quint32>());
    QCOMPARE(r.layersCreated, 1);
    QCOMPARE(r.mapGroupsCreated, 1);

    MonitorProperties *tp = m_target->monitorProperties();
    PreviewItem got = tp->fixtureItem(fid, 0, 0);
    QVERIFY(got.m_layerId != 0);
    QVERIFY(got.m_groupId != 0);
    QCOMPARE(tp->layer(got.m_layerId).name, QString("Overhead"));
    QCOMPARE(tp->group(got.m_groupId).name, QString("Movers"));
    // The group must sit under the translated layer, not the source's id.
    QCOMPARE(tp->group(got.m_groupId).layerId, got.m_layerId);
}

void QxwImporter_Test::mapLayerAndGroupMatchedByName()
{
    const quint32 fid = makeFixture(m_source, 0);
    MonitorProperties *sp = m_source->monitorProperties();
    const quint32 srcLayer = sp->addLayer("Overhead");
    const quint32 srcGroup = sp->nextGroupId();
    sp->createGroup(srcGroup, "Movers", srcLayer, 0);
    PreviewItem item = PreviewItem();
    item.m_layerId = srcLayer;
    item.m_groupId = srcGroup;
    sp->setFixtureItem(fid, 0, 0, item);

    // Target already has both, by name.
    MonitorProperties *tp = m_target->monitorProperties();
    const quint32 tgtLayer = tp->addLayer("Overhead");
    const quint32 tgtGroup = tp->nextGroupId();
    tp->createGroup(tgtGroup, "Movers", tgtLayer, 0);

    QxwImportResult r = QxwImporter::import(m_source, m_target,
                                            QList<quint32>() << fid,
                                            QList<quint32>(), QList<quint32>());
    // Reused, not duplicated.
    QCOMPARE(r.layersCreated, 0);
    QCOMPARE(r.mapGroupsCreated, 0);
    PreviewItem got = tp->fixtureItem(fid, 0, 0);
    QCOMPARE(got.m_layerId, tgtLayer);
    QCOMPARE(got.m_groupId, tgtGroup);
}

void QxwImporter_Test::mapGroupParentChainFollows()
{
    const quint32 fid = makeFixture(m_source, 0);
    MonitorProperties *sp = m_source->monitorProperties();
    const quint32 parent = sp->nextGroupId();
    sp->createGroup(parent, "Stage Left", 0, 0);
    const quint32 child = sp->nextGroupId();
    sp->createGroup(child, "Movers", 0, parent);

    PreviewItem item = PreviewItem();
    item.m_groupId = child;
    sp->setFixtureItem(fid, 0, 0, item);

    QxwImporter::import(m_source, m_target, QList<quint32>() << fid,
                        QList<quint32>(), QList<quint32>());

    MonitorProperties *tp = m_target->monitorProperties();
    const quint32 gotChild = tp->fixtureItem(fid, 0, 0).m_groupId;
    QVERIFY(gotChild != 0);
    QCOMPARE(tp->group(gotChild).name, QString("Movers"));
    // The ancestor came too, and the child hangs off it.
    const quint32 gotParent = tp->group(gotChild).parentGroupId;
    QVERIFY(gotParent != 0);
    QCOMPARE(tp->group(gotParent).name, QString("Stage Left"));
}

/* Merging a show that has its own "AT1-3z" does not make two of them. */
void QxwImporter_Test::anImportedFixtureNameAlreadyInTheShowIsNumbered()
{
    Fixture *mine = new Fixture(m_target);
    mine->setName("AT1-3z");
    mine->setChannels(1);
    QVERIFY(m_target->addFixture(mine));

    Fixture *theirs = new Fixture(m_source);
    theirs->setName("AT1-3z");
    theirs->setChannels(1);
    theirs->setAddress(100);
    QVERIFY(m_source->addFixture(theirs));

    QxwImportResult r = QxwImporter::import(m_source, m_target, { theirs->id() }, {}, {});
    QCOMPARE(r.fixturesImported, 1);
    QVERIFY(m_target->duplicateFixtureNames().isEmpty());
    QCOMPARE(mine->name(), QString("AT1-3z"));
    QStringList names;
    foreach (Fixture *f, m_target->fixtures()) names << f->name();
    QVERIFY(names.contains("AT1-3z #2"));
    QVERIFY(r.warnings.join("\n").contains("imported as \"AT1-3z #2\""));
}

void QxwImporter_Test::collectionMembersFollowChainedRemaps()
{
    // Source: scenes 1 and 2, a collection of both in that order
    Scene *a = new Scene(m_source);
    a->setName("A");
    QVERIFY(m_source->addFunction(a, 1));
    Scene *b = new Scene(m_source);
    b->setName("B");
    QVERIFY(m_source->addFunction(b, 2));
    Collection *coll = new Collection(m_source);
    coll->setName("Both");
    QVERIFY(m_source->addFunction(coll, 5));
    QVERIFY(coll->addFunction(1));
    QVERIFY(coll->addFunction(2));

    /* Target has functions 0 and 1: A can't keep its id */
    QVERIFY(m_target->addFunction(new Scene(m_target), 0));
    Scene *occupant = new Scene(m_target);
    occupant->setName("Occupant");
    QVERIFY(m_target->addFunction(occupant, 1));

    QxwImportResult r = QxwImporter::import(m_source, m_target, QList<quint32>(),
                                            QList<quint32>(), QList<quint32>() << 5);
    QCOMPARE(r.functionsImported, 3);
    const quint32 newA = r.functionMap.value(1);
    const quint32 newB = r.functionMap.value(2);
    /* Which of the two is cloned first is up to a hash's order: A on 2 and
       B on 3 (the chain the old loop broke), or B kept on 2 and A on 3 */
    QVERIFY(newA != 1 && newA != newB);
    QVERIFY((newA == 2 && newB == 3) || (newA == 3 && newB == 2));
    QCOMPARE(m_target->function(newA)->name(), QString("A"));
    QCOMPARE(m_target->function(newB)->name(), QString("B"));

    Collection *imported = qobject_cast<Collection *>(m_target->function(r.functionMap.value(5)));
    QVERIFY(imported != NULL);
    QCOMPARE(imported->components(), QList<quint32>({ newA, newB }));
    QCOMPARE(m_target->function(1)->name(), QString("Occupant"));     // untouched
}

void QxwImporter_Test::chaserStepsAndEfxHeadsFollowRemaps()
{
    Fixture *fx = new Fixture(m_source);
    fx->setName("Mover");
    fx->setChannels(4);
    QVERIFY(m_source->addFixture(fx, 0));
    EFX *efx = new EFX(m_source);
    efx->setName("Circle");
    QVERIFY(m_source->addFunction(efx, 3));
    EFXFixture *ef = new EFXFixture(efx);
    ef->setHead(GroupHead(0, 0));
    QVERIFY(efx->addFixture(ef));
    Scene *s = new Scene(m_source);
    QVERIFY(m_source->addFunction(s, 4));
    Chaser *ch = new Chaser(m_source);
    QVERIFY(m_source->addFunction(ch, 6));
    QVERIFY(ch->addStep(ChaserStep(4)));
    QVERIFY(ch->addStep(ChaserStep(3)));

    // The target already has fixture 0 and functions 3 and 4
    Fixture *tfx = new Fixture(m_target);
    tfx->setName("House");
    tfx->setChannels(1);
    tfx->setAddress(100);
    QVERIFY(m_target->addFixture(tfx, 0));
    QVERIFY(m_target->addFunction(new Scene(m_target), 3));
    QVERIFY(m_target->addFunction(new Scene(m_target), 4));

    QxwImportResult r = QxwImporter::import(m_source, m_target, QList<quint32>(),
                                            QList<quint32>(), QList<quint32>() << 6);
    Chaser *ich = qobject_cast<Chaser *>(m_target->function(r.functionMap.value(6)));
    QVERIFY(ich != NULL);
    QCOMPARE(ich->steps().size(), 2);
    QCOMPARE(ich->steps().at(0).fid, r.functionMap.value(4));
    QCOMPARE(ich->steps().at(1).fid, r.functionMap.value(3));
    QVERIFY(r.functionMap.value(3) != 3);

    EFX *iefx = qobject_cast<EFX *>(m_target->function(r.functionMap.value(3)));
    QVERIFY(iefx != NULL);
    QCOMPARE(iefx->fixtures().size(), 1);
    const quint32 movedFx = iefx->fixtures().first()->head().fxi;
    QVERIFY(movedFx != 0);
    QVERIFY(m_target->fixture(movedFx) != NULL);
    QCOMPARE(m_target->fixture(movedFx)->name(), QString("Mover"));
    QCOMPARE(m_target->fixture(0)->name(), QString("House"));
}

QTEST_APPLESS_MAIN(QxwImporter_Test)

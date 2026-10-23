/*
  Q Light Controller Plus - Unit test
  programmercontroller_test.cpp

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

#include <QtTest>
#include <QSignalSpy>
#include <QColor>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#define protected public
#define private public
#include "programmercontroller.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcchannel.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "qlcfile.h"
#include "scene.h"
#include "doc.h"
#include "mastertimer.h"
#include "universe.h"
#include "inputoutputmap.h"
#include "parkeffect.h"
#undef private
#undef protected

#include "programmercontroller_test.h"
#include "../common/resource_paths.h"

void ProgrammerController_Test::initTestCase()
{
    m_doc = new Doc(this);

    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir) == true);

    m_scannerDef = m_doc->fixtureDefCache()->fixtureDef("Futurelight", "DJScan250");
    QVERIFY(m_scannerDef != nullptr);
    m_rgbDef = m_doc->fixtureDefCache()->fixtureDef("Generic", "Generic RGB");
    QVERIFY(m_rgbDef != nullptr);
}

void ProgrammerController_Test::cleanupTestCase()
{
    delete m_doc;
}

void ProgrammerController_Test::cleanup()
{
    // clearContents() drops functions/palettes/fixture groups/fixtures, but
    // NOT programmer state (raw values, edited-scene tracking) -- that's a
    // separate concern the real Save gesture only clears explicitly at the
    // very end (see programmercontroller.h's own doc comment on
    // clearProgrammerValues()). Reset both so each test starts clean.
    // A test that fails part-way leaves its functions running; stop them
    // before clearContents() deletes them under the next test's ticks.
    MasterTimer *timer = m_doc->masterTimer();
    timer->m_stopAllFunctions = true;
    timer->timerTick();
    timer->timerTick();
    timer->m_stopAllFunctions = false;
    m_doc->clearContents();
    m_doc->clearProgrammerValues();
}

/* A fixture built from a real fixture definition -- categorizeChannel() (the
 * heart of proposedSaveBuckets()) needs real QLCChannel::group()/colour()
 * data, which a channel-less stub Fixture (just setChannels(n)) doesn't
 * have. */
static Fixture *makeFixture(Doc *doc, QLCFixtureDef *def, const QString &modeName,
                            quint32 address)
{
    QLCFixtureMode *mode = def->mode(modeName);
    if (mode == nullptr)
        return nullptr;
    Fixture *fxi = new Fixture(doc);
    fxi->setFixtureDefinition(def, mode);
    fxi->setAddress(address);
    fxi->setUniverse(0);
    if (!doc->addFixture(fxi))
    {
        delete fxi;
        return nullptr;
    }
    return fxi;
}

void ProgrammerController_Test::paintingAProgrammerValueMarksItDirty()
{
    Fixture *fxi = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    QVERIFY(fxi != nullptr);

    QVERIFY2(!m_doc->isProgrammerDirty(), "a fresh Doc's programmer is already dirty");
    QVERIFY(!m_doc->hasProgrammerValues());

    QSignalSpy dirtySpy(m_doc->programmer(), &ProgrammerController::programmerDirtyChanged);

    m_doc->setProgrammerValue(fxi->id(), 0, 200);

    QVERIFY2(m_doc->isProgrammerDirty(), "painting a raw value did not dirty the programmer");
    QVERIFY(m_doc->hasProgrammerValues());
    QCOMPARE(m_doc->programmerValues().value(fxi->id()).value(0), uchar(200));
    QCOMPARE(dirtySpy.count(), 1);
    QCOMPARE(dirtySpy.first().first().toBool(), true);

    m_doc->clearProgrammerValues();

    QVERIFY2(!m_doc->isProgrammerDirty(), "clearProgrammerValues() left the programmer dirty");
    QVERIFY(!m_doc->hasProgrammerValues());
    QVERIFY(m_doc->programmerValues().isEmpty());
    QCOMPARE(dirtySpy.count(), 2);
    QCOMPARE(dirtySpy.last().first().toBool(), false);
}

void ProgrammerController_Test::proposedSaveBucketsCategorizeByChannelRole()
{
    // DJScan250 "Mode 1": ch0=Pan, ch1=Tilt, ch2=Colour(wheel), ch3=Gobo
    // Selection, ch4=Gobo Rotation, ch5=Shutter(Intensity, NoColour).
    Fixture *scanner = makeFixture(m_doc, m_scannerDef, "Mode 1", 0);
    QVERIFY(scanner != nullptr);
    // Generic RGB "RGB Dimmer": ch0=Red, ch1=Green, ch2=Blue, ch3=Dimmer.
    Fixture *rgb = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 10);
    QVERIFY(rgb != nullptr);

    m_doc->setProgrammerValue(scanner->id(), 0, 100);   // Pan -> Position
    m_doc->setProgrammerValue(scanner->id(), 1, 50);    // Tilt -> Position
    m_doc->setProgrammerValue(scanner->id(), 3, 20);    // Gobo Selection -> Special
    m_doc->setProgrammerValue(rgb->id(), 0, 255);       // Red -> Color
    m_doc->setProgrammerValue(rgb->id(), 1, 0);         // Green -> Color
    m_doc->setProgrammerValue(rgb->id(), 2, 0);         // Blue -> Color
    m_doc->setProgrammerValue(rgb->id(), 3, 191);       // Dimmer -> Intensity (NoColour)

    const QList<Doc::SaveBucket> buckets = m_doc->proposedSaveBuckets();
    // No FixtureGroup exists at all, so every category falls back to the
    // per-fixture "no covering group" branch -- one bucket per category
    // here, since each category's edited channels all belong to a single
    // fixture.
    QCOMPARE(buckets.size(), 4);

    bool sawPosition = false, sawColor = false, sawIntensity = false, sawSpecial = false;
    for (const Doc::SaveBucket &b : buckets)
    {
        // No group covers any of these fixtures, so groupName/defaultPath
        // fall back to the "Custom" placeholder in every bucket.
        QCOMPARE(b.groupName, QString("Custom"));

        switch (b.category)
        {
        case Doc::SaveCatPosition:
            sawPosition = true;
            QCOMPARE(b.defaultName, QString("Pos"));
            QCOMPARE(b.defaultPath, QString("Positions/Custom"));
            QCOMPARE(b.values.value(scanner->id()).value(0), uchar(100));
            QCOMPARE(b.values.value(scanner->id()).value(1), uchar(50));
            QCOMPARE(b.values.size(), 1);
            break;
        case Doc::SaveCatColor:
            sawColor = true;
            QCOMPARE(b.defaultName, QString("Red"));
            QCOMPARE(b.defaultPath, QString("Colors/Custom"));
            QCOMPARE(b.values.value(rgb->id()).size(), 3);
            QCOMPARE(b.values.value(rgb->id()).value(0), uchar(255));
            break;
        case Doc::SaveCatIntensity:
            sawIntensity = true;
            QCOMPARE(b.defaultName, QString("Int 75%"));
            QCOMPARE(b.defaultPath, QString("Intensity/Custom"));
            QCOMPARE(b.values.value(rgb->id()).value(3), uchar(191));
            break;
        case Doc::SaveCatSpecial:
            sawSpecial = true;
            QCOMPARE(b.defaultName, QString("Special"));
            QCOMPARE(b.defaultPath, QString("Specials/Custom"));
            QCOMPARE(b.values.value(scanner->id()).value(3), uchar(20));
            break;
        default:
            QFAIL(qPrintable(QString("unexpected bucket category %1").arg(int(b.category))));
        }
    }
    QVERIFY2(sawPosition && sawColor && sawIntensity && sawSpecial,
             "one of the four expected categories never got a bucket");
}

void ProgrammerController_Test::proposedSaveBucketsPickTheLargestCoveringGroup()
{
    Fixture *f1 = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    Fixture *f2 = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 10);
    QVERIFY(f1 != nullptr && f2 != nullptr);

    // A smaller group covering only f1 -- does NOT cover every edited
    // fixture in the category, so it's not even a candidate.
    FixtureGroup *small = new FixtureGroup(m_doc);
    small->setName("Just F1");
    QVERIFY(m_doc->addFixtureGroup(small));
    small->assignFixture(f1->id());

    // A larger group covering both -- the only real candidate, so it wins
    // regardless of how many OTHER groups might also (partially) apply.
    FixtureGroup *large = new FixtureGroup(m_doc);
    large->setName("Both");
    QVERIFY(m_doc->addFixtureGroup(large));
    large->assignFixture(f1->id());
    large->assignFixture(f2->id());

    m_doc->setProgrammerValue(f1->id(), 3, 200);   // Dimmer -> Intensity
    m_doc->setProgrammerValue(f2->id(), 3, 200);

    const QList<Doc::SaveBucket> buckets = m_doc->proposedSaveBuckets();
    QCOMPARE(buckets.size(), 1);
    QCOMPARE(buckets.first().fixtureGroupId, large->id());
    QCOMPARE(buckets.first().groupName, QString("Both"));
    QCOMPARE(buckets.first().values.size(), 2);
}

void ProgrammerController_Test::proposedSaveBucketsFallBackPerFixtureWhenNoGroupCoversAll()
{
    Fixture *f1 = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    Fixture *f2 = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 10);
    QVERIFY(f1 != nullptr && f2 != nullptr);

    // Disjoint groups: neither contains both edited fixtures, so there is
    // no single covering group for the category at all.
    FixtureGroup *gA = new FixtureGroup(m_doc);
    gA->setName("Group A");
    QVERIFY(m_doc->addFixtureGroup(gA));
    gA->assignFixture(f1->id());

    FixtureGroup *gB = new FixtureGroup(m_doc);
    gB->setName("Group B");
    QVERIFY(m_doc->addFixtureGroup(gB));
    gB->assignFixture(f2->id());

    m_doc->setProgrammerValue(f1->id(), 3, 200);   // Dimmer -> Intensity
    m_doc->setProgrammerValue(f2->id(), 3, 100);

    const QList<Doc::SaveBucket> buckets = m_doc->proposedSaveBuckets();
    QCOMPARE(buckets.size(), 2);

    bool sawA = false, sawB = false;
    for (const Doc::SaveBucket &b : buckets)
    {
        QCOMPARE(b.category, Doc::SaveCatIntensity);
        if (b.fixtureGroupId == gA->id())
        {
            sawA = true;
            QCOMPARE(b.groupName, QString("Group A"));
            QCOMPARE(b.values.size(), 1);
            QVERIFY(b.values.contains(f1->id()));
        }
        else if (b.fixtureGroupId == gB->id())
        {
            sawB = true;
            QCOMPARE(b.groupName, QString("Group B"));
            QCOMPARE(b.values.size(), 1);
            QVERIFY(b.values.contains(f2->id()));
        }
    }
    QVERIFY2(sawA && sawB, "each fixture should have landed in its OWN group's bucket");
}

void ProgrammerController_Test::saveBucketAsSceneBakesExactValuesAndDoesNotClearProgrammerState()
{
    Fixture *fxi = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    QVERIFY(fxi != nullptr);

    m_doc->setProgrammerValue(fxi->id(), 0, 255);   // Red
    m_doc->setProgrammerValue(fxi->id(), 1, 10);    // Green
    m_doc->setProgrammerValue(fxi->id(), 2, 20);    // Blue

    const QList<Doc::SaveBucket> buckets = m_doc->proposedSaveBuckets();
    QCOMPARE(buckets.size(), 1);
    const Doc::SaveBucket bucket = buckets.first();
    QCOMPARE(bucket.defaultName, QString("Red"));   // nearest named colour to (255,10,20)

    // Real caller shape: the Save dialog pre-fills its Path field from
    // bucket.defaultPath and always passes it through (possibly
    // user-edited) -- pass it explicitly here too. Empty name DOES fall
    // back to bucket.defaultName, but empty path does NOT fall back to
    // bucket.defaultPath the same way (see the dedicated test for that
    // asymmetry below); this test is about the bake, not that quirk.
    const quint32 sceneId = m_doc->saveBucketAsScene(bucket, QString(), bucket.defaultPath);
    QVERIFY2(sceneId != Function::invalidId(), "saveBucketAsScene() failed on a non-empty bucket");

    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    QVERIFY(scene != nullptr);
    QCOMPARE(scene->name(), bucket.defaultName);
    // path(true) = "simplified": without the "<Type>/" prefix path(false)
    // (Function::path()'s default) always adds -- bucket.defaultPath is
    // already in that simplified form.
    QCOMPARE(scene->path(true), bucket.defaultPath);

    QHash<quint32, uchar> got;
    for (const SceneValue &sv : scene->values())
    {
        QCOMPARE(sv.fxi, fxi->id());
        got[sv.channel] = sv.value;
    }
    QCOMPARE(got.size(), 3);
    QCOMPARE(got.value(0), uchar(255));
    QCOMPARE(got.value(1), uchar(10));
    QCOMPARE(got.value(2), uchar(20));

    // Save does NOT clear programmer state itself -- that is the caller's
    // job (VCButton::saveProgrammer() does it once, after processing every
    // row in the real Save dialog).
    QVERIFY2(m_doc->isProgrammerDirty(),
             "saveBucketAsScene() cleared the programmer -- it shouldn't, the caller does that");
    QVERIFY(m_doc->hasProgrammerValues());
    QCOMPARE(m_doc->programmerValues().value(fxi->id()).size(), 3);

    m_doc->clearProgrammerValues();
    QVERIFY(!m_doc->isProgrammerDirty());
    QVERIFY(m_doc->programmerValues().isEmpty());
}

/* Found while writing the test above: saveBucketAsScene()'s name and path
 * arguments look symmetric (both "possibly user-edited, falls back to the
 * bucket's own default when blank") but are NOT:
 *   scene->setName(name.isEmpty() ? bucket.defaultName : name);
 *   if (!path.isEmpty()) scene->setPath(path);
 * An empty path is simply never passed to setPath() at all -- the Scene
 * keeps whatever its own ordinary constructor default is ("Scene/"), not
 * bucket.defaultPath. The real UI caller (VCButton::saveProgrammer()) never
 * hits this in practice because its Save dialog always pre-fills the Path
 * field from bucket.defaultPath before the user could possibly blank it,
 * but any other/future caller passing an empty path expecting the same
 * "falls back to the sensible default" behaviour name gets would silently
 * misfile the scene into the wrong folder.
 */
void ProgrammerController_Test::saveBucketAsSceneEmptyPathDoesNotFallBackToTheBucketsDefaultPath()
{
    Fixture *fxi = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    QVERIFY(fxi != nullptr);
    m_doc->setProgrammerValue(fxi->id(), 3, 100);   // Dimmer -> Intensity

    const QList<Doc::SaveBucket> buckets = m_doc->proposedSaveBuckets();
    QCOMPARE(buckets.size(), 1);
    const Doc::SaveBucket bucket = buckets.first();
    QVERIFY2(!bucket.defaultPath.isEmpty(), "test setup assumption broken: expected a non-empty default path");

    const quint32 sceneId = m_doc->saveBucketAsScene(bucket, QString(), QString());
    QVERIFY(sceneId != Function::invalidId());
    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    QVERIFY(scene != nullptr);

    QCOMPARE(scene->name(), bucket.defaultName);   // name DID fall back
    QVERIFY2(scene->path(true) != bucket.defaultPath,
             "an empty path argument unexpectedly landed the scene in the bucket's default path -- "
             "if saveBucketAsScene() was changed to fall back the way name() does, update this test "
             "(and its sibling above, which now deliberately passes bucket.defaultPath explicitly) "
             "to match the new, more consistent behaviour");
}

void ProgrammerController_Test::findMatchingSceneFindsByteForByteMatchesOnly()
{
    Fixture *fxi = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    QVERIFY(fxi != nullptr);

    Scene *scene = new Scene(m_doc);
    scene->setName("Reference");
    scene->setValue(fxi->id(), 0, 100);
    scene->setValue(fxi->id(), 1, 50);
    QVERIFY(m_doc->addFunction(scene));

    QHash<quint32, QHash<quint32, uchar>> exact;
    exact[fxi->id()][0] = 100;
    exact[fxi->id()][1] = 50;
    QCOMPARE(m_doc->findMatchingScene(exact), scene->id());

    QHash<quint32, QHash<quint32, uchar>> oneByteOff = exact;
    oneByteOff[fxi->id()][1] = 51;
    QCOMPARE(m_doc->findMatchingScene(oneByteOff), Function::invalidId());

    QHash<quint32, QHash<quint32, uchar>> subset = exact;
    subset[fxi->id()].remove(1);
    QVERIFY2(m_doc->findMatchingScene(subset) == Function::invalidId(),
             "a values SUBSET of an existing scene incorrectly counted as a match");

    QCOMPARE(m_doc->findMatchingScene(exact, scene->id()), Function::invalidId());
}

void ProgrammerController_Test::savingTheSameBucketTwiceCreatesTwoIndependentScenes()
{
    Fixture *fxi = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    QVERIFY(fxi != nullptr);
    m_doc->setProgrammerValue(fxi->id(), 3, 100);   // Dimmer -> Intensity

    const QList<Doc::SaveBucket> buckets = m_doc->proposedSaveBuckets();
    QCOMPARE(buckets.size(), 1);
    const Doc::SaveBucket bucket = buckets.first();

    const quint32 id1 = m_doc->saveBucketAsScene(bucket, QStringLiteral("Dup"), QStringLiteral("Path"));
    const quint32 id2 = m_doc->saveBucketAsScene(bucket, QStringLiteral("Dup"), QStringLiteral("Path"));
    QVERIFY(id1 != Function::invalidId());
    QVERIFY(id2 != Function::invalidId());
    QVERIFY2(id1 != id2,
             "saveBucketAsScene() returned the SAME id twice -- it should always create a new Scene");

    Scene *s1 = qobject_cast<Scene*>(m_doc->function(id1));
    Scene *s2 = qobject_cast<Scene*>(m_doc->function(id2));
    QVERIFY(s1 != nullptr && s2 != nullptr);
    QCOMPARE(s1->values().size(), 1);
    QCOMPARE(s2->values().size(), 1);
    QCOMPARE(s1->values().first().value, s2->values().first().value);

    // No de-duplication anywhere in this pipeline: saveBucketAsScene() never
    // consults findMatchingScene() itself, so two functionally-identical
    // Scenes now both exist with no complaint. findMatchingScene() finds
    // WHICHEVER one the (unordered) function map iterates to first.
    const quint32 matched = m_doc->findMatchingScene(bucket.values);
    QVERIFY2(matched == id1 || matched == id2,
             "neither duplicate scene was found as a match for the bucket's own values");
}

void ProgrammerController_Test::saveBucketAsGroupSceneBuildsAPaletteAndFallsBackWhenItCant()
{
    // --- Part A: a Color bucket becomes a real palette-driven group scene ---
    Fixture *f1 = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    Fixture *f2 = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 10);
    QVERIFY(f1 != nullptr && f2 != nullptr);

    FixtureGroup *wash = new FixtureGroup(m_doc);
    wash->setName("Wash");
    QVERIFY(m_doc->addFixtureGroup(wash));
    wash->assignFixture(f1->id());
    wash->assignFixture(f2->id());

    // Different Red values on each fixture, to prove the palette carries
    // the AVERAGE, not either fixture's own value.
    m_doc->setProgrammerValue(f1->id(), 0, 200);
    m_doc->setProgrammerValue(f1->id(), 1, 0);
    m_doc->setProgrammerValue(f1->id(), 2, 0);
    m_doc->setProgrammerValue(f2->id(), 0, 100);
    m_doc->setProgrammerValue(f2->id(), 1, 0);
    m_doc->setProgrammerValue(f2->id(), 2, 0);

    QList<Doc::SaveBucket> buckets = m_doc->proposedSaveBuckets();
    QCOMPARE(buckets.size(), 1);
    QCOMPARE(buckets.first().fixtureGroupId, wash->id());

    const quint32 colorSceneId = m_doc->saveBucketAsGroupScene(buckets.first(), QString(), QString());
    QVERIFY2(colorSceneId != Function::invalidId(), "saveBucketAsGroupScene() failed on a plain RGB bucket");

    Scene *colorScene = qobject_cast<Scene*>(m_doc->function(colorSceneId));
    QVERIFY(colorScene != nullptr);
    QVERIFY2(colorScene->fixtureGroups().contains(wash->id()),
             "the group scene does not reference the fixture group it was built from");
    QCOMPARE(colorScene->palettes().size(), 1);

    QLCPalette *pal = m_doc->palette(colorScene->palettes().first());
    QVERIFY(pal != nullptr);
    QCOMPARE(pal->type(), QLCPalette::Color);
    QCOMPARE(pal->rgbValue(), QColor(150, 0, 0));   // (200+100)/2 averaged

    QVERIFY2(colorScene->values().isEmpty(),
             "the palette fully covers R/G/B on both fixtures -- nothing should be baked per-fixture");

    m_doc->clearProgrammerValues();

    // --- Part B: a mixed Special bucket can't be ONE palette, so it falls
    //     back to baking every channel per-fixture into the SAME scene, and
    //     the scene ends up with no fixture-group reference at all (the
    //     addFixtureGroup() call above only happens inside the "palette was
    //     built" branch) -- functionally indistinguishable from a plain
    //     saveBucketAsScene() bake. ---
    Fixture *scanner = makeFixture(m_doc, m_scannerDef, "Mode 1", 20);
    QVERIFY(scanner != nullptr);
    FixtureGroup *scanners = new FixtureGroup(m_doc);
    scanners->setName("Scanners");
    QVERIFY(m_doc->addFixtureGroup(scanners));
    scanners->assignFixture(scanner->id());

    m_doc->setProgrammerValue(scanner->id(), 3, 20);   // Gobo Selection (Gobo group)
    m_doc->setProgrammerValue(scanner->id(), 4, 50);   // Gobo Rotation (Speed group) -- mixes the bucket

    buckets = m_doc->proposedSaveBuckets();
    QCOMPARE(buckets.size(), 1);
    QCOMPARE(buckets.first().category, Doc::SaveCatSpecial);
    QCOMPARE(buckets.first().fixtureGroupId, scanners->id());

    const quint32 specialSceneId = m_doc->saveBucketAsGroupScene(buckets.first(), QString(), QString());
    QVERIFY2(specialSceneId != Function::invalidId(),
             "a mixed Special bucket should still save via the per-fixture bake fallback");

    Scene *specialScene = qobject_cast<Scene*>(m_doc->function(specialSceneId));
    QVERIFY(specialScene != nullptr);
    QVERIFY2(specialScene->palettes().isEmpty(),
             "a mixed Gobo+Speed bucket can't be one palette -- buildPaletteFromBucket() should have "
             "returned NULL, so no palette should have been attached");
    QVERIFY2(specialScene->fixtureGroups().isEmpty(),
             "the fixture group is only attached inside the palette-built branch -- a fallback-baked "
             "group scene ends up with no group reference at all, same as a plain saveBucketAsScene()");

    QHash<quint32, uchar> gotSpecial;
    for (const SceneValue &sv : specialScene->values())
        gotSpecial[sv.channel] = sv.value;
    QCOMPARE(gotSpecial.size(), 2);
    QCOMPARE(gotSpecial.value(3), uchar(20));
    QCOMPARE(gotSpecial.value(4), uchar(50));
}

void ProgrammerController_Test::saveBucketAsGroupSceneRequiresARealFixtureGroup()
{
    Fixture *fxi = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    QVERIFY(fxi != nullptr);

    Doc::SaveBucket bucket;
    bucket.category = Doc::SaveCatColor;
    bucket.fixtureGroupId = Function::invalidId();   // the "Custom"/no-group shape
    bucket.defaultName = QStringLiteral("Whatever");
    bucket.values[fxi->id()][0] = 200;

    const quint32 id = m_doc->saveBucketAsGroupScene(bucket, QString(), QString());
    QCOMPARE(id, Function::invalidId());
    QVERIFY2(m_doc->functions().isEmpty(),
             "saveBucketAsGroupScene() left behind a Scene despite returning invalidId()");
}

static void tick(Doc *doc, int n = 3)
{
    for (int i = 0; i < n; ++i)
        doc->masterTimer()->timerTick();
}

static uchar preGM(Doc *doc, int address)
{
    QList<Universe *> unis = doc->inputOutputMap()->claimUniverses();
    for (Universe *u : unis)
        u->processFaders();
    const uchar v = unis.first()->preGMValue(address);
    doc->inputOutputMap()->releaseUniverses(false);
    return v;
}

/* Park records the fixture's output at the moment it is parked. */
static Scene *sceneOf(Doc *doc, quint32 fid, const QList<QPair<quint32, uchar>> &values)
{
    Scene *sc = new Scene(doc);
    sc->addFixture(fid);
    for (const auto &v : values)
        sc->setValue(fid, v.first, v.second);
    doc->addFunction(sc);
    return sc;
}

void ProgrammerController_Test::aParkIsSavedWithTheShow()
{
    Fixture *fxi = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    QVERIFY(fxi != nullptr);
    const quint32 fid = fxi->id();
    ProgrammerController *pc = m_doc->programmer();
    QSignalSpy parkSpy(pc, &ProgrammerController::parkChanged);

    Scene *warm = sceneOf(m_doc, fid, { { 0, 200 }, { 3, 255 } });
    warm->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc);
    QCOMPARE(preGM(m_doc, 0), uchar(200));

    pc->parkFixtures({ fid });
    QVERIFY(pc->isFixtureParked(fid));
    QVERIFY(parkSpy.count() >= 1);
    QCOMPARE(pc->m_parkEffect->m_parked.value(fid).value(0), uchar(200));   // where it sat
    QCOMPARE(pc->m_parkEffect->m_parked.value(fid).value(3), uchar(255));
    warm->stop(FunctionParent::master());
    tick(m_doc);

    QByteArray xml;
    {
        QBuffer buf(&xml);
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        w.writeStartDocument();
        QVERIFY(m_doc->saveXML(&w));
        w.writeEndDocument();
    }
    QVERIFY(xml.contains("<Park"));

    // A new show lets go of it...
    m_doc->clearContents();
    QVERIFY(!pc->hasParkedFixtures());

    // ...and loading this one parks it again, at the same values.
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(m_doc->loadXML(r, false));
    QVERIFY(pc->isFixtureParked(fid));
    QCOMPARE(pc->m_parkEffect->m_parked.value(fid).value(0), uchar(200));
    QCOMPARE(pc->m_parkEffect->m_parked.value(fid).value(3), uchar(255));

    pc->unparkAllFixtures();
    QVERIFY(!pc->hasParkedFixtures());
}

void ProgrammerController_Test::aParkedFixtureHoldsItsOutput()
{
    Fixture *fxi = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    QVERIFY(fxi != nullptr);
    const quint32 fid = fxi->id();
    ProgrammerController *pc = m_doc->programmer();

    Scene *warm = sceneOf(m_doc, fid, { { 0, 200 }, { 3, 255 } });
    Scene *cold = sceneOf(m_doc, fid, { { 0, 10 }, { 2, 180 }, { 3, 255 } });
    warm->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc);
    QCOMPARE(preGM(m_doc, 3), uchar(255));     // a frame out: the rig shows it
    pc->parkFixtures({ fid });
    warm->stop(FunctionParent::master());
    tick(m_doc, 4);

    // Nothing running: the parked fixture stays lit where it was.
    QCOMPARE(preGM(m_doc, 0), uchar(200));
    QCOMPARE(preGM(m_doc, 3), uchar(255));

    // The next cue would turn it blue and dim its red: parked, it stays as
    // it was -- even where the cue is higher (blue, an HTP channel).
    cold->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 4);
    QCOMPARE(preGM(m_doc, 0), uchar(200));
    QCOMPARE(preGM(m_doc, 2), uchar(0));

    // Unparked, the fixture follows the show.
    pc->unparkAllFixtures();
    tick(m_doc, 4);
    QCOMPARE(preGM(m_doc, 0), uchar(10));
    QCOMPARE(preGM(m_doc, 2), uchar(180));
    cold->stop(FunctionParent::master());
    tick(m_doc);
}

void ProgrammerController_Test::highlightDrivesTheSelectionToFull()
{
    Fixture *fxi = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    QVERIFY(fxi != nullptr);
    ProgrammerController *pc = m_doc->programmer();
    Scene *dim = sceneOf(m_doc, fxi->id(), { { 0, 40 }, { 3, 60 } });
    dim->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc);
    QCOMPARE(preGM(m_doc, 3), uchar(60));

    pc->setProgrammerSelection({ fxi->id() });
    pc->setHighlightActive(true);
    QVERIFY(pc->isHighlightActive());
    tick(m_doc);
    QCOMPARE(preGM(m_doc, 3), uchar(255));

    pc->setHighlightActive(false);
    tick(m_doc);
    QCOMPARE(preGM(m_doc, 3), uchar(60));                // back to the show
    dim->stop(FunctionParent::master());
    tick(m_doc);
}

/* Highlight is over everything that runs, not merged with it: a cyan
   channel goes to 0 for the white whatever the cue gives it (an LTP channel
   the cue's fader used to rewrite every frame). Park is over Highlight. */
void ProgrammerController_Test::highlightBeatsARunningCue()
{
    QScopedPointer<QLCFixtureDef> def(new QLCFixtureDef());
    def->setManufacturer("Test");
    def->setModel("Dimmer CMY");
    QLCFixtureMode *mode = new QLCFixtureMode(def.data());
    mode->setName("2ch");
    QLCChannel *dim = new QLCChannel();
    dim->setName("Dimmer");
    dim->setGroup(QLCChannel::Intensity);
    def->addChannel(dim);
    mode->insertChannel(dim, 0);
    QLCChannel *cyan = new QLCChannel();
    cyan->setName("Cyan");
    cyan->setGroup(QLCChannel::Colour);                 // LTP
    cyan->setColour(QLCChannel::Cyan);
    def->addChannel(cyan);
    mode->insertChannel(cyan, 1);
    def->addMode(mode);
    Fixture *fxi = makeFixture(m_doc, def.data(), "2ch", 0);
    QVERIFY(fxi != nullptr);
    ProgrammerController *pc = m_doc->programmer();

    Scene *cue = sceneOf(m_doc, fxi->id(), { { 0, 90 }, { 1, 200 } });
    cue->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc);
    QCOMPARE(preGM(m_doc, 0), uchar(90));
    QCOMPARE(preGM(m_doc, 1), uchar(200));

    pc->setProgrammerSelection({ fxi->id() });
    pc->setHighlightActive(true);
    tick(m_doc, 4);
    QCOMPARE(preGM(m_doc, 0), uchar(255));
    QCOMPARE(preGM(m_doc, 1), uchar(0));                 // the cue's 200 loses

    // Parked (at the cue's look) while highlighted: Park is on top.
    pc->setHighlightActive(false);
    tick(m_doc);
    QCOMPARE(preGM(m_doc, 1), uchar(200));
    pc->parkFixtures({ fxi->id() });
    pc->setHighlightActive(true);
    tick(m_doc, 4);
    QCOMPARE(preGM(m_doc, 0), uchar(90));
    QCOMPARE(preGM(m_doc, 1), uchar(200));
    pc->unparkAllFixtures();
    tick(m_doc);
    QCOMPARE(preGM(m_doc, 1), uchar(0));                 // highlight again

    pc->setHighlightActive(false);
    tick(m_doc);
    QCOMPARE(preGM(m_doc, 0), uchar(90));                // back to the show
    QCOMPARE(preGM(m_doc, 1), uchar(200));
    cue->stop(FunctionParent::master());
    tick(m_doc);
    m_doc->clearContents();                              // before the def goes
}

void ProgrammerController_Test::aLearnedControlTogglesItsFunction()
{
    Fixture *fxi = makeFixture(m_doc, m_rgbDef, "RGB Dimmer", 0);
    QVERIFY(fxi != nullptr);
    Scene *a = new Scene(m_doc);
    a->addFixture(fxi->id());
    a->setValue(fxi->id(), 3, 255);
    m_doc->addFunction(a);
    Scene *b = new Scene(m_doc);
    b->addFixture(fxi->id());
    b->setValue(fxi->id(), 0, 255);
    m_doc->addFunction(b);
    ProgrammerController *pc = m_doc->programmer();

    QSignalSpy changed(pc, &ProgrammerController::functionTriggerChanged);
    pc->learnFunctionTrigger(a->id());
    pc->slotControllerInputChanged(0, 36, 0, QString());   // a release: not bound
    QVERIFY(!pc->hasFunctionTrigger(a->id()));
    pc->slotControllerInputChanged(0, 36, 127, QString()); // the press binds
    QVERIFY(pc->hasFunctionTrigger(a->id()));
    QCOMPARE(pc->functionTriggerLabel(a->id()), QStringLiteral("Uni 1 Ch 36"));
    QCOMPARE(changed.count(), 1);
    QVERIFY(!a->isRunning());                               // learning didn't fire it

    // Press: on. Release: nothing. Press again: off.
    pc->slotControllerInputChanged(0, 36, 127, QString());
    tick(m_doc);
    QVERIFY(a->isRunning());
    QCOMPARE(preGM(m_doc, 3), uchar(255));
    pc->slotControllerInputChanged(0, 36, 0, QString());
    tick(m_doc);
    QVERIFY(a->isRunning());
    pc->slotControllerInputChanged(0, 36, 127, QString());
    tick(m_doc);
    QVERIFY(!a->isRunning());

    // Another control does nothing.
    pc->slotControllerInputChanged(0, 37, 127, QString());
    tick(m_doc);
    QVERIFY(!a->isRunning());

    // The same control learned for B: one control, one function.
    pc->learnFunctionTrigger(b->id());
    pc->slotControllerInputChanged(0, 36, 127, QString());
    QVERIFY(pc->hasFunctionTrigger(b->id()));
    QVERIFY(!pc->hasFunctionTrigger(a->id()));
    pc->slotControllerInputChanged(0, 36, 127, QString());
    tick(m_doc);
    QVERIFY(b->isRunning());
    QVERIFY(!a->isRunning());
    pc->slotControllerInputChanged(0, 36, 127, QString());
    tick(m_doc);
    QVERIFY(!b->isRunning());

    // Cleared: the control is free again.
    pc->clearFunctionTrigger(b->id());
    QVERIFY(!pc->hasFunctionTrigger(b->id()));
    pc->slotControllerInputChanged(0, 36, 127, QString());
    tick(m_doc);
    QVERIFY(!b->isRunning());
}

QTEST_MAIN(ProgrammerController_Test)

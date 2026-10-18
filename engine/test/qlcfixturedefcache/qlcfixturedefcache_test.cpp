/*
  Q Light Controller - Unit tests
  qlcfixturedefcache_test.cpp

  Copyright (C) Heikki Junnila

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
#include <QTemporaryDir>
#include <QDateTime>
#include <QRegularExpression>
#include "qlcfixturemode.h"
#include "qlcphysical.h"

#define private public
#include "qlcfixturedefcache.h"
#include "qlcchannel.h"
#undef private

#include "qlcfixturedefcache_test.h"
#include "qlcfixturedef.h"
#include "qlcconfig.h"
#include "qlcfile.h"

#include "../common/resource_paths.h"

void QLCFixtureDefCache_Test::init()
{
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(cache.load(QDir("/just/kidding/stoopid")) == false);
    QVERIFY(cache.loadMap(dir) == true);
}

void QLCFixtureDefCache_Test::cleanup()
{
    cache.clear();
}

void QLCFixtureDefCache_Test::duplicates()
{
    // Check that duplicates are discarded
    int num = cache.m_defs.size();
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    cache.load(dir);
    QCOMPARE(cache.m_defs.size(), num);
}

void QLCFixtureDefCache_Test::add()
{
    QVERIFY(cache.fixtureCache().isEmpty() == false);
    QVERIFY(cache.fixtureCache()["Martin"]["MAC250"] == false);

    QVERIFY(cache.addFixtureDef(NULL) == false);

    QVERIFY(cache.manufacturers().count() != 0);
    cache.clear();
    QVERIFY(cache.manufacturers().count() == 0);

    /* Add the first fixtureDef */
    QLCFixtureDef* def = new QLCFixtureDef();
    def->setManufacturer("Martin");
    def->setModel("MAC250");

    QVERIFY(cache.addFixtureDef(def) == true);
    QVERIFY(cache.manufacturers().count() == 1);
    QVERIFY(cache.manufacturers().contains("Martin") == true);
    QVERIFY(cache.manufacturers().contains("MAC250") == false);

    QVERIFY(cache.models("Martin").count() == 1);
    QVERIFY(cache.models("Martin").contains("MAC250") == true);
    QVERIFY(cache.models("Foo").count() == 0);

    /* Another fixtureDef, same manufacturer & model. Should be ignored. */
    QLCFixtureDef* def2 = new QLCFixtureDef();
    def2->setManufacturer("Martin");
    def2->setModel("MAC250");

    QVERIFY(cache.addFixtureDef(def2) == false);
    QVERIFY(cache.manufacturers().count() == 1);
    QVERIFY(cache.manufacturers().contains("Martin") == true);

    delete def2;
    def2 = NULL;

    /* Another fixtureDef, same manufacturer, different model */
    def2 = new QLCFixtureDef();
    def2->setManufacturer("Martin");
    def2->setModel("MAC500");

    QVERIFY(cache.addFixtureDef(def2) == true);
    QVERIFY(cache.manufacturers().count() == 1);
    QVERIFY(cache.manufacturers().contains("Martin") == true);
    QVERIFY(cache.manufacturers().contains("MAC500") == false);

    QVERIFY(cache.models("Martin").count() == 2);
    QVERIFY(cache.models("Martin").contains("MAC250") == true);
    QVERIFY(cache.models("Martin").contains("MAC500") == true);

    /* Another fixtureDef, different manufacturer, different model */
    QLCFixtureDef* def3 = new QLCFixtureDef();
    def3->setManufacturer("Futurelight");
    def3->setModel("PHS700");

    QVERIFY(cache.addFixtureDef(def3) == true);
    QVERIFY(cache.manufacturers().count() == 2);
    QVERIFY(cache.manufacturers().contains("Martin") == true);
    QVERIFY(cache.manufacturers().contains("Futurelight") == true);
    QVERIFY(cache.manufacturers().contains("PHS700") == false);

    /* Another fixtureDef, different manufacturer, same model */
    QLCFixtureDef* def4 = new QLCFixtureDef();
    def4->setManufacturer("Yoyodyne");
    def4->setModel("MAC250");

    QVERIFY(cache.addFixtureDef(def4) == true);
    QVERIFY(cache.manufacturers().count() == 3);
    QVERIFY(cache.manufacturers().contains("Martin") == true);
    QVERIFY(cache.manufacturers().contains("Futurelight") == true);
    QVERIFY(cache.manufacturers().contains("Yoyodyne") == true);
    QVERIFY(cache.manufacturers().contains("MAC250") == false);

    QVERIFY(cache.models("Yoyodyne").count() == 1);
    QVERIFY(cache.models("Yoyodyne").contains("MAC250") == true);
}

void QLCFixtureDefCache_Test::reload()
{
    QLCFixtureDef *def = cache.fixtureDef("Botex", "SP-1500");
    QLCChannel *channel = def->channel("Control");

    QVERIFY(def->channels().count() == 5);
    def->removeChannel(channel);
    QVERIFY(def->channels().count() == 4);

    cache.reloadFixtureDef(def);
    QVERIFY(def->channels().count() == 5);
}

void QLCFixtureDefCache_Test::fixtureDef()
{
    // check the content of a cached fixture relative path
    QString firstManufacturer = cache.m_defs.first()->manufacturer();
    QString firstModel = cache.m_defs.first()->model();
    QString relPath = QString("%1/%1-%2.qxf").arg(firstManufacturer).arg(firstModel);
    relPath.replace(" ", "-");
    QVERIFY(cache.m_defs.first()->definitionSourceFile() == relPath);

    // request a fixture cached but not yet loaded
    QLCFixtureDef *def = cache.fixtureDef("Futurelight", "CY-200");

    // check that once loaded, the relative path becomes absolute
    QDir absDir(def->definitionSourceFile());
    QVERIFY(absDir.isAbsolute() == true);

    cache.clear();

    QLCFixtureDef *def1 = new QLCFixtureDef();
    def1->setManufacturer("Martin");
    def1->setModel("MAC250");
    cache.addFixtureDef(def1);

    QLCFixtureDef *def2 = new QLCFixtureDef();
    def2->setManufacturer("Martin");
    def2->setModel("MAC500");
    cache.addFixtureDef(def2);

    QLCFixtureDef *def3 = new QLCFixtureDef();
    def3->setManufacturer("Robe");
    def3->setModel("WL250");
    cache.addFixtureDef(def3);

    QLCFixtureDef *def4 = new QLCFixtureDef();
    def4->setManufacturer("Futurelight");
    def4->setModel("DJ Scan 250");
    cache.addFixtureDef(def4);

    QVERIFY(cache.fixtureDef("Martin", "MAC250") == def1);
    QVERIFY(cache.fixtureDef("Martin", "MAC500") == def2);
    QVERIFY(cache.fixtureDef("Robe", "WL250") == def3);
    QVERIFY(cache.fixtureDef("Futurelight", "DJ Scan 250") == def4);
    QVERIFY(cache.fixtureDef("Martin", "MAC 250") == NULL);
    QVERIFY(cache.fixtureDef("Mar tin", "MAC250") == NULL);
    QVERIFY(cache.fixtureDef("Foobar", "Foobar") == NULL);
    QVERIFY(cache.fixtureDef("", "") == NULL);
}

void QLCFixtureDefCache_Test::load()
{
    /* At least these should be available */
    QVERIFY(cache.manufacturers().contains("Elation") == true);
    QVERIFY(cache.manufacturers().contains("Eurolite") == true);
    QVERIFY(cache.manufacturers().contains("Futurelight") == true);
    QVERIFY(cache.manufacturers().contains("GLP") == true);
    QVERIFY(cache.manufacturers().contains("JB-Lighting") == true);
    QVERIFY(cache.manufacturers().contains("Lite-Works") == true);
    QVERIFY(cache.manufacturers().contains("Martin") == true);
    QVERIFY(cache.manufacturers().contains("Robe") == true);
    QVERIFY(cache.manufacturers().contains("SGM") == true);

    QString loadPath = QString("%1/Futurelight/Futurelight-CY-200.qxf").arg(INTERNAL_FIXTUREDIR);
    QVERIFY(cache.loadQXF(loadPath) == true);
    QVERIFY(cache.loadQXF("Foo/Baz.qxf") == false);
    QVERIFY(cache.loadD4("QLC/Plus.d4") == false);
}

void QLCFixtureDefCache_Test::defDirectories()
{
    QDir dir = QLCFixtureDefCache::systemDefinitionDirectory();

    QVERIFY(dir.filter() & QDir::Files);
    QVERIFY(dir.nameFilters().contains(QString("*%1").arg(KExtFixture)));
    QDir fxDir;
#if defined(__APPLE__) || defined(Q_OS_MAC)
    // QLCFile::systemDirectory() resolves this relative to the app bundle's
    // executable (Contents/MacOS/<app> -> ../Resources/...), not the process's
    // CWD — mirror that here rather than assuming a flat CWD-relative path
    // (which only holds on Linux/Windows, where systemDirectory() doesn't
    // consult applicationDirPath() at all).
    fxDir.setPath(QString("%1/../%2").arg(QCoreApplication::applicationDirPath())
                                      .arg(FIXTUREDIR));
#else
    fxDir.setPath(FIXTUREDIR);
#endif
    QCOMPARE(dir.absolutePath(), fxDir.absolutePath());

    dir = QLCFixtureDefCache::userDefinitionDirectory();
#ifndef SKIP_TEST
    QVERIFY(dir.exists() == true);
#endif
    QVERIFY(dir.filter() & QDir::Files);
    QVERIFY(dir.nameFilters().contains(QString("*%1").arg(KExtFixture)));
    QVERIFY(dir.absolutePath().contains(USERFIXTUREDIR));

}

void QLCFixtureDefCache_Test::storeDef()
{
    QLCFixtureDef *def = cache.fixtureDef("Futurelight", "CY-200");
    QFile defFile(def->definitionSourceFile());
    QVERIFY(defFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString defBuffer = defFile.readAll();
    defFile.close();
    QVERIFY(cache.storeFixtureDef("storeTest.qxf", defBuffer) == true);

    QDir dir = QLCFixtureDefCache::userDefinitionDirectory();
    QFile file (dir.absoluteFilePath("storeTest.qxf"));
    // Written whole (QSaveFile), as given.
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(file.readAll()), defBuffer);
    file.close();
    file.remove();
}

// Not APPLESS: defDirectories() exercises QLCFile::systemDirectory(), which
// calls QCoreApplication::applicationDirPath() on macOS/iOS/Windows — that
// needs a real QCoreApplication instance to resolve correctly (without one
// it warns and returns an empty path, failing the path comparison).
/* A definition edited while a show is open -- its lumens set from the
   footer's "declare no light output" -- is re-read in place: the new
   physical values, the same mode objects fixtures already point at. */
void QLCFixtureDefCache_Test::reloadPhysicalKeepsTheModes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("CY-200.qxf");
    QVERIFY(QFile::copy(QString("%1/Futurelight/Futurelight-CY-200.qxf").arg(INTERNAL_FIXTUREDIR), path));
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);

    QLCFixtureDefCache local;
    QVERIFY(local.loadQXF(path));
    QLCFixtureDef *def = local.fixtureDef("Futurelight", "CY-200");
    QVERIFY(def != NULL);
    QVERIFY(!def->modes().isEmpty());
    QLCFixtureMode *mode = def->modes().first();

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QString xml = QString::fromUtf8(f.readAll());
    f.close();
    xml.replace(QRegularExpression("Lumens=\"\\d*\""), "Lumens=\"1234\"");
    QVERIFY(xml.contains("Lumens=\"1234\""));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(xml.toUtf8());
    f.close();

    QVERIFY(local.reloadPhysical(def));
    QCOMPARE(def->modes().first(), mode);                  // the same mode object
    QCOMPARE(mode->physical().bulbLumens(), 1234);
    QVERIFY(mode->physical().declaresOutput());
}

/* A definition file that cannot be read is remembered, with why, for the
   footer -- and forgotten once it loads. */
void QLCFixtureDefCache_Test::aDefinitionThatFailsIsRemembered()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("Broken.qxf");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("<?xml version=\"1.0\"?>\n<FixtureDefinition><Manufacturer>X</Manu");
    f.close();

    QLCFixtureDefCache local;
    QVERIFY(!local.loadQXF(path));
    QVERIFY(local.failedDefinitions().contains(path));
    QVERIFY(!local.failedDefinitions().value(path).isEmpty());

    QFile::remove(path);
    QVERIFY(QFile::copy(QString("%1/Futurelight/Futurelight-CY-200.qxf").arg(INTERNAL_FIXTUREDIR), path));
    QVERIFY(local.loadQXF(path));
    QVERIFY(!local.failedDefinitions().contains(path));
}

/* The user's folder is read first: a local definition of a fixture that
   also ships with the app is the one used, and says it replaces it. */
void QLCFixtureDefCache_Test::aLocalDefinitionSaysItReplacesTheIncludedOne()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString included = QString("%1/Futurelight/Futurelight-CY-200.qxf").arg(INTERNAL_FIXTUREDIR);
    const QString path = dir.filePath("Futurelight-CY-200.qxf");
    QVERIFY(QFile::copy(included, path));

    QLCFixtureDefCache local;
    QVERIFY(local.loadQXF(path, true));                     // the user's copy
    QLCFixtureDef *def = local.definitions().first();
    QCOMPARE(def->source(), QLCFixtureDef::Local);
    QVERIFY(!def->overridesIncluded());
    QCOMPARE(def->sourceDescription(), QString("Local"));

    QVERIFY(local.loadQXF(included, false));                // then the included one
    QCOMPARE(local.definitions().size(), 1);                // set aside
    QVERIFY(def->overridesIncluded());
    QVERIFY(def->sourceDescription().contains("replaces the included one"));

    QLCFixtureDef other;
    QCOMPARE(other.source(), QLCFixtureDef::Included);
    other.setSource(QLCFixtureDef::Downloadable);
    QCOMPARE(other.source(), QLCFixtureDef::Downloadable);
    QVERIFY(!other.isUser());
    QCOMPARE(other.sourceDescription(), QString("Downloadable"));
}

/* Two local files for one fixture: the one changed last is used (it was
   alphabetical, so "X 2.qxf" beat "X.qxf" whichever was current), and the
   one set aside is recorded -- identical or not -- for the footer. */
void QLCFixtureDefCache_Test::ofTwoLocalCopiesTheNewestIsUsed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = QString("%1/Futurelight/Futurelight-CY-200.qxf").arg(INTERNAL_FIXTUREDIR);
    QFile in(source);
    QVERIFY(in.open(QIODevice::ReadOnly));
    const QByteArray xml = in.readAll();

    auto write = [&](const QString &name, const QByteArray &data, int ageSeconds) {
        const QString path = dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return path;
        f.write(data);
        f.close();
        if (!f.open(QIODevice::ReadWrite))
            return path;
        f.setFileTime(QDateTime::currentDateTime().addSecs(-ageSeconds), QFileDevice::FileModificationTime);
        f.close();
        return path;
    };
    QByteArray edited = xml;
    edited.replace("Lumens=\"0\"", "Lumens=\"777\"");
    const QString older = write("Futurelight-CY-200.qxf", xml, 3600);
    const QString newer = write("Futurelight-CY-200 2.qxf", edited, 10);
    write("Futurelight-CY-200 3.qxf", xml, 7200);                // same as the original

    QLCFixtureDefCache local;
    QVERIFY(local.load(QDir(dir.path(), "*.qxf")));
    QCOMPARE(local.definitions().size(), 1);
    QCOMPARE(local.definitions().first()->definitionSourceFile(), newer);

    const QList<QLCFixtureDefCache::Duplicate> dups = local.duplicates();
    QCOMPARE(dups.size(), 2);
    foreach (const QLCFixtureDefCache::Duplicate &d, dups)
    {
        QCOMPARE(d.kept, newer);
        QVERIFY(d.keptIsLocal && d.ignoredIsLocal);
        QVERIFY(!d.identical);                                   // newer was edited
    }
    QVERIFY(dups.at(0).ignored == older || dups.at(1).ignored == older);

    // A local one replacing an included one is recorded, not a local copy.
    local.loadQXF(source, false);                            // read fine, set aside
    QCOMPARE(local.duplicates().size(), 3);
    QVERIFY(!local.duplicates().last().ignoredIsLocal);
    QVERIFY(local.definitions().first()->overridesIncluded());
}

QTEST_GUILESS_MAIN(QLCFixtureDefCache_Test)

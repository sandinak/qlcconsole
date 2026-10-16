/*
  Q Light Controller Plus
  autosave_test.cpp

  Copyright (C) Massimo Callegari

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
#include <QSettings>
#include <QTemporaryDir>
#include <QScopeGuard>

#include "autosave_test.h"
#define private public
#define protected public
#include "app.h"
#undef protected
#undef private

#include "qlcconfig.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "doc.h"

void Autosave_Test::initTestCase()
{
    /* Never the user's: App's start-up merges the legacy user data into the
       user folder under HOME, and these tests change autosave settings and
       write recovery files. A home and an INI settings file of our own,
       before App exists. */
    QVERIFY(m_home.isValid());
    m_realHome = qgetenv("HOME");
    qputenv("HOME", m_home.path().toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_home.filePath("settings"));

    QSettings settings;
    settings.remove("workspace/autosave");

    m_app = new App();
    m_app->init();
}

void Autosave_Test::cleanupTestCase()
{
    delete m_app;
    m_app = nullptr;
    qputenv("HOME", m_realHome);
}

void Autosave_Test::defaultSettings()
{
    // Default autosave should be enabled with 5 minute interval
    QCOMPARE(m_app->isAutosaveEnabled(), true);
    QCOMPARE(m_app->autosaveInterval(), 5);
}

void Autosave_Test::enableDisable()
{
    // Disable autosave
    m_app->setAutosaveEnabled(false);
    QCOMPARE(m_app->isAutosaveEnabled(), false);

    // Verify it's saved in settings
    QSettings settings;
    QCOMPARE(settings.value("workspace/autosave/enabled").toBool(), false);

    // Re-enable autosave
    m_app->setAutosaveEnabled(true);
    QCOMPARE(m_app->isAutosaveEnabled(), true);
    QCOMPARE(settings.value("workspace/autosave/enabled").toBool(), true);
}

void Autosave_Test::intervalSettings()
{
    // Set a new interval
    m_app->setAutosaveInterval(10);
    QCOMPARE(m_app->autosaveInterval(), 10);

    // Verify it's saved in settings
    QSettings settings;
    QCOMPARE(settings.value("workspace/autosave/interval").toInt(), 10);

    // Test minimum interval (should be clamped to 1)
    m_app->setAutosaveInterval(0);
    QCOMPARE(m_app->autosaveInterval(), 1);

    // Restore default
    m_app->setAutosaveInterval(5);
}

void Autosave_Test::autosaveFilePath()
{
    // When no file is set, should use default location
    m_app->setFileName(QString());
    QString path = m_app->autosaveFilePath();

    // Should end with untitled.qxw.autosave
    QVERIFY(path.endsWith("untitled.qxw.autosave"));

    // Should contain the app's user data directory
    QVERIFY(path.contains(".qlcplus") || path.contains("QLC+") || path.contains("qlcconsole"));
}

void Autosave_Test::autosaveFilePathWithFileName()
{
    // When a file is set, autosave should be next to it
    QString testFile = "/tmp/test_workspace.qxw";
    m_app->setFileName(testFile);

    QString path = m_app->autosaveFilePath();
    QCOMPARE(path, QString("/tmp/test_workspace.qxw.autosave"));

    // Clean up
    m_app->setFileName(QString());
}

/* qlcconsole's own shows are .qlcc (.qxw is still read, for QLC+ shows):
   saved and opened again, everything is there. */
void Autosave_Test::aQlccShowSavesAndOpensAgain()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("DanceHysteria-test.qlcc");

    Doc *doc = m_app->doc();
    doc->clearContents();
    auto tidy = qScopeGuard([doc]() { doc->clearContents(); });
    for (int i = 0; i < 3; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("Step %1").arg(i + 1));
        f->setChannels(4);
        f->setAddress(quint32(i) * 4);
        QVERIFY(doc->addFixture(f));
    }
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName("US2f");
    g->setSize(QSize(3, 1));
    doc->addFixtureGroup(g);
    for (int i = 0; i < 3; ++i)
        g->assignFixture(quint32(i), QLCPoint(i, 0));
    const QMap<QLCPoint, GroupHead> heads = g->headsMap();
    const QSize size = g->size();

    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    QVERIFY(QFile::exists(path));
    QVERIFY(!QFile::exists(path + ".temp"));

    doc->clearContents();
    QCOMPARE(doc->fixtures().size(), 0);
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(doc->fixtures().size(), 3);
    QCOMPARE(doc->fixtureGroups().size(), 1);
    QCOMPARE(doc->fixtureGroups().first()->name(), QString("US2f"));
    QCOMPARE(doc->fixtureGroups().first()->size(), size);
    QCOMPARE(doc->fixtureGroups().first()->headsMap(), heads);
}

/* Opening a show that is not there (renamed .qxw -> .qlcc) is an error, and
   the show already open is left as it was -- not swapped for an empty one. */
void Autosave_Test::aMissingShowFileIsAnErrorNotAnEmptyShow()
{
    Doc *doc = m_app->doc();
    doc->clearContents();
    auto tidy = qScopeGuard([doc]() { doc->clearContents(); });
    Fixture *f = new Fixture(doc);
    f->setName("Kept");
    f->setChannels(1);
    QVERIFY(doc->addFixture(f));

    QTemporaryDir dir;
    const QString missing = dir.filePath("DanceHysteria-0.1.qxw");
    QVERIFY(!QFile::exists(missing));
    QVERIFY(m_app->loadXML(missing) != QFile::NoError);
    QCOMPARE(doc->fixtures().size(), 1);
    QVERIFY(m_app->fileName() != missing);
    QVERIFY(!QFile::exists(missing));       // nothing written in its place
}

QTEST_MAIN(Autosave_Test)


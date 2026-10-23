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
#include <QMessageBox>
#include <QAbstractButton>
#include <QApplication>

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
    m_realProfile = qgetenv("USERPROFILE");
    qputenv("HOME", m_home.path().toUtf8());
    qputenv("USERPROFILE", m_home.path().toUtf8());   // Windows' user folder
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
    if (m_realProfile.isEmpty())
        qunsetenv("USERPROFILE");
    else
        qputenv("USERPROFILE", m_realProfile);
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

/* Answer the next modal message box with @p button (it never shows: nobody
   is there to click it). */
static void answerNextMessageBox(QMessageBox::StandardButton button)
{
    QTimer::singleShot(0, [button]() {
        QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        QVERIFY2(box != nullptr, "no message box came up");
        if (box != nullptr)
            box->button(button)->click();
    });
}

static Fixture *addNamedFixture(Doc *doc, const QString &name, quint32 address)
{
    Fixture *f = new Fixture(doc);
    f->setName(name);
    f->setChannels(2);
    f->setAddress(address);
    return doc->addFixture(f) ? f : nullptr;
}

static QStringList fixtureNames(Doc *doc)
{
    QStringList names;
    for (Fixture *f : doc->fixtures())
        names << f->name();
    return names;
}

void Autosave_Test::aModifiedShowWritesARecoveryCopyThatLoadsBack()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString show = dir.filePath("Recovery.qxw");
    Doc *doc = m_app->doc();
    doc->clearContents();
    auto tidy = qScopeGuard([this, doc]() {
        doc->clearContents();
        doc->resetModified();
        m_app->setFileName(QString());
    });
    m_app->setFileName(show);
    const QString copy = m_app->autosaveFilePath();
    QCOMPARE(copy, show + ".autosave");

    // Nothing changed: nothing written.
    doc->resetModified();
    m_app->slotAutosave();
    QVERIFY(!QFile::exists(copy));

    // A change: written, beside the show, and the show itself left alone.
    QVERIFY(addNamedFixture(doc, "Added since the save", 0) != nullptr);
    QVERIFY(doc->isModified());
    m_app->slotAutosave();
    QVERIFY(QFile::exists(copy));
    QVERIFY(!QFile::exists(copy + ".temp"));
    QVERIFY(!QFile::exists(show));
    QCOMPARE(m_app->fileName(), show);
    QVERIFY2(doc->isModified(), "a recovery copy is not a save");

    // ...and it holds the change.
    doc->clearContents();
    QCOMPARE(doc->fixtures().size(), 0);
    QCOMPARE(m_app->loadXML(copy), QFile::NoError);
    QCOMPARE(fixtureNames(doc), QStringList{ "Added since the save" });

    // Unmodified again: the old copy is not rewritten.
    m_app->setFileName(show);
    doc->resetModified();
    QVERIFY(QFile::remove(copy));
    m_app->slotAutosave();
    QVERIFY(!QFile::exists(copy));
}

void Autosave_Test::anUntitledShowsRecoveryCopyIsWhereStartUpLooks()
{
    Doc *doc = m_app->doc();
    doc->clearContents();
    m_app->setFileName(QString());
    const QString copy = m_app->autosaveFilePath();
    auto tidy = qScopeGuard([doc, copy]() {
        QFile::remove(copy);
        doc->clearContents();
        doc->resetModified();
    });
    // In this test's own home, never the user's.
    QVERIFY2(copy.startsWith(m_home.path()), qPrintable(copy));
    QFile::remove(copy);

    QVERIFY(addNamedFixture(doc, "Never saved", 0) != nullptr);
    m_app->slotAutosave();
    QVERIFY(QFile::exists(copy));
    QCOMPARE(QFileInfo(copy).fileName(), QString("untitled.qxw.autosave"));

    doc->clearContents();
    QCOMPARE(m_app->loadXML(copy), QFile::NoError);
    QCOMPARE(fixtureNames(doc), QStringList{ "Never saved" });
}

void Autosave_Test::aRecoveryFileIsKeptWhenThereIsNobodyToAsk()
{
    /* The prompt itself cannot be reached from here: checkAutosaveRecovery()
       returns before asking on the offscreen platform, whatever the
       environment says, so recovering (load, then Save As) is untested. */
    QVERIFY(QGuiApplication::platformName() == QLatin1String("offscreen")
            || qEnvironmentVariableIsSet("QLC_NO_RECOVERY_PROMPT"));

    Doc *doc = m_app->doc();
    doc->clearContents();
    m_app->setFileName(QString());
    const QString copy = m_app->autosaveFilePath();
    auto tidy = qScopeGuard([doc, copy]() {
        QFile::remove(copy);
        doc->clearContents();
        doc->resetModified();
    });
    QVERIFY(addNamedFixture(doc, "From last time", 0) != nullptr);
    m_app->slotAutosave();
    QVERIFY(QFile::exists(copy));
    const QByteArray before = [&copy]() {
        QFile f(copy);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }();
    QVERIFY(!before.isEmpty());

    // This session's show, then the start-up check.
    doc->clearContents();
    QVERIFY(addNamedFixture(doc, "Open now", 10) != nullptr);
    doc->resetModified();
    m_app->checkAutosaveRecovery();

    QCOMPARE(fixtureNames(doc), QStringList{ "Open now" });     // not swapped
    QVERIFY(!doc->isModified());
    QVERIFY2(QFile::exists(copy), "kept for a session that can ask");
    QFile f(copy);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), before);
}

void Autosave_Test::savingOrDiscardingRemovesTheRecoveryCopy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString show = dir.filePath("Saved.qxw");
    Doc *doc = m_app->doc();
    doc->clearContents();
    auto tidy = qScopeGuard([this, doc]() {
        doc->clearContents();
        doc->resetModified();
        m_app->setFileName(QString());
    });
    m_app->setFileName(show);
    const QString copy = m_app->autosaveFilePath();

    // File ▸ Save: the show written, the copy gone.
    QVERIFY(addNamedFixture(doc, "Saved", 0) != nullptr);
    m_app->slotAutosave();
    QVERIFY(QFile::exists(copy));
    QCOMPARE(m_app->slotFileSave(), QFile::NoError);
    QVERIFY(QFile::exists(show));
    QVERIFY(!doc->isModified());
    QVERIFY2(!QFile::exists(copy), "a saved show keeps no recovery copy");

    // Changed again, then "Don't save" when asked: discarded on purpose, so
    // it is not offered back at the next start either.
    QVERIFY(addNamedFixture(doc, "Thrown away", 10) != nullptr);
    m_app->slotAutosave();
    QVERIFY(QFile::exists(copy));
    answerNextMessageBox(QMessageBox::No);
    QVERIFY(m_app->saveModifiedDoc("Close", "Save the show?"));
    QVERIFY2(!QFile::exists(copy), "a discarded show keeps no recovery copy");

    // Cancel keeps it: nothing was decided.
    m_app->slotAutosave();
    QVERIFY(QFile::exists(copy));
    answerNextMessageBox(QMessageBox::Cancel);
    QVERIFY(!m_app->saveModifiedDoc("Close", "Save the show?"));
    QVERIFY(QFile::exists(copy));
    m_app->removeAutosaveFile();
    QVERIFY(!QFile::exists(copy));
}

QTEST_MAIN(Autosave_Test)


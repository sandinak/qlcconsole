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
#include <QXmlStreamWriter>
#include <QLabel>
#include <QScopeGuard>
#include <QMessageBox>
#include <QAbstractButton>
#include <QApplication>
#include <QFileDialog>
#include <QPushButton>
#include <QPainter>
#include <functional>
#include <memory>

#include "autosave_test.h"
#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#undef protected
#undef private

#include "qlcconfig.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "doc.h"
#include "startupwindow.h"

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

/* wfaudit 2026-11-03: a save or autosave that can't be written whole never
   replaces the file it was to replace, and says so: the status chip for an
   autosave, the error (naming the file) for a save. */
void Autosave_Test::aSaveThatCannotWriteKeepsTheShowAndSaysSo()
{
    // A writer whose writes don't reach the file: not "written whole".
    {
        QTemporaryDir tmp;
        QFile ro(tmp.filePath("ro.xml"));
        QVERIFY(ro.open(QIODevice::WriteOnly));
        ro.write("x");
        ro.close();
        QVERIFY(ro.open(QIODevice::ReadOnly));
        QXmlStreamWriter w(&ro);
        w.writeStartDocument();
        w.writeTextElement("A", QString(64, QLatin1Char('a')));
        w.writeEndDocument();
        QVERIFY(!App::writtenWhole(w, &ro));
        ro.close();
        QFile ok(tmp.filePath("ok.xml"));
        QVERIFY(ok.open(QIODevice::WriteOnly));
        QXmlStreamWriter w2(&ok);
        w2.writeStartDocument();
        w2.writeTextElement("A", "b");
        w2.writeEndDocument();
        QVERIFY(App::writtenWhole(w2, &ok));
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString folder = dir.filePath("locked");
    QVERIFY(QDir().mkpath(folder));
    const QString show = folder + "/Kept.qxw";
    Doc *doc = m_app->doc();
    doc->clearContents();
    auto tidy = qScopeGuard([this, doc, folder]() {
        QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        doc->clearContents();
        doc->resetModified();
        m_app->setFileName(QString());
    });
    QVERIFY(addNamedFixture(doc, "First", 0) != nullptr);
    QCOMPARE(m_app->saveXML(show), QFile::NoError);
    const QByteArray before = [&]() {
        QFile f(show);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }();
    QVERIFY(!before.isEmpty());

    // The folder can't be written any more (a locked share, a full volume).
    QVERIFY(QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    {
        QFile probe(folder + "/probe");
        if (probe.open(QIODevice::WriteOnly))
        {
            probe.close();
            probe.remove();
            QSKIP("this account writes into a read-only folder (root): nothing to test");
        }
    }
    QVERIFY(addNamedFixture(doc, "Second", 10) != nullptr);
    QVERIFY(m_app->saveXML(show) != QFile::NoError);
    {
        QFile f(show);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), before);          // the show on disk as it was
    }
    QVERIFY(doc->isModified());

    // The autosave beside it can't be written either: the chip says so.
    m_app->setFileName(show);
    m_app->slotAutosave();
    QVERIFY(m_app->m_statusDirtyLabel != nullptr);
    QCOMPARE(m_app->m_statusDirtyLabel->text(), QString("⚠ Autosave failed"));
    QVERIFY(m_app->m_statusDirtyLabel->toolTip().contains("Save the show"));
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

/*****************************************************************************
 * A saved show's autosave, offered when the show is opened
 *****************************************************************************/

/* Each time a modal window is up, hand it to @p act until act says it is
   done (after a minute of wall-clock time -- not a tick count, which a loaded
   machine stretches -- it closes what is up and stops, so a missing prompt
   fails, not hangs). */
static void onModal(std::function<bool(QWidget *)> act)
{
    QTimer *t = new QTimer;
    t->setInterval(10);
    auto clock = std::make_shared<QElapsedTimer>();
    clock->start();
    QObject::connect(t, &QTimer::timeout, t, [t, act, clock]() {
        QWidget *w = QApplication::activeModalWidget();
        // Not again from inside act() (a shot pumps events)
        t->stop();
        if (w != nullptr && act(w))
        {
            t->deleteLater();
        }
        else if (clock->hasExpired(60000))
        {
            // Never a hang: whatever is still up is closed, and the test
            // fails on what it then finds.
            if (QDialog *d = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
                d->reject();
            t->deleteLater();
        }
        else
        {
            t->start();
        }
    });
    t->start();
}

static QByteArray contents(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

static void setModified(const QString &path, const QDateTime &when)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadWrite));
    QVERIFY(f.setFileTime(when, QFileDevice::FileModificationTime));
    f.close();
}

/* QLC_AUTOSAVE_SHOTS (not QLC_SHOT_DIR: App runs a shot scenario of its
   own after a load when that is set): @p prompt over @p behind (or alone on a plain "desktop"),
   at the three sizes, as autosave-<name>-<size>.png */
static void shootPrompt(App *app, QWidget *prompt, const QString &name, bool onTheApp)
{
    const QString dir = qEnvironmentVariable("QLC_AUTOSAVE_SHOTS");
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    const QList<QPair<QSize, QString> > sizes = { { QSize(1024, 640), "1024x640" },
                                                  { QSize(1280, 800), "1280x800" },
                                                  { QSize(1920, 1080), "1920x1080" } };
    for (const auto &sz : sizes)
    {
        QPixmap shot(sz.first);
        shot.fill(QColor(60, 60, 64));
        if (onTheApp)
        {
            app->resize(sz.first);
            for (int i = 0; i < 5; ++i)
                QCoreApplication::processEvents();
            shot = app->grab();
        }
        prompt->adjustSize();
        for (int i = 0; i < 5; ++i)
            QCoreApplication::processEvents();
        const QPixmap box = prompt->grab();
        QPainter p(&shot);
        p.drawPixmap((shot.width() - box.width()) / 2, (shot.height() - box.height()) / 3, box);
        p.end();
        QVERIFY(shot.save(QStringLiteral("%1/autosave-%2-%3.png").arg(dir, name, sz.second)));
    }
}

/* A saved show of one fixture, "Saved", and an autosave a minute newer that
   also has "Since the save". The doc is left empty and unmodified. */
static void makeShowWithNewerAutosave(App *app, const QString &show)
{
    Doc *doc = app->doc();
    doc->clearContents();
    QVERIFY(addNamedFixture(doc, "Saved", 0) != nullptr);
    QCOMPARE(app->saveXML(show), QFile::NoError);
    QVERIFY(addNamedFixture(doc, "Since the save", 10) != nullptr);
    QVERIFY(doc->isModified());
    app->slotAutosave();
    QVERIFY(QFile::exists(show + ".autosave"));
    const QDateTime now = QDateTime::currentDateTime();
    setModified(show, now.addSecs(-120));
    setModified(show + ".autosave", now.addSecs(-60));
    doc->clearContents();
    doc->resetModified();
    app->setFileName(QString());
}

static QAction *recentAction(const QString &path)
{
    static QAction action;
    action.setText(path);
    return &action;
}

void Autosave_Test::aSavedShowsNewerAutosaveIsOfferedAndRecovered()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString show = dir.filePath("Recover.qlcc");
    Doc *doc = m_app->doc();
    App::s_askRecoveryOffscreen = true;
    auto tidy = qScopeGuard([this, doc]() {
        App::s_askRecoveryOffscreen = false;
        doc->clearContents();
        doc->resetModified();
        m_app->setFileName(QString());
    });
    makeShowWithNewerAutosave(m_app, show);
    const QByteArray copy = contents(show + ".autosave");

    QString asked;
    onModal([&asked](QWidget *w) {
        QMessageBox *box = qobject_cast<QMessageBox *>(w);
        if (box == nullptr || box->objectName() != QLatin1String("RecoverAutosave"))
            return false;
        asked = box->windowTitle() + "|" + box->text() + "|" + box->informativeText();
        box->findChild<QPushButton *>("Recover")->click();
        return true;
    });
    m_app->slotRecentFileClicked(recentAction(show));

    // A message box has no title on a Mac (Qt leaves it out there)
#ifndef Q_OS_MACOS
    QVERIFY2(asked.startsWith("Recover Autosave|"), qPrintable(asked));
#endif
    QVERIFY2(asked.contains("|Recover the autosave from "), qPrintable(asked));
    QVERIFY2(asked.contains("Recover.qlcc has changes from"), qPrintable(asked));
    QVERIFY(asked.contains("Open the Saved Show keeps the autosave beside it as Recover.qlcc.autosave-"));
    QCOMPARE(fixtureNames(doc), (QStringList{ "Saved", "Since the save" }));
    QCOMPARE(m_app->fileName(), show);                  // the show, not the copy
    QVERIFY2(doc->isModified(), "recovered work is not saved yet");
    QCOMPARE(contents(show + ".autosave"), copy);       // kept until the save

    // Save writes the show itself, and the copy goes.
    QCOMPARE(m_app->slotFileSave(), QFile::NoError);
    QVERIFY(!doc->isModified());
    QVERIFY(!QFile::exists(show + ".autosave"));
    doc->clearContents();
    QCOMPARE(m_app->loadXML(show), QFile::NoError);
    QCOMPARE(fixtureNames(doc), (QStringList{ "Saved", "Since the save" }));
}

void Autosave_Test::openTheSavedShowKeepsTheAutosaveAside()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString show = dir.filePath("Aside.qlcc");
    Doc *doc = m_app->doc();
    App::s_askRecoveryOffscreen = true;
    auto tidy = qScopeGuard([this, doc]() {
        App::s_askRecoveryOffscreen = false;
        doc->clearContents();
        doc->resetModified();
        m_app->setFileName(QString());
    });
    makeShowWithNewerAutosave(m_app, show);
    const QByteArray copy = contents(show + ".autosave");
    const QDateTime when = QFileInfo(show + ".autosave").lastModified();
    const QString kept = App::keptAutosaveName(show, when);
    QCOMPARE(QFileInfo(kept).fileName(),
             "Aside.qlcc.autosave-" + when.toString("yyyyMMdd-HHmmss"));

    // The show open now, as File ▸ Open finds it
    QVERIFY(addNamedFixture(doc, "Open now", 0) != nullptr);
    doc->resetModified();

    // File ▸ Open: the file chosen, then the question.
    bool chosen = false;
    bool asked = false;
    onModal([&](QWidget *w) {
        if (QFileDialog *fd = qobject_cast<QFileDialog *>(w))
        {
            // The dialog reads its folder in the background, and an accept
            // before it holds our file takes whatever else it holds (under
            // load, a different file with no autosave beside it). So accept
            // only once it reports the show as the selection, else try
            // again on the next tick.
            fd->setDirectory(QFileInfo(show).absolutePath());
            fd->selectFile(show);
            // Compared as canonical paths: a temp folder under /var is
            // /private/var to the dialog once it has read it.
            const QString picked = fd->selectedFiles().value(0);
            if (picked.isEmpty()
                || QFileInfo(picked).canonicalFilePath() != QFileInfo(show).canonicalFilePath())
                return false;
            chosen = true;
            static_cast<QDialog *>(fd)->accept();
            return false;
        }
        QMessageBox *box = qobject_cast<QMessageBox *>(w);
        if (box == nullptr || box->objectName() != QLatin1String("RecoverAutosave"))
            return false;
        asked = true;
        shootPrompt(m_app, box, "file-open", true);
        box->findChild<QPushButton *>("OpenSavedShow")->click();
        return true;
    });
    QCOMPARE(m_app->slotFileOpen(), QFile::NoError);

    QVERIFY(chosen);
    QVERIFY(asked);
    QCOMPARE(fixtureNames(doc), QStringList{ "Saved" });
    QCOMPARE(m_app->fileName(), show);
    QVERIFY(!doc->isModified());
    QVERIFY2(!QFile::exists(show + ".autosave"), "moved aside, not written over");
    QVERIFY(QFile::exists(kept));
    QCOMPARE(contents(kept), copy);
}

void Autosave_Test::atStartUpItIsAskedInTheStartUpWindow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString show = dir.filePath("StartUp.qlcc");
    Doc *doc = m_app->doc();
    App::s_askRecoveryOffscreen = true;
    StartupWindow *window = new StartupWindow(3);
    auto tidy = qScopeGuard([this, doc, window]() {
        App::s_askRecoveryOffscreen = false;
        m_app->m_startupWindow = nullptr;
        delete window;
        doc->clearContents();
        doc->resetModified();
        m_app->setFileName(QString());
    });
    makeShowWithNewerAutosave(m_app, show);
    window->beginStep("Loading");
    m_app->m_startupWindow = window;

    // Inline: buttons in the start-up window, no second window.
    QStringList offered;
    QTimer poll;
    poll.setInterval(10);
    connect(&poll, &QTimer::timeout, this, [&]() {
        QWidget *row = window->findChild<QWidget *>("StartupQuestion");
        if (row == nullptr)
            return;
        poll.stop();
        QVERIFY(qobject_cast<QMessageBox *>(QApplication::activeModalWidget()) == nullptr);
        for (QPushButton *b : row->findChildren<QPushButton *>())
            offered << b->text();
        QLabel *label = row->findChild<QLabel *>();
        QVERIFY(label != nullptr && label->text().startsWith("Recover the autosave from "));
        shootPrompt(m_app, window, "startup", false);
        for (QPushButton *b : row->findChildren<QPushButton *>())
            if (b->text() == QLatin1String("Recover"))
                b->click();
    });
    poll.start();
    const App::RecoveryChoice choice = m_app->askAboutAutosave(show);
    QCOMPARE(offered, (QStringList{ "Recover", "Open the Saved Show", "Cancel" }));
    QCOMPARE(choice, App::RecoverAutosave);
    QCOMPARE(m_app->loadShowFile(show, choice), QFile::NoError);
    QCOMPARE(fixtureNames(doc), (QStringList{ "Saved", "Since the save" }));
    QCOMPARE(m_app->fileName(), show);
    QVERIFY(doc->isModified());
}

void Autosave_Test::cancelOpensNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString show = dir.filePath("Cancel.qlcc");
    const QString open = dir.filePath("Open.qlcc");
    Doc *doc = m_app->doc();
    App::s_askRecoveryOffscreen = true;
    auto tidy = qScopeGuard([this, doc]() {
        App::s_askRecoveryOffscreen = false;
        doc->clearContents();
        doc->resetModified();
        m_app->setFileName(QString());
    });
    makeShowWithNewerAutosave(m_app, show);
    const QByteArray copy = contents(show + ".autosave");
    const QDateTime stamp = QFileInfo(show + ".autosave").lastModified();
    QVERIFY(addNamedFixture(doc, "Open now", 0) != nullptr);
    QCOMPARE(m_app->saveXML(open), QFile::NoError);
    QCOMPARE(m_app->fileName(), open);

    onModal([](QWidget *w) {
        QMessageBox *box = qobject_cast<QMessageBox *>(w);
        if (box == nullptr || box->objectName() != QLatin1String("RecoverAutosave"))
            return false;
        box->findChild<QPushButton *>("Cancel")->click();
        return true;
    });
    m_app->slotRecentFileClicked(recentAction(show));

    QCOMPARE(fixtureNames(doc), QStringList{ "Open now" });
    QCOMPARE(m_app->fileName(), open);
    QCOMPARE(contents(show + ".autosave"), copy);
    QCOMPARE(QFileInfo(show + ".autosave").lastModified(), stamp);
    QCOMPARE(QDir(dir.path()).entryList(QStringList() << "Cancel.qlcc.autosave-*").size(), 0);
}

void Autosave_Test::anOlderOrSameAutosaveIsNotOffered()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString show = dir.filePath("Older.qlcc");
    Doc *doc = m_app->doc();
    App::s_askRecoveryOffscreen = true;
    auto tidy = qScopeGuard([this, doc]() {
        App::s_askRecoveryOffscreen = false;
        doc->clearContents();
        doc->resetModified();
        m_app->setFileName(QString());
    });
    // Any box that came up would be a failure: say so and close it.
    bool boxed = false;
    QTimer watch;
    watch.setInterval(10);
    connect(&watch, &QTimer::timeout, this, [&boxed]() {
        if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
        {
            boxed = true;
            box->reject();
        }
    });
    watch.start();

    // Older than the show: saved since, not offered, left alone.
    makeShowWithNewerAutosave(m_app, show);
    const QDateTime now = QDateTime::currentDateTime();
    setModified(show + ".autosave", now.addSecs(-300));
    const QByteArray copy = contents(show + ".autosave");
    QCOMPARE(App::pendingAutosave(show), QString());
    QCOMPARE(m_app->askAboutAutosave(show), App::NoRecovery);
    QCOMPARE(contents(show + ".autosave"), copy);

    // Newer but the same as the show: nothing to recover, removed.
    QVERIFY(QFile::remove(show + ".autosave"));
    QVERIFY(QFile::copy(show, show + ".autosave"));
    setModified(show + ".autosave", now.addSecs(60));
    QCOMPARE(m_app->askAboutAutosave(show), App::NoRecovery);
    QVERIFY(!QFile::exists(show + ".autosave"));

    // None at all, and none for an untitled show
    QCOMPARE(m_app->askAboutAutosave(show), App::NoRecovery);
    QCOMPARE(m_app->askAboutAutosave(QString()), App::NoRecovery);

    for (int i = 0; i < 5; ++i)
        QCoreApplication::processEvents();
    QVERIFY(!boxed);
}

void Autosave_Test::theAutosaveIsNotWrittenOverBeforeTheChoice()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString show = dir.filePath("Held.qlcc");
    const QString other = dir.filePath("Other.qlcc");
    Doc *doc = m_app->doc();
    auto tidy = qScopeGuard([this, doc]() {
        App::s_askRecoveryOffscreen = false;
        m_app->m_heldAutosave.clear();
        doc->clearContents();
        doc->resetModified();
        m_app->setFileName(QString());
    });
    makeShowWithNewerAutosave(m_app, show);
    const QByteArray copy = contents(show + ".autosave");

    // While the question is open: no autosave is written, of any show.
    App::s_askRecoveryOffscreen = true;
    QVERIFY(addNamedFixture(doc, "Edited while asked", 0) != nullptr);
    m_app->setFileName(show);
    bool keptWhileAsked = false;
    onModal([this, &show, &copy, &keptWhileAsked](QWidget *w) {
        QMessageBox *box = qobject_cast<QMessageBox *>(w);
        if (box == nullptr || box->objectName() != QLatin1String("RecoverAutosave"))
            return false;
        m_app->slotAutosave();
        m_app->slotDocAutosave();
        m_app->removeAutosaveFile();
        keptWhileAsked = contents(show + ".autosave") == copy;
        box->findChild<QPushButton *>("Cancel")->click();
        return true;
    });
    QCOMPARE(m_app->askAboutAutosave(show), App::CancelOpen);
    QVERIFY(keptWhileAsked);
    QCOMPARE(contents(show + ".autosave"), copy);

    // Nobody to ask: the show opens, and its autosave is neither offered,
    // written over nor removed this session.
    App::s_askRecoveryOffscreen = false;
    doc->clearContents();
    doc->resetModified();
    QCOMPARE(m_app->askAboutAutosave(show), App::NoRecovery);
    QCOMPARE(m_app->loadShowFile(show, App::NoRecovery), QFile::NoError);
    QCOMPARE(fixtureNames(doc), QStringList{ "Saved" });
    QVERIFY(addNamedFixture(doc, "Edited here", 20) != nullptr);
    m_app->slotAutosave();
    m_app->slotDocAutosave();
    QCOMPARE(contents(show + ".autosave"), copy);
    m_app->removeAutosaveFile();
    QVERIFY(QFile::exists(show + ".autosave"));

    // Another show is not held: its autosave is written as always.
    m_app->setFileName(other);
    m_app->slotAutosave();
    QVERIFY(QFile::exists(other + ".autosave"));
}

void Autosave_Test::aWebLoadThatIsNotAShowKeepsTheShow()
{
    Doc *doc = m_app->doc();
    doc->clearContents();
    auto tidy = qScopeGuard([doc]() { doc->clearContents(); });
    Fixture *f = new Fixture(doc);
    f->setName("Kept");
    f->setChannels(1);
    QVERIFY(doc->addFixture(f));

    m_app->slotLoadDocFromMemory(QString());
    QCOMPARE(doc->fixtures().size(), 1);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("doesn't have a Workspace tag"));
    m_app->slotLoadDocFromMemory("<!DOCTYPE html><html><body>not a show</body></html>");
    QCOMPARE(doc->fixtures().size(), 1);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("doesn't have a Workspace tag"));
    m_app->slotLoadDocFromMemory("<?xml version=\"1.0\"?><Workspace><Engine/></Workspace>");   // no DTD
    QCOMPARE(doc->fixtures().size(), 1);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("XML has errors"));
    m_app->slotLoadDocFromMemory("<?xml version=\"1.0\"?><!DOCTYPE Workspace<<<");
    QCOMPARE(doc->fixtures().size(), 1);
    QCOMPARE(doc->fixtures().first()->name(), QString("Kept"));
    QVERIFY(!doc->loadedFromWeb());
}

void Autosave_Test::aWebLoadReplacesTheShowAndIsMarkedFromTheWeb()
{
    Doc *doc = m_app->doc();
    doc->clearContents();
    auto tidy = qScopeGuard([doc]() { doc->clearContents(); doc->setLoadedFromWeb(false); });

    // A show of three fixtures, as a file
    QTemporaryDir dir;
    const QString path = dir.filePath("Web.qxw");
    for (int i = 0; i < 3; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("Web %1").arg(i + 1));
        f->setChannels(2);
        f->setAddress(quint32(i) * 2);
        QVERIFY(doc->addFixture(f));
    }
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString xml = QString::fromUtf8(file.readAll());
    file.close();

    // The open show is something else
    doc->clearContents();
    Fixture *other = new Fixture(doc);
    other->setName("Before");
    other->setChannels(1);
    QVERIFY(doc->addFixture(other));

    m_app->slotLoadDocFromMemory(xml);
    QCOMPARE(doc->fixtures().size(), 3);
    QCOMPARE(doc->fixtures().first()->name(), QString("Web 1"));
    QVERIFY(doc->loadedFromWeb());                  // its scripts run no programs

    // Opened from a file, or a new show: the operator's again
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QVERIFY(!doc->loadedFromWeb());
    m_app->slotLoadDocFromMemory(xml);
    QVERIFY(doc->loadedFromWeb());
    m_app->clearDocument();
    QVERIFY(!doc->loadedFromWeb());
}

QTEST_MAIN(Autosave_Test)


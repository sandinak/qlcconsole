/*
  Q Light Controller - Unit tests
  qlcfile_test.cpp

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
#include <QXmlStreamReader>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#if defined(WIN32) || defined(Q_OS_WIN)
#else
#   include <sys/types.h>
#   include <sys/stat.h>
#endif

#include "qlcfile_test.h"
#include "qlcfile.h"
#include "qlcconfig.h"
#include "effectscriptcache.h"
#include "effectpresetcache.h"
#include "bundlecache.h"
#include "headlayout.h"
#include "gobolibrary.h"
#include "networkgearcatalog.h"

void QLCFile_Test::XMLReader()
{
    QVERIFY(QLCFile::getXMLReader("") == NULL);

    QXmlStreamReader *reader = QLCFile::getXMLReader("readonly.xml.in");

    QVERIFY(reader != NULL);
    QVERIFY(reader->device() != NULL);
    QVERIFY(reader->device()->isOpen() == true);

    /* check releasing an invalid reader does nothing */
    QLCFile::releaseXMLReader(NULL);

    /* release the real reader */
    QLCFile::releaseXMLReader(reader);
    reader = NULL;

    /* check a file that cannot be created */
    reader = QLCFile::getXMLReader("foo.xml");
    QVERIFY(reader == NULL);
}

void QLCFile_Test::getXMLHeader()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter doc(&buffer);
    doc.setAutoFormatting(true);
    doc.setAutoFormattingIndent(1);

    QLCFile::writeXMLHeader(&doc, "DocumentTag", "TestUnit");
    doc.writeEndDocument();

    doc.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);

    while (!xmlReader.atEnd())
    {
        if (xmlReader.readNext() == QXmlStreamReader::DTD)
            break;
    }

    bool creatorTag = false, author = false, appname = false,
         appversion = false;

    QVERIFY(xmlReader.hasError() == false);
    QVERIFY(xmlReader.dtdName().toString() == "DocumentTag");

    QVERIFY(xmlReader.readNextStartElement() == true);

    QVERIFY(xmlReader.name().toString() == "DocumentTag");

    while (xmlReader.readNextStartElement())
    {
        if (xmlReader.name() == KXMLQLCCreatorAuthor)
        {
            author = true;
            QVERIFY(xmlReader.readElementText() == "TestUnit");
        }
        else if (xmlReader.name() == KXMLQLCCreator)
            creatorTag = true;
        else if (xmlReader.name() == KXMLQLCCreatorName)
        {
            appname = true;
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name() == KXMLQLCCreatorVersion)
        {
            appversion = true;
            xmlReader.skipCurrentElement();
        }
    }

    QVERIFY(creatorTag == true);
    QVERIFY(appname == true);
    QVERIFY(appversion == true);
    QVERIFY(author == true);
}

void QLCFile_Test::errorString()
{
    QCOMPARE(QLCFile::errorString(QFile::NoError),
             tr("No error occurred."));
    QCOMPARE(QLCFile::errorString(QFile::ReadError),
             tr("An error occurred when reading from the file."));
    QCOMPARE(QLCFile::errorString(QFile::WriteError),
             tr("An error occurred when writing to the file."));
    QCOMPARE(QLCFile::errorString(QFile::FatalError),
             tr("A fatal error occurred."));
    QCOMPARE(QLCFile::errorString(QFile::ResourceError),
             tr("Resource error occurred."));
    QCOMPARE(QLCFile::errorString(QFile::OpenError),
             tr("The file could not be opened."));
    QCOMPARE(QLCFile::errorString(QFile::AbortError),
             tr("The operation was aborted."));
    QCOMPARE(QLCFile::errorString(QFile::TimeOutError),
             tr("A timeout occurred."));
    QCOMPARE(QLCFile::errorString(QFile::UnspecifiedError),
             tr("An unspecified error occurred."));
    QCOMPARE(QLCFile::errorString(QFile::RemoveError),
             tr("The file could not be removed."));
    QCOMPARE(QLCFile::errorString(QFile::RenameError),
             tr("The file could not be renamed."));
    QCOMPARE(QLCFile::errorString(QFile::PositionError),
             tr("The position in the file could not be changed."));
    QCOMPARE(QLCFile::errorString(QFile::ResizeError),
             tr("The file could not be resized."));
    QCOMPARE(QLCFile::errorString(QFile::PermissionsError),
             tr("The file could not be accessed."));
    QCOMPARE(QLCFile::errorString(QFile::CopyError),
             tr("The file could not be copied."));
    // 15: no error of Qt's, though a value a FileError can hold (0..15).
    QCOMPARE(QLCFile::errorString(QFile::FileError(15)),
             tr("An unknown error occurred."));
}

void QLCFile_Test::version()
{
    QVERIFY(QLCFile::getQtRuntimeVersion() > 40000);
}

void QLCFile_Test::windowManager()
{
    QVERIFY(QLCFile::hasWindowManager() == true);

    QLCFile::setHasWindowManager(false);
    QVERIFY(QLCFile::hasWindowManager() == false);
}

void QLCFile_Test::onlineOnlyFilesAreNotedNotWaitedOn()
{
    // An ordinary file, and one that is not there: neither is online-only.
    QTemporaryDir dir;
    const QString here = dir.filePath("here.qxf");
    QFile f(here);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("<FixtureDefinition/>");
    f.close();
    QVERIFY(!QLCFile::isOnlineOnly(here));
    QVERIFY(!QLCFile::isOnlineOnly(dir.filePath("not-there.qxf")));

    // What a loader skipped is listed once, and forgotten when fetched.
    QLCFile::forgetSkippedOnlineOnly();
    QLCFile::noteSkippedOnlineOnly("/cloud/Fixtures/a.qxf");
    QLCFile::noteSkippedOnlineOnly("/cloud/RGBScripts/fire.js");
    QLCFile::noteSkippedOnlineOnly("/cloud/Fixtures/a.qxf");
    QCOMPARE(QLCFile::skippedOnlineOnly(), QStringList() << "/cloud/Fixtures/a.qxf" << "/cloud/RGBScripts/fire.js");
    QLCFile::forgetSkippedOnlineOnly("/cloud/Fixtures/a.qxf");
    QCOMPARE(QLCFile::skippedOnlineOnly(), QStringList() << "/cloud/RGBScripts/fire.js");
    QLCFile::forgetSkippedOnlineOnly();
    QVERIFY(QLCFile::skippedOnlineOnly().isEmpty());

    /* Where this machine has a real online-only file (a cloud folder not
       pinned), it is seen as one -- by its flags, without reading it. */
    const QString probe = qEnvironmentVariable("QLC_ONLINE_ONLY_PROBE");
    if (!probe.isEmpty())
        QVERIFY2(QLCFile::isOnlineOnly(probe), qPrintable(probe));
}

/* The old QLC+ folder's files are linked into qlcconsole's at every start
   -- but one removed here stays removed (four malformed definitions came
   back after being deleted). New files there still come across. */
void QLCFile_Test::userFoldersAreThePlatformsUserFolder()
{
#if defined(WIN32) || defined(Q_OS_WIN)
    QSKIP("the user folder comes from %USERPROFILE% there, not HOME");
#else
    QTemporaryDir home;
    const QByteArray realHome = qgetenv("HOME");
    qputenv("HOME", home.path().toUtf8());
    const QString root = QDir(home.path()).absoluteFilePath(QString(USERQLCPLUSDIR));
    auto under = [&](const QDir &dir, const QString &sub) {
        return QDir::cleanPath(dir.absolutePath()) == QDir::cleanPath(root + "/" + sub);
    };
    QVERIFY(under(EffectScriptCache::userScriptsDirectory(), "EffectScripts"));
    QVERIFY(under(EffectPresetCache::userPresetsDirectory(), "EffectPresets"));
    QVERIFY(under(BundleCache::userBundlesDirectory(), "Bundles"));
    QVERIFY(under(HeadLayoutLibrary::userDirectory(), "HeadLayouts"));
    QVERIFY(under(GoboLibrary::userDirectory(), "Gobos"));
    QVERIFY(under(NetworkGearCatalog::userDirectory(), "NetworkGear"));
    qputenv("HOME", realHome);
#endif
}

void QLCFile_Test::aRemovedLegacyLinkStaysRemoved()
{
#if defined(__APPLE__) || defined(Q_OS_MAC)
    QTemporaryDir home;
    const QByteArray realHome = qgetenv("HOME");
    qputenv("HOME", home.path().toUtf8());
    const QString oldFixtures = home.filePath("Library/Application Support/QLC+/Fixtures");
    const QString newFixtures = home.filePath(QString(USERQLCPLUSDIR) + "/Fixtures");
    QVERIFY(QDir().mkpath(oldFixtures));
    auto write = [](const QString &path) {
        QFile f(path);
        return f.open(QIODevice::WriteOnly) && f.write("<x/>") > 0;
    };
    QVERIFY(write(oldFixtures + "/Keep.qxf"));
    QVERIFY(write(oldFixtures + "/Broken.qxf"));

    QLCFile::mergeLegacyUserData();
    QVERIFY(QFileInfo(newFixtures + "/Keep.qxf").isSymLink());
    QVERIFY(QFileInfo(newFixtures + "/Broken.qxf").isSymLink());

    // Removed here: not put back next start.
    QVERIFY(QFile::remove(newFixtures + "/Broken.qxf"));
    QLCFile::mergeLegacyUserData();
    QVERIFY2(!QFileInfo(newFixtures + "/Broken.qxf").exists()
             && !QFileInfo(newFixtures + "/Broken.qxf").isSymLink(), "a removed link came back");
    QVERIFY(QFileInfo(newFixtures + "/Keep.qxf").isSymLink());

    // A file new to the old folder still comes across.
    QVERIFY(write(oldFixtures + "/New.qxf"));
    QLCFile::mergeLegacyUserData();
    QVERIFY(QFileInfo(newFixtures + "/New.qxf").isSymLink());
    qputenv("HOME", realHome);
#else
    QSKIP("the legacy folder layout this checks is the macOS one");
#endif
}

/* Windows, simulated (CI's Windows job runs it for real): the user data
   lives under %USERPROFILE%, never "HOME/qlcconsole" -- with HOME unset (a
   desktop session) that was the current drive's root, C:\qlcconsole. */
void QLCFile_Test::windowsLegacyRootsComeFromTheUserProfile()
{
    const QString profile = QStringLiteral("C:/Users/Stage Crew");
    QLCFile::LegacyRoots r = QLCFile::legacyUserDataRoots(QLCFile::LegacyPlatform::Windows,
                                                          QString(), profile);
    QCOMPARE(r.oldRoot, profile + "/QLC+");
    QCOMPARE(r.newRoot, profile + "/qlcconsole");
    QVERIFY(r.copy);
    QVERIFY(r.subdirs.contains("Fixtures"));

    // HOME set too (OpenSSH, MSYS): still the profile
    r = QLCFile::legacyUserDataRoots(QLCFile::LegacyPlatform::Windows,
                                     QStringLiteral("/home/crew"), profile);
    QCOMPARE(r.newRoot, profile + "/qlcconsole");

    // no profile known: nothing at all, never a folder at a drive's root
    r = QLCFile::legacyUserDataRoots(QLCFile::LegacyPlatform::Windows,
                                     QStringLiteral("/home/crew"), QString());
    QVERIFY(r.oldRoot.isEmpty() && r.newRoot.isEmpty());
    r = QLCFile::legacyUserDataRoots(QLCFile::LegacyPlatform::MacOS, QString(), profile);
    QVERIFY(r.newRoot.isEmpty());

    r = QLCFile::legacyUserDataRoots(QLCFile::LegacyPlatform::MacOS,
                                     QStringLiteral("/Users/crew"), QString());
    QCOMPARE(r.oldRoot, QStringLiteral("/Users/crew/Library/Application Support/QLC+"));
    QVERIFY(!r.copy);
}

/* A Windows "link" was a shell shortcut named like the definition, which the
   loader can't read ("Resource error occurred" at every start): the files
   are copied there, and a shortcut an earlier build left is replaced. */
void QLCFile_Test::windowsLegacyFilesAreCopiedNotShortcut()
{
    QTemporaryDir profile;
    QVERIFY(profile.isValid());
    QLCFile::LegacyRoots roots = QLCFile::legacyUserDataRoots(QLCFile::LegacyPlatform::Windows,
                                                              QString(), profile.path());
    const QString oldFixtures = roots.oldRoot + "/Fixtures";
    const QString newFixtures = roots.newRoot + "/Fixtures";
    QVERIFY(QDir().mkpath(oldFixtures));
    QVERIFY(QDir().mkpath(newFixtures));
    auto write = [](const QString &path, const QByteArray &data) {
        QFile f(path);
        return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
    };
    auto read = [](const QString &path) {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    };
    const QByteArray def("<?xml version='1.0'?><FixtureDefinition/>\n");
    QVERIFY(write(oldFixtures + "/Legacy-Test.qxf", def));
    QVERIFY(write(oldFixtures + "/Old-Shortcut.qxf", def));
    QVERIFY(write(oldFixtures + "/Broken.qxf", def));
    // what an earlier build's QFile::link() wrote on Windows
    QByteArray lnk("\x4c\x00\x00\x00\x01\x14\x02\x00\x00\x00\x00\x00\xc0\x00\x00\x00", 16);
    lnk.append(QByteArray(60, '\0'));
    QVERIFY(write(newFixtures + "/Old-Shortcut.qxf", lnk));
    QVERIFY(QLCFile::isWindowsShortcut(newFixtures + "/Old-Shortcut.qxf"));
    QVERIFY(!QLCFile::isWindowsShortcut(oldFixtures + "/Legacy-Test.qxf"));

    QLCFile::mergeLegacyUserData(roots);
    for (const QString &name : { QStringLiteral("Legacy-Test.qxf"), QStringLiteral("Old-Shortcut.qxf"),
                                QStringLiteral("Broken.qxf") })
    {
        const QFileInfo fi(newFixtures + "/" + name);
        QVERIFY2(fi.exists() && !fi.isSymLink(), qPrintable(name));
        QVERIFY2(!QLCFile::isWindowsShortcut(fi.filePath()), qPrintable(name));
        QCOMPARE(read(fi.filePath()), def);
    }

    // removed here: not copied back next start
    QVERIFY(QFile::remove(newFixtures + "/Broken.qxf"));
    QLCFile::mergeLegacyUserData(roots);
    QVERIFY2(!QFileInfo::exists(newFixtures + "/Broken.qxf"), "a removed copy came back");

    // a file of the user's own under the same name always wins
    const QByteArray mine("<?xml version='1.0'?><FixtureDefinition>mine</FixtureDefinition>\n");
    QVERIFY(write(newFixtures + "/Legacy-Test.qxf", mine));
    QLCFile::mergeLegacyUserData(roots);
    QCOMPARE(read(newFixtures + "/Legacy-Test.qxf"), mine);

    // nothing written anywhere but the profile
    QVERIFY(QFileInfo(roots.newRoot + "/.legacy-linked").exists());
}

/* Qt's object hooks (qhooks_p.h, stable since Qt 5.4: index 3 is
   AddQObject, 4 RemoveQObject) count the QObjects alive. */
extern Q_CORE_EXPORT quintptr qtHookData[];
static int s_liveObjects = 0;
static void countAdd(QObject *) { ++s_liveObjects; }
static void countRemove(QObject *) { --s_liveObjects; }

/* A file that can't be opened: the reader is NULL and its QFile is gone.
   The QFile leaked, once per fixture definition a show names that isn't
   in the show's folder (TEST_MATRIX L3). */
void QLCFile_Test::aFileThatCannotBeOpenedLeavesNothingBehind()
{
    const quintptr oldAdd = qtHookData[3];
    const quintptr oldRemove = qtHookData[4];
    qtHookData[3] = reinterpret_cast<quintptr>(&countAdd);
    qtHookData[4] = reinterpret_cast<quintptr>(&countRemove);
    s_liveObjects = 0;

    QTemporaryDir dir;
    const QString missing = dir.filePath("No-Such-Fixture.qxf");
    for (int i = 0; i < 10; i++)
        QVERIFY(QLCFile::getXMLReader(missing) == NULL);
    const int leftAfterReader = s_liveObjects;

    QXmlStreamReader *ok = QLCFile::getXMLReader(QFINDTESTDATA("readonly.xml.in"));
    QVERIFY(ok != NULL);
    QLCFile::releaseXMLReader(ok);
    const int leftAfterRelease = s_liveObjects;

    qtHookData[3] = oldAdd;
    qtHookData[4] = oldRemove;
    QCOMPARE(leftAfterReader, 0);
    QCOMPARE(leftAfterRelease, 0);
}

/* The HID plugin's folders come from the build, as the installer lays them
   out. The user's was "Library/Application Support/QLC+/HIDProfiles" under
   the home folder on every platform, which made stock QLC+'s folder on a
   clean Mac, and a "Library" folder in a Windows or Linux home; the
   built-in one was the macOS bundle's, so Windows and Linux never found the
   pad profiles they install (TEST_MATRIX L5). */
void QLCFile_Test::hidProfilesLiveInQlcconsolesFolders()
{
    const QString user = QStringLiteral(USERHIDPROFILEDIR);
    QCOMPARE(user, QString(USERQLCPLUSDIR) + QStringLiteral("/HIDProfiles"));
    QVERIFY(!user.contains(QStringLiteral("QLC+")));
    const QString system = QStringLiteral(HIDPROFILEDIR);
    QVERIFY2(system.endsWith(QStringLiteral("HIDProfiles"), Qt::CaseInsensitive), qPrintable(system));
#if defined(WIN32) || defined(Q_OS_WIN)
    QCOMPARE(system, QStringLiteral("HIDProfiles"));            // beside qlcconsole.exe
#elif defined(__APPLE__) || defined(Q_OS_MAC)
    QCOMPARE(system, QStringLiteral("Resources/HIDProfiles"));  // in the bundle
#else
    QVERIFY2(system.contains(QStringLiteral("hidprofiles")), qPrintable(system));
#endif
}

/* QLC_SHOW_DEBUG's log went to the fixed /tmp/qlc_showdebug.log, a name
   another user on the machine could take first (a link, followed). It is
   in the user's own folder (red team L3). */
void QLCFile_Test::theShowDebugLogIsInTheUsersFolder()
{
#if defined(WIN32) || defined(Q_OS_WIN)
    QVERIFY(QLCFile::showDebugLogPath().contains(QStringLiteral(USERQLCPLUSDIR)));
#else
    QTemporaryDir home;
    const QByteArray realHome = qgetenv("HOME");
    qputenv("HOME", home.path().toUtf8());
    const QString path = QLCFile::showDebugLogPath();
    qputenv("HOME", realHome);
    QCOMPARE(QFileInfo(path).absolutePath(),
             QDir(home.path()).absoluteFilePath(QStringLiteral(USERQLCPLUSDIR)));
    QVERIFY(!path.startsWith(QStringLiteral("/tmp/")));
#endif
}

QTEST_APPLESS_MAIN(QLCFile_Test)

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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    doc.setCodec("UTF-8");
#endif

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
    QCOMPARE(QLCFile::errorString(QFile::FileError(31337)),
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

QTEST_APPLESS_MAIN(QLCFile_Test)

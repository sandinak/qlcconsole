/*
  Q Light Controller Plus
  qlcfile.cpp

  Copyright (C) Heikki Junnila
                Massimo Callegari

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#ifdef QT_XML_LIB
#   include <QtXml>
#else
#   include <QDebug>
#endif

#if defined(WIN32) || defined(Q_OS_WIN)
#   include <windows.h>
#   include <lmcons.h>
#else
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <unistd.h>
#   include <pwd.h>
#endif
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>

#include "qlcconfig.h"
#include "qlcfile.h"

bool QLCFile::m_hasWindowManager = true;

QXmlStreamReader *QLCFile::getXMLReader(const QString &path)
{
    QXmlStreamReader *reader = NULL;

    if (path.isEmpty() == true)
    {
        qWarning() << Q_FUNC_INFO
                   << "Empty path given. Not attempting to load file.";
        return reader;
    }

    QFile *file = new QFile(path);
    if (file->open(QIODevice::ReadOnly | QFile::Text) == true)
    {
        reader = new QXmlStreamReader(file);
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "Unable to open file:" << path;
        /* it leaked, once per definition a show names that isn't there
           (Fixture::loadXML() tries the show's own folder for each one) */
        delete file;
    }

    return reader;
}

void QLCFile::releaseXMLReader(QXmlStreamReader *reader)
{
    if (reader == NULL)
        return;

    if (reader->device() != NULL)
    {
        if (reader->device()->isOpen())
            reader->device()->close();
        delete reader->device();
    }
    delete reader;
}

bool QLCFile::writeXMLHeader(QXmlStreamWriter *xml, const QString &content, const QString &author)
{
    if (xml == NULL || xml->device() == NULL)
        return false;

    xml->writeStartDocument();
    xml->writeDTD(QString("<!DOCTYPE %1>").arg(content));

    xml->writeStartElement(content);
    xml->writeAttribute("xmlns", KXMLQLCplusNamespace + content);

    xml->writeStartElement(KXMLQLCCreator);
    xml->writeTextElement(KXMLQLCCreatorName, APPNAME);
    xml->writeTextElement(KXMLQLCCreatorVersion, APPVERSION);
    if (author.isEmpty())
        xml->writeTextElement(KXMLQLCCreatorAuthor, currentUserName());
    else
        xml->writeTextElement(KXMLQLCCreatorAuthor, author);
    xml->writeEndElement(); // close KXMLQLCCreator

    return true;
}

QString QLCFile::errorString(QFile::FileError error)
{
    switch (error)
    {
    case QFile::NoError:
        return QObject::tr("No error occurred.");
    case QFile::ReadError:
        return QObject::tr("An error occurred when reading from the file.");
    case QFile::WriteError:
        return QObject::tr("An error occurred when writing to the file.");
    case QFile::FatalError:
        return QObject::tr("A fatal error occurred.");
    case QFile::ResourceError:
        return QObject::tr("Resource error occurred.");
    case QFile::OpenError:
        return QObject::tr("The file could not be opened.");
    case QFile::AbortError:
        return QObject::tr("The operation was aborted.");
    case QFile::TimeOutError:
        return QObject::tr("A timeout occurred.");
    case QFile::UnspecifiedError:
        return QObject::tr("An unspecified error occurred.");
    case QFile::RemoveError:
        return QObject::tr("The file could not be removed.");
    case QFile::RenameError:
        return QObject::tr("The file could not be renamed.");
    case QFile::PositionError:
        return QObject::tr("The position in the file could not be changed.");
    case QFile::ResizeError:
        return QObject::tr("The file could not be resized.");
    case QFile::PermissionsError:
        return QObject::tr("The file could not be accessed.");
    case QFile::CopyError:
        return QObject::tr("The file could not be copied.");
    default:
        return QObject::tr("An unknown error occurred.");
    }
}

QString QLCFile::settingsOrganisation()
{
    /* As QSettings() picks it (App sets both): the domain on Apple
       platforms, the name everywhere else. */
#if defined(Q_OS_DARWIN)
    return QStringLiteral("qlcplus.org");
#else
    return QStringLiteral("qlcplus");
#endif
}

QString QLCFile::currentUserName()
{
#if defined(WIN32) || defined(Q_OS_WIN)
    DWORD length = UNLEN + 1;
    TCHAR name[length];
    if (GetUserName(name, &length))
        return QString::fromUtf16(reinterpret_cast<char16_t*>(name));
    else
        return QString("Unknown windows user");
#else
 #if defined(Q_OS_ANDROID)
    return QString(getenv("USER"));
 #else
    QString name;
    struct passwd* passwd = getpwuid(getuid());
    if (passwd == NULL)
        name.append(getenv("USER"));
    else
        name.append(passwd->pw_gecos);
    name.remove(",,,");
    return name;
 #endif
#endif
}

void QLCFile::setHasWindowManager(bool enable)
{
    m_hasWindowManager = enable;
}

bool QLCFile::hasWindowManager()
{
    return m_hasWindowManager;
}

QDir QLCFile::systemDirectory(QString path, QString extension)
{
    QDir dir;
#if defined(Q_OS_IOS)
    dir.setPath(QString("%1/%2").arg(QCoreApplication::applicationDirPath())
                                   .arg(path));
#elif defined(__APPLE__) || defined(Q_OS_MAC)
    dir.setPath(QString("%1/../%2").arg(QCoreApplication::applicationDirPath())
                                   .arg(path));
#elif defined(WIN32) || defined(Q_OS_WIN)
    dir.setPath(QString("%1%2%3").arg(QCoreApplication::applicationDirPath())
                                 .arg(QDir::separator())
                                 .arg(path));
#elif defined(Q_OS_ANDROID)
    dir.setPath(QString("assets:/%1").arg(path.remove(0, path.lastIndexOf("/") + 1)));
#else
    dir.setPath(path);
#endif

    dir.setFilter(QDir::Files);
    if (!extension.isEmpty())
        dir.setNameFilters(QStringList() << QString("*%1").arg(extension));

    return dir;
}

QDir QLCFile::userDirectory(QString path, QString fallBackPath, QStringList extensions)
{
    Q_UNUSED(fallBackPath)
    QDir dir;

#if defined(Q_WS_X11) || defined(Q_OS_LINUX)
    // If the current user is root, return the system fixture dir.
    // Otherwise return a path under user's home dir.
    if (geteuid() == 0 && QLCFile::hasWindowManager())
        dir = QDir(fallBackPath);
    else
        dir.setPath(QString("%1/%2").arg(getenv("HOME")).arg(path));
#elif defined(__APPLE__) || defined (Q_OS_MAC)
    /* User's input profile directory on OSX */
    dir.setPath(QString("%1/%2").arg(getenv("HOME")).arg(path));
#else
    /* User's input profile directory on Windows */
    LPTSTR home = (LPTSTR) malloc(256 * sizeof(TCHAR));
    GetEnvironmentVariable(TEXT("UserProfile"), home, 256);
    dir.setPath(QString("%1/%2")
                    .arg(QString::fromUtf16(reinterpret_cast<char16_t*>(home)))
                    .arg(path));
    free(home);
#endif

    // Ensure the directory exists
    if (dir.exists() == false)
        dir.mkpath(".");

    dir.setFilter(QDir::Files);
    dir.setNameFilters(extensions);

    return dir;
}

QLCFile::LegacyRoots QLCFile::legacyUserDataRoots(LegacyPlatform platform,
                                                  const QString &home,
                                                  const QString &userProfile)
{
    LegacyRoots roots;
    /* Windows keeps user data under %USERPROFILE%, as userDirectory() does.
       HOME is not set in a normal desktop session there (OpenSSH and MSYS set
       it), and "/qlcconsole" with no HOME is the current drive's root: on a
       default install, C:\qlcconsole. */
    const QString base = (platform == LegacyPlatform::Windows) ? userProfile : home;
    if (base.isEmpty())
        return roots;

    switch (platform)
    {
        case LegacyPlatform::Windows:
            roots.oldRoot = QString("%1/QLC+").arg(base);
            roots.newRoot = QString("%1/qlcconsole").arg(base);
        break;
        case LegacyPlatform::MacOS:
            roots.oldRoot = QString("%1/Library/Application Support/QLC+").arg(base);
            roots.newRoot = QString("%1/Library/Application Support/qlcconsole").arg(base);
        break;
        case LegacyPlatform::Unix:
            roots.oldRoot = QString("%1/.qlcplus").arg(base);
            roots.newRoot = QString("%1/.qlcconsole").arg(base);
        break;
    }
    roots.subdirs = (platform == LegacyPlatform::Unix)
        ? QStringList{ "fixtures", "rgbscripts", "miditemplates", "modifierstemplates",
                       "inputprofiles", "colorfilters", "Bundles", "EffectScripts",
                       "EffectPresets" }
        : QStringList{ "Fixtures", "RGBScripts", "MidiTemplates", "ModifiersTemplates",
                       "InputProfiles", "ColorFilters", "Bundles", "EffectScripts",
                       "EffectPresets" };
    /* A Windows "link" is a shell shortcut (.lnk) file under the target's
       name, which no loader can read: copy there instead. */
    roots.copy = (platform == LegacyPlatform::Windows);
    return roots;
}

bool QLCFile::isWindowsShortcut(const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly) == false)
        return false;
    /* ShellLinkHeader: HeaderSize 0x4C, then LinkCLSID 00021401-0000-0000-C000-000000000046 */
    static const QByteArray magic("\x4c\x00\x00\x00\x01\x14\x02\x00", 8);
    return f.read(8) == magic;
}

void QLCFile::mergeLegacyUserData()
{
#if defined(WIN32) || defined(Q_OS_WIN)
    const LegacyPlatform platform = LegacyPlatform::Windows;
#elif defined(__APPLE__) || defined(Q_OS_MAC)
    const LegacyPlatform platform = LegacyPlatform::MacOS;
#else
    const LegacyPlatform platform = LegacyPlatform::Unix;
#endif
    const QString home = QString::fromLocal8Bit(qgetenv("HOME"));
    const QString userProfile = QString::fromLocal8Bit(qgetenv("USERPROFILE"));
    LegacyRoots roots = legacyUserDataRoots(platform, home, userProfile);
    if (roots.newRoot.isEmpty())
        return;
    /* the build's own folder name, as userDirectory() uses it */
    roots.newRoot = QString("%1/%2").arg(platform == LegacyPlatform::Windows ? userProfile : home)
                                    .arg(USERQLCPLUSDIR);
    mergeLegacyUserData(roots);
}

void QLCFile::mergeLegacyUserData(const LegacyRoots &roots)
{
    const QString &oldRoot = roots.oldRoot;
    const QString &newRoot = roots.newRoot;
    if (oldRoot.isEmpty() || newRoot.isEmpty() || oldRoot == newRoot)
        return;

    QDir oldDir(oldRoot);
    if (oldDir.exists() == false)
        return;

    /* What has been linked before, so a link removed on purpose stays
       removed: this runs at every start, and used to put back whatever was
       taken out (a malformed definition deleted here came back next time). */
    const QString ledgerPath = QString("%1/.legacy-linked").arg(newRoot);
    QSet<QString> linked;
    {
        QFile ledger(ledgerPath);
        if (ledger.open(QIODevice::ReadOnly | QIODevice::Text))
            for (const QByteArray &line : ledger.readAll().split('\n'))
                if (!line.trimmed().isEmpty())
                    linked.insert(QString::fromUtf8(line.trimmed()));
    }
    const QSet<QString> before = linked;

    foreach (const QString &sub, roots.subdirs)
    {
        QDir oldSub(oldDir.filePath(sub));
        if (oldSub.exists() == false)
            continue;

        QDir newSub(QString("%1/%2").arg(newRoot).arg(sub));
        newSub.mkpath(".");

        foreach (const QString &fileName, oldSub.entryList(QDir::Files))
        {
            const QString key = sub + QLatin1Char('/') + fileName;
            const QString oldFile = oldSub.filePath(fileName);
            QString newFile = newSub.filePath(fileName);
            QFileInfo there(newFile);

            /* An earlier build "linked" on Windows too: a shell shortcut
               under the definition's name, which failed to load at every
               start. Replace it with the file itself. */
            if (roots.copy && there.exists() && isWindowsShortcut(newFile))
            {
                if (QFile::remove(newFile) && QFile::copy(oldFile, newFile))
                    linked.insert(key);
                continue;
            }

            if (there.exists() || there.isSymLink())
            {
                // a file already placed under the new root wins -- and a
                // link this made earlier is now on the ledger
                if (there.isSymLink() && there.symLinkTarget() == QFileInfo(oldFile).absoluteFilePath())
                    linked.insert(key);
                continue;
            }
            if (linked.contains(key))
                continue; // linked once, removed since: leave it removed

            if (roots.copy ? QFile::copy(oldFile, newFile) : QFile::link(oldFile, newFile))
                linked.insert(key);
        }
    }

    if (linked != before)
    {
        QFile ledger(ledgerPath);
        if (ledger.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        {
            QStringList keys = linked.values();
            keys.sort();
            ledger.write(keys.join(QLatin1Char('\n')).toUtf8() + '\n');
        }
    }
}

QString QLCFile::showDebugLogPath()
{
    const QDir dir = userDirectory(QString(USERQLCPLUSDIR), QString(USERQLCPLUSDIR), QStringList());
    return dir.absoluteFilePath(QStringLiteral("showdebug.log"));
}

QString QLCFile::fileUrlPrefix()
{
#if defined(WIN32) || defined(Q_OS_WIN)
    return QString("file:///");
#else
    return QString("file://");
#endif
}

quint32 QLCFile::getQtRuntimeVersion()
{
    QString ver(qVersion());
    if (ver.isEmpty())
        return 0;

    QStringList digits = ver.split(".");

    return digits.at(0).toInt() * 10000 + digits.at(1).toInt() * 100 + digits.at(2).toInt();
}

/*****************************************************************************
 * Cloud storage: online-only files
 *****************************************************************************/

bool QLCFile::isOnlineOnly(const QString &path)
{
#if defined(WIN32) || defined(Q_OS_WIN)
    const DWORD attrs = GetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()));
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return false;
    // FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS, _RECALL_ON_OPEN, _OFFLINE
    return (attrs & (0x00400000 | 0x00040000 | FILE_ATTRIBUTE_OFFLINE)) != 0;
#elif defined(__APPLE__) || defined(Q_OS_MAC)
#   ifndef SF_DATALESS
#       define SF_DATALESS 0x40000000
#   endif
    struct stat st;
    if (stat(QFile::encodeName(path).constData(), &st) != 0)
        return false;
    return (st.st_flags & SF_DATALESS) != 0;
#else
    Q_UNUSED(path)
    return false;
#endif
}

namespace
{
/* Kept on the application object, not in a static here: plugins build their
   own copy of this file, and what a plugin skips (a MIDI template, an HID
   profile) belongs in the same list as the rest. */
const char *const kOnlineOnlyProperty = "qlcSkippedOnlineOnly";
QMutex s_onlineOnlyMutex;
QStringList s_onlineOnly;                   // no application object (some tests)

QStringList readOnlineOnly()
{
    if (QCoreApplication *app = QCoreApplication::instance())
        return app->property(kOnlineOnlyProperty).toStringList();
    return s_onlineOnly;
}

void writeOnlineOnly(const QStringList &list)
{
    if (QCoreApplication *app = QCoreApplication::instance())
        app->setProperty(kOnlineOnlyProperty, list);
    else
        s_onlineOnly = list;
}
}

void QLCFile::noteSkippedOnlineOnly(const QString &path)
{
    QMutexLocker locker(&s_onlineOnlyMutex);
    QStringList list = readOnlineOnly();
    if (!list.contains(path))
    {
        list << path;
        writeOnlineOnly(list);
    }
}

QStringList QLCFile::skippedOnlineOnly()
{
    QMutexLocker locker(&s_onlineOnlyMutex);
    return readOnlineOnly();
}

void QLCFile::forgetSkippedOnlineOnly(const QString &path)
{
    QMutexLocker locker(&s_onlineOnlyMutex);
    QStringList list = readOnlineOnly();
    if (path.isEmpty())
        list.clear();
    else
        list.removeAll(path);
    writeOnlineOnly(list);
}

/*
  Q Light Controller
  qlcfixturedefcache.cpp

  Copyright (c) Heikki Junnila

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

#include <QCoreApplication>
#include <QXmlStreamReader>
#include <QDebug>
#include <QList>
#include <QSet>

#if defined(WIN32) || defined(Q_OS_WIN)
#   include <windows.h>
#else
#   include <unistd.h>
#endif

#include "qlcfixturedefcache.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "avolitesd4parser.h"
#include "qlcfixturedef.h"
#include "qlcconfig.h"
#include "qlcfile.h"

#define FIXTURES_MAP_NAME QStringLiteral("FixturesMap.xml")
#define KXMLQLCFixtureMap QStringLiteral("FixturesMap")

QLCFixtureDefCache::QLCFixtureDefCache()
{
}

QLCFixtureDefCache::~QLCFixtureDefCache()
{
    clear();
}

QLCFixtureDef* QLCFixtureDefCache::fixtureDef(
    const QString& manufacturer, const QString& model) const
{
    QListIterator <QLCFixtureDef*> it(m_defs);
    while (it.hasNext() == true)
    {
        QLCFixtureDef* def = it.next();
        if (def->manufacturer() == manufacturer && def->model() == model)
        {
            def->checkLoaded(m_mapAbsolutePath);
            return def;
        }
    }

    return NULL;
}

QStringList QLCFixtureDefCache::manufacturers() const
{
    QSet <QString> makers;

    // Gather a list of manufacturers
    QListIterator <QLCFixtureDef*> it(m_defs);
    while (it.hasNext() == true)
        makers << it.next()->manufacturer();

    // Bounce the QSet into a QStringList
    QStringList list;
    foreach (QString manuf, makers)
        list << manuf;

    return list;
}

QStringList QLCFixtureDefCache::models(const QString& manufacturer) const
{
    QSet <QString> models;
    QListIterator <QLCFixtureDef*> it(m_defs);
    while (it.hasNext() == true)
    {
        QLCFixtureDef* def = it.next();
        if (def->manufacturer() == manufacturer)
            models << def->model();
    }

    // Bounce the QSet into a QStringList
    QStringList list;
    foreach (QString model, models)
        list << model;

    return list;
}

QMap<QString, QMap<QString, bool> > QLCFixtureDefCache::fixtureCache() const
{
    QMap<QString, QMap<QString, bool> > map;

    QListIterator <QLCFixtureDef*> it(m_defs);
    while (it.hasNext() == true)
    {
        QLCFixtureDef *def = it.next();
        map[def->manufacturer()][def->model()] = def->isUser();
    }

    return map;
}

bool QLCFixtureDefCache::addFixtureDef(QLCFixtureDef* fixtureDef)
{
    if (fixtureDef == NULL)
        return false;

    if (models(fixtureDef->manufacturer()).contains(fixtureDef->model()) == false)
    {
        m_defs << fixtureDef;
        return true;
    }
    else
    {
        /* The user's folder is read first, so an included definition of the
           same fixture arrives second and is set aside -- say so on the one
           that is used. */
        foreach (QLCFixtureDef *existing, m_defs)
        {
            if (existing->manufacturer() != fixtureDef->manufacturer()
                || existing->model() != fixtureDef->model())
                continue;
            if (fixtureDef->source() == QLCFixtureDef::Included
                && existing->source() == QLCFixtureDef::Local)
                existing->setOverridesIncluded(true);

            Duplicate d;
            d.kept = existing->definitionSourceFile();
            d.ignored = fixtureDef->definitionSourceFile();
            d.keptIsLocal = existing->source() == QLCFixtureDef::Local;
            d.ignoredIsLocal = fixtureDef->source() == QLCFixtureDef::Local;
            if (d.keptIsLocal && d.ignoredIsLocal)
            {
                QFile a(d.kept), b(d.ignored);
                d.identical = a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly)
                              && a.readAll() == b.readAll();
            }
            m_duplicates << d;

            /* A local definition replacing an included one is how the library
               is customised: not worth a warning. Two local files for one
               fixture are, and the footer lists them. */
            if (d.keptIsLocal && d.ignoredIsLocal)
                qWarning() << Q_FUNC_INFO << "Two local definitions of" << fixtureDef->name()
                           << "- using" << d.kept << "not" << d.ignored;
            else
                qDebug() << Q_FUNC_INFO << fixtureDef->name() << "- using" << d.kept;
            break;
        }
        return false;
    }
}

bool QLCFixtureDefCache::storeFixtureDef(QString filename, QString data)
{
    QDir userFolder = userDefinitionDirectory();

    QFile file(userFolder.absoluteFilePath(filename));
    if (file.open(QIODevice::WriteOnly | QIODevice::Text) == false)
        return false;

    file.write(data.toUtf8());
    file.close();
#ifdef Q_OS_UNIX
    sync();
#endif

    // reload user definitions
    load(userDefinitionDirectory());

    return true;
}

bool QLCFixtureDefCache::reloadFixtureDef(QLCFixtureDef *fixtureDef)
{
    int idx = m_defs.indexOf(fixtureDef);
    if (idx == -1)
        return false;

    // Reload into a temporary object and deep-copy over the caller's
    // instance in place, rather than deleting it and swapping in a new
    // one: callers (e.g. the fixture editor) keep this pointer around,
    // and replacing it out from under them would leave a dangling pointer.
    QString absPath = fixtureDef->definitionSourceFile();

    QLCFixtureDef reloaded;
    if (reloaded.loadXML(absPath) != QFile::NoError)
        return false;

    *fixtureDef = reloaded;

    return true;
}

bool QLCFixtureDefCache::reloadPhysical(QLCFixtureDef *fixtureDef)
{
    if (fixtureDef == NULL || fixtureDef->definitionSourceFile().isEmpty())
        return false;

    QLCFixtureDef reloaded;
    if (reloaded.loadXML(fixtureDef->definitionSourceFile()) != QFile::NoError)
        return false;

    fixtureDef->setPhysical(reloaded.physical());
    foreach (QLCFixtureMode *mode, fixtureDef->modes())
    {
        QLCFixtureMode *fresh = reloaded.mode(mode->name());
        if (fresh != NULL)
            mode->setPhysical(fresh->physical());
    }
    return true;
}

bool QLCFixtureDefCache::reloadOrAddFixtureDef(QLCFixtureDef *fixtureDef)
{
    // check upon bundled definitions
    QListIterator <QLCFixtureDef*> it(m_defs);
    while (it.hasNext() == true)
    {
        QLCFixtureDef *def = it.next();
        if (def->manufacturer() == fixtureDef->manufacturer() &&
            def->model() == fixtureDef->model())
        {
            // set as user and perform a deep copy
            def->setIsUser(true);
            *def = *fixtureDef;
            return true;
        }
    }

    // add a new user fixture
    addFixtureDef(fixtureDef);

    return true;
}

bool QLCFixtureDefCache::load(const QDir& dir)
{
    qDebug() << Q_FUNC_INFO << dir.path();

    if (dir.exists() == false || dir.isReadable() == false)
        return false;

    /* Attempt to read all specified files from the given directory --
       newest first, so of two files for the same fixture the one edited
       last is the one used (it was alphabetical: "X 2.qxf" beat "X.qxf"
       whichever was current). */
    QStringListIterator it(dir.entryList(QDir::Files | QDir::Readable, QDir::Time));
    while (it.hasNext() == true)
    {
        QString path(dir.absoluteFilePath(it.next()));
        if (QLCFile::isOnlineOnly(path))   // in cloud storage, not on this machine: do not wait for it
        {
            QLCFile::noteSkippedOnlineOnly(path);
            continue;
        }

        if (path.toLower().endsWith(KExtFixture) == true)
            loadQXF(path, true);
        else if (path.toLower().endsWith(KExtAvolitesFixture) == true)
            loadD4(path);
        else
            qWarning() << Q_FUNC_INFO << "Unrecognized fixture extension:" << path;
    }

    return true;
}

int QLCFixtureDefCache::loadMapManufacturer(QXmlStreamReader *doc, QString manufacturer)
{
    int count = 0;
    QString spacedManufacturer = manufacturer;
    spacedManufacturer.replace("_", " ");

    while (doc->readNextStartElement())
    {
        if (doc->name() == QString("F"))
        {
            QString defFile = "";
            QString model = "";

            if (doc->attributes().hasAttribute("n"))
            {
                defFile = QString("%1%2%3%4")
                            .arg(manufacturer).arg(QDir::separator())
                            .arg(doc->attributes().value("n").toString()).arg(KExtFixture);
                //qDebug() << "Manufacturer" << spacedManufacturer << "file" << defFile;
            }

            if (doc->attributes().hasAttribute("m"))
                model = doc->attributes().value("m").toString();

            if (defFile.isEmpty() == false &&
                spacedManufacturer.isEmpty() == false &&
                model.isEmpty() == false)
            {
                QLCFixtureDef *fxi = new QLCFixtureDef();
                Q_ASSERT(fxi != NULL);

                fxi->setDefinitionSourceFile(defFile);
                fxi->setManufacturer(spacedManufacturer);
                fxi->setModel(model);

                /* Delete the def if it's a duplicate. */
                if (addFixtureDef(fxi) == false)
                    delete fxi;
                fxi = NULL;
                count++;
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown manufacturer tag: " << doc->name();
        }
        doc->skipCurrentElement();
    }

    return count;
}

bool QLCFixtureDefCache::loadMap(const QDir &dir)
{
    qDebug() << Q_FUNC_INFO << dir.path();

    if (dir.exists() == false || dir.isReadable() == false)
        return false;

    QString mapPath(dir.absoluteFilePath(FIXTURES_MAP_NAME));

    if (mapPath.isEmpty() == true)
        return false;

    // cache the map path to be used when composing the fixture
    // definition absolute path
    m_mapAbsolutePath = dir.absolutePath();

    QXmlStreamReader *doc = QLCFile::getXMLReader(mapPath);
    if (doc == NULL || doc->device() == NULL || doc->hasError())
    {
        qWarning() << Q_FUNC_INFO << "Unable to read from" << mapPath;
        return false;
    }

    while (!doc->atEnd())
    {
        if (doc->readNext() == QXmlStreamReader::DTD)
            break;
    }

    if (doc->hasError())
    {
        QLCFile::releaseXMLReader(doc);
        return false;
    }

    // make sure the doc type is FixtureMap
    if (doc->dtdName() != KXMLQLCFixtureMap)
    {
        qWarning() << Q_FUNC_INFO << mapPath << "is not a fixture map file";
        QLCFile::releaseXMLReader(doc);
        return false;
    }

    if (doc->readNextStartElement() == false)
    {
        QLCFile::releaseXMLReader(doc);
        return false;
    }

    // make sure the root tag is FixtureMap
    if (doc->name() != KXMLQLCFixtureMap)
    {
        qWarning() << Q_FUNC_INFO << mapPath << "is not a fixture map file";
        QLCFile::releaseXMLReader(doc);
        return false;
    }

    int fxCount = 0;
    QString manufacturer = "";

    while (doc->readNextStartElement())
    {
        if (doc->name() == QString("M"))
        {
            if (doc->attributes().hasAttribute("n"))
            {
                manufacturer = doc->attributes().value("n").toString();
                fxCount += loadMapManufacturer(doc, manufacturer);
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Fixture Map tag: " << doc->name();
            doc->skipCurrentElement();
        }
    }
    qDebug() << fxCount << "fixtures found in map";

#if 0
    /* Attempt to read all files not in FixtureMap */
    QStringList definitionPaths;

    // Gather a list of manufacturers
    QListIterator <QLCFixtureDef*> mfit(m_defs);
    while (mfit.hasNext() == true)
        definitionPaths << mfit.next()->definitionSourceFile();

    QStringListIterator it(dir.entryList());
    while (it.hasNext() == true)
    {
        QString path(dir.absoluteFilePath(it.next()));
        if (QLCFile::isOnlineOnly(path))   // in cloud storage, not on this machine: do not wait for it
        {
            QLCFile::noteSkippedOnlineOnly(path);
            continue;
        }
        if (definitionPaths.contains(path))
            continue;

        qWarning() << path << "not in" << FIXTURES_MAP_NAME;

        if (path.toLower().endsWith(KExtFixture) == true)
            loadQXF(path);
        else if (path.toLower().endsWith(KExtAvolitesFixture) == true)
            loadD4(path);
        else
            qWarning() << Q_FUNC_INFO << "Unrecognized fixture extension:" << path;
    }
#endif
    return true;
}

void QLCFixtureDefCache::clear()
{
    while (m_defs.isEmpty() == false)
        delete m_defs.takeFirst();
}

QDir QLCFixtureDefCache::systemDefinitionDirectory()
{
    return QLCFile::systemDirectory(QString(FIXTUREDIR), QString(KExtFixture));
}

QDir QLCFixtureDefCache::userDefinitionDirectory()
{
    QStringList filters;
    filters << QString("*%1").arg(KExtFixture);
    filters << QString("*%1").arg(KExtAvolitesFixture);

    return QLCFile::userDirectory(QString(USERFIXTUREDIR), QString(FIXTUREDIR), filters);
}

QDir QLCFixtureDefCache::showCacheDirectory()
{
    /* Beside the user folders -- unless they are in cloud storage, which is
       the very thing this is for not depending on: then the system cache. */
    QDir dir(userDefinitionDirectory());
    dir.cdUp();
    QString base = dir.absolutePath();
    const QString real = QFileInfo(base).canonicalFilePath();
    if (real.isEmpty() || real.contains(QLatin1String("/CloudStorage/")))
        base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir cache(base + QStringLiteral("/ShowCache/Fixtures"));
    if (!cache.exists())
        cache.mkpath(QStringLiteral("."));
    return cache;
}

QString QLCFixtureDefCache::showCacheFile(const QString &manufacturer, const QString &model)
{
    QString name = manufacturer + QLatin1Char('-') + model;
    static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9._+-]+"));
    name.replace(unsafe, QStringLiteral("-"));
    return showCacheDirectory().absoluteFilePath(name + QString(KExtFixture));
}

bool QLCFixtureDefCache::cacheForShows(const QLCFixtureDef *def) const
{
    if (def == nullptr || def->source() == QLCFixtureDef::Included
        || def->model() == QLatin1String("Generic"))
        return false;
    const QString from = def->definitionSourceFile();
    const QString to = showCacheFile(def->manufacturer(), def->model());
    if (from.isEmpty() || QFileInfo(from).absoluteFilePath() == QFileInfo(to).absoluteFilePath())
        return QFile::exists(to);
    if (QLCFile::isOnlineOnly(from))
        return QFile::exists(to);           // not here to copy: keep what the cache has
    QFile src(from);
    if (!src.open(QIODevice::ReadOnly))
        return QFile::exists(to);
    const QByteArray bytes = src.readAll();
    QFile old(to);
    if (old.open(QIODevice::ReadOnly) && old.readAll() == bytes)
        return true;                        // already the same
    old.close();
    QFile dst(to + QStringLiteral(".part"));
    if (!dst.open(QIODevice::WriteOnly) || dst.write(bytes) != bytes.size())
        return QFile::exists(to);
    dst.close();
    QFile::remove(to);
    return QFile::rename(to + QStringLiteral(".part"), to);
}

bool QLCFixtureDefCache::loadFromShowCache(const QString &manufacturer, const QString &model)
{
    const QString path = showCacheFile(manufacturer, model);
    if (!QFile::exists(path))
        return false;
    loadQXF(path, true);
    return fixtureDef(manufacturer, model) != nullptr;
}

bool QLCFixtureDefCache::loadQXF(const QString& path, bool isUser)
{
    QLCFixtureDef *fxi = new QLCFixtureDef();
    Q_ASSERT(fxi != NULL);

    QFile::FileError error = fxi->loadXML(path);
    if (error == QFile::NoError)
    {
        m_failedDefinitions.remove(path);
        int unnamed = 0;
        foreach (const QLCChannel *ch, fxi->channels())
            foreach (const QLCCapability *cap, ch->capabilities())
                if (cap->name().trimmed().isEmpty())
                    ++unnamed;
        if (unnamed > 0)
            qWarning() << path << ":" << unnamed << "capabilities have no description";
        fxi->setIsUser(isUser);
        fxi->setDefinitionSourceFile(path);
        fxi->setLoaded(true);

        /* Delete the def if it's a duplicate. */
        if (addFixtureDef(fxi) == false)
            delete fxi;
        fxi = NULL;
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "Fixture definition loading from"
                   << path << "failed:" << QLCFile::errorString(error);
        m_failedDefinitions.insert(path, QLCFile::errorString(error));
        delete fxi;
        fxi = NULL;
        return false;
    }
    return true;
}

bool QLCFixtureDefCache::loadD4(const QString& path)
{
    QLCFixtureDef *fxi = new QLCFixtureDef();
    AvolitesD4Parser parser;
    if (parser.loadXML(path, fxi) == false)
    {
        qWarning() << Q_FUNC_INFO << "Unable to load D4 fixture from" << path
                   << ":" << parser.lastError();
        delete fxi;
        return false;
    }

    // a D4 personality is always a user-made fixture
    fxi->setIsUser(true);
    fxi->setDefinitionSourceFile(path);
    fxi->setLoaded(true);

    /* Delete the def if it's a duplicate. */
    if (addFixtureDef(fxi) == false)
    {
        qDebug() << Q_FUNC_INFO << "Deleting duplicate" << path;
        delete fxi;
    }
    fxi = NULL;

    return true;
}

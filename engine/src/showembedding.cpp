/*
  Q Light Controller Plus - qlcconsole
  showembedding.cpp

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
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QFile>
#include <QSet>
#include <QMap>
#include <QDebug>

#include "showembedding.h"
#include "qlcfixturedefcache.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "rgbscriptscache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "effectscript.h"
#include "rgbscriptv4.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "monitorproperties.h"
#include "headlayout.h"
#include "rgbmatrix.h"
#include "qlcfile.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

#define KXMLEmbeddedFixture     QStringLiteral("FixtureDefinition")
#define KXMLEmbeddedEffect      QStringLiteral("EffectScript")
#define KXMLEmbeddedRGB         QStringLiteral("RGBScript")

namespace
{
    bool s_deleteInsteadOfTrash = false;

    QString cleanAbs(const QString &path)
    {
        return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    }

    /* @p path is in @p dir (or a folder in it) -- by its path, or by where
       the links lead. */
    bool isUnder(const QString &path, const QString &dir)
    {
        if (path.isEmpty() || dir.isEmpty())
            return false;
        const QString d = QDir::cleanPath(QDir(dir).absolutePath()) + QLatin1Char('/');
        if (cleanAbs(path).startsWith(d))
            return true;
        const QString realDir = QFileInfo(dir).canonicalFilePath();
        const QString realPath = QFileInfo(path).canonicalFilePath();
        return !realDir.isEmpty() && !realPath.isEmpty()
               && realPath.startsWith(realDir + QLatin1Char('/'));
    }

    bool samePath(const QString &a, const QString &b)
    {
        if (a.isEmpty() || b.isEmpty())
            return false;
        if (cleanAbs(a) == cleanAbs(b))
            return true;
        const QString ra = QFileInfo(a).canonicalFilePath();
        return !ra.isEmpty() && ra == QFileInfo(b).canonicalFilePath();
    }

    QByteArray readFile(const QString &path, bool *ok = nullptr)
    {
        if (ok != nullptr)
            *ok = false;
        if (path.isEmpty() || QLCFile::isOnlineOnly(path))
            return QByteArray();
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return QByteArray();
        if (ok != nullptr)
            *ok = true;
        return f.readAll();
    }

    bool writeFile(const QString &path, const QByteArray &bytes)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile part(path + QStringLiteral(".part"));
        if (!part.open(QIODevice::WriteOnly) || part.write(bytes) != bytes.size())
            return false;
        part.close();
        QFile::remove(path);
        return QFile::rename(path + QStringLiteral(".part"), path);
    }

    /* Bytes that come back from CDATA exactly as they went in: UTF-8 text,
       no "]]>", no carriage returns (XML turns CR LF into LF) and nothing
       XML cannot hold. Anything else goes as base64. */
    bool fitsCDATA(const QByteArray &bytes)
    {
        if (bytes.contains("]]>") || bytes.contains('\r'))
            return false;
        for (char c : bytes)
            if (uchar(c) < 0x20 && c != '\n' && c != '\t')
                return false;
        return QString::fromUtf8(bytes).toUtf8() == bytes;
    }

    QString safeFileName(const QString &name)
    {
        QString n = QFileInfo(name).fileName();     // never a path
        static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9._+ -]+"));
        n.replace(unsafe, QStringLiteral("-"));
        return n;
    }

    QString fixtureFileName(const ShowEmbedding::Item &item)
    {
        QString n = item.manufacturer + QLatin1Char('-') + item.model;
        static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9._+-]+"));
        n.replace(unsafe, QStringLiteral("-"));
        return n + QString(KExtFixture);
    }

    QString subFolder(ShowEmbedding::Kind kind)
    {
        switch (kind)
        {
            case ShowEmbedding::FixtureDefinition: return QStringLiteral("Fixtures");
            case ShowEmbedding::EffectScript:      return QStringLiteral("EffectScripts");
            case ShowEmbedding::RGBScript:         return QStringLiteral("RGBScripts");
        }
        return QString();
    }

    /* What a channel IS, for "the same channels in the same order". */
    QString channelKind(const QLCChannel *ch)
    {
        if (ch == nullptr)
            return QString();
        QString k = QLCChannel::groupToString(ch->group());
        if (ch->group() == QLCChannel::Intensity && ch->colour() != QLCChannel::NoColour)
            k += QLatin1Char(' ') + QLCChannel::colourToString(ch->colour());
        if (ch->controlByte() == QLCChannel::LSB)
            k += QObject::tr(" (fine)");
        return k;
    }

    QString namesOf(const QList<Fixture *> &fixtures)
    {
        QStringList names;
        for (int i = 0; i < fixtures.size() && i < 4; ++i)
            names << fixtures.at(i)->name();
        if (fixtures.size() > 4)
            names << QObject::tr("%n more", "", fixtures.size() - 4);
        return names.join(QStringLiteral(", "));
    }

    QString paramKind(const EffectScript::ParamDef &d)
    {
        if (d.type == QLatin1String("path"))
            return QObject::tr("drawn path");
        if (d.type == QLatin1String("text"))
            return QObject::tr("text");
        if (!d.enumValues.isEmpty())
            return QObject::tr("list");
        return QObject::tr("number");
    }

    const EffectScript::ParamDef *paramNamed(const QList<EffectScript::ParamDef> &defs,
                                             const QString &name)
    {
        for (const EffectScript::ParamDef &d : defs)
            if (d.name == name || d.aliases.contains(name))
                return &d;
        return nullptr;
    }

    QString kindWord(ShowEmbedding::Kind kind)
    {
        switch (kind)
        {
            case ShowEmbedding::FixtureDefinition: return QObject::tr("fixture definition");
            case ShowEmbedding::EffectScript:      return QObject::tr("effect script");
            case ShowEmbedding::RGBScript:         return QObject::tr("RGB script");
        }
        return QString();
    }
}

/*****************************************************************************
 * Item
 *****************************************************************************/

QString ShowEmbedding::Item::key() const
{
    switch (kind)
    {
        case FixtureDefinition:
            return QStringLiteral("fixture:") + manufacturer + QLatin1Char('|') + model;
        case EffectScript:
            return QStringLiteral("effect:") + file;
        case RGBScript:
            return QStringLiteral("rgb:") + file;
    }
    return QString();
}

QString ShowEmbedding::Item::displayName() const
{
    QString what;
    if (kind == FixtureDefinition)
        what = manufacturer + QLatin1Char(' ') + model;
    else if (kind == RGBScript && !name.isEmpty())
        what = QStringLiteral("%1 (%2)").arg(name, file);
    else
        what = file;
    return QStringLiteral("%1 (%2)").arg(what, kindWord(kind));
}

/*****************************************************************************
 * Initialization
 *****************************************************************************/

ShowEmbedding::ShowEmbedding(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

ShowEmbedding::~ShowEmbedding()
{
    qDeleteAll(m_retired);
    m_retired.clear();
}

void ShowEmbedding::setDeleteInsteadOfTrash(bool on)
{
    s_deleteInsteadOfTrash = on;
}

QString ShowEmbedding::sha256(const QByteArray &bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

bool ShowEmbedding::isShippedScript(const QString &path, bool rgb)
{
    const QDir sys = rgb ? EffectScriptCache::systemRgbScriptsDirectory()
                         : EffectScriptCache::systemScriptsDirectory();
    if (!sys.exists())
        return false;
    if (isUnder(path, sys.absolutePath()))
        return true;
    /* A dev build links the source folder one level down. */
    const QStringList subs = sys.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &s : subs)
        if (isUnder(path, sys.absoluteFilePath(s)))
            return true;
    return false;
}

QString ShowEmbedding::workingDirectory() const
{
    return m_work ? m_work->path() : QString();
}

void ShowEmbedding::ensureWorkingDirectory()
{
    if (m_work && m_work->isValid())
        return;
    m_work.reset(new QTemporaryDir(QDir::tempPath() + QStringLiteral("/qlcconsole-show-XXXXXX")));
    if (m_doc->fixtureDefCache() != nullptr)
        m_doc->fixtureDefCache()->setShowCopiesDirectory(m_work->path());
}

QString ShowEmbedding::materialize(Item &item)
{
    ensureWorkingDirectory();
    const QString name = item.kind == FixtureDefinition ? fixtureFileName(item)
                                                        : safeFileName(item.file);
    const QString path = QDir(m_work->path()).absoluteFilePath(subFolder(item.kind) + QLatin1Char('/') + name);
    if (!writeFile(path, item.bytes))
    {
        qWarning() << Q_FUNC_INFO << "cannot write" << path;
        return QString();
    }
    item.showPath = path;
    return path;
}

ShowEmbedding::Item *ShowEmbedding::find(const QString &key)
{
    for (Item &it : m_items)
        if (it.key() == key)
            return &it;
    return nullptr;
}

bool ShowEmbedding::hasItem(const QString &key) const
{
    for (const Item &it : m_items)
        if (it.key() == key)
            return true;
    return false;
}

ShowEmbedding::Item ShowEmbedding::item(const QString &key) const
{
    for (const Item &it : m_items)
        if (it.key() == key)
            return it;
    return Item();
}

QList<ShowEmbedding::Item> ShowEmbedding::differences() const
{
    QList<Item> out;
    for (const Item &it : m_items)
        if (it.isUnreviewedDifference())
            out << it;
    return out;
}

QList<ShowEmbedding::Item> ShowEmbedding::showOnly() const
{
    QList<Item> out;
    for (const Item &it : m_items)
        if (it.state == ShowOnly && it.usingShow)
            out << it;
    return out;
}

QString ShowEmbedding::classify(const QString &path) const
{
    const QFileInfo fi(path);
    if (fi.isSymLink())
    {
        /* Linked in from the old QLC+ user folder (QLCFile::mergeLegacyUserData). */
        const QString target = fi.symLinkTarget();
        if (target.contains(QLatin1String("/QLC+/")) || target.contains(QLatin1String("/.qlcplus/"))
            || target.contains(QLatin1String("\\QLC+\\")))
            return QStringLiteral("legacy");
    }
    if (isUnder(path, QLCFixtureDefCache::showCacheDirectory().absolutePath()))
        return QStringLiteral("showcache");
    if (!m_doc->workspacePath().isEmpty() && isUnder(path, m_doc->workspacePath()))
        return QStringLiteral("showfolder");
    return QStringLiteral("user");
}

bool ShowEmbedding::isShowCopy(const QString &path) const
{
    if (m_work && isUnder(path, m_work->path()))
        return true;
    for (const Item &it : m_items)
        if (it.fromShowFolder && samePath(it.showPath, path))
            return true;
    return false;
}

bool ShowEmbedding::discard(const QString &path) const
{
    if (s_deleteInsteadOfTrash)
        return QFile::remove(path);
    return QFile::moveToTrash(path);
}

/*****************************************************************************
 * The file
 *****************************************************************************/

bool ShowEmbedding::readWorkspace(const QString &fileName)
{
    /* Not a show (a file that is not there, say): what is open stays as it
       is -- nothing read, nothing cleared. */
    if (fileName.isEmpty() || !QFileInfo(fileName).isFile())
        return false;
    QXmlStreamReader *xml = QLCFile::getXMLReader(fileName);
    if (xml == nullptr || xml->device() == nullptr || xml->hasError())
    {
        if (xml != nullptr)
            QLCFile::releaseXMLReader(xml);
        return false;
    }
    while (!xml->atEnd())
        if (xml->readNext() == QXmlStreamReader::DTD)
            break;
    if (xml->hasError() || xml->dtdName() != QLatin1String("Workspace"))
    {
        QLCFile::releaseXMLReader(xml);
        return false;
    }
    m_items.clear();
    m_problems.clear();
    if (xml->readNextStartElement())          // <Workspace>
    {
        while (xml->readNextStartElement())
        {
            if (xml->name() == KXMLShowEmbedded)
            {
                loadXML(*xml);
                break;
            }
            /* It is written before <Engine>: past that, there is none. */
            if (xml->name() == QLatin1String("Engine"))
                break;
            xml->skipCurrentElement();
        }
    }
    QLCFile::releaseXMLReader(xml);
    return true;
}

bool ShowEmbedding::loadXML(QXmlStreamReader &xml)
{
    if (xml.name() != KXMLShowEmbedded)
        return false;
    m_items.clear();
    while (xml.readNextStartElement())
    {
        Item it;
        if (xml.name() == KXMLEmbeddedFixture)
            it.kind = FixtureDefinition;
        else if (xml.name() == KXMLEmbeddedEffect)
            it.kind = EffectScript;
        else if (xml.name() == KXMLEmbeddedRGB)
            it.kind = RGBScript;
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown embedded item" << xml.name();
            xml.skipCurrentElement();
            continue;
        }
        const QXmlStreamAttributes a = xml.attributes();
        it.manufacturer = a.value(QStringLiteral("Manufacturer")).toString();
        it.model = a.value(QStringLiteral("Model")).toString();
        it.name = a.value(QStringLiteral("Name")).toString();
        it.file = a.value(QStringLiteral("File")).toString();
        it.from = a.value(QStringLiteral("From")).toString();
        it.fromModified = QDateTime::fromString(a.value(QStringLiteral("FromModified")).toString(), Qt::ISODate);
        it.embedded = QDateTime::fromString(a.value(QStringLiteral("Embedded")).toString(), Qt::ISODate);
        it.keptAgainst = a.value(QStringLiteral("KeptAgainst")).toString();
        const QString declared = a.value(QStringLiteral("SHA256")).toString();
        const bool base64 = a.value(QStringLiteral("Encoding")).toString() == QLatin1String("base64");
        const QString text = xml.readElementText();
        it.bytes = base64 ? QByteArray::fromBase64(text.toLatin1()) : text.toUtf8();
        it.sha256 = sha256(it.bytes);
        if (!declared.isEmpty() && declared.compare(it.sha256, Qt::CaseInsensitive) != 0)
        {
            qWarning() << Q_FUNC_INFO << it.displayName() << "does not match its checksum";
            m_problems << tr("The show's copy of %1 has been altered since it was saved "
                             "(its checksum does not match)").arg(it.displayName());
        }
        if ((it.kind == FixtureDefinition && (it.manufacturer.isEmpty() || it.model.isEmpty()))
            || (it.kind != FixtureDefinition && it.file.isEmpty()) || it.bytes.isEmpty())
            continue;
        if (!hasItem(it.key()))
            m_items << it;
    }
    return true;
}

void ShowEmbedding::saveXML(QXmlStreamWriter *xml) const
{
    if (xml == nullptr || m_items.isEmpty())
        return;
    xml->writeStartElement(KXMLShowEmbedded);
    for (const Item &it : m_items)
    {
        if (it.kind == FixtureDefinition)
            xml->writeStartElement(KXMLEmbeddedFixture);
        else if (it.kind == EffectScript)
            xml->writeStartElement(KXMLEmbeddedEffect);
        else
            xml->writeStartElement(KXMLEmbeddedRGB);
        if (it.kind == FixtureDefinition)
        {
            xml->writeAttribute(QStringLiteral("Manufacturer"), it.manufacturer);
            xml->writeAttribute(QStringLiteral("Model"), it.model);
        }
        if (!it.name.isEmpty())
            xml->writeAttribute(QStringLiteral("Name"), it.name);
        if (!it.file.isEmpty())
            xml->writeAttribute(QStringLiteral("File"), it.file);
        xml->writeAttribute(QStringLiteral("SHA256"), it.sha256);
        if (!it.from.isEmpty())
            xml->writeAttribute(QStringLiteral("From"), it.from);
        if (it.fromModified.isValid())
            xml->writeAttribute(QStringLiteral("FromModified"), it.fromModified.toUTC().toString(Qt::ISODate));
        if (it.embedded.isValid())
            xml->writeAttribute(QStringLiteral("Embedded"), it.embedded.toUTC().toString(Qt::ISODate));
        if (!it.keptAgainst.isEmpty())
            xml->writeAttribute(QStringLiteral("KeptAgainst"), it.keptAgainst);
        if (fitsCDATA(it.bytes))
            xml->writeCDATA(QString::fromUtf8(it.bytes));
        else
        {
            xml->writeAttribute(QStringLiteral("Encoding"), QStringLiteral("base64"));
            xml->writeCharacters(QString::fromLatin1(it.bytes.toBase64()));
        }
        xml->writeEndElement();
    }
    xml->writeEndElement();
}

/*****************************************************************************
 * Opening
 *****************************************************************************/

void ShowEmbedding::apply(const QString &showFolder)
{
    m_showFolder = showFolder;
    for (Item &it : m_items)
    {
        it.fromShowFolder = false;
        it.usingShow = false;
        if (it.kind == FixtureDefinition)
            applyFixture(it);
        else
            applyScript(it);
    }
    /* Scripts of its own, or a folder that may hold some: the script cache
       searches them (a rescan -- not paid by a show without). */
    const QString work = workingDirectory();
    if ((!work.isEmpty() && (QDir(work + QStringLiteral("/EffectScripts")).exists()
                             || QDir(work + QStringLiteral("/RGBScripts")).exists()))
        || (!showFolder.isEmpty() && (QDir(showFolder + QStringLiteral("/EffectScripts")).exists()
                                      || QDir(showFolder + QStringLiteral("/RGBScripts")).exists())))
        refreshScripts();
    if (!showFolder.isEmpty())
        applyShowFolder(showFolder);
    emit changed();
}

void ShowEmbedding::applyFixture(Item &item)
{
    QLCFixtureDefCache *cache = m_doc->fixtureDefCache();
    if (cache == nullptr)
        return;
    QLCFixtureDef *computer = cache->fixtureDef(item.manufacturer, item.model);
    if (computer == nullptr && cache->loadFromShowCache(item.manufacturer, item.model))
        computer = cache->fixtureDef(item.manufacturer, item.model);
    if (computer != nullptr)
    {
        item.computerPath = computer->definitionSourceFile();
        if (QDir::isRelativePath(item.computerPath))
            item.computerPath = QLCFixtureDefCache::systemDefinitionDirectory().absoluteFilePath(item.computerPath);
        bool ok = false;
        const QByteArray bytes = readFile(item.computerPath, &ok);
        item.computerSha = ok ? sha256(bytes) : QString();
        if (ok && item.computerSha == item.sha256)
        {
            item.state = Identical;
            return;
        }
        item.state = Different;
    }
    else
        item.state = ShowOnly;

    const QString path = materialize(item);
    QLCFixtureDef *def = new QLCFixtureDef();
    if (path.isEmpty() || def->loadXML(path) != QFile::NoError
        || def->manufacturer() != item.manufacturer || def->model() != item.model)
    {
        qWarning() << Q_FUNC_INFO << "the show's copy of" << item.displayName() << "does not load";
        m_problems << (item.state == ShowOnly
                       ? tr("The show's copy of %1 cannot be read").arg(item.displayName())
                       : tr("The show's copy of %1 cannot be read; this computer's is used")
                             .arg(item.displayName()));
        delete def;
        return;
    }
    def->setIsUser(true);
    def->setDefinitionSourceFile(path);
    def->setLoaded(true);
    QLCFixtureDef *displaced = cache->substitute(def);
    m_showDefs.insert(item.key(), def);
    if (displaced != nullptr)
        m_displaced.insert(item.key(), displaced);
    item.usingShow = true;
}

void ShowEmbedding::applyScript(Item &item)
{
    const bool rgb = item.kind == RGBScript;
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    const QString base = QFileInfo(item.file).completeBaseName();

    // This computer's copy: the file a look would find by this name.
    QString computer;
    if (runner != nullptr)
    {
        computer = runner->cache()->scriptPath(rgb ? QStringLiteral("rgb:") + base : base);
        if (QFileInfo(computer).fileName() != item.file)
            computer.clear();
    }
    if (computer.isEmpty() && rgb && !item.name.isEmpty() && m_doc->rgbScriptsCache() != nullptr)
        computer = m_doc->rgbScriptsCache()->scriptFile(item.name);

    if (!computer.isEmpty())
    {
        item.computerPath = computer;
        bool ok = false;
        const QByteArray bytes = readFile(computer, &ok);
        item.computerSha = ok ? sha256(bytes) : QString();
        if (ok && item.computerSha == item.sha256)
        {
            item.state = Identical;
            return;
        }
        item.state = Different;
    }
    else
        item.state = ShowOnly;

    if (materialize(item).isEmpty())
        return;
    if (rgb && !item.name.isEmpty() && m_doc->rgbScriptsCache() != nullptr)
        m_doc->rgbScriptsCache()->setShowScript(item.name, item.showPath);
    item.usingShow = true;
}

void ShowEmbedding::applyShowFolder(const QString &showFolder)
{
    const QDir root(showFolder);
    QLCFixtureDefCache *cache = m_doc->fixtureDefCache();

    /* Definitions: what neither the show nor this computer has. */
    const QDir fixtures(root.absoluteFilePath(QStringLiteral("Fixtures")));
    if (cache != nullptr && fixtures.exists())
    {
        const QStringList files = fixtures.entryList(QStringList() << QStringLiteral("*") + KExtFixture,
                                                     QDir::Files | QDir::Readable);
        for (const QString &f : files)
        {
            const QString path = fixtures.absoluteFilePath(f);
            QLCFixtureDef *def = new QLCFixtureDef();
            if (QLCFile::isOnlineOnly(path) || def->loadXML(path) != QFile::NoError)
            {
                delete def;
                continue;
            }
            Item it;
            it.kind = FixtureDefinition;
            it.manufacturer = def->manufacturer();
            it.model = def->model();
            if (hasItem(it.key()) || cache->fixtureDef(it.manufacturer, it.model) != nullptr
                || cache->loadFromShowCache(it.manufacturer, it.model))
            {
                delete def;
                continue;
            }
            def->setIsUser(true);
            def->setDefinitionSourceFile(path);
            def->setLoaded(true);
            cache->substitute(def);
            m_showDefs.insert(it.key(), def);
            it.file = f;
            it.bytes = readFile(path);
            it.sha256 = sha256(it.bytes);
            it.from = QStringLiteral("showfolder");
            it.fromModified = QFileInfo(path).lastModified();
            it.embedded = QDateTime::currentDateTimeUtc();
            it.fromShowFolder = true;
            it.showPath = path;
            it.state = ShowOnly;
            it.usingShow = true;
            m_items << it;
        }
    }

    /* RGB Matrix scripts, by the name a matrix asks for. */
    const QDir rgbDir(root.absoluteFilePath(QStringLiteral("RGBScripts")));
    RGBScriptsCache *rgbCache = m_doc->rgbScriptsCache();
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    if (rgbDir.exists())
    {
        const QStringList files = rgbDir.entryList(QStringList() << QStringLiteral("*.js"), QDir::Files | QDir::Readable);
        for (const QString &f : files)
        {
            const QString path = rgbDir.absoluteFilePath(f);
            Item it;
            it.kind = RGBScript;
            it.file = f;
            it.name = RGBScriptsCache::scriptNameIn(path);
            if (hasItem(it.key()))
                continue;
            bool known = false;
            if (rgbCache != nullptr && !it.name.isEmpty())
            {
                known = !rgbCache->scriptFile(it.name).isEmpty();
                if (!known)
                    rgbCache->setShowScript(it.name, path);
            }
            if (runner != nullptr
                && !samePath(runner->cache()->scriptPath(QStringLiteral("rgb:") + QFileInfo(f).completeBaseName()), path))
                known = true;
            if (known)
                continue;
            it.bytes = readFile(path);
            it.sha256 = sha256(it.bytes);
            it.from = QStringLiteral("showfolder");
            it.fromModified = QFileInfo(path).lastModified();
            it.embedded = QDateTime::currentDateTimeUtc();
            it.fromShowFolder = true;
            it.showPath = path;
            it.state = ShowOnly;
            it.usingShow = true;
            m_items << it;
        }
    }

    /* Effect scripts: the cache searches the show's folder last, so one
       found there is one this computer does not have. */
    const QDir fxDir(root.absoluteFilePath(QStringLiteral("EffectScripts")));
    if (runner != nullptr && fxDir.exists())
    {
        const QStringList files = fxDir.entryList(QStringList() << QStringLiteral("*.js"), QDir::Files | QDir::Readable);
        for (const QString &f : files)
        {
            const QString path = fxDir.absoluteFilePath(f);
            Item it;
            it.kind = EffectScript;
            it.file = f;
            if (hasItem(it.key())
                || !samePath(runner->cache()->scriptPath(QFileInfo(f).completeBaseName()), path))
                continue;
            it.bytes = readFile(path);
            it.sha256 = sha256(it.bytes);
            it.from = QStringLiteral("showfolder");
            it.fromModified = QFileInfo(path).lastModified();
            it.embedded = QDateTime::currentDateTimeUtc();
            it.fromShowFolder = true;
            it.showPath = path;
            it.state = ShowOnly;
            it.usingShow = true;
            m_items << it;
        }
    }
}

void ShowEmbedding::refreshScripts()
{
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    if (runner == nullptr)
        return;
    auto existing = [](const QString &base, const QString &sub) {
        if (base.isEmpty())
            return QString();
        const QString p = QDir(base).absoluteFilePath(sub);
        return QDir(p).exists() ? p : QString();
    };
    const QString work = workingDirectory();
    m_scriptDirsSet = true;
    runner->cache()->setShowDirectories(existing(work, QStringLiteral("EffectScripts")),
                                        existing(work, QStringLiteral("RGBScripts")),
                                        existing(m_showFolder, QStringLiteral("EffectScripts")),
                                        existing(m_showFolder, QStringLiteral("RGBScripts")));
}

void ShowEmbedding::restoreComputer()
{
    const bool hadScripts = m_scriptDirsSet;
    QLCFixtureDefCache *cache = m_doc->fixtureDefCache();
    for (auto it = m_showDefs.constBegin(); it != m_showDefs.constEnd(); ++it)
    {
        QLCFixtureDef *mine = m_displaced.value(it.key());
        if (cache != nullptr)
        {
            if (mine != nullptr)
                cache->substitute(mine);
            else
                cache->take(it.value());
        }
        m_retired << it.value();
    }
    m_showDefs.clear();
    m_displaced.clear();
    if (m_doc->rgbScriptsCache() != nullptr)
        m_doc->rgbScriptsCache()->clearShowScripts();
    if (cache != nullptr)
        cache->setShowCopiesDirectory(QString());
    m_items.clear();
    m_problems.clear();
    m_work.reset();
    m_showFolder.clear();
    if (hadScripts)
    {
        refreshScripts();           // none now: this computer's alone
        m_scriptDirsSet = false;
    }
    emit changed();
}

/*****************************************************************************
 * Saving
 *****************************************************************************/

void ShowEmbedding::collect()
{
    QList<Item> out;
    QSet<QString> seen;
    const QDateTime now = QDateTime::currentDateTimeUtc();

    auto add = [&](Item fresh, const QString &path) {
        const QString key = fresh.key();
        if (seen.contains(key))
            return;
        seen.insert(key);
        const Item *old = nullptr;
        for (const Item &o : m_items)
            if (o.key() == key)
                old = &o;
        bool ok = false;
        const QByteArray bytes = readFile(path, &ok);
        if (!ok || bytes.isEmpty())
        {
            if (old != nullptr)
                out << *old;            // not to hand: what the show had
            return;
        }
        const QString sha = sha256(bytes);
        if (old != nullptr && (isShowCopy(path) || old->sha256 == sha))
        {
            Item it = *old;
            if (it.sha256 != sha)       // the show's copy was edited
            {
                it.bytes = bytes;
                it.sha256 = sha;
                it.embedded = now;
            }
            out << it;
            return;
        }
        fresh.bytes = bytes;
        fresh.sha256 = sha;
        fresh.file = QFileInfo(path).fileName();
        fresh.from = classify(path);
        fresh.fromModified = QFileInfo(path).lastModified();
        fresh.embedded = now;
        // This computer's, in use: the show now has the same.
        fresh.state = Identical;
        fresh.computerPath = path;
        fresh.computerSha = sha;
        fresh.usingShow = false;
        out << fresh;
    };

    /* Fixture definitions not shipped with the app. */
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr)
            continue;
        if (fx->definitionUnavailable())
        {
            // Standing in: whatever copy the show had, it keeps.
            Item it;
            it.manufacturer = fx->unavailableManufacturer();
            it.model = fx->unavailableModel();
            const QString key = it.key();
            if (!seen.contains(key))
            {
                seen.insert(key);
                for (const Item &o : m_items)
                    if (o.key() == key)
                        out << o;
            }
            continue;
        }
        const QLCFixtureDef *def = fx->fixtureDef();
        if (def == nullptr || def->source() == QLCFixtureDef::Included
            || def->manufacturer() == KXMLFixtureGeneric)
            continue;
        Item it;
        it.kind = FixtureDefinition;
        it.manufacturer = def->manufacturer();
        it.model = def->model();
        add(it, def->definitionSourceFile());
    }

    /* Scripts the looks use. */
    foreach (QLCPalette *p, m_doc->palettes())
    {
        if (p == nullptr || p->type() != QLCPalette::Effect || p->scriptPath().isEmpty())
            continue;
        const QString path = p->scriptPath();
        Item it;
        if (!QFileInfo(path).isAbsolute() || !QFile::exists(path))
        {
            // Not found on load: keep the show's copy of it, if it had one.
            const bool rgb = path.startsWith(QLatin1String("rgb:"));
            it.kind = rgb ? RGBScript : EffectScript;
            it.file = QFileInfo(rgb ? path.mid(4) : path).completeBaseName() + QStringLiteral(".js");
            if (!seen.contains(it.key()))
            {
                seen.insert(it.key());
                for (const Item &o : m_items)
                    if (o.key() == it.key())
                        out << o;
            }
            continue;
        }
        const bool rgb = EffectScriptCache::isRgbScriptPath(path);
        if (isShippedScript(path, rgb) && !isShowCopy(path))
            continue;
        it.kind = rgb ? RGBScript : EffectScript;
        it.file = QFileInfo(path).fileName();
        if (rgb)
            it.name = RGBScriptsCache::scriptNameIn(path);
        add(it, path);
    }

    /* Scripts the RGB matrices use. */
    foreach (Function *f, m_doc->functionsByType(Function::RGBMatrixType))
    {
        RGBMatrix *m = qobject_cast<RGBMatrix *>(f);
        if (m == nullptr || m->algorithm() == nullptr || m->algorithm()->type() != RGBAlgorithm::Script)
            continue;
        const ::RGBScript *s = static_cast<const ::RGBScript *>(m->algorithm());
        const QString path = s->fileName();
        if (path.isEmpty() || (isShippedScript(path, true) && !isShowCopy(path)))
            continue;
        Item it;
        it.kind = RGBScript;
        it.file = QFileInfo(path).fileName();
        it.name = s->name();
        add(it, path);
    }

    m_items = out;
    emit changed();
}

/*****************************************************************************
 * Differences
 *****************************************************************************/

ShowEmbedding::Newer ShowEmbedding::newer(const Item &item) const
{
    if (!item.fromModified.isValid() || item.computerPath.isEmpty())
        return NewerUnknown;
    const QDateTime here = QFileInfo(item.computerPath).lastModified();
    if (!here.isValid())
        return NewerUnknown;
    const qint64 secs = item.fromModified.secsTo(here);
    if (secs > 2)
        return ComputerNewer;
    if (secs < -2)
        return ShowNewer;
    return NewerUnknown;
}

QString ShowEmbedding::newerText(Newer n)
{
    switch (n)
    {
        case ComputerNewer: return tr("Updated on this computer since the show was saved");
        case ShowNewer:     return tr("This computer has an older copy");
        case NewerUnknown:  break;
    }
    return tr("Can't tell which is newer");
}

ShowEmbedding::Compatibility ShowEmbedding::compatibility(const QString &key) const
{
    Compatibility c;
    const Item it = item(key);
    if (!it.usingShow || it.computerPath.isEmpty())
        return c;

    if (it.kind == FixtureDefinition)
    {
        const QLCFixtureDef *from = m_showDefs.value(key);
        const QLCFixtureDef *to = m_displaced.value(key);
        if (from == nullptr || to == nullptr)
            return c;
        QMap<QString, QList<Fixture *> > byMode;
        foreach (Fixture *fx, m_doc->fixtures())
            if (fx != nullptr && fx->fixtureDef() == from && fx->fixtureMode() != nullptr)
                byMode[fx->fixtureMode()->name()] << fx;
        for (auto m = byMode.constBegin(); m != byMode.constEnd(); ++m)
        {
            const QLCFixtureMode *a = const_cast<QLCFixtureDef *>(from)->mode(m.key());
            const QLCFixtureMode *b = const_cast<QLCFixtureDef *>(to)->mode(m.key());
            const int n = m.value().size();
            if (b == nullptr)
            {
                c.problems << tr("Mode “%1” is not in this computer's copy: %n fixture(s) would "
                                 "lose their mode (%2)", "", n).arg(m.key(), namesOf(m.value()));
                continue;
            }
            if (a == nullptr)
                continue;
            if (a->channels().size() != b->channels().size())
            {
                c.problems << tr("Mode “%1” is %2 channels here, %3 in the show: %n fixture(s) "
                                 "would need re-patching (%4)", "", n)
                                  .arg(m.key()).arg(b->channels().size()).arg(a->channels().size())
                                  .arg(namesOf(m.value()));
                continue;
            }
            for (int i = 0; i < a->channels().size(); ++i)
            {
                const QString ka = channelKind(a->channel(quint32(i)));
                const QString kb = channelKind(b->channel(quint32(i)));
                if (ka != kb)
                {
                    c.problems << tr("Mode “%1” channel %2 is %3 here, %4 in the show: %n "
                                     "fixture(s) would do something else (%5)", "", n)
                                      .arg(m.key()).arg(i + 1).arg(kb, ka, namesOf(m.value()));
                    break;
                }
            }
        }
        c.compatible = c.problems.isEmpty();
        return c;
    }

    /* Scripts: the parameters the looks set. */
    bool lookUses = false;
    foreach (QLCPalette *p, m_doc->palettes())
        if (p != nullptr && p->type() == QLCPalette::Effect && samePath(p->scriptPath(), it.showPath))
            lookUses = true;
    if (lookUses)
    {
        ::EffectScript a, b;
        a.load(it.showPath);
        if (!b.load(it.computerPath))
            c.problems << tr("This computer's copy does not run: %1").arg(b.lastError());
        else
        {
            foreach (QLCPalette *p, m_doc->palettes())
            {
                if (p == nullptr || p->type() != QLCPalette::Effect || !samePath(p->scriptPath(), it.showPath))
                    continue;
                QStringList set = p->effectParamValues().keys();
                set << p->effectStringParams().keys();
                set.removeDuplicates();
                for (const QString &name : set)
                {
                    const ::EffectScript::ParamDef *da = paramNamed(a.paramDefs(), name);
                    if (da == nullptr)
                        continue;           // not the show's either: nothing to lose
                    const ::EffectScript::ParamDef *db = paramNamed(b.paramDefs(), name);
                    if (db == nullptr)
                        c.problems << tr("Look “%1”: its setting “%2” is not in this computer's copy")
                                          .arg(p->name(), name);
                    else if (paramKind(*da) != paramKind(*db))
                        c.problems << tr("Look “%1”: “%2” is a %3 in this computer's copy, a %4 in the show's")
                                          .arg(p->name(), name, paramKind(*db), paramKind(*da));
                }
            }
        }
    }

    if (it.kind == RGBScript)
    {
        QList<RGBMatrix *> matrices;
        foreach (Function *f, m_doc->functionsByType(Function::RGBMatrixType))
        {
            RGBMatrix *m = qobject_cast<RGBMatrix *>(f);
            if (m != nullptr && m->algorithm() != nullptr && m->algorithm()->type() == RGBAlgorithm::Script
                && samePath(static_cast<const ::RGBScript *>(m->algorithm())->fileName(), it.showPath))
                matrices << m;
        }
        if (!matrices.isEmpty())
        {
            ::RGBScript a(m_doc), b(m_doc);
            a.load(it.showPath);
            if (!b.load(it.computerPath))
                c.problems << tr("This computer's copy does not load");
            else
            {
                const QList<RGBScriptProperty> pa = a.properties(), pb = b.properties();
                for (RGBMatrix *m : matrices)
                {
                    const QStringList set = m->properties().keys();
                    for (const QString &name : set)
                    {
                        const RGBScriptProperty *fa = nullptr, *fb = nullptr;
                        for (const RGBScriptProperty &p : pa)
                            if (p.m_name == name)
                                fa = &p;
                        for (const RGBScriptProperty &p : pb)
                            if (p.m_name == name)
                                fb = &p;
                        if (fa == nullptr)
                            continue;
                        if (fb == nullptr)
                            c.problems << tr("RGB matrix “%1”: its setting “%2” is not in this computer's copy")
                                              .arg(m->name(), name);
                        else if (fa->m_type != fb->m_type)
                            c.problems << tr("RGB matrix “%1”: “%2” is a different kind of setting "
                                             "in this computer's copy").arg(m->name(), name);
                    }
                }
            }
        }
    }
    c.compatible = c.problems.isEmpty();
    return c;
}

/*****************************************************************************
 * The operator's choices
 *****************************************************************************/

void ShowEmbedding::repointScripts(const QString &fromPath, const QString &toPath, const Item &item)
{
    QSet<quint32> changedLooks;
    foreach (QLCPalette *p, m_doc->palettes())
    {
        if (p != nullptr && p->type() == QLCPalette::Effect && samePath(p->scriptPath(), fromPath))
        {
            p->setScriptPath(toPath);
            changedLooks.insert(p->id());
        }
    }
    if (!changedLooks.isEmpty() && m_doc->effectScriptRunner() != nullptr)
    {
        foreach (Function *f, m_doc->functionsByType(Function::SceneType))
        {
            Scene *s = qobject_cast<Scene *>(f);
            if (s == nullptr || !s->isRunning())
                continue;
            foreach (quint32 pid, s->palettes())
                if (changedLooks.contains(pid))
                {
                    m_doc->effectScriptRunner()->syncScene(s->id());
                    break;
                }
        }
    }
    if (item.kind == RGBScript && m_doc->rgbScriptsCache() != nullptr)
    {
        foreach (Function *f, m_doc->functionsByType(Function::RGBMatrixType))
        {
            RGBMatrix *m = qobject_cast<RGBMatrix *>(f);
            if (m == nullptr || m->algorithm() == nullptr || m->algorithm()->type() != RGBAlgorithm::Script
                || !samePath(static_cast<const ::RGBScript *>(m->algorithm())->fileName(), fromPath))
                continue;
            const QString name = m->algorithm()->name();
            const QMap<QString, QString> props = m->properties();
            m->setAlgorithm(m_doc->rgbScriptsCache()->script(name));
            for (auto it = props.constBegin(); it != props.constEnd(); ++it)
                m->setProperty(it.key(), it.value());
        }
    }
}

bool ShowEmbedding::useComputers(const QString &key, QString *error)
{
    Item *it = find(key);
    if (it == nullptr || !it->usingShow)
        return true;
    if (it->state == ShowOnly || it->computerPath.isEmpty())
    {
        if (error != nullptr)
            *error = tr("This computer has no copy of %1.").arg(it->displayName());
        return false;
    }

    if (it->kind == FixtureDefinition)
    {
        QLCFixtureDef *from = m_showDefs.value(key);
        QLCFixtureDef *to = m_displaced.value(key);
        if (from == nullptr || to == nullptr)
        {
            if (error != nullptr)
                *error = tr("This computer has no copy of %1.").arg(it->displayName());
            return false;
        }
        /* Every fixture has to fit: its mode there, and its channels where it
           is patched -- a mode that grew would run into the next fixture. */
        QList<Fixture *> users;
        QStringList blocked;
        foreach (Fixture *fx, m_doc->fixtures())
        {
            if (fx == nullptr || fx->fixtureDef() != from || fx->fixtureMode() == nullptr)
                continue;
            users << fx;
            const QLCFixtureMode *mode = to->mode(fx->fixtureMode()->name());
            if (mode == nullptr)
            {
                blocked << tr("%1: this computer's copy has no mode “%2”")
                               .arg(fx->name(), fx->fixtureMode()->name());
                continue;
            }
            const quint32 now = fx->channels();
            const quint32 then = quint32(mode->channels().size());
            if (!fx->isPatched() || then <= now)
                continue;
            if (!fx->crossUniverse() && fx->address() + then > 512)
            {
                blocked << tr("%1: %2 channels would run past the end of the universe")
                               .arg(fx->name()).arg(then);
                continue;
            }
            for (quint32 ch = now; ch < then; ++ch)
            {
                const quint32 other = m_doc->fixtureForAddress(fx->universeAddress() + ch);
                if (other != Fixture::invalidId() && other != fx->id())
                {
                    Fixture *o = m_doc->fixture(other);
                    blocked << tr("%1: %2 channels would overlap %3")
                                   .arg(fx->name()).arg(then).arg(o != nullptr ? o->name() : QString::number(other));
                    break;
                }
            }
        }
        if (!blocked.isEmpty())
        {
            if (error != nullptr)
                *error = tr("These fixtures need re-patching before they can use this computer's "
                            "copy:\n\n%1").arg(blocked.join(QLatin1Char('\n')));
            return false;
        }
        QLCFixtureDefCache *cache = m_doc->fixtureDefCache();
        cache->substitute(to);
        m_retired << from;
        m_showDefs.remove(key);
        m_displaced.remove(key);
        for (Fixture *fx : users)
            fx->setFixtureDefinition(to, to->mode(fx->fixtureMode()->name()));
    }
    else
    {
        const QString show = it->showPath;
        if (!it->fromShowFolder)
            QFile::remove(show);
        if (it->kind == RGBScript && !it->name.isEmpty() && m_doc->rgbScriptsCache() != nullptr)
            m_doc->rgbScriptsCache()->setShowScript(it->name, QString());
        refreshScripts();
        repointScripts(show, it->computerPath, *it);
    }

    it->usingShow = false;
    m_doc->setModified();
    emit changed();
    return true;
}

void ShowEmbedding::keepShows(const QString &key)
{
    Item *it = find(key);
    if (it == nullptr || it->keptAgainst == it->computerSha)
        return;
    it->keptAgainst = it->computerSha;
    m_doc->setModified();
    emit changed();
}

bool ShowEmbedding::installShows(const QString &key, QString *error)
{
    Item *it = find(key);
    if (it == nullptr)
        return false;
    if (!it->usingShow)
    {
        if (error != nullptr)
            *error = tr("%1: this computer's copy is the one in use.").arg(it->displayName());
        return false;
    }

    QDir userDir;
    if (it->kind == FixtureDefinition)
        userDir = QLCFixtureDefCache::userDefinitionDirectory();
    else if (it->kind == EffectScript)
        userDir = EffectScriptCache::userScriptsDirectory();
    else
        userDir = RGBScriptsCache::userScriptsDirectory();
    userDir.mkpath(QStringLiteral("."));

    /* Over this computer's own file when it is one of the user's; beside the
       app's (a user copy replaces a shipped one), or new, in the user folder. */
    QString target;
    if (!it->computerPath.isEmpty() && isUnder(it->computerPath, userDir.absolutePath()))
        target = QDir(userDir.absolutePath()).absoluteFilePath(QFileInfo(it->computerPath).fileName());
    else
    {
        QString name = safeFileName(it->file);
        if (it->kind == FixtureDefinition && !name.endsWith(KExtFixture, Qt::CaseInsensitive))
            name = fixtureFileName(*it);
        target = userDir.absoluteFilePath(name);
        /* A definition this computer does not have, whose file name another
           one has: beside it, not over it. (A script's file name is its name,
           so one there is the same script.) */
        if (it->kind == FixtureDefinition)
        {
            const QString stem = QFileInfo(name).completeBaseName();
            for (int n = 2; QFileInfo(target).exists() || QFileInfo(target).isSymLink(); ++n)
                target = userDir.absoluteFilePath(QStringLiteral("%1 %2%3").arg(stem).arg(n).arg(KExtFixture));
        }
    }

    const QFileInfo there(target);
    if ((there.exists() || there.isSymLink()) && !discard(target))
    {
        if (error != nullptr)
            *error = tr("Could not move %1 to the Trash.").arg(target);
        return false;
    }
    if (!writeFile(target, it->bytes))
    {
        if (error != nullptr)
            *error = tr("Could not write %1.").arg(target);
        return false;
    }

    if (it->kind == FixtureDefinition)
    {
        /* The definition in use is now this computer's: it is the same. */
        if (QLCFixtureDef *def = m_showDefs.take(key))
            def->setDefinitionSourceFile(target);
        if (QLCFixtureDef *old = m_displaced.take(key))
            m_retired << old;
    }
    else
    {
        const QString show = it->showPath;
        if (!it->fromShowFolder)
            QFile::remove(show);
        if (it->kind == RGBScript && m_doc->rgbScriptsCache() != nullptr)
        {
            if (!it->name.isEmpty())
                m_doc->rgbScriptsCache()->setShowScript(it->name, QString());
            m_doc->rgbScriptsCache()->load(RGBScriptsCache::userScriptsDirectory());
        }
        refreshScripts();
        repointScripts(show, target, *it);
    }

    it->state = Identical;
    it->usingShow = false;
    it->computerPath = target;
    it->computerSha = it->sha256;
    emit changed();
    return true;
}

/*****************************************************************************
 * Collect for Transfer
 *****************************************************************************/

bool ShowEmbedding::embedsIn(const QString &fileName)
{
    return fileName.endsWith(KExtWorkspaceConsole, Qt::CaseInsensitive);
}

bool ShowEmbedding::writeTransferFolder(const QString &folder, QStringList *written) const
{
    const QDir root(folder);
    bool ok = true;
    auto put = [&](const QString &rel, const QByteArray &bytes) {
        const QString path = root.absoluteFilePath(rel);
        if (writeFile(path, bytes))
        {
            if (written != nullptr)
                *written << path;
        }
        else
            ok = false;
    };
    for (const Item &it : m_items)
    {
        QString name = safeFileName(it.file);
        if (it.kind == FixtureDefinition && !name.endsWith(KExtFixture, Qt::CaseInsensitive))
            name = fixtureFileName(it);
        put(subFolder(it.kind) + QLatin1Char('/') + name, it.bytes);
    }
    if (m_doc->monitorProperties() != nullptr)
        foreach (const HeadLayout &l, m_doc->monitorProperties()->headLayouts())
            if (l.isValid())
                put(QStringLiteral("HeadLayouts/") + HeadLayoutLibrary::fileNameFor(l.key),
                    HeadLayoutLibrary::toJson(l));
    return ok;
}

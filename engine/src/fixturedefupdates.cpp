/*
  Q Light Controller Plus - qlcconsole
  fixturedefupdates.cpp

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

#include <QCryptographicHash>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QSettings>
#include <QLocale>
#include <QFileInfo>
#include <QFile>
#include <QSet>
#include <QMap>
#include <QDebug>

#include "fixturedefupdates.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "showembedding.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "efxfixture.h"
#include "fixturegroup.h"
#include "rgbmatrix.h"
#include "qlcfile.h"
#include "fixture.h"
#include "scene.h"
#include "efx.h"
#include "doc.h"

#define SETTINGS_GROUP QStringLiteral("FixtureDefUpdates")
#define MANIFEST QStringLiteral("manifest.json")

namespace FixtureDefUpdates
{

namespace
{
    QString sha256(const QByteArray &bytes)
    {
        return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    }

    bool samePath(const QString &a, const QString &b)
    {
        if (a.isEmpty() || b.isEmpty())
            return false;
        if (QDir::cleanPath(QFileInfo(a).absoluteFilePath()) == QDir::cleanPath(QFileInfo(b).absoluteFilePath()))
            return true;
        const QString ra = QFileInfo(a).canonicalFilePath();
        return !ra.isEmpty() && ra == QFileInfo(b).canonicalFilePath();
    }

    /* Whole or not at all -- and never through a link: @p path is the file
       itself. */
    bool writeFile(const QString &path, const QByteArray &bytes)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile f(path);
        f.setDirectWriteFallback(false);
        if (!f.open(QIODevice::WriteOnly))
            return false;
        if (f.write(bytes) != bytes.size())
        {
            f.cancelWriting();
            return false;
        }
        return f.commit();
    }

    void parseHeader(Info &i)
    {
        QXmlStreamReader xml(i.bytes);
        if (!xml.readNextStartElement() || xml.name() != QLatin1String("FixtureDefinition"))
            return;
        while (xml.readNextStartElement())
        {
            if (xml.name() == KXMLQLCCreator)
            {
                while (xml.readNextStartElement())
                {
                    if (xml.name() == KXMLQLCCreatorName)
                        i.creatorName = xml.readElementText().trimmed();
                    else if (xml.name() == KXMLQLCCreatorVersion)
                        i.creatorVersion = xml.readElementText().trimmed();
                    else if (xml.name() == KXMLQLCCreatorAuthor)
                        i.author = xml.readElementText().trimmed();
                    else
                        xml.skipCurrentElement();
                }
            }
            else if (xml.name() == QLatin1String("Manufacturer"))
                i.manufacturer = xml.readElementText().trimmed();
            else if (xml.name() == QLatin1String("Model"))
            {
                i.model = xml.readElementText().trimmed();
                return;             // all there is to know before the channels
            }
            else
                xml.skipCurrentElement();
        }
    }

    bool parseVersion(const QString &s, int v[4])
    {
        static const QRegularExpression re(QStringLiteral("(\\d+)\\.(\\d+)(?:\\.(\\d+))?"));
        static const QRegularExpression build(QStringLiteral("qlcconsole\\.(\\d+)"));
        const QRegularExpressionMatch m = re.match(s);
        if (!m.hasMatch())
            return false;
        v[0] = m.captured(1).toInt();
        v[1] = m.captured(2).toInt();
        v[2] = m.captured(3).isEmpty() ? 0 : m.captured(3).toInt();
        const QRegularExpressionMatch b = build.match(s);
        v[3] = b.hasMatch() ? b.captured(1).toInt() : -1;
        return true;
    }

    bool sameAuthor(const QString &a, const QString &b)
    {
        static const QRegularExpression notAlnum(QStringLiteral("[^a-z0-9]"));
        QString x = a.toLower(), y = b.toLower();
        x.remove(notAlnum);
        y.remove(notAlnum);
        return x.isEmpty() || y.isEmpty() || x == y;
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

    /* @p mine has a channel or a mode @p current does not: made on purpose. */
    bool hasExtras(const QLCFixtureDef *mine, const QLCFixtureDef *current)
    {
        QLCFixtureDef *m = const_cast<QLCFixtureDef *>(mine);
        QLCFixtureDef *c = const_cast<QLCFixtureDef *>(current);
        foreach (QLCChannel *ch, m->channels())
            if (c->channel(ch->name()) == nullptr)
                return true;
        foreach (QLCFixtureMode *mode, m->modes())
            if (c->mode(mode->name()) == nullptr)
                return true;
        return false;
    }

    QString settingsKey(const QString &what, const QString &path)
    {
        const QString canon = QFileInfo(path).absoluteFilePath();
        return SETTINGS_GROUP + QLatin1Char('/') + what + QLatin1Char('/')
               + sha256(canon.toUtf8()).left(20);
    }

    QLCFixtureDef *cachedFor(QLCFixtureDefCache *cache, const QString &path)
    {
        if (cache == nullptr)
            return nullptr;
        foreach (QLCFixtureDef *def, cache->definitions())
            if (def != nullptr && def->source() == QLCFixtureDef::Local
                && samePath(def->definitionSourceFile(), path))
                return def;
        return nullptr;
    }

    /* The file @p path now holds a different definition: the cache's copy
       of it replaced (the show's fixtures switched to it, mode by mode), the
       duplicates and the show's comparison brought up to date. */
    void reloadFile(Doc *doc, const QString &path, QLCFixtureDef *fresh)
    {
        QLCFixtureDefCache *cache = doc != nullptr ? doc->fixtureDefCache() : nullptr;
        if (cache == nullptr)
        {
            delete fresh;
            return;
        }
        QLCFixtureDef *old = cachedFor(cache, path);
        if (old != nullptr)
        {
            if (fresh == nullptr)
            {
                fresh = new QLCFixtureDef();
                if (fresh->loadXML(path) != QFile::NoError)
                {
                    delete fresh;
                    fresh = nullptr;
                }
            }
            if (fresh != nullptr)
            {
                fresh->setIsUser(true);
                fresh->setDefinitionSourceFile(old->definitionSourceFile());
                fresh->setLoaded(true);
                fresh->setOverridesIncluded(old->overridesIncluded());
                if (!remap(doc, old, fresh))
                    delete fresh;
            }
        }
        else
            delete fresh;
        cache->noteFileChanged(path);
        if (doc->showEmbedding() != nullptr)
            doc->showEmbedding()->computerFileChanged(path);
    }

    QString uniqueIn(const QDir &dir, const QString &name)
    {
        QString path = dir.absoluteFilePath(name);
        const QString stem = QFileInfo(name).completeBaseName();
        const QString ext = QFileInfo(name).suffix();
        for (int n = 2; QFileInfo::exists(path) || QFileInfo(path).isSymLink(); ++n)
            path = dir.absoluteFilePath(QStringLiteral("%1 (%2).%3").arg(stem).arg(n).arg(ext));
        return path;
    }

    QJsonObject readManifest(const QString &folder)
    {
        QFile f(QDir(folder).absoluteFilePath(MANIFEST));
        if (!f.open(QIODevice::ReadOnly))
            return QJsonObject();
        return QJsonDocument::fromJson(f.readAll()).object();
    }

    bool writeManifest(const QString &folder, const QJsonObject &o)
    {
        return writeFile(QDir(folder).absoluteFilePath(MANIFEST),
                         QJsonDocument(o).toJson(QJsonDocument::Indented));
    }
}

/*****************************************************************************
 * Reading and comparing
 *****************************************************************************/

QString Info::versionText() const
{
    QStringList parts;
    if (!creatorVersion.isEmpty())
        parts << creatorVersion;
    if (modified.isValid() && !shipped)
        parts << QLocale().toString(modified.date(), QLocale::ShortFormat);
    if (shipped)
        parts << QObject::tr("shipped");
    return parts.isEmpty() ? QObject::tr("unknown") : parts.join(QStringLiteral(", "));
}

Info readInfo(const QString &path, bool shipped)
{
    Info i;
    i.path = path;
    i.shipped = shipped;
    const QFileInfo fi(path);
    if (fi.isSymLink())
    {
        i.isLink = true;
        i.linkTarget = fi.symLinkTarget();
        i.linkIntoLegacy = i.linkTarget.contains(QLatin1String("/QLC+/"))
                           || i.linkTarget.contains(QLatin1String("/.qlcplus/"))
                           || i.linkTarget.contains(QLatin1String("\\QLC+\\"));
        i.realPath = fi.canonicalFilePath();
    }
    else
        i.realPath = fi.absoluteFilePath();
    if (i.realPath.isEmpty() || QLCFile::isOnlineOnly(i.realPath))
        return i;
    QFile f(i.realPath);
    if (!f.open(QIODevice::ReadOnly))
        return i;
    i.bytes = f.readAll();
    i.sha256 = sha256(i.bytes);
    i.modified = QFileInfo(i.realPath).lastModified();
    parseHeader(i);
    return i;
}

Info infoFromBytes(const QByteArray &bytes)
{
    Info i;
    i.bytes = bytes;
    i.sha256 = sha256(bytes);
    parseHeader(i);
    return i;
}

int compareVersions(const QString &a, const QString &b, bool *known)
{
    int va[4], vb[4];
    const bool ok = parseVersion(a, va) && parseVersion(b, vb);
    if (known != nullptr)
        *known = ok;
    if (!ok)
        return 0;
    int base = 0;
    for (int i = 0; i < 3 && base == 0; ++i)
        if (va[i] != vb[i])
            base = va[i] < vb[i] ? -1 : 1;
    /* A qlcconsole build carries the QLC+ release it grew from (4.14.2):
       a later QLC+ (4.14.4) is a different line, not a newer one -- unless
       it is the same release or an older one. */
    const bool qa = va[3] >= 0, qb = vb[3] >= 0;
    if (qa != qb)
    {
        const int plainVsBuild = qa ? -base : base;   // plain's release against the build's
        if (plainVsBuild <= 0)
            return qa ? 1 : -1;     // the build grew from it or after it
        if (known != nullptr)
            *known = false;
        return 0;
    }
    if (base != 0)
        return base;
    return va[3] == vb[3] ? 0 : (va[3] < vb[3] ? -1 : 1);
}

int compare(const Info &a, const Info &b)
{
    bool known = false;
    const int v = compareVersions(a.creatorVersion, b.creatorVersion, &known);
    if (known && v != 0)
        return v;
    /* The same version: between the user's own files, the one changed last.
       A shipped file's time says when the app was installed -- nothing. */
    if (!a.shipped && !b.shipped && a.modified.isValid() && b.modified.isValid())
    {
        const qint64 secs = b.modified.secsTo(a.modified);
        if (secs > 2)
            return 1;
        if (secs < -2)
            return -1;
    }
    return 0;
}

Verdict judge(const Info &mine, const Info &current)
{
    if (!mine.sha256.isEmpty() && mine.sha256 == current.sha256)
        return Identical;
    if (!sameAuthor(mine.author, current.author))
        return Customised;
    const int c = compare(mine, current);
    if (c > 0)
        return Newer;
    if (c == 0)
        return Customised;
    QLCFixtureDef a, b;
    if (!a.loadXMLData(mine.bytes) || !b.loadXMLData(current.bytes) || hasExtras(&a, &b))
        return Customised;
    return OutOfDate;
}

QString verdictText(Verdict v)
{
    switch (v)
    {
        case Identical:  return QObject::tr("Same as the current version");
        case OutOfDate:  return QObject::tr("Older than the current version");
        case Customised: return QObject::tr("Customised (differs on purpose, or can't tell)");
        case Newer:      return QObject::tr("Newer than the current version");
    }
    return QString();
}

/*****************************************************************************
 * What changes
 *****************************************************************************/

QString Change::text() const
{
    QString what;
    switch (area)
    {
        case Channel: what = QObject::tr("Channel “%1”").arg(name); break;
        case Mode:    what = QObject::tr("Mode “%1”").arg(name); break;
        case General: what = name; break;
    }
    QString how;
    switch (kind)
    {
        case Added:   how = QObject::tr("added"); break;
        case Removed: how = QObject::tr("removed"); break;
        case Changed: how = QString(); break;
    }
    QStringList bits;
    if (!how.isEmpty())
        bits << how;
    if (!detail.isEmpty())
        bits << detail;
    return QStringLiteral("%1: %2").arg(what, bits.join(QStringLiteral(", ")));
}

ModeFit modeFit(const QLCFixtureDef *from, const QLCFixtureDef *to, const QString &mode, QString *why)
{
    const QLCFixtureMode *a = const_cast<QLCFixtureDef *>(from)->mode(mode);
    const QLCFixtureMode *b = const_cast<QLCFixtureDef *>(to)->mode(mode);
    if (b == nullptr)
    {
        if (why != nullptr)
            *why = QObject::tr("no mode of that name");
        return ModeMissing;
    }
    if (a == nullptr)
        return SameLayout;
    if (a->channels().size() != b->channels().size())
    {
        if (why != nullptr)
            *why = QObject::tr("%1 → %2 channels").arg(a->channels().size()).arg(b->channels().size());
        return CountChanged;
    }
    for (int i = 0; i < a->channels().size(); ++i)
    {
        const QString ka = channelKind(a->channel(quint32(i)));
        const QString kb = channelKind(b->channel(quint32(i)));
        if (ka != kb)
        {
            if (why != nullptr)
                *why = QObject::tr("channel %1 is %2, was %3").arg(i + 1).arg(kb, ka);
            return ChannelsChanged;
        }
    }
    return SameLayout;
}

QList<Change> diff(const QLCFixtureDef *fromDef, const QLCFixtureDef *toDef)
{
    QList<Change> modes, channels, general;
    if (fromDef == nullptr || toDef == nullptr)
        return modes;
    QLCFixtureDef *from = const_cast<QLCFixtureDef *>(fromDef);
    QLCFixtureDef *to = const_cast<QLCFixtureDef *>(toDef);

    /* Modes: what a patched fixture is. */
    foreach (QLCFixtureMode *m, from->modes())
    {
        Change c;
        c.area = Change::Mode;
        c.name = m->name();
        QLCFixtureMode *n = to->mode(m->name());
        if (n == nullptr)
        {
            c.kind = Change::Removed;
            c.layout = true;
            modes << c;
            continue;
        }
        QString why;
        const ModeFit fit = modeFit(from, to, m->name(), &why);
        QStringList detail;
        if (fit != SameLayout)
        {
            c.layout = true;
            detail << why;
        }
        else
        {
            for (int i = 0; i < m->channels().size(); ++i)
            {
                const QLCChannel *ca = m->channel(quint32(i));
                const QLCChannel *cb = n->channel(quint32(i));
                if (ca != nullptr && cb != nullptr && ca->name() != cb->name())
                {
                    detail << QObject::tr("channel %1 renamed “%2” → “%3”").arg(i + 1).arg(ca->name(), cb->name());
                    break;
                }
            }
        }
        QList<QList<quint32> > ha, hb;
        foreach (const QLCFixtureHead &h, m->heads())
            ha << h.channels();
        foreach (const QLCFixtureHead &h, n->heads())
            hb << h.channels();
        if (ha != hb)
            detail << (ha.size() != hb.size()
                       ? QObject::tr("%1 → %2 heads").arg(ha.size()).arg(hb.size())
                       : QObject::tr("heads changed"));
        if (!detail.isEmpty())
        {
            c.detail = detail.join(QStringLiteral("; "));
            modes << c;
        }
    }
    foreach (QLCFixtureMode *m, to->modes())
    {
        if (from->mode(m->name()) != nullptr)
            continue;
        Change c;
        c.kind = Change::Added;
        c.area = Change::Mode;
        c.name = m->name();
        c.detail = QObject::tr("%n channel(s)", "", m->channels().size());
        modes << c;
    }

    /* Channels: what each one does. */
    foreach (QLCChannel *a, from->channels())
    {
        Change c;
        c.area = Change::Channel;
        c.name = a->name();
        QLCChannel *b = to->channel(a->name());
        if (b == nullptr)
        {
            c.kind = Change::Removed;
            channels << c;
            continue;
        }
        QStringList detail;
        const QString ka = channelKind(a), kb = channelKind(b);
        if (ka != kb)
            detail << QObject::tr("%1, was %2").arg(kb, ka);
        if (a->defaultValue() != b->defaultValue())
            detail << QObject::tr("default %1 → %2").arg(a->defaultValue()).arg(b->defaultValue());
        if (a->preset() != b->preset() && ka == kb)
            detail << QObject::tr("preset changed");
        const QList<QLCCapability *> capsA = a->capabilities(), capsB = b->capabilities();
        if (capsA.size() != capsB.size())
            detail << QObject::tr("%1 → %2 ranges").arg(capsA.size()).arg(capsB.size());
        else
        {
            for (int i = 0; i < capsA.size(); ++i)
            {
                if (capsA.at(i)->min() != capsB.at(i)->min() || capsA.at(i)->max() != capsB.at(i)->max()
                    || capsA.at(i)->name() != capsB.at(i)->name())
                {
                    detail << QObject::tr("ranges changed");
                    break;
                }
            }
        }
        if (!detail.isEmpty())
        {
            c.detail = detail.join(QStringLiteral("; "));
            channels << c;
        }
    }
    foreach (QLCChannel *b, to->channels())
    {
        if (from->channel(b->name()) != nullptr)
            continue;
        Change c;
        c.kind = Change::Added;
        c.area = Change::Channel;
        c.name = b->name();
        c.detail = channelKind(b);
        channels << c;
    }

    if (from->type() != to->type())
    {
        Change c;
        c.name = QObject::tr("Type");
        c.detail = QStringLiteral("%1 → %2").arg(QLCFixtureDef::typeToString(from->type()),
                                                QLCFixtureDef::typeToString(to->type()));
        general << c;
    }
    return modes + channels + general;
}

QList<Change> diff(const QByteArray &from, const QByteArray &to)
{
    QLCFixtureDef a, b;
    if (!a.loadXMLData(from) || !b.loadXMLData(to))
        return QList<Change>();
    return diff(&a, &b);
}

QStringList functionsUsing(Doc *doc, const QList<Fixture *> &fixtures)
{
    QSet<quint32> ids;
    for (Fixture *fx : fixtures)
        if (fx != nullptr)
            ids.insert(fx->id());
    QStringList names;
    if (doc == nullptr || ids.isEmpty())
        return names;
    auto groupUses = [doc, &ids](quint32 gid) {
        FixtureGroup *g = doc->fixtureGroup(gid);
        if (g == nullptr)
            return false;
        foreach (quint32 id, g->fixtureList())
            if (ids.contains(id))
                return true;
        return false;
    };
    foreach (Function *f, doc->functions())
    {
        bool uses = false;
        if (Scene *s = qobject_cast<Scene *>(f))
        {
            // Its values, the fixtures it targets, the groups it targets.
            foreach (const SceneValue &v, s->values())
                uses = uses || ids.contains(v.fxi);
            foreach (quint32 id, s->fixtures())
                uses = uses || ids.contains(id);
            foreach (quint32 gid, s->fixtureGroups())
                uses = uses || groupUses(gid);
        }
        else if (EFX *e = qobject_cast<EFX *>(f))
        {
            foreach (EFXFixture *ef, e->fixtures())
                uses = uses || (ef != nullptr && ids.contains(ef->head().fxi));
        }
        else if (RGBMatrix *m = qobject_cast<RGBMatrix *>(f))
            uses = groupUses(m->fixtureGroup());
        if (uses)
            names << f->name();
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

Impact impact(Doc *doc, const QLCFixtureDef *from, const QLCFixtureDef *to, const QString &toName)
{
    const QString target = toName.isEmpty() ? QObject::tr("the new version") : toName;
    Impact im;
    if (doc == nullptr || from == nullptr || to == nullptr)
        return im;
    QMap<QString, QList<Fixture *> > byMode;
    foreach (Fixture *fx, doc->fixtures())
    {
        if (fx == nullptr || fx->fixtureDef() != from || fx->fixtureMode() == nullptr)
            continue;
        byMode[fx->fixtureMode()->name()] << fx;
        im.fixtures << fx;
    }
    for (auto m = byMode.constBegin(); m != byMode.constEnd(); ++m)
    {
        QString why;
        const int n = m.value().size();
        switch (modeFit(from, to, m.key(), &why))
        {
            case SameLayout:
                break;
            case ModeMissing:
                im.refused << QObject::tr("Mode “%1” is not in %2: %n fixture(s) would "
                                          "lose their mode (%3)", "", n).arg(m.key(), target, namesOf(m.value()));
                break;
            case CountChanged:
                im.refused << QObject::tr("Mode “%1” goes %2: %n fixture(s) would need re-patching (%3)",
                                          "", n).arg(m.key(), why, namesOf(m.value()));
                break;
            case ChannelsChanged:
            {
                QStringList fns = functionsUsing(doc, m.value());
                QString fnText = QObject::tr("no functions");
                if (!fns.isEmpty())
                {
                    if (fns.size() > 8)
                    {
                        const int more = fns.size() - 8;
                        fns = fns.mid(0, 8);
                        fns << QObject::tr("%n more", "", more);
                    }
                    fnText = fns.join(QStringLiteral(", "));
                }
                im.confirm << QObject::tr("Mode “%1”: %2 — %n fixture(s) (%3); functions: %4", "", n)
                                  .arg(m.key(), why, namesOf(m.value()), fnText);
                break;
            }
        }
    }
    return im;
}

bool remap(Doc *doc, QLCFixtureDef *from, QLCFixtureDef *to)
{
    if (doc == nullptr || from == nullptr || to == nullptr)
        return false;
    QList<Fixture *> users;
    foreach (Fixture *fx, doc->fixtures())
    {
        if (fx == nullptr || fx->fixtureDef() != from)
            continue;
        if (fx->fixtureMode() != nullptr && to->mode(fx->fixtureMode()->name()) == nullptr)
            return false;
        users << fx;
    }
    QLCFixtureDefCache *cache = doc->fixtureDefCache();
    if (cache != nullptr)
    {
        if (cache->definitions().contains(from))
        {
            cache->substitute(to);
            cache->retire(from);
        }
    }
    for (Fixture *fx : users)
        fx->setFixtureDefinition(to, fx->fixtureMode() != nullptr ? to->mode(fx->fixtureMode()->name())
                                                                  : nullptr);
    return true;
}

/*****************************************************************************
 * The user folder
 *****************************************************************************/

bool isShippedPath(const QString &path)
{
    if (path.isEmpty())
        return false;
    const QString real = QFileInfo(path).canonicalFilePath();
    const QString p = real.isEmpty() ? QDir::cleanPath(QFileInfo(path).absoluteFilePath()) : real;
    const QString sys = QLCFixtureDefCache::systemDefinitionDirectory().canonicalPath();
    if (!sys.isEmpty() && p.startsWith(sys + QLatin1Char('/')))
        return true;
    /* A dev build, or a link into a source tree: the app's own all the same. */
    return p.contains(QLatin1String("/resources/fixtures/"));
}

QDir legacyFixturesDirectory()
{
#if defined(WIN32) || defined(Q_OS_WIN)
    return QDir(qEnvironmentVariable("USERPROFILE") + QStringLiteral("/QLC+/Fixtures"));
#elif defined(__APPLE__) || defined(Q_OS_MAC)
    return QDir(qEnvironmentVariable("HOME") + QStringLiteral("/Library/Application Support/QLC+/Fixtures"));
#else
    return QDir(qEnvironmentVariable("HOME") + QStringLiteral("/.qlcplus/fixtures"));
#endif
}

QDir backupRoot()
{
    return QDir(QLCFixtureDefCache::userDefinitionDirectory().absoluteFilePath(QStringLiteral(".backup")));
}

Scan scan(Doc *doc, bool withCustomised)
{
    Scan s;
    QLCFixtureDefCache *cache = doc != nullptr ? doc->fixtureDefCache() : nullptr;
    const QDir user = QLCFixtureDefCache::userDefinitionDirectory();
    if (!user.exists())
        return s;

    QMap<QString, QList<Info> > groups;
    const QFileInfoList entries = user.entryInfoList(QDir::Files | QDir::System | QDir::Hidden, QDir::Name);
    for (const QFileInfo &fi : entries)
    {
        if (!fi.fileName().endsWith(KExtFixture, Qt::CaseInsensitive))
            continue;
        if (fi.isSymLink() && !fi.exists())
        {
            s.unreachable << fi.fileName();
            continue;
        }
        if (QLCFile::isOnlineOnly(fi.absoluteFilePath()))
            continue;
        const Info i = readInfo(fi.absoluteFilePath());
        if (i.isValid())
            groups[i.key()] << i;
    }
    if (groups.isEmpty())
        return s;

    /* The old QLC+ folder's copies of the same fixtures that no link here
       leads to: left behind, and synced to other computers. */
    const QDir legacy = legacyFixturesDirectory();
    if (legacy.exists() && !samePath(legacy.absolutePath(), user.absolutePath()))
    {
        QStringList linked;
        for (auto g = groups.constBegin(); g != groups.constEnd(); ++g)
            for (const Info &i : g.value())
                linked << i.realPath;
        const QFileInfoList old = legacy.entryInfoList(QDir::Files, QDir::Name);
        for (const QFileInfo &fi : old)
        {
            if (!fi.fileName().endsWith(KExtFixture, Qt::CaseInsensitive)
                || QLCFile::isOnlineOnly(fi.absoluteFilePath()))
                continue;
            bool isLinked = false;
            for (const QString &l : linked)
                isLinked = isLinked || samePath(l, fi.absoluteFilePath());
            if (isLinked)
                continue;
            Info i = readInfo(fi.absoluteFilePath());
            if (!i.isValid() || !groups.contains(i.key()))
                continue;
            i.inLegacyFolder = true;
            groups[i.key()] << i;
        }
    }

    /* The shipped one of each, and which file the cache uses. */
    QHash<QString, Info> shipped;
    QHash<QString, QString> inUse;
    const QDir sys = QLCFixtureDefCache::systemDefinitionDirectory();
    auto resolve = [&sys](const QString &p) {
        return QDir::isRelativePath(p) ? sys.absoluteFilePath(p) : p;
    };
    if (cache != nullptr)
    {
        foreach (QLCFixtureDef *def, cache->definitions())
        {
            if (def == nullptr)
                continue;
            const QString key = def->manufacturer() + QLatin1Char('|') + def->model();
            if (!groups.contains(key))
                continue;
            if (def->source() == QLCFixtureDef::Local)
                inUse.insert(key, def->definitionSourceFile());
            else if (!shipped.contains(key))
            {
                const Info i = readInfo(resolve(def->definitionSourceFile()), true);
                if (i.isValid() && i.key() == key)
                    shipped.insert(key, i);
            }
        }
        foreach (const QLCFixtureDefCache::Duplicate &d, cache->duplicates())
        {
            if (d.ignoredIsLocal)
                continue;
            const Info i = readInfo(resolve(d.ignored), true);
            if (i.isValid() && groups.contains(i.key()) && !shipped.contains(i.key()))
                shipped.insert(i.key(), i);
        }
    }

    for (auto g = groups.constBegin(); g != groups.constEnd(); ++g)
    {
        QList<Info> all = g.value();
        if (shipped.contains(g.key()))
            all << shipped.value(g.key());
        if (all.size() < 2)
            continue;

        /* The current one: the newest, starting from the one in use. */
        int cur = 0;
        for (int i = 0; i < all.size(); ++i)
            if (samePath(all.at(i).path, inUse.value(g.key())))
                cur = i;
        if (inUse.value(g.key()).isEmpty() && shipped.contains(g.key()))
            cur = all.size() - 1;
        for (int i = 0; i < all.size(); ++i)
            if (i != cur && compare(all.at(i), all.at(cur)) > 0)
                cur = i;
        const Info current = all.at(cur);

        for (const Info &mine : g.value())
        {
            if (samePath(mine.realPath, current.realPath))
                continue;           // the current one, or a link to it
            Candidate c;
            c.mine = mine;
            c.current = current;
            c.verdict = judge(mine, current);
            if (c.verdict == Identical || c.verdict == Newer
                || (c.verdict == Customised && !withCustomised))
                continue;
            if (dismissed(c))
                continue;
            if (doc != nullptr)
            {
                foreach (Fixture *fx, doc->fixtures())
                {
                    if (fx != nullptr && fx->fixtureDef() != nullptr
                        && samePath(fx->fixtureDef()->definitionSourceFile(), mine.path))
                        c.usedByShow = true;
                }
            }
            c.changes = diff(mine.bytes, current.bytes);
            s.candidates << c;
        }
    }
    std::stable_sort(s.candidates.begin(), s.candidates.end(), [](const Candidate &a, const Candidate &b) {
        if (a.usedByShow != b.usedByShow)
            return a.usedByShow;
        return QString::localeAwareCompare(a.mine.key(), b.mine.key()) < 0;
    });
    return s;
}

Impact impactOf(Doc *doc, const Candidate &c)
{
    QLCFixtureDef *used = doc != nullptr ? cachedFor(doc->fixtureDefCache(), c.mine.path) : nullptr;
    if (used == nullptr)
        return Impact();
    QLCFixtureDef fresh;
    if (!fresh.loadXMLData(c.current.bytes))
        return Impact();
    return impact(doc, used, &fresh);
}

void keepMine(const Candidate &c)
{
    QSettings().setValue(settingsKey(QStringLiteral("keep"), c.mine.path), c.mine.sha256);
}

void ignoreVersion(const Candidate &c)
{
    QSettings().setValue(settingsKey(QStringLiteral("ignore"), c.mine.path), c.current.sha256);
}

bool dismissed(const Candidate &c)
{
    QSettings settings;
    return settings.value(settingsKey(QStringLiteral("keep"), c.mine.path)).toString() == c.mine.sha256
        || settings.value(settingsKey(QStringLiteral("ignore"), c.mine.path)).toString() == c.current.sha256;
}

void clearDismissals()
{
    QSettings().remove(SETTINGS_GROUP);
}

/*****************************************************************************
 * Updating, and putting it back
 *****************************************************************************/

Result update(Doc *doc, const QList<Candidate> &candidates, LinkMode linkMode, bool confirmed)
{
    Result r;
    if (candidates.isEmpty())
        return r;
    QDir root = backupRoot();
    root.mkpath(QStringLiteral("."));
    QString folder = root.absoluteFilePath(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss")));
    for (int n = 2; QFileInfo::exists(folder); ++n)
        folder = root.absoluteFilePath(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss"))
                                       + QStringLiteral("-%1").arg(n));
    QDir().mkpath(folder);
    QJsonArray entries;

    for (const Candidate &c : candidates)
    {
        const QString name = c.mine.manufacturer + QLatin1Char(' ') + c.mine.model;
        const QString file = QFileInfo(c.mine.path).fileName();
        Info now = readInfo(c.mine.path);
        now.inLegacyFolder = c.mine.inLegacyFolder;
        if (!now.isValid() || now.sha256 != c.mine.sha256)
        {
            r.failed << QObject::tr("%1 (%2): changed since it was checked; left as it is").arg(name, file);
            continue;
        }
        if (c.current.bytes.isEmpty() || c.current.key() != now.key())
        {
            r.failed << QObject::tr("%1 (%2): the current version cannot be read").arg(name, file);
            continue;
        }
        const bool viaLink = now.isLink && linkMode == UpdateLinkedOriginal && !isShippedPath(now.realPath);
        const bool replaceLink = now.isLink && !viaLink;
        const QString target = viaLink ? now.realPath : now.path;
        if (!replaceLink && isShippedPath(target))
        {
            r.failed << QObject::tr("%1 (%2): part of the app; never changed").arg(name, file);
            continue;
        }

        /* The open show's fixtures: they follow, mode by mode -- or nothing
           happens. */
        QLCFixtureDef *fresh = nullptr;
        QLCFixtureDef *used = doc != nullptr ? cachedFor(doc->fixtureDefCache(), now.path) : nullptr;
        if (used != nullptr)
        {
            fresh = new QLCFixtureDef();
            if (!fresh->loadXMLData(c.current.bytes))
            {
                delete fresh;
                r.failed << QObject::tr("%1 (%2): the current version does not load").arg(name, file);
                continue;
            }
            const Impact im = impact(doc, used, fresh);
            if (!im.refused.isEmpty())
            {
                delete fresh;
                r.failed << QObject::tr("%1 (%2): not updated -- %3").arg(name, file, im.refused.join(QStringLiteral("; ")));
                continue;
            }
            if (!im.confirm.isEmpty() && !confirmed)
            {
                delete fresh;
                r.failed << QObject::tr("%1 (%2): needs confirming -- %3").arg(name, file, im.confirm.join(QStringLiteral("; ")));
                continue;
            }
        }

        // The old file, as it was, before anything is written.
        const QString backup = uniqueIn(QDir(folder), file);
        if (!writeFile(backup, now.bytes))
        {
            delete fresh;
            r.failed << QObject::tr("%1 (%2): could not make the backup; left as it is").arg(name, file);
            continue;
        }
        QFile b(backup);
        if (b.open(QIODevice::ReadWrite))
            b.setFileTime(now.modified, QFileDevice::FileModificationTime);
        b.close();

        bool ok;
        if (replaceLink)
        {
            ok = QFile::remove(now.path) && writeFile(now.path, c.current.bytes);
            if (!ok && !QFileInfo(now.path).exists())
                QFile::link(now.linkTarget, now.path);      // as it was
        }
        else
            ok = writeFile(target, c.current.bytes);
        if (!ok)
        {
            delete fresh;
            QFile::remove(backup);
            r.failed << QObject::tr("%1 (%2): could not write %3").arg(name, file, target);
            continue;
        }

        QJsonObject e;
        e[QStringLiteral("manufacturer")] = now.manufacturer;
        e[QStringLiteral("model")] = now.model;
        e[QStringLiteral("path")] = now.path;
        e[QStringLiteral("target")] = replaceLink ? now.path : target;
        e[QStringLiteral("replacedLink")] = replaceLink;
        e[QStringLiteral("linkTarget")] = now.linkTarget;
        e[QStringLiteral("backup")] = QFileInfo(backup).fileName();
        e[QStringLiteral("oldSha256")] = now.sha256;
        e[QStringLiteral("newSha256")] = c.current.sha256;
        e[QStringLiteral("oldModified")] = now.modified.toUTC().toString(Qt::ISODate);
        e[QStringLiteral("from")] = c.current.path;
        entries << e;

        const QString versions = QObject::tr("%1 → %2").arg(now.versionText(), c.current.versionText());
        if (viaLink)
            r.updated << (now.linkIntoLegacy
                ? QObject::tr("%1: updated the linked original %2 (%3). It is in the QLC+ folder, which "
                              "syncs: other computers get the update too.").arg(name, now.realPath, versions)
                : QObject::tr("%1: updated the linked original %2 (%3)").arg(name, now.realPath, versions));
        else if (now.inLegacyFolder || c.mine.inLegacyFolder)
            r.updated << QObject::tr("%1: updated %2 in the QLC+ folder (%3). That folder syncs: other "
                                     "computers get the update too.").arg(name, now.path, versions);
        else if (replaceLink)
            r.updated << QObject::tr("%1: %2 was a link to %3; it is now a file of its own (%4), and the "
                                     "original was left as it was").arg(name, file, now.linkTarget, versions);
        else
            r.updated << QObject::tr("%1: %2 updated (%3)").arg(name, file, versions);

        reloadFile(doc, now.path, fresh);
    }

    if (entries.isEmpty())
    {
        QDir(folder).removeRecursively();
        return r;
    }
    QJsonObject manifest;
    manifest[QStringLiteral("created")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    manifest[QStringLiteral("entries")] = entries;
    manifest[QStringLiteral("undone")] = false;
    writeManifest(folder, manifest);
    r.backupFolder = folder;
    return r;
}

Result undo(Doc *doc, const QString &folder)
{
    Result r;
    QJsonObject manifest = readManifest(folder);
    const QJsonArray entries = manifest.value(QStringLiteral("entries")).toArray();
    if (entries.isEmpty() || manifest.value(QStringLiteral("undone")).toBool())
    {
        r.failed << QObject::tr("Nothing to undo in %1").arg(folder);
        return r;
    }
    r.backupFolder = folder;
    for (int i = entries.size() - 1; i >= 0; --i)
    {
        const QJsonObject e = entries.at(i).toObject();
        const QString path = e.value(QStringLiteral("path")).toString();
        const QString target = e.value(QStringLiteral("target")).toString();
        const QString name = e.value(QStringLiteral("manufacturer")).toString() + QLatin1Char(' ')
                             + e.value(QStringLiteral("model")).toString();
        const Info now = readInfo(target);
        if (now.sha256 != e.value(QStringLiteral("newSha256")).toString())
        {
            r.failed << QObject::tr("%1: %2 has changed since the update; left as it is (the old file "
                                    "is in %3)").arg(name, target, folder);
            continue;
        }
        QFile b(QDir(folder).absoluteFilePath(e.value(QStringLiteral("backup")).toString()));
        if (!b.open(QIODevice::ReadOnly))
        {
            r.failed << QObject::tr("%1: the backup is missing").arg(name);
            continue;
        }
        const QByteArray old = b.readAll();
        bool ok;
        if (e.value(QStringLiteral("replacedLink")).toBool())
            ok = QFile::remove(path) && QFile::link(e.value(QStringLiteral("linkTarget")).toString(), path);
        else
        {
            ok = writeFile(target, old);
            const QDateTime when = QDateTime::fromString(e.value(QStringLiteral("oldModified")).toString(), Qt::ISODate);
            QFile f(target);
            if (ok && when.isValid() && f.open(QIODevice::ReadWrite))
                f.setFileTime(when, QFileDevice::FileModificationTime);
        }
        if (!ok)
        {
            r.failed << QObject::tr("%1: could not put %2 back").arg(name, path);
            continue;
        }
        r.updated << QObject::tr("%1: %2 is as it was").arg(name, QFileInfo(path).fileName());
        QLCFixtureDef *fresh = new QLCFixtureDef();
        if (!fresh->loadXMLData(old))
        {
            delete fresh;
            fresh = nullptr;
        }
        reloadFile(doc, path, fresh);
    }
    manifest[QStringLiteral("undone")] = r.failed.isEmpty() || !r.updated.isEmpty();
    writeManifest(folder, manifest);
    return r;
}

QString lastUndoable()
{
    const QDir root = backupRoot();
    QStringList dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    std::sort(dirs.begin(), dirs.end(), std::greater<QString>());
    for (const QString &d : dirs)
    {
        const QString folder = root.absoluteFilePath(d);
        const QJsonObject m = readManifest(folder);
        if (m.isEmpty())
            continue;
        return m.value(QStringLiteral("undone")).toBool() ? QString() : folder;
    }
    return QString();
}

}

/*
  Q Light Controller Plus
  pageset.cpp

  Copyright (C) Branson Matheson

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

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDirIterator>
#include <QSaveFile>
#include <QFileInfo>
#include <QObject>
#include <QFile>
#include <QSet>

#include "pageset.h"
#include "encoderbank.h"
#include "surfacelayout.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "qlcconfig.h"
#include "qlcfile.h"

namespace
{
const QString kDir = QStringLiteral(":/surfacepagesets");
const QString kExt = QStringLiteral(".pageset.json");
const quint32 kNone = 0xFFFFFFFF;

QString s_userDir;

struct Param
{
    const char *key;
    int bank;
    int slot;
    const char *colour;
};

// EncoderBank's slots by name (encoderbank.cpp slotName()), in its order.
const Param kParams[] = {
    { "intensity", EncoderBank::IntensityBank, 0, nullptr },
    { "hue", EncoderBank::ColourBank, 0, nullptr },
    { "saturation", EncoderBank::ColourBank, 1, nullptr },
    { "red", EncoderBank::ColourBank, 2, "#e0403a" },
    { "green", EncoderBank::ColourBank, 3, "#3aa845" },
    { "blue", EncoderBank::ColourBank, 4, "#3a6ee0" },
    { "white", EncoderBank::ColourBank, 5, "#d8d8d8" },
    { "amber", EncoderBank::ColourBank, 6, "#e89a2c" },
    { "uv", EncoderBank::ColourBank, 7, "#8a4ae0" },
    { "pan", EncoderBank::PositionBank, 0, nullptr },
    { "tilt", EncoderBank::PositionBank, 1, nullptr },
    { "zoom", EncoderBank::BeamBank, 0, nullptr },
    { "focus", EncoderBank::BeamBank, 1, nullptr },
    { "iris", EncoderBank::BeamBank, 2, nullptr },
    { "frost", EncoderBank::BeamBank, 3, nullptr },
    { "strobe", EncoderBank::BeamBank, 4, nullptr },
    { "shutter", EncoderBank::BeamBank, 5, nullptr },
    { "gobo", EncoderBank::GoboBank, 0, nullptr },
    { "gobo-rotation", EncoderBank::GoboBank, 1, nullptr },
    { "prism", EncoderBank::GoboBank, 2, nullptr },
    { "prism-rotation", EncoderBank::GoboBank, 3, nullptr },
};

/* The PMJ's two numbered rules (Q335 a): fader N is the open look's
   colour or level (else the intensity of selected fixture N: today's
   rule), or the intensity of the focused scene's group N. */
const QString kLookLevel = QStringLiteral("look-level");
const QString kGroupIntensity = QStringLiteral("group-intensity");
const QString kNoneParam = QStringLiteral("none");

const Param *findParam(const QString &key)
{
    for (const Param &p : kParams)
        if (key == QLatin1String(p.key))
            return &p;
    return nullptr;
}

quint32 chOf(const QJsonValue &v)
{
    return v.isDouble() ? quint32(v.toInt(-1)) : kNone;
}

PageSet::Binding bindingOf(const QJsonObject &o)
{
    PageSet::Binding b;
    b.channel = chOf(o.value(QStringLiteral("ch")));
    b.param = o.value(QStringLiteral("param")).toString(kNoneParam);
    b.fine = o.value(QStringLiteral("fine")).toBool();
    b.index = o.value(QStringLiteral("index")).toInt();
    b.label = o.value(QStringLiteral("label")).toString();
    return b;
}

QJsonObject toObject(const PageSet::Binding &b)
{
    QJsonObject o;
    o.insert(QStringLiteral("ch"), qint64(b.channel));
    o.insert(QStringLiteral("param"), b.param);
    if (b.fine)
        o.insert(QStringLiteral("fine"), true);
    if (b.index > 0)
        o.insert(QStringLiteral("index"), b.index);
    if (!b.label.isEmpty())
        o.insert(QStringLiteral("label"), b.label);
    return o;
}

/* The layouts and the page sets are one resource of the engine
   (surfacelayouts.qrc); reading a layout first registers it. */
void initResources()
{
    SurfaceLayout::builtInFiles();
}
}

/*****************************************************************************
 * Bindings
 *****************************************************************************/

bool PageSet::Binding::isRule() const
{
    return PageSet::isRule(param);
}

QString PageSet::Binding::text() const
{
    if (!label.isEmpty())
        return label;
    if (param == kLookLevel)
        return QObject::tr("Level %1").arg(index);
    if (param == kGroupIntensity)
        return QObject::tr("Group %1").arg(index);
    const QString n = paramName(param);
    return fine ? QObject::tr("%1 fine").arg(n) : n;
}

/*****************************************************************************
 * The vocabulary
 *****************************************************************************/

QStringList PageSet::params()
{
    QStringList out;
    for (const Param &p : kParams)
        out << QLatin1String(p.key);
    out << kLookLevel << kGroupIntensity << kNoneParam;
    return out;
}

bool PageSet::isRule(const QString &param)
{
    return param == kLookLevel || param == kGroupIntensity;
}

bool PageSet::bankSlot(const QString &param, int *bank, int *slot)
{
    const Param *p = findParam(param);
    if (p == nullptr)
        return false;
    if (bank != nullptr)
        *bank = p->bank;
    if (slot != nullptr)
        *slot = p->slot;
    return true;
}

QString PageSet::paramName(const QString &param)
{
    const Param *p = findParam(param);
    if (p != nullptr)
        return EncoderBank::slotName(p->bank, p->slot);
    if (param == kLookLevel)
        return QObject::tr("Look level");
    if (param == kGroupIntensity)
        return QObject::tr("Group intensity");
    return QString();
}

QString PageSet::paramColour(const QString &param)
{
    const Param *p = findParam(param);
    return p != nullptr && p->colour != nullptr ? QLatin1String(p->colour) : QString();
}

/*****************************************************************************
 * Pages
 *****************************************************************************/

const PageSet::Binding *PageSet::binding(int page, quint32 channel) const
{
    if (page >= 0 && page < m_pages.size())
        for (const Binding &b : m_pages.at(page).controls)
            if (b.channel == channel)
                return &b;
    for (const Binding &b : m_always)
        if (b.channel == channel)
            return &b;
    return nullptr;
}

QList<quint32> PageSet::keys() const
{
    QList<quint32> out;
    for (const Page &p : m_pages)
        if (p.key != kNone && !out.contains(p.key))
            out << p.key;
    for (quint32 k : { m_next, m_previous, m_stepper })
        if (k != kNone && !out.contains(k))
            out << k;
    return out;
}

QList<quint32> PageSet::controlsOn(int page) const
{
    QList<quint32> out = keys();
    auto add = [&out](const QList<Binding> &list) {
        for (const Binding &b : list)
            if (b.param != kNoneParam && !out.contains(b.channel))
                out << b.channel;
    };
    add(m_always);
    if (page >= 0 && page < m_pages.size())
        add(m_pages.at(page).controls);
    return out;
}

QHash<quint32, QString> PageSet::labels(int page) const
{
    QHash<quint32, QString> out;
    for (const Binding &b : m_always)
        if (b.param != kNoneParam)
            out.insert(b.channel, b.text());
    if (page >= 0 && page < m_pages.size())
        for (const Binding &b : m_pages.at(page).controls)
            if (b.param != kNoneParam)
                out.insert(b.channel, b.text());
    // A page key says its page; the keys sharing it say each of theirs.
    for (const Page &p : m_pages)
        if (p.key != kNone)
        {
            const QString had = out.value(p.key);
            out.insert(p.key, had.isEmpty() ? p.name : had + QStringLiteral(" / ") + p.name);
        }
    if (m_next != kNone)
        out.insert(m_next, QObject::tr("Next page"));
    if (m_previous != kNone)
        out.insert(m_previous, QObject::tr("Previous page"));
    if (m_stepper != kNone)
        out.insert(m_stepper, QObject::tr("Pages"));
    return out;
}

int PageSet::pageForKey(quint32 key, int from) const
{
    if (key == kNone)
        return -1;
    QList<int> withKey;
    for (int i = 0; i < m_pages.size(); ++i)
        if (m_pages.at(i).key == key)
            withKey << i;
    if (withKey.isEmpty())
        return -1;
    // The key of the page on now again: the next page of that key.
    const int at = withKey.indexOf(from);
    return at < 0 ? withKey.first() : withKey.at((at + 1) % withKey.size());
}

/*****************************************************************************
 * Files
 *****************************************************************************/

PageSet PageSet::fromJson(const QByteArray &json, QString *error)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
    {
        if (error != nullptr)
            *error = QObject::tr("not a page set: %1").arg(pe.errorString());
        return PageSet();
    }
    const QJsonObject root = doc.object();
    PageSet s;
    s.m_name = root.value(QStringLiteral("name")).toString();
    s.m_description = root.value(QStringLiteral("description")).toString();
    const QJsonObject fits = root.value(QStringLiteral("fits")).toObject();
    s.m_profile = fits.value(QStringLiteral("profile")).toString();
    s.m_layoutFormat = fits.value(QStringLiteral("layout")).toInt(1);
    const QJsonObject keys = root.value(QStringLiteral("keys")).toObject();
    s.m_next = chOf(keys.value(QStringLiteral("next")));
    s.m_previous = chOf(keys.value(QStringLiteral("previous")));
    s.m_stepper = chOf(keys.value(QStringLiteral("stepper")));
    s.m_invert = root.value(QStringLiteral("invertTurns")).toBool();
    for (const QJsonValue &v : root.value(QStringLiteral("always")).toArray())
        s.m_always << bindingOf(v.toObject());
    for (const QJsonValue &v : root.value(QStringLiteral("pages")).toArray())
    {
        const QJsonObject o = v.toObject();
        Page p;
        p.name = o.value(QStringLiteral("name")).toString();
        p.key = chOf(o.value(QStringLiteral("key")));
        for (const QJsonValue &c : o.value(QStringLiteral("controls")).toArray())
            p.controls << bindingOf(c.toObject());
        s.m_pages << p;
    }
    if (!s.isValid() && error != nullptr)
        *error = QObject::tr("a page set names itself, the board it fits and at least one page");
    return s;
}

PageSet PageSet::fromFile(const QString &path, QString *error)
{
    initResources();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
            *error = QObject::tr("%1 can't be read").arg(QFileInfo(path).fileName());
        return PageSet();
    }
    PageSet s = fromJson(f.readAll(), error);
    s.m_file = path;
    s.m_builtIn = path.startsWith(QLatin1Char(':'));
    return s;
}

QByteArray PageSet::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), 1);
    root.insert(QStringLiteral("name"), m_name);
    if (!m_description.isEmpty())
        root.insert(QStringLiteral("description"), m_description);
    QJsonObject fits;
    fits.insert(QStringLiteral("profile"), m_profile);
    fits.insert(QStringLiteral("layout"), m_layoutFormat);
    root.insert(QStringLiteral("fits"), fits);
    QJsonObject keys;
    if (m_next != kNone)
        keys.insert(QStringLiteral("next"), qint64(m_next));
    if (m_previous != kNone)
        keys.insert(QStringLiteral("previous"), qint64(m_previous));
    if (m_stepper != kNone)
        keys.insert(QStringLiteral("stepper"), qint64(m_stepper));
    if (!keys.isEmpty())
        root.insert(QStringLiteral("keys"), keys);
    if (m_invert)
        root.insert(QStringLiteral("invertTurns"), true);
    QJsonArray always;
    for (const Binding &b : m_always)
        always.append(toObject(b));
    if (!always.isEmpty())
        root.insert(QStringLiteral("always"), always);
    QJsonArray pages;
    for (const Page &p : m_pages)
    {
        QJsonObject o;
        o.insert(QStringLiteral("name"), p.name);
        if (p.key != kNone)
            o.insert(QStringLiteral("key"), qint64(p.key));
        QJsonArray controls;
        for (const Binding &b : p.controls)
            controls.append(toObject(b));
        o.insert(QStringLiteral("controls"), controls);
        pages.append(o);
    }
    root.insert(QStringLiteral("pages"), pages);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool PageSet::save(const QString &path, QString *error) const
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(toJson()) < 0 || !f.commit())
    {
        if (error != nullptr)
            *error = QObject::tr("%1 can't be written").arg(QFileInfo(path).fileName());
        return false;
    }
    return true;
}

/*****************************************************************************
 * The checker
 *****************************************************************************/

QStringList PageSet::check(const SurfaceLayout &layout, const QLCInputProfile *profile) const
{
    QStringList out;
    if (!isValid())
        out << QObject::tr("the set has no name, no board or no page");
    if (!layout.isValid())
        return out << QObject::tr("there is no drawing of %1").arg(m_profile);
    if (layout.profileName() != m_profile)
        out << QObject::tr("the set fits %1, not %2").arg(m_profile, layout.profileName());
    if (profile != nullptr && profile->name() != m_profile)
        out << QObject::tr("the set fits %1, not %2").arg(m_profile, profile->name());

    const QList<SurfaceLayout::Control> &controls = layout.controls();
    auto face = [&](quint32 ch) {
        const int i = layout.indexOf(ch);
        return i < 0 ? QStringLiteral("#%1").arg(ch) : controls.at(i).face;
    };
    // Each desk role (the layout's GO, Back, STOP, GM) by every channel it hears.
    auto roleOf = [&](quint32 ch) {
        const int i = layout.indexOf(ch);
        if (i < 0)
            return QString();
        for (const SurfaceLayout::Role &r : layout.roles())
            if (controls.at(i).hears(r.channel))
                return r.role;
        return QString();
    };
    const QList<quint32> keyList = keys();

    for (quint32 k : keyList)
    {
        const int i = layout.indexOf(k);
        if (i < 0 || controls.at(i).channel != k)
        {
            out << QObject::tr("page key %1 is not a control of the %2").arg(k).arg(layout.name());
            continue;
        }
        const SurfaceLayout::Kind kind = controls.at(i).kind;
        const bool stepper = k == m_stepper;
        if (stepper ? !controls.at(i).turns()
                    : (kind != SurfaceLayout::Button && kind != SurfaceLayout::Pad))
            out << QObject::tr("%1 can't change pages: it is not a %2").arg(face(k),
                stepper ? QObject::tr("knob") : QObject::tr("button"));
        const QString role = roleOf(k);
        if (!role.isEmpty())
            out << QObject::tr("%1 is the desk's %2: never a page key").arg(face(k), role.toUpper());
    }

    auto checkList = [&](const QString &where, const QList<Binding> &list) {
        QSet<quint32> seen;
        for (const Binding &b : list)
        {
            const int i = layout.indexOf(b.channel);
            if (i < 0 || controls.at(i).channel != b.channel)
            {
                out << QObject::tr("%1: channel %2 is not a control of the %3").arg(where).arg(b.channel)
                           .arg(layout.name());
                continue;
            }
            const SurfaceLayout::Control &c = controls.at(i);
            if (seen.contains(b.channel))
                out << QObject::tr("%1: %2 is given twice").arg(where, c.face);
            seen.insert(b.channel);
            const QString role = roleOf(b.channel);
            if (!role.isEmpty())
            {
                out << QObject::tr("%1: %2 is the desk's %3 and is never on a page").arg(where, c.face,
                                                                                          role.toUpper());
                continue;
            }
            if (keyList.contains(b.channel))
                out << QObject::tr("%1: %2 changes pages and can't have a job too").arg(where, c.face);
            if (b.param == kNoneParam)
                continue;
            if (!params().contains(b.param))
            {
                out << QObject::tr("%1: %2: unknown attribute '%3'").arg(where, c.face, b.param);
                continue;
            }
            if (!c.isFader() && !c.turns())
                out << QObject::tr("%1: %2 is a button: it can't set %3").arg(where, c.face, b.text());
            if (b.fine && !c.turns())
                out << QObject::tr("%1: %2 is a fader: only a knob turns fine").arg(where, c.face);
            if (b.isRule() && !c.isFader())
                out << QObject::tr("%1: %2: %3 is a fader's rule").arg(where, c.face, b.text());
            if (b.isRule() && (b.index < 1 || b.index > 99))
                out << QObject::tr("%1: %2: %3 needs its number").arg(where, c.face, paramName(b.param));
            if (profile != nullptr)
            {
                const QLCInputChannel *ich = profile->channel(b.channel);
                if (ich == nullptr)
                    out << QObject::tr("%1: %2 is not in the profile").arg(where, c.face);
            }
        }
    };
    checkList(QObject::tr("Every page"), m_always);
    QSet<QString> names;
    for (const Page &p : m_pages)
    {
        if (p.name.isEmpty())
            out << QObject::tr("a page has no name");
        if (names.contains(p.name))
            out << QObject::tr("two pages are named %1").arg(p.name);
        names.insert(p.name);
        for (const Binding &b : p.controls)
        {
            bool inAlways = false;
            for (const Binding &a : m_always)
                inAlways = inAlways || a.channel == b.channel;
            if (inAlways)
                out << QObject::tr("%1: %2 is on every page already").arg(p.name, face(b.channel));
        }
        checkList(p.name, p.controls);
    }
    return out;
}

/*****************************************************************************
 * Where sets live
 *****************************************************************************/

QList<PageSet> PageSet::builtIns()
{
    static QList<PageSet> cache;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        initResources();
        QStringList files;
        QDirIterator it(kDir, QStringList() << QStringLiteral("*") + kExt, QDir::Files);
        while (it.hasNext())
            files << it.next();
        files.sort();
        for (const QString &f : files)
        {
            const PageSet s = fromFile(f);
            if (s.isValid())
                cache << s;
        }
    }
    return cache;
}

QDir PageSet::userDirectory()
{
    QDir dir;
    if (!s_userDir.isEmpty())
        dir = QDir(s_userDir);
    else
    {
        dir = QLCFile::userDirectory(QString(USERINPUTPROFILEDIR), QString(INPUTPROFILEDIR),
                                     QStringList() << QStringLiteral("*") + kExt);
        dir = QDir(dir.absoluteFilePath(QStringLiteral("PageSets")));
    }
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));
    dir.setNameFilters(QStringList() << QStringLiteral("*") + kExt);
    dir.setFilter(QDir::Files);
    return dir;
}

void PageSet::setUserDirectory(const QString &path)
{
    s_userDir = path;
}

QList<PageSet> PageSet::forProfile(const QString &profileName)
{
    QList<PageSet> out;
    for (const PageSet &s : builtIns())
        if (s.profileName() == profileName)
            out << s;
    const QDir dir = userDirectory();
    QStringList files = dir.entryList();
    files.sort();
    for (const QString &f : files)
    {
        const PageSet s = fromFile(dir.absoluteFilePath(f));
        if (s.isValid() && s.profileName() == profileName)
            out << s;
    }
    return out;
}

PageSet PageSet::find(const QString &profileName, const QString &name)
{
    for (const PageSet &s : forProfile(profileName))
        if (s.name() == name)
            return s;
    return PageSet();
}

PageSet PageSet::defaultFor(const QString &profileName)
{
    for (const PageSet &s : builtIns())
        if (s.profileName() == profileName)
            return s;
    return PageSet();
}

bool PageSet::importFile(const QString &path, QString *error, PageSet *imported)
{
    QString why;
    PageSet s = fromFile(path, &why);
    if (!s.isValid())
    {
        if (error != nullptr)
            *error = why;
        return false;
    }
    const SurfaceLayout layout = SurfaceLayout::forProfile(s.profileName());
    const QStringList problems = s.check(layout);
    if (!problems.isEmpty())
    {
        if (error != nullptr)
            *error = problems.join(QStringLiteral("\n"));
        return false;
    }
    for (const PageSet &b : builtIns())
        if (b.profileName() == s.profileName() && b.name() == s.name())
        {
            if (error != nullptr)
                *error = QObject::tr("a built-in set of the %1 is named %2: rename it first")
                             .arg(layout.name(), s.name());
            return false;
        }
    QString base = s.name();
    for (QChar &c : base)
        if (!c.isLetterOrNumber() && c != QLatin1Char('-') && c != QLatin1Char(' '))
            c = QLatin1Char('_');
    const QString file = QStringLiteral("%1.%2%3").arg(QString(s.profileName()).replace(QLatin1Char(' '),
                                                                                         QLatin1Char('-')),
                                                       base, kExt);
    const QString to = userDirectory().absoluteFilePath(file);
    s.m_builtIn = false;
    if (!s.save(to, error))
        return false;
    s.m_file = to;
    if (imported != nullptr)
        *imported = s;
    return true;
}

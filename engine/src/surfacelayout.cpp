/*
  Q Light Controller Plus
  surfacelayout.cpp

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
#include <QFile>
#include <QSet>

#include "surfacelayout.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"

/* The layouts are a resource of the engine (surfacelayouts.qrc): a shared
   library registers it by itself, a static build here. */
static void initLayoutResources()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    Q_INIT_RESOURCE(surfacelayouts);
}

namespace
{
const QString kDir = QStringLiteral(":/surfacelayouts");

SurfaceLayout::Kind kindOf(const QString &k, bool *ok)
{
    *ok = true;
    if (k == QLatin1String("button")) return SurfaceLayout::Button;
    if (k == QLatin1String("pad")) return SurfaceLayout::Pad;
    if (k == QLatin1String("fader")) return SurfaceLayout::Fader;
    if (k == QLatin1String("xfader")) return SurfaceLayout::CrossFader;
    if (k == QLatin1String("knob")) return SurfaceLayout::Knob;
    if (k == QLatin1String("encoder")) return SurfaceLayout::Encoder;
    if (k == QLatin1String("led")) return SurfaceLayout::Indicator;
    *ok = false;
    return SurfaceLayout::Button;
}

QRectF rectOf(const QJsonObject &o)
{
    return QRectF(o.value(QStringLiteral("x")).toDouble(), o.value(QStringLiteral("y")).toDouble(),
                  o.value(QStringLiteral("w")).toDouble(1.0), o.value(QStringLiteral("h")).toDouble(1.0));
}

bool knownRole(const QString &r)
{
    return r == QLatin1String("go") || r == QLatin1String("back") || r == QLatin1String("stop")
           || r == QLatin1String("gm");
}
}

int SurfaceLayout::indexOf(quint32 channel) const
{
    return m_index.value(channel, -1);
}

QString SurfaceLayout::roleOn(quint32 channel) const
{
    const int i = indexOf(channel);
    if (i < 0)
        return QString();
    for (const Role &r : m_roles)
        if (m_controls.at(i).hears(r.channel))
            return r.role;
    return QString();
}

quint32 SurfaceLayout::roleChannel(const QString &role) const
{
    for (const Role &r : m_roles)
        if (r.role == role)
            return r.channel;
    return 0xFFFFFFFF;
}

QColor SurfaceLayout::lampColour(uchar sent) const
{
    const int v = sent >> 1;    // a MIDI velocity is half the value sent
    if (v < 0 || v >= m_lampColours.size())
        return QColor();
    return m_lampColours.at(v);
}

SurfaceLayout SurfaceLayout::fromFile(const QString &path, QString *error)
{
    initLayoutResources();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
            *error = QStringLiteral("%1: can't be read").arg(path);
        return SurfaceLayout();
    }
    return fromJson(f.readAll(), error);
}

SurfaceLayout SurfaceLayout::fromJson(const QByteArray &json, QString *error)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
    {
        if (error != nullptr)
            *error = QStringLiteral("not a layout: %1").arg(pe.errorString());
        return SurfaceLayout();
    }
    const QJsonObject root = doc.object();
    SurfaceLayout l;
    l.m_profile = root.value(QStringLiteral("profile")).toString();
    l.m_name = root.value(QStringLiteral("name")).toString(l.m_profile);
    l.m_short = root.value(QStringLiteral("short")).toString();
    l.m_lampKind = root.value(QStringLiteral("lampKind")).toString();
    const QJsonObject display = root.value(QStringLiteral("display")).toObject();
    l.m_displayLines = display.value(QStringLiteral("lines")).toInt();
    l.m_displayChars = display.value(QStringLiteral("chars")).toInt();
    for (const QJsonValue &v : display.value(QStringLiteral("shows")).toArray())
        l.m_displayShows << v.toString();
    l.m_size = QSizeF(root.value(QStringLiteral("width")).toDouble(), root.value(QStringLiteral("height")).toDouble());

    for (const QJsonValue &v : root.value(QStringLiteral("sections")).toArray())
    {
        const QJsonObject o = v.toObject();
        Section s;
        s.label = o.value(QStringLiteral("label")).toString();
        s.rect = rectOf(o);
        l.m_sections << s;
    }
    for (const QJsonValue &v : root.value(QStringLiteral("controls")).toArray())
    {
        const QJsonObject o = v.toObject();
        Control c;
        c.channel = quint32(o.value(QStringLiteral("ch")).toInt(-1));
        for (const QJsonValue &a : o.value(QStringLiteral("also")).toArray())
            c.also << quint32(a.toInt());
        c.rect = rectOf(o);
        bool ok = false;
        c.kind = kindOf(o.value(QStringLiteral("kind")).toString(), &ok);
        if (!ok)
        {
            if (error != nullptr)
                *error = QStringLiteral("channel %1: unknown kind '%2'")
                             .arg(c.channel).arg(o.value(QStringLiteral("kind")).toString());
            return SurfaceLayout();
        }
        c.face = o.value(QStringLiteral("face")).toString();
        c.job = o.value(QStringLiteral("job")).toString();
        const QString lamp = o.value(QStringLiteral("lamp")).toString();
        c.lamp = lamp == QLatin1String("rgb") ? Rgb : lamp == QLatin1String("led") ? Led : NoLamp;
        l.m_controls << c;
    }
    for (int i = 0; i < l.m_controls.size(); ++i)
    {
        // The first place wins here; selfTest() says a channel placed twice.
        if (!l.m_index.contains(l.m_controls.at(i).channel))
            l.m_index.insert(l.m_controls.at(i).channel, i);
        for (quint32 a : l.m_controls.at(i).also)
            if (!l.m_index.contains(a))
                l.m_index.insert(a, i);
    }
    for (const QJsonValue &v : root.value(QStringLiteral("roles")).toArray())
    {
        const QJsonObject o = v.toObject();
        Role r;
        r.role = o.value(QStringLiteral("role")).toString();
        r.channel = quint32(o.value(QStringLiteral("ch")).toInt(-1));
        r.seed = o.value(QStringLiteral("seed")).toBool();
        l.m_roles << r;
    }
    for (const QJsonValue &v : root.value(QStringLiteral("pages")).toArray())
    {
        const QJsonObject o = v.toObject();
        Page p;
        p.name = o.value(QStringLiteral("name")).toString();
        const QJsonObject labels = o.value(QStringLiteral("labels")).toObject();
        for (auto it = labels.begin(); it != labels.end(); ++it)
            p.labels.insert(it.key().toUInt(), it.value().toString());
        l.m_pages << p;
    }
    for (const QJsonValue &v : root.value(QStringLiteral("lampColours")).toArray())
        l.m_lampColours << QColor(v.toString());

    if (!l.isValid() && error != nullptr)
        *error = QStringLiteral("a layout names its profile and has controls");
    return l;
}

QStringList SurfaceLayout::builtInFiles()
{
    initLayoutResources();
    QStringList out;
    QDirIterator it(kDir, QStringList() << QStringLiteral("*.layout.json"), QDir::Files);
    while (it.hasNext())
        out << it.next();
    out.sort();
    return out;
}

SurfaceLayout SurfaceLayout::forProfile(const QString &profileName)
{
    if (profileName.isEmpty())
        return SurfaceLayout();
    // A handful of small files, read once.
    static QHash<QString, SurfaceLayout> cache;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        for (const QString &file : builtInFiles())
        {
            const SurfaceLayout l = fromFile(file);
            if (l.isValid())
                cache.insert(l.profileName(), l);
        }
    }
    return cache.value(profileName);
}

QStringList SurfaceLayout::selfTest(const QLCInputProfile *profile) const
{
    QStringList out;
    if (profile == nullptr)
        return QStringList() << QStringLiteral("no profile");
    if (profile->name() != m_profile)
        out << QStringLiteral("the layout is of '%1', the profile is '%2'").arg(m_profile, profile->name());

    const QMap<quint32, QLCInputChannel *> chs = profile->channels();
    QHash<quint32, int> placed;
    for (const Control &c : m_controls)
    {
        placed[c.channel]++;
        for (quint32 a : c.also)
            placed[a]++;
    }
    for (auto it = chs.begin(); it != chs.end(); ++it)
    {
        const int n = placed.value(it.key());
        if (n == 0)
            out << QStringLiteral("channel %1 (%2) is not placed").arg(it.key()).arg(it.value()->name());
        else if (n > 1)
            out << QStringLiteral("channel %1 (%2) is placed %3 times").arg(it.key()).arg(it.value()->name()).arg(n);
    }
    for (auto it = placed.begin(); it != placed.end(); ++it)
        if (!chs.contains(it.key()))
            out << QStringLiteral("channel %1 is not in the profile").arg(it.key());

    const QRectF board(QPointF(0, 0), m_size);
    for (int i = 0; i < m_controls.size(); ++i)
    {
        const Control &a = m_controls.at(i);
        if (a.rect.width() <= 0 || a.rect.height() <= 0)
            out << QStringLiteral("channel %1 has no size").arg(a.channel);
        if (!board.contains(a.rect))
            out << QStringLiteral("channel %1 is off the board").arg(a.channel);
        for (int j = i + 1; j < m_controls.size(); ++j)
        {
            const QRectF both = a.rect.intersected(m_controls.at(j).rect);
            // Edges may touch; any area shared is an overlap.
            if (both.width() > 1e-6 && both.height() > 1e-6)
                out << QStringLiteral("channels %1 and %2 overlap").arg(a.channel).arg(m_controls.at(j).channel);
        }
    }
    QSet<QString> roles;
    for (const Role &r : m_roles)
    {
        if (!knownRole(r.role))
            out << QStringLiteral("unknown role '%1'").arg(r.role);
        if (roles.contains(r.role))
            out << QStringLiteral("role '%1' is given twice").arg(r.role);
        roles.insert(r.role);
        if (indexOf(r.channel) < 0)
            out << QStringLiteral("role '%1' is on channel %2, which no control has").arg(r.role).arg(r.channel);
        else if (QLCInputChannel *ich = chs.value(r.channel))
        {
            // The Grand Master takes a fader, the others a button (DeskRecord's learn rule).
            const bool fader = ich->type() == QLCInputChannel::Slider;
            if ((r.role == QLatin1String("gm")) != fader)
                out << QStringLiteral("role '%1' is on %2, a %3").arg(r.role, ich->name(),
                                                                       fader ? QStringLiteral("fader")
                                                                             : QStringLiteral("button"));
        }
    }
    for (const Page &p : m_pages)
        for (auto it = p.labels.begin(); it != p.labels.end(); ++it)
            if (indexOf(it.key()) < 0)
                out << QStringLiteral("page '%1' labels channel %2, which no control has").arg(p.name).arg(it.key());
    return out;
}

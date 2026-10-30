/*
  qlcconsole
  legacysettings.cpp

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

#include <QDateTime>
#include <QFileInfo>
#include <QSettings>

#include "legacysettings.h"

#define KEY_RECENT      QStringLiteral("workspace/recent")
#define KEY_WORKINGPATH QStringLiteral("workspace/workingpath")
#define KEY_OFFERED     QStringLiteral("legacy/qlcplusSettingsOffered")

namespace
{
/* QLC+ 4.14's I/O and plugin preferences: the default patch (used for a
   new show), hotplug, and each plugin's own settings. */
const char *const kIoPrefixes[] = {
    "inputmap/", "outputmap/", "inputmanager/hotplug",
    "midiplugin/", "ArtNetPlugin/", "E131Plugin/", "OSCPlugin/", "SPIPlugin/",
    "GPIOPlugin/", "OlaIO/", "udmx/", "qlcftdi/", "dmxusbopenrx/", "enttecdmxusbopen/",
};

/* QLC+ 4.14's interface preferences that mean the same here. Window
   geometry, splitters and states are not among them. */
const char *const kUiPrefixes[] = {
    "workspace/sliderstyle",
    "simpledesk/channelsperpage", "simpledesk/playbacksperpage", "simpledesk/showchannelnames",
    "virtualconsole/", "slidermatrix/defaultSize",
    "functionselection/filter", "sceneeditor/chaser", "addfixture/expanded",
    "speeddialwidget/direction", "selectinputchannel/allowunpatched",
    "assignhotkey/autoclose", "addchannelsgroup/applyall",
};

bool isLayout(const QString &key)
{
    const QString last = key.section('/', -1).toLower();
    return last.contains(QLatin1String("geometry")) || last.contains(QLatin1String("splitter"))
           || last == QLatin1String("state")
           || (last == QLatin1String("size") && !key.startsWith(QLatin1String("virtualconsole/")));
}

template <size_t N>
bool matches(const QString &key, const char *const (&prefixes)[N])
{
    for (const char *p : prefixes)
    {
        const QLatin1String prefix(p);
        if (prefix.endsWith('/') ? key.startsWith(prefix) : key == prefix)
            return true;
    }
    return false;
}

QStringList recentOf(const QSettings &s)
{
    QStringList list;
    for (int i = 0; i < LegacySettings::kMaxRecent; i++)
    {
        const QString path = s.value(KEY_RECENT + QString::number(i)).toString();
        if (!path.isEmpty() && !list.contains(path))
            list << path;
    }
    return list;
}
}

QPair<QString, QString> LegacySettings::qlcPlusScope()
{
    /* What stock QLC+ 4.14 uses (ui/src/app.cpp): organization "qlcplus",
       domain "qlcplus.org", application "Q Light Controller Plus". macOS
       names the store by the domain (org.qlcplus.Q Light Controller
       Plus.plist), the other systems by the organization. */
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    return qMakePair(QStringLiteral("qlcplus.org"), QStringLiteral("Q Light Controller Plus"));
#else
    return qMakePair(QStringLiteral("qlcplus"), QStringLiteral("Q Light Controller Plus"));
#endif
}

std::unique_ptr<QSettings> LegacySettings::openQlcPlus()
{
    /* Tests (and a curious user) point it at an .ini file instead */
    const QString path = qEnvironmentVariable("QLC_QLCPLUS_SETTINGS");
    if (!path.isEmpty())
        return std::make_unique<QSettings>(path, QSettings::IniFormat);

    const QPair<QString, QString> scope = qlcPlusScope();
    return std::make_unique<QSettings>(QSettings::NativeFormat, QSettings::UserScope,
                                       scope.first, scope.second);
}

LegacySettings::Plan LegacySettings::plan(const QSettings &from, const QSettings &to)
{
    Plan plan;

    const QStringList own = recentOf(to);
    for (const QString &path : recentOf(from))
    {
        if (own.contains(path) || plan.recentFiles.contains(path) || !QFileInfo::exists(path))
            continue;
        if (own.size() + plan.recentFiles.size() >= kMaxRecent)
            break;
        plan.recentFiles << path;
    }

    const QStringList keys = from.allKeys();
    for (const QString &key : keys)
    {
        if (to.contains(key) || isLayout(key))
            continue;
        if (matches(key, kIoPrefixes))
            plan.ioKeys << key;
        else if (matches(key, kUiPrefixes))
            plan.uiKeys << key;
    }
    /* The folder QLC+'s file dialogs opened in, with the recent shows */
    if (!plan.recentFiles.isEmpty() && !to.contains(KEY_WORKINGPATH) && from.contains(KEY_WORKINGPATH))
        plan.uiKeys << KEY_WORKINGPATH;

    plan.ioKeys.sort();
    plan.uiKeys.sort();
    return plan;
}

int LegacySettings::apply(const QSettings &from, QSettings &to, const Plan &plan)
{
    int written = 0;
    if (!plan.recentFiles.isEmpty())
    {
        const QStringList merged = (recentOf(to) + plan.recentFiles).mid(0, kMaxRecent);
        for (int i = 0; i < merged.size(); i++)
            to.setValue(KEY_RECENT + QString::number(i), merged.at(i));
        written += plan.recentFiles.size();
    }
    for (const QString &key : plan.ioKeys + plan.uiKeys)
    {
        if (to.contains(key) || !from.contains(key))
            continue;
        to.setValue(key, from.value(key));
        written++;
    }
    to.sync();
    return written;
}

QString LegacySettings::describe(const Plan &plan)
{
    QStringList lines;
    const int recent = plan.recentFiles.size(), io = plan.ioKeys.size(), ui = plan.uiKeys.size();
    if (recent > 0)
        lines << (recent == 1 ? tr("1 recent show (File ▸ Open Recent)")
                              : tr("%1 recent shows (File ▸ Open Recent)").arg(recent));
    if (io > 0)
        lines << (io == 1 ? tr("I/O and plugin preferences (1 setting)")
                          : tr("I/O and plugin preferences (%1 settings)").arg(io));
    if (ui > 0)
        lines << (ui == 1 ? tr("interface preferences (1 setting)")
                          : tr("interface preferences (%1 settings)").arg(ui));
    return lines.isEmpty() ? QString() : QStringLiteral("• ") + lines.join(QStringLiteral("\n• "));
}

bool LegacySettings::wasOffered(const QSettings &own)
{
    return own.contains(KEY_OFFERED);
}

void LegacySettings::markOffered(QSettings &own)
{
    own.setValue(KEY_OFFERED, QDateTime::currentDateTime().toString(Qt::ISODate));
    own.sync();
}

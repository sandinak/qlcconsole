/*
  Q Light Controller Plus
  wledlayoutstore.h

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

#ifndef WLEDLAYOUTSTORE_H
#define WLEDLAYOUTSTORE_H

#include <QStringList>
#include <QSettings>
#include <QString>
#include <QVector>

/**
 * Each WLED's segment layout and the direction each segment runs, kept in
 * the settings -- header-only, so the WLED output and Write Segments (the
 * UI) keep the same entries.
 *
 * A device only says which way a segment runs NOW, and while an effect's
 * Reverse plays that is the layout's way turned round. Left like that -- the
 * app quit or crashed mid-look -- the next start would take the turned-round
 * way for the layout's. So the layout's own ways are kept here, by the
 * device (its MAC, else its address) and the layout (ids, starts, stops):
 * the same layout seen again takes its ways from here, not from the device.
 *
 *   wled/layouts/<mac or address>/segments   "0:0-12,1:12-25,..."
 *   wled/layouts/<mac or address>/rev        "01..." (one per segment)
 */
namespace WledLayoutStore
{
    struct Segment
    {
        int id = 0;
        int start = 0;
        int stop = 0;
        bool rev = false;
    };

    inline QString deviceKey(const QString &mac, const QString &address)
    {
        QString m = mac.trimmed().toLower();
        m.remove(QLatin1Char(':'));
        m.remove(QLatin1Char('-'));
        QString k = m.isEmpty() ? address.trimmed() : m;   // "172_18_2_55", "aabbccddeeff"
        for (QChar &c : k)
            if (!c.isLetterOrNumber())
                c = QLatin1Char('_');
        return QStringLiteral("wled/layouts/") + k;
    }

    inline QString signature(const QVector<Segment> &segs)
    {
        QStringList parts;
        for (const Segment &s : segs)
            parts << QStringLiteral("%1:%2-%3").arg(s.id).arg(s.start).arg(s.stop);
        return parts.join(QLatin1Char(','));
    }

    /** Keep @p segs' ways as the layout's own. */
    inline void save(const QString &mac, const QString &address, const QVector<Segment> &segs)
    {
        if (segs.isEmpty() || (mac.trimmed().isEmpty() && address.trimmed().isEmpty()))
            return;
        QString dirs;
        for (const Segment &s : segs)
            dirs += s.rev ? QLatin1Char('1') : QLatin1Char('0');
        QSettings settings;
        const QString key = deviceKey(mac, address);
        settings.setValue(key + QStringLiteral("/segments"), signature(segs));
        settings.setValue(key + QStringLiteral("/rev"), dirs);
    }

    /** The kept ways of @p segs (ids, starts and stops as the device says
     *  them) into their rev, when this layout is the one kept for it. */
    inline bool load(const QString &mac, const QString &address, QVector<Segment> &segs)
    {
        if (segs.isEmpty())
            return false;
        QSettings settings;
        const QString key = deviceKey(mac, address);
        if (settings.value(key + QStringLiteral("/segments")).toString() != signature(segs))
            return false;
        const QString dirs = settings.value(key + QStringLiteral("/rev")).toString();
        if (dirs.size() != segs.size())
            return false;
        for (int k = 0; k < segs.size(); k++)
            segs[k].rev = dirs.at(k) == QLatin1Char('1');
        return true;
    }
}

#endif

/*
  Q Light Controller Plus
  wledpalettes.h

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

#ifndef WLEDPALETTES_H
#define WLEDPALETTES_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QVector>
#include <QString>
#include <QColor>

/**
 * What each of a WLED's colour palettes looks like: the colour stops its
 * /json/palx gives (paged), and the strip they draw. Header-only, so the WLED
 * output (which fetches and keeps them) and the UI (which draws them) share
 * one reading of the format.
 *
 * An entry of a palette is [pos, r, g, b] (pos 0-255), or a string: "c1",
 * "c2", "c3" -- the effect's colour 1-3, whatever those are right now -- or
 * "r", random. Strings carry no position; they are spread evenly.
 *
 * Kept per device, by its MAC (else its address), like its layout:
 *   wled/palettes/<mac or address>/palx   {"m":9,"p":{"0":[...],...}}
 *   wled/palettes/<mac or address>/pal    ["Default","* Random Cycle",...]
 */
namespace WledPalettes
{
    struct Stop
    {
        enum Kind { Rgb, Colour1, Colour2, Colour3, Random };
        int pos = 0;            ///< 0-255
        Kind kind = Rgb;
        QRgb rgb = 0;
    };
    typedef QVector<Stop> Palette;

    /** Fold one /json/palx page into @p acc ({"m":..,"p":{..}}). */
    inline void mergePage(QJsonObject &acc, const QJsonObject &page)
    {
        QJsonObject p = acc.value(QStringLiteral("p")).toObject();
        const QJsonObject add = page.value(QStringLiteral("p")).toObject();
        for (auto it = add.constBegin(); it != add.constEnd(); ++it)
            p.insert(it.key(), it.value());
        acc.insert(QStringLiteral("p"), p);
        acc.insert(QStringLiteral("m"), page.value(QStringLiteral("m")).toInt());
    }

    inline Palette parseOne(const QJsonArray &arr)
    {
        Palette out;
        const int n = arr.size();
        for (int i = 0; i < n; ++i)
        {
            Stop s;
            const QJsonValue v = arr.at(i);
            if (v.isArray())
            {
                const QJsonArray a = v.toArray();
                if (a.size() < 4)
                    continue;
                s.pos = qBound(0, a.at(0).toInt(), 255);
                s.rgb = qRgb(qBound(0, a.at(1).toInt(), 255), qBound(0, a.at(2).toInt(), 255),
                             qBound(0, a.at(3).toInt(), 255));
            }
            else
            {
                const QString t = v.toString().toLower();
                s.pos = n > 1 ? int(i * 255.0 / (n - 1) + 0.5) : 0;
                if (t == QLatin1String("c1")) s.kind = Stop::Colour1;
                else if (t == QLatin1String("c2")) s.kind = Stop::Colour2;
                else if (t == QLatin1String("c3")) s.kind = Stop::Colour3;
                else if (t == QLatin1String("r")) s.kind = Stop::Random;
                else continue;
            }
            out << s;
        }
        return out;
    }

    /** The palettes of a merged palx document, by WLED palette number (index;
     *  one nobody sent is empty). */
    inline QVector<Palette> parsePalx(const QByteArray &json)
    {
        QVector<Palette> out;
        const QJsonObject p = QJsonDocument::fromJson(json).object()
                                  .value(QStringLiteral("p")).toObject();
        for (auto it = p.constBegin(); it != p.constEnd(); ++it)
        {
            bool ok = false;
            const int id = it.key().toInt(&ok);
            if (!ok || id < 0 || id > 1000)
                continue;
            if (out.size() <= id)
                out.resize(id + 1);
            out[id] = parseOne(it.value().toArray());
        }
        return out;
    }

    inline QRgb lerp(QRgb a, QRgb b, double t)
    {
        return qRgb(int(qRed(a) + (qRed(b) - qRed(a)) * t + 0.5),
                    int(qGreen(a) + (qGreen(b) - qGreen(a)) * t + 0.5),
                    int(qBlue(a) + (qBlue(b) - qBlue(a)) * t + 0.5));
    }

    /** @p n colours along @p pal, left to right; colour slots as @p c1 .. @p c3.
     *  Random is drawn as a soft rainbow (it is a different colour every time). */
    inline QVector<QRgb> colours(const Palette &pal, QRgb c1, QRgb c2, QRgb c3, int n)
    {
        QVector<QRgb> out;
        if (n <= 0)
            return out;
        QVector<QRgb> col;
        QVector<double> pos;
        for (const Stop &s : pal)
        {
            QRgb c = s.rgb;
            switch (s.kind)
            {
            case Stop::Colour1: c = c1; break;
            case Stop::Colour2: c = c2; break;
            case Stop::Colour3: c = c3; break;
            case Stop::Random:
                c = QColor::fromHsv(int(s.pos * 359.0 / 255.0) % 360, 140, 200).rgb();
                break;
            case Stop::Rgb: break;
            }
            col << (c | 0xFF000000u);
            pos << s.pos;
        }
        if (col.isEmpty())
        {
            out.fill(qRgb(64, 64, 64), n);
            return out;
        }
        for (int i = 0; i < n; ++i)
        {
            const double x = n > 1 ? i * 255.0 / (n - 1) : 0.0;
            if (x <= pos.first())
            {
                out << col.first();
                continue;
            }
            if (x >= pos.last())
            {
                out << col.last();
                continue;
            }
            int k = 1;
            while (k < pos.size() - 1 && pos.at(k) < x)
                ++k;
            const double span = pos.at(k) - pos.at(k - 1);
            out << lerp(col.at(k - 1), col.at(k), span > 0 ? (x - pos.at(k - 1)) / span : 1.0);
        }
        return out;
    }

    inline QString storeKey(const QString &mac, const QString &address)
    {
        QString m = mac.trimmed().toLower();
        m.remove(QLatin1Char(':'));
        m.remove(QLatin1Char('-'));
        QString k = m.isEmpty() ? address.trimmed() : m;
        for (QChar &c : k)
            if (!c.isLetterOrNumber())
                c = QLatin1Char('_');
        return QStringLiteral("wled/palettes/") + k;
    }

    inline bool has(const QString &mac, const QString &address)
    {
        QSettings settings;
        return !settings.value(storeKey(mac, address) + QStringLiteral("/palx")).toString().isEmpty();
    }

    inline void save(const QString &mac, const QString &address, const QJsonObject &palx,
                     const QJsonArray &names)
    {
        if (palx.value(QStringLiteral("p")).toObject().isEmpty()
            || (mac.trimmed().isEmpty() && address.trimmed().isEmpty()))
            return;
        QSettings settings;
        const QString key = storeKey(mac, address);
        settings.setValue(key + QStringLiteral("/palx"),
                          QString::fromUtf8(QJsonDocument(palx).toJson(QJsonDocument::Compact)));
        settings.setValue(key + QStringLiteral("/pal"),
                          QString::fromUtf8(QJsonDocument(names).toJson(QJsonDocument::Compact)));
    }
}

#endif

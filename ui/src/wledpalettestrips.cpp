/*
  Q Light Controller Plus
  wledpalettestrips.cpp

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

#include <QLinearGradient>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSettings>
#include <QComboBox>
#include <QPainter>
#include <QFile>

#include "wledpalettestrips.h"
#include "wledfixture.h"
#include "fixture.h"
#include "doc.h"

namespace
{
QByteArray resource(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
}

WledPaletteStrips::Set WledPaletteStrips::bundled()
{
    static const Set set = []() {
        Set s;
        s.palettes = WledPalettes::parsePalx(resource(QStringLiteral(":/wled/palx.json")));
        const QJsonArray n = QJsonDocument::fromJson(resource(QStringLiteral(":/wled/pal.json"))).array();
        for (const QJsonValue &v : n)
            s.names << v.toString();
        return s;
    }();
    return set;
}

WledPaletteStrips::Set WledPaletteStrips::forDevice(const QString &mac, const QString &address)
{
    QSettings settings;
    const QString key = WledPalettes::storeKey(mac, address);
    const QByteArray palx = settings.value(key + QStringLiteral("/palx")).toString().toUtf8();
    if (palx.isEmpty())
        return bundled();
    Set s;
    s.palettes = WledPalettes::parsePalx(palx);
    if (s.palettes.isEmpty())
        return bundled();
    const QJsonArray n = QJsonDocument::fromJson(
        settings.value(key + QStringLiteral("/pal")).toString().toUtf8()).array();
    for (const QJsonValue &v : n)
        s.names << v.toString();
    // A name the device didn't give is the bundled one's (or its number).
    const Set b = bundled();
    for (int i = s.names.size(); i < s.palettes.size(); ++i)
        s.names << (i < b.names.size() ? b.names.at(i) : QString::number(i));
    s.fromDevice = true;
    return s;
}

WledPaletteStrips::Set WledPaletteStrips::forFixtures(const Doc *doc, const QList<quint32> &fixtureIds)
{
    if (doc != nullptr)
    {
        for (quint32 fid : fixtureIds)
        {
            const Fixture *fx = doc->fixture(fid);
            if (fx == nullptr || !WledFixture::isPixelString(fx))
                continue;
            if (fx->ipAddress().isEmpty() && fx->macAddress().isEmpty())
                continue;
            const Set s = forDevice(fx->macAddress(), fx->ipAddress());
            if (s.fromDevice)
                return s;
        }
    }
    return bundled();
}

QPixmap WledPaletteStrips::strip(const Set &set, int index, QRgb c1, QRgb c2, QRgb c3, const QSize &size)
{
    const qreal dpr = 2.0;
    QPixmap pm(size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    if (index >= 0 && index < set.palettes.size() && !set.palettes.at(index).isEmpty())
    {
        const int n = 16;
        const QVector<QRgb> col = WledPalettes::colours(set.palettes.at(index), c1, c2, c3, n);
        QLinearGradient g(0, 0, size.width(), 0);
        for (int i = 0; i < n; ++i)
            g.setColorAt(i / double(n - 1), QColor::fromRgb(col.at(i)));
        p.fillRect(QRect(QPoint(0, 0), size), g);
    }
    else
        p.fillRect(QRect(QPoint(0, 0), size), QColor(64, 64, 64));
    p.setPen(QColor(0, 0, 0, 90));
    p.drawRect(QRect(QPoint(0, 0), size - QSize(1, 1)));
    return pm;
}

void WledPaletteStrips::fill(QComboBox *box, const Set &set, QRgb c1, QRgb c2, QRgb c3)
{
    box->setIconSize(QSize(72, 12));
    const int n = qMax(set.names.size(), set.palettes.size());
    for (int i = 0; i < n; ++i)
        box->addItem(QIcon(strip(set, i, c1, c2, c3, box->iconSize())),
                     i < set.names.size() ? set.names.at(i) : QString::number(i), i);
}

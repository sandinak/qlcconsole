/*
  Q Light Controller Plus
  wledpalettes_test.cpp

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

#include <QSettings>
#include <QComboBox>
#include <QtTest>
#include <QFile>

#include "wledpalettes_test.h"
#include "wledpalettestrips.h"
#include "wledpalettes.h"

namespace
{
QByteArray capture()
{
    QFile f(QString(WLED_OFFICE_DIR) + "palx.json");
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QByteArray one(const char *entries)
{
    return QByteArray("{\"m\":0,\"p\":{\"0\":") + entries + "}}";
}
}

void WledPalettes_Test::init()
{
    QSettings().remove("wled/palettes");
}

void WledPalettes_Test::aCapturedPalxIsReadWhole()
{
    const QByteArray palx = capture();
    QVERIFY2(!palx.isEmpty(), "tools/uxwalk/data/wled-office/palx.json is missing");
    const QVector<WledPalettes::Palette> pals = WledPalettes::parsePalx(palx);
    QCOMPARE(pals.size(), 72);
    for (const WledPalettes::Palette &p : pals)
        QVERIFY(!p.isEmpty());
    QCOMPARE(pals.at(0).size(), 16);                            // Default: 16 stops
    QCOMPARE(pals.at(0).first().rgb, qRgb(155, 0, 213));
    QCOMPARE(pals.at(0).at(1).pos, 16);
    QCOMPARE(pals.at(1).first().kind, WledPalettes::Stop::Random);
    QCOMPARE(pals.at(2).size(), 1);
    QCOMPARE(pals.at(2).first().kind, WledPalettes::Stop::Colour1);
    QCOMPARE(pals.at(4).at(0).kind, WledPalettes::Stop::Colour3);
    QCOMPARE(pals.at(4).at(2).kind, WledPalettes::Stop::Colour1);
    QCOMPARE(pals.at(4).at(2).pos, 255);                        // evenly spread
    QCOMPARE(pals.at(4).at(1).pos, 128);
}

void WledPalettes_Test::pagesMergeIntoOne()
{
    QJsonObject all;
    WledPalettes::mergePage(all, QJsonDocument::fromJson("{\"m\":1,\"p\":{\"0\":[\"c1\"]}}").object());
    WledPalettes::mergePage(all, QJsonDocument::fromJson("{\"m\":1,\"p\":{\"8\":[[0,1,2,3]]}}").object());
    const QVector<WledPalettes::Palette> pals =
        WledPalettes::parsePalx(QJsonDocument(all).toJson(QJsonDocument::Compact));
    QCOMPARE(pals.size(), 9);
    QCOMPARE(pals.at(0).size(), 1);
    QCOMPARE(pals.at(8).first().rgb, qRgb(1, 2, 3));
    QVERIFY(pals.at(5).isEmpty());                              // nobody sent it
}

void WledPalettes_Test::stopsSampleAsAGradient()
{
    const WledPalettes::Palette p = WledPalettes::parsePalx(
        one("[[64,255,0,0],[192,0,0,255]]")).first();
    const QVector<QRgb> c = WledPalettes::colours(p, 0, 0, 0, 5);   // x = 0, 64, 128, 191, 255
    QCOMPARE(c.size(), 5);
    QCOMPARE(c.at(0) & 0xFFFFFF, quint32(0xFF0000));            // before the first stop: clamped
    QCOMPARE(c.at(1) & 0xFFFFFF, quint32(0xFF0000));
    QVERIFY(qAbs(qRed(c.at(2)) - 128) <= 1 && qAbs(qBlue(c.at(2)) - 128) <= 1);   // halfway
    QCOMPARE(c.at(4) & 0xFFFFFF, quint32(0x0000FF));
    QVERIFY(WledPalettes::colours(WledPalettes::Palette(), 0, 0, 0, 3).size() == 3);
    QVERIFY(WledPalettes::colours(p, 0, 0, 0, 0).isEmpty());
}

void WledPalettes_Test::colourSlotsTakeTheEffectsColours()
{
    const QRgb red = qRgb(255, 0, 0), blue = qRgb(0, 0, 255), green = qRgb(0, 255, 0);
    // "* Color 1": solid colour 1.
    const WledPalettes::Palette solid = WledPalettes::parsePalx(one("[\"c1\"]")).first();
    for (QRgb c : WledPalettes::colours(solid, red, blue, green, 4))
        QCOMPARE(c & 0xFFFFFF, red & 0xFFFFFF);
    // "* Colors 1&2": c1,c1,c2,c2 -- starts colour 1, ends colour 2.
    const WledPalettes::Palette two = WledPalettes::parsePalx(one("[\"c1\",\"c1\",\"c2\",\"c2\"]")).first();
    QVector<QRgb> c = WledPalettes::colours(two, red, blue, green, 8);
    QCOMPARE(c.first() & 0xFFFFFF, red & 0xFFFFFF);
    QCOMPARE(c.last() & 0xFFFFFF, blue & 0xFFFFFF);
    // The same palette, other colours 1-2: it follows them.
    c = WledPalettes::colours(two, green, red, blue, 8);
    QCOMPARE(c.first() & 0xFFFFFF, green & 0xFFFFFF);
    QCOMPARE(c.last() & 0xFFFFFF, red & 0xFFFFFF);
    // "* Color Gradient": c3, c2, c1 -- colour 3 to colour 1 through 2.
    const WledPalettes::Palette grad = WledPalettes::parsePalx(one("[\"c3\",\"c2\",\"c1\"]")).first();
    c = WledPalettes::colours(grad, red, blue, green, 256);      // one per position 0-255
    QCOMPARE(c.at(0) & 0xFFFFFF, green & 0xFFFFFF);
    QCOMPARE(c.at(128) & 0xFFFFFF, blue & 0xFFFFFF);
    QCOMPARE(c.at(255) & 0xFFFFFF, red & 0xFFFFFF);
}

void WledPalettes_Test::randomIsDrawnAsARainbow()
{
    const WledPalettes::Palette p = WledPalettes::parsePalx(one("[\"r\",\"r\",\"r\",\"r\"]")).first();
    const QVector<QRgb> c = WledPalettes::colours(p, 0, 0, 0, 4);
    QSet<QRgb> distinct;
    for (QRgb v : c)
        distinct << v;
    QVERIFY(distinct.size() > 1);
}

void WledPalettes_Test::theBundledBuiltInsAreComplete()
{
    const WledPaletteStrips::Set b = WledPaletteStrips::bundled();
    QCOMPARE(b.palettes.size(), 72);
    QCOMPARE(b.names.size(), 72);
    QCOMPARE(b.names.first(), QString("Default"));
    QVERIFY(!b.fromDevice);
    for (int i = 0; i < 72; ++i)
        QVERIFY2(!b.palettes.at(i).isEmpty(), qPrintable(b.names.at(i)));
}

void WledPalettes_Test::aDeviceUsesItsKeptPalettesElseTheBundledOnes()
{
    QVERIFY(!WledPaletteStrips::forDevice("aa:bb:cc:dd:ee:ff", "10.0.0.5").fromDevice);
    QCOMPARE(WledPaletteStrips::forDevice("aa:bb:cc:dd:ee:ff", "10.0.0.5").palettes.size(), 72);

    QJsonObject palx;
    WledPalettes::mergePage(palx, QJsonDocument::fromJson(
        "{\"m\":0,\"p\":{\"0\":[[0,1,2,3]],\"1\":[\"c2\"],\"2\":[\"c3\"]}}").object());
    WledPalettes::save("AA:BB:CC:DD:EE:FF", "10.0.0.5", palx, QJsonArray{ "Mine", "Two" });
    const WledPaletteStrips::Set s = WledPaletteStrips::forDevice("aabbccddeeff", "10.0.0.99");
    QVERIFY(s.fromDevice);
    QCOMPARE(s.palettes.size(), 3);
    QCOMPARE(s.names.size(), 3);
    QCOMPARE(s.names.at(0), QString("Mine"));
    QCOMPARE(s.names.at(2), WledPaletteStrips::bundled().names.at(2));   // not given: the built-in's
    // Another device is not this one.
    QVERIFY(!WledPaletteStrips::forDevice("112233445566", "10.0.0.7").fromDevice);
}

void WledPalettes_Test::theStripIsDrawnFromTheColours()
{
    WledPaletteStrips::Set s;
    s.names << "Two";
    s.palettes << WledPalettes::parsePalx(one("[\"c1\",\"c2\"]")).first();
    const QPixmap pm = WledPaletteStrips::strip(s, 0, qRgb(255, 0, 0), qRgb(0, 0, 255), 0, QSize(60, 10));
    QCOMPARE(pm.size(), QSize(120, 20));                        // drawn at 2x
    QCOMPARE(pm.devicePixelRatio(), 2.0);
    const QImage img = pm.toImage();
    const QColor left = img.pixelColor(3, 10), right = img.pixelColor(116, 10), mid = img.pixelColor(60, 10);
    QVERIFY(left.red() > 230 && left.blue() < 25);
    QVERIFY(right.blue() > 230 && right.red() < 25);
    QVERIFY(mid.red() > 80 && mid.red() < 180 && mid.blue() > 80 && mid.blue() < 180);
    // Other colours 1-2: the same palette follows them.
    const QImage img2 = WledPaletteStrips::strip(s, 0, qRgb(0, 255, 0), qRgb(255, 255, 0), 0,
                                                 QSize(60, 10)).toImage();
    QVERIFY(img2.pixelColor(3, 10).green() > 230 && img2.pixelColor(3, 10).red() < 25);
    QVERIFY(img2.pixelColor(116, 10).red() > 230);
    // A number it has no stops for: a grey strip, not a crash.
    QVERIFY(!WledPaletteStrips::strip(s, 9, 0, 0, 0).isNull());
}

void WledPalettes_Test::theDropdownHasAStripAndANameEach()
{
    QComboBox box;
    WledPaletteStrips::fill(&box, WledPaletteStrips::bundled(), qRgb(255, 0, 0), 0, 0);
    QCOMPARE(box.count(), 72);
    QCOMPARE(box.itemText(6), QString("Party"));
    QCOMPARE(box.itemData(6).toInt(), 6);
    QVERIFY(!box.itemIcon(6).isNull());
    QVERIFY(!box.itemIcon(2).pixmap(72, 12).isNull());
}

QTEST_MAIN(WledPalettes_Test)

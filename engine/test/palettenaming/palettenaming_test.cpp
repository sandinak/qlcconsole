/*
  Q Light Controller Plus - Unit test
  palettenaming_test.cpp

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

#include <QtTest>

#include "palettenaming_test.h"
#include "palettenaming.h"
#include "paletteops.h"
#include "qlcpalette.h"
#include "doc.h"

void PaletteNaming_Test::colourNames_data()
{
    QTest::addColumn<QColor>("rgb");
    QTest::addColumn<QColor>("wauv");
    QTest::addColumn<QString>("name");

    const QColor none;
    QTest::newRow("red")          << QColor(255, 0, 0)     << none << "Red";
    QTest::newRow("dark red")     << QColor(139, 0, 0)     << none << "Dark Red";
    QTest::newRow("light red")    << QColor(255, 120, 120) << none << "Light Red";
    QTest::newRow("blue")         << QColor(0, 0, 255)     << none << "Blue";
    QTest::newRow("dark blue")    << QColor(0, 0, 120)     << none << "Dark Blue";
    QTest::newRow("green")        << QColor(0, 255, 0)     << none << "Green";
    QTest::newRow("dark green")   << QColor(0, 100, 0)     << none << "Dark Green";
    QTest::newRow("amber")        << QColor(255, 176, 0)   << none << "Amber";
    QTest::newRow("magenta")      << QColor(255, 0, 255)   << none << "Magenta";
    QTest::newRow("white")        << QColor(255, 255, 255) << none << "White";
    QTest::newRow("warm white")   << QColor(255, 205, 140) << none << "Warm White";
    QTest::newRow("black")        << QColor(0, 0, 0)       << none << "Black";
    QTest::newRow("W only")       << QColor(0, 0, 0)       << QColor(255, 0, 0) << "White";
    QTest::newRow("A only")       << QColor(0, 0, 0)       << QColor(0, 200, 0) << "Amber";
    QTest::newRow("UV only")      << QColor(0, 0, 0)       << QColor(0, 0, 255) << "UV";
}

void PaletteNaming_Test::colourNames()
{
    QFETCH(QColor, rgb);
    QFETCH(QColor, wauv);
    QFETCH(QString, name);
    QCOMPARE(PaletteNaming::colourName(rgb, wauv), name);
}

void PaletteNaming_Test::followColour_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QColor>("from");
    QTest::addColumn<QColor>("to");
    QTest::addColumn<QString>("expected");      // empty: stays

    const QColor red(255, 0, 0), darkRed(139, 0, 0), blue(0, 0, 255), amber(255, 176, 0);
    QTest::newRow("Red -> Dark Red")             << "Red"              << red << darkRed << "Dark Red";
    QTest::newRow("Dark Red -> Red")             << "Dark Red"         << darkRed << red << "Red";
    QTest::newRow("prefix kept")                 << "Act2 Red"         << red << darkRed << "Act2 Dark Red";
    QTest::newRow("show number and suffix")      << "02-01 Red Wash"   << red << darkRed << "02-01 Dark Red Wash";
    QTest::newRow("suffix kept")                 << "Red (stage left)" << red << blue << "Blue (stage left)";
    QTest::newRow("lower case kept")             << "act2 red"         << red << blue << "act2 blue";
    QTest::newRow("older table's word")          << "Blue"             << QColor(0, 0, 150) << amber << "Amber";
    QTest::newRow("words round it, picked by value") << "02-Blue Moon Red" << red << amber << "02-Blue Moon Amber";
    QTest::newRow("custom name stays")           << "Crimson Tide"     << red << darkRed << "";
    QTest::newRow("another colour's word stays") << "Act2 Blue"        << red << darkRed << "";
    QTest::newRow("no word in a word")           << "Redhead"          << red << blue << "";
    QTest::newRow("same name, no change")        << "Red"              << red << QColor(250, 5, 5) << "";
}

void PaletteNaming_Test::followColour()
{
    QFETCH(QString, name);
    QFETCH(QColor, from);
    QFETCH(QColor, to);
    QFETCH(QString, expected);
    QCOMPARE(PaletteNaming::followColour(name, from, QColor(), QString(), PaletteNaming::colourName(to)),
             expected);
}

void PaletteNaming_Test::followDimmer_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("from");
    QTest::addColumn<int>("to");
    QTest::addColumn<QString>("expected");

    QTest::newRow("percent")             << "Intensity 45%"       << 115 << 153 << "Intensity 60%";
    QTest::newRow("prefix and suffix")   << "Act2 45% Wash"       << 115 << 153 << "Act2 60% Wash";
    QTest::newRow("to full")             << "Intensity 45%"       << 115 << 255 << "Full";
    QTest::newRow("to full, prefix")     << "Act2 Intensity 45%"  << 115 << 255 << "Act2 Full";
    QTest::newRow("from full")           << "Full"                << 255 << 153 << "Intensity 60%";
    QTest::newRow("from off, in a name") << "Act2 Off"            << 0   << 153 << "Act2 60%";
    QTest::newRow("another level stays") << "Intensity 80%"       << 115 << 153 << "";
    QTest::newRow("custom name stays")   << "Half-light"          << 115 << 153 << "";
}

void PaletteNaming_Test::followDimmer()
{
    QFETCH(QString, name);
    QFETCH(int, from);
    QFETCH(int, to);
    QFETCH(QString, expected);
    QCOMPARE(PaletteNaming::followDimmer(name, from, to), expected);
}

void PaletteNaming_Test::followStrobe_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("from");
    QTest::addColumn<int>("to");
    QTest::addColumn<QString>("expected");

    QTest::newRow("percent")      << "Strobe 40%"       << 40  << 70  << "Strobe 70%";
    QTest::newRow("off to rate")  << "Strobe Off"       << 0   << 25  << "Strobe 25%";
    QTest::newRow("rate to max")  << "Chorus Strobe 40%" << 40 << 100 << "Chorus Strobe Max";
    QTest::newRow("custom stays") << "Lightning"        << 40  << 70  << "";
}

void PaletteNaming_Test::followStrobe()
{
    QFETCH(QString, name);
    QFETCH(int, from);
    QFETCH(int, to);
    QFETCH(QString, expected);
    QCOMPARE(PaletteNaming::followStrobe(name, from, to), expected);
}

void PaletteNaming_Test::followPart_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("from");
    QTest::addColumn<QString>("to");
    QTest::addColumn<QString>("expected");

    QTest::newRow("shutter state")    << "Open"            << "Open"   << "Closed"      << "Closed";
    QTest::newRow("kept round it")    << "Act2 Open"       << "Open"   << "Strobe slow" << "Act2 Strobe slow";
    QTest::newRow("gobo")             << "02-01 Stars"     << "Stars"  << "Bubbles"     << "02-01 Bubbles";
    QTest::newRow("custom stays")     << "Blackout hold"   << "Open"   << "Closed"      << "";
    QTest::newRow("no new name")      << "Open"            << "Open"   << ""            << "";
}

void PaletteNaming_Test::followPart()
{
    QFETCH(QString, name);
    QFETCH(QString, from);
    QFETCH(QString, to);
    QFETCH(QString, expected);
    QCOMPARE(PaletteNaming::followPart(name, from, to), expected);
}

void PaletteNaming_Test::factoryNames()
{
    QVERIFY(PaletteNaming::isFactoryName("New Color", "Color"));
    QVERIFY(PaletteNaming::isFactoryName("New Color 3", "Color"));
    QVERIFY(PaletteNaming::isFactoryName("  ", "Color"));
    QVERIFY(!PaletteNaming::isFactoryName("New Colorway", "Color"));
    QVERIFY(!PaletteNaming::isFactoryName("Red", "Color"));
    QVERIFY(!PaletteNaming::isFactoryName("New Dimmer", "Color"));
}

void PaletteNaming_Test::conventionFromSiblings()
{
    // Two agree: their way.
    QCOMPARE(PaletteNaming::withConvention("Amber", { "Act2 Red", "Act2 Blue" }, true),
             QString("Act2 Amber"));
    QCOMPARE(PaletteNaming::withConvention("Amber", { "Act2 Red Wash", "Act2 Blue Wash", "Specials" }, true),
             QString("Act2 Amber Wash"));
    // One, or no agreement, or plain names: none.
    QVERIFY(PaletteNaming::withConvention("Amber", { "Act2 Red" }, true).isEmpty());
    QVERIFY(PaletteNaming::withConvention("Amber", { "Act2 Red", "Act3 Blue" }, true).isEmpty());
    QVERIFY(PaletteNaming::withConvention("Amber", { "Red", "Blue" }, true).isEmpty());
    // Levels.
    QCOMPARE(PaletteNaming::withConvention("30%", { "Verse 45%", "Verse Full" }, false),
             QString("Verse 30%"));
}

void PaletteNaming_Test::aValueStepUndoesTheNameWithIt()
{
    Doc doc(this);
    PaletteOps *ops = doc.paletteOps();
    QVERIFY(ops != nullptr);
    const quint32 id = ops->create(QLCPalette::Color, PaletteOps::pathForFolder("Act2"), "Act2 Red",
                                   { QLCPalette::colorToString(QColor(255, 0, 0), QColor()) });
    QLCPalette *p = doc.palette(id);
    QVERIFY(p != nullptr);
    ops->clearUndo();

    auto edit = [&](const QColor &c) {
        ops->valueStep("Edit", id, [&]() {
            const QColor was = p->rgbValue();
            p->setValue(QLCPalette::colorToString(c, QColor()));
            const QString nm = PaletteNaming::followColour(p->name(), was, QColor(), QString(),
                                                           PaletteNaming::colourName(c));
            if (!nm.isEmpty())
                p->setName(nm);
        }, "drag");
    };
    edit(QColor(200, 0, 0));
    edit(QColor(139, 0, 0));        // the same drag: one step
    QCOMPARE(p->name(), QString("Act2 Dark Red"));
    QCOMPARE(PaletteOps::folderOf(p), QString("Act2"));    // the folder stays
    QVERIFY(ops->canUndo());

    QVERIFY(ops->undo());
    QCOMPARE(p->name(), QString("Act2 Red"));
    QCOMPARE(p->rgbValue(), QColor(255, 0, 0));
    QVERIFY(!ops->canUndo());

    QVERIFY(ops->redo());
    QCOMPARE(p->name(), QString("Act2 Dark Red"));
    QCOMPARE(p->rgbValue(), QColor(139, 0, 0));

    // Nothing changed: no step.
    ops->clearUndo();
    QVERIFY(!ops->valueStep("Edit", id, []() {}));
    QVERIFY(!ops->canUndo());
}

QTEST_MAIN(PaletteNaming_Test)

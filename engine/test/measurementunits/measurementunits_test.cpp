/*
  Q Light Controller Plus - Unit tests
  measurementunits_test.cpp

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

#include <QSettings>
#include <QtTest>

#include "measurementunits_test.h"
#include "measurementunits.h"

typedef MeasurementUnits MU;
Q_DECLARE_METATYPE(MeasurementUnits::LengthUnit)
Q_DECLARE_METATYPE(MeasurementUnits::WeightUnit)

void MeasurementUnits_Test::initTestCase()
{
    // Never the person's own settings.
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
}

void MeasurementUnits_Test::parseLength_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<MU::LengthUnit>("bare");
    QTest::addColumn<double>("mm");

    QTest::newRow("7.5\"")       << "7.5\""       << MU::Millimetres << 190.5;
    QTest::newRow("7.5 in")      << "7.5 in"      << MU::Millimetres << 190.5;
    QTest::newRow("7.5in")       << "7.5in"       << MU::Millimetres << 190.5;
    QTest::newRow("7.5 inches")  << "7.5 inches"  << MU::Millimetres << 190.5;
    QTest::newRow("7.5 IN.")     << " 7.5 IN. "   << MU::Millimetres << 190.5;
    QTest::newRow("7.5''")       << "7.5''"       << MU::Millimetres << 190.5;
    QTest::newRow("7.5 prime")   << QString("7.5") + QChar(0x2033) << MU::Millimetres << 190.5;
    QTest::newRow("0.625'")      << "0.625'"      << MU::Millimetres << 190.5;
    QTest::newRow("0.625 ft")    << "0.625 ft"    << MU::Millimetres << 190.5;
    QTest::newRow("6' 3\"")      << "6' 3\""      << MU::Millimetres << 1905.0;
    QTest::newRow("6'3\"")       << "6'3\""       << MU::Millimetres << 1905.0;
    QTest::newRow("6'3")         << "6'3"         << MU::Millimetres << 1905.0;
    QTest::newRow("6 ft 3 in")   << "6 ft 3 in"   << MU::Millimetres << 1905.0;
    QTest::newRow("6 feet 3")    << "6 feet 3"    << MU::Millimetres << 1905.0;
    QTest::newRow("190.5mm")     << "190.5mm"     << MU::Inches      << 190.5;
    QTest::newRow("19.05 cm")    << "19.05 cm"    << MU::Inches      << 190.5;
    QTest::newRow("0.19 m")      << "0.19 m"      << MU::Inches      << 190.0;
    QTest::newRow("1 m 20 cm")   << "1 m 20 cm"   << MU::Inches      << 1200.0;
    QTest::newRow("7,5 in")      << "7,5 in"      << MU::Millimetres << 190.5;
    QTest::newRow(".5 in")       << ".5 in"       << MU::Millimetres << 12.7;
    QTest::newRow("bare mm")     << "190"         << MU::Millimetres << 190.0;
    QTest::newRow("bare cm")     << "19"          << MU::Centimetres << 190.0;
    QTest::newRow("bare m")      << "0.19"        << MU::Metres      << 190.0;
    QTest::newRow("bare in")     << "7.5"         << MU::Inches      << 190.5;
    QTest::newRow("bare ft")     << "0.625"       << MU::Feet        << 190.5;
    QTest::newRow("empty")       << "  "          << MU::Inches      << 0.0;
    QTest::newRow("zero")        << "0"           << MU::Inches      << 0.0;
}

void MeasurementUnits_Test::parseLength()
{
    QFETCH(QString, text);
    QFETCH(MU::LengthUnit, bare);
    QFETCH(double, mm);

    bool ok = false;
    const double v = MU::parseLength(text, bare, &ok);
    QVERIFY(ok);
    QVERIFY2(qAbs(v - mm) < 1e-9, qPrintable(QString::number(v, 'g', 12)));
}

void MeasurementUnits_Test::parseLengthInvalid_data()
{
    QTest::addColumn<QString>("text");
    QTest::newRow("word")         << "wide";
    QTest::newRow("unknown unit") << "7.5 yd";
    QTest::newRow("weight unit")  << "12 kg";
    QTest::newRow("negative")     << "-5 mm";
    QTest::newRow("thousands")    << "1,200 mm";
    QTest::newRow("two bare")     << "6 3";
    QTest::newRow("bare first")   << "3 6'";
    QTest::newRow("bare after in")<< "3\" 4";
    QTest::newRow("trailing junk")<< "7.5 in x";
    QTest::newRow("unit only")    << "in";
}

void MeasurementUnits_Test::parseLengthInvalid()
{
    QFETCH(QString, text);
    bool ok = true;
    MU::parseLength(text, MU::Millimetres, &ok);
    QVERIFY(!ok);
}

void MeasurementUnits_Test::parseWeight_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<MU::WeightUnit>("bare");
    QTest::addColumn<double>("kg");

    QTest::newRow("12 kg")     << "12 kg"     << MU::Pounds    << 12.0;
    QTest::newRow("12kg")      << "12kg"      << MU::Pounds    << 12.0;
    QTest::newRow("26.5 lb")   << "26.5 lb"   << MU::Kilograms << 26.5 * 0.45359237;
    QTest::newRow("26.5 lbs")  << "26.5 lbs"  << MU::Kilograms << 26.5 * 0.45359237;
    QTest::newRow("400 g")     << "400 g"     << MU::Pounds    << 0.4;
    QTest::newRow("5 lb 4 oz") << "5 lb 4 oz" << MU::Kilograms << 5.25 * 0.45359237;
    QTest::newRow("5 lb 4")    << "5 lb 4"    << MU::Kilograms << 5.25 * 0.45359237;
    QTest::newRow("bare kg")   << "12.5"      << MU::Kilograms << 12.5;
    QTest::newRow("bare lb")   << "10"        << MU::Pounds    << 4.5359237;
    QTest::newRow("empty")     << ""          << MU::Pounds    << 0.0;
}

void MeasurementUnits_Test::parseWeight()
{
    QFETCH(QString, text);
    QFETCH(MU::WeightUnit, bare);
    QFETCH(double, kg);

    bool ok = false;
    const double v = MU::parseWeight(text, bare, &ok);
    QVERIFY(ok);
    QVERIFY2(qAbs(v - kg) < 1e-9, qPrintable(QString::number(v, 'g', 12)));
}

void MeasurementUnits_Test::parseWeightInvalid_data()
{
    QTest::addColumn<QString>("text");
    QTest::newRow("length unit") << "7 in";
    QTest::newRow("unknown")     << "3 stone";
    QTest::newRow("negative")    << "-1 kg";
    QTest::newRow("junk")        << "heavy";
}

void MeasurementUnits_Test::parseWeightInvalid()
{
    QFETCH(QString, text);
    bool ok = true;
    MU::parseWeight(text, MU::Kilograms, &ok);
    QVERIFY(!ok);
}

void MeasurementUnits_Test::formatLength_data()
{
    QTest::addColumn<double>("mm");
    QTest::addColumn<MU::LengthUnit>("unit");
    QTest::addColumn<QString>("text");

    // Exactly what was typed, before it's stored
    QTest::newRow("190.5 in")   << 190.5  << MU::Inches      << "7.5 in";
    QTest::newRow("190.5 mm")   << 190.5  << MU::Millimetres << "190.5 mm";
    QTest::newRow("190.5 cm")   << 190.5  << MU::Centimetres << "19.05 cm";
    QTest::newRow("190.5 m")    << 190.5  << MU::Metres      << "0.1905 m";
    QTest::newRow("190.5 ft")   << 190.5  << MU::Feet        << "7.5\"";
    QTest::newRow("1905 ft")    << 1905.0 << MU::Feet        << "6' 3\"";
    QTest::newRow("1828.8 ft")  << 1828.8 << MU::Feet        << "6'";
    // Whole millimetres, as a file holds them: the fewest decimals that
    // still mean that millimetre.
    QTest::newRow("191 in")     << 191.0  << MU::Inches      << "7.5 in";
    QTest::newRow("190 in")     << 190.0  << MU::Inches      << "7.48 in";
    QTest::newRow("254 in")     << 254.0  << MU::Inches      << "10 in";
    QTest::newRow("191 mm")     << 191.0  << MU::Millimetres << "191 mm";
    QTest::newRow("191 cm")     << 191.0  << MU::Centimetres << "19.1 cm";
    QTest::newRow("191 m")      << 191.0  << MU::Metres      << "0.191 m";
    QTest::newRow("1905 whole ft") << 1905.0 << MU::Feet     << "6' 3\"";
    QTest::newRow("400 ft")     << 400.0  << MU::Feet        << "1' 3.75\"";
    QTest::newRow("8191 ft")    << 8191.0 << MU::Feet        << "26' 10.48\"";
    QTest::newRow("8192 ft")    << 8192.0 << MU::Feet        << "26' 10.5\"";
    QTest::newRow("8190 ft")    << 8190.0 << MU::Feet        << "26' 10.44\"";
    QTest::newRow("0 in")       << 0.0    << MU::Inches      << "0 in";
    QTest::newRow("0 mm")       << 0.0    << MU::Millimetres << "0 mm";
    QTest::newRow("0 ft")       << 0.0    << MU::Feet        << "0\"";
    QTest::newRow("190.33 mm")  << 190.33 << MU::Millimetres << "190.3 mm";
}

void MeasurementUnits_Test::formatLength()
{
    QFETCH(double, mm);
    QFETCH(MU::LengthUnit, unit);
    QFETCH(QString, text);
    QCOMPARE(MU::formatLength(mm, unit), text);
}

void MeasurementUnits_Test::formatWeight_data()
{
    QTest::addColumn<double>("kg");
    QTest::addColumn<MU::WeightUnit>("unit");
    QTest::addColumn<QString>("text");

    QTest::newRow("12 kg")      << 12.0                 << MU::Kilograms << "12 kg";
    QTest::newRow("0.4 kg")     << 0.4                  << MU::Kilograms << "0.4 kg";
    QTest::newRow("26.5 lb")    << 26.5 * 0.45359237    << MU::Pounds    << "26.5 lb";
    QTest::newRow("as saved")   << 12.0202              << MU::Pounds    << "26.5 lb";
    QTest::newRow("12 kg in lb")<< 12.0                 << MU::Pounds    << "26.46 lb";
    QTest::newRow("26.5lb kg")  << 26.5 * 0.45359237    << MU::Kilograms << "12.02 kg";
    QTest::newRow("0")          << 0.0                  << MU::Pounds    << "0 lb";
}

void MeasurementUnits_Test::formatWeight()
{
    QFETCH(double, kg);
    QFETCH(MU::WeightUnit, unit);
    QFETCH(QString, text);
    QCOMPARE(MU::formatWeight(kg, unit), text);
}

void MeasurementUnits_Test::roundTripStoredMm()
{
    for (MU::LengthUnit u : { MU::Millimetres, MU::Centimetres, MU::Metres,
                              MU::Inches, MU::Feet })
    {
        for (int mm = 0; mm <= 9999; mm++)
        {
            const QString shown = MU::formatLength(mm, u);
            bool ok = false;
            const double back = MU::parseLength(shown, u, &ok);
            QVERIFY2(ok, qPrintable(shown));
            QVERIFY2(MU::toStoredMm(back) == mm,
                     qPrintable(QString("%1 mm -> %2 -> %3").arg(mm).arg(shown).arg(back)));
        }
    }
}

void MeasurementUnits_Test::roundTripWeight()
{
    // Shown, then parsed again, is within the display's own precision.
    for (MU::WeightUnit u : { MU::Kilograms, MU::Pounds })
    {
        for (int g = 0; g <= 100000; g += 37)
        {
            const double kg = g / 1000.0;
            const QString shown = MU::formatWeight(kg, u);
            bool ok = false;
            const double back = MU::parseWeight(shown, u, &ok);
            QVERIFY2(ok, qPrintable(shown));
            QVERIFY2(qAbs(back - kg) <= 0.005 * MU::kgPer(u) + 1e-9, qPrintable(shown));
        }
    }
}

void MeasurementUnits_Test::storedMm()
{
    QCOMPARE(MU::toStoredMm(190.5), 191);      // 7.5": halves go up
    QCOMPARE(MU::toStoredMm(190.4999999), 191); // float noise of a sum
    QCOMPARE(MU::toStoredMm(190.49), 190);
    QCOMPARE(MU::toStoredMm(0.0), 0);
    bool ok = false;
    QCOMPARE(MU::toStoredMm(MU::parseLength("7.5\"", MU::Inches, &ok)), 191);
    QCOMPARE(MU::formatLength(191, MU::Inches), QString("7.5 in"));
}

void MeasurementUnits_Test::preference()
{
    QCOMPARE(MU::preferredLength(), MU::Millimetres);   // never set: mm
    MU::setPreferredLength(MU::Inches);
    QCOMPARE(MU::preferredLength(), MU::Inches);
    QCOMPARE(MU::weightUnitFor(MU::Inches), MU::Pounds);
    QCOMPARE(MU::weightUnitFor(MU::Feet), MU::Pounds);
    QCOMPARE(MU::weightUnitFor(MU::Centimetres), MU::Kilograms);
    MU::setPreferredLength(MU::Feet);
    QCOMPARE(MU::preferredLength(), MU::Feet);

    // Stored by key in qlcconsole's own settings, which the fixture editor
    // reads too.
    QSettings s(QSettings::defaultFormat(), QSettings::UserScope,
                QStringLiteral("qlcplus"), QStringLiteral("qlcconsole"));
    QCOMPARE(s.value(MU::settingsKey()).toString(), QString("ft"));
    s.setValue(MU::settingsKey(), QStringLiteral("nonsense"));
    QCOMPARE(MU::preferredLength(), MU::Millimetres);
}

void MeasurementUnits_Test::seedFromFirstShow()
{
    QSettings s(QSettings::defaultFormat(), QSettings::UserScope,
                QStringLiteral("qlcplus"), QStringLiteral("qlcconsole"));
    s.clear();
    s.sync();

    // Never set, first show in feet: feet, once.
    QVERIFY(!MU::isPreferenceSet());
    QVERIFY(MU::seedFromFirstShow(true));
    QCOMPARE(MU::preferredLength(), MU::Feet);
    QVERIFY(MU::isPreferenceSet());

    // Changed afterwards, later shows in feet leave it alone.
    MU::setPreferredLength(MU::Millimetres);
    QVERIFY(!MU::seedFromFirstShow(true));
    QCOMPARE(MU::preferredLength(), MU::Millimetres);

    // Even with the unit unset again, the seed was spent.
    s.remove(MU::settingsKey());
    QVERIFY(!MU::seedFromFirstShow(true));
    QVERIFY(!MU::isPreferenceSet());

    // A first show in metres seeds nothing -- and spends the seed.
    s.clear();
    QVERIFY(!MU::seedFromFirstShow(false));
    QVERIFY(!MU::isPreferenceSet());
    QVERIFY(!MU::seedFromFirstShow(true));
    QCOMPARE(MU::preferredLength(), MU::Millimetres);

    // A unit already chosen is never overridden.
    s.clear();
    MU::setPreferredLength(MU::Centimetres);
    QVERIFY(!MU::seedFromFirstShow(true));
    QCOMPARE(MU::preferredLength(), MU::Centimetres);
    s.clear();
}

QTEST_MAIN(MeasurementUnits_Test)

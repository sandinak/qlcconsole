/*
  Q Light Controller Plus - Unit tests
  editphysical_test.cpp

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

#include <QLabel>
#include <QtTest>

#include "editphysical_test.h"
#include "measurementunits.h"
#include "measurementedit.h"
#include "editphysical.h"

typedef MeasurementUnits MU;

namespace
{
    /* Type into a field as a person would: select all, type, Return */
    void typeInto(QLineEdit *edit, const QString &text)
    {
        edit->setFocus();
        edit->selectAll();
        QTest::keyClick(edit, Qt::Key_Backspace);
        QTest::keyClicks(edit, text);
        QTest::keyClick(edit, Qt::Key_Return);
    }
}

void EditPhysical_Test::cleanup()
{
    // Settings are the test's own (common/testsettings.cpp); back to unset.
    QSettings(QSettings::defaultFormat(), QSettings::UserScope,
              QStringLiteral("qlcplus"), QStringLiteral("qlcconsole")).remove(MU::settingsKey());
}

void EditPhysical_Test::inchesTypedInInches()
{
    MU::setPreferredLength(MU::Inches);
    EditPhysical page(QLCPhysical(), nullptr);
    page.show();
    QCOMPARE(page.m_widthEdit->unit(), MU::Inches);

    typeInto(page.m_widthEdit, QStringLiteral("7.5\""));
    QCOMPARE(page.m_widthEdit->text(), QString("7.5 in"));
    QVERIFY(!page.m_widthEdit->hasError());
    QVERIFY(page.m_widthEdit->toolTip().contains("191 mm"));
    QCOMPARE(page.physical().width(), 191);

    // and as the saved 191 mm comes back
    EditPhysical again(page.physical(), nullptr);
    again.setUnit(MU::Inches);
    QCOMPARE(again.m_widthEdit->text(), QString("7.5 in"));
}

void EditPhysical_Test::inchesTypedInMillimetres()
{
    EditPhysical page(QLCPhysical(), nullptr);   // never set: mm
    page.show();
    QCOMPARE(page.m_widthEdit->unit(), MU::Millimetres);

    typeInto(page.m_widthEdit, QStringLiteral("7.5\""));
    QCOMPARE(page.m_widthEdit->text(), QString("190.5 mm"));
    QCOMPARE(page.physical().width(), 191);

    typeInto(page.m_heightEdit, QStringLiteral("6' 3\""));
    QCOMPARE(page.m_heightEdit->text(), QString("1905 mm"));
    typeInto(page.m_depthEdit, QStringLiteral("19.05 cm"));
    QCOMPARE(page.m_depthEdit->text(), QString("190.5 mm"));
    QCOMPARE(page.physical().height(), 1905);
    QCOMPARE(page.physical().depth(), 191);
}

void EditPhysical_Test::bareNumber()
{
    EditPhysical page(QLCPhysical(), nullptr);
    page.setUnit(MU::Centimetres);
    typeInto(page.m_widthEdit, QStringLiteral("30"));
    QCOMPARE(page.m_widthEdit->text(), QString("30 cm"));
    QCOMPARE(page.physical().width(), 300);

    page.setUnit(MU::Feet);
    QCOMPARE(page.m_widthEdit->text(), QString("11.8\""));
    typeInto(page.m_widthEdit, QStringLiteral("1.5"));
    QCOMPARE(page.m_widthEdit->text(), QString("1' 6\""));
    QCOMPARE(page.physical().width(), 457);
}

void EditPhysical_Test::weight()
{
    EditPhysical page(QLCPhysical(), nullptr);
    page.setUnit(MU::Inches);
    typeInto(page.m_weightEdit, QStringLiteral("26.5 lbs"));
    QCOMPARE(page.m_weightEdit->text(), QString("26.5 lb"));
    QVERIFY(qAbs(page.physical().weight() - 26.5 * 0.45359237) < 1e-9);

    page.setUnit(MU::Millimetres);
    QCOMPARE(page.m_weightEdit->text(), QString("12.02 kg"));
    typeInto(page.m_weightEdit, QStringLiteral("400 g"));
    QCOMPARE(page.m_weightEdit->text(), QString("0.4 kg"));
    QCOMPARE(page.physical().weight(), 0.4);
}

void EditPhysical_Test::invalid()
{
    QLCPhysical phys;
    phys.setWidth(300);
    EditPhysical page(phys, nullptr);
    page.setUnit(MU::Inches);
    QCOMPARE(page.m_widthEdit->text(), QString("11.8 in"));

    typeInto(page.m_widthEdit, QStringLiteral("wide"));
    QVERIFY(page.m_widthEdit->hasError());
    QCOMPARE(page.m_widthEdit->property("invalid").toBool(), true);
    QCOMPARE(page.m_widthEdit->text(), QString("wide"));   // left to fix
    QVERIFY(page.m_widthEdit->toolTip().contains("7.5\""));
    QVERIFY(page.m_widthEdit->toolTip().contains("11.8 in"));
    QCOMPARE(page.physical().width(), 300);                // unchanged

    typeInto(page.m_widthEdit, QStringLiteral("500 m"));   // too big
    QVERIFY(page.m_widthEdit->hasError());
    QCOMPARE(page.physical().width(), 300);

    typeInto(page.m_widthEdit, QStringLiteral("12"));      // fixed
    QVERIFY(!page.m_widthEdit->hasError());
    QCOMPARE(page.m_widthEdit->property("invalid").toBool(), false);
    QCOMPARE(page.physical().width(), 305);
}

void EditPhysical_Test::loadShowsPreferredUnit()
{
    QLCPhysical phys;
    phys.setWidth(190);
    phys.setHeight(1905);
    phys.setDepth(0);
    phys.setWeight(12.0202);   // 26.5 lb as a file keeps it
    EditPhysical page(phys, nullptr);

    page.setUnit(MU::Inches);
    QCOMPARE(page.m_widthEdit->text(), QString("7.48 in"));
    QCOMPARE(page.m_heightEdit->text(), QString("75 in"));
    QCOMPARE(page.m_depthEdit->text(), QString("0 in"));
    QCOMPARE(page.m_weightEdit->text(), QString("26.5 lb"));

    page.setUnit(MU::Metres);
    QCOMPARE(page.m_widthEdit->text(), QString("0.19 m"));
    QCOMPARE(page.m_weightEdit->text(), QString("12.02 kg"));

    // Nothing typed: nothing changes, not even the weight's last digits.
    const QLCPhysical out = page.physical();
    QCOMPARE(out.width(), 190);
    QCOMPARE(out.height(), 1905);
    QCOMPARE(out.depth(), 0);
    QCOMPARE(out.weight(), 12.0202);
}

void EditPhysical_Test::uncommittedText()
{
    EditPhysical page(QLCPhysical(), nullptr);
    page.setUnit(MU::Inches);
    page.m_widthEdit->setFocus();
    page.m_widthEdit->selectAll();
    QTest::keyClicks(page.m_widthEdit, QStringLiteral("10"));   // no Return
    QCOMPARE(page.physical().width(), 254);
}

void EditPhysical_Test::followsPreference()
{
    EditPhysical page(QLCPhysical(), nullptr);
    page.show();
    QCOMPARE(page.m_widthEdit->unit(), MU::Millimetres);
    QLabel *note = page.findChild<QLabel *>(QStringLiteral("m_unitsNote"));
    QVERIFY(note != nullptr);
    QVERIFY(note->text().contains("mm and kg"));

    page.hide();
    MU::setPreferredLength(MU::Feet);   // as Settings does in qlcconsole
    page.show();
    QCOMPARE(page.m_widthEdit->unit(), MU::Feet);
    QCOMPARE(page.m_weightEdit->unit(), MU::Feet);
    QVERIFY(note->text().contains("ft and lb"));
}

QTEST_MAIN(EditPhysical_Test)

/*
  Q Light Controller Plus - Test Unit
  lookeditor_test.cpp

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
#include <QSlider>
#include <QLabel>

#define private public
#include "lookeditor.h"
#undef private
#include "qlcpalette.h"
#include "doc.h"

#include "lookeditor_test.h"

void LookEditor_Test::aColourDarkerThanAFifthIsCalledOut()
{
    const QColor none;                                  // no white/amber/UV stored
    // The "Red" that shipped in the demo show: red 10, blue 2.
    const QString dim = LookEditor::darkColourWarning(QColor(10, 0, 2), none);
    QVERIFY2(dim.contains("4%") && dim.contains("Val"), qPrintable(dim));
    QVERIFY(dim.contains("Colour-wheel"));

    QVERIFY(LookEditor::darkColourWarning(QColor(0, 0, 0), QColor(0, 0, 0)).contains("black"));
    QVERIFY(LookEditor::darkColourWarning(QColor(1, 0, 0), none).contains("1%"));
    QVERIFY(LookEditor::darkColourWarning(QColor(50, 0, 0), none).contains("20%"));

    // Bright enough, by RGB or by an extra emitter: nothing to say.
    QVERIFY(LookEditor::darkColourWarning(QColor(51, 0, 0), none).isEmpty());
    QVERIFY(LookEditor::darkColourWarning(QColor(255, 0, 0), none).isEmpty());
    QVERIFY(LookEditor::darkColourWarning(QColor(0, 0, 0), QColor(200, 0, 0)).isEmpty());
}

void LookEditor_Test::aDarkRedLookWarnsUntilItIsBrightened()
{
    Doc doc(this);
    QLCPalette *red = new QLCPalette(QLCPalette::Color, &doc);
    red->setName("Red");
    red->setValue("#0a0002000000");
    QVERIFY(doc.addPalette(red));

    LookEditor ed(&doc);
    ed.setPalette(red->id());
    QVERIFY(!ed.m_colorWarning->isHidden());
    QVERIFY2(ed.m_colorWarning->text().contains("4%"), qPrintable(ed.m_colorWarning->text()));

    // Brightened with the R slider: the look is red now, and says nothing.
    ed.m_redSlider->setValue(255);
    QVERIFY(ed.m_colorWarning->isHidden());
    QCOMPARE(red->rgbValue().red(), 255);

    // Dragged back down to near nothing: said again.
    ed.m_redSlider->setValue(20);
    QVERIFY(!ed.m_colorWarning->isHidden());
    QVERIFY(ed.m_colorWarning->text().contains("8%"));
}

QTEST_MAIN(LookEditor_Test)

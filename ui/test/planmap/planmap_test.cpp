/*
  Q Light Controller Plus
  planmap_test.cpp

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

#include <QSignalSpy>
#include <QtTest>
#include <QDir>
#include <cmath>

#include "planmapwidget.h"
#include "measurementunits.h"

#include "planmap_test.h"

/* A 30' x 20' stage (world mm), CL at 15', plaster line at 20' (the front),
   the plot origin at the centre of the stage, 1 ft grid. */
static void setup(PlanMapWidget &w)
{
    w.setStage(QRectF(0, 0, 9144, 6096), 4572, 6096);
    w.setOrigin(QPointF(4572, 3048));
    w.setGridStep(304.8);
    w.show();
}

void PlanMap_Test::init()
{
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
}

void PlanMap_Test::labelsInFeet()
{
    PlanMapWidget w;
    setup(w);
    // 10' 1/2" right of the origin (= of CL), 12' 1/4" upstage of... plaster is the
    // front, so the origin's 10' above it plus 2' 1/4": y = -(12' 1/4" - 10') from origin.
    const double x = 10 * 304.8 + 12.7;                      // 10' 1/2"
    const double yFromPlaster = 12 * 304.8 + 6.35;           // 12' 1/4"
    w.setPosition(x, 6096 - yFromPlaster - 3048);
    QCOMPARE(w.centreLineText(), MeasurementUnits::formatLength(x) + QStringLiteral(" SL of CL"));
    QVERIFY2(w.centreLineText().startsWith(QStringLiteral("10' 1/2\"")), qPrintable(w.centreLineText()));
    QVERIFY2(w.plasterLineText().startsWith(QStringLiteral("12' 1/4\"")), qPrintable(w.plasterLineText()));
    QVERIFY(w.plasterLineText().endsWith(QStringLiteral("from plaster line")));
}

void PlanMap_Test::labelsInMetres()
{
    MeasurementUnits::setPreferredLength(MeasurementUnits::Metres);
    PlanMapWidget w;
    setup(w);
    w.setPosition(-3000, 1000);                              // 3 m SR of CL, 2.048 m from the plaster line
    QVERIFY2(w.centreLineText().startsWith(QStringLiteral("3 m")), qPrintable(w.centreLineText()));
    QCOMPARE(w.plasterLineText(), MeasurementUnits::formatLength(6096 - 4048)
                                      + QStringLiteral(" from plaster line"));
    QVERIFY(!w.centreLineText().contains(QLatin1Char('\'')));
    QVERIFY2(w.centreLineText().endsWith(QStringLiteral("SR of CL")), qPrintable(w.centreLineText()));
}

void PlanMap_Test::onTheLinesSaysSo()
{
    PlanMapWidget w;
    setup(w);
    w.setPosition(0, 3048);                                  // on CL and on the plaster line
    QCOMPARE(w.centreLineText(), QStringLiteral("on CL"));
    QCOMPARE(w.plasterLineText(), QStringLiteral("on plaster line"));
}

void PlanMap_Test::dragEmitsSnappedPosition()
{
    PlanMapWidget w;
    setup(w);
    w.setPosition(0, 0);
    QSignalSpy spy(&w, &PlanMapWidget::positionChanged);
    const QPointF from = w.dotPoint();
    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, (from + QPointF(23.3, 11.7)).toPoint());
    QVERIFY(spy.count() >= 1);
    const QList<QVariant> a = spy.last();
    const double gx = a.at(0).toDouble(), gy = a.at(1).toDouble();
    QVERIFY(gx != 0 || gy != 0);
    QVERIFY(qAbs(gx / 304.8 - std::round(gx / 304.8)) < 1e-9);   // on the foot grid
    QVERIFY(qAbs(gy / 304.8 - std::round(gy / 304.8)) < 1e-9);
    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, (from + QPointF(23, 12)).toPoint());
    QCOMPARE(w.position(), QPointF(gx, gy));
}

void PlanMap_Test::shiftDragIsFree()
{
    PlanMapWidget w;
    setup(w);
    w.setPosition(0, 0);
    QSignalSpy spy(&w, &PlanMapWidget::positionChanged);
    const QPointF from = w.dotPoint();
    QTest::mousePress(&w, Qt::LeftButton, Qt::ShiftModifier, (from + QPointF(23.3, 11.7)).toPoint());
    QVERIFY(spy.count() >= 1);
    const double gx = spy.last().at(0).toDouble(), gy = spy.last().at(1).toDouble();
    const double ox = qAbs(gx - std::round(gx / 304.8) * 304.8);
    const double oy = qAbs(gy - std::round(gy / 304.8) * 304.8);
    QVERIFY2(ox > 1.0 || oy > 1.0,
             "shift drag should not land on the grid");
}

void PlanMap_Test::mountedIsNotEditableAndSaysWhy()
{
    PlanMapWidget w;
    setup(w);
    w.setPosition(1000, 500);
    w.setEditable(false, QStringLiteral("On Truss SR 1: its position follows the truss"));
    QVERIFY(!w.isEditable());
    QSignalSpy spy(&w, &PlanMapWidget::positionChanged);
    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
    QTest::mouseMove(&w, QPoint(60, 60));
    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, QPoint(60, 60));
    QCOMPARE(spy.count(), 0);
    QCOMPARE(w.position(), QPointF(1000, 500));
    QCOMPARE(w.captionText(), QStringLiteral("On Truss SR 1: its position follows the truss"));
    QCOMPARE(w.toolTip(), w.reason());
    w.setEditable(true);
    QVERIFY(w.reason().isEmpty());
    QVERIFY(w.captionText().contains(QStringLiteral("plot origin")));
}

void PlanMap_Test::settersDoNotEmit()
{
    PlanMapWidget w;
    QSignalSpy spy(&w, &PlanMapWidget::positionChanged);
    setup(w);
    w.setPosition(500, 600);
    w.setOrigin(QPointF(0, 0));
    w.setGridStep(100);
    w.setEditable(false, QStringLiteral("x"));
    w.setEditable(true);
    w.setStage(QRectF(0, 0, 1000, 1000), 500, 1000);
    QCOMPARE(spy.count(), 0);
}

void PlanMap_Test::picturesForReview()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    PlanMapWidget w;
    setup(w);
    w.setPosition(10 * 304.8 + 12.7, -2 * 304.8);
    QVERIFY(w.grab().save(dir + QStringLiteral("/planmap-feet.png")));
    MeasurementUnits::setPreferredLength(MeasurementUnits::Metres);
    QVERIFY(w.grab().save(dir + QStringLiteral("/planmap-metres.png")));
    w.setEditable(false, QStringLiteral("On Truss SR 1: position follows the mount"));
    QVERIFY(w.grab().save(dir + QStringLiteral("/planmap-mounted.png")));
}

QTEST_MAIN(PlanMap_Test)

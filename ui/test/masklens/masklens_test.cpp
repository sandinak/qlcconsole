/*
  Q Light Controller Plus
  masklens_test.cpp

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
#include <QSignalSpy>
#include <QDir>
#include <QPainter>
#include <QImage>

#include "masklens_test.h"
#include "masklenswidget.h"

namespace
{
void drag(MaskLensWidget &w, MaskLensWidget::Side s, QPointF to)
{
    const QPoint from = w.edgePoint(s).toPoint();
    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(&w, to.toPoint());
    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, to.toPoint());
}
}

void MaskLens_Test::percentForKnownMasks()
{
    MaskLensWidget w;
    w.setLensAngle(40.0);          // half angle 20
    QCOMPARE(w.percentThrough(), 100.0);
    for (int side = 0; side < 4; ++side)
    {
        LightMask m;
        float *f[] = { &m.top, &m.bottom, &m.left, &m.right };
        *f[side] = 20.0f;          // tape to the middle: a half-disc
        w.setMask(m);
        QVERIFY2(qAbs(w.percentThrough() - 50.0) < 0.1, qPrintable(QString::number(w.percentThrough())));
    }
    w.setMask(LightMask{ 10.f, 10.f, 10.f, 10.f });   // a 20 x 20 square in a r=20 disc
    QVERIFY(qAbs(w.percentThrough() - 100.0 * 400.0 / (M_PI * 400.0)) < 0.1);
    w.setMask(LightMask{ 20.f, 20.f, 0.f, 0.f });     // nothing left
    QCOMPARE(w.percentThrough(), 0.0);
    // The label says it.
    QVERIFY(w.accessibleDescription().contains(QStringLiteral("0% of the beam gets through")));
}

void MaskLens_Test::percentOfAScanWindow()
{
    MaskLensWidget w;
    w.setScanWindow(40.0, 20.0);
    QCOMPARE(w.percentThrough(), 100.0);
    w.setMask(LightMask{ 5.f, 0.f, 10.f, 0.f });      // a quarter off the height, a quarter off the width
    QVERIFY(qAbs(w.percentThrough() - 100.0 * (30.0 * 15.0) / (40.0 * 20.0)) < 0.001);
}

void MaskLens_Test::everyEdgeDragEmitsItsSide()
{
    MaskLensWidget w;
    w.setLensAngle(40.0);
    w.show();
    const double k = 40.0 / 40.0;   // placeholder to keep the maths below readable
    Q_UNUSED(k);
    const QPointF top0 = w.edgePoint(MaskLensWidget::Top);
    const QPointF bot0 = w.edgePoint(MaskLensWidget::Bottom);
    const double ppd = (bot0.y() - top0.y()) / 40.0;   // pixels per degree

    QSignalSpy spy(&w, &MaskLensWidget::maskChanged);
    drag(w, MaskLensWidget::Top, top0 + QPointF(0, 5 * ppd));
    QVERIFY(spy.count() >= 1);
    QList<QVariant> last = spy.last();
    QCOMPARE(last.at(0).toFloat(), 5.0f);
    QCOMPARE(last.at(1).toFloat(), 0.0f);
    QCOMPARE(last.at(2).toFloat(), 0.0f);
    QCOMPARE(last.at(3).toFloat(), 0.0f);

    spy.clear();
    drag(w, MaskLensWidget::Bottom, bot0 - QPointF(0, 8 * ppd));
    last = spy.last();
    QCOMPARE(last.at(1).toFloat(), 8.0f);
    QCOMPARE(last.at(0).toFloat(), 5.0f);

    spy.clear();
    const QPointF l0 = w.edgePoint(MaskLensWidget::Left);
    drag(w, MaskLensWidget::Left, l0 + QPointF(3 * ppd, 0));
    last = spy.last();
    QCOMPARE(last.at(2).toFloat(), 3.0f);

    spy.clear();
    const QPointF r0 = w.edgePoint(MaskLensWidget::Right);
    drag(w, MaskLensWidget::Right, r0 - QPointF(12.5 * ppd, 0));
    last = spy.last();
    QCOMPARE(last.at(3).toFloat(), 12.5f);
    QCOMPARE(w.mask().right, 12.5f);
    QCOMPARE(w.mask().left, 3.0f);
}

void MaskLens_Test::settersDoNotEmit()
{
    MaskLensWidget w;
    QSignalSpy spy(&w, &MaskLensWidget::maskChanged);
    w.setLensAngle(30.0);
    w.setScanWindow(40.0, 20.0);
    w.setMask(1.f, 2.f, 3.f, 4.f);
    w.setMask(LightMask{ 1.f, 1.f, 1.f, 1.f });
    QCOMPARE(spy.count(), 0);
    QCOMPARE(w.mask().top, 1.0f);
}

void MaskLens_Test::valuesClampToTheHalfAngle()
{
    MaskLensWidget w;
    w.setLensAngle(30.0);          // half 15
    w.setMask(99.f, -4.f, 15.5f, 15.f);
    QCOMPARE(w.mask().top, 15.0f);
    QCOMPARE(w.mask().bottom, 0.0f);
    QCOMPARE(w.mask().left, 15.0f);
    QCOMPARE(w.mask().right, 15.0f);
    // A drag far past the other side stops at the half angle too.
    w.setMask(0.f, 0.f, 0.f, 0.f);
    w.show();
    QSignalSpy spy(&w, &MaskLensWidget::maskChanged);
    drag(w, MaskLensWidget::Top, QPointF(70, 5000));
    QCOMPARE(spy.last().at(0).toFloat(), 15.0f);
    // And a half-degree step.
    w.setMask(0.f, 0.f, 0.f, 0.f);
    const QPointF e = w.edgePoint(MaskLensWidget::Left);
    const double ppd = (w.edgePoint(MaskLensWidget::Right).x() - e.x()) / 30.0;
    drag(w, MaskLensWidget::Left, e + QPointF(2.5 * ppd, 0));
    QCOMPARE(w.mask().left, 2.5f);
}

void MaskLens_Test::shot()
{
    const QByteArray dir = qgetenv("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(QString::fromLocal8Bit(dir));
    MaskLensWidget a, b, c;
    a.setLensAngle(40.0);
    b.setLensAngle(40.0);
    b.setMask(0.f, 12.f, 0.f, 0.f);
    c.setScanWindow(40.0, 20.0);
    c.setMask(3.f, 3.f, 8.f, 8.f);
    QImage img(3 * 150 + 10, 150, QImage::Format_ARGB32);
    img.fill(QColor(40, 40, 40));
    QPainter p(&img);
    int x = 5;
    for (MaskLensWidget *w : { &a, &b, &c })
    {
        w->show();
        p.drawImage(x, 5, w->grab().toImage());
        x += 150;
    }
    p.end();
    QVERIFY(img.save(QString::fromLocal8Bit(dir) + QStringLiteral("/masklens.png")));
}

QTEST_MAIN(MaskLens_Test)

/*
  Q Light Controller Plus - Unit test
  stickfilter_test.cpp

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

#include "stickfilter_test.h"
#include "stickfilter.h"

namespace
{
const int LX = 0, LY = 1, LT = 2;

// A -1..1 position as the plugin reads it.
uchar at(double norm)
{
    return StickFilter::fromNorm(norm);
}

StickFilter pad(qint64 now = 0)
{
    StickFilter f;
    f.setAxes({ LX, LY, 3, 4 });
    f.setDeadzone(0.06);
    f.begin(now);
    return f;
}

// Ticks every 100 ms from @p from to @p to, as the plugin's timer does.
StickFilter::Tick run(StickFilter &f, qint64 from, qint64 to)
{
    StickFilter::Tick all;
    for (qint64 t = from; t <= to; t += 100)
    {
        const StickFilter::Tick one = f.tick(t);
        all.learnt << one.learnt;
        all.captureDone = all.captureDone || one.captureDone;
        all.held += one.held;
        if (one.drifted >= 0)
        {
            all.drifted = one.drifted;
            all.driftOffset = one.driftOffset;
        }
    }
    return all;
}
}

void StickFilter_Test::aCentredStickIsUntouched()
{
    StickFilter f = pad();
    for (int v = 0; v <= 255; ++v)
        QCOMPARE(int(f.feed(LX, uchar(v), 0)), v);
    QCOMPARE(int(f.feed(LX, 128, 0)), 128);
    run(f, 0, 2500);
    QVERIFY(f.isLearnt(LX));
    QCOMPARE(f.rest(LX), 0.0);
    for (int v = 0; v <= 255; ++v)
        QCOMPARE(int(f.feed(LX, uchar(v), 3000)), v);
}

void StickFilter_Test::aRestingStickIsLearntAsItArrives()
{
    StickFilter f = pad();
    // Before it is learnt the reading is what it is.
    QCOMPARE(int(f.feed(LX, at(0.10), 10)), int(at(0.10)));
    QCOMPARE(int(f.feed(LY, at(-0.07), 10)), int(at(-0.07)));
    // A little noise is still resting.
    f.feed(LX, uchar(at(0.10) + 1), 150);
    const StickFilter::Tick t = run(f, 100, 600);
    QVERIFY(t.learnt.contains(LX));
    QVERIFY(t.learnt.contains(LY));
    // Within a count of the middle (it learnt a noisy reading's middle).
    QVERIFY(qAbs(int(f.output(LX)) - 128) <= 1);
    QCOMPARE(int(f.output(LY)), 128);
    QVERIFY(qAbs(f.rest(LX) - 0.10) < 0.02);
    QVERIFY(qAbs(f.rest(LY) + 0.07) < 0.02);
    // And so from now on.
    QVERIFY(qAbs(int(f.feed(LX, at(0.10), 700)) - 128) <= 1);
    QVERIFY(!f.needsTick() || f.tick(2100).drifted < 0);
}

void StickFilter_Test::fullTravelStillReachesBothEnds()
{
    StickFilter f = pad();
    f.feed(LX, at(0.10), 0);
    f.feed(LY, at(-0.07), 0);
    run(f, 0, 500);
    QCOMPARE(int(f.feed(LX, 255, 600)), 255);
    QCOMPARE(int(f.feed(LX, 0, 610)), 0);
    QCOMPARE(int(f.feed(LY, 255, 620)), 255);
    QCOMPARE(int(f.feed(LY, 0, 630)), 0);
    // Monotonic in between: no step, no fold.
    int last = -1;
    for (int v = 0; v <= 255; ++v)
    {
        const int out = f.feed(LX, uchar(v), 700);
        QVERIFY2(out >= last, qPrintable(QString::number(v)));
        last = out;
    }
    // Halfway past the rest is about half travel.
    QVERIFY(qAbs(StickFilter::correct(0.55, 0.10) - 0.5) < 1e-9);
    QVERIFY(qAbs(StickFilter::correct(-0.45, 0.10) + 0.5) < 1e-9);
}

void StickFilter_Test::bothWaysReadTheSame()
{
    // A stick resting at -0.12, pushed halfway right and halfway left.
    const double rest = -0.12;
    const double right = rest + 0.5 * (1.0 - rest), left = rest - 0.5 * (1.0 + rest);
    // As read: right 0.44, left -0.56 -- right is a quarter slower.
    QVERIFY(qAbs(right) < qAbs(left) * 0.8);
    // Corrected: the same either way.
    QVERIFY(qAbs(StickFilter::correct(right, rest) - 0.5) < 1e-9);
    QVERIFY(qAbs(StickFilter::correct(left, rest) + 0.5) < 1e-9);
}

void StickFilter_Test::aMovingOrHeldStickIsNotLearnt()
{
    StickFilter f = pad();
    // LX keeps moving all through the window; LY is held well out.
    for (qint64 t = 0; t <= 2000; t += 50)
        f.feed(LX, at((t / 50) % 2 ? 0.2 : -0.2), t);
    f.feed(LY, at(0.45), 0);
    run(f, 0, 2100);
    QVERIFY(!f.isLearnt(LX));
    QVERIFY(!f.isLearnt(LY));
    QCOMPARE(int(f.output(LY)), int(at(0.45)));
    // A trigger at its end (-1) is not a stick resting either.
    StickFilter g;
    g.setAxes({ LT });
    g.begin(0);
    g.feed(LT, 0, 0);
    run(g, 0, 2100);
    QVERIFY(!g.isLearnt(LT));
}

void StickFilter_Test::aHeldPushAfterwardsIsKept()
{
    StickFilter f = pad();
    f.feed(LX, 128, 0);
    run(f, 0, 2100);
    // A slow pan: pushed to 0.2 and held there, still, for a long time.
    for (qint64 t = 2200; t <= 2600; t += 50)
        f.feed(LX, at(0.2 * (t - 2200) / 400.0), t);
    run(f, 2700, 20000);
    QVERIFY(qAbs(StickFilter::toNorm(f.output(LX)) - 0.2) < 0.01);
    QCOMPARE(f.rest(LX), 0.0);
}

void StickFilter_Test::driftIsSaidOnce()
{
    StickFilter f = pad();
    f.setDriftTime(3000);
    // Held as it arrived, so its rest was not learnt...
    f.feed(LX, at(0.2), 0);
    for (qint64 t = 0; t <= 2000; t += 100)
        f.feed(LX, at(t % 200 ? 0.35 : 0.2), t);
    run(f, 0, 2100);
    QVERIFY(!f.isLearnt(LX));
    // ...then let go, it sits at +0.20 (past the deadzone, +-0.12).
    f.feed(LX, at(0.2), 2200);
    StickFilter::Tick t = run(f, 2300, 4400);
    QCOMPARE(t.drifted, -1);                     // not yet 3 s
    // A button pressed puts it off.
    f.feed(20, 255, 4500);
    t = run(f, 4600, 7400);
    QCOMPARE(t.drifted, -1);
    t = run(f, 7500, 7700);
    QCOMPARE(t.drifted, LX);
    QVERIFY(qAbs(t.driftOffset - 0.2) < 0.01);
    // Once.
    QVERIFY(!f.needsTick());
    t = run(f, 8300, 30000);
    QCOMPARE(t.drifted, -1);
    // Nothing was re-centred by itself.
    QCOMPARE(int(f.output(LX)), int(at(0.2)));
}

void StickFilter_Test::centreSticksLearnsAgain()
{
    StickFilter f = pad();
    f.feed(LX, 128, 0);
    f.feed(LY, 128, 0);
    run(f, 0, 2100);
    // Later the stick comes to rest at 0.4 (worn), and LY is held at 0.8.
    f.feed(LX, at(0.4), 3000);
    f.feed(LY, at(0.8), 3000);
    f.requestCapture(3100);
    QVERIFY(f.isCapturing());
    // Not before it has held still from the request on.
    StickFilter::Tick t = f.tick(3300);
    QVERIFY(!t.learnt.contains(LX));
    t = run(f, 3400, 7000);
    QVERIFY(t.learnt.contains(LX));
    QVERIFY(t.captureDone);
    QCOMPARE(t.held, 1);                         // LY: held, left as it was
    QCOMPARE(int(f.output(LX)), 128);
    QCOMPARE(int(f.output(LY)), int(at(0.8)));
    QVERIFY(!f.isCapturing());
    // All let go: done as soon as each has held still.
    f.feed(LY, 128, 8000);
    f.requestCapture(8000);
    t = run(f, 8100, 8500);
    QVERIFY(t.captureDone);
    QCOMPARE(t.held, 0);
}

void StickFilter_Test::aRememberedRestIsUsed()
{
    StickFilter f;
    f.setAxes({ LX, LY });
    f.setRest(LX, 0.10);
    f.setRest(LY, 0.9);                          // nonsense: not taken
    QVERIFY(f.isLearnt(LX));
    QVERIFY(!f.isLearnt(LY));
    QCOMPARE(int(f.feed(LX, at(0.10), 0)), 128);
    QCOMPARE(f.rests().value(LX), 0.10);
    QVERIFY(!f.rests().contains(LY));
}

void StickFilter_Test::otherChannelsPass()
{
    StickFilter f = pad();
    for (int v = 0; v <= 255; v += 5)
    {
        QCOMPARE(int(f.feed(LT, uchar(v), 0)), v);
        QCOMPARE(int(f.feed(20, uchar(v), 0)), v);
    }
    QVERIFY(!f.isAxis(LT));
}

QTEST_GUILESS_MAIN(StickFilter_Test)

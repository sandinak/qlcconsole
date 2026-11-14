/*
  Q Light Controller Plus - Test Unit
  inputdevicenotices_test.h

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

#include "mappingcheck_test.h"
#include "mappingcheck.h"

using R = GamepadRole::Role;

namespace
{
void press(MappingCheck &c, R role)
{
    c.feed(role, 255);
    c.feed(role, 0);
}
}

void MappingCheck_Test::theWalkOrderAndNames()
{
    MappingCheck c(PadNames::Xbox);
    QCOMPARE(c.count(), 20);
    QCOMPARE(c.step(0).role, R::A);
    QCOMPARE(c.step(0).name, QString("A"));
    QVERIFY(c.step(0).hint.contains("green"));
    // face, shoulders, triggers, small buttons, stick clicks, D-pad, sticks
    const QList<R> order = { R::A, R::B, R::X, R::Y, R::LB, R::RB, R::LeftTrigger,
                             R::RightTrigger, R::Back, R::Start, R::LS, R::RS, R::DpadUp,
                             R::DpadDown, R::DpadLeft, R::DpadRight, R::LeftY, R::LeftX,
                             R::RightY, R::RightX };
    for (int i = 0; i < order.size(); ++i)
        QCOMPARE(c.step(i).role, order.at(i));
    QCOMPARE(c.step(8).name, QString("View"));
    QCOMPARE(c.step(16).name, QString("Left stick up"));
    QCOMPARE(c.current(), 0);
    QVERIFY2(c.prompt().startsWith("Press A"), qPrintable(c.prompt()));
}

void MappingCheck_Test::aMatchingRoleWalksOn()
{
    MappingCheck c;
    QCOMPARE(c.feed(R::A, 255), MappingCheck::Matched);
    QCOMPARE(c.state(0), MappingCheck::Passed);
    QCOMPARE(c.current(), 1);
    QCOMPARE(c.passedCount(), 1);
    QVERIFY(c.line(0).startsWith(QString::fromUtf8("\342\234\223")));
}

void MappingCheck_Test::aDifferentRoleIsAMismatch()
{
    MappingCheck c;
    press(c, R::A);
    QCOMPARE(c.feed(R::X, 255), MappingCheck::Mismatch);   // B asked, X read
    QCOMPARE(c.state(1), MappingCheck::Wrong);
    QCOMPARE(c.current(), 1);                              // stays on it
    QCOMPARE(c.mismatchText(1), QString("Expected B, read X"));
    QCOMPARE(c.line(1), QString::fromUtf8("\342\234\227 B (read X)"));
    // Another stray press while it is marked wrong does not pile up.
    c.feed(R::X, 0);
    QCOMPARE(c.feed(R::Y, 255), MappingCheck::Ignored);
    QCOMPARE(c.readAs(1), QString("X"));
}

void MappingCheck_Test::retryAsksAgain()
{
    MappingCheck c;
    press(c, R::A);
    c.feed(R::X, 255);
    c.feed(R::X, 0);
    c.retry();
    QCOMPARE(c.state(1), MappingCheck::Pending);
    QCOMPARE(c.feed(R::B, 255), MappingCheck::Matched);
    QCOMPARE(c.state(1), MappingCheck::Passed);
    QCOMPARE(c.wrongCount(), 0);
}

void MappingCheck_Test::skipLeavesTheControl()
{
    MappingCheck c;
    c.skip();                       // A: not on this pad
    QCOMPARE(c.state(0), MappingCheck::Skipped);
    QCOMPARE(c.current(), 1);
    press(c, R::Y);                 // B asked: a mismatch, then skipped stays wrong
    QCOMPARE(c.state(1), MappingCheck::Wrong);
    c.feed(R::Y, 0);
    c.skip();
    QCOMPARE(c.state(1), MappingCheck::Wrong);
    QCOMPARE(c.current(), 2);
    c.stop();
    QVERIFY(c.finished());
    QCOMPARE(c.state(19), MappingCheck::Skipped);
    QCOMPARE(c.skippedCount(), 19);
}

void MappingCheck_Test::releasesAndChatterAreIgnored()
{
    MappingCheck c;
    QCOMPARE(c.feed(R::B, 0), MappingCheck::Ignored);          // a release
    QCOMPARE(c.feed(R::TouchX, 200), MappingCheck::Ignored);   // a finger on the pad
    QCOMPARE(c.feed(R::None, 255), MappingCheck::Ignored);
    QCOMPARE(c.feed(R::LeftX, 130), MappingCheck::Ignored);    // a stick at rest
    QCOMPARE(c.current(), 0);
    // A held control does not count again for the next one.
    c.feed(R::A, 255);
    QCOMPARE(c.feed(R::A, 255), MappingCheck::Ignored);
    QCOMPARE(c.state(1), MappingCheck::Pending);
}

void MappingCheck_Test::triggersNeedAFullPullAndSticksAPush()
{
    MappingCheck c;
    for (R r : { R::A, R::B, R::X, R::Y, R::LB, R::RB })
        press(c, r);
    QCOMPARE(c.step(c.current()).role, R::LeftTrigger);
    QCOMPARE(c.feed(R::LeftTrigger, 120), MappingCheck::Ignored);   // half way
    QCOMPARE(c.feed(R::LeftTrigger, 255), MappingCheck::Matched);
    // The trigger still down (reads repeat near full): not a mismatch for RT.
    QCOMPARE(c.feed(R::LeftTrigger, 250), MappingCheck::Ignored);
    c.feed(R::LeftTrigger, 0);
    QCOMPARE(c.feed(R::RightTrigger, 255), MappingCheck::Matched);
    for (R r : { R::Back, R::Start, R::LS, R::RS, R::DpadUp, R::DpadDown, R::DpadLeft, R::DpadRight })
        press(c, r);
    QCOMPARE(c.step(c.current()).role, R::LeftY);
    QCOMPARE(c.feed(R::LeftY, 128 + 20), MappingCheck::Ignored);
    QCOMPARE(c.feed(R::LeftY, 0), MappingCheck::Matched);   // forward is low or high: either
}

void MappingCheck_Test::theSummaryNamesEveryMismatch()
{
    MappingCheck c;
    for (int i = 0; i < c.count(); ++i)
    {
        const R want = c.step(i).role;
        if (i == 1)
            press(c, R::X);        // B read as X
        else if (i == 3)
            press(c, R::A);        // Y read as A
        else if (want == R::LeftTrigger)
            c.feed(want, 255);
        else if (want == R::LeftY || want == R::LeftX || want == R::RightY || want == R::RightX)
            c.feed(want, 255);
        else
            press(c, want);
        if (c.state(i) == MappingCheck::Wrong)
        {
            // A is held from before for i == 3: let go, then skip on.
            c.feed(R::A, 0);
            c.feed(R::X, 0);
            c.skip();
        }
        // Release so the next press counts.
        c.feed(want, 0);
    }
    QVERIFY(c.finished());
    QCOMPARE(c.passedCount(), 18);
    QCOMPARE(c.wrongCount(), 2);
    QCOMPARE(c.summary(), QString("18 of 20 match; 2 wrong: B read as X, Y read as A"));
    QVERIFY(c.report().contains("Expected B, read X"));
    QVERIFY(c.report().endsWith(c.summary()));
    // Skips are said, and not counted against the pad.
    MappingCheck d;
    d.skip();
    d.stop();
    QCOMPARE(d.summary(), QString("0 of 0 match; 20 skipped"));
}

void MappingCheck_Test::playStationNames()
{
    MappingCheck c(PadNames::DualSense);
    QCOMPARE(c.count(), 21);                       // the touchpad click
    QCOMPARE(c.step(0).name, QString::fromUtf8("Cross (\342\234\225)"));
    QCOMPARE(c.step(1).name, QString::fromUtf8("Circle (\342\227\213)"));
    QCOMPARE(c.step(2).name, QString::fromUtf8("Square (\342\226\241)"));
    QCOMPARE(c.step(3).name, QString::fromUtf8("Triangle (\342\226\263)"));
    QCOMPARE(c.step(4).name, QString("L1"));
    QCOMPARE(c.step(6).name, QString("L2"));
    QCOMPARE(c.step(8).name, QString("Create"));
    QCOMPARE(c.step(9).name, QString("Options"));
    QCOMPARE(c.step(10).role, R::TouchClick);
    QCOMPARE(c.step(11).name, QString("L3"));
    QVERIFY(!c.step(0).hint.contains("green"));
    MappingCheck ds4(PadNames::DualShock4);
    QCOMPARE(ds4.step(8).name, QString("Share"));
    // The wrong-name line uses the pad's words, too.
    c.feed(R::B, 255);
    QCOMPARE(c.mismatchText(0), QString::fromUtf8("Expected Cross (\342\234\225), read Circle (\342\227\213)"));
    // The Xbox pad has no touchpad step.
    QCOMPARE(MappingCheck(PadNames::Xbox).count(), 20);
}

QTEST_MAIN(MappingCheck_Test)

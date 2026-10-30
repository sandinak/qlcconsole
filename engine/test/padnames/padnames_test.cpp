/*
  Q Light Controller Plus
  padnames_test.cpp

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

#include "padnames_test.h"
#include "padnames.h"
#include "touchpadgesture.h"
#include "gamepadrole.h"

using R = GamepadRole::Role;
using F = PadNames::Family;

void PadNames_Test::names_data()
{
    QTest::addColumn<int>("role");
    QTest::addColumn<QString>("xbox");
    QTest::addColumn<QString>("ds4");
    QTest::addColumn<QString>("dualSense");
    QTest::addColumn<QString>("psGlyph");

    const QString cross = QString::fromUtf8("\342\234\225");
    const QString circle = QString::fromUtf8("\342\227\213");
    const QString square = QString::fromUtf8("\342\226\241");
    const QString triangle = QString::fromUtf8("\342\226\263");
    QTest::newRow("A") << int(R::A) << "A" << "Cross" << "Cross" << cross;
    QTest::newRow("B") << int(R::B) << "B" << "Circle" << "Circle" << circle;
    QTest::newRow("X") << int(R::X) << "X" << "Square" << "Square" << square;
    QTest::newRow("Y") << int(R::Y) << "Y" << "Triangle" << "Triangle" << triangle;
    QTest::newRow("LB") << int(R::LB) << "LB" << "L1" << "L1" << "L1";
    QTest::newRow("RB") << int(R::RB) << "RB" << "R1" << "R1" << "R1";
    QTest::newRow("LT") << int(R::LeftTrigger) << "LT" << "L2" << "L2" << "L2";
    QTest::newRow("RT") << int(R::RightTrigger) << "RT" << "R2" << "R2" << "R2";
    QTest::newRow("LS") << int(R::LS) << "LS" << "L3" << "L3" << "L3";
    QTest::newRow("RS") << int(R::RS) << "RS" << "R3" << "R3" << "R3";
    // Nothing written on these: no glyph.
    QTest::newRow("Back") << int(R::Back) << "View" << "Share" << "Create" << "";
    QTest::newRow("Start") << int(R::Start) << "Menu" << "Options" << "Options" << "";
    QTest::newRow("Guide") << int(R::Guide) << "Xbox button" << "PS button" << "PS button" << "";
    QTest::newRow("Touchpad") << int(R::TouchClick) << "Touchpad" << "Touchpad" << "Touchpad" << "";
    // The same on every pad.
    QTest::newRow("D-pad up") << int(R::DpadUp) << "D-pad up" << "D-pad up" << "D-pad up" << "D-pad up";
    QTest::newRow("Left stick") << int(R::LeftX) << "Left stick" << "Left stick" << "Left stick" << "Left stick";
    QTest::newRow("None") << int(R::None) << "" << "" << "" << "";
}

void PadNames_Test::names()
{
    QFETCH(int, role);
    QFETCH(QString, xbox);
    QFETCH(QString, ds4);
    QFETCH(QString, dualSense);
    QFETCH(QString, psGlyph);
    const R r = R(role);
    QCOMPARE(PadNames::name(r, F::Xbox), xbox);
    QCOMPARE(PadNames::name(r, F::DualShock4), ds4);
    QCOMPARE(PadNames::name(r, F::DualSense), dualSense);
    QCOMPARE(PadNames::name(r, F::DualShock4, PadNames::Glyph), psGlyph);
    QCOMPARE(PadNames::name(r, F::DualSense, PadNames::Glyph), psGlyph);
    // An Xbox pad's glyphs are its letters (nothing on View, Menu, Guide).
    if (r == R::Back || r == R::Start || r == R::Guide || r == R::TouchClick)
        QVERIFY(PadNames::name(r, F::Xbox, PadNames::Glyph).isEmpty());
    else
        QCOMPARE(PadNames::name(r, F::Xbox, PadNames::Glyph), xbox);
}

void PadNames_Test::expandsTokens()
{
    const QString line = QStringLiteral("{A} keeps it, {B} puts it back.");
    QCOMPARE(PadNames::expand(line, F::Xbox), QString("A keeps it, B puts it back."));
    QCOMPARE(PadNames::expand(line, F::DualShock4), QString("Cross keeps it, Circle puts it back."));
    QCOMPARE(PadNames::expand(line, F::DualShock4, PadNames::Glyph),
             QString::fromUtf8("\342\234\225 keeps it, \342\227\213 puts it back."));
    // Every token.
    QCOMPARE(PadNames::expand(QStringLiteral("{X}{Y}{LB}{RB}{LT}{RT}{LS}{RS}"), F::DualSense),
             QString("SquareTriangleL1R1L2R2L3R3"));
    QCOMPARE(PadNames::expand(QStringLiteral("{View} + {Menu}, {Guide}, {Touchpad}"), F::DualSense),
             QString("Create + Options, PS button, Touchpad"));
    QCOMPARE(PadNames::expand(QStringLiteral("{View} + {Menu}, {Guide}"), F::Xbox),
             QString("View + Menu, Xbox button"));
    // A glyph with nothing on the button falls back to the name.
    QCOMPARE(PadNames::expand(QStringLiteral("{View}"), F::DualShock4, PadNames::Glyph), QString("Share"));
    // Not ours: left as it is, and %1 placeholders survive for arg().
    QCOMPARE(PadNames::expand(QStringLiteral("{Q} {a} {} { A } %1 {"), F::DualShock4),
             QString("{Q} {a} {} { A } %1 {"));
    QCOMPARE(PadNames::expand(QStringLiteral("%1 moved: {A} keeps it."), F::DualShock4).arg("DSC"),
             QString("DSC moved: Cross keeps it."));
    QCOMPARE(PadNames::expand(QStringLiteral("no tokens"), F::DualShock4), QString("no tokens"));
    QCOMPARE(PadNames::expand(QString(), F::DualShock4), QString());
}

void PadNames_Test::familyFromKindOrProfile()
{
    QCOMPARE(PadNames::familyFrom("dualshock4"), F::DualShock4);
    QCOMPARE(PadNames::familyFrom("DualSense"), F::DualSense);
    QCOMPARE(PadNames::familyFrom("xbox"), F::Xbox);
    QCOMPARE(PadNames::familyFrom(""), F::Xbox);
    QCOMPARE(PadNames::familyFrom("joystick"), F::Xbox);
    for (F f : { F::Xbox, F::DualShock4, F::DualSense })
        QCOMPARE(PadNames::familyFrom(PadNames::kindKey(f)), f);
    QVERIFY(PadNames::isPlayStation(F::DualShock4));
    QVERIFY(PadNames::isPlayStation(F::DualSense));
    QVERIFY(!PadNames::isPlayStation(F::Xbox));

    // No kind from the plugin: the profile, the device, or what A is called.
    const QMap<int, QString> none;
    QMap<int, QString> cross;
    cross.insert(int(R::A), QStringLiteral("Cross"));
    QCOMPARE(PadNames::guess("Sony PlayStation Controller", "Wireless Controller", none), F::DualShock4);
    QCOMPARE(PadNames::guess("Sony PlayStation Controller", "DualSense Wireless Controller", none),
             F::DualSense);
    QCOMPARE(PadNames::guess("", "DUALSHOCK 4 Wireless Controller", none), F::DualShock4);
    QCOMPARE(PadNames::guess("Some pad", "Pad", cross), F::DualShock4);
    QCOMPARE(PadNames::guess("Microsoft Xbox Controller", "Xbox Wireless Controller", none), F::Xbox);
    QCOMPARE(PadNames::guess("", "", none), F::Xbox);
}

void PadNames_Test::faceColours()
{
    // PlayStation: Cross blue, Circle red, Square pink, Triangle green.
    QCOMPARE(PadNames::faceColour(R::A, F::DualShock4), QColor("#7fa6d9"));
    QCOMPARE(PadNames::faceColour(R::B, F::DualSense), QColor("#e0566b"));
    QCOMPARE(PadNames::faceColour(R::X, F::DualShock4), QColor("#d891bf"));
    QCOMPARE(PadNames::faceColour(R::Y, F::DualShock4), QColor("#3fb39d"));
    // Xbox: A green, B red, X blue, Y yellow.
    QCOMPARE(PadNames::faceColour(R::A, F::Xbox), QColor("#3a9a4a"));
    QCOMPARE(PadNames::faceColour(R::Y, F::Xbox), QColor("#d0a01c"));
    QVERIFY(!PadNames::faceColour(R::LB, F::DualShock4).isValid());
}

/*****************************************************************************
 * The touchpad
 *****************************************************************************/

namespace
{
/* A finger's reading as the controller sends it: bytes 0..255. */
void finger(TouchpadGesture &g, int f, int x, int y, bool down)
{
    g.setValue(f == 0 ? R::TouchX : R::Touch2X, x / 255.0f);
    g.setValue(f == 0 ? R::TouchY : R::Touch2Y, y / 255.0f);
    g.setValue(f == 0 ? R::TouchDown : R::Touch2Down, down ? 1.0f : 0.0f);
}

const float kStep = 1.0f / 255.0f;
}

void PadNames_Test::aDragMovesByHowFar()
{
    TouchpadGesture g;
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);   // nothing on it
    finger(g, 0, 100, 100, true);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);   // landed
    QCOMPARE(g.fingers(), 1);
    finger(g, 0, 140, 90, true);
    TouchpadGesture::Motion m = g.take();
    QCOMPARE(m.kind, TouchpadGesture::Drag);
    QVERIFY(m.started);
    QVERIFY(qAbs(m.dx - 40 * kStep) < 1e-5f);
    QVERIFY(qAbs(m.dy + 10 * kStep) < 1e-5f);
    // Held still: nothing. Then on: by that much again.
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    finger(g, 0, 150, 90, true);
    m = g.take();
    QCOMPARE(m.kind, TouchpadGesture::Drag);
    QVERIFY(!m.started);
    QVERIFY(qAbs(m.dx - 10 * kStep) < 1e-5f);
    QCOMPARE(m.dy, 0.0f);
    // Over a run of frames the moves add up to the whole drag.
    float sum = 0.0f;
    for (int x = 152; x <= 250; x += 2)
    {
        finger(g, 0, x, 90, true);
        sum += g.take().dx;
    }
    QVERIFY(qAbs(sum - 100 * kStep) < 1e-4f);
}

void PadNames_Test::aLiftOrLandingNeverJumps()
{
    TouchpadGesture g;
    finger(g, 0, 200, 200, true);
    g.take();
    finger(g, 0, 220, 200, true);
    QCOMPARE(g.take().kind, TouchpadGesture::Drag);
    // Lifted: the plugin keeps the place; even if it read the middle, no move.
    finger(g, 0, 128, 128, false);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    QCOMPARE(g.fingers(), 0);
    // Landing far away: from there, no jump.
    finger(g, 0, 10, 10, true);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    finger(g, 0, 10, 10, true);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    // A touch in the very middle is a touch like any other.
    finger(g, 0, 128, 128, false);
    g.take();
    finger(g, 0, 128, 128, true);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    QCOMPARE(g.fingers(), 1);
    finger(g, 0, 128, 118, true);
    const TouchpadGesture::Motion m = g.take();
    QCOMPARE(m.kind, TouchpadGesture::Drag);
    QVERIFY(qAbs(m.dy + 10 * kStep) < 1e-5f);
}

void PadNames_Test::aSecondFingerIsATwoFingerDrag()
{
    TouchpadGesture g;
    finger(g, 0, 100, 150, true);
    g.take();
    // The second lands as the first moves: neither counts.
    finger(g, 0, 130, 150, true);
    finger(g, 1, 160, 150, true);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    QCOMPARE(g.fingers(), 2);
    // Both up the pad: a two-finger drag, by their average.
    finger(g, 0, 130, 130, true);
    finger(g, 1, 160, 120, true);
    TouchpadGesture::Motion m = g.take();
    QCOMPARE(m.kind, TouchpadGesture::TwoFingerDrag);
    QVERIFY(m.started);
    QVERIFY(qAbs(m.dy + 25 * kStep) < 1e-5f);
    QCOMPARE(m.dx, 0.0f);
    // One lifts: nothing that frame, then the other drags on its own.
    finger(g, 1, 160, 100, false);
    finger(g, 0, 140, 100, true);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    finger(g, 0, 150, 100, true);
    m = g.take();
    QCOMPARE(m.kind, TouchpadGesture::Drag);
    QVERIFY(qAbs(m.dx - 10 * kStep) < 1e-5f);
}

void PadNames_Test::aClickIsNotADrag()
{
    TouchpadGesture g;
    finger(g, 0, 100, 100, true);
    g.take();
    finger(g, 0, 120, 100, true);
    QCOMPARE(g.take().kind, TouchpadGesture::Drag);
    // A firm press moves the finger: the click, no drag.
    g.setClick(true);
    finger(g, 0, 126, 104, true);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    finger(g, 0, 128, 106, true);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    // Pressed and let go between two frames: still a click, no drag.
    g.setClick(false);
    finger(g, 0, 132, 106, true);
    g.setClick(true);
    g.setClick(false);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    // After it, from where the finger is.
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    finger(g, 0, 142, 106, true);
    const TouchpadGesture::Motion m = g.take();
    QCOMPARE(m.kind, TouchpadGesture::Drag);
    QVERIFY(qAbs(m.dx - 10 * kStep) < 1e-5f);
    QCOMPARE(m.dy, 0.0f);
}

void PadNames_Test::slopAndJitter()
{
    TouchpadGesture g;
    finger(g, 0, 100, 100, true);
    g.take();
    // Under slop(): nothing yet, but not lost.
    finger(g, 0, 101, 100, true);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    finger(g, 0, 102, 100, true);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    finger(g, 0, 103, 100, true);
    TouchpadGesture::Motion m = g.take();
    QCOMPARE(m.kind, TouchpadGesture::Drag);
    QVERIFY(qAbs(m.dx - 3 * kStep) < 1e-5f);
    // A resting finger's one-step jitter: nothing, however long.
    for (int i = 0; i < 40; ++i)
    {
        finger(g, 0, i % 2 ? 104 : 103, i % 3 ? 100 : 101, true);
        m = g.take();
        QCOMPARE(m.kind, TouchpadGesture::NoMotion);
    }
    // A slow steady drag still gets there.
    float sum = 0.0f;
    for (int x = 104; x <= 120; ++x)
    {
        finger(g, 0, x, 100, true);
        sum += g.take().dx;
    }
    QVERIFY2(sum >= 15 * kStep, qPrintable(QString::number(sum / kStep)));
    g.reset();
    QCOMPARE(g.fingers(), 0);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
}

void PadNames_Test::anyOrderInAFrame()
{
    // Landing: the touch before its place, in one frame.
    TouchpadGesture g;
    g.setValue(R::TouchDown, 1.0f);
    g.setValue(R::TouchY, 200 / 255.0f);
    g.setValue(R::TouchX, 30 / 255.0f);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
    // Moving: Y then X.
    g.setValue(R::TouchY, 190 / 255.0f);
    g.setValue(R::TouchX, 50 / 255.0f);
    const TouchpadGesture::Motion m = g.take();
    QCOMPARE(m.kind, TouchpadGesture::Drag);
    QVERIFY(qAbs(m.dx - 20 * kStep) < 1e-5f);
    QVERIFY(qAbs(m.dy + 10 * kStep) < 1e-5f);
    // Something else entirely: ignored.
    g.setValue(R::A, 1.0f);
    g.setValue(R::LeftX, 0.0f);
    QCOMPARE(g.take().kind, TouchpadGesture::NoMotion);
}

void PadNames_Test::touchRoles()
{
    const R touch[] = { R::TouchX, R::TouchY, R::TouchDown, R::Touch2X, R::Touch2Y, R::Touch2Down,
                        R::TouchClick };
    for (R r : touch)
    {
        QCOMPARE(GamepadRole::fromProfile(GamepadRole::key(r), QString()), r);
        QVERIFY(!GamepadRole::isAxis(r));
        QCOMPARE(GamepadRole::isTouch(r), r != R::TouchClick);
    }
    // The profile's name alone finds the click (a profile before roles).
    QCOMPARE(GamepadRole::fromProfile(QString(), "Touchpad"), R::TouchClick);
    QVERIFY(!GamepadRole::isTouch(R::LeftX));
}

QTEST_GUILESS_MAIN(PadNames_Test)

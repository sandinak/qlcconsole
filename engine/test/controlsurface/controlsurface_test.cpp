/*
  Q Light Controller Plus - Unit test
  controlsurface_test.cpp

  Exercises the device-agnostic control-surface engine: role→state→brightness LED
  feedback, input routing to roles, and that two devices share one role vocabulary.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>
#include <QSignalSpy>
#include <QHash>

#include "controlsurface_test.h"
#include "controlsurfaceengine.h"

using namespace ControlSurface;

// Build a tiny board: a Page button, two Select strips, and a Fader (no LED).
static ControlSurfaceEngine::Device makeBoard(const QString &id,
                                              QHash<int, int> *ledSpy)
{
    ControlSurfaceEngine::Device dev;
    dev.id = id;
    dev.maxBrightness = 15;

    Control page;  page.kind = Kind::Button;  page.name = "Groups"; page.ledId = 100;
    Control s1;    s1.kind   = Kind::Button;  s1.name   = "1";      s1.ledId   = 1;
    Control s2;    s2.kind   = Kind::Button;  s2.name   = "2";      s2.ledId   = 2;
    Control fader; fader.kind = Kind::Fader;  fader.name = "Master"; fader.ledId = -1;
    dev.controls << page << s1 << s2 << fader;

    dev.roleOf.insert(0, Role(RoleType::Page,   0));   // Groups page
    dev.roleOf.insert(1, Role(RoleType::Select, 1));   // strip 1
    dev.roleOf.insert(2, Role(RoleType::Select, 2));   // strip 2
    dev.roleOf.insert(3, Role(RoleType::Level, -1));   // master fader (no LED)

    // Record the last brightness pushed to each ledId.
    dev.ledSink = [ledSpy](const Control &c, int level) {
        if (ledSpy) ledSpy->insert(c.ledId, level);
    };
    return dev;
}

// State provider: Groups page active, strip 1 selected, strip 2 idle/valid.
static State provider(const Role &r)
{
    if (r == Role(RoleType::Page,   0)) return State::Active;
    if (r == Role(RoleType::Select, 1)) return State::Selected;
    if (r == Role(RoleType::Select, 2)) return State::Valid;
    return State::Empty;
}

void ControlSurface_Test::ledStatesReflectContext()
{
    ControlSurfaceEngine eng;
    QHash<int, int> leds;
    eng.setStateProvider(provider);
    eng.registerDevice(makeBoard("pmj", &leds));   // registering pushes LEDs
    eng.refresh();

    QCOMPARE(leds.value(100), 15);   // Active page  → full
    QCOMPARE(leds.value(1),   10);   // Selected     → (15*2)/3 = 10
    QCOMPARE(leds.value(2),   3);    // Valid        → 15/4 = 3
}

void ControlSurface_Test::unboundAndFaderStayDark()
{
    ControlSurfaceEngine eng;
    QHash<int, int> leds;
    // No provider set → every LED-bearing control reads Empty.
    eng.registerDevice(makeBoard("pmj", &leds));
    eng.refresh();

    QCOMPARE(leds.value(100), 0);
    QCOMPARE(leds.value(1),   0);
    QVERIFY(!leds.contains(-1));   // the fader has no LED → sink never called for it
}

void ControlSurface_Test::inputRoutesToRole()
{
    ControlSurfaceEngine eng;
    eng.registerDevice(makeBoard("pmj", nullptr));
    qRegisterMetaType<ControlSurface::Role>("ControlSurface::Role");
    QSignalSpy spy(&eng, &ControlSurfaceEngine::roleActivated);

    eng.inputReceived("pmj", 1, 127);   // strip 1 pressed
    QCOMPARE(spy.count(), 1);
    const Role r = spy.at(0).at(1).value<ControlSurface::Role>();
    QVERIFY(r == Role(RoleType::Select, 1));
    QCOMPARE(spy.at(0).at(2).toInt(), 127);

    // An unbound control index emits nothing.
    eng.inputReceived("pmj", 99, 127);
    QCOMPARE(spy.count(), 1);
}

void ControlSurface_Test::twoDevicesShareRoles()
{
    ControlSurfaceEngine eng;
    QHash<int, int> pmjLeds, apcLeds;
    eng.setStateProvider(provider);
    eng.registerDevice(makeBoard("pmj", &pmjLeds));
    eng.registerDevice(makeBoard("apc", &apcLeds));
    eng.refresh();

    // Same roles → same states → same brightness on both boards.
    QCOMPARE(pmjLeds.value(100), apcLeds.value(100));
    QCOMPARE(pmjLeds.value(1),   apcLeds.value(1));
    QVERIFY(pmjLeds.value(1) > 0);
}

QTEST_GUILESS_MAIN(ControlSurface_Test)

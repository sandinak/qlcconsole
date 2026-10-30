/*
  Q Light Controller Plus
  hidgamecontroller_test.mm

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

// Built with ARC (test/CMakeLists.txt).

#import <Foundation/Foundation.h>
#import <GameController/GameController.h>

#include <QtTest>

#include "hidgamecontroller_test.h"
#include "hidgamecontroller.h"

/* A controller as the framework would hand one over: a snapshot gamepad
   (GCController's own, so its elements are real) under a name and category
   of the test's choosing. GCController's own -init is not called: on some
   macOS versions (26.3) it hands back an object of another class, which
   knows none of this. Everything the plugin asks of a controller is
   answered here. */
@interface QLCTestPad : GCController
- (instancetype)initWithName:(NSString *)name category:(NSString *)category;
@property (nonatomic, readonly) GCController *snapshot;
@end

@implementation QLCTestPad
{
    GCController *m_snapshot;
    NSString *m_name;
    NSString *m_category;
    dispatch_queue_t m_queue;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-designated-initializers"
- (instancetype)initWithName:(NSString *)name category:(NSString *)category
{
    m_snapshot = [GCController controllerWithExtendedGamepad];
    m_name = [name copy];
    m_category = [category copy];
    return self;
}
#pragma clang diagnostic pop

- (GCController *)snapshot { return m_snapshot; }
- (GCExtendedGamepad *)extendedGamepad { return m_snapshot.extendedGamepad; }
- (NSString *)vendorName { return m_name; }
- (NSString *)productCategory { return m_category; }
- (dispatch_queue_t)handlerQueue { return m_queue; }
- (void)setHandlerQueue:(dispatch_queue_t)queue { m_queue = queue; }
- (BOOL)isEqual:(id)other { return self == other; }
- (NSUInteger)hash { return (NSUInteger)(__bridge void *)self; }
@end

namespace
{
int g_changed = 0;

/** A pad named @p name, as the framework's Xbox (or DualSense) entry. */
/* Kept for the whole run: a GCController that never went through its own
   -init is never deallocated by it either. */
NSMutableArray *g_pads = nil;

QLCTestPad *makePad(const QString &name, const char *category)
{
    QLCTestPad *pad = [[QLCTestPad alloc] initWithName:name.toNSString()
                                              category:[NSString stringWithUTF8String:category]];
    if (g_pads == nil)
        g_pads = [NSMutableArray array];
    [g_pads addObject:pad];
    return pad;
}

void post(NSNotificationName name, GCController *pad)
{
    [[NSNotificationCenter defaultCenter] postNotificationName:name object:pad];
}

/** Unique to this run, so a claim never matches a real pad on this Mac. */
QString testName(const char *what)
{
    return QStringLiteral("qlcconsole test %1 %2")
        .arg(QString::fromLatin1(what)).arg(QCoreApplication::applicationPid());
}
}

void HIDGameController_Test::initTestCase()
{
    if (!HIDGameController::available())
        QSKIP("The GameController framework is not used here (macOS before 11.3, "
              "or QLC_HID_NO_GAMECONTROLLER set)");
    HIDGameController::startMonitoring([]() { ++g_changed; });
}

void HIDGameController_Test::cleanupTestCase()
{
    HIDGameController::stopMonitoring();
}

void HIDGameController_Test::aPadComingAndGoingIsAnnounced()
{
    /* What the plugin rescans on (HIDOSXHotplug passes the same callback):
       a pad the framework connects or disconnects, which IOKit's hotplug
       may never mention. */
    QLCTestPad *pad = makePad(testName("announce"), "Xbox One");
    const int before = g_changed;
    post(GCControllerDidConnectNotification, pad);
    QTRY_COMPARE(g_changed, before + 1);
    post(GCControllerDidDisconnectNotification, pad);
    QTRY_COMPARE(g_changed, before + 2);
}

void HIDGameController_Test::aPadThatGoesReadsAsGoneAndItsWokenSelfIsClaimed()
{
    const QString name = testName("xbox");
    QLCTestPad *pad = makePad(name, "Xbox One");
    [pad.snapshot.extendedGamepad.buttonA setValue:1.0f];
    post(GCControllerDidConnectNotification, pad);
    QTRY_VERIFY(g_changed > 0);

    const int handle = HIDGameController::claim(name, HIDGameController::XboxPad);
    QVERIFY(handle > 0);
    QVERIFY2(HIDGameController::describe(handle).contains(name),
             qPrintable(HIDGameController::describe(handle)));
    GamepadLayout::PadState state;
    QVERIFY(HIDGameController::read(handle, &state, nullptr));
    QVERIFY(state.buttons[GamepadLayout::A]);
    // Claimed once: a second claim does not take the same pad.
    const int second = HIDGameController::claim(name, HIDGameController::XboxPad);
    if (second != 0)
    {
        QVERIFY(!HIDGameController::describe(second).contains(name));
        HIDGameController::release(second);
    }

    // Auto-off: the framework disconnects it. The reader sees it gone
    // (and goes back to IOKit until the device is rescanned away).
    const int changed = g_changed;
    post(GCControllerDidDisconnectNotification, pad);
    QTRY_COMPARE(g_changed, changed + 1);
    QVERIFY(!HIDGameController::read(handle, &state, nullptr));
    HIDGameController::release(handle);

    // Woken: a new controller object, the same name -- claimed again.
    QLCTestPad *woken = makePad(name, "Xbox One");
    post(GCControllerDidConnectNotification, woken);
    QTRY_COMPARE(g_changed, changed + 2);
    const int again = HIDGameController::claim(name, HIDGameController::XboxPad);
    QVERIFY(again > 0);
    QVERIFY(again != handle);
    QVERIFY(HIDGameController::describe(again).contains(name));
    QVERIFY(HIDGameController::read(again, &state, nullptr));
    QVERIFY(!state.buttons[GamepadLayout::A]);
    HIDGameController::release(again);
    post(GCControllerDidDisconnectNotification, woken);
}

void HIDGameController_Test::aPlayStationPadIsFollowedTheSameWay()
{
    const QString name = testName("dualsense");
    QLCTestPad *pad = makePad(name, "DualSense");
    post(GCControllerDidConnectNotification, pad);
    // Claims are per kind: an Xbox line never takes it.
    const int xbox = HIDGameController::claim(name, HIDGameController::XboxPad);
    if (xbox != 0)
    {
        QVERIFY(!HIDGameController::describe(xbox).contains(name));
        HIDGameController::release(xbox);
    }
    const int handle = HIDGameController::claim(name, HIDGameController::PlayStationPad);
    QVERIFY(handle > 0);
    GamepadLayout::PadState state;
    QVERIFY(HIDGameController::read(handle, &state, nullptr));
    /* Q170: no touchpad on this stand-in (a real GCDualShockGamepad has no
       public initialiser), so its fingers read as none; the touch itself
       is tested through GamepadLayout::readTouch() and TouchpadGesture. */
    QCOMPARE(int(state.touch[GamepadLayout::Touch1Down]), 0);
    QCOMPARE(int(state.touch[GamepadLayout::Touch2Down]), 0);
    post(GCControllerDidDisconnectNotification, pad);
    QTRY_VERIFY(!HIDGameController::read(handle, &state, nullptr));
    HIDGameController::release(handle);
}

QTEST_GUILESS_MAIN(HIDGameController_Test)

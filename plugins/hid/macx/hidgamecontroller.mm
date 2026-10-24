/*
  Q Light Controller Plus
  hidgamecontroller.mm

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

// Built with ARC (plugins/hid/CMakeLists.txt).

#import <Foundation/Foundation.h>
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>

#include <memory>
#include <mutex>
#include <map>

#include <QtGlobal>

#include "hidgamecontroller.h"

using namespace GamepadLayout;

namespace
{
/** What the framework's handler queue writes and a device thread reads. */
struct Shared
{
    std::mutex mutex;
    PadState state;
    QList<HIDGameController::Event> events;
    bool keepEvents = false;
    bool connected = true;
};

struct Claim
{
    GCController *controller;           // strong (ARC)
    dispatch_queue_t queue;             // strong (ARC)
    std::shared_ptr<Shared> shared;
    id haptics = nil;                   // CHHapticEngine, made on the first buzz (strong)
};

std::mutex g_mutex;                     // guards the three below
NSMutableArray<GCController *> *g_controllers = nil;
std::map<int, Claim> g_claims;
int g_nextHandle = 1;

id g_connectObserver = nil;
id g_disconnectObserver = nil;

const int kButtonChannel = AxisCount;   // buttons follow the axes

uchar stick(float v, bool flip)
{
    // -1..1 as the signed 16-bit reading it stands for; GameController has
    // up positive, the profile (as HID) up = 0.
    int raw = qRound(qBound(-1.0f, v, 1.0f) * 32767.0f);
    if (flip)
        raw = -raw;
    return stickToByte(raw);
}

uchar trigger(float v)
{
    return uchar(qRound(qBound(0.0f, v, 1.0f) * 255.0f));
}

bool pressed(GCControllerButtonInput *b)
{
    return b != nil && b.isPressed;
}

GCControllerButtonInput *shareButton(GCExtendedGamepad *g)
{
    if (@available(macOS 12.0, *))
    {
        if ([g isKindOfClass:[GCXboxGamepad class]])
            return ((GCXboxGamepad *)g).buttonShare;
    }
    return nil;
}

void fill(GCExtendedGamepad *g, PadState *s)
{
    s->axes[LeftX]  = stick(g.leftThumbstick.xAxis.value, false);
    s->axes[LeftY]  = stick(g.leftThumbstick.yAxis.value, true);
    s->axes[RightX] = stick(g.rightThumbstick.xAxis.value, false);
    s->axes[RightY] = stick(g.rightThumbstick.yAxis.value, true);
    s->axes[LeftTrigger]  = trigger(g.leftTrigger.value);
    s->axes[RightTrigger] = trigger(g.rightTrigger.value);

    s->buttons[A]  = pressed(g.buttonA);
    s->buttons[B]  = pressed(g.buttonB);
    s->buttons[X]  = pressed(g.buttonX);
    s->buttons[Y]  = pressed(g.buttonY);
    s->buttons[LB] = pressed(g.leftShoulder);
    s->buttons[RB] = pressed(g.rightShoulder);
    s->buttons[View] = pressed(g.buttonOptions);
    s->buttons[Menu] = pressed(g.buttonMenu);
    s->buttons[LS] = pressed(g.leftThumbstickButton);
    s->buttons[RS] = pressed(g.rightThumbstickButton);
    s->buttons[Guide] = pressed(g.buttonHome);
    s->buttons[DpadUp]    = pressed(g.dpad.up);
    s->buttons[DpadDown]  = pressed(g.dpad.down);
    s->buttons[DpadLeft]  = pressed(g.dpad.left);
    s->buttons[DpadRight] = pressed(g.dpad.right);
    s->buttons[Share] = pressed(shareButton(g));
}

/** What element @p e of @p g is, as the Controller test window shows it. */
void describeElement(GCExtendedGamepad *g, GCControllerElement *e,
                     QList<HIDGameController::Event> *out)
{
    auto add = [out](const char *name, float value, int channel) {
        out->append({ QString::fromLatin1(name), value, channel });
    };
    auto pad = [&](GCControllerDirectionPad *p, const char *x, const char *y, int cx, int cy) {
        add(x, p.xAxis.value, cx);
        add(y, p.yAxis.value, cy);
    };

    if (e == g.leftThumbstick)
        pad(g.leftThumbstick, "leftThumbstick x", "leftThumbstick y", LeftX, LeftY);
    else if (e == g.rightThumbstick)
        pad(g.rightThumbstick, "rightThumbstick x", "rightThumbstick y", RightX, RightY);
    else if (e == g.dpad)
    {
        add("dpad up", g.dpad.up.value, kButtonChannel + DpadUp);
        add("dpad down", g.dpad.down.value, kButtonChannel + DpadDown);
        add("dpad left", g.dpad.left.value, kButtonChannel + DpadLeft);
        add("dpad right", g.dpad.right.value, kButtonChannel + DpadRight);
    }
    else if (e == g.leftTrigger)   add("leftTrigger", g.leftTrigger.value, LeftTrigger);
    else if (e == g.rightTrigger)  add("rightTrigger", g.rightTrigger.value, RightTrigger);
    else if (e == g.buttonA)       add("buttonA", g.buttonA.value, kButtonChannel + A);
    else if (e == g.buttonB)       add("buttonB", g.buttonB.value, kButtonChannel + B);
    else if (e == g.buttonX)       add("buttonX", g.buttonX.value, kButtonChannel + X);
    else if (e == g.buttonY)       add("buttonY", g.buttonY.value, kButtonChannel + Y);
    else if (e == g.leftShoulder)  add("leftShoulder", g.leftShoulder.value, kButtonChannel + LB);
    else if (e == g.rightShoulder) add("rightShoulder", g.rightShoulder.value, kButtonChannel + RB);
    else if (e == g.buttonOptions) add("buttonOptions", g.buttonOptions.value, kButtonChannel + View);
    else if (e == g.buttonMenu)    add("buttonMenu", g.buttonMenu.value, kButtonChannel + Menu);
    else if (e == g.leftThumbstickButton)
        add("leftThumbstickButton", g.leftThumbstickButton.value, kButtonChannel + LS);
    else if (e == g.rightThumbstickButton)
        add("rightThumbstickButton", g.rightThumbstickButton.value, kButtonChannel + RS);
    else if (e == g.buttonHome)    add("buttonHome", g.buttonHome.value, kButtonChannel + Guide);
    else if (e != nil && e == shareButton(g))
        add("buttonShare", shareButton(g).value, kButtonChannel + Share);
    else if (e != nil)
    {
        // Something the layout has no place for (an Elite's paddles...).
        NSString *name = e.localizedName != nil ? e.localizedName : @"element";
        float value = 0.0f;
        if ([e isKindOfClass:[GCControllerButtonInput class]])
            value = ((GCControllerButtonInput *)e).value;
        out->append({ QString::fromNSString(name), value, -1 });
    }
}

bool isXbox(GCController *c)
{
    if (c.extendedGamepad == nil)
        return false;
    if ([c.extendedGamepad isKindOfClass:[GCXboxGamepad class]])
        return true;
    NSString *category = c.productCategory;
    NSString *vendor = c.vendorName;
    return (category != nil && [category rangeOfString:@"Xbox" options:NSCaseInsensitiveSearch].location != NSNotFound)
        || (vendor != nil && [vendor rangeOfString:@"Xbox" options:NSCaseInsensitiveSearch].location != NSNotFound);
}

bool isClaimed(GCController *c)
{
    for (const auto &it : g_claims)
        if (it.second.controller == c)
            return true;
    return false;
}
}

namespace HIDGameController
{

bool available()
{
    if (qEnvironmentVariableIsSet("QLC_HID_NO_GAMECONTROLLER"))
        return false;
    if (@available(macOS 11.3, *))
        return true;
    return false;
}

void startMonitoring(std::function<void()> changed)
{
    if (!available() || g_connectObserver != nil)
        return;

    // Without this, the framework stops delivering as soon as another app
    // is in front -- the controller went dead when Focus mode was not.
    if (@available(macOS 11.3, *))
        GCController.shouldMonitorBackgroundEvents = YES;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_controllers = [NSMutableArray arrayWithArray:GCController.controllers];
    }

    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    g_connectObserver = [nc addObserverForName:GCControllerDidConnectNotification
                                        object:nil
                                         queue:[NSOperationQueue mainQueue]
                                    usingBlock:^(NSNotification *note) {
        GCController *c = note.object;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (c != nil && ![g_controllers containsObject:c])
                [g_controllers addObject:c];
        }
        if (changed)
            changed();
    }];
    g_disconnectObserver = [nc addObserverForName:GCControllerDidDisconnectNotification
                                           object:nil
                                            queue:[NSOperationQueue mainQueue]
                                       usingBlock:^(NSNotification *note) {
        GCController *c = note.object;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            [g_controllers removeObject:c];
            for (auto &it : g_claims)
            {
                if (it.second.controller != c)
                    continue;
                std::lock_guard<std::mutex> l(it.second.shared->mutex);
                it.second.shared->connected = false;
            }
        }
        if (changed)
            changed();
    }];
}

void stopMonitoring()
{
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    if (g_connectObserver != nil)
        [nc removeObserver:g_connectObserver];
    if (g_disconnectObserver != nil)
        [nc removeObserver:g_disconnectObserver];
    g_connectObserver = nil;
    g_disconnectObserver = nil;
}

int claim(const QString &preferredName)
{
    if (!available())
        return 0;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_controllers == nil)
        return 0;

    GCController *chosen = nil;
    NSString *preferred = preferredName.toNSString();
    for (GCController *c in g_controllers)
    {
        if (!isXbox(c) || isClaimed(c))
            continue;
        if (chosen == nil)
            chosen = c;
        if (preferred.length > 0 && [c.vendorName isEqualToString:preferred])
        {
            chosen = c;
            break;
        }
    }
    if (chosen == nil)
        return 0;

    Claim claim;
    claim.controller = chosen;
    claim.shared = std::make_shared<Shared>();
    centre(&claim.shared->state);
    /* Its own queue, not the main one: the state keeps up while the main
       thread is busy (a modal dialog, a long load). */
    claim.queue = dispatch_queue_create("org.qlcconsole.hid.gamecontroller", DISPATCH_QUEUE_SERIAL);
    chosen.handlerQueue = claim.queue;

    GCExtendedGamepad *pad = chosen.extendedGamepad;
    fill(pad, &claim.shared->state);
    std::weak_ptr<Shared> weak = claim.shared;
    pad.valueChangedHandler = ^(GCExtendedGamepad *g, GCControllerElement *e) {
        std::shared_ptr<Shared> sp = weak.lock();
        if (!sp)
            return;
        std::lock_guard<std::mutex> l(sp->mutex);
        fill(g, &sp->state);
        if (sp->keepEvents && sp->events.size() < 512)
            describeElement(g, e, &sp->events);
    };

    const int handle = g_nextHandle++;
    g_claims[handle] = claim;
    return handle;
}

void release(int handle)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_claims.find(handle);
    if (it == g_claims.end())
        return;
    GCExtendedGamepad *pad = it->second.controller.extendedGamepad;
    if (pad != nil)
        pad.valueChangedHandler = nil;
    g_claims.erase(it);
}

bool read(int handle, PadState *state, QList<Event> *events)
{
    std::shared_ptr<Shared> sp;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_claims.find(handle);
        if (it == g_claims.end())
            return false;
        sp = it->second.shared;
    }
    std::lock_guard<std::mutex> l(sp->mutex);
    *state = sp->state;
    sp->keepEvents = events != nullptr;
    if (events != nullptr)
        events->swap(sp->events);
    sp->events.clear();
    return sp->connected;
}

bool setLight(int handle, float r, float g, float b)
{
    GCController *c = nil;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_claims.find(handle);
        if (it == g_claims.end())
            return false;
        c = it->second.controller;
    }
    if (@available(macOS 11.0, *))
    {
        GCDeviceLight *light = c.light;
        if (light == nil)
            return false;
        light.color = [[GCColor alloc] initWithRed:r green:g blue:b];
        return true;
    }
    return false;
}

bool rumble(int handle, int pulses, int lengthMs)
{
    if (pulses <= 0)
        return false;
    if (@available(macOS 11.0, *))
    {
        GCController *c = nil;
        CHHapticEngine *engine = nil;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_claims.find(handle);
            if (it == g_claims.end())
                return false;
            c = it->second.controller;
            engine = it->second.haptics;
        }
        if (engine == nil)
        {
            GCDeviceHaptics *h = c.haptics;
            if (h == nil)
                return false;
            engine = [h createEngineWithLocality:GCHapticsLocalityDefault];
            if (engine == nil)
                return false;
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_claims.find(handle);
            if (it != g_claims.end())
                it->second.haptics = engine;
        }
        NSError *error = nil;
        if (![engine startAndReturnError:&error])
            return false;
        /* Firm buzzes, each lengthMs long, 0.1 s apart: one short for a
           save, two for "but", one long for something important. */
        const double length = qBound(0.05, lengthMs / 1000.0, 2.0);
        NSMutableArray<CHHapticEvent *> *events = [NSMutableArray array];
        for (int i = 0; i < pulses; ++i)
        {
            CHHapticEventParameter *strong =
                [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity
                                                              value:1.0f];
            [events addObject:[[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
                                                            parameters:@[ strong ]
                                                          relativeTime:(length + 0.1) * i
                                                              duration:length]];
        }
        CHHapticPattern *pattern = [[CHHapticPattern alloc] initWithEvents:events parameters:@[] error:&error];
        if (pattern == nil)
            return false;
        id<CHHapticPatternPlayer> player = [engine createPlayerWithPattern:pattern error:&error];
        if (player == nil)
            return false;
        return [player startAtTime:CHHapticTimeImmediate error:&error];
    }
    return false;
}

QString describe(int handle)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_claims.find(handle);
    if (it == g_claims.end())
        return QString();
    GCController *c = it->second.controller;
    const QString vendor = c.vendorName != nil ? QString::fromNSString(c.vendorName)
                                               : QStringLiteral("Game controller");
    if (c.productCategory != nil)
        return QStringLiteral("%1 (%2)").arg(vendor, QString::fromNSString(c.productCategory));
    return vendor;
}

}

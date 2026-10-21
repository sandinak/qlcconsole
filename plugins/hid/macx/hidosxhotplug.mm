/*
  Q Light Controller Plus
  hidosxhotplug.mm

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

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>

#include "hidosxhotplug.h"
#include "hidgamecontroller.h"

struct HIDOSXHotplug::Private
{
    std::function<void()> changed;
    std::function<void()> woke;
    IOHIDManagerRef manager = nullptr;
    NSMutableArray *observers = nil;          // NSWorkspace's
    NSMutableArray *distributed = nil;        // the distributed centre's
};

namespace
{
void deviceEvent(void *context, IOReturn, void *, IOHIDDeviceRef)
{
    auto *d = static_cast<HIDOSXHotplug::Private *>(context);
    if (d->changed)
        d->changed();
}

CFDictionaryRef usageMatch(int usage)
{
    NSDictionary *dict = @{ @kIOHIDDeviceUsagePageKey: @(kHIDPage_GenericDesktop),
                            @kIOHIDDeviceUsageKey: @(usage) };
    return (CFDictionaryRef)CFBridgingRetain(dict);
}
}

HIDOSXHotplug::HIDOSXHotplug(std::function<void()> changed, std::function<void()> woke)
    : d(new Private)
{
    d->changed = changed;
    d->woke = woke;

    /* Game controllers only: matching every HID device would also open the
       keyboards, which is what macOS asks Input Monitoring permission for. */
    d->manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (d->manager != nullptr)
    {
        CFDictionaryRef joystick = usageMatch(kHIDUsage_GD_Joystick);
        CFDictionaryRef gamepad = usageMatch(kHIDUsage_GD_GamePad);
        CFDictionaryRef multiAxis = usageMatch(kHIDUsage_GD_MultiAxisController);
        const void *matches[] = { joystick, gamepad, multiAxis };
        CFArrayRef array = CFArrayCreate(kCFAllocatorDefault, matches, 3, &kCFTypeArrayCallBacks);
        IOHIDManagerSetDeviceMatchingMultiple(d->manager, array);
        CFRelease(array);
        CFRelease(joystick);
        CFRelease(gamepad);
        CFRelease(multiAxis);

        IOHIDManagerRegisterDeviceMatchingCallback(d->manager, deviceEvent, d);
        IOHIDManagerRegisterDeviceRemovalCallback(d->manager, deviceEvent, d);
        IOHIDManagerScheduleWithRunLoop(d->manager, CFRunLoopGetMain(), kCFRunLoopCommonModes);
        IOHIDManagerOpen(d->manager, kIOHIDOptionsTypeNone);
    }

    Private *p = d;
    auto onWake = ^(NSNotification *) {
        if (p->woke)
            p->woke();
    };
    d->observers = [NSMutableArray array];
    NSNotificationCenter *ws = [[NSWorkspace sharedWorkspace] notificationCenter];
    for (NSNotificationName name : { NSWorkspaceDidWakeNotification,
                                     NSWorkspaceScreensDidWakeNotification,
                                     NSWorkspaceSessionDidBecomeActiveNotification })
        [d->observers addObject:[ws addObserverForName:name object:nil
                                                 queue:[NSOperationQueue mainQueue]
                                            usingBlock:onWake]];
    d->distributed = [NSMutableArray array];
    [d->distributed addObject:[[NSDistributedNotificationCenter defaultCenter]
                                  addObserverForName:@"com.apple.screenIsUnlocked" object:nil
                                               queue:[NSOperationQueue mainQueue]
                                          usingBlock:onWake]];

    HIDGameController::startMonitoring(changed);
}

HIDOSXHotplug::~HIDOSXHotplug()
{
    HIDGameController::stopMonitoring();

    NSNotificationCenter *ws = [[NSWorkspace sharedWorkspace] notificationCenter];
    for (id o in d->observers)
        [ws removeObserver:o];
    for (id o in d->distributed)
        [[NSDistributedNotificationCenter defaultCenter] removeObserver:o];

    if (d->manager != nullptr)
    {
        IOHIDManagerRegisterDeviceMatchingCallback(d->manager, nullptr, nullptr);
        IOHIDManagerRegisterDeviceRemovalCallback(d->manager, nullptr, nullptr);
        IOHIDManagerUnscheduleFromRunLoop(d->manager, CFRunLoopGetMain(), kCFRunLoopCommonModes);
        IOHIDManagerClose(d->manager, kIOHIDOptionsTypeNone);
        CFRelease(d->manager);
    }
    delete d;
}

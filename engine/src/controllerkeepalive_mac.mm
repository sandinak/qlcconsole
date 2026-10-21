/*
  Q Light Controller Plus
  controllerkeepalive_mac.mm

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

// ControllerKeepAlive's macOS side. Built with ARC (engine/src/CMakeLists.txt).

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#include <IOKit/hidsystem/IOHIDLib.h>

#include <functional>
#include <QString>

#include "controllerkeepalive.h"

namespace KeepAlivePlatform
{

void *beginActivity(const QString &reason)
{
    /* UserInitiated: no App Nap (timers on time, full priority) and no idle
       system sleep. LatencyCritical: the 50 Hz joystick tick and the HID
       readers are not coalesced either. */
    id token = [[NSProcessInfo processInfo]
        beginActivityWithOptions:(NSActivityUserInitiated | NSActivityLatencyCritical)
                          reason:reason.toNSString()];
    return (__bridge_retained void *)token;
}

void endActivity(void *token)
{
    if (token == nullptr)
        return;
    id activity = (__bridge_transfer id)token;
    [[NSProcessInfo processInfo] endActivity:activity];
}

int inputMonitoring()
{
    if (@available(macOS 10.15, *))
    {
        switch (IOHIDCheckAccess(kIOHIDRequestTypeListenEvent))
        {
            case kIOHIDAccessTypeGranted: return ControllerKeepAlive::PermissionGranted;
            case kIOHIDAccessTypeDenied:  return ControllerKeepAlive::PermissionDenied;
            default:                      return ControllerKeepAlive::PermissionUnknown;
        }
    }
    return ControllerKeepAlive::PermissionNotNeeded;
}

void requestInputMonitoring()
{
    if (@available(macOS 10.15, *))
        IOHIDRequestAccess(kIOHIDRequestTypeListenEvent);
}

void *observe(std::function<void(int)> changed)
{
    NSMutableArray *observers = [NSMutableArray array];
    NSNotificationCenter *ws = [[NSWorkspace sharedWorkspace] notificationCenter];
    NSNotificationCenter *dist = [NSDistributedNotificationCenter defaultCenter];
    NSOperationQueue *main = [NSOperationQueue mainQueue];

    auto on = ^(NSNotificationCenter *centre, NSNotificationName name, int state) {
        id o = [centre addObserverForName:name object:nil queue:main
                               usingBlock:^(NSNotification *) { changed(state); }];
        [observers addObject:@[ centre, o ]];
    };
    on(ws, NSWorkspaceWillSleepNotification, ControllerKeepAlive::Asleep);
    on(ws, NSWorkspaceDidWakeNotification, ControllerKeepAlive::Waking);
    on(ws, NSWorkspaceSessionDidResignActiveNotification, ControllerKeepAlive::ScreenLocked);
    on(ws, NSWorkspaceSessionDidBecomeActiveNotification, -1);   // "unlocked"
    on(dist, @"com.apple.screenIsLocked", ControllerKeepAlive::ScreenLocked);
    on(dist, @"com.apple.screenIsUnlocked", -1);

    return (__bridge_retained void *)observers;
}

void unobserve(void *observer)
{
    if (observer == nullptr)
        return;
    NSArray *observers = (__bridge_transfer NSArray *)observer;
    for (NSArray *pair in observers)
        [(NSNotificationCenter *)pair[0] removeObserver:pair[1]];
}

}

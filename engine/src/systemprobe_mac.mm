/*
  Q Light Controller Plus - qlcconsole
  systemprobe_mac.mm

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


// SystemProbe's macOS side. Built with ARC (engine/src/CMakeLists.txt).

#import <Foundation/Foundation.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

#include <QString>

namespace SystemProbeMac
{

int lowPowerMode()
{
    if (@available(macOS 12.0, *))
        return [[NSProcessInfo processInfo] isLowPowerModeEnabled] ? 1 : 0;
    return -1;
}

int thermalState()
{
    switch ([[NSProcessInfo processInfo] thermalState])
    {
        case NSProcessInfoThermalStateNominal:  return 0;
        case NSProcessInfoThermalStateFair:     return 1;
        case NSProcessInfoThermalStateSerious:  return 2;
        case NSProcessInfoThermalStateCritical: return 3;
    }
    return -1;
}

int onBattery(int *percent)
{
    if (percent != nullptr)
        *percent = -1;
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    if (info == nullptr)
        return -1;
    int result = -1;
    CFStringRef source = IOPSGetProvidingPowerSourceType(info); /* not ours to release */
    if (source != nullptr)
        result = CFStringCompare(source, CFSTR(kIOPSBatteryPowerValue), 0) == kCFCompareEqualTo ? 1 : 0;
    CFArrayRef list = IOPSCopyPowerSourcesList(info);
    if (list != nullptr)
    {
        for (CFIndex i = 0; percent != nullptr && i < CFArrayGetCount(list); i++)
        {
            CFDictionaryRef d = IOPSGetPowerSourceDescription(info, CFArrayGetValueAtIndex(list, i));
            if (d == nullptr)
                continue;
            int now = 0, max = 0;
            CFNumberRef n = (CFNumberRef)CFDictionaryGetValue(d, CFSTR(kIOPSCurrentCapacityKey));
            CFNumberRef m = (CFNumberRef)CFDictionaryGetValue(d, CFSTR(kIOPSMaxCapacityKey));
            if (n != nullptr && m != nullptr
                && CFNumberGetValue(n, kCFNumberIntType, &now)
                && CFNumberGetValue(m, kCFNumberIntType, &max) && max > 0)
                *percent = now * 100 / max;
        }
        CFRelease(list);
    }
    CFRelease(info);
    return result;
}

void *beginActivity(const QString &reason, bool keepDisplayAwake)
{
    /* UserInitiated: no App Nap (timers on time, full priority) and no idle
       system sleep. LatencyCritical: timers -- the main thread's input
       handling among them -- are not coalesced. Same options as
       ControllerKeepAlive's. */
    NSActivityOptions options = NSActivityUserInitiated | NSActivityLatencyCritical;
    if (keepDisplayAwake)
        options |= NSActivityIdleDisplaySleepDisabled;
    id token = [[NSProcessInfo processInfo] beginActivityWithOptions:options
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

}

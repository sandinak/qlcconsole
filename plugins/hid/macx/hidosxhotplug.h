/*
  Q Light Controller Plus
  hidosxhotplug.h

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

#ifndef HIDOSXHOTPLUG_H
#define HIDOSXHOTPLUG_H

#include <functional>

/**
 * Tells the HID plugin when its game controllers may have changed, on a Mac.
 *
 * The app's HotPlugMonitor only sees USB devices. An Xbox pad on Bluetooth
 * that switched itself off (it does after 15 idle minutes) and was switched
 * back on, or that reconnected after the Mac slept, was never noticed: the
 * plugin kept reading the device that had gone, and the controller "stopped
 * working" until the app was restarted. This watches, on the main run loop:
 *  - IOKit game controllers (joystick, gamepad, multi-axis) arriving and
 *    leaving, whatever their transport;
 *  - the Mac and its screens waking, and the screen unlocking -- after
 *    which a pad that slept with the Mac reconnects as a new device;
 *  - the GameController framework's controllers connecting and
 *    disconnecting.
 * and calls @p changed (main thread) for each; the plugin rescans, debounced.
 */
class HIDOSXHotplug
{
public:
    explicit HIDOSXHotplug(std::function<void()> changed,
                           std::function<void()> woke);
    ~HIDOSXHotplug();

    struct Private;

private:
    Private *d;
};

#endif

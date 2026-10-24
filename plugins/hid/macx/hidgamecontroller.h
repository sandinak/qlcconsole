/*
  Q Light Controller Plus
  hidgamecontroller.h

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

#ifndef HIDGAMECONTROLLER_H
#define HIDGAMECONTROLLER_H

#include <functional>
#include <QString>
#include <QList>

#include "gamepadlayout.h"

/**
 * Apple's GameController framework, as a source for an Xbox pad's input.
 *
 * It names every element itself (buttonA, leftShoulder, dpad,
 * leftThumbstick...) for every Xbox model, firmware and connection -- USB,
 * Bluetooth, Bluetooth LE -- so nothing here depends on which report layout a
 * pad uses; and, asked to (shouldMonitorBackgroundEvents), it keeps
 * delivering while another app is in front, which it otherwise does not.
 * HIDOSXJoystick reads a pad through it when it can and falls back to its own
 * IOKit readers when it cannot (no such controller in the framework yet, or
 * QLC_HID_NO_GAMECONTROLLER set in the environment).
 *
 * A handle is a claim on one connected controller: two QLC+ input lines never
 * read the same pad.
 */
namespace HIDGameController
{
    /** The framework can be used here for background input (macOS 11.3+),
     *  and QLC_HID_NO_GAMECONTROLLER is not set. */
    bool available();

    /** Main thread, once: ask for events while in the background, and watch
     *  controllers come and go -- @p changed is called (main thread) when one
     *  does. */
    void startMonitoring(std::function<void()> changed);
    /** Main thread: stop watching. Claims stay valid. */
    void stopMonitoring();

    /** Claim a connected Xbox controller nobody has claimed; one named
     *  @p preferredName (the HID product string) first. A handle (> 0), or
     *  0 when there is none. Any thread. */
    int claim(const QString &preferredName);
    /** Let it go. Any thread. */
    void release(int handle);

    /** One input as the framework reported it. */
    struct Event
    {
        QString element;   ///< "buttonA", "leftThumbstick x"...
        float value;       ///< as reported: -1..1 for a stick, 0..1 otherwise
        int channel;       ///< layout channel (axes, then buttons), -1 for none
    };

    /** The controller's state in the layout, into @p state. With @p events,
     *  also what it reported since the last call (and from now on keep
     *  them); without, stop keeping them. False once it has disconnected. */
    bool read(int handle, GamepadLayout::PadState *state, QList<Event> *events);

    /** "Xbox Wireless Controller (Xbox One)". */
    QString describe(int handle);

    /** The pad's light (GCDeviceLight: a DualShock 4 / DualSense light bar)
     *  in r, g, b 0..1. False where the claimed pad has none -- an Xbox pad
     *  has no coloured light. Any thread. */
    bool setLight(int handle, float r, float g, float b);
    /** @p pulses short buzzes (GCDeviceHaptics, Core Haptics; macOS 11+).
     *  False where the pad has no haptics. Any thread. */
    bool rumble(int handle, int pulses);
}

#endif

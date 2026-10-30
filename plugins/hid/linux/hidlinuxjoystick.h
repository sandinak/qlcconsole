/*
  Q Light Controller Plus
  hidlinuxjoystick.h

  Copyright (c) Massimo Callegari

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

#ifndef HIDLINUXJOYSTICK_H
#define HIDLINUXJOYSTICK_H

#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/types.h>

#include <QVector>

#include "hidjsdevice.h"
#include "gamepadlayout.h"

class HIDLinuxJoystick final : public HIDJsDevice
{
    Q_OBJECT
public:
    HIDLinuxJoystick(HIDPlugin* parent, quint32 line, struct hid_device_info *info);

    /** @reimp */
    void init() override;

    /** @reimp */
    bool openInput() override;

    /** @reimp */
    bool readEvent() override;

protected:
    bool openDevice();

private:
    /** @reimp */
    void run() override;

    /** A pad the layout knows: its joystick controls by evdev code into
     *  the stock profiles' layout (GamepadLayout::mapLinuxButton()), so a
     *  DualSense, a DualShock 4 and an Xbox pad all read the same here as
     *  on a Mac. Empty tables: its own order, as before. */
    void readControlMap();
    void emitButton(int index, bool down);

    QVector<GamepadLayout::Mapping> m_buttonMap;    // by joystick button number
    QVector<GamepadLayout::Mapping> m_axisMap;      // by joystick axis number
    QVector<quint16> m_axisCodes;                   // evdev code by axis number
    bool m_dpad[4] = { false, false, false, false };
    bool m_inLayout = false;
};

#endif // HIDLINUXJOYSTICK_H

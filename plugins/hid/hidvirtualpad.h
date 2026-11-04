/*
  Q Light Controller Plus
  hidvirtualpad.h

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

#ifndef HIDVIRTUALPAD_H
#define HIDVIRTUALPAD_H

#include "hiddevice.h"

/**
 * A game controller that is not there: listed, patched, profiled and read
 * as a real one is (its vendor and product pick the HID profile; its values
 * leave through HIDDevice::valueChanged(), the signal a real pad's reads
 * leave through). Only the reading of the hardware is missing -- inject()
 * stands for it. Made only when QLC_HID_VIRTUAL_PAD is set, for the tests
 * that drive the whole input path from the plugin up (followspotcue_test).
 */
class HIDVirtualPad final : public HIDDevice
{
    Q_OBJECT

public:
    HIDVirtualPad(HIDPlugin *parent, quint32 line, const QString &name,
                  unsigned short vendorId, unsigned short productId);

    bool hasInput() const override { return true; }
    bool openInput() override;
    void closeInput() override;
    bool readEvent() override { return false; }

    unsigned short vendorId() const override { return m_vendorId; }
    unsigned short productId() const override { return m_productId; }
    /** The layout the Mac reads a pad into: six axes, then the buttons. */
    int axisCount() const override { return 6; }
    int buttonCount() const override { return 22; }
    QString connectionInfo() const override;

    /** A value as the pad's reading would give it. */
    void inject(quint32 channel, uchar value);

    /** "054c:09cc: DUALSHOCK 4 Wireless Controller (DE:4C:...)" pads,
     *  separated by ';', from QLC_HID_VIRTUAL_PAD. */
    static QList<HIDVirtualPad *> fromEnvironment(HIDPlugin *parent);

private:
    unsigned short m_vendorId;
    unsigned short m_productId;
    bool m_open = false;
};

#endif

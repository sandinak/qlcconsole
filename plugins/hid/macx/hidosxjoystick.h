/*
  Q Light Controller Plus
  hidosxjoystick.h

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


#ifndef HIDOSXJOYSTICK_H
#define HIDOSXJOYSTICK_H

#include "hidjsdevice.h"
#include "hidapi.h"

#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>

typedef struct
{
    IOHIDElementRef IOHIDRef;
    int value;
    int logicalMin;  // device-reported axis minimum (e.g. -32768 for Xbox)
    int logicalMax;  // device-reported axis maximum (e.g.  32767 for Xbox)
} HIDInfo;

class HIDOSXJoystick final : public HIDJsDevice
{
    Q_OBJECT
public:
    HIDOSXJoystick(HIDPlugin* parent, quint32 line, struct hid_device_info *info);
    ~HIDOSXJoystick() override;

    static bool isJoystick(unsigned short usage);

    /** @reimp */
    void init() override;

    /** @reimp */
    bool readEvent() override;

    /** @reimp — raw mode schedules the IOKit device on the thread run loop. */
    void run() override;

    /** Called from the IOHIDDevice input-report callback with raw report bytes. */
    void processRawReport(uint32_t reportID, const uint8_t *report, CFIndex len);

private:
    IOHIDManagerRef m_HIDManager;
    IOHIDDeviceRef m_IOKitDevice;

    QList<HIDInfo> m_axesValues;
    QList<HIDInfo> m_buttonsValues;

    // Vendor-element mode: used when Apple's driver (AppleUserHIDDevice) virtualises
    // the controller and exposes only vendor-page (0xff00) elements instead of standard
    // Generic Desktop axes/buttons.  We poll the element directly via IOHIDDeviceGetValue
    // and parse the raw Xbox input report bytes from IOHIDValueGetBytePtr.
    IOHIDElementRef m_vendorXboxElem = nullptr;  // page=0xff00, usage=0x20 (main state)
    bool            m_rawMode        = false;
    int             m_rawDumpCount   = 0;
    uint8_t         m_rawPrevBytes[32] = {};
    uint8_t         m_reportBuf[64]    = {};  // scratch buffer for input-report callback

    // hidapi fallback (less preferred — Apple kext consumes reports before hidapi sees them)
    hid_device *m_hidHandle = nullptr;
};

#endif // HIDOSXJOYSTICK_H

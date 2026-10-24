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

#include <QAtomicInt>
#include <QHash>

#include "hidjsdevice.h"
#include "hidapi.h"
#include "gamepadlayout.h"

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

/**
 * A joystick or gamepad on a Mac.
 *
 * An Xbox pad (Microsoft's vendor ID) is read into ONE layout whatever its
 * model and connection -- the one the Xbox profile names (GamepadLayout):
 * through Apple's GameController framework when it has the pad, else
 * through IOKit, by the report layout its model and firmware use (the USB
 * GIP report, or the Bluetooth HID elements, mapped by usage). Any other
 * device is read as before: its axes and buttons in the order it lists them,
 * polled.
 */
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

    /** @reimp */
    void run() override;

    /** @reimp */
    bool isStale() const override;
    /** @reimp */
    QString connectionInfo() const override;
    /** @reimp */
    QString inputProblem() const override;

    /** The pad's light bar (Q126): through the GameController framework when
     *  this pad is read that way; false otherwise (or no light). */
    bool setPadLight(float r, float g, float b);
    /** @p pulses buzzes, the same way; false where it cannot. */
    bool padRumble(int pulses, int lengthMs = 120);

    /** From the IOHIDDevice input-report callback (the USB GIP report). */
    void processRawReport(uint32_t reportID, const uint8_t *report, CFIndex len);
    /** From the IOHIDDevice input-value callback (the Bluetooth elements). */
    void processValue(IOHIDValueRef value);

private:
    /** An input element as the Bluetooth reader maps it. */
    struct Element
    {
        quint32 page;
        quint32 usage;
        int logicalMin;
        int logicalMax;
        GamepadLayout::Mapping mapping;
    };

    bool isXbox() const { return m_family != GamepadLayout::NotXbox; }
    void scheduleIOKit();
    void unscheduleIOKit();
    /** Read the claimed GameController pad once; false when it has gone. */
    bool pollGameController();
    /** Emit what changed in m_state since it was last sent. */
    void emitChanges();
    void emitRaw(const QString &element, int raw, int channel);
    int channelValue(int channel) const;

private:
    IOHIDManagerRef m_HIDManager = nullptr;
    IOHIDDeviceRef m_IOKitDevice = nullptr;    ///< retained
    quint64 m_registryID = 0;                  ///< for isStale()
    IOReturn m_openResult = kIOReturnSuccess;
    bool m_opened = false;

    QString m_productName;
    GamepadLayout::Transport m_transport = GamepadLayout::UnknownTransport;
    GamepadLayout::Family m_family = GamepadLayout::NotXbox;

    // Any other device: its elements in its own order, polled.
    QList<HIDInfo> m_axesValues;
    QList<HIDInfo> m_buttonsValues;

    // An Xbox pad: its state in the layout, and what was last sent.
    GamepadLayout::PadState m_state;
    GamepadLayout::PadState m_sent;
    bool m_sentValid = false;
    QHash<quint32, Element> m_elements;        ///< by element cookie
    uint8_t m_reportBuf[64] = {};              ///< for the input-report callback
    uint8_t m_prevReport[64] = {};             ///< for diagnostics
    int m_prevReportLength = 0;

    /** The GameController claim (HIDGameController), 0 for none. */
    QAtomicInt m_gcHandle { 0 };
};

#endif // HIDOSXJOYSTICK_H

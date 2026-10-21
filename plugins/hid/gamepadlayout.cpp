/*
  Q Light Controller Plus
  gamepadlayout.cpp

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

#include "gamepadlayout.h"

namespace GamepadLayout
{

namespace
{
// HID usage pages and usages used here (HID Usage Tables 1.4).
const quint32 kPageGenericDesktop = 0x01;
const quint32 kPageSimulation     = 0x02;
const quint32 kPageButton         = 0x09;
const quint32 kPageConsumer       = 0x0C;

const quint32 kGDX = 0x30, kGDY = 0x31, kGDZ = 0x32;
const quint32 kGDRx = 0x33, kGDRy = 0x34, kGDRz = 0x35;
const quint32 kGDHat = 0x39;
const quint32 kGDSystemMainMenu = 0x85;

const quint32 kSimAccelerator = 0xC4;
const quint32 kSimBrake       = 0xC5;

const quint32 kConsumerRecord = 0xB2;
const quint32 kConsumerACHome = 0x223;
const quint32 kConsumerACBack = 0x224;

struct ModelEntry
{
    quint16 productId;
    const char *name;
    Transport transport;   ///< what this ID means; UnknownTransport for USB-or-other
    Family family;         ///< NotXbox here = decide from the elements
};

/* Microsoft's product IDs (as SDL's and Linux's tables list them). Each
   Bluetooth firmware enumerates under its own ID, which is how the layout of
   one is told from another. */
const ModelEntry kModels[] =
{
    { 0x028E, "Xbox 360 Controller",                  Usb,         NotXbox },
    { 0x028F, "Xbox 360 Wireless Controller",         UnknownTransport, NotXbox },
    { 0x0719, "Xbox 360 Wireless Receiver",           Usb,         NotXbox },
    { 0x02D1, "Xbox One Controller",                  Usb,         XboxGip },
    { 0x02DD, "Xbox One Controller (2015)",           Usb,         XboxGip },
    { 0x02E3, "Xbox One Elite Controller",            Usb,         XboxGip },
    { 0x02EA, "Xbox One S Controller",                Usb,         XboxGip },
    { 0x0B00, "Xbox Elite Series 2 Controller",       Usb,         XboxGip },
    { 0x0B0A, "Xbox Adaptive Controller",             Usb,         XboxGip },
    { 0x0B12, "Xbox Series X|S Controller",           Usb,         XboxGip },
    { 0x02E0, "Xbox One S Controller (first Bluetooth firmware)", Bluetooth, XboxBtClassic },
    { 0x02FD, "Xbox One S Controller",                Bluetooth,   XboxBtModern },
    { 0x0B05, "Xbox Elite Series 2 Controller",       Bluetooth,   XboxBtModern },
    { 0x0B0C, "Xbox Adaptive Controller",             Bluetooth,   XboxBtModern },
    { 0x0B13, "Xbox Series X|S Controller",           BluetoothLE, XboxBtModern },
    { 0x0B20, "Xbox One S Controller",                BluetoothLE, XboxBtModern },
    { 0x0B21, "Xbox Adaptive Controller",             BluetoothLE, XboxBtModern },
    { 0x0B22, "Xbox Elite Series 2 Controller",       BluetoothLE, XboxBtModern },
};

const ModelEntry *findModel(quint16 vendorId, quint16 productId)
{
    if (vendorId != kMicrosoftVendorId)
        return nullptr;
    for (const ModelEntry &m : kModels)
        if (m.productId == productId)
            return &m;
    return nullptr;
}

Mapping axis(int index) { Mapping m; m.kind = AxisElement; m.index = index; return m; }
Mapping button(int index) { Mapping m; m.kind = ButtonElement; m.index = index; return m; }
Mapping hat() { Mapping m; m.kind = HatElement; return m; }
}

Transport transportFromString(const QString &name)
{
    const QString n = name.trimmed().toLower();
    if (n == QLatin1String("usb"))
        return Usb;
    if (n.startsWith(QLatin1String("bluetooth")))
        return n.contains(QLatin1String("low energy")) || n.endsWith(QLatin1String(" le"))
               ? BluetoothLE : Bluetooth;
    return UnknownTransport;
}

Transport transportFromPath(const QString &path)
{
    // "<transport>_<vid>_<pid>_<location>": the transport is all but the last three fields.
    int cut = path.size();
    for (int field = 0; field < 3; ++field)
    {
        if (cut <= 0)
            return UnknownTransport;
        cut = path.lastIndexOf(QLatin1Char('_'), cut - 1);
    }
    if (cut <= 0)
        return UnknownTransport;
    return transportFromString(path.left(cut));
}

QString transportName(Transport transport)
{
    switch (transport)
    {
        case Usb:         return QStringLiteral("USB");
        case Bluetooth:   return QStringLiteral("Bluetooth");
        case BluetoothLE: return QStringLiteral("Bluetooth LE");
        case UnknownTransport: break;
    }
    return QStringLiteral("unknown connection");
}

QString modelName(quint16 vendorId, quint16 productId)
{
    const ModelEntry *m = findModel(vendorId, productId);
    return m != nullptr ? QString::fromLatin1(m->name) : QString();
}

Transport transportOfProduct(quint16 vendorId, quint16 productId)
{
    const ModelEntry *m = findModel(vendorId, productId);
    return m != nullptr ? m->transport : UnknownTransport;
}

Family family(quint16 vendorId, quint16 productId, Transport transport,
              bool hasStandardElements, int maxButtonUsage, bool hasSimulationTriggers)
{
    Q_UNUSED(transport)
    if (vendorId != kMicrosoftVendorId)
        return NotXbox;
    const ModelEntry *m = findModel(vendorId, productId);
    /* The Xbox 360 pads need a third-party driver on a Mac, and it is that
       driver's layout they arrive in: keep their own order. */
    if (m != nullptr && m->family == NotXbox)
        return NotXbox;
    /* Apple's driver for the USB pads shows no standard element at all,
       only the vendor one carrying the GIP report. */
    if (!hasStandardElements)
        return XboxGip;
    if (m != nullptr && (m->family == XboxBtClassic || m->family == XboxBtModern))
        return m->family;
    /* A pad the table does not know (or a USB one showing standard
       elements): the later firmwares' gaps put Menu, the stick clicks and
       the Xbox button past Button 10, and they alone use Brake/Accelerator. */
    if (hasSimulationTriggers || maxButtonUsage > 10)
        return XboxBtModern;
    return XboxBtClassic;
}

QString familyName(Family family)
{
    switch (family)
    {
        case NotXbox:       return QStringLiteral("HID elements, in the order the device lists them");
        case XboxGip:       return QStringLiteral("Xbox USB report (GIP)");
        case XboxBtClassic: return QStringLiteral("Xbox Bluetooth layout (first firmware)");
        case XboxBtModern:  return QStringLiteral("Xbox Bluetooth layout");
    }
    return QString();
}

Mapping mapElement(Family family, quint32 usagePage, quint32 usage)
{
    if (family != XboxBtClassic && family != XboxBtModern)
        return Mapping();
    const bool modern = family == XboxBtModern;

    switch (usagePage)
    {
        case kPageGenericDesktop:
            switch (usage)
            {
                case kGDX:  return axis(LeftX);
                case kGDY:  return axis(LeftY);
                case kGDZ:  return axis(modern ? RightX : LeftTrigger);
                case kGDRz: return axis(modern ? RightY : RightTrigger);
                case kGDRx: return modern ? Mapping() : axis(RightX);
                case kGDRy: return modern ? Mapping() : axis(RightY);
                case kGDHat: return hat();
                case kGDSystemMainMenu: return button(Guide);
                default: break;
            }
            break;
        case kPageSimulation:
            // Only the later firmwares have them (family() tells them apart so).
            if (modern && usage == kSimBrake)
                return axis(LeftTrigger);
            if (modern && usage == kSimAccelerator)
                return axis(RightTrigger);
            break;
        case kPageButton:
            if (modern)
            {
                switch (usage)
                {
                    case 1:  return button(A);
                    case 2:  return button(B);
                    case 4:  return button(X);
                    case 5:  return button(Y);
                    case 7:  return button(LB);
                    case 8:  return button(RB);
                    case 11: return button(View);
                    case 12: return button(Menu);
                    case 13: return button(Guide);
                    case 14: return button(LS);
                    case 15: return button(RS);
                    default: break;
                }
            }
            else if (usage >= 1 && usage <= 10)
            {
                static const int classic[10] = { A, B, X, Y, LB, RB, View, Menu, LS, RS };
                return button(classic[usage - 1]);
            }
            break;
        case kPageConsumer:
            if (usage == kConsumerACHome)
                return button(Guide);
            if (usage == kConsumerACBack)
                return button(View);
            if (usage == kConsumerRecord)
                return button(Share);
            break;
        default:
            break;
    }
    return Mapping();
}

void hatToDpad(int value, int logicalMin, int logicalMax, bool dpad[4])
{
    dpad[0] = dpad[1] = dpad[2] = dpad[3] = false;
    const int dir = value - logicalMin;
    // Eight directions; a value outside them is the null state, centred.
    if (dir < 0 || dir > 7 || value > logicalMax)
        return;
    dpad[0] = dir == 7 || dir == 0 || dir == 1;   // up
    dpad[3] = dir >= 1 && dir <= 3;               // right
    dpad[1] = dir >= 3 && dir <= 5;               // down
    dpad[2] = dir >= 5 && dir <= 7;               // left
}

void centre(PadState *state)
{
    for (int a = 0; a < AxisCount; ++a)
        state->axes[a] = (a == LeftTrigger || a == RightTrigger) ? 0 : stickToByte(0);
    for (int b = 0; b < ButtonCount; ++b)
        state->buttons[b] = false;
}

uchar stickToByte(int raw)
{
    const qint64 clamped = qBound(-32768, raw, 32767);
    return uchar(((clamped + 32768) * 255) / 65535);
}

uchar scaleToByte(int value, int min, int max)
{
    if (max <= min)
        return uchar(qBound(0, value, 255));
    const qint64 clamped = qBound(min, value, max);
    return uchar(((clamped - min) * 255) / (qint64(max) - min));
}

bool parseGipReport(const uchar *report, int length, PadState *state)
{
    if (report == nullptr || state == nullptr || length < 1)
        return false;

    /* The Xbox button has a report of its own (GIP command 0x07): its
       state is bit 0 of the first byte after the 4-byte header. */
    if (report[0] == 0x07)
    {
        if (length < 5)
            return false;
        state->buttons[Guide] = report[4] & 0x01;
        return true;
    }

    /* GIP input (command 0x20), as Linux's xpad reads it -- after the 4-byte
       header (command, flags, sequence, length):
         [4]     b2 Menu  b3 View  b4 A  b5 B  b6 X  b7 Y
         [5]     b0 Up  b1 Down  b2 Left  b3 Right  b4 LB  b5 RB  b6 LS  b7 RS
         [6-7]   LT  0..1023        [8-9]   RT  0..1023
         [10-11] LX  int16          [12-13] LY  int16, UP positive
         [14-15] RX  int16          [16-17] RY  int16, UP positive */
    if (report[0] != 0x20 || length < 18)
        return false;

    const uchar b4 = report[4];
    const uchar b5 = report[5];
    state->buttons[Menu] = b4 & 0x04;
    state->buttons[View] = b4 & 0x08;
    state->buttons[A]    = b4 & 0x10;
    state->buttons[B]    = b4 & 0x20;
    state->buttons[X]    = b4 & 0x40;
    state->buttons[Y]    = b4 & 0x80;
    state->buttons[DpadUp]    = b5 & 0x01;
    state->buttons[DpadDown]  = b5 & 0x02;
    state->buttons[DpadLeft]  = b5 & 0x04;
    state->buttons[DpadRight] = b5 & 0x08;
    state->buttons[LB] = b5 & 0x10;
    state->buttons[RB] = b5 & 0x20;
    state->buttons[LS] = b5 & 0x40;
    state->buttons[RS] = b5 & 0x80;

    auto u16 = [report](int i) { return int(report[i] | (report[i + 1] << 8)); };
    auto s16 = [report](int i) { return int(qint16(quint16(report[i] | (report[i + 1] << 8)))); };
    state->axes[LeftTrigger]  = scaleToByte(u16(6), 0, 1023);
    state->axes[RightTrigger] = scaleToByte(u16(8), 0, 1023);
    state->axes[LeftX]  = stickToByte(s16(10));
    // GIP has up positive; the profile (as HID) has up = 0.
    state->axes[LeftY]  = uchar(255 - stickToByte(s16(12)));
    state->axes[RightX] = stickToByte(s16(14));
    state->axes[RightY] = uchar(255 - stickToByte(s16(16)));
    return true;
}

QString axisName(int axis)
{
    static const char *names[AxisCount] = { "Left Stick X", "Left Stick Y", "Left Trigger",
                                            "Right Stick X", "Right Stick Y", "Right Trigger" };
    return axis >= 0 && axis < AxisCount ? QString::fromLatin1(names[axis]) : QString();
}

QString buttonName(int button)
{
    static const char *names[ButtonCount] = { "A", "B", "X", "Y", "LB", "RB", "View", "Menu",
                                              "Left Stick", "Right Stick", "Xbox",
                                              "D-pad Up", "D-pad Down", "D-pad Left",
                                              "D-pad Right", "Share" };
    return button >= 0 && button < ButtonCount ? QString::fromLatin1(names[button]) : QString();
}

QString axisRole(int axis)
{
    static const char *roles[AxisCount] = { "lx", "ly", "lt", "rx", "ry", "rt" };
    return axis >= 0 && axis < AxisCount ? QString::fromLatin1(roles[axis]) : QString();
}

QString buttonRole(int button)
{
    static const char *roles[ButtonCount] = { "a", "b", "x", "y", "lb", "rb", "back", "start",
                                              "ls", "rs", "guide", "dpad_up", "dpad_down",
                                              "dpad_left", "dpad_right", "" };
    return button >= 0 && button < ButtonCount ? QString::fromLatin1(roles[button]) : QString();
}

QString elementName(quint32 usagePage, quint32 usage)
{
    const QString hex = QStringLiteral("0x%1").arg(usage, 0, 16);
    switch (usagePage)
    {
        case kPageGenericDesktop:
            switch (usage)
            {
                case kGDX:  return QStringLiteral("X");
                case kGDY:  return QStringLiteral("Y");
                case kGDZ:  return QStringLiteral("Z");
                case kGDRx: return QStringLiteral("Rx");
                case kGDRy: return QStringLiteral("Ry");
                case kGDRz: return QStringLiteral("Rz");
                case 0x36:  return QStringLiteral("Slider");
                case 0x37:  return QStringLiteral("Dial");
                case 0x38:  return QStringLiteral("Wheel");
                case kGDHat: return QStringLiteral("Hat switch");
                case kGDSystemMainMenu: return QStringLiteral("System Main Menu");
                default:    return QStringLiteral("Generic Desktop %1").arg(hex);
            }
        case kPageSimulation:
            if (usage == kSimBrake)
                return QStringLiteral("Brake");
            if (usage == kSimAccelerator)
                return QStringLiteral("Accelerator");
            return QStringLiteral("Simulation %1").arg(hex);
        case kPageButton:
            return QStringLiteral("Button %1").arg(usage);
        case kPageConsumer:
            if (usage == kConsumerACHome)
                return QStringLiteral("Consumer %1 (AC Home)").arg(hex);
            if (usage == kConsumerACBack)
                return QStringLiteral("Consumer %1 (AC Back)").arg(hex);
            if (usage == kConsumerRecord)
                return QStringLiteral("Consumer %1 (Record)").arg(hex);
            return QStringLiteral("Consumer %1").arg(hex);
        default:
            break;
    }
    return QStringLiteral("Page 0x%1 usage %2").arg(usagePage, 0, 16).arg(hex);
}

}

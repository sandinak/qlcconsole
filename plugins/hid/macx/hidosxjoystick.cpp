/*
  Q Light Controller Plus
  hidosxjoystick.cpp

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
#include <QDebug>
#include <QtGlobal>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "hidosxjoystick.h"

HIDOSXJoystick::HIDOSXJoystick(HIDPlugin *parent, quint32 line, struct hid_device_info *info)
    : HIDJsDevice(parent, line, info)
{
    init();
}

HIDOSXJoystick::~HIDOSXJoystick()
{
    if (m_vendorXboxElem)
    {
        CFRelease(m_vendorXboxElem);
        m_vendorXboxElem = nullptr;
    }
    if (m_hidHandle)
    {
        hid_close(m_hidHandle);
        m_hidHandle = nullptr;
    }
}

bool HIDOSXJoystick::isJoystick(unsigned short usage)
{
    if (usage == kHIDUsage_GD_Joystick || usage == kHIDUsage_GD_GamePad ||
        usage == kHIDUsage_GD_MultiAxisController || usage == kHIDUsage_GD_Hatswitch)
        return true;

    return false;
}

static int32_t get_int_property(IOHIDDeviceRef device, CFStringRef key)
{
    CFTypeRef ref;
    int32_t value;

    ref = IOHIDDeviceGetProperty(device, key);
    if (ref) {
        if (CFGetTypeID(ref) == CFNumberGetTypeID()) {
            CFNumberGetValue((CFNumberRef) ref, kCFNumberSInt32Type, &value);
            return value;
        }
    }
    return 0;
}

void HIDOSXJoystick::init()
{
    m_HIDManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    IOHIDManagerOpen(m_HIDManager, kIOHIDOptionsTypeNone);
    /* Get a list of the Devices */
    IOHIDManagerSetDeviceMatching(m_HIDManager, NULL);
    CFSetRef device_set = IOHIDManagerCopyDevices(m_HIDManager);

    /* Convert the list into a C array so we can iterate easily. */
    CFIndex num_devices = CFSetGetCount(device_set);
    IOHIDDeviceRef *device_array = (IOHIDDeviceRef *)calloc(num_devices, sizeof(IOHIDDeviceRef));
    CFSetGetValues(device_set, (const void **) device_array);

    m_IOKitDevice = 0;
    m_axesNumber = 0;
    m_buttonsNumber = 0;

    // Parse the location ID from the hidapi path (format: "transport_vid_pid_location" hex).
    // The location ID uniquely identifies the physical USB port / BT session — it avoids
    // grabbing Apple's AppleUserHIDDevice wrapper (same VID+PID, wrong elements) that can
    // appear before the real HID interface in the IOKit enumeration order.
    int32_t targetLocation = 0;
    if (m_dev_info->path)
    {
        const char *last = strrchr(m_dev_info->path, '_');
        if (last)
            targetLocation = (int32_t)strtoul(last + 1, nullptr, 16);
    }
    FILE *dbg = fopen("/tmp/qlc_hid_debug.txt", "a");
    if (dbg) {
        fprintf(dbg, "HIDOSXJoystick::init path=%s targetLoc=0x%x VID=0x%x PID=0x%x numDevices=%d\n",
                m_dev_info->path ? m_dev_info->path : "(null)",
                (unsigned)targetLocation, m_dev_info->vendor_id, m_dev_info->product_id, (int)num_devices);
        fflush(dbg);
    }
    qDebug() << Q_FUNC_INFO << "path:" << m_dev_info->path
             << "targetLocation:" << QString::number(targetLocation, 16)
             << "VID:" << QString::number(m_dev_info->vendor_id, 16)
             << "PID:" << QString::number(m_dev_info->product_id, 16)
             << "numIOKitDevices:" << num_devices;

    for (int d = 0; d < num_devices; d++)
    {
        unsigned short vid = get_int_property(device_array[d], CFSTR(kIOHIDVendorIDKey));
        unsigned short pid = get_int_property(device_array[d], CFSTR(kIOHIDProductIDKey));
        int32_t location  = get_int_property(device_array[d], CFSTR(kIOHIDLocationIDKey));
        if (dbg) fprintf(dbg, "  iokit[%d] VID=0x%x PID=0x%x Loc=0x%x\n", d, vid, pid, (unsigned)location);
        qDebug() << Q_FUNC_INFO << "  IOKit device" << d
                 << "VID:" << QString::number(vid, 16)
                 << "PID:" << QString::number(pid, 16)
                 << "Loc:" << QString::number(location, 16);

        // Prefer location-ID match (precise); fall back to VID+PID when location is absent.
        bool match = (targetLocation != 0 && location == targetLocation) ||
                     (targetLocation == 0 && m_dev_info->vendor_id == vid &&
                      m_dev_info->product_id == pid);
        if (match)
        {
            if (dbg) fprintf(dbg, "  -> MATCHED device %d\n", d);
            qDebug() << Q_FUNC_INFO << "  -> matched device" << d;
            m_IOKitDevice = device_array[d];
            break;
        }
    }

    if (m_IOKitDevice == 0)
    {
        if (dbg) { fprintf(dbg, "  NO MATCH FOUND\n"); fclose(dbg); }
        qWarning() << Q_FUNC_INFO << "no IOKit HID device found for path" << m_dev_info->path
                   << "(VID" << QString::number(m_dev_info->vendor_id, 16)
                   << "PID" << QString::number(m_dev_info->product_id, 16) << ")";
        free(device_array);
        CFRelease(device_set);
        return;
    }

    // to return all elements for a device
    CFArrayRef elementsArray = IOHIDDeviceCopyMatchingElements(m_IOKitDevice, NULL, kIOHIDOptionsTypeNone);
    if (elementsArray)
    {
        CFIndex count = CFArrayGetCount(elementsArray);
        if (dbg) fprintf(dbg, "  device found! %ld elements\n", (long)count);
        qDebug() << Q_FUNC_INFO << "device has" << count << "elements total";

        for (int i = 0; i < count; i++)
        {
            IOHIDElementRef elem = (IOHIDElementRef)CFArrayGetValueAtIndex(elementsArray, i);
            IOHIDElementType elemType = IOHIDElementGetType(elem);
            unsigned int usage = IOHIDElementGetUsage(elem);
            unsigned int usagePage = IOHIDElementGetUsagePage(elem);
            if (dbg && i < 40) {
                fprintf(dbg, "    elem[%d] type=%d page=0x%x usage=0x%x rptSize=%u rptCnt=%u\n",
                        i, (int)elemType, usagePage, usage,
                        (unsigned)IOHIDElementGetReportSize(elem),
                        (unsigned)IOHIDElementGetReportCount(elem));
            }
            qDebug() << Q_FUNC_INFO << "  elem" << i
                     << "type:" << elemType
                     << "page:" << QString::number(usagePage, 16)
                     << "usage:" << QString::number(usage, 16);

            // Capture vendor-page Misc Input elements exposed by Apple's AppleUserHIDDevice
            // kext.  usage=0x20 (Report ID 32) carries the Xbox Series X main controller state.
            if (elemType == kIOHIDElementTypeInput_Misc && usagePage == 0xff00 && usage == 0x20)
            {
                m_vendorXboxElem = elem;
                CFRetain(m_vendorXboxElem);
                if (dbg) fprintf(dbg, "    -> saved vendorXboxElem\n");
            }

            if (elemType == kIOHIDElementTypeInput_Misc ||
               elemType == kIOHIDElementTypeInput_Button ||
               elemType == kIOHIDElementTypeInput_Axis ||
               elemType == kIOHIDElementTypeInput_ScanCodes)
            {
                switch(usagePage)
                {
                    case kHIDPage_GenericDesktop:
                        switch(usage)
                        {
                            case kHIDUsage_GD_X:
                            case kHIDUsage_GD_Y:
                            case kHIDUsage_GD_Z:
                            case kHIDUsage_GD_Rx:
                            case kHIDUsage_GD_Ry:
                            case kHIDUsage_GD_Rz:
                            case kHIDUsage_GD_Slider:
                            case kHIDUsage_GD_Dial:
                            case kHIDUsage_GD_Wheel:
                            {
                                HIDInfo axis;
                                axis.IOHIDRef = elem;
                                axis.value    = 0;
                                axis.logicalMin = (int)IOHIDElementGetLogicalMin(elem);
                                axis.logicalMax = (int)IOHIDElementGetLogicalMax(elem);
                                // Clamp sanity: treat degenerate range as 0-255
                                if (axis.logicalMax <= axis.logicalMin)
                                {
                                    axis.logicalMin = 0;
                                    axis.logicalMax = 255;
                                }
                                m_axesValues.append(axis);
                                m_axesNumber++;
                            }
                            break;
                            default:
                            break;
                        }
                    break;
                    case kHIDPage_Button:
                    {
                        HIDInfo button;
                        button.IOHIDRef = elem;
                        button.value = 0;
                        m_buttonsValues.append(button);
                        m_buttonsNumber++;
                    }
                    break;
                    default:
                    break;
                }
            }
        }
        CFRelease(elementsArray);
    }
    // If the IOKit element API yielded no standard axes/buttons (e.g. Apple's
    // AppleUserHIDDevice kext virtualised the controller), switch to vendor-element
    // polling mode.  The element at page=0xff00, usage=0x20 is Apple's wrapper for
    // the Xbox Series X main controller state; we read its raw bytes via
    // IOHIDValueGetBytePtr and parse them as an Xbox input report.
    if (m_axesNumber == 0 && m_buttonsNumber == 0)
    {
        if (m_vendorXboxElem && m_dev_info->vendor_id == 0x045E)
        {
            // Axes 0-5:  LX, LY, RX, RY, LT, RT
            // Buttons 0-9: A, B, X, Y, LB, RB, View, Menu, LS, RS
            m_axesNumber    = 6;
            m_buttonsNumber = 10;
            m_rawMode       = true;
            if (dbg) fprintf(dbg, "  vendor-element mode (Xbox): axes=%d buttons=%d\n",
                             m_axesNumber, m_buttonsNumber);
        }
        else if (m_dev_info->path)
        {
            // Last resort: hidapi raw read (may return 0 if kext seizes reports)
            m_hidHandle = hid_open_path(m_dev_info->path);
            if (m_hidHandle)
            {
                hid_set_nonblocking(m_hidHandle, 1);
                m_rawMode = true;
                if (m_dev_info->vendor_id == 0x045E)
                {
                    m_axesNumber    = 6;
                    m_buttonsNumber = 10;
                }
                if (dbg) fprintf(dbg, "  hidapi raw mode fallback (axes=%d buttons=%d)\n",
                                 m_axesNumber, m_buttonsNumber);
            }
            else
            {
                if (dbg) fprintf(dbg, "  raw mode: hid_open_path failed\n");
            }
        }

        // Populate placeholder HIDInfo entries for change tracking in readEvent()
        // (IOHIDRef left null — we don't poll them via standard element API in raw mode)
        for (int i = 0; i < m_axesNumber; i++)
        {
            HIDInfo a; a.IOHIDRef = nullptr; a.value = -1; a.logicalMin = 0; a.logicalMax = 255;
            m_axesValues.append(a);
        }
        for (int i = 0; i < m_buttonsNumber; i++)
        {
            HIDInfo b; b.IOHIDRef = nullptr; b.value = -1; b.logicalMin = 0; b.logicalMax = 1;
            m_buttonsValues.append(b);
        }
    }

    if (dbg) {
        fprintf(dbg, "  final: axes=%d buttons=%d rawMode=%d\n",
                m_axesNumber, m_buttonsNumber, (int)m_rawMode);
        fclose(dbg);
    }
    free(device_array);
    CFRelease(device_set);
}

// ---------------------------------------------------------------------------
// Raw report parser — Xbox Series X / One (USB, vendor 0x045E)
// Report ID 0x20, 17 bytes total (including the report-ID byte).
//
// [0]    Report ID  (0x20)
// [1]    Buttons A: bit2=Menu  bit3=View  bit4=A  bit5=B  bit6=X  bit7=Y
// [2]    Buttons B: bit2=LS    bit3=RS    bit4=LB bit5=RB bit6=LT-d bit7=RT-d
// [3]    D-pad nibble (0=up … 7=up-left, 8=centre)
// [4-5]  LT  uint16 LE  0-1023
// [6-7]  RT  uint16 LE  0-1023
// [8-9]  LX  int16  LE  –32768..32767
// [10-11]LY  int16  LE
// [12-13]RX  int16  LE
// [14-15]RY  int16  LE
// [16]   reserved
//
// QLC+ axis indices: 0=LX 1=LY 2=RX 3=RY 4=LT 5=RT
// QLC+ button indices (offset by axesNumber=6): 0=A 1=B 2=X 3=Y 4=LB 5=RB
//                                                6=View 7=Menu 8=LS 9=RS

static uchar xboxAxisNorm(int raw)
{
    // raw is int16 -32768..32767 → 0..255
    const int clamped = qBound(-32768, raw, 32767);
    return (uchar)(((clamped + 32768) * 255) / 65535);
}

static uchar xboxTriggerNorm(int raw)
{
    // raw is uint16 0..1023 → 0..255
    const int clamped = qBound(0, raw, 1023);
    return (uchar)((clamped * 255) / 1023);
}

bool HIDOSXJoystick::readEvent()
{
    IOHIDValueRef valueRef;
    int value;

    if (m_rawMode)
    {
        // In raw mode, data is delivered via s_inputReportCallback registered in run().
        // readEvent() is not called from run() in this mode, so this branch is only
        // reached if somehow the base class run() is used (shouldn't happen).
        return true;

        // ---------------------------------------------------------------------------
        // (Dead code below — kept for reference during debugging; remove later)
        // Primary path: Apple's AppleUserHIDDevice exposes the Xbox controller state
        // as a vendor-page IOKit element (page=0xff00, usage=0x20).  We poll it via
        // IOHIDDeviceGetValue and read the raw bytes with IOHIDValueGetBytePtr.
        if (m_vendorXboxElem && m_IOKitDevice)
        {
            IOHIDValueRef valueRef = nullptr;
            IOReturn ret = IOHIDDeviceGetValue(m_IOKitDevice, m_vendorXboxElem, &valueRef);

            if (ret == kIOReturnSuccess && valueRef)
            {
                CFIndex len = IOHIDValueGetLength(valueRef);
                const uint8_t *bytes = IOHIDValueGetBytePtr(valueRef);

                // Diagnostic: log first poll + any poll where bytes changed (up to 500 events)
                bool changed = (m_rawDumpCount == 0);
                if (!changed && bytes && len <= (CFIndex)sizeof(m_rawPrevBytes))
                {
                    for (CFIndex i = 0; i < len; i++)
                        if (bytes[i] != m_rawPrevBytes[i]) { changed = true; break; }
                }
                if ((m_rawDumpCount < 500) && changed && bytes)
                {
                    FILE *dbg = fopen("/tmp/qlc_hid_debug.txt", "a");
                    if (dbg)
                    {
                        if (m_rawDumpCount == 0)
                            fprintf(dbg, "--- vendor-element dump (logs on change) ---\n");
                        fprintf(dbg, "e[%d] len=%ld:", m_rawDumpCount, (long)len);
                        for (CFIndex i = 0; i < len && i < 32; i++)
                            fprintf(dbg, " %02x", bytes[i]);
                        fprintf(dbg, "\n");
                        fclose(dbg);
                    }
                    if (len <= (CFIndex)sizeof(m_rawPrevBytes))
                        memcpy(m_rawPrevBytes, bytes, len);
                    m_rawDumpCount++;
                }

                // Parse Xbox Series X input: the vendor element carries the controller
                // state WITHOUT a leading report-ID byte (Apple strips it).
                // Layout (based on Xbox One/Series X USB HID, 16 bytes):
                //   [0-1] buttons, [2] d-pad, [3-4] LT, [5-6] RT,
                //   [7-8] LX, [9-10] LY, [11-12] RX, [13-14] RY
                // BUT we don't know Apple's exact layout until we see the bytes.
                // Log first 10 polls above; implement parser once layout is confirmed.
                if (bytes && len >= 14 && m_dev_info->vendor_id == 0x045E)
                {
                    auto emitAxis = [&](int idx, uchar norm) {
                        if (idx < m_axesValues.size() && (uchar)m_axesValues[idx].value != norm)
                        {
                            m_axesValues[idx].value = norm;
                            emit valueChanged(UINT_MAX, m_line, idx, norm);
                        }
                    };
                    auto emitBtn = [&](int idx, bool pressed) {
                        uchar val = pressed ? UCHAR_MAX : 0;
                        if (idx < m_buttonsValues.size() && (uchar)m_buttonsValues[idx].value != val)
                        {
                            m_buttonsValues[idx].value = val;
                            emit valueChanged(UINT_MAX, m_line, m_axesNumber + idx, val);
                        }
                    };

                    // Tentative layout (NO report-ID prefix):
                    // [0-1] buttons A/B, [2] d-pad, [3-4] LT u16, [5-6] RT u16
                    // [7-8] LX i16, [9-10] LY i16, [11-12] RX i16, [13-14] RY i16
                    if (len >= 15)
                    {
                        const uint8_t b1 = bytes[0];
                        const uint8_t b2 = bytes[1];
                        const uint16_t lt = (uint16_t)(bytes[3]  | (bytes[4]  << 8));
                        const uint16_t rt = (uint16_t)(bytes[5]  | (bytes[6]  << 8));
                        const int16_t  lx = (int16_t)(bytes[7]  | (bytes[8]  << 8));
                        const int16_t  ly = (int16_t)(bytes[9]  | (bytes[10] << 8));
                        const int16_t  rx = (int16_t)(bytes[11] | (bytes[12] << 8));
                        const int16_t  ry = (int16_t)(bytes[13] | (bytes[14] << 8));

                        emitAxis(0, xboxAxisNorm(lx));
                        emitAxis(1, xboxAxisNorm(ly));
                        emitAxis(2, xboxAxisNorm(rx));
                        emitAxis(3, xboxAxisNorm(ry));
                        emitAxis(4, xboxTriggerNorm(lt));
                        emitAxis(5, xboxTriggerNorm(rt));

                        emitBtn(0, b1 & 0x10); // A
                        emitBtn(1, b1 & 0x20); // B
                        emitBtn(2, b1 & 0x40); // X
                        emitBtn(3, b1 & 0x80); // Y
                        emitBtn(4, b2 & 0x10); // LB
                        emitBtn(5, b2 & 0x20); // RB
                        emitBtn(6, b1 & 0x08); // View
                        emitBtn(7, b1 & 0x04); // Menu
                        emitBtn(8, b2 & 0x04); // LS
                        emitBtn(9, b2 & 0x08); // RS
                    }
                }
            }
            else if (m_rawDumpCount < 10)
            {
                FILE *dbg = fopen("/tmp/qlc_hid_debug.txt", "a");
                if (dbg) { fprintf(dbg, "vpoll[%d] ret=0x%x (fail)\n", m_rawDumpCount, ret); fclose(dbg); }
                m_rawDumpCount++;
            }
            return true;
        }

        // ---------------------------------------------------------------------------
        // Fallback: hidapi raw read (may return 0 if Apple's kext consumed reports)
        if (m_hidHandle)
        {
            unsigned char buf[64] = {};
            int n = hid_read(m_hidHandle, buf, sizeof(buf));
            if (m_rawDumpCount < 10)
            {
                FILE *dbg = fopen("/tmp/qlc_hid_debug.txt", "a");
                if (dbg) {
                    fprintf(dbg, "hpoll[%d] n=%d\n", m_rawDumpCount, n);
                    fclose(dbg);
                }
                m_rawDumpCount++;
            }
            // hid_read parser would go here once we confirm we get data
        }
        return true;
    }

    if (m_IOKitDevice == 0)
        return false;

    for (int b = 0; b < m_buttonsNumber; b++)
    {
        IOHIDDeviceGetValue(m_IOKitDevice, m_buttonsValues[b].IOHIDRef, &valueRef);
        value = IOHIDValueGetIntegerValue(valueRef);

        if (value != m_buttonsValues[b].value)
        {
            uchar val;
            if (value != 0)
                val = UCHAR_MAX;
            else
                val = 0;

            emit valueChanged(UINT_MAX, m_line, m_axesNumber + b, val);
            m_buttonsValues[b].value = value;
        }
    }
    for (int a = 0; a < m_axesNumber; a++)
    {
        IOHIDDeviceGetValue(m_IOKitDevice, m_axesValues[a].IOHIDRef, &valueRef);
        value = IOHIDValueGetIntegerValue(valueRef);

        if (value != m_axesValues[a].value)
        {
            // Normalize raw axis value to 0–255 using the element's logical
            // range.  Xbox/Microsoft controllers use -32768..32767, so a plain
            // uchar() truncation produces garbage; this mapping is correct for
            // any device range.
            const int lo  = m_axesValues[a].logicalMin;
            const int hi  = m_axesValues[a].logicalMax;
            const int clamped = qBound(lo, value, hi);
            const uchar norm = (uchar)(((clamped - lo) * 255) / (hi - lo));
            emit valueChanged(UINT_MAX, m_line, a, norm);
            m_axesValues[a].value = value;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Static C callback — IOHIDDevice fires this for every incoming input report.
// The report bytes do NOT include the leading report-ID byte; it is passed
// separately as reportID.

static void s_inputReportCallback(void *ctx,
                                  IOReturn /*result*/,
                                  void * /*sender*/,
                                  IOHIDReportType type,
                                  uint32_t reportID,
                                  uint8_t *report,
                                  CFIndex reportLength)
{
    if (type != kIOHIDReportTypeInput)
        return;
    reinterpret_cast<HIDOSXJoystick *>(ctx)->processRawReport(reportID, report, reportLength);
}

void HIDOSXJoystick::processRawReport(uint32_t reportID, const uint8_t *report, CFIndex len)
{
    // Diagnostic: log up to 500 changes
    bool changed = (m_rawDumpCount == 0);
    if (!changed && report && len <= (CFIndex)sizeof(m_rawPrevBytes))
    {
        for (CFIndex i = 0; i < len; i++)
            if (report[i] != m_rawPrevBytes[i]) { changed = true; break; }
    }
    if (m_rawDumpCount < 500 && changed && report)
    {
        FILE *dbg = fopen("/tmp/qlc_hid_debug.txt", "a");
        if (dbg)
        {
            if (m_rawDumpCount == 0)
                fprintf(dbg, "--- input-report callback dump ---\n");
            fprintf(dbg, "rpt[%d] id=0x%02x len=%ld:", m_rawDumpCount, reportID, (long)len);
            for (CFIndex i = 0; i < len && i < 32; i++)
                fprintf(dbg, " %02x", report[i]);
            fprintf(dbg, "\n");
            fclose(dbg);
        }
        if (len <= (CFIndex)sizeof(m_rawPrevBytes))
            memcpy(m_rawPrevBytes, report, len);
        m_rawDumpCount++;
    }

    // Parse Xbox Series X / One USB report 0x20 via Apple's AppleUserHIDDevice wrapper.
    // Apple includes the report-ID byte at report[0], verified from live dump data.
    //
    //   [0]     0x20  report ID (duplicated; also passed as reportID param)
    //   [1]     sequence counter (skips 0; wraps 255→1)
    //   [2-3]   timestamp uint16 LE (ms, wraps ~65 s)
    //   [4]     buttons 1: d0=DpadUp d1=DpadDn d2=DpadL d3=DpadR
    //                      d4=View   d5=Menu   d6=LS    d7=RS
    //   [5]     buttons 2: d0=A d1=B d2=X d3=Y d4=LB d5=RB
    //   [6-7]   LT  uint16 LE  0-1023
    //   [8-9]   RT  uint16 LE  0-1023
    //   [10-11] LX  int16 LE  -32768..32767
    //   [12-13] LY  int16 LE
    //   [14-15] RX  int16 LE
    //   [16-17] RY  int16 LE
    //   [18]    0x00 padding
    if (reportID != 0x20 || !report || len < 18 || m_dev_info->vendor_id != 0x045E)
        return;

    auto emitAxis = [&](int idx, uchar norm) {
        if (idx < m_axesValues.size() && (uchar)m_axesValues[idx].value != norm)
        {
            m_axesValues[idx].value = norm;
            emit valueChanged(UINT_MAX, m_line, idx, norm);
        }
    };
    auto emitBtn = [&](int idx, bool pressed) {
        uchar val = pressed ? UCHAR_MAX : 0;
        if (idx < m_buttonsValues.size() && (uchar)m_buttonsValues[idx].value != val)
        {
            m_buttonsValues[idx].value = val;
            emit valueChanged(UINT_MAX, m_line, m_axesNumber + idx, val);
        }
    };

    const uint8_t  b1 = report[4];                                       // d-pad/LS/RS/View/Menu
    const uint8_t  b2 = report[5];                                       // A/B/X/Y/LB/RB
    const uint16_t lt = (uint16_t)(report[6]  | (report[7]  << 8));     // Left trigger
    const uint16_t rt = (uint16_t)(report[8]  | (report[9]  << 8));     // Right trigger
    const int16_t  lx = (int16_t)(report[10] | (report[11] << 8));      // Left stick X
    const int16_t  ly = (int16_t)(report[12] | (report[13] << 8));      // Left stick Y
    const int16_t  rx = (int16_t)(report[14] | (report[15] << 8));      // Right stick X
    const int16_t  ry = (int16_t)(report[16] | (report[17] << 8));      // Right stick Y

    emitAxis(0, xboxAxisNorm(lx));
    emitAxis(1, xboxAxisNorm(ly));
    emitAxis(2, xboxAxisNorm(rx));
    emitAxis(3, xboxAxisNorm(ry));
    emitAxis(4, xboxTriggerNorm(lt));
    emitAxis(5, xboxTriggerNorm(rt));

    emitBtn(0, b2 & 0x01); // A
    emitBtn(1, b2 & 0x02); // B
    emitBtn(2, b2 & 0x04); // X
    emitBtn(3, b2 & 0x08); // Y
    emitBtn(4, b2 & 0x10); // LB
    emitBtn(5, b2 & 0x20); // RB
    emitBtn(6, b1 & 0x10); // View
    emitBtn(7, b1 & 0x20); // Menu
    emitBtn(8, b1 & 0x40); // LS
    emitBtn(9, b1 & 0x80); // RS
}

void HIDOSXJoystick::run()
{
    if (m_rawMode && m_IOKitDevice)
    {
        // Register the raw input-report callback BEFORE scheduling.
        // Apple's AppleUserHIDDevice kext delivers Xbox controller data as raw
        // input reports — element value callbacks don't fire for vendor-page
        // elements. The input-report callback fires on the run loop below.
        IOHIDDeviceRegisterInputReportCallback(m_IOKitDevice,
                                              m_reportBuf, sizeof(m_reportBuf),
                                              s_inputReportCallback, this);

        IOHIDDeviceScheduleWithRunLoop(m_IOKitDevice, CFRunLoopGetCurrent(),
                                       kCFRunLoopDefaultMode);

        FILE *dbg = fopen("/tmp/qlc_hid_debug.txt", "a");
        if (dbg) { fprintf(dbg, "run: registered inputReportCallback + scheduled CFRunLoop\n"); fclose(dbg); }

        while (m_running)
        {
            // 40ms slice — lets IOKit deliver pending input reports to our callback.
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.04, FALSE);
        }

        IOHIDDeviceUnscheduleFromRunLoop(m_IOKitDevice, CFRunLoopGetCurrent(),
                                         kCFRunLoopDefaultMode);
        IOHIDDeviceRegisterInputReportCallback(m_IOKitDevice,
                                              m_reportBuf, sizeof(m_reportBuf),
                                              nullptr, nullptr);
        return;
    }

    // Standard (non-raw) path: delegate to base-class polling loop.
    while (m_running)
    {
        readEvent();
        msleep(50);
    }
}


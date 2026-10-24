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
#include <QElapsedTimer>
#include <QDebug>
#include <QtGlobal>
#include <cstdlib>
#include <cstring>

#include <IOKit/IOKitLib.h>

#include "hidosxjoystick.h"
#include "hidgamecontroller.h"

using namespace GamepadLayout;

HIDOSXJoystick::HIDOSXJoystick(HIDPlugin *parent, quint32 line, struct hid_device_info *info)
    : HIDJsDevice(parent, line, info)
{
    centre(&m_state);
    centre(&m_sent);
    init();
}

bool HIDOSXJoystick::setPadLight(float r, float g, float b)
{
    const int gc = m_gcHandle.loadRelaxed();
    return gc != 0 && HIDGameController::setLight(gc, r, g, b);
}

bool HIDOSXJoystick::padRumble(int pulses)
{
    const int gc = m_gcHandle.loadRelaxed();
    return gc != 0 && HIDGameController::rumble(gc, pulses);
}

HIDOSXJoystick::~HIDOSXJoystick()
{
    /* Stop the reader before what it reads goes: the base class would only
       stop it after these members are gone. */
    if (isRunning())
    {
        m_running = false;
        wait();
    }
    if (m_gcHandle.loadRelaxed() != 0)
        HIDGameController::release(m_gcHandle.fetchAndStoreRelaxed(0));
    if (m_IOKitDevice != nullptr)
    {
        if (m_opened)
            IOHIDDeviceClose(m_IOKitDevice, kIOHIDOptionsTypeNone);
        CFRelease(m_IOKitDevice);
        m_IOKitDevice = nullptr;
    }
    if (m_HIDManager != nullptr)
    {
        CFRelease(m_HIDManager);
        m_HIDManager = nullptr;
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

static QString get_string_property(IOHIDDeviceRef device, CFStringRef key)
{
    CFTypeRef ref = IOHIDDeviceGetProperty(device, key);
    if (ref == nullptr || CFGetTypeID(ref) != CFStringGetTypeID())
        return QString();
    return QString::fromCFString((CFStringRef)ref);
}

void HIDOSXJoystick::init()
{
    m_axesNumber = 0;
    m_buttonsNumber = 0;
    m_IOKitDevice = nullptr;

    const unsigned short vid = m_dev_info->vendor_id;
    const unsigned short pid = m_dev_info->product_id;
    /* hid_device_info's strings belong to the enumeration, which is freed
       once the plugin has made its devices: keep what is needed now. */
    m_productName = m_dev_info->product_string ? QString::fromWCharArray(m_dev_info->product_string)
                                               : QString();
    const QString path = m_dev_info->path ? QString::fromUtf8(m_dev_info->path) : QString();
    m_transport = transportFromPath(path);
    if (m_transport == UnknownTransport)
        m_transport = transportOfProduct(vid, pid);

    /* Look the device up -- only look: opening a manager that matches every
       HID device opened the keyboards too, which is what macOS asks Input
       Monitoring permission for. Just this device is opened, below. */
    m_HIDManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    IOHIDManagerSetDeviceMatching(m_HIDManager, NULL);
    CFSetRef device_set = IOHIDManagerCopyDevices(m_HIDManager);
    CFIndex num_devices = device_set ? CFSetGetCount(device_set) : 0;
    IOHIDDeviceRef *device_array = (IOHIDDeviceRef *)calloc(qMax<CFIndex>(num_devices, 1), sizeof(IOHIDDeviceRef));
    if (device_set)
        CFSetGetValues(device_set, (const void **) device_array);

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

    for (int d = 0; d < num_devices; d++)
    {
        unsigned short dvid = get_int_property(device_array[d], CFSTR(kIOHIDVendorIDKey));
        unsigned short dpid = get_int_property(device_array[d], CFSTR(kIOHIDProductIDKey));
        int32_t location  = get_int_property(device_array[d], CFSTR(kIOHIDLocationIDKey));

        // Prefer location-ID match (precise); fall back to VID+PID when location is absent.
        bool match = (targetLocation != 0 && location == targetLocation) ||
                     (targetLocation == 0 && vid == dvid && pid == dpid);
        if (match)
        {
            m_IOKitDevice = device_array[d];
            CFRetain(m_IOKitDevice);
            break;
        }
    }
    free(device_array);
    if (device_set)
        CFRelease(device_set);

    bool hasStandard = false;
    bool hasSimulation = false;
    int maxButtonUsage = 0;

    if (m_IOKitDevice != nullptr)
    {
        if (m_transport == UnknownTransport)
            m_transport = transportFromString(get_string_property(m_IOKitDevice, CFSTR(kIOHIDTransportKey)));

        io_service_t service = IOHIDDeviceGetService(m_IOKitDevice);
        uint64_t entryID = 0;
        if (service != IO_OBJECT_NULL && IORegistryEntryGetRegistryEntryID(service, &entryID) == KERN_SUCCESS)
            m_registryID = entryID;

        CFArrayRef elementsArray = IOHIDDeviceCopyMatchingElements(m_IOKitDevice, NULL, kIOHIDOptionsTypeNone);
        if (elementsArray)
        {
            CFIndex count = CFArrayGetCount(elementsArray);
            for (int i = 0; i < count; i++)
            {
                IOHIDElementRef elem = (IOHIDElementRef)CFArrayGetValueAtIndex(elementsArray, i);
                IOHIDElementType elemType = IOHIDElementGetType(elem);
                const quint32 usage = IOHIDElementGetUsage(elem);
                const quint32 usagePage = IOHIDElementGetUsagePage(elem);
                if (elemType != kIOHIDElementTypeInput_Misc &&
                    elemType != kIOHIDElementTypeInput_Button &&
                    elemType != kIOHIDElementTypeInput_Axis &&
                    elemType != kIOHIDElementTypeInput_ScanCodes)
                    continue;

                Element e;
                e.page = usagePage;
                e.usage = usage;
                e.logicalMin = (int)IOHIDElementGetLogicalMin(elem);
                e.logicalMax = (int)IOHIDElementGetLogicalMax(elem);
                m_elements.insert(quint32(IOHIDElementGetCookie(elem)), e);

                if (usagePage == kHIDPage_Simulation &&
                    (usage == kHIDUsage_Sim_Brake || usage == kHIDUsage_Sim_Accelerator))
                    hasSimulation = true;

                switch (usagePage)
                {
                    case kHIDPage_GenericDesktop:
                        switch (usage)
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
                                axis.logicalMin = e.logicalMin;
                                axis.logicalMax = e.logicalMax;
                                // Clamp sanity: treat degenerate range as 0-255
                                if (axis.logicalMax <= axis.logicalMin)
                                {
                                    axis.logicalMin = 0;
                                    axis.logicalMax = 255;
                                }
                                m_axesValues.append(axis);
                                hasStandard = true;
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
                        button.logicalMin = 0;
                        button.logicalMax = 1;
                        m_buttonsValues.append(button);
                        maxButtonUsage = qMax(maxButtonUsage, int(usage));
                        hasStandard = true;
                    }
                    break;
                    default:
                    break;
                }
            }
            CFRelease(elementsArray);
        }
    }

    m_family = GamepadLayout::family(vid, pid, m_transport, hasStandard, maxButtonUsage, hasSimulation);

    if (isXbox())
    {
        /* The layout the Xbox profile names, whatever this one reports:
           axes LX LY LT RX RY RT, then the buttons. */
        m_axesNumber = AxisCount;
        m_buttonsNumber = ButtonCount;
        QMutableHashIterator<quint32, Element> it(m_elements);
        while (it.hasNext())
        {
            it.next();
            it.value().mapping = mapElement(m_family, it.value().page, it.value().usage);
        }
        // The elements' own lists are the other devices' reader's.
        m_axesValues.clear();
        m_buttonsValues.clear();
    }
    else
    {
        m_axesNumber = (unsigned char)qMin(m_axesValues.size(), 255);
        m_buttonsNumber = (unsigned char)qMin(m_buttonsValues.size(), 255);
    }

    /* Open just this device. macOS refuses (kIOReturnNotPermitted) when it
       wants Input Monitoring permission for it: inputProblem() says so
       instead of the controller silently doing nothing. */
    if (m_IOKitDevice != nullptr && (m_axesNumber > 0 || m_buttonsNumber > 0))
    {
        m_openResult = IOHIDDeviceOpen(m_IOKitDevice, kIOHIDOptionsTypeNone);
        m_opened = m_openResult == kIOReturnSuccess;
        if (!m_opened)
            qWarning() << Q_FUNC_INFO << m_name << "could not be opened, IOReturn"
                       << QString::number(uint(m_openResult), 16);
    }

    qDebug() << Q_FUNC_INFO << m_name << "path" << path
             << "transport" << transportName(m_transport)
             << "family" << familyName(m_family)
             << "axes" << m_axesNumber << "buttons" << m_buttonsNumber;
}

/*****************************************************************************
 * State
 *****************************************************************************/

bool HIDOSXJoystick::isStale() const
{
    if (m_registryID == 0)
        return false;
    /* The registry entry goes when the device does; a pad that came back
       (switched on again, reconnected after sleep) is a new entry. */
    io_service_t service = IOServiceGetMatchingService(MACH_PORT_NULL,
                                                       IORegistryEntryIDMatching(m_registryID));
    if (service == IO_OBJECT_NULL)
        return true;
    IOObjectRelease(service);
    return false;
}

QString HIDOSXJoystick::connectionInfo() const
{
    const unsigned short vid = vendorId();
    const unsigned short pid = productId();
    QString model = modelName(vid, pid);
    if (model.isEmpty())
        model = m_productName.isEmpty() ? m_name : m_productName;
    const QString ids = QStringLiteral("%1:%2").arg(vid, 4, 16, QLatin1Char('0'))
                                               .arg(pid, 4, 16, QLatin1Char('0')).toUpper();
    QString via;
    const int gc = m_gcHandle.loadRelaxed();
    if (gc != 0)
        via = QStringLiteral("GameController framework (%1)").arg(HIDGameController::describe(gc));
    else if (isXbox())
        via = familyName(m_family);
    else
        via = QStringLiteral("HID elements, polled");
    return QStringLiteral("%1 \u00B7 %2 (%3) \u00B7 read via %4")
        .arg(transportName(m_transport), model, ids, via);
}

QString HIDOSXJoystick::inputProblem() const
{
    if (m_gcHandle.loadRelaxed() != 0)
        return QString();
    if (m_IOKitDevice == nullptr)
        return isXbox() && HIDGameController::available()
               ? QString() // the GameController framework may still have it
               : tr("macOS does not list this controller any more");
    if (!m_opened && m_openResult == kIOReturnNotPermitted)
        return tr("macOS is not letting qlcconsole read the controller. Allow qlcconsole in "
                  "System Settings \342\206\222 Privacy & Security \342\206\222 Input Monitoring, "
                  "then quit and reopen it.");
    if (!m_opened && (m_axesNumber > 0 || m_buttonsNumber > 0))
        return tr("macOS could not open the controller (IOKit error 0x%1)")
               .arg(uint(m_openResult), 0, 16);
    return QString();
}

/*****************************************************************************
 * Reading
 *****************************************************************************/

int HIDOSXJoystick::channelValue(int channel) const
{
    if (channel >= 0 && channel < AxisCount)
        return m_state.axes[channel];
    if (channel >= AxisCount && channel < AxisCount + ButtonCount)
        return m_state.buttons[channel - AxisCount] ? 255 : 0;
    return -1;
}

void HIDOSXJoystick::emitRaw(const QString &element, int raw, int channel)
{
    emit rawInput(m_line, element, raw, channel, channelValue(channel));
}

void HIDOSXJoystick::emitChanges()
{
    for (int a = 0; a < AxisCount; ++a)
    {
        if (m_sentValid && m_sent.axes[a] == m_state.axes[a])
            continue;
        emit valueChanged(UINT_MAX, m_line, quint32(a), m_state.axes[a]);
    }
    for (int b = 0; b < ButtonCount; ++b)
    {
        if (m_sentValid && m_sent.buttons[b] == m_state.buttons[b])
            continue;
        emit valueChanged(UINT_MAX, m_line, quint32(m_axesNumber + b),
                          m_state.buttons[b] ? UCHAR_MAX : 0);
    }
    m_sent = m_state;
    m_sentValid = true;
}

bool HIDOSXJoystick::readEvent()
{
    // Any other device: its elements, polled, in its own order.
    if (isXbox() || m_IOKitDevice == nullptr || !m_opened)
        return false;

    const bool diag = diagnostics();
    for (int b = 0; b < m_buttonsValues.size(); b++)
    {
        IOHIDValueRef valueRef = nullptr;
        if (IOHIDDeviceGetValue(m_IOKitDevice, m_buttonsValues[b].IOHIDRef, &valueRef) != kIOReturnSuccess
            || valueRef == nullptr)
            continue;
        const int value = (int)IOHIDValueGetIntegerValue(valueRef);

        if (value != m_buttonsValues[b].value)
        {
            const uchar val = value != 0 ? UCHAR_MAX : 0;
            emit valueChanged(UINT_MAX, m_line, m_axesNumber + b, val);
            m_buttonsValues[b].value = value;
            if (diag)
            {
                IOHIDElementRef e = m_buttonsValues[b].IOHIDRef;
                emit rawInput(m_line, elementName(IOHIDElementGetUsagePage(e), IOHIDElementGetUsage(e)),
                              value, m_axesNumber + b, val);
            }
        }
    }
    for (int a = 0; a < m_axesValues.size(); a++)
    {
        IOHIDValueRef valueRef = nullptr;
        if (IOHIDDeviceGetValue(m_IOKitDevice, m_axesValues[a].IOHIDRef, &valueRef) != kIOReturnSuccess
            || valueRef == nullptr)
            continue;
        const int value = (int)IOHIDValueGetIntegerValue(valueRef);

        if (value != m_axesValues[a].value)
        {
            // Normalize raw axis value to 0–255 using the element's logical range.
            const uchar norm = scaleToByte(value, m_axesValues[a].logicalMin, m_axesValues[a].logicalMax);
            emit valueChanged(UINT_MAX, m_line, a, norm);
            m_axesValues[a].value = value;
            if (diag)
            {
                IOHIDElementRef e = m_axesValues[a].IOHIDRef;
                emit rawInput(m_line, elementName(IOHIDElementGetUsagePage(e), IOHIDElementGetUsage(e)),
                              value, a, norm);
            }
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// IOKit callbacks: they fire on the reader thread's run loop (run()).

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

static void s_inputValueCallback(void *ctx, IOReturn result, void * /*sender*/, IOHIDValueRef value)
{
    if (result != kIOReturnSuccess || value == nullptr)
        return;
    reinterpret_cast<HIDOSXJoystick *>(ctx)->processValue(value);
}

void HIDOSXJoystick::processRawReport(uint32_t reportID, const uint8_t *report, CFIndex len)
{
    if (report == nullptr || len <= 0 || len > CFIndex(sizeof(m_reportBuf)))
        return;

    /* Apple's driver passes the report with its ID at [0]; should it ever
       not, put it back so the GIP offsets hold. */
    uint8_t buf[sizeof(m_reportBuf) + 1];
    int length = int(len);
    if (report[0] != reportID && length < int(sizeof(buf)))
    {
        buf[0] = uint8_t(reportID);
        memcpy(buf + 1, report, size_t(length));
        ++length;
    }
    else
    {
        memcpy(buf, report, size_t(length));
    }

    if (!parseGipReport(buf, length, &m_state))
        return;

    if (!diagnostics())
        m_prevReportLength = 0;    // so turning them on starts from a clean slate
    else
    {
        const bool guide = buf[0] == 0x07;
        const bool first = m_prevReportLength == 0 || m_prevReport[0] != buf[0];
        auto bitChannel = [](int byte, int bit) -> int {
            static const int b4[8] = { -1, -1, Menu, View, A, B, X, Y };
            static const int b5[8] = { DpadUp, DpadDown, DpadLeft, DpadRight, LB, RB, LS, RS };
            const int b = byte == 4 ? b4[bit] : b5[bit];
            return b < 0 ? -1 : AxisCount + b;
        };
        if (guide)
        {
            emitRaw(QStringLiteral("USB report 0x07 byte 4 bit 0"), buf[4] & 0x01, AxisCount + Guide);
        }
        else
        {
            for (int byte = 4; byte <= 5; ++byte)
                for (int bit = 0; bit < 8; ++bit)
                {
                    const int now = (buf[byte] >> bit) & 1;
                    if (!first && now == ((m_prevReport[byte] >> bit) & 1))
                        continue;
                    if (first && now == 0)
                        continue;
                    emitRaw(QStringLiteral("USB report byte %1 bit %2").arg(byte).arg(bit), now,
                            bitChannel(byte, bit));
                }
            static const int fields[6][2] = { { 6, LeftTrigger }, { 8, RightTrigger }, { 10, LeftX },
                                              { 12, LeftY }, { 14, RightX }, { 16, RightY } };
            for (const auto &f : fields)
            {
                const int at = f[0];
                if (!first && buf[at] == m_prevReport[at] && buf[at + 1] == m_prevReport[at + 1])
                    continue;
                const int word = buf[at] | (buf[at + 1] << 8);
                const int raw = at >= 10 ? int(qint16(quint16(word))) : word;
                emitRaw(QStringLiteral("USB report bytes %1-%2").arg(at).arg(at + 1), raw, f[1]);
            }
            // Anything past the standard state (Share, an Elite's paddles...).
            for (int at = 18; at < length; ++at)
            {
                if (!first && at < m_prevReportLength && buf[at] == m_prevReport[at])
                    continue;
                if (first && buf[at] == 0)
                    continue;
                emitRaw(QStringLiteral("USB report byte %1").arg(at), buf[at], -1);
            }
        }
        if (!guide)
        {
            memcpy(m_prevReport, buf, size_t(qMin(length, int(sizeof(m_prevReport)))));
            m_prevReportLength = length;
        }
    }

    emitChanges();
}

void HIDOSXJoystick::processValue(IOHIDValueRef value)
{
    IOHIDElementRef elem = IOHIDValueGetElement(value);
    if (elem == nullptr)
        return;
    const quint32 cookie = quint32(IOHIDElementGetCookie(elem));
    const auto it = m_elements.constFind(cookie);
    if (it == m_elements.constEnd())
        return;
    const Element &e = it.value();
    const int raw = (int)IOHIDValueGetIntegerValue(value);

    int channel = -1;
    switch (e.mapping.kind)
    {
        case AxisElement:
            m_state.axes[e.mapping.index] = scaleToByte(raw, e.logicalMin, e.logicalMax);
            channel = e.mapping.index;
            break;
        case ButtonElement:
            m_state.buttons[e.mapping.index] = raw != 0;
            channel = AxisCount + e.mapping.index;
            break;
        case HatElement:
        {
            bool dpad[4];
            hatToDpad(raw, e.logicalMin, e.logicalMax, dpad);
            m_state.buttons[DpadUp] = dpad[0];
            m_state.buttons[DpadDown] = dpad[1];
            m_state.buttons[DpadLeft] = dpad[2];
            m_state.buttons[DpadRight] = dpad[3];
            for (int d = 0; d < 4; ++d)
                if (dpad[d])
                    channel = AxisCount + DpadUp + d;
            break;
        }
        case Unmapped:
            break;
    }

    if (diagnostics())
        emitRaw(elementName(e.page, e.usage), raw, channel);
    if (e.mapping.kind != Unmapped)
        emitChanges();
}

bool HIDOSXJoystick::pollGameController()
{
    const int handle = m_gcHandle.loadRelaxed();
    QList<HIDGameController::Event> events;
    const bool diag = diagnostics();
    const bool alive = HIDGameController::read(handle, &m_state, diag ? &events : nullptr);
    for (const HIDGameController::Event &e : events)
        emitRaw(QStringLiteral("GameController ") + e.element, qRound(e.value * 1000.0f), e.channel);
    if (alive)
        emitChanges();
    return alive;
}

void HIDOSXJoystick::scheduleIOKit()
{
    if (m_IOKitDevice == nullptr || !m_opened)
        return;
    if (m_family == XboxGip)
        IOHIDDeviceRegisterInputReportCallback(m_IOKitDevice, m_reportBuf, sizeof(m_reportBuf),
                                               s_inputReportCallback, this);
    else
        IOHIDDeviceRegisterInputValueCallback(m_IOKitDevice, s_inputValueCallback, this);
    IOHIDDeviceScheduleWithRunLoop(m_IOKitDevice, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
}

void HIDOSXJoystick::unscheduleIOKit()
{
    if (m_IOKitDevice == nullptr || !m_opened)
        return;
    IOHIDDeviceUnscheduleFromRunLoop(m_IOKitDevice, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    if (m_family == XboxGip)
        IOHIDDeviceRegisterInputReportCallback(m_IOKitDevice, m_reportBuf, sizeof(m_reportBuf),
                                               nullptr, nullptr);
    else
        IOHIDDeviceRegisterInputValueCallback(m_IOKitDevice, nullptr, nullptr);
}

void HIDOSXJoystick::run()
{
    if (!isXbox())
    {
        // Any other device, as it always was: polled.
        while (m_running)
        {
            readEvent();
            msleep(50);
        }
        return;
    }

    /* An Xbox pad: through the GameController framework when it has it (it
       may only report the pad a moment after the app starts, so keep
       asking), else through IOKit, event by event. */
    centre(&m_state);
    m_sentValid = false;
    m_prevReportLength = 0;
    bool scheduled = false;
    QElapsedTimer sinceAsked;
    bool asked = false;

    while (m_running)
    {
        if (m_gcHandle.loadRelaxed() == 0 && HIDGameController::available()
            && (!asked || sinceAsked.elapsed() >= 500))
        {
            asked = true;
            sinceAsked.restart();
            const int handle = HIDGameController::claim(m_productName);
            if (handle != 0)
            {
                if (scheduled)
                {
                    unscheduleIOKit();
                    scheduled = false;
                }
                m_gcHandle.storeRelaxed(handle);
                m_sentValid = false;
                qDebug() << Q_FUNC_INFO << m_name << "reading through the GameController framework:"
                         << HIDGameController::describe(handle);
            }
        }

        if (m_gcHandle.loadRelaxed() != 0)
        {
            if (!pollGameController())
            {
                // Gone from the framework: back to IOKit until it returns.
                HIDGameController::release(m_gcHandle.fetchAndStoreRelaxed(0));
                asked = false;
                m_sentValid = false;
            }
            msleep(8);
            continue;
        }

        if (!scheduled && m_IOKitDevice != nullptr && m_opened)
        {
            scheduleIOKit();
            scheduled = true;
        }
        if (scheduled)
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, FALSE);
        else
            msleep(50);
    }

    if (scheduled)
        unscheduleIOKit();
    if (m_gcHandle.loadRelaxed() != 0)
        HIDGameController::release(m_gcHandle.fetchAndStoreRelaxed(0));
}

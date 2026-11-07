/*
  Q Light Controller
  hidplugin.cpp

  Copyright (c) Heikki Junnila

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

#include <QApplication>
#include <QFileSystemWatcher>
#include <QSettings>
#include <memory>
#include <QMessageBox>
#include <QStringList>
#include <QTimer>
#include <QDebug>
#include <QDir>

#include "configurehid.h"
#include "hiddmxdevice.h"
#include "hidjsdevice.h"
#include "hidvirtualpad.h"
#include "hidprofile.h"
#include "gamepadlayout.h"
#include "hidprofileeditor.h"
#include "hidapi.h"
#include <QColor>

#include "hidplugin.h"
#include "qlcfile.h"

// HIDPROFILEDIR / USERHIDPROFILEDIR: variables.cmake, as the installer puts
// them (the bundle's Resources/HIDProfiles on macOS, beside the program on
// Windows, share/qlcconsole/hidprofiles on Linux)
#include "qlcconfig.h"

#if defined(Q_WS_X11) || defined(Q_OS_LINUX)
  #include "hidlinuxjoystick.h"
#elif defined(WIN32) || defined (Q_OS_WIN)
  #include "win32/hidwindowsjoystick.h"
#elif defined (__APPLE__) || defined(Q_OS_MACOS)
  #include "hidosxjoystick.h"
  #include "hidosxhotplug.h"
#endif

/*****************************************************************************
 * HID Initialization
 *****************************************************************************/

void HIDPlugin::init()
{
    /* QLC_HID_PROFILE_DIR: the profiles from there alone (tests: the
       source tree's, not whatever this machine's user has). */
    const QString profileDir = qEnvironmentVariable("QLC_HID_PROFILE_DIR");
    if (!profileDir.isEmpty())
    {
        loadProfiles(QDir(profileDir));
    }
    else
    {
        loadProfiles(QLCFile::systemDirectory(QString(HIDPROFILEDIR), QString(".qxhid")));
        loadProfiles(QLCFile::userDirectory(QString(USERHIDPROFILEDIR), QString(HIDPROFILEDIR),
                                            QStringList() << "*.qxhid"));
#if defined (__APPLE__) || defined(Q_OS_MACOS)
        /* Where earlier builds looked: read if it is there, never made */
        loadProfiles(QDir(QDir::homePath() + QStringLiteral("/Library/Application Support/QLC+/HIDProfiles")));
#endif
    }

    m_stickClock.start();

    /* QLC_HID_VIRTUAL_PAD: pads that are not there, read as real ones
       (HIDVirtualPad). QLC_HID_VIRTUAL_ONLY=1: and none that are. */
    for (HIDVirtualPad *pad : HIDVirtualPad::fromEnvironment(this))
    {
        m_virtualPads << pad;
        addDevice(pad);
    }
    m_virtualOnly = qEnvironmentVariableIntValue("QLC_HID_VIRTUAL_ONLY") == 1;

    m_rescanTimer = new QTimer(this);
    m_rescanTimer->setSingleShot(true);
    connect(m_rescanTimer, &QTimer::timeout, this, &HIDPlugin::rescanDevices);

    rescanDevices();

#if defined (__APPLE__) || defined(Q_OS_MACOS)
    /* The app's hotplug monitor sees USB only: a Bluetooth pad switched off
       and on again, or back after the Mac slept, was never picked up again.
       Rescan when one comes or goes -- an IOKit device, or a pad the
       GameController framework connects or disconnects (an Xbox pad's
       auto-off, a Bluetooth drop) -- now and once more a moment later: the
       HID list can still hold a pad that just went, or not yet one that
       just came, and nothing else would ask again. After a wake it takes a
       few seconds to reconnect, so twice. */
    m_osxHotplug = new HIDOSXHotplug([this]() {
                                         scheduleRescan();
                                         QTimer::singleShot(2500, this, [this]() { scheduleRescan(0); });
                                     },
                                     [this]() {
                                         scheduleRescan(1500);
                                         QTimer::singleShot(6000, this, [this]() { scheduleRescan(0); });
                                     });
#endif

    /* And whatever any notification missed (a Mac asleep through it, App
       Nap): a device whose system entry is gone (isStale()) rescans, so a
       controller that went is never left patched and silent. */
    m_staleTimer = new QTimer(this);
    m_staleTimer->setInterval(3000);
    connect(m_staleTimer, &QTimer::timeout, this, [this]() {
        for (HIDDevice *dev : std::as_const(m_devices))
        {
            if (dev->isStale())
            {
                qDebug() << Q_FUNC_INFO << dev->name() << "is gone: rescanning";
                scheduleRescan(0);
                break;
            }
        }
    });
    m_staleTimer->start();

#if defined(Q_OS_LINUX)
    /* The app's hotplug monitor watches the USB subsystem only, so a
       Bluetooth pad coming and going was never seen here either. Its
       joystick node (/dev/input/jsN) appearing or going is. */
    QFileSystemWatcher *inputNodes = new QFileSystemWatcher(this);
    if (inputNodes->addPath(QStringLiteral("/dev/input")))
        connect(inputNodes, &QFileSystemWatcher::directoryChanged,
                this, [this]() { scheduleRescan(); });
#endif
}

void HIDPlugin::scheduleRescan(int ms)
{
    if (m_rescanTimer == nullptr)
        return;
    /* A pending sooner rescan stays sooner; otherwise this one waits for
       the burst (a device's arrival is several callbacks) to settle. */
    if (m_rescanTimer->isActive() && m_rescanTimer->remainingTime() <= ms)
        return;
    m_rescanTimer->start(ms);
}

void HIDPlugin::loadProfiles(const QDir &dir)
{
    if (!dir.exists() || !dir.isReadable())
        return;
    const QStringList files = dir.entryList(QStringList() << "*.qxhid",
                                             QDir::Files | QDir::Readable);
    for (const QString &f : files)
    {
        if (QLCFile::isOnlineOnly(dir.absoluteFilePath(f)))   // in cloud storage, not here: do not wait for it
        {
            QLCFile::noteSkippedOnlineOnly(dir.absoluteFilePath(f));
            continue;
        }
        HIDProfile *p = HIDProfile::load(dir.absoluteFilePath(f));
        if (p)
            m_profiles.append(p);
    }
}

const HIDProfile *HIDPlugin::profileForDevice(const HIDDevice *device) const
{
    if (!device) return nullptr;
    const unsigned short vid = device->vendorId();
    const unsigned short pid = device->productId();
    for (const HIDProfile *p : m_profiles)
        if (p->matchesDevice(vid, pid))
            return p;
    return nullptr;
}

QStringList HIDPlugin::inputDeviceProfileNames(quint32 input) const
{
    Q_UNUSED(input);
    QStringList names;
    names.append(QString()); // empty = "None"
    for (const HIDProfile *p : m_profiles)
        names.append(p->name());
    return names;
}

const HIDProfile *HIDPlugin::profileByName(const QString &name) const
{
    for (const HIDProfile *p : m_profiles)
        if (p->name() == name)
            return p;
    return nullptr;
}

int HIDPlugin::inputAxisForSlot(quint32 input, const QString &slot) const
{
    HIDDevice *dev = device(input);
    if (!dev) return -1;
    const HIDProfile *prof = profileForDevice(dev);
    if (!prof) return -1;
    return prof->axisForSlot(slot);
}

void HIDPlugin::editDeviceProfile(quint32 input, const QString &profileName, QWidget *parent)
{
    Q_UNUSED(input);
    HIDProfile *prof = nullptr;
    for (HIDProfile *p : m_profiles)
        if (p->name() == profileName) { prof = p; break; }
    if (!prof) return;

    HIDProfileEditor dlg(prof, parent);
    dlg.exec();
}

float HIDPlugin::inputProfileSensitivity(quint32 input) const
{
    HIDDevice *dev = device(input);
    if (!dev) return 1.0f;
    const HIDProfile *prof = profileForDevice(dev);
    return prof ? prof->sensitivity() : 1.0f;
}

float HIDPlugin::inputProfileDeadzone(quint32 input) const
{
    HIDDevice *dev = device(input);
    if (!dev) return 0.05f;
    return deviceDeadzone(dev);
}

float HIDPlugin::deviceDeadzone(const HIDDevice *dev) const
{
    if (const HIDProfile *prof = profileForDevice(dev))
        return prof->deadzone();
    /* No profile: a game pad this plugin knows (a Steam Deck) is a modern
       stick; any other joystick -- often an older, uncalibrated one -- gets
       a wider middle (as the tools apply it: 0.08 is +-0.16 of travel). */
    if (dev != nullptr && dev->axisCount() >= 0
        && !GamepadLayout::deviceKind(dev->vendorId(), dev->productId()).isEmpty())
        return 0.05f;
    return 0.08f;
}

QString HIDPlugin::inputButtonAction(quint32 input, quint32 channel) const
{
    HIDDevice *dev = device(input);
    if (!dev) return QString();
    const HIDProfile *prof = profileForDevice(dev);
    if (!prof) return QString();
    const int axes = dev->axisCount();
    if (axes < 0 || (int)channel < axes)
        return QString(); // axis channel, not a button
    return prof->buttonAction((int)channel - axes);
}

QString HIDPlugin::inputChannelName(quint32 input, quint32 channel) const
{
    HIDDevice *dev = device(input);
    if (!dev) return QString();
    const HIDProfile *prof = profileForDevice(dev);
    if (!prof) return QString();

    /* Profiles explicitly list axis indices — check that first so we don't
       depend on axisCount() being correct (IOKit multi-interface matching can
       land on the wrong interface and report 0 axes). */
    const QString axName = prof->axisName((int)channel);
    if (!axName.isEmpty())
        return axName;

    /* Fall back to button name using axisCount as channel offset */
    const int axes = dev->axisCount();
    if (axes >= 0 && (int)channel >= axes)
        return prof->buttonName((int)channel - axes);

    return QString();
}

QString HIDPlugin::inputConnectionInfo(quint32 input) const
{
    HIDDevice *dev = device(input);
    return dev != nullptr ? dev->connectionInfo() : QString();
}

QString HIDPlugin::inputProblem(quint32 input) const
{
    HIDDevice *dev = device(input);
    return dev != nullptr ? dev->inputProblem() : QString();
}

QString HIDPlugin::inputDeviceKind(quint32 input) const
{
    HIDDevice *dev = device(input);
    if (dev == nullptr || dev->axisCount() < 0)    // joysticks only
        return QString();
    return GamepadLayout::deviceKind(dev->vendorId(), dev->productId());
}

bool HIDPlugin::isPatchedInput(quint32 line) const
{
    for (const PluginUniverseDescriptor &d : m_universesMap)
        if (d.inputLine == line)
            return true;
    return false;
}

void HIDPlugin::setupDiagnostics(HIDDevice *dev)
{
    if (dev == nullptr || dev->axisCount() < 0)   // joysticks only
        return;
    connect(dev, &HIDDevice::rawInput, this, &HIDPlugin::slotRawInput, Qt::UniqueConnection);
    dev->setDiagnostics(m_diagnostics > 0);
    if (m_diagnostics > 0 && !dev->isRunning())
    {
        /* Not patched: run it for the test window alone. Its values go no
           further than its sticks' readout (slotDeviceValue()). */
        connect(dev, &HIDDevice::valueChanged, this, &HIDPlugin::slotDeviceValue,
                Qt::UniqueConnection);
        // Patched and in use, it learnt its rest when it was opened.
        StickFilter *f = isPatchedInput(dev->line()) ? nullptr : stickFilter(dev);
        if (f != nullptr)
        {
            f->begin(m_stickClock.elapsed());
            ensureStickTimer();
        }
        dev->openInput();
        m_diagnosticsStarted.insert(dev);
    }
}

bool HIDPlugin::setInputDiagnostics(bool enable)
{
    const bool wasOn = m_diagnostics > 0;
    m_diagnostics = qMax(0, m_diagnostics + (enable ? 1 : -1));
    const bool on = m_diagnostics > 0;
    if (on == wasOn)
        return true;

    if (on)
    {
        for (HIDDevice *dev : std::as_const(m_devices))
            setupDiagnostics(dev);
    }
    else
    {
        for (HIDDevice *dev : std::as_const(m_devices))
            dev->setDiagnostics(false);
        for (HIDDevice *dev : std::as_const(m_diagnosticsStarted))
            if (m_devices.contains(dev) && !isPatchedInput(quint32(m_devices.indexOf(dev))))
                dev->closeInput();
        m_diagnosticsStarted.clear();
    }
    return true;
}

void HIDPlugin::slotRawInput(quint32 line, const QString &element, int raw, int channel, int value)
{
    /* Queued from the device's thread: find it by its line, since it may
       have been replaced since (never dereference the sender). */
    for (int i = 0; i < m_devices.size(); ++i)
    {
        HIDDevice *dev = m_devices.at(i);
        if (dev->line() != line || dev->axisCount() < 0)
            continue;
        emit inputDiagnostic(quint32(i), dev->name(), dev->connectionInfo(),
                             element, raw, channel, value);
        return;
    }
}

QString HIDPlugin::inputChannelRole(quint32 input, quint32 channel) const
{
    HIDDevice *dev = device(input);
    if (!dev) return QString();
    const HIDProfile *prof = profileForDevice(dev);
    if (!prof) return QString();

    // Axes first, by their own index -- as inputChannelName() does.
    if (!prof->axisName((int)channel).isEmpty())
        return prof->axisRole((int)channel);
    const int axes = dev->axisCount();
    if (axes >= 0 && (int)channel >= axes)
        return prof->buttonRole((int)channel - axes);
    return QString();
}

HIDPlugin::~HIDPlugin()
{
#if defined (__APPLE__) || defined(Q_OS_MACOS)
    delete m_osxHotplug;
    m_osxHotplug = nullptr;
#endif
    while (m_devices.isEmpty() == false)
        delete m_devices.takeFirst();
}

QString HIDPlugin::name() const
{
    return QString("HID");
}

int HIDPlugin::capabilities() const
{
    return QLCIOPlugin::Input | QLCIOPlugin::Output;
}

/*****************************************************************************
 * Inputs
 *****************************************************************************/

bool HIDPlugin::openInput(quint32 input, quint32 universe)
{
    HIDDevice* dev = device(input);
    if (dev != NULL)
    {
        connect(dev, &HIDDevice::valueChanged, this, &HIDPlugin::slotDeviceValue,
                Qt::UniqueConnection);
        addToMap(universe, input, Input);
        // Its sticks' rest is learnt from what it reads as it starts.
        if (StickFilter *f = stickFilter(dev))
        {
            f->begin(m_stickClock.elapsed());
            ensureStickTimer();
        }
        return dev->openInput();
    }
    else
        qDebug() << name() << "has no input number:" << input;
    return false;
}

void HIDPlugin::closeInput(quint32 input, quint32 universe)
{
    HIDDevice* dev = device(input);
    if (dev != NULL)
    {
        removeFromMap(universe, input, Input);
        dev->closeInput();
        disconnect(dev, &HIDDevice::valueChanged, this, &HIDPlugin::slotDeviceValue);
    }
    else
        qDebug() << name() << "has no input number:" << input;
}

QStringList HIDPlugin::inputs()
{
    QStringList list;

    QListIterator <HIDDevice*> it(m_devices);
    while (it.hasNext() == true)
    {
        HIDDevice* dev = it.next();
        if (dev->hasInput())
            list << dev->name();
    }

    return list;
}

QString HIDPlugin::pluginInfo() const
{
    QString str;

    str += QString("<HTML>");
    str += QString("<HEAD>");
    str += QString("<TITLE>%1</TITLE>").arg(name());
    str += QString("</HEAD>");
    str += QString("<BODY>");

    str += QString("<P>");
    str += QString("<H3>%1</H3>").arg(name());
    str += tr("This plugin provides support for HID-based joysticks, gamepads and some USB DMX adapters.");
    str += QString("</P>");

    return str;
}

QString HIDPlugin::inputInfo(quint32 input)
{
    QString str;

    if (input != QLCIOPlugin::invalidLine())
    {
        /* A specific input line selected. Display its information if
           available. */
        HIDDevice* dev = device(input);
        if (dev != NULL)
            str += dev->infoText();
    }

    str += QString("</BODY>");
    str += QString("</HTML>");

    return str;
}

/*********************************************************************
 * Outputs
 *********************************************************************/
bool HIDPlugin::openOutput(quint32 output, quint32 universe)
{
    HIDDevice* dev = deviceOutput(output);
    if (dev != NULL)
    {
        addToMap(universe, output, Output);
        return dev->openOutput();
    }
    else
        qDebug() << name() << "has no output number:" << output;
    return false;
}

void HIDPlugin::closeOutput(quint32 output, quint32 universe)
{
    HIDDevice* dev = deviceOutput(output);
    if (dev != NULL)
    {
        removeFromMap(universe, output, Output);
        dev->closeOutput();
    }
    else
        qDebug() << name() << "has no output number:" << output;
}

QStringList HIDPlugin::outputs()
{
    QStringList list;

    QListIterator <HIDDevice*> it(m_devices);
    while (it.hasNext() == true)
    {
        HIDDevice* dev = it.next();
        if (dev->hasOutput())
            list << dev->name();
    }

    return list;
}

QString HIDPlugin::outputInfo(quint32 output)
{
    QString str;

    if (output != QLCIOPlugin::invalidLine())
    {
        /* A specific output line selected. Display its information if
           available. */
        HIDDevice* dev = deviceOutput(output);
        if (dev != NULL)
            str += dev->infoText();
    }

    str += QString("</BODY>");
    str += QString("</HTML>");

    return str;
}

void HIDPlugin::writeUniverse(quint32 universe, quint32 output, const QByteArray &data, bool dataChanged)
{
    Q_UNUSED(universe)
    Q_UNUSED(dataChanged)

    if (output != QLCIOPlugin::invalidLine())
    {
        HIDDevice *dev = deviceOutput(output);
        if (dev != NULL)
            dev->outputDMX(data);
    }
}

/*****************************************************************************
 * Configuration
 *****************************************************************************/

void HIDPlugin::configure()
{
    ConfigureHID conf(NULL, this);
    conf.exec();
}

bool HIDPlugin::canConfigure() const
{
    return true;
}

/*****************************************************************************
 * Devices
 *****************************************************************************/

/** A joystick for @p info on this platform, or null when it is not one. */
static HIDDevice *makeJoystick(HIDPlugin *plugin, quint32 line, struct hid_device_info *info)
{
    HIDDevice *dev = NULL;
#if defined(Q_WS_X11) || defined(Q_OS_LINUX)
    if (QString(info->path).contains("js"))
        dev = new HIDLinuxJoystick(plugin, line, info);
#elif defined(WIN32) || defined (Q_OS_WIN)
    if (HIDWindowsJoystick::isJoystick(info->vendor_id, info->product_id) == true)
        dev = new HIDWindowsJoystick(plugin, line, info);
#elif defined (__APPLE__) || defined(Q_OS_MACOS)
    if (HIDOSXJoystick::isJoystick(info->usage) == true)
        dev = new HIDOSXJoystick(plugin, line, info);
#endif
    /* Matched a joystick-shaped top-level HID usage (why the isJoystick()
       check above passed) but turned out, once its actual report descriptor
       was parsed, to have neither an axis nor a button. Apple's HID stack
       exposes several of its own internal devices (trackpad/keyboard
       auxiliary interfaces, wrapper nodes around the real device) this way,
       typically with no product string either -- just a bare manufacturer
       name and nothing to tell them apart or patch to. Not a real control
       surface, so do not list it as if it might be one. */
    if (dev != NULL && dev->axisCount() <= 0 && dev->buttonCount() <= 0)
    {
        delete dev;
        dev = NULL;
    }
    /* Listed, but already gone from the system (the HID list a moment
       behind a pad switching off): not a device, or the stale check would
       make and drop it on every rescan. */
    if (dev != NULL && dev->isStale())
    {
        delete dev;
        dev = NULL;
    }
    return dev;
}

static bool isDMXInterface(const struct hid_device_info *info)
{
    return (info->vendor_id == HID_DMX_INTERFACE_VENDOR_ID
            && info->product_id == HID_DMX_INTERFACE_PRODUCT_ID) ||
           (info->vendor_id == HID_DMX_INTERFACE_VENDOR_ID_2
            && info->product_id == HID_DMX_INTERFACE_PRODUCT_ID_2) ||
           (info->vendor_id == HID_DMX_INTERFACE_VENDOR_ID_3
            && info->product_id == HID_DMX_INTERFACE_PRODUCT_ID_3) ||
           (info->vendor_id == HID_DMX_INTERFACE_VENDOR_ID_4
            && info->product_id == HID_DMX_INTERFACE_PRODUCT_ID_4);
}

void HIDPlugin::rescanDevices()
{
    /* Treat all devices as dead first, until we find them again. Those
       that aren't found, get destroyed at the end of this function. */
    QList <HIDDevice*> destroyList(m_devices);
    for (HIDVirtualPad *pad : std::as_const(m_virtualPads))
        destroyList.removeAll(pad);     // never found: never gone
    bool changed = false;

    struct hid_device_info *devs, *cur_dev;
    quint32 line = 0;

    devs = m_virtualOnly ? nullptr : hid_enumerate(0x0, 0x0);

    cur_dev = devs;

    while (cur_dev)
    {
        //qDebug() << "[HID Device found] path:" << QString(cur_dev->path) << ", name:" << QString::fromWCharArray(cur_dev->product_string);

        HIDDevice* dev = device(QString(cur_dev->path));
        if (dev != NULL && dev->isStale())
        {
            /* Same path, but not the device it was opened on: a controller
               switched off and on again, or reconnected after the Mac slept.
               The old one reads nothing any more. Replace it where it is, so
               the input line -- and the patch on it -- stays the same; the
               configurationChanged() below reopens it. */
            HIDDevice *fresh = makeJoystick(this, dev->line(), cur_dev);
            /* No device to read there now: it goes with the others not
               found (below), not kept listed, patched and silent. */
            if (fresh != NULL)
            {
                destroyList.removeAll(dev);
                qDebug() << Q_FUNC_INFO << "reconnected:" << fresh->name();
                const int index = m_devices.indexOf(dev);
                m_devices[index] = fresh;
                m_diagnosticsStarted.remove(dev);
                emit deviceRemoved(dev);
                delete dev;
                emit deviceAdded(fresh);
                setupDiagnostics(fresh);
                changed = true;
            }
        }
        else if (dev != NULL)
        {
            /** Device already exists, delete from remove list */
            destroyList.removeAll(dev);
        }
        else if (isDMXInterface(cur_dev))
        {
            /* Device is a USB DMX Interface, add it */
            dev = new HIDDMXDevice(this, line++,
                                   QString::fromWCharArray(cur_dev->manufacturer_string) + " " +
                                   QString::fromWCharArray(cur_dev->product_string) + " " +
                                   "(" + QString::fromWCharArray(cur_dev->serial_number) + ")",
                                   QString(cur_dev->path));
            addDevice(dev);
        }
        else
        {
            dev = makeJoystick(this, line, cur_dev);
            if (dev != NULL)
            {
                line++;
                addDevice(dev);
                setupDiagnostics(dev);
            }
        }

        cur_dev = cur_dev->next;
    }

    if (devs != nullptr)
        hid_free_enumeration(devs);

    /* Destroy those devices that were no longer found. */
    while (destroyList.isEmpty() == false)
    {
        HIDDevice* dev = destroyList.takeFirst();
        m_devices.removeAll(dev);
        m_diagnosticsStarted.remove(dev);
        delete dev;
        changed = true;
    }

    /* An input line is a device's place in this list (device(index)), and
       InputPatch only takes values whose line matches the one it opened.
       Numbering devices as they were found did not keep to that: a device
       skipped above (Apple's internal ones) still used a number up, and a
       rescan numbered new devices from 0 again -- so a controller found
       after them, or back after being switched off, sent its input under a
       line no patch listens to. Keep them in step. */
    for (int i = 0; i < m_devices.size(); ++i)
        m_devices.at(i)->setLine(quint32(i));

    if (changed)
        emit configurationChanged();
}

bool HIDPlugin::injectVirtualInput(quint32 input, quint32 channel, uint value)
{
    HIDVirtualPad *pad = qobject_cast<HIDVirtualPad *>(device(input));
    if (pad == nullptr || !m_virtualPads.contains(pad))
        return false;
    pad->inject(channel, uchar(qMin(value, 255u)));
    return true;
}

HIDDevice* HIDPlugin::device(const QString& path) const
{
    QListIterator <HIDDevice*> it(m_devices);

    while (it.hasNext() == true)
    {
        HIDDevice* dev = it.next();
        if (dev->path() == path)
            return dev;
    }

    return NULL;
}

void HIDPlugin::sendFeedBack(quint32 universe, quint32 inputLine, quint32 channel, uchar value,
                             const QVariant &params)
{
    Q_UNUSED(universe)
    Q_UNUSED(channel)
    Q_UNUSED(value)
    const QVariantMap p = params.toMap();
    const QString what = p.value(QStringLiteral("gamepad")).toString();
    if (what.isEmpty())
        return;
#if defined(Q_OS_MACOS)
    HIDOSXJoystick *pad = qobject_cast<HIDOSXJoystick *>(device(inputLine));
    if (pad == nullptr)
        return;
    if (what == QLatin1String("light"))
    {
        // Its own colour back: what a DualShock shows by itself, blue.
        const QColor c(p.value(QStringLiteral("rgb")).toString());
        const QColor show = c.isValid() ? c : QColor(0, 0, 255);
        pad->setPadLight(float(show.redF()), float(show.greenF()), float(show.blueF()));
    }
    else if (what == QLatin1String("triggers"))
    {
        // Q199: "off" / "firm" / "click", to the DualSense's adaptive triggers.
        auto feel = [&p](const char *key) {
            const QByteArray n = p.value(QLatin1String(key)).toString().toLatin1();
            return n == "firm" ? 1 : n == "click" ? 2 : 0;
        };
        pad->setPadTriggers(feel("right"), feel("left"));
    }
    else if (what == QLatin1String("rumble"))
        pad->padRumble(p.value(QStringLiteral("pulses")).toInt(),
                       p.value(QStringLiteral("ms"), 120).toInt());
#else
    Q_UNUSED(inputLine)
#endif
}

HIDDevice* HIDPlugin::device(quint32 index) const
{
    if (index < quint32(m_devices.count()))
        return m_devices.at(index);
    else
        return NULL;
}

HIDDevice* HIDPlugin::deviceOutput(quint32 index) const
{
    QListIterator <HIDDevice*> it(m_devices);
    quint32 pos = 0;
    while (it.hasNext() == true)
    {
        HIDDevice* dev = it.next();
        if (dev->hasOutput())
        {
            if (pos == index)
                return dev;
            else
                pos++;
        }
    }
    return NULL;
}

void HIDPlugin::addDevice(HIDDevice* device)
{
    Q_ASSERT(device != NULL);

    device->setLine(quint32(m_devices.count()));
    m_devices.append(device);
    emit deviceAdded(device);

    emit configurationChanged();
}

void HIDPlugin::removeDevice(HIDDevice* device)
{
    Q_ASSERT(device != NULL);

    m_devices.removeAll(device);

    emit deviceRemoved(device);
    delete device;

    emit configurationChanged();
}

/*****************************************************************************
 * Hotplug
 *****************************************************************************/

void HIDPlugin::slotDeviceAdded(uint vid, uint pid)
{
    qDebug() << Q_FUNC_INFO << QString::number(vid, 16) << QString::number(pid, 16);

    rescanDevices();
}

void HIDPlugin::slotDeviceRemoved(uint vid, uint pid)
{
    qDebug() << Q_FUNC_INFO << QString::number(vid, 16) << QString::number(pid, 16);

    rescanDevices();
}

/*****************************************************************************
 * Stick rest
 *****************************************************************************/

namespace
{
/** Where learnt rests are kept: the app's settings -- but in a test program
 *  (QLC_TEST_OFFSCREEN_GUARD) nowhere, unless QLC_HID_STICK_SETTINGS names
 *  a file of its own, so a test never writes the user's settings. */
std::unique_ptr<QSettings> restSettings()
{
    const QString file = qEnvironmentVariable("QLC_HID_STICK_SETTINGS");
    if (!file.isEmpty())
        return std::make_unique<QSettings>(file, QSettings::IniFormat);
    if (qEnvironmentVariableIntValue("QLC_TEST_OFFSCREEN_GUARD") == 1)
        return nullptr;
    return std::make_unique<QSettings>();
}

QString restSettingsKey(const QString &stickKey)
{
    QString k = stickKey;
    for (QChar &c : k)
        if (!c.isLetterOrNumber())
            c = QLatin1Char('_');
    return QStringLiteral("hid/stickRest/") + k;
}
}

QString HIDPlugin::stickKey(const HIDDevice *dev)
{
    return QStringLiteral("%1:%2:%3").arg(dev->vendorId(), 4, 16, QLatin1Char('0'))
                                     .arg(dev->productId(), 4, 16, QLatin1Char('0'))
                                     .arg(dev->name().trimmed());
}

QList<int> HIDPlugin::stickAxes(const HIDDevice *dev) const
{
    QList<int> axes;
    // Its profile's stick roles (and pan / tilt slots): never a trigger.
    if (const HIDProfile *prof = profileForDevice(dev))
    {
        for (int i = 0; i < 64; ++i)
        {
            const QString role = prof->axisRole(i);
            if (role == QLatin1String("lx") || role == QLatin1String("ly")
                || role == QLatin1String("rx") || role == QLatin1String("ry")
                || !prof->axisSlot(i).isEmpty())
                axes << i;
        }
        if (!axes.isEmpty())
            return axes;
    }
    // A pad read into the Xbox layout (a Steam Deck, a pad with no profile).
    if (!GamepadLayout::deviceKind(dev->vendorId(), dev->productId()).isEmpty())
        return { GamepadLayout::LeftX, GamepadLayout::LeftY,
                 GamepadLayout::RightX, GamepadLayout::RightY };
    /* Any other joystick: X and Y, its first two axes. The rest may be a
       throttle or a slider, which has no middle to come back to. */
    if (dev->axisCount() >= 2)
        return { 0, 1 };
    return axes;
}

StickFilter *HIDPlugin::stickFilter(const HIDDevice *dev)
{
    if (dev == nullptr || dev->axisCount() < 0)
        return nullptr;
    const QString key = stickKey(dev);
    auto it = m_sticks.find(key);
    if (it == m_sticks.end())
    {
        StickFilter f;
        f.setAxes(stickAxes(dev));
        // QLC_HID_STICK_DRIFT_MS: a test's shorter wait before drift is said.
        const int driftMs = qEnvironmentVariableIntValue("QLC_HID_STICK_DRIFT_MS");
        if (driftMs > 0)
            f.setDriftTime(driftMs);
        if (f.axes().isEmpty())
            return nullptr;
        // As it rested last time (Centre Sticks, or learnt as it arrived).
        if (std::unique_ptr<QSettings> s = restSettings())
        {
            const QStringList saved = s->value(restSettingsKey(key)).toStringList();
            for (const QString &one : saved)
            {
                const int eq = one.indexOf(QLatin1Char('='));
                bool okC = false, okR = false;
                const int ch = one.left(eq).toInt(&okC);
                const double rest = one.mid(eq + 1).toDouble(&okR);
                if (eq > 0 && okC && okR)
                    f.setRest(ch, rest);
            }
        }
        it = m_sticks.insert(key, f);
    }
    // The profile may have been edited since.
    it->setDeadzone(deviceDeadzone(dev));
    return &it.value();
}

void HIDPlugin::rememberRests(const QString &key, const StickFilter &f) const
{
    std::unique_ptr<QSettings> s = restSettings();
    if (!s)
        return;
    QStringList out;
    const QMap<int, double> rests = f.rests();
    for (auto it = rests.constBegin(); it != rests.constEnd(); ++it)
        out << QStringLiteral("%1=%2").arg(it.key()).arg(it.value(), 0, 'f', 4);
    s->setValue(restSettingsKey(key), out);
}

void HIDPlugin::ensureStickTimer()
{
    if (m_stickTimer == nullptr)
    {
        m_stickTimer = new QTimer(this);
        m_stickTimer->setInterval(100);
        connect(m_stickTimer, &QTimer::timeout, this, &HIDPlugin::slotStickTick);
    }
    if (!m_stickTimer->isActive())
        m_stickTimer->start();
}

void HIDPlugin::slotDeviceValue(quint32 universe, quint32 line, quint32 channel, uchar value)
{
    /* Queued from the device's thread: found by its line, never the
       sender (it may have been replaced since). */
    HIDDevice *dev = device(line);
    if (dev != nullptr && dev->line() == line)
    {
        if (StickFilter *f = stickFilter(dev))
        {
            if (f->isAxis(int(channel)))
            {
                value = f->feed(int(channel), value, m_stickClock.elapsed());
                if (f->needsTick())
                    ensureStickTimer();
            }
            else
                f->feed(int(channel), value, m_stickClock.elapsed());
        }
    }
    if (isPatchedInput(line))
        emit valueChanged(universe, line, channel, value);
}

void HIDPlugin::slotStickTick()
{
    const qint64 now = m_stickClock.elapsed();
    bool busy = false;
    for (int i = 0; i < m_devices.size(); ++i)
    {
        HIDDevice *dev = m_devices.at(i);
        if (dev->axisCount() < 0)
            continue;
        const QString key = stickKey(dev);
        auto it = m_sticks.find(key);
        if (it == m_sticks.end())
            continue;
        StickFilter &f = it.value();
        const StickFilter::Tick t = f.tick(now);
        const quint32 line = quint32(i);
        if (!t.learnt.isEmpty() || t.captureDone)
            rememberRests(key, f);
        // A rest learnt changes what the stick reads: say it again.
        if (isPatchedInput(line))
            for (int ch : t.learnt)
                emit valueChanged(UINT_MAX, line, quint32(ch), f.output(ch));
        if (t.captureDone)
            emit inputSticksCentred(line, t.held);
        if (t.drifted >= 0)
            emit inputStickDrift(line, t.drifted, t.driftOffset);
        busy = busy || f.needsTick();
    }
    if (!busy && m_stickTimer != nullptr)
        m_stickTimer->stop();
}

QVariantList HIDPlugin::inputStickAxes(quint32 input) const
{
    QVariantList out;
    HIDDevice *dev = device(input);
    if (dev == nullptr)
        return out;
    const StickFilter *f = const_cast<HIDPlugin *>(this)->stickFilter(dev);
    if (f == nullptr)
        return out;
    for (int ch : f->axes())
    {
        QVariantMap m;
        m.insert(QStringLiteral("channel"), ch);
        m.insert(QStringLiteral("raw"), f->raw(ch));
        m.insert(QStringLiteral("rest"), f->rest(ch));
        m.insert(QStringLiteral("learnt"), f->isLearnt(ch));
        m.insert(QStringLiteral("output"), int(f->output(ch)));
        m.insert(QStringLiteral("deadzone"), f->deadzone());
        out << m;
    }
    return out;
}

bool HIDPlugin::centreInputSticks(quint32 input)
{
    HIDDevice *dev = device(input);
    StickFilter *f = stickFilter(dev);
    if (f == nullptr)
        return false;
    f->requestCapture(m_stickClock.elapsed());
    ensureStickTimer();
    return true;
}

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
#include <QMessageBox>
#include <QStringList>
#include <QTimer>
#include <QDebug>
#include <QDir>

#include "configurehid.h"
#include "hiddmxdevice.h"
#include "hidjsdevice.h"
#include "hidprofile.h"
#include "hidprofileeditor.h"
#include "hidapi.h"
#include "hidplugin.h"
#include "qlcfile.h"

// macOS bundle layout: mirrors RGBScripts / EffectScripts
#define HIDPROFILEDIR     "Resources/HIDProfiles"
#define USERHIDPROFILEDIR "Library/Application Support/QLC+/HIDProfiles"

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
    loadProfiles(QLCFile::systemDirectory(QString(HIDPROFILEDIR), QString(".qxhid")));
    loadProfiles(QLCFile::userDirectory(QString(USERHIDPROFILEDIR), QString(HIDPROFILEDIR),
                                        QStringList() << "*.qxhid"));

    m_rescanTimer = new QTimer(this);
    m_rescanTimer->setSingleShot(true);
    connect(m_rescanTimer, &QTimer::timeout, this, &HIDPlugin::rescanDevices);

    rescanDevices();

#if defined (__APPLE__) || defined(Q_OS_MACOS)
    /* The app's hotplug monitor sees USB only: a Bluetooth pad switched off
       and on again, or back after the Mac slept, was never picked up again.
       Rescan when one comes or goes, and after a wake -- when it takes a
       few seconds to reconnect, so twice. */
    m_osxHotplug = new HIDOSXHotplug([this]() { scheduleRescan(); },
                                     [this]() {
                                         scheduleRescan(1500);
                                         QTimer::singleShot(6000, this, [this]() { scheduleRescan(0); });
                                     });
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
    const HIDProfile *prof = profileForDevice(dev);
    return prof ? prof->deadzone() : 0.05f;
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
        /* Not patched: run it for the test window alone. Its values go
           nowhere -- nothing is connected to them. */
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
        connect(dev, SIGNAL(valueChanged(quint32,quint32,quint32,uchar)),
                this, SIGNAL(valueChanged(quint32,quint32,quint32,uchar)));
        addToMap(universe, input, Input);
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
        removeFromMap(input, universe, Input);
        dev->closeInput();
        disconnect(dev, SIGNAL(valueChanged(quint32,quint32,quint32,uchar)),
                   this, SIGNAL(valueChanged(quint32,quint32,quint32,uchar)));
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
        removeFromMap(output, universe, Output);
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
    bool changed = false;

    struct hid_device_info *devs, *cur_dev;
    quint32 line = 0;

    devs = hid_enumerate(0x0, 0x0);

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
            destroyList.removeAll(dev);
            HIDDevice *fresh = makeJoystick(this, dev->line(), cur_dev);
            if (fresh != NULL)
            {
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

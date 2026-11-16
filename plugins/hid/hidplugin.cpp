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
#include <QDeadlineTimer>
#include <QFileSystemWatcher>
#include <QMutex>
#include <QSettings>
#include <QWaitCondition>
#include <atomic>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <QMessageBox>
#include <QPair>
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
#include "hidbackend.h"
#include "hidtrace.h"
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
 * The scan and open threads
 *****************************************************************************/

/* What the GUI thread and the HID threads share; each thread holds it, so
   it outlives the plugin while one is still in a call to the system. The
   lock is only ever held for bookkeeping, never across a call to the
   system (or the backend). */
struct HIDScanShared
{
    QMutex lock;
    QWaitCondition changed;
    HIDPlugin *owner = nullptr;         ///< null once the plugin has gone
    QThread *guiThread = nullptr;       ///< where finished devices go
    bool quit = false;

    // The scan thread: one, made at the first scan, kept.
    bool scannerStarted = false;
    bool scanPending = false;
    bool scanBusy = false;
    /* The stale check between scans: the devices' staleProbe()s alone,
       run here so the GUI thread makes no call to the system for it. */
    bool probePending = false;
    bool staleSeen = false;
    HIDBackend *scanBackend = nullptr;
    QList<QPair<QString, std::function<bool()>>> staleProbes;
    QSet<QString> knownPaths;

    // Finished, for drainResults().
    struct Scan
    {
        QList<HIDFound> found;
        QSet<QString> stale;
        QList<HIDDevice *> dmx;
    };
    std::optional<Scan> scan;
    struct Opened
    {
        QString path;
        quint64 job = 0;
        HIDDevice *dev = nullptr;
    };
    QList<Opened> opened;
    int opening = 0;                    ///< open threads still in their call

    /** The system's backend: here, not in the plugin, so a thread stuck in
     *  it still has it after the plugin has gone. */
    std::unique_ptr<HIDBackend> systemBackend;

    ~HIDScanShared()
    {
        if (scan)
            qDeleteAll(scan->dmx);
        for (const Opened &o : std::as_const(opened))
            delete o.dev;
    }

    /** Tell the plugin there is something to take. Lock held. */
    void ping()
    {
        if (owner != nullptr)
            QMetaObject::invokeMethod(owner, "drainResults", Qt::QueuedConnection);
        changed.wakeAll();
    }
};

namespace
{
/** Every HID worker thread still running (scan, open, dispose), for
 *  HIDPlugin::workerThreads(). */
std::atomic<int> s_workers { 0 };

/** Counts a worker thread out as it ends, however it ends. */
struct WorkerCount
{
    ~WorkerCount() { s_workers--; }
};

/** A device that is out of the plugin's list: deleted off the GUI thread,
 *  as stopping its reader and closing it are calls to the system too. */
void disposeOffThread(HIDDevice *dev)
{
    if (dev == nullptr)
        return;
    s_workers++;
    std::thread([dev]() {
        WorkerCount counted;
        delete dev;
    }).detach();
}

HIDDevice *makeVirtualPad(const HIDFound &found)
{
    const QString name = found.product.isEmpty()
                         ? QStringLiteral("Virtual pad %1").arg(found.path) : found.product;
    HIDVirtualPad *pad = new HIDVirtualPad(nullptr, 0, name, found.vendorId, found.productId);
    pad->setPath(found.path);
    /* The port the system path says, as a real pad's location (only a test
       backend's pads are made here), so their controller-jobs identity is
       port-based as a real pad's is. */
    pad->setLocation(GamepadLayout::locationFromPath(found.path));
    if (found.testRead)
        pad->setReadHook(found.testRead);
    return pad;
}

/** The scan thread: one scan at a time, as asked, until the plugin goes. */
void scanLoop(std::shared_ptr<HIDScanShared> sh)
{
    WorkerCount counted;
    QMutexLocker locker(&sh->lock);
    for (;;)
    {
        while (sh->scanPending == false && sh->probePending == false && sh->quit == false)
            sh->changed.wait(&sh->lock);
        if (sh->quit)
        {
            locker.unlock();
            sh.reset();     // before it counts itself out
            return;
        }
        if (sh->scanPending == false)
        {
            /* The stale check alone: is each device's system entry still
               there? One gone asks the plugin for a scan. */
            sh->probePending = false;
            const auto probes = sh->staleProbes;
            locker.unlock();
            bool gone = false;
            for (const auto &probe : probes)
                gone = gone || (probe.second && probe.second());
            locker.relock();
            if (gone)
            {
                sh->staleSeen = true;
                sh->ping();
            }
            continue;
        }
        sh->probePending = false;   // the scan checks them too
        sh->scanPending = false;
        sh->scanBusy = true;
        HIDBackend *backend = sh->scanBackend;
        const auto probes = sh->staleProbes;
        const QSet<QString> known = sh->knownPaths;
        QThread *gui = sh->guiThread;
        locker.unlock();

        HIDScanShared::Scan scan;
        if (backend != nullptr)
            scan.found = backend->enumerate();
        for (const auto &probe : probes)
            if (probe.second && probe.second())
                scan.stale.insert(probe.first);
        /* A DMX interface is opened here, one at a time: hidapi's own
           handle, which shares its listing's state. */
        for (const HIDFound &f : std::as_const(scan.found))
        {
            if (f.dmx == false || known.contains(f.path) || backend == nullptr)
                continue;
            if (HIDDevice *dev = backend->open(f, makeVirtualPad))
            {
                dev->handTo(gui);
                scan.dmx << dev;
            }
        }

        locker.relock();
        sh->scanBusy = false;
        if (sh->scan)
        {
            // Not taken yet: the newer listing wins, its DMX devices join.
            scan.dmx = sh->scan->dmx + scan.dmx;
        }
        sh->scan = std::move(scan);
        sh->ping();
    }
}

/** An open thread: one device, as long as the system takes. */
void openJob(std::shared_ptr<HIDScanShared> sh, HIDBackend *backend, HIDFound found, quint64 job)
{
    WorkerCount counted;
    HIDDevice *dev = backend->open(found, makeVirtualPad);
    if (dev != nullptr)
        dev->setInstance(found.instance);

    QMutexLocker locker(&sh->lock);
    sh->opening--;
    if (sh->owner != nullptr)
    {
        if (dev != nullptr)
            dev->handTo(sh->guiThread);
        sh->opened << HIDScanShared::Opened { found.path, job, dev };
        dev = nullptr;
    }
    sh->ping();
    locker.unlock();
    delete dev;     // the plugin has gone: nobody to give it to
    sh.reset();
}
}

/*****************************************************************************
 * HID Initialization
 *****************************************************************************/

void HIDPlugin::init()
{
    if (m_initDone)
        return;     // once: a second call would list the virtual pads and the recorder again
    m_initDone = true;

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

    /* QLC_HID_RECORD=<dir>: a trace of this session (hidtrace.h). */
    const QString recordDir = qEnvironmentVariable("QLC_HID_RECORD");
    if (!recordDir.isEmpty())
    {
        m_trace = HIDTrace::create(QDir::fromNativeSeparators(recordDir.startsWith(QLatin1String("~/"))
            ? QDir::homePath() + recordDir.mid(1) : recordDir));
        if (m_trace == nullptr)
            qWarning() << "QLC_HID_RECORD:" << recordDir << "cannot be written";
    }

    /* QLC_HID_VIRTUAL_PAD: pads that are not there, read as real ones
       (HIDVirtualPad). QLC_HID_VIRTUAL_ONLY=1: and none that are. */
    for (HIDVirtualPad *pad : HIDVirtualPad::fromEnvironment(this))
    {
        m_virtualPads << pad;
        addDevice(pad);
    }
    m_virtualOnly = qEnvironmentVariableIntValue("QLC_HID_VIRTUAL_ONLY") == 1;
    const int limit = qEnvironmentVariableIntValue("QLC_HID_OPEN_LIMIT_MS");
    if (limit > 0)
        m_openLimitMs = limit;

    m_rescanTimer = new QTimer(this);
    m_rescanTimer->setSingleShot(true);
    connect(m_rescanTimer, &QTimer::timeout, this, &HIDPlugin::rescanDevices);

    /* A scan that does not come back in time: the system's list itself is
       not answering. Named, and the next one waits for it (never two). */
    m_scanWatch = new QTimer(this);
    m_scanWatch->setSingleShot(true);
    connect(m_scanWatch, &QTimer::timeout, this, [this]() {
        if (isScanning() && !m_scanStuck)
        {
            qWarning() << Q_FUNC_INFO << "the HID device list has not answered in"
                       << m_openLimitMs << "ms";
            m_scanStuck = true;
            noteProblems();
        }
    });

    /* The first scan, waited for a moment so a show's patches find the
       pads that are there; one that hangs is not waited for. */
    rescanDevices();
    settleScan(m_openLimitMs);

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
    connect(m_staleTimer, &QTimer::timeout, this, &HIDPlugin::checkStale);
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

QString HIDPlugin::inputDeviceLocation(quint32 input) const
{
    HIDDevice *dev = device(input);
    if (dev == nullptr || dev->axisCount() < 0)    // joysticks only
        return QString();
    return dev->location();
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
    /* Threads still in a call to the system are left to it: they see the
       plugin gone and delete what they made themselves. Nothing here waits
       for them -- but one that never returns may keep the process from
       finishing its exit until the computer restarts. Said in the log. */
    if (openingCount() > 0)
        qWarning() << Q_FUNC_INFO << openingCount() << "controller open(s) still stuck in the system:"
                   << "the app may not finish quitting until this computer restarts";
    if (m_shared)
    {
        QMutexLocker locker(&m_shared->lock);
        m_shared->owner = nullptr;
        m_shared->quit = true;
        m_shared->changed.wakeAll();
    }
#if defined (__APPLE__) || defined(Q_OS_MACOS)
    delete m_osxHotplug;
    m_osxHotplug = nullptr;
#endif
    while (m_devices.isEmpty() == false)
    {
        HIDDevice *dev = m_devices.takeFirst();
        /* A reader stuck in a call cannot be deleted under it: left, as
           the process is closing anyway. */
        if (dev->stopReading(500))
            delete dev;
        else
            qWarning() << Q_FUNC_INFO << dev->name() << "is still in a call: left";
    }
    delete m_trace;
    m_trace = nullptr;
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
static HIDDevice *makeJoystick(struct hid_device_info *info)
{
    HIDPlugin *plugin = nullptr;    // made off the GUI thread: no parent
    const quint32 line = 0;         // HIDPlugin numbers it as it adds it
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

/** The system's devices: hidapi's listing, each platform's joystick. */
class HIDSystemBackend final : public HIDBackend
{
public:
    QList<HIDFound> enumerate() override
    {
        QList<HIDFound> list;
        struct hid_device_info *devs = hid_enumerate(0x0, 0x0);
        for (struct hid_device_info *cur = devs; cur != nullptr; cur = cur->next)
        {
            HIDFound f;
            f.path = QString(cur->path);
            f.vendorId = cur->vendor_id;
            f.productId = cur->product_id;
            f.usagePage = cur->usage_page;
            f.usage = cur->usage;
            f.releaseNumber = cur->release_number;
            f.interfaceNumber = cur->interface_number;
            f.manufacturer = cur->manufacturer_string ? QString::fromWCharArray(cur->manufacturer_string) : QString();
            f.product = cur->product_string ? QString::fromWCharArray(cur->product_string) : QString();
            f.serial = cur->serial_number ? QString::fromWCharArray(cur->serial_number) : QString();
            f.dmx = isDMXInterface(cur);
            if (f.dmx || isJoystickCandidate(cur))
                list << f;
        }
        if (devs != nullptr)
            hid_free_enumeration(devs);
        return list;
    }

    HIDDevice *open(const HIDFound &f,
                    const std::function<HIDDevice *(const HIDFound &)> &) override
    {
        if (f.dmx)
            return new HIDDMXDevice(nullptr, 0, f.manufacturer + " " + f.product + " "
                                                + "(" + f.serial + ")", f.path);
        /* A listing entry again, its strings owned here for the call. */
        QByteArray path = f.path.toUtf8();
        std::wstring manufacturer = f.manufacturer.toStdWString();
        std::wstring product = f.product.toStdWString();
        std::wstring serial = f.serial.toStdWString();
        struct hid_device_info info;
        memset(&info, 0, sizeof(info));
        info.path = path.data();
        info.vendor_id = f.vendorId;
        info.product_id = f.productId;
        info.usage_page = f.usagePage;
        info.usage = f.usage;
        info.release_number = f.releaseNumber;
        info.interface_number = f.interfaceNumber;
        info.manufacturer_string = manufacturer.data();
        info.product_string = product.data();
        info.serial_number = serial.data();
        info.next = nullptr;
        return makeJoystick(&info);
    }

private:
    static bool isJoystickCandidate(const struct hid_device_info *info)
    {
#if defined(Q_WS_X11) || defined(Q_OS_LINUX)
        return QString(info->path).contains("js");
#elif defined(WIN32) || defined (Q_OS_WIN)
        return HIDWindowsJoystick::isJoystick(info->vendor_id, info->product_id);
#elif defined (__APPLE__) || defined(Q_OS_MACOS)
        return HIDOSXJoystick::isJoystick(info->usage);
#else
        Q_UNUSED(info)
        return false;
#endif
    }
};

HIDBackend *HIDPlugin::backend() const
{
    if (m_testBackend != nullptr)
        return m_testBackend;
    if (m_virtualOnly || !m_shared)
        return nullptr;
    return m_shared->systemBackend.get();
}

void HIDPlugin::setTestBackend(quintptr backend)
{
    m_testBackend = reinterpret_cast<HIDBackend *>(backend);
    rescanDevices();
}

int HIDPlugin::openingCount() const
{
    if (!m_shared)
        return 0;
    QMutexLocker locker(&m_shared->lock);
    return m_shared->opening;
}

bool HIDPlugin::isScanning() const
{
    if (!m_shared)
        return false;
    QMutexLocker locker(&m_shared->lock);
    return m_shared->scanPending || m_shared->scanBusy;
}

void HIDPlugin::rescanDevices()
{
    if (!m_shared)
    {
        m_shared = std::make_shared<HIDScanShared>();
        m_shared->owner = this;
        m_shared->guiThread = thread();
        m_shared->systemBackend = std::make_unique<HIDSystemBackend>();
    }
    HIDBackend *be = backend();
    if (be == nullptr)
        return;

    QSet<QString> known;
    for (HIDDevice *dev : std::as_const(m_devices))
        known.insert(dev->path());

    QMutexLocker locker(&m_shared->lock);
    /* Asked while one runs: that one is followed by this one, never a
       second beside it. */
    m_shared->scanPending = true;
    m_shared->scanBackend = be;
    m_shared->staleProbes = staleProbes();
    m_shared->knownPaths = known;
    startScanner();
    m_shared->changed.wakeAll();
    locker.unlock();

    if (m_scanWatch != nullptr && m_scanWatch->isActive() == false)
        m_scanWatch->start(m_openLimitMs);
}

QList<QPair<QString, std::function<bool()>>> HIDPlugin::staleProbes() const
{
    /* What the scan thread checks without touching a device: whether each
       one's system entry is still there (staleProbe()). */
    QList<QPair<QString, std::function<bool()>>> probes;
    for (HIDDevice *dev : std::as_const(m_devices))
        if (std::function<bool()> probe = dev->staleProbe())
            probes << qMakePair(dev->path(), probe);
    return probes;
}

void HIDPlugin::startScanner()
{
    if (m_shared->scannerStarted)
        return;
    m_shared->scannerStarted = true;
    s_workers++;
    std::thread(scanLoop, m_shared).detach();
}

void HIDPlugin::checkStale()
{
    /* Whatever a notification missed (a Mac asleep through it, App Nap):
       a device whose system entry is gone rescans, so a controller that
       went is never left patched and silent. Asked of the scan thread --
       the GUI thread makes no call to the system -- and not while a scan
       (which checks them too) is asked for or under way. */
    if (!m_shared)
        return;
    const auto probes = staleProbes();
    if (probes.isEmpty())
        return;
    QMutexLocker locker(&m_shared->lock);
    if (m_shared->scanPending || m_shared->scanBusy || m_shared->probePending)
        return;
    m_shared->probePending = true;
    m_shared->staleProbes = probes;
    startScanner();
    m_shared->changed.wakeAll();
}

int HIDPlugin::workerThreads()
{
    return s_workers.load();
}

void HIDPlugin::settleScan(int ms)
{
    if (!m_shared)
        return;
    QDeadlineTimer deadline(ms);
    for (;;)
    {
        drainResults();
        QMutexLocker locker(&m_shared->lock);
        if (m_shared->scan || m_shared->opened.isEmpty() == false)
            continue;
        if (m_shared->scanPending == false && m_shared->scanBusy == false
            && m_shared->opening == 0)
            break;
        if (m_shared->changed.wait(&m_shared->lock, deadline) == false)
            break;
    }
}

void HIDPlugin::drainResults()
{
    if (!m_shared)
        return;
    std::optional<HIDScanShared::Scan> scan;
    QList<HIDScanShared::Opened> opened;
    bool staleSeen = false;
    {
        QMutexLocker locker(&m_shared->lock);
        scan.swap(m_shared->scan);
        opened.swap(m_shared->opened);
        staleSeen = m_shared->staleSeen;
        m_shared->staleSeen = false;
    }
    if (staleSeen)
    {
        qDebug() << Q_FUNC_INFO << "a device's system entry is gone: rescanning";
        scheduleRescan(0);
    }
    if (scan)
        applyScan(scan->found, scan->stale, scan->dmx);
    for (const HIDScanShared::Opened &o : std::as_const(opened))
        applyOpened(o.path, o.job, o.dev);
}

void HIDPlugin::dropDevice(HIDDevice *dev)
{
    if (m_trace != nullptr && dev->axisCount() >= 0)
        m_trace->gone(dev);
    m_devices.removeAll(dev);
    m_deviceIds.remove(dev);
    m_diagnosticsStarted.remove(dev);
    emit deviceRemoved(dev);
    disposeOffThread(dev);
}

void HIDPlugin::announceLines()
{
    /* Each line's device now, by an id never reused (a pointer can be, to
       a device made where a dropped one was). */
    QList<quint64> now;
    now.reserve(m_devices.size());
    for (HIDDevice *dev : std::as_const(m_devices))
    {
        auto it = m_deviceIds.find(dev);
        if (it == m_deviceIds.end())
            it = m_deviceIds.insert(dev, ++m_nextDeviceId);
        now << it.value();
    }
    /* A line that had a device and has another now -- replaced where it
       was, or another shifted onto it when one before it went (a Linux
       pad with no location: the patch on the line stays) -- is told:
       what was held on it is not known to be held (third review of Q253).
       A line new at the end, or one gone, is not: the patch follows those. */
    QList<quint32> changed;
    for (int i = 0; i < qMin(m_lineIds.size(), now.size()); ++i)
        if (m_lineIds.at(i) != now.at(i))
            changed << quint32(i);
    m_lineIds = now;
    for (quint32 line : std::as_const(changed))
        emit inputReopened(line);
}

void HIDPlugin::renumberLines()
{
    /* An input line is a device's place in this list (device(index)), and
       InputPatch only takes values whose line matches the one it opened.
       Numbering devices as they were found did not keep to that: a device
       skipped (Apple's internal ones) still used a number up, and a rescan
       numbered new devices from 0 again -- so a controller found after
       them, or back after being switched off, sent its input under a line
       no patch listens to. Keep them in step. */
    for (int i = 0; i < m_devices.size(); ++i)
        m_devices.at(i)->setLine(quint32(i));
}

void HIDPlugin::applyScan(const QList<HIDFound> &found, const QSet<QString> &stale,
                          const QList<HIDDevice *> &dmx)
{
    if (m_scanWatch != nullptr)
        m_scanWatch->stop();
    m_scanStuck = false;

    /* The same device listed twice is one device. */
    QList<HIDFound> unique;
    QSet<QString> listed;
    for (const HIDFound &f : found)
    {
        if (listed.contains(f.path))
            continue;
        listed.insert(f.path);
        unique << f;
    }
    m_listed = listed;
    bool changed = false;

    /* Gone from the list: gone. */
    const QList<HIDDevice *> devices = m_devices;
    for (HIDDevice *dev : devices)
    {
        if (m_virtualPads.contains(qobject_cast<HIDVirtualPad *>(dev)))
            continue;   // never found: never gone
        if (listed.contains(dev->path()) == false)
        {
            dropDevice(dev);
            changed = true;
        }
    }

    /* USB DMX interfaces, opened by the scan thread. */
    for (HIDDevice *dev : dmx)
    {
        if (device(dev->path()) != nullptr || listed.contains(dev->path()) == false)
        {
            disposeOffThread(dev);
            continue;
        }
        HIDDMXDevice *iface = qobject_cast<HIDDMXDevice *>(dev);
        if (iface != nullptr && iface->openFailed()
            && qobject_cast<QApplication *>(QCoreApplication::instance()) != nullptr
            && qEnvironmentVariableIntValue("QLC_TEST_OFFSCREEN_GUARD") != 1)
        {
            QMessageBox::warning(NULL, tr("HID DMX Interface Error"),
                tr("Unable to open %1. Make sure the udev rule is installed.").arg(dev->name()),
                QMessageBox::Ok, QMessageBox::Ok);
        }
        m_devices.append(dev);
        emit deviceAdded(dev);
        changed = true;
    }

    /* A stuck open whose device left the list, or is listed as another
       system entry at the same path: given up, so a device there now can
       be opened if the system ever offers it again. That is not a fix for
       the stuck one: its call may never return (dink, 0.4.10: macOS kept
       the dead device, and the USB hub, until a restart). Its thread is
       left to the system -- one such per path: a second one stuck there
       keeps the path until one of them returns. */
    QHash<QString, quint64> instances;
    for (const HIDFound &f : std::as_const(unique))
        instances.insert(f.path, f.instance);
    for (auto it = m_opening.begin(); it != m_opening.end(); )
    {
        const bool gone = listed.contains(it.key()) == false;
        const quint64 now = instances.value(it.key());
        const bool another = !gone && now != 0 && now != it->instance;
        if (it->stuck && (gone || another) && m_abandoned.contains(it.key()) == false)
        {
            qWarning() << Q_FUNC_INFO << it->name << (gone ? "left the list" : "is another device now")
                       << "while its open is stuck: given up";
            if (m_trace != nullptr)
                m_trace->openFailed(it.key(), it->product, it->vid, it->pid, QStringLiteral("given_up"));
            m_abandoned.insert(it.key(), it->job);
            it = m_opening.erase(it);
        }
        else
            ++it;
    }

    /* Joysticks: a new one, or one that came back at the same path (the
       old one reads nothing any more), is opened on a thread of its own. */
    for (const HIDFound &f : std::as_const(unique))
    {
        if (f.dmx)
            continue;
        HIDDevice *dev = device(f.path);
        const bool cameBack = dev != nullptr
            && (stale.contains(f.path)
                || (f.instance != 0 && dev->instance() != 0 && f.instance != dev->instance()));
        if (dev != nullptr && cameBack == false)
            continue;
        if (m_opening.contains(f.path))
            continue;   // one open at a time per device: a stuck one holds only itself
        startOpen(f, dev != nullptr);
    }

    if (changed)
    {
        renumberLines();
        emit configurationChanged();
        announceLines();
    }
    noteProblems();
}

void HIDPlugin::startOpen(const HIDFound &found, bool replacing)
{
    HIDBackend *be = backend();
    if (be == nullptr)
        return;
    Opening o;
    o.job = ++m_nextJob;
    o.replacing = replacing;
    o.instance = found.instance;
    o.serial = found.serial;
    o.vid = found.vendorId;
    o.pid = found.productId;
    o.product = found.product.trimmed();
    QString name = found.product.trimmed();
    if (name.isEmpty())
        name = QStringLiteral("HID device %1:%2")
                   .arg(found.vendorId, 4, 16, QLatin1Char('0'))
                   .arg(found.productId, 4, 16, QLatin1Char('0')).toUpper();
    const QString where = GamepadLayout::locationFromPath(found.path);
    o.name = where.isEmpty() ? name : QStringLiteral("%1 (%2)").arg(name, where);
    m_opening.insert(found.path, o);
    {
        QMutexLocker locker(&m_shared->lock);
        m_shared->opening++;
    }
    s_workers++;
    std::thread(openJob, m_shared, be, found, o.job).detach();
    const QString path = found.path;
    const quint64 job = o.job;
    QTimer::singleShot(m_openLimitMs, this, [this, path, job]() { openTimedOut(path, job); });
}

void HIDPlugin::openTimedOut(const QString &path, quint64 job)
{
    auto it = m_opening.find(path);
    if (it == m_opening.end() || it->job != job || it->stuck)
        return;
    qWarning() << Q_FUNC_INFO << it->name << "has not answered in" << m_openLimitMs
               << "ms: skipped until the system lets go of it";
    it->stuck = true;
    if (m_trace != nullptr)
        m_trace->openFailed(path, it->product, it->vid, it->pid, QStringLiteral("stuck"));
    /* The one it was to replace reads nothing any more: not kept listed,
       patched and silent (a patch on it goes offline, Pad Run disarms). */
    if (it->replacing)
    {
        it->replacing = false;
        if (HIDDevice *old = device(path))
        {
            dropDevice(old);
            renumberLines();
            emit configurationChanged();
            announceLines();
        }
    }
    noteProblems();
}

void HIDPlugin::applyOpened(const QString &path, quint64 job, HIDDevice *dev)
{
    /* One given up on (its pad left, or came back as another): whatever it
       made is not wanted; the path may take a new stuck one again. */
    auto given = m_abandoned.find(path);
    if (given != m_abandoned.end() && given.value() == job)
    {
        m_abandoned.erase(given);
        disposeOffThread(dev);
        return;
    }
    auto it = m_opening.find(path);
    if (it == m_opening.end() || it->job != job)
    {
        disposeOffThread(dev);
        return;
    }
    const bool wasStuck = it->stuck;
    const Opening opened = *it;
    m_opening.erase(it);
    HIDDevice *old = device(path);
    bool changed = false;

    if (m_listed.contains(path) == false || dev == nullptr)
    {
        /* Gone while it was opened, or nothing there to read: no device,
           and not the stale one it was to replace either. */
        disposeOffThread(dev);
        if (old != nullptr && m_listed.contains(path) == false)
        {
            dropDevice(old);
            changed = true;
        }
        else if (old != nullptr && dev == nullptr)
        {
            dropDevice(old);
            changed = true;
        }
        if (dev == nullptr && m_trace != nullptr)
            m_trace->openFailed(path, opened.product, opened.vid, opened.pid, QStringLiteral("nothing"));
        /* The system let go of one it held: ask again, it may answer now. */
        if (wasStuck && m_listed.contains(path))
            scheduleRescan(0);
    }
    else if (old != nullptr)
    {
        /* Same path, but not the device it was opened on: a controller
           switched off and on again, re-enumerated, or reconnected after
           the Mac slept. Replaced where it is, so the input line -- and the
           patch on it -- stays the same; configurationChanged() reopens it. */
        qDebug() << Q_FUNC_INFO << "reconnected:" << dev->name();
        const int index = m_devices.indexOf(old);
        m_devices[index] = dev;
        if (m_trace != nullptr && old->axisCount() >= 0)
            m_trace->gone(old);
        if (m_trace != nullptr && dev->axisCount() >= 0)
            m_trace->arrived(dev, opened.serial);
        m_diagnosticsStarted.remove(old);
        emit deviceRemoved(old);
        disposeOffThread(old);
        emit deviceAdded(dev);
        setupDiagnostics(dev);
        changed = true;
        m_deviceIds.remove(old);            // its line's device is another now
    }
    else
    {
        if (m_trace != nullptr && dev->axisCount() >= 0)
            m_trace->arrived(dev, opened.serial);
        m_devices.append(dev);
        emit deviceAdded(dev);
        setupDiagnostics(dev);
        changed = true;
    }

    if (changed)
    {
        renumberLines();
        emit configurationChanged();
        announceLines();
    }
    noteProblems();
}

QStringList HIDPlugin::targetProblems() const
{
    QStringList out;
    if (m_scanStuck)
        out << tr("USB controllers — the system's device list is not answering; "
                  "the show keeps running");
    for (auto it = m_opening.constBegin(); it != m_opening.constEnd(); ++it)
    {
        /* One the system no longer lists is gone, even if its open never
           returns: nothing to name, nothing the operator can do. */
        if (it->stuck && m_listed.contains(it.key()))
            out << tr("%1 — not answering; skipped, the show keeps running").arg(it->name);
    }
    out.sort();
    return out;
}

void HIDPlugin::noteProblems()
{
    const QStringList now = targetProblems();
    if (now == m_lastProblems)
        return;
    m_lastProblems = now;
    emit targetProblemsChanged();
}

int HIDPlugin::inputForPath(const QString &path) const
{
    for (int i = 0; i < m_devices.size(); ++i)
        if (m_devices.at(i)->axisCount() >= 0 && m_devices.at(i)->path() == path)
            return i;
    return -1;
}

bool HIDPlugin::injectVirtualInput(quint32 input, quint32 channel, uint value)
{
    /* QLC_HID_VIRTUAL_PAD's, or a test backend's (HIDBackend). */
    HIDVirtualPad *pad = qobject_cast<HIDVirtualPad *>(device(input));
    if (pad == nullptr)
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
    if (m_trace != nullptr && device->axisCount() >= 0)
        m_trace->arrived(device, QString());
    emit deviceAdded(device);

    emit configurationChanged();
    announceLines();
}

void HIDPlugin::removeDevice(HIDDevice* device)
{
    Q_ASSERT(device != NULL);

    m_devices.removeAll(device);

    m_deviceIds.remove(device);
    emit deviceRemoved(device);
    delete device;

    emit configurationChanged();
    announceLines();
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
    {
        if (m_trace != nullptr && dev != nullptr)
            m_trace->input(dev->path(), channel, value);
        emit valueChanged(universe, line, channel, value);
    }
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
            {
                if (m_trace != nullptr)
                    m_trace->input(dev->path(), quint32(ch), f.output(ch));
                emit valueChanged(UINT_MAX, line, quint32(ch), f.output(ch));
            }
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

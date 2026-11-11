/*
  Q Light Controller Plus - qlcconsole
  systemtuninghost.cpp

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

#include <QDeadlineTimer>
#include <QNetworkInterface>
#include <QStorageInfo>
#include <QSysInfo>
#include <QThread>
#include <QDir>
#include <QMap>

#include <algorithm>

#include "systemtuning.h"
#include "systemprobe.h"

/* The whole look may not take longer than this, however a tool behaves */
#define HOST_LOOK_MS 800

namespace
{
/** Using this much of a core: it is working, not just installed. The
 *  system's own daemons tick over higher when they have nothing to do. */
const double kActiveCpu = 5.0;
const double kActiveCpuBuiltIn = 15.0;

struct Known
{
    const char *name;   ///< lower case, no .exe; a trailing * matches the start
    const char *label;
    bool builtIn;       ///< part of the system: idle, it isn't worth a mention
    bool oneShot;       ///< only runs while working: being there is enough
};

const Known kBackups[] = {
    { "restic", "restic", false, true },
    { "borg", "Borg", false, true },
    { "rsync", "rsync", false, true },
    { "rclone", "rclone", false, true },
    { "duplicity", "Duplicity", false, true },
    { "rsnapshot", "rsnapshot", false, true },
    { "timeshift", "Timeshift", false, true },
    { "deja-dup*", "Déjà Dup", false, false },
    { "wbengine", "Windows Backup", true, true },
    { "bztransmit*", "Backblaze", false, false },
    { "bzfilelist*", "Backblaze", false, false },
    { "bzbmenu*", "Backblaze", false, false },
    { "bzserv*", "Backblaze", false, false },
    { "arq*", "Arq", false, false },
    { "com.bombich.ccc*", "Carbon Copy Cloner", false, false },
    { "ccc", "Carbon Copy Cloner", false, false },
    { "superduper*", "SuperDuper!", false, false },
    { "chronosync*", "ChronoSync", false, false },
    { "crashplan*", "CrashPlan", false, false },
    { "code42*", "CrashPlan", false, false },
    { "acronis*", "Acronis", false, false },
    { "trueimage*", "Acronis", false, false },
    { "reflectbin*", "Macrium Reflect", false, false },
    { "veeam*", "Veeam", false, false },
    { "dropbox*", "Dropbox", false, false },
    { "onedrive*", "OneDrive", false, false },
    { "googledrivefs*", "Google Drive", false, false },
    { "google drive*", "Google Drive", false, false },
    { "synologydrive*", "Synology Drive", false, false },
    { "cloud-drive-*", "Synology Drive", false, false },
    { "nextcloud*", "Nextcloud", false, false },
    { "owncloud*", "ownCloud", false, false },
    { "syncthing*", "Syncthing", false, false },
    { "insync*", "Insync", false, false },
    { "rslsync*", "Resilio Sync", false, false },
    { "bird", "iCloud Drive", true, false },
    { "fileproviderd", "macOS file sync", true, false },
};

const Known kUpdates[] = {
    { "softwareupdated", "Software Update", true, false },
    { "tiworker", "Windows Update", true, false },
    { "trustedinstaller", "Windows Update", true, false },
    { "mousocoreworker", "Windows Update", true, false },
    { "wuauclt", "Windows Update", true, false },
    { "apt", "apt", true, true },
    { "apt-get", "apt", true, true },
    { "dpkg", "dpkg", true, true },
    /* Not one-shot: its shutdown helper waits in the background all day */
    { "unattended-upgr*", "unattended-upgrades", true, false },
    { "packagekitd", "PackageKit", true, false },
    { "dnf", "dnf", true, true },
};

/* Friendlier names in the "what else is busy" list */
const Known kBusyNames[] = {
    { "mds_stores", "Spotlight indexing", true, false },
    { "mds", "Spotlight indexing", true, false },
    { "mdworker*", "Spotlight indexing", true, false },
    { "spotlightknowledged*", "Spotlight indexing", true, false },
    { "corespotlightd", "Spotlight indexing", true, false },
    { "fileproviderd", "macOS file sync", true, false },
    { "mediaanalysisd", "Photos analysis", true, false },
    { "photoanalysisd", "Photos analysis", true, false },
    { "searchindexer", "Windows Search indexing", true, false },
    { "msmpeng", "Windows Defender", true, false },
    { "tracker-miner*", "File indexing", true, false },
    { "localsearch*", "File indexing", true, false },
    { "baloo_file*", "File indexing", true, false },
    { "updatedb*", "File indexing", true, false },
    { "plocate*", "File indexing", true, false },
};

QString normalized(const QString &process)
{
    QString n = process.toLower();
    if (n.endsWith(QLatin1String(".exe")))
        n.chop(4);
    return n;
}

template <size_t N>
const Known *findKnown(const Known (&table)[N], const QString &name)
{
    for (const Known &k : table)
    {
        const QString pattern = QString::fromUtf8(k.name);
        if (pattern.endsWith(QLatin1Char('*')) ? name.startsWith(pattern.chopped(1)) : name == pattern)
            return &k;
    }
    return nullptr;
}

template <size_t N>
void sortOut(const Known (&table)[N], const QList<SystemProbe::ProcessUse> &processes,
             QStringList *active, QStringList *idle)
{
    for (const SystemProbe::ProcessUse &use : processes)
    {
        const Known *k = findKnown(table, normalized(use.name));
        if (k == nullptr)
            continue;
        const QString label = QString::fromUtf8(k->label);
        if (k->oneShot || use.cpu >= (k->builtIn ? kActiveCpuBuiltIn : kActiveCpu))
        {
            if (active->contains(label) == false)
                active->append(label);
        }
        else if (k->builtIn == false && idle->contains(label) == false)
        {
            idle->append(label);
        }
    }
    for (const QString &label : *active)
        idle->removeAll(label);
}
}

void SystemTuning::probeHost(Probe *p, bool deep)
{
    const QDeadlineTimer deadline(HOST_LOOK_MS);

    p->arch = QSysInfo::currentCpuArchitecture();
    p->buildArch = QSysInfo::buildCpuArchitecture();
    p->cpuName = SystemProbe::cpuName();
    p->performanceCores = SystemProbe::performanceCores();
    p->logicalCores = QThread::idealThreadCount();
    p->translated = SystemProbe::translated();

    p->onBattery = SystemProbe::onBattery(&p->batteryPercent);
    p->thermal = SystemProbe::thermalState();
    p->memory = SystemProbe::memoryPressure();
    const QStorageInfo disk(QDir::homePath());
    if (disk.isValid())
        p->diskFreeMb = disk.bytesAvailable() / (1024 * 1024);

    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces)
    {
        if (iface.type() != QNetworkInterface::Wifi
            || (iface.flags() & QNetworkInterface::IsRunning) == 0)
            continue;
        bool hasAddress = false;
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry &e : entries)
        {
            if (e.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            hasAddress = true;
            p->wirelessAddresses << e.ip().toString();
        }
        if (hasAddress && p->wirelessUp.contains(iface.name()) == false)
            p->wirelessUp << iface.name();
    }

    p->backupScheduled = SystemProbe::backupScheduled();
    p->autoUpdates = SystemProbe::autoUpdates();
    p->rebootPending = SystemProbe::rebootPending();

    /* Each of these starts a process, so only the deep look does them.
       publish() carries the last deep answers over the looks between. */
    p->deepLooked = deep;
    if (deep)
    {
        p->screenLock = SystemProbe::screenLock(&p->idleMinutes, deadline);
        p->backupRunning = SystemProbe::backupRunning(deadline);
        p->processes = SystemProbe::processes(deadline);
        p->processesKnown = p->processes.isEmpty() == false;
        if (p->model.contains(QLatin1String("Raspberry Pi")))
            p->piThrottled = SystemProbe::piThrottled(deadline);
    }
}

void SystemTuning::appendHostItems(const Probe &p, QList<Item> *items)
{
    /* What the frames actually did */
    {
        Item it;
        it.key = QStringLiteral("measured");
        it.title = tr("Measured with this show");
        it.what = tr("Everything else on this page is about what could go wrong; this is what "
                     "happened. A late frame left more than half a frame after it was due.");
        const quint64 frames = p.frames.frames;
        if (frames < 250)
        {
            it.status = Advice;
            it.state = tr("not enough frames yet");
        }
        else
        {
            const double percent = double(p.frames.late) * 100.0 / double(frames);
            it.state = tr("%1 late of %2 (%3%), %4 dropped")
                           .arg(p.frames.late).arg(frames)
                           .arg(percent, 0, 'f', 2).arg(p.frames.dropped);
            if (percent >= 1.0)
            {
                it.status = CanImprove;
                it.important = true;
            }
            else
            {
                it.status = percent >= 0.1 ? CanImprove : Good;
            }
            if (it.status == CanImprove)
                it.howTo = tr("Work down the other items here. If it stays late, the show is more "
                              "than this computer can compute in time: fewer universes, or a "
                              "faster computer.");
        }
        if (p.sendP99Ms >= 0)
            it.state += tr("; send delay p99 %1 ms").arg(p.sendP99Ms, 0, 'f', 1);
        items->append(it);
    }

    /* The universe threads */
    {
        Item it;
        it.key = QStringLiteral("universes");
        it.title = tr("Universe threads");
        it.what = tr("One thread per universe computes its frame and sends it. If only the timer "
                     "is real-time, it can end up waiting on them.");
        switch (p.universeScheduling)
        {
            case Universe::ThreadsRealtime:
                it.status = Good;
                it.state = tr("real-time");
            break;
            case Universe::ThreadsElevated:
                it.status = Good;
                it.state = tr("high priority");
            break;
            case Universe::ThreadsRefused:
                it.status = CanImprove;
                it.state = tr("real-time asked for, refused");
                if (p.os == OsLinux)
                    it.howTo = tr("Give this user a real-time limit: see Real-time DMX timer.");
            break;
            case Universe::ThreadsNormal:
                it.state = tr("normal priority; patched universes: %1").arg(p.patchedUniverses);
                if (p.os == OsLinux && p.patchedUniverses >= 64
                    && p.scheduling == MasterTimer::SchedulingRealtime)
                {
                    it.status = CanImprove;
                    it.what += QLatin1Char(' ')
                            + tr("On a Raspberry Pi 5 at 160 universes, making them real-time too "
                                 "halved the worst frame times, for about 10% more CPU.");
                    it.command = QStringLiteral("QLC_UNIVERSE_SCHED=rt '%1' -o your-show.qlcc").arg(p.binary);
                    it.howTo = tr("Experimental: start the app this way to try it.");
                }
                else
                {
                    it.status = p.patchedUniverses >= 64 ? Advice : Good;
                }
            break;
        }
        items->append(it);
    }

    /* The machine */
    {
        Item it;
        it.key = QStringLiteral("machine");
        it.title = tr("This computer");
        const int efficiency = qMax(0, p.logicalCores - p.performanceCores);
        QString cores = efficiency > 0
                ? tr("%1 performance + %2 efficiency cores").arg(p.performanceCores).arg(efficiency)
                : tr("%1 cores").arg(p.logicalCores);
        /* An ARM board has no processor name to read: its model says more */
        QStringList parts({ p.cpuName.isEmpty() ? p.model : p.cpuName, cores, p.arch });
        parts.removeAll(QString());
        it.state = parts.join(QStringLiteral(", "));
        const bool arm = p.arch.startsWith(QLatin1String("arm"));
        if (p.os == OsMac && arm)
            it.what = tr("Apple silicon: the timer runs on the performance cores. Heat and "
                         "background work on those cores are what to watch.");
        else if (p.os == OsMac)
            it.what = tr("An Intel Mac: under sustained load it slows down when it gets hot, so "
                         "keep the vents clear and keep it plugged in.");
        else if (p.os == OsLinux && arm)
            it.what = tr("A small ARM computer: 64-bit Linux, a good power supply and cooling "
                         "make the most difference.");
        else
            it.what = efficiency > 0
                    ? tr("This processor has efficiency cores; a power-saving plan moves work onto them.")
                    : tr("The processor the show runs on.");

        if (p.translated || (p.arch.isEmpty() == false && p.arch != p.buildArch))
        {
            it.status = CanImprove;
            it.important = p.translated;
            it.state += QLatin1String("; ")
                     + tr("this build is for %1").arg(p.translated ? tr("another processor") : p.buildArch);
            it.howTo = p.os == OsMac
                     ? tr("Finder ▸ the app ▸ Get Info: untick \"Open using Rosetta\", then open it again.")
                     : tr("Install the version built for this processor (%1).").arg(p.arch);
        }
        else
        {
            it.status = p.arch.isEmpty() ? NotAvailable : Good;
        }
        items->append(it);
    }

    /* Mains */
    {
        Item it;
        it.key = QStringLiteral("power");
        it.title = tr("Mains power");
        it.what = tr("On battery, most computers slow the processor down, and the show stops "
                     "when the battery does.");
        if (p.onBattery == 1)
        {
            it.status = CanImprove;
            it.important = true;
            it.state = p.batteryPercent >= 0 ? tr("on battery, %1%").arg(p.batteryPercent) : tr("on battery");
            it.howTo = tr("Plug it in.");
        }
        else if (p.onBattery == 0)
        {
            it.status = Good;
            it.state = tr("plugged in, or no battery");
        }
        else
        {
            it.status = NotAvailable;
        }
        items->append(it);
    }

    /* Heat */
    {
        Item it;
        it.key = QStringLiteral("thermal");
        it.title = tr("Temperature");
        it.what = tr("A processor that gets too hot slows itself down, with no warning on screen.");
        switch (p.thermal)
        {
            case 0: it.status = Good; it.state = tr("normal"); break;
            case 1: it.status = Advice; it.state = tr("warm"); break;
            case 2:
            case 3:
                it.status = CanImprove;
                it.important = true;
                it.state = p.thermal == 2 ? tr("hot: slowing down") : tr("critical");
                it.howTo = tr("Clear the vents, get it out of the sun and off soft surfaces, "
                              "and close what else is running.");
            break;
            default: it.status = NotAvailable; it.state = tr("this system doesn't say"); break;
        }
        items->append(it);
    }

    /* Wi-Fi */
    {
        Item it;
        it.key = QStringLiteral("wifi");
        it.title = tr("Wi-Fi");
        it.what = tr("Wi-Fi holds packets back to save power and retries them on its own. "
                     "Art-Net or sACN over Wi-Fi then arrives in bursts.");
        bool carriesDmx = false;
        for (const QString &address : p.dmxAddresses)
            carriesDmx = carriesDmx || p.wirelessAddresses.contains(address);
        if (p.wirelessUp.isEmpty())
        {
            it.status = Good;
            it.state = tr("no Wi-Fi in use");
        }
        else
        {
            it.status = CanImprove;
            it.important = carriesDmx;
            it.state = carriesDmx ? tr("carrying DMX: %1").arg(p.wirelessUp.join(QStringLiteral(", ")))
                                  : tr("Wi-Fi up: %1").arg(p.wirelessUp.join(QStringLiteral(", ")));
            if (p.os == OsLinux)
            {
                QStringList cmds;
                for (const QString &dev : p.wirelessUp)
                    cmds << QStringLiteral("sudo iw dev %1 set power_save off").arg(dev);
                it.command = cmds.join(QLatin1Char('\n'));
                it.howTo = tr("Best: send DMX over wired Ethernet. The command lasts until the "
                              "next restart; NetworkManager keeps it with "
                              "wifi.powersave = 2 in /etc/NetworkManager/conf.d/.");
            }
            else
            {
                it.howTo = tr("Best: send DMX over wired Ethernet, and turn Wi-Fi off for the show "
                              "if nothing needs it.");
            }
        }
        items->append(it);
    }

    /* macOS can refuse the app the network without saying so */
    if (p.os == OsMac && p.networkOutputs > 0)
    {
        Item it;
        it.key = QStringLiteral("localnetwork");
        it.title = tr("Local Network permission");
        it.what = tr("macOS can refuse an app the local network. Then every Art-Net or sACN "
                     "output fails, however good the wiring is.");
        if (p.failingOutputs >= p.networkOutputs)
        {
            it.status = CanImprove;
            it.important = true;
            it.state = tr("every network output is failing");
            it.howTo = tr("Most likely this permission. System Settings ▸ Privacy & Security ▸ "
                          "Local Network: turn on qlcconsole, then quit and open the app again. "
                          "If it is on already, check the cable and the address.");
        }
        else
        {
            it.status = Good;
            it.state = tr("outputs are sending");
        }
        items->append(it);
    }

    /* The screen */
    {
        Item it;
        it.key = QStringLiteral("screenlock");
        it.title = tr("Screen sleep and lock");
        it.what = tr("A screen that sleeps or locks in the middle of a show leaves the operator "
                     "looking at a password prompt. DMX keeps going, but nobody can touch it.");
        if (p.keepDisplayAwake && p.fullSpeedInOperate && p.fullSpeedSupported)
        {
            it.status = Good;
            it.state = tr("kept awake during Operate (setting above)");
        }
        else if (p.screenLock < 0 && p.idleMinutes < 0)
        {
            it.status = Advice;
            it.state = tr("this system doesn't say");
            it.howTo = tr("Turn off the screen lock and the idle timeout in the system's settings "
                          "before the show.");
        }
        else if (p.idleMinutes == 0)
        {
            it.status = Good;
            it.state = tr("never sleeps");
        }
        else
        {
            it.status = CanImprove;
            it.important = p.screenLock == 1;
            const QString after = p.idleMinutes > 0 ? tr("after %1 min idle").arg(p.idleMinutes)
                                                    : tr("when idle");
            it.state = p.screenLock == 1 ? tr("locks %1").arg(after) : tr("sleeps %1").arg(after);
            if (p.fullSpeedSupported)
            {
                it.howTo = p.fullSpeedInOperate
                         ? tr("Tick \"Keep the display awake\" above.")
                         : tr("Tick \"Run at full speed\" and \"Keep the display awake\" above: "
                              "the app can only hold the screen awake along with full speed.");
            }
            else
            {
                it.command = QStringLiteral("gsettings set org.gnome.desktop.session idle-delay 0\n"
                                            "gsettings set org.gnome.desktop.screensaver lock-enabled false");
                it.howTo = tr("The app can't hold the screen awake on this system. These turn the "
                              "timeout and the lock off until you turn them on again.");
            }
        }
        items->append(it);
    }

    /* Backups and file sync */
    {
        Item it;
        it.key = QStringLiteral("backups");
        it.title = tr("Backups and file sync");
        it.what = tr("A backup or sync reads and writes for a long time, and takes processor and "
                     "disk and network from the show. It can start by itself mid-show.");
        QStringList active, idle;
        sortOut(kBackups, p.processes, &active, &idle);
        if (p.backupRunning == 1)
            active.prepend(tr("Time Machine"));
        if (active.isEmpty() == false)
        {
            it.status = CanImprove;
            it.important = true;
            it.state = tr("running now: %1").arg(active.join(QStringLiteral(", ")));
            if (p.backupRunning == 1)
                it.command = QStringLiteral("tmutil stopbackup");
            it.howTo = tr("Stop it or pause it until after the show.");
        }
        else if (idle.isEmpty() == false || p.backupScheduled == 1)
        {
            it.status = CanImprove;
            if (p.backupScheduled == 1)
                idle.prepend(tr("Time Machine"));
            it.state = tr("idle, could start: %1").arg(idle.join(QStringLiteral(", ")));
            it.howTo = p.backupScheduled == 1
                     ? tr("Pause sync apps before the show. Time Machine: System Settings ▸ General "
                          "▸ Time Machine ▸ Options ▸ Back up frequency: Manually.")
                     : tr("Pause them before the show.");
        }
        else if (p.processesKnown || p.backupRunning == 0)
        {
            it.status = Good;
            it.state = tr("none found");
        }
        else
        {
            it.status = NotAvailable;
            it.state = tr("not checked");
        }
        items->append(it);
    }

    /* System updates */
    {
        Item it;
        it.key = QStringLiteral("updates");
        it.title = tr("System updates");
        it.what = tr("An update takes the processor and the disk while it installs, and some "
                     "restart the computer.");
        QStringList active, idle;
        sortOut(kUpdates, p.processes, &active, &idle);
        if (p.rebootPending)
        {
            it.status = CanImprove;
            it.important = true;
            it.state = tr("waiting to restart the computer");
            it.howTo = tr("Restart before the show, so Windows doesn't choose when.");
        }
        else if (active.isEmpty() == false)
        {
            it.status = CanImprove;
            it.important = true;
            it.state = tr("running now: %1").arg(active.join(QStringLiteral(", ")));
            it.howTo = tr("Let it finish before the show.");
        }
        else if (p.autoUpdates == 1)
        {
            it.status = CanImprove;
            it.state = tr("installs by itself");
            if (p.os == OsMac)
            {
                it.howTo = tr("System Settings ▸ General ▸ Software Update ▸ Automatic Updates: "
                              "turn off \"Install macOS updates\" for show computers.");
            }
            else
            {
                it.command = QStringLiteral("sudo systemctl disable --now apt-daily.timer apt-daily-upgrade.timer");
                it.howTo = tr("On a computer that only runs shows. Update by hand between shows.");
            }
        }
        else if (p.os == OsWindows)
        {
            it.status = Advice;
            it.state = tr("Windows decides when");
            it.howTo = tr("Settings ▸ Windows Update: pause updates, and set active hours to "
                          "cover the show.");
        }
        else if (p.autoUpdates == 0 || p.processesKnown)
        {
            it.status = Good;
            it.state = tr("none running");
        }
        else
        {
            it.status = NotAvailable;
            it.state = tr("not checked");
        }
        items->append(it);
    }

    /* Whatever else is busy */
    {
        Item it;
        it.key = QStringLiteral("load");
        it.title = tr("Other programs");
        it.what = tr("Anything else using the processor competes with the show. The timer goes "
                     "first; drawing the screen, and the universes, may not.");
        QMap<QString, double> byName;
        for (const SystemProbe::ProcessUse &use : p.processes)
        {
            const Known *k = findKnown(kBusyNames, normalized(use.name));
            byName[k != nullptr ? QString::fromUtf8(k->label) : use.name] += use.cpu;
        }
        QList<QPair<double, QString> > busy;
        double total = 0;
        for (auto i = byName.constBegin(); i != byName.constEnd(); ++i)
        {
            total += i.value();
            if (i.value() >= 25.0)
                busy << qMakePair(i.value(), i.key());
        }
        std::sort(busy.begin(), busy.end(), [](const QPair<double, QString> &a, const QPair<double, QString> &b) {
            return a.first > b.first;
        });
        QStringList named;
        for (int i = 0; i < busy.size() && i < 4; i++)
            named << tr("%1 %2%").arg(busy.at(i).second).arg(qRound(busy.at(i).first));

        if (p.processesKnown == false)
        {
            it.status = NotAvailable;
            it.state = tr("not checked");
        }
        else if (named.isEmpty())
        {
            it.status = Good;
            it.state = tr("nothing busy");
        }
        else
        {
            const int cores = qMax(1, p.performanceCores);
            it.status = CanImprove;
            it.important = total >= 50.0 * cores;
            it.state = named.join(QStringLiteral(", "));
            it.howTo = tr("Close what the show doesn't need. 100% is one whole core.");
        }
        items->append(it);
    }

    /* Memory and disk */
    {
        Item it;
        it.key = QStringLiteral("memory");
        it.title = tr("Memory and disk space");
        it.what = tr("With memory short the system swaps to disk and everything stalls. With the "
                     "disk full, saves and autosaves fail.");
        QStringList states;
        bool bad = false;
        if (p.memory >= 1)
        {
            bad = true;
            it.important = p.memory == 2;
            states << (p.memory == 2 ? tr("memory critically low") : tr("memory low"));
        }
        const bool lowDisk = p.diskFreeMb >= 0 && p.diskFreeMb < 2048;
        if (lowDisk)
        {
            bad = true;
            states << tr("%1 MB of disk free").arg(p.diskFreeMb);
        }
        if (bad)
        {
            it.status = CanImprove;
            it.howTo = tr("Close other programs, and free some disk space.");
        }
        else if (p.memory == 0 || p.diskFreeMb >= 0)
        {
            it.status = Good;
            states << tr("enough");
        }
        else
        {
            it.status = NotAvailable;
        }
        it.state = states.join(QStringLiteral(", "));
        items->append(it);
    }

    /* The app's own settings */
    {
        Item it;
        it.key = QStringLiteral("appsettings");
        it.title = tr("Rig view settings");
        it.what = tr("The rig view draws on this same computer. During Operate it should stay "
                     "out of the way of the show.");
        QStringList states;
        if (p.rigOperateFps > 30)
            states << tr("%1 frames a second in Operate").arg(p.rigOperateFps);
        if (p.rigSupersample >= 3)
            states << tr("supersample %1×").arg(p.rigSupersample);
        if (p.rigGpu == false)
            states << tr("graphics card not used");
        if (states.isEmpty())
        {
            it.status = Good;
            it.state = tr("%1 frames a second in Operate").arg(p.rigOperateFps);
        }
        else
        {
            it.status = CanImprove;
            it.state = states.join(QStringLiteral(", "));
            it.howTo = tr("In the rig view's settings: 15 to 30 frames a second in Operate, "
                          "supersample 1 or 2, graphics card on.");
        }
        items->append(it);
    }
}

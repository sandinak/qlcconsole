# Tuning your computer for steady DMX

qlcconsole sends a DMX frame to every universe 50 times a second. The
frames have to leave on time, every 20 ms. A busy computer, or one saving
power, can hold the timer up, and a late frame shows as a stutter in a
chase or a fade. **Settings ▸ Performance** checks this computer and says
what to change.

## Settings ▸ Performance

**During Operate** has the settings the app applies itself:

| Setting | What it does | Default |
|---|---|---|
| **Run at full speed** | macOS: no App Nap and no idle sleep. Windows: no power throttling (EcoQoS) and no idle sleep. Only while the show is in Operate. | On |
| **Keep the display awake** | The screen doesn't sleep or lock when idle either, while in Operate. Needs **Run at full speed**. | On |
| **Say in Show Status…** | The footer warns during Operate when an item marked ● below is off. | On |

**This computer** lists what helps the timer, one line each:

- **OK**: set up for steady output.
- **Can improve**: select it to see why, and the command or the steps.
  **Copy** puts the command on the clipboard to paste into a terminal.
- **Not available**: this computer doesn't have it, or a virtual machine's
  host decides it.
- **Worth knowing**: nothing to detect, just good to know.

An item marked **●** is important: the footer says so during Operate.
**Check Again** looks again after you change something.

The app never changes a system setting that needs an administrator, and
never asks for a password. Those are yours to do, with the command shown.

## On every computer

These look at what this computer is doing now. During Operate the app looks
again every 15 seconds (once a minute for the ones that run a system tool),
so a backup that starts mid-show, or a laptop that comes unplugged, reaches
the footer while it matters. These looks run at the lowest priority the
system has, so the show always comes first.

| Item | What it looks at | What to do |
|---|---|---|
| **Measured with this show** ● | How many frames actually left late, or never left. In the footer it covers the last 15 seconds. 1% late or more is marked important. | Work down the list. If it stays late, the show is more than this computer can compute in time. |
| **Universe threads** | Whether the threads that compute and send each universe are real-time too. Measured on a Raspberry Pi 5: at 160 universes, real-time universe threads halved the worst frame times for about 10% more CPU. | On Linux with 64 or more universes it shows how to try it. Not measured on other systems yet. |
| **This computer** ● | The processor, its cores, and whether this build matches it. | Apple silicon, Intel Macs, and small ARM boards each get their own advice. A Mac running the app under Rosetta, or a build for the wrong processor: use the right one. |
| **Mains power** ● | On battery or plugged in. | Plug it in. |
| **Temperature** ● | Whether the system says it is slowing down from heat. Windows doesn't say. | Clear the vents and close what else is running. |
| **Wi-Fi** ● | Wi-Fi in use. Important when your Art-Net or sACN outputs are on it. | Send DMX over wired Ethernet. |
| **Screen sleep and lock** ● | How soon the screen sleeps, and whether it asks for a password. DMX keeps going behind a locked screen, but nobody can operate it. Important when it locks. | **Keep the display awake** is on by default. On Linux (GNOME) copy the commands. |
| **Backups and file sync** ● | Time Machine, backup tools (Backblaze, Arq, Carbon Copy Cloner, restic and others) and sync clients (iCloud, Dropbox, OneDrive, Google Drive, Synology Drive and others). Important while one is working. | Stop or pause it until after the show. Time Machine: **Back up frequency: Manually**. |
| **System updates** ● | Updates installing now, a Windows restart waiting, and updates set to install by themselves. | Let it finish, or restart, before the show. Turn automatic installs off on a computer that only runs shows. |
| **Other programs** ● | Anything using a quarter of a core or more. Important when together they use half of the performance cores. Spotlight and other indexers are named. | Close what the show doesn't need. |
| **Memory and disk space** ● | Memory pressure, and under 2 GB of disk free. | Close programs, free some disk. |
| **Rig view settings** | Frames a second in Operate, supersampling, and whether the graphics card is used. | 15 to 30 frames a second, supersample 1 or 2, graphics card on. |

## Linux and Raspberry Pi

| Item | What to do |
|---|---|
| **Real-time DMX timer** ● | Linux lets the timer jump the queue (real-time scheduling, `SCHED_FIFO`) only for a user with a real-time limit. Without it the timer waits its turn behind every other thread. Copy the commands: they add `@audio - rtprio 95` in `/etc/security/limits.d/` and put you in the `audio` group. Then log out and back in. Or run `sudo setcap cap_sys_nice=eip` on the program itself (again after each update). |
| **CPU speed (governor)** | `performance` keeps the processor at full clock. The command lasts until the next restart. |
| **Real-time kernel** | A `PREEMPT_RT` kernel is not needed for 50 Hz DMX once the timer has real-time scheduling. |
| **Raspberry Pi power and heat** ● | Read for you when `vcgencmd` is available: whether the Pi is slowing down now (important), or has since it started. Use the official power supply and a fan or heatsink. Overclocking isn't needed. |

What real-time scheduling does, measured on a Raspberry Pi 5: 3 to 15 times fewer
late frames below the Pi's limit. It does not raise the limit: a Pi that
can't compute 256 universes in time still can't.

Measured on the same Pi 5 (2026-11-03, 128 universes, two 40-second runs
each, late frames = more than 30 ms apart):

| | Late frames | Frame interval p99 |
|---|---|---|
| As installed (`ondemand`, normal priority) | 40, 60 | 32–38 ms |
| `performance` governor only | 72, 67 | 38 ms |
| Real-time timer only | 25, 1 | 26–34 ms |
| Real-time timer and `performance` | 4, 1 | 25–26 ms |

The governor alone made no difference; the real-time timer is what
counts. That is why only the timer is marked ●.

**Advanced, not checked:** on a machine that only runs the show you can
keep other work off one core (`isolcpus=` and `irqaffinity=` on the kernel
command line) and turn off interrupt coalescing on the DMX network card
(`ethtool -C eth0 rx-usecs 0`). Neither is needed for 50 Hz output.

## macOS

| Item | What to do |
|---|---|
| **Real-time DMX timer** ● | Set by the app: macOS gives it the same time-constraint priority audio apps use. Nothing to do. |
| **No App Nap during Operate** | Behind other windows, or with the screen asleep, macOS slows an app's timers and its input to save power. **Run at full speed** turns this off during Operate. |
| **Low Power Mode** ● | Lowers the processor's speed. System Settings ▸ Battery ▸ Low Power Mode: **Never**, or **Only on Battery** and keep the Mac plugged in. |
| **Local Network permission** ● | Shown when the show has Art-Net or sACN outputs. If every one of them is failing, macOS has most likely refused the app the network: System Settings ▸ Privacy & Security ▸ Local Network, turn on qlcconsole, then quit and open the app again. |

## Windows

| Item | What to do |
|---|---|
| **Real-time DMX timer** ● | Set by the app through the Multimedia Class Scheduler ("Pro Audio"), with a high-resolution timer. If it was refused, check that the **Multimedia Class Scheduler** service is running. |
| **No power throttling during Operate** | Windows 11 slows an app it thinks is in the background (EcoQoS). **Run at full speed** opts out during Operate. |
| **Power plan** | **Power saver** ● and **Balanced** lower the processor's speed. Copy `powercfg /setactive SCHEME_MIN` (High performance), or Control Panel ▸ Power Options. On a laptop also set Settings ▸ System ▸ Power & battery ▸ Power mode: **Best performance**, and keep it plugged in. |

The Windows settings are built and unit-tested but not yet measured on a
Windows computer.

## How to tell it worked

The footer's **Engine Load** chip (Settings ▸ Appearance) shows how long
each frame takes to compute. Late frames, when the computer held the timer
up, are what this page reduces. The measurements behind it are in
`docs/performance.md` in the source.

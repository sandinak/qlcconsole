# qlcconsole on the Raspberry Pi 5

## The package (0.3.1)

A school can leave a Pi as the show machine: `qlcconsole_<VERSION>+git<N>.<sha>_arm64.deb`.
Decided (can change): a **.deb**, not an AppImage. The Pi's apt already
carries Qt 6.8.2, so the package depends on it (small, updates with the OS),
`apt remove` takes it all away, and the udev rules, MIME type and icons go
where the system looks. The x86_64 AppImage stays the generic Linux build.

Build it on the Pi from a committed revision (never the Pi's own checkout;
about 35 to 45 min at -j3, `nice -n 10`, one build at a time on that host):

```sh
platforms/linux/make-pi-deb.sh --remote rpi5 [--rev SHA]     # result in dist/
```

Install, check and remove on the Pi:

```sh
sudo apt install ./qlcconsole_*_arm64.deb      # pulls the Qt 6 packages
qlcconsole --version
QT_QPA_PLATFORM=offscreen qlcconsole -d 0 -o /path/to/show.qlcc
sudo apt remove qlcconsole                     # udev rules are conffiles: purge removes them
```

It installs the program, the I/O plugins and the stock fixtures, scripts and
profiles under `/usr`, a desktop entry, icons, the MIME type, the man pages
and the stock USB DMX udev rules (`/etc/udev/rules.d/z65-*.rules`: Enttec/FTDI,
uDMX, Peperoni, HID, SPI); the post-install step reloads udev so an adapter
already plugged in is picked up. It conflicts with the dev symlinks in
section 3 below (same paths): remove those before installing the package.
The package is not published anywhere; copy it to the Pi by hand.

How qlcconsole was built and tested on `rpi5` (Raspberry Pi 5, 8 GB, aarch64)
on 2026-10-20. Follow it top to bottom to repeat it.

## The machine

- Debian 13 trixie (Raspberry Pi OS), upgraded in place from bookworm.
  Kernel 6.18.50+rpt-rpi-2712.
- GCC 14.2, CMake 3.31.6, Qt 6.8.2 from apt.
- Reach it at `ssh branson@192.168.20.120` (wlan0). It also holds
  172.18.2.218 on eth0, which is the **show VLAN**: never run a show with
  patched outputs here unless you mean to send DMX.

Bookworm's Qt is 6.4, and qlcconsole needs 6.8 or later, so building on
bookworm needs Qt from aqtinstall. Trixie's apt Qt 6.8.2 works with no extra
steps.

## 1. Packages

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git \
  qt6-base-dev qt6-base-private-dev qt6-base-dev-tools qt6-declarative-dev \
  qt6-3d-dev qt6-multimedia-dev qt6-serialport-dev qt6-websockets-dev \
  qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools qt6-shadertools-dev \
  qt6-svg-dev qt6-image-formats-plugins libgl1-mesa-dev libasound2-dev \
  libusb-1.0-0-dev libftdi1-dev libudev-dev libfftw3-dev libsndfile1-dev \
  libmad0-dev liblo-dev libxml2-utils shared-mime-info xvfb xauth x11-utils \
  python3 python3-lxml gdb
```

This is the same list as the x86 Linux build host. Every name exists on
trixie arm64. `qt6-wayland` came in with the desktop, so Qt runs natively on
labwc.

## 2. Source and build

```sh
cd ~/git/qlcconsole
git fetch origin
git checkout -B feat/show-tree origin/feat/show-tree
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build -j3
```

- `-j3` leaves one core free so the desktop and ssh stay responsive.
- 1493 steps took **34 min 22 s** (2062 s) from a clean tree, with no code
  changes and no warnings promoted to errors. Linux builds always use
  `-Werror`.
- Peak temperature was 61.5 °C, and the CPU stayed at 2.4 GHz for the whole
  build.
- A rebuild after a few edits is a few minutes.

## 3. System data and plugins (symlinks into the checkout)

The binary has these paths compiled in: `/usr/share/qlcconsole/...` for
fixtures, scripts and so on, and `/usr/lib/qt6/plugins/qlcconsole` for I/O
plugins. A build tree has neither. Rather than `cmake --install`, the system
paths are symlinks back into the checkout, so a rebuild is live straight away:

```sh
R=$HOME/git/qlcconsole
# I/O plugins (these were set up in Aug 2026 and still point at build/plugins)
sudo mkdir -p /usr/lib/qt6/plugins/qlcconsole/audio
for p in $R/build/plugins/*.so; do sudo ln -sfn "$p" /usr/lib/qt6/plugins/qlcconsole/; done
# Stock data (fixtures, gobos, RGB/effect scripts, presets, profiles, web UI)
sudo mkdir -p /usr/share/qlcconsole
for d in bundles colorfilters deviceprofiles effectpresets effectscripts fixtures \
         gobos inputprofiles meshes miditemplates modifierstemplates rgbscripts; do
  sudo ln -sfn $R/resources/$d /usr/share/qlcconsole/$d
done
sudo ln -sfn $R/webaccess/res /usr/share/qlcconsole/web
```

Without the data links, any show that uses a stock definition it doesn't
embed fails to load those fixtures. The classroom show's WLED lanterns are an
example.

User data lives in `~/.qlcconsole/`. Some old personal definitions there
(PHS-Crew-*, PHS-Chorus-Riser, American_DJ-3Z) fail to parse. That is
harmless noise in the log.

## 4. Unit tests (headless)

The installed plugin symlinks put the **real** Art-Net plugin in the plugin
directory. That shadows the test stub "dressed as ArtNet" in `appviews_test`,
so two rig-network tests fail (`rigNetworkFooterSaysWhyOutputIsHeld`, then
`rigNetworkSettingsRoundTrip` as a knock-on), and `unittest.sh` stops at the
first failing binary. Run the suite with the plugin directory masked by an
empty bind mount, in a private mount namespace. Nothing changes system-wide:

```sh
cd ~/git/qlcconsole
mkdir -p ~/qlc-smoke/emptyplugins
sudo unshare -m sh -c "mount --bind $HOME/qlc-smoke/emptyplugins /usr/lib/qt6/plugins/qlcconsole && \
  exec setpriv --reuid=$(id -u) --regid=$(id -g) --init-groups \
  env HOME=$HOME USER=$USER QT_QPA_PLATFORM=offscreen ./unittest.sh ui build" \
  > build/test.log 2>&1; echo rc=$?
tail -3 build/test.log
```

Result on e79b11733: **all pass**. That is 92 engine and 61 UI test programs,
5174 test functions passed, 0 failed, 7 skipped (by design) and 1 XFAIL
(translation). It took 164 s, or 273 s on a second run.

## 5. Smoke test (offscreen, no outputs)

The show is `docs/course/shows/ShowChoir-Classroom.qlcc` from branch
`docs/show-tree-course`. It has 13 universes and no output patches.

```sh
mkdir -p ~/qlc-smoke && cd ~/qlc-smoke
git -C ~/git/qlcconsole show origin/docs/show-tree-course:docs/course/shows/ShowChoir-Classroom.qlcc > ShowChoir-Classroom.qlcc
cp ShowChoir-Classroom.qlcc run.qlcc        # run a copy; the original stays pristine
QT_QPA_PLATFORM=offscreen ~/git/qlcconsole/build/main/qlcconsole -d 0 -o $HOME/qlc-smoke/run.qlcc > app.log 2>&1 &
P=$!; sleep 30
ps -o etime=,%cpu=,rss= -p $P
# Clean quit: the app has no SIGTERM handler, so ask Qt to quit.
sudo gdb -p $P -batch -ex "call (void)_ZN16QCoreApplication4quitEv()" >/dev/null 2>&1
wait $P; echo exit=$?
```

Result: the show loaded, including the WLED lanterns from
`/usr/share/qlcconsole/fixtures`. After 30 s the app was at about 8% CPU and
about 200 MB RSS. It quit at once with exit code 0, and the show file was
unchanged. Its only socket was a UDP listener on :3330 (ENTTEC Wing input,
receive only). Nothing was sent.

## 6. Running it on the Pi's desktop

From a terminal inside the labwc session:

```sh
~/git/qlcconsole/build/main/qlcconsole -o /path/to/show.qlcc
```

Always use `-o`. Positional arguments are ignored and can overwrite a show.
There is also a menu entry: `~/.local/share/applications/qlcconsole-dev.desktop`
("qlcconsole (dev build)").

Over ssh into the running desktop session (Wayland):

```sh
XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0 \
  ~/git/qlcconsole/build/main/qlcconsole -o /path/to/show.qlcc
```

Check the socket name with `ls /run/user/1000/wayland-*`.

**As of 2026-10-20 the desktop does not start after the in-place trixie
upgrade.** `lightdm.service` fails with "Failed to find session configuration
lightdm-greeter", because the `lightdm-greeter` alternative still points at
`/usr/share/xgreeters/pi-greeter.desktop`, which trixie's pi-greeter 1.3 no
longer ships (it has `pi-greeter-labwc.desktop` and friends). The bookworm
`raspberrypi-ui-mods` meta-package is still installed, and trixie's
`rpd-wayland-core` is not. One likely fix (not applied):

```sh
sudo apt-get install rpd-wayland-core        # trixie's desktop meta-package
# or just repoint the greeter:
sudo update-alternatives --set lightdm-greeter /usr/share/xgreeters/pi-greeter-labwc.desktop
sudo systemctl restart lightdm
```

## Power

`vcgencmd get_throttled` reads `0x50000` (under-voltage and throttling have
*occurred* since boot) after a burst of test load, at only about 52 °C. The 5 V
rail read 4.87 V and `usb_max_current_enable=0`, which suggests the supply is
not the official 27 W (5 V/5 A) unit. Builds were not slowed by it, but use
the official PSU for long runs.

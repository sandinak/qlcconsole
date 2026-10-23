# Software updates

qlcconsole can tell you when a newer version is on GitHub and, on macOS and
Windows, install it for you. It never updates or restarts during a show.

## What you see

A few seconds after the window opens, and then at most once a day,
qlcconsole asks GitHub for its list of releases. This happens in the
background. When there is a newer version, the footer shows a chip:

**Update available: 0.2.0**

Nothing pops up. Click the chip, or choose **Help → Check for Updates…**,
to open **Software Update**. It shows the release notes and these buttons:

| Button | What it does |
|---|---|
| **Install** | Downloads the update and checks it. The show keeps running. |
| **Restart and Install** | When the update is ready and nothing is running: asks about unsaved changes, quits, installs, and reopens the same show. |
| **When I Quit** / **Later** | Closes the window. A ready update installs the next time you quit qlcconsole. |
| **Skip This Version** | Stops offering this release. A newer one is still offered, and **Check for Updates…** shows the skipped one again. |

While it works, the chip says what it is doing: *Downloading 0.2.0… 42%*,
*Checking update 0.2.0…* and then *Update 0.2.0 installs when you quit*.
After the update, the chip says *Updated to 0.2.0* for a couple of minutes.

## Never during a show

An update is only put in place when **you** quit qlcconsole, or click
**Restart and Install**. Even then, it waits if:

- qlcconsole is in **Perform (Operate)** mode,
- any function (a scene, a chase, a cue) is running, or
- qlcconsole is running as a kiosk.

If one of these is true when you quit, the update is left for the next quit.
**Restart and Install** stays greyed out, and the window says why.

## Settings

The **Software Update** group in **Settings → General** belongs to this computer, not
to the show:

- **Check for updates (once a day, from GitHub)** turns checking on or off.
- **Channel**: **Stable releases**, or **Pre-releases too** (for example
  the **Show Tree** builds handed out to a class). A build starts on the
  channel of its own release. If GitHub marks that release as a
  pre-release, the build follows Pre-releases from its first check.
  Otherwise it follows Stable.
- **Install updates automatically** is off by default, so qlcconsole asks
  first. When it is on, an update is downloaded and checked in the
  background as soon as it is found. The chip then says it will install
  when you quit, and it does.

### For IT: turning it off

Either of these turns update checks off completely. No request is made, and
**Check for Updates…** disappears from the Help menu:

- the environment variable `QLCCONSOLE_NO_UPDATES=1`, or
- the system-wide setting `updates/disabled = true` for organisation
  `qlcplus`, application `qlcconsole`. On macOS that is
  `sudo defaults write /Library/Preferences/org.qlcplus.qlcconsole updates.disabled -bool true`.
  On Windows it is the `HKEY_LOCAL_MACHINE\Software\qlcplus\qlcconsole\updates`
  key, value `disabled` = `true`. On Linux it is `/etc/xdg/qlcplus/qlcconsole.conf`
  with `[updates]` and `disabled=true`.

People cannot turn these back on from Settings.

## How each computer is updated

| Computer | What happens |
|---|---|
| **macOS** | Downloads the disk image and checks its SHA-256. Copies the new app next to the old one, then checks its code signature: it must pass Gatekeeper and come from the same developer (Team ID) as the running app. When you quit, a small helper swaps the apps. The old app is kept until the new one has started once. |
| **macOS, no write access** | If qlcconsole is in a folder you cannot change (for example **Applications** on a school Mac without an admin account), it downloads and checks the disk image, then **Open Download** opens it so you can install by hand. |
| **Windows** | Downloads the installer and checks its SHA-256. After you quit, the installer runs silently into the same folder, then qlcconsole reopens if you restarted. |
| **Linux, Raspberry Pi** | Tells you about the update only. **Open Release Page** links to it. If you built from source (Debian, Ubuntu, Raspberry Pi 5), update your checkout with `git pull` and rebuild. |

qlcconsole refuses to install an update if it does not match:

- its download must be listed in the release's `SHA256SUMS` with the same
  checksum, and must be the size GitHub lists;
- on macOS, the new app's signature, developer and bundle identifier must
  match the running app.

A copy you built yourself is not signed as a release, so on macOS it is never
replaced automatically. Use the release page instead.

## Privacy

The only thing qlcconsole sends is a request for the public list of releases
(`api.github.com/repos/sandinak/qlcconsole/releases`), plus the download
itself when you install. It sends no account, no identifier, and nothing
about your computer or your show. The answer is cached. A repeat check asks
"anything new?" (with an ETag), so it counts against nothing. If GitHub asks
qlcconsole to wait (rate limit), it waits until the time GitHub gives. If
you are offline, the check gives up quietly.

<p align="center">
  <img src="resources/icons/png/qlcconsole.png" alt="qlcconsole Logo" height="60" />
</p>

<h1 align="center">qlcconsole</h1>
<p align="center"><em>A console-first fork of QLC+, for macOS.</em></p>
<p align="center">
  <strong>Lighting control built around running a show live</strong> —
  mouse, joystick, and MIDI control surfaces (OpenDeck PMJ Black 1 today;
  APC40 mk2 and Xbox controller planned).<br/>
  Not affiliated with, and not a replacement for, the upstream QLC+ project.
</p>

<p align="center">
  <a href="https://github.com/sandinak/qlcconsole/releases/latest">
    <img src="https://img.shields.io/github/v/release/sandinak/qlcconsole" alt="Latest release version badge" /></a>
  <a href="https://github.com/sandinak/qlcconsole/commits/main/">
    <img src="https://img.shields.io/github/commit-activity/w/sandinak/qlcconsole" alt="Weekly commit activity badge" /></a>
  <a href="https://github.com/sandinak/qlcconsole/blob/main/COPYING">
    <img alt="License badge" src="https://img.shields.io/github/license/sandinak/qlcconsole?style=flat-square" /></a>
</p>

---

## This is a fork

qlcconsole is a fork of [QLC+](https://github.com/mcallegari/qlcplus) 4.14.2
(Q Light Controller Plus), the excellent open-source lighting control
software originally created by Heikki Junnila and developed since by Massimo
Callegari and the QLC+ community. **qlcconsole is not affiliated with the
QLC+ project** — it's a personal fork that takes QLC+ in a different, more
opinionated direction. If you want a general-purpose, cross-platform lighting
controller with a large community and an active forum, use
[QLC+](https://www.qlcplus.org/) itself.

qlcconsole exists because I wanted a **programmer-mode workflow** for
building and running looks live from a console, driven by a mouse, a
joystick, and a MIDI control surface — not primarily from a
patched-together virtual console. It adds a dedicated **Programming** tab
(palettes → looks → group scenes, with live preview), an explicit show-length
model, an effect-scripting system, and Fixture Manager tooling for building
fixture groups quickly. The **Connections** tab is a full rework of QLC+'s
I/O manager into a strict, hierarchical device tree (host → protocol →
interface → target → port → universe) with patch undo and portable patches
that survive opening a workspace on a machine missing the network it was
built for. A device-agnostic **control-surface engine** drives hardware
control surfaces (an OpenDeck PMJ Black 1 overlay today), and boot now runs
through one consistent startup window — name/version, a live log of what's
loading, a progress bar — instead of a bare, platform-inconsistent dialog.
See [CHANGELOG.md](CHANGELOG.md) for a user-facing summary of what's
changed, or [DONE.md](DONE.md)/[TODO.md](TODO.md) for the detailed
development log.

**Platform support:** macOS first. The upstream QLC+ codebase this is forked
from supports Linux and Windows too, but qlcconsole's own testing, packaging,
and releases currently target macOS only — see [RELEASE.md](RELEASE.md).

## Building qlcconsole

CMake, built out-of-tree in `build/`. `find_package(QT NAMES Qt5 Qt6 ...)`
auto-detects whichever Qt is installed (Homebrew `qt` gives Qt6; `qt@5` also
works) — CI and this fork's own releases build against Qt6:

```sh
# Reconfigure only when CMakeLists/sources are added or removed:
cmake -S . -B build

# Incremental build (parallel). Run after editing sources: performance
# cores only, not hw.ncpu — on Apple Silicon that also counts efficiency
# cores, which saturating slows down everything else running on the machine.
cmake --build build -j"$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || sysctl -n hw.ncpu)"

# Run. Always pass -o to open a workspace explicitly.
build/main/qlcconsole -o your-workspace.qxw
```

See [CLAUDE.md](CLAUDE.md) for the fuller architecture/build reference, and
[RELEASE.md](RELEASE.md) for how signed macOS releases get built and
published.

## Contributing

This is primarily a personal fork built around one person's show workflow,
so the bar for outside contributions is different than upstream's. If you'd
like to contribute, see [CONTRIBUTING.md](CONTRIBUTING.md) — most of the
underlying C++/Qt engineering guidelines still come from upstream QLC+ and
apply here too.

## Support

See [SUPPORT.md](SUPPORT.md). Fork-specific bugs and questions go to
[this repo's issues](https://github.com/sandinak/qlcconsole/issues); general
QLC+ usage questions are better served by the
[upstream QLC+ forum and docs](https://www.qlcplus.org/forum/), since that's
where the broader knowledge base lives.

## Thank you

qlcconsole is built entirely on top of the work of the QLC+ and Q Light
Controller communities. The following list recognizes the contributors whose
work this fork inherits and builds on.

<details>
<summary>QLC+ 5</summary>

*   Eric Arnebäck (3D preview features)
*   Santiago Benejam Torres (Catalan translation)
*   Luis García Tornel (Spanish translation)
*   Nils Van Zuijlen, Jérôme Lebleu (French translation)
*   Felix Edelmann, Florian Edelmann (fixture definitions, German translation)
*   Jannis Achstetter (German translation)
*   Dai Suetake (Japanese translation)
*   Hannes Bossuyt (Dutch translation)
*   Aleksandr Gusarov (Russian translation)
*   Vadim Syniuhin (Ukrainian translation)
*   Mateusz Kędzierski + smaks6 (Polish translation)

</details>

<details>
<summary>QLC+ 4</summary>

*   Jano Svitok (bugfix, new features and improvements)
*   David Garyga (bugfix, new features and improvements)
*   Lukas Jähn (bugfix, new features)
*   Robert Box (fixtures review)
*   Thomas Achtner (ENTTEC wing improvements)
*   Joep Admiraal (MIDI SysEx init messages, Dutch translation)
*   Florian Euchner (FX5 USB DMX support)
*   Stefan Riemens (new features)
*   Bartosz Grabias (new features)
*   Simon Newton, Peter Newman (OLA plugin)
*   Janosch Frank (webaccess improvements)
*   Karri Kaksonen (DMX USB Eurolite USB DMX512 Pro support)
*   Stefan Krupop (HID DMXControl Projects e.V. Nodle U1 support)
*   Nathan Durnan (RGB scripts, new features)
*   Giorgio Rebecchi (new features)
*   Florian Edelmann (code cleanup, German translation)
*   Heiko Fanieng, Jannis Achstetter (German translation)
*   NiKoyes, Jérôme Lebleu, Olivier Humbert, Nils Van Zuijlen (French translation)
*   Raymond Van Laake (Dutch translation)
*   Luis García Tornel (Spanish translation)
*   Jan Lachman (Czech translation)
*   Nuno Almeida, Carlos Eduardo Porto de Oliveira (Portuguese translation)
*   Santiago Benejam Torres (Catalan translation)
*   Koichiro Saito, Dai Suetake (Japanese translation)

</details>

<details>
<summary>Q Light Controller</summary>

*   Stefan Krumm (Bugfixes, new features)
*   Christian Suehs (Bugfixes, new features)
*   Christopher Staite (Bugfixes)
*   Klaus Weidenbach (Bugfixes, German translation)
*   Lutz Hillebrand (uDMX plugin)
*   Matthew Jaggard (Velleman plugin)
*   Ptit Vachon (French translation)

</details>

---

<p align="center">
<a href="https://github.com/mcallegari/qlcplus/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=mcallegari/qlcplus" />
</a>
</p>

---

## License

<a href="https://github.com/sandinak/qlcconsole/blob/main/COPYING">
  <img alt="GitHub License badge" src="https://img.shields.io/github/license/sandinak/qlcconsole?style=flat-square" />
</a>

Licensed under the **Apache 2.0** License. See [COPYING](COPYING) for the
full text.

QLC+ and Q Light Controller portions: Copyright © Heikki Junnila, Massimo
Callegari, and the [QLC+ contributors](https://github.com/mcallegari/qlcplus/blob/master/COPYING).
qlcconsole additions: Copyright © Branson Matheson.

---
<p align="center">
  <img src="https://img.shields.io/badge/c++-%2300599C.svg?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++ badge" />
  <img src="https://img.shields.io/badge/Qt-%23217346.svg?style=for-the-badge&logo=Qt&logoColor=white" alt="Qt badge" />
  <img src="https://img.shields.io/badge/CMake-%23008FBA.svg?style=for-the-badge&logo=cmake&logoColor=white" alt="CMake badge" />
</p>

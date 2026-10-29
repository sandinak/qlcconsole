# Support

[qlcconsole](https://github.com/sandinak/qlcconsole) is a fork of QLC+ 4,
maintained by one person and **not affiliated with the upstream QLC+
project**. Questions, bugs and requests about qlcconsole, including how
QLC+ 4 features behave in it, go to
[this repo's issues](https://github.com/sandinak/qlcconsole/issues), not to
the upstream forum: upstream's developers shouldn't have to answer for a
fork.

## What qlcconsole promises QLC+ 4 users

- **Your shows are safe here.** A show QLC+ 4.14 opens, qlcconsole opens,
  plays and saves without changing it unless you ask. Stock QLC+ 4.14 can
  open it again; it skips what only qlcconsole has (the Show Tree, venues,
  palettes), and loses those if it saves the file; qlcconsole names what
  it would lose when you save a .qxw. Every build's test gate checks that
  QLC+ 4.14.4's own engine opens a .qxw saved here.
- **The Classic QLC+ view stays**: QLC+ 4's tabs, names and toolbars. New
  features never require leaving it to keep a show working.
- **Fixture definitions keep arriving**: definitions from QLC+ upstream are
  brought in every release, and upstream fixes are reviewed each release
  and logged in [docs/dev/UPSTREAM.md](docs/dev/UPSTREAM.md).
- **macOS, Windows and Linux builds**, on Qt 6, for as long as the project
  is maintained. Security fixes and fixes for OS updates that break the app
  come first. A Raspberry Pi builds from source but isn't packaged yet.
- **If maintenance stops, this page says so**, and the last release stays
  downloadable with its source.

QLC+ 4.x is in maintenance upstream; qlcconsole keeps its widgets UI going.
If you use QLC+ 5 and it works for you, keep using it.

## Where to Get Help

- **qlcconsole:** [GitHub Issues](https://github.com/sandinak/qlcconsole/issues)
  for anything you hit in this app, the QLC+ 4 parts included. The in-app
  help (**F1**) and the [user guide](docs/features/README.md) cover the
  qlcconsole features.
- **QLC+ reference:** the [QLC+ documentation](https://www.qlcplus.org/docs/)
  still describes the stock 4.x features (fixtures, DMX/Art-Net/sACN
  setup, virtual console widgets) well.
- **Professional Support and Training** (an upstream offering, not this
  fork): [Book a session](https://qlcplus-merch.myshopify.com/products/1h-expert-qlc-consulting-session)

## Before You Ask for Help

Please:

- Search [this fork's issues](https://github.com/sandinak/qlcconsole/issues)
  first.
- Check the [user guide](docs/features/README.md), the
  [QLC+ documentation](https://www.qlcplus.org/docs/) for stock 4.x
  features, or [CLAUDE.md](CLAUDE.md)/[DONE.md](docs/project/DONE.md)/[TODO.md](docs/project/TODO.md)
  for this fork's own architecture and development log.
- Make sure you're on the latest qlcconsole build — check
  Help → About for the version string
  (`4.14.4+qlcconsole.<N>`, where `N` tracks this fork's own commit count;
  builds before 0.3.1 say `4.14.2+qlcconsole.<N>`).

## How to Report Issues Effectively

When reporting a bug, include the following:

- **qlcconsole version:** from Help → About, e.g. `4.14.4+qlcconsole.2390`
- **Operating System:** e.g., Windows 11, Ubuntu 22.04, macOS Ventura
- **Steps to Reproduce:** Clear instructions to trigger the issue
- **Expected Behavior:** What you expected to happen
- **Actual Behavior:** What actually happened
- **Additional Info:** Logs, screenshots, or sample files if relevant

Providing detailed information helps diagnose and fix issues faster.

## Contributing to Support

- Fork-specific: open an issue or a PR against
  [sandinak/qlcconsole](https://github.com/sandinak/qlcconsole) — see
  [CONTRIBUTING.md](CONTRIBUTING.md).
- QLC+ itself: contribute upstream at
  [mcallegari/qlcplus](https://github.com/mcallegari/qlcplus); fixes that
  matter to both go upstream too.

## Need Professional Support?

For advanced QLC+ consulting, custom development, or priority support (an
upstream offering, not run by this fork), [book a session here](https://qlcplus-merch.myshopify.com/products/1h-expert-qlc-consulting-session).

## Disclaimer

qlcconsole is a personal fork maintained by one person in their spare time —
response times will vary. The project's direction is in
[docs/project/STRATEGY.md](docs/project/STRATEGY.md). QLC+ itself is a much larger open-source project
maintained by volunteers; thank you for being part of that community too.

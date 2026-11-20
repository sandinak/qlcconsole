# Security

## Supported versions

Security fixes go into the newest release only. Update from inside the app
(**Help ▸ Check for Updates…**) or from the
[releases page](https://github.com/sandinak/qlcconsole/releases).

| Version | Supported |
|---|---|
| newest release (0.5.x) | yes |
| anything older | no: update first |

## Reporting a vulnerability

Please report privately, not in a public issue:
**[open a private report](https://github.com/sandinak/qlcconsole/security/advisories/new)**
(the repository's **Security** tab ▸ **Report a vulnerability**). Only the
maintainer sees it.

Include what you can:

- the qlcconsole version (**Help ▸ About**) and the operating system;
- what an attacker can do, and from where (the same computer, the local
  network, the internet);
- steps or a show file that shows it. Strip anything private from the show
  first.

qlcconsole is kept by one person. Expect an answer within a week. A
confirmed problem gets a fix in the next release, sooner if it is being
used. You are credited in the release notes unless you'd rather not be.
There is no bug bounty.

## What counts

- Anything that lets someone run code, read or change files, or take
  control of the console through a show file, a fixture or profile file,
  the web pages, OSC, MIDI, or the update check.
- Anything that stops the emergency stop, Panic or the Grand Master from
  working, or brings lights up that should be held. On a stage that is a
  safety problem, so report it here even if it needs no attacker.
- A download or update that could be swapped without the app noticing.
  Releases list their checksums in `SHA256SUMS`, and the in-app updater
  refuses a file whose checksum is not listed there.

## What doesn't

- **Lighting network protocols have no security of their own.** Art-Net,
  sACN and OSC trust anything on the network by design. Keep the show
  network separate from the internet and from guest Wi-Fi. That is not a
  qlcconsole bug, but a crash or code run caused by a bad packet is.
- **The Windows installer is not code-signed yet**, so Windows SmartScreen
  warns on first run. Check the download against the release's
  `SHA256SUMS`.
- **Problems in QLC+ itself**, also present in upstream QLC+ 4, are fixed
  here too. Please also tell
  [the QLC+ project](https://github.com/mcallegari/qlcplus/security), or
  say in your report that we may.

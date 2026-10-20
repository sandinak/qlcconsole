---
title: Install qlcconsole
nav_order: 3
---

# Install qlcconsole

qlcconsole is free. It runs on **Mac**, **Windows** and **Linux**
computers. You don't need any lights: the course stage is on the screen.

**Download qlcconsole 0.2.0 (or later) from the
[qlcconsole Releases page](https://github.com/sandinak/qlcconsole/releases).**
The lessons are checked on 0.2.0.
The course uses the newest **pre-release** whose name starts with
**Show Tree** (it has a *Pre-release* badge; the "Latest" release above it
may be an older build without the Show Tree). Under its **Assets**, pick
the file for your computer:

| Your computer | File to download | Ends in |
|---|---|---|
| Mac (macOS 12 or later; Apple or Intel chip) | `qlcconsole-…-universal-….dmg` | `.dmg` |
| Windows 10 or 11 (64-bit) | `qlcconsole-…-win64-setup.exe` | `.exe` |
| Linux (64-bit Intel/AMD) | `qlcconsole-…-x86_64.AppImage` | `.AppImage` |
| Chromebook | see below | |

## Mac

1. Open the `.dmg` you downloaded.
2. Drag **qlcconsole** into **Applications**.
3. Open it from Applications. (It is signed by its developer, so macOS lets
   it open. If macOS still asks, click **Open**.)

## Windows

1. Run the `…-win64-setup.exe` you downloaded.
2. Windows may say **"Windows protected your PC"** (the installer isn't
   signed yet). Click **More info → Run anyway**.
3. Follow the installer, then open **qlcconsole** from the Start menu.

On a school-managed laptop you may not be allowed to install programs: ask
your teacher.

## Linux

1. Make the file runnable: right-click it → Properties → *Allow executing*,
   or in a terminal `chmod +x qlcconsole-*.AppImage`.
2. Double-click it (or run `./qlcconsole-*.AppImage`).

## Chromebook

A Chromebook can run it only if **both** are true:

- its **Linux development environment** is turned on (Settings → Advanced →
  Developers). Many school Chromebooks don't allow this;
- it has an **Intel or AMD** chip (Settings → About ChromeOS → Additional
  details). The Linux version doesn't run on ARM Chromebooks.

If so, download the `.AppImage`, move it into **Linux files**, and follow
the Linux steps in the Linux terminal. If not, do the **paper version** of
each homework (every lesson's *At home* page has one).

## Get the course files

Each lesson links its show files (`.qlcc`). On GitHub: click the file,
then the **Download raw file** button (↓). In qlcconsole: **File → Open**
and pick it.

Open shows with **File → Open**. Save your own work with **File → Save
As** under a new name, so the course file stays as it was.

## Saving your work

Save your shows **on your own computer**. qlcconsole has no folder of its
own for shows: the first time, the Save dialog starts wherever your
computer starts; after that it remembers the last folder you used. So make
one folder and always save there:

| Computer | Make this folder |
|---|---|
| Mac | **Documents → qlcconsole** (in Finder: Documents, then File → New Folder) |
| Windows | **Documents\\qlcconsole** (`C:\Users\yourname\Documents\qlcconsole`) |
| Linux | `~/Documents/qlcconsole` |
| Chromebook (Linux on) | **Linux files → qlcconsole** |

Name files `Lesson01-yourname.qlcc`. A `.qlcc` file carries everything the
show needs, so it opens on any computer.

**Bringing a show to class:** copy the `.qlcc` to a USB stick, upload it to
the school's cloud drive (Google Drive / OneDrive), or email it to
yourself. In class, download it and **File → Open** it.

## Is it safe?

Yes. The course shows aren't connected to any real lights or network.
The footer will say **Not ready** in red for that reason — that's
expected.

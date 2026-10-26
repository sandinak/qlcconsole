---
title: Install qlcconsole
nav_order: 3
---

# Install qlcconsole

qlcconsole is free and runs on **Mac**, **Windows** and **Linux**. You
don't need any lighting hardware: the course stage is a 3D model on
screen.

**Download qlcconsole 0.3.0 (or later) from the
[qlcconsole Releases page](https://github.com/sandinak/qlcconsole/releases).**
The lessons are checked against 0.3.0. Take the newest release numbered
0.3.0 or higher: a full release is named **qlcconsole 0.3.0**, a
pre-release **Show Tree 0.3.0 (beta)** with a *Pre-release* badge; either
works. Under its **Assets**, pick the file for your computer:

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

School-managed laptops often block installing programs; if yours does,
ask your teacher.

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

If both are true, download the `.AppImage`, move it into **Linux files**
and follow the Linux steps in the Linux terminal. If not, use the
**paper version** of each at-home task (every lesson's *At home* page has
one).

## Get the course files

Each lesson links its show files (`.qlcc`). On GitHub: click the file,
then the **Download raw file** button (↓). In qlcconsole: **File → Open**
and pick it.

Save your own work with **File → Save As** under a new name, so the
course file stays unchanged.

## Saving your work

Save your shows **on your own computer**. qlcconsole has no shows folder
of its own: the first time, the Save dialog opens in your computer's
default location, and after that it remembers the last folder you used.
Make one folder for the course and always save there:

| Computer | Make this folder |
|---|---|
| Mac | **Documents → qlcconsole** (in Finder: Documents, then File → New Folder) |
| Windows | **Documents\\qlcconsole** (`C:\Users\yourname\Documents\qlcconsole`) |
| Linux | `~/Documents/qlcconsole` |
| Chromebook (Linux on) | **Linux files → qlcconsole** |

Name files like `Lesson01-yourname.qlcc`. A `.qlcc` file carries
everything the show needs, so it opens on any computer.

**Bringing a show to class:** copy the `.qlcc` to a USB stick, upload it to
the school's cloud drive (Google Drive / OneDrive), or email it to
yourself. In class, download it and **File → Open** it.

## Real lights and the network

The course shows aren't connected to any real lights and send nothing
on the network. That's why the footer reads **No outputs: preview only**,
in grey.

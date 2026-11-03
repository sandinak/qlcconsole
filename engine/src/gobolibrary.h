/*
  Q Light Controller Plus
  gobolibrary.h

  Where gobo pictures come from: the operator's own folder first (a logo, a
  show's custom glass, a breakup), then the library bundled with the app.

  A swapped slot (MonitorProperties::setGoboSwap) names its picture by FILE
  NAME when it lives in the user folder, so a workspace keeps working on
  another machine that has the same folder; anything else is kept as the
  path it was given.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef GOBOLIBRARY_H
#define GOBOLIBRARY_H

#include <QString>
#include <QStringList>
#include <QDir>
#include <QList>

class QLCChannel;
class QLCCapability;

namespace GoboLibrary
{
    /** The operator's gobo folder (created if missing):
     *  Gobos in the per-platform user folder (~/Library/Application Support/
     *  qlcconsole on macOS, %USERPROFILE%\qlcconsole on Windows, ~/.qlcconsole
     *  on Linux). */
    QDir userDirectory();

    /** Point userDirectory() somewhere else (tests); empty restores it. */
    void setUserDirectoryOverride(const QString &path);

    /** Image files the pictures may be: svg, png, jpg... as name filters. */
    QStringList imageFilters();

    /** The pictures in the user folder, file names, sorted. */
    QStringList userGobos();

    /** A full path for a swap's picture: an absolute path as it is (when it
     *  is a file), a bare name looked up in the user folder, then the bundled
     *  library. An absolute path that is not there but runs through a
     *  ".../Gobos/" folder (a definition saved by another install, e.g.
     *  /Applications/QLC+.app/.../Resources/Gobos/x/y.svg) is looked for
     *  under the same tail in the bundled library. Empty if it is nowhere,
     *  or is a folder. */
    QString resolve(const QString &image);

    /** True when @p file (a full path) is one of the app's own gobos: every
     *  computer with the app has it, so a show need not carry it. */
    bool isBundled(const QString &file);

    /** @p file as a swap should store it: a bundled library picture by its
     *  path in the library ("Chauvet/gobo00026.svg"), so it is found on any
     *  computer with the app; anything else as it is. */
    QString portableName(const QString &file);

    /** Bring @p path into the user folder (copying it unless it is already
     *  there, never overwriting a different file of the same name) and return
     *  the name to store. Empty on failure. */
    QString import(const QString &path);

    /** @name Slots a definition names but draws no picture for
     *  A Gobo channel with no pictures that still names its slots ("3 Spots",
     *  "Zebra"): the rig view draws a generated stand-in for each, and Show
     *  Review notes it. */
    //@{
    /** Whether @p cap carries a usable picture: a Picture preset whose
     *  Res1 resolves (resolve()) to an image file. A bare folder, a file
     *  that isn't there and a file that isn't an image are not pictures.
     *  Cached by path. */
    bool hasPicture(const QLCCapability *cap);
    /** Why a Picture-preset @p cap has none. */
    enum PictureProblem { PictureOk, NoPicture, PointsAtFolder, PictureMissing };
    PictureProblem pictureProblem(const QLCCapability *cap);
    /** The capability turns the gobo or the wheel (a rotation preset, or a
     *  name saying rotate/spin/index) rather than naming a slot. */
    bool isRotationCap(const QLCCapability *cap);
    /** A rotation preset (not just a name). */
    bool isRotationPreset(int preset);
    /** Shake: the GoboShakeMacro preset or "shake" in the name. */
    bool isShakeCap(const QLCCapability *cap);
    /** Place of @p cap's glass in standInGlass(@p ch); -1 if it is no stand-in. */
    int standInIndex(const QLCChannel *ch, const QLCCapability *cap);

    /** The slot name says no gobo: open, white, clear, empty... */
    bool isOpenSlotName(const QString &name);
    /** The range turns the gobo ("Gobo Rotate Speed"), it isn't a slot. */
    bool isRotationSlotName(const QString &name);
    /** @p name lower-cased, without "shake" and extra spaces: a slot and its
     *  shake variant are one piece of glass. */
    QString slotBaseName(const QString &name);
    /** Whether the slot name says it shakes. */
    bool isShakeName(const QString &name);
    /** The capabilities of @p ch that are gobo slots to stand in for (open
     *  and rotation ranges left out, and slots that have a picture), in
     *  channel order; empty unless @p ch is a Gobo channel and not itself a
     *  rotation channel. Cached per channel. */
    QList<QLCCapability *> standInCaps(const QLCChannel *ch);
    /** Distinct pieces of glass in standInCaps(), by slotBaseName(), in order:
     *  a slot's place in this list seeds its stand-in. */
    QStringList standInGlass(const QLCChannel *ch);
    //@}
}

#endif

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

namespace GoboLibrary
{
    /** The operator's gobo folder (created if missing):
     *  ~/Library/Application Support/qlcconsole/Gobos on macOS. */
    QDir userDirectory();

    /** Point userDirectory() somewhere else (tests); empty restores it. */
    void setUserDirectoryOverride(const QString &path);

    /** Image files the pictures may be: svg, png, jpg... as name filters. */
    QStringList imageFilters();

    /** The pictures in the user folder, file names, sorted. */
    QStringList userGobos();

    /** A full path for a swap's picture: an absolute path as it is, a bare
     *  name looked up in the user folder, then the bundled library. Empty if
     *  it is nowhere. */
    QString resolve(const QString &image);

    /** Bring @p path into the user folder (copying it unless it is already
     *  there, never overwriting a different file of the same name) and return
     *  the name to store. Empty on failure. */
    QString import(const QString &path);
}

#endif

/*
  Q Light Controller Plus
  gobolibrary.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QFileInfo>
#include <QFile>
#include <QCryptographicHash>

#include "gobolibrary.h"
#include "qlcconfig.h"
#include "qlcfile.h"

// The per-platform user folder, as fixture definitions (it was the Mac's on all).
#define USERGOBODIR USERQLCPLUSDIR "/Gobos"

namespace
{
    QString &overridePath()
    {
        static QString path;
        return path;
    }

    QByteArray digest(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return QByteArray();
        return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha1);
    }
}

QStringList GoboLibrary::imageFilters()
{
    return QStringList() << "*.svg" << "*.png" << "*.jpg" << "*.jpeg"
                         << "*.gif" << "*.bmp";
}

void GoboLibrary::setUserDirectoryOverride(const QString &path)
{
    overridePath() = path;
}

QDir GoboLibrary::userDirectory()
{
    if (!overridePath().isEmpty())
    {
        QDir dir(overridePath());
        if (!dir.exists())
            dir.mkpath(".");
        dir.setFilter(QDir::Files);
        dir.setNameFilters(imageFilters());
        return dir;
    }
    return QLCFile::userDirectory(QString(USERGOBODIR), QString(GOBODIR), imageFilters());
}

QStringList GoboLibrary::userGobos()
{
    QStringList names = userDirectory().entryList(QDir::Files, QDir::Name | QDir::IgnoreCase);
    return names;
}

QString GoboLibrary::resolve(const QString &image)
{
    if (image.isEmpty())
        return QString();
    const QFileInfo fi(image);
    if (fi.isAbsolute())
        return fi.exists() ? image : QString();
    const QString user = userDirectory().absoluteFilePath(image);
    if (QFileInfo::exists(user))
        return user;
    const QString sys = QLCFile::systemDirectory(GOBODIR).absoluteFilePath(image);
    if (QFileInfo::exists(sys))
        return sys;
    return QString();
}

QString GoboLibrary::import(const QString &path)
{
    const QFileInfo src(path);
    if (!src.exists() || !src.isFile())
        return QString();
    const QDir dir = userDirectory();
    if (QFileInfo(src.absolutePath()) == QFileInfo(dir.absolutePath()))
        return src.fileName();                       // already in the folder

    // Same name already there: reuse it if it's the same picture, otherwise
    // pick a free name rather than overwrite someone's other gobo.
    const QByteArray want = digest(path);
    QString name = src.fileName();
    for (int n = 2; QFileInfo::exists(dir.absoluteFilePath(name)); ++n)
    {
        if (digest(dir.absoluteFilePath(name)) == want)
            return name;
        name = QString("%1-%2.%3").arg(src.completeBaseName()).arg(n).arg(src.suffix());
    }
    if (!QFile::copy(path, dir.absoluteFilePath(name)))
        return QString();
    return name;
}

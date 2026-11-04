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
#include <QRegularExpression>
#include <QHash>
#include <QMutex>

#include "gobolibrary.h"
#include "qlcconfig.h"
#include "qlcfile.h"
#include "qlcchannel.h"
#include "qlccapability.h"

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
    {
        if (fi.isFile())
            return image;
        // Another install's library path: the same tail in ours.
        const int at = image.lastIndexOf(QLatin1String("/Gobos/"), -1, Qt::CaseInsensitive);
        if (at >= 0 && !image.endsWith(QLatin1Char('/')))
        {
            const QString ours = QLCFile::systemDirectory(GOBODIR).absoluteFilePath(image.mid(at + 7));
            if (QFileInfo(ours).isFile())
                return ours;
        }
        return QString();
    }
    const QString user = userDirectory().absoluteFilePath(image);
    if (QFileInfo::exists(user))
        return user;
    const QString sys = QLCFile::systemDirectory(GOBODIR).absoluteFilePath(image);
    if (QFileInfo::exists(sys))
        return sys;
    return QString();
}

bool GoboLibrary::isBundled(const QString &file)
{
    const QString lib = QFileInfo(QLCFile::systemDirectory(GOBODIR).absolutePath()).canonicalFilePath();
    const QString at = QFileInfo(file).canonicalFilePath();
    return !lib.isEmpty() && !at.isEmpty() && at.startsWith(lib + QLatin1Char('/'));
}

QString GoboLibrary::portableName(const QString &file)
{
    if (!isBundled(file))
        return file;
    const QString lib = QFileInfo(QLCFile::systemDirectory(GOBODIR).absolutePath()).canonicalFilePath();
    const QString at = QFileInfo(file).canonicalFilePath();
    return at.mid(lib.length() + 1);
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

bool GoboLibrary::isOpenSlotName(const QString &name)
{
    static const QRegularExpression rx(
        QStringLiteral("\\b(open|white|clear|empty|no\\s*gobo|none|blank)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return rx.match(name).hasMatch();
}

bool GoboLibrary::isRotationSlotName(const QString &name)
{
    static const QRegularExpression rx(QStringLiteral("rotat|\\bspin|\\bindex\\b"),
                                       QRegularExpression::CaseInsensitiveOption);
    return rx.match(name).hasMatch();
}

bool GoboLibrary::isShakeName(const QString &name)
{
    return name.contains(QLatin1String("shake"), Qt::CaseInsensitive);
}

QString GoboLibrary::slotBaseName(const QString &name)
{
    static const QRegularExpression shake(QStringLiteral("\\bshak\\w*"),
                                          QRegularExpression::CaseInsensitiveOption);
    QString out = name;
    out.remove(shake);
    return out.simplified().toLower();
}

bool GoboLibrary::isRotationPreset(int preset)
{
    switch (preset)
    {
        case QLCCapability::RotationStop:
        case QLCCapability::RotationIndexed:
        case QLCCapability::RotationClockwise:
        case QLCCapability::RotationClockwiseSlowToFast:
        case QLCCapability::RotationClockwiseFastToSlow:
        case QLCCapability::RotationCounterClockwise:
        case QLCCapability::RotationCounterClockwiseSlowToFast:
        case QLCCapability::RotationCounterClockwiseFastToSlow:
            return true;
        default:
            return false;
    }
}

bool GoboLibrary::isRotationCap(const QLCCapability *cap)
{
    return cap != nullptr && (isRotationPreset(cap->preset()) || isRotationSlotName(cap->name()));
}

bool GoboLibrary::isShakeCap(const QLCCapability *cap)
{
    return cap != nullptr && (cap->preset() == QLCCapability::GoboShakeMacro || isShakeName(cap->name()));
}

namespace
{
    QString pictureResource(const QLCCapability *cap)
    {
        if (cap == nullptr || cap->presetType() != QLCCapability::Picture)
            return QString();
        return const_cast<QLCCapability *>(cap)->resource(0).toString();
    }

    bool isImageFile(const QString &path)
    {
        const QString suffix = QFileInfo(path).suffix().toLower();
        foreach (const QString &f, GoboLibrary::imageFilters())
            if (f.mid(2) == suffix)
                return true;
        return false;
    }
}

GoboLibrary::PictureProblem GoboLibrary::pictureProblem(const QLCCapability *cap)
{
    const QString res = pictureResource(cap);
    if (res.isEmpty())
        return NoPicture;
    if (res.endsWith(QLatin1Char('/')) || QFileInfo(res).isDir())
        return PointsAtFolder;
    const QString path = resolve(res);
    return (!path.isEmpty() && isImageFile(path)) ? PictureOk : PictureMissing;
}

bool GoboLibrary::hasPicture(const QLCCapability *cap)
{
    const QString res = pictureResource(cap);
    if (res.isEmpty())
        return false;
    // Asked from the rig view's frames: a file's stat once, not per frame.
    static QHash<QString, bool> cache;
    static QMutex lock;
    {
        QMutexLocker locker(&lock);
        auto it = cache.constFind(res);
        if (it != cache.constEnd())
            return it.value();
    }
    const bool ok = pictureProblem(cap) == PictureOk;
    QMutexLocker locker(&lock);
    if (cache.size() > 4096)
        cache.clear();
    cache.insert(res, ok);
    return ok;
}

namespace
{
    struct StandIns
    {
        QList<QLCCapability *> snapshot;     ///< the channel's capabilities when worked out
        QList<QLCCapability *> caps;
        QStringList glass;
        QHash<const QLCCapability *, int> index;
    };

    StandIns computeStandIns(const QLCChannel *ch)
    {
        StandIns out;
        if (ch == nullptr || ch->group() != QLCChannel::Gobo
            || ch->name().contains(QLatin1String("rotat"), Qt::CaseInsensitive))
            return out;
        static const QRegularExpression notSlot(
            QStringLiteral("\\b(reserved|unused|not used|reset|macro|change|auto\\w*)\\b"),
            QRegularExpression::CaseInsensitiveOption);
        foreach (QLCCapability *cap, ch->capabilities())
        {
            if (GoboLibrary::isRotationCap(cap) || GoboLibrary::hasPicture(cap)
                || notSlot.match(cap->name()).hasMatch())
                continue;
            const QString base = GoboLibrary::slotBaseName(cap->name());
            if (base.isEmpty() || GoboLibrary::isOpenSlotName(base))
                continue;
            out.caps << cap;
            int at = out.glass.indexOf(base);
            if (at < 0)
            {
                at = out.glass.size();
                out.glass << base;
            }
            out.index.insert(cap, at);
        }
        return out;
    }

    /** Worked out once per channel; redone if its capabilities change. */
    StandIns standInsOf(const QLCChannel *ch)
    {
        static QHash<const QLCChannel *, StandIns> cache;
        static QMutex lock;
        if (ch == nullptr)
            return StandIns();
        const QList<QLCCapability *> now = ch->capabilities();
        {
            QMutexLocker locker(&lock);
            auto it = cache.constFind(ch);
            if (it != cache.constEnd() && it->snapshot == now)
                return it.value();
        }
        StandIns made = computeStandIns(ch);
        made.snapshot = now;
        QMutexLocker locker(&lock);
        if (cache.size() > 1024)
            cache.clear();
        cache.insert(ch, made);
        return made;
    }
}

QList<QLCCapability *> GoboLibrary::standInCaps(const QLCChannel *ch)
{
    return standInsOf(ch).caps;
}

QStringList GoboLibrary::standInGlass(const QLCChannel *ch)
{
    return standInsOf(ch).glass;
}

int GoboLibrary::standInIndex(const QLCChannel *ch, const QLCCapability *cap)
{
    return standInsOf(ch).index.value(cap, -1);
}

/*
  Q Light Controller Plus
  effectscriptcache.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "effectscriptcache.h"
#include "qlcfile.h"
#include "qlcconfig.h"   // RGBSCRIPTDIR / USERRGBSCRIPTDIR

#include <QCoreApplication>
#include <QFileInfo>
#include <QRegularExpression>
#include <QFile>
#include <QDebug>

/* The user's own scripts: in the same per-platform user folder as fixture
   definitions (it was the Mac's path on every platform, so on Windows and
   Linux a script of your own was never found). The system folder comes from
   the build (EFFECTSCRIPTDIR, qlcconfig.h). */
#define USEREFFECTSCRIPTDIR USERQLCPLUSDIR "/EffectScripts"

namespace
{
    QString s_systemOverride, s_systemRgbOverride;
}

void EffectScriptCache::setSystemDirectoriesOverride(const QString &effects, const QString &rgb)
{
    s_systemOverride = effects;
    s_systemRgbOverride = rgb;
}

EffectScriptCache::EffectScriptCache(QObject *parent)
    : QObject(parent)
{
}

void EffectScriptCache::rescan()
{
    m_scripts.clear();
    m_displayNames.clear();
    // The open show's own copies before anything: its version is the one
    // used until the operator chooses otherwise.
    if (!m_showDir.isEmpty())
        scanDir(QDir(m_showDir));
    // User dir FIRST: scanDir() is first-found-wins (keyed on display name), so
    // scanning the system dir first meant a user's edited copy of a shipped
    // script was silently discarded and the shipped one kept loading. This
    // matches EffectPresetCache::rescan() and BundleCache::rescan().
    scanDir(userScriptsDirectory());
    scanDir(systemScriptsDirectory());
    // RGBScript library (QLC+ RGB Matrix scripts) — run through the EffectScript
    // compat host. Scanned AFTER effect scripts so a purpose-built effect of the
    // same name wins (first-found-wins keyed on display name). The show's own
    // copies first among them.
    if (!m_showRgbDir.isEmpty())
        scanDir(QDir(m_showRgbDir), true);
    scanRgbDir(userRgbScriptsDirectory());
    scanRgbDir(systemRgbScriptsDirectory());
    for (const QDir &d : m_extraDirs)
        scanDir(d);
    for (const QDir &d : m_extraRgbDirs)
        scanRgbDir(d);
    // The show's folder: only what nothing above had.
    if (!m_fallbackDir.isEmpty())
        scanDir(QDir(m_fallbackDir));
    if (!m_fallbackRgbDir.isEmpty())
        scanDir(QDir(m_fallbackRgbDir), true);
}

void EffectScriptCache::setShowDirectories(const QString &effects, const QString &rgb,
                                           const QString &fallbackEffects,
                                           const QString &fallbackRgb)
{
    m_showDir = effects;
    m_showRgbDir = rgb;
    m_fallbackDir = fallbackEffects;
    m_fallbackRgbDir = fallbackRgb;
    rescan();
}

void EffectScriptCache::addDirectory(const QDir &dir, bool rgb)
{
    QList<QDir> &dirs = rgb ? m_extraRgbDirs : m_extraDirs;
    if (!dirs.contains(dir))
        dirs.append(dir);
    rescan();
}

QStringList EffectScriptCache::scriptNames() const
{
    QStringList names = m_displayNames;
    names.sort(Qt::CaseInsensitive);
    return names;
}

QString EffectScriptCache::scriptPath(const QString &name) const
{
    return m_scripts.value(name).path;
}

EffectScriptCache::ScriptMeta EffectScriptCache::scriptMeta(const QString &name) const
{
    return m_scripts.value(name);
}

QString EffectScriptCache::nameFromPath(const QString &path)
{
    return QFileInfo(path).completeBaseName();
}

QString EffectScriptCache::categoryForTypes(const QStringList &ft)
{
    // A pixel-matrix effect ("matrix" beside the fixtures it runs on) is
    // listed with its kind, not under the first fixture type it names.
    if (ft.contains(QStringLiteral("matrix")))
        return QStringLiteral("Matrix");
    if (ft.contains(QStringLiteral("dimmer")))
        return QStringLiteral("Dimmer");
    if (ft.contains(QStringLiteral("rgb"))   || ft.contains(QStringLiteral("rgbw"))
        || ft.contains(QStringLiteral("color")) || ft.contains(QStringLiteral("colorwheel")))
        return QStringLiteral("Color");
    if (ft.contains(QStringLiteral("moving")) || ft.contains(QStringLiteral("mover"))
        || ft.contains(QStringLiteral("pantilt")))
        return QStringLiteral("Position");
    return QStringLiteral("Other");
}

QDir EffectScriptCache::systemScriptsDirectory()
{
    if (!s_systemOverride.isEmpty())
        return QDir(s_systemOverride, QStringLiteral("*.js"));
    return QLCFile::systemDirectory(QString(EFFECTSCRIPTDIR), QString(".js"));
}

QDir EffectScriptCache::userScriptsDirectory()
{
    return QLCFile::userDirectory(QString(USEREFFECTSCRIPTDIR),
                                  QString(EFFECTSCRIPTDIR),
                                  QStringList() << "*.js");
}

QDir EffectScriptCache::systemRgbScriptsDirectory()
{
    if (!s_systemRgbOverride.isEmpty())
        return QDir(s_systemRgbOverride, QStringLiteral("*.js"));
    return QLCFile::systemDirectory(QString(RGBSCRIPTDIR), QString(".js"));
}

QDir EffectScriptCache::userRgbScriptsDirectory()
{
    return QLCFile::userDirectory(QString(USERRGBSCRIPTDIR), QString(RGBSCRIPTDIR),
                                  QStringList() << "*.js");
}

void EffectScriptCache::scanRgbDir(const QDir &dir)
{
    if (!dir.exists())
        return;
    scanDir(dir, true);   // .js directly in the dir (packaged layout)
    // Dev build symlinks the source dir one level down (Resources/RGBScripts/
    // rgbscripts -> resources/rgbscripts); follow immediate subdirs too.
    const QStringList subs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);
    for (const QString &s : subs)
        scanDir(QDir(dir.absoluteFilePath(s)), true);
}

void EffectScriptCache::scanDir(const QDir &dir, bool rgb)
{
    if (!dir.exists() || !dir.isReadable())
        return;

    const QStringList files = dir.entryList(QStringList() << "*.js",
                                             QDir::Files | QDir::Readable);
    for (const QString &file : files)
    {
        const QString absPath = dir.absoluteFilePath(file);
        if (QLCFile::isOnlineOnly(absPath))   // in cloud storage, not on this machine: do not wait for it
        {
            QLCFile::noteSkippedOnlineOnly(absPath);
            continue;
        }
        ScriptMeta meta = parseMeta(absPath);
        meta.rgb = rgb;
        if (meta.displayName.isEmpty())
            meta.displayName = nameFromPath(absPath);  // fallback to filename
        /* An RGB Matrix script whose name an effect script already has is
           listed as itself, not dropped: both libraries have a Fire, a
           Plasma, a Meteor... and the RGB Matrix one (a flame rising up a
           grid) was simply never offered. */
        if (rgb && m_scripts.contains(meta.displayName)
            && m_scripts.value(meta.displayName).path != absPath
            && !m_scripts.value(meta.displayName).rgb)
            meta.displayName += QObject::tr(" (RGB Matrix)");
        // An RGB Matrix script is also found as "rgb:<file name>" -- what a
        // look saves it as -- whether or not its plain names are taken.
        if (rgb && !m_scripts.contains(QStringLiteral("rgb:") + nameFromPath(absPath)))
            m_scripts.insert(QStringLiteral("rgb:") + nameFromPath(absPath), meta);
        if (!meta.displayName.isEmpty() && !m_scripts.contains(meta.displayName))
        {
            m_scripts.insert(meta.displayName, meta);
            // The picker shows ONE name per script (the display name); filename
            // aliases below are for resolution only, not for the list. A
            // hidden script is never listed (a host's stand-in).
            if (!meta.hidden)
                m_displayNames.append(meta.displayName);
            // Also index by filename basename so XML round-trips work when
            // the display name (effect.name) differs from the .js filename.
            const QString baseName = nameFromPath(absPath);
            if (baseName != meta.displayName && !m_scripts.contains(baseName))
                m_scripts.insert(baseName, meta);
            qDebug() << "[EffectScriptCache] found:" << meta.displayName << "at" << absPath;
        }
    }
}

/** Extract a single-quoted or double-quoted string value for `effect.<field> = "..."` */
bool EffectScriptCache::isRgbScriptPath(const QString &path)
{
    // Its folder, or (a dev build's symlinked source dir) its folder's parent.
    const QDir dir = QFileInfo(path).dir();
    if (dir.dirName().compare(QLatin1String("rgbscripts"), Qt::CaseInsensitive) == 0)
        return true;
    QDir up = dir;
    return up.cdUp() && up.dirName().compare(QLatin1String("rgbscripts"), Qt::CaseInsensitive) == 0;
}

QString EffectScriptCache::portableName(const QString &path)
{
    const QString base = QFileInfo(path).completeBaseName();
    return isRgbScriptPath(path) ? QStringLiteral("rgb:") + base : base;
}

QString EffectScriptCache::scriptPathForWled(const QString &wledName) const
{
    if (wledName.isEmpty())
        return QString();
    for (const ScriptMeta &meta : m_scripts)
        if (meta.wled.compare(wledName, Qt::CaseInsensitive) == 0)
            return meta.path;
    return QString();
}

QString EffectScriptCache::extractField(const QString &src, const QString &field)
{
    // Match:  effect.fieldName  = "value"  or  = 'value'
    QRegularExpression re(
        QString("effect\\.%1\\s*=\\s*[\"']([^\"']*)[\"']").arg(
            QRegularExpression::escape(field)));
    const QRegularExpressionMatch m = re.match(src);
    if (m.hasMatch())
        return m.captured(1);
    return QString();
}

EffectScriptCache::ScriptMeta EffectScriptCache::parseMeta(const QString &path)
{
    ScriptMeta meta;
    meta.path = path;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return meta;
    const QString src = QString::fromUtf8(f.readAll());
    f.close();

    meta.displayName  = extractField(src, "name");
    meta.description  = extractField(src, "description");
    meta.notes        = extractField(src, "notes");
    meta.wled         = extractField(src, "wled");
    meta.hidden       = QRegularExpression(QStringLiteral("effect\\.hidden\\s*=\\s*true")).match(src).hasMatch();

    // RGBScript (QLC+ RGB Matrix API): no effect.name, but algo.name + rgbMap().
    // Run through the EffectScript compat host; treat as an RGB pixel effect.
    if (meta.displayName.isEmpty() &&
        src.contains(QStringLiteral("rgbMap")) &&
        !src.contains(QStringLiteral("effect.tick")))
    {
        QRegularExpression nameRe(
            QStringLiteral("\\balgo\\.name\\s*=\\s*[\"']([^\"']*)[\"']"));
        const QRegularExpressionMatch nm = nameRe.match(src);
        if (nm.hasMatch())
            meta.displayName = nm.captured(1);
        meta.fixtureTypes = QStringList() << QStringLiteral("rgb") << QStringLiteral("dimmer");
        if (meta.description.isEmpty())
            meta.description = QObject::tr("From the RGB Matrix library");
        return meta;   // fixtureTypes set explicitly; skip the effect.* parsing below
    }

    // Parse fixtureTypes array: effect.fixtureTypes = ["a", "b", ...]
    QRegularExpression ftRe("effect\\.fixtureTypes\\s*=\\s*\\[([^\\]]*)\\]");
    const QRegularExpressionMatch ftm = ftRe.match(src);
    if (ftm.hasMatch())
    {
        const QString inside = ftm.captured(1);
        QRegularExpression itemRe("[\"']([^\"']+)[\"']");
        QRegularExpressionMatchIterator it = itemRe.globalMatch(inside);
        while (it.hasNext())
            meta.fixtureTypes << it.next().captured(1);
    }

    return meta;
}

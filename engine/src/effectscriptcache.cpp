/*
  Q Light Controller Plus
  effectscriptcache.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "effectscriptcache.h"
#include "qlcfile.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QRegularExpression>
#include <QFile>
#include <QDebug>

// macOS bundle layout mirrors RGBScripts
#define EFFECTSCRIPTDIR     "Resources/EffectScripts"
#define USEREFFECTSCRIPTDIR "Library/Application Support/QLC+/EffectScripts"

EffectScriptCache::EffectScriptCache(QObject *parent)
    : QObject(parent)
{
}

void EffectScriptCache::rescan()
{
    m_scripts.clear();
    scanDir(systemScriptsDirectory());
    scanDir(userScriptsDirectory());
    for (const QDir &d : m_extraDirs)
        scanDir(d);
}

void EffectScriptCache::addDirectory(const QDir &dir)
{
    if (!m_extraDirs.contains(dir))
        m_extraDirs.append(dir);
    rescan();
}

QStringList EffectScriptCache::scriptNames() const
{
    return m_scripts.keys();
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

QDir EffectScriptCache::systemScriptsDirectory()
{
    return QLCFile::systemDirectory(QString(EFFECTSCRIPTDIR), QString(".js"));
}

QDir EffectScriptCache::userScriptsDirectory()
{
    return QLCFile::userDirectory(QString(USEREFFECTSCRIPTDIR),
                                  QString(EFFECTSCRIPTDIR),
                                  QStringList() << "*.js");
}

void EffectScriptCache::scanDir(const QDir &dir)
{
    if (!dir.exists() || !dir.isReadable())
        return;

    const QStringList files = dir.entryList(QStringList() << "*.js",
                                             QDir::Files | QDir::Readable);
    for (const QString &file : files)
    {
        const QString absPath = dir.absoluteFilePath(file);
        ScriptMeta meta = parseMeta(absPath);
        if (meta.displayName.isEmpty())
            meta.displayName = nameFromPath(absPath);  // fallback to filename
        if (!meta.displayName.isEmpty() && !m_scripts.contains(meta.displayName))
        {
            m_scripts.insert(meta.displayName, meta);
            qDebug() << "[EffectScriptCache] found:" << meta.displayName << "at" << absPath;
        }
    }
}

/** Extract a single-quoted or double-quoted string value for `effect.<field> = "..."` */
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

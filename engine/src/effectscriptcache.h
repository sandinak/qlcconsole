/*
  Q Light Controller Plus
  effectscriptcache.h

  Scans the well-known effect-script directories for .js files and
  maintains a name → absolute-path map that the UI and engine use.

  Search order (first found wins on name collision):
    1. <app>/Resources/EffectScripts/        — bundled
    2. ~/Library/…/QLC+/EffectScripts/       — user scripts
    3. <project-dir>/effects/                — project-local (added on load)

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef EFFECTSCRIPTCACHE_H
#define EFFECTSCRIPTCACHE_H

#include <QObject>
#include <QMap>
#include <QDir>
#include <QString>
#include <QStringList>

class EffectScriptCache : public QObject
{
    Q_OBJECT

public:
    /** Lightweight metadata extracted from each .js file without full JS eval. */
    struct ScriptMeta {
        QString path;
        QString displayName;    //!< effect.name
        QString description;    //!< effect.description (one-liner)
        QString notes;          //!< effect.notes (longer paragraph)
        QStringList fixtureTypes; //!< effect.fixtureTypes array
    };

    explicit EffectScriptCache(QObject *parent = nullptr);

    /** Scan all default directories and rebuild the cache. */
    void rescan();

    /** Add a project-local directory to the search set and rescan. */
    void addDirectory(const QDir &dir);

    /** Display names of all discovered scripts (sorted, from effect.name). */
    QStringList scriptNames() const;

    /** Absolute path for display @p name, or empty string if not found. */
    QString scriptPath(const QString &name) const;

    /** Full metadata for display @p name, or empty struct if not found. */
    ScriptMeta scriptMeta(const QString &name) const;

    /** Infer a script name from its file basename (strips .js). */
    static QString nameFromPath(const QString &path);

    // --- Standard search directories ---
    static QDir systemScriptsDirectory();
    static QDir userScriptsDirectory();

private:
    void scanDir(const QDir &dir);
    static ScriptMeta parseMeta(const QString &path);
    static QString extractField(const QString &src, const QString &field);

    QMap<QString, ScriptMeta> m_scripts;  // displayName → ScriptMeta
    QList<QDir>               m_extraDirs;
};

#endif // EFFECTSCRIPTCACHE_H

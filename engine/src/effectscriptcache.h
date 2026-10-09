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
        QString wled;           //!< effect.wled: the WLED effect it ports
        bool hidden = false;    //!< effect.hidden = true: resolvable, never listed
        bool rgb = false;       //!< from the RGB Matrix library
    };

    explicit EffectScriptCache(QObject *parent = nullptr);

    /** Scan all default directories and rebuild the cache. */
    void rescan();

    /** Add a project-local directory to the search set and rescan; @p rgb
     *  for a folder of RGB Matrix scripts. */
    void addDirectory(const QDir &dir, bool rgb = false);

    /** Display names of all discovered scripts (sorted, from effect.name). */
    QStringList scriptNames() const;

    /** Absolute path for display @p name, or empty string if not found. */
    QString scriptPath(const QString &name) const;

    /** Full metadata for display @p name, or empty struct if not found. */
    ScriptMeta scriptMeta(const QString &name) const;

    /** The script porting WLED's effect @p wledName ("Chase", any case), or
     *  an empty string when none does. */
    QString scriptPathForWled(const QString &wledName) const;

    /** Whether @p path is an RGB Matrix library script. A look saves one as
     *  "rgb:<file name>", because a file name alone can be an effect
     *  script's too (both libraries have a fire.js). */
    static bool isRgbScriptPath(const QString &path);
    /** The name a look saves @p path under: its file name, or for an RGB
     *  Matrix script "rgb:" and its file name. scriptPath() resolves both. */
    static QString portableName(const QString &path);

    /** Infer a script name from its file basename (strips .js). */
    static QString nameFromPath(const QString &path);

    /** Picker/folder category ("Color"/"Dimmer"/"Position"/"Beam"/"Other")
     *  inferred from a script's fixtureTypes. */
    static QString categoryForTypes(const QStringList &fixtureTypes);

    // --- Standard search directories ---
    static QDir systemScriptsDirectory();
    static QDir userScriptsDirectory();
    // RGBScript (QLC+ RGB Matrix) library — run via the EffectScript compat host.
    static QDir systemRgbScriptsDirectory();
    static QDir userRgbScriptsDirectory();

private:
    void scanDir(const QDir &dir, bool rgb = false);
    /** Scan an RGBScript directory and its immediate subdirs (the dev build
     *  symlinks the source dir one level down; production has .js directly). */
    void scanRgbDir(const QDir &dir);
    static ScriptMeta parseMeta(const QString &path);
    static QString extractField(const QString &src, const QString &field);

    // Resolution map: keyed by BOTH the display name (effect.name) and the
    // file basename, so workspaces that stored either can resolve a script.
    QMap<QString, ScriptMeta> m_scripts;
    // Canonical display names for the picker — one entry per script, no
    // filename aliases (scriptNames() returns these, sorted).
    QStringList               m_displayNames;
    QList<QDir>               m_extraDirs;
    QList<QDir>               m_extraRgbDirs;
};

#endif // EFFECTSCRIPTCACHE_H

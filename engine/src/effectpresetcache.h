/*
  Q Light Controller Plus
  effectpresetcache.h

  Scans the effect-PRESET directories for .json files. A preset pins an effect
  SCRIPT (engine) to a set of named parameter values, so the picker can list
  many named "capabilities" backed by few scripts. See EFFECTPRESET_DESIGN.md.

  Search order (first found wins on name collision):
    1. <app>/Resources/EffectPresets/         — bundled
    2. ~/Library/…/QLC+/EffectPresets/        — user-authored

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef EFFECTPRESETCACHE_H
#define EFFECTPRESETCACHE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QDir>

class EffectPresetCache : public QObject
{
    Q_OBJECT

public:
    /** A named effect config: an engine script + pinned parameter values. */
    struct Preset {
        QString name;          //!< display name + identity (unique)
        QString script;        //!< engine script basename (resolved via script cache)
        QString category;      //!< picker grouping (Color/Dimmer/Position/Beam)
        QString description;   //!< one-line tooltip
        QMap<QString, double> params;  //!< param-name → pinned value
        QMap<QString, double> inputs;  //!< optional default input-slot values
        QString path;          //!< source file (empty = not found)
        bool isValid() const { return !name.isEmpty() && !script.isEmpty(); }
    };

    explicit EffectPresetCache(QObject *parent = nullptr);

    /** Scan all default directories and rebuild the cache. */
    void rescan();

    /** Write @p p as a JSON Effect into the USER presets dir and rescan.
     *  Overwrites an existing user file of the same name. Returns false on
     *  I/O error. */
    bool savePreset(const Preset &p);

    /** Display names of all discovered presets (sorted). */
    QStringList presetNames() const;

    /** All presets, in discovery order. */
    QList<Preset> presets() const { return m_presets.values(); }

    /** The preset for @p name, or an invalid Preset if not found. */
    Preset preset(const QString &name) const { return m_presets.value(name); }

    bool contains(const QString &name) const { return m_presets.contains(name); }

    static QDir systemPresetsDirectory();
    static QDir userPresetsDirectory();

private:
    void scanDir(const QDir &dir);
    static Preset parsePreset(const QString &path);

    QMap<QString, Preset> m_presets;  //!< name → Preset
};

#endif // EFFECTPRESETCACHE_H

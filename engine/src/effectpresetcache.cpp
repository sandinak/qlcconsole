/*
  Q Light Controller Plus
  effectpresetcache.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "effectpresetcache.h"
#include "qlcfile.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>
#include <QDebug>

#define EFFECTPRESETDIR     "Resources/EffectPresets"
#define USEREFFECTPRESETDIR "Library/Application Support/QLC+/EffectPresets"

EffectPresetCache::EffectPresetCache(QObject *parent)
    : QObject(parent)
{
}

void EffectPresetCache::rescan()
{
    m_presets.clear();
    // User dir first so a user-saved Effect overrides a shipped one of the same
    // name (first found wins) — "Save as Effect" should always take effect.
    scanDir(userPresetsDirectory());
    scanDir(systemPresetsDirectory());
}

QStringList EffectPresetCache::presetNames() const
{
    QStringList names = m_presets.keys();
    names.sort(Qt::CaseInsensitive);
    return names;
}

bool EffectPresetCache::savePreset(const Preset &p)
{
    if (!p.isValid())
        return false;

    QJsonObject o;
    o.insert("name",   p.name);
    o.insert("script", p.script);
    if (!p.category.isEmpty())    o.insert("category", p.category);
    if (!p.description.isEmpty()) o.insert("description", p.description);
    QJsonObject params;
    for (auto it = p.params.constBegin(); it != p.params.constEnd(); ++it)
        params.insert(it.key(), it.value());
    if (!params.isEmpty())
        o.insert("params", params);
    QJsonObject inputs;
    for (auto it = p.inputs.constBegin(); it != p.inputs.constEnd(); ++it)
        inputs.insert(it.key(), it.value());
    if (!inputs.isEmpty())
        o.insert("inputs", inputs);

    QDir dir = userPresetsDirectory();
    if (!dir.exists())
        dir.mkpath(".");

    // Filename from the name, stripped of path-hostile characters.
    QString fileBase = p.name;
    fileBase.replace(QRegularExpression("[/\\\\:*?\"<>|]"), "_");
    QFile f(dir.absoluteFilePath(fileBase + ".json"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        qWarning() << "[EffectPresetCache] cannot write" << f.fileName();
        return false;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    f.close();

    rescan();
    return true;
}

QDir EffectPresetCache::systemPresetsDirectory()
{
    return QLCFile::systemDirectory(QString(EFFECTPRESETDIR), QString(".json"));
}

QDir EffectPresetCache::userPresetsDirectory()
{
    return QLCFile::userDirectory(QString(USEREFFECTPRESETDIR),
                                  QString(EFFECTPRESETDIR),
                                  QStringList() << "*.json");
}

void EffectPresetCache::scanDir(const QDir &dir)
{
    if (!dir.exists() || !dir.isReadable())
        return;

    const QStringList files = dir.entryList(QStringList() << "*.json",
                                            QDir::Files | QDir::Readable);
    for (const QString &file : files)
    {
        const Preset p = parsePreset(dir.absoluteFilePath(file));
        // First found wins (system before user); skip invalid / duplicate names.
        if (p.isValid() && !m_presets.contains(p.name))
        {
            m_presets.insert(p.name, p);
            qDebug() << "[EffectPresetCache] found:" << p.name
                     << "→ script" << p.script << "at" << p.path;
        }
    }
}

EffectPresetCache::Preset EffectPresetCache::parsePreset(const QString &path)
{
    Preset p;
    p.path = path;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return p;
    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning() << "[EffectPresetCache] bad JSON in" << path << ":" << err.errorString();
        return Preset(); // invalid
    }

    const QJsonObject o = doc.object();
    p.name        = o.value("name").toString();
    p.script      = o.value("script").toString();
    p.category    = o.value("category").toString();
    p.description = o.value("description").toString();

    const QJsonObject params = o.value("params").toObject();
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        p.params.insert(it.key(), it.value().toDouble());

    const QJsonObject inputs = o.value("inputs").toObject();
    for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it)
        p.inputs.insert(it.key(), it.value().toDouble());

    return p;
}

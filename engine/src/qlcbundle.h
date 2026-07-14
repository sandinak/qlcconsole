/*
  Q Light Controller Plus
  qlcbundle.h

  A Bundle is a portable, shareable snapshot of an ordered palette stack
  (colours, dimmer, effects) without fixture targets. Stamping a Bundle onto
  a scene recreates the palette stack, de-duping against existing palettes
  where possible.

  See EFFECTPRESET_DESIGN.md §Part 2 for the full design rationale.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef QLCBUNDLE_H
#define QLCBUNDLE_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>

class QJsonObject;

/** A single palette entry inside a Bundle snapshot. */
struct BundleEntry
{
    QString type;   //!< "Color", "Dimmer", "Effect", "PanTilt", "Beam"
    QString name;   //!< Display name for the stamped palette

    // Color
    QString color;  //!< Hex "#rrggbb"
    QString wauv;   //!< Hex for white/amber/uv combined, optional

    // Dimmer
    int dimmerValue = 128; //!< 0-255

    // Effect
    QString script;        //!< Generator basename
    QString effectPreset;  //!< Named Effect, if from one (display identity)
    QMap<QString, double> params;

    // PanTilt
    double pan  = 0.0;
    double tilt = 0.0;

    // Beam
    int focus = 128;
    int frost = 0;
    int iris  = 255;

    bool isValid() const { return !type.isEmpty(); }

    QJsonObject toJson() const;
    static BundleEntry fromJson(const QJsonObject &o);
};

/** Attributes sub-object for search/filtering. */
struct BundleAttributes
{
    QString      category;  //!< Color/Dimmer/Position/Beam/Mixed
    QStringList  contains;  //!< ["color","dimmer","effect","position","beam"]
    QString      tempo;     //!< slow/medium/fast
    QString      mood;      //!< warm/dramatic/subtle etc.

    QJsonObject  toJson() const;
    static BundleAttributes fromJson(const QJsonObject &o);
};

/**
 * A Bundle — metadata + ordered palette snapshots.
 * Loaded from / saved to a single .json file.
 */
class QLCBundle
{
public:
    QString          name;
    QString          description;
    QString          author;
    QString          created;   //!< ISO date YYYY-MM-DD
    QString          modified;  //!< ISO date YYYY-MM-DD
    int              version = 1;
    QStringList      keywords;
    BundleAttributes attributes;
    QList<BundleEntry> palettes;
    QString          path;      //!< Source file path (empty = unsaved)

    bool isValid() const { return !name.isEmpty() && !palettes.isEmpty(); }

    /** Load from a JSON file. Returns invalid bundle on error. */
    static QLCBundle fromFile(const QString &filePath);

    /** Save to filePath. Returns false on I/O error. */
    bool saveToFile(const QString &filePath) const;

    /** Build a sanitised filename from name (strips path-hostile chars). */
    static QString sanitiseFilename(const QString &name);

    /** True if any searchable field contains @p text (case-insensitive). */
    bool matches(const QString &text) const;
};

#endif // QLCBUNDLE_H

/*
  Q Light Controller Plus
  effectorigin.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef EFFECTORIGIN_H
#define EFFECTORIGIN_H

#include <QCoreApplication>
#include <QStringList>
#include <QString>
#include <QMap>

class QLCPalette;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Where an effect came from, said the same way everywhere (the effect
 * browser, the look editor, Build ▸ Effects):
 *
 *   Built-in   ships with qlcconsole: no mark.
 *   Mine       saved with "Save to My effects…", on this computer: "mine".
 *   This show  one of the show's effect palettes ("Save in this show…"):
 *              the palette mark.
 *
 * Anything made from another effect is "based on" it, and says which of its
 * settings differ from that one.
 */
class EffectOrigin
{
    Q_DECLARE_TR_FUNCTIONS(EffectOrigin)

public:
    enum Kind { BuiltIn = 0, Mine = 1, ThisShow = 2 };

    /** Where the look editor's effect combo keeps an entry's origin (Kind),
     *  its "based on" line and what differs (the effect browser reads them). */
    enum { OriginRole = Qt::UserRole + 3, BasedOnRole, DiffTipRole };
    /** The palette mark: what the show's effect palettes are drawn with. */
    static QString paletteMark() { return QStringLiteral(":/script.png"); }

    /** Whether the preset file @p path is one saved on this computer (in
     *  the user's effects folder), not one that came with the app. */
    static bool isMinePath(const QString &path);

    /** The effect @p p was made from: its preset ("Fire"), else its WLED
     *  effect, else its script's name. Empty when it runs nothing. */
    static QString baseName(Doc *doc, const QLCPalette *p);

    /** The settings @p p has that differ from what it was based on (its
     *  preset's, else its script's defaults), one line each:
     *  "Speed: 2.00 (Fire: 1.00)". Run settings count too. */
    static QStringList differences(Doc *doc, const QLCPalette *p);

    /** A preset's settings that differ from its script's defaults. */
    static QStringList presetDifferences(Doc *doc, const QString &presetName);

    /** Script @p path's own run defaults: whether it is a one-shot, and
     *  whether it lets go at the end (read once per file). */
    static void scriptRunDefaults(const QString &path, bool *oneShot, bool *release);

    /** "based on Fire", and "· 2 changes" when there are some. */
    static QString basedOnLine(const QString &base, int changes);

    /** The tooltip: what differs, or that nothing does. */
    static QString differencesTip(const QString &base, const QStringList &diffs);

private:
    static QStringList compare(Doc *doc, const QString &scriptPath, const QString &base,
                               const QMap<QString, double> &original,
                               const QMap<QString, double> &values,
                               const QMap<QString, QString> &strings);
};

/** @} */

#endif

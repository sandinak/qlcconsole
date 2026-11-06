/*
  Q Light Controller Plus
  effectrows.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef EFFECTROWS_H
#define EFFECTROWS_H

#include <QCoreApplication>
#include <QList>
#include <QString>

#include "effectscript.h"
#include "showtreemodel.h"

class QLCPalette;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The rows under an effect on a Show Tree layer page, as a line aim's
 * Spread / Grouping / Cross sit under its Position: what the effect runs on
 * (by group, with how many of them can run it), what it moves, its colours,
 * and each of its settings with where the value comes from -- this cue's
 * own, the effect palette's, its preset's, the script's default, carried
 * in from an earlier cue. So a person sees what they are about to adjust.
 *
 * A setting set here (setParam()) is this cue's own: an effect that is the
 * cue's own one-off changes in place; a shared effect palette, or one
 * carried in, is copied into a one-off for this cue first -- the palette
 * and its other cues keep theirs. One undo step either way.
 */
class EffectRows
{
    Q_DECLARE_TR_FUNCTIONS(EffectRows)

public:
    struct Row
    {
        QString id;             //!< "runsOn", "moves", "colours", "runs", or the param's name
        QString label;          //!< "Runs on", "Chase speed"
        QString text;           //!< "Towers · 12 fixtures", "3"
        QString origin;         //!< empty: this cue's own; else where it comes from
        QString tip;
        bool inherited = false; //!< grey and italic
        /** A script parameter that can be set here (numbers and lists). */
        bool editable = false;
        EffectScript::ParamDef def;
        double value = 0.0;
    };

    /** The rows under effect @p v (an Effect look, resolved) on @p layer.
     *  @p carried: where the look itself comes from when it is not this
     *  cue's own ("tracked from 1 One"; empty: own). @p span: the layers it
     *  is ONE effect with (D10; fewer than two: just this one). */
    static QList<Row> rows(Doc *doc, const ShowTree::ResolvedLayer &layer, const QString &key,
                           const ShowTree::ResolvedValue &v, const QString &carried,
                           const QList<ShowTree::Target> &span);

    /** "Runs on" alone (the cue page's cue-level effects): "Left + Right ·
     *  4 fixtures", or "· 2 of 4 fixtures (Pars have no pan/tilt)". */
    static QString runsOnText(Doc *doc, const QString &key, const QList<ShowTree::Target> &targets);

    /** The effect palette a look plays (null: none). */
    static QLCPalette *effectPalette(Doc *doc, const ShowTree::Look &look);

    /** What a person reads for @p value of @p def: an enum's name, a length
     *  in Settings ▸ Units, "rev" for a "(0=fwd, 1=rev)" switch, a number. */
    static QString valueText(const EffectScript::ParamDef &def, double value);
    /** The label a person reads for @p def: its description up to any
     *  "(...)", else its name. */
    static QString paramLabel(const EffectScript::ParamDef &def);

    /** Set parameter @p param of effect @p key on layer @p layerId (a
     *  carried-in @p target when invalid) in cue @p cueId to @p value
     *  (@p clear: back to the script's / preset's default). It becomes
     *  this cue's own: see the class note. One undo step; false when there
     *  is no such effect there or nothing changed. */
    static bool setParam(Doc *doc, quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                         const QString &key, const QString &param, double value, bool clear = false);
};

/** @} */

#endif

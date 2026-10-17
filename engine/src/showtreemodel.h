/*
  Q Light Controller Plus
  showtreemodel.h

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef SHOWTREEMODEL_H
#define SHOWTREEMODEL_H

#include <QStringList>
#include <QString>
#include <QList>
#include <QMap>

#include "qlcengine_export.h"

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The Show Tree (SHOW_TREE_DESIGN.md): Production -> Cue list -> Cue ->
 * Layer -> looks. Plain values with no Doc behind them, so the cascade can
 * be read and tested on its own; ShowTreeController bakes them into
 * functions.
 *
 * A look is one ATTRIBUTE's value (D4): a key ("Colour", "Dimmer",
 * "Position", "Beam.Gobo", "Beam.Zoom", "Beam.Beam", "Shutter", or
 * "Effect.<kind>") and the palettes that make it. Every key cascades on its
 * own.
 */
namespace ShowTree
{
    QLC_ENGINE_EXPORT quint32 invalidId();

    /************************************************************************
     * Attribute keys
     ************************************************************************/

    extern QLC_ENGINE_EXPORT const QString KeyColour;
    extern QLC_ENGINE_EXPORT const QString KeyDimmer;
    extern QLC_ENGINE_EXPORT const QString KeyPosition;
    extern QLC_ENGINE_EXPORT const QString KeyGobo;
    extern QLC_ENGINE_EXPORT const QString KeyZoom;
    extern QLC_ENGINE_EXPORT const QString KeyBeam;
    extern QLC_ENGINE_EXPORT const QString KeyShutter;
    extern QLC_ENGINE_EXPORT const QString KeyEffectPrefix;     //!< "Effect."

    /** "Effect.<kind>" for an effect of @a kind ("Color", "Dimmer",
     *  "Position", "Beam", "Other"). */
    QLC_ENGINE_EXPORT QString effectKey(const QString &kind);
    QLC_ENGINE_EXPORT bool isEffectKey(const QString &key);

    /** Every key in the order a generated scene's looks are written in
     *  (the look-stack order, docs/features/effects.md): the base looks,
     *  then the effects, each followed by its own looks. Unknown keys sort
     *  after the known ones of their kind, by name. */
    QLC_ENGINE_EXPORT bool keyLessThan(const QString &a, const QString &b);

    /** What a person reads for @a key: "Colour", "Gobo", "Colour effect". */
    QLC_ENGINE_EXPORT QString keyLabel(const QString &key);

    /************************************************************************
     * The tree
     ************************************************************************/

    /** One attribute's value on a layer or a cue. */
    struct QLC_ENGINE_EXPORT Look
    {
        QString key;
        /** The value: one palette, or for Colour the ordered colours
         *  (colour 1, 2, 3) and for Position a Pan and a Tilt. */
        QList<quint32> palettes;
        /** An effect's own looks: the Colours/Dimmers that belong to it
         *  (they apply inside that effect only). */
        QList<quint32> own;
        /** "Off here": stops an inherited value; no palette. */
        bool none = false;

        bool operator==(const Look &other) const;
        bool operator!=(const Look &other) const { return !(*this == other); }
    };

    /** What a layer lights: groups and fixtures, or one existing function
     *  played as it is (an opaque function layer). */
    struct QLC_ENGINE_EXPORT Target
    {
        QList<quint32> groups;
        QList<quint32> fixtures;
        quint32 function = 0xFFFFFFFF;

        bool isOpaque() const { return function != 0xFFFFFFFF; }
        bool isEmpty() const { return groups.isEmpty() && fixtures.isEmpty() && !isOpaque(); }
        /** Identity across cues: a layer on the same target in the next
         *  cue is the same layer (it tracks, D5). Order-free. */
        QString signature() const;

        bool operator==(const Target &other) const { return signature() == other.signature(); }
    };

    enum class CueMode { ListDefault, Track, Block, CueOnly };
    enum class ListMode { Tracking, NonTracking };

    QLC_ENGINE_EXPORT QString cueModeToString(CueMode mode);
    QLC_ENGINE_EXPORT CueMode stringToCueMode(const QString &str);

    struct QLC_ENGINE_EXPORT Layer
    {
        quint32 id = 0xFFFFFFFF;
        QString name;                   //!< optional; else the target's name
        Target target;
        QList<Look> looks;
        /** Leaving the cue it is set in, this layer is dropped from what
         *  passes on: the base cue's values, else dark (D7). */
        bool release = false;

        const Look *look(const QString &key) const;
        /** Set (replace) the look for its key; an empty, non-None look
         *  removes it. */
        void setLook(const Look &look);
    };

    struct QLC_ENGINE_EXPORT Cue
    {
        quint32 id = 0xFFFFFFFF;
        QString number;                 //!< "0.0.4"; empty: its position
        QString name;
        QString note;
        CueMode mode = CueMode::ListDefault;
        uint fadeIn = 0;
        uint fadeOut = 0;
        uint hold = 0xFFFFFFFE;         //!< Function::infiniteSpeed(): waits for GO
        QList<Look> looks;              //!< cue looks: reach this cue's own layers (D6)
        QList<Layer> layers;
        QList<quint32> includes;        //!< cues (of the same list) included, in order (D8)
        /** Generated: the Collection this cue bakes to. */
        quint32 collection = 0xFFFFFFFF;

        const Look *look(const QString &key) const;
        void setLook(const Look &look);
        Layer *layer(quint32 id);
        const Layer *layer(quint32 id) const;
    };

    struct QLC_ENGINE_EXPORT CueList
    {
        quint32 id = 0xFFFFFFFF;
        QString name;
        ListMode mode = ListMode::NonTracking;
        quint32 baseCue = 0xFFFFFFFF;   //!< what Release goes to (D7)
        QList<Cue> cues;
        /** Generated: the Chaser this list bakes to. */
        quint32 chaser = 0xFFFFFFFF;

        int indexOf(quint32 cueId) const;
        Cue *cue(quint32 id);
        const Cue *cue(quint32 id) const;
    };

    struct QLC_ENGINE_EXPORT Production
    {
        quint32 id = 0xFFFFFFFF;
        QString name;
        QList<CueList> lists;

        CueList *list(quint32 id);
        const CueList *list(quint32 id) const;
    };

    /************************************************************************
     * Resolution: what each cue really plays (§2)
     ************************************************************************/

    /** Where a resolved value comes from, for "Blue -- from Cue 0.1". */
    enum class Origin
    {
        Own,        //!< set on the layer in this cue
        CueLook,    //!< the cue's look reaching the layer
        Tracked,    //!< carried in from an earlier cue
        Included,   //!< from an included cue
        Base        //!< a released layer, back to the base cue's value
    };

    struct QLC_ENGINE_EXPORT ResolvedValue
    {
        Look look;
        Origin origin = Origin::Own;
        quint32 fromCue = 0xFFFFFFFF;   //!< the cue it was set in
    };

    struct QLC_ENGINE_EXPORT ResolvedLayer
    {
        Target target;
        QString name;
        quint32 layerId = 0xFFFFFFFF;   //!< the layer it was last set by
        /** The layer is this cue's own (set here), not only carried in. */
        bool own = false;
        bool release = false;
        Origin origin = Origin::Own;    //!< how the layer itself got here
        quint32 fromCue = 0xFFFFFFFF;
        QMap<QString, ResolvedValue> values;

        /** The looks a generated scene plays, in look-stack order: None
         *  values left out. */
        QList<Look> playedLooks() const;
        /** Whether it plays anything at all. */
        bool isDark() const;
    };

    struct QLC_ENGINE_EXPORT ResolvedCue
    {
        quint32 cueId = 0xFFFFFFFF;
        QList<ResolvedLayer> layers;    //!< what is on stage in this cue, in order
        /** What the next cue starts from, when it tracks. */
        QList<ResolvedLayer> passOn;
        /** Includes that were left out (one that would loop). */
        QList<quint32> skippedIncludes;

        const ResolvedLayer *layer(const Target &target) const;
    };

    /** Resolve every cue of @a list, in list order. Pure: no Doc. */
    QLC_ENGINE_EXPORT QList<ResolvedCue> resolve(const CueList &list);

    /** The mode @a cue really plays in, in @a list (List default made
     *  Track or Block). */
    QLC_ENGINE_EXPORT CueMode effectiveMode(const CueList &list, const Cue &cue);

    /************************************************************************
     * Numbering (D23)
     ************************************************************************/

    /** The number for a cue inserted after @a afterIndex (-1: at the top):
     *  the next whole number in the last place if free, else a point cue
     *  between its neighbours. Never renumbers. */
    QLC_ENGINE_EXPORT QString numberForInsert(const CueList &list, int afterIndex);

    /** Split "0.0.4-SHOUT" into "0.0.4" and "SHOUT"; false when @a text
     *  doesn't start with a number. */
    QLC_ENGINE_EXPORT bool splitNumberedName(const QString &text, QString *number, QString *name);

    /************************************************************************
     * XML
     ************************************************************************/

    QLC_ENGINE_EXPORT void saveXML(QXmlStreamWriter *doc, const Production &production);
    /** Read a <Production> (the reader on its start element). */
    QLC_ENGINE_EXPORT bool loadXML(QXmlStreamReader &root, Production &production);

    /** Ids in a comma list ("1,2,3"). */
    QLC_ENGINE_EXPORT QString idsToString(const QList<quint32> &ids);
    QLC_ENGINE_EXPORT QList<quint32> stringToIds(const QString &str);
}

/** @} */

#endif

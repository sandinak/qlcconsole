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
#include <functional>
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
    /** A line aim's parameters (§3B, D16): each a key of its own, so each
     *  is set and inherited on its own. Values, not palettes (Look::value):
     *  Spread "-5".."5", Grouping "each" / "pairs" / "threes" / "fours" /
     *  "halves", Cross "swap" / "mirror". */
    extern QLC_ENGINE_EXPORT const QString KeySpread;       //!< "Position.Spread"
    extern QLC_ENGINE_EXPORT const QString KeyGrouping;     //!< "Position.Grouping"
    extern QLC_ENGINE_EXPORT const QString KeyCross;        //!< "Position.Cross"
    QLC_ENGINE_EXPORT bool isLineParamKey(const QString &key);

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
        /** A value that is not a palette: a line aim's Spread ("2.5"),
         *  Grouping ("pairs") or Cross ("mirror"). */
        QString value;

        bool operator==(const Look &other) const;
        bool operator!=(const Look &other) const { return !(*this == other); }
    };

    /** What a layer lights: groups and fixtures, or one existing function
     *  played as it is (an opaque function layer). */
    struct QLC_ENGINE_EXPORT Target
    {
        QList<quint32> groups;
        QList<quint32> fixtures;
        /** Group sets (GroupSet ids): one target of all their members,
         *  each member keeping its own head layout (§1). The set, not its
         *  members, is the layer's identity: a set whose members change
         *  stays the same layer. */
        QList<quint32> sets;
        quint32 function = 0xFFFFFFFF;

        bool isOpaque() const { return function != 0xFFFFFFFF; }
        bool isEmpty() const { return groups.isEmpty() && fixtures.isEmpty() && sets.isEmpty() && !isOpaque(); }
        /** Identity across cues: a layer on the same target in the next
         *  cue is the same layer (it tracks, D5). Order-free. */
        QString signature() const;

        bool operator==(const Target &other) const { return signature() == other.signature(); }
    };

    /** A named set of groups (and fixtures) in the Library, e.g. Towers =
     *  SL + SR (§1, "Make set"). */
    struct QLC_ENGINE_EXPORT GroupSet
    {
        quint32 id = 0xFFFFFFFF;
        QString name;
        QList<quint32> groups;
        QList<quint32> fixtures;
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
        /** The carried-in layer this one carries on from although its target
         *  differs (a group was added to it, or taken away, in this cue): it
         *  takes that layer's place and tracked values instead of lighting
         *  beside it. Empty: none (the usual case, the same target). */
        Target continues;
        /** More carried-in layers it takes the place of, after @a continues
         *  (a merge: one layer now stands for several). Their values fill in
         *  what the first one doesn't have. Several own layers in one cue may
         *  carry on from the same layer (a split): each starts from it. */
        QList<Target> continuesAlso;
        /** A child layer (§3.5): the layer of the same cue whose target this
         *  one is part of (a member of its set, a sub-group). It sets only
         *  what it sets, and is lit by its parent for the rest; the parent
         *  doesn't light what the child sets on the child's fixtures. Cue
         *  looks reach the parent, not the child. Invalid: a top layer. */
        quint32 parent = 0xFFFFFFFF;
        /** A use of a reusable layer (ReusableLayer id, P4): its name,
         *  target and looks are the reusable's, kept the same in every use
         *  (an edit to one is an edit to all). Release, what it carries on
         *  from and its parent stay this use's. Invalid: a plain layer. */
        quint32 reusable = 0xFFFFFFFF;

        /** The pieces a split makes: one per group, per set, and one on the
         *  fixtures. */
        QList<Target> pieces() const;

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
        /** Layers on fixtures in common: false (the tree's rule, §2) -- the
         *  lower layer wins what both set, the upper isn't lit there for it
         *  (Scene::excludedFixtures); true -- they mix as a plain collection
         *  does (intensity and RGB highest-wins), which an imported cue keeps
         *  so it plays as its chaser did. Child layers always win. */
        bool mixOverlap = false;
        /** A use of a reusable cue (ReusableCue id, P4): its name, mode,
         *  overlap, cue looks and layers are the reusable's, kept the same
         *  in every use. Number, note, timing and includes stay this use's.
         *  Invalid: a plain cue. */
        quint32 reusable = 0xFFFFFFFF;
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

    /** A reusable layer in the Library (Make reusable, P4): a named target
     *  and its looks, used by layers in any cue of any list (Layer::
     *  reusable). Editing it -- or any of its uses -- changes every use. */
    struct QLC_ENGINE_EXPORT ReusableLayer
    {
        quint32 id = 0xFFFFFFFF;
        QString name;
        Target target;
        QList<Look> looks;
    };

    /** A reusable cue in the Library (Make reusable, P4): a named state --
     *  a "Blackout", a "Walk-in" -- put in any list as a cue (Cue::
     *  reusable). Its layers' ids are its own; each use has its own ids
     *  for the same layers, in the same order. Timing and note are what a
     *  new use starts with. */
    struct QLC_ENGINE_EXPORT ReusableCue
    {
        quint32 id = 0xFFFFFFFF;
        QString name;
        CueMode mode = CueMode::ListDefault;
        bool mixOverlap = false;
        QList<Look> looks;
        QList<Layer> layers;
        QString note;
        uint fadeIn = 0;
        uint fadeOut = 0;
        uint hold = 0xFFFFFFFE;
    };

    /** What makes two uses of a reusable the same (P4): a layer's name,
     *  target and looks; a cue's name, mode, overlap, cue looks and its
     *  layers' content (with release, their parents by position and
     *  their own links), not ids, timing, includes or tracking. */
    QLC_ENGINE_EXPORT QString layerContent(const Layer &layer);
    QLC_ENGINE_EXPORT QString cueContent(const Cue &cue);

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
        /** Set at cue level, as ONE across the layers it reaches: the cue
         *  it was set in (an effect, D10; a line aim and its parameters,
         *  D17). Kept as it tracks, so the layers carrying the same value
         *  and span are one effect / one formation until a later cue
         *  changes some of them. Invalid: the layer's own. */
        quint32 span = 0xFFFFFFFF;
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
        /** An own layer that took a carried-in (tracked, included) layer's
         *  place: that layer's target. Empty when it came in fresh. */
        Target tookOver;
        /** A child layer (§3.5): its parent's target and how deep it is
         *  (0: a top layer). Kept as it tracks. */
        Target parent;
        int depth = 0;
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

    /** Whether a Position look aims at a line (D18): only those take a
     *  cue's Spread / Grouping / Cross. Without one, none do. */
    typedef std::function<bool(const Look &)> LineAimTest;

    /** Resolve every cue of @a list, in list order. Pure: no Doc. */
    QLC_ENGINE_EXPORT QList<ResolvedCue> resolve(const CueList &list, const LineAimTest &isLineAim = LineAimTest());

    /** What a cue puts on stage, per unit -- each group, fixture and opaque
     *  function a layer lights ("g:12", "f:3", "x:40") -- and attribute:
     *  the value that wins there (layers later in the cue win), or for
     *  Dimmer (HTP) every value that plays. Two lists whose cues have the
     *  same stage play the same, however their layers are cut up. */
    typedef QMap<QString, QMap<QString, QStringList>> Stage;
    QLC_ENGINE_EXPORT Stage stageOf(const ResolvedCue &cue);

    /** The first place where @a before and @a after (the same list, edited)
     *  play differently: its cue, unit ("g:12") and attribute. False when
     *  every cue plays the same. */
    QLC_ENGINE_EXPORT bool stageDiffers(const CueList &before, const CueList &after,
                                        quint32 *cueId = nullptr, QString *unit = nullptr,
                                        QString *key = nullptr,
                                        const LineAimTest &isLineAim = LineAimTest());

    /** The mode @a cue really plays in, in @a list (List default made
     *  Track or Block). */
    QLC_ENGINE_EXPORT CueMode effectiveMode(const CueList &list, const Cue &cue);

    /** Whether resolving cue @a fromCue needs cue @a onCue, at any depth:
     *  it tracks from the cue before it (Track, Cue-only), it includes a
     *  cue, or it releases a layer to the base cue. A cue depends on
     *  itself. An include that would make a cue depend on itself is a loop
     *  (D8: no cycles). */
    QLC_ENGINE_EXPORT bool dependsOn(const CueList &list, quint32 fromCue, quint32 onCue);

    /** Whether cue @a cueId may include cue @a includedId: both in @a list,
     *  not the same, not already included, and no loop (@a includedId does
     *  not depend on @a cueId). @a why says why not. */
    QLC_ENGINE_EXPORT bool canInclude(const CueList &list, quint32 cueId, quint32 includedId,
                                      QString *why = nullptr);

    /** The cues whose output can change when cue @a cueId's own content
     *  changes (its layers, looks, mode, includes), in list order: the cue
     *  itself; the cues after it that start from it, up to a Block cue (a
     *  Cue-only cue passes on only what it started from); every cue that
     *  includes any of them; and, when one of them is the base cue, the
     *  cues a released layer comes back to it in -- and so on, until
     *  nothing more is reached. A bake changes no cue outside it. */
    QLC_ENGINE_EXPORT QList<quint32> rebakeChain(const CueList &list, quint32 cueId);

    /************************************************************************
     * Numbering (D23)
     ************************************************************************/

    /** The number for a cue inserted after @a afterIndex (-1: at the top):
     *  the next whole number in the last place if free, else a point cue
     *  between its neighbours. Never renumbers. */
    QLC_ENGINE_EXPORT QString numberForInsert(const CueList &list, int afterIndex);

    /** Compare two cue numbers part by part, as numbers: "0.0.4" <
     *  "0.0.4.5" < "0.0.5". Negative, 0 or positive. */
    QLC_ENGINE_EXPORT int compareCueNumbers(const QString &a, const QString &b);

    /** The index of the cue numbered @a number in @a list (-1: none). A cue
     *  with no number is numbered by its position. */
    QLC_ENGINE_EXPORT int indexOfNumber(const CueList &list, const QString &number);

    /** Split "0.0.4-SHOUT" into "0.0.4" and "SHOUT"; false when @a text
     *  doesn't start with a number. */
    QLC_ENGINE_EXPORT bool splitNumberedName(const QString &text, QString *number, QString *name);

    /************************************************************************
     * XML
     ************************************************************************/

    QLC_ENGINE_EXPORT void saveXML(QXmlStreamWriter *doc, const Production &production);
    QLC_ENGINE_EXPORT void saveXML(QXmlStreamWriter *doc, const GroupSet &set);
    QLC_ENGINE_EXPORT void saveXML(QXmlStreamWriter *doc, const ReusableLayer &reusable);
    QLC_ENGINE_EXPORT void saveXML(QXmlStreamWriter *doc, const ReusableCue &reusable);
    /** Read a <ReusableLayer> / <ReusableCue> (the reader on its start
     *  element). */
    QLC_ENGINE_EXPORT bool loadXML(QXmlStreamReader &root, ReusableLayer &reusable);
    QLC_ENGINE_EXPORT bool loadXML(QXmlStreamReader &root, ReusableCue &reusable);
    /** Read a <GroupSet> (the reader on its start element). */
    QLC_ENGINE_EXPORT bool loadXML(QXmlStreamReader &root, GroupSet &set);
    /** Read a <Production> (the reader on its start element). */
    QLC_ENGINE_EXPORT bool loadXML(QXmlStreamReader &root, Production &production);

    /** Ids in a comma list ("1,2,3"). */
    QLC_ENGINE_EXPORT QString idsToString(const QList<quint32> &ids);
    QLC_ENGINE_EXPORT QList<quint32> stringToIds(const QString &str);
}

/** @} */

#endif

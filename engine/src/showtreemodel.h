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
class QColor;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The Show Tree (docs/design/SHOW_TREE_DESIGN.md): Production -> Cue list -> Cue ->
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
        /** An effect's fade in / fade out (ms): how its depth -- what it
         *  does over the layer's base look -- comes up when it starts and
         *  goes when it is released or replaced, while the base look fades
         *  on the cue's own time. FadeFollowCue (a new effect's): the
         *  cue's fade in / out. FadeSnap: at once -- an effect saved
         *  before effect fades (no FadeIn/FadeOut in the file) or
         *  converted from a scene, so it plays as it always did. Only an
         *  effect's (isEffectKey()); other looks ignore them. */
        static constexpr int FadeSnap = -1;
        static constexpr int FadeFollowCue = -2;
        int fadeIn = FadeFollowCue;
        int fadeOut = FadeFollowCue;

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

    /** A cue list's automatic move-in-black (Q38): the show's setting
     *  (ProgrammerController::isAutoMoveInBlack), or always on / off for
     *  this list. */
    enum class MoveInBlack { Show, On, Off };

    /** Where a hit lives in the tree, and so when it can fire (Q154, 0.3.1
     *  scoped hits): Production -- its production's Hits section, armed
     *  while that production is current (or none is); Global -- the top
     *  of the tree, armed always; List -- under a cue list, armed while it
     *  runs (or, nothing of its production running, while it is the
     *  current production's main list); Cue -- under a cue or sub-cue,
     *  armed while that cue is live. In this order, from the widest: the
     *  most specific armed hit of a VC button is the one it fires. */
    enum class HitScope { Global, Production, List, Cue };

    /** Whether the planner moves dark heads in black into a cue: never when
     *  the cue says "Don't move in black into this cue" (@a cueOff), else
     *  the list's setting when it has one, else the show's. */
    QLC_ENGINE_EXPORT bool effectiveMoveInBlack(bool showDefault, MoveInBlack list, bool cueOff);

    /** How a production's cues are numbered (Q190), and so what its song
     *  sections are: None (no sections); ShowPartIteration, {S}.{P}.{II}
     *  -- show position, part, iteration, each level period-separated
     *  (0.0.1 setup, 1.0.1 the first song starts) -- a section per first
     *  level; Hundreds (Eos / MA style: 100, 101, 101.5 for song 1, 200..
     *  for song 2) a section per hundred; Manual (QLab style: numbers say
     *  nothing) sections begin at the cues marked by hand. */
    enum class Numbering { None, ShowPartIteration, Hundreds, Manual };
    QLC_ENGINE_EXPORT QString numberingToString(Numbering numbering);
    QLC_ENGINE_EXPORT Numbering stringToNumbering(const QString &str);

    /** A song section's own settings in a cue list (Q190): its name ("Everybody
     *  Scream"). Sections are found by @a key -- the first level, or the
     *  hundred, of the scheme -- or, for Manual, begin at cue @a cue. */
    struct QLC_ENGINE_EXPORT SectionHead
    {
        QString key;                    //!< "1", "200"; empty for Manual
        quint32 cue = 0xFFFFFFFF;       //!< Manual: the cue it begins at
        QString name;
    };

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
        /** After Unlink here (open 20): the reusable it was a use of, by
         *  name, for the tooltip "Unlinked from BackStage". Empty: none. */
        QString unlinkedFrom;

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
         *  so it plays as its chaser did. Child layers always win. In a
         *  cue that mixes, an included cue's layers also play beside this
         *  cue's own on the same target (and beside another include's),
         *  as the chaser's nested collection did, instead of the own layer
         *  taking their place; what it passes on is the one rule's. */
        bool mixOverlap = false;
        /** A use of a reusable cue (ReusableCue id, P4): its name, mode,
         *  overlap, cue looks and layers are the reusable's, kept the same
         *  in every use. Number, note, timing and includes stay this use's.
         *  Invalid: a plain cue. */
        quint32 reusable = 0xFFFFFFFF;
        /** After Unlink here (open 20): the reusable cue's name. */
        QString unlinkedFrom;
        /** Generated: the Collection this cue bakes to. */
        quint32 collection = 0xFFFFFFFF;
        /** A look of a production's Looks section (CueList::looks): the
         *  folder it is filed in there ("Hits", "Hits/Big"; empty: none). */
        QString folder;
        /** A hit's place (HitScope): it stays a cue of its production's
         *  Hits section, so everything a hit does is unchanged; @a scopeOf
         *  is the cue list or cue it lives under (same production). Saved
         *  as Scope= and ScopeOf=, which an older build ignores: there it
         *  is in its production's Hits section, firing at any time. */
        HitScope scope = HitScope::Production;
        quint32 scopeOf = 0xFFFFFFFF;
        /** A sub-cue (P6, §3C): it goes on by itself @a wait ms after the
         *  sub-cue before it has faded in (Auto-follow, "auto 0.5s"); false:
         *  it waits for Sub GO. The first sub-cue plays with GO. */
        bool follow = false;
        uint wait = 0;
        /** Skipped (rehearsals, a cut song): GO, auto-follow and Back pass
         *  over it, and it tracks as if it weren't in the list -- nothing
         *  of it passes on, the cue after starts from the cue before it.
         *  A jump straight to it still plays it (as from the cue before).
         *  Its chaser step is skipped (ChaserStep::skip). Kept so it can
         *  be put back; this use's, like its number (not a reusable's). */
        bool skip = false;
        /** The colour its text is shown in -- the tree, the Run tab, the VC
         *  Cue Runner and Cue List ("#rrggbb"; empty: its list's, else the
         *  theme's). Only how it reads: nothing plays differently. This
         *  use's, like its number. Saved as Colour=, which an older build
         *  ignores. */
        QString colour;
        /** "Don't move in black into this cue" (Q38): the move-in-black
         *  planner never pre-positions heads for it, whatever the list and
         *  the show say (a cue whose heads must be seen moving, or that
         *  follows too closely). This use's, like its number. Saved as
         *  NoMoveInBlack="1", which an older build ignores. */
        bool noMoveInBlack = false;
        /** A focus-call cue (docs/design/FOCUS_REFINE_DESIGN.md section 7): GO plays its
         *  own looks as a preset and ARMS the show's focus calls; the
         *  controller's Start begins them, and leaving them goes on to the
         *  next cue. This use's, like its number. Saved as FocusCall="1",
         *  which an older build ignores (it is then an ordinary cue). */
        bool focusCall = false;
        /** An override hit (0.4): it wins over everything the show plays on
         *  the channels it sets, intensity too, the last fired on top; let
         *  go, they fade back over its fade out to what the show plays then
         *  (Collection::setHitOverride). Hits only. Saved as Override="1",
         *  which an older build ignores (there it mixes as any hit). */
        bool override = false;

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
        /** A production's Looks section (docs/design/PROGRAMMING_ROLE_DESIGN.md §4.1,
         *  P-D5): its "cues" are busking looks, fired from VC buttons, not
         *  cues of a list. Each always resolves as a Block cue would (no
         *  tracking, includes or base) and bakes to a Collection of its
         *  own; there is no chaser. One per production at most. Saved as
         *  <Looks> beside the <CueList>s. */
        bool looks = false;
        /** A Looks section's folders kept although no look is in them yet
         *  (New folder); a folder a look is in needs no entry. */
        QStringList folders;
        /** The text colour of its cues that have none of their own
         *  (Cue::colour; empty: the theme's). */
        QString colour;
        /** Automatic move-in-black for its cues (Q38): the show's setting,
         *  or On / Off for this list whatever the show says. A sub-cue list
         *  follows its cue's list; a Looks section has none. Saved as
         *  MoveInBlack="On|Off", which an older build ignores. */
        MoveInBlack moveInBlack = MoveInBlack::Show;
        /** A cue's sub-cues (P6, §3C, D9, D25): the sub-cues of cue @a subOf
         *  (in another list of the same production) as a list of their own,
         *  as a Looks section is -- so every cue operation works on a
         *  sub-cue. Its first sub-cue starts from what its cue puts on
         *  stage, each next one from the one before (always tracking: no
         *  mode, includes or base); nothing passes on to the cue after the
         *  parent (sub-cue changes are cue-only). Numbered <cue>.<n> unless
         *  a sub-cue has a number of its own. Bakes to a hidden Chaser of the
         *  sub-cues' Collections that is the parent's step, SubCues="SubGo"
         *  (docs/design/SUBCUES_DESIGN.md). Saved as <SubCues>. Invalid: a plain list. */
        quint32 subOf = 0xFFFFFFFF;
        bool isSubCues() const { return subOf != 0xFFFFFFFF; }

        /** The chaser this list was imported from (W5, `Source=`): the link
         *  back that Restore original follows. Invalid: made in the tree. */
        quint32 source = 0xFFFFFFFF;
        /** Imported in place (W5): the list took over from its chaser, which
         *  -- with the collections and scenes only it used -- was filed under
         *  Imported/… (@a originals: each one's id and the folder it was in,
         *  for Restore original). Saved as <Original> children, which an
         *  older build skips. */
        bool inPlace = false;
        struct Original
        {
            quint32 function = 0xFFFFFFFF;
            QString path;               //!< its folder before it was filed
        };
        QList<Original> originals;

        /** Its song sections' names, and (Manual numbering) where each
         *  begins (Q190). Saved as <Section> children, which an older build
         *  skips. */
        QList<SectionHead> sections;

        int indexOf(quint32 cueId) const;
        Cue *cue(quint32 id);
        const Cue *cue(quint32 id) const;
    };

    /** One song of tonight's running order: a section's id (sectionId())
     *  and whether it is left out tonight. */
    struct QLC_ENGINE_EXPORT RunningSong
    {
        QString id;
        bool skip = false;
        bool operator==(const RunningSong &o) const { return id == o.id && skip == o.skip; }
    };

    struct QLC_ENGINE_EXPORT Production
    {
        quint32 id = 0xFFFFFFFF;
        QString name;
        QList<CueList> lists;
        /** The cue list GO, the footer and show control play while this is
         *  the current production (P5). Invalid: its first cue list. */
        quint32 mainList = 0xFFFFFFFF;
        /** Where its timecode comes from while it is current (P5): a
         *  universe, -1 any source, -2 (default) the show's own setting. */
        qint32 timecode = -2;
        /** How its cues are numbered, and so its song sections (Q190).
         *  Saved as Numbering=, which an older build ignores; None: no
         *  sections (every older show). */
        Numbering numbering = Numbering::None;
        /** Tonight's running order (0.3.1 ro): the song sections in the
         *  order the Run plays them, and the ones left out tonight. Empty:
         *  number order. Saved as a RunningOrder element, which an older
         *  build skips. See ShowTree::playOrder(). */
        QList<RunningSong> runningOrder;

        CueList *list(quint32 id);
        const CueList *list(quint32 id) const;
        /** The Looks section (null: the production has none yet). */
        CueList *looks();
        const CueList *looks() const;
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

        /** The layer on @a target; in a cue that mixes, where an include
         *  plays beside the cue's own layer on it, the later (the own). */
        const ResolvedLayer *layer(const Target &target) const;
    };

    /** Whether a Position look aims at a line (D18): only those take a
     *  cue's Spread / Grouping / Cross. Without one, none do. */
    typedef std::function<bool(const Look &)> LineAimTest;

    /** Resolve every cue of @a list, in list order. Pure: no Doc. A list of
     *  sub-cues (CueList::subOf) starts from @a start: what its cue puts
     *  on stage (that cue's ResolvedCue::layers).
     *
     *  A skipped cue (Cue::skip) is resolved as a jump to it plays it --
     *  from what the cue before it passes on -- but passes on nothing of
     *  its own: its ResolvedCue::passOn is what it was handed, so every
     *  cue after it resolves as if it weren't in the list. */
    QLC_ENGINE_EXPORT QList<ResolvedCue> resolve(const CueList &list, const LineAimTest &isLineAim = LineAimTest(),
                                                 const QList<ResolvedLayer> *start = nullptr);

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
     *  Track or Block; a look of a Looks section is always Block, a sub-cue
     *  always Track). */
    QLC_ENGINE_EXPORT CueMode effectiveMode(const CueList &list, const Cue &cue);

    /** The cue a cue at @a index starts from when it tracks: the nearest
     *  cue before it that isn't skipped (-1: none, it is the first played). */
    QLC_ENGINE_EXPORT int playedBefore(const CueList &list, int index);
    /** The next cue after @a index that isn't skipped (-1: none). */
    QLC_ENGINE_EXPORT int playedAfter(const CueList &list, int index);

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
     *  between its neighbours (numberBetween()). Never renumbers. */
    QLC_ENGINE_EXPORT QString numberForInsert(const CueList &list, int afterIndex,
                                              const QStringList &taken = QStringList());
    /** D23's rule for a number between @a prev and @a next (either empty: the
     *  top or the end of the list), none of @a used: prev with its last
     *  place + 1 if that fits ("2" -> "3"); else a point cue one level down,
     *  trying 5, 6, 7, 8, 9, 1, 2, 3, 4 ("2" .. "3" -> "2.5", "2" .. "2.5"
     *  -> "2.1"); else the same below prev.0, at most three levels below
     *  prev ("2" .. "2.1" -> "2.0.5"). At the top: one less in the first's
     *  last place while that stays 1 or more ("5" -> "4", "0.0.3" ->
     *  "0.0.2"); else the first cut short, ending 1 or more ("2.1" -> "2");
     *  else the rule above after the first with its last place that isn't
     *  0 one less ("1" -> "0.5", "0.0.1" -> "0.0.0.5"). Empty: none fits.
     *  + Cue and a dragged cue both number with it. */
    QLC_ENGINE_EXPORT QString numberBetween(const QString &prev, const QString &next,
                                            const QStringList &used = QStringList());
    /** Digits and dots: "4", "0.0.4", "2.5". */
    QLC_ENGINE_EXPORT bool isCueNumber(const QString &number);
    /** A cue's number as a person (and show control) reads it: its own, else
     *  its position; a sub-cue's is its cue's and its own, "4.1" (P6). */
    QLC_ENGINE_EXPORT QString cueNumberAt(const CueList &list, int index, const QString &parentNumber = QString());

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
     * Song sections (Q190)
     ************************************************************************/

    /** The section a cue numbered @a number is in under @a numbering: the
     *  first level ("1.2.3" -> "1"), the hundred ("215.5" -> "200"); empty
     *  for None and Manual (sections by hand) or a number that isn't one. */
    QLC_ENGINE_EXPORT QString sectionKey(Numbering numbering, const QString &number);

    /** A run of cues of a list that is one song section. */
    struct QLC_ENGINE_EXPORT Section
    {
        QString key;                    //!< sectionKey(); empty for Manual
        quint32 headCue = 0xFFFFFFFF;   //!< its first cue
        QString name;                   //!< its SectionHead's (empty: none)
        int first = -1;                 //!< index of its first cue
        int last = -1;                  //!< index of its last cue
    };

    /** The song sections of @a list, in list order (none for None, a Looks
     *  section or sub-cues). A scheme's sections follow the cue numbers: a
     *  new one wherever the key changes. Manual sections begin at their
     *  heads' cues; cues before the first head are in none. */
    QLC_ENGINE_EXPORT QList<Section> sectionsOf(const CueList &list, Numbering numbering);
    /** The index in @a sections of the one cue @a index is in (-1: none). */
    QLC_ENGINE_EXPORT int sectionOfIndex(const QList<Section> &sections, int index);
    /** What a section header row says: "1 — Everybody Scream", "200", or
     *  (Manual, no name) "Section". */
    QLC_ENGINE_EXPORT QString sectionTitle(const Section &section);

    /** A section's identity in a running order: its key, or (free numbers)
     *  "#" and its head cue's id. Empty for cues in no section. */
    QLC_ENGINE_EXPORT QString sectionId(const Section &section);
    /** Tonight's running order resolved against @a sections (a list's, in
     *  number order): @a order's songs that still exist, in its order, then
     *  every other section (a new song) after the section before it in
     *  number order. With @a order empty, number order. Each id once. */
    QLC_ENGINE_EXPORT QList<RunningSong> resolveRunningOrder(const QList<Section> &sections,
                                                             const QList<RunningSong> &order);
    /** The cues of @a list in the order the Run plays them: their indexes,
     *  section by section as resolveRunningOrder() says, each song's cues
     *  in list order. The identity for an empty @a order or no scheme.
     *  @a skipped (optional) gets, per list index, whether its song is left
     *  out tonight. */
    QLC_ENGINE_EXPORT QList<int> playOrder(const CueList &list, Numbering numbering,
                                           const QList<RunningSong> &order, QList<bool> *skipped = nullptr);

    /** The scheme the cue numbers of @a production look like they follow
     *  (a new show's suggestion, confirmed in the setting): most numbers
     *  with three levels or more -> ShowPartIteration; most whole numbers
     *  of 100 or more in two hundreds or more -> Hundreds; else None. */
    QLC_ENGINE_EXPORT Numbering suggestNumbering(const Production &production);

    /** The number for a cue inserted after @a afterIndex (-1: at the top)
     *  that stays in section @a key under @a numbering: between its
     *  neighbours, none of @a taken, its key @a key ("1.0.3" .. "2.0.1" in
     *  section 1 -> "1.0.4"; at the top of section 2 -> "2"; into an empty
     *  section 3 -> "3.0.1"). Empty: none fits (or no scheme). */
    QLC_ENGINE_EXPORT QString numberInSection(const CueList &list, Numbering numbering, int afterIndex,
                                              const QString &key, const QStringList &taken = QStringList());
    /** numberForInsert() that keeps the new cue in the section of the cue
     *  it follows (at the top: of the first cue) under @a numbering, when
     *  the plain rule would leave it ("1" .. "3" in section 1 -> "1.5", not
     *  "2"). */
    QLC_ENGINE_EXPORT QString numberForInsertIn(const CueList &list, Numbering numbering, int afterIndex,
                                                const QStringList &taken = QStringList());
    /** A new section after the last (Q190): its key and its first cue's
     *  number ("4", "4.0.1"; "400", "400"). False for None and Manual. */
    QLC_ENGINE_EXPORT bool nextSection(const CueList &list, Numbering numbering, QString *key, QString *number);

    /** What Merge lists into one did (Q190). */
    struct QLC_ENGINE_EXPORT MergeReport
    {
        /** "Opener 1.0.1 → 1.0.1.5": the cues renumbered (only on a clash). */
        QStringList renumbered;
        /** The cues set to Block so they still start from nothing, by id. */
        QList<quint32> blocked;
        /** What couldn't be kept as it was, a line each. */
        QStringList notes;
    };

    /** Merge lists @a listIds of @a production (in that order, two or more,
     *  none a Looks section or sub-cues) into the first: their cues in
     *  order, keeping their numbers but where one clashes; each later
     *  list's start Block (and a cue that plays its list's default mode set
     *  to it), so every cue plays as it did; each list a section named
     *  after it. The others are removed; the merged list is the main list
     *  if one of them was. Numbering None becomes the suggested scheme,
     *  else Manual. Pure: no Doc. False (@a why): nothing merged. */
    QLC_ENGINE_EXPORT bool mergeLists(Production &production, const QList<quint32> &listIds,
                                      MergeReport *report = nullptr, QString *why = nullptr);

    /************************************************************************
     * Text colours (Cue::colour, CueList::colour)
     ************************************************************************/

    /** One of the colours offered for a cue's text: show-friendly, each
     *  readable on light and dark backgrounds once readableOn() has had a
     *  say. */
    struct QLC_ENGINE_EXPORT TextColour
    {
        QString name;           //!< "Red"
        QString hex;            //!< "#e53935"
    };
    QLC_ENGINE_EXPORT const QList<TextColour> &textColours();
    /** A text colour from what a person types: a name of textColours()
     *  ("red", any case), "#rgb" / "#rrggbb", or "none" / "off" / "clear"
     *  (empty: the colour is taken away). False when it is none of these. */
    QLC_ENGINE_EXPORT bool parseTextColour(const QString &text, QString *hex);
    /** What a person reads for @a hex: its name if it is one of
     *  textColours(), else the hex; "" for none. */
    QLC_ENGINE_EXPORT QString textColourName(const QString &hex);
    /** WCAG's contrast ratio of two colours (1..21). */
    QLC_ENGINE_EXPORT double contrastRatio(const QColor &a, const QColor &b);
    /** @a colour as text on @a background: itself when it reads (@a ratio,
     *  WCAG's 4.5:1 for text), else the nearest variant of it that does --
     *  lighter on a dark background, darker on a light one, the same hue. */
    QLC_ENGINE_EXPORT QColor readableOn(const QColor &colour, const QColor &background, double ratio = 4.5);

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

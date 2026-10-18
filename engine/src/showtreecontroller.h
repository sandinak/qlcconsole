/*
  Q Light Controller Plus
  showtreecontroller.h

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

#ifndef SHOWTREECONTROLLER_H
#define SHOWTREECONTROLLER_H

#include <functional>
#include <QStringList>
#include <QObject>
#include <QHash>
#include <QSet>

#include "qlcengine_export.h"
#include "functionparent.h"
#include "showtreemodel.h"
#include "qlcpalette.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class QTimer;
class Scene;
class Doc;

#define KXMLQLCShowTree QStringLiteral("ShowTree")

/** @addtogroup engine Engine
 * @{
 */

/**
 * The Show Tree's productions, and the functions they bake to
 * (SHOW_TREE_DESIGN.md §4): a cue list is a Chaser, a cue a Collection
 * holding its complete output, a resolved layer a generated Scene --
 * one per distinct layer, shared by every cue where it is the same, so it
 * plays on across GO (ChaserRunner's hand-over).
 *
 * Every edit goes through edit(): one undo step, then a re-bake. A bake is
 * deterministic from the tree, so undo is "put the tree back and bake".
 *
 * Owned by Doc (Doc::showTree()); Doc only forwards load/save/clear.
 */
class QLC_ENGINE_EXPORT ShowTreeController final : public QObject
{
    Q_OBJECT

public:
    explicit ShowTreeController(Doc *doc);
    ~ShowTreeController();

    /************************************************************************
     * The tree
     ************************************************************************/
public:
    const QList<ShowTree::Production> &productions() const { return m_productions; }
    const ShowTree::Production *production(quint32 id) const;
    /** The production and list a list id belongs to (null: none). */
    const ShowTree::CueList *cueList(quint32 listId, quint32 *productionId = nullptr) const;
    /** The list a cue belongs to. */
    const ShowTree::CueList *listOfCue(quint32 cueId) const;
    const ShowTree::Cue *cue(quint32 cueId) const;

    /** A new node id (production, list, cue, layer): unique in the show. */
    quint32 newId();

    /** @a list resolved (ShowTree::resolve) with this show's palettes:
     *  a Position look aims at a line when its Aim palette's target is a
     *  line (D18). Everything that reads the cascade goes through this. */
    QList<ShowTree::ResolvedCue> resolved(const ShowTree::CueList &list) const;
    /** Whether @a look (a Position) aims at a line target. */
    bool isLineAim(const ShowTree::Look &look) const;
    ShowTree::LineAimTest lineAimTest() const;

    /************************************************************************
     * Group sets (the Library's "Make set", §1)
     ************************************************************************/
public:
    const QList<ShowTree::GroupSet> &groupSets() const { return m_groupSets; }
    const ShowTree::GroupSet *groupSet(quint32 id) const;
    /** Make set: a named set of groups (and fixtures), e.g. Towers = SL +
     *  SR. One undo step. A name the show already has gets " (2)". */
    quint32 addGroupSet(const QString &name, const QList<quint32> &groups,
                        const QList<quint32> &fixtures = QList<quint32>());
    /** Its members: every layer on the set follows (it stays the same
     *  layer, so tracking is kept). One undo step. */
    bool setGroupSetMembers(quint32 setId, const QList<quint32> &groups,
                            const QList<quint32> &fixtures = QList<quint32>());
    bool renameGroupSet(quint32 setId, const QString &name);
    /** Refused (@a why names the cues) while a layer uses it. */
    bool removeGroupSet(quint32 setId, QString *why = nullptr);
    /** @a target with its sets replaced by their members (groups, then
     *  fixtures), once each, in order: what a generated scene plays. */
    ShowTree::Target expandTarget(const ShowTree::Target &target) const;
    /** Every fixture @a target lights (sets and groups expanded), once. */
    QList<quint32> targetFixtures(const ShowTree::Target &target) const;

    /************************************************************************
     * Child layers (§3.5)
     ************************************************************************/
public:
    /** A child layer of layer @a parentId on @a target, which must be part
     *  of the parent's (a member of its set, a sub-group, some of its
     *  fixtures): put after the parent's other children. It sets only what
     *  it sets; the parent lights the rest of it, and doesn't light what it
     *  sets (no show-through). One undo step. Refused (@a why) when the
     *  target isn't inside the parent's. */
    quint32 addChildLayer(quint32 cueId, quint32 parentId, const ShowTree::Target &target,
                          QString *why = nullptr);
    /** Whether @a inner lights only fixtures @a outer lights. */
    bool targetContains(const ShowTree::Target &outer, const ShowTree::Target &inner) const;
    /** The layers of the cue under layer @a layerId, at any depth. */
    QList<quint32> childLayers(quint32 cueId, quint32 layerId) const;

    /************************************************************************
     * Line aims (§3B): Position aimed at a line, with Spread / Grouping /
     * Cross as keys of their own (D16)
     ************************************************************************/
public:
    /** A look setting a line parameter: @a key KeySpread ("-5".."5"),
     *  KeyGrouping (each, pairs, threes, fours, halves) or KeyCross (swap,
     *  mirror). Empty look (key only): @a value doesn't read. */
    static ShowTree::Look lineParamLook(const QString &key, const QString &value);
    /** "+2.5" -> "2.5"; "pair" -> "pairs"; "mirror" -> "mirror". Empty: no. */
    static QString lineParamValue(const QString &key, const QString &word);
    /** What a person reads for a line parameter value: "+2.5", "Pairs",
     *  "Each group flips (mirror)". */
    static QString lineParamText(const QString &key, const QString &value);
    /** The Aim palette pointing at stage target @a targetId (a line, a point,
     *  an area): a named one already pointing there with nothing else set
     *  is reused (the palette reuse rule), else one is made, named after the
     *  target, in the show's Aim palette folder. Invalid: no such target. */
    quint32 aimPaletteFor(quint32 targetId);
    /** The generated scene layer @a target plays in cue @a cueId (after the
     *  last bake; invalid: none), for the order strip. */
    quint32 layerScene(quint32 cueId, const ShowTree::Target &target) const;
    /** The layers @a target's @a key is ONE effect / ONE formation with in
     *  cue @a cueId (itself included; fewer than two: on its own). */
    QList<ShowTree::Target> spanMembers(quint32 cueId, const ShowTree::Target &target,
                                        const QString &key) const;

    /************************************************************************
     * Editing: each is one undo step, and bakes
     ************************************************************************/
public:
    /** Run @a change on the tree as one undo step named @a label, then bake.
     *  @a change returns false to leave everything as it was. */
    bool edit(const QString &label, const std::function<bool(QList<ShowTree::Production> &)> &change);

    quint32 addProduction(const QString &name);
    /** A new cue list: **Tracking**, the point of the tab -- the same from
     *  + Cue list and from `list "name"`. Only an imported chaser starts
     *  Non-tracking (D12, importChaser()), so it plays as it did. */
    quint32 addCueList(quint32 productionId, const QString &name,
                       ShowTree::ListMode mode = ShowTree::ListMode::Tracking);
    /** A new cue after @a afterIndex (-1: at the top), numbered per D23. */
    quint32 addCue(quint32 listId, int afterIndex, const QString &name);
    bool removeCue(quint32 cueId);
    bool moveCue(quint32 cueId, int toIndex);
    bool setCue(const ShowTree::Cue &cue);   //!< its own fields; layers and looks kept as given
    quint32 addLayer(quint32 cueId, const ShowTree::Target &target, const QString &name = QString());
    bool removeLayer(quint32 cueId, quint32 layerId);
    bool setLayerLook(quint32 cueId, quint32 layerId, const ShowTree::Look &look);
    bool setCueLook(quint32 cueId, const ShowTree::Look &look);
    /** Set a look on the cue's own layer for @a target -- made one first if
     *  the layer is only carried in (tracked, included): one undo step. */
    bool setTargetLook(quint32 cueId, const ShowTree::Target &target, const ShowTree::Look &look);
    bool setLayerRelease(quint32 cueId, quint32 layerId, bool release);
    /** Give a layer another target (groups and fixtures): one undo step.
     *  The layer stays the same layer -- a layer it carried on from (tracked,
     *  included) keeps giving it its values, and the same layer set again in
     *  the following cues it tracks into is changed with it, so nothing
     *  lights the old target beside the new one. */
    bool setLayerTarget(quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                        const QString &label = QString());
    /** Add groups / fixtures to a layer's target (dropped onto it). */
    bool addToLayer(quint32 cueId, quint32 layerId, const QList<quint32> &groups,
                    const QList<quint32> &fixtures, const QList<quint32> &sets = QList<quint32>());
    /** Take a group (or a fixture) out of a layer's target; never its last. */
    bool removeGroupFromLayer(quint32 cueId, quint32 layerId, quint32 groupId);
    bool removeFixtureFromLayer(quint32 cueId, quint32 layerId, quint32 fixtureId);
    bool setListMode(quint32 listId, ShowTree::ListMode mode);
    bool setBaseCue(quint32 listId, quint32 cueId);
    bool setCueMode(quint32 cueId, ShowTree::CueMode mode);
    /** Layers of the cue on fixtures in common: the lower wins what both
     *  set (false, the tree's rule), or they mix as a chaser's collection
     *  does (true, how an import keeps them). One undo step. */
    bool setCueMixOverlap(quint32 cueId, bool mix);

    /** Release (or carry) the layer on @a target in cue @a cueId -- made the
     *  cue's own first if it is only carried in: one undo step. */
    bool setTargetRelease(quint32 cueId, const ShowTree::Target &target, bool release);

    /** Includes (D8): add one at the end, take one out, or move one to
     *  @a toIndex in the cue's include order (later wins). An include that
     *  would loop is refused (ShowTree::canInclude); @a why says why. */
    bool addInclude(quint32 cueId, quint32 includedId, QString *why = nullptr);
    bool removeInclude(quint32 cueId, quint32 includedId);
    bool moveInclude(quint32 cueId, quint32 includedId, int toIndex);

    /** A new cue numbered @a number (D23 is not asked: the number is
     *  given), put in number order. Invalid when the number is taken. */
    quint32 addCueNumbered(quint32 listId, const QString &number, const QString &name);
    /** A copy of cue @a cueId after it (its own layers, looks, mode,
     *  includes and timing; never the base), numbered per D23. */
    quint32 duplicateCue(quint32 cueId);
    /** Paste copies of cues @a cueIds (any list, in the order given) as a
     *  run right after cue @a afterCueId, each numbered per D23 after the
     *  one before (never renumbering others): their own layers (the same
     *  targets, so they track as any cue's own), looks, mode, timing, note,
     *  name and overlap; includes only within the same list; never the
     *  base flag. One undo step. The new cue ids (empty: nothing pasted). */
    QList<quint32> pasteCues(const QList<quint32> &cueIds, quint32 afterCueId);
    /** Paste copies of layers (cue, layer pairs) into cue @a toCueId: one
     *  undo step. A layer on a target the cue already has sets its looks
     *  there instead (one layer per target). A child keeps its parent when
     *  the parent comes too, or the cue has a layer on the parent's target.
     *  The layers pasted (or set). */
    QList<quint32> pasteLayers(const QList<QPair<quint32, quint32>> &layers, quint32 toCueId);
    /** Record as new cue (D22): what cue @a cueId puts on stage -- tracked,
     *  included and its own, resolved -- frozen into a new Block cue after
     *  it, numbered per D23. Every layer becomes the new cue's own with the
     *  values it has there. */
    quint32 recordCue(quint32 cueId);

    /** What Record as new cue records (D22, open question 9): */
    enum class RecordSource
    {
        Selected,   //!< the selected cue's resolved stage (what Design previews)
        Running,    //!< the current cue of the selected cue's list, running for real
        Lit         //!< everything on stage: every list running for real, the
                    //!< Design preview on top, and any other scene / EFX / RGB
                    //!< matrix running (as opaque function layers)
    };
    static QString recordSourceToString(RecordSource source);   //!< "selected", "running", "lit"
    static bool stringToRecordSource(const QString &str, RecordSource *source);

    /** Record @a source as a new Block cue after cue @a cueId (numbered per
     *  D23), one undo step. Invalid when @a source isn't available (nothing
     *  running) or the list is gone. */
    quint32 recordCue(quint32 cueId, RecordSource source);
    /** The layers @a source would record with cue @a cueId selected;
     *  @a available false when there is nothing of that kind (no list
     *  running). */
    QList<ShowTree::Layer> recordLayers(quint32 cueId, RecordSource source, bool *available = nullptr) const;
    /** The choices that are meaningful now, in order: available, putting
     *  something on stage, and each different from the ones before it (the
     *  running cue when it is the selected one is not offered twice). One
     *  or none: nothing to ask. */
    QList<RecordSource> recordChoices(quint32 cueId) const;
    /** Whether @a source may be recorded now: it is available and puts
     *  something on stage (it may be the same as another choice). */
    bool canRecord(quint32 cueId, RecordSource source) const;
    /** The cue a list's chaser is playing now, running for real (invalid:
     *  not running). */
    quint32 runningCue(quint32 listId) const;

    /** Split a layer on several groups into one layer per group (and one on
     *  its fixtures), each with its looks: one undo step. The layer keeps
     *  tracking as before -- the pieces carry on from what it carried on
     *  from, and where the same layer is set again in the following cues it
     *  tracks into, it is split there too. Refused (@a why) when it would
     *  change what any cue plays. */
    bool splitLayer(quint32 cueId, quint32 layerId, QList<quint32> *made = nullptr, QString *why = nullptr);
    /** Merge layers of one cue into one layer on all their groups: one undo
     *  step. Refused (@a why) when they set an attribute differently
     *  ("Towers sets Colour Blue, USTs Red"), one is released and another
     *  not, or the merge would change what any cue plays (a following cue
     *  sets one of them on its own). The same layers set again in the
     *  following cues they track into are merged there too. Returns the
     *  merged layer (invalid: refused). */
    quint32 mergeLayers(quint32 cueId, const QList<quint32> &layerIds, QString *why = nullptr);
    /** What a person reads for a stage unit ("g:12" -> the group's name). */
    QString unitLabel(const QString &unit) const;

    /** Where the value of @a key on @a target in cue @a cueId was set (D22):
     *  the cue, and the layer there (invalid: the cue's own look). False
     *  when it is set in this cue itself, or nowhere. Follows tracking,
     *  includes and the base back to the cue that sets it. */
    bool valueSource(quint32 cueId, const ShowTree::Target &target, const QString &key,
                     quint32 *sourceCue, quint32 *sourceLayer = nullptr) const;
    /** "Update cue 1.0.0" (D22): set @a look where the value @a target has
     *  in cue @a cueId was set, so the change tracks on from there. One
     *  undo step. False when it is this cue's own value. */
    bool updateSource(quint32 cueId, const ShowTree::Target &target, const ShowTree::Look &look);

    /** Several edits as one undo step (a command line command): edits made
     *  between beginBatch() and endBatch() are applied at once but bake and
     *  record undo only at the end. endBatch(false) puts everything back. */
    void beginBatch(const QString &label);
    void endBatch(bool keep);
    bool inBatch() const { return m_batchDepth > 0; }

    /** The cues whose Collection the last bake changed (what they play),
     *  in tree order: the re-bake chain of the last edit, as it came out. */
    QList<quint32> lastBakeChanged() const { return m_lastChanged; }

    /** What the cues of a list use, with counts (Library "In this cue
     *  list", D20): palettes (cue and layer looks, an effect's own), groups,
     *  fixtures and functions (opaque layers). */
    struct Usage
    {
        QHash<quint32, int> palettes;
        QHash<quint32, int> groups;
        QHash<quint32, int> fixtures;
        QHash<quint32, int> functions;
        QHash<quint32, int> sets;
    };
    Usage usageInList(quint32 listId) const;
    bool renameNode(quint32 nodeId, const QString &name);
    bool removeCueList(quint32 listId);
    bool removeProduction(quint32 productionId);

    /************************************************************************
     * Reusable layers and cues (P4): Make reusable, uses, Unlink here
     ************************************************************************/
public:
    const QList<ShowTree::ReusableLayer> &reusableLayers() const { return m_reusableLayers; }
    const QList<ShowTree::ReusableCue> &reusableCues() const { return m_reusableCues; }
    const ShowTree::ReusableLayer *reusableLayer(quint32 id) const;
    const ShowTree::ReusableCue *reusableCue(quint32 id) const;
    /** The name of reusable @a id (a layer or a cue); empty: none. */
    QString reusableName(quint32 id) const;

    /** Make reusable (§1): the layer's name, target and looks become a
     *  reusable layer in the Library named @a name (empty: the layer's
     *  name, else its groups'), and the layer its first use. Every use
     *  stays live-linked: an edit to any of them is an edit to all. One
     *  undo step. Refused (invalid, @a why) for a child layer, a layer
     *  already reusable, or a layer of a reusable cue's use. */
    quint32 makeLayerReusable(quint32 cueId, quint32 layerId, const QString &name = QString(),
                              QString *why = nullptr);
    /** Make reusable for a cue: its name, mode, overlap, cue looks and
     *  layers become a reusable cue (a "Blackout", a "Walk-in"), the cue
     *  its first use. Number, timing, note and includes stay each use's
     *  own. One undo step. */
    quint32 makeCueReusable(quint32 cueId, const QString &name = QString(), QString *why = nullptr);
    /** A use of reusable layer @a reusableId in cue @a cueId (a new layer,
     *  linked). One undo step. Invalid: no such cue or reusable. */
    quint32 insertReusableLayer(quint32 cueId, quint32 reusableId);
    /** A use of reusable cue @a reusableId as a new cue of list @a listId
     *  after @a afterIndex (-1: at the top), numbered per D23, starting
     *  with the reusable's timing. One undo step. */
    quint32 insertReusableCue(quint32 listId, int afterIndex, quint32 reusableId);
    /** Unlink here: this use becomes a plain local copy, as it is now; the
     *  other uses stay linked. One undo step. */
    bool unlinkLayer(quint32 cueId, quint32 layerId);
    bool unlinkCue(quint32 cueId);
    /** Rename a reusable: every use takes the name. One undo step. */
    bool renameReusable(quint32 reusableId, const QString &name);

    /** One use of a reusable: its list, cue and (a layer's) layer. */
    struct ReusableUse
    {
        quint32 list = 0xFFFFFFFF;
        quint32 cue = 0xFFFFFFFF;
        quint32 layer = 0xFFFFFFFF;     //!< invalid for a reusable cue's use
        QString label;                  //!< "Full Show 0.0.3 Announcer" (+ " — BackStage")
    };
    /** Every use of reusable @a reusableId, in tree order. */
    QList<ReusableUse> reusableUses(quint32 reusableId) const;
    /** Delete a reusable. While it is used it is refused (@a why names the
     *  uses), unless @a unlinkUses: then every use becomes a plain copy as
     *  it is, and the reusable goes -- one undo step. */
    bool removeReusable(quint32 reusableId, bool unlinkUses = false, QString *why = nullptr);

    /************************************************************************
     * The Looks section (PROGRAMMING_ROLE_DESIGN.md §4.1, P-D5..P-D7, R3):
     * busking looks beside a production's cue lists. A look is a cue of the
     * production's Looks list (CueList::looks) -- the same layers, rows and
     * cascade, never tracking -- baked to a Collection of its own that VC
     * buttons fire like any function.
     ************************************************************************/
public:
    /** Production @a productionId's Looks section (invalid: none yet). */
    quint32 looksSection(quint32 productionId) const;
    /** Make it (one undo step); its id, or the one already there. */
    quint32 addLooksSection(quint32 productionId);
    bool isLooks(quint32 listId) const;
    /** Whether cue @a cueId is a look (of a Looks section). */
    bool isLook(quint32 cueId) const;
    /** A new look in production @a productionId's Looks section (made
     *  first, in the same undo step, when there is none), after look
     *  @a afterId (invalid: at the end), filed in @a folder. One undo step. */
    quint32 addLook(quint32 productionId, const QString &name, const QString &folder = QString(),
                    quint32 afterId = ShowTree::invalidId());
    /** File looks @a lookIds in @a folder ("": none). One undo step. */
    bool setLookFolder(const QList<quint32> &lookIds, const QString &folder);
    /** A folder with no look in it yet (New folder). One undo step. */
    bool addLookFolder(quint32 listId, const QString &folder);
    /** Every folder of a Looks section ("Hits", "Hits/Big"), parents too,
     *  sorted. */
    QStringList lookFolders(quint32 listId) const;
    /** Rename folder @a folder (its last part) to @a name: its looks and
     *  folders under it follow. One undo step. */
    bool renameLookFolder(quint32 listId, const QString &folder, const QString &name);
    /** Delete folder @a folder: what is in it moves up one level. One undo
     *  step. */
    bool removeLookFolder(quint32 listId, const QString &folder);
    /** The looks in @a folder and under it ("": every look), in order. */
    QList<quint32> looksIn(quint32 listId, const QString &folder) const;
    /** What a VC button fires for look @a lookId: its Collection (invalid:
     *  not baked yet). */
    quint32 lookFunction(quint32 lookId) const;
    /** The look whose Collection is @a functionId (invalid: none). */
    quint32 lookOfFunction(quint32 functionId) const;
    /** Adopt as look (P-D16): scene @a functionId as a new look of
     *  production @a productionId -- its targets and palettes as a layer
     *  when it reads as one (as an import does), else the scene played as
     *  it is (an opaque layer). The scene stays. One undo step; the new
     *  look (invalid, @a why: refused). */
    quint32 adoptAsLook(quint32 productionId, quint32 functionId, QString *why = nullptr);
    /** Copy to look (§4.1): what cue @a cueId puts on stage -- tracked,
     *  included and its own, resolved -- frozen into a new look of its
     *  production, named after the cue. No live link. One undo step. */
    quint32 copyCueToLook(quint32 cueId);

    /************************************************************************
     * Detach a cue list to a plain chaser (D13 for lists, P4)
     ************************************************************************/
public:
    /** The list's generated Chaser and its cue Collections become the
     *  show's own, ordinary unlocked functions (shown in the Functions
     *  window, where they are) and the list leaves the tree. The scenes
     *  they play that no other list plays go with them; one another list
     *  still plays stays the tree's (locked) until the tree lets go of it,
     *  then it is handed over too, never deleted. One-off palettes they
     *  play become ordinary palettes. One undo step: undo puts the list
     *  back on the same functions. The chaser (invalid: refused). */
    quint32 detachList(quint32 listId);

    /************************************************************************
     * Convert to tracking (§7, P4)
     ************************************************************************/
public:
    /** What Convert to tracking… would do to a list: the list as it would
     *  be, and what changes, as a person reads it. */
    struct ConvertPlan
    {
        bool ok = false;
        QString why;                        //!< not ok: why not
        ShowTree::CueList list;             //!< the list after
        QStringList changes;                //!< "0.0.4 SHOUT: tracks from 0.0.3 (include taken out)"
        int tracking = 0;                   //!< cues that now track
        int blocks = 0;                     //!< cues kept Block (they'd change otherwise)
        int looksRemoved = 0;               //!< values that only repeated what tracks in
        int layersRemoved = 0;              //!< layers left with nothing of their own
        int includesRemoved = 0;            //!< "includes the cue before" made tracking
        int releases = 0;                   //!< Release set so a layer doesn't track on
    };
    /** Plan Convert to tracking for an imported (non-tracking) list (§7):
     *  each cue tracks where it then plays exactly as before (else it stays
     *  Block); an include of the cue before becomes tracking; a layer that
     *  the next cue doesn't light is released there; a value equal to what
     *  tracks in is taken out (a layer left with nothing is taken out, if it
     *  overlaps no other). What every cue puts on stage is unchanged, checked
     *  cue by cue. Reusable uses are left as they are. Nothing is changed. */
    ConvertPlan planConvertToTracking(quint32 listId) const;
    /** Apply the plan: one undo step. False: nothing to do, or refused. */
    bool convertToTracking(quint32 listId, ConvertPlan *plan = nullptr);

    /************************************************************************
     * Looks and palettes
     ************************************************************************/
public:
    /** The attribute key a palette sets ("Colour", "Effect.Color"...). */
    QString keyForPalette(quint32 paletteId) const;
    /** An effect palette's kind: "Color", "Dimmer", "Position", "Beam" or
     *  "Other", from its script's fixture types. */
    QString effectKind(const QLCPalette *palette) const;
    /** A look that sets @a paletteId on its attribute. */
    ShowTree::Look lookForPalette(quint32 paletteId) const;

    /** A one-off value (D11, D21): an unnamed palette owned by the tree,
     *  hidden from the palette tree. Returns its id. */
    quint32 createInlinePalette(QLCPalette::PaletteType type, const QVariantList &values,
                                const QString &label);
    bool isInlinePalette(quint32 paletteId) const { return m_inlinePalettes.contains(paletteId); }
    /** Make an inline palette an ordinary named one (Make reusable): one
     *  undo step (D22) -- undo makes it a one-off again, as it was. */
    bool makeReusable(quint32 paletteId, const QString &name, const QString &path);
    /** Make reusable in one click: named and filed as the show's own
     *  palettes of that type are (suggestPaletteName/-Folder). */
    bool makeReusable(quint32 paletteId);
    /** The same for several one-offs at once (a row's colours 1, 2...):
     *  one undo step. */
    bool makeReusable(const QList<quint32> &paletteIds);

    /** A named palette of @a type that plays exactly @a values (a Dimmer
     *  level 0..255, a Colour string), to reuse instead of making another
     *  (D11/D21 as changed 2026-10-17): first one already used in list
     *  @a listId (most used, then lowest id), then the most recent of
     *  @a recent, then any (by name, then id). One-off (inline) palettes and
     *  fanned ones never match. Dimmer and Colour only. Invalid: none. */
    quint32 findPalette(QLCPalette::PaletteType type, const QVariantList &values,
                        quint32 listId = ShowTree::invalidId(),
                        const QList<quint32> &recent = QList<quint32>()) const;
    /** A name for a new palette of @a type playing @a values, learned from
     *  the show's named palettes of that type: one whose name holds its own
     *  value ("Intensity 45%" at 115, "Colour #ff0000") gives the pattern
     *  ("Intensity 30%"); else Dimmer "Dimmer 30%", Colour the colour's
     *  name ("Red", "Sea Green") as palettes named by colour are, or its
     *  hex. Never a name already taken by that type. */
    QString suggestPaletteName(QLCPalette::PaletteType type, const QVariantList &values) const;
    /** Where the show keeps named palettes of @a type: their most common
     *  folder ("Palettes/Dimmer"); "Palettes/<Type>" when there are none. */
    QString suggestPaletteFolder(QLCPalette::PaletteType type) const;
    /** A new named, reusable palette, named and filed per the show's own
     *  pattern. In a batch (a command) it is made reusable only when the
     *  command succeeds: a failed one leaves no palette behind. */
    quint32 createNamedPalette(QLCPalette::PaletteType type, const QVariantList &values);

    /** How many of @a target's fixtures a look of @a key can reach ("applies
     *  to 4 of 8 fixtures", §3A): @a reach of @a total. */
    void lookReach(const ShowTree::Target &target, const QString &key, int *reach, int *total) const;
    /** The target's fixtures a look of @a key can't reach (they ignore it). */
    QList<quint32> lookMisses(const ShowTree::Target &target, const QString &key) const;
    /** The same for @a look itself: a Gobo look by name reaches only the
     *  fixtures whose wheel has that gobo (or one picked for their type). */
    void lookReach(const ShowTree::Target &target, const ShowTree::Look &look, int *reach, int *total) const;
    QList<quint32> lookMisses(const ShowTree::Target &target, const ShowTree::Look &look) const;
    /** A Gobo look by name that is ambiguous on some of @a target's fixture
     *  types (several slots match, so it is not guessed): one entry per
     *  type, "Model: Dots Shake / Dots Rotating". Empty: none. */
    QStringList lookAmbiguities(const ShowTree::Target &target, const ShowTree::Look &look) const;

    /** What a person reads for @a target: its groups' and fixtures' names,
     *  or the function's. */
    QString targetLabel(const ShowTree::Target &target) const;
    /** What a layer on @a target holds, with counts: "TrussC-RGB (8) ·
     *  TrussW-RGB (8) — 16 fixtures"; a set "Towers: SL (2) · SR (2) — 4
     *  fixtures"; a function layer "plays Haze". */
    QString targetContents(const ShowTree::Target &target) const;
    /** "0.0.4 SHOUT" */
    QString cueLabel(const ShowTree::CueList &list, int index) const;

    /************************************************************************
     * Undo
     ************************************************************************/
public:
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString undoText() const;
    QString redoText() const;
    bool undo();
    bool redo();
    void clearUndo();
    void clearRedo() { m_redo.clear(); }
    /** When the top undo / redo step was taken (0: none), comparable with
     *  the palette stack's (PaletteOps::nextUndoStamp()). */
    quint64 undoStamp() const { return m_undo.isEmpty() ? 0 : m_undo.last().stamp; }
    quint64 redoStamp() const { return m_redo.isEmpty() ? 0 : m_redo.last().stamp; }

    /** The tab's Undo / Redo (P-D14): the tree's steps and the palette
     *  steps (PaletteOps: + Palette, rename, folders, delete -- from the
     *  Library, the command line or the Programming tab) are one history;
     *  undo takes back whichever was done last, redo puts back whichever
     *  was undone last. */
    bool undoShared();
    bool redoShared();
    bool canUndoShared() const;
    bool canRedoShared() const;
    QString sharedUndoText() const;
    QString sharedRedoText() const;

private:
    struct Snapshot
    {
        QString label;
        QByteArray xml;
        quint64 stamp = 0;
    };
    QByteArray snapshotXml() const;
    void restoreSnapshot(const QByteArray &xml);

    QList<Snapshot> m_undo;
    QList<Snapshot> m_redo;
    static const int kUndoLimit = 200;

    /************************************************************************
     * Baking
     ************************************************************************/
public:
    /** Bake every production into functions; drop what's no longer used.
     *  Deterministic: the same tree always gives the same functions. */
    void bake();

    /** Whether @a functionId was generated by the tree (and so is locked,
     *  D13). */
    bool isGenerated(quint32 functionId) const;
    /** For a generated function: the cue (or list) it plays, "Cue 0.0.4
     *  SHOUT in Full Show"; empty otherwise. Delete/usage dialogs name cues,
     *  not generated scenes. */
    QString describeGenerated(quint32 functionId) const;

    /** The ordered palette list a resolved layer's scene plays (the look
     *  stack: base looks, then effects each with its own). */
    static QList<quint32> scenePalettes(const ShowTree::ResolvedLayer &layer);

    /** Detach a generated scene: a plain copy the tree then plays as an
     *  opaque function layer in @a cueId in place of @a layerId (D13).
     *  One undo step: undo puts the layer back and deletes the copy (redo
     *  brings it back as it was). */
    quint32 detachLayer(quint32 cueId, quint32 layerId);

    /** D13, said wherever a generated function would be edited or deleted
     *  outside the tree: "Made by the Show Tree — edit it there." */
    static QString lockedText();
    /** Where generated function @a functionId is in the tree: its cue list
     *  (@a listId); for a cue's Collection the cue; for a layer's Scene the
     *  first cue where it is a layer of the cue's own (@a layerId: that
     *  layer -- what Detach detaches), else the first cue playing it. False
     *  when it isn't generated or no longer in the tree. */
    bool generatedNode(quint32 functionId, quint32 *listId, quint32 *cueId = nullptr,
                       quint32 *layerId = nullptr) const;

private:
    /** One generated scene a cue plays: what a resolved layer bakes to,
     *  with what it leaves out for a lower layer (§3.5) and what it lays out
     *  over with others (D10, D17). */
    struct ScenePlan
    {
        int layer = -1;                     //!< in the resolved cue's layers
        ShowTree::Target target;            //!< sets expanded
        QList<quint32> palettes;
        QHash<quint32, QList<quint32> > excluded;
        QHash<quint32, QList<quint32> > spanGroups;
        QHash<quint32, QList<quint32> > spanFixtures;
        /** Per attribute key: the layers (indexes) it is one with. */
        QHash<QString, QList<int> > spanMembers;
        QString signature() const;
    };
    /** The scenes cue @a index of @a list plays, from @a resolved. A line
     *  aim with its own parameters plays an inline Aim palette made for
     *  them: made when @a create, else only found. */
    QList<ScenePlan> planCue(const ShowTree::CueList &list, const QList<ShowTree::ResolvedCue> &resolved,
                             int index, bool create);
    QList<ScenePlan> planCue(const ShowTree::CueList &list, const QList<ShowTree::ResolvedCue> &resolved,
                             int index) const;
    /** The Aim palette a line aim with @a layer's Spread / Grouping / Cross
     *  plays (@a basePid itself when they are its own). */
    quint32 lineAimPalette(quint32 basePid, const ShowTree::ResolvedLayer &layer, bool create);
    QString sceneSignature(const Scene *scene) const;
    static QString sceneSignature(const ShowTree::Target &target, const QList<quint32> &palettes);
    void deleteUnused(const QSet<quint32> &keep, QSet<quint32> &generated);
    void dropUnusedInlinePalettes();

    QSet<quint32> m_generatedScenes;
    QSet<quint32> m_generatedCollections;
    QSet<quint32> m_generatedChasers;
    QSet<quint32> m_inlinePalettes;
    /** One-offs made reusable here (and palettes a command made): an undo
     *  snapshot carries them as they are, so redo can name them again. */
    QSet<quint32> m_madeReusable;
    /** Scenes Detach made: an undo snapshot carries them as they are, and
     *  restoring one made before a detach deletes that detach's scene. */
    QSet<quint32> m_detached;
    /** The detached scenes the snapshot being restored holds. */
    QSet<quint32> m_snapshotDetached;
    /** Put an undo step (@a before: the state before it) on the stack. */
    void pushUndo(const QString &label, const QByteArray &before);
    bool applyReusable(quint32 paletteId, const QString &name, const QString &path);
    /** What a one-off would be named and filed as, made reusable now. */
    void reusableName(quint32 paletteId, QString *name, QString *path);
    /** What a generated function plays, for describeGenerated(). */
    QHash<quint32, QString> m_generatedLabels;
    /** After a bake: "cue|target signature" -> its scene; and -> key ->
     *  the targets it is one span with. */
    QHash<QString, quint32> m_layerScenes;
    QHash<QString, QHash<QString, QList<ShowTree::Target> > > m_spanMembers;

    /************************************************************************
     * Preview (Design mode): the selected cue on the rig
     ************************************************************************/
public:
    /** Why a wanted preview is (not) on the rig. */
    enum class PreviewState
    {
        Off,            //!< nothing wanted
        Playing,        //!< the wanted cue plays
        NotDesign,      //!< Live (Operate) mode: edits are blind until GO
        Dark            //!< the cue plays nothing
    };

    /** Play cue @a cueId on the rig as a preview, as the Programming tab
     *  does with its canvas: Design mode only. In Design what is in edit is
     *  what shows: also over a list running for real (the cue's own or
     *  another), which keeps going underneath and is what remains when the
     *  preview stops; the preview's scenes win on the channels they play
     *  (Scene::setEditOverride). In Live the running list owns the stage
     *  and nothing previews (NotDesign). The cue's members
     *  (its generated scenes and opaque functions) run under previewParent(),
     *  so a scene two previewed cues share keeps running when the preview
     *  moves on, and every bake re-takes the preview at once: an edit shows
     *  on the rig immediately. Another cue switches the preview. */
    void previewCue(quint32 cueId);
    /** Stop the preview (leaving the tab, nothing selected). */
    void stopPreview();
    /** The cue the preview was asked for (invalid: none). */
    quint32 previewedCue() const { return m_previewCue; }
    PreviewState previewState() const { return m_previewState; }
    /** What the preview runs now. */
    QList<quint32> previewFunctions() const { return m_previewRunning; }
    /** The names of the cue lists running for real (their chaser runs). */
    QStringList listsRunning() const;
    /** The parent preview functions run under: stopping it never stops
     *  anything a chaser, a VC widget or another tab started. */
    static FunctionParent previewParent();

signals:
    /** The preview started, stopped or moved to another cue. */
    void previewChanged();

private slots:
    void slotModeChanged();
    void slotTimerFunctionStarted(quint32 fid);
    void slotTimerFunctionStopped(quint32 fid);

private:
    /** Bring what runs in line with the wanted cue (after a bake, a mode
     *  change, the list starting or stopping). */
    void syncPreview();

    quint32 m_previewCue = 0xFFFFFFFF;
    PreviewState m_previewState = PreviewState::Off;
    QList<quint32> m_previewRunning;
    /** Sweeps, by a bake, the generated scenes the preview let go of. */
    QTimer *m_sweepTimer = nullptr;

    /************************************************************************
     * Import (§7)
     ************************************************************************/
public:
    struct ImportReport
    {
        quint32 production = 0xFFFFFFFF;
        quint32 list = 0xFFFFFFFF;
        int cues = 0;
        int layers = 0;             //!< scenes read into layers
        int opaqueLayers = 0;       //!< functions kept as they are
        int includes = 0;
        int generatedScenes = 0;    //!< distinct scenes the list bakes to
        QStringList opaqueReasons;  //!< "BackStage: has raw channel values"
        /** Cues where two scenes set the same target: the chaser ran both
         *  (intensity HTP); the tree keeps the later one's values. */
        QStringList htpConflicts;
        QStringList notes;
    };

    /** Import @a chaserId as a cue list in a new production (D12): each
     *  step a cue, each scene of targets + palettes a layer, a collection
     *  that is another step's an include, anything else an opaque function
     *  layer. Non-tracking, so it plays as the chaser did. The original is
     *  untouched. One undo step. */
    ImportReport importChaser(quint32 chaserId);

    /** Read @a scene as a layer; false (and @a why) when it can't be read
     *  without changing what it plays. */
    bool sceneToLayer(const Scene *scene, ShowTree::Layer *layer, QString *why) const;

    /** Whether two look stacks play the same: the same colours in the same
     *  order, the same other static looks, the same effects in the same
     *  order each with the same own looks. */
    bool playsTheSame(const QList<quint32> &a, const QList<quint32> &b) const;

    /************************************************************************
     * Load & save
     ************************************************************************/
public:
    bool loadXML(QXmlStreamReader &root);
    void saveXML(QXmlStreamWriter *doc) const;
    void reset();

private slots:
    void slotFunctionRemoved(quint32 fid);
    void slotPaletteRemoved(quint32 pid);

signals:
    /** The tree changed (an edit, undo, load). */
    void changed();

private:
    void loadState(QXmlStreamReader &root, bool snapshot);
    void saveState(QXmlStreamWriter *doc, bool snapshot) const;

    ShowTree::Cue *findCue(quint32 cueId, ShowTree::CueList **list = nullptr);

    /** After an edit (@a before, @a after): a use of a reusable that the
     *  edit changed gives the reusable its content, and every other use
     *  takes it (P4). Uses the edit didn't touch take the reusable's. */
    void syncReusables(const QList<ShowTree::Production> &before, QList<ShowTree::Production> &after);
    QList<ShowTree::ReusableLayer> m_reusableLayers;
    QList<ShowTree::ReusableCue> m_reusableCues;
    QList<ShowTree::ReusableLayer> m_batchReusableLayers;
    QList<ShowTree::ReusableCue> m_batchReusableCues;
    /** Functions a list Detach handed over (chaser, collections, scenes):
     *  an undo past it takes them back, a redo hands them over again. */
    QSet<quint32> m_detachedLists;
    QSet<quint32> m_snapshotDetachedLists;
    bool m_snapshotHasDetachedLists = false;
    /** Hand generated function @a id over to the show (no longer the tree's). */
    void handOver(quint32 id);
    /** After a snapshot is read: take back what a list Detach handed over
     *  that the snapshot's state didn't have, hand over what it did. */
    void applyDetachedLists();
    /** Whether a function the tree didn't generate plays generated @a id. */
    bool usedOutsideTree(quint32 id);

    /** Run @a change on the group sets as one undo step, then bake. */
    bool editSets(const QString &label, const std::function<bool(QList<ShowTree::GroupSet> &)> &change);
    QList<ShowTree::GroupSet> m_groupSets;
    QList<ShowTree::GroupSet> m_batchSets;

    /** Named palettes a command made: reusable once it succeeds. */
    QList<quint32> m_batchNamed;
    int m_batchDepth = 0;
    QString m_batchLabel;
    QByteArray m_batchBefore;
    QList<ShowTree::Production> m_batchProductions;
    bool m_batchChanged = false;
    QList<quint32> m_lastChanged;

    Doc *m_doc;
    QList<ShowTree::Production> m_productions;
    quint32 m_nextId = 1;
    bool m_baking = false;
};

/** @} */

#endif

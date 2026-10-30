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
#include <vector>
#include <QStringList>
#include <QObject>
#include <QHash>
#include <QMap>
#include <QSet>

#include "qlcengine_export.h"
#include "functionparent.h"
#include "showtreemodel.h"
#include "qlcpalette.h"
#include "scenevalue.h"

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
 * (docs/design/SHOW_TREE_DESIGN.md §4): a cue list is a Chaser, a cue a Collection
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
     * Productions (P5): which one is current -- what GO, the footer, VC
     * and show control play -- its main list and timecode source
     ************************************************************************/
public:
    /** The current production (invalid: none -- GO plays the show's main
     *  cue list, whatever it is). Saved with the show, not an undo step:
     *  what GO plays is the operator's choice, not an edit. */
    quint32 currentProduction() const { return m_current; }
    /** Make @a productionId current: its main list's chaser becomes the
     *  show's main cue list (ShowControl), so the footer GO, ⌘↩, show
     *  control and every Cue Runner following "the current production" play
     *  it, and its timecode source is followed. Invalid: none. A running
     *  list is not stopped. False: no such production. */
    bool setCurrentProduction(quint32 productionId);
    /** The list a production plays while current: its main list if that is
     *  still one of its cue lists, else its first (invalid: none). Never a
     *  Looks section or a sub-cue list. */
    quint32 mainList(quint32 productionId) const;
    /** Make @a listId its production's main list: one undo step. When the
     *  production is current, GO plays it from now on. */
    bool setMainList(quint32 listId);
    /** A production's timecode source while current: a universe, -1 any,
     *  -2 the show's own (Doc's TimecodeSource setting). One undo step. */
    bool setProductionTimecode(quint32 productionId, qint32 universe);
    /** The production whose cue list plays as chaser @a chaserId (invalid:
     *  none, or not a tree chaser). @a listId: that list. */
    quint32 productionOfChaser(quint32 chaserId, quint32 *listId = nullptr) const;
    /** The cue lists of a production that play (no Looks, no sub-cues), in
     *  order. */
    QList<quint32> playableLists(quint32 productionId) const;

signals:
    /** The current production (or its main list) changed. */
    void currentProductionChanged();

private slots:
    void slotMainCueListChanged(quint32 chaserId);

private:
    /** After a bake or an edit: the current production's main list's chaser
     *  is the show's main cue list. */
    void syncCurrentProduction();
    /** Doc's timecode source follows the current production's. */
    void applyProductionTimecode();
    quint32 m_current = 0xFFFFFFFF;
    bool m_settingMain = false;

    /************************************************************************
     * Sub-cues (P6, §3C, D9, D25): a cue's sub-cues 4.1, 4.2... are a list of
     * their own (CueList::subOf), so every cue operation works on them
     ************************************************************************/
public:
    /** Cue @a cueId's list of sub-cues (invalid: it has none). */
    quint32 subCueList(quint32 cueId) const;
    /** Whether @a cueId is a sub-cue; @a parentCue: its cue. */
    bool isSubCue(quint32 cueId, quint32 *parentCue = nullptr) const;
    /** A new sub-cue of cue @a cueId after sub-cue @a afterId (invalid: at
     *  the end), numbered <cue>.<n> (D23 under the cue). Its list is made
     *  in the same undo step when the cue has none. Refused (invalid, @a
     *  why) for a look, a sub-cue (one level) or a cue that isn't there. */
    quint32 addSubCue(quint32 cueId, const QString &name, quint32 afterId = ShowTree::invalidId(),
                      QString *why = nullptr);
    /** A sub-cue's advance: Auto-follow @a wait ms after the one before has
     *  faded in, or (false) on Sub GO. One undo step. */
    bool setSubCueFollow(quint32 subCueId, bool follow, uint wait);
    /** The cue's number as show control reads it: its own, else its
     *  position; a sub-cue's "4.1" (its cue's and its own). */
    QString cueNumber(const ShowTree::CueList &list, int index) const;
    /** The full numbers of the sub-cues of @a list's cues ("4.1", "4.2"):
     *  taken, so a new cue never gets one (D23). In @a ps. */
    static QStringList takenNumbers(const QList<ShowTree::Production> &ps, const ShowTree::CueList &list);
    /** How many of @a listId's cues have sub-cues. */
    int subCueListsOf(quint32 listId) const;

    /************************************************************************
     * The cue runner (P7, §6B): what it reads of the tree
     ************************************************************************/
public:
    /** The cue (or sub-cue) step @a step (and sub-cue @a subStep, -1:
     *  none) of a list's chaser plays (invalid: none). */
    quint32 cueOfStep(quint32 listId, int step, int subStep = -1) const;
    /** Cue @a cueId resolved as it plays (tracked, included, its own; a
     *  sub-cue from its cue). False: no such cue. */
    bool resolvedCue(quint32 cueId, ShowTree::ResolvedCue *out) const;
    /** What changes on stage going from cue @a fromCue to cue @a toCue
     *  (§6B "what changes"): per layer, only the attributes that differ --
     *  "Towers → Red, spread +5, gobo Breakup; BackStage in → Green,
     *  Intensity 45%; USTs out". @a fromCue invalid: from dark. Empty: no
     *  change (or no such cue). */
    QString changeLine(quint32 fromCue, quint32 toCue) const;

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

    /** A new production. It becomes the current one -- what GO plays --
     *  when nothing else has GO: no production is current and the show's
     *  main cue list isn't another chaser. So a first production plays
     *  without a trip to Make current, and adding one never takes GO from
     *  a production (or a plain chaser) the operator chose. */
    quint32 addProduction(const QString &name);
    /** A new cue list: **Tracking**, the point of the tab -- the same from
     *  + Cue list and from `list "name"`. Only an imported chaser starts
     *  Non-tracking (D12, importChaser()), so it plays as it did. */
    quint32 addCueList(quint32 productionId, const QString &name,
                       ShowTree::ListMode mode = ShowTree::ListMode::Tracking);
    /** A new cue after @a afterIndex (-1: at the top), numbered per D23. */
    quint32 addCue(quint32 listId, int afterIndex, const QString &name);
    bool removeCue(quint32 cueId);
    /** Cue @a cueId to @a toIndex: moveCues() onto the cue there (after it
     *  when moving down), so it is renumbered for its new place. */
    bool moveCue(quint32 cueId, int toIndex);
    /** Its own fields; layers and looks kept as given. A new number must be
     *  free (checkCueNumber()) and moves the cue to its place in number
     *  order: a list's order is always its numbers' order. */
    bool setCue(const ShowTree::Cue &cue, QString *why = nullptr);

    /**********************************************************************
     * Order and numbers (docs/features/show-tree.md "Reordering and
     * numbering"): a list's order is its cues' number order.
     **********************************************************************/
    /** Whether cue @a cueId may be numbered @a number: digits and dots, not
     *  another cue's in its list, and not a sub-cue's ("4.1" is sub-cue 1
     *  of cue 4) -- nor may its own sub-cues land on a cue's number. */
    bool checkCueNumber(quint32 cueId, const QString &number, QString *why = nullptr) const;
    /** The numbers cues @a cueIds (one list) get when dragged, as a block in
     *  their order, before (or @a after) cue @a anchorCueId: each in turn
     *  numbered between its new neighbours by numberBetween(). Empty when
     *  refused (@a why: no number fits -- Renumber this level -- or the
     *  drop changes nothing). */
    QStringList moveNumbers(const QList<quint32> &cueIds, quint32 anchorCueId, bool after,
                            QString *why = nullptr) const;
    /** Move and renumber them so (one undo step, "Move cue"). A Looks
     *  section's looks just move: they have no numbers to keep in order. */
    bool moveCues(const QList<quint32> &cueIds, quint32 anchorCueId, bool after, QString *why = nullptr);

    struct Renumbered
    {
        quint32 cue;
        QString from;           //!< as show control knows it ("4", "4.1")
        QString to;
    };
    /** Renumber this level: the cues at cue @a cueId's level (its number's
     *  part count and everything before the last part the same) become
     *  @a start, @a start + @a step... in list order, in the last place;
     *  a deeper (point) cue under one of them keeps its own places under
     *  its new number (1.0.2.5 -> 1.0.3.5 when 1.0.2 becomes 1.0.3), and
     *  sub-cues follow their cue. Lists every number that changes, sub-cues
     *  too (what show control fires); empty with @a why when refused. */
    QList<Renumbered> renumberLevelPlan(quint32 cueId, int start = 1, int step = 1,
                                        QString *why = nullptr) const;
    bool renumberLevel(quint32 cueId, int start = 1, int step = 1, QString *why = nullptr);
private:
    /** Run @a op (a move or renumber in cue @a cueId's list) keeping what
     *  show control has LOADed on the list's chaser on the same cue. */
    bool keepingStandby(quint32 cueId, const std::function<bool()> &op);

    /************************************************************************
     * Song sections and the numbering scheme (Q190)
     ************************************************************************/
public:
    /** How production @a productionId numbers its cues, and so its song
     *  sections (ShowTree::Numbering). */
    ShowTree::Numbering numbering(quint32 productionId) const;
    bool setNumbering(quint32 productionId, ShowTree::Numbering numbering);
    /** The numbering list @a listId's sections follow (None for a Looks
     *  section or sub-cues). */
    ShowTree::Numbering listNumbering(quint32 listId) const;
    /** The song sections of list @a listId, in order (ShowTree::sectionsOf). */
    QList<ShowTree::Section> sections(quint32 listId) const;
    /** The section cue @a cueId is in (false: none). */
    bool sectionOfCue(quint32 cueId, ShowTree::Section *out) const;
    /** Name @a section of list @a listId ("Everybody Scream"; empty: no
     *  name). One undo step. */
    bool setSectionName(quint32 listId, const ShowTree::Section &section, const QString &name);
    /** + Section: under a scheme, a new cue at the end numbered to the next
     *  section ("4.0.1"), the section named @a name; under Manual, a
     *  section begins at cue @a atCue (invalid: at a new cue at the end).
     *  The section's first cue (invalid: none made). One undo step. */
    quint32 addSection(quint32 listId, const QString &name, quint32 atCue = 0xFFFFFFFF);
    /** Manual: the section that begins at @a headCue is no longer one (its
     *  cues join the section before). */
    bool removeSection(quint32 listId, quint32 headCue);
    /** A new cue at the top of @a section, numbered to the scheme. */
    quint32 addCueInSection(quint32 listId, const ShowTree::Section &section, const QString &name);
    /** "Block at section start": the section's first cue is Block (it
     *  starts from nothing), or back to the list's default. */
    bool setSectionBlock(quint32 listId, const ShowTree::Section &section, bool block);
    bool sectionBlocks(quint32 listId, const ShowTree::Section &section) const;
    /** Tonight's running order (0.3.1 ro): production @a productionId's
     *  songs in the order the Run and GO play them, and the ones left out
     *  tonight -- no cue is renumbered or moved. The sections of its main
     *  list, resolved (ShowTree::resolveRunningOrder); @a sectionsOut (if
     *  given) gets each song's section, number order. Empty with no scheme. */
    QList<ShowTree::RunningSong> runningOrder(quint32 productionId, QList<ShowTree::Section> *sectionsOut = nullptr) const;
    /** Whether the order differs from number order (a song moved or left out). */
    bool hasRunningOrder(quint32 productionId) const;
    /** Play the songs in @a order (one undo step, "Running order"); one equal
     *  to number order with nothing left out is stored as none. The chaser's
     *  steps follow it. */
    bool setRunningOrder(quint32 productionId, const QList<ShowTree::RunningSong> &order);
    /** Back to number order, every song in. */
    bool resetRunningOrder(quint32 productionId);
    /** The cues of list @a listId in the order it plays: list indexes, one
     *  per chaser step (the identity without a running order). */
    QList<int> playOrderOf(quint32 listId, QList<bool> *skipped = nullptr) const;
    /** Merge lists into one… (ShowTree::mergeLists): one undo step. False
     *  (@a why): nothing merged. */
    bool mergeLists(quint32 productionId, const QList<quint32> &listIds, ShowTree::MergeReport *report = nullptr,
                    QString *why = nullptr);
public:
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
    /** Skip cues (Cue::skip), or put them back: GO, auto-follow and Back
     *  pass over a skipped cue, and it tracks as if it weren't in the list;
     *  a jump to it still plays it. Cues of a Looks section are left
     *  alone. One undo step for them all ("Skip cue" / "Unskip cue");
     *  false when none changed. */
    bool setCuesSkipped(const QList<quint32> &cueIds, bool skip);
    bool isCueSkipped(quint32 cueId) const;
    /** The text colour of cues (Cue::colour: "#rrggbb", "" none -- they
     *  take their list's again) or of a cue list (CueList::colour): how
     *  they read in the tree, the Run tab and the VC, nothing more. One
     *  undo step ("Cue colour" / "Cue list colour"); false when none
     *  changed. */
    bool setCuesColour(const QList<quint32> &cueIds, const QString &colour);
    bool setListColour(quint32 listId, const QString &colour);
    /** The colour cue @a cueId's text is shown in: its own, else its
     *  list's -- a sub-cue's, else its cue's -- ("" none: the theme's). */
    QString cueColour(quint32 cueId) const;
    QString listColour(quint32 listId) const;

    /** Automatic move-in-black per cue list and per cue (Q38). The show's
     *  setting (ProgrammerController::isAutoMoveInBlack) is the default; a
     *  list may say On or Off whatever the show says (CueList::moveInBlack,
     *  a sub-cue list: its cue's list's), and a cue may say "Don't move in
     *  black into this cue" (Cue::noMoveInBlack), which wins. Lead time
     *  stays the show's. Each set is one undo step ("Move in black: On" /
     *  "Don't move in black into cue"); false when nothing changed (a
     *  Looks section has neither). */
    bool setListMoveInBlack(quint32 listId, ShowTree::MoveInBlack mode);
    ShowTree::MoveInBlack listMoveInBlack(quint32 listId) const;
    bool setCuesNoMoveInBlack(const QList<quint32> &cueIds, bool no);
    bool isCueNoMoveInBlack(quint32 cueId) const;
    /** A focus-call cue (docs/design/FOCUS_REFINE_DESIGN.md section 7): GO arms the
     *  show's focus calls, the controller's Start begins them. One undo
     *  step; false when nothing changed (a Looks section has none). */
    bool setCuesFocusCall(const QList<quint32> &cueIds, bool on);
    bool isCueFocusCall(quint32 cueId) const;
    /** The show's own setting, read from Doc's programmer (false: none). */
    bool showMoveInBlack() const;
    /** Whether heads move in black into cue @a cueId: the cue's flag, else
     *  its list's setting, else @a showDefault (ShowTree::effectiveMoveInBlack). */
    bool moveInBlackInto(quint32 cueId, bool showDefault) const;
    bool moveInBlackInto(quint32 cueId) const { return moveInBlackInto(cueId, showMoveInBlack()); }
    /** The list's setting as it plays: its own, else @a showDefault. */
    bool listMovesInBlack(quint32 listId, bool showDefault) const;
    /** Any list or cue that says other than the show (for the MIB button's
     *  tooltip); @a lists / @a cues: how many of each. */
    bool hasMoveInBlackOverrides(int *lists = nullptr, int *cues = nullptr) const;
    /** Any cue list set to On: the planner has to look ahead even while
     *  the show's setting is off. */
    bool anyListMoveInBlackOn() const;
    /** For the planner (MarkPlanner): whether to move in black into what
     *  fires next -- step @a step of chaser @a chaserId (a running list),
     *  else function @a functionId (a Show's next item: a cue's collection
     *  or a list's chaser). Not a tree cue or list: @a showDefault. */
    bool moveInBlackForNext(quint32 chaserId, int step, quint32 functionId, bool showDefault) const;

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
     * The Looks section (docs/design/PROGRAMMING_ROLE_DESIGN.md §4.1, P-D5..P-D7, R3):
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
    /** Make VC page (open 22): the VC frame it made for Looks section
     *  @a listId's @a folder ("": all), so a second Make VC page updates it
     *  instead of adding another (invalid: none yet). Saved with the show,
     *  not an undo step (the Virtual Console has its own). */
    quint32 lookVcFrame(quint32 listId, const QString &folder) const;
    /** The buttons it made there (VC widget ids): only these are ever
     *  removed, when their look is gone. */
    QList<quint32> lookVcButtons(quint32 listId, const QString &folder) const;
    void setLookVcFrame(quint32 listId, const QString &folder, quint32 frameId,
                        const QList<quint32> &buttons = QList<quint32>());
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
        int mixCleared = 0;                 //!< Mix cues now on the single rule (output the same)
        int cueOnly = 0;                    //!< cues made Cue-only (removes repetition)
        /** Mix cues left mixing: the single rule would change their output
         *  ("0.0.4 SHOUT: StepTops and Steps both set Colour on 2 fixtures"). */
        QStringList mixKept;
    };
    /** What cue @a index would put out if it ran alone, statically: per
     *  fixture and channel (fixture << 32 | channel), its scenes' palettes
     *  as Scene::write lays them (left-out fixtures, the look's dimmer on a
     *  fixture with no dimmer), merged as a Collection plays them (HTP on
     *  intensity, else the later scene), opaque functions by CueOutput.
     *  Effects are dynamic and not in it: @a effects, when given, gets
     *  each effect palette's played fixtures instead, to compare. */
    QHash<quint64, uchar> staticCueOutput(const ShowTree::CueList &list,
                                          const QList<ShowTree::ResolvedCue> &resolved, int index,
                                          QStringList *effects = nullptr) const;
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
    /** Something outside the tree that the last undo step also changed --
     *  the VC buttons an import re-pointed (W0): @a apply(true) once the
     *  step is undone, @a apply(false) once it is redone, so it stays one
     *  step. Several attach in order (undone last first). @a beforeTree:
     *  it was changed before the step's tree change (Restore original
     *  points VC back at the chaser before the list's goes), so undo runs
     *  it after the tree is back, redo before. False when there is no step
     *  to attach it to. */
    bool attachToLastStep(const std::function<void(bool undone)> &apply, bool beforeTree = false);
    void clearRedo() { m_redo.clear(); }
    /** The steps taken since undoStamp() was @a stamp become one step named
     *  @a label (Upgrade this show: several imports and captures, one
     *  undo). Undo takes them back last first, each with what it changed
     *  outside the tree; redo does them again in order. False when no
     *  step was taken since. */
    bool groupStepsSince(quint64 stamp, const QString &label);
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

    /** Deleting palettes (open 26, PaletteOps::remove): take @a paletteIds
     *  off every look of the tree -- cue looks, layers, the Looks
     *  sections, reusable layers and cues; a look left without a palette
     *  goes (its key is unset, not None) -- then run @a deleteThem and
     *  bake once. No undo step of its own: the palette step carries
     *  outsideUndoState() from either side. False: the tree had none
     *  (@a deleteThem has still run). */
    bool removePaletteUses(const QList<quint32> &paletteIds, const std::function<void()> &deleteThem);
    /** The tree as another stack's undo step keeps it: an undo snapshot
     *  plus the generated scenes and Collections as they are, so undoing
     *  that step brings back the very functions (and ids) it had. */
    QByteArray outsideUndoState() const;
    /** Put back outsideUndoState(): generated functions gone since are
     *  made again with their ids, then the tree is restored and baked. */
    void restoreOutsideUndoState(const QByteArray &state);

private:
    struct Snapshot
    {
        QString label;
        QByteArray xml;
        quint64 stamp = 0;
        /** What the step changed outside the tree (attachToLastStep):
         *  after its tree change, and before it. */
        std::function<void(bool undone)> outside;
        std::function<void(bool undone)> outsideBefore;
        /** A step made of several (groupStepsSince()), in the order they
         *  were done; its own xml is unused. */
        std::vector<Snapshot> parts;
    };
    QByteArray snapshotXml() const;
    void restoreSnapshot(const QByteArray &xml);
    /** Undo / redo one step (a group: its parts, last first / in order);
     *  the step that takes it back. */
    Snapshot undoStep(const Snapshot &s);
    Snapshot redoStep(const Snapshot &s);

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
    /** "listId\nfolder" -> the VC frame Make VC page made (open 22). */
    QHash<QString, quint32> m_lookVcFrames;
    QHash<QString, QList<quint32>> m_lookVcButtons;
    QSet<quint32> m_inlinePalettes;
    /** One-offs made reusable here (and palettes a command made): an undo
     *  snapshot carries them as they are, so redo can name them again. */
    QSet<quint32> m_madeReusable;
    /** Scenes Detach made: an undo snapshot carries them as they are, and
     *  restoring one made before a detach deletes that detach's scene. */
    QSet<quint32> m_detached;
    /** The detached scenes the snapshot being restored holds. */
    QSet<quint32> m_snapshotDetached;
    /** A function an undo step's outside change had to make again under
     *  another id (its own was taken meanwhile by a bake): every snapshot
     *  restored from then on plays it there on its ▣ layers, where the old
     *  id is gone or a generated function's. */
    void remapSnapshotFunction(quint32 from, quint32 to);
    QHash<quint32, quint32> m_snapshotRemap;
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
        Dark,           //!< the cue plays nothing
        Held            //!< a run took the stage (GO): off until Play
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

    /** RC8, "GO holds previews": a GO, Back or Sub GO from anywhere, or a
     *  cue list starting for real, takes the stage. The preview stops and
     *  is held -- selecting a cue keeps it off stage -- until Play
     *  (resumePreview()) or, with no list running, the next cue selected.
     *  stageTakenForRun() asks the Workbench, Solo and the editors' Test
     *  plays to stop theirs too. Returns what was stopped, for the notice
     *  ("the Show Tree preview"...); empty when nothing was playing. Simple
     *  Desk, the VC, Focus/Refine and the programmer are not previews and
     *  keep running. UI thread. */
    QStringList takeStageForRun();
    /** Play: the held preview comes back, over the run (the edit wins on
     *  the channels it plays), until the next GO. */
    void resumePreview();
    bool previewHeld() const { return m_previewHeld; }

    /** Live / blind (docs/design/LIVE_BLIND.md): the user picked cue
     *  @a cueId -- a click in the tree, the Workbench following it, a cue
     *  clicked in the Run in Design. The latest action wins: a hold by a
     *  run lets go and, in Design, the cue plays (previewCue()). A refresh
     *  of the same selection (a rebuild, the tab coming back) is not a
     *  pick and calls previewCue(), which keeps the hold. */
    void pickCue(quint32 cueId);
    /** The cue last picked or previewed for editing -- what a Blind rig
     *  view shows. Kept when the preview stops (leaving the tab); invalid
     *  when none, or it was deleted. */
    quint32 editedCue() const;
    /** What cue @a cueId puts on stage, as levels: fixture id -> one byte
     *  per channel (0 where it sets nothing). Its scenes' own values and
     *  static looks, intensities HTP between scenes and the rest the later
     *  scene's; effects and other functions are not drawn (their base
     *  only). For a Blind rig view; UI thread. */
    QHash<quint32, QByteArray> cueLevels(quint32 cueId) const;

signals:
    /** The preview started, stopped or moved to another cue. */
    void previewChanged();
    /** editedCue() changed. */
    void editedCueChanged();
    /** A run took the stage (takeStageForRun()): whatever previews stops
     *  and holds; a listener that stopped something appends what to
     *  @a held (non-null; direct connections only). */
    void stageTakenForRun(QStringList *held);

private slots:
    void slotModeChanged();
    void slotTimerFunctionStarted(quint32 fid);
    void slotTimerFunctionStopped(quint32 fid);

private:
    /** Bring what runs in line with the wanted cue (after a bake, a mode
     *  change, the list starting or stopping). */
    void syncPreview();

    quint32 m_previewCue = 0xFFFFFFFF;
    /** editedCue(). */
    quint32 m_editedCue = 0xFFFFFFFF;
    void setEditedCue(quint32 cueId);
    /** A run took the stage: the preview stays off until Play. */
    bool m_previewHeld = false;
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
        /** Cues made to mix (Cue::mixOverlap) because an include at full
         *  beside the cue's own scenes would, under the tree's rule, play
         *  differently from the chaser: one line each, with how many
         *  channels the rule would change. Their two-scene conflicts are
         *  not in @a htpConflicts (they play as the chaser did). */
        QStringList mixedIncludes;
        QStringList notes;
        /** The same-output check (W0), as Convert's: what each cue would
         *  put out statically against the chaser's step. Cues checked,
         *  channels that differ in all, and one line per cue that does. */
        int checkedCues = 0;
        int differChannels = 0;
        QStringList differCues;
        /** W5: collections inside a step that are no step themselves, read
         *  into the cue's layers (@a flattened) or left out because they
         *  play nothing (@a dropped); one line each in @a flattenNotes. */
        int flattened = 0;
        int dropped = 0;
        QStringList flattenNotes;
        /** W5, Import in place: the originals filed under Imported/…, and
         *  the Show timelines that played the chaser and play the list now. */
        int filed = 0;
        QStringList timelines;
        /** The same function (a scene, a nested collection's member, an
         *  effect) or the cue before's collection in consecutive steps: set
         *  in the first cue, tracked through the rest, released where it
         *  stops (trackRepeats). Cues that track, layers and includes no
         *  longer repeated, releases made; one line per cue that tracks. */
        int trackingCues = 0;
        int trackedLayers = 0;
        int trackedIncludes = 0;
        int trackReleases = 0;
        QStringList trackNotes;
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

    /** The import's same-output check (W0): each cue of @a listId against
     *  step i of @a chaserId, statically -- every channel the step's scenes
     *  set (a Collection's members as it runs them: HTP on intensity, else
     *  the later one), against staticCueOutput(). Effects (EFX, RGB Matrix,
     *  effect palettes) are left out on both sides; fade times too. The
     *  number of channels that differ; @a lines gets a line per cue that
     *  differs, @a checked the cues compared. */
    int importDifferences(quint32 chaserId, quint32 listId, QStringList *lines = nullptr,
                          int *checked = nullptr) const;
    /** The same for a list that isn't in the tree (yet): a planned import. */
    int importDifferences(quint32 chaserId, const ShowTree::CueList &list, QStringList *lines = nullptr,
                          int *checked = nullptr) const;

private:
    /** What importChaser() makes of @a chaserId, without changing anything:
     *  the list (its node ids from @a nextId) and the report, the
     *  same-output check not yet run. False: not a chaser. */
    bool buildImport(quint32 chaserId, ShowTree::CueList *list, ImportReport *report,
                     const std::function<quint32()> &nextId) const;
    /** The import's tracking reading: a layer made from the same function
     *  (@a origin: layer id -> function) as one the cue before puts on
     *  stage, or an include of what the cue before already plays, is set
     *  only where it starts and tracks on; what the cue before plays that
     *  this cue doesn't is released there. Cue by cue, each kept only
     *  where every cue still plays exactly what it did (else the cue
     *  stays Block and repeats it). The list is made Tracking when any
     *  cue tracks; the first cue stays Block. */
    void trackRepeats(ShowTree::CueList &list, const QHash<quint32, quint32> &origin, ImportReport &report,
                      const std::function<quint32()> &nextId) const;

    /************************************************************************
     * The bridge (W5): an old show into the tree -- and back
     ************************************************************************/
public:
    /** What a chaser is used as (docs/design/WORKFLOW_DESIGN.md §3.2, §3.5). */
    enum class ChaserKind
    {
        CueList,    //!< manual: waits for GO, notes, cue numbers, show control, a VC Cue List
        Effect      //!< loops on its own: strobes, chases, sweeps -- stays a function
    };
    struct ChaserClass
    {
        ChaserKind kind = ChaserKind::Effect;
        QStringList why;            //!< "waits for GO on 7 of 7 steps", "loops every 0.5 s"
    };
    /** Tell a cue-list chaser from an effect chaser, by how it runs: any
     *  step waiting for GO, a note or cue number, a cue-list number, the
     *  show's main cue list or a VC Cue List playing it (@a vcCueList: the
     *  UI knows) make a cue list; a chaser that only loops on its own,
     *  timed, is an effect. */
    ChaserClass classifyChaser(quint32 chaserId, bool vcCueList = false) const;
    /** The chasers an old show could convert: its own (not the tree's, not
     *  already imported in place), by name. */
    QList<quint32> convertibleChasers() const;
    /** Why chaser @a chaserId can't be sent to the Show Tree (Build's Send
     *  to Show Tree…): not a chaser, the tree's own, or imported in place
     *  already. Empty: it can. */
    QString whyNotSend(quint32 chaserId) const;
    /** What Import would make of @a chaserId, without changing anything:
     *  the report with its same-output check (and the list, @a list). */
    ImportReport planImport(quint32 chaserId, ShowTree::CueList *list = nullptr) const;

    /** Import in place (W5): importChaser(), then fileOriginals() in the
     *  same undo step. What fires the chaser is the UI's to move
     *  (ImportSheet::planRepoint), attached to that step too. */
    ImportReport importInPlace(quint32 chaserId);
    /** File what list @a listId was imported from: its source chaser, and
     *  the collections and scenes only it uses that the list doesn't play
     *  as they are, under "Imported/<chaser> (originals)" -- each one's
     *  folder kept in the list (Restore original puts them back), the list
     *  marked in place. A Show timeline that played the chaser plays the
     *  list from now on. Part of the last undo step (call it right after
     *  the import). The number filed (0: nothing, or not an import). */
    int fileOriginals(quint32 listId);
    /** What fileOriginals() would file, without changing anything; the
     *  Show timelines it would point at the list in @a timelines. */
    QList<quint32> originalsOf(quint32 listId, QList<quint32> *timelines = nullptr) const;
    /** The timelines the last fileOriginals() pointed at the list. */
    QList<quint32> lastTimelines() const { return m_lastTimelines; }
    /** "Imported/Full Show (originals)" */
    static QString originalsFolder(const QString &chaserName);
    /** The in-place list that filed @a functionId (invalid: none). */
    quint32 importedInto(quint32 functionId) const;
    /** An original Import in place keeps for Restore original that the
     *  list doesn't play (Q141): its source chaser, and what only that
     *  chaser plays and no cue plays as it is -- what Capture into palettes
     *  left behind too, filed or not. Its in-place list (invalid: not an
     *  original); @a cueId: the cue that stands for it (by the step's cue
     *  number, else its position; invalid: none, or the chaser itself).
     *  View only: editing it would not change what plays. */
    quint32 originalFor(quint32 functionId, quint32 *cueId = nullptr) const;
    /** "Original from the import, kept for Restore original." */
    static QString originalText();
    /** Where an original's show is edited: "The show plays the Show
     *  Tree's cue list X; edit cue 6.1 Backout there." Empty: not one. */
    QString originalWhere(quint32 functionId) const;
    /** Why @a functionId is edited in the tree, not where it is: lockedText()
     *  for a generated function, originalText() and originalWhere() for an
     *  original. Empty: neither (it is the user's to edit). */
    QString lockedTextFor(quint32 functionId) const;
    /** Generated, or an original kept for Restore original. */
    bool isLocked(quint32 functionId) const;
    /** Whether Restore original can run on @a listId (@a why: not). */
    bool canRestoreOriginal(quint32 listId, QString *why = nullptr) const;
    /** Restore original: the list (its production too, when it has no
     *  other list) leaves the tree and the originals go back to their
     *  folders. Edits made to the list since go with it. One undo step;
     *  what fires the list is the UI's to move back (attachToLastStep). */
    bool restoreOriginal(quint32 listId, QString *why = nullptr);

    /** Capture into palettes (W5): a scene of raw channel values read as
     *  layers of groups / fixtures + palettes, checked channel for
     *  channel. The scene itself is never changed. */
    struct CapturePalette
    {
        QLCPalette::PaletteType type = QLCPalette::Undefined;
        QVariantList values;
        quint32 reuse = 0xFFFFFFFF;     //!< a named palette that plays it already
        QString name;                   //!< the reused one's, or the new one's
    };
    struct CaptureLayer
    {
        ShowTree::Target target;
        QMap<QString, int> looks;       //!< key -> index in CapturePlan::palettes
    };
    struct CapturePlan
    {
        bool ok = false;
        QString why;                    //!< not ok: why not
        quint32 scene = 0xFFFFFFFF;
        QList<CapturePalette> palettes;
        QList<CaptureLayer> layers;
        int capturedChannels = 0;
        /** What no palette plays exactly: kept raw, in a scene of their own
         *  ("Wash-Red (rest)") played beside the new layers. */
        QList<SceneValue> leftovers;
        QStringList leftoverLines;      //!< "LM70 #1: Effects 255"
        /** Where the tree plays the scene as it is: (list, cue). */
        QList<QPair<quint32, quint32>> uses;
        // After captureIntoPalettes():
        int captured = 0;               //!< uses now playing the layers
        QStringList kept;               //!< uses left raw, and why
        int differChannels = 0;         //!< in the cues captured: 0 by construction
        QList<quint32> made;            //!< new palettes
        quint32 rest = 0xFFFFFFFF;      //!< the leftovers' scene
    };
    /** Plan Capture into palettes for raw scene @a sceneId: per fixture,
     *  the Dimmer, Colour, Position and Gobo looks that put out exactly its
     *  values (an intensity channel a look sets to 0 that the scene leaves
     *  alone adds nothing); palettes reused by value, else named as the
     *  show names its own; fixtures with the same looks one layer (a group
     *  when one has exactly them). Nothing is changed. */
    CapturePlan planCapture(quint32 sceneId) const;
    /** Apply it where the tree plays the scene as it is: each such layer
     *  becomes the plan's layers (+ the rest as a raw scene), kept only
     *  where every cue of its list then puts out what it did (else that
     *  use stays raw, said in @a plan->kept). One undo step. False:
     *  nothing captured. */
    bool captureIntoPalettes(quint32 sceneId, CapturePlan *plan = nullptr);

private:
    QList<quint32> m_lastTimelines;
    /** What of @a list's source chaser only the chaser plays and the tree
     *  doesn't play as it is (originalsOf(), originalFor()); the Show
     *  timelines that play the chaser in @a timelines. */
    QSet<quint32> unplayedOriginals(const ShowTree::CueList &list, QSet<quint32> *timelines) const;
    /** m_originals brought up to date. */
    void refreshOriginals() const;
    /** originalFor(): function -> (its in-place list, its cue). */
    mutable QHash<quint32, QPair<quint32, quint32>> m_originals;
    mutable QList<quint32> m_originalsKey;
    mutable bool m_originalsStale = true;

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
    /** Detach (open 19): a scene of @a collectionId another list still
     *  plays is copied, and the copy put in its place, so the detached
     *  chaser is its own from the start. The copies' ids into @a made. */
    void copySharedScenes(quint32 collectionId, QList<quint32> *made);
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

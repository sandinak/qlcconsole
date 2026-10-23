/*
  Q Light Controller Plus
  showtreeview.h

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

#ifndef SHOWTREEVIEW_H
#define SHOWTREEVIEW_H

#include <functional>
#include <QPointer>
#include <QColor>
#include <QWidget>
#include <QPair>
#include <QList>
#include <QSet>

#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "qlcpalette.h"

class QStackedWidget;
class QKeyEvent;
class QListWidget;
class ShowTreeCommand;
class QDoubleSpinBox;
class QTreeWidgetItem;
class LookEditor;
class QTreeWidget;
class QPushButton;
class QToolButton;
class QGridLayout;
class NoteEdit;
class QComboBox;
class QLineEdit;
class QMimeData;
class QTimer;
class QCheckBox;
class QFormLayout;
class QTextBrowser;
class QLabel;
class QFrame;
class QMenu;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The Show Tree tab (SHOW_TREE_DESIGN.md §6): the tree of productions, cue
 * lists, cues and layers on the left; the selected cue or layer in the
 * middle (a layer as one row per attribute, inherited values greyed with
 * where they come from); the Library on the right (Recent, Pinned, All),
 * everything in it draggable onto a cue or a layer.
 *
 * Every edit goes through the engine's ShowTreeController (one undo step
 * each, then a bake); this view only shows the tree and turns clicks and
 * drops into those calls.
 */
class ShowTreeView final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(ShowTreeView)
    friend class TreeCueStack;

public:
    ShowTreeView(QWidget *parent, Doc *doc);
    ~ShowTreeView();

    enum NodeKind
    {
        NoNode = 0,
        ProductionNode,
        ListNode,
        CueNode,
        LayerNode,          //!< a cue's own layer
        TrackedLayerNode,   //!< carried into the cue (tracked, included, base)
        LookFolderNode      //!< a folder of a Looks section (R3): IdRole its list, FolderRole its path
    };

    /** Item roles in the tree and the Library. */
    enum
    {
        KindRole = Qt::UserRole,
        IdRole = Qt::UserRole + 1,
        CueRole = Qt::UserRole + 2,     //!< a layer row's cue
        TargetRole = Qt::UserRole + 3,  //!< a tracked layer's target signature
        PathRole = Qt::UserRole + 4,    //!< a Library folder's path ("Groups/Movers/Front")
        FolderRole = Qt::UserRole + 5   //!< a look folder's / palette folder's own path ("Hits")
    };

    /** Library item kinds. */
    enum LibraryKind { LibSection = 100, LibFolder, LibGroup, LibFixture, LibPalette, LibFunction, LibSet, LibTarget,
                       LibReusable, LibPaletteFolder };

    QTreeWidget *tree() const { return m_tree; }
    QTreeWidget *library() const { return m_library; }

    /** Select the tree row of node @a id (a layer: in cue @a cueId). */
    bool selectNode(quint32 id, quint32 cueId = ShowTree::invalidId());
    NodeKind currentKind() const;
    quint32 currentId() const;
    quint32 currentCueId() const;

    /************************************************************************
     * What the mouse does (drops, pickers) -- public for the tests
     ************************************************************************/
public:
    /** Groups / fixtures / group sets dropped on a cue: one layer on all
     *  of them, or (@a separate, ⌥ held) one layer per group -- a set's
     *  members each a layer of their own -- and one on the fixtures. */
    bool dropTargets(quint32 cueId, const QList<quint32> &groups, const QList<quint32> &fixtures,
                     bool separate = false, const QList<quint32> &sets = QList<quint32>());
    /** Groups / fixtures / sets dropped on a layer: added to its target. */
    bool dropTargetsOnLayer(quint32 cueId, quint32 layerId, const QList<quint32> &groups,
                            const QList<quint32> &fixtures, const QList<quint32> &sets = QList<quint32>());
    /** Groups / fixtures / sets dropped on a layer with ⇧ held: a child
     *  layer on them (§3.5) -- or, @a separate (⌥⇧), one child each. Each
     *  must be part of the layer's fixtures; refused (@a why) if not. */
    bool dropChildLayers(quint32 cueId, quint32 parentId, const QList<quint32> &groups,
                         const QList<quint32> &fixtures, const QList<quint32> &sets = QList<quint32>(),
                         bool separate = false, QString *why = nullptr);
    /** Make set (§1): the groups and fixtures selected in the Library as a
     *  group set named @a name. Invalid: nothing selected. */
    quint32 makeSetFromLibrary(const QString &name);
    /** Select All Similar: every group in the Library like one selected
     *  there (FixtureManager::similarGroups: same folder, same fixtures,
     *  as many heads) -- selected, ready for Make set. Returns how many
     *  groups are selected. */
    int selectSimilarLibraryGroups();
    /** Move to folder: file the groups selected in the Library in group
     *  folder @a path ("" = none) -- reloaded with them still selected.
     *  Returns how many moved. */
    int moveLibraryGroupsToFolder(const QString &path);
    /** MIME type of group sets dragged from the Library (a stream of ids). */
    static const char *setMimeType();
    /** MIME type of stage targets (points, areas, lines) dragged from the
     *  Library: dropped, a Position "Aim at" that target (§3B). */
    static const char *targetMimeType();
    /** MIME type of reusable layers and cues dragged from the Library (a
     *  stream of ids): dropped on a cue, a use of each -- a layer in it, a
     *  cue after it (P4). */
    static const char *reusableMimeType();
    /** Reusables dropped on cue @a cueId: each reusable layer a use in it,
     *  each reusable cue a new cue (a use) after it. One undo step. */
    bool dropReusables(quint32 cueId, const QList<quint32> &reusableIds);

    /************************************************************************
     * Reusable layers and cues; Detach a list; Convert to tracking (P4)
     ************************************************************************/
public:
    /** Who uses reusable @a id, as text: each cue ("Full Show 0.0.3
     *  Announcer"), and the VC widgets whose function is one of its uses'
     *  cue or layer ("VC button “Walk-in”"). */
    QStringList reusableUsedBy(quint32 id) const;
    /** The Library's Reusable items, as text ("⟲ BackStage — layer, used
     *  in 2"). */
    QStringList reusableItems() const;
    /** Delete a reusable: when it is used, asks first (its uses become
     *  plain copies). */
    bool deleteReusable(quint32 id);
    /** Convert to tracking… for list @a listId: shows what would change
     *  (the plan) and asks; one undo step. False: cancelled or nothing to
     *  do (the reason in the hint line). */
    bool convertListToTracking(quint32 listId);
    /** Detach to plain chaser: asks, then the list leaves the tree and its
     *  chaser is the show's own. */
    bool detachListToChaser(quint32 listId);
    /** In place of the confirmation dialogs (tests): @a title, @a text,
     *  @a details; return false to cancel. */
    typedef std::function<bool(const QString &title, const QString &text, const QString &details)> Confirm;
    void setConfirm(const Confirm &confirm) { m_confirm = confirm; }
    /** The reuse line on the cue / layer page, as text (tests). */
    QString reuseLineText() const;

    /** W0: the function a layer row plays as it is (an opaque layer: a raw
     *  scene, EFX, RGB Matrix, Collection or chaser) -- a layer of a cue,
     *  or one carried into it. Function::invalidId() for anything else. */
    quint32 openableFunction(QTreeWidgetItem *item) const;
    /** W0: open function @a functionId in its own editor, in the
     *  Workbench: the one docked here (↩ comes back), else Build's (the
     *  Functions window is hidden). Open on an opaque layer, its
     *  double-click and its page. What is changed there is what the cue
     *  plays. */
    bool openFunction(quint32 functionId);
    /** In place of opening it in Build when nothing is docked (tests). */
    typedef std::function<bool(quint32 functionId)> FunctionOpener;
    void setFunctionOpener(const FunctionOpener &opener) { m_functionOpener = opener; }
    /** Where else @a functionId plays, besides cue @a cueId: "cue 0.0.3
     *  Announcer", "collection 0.0.4-SHOUT" (W0, the "also in" line). */
    QStringList alsoIn(quint32 functionId, quint32 cueId) const;
    /** In place of exec() for the import sheet (tests): look at it, tick or
     *  untick, press a button. */
    typedef std::function<void(class ImportSheet *sheet)> SheetRunner;
    void setSheetRunner(const SheetRunner &runner) { m_sheetRunner = runner; }
    /** W0: what fires chaser @a chaserId now (VC buttons, VC Cue Lists and
     *  sliders, the main cue list, its cue-list number) fires list
     *  @a listId's chaser instead, in the last undo step (the import's).
     *  How many were moved. */
    int repointToList(quint32 chaserId, quint32 listId);

    /** W5: the show converter (Import chaser… ▸ Convert an old show…, and
     *  the open banner's Convert…). */
    void openConverter();
    /** In place of exec() for the converter (tests). */
    typedef std::function<void(class ShowConverter *converter)> ConverterRunner;
    void setConverterRunner(const ConverterRunner &runner) { m_converterRunner = runner; }
    /** W5: Import in place -- the list takes over from chaser @a chaserId:
     *  the originals filed under Imported/…, what fires the chaser (VC,
     *  timelines, show control) fires the list; one undo step. What
     *  happened, a line each. */
    QStringList importInPlace(quint32 chaserId);
    /** Upgrade this show… (File menu, the Show Tree's header and banner,
     *  the Run tab's empty state, Build's Send to Show Tree…: the one door,
     *  Q76): the converter over every chaser and collection, with Capture
     *  into palettes for raw scenes and Restore original; applied as one
     *  undo step. With @a onlyChaser, only that chaser is ticked and nothing
     *  is captured. The lists it made. */
    QList<quint32> openUpgrade(quint32 onlyChaser = 0xFFFFFFFF);
    /** What Upgrade does: convert @a chasers (in place or beside), then
     *  capture @a scenes into palettes, all one undo step ("Upgrade this
     *  show"). What happened, a line each. */
    QStringList upgradeShow(const QList<quint32> &chasers, bool inPlace, const QList<quint32> &scenes);
    /** Import beside the chaser (the converter's other way): a line. */
    QStringList importBeside(quint32 chaserId);
    /** Send to Show Tree… (Build, on a chaser): Upgrade This Show… with
     *  just @a chaserId ticked (Q76); made, the Show Tree comes forward
     *  with the new list selected. False: not sent
     *  (ShowTreeController::whyNotSend), or nothing was upgraded. */
    bool sendToShowTree(quint32 chaserId, QWidget *parent = nullptr);
    /** W5: Restore original for list @a listId: asks, then the list leaves
     *  the tree, the originals go back to their folders and what fires the
     *  list fires the chaser again; one undo step. */
    bool restoreOriginal(quint32 listId);
    /** W5: Capture into palettes for raw scene @a sceneId (a ▣ row): shows
     *  the plan, asks; one undo step. */
    bool captureIntoPalettes(quint32 sceneId);
    /** W5: the banner an old show opens with ("This show uses chasers…
     *  Convert…"): shown while the show has chasers that look like cue
     *  lists and no Show Tree, until dismissed (for this show). */
    QWidget *bridgeBanner() const;
    void updateBridgeBanner();

    /** Functions dropped on a cue: opaque function layers. */
    bool dropFunctions(quint32 cueId, const QList<quint32> &functions);
    /** Palettes dropped on a layer (or, with no layer, on the cue's looks;
     *  with no layer but a @a target, on a layer only carried into the cue,
     *  which then becomes the cue's own). */
    bool dropPalettes(quint32 cueId, quint32 layerId, const QList<quint32> &palettes,
                      const ShowTree::Target &target = ShowTree::Target());
    /** Groups / fixtures dropped on a layer only carried into the cue
     *  (tracked, included, base): it becomes the cue's own with them added,
     *  keeping what it carried -- one undo step. */
    bool dropTargetsOnTracked(quint32 cueId, const ShowTree::Target &target, const QList<quint32> &groups,
                              const QList<quint32> &fixtures, const QList<quint32> &sets = QList<quint32>());
    /** What a drop of @a mime on (@a cueId, @a layerId) would do, before it
     *  is let go of: "Blue → cue look: reaches 2 of 3 layers". With no layer
     *  but a @a target: onto a layer carried into the cue. */
    QString dropHint(quint32 cueId, quint32 layerId, const QMimeData *mime,
                     bool separate = false, const ShowTree::Target &target = ShowTree::Target(),
                     bool child = false) const;
    /** A drop below the tree's last row, on the selected cue: "New layer
     *  in 1.0.3 Out: Towers" (else what dropHint() says for the cue). */
    QString dropHintBelow(quint32 cueId, const QMimeData *mime, bool separate = false) const;
    /** Return or double-click in the Library: what is selected there acts on
     *  the tree's selection, as a drop on it would -- on a cue: a new layer
     *  (several: one; @a separate, ⌥: one each), a palette a cue look; on a
     *  layer: added to it, a palette its look; functions a function layer.
     *  One undo step. False: nothing to do. */
    bool applyLibrarySelection(bool separate = false);
    /** What Return in the Library would do now (the hint line says it). */
    QString libraryReturnHint(bool separate = false) const;
    /** ⌘F, or / in the tree: to the Library's filter. */
    void focusLibraryFilter();
    bool acceptsDrop(quint32 cueId, quint32 layerId, const QMimeData *mime,
                     const ShowTree::Target &target = ShowTree::Target(), bool child = false) const;
    /** Any drop on (@a cueId, @a layerId or carried-in @a target), as the
     *  tree, the cue page and the layer page take it: palettes set looks,
     *  groups / fixtures make a layer (⌥ @a separate: one each) or join
     *  one, functions play as they are. @a stayOnCue: the cue stays
     *  selected (a drop on the cue page), else the new layer is. */
    bool dropMime(quint32 cueId, quint32 layerId, const QMimeData *mime, bool separate = false,
                  const ShowTree::Target &target = ShowTree::Target(), bool stayOnCue = false,
                  bool child = false);
    /** The cue page's layer rows, as text ("⤳ Towers  Blue · tracked from 1
     *  One"), and the list itself (it takes drops). */
    QStringList cueLayerItems() const;
    QListWidget *cueLayerList() const { return m_cueLayers; }
    /** The target of the layer carried into cue @a cueId with @a signature
     *  (empty: none such). */
    ShowTree::Target carriedTarget(quint32 cueId, const QString &signature) const;
    /** Select a layer carried into cue @a cueId, by its target signature. */
    bool selectTracked(quint32 cueId, const QString &signature);

    /** MIME type of cues dragged within the tree (a stream of cue ids). */
    static const char *cueMimeType();
    /** A cue dropped on another of its list: moved to its place, or (with
        @a include, Alt held) included by it. */
    bool dropCue(quint32 cueId, quint32 onCueId, bool include);
    QString cueDropHint(quint32 cueId, quint32 onCueId, bool include) const;
    /** Cues @a cueIds (one list) dropped before (@a after: after) cue
     *  @a onCueId: moved there as a block and numbered between their new
     *  neighbours (ShowTreeController::moveCues). @a why: the refusal. */
    bool dropCues(const QList<quint32> &cueIds, quint32 onCueId, bool after, QString *why = nullptr);
    /** What that drop does ("Move 4 Fog between 2 and 3, as 2.5"), or why
     *  it can't (@a refused true). */
    QString cuesDropHint(const QList<quint32> &cueIds, quint32 onCueId, bool after,
                         bool *refused = nullptr) const;
    /** Renumber this level… of cue @a cueId from @a start in steps of
     *  @a step (ShowTreeController::renumberLevel). With @a ask, a dialog
     *  asks for start and step first, and lists the numbers show control
     *  fires that change when OSC or MSC is listening. */
    bool renumberLevel(quint32 cueId, int start = 1, int step = 1, bool ask = true);
    /** Inline rename of a cue row ("2.5 Fog"): number and name in one
     *  step; a new number moves the cue to its place, one taken is refused
     *  and said. */
    bool renameCue(quint32 cueId, const QString &number, const QString &name);

    /** A value picked (D21 as changed 2026-10-17): the palette that plays
     *  exactly it (In this cue list, then Recent, then any), else a one-off
     *  -- an inline palette on the layer (or cue), "Make reusable" a click
     *  away on its row. */
    bool setOneOffDimmer(quint32 cueId, quint32 layerId, int percent);
    bool setOneOffColour(quint32 cueId, quint32 layerId, const QColor &colour);
    /** The docked picker (Q61): ⋯ → Level… and Colour… open in a strip
     *  above the command line, inside the window (never a separate window
     *  that can fall behind it). @a editor goes in the strip; Apply (or
     *  Return) runs @a apply and closes it, Cancel (or Esc) just closes it.
     *  One at a time: opening another replaces it. */
    void openDockedPicker(const QString &title, QWidget *editor, QWidget *focus,
                          const std::function<void()> &apply);
    /** The docked picker's Apply / Cancel (tests and keys). */
    void applyDockedPicker();
    void closeDockedPicker();
    bool dockedPickerOpen() const;
    /** The Library's Recent palettes, newest first. */
    QList<quint32> recentPalettes() const;

    /** + Layer: the groups / fixtures selected in the Library as one layer,
     *  or (@a separate: ⌥-click, or its menu) one layer each -- as a drop. */
    bool addLayersFromLibrary(bool separate);
    /** Split a layer into one per group; merge layers of one cue. False /
     *  invalid (and @a why) when refused. */
    bool splitLayer(quint32 cueId, quint32 layerId, QString *why = nullptr);
    quint32 mergeLayers(quint32 cueId, const QList<quint32> &layerIds, QString *why = nullptr);
    /** The cue's own layers selected in the tree, when all are of one cue
     *  (@a cueId); empty otherwise. */
    QList<quint32> selectedLayers(quint32 *cueId = nullptr) const;
    /** Every own layer selected in the tree, of any cue, in tree order:
     *  (cue, layer). Two or more: the page edits them together. */
    QList<QPair<quint32, quint32>> selectedLayerRefs() const;
    /** The layers the page edits together now (two or more; else empty). */
    QList<QPair<quint32, quint32>> multiLayers() const { return m_multi; }
    /** Release on (or off) on every selected layer: one undo step. */
    bool setReleaseOnSelected(bool release);
    /** The cues selected in the tree (cue lists and sub-cues; not looks),
     *  in tree order. */
    QList<quint32> selectedCues() const;
    /** Skip (or, @a skip false, put back) cues @a cueIds: one undo step.
     *  The tree's tick, the menu's Skip cue / Unskip cue, ⌘/ and the cue
     *  page's Skip all come here. */
    bool setCuesSkipped(const QList<quint32> &cueIds, bool skip);
    /** ⌘/ in the tree: the selected cues skipped -- or, when every one of
     *  them is skipped already, put back. False: no cue selected. */
    bool toggleSkipSelected();
    /** Colour ▸ (the menus and the pages' Text colour button): the text
     *  colours (ShowTree::textColours()), Custom… and None, for cues
     *  @a cueIds or -- @a listId valid -- the cue list. Each one undo step. */
    void colourMenu(QMenu *menu, const QList<quint32> &cueIds, quint32 listId = 0xFFFFFFFF);
    /** Move in black ▸ Use show's (On) / On / Off for list @a listId (Q38). */
    void listMibMenu(QMenu *menu, quint32 listId);
    /** "Use show's (On)": the show's setting as it is now. */
    QString showMibLabel() const;
    /** @a hex ("" none) as the tree shows text on its background. */
    QColor treeTextColour(const QString &hex) const;
    /** Delete the cues and own layers selected in the tree (two or more)
     *  as one undo step; false (nothing done) when fewer, or a cue list
     *  or production is among them. */
    bool deleteSelected();

    /************************************************************************
     * Palettes in the Library (R2, P-D8): the everyday operations, through
     * the shared PaletteOps (one undo history with the tree)
     ************************************************************************/
public:
    /** + Palette: a new palette of @a type in the folder selected in the
     *  Library (else where the show keeps that type), selected there and
     *  opened in the Look Editor. */
    quint32 newPaletteFromLibrary(QLCPalette::PaletteType type);
    /** New folder in the Library (under the selected palette folder). */
    bool newPaletteFolder(const QString &name);
    /** The palette folder the Library's selection is in ("": none). */
    QString libraryPaletteFolder() const;
    /** F2 in the Library: rename the selected palette or palette folder in
     *  place (Return commits, Esc cancels). False: nothing to rename. */
    bool startLibraryRename();
    bool isLibraryRenaming() const { return m_libRenaming; }
    /** ⌫ in the Library: the selected palettes (asks, naming where each is
     *  used), or the selected palette folder (what is in it moves up). */
    bool deleteLibrarySelection();
    /** Palettes dropped on a Library palette folder ("": none). */
    bool dropPalettesOnFolder(const QList<quint32> &paletteIds, const QString &folder);
    /** All → Palettes by type, folders under each (the default), or folders
     *  first, any type in each (feat/palette-folders' view). Remembered. */
    bool paletteGroupByType() const { return m_paletteByType; }
    void setPaletteGroupByType(bool byType);
    /** All → Palettes as text, one row per item, indented two spaces a
     *  level: "Colour", "  01-Song/", "    Blue" (tests). */
    QStringList libraryPaletteRows() const;
    /** The Look Editor's Used by line, as text (tests). */
    QString usedByText() const;

    /************************************************************************
     * The Looks section (R3, P-D5..P-D7)
     ************************************************************************/
public:
    /** + Look: a new look after the selected one (in its folder), or in the
     *  selected Looks section / folder; the section is made the first time.
     *  Selected, its name field focused. */
    quint32 newLook();
    /** Looks dropped on a folder of their Looks section ("": the section
     *  itself, no folder). One undo step. */
    bool dropLooksOnFolder(const QList<quint32> &lookIds, quint32 listId, const QString &folder);
    /** What that drop would do ("Hit 1 → Hits"); empty: refused. */
    QString lookFolderDropHint(const QList<quint32> &lookIds, quint32 listId, const QString &folder) const;
    /** Make VC page (P-D7): a frame on the Virtual Console with one Toggle
     *  button per look in @a folder of Looks section @a listId ("": every
     *  look), captioned with the look's name, coloured by its first colour.
     *  False (and @a message why) when there is no VC or no look. */
    bool makeVcPage(quint32 listId, const QString &folder, QString *message = nullptr);
    /** The VC buttons that fire look @a lookId, as text. */
    QStringList lookFiredBy(quint32 lookId) const;
    /** Adopt as look (P-D16) scene @a functionId into the selected
     *  production; offers to re-point the VC buttons that fire the scene to
     *  the look. The look (invalid: refused, said in the hint). */
    quint32 adoptAsLook(quint32 functionId);
    /** The folder selected in the tree's Looks section ("": none). */
    QString currentLookFolder() const;

    /************************************************************************
     * Record as new cue: which stage (selected / running / everything lit)
     ************************************************************************/
public:
    typedef ShowTreeController::RecordSource RecordSource;
    /** QSettings key of the default for Design (@a perform false) or
     *  Perform: "ask" (default), "selected", "running" or "lit". */
    static QString recordSettingKey(bool perform);
    static QString recordSetting(bool perform);
    /** Record as new cue from @a cueId: when the choices differ the
     *  setting picks, or the dialog asks ("Don't ask again" writes the
     *  setting). The new cue, selected (invalid: cancelled). */
    quint32 recordFrom(quint32 cueId);
    /** Pick among @a choices (two or more) as recordFrom() does. */
    bool chooseRecordSource(quint32 cueId, const QList<RecordSource> &choices, RecordSource *chosen);
    /** In place of the dialog (tests): false cancels; @a dontAsk set writes
     *  the choice as the default for the mode. */
    typedef std::function<bool(const QList<RecordSource> &choices, RecordSource *chosen, bool *dontAsk)> RecordAsker;
    void setRecordAsker(const RecordAsker &asker) { m_recordAsker = asker; }
    /** Clear an attribute on the layer/cue (it inherits again), or set it
     *  None (off here). */
    bool clearAttribute(quint32 cueId, quint32 layerId, const QString &key);
    /** An effect's own Colour/Dimmer looks (they apply inside it only). */
    bool setEffectOwn(quint32 cueId, quint32 layerId, const QString &effectKey,
                      const QList<quint32> &palettes,
                      const ShowTree::Target &target = ShowTree::Target());
    bool setAttributeNone(quint32 cueId, quint32 layerId, const QString &key);

    /** Set a line parameter look (@a key KeySpread / KeyGrouping /
     *  KeyCross, @a value as typed; empty clears it) on the layer, the
     *  carried-in @a target, or (no layer) the cue. */
    bool setLineParam(quint32 cueId, quint32 layerId, const QString &key, const QString &value,
                      const ShowTree::Target &target = ShowTree::Target());
    /** The layer page's "Contains:" line, as text (tests). */
    QString layerContentsText() const;
    /** The order strip under a line aim's Position row (null: none). */
    QWidget *lineStrip() const;
    /** The plan view under it (D19), a LinePlanView (tests). */
    QWidget *linePlan() const;
    /** The attribute rows the inspector shows for the selected layer, as
     *  text: "Colour | Blue | from cue 1" -- for the tests. */
    QStringList attributeRows() const;
    /** The Library's Recent items, newest first, as text. */
    QStringList recentItems() const;
    /** The Library's "In this cue list" items, most used first, as text
     *  ("Towers ×6"). */
    QStringList inListItems() const;

    /** "Next cue receives", as the cue page shows it. */
    QString nextCueText() const;
    /** ... for cue @a cueId (empty: not a cue of a list). */
    QString nextCueText(quint32 cueId) const;
    /** The cue page's "Tracked in" list, as text: one line per carried-in
     *  layer ("Towers — tracked from 1 One"), then one per value under it
     *  ("    Colour: Blue"). Empty when nothing tracks in. */
    QString cueTrackedText() const;
    /** What tracks into cue @a cueId, its own changes, what the next cue
     *  receives and whether the last edit changed it: the cue row's
     *  tooltip in the tree. */
    QString cueTrackingTip(quint32 cueId) const;
    /** The tree's badge column (the second): its width, kept as the tree
     *  pane is resized, set by dragging the line between the columns and
     *  remembered (QSettings). */
    int badgeColumnWidth() const { return m_badgeWidth; }
    /** The cue page's includes, as text, in order. */
    QStringList includeItems() const;

    /** D22: a tracked value's picker -- where it was set, as a cue label
     *  ("1.0.0 base"); empty when the value is this cue's own. */
    QString updateSourceLabel(quint32 cueId, const ShowTree::Target &target, const QString &key) const;

    /************************************************************************
     * Command line (§6C, optional)
     ************************************************************************/
public:
    /** The command line is always there (bottom of the tab); this gives it
     *  the keyboard, as : in the tree does. */
    void focusCommandLine();
    bool isCommandLineVisible() const;
    /** Run a command as if typed and Enter pressed: true when it ran. */
    bool runCommand(const QString &line);
    /** What the line under the command line says now. */
    QString commandMessage() const;
    QLineEdit *commandLine() const { return m_command; }
    /** The help output over the command line (`help`, `?`), hidden until
     *  asked for; Esc closes it. Its text, plain (tests). */
    QString helpOutput() const;
    bool isHelpShown() const;
    /** Q59: the command line's syntax lines, folded away by default. ?
     *  typed first in the line, the ? button beside it or F1 there shows
     *  them; the choice is remembered (QSettings showtree/commandSyntax). */
    bool isCommandSyntaxShown() const;
    void setCommandSyntaxShown(bool shown);

    /** ⌘C / ⌘V / ⌘D in the tree (Copy / Paste / Duplicate in its menu):
     *  the selected cues (in list order) or own layers; paste puts cues as
     *  a run after the selected cue, layers into it. One undo step. */
    bool copySelection();
    bool pasteClipboard();
    bool duplicateSelection();

    /** Open @a paletteId in the Look Editor under the tree. */
    void editPalette(quint32 paletteId);
    /** The palette the Look Editor shows (invalid: none). */
    quint32 editedPalette() const;
    LookEditor *lookEditor() const { return m_lookEditor; }

    /** What the header says about the preview: "▶ On the rig: 1 One". */
    QString previewText() const;

    /************************************************************************
     * D13: what the tree made is changed in the tree
     ************************************************************************/
public:
    /** The Show Tree tab (null: none). */
    static ShowTreeView *instance();
    /** Bring the tab forward with generated function @a functionId's node
     *  selected (a list's chaser: the list; a cue's collection: the cue; a
     *  layer's scene: the layer). False: not in the tree. */
    bool reveal(quint32 functionId);
    /** A bar saying generated function @a functionId is the tree's
     *  (ShowTreeController::lockedText() and what it plays), with **Show in
     *  Show Tree** and, for a layer's scene, **Detach to plain scene**;
     *  @a detached is told the new scene. Editors of a generated function
     *  show it in place of (or, a chaser's, above) their controls. */
    static QWidget *lockedBanner(QWidget *parent, Doc *doc, quint32 functionId,
                                 const std::function<void(quint32)> &detached = nullptr);
    /** When any of @a functionIds is generated: says so (a message titled
     *  @a title naming them, with Show in Show Tree) and returns true -- the
     *  caller then leaves them alone (deleting, renaming, adding to). */
    static bool refuseGenerated(QWidget *parent, Doc *doc, const QList<quint32> &functionIds,
                                const QString &title);
    /** Detach the layer generated scene @a sceneId is (where it is a layer
     *  of a cue's own): the new plain scene, invalid when there is none. */
    static quint32 detachGenerated(Doc *doc, quint32 sceneId);

    /** Rename the selected tree row in place, as the Programming tab does
     *  (Return or F2 in the tree): a production, cue list, cue (its name,
     *  not its number) or layer (empty: back to its groups' name). Return
     *  commits, Esc cancels; one undo step. False: nothing to rename. */
    bool startRename();
    bool isRenaming() const { return m_renaming; }

    /************************************************************************
     * ⇧⌘N, New (here): the next thing the selection calls for
     * (showtreenewhere.cpp)
     ************************************************************************/
public:
    /** ⇧⌘N (and Edit ▸ New …): with groups, fixtures or sets selected in
     *  the Library and the focus there, a layer on them in the selected
     *  cue; else by the tree's selection -- nothing, or a production: a cue
     *  list in it (no production yet: one); a cue list: a cue at its end; a
     *  cue: the next cue after it (numbered as + Cue numbers one; a sub-cue:
     *  the next main cue, as + Cue does); a layer: a new layer in its cue,
     *  on what the Library has selected (nothing there: the cue selected and
     *  the Library's filter focused, so Return makes it); a look: the next
     *  look. Something other than a cue opened in the docked Workbench: its
     *  + Scene / + Step. One undo step; the new row selected and renaming in
     *  place. Its id (invalid: nothing made). */
    quint32 newHere();
    /** What newHere() would make now ("New Cue After 0.0.3"; empty:
     *  nothing) -- the Edit menu's text. */
    QString newHereLabel() const;

    /************************************************************************
     * W3: Show Tree <-> Workbench (WORKFLOW_DESIGN.md §3.4). A cue, look or
     * layer opens in the Workbench, docked in place of the page; ↩ puts
     * the page back with the cue (and layer) selected. (showtreeworkbench.cpp)
     ************************************************************************/
public:
    /** Open cue (or look) @a cueId in the Workbench, docked here, with its
     *  layer @a layerId -- or the layer carried in on @a trackedSignature
     *  -- selected. False: no such cue. */
    bool openInWorkbench(quint32 cueId, quint32 layerId = 0xFFFFFFFF,
                         const QString &trackedSignature = QString());
    /** The tree's selection (a cue, a look, a layer) in the Workbench. */
    bool openSelectionInWorkbench();
    /** ↩: the page back, the Workbench handed back, the cue (and the layer
     *  the Workbench had selected) selected in the tree. */
    bool closeWorkbench();
    bool isWorkbenchDocked() const;
    /** The Workbench docked here (null: none). */
    class Workbench *dockedWorkbench() const;
    /** Where the Workbench comes from: Build's (App sets it) -- @a borrow
     *  takes it out of Build, @a giveBack puts it back as it was. Without
     *  one, the tree makes a Workbench of its own (tests). */
    void setWorkbenchLender(const std::function<class Workbench *()> &borrow,
                            const std::function<void(class Workbench *)> &giveBack);

    bool isPinned(int kind, quint32 id) const;
    void setPinned(int kind, quint32 id, bool pinned);

public slots:
    void reload();
    void slotNewProduction();
    void slotNewCueList();
    void slotNewCue();
    void slotAddLayer();
    void slotRecordCue();
    void slotDelete();
    /** Merge layers: the layers selected in the tree (two or more, of one
     *  cue), as the context menu's Merge. */
    void slotMergeSelected();
    void slotUndo();
    void slotRedo();
    /** Import a chaser as a cue list (§7): shows the report, a sheet
     *  (W0) that also offers to fire the list from what fires the chaser. */
    void importChaser(quint32 chaserId, bool showReport = true);
    /** This tab forward (and its window), the tree focused. */
    void bringForward();

protected:
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;
    /** A theme change re-colours the cue page's drop area. */
    void changeEvent(QEvent *ev) override;

private slots:
    void slotTreeSelectionChanged();
    void slotPreviewChanged();
    void slotTreeContextMenu(const QPoint &pos);
    void slotLibraryContextMenu(const QPoint &pos);
    void slotCueEdited();
    void slotListModeChanged(int index);
    void slotListBaseChanged(int index);
    void slotCueBaseToggled(bool on);
    void slotCueMixToggled(bool on);
    void slotCueSkipToggled(bool on);
    void slotCueNoMibToggled(bool on);
    void slotListMibChanged(int index);
    void slotLayerReleaseToggled(bool on);
    void slotCommandEntered();
    void slotFilterChanged(const QString &text);
    void slotTreeChanged();
    void reloadLibrary();

private:
    /** The groups selected in the Library, in order, each once. */
    QList<quint32> selectedLibraryGroups() const;
    /** Select (only) the Library's rows of @a groupIds, their folders
     *  opened, the first scrolled to. Returns how many groups. */
    int selectLibraryGroups(const QList<quint32> &groupIds);
    void buildTree();
    void updateInspector();
    void updateButtons();
    void showCuePage(const ShowTree::CueList &list, int index);
    void showLayerPage(const ShowTree::CueList &list, int index, const ShowTree::Layer *own,
                       const ShowTree::ResolvedLayer *resolved);
    /** Several layers selected: one page editing them together. */
    void showMultiLayerPage(const QList<QPair<quint32, quint32>> &layers);
    /** Whether (@a cueId, @a layerId) is one of several edited together. */
    bool isMulti(quint32 cueId, quint32 layerId) const;
    /** Run @a one on every layer edited together (one undo step, @a label),
     *  or only on (@a cueId, @a layerId) when it isn't one of them. */
    bool forEachEdited(quint32 cueId, quint32 layerId, const QString &label,
                       const std::function<bool(quint32, quint32)> &one);
    /** @a target: a carried-in layer's (set on it makes it the cue's own);
     *  @a rowTarget: the row's layer, own or not (for "Update cue X"). */
    void attributeMenu(quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                       const ShowTree::Target &rowTarget, const QString &key, const QPoint &globalPos);
    void includesMenu(QMenu *menu, quint32 cueId);
    /** A line's Spread / Grouping / Cross (D16): submenus that set them on
     *  the layer (or the carried-in @a target, or the cue). */
    void lineParamMenu(QMenu *menu, quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                       const QString &onlyKey = QString());
    void updateIncludeButtons();
    /** The cue list the selection is in (invalid: none). */
    quint32 currentListId() const;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    /** Esc that nothing took: the docked Workbench backs out (to the page:
     *  ↩ Show Tree), else the Look Editor under the page closes. */
    void keyPressEvent(QKeyEvent *ev) override;
    /** Set @a look on the layer, on the carried-in @a target, or the cue. */
    bool applyLook(quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                   const ShowTree::Look &look);
    quint32 inlinePalette(quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                          QLCPalette::PaletteType type, const QVariantList &values, const QString &value);
    /** The palette playing exactly @a values to reuse, else inlinePalette(). */
    quint32 valuePalette(quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                         QLCPalette::PaletteType type, const QVariantList &values, const QString &value);
    bool askRecordSource(quint32 cueId, const QList<RecordSource> &choices, RecordSource *chosen,
                         bool *dontAsk);
    QString lookText(const ShowTree::Look &look) const;
    QString originText(const ShowTree::CueList &list, const ShowTree::ResolvedValue &value) const;
    /** Where a carried-in layer (or one of its values) came from, by
     *  its cue, wherever that cue is ("tracked from 1 One"). */
    QString carriedOrigin(ShowTree::Origin origin, quint32 fromCue) const;
    /** A carried-in layer, for a tooltip or the cue page: its name and
     *  where it came from, then each value ("    Colour: Blue"), with its
     *  own origin when that is another cue. */
    QString carriedDetail(const ShowTree::ResolvedLayer &layer) const;
    /** The tree's columns: the names get all but the badge column. */
    void fitTreeColumns();
    QString paletteName(quint32 id) const;
    /** The Look Editor changed @a paletteId (@a full: its name/icon too). */
    void paletteEdited(quint32 paletteId, bool full);
    void noteRecent(int kind, quint32 id, const QString &label = QString());
    QTreeWidgetItem *findItem(quint32 id, quint32 cueId) const;
    void saveExpanded();
    /** Preview the selected cue (Design mode, tab on screen), else stop. */
    void syncPreview();

    Doc *m_doc;
    /** The Doc's; guarded, so a hide event during teardown (the Doc gone
     *  first) finds it null instead of freed. */
    QPointer<ShowTreeController> m_st;

    QToolButton *m_newProduction;
    QAction *m_newProductionAction = nullptr;   ///< on the bar while there's no production (Q96)
    QToolButton *m_newList;
    QToolButton *m_newCue;
    QToolButton *m_addLayer;
    QToolButton *m_delete;
    QToolButton *m_undo;
    QToolButton *m_redo;
    QToolButton *m_import;
    QToolButton *m_record;
    QToolButton *m_merge;
    QLabel *m_hint;
    QLabel *m_previewLabel;

    QTreeWidget *m_tree = nullptr;
    int m_badgeWidth = 0;               //!< the badge column's width
    bool m_fittingColumns = false;

    // Inspector
    QStackedWidget *m_pages;
    QLabel *m_emptyPage;
    QWidget *m_listPage;
    /** P5: the production page (current, main list, timecode) and the
     *  header's switcher of the current production. */
    QWidget *m_prodPage = nullptr;
    QLabel *m_prodTitle = nullptr;
    QPushButton *m_prodCurrent = nullptr;
    QComboBox *m_prodMainList = nullptr;
    QComboBox *m_prodTimecode = nullptr;
    QLabel *m_prodInfo = nullptr;
    QComboBox *m_production = nullptr;
    /** The header's "Production:" label and switcher, hidden with one
     *  production (Q75). */
    class QAction *m_productionLabelAction = nullptr;
    class QAction *m_productionAction = nullptr;
    /** One production (Q75): it plays on GO; no Current / Main list /
     *  switcher until a second one is added. */
    bool singleProduction() const;
    class MoveInBlackButton *m_moveInBlack = nullptr;
    void showProductionPage(quint32 productionId);
    void updateProductionSwitcher();
    /** The timecode choices: (label, universe) -- the show's, any, each universe. */
    QList<QPair<QString, qint32>> timecodeChoices() const;
public:
    QComboBox *productionSwitcher() const { return m_production; }
    /** Auto move-in-black (MIB) in the bar */
    class MoveInBlackButton *moveInBlackButton() const { return m_moveInBlack; }
private:
    QLabel *m_listTitle;
    QComboBox *m_listMode;
    QComboBox *m_listBase;
    QLabel *m_listInfo;
    QWidget *m_cuePage;
    QLineEdit *m_cueNumber;
    QLineEdit *m_cueName;
    NoteEdit *m_cueNote;
    QComboBox *m_cueMode;
    QCheckBox *m_cueBase;
    QCheckBox *m_cueMix;
    QCheckBox *m_cueSkip = nullptr;
    /** Q38: "Don't move in black into this cue"; the list's Move in black. */
    QCheckBox *m_cueNoMib = nullptr;
    QComboBox *m_listMib = nullptr;
    /** The pages' Text colour buttons (Colour ▸ menus). */
    QToolButton *m_cueColour = nullptr;
    QToolButton *m_listColour = nullptr;
    /** What a Text colour button shows: @a own, else (dimmed) @a inherited. */
    void showColourButton(QToolButton *button, const QString &own, const QString &inherited);
    QListWidget *m_cueIncludes;
    QToolButton *m_includeAdd;
    QToolButton *m_includeRemove;
    QToolButton *m_includeUp;
    QToolButton *m_includeDown;
    QDoubleSpinBox *m_cueFadeIn;
    QDoubleSpinBox *m_cueFadeOut;
    QDoubleSpinBox *m_cueHold;
    /** P6: a sub-cue's advance (Sub GO / Auto-follow + wait); a cue's
     *  sub-cues line (+ Sub-cue). */
    QComboBox *m_cueAdvance = nullptr;
    QDoubleSpinBox *m_cueWait = nullptr;
    /** Q60: a time typed (or stepped) on the cue page and not yet applied
     *  -- for m_pageCue. Applied before the page shows anything else, so a
     *  click on another cue keeps it; Esc in the field puts it back. */
    bool m_cueTimesDirty = false;
    bool m_cueWaitDirty = false;
    bool m_committingTimes = false;
public:
    /** Q60: apply the cue page's typed times to the cue it shows (true:
     *  something was applied). Done before a click in the tree, before the
     *  page changes, and on Return / focus out. */
    bool commitPendingCueEdits();
private:
    /** Esc in a time field: back to the cue's own value, nothing applied. */
    void revertCueTimes();
    QWidget *m_cueAdvanceRow = nullptr;
    QLabel *m_cueSubCues = nullptr;
    QToolButton *m_newSubCue = nullptr;
public:
    /** + Sub-cue: a sub-cue of the selected cue (after the selected
     *  sub-cue). Its id (invalid: none). */
    quint32 newSubCue();
private:
    QLabel *m_cueLooks;
    QListWidget *m_cueLayers;
    QLabel *m_cueDropZone = nullptr;
    /** Something is being dragged over the drop area. */
    bool m_cueDropActive = false;
    /** The drop area's look, from the current palette: the highlight while
     *  a drag is over it, else a quiet colour that still reads as text on
     *  every theme (palette(mid) all but vanishes on the dark ones). */
    void styleCueDropZone();
    QLabel *m_cueNext;
    QLabel *m_cueTracked = nullptr;     //!< what tracks in, value by value
    QWidget *m_layerPage;
    QLabel *m_layerTitle;
    QLabel *m_layerContents;
    QCheckBox *m_layerRelease;
    ShowTree::Target m_layerReleaseTarget;     //!< a carried-in layer's, for Release
    QGridLayout *m_rows;
    QWidget *m_rowsHost;
    QStringList m_rowText;

    LookEditor *m_lookEditor = nullptr;

    // Library
    QLineEdit *m_filter;
    QTreeWidget *m_library;
    QList<QPair<int, quint32>> m_recent;
    QList<QPair<int, quint32>> m_pinned;
    /** Library folders open before a filter was typed (it opens more). */
    QSet<QString> m_openWithoutFilter;
    bool m_filtering = false;

    QTimer *m_libraryTimer = nullptr;
    quint32 m_libraryList = 0xFFFFFFFF;    //!< the list "In this cue list" shows

    // Command line
    QWidget *m_commandBar = nullptr;
    /** The docked picker strip (Q61), its body and what Apply runs. */
    QWidget *m_picker = nullptr;
    class QLabel *m_pickerTitle = nullptr;
    class QHBoxLayout *m_pickerBody = nullptr;
    std::function<void()> m_pickerApply;
    QLineEdit *m_command = nullptr;
    QLabel *m_commandMsg = nullptr;
    QTextBrowser *m_commandHelp = nullptr;
    QLabel *m_commandSyntax = nullptr;
    QToolButton *m_commandSyntaxButton = nullptr;
    ShowTreeCommand *m_commander = nullptr;
    QStringList m_commandHistory;
    RecordAsker m_recordAsker;
    Confirm m_confirm;
    FunctionOpener m_functionOpener;
    SheetRunner m_sheetRunner;
    ConverterRunner m_converterRunner;
    QFrame *m_bridgeBanner = nullptr;
    QLabel *m_bridgeText = nullptr;
    bool m_bridgeDismissed = false;
    QPushButton *m_listRestore = nullptr;
    bool confirm(const QString &title, const QString &text, const QString &details = QString());
    /** The reuse line's text for a layer (own, in cue @a cueId) or a cue. */
    QString reuseLine(quint32 cueId, quint32 layerId) const;
    void reuseLinkActivated(const QString &link);
    // ⇧⌘N (showtreenewhere.cpp): what newHere() makes, worked out once for
    // the label and the doing.
    struct NewHerePlan;
    NewHerePlan planNewHere() const;

    // W3 (showtreeworkbench.cpp)
    struct WorkbenchDock;
    WorkbenchDock *m_wb = nullptr;
    /** The tree's selection moved while docked: the Workbench follows. */
    void workbenchFollowTree();
    /** The page (the inspector) lent to the docked Workbench's editor pane,
     *  for cue @a cueId's layer @a layerId / carried-in @a signature (none:
     *  the cue's page). */
    QWidget *lendInspector(quint32 cueId, quint32 layerId, const QString &signature);
    void takeBackInspector();
    /** The Workbench selected a layer (or, none, the cue): the tree too,
     *  without the tree moving the Workbench in turn. */
    void selectForWorkbench(quint32 cueId, quint32 layerId, const QString &signature);
    /** Leaving / coming back to the tab while docked. */
    void suspendWorkbench();
    void resumeWorkbench();
    void destroyWorkbenchDock();
    QLabel *m_layerReuse = nullptr;
    QLabel *m_cueReuse = nullptr;
    QPushButton *m_listConvert = nullptr;
    QPushButton *m_listDetach = nullptr;
    int m_historyAt = -1;
    QList<quint32> m_expanded;
    quint32 m_pageCue = 0xFFFFFFFF;      //!< the cue the cue page shows
    bool m_reloading = false;
    bool m_renaming = false;
    QList<QPair<quint32, quint32>> m_multi;
    bool m_applyingMulti = false;
    void slotTreeItemRenamed(QTreeWidgetItem *item, int column);
    bool m_inspectorUpdating = false;

    // R2: palettes in the Library
    QToolButton *m_newPalette = nullptr;
    bool m_paletteByType = true;
    bool m_libRenaming = false;
    QTreeWidgetItem *m_libRenameItem = nullptr;
    void slotLibraryItemRenamed(QTreeWidgetItem *item, int column);
    /** The Look Editor's Used by line for @a paletteId. */
    void updateUsedBy(quint32 paletteId);
    void usedByLinkActivated(const QString &link);
    /** The Library's All → Palettes item for @a paletteId, selected. */
    void selectLibraryPalette(quint32 paletteId);
    /** + Palette's menu, by type: what Programming offers. */
    void fillNewPaletteMenu(QMenu *menu, const QString &folder);
    /** The Library's palette rows, under All. */
    void buildLibraryPalettes(QTreeWidgetItem *all, const std::function<QTreeWidgetItem *(QTreeWidgetItem *, int, quint32)> &add);

    // R3: the Looks section
    QFormLayout *m_cueForm = nullptr;
    QFormLayout *m_listForm = nullptr;
    QLabel *m_lookFiredBy = nullptr;
    QPushButton *m_listVcPage = nullptr;
    QSet<QString> m_collapsedFolders;      //!< "listId:path" of look folders closed by hand
    /** Whether the selection is in a Looks section (its node, a folder, a
     *  look, a look's layer). */
    bool selectionInLooks() const;
    /** The production the selection is in (invalid: none). */
    quint32 currentProductionId() const;
    void showLooksPage(quint32 listId, const QString &folder);
    void lookLinkActivated(const QString &link);
    /** "Move to folder" for looks @a lookIds of Looks section @a listId. */
    void lookFolderMenu(QMenu *menu, quint32 listId, const QList<quint32> &lookIds);
};

/** @} */

#endif

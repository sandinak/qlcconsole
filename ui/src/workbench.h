/*
  Q Light Controller Plus
  workbench.h

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

#ifndef WORKBENCH_H
#define WORKBENCH_H

#include <QSharedPointer>
#include <QPointer>
#include <QWidget>
#include <functional>

#include "programmingoutline.h"
#include "function.h"

class WorkbenchStack;
class SceneGroupLooks;
class FixtureConsole;
class QStackedWidget;
class QMenu;
class QKeyEvent;
class QScrollArea;
class QToolButton;
class QPushButton;
class LookEditor;
class QSplitter;
class QLabel;
class Scene;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * The Workbench (docs/design/WORKFLOW_DESIGN.md §3.4): one editor for "a stack of
 * things that play together". It loads a scene, a collection, a chaser or
 * any other function -- W3 adds a Show Tree cue -- and shows it the same
 * way every time:
 *
 * - a header: ↩ back, where it is, its name and kind, where it is used and
 *   what fires it, what is playing;
 * - its layers (a ProgrammingOutline in Stack mode): a scene opens up into
 *   its targets and looks, anything else is a ▣ row that opens;
 * - + Scene / + EFX / + RGB Matrix, remove, reorder, ▣ Open;
 * - the editor pane, at full height, for the selected row: the Look Editor
 *   for a look, the channel console for a target, the scene's timing, the
 *   stock EFX / RGB Matrix / Chaser... editor for a function;
 * - a live preview of what is loaded (Design mode).
 *
 * Every edit is one step on the shared history (PaletteOps), so Undo in
 * either tab takes it back. It is self-contained: the Build tab hosts it,
 * and W3 docks the same widget in the Show Tree with a TreeCueStack loaded
 * (setStack(), setReturnTo()).
 */
class Workbench final : public QWidget
{
    Q_OBJECT
    friend class HelpShots;

public:
    Workbench(Doc *doc, QWidget *parent = nullptr);
    ~Workbench();

    /** Each pane's wrapping text needs more height at the narrowest width
     *  than the layouts' plain minimum says. */
    QSize minimumSizeHint() const override;

    /************************************************************************
     * What is loaded
     ************************************************************************/
public:
    /** Load function @a functionId afresh (from a navigator): ↩ starts over. */
    void load(quint32 functionId);
    /** Open function @a functionId from what is loaded: ↩ comes back. */
    void open(quint32 functionId);
    /** Load any back end (the Workbench owns it); @a keepHistory: ↩ comes
     *  back to what was loaded. */
    void setStack(WorkbenchStack *stack, bool keepHistory = false);
    WorkbenchStack *stack() const { return m_stack; }
    /** The function loaded (invalid: nothing, or a back end that isn't one). */
    quint32 loadedFunction() const;
    /** Unload: nothing loaded, ↩ forgotten. */
    void clear();

    /** ⇧⌘N on what is loaded: a chaser, a new empty scene as a step after
     *  the step walked to (walked to); a collection (or a chaser step's), a
     *  new empty scene as a layer (+ Scene ▸ New empty scene). One undo
     *  step; the new row selected, renaming in place. Its id (invalid:
     *  nothing loaded that takes one -- a cue is the Show Tree's). */
    quint32 newHere();
    /** What newHere() would make ("New Step After 2 in Chase"; empty:
     *  nothing). */
    QString newHereLabel() const;

    /** ↩: back to what was loaded before (or to setReturnTo()'s place). */
    bool goBack();
    bool canGoBack() const;
    /** Esc: back out one level. A palette opened from a list or a group's
     *  layout, over what was showing: back to that (its ↩ / ← Back); the
     *  effect bench: left as clicking away leaves it, to what it was
     *  opened from; else ↩ (goBack()). Edits stay. False: nowhere to go
     *  (Esc goes on). */
    bool backOut();
    /** In place of backOut()'s question (tests): @a title, @a text; true
     *  leaves. Unset: a message box, Keep designing the default. */
    typedef std::function<bool(const QString &title, const QString &text)> Confirm;
    void setConfirm(const Confirm &confirm) { m_confirm = confirm; }
    /** Where ↩ goes when there is nothing to go back to here ("Show Tree"),
     *  and how; an empty @a label clears it. W3 docks the Workbench in the
     *  Show Tree with this. */
    void setReturnTo(const QString &label, const std::function<void()> &go);

    /** The layer rows. */
    ProgrammingOutline *rows() const { return m_rows; }
    /** Select @a row in the layers and edit it. */
    bool selectRow(const ProgrammingOutline::Row &row);

    /************************************************************************
     * What is being edited
     ************************************************************************/
public:
    /** The scene the editor pane edits: the selected layer's (the loaded
     *  scene's), invalid if none. */
    /** Refine on the controller what is loaded (docs/design/FOCUS_REFINE_DESIGN.md 6B):
     *  a Show Tree cue from its selected layer, else the stack's scenes from
     *  @p sceneId (invalid: the current one), on group @p groupId or fixture
     *  @p fixtureId when given. False when there is nothing to refine. */
    bool openRefine(quint32 sceneId = 0xFFFFFFFF, quint32 groupId = 0xFFFFFFFF,
                    quint32 fixtureId = 0xFFFFFFFF);
    /** The scenes Refine would step through (the stack's scene layers). */
    QList<quint32> refineScenes() const;
    quint32 currentSceneId() const { return m_currentScene; }
    /** The palette the Look Editor shows (invalid: none). */
    quint32 currentPaletteId() const;
    LookEditor *lookEditor() const { return m_lookEditor; }
    /** The fixtures selected (a target row, the 2D view, the sources). */
    QList<quint32> selectedFixtures() const { return m_selectedFixtures; }

    /** Edit palette @a paletteId in the Look Editor (from the Palettes
     *  list), tried on the scene being edited. */
    void editPalette(quint32 paletteId);
    /** A fixture group's head layout in the editor pane (← back returns). */
    void showGroupLayout(quint32 groupId);
    /** Fixtures picked elsewhere (the 2D view, the sources): their channels. */
    void selectFixtures(const QList<quint32> &fixtureIds);

    /** Selection ▸ New Layer from Selection (selectionmenu.h): a new scene
     *  layer on @a fixtures after the selected one (a Show Tree cue: a
     *  layer targeting them), selected, the fixtures its target. One undo
     *  step. False: nothing made (newLayerBlocker() says why). */
    bool newLayerFromFixtures(const QList<quint32> &fixtures);
    /** Why newLayerFromFixtures() can't ("" it can). */
    QString newLayerBlocker(const QList<quint32> &fixtures) const;
    /** Selection ▸ Add to Current Layer: @a fixtures added to the selected
     *  layer's targets (a scene loaded alone: the scene). One undo step. */
    bool addFixturesToCurrentLayer(const QList<quint32> &fixtures);
    /** Why addFixturesToCurrentLayer() can't ("" it can). */
    QString addToLayerBlocker(const QList<quint32> &fixtures) const;
    /** The layer Add to Current Layer adds to (-1: none). */
    int currentLayer() const;

    /** Scene @a sceneIds changed elsewhere (the navigator): show them. */
    void scenesEdited(const QList<quint32> &sceneIds);
    /** The scene being edited was changed from outside (a snapshot, a
     *  bundle stamp): show it, one step named @a label. */
    void sceneEditedOutside(const QString &label);
    /** Effects were just synced for the scene being edited: don't again. */
    void markEffectsSynced();
    /** Show everything as it now is (after an Undo). */
    void refreshAll();
    /** Apply an edit to the live preview without a stop/start. */
    void refreshPreview();

    enum Page { EmptyPage, OverviewPage, ScenePage, LookPage, TargetPage, FunctionPage };
    Page currentPage() const;

    /************************************************************************
     * W3: a Show Tree cue loaded (TreeCueStack), and W4's plug
     ************************************************************************/
public:
    /** What W4's effect designer is asked to design: an Effect row of a
     *  cue's layer (or the layer itself, @a key empty). */
    struct EffectRequest
    {
        /** Invalid: no cue -- the bench (Build ▸ Effects): design effect
         *  palette @a palette (invalid: a new effect) on the Try on groups. */
        quint32 cueId = 0xFFFFFFFF;
        quint32 layerId = 0xFFFFFFFF;   //!< invalid: a layer carried in
        QString targetSignature;        //!< ShowTree::Target::signature()
        QString key;                    //!< "Effect.Color"; empty: the layer's
        quint32 palette = 0xFFFFFFFF;   //!< the effect it has (invalid: none)
        /** The bench: the cue or scene it was opened from, Make a layer's
         *  first choice. Both invalid: what the Workbench had loaded (a
         *  cue, a plain scene) -- or the bench showing already, its own. */
        quint32 fromCue = 0xFFFFFFFF;
        quint32 fromScene = 0xFFFFFFFF;
    };
    /** The effect designer's widget for @a request, made for the editor
     *  pane (the Workbench owns it and deletes it when the row changes);
     *  null: not designed here (the Look Editor shows the effect palette,
     *  or the layer's page). W4 sets it; until then nothing is set. */
    typedef std::function<QWidget *(Workbench *workbench, const EffectRequest &request,
                                    QWidget *parent)> EffectPaneFactory;
    static void setEffectPaneFactory(const EffectPaneFactory &factory);
    /** Ask for the effect designer on the selected cue layer (its Effect
     *  row, or the layer). False: no designer plugged in, or no cue layer. */
    bool designEffect();
    /** Ask for the effect designer for @a request: a cue layer's, or the
     *  bench's (cueId invalid; Build ▸ Effects) -- what is loaded is
     *  unloaded first and the layers column steps aside while it shows.
     *  False: no designer plugged in, or it declined. */
    bool designEffect(const EffectRequest &request);
    /** The designer the factory made, while the editor pane shows it
     *  (null: none). The Workbench deletes it when the row changes. */
    QWidget *effectPane() const
    { return m_funcEditor != nullptr && m_funcEditor == m_effectPane ? m_effectPane.data() : nullptr; }
    /** The show this Workbench edits (for a factory). */
    Doc *doc() const { return m_doc; }
    /** The page the back end lent the editor pane (the Show Tree's cue or
     *  layer page; null: none). */
    QWidget *lentPane() const { return m_lentPane; }
    /** Solo: only the selected layer plays (a cue loaded); else in context. */
    void setSolo(bool solo);
    bool isSolo() const;
    /** The pointer line to the Show Tree's Record (null label: not built).
     *  Shown only while @a loadedFunction is a look worth recording: a
     *  Scene with levels in it, or any other function, played from
     *  Build -- never a Show Tree cue (the Record is already beside it). */
    bool recordPointerWanted() const;
    QLabel *recordPointer() const { return m_recordPointer; }

signals:
    /** What is loaded changed (its function; invalid: none or not one). */
    void loaded(quint32 functionId);
    void fixturesSelected(const QList<quint32> &fixtureIds);
    /** The Look Editor shows another palette (or redrew it in place). */
    void currentPaletteIdChanged(quint32 paletteId);
    /** A palette was edited or renamed here: lists of palettes refresh. */
    void paletteEdited(quint32 paletteId);
    /** "Show in Palettes". */
    void revealPaletteRequested(quint32 paletteId);
    /** "How this works". */
    void guideRequested();
    /** Send to Show Tree… on a loaded chaser (its header button). */
    void sendToShowTreeRequested(quint32 chaserId);
    /** The quiet line under Play: "Record it as a cue in the Show Tree ▸". */
    void recordPointerClicked();

protected:
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;
public:
    /** RC8, "GO holds previews" (ShowTreeController::stageTakenForRun):
     *  what previews here stops and holds until Play; says so in
     *  @a held. Public for the tests. */
    void holdForRun(QStringList *held);
    /** The user picked what is loaded (the Show Tree's selection): the
     *  latest action wins, so a hold by a run lets go and it plays, in
     *  Design (docs/design/LIVE_BLIND.md). */
    void playPicked();
protected:
    /** Esc: backOut(). */
    void keyPressEvent(QKeyEvent *ev) override;

private slots:
    void slotRowsSelectionChanged();
    void slotLookEdited(quint32 paletteId);
    void updateTrialPalette(quint32 paletteId);
    void slotCanvasModified();
    void slotFixtureValueChanged(quint32 fxi, quint32 ch, uchar value);
    void slotFixtureChecked(quint32 fxi, quint32 ch, bool state);
    void slotModeChanged();

private:
    /** Build the header, the rows, the add bar and the editor pane. */
    void buildHeader(QWidget *column);
    void buildAddBar(QWidget *column);
    void buildEditorPane();

    void showPage(Page page);
    /** The header, the add bar and the overview, as the stack now is. */
    void updateHeader();
    void updateAddBar();
    void updateOverview();
    void updateScenePage();
    void updateOnRig();

    /** The scene the editor pane edits (its timing on the scene page). */
    void loadCanvas(quint32 sceneId);
    /** The stock editor of function @a fid in the editor pane. */
    void loadFunctionEditor(quint32 fid);
    void clearEditors();
    void retireOverviewEditor();
    /** Target rows / fixtures: their channels on the console. */
    void showFixtures(const QList<quint32> &fixtureIds);
    /** The programmer selection changed (anywhere): show it (Q201). */
    void followProgrammerSelection();
    /** Set the programmer selection, without following our own write. */
    void writeProgrammerSelection(const QList<quint32> &fixtureIds);
    bool m_writingSelection = false;
    bool sameFixtureType(const class Fixture *a, const class Fixture *b) const;

    /** + Scene (an existing one, picked), + new scene / EFX / RGB Matrix on
     *  the selected layer, remove, ▲ ▼, ▣ Open. */
    void addExisting();
    /** "Add existing…" once the picker has chosen: adds, or says why not on the hint line. */
    bool addExistingChosen(const QList<quint32> &ids);
    /** A bold line over the layers for a few seconds (a refusal, never silence). */
    void sayHint(const QString &text);
    quint32 addNew(Function::Type type);
    void removeSelectedLayers();
    void moveSelectedLayer(int delta);
    void openSelected();
    /** The selected layer (-1: none) and its groups' / fixtures' members. */
    int selectedLayer() const;

    QList<quint32> layerGroups(int layer) const;
    /** The heads of layer @a layer's targets: its groups' layouts, then
     *  every head of its fixed fixtures. */
    QList<class GroupHead> layerHeads(int layer) const;
    void selectLayer(int index);
    /** The stack's layers changed: show it, replay it. */
    void layersChanged();

    void startPreview();
    void stopPreview();
    /** A collection plays its members from its start: after its layers
     *  change it is stopped and started again (once the stop has landed). */
    void restartPreview();
    void startOperate();
    void stopOperate();
    QStringList effectSignature(Scene *scene) const;

    void recaptureBaseline();
    void recordSceneEdit(const QString &label, const QString &mergeKey = QString());

private:
    Doc *m_doc;
    WorkbenchStack *m_stack = nullptr;

    struct Back
    {
        QString label;
        QString selectedKey;
        std::function<WorkbenchStack *()> make;
    };
    QList<Back> m_back;
    QString m_returnLabel;
    QLabel *m_recordPointer = nullptr;   //!< the grey line pointing to the Show Tree's Record
    void updateRecordPointer();
    std::function<void()> m_returnTo;

    // Header
    QToolButton *m_backBtn = nullptr;
    QLabel *m_crumb = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_kind = nullptr;
    QLabel *m_info = nullptr;
    QLabel *m_onRig = nullptr;
    QToolButton *m_playBtn = nullptr;
    QWidget *m_lockedBanner = nullptr;
    class QVBoxLayout *m_headerLayout = nullptr;

    // Rows + add bar
    ProgrammingOutline *m_rows = nullptr;
    QLabel *m_rowsHint = nullptr;
    QString m_rowsWhat;                  //!< what the hint says when nothing is being said
    class QTimer *m_hintTimer = nullptr;       //!< puts the hint back after sayHint()
    QToolButton *m_addSceneBtn = nullptr;
    /** + Effect ▾ (Q94): one button for the QLC+ effects, as Build ▸
     *  Effects' + New ▾; its EFX and RGB Matrix are these actions. */
    QToolButton *m_addEffectBtn = nullptr;
    QAction *m_addEfxBtn = nullptr;
    QAction *m_addMatrixBtn = nullptr;
    QToolButton *m_upBtn = nullptr;
    QToolButton *m_downBtn = nullptr;
    QToolButton *m_removeBtn = nullptr;
    QToolButton *m_openBtn = nullptr;

    // The editor pane
    QSplitter *m_split = nullptr;
    QStackedWidget *m_page = nullptr;
    QWidget *m_emptyPage = nullptr;
    QLabel *m_emptyHint = nullptr;
    QWidget *m_overviewPage = nullptr;
    QLabel *m_overviewHeading = nullptr;
    QLabel *m_overviewText = nullptr;
    class QVBoxLayout *m_overviewLayout = nullptr;
    QWidget *m_overviewEditor = nullptr;     //!< a chaser's step table
    QWidget *m_scenePage = nullptr;
    QLabel *m_sceneHeading = nullptr;
    QLabel *m_sceneSummary = nullptr;
    QLabel *m_sceneWarnings = nullptr;
    class QVBoxLayout *m_sceneCanvasLayout = nullptr;
    QWidget *m_lookPage = nullptr;
    LookEditor *m_lookEditor = nullptr;
    /** editPalette() put the Look Editor over the page that was showing
     *  (its ↩ Back, Esc: the rows' page again). */
    bool m_paletteDetour = false;
    Confirm m_confirm;
    QWidget *m_targetPage = nullptr;
    QLabel *m_targetHeading = nullptr;
    QPushButton *m_targetLayoutBtn = nullptr;
    quint32 m_targetGroup = UINT_MAX;
    QScrollArea *m_fixtureScroll = nullptr;
    FixtureConsole *m_fixtureConsole = nullptr;
    QLabel *m_fixtureMixedNote = nullptr;
    QWidget *m_functionPage = nullptr;
    class QVBoxLayout *m_functionLayout = nullptr;
    QLabel *m_functionHeading = nullptr;
    QPushButton *m_groupBackButton = nullptr;
    quint32 m_groupReturnScene = UINT_MAX;

    SceneGroupLooks *m_canvas = nullptr;     //!< the scene page's timing
    QWidget *m_funcEditor = nullptr;         //!< a stock editor / group layout

    // State
    quint32 m_currentScene = Function::invalidId();
    quint32 m_editorFunction = Function::invalidId();
    quint32 m_previewFunction = Function::invalidId();
    quint32 m_operateFunction = Function::invalidId();
    quint32 m_trialScene = Function::invalidId();
    bool m_previewHeld = false;              //!< Stop pressed: don't replay until Play / a load
    bool m_heldByRun = false;                //!< RC8: a GO held the preview (Play brings it back)
    QList<quint32> m_selectedFixtures;
    quint32 m_selectedFixturesScene = UINT_MAX;
    QList<quint32> m_previewMembers;         //!< a collection's layers as last played
    QSharedPointer<Scene> m_editBaseline;
    QStringList m_lastSyncedEffects;
    bool m_refreshQueued = false;
    class QTimer *m_restartTimer = nullptr;
    int m_restartTries = 0;
    // W3
    /** A cue's layer or attribute row selected: its editor. */
    void showStackRow(const ProgrammingOutline::Row &row);
    /** Host @a pane, lent by the back end, on the function page. */
    void hostLentPane(QWidget *pane);
    void releaseLentPane();
    /** The back end's own preview, or a Solo layer that changed. */
    void syncStackPreview();
    QPointer<QWidget> m_lentPane;
    QToolButton *m_soloBtn = nullptr;
    bool m_ownPreview = false;          //!< the back end plays it (playsItself())
    static EffectPaneFactory s_effectPane;

    /** setStack()'s way with ↩: start over, push what was loaded, or pop. */
    enum History { Fresh, Push, Pop };
    void setStackImpl(WorkbenchStack *stack, History history);

    /************************************************************************
     * W2: a back end's own header widget (a chaser's step walker), the
     * layers replaced (a step walked to), a shared layer split off
     ************************************************************************/
private:
    void installStackWidget();
    void stackLayersReplaced();
    void makeOwnCopySelected();

    QPointer<QWidget> m_stackWidget;
    QToolButton *m_ownCopyBtn = nullptr;

    /** designEffect()'s designer, and whether it is the bench's. */
    QPointer<QWidget> m_effectPane;
    bool m_benchPane = false;
    /** The bench's EffectRequest::fromCue / fromScene, kept while it shows. */
    quint32 m_benchFromCue = 0xFFFFFFFF;
    quint32 m_benchFromScene = 0xFFFFFFFF;
    /** Host the factory's @a pane under @a heading. */
    void hostEffectPane(QWidget *pane, const QString &heading, bool bench);
    /************************************************************************
     * Encoder banks: a MIDI controller's encoders turn the selection
     * (EncoderBank, docs/features/encoder-banks.md)
     ************************************************************************/
public:
    /** What the encoders turn now: a look in the Look Editor, or the
     *  channels of the selected fixtures in a scene (the scene of the page,
     *  or of a layer picked in a Build stack). Both empty: nothing selected;
     *  @a why then says so when there is a reason (a Show Tree cue's layer is
     *  made from its looks, so a turn is made on a look, not on the layer). */
    struct EncoderTarget
    {
        LookEditor *look = nullptr;
        Scene *scene = nullptr;
        QList<quint32> fixtures;
        QString why;
        bool isValid() const { return look != nullptr || (scene != nullptr && !fixtures.isEmpty()); }
    };
    EncoderTarget encoderTarget() const;
    /** The line under the title that says what the encoders turn / why not
     *  (empty and hidden while the Encoder bank is off). */
    QString encoderLine() const;

private:
    void connectEncoders();
    void encoderAdjusted(int bank, int slot, int detents, bool fine);
    void encoderHomed(int bank, int slot);
    void encoderMessage(const QString &text);
    /** The rings and the line, as they are now (queued: one per event loop turn). */
    void scheduleEncoderRings();
    void refreshEncoderRings();
    QString encoderSlotLabel(const EncoderTarget &target, int bank, int slot) const;
    /** One undo step for the scene's raw edit; @a before is the scene as it was. */
    void afterEncoderEdit(const EncoderTarget &target, const QString &what,
                          const QSharedPointer<Scene> &before, int bank, int slot, bool home = false);
    void recordEncoderStep(Scene *scene, const QSharedPointer<Scene> &before, const QString &what, int bank,
                           int slot, bool home = false);

    QLabel *m_encoderLine = nullptr;
    QString m_encoderMessage;
    bool m_encoderRingsQueued = false;
    int m_stackLayer = -1;      //!< the layer showStackRow() last showed

    QToolButton *m_sendToTreeBtn = nullptr;
    /** The header's Focus ▾ (board Q149). */
    QToolButton *m_refineBtn = nullptr;
    void fillFocusMenu(QMenu *menu);
};

/** @} */

#endif

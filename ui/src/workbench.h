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
 * The Workbench (WORKFLOW_DESIGN.md §3.4): one editor for "a stack of
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
 * and W3 docks the same widget in the Show Tree with a CueStack loaded
 * (setStack(), setReturnTo()).
 */
class Workbench final : public QWidget
{
    Q_OBJECT
    friend class HelpShots;

public:
    Workbench(Doc *doc, QWidget *parent = nullptr);
    ~Workbench();

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

    /** ↩: back to what was loaded before (or to setReturnTo()'s place). */
    bool goBack();
    bool canGoBack() const;
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

protected:
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;

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
    /** Target rows / fixtures: their channels on the console. */
    void showFixtures(const QList<quint32> &fixtureIds);
    bool sameFixtureType(const class Fixture *a, const class Fixture *b) const;

    /** + Scene (an existing one, picked), + new scene / EFX / RGB Matrix on
     *  the selected layer, remove, ▲ ▼, ▣ Open. */
    void addExisting();
    void addNew(Function::Type type);
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
    QToolButton *m_addSceneBtn = nullptr;
    QPushButton *m_addEfxBtn = nullptr;
    QPushButton *m_addMatrixBtn = nullptr;
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
    QList<quint32> m_selectedFixtures;
    quint32 m_selectedFixturesScene = UINT_MAX;
    QList<quint32> m_previewMembers;         //!< a collection's layers as last played
    QSharedPointer<Scene> m_editBaseline;
    QStringList m_lastSyncedEffects;
    bool m_refreshQueued = false;
    class QTimer *m_restartTimer = nullptr;
    int m_restartTries = 0;
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
    QToolButton *m_sendToTreeBtn = nullptr;
};

/** @} */

#endif

/*
  Q Light Controller
  functionmanager.h

  Copyright (C) Heikki Junnila

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

#ifndef FUNCTIONMANAGER_H
#define FUNCTIONMANAGER_H

#include <QWidget>
#include <QList>

#include "function.h"
#include "doc.h"

class FunctionsTreeWidget;
class QTreeWidgetItem;
class QSplitter;
class QToolBar;
class QToolButton;
class QAction;
class Fixture;
class QMenu;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/** QLC+'s Function Manager: the Functions window. Hidden by default (board
 *  Q32) -- App::setFunctionsWindowShown() is the one switch. The Classic QLC+
 *  and Everything views show it; Window → Functions switches to one of them
 *  from the qlcconsole view (Q73: "Classic" means stock QLC+ only). */
class FunctionManager final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(FunctionManager)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    FunctionManager(QWidget* parent, Doc* doc);
    ~FunctionManager();

    /** Get the singleton instance */
    static FunctionManager* instance();

    /** Match the main window's icon/text display preference
     *  (workspace/tabLabelMode) — mirrors Monitor::applyToolbarLabelMode(). */
    void applyToolbarLabelMode();
    /** Classic QLC+ view (Q78): the stock QLC+ toolbar -- one button per
     *  new function type and a folder -- instead of the Add dropdown. */
    void setClassicToolbar(bool classic);

    /** Classic QLC+ view: picking one function in the tree opens its editor,
     *  as in QLC+; elsewhere a double-click does (a single click selects,
     *  to drag it into the open collection or chaser). */
    void setOpenOnSelect(bool open);

    /** "Show the Show Tree's own functions" (the tree's right-click menu,
     *  functionmanager/showShowTreeFunctions): off, Classic QLC+ lists none
     *  of the functions the Show Tree generates or keeps (they are edited in
     *  the Show Tree), and the other views list what is visible, as QLC+
     *  does; on, every view lists all of them. */
    void setShowTreeFunctionsShown(bool shown);
    bool showTreeFunctionsShown() const;

signals:
    /** Emitted when the FunctionManager's tab is de/activated */
    void functionManagerActive(bool active);

protected:
    /** @reimp */
    void showEvent(QShowEvent* ev) override;

    /** @reimp */
    void hideEvent(QHideEvent* ev) override;

protected slots:
    void slotModeChanged();
    void slotDocClearing();
    void slotDocLoading();
    void slotDocLoaded();
    void slotFunctionNameChanged(quint32 id);
    void slotFunctionAdded(quint32 id);
    void slotPaletteAdded(quint32 id);
    void slotPaletteRemoved(quint32 id);

protected:
    static FunctionManager* s_instance;
    Doc* m_doc;

    /*********************************************************************
     * Function tree
     *********************************************************************/
public:

    /** Select the function with the given ID */
    void selectFunction(quint32 id);

private:
    /** Init the splitter view */
    void initSplitterView();

    /** Init function tree view */
    void initTree();

    /** Rebuild the tree, keeping what is selected (and so the editor). */
    void refreshTree();
    /** The tree's ShowTreeFunctions for the view and the setting above. */
    void applyShowTreeFilter();

    /** Delete all currently selected functions */
    void deleteSelectedFunctions();

private slots:
    /** The tree's selection changed: the actions follow it, and with
     *  setOpenOnSelect() one picked function opens in the editor pane. */
    void slotTreeSelectionChanged();

    /** Function was double-clicked in the tree */
    void slotTreeItemDoubleClicked(QTreeWidgetItem* item);

    /** Right mouse button was clicked on function tree */
    void slotTreeContextMenuRequested();

private:
    QSplitter* m_hsplitter;
    QSplitter* m_vsplitter;
    FunctionsTreeWidget* m_tree;
    bool m_classic = false;
    QAction *m_showTreeFunctionsAction = nullptr;
    QTimer *m_refreshTimer = nullptr;

    /*********************************************************************
     * Menus, toolbar & actions
     *********************************************************************/
protected:
    void initActions();
    void initToolbar();

protected slots:
    void slotAddScene();
    void slotAddChaser();
    void slotAddSequence();
    void slotAddCollection();
    void slotAddEFX();
    void slotAddRGBMatrix();
    void slotAddScript();
    void slotAddAudio();
    void slotAddVideo();
    void slotAddFolder();

    void slotSelectAutostartFunction();
    void slotWizard();
    void slotPaletteManager();
    void slotAddPalette();

    void slotClone();
    void slotDelete();
    void slotSelectAll();

protected slots:
    /** Enable the actions for what's selected; a slot, since the tree's
     *  selection changes reach it by name (it never did as a method). */
    void updateActionStatus();

protected:
    QToolBar* m_toolbar;
    QToolButton* m_addButton;   //!< "Add" dropdown consolidating New Scene/Chaser/…/Folder
    QAction* m_addButtonAction = nullptr;
    /** The stock toolbar's buttons (Q78), shown only in Classic QLC+. */
    QList<QAction *> m_classicToolbarActions;

    QAction* m_addSceneAction;
    QAction* m_addChaserAction;
    QAction* m_addSequenceAction;
    QAction* m_addCollectionAction;
    QAction* m_addEFXAction;
    QAction* m_addRGBMatrixAction;
    QAction* m_addScriptAction;
    QAction* m_addAudioAction;
    QAction* m_addVideoAction;

    QAction* m_autostartAction;
    QAction* m_wizardAction;
    QAction* m_paletteManagerAction;
    QAction* m_addPaletteAction;
    QAction* m_addFolderAction;
    QAction* m_cloneAction;
    QAction* m_deleteAction;
    QAction* m_selectAllAction;

    /*********************************************************************
     * Helpers
     *********************************************************************/
private:
    /** Get the folder path based on current selection */
    QString getSelectedFolderPath();

    /** Create a copy of the given function */
    void copyFunction(quint32 fid);

    /** Create a copy of the given palette */
    void copyPalette(quint32 pid);

    /** Open an editor for the given function */
    void editFunction(Function* function);

    /** Delete current editor. Can be synchronous. */
    void deleteCurrentEditor(bool async = true);

    /** Highlight the currently edited function in the tree */
    void highlightEditedFunction(quint32 fid);

    /** Clear the highlight from the previously edited function */
    void clearEditedFunctionHighlight();

    /** Show the editor pane at a usable width: a saved splitter state that
     *  collapsed it (or the tree) would leave the editor a dead strip. */
    void showEditorPane(QWidget *pane);

private:
    QWidget* m_editor;
    QWidget* m_scene_editor;
    /** Above a Show Tree function's editor: why it can't be changed here */
    QWidget* m_lockedBanner;
    quint32 m_editedFunctionId;  // ID of the currently edited function
    /** The edited row's own text colour, put back with its highlight */
    QVariant m_editedForeground;
    bool m_openOnSelect;
};

/** @} */

#endif

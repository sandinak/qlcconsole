/*
  Q Light Controller Plus
  programmingmanager.h

  Fork-owned top-level "Programming" tab: a drag-drop authoring surface.
  Three panes:
    - left:   a function tree (folders) of what you build; select a scene
              to edit it (chaser/collection canvases come later)
    - center: the selected scene's canvas (reuses SceneGroupLooks)
    - right:  a Palettes tree + a Fixtures & Groups tree, dragged into the
              canvas (palette -> look, group -> dynamic target, fixture ->
              fixed target)

  It edits plain Scenes, so everything stays compatible with the classic
  Function Manager / scene editor.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PROGRAMMINGMANAGER_H
#define PROGRAMMINGMANAGER_H

#include <QWidget>
#include <QSet>

class QTreeWidgetItem;
class Function;
class FunctionsTreeWidget;
class FixtureGroupSource;
class Fixture;
class SceneGroupLooks;
class LookEditor;
class FixtureConsole;
class QScrollArea;
class QVBoxLayout;
class QLabel;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

class ProgrammingManager final : public QWidget
{
    Q_OBJECT

public:
    ProgrammingManager(QWidget *parent, Doc *doc);
    ~ProgrammingManager();

protected:
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;

private slots:
    void slotFunctionActivated(QTreeWidgetItem *item);
    void slotFuncTreeMenu(const QPoint &pos);
    void slotPaletteTreeMenu(const QPoint &pos);
    void slotCanvasModified();
    void slotModeChanged();
    void slotLookEdited();
    void slotCopy();
    void slotPaste();
    void slotPaletteDoubleClicked(QTreeWidgetItem *item);
    void slotFixturesSelected(const QList<quint32> &fixtureIds);
    void slotFixtureValueChanged(quint32 fxi, quint32 ch, uchar value);
    void slotFixtureChecked(quint32 fxi, quint32 ch, bool state);

private:
    void loadCanvas(quint32 sceneId);
    /** Host the stock editor for a non-scene function in the canvas. */
    void loadFunctionEditor(Function *function);
    /** Tear down whichever canvas editor is currently shown. */
    void clearEditors();
    void updateTitle();

    /** True if two fixtures share a channel layout (same def+mode, or both
     *  generic with equal channel counts) and so can be edited together. */
    bool sameFixtureType(const Fixture *a, const Fixture *b) const;

    /** Show the look editor in the bottom panel (pivoting away from the
     *  per-fixture DMX console if it's showing). */
    void showLookEditorPanel();

    /** Create a new palette of the given QLCPalette::PaletteType in the
     *  selected palette folder and open it in the look editor. */
    void createPalette(int paletteType);

    /** Duplicate palette $pid ("… (copy)"), add it, and open it for editing. */
    void duplicatePalette(quint32 pid);

    /** Strip leading "NN. " step-number prefixes accidentally baked into
     *  function names by an earlier nesting bug. */
    void repairFunctionNames();

    /** Folder path of the current func-tree selection (for new functions). */
    QString selectedFuncFolderPath() const;
    /** How many chasers/collections reference this function. */
    int functionUsageCount(quint32 fid) const;

    /** Show the members/steps of the given collection/chaser nested under
     *  its node in the function tree (read-only nav; invalidId clears). */
    void syncMemberNodes(quint32 containerId);
    /** Recursively nest the members of a container function under treeNode
     *  (collections-in-chasers-in-… ); visited guards against cycles. */
    void addMemberChildren(QTreeWidgetItem *treeNode, quint32 containerId,
                           QSet<quint32> &visited, int depth);

    /** Is fid anywhere in the container's tree (recursively)? */
    bool isContainerMember(quint32 containerId, quint32 fid) const;
    bool containerHas(quint32 containerId, quint32 fid,
                      QSet<quint32> &visited, int depth) const;

    /** Live-preview the canvas scene by running it (Design mode only), so
     *  the DMX/2D view reflects the look as you build. Blind/live handling
     *  comes later. */
    void startPreview();
    void stopPreview();
    /** Re-apply the running preview after an edit WITHOUT churning
     *  start/stop (resetRuntime if running; start if not). */
    void refreshPreview();

private:
    Doc *m_doc;

    FunctionsTreeWidget *m_funcTree;

    QVBoxLayout *m_canvasLayout;
    QLabel *m_canvasTitle;
    QLabel *m_canvasPlaceholder;
    SceneGroupLooks *m_canvas;
    QWidget *m_funcEditor;     //!< stock editor for non-scene functions
    LookEditor *m_lookEditor;      //!< bottom pane; sizes to content (no scroll)
    QScrollArea *m_fixtureScroll;  //!< wraps the per-fixture console (bottom)
    FixtureConsole *m_fixtureConsole;
    QLabel *m_fixtureMixedNote;    //!< shown when selected fixed fixtures differ in type
    QList<quint32> m_selectedFixtures; //!< fixed fixtures edited together (same type)
    quint32 m_currentScene;       //!< scene shown in canvas (invalid if non-scene)
    quint32 m_canvasFunction;     //!< any function shown in canvas (preview target)
    quint32 m_previewFunction;    //!< the function we started for live preview
    quint32 m_clipboardFunction;  //!< Cmd-C source for Cmd-V duplicate
    quint32 m_clipboardPalette;   //!< Cmd-C source palette for Cmd-V duplicate
    quint32 m_memberContainer;    //!< collection/chaser whose members are nested

    FunctionsTreeWidget *m_paletteTree;
    FixtureGroupSource *m_fixGroupSource;
};

/** @} */

#endif

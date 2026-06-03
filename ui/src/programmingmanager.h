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

class QTreeWidgetItem;
class Function;
class FunctionsTreeWidget;
class FixtureGroupSource;
class SceneGroupLooks;
class LookEditor;
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

private:
    void loadCanvas(quint32 sceneId);
    /** Host the stock editor for a non-scene function in the canvas. */
    void loadFunctionEditor(Function *function);
    /** Tear down whichever canvas editor is currently shown. */
    void clearEditors();
    void updateTitle();

    /** Create a new palette of the given QLCPalette::PaletteType in the
     *  selected palette folder and open it in the look editor. */
    void createPalette(int paletteType);

    /** Folder path of the current func-tree selection (for new functions). */
    QString selectedFuncFolderPath() const;
    /** How many chasers/collections reference this function. */
    int functionUsageCount(quint32 fid) const;

    /** Show the members/steps of the given collection/chaser nested under
     *  its node in the function tree (read-only nav; invalidId clears). */
    void syncMemberNodes(quint32 containerId);

    /** Live-preview the canvas scene by running it (Design mode only), so
     *  the DMX/2D view reflects the look as you build. Blind/live handling
     *  comes later. */
    void startPreview();
    void stopPreview();

private:
    Doc *m_doc;

    FunctionsTreeWidget *m_funcTree;

    QVBoxLayout *m_canvasLayout;
    QLabel *m_canvasTitle;
    QLabel *m_canvasPlaceholder;
    SceneGroupLooks *m_canvas;
    QWidget *m_funcEditor;     //!< stock editor for non-scene functions
    LookEditor *m_lookEditor;
    quint32 m_currentScene;
    quint32 m_previewScene;       //!< the scene we started for live preview
    quint32 m_clipboardFunction;  //!< Cmd-C source for Cmd-V duplicate
    quint32 m_memberContainer;    //!< collection/chaser whose members are nested

    FunctionsTreeWidget *m_paletteTree;
    FixtureGroupSource *m_fixGroupSource;
};

/** @} */

#endif

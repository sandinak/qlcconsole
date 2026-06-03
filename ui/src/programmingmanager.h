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
    void slotFunctionSelected();
    void slotNewScene();
    void slotCanvasModified();
    void slotModeChanged();
    void slotLookEdited();

private:
    void loadCanvas(quint32 sceneId);
    void updateTitle();

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
    LookEditor *m_lookEditor;
    quint32 m_currentScene;
    quint32 m_previewScene; //!< the scene we started for live preview

    FunctionsTreeWidget *m_paletteTree;
    FixtureGroupSource *m_fixGroupSource;
};

/** @} */

#endif

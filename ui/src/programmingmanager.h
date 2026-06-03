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

private slots:
    void slotFunctionSelected();
    void slotNewScene();

private:
    void loadCanvas(quint32 sceneId);

private:
    Doc *m_doc;

    FunctionsTreeWidget *m_funcTree;

    QVBoxLayout *m_canvasLayout;
    QLabel *m_canvasPlaceholder;
    SceneGroupLooks *m_canvas;
    quint32 m_currentScene;

    FunctionsTreeWidget *m_paletteTree;
    FixtureGroupSource *m_fixGroupSource;
};

/** @} */

#endif

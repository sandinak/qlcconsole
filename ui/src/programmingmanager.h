/*
  Q Light Controller Plus
  programmingmanager.h

  Fork-owned top-level "Programming" tab: a drag-drop authoring surface for
  building scenes from palettes (looks) and fixture groups. Three panes:
    - left:   the scenes you're building (select to edit, or create new)
    - center: the selected scene's canvas (reuses SceneGroupLooks)
    - right:  searchable Palettes + Fixture Groups, dragged into the canvas

  It edits plain Scenes, so everything stays compatible with the classic
  Function Manager / scene editor.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PROGRAMMINGMANAGER_H
#define PROGRAMMINGMANAGER_H

#include <QWidget>

class QListWidget;
class QLineEdit;
class QVBoxLayout;
class QLabel;
class SceneGroupLooks;
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
    void slotReloadScenes();
    void slotSceneSelected();
    void slotNewScene();
    void slotReloadSources();
    void slotPaletteFilter(const QString &text);
    void slotGroupFilter(const QString &text);

private:
    void loadCanvas(quint32 sceneId);

private:
    Doc *m_doc;

    QListWidget *m_sceneList;

    QVBoxLayout *m_canvasLayout;
    QLabel *m_canvasPlaceholder;
    SceneGroupLooks *m_canvas;
    quint32 m_currentScene;

    QLineEdit *m_paletteFilter;
    QListWidget *m_paletteList;
    QLineEdit *m_groupFilter;
    QListWidget *m_groupList;
};

/** @} */

#endif

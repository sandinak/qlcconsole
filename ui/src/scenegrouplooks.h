/*
  Q Light Controller Plus
  scenegrouplooks.h

  Fork-owned widget: lets a Scene target fixture GROUPS with dynamic
  palette "looks" (Color/Dimmer/Pan-Tilt/Gobo/Shutter) that follow group
  membership at run time. Embedded into the classic Scene Editor's
  General tab. Kept in its own file so the upstream sceneeditor.* diff
  stays a tiny hook (cherry-pick friendliness).

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef SCENEGROUPLOOKS_H
#define SCENEGROUPLOOKS_H

#include <QWidget>

class QListWidget;
class QPushButton;
class Scene;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * Two small lists inside the Scene Editor's General tab:
 *   - Target groups: the fixture groups this scene applies its looks to.
 *   - Looks: QLCPalettes (color/dimmer/position/gobo/shutter) the scene
 *     asserts. Per the engine, every look applies to every target group
 *     (and to the scene's individual fixtures) — so one scene is "these
 *     looks on these groups". Different looks per group => separate scenes.
 *
 * This is a set-and-attach surface, not a live-preview editor: a look
 * takes effect when the scene runs. Authoring/live-tweak of group looks
 * happens in the VC programmer; this is the classic-editor companion so
 * group scenes are visible and editable here instead of opaque.
 */
class SceneGroupLooks final : public QWidget
{
    Q_OBJECT

public:
    SceneGroupLooks(Scene *scene, Doc *doc, QWidget *parent = nullptr);
    ~SceneGroupLooks();

    /** Repopulate both lists from the scene's current groups/palettes. */
    void reload();

protected:
    /** Accept palettes dragged from the Function Manager tree. */
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void slotAddGroup();
    void slotRemoveGroup();
    void slotAddLook();
    void slotRemoveLook();

private:
    /** Human-readable one-liner for a palette (type + value). */
    QString lookLabel(quint32 paletteId) const;

private:
    Scene *m_scene;
    Doc *m_doc;

    QListWidget *m_groupList;
    QListWidget *m_lookList;
    QPushButton *m_addGroupButton;
    QPushButton *m_removeGroupButton;
    QPushButton *m_addLookButton;
    QPushButton *m_removeLookButton;
};

/** @} */

#endif

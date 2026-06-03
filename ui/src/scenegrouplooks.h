/*
  Q Light Controller Plus
  scenegrouplooks.h

  Fork-owned widget: a Scene's LOOKS (palettes) and TARGETS (fixture
  groups = dynamic, individual fixtures = fixed). Add by dragging from the
  Programming tab's sources (or the scene editor); remove with the buttons.
  Embedded in the classic Scene Editor's General tab and in the Programming
  tab's canvas. Kept in its own file so the upstream sceneeditor.* diff
  stays a tiny hook (cherry-pick friendliness).

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef SCENEGROUPLOOKS_H
#define SCENEGROUPLOOKS_H

#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class Scene;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * Two lists describing what a scene asserts:
 *   - Targets: fixture groups (dynamic — follow membership at run time)
 *     and/or individual fixtures (fixed membership). The scene's looks
 *     apply to every target.
 *   - Looks: QLCPalettes the scene asserts on its targets.
 *
 * Items are added by dropping palettes / groups / fixtures dragged from
 * the sources; routed by MIME type. Emits sceneModified() after any change
 * so a host (e.g. the Programming tab) can refresh a live preview.
 */
class SceneGroupLooks final : public QWidget
{
    Q_OBJECT

public:
    /** @param includeFixtureTargets  when true, individual fixtures are
     *  listed in (and accepted into) Targets. The classic scene editor
     *  leaves it false — it shows fixtures in its own tree — while the
     *  Programming tab sets it true. */
    SceneGroupLooks(Scene *scene, Doc *doc, QWidget *parent = nullptr,
                    bool includeFixtureTargets = false);
    ~SceneGroupLooks();

    /** Repopulate the lists from the scene's current targets/looks. */
    void reload();

signals:
    /** Emitted after the scene's targets/looks were changed here. */
    void sceneModified();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void slotRemoveTarget();
    void slotRemoveLook();

private:
    QString lookLabel(quint32 paletteId) const;

private:
    Scene *m_scene;
    Doc *m_doc;
    bool m_includeFixtureTargets;

    QListWidget *m_targetList;
    QListWidget *m_lookList;
    QPushButton *m_removeTargetButton;
    QPushButton *m_removeLookButton;
};

/** @} */

#endif

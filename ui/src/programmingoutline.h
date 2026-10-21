/*
  Q Light Controller Plus
  programmingoutline.h

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

#ifndef PROGRAMMINGOUTLINE_H
#define PROGRAMMINGOUTLINE_H

#include <QTreeWidget>
#include <QVariantMap>
#include <QList>
#include <QSet>

class QMimeData;
class QTimer;
class Scene;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * The Programming tab's outline (PROGRAMMING_REDESIGN.md P1): every scene
 * folder -> its scenes -> each scene's targets (groups, fixed fixtures) and
 * looks (palettes, an effect's own colour/dimmer nested under it), in one
 * tree, so scenes no longer open one at a time.
 *
 * Click a row and the tab's page edits it. Rows drag between scenes (move;
 * with Option held, copy) and within one (reorder the looks); palettes,
 * groups and fixtures drag in from the sources; scenes drag into folders.
 * Every edit made here is one step on the shared undo history
 * (PaletteOps::sceneStep()), so it undoes and redoes from either tab.
 *
 * A view of what the show already holds: no file format of its own.
 */
class ProgrammingOutline final : public QTreeWidget
{
    Q_OBJECT

public:
    enum Kind
    {
        FolderRow = 1,
        SceneRow,
        OtherRow,       //!< a chaser / collection / ... in a scene folder (greyed)
        GroupRow,       //!< a dynamic target
        FixtureRow,     //!< a fixed target
        PaletteRow,     //!< a look
        HintRow         //!< the empty state
    };
    enum Role
    {
        KindRole = Qt::UserRole + 20,
        IdRole,         //!< function / group / fixture / palette id
        SceneRole,      //!< the scene a target or look row belongs to
        KeyRole,        //!< stable key, for expansion and selection
        PathRole        //!< a folder row's path
    };
    enum Column { NameCol = 0, BadgeCol = 1 };

    /** A row as the edits take it. */
    struct Row
    {
        int kind = 0;
        quint32 scene = 0xFFFFFFFF;
        quint32 id = 0xFFFFFFFF;
        bool operator==(const Row &o) const
        { return kind == o.kind && scene == o.scene && id == o.id; }
    };

    ProgrammingOutline(Doc *doc, QWidget *parent = nullptr);
    ~ProgrammingOutline();

    /** The drag payload of outline rows. */
    static const char *rowMimeType();

    /** Rebuild now (keeps expansion, selection and scroll). */
    void rebuild();
    /** Rebuild soon, once for a burst of changes. */
    void scheduleRebuild();

    void setFilterText(const QString &text);
    /** Expand / collapse every row (⌘→ / ⌘←). */
    void expandEverything();
    void collapseEverything();
    /** Open or close @a item and everything under it (⌥-click a triangle). */
    void setSubtreeExpanded(QTreeWidgetItem *item, bool expanded);

    /** The row of @a row, or null. */
    QTreeWidgetItem *itemFor(const Row &row) const;
    QTreeWidgetItem *sceneItem(quint32 sceneId) const;
    /** Make @a row the one selected row (opening what holds it); false: no
     *  such row. Emits itemSelectionChanged(). */
    bool selectRow(const Row &row);

    static Row rowOf(const QTreeWidgetItem *item);
    QList<Row> selectedRows() const;
    /** The folder a new scene made from @a item goes in ("" the top). */
    QString folderFor(const QTreeWidgetItem *item) const;

    /************************************************************************
     * Edits: each one step on the shared history
     ************************************************************************/
    /** Move (or with @a copy, copy) target and look rows into scene
     *  @a toScene. Looks go before look @a atPalette, or nested under it if
     *  it is an effect and they are not; with no @a atPalette they become
     *  base looks (an effect and its own go at the end). Rows of
     *  @a toScene itself reorder its looks. */
    bool moveRows(const QList<Row> &rows, quint32 toScene, quint32 atPalette, bool copy);
    /** Add what a drag from the sources carries (palettes, groups, fixtures)
     *  to @a toScene, looks placed as moveRows() places them. */
    bool addFromSources(const QMimeData *mime, quint32 toScene, quint32 atPalette);
    /** Take target and look rows out of their scenes. */
    bool removeRows(const QList<Row> &rows);
    /** Move look @a row one place up (-1) or down (+1) in its scene. */
    bool shiftLook(const Row &row, int delta);
    /** File scenes @a sceneIds in @a folder. */
    bool fileScenes(const QList<quint32> &sceneIds, const QString &folder);
    bool renameScene(quint32 sceneId, const QString &name);
    /** Rename folder @a path's last part to @a name: every function filed in
     *  it or under it follows. */
    bool renameFolder(const QString &path, const QString &name);

    /** What dropping @a mime at @a pos would do (empty: nothing), and how
     *  (@a toScene, @a atPalette, @a folder), as the drop takes it. */
    QString dropPlan(const QMimeData *mime, const QPoint &pos, bool copy,
                     quint32 *toScene = nullptr, quint32 *atPalette = nullptr,
                     QString *folder = nullptr) const;

    /** The badge column's width (dragged, remembered). */
    int badgeWidth() const { return m_badgeWidth; }

signals:
    /** Return on a row: go to its page. */
    void pageRequested();
    /** What a drag over the outline would do, or a refusal ("" clears). */
    void hintChanged(const QString &text);
    /** Scenes an edit here changed (the tab re-plays the one it shows). */
    void scenesEdited(const QList<quint32> &sceneIds);
    void newSceneRequested(const QString &folder);
    void duplicateRequested(const QList<quint32> &sceneIds);
    void deleteScenesRequested(const QList<quint32> &sceneIds);
    void openInFunctionsRequested(quint32 functionId);
    void editGroupLayoutRequested(quint32 groupId);
    void revealPaletteRequested(quint32 paletteId);

protected:
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override;
    QStringList mimeTypes() const override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void slotContextMenu(const QPoint &pos);
    void slotItemChanged(QTreeWidgetItem *item, int column);

private:
    QString key(const QTreeWidgetItem *item) const;
    void addSceneRows(QTreeWidgetItem *sceneItem, Scene *scene);
    void decorateScene(QTreeWidgetItem *item, Scene *scene);
    QTreeWidgetItem *folderItem(const QString &path, QHash<QString, QTreeWidgetItem *> &folders);
    bool defaultExpanded(const QTreeWidgetItem *item) const;
    void applyFilter();
    void fitColumns();
    void saveExpansion();
    QString settingsKey() const;
    /** The look rows @a rows stand for in their scene's order, an effect
     *  row bringing its own (nested) looks. */
    QList<quint32> lookIdsOf(const QList<Row> &rows, quint32 sceneId) const;
    /** Rows (or their keys) to select once the next rebuild is done. */
    void selectKeys(const QStringList &keys);
    /** Edits done here: rebuild now, select @a keys, say which scenes. */
    void finishEdit(const QList<quint32> &sceneIds, const QStringList &keys = QStringList());

    Doc *m_doc;
    QTimer *m_rebuildTimer;
    bool m_building = false;
    bool m_dirty = true;
    QString m_filter;
    QVariantMap m_expanded;      //!< row key -> open, as last left
    int m_badgeWidth = 0;
    bool m_fittingColumns = false;
};

/** @} */

#endif

/*
  Q Light Controller Plus
  fixturegroupsource.h

  Fork-owned drag-source tree of Fixture Groups and Fixtures, docked next
  to the Scene editor. It is a SOURCE only: items are dragged out onto the
  scene's drop zones (dynamic group targets / static fixture console).
  Creating/editing fixtures and groups stays in the Fixture Manager; this
  tree mirrors them and refreshes off the Doc's change signals.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef FIXTUREGROUPSOURCE_H
#define FIXTUREGROUPSOURCE_H

#include <QTreeWidget>
#include <QHash>

class QTreeWidgetItem;
class Fixture;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

class FixtureGroupSource final : public QTreeWidget
{
    Q_OBJECT

public:
    FixtureGroupSource(Doc *doc, QWidget *parent = nullptr);

    /** MIME type carrying a stream of fixture-group IDs (quint32). */
    static const char* fixtureGroupMimeType();

    /** MIME type carrying a stream of fixture IDs (quint32). */
    static const char* fixtureMimeType();

    /** Item-data roles / node kinds. */
    enum { IdRole = Qt::UserRole, KindRole = Qt::UserRole + 1,
           PathRole = Qt::UserRole + 2 /*!< folder path (string) */ };
    enum NodeKind { CategoryNode = 0, GroupNode = 1, FixtureNode = 2 };

public slots:
    /** Rebuild the tree from the Doc's fixtures and groups. */
    void reload();

signals:
    /** A group row was double-clicked (host may visualize/edit it). */
    void groupDoubleClicked(quint32 groupId);

protected:
    /** Provide group/fixture IDs for dragging onto scene drop zones. */
    QMimeData* mimeData(const QList<QTreeWidgetItem*> &items) const
        { return buildMimeData(items); }
    QMimeData* mimeData(const QList<QTreeWidgetItem*> items) const
        { return buildMimeData(items); }
    /** Enter/Return on a selected group row opens the rename prompt. */
    void keyPressEvent(QKeyEvent *event) override;
private:
    QMimeData* buildMimeData(const QList<QTreeWidgetItem*> &items) const;

    /** Prompt to rename group @p groupId (shared by the context menu + Enter). */
    void renameGroupPrompt(quint32 groupId);

    /** Accept group drops onto folders (re-file groups); ignore otherwise. */
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    /** Right-click: move a group into a folder, or create a new fixture group
     *  from the selected fixtures. */
    void slotContextMenu(const QPoint &pos);

    /** Emit groupDoubleClicked when a group row is double-clicked. */
    void slotItemDoubleClicked(QTreeWidgetItem *item, int column);

private:
    /** Create a new FixtureGroup from the given fixtures (name/size prompted
     *  via CreateFixtureGroup), assigning each fixture's heads into the grid. */
    void createGroupFromFixtures(const QList<quint32> &fixtureIds);

    /** Get/create the folder item for a "/"-separated path (empty=root). */
    QTreeWidgetItem *folderItem(const QString &path);

    /** Append a draggable fixture leaf under parent. */
    void addFixtureLeaf(QTreeWidgetItem *parent, Fixture *f);

private:
    Doc *m_doc;
    QHash<QString, QTreeWidgetItem*> m_folderMap; //!< rebuilt each reload()
    bool m_clearing = false;  //!< true while the Doc is clearing/tearing down
};

/** @} */

#endif

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

protected:
    /** Provide group/fixture IDs for dragging onto scene drop zones. */
    QMimeData* mimeData(const QList<QTreeWidgetItem*> &items) const override;

    /** Accept group drops onto folders (re-file groups); ignore otherwise. */
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    /** Right-click: move a group into a folder. */
    void slotContextMenu(const QPoint &pos);

private:
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

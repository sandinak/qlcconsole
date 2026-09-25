/*
  Q Light Controller Plus
  monitorinventorypanel.h

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

#ifndef MONITORINVENTORYPANEL_H
#define MONITORINVENTORYPANEL_H

#include <QTreeWidget>
#include <QWidget>

class QMimeData;
class QLineEdit;
class QLabel;
class Doc;
class MonitorGraphicsView;

/** @addtogroup ui_mon DMX Monitor
 * @{
 */

/** The tree inside the panel. Drags out the ids of the fixtures under the
 *  selection as "application/x-qlc-fid" -- the format the plot and the
 *  Structure Studio already accept. */
class InventoryTree : public QTreeWidget
{
    Q_OBJECT
public:
    explicit InventoryTree(QWidget *parent = NULL);

    /** Fixture ids under the current selection (a model node stands for all of its fixtures). */
    QList<quint32> selectedFixtureIds() const;

    /** The drag payload for @p ids: the application/x-qlc-fid list the plot
     *  accepts on drop. Caller owns the result. */
    static QMimeData *fixtureMime(const QList<quint32> &ids);

protected:
    // startDrag() rather than mimeData(): mimeData()'s signature differs between
    // Qt5 (list by value) and Qt6 (const reference), so overriding it forks the build.
    void startDrag(Qt::DropActions supportedActions) override;
};

/**
 * The Studio's Inventory panel, docked on the right of the plot: every fixture
 * staged in the show but not yet placed, grouped by model, ready to be dragged
 * onto the plot -- straight onto a truss, riser or tower if that is where it
 * belongs. The left-hand Layers panel is where placed things live; this is
 * where the unplaced ones wait.
 */
class MonitorInventoryPanel : public QWidget
{
    Q_OBJECT

public:
    MonitorInventoryPanel(Doc *doc, MonitorGraphicsView *view, QWidget *parent = NULL);

    /** Rebuild from the Doc: unpatched fixtures that are not on the plot yet. */
    void reload();

    InventoryTree *tree() const { return m_tree; }

signals:
    /** "Add to inventory..." was pressed. */
    void addRequested();
    /** The panel's own close button. */
    void closeRequested();

private:
    Doc *m_doc;
    MonitorGraphicsView *m_view;
    InventoryTree *m_tree;
    QLineEdit *m_filter;
    QLabel *m_count;
};

/** @} */

#endif

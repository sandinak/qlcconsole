/*
  Q Light Controller - Fixture Definition Editor
  fixturebrowser.h

  Copyright (c) Branson Matheson

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

#ifndef FIXTUREBROWSER_H
#define FIXTUREBROWSER_H

#include <QWidget>

class QLCFixtureDefCache;
class QTreeWidgetItem;
class QTreeWidget;
class QLineEdit;

/** @addtogroup fixtureeditor Fixture Editor
 * @{
 */

/**
 * Side panel listing every fixture definition found on disk (system +
 * user definition directories), grouped by manufacturer, filterable by a
 * manufacturer-or-model substring search. Double-clicking (or activating)
 * a model emits its .qxf/.d4 path so App can open it the same way File >
 * Open does.
 */
class FixtureBrowser final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(FixtureBrowser)

public:
    explicit FixtureBrowser(QWidget* parent = 0);
    ~FixtureBrowser();

signals:
    /** A definition was picked (double-click / Enter) — @p path is the
     *  absolute file path of the .qxf/.d4 it was loaded from. */
    void definitionActivated(const QString& path);

private slots:
    void slotSearchChanged(const QString& text);
    void slotItemActivated(QTreeWidgetItem* item, int column);

private:
    /** (Re)builds the tree from m_cache, applying the current search text. */
    void fillTree();

    QLCFixtureDefCache* m_cache;
    QLineEdit* m_search;
    QTreeWidget* m_tree;
};

/** @} */

#endif

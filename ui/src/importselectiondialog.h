/*
  Q Light Controller
  importselectiondialog.h

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

#ifndef IMPORTSELECTIONDIALOG_H
#define IMPORTSELECTIONDIALOG_H

#include <QDialog>
#include <QList>

class QTreeWidget;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * File > Import's picker: browse a second (scratch) Doc loaded from another
 * .qxw file and choose which fixtures / fixture groups / functions to bring
 * into the current workspace. Follows the same shape as FixtureSelection/
 * FunctionSelection (modal QDialog + tree in ExtendedSelection mode), just
 * covering all three object kinds Import can pull in.
 *
 * Only lists top-level items -- selecting a function does not require
 * hand-picking its dependencies here; QxwImporter expands the full
 * dependency closure (member functions, the fixtures they touch) itself.
 */
class ImportSelectionDialog final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(ImportSelectionDialog)

public:
    ImportSelectionDialog(QWidget *parent, Doc *sourceDoc);
    ~ImportSelectionDialog();

    QList<quint32> selectedFixtures() const;
    QList<quint32> selectedFixtureGroups() const;
    QList<quint32> selectedFunctions() const;

private:
    void populate();

private:
    Doc *m_sourceDoc;
    QTreeWidget *m_fixtureTree;
    QTreeWidget *m_groupTree;
    QTreeWidget *m_functionTree;
};

/** @} */

#endif

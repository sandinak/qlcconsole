/*
  Q Light Controller Plus
  patchlistdialog.h

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

#ifndef PATCHLISTDIALOG_H
#define PATCHLISTDIALOG_H

#include <QDialog>

#include "patchlist.h"

class QTableWidget;
class QPushButton;
class Doc;

/** @addtogroup ui_fixtures
 * @{
 */

/**
 * The patch list (PatchList) to look over, copy, or save as a spreadsheet
 * (CSV) or a printable PDF for the crew. Read-only; built when it opens.
 */
class PatchListDialog : public QDialog
{
    Q_OBJECT

public:
    /** The table's columns that say where a fixture's data goes. */
    enum { ConnectionColumn = 5, GoesToColumn = 6 };

    PatchListDialog(Doc *doc, QWidget *parent = NULL);

    const PatchList &list() const { return m_list; }
    QTableWidget *table() const { return m_table; }
    /** "Patch list — show.qlcc": the PDF's heading. */
    QString title() const { return m_title; }

    /** Write the CSV to @p path. False (with @p error set) if it cannot. */
    bool saveCsv(const QString &path, QString *error = NULL) const;

    /** The rows picked in the table, in list order. */
    QList<PatchList::Row> selectedRows() const;

public slots:
    void slotCopy();
    void slotSaveCsv();
    void slotSavePdf();
    void slotLabels();

private:
    PatchList m_list;
    QString m_title;
    QTableWidget *m_table;
};

/** @} */

#endif

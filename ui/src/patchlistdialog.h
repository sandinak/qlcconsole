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
#include "rigowner.h"

class QTreeWidgetItem;
class QTreeWidget;
class QPushButton;
class QComboBox;
class QLabel;
class QMenu;
class Doc;

/** @addtogroup ui_fixtures
 * @{
 */

#define SETTINGS_PATCH_LIST_GROUPBY "patchlist/groupby"

/**
 * The patch list (PatchList) to look over, copy, or save as a spreadsheet
 * (CSV, XLSX) or a printable PDF for the crew, and to print fixture labels
 * from. Grouped by where the fixtures hang (or universe, or type), filtered
 * to the discrete or the pixel / matrix fixtures and by owner; the exports
 * and the labels follow what is shown. Read-only; built when it opens.
 */
class PatchListDialog : public QDialog
{
    Q_OBJECT

public:
    /** The columns. A fixture's mode carries its channel count when the
     *  mode's name doesn't say it ("Open + Fly (2 ch)"). */
    enum { PatchColumn = 0, FixtureColumn, MakeColumn, ModeColumn, ConnectionColumn,
           GoesToColumn, MountedColumn, OwnerColumn, GelColumn, ColumnCount };
    /** On the Patch cell: true for a fixture that is not patched (and is
     *  highlighted, "— not patched"). */
    enum { UnpatchedRole = Qt::UserRole + 1 };

    PatchListDialog(Doc *doc, QWidget *parent = NULL);

    /** Every fixture in the show. */
    const PatchList &list() const { return m_list; }
    /** What the dialog shows: list() filtered. */
    PatchList shown() const;
    QTreeWidget *tree() const { return m_tree; }
    /** "Patch list — show.qlcc": the PDF's heading. */
    QString title() const { return m_title; }

    PatchList::GroupBy groupBy() const;
    void setGroupBy(PatchList::GroupBy by);
    PatchList::Filter filter() const;
    void setKind(PatchList::Kind kind);
    /** Only @p owner's fixtures; RigOwner::Unset for anybody's. */
    void setOwner(RigOwner::Owner owner);

    /** The fixture rows shown, in the order shown. */
    QList<PatchList::Row> shownRows() const;
    /** The rows picked in the list, in list order; a section's heading
     *  picks the whole section. */
    QList<PatchList::Row> selectedRows() const;
    /** The item for the fixture called @p name, or null. */
    QTreeWidgetItem *itemFor(const QString &name) const;

    /** Write the CSV / XLSX (what is shown, as it is grouped) to @p path.
     *  False (with @p error set) if it cannot. */
    bool saveCsv(const QString &path, QString *error = NULL) const;
    bool saveXlsx(const QString &path, QString *error = NULL) const;

    /** The right-click menu for the rows selected now (the caller owns it). */
    QMenu *contextMenu();

public slots:
    void slotCopy();
    void slotSaveCsv();
    void slotSaveXlsx();
    void slotSavePdf();
    /** Labels for the list shown (or the rows picked). */
    void slotLabels();
    /** Labels for the rows picked. */
    void slotPrintSelectedLabels();

private slots:
    void slotRefill();
    void slotContextMenu(const QPoint &pos);

private:
    void openLabels(const QList<PatchList::Row> &selected);
    /** Highlight @p item's Patch cell: amber, italic, "— not patched". */
    static void markUnpatched(QTreeWidgetItem *item);

private:
    PatchList m_list;
    QString m_title;
    QList<PatchList::Row> m_shown;     //!< the fixture rows, in the order shown
    QTreeWidget *m_tree;
    QComboBox *m_groupBy;
    QComboBox *m_kind;
    QComboBox *m_owner;
    QLabel *m_summary;
};

/** @} */

#endif

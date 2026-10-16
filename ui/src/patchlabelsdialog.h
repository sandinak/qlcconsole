/*
  Q Light Controller Plus
  patchlabelsdialog.h

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

#ifndef PATCHLABELSDIALOG_H
#define PATCHLABELSDIALOG_H

#include <QDialog>

#include "patchlist.h"

class QRadioButton;
class QComboBox;
class QLabel;

/** @addtogroup ui_fixtures
 * @{
 */

#define SETTINGS_PATCH_LABEL_SHEET "patchlist/labelsheet"

/**
 * Labels from the patch list: a label per fixture (all of them, or the rows
 * picked in the list) on a label stock (PatchLabels::sheets()), saved as a
 * PDF to print. The stock chosen is remembered.
 */
class PatchLabelsDialog : public QDialog
{
    Q_OBJECT

public:
    PatchLabelsDialog(const QList<PatchList::Row> &all, const QList<PatchList::Row> &selected,
                      const QString &title, const QString &suggestedFile, QWidget *parent = NULL);

    /** The stock chosen ("avery5160"). */
    QString sheetKey() const;
    void setSheetKey(const QString &key);

    /** Only the rows picked in the list, rather than every fixture. */
    bool selectedOnly() const;
    void setSelectedOnly(bool only);

    /** The rows that get a label. */
    QList<PatchList::Row> rows() const;

    /** Write the labels to @p path. False (with @p error set) if it cannot. */
    bool savePdf(const QString &path, QString *error = NULL) const;

public slots:
    void slotSave();

private slots:
    void slotUpdateCount();

private:
    QList<PatchList::Row> m_all;
    QList<PatchList::Row> m_selected;
    QString m_title;
    QString m_suggestedFile;

    QComboBox *m_sheet;
    QRadioButton *m_allRadio;
    QRadioButton *m_selectedRadio;
    QLabel *m_count;
};

/** @} */

#endif

/*
  Q Light Controller Plus
  cuesheetdialog.h

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

#ifndef CUESHEETDIALOG_H
#define CUESHEETDIALOG_H

#include <QDialog>

#include "cuesheet.h"

class QTextBrowser;
class QComboBox;
class QLabel;
class Doc;

/** @addtogroup ui_showtree
 * @{
 */

/**
 * The stage manager's paper (CueSheet) to look over and take away: the cue
 * sheet or the tracking sheet of one production, as CSV, an Excel workbook
 * or a printable PDF. Read-only; built when it opens, from the Show Tree's
 * own resolve.
 */
class CueSheetDialog : public QDialog
{
    Q_OBJECT

public:
    enum Sheet { CueSheetPage = 0, TrackingPage };

    CueSheetDialog(Doc *doc, quint32 productionId, QWidget *parent = nullptr);

    const CueSheet &sheet() const { return m_sheet; }
    Sheet page() const;
    void setPage(Sheet page);
    /** "Cue sheet — Dance Hysteria — show.qlcc": the PDF's heading. */
    QString title() const;
    /** What is shown, as HTML. */
    QString shownHtml() const;

    /** Write what is shown to @p path. False (with @p error) if it cannot. */
    bool saveCsv(const QString &path, QString *error = nullptr) const;
    bool saveXlsx(const QString &path, QString *error = nullptr) const;
    bool savePdf(const QString &path, QString *error = nullptr) const;

private slots:
    void slotShow();
    void slotSaveCsv();
    void slotSaveXlsx();
    void slotSavePdf();

private:
    QString suggestedName(const QString &ext) const;

    CueSheet m_sheet;
    QString m_show;
    QComboBox *m_page;
    QTextBrowser *m_view;
    QLabel *m_summary;
};

/** @} */

#endif

/*
  Q Light Controller Plus
  networkreportdialog.h

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

#ifndef NETWORKREPORTDIALOG_H
#define NETWORKREPORTDIALOG_H

#include <QDialog>

#include "networkreport.h"

class QPlainTextEdit;
class QPushButton;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * The network summary for the data package (docs/design/NETWORK_CABLING_DESIGN.md, N5):
 * NetworkReport::toText() to read or copy, and its CSV to save for a
 * spreadsheet. Read-only; built once when the dialog opens.
 */
class NetworkReportDialog : public QDialog
{
    Q_OBJECT

public:
    NetworkReportDialog(Doc *doc, QWidget *parent = NULL);

    const NetworkReport &report() const { return m_report; }

    /** Write the CSV to @p path. False (with @p error set) if it cannot. */
    bool saveCsv(const QString &path, QString *error = NULL) const;

public slots:
    void slotCopy();
    void slotSaveCsv();

private:
    NetworkReport m_report;
    bool m_feet;
    QPlainTextEdit *m_text;
    QPushButton *m_copyBtn;
    QPushButton *m_saveBtn;
};

/** @} */

#endif

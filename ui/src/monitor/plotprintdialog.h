/*
  Q Light Controller Plus
  plotprintdialog.h

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

#ifndef PLOTPRINTDIALOG_H
#define PLOTPRINTDIALOG_H

#include <QDialog>

#include "plotsheet.h"

class QPushButton;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
class QTimer;
class Doc;

/** @addtogroup ui_mon DMX Monitor
 * @{
 */

/**
 * Lighting Studio > More > Print & Reports > Print Plot / Save PDF…: the
 * plot as a drawing (PlotSheet) with a preview of the page, saved as a PDF
 * or sent to a printer. What is typed in the title block is remembered for
 * the next sheet; the show is not changed.
 */
class PlotPrintDialog : public QDialog
{
    Q_OBJECT

public:
    PlotPrintDialog(Doc *doc, const PlotSheet::Context &context, QWidget *parent = nullptr);

    /** The sheet as the dialog stands. */
    PlotSheet sheet() const;
    PlotSheet::Options options() const;

    /** Write the PDF to @p path. False (with @p error set) when it can't. */
    bool savePdf(const QString &path, QString *error = nullptr);

    /** The file name offered: "<show>-plot-<view>.pdf". */
    QString suggestedName() const;

public slots:
    void slotSavePdf();
    void slotPrint();

private slots:
    void slotRefresh();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void remember() const;

    Doc *m_doc;
    PlotSheet::Context m_context;
    QLineEdit *m_title;
    QLineEdit *m_venue;
    QLineEdit *m_drawnBy;
    QLineEdit *m_revision;
    QComboBox *m_view;
    QComboBox *m_page;
    QComboBox *m_background;
    QCheckBox *m_symbolKey;
    QCheckBox *m_layerKey;
    QLabel *m_summary;
    QLabel *m_preview;
    QPushButton *m_saveBtn;
    QPushButton *m_printBtn;
    QTimer *m_refresh;
};

/** @} */

#endif

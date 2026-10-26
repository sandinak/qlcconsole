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

#include "patchlabels.h"
#include "patchlist.h"

class QDoubleSpinBox;
class QRadioButton;
class QLineEdit;
class QComboBox;
class QPrinter;
class QLabel;

/** @addtogroup ui_fixtures
 * @{
 */

#define SETTINGS_PATCH_LABEL_SHEET "patchlist/labelsheet"
#define SETTINGS_PATCH_LABEL_TAPE "patchlist/labeltape"
#define SETTINGS_PATCH_LABEL_TEMPLATE "patchlist/labeltemplate"
#define SETTINGS_PATCH_LABEL_CUTGAP "patchlist/labelcutgap"

/**
 * Labels from the patch list: a label per fixture (all of them, or the rows
 * picked in the list) saying what the template says ("{name} D{address}"),
 * on a label stock (PatchLabels::sheets()): printed through the system's
 * print dialog or saved as a PDF. The stock, the tape width, the template
 * and the cut gap are remembered.
 */
class PatchLabelsDialog : public QDialog
{
    Q_OBJECT

public:
    /** A printer installed on this computer, as QPrinterInfo has it. */
    struct InstalledPrinter
    {
        QString name;           //!< the queue's name ("Brother_PT_P710BT")
        QString description;    //!< what the system calls it ("Brother PT-P710BT")
        QString model;          //!< make and model
    };

    /** The printers installed (QPrinterInfo::availablePrinters()), or
     *  the ones a test stands in. */
    static QList<InstalledPrinter> installedPrinters();
    /** Tests: stand @p printers in for the system's (null: the system's).
     *  Never the real ones in a test: a found Brother picks the stock. */
    static void setInstalledPrintersForTest(const QList<InstalledPrinter> *printers);
    /** The installed printer the Brother preset is for: "PT-P710BT" or
     *  "PT-P710" in its name, description or model. Null if none. */
    static const InstalledPrinter *brotherPrinter(const QList<InstalledPrinter> &printers);

    PatchLabelsDialog(const QList<PatchList::Row> &all, const QList<PatchList::Row> &selected,
                      const QString &title, const QString &suggestedFile, QWidget *parent = NULL);

    /** The stock chosen ("tape"). */
    QString sheetKey() const;
    void setSheetKey(const QString &key);
    /** The tape's width in millimetres (12, 24), for a tape stock. */
    int tapeWidth() const;
    void setTapeWidth(int mm);
    /** The label's template. */
    QString labelTemplate() const;
    void setLabelTemplate(const QString &text);
    /** Millimetres of blank tape between labels. */
    qreal cutGap() const;
    void setCutGap(qreal mm);
    /** The show patches more than one universe: addresses say which. */
    void setMultiUniverse(bool multi);

    /** The stock as it prints the rows chosen (PatchLabels::fitted()). */
    PatchLabels::Sheet sheet() const;
    PatchLabels::Options options() const;

    /** Only the rows picked in the list, rather than every fixture. */
    bool selectedOnly() const;
    void setSelectedOnly(bool only);

    /** The rows that get a label. */
    QList<PatchList::Row> rows() const;

    /** Write the labels to @p path. False (with @p error set) if it cannot. */
    bool savePdf(const QString &path, QString *error = NULL) const;
    /** Set @p printer up for the labels (page size, no margins) and the
     *  Brother preset's printer when it is there. */
    void setUpPrinter(QPrinter *printer) const;
    /** The printer setUpPrinter() picks: the Brother found, for its preset;
     *  empty for the system's choice. */
    QString printerName() const;
    /** The Brother PT-P710BT installed here (its queue name), or empty. */
    QString brotherFound() const { return m_brother; }
    /** Print the labels on @p printer as it is set up. False (with @p error
     *  set) if it cannot. */
    bool print(QPrinter *printer, QString *error = NULL) const;

public slots:
    void slotSave();
    void slotPrint();

private slots:
    void slotUpdate();

private:
    QList<PatchList::Row> m_all;
    QList<PatchList::Row> m_selected;
    QString m_title;
    QString m_suggestedFile;
    bool m_multiUniverse = false;
    QString m_brother;          //!< the Brother's queue, when it is installed

    QComboBox *m_sheet;
    QComboBox *m_tape;
    QLineEdit *m_template;
    QDoubleSpinBox *m_cutGap;
    QRadioButton *m_allRadio;
    QRadioButton *m_selectedRadio;
    QLabel *m_preview;
    QLabel *m_count;
    QLabel *m_printerFound;
};

/** @} */

#endif

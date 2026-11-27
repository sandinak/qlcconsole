/*
  Q Light Controller Plus - qlcconsole
  surfacekeylabels.h

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

#ifndef SURFACEKEYLABELS_H
#define SURFACEKEYLABELS_H

#include <QDialog>
#include <QImage>
#include <QList>
#include <QSizeF>
#include <QString>

#include "qlcui_export.h"

class QPagedPaintDevice;
class QComboBox;
class QLabel;
class QPrinter;
class SurfaceMapView;

/** @addtogroup ui_io
 * @{
 */

/**
 * Keycap labels (Q357): what each key of a board does, on paper cut to the
 * key. A sheet is a PDF with a cut-out square at the board's real key size
 * for each key, the job in it, one set per surface page; or one label per
 * key on tape (12, 18 or 24 mm TZe) at the Brother P-touch CUBE's 180 dpi,
 * written as a PDF or PNGs, or printed through the system's print dialog:
 * the system has the printer, there is no Bluetooth code here.
 *
 * A key's size is its drawing's size times the layout's "unitMm"
 * (SurfaceLayout::unitMm(), measured off the published size: check it with
 * a rule before cutting). A control that is not a button or a pad has no
 * label.
 */
class QLC_UI_EXPORT SurfaceKeyLabels
{
public:
    struct Key
    {
        quint32 channel = 0;
        QString face;       //!< what the board prints on it
        QString text;       //!< what the label says; empty: the face, grey
        QSizeF mm;          //!< the real key
    };
    /** The keys of one surface page (or "Now" for a board with none). */
    struct Page
    {
        QString name;
        QList<Key> keys;
    };

    /** The pages of @p view's board: every key with its job on that page
     *  (the page's own label, else what the desk or the show gives it now). */
    static QList<Page> pages(const SurfaceMapView *view);

    /** A sheet: a cut-out per key, a section per page, A4. False, with
     *  @p error, when it can't be written or the board's key size is not
     *  known. */
    static bool writeSheet(const QString &path, const QString &title, const QList<Page> &pages,
                           QString *error = nullptr);

    /** The same, drawn on @p device (a QPdfWriter, or a printer on A4). */
    static bool paintSheet(QPagedPaintDevice *device, const QString &title, const QList<Page> &pages,
                           QString *error = nullptr);

    /** The tape widths there are, in millimetres. */
    static QList<int> tapeWidths();
    /** What a tape prints across it (a 12 mm TZe prints 9 mm, 18 mm 12, 24 mm 18). */
    static double printableMm(int tapeMm);
    /** The Brother P-touch CUBE's resolution. */
    static constexpr int kTapeDpi = 180;
    /** One label on tape: as long as the key, @p tapeMm's printable width
     *  high, @p kTapeDpi, black on white. */
    static QImage tapeLabel(const Key &key, int tapeMm);
    /** The label's length on the tape, mm (the key's, and at least 8). */
    static double tapeLengthMm(const Key &key);
    /** One tape label a PDF page, to a PDF or a printer set up with the
     *  tape's page (setUpPrinter()). */
    static bool paintTape(QPagedPaintDevice *device, const QList<Key> &keys, int tapeMm,
                          QString *error = nullptr);
    static bool writeTapePdf(const QString &path, const QList<Key> &keys, int tapeMm, QString *error = nullptr);
    /** A PNG a label in @p dir, named by page and face. The paths made. */
    static QStringList writeTapePngs(const QString &dir, const QList<Key> &keys, int tapeMm,
                                     QString *error = nullptr);
    /** The printer set to the tape: its page the longest label by the
     *  tape's width, full page, the Brother when one is installed. */
    static void setUpPrinter(QPrinter *printer, const QList<Key> &keys, int tapeMm);
};

/**
 * Print Key Labels… on a map: the sheet or the tape, the page, the sizes
 * (in Settings ▸ Units), Save PDF…, Save PNGs… and Print…
 */
class QLC_UI_EXPORT SurfaceKeyLabelsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SurfaceKeyLabelsDialog(const SurfaceMapView *view, QWidget *parent = nullptr);

    /** 0: the sheet; 12, 18, 24: tape of that width. */
    int tapeMm() const;
    void setTapeMm(int mm);
    /** The keys that would be labelled, of the page chosen (all pages for a sheet). */
    QList<SurfaceKeyLabels::Key> keys() const;
    /** The words the dialog says of the sizes ("Keys: 14 mm x 13 mm ..."). */
    QString sizeLine() const;
    bool savePdf(const QString &path, QString *error = nullptr) const;
    bool savePngs(const QString &dir, QStringList *made = nullptr, QString *error = nullptr) const;

private slots:
    void update();
    void slotSavePdf();
    void slotSavePngs();
    void slotPrint();

private:
    QList<SurfaceKeyLabels::Page> m_pages;
    QString m_title;
    QComboBox *m_stock;
    QComboBox *m_page;
    QLabel *m_sizes;
    QLabel *m_preview;
};

/** @} */

#endif

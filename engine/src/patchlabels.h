/*
  Q Light Controller Plus
  patchlabels.h

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

#ifndef PATCHLABELS_H
#define PATCHLABELS_H

#include <QRectF>
#include <QSizeF>
#include <QImage>
#include <QString>
#include <QList>

#include "patchlist.h"

/** @addtogroup engine Engine
 * @{
 */

class QPagedPaintDevice;
class QPageLayout;

/**
 * A label per fixture, from the patch list: stuck on the fixture (or its
 * case) so whoever hangs it knows its name and where it is addressed. What
 * the label says comes from a template ("{name} D{address}" -> "Spot 3
 * D101"). Laid out on continuous tape (a label a page, as long as its text
 * needs), a sheet of labels or a roll; printed through the system's print
 * dialog or written as a PDF.
 */
class PatchLabels
{
public:
    /** A label stock. Every length in millimetres; the labels run left to
     *  right, then down, then on to the next page. */
    struct Sheet
    {
        QString key;           //!< remembered in the settings: "avery5160"
        QString name;          //!< "Avery 5160 (Letter, 30 a sheet)"
        QSizeF page;           //!< the page (a roll: one label's length)
        QSizeF label;          //!< one label
        QPointF origin;        //!< the first label's top-left corner on the page
        QSizeF pitch;          //!< label to label, across and down
        int columns = 1;
        int rows = 1;

        /** Continuous tape: a label a page, the page as wide as the tape
         *  and as long as the longest label's text needs (fitted()). */
        bool tape = false;
        /** Millimetres the print keeps from the label's edges across it
         *  (a 24 mm TZe tape prints 18 mm of it). */
        qreal margin = 2.0;
        /** A printer to pick, when one by this name is there: the
         *  Brother preset's hook ("PT-P710BT"). Empty: the system's. */
        QString printer;

        int perPage() const { return columns * rows; }
        bool isValid() const { return !key.isEmpty() && perPage() > 0; }
    };

    /** What one label says. */
    struct Label
    {
        QString name;          //!< "Wash 3"
        QString address;       //!< "U3 · 101", "not patched", "clash · U1 · 12"
        QString universe;      //!< the universe's name, when it has one: "Stage left"
        QString detail;        //!< "16-channel · 16 ch"
        QString network;       //!< "WLED · lantern-2", or "WLED · 172.18.2.50" when the IP has no name
        QString mount;         //!< "Truss FOH · 2.5 m along"
    };

    /** How the labels are made. */
    struct Options
    {
        /** What a label says (render()); empty: defaultTemplate(). */
        QString text;
        /** The show uses more than one universe: addresses say which. */
        bool multiUniverse = false;
        /** Tape: millimetres of blank tape between one label's text and
         *  the next's, half at each end of a label, where it is cut. */
        qreal cutGap = 4.0;
    };

    /** The stocks there are, the default first. A tape stock is given at
     *  24 mm; sheet() makes it another width. */
    static QList<Sheet> sheets();
    /** The stock with @p key, or the default when there is none; a tape
     *  stock @p tapeMm wide (12 or 24; anything else is 24). */
    static Sheet sheet(const QString &key, int tapeMm = 24);
    static QString defaultSheetKey();
    /** The tape widths there are presets for, in millimetres: 12, 24. */
    static QList<int> tapeWidths();

    /** @p sheet ready for @p rows: a tape as long as the longest label's
     *  text needs at the full height of the tape (with the cut gap), 25 to
     *  150 mm; text longer than that is set smaller. Other stocks as they are. */
    static Sheet fitted(const Sheet &sheet, const QList<PatchList::Row> &rows,
                        const Options &options);
    /** The page @p sheet prints on: its size, no margins, landscape when
     *  it is wider than it is tall. */
    static QPageLayout pageLayout(const Sheet &sheet);

    /** "{name} D{address}": the label a fixture gets unless it is changed. */
    static QString defaultTemplate();
    /** The fields a template can use: {name} {address} {universe} {mode}
     *  {location} {owner} {group}. */
    static QStringList templateFields();
    /** The DMX start address, one-based: "101"; "2.101" (universe 2) when
     *  @p multiUniverse; "—" when not patched. A clash says where it asked. */
    static QString address(const PatchList::Row &row, bool multiUniverse);
    /** @p text with each field filled in for @p row; an unknown {field}
     *  stays as it is. */
    static QString render(const QString &text, const PatchList::Row &row, bool multiUniverse);

    /** Where label @p index (zero-based) goes: its page (zero-based) and its
     *  rectangle on that page, in millimetres. */
    static QRectF labelRect(const Sheet &sheet, int index, int *page = nullptr);
    /** Pages @p count labels take. */
    static int pageCount(const Sheet &sheet, int count);

    /** What the label for @p row says. */
    static Label label(const PatchList::Row &row);

    /** Draw a label per row of @p rows on @p device (a QPdfWriter, or a
     *  QPrinter set to pageLayout() of the fitted() sheet, full page).
     *  @p sheet should be fitted() already. False, with @p error, if there
     *  is nothing to label or it can't be drawn. */
    static bool paint(QPagedPaintDevice *device, const QList<PatchList::Row> &rows,
                      const Sheet &sheet, const Options &options, QString *error = nullptr);

    /** @p row's label as a picture, @p pxPerMm pixels a millimetre, on
     *  white with a grey outline: the dialog's preview. */
    static QImage preview(const PatchList::Row &row, const Sheet &sheet, const Options &options,
                          qreal pxPerMm = 8.0);

    /** Write a label per row of @p rows to the PDF at @p path. False, with
     *  @p error, if the file can't be written. */
    static bool writePdf(const QString &path, const QList<PatchList::Row> &rows,
                         const Sheet &sheet, const QString &title, QString *error = nullptr);
    /** The same, made the way @p options says. */
    static bool writePdf(const QString &path, const QList<PatchList::Row> &rows,
                         const Sheet &sheet, const QString &title, const Options &options,
                         QString *error = nullptr);
};

/** @} */

#endif

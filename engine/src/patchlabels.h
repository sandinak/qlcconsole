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
#include <QString>
#include <QList>

#include "patchlist.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * A label per fixture, from the patch list: stuck on the fixture (or its
 * case) so whoever hangs it knows its name, where it is addressed and where
 * it goes. Laid out on a sheet of labels or a roll, written as a PDF to
 * print from Preview.
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

        int perPage() const { return columns * rows; }
        bool isValid() const { return !key.isEmpty() && perPage() > 0; }
    };

    /** What one label says. */
    struct Label
    {
        QString name;          //!< "Wash 3"
        QString address;       //!< "U3 · 101", "not patched", "clash · U1 · 12"
        QString detail;        //!< "16-channel · 16 ch"
        QString mount;         //!< "Truss FOH · 2.5 m along"
    };

    /** The stocks there are, the default first. */
    static QList<Sheet> sheets();
    /** The stock with @p key, or the default when there is none. */
    static Sheet sheet(const QString &key);
    static QString defaultSheetKey();

    /** Where label @p index (zero-based) goes: its page (zero-based) and its
     *  rectangle on that page, in millimetres. */
    static QRectF labelRect(const Sheet &sheet, int index, int *page = nullptr);
    /** Pages @p count labels take. */
    static int pageCount(const Sheet &sheet, int count);

    /** What the label for @p row says. */
    static Label label(const PatchList::Row &row);

    /** Write a label per row of @p rows to the PDF at @p path. False, with
     *  @p error, if the file can't be written. */
    static bool writePdf(const QString &path, const QList<PatchList::Row> &rows,
                         const Sheet &sheet, const QString &title, QString *error = nullptr);
};

/** @} */

#endif

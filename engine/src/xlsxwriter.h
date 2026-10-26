/*
  Q Light Controller Plus
  xlsxwriter.h

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

#ifndef XLSXWRITER_H
#define XLSXWRITER_H

#include <QStringList>
#include <QByteArray>
#include <QString>
#include <QPair>
#include <QList>

#include "qlcengine_export.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * Just enough of Office Open XML to hand a table to Excel, Numbers or
 * Google Sheets: one sheet, text and numbers, the first row bold. The
 * package is a ZIP stored without compression (no zlib needed); every
 * reader takes that.
 */
namespace XlsxWriter
{
    /** A workbook of one sheet named @p sheetName holding @p rows (the
     *  first is the header, set bold). Cells of @p numericColumns that read
     *  as numbers are written as numbers; empty cells are left out. */
    QLC_ENGINE_EXPORT QByteArray workbook(const QString &sheetName, const QList<QStringList> &rows,
                                          const QList<int> &numericColumns = QList<int>());

    /** A ZIP archive of @p files (name, contents), stored. */
    QLC_ENGINE_EXPORT QByteArray zip(const QList<QPair<QString, QByteArray>> &files);

    /** The ZIP's checksum (CRC-32, IEEE 802.3) of @p data. */
    QLC_ENGINE_EXPORT quint32 crc32(const QByteArray &data);

    /** "A", "B", ... "Z", "AA": the spreadsheet's name for column @p index
     *  (zero-based). */
    QLC_ENGINE_EXPORT QString columnName(int index);
}

/** @} */

#endif

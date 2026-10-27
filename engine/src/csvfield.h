/*
  Q Light Controller Plus - qlcconsole
  csvfield.h

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

#ifndef CSVFIELD_H
#define CSVFIELD_H

#include <QString>

/** @addtogroup engine Engine
 * @{
 */

/* One field of the CSV files the reports write (patch list, focus report,
 * network report). They are opened in Excel, Numbers and Lightwright, which
 * take a cell starting with = + - @ (or a tab or carriage return) as a
 * formula: a fixture or show named =cmd|'/C calc'!A1 would run. Such a cell
 * gets a leading apostrophe, which a spreadsheet shows as text (OWASP's
 * CSV-injection advice). A plain number (-1.5, a coordinate) is left as it
 * is: it can't be a formula. */
namespace CsvField
{
    inline bool looksLikeFormula(const QString &field)
    {
        if (field.isEmpty())
            return false;
        const QChar c = field.at(0);
        if (c != QLatin1Char('=') && c != QLatin1Char('+') && c != QLatin1Char('-')
            && c != QLatin1Char('@') && c != QLatin1Char('\t') && c != QLatin1Char('\r'))
            return false;
        bool isNumber = false;
        field.toDouble(&isNumber);
        return isNumber == false;
    }

    /** @a field defused (see above), then quoted when it holds a comma,
     *  quote or line break. */
    inline QString escape(const QString &field)
    {
        QString f = looksLikeFormula(field) ? QLatin1Char('\'') + field : field;
        if (!f.contains(QLatin1Char(',')) && !f.contains(QLatin1Char('"'))
            && !f.contains(QLatin1Char('\n')) && !f.contains(QLatin1Char('\r')))
            return f;
        f.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + f + QLatin1Char('"');
    }
}

/** @} */

#endif

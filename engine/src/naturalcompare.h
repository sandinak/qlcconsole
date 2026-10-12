/*
  Q Light Controller Plus
  naturalcompare.h

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

#ifndef NATURALCOMPARE_H
#define NATURALCOMPARE_H

#include <QString>

/** @addtogroup engine Engine
 * @{
 */

/** Natural (human) string compare: runs of digits compare as numbers, so
 *  "Wash 2" < "Wash 10"; the rest case-insensitively. <0 / 0 / >0 like
 *  strcmp.
 *
 *  Used instead of QCollator::setNumericMode(), which is quietly ignored by a
 *  Qt built without ICU -- as the Linux CI's is -- so the same show sorted
 *  "Wash 10" before "Wash 2" there and the patch came out in a different
 *  order. */
inline int naturalCompare(const QString &a, const QString &b)
{
    int i = 0, j = 0;
    const int na = a.size(), nb = b.size();
    while (i < na && j < nb)
    {
        const QChar ca = a.at(i);
        const QChar cb = b.at(j);
        if (ca.isDigit() && cb.isDigit())
        {
            int i2 = i, j2 = j;
            while (i2 < na && a.at(i2).isDigit()) i2++;
            while (j2 < nb && b.at(j2).isDigit()) j2++;
            const qulonglong va = a.mid(i, i2 - i).toULongLong();
            const qulonglong vb = b.mid(j, j2 - j).toULongLong();
            if (va != vb)
                return va < vb ? -1 : 1;
            i = i2;
            j = j2;
        }
        else
        {
            const int c = QString::compare(QString(ca), QString(cb), Qt::CaseInsensitive);
            if (c != 0)
                return c;
            i++;
            j++;
        }
    }
    return (na - i) - (nb - j);
}

/** @} */

#endif

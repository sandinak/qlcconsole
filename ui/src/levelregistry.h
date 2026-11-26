/*
  Q Light Controller Plus - qlcconsole
  levelregistry.h

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

#ifndef LEVELREGISTRY_H
#define LEVELREGISTRY_H

#include <QCoreApplication>
#include <QList>
#include <QString>

#include "qlcui_export.h"
#include "showlevel.h"

/** @addtogroup ui UI
 * @{
 */

/**
 * The one table of what each level shows (docs/design/WORKSPACE_LEVELS.md
 * §1.3): one row per feature key, with the lowest level that shows it, how
 * it hides below that (ShowLevel::Treatment) and the words its switch path
 * uses. A new feature that should not be at Student is one row here and
 * one ShowLevel::bind() where it is made; levels_test fails a bound key
 * with no row and a row nothing binds.
 */
class QLC_UI_EXPORT LevelRegistry
{
    Q_DECLARE_TR_FUNCTIONS(LevelRegistry)

public:
    struct Row
    {
        QString key;
        ShowLevel::Level level = ShowLevel::Standard;
        ShowLevel::Treatment treatment = ShowLevel::Fold;
        /** What it is, for the switch path: "Importing another show". */
        QString words;
        /** Tab rows: the tab's key (App::tabKeys()), "Simple Desk". */
        QString tab;
        /** A switch point for a setting still to be built (board Q343):
         *  nothing binds it yet. */
        bool followUp = false;
    };

    static const QList<Row> &rows();
    /** The row of @p key, or null. */
    static const Row *row(const QString &key);
};

/** @} */

#endif

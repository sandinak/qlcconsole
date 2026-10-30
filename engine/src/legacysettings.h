/*
  qlcconsole
  legacysettings.h

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

#ifndef LEGACYSETTINGS_H
#define LEGACYSETTINGS_H

#include <QCoreApplication>
#include <QPair>
#include <QStringList>
#include <memory>

class QSettings;

/** @addtogroup engine Engine
 * @{
 */

/**
 * QLC+ 4's own settings -- its QSettings store, "Q Light Controller Plus"
 * -- brought over into qlcconsole's, once and only when the user says
 * yes: the recent shows, the I/O and plugin preferences, and the
 * interface preferences that mean the same thing here. Window layouts
 * (geometry, splitters) are left behind: the windows differ.
 *
 * QLC+'s user folder (fixtures, input profiles, scripts) is not copied
 * here: QLCFile::mergeLegacyUserData() already links it in at every start.
 *
 * Nothing in QLC+'s store is ever written, and a setting qlcconsole
 * already has is kept (recent shows are merged: qlcconsole's first).
 */
class LegacySettings
{
    Q_DECLARE_TR_FUNCTIONS(LegacySettings)

public:
    /** What would be brought over (computed by plan(), applied by apply()) */
    struct Plan
    {
        QStringList recentFiles;    ///< QLC+'s recent shows qlcconsole lacks, that still exist
        QStringList ioKeys;         ///< I/O and plugin preference keys
        QStringList uiKeys;         ///< interface preference keys
        bool isEmpty() const
        { return recentFiles.isEmpty() && ioKeys.isEmpty() && uiKeys.isEmpty(); }
    };

    /** QLC+ 4's settings store on this computer (read it only), or the
     *  .ini file QLC_QLCPLUS_SETTINGS names (the tests use that) */
    static std::unique_ptr<QSettings> openQlcPlus();

    /** The organization and application openQlcPlus() opens */
    static QPair<QString, QString> qlcPlusScope();

    /** What @a from (QLC+'s store) holds that @a to (qlcconsole's) lacks */
    static Plan plan(const QSettings &from, const QSettings &to);

    /** Copy @a plan from @a from into @a to. Returns the settings written. */
    static int apply(const QSettings &from, QSettings &to, const Plan &plan);

    /** The plan in a few lines, for the question and the Settings page */
    static QString describe(const Plan &plan);

    /** Whether the one-time offer has been made (accepted or not) */
    static bool wasOffered(const QSettings &own);
    static void markOffered(QSettings &own);

    /** How many recent shows the menu keeps ("workspace/recent<N>") */
    static const int kMaxRecent = 10;
};

/** @} */

#endif

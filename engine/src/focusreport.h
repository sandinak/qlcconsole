/*
  Q Light Controller Plus
  focusreport.h

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

#ifndef FOCUSREPORT_H
#define FOCUSREPORT_H

#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QList>

#include "qlcengine_export.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The focus report (board Q134): what was focused in one venue, light by
 * light, call by call -- the call, the fixture, where it focuses to (with
 * the stage marks' position, "SL 2.4 · US 3.1"), the correction from the
 * show's own calibration, the lens, who checked it and when, the call's
 * note, and the flags: dead, swapped (not the fixture focused there),
 * needs focus. A practice venue's report also has the score.
 *
 * Two forms: a printable page (HTML, which the UI also prints to PDF) and
 * a CSV in the columns Lightwright imports. Both are made here, from the
 * venue's file as it is held, without touching the rig.
 */
class QLC_ENGINE_EXPORT FocusReport final
{
    Q_DECLARE_TR_FUNCTIONS(FocusReport)

public:
    struct Row
    {
        int call = -1;              ///< 0-based; -1: corrected outside the calls
        QString callName;
        quint32 fid = 0xFFFFFFFF;
        QString fixture;            ///< its name
        QString type;               ///< "Robe Robin 600"
        QString mode;
        QString address;            ///< "1.101" (universe.address, both 1-based)
        QString hung;               ///< the truss or pipe it hangs on, else empty
        QString owner;              ///< Tour, House or Rental (RigOwner, VENUES.md)
        QString target;             ///< "DSC", or "where it lands"
        QString position;           ///< the target from the stage marks
        QString correction;         ///< "+1.3° pan, −0.4° tilt"; empty: none
        QString lens;               ///< "19.0°"; empty: no hand-set lens
        QString checkedBy;
        QDateTime checkedAt;
        QString notes;
        bool checked = false;
        bool dead = false;
        bool swapped = false;
        bool needsFocus = false;
        double practiceError = -1.0;  ///< degrees left off, practice only
    };

    /** The rows for venue @p venue, in call order and stage order within a
     *  call, then the lights it corrects that no call has. */
    static QList<Row> rows(Doc *doc, const QString &venue);

    /** "dead, swapped, needs focus" -- empty when none. */
    static QString flags(const Row &row);

    /** The printable page, a whole HTML document. @p showName heads it. */
    static QString html(Doc *doc, const QString &venue, const QString &showName);

    /** The Lightwright-style CSV: a header line, then one line per row. */
    static QString csv(Doc *doc, const QString &venue);
    /** The CSV's header columns. */
    static QStringList csvColumns();

    /** One CSV field: quoted when it holds a comma, a quote or a newline. */
    static QString csvField(const QString &text);
};

/** @} */

#endif

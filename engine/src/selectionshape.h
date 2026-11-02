/*
  Q Light Controller Plus
  selectionshape.h

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

#ifndef SELECTIONSHAPE_H
#define SELECTIONSHAPE_H

#include <QCoreApplication>
#include <QPointF>
#include <QRectF>
#include <QHash>
#include <QList>

#include "qlcengine_export.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * What a fixture selection is, in the show's own groups (Q201): picking
 * every fixture of Front Wash picks Front Wash. Deterministic rules, no
 * guessing:
 *
 * - **Groups.** Only fixture groups of two or more fixtures count. A
 *   selection equal to one group is that group. Otherwise the whole groups
 *   inside it are taken, largest first (a parent equal to two towers
 *   together beats the towers), never overlapping; what is left over stays
 *   discrete fixtures ("SR Tower + 1 fixture"). Ties: the group used most
 *   in the show (scenes and Show Tree layers targeting it), then name
 *   order, then id. No whole group inside it: all discrete.
 * - **Near misses** (hint()): a group minus one or two of its fixtures
 *   (more than half of it picked, nothing else) -- "select all 6?"; on the
 *   plot, a member just outside the rubber band (within a third of the
 *   group's fixture spacing) the same; a group plus one stray -- "drop
 *   it?". Only offered: nothing here changes a selection.
 */
class QLC_ENGINE_EXPORT SelectionShape
{
    Q_DECLARE_TR_FUNCTIONS(SelectionShape)

public:
    /** Whole groups (in the order taken) and the fixtures left over (in
     *  the selection's order). */
    QList<quint32> groups;
    QList<quint32> fixtures;

    bool isEmpty() const { return groups.isEmpty() && fixtures.isEmpty(); }

    /** @a fixtureIds in the show's groups. */
    static SelectionShape resolve(const Doc *doc, const QList<quint32> &fixtureIds);
    /** "Front Wash (6)", "SR Tower + SL Tower (12)", "SR Tower + 1
     *  fixture", "3 fixtures"; "" nothing. */
    static QString label(const Doc *doc, const QList<quint32> &fixtureIds);
    /** How often group @a groupId is targeted in the show (the tie-break). */
    static int groupUsage(const Doc *doc, quint32 groupId);

    /** A near miss, offered (never applied). */
    struct Hint
    {
        enum Kind { None, AddMissing, DropStray };
        Kind kind = None;
        quint32 group = 0xFFFFFFFF;
        /** The fixtures it would add (AddMissing) or drop (DropStray). */
        QList<quint32> fixtures;
        /** The selection it would make. */
        QList<quint32> result;
        /** Its words: "Front Wash is 6 — select all 6?". */
        QString text;
        /** A member just outside the rubber band (the plot's). */
        bool nearBand = false;
        bool isNull() const { return kind == None; }
    };
    /** The near miss of @a fixtureIds (None: none). @a positions (a fixture's
     *  centre) and @a band (the rubber band, same coordinates; null: a
     *  click) add the "just outside the box" case. */
    static Hint hint(const Doc *doc, const QList<quint32> &fixtureIds,
                     const QHash<quint32, QPointF> &positions = QHash<quint32, QPointF>(),
                     const QRectF &band = QRectF());
};

/** @} */

#endif

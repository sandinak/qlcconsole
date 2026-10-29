/*
  Q Light Controller Plus
  linespread.h

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

#ifndef LINESPREAD_H
#define LINESPREAD_H

#include <QVector3D>
#include <QVector>
#include <QString>
#include <QMap>

/** @addtogroup engine Engine
 * @{
 */

/**
 * Where each light lands on a line target (docs/design/LINE_TARGET_DESIGN.md): N lights,
 * in order, spread evenly along the line end to end; SPREAD (-5..+5) squeezes
 * them towards the centre (0 = all on it) and, below 0, across it so the
 * beams cross; GROUP and CROSS say what crosses -- whole groups swapping sides
 * keeping their order, or each group flipping in place.
 *
 * Pure arithmetic over an ordered list: it does not know what a fixture is, so
 * the same rule serves one look's lights or the union of several (a cue-level
 * aim, docs/design/SHOW_TREE_DESIGN.md). Each setting is its own palette param
 * ("__lineSpread", "__lineGroup", "__lineCross", "__lineShare",
 * "__lineOrder"), absent = the default, so each can be inherited on its own.
 */
class LineSpread
{
public:
    /** At a negative spread, what crosses. */
    enum Cross
    {
        SwapSides = 0,  //!< groups swap sides of the centre, keeping their order
        Mirror    = 1   //!< each group flips in place
    };
    /** Which light goes to which point. */
    enum Order
    {
        AlongLine = 0,  //!< by where the lights hang, measured along the line
        LookOrder = 1   //!< the order they are listed in
    };
    /** Group value meaning "two halves" (with an odd count the middle light
     *  is in neither, and stays on the centre). */
    static constexpr int Halves = 0;

    static constexpr double maxSpread() { return 5.0; }

    struct Settings
    {
        double spread = 5.0;        //!< -5..+5; +5 = evenly along the whole line
        int group = 1;              //!< lights per group; Halves = two halves
        Cross cross = SwapSides;
        bool share = false;         //!< a group's lights land on one point
        Order order = AlongLine;

        bool operator==(const Settings &o) const
        {
            return spread == o.spread && group == o.group && cross == o.cross
                   && share == o.share && order == o.order;
        }
        bool operator!=(const Settings &o) const { return !(*this == o); }
    };

    static QString paramName(const char *what) { return QStringLiteral("__line") + QLatin1String(what); }

    /** The settings a look carries; a missing param is its default. */
    static Settings fromParams(const QMap<QString, double> &params);

    /** @p params with @p s written into it: each setting at its default is
     *  REMOVED rather than written, so a look only carries what it changes. */
    static void toParams(const Settings &s, QMap<QString, double> &params);

    /** How far along the line (0..1) each of @p count lights lands, in their
     *  order. One light: the centre. */
    static QVector<double> fractions(int count, const Settings &s);

    /** The order to give lights at @p positions so the one nearest the
     *  line's start (@p from) gets the start: indices into @p positions,
     *  sorted by how far along from -> to each one is. Lights level with each
     *  other (within a millimetre) keep the order they came in. A line with
     *  no length keeps the order as given. */
    static QVector<int> orderAlong(const QVector<QVector3D> &positions,
                                   const QVector3D &from, const QVector3D &to);
};

/** @} */

#endif

/*
  Q Light Controller Plus
  linetarget_test.h

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

#ifndef LINETARGET_TEST_H
#define LINETARGET_TEST_H

#include <QObject>

/** Line targets (LINE_TARGET_DESIGN.md): the curve, where lights land on it,
 *  the target's storage, and Aim looks pointing each head at its own point. */
class LineTarget_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** Two points: a straight line, measured end to end. */
    void aStraightLineIsMeasuredEvenly();
    /** The curve goes through every handle, mid points included. */
    void theCurvePassesThroughEveryPoint();
    /** Even along the CURVE: half-way is half the distance, however unevenly
     *  the handles are spaced. */
    void halfWayIsHalfTheDistance();
    /** Which span a fraction lies in, and the fraction nearest a point --
     *  what inserting a point on a double-click needs. */
    void spansAndNearestPoints();
    /** Re-centring moves the points, not the line. */
    void recentringKeepsTheLineInPlace();

    /** N lights end to end, the first on one end and the last on the other;
     *  one light on the centre. */
    void lightsSpreadEndToEnd();
    /** Lights are put in order by where they hang along the line. */
    void lightsAreOrderedAlongTheLine();

    /** A line target saves its kind and points, and loads them back. */
    void aLineSavesAndLoads();
    /** A line read with no points gets the default ones; LinePoint elements
     *  under any other kind are skipped cleanly (the reader is left where an
     *  older build would leave it). */
    void loadingIsForgiving();
    /** Every head in an Aim look on a line gets its own point, stage-left
     *  head to the stage-left end, whatever order the look lists them in. */
    void anAimLookSpreadsItsHeadsAlongTheLine();
    /** The palette writes different pan for each head on a line, and the
     *  same for all of them on a point. */
    void thePaletteAimsEachHeadAtItsOwnPoint();
    /** Moving the target moves the whole line. */
    void movingTheTargetMovesTheLine();
};

#endif

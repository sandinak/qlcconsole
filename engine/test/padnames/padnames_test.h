/*
  Q Light Controller Plus
  padnames_test.h

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

#ifndef PADNAMES_TEST_H
#define PADNAMES_TEST_H

#include <QObject>

/* Board Q170: what a pad's controls are called, by family (PadNames), and
   its touchpad read as a trackpad (TouchpadGesture). */
class PadNames_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** Every role in every family, both forms: the table. */
    void names_data();
    void names();
    /** Tokens become the family's words; anything else in braces stays. */
    void expandsTokens();
    /** The plugin's kind keys, and the guess from a profile with none. */
    void familyFromKindOrProfile();
    /** The face buttons' own colours. */
    void faceColours();

    /** One finger dragged: the move, as a fraction of the pad. */
    void aDragMovesByHowFar();
    /** A lift reads as no finger, wherever it was: no jump; a landing
     *  anywhere (the middle too) starts again from there. */
    void aLiftOrLandingNeverJumps();
    /** A second finger starts a two-finger drag; the first finger's move
     *  in that frame is not a one-finger drag. */
    void aSecondFingerIsATwoFingerDrag();
    /** A click mid-drag is a click, not a drag. */
    void aClickIsNotADrag();
    /** A new touch moves nothing until slop(); a resting finger's jitter
     *  adds up to nothing. */
    void slopAndJitter();
    /** The readings of a frame come in any order. */
    void anyOrderInAFrame();
    /** The roles' keys round-trip, and touch roles are not axes. */
    void touchRoles();
};

#endif

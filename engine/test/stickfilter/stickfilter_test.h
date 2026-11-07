/*
  Q Light Controller Plus - Unit test
  stickfilter_test.h

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

#ifndef STICKFILTER_TEST_H
#define STICKFILTER_TEST_H

#include <QObject>

class StickFilter_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** A stick that rests in the middle reads exactly as it did. */
    void aCentredStickIsUntouched();
    /** Rests of +0.10 / -0.07, learnt as the pad arrives, read 0. */
    void aRestingStickIsLearntAsItArrives();
    /** After the rest is taken out, full travel still reaches both ends. */
    void fullTravelStillReachesBothEnds();
    /** A rest off the middle made one way faster than the other (a pan
     *  right slower than left on a pad resting left); corrected, the same
     *  push either way reads the same. */
    void bothWaysReadTheSame();
    /** A stick moving, or held far out, as the pad arrives is not learnt. */
    void aMovingOrHeldStickIsNotLearnt();
    /** Once the pad has settled in, a slow held push is never swallowed. */
    void aHeldPushAfterwardsIsKept();
    /** A stick still off the middle for the drift time is said once;
     *  touching anything else on the pad puts that off. */
    void driftIsSaidOnce();
    /** Centre Sticks learns the rest again, further out; a stick still
     *  held when it gives up is counted. */
    void centreSticksLearnsAgain();
    /** A rest remembered from before is used from the first reading. */
    void aRememberedRestIsUsed();
    /** Buttons and triggers pass untouched. */
    void otherChannelsPass();
};

#endif

/*
  Q Light Controller Plus - Unit test
  controllerkeepalive_test.h

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

#ifndef CONTROLLERKEEPALIVE_TEST_H
#define CONTROLLERKEEPALIVE_TEST_H

#include <QObject>

class ControllerKeepAlive_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** The activity is taken while Focus mode or the follow spot wants the
     *  controller -- once, however many do -- and let go when none does. */
    void theActivityFollowsWhoWantsIt();
    /** A new backend takes over a held activity. */
    void aNewBackendTakesItOver();
    /** Paused only for what is known, and why. */
    void pausedSaysWhy();
    /** After a wake: until input comes, or the grace runs out. */
    void wakingEndsWithInputOrTime();
    /** Unlocking ends a lock, not a wake. */
    void unlockingIsNotWaking();
    /** What is said about Input Monitoring. */
    void thePermissionHint();
};

#endif

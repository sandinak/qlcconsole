/*
  Q Light Controller Plus
  hidgamecontroller_test.h

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

#ifndef HIDGAMECONTROLLER_TEST_H
#define HIDGAMECONTROLLER_TEST_H

#include <QObject>

/**
 * The GameController framework side of the HID plugin, on a Mac: a pad the
 * framework reads (an Xbox pad, a DualShock 4 / DualSense) going away --
 * auto-off, a Bluetooth drop -- and coming back, as the framework announces
 * it. No real pad is touched: the controllers are made here, and every claim
 * names one of them.
 */
class HIDGameController_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void aPadComingAndGoingIsAnnounced();
    void aPadThatGoesReadsAsGoneAndItsWokenSelfIsClaimed();
    void aPlayStationPadIsFollowedTheSameWay();
};

#endif

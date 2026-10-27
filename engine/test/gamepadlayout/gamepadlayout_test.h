/*
  Q Light Controller Plus - Unit test
  gamepadlayout_test.h

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

#ifndef GAMEPADLAYOUT_TEST_H
#define GAMEPADLAYOUT_TEST_H

#include <QObject>

class GamepadLayout_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** The transport in a macOS hidapi path, and a product ID's. */
    void transports();
    /** Each model and connection is read the way it reports. */
    void families_data();
    void families();
    /** First Bluetooth firmware: buttons in a row, triggers on Z/Rz. */
    void classicElements();
    /** Later Bluetooth / BLE: gaps, right stick on Z/Rz, Brake/Accelerator. */
    void modernElements();
    /** No family maps two elements to one control, Bluetooth's alternate
     *  View/Xbox/Share keys aside; each one Focus mode uses is reachable. */
    void everyControlOnce();
    /** A hat switch as the D-pad, either numbering. */
    void hatSwitch();
    /** The USB GIP report, bit by bit. */
    void gipReport();
    /** The layout is the shipped Xbox profile's, and the profile matches
     *  every model, wired and wireless. */
    void matchesTheXboxProfile();
    /** Board Q139: a DualShock 4 / DualSense's HID elements, by usage. */
    void playStationElements();
    /** ...and the profile names them, with the same roles. */
    void matchesThePlayStationProfile();
    /** u8: neither shipped profile steps a cue list from a button: a pad
     *  picked up mid-show must never move the show. */
    void noShippedProfileStepsAList();
};

#endif

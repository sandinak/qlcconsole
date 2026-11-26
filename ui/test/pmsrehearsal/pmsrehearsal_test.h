/*
  Q Light Controller Plus
  pmsrehearsal_test.h

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

#ifndef PMSREHEARSAL_TEST_H
#define PMSREHEARSAL_TEST_H

#include <QObject>

/**
 * The PMS visit rehearsed (docs/project/PMS_ONSITE_CHECKLIST.md): a touring
 * show opened on its office venue, New Venue from a Show on the room's
 * show file through the venue menu as he would, then the house rig, the
 * held groups, the clash check, the output (Not yet, amber), the roles and
 * the six standard cues on the room's lights, the room lined up under the
 * show, a save and a reopen, the USB interface chosen on site, and the
 * office again with the show unchanged.
 *
 * Runs on a made-up tour show and a PMS-like room (engine/test/common/
 * pmslikeshow.h) in the gate. With QLC_PMS_TOUR_SHOW and QLC_PMS_ROOM_SHOW
 * (and the tour show's .venues.xml beside it) it runs on copies of real
 * files too, writing what it found to $QLC_SHOT_DIR/pms-rehearsal-*.txt.
 * Nothing goes to a real network: only the I/O plugin stub is loaded.
 */
class PmsRehearsal_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void rehearseTheVisit_data();
    void rehearseTheVisit();
};

#endif

/*
  Q Light Controller Plus
  rigroomlight_test.h

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

#ifndef RIGROOMLIGHT_TEST_H
#define RIGROOMLIGHT_TEST_H

#include <QObject>

class App;

/* The Rig Overview's 45° view in the room's light: an object's edges follow
 * the room as its faces do, so a blackout leaves dim silhouettes, not a line
 * drawing in plot or warning colours. */
class RigRoomLight_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** Review shots of a real show (repro/show.qlcc beside the test, never
     *  committed) in Blackout and House, with a per-object list of the
     *  red-dominant colours each draws. Skipped when there is no show or no
     *  QLC_SHOT_DIR. */
    void userShowShots();

private:
    App *m_app = nullptr;
};

#endif

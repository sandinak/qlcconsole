/*
  Q Light Controller Plus
  hazardarmingshow_test.h

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

#ifndef HAZARDARMINGSHOW_TEST_H
#define HAZARDARMINGSHOW_TEST_H

#include <QObject>
#include <QString>

class App;

/* test-workspaces/hazard-arming.qlcc: the bench show for hazard arming
   (docs/features: Behaves as, the Hazard Hold / Arm / Kill). A CO2 jet
   (Hazard — Burst), a hazer (Hazard — Running) and a plain wash on the OLA
   probe's Art-Net universe 0:0:15; cues Preset, CO2 hit, Haze on, Haze
   off, Blackout. The desk's Hazard controls are left unlearned: they are
   learned on the bench's own board. Built here with the app's own engine,
   saved, opened again and checked. QLC_SHOT_DIR (the pool's --shots) or
   HAZARD_OUT=<.qlcc> gets the built file. */
class HazardArmingShow_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** Builds the show, saves it, opens it again and runs every check. */
    void buildSaveAndCheck();
    /** The committed test-workspaces/hazard-arming.qlcc passes the same. */
    void theCommittedShowChecksOut();

private:
    bool open(const QString &path);
    void build(const QString &path);
    void check(const QString &path);
    /** The file's XML Output: Art-Net to the probe, never 0:0:0. */
    void checkOutputXml(const QString &path);

private:
    App *m_app = nullptr;
};

#endif

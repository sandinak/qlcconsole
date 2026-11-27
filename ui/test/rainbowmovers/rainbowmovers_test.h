/*
  Q Light Controller Plus
  rainbowmovers_test.h

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

#ifndef RAINBOWMOVERS_TEST_H
#define RAINBOWMOVERS_TEST_H

#include <QObject>
#include <QString>

class App;

/* test-workspaces/rainbow-movers.qlcc: the show for testing "Rainbow on a
   CMY or wheel-only mover: CMY mixes, the wheel steps" by eye (a CMY mover,
   a colour-wheel mover and an RGB par on the OLA probe's Art-Net universe,
   cues Rainbow on / Rainbow off). Built here with the app's own engine,
   saved, opened again and checked, effect values included. QLC_SHOT_DIR
   (the pool's --shots) or RAINBOW_OUT=<.qlcc> gets the built file. */
class RainbowMovers_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** Builds the show, saves it where RAINBOW_OUT / QLC_SHOT_DIR says (else
     *  a temporary folder), opens it again and runs every check. */
    void buildSaveAndCheck();
    /** The committed test-workspaces/rainbow-movers.qlcc passes the same. */
    void theCommittedShowChecksOut();

private:
    bool open(const QString &path);
    void build(const QString &path);
    void check(const QString &path);
    /** The cue-list's XML Output: Art-Net to the probe, never 0:0:0. */
    void checkOutputXml(const QString &path);
    /** The Rainbow on the three fixtures, sampled on a fixed clock. */
    void checkEffect();

private:
    App *m_app = nullptr;
};

#endif

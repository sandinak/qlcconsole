/*
  Q Light Controller Plus
  pixelboxes_test.h

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

#ifndef PIXELBOXES_TEST_H
#define PIXELBOXES_TEST_H

#include <QObject>
#include <QList>

class Doc;
class Fixture;
class QLCPalette;

/* The course rig's pixel boxes and beams, played from the Show Tree
   (docs/course/COURSE_PLAN.md §8 #15, #17, #20): the classroom show's
   XL-450 truss boxes (45 RGB pixels and 30 white ones, no dimmer) and its
   two-arm beams. Offscreen; the universes are read, never sent. */
class PixelBoxes_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** Scoped hits (sch): a Once breathe in a hit under a cue, fired,
     *  let go, the cue left and come back to, fired again: it breathes
     *  every time. */
    void aOnceBreatheInACuesHitBreathesEachFire();
    void whitePixelsLightFromALook_data();
    void whitePixelsLightFromALook();
    void dimmerEffectsMoveThePixelBoxes_data();
    void dimmerEffectsMoveThePixelBoxes();
    void oneEffectOverSeveralLayersMovesThePixelBoxes();
    void aCueWideEffectMovesThePixelBoxes();
    void aColourChaseRunsAtItsSpeedOverPixelBoxes();
    void beamsAimedAtALineTiltUp();

    // A Breathe on the step tops at 20% (the user's report, build 2593)
    void aBreatheStartsAtItsFloor();
    void aBreatheOnceEndsOnItsFloor_data();
    void aBreatheOnceEndsOnItsFloor();
    void aBreatheSetToOncePlaysItAgain();
    void aOnceBreatheInEachOfThreeCopiedCues();
    void aOnceBreatheInAHitFiredAgain();
    void aOnceBreatheTrackedThroughStaysDone();

private:
    quint32 group(const QString &name) const;
    Fixture *fixture(const QString &name) const;
    quint32 palette(const QString &name) const;
    quint32 effect(const QString &script, const QString &name);
    /** A production of one cue: returns the cue. */
    quint32 oneCue();
    void go();
    void run(int ms);
    QByteArray sends(const QString &fixture, int from, int n) const;
    /** A Breathe (dimmer-phaser at its preset) on the step tops in White at
     *  20%: the cue's one layer. */
    bool breatheOnTheStepTops(double rate, int runs);

    Doc *m_doc = nullptr;
    quint32 m_cue = 0;      //!< oneCue()'s
};

#endif

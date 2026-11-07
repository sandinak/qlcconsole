/*
  Q Light Controller Plus
  followspotmover.h

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

#ifndef FOLLOWSPOTMOVER_H
#define FOLLOWSPOTMOVER_H

#include <QMutex>
#include <climits>
#include <QVector3D>

#include "followspotdrive.h"
#include "dmxsource.h"

class Doc;
class Scene;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The follow spot's motion, stepped on the MasterTimer: once a DMX frame,
 * over the time that frame really took.
 *
 * It used to move on the controller's own 20 ms timer on the main thread
 * while the MasterTimer wrote DMX on another 20 ms clock. The two drift
 * against each other, so a quarter of the frames saw no move and a
 * quarter two -- the beam stepped, held, jumped (measured: per-frame step
 * CoV 0.73). Here every frame moves the subject once, by the time since
 * the last frame, and asks the scene to re-aim at the next one.
 *
 * Target mode (an Aim look): the subject is moved in floor space (in the
 * live blocking) and every head re-aims at it. Free-fly (no Aim look): it
 * only keeps the velocity, which the followspot effect reads.
 *
 * Registered with the MasterTimer only while a follow spot is on. The main
 * thread sets the sticks and settings; everything is under one mutex.
 */
class FollowSpotMover final : public DMXSource
{
public:
    explicit FollowSpotMover(Doc *doc);
    ~FollowSpotMover() override;

    /** Follow @p sceneId's subject @p targetId (UINT_MAX: free-fly, only
     *  the velocity); @p sceneId UINT_MAX: off (unregistered). Main thread.
     *  The scene is looked up here and kept: the controller turns this off
     *  when Doc removes it (functionRemoved / clearing, before the delete),
     *  so the MasterTimer thread never looks into Doc's function map. */
    void follow(quint32 sceneId, quint32 targetId);
    quint32 sceneId() const;
    quint32 targetId() const;
    bool isOn() const;

    void setSettings(const FollowSpotDrive::Settings &s);
    FollowSpotDrive::Settings settings() const;
    /** Deflections, -1..1: x right, y up the stage (away from the
     *  operator: the stick pushed up). */
    void setSticks(double coarseX, double coarseY, double fineX, double fineY);
    bool stepMultiplier(int dir);
    double multiplier() const;
    void setMultiplier(double m);

    /** The smoothed velocity now, metres a second. */
    QPointF velocity() const;
    bool isActive() const;
    /** Stop dead (the subject was put somewhere by something else). */
    void stop();

    /** Frames that moved the subject since the last call (the main thread's
     *  "it moved" for the indicators and the held beam). */
    int takeMoves();

    /** Tests: every frame is @p dt seconds (0: real time). */
    void setFixedDt(double dt);

    /** One frame (the MasterTimer thread, or a test). */
    void step(double dt);

    /** The controller's own tick. When the MasterTimer has not stepped
     *  this for three of them -- it is not running (a test's own timer)
     *  or has stalled -- the tick steps it, @p nominal seconds a time, so
     *  the follow spot never just stops. */
    void mainTick(double nominal);

    /** @reimp */
    void writeDMX(MasterTimer *timer, QList<Universe *> universes) override;

    /** How far the subject may be taken from the stage centre (metres). */
    static constexpr float kReach = 50.0f;

private:
    Doc *m_doc;
    mutable QMutex m_mutex;
    FollowSpotDrive m_drive;
    TickClock m_clock;
    quint32 m_sceneId = UINT_MAX;
    Scene *m_scene = nullptr;
    quint32 m_targetId = UINT_MAX;
    bool m_registered = false;
    int m_moves = 0;
    int m_frames = 0;       ///< MasterTimer frames that stepped this
    int m_framesSeen = -1;  ///< ...as the last mainTick() saw
    int m_idleTicks = 0;    ///< mainTick()s since one did
};

/** @} */

#endif

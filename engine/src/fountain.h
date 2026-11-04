/*
  Q Light Controller Plus
  fountain.h

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

#ifndef FOUNTAIN_H
#define FOUNTAIN_H

#include <QList>
#include <QPointF>
#include <QVector3D>

class MonitorProperties;
class StagePlatform;
class Scenery;
class Fixture;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * A fountain as Add Fountain builds it: a plot group holding a base (a
 * platform) and rounded-U walls standing on it, with a silk flame in the
 * middle (docs/design/FOUNTAIN_WAND.md). Nothing is stored to say "this is
 * a fountain": it is recognised from its pieces, so an older show's
 * fountains are found too.
 */
namespace Fountain
{
    /** A new fountain's fire height: 3 ft of silk above the inner wall. */
    const float kDefaultFireHeight = 0.9144f;

    struct Parts
    {
        quint32 groupId = 0;
        StagePlatform *base = nullptr;
        Scenery *inner = nullptr;   ///< the tallest wall
        Scenery *outer = nullptr;   ///< the lowest wall (== inner with one wall)
        bool isValid() const { return base != nullptr && inner != nullptr; }
    };

    /** The fountain group @p groupId is, or an invalid Parts. */
    Parts ofGroup(MonitorProperties *props, quint32 groupId);
    /** The fountain @p fid is in (its plot group), or invalid. */
    Parts ofFixture(MonitorProperties *props, quint32 fid);
    /** The fountain whose base @p platformId is, or invalid. */
    Parts ofBase(MonitorProperties *props, quint32 platformId);
    /** The fountain whose base covers @p x, @p y (metres), or invalid. */
    Parts at(MonitorProperties *props, double x, double y);

    /** The top of its inner wall (absolute, metres). */
    float innerTop(const Parts &f);
    /** The top of its base (absolute, metres). */
    float baseTop(MonitorProperties *props, const Parts &f);
    /** Whether @p x, @p y is on the inner wall's top (within its thickness). */
    bool onInnerWall(const Parts &f, double x, double y);

    /** A silk flame (QLCPhysical::FlameEmission). */
    bool isFlame(const Fixture *fx);
    /** Where its silk starts: the inner wall's top in a fountain, else the
     *  top of its own unit. */
    float flameBottom(const Doc *doc, MonitorProperties *props, quint32 fid);
    /** How high the silk rises from flameBottom(): its fire height, or for a
     *  flame that has none what it was always drawn as. */
    float flameHeight(const Doc *doc, MonitorProperties *props, quint32 fid);
    /** Where the silk was drawn before fire heights: the unit's top plus the
     *  greater of 0.45 m and twice its width. */
    float legacyFlameTop(const Doc *doc, MonitorProperties *props, quint32 fid);
    /** Its unit's size from the definition (metres): width, height. */
    QPointF flameUnitSize(const Fixture *fx);

    /** Mount @p fid in the fountain under @p x, @p y: on its base, or on the
     *  inner wall's top when it is over it, in its group and layer. A fixed
     *  light is aimed up at the flame. False (nothing changed) when there is
     *  no fountain there. */
    bool mountIn(const Doc *doc, MonitorProperties *props, quint32 fid, double x, double y);

    /** On load: every fountain group is an assembly; a fixture standing on a
     *  fountain's base joins its group and layer; a flame with no fire height
     *  gets one that keeps its top where it was drawn, starting now at the
     *  inner wall's top. */
    void repair(const Doc *doc, MonitorProperties *props);
}

/** @} */

#endif

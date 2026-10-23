/*
  Q Light Controller Plus
  itemicons.h

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

#ifndef ITEMICONS_H
#define ITEMICONS_H

#include <QIcon>
#include <QColor>
#include <QList>
#include "fixture.h"

class FixtureGroup;
class StageTarget;
class QLCPalette;
class Fixture;
class Doc;

/**
 * Icons that say what a row IS, drawn in code (QPainter through a
 * QIconEngine, so they are crisp at any size and pixel ratio, and their
 * outlines follow the current theme's text colour): a Dimmer look as a pie
 * of its level, a Shutter look as open/closed/strobe, a point or a line
 * target, and a fixture (or a group of one kind) as what it is -- a fixed
 * wash, a moving spot, a moving wash.
 *
 * One function per kind of icon; paletteIcon(), fixtureIcon() and
 * groupIcon() pick among them for the call sites (Build's outline and
 * Workbench, the Palettes list, the Show Tree and its Library).
 */
namespace ItemIcons
{
    /** What a fixture is, as far as its icon goes. */
    enum FixtureKind
    {
        OtherKind,      ///< anything else: its definition type's own icon
        FixedWash,      ///< no pan/tilt, and colour (a PAR, a colour changer)
        MovingSpot,     ///< pan/tilt + gobo, prism or iris
        MovingWash,     ///< pan/tilt + zoom, no gobo
        MovingHead      ///< pan/tilt, neither of the above
    };

    /** What a Shutter or Strobe look does to the beam. */
    enum ShutterState { ShutterOpen, ShutterClosed, ShutterStrobe };

    /** From the definition: its type, and pan/tilt, gobo/prism/iris,
     *  zoom and colour channels. */
    FixtureKind fixtureKind(const Fixture *fixture);
    /** The kind all of @p group's fixtures share; OtherKind when mixed,
     *  empty, or all OtherKind. */
    FixtureKind groupKind(Doc *doc, const FixtureGroup *group);
    /** A Shutter look's state: what the engine puts out for it on the
     *  workspace's first fixture with a shutter channel -- open where that
     *  channel can open (a ShutterOpen capability, a strobe-preset channel),
     *  else what its capability at the look's value says (nothing known:
     *  open). A Strobe look is always ShutterStrobe. */
    ShutterState shutterState(Doc *doc, const QLCPalette *palette);
    /** A Dimmer look's level, 0..100. */
    int intensityPercent(const QLCPalette *palette);

    /* One function per icon kind. */
    QIcon intensityIcon(int percent);
    QIcon shutterIcon(ShutterState state, int ratePercent = -1);
    QIcon pointTargetIcon();
    QIcon lineTargetIcon();
    /** A hard hat: the Build tab (and wherever Build is named). */
    QIcon buildIcon();
    QIcon swatchIcon(const QColor &color);
    QIcon kindIcon(FixtureKind kind, bool group = false);

    /** The icon for a target of @p target's kind (nullptr: a point). */
    QIcon targetIcon(const StageTarget *target);
    /** A palette's icon: a swatch, a pie, a shutter, a target, or the
     *  type's stock icon. @p doc may be nullptr (no shutter/aim lookup). */
    QIcon paletteIcon(Doc *doc, const QLCPalette *palette);
    /** A curtain (Scenery::isCurtainFixture()): a drape on its track; with
     *  @p group, a little smaller with the group badge. */
    QIcon curtainIcon(bool group = false);
    /** A fixture's icon -- THE one, wherever a fixture is listed: its own
     *  stage look's when it has one; else a curtain's when it is one; else
     *  the look it is shown as (drawnStageLook(): on Auto, its closest when
     *  the show says so); else its kind's, or its definition type's -- with
     *  the output-rules badge when it has any (Fixture::OutputRules). */
    QIcon fixtureIcon(const Fixture *fixture);
    /** What a stage-look picker's "Auto" entry shows for @p fixture: its
     *  closest look's icon when it has one, else what it is (a curtain, its
     *  kind, its type) -- no badge: the entry is a look, not the fixture. */
    QIcon autoLookIcon(const Fixture *fixture);
    /** @p base with the output-rules badge (an amber power dot) */
    QIcon outputRulesBadged(const QIcon &base);
    /** A group's icon: all curtains, the curtain's with the group badge;
     *  when the show shows Auto fixtures as their closest look and its
     *  fixtures all come out as one look, that look's, with the badge; else
     *  its one kind's (stacked), else the group icon. */
    QIcon groupIcon(Doc *doc, const FixtureGroup *group);
    /** The one look @p group's fixtures are all shown as (drawnStageLook()),
     *  while the show shows Auto fixtures as their closest look; AutoLook
     *  when it does not, the group is empty, or they differ. */
    Fixture::StageLook groupStageLook(Doc *doc, const FixtureGroup *group);
    /** @p look's icon as a group's: a little smaller, with the badge. */
    QIcon lookGroupIcon(Fixture::StageLook look);
    /** The icon for fixtures/groups together (a Show Tree layer): one
     *  fixture, fixtureIcon(); one group, groupIcon(); more, as a group of
     *  all of them would be. */
    QIcon targetsIcon(Doc *doc, const QList<quint32> &groups,
                      const QList<quint32> &fixtures);
    /** The same for a Show Tree target by its signature ("g:1,2|f:3|s:4",
     *  "x:7" a function played as it is): a Workbench cue-layer row. */
    QIcon signatureIcon(Doc *doc, const QString &signature);
}

#endif

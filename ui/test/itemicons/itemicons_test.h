/*
  Q Light Controller Plus - Test Unit
  itemicons_test.h

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

#ifndef ITEMICONS_TEST_H
#define ITEMICONS_TEST_H

#include <QObject>
#include <QList>
#include <QPair>

class Doc;
class Fixture;

/* Icons that say what a row is (ItemIcons): a Dimmer look's level as a pie,
   a Shutter look's state, point and line targets, and fixtures and groups
   by what they are. */
class ItemIcons_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** The pie's lit share is the look's level: 0, 45, 100 %. */
    void thePieIsTheLevel();
    /** A Shutter look reads open / closed / strobe from what its value
     *  means; a Strobe look is a strobe whose ring is its rate. */
    void aShutterLookShowsItsState();
    /** An Aim look at a point shows the bullseye; at a line, the line. */
    void aTargetShowsItsKind();
    /** From the definition: fixed wash, moving spot, moving wash, a plain
     *  moving head, and anything else its own type's icon. */
    void aFixtureShowsWhatItIs();
    void aStageLookKeepsTheOutputRulesBadge();
    /** The show's "Auto fixtures as their closest stage look": on, an Auto
     *  fixture wears its closest look's icon (badge and all); off, exactly
     *  today's; a group of one look wears it too. */
    void anAutoFixtureWearsItsClosestLookWhenTheShowSaysSo();
    /** A group of one kind wears that kind (stacked); a mixed one the group
     *  icon; a layer on a group the same. */
    void aGroupOfOneKindShowsIt();
    /** A curtain (Scenery::isCurtainFixture()) shows the curtain, on Auto
     *  whatever its type is closest to; a group of curtains the group
     *  variant; a set look still wins; a hazer keeps its hazer icon. */
    void aCurtainShowsAsACurtain();
    /** The same fixture or group wears the same icon in every list: the
     *  Fixture Manager tree, Build's groups list, the Show Tree Library and
     *  the plot's inventory and layers panels -- pixel for pixel
     *  fixtureIcon() / groupIcon(), with the show setting off and on. */
    void theSameIconEverywhere();
    /** Crisp at 1x and 2x: the same picture, twice the pixels. */
    void iconsAreCrispAtTwoX();
    /** Pictures of Build ▸ Scenes, the Palettes list and the Show Tree, in
     *  the default and a dark theme (QLC_ICON_SHOW + QLC_ICON_SHOTS). */
    void pictures();

private:
    Fixture *makeFixture(int type, const QString &model,
                         const QList<QPair<int, int>> &channels);
    /** A Generic > Stage Curtain like one: no light, an "Open" channel. */
    Fixture *makeCurtain(int type, const QString &name);
    Doc *m_doc = nullptr;
};

#endif

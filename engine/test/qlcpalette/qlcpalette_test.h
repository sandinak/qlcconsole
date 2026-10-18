/*
  Q Light Controller Plus - Unit tests
  qlcpalette_test.h

  Copyright (C) Massimo Callegari

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

#ifndef QLCPHYSICAL_TEST_H
#define QLCPHYSICAL_TEST_H

#include <QObject>
#include "qlcpalette.h"

class QLCPalette_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initialization();
    void type();
    void icon();
    void value();
    void fanning();

    void colorHelpers();

    void load();
    void loadWrongRoot();
    void save();
    /** Palette folders are type-independent: an older show's type folders
     *  ("Palettes/Color/01-Song/") migrate on load, a saved show is marked so
     *  it never migrates again, and a folder named like a type survives. */
    void olderShowsTypeFoldersBecomeSharedFolders();
    void aUserFolderNamedLikeATypeIsKept();

    /** A gobo look writes its value to the fixture's gobo channel -- which
     *  a head's channel map never lists, so it has to be found per fixture. */
    void aGoboLookReachesTheGoboChannel();
    void aColourLookOnAGroupOnlyLightsTheGroupsHeads();

    /** Gobo by name (SHOW_TREE_DESIGN.md §10.7): exact, then singular /
     *  plural, then a unique prefix/contains; several in a tier: none. */
    void goboNamesMatchInTiers_data();
    void goboNamesMatchInTiers();

    /** A colour ring (Branson's "LED Movinghead+Circle"): a colour picks
     *  the nearest named slot -- by hue, black is "off", white/grey is
     *  White -- on the ring and on the wheel alike. */
    void slotColoursComeFromTheirNames();
    void colourPicksTheNearestSlot_data();
    void colourPicksTheNearestSlot();
    /** The two-head definition: a look on the Spot heads sets only the
     *  wheel, on the Ring heads only the ring, on both each its nearest. */
    void spotAndRingAreIndependent();
    /** The old one-head definition: the wheel AND the ring follow a colour
     *  in both modes (the 10-channel mode's ring is in no head). */
    void oldDefinitionSetsWheelAndRing();
    /** A look can name a ring slot that is no colour (a chase): the ring
     *  plays it, everything else the look's colour; saved with the show. */
    void aLookCanNameARingMacro();
};

#endif

/*
  Q Light Controller Plus
  wledshow_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDSHOW_TEST_H
#define WLEDSHOW_TEST_H

#include <QObject>

/* A WLED from Add Fixture to the stage, on the real WLED output plugin and a
   stand-in device (ui/test/common/fakewled.h): patched, its settings found
   and fixed, its string a fountain's Sprinkle loops written as segments,
   and a scene's effect played on it. */
class WledShow_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** The effect a scene sets ends up on every span of the fountain, each
     *  turned by Reverse from its own way. */
    void anEffectFromAShowPlaysOnEverySpanOfAFountain();

    /** Locate on a WLED playing a device effect (Fireworks Starburst): the
     *  device is sent Solid white at full and dark in turn, each at once
     *  (transition 0), and the look again after -- a normal fixture located
     *  with it in the same go flashes too. */
    void locateFlashesAWledPlayingAnEffectSolidWhiteAndGivesTheLookBack();

    /** Locate on a WLED a look had played and let go of: afterwards it is
     *  as it was -- off, its effect still the look's, not left on Solid. */
    void locateLeavesAWledNothingPlaysOnAsItWas();

    /** UX-72, F-11: a Tracking list whose cue plays Fireworks Starburst
     *  Runs Once, At the end Hold (the device frozen on its last frame),
     *  then a cue that sets only a colour (amber; the burst tracks into
     *  it), previewed in Design from the Show Tree. Right-click ▸ Locate
     *  (ContextMenus::locate) flashes it without leaving the Show Tree,
     *  and then gives back exactly what was showing: the same effect,
     *  colour, freeze and brightness, the preview still playing. */
    void locateGivesBackAHeldTrackedEffectWithItsNewColour();

    /** UX-73, his spell cue: the stage snaps to black but for the staff (a
     *  WLED on its own API) and the steps' effect. Each GO, through the
     *  Show Tree as the person fires it, reaches the staff within a few
     *  milliseconds of the DMX frame that snaps the stage, with no glide --
     *  and, moved in black while it was dark, the spell's GO sends the
     *  staff its brightness alone. */
    void aSnapCueReachesTheStaffWithTheStage();
};

#endif

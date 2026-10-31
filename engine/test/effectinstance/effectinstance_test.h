/*
  Q Light Controller Plus - Unit test
  effectinstance_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef EFFECTINSTANCE_TEST_H
#define EFFECTINSTANCE_TEST_H

#include <QObject>
#include <QList>

class Doc;
class FixtureGroup;
class QLCPalette;

class EffectInstance_Test final : public QObject
{
    Q_OBJECT

private slots:
    void theStepTestPatternCountsNumbersAndNamesTheSteps();
    void theFixturesAScriptSeesFollowTheRig();
    void aWholeRigOfPixelsTicksQuickly();
    void theTextEffectSpellsOnAGrid();
    /** The matrix effects on a box of RGB and white pixel rows, eight boxes
     *  as one canvas, its RGB-only and white-only modes and a plain RGB
     *  matrix: each head told what it emits, every effect running through
     *  the host, its output in range and where it belongs. */
    void matrixEffectsRunOnBoxesAndTheirModes();
    /** The same effects frame by frame (as the host hands them their
     *  heads): moving, repeatable, in range, white pixels doing what the
     *  white setting says; and still frames that must not drift. */
    void matrixEffectsAreRepeatableAndKeepTheirWhites();
    /** Heads past the grid they claim: every matrix effect answers them. */
    void matrixEffectsTakeCellsOffTheGrid();
    /** The second set of matrix effects: the look's colours, speed scaling
     *  their time, the beat clock deciding where they are. */
    void matrixEffectsTakeTheLooksColoursAndKeepTime();
    /** Each box, staggered: the same picture in each box, each a set part
     *  of a cycle behind the last, in the order chosen. */
    void matrixEffectsStaggerBoxToBox();
    /** Matrix Image: a GIF kept in the show as a data URI, decoded once,
     *  played at its own pace or on the beat, fitted, filled or stretched,
     *  and through a look's save and load to DMX. */
    void matrixImagePlaysAGifKeptInTheShow();
    /** Every matrix effect in each scope on eight XL-450s, within budget. */
    void matrixEffectsKeepTimeOnEightBoxes();
    /** Not a check: frames of each new effect to look at, when asked. */
    void matrixEffectPreviews();
    /** A loose pixel box with no group is drawn on as its grid, not a
     *  strip of its heads. */
    void aLooseBoxIsItsOwnGrid();
    void init();
    void cleanup();

    /** An Effect look with no fixture/group targets must drive ZERO fixtures
     *  (safety: no run-on-the-whole-rig fallback). */
    void emptyLookDrivesNothing();

    /** Assigning a group targets exactly that group's fixtures. */
    void groupTargetDrivesItsFixtures();
    void effectsStartedApartAreOnTheSameBeat();
    void anEffectOnASpeedMasterCountsItsBeats();
    void ownSpeedScalesTheEffectsTime();
    /** Runs: Loop | Once | N times and At the end: Hold | Release, on any
     *  effect (EffectRuns): a looping script made to run once, or three
     *  times, then hold its last frame or let go. */
    void anyEffectRunsOnceOrNTimesThenHoldsOrReleases();
    /** A one-shot script told to loop (or repeat) starts each run afresh;
     *  left as it is, it runs once. */
    void aOneShotScriptCanLoop();
    void anotherEffectPickedPlaysAtOnceDespiteARelease();

    /** Effect fades (0.4): the depth ramps 0 -> 1 over the fade in and
     *  1 -> 0 over the fade out; a release part way in goes from there. */
    void anEffectsDepthRampsInAndOut();
    void aReleaseMidFadeInFadesOutFromWhereItWas();
    /** A dimmer effect read off the universe frame by frame, fading in
     *  over what is under it and out to it. */
    void aDimmerEffectFadesInAndOutFrameByFrame();
    /** FadeFollow: the fades the scene starts and stops with (a cue's);
     *  none (0) plays as before, at full at once. */
    void anEffectFollowsItsScenesFades();
    void anEffectWithNoFadeTimesPlaysAsBefore();
    /** Two effects on one channel, one going and one coming. */
    void effectsPartWayInOrOutCompose();

    /** Assigning a single fixture targets only that fixture. */
    void singleFixtureTargetOnly();

    /** A multi-head fixture yields one cell PER HEAD (pixel addressing), not one
     *  cell for the whole fixture. */
    void multiHeadYieldsPerHeadCells();
    void groupsAreLaidAcrossTheStageWhereTheyStand();
    void aBarsHeadsAreAlongItsLength();
    void aLanternIsBuiltOfItsLeds();
    void wledPortsRunAsWledDoes();
    void everyWledPortRuns();
    void aWledLookRunsOnTheDeviceAndThePortElsewhere();
    void bothFiresAreOfferedAndEachComesBackAsItself();
    void anEffectArrivesByItsScenesTransition();
    void anEffectRunsOnUnpatchedFixturesToo();
    void aRainbowOnRgbwWithNoDimmerIsAHueNotWhite();
    void aRainbowWithALookColourSweepsThatColour();
    void effectsOnOneChannelLayerAsADeskDoes();
    void aMovementEffectMovesRoundWhereTheLookPoints();
    void aDimmerInTheHeadBreathesAndTheWheelHoldsItsColour();
    void aSharedDimmerTakesItsBrightestHead();
    /** A follow-spot with Fade Near Limit takes a head's dimmer down when its
     *  target is out of that head's reach, and leaves the dimmer to the scene
     *  when it is comfortably within it. */
    void followSpotFadesAHeadThatCannotReach();
    void followSpotFadesAHeadThatCannotSeeTheSubject();
    void aScriptIsToldWhatIsInTheWay();
    /** Size On Subject zooms a head so its beam is that wide where it lands,
     *  whatever the throw, within the lens's range. */
    void followSpotZoomsToHoldSizeOnSubject();
    /** A follow spot on an Aim look: two heads in different places both
     *  land on the follow-spot point, and moving the point moves both to
     *  the same new place (they converge; the effect leaves pan/tilt alone). */
    void followSpotMovesEveryHeadToTheSamePoint();
    /** A script that leaves out notes, lifecycle and the rest: empty and
     *  the defaults -- not the word "undefined" shown under the effect. */
    void aScriptsMissingTextIsEmptyNotUndefined();
    /** Input on the note range updates the "midi" data channel once per
     *  tick, not once per value: a busy Art-Net input landed there too and
     *  rebuilt it ~90,000 times a second on the GUI thread (pb5). */
    void midiDataIsPublishedOncePerTick();

private:
    Doc          *m_doc = nullptr;
    FixtureGroup *m_group = nullptr;
    QLCPalette   *m_palette = nullptr;
    QList<quint32> m_fixtureIds;
};

#endif

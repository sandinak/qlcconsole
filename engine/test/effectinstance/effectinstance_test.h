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
    void init();
    void cleanup();

    /** An Effect look with no fixture/group targets must drive ZERO fixtures
     *  (safety: no run-on-the-whole-rig fallback). */
    void emptyLookDrivesNothing();

    /** Assigning a group targets exactly that group's fixtures. */
    void groupTargetDrivesItsFixtures();
    void effectsStartedApartAreOnTheSameBeat();
    void anEffectOnASpeedMasterCountsItsBeats();

    /** Assigning a single fixture targets only that fixture. */
    void singleFixtureTargetOnly();

    /** A multi-head fixture yields one cell PER HEAD (pixel addressing), not one
     *  cell for the whole fixture. */
    void multiHeadYieldsPerHeadCells();
    void groupsAreLaidAcrossTheStageWhereTheyStand();
    void aBarsHeadsAreAlongItsLength();
    void aLanternIsBuiltOfItsLeds();
    void wledPortsRunAsWledDoes();
    void aWledLookRunsOnTheDeviceAndThePortElsewhere();
    void bothFiresAreOfferedAndEachComesBackAsItself();
    void anEffectArrivesByItsScenesTransition();
    void anEffectRunsOnUnpatchedFixturesToo();
    /** A follow-spot with Fade Near Limit takes a head's dimmer down when its
     *  target is out of that head's reach, and leaves the dimmer to the scene
     *  when it is comfortably within it. */
    void followSpotFadesAHeadThatCannotReach();
    /** Size On Subject zooms a head so its beam is that wide where it lands,
     *  whatever the throw, within the lens's range. */
    void followSpotZoomsToHoldSizeOnSubject();
    /** A script that leaves out notes, lifecycle and the rest: empty and
     *  the defaults -- not the word "undefined" shown under the effect. */
    void aScriptsMissingTextIsEmptyNotUndefined();

private:
    Doc          *m_doc = nullptr;
    FixtureGroup *m_group = nullptr;
    QLCPalette   *m_palette = nullptr;
    QList<quint32> m_fixtureIds;
};

#endif

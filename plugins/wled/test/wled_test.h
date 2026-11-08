/*
  Q Light Controller Plus
  wled_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLED_TEST_H
#define WLED_TEST_H

#include <QObject>

class Wled_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** Each test starts with no kept layouts (the settings are the test's own). */
    void init();

    void effectChannelsAreReadAsWledReadsThem();
    void colourModesAreSolidColours();
    void onlyChangesAreSentAndBurstsCollapse();
    /** A step (on or off, another effect, a change after the look stood
     *  still) goes at once; only a fade, send after send, glides. */
    void aStepGoesAtOnceAndOnlyAFadeGlides();
    /** UX-73: a change goes when it happens, not on the next send tick. */
    void aSnapGoesAtOnceNotOnTheNextTick();
    /** After a move in black, the GO sends only the brightness. */
    void aLookMovedInBlackGoesOnWithItsBrightnessAlone();
    void aWeakOrSilentLanternIsAProblem();
    void aWatchedLanternSaysWhatItsLedsShow();

    /* Backup lanterns: several devices on one universe. */
    void backupDevicesAreParametersOfTheirOwn();
    void everyDeviceIsSentTheSameLook();
    void aDeadDeviceDoesNotHoldUpTheOthers();
    void aDeviceThatComesBackIsSentTheLookAgain();
    void theRigViewWatchesTheFirstDeviceThatAnswers();
    void rigNetworkGateIsDecidedOffTheSendTick();

    /* Segments (a fountain's string, one segment per span). */
    void theFakeAppliesASegmentWithoutAnIdByItsIndex();
    void anEffectReachesEverySegment();
    void theOptionsFreezeBitIsReadAsFrz();
    void aFreezeReachesEverySegmentAndLetsGoAgain();
    void reverseTurnsEachSegmentRoundFromItsOwnWay();
    void aNewLayoutIsLearnedAndReverseStartsOverFromIt();
    void aSegmentTurnedRoundBySomeoneElseKeepsThatWay();
    void aSingleSegmentDeviceIsSentAsBefore();
    void anOldWledWithoutStateAndInfoIsSentAsBefore();
    void backupsKeepSegmentLayoutsOfTheirOwn();

    /* Never left turned round: closing, quitting, crashing. */
    void closingTheOutputPutsEachSegmentBackItsOwnWay();
    void aRestartAfterQuittingWithReverseOnTakesTheKeptWays();
    void aCrashWithReverseOnIsPutRightFromTheKeptWays();
    /** Against a real WLED, when WLED_REAL_IP names one (skipped otherwise):
     *  the look reaches it, and it answers. Its state is put back after. */
    void aRealLanternShowsTheLook();
};

#endif
